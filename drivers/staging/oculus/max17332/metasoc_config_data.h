/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __METASOC_CONFIG_DATA_H_
#define __METASOC_CONFIG_DATA_H_

#if (IS_ENABLED(CONFIG_METASOC))

#include "metasoc.h"

static metasoc_config_data config_data_hammerheadpack_cellv_0_packv_0 = {
	.soc_table = { 0, 85, 653, 1582, 2201, 2923, 3318, 4588, 5349, 5706,
		       6301, 7231, 7699, 8375, 9069, 10000 },
	.ocv_table_mv = { 3000, 3266, 3670, 3714, 3755, 3779, 3803, 3859, 3914,
			  3950, 4010, 4104, 4169, 4248, 4328, 4450 },
	.fast_conv_threshold_mv = 3800,
	.dv_buffer_room_mv = 300,
	.temp_coeff_uvc = 2000,
	.batt_active_frac_mv = 20,
	.charge_voltage_mv = 4440,
	.vsys_min_mv = 2600,
	.sys_resistance_mohm = 392,
	.batt_rsense_mohm = 20,
	.max_slew_rate = 5,
	.soc_init_val = 5,
	.last_metasoc = 0,
	.last_update_time_s = 0,
	.csoc_correction_pct = 74,
	.csoc_gain = 9,
	.usoc_gain = 90,
	.low_voltage_comp_thresh_mv = 2700,
	.droop_penalty_coefficient = 3,
	.min_avg_voltage_droop_mv = 250,
	.charge_soc_masking = true,
	.low_battery_alert_threshold_high = 1500,
	.low_battery_alert_threshold_low = 500,
	.enable_zero_repsoc_convergence = 1,
	.v_empty_mv = 3300,
	.qres_gain = 95,
	.config_id = METASOC_CONFIG_ID_HAMMERHEAD_C1,
};

static metasoc_config_data config_data_sc50_cellv_0_packv_0 = {
	.soc_table = { 0, 102, 790, 1267, 1897, 2975, 3571, 4171, 4727, 5466,
		       6293, 6477, 7563, 8734, 9809, 10000 },
	.ocv_table_mv = { 3000, 3403, 3678, 3690, 3729, 3786, 3820, 3850, 3884,
			  3940, 4028, 4056, 4191, 4328, 4424, 4465 },
	.fast_conv_threshold_mv = 3800,
	.dv_buffer_room_mv = 300,
	.temp_coeff_uvc = 2000,
	.batt_active_frac_mv = 20,
	.charge_voltage_mv = 4450,
	.vsys_min_mv = 2600,
	.sys_resistance_mohm = 392,
	.batt_rsense_mohm = 20,
	.max_slew_rate = 5,
	.soc_init_val = 5,
	.csoc_correction_pct = 0,
	.csoc_gain = 0,
	.usoc_gain = 90,
	.low_voltage_comp_thresh_mv = 2700,
	.droop_penalty_coefficient = 3,
	.min_avg_voltage_droop_mv = 250,
	.charge_soc_masking = true,
	.low_battery_alert_threshold_high = 1500,
	.low_battery_alert_threshold_low = 500,
	.enable_zero_repsoc_convergence = 1,
	.v_empty_mv = 3300,
	.qres_gain = 95,
	.config_id = METASOC_CONFIG_ID_SC50_C1,
};

static metasoc_config_data config_data_sc50_cellv_2_packv_0 = {
	.soc_table = { 0, 102, 790, 1267, 1897, 2975, 3571, 4171, 4727, 5466,
		       6293, 6477, 7563, 8734, 9809, 10000 },
	.ocv_table_mv = { 3000, 3403, 3678, 3690, 3729, 3786, 3820, 3850, 3884,
			  3940, 4028, 4056, 4191, 4328, 4424, 4465 },
	.fast_conv_threshold_mv = 3800,
	.dv_buffer_room_mv = 300,
	.temp_coeff_uvc = 2000,
	.batt_active_frac_mv = 20,
	.charge_voltage_mv = 4450,
	.vsys_min_mv = 2600,
	.sys_resistance_mohm = 392,
	.batt_rsense_mohm = 20,
	.max_slew_rate = 5,
	.soc_init_val = 5,
	.csoc_correction_pct = 0,
	.csoc_gain = 0,
	.usoc_gain = 90,
	.low_voltage_comp_thresh_mv = 2700,
	.droop_penalty_coefficient = 3,
	.min_avg_voltage_droop_mv = 250,
	.charge_soc_masking = true,
	.low_battery_alert_threshold_high = 1500,
	.low_battery_alert_threshold_low = 500,
	.enable_zero_repsoc_convergence = 1,
	.v_empty_mv = 3300,
	.qres_gain = 95,
	.config_id = METASOC_CONFIG_ID_SC50_C2,
};

#endif

#endif // __MAX17332_BATTERY_H_
