/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *qm215_dt_match[] __initconst = {
	"qcom,qm215",
	"qcom,qcm2150",
	NULL
};

static void __init qm215_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(QM215_DT,
	"Qualcomm Technologies, Inc. QM215")
	.init_machine		= qm215_init,
	.dt_compat		= qm215_dt_match,
MACHINE_END
