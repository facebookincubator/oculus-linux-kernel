// SPDX-License-Identifier: GPL-2.0-only

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "max17332-current-override.h"
#include "max17332.h"
#include "max17332-charger.h"

#define DELAY_WORK_MS  (500)
#define EXTCON_NOTIFY_WORK_DELAY_MS (3500)

static void reconfig_chg_curr(struct max17332_charger_data *charger, bool set_high_current)
{
	int ret = 0;
	int nichg_cfg1, nichg_cfg2, nstep_curr;

	if (set_high_current) {
		nichg_cfg1 = charger->current_override.config.nichg_cfg1_high;
		nichg_cfg2 = charger->current_override.config.nichg_cfg2_high;
		nstep_curr = charger->current_override.config.nstep_curr_high;
	} else {
		nichg_cfg1 = charger->current_override.config.nichg_cfg1_low;
		nichg_cfg2 = charger->current_override.config.nichg_cfg2_low;
		nstep_curr = charger->current_override.config.nstep_curr_low;
	}

	if (nstep_curr != MAX17332_RECFG_CHG_CURR_NULL_VALUE &&
		nichg_cfg1 != MAX17332_RECFG_CHG_CURR_NULL_VALUE &&
		nichg_cfg2 != MAX17332_RECFG_CHG_CURR_NULL_VALUE) {
		/* If battery pack is MC-1072292_02 or MC-1085405_01
		 * nStepCurr, nIChgCfg1 and nIChgCfg1 need to be modified */
		ret = max17332_write_unlock(charger->max17332, charger->regmap_nvm,
				REG_NSTEPCURR_NVM, nstep_curr);
		ret = max17332_write_unlock(charger->max17332, charger->regmap_nvm,
				REG_NICHGCFG1_NVM, nichg_cfg1);
		ret = max17332_write_unlock(charger->max17332, charger->regmap_nvm,
				REG_NICHGCFG2_NVM, nichg_cfg2);
	} else {
		/* If battery pack is MC-1015759-02 or 890-0337_04
		 * nIChgCfg1 and nIChgCfg1 need to be modified */
		ret = max17332_write_unlock(charger->max17332, charger->regmap_nvm,
			REG_NICHGCFG1_NVM, nichg_cfg1);
		ret = max17332_write_unlock(charger->max17332, charger->regmap_nvm,
			REG_NICHGCFG2_NVM, nichg_cfg2);
	}
	if (ret < 0)
		pr_err("%s : fail to reconfig charging current\n", __func__);

	return;
}

static void notify_work(struct work_struct *work)
{
	int ret;
	union power_supply_propval prop;
	struct max17332_charger_data *charger =
		container_of(work, struct max17332_charger_data, current_override.notify_work.work);

	if (!charger->current_override.has_extcon) {
		return;
	}
    if (charger->current_override.extcon_dev == NULL) {
            struct extcon_dev *extcon_dev = extcon_get_edev_by_phandle(charger->dev, 0);
            if (IS_ERR_OR_NULL(extcon_dev)) {
                    dev_err(charger->dev, "%s: Failed to get extcon\n", __func__);
                    return;
            }
			charger->current_override.extcon_dev = extcon_dev;
    }

	ret = power_supply_get_property(charger->usb_charger, POWER_SUPPLY_PROP_ONLINE, &prop);
	if (!ret && prop.intval) {
		/**
		 * if USB is connected, check host current capability
		 * then update charging current
		 */
		dev_dbg(charger->dev, "%s: USB charger is connected\n", __func__);
		if (extcon_get_state(charger->current_override.extcon_dev, EXTCON_CHG_USB_FAST) &&
			charger->current_override.config.ini_ver != 0) {
			reconfig_chg_curr(charger, true);
		} else if (charger->current_override.config.ini_ver != 0) {
			reconfig_chg_curr(charger, false);
		}
	} else {
		/* if USB is disconnected, set charging current to low */
		if (charger->current_override.config.ini_ver != 0) {
			reconfig_chg_curr(charger, false);
		}
	}
}

static int max17332_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr)
{
	struct power_supply *psy = ptr;
	struct max17332_charger_data *charger = NULL;

	if (IS_ERR_OR_NULL(nb))
		return NOTIFY_BAD;

	charger = container_of(nb, struct max17332_charger_data, current_override.nb);
	if (IS_ERR_OR_NULL(charger) || IS_ERR_OR_NULL(charger->usb_charger)) {
		dev_err(charger->dev, "PSY notifier provided object handle is invalid\n");
		return NOTIFY_BAD;
	}

	if (psy != charger->usb_charger || ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_DONE;

	/**
	 * if we have extcon, schedule it, the notify work adjust charging
	 * current follow extcon type. We need to wait for extcon type updating
	 * finished, then check extcon type, so add a delay.
	 */
	if (charger->current_override.extcon_dev) {
		/**
		 * Cancel delay work before continuous usb plug\unplug trigger
		 */
		cancel_delayed_work_sync(&charger->current_override.notify_work);
		schedule_delayed_work(&charger->current_override.notify_work,
				msecs_to_jiffies(EXTCON_NOTIFY_WORK_DELAY_MS));
	}

	return NOTIFY_OK;
}

int max17332_init_charging_current_override(struct max17332_charger_data *charger)
{
	u16 val;
	struct device_node *max17332_node;
	struct max17332_charger_current_config *current_configs;
	int ini_rev, i, ret, config_len, size = 0;
	const void *prop;

	max17332_node = of_find_node_by_name(NULL, "max17332");
	if (max17332_node == NULL) {
		pr_err("%s max17332 node NULL\n", __func__);
		return -EINVAL;
	}

	charger->dev->of_node = of_find_node_by_name(max17332_node, "charger");
	if (charger->dev->of_node == NULL) {
		pr_err("%s charger node NULL\n", __func__);
		return -EINVAL;
	}

	ret = max17332_read(charger->regmap_nvm, REG_NRSENSE_NVM, &val);
	if (ret < 0) {
		pr_err("%s : Failed to read REG_NRSENSE_NVM\n", __func__);
		return -EINVAL;
	}
	ini_rev = (MAX17332_INI_REV_REG & val) >> MAX17332_NRSENSE_USERMEMORY_POS;

	prop = of_get_property(charger->dev->of_node, "charger,charging-current-override", &size);
	if (!prop) {
		pr_err("%s charging current override not found\n", __func__);
		return -EINVAL;
	}
	if (!size) {
		pr_err("%s charging current override NULL\n", __func__);
		return -EINVAL;
	}

	current_configs = devm_kzalloc(charger->dev, size, GFP_KERNEL);
	if (!current_configs)
		return -ENOMEM;

	ret = of_property_read_u16_array(charger->dev->of_node, "charger,charging-current-override",
			(u16 *)current_configs,
			size / sizeof(u16));
	if (ret) {
		devm_kfree(charger->dev, current_configs);
		pr_err("%s read current override failed\n", __func__);
		return -EINVAL;
	}

	config_len = size / sizeof(struct max17332_charger_current_config);
	for (i = 0; i < config_len; i++) {
		if (current_configs[i].ini_ver == ini_rev) {
			memcpy(&charger->current_override.config,
				&current_configs[i],
				sizeof(struct max17332_charger_current_config));
			break;
		}
	}
	devm_kfree(charger->dev, current_configs);

	if (i == config_len) {
		pr_warn("%s no matched ini version\n", __func__);
		return 0;
	}
	pr_info("%s: version: %u\n", __func__, charger->current_override.config.ini_ver);

	charger->current_override.extcon_dev = NULL;
	charger->current_override.has_extcon = of_property_read_bool(charger->dev->of_node, "extcon");
	if (!charger->current_override.has_extcon) {
		pr_warn("%s: extcon not exist\n", __func__);
		return 0;
	}

	INIT_DELAYED_WORK(&charger->current_override.notify_work, notify_work);

	charger->current_override.nb.notifier_call = max17332_psy_notifier_call;
	ret = power_supply_reg_notifier(&charger->current_override.nb);
	if (ret) {
		pr_err("%s : Failed to register psy notifier\n", __func__);
		return -EINVAL;
	}

	/* query usb status and update charging current. Need a delay to wait for extcon ready. */
	schedule_delayed_work(&charger->current_override.notify_work, msecs_to_jiffies(DELAY_WORK_MS));

	return 0;
}

void max17332_deinit_charging_current_override(struct max17332_charger_data *charger)
{
	if (charger->current_override.config.ini_ver != 0) {
		reconfig_chg_curr(charger, false);
	}

	if (charger->current_override.extcon_dev) {
		cancel_delayed_work_sync(&charger->current_override.notify_work);
		power_supply_unreg_notifier(&charger->current_override.nb);
	}
}
