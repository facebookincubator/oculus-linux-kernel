/*
 * Platform Dependent file for Qualcomm MSM/APQ
 *
 * Copyright (C) 2022, Broadcom.
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
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/skbuff.h>
#include <linux/mmc/host.h>
#ifdef CONFIG_BCMDHD_PCIE
#include <linux/msm_pcie.h>
#endif /* CONFIG_BCMDHD_PCIE */
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/of_gpio.h>
#include <dhd_plat.h>
#include <bcmdevs.h>

#if IS_ENABLED(CONFIG_PCI_MSM)
#include <linux/msm_pcie.h>
#else
#include <mach/msm_pcie.h>
#endif /* CONFIG_PCI_MSM */

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
extern void dhd_exit_wlan_mem(void);
extern int dhd_init_wlan_mem(void);
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#define MSM_PCIE_VENDOR_ID 0x17cb
#if defined(CONFIG_ARCH_APQ8084)
#define MSM_PCIE_DEVICE_ID 0x0101
#elif defined(CONFIG_ARCH_MSM8994)
#define MSM_PCIE_DEVICE_ID 0x0300
#elif defined(CONFIG_ARCH_MSM8996)
#define MSM_PCIE_DEVICE_ID 0x0104
#elif defined(CONFIG_ARCH_MSM8998)
#define MSM_PCIE_DEVICE_ID 0x0105
#elif defined(CONFIG_ARCH_SDM845) || defined(CONFIG_ARCH_SM8150) || \
	defined(CONFIG_ARCH_KONA) || defined(CONFIG_ARCH_LAHAINA) || \
	defined(CONFIG_ARCH_WAIPIO)
#define MSM_PCIE_DEVICE_ID 0x0106
#else
#error "Not supported platform"
#endif

#define WIFI_TURNON_DELAY_MIN_US	190000
#define WIFI_TURNON_DELAY_MAX_US	200000
#define WIFI_TURNOFF_DELAY_MIN_US	4000
#define WIFI_TURNOFF_DELAY_MAX_US	5000
#define WIFI_LOAD_SWITCH_DELAY_MIN_US      190000
#define WIFI_LOAD_SWITCH_DELAY_MAX_US      200000

#define WIFI_TURNON_DELAY       200
static int wlan_reg_on = -1;
static int wlan_pwr_on = -1;
#define DHD_DT_COMPAT_ENTRY		"android,bcmdhd_wlan"
#ifdef CUSTOMER_HW2
#define WIFI_WL_REG_ON_PROPNAME		"wl_reg_on"
#else
#define WIFI_WL_REG_ON_PROPNAME		"wlan-en-gpio"
#endif /* CUSTOMER_HW2 */
#define WIFI_WL_PWR_EN_PROPNAME		"wlan-pwr-en-gpio"

#if defined(CONFIG_ARCH_MSM8996) || defined(CONFIG_ARCH_MSM8998) || \
	defined(CONFIG_ARCH_SDM845) || defined(CONFIG_ARCH_SM8150) || defined(CONFIG_ARCH_KONA) \
	|| defined(CONFIG_ARCH_LAHAINA) || defined(CONFIG_ARCH_WAIPIO)
#define MSM_PCIE_CH_NUM			0
#else
#define MSM_PCIE_CH_NUM			1
#endif /* MSM PCIE Platforms */

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
static int wlan_host_wake_up = -1;
static int wlan_host_wake_irq = 0;
#ifdef CUSTOMER_HW2
#define WIFI_WLAN_HOST_WAKE_PROPNAME    "wl_host_wake"
#else
#define WIFI_WLAN_HOST_WAKE_PROPNAME    "wlan-host-wake-gpio"
#endif /* CUSTOMER_HW2 */
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */

#include <dhd_plat.h>
typedef struct dhd_plat_info {
	struct msm_pcie_register_event pcie_event;
	struct msm_pcie_notify pcie_notify;
	struct pci_dev *pdev;
} dhd_plat_info_t;

static dhd_pcie_event_cb_t g_pfn = NULL;

char* dhd_get_device_dt_name(void)
{
	return DHD_DT_COMPAT_ENTRY;
}

uint32 dhd_plat_get_info_size(void)
{
	return sizeof(dhd_plat_info_t);
}

void plat_pcie_notify_cb(struct msm_pcie_notify *pcie_notify)
{
	struct pci_dev *pdev;

	if (pcie_notify == NULL) {
		pr_err("%s(): Invalid argument to Platform layer call back \r\n", __func__);
		return;
	}

	if (g_pfn) {
		pdev = (struct pci_dev *)pcie_notify->user;
		pr_err("%s(): Invoking DHD call back with pdev %p \r\n",
				__func__, pdev);
		(*(g_pfn))(pdev);
	} else {
		pr_err("%s(): Driver Call back pointer is NULL \r\n", __func__);
	}
	return;
}

int dhd_plat_pcie_register_event(void *plat_info, struct pci_dev *pdev, dhd_pcie_event_cb_t pfn)
{
		dhd_plat_info_t *p = plat_info;

		if ((p == NULL) || (pdev == NULL) || (pfn == NULL)) {
			pr_err("%s(): Invalid argument p %p, pdev %p, pfn %p\r\n",
				__func__, p, pdev, pfn);
			return -1;
		}

		g_pfn = pfn;
		p->pdev = pdev;
		p->pcie_event.events = MSM_PCIE_EVENT_LINKDOWN;
		p->pcie_event.user = pdev;
		p->pcie_event.mode = MSM_PCIE_TRIGGER_CALLBACK;
		p->pcie_event.callback = plat_pcie_notify_cb;
		p->pcie_event.options = MSM_PCIE_CONFIG_NO_RECOVERY;
		msm_pcie_register_event(&p->pcie_event);
		pr_err("%s(): Registered Event PCIe event pdev %p \r\n", __func__, pdev);
		return 0;
}

void dhd_plat_pcie_deregister_event(void *plat_info)
{
	dhd_plat_info_t *p = plat_info;
	if (p) {
		msm_pcie_deregister_event(&p->pcie_event);
	}
	return;
}

int __init
dhd_wifi_init_gpio(void)
{
	char *wlan_node = DHD_DT_COMPAT_ENTRY;
	struct device_node *root_node = NULL;

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of BRCM WLAN\n");
		return -ENODEV;
	}

	if (!of_device_is_available(root_node)) {
		printk(KERN_ERR "%s: brcm wlan device status is disable\n", __FUNCTION__);
		return -ENXIO;
	}

#ifdef CONFIG_WL_LOAD_SWITCH
    /* ========== WLAN_PWR_EN ============ */
    wlan_pwr_on = of_get_named_gpio(root_node, WIFI_WL_PWR_EN_PROPNAME, 0);
    printk(KERN_INFO "%s: gpio_wlan_power : %d\n", __FUNCTION__, wlan_pwr_on);

    if (gpio_request_one(wlan_pwr_on, GPIOF_DIR_OUT, "WLAN_PWR_EN")) {
        printk(KERN_ERR "%s: Faiiled to request gpio %d for WLAN_PWR_EN\n",
            __FUNCTION__, wlan_pwr_on);
    } else {
        printk(KERN_ERR "%s: gpio_request WLAN_PWR_EN done - WLAN_EN: GPIO %d\n",
            __FUNCTION__, wlan_pwr_on);
    }
#endif /* CONFIG_WL_LOAD_SWITCH */

	/* ========== WLAN_PWR_EN ============ */
	wlan_reg_on = of_get_named_gpio(root_node, WIFI_WL_REG_ON_PROPNAME, 0);
	printk(KERN_INFO "%s: gpio_wlan_power : %d\n", __FUNCTION__, wlan_reg_on);

	if (gpio_request_one(wlan_reg_on, GPIOF_OUT_INIT_LOW, "WL_REG_ON")) {
		printk(KERN_ERR "%s: Faiiled to request gpio %d for WL_REG_ON\n",
			__FUNCTION__, wlan_reg_on);
	} else {
		printk(KERN_ERR "%s: gpio_request WL_REG_ON done - WLAN_EN: GPIO %d\n",
			__FUNCTION__, wlan_reg_on);
	}

	if (gpio_direction_output(wlan_reg_on, 1)) {
		printk(KERN_ERR "%s: WL_REG_ON failed to pull up\n", __FUNCTION__);
	} else {
		printk(KERN_ERR "%s: WL_REG_ON is pulled up\n", __FUNCTION__);
	}

	if (gpio_get_value(wlan_reg_on)) {
		printk(KERN_INFO "%s: Initial WL_REG_ON: [%d]\n",
			__FUNCTION__, gpio_get_value(wlan_reg_on));
	}

	/* Wait for WIFI_TURNON_DELAY due to power stability */
	msleep(WIFI_TURNON_DELAY);

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_named_gpio(root_node, WIFI_WLAN_HOST_WAKE_PROPNAME, 0);
	printk(KERN_INFO "%s: gpio_wlan_host_wake : %d\n", __FUNCTION__, wlan_host_wake_up);

#ifndef CUSTOMER_HW2
	if (gpio_request_one(wlan_host_wake_up, GPIOF_IN, "WLAN_HOST_WAKE")) {
		printk(KERN_ERR "%s: Faiiled to request gpio %d for WLAN_HOST_WAKE\n",
			__FUNCTION__, wlan_host_wake_up);
			return -ENODEV;
	} else {
		printk(KERN_ERR "%s: gpio_request WLAN_HOST_WAKE done"
			" - WLAN_HOST_WAKE: GPIO %d\n",
			__FUNCTION__, wlan_host_wake_up);
	}
#endif /* !CUSTOMER_HW2 */

	gpio_direction_input(wlan_host_wake_up);
	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */

	return 0;
}

int
dhd_wlan_power(int onoff)
{
    struct pci_dev *pcidev = NULL;
    int rc;
    printk(KERN_INFO"%s Enter: power %s\n", __func__, onoff ? "on" : "off");

    if (onoff) {
        if (gpio_get_value(wlan_reg_on)) {
            printk(KERN_INFO"WL_REG_ON on-step-2 : [%d]\n",
                gpio_get_value(wlan_reg_on));
        } else {
            printk("[%s] gpio value is 0. We need reinit.\n", __func__);

#ifdef CONFIG_WL_LOAD_SWITCH
            if (gpio_direction_output(wlan_pwr_on, 1)) {
                printk(KERN_ERR "%s: WLAN_PWR_EN is "
                    "failed to pull up\n", __func__);
            }

            usleep_range(WIFI_LOAD_SWITCH_DELAY_MIN_US, WIFI_LOAD_SWITCH_DELAY_MAX_US);
#endif /* CONFIG_WL_LOAD_SWITCH */

            if (gpio_direction_output(wlan_reg_on, 1)) {
                printk(KERN_ERR "%s: WL_REG_ON is "
                    "failed to pull up\n", __func__);
            }
            /* Wait for WIFI_TURNON_DELAY due to power stability */
            usleep_range(WIFI_TURNON_DELAY_MIN_US,
                     WIFI_TURNON_DELAY_MAX_US);
            msm_pcie_enumerate(MSM_PCIE_CH_NUM);
        }
    }
    pcidev = pci_get_device(PCI_VENDOR_ID_BROADCOM,
                BCM4389_D11AX_ID, pcidev);
    if (!pcidev) {
        pr_warn("%s: can't find PCI device\n", __func__);
        return -ENODEV;
    }
    rc = msm_pcie_pm_control(onoff ? MSM_PCIE_RESUME : MSM_PCIE_SUSPEND,
                 pcidev->bus->number, pcidev, NULL,
                 MSM_PCIE_CONFIG_NO_CFG_RESTORE);
    if (rc) {
        dev_err(&pcidev->dev, "%s: failed to %s link, rc=%d\n",
            __func__, onoff ? "resume" : "suspend", rc);
        return rc;
    }
    if (onoff) {
        rc = msm_pcie_recover_config(pcidev);
        if (rc) {
            dev_err(&pcidev->dev, "%s: failed to recover config, rc=%d\n",
                __func__, rc);
            return rc;
        }
    } else {
        if (gpio_direction_output(wlan_reg_on, 0)) {
            pr_err("%s: WL_REG_ON is failed to pull up\n",
                   __func__);
            return -EIO;
        }
        usleep_range(WIFI_TURNOFF_DELAY_MIN_US,
                 WIFI_TURNOFF_DELAY_MAX_US);
        if (gpio_get_value(wlan_reg_on)) {
            pr_debug("WL_REG_ON on-step-2 : [%d]\n",
                 gpio_get_value(wlan_reg_on));
        }
    }
    dev_dbg(&pcidev->dev, "%s:link successfully %s\n", __func__,
        onoff ? "resumed" : "suspended");
    return 0;
}
EXPORT_SYMBOL(dhd_wlan_power);

static int
dhd_wlan_reset(int onoff)
{
	return 0;
}

static int
dhd_wlan_set_carddetect(int val)
{
#ifdef CONFIG_BCMDHD_PCIE
	printk(KERN_INFO "%s: Call msm_pcie_enumerate\n", __FUNCTION__);
	msm_pcie_enumerate(MSM_PCIE_CH_NUM);
#endif /* CONFIG_BCMDHD_PCIE */
	return 0;
}

#if defined(CONFIG_BCMDHD_OOB_HOST_WAKE) && defined(CONFIG_BCMDHD_GET_OOB_STATE)
int
dhd_get_wlan_oob_gpio(void)
{
	return gpio_is_valid(wlan_host_wake_up) ?
		gpio_get_value(wlan_host_wake_up) : -1;
}
EXPORT_SYMBOL(dhd_get_wlan_oob_gpio);
int
dhd_get_wlan_oob_gpio_number(void)
{
	return gpio_is_valid(wlan_host_wake_up) ?
		wlan_host_wake_up : -1;
}
EXPORT_SYMBOL(dhd_get_wlan_oob_gpio_number);

#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE && CONFIG_BCMDHD_GET_OOB_STATE */

struct resource dhd_wlan_resources = {
	.name	= "bcmdhd_wlan_irq",
	.start	= 0, /* Dummy */
	.end	= 0, /* Dummy */
	.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE |
#ifdef CONFIG_BCMDHD_PCIE
    IORESOURCE_IRQ_HIGHEDGE,
#else
    IORESOURCE_IRQ_HIGHLEVEL,
#endif /* CONFIG_BCMDHD_PCIE */
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

	printk(KERN_INFO"%s: START.......\n", __FUNCTION__);
	ret = dhd_wifi_init_gpio();
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
			" ret=%d\n", __FUNCTION__, ret);
	}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

fail:
	printk(KERN_INFO"%s: FINISH.......\n", __FUNCTION__);
	return ret;
}

int
dhd_wlan_deinit(void)
{
#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	gpio_free(wlan_host_wake_up);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */
	gpio_free(wlan_reg_on);

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	dhd_exit_wlan_mem();
#endif /*  CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	return 0;
}

uint32 dhd_plat_get_rc_vendor_id(void)
{
	return MSM_PCIE_VENDOR_ID;
}

uint32 dhd_plat_get_rc_device_id(void)
{
	return MSM_PCIE_DEVICE_ID;
}

#ifndef BCMDHD_MODULAR
#if defined(CONFIG_ARCH_MSM8996) || defined(CONFIG_ARCH_MSM8998) || \
	defined(CONFIG_ARCH_SDM845) || defined(CONFIG_ARCH_SM8150) || defined(CONFIG_ARCH_KONA) \
	|| defined(CONFIG_ARCH_LAHAINA) || defined(CONFIG_ARCH_WAIPIO)
#if defined(CONFIG_DEFERRED_INITCALLS)
deferred_module_init(dhd_wlan_init);
#else
late_initcall(dhd_wlan_init);
#endif /* CONFIG_DEFERRED_INITCALLS */
#else
device_initcall(dhd_wlan_init);
#endif /* MSM PCIE Platforms */
#endif /* !BCMDHD_MODULAR */
