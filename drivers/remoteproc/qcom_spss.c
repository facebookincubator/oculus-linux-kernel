// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Qualcomm Technologies, Inc. SPSS Peripheral Image Loader
 *
 */

#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/qcom_scm.h>
#include <linux/regulator/consumer.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/qcom_spss.h>
#include <linux/rpmsg/qcom_glink.h>
#include <linux/soc/qcom/mdt_loader.h>

#include "qcom_common.h"
#include "remoteproc_internal.h"

#define ERR_READY	0
#define PBL_DONE	1
#define SPSS_WDOG_ERR	0x44554d50
#define SPSS_POLL_RETRIES_NUM	20
#define SPSS_POLL_TIMEOUT_MS	500
#define SPSS_WAIT_TIMEOUT	(SPSS_POLL_TIMEOUT_MS * SPSS_POLL_RETRIES_NUM)

/* err_status definitions                       */
#define PBL_LOG_VALUE                 (0xef000000)
#define PBL_LOG_MASK                  (0xff000000)

/* This bit will be set by UEFI driver if SPSS failed to load */
#define SP_SCSR_SPSS_LOAD_FAILURE_MASK		BIT(0)

#define to_glink_subdev(d) container_of(d, struct qcom_rproc_glink, subdev)

struct spss_data {
	const char *firmware_name;
	int pas_id;
	const char *ssr_name;
	bool auto_boot;
};

struct qcom_spss {
	struct device *dev;
	struct rproc *rproc;

	struct clk *xo;

	struct reg_info cx;

	int pas_id;

	struct completion start_done;

	phys_addr_t mem_phys;
	phys_addr_t mem_reloc;
	void *mem_region;
	size_t mem_size;
	int generic_irq;

	struct qcom_rproc_glink glink_subdev;
	struct qcom_rproc_ssr ssr_subdev;
	struct qcom_sysmon *sysmon_subdev;
	void __iomem *irq_status;
	void __iomem *irq_clr;
	void __iomem *irq_mask;
	void __iomem *err_status;
	void __iomem *err_status_spare;
	void __iomem *rmb_gpm;
	u32 bits_arr[2];
};

/* Forward declarations */
static bool spss_check_irq(struct qcom_spss *spss);

/* When SPSS mitigation is enabled then access-control's resource-group-1
 * can be written to only by SPU and TME_FW. Hence, all writes
 * in this driver to SP_CNOC_SP_SCSR_RMB_SP2SOC_IRQ_CLR and
 * SP_CNOC_SP_SCSR_RMB_SP2SOC_IRQ_MASK are disabled. They are replaced by
 * changes in TME_FW (masking all interrupts before starting SPSS) and in SPU
 * (clearing and un-masking IRQ as needed)
 */
#if IS_ENABLED(CONFIG_QCOM_SPSS_AC_RESTRICTION)
#define SPSS_CLEAR_IRQ(_bit, _spss) /* Empty */

static void mask_scsr_irqs(struct qcom_spss *spss)
{
	(void)(spss);
}

int spss_wait_for_start_done(struct qcom_spss *spss)
{
	int i, ret;

	/* Check IRQ status explicitly before starting to wait */
	if (spss_check_irq(spss))
		return 1;

	for (i = 0, ret = 0; i < SPSS_POLL_RETRIES_NUM && ret == 0; i++) {
		ret = wait_for_completion_timeout(&spss->start_done,
				msecs_to_jiffies(SPSS_POLL_TIMEOUT_MS));
		if (ret != 0) {
			/* Completed */
			break;
		}

		/* Timed-out - check IRQ status explicitly */
		ret = spss_check_irq(spss);
	}

	return ret;
}
#else
#define SPSS_CLEAR_IRQ(_bit, _spss) \
	__raw_writel((_bit), (_spss)->irq_clr)

static void mask_scsr_irqs(struct qcom_spss *spss)
{
	uint32_t mask_val;

	/* Masking all interrupts */
	mask_val = ~0;
	__raw_writel(mask_val,  spss->irq_mask);
}

static void unmask_scsr_irqs(struct qcom_spss *spss)
{
	uint32_t mask_val;

	/* unmasking interrupts handled by HLOS */
	mask_val = ~0;
	__raw_writel(mask_val & ~BIT(spss->bits_arr[ERR_READY]) &
		     ~BIT(spss->bits_arr[PBL_DONE]), spss->irq_mask);
}

int spss_wait_for_start_done(struct qcom_spss *spss)
{
	unmask_scsr_irqs(spss);

	return wait_for_completion_timeout(&spss->start_done,
			msecs_to_jiffies(SPSS_WAIT_TIMEOUT));
}
#endif /* CONFIG_QCOM_SPSS_AC_RESTRICTION */

static int glink_spss_subdev_start(struct rproc_subdev *subdev)
{
	struct qcom_rproc_glink *glink = to_glink_subdev(subdev);

	glink->edge = qcom_glink_spss_register(glink->dev, glink->node);

	return PTR_ERR_OR_ZERO(glink->edge);
}

static void glink_spss_subdev_stop(struct rproc_subdev *subdev, bool crashed)
{
	struct qcom_rproc_glink *glink = to_glink_subdev(subdev);

	qcom_glink_spss_unregister(glink->edge);
	glink->edge = NULL;
}

static void glink_spss_subdev_unprepare(struct rproc_subdev *subdev)
{
	struct qcom_rproc_glink *glink = to_glink_subdev(subdev);

	qcom_glink_ssr_notify(glink->ssr_name);
}

/**
 * qcom_add_glink_spss_subdev() - try to add a GLINK SPSS subdevice to rproc
 * @rproc:	rproc handle to parent the subdevice
 * @glink:	reference to a GLINK subdev context
 * @ssr_name:	identifier of the associated remoteproc for ssr notifications
 */
static void qcom_add_glink_spss_subdev(struct rproc *rproc,
				       struct qcom_rproc_glink *glink,
				       const char *ssr_name)
{
	struct device *dev = &rproc->dev;

	glink->node = of_get_child_by_name(dev->parent->of_node, "glink-edge");
	if (!glink->node)
		return;

	glink->ssr_name = kstrdup_const(ssr_name, GFP_KERNEL);
	if (!glink->ssr_name)
		return;

	glink->dev = dev;
	glink->subdev.start = glink_spss_subdev_start;
	glink->subdev.stop = glink_spss_subdev_stop;
	glink->subdev.unprepare = glink_spss_subdev_unprepare;

	rproc_add_subdev(rproc, &glink->subdev);
}

/**
 * qcom_remove_glink_spss_subdev() - remove a GLINK SPSS subdevice from rproc
 * @rproc:	rproc handle
 * @glink:	reference to a GLINK subdev context
 */
static void qcom_remove_glink_spss_subdev(struct rproc *rproc,
					  struct qcom_rproc_glink *glink)
{
	if (!glink->node)
		return;

	rproc_remove_subdev(rproc, &glink->subdev);
	kfree_const(glink->ssr_name);
	of_node_put(glink->node);
}

static void clear_pbl_done(struct qcom_spss *spss)
{
	uint32_t err_value, rmb_err_spare0, rmb_err_spare1, rmb_err_spare2;

	err_value =  __raw_readl(spss->err_status);
	rmb_err_spare2 =  __raw_readl(spss->err_status_spare);
	rmb_err_spare1 =  __raw_readl(spss->err_status_spare-4);
	rmb_err_spare0 =  __raw_readl(spss->err_status_spare-8);

	if (err_value) {
		dev_err(spss->dev, "PBL error status register: 0x%08x, spare0 register: 0x%08x, spare1 register: 0x%08x, spare2 register: 0x%08x\n",
			err_value, rmb_err_spare0, rmb_err_spare1, rmb_err_spare2);
	} else
		dev_info(spss->dev, "PBL_DONE - 1st phase loading [%s] completed ok\n",
			 spss->rproc->name);

	SPSS_CLEAR_IRQ(BIT(spss->bits_arr[PBL_DONE]), spss);
}

static void clear_err_ready(struct qcom_spss *spss)
{
	dev_info(spss->dev, "SW_INIT_DONE - 2nd phase loading [%s] completed ok\n",
		 spss->rproc->name);

	SPSS_CLEAR_IRQ(BIT(spss->bits_arr[ERR_READY]), spss);
	complete(&spss->start_done);
}

static void clear_sw_init_done_error(struct qcom_spss *spss, int err)
{
	uint32_t rmb_err_spare0;
	uint32_t rmb_err_spare1;
	uint32_t rmb_err_spare2;

	dev_info(spss->dev, "SW_INIT_DONE - ERROR [%s] [0x%x].\n", spss->rproc->name, err);

	rmb_err_spare2 =  __raw_readl(spss->err_status_spare);
	rmb_err_spare1 =  __raw_readl(spss->err_status_spare-4);
	rmb_err_spare0 =  __raw_readl(spss->err_status_spare-8);

	dev_err(spss->dev, "spare0 register: 0x%08x, spare1 register: 0x%08x, spare2 register: 0x%08x\n",
		rmb_err_spare0, rmb_err_spare1, rmb_err_spare2);

	/* Clear the interrupt source */
	SPSS_CLEAR_IRQ(BIT(spss->bits_arr[ERR_READY]), spss);
}

static void clear_wdog(struct qcom_spss *spss)
{
	dev_err(spss->dev, "wdog bite received from %s!\n", spss->rproc->name);
	if (spss->rproc->recovery_disabled) {
		spss->rproc->state = RPROC_CRASHED;
		panic("Panicking, remoterpoc %s crashed\n", spss->rproc->name);
	}

	SPSS_CLEAR_IRQ(BIT(spss->bits_arr[ERR_READY]), spss);
	rproc_report_crash(spss->rproc, RPROC_WATCHDOG);
}

/**
 * spss_check_irq() - check SP2SOC IRQ and errors explicitly, regardless of
 * IRQ mask.
 * Returns true if ERR_READY IRQ is set without errors, or false otherwise.
 *
 * @spss:  pointer to SPSS private data
 */
static bool spss_check_irq(struct qcom_spss *spss)
{
	bool ret = false;
	uint32_t status_val, err_value;

	err_value =  __raw_readl(spss->err_status_spare);
	status_val = __raw_readl(spss->irq_status);

	if (status_val & BIT(spss->bits_arr[ERR_READY])) {
		if (!err_value) {
			clear_err_ready(spss);
			ret = true;
		}
		else if (err_value == SPSS_WDOG_ERR)
			clear_wdog(spss);
		else
			clear_sw_init_done_error(spss, err_value);
	}

	if (status_val & BIT(spss->bits_arr[PBL_DONE]))
		clear_pbl_done(spss);

	return ret;
}

static irqreturn_t spss_generic_handler(int irq, void *dev_id)
{
	struct qcom_spss *spss = dev_id;

	spss_check_irq(spss);

	return IRQ_HANDLED;
}

static bool check_status(struct qcom_spss *spss, int *ret_error)
{
	uint32_t status_val, err_value, rmb_err;

	err_value =  __raw_readl(spss->err_status_spare);
	status_val = __raw_readl(spss->irq_status);
	rmb_err = __raw_readl(spss->err_status);

	if ((rmb_err & PBL_LOG_MASK) == PBL_LOG_VALUE) {
		dev_err(spss->dev, "PBL error detected\n");
		*ret_error = rmb_err;
		return true;
	}

	if ((status_val & BIT(spss->bits_arr[ERR_READY])) && err_value == SPSS_WDOG_ERR) {
		dev_err(spss->dev, "wdog bite is pending\n");
		SPSS_CLEAR_IRQ(BIT(spss->bits_arr[ERR_READY]), spss);
		*ret_error = -ETIMEDOUT;
		return true;
	}
	return false;
}

static int spss_load(struct rproc *rproc, const struct firmware *fw)
{
	struct qcom_spss *spss = (struct qcom_spss *)rproc->priv;

	return qcom_mdt_load(spss->dev, fw, rproc->firmware, spss->pas_id,
			     spss->mem_region, spss->mem_phys, spss->mem_size,
			     &spss->mem_reloc);
}

static int spss_stop(struct rproc *rproc)
{
	struct qcom_spss *spss = (struct qcom_spss *)rproc->priv;
	int ret;

	ret = qcom_scm_pas_shutdown(spss->pas_id);
	if (ret)
		panic("Panicking, remoteproc %s failed to shutdown.\n", rproc->name);

	mask_scsr_irqs(spss);

	/* Set state as OFFLINE */
	rproc->state = RPROC_OFFLINE;
	reinit_completion(&spss->start_done);

	return ret;
}

static int spss_attach(struct rproc *rproc)
{
	struct qcom_spss *spss = (struct qcom_spss *)rproc->priv;
	int ret = 0;

	/* If rproc already crashed stop it and propagate error */
	if (check_status(spss, &ret)) {
		dev_err(spss->dev, "Failed to attach SPSS remote proc and shutdown\n");
		spss_stop(rproc);
		return ret;
	}
	/* If booted successfully then wait for init_done*/

	ret = spss_wait_for_start_done(spss);

	if (rproc->recovery_disabled && !ret) {
		dev_err(spss->dev, "%d ms timeout poked\n", SPSS_WAIT_TIMEOUT);
		panic("Panicking, %s attach timed out\n", rproc->name);
	} else if (!ret) {
		dev_err(spss->dev, "recovery disabled (after timeout)\n");
	}

	ret = ret ? 0 : -ETIMEDOUT;

	return ret;
}

static inline void disable_regulator(struct reg_info *regulator)
{
	regulator_set_voltage(regulator->reg, 0, INT_MAX);
	regulator_set_load(regulator->reg, 0);
	regulator_disable(regulator->reg);
}

static inline int enable_regulator(struct reg_info *regulator)
{
	regulator_set_voltage(regulator->reg, regulator->uV, INT_MAX);
	regulator_set_load(regulator->reg, regulator->uA);
	return regulator_enable(regulator->reg);
}

static int spss_start(struct rproc *rproc)
{
	struct qcom_spss *spss = (struct qcom_spss *)rproc->priv;
	int ret = 0;

	ret = clk_prepare_enable(spss->xo);
	if (ret)
		return ret;

	ret = enable_regulator(&spss->cx);
	if (ret)
		goto disable_xo_clk;

	ret = qcom_scm_pas_auth_and_reset(spss->pas_id);
	if (ret)
		panic("Panicking, auth and reset failed for remoteproc %s\n", rproc->name);

	ret = spss_wait_for_start_done(spss);

	if (rproc->recovery_disabled && !ret)
		panic("Panicking, %s start timed out\n", rproc->name);
	else if (!ret)
		dev_err(spss->dev, "start timed out\n");

	ret = ret ? 0 : -ETIMEDOUT;

	disable_regulator(&spss->cx);
disable_xo_clk:
	clk_disable_unprepare(spss->xo);
	return ret;
}

static void *spss_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct qcom_spss *spss = (struct qcom_spss *)rproc->priv;
	int offset;

	offset = da - spss->mem_reloc;
	if (offset < 0 || offset + len > spss->mem_size) {
		dev_err(&rproc->dev, "offset: %llx, da: %llx, len: %llx\n", offset, da, len);
		return NULL;
	}

	return spss->mem_region + offset;
}

static const struct rproc_ops spss_ops = {
	.stop = spss_stop,
	.da_to_va = spss_da_to_va,
	.load = spss_load,
	.attach = spss_attach,
	.parse_fw = qcom_register_dump_segments,
	.start = spss_start,
};

static int spss_init_clock(struct qcom_spss *spss)
{
	int ret;

	spss->xo = devm_clk_get(spss->dev, "xo");
	if (IS_ERR(spss->xo)) {
		ret = PTR_ERR(spss->xo);
		if (ret != -EPROBE_DEFER)
			dev_err(spss->dev, "failed to get xo clock\n");
		return ret;
	}

	return 0;
}

static int init_regulator(struct device *dev, struct reg_info *regulator, const char *reg_name)
{
	int len, rc;
	char uv_ua[50];
	u32 uv_ua_vals[2];

	regulator->reg = devm_regulator_get(dev, reg_name);
	if (IS_ERR(regulator->reg))
		return PTR_ERR(regulator->reg);

	snprintf(uv_ua, sizeof(uv_ua), "%s-uV-uA", reg_name);
	if (!of_find_property(dev->of_node, uv_ua, &len))
		return -EINVAL;

	rc = of_property_read_u32_array(dev->of_node, uv_ua,
					uv_ua_vals,
					ARRAY_SIZE(uv_ua_vals));
	if (rc) {
		dev_err(dev, "Failed to read uV-uA value(rc:%d)\n", rc);
		return rc;
	}
	if (uv_ua_vals[0] > 0)
		regulator->uV = uv_ua_vals[0];
	if (uv_ua_vals[1] > 0)
		regulator->uA = uv_ua_vals[1];

	return 0;
}

static int spss_alloc_memory_region(struct qcom_spss *spss)
{
	struct device_node *node;
	struct resource r;
	int ret, extra_size = 0;

	node = of_parse_phandle(spss->dev->of_node, "memory-region", 0);
	if (!node) {
		dev_err(spss->dev, "no memory-region specified\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(node, 0, &r);
	if (ret)
		return ret;

	ret = of_property_read_u32(spss->dev->of_node, "qcom,extra-size", &extra_size);

	spss->mem_phys = spss->mem_reloc = r.start;
	spss->mem_size = resource_size(&r) + extra_size;
	spss->mem_region = devm_ioremap_wc(spss->dev, spss->mem_phys, spss->mem_size);
	if (!spss->mem_region) {
		dev_err(spss->dev, "unable to map memory region: %pa+%zx\n",
			&r.start, spss->mem_size);
		return -EBUSY;
	}

	return 0;
}

static int qcom_spss_init_mmio(struct platform_device *pdev, struct qcom_spss *spss)
{
	struct resource *res;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rmb_general_purpose");
	spss->rmb_gpm = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spss->rmb_gpm))
		return PTR_ERR(spss->rmb_gpm);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sp2soc_irq_status");
	spss->irq_status = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spss->irq_status))
		return PTR_ERR(spss->irq_status);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sp2soc_irq_clr");
	spss->irq_clr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spss->irq_clr))
		return PTR_ERR(spss->irq_clr);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sp2soc_irq_mask");
	spss->irq_mask = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spss->irq_mask))
		return PTR_ERR(spss->irq_mask);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rmb_err");
	spss->err_status = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spss->err_status))
		return PTR_ERR(spss->err_status);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rmb_err_spare2");
	spss->err_status_spare = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(spss->err_status_spare))
		return PTR_ERR(spss->err_status_spare);

	ret = of_property_read_u32_array(pdev->dev.of_node, "qcom,spss-scsr-bits", spss->bits_arr,
					 ARRAY_SIZE(spss->bits_arr));
	if (ret)
		return ret;

	return 0;

}

int qcom_spss_set_fw_name(struct rproc *rproc, const char *fw_name)
{
	const char *p;

	p = kstrdup_const(fw_name, GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	mutex_lock(&rproc->lock);
	kfree(rproc->firmware);
	rproc->firmware = p;
	mutex_unlock(&rproc->lock);

	return 0;
}
EXPORT_SYMBOL(qcom_spss_set_fw_name);

static int qcom_spss_probe(struct platform_device *pdev)
{
	const struct spss_data *desc;
	struct qcom_spss *spss;
	struct rproc *rproc;
	const char *fw_name;
	int ret;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	if (!qcom_scm_is_available())
		return -EPROBE_DEFER;

	fw_name = desc->firmware_name;
	rproc = rproc_alloc(&pdev->dev, pdev->name, &spss_ops, fw_name, sizeof(*spss));
	if (!rproc) {
		dev_err(&pdev->dev, "unable to allocate remoteproc\n");
		return -ENOMEM;
	}

	spss = (struct qcom_spss *)rproc->priv;
	spss->dev = &pdev->dev;
	spss->rproc = rproc;
	spss->pas_id = desc->pas_id;
	init_completion(&spss->start_done);
	platform_set_drvdata(pdev, spss);
	rproc->auto_boot = desc->auto_boot;
	rproc->recovery_disabled = true;
	rproc_coredump_set_elf_info(rproc, ELFCLASS32, EM_NONE);

	ret = device_init_wakeup(spss->dev, true);
	if (ret)
		goto free_rproc;

	ret = qcom_spss_init_mmio(pdev, spss);
	if (ret)
		goto deinit_wakeup_source;

	if (!(__raw_readl(spss->rmb_gpm) & SP_SCSR_SPSS_LOAD_FAILURE_MASK) &&
			!(__raw_readl(spss->err_status_spare-4) & SP_SCSR_SPSS_LOAD_FAILURE_MASK))
		rproc->state = RPROC_DETACHED;
	else
		rproc->state = RPROC_OFFLINE;

	ret = spss_alloc_memory_region(spss);
	if (ret)
		goto deinit_wakeup_source;

	ret = spss_init_clock(spss);
	if (ret)
		goto deinit_wakeup_source;

	ret = init_regulator(spss->dev, &spss->cx, "cx");
	if (ret)
		goto deinit_wakeup_source;

	qcom_add_glink_spss_subdev(rproc, &spss->glink_subdev, "spss");
	spss->sysmon_subdev = qcom_add_sysmon_subdev(rproc, desc->ssr_name, -EINVAL);
	if (IS_ERR(spss->sysmon_subdev)) {
		dev_err(spss->dev, "failed to add sysmon subdevice\n");
		goto deinit_wakeup_source;
	}

	qcom_add_ssr_subdev(rproc, &spss->ssr_subdev, desc->ssr_name);
	mask_scsr_irqs(spss);
	spss->generic_irq = platform_get_irq(pdev, 0);
	ret = devm_request_threaded_irq(&pdev->dev, spss->generic_irq, NULL, spss_generic_handler,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT, "generic-irq", spss);
	if (ret) {
		dev_err(&pdev->dev, "failed to acquire generic IRQ\n");
		goto remove_subdev;
	}

	ret = rproc_add(rproc);
	if (ret)
		goto remove_subdev;

	return 0;

remove_subdev:
	qcom_remove_sysmon_subdev(spss->sysmon_subdev);
deinit_wakeup_source:
	device_init_wakeup(spss->dev, false);
free_rproc:
	rproc_free(rproc);

	return ret;
}

static int qcom_spss_remove(struct platform_device *pdev)
{
	struct qcom_spss *spss = platform_get_drvdata(pdev);

	rproc_del(spss->rproc);
	qcom_remove_glink_spss_subdev(spss->rproc, &spss->glink_subdev);
	qcom_remove_ssr_subdev(spss->rproc, &spss->ssr_subdev);
	qcom_remove_sysmon_subdev(spss->sysmon_subdev);
	device_init_wakeup(spss->dev, false);
	rproc_free(spss->rproc);

	return 0;
}

static const struct spss_data spss_resource_init = {
		.firmware_name = "spss.mdt",
		.pas_id = 14,
		.ssr_name = "spss",
		.auto_boot = false,
};

static const struct of_device_id spss_of_match[] = {
	{ .compatible = "qcom,waipio-spss-pas", .data = &spss_resource_init},
	{ .compatible = "qcom,cape-spss-pas", .data = &spss_resource_init},
	{ .compatible = "qcom,anorak-spss-pas", .data = &spss_resource_init},
	{ },
};
MODULE_DEVICE_TABLE(of, spss_of_match);

static struct platform_driver spss_driver = {
	.probe = qcom_spss_probe,
	.remove = qcom_spss_remove,
	.driver = {
		.name = "qcom_spss",
		.of_match_table = spss_of_match,
	},
};

module_platform_driver(spss_driver);
MODULE_DESCRIPTION("QTI Peripheral Image Loader for Secure Subsystem");
MODULE_LICENSE("GPL v2");
