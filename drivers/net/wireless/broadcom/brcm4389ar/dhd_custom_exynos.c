/*
 * Platform Dependent file for Samsung Exynos
 *
 * Copyright (C) 1999-2019, Broadcom.
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
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: dhd_custom_exynos.c 797239 2018-12-31 00:59:21Z $
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
#include <linux/wlan_plat.h>
#if defined(CONFIG_SOC_EXYNOS8895) || defined(CONFIG_SOC_EXYNOS9810) ||        \
	defined(CONFIG_SOC_EXYNOS9820) || defined(CONFIG_SOC_S5E9925)
#include <linux/exynos-pci-ctrl.h>
#endif /* CONFIG_SOC_EXYNOS8895 || CONFIG_SOC_EXYNOS9810 || CONFIG_SOC_EXYNOS9820 */

#if defined(CONFIG_64BIT)
#include <asm-generic/gpio.h>
#else
#if !defined(CONFIG_ARCH_SWA100) && !defined(CONFIG_MACH_UNIVERSAL7580)
#include <mach/gpio.h>
#endif /* !CONFIG_ARCH_SWA100 && !CONFIG_MACH_UNIVERSAL7580 */
#endif /* CONFIG_64BIT */

#if defined(CONFIG_MACH_UNIVERSAL7580) ||                                      \
	defined(CONFIG_MACH_UNIVERSAL5430) ||                                  \
	defined(CONFIG_MACH_UNIVERSAL5422)
#include <mach/irqs.h>
#endif /* CONFIG_MACH_UNIVERSAL7580 || CONFIG_MACH_UNIVERSAL5430 || CONFIG_MACH_UNIVERSAL5422 */

#if !defined(CONFIG_SOC_S5E9925)
#include <linux/sec_sysfs.h>
#endif

#ifdef CONFIG_MACH_A7LTE
#define PINCTL_DELAY 150
#endif /* CONFIG_MACH_A7LTE */
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
extern int dhd_init_wlan_mem(void);
extern void dhd_exit_wlan_mem(void);
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#define WIFI_LOAD_SWITCH_DELAY 100
#define WIFI_TURNON_DELAY 200
static int wlan_reg_en = -1;
static int wlan_load_switch_en = -1;
static int wlan_pwr_on = -1;

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
static int wlan_host_wake_irq = 0;
//EXPORT_SYMBOL(wlan_host_wake_irq);
static unsigned int wlan_host_wake_up = -1;
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */
#ifdef CONFIG_MACH_A7LTE
extern struct device *mmc_dev_for_wlan;
#endif /* CONFIG_MACH_A7LTE */

#ifdef CONFIG_BCMDHD_PCIE
#define EXYNOS_PCIE_RC_ONOFF
#endif /* CONFIG_BCMDHD_PCIE */

#ifdef EXYNOS_PCIE_RC_ONOFF
#ifdef CONFIG_MACH_UNIVERSAL5433
#define SAMSUNG_PCIE_CH_NUM
#elif defined(CONFIG_MACH_UNIVERSAL7420) || defined(CONFIG_MACH_EXSOM7420)
#define SAMSUNG_PCIE_CH_NUM 1
#elif defined(CONFIG_SOC_EXYNOS8890) || defined(CONFIG_SOC_EXYNOS8895) ||      \
	defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS9820) ||    \
	defined(CONFIG_SOC_S5E9925)
#define SAMSUNG_PCIE_CH_NUM 0
#endif // endif

int pcie_ch_num = SAMSUNG_PCIE_CH_NUM;

#ifdef CONFIG_MACH_UNIVERSAL5433
extern void exynos_pcie_pm_resume(void);
extern void exynos_pcie_pm_suspend(void);
#else
extern void exynos_pcie_pm_resume(int);
extern void exynos_pcie_pm_suspend(int);
#endif /* CONFIG_MACH_UNIVERSAL5433 */
#endif /* EXYNOS_PCIE_RC_ONOFF */

#if (defined(CONFIG_MACH_UNIVERSAL3475) || defined(CONFIG_SOC_EXYNOS7870) ||   \
     defined(CONFIG_MACH_UNIVERSAL7580))
extern struct mmc_host *wlan_mmc;
extern void mmc_ctrl_power(struct mmc_host *host, bool onoff);
#endif /* MACH_UNIVERSAL3475 || SOC_EXYNOS7870 || MACH_UNIVERSAL7580 */

MODULE_LICENSE("GPL v2");

static int dhd_wlan_power(int onoff)
{
#ifdef CONFIG_MACH_A7LTE
	struct pinctrl *pinctrl = NULL;
#endif /* CONFIG_MACH_A7LTE */

	printk(KERN_INFO "------------------------------------------------");
	printk(KERN_INFO "------------------------------------------------\n");
	printk(KERN_INFO "%s Enter: power %s\n", __FUNCTION__,
	       onoff ? "on" : "off");

#ifdef EXYNOS_PCIE_RC_ONOFF
	if (!onoff) {
		exynos_pcie_pm_suspend(SAMSUNG_PCIE_CH_NUM);
	}

	if (gpio_direction_output(wlan_pwr_on, onoff)) {
		printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
		       __FUNCTION__, onoff ? "HIGH" : "LOW");
		return -EIO;
	}

	if (onoff) {
#if defined(CONFIG_SOC_EXYNOS8895) || defined(CONFIG_SOC_EXYNOS9810) ||        \
	defined(CONFIG_SOC_EXYNOS9820)
		printk(KERN_ERR "%s Disable L1ss EP side\n", __FUNCTION__);
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_WIFI);
#endif /* CONFIG_SOC_EXYNOS8895 || CONFIG_SOC_EXYNOS9810 || CONFIG_SOC_EXYNOS9820 */
		exynos_pcie_pm_resume(SAMSUNG_PCIE_CH_NUM);
	}
#else
#ifdef CONFIG_MACH_A7LTE
	if (onoff) {
		pinctrl = devm_pinctrl_get_select(mmc_dev_for_wlan,
						  "sdio_wifi_on");
		if (IS_ERR(pinctrl))
			printk(KERN_INFO "%s WLAN SDIO GPIO control error\n",
			       __FUNCTION__);
		msleep(PINCTL_DELAY);
	}
#endif /* CONFIG_MACH_A7LTE */

	if (gpio_direction_output(wlan_pwr_on, onoff)) {
		printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
		       __FUNCTION__, onoff ? "HIGH" : "LOW");
		return -EIO;
	}

#ifdef CONFIG_MACH_A7LTE
	if (!onoff) {
		pinctrl = devm_pinctrl_get_select(mmc_dev_for_wlan,
						  "sdio_wifi_off");
		if (IS_ERR(pinctrl))
			printk(KERN_INFO "%s WLAN SDIO GPIO control error\n",
			       __FUNCTION__);
	}
#endif /* CONFIG_MACH_A7LTE */
#if (defined(CONFIG_MACH_UNIVERSAL3475) || defined(CONFIG_SOC_EXYNOS7870) ||   \
     defined(CONFIG_MACH_UNIVERSAL7580))
	if (wlan_mmc)
		mmc_ctrl_power(wlan_mmc, onoff);
#endif /* MACH_UNIVERSAL3475 || SOC_EXYNOS7870 || MACH_UNIVERSAL7580 */
#endif /* EXYNOS_PCIE_RC_ONOFF */
	return 0;
}

static int dhd_wlan_reset(int onoff)
{
	return 0;
}

#ifndef CONFIG_BCMDHD_PCIE
extern void (*notify_func_callback)(void *dev_id, int state);
extern void *mmc_host_dev;

static int dhd_wlan_set_carddetect(int val)
{
	pr_err("%s: notify_func=%p, mmc_host_dev=%p, val=%d\n", __FUNCTION__,
	       notify_func_callback, mmc_host_dev, val);

	if (notify_func_callback) {
		notify_func_callback(mmc_host_dev, val);
	} else {
		pr_warning("%s: Nobody to notify\n", __FUNCTION__);
	}

	return 0;
}
#endif /* !CONFIG_BCMDHD_PCIE */

void dhd_plat_l1ss_ctrl(bool ctrl)
{
	exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_WIFI, SAMSUNG_PCIE_CH_NUM);
}

#ifdef BCMDHD_MODULAR
int
#else
int __init
#endif /* BCMDHD_MODULAR */
dhd_wlan_init_gpio(void)
{
	const char *wlan_node = "samsung,brcm-wlan";
	const char *wlan_regulator_node = "wlan-regulator-gpio";
	struct device_node *chip_node = NULL;
	struct device_node *regulator_node = NULL;

	// This node is for the chip internal regulator for wifi
	// subsystem
	chip_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!chip_node) {
		WARN(1, "failed to get device node of bcm4389\n");
		return -ENODEV;
	}

	// This node is for the chip external regulator and load switch
	regulator_node =
		of_find_compatible_node(NULL, NULL, wlan_regulator_node);
	if (!regulator_node) {
		WARN(1, "failed to get regulator device node of bcm4389\n");
		return -ENODEV;
	}

	/* ========== AP2WIFI_REG_EN ============ */
	wlan_reg_en = of_get_gpio(regulator_node, 0);
	if (!gpio_is_valid(wlan_reg_en)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_reg_en);
		return -ENODEV;
	}

	if (gpio_request(wlan_reg_en, "AP2WIFI_REG_EN")) {
		WARN(1, "fail to request gpio(AP2WIFI_REG_EN)\n");
		return -ENODEV;
	}

	/* ========== AP2WIFI_LS_EN ============ */
	wlan_load_switch_en = of_get_gpio(regulator_node, 1);
	if (!gpio_is_valid(wlan_load_switch_en)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_load_switch_en);
		return -ENODEV;
	}

	if (gpio_request(wlan_load_switch_en, "AP2WIFI_LS_EN")) {
		WARN(1, "fail to request gpio(AP2WIFI_LS_EN)\n");
		return -ENODEV;
	}

	/* ========== WLAN_PWR_EN ============ */
	wlan_pwr_on = of_get_gpio(chip_node, 0);
	if (!gpio_is_valid(wlan_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_pwr_on);
		return -ENODEV;
	}

	if (gpio_request(wlan_pwr_on, "WLAN_REG_ON")) {
		WARN(1, "fail to request gpio(WLAN_REG_ON)\n");
		return -ENODEV;
	}
#ifdef CONFIG_BCMDHD_PCIE
	gpio_direction_output(wlan_reg_en, 1);
	msleep(WIFI_LOAD_SWITCH_DELAY);
	gpio_direction_output(wlan_load_switch_en, 1);

	gpio_direction_output(wlan_pwr_on, 1);
	msleep(WIFI_TURNON_DELAY);
#else
	gpio_direction_output(wlan_reg_en, 0);
	gpio_direction_output(wlan_load_switch_en, 0);
	gpio_direction_output(wlan_pwr_on, 0);
#endif /* CONFIG_BCMDHD_PCIE */

#ifdef EXYNOS_PCIE_RC_ONOFF
	exynos_pcie_pm_resume(SAMSUNG_PCIE_CH_NUM);
#endif /* EXYNOS_PCIE_RC_ONOFF */

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_gpio(chip_node, 1);
	if (!gpio_is_valid(wlan_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_host_wake_up);
		return -ENODEV;
	}

	if (gpio_request(wlan_host_wake_up, "WLAN_HOST_WAKE")) {
		WARN(1, "fail to request gpio(WLAN_HOST_WAKE)\n");
		return -ENODEV;
	}
	gpio_direction_input(wlan_host_wake_up);
	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */

	return 0;
}

#ifdef BCMDHD_MODULAR
int
#else
int __deinit
#endif /* BCMDHD_MODULAR */
dhd_wlan_deinit_gpio(void)
{
#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	gpio_free(wlan_host_wake_up);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */
	gpio_free(wlan_pwr_on);
	gpio_free(wlan_load_switch_en);
	gpio_free(wlan_reg_en);
	return 0;
}

void interrupt_set_cpucore(int set, unsigned int dpc_cpucore,
			   unsigned int primary_cpucore)
{
	printk(KERN_INFO "%s: set: %d\n", __FUNCTION__, set);
	if (set) {
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1, cpumask_of(dpc_cpucore));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31, cpumask_of(dpc_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5422 */
#if defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(dpc_cpucore));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(dpc_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5430 */
#if defined(CONFIG_MACH_UNIVERSAL7580)
		irq_set_affinity(IRQ_SPI(246), cpumask_of(dpc_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL7580 */
	} else {
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1,
				 cpumask_of(primary_cpucore));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31,
				 cpumask_of(primary_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5422 */
#if defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(primary_cpucore));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(primary_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL5430 */
#if defined(CONFIG_MACH_UNIVERSAL7580)
		irq_set_affinity(IRQ_SPI(246), cpumask_of(primary_cpucore));
#endif /* CONFIG_MACH_UNIVERSAL7580 */
	}
}

#if defined(CONFIG_BCMDHD_OOB_HOST_WAKE) && defined(CONFIG_BCMDHD_GET_OOB_STATE)
int dhd_get_wlan_oob_gpio(void)
{
	return gpio_is_valid(wlan_host_wake_up) ?
		       gpio_get_value(wlan_host_wake_up) :
		       -1;
}
EXPORT_SYMBOL(dhd_get_wlan_oob_gpio);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE && CONFIG_BCMDHD_GET_OOB_STATE */

struct resource dhd_wlan_resources = {
	.name = "bcmdhd_wlan_irq",
	.start = 0,
	.end = 0,
	.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE |
#ifdef CONFIG_BCMDHD_PCIE
		 IORESOURCE_IRQ_HIGHEDGE,
#else
		 IORESOURCE_IRQ_HIGHLEVEL,
#endif /* CONFIG_BCMDHD_PCIE */
};
EXPORT_SYMBOL(dhd_wlan_resources);

struct wifi_platform_data dhd_wlan_control = {
	.set_power = dhd_wlan_power,
	.set_reset = dhd_wlan_reset,
#ifndef CONFIG_BCMDHD_PCIE
	.set_carddetect = dhd_wlan_set_carddetect,
#endif /* !CONFIG_BCMDHD_PCIE */
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc = dhd_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
};
EXPORT_SYMBOL(dhd_wlan_control);

int __init dhd_wlan_init(void)
{
	int ret;

	printk(KERN_INFO "%s: START.......\n", __FUNCTION__);
	ret = dhd_wlan_init_gpio();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initiate GPIO, ret=%d\n",
		       __FUNCTION__, ret);
		goto fail;
	}

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	dhd_wlan_resources.start = wlan_host_wake_irq;
	dhd_wlan_resources.end = wlan_host_wake_irq;
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	ret = dhd_init_wlan_mem();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to alloc reserved memory,"
				" ret=%d\n",
		       __FUNCTION__, ret);
	}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

fail:
	return ret;
}

int dhd_wlan_deinit(void)
{
	dhd_wlan_deinit_gpio();
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	dhd_exit_wlan_mem();
#endif /*  CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	return 0;
}

#ifndef BCMDHD_MODULAR
#if defined(CONFIG_MACH_UNIVERSAL7420) || defined(CONFIG_SOC_EXYNOS8890) ||    \
	defined(CONFIG_SOC_EXYNOS8895) || defined(CONFIG_SOC_EXYNOS9810) ||    \
	defined(CONFIG_SOC_EXYNOS9820) || defined(CONFIG_SOC_S5E9925)
#if defined(CONFIG_DEFERRED_INITCALLS)
deferred_module_init(dhd_wlan_init);
#else
late_initcall(dhd_wlan_init);
#endif /* CONFIG_DEFERRED_INITCALLS */
#else
device_initcall(dhd_wlan_init);
#endif /* CONFIG Exynos PCIE Platforms */
#endif /* BCMDHD_MODULAR */
