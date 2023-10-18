/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_COMM_DEF_H_
#define _MSM_COMM_DEF_H_

#include <linux/types.h>

enum op_mode {
	OP_NORMAL,
	OP_DRAINING,
	OP_FLUSH,
	OP_INVALID,
};

enum queue_state {
	QUEUE_INIT,
	QUEUE_ACTIVE = 1,
	QUEUE_START,
	QUEUE_STOP,
	QUEUE_INVALID,
};

#ifdef CONFIG_EVA_WAIPIO
#define CVP_SYNX_ENABLED 1
#define CVP_MMRM_ENABLED 1
#define CVP_FASTRPC_ENABLED 1
#define CVP_MINIDUMP_ENABLED 1
#endif

#ifdef CONFIG_EVA_NEO
#define CVP_SYNX_ENABLED 1
#define CVP_MMRM_ENABLED 1
#define CVP_FASTRPC_ENABLED 1
#define CVP_MINIDUMP_ENABLED 1
#endif

#ifdef CONFIG_EVA_ANORAK
#define CVP_SYNX_ENABLED 1
#define CVP_MMRM_ENABLED 1
#define CVP_FASTRPC_ENABLED 1
#define CVP_MINIDUMP_ENABLED 1
#define CVP_CONFIG_SYNX_V2 1
#endif

#endif
