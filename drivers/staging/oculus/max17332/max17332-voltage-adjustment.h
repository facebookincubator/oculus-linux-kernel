// SPDX-License-Identifier: GPL-2.0-only

#ifndef __MAX17332_VOLTAGE_ADJUSTMENT_H_
#define __MAX17332_VOLTAGE_ADJUSTMENT_H_

#include <linux/types.h>

struct max17332_dev;

#ifdef CONFIG_MAX17332_VOLTAGE_ADJUSTMENT

/* Initial values of parameters for vsys voltage adjustment */
#define VREG_UV_MAX_INIT		600000	/* Regulator undervoltage maximum uV */
#define VREG_OUT_DELTA_RISE_INIT	150000	/* uV */
#define VREG_OUT_DELTA_FALL_INIT	100000	/* uV */
#define VREG_DROPOUT_MIN_INIT		200000	/* uV */
#define VSYS_CHANGE_PERIOD_MIN_INIT	10000	/* ms */

/* Alert types for vsys voltage adjustment */
#define ALRT_TYPE_NONE		0
#define ALRT_TYPE_DROPOUT	1
#define ALRT_TYPE_CT		2
#define ALRT_TYPE_CP		3
#define ALRT_TYPE_RSOC_CHANGED	4

struct max17332_voltage_adjustment {
	int vreg_out_delta_rise;
	int vreg_out_delta_fall;
	int vreg_uv_max;
	int vreg_dropout_min;
	int vsys_change_period_min;
	int alert_type;
	u64 vsys_last_change_time;
	bool is_increase_bob_delta;
	bool is_usb_insert_event;
	bool last_change_vsys_for_dropout;
	bool reset_vsys_on_usb_insertion;
};

int max17332_init_voltage_adjustment(struct max17332_dev *pdev);
void max17332_set_alert_type(struct max17332_dev *pdev, u16 value);
int max17332_adjust_vsys_voltage(struct max17332_dev *pdev);
void max17332_set_vsys_change_time(struct max17332_dev *pdev, u64 time);
void max17332_set_usb_insert_event(struct max17332_dev *pdev, bool is_insert);
void max17332_destroy_voltage_adjustment(struct max17332_dev *pdev);
bool max17332_check_current_avg(struct max17332_dev *pdev);

#else

static inline int max17332_init_voltage_adjustment(struct max17332_dev *pdev)
{
	return 0;
}

static inline void max17332_set_alert_type(struct max17332_dev *pdev, u16 value)
{
}

static inline int max17332_adjust_vsys_voltage(struct max17332_dev *pdev)
{
	return 0;
}

static inline void max17332_set_vsys_change_time(struct max17332_dev *pdev, u64 time)
{
}

static inline void max17332_set_usb_insert_event(struct max17332_dev *pdev, bool is_insert)
{
}

static inline void max17332_destroy_voltage_adjustment(struct max17332_dev *pdev)
{
}

static inline bool max17332_check_current_avg(struct max17332_dev *pdev)
{
	return false;
}

#endif /* CONFIG_MAX17332_VOLTAGE_ADJUSTMENT */
#endif /* __MAX17332_VOLTAGE_ADJUSTMENT_H_ */
