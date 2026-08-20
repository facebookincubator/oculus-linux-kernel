/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) Meta Platforms, Inc. and affiliates. */

/* linux kernel cannot include c standard headers */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include "metasoc_utility.h"

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#define METASOC_TABLE_SZ 16

typedef enum
{
    METASOC_CONFIG_ID_ERROR   = -1, // error when requesting config ID
    METASOC_CONFIG_ID_UNKNOWN = 0,
    METASOC_CONFIG_ID_HAMMERHEAD_C1, // hammerhead config1
    METASOC_CONFIG_ID_SC50_C1, // sc50 config1
    METASOC_CONFIG_ID_GREATWHITE_C1, // greatwhite config1
    METASOC_CONFIG_ID_FLORIAN_MAKO, // Florian config1 - Mako
    METASOC_CONFIG_ID_SC50_C2, // sc50 config2
    METASOC_CONFIG_ID_FLORIAN_SILVERTIP_ZEBRA, // Florian config2 - Silvertip, Zebra
    METASOC_CONFIG_ID_FLORIAN_LAGER, // Florian config3 - Lager
} metasoc_config_id;

typedef enum
{
    DISABLE_ZERO_REPSOC_CONVERGENCE           = 0, // v2
    ENABLE_IMMEDIATE_ZERO_REPSOC_CONVERGENCE  = 1, // v1
    ENABLE_DELAYED_ZERO_REPSOC_CONVERGENCE    = 2, // v3
    ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE = 3, // v4
    CONVERGENCE_THROUGH_PEAK_POWER_MANAGER    = 4, // v5
} metasoc_repsoc_convergence_t;

/**
 * Initialization structure for metasoc
 */
typedef struct
{
    // chemical SOC values between 0 and 100 with 2 decimal points in increasing order
    uint16_t soc_table[METASOC_TABLE_SZ];
    // OCV vector in mV in increasing order
    uint16_t ocv_table_mv[METASOC_TABLE_SZ];
    // voltage threshold for fast convergence region in mV
    uint16_t fast_conv_threshold_mv;
    // high SOC shutdown voltage buffer in mV at room temperature
    uint16_t dv_buffer_room_mv;
    // temperature coefficent for shutdown voltage buffer in uV/degC
    uint16_t temp_coeff_uvc;
    // battery impedance scaling factor in % for peak power compensation
    uint16_t batt_active_frac_mv;
    // max charging voltage threshold in mV
    uint16_t charge_voltage_mv;
    // minimum operating voltage in mV
    uint16_t vsys_min_mv;
    // total round trip resistance in mOhm between gauge sense point and pmic input
    uint16_t sys_resistance_mohm;
    // gauge current sense resistor in mOhm
    uint16_t batt_rsense_mohm;
    // typical max slew rate for metasoc in % per min
    uint16_t max_slew_rate;
    // seed value to initialize metasoc with after a low battery shutdown
    uint8_t soc_init_val;
    // last value of metasoc
    uint8_t last_metasoc;
    // last time in seconds that metasoc was updated (including time in shutdown)
    uint32_t last_update_time_s;
    // cSOC correction offset in 0.01%
    uint16_t csoc_correction_pct;
    // cSOC correction gain in 1/10000
    uint8_t csoc_gain;
    // uSOC filter gain in % for smoothing
    uint8_t usoc_gain;
    // low voltage HW comparator threshold in mV
    uint16_t low_voltage_comp_thresh_mv;
    // droop penalty coefficient in %
    uint8_t droop_penalty_coefficient;
    // min limit of average voltage droop in mV
    uint8_t min_avg_voltage_droop_mv;
    // Masking of SOC from 95-100% during charging to show 100% SOC
    bool charge_soc_masking;
    // low battery alert thresholds to be used for low battery shutdown
    uint16_t low_battery_alert_threshold_high;
    uint16_t low_battery_alert_threshold_low;
    // enable zero repsoc convergence helps with convergence at low battery levels
    uint8_t enable_zero_repsoc_convergence;
    // battery empty voltage theshold in mV
    uint16_t v_empty_mv;
    // qres filter gain in % for smoothing
    uint8_t qres_gain;
    // private data to be used by implementor of utility functions
    void *private_data;
    // utility functions to be implemented by caller
    metasoc_utility_function_impl utility_function;
    // config identifier
    metasoc_config_id config_id;
} metasoc_config_data;

/**
 * Parameter list for metasoc calculation
 */
typedef struct
{
    // filtered cell voltage in mV
    uint16_t avg_voltage_mv;
    // instantaneous cell voltage in mV
    uint16_t inst_voltage_mv;
    // instantaneous battery current in mA
    int16_t inst_current_ma;
    // battery temperature in C
    int8_t temp_c;
    // battery level in % with 2 decimal points
    uint16_t battery_level;
    // battery impedance in mOhm
    uint16_t battery_impedance_mohm;
    // load compensated battery capacity in mAh
    uint16_t battery_capacity_mah;
    // charging status
    bool is_charging;
    // battery fully charged
    bool is_fully_charged;
    // HW brownout low voltage comparator tripped input
    bool low_voltage_comp_tripped;
    // OCV in mV
    int ocv_mv;
    // binary to represent if the battery is empty
    bool is_repsoc_zero;
    // FullCapNom register from the FG
    int full_capacity_nominal_mah;
    // Discharge FET OFF bit read from the FG
    bool block_discharge;
    // QRES value from the FG
    uint16_t qres;
} metasoc_param;

/**
 * Internal stats for metasoc
 */
typedef struct
{
    // End of discharge calculation stats
    int fast_conv_threshold;
    int voltage_buffer;
    int peak_load_comp_weight;
    int voltage_buffer_weight;
    int ocv;
    int avg_voltage_droop_mv;
    int peak_voltage_droop_mv;
    int predicted_ocv_eod;
    int soc_end_of_discharge;

    // Additional stats
    int corrected_battery_level;
    int soc_end_of_charge;
    int usoc;
    int time_delta_s;
    bool is_battery_empty;
    uint16_t droop_penalty_scalar;
    uint16_t droop_penalty;
    int usoc_filtered;
    int remaining_capacity;
} metasoc_internal_stats;

/**
 * Internal stats for fused metasoc stats
 */
typedef struct
{
    int fused_usoc;
    int fused_usoc_filtered;
    uint16_t fused_peak_voltage_droop_penalty;
} metasoc_internal_stats_fused;

typedef enum
{
    METASOC_ERROR_NONE = 0, // No error
    METASOC_ERROR_INVALID_CONFIG, // Invalid configuration data
    METASOC_ERROR_INIT_FAILED, // Initialization failed
    METASOC_ERROR_UPDATE_FAILED, // Update of state of charge failed
    METASOC_ERROR_GET_SOC_FAILED, // Retrieval of state of charge failed
    METASOC_ERROR_GET_STATS_FAILED, // Retrieval of internal stats failed
    METASOC_ERROR_LOW_BATTERY_SHUTDOWN_FAILED, // Setting low battery shutdown failed
    METASOC_ERROR_GET_PARAMS_FAILED, // Retrieval of parameters failed
    METASOC_ERROR_UNKNOWN // Unknown error
} metasoc_error_t;

/**
 * \brief Initializes the metasoc library
 * \param config metasoc configuration
 * \return 0 on success, negative value otherwise
 */
int metasoc_init(uint8_t batt_id, metasoc_config_data *config);

/**
 * \brief Updates the internal calculation for state of charge (soc)
 * \param params parameters used to calculate metasoc
 * \return 0 on success, negative value otherwise
 */
int metasoc_update_soc(uint8_t batt_id, metasoc_param *params);

/**
 * \brief Returns the last metasoc calculated
 * \note Only supported for single battery. Call metasoc_get_fused_soc() for multiple batteries
 * \param soc current state of charge (0 - 100%)
 * \return 0 on success, negative value otherwise
 */
int metasoc_get_soc(uint8_t batt_id, uint8_t *soc);

/**
 * \brief Calculates the fused metasoc
 * \note Only supported for multiple batteries. Once the metasoc_update_soc() is called for each
 * battery, this function can then be called to calculate and get the fused metasoc
 * \param fused_soc Fused SOC value as output
 * \return 0 on success, negative value otherwise
 */
int metasoc_get_fused_soc(uint16_t *fused_soc);

/**
 * \brief Returns the internal stats used to calculate metasoc
 * \param stats stats to be filled
 * \return 0 on success, negative value otherwise
 */
int metasoc_get_internal_stats(uint8_t batt_id, metasoc_internal_stats *stats);

/**
 * \brief Sets the low battery shutdown bit externally
 * \param battery_capacity_mah current battery capacity
 * \return 0 on success, negative value otherwise
 */
int metasoc_set_low_battery_shutdown(uint8_t batt_id, uint16_t battery_capacity_mah);

/**
 * \brief Returns the config id of the metasoc config being used
 * \return config_id
 */
metasoc_config_id metasoc_get_config_id(uint8_t batt_id);

/**
 * \brief Deinitializes the metasoc library
 * \note This function should be called to clean up resources used by the metasoc library.
 */

void metasoc_deinit(void);

/**
 * \brief Returns the fused internal stats used to calculate metasoc
 * \param stats fused stats to be filled
 * \return 0 on success, negative value otherwise
 */
int metasoc_get_internal_stats_fused(metasoc_internal_stats_fused *stats);

#ifdef __cplusplus
}
#endif
