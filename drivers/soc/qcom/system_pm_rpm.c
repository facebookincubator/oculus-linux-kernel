// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/tick.h>
#include <clocksource/arm_arch_timer.h>
#include <linux/ktime.h>
#include <asm/arch_timer.h>
#include <soc/qcom/rpm-notifier.h>
#include <soc/qcom/lpm_levels.h>
#include <soc/qcom/mpm.h>

static void system_sleep_exit(bool success)
{
	msm_rpm_exit_sleep();
}

static int system_sleep_update_wakeup(bool from_idle)
{
	uint32_t lo = ~0U, hi = ~0U;
	u64 wakeup;

	/* Read the hardware to get the most accurate value */
	arch_timer_mem_get_cval(&lo, &hi);
	wakeup = lo;
	wakeup |= ((uint64_t)(hi) << 32);
	msm_mpm_timer_write((uint32_t *)&wakeup);

	return 0;
}

static int system_sleep_enter(struct cpumask *mask)
{
	int ret = 0;

	ret = msm_rpm_enter_sleep(0, mask);
	if (ret)
		return ret;

	msm_mpm_enter_sleep(mask);

	return ret;
}

static bool system_sleep_allowed(void)
{
	return !msm_rpm_waiting_for_ack();
}

static struct system_pm_ops pm_ops = {
	.enter = system_sleep_enter,
	.exit = system_sleep_exit,
	.update_wakeup = system_sleep_update_wakeup,
	.sleep_allowed = system_sleep_allowed,
};

static int sys_pm_rpm_probe(struct platform_device *pdev)
{
	return register_system_pm_ops(&pm_ops);
}

static const struct of_device_id sys_pm_drv_match[] = {
	{ .compatible = "qcom,system-pm-rpm", },
	{ }
};

static struct platform_driver sys_pm_rpm_driver = {
	.probe = sys_pm_rpm_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.suppress_bind_attrs = true,
		.of_match_table = sys_pm_drv_match,
	},
};
module_platform_driver(sys_pm_rpm_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. (QTI) SYSTEM PM RPM Driver");
MODULE_LICENSE("GPL v2");
