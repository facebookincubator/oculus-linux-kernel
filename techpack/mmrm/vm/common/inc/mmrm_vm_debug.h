/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __MMRM_VM_DEBUG__
#define __MMRM_VM_DEBUG__

#include <linux/debugfs.h>
#include <linux/printk.h>

#ifndef MMRM_VM_DBG_LABEL
#define MMRM_VM_DBG_LABEL "mmrm_vm"
#endif

#define MMRM_VM_DBG_TAG MMRM_VM_DBG_LABEL ": %4s: "

/* To enable messages OR these values and
 * echo the result to debugfs file.
 */
enum mmrm_msg_prio {
	MMRM_VM_ERR = 0x000001,
	MMRM_VM_HIGH = 0x000002,
	MMRM_VM_LOW = 0x000004,
	MMRM_VM_WARN = 0x000008,
	MMRM_VM_PRINTK = 0x010000,
};

extern int mmrm_vm_debug;

#define dprintk(__level, __fmt, ...) \
	do { \
		if (mmrm_vm_debug & __level) { \
			if (mmrm_vm_debug & MMRM_VM_PRINTK) { \
				pr_info(MMRM_VM_DBG_TAG __fmt, \
					get_debug_level_str(__level), \
					##__VA_ARGS__); \
			} \
		} \
	} while (0)

#define d_mpr_e(__fmt, ...) dprintk(MMRM_VM_ERR, __fmt, ##__VA_ARGS__)
#define d_mpr_h(__fmt, ...) dprintk(MMRM_VM_HIGH, __fmt, ##__VA_ARGS__)
#define d_mpr_l(__fmt, ...) dprintk(MMRM_VM_LOW, __fmt, ##__VA_ARGS__)
#define d_mpr_w(__fmt, ...) dprintk(MMRM_VM_WARN, __fmt, ##__VA_ARGS__)

static inline char *get_debug_level_str(int level)
{
	switch (level) {
	case MMRM_VM_ERR:
		return "err ";
	case MMRM_VM_HIGH:
		return "high";
	case MMRM_VM_LOW:
		return "low ";
	case MMRM_VM_WARN:
		return "warn";
	default:
		return "????";
	}
}

#endif /* __MMRM_VM_DEBUG__ */
