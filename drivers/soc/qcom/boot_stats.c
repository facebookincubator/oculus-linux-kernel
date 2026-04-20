// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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
#include <linux/ktime.h>
#include <linux/boot_stats.h>

static int __init bootstatsdev_init(void);

struct boot_stats {
	uint32_t bootloader_start;
	uint32_t bootloader_end;
	uint32_t bootloader_display;
	uint32_t bootloader_load_kernel;
	/* The qcom,msm-imem-boot_stats region is 32 bytes. So there is
	 * room for the first stage kernel to record events to be
	 * consumed by the second stage kernel.
	 */
	uint32_t kernel1[HIBEVENT_KERN1_MAX];
};

static void __iomem *mpm_counter_base;
static uint32_t mpm_counter_freq;
static struct boot_stats __iomem *boot_stats;

//Creating the struct which will store the bootloader statistics
struct bootstat_data {
	struct device bootstat_dev;
	struct boot_stats saved_boot_stats;
	uint32_t boot_completed_since_kernel; /* sys.boot_complete since kernel start */
	uint32_t boot_completed_since_reset; /* sys.boot_complete since power-up or reboot */
	struct {
		/*
		 * If the hibernation data below is valid.
		 */
		bool valid;
		/* Booloader and first-stage-kernel stats from the boot
		 * on hibernation wakeup. */
		struct boot_stats boot_stats;
		/*
		 * Stats from first and second stage kernels, as MPM counter values.
		 * MPM counter would overflow much late after boot:
		 *       2^32 / 2^15 = 131072 seconds
		 */
		uint32_t kernel2[HIBEVENT_KERN2_MAX];
	} hibernation;
};
static struct bootstat_data bootstat_data;

void bootstat_reset_hibernation_stats(void)
{
	/* This flag is a safeguard to ensure that
	 * the bootloader MPM counter values we obtain
	 * will correspond to the kernel timestamps we collect during
	 * hibernation resume.
	 *
	 * It's a safeguard.
	 */
	memset(&bootstat_data.hibernation, 0, sizeof(bootstat_data.hibernation));
	memset(boot_stats, 0, sizeof(*boot_stats));
	bootstat_data.hibernation.valid = false;
}

void bootstat_record_kernel1_event(const enum hibernation_kernel1_event e)
{
	pr_debug("hibernation_kern1_stats: %d\n", e);

	if (e >= HIBEVENT_KERN1_MAX)
		return;

	/* Events from first stage kernel must be stored in IMEM.
	 * Once second stage kernel boots, it would read from there. */
	boot_stats->kernel1[e] = readl_relaxed(mpm_counter_base);

	if (e == HIBEVENT_KERN1_HIBERNATION_RESTORE_FAILED) {
		/* Final event recorded. There will be no second stage
		 * kernel, so record the failure here. */
		memset(&bootstat_data.hibernation, 0, sizeof(bootstat_data.hibernation));
		bootstat_data.hibernation.boot_stats = *boot_stats;
		smp_wmb();
		bootstat_data.hibernation.valid = true;
	}
}

void bootstat_record_kernel2_event(const enum hibernation_kernel2_event e)
{
	pr_debug("hibernation_kern2_stats: %d\n", e);

	if (e >= HIBEVENT_KERN2_MAX)
		return;

	if (e == HIBEVENT_KERN2_ARCH_RESUME) {
		/* On first entry, save the bootloader and
		 * the first-stage-kernel timestamps. */
		memset(&bootstat_data.hibernation, 0, sizeof(bootstat_data.hibernation));
		bootstat_data.hibernation.boot_stats = *boot_stats;
	}
	bootstat_data.hibernation.kernel2[e] = readl_relaxed(mpm_counter_base);

	if (e == HIBEVENT_KERN2_HIBERNATION_EXIT) {
		/* If second-stage has been successfully reached,
		 * then obviously first-stage kernel did not fail.
		 * Wipe the random value left in IMEM then. */
		bootstat_data.hibernation.boot_stats.kernel1[HIBEVENT_KERN1_HIBERNATION_RESTORE_FAILED] = 0;
		/* Final event recorded. Mark data as valid. */
		smp_wmb();
		bootstat_data.hibernation.valid = true;
	}
}

/*
 * Validate the IMEM DT node (e.g. if it is large enough
 * to accommodate all the counters we're using).
 */
static int mpm_validate_imem_node(struct device_node *np_imem)
{
	struct resource r;
	int st;

	st = of_address_to_resource(np_imem, 0, &r);
	if (st)
		return st;
	if (sizeof(*boot_stats) > resource_size(&r))
		return -EINVAL;

	return 0;
}

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
	if (mpm_validate_imem_node(np_imem)) {
		pr_err("qcom,msm-imem node is not compatible\n");
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

static int __init boot_stats_init(void)
{
	int ret;

	ret = mpm_parse_dt();
	if (ret < 0)
		return -ENODEV;

	print_boot_stats();

	bootstatsdev_init();
	bootstat_data.saved_boot_stats = *boot_stats;

	/*
	 * Do not unmap boot_stats and mpm_counter_base. They will
	 * be used in the hibernation resume paths.
	 */

	return 0;
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
			       bootstat_data.saved_boot_stats.bootloader_start *
				       1000 / mpm_counter_freq,
			       bootstat_data.saved_boot_stats.bootloader_end *
				       1000 / mpm_counter_freq,
			       mpm_counter_freq,
			       bootstat_data.boot_completed_since_reset,
			       bootstat_data.boot_completed_since_kernel);
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
		// struct timespec ts;
		u64 ts_ns = ktime_get_ns();
		u32 timestamp_ms;
		u32 kern_ms;

		// getrawmonotonic(&ts);
		// timestamp_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
		timestamp_ms = ts_ns / 1000000;
		kern_ms = bootstat_data.saved_boot_stats.bootloader_end * 1000 / mpm_counter_freq;
		bootstat_data.boot_completed_since_kernel = timestamp_ms;
		bootstat_data.boot_completed_since_reset = timestamp_ms + kern_ms;
		dev_dbg(dev, "boot_completed_since_reset=(%u+%u)=%u\n",
			timestamp_ms, kern_ms, bootstat_data.boot_completed_since_reset);
	}
	return count;
}

//Not sure what this definition does, I could not find a definition for __ATTR, I am not completely sure about the 0644
static struct device_attribute bootstats_attr =
	__ATTR(bootstats, 0644, bootstat_get, bootstat_set);


static inline unsigned int mpm2ms(uint32_t mpm_val)
{
	return ((uint64_t)mpm_val * 1000ULL) / mpm_counter_freq;
}

static ssize_t hibernation_stats_get(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	ssize_t ret = 0;
	const uint32_t *saved_kernel2 = &bootstat_data.hibernation.kernel2[0];
	const struct boot_stats *saved_boot_stats = &bootstat_data.hibernation.boot_stats;

	if (!bootstat_data.hibernation.valid)
		return snprintf(buf, PAGE_SIZE, "no valid hibernation statistics found\n");

	ret = snprintf(buf, PAGE_SIZE,
		       "bootloader_start %u\n"
		       "bootloader_end %u\n"
		       "kernel1_load_mem_from_disk_started %u\n"
		       "kernel1_load_mem_from_disk_finished %u\n"
		       "kernel1_hibernation_restore_failed %u\n"
		       "kernel2_arch_resume %u\n"
		       "kernel2_start_device_resume %u\n"
		       "kernel2_end_device_resume %u\n"
		       "kernel2_image_restored %u\n"
		       "kernel2_hibernation_process_thaw_done %u\n"
		       "kernel2_hibernation_exit %u\n",
		       mpm2ms(saved_boot_stats->bootloader_start),
		       mpm2ms(saved_boot_stats->bootloader_end),
		       mpm2ms(saved_boot_stats->kernel1[HIBEVENT_KERN1_LOAD_MEM_FROM_DISK_STARTED]),
		       mpm2ms(saved_boot_stats->kernel1[HIBEVENT_KERN1_LOAD_MEM_FROM_DISK_FINISHED]),
		       mpm2ms(saved_boot_stats->kernel1[HIBEVENT_KERN1_HIBERNATION_RESTORE_FAILED]),
		       mpm2ms(saved_kernel2[HIBEVENT_KERN2_ARCH_RESUME]),
		       mpm2ms(saved_kernel2[HIBEVENT_KERN2_START_DEVICE_RESUME]),
		       mpm2ms(saved_kernel2[HIBEVENT_KERN2_END_DEVICE_RESUME]),
		       mpm2ms(saved_kernel2[HIBEVENT_KERN2_IMAGE_RESTORED]),
		       mpm2ms(saved_kernel2[HIBEVENT_KERN2_HIBERNATION_PROCESSES_THAW_DONE]),
		       mpm2ms(saved_kernel2[HIBEVENT_KERN2_HIBERNATION_EXIT]));

	return ret;
}

static ssize_t hibernation_stats_set(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	dev_dbg(dev, "resetting hibernation stats");

	if (*buf) {
		/* Invalidate the hibernation stats. */
		bootstat_reset_hibernation_stats();
	}
	return count;
}

static struct device_attribute hibernation_stats_attr =
	__ATTR(hibernation_stats, 0644, hibernation_stats_get, hibernation_stats_set);


static int __init bootstatsdev_init(void)
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
	if (device_create_file(&bootstat_data.bootstat_dev, &hibernation_stats_attr))
		dev_err(&bootstat_data.bootstat_dev, "unable to create file\n");

err:
	return rc;
}

module_init(boot_stats_init);

static void __exit boot_stats_exit(void)
{
}
module_exit(boot_stats_exit)

MODULE_DESCRIPTION("MSM boot stats info driver");
MODULE_LICENSE("GPL v2");
