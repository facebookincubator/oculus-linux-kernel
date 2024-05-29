// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2013, 2015-2017, 2019-2021 The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/clk.h>
#include <linux/mutex.h>

#include "coresight-priv.h"
#include "coresight-common.h"

#define csr_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define csr_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define CSR_LOCK(drvdata)						\
do {									\
	mb(); /* ensure configuration take effect before we lock it */	\
	csr_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define CSR_UNLOCK(drvdata)						\
do {									\
	csr_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb(); /* ensure unlock take effect before we configure */	\
} while (0)

#define CSR_SWDBGPWRCTRL	(0x000)
#define CSR_SWDBGPWRACK		(0x004)
#define CSR_SWSPADREG0		(0x008)
#define CSR_SWSPADREG1		(0x00C)
#define CSR_STMTRANSCTRL	(0x010)
#define CSR_STMAWIDCTRL		(0x014)
#define CSR_STMCHNOFST0		(0x018)
#define CSR_STMCHNOFST1		(0x01C)
#define CSR_STMEXTHWCTRL0	(0x020)
#define CSR_STMEXTHWCTRL1	(0x024)
#define CSR_STMEXTHWCTRL2	(0x028)
#define CSR_STMEXTHWCTRL3	(0x02C)
#define CSR_USBBAMCTRL		(0x030)
#define CSR_USBFLSHCTRL		(0x034)
#define CSR_TIMESTAMPCTRL	(0x038)
#define CSR_AOTIMEVAL0		(0x03C)
#define CSR_AOTIMEVAL1		(0x040)
#define CSR_QDSSTIMEVAL0	(0x044)
#define CSR_QDSSTIMEVAL1	(0x048)
#define CSR_QDSSTIMELOAD0	(0x04C)
#define CSR_QDSSTIMELOAD1	(0x050)
#define CSR_DAPMSAVAL		(0x054)
#define CSR_QDSSCLKVOTE		(0x058)
#define CSR_QDSSCLKIPI		(0x05C)
#define CSR_QDSSPWRREQIGNORE	(0x060)
#define CSR_QDSSSPARE		(0x064)
#define CSR_IPCAT		(0x068)
#define CSR_BYTECNTVAL		(0x06C)
#define MSR_NUM			((drvdata->msr_end - drvdata->msr_start + 1) \
				/ sizeof(uint32_t))
#define MSR_MAX_NUM		128

#define BLKSIZE_256		0
#define BLKSIZE_512		1
#define BLKSIZE_1024		2
#define BLKSIZE_2048		3

#define FLUSHPERIOD_1	    0x1
#define FLUSHPERIOD_2048	0x800
#define PERFLSHEOT_BIT		BIT(18)

#define CSR_ATID_REG_OFFSET(atid, atid_offset) \
		((atid / 32) * 4 + atid_offset)

#define CSR_ATID_REG_BIT(atid)	(atid % 32)
#define CSR_MAX_ATID	128
#define CSR_ATID_REG_SIZE	0xc

struct csr_drvdata {
	void __iomem		*base;
	phys_addr_t		pbase;
	phys_addr_t		msr_start;
	phys_addr_t		msr_end;
	struct device		*dev;
	struct coresight_device	*csdev;
	uint32_t		*msr;
	atomic_t		*msr_refcnt;
	uint32_t		blksize;
	uint32_t		flushperiod;
	struct coresight_csr		csr;
	struct clk		*clk;
	spinlock_t		spin_lock;
	bool			usb_bam_support;
	bool			perflsheot_set_support;
	bool			hwctrl_set_support;
	bool			set_byte_cntr_support;
	bool			timestamp_support;
	bool			enable_flush;
	bool			msr_support;
};

DEFINE_CORESIGHT_DEVLIST(csr_devs, "csr");

static LIST_HEAD(csr_list);
static DEFINE_MUTEX(csr_lock);

#define to_csr_drvdata(c) container_of(c, struct csr_drvdata, csr)

static void msm_qdss_csr_config_flush_period(struct csr_drvdata *drvdata)
{
	uint32_t usbflshctrl;

	CSR_UNLOCK(drvdata);

	usbflshctrl = csr_readl(drvdata, CSR_USBFLSHCTRL);
	usbflshctrl = (usbflshctrl & ~0x3FFFC) | (drvdata->flushperiod << 2);
	csr_writel(drvdata, usbflshctrl, CSR_USBFLSHCTRL);

	CSR_LOCK(drvdata);
}

void msm_qdss_csr_enable_bam_to_usb(struct coresight_csr *csr)
{
	struct csr_drvdata *drvdata;
	uint32_t usbbamctrl;
	unsigned long flags;

	if (csr == NULL)
		return;

	drvdata = to_csr_drvdata(csr);
	if (IS_ERR_OR_NULL(drvdata) || !drvdata->usb_bam_support)
		return;

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);

	usbbamctrl = csr_readl(drvdata, CSR_USBBAMCTRL);
	usbbamctrl = (usbbamctrl & ~0x3) | drvdata->blksize;
	csr_writel(drvdata, usbbamctrl, CSR_USBBAMCTRL);

	usbbamctrl |= 0x4;
	csr_writel(drvdata, usbbamctrl, CSR_USBBAMCTRL);

	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
}
EXPORT_SYMBOL(msm_qdss_csr_enable_bam_to_usb);

void msm_qdss_csr_enable_flush(struct coresight_csr *csr)
{
	struct csr_drvdata *drvdata;
	uint32_t usbflshctrl;
	unsigned long flags;

	if (csr == NULL)
		return;

	drvdata = to_csr_drvdata(csr);
	if (IS_ERR_OR_NULL(drvdata) || !drvdata->usb_bam_support)
		return;

	spin_lock_irqsave(&drvdata->spin_lock, flags);

	msm_qdss_csr_config_flush_period(drvdata);

	CSR_UNLOCK(drvdata);

	usbflshctrl = csr_readl(drvdata, CSR_USBFLSHCTRL);
	usbflshctrl |= 0x2;
	if (drvdata->perflsheot_set_support)
		usbflshctrl |= PERFLSHEOT_BIT;
	csr_writel(drvdata, usbflshctrl, CSR_USBFLSHCTRL);

	CSR_LOCK(drvdata);
	drvdata->enable_flush = true;
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
}
EXPORT_SYMBOL(msm_qdss_csr_enable_flush);


void msm_qdss_csr_disable_bam_to_usb(struct coresight_csr *csr)
{
	struct csr_drvdata *drvdata;
	uint32_t usbbamctrl;
	unsigned long flags;

	if (csr == NULL)
		return;

	drvdata = to_csr_drvdata(csr);
	if (IS_ERR_OR_NULL(drvdata) || !drvdata->usb_bam_support)
		return;

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);

	usbbamctrl = csr_readl(drvdata, CSR_USBBAMCTRL);
	usbbamctrl &= (~0x4);
	csr_writel(drvdata, usbbamctrl, CSR_USBBAMCTRL);

	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
}
EXPORT_SYMBOL(msm_qdss_csr_disable_bam_to_usb);

void msm_qdss_csr_disable_flush(struct coresight_csr *csr)
{
	struct csr_drvdata *drvdata;
	uint32_t usbflshctrl;
	unsigned long flags;

	if (csr == NULL)
		return;

	drvdata = to_csr_drvdata(csr);
	if (IS_ERR_OR_NULL(drvdata) || !drvdata->usb_bam_support)
		return;

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);

	usbflshctrl = csr_readl(drvdata, CSR_USBFLSHCTRL);
	usbflshctrl &= ~0x2;
	if (drvdata->perflsheot_set_support)
		usbflshctrl &= ~PERFLSHEOT_BIT;
	csr_writel(drvdata, usbflshctrl, CSR_USBFLSHCTRL);

	CSR_LOCK(drvdata);
	drvdata->enable_flush = false;
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
}
EXPORT_SYMBOL(msm_qdss_csr_disable_flush);

int coresight_csr_hwctrl_set(struct coresight_csr *csr, uint64_t addr,
			 uint32_t val)
{
	struct csr_drvdata *drvdata;
	int ret = 0;
	unsigned long flags;

	if (csr == NULL)
		return -EINVAL;

	drvdata = to_csr_drvdata(csr);
	if (IS_ERR_OR_NULL(drvdata) || !drvdata->hwctrl_set_support)
		return -EINVAL;

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);

	if (addr == (drvdata->pbase + CSR_STMEXTHWCTRL0))
		csr_writel(drvdata, val, CSR_STMEXTHWCTRL0);
	else if (addr == (drvdata->pbase + CSR_STMEXTHWCTRL1))
		csr_writel(drvdata, val, CSR_STMEXTHWCTRL1);
	else if (addr == (drvdata->pbase + CSR_STMEXTHWCTRL2))
		csr_writel(drvdata, val, CSR_STMEXTHWCTRL2);
	else if (addr == (drvdata->pbase + CSR_STMEXTHWCTRL3))
		csr_writel(drvdata, val, CSR_STMEXTHWCTRL3);
	else
		ret = -EINVAL;

	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
	return ret;
}
EXPORT_SYMBOL(coresight_csr_hwctrl_set);

void coresight_csr_set_byte_cntr(struct coresight_csr *csr, int irqctrl_offset, uint32_t count)
{
	struct csr_drvdata *drvdata;
	unsigned long flags;

	if (csr == NULL)
		return;

	drvdata = to_csr_drvdata(csr);
	if (IS_ERR_OR_NULL(drvdata) || !drvdata->set_byte_cntr_support)
		return;

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);

	csr_writel(drvdata, count, irqctrl_offset);

	/* make sure byte count value is written */
	mb();

	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
}
EXPORT_SYMBOL(coresight_csr_set_byte_cntr);

int coresight_csr_set_etr_atid(struct coresight_csr *csr,
			uint32_t atid_offset, uint32_t atid,
			bool enable)
{
	struct csr_drvdata *drvdata;
	unsigned long flags;
	uint32_t reg_offset;
	int bit;
	uint32_t val;

	if (csr == NULL)
		return -EINVAL;

	drvdata = to_csr_drvdata(csr);
	if (IS_ERR_OR_NULL(drvdata))
		return -EINVAL;

	if (atid < 0 || atid_offset <= 0)
		return -EINVAL;

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);

	reg_offset = CSR_ATID_REG_OFFSET(atid, atid_offset);
	bit = CSR_ATID_REG_BIT(atid);
	if (reg_offset - atid_offset > CSR_ATID_REG_SIZE
		|| bit >= CSR_MAX_ATID) {
		CSR_LOCK(drvdata);
		spin_unlock_irqrestore(&drvdata->spin_lock, flags);
		return -EINVAL;
	}

	val = csr_readl(drvdata, reg_offset);
	if (enable)
		val = val | BIT(bit);
	else
		val = val & ~BIT(bit);
	csr_writel(drvdata, val, reg_offset);

	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);

	return 0;
}
EXPORT_SYMBOL(coresight_csr_set_etr_atid);

struct coresight_csr *coresight_csr_get(const char *name)
{
	struct coresight_csr *csr;

	mutex_lock(&csr_lock);
	list_for_each_entry(csr, &csr_list, link) {
		if (!strcmp(csr->name, name)) {
			mutex_unlock(&csr_lock);
			return csr;
		}
	}

	mutex_unlock(&csr_lock);
	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(coresight_csr_get);

int of_get_coresight_csr_name(struct device_node *node, const char **csr_name)
{
	int ret;
	struct device_node *csr_node;

	csr_node = of_parse_phandle(node, "coresight-csr", 0);
	if (!csr_node)
		return -EINVAL;

	ret = of_property_read_string(csr_node, "coresight-name", csr_name);
	of_node_put(csr_node);
	return ret;
}
EXPORT_SYMBOL(of_get_coresight_csr_name);

static ssize_t timestamp_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t size = 0;
	uint64_t time_tick = 0;
	uint32_t val, time_val0, time_val1;
	int ret;
	unsigned long flags;

	struct csr_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (IS_ERR_OR_NULL(drvdata) || !drvdata->timestamp_support) {
		dev_err(dev, "Invalid param\n");
		return 0;
	}

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);

	val = csr_readl(drvdata, CSR_TIMESTAMPCTRL);

	val  = val & ~BIT(0);
	csr_writel(drvdata, val, CSR_TIMESTAMPCTRL);

	val  = val | BIT(0);
	csr_writel(drvdata, val, CSR_TIMESTAMPCTRL);

	time_val0 = csr_readl(drvdata, CSR_QDSSTIMEVAL0);
	time_val1 = csr_readl(drvdata, CSR_QDSSTIMEVAL1);

	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);

	clk_disable_unprepare(drvdata->clk);

	time_tick |= (uint64_t)time_val1 << 32;
	time_tick |= (uint64_t)time_val0;
	size = scnprintf(buf, PAGE_SIZE, "%llu\n", time_tick);
	dev_dbg(dev, "timestamp : %s\n", buf);
	return size;
}

static DEVICE_ATTR_RO(timestamp);

static ssize_t msr_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int i;
	ssize_t len = 0;
	struct csr_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (IS_ERR_OR_NULL(drvdata) || !drvdata->msr_support ||
			IS_ERR_OR_NULL(drvdata->msr))
		return -EINVAL;
	for (i = 0; i < MSR_NUM; i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d 0x%x\n",
			i, drvdata->msr[i]);
	return len;
}

static ssize_t msr_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	uint32_t offset, val, rval;
	int nval, ret;
	unsigned long flags;
	struct csr_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (IS_ERR_OR_NULL(drvdata) || !drvdata->msr_support ||
			IS_ERR_OR_NULL(drvdata->msr))
		return -EINVAL;

	nval = sscanf(buf, "%x %x", &offset, &val);
	if (nval != 2)
		return -EINVAL;
	if (offset >= MSR_NUM)
		return -EINVAL;

	if (atomic_read(drvdata->msr_refcnt) == 0) {
		ret = clk_prepare_enable(drvdata->clk);
		if (ret)
			return ret;
		atomic_inc(drvdata->msr_refcnt);
	}

	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);
	csr_writel(drvdata, val, drvdata->msr_start + offset * 4);
	rval = csr_readl(drvdata, drvdata->msr_start + offset * 4);
	drvdata->msr[offset] = rval;
	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
	return size;
}

static DEVICE_ATTR_RW(msr);

static ssize_t msr_reset_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	unsigned long flags, val;
	int i;
	struct csr_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (IS_ERR_OR_NULL(drvdata) || !drvdata->msr_support ||
			IS_ERR_OR_NULL(drvdata->msr))
		return -EINVAL;

	if (kstrtoul(buf, 0, &val) || val != 1)
		return -EINVAL;

	if (atomic_read(drvdata->msr_refcnt) == 0)
		return -EINVAL;

	atomic_set(drvdata->msr_refcnt, 0);
	spin_lock_irqsave(&drvdata->spin_lock, flags);
	CSR_UNLOCK(drvdata);
	for (i = 0; i < MSR_NUM; i++) {
		csr_writel(drvdata, 0, drvdata->msr_start + i * 4);
		drvdata->msr[i] = 0;
	}
	CSR_LOCK(drvdata);
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
	clk_disable_unprepare(drvdata->clk);
	return size;
}

static DEVICE_ATTR_WO(msr_reset);

static ssize_t flushperiod_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct csr_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (IS_ERR_OR_NULL(drvdata) || !drvdata->usb_bam_support) {
		dev_err(dev, "Invalid param\n");
		return -EINVAL;
	}

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", drvdata->flushperiod);
}

static ssize_t flushperiod_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t size)
{
	unsigned long flags;
	unsigned long val;
	struct csr_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (IS_ERR_OR_NULL(drvdata) || !drvdata->usb_bam_support) {
		dev_err(dev, "Invalid param\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&drvdata->spin_lock, flags);

	if (kstrtoul(buf, 0, &val) || val > 0xffff) {
		spin_unlock_irqrestore(&drvdata->spin_lock, flags);
		return -EINVAL;
	}

	if (drvdata->flushperiod == val)
		goto out;

	drvdata->flushperiod = val;

	if (drvdata->enable_flush)
		msm_qdss_csr_config_flush_period(drvdata);

out:
	spin_unlock_irqrestore(&drvdata->spin_lock, flags);
	return size;
}

static DEVICE_ATTR_RW(flushperiod);

static struct attribute *swao_csr_attrs[] = {
	&dev_attr_timestamp.attr,
	&dev_attr_msr.attr,
	&dev_attr_msr_reset.attr,
	NULL,
};

static struct attribute_group swao_csr_attr_grp = {
	.attrs = swao_csr_attrs,
};

static const struct attribute_group *swao_csr_attr_grps[] = {
	&swao_csr_attr_grp,
	NULL,
};

static struct attribute *csr_attrs[] = {
	&dev_attr_flushperiod.attr,
	NULL,
};

static struct attribute_group csr_attr_grp = {
	.attrs = csr_attrs,
};

static const struct attribute_group *csr_attr_grps[] = {
	&csr_attr_grp,
	NULL,
};

static int csr_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct csr_drvdata *drvdata;
	struct resource *res, *msr_res;
	struct coresight_desc desc = { 0 };

	desc.name = coresight_alloc_device_name(&csr_devs, dev);
	if (!desc.name)
		return -ENOMEM;
	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	drvdata->clk = devm_clk_get(dev, "apb_pclk");
	if (IS_ERR(drvdata->clk))
		dev_dbg(dev, "csr not config clk\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csr-base");
	if (!res)
		return -ENODEV;
	drvdata->pbase = res->start;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,blk-size",
			&drvdata->blksize);
	if (ret)
		drvdata->blksize = BLKSIZE_256;

	drvdata->usb_bam_support = of_property_read_bool(pdev->dev.of_node,
						"qcom,usb-bam-support");
	if (!drvdata->usb_bam_support)
		dev_dbg(dev, "usb_bam support handled by other subsystem\n");
	else
		dev_dbg(dev, "usb_bam operation supported\n");

	drvdata->hwctrl_set_support = of_property_read_bool(pdev->dev.of_node,
						"qcom,hwctrl-set-support");
	if (!drvdata->hwctrl_set_support)
		dev_dbg(dev, "hwctrl_set_support handled by other subsystem\n");
	else
		dev_dbg(dev, "hwctrl_set_support operation supported\n");

	drvdata->set_byte_cntr_support = of_property_read_bool(
			pdev->dev.of_node, "qcom,set-byte-cntr-support");
	if (!drvdata->set_byte_cntr_support)
		dev_dbg(dev, "set byte_cntr_support handled by other subsystem\n");
	else
		dev_dbg(dev, "set_byte_cntr_support operation supported\n");

	drvdata->timestamp_support = of_property_read_bool(pdev->dev.of_node,
						"qcom,timestamp-support");
	if (!drvdata->timestamp_support)
		dev_dbg(dev, "timestamp_support handled by other subsystem\n");
	else
		dev_dbg(dev, "timestamp_support operation supported\n");

	drvdata->perflsheot_set_support = of_property_read_bool(
			pdev->dev.of_node, "qcom,perflsheot-set-support");
	if (!drvdata->perflsheot_set_support)
		dev_dbg(dev, "perflsheot_set_support handled by other subsystem\n");
	else
		dev_dbg(dev, "perflsheot_set_support operation supported\n");

	if (drvdata->usb_bam_support)
		drvdata->flushperiod = FLUSHPERIOD_1;
	drvdata->msr_support = of_property_read_bool(pdev->dev.of_node,
						"qcom,msr-support");
	if (!drvdata->msr_support) {
		dev_dbg(dev, "msr_support handled by other subsystem\n");
	} else {
		msr_res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				"msr-base");
		if (!msr_res || msr_res->start < res->start || msr_res->end
		> res->end)
			return -ENODEV;
		drvdata->msr_start = msr_res->start - res->start;
		drvdata->msr_end = msr_res->end - res->start;
		drvdata->msr = devm_kzalloc(dev, MSR_NUM * sizeof(uint32_t),
						GFP_KERNEL);
		if (!drvdata->msr)
			return -ENOMEM;

		drvdata->msr_refcnt = devm_kzalloc(dev, sizeof(atomic_t),
				GFP_KERNEL);
		if (!drvdata->msr_refcnt)
			return -ENOMEM;
		atomic_set(drvdata->msr_refcnt, 0);
		dev_dbg(dev, "msr_support operation supported\n");
	}

	desc.type = CORESIGHT_DEV_TYPE_NONE;
	desc.pdata = pdev->dev.platform_data;
	desc.dev = &pdev->dev;
	if (drvdata->timestamp_support || drvdata->msr_support)
		desc.groups = swao_csr_attr_grps;
	else if (drvdata->usb_bam_support)
		desc.groups = csr_attr_grps;

	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	/* Store the driver data pointer for use in exported functions */
	spin_lock_init(&drvdata->spin_lock);
	drvdata->csr.name = desc.name;

	mutex_lock(&csr_lock);
	list_add_tail(&drvdata->csr.link, &csr_list);
	mutex_unlock(&csr_lock);

	dev_info(dev, "CSR initialized: %s\n", drvdata->csr.name);
	return 0;
}

static int csr_remove(struct platform_device *pdev)
{
	struct csr_drvdata *drvdata = platform_get_drvdata(pdev);

	mutex_lock(&csr_lock);
	list_del(&drvdata->csr.link);
	mutex_unlock(&csr_lock);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static const struct of_device_id csr_match[] = {
	{.compatible = "qcom,coresight-csr"},
	{}
};

static struct platform_driver csr_driver = {
	.probe          = csr_probe,
	.remove         = csr_remove,
	.driver         = {
		.name   = "coresight-csr",
		.of_match_table = csr_match,
		.suppress_bind_attrs = true,
	},
};

static int __init csr_init(void)
{
	return platform_driver_register(&csr_driver);
}
module_init(csr_init);

static void __exit csr_exit(void)
{
	platform_driver_unregister(&csr_driver);
}
module_exit(csr_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight CSR driver");
