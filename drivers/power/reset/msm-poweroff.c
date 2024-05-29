// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/of_address.h>
#include <linux/qcom_scm.h>
#include <linux/nvmem-consumer.h>

#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/memory.h>

#include <soc/qcom/restart.h>
#include <soc/qcom/watchdog.h>
#include <soc/qcom/minidump.h>

#define EMERGENCY_DLOAD_MAGIC1    0x322A4F99
#define EMERGENCY_DLOAD_MAGIC2    0xC67E4350
#define EMERGENCY_DLOAD_MAGIC3    0x77777777
#define EMMC_DLOAD_TYPE		0x2

#define SCM_DLOAD_FULLDUMP		QCOM_DOWNLOAD_FULLDUMP
#define SCM_EDLOAD_MODE			QCOM_DOWNLOAD_EDL
#define SCM_DLOAD_MINIDUMP		QCOM_DOWNLOAD_MINIDUMP
#define SCM_DLOAD_BOTHDUMPS	(SCM_DLOAD_FULLDUMP | SCM_DLOAD_MINIDUMP)

#define DL_MODE_PROP "qcom,msm-imem-download_mode"
#define EDL_MODE_PROP "qcom,msm-imem-emergency_download_mode"
#define IMEM_DL_TYPE_PROP "qcom,msm-imem-dload-type"

#define KASLR_OFFSET_PROP "qcom,msm-imem-kaslr_offset"
#define KASLR_OFFSET_BIT_MASK	0x00000000FFFFFFFF

static int restart_mode;
static void *restart_reason, *dload_type_addr;
/* Download mode master kill-switch */
static void __iomem *msm_ps_hold;
static phys_addr_t tcsr_boot_misc_detect;
static struct nvmem_cell *nvmem_cell;

/*
 * Runtime could be only changed value once.
 * There is no API from TZ to re-enable the registers.
 * So the SDI cannot be re-enabled when it already by-passed.
 */
static int download_mode = 1;
static struct kobject dload_kobj;

static int in_panic;
static int dload_type = SCM_DLOAD_FULLDUMP;
static void *dload_mode_addr;
static bool dload_mode_enabled;
static void *emergency_dload_mode_addr;

static bool force_warm_reboot;

static struct notifier_block restart_nb;

/* interface for exporting attributes */
struct reset_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	size_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};
#define to_reset_attr(_attr) \
	container_of(_attr, struct reset_attribute, attr)
#define RESET_ATTR(_name, _mode, _show, _store)	\
	static struct reset_attribute reset_attr_##_name = \
			__ATTR(_name, _mode, _show, _store)

/* sysfs related globals */
static ssize_t show_emmc_dload(struct kobject *kobj, struct attribute *attr,
			       char *buf);
static size_t store_emmc_dload(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count);
RESET_ATTR(emmc_dload, 0644, show_emmc_dload, store_emmc_dload);

#ifdef CONFIG_QCOM_MINIDUMP
static ssize_t show_dload_mode(struct kobject *kobj, struct attribute *attr,
			       char *buf);
static size_t store_dload_mode(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count);
RESET_ATTR(dload_mode, 0644, show_dload_mode, store_dload_mode);
#endif /* CONFIG_QCOM_MINIDUMP */

static struct attribute *reset_attrs[] = {
	&reset_attr_emmc_dload.attr,
#ifdef CONFIG_QCOM_MINIDUMP
	&reset_attr_dload_mode.attr,
#endif
	NULL
};

static struct attribute_group reset_attr_group = {
	.attrs = reset_attrs,
};

static int dload_set(const char *val, const struct kernel_param *kp);
module_param_call(download_mode, dload_set, param_get_int,
			&download_mode, 0644);

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
			       char *buf);
static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count);
static const struct sysfs_ops reset_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static struct kobj_type reset_ktype = {
	.sysfs_ops	= &reset_sysfs_ops,
};

static int panic_prep_restart(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	in_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

static void set_dload_mode(int on)
{
	if (dload_mode_addr) {
		__raw_writel(on ? 0xE47B337D : 0, dload_mode_addr);
		__raw_writel(on ? 0xCE14091A : 0,
		       dload_mode_addr + sizeof(unsigned int));
		/* Make sure the download cookie is updated */
		mb();
	}

	qcom_scm_set_download_mode(on ? dload_type : 0,
				   tcsr_boot_misc_detect ? : 0);

	dload_mode_enabled = on;
}

static bool get_dload_mode(void)
{
	return dload_mode_enabled;
}

static void enable_emergency_dload_mode(void)
{
	if (emergency_dload_mode_addr) {
		__raw_writel(EMERGENCY_DLOAD_MAGIC1,
				emergency_dload_mode_addr);
		__raw_writel(EMERGENCY_DLOAD_MAGIC2,
				emergency_dload_mode_addr +
				sizeof(unsigned int));
		__raw_writel(EMERGENCY_DLOAD_MAGIC3,
				emergency_dload_mode_addr +
				(2 * sizeof(unsigned int)));

		/* Need disable the pmic wdt, then the emergency dload mode
		 * will not auto reset.
		 */
		qpnp_pon_wd_config(0);
		/* Make sure all the cookied are flushed to memory */
		mb();
	}

	qcom_scm_set_download_mode(SCM_EDLOAD_MODE, tcsr_boot_misc_detect ?: 0);
}

static int dload_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	int old_val = download_mode;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	/* If download_mode is not zero or one, ignore. */
	if (download_mode >> 1) {
		download_mode = old_val;
		return -EINVAL;
	}

	set_dload_mode(download_mode);

	return 0;
}

static void free_dload_mode_mem(void)
{
	iounmap(emergency_dload_mode_addr);
	iounmap(dload_mode_addr);
}

static void *map_prop_mem(const char *propname)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, propname);
	void *addr;

	if (!np) {
		pr_err("Unable to find DT property: %s\n", propname);
		return NULL;
	}

	addr = of_iomap(np, 0);
	if (!addr)
		pr_err("Unable to map memory for DT property: %s\n", propname);

	return addr;
}

#ifdef CONFIG_RANDOMIZE_BASE
static void *kaslr_imem_addr;
static void store_kaslr_offset(void)
{
	kaslr_imem_addr = map_prop_mem(KASLR_OFFSET_PROP);

	if (kaslr_imem_addr) {
		__raw_writel(0xdead4ead, kaslr_imem_addr);
		__raw_writel(KASLR_OFFSET_BIT_MASK &
		(kimage_vaddr - KIMAGE_VADDR), kaslr_imem_addr + 4);
		__raw_writel(KASLR_OFFSET_BIT_MASK &
			((kimage_vaddr - KIMAGE_VADDR) >> 32),
			kaslr_imem_addr + 8);
		iounmap(kaslr_imem_addr);
	}
}
#else
static void store_kaslr_offset(void)
{
}
#endif /* CONFIG_RANDOMIZE_BASE */

static void setup_dload_mode_support(void)
{
	int ret;

	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);

	dload_mode_addr = map_prop_mem(DL_MODE_PROP);

	emergency_dload_mode_addr = map_prop_mem(EDL_MODE_PROP);

	store_kaslr_offset();

	dload_type_addr = map_prop_mem(IMEM_DL_TYPE_PROP);
	if (!dload_type_addr)
		return;

	ret = kobject_init_and_add(&dload_kobj, &reset_ktype,
			kernel_kobj, "%s", "dload");
	if (ret) {
		pr_err("%s:Error in creation kobject_add\n", __func__);
		kobject_put(&dload_kobj);
		return;
	}

	ret = sysfs_create_group(&dload_kobj, &reset_attr_group);
	if (ret) {
		pr_err("%s:Error in creation sysfs_create_group\n", __func__);
		kobject_del(&dload_kobj);
	}
}

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct reset_attribute *reset_attr = to_reset_attr(attr);
	ssize_t ret = -EIO;

	if (reset_attr->show)
		ret = reset_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct reset_attribute *reset_attr = to_reset_attr(attr);
	ssize_t ret = -EIO;

	if (reset_attr->store)
		ret = reset_attr->store(kobj, attr, buf, count);

	return ret;
}

static ssize_t show_emmc_dload(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	uint32_t read_val, show_val;

	if (!dload_type_addr)
		return -ENODEV;

	read_val = __raw_readl(dload_type_addr);
	if (read_val == EMMC_DLOAD_TYPE)
		show_val = 1;
	else
		show_val = 0;

	return snprintf(buf, sizeof(show_val), "%u\n", show_val);
}

static size_t store_emmc_dload(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	uint32_t enabled;
	int ret;

	if (!dload_type_addr)
		return -ENODEV;

	ret = kstrtouint(buf, 0, &enabled);
	if (ret < 0)
		return ret;

	if (!((enabled == 0) || (enabled == 1)))
		return -EINVAL;

	if (enabled == 1)
		__raw_writel(EMMC_DLOAD_TYPE, dload_type_addr);
	else
		__raw_writel(0, dload_type_addr);

	return count;
}

#ifdef CONFIG_QCOM_MINIDUMP
static DEFINE_MUTEX(tcsr_lock);

static ssize_t show_dload_mode(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "DLOAD dump type: %s\n",
		(dload_type == SCM_DLOAD_BOTHDUMPS) ? "both" :
		((dload_type == SCM_DLOAD_MINIDUMP) ? "mini" : "full"));
}

static size_t store_dload_mode(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	if (sysfs_streq(buf, "full")) {
		dload_type = SCM_DLOAD_FULLDUMP;
	} else if (sysfs_streq(buf, "mini")) {
		if (!msm_minidump_enabled()) {
			pr_err("Minidump is not enabled\n");
			return -ENODEV;
		}
		dload_type = SCM_DLOAD_MINIDUMP;
	} else if (sysfs_streq(buf, "both")) {
		if (!msm_minidump_enabled()) {
			pr_err("Minidump not enabled, setting fulldump only\n");
			dload_type = SCM_DLOAD_FULLDUMP;
			return count;
		}
		dload_type = SCM_DLOAD_BOTHDUMPS;
	} else {
		pr_err("Invalid Dump setup request..\n");
		pr_err("Supported dumps:'full', 'mini', or 'both'\n");
		return -EINVAL;
	}

	mutex_lock(&tcsr_lock);
	/*Overwrite TCSR reg*/
	set_dload_mode(dload_type);
	mutex_unlock(&tcsr_lock);
	return count;
}
#endif /* CONFIG_QCOM_MINIDUMP */

void msm_set_restart_mode(int mode)
{
	restart_mode = mode;
}
EXPORT_SYMBOL(msm_set_restart_mode);


static void msm_restart_prepare(const char *cmd)
{
	bool need_warm_reset = false;
	u8 reason = PON_RESTART_REASON_UNKNOWN;
	/* Write download mode flags if we're panic'ing
	 * Write download mode flags if restart_mode says so
	 * Kill download mode if master-kill switch is set
	 */

	if (cmd != NULL && !strcmp(cmd, "qcom_dload"))
		restart_mode = RESTART_DLOAD;

	set_dload_mode(download_mode &&
			(in_panic || restart_mode == RESTART_DLOAD));

	if (qpnp_pon_check_hard_reset_stored()) {
		/* Set warm reset as true when device is in dload mode */
		if (get_dload_mode() ||
			((cmd != NULL && cmd[0] != '\0') &&
			!strcmp(cmd, "edl")))
			need_warm_reset = true;
	} else {
		need_warm_reset = (get_dload_mode() ||
				(cmd != NULL && cmd[0] != '\0'));
	}

	if (force_warm_reboot)
		pr_info("Forcing a warm reset of the system\n");

	/* Hard reset the PMIC unless memory contents must be maintained. */
	if (force_warm_reboot || need_warm_reset)
		qpnp_pon_system_pwr_off(PON_POWER_OFF_WARM_RESET);
	else
		qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);

	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {
			reason = PON_RESTART_REASON_BOOTLOADER;
			__raw_writel(0x77665500, restart_reason);
		} else if (!strncmp(cmd, "recovery", 8)) {
			reason = PON_RESTART_REASON_RECOVERY;
			__raw_writel(0x77665502, restart_reason);
		} else if (!strcmp(cmd, "rtc")) {
			reason = PON_RESTART_REASON_RTC;
			__raw_writel(0x77665503, restart_reason);
		} else if (!strcmp(cmd, "dm-verity device corrupted")) {
			reason = PON_RESTART_REASON_DMVERITY_CORRUPTED;
			__raw_writel(0x77665508, restart_reason);
		} else if (!strcmp(cmd, "dm-verity enforcing")) {
			reason = PON_RESTART_REASON_DMVERITY_ENFORCE;
			__raw_writel(0x77665509, restart_reason);
		} else if (!strcmp(cmd, "keys clear")) {
			reason = PON_RESTART_REASON_KEYS_CLEAR;
			__raw_writel(0x7766550a, restart_reason);
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			int ret;

			ret = kstrtoul(cmd + 4, 16, &code);
			if (!ret)
				__raw_writel(0x6f656d00 | (code & 0xff),
					     restart_reason);
		} else if (!strncmp(cmd, "edl", 3)) {
			enable_emergency_dload_mode();
		} else {
			__raw_writel(0x77665501, restart_reason);
		}

		if (reason && nvmem_cell)
			nvmem_cell_write(nvmem_cell, &reason, sizeof(reason));
		else
			qpnp_pon_set_restart_reason(
				(enum pon_restart_reason)reason);
	}

	/*outer_flush_all is not supported by 64bit kernel*/
#ifndef CONFIG_ARM64
	outer_flush_all();
#endif

}

/*
 * Deassert PS_HOLD to signal the PMIC that we are ready to power down or reset.
 * Do this by calling into the secure environment, if available, or by directly
 * writing to a hardware register.
 *
 * This function should never return.
 */
static void deassert_ps_hold(void)
{
	qcom_scm_deassert_ps_hold();

	/* Fall-through to the direct write in case the scm_call "returns" */
	__raw_writel(0, msm_ps_hold);
}

static int do_msm_restart(struct notifier_block *unused, unsigned long action,
			   void *arg)
{
	const char *cmd = arg;

	pr_notice("Going down for restart now\n");

	msm_restart_prepare(cmd);

	deassert_ps_hold();

	msleep(10000);

	return NOTIFY_DONE;
}

static void do_msm_poweroff(void)
{
	pr_notice("Powering off the SoC\n");

	set_dload_mode(0);
	qpnp_pon_system_pwr_off(PON_POWER_OFF_SHUTDOWN);

	deassert_ps_hold();

	msleep(10000);
	pr_err("Powering off has failed\n");
}

static int msm_restart_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *mem;
	struct device_node *np;
	int ret = 0;

	nvmem_cell = devm_nvmem_cell_get(dev, "restart_reason");
	if (PTR_ERR(nvmem_cell) == -EPROBE_DEFER)
		return PTR_ERR(nvmem_cell);
	else if (IS_ERR_VALUE(nvmem_cell))
		nvmem_cell = NULL;

	setup_dload_mode_support();

	np = of_find_compatible_node(NULL, NULL,
				"qcom,msm-imem-restart_reason");
	if (!np) {
		pr_err("unable to find DT imem restart reason node\n");
	} else {
		restart_reason = of_iomap(np, 0);
		if (!restart_reason) {
			pr_err("unable to map imem restart reason offset\n");
			ret = -ENOMEM;
			goto err_restart_reason;
		}
	}

	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pshold-base");
	msm_ps_hold = devm_ioremap_resource(dev, mem);
	if (IS_ERR(msm_ps_hold))
		return PTR_ERR(msm_ps_hold);

	mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "tcsr-boot-misc-detect");
	if (mem)
		tcsr_boot_misc_detect = mem->start;

	pm_power_off = do_msm_poweroff;
	restart_nb.notifier_call = do_msm_restart;
	restart_nb.priority = 200;
	register_restart_handler(&restart_nb);

	set_dload_mode(download_mode);
	if (!download_mode)
		qcom_scm_disable_sdi();

	force_warm_reboot = of_property_read_bool(dev->of_node,
						"qcom,force-warm-reboot");

	return 0;

err_restart_reason:
	free_dload_mode_mem();
	return ret;
}

static const struct of_device_id of_msm_restart_match[] = {
	{ .compatible = "qcom,pshold", },
	{},
};
MODULE_DEVICE_TABLE(of, of_msm_restart_match);

static struct platform_driver msm_restart_driver = {
	.probe = msm_restart_probe,
	.driver = {
		.name = "msm-restart",
		.of_match_table = of_match_ptr(of_msm_restart_match),
	},
};

static int __init msm_restart_init(void)
{
	return platform_driver_register(&msm_restart_driver);
}

#if IS_MODULE(CONFIG_POWER_RESET_MSM)
module_init(msm_restart_init);
#else
pure_initcall(msm_restart_init);
#endif

static __exit void msm_restart_exit(void)
{
	platform_driver_unregister(&msm_restart_driver);
}
module_exit(msm_restart_exit);

MODULE_DESCRIPTION("MSM Poweroff Driver");
MODULE_LICENSE("GPL v2");
