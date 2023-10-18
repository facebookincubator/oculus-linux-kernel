/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved. */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>

enum vdd_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN_SVS */
	VDD_LOWER,		/* LOW_SVS / SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVS_L1 */
	VDD_NOMINAL,		/* NOM */
	VDD_NOMINAL_L1,		/* NOM L1 */
	VDD_HIGH,		/* TURBO */
	VDD_HIGH_L1,		/* TURBO_L1 */
	VDD_NUM,
};

static int vdd_corner[] = {
	[VDD_NONE]    = 0,
	[VDD_MIN]     = RPMH_REGULATOR_LEVEL_MIN_SVS,
	[VDD_LOWER]   = RPMH_REGULATOR_LEVEL_LOW_SVS,
	[VDD_LOW]     = RPMH_REGULATOR_LEVEL_SVS,
	[VDD_LOW_L1]  = RPMH_REGULATOR_LEVEL_SVS_L1,
	[VDD_NOMINAL] = RPMH_REGULATOR_LEVEL_NOM,
	[VDD_NOMINAL_L1] = RPMH_REGULATOR_LEVEL_NOM_L1,
	[VDD_HIGH]    = RPMH_REGULATOR_LEVEL_TURBO,
	[VDD_HIGH_L1]    = RPMH_REGULATOR_LEVEL_TURBO_L1,
};

#endif
