// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>

#include "max17332.h"
#include "max17332-battery.h"
#include "max17332-voltage-adjustment.h"

#define VPCKP_READ_TIMES	10
#define PCKP_RAW_TO_UV(v)	((v * 625) / 2)  /* 312.5 uV per bit */
#define VSYS_CHANGE_STEP	50000	/* uV */
#define CURRENT_AVG_SCALING	7
#define VOLTAGE_ADJUST_INITIAL_VAL	3800000	/* uV */
#define VSYS_DEFAULT		4200000 /* uV */

static int max17332_calculate_vpckp_avg(struct max17332_dev *pdev)
{
	u32 vpckp, max_pckp, min_pckp, sum = 0;
	u16 value;
	int i;
	int ret;

	for (i = 0; i < VPCKP_READ_TIMES; i++) {
		ret = max17332_read(pdev->regmap_pmic, REG_PCKP, &value);
		if (ret < 0) {
			dev_err(pdev->dev, "%s: Failed to read REG_PCKP\n", __func__);
			return ret;
		}
		vpckp = PCKP_RAW_TO_UV(value);
		if (i == 0) {
			max_pckp = vpckp;
			min_pckp = vpckp;
		} else {
			if (vpckp > max_pckp)
				max_pckp = vpckp;
			if (vpckp < min_pckp)
				min_pckp = vpckp;
		}
		sum += vpckp;
		msleep(40);
	}
	sum -= (max_pckp + min_pckp);
	return sum / (VPCKP_READ_TIMES - 2);
}

static bool max17332_calculate_vout_delta(struct max17332_dev *pdev)
{
	int bob_voltage, vpckp_avg, vreg_uv, vreg_delta;
	struct max17332_voltage_adjustment *vadj = pdev->voltage_adjustment;

	/**
	 * Calculate vout_delta:
	 * 1. Calculate V220: read VPCKP 10 times, calculate median value as VBAT
	 *	V220 = max (VBAT, 3.75V)
	 * 2. Calculate Vreg_uv = Vreg_current - V220
	 * 3. If DROPOUT-alert is received and Vreg_uv < Vreg_uv_max,
	 *    or RSoC increased 1% and current_avg < 70% of current_target:
	 * 		Vreg_delta = min (Vreg_out_delta_rise,
	 * 				Vreg_uv_max – Vreg_uv,
	 * 				Vreg_out_max – Vreg_out)
	 * 		if Vreg_delta > 0:
	 * 			Set Vreg_out(new) = Vreg_out(current) + Vreg_delta
	 * 			(modifying the USB regulator Vout register with 50mV steps ever 10ms)
	 * 		Else:
	 * 			Do not change Vreg_out
	 * 4. If CT or CP alert is received:
	 * 		IF Vreg_uv > Vreg_dropout_min:
	 * 			set Vreg_out(new) = Vreg_out(current) - Vreg_delta_fall
	 * 			(modifying the USB regulator Vout register directly)
	 * 		Else:
	 * 			Do not change Vreg_out
	*/
	vpckp_avg = max(max17332_calculate_vpckp_avg(pdev), 3750000);
	if (vpckp_avg < 0) {
		dev_err(pdev->dev, "%s: get vpckp_avg failed\n", __func__);
		vadj->alert_type = ALRT_TYPE_NONE;
		return false;
	}
	bob_voltage = regulator_get_voltage(pdev->regulator_dev->charger_bob);

	// Calculate undervoltage of regulator and set vout_delta
	vreg_uv = bob_voltage - vpckp_avg;
	if (vadj->alert_type == ALRT_TYPE_DROPOUT
		|| vadj->alert_type == ALRT_TYPE_RSOC_CHANGED) {
		/**
		 * If Dropout and CP\CT alrt occur simultaneously in 10s,
		 * give priority to CP\CT alrt to decrease vsys.
		 */
		if (vadj->alert_type == ALRT_TYPE_DROPOUT)
			vadj->last_change_vsys_for_dropout = true;
		vadj->alert_type = ALRT_TYPE_NONE;
		dev_info(pdev->dev, "%s: current vout_delta :%d\n",
					__func__, pdev->regulator_dev->vout_delta);
		if (vreg_uv < vadj->vreg_uv_max) {
				vreg_delta = min(vadj->vreg_out_delta_rise, vadj->vreg_uv_max - vreg_uv);
				vreg_delta = min(vreg_delta, pdev->regulator_dev->vout_max - bob_voltage);
				if (vreg_delta > 0) {
					pdev->regulator_dev->vout_delta = vreg_delta;
					vadj->is_increase_bob_delta = true;
					dev_info(pdev->dev, "%s: vout_delta changed to: %d\n",
						__func__, pdev->regulator_dev->vout_delta);
					return true;
				}
		} else {
			dev_info(pdev->dev, "%s: vreg_uv > vreg_uv_max, do not change vout_delta", __func__);
			return false;
		}
	}
	if (vadj->alert_type == ALRT_TYPE_CT || vadj->alert_type == ALRT_TYPE_CP) {
		vadj->alert_type = ALRT_TYPE_NONE;
		if (vreg_uv > vadj->vreg_dropout_min) {
			vadj->is_increase_bob_delta = false;
			dev_info(pdev->dev, "%s: decrease vreg_out by: %d\n",
					__func__, vadj->vreg_out_delta_fall);
			return true;
		} else {
			dev_info(pdev->dev, "%s: vreg_uv < vreg_dropout_min, do not change vout_delta", __func__);
			return false;
		}
	}
	return false;
}

static bool max17332_check_vsys_change_period(struct max17332_dev *pdev)
{
	u64 current_time, period_time;

	// Should not modify vsys voltage more than once in a period of "vsys_change_period_min"
	current_time = jiffies;
	period_time = current_time - pdev->voltage_adjustment->vsys_last_change_time;
	if (jiffies_to_msecs(period_time) < pdev->voltage_adjustment->vsys_change_period_min) {
		if (pdev->voltage_adjustment->alert_type == ALRT_TYPE_CP
			|| pdev->voltage_adjustment->alert_type == ALRT_TYPE_CT) {
			if (pdev->voltage_adjustment->last_change_vsys_for_dropout) {
				pdev->voltage_adjustment->last_change_vsys_for_dropout = false;
				return true;
			}
		}
		dev_dbg_ratelimited(pdev->dev,
			"%s: Skip changing vsys voltage: more than once in a period of %d ms\n",
			__func__, pdev->voltage_adjustment->vsys_change_period_min);
		return false;
	}

	return true;
}

static int max17332_set_initial_vsys(struct max17332_dev *pdev)
{
	int target_vol, rc;
	u16 curr_vol;
	int vout_delta = pdev->regulator_dev->vout_delta;

	rc = max17332_read(pdev->regmap_pmic, REG_VCELLREP, &curr_vol);
	if (rc < 0) {
		dev_err(pdev->dev, "Failed to read REG_VCELLREP: %d\n", rc);
		return rc;
	}

	/*
	 * According to meta proposed, changed the initial
	 * value of max77789 vout from vbat+250mV to
	 * max(vbat+250mV, 3.8V)
	 * */
	target_vol = max17332_raw_voltage_to_uvolts(curr_vol) + vout_delta;
	target_vol = max(target_vol, VOLTAGE_ADJUST_INITIAL_VAL);
	rc = max17332_set_vsys_voltage(pdev, target_vol);

	return rc;
}

int max17332_adjust_vsys_voltage(struct max17332_dev *pdev)
{
	int ret;
	int i, change_times;
	int bob_voltage;
	int target_vol;
	struct max17332_voltage_adjustment *vadj = pdev->voltage_adjustment;

	// if this is a usb insert event, set vreg_out =max(vcell+250mV, 3.75V)
	if (vadj->is_usb_insert_event) {
		max17332_set_usb_insert_event(pdev, false);
		/* After usb disconnected, max77789 driver will auto set it's output
		* to 4.2V, and Linux regulator framework does not know this, since
		* Linux regulator framework will record last setting voltage, if we're
		* setting the same range as last time the change after usb connected,
		* it will be a noop, so set it to default 4.2V, then the next setting
		* will take effect
		*/
		if (pdev->voltage_adjustment->reset_vsys_on_usb_insertion) {
			dev_info(pdev->dev, "USB insert event, set vsys to default %d uV\n",
				 VSYS_DEFAULT);
			max17332_set_vsys_voltage(pdev, VSYS_DEFAULT);
		}

		dev_info(pdev->dev,
			"%s: USB insert event, do not calculate vout_delta",
			__func__);
		ret = max17332_set_initial_vsys(pdev);
		if (ret < 0) {
			dev_err(pdev->dev, "Failed to set vsys initial value: %d\n", ret);
			return 0;
		}
		return 1;
	}

	bob_voltage = regulator_get_voltage(pdev->regulator_dev->charger_bob);
	if (pdev->voltage_adjustment->alert_type == ALRT_TYPE_DROPOUT) {
		if (bob_voltage == pdev->regulator_dev->vout_max) {
			dev_info(pdev->dev,
				"%s: vreg_out_set reach the maximum, do not enhance it for Dropout!",
				__func__);
			return 1;
		}
	}

	if (!max17332_check_vsys_change_period(pdev))
		return 1;

	if (!max17332_calculate_vout_delta(pdev))
		return -1;

	/**
	 * For adjusting vsys:
	 * if increase vsys:
	 *     set vsys = vsys + vout_delta
	 *     modifying the vsys with 50mV steps ever 10ms
	 * if decrease vsys:
	 *     set vsys = vsys - vreg_out_delta_fall;
	 *     modifying the vsys directly
	 */
	if (vadj->is_increase_bob_delta) {
		dev_info(pdev->dev, "%s: get DROPOUT alrt or RSoC changed, set vsys step by step\n", __func__);
		change_times = pdev->regulator_dev->vout_delta / VSYS_CHANGE_STEP;
		target_vol =  bob_voltage;
		for (i = 0; i < change_times; i++) {
			target_vol += VSYS_CHANGE_STEP;
			ret = max17332_set_vsys_voltage(pdev, target_vol);
			if (ret < 0)
				return ret;
			msleep(10);
		}
		if (pdev->regulator_dev->vout_delta < VSYS_CHANGE_STEP) {
			// When vout_delta < 50mV
			target_vol += pdev->regulator_dev->vout_delta;
			ret = max17332_set_vsys_voltage(pdev, target_vol);
			if (ret < 0)
				return ret;
		}
	} else {
		dev_info(pdev->dev, "%s: get CT or CP alrt, set vsys directly\n", __func__);
		target_vol = bob_voltage - vadj->vreg_out_delta_fall;
		ret = max17332_set_vsys_voltage(pdev, target_vol);
		if (ret < 0)
			return ret;
	}

	return 1;
}

int max17332_init_voltage_adjustment(struct max17332_dev *pdev)
{
	pdev->voltage_adjustment = devm_kzalloc(pdev->dev, sizeof(*pdev->voltage_adjustment), GFP_KERNEL);
	if (unlikely(!pdev->voltage_adjustment)) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}
	// Initialize parameters for vsys voltage adjustment
	pdev->voltage_adjustment->vreg_dropout_min = VREG_DROPOUT_MIN_INIT;
	pdev->voltage_adjustment->vreg_out_delta_fall = VREG_OUT_DELTA_FALL_INIT;
	pdev->voltage_adjustment->vreg_out_delta_rise = VREG_OUT_DELTA_RISE_INIT;
	pdev->voltage_adjustment->vreg_uv_max = VREG_UV_MAX_INIT;
	pdev->voltage_adjustment->vsys_change_period_min = VSYS_CHANGE_PERIOD_MIN_INIT;
	pdev->voltage_adjustment->alert_type = ALRT_TYPE_NONE;
	pdev->voltage_adjustment->vsys_last_change_time = 0;
	if (of_property_read_bool(pdev->dev->of_node, "max17332,reset-vsys-on-usb-insertion")) {
		pdev->voltage_adjustment->reset_vsys_on_usb_insertion = true;
	}
	return 0;
}

void max17332_set_alert_type(struct max17332_dev *pdev, u16 value)
{
	int alert_type = ALRT_TYPE_NONE;

	if (value & BIT_STATUS_DROPOUT)
		alert_type = ALRT_TYPE_DROPOUT;
	if (value & BIT_STATUS_CP)
		alert_type = ALRT_TYPE_CP;
	if (value & BIT_STATUS_CT)
		alert_type = ALRT_TYPE_CT;

	pdev->voltage_adjustment->alert_type = alert_type;
}

void max17332_set_vsys_change_time(struct max17332_dev *pdev, u64 time)
{
	pdev->voltage_adjustment->vsys_last_change_time = time;
}

void max17332_set_usb_insert_event(struct max17332_dev *pdev, bool is_insert)
{
	pdev->voltage_adjustment->is_usb_insert_event = is_insert;
}

void max17332_destroy_voltage_adjustment(struct max17332_dev *pdev)
{
	if (likely(pdev->voltage_adjustment))
		devm_kfree(pdev->dev, pdev->voltage_adjustment);
}

static int max17332_raw_current_to_uamps(struct max17332_dev *pdev, int curr)
{
	u16 rsense;
	rsense = pdev->pdata->rsense;
	return curr * 15625 / ((int)rsense * 10);
}

static int max17332_get_target_chg_current(struct max17332_dev *pdev)
{
	u16 val;
	int ret;
	int current_target;

	ret = max17332_read(pdev->regmap_pmic, REG_CHGCURRENT, &val);
	if (ret < 0) {
		pr_err("%s : fail to read REG_CHGCURRENT\n", __func__);
		return ret;
	}
	current_target = max17332_raw_current_to_uamps(pdev, sign_extend32(val, 15));
	dev_info(pdev->dev,
			"%s: current target: %d",
			__func__, current_target);
	return current_target;
}

bool max17332_check_current_avg(struct max17332_dev *pdev)
{
	u16 reg;
	int ret = 0;
	int curr_avg;
	int curr_target;

	ret = max17332_read(pdev->regmap_pmic, REG_AVGCURRENT, &reg);
	if (ret < 0)
		return false;
	curr_avg = max17332_raw_current_to_uamps(pdev, sign_extend32(reg, 15));
	curr_target = max17332_get_target_chg_current(pdev);
	if (curr_target < 0)
		return false;
	//Check if current_avg < 70% of current_target
	if (curr_avg < curr_target * CURRENT_AVG_SCALING / 10) {
		pdev->voltage_adjustment->alert_type = ALRT_TYPE_RSOC_CHANGED;
		dev_info(pdev->dev,
			"%s: RSoC changed, current_avg < current_target * 0.7, increase vsys",
			__func__);
		return true;
	}

	return false;
}
