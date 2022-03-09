/*
 * xhci-plat.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/slab.h>
#include <linux/acpi.h>

#include "xhci.h"
#include "xhci-plat.h"
#include "xhci-mvebu.h"
#include "xhci-rcar.h"

static struct hc_driver __read_mostly xhci_plat_hc_driver;

static int xhci_plat_setup(struct usb_hcd *hcd);
static int xhci_plat_start(struct usb_hcd *hcd);

static const struct xhci_driver_overrides xhci_plat_overrides __initconst = {
	.extra_priv_size = sizeof(struct xhci_plat_priv),
	.reset = xhci_plat_setup,
	.start = xhci_plat_start,
};

static void xhci_priv_plat_start(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (priv->plat_start)
		priv->plat_start(hcd);
}

static int xhci_priv_init_quirk(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->init_quirk)
		return 0;

	return priv->init_quirk(hcd);
}

static void xhci_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;
}

/* called during probe() after chip reset completes */
static int xhci_plat_setup(struct usb_hcd *hcd)
{
	int ret;


	ret = xhci_priv_init_quirk(hcd);
	if (ret)
		return ret;

	return xhci_gen_setup(hcd, xhci_plat_quirks);
}

static int xhci_plat_start(struct usb_hcd *hcd)
{
	xhci_priv_plat_start(hcd);
	return xhci_run(hcd);
}

#ifdef CONFIG_OF
static const struct xhci_plat_priv xhci_plat_marvell_armada = {
	.init_quirk = xhci_mvebu_mbus_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen2 = {
	.firmware_name = XHCI_RCAR_FIRMWARE_NAME_V1,
	.init_quirk = xhci_rcar_init_quirk,
	.plat_start = xhci_rcar_start,
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen3 = {
	.firmware_name = XHCI_RCAR_FIRMWARE_NAME_V2,
	.init_quirk = xhci_rcar_init_quirk,
	.plat_start = xhci_rcar_start,
};

static const struct of_device_id usb_xhci_of_match[] = {
	{
		.compatible = "generic-xhci",
	}, {
		.compatible = "xhci-platform",
	}, {
		.compatible = "marvell,armada-375-xhci",
		.data = &xhci_plat_marvell_armada,
	}, {
		.compatible = "marvell,armada-380-xhci",
		.data = &xhci_plat_marvell_armada,
	}, {
		.compatible = "renesas,xhci-r8a7790",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7791",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7793",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7795",
		.data = &xhci_plat_renesas_rcar_gen3,
	}, {
		.compatible = "renesas,rcar-gen2-xhci",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,rcar-gen3-xhci",
		.data = &xhci_plat_renesas_rcar_gen3,
	},
	{},
};
MODULE_DEVICE_TABLE(of, usb_xhci_of_match);
#endif

static ssize_t config_imod_store(struct device *pdev,
		struct device_attribute *attr, const char *buff, size_t size)
{
	struct usb_hcd *hcd = dev_get_drvdata(pdev);
	struct xhci_hcd *xhci;
	u32 temp;
	u32 imod;
	unsigned long flags;

	if (kstrtouint(buff, 10, &imod) != 1)
		return 0;

	imod &= ER_IRQ_INTERVAL_MASK;
	xhci = hcd_to_xhci(hcd);

	if (xhci->shared_hcd->state == HC_STATE_SUSPENDED
		&& hcd->state == HC_STATE_SUSPENDED)
		return -EACCES;

	spin_lock_irqsave(&xhci->lock, flags);
	temp = readl_relaxed(&xhci->ir_set->irq_control);
	temp &= ~ER_IRQ_INTERVAL_MASK;
	temp |= imod;
	writel_relaxed(temp, &xhci->ir_set->irq_control);
	spin_unlock_irqrestore(&xhci->lock, flags);

	return size;
}

static ssize_t config_imod_show(struct device *pdev,
		struct device_attribute *attr, char *buff)
{
	struct usb_hcd *hcd = dev_get_drvdata(pdev);
	struct xhci_hcd *xhci;
	u32 temp;
	unsigned long flags;

	xhci = hcd_to_xhci(hcd);

	if (xhci->shared_hcd->state == HC_STATE_SUSPENDED
		&& hcd->state == HC_STATE_SUSPENDED)
		return -EACCES;

	spin_lock_irqsave(&xhci->lock, flags);
	temp = readl_relaxed(&xhci->ir_set->irq_control) &
			ER_IRQ_INTERVAL_MASK;
	spin_unlock_irqrestore(&xhci->lock, flags);

	return snprintf(buff, PAGE_SIZE, "%08u\n", temp);
}

static DEVICE_ATTR(config_imod, 0644, config_imod_show, config_imod_store);

static int xhci_plat_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct hc_driver	*driver;
	struct device		*sysdev, *phydev;
	struct xhci_hcd		*xhci;
	struct resource         *res;
	struct usb_hcd		*hcd;
	struct clk              *clk;
	int			ret;
	int			irq;
	u32			temp, imod;
	unsigned long		flags;

	if (usb_disabled())
		return -ENODEV;

	driver = &xhci_plat_hc_driver;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/*
	 * sysdev must point to a device that is known to the system firmware
	 * or PCI hardware. We handle these three cases here:
	 * 1. xhci_plat comes from firmware
	 * 2. xhci_plat is child of a device from firmware (dwc3-plat)
	 * 3. xhci_plat is grandchild of a pci device (dwc3-pci)
	 */
	sysdev = &pdev->dev;
	phydev = &pdev->dev;
	if (sysdev->parent && !sysdev->of_node && sysdev->parent->of_node)
		phydev = sysdev->parent;
	/*
	 * If sysdev->parent->parent is available and part of IOMMU group
	 * (indicating possible usage of SMMU enablement), then use
	 * sysdev->parent->parent as sysdev.
	 */
	if (sysdev->parent && !sysdev->of_node && sysdev->parent->of_node &&
		sysdev->parent->parent && sysdev->parent->parent->iommu_group)
		sysdev = sysdev->parent->parent;
	else if (sysdev->parent && !sysdev->of_node && sysdev->parent->of_node)
		sysdev = sysdev->parent;
#ifdef CONFIG_PCI
	else if (sysdev->parent && sysdev->parent->parent &&
		 sysdev->parent->parent->bus == &pci_bus_type)
		sysdev = sysdev->parent->parent;
#endif

	/* Try to set 64-bit DMA first */
	if (WARN_ON(!sysdev->dma_mask))
		/* Platform did not initialize dma_mask */
		ret = dma_coerce_mask_and_coherent(sysdev,
						   DMA_BIT_MASK(64));
	else
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(64));

	/* If seting 64-bit DMA mask fails, fall back to 32-bit DMA mask */
	if (ret) {
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(32));
		if (ret)
			return ret;
	}

	hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			       dev_name(&pdev->dev), NULL);
	if (!hcd)
		return -ENOMEM;

	hcd_to_bus(hcd)->skip_resume = true;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hcd->regs)) {
		ret = PTR_ERR(hcd->regs);
		goto put_hcd;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	/*
	 * Not all platforms have a clk so it is not an error if the
	 * clock does not exists.
	 */
	clk = devm_clk_get(&pdev->dev, NULL);
	if (!IS_ERR(clk)) {
		ret = clk_prepare_enable(clk);
		if (ret)
			goto put_hcd;
	} else if (PTR_ERR(clk) == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto put_hcd;
	}

	if (pdev->dev.parent)
		pm_runtime_resume(pdev->dev.parent);

	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	xhci = hcd_to_xhci(hcd);
	match = of_match_node(usb_xhci_of_match, pdev->dev.of_node);
	if (match) {
		const struct xhci_plat_priv *priv_match = match->data;
		struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

		/* Just copy data for now */
		if (priv_match)
			*priv = *priv_match;
	}

	device_wakeup_enable(hcd->self.controller);

	xhci->clk = clk;
	xhci->main_hcd = hcd;
	xhci->shared_hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto disable_clk;
	}

	hcd_to_bus(xhci->shared_hcd)->skip_resume = true;

	if (device_property_read_bool(&pdev->dev, "usb3-lpm-capable"))
		xhci->quirks |= XHCI_LPM_SUPPORT;

	if (device_property_read_bool(&pdev->dev, "quirk-broken-port-ped"))
		xhci->quirks |= XHCI_BROKEN_PORT_PED;

	if (device_property_read_u32(&pdev->dev, "xhci-imod-value", &imod))
		imod = 0;

	if (device_property_read_u32(&pdev->dev, "usb-core-id", &xhci->core_id))
		xhci->core_id = -EINVAL;

	hcd->usb_phy = devm_usb_get_phy_by_phandle(phydev, "usb-phy", 0);
	if (IS_ERR(hcd->usb_phy)) {
		ret = PTR_ERR(hcd->usb_phy);
		if (ret == -EPROBE_DEFER)
			goto put_usb3_hcd;
		hcd->usb_phy = NULL;
	} else {
		ret = usb_phy_init(hcd->usb_phy);
		if (ret)
			goto put_usb3_hcd;
	}

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto disable_usb_phy;

	device_wakeup_enable(&hcd->self.root_hub->dev);

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto dealloc_usb2_hcd;

	device_wakeup_enable(&xhci->shared_hcd->self.root_hub->dev);

	/* override imod interval if specified */
	if (imod) {
		imod &= ER_IRQ_INTERVAL_MASK;
		spin_lock_irqsave(&xhci->lock, flags);
		temp = readl_relaxed(&xhci->ir_set->irq_control);
		temp &= ~ER_IRQ_INTERVAL_MASK;
		temp |= imod;
		writel_relaxed(temp, &xhci->ir_set->irq_control);
		spin_unlock_irqrestore(&xhci->lock, flags);
		dev_dbg(&pdev->dev, "%s: imod set to %u\n", __func__, imod);
	}

	ret = device_create_file(&pdev->dev, &dev_attr_config_imod);
	if (ret)
		dev_err(&pdev->dev, "%s: unable to create imod sysfs entry\n",
					__func__);

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	return 0;


dealloc_usb2_hcd:
	usb_remove_hcd(hcd);

disable_usb_phy:
	usb_phy_shutdown(hcd->usb_phy);

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

disable_clk:
	if (!IS_ERR(clk))
		clk_disable_unprepare(clk);

put_hcd:
	usb_put_hcd(hcd);

	return ret;
}

static int xhci_plat_remove(struct platform_device *dev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct clk *clk = xhci->clk;

	pm_runtime_disable(&dev->dev);
	xhci->xhc_state |= XHCI_STATE_REMOVING;

	device_remove_file(&dev->dev, &dev_attr_config_imod);
	usb_remove_hcd(xhci->shared_hcd);
	usb_phy_shutdown(hcd->usb_phy);

	usb_remove_hcd(hcd);
	usb_put_hcd(xhci->shared_hcd);

	if (!IS_ERR(clk))
		clk_disable_unprepare(clk);
	usb_put_hcd(hcd);

	return 0;
}

#ifdef CONFIG_PM
static int xhci_plat_runtime_idle(struct device *dev)
{
	/*
	 * When pm_runtime_put_autosuspend() is called on this device,
	 * after this idle callback returns the PM core will schedule the
	 * autosuspend if there is any remaining time until expiry. However,
	 * when reaching this point because the child_count becomes 0, the
	 * core does not honor autosuspend in that case and results in
	 * idle/suspend happening immediately. In order to have a delay
	 * before suspend we have to call pm_runtime_autosuspend() manually.
	 */
	pm_runtime_mark_last_busy(dev);
	pm_runtime_autosuspend(dev);
	return -EBUSY;
}

static int xhci_plat_pm_freeze(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	if (!xhci)
		return 0;

	dev_dbg(dev, "xhci-plat freeze\n");

	return xhci_suspend(xhci, false);
}

static int xhci_plat_pm_restore(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int ret;

	if (!xhci)
		return 0;

	dev_dbg(dev, "xhci-plat restore\n");

	ret = xhci_resume(xhci, true);
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_mark_last_busy(dev);

	return ret;
}

static int xhci_plat_runtime_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	if (!xhci)
		return 0;

	dev_dbg(dev, "xhci-plat runtime suspend\n");

	return xhci_suspend(xhci, true);
}

static int xhci_plat_runtime_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int ret;

	if (!xhci)
		return 0;

	dev_dbg(dev, "xhci-plat runtime resume\n");

	ret = xhci_resume(xhci, false);
	pm_runtime_mark_last_busy(dev);

	return ret;
}

static const struct dev_pm_ops xhci_plat_pm_ops = {
	.freeze		= xhci_plat_pm_freeze,
	.restore	= xhci_plat_pm_restore,
	.thaw		= xhci_plat_pm_restore,
	SET_RUNTIME_PM_OPS(xhci_plat_runtime_suspend, xhci_plat_runtime_resume,
			   xhci_plat_runtime_idle)
};
#define DEV_PM_OPS	(&xhci_plat_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM */

static const struct acpi_device_id usb_xhci_acpi_match[] = {
	/* XHCI-compliant USB Controller */
	{ "PNP0D10", },
	{ }
};
MODULE_DEVICE_TABLE(acpi, usb_xhci_acpi_match);

static struct platform_driver usb_xhci_driver = {
	.probe	= xhci_plat_probe,
	.remove	= xhci_plat_remove,
	.driver	= {
		.name = "xhci-hcd",
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(usb_xhci_of_match),
		.acpi_match_table = ACPI_PTR(usb_xhci_acpi_match),
	},
};
MODULE_ALIAS("platform:xhci-hcd");

static int __init xhci_plat_init(void)
{
	xhci_init_driver(&xhci_plat_hc_driver, &xhci_plat_overrides);
	return platform_driver_register(&usb_xhci_driver);
}
module_init(xhci_plat_init);

static void __exit xhci_plat_exit(void)
{
	platform_driver_unregister(&usb_xhci_driver);
}
module_exit(xhci_plat_exit);

MODULE_DESCRIPTION("xHCI Platform Host Controller Driver");
MODULE_LICENSE("GPL");
