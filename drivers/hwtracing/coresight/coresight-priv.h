/* Copyright (c) 2011-2012, 2016-2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CORESIGHT_PRIV_H
#define _CORESIGHT_PRIV_H

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/coresight.h>
#include <linux/pm_runtime.h>

/*
 * Coresight management registers (0xf00-0xfcc)
 * 0xfa0 - 0xfa4: Management	registers in PFTv1.0
 *		  Trace		registers in PFTv1.1
 */
#define CORESIGHT_ITCTRL	0xf00
#define CORESIGHT_CLAIMSET	0xfa0
#define CORESIGHT_CLAIMCLR	0xfa4
#define CORESIGHT_LAR		0xfb0
#define CORESIGHT_LSR		0xfb4
#define CORESIGHT_AUTHSTATUS	0xfb8
#define CORESIGHT_DEVID		0xfc8
#define CORESIGHT_DEVTYPE	0xfcc

#define TIMEOUT_US		100
#define BM(lsb, msb)		((BIT(msb) - BIT(lsb)) + BIT(msb))
#define BMVAL(val, lsb, msb)	((val & GENMASK(msb, lsb)) >> lsb)
#define BVAL(val, n)		((val & BIT(n)) >> n)

#define ETM_MODE_EXCL_KERN	BIT(30)
#define ETM_MODE_EXCL_USER	BIT(31)

typedef u32 (*coresight_read_fn)(const struct device *, u32 offset);
#define coresight_simple_func(type, func, name, offset)			\
static ssize_t name##_show(struct device *_dev,				\
			   struct device_attribute *attr, char *buf)	\
{									\
	type *drvdata = dev_get_drvdata(_dev->parent);			\
	coresight_read_fn fn = func;					\
	u32 val;							\
	pm_runtime_get_sync(_dev->parent);				\
	if (fn)								\
		val = fn(_dev->parent, offset);				\
	else								\
		val = readl_relaxed(drvdata->base + offset);		\
	pm_runtime_put_sync(_dev->parent);				\
	return scnprintf(buf, PAGE_SIZE, "0x%x\n", val);		\
}									\
static DEVICE_ATTR_RO(name)

enum etm_addr_type {
	ETM_ADDR_TYPE_NONE,
	ETM_ADDR_TYPE_SINGLE,
	ETM_ADDR_TYPE_RANGE,
	ETM_ADDR_TYPE_START,
	ETM_ADDR_TYPE_STOP,
};

enum cs_mode {
	CS_MODE_DISABLED,
	CS_MODE_SYSFS,
	CS_MODE_PERF,
};

struct coresight_csr {
	const char *name;
	struct list_head link;
};

/**
 * struct cs_buffer - keep track of a recording session' specifics
 * @cur:	index of the current buffer
 * @nr_pages:	max number of pages granted to us
 * @offset:	offset within the current buffer
 * @data_size:	how much we collected in this run
 * @lost:	other than zero if we had a HW buffer wrap around
 * @snapshot:	is this run in snapshot mode
 * @data_pages:	a handle the ring buffer
 */
struct cs_buffers {
	unsigned int		cur;
	unsigned int		nr_pages;
	unsigned long		offset;
	local_t			data_size;
	local_t			lost;
	bool			snapshot;
	void			**data_pages;
};

static inline void CS_LOCK(void __iomem *addr)
{
	do {
		/* Wait for things to settle */
		mb();
		writel_relaxed(0x0, addr + CORESIGHT_LAR);
	} while (0);
}

static inline void CS_UNLOCK(void __iomem *addr)
{
	do {
		writel_relaxed(CORESIGHT_UNLOCK, addr + CORESIGHT_LAR);
		/* Make sure everyone has seen this */
		mb();
	} while (0);
}

static inline bool coresight_authstatus_enabled(void __iomem *addr)
{
	int ret;
	unsigned int auth_val;

	if (!addr)
		return false;

	auth_val = readl_relaxed(addr + CORESIGHT_AUTHSTATUS);

	if ((BMVAL(auth_val, 0, 1) == 0x2) ||
	    (BMVAL(auth_val, 2, 3) == 0x2) ||
	    (BMVAL(auth_val, 4, 5) == 0x2) ||
	    (BMVAL(auth_val, 6, 7) == 0x2))
		ret = false;
	else
		ret = true;

	return ret;
}

void coresight_disable_path(struct list_head *path);
int coresight_enable_path(struct list_head *path, u32 mode);
struct coresight_device *coresight_get_sink(struct list_head *path);
struct coresight_device *coresight_get_source(struct list_head *path);
struct coresight_device *coresight_get_enabled_sink(bool reset);
struct list_head *coresight_build_path(struct coresight_device *csdev,
				       struct coresight_device *sink);
void coresight_release_path(struct coresight_device *csdev,
			    struct list_head *path);

#ifdef CONFIG_CORESIGHT_SOURCE_ETM3X
extern int etm_readl_cp14(u32 off, unsigned int *val);
extern int etm_writel_cp14(u32 off, u32 val);
#else
static inline int etm_readl_cp14(u32 off, unsigned int *val) { return 0; }
static inline int etm_writel_cp14(u32 off, u32 val) { return 0; }
#endif

#ifdef CONFIG_CORESIGHT_CSR
extern void msm_qdss_csr_enable_bam_to_usb(struct coresight_csr *csr);
extern void msm_qdss_csr_disable_bam_to_usb(struct coresight_csr *csr);
extern void msm_qdss_csr_disable_flush(struct coresight_csr *csr);
extern int coresight_csr_hwctrl_set(struct coresight_csr *csr, uint64_t addr,
				 uint32_t val);
extern void coresight_csr_set_byte_cntr(struct coresight_csr *csr,
				 uint32_t count);
extern struct coresight_csr *coresight_csr_get(const char *name);
#else
static inline void msm_qdss_csr_enable_bam_to_usb(struct coresight_csr *csr) {}
static inline void msm_qdss_csr_disable_bam_to_usb(struct coresight_csr *csr) {}
static inline void msm_qdss_csr_disable_flush(struct coresight_csr *csr) {}
static inline int coresight_csr_hwctrl_set(struct coresight_csr *csr,
	uint64_t addr, uint32_t val) { return -EINVAL; }
static inline void coresight_csr_set_byte_cntr(struct coresight_csr *csr,
					   uint32_t count) {}
static inline struct coresight_csr *coresight_csr_get(const char *name)
					{ return NULL; }
#endif

#endif
