/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _BATTERY_CHARGER_H
#define _BATTERY_CHARGER_H

#include <linux/notifier.h>

enum battery_charger_prop {
	BATTERY_RESISTANCE,
	FLASH_ACTIVE,
	FCT,
	FCT_STATE,
	BATTERY_CHARGER_PROP_MAX,
};

/* Matches ADSP side */
enum oem_property_id {
	BATTMAN_OEM_PROPERTY_1,
	BATTMAN_OEM_FCT_LIFETIMES,
	BATTMAN_OEM_PROPERTY_MAX,
};

enum bc_hboost_event {
	VMAX_CLAMP,
};

#if IS_ENABLED(CONFIG_QTI_BATTERY_CHARGER)
int qti_battery_charger_get_prop(const char *name,
				enum battery_charger_prop prop_id, int *val);
int qti_battery_charger_set_prop(const char *name,
				enum battery_charger_prop prop_id, int val);
int register_hboost_event_notifier(struct notifier_block *nb);
int unregister_hboost_event_notifier(struct notifier_block *nb);
int qti_battery_charger_set_buffer(const char *name,
				enum oem_property_id prop_id, u32 *data, u32 size);
#else
static inline int
qti_battery_charger_get_prop(const char *name,
				enum battery_charger_prop prop_id, int *val)
{
	return -EINVAL;
}

static inline int
qti_battery_charger_set_prop(const char *name,
				enum battery_charger_prop prop_id, int val)
{
	return -EINVAL;
}

static inline int register_hboost_event_notifier(struct notifier_block *nb)
{
	return -EOPNOTSUPP;
}

static inline int unregister_hboost_event_notifier(struct notifier_block *nb)
{
	return -EOPNOTSUPP;
}
#endif

#endif
