/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/**
 * DOC: This file contains centralized definitions of CP Stats component
 */
#ifndef __CONFIG_CP_STATS_H
#define __CONFIG_CP_STATS_H

#include "cfg_define.h"

#ifdef WLAN_CHIPSET_STATS
/*
 * <ini>
 * chipset_stats_enable - Enable/Disable chipset stats logging feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable Chipset Stats Logging. Configurations
 * are as follows:
 * 0 - Disable Chipset Stats logging
 * 1 - Enable Chipset Stats logging
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CHIPSET_STATS_ENABLE CFG_INI_BOOL(\
		"chipset_stats_enable", false,\
		"Enable Chipset stats logging feature")

#define CFG_CP_STATS_CSTATS CFG(CHIPSET_STATS_ENABLE)
#else
#define CFG_CP_STATS_CSTATS
#endif /* WLAN_CHIPSET_STATS */

#define CFG_CP_STATS_ALL \
	CFG_CP_STATS_CSTATS

#endif /* __CONFIG_CP_STATS_H */
