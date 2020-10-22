/*
 * Platform Dependent file for Exynos
 *
 * Copyright (C) 2020, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 */
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <wlan_plat.h>
#if defined(CONFIG_64BIT)
#include <asm-generic/gpio.h>
#else
#include <mach/gpio.h>
#endif /* CONFIG_64BIT */
#if defined(CONFIG_SEC_SYSFS)
#include <linux/sec_sysfs.h>
#elif defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_SYSFS */

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
extern int dhd_init_wlan_mem(void);
extern void dhd_exit_wlan_mem(void);
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#define WIFI_TURNON_DELAY	200
static int wlan_pwr_on = -1;
static int wlan_host_wake_irq = 0;
EXPORT_SYMBOL(wlan_host_wake_irq);

#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_MACH_UNIVERSAL7420)
#define EXYNOS_PCIE_CH_NUM 1
#elif defined(CONFIG_SOC_EXYNOS8890)
#define EXYNOS_PCIE_CH_NUM 0
#else
#error "PCIE_CH_NUM should be defined"
#endif // endif

extern void exynos_pcie_poweron(int);
extern void exynos_pcie_poweroff(int);

static int
dhd_wlan_power(int onoff)
{
	printk(KERN_INFO"------------------------------------------------\n");
	printk(KERN_INFO"%s Enter: power %s\n", __FUNCTION__, onoff ? "on" : "off");

	if (!onoff) {
		exynos_pcie_poweroff(EXYNOS_PCIE_CH_NUM);
	}
	if (gpio_direction_output(wlan_pwr_on, onoff)) {
		printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
			__FUNCTION__, onoff ? "HIGH" : "LOW");
		return -EIO;
	}
	if (onoff) {
		exynos_pcie_poweron(EXYNOS_PCIE_CH_NUM);
	}

	return 0;
}

static int
dhd_wlan_reset(int onoff)
{
	return 0;
}

static int
dhd_wlan_set_carddetect(int val)
{
	return 0;
}

int __init
dhd_wlan_init_gpio(void)
{
	const char *wlan_node = "samsung,brcm-wlan";
	unsigned int wlan_host_wake_up = -1;
	struct device_node *root_node = NULL;

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of broadcom wifi\n");
		return -ENODEV;
	}

	/* ========== WLAN_PWR_EN ============ */
	wlan_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(wlan_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_pwr_on);
		return -ENODEV;
	}

	if (gpio_request(wlan_pwr_on, "WLAN_REG_ON")) {
		WARN(1, "fail to request gpio(WLAN_REG_ON)\n");
		return -ENODEV;
	}
	gpio_direction_output(wlan_pwr_on, 1);

	gpio_export(wlan_pwr_on, 1);
	msleep(WIFI_TURNON_DELAY);
	exynos_pcie_poweron(EXYNOS_PCIE_CH_NUM);

	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_gpio(root_node, 1);
	if (!gpio_is_valid(wlan_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_host_wake_up);
		return -ENODEV;
	}

	if (gpio_request(wlan_host_wake_up, "WLAN_HOST_WAKE")) {
		WARN(1, "fail to request gpio(WLAN_HOST_WAKE)\n");
		return -ENODEV;
	}
	gpio_direction_input(wlan_host_wake_up);
	gpio_export(wlan_host_wake_up, 1);

	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);

	return 0;
}

int __exit
dhd_wlan_exit_gpio(void)
{
	const char *wlan_node = "exynos,brcm-wlan";
	unsigned int wlan_host_wake_up = -1;
	struct device_node *root_node = NULL;

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of broadcom wifi\n");
		return -ENODEV;
	}

	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_gpio(root_node, 1);
	if (!gpio_is_valid(wlan_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_host_wake_up);
		return -ENODEV;
	}

	gpio_direction_input(wlan_host_wake_up);
	gpio_export(wlan_host_wake_up, 0);

	gpio_free(wlan_host_wake_up);

	/* ========== WLAN_PWR_EN ============ */
	wlan_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(wlan_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_pwr_on);
		return -ENODEV;
	}

	gpio_direction_output(wlan_pwr_on, 1);
	gpio_export(wlan_pwr_on, 0);

	gpio_free(wlan_pwr_on);

	return 0;
}

struct resource dhd_wlan_resources = {
	.name	= "bcmdhd_wlan_irq",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE | IORESOURCE_IRQ_HIGHEDGE,
};
EXPORT_SYMBOL(dhd_wlan_resources);

struct wifi_platform_data dhd_wlan_control = {
	.set_power	= dhd_wlan_power,
	.set_reset	= dhd_wlan_reset,
	.set_carddetect	= dhd_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= dhd_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
};
EXPORT_SYMBOL(dhd_wlan_control);

int __init
dhd_wlan_init(void)
{
	int ret;

	printk(KERN_INFO "%s: START.......\n", __FUNCTION__);
	ret = dhd_wlan_init_gpio();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initiate GPIO, ret=%d\n",
			__FUNCTION__, ret);
		goto fail;
	}

	dhd_wlan_resources.start = wlan_host_wake_irq;
	dhd_wlan_resources.end = wlan_host_wake_irq;

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	ret = dhd_init_wlan_mem();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to alloc reserved memory,"
			" ret=%d\n", __FUNCTION__, ret);
	}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

fail:
	return ret;
}

static void __exit
dhd_wlan_exit(void)
{
	int ret;

	printk(KERN_INFO "%s: EXIT.......\n", __FUNCTION__);
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	dhd_exit_wlan_mem();
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	ret = dhd_wlan_exit_gpio();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initiate GPIO, ret=%d\n",
				__FUNCTION__, ret);
	}
}

device_initcall(dhd_wlan_init);
module_exit(dhd_wlan_exit);

MODULE_LICENSE("GPL and additional rights");
