/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_9650_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_9650_H

#include <linux/clk/msm-clock-generic.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/consumer.h>

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
	},					\
	.num_fmax = VDD_DIG_NUM
#define VDD_DIG_FMAX_MAP4(l1, f1, l2, f2, l3, f3, l4, f4) \
	.vdd_class = &vdd_dig,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
		[VDD_DIG_##l3] = (f3),		\
		[VDD_DIG_##l4] = (f4),		\
	},					\
	.num_fmax = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2_AO(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig_ao,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM

#define VDD_GPU_PLL_FMAX_MAP2(l1, f1, l2, f2)	\
	.vdd_class = &vdd_gpucc_mx,		\
	.fmax = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),		\
		[VDD_DIG_##l2] = (f2),		\
	},					\
	.num_fmax = VDD_DIG_NUM

#define VDD_GPU_PLL_FMAX_MAP3(l1, f1, l2, f2, l3, f3)	\
	.vdd_class = &vdd_gpucc_mx,			\
	.fmax = (unsigned long[VDD_DIG_NUM]) {		\
		[VDD_DIG_##l1] = (f1),			\
		[VDD_DIG_##l2] = (f2),			\
		[VDD_DIG_##l3] = (f3),			\
	},						\
	.num_fmax = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_MIN,		/* MIN SVS */
	VDD_DIG_LOWER,		/* SVS2 */
	VDD_DIG_LOW,		/* SVS */
	VDD_DIG_NOMINAL,	/* NOM */
	VDD_DIG_HIGH,		/* TURBO */
	VDD_DIG_NUM
};

static int vdd_corner[] = {
	RPM_REGULATOR_LEVEL_NONE,		/* VDD_DIG_NONE */
	RPM_REGULATOR_LEVEL_MIN_SVS,		/* VDD_DIG_MIN */
	RPM_REGULATOR_LEVEL_LOW_SVS,		/* VDD_DIG_LOWER */
	RPM_REGULATOR_LEVEL_SVS,		/* VDD_DIG_LOW */
	RPM_REGULATOR_LEVEL_NOM,		/* VDD_DIG_NOMINAL */
	RPM_REGULATOR_LEVEL_TURBO,		/* VDD_DIG_HIGH */
};

#endif
