/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MACH_SCM_H
#define __MACH_SCM_H

#include <soc/qcom/qtee_shmbridge.h>

#define SCM_SVC_BOOT			0x1
#define SCM_SVC_PIL			0x2
#define SCM_SVC_UTIL			0x3
#define SCM_SVC_TZ			0x4
#define SCM_SVC_IO			0x5
#define SCM_SVC_INFO			0x6
#define SCM_SVC_SSD			0x7
#define SCM_SVC_FUSE			0x8
#define SCM_SVC_PWR			0x9
#define SCM_SVC_MP			0xC
#define SCM_SVC_DCVS			0xD
#define SCM_SVC_ES			0x10
#define SCM_SVC_HDCP			0x11
#define SCM_SVC_MDTP			0x12
#define SCM_SVC_LMH			0x13
#define SCM_SVC_SMMU_PROGRAM		0x15
#define SCM_SVC_QDSS			0x16
#define SCM_SVC_RTIC			0x19
#define SCM_SVC_TSENS			0x1E
#define SCM_SVC_TZSCHEDULER		0xFC

#define SCM_FUSE_READ			0x7
#define SCM_CMD_HDCP			0x01

/* SCM Features */
#define SCM_SVC_SEC_CAMERA		0xD

#define DEFINE_SCM_BUFFER(__n) \
static char __n[PAGE_SIZE] __aligned(PAGE_SIZE)

#define SCM_BUFFER_SIZE(__buf)	sizeof(__buf)

#define SCM_BUFFER_PHYS(__buf)	virt_to_phys(__buf)

#define SCM_SIP_FNID(s, c) (((((s) & 0xFF) << 8) | ((c) & 0xFF)) | 0x02000000)
#define SCM_QSEEOS_FNID(s, c) (((((s) & 0xFF) << 8) | ((c) & 0xFF)) | \
			      0x32000000)
#define SCM_SVC_ID(s) (((s) & 0xFF00) >> 8)

#define MAX_SCM_ARGS 10
#define MAX_SCM_RETS 3

enum scm_arg_types {
	SCM_VAL,
	SCM_RO,
	SCM_RW,
	SCM_BUFVAL,
};

#define SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
			(((a) & 0xff) << 4) | \
			(((b) & 0xff) << 6) | \
			(((c) & 0xff) << 8) | \
			(((d) & 0xff) << 10) | \
			(((e) & 0xff) << 12) | \
			(((f) & 0xff) << 14) | \
			(((g) & 0xff) << 16) | \
			(((h) & 0xff) << 18) | \
			(((i) & 0xff) << 20) | \
			(((j) & 0xff) << 22) | \
			(num & 0xffff))

#define SCM_ARGS(...) SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

/**
 * struct scm_desc
 * @arginfo: Metadata describing the arguments in args[]
 * @args: The array of arguments for the secure syscall
 * @ret: The values returned by the secure syscall
 * @extra_arg_buf: The buffer containing extra arguments
		   (that don't fit in available registers)
 * @x5: The 4rd argument to the secure syscall or physical address of
	extra_arg_buf
 */
struct scm_desc {
	u32 arginfo;
	u64 args[MAX_SCM_ARGS];
	u64 ret[MAX_SCM_RETS];

	/* private */
	struct qtee_shm shm;
	u64 x5;
};

#ifdef CONFIG_QCOM_SCM

#define SCM_VERSION(major, minor) (((major) << 16) | ((minor) & 0xFF))
extern int scm_call2(u32 cmd_id, struct scm_desc *desc);
extern int scm_call2_noretry(u32 cmd_id, struct scm_desc *desc);
extern int scm_call2_atomic(u32 cmd_id, struct scm_desc *desc);
extern u32 scm_get_version(void);
extern int scm_is_call_available(u32 svc_id, u32 cmd_id);
extern int scm_restore_sec_cfg(u32 device_id, u32 spare, int *scm_ret);
extern u32 scm_io_read(phys_addr_t address);
extern int scm_io_write(phys_addr_t address, u32 val);
extern bool scm_is_secure_device(void);
extern int scm_get_feat_version(u32 feat);
extern bool is_scm_armv8(void);

extern struct mutex scm_lmh_lock;

#else

static inline int scm_call2(u32 cmd_id, struct scm_desc *desc)
{
	return 0;
}

static inline int scm_call2_noretry(u32 cmd_id, struct scm_desc *desc)
{
	return 0;
}

static inline int scm_call2_atomic(u32 cmd_id, struct scm_desc *desc)
{
	return 0;
}

static inline u32 scm_get_version(void)
{
	return 0;
}

static inline int scm_get_feat_version(u32 feat)
{
	return 0;
}

static inline int scm_is_call_available(u32 svc_id, u32 cmd_id)
{
	return 0;
}

static inline bool is_scm_armv8(void)
{
	return true;
}

static inline int scm_restore_sec_cfg(u32 device_id, u32 spare, int *scm_ret)
{
	return 0;
}

static inline u32 scm_io_read(phys_addr_t address)
{
	return 0;
}

static inline int scm_io_write(phys_addr_t address, u32 val)
{
	return 0;
}

static inline bool scm_is_secure_device(void)
{
	return false;
}

#endif
#endif
