/*
 * Linux platform device for DHD WLAN adapter
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
 */
#include <typedefs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <bcmutils.h>
#include <linux_osl.h>
#include <dhd_dbg.h>
#include <dngl_stats.h>
#include <dhd.h>
#include <dhd_bus.h>
#include <dhd_linux.h>
#if defined(OEM_ANDROID)
#include <wl_android.h>
#endif
#include <dhd_plat.h>
#if defined(CONFIG_WIFI_CONTROL_FUNC) || defined(CUSTOMER_HW4)
#include <linux/wlan_plat.h>
#endif /* CONFIG_WIFI_CONTROL_FUNC */
#ifdef BCMDBUS
#include <dbus.h>
#endif
#ifdef CONFIG_DTS
#include<linux/regulator/consumer.h>
#include<linux/of_gpio.h>
#endif /* CONFIG_DTS */
#define WIFI_PLAT_NAME		"bcmdhd_wlan"
#define WIFI_PLAT_NAME2		"bcm4329_wlan"
#define WIFI_PLAT_EXT		"bcmdhd_wifi_platform"

#if defined(SUPPORT_MULTIPLE_BOARD_REVISION)
#include <linux/of.h>
extern char* dhd_get_device_dt_name(void);
#endif /* SUPPORT_MULTIPLE_BOARD_REVISION */

#ifdef DHD_WIFI_SHUTDOWN
extern void wifi_plat_dev_drv_shutdown(struct platform_device *pdev);
#endif

#ifdef CONFIG_DTS
struct regulator *wifi_regulator = NULL;
#endif /* CONFIG_DTS */

bool cfg_multichip = FALSE;
bcmdhd_wifi_platdata_t *dhd_wifi_platdata = NULL;
static int wifi_plat_dev_probe_ret = 0;
static bool is_power_on = FALSE;
/* XXX Some Qualcomm based CUSTOMER_HW4 platforms are using platform
 * device structure even if the Kernel uses device tree structure.
 * Therefore, the CONFIG_ARCH_MSM condition is temporarly remained
 * to support in this case.
 */
#if !defined(CONFIG_DTS)
#if defined(DHD_OF_SUPPORT)
static bool dts_enabled = TRUE;
extern struct resource dhd_wlan_resources;
extern struct wifi_platform_data dhd_wlan_control;
#else
static bool dts_enabled = FALSE;
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
struct resource dhd_wlan_resources = {0};
struct wifi_platform_data dhd_wlan_control = {0};
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif /* CONFIG_OF && !defined(CONFIG_ARCH_MSM) */
#endif /* !defind(CONFIG_DTS) */

static int dhd_wifi_platform_load(void);

extern void* wl_cfg80211_get_dhdp(struct net_device *dev);

#ifdef BCMDHD_MODULAR
extern int dhd_wlan_init(void);
extern int dhd_wlan_deinit(void);
#ifdef WBRC
extern int wbrc_init(void);
extern void wbrc_exit(void);
#endif /* WBRC */
#endif /* BCMDHD_MODULAR */

#ifdef ENABLE_4335BT_WAR
extern int bcm_bt_lock(int cookie);
extern void bcm_bt_unlock(int cookie);
static int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */
#endif /* ENABLE_4335BT_WAR */

#ifdef BCM4335_XTAL_WAR
extern bool check_bcm4335_rev(void);
#endif /* BCM4335_XTAL_WAR */

#if defined(CONFIG_X86)
#define PCIE_RC_VENDOR_ID 0x8086
#define PCIE_RC_DEVICE_ID 0x9c1a
#elif defined(CONFIG_ARCH_TEGRA)
#define PCIE_RC_VENDOR_ID 0x14e4
#define PCIE_RC_DEVICE_ID 0x4347
#else /* CONFIG_ARCH_TEGRA */
/* Dummy defn */
#define PCIE_RC_VENDOR_ID 0xffff
#define PCIE_RC_DEVICE_ID 0xffff
#endif /* CONFIG_X86 */

wifi_adapter_info_t* dhd_wifi_platform_get_adapter(uint32 bus_type, uint32 bus_num, uint32 slot_num)
{
	int i;

	if (dhd_wifi_platdata == NULL)
		return NULL;

	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		wifi_adapter_info_t *adapter = &dhd_wifi_platdata->adapters[i];
		if ((adapter->bus_type == -1 || adapter->bus_type == bus_type) &&
			(adapter->bus_num == -1 || adapter->bus_num == bus_num) &&
			(adapter->slot_num == -1 || adapter->slot_num == slot_num)) {
			DHD_TRACE(("found adapter info '%s'\n", adapter->name));
			return adapter;
		}
	}
	return NULL;
}

void* wifi_platform_prealloc(wifi_adapter_info_t *adapter, int section, unsigned long size)
{
	void *alloc_ptr = NULL;
	struct wifi_platform_data *plat_data;

	if (!adapter || !adapter->wifi_plat_data)
		return NULL;
	plat_data = adapter->wifi_plat_data;
	if (plat_data->mem_prealloc) {
		alloc_ptr = plat_data->mem_prealloc(section, size);
		if (alloc_ptr) {
			DHD_INFO(("success alloc section %d\n", section));
			if (size != 0L)
				bzero(alloc_ptr, size);
			return alloc_ptr;
		}
	}

	DHD_ERROR(("%s: failed to alloc static mem section %d\n", __FUNCTION__, section));
	return NULL;
}

void* wifi_platform_get_prealloc_func_ptr(wifi_adapter_info_t *adapter)
{
	struct wifi_platform_data *plat_data;

	if (!adapter || !adapter->wifi_plat_data)
		return NULL;
	plat_data = adapter->wifi_plat_data;
	return plat_data->mem_prealloc;
}

int wifi_platform_get_irq_number(wifi_adapter_info_t *adapter, unsigned long *irq_flags_ptr)
{
	if (adapter == NULL)
		return -1;
	if (irq_flags_ptr)
		*irq_flags_ptr = adapter->intr_flags;
	return adapter->irq_num;
}

int wifi_platform_set_power(wifi_adapter_info_t *adapter, bool on, unsigned long msec)
{
	int err = 0;
#ifdef CONFIG_DTS
	if (on) {
		err = regulator_enable(wifi_regulator);
		is_power_on = TRUE;
	}
	else {
		err = regulator_disable(wifi_regulator);
		is_power_on = FALSE;
	}
	if (err < 0)
		DHD_ERROR(("%s: regulator enable/disable failed", __FUNCTION__));
#else
	struct wifi_platform_data *plat_data;

	if (!adapter || !adapter->wifi_plat_data)
		return -EINVAL;
	plat_data = adapter->wifi_plat_data;

	DHD_ERROR(("%s = %d, delay: %lu msec\n", __FUNCTION__, on, msec));
	if (plat_data->set_power) {
#ifdef ENABLE_4335BT_WAR
		if (on) {
			printk("WiFi: trying to acquire BT lock\n");
			if (bcm_bt_lock(lock_cookie_wifi) != 0)
				printk("** WiFi: timeout in acquiring bt lock**\n");
			printk("%s: btlock acquired\n", __FUNCTION__);
		}
		else {
			/* For a exceptional case, release btlock */
			bcm_bt_unlock(lock_cookie_wifi);
		}
#endif /* ENABLE_4335BT_WAR */

#ifdef BCM4335_XTAL_WAR
		err = plat_data->set_power(on, check_bcm4335_rev());
#else /* BCM4335_XTAL_WAR */
		err = plat_data->set_power(on);
#endif /* BCM4335_XTAL_WAR */
	}

	if (msec && !err) {
		OSL_SLEEP(msec);
		DHD_ERROR(("%s = %d, sleep done: %lu msec\n", __FUNCTION__, on, msec));
	}

	if (on && !err)
		is_power_on = TRUE;
	else
		is_power_on = FALSE;

#endif /* CONFIG_DTS */

	return err;
}

int wifi_platform_bus_enumerate(wifi_adapter_info_t *adapter, bool device_present)
{
	int err = 0;
	struct wifi_platform_data *plat_data;

	if (!adapter || !adapter->wifi_plat_data)
		return -EINVAL;
	plat_data = adapter->wifi_plat_data;

	DHD_ERROR(("%s device present %d\n", __FUNCTION__, device_present));
	if (plat_data->set_carddetect) {
		err = plat_data->set_carddetect(device_present);
	}
	return err;

}

int wifi_platform_get_mac_addr(wifi_adapter_info_t *adapter, unsigned char *buf)
{
	struct wifi_platform_data *plat_data;

	DHD_INFO(("%s\n", __FUNCTION__));
	if (!buf || !adapter || !adapter->wifi_plat_data)
		return -EINVAL;
	plat_data = adapter->wifi_plat_data;
	if (plat_data->get_mac_addr) {
		return plat_data->get_mac_addr(buf);
	}
	return -EOPNOTSUPP;
}

#ifdef DHD_COREDUMP
int wifi_platform_set_coredump(wifi_adapter_info_t *adapter, const char *buf,
	int buf_len, const char *info)
{
	struct wifi_platform_data *plat_data;

	DHD_ERROR(("%s\n", __FUNCTION__));
	if (!buf || !adapter || !adapter->wifi_plat_data)
		return -EINVAL;
	plat_data = adapter->wifi_plat_data;
	if (plat_data->set_coredump) {
		return plat_data->set_coredump(buf, buf_len, info);
	}
	return -EOPNOTSUPP;
}
#endif /* DHD_COREDUMP */

#ifdef	CUSTOM_COUNTRY_CODE
void *wifi_platform_get_country_code(wifi_adapter_info_t *adapter, char *ccode, u32 flags)
#else
void *wifi_platform_get_country_code(wifi_adapter_info_t *adapter, char *ccode)
#endif /* CUSTOM_COUNTRY_CODE */
{
	/* get_country_code was added after 2.6.39 */
#if	(LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
	struct wifi_platform_data *plat_data;

	if (!ccode || !adapter || !adapter->wifi_plat_data)
		return NULL;
	plat_data = adapter->wifi_plat_data;

	DHD_TRACE(("%s\n", __FUNCTION__));
	if (plat_data->get_country_code) {
#if     (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 58))
		return plat_data->get_country_code(ccode, WLAN_PLAT_NODFS_FLAG);
#else
#ifdef	CUSTOM_COUNTRY_CODE
		return plat_data->get_country_code(ccode, flags);
#else
		return plat_data->get_country_code(ccode);
#endif /* CUSTOM_COUNTRY_CODE */
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 58)) */
	}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)) */

	return NULL;
}

static int wifi_plat_dev_drv_probe(struct platform_device *pdev)
{
	struct resource *resource;
	wifi_adapter_info_t *adapter;
#ifdef CONFIG_DTS
	int irq, gpio;
#endif /* CONFIG_DTS */

	/* Android style wifi platform data device ("bcmdhd_wlan" or "bcm4329_wlan")
	 * is kept for backward compatibility and supports only 1 adapter
	 */
	ASSERT(dhd_wifi_platdata != NULL);
	ASSERT(dhd_wifi_platdata->num_adapters == 1);
	adapter = &dhd_wifi_platdata->adapters[0];
	adapter->wifi_plat_data = (struct wifi_platform_data *)(pdev->dev.platform_data);

	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "bcmdhd_wlan_irq");
	if (resource == NULL)
		resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "bcm4329_wlan_irq");
	if (resource) {
		adapter->irq_num = resource->start;
		adapter->intr_flags = resource->flags & IRQF_TRIGGER_MASK;
#ifdef DHD_ISR_NO_SUSPEND
		adapter->intr_flags |= IRQF_NO_SUSPEND;
#endif
	}

#ifdef CONFIG_DTS
	wifi_regulator = regulator_get(&pdev->dev, "wlreg_on");
	if (wifi_regulator == NULL) {
		DHD_ERROR(("%s regulator is null\n", __FUNCTION__));
		return -1;
	}

	/* This is to get the irq for the OOB */
	gpio = of_get_gpio(pdev->dev.of_node, 0);

	if (gpio < 0) {
		DHD_ERROR(("%s gpio information is incorrect\n", __FUNCTION__));
		return -1;
	}
	irq = gpio_to_irq(gpio);
	if (irq < 0) {
		DHD_ERROR(("%s irq information is incorrect\n", __FUNCTION__));
		return -1;
	}
	adapter->irq_num = irq;

	/* need to change the flags according to our requirement */
	adapter->intr_flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL |
		IORESOURCE_IRQ_SHAREABLE;
#endif /* CONFIG_DTS */

	wifi_plat_dev_probe_ret = dhd_wifi_platform_load();
	return wifi_plat_dev_probe_ret;
}

static int wifi_plat_dev_drv_remove(struct platform_device *pdev)
{
	wifi_adapter_info_t *adapter;

	/* Android style wifi platform data device ("bcmdhd_wlan" or "bcm4329_wlan")
	 * is kept for backward compatibility and supports only 1 adapter
	 */
	ASSERT(dhd_wifi_platdata != NULL);
	ASSERT(dhd_wifi_platdata->num_adapters == 1);
	adapter = &dhd_wifi_platdata->adapters[0];
	if (is_power_on) {
#ifdef BCMPCIE
		wifi_platform_bus_enumerate(adapter, FALSE);
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
#else
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
		wifi_platform_bus_enumerate(adapter, FALSE);
#endif /* BCMPCIE */
	}

#ifdef CONFIG_DTS
	regulator_put(wifi_regulator);
#endif /* CONFIG_DTS */
	return 0;
}

static int wifi_plat_dev_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	DHD_TRACE(("##> %s\n", __FUNCTION__));
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY) && \
	defined(BCMSDIO)
	bcmsdh_oob_intr_set(0);
#endif /* (OOB_INTR_ONLY) */
	return 0;
}

static int wifi_plat_dev_drv_resume(struct platform_device *pdev)
{
	DHD_TRACE(("##> %s\n", __FUNCTION__));
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && defined(OOB_INTR_ONLY) && \
	defined(BCMSDIO)
	if (dhd_os_check_if_up(wl_cfg80211_get_dhdp()))
		bcmsdh_oob_intr_set(1);
#endif /* (OOB_INTR_ONLY) */
	return 0;
}

#ifdef CONFIG_DTS
static const struct of_device_id wifi_device_dt_match[] = {
	{ .compatible = "android,bcmdhd_wlan", },
	{},
};
#endif /* CONFIG_DTS */
static struct platform_driver wifi_platform_dev_driver = {
	.probe          = wifi_plat_dev_drv_probe,
	.remove         = wifi_plat_dev_drv_remove,
	.suspend        = wifi_plat_dev_drv_suspend,
	.resume         = wifi_plat_dev_drv_resume,
#ifdef DHD_WIFI_SHUTDOWN
	.shutdown       = wifi_plat_dev_drv_shutdown,
#endif /* DHD_WIFI_SHUTDOWN */
	.driver         = {
	.name   = WIFI_PLAT_NAME,
#ifdef CONFIG_DTS
	.of_match_table = wifi_device_dt_match,
#endif /* CONFIG_DTS */
	}
};

static struct platform_driver wifi_platform_dev_driver_legacy = {
	.probe          = wifi_plat_dev_drv_probe,
	.remove         = wifi_plat_dev_drv_remove,
	.suspend        = wifi_plat_dev_drv_suspend,
	.resume         = wifi_plat_dev_drv_resume,
#ifdef DHD_WIFI_SHUTDOWN
	.shutdown       = wifi_plat_dev_drv_shutdown,
#endif /* DHD_WIFI_SHUTDOWN */
	.driver         = {
	.name	= WIFI_PLAT_NAME2,
	}
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
static int wifi_platdev_match(struct device *dev, const void *data)
#else
static int wifi_platdev_match(struct device *dev, void *data)
#endif /* LINUX_VER >= 5.3.0 */
{
	char *name = (char*)data;
	const struct platform_device *pdev;
	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	pdev = to_platform_device(dev);
	GCC_DIAGNOSTIC_POP();

	if (strcmp(pdev->name, name) == 0) {
		DHD_ERROR(("found wifi platform device %s\n", name));
		return TRUE;
	}

	return FALSE;
}

static int wifi_ctrlfunc_register_drv(void)
{
	int err = 0;
	struct device *dev1, *dev2;
	wifi_adapter_info_t *adapter;

	dev1 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME, wifi_platdev_match);
	dev2 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME2, wifi_platdev_match);

#ifdef BCMDHD_MODULAR
	if ((err = dhd_wlan_init())) {
		DHD_ERROR(("%s: dhd_wlan_init() failed(%d)\n", __FUNCTION__, err));
		return err;
	}
#ifdef WBRC
	wbrc_init();
#endif /* WBRC */
#endif /* BCMDHD_MODULAR */

#if !defined(CONFIG_DTS)
	if (!dts_enabled) {
		if (dev1 == NULL && dev2 == NULL) {
			DHD_ERROR(("no wifi platform data, skip\n"));
			return -ENXIO;
		}
	}
#endif /* !defined(CONFIG_DTS) */

	/* multi-chip support not enabled, build one adapter information for
	 * DHD (either SDIO, USB or PCIe)
	 */
	adapter = kzalloc(sizeof(wifi_adapter_info_t), GFP_KERNEL);
	if (adapter == NULL) {
		DHD_ERROR(("%s:adapter alloc failed", __FUNCTION__));
		return ENOMEM;
	}
	adapter->name = "DHD generic adapter";
	adapter->bus_type = -1;
	adapter->bus_num = -1;
	adapter->slot_num = -1;
	adapter->irq_num = -1;
	is_power_on = FALSE;
	wifi_plat_dev_probe_ret = 0;
	dhd_wifi_platdata = kzalloc(sizeof(bcmdhd_wifi_platdata_t), GFP_KERNEL);
	dhd_wifi_platdata->num_adapters = 1;
	dhd_wifi_platdata->adapters = adapter;

	if (dev1) {
		err = platform_driver_register(&wifi_platform_dev_driver);
		if (err) {
			DHD_ERROR(("%s: failed to register wifi ctrl func driver\n",
				__FUNCTION__));
			return err;
		}
	}
	if (dev2) {
		err = platform_driver_register(&wifi_platform_dev_driver_legacy);
		if (err) {
			DHD_ERROR(("%s: failed to register wifi ctrl func legacy driver\n",
				__FUNCTION__));
			return err;
		}
	}
#if !defined(CONFIG_DTS)
	if (dts_enabled) {
		struct resource *resource;
		adapter->wifi_plat_data = (void *)&dhd_wlan_control;
		resource = &dhd_wlan_resources;
		adapter->irq_num = resource->start;
		adapter->intr_flags = resource->flags & IRQF_TRIGGER_MASK;
#ifdef DHD_ISR_NO_SUSPEND
		adapter->intr_flags |= IRQF_NO_SUSPEND;
#endif
		wifi_plat_dev_probe_ret = dhd_wifi_platform_load();
	}
#endif /* !defined(CONFIG_DTS) */

#ifdef CONFIG_DTS
	wifi_plat_dev_probe_ret = platform_driver_register(&wifi_platform_dev_driver);
#endif /* CONFIG_DTS */

	/* return probe function's return value if registeration succeeded */
	return wifi_plat_dev_probe_ret;
}

void wifi_ctrlfunc_unregister_drv(void)
{

#ifdef CONFIG_DTS
	DHD_ERROR(("unregister wifi platform drivers\n"));
	platform_driver_unregister(&wifi_platform_dev_driver);
#else
	struct device *dev1, *dev2;
	dev1 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME, wifi_platdev_match);
	dev2 = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_NAME2, wifi_platdev_match);
	if (!dts_enabled)
		if (dev1 == NULL && dev2 == NULL)
			return;

	DHD_ERROR(("unregister wifi platform drivers\n"));

	if (dev1)
		platform_driver_unregister(&wifi_platform_dev_driver);
	if (dev2)
		platform_driver_unregister(&wifi_platform_dev_driver_legacy);

	if (!dhd_wifi_platdata) {
		goto done;
	}

	if (dts_enabled) {
		wifi_adapter_info_t *adapter;
		adapter = &dhd_wifi_platdata->adapters[0];
		if (is_power_on) {
			wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
			wifi_platform_bus_enumerate(adapter, FALSE);
		}
	}
#ifdef BCMDHD_MODULAR
	dhd_wlan_deinit();
	osl_static_mem_deinit(NULL, NULL);
#ifdef WBRC
	wbrc_exit();
#endif /* WBRC */
#endif /* BCMDHD_MODULAR */

#endif /* !defined(CONFIG_DTS) */

done:
	if (dhd_wifi_platdata && dhd_wifi_platdata->adapters) {
		kfree(dhd_wifi_platdata->adapters);
		dhd_wifi_platdata->adapters = NULL;
		dhd_wifi_platdata->num_adapters = 0;
	}
	if (dhd_wifi_platdata) {
		kfree(dhd_wifi_platdata);
		dhd_wifi_platdata = NULL;
	}
}

static int bcmdhd_wifi_plat_dev_drv_probe(struct platform_device *pdev)
{
	dhd_wifi_platdata = (bcmdhd_wifi_platdata_t *)(pdev->dev.platform_data);

	return dhd_wifi_platform_load();
}

static int bcmdhd_wifi_plat_dev_drv_remove(struct platform_device *pdev)
{
	int i;
	wifi_adapter_info_t *adapter;
	ASSERT(dhd_wifi_platdata != NULL);

	/* power down all adapters */
	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		adapter = &dhd_wifi_platdata->adapters[i];
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
		wifi_platform_bus_enumerate(adapter, FALSE);
	}
	return 0;
}

static struct platform_driver dhd_wifi_platform_dev_driver = {
	.probe          = bcmdhd_wifi_plat_dev_drv_probe,
	.remove         = bcmdhd_wifi_plat_dev_drv_remove,
	.driver         = {
	.name   = WIFI_PLAT_EXT,
	}
};

int __init dhd_wifi_platform_register_drv(void)
{
	int err = 0;
	struct device *dev;

	/* register Broadcom wifi platform data driver if multi-chip is enabled,
	 * otherwise use Android style wifi platform data (aka wifi control function)
	 * if it exists
	 *
	 * to support multi-chip DHD, Broadcom wifi platform data device must
	 * be added in kernel early boot (e.g. board config file).
	 */
	if (cfg_multichip) {
		dev = bus_find_device(&platform_bus_type, NULL, WIFI_PLAT_EXT, wifi_platdev_match);
		if (dev == NULL) {
			DHD_ERROR(("bcmdhd wifi platform data device not found!!\n"));
			return -ENXIO;
		}
		err = platform_driver_register(&dhd_wifi_platform_dev_driver);
	} else {
		err = wifi_ctrlfunc_register_drv();

		/* no wifi ctrl func either, load bus directly and ignore this error */
		if (err) {
			if (err == -ENXIO) {
				/* wifi ctrl function does not exist */
				err = dhd_wifi_platform_load();
			} else {
				/* unregister driver due to initialization failure */
				wifi_ctrlfunc_unregister_drv();
			}
		}
	}

	return err;
}

#ifdef BCMPCIE
static int dhd_wifi_platform_load_pcie(void)
{
	int i;
	int err;
	int retry;
	wifi_adapter_info_t *adapter;

	if (dhd_wifi_platdata) {
		/* enumerate PCIe RC */
		for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
			adapter = &dhd_wifi_platdata->adapters[i];
			err = wifi_platform_bus_enumerate(adapter, TRUE);
			if (err) {
				DHD_ERROR(("failed to enumerate bus %s err=%d",
					adapter->name, err));
				return err;
			}
		}
#ifdef DHD_SUPPORT_HDM
		if (dhd_download_fw_on_driverload || hdm_trigger_init) {
#else
		if (dhd_download_fw_on_driverload) {
#endif /* DHD_SUPPORT_HDM */
			/* power up all adapters */
			for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
				retry = POWERUP_MAX_RETRY;
				adapter = &dhd_wifi_platdata->adapters[i];

				DHD_ERROR(("Power-up adapter '%s'\n", adapter->name));
				DHD_INFO((" - irq %d [flags %d], firmware: %s, nvram: %s\n",
					adapter->irq_num, adapter->intr_flags, adapter->fw_path,
					adapter->nv_path));
				DHD_INFO((" - bus type %d, bus num %d, slot num %d\n\n",
					adapter->bus_type, adapter->bus_num, adapter->slot_num));

				do {
					err = wifi_platform_set_power(adapter,
						TRUE, WIFI_TURNON_DELAY);
					if (err) {
						DHD_ERROR(("failed to power up %s,"
							" %d retry left\n",
							adapter->name, retry));
						/* WL_REG_ON state unknown, Power off forcely */
						wifi_platform_set_power(adapter,
							FALSE, WIFI_TURNOFF_DELAY);
						continue;
					}

					err = wifi_platform_bus_enumerate(adapter, TRUE);
					if (err) {
						DHD_ERROR(("failed to enumerate bus %s, "
							"%d retry left\n",
							adapter->name, retry));
						wifi_platform_set_power(adapter, FALSE,
							WIFI_TURNOFF_DELAY);
					} else {
						break;
					}
				} while (retry--);

				if (retry < 0) {
					DHD_ERROR(("failed to power up %s, max retry reached**\n",
						adapter->name));
					return -ENODEV;
				}
			}
		}
	}

	err = dhd_bus_register();
	if (err) {
		DHD_ERROR(("%s: dhd_bus_register failed err=%d\n", __FUNCTION__, err));
		if (dhd_wifi_platdata && dhd_download_fw_on_driverload) {
			/* power down all adapters */
			for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
				adapter = &dhd_wifi_platdata->adapters[i];
				wifi_platform_bus_enumerate(adapter, FALSE);
				wifi_platform_set_power(adapter,
					FALSE, WIFI_TURNOFF_DELAY);
			}
		}
	}

	return err;
}
#else
static int dhd_wifi_platform_load_pcie(void)
{
	return 0;
}
#endif /* BCMPCIE  */

void dhd_wifi_platform_unregister_drv(void)
{
	if (cfg_multichip)
		platform_driver_unregister(&dhd_wifi_platform_dev_driver);
	else
		wifi_ctrlfunc_unregister_drv();
}

extern int dhd_watchdog_prio;
extern int dhd_dpc_prio;
extern uint dhd_deferred_tx;
#if defined(OEM_ANDROID) && defined(BCMLXSDMMC)
extern struct semaphore dhd_registration_sem;
#endif /* defined(OEM_ANDROID) && defined(BCMLXSDMMC) */

#ifdef BCMSDIO
static int dhd_wifi_platform_load_sdio(void)
{
	int i;
	int err = 0;
	wifi_adapter_info_t *adapter;

	BCM_REFERENCE(i);
	BCM_REFERENCE(adapter);
	/* Sanity check on the module parameters
	 * - Both watchdog and DPC as tasklets are ok
	 * - If both watchdog and DPC are threads, TX must be deferred
	 */
	if (!(dhd_watchdog_prio < 0 && dhd_dpc_prio < 0) &&
		!(dhd_watchdog_prio >= 0 && dhd_dpc_prio >= 0 && dhd_deferred_tx))
		return -EINVAL;

#if defined(OEM_ANDROID) && defined(BCMLXSDMMC)
	sema_init(&dhd_registration_sem, 0);
#endif

	if (dhd_wifi_platdata == NULL) {
		DHD_ERROR(("DHD wifi platform data is required for Android build\n"));
		DHD_ERROR(("DHD registering bus directly\n"));
		/* x86 bring-up PC needs no power-up operations */
		err = dhd_bus_register();
		return err;
	}

#if defined(OEM_ANDROID) && defined(BCMLXSDMMC)
	/* power up all adapters */
	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		bool chip_up = FALSE;
		int retry = POWERUP_MAX_RETRY;
		struct semaphore dhd_chipup_sem;

		adapter = &dhd_wifi_platdata->adapters[i];

		DHD_ERROR(("Power-up adapter '%s'\n", adapter->name));
		DHD_INFO((" - irq %d [flags %d], firmware: %s, nvram: %s\n",
			adapter->irq_num, adapter->intr_flags, adapter->fw_path, adapter->nv_path));
		DHD_INFO((" - bus type %d, bus num %d, slot num %d\n\n",
			adapter->bus_type, adapter->bus_num, adapter->slot_num));

		do {
			sema_init(&dhd_chipup_sem, 0);
			err = dhd_bus_reg_sdio_notify(&dhd_chipup_sem);
			if (err) {
				DHD_ERROR(("%s dhd_bus_reg_sdio_notify fail(%d)\n\n",
					__FUNCTION__, err));
				return err;
			}
			err = wifi_platform_set_power(adapter, TRUE, WIFI_TURNON_DELAY);
			if (err) {
				DHD_ERROR(("%s: wifi pwr on error ! \n", __FUNCTION__));
				dhd_bus_unreg_sdio_notify();
				/* WL_REG_ON state unknown, Power off forcely */
				wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
				continue;
			} else {
				wifi_platform_bus_enumerate(adapter, TRUE);
			}

			if (down_timeout(&dhd_chipup_sem, msecs_to_jiffies(POWERUP_WAIT_MS)) == 0) {
				dhd_bus_unreg_sdio_notify();
				chip_up = TRUE;
				break;
			}

			DHD_ERROR(("failed to power up %s, %d retry left\n", adapter->name, retry));
			dhd_bus_unreg_sdio_notify();
			wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
			wifi_platform_bus_enumerate(adapter, FALSE);
		} while (retry--);

		if (!chip_up) {
			DHD_ERROR(("failed to power up %s, max retry reached**\n", adapter->name));
			return -ENODEV;
		}

	}

	err = dhd_bus_register();

	if (err) {
		DHD_ERROR(("%s: sdio_register_driver failed\n", __FUNCTION__));
		goto fail;
	}

	/*
	 * Wait till MMC sdio_register_driver callback called and made driver attach.
	 * It's needed to make sync up exit from dhd insmod  and
	 * Kernel MMC sdio device callback registration
	 */
	err = down_timeout(&dhd_registration_sem, msecs_to_jiffies(DHD_REGISTRATION_TIMEOUT));
	if (err) {
		DHD_ERROR(("%s: sdio_register_driver timeout or error \n", __FUNCTION__));
		dhd_bus_unregister();
		goto fail;
	}

	return err;

fail:
	/* power down all adapters */
	for (i = 0; i < dhd_wifi_platdata->num_adapters; i++) {
		adapter = &dhd_wifi_platdata->adapters[i];
		wifi_platform_set_power(adapter, FALSE, WIFI_TURNOFF_DELAY);
		wifi_platform_bus_enumerate(adapter, FALSE);
	}
#endif /* defined(OEM_ANDROID) && defined(BCMLXSDMMC) */

	return err;
}
#else /* BCMSDIO */
static int dhd_wifi_platform_load_sdio(void)
{
	return 0;
}
#endif /* BCMSDIO */

#ifdef BCMDBUS
/* User-specified vid/pid */
int dhd_vid = 0xa5c;
int dhd_pid = 0x48f;
module_param(dhd_vid, int, 0);
module_param(dhd_pid, int, 0);
void *dhd_dbus_probe_cb(void *arg, const char *desc, uint32 bustype, uint32 hdrlen);
void dhd_dbus_disconnect_cb(void *arg);

static int dhd_wifi_platform_load_usb(void)
{
	int err = 0;

	if (dhd_vid < 0 || dhd_vid > 0xffff) {
		DHD_ERROR(("%s: invalid dhd_vid 0x%x\n", __FUNCTION__, dhd_vid));
		return -EINVAL;
	}
	if (dhd_pid < 0 || dhd_pid > 0xffff) {
		DHD_ERROR(("%s: invalid dhd_pid 0x%x\n", __FUNCTION__, dhd_pid));
		return -EINVAL;
	}

	err = dbus_register(dhd_vid, dhd_pid, dhd_dbus_probe_cb, dhd_dbus_disconnect_cb,
		NULL, NULL, NULL);

	/* Device not detected */
	if (err == DBUS_ERR_NODEVICE)
		err = DBUS_OK;

	return err;
}
#else /* BCMDBUS */
static int dhd_wifi_platform_load_usb(void)
{
	return 0;
}
#endif /* BCMDBUS */

static int dhd_wifi_platform_load()
{
	int err = 0;

#if defined(OEM_ANDROID)
		wl_android_init();
#endif /* OEM_ANDROID */

	if ((err = dhd_wifi_platform_load_usb()))
		goto end;
	else if ((err = dhd_wifi_platform_load_sdio()))
		goto end;
	else
		err = dhd_wifi_platform_load_pcie();

end:
#if defined(OEM_ANDROID)
	if (err)
		wl_android_exit();
	else
		wl_android_post_init();
#endif /* OEM_ANDROID */

	return err;
}

#if defined(SUPPORT_MULTIPLE_BOARD_REVISION)
void
concate_custom_board_revision(char *nv_path)
{
	uint32 board_revision = 0;
	struct device_node *root_node = NULL;
	char* wlan_node = NULL;

	if (!nv_path) {
		DHD_ERROR(("nv_path is null\n"));
		return;
	}

	wlan_node = dhd_get_device_dt_name();
	if (!wlan_node) {
		DHD_ERROR(("Failed to dt name\n"));
		return;
	}

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		DHD_ERROR(("Failed to get device node\n"));
		return;
	}

	if (of_property_read_u32(root_node, "nvram-ES", &board_revision)) {
		DHD_ERROR(("No board revision property in dtsi\n"));
		return;
	}

	DHD_INFO(("Board revision:%d\n", board_revision));

	if (board_revision == 1) {
		strcat(nv_path, "_ES10");
		DHD_INFO(("Mached Board revision ES10: nvram name:%s\n", nv_path));
	}

}
#endif /* SUPPORT_MULTIPLE_BOARD_REVISION */

/* Weak functions that can be overridden in Platform specific implementation */
char* __attribute__ ((weak)) dhd_get_device_dt_name(void)
{
	return NULL;
}

uint32 __attribute__ ((weak)) dhd_plat_get_info_size(void)
{
	return 0;
}

int __attribute__ ((weak)) dhd_plat_pcie_register_event(void *plat_info,
		struct pci_dev *pdev, dhd_pcie_event_cb_t pfn)
{
	return 0;
}

void __attribute__ ((weak)) dhd_plat_pcie_deregister_event(void *plat_info)
{
	return;
}

void __attribute__ ((weak)) dhd_plat_l1ss_ctrl(bool ctrl)
{
	return;
}

void __attribute__ ((weak)) dhd_plat_l1_exit_io(void)
{
	return;
}

void __attribute__ ((weak)) dhd_plat_l1_exit(void)
{
	return;
}

void __attribute__ ((weak)) dhd_plat_report_bh_sched(void *plat_info, int resched)
{
	return;
}

int __attribute__ ((weak)) dhd_plat_pcie_suspend(void *plat_info)
{
	return 0;
}

int __attribute__ ((weak)) dhd_plat_pcie_resume(void *plat_info)
{
	return 0;
}

void __attribute__ ((weak)) dhd_plat_pcie_register_dump(void *plat_info)
{
	return;
}

void __attribute__ ((weak)) dhd_plat_pin_dbg_show(void *plat_info)
{
	return;
}

uint32 __attribute__ ((weak)) dhd_plat_get_rc_vendor_id(void)
{
	return PCIE_RC_VENDOR_ID;
}

uint32 __attribute__ ((weak)) dhd_plat_get_rc_device_id(void)
{
	return PCIE_RC_DEVICE_ID;
}

uint16 __attribute__ ((weak)) dhd_plat_align_rxbuf_size(uint16 rxbufpost_sz)
{
	return rxbufpost_sz;
}

int
__attribute__ ((weak)) dhd_get_platform_naming_for_nvram_clmblob_file(download_type_t component,
	char *file_name)
{
	return BCME_ERROR;
}
