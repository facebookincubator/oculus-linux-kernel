/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef __SOC_QCOM_LPM_LEVEL_H__
#define __SOC_QCOM_LPM_LEVEL_H__

struct system_pm_ops {
	int (*enter)(struct cpumask *mask);
	void (*exit)(bool success);
	int (*update_wakeup)(bool b);
	bool (*sleep_allowed)(void);
};

#ifdef CONFIG_MSM_PM
uint32_t register_system_pm_ops(struct system_pm_ops *pm_ops);
#else
static inline uint32_t register_system_pm_ops(struct system_pm_ops *pm_ops)
{ return -ENODEV; }
#endif

#endif
