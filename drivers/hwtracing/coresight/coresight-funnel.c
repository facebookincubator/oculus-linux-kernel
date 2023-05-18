// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2011-2012,2017, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight Funnel driver
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/coresight.h>
#include <linux/amba/bus.h>
#include <linux/clk.h>
#include <linux/of_address.h>

#include "coresight-priv.h"

#define FUNNEL_FUNCTL		0x000
#define FUNNEL_PRICTL		0x004

#define FUNNEL_HOLDTIME_MASK	0xf00
#define FUNNEL_HOLDTIME_SHFT	0x8
#define FUNNEL_HOLDTIME		(0x7 << FUNNEL_HOLDTIME_SHFT)
#define FUNNEL_ENSx_MASK	0xff

/**
 * struct funnel_drvdata - specifics associated to a funnel component
 * @base:	memory mapped base address for this component.
 * @dev:	the device entity associated to this component.
 * @atclk:	optional clock for the core parts of the funnel.
 * @csdev:	component vitals needed by the framework.
 * @priority:	port selection order.
 * @spinlock:	serialize enable/disable operations.
 */
struct funnel_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct clk		*atclk;
	struct coresight_device	*csdev;
	unsigned long		priority;
	spinlock_t		spinlock;
};

static int dynamic_funnel_enable_hw(struct funnel_drvdata *drvdata, int port)
{
	u32 functl;
	int rc = 0;

	CS_UNLOCK(drvdata->base);

	functl = readl_relaxed(drvdata->base + FUNNEL_FUNCTL);
	/* Claim the device only when we enable the first slave */
	if (!(functl & FUNNEL_ENSx_MASK)) {
		rc = coresight_claim_device_unlocked(drvdata->base);
		if (rc)
			goto done;
	}

	functl &= ~FUNNEL_HOLDTIME_MASK;
	functl |= FUNNEL_HOLDTIME;
	functl |= (1 << port);
	writel_relaxed(functl, drvdata->base + FUNNEL_FUNCTL);
	writel_relaxed(drvdata->priority, drvdata->base + FUNNEL_PRICTL);
done:
	CS_LOCK(drvdata->base);
	return rc;
}

static int funnel_enable(struct coresight_device *csdev, int inport,
			 int outport)
{
	int rc = 0;
	struct funnel_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;
	bool first_enable = false;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (atomic_read(&csdev->refcnt[inport]) == 0) {
		if (drvdata->base)
			rc = dynamic_funnel_enable_hw(drvdata, inport);
		if (!rc)
			first_enable = true;
	}
	if (!rc)
		atomic_inc(&csdev->refcnt[inport]);
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (first_enable)
		dev_info(drvdata->dev, "FUNNEL inport %d enabled\n", inport);
	return rc;
}

static void dynamic_funnel_disable_hw(struct funnel_drvdata *drvdata,
				      int inport)
{
	u32 functl;

	CS_UNLOCK(drvdata->base);

	functl = readl_relaxed(drvdata->base + FUNNEL_FUNCTL);
	functl &= ~(1 << inport);
	writel_relaxed(functl, drvdata->base + FUNNEL_FUNCTL);

	/* Disclaim the device if none of the slaves are now active */
	if (!(functl & FUNNEL_ENSx_MASK))
		coresight_disclaim_device_unlocked(drvdata->base);

	CS_LOCK(drvdata->base);
}

static void funnel_disable(struct coresight_device *csdev, int inport,
			   int outport)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	unsigned long flags;
	bool last_disable = false;

	spin_lock_irqsave(&drvdata->spinlock, flags);
	if (atomic_dec_return(&csdev->refcnt[inport]) == 0) {
		if (drvdata->base)
			dynamic_funnel_disable_hw(drvdata, inport);
		last_disable = true;
	}
	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	if (last_disable)
		dev_dbg(drvdata->dev, "FUNNEL inport %d disabled\n", inport);
}

static const struct coresight_ops_link funnel_link_ops = {
	.enable		= funnel_enable,
	.disable	= funnel_disable,
};

static const struct coresight_ops funnel_cs_ops = {
	.link_ops	= &funnel_link_ops,
};

static ssize_t priority_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->priority;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t priority_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	drvdata->priority = val;
	return size;
}
static DEVICE_ATTR_RW(priority);

static u32 get_funnel_ctrl_hw(struct funnel_drvdata *drvdata)
{
	u32 functl;

	CS_UNLOCK(drvdata->base);
	functl = readl_relaxed(drvdata->base + FUNNEL_FUNCTL);
	CS_LOCK(drvdata->base);

	return functl;
}

static ssize_t funnel_ctrl_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	u32 val;
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);

	pm_runtime_get_sync(drvdata->dev);

	val = get_funnel_ctrl_hw(drvdata);

	pm_runtime_put(drvdata->dev);

	return scnprintf(buf, PAGE_SIZE, "%#x\n", val);
}
static DEVICE_ATTR_RO(funnel_ctrl);

static struct attribute *coresight_funnel_attrs[] = {
	&dev_attr_funnel_ctrl.attr,
	&dev_attr_priority.attr,
	NULL,
};
ATTRIBUTE_GROUPS(coresight_funnel);

static int funnel_get_resource_byname(struct device_node *np,
				   char *ch_base, struct resource *res)
{
	const char *name = NULL;
	int index = 0, found = 0;

	while (!of_property_read_string_index(np, "reg-names", index, &name)) {
		if (strcmp(ch_base, name)) {
			index++;
			continue;
		}

		/* We have a match and @index is where it's at */
		found = 1;
		break;
	}

	if (!found)
		return -EINVAL;

	return of_address_to_resource(np, index, res);
}

static int funnel_probe(struct device *dev, struct resource *res)
{
	int ret;
	void __iomem *base;
	struct coresight_platform_data *pdata = NULL;
	struct funnel_drvdata *drvdata;
	struct resource res_real;
	struct coresight_desc desc = { 0 };
	struct device_node *np = dev->of_node;

	if (np) {
		pdata = of_get_coresight_platform_data(dev, np);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		dev->platform_data = pdata;
	}

	if (of_device_is_compatible(np, "arm,coresight-funnel"))
		pr_warn_once("Uses OBSOLETE CoreSight funnel binding\n");

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	drvdata->atclk = devm_clk_get(dev, "atclk"); /* optional */
	if (!IS_ERR(drvdata->atclk)) {
		ret = clk_prepare_enable(drvdata->atclk);
		if (ret)
			return ret;
	}


	if (of_property_read_bool(np, "qcom,duplicate-funnel")) {
		ret = funnel_get_resource_byname(np, "funnel-base-real",
						 &res_real);
		if (ret)
			return ret;


		res = &res_real;
		base = devm_ioremap(dev, res->start, resource_size(res));
		if (IS_ERR(base)) {
			ret = PTR_ERR(base);
			goto out_disable_clk;
		}
		drvdata->base = base;
		desc.groups = coresight_funnel_groups;
	} else if (res) {
		/*
		 * Map the device base for dynamic-funnel, which has been
		 * validated by AMBA core.
		 */
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base)) {
			ret = PTR_ERR(base);
			goto out_disable_clk;
		}
		drvdata->base = base;
		desc.groups = coresight_funnel_groups;
	}

	dev_set_drvdata(dev, drvdata);

	spin_lock_init(&drvdata->spinlock);
	desc.type = CORESIGHT_DEV_TYPE_LINK;
	desc.subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_MERG;
	desc.ops = &funnel_cs_ops;
	desc.pdata = pdata;
	desc.dev = dev;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto out_disable_clk;
	}

	pm_runtime_put(dev);
	ret = 0;

out_disable_clk:
	if (ret && !IS_ERR_OR_NULL(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);
	return ret;
}

#ifdef CONFIG_PM
static int funnel_runtime_suspend(struct device *dev)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);

	return 0;
}

static int funnel_runtime_resume(struct device *dev)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_prepare_enable(drvdata->atclk);

	return 0;
}
#endif

static const struct dev_pm_ops funnel_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(funnel_runtime_suspend, funnel_runtime_resume, NULL)
};

static int static_funnel_probe(struct platform_device *pdev)
{
	int ret;

	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	/* Static funnel do not have programming base */
	ret = funnel_probe(&pdev->dev, NULL);

	if (ret) {
		pm_runtime_put_noidle(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
	}

	return ret;
}

static const struct of_device_id static_funnel_match[] = {
	{.compatible = "arm,coresight-static-funnel"},
	{}
};

static struct platform_driver static_funnel_driver = {
	.probe          = static_funnel_probe,
	.driver         = {
		.name   = "coresight-static-funnel",
		.of_match_table = static_funnel_match,
		.pm	= &funnel_dev_pm_ops,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver(static_funnel_driver);

static int dynamic_funnel_probe(struct amba_device *adev,
				const struct amba_id *id)
{
	return funnel_probe(&adev->dev, &adev->res);
}

static const struct amba_id dynamic_funnel_ids[] = {
	{
		.id     = 0x000bb908,
		.mask   = 0x000fffff,
	},
	{
		/* Coresight SoC-600 */
		.id     = 0x000bb9eb,
		.mask   = 0x000fffff,
	},
	{ 0, 0},
};

static struct amba_driver dynamic_funnel_driver = {
	.drv = {
		.name	= "coresight-dynamic-funnel",
		.owner	= THIS_MODULE,
		.pm	= &funnel_dev_pm_ops,
		.suppress_bind_attrs = true,
	},
	.probe		= dynamic_funnel_probe,
	.id_table	= dynamic_funnel_ids,
};
builtin_amba_driver(dynamic_funnel_driver);
