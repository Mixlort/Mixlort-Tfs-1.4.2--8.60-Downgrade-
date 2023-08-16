// Copyright 2023 The Forgotten Server Authors. All rights reserved.
// Use of this source code is governed by the GPL-2.0 License that can be found in the LICENSE file.

#include "otpch.h"

#include "depotlocker.h"

DepotLocker::DepotLocker(uint16_t type) : Container(type, 30), depotId(0) {}

Attr_ReadValue DepotLocker::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	if (attr == ATTR_DEPOT_ID) {
		if (!propStream.read<uint16_t>(depotId)) {
			return ATTR_READ_ERROR;
		}
		return ATTR_READ_CONTINUE;
	}
	return Item::readAttr(attr, propStream);
}

ReturnValue DepotLocker::queryAdd(int32_t index, const Thing& thing, uint32_t count,
	uint32_t flags, Creature* actor/* = nullptr*/) const
{
	return Container::queryAdd(index, thing, count, flags, actor);
}

void DepotLocker::postAddNotification(Thing* thing, const Cylinder* oldParent, int32_t index, cylinderlink_t)
{
	if (parent) {
		parent->postAddNotification(thing, oldParent, index, LINK_PARENT);
	}
}

void DepotLocker::postRemoveNotification(Thing* thing, const Cylinder* newParent, int32_t index, cylinderlink_t)
{
	if (parent) {
		parent->postRemoveNotification(thing, newParent, index, LINK_PARENT);
	}
}
