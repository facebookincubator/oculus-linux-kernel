/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
/** @cond */
#pragma once

#include <soc/qcom/smci_object.h>

typedef struct {
	uint32_t key_size;
	uint32_t reserved;
} ILOWPOWERKEYMANAGER_key_info;

#define CLOWPOWERKEYMANAGER_UID (0x13B)

#define ILOWPOWERKEYMANAGER_HIBERNATE 1
#define ILOWPOWERKEYMANAGER_HIBERNATE_WITH_ENCRYPTION 2

#define ILOWPOWERKEYMANAGER_ERROR_INVALID_EVENT 10
#define ILOWPOWERKEYMANAGER_ERROR_INVALID_OPERATION 11
#define ILOWPOWERKEYMANAGER_ERROR_INVALID_KEYSIZE 12
#define ILOWPOWERKEYMANAGER_ERROR_KEY_GENERATION 13
#define ILOWPOWERKEYMANAGER_ERROR_RPMB_OPERATION 14

#define ILOWPOWERKEYMANAGER_OP_GETKEY 0
#define ILOWPOWERKEYMANAGER_OP_PREPARE 1
#define ILOWPOWERKEYMANAGER_OP_RESERVED 2

static inline int32_t
ILowPowerKeyManager_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
ILowPowerKeyManager_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
ILowPowerKeyManager_getKey(struct smci_object self, uint32_t event_val,
			   void *keyOut_ptr, size_t keyOut_len,
			   size_t *keyOut_lenout)
{
	int32_t result = 0;
	union smci_object_arg a[2] = {{{0, 0}}};

	a[0].b = (struct smci_object_buf) { &event_val, sizeof(uint32_t) };
	a[1].b = (struct smci_object_buf) { keyOut_ptr, keyOut_len * 1 };

	result = smci_object_invoke(self, ILOWPOWERKEYMANAGER_OP_GETKEY, a,
				    SMCI_OBJECT_COUNTS_PACK(1, 1, 0, 0));

	*keyOut_lenout = a[1].b.size / 1;

	return result;
}

static inline int32_t
ILowPowerKeyManager_prepare(struct smci_object self, uint32_t event_val,
			    const ILOWPOWERKEYMANAGER_key_info *keyInfo_ptr)
{
	union smci_object_arg a[1] = {{{0, 0}}};
	struct {
		uint32_t m_event;
		ILOWPOWERKEYMANAGER_key_info m_keyInfo;
	} i = {0};

	a[0].b = (struct smci_object_buf) { &i, 12 };
	i.m_event = event_val;
	i.m_keyInfo = *keyInfo_ptr;

	return smci_object_invoke(self, ILOWPOWERKEYMANAGER_OP_PREPARE, a,
				  SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}

