/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __MINIDUMP_H
#define __MINIDUMP_H

#include <linux/types.h>

#define MAX_NAME_LENGTH		12
/* md_region -  Minidump table entry
 * @name:	Entry name, Minidump will dump binary with this name.
 * @id:		Entry ID, used only for SDI dumps.
 * @virt_addr:  Address of the entry.
 * @phys_addr:	Physical address of the entry to dump.
 * @size:	Number of byte to dump from @address location
 *		it should be 4 byte aligned.
 */
struct md_region {
	char	name[MAX_NAME_LENGTH];
	u32	id;
	u64	virt_addr;
	u64	phys_addr;
	u64	size;
};

/*
 * Register an entry in Minidump table
 * Returns:
 *	region number: entry position in minidump table.
 *	Negative error number on failures.
 */
#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
extern int msm_minidump_add_region(const struct md_region *entry);
extern int msm_minidump_remove_region(const struct md_region *entry);
/*
 * Update registered region address in Minidump table.
 * It does not hold any locks, so strictly serialize the region updates.
 * Returns:
 *	Zero: on successfully update
 *	Negetive error number on failures.
 */
extern int msm_minidump_update_region(int regno, const struct md_region *entry);
extern bool msm_minidump_enabled(void);
extern struct md_region *md_get_region(char *name);
extern void dump_stack_minidump(u64 sp);
#else
static inline int msm_minidump_add_region(const struct md_region *entry)
{
	/* Return quietly, if minidump is not supported */
	return 0;
}
static inline int msm_minidump_remove_region(const struct md_region *entry)
{
	return 0;
}
static inline bool msm_minidump_enabled(void) { return false; }
static inline struct md_region *md_get_region(char *name) { return NULL; }
static inline void dump_stack_minidump(u64 sp) {}
static inline void add_trace_event(char *buf, size_t size) {}
#endif


#define MAX_OWNER_STRING	32
struct va_md_entry {
	unsigned long vaddr;
	unsigned char owner[MAX_OWNER_STRING];
	unsigned int size;
	void (*cb)(void *dst, unsigned long size);
};

#if IS_ENABLED(CONFIG_QCOM_VA_MINIDUMP)
extern bool qcom_va_md_enabled(void);
extern int qcom_va_md_register(const char *name, struct notifier_block *nb);
extern int qcom_va_md_unregister(const char *name, struct notifier_block *nb);
extern int qcom_va_md_add_region(struct va_md_entry *entry);
#else
static inline bool qcom_va_md_enabled(void) { return false; }
static inline int qcom_va_md_register(const char *name, struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int qcom_va_md_unregister(const char *name, struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int qcom_va_md_add_region(struct va_md_entry *entry)
{
	return -ENODEV;
}
#endif

#endif
