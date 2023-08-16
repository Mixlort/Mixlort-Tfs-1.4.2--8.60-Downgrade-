// Copyright 2023 The Forgotten Server Authors. All rights reserved.
// Use of this source code is governed by the GPL-2.0 License that can be found in the LICENSE file.

#include "otpch.h"

#include "protocollogin.h"

#include "ban.h"
#include <iomanip>
#include "configmanager.h"
#include "game.h"
#include "tools.h"
#include "iologindata.h"
#include "outputmessage.h"
#include "tasks.h"

extern ConfigManager g_config;
extern Game g_game;

void ProtocolLogin::disconnectClient(const std::string& message)
{
	auto output = OutputMessagePool::getOutputMessage();

	output->addByte(0x0A);
	output->addString(message);
	send(output);

	disconnect();
}

void ProtocolLogin::getCharacterList(const std::string& accountName, const std::string& password)
{
	Account account;
	if (!IOLoginData::loginserverAuthentication(accountName, password, account)) {
		disconnectClient("Account name or password is not correct.");
		return;
	}

	auto output = OutputMessagePool::getOutputMessage();

	// Add char list
	output->addByte(0x64);

	uint8_t size = std::min<size_t>(std::numeric_limits<uint8_t>::max(), account.characters.size());

	output->addByte(size);
	for (uint8_t i = 0; i < size; i++) {
		output->addString(account.characters[i]);
		output->addString(g_config.getString(ConfigManager::SERVER_NAME));
		output->add<uint32_t>(g_config.getNumber(ConfigManager::IP));
		output->add<uint16_t>(g_config.getNumber(ConfigManager::GAME_PORT));
	}

	// Add premium days
	if (g_config.getBoolean(ConfigManager::FREE_PREMIUM)) {
		output->add<uint16_t>(0xFFFF); //client displays free premium
	} else {
		output->add<uint16_t>(account.premiumEndsAt - time(nullptr) / 86400);
	}

	send(output);

	disconnect();
}

// Character list request
void ProtocolLogin::onRecvFirstMessage(NetworkMessage& msg)
{
	if (g_game.getGameState() == GAME_STATE_SHUTDOWN) {
		disconnect();
		return;
	}

	msg.skipBytes(2); // client OS

	uint16_t version = msg.get<uint16_t>();
	if (version <= 822) {
		setChecksumMode(CHECKSUM_DISABLED);
	}

	if (version <= 760) {
		disconnectClient(fmt::format("Only clients with protocol {:s} allowed!", CLIENT_VERSION_STR));
		return;
	}

	msg.skipBytes(12);
	/*
	 * Skipped bytes:
	 * 4 bytes: protocolVersion
	 * 12 bytes: dat, spr, pic signatures (4 bytes each)
	 * 1 byte: 0
	 */

	if (!Protocol::RSA_decrypt(msg)) {
		disconnect();
		return;
	}

	xtea::key key;
	key[0] = msg.get<uint32_t>();
	key[1] = msg.get<uint32_t>();
	key[2] = msg.get<uint32_t>();
	key[3] = msg.get<uint32_t>();
	enableXTEAEncryption();
	setXTEAKey(std::move(key));

	if (version < CLIENT_VERSION_MIN || version > CLIENT_VERSION_MAX) {
		disconnectClient(fmt::format("Only clients with protocol {:s} allowed!", CLIENT_VERSION_STR));
		return;
	}

	if (g_game.getGameState() == GAME_STATE_STARTUP) {
		disconnectClient("Gameworld is starting up. Please wait.");
		return;
	}

	if (g_game.getGameState() == GAME_STATE_MAINTAIN) {
		disconnectClient("Gameworld is under maintenance.\nPlease re-connect in a while.");
		return;
	}

	BanInfo banInfo;
	auto connection = getConnection();
	if (!connection) {
		return;
	}

	if (IOBan::isIpBanned(connection->getIP(), banInfo)) {
		if (banInfo.reason.empty()) {
			banInfo.reason = "(none)";
		}

		disconnectClient(fmt::format("Your IP has been banned until {:s} by {:s}.\n\nReason specified:\n{:s}",
		                             formatDateShort(banInfo.expiresAt), banInfo.bannedBy, banInfo.reason));
		return;
	}

	auto accountName = msg.getString();
	if (accountName.empty()) {
		disconnectClient("Invalid account name.");
		return;
	}

	auto password = msg.getString();
	if (password.empty()) {
		disconnectClient("Invalid password.");
		return;
	}

	auto authToken = msg.getString();

	g_dispatcher.addTask([=, thisPtr = std::static_pointer_cast<ProtocolLogin>(shared_from_this()),
	                      accountName = std::string{accountName}, password = std::string{password},
	                      authToken = std::string{authToken}]() {
		thisPtr->getCharacterList(accountName, password);
	});
}
