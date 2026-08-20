// SPDX-License-Identifier: GPL-2.0-only

#ifndef __MAX17332_CURRENT_OVERRIDE_H_
#define __MAX17332_CURRENT_OVERRIDE_H_

#ifdef CONFIG_MAX17332_CURRENT_OVERRIDE

#include <linux/extcon.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>

#define MAX17332_RECFG_CHG_CURR_NULL_VALUE 0xffff

struct max17332_charger_data;

/* low/high charging current for different current capacity */
struct max17332_charger_current_config {
	u16 ini_ver;
	u16 nstep_curr_low;
	u16 nichg_cfg1_low;
	u16 nichg_cfg2_low;
	u16 nstep_curr_high;
	u16 nichg_cfg1_high;
	u16 nichg_cfg2_high;
};

struct max17332_charger_current_override {
	struct max17332_charger_current_config config;
	bool has_extcon;
	struct extcon_dev *extcon_dev;
	struct notifier_block nb;
	struct delayed_work notify_work;
};

int max17332_init_charging_current_override(struct max17332_charger_data *charger);
void max17332_deinit_charging_current_override(struct max17332_charger_data *charger);

#else

struct max17332_charger_data;

int max17332_init_charging_current_override(struct max17332_charger_data *charger)
{
    return 0;
}

void max17332_deinit_charging_current_override(struct max17332_charger_data *charger)
{
}

#endif // CONFIG_MAX17332_CURRENT_OVERRIDE

#endif //__MAX17332_CURRENT_OVERRIDE_H_
