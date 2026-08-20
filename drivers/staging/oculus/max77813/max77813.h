/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef __MAX77813_H
#define __MAX77813_H

/* MAX77813 Registers */
enum {
	MAX77813_REG_DEVICE_ID,
	MAX77813_REG_STATUS,
	MAX77813_REG_CONFIG1,
	MAX77813_REG_CONFIG2,
	MAX77813_REG_VOUT,
};

#define MAX77813_MASK_CHIP_REV		(0x7 << 0)
#define MAX77813_MASK_VERSION		(0xf << 3)

#define MAX77813_MASK_ST		(0xf)
#define MAX77813_MASK_ST_OCP		(0x1 << 0)
#define MAX77813_MASK_ST_OVP		(0x1 << 1)
#define MAX77813_MASK_ST_POK		(0x1 << 2)
#define MAX77813_MASK_ST_TSHDN		(0x1 << 3)

#define MAX77813_MASK_BB_EN		(0x1 << 6)
#define MAX77813_MASK_PD_EN		(0x1 << 5)
#define MAX77813_MASK_POK_POL		(0x1 << 4)

#define MAX77813_MASK_FPWM		(0x1 << 0)
#define MAX77813_MASK_AD		(0x1 << 0)
#define MAX77813_MASK_OVP_TH		(0x3 << 2)
#define MAX77813_MASK_RD_SR		(0x1 << 4)
#define MAX77813_MASK_RU_SR		(0x1 << 5)

#define MAX77813_MASK_VOUT		(0x7f)

#define MAX77813_VOUT_MIN_UV		2600000
#define MAX77813_VOUT_MAX_UV		4580000
#define MAX77813_VOUT_STEP_UV		20000

#define MAX77813_AD_DISABLE		0

#endif //__MAX77813_H
