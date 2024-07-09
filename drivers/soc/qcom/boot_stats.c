// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>

struct boot_stats {
	uint32_t bootloader_start;
	uint32_t bootloader_end;
	uint32_t bootloader_display;
	uint32_t bootloader_load_kernel;
};

static void __iomem *mpm_counter_base;
static uint32_t mpm_counter_freq;
static struct boot_stats __iomem *boot_stats;

struct bootstat_data {
	struct device bootstat_dev;
	struct boot_stats saved_boot_stats;
	uint32_t boot_completed_since_kernel; /* sys.boot_complete since kernel start */
	uint32_t boot_completed_since_reset; /* sys.boot_complete since power-up or reboot */
};
static struct bootstat_data bootstat_data;

static int mpm_parse_dt(void)
{
	struct device_node *np_imem, *np_mpm2;

	np_imem = of_find_compatible_node(NULL, NULL,
				"qcom,msm-imem-boot_stats");
	if (!np_imem) {
		pr_err("can't find qcom,msm-imem node\n");
		return -ENODEV;
	}
	boot_stats = of_iomap(np_imem, 0);
	if (!boot_stats) {
		pr_err("boot_stats: Can't map imem\n");
		goto err1;
	}

	np_mpm2 = of_find_compatible_node(NULL, NULL,
				"qcom,mpm2-sleep-counter");
	if (!np_mpm2) {
		pr_err("mpm_counter: can't find DT node\n");
		goto err1;
	}

	if (of_property_read_u32(np_mpm2, "clock-frequency", &mpm_counter_freq))
		goto err2;

	if (of_get_address(np_mpm2, 0, NULL, NULL)) {
		mpm_counter_base = of_iomap(np_mpm2, 0);
		if (!mpm_counter_base) {
			pr_err("mpm_counter: cant map counter base\n");
			goto err2;
		}
	} else
		goto err2;

	return 0;

err2:
	of_node_put(np_mpm2);
err1:
	of_node_put(np_imem);
	return -ENODEV;
}

static void print_boot_stats(void)
{
	pr_info("KPI: Bootloader start count = %u\n",
		readl_relaxed(&boot_stats->bootloader_start));
	pr_info("KPI: Bootloader end count = %u\n",
		readl_relaxed(&boot_stats->bootloader_end));
	pr_info("KPI: Bootloader display count = %u\n",
		readl_relaxed(&boot_stats->bootloader_display));
	pr_info("KPI: Bootloader load kernel count = %u\n",
		readl_relaxed(&boot_stats->bootloader_load_kernel));
	pr_info("KPI: Kernel MPM timestamp = %u\n",
		readl_relaxed(mpm_counter_base));
	pr_info("KPI: Kernel MPM Clock frequency = %u\n",
		mpm_counter_freq);
}

static ssize_t bootstat_get(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	ssize_t ret = 0;

	if (mpm_counter_freq > 0) {
		/**
		 * the structure is passed from bootloader and needs to be consistent with
		 * bootable/bootloader/edk2/QcomModulePkg/Include/Library/BootStats.h
		 **/
		ret = snprintf(buf, PAGE_SIZE,
			"bootloader_start %u\n"
			"bootloader_end %u\n"
			"mpm_counter_freq %u\n"
			"boot_completed_ms %u\n"
			"boot_completed_kernel_ms %u\n",
			bootstat_data.saved_boot_stats.bootloader_start * 1000 / mpm_counter_freq,
			bootstat_data.saved_boot_stats.bootloader_end * 1000 / mpm_counter_freq,
			mpm_counter_freq,
			bootstat_data.boot_completed_since_reset,
			bootstat_data.boot_completed_since_kernel
		);
	}
	return ret;
}

/**
 * In init.xxx.rc, on property:sys.boot_completed=1,
 * write anything except empty string to mark boot completion
 */
static ssize_t bootstat_set(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	/* only access from init is accepted, (in init-forked child process) */
	if (strcmp(current->comm, "init") == 0 && *buf) {
		struct timespec ts;
		u32 timestamp_ms;
		u32 kern_ms;

		getrawmonotonic(&ts);
		timestamp_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
		kern_ms = bootstat_data.saved_boot_stats.bootloader_end * 1000 / mpm_counter_freq;
		bootstat_data.boot_completed_since_kernel = timestamp_ms;
		bootstat_data.boot_completed_since_reset = timestamp_ms + kern_ms;
		dev_dbg(dev, "boot_completed_since_reset=(%u+%u)=%u\n",
			timestamp_ms, kern_ms, bootstat_data.boot_completed_since_reset);
	}
	return count;
}

static struct device_attribute bootstats_attr =
	__ATTR(bootstats, 0644, bootstat_get, bootstat_set);

static int bootstatsdev_init(void)
{
	int rc = 0;

	dev_set_name(&bootstat_data.bootstat_dev, "bootstatdev");
	rc = device_register(&bootstat_data.bootstat_dev);
	if (rc) {
		pr_err("%s: driver_register failed\n", __func__);
		goto err;
	}
	dev_dbg(&bootstat_data.bootstat_dev, "device register successful\n");

	if (device_create_file(&bootstat_data.bootstat_dev, &bootstats_attr))
		dev_err(&bootstat_data.bootstat_dev, "unable to create file\n");

err:
	return rc;
}

int boot_stats_init(void)
{
	int ret;

	ret = mpm_parse_dt();
	if (ret < 0)
		return -ENODEV;

	print_boot_stats();

	bootstatsdev_init();
	bootstat_data.saved_boot_stats = *boot_stats;
	iounmap(boot_stats);
	iounmap(mpm_counter_base);

	return 0;
}

