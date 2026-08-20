// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "limits.h"

#include "metasoc.h"
#include "metasoc_platform.h"

#ifndef NUM_BATTERIES
#define NUM_BATTERIES 1
#endif

#define BATT_ID_0 0

// Interpolation table size for metasoc calculation
#define METASOC_INTERPOLATION_TABLE_SZ 4

#ifndef CAP_0_TO_100
#define CAP_0_TO_100(val) ((MIN(MAX(0, val), 100)))
#endif

#ifndef CAP_0_TO_10000
#define CAP_0_TO_10000(val) ((MIN(MAX(0, val), 10000)))
#endif

typedef struct
{
    metasoc_config_data config;
    int soc_end_of_charge;
    uint16_t metasoc;
    uint32_t last_update_time_s;
    bool is_prev_low_battery_shutdown;
    uint8_t stored_batt_capacity;
    bool metasoc_enabled;
    bool oob_charging;
    bool is_init;
    int peak_voltage_droop_penalty;
    int usoc_filtered;
    int remaining_capacity;
    int is_charging;
    bool is_battery_voltage_low;
    bool charge_transition;
    bool is_fully_charged;
    int battery_capacity_mah;
    int qres_filtered;
    uint32_t time_is_battery_voltage_low;
    uint16_t avg_voltage_mv;
    int8_t temp_c;
    metasoc_internal_stats stats;
} metasoc_context;

typedef struct
{
    metasoc_config_data *config;
    int usoc_filtered;
    uint16_t metasoc;
    uint16_t peak_voltage_droop_penalty;
    uint32_t last_update_time_s;
    int fused_usoc;
    bool is_charging;
} metasoc_fusion_context;

static metasoc_context ctx[NUM_BATTERIES];
static metasoc_fusion_context fusion_ctx;
static int linear_interpolation(uint16_t val, uint16_t *x_table, uint16_t *y_table, int table_len);

// interpolation table in state of charge percentage with 2 decimal points of resolution
static uint16_t metasoc_interpolation_table_x[METASOC_INTERPOLATION_TABLE_SZ] = {0,
                                                                                 5000,
                                                                                 9500,
                                                                                 10000};
static uint16_t metasoc_interpolation_table_y[METASOC_INTERPOLATION_TABLE_SZ] = {0,
                                                                                 5000,
                                                                                 10000,
                                                                                 10000};

static inline uint16_t min_ocv(metasoc_config_data *cfg)
{
    return cfg->ocv_table_mv[0];
}

static inline uint16_t max_ocv(metasoc_config_data *cfg)
{
    return cfg->ocv_table_mv[METASOC_TABLE_SZ - 1];
}

static inline int compute_peak_load_comp_weight(metasoc_param *params,
                                                metasoc_config_data *cfg,
                                                int fast_conv_threshold)
{
    // scaling factor for peak load calculation
    const int SCALE_FACTOR = 100;
    int peak_load_weight   = 0;
    if (params->is_charging)
    {
        return 0;
    }

    peak_load_weight = ((max_ocv(cfg) - params->avg_voltage_mv) * SCALE_FACTOR) /
                       (max_ocv(cfg) - fast_conv_threshold);

    return CAP_0_TO_100(peak_load_weight);
}

static inline int compute_voltage_buffer_weight(metasoc_param *params,
                                                metasoc_config_data *cfg,
                                                int fast_conv_threshold)
{
    // scaling factor for voltage buffer calculation
    const int SCALE_FACTOR    = 100;
    int voltage_buffer_weight = 0;

    if (params->is_charging)
    {
        return 0;
    }

    voltage_buffer_weight = ((params->avg_voltage_mv - min_ocv(cfg)) * SCALE_FACTOR) /
                            (fast_conv_threshold - min_ocv(cfg));

    return CAP_0_TO_100(voltage_buffer_weight);
}

static inline int compute_peak_voltage_droop(metasoc_config_data *cfg, metasoc_param *params)
{
    if (params->is_charging)
    {
        return 0;
    }

    return -(params->battery_impedance_mohm * params->inst_current_ma * cfg->batt_active_frac_mv);
}

static inline int compute_peak_voltage_droop_penalty(metasoc_config_data *cfg,
                                                     bool is_charging,
                                                     bool low_voltage_comp_tripped,
                                                     int curr_droop_penalty,
                                                     int pmu_voltage_estimate)
{
    int droop_penalty = 0;

    if (is_charging)
    {
        return 0;
    }

    droop_penalty = MAX((min_ocv(cfg) - pmu_voltage_estimate), curr_droop_penalty);
    if (low_voltage_comp_tripped)
    {
        droop_penalty = MAX((min_ocv(cfg) - cfg->low_voltage_comp_thresh_mv), droop_penalty);
    }

    return droop_penalty;
}

static inline int compute_voltage_buffer(metasoc_config_data *cfg, metasoc_param *params)
{
    const int ROOM_TEMPERATURE_C = 25;
    int temp_comp_mc             = params->temp_c - ROOM_TEMPERATURE_C;

    return cfg->dv_buffer_room_mv - (cfg->temp_coeff_uvc * temp_comp_mc / 1000);
}

static inline int compute_fast_conv_threshold(metasoc_config_data *cfg, metasoc_param *params)
{
    const int ROOM_TEMPERATURE_C = 25;
    int temp_comp_mc             = params->temp_c - ROOM_TEMPERATURE_C;

    return cfg->fast_conv_threshold_mv - (cfg->temp_coeff_uvc * temp_comp_mc / 1000);
}

static int calculate_soc_eoc(metasoc_config_data *cfg)
{
    const int INTERPOLATION_SHIFT = 100;

    return linear_interpolation(
               cfg->charge_voltage_mv, cfg->ocv_table_mv, cfg->soc_table, METASOC_TABLE_SZ) -
           INTERPOLATION_SHIFT;
}

static int calculate_pmu_voltage_estimate(metasoc_config_data *cfg, metasoc_param *params)
{
    const int SCALE_TO_MV = 1000;
    int internal_loss_mv =
        ((int)MIN(0, params->inst_current_ma) * cfg->sys_resistance_mohm) / SCALE_TO_MV;

    return params->inst_voltage_mv + internal_loss_mv;
}

static bool is_pmic_voltage_lte_min(metasoc_config_data *cfg, int pmu_voltage_estimate)
{
    return pmu_voltage_estimate <= cfg->vsys_min_mv;
}

static int calculate_usoc(metasoc_config_data *cfg,
                          int soc_eod,
                          int soc_eoc,
                          int corrected_battery_level,
                          int pmu_voltage_estimate,
                          int ocv_mv,
                          bool is_repsoc_zero,
                          bool is_charging,
                          bool *is_battery_voltage_low,
                          uint32_t *time_is_battery_voltage_low)
{
    // scale for battery full with 2 decimal place resolution
    const int BATTERY_FULL_2DEC_SCALE = 10000;
    const int BATTERY_1_PCT_HOLD      = 100;
    const int OCV_THRESHOLD_MV        = 3350;

    if (cfg->enable_zero_repsoc_convergence == DISABLE_ZERO_REPSOC_CONVERGENCE ||
        cfg->enable_zero_repsoc_convergence == CONVERGENCE_THROUGH_PEAK_POWER_MANAGER)
    {
        is_repsoc_zero = false;
    }
    if (is_pmic_voltage_lte_min(cfg, pmu_voltage_estimate) || ocv_mv < OCV_THRESHOLD_MV ||
        (is_repsoc_zero && !is_charging))
    {
        *is_battery_voltage_low = true;
        if (*time_is_battery_voltage_low == 0)
        {
            *time_is_battery_voltage_low = cfg->utility_function.get_time_in_sec(cfg->private_data);
        }
        return 0;
    }
    else
    {
        *is_battery_voltage_low      = false;
        *time_is_battery_voltage_low = 0;
    }

    if (soc_eod > soc_eoc)
    {
        // 1% hold logic for this unexpected corner case
        return BATTERY_1_PCT_HOLD;
    }
    return CAP_0_TO_10000(
        (((corrected_battery_level - soc_eod) * BATTERY_FULL_2DEC_SCALE) / (soc_eoc - soc_eod)));
}

static int linear_interpolation(uint16_t val, uint16_t *x_table, uint16_t *y_table, int table_len)
{
    int idx = table_len - 1;
    int i   = 0;
    // For Multiplication of uint16 and intermediate calculations, must use int32
    int32_t y1 = 0;
    int32_t y2 = 0;
    int32_t x1 = 0;
    int32_t x2 = 0;
    int32_t m  = 0;
    int32_t z  = 0;

    // Limit to smallest entry in y_table
    if (val < x_table[0])
    {
        return y_table[0];
    }

    // Find the largest element in the sorted list that is less
    // than val
    for (i = 1; i < table_len; i++)
    {
        if (val < x_table[i])
        {
            idx = i - 1;
            break;
        }
    }

    // If we didn't find one, limit to highest value in y_table
    if (i == table_len)
    {
        return y_table[table_len - 1];
    }

    // perform linear interpolation
    y1 = y_table[idx];
    y2 = y_table[idx + 1];
    x1 = x_table[idx];
    x2 = x_table[idx + 1];
    m  = y2 - y1;
    z  = ((val - x1) * m) / (x2 - x1);

    return z + y1;
}

static inline int calculate_droop_penalty_scalar(int droop_penalty_coefficient,
                                                 int metasoc,
                                                 uint16_t low_batt_thresh_hi,
                                                 uint16_t low_batt_thresh_lo)
{
    const int BATTERY_FULL_2DEC = 10000;
    int clamp_metasoc =
        MIN(MAX(metasoc, low_batt_thresh_lo), low_batt_thresh_hi) - low_batt_thresh_lo;

    return BATTERY_FULL_2DEC + (droop_penalty_coefficient * clamp_metasoc);
}

static inline int calculate_fast_convergence_slew_rate(uint16_t metasoc,
                                                       uint16_t max_slew_rate,
                                                       uint32_t time_since_battery_voltage_low)
{
    const int MIN_TO_S           = 60;
    const int SHUTDOWN_TIME_S    = 30;
    const int BATTERY_2PCT_SCALE = 100;
    int base_slew_rate           = max_slew_rate * BATTERY_2PCT_SCALE;
    // enforce shutdown in 30s of is_battery_voltage_low being set
    int slew_rate =
        MAX((metasoc * MIN_TO_S) / MAX((int)(SHUTDOWN_TIME_S - time_since_battery_voltage_low), 1),
            base_slew_rate);

    return slew_rate;
}

static inline int calculate_slew_rate_for_metasoc(int enable_zero_repsoc_convergence,
                                                  int peak_voltage_droop_penalty,
                                                  uint16_t metasoc,
                                                  uint16_t max_slew_rate,
                                                  bool charging,
                                                  uint16_t low_batt_thresh_hi,
                                                  uint16_t low_batt_thresh_lo,
                                                  bool is_battery_voltage_low,
                                                  uint32_t time_since_battery_voltage_low)
{
    const uint16_t DROOP_PENALTY_THRESHOLD_1 = 100;
    const uint16_t DROOP_PENALTY_THRESHOLD_2 = 300;
    const int BATTERY_2PCT_SCALE             = 100;
    int base_slew_rate                       = max_slew_rate * BATTERY_2PCT_SCALE;

    // Initialize to charging slew rate
    int slew_rate = base_slew_rate / 2;

    // Identify the slew rate based on battery level
    if (!charging)
    {
        if ((metasoc > low_batt_thresh_hi) &&
            (peak_voltage_droop_penalty < DROOP_PENALTY_THRESHOLD_1))
        {
            // Base slew rate for higher SOC
            slew_rate = base_slew_rate;
        }
        else if ((metasoc <= low_batt_thresh_lo) ||
                 (peak_voltage_droop_penalty > DROOP_PENALTY_THRESHOLD_2))
        {
            // Expedited slew rate after crossing second battery alert threshold at 5% SOC
            slew_rate = base_slew_rate * 2;
        }
        else
        {
            // Expedited slew rate after crossing first battery alert threshold at 15% SOC
            slew_rate = (base_slew_rate * 3) / 2;
        }
    }
    if (is_battery_voltage_low &&
        enable_zero_repsoc_convergence == ENABLE_DELAYED_ZERO_REPSOC_CONVERGENCE)
    {
        // Drop to 0 in 1 minute
        slew_rate = calculate_fast_convergence_slew_rate(
            metasoc, max_slew_rate, time_since_battery_voltage_low);
    }

    return slew_rate;
}

static inline int calculate_slew_rate_using_qres(uint16_t metasoc,
                                                 uint16_t max_slew_rate,
                                                 bool charging,
                                                 int battery_temp,
                                                 bool is_battery_voltage_low,
                                                 uint32_t time_since_battery_voltage_low)
{
    const int BATTERY_2PCT_SCALE   = 100;
    const int COLD_TEMP_RANGE_HIGH = 10;
    const int COLD_TEMP_RANGE_LOW  = -10;
    const int COLD_TEMP_RANGE      = COLD_TEMP_RANGE_HIGH - COLD_TEMP_RANGE_LOW;
    int base_slew_rate             = max_slew_rate * BATTERY_2PCT_SCALE;
    // Initialize to charging slew rate
    int slew_rate = base_slew_rate / 2;

    // Identify the slew rate based on battery temperature
    if (!charging)
    {
        slew_rate =
            base_slew_rate + MIN(MAX(COLD_TEMP_RANGE_HIGH - battery_temp, 0), COLD_TEMP_RANGE) *
                                 COLD_TEMP_RANGE_HIGH;
        if (is_battery_voltage_low)
        {
            slew_rate = calculate_fast_convergence_slew_rate(
                metasoc, max_slew_rate, time_since_battery_voltage_low);
        }
    }

    return slew_rate;
}

static int calculate_soc_eod(metasoc_config_data *cfg,
                             metasoc_context *context,
                             metasoc_param *params,
                             int corrected_battery_level,
                             int peak_voltage_droop_penalty,
                             int droop_penalty_scalar)
{
    const int BATTERY_FULL_SCALE         = 100;
    const int VOLTAGE_DROOP_SCALE        = 100000;
    const int PEAK_VOLTAGE_PENALTY_SCALE = 10;

    int fast_conv_threshold   = compute_fast_conv_threshold(cfg, params);
    int voltage_buffer        = compute_voltage_buffer(cfg, params);
    int peak_load_comp_weight = compute_peak_load_comp_weight(params, cfg, fast_conv_threshold);
    int voltage_buffer_weight = compute_voltage_buffer_weight(params, cfg, fast_conv_threshold);
    int ocv                   = linear_interpolation(
        corrected_battery_level, cfg->soc_table, cfg->ocv_table_mv, METASOC_TABLE_SZ);
    int avg_voltage_droop_mv =
        params->is_charging ? 0
                            : MAX((ocv - params->avg_voltage_mv),
                                  (cfg->min_avg_voltage_droop_mv * peak_load_comp_weight) / 100);
    int peak_voltage_droop_mv =
        (compute_peak_voltage_droop(cfg, params) +
         (PEAK_VOLTAGE_PENALTY_SCALE * peak_voltage_droop_penalty * droop_penalty_scalar)) /
        VOLTAGE_DROOP_SCALE;
    int predicted_ocv_eod = min_ocv(cfg) + avg_voltage_droop_mv +
                            ((voltage_buffer_weight * voltage_buffer) +
                             (peak_load_comp_weight * peak_voltage_droop_mv)) /
                                BATTERY_FULL_SCALE;
    int soc_eod = linear_interpolation(
        predicted_ocv_eod, cfg->ocv_table_mv, cfg->soc_table, METASOC_TABLE_SZ);

    // fill up the stats
    context->stats.fast_conv_threshold   = fast_conv_threshold;
    context->stats.voltage_buffer        = voltage_buffer;
    context->stats.peak_load_comp_weight = peak_load_comp_weight;
    context->stats.voltage_buffer_weight = voltage_buffer_weight;
    context->stats.ocv                   = ocv;
    context->stats.avg_voltage_droop_mv  = avg_voltage_droop_mv;
    context->stats.peak_voltage_droop_mv = peak_voltage_droop_mv;
    context->stats.predicted_ocv_eod     = predicted_ocv_eod;
    context->stats.soc_end_of_discharge  = soc_eod;
    context->stats.droop_penalty_scalar  = droop_penalty_scalar;
    context->stats.droop_penalty         = peak_voltage_droop_penalty;

    METASOC_LOG(
        "metasoc: voltage_buffer: %d, peak_load_comp_weight: %d, voltage_buffer_weight: %d, ocv: %d, avg_voltage_droop_mv: %d, peak_voltage_droop_mv: %d, predicted_ocv_eod:%d, soc_eod: %d, fast_conv_threshold: %d, peak_voltage_droop_penalty: %d, droop_penalty_scalar: %d, throttling_comparator: %d, prev_time: %lu, is_charging: %d, is_repsoc_zero: %d\n",
        voltage_buffer,
        peak_load_comp_weight,
        voltage_buffer_weight,
        ocv,
        avg_voltage_droop_mv,
        peak_voltage_droop_mv,
        predicted_ocv_eod,
        soc_eod,
        fast_conv_threshold,
        peak_voltage_droop_penalty,
        droop_penalty_scalar,
        params->low_voltage_comp_tripped,
        (long unsigned int)context->last_update_time_s,
        params->is_charging,
        params->is_repsoc_zero);

    return soc_eod;
}

static int calculate_soc_eod_using_qres(metasoc_config_data *cfg,
                                        metasoc_context *context,
                                        metasoc_param *params,
                                        int corrected_battery_level,
                                        uint16_t qres_filtered)
{
    const int QRES_SCALE = 100;
    // VEmpty buffer threshold for slowing down 0% convergfance in mV
    const int V_EMPTY_BUFFER         = 200;
    const int SOC_TOO_LOW_NORMALIZER = 4;
    int predicted_ocv_eod            = 0;

    int fast_conv_threshold = compute_fast_conv_threshold(cfg, params);

    int ocv = linear_interpolation(
        corrected_battery_level, cfg->soc_table, cfg->ocv_table_mv, METASOC_TABLE_SZ);

    int soc_too_high_convergence_scalar =
        MIN(MAX(100 - ((params->avg_voltage_mv - cfg->v_empty_mv) * 100) /
                          (fast_conv_threshold - cfg->v_empty_mv),
                0),
            100);
    int soc_too_low_convergence_scalar =
        MIN(MAX(((params->avg_voltage_mv - cfg->v_empty_mv - V_EMPTY_BUFFER) * 100) /
                    (fast_conv_threshold - cfg->v_empty_mv),
                0),
            100) /
        SOC_TOO_LOW_NORMALIZER;

    int soc_eod = (qres_filtered * QRES_SCALE) / params->full_capacity_nominal_mah;
    soc_eod     = soc_eod +
              (soc_too_high_convergence_scalar * (corrected_battery_level - soc_eod)) / 100 -
              (soc_too_low_convergence_scalar * soc_eod) / 100;

    predicted_ocv_eod =
        linear_interpolation(soc_eod, cfg->soc_table, cfg->ocv_table_mv, METASOC_TABLE_SZ);

    // fill up the stats
    context->stats.fast_conv_threshold   = fast_conv_threshold;
    context->stats.voltage_buffer        = 0;
    context->stats.peak_load_comp_weight = 0;
    context->stats.voltage_buffer_weight = 0;
    context->stats.ocv                   = ocv;
    context->stats.avg_voltage_droop_mv  = 0;
    context->stats.peak_voltage_droop_mv = 0;
    context->stats.predicted_ocv_eod     = predicted_ocv_eod;
    context->stats.soc_end_of_discharge  = soc_eod;
    context->stats.droop_penalty_scalar  = 0;
    context->stats.droop_penalty         = 0;

    METASOC_LOG(
        "metasoc:  soc_too_high_convergence_scalar: %d, soc_too_low_convergence_scalar: %d, ocv: %d, predicted_ocv_eod:%d, soc_eod: %d, fast_conv_threshold: %d, throttling_comparator: %d, prev_time: %lu, is_charging: %d, is_repsoc_zero: %d\n",
        soc_too_high_convergence_scalar,
        soc_too_low_convergence_scalar,
        ocv,
        predicted_ocv_eod,
        soc_eod,
        fast_conv_threshold,
        params->low_voltage_comp_tripped,
        (long unsigned int)context->last_update_time_s,
        params->is_charging,
        params->is_repsoc_zero);

    return soc_eod;
}

static int compute_metasoc(bool charge_soc_masking,
                           int enable_zero_repsoc_convergence,
                           uint16_t max_slew_rate,
                           int curr_metasoc,
                           bool is_charging,
                           bool is_fully_charged,
                           int peak_voltage_droop_penalty,
                           int usoc,
                           int usoc_filtered,
                           int time_delta_s,
                           uint16_t low_batt_thresh_hi,
                           uint16_t low_batt_thresh_lo,
                           bool is_battery_voltage_low,
                           uint32_t time_since_battery_voltage_low,
                           int battery_temp)
{
    const int BATTERY_FULL_2DEC           = 10000;
    const int BATTERY_99_PCT_HOLD         = 9900;
    const int BATTERY_1_PCT_HOLD          = 100;
    const int IDLE_DISCHARGE_TIME_DELTA_S = 1200;
    const int MAX_TIME_DELTA_S            = 10000;
    int new_metasoc                       = 0;
    int slew_rate                         = 0;
    int usoc_filtered_threshold =
        (enable_zero_repsoc_convergence == ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE) ? -1 : 100;

    if (is_fully_charged)
    {
        return BATTERY_FULL_2DEC;
    }

    if (time_delta_s < 0)
    {
        METASOC_LOG("metasoc: time delta was < 0, forcing to 0\n");
        time_delta_s = 0;
    }
    time_delta_s = MIN(time_delta_s, MAX_TIME_DELTA_S);

    if (enable_zero_repsoc_convergence == ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE)
    {
        slew_rate = calculate_slew_rate_using_qres(curr_metasoc,
                                                   max_slew_rate,
                                                   is_charging,
                                                   battery_temp,
                                                   is_battery_voltage_low,
                                                   time_since_battery_voltage_low);
    }
    else if (enable_zero_repsoc_convergence == CONVERGENCE_THROUGH_PEAK_POWER_MANAGER)
    {
        slew_rate = calculate_slew_rate_using_qres(
            curr_metasoc,
            max_slew_rate,
            is_charging,
            battery_temp,
            false, // No slew rate expediation for low voltage reached
            time_since_battery_voltage_low);
    }
    else
    {
        slew_rate = calculate_slew_rate_for_metasoc(enable_zero_repsoc_convergence,
                                                    peak_voltage_droop_penalty,
                                                    curr_metasoc,
                                                    max_slew_rate,
                                                    is_charging,
                                                    low_batt_thresh_hi,
                                                    low_batt_thresh_lo,
                                                    is_battery_voltage_low,
                                                    time_since_battery_voltage_low);
    }

    if (is_charging)
    {
        if (charge_soc_masking)
        {
            new_metasoc = linear_interpolation(usoc,
                                               metasoc_interpolation_table_x,
                                               metasoc_interpolation_table_y,
                                               METASOC_INTERPOLATION_TABLE_SZ);
        }
        else
        {
            new_metasoc = usoc;
        }

        if (time_delta_s < IDLE_DISCHARGE_TIME_DELTA_S)
        {
            new_metasoc = MAX(new_metasoc, curr_metasoc);
        }

        new_metasoc = MIN(new_metasoc, curr_metasoc + ((slew_rate * time_delta_s) / 60));

        METASOC_LOG("metasoc: slewrate: %d, time_delta_s: %d, new_metasoc: %d, curr_metasoc: %d\n",
                    slew_rate,
                    time_delta_s,
                    new_metasoc,
                    curr_metasoc);

        // 99% hold until fully charged
        if (!charge_soc_masking && (new_metasoc > BATTERY_99_PCT_HOLD) &&
            (curr_metasoc < BATTERY_FULL_2DEC))
        {
            new_metasoc = BATTERY_99_PCT_HOLD;
        }
    }
    else
    {
        int slewrate_offset = ((slew_rate * time_delta_s) / 60);
        new_metasoc         = linear_interpolation(MIN(usoc, usoc_filtered),
                                           metasoc_interpolation_table_x,
                                           metasoc_interpolation_table_y,
                                           METASOC_INTERPOLATION_TABLE_SZ);

        METASOC_LOG(
            "metasoc: slewrate: %d, slewrate_offset: %d, new_metasoc: %d, curr_metasoc: %d\n",
            slew_rate,
            slewrate_offset,
            new_metasoc,
            curr_metasoc);

        new_metasoc = MIN(new_metasoc, curr_metasoc);
        if (curr_metasoc == BATTERY_FULL_2DEC)
        {
            // due to full masking dt is not representative at 100% and it requires special handling
            new_metasoc = MAX(new_metasoc, curr_metasoc - 2 * BATTERY_1_PCT_HOLD);
        }
        else
        {
            new_metasoc = MAX(new_metasoc, curr_metasoc - slewrate_offset);
        }

        // 1% hold until shutdown
        if ((enable_zero_repsoc_convergence != ENABLE_IMMEDIATE_ZERO_REPSOC_CONVERGENCE) &&
            (new_metasoc < BATTERY_1_PCT_HOLD) && (usoc_filtered > usoc_filtered_threshold) &&
            !is_battery_voltage_low)
        {
            new_metasoc = BATTERY_1_PCT_HOLD;
        }
    }

    if (enable_zero_repsoc_convergence == ENABLE_IMMEDIATE_ZERO_REPSOC_CONVERGENCE &&
        is_battery_voltage_low)
    {
        new_metasoc = 0;
    }

    return new_metasoc;
}

static int calc_remaining_capacity(int corrected_battery_level,
                                   int soc_end_of_discharge,
                                   int full_capacity_nominal_mah,
                                   bool is_battery_voltage_low,
                                   bool block_discharge)
{
    const int BATTERY_FULL_2DEC = 10000;
    const int MIN_CAP           = 1;
    int remaining_capacity      = 0;

    if (!is_battery_voltage_low && !block_discharge)
    {
        remaining_capacity =
            MAX(((MAX(MIN((corrected_battery_level - soc_end_of_discharge), BATTERY_FULL_2DEC), 0) *
                  full_capacity_nominal_mah) /
                 BATTERY_FULL_2DEC),
                MIN_CAP);
    }

    return remaining_capacity;
}

void metasoc_deinit(void)
{
    uint8_t batt_id;
    for (batt_id = 0; batt_id < NUM_BATTERIES; batt_id++)
    {
        metasoc_context *context = &ctx[batt_id];
        if (context->is_init)
        {
            memset(context, 0, sizeof(metasoc_context));
        }
    }
    memset(&fusion_ctx, 0, sizeof(metasoc_fusion_context));
}

int metasoc_init(uint8_t batt_id, metasoc_config_data *config)
{
    metasoc_context *context;
    const int BATTERY_2DEC_SCALE = 100;
    uint8_t persist_data         = 0;

    if (!config)
    {
        METASOC_LOG("metasoc: requires valid config data\n");
        return -1;
    }

    if (batt_id >= NUM_BATTERIES)
    {
        METASOC_LOG("metasoc: requires valid battery ID\n");
        return -1;
    }
    context = &ctx[batt_id];

    if (!config->utility_function.get_time_in_sec)
    {
        METASOC_LOG("metasoc: requires a valid get time function\n");
        return -1;
    }

    if (!config->utility_function.read_persist_data)
    {
        METASOC_LOG("metasoc: requires read persist data function");
        return -1;
    }

    if (!config->utility_function.write_persist_data)
    {
        METASOC_LOG("metasoc: requires a valid write persist data function");
        return -1;
    }

    memcpy(&context->config, config, sizeof(metasoc_config_data));

    metasoc_utility_unpack_persist_data(
        config->utility_function.read_persist_data(config->private_data),
        &context->is_prev_low_battery_shutdown,
        &context->stored_batt_capacity,
        &context->metasoc_enabled,
        &context->oob_charging);

    // clear all persist data data except the enable bit
    persist_data = metasoc_utility_pack_persist_data(false, 0, context->metasoc_enabled, false);
    context->config.utility_function.write_persist_data(context->config.private_data, persist_data);

    // copy the last updated time at init
    context->last_update_time_s          = config->last_update_time_s;
    context->soc_end_of_charge           = calculate_soc_eoc(config);
    context->stats.soc_end_of_charge     = context->soc_end_of_charge;
    context->usoc_filtered               = INT_MIN;
    context->qres_filtered               = INT_MIN;
    context->is_charging                 = -1;
    context->peak_voltage_droop_penalty  = 0;
    context->is_battery_voltage_low      = false;
    context->time_is_battery_voltage_low = 0;
    context->is_init                     = true;

    if (context->is_prev_low_battery_shutdown)
    {
        context->metasoc = (int)context->config.soc_init_val * BATTERY_2DEC_SCALE;
        context->last_update_time_s =
            context->config.utility_function.get_time_in_sec(context->config.private_data);
    }
    else
    {
        context->metasoc = (int)config->last_metasoc * BATTERY_2DEC_SCALE;
    }

    if (batt_id == 0)
    {
        fusion_ctx.config             = config;
        fusion_ctx.metasoc            = config->last_metasoc * BATTERY_2DEC_SCALE;
        fusion_ctx.last_update_time_s = config->last_update_time_s;
        fusion_ctx.usoc_filtered      = INT_MIN;
        fusion_ctx.is_charging        = true;
    }
    else
    {
        // Update last update time for battery 1 and beyond to be current time wrt its own clock to
        // avoid negative time deltas
        context->last_update_time_s =
            context->config.utility_function.get_time_in_sec(context->config.private_data);
    }

    if (context->config.enable_zero_repsoc_convergence == CONVERGENCE_THROUGH_PEAK_POWER_MANAGER)
    {
        // relaxed usoc_gain to reduce brownout risk
        context->config.usoc_gain = 80;
    }

    METASOC_LOG("metasoc: successfully init, low battery shutdown: %d, oob_charging: %d, repsoc_convergence: %d\n",
                context->is_prev_low_battery_shutdown,
                context->oob_charging,
            context->config.enable_zero_repsoc_convergence);

    return 0;
}

int metasoc_update_soc(uint8_t batt_id, metasoc_param *params)
{
    metasoc_context *context;
    const int BATTERY_SCALE_100             = 100;
    const int BATTERY_EMPTY                 = 0;
    const int CORRECTED_PCT_SCALE           = 10000;
    const int DIRECTION_SWITCH_TIME_DELTA_S = 20;
    int time_delta_s                        = 0;
    int new_metasoc                         = 0;
    int soc_end_of_discharge                = 0;
    int usoc                                = 0;
    uint32_t time_since_battery_voltage_low = 0;
    uint32_t time                           = 0;

    if (batt_id >= NUM_BATTERIES)
    {
        METASOC_LOG("metasoc: requires valid battery ID\n");
        return -1;
    }
    context = &ctx[batt_id];

    if (!context->is_init)
    {
        METASOC_LOG("metasoc: error: metasoc not init\n");
        return -1;
    }

    if (!params)
    {
        return -1;
    }

    if (context->oob_charging && !context->is_prev_low_battery_shutdown)
    {
        params->is_charging   = true;
        context->oob_charging = false;
    }

    // initialize the is_charging flag
    if (context->is_charging == -1)
    {
        context->is_charging = params->is_charging;
    }

    time         = context->config.utility_function.get_time_in_sec(context->config.private_data);
    time_delta_s = time - context->last_update_time_s;
    context->charge_transition = context->is_charging != params->is_charging;
    // if the battery switches between charging and discharging, we need to reset time delta so it
    // doesn't contrbute to a jump in metasoc
    if (NUM_BATTERIES == 1 && context->charge_transition &&
        time_delta_s > DIRECTION_SWITCH_TIME_DELTA_S)
    {
        context->last_update_time_s =
            context->config.utility_function.get_time_in_sec(context->config.private_data);
        time_delta_s         = 0;
        context->is_charging = params->is_charging;
    }
    else if (NUM_BATTERIES > 1 && context->charge_transition)
    {
        context->last_update_time_s =
            context->config.utility_function.get_time_in_sec(context->config.private_data);
        time_delta_s         = 0;
        context->is_charging = params->is_charging;
    }
    context->stats.time_delta_s   = time_delta_s;
    context->is_fully_charged     = params->is_fully_charged;
    context->battery_capacity_mah = params->battery_capacity_mah;
    context->avg_voltage_mv       = params->avg_voltage_mv;
    context->temp_c               = params->temp_c;

    METASOC_LOG(
        "metasoc: BATT%d: time: %lu, last_time: %lu, time_delta: %d, param_chg: %d, ctx_chg: %d\n",
        batt_id,
        (long unsigned int)time,
        (long unsigned int)context->last_update_time_s,
        time_delta_s,
        params->is_charging,
        context->is_charging);

    if (context->is_prev_low_battery_shutdown)
    {
        context->is_prev_low_battery_shutdown = false;
        new_metasoc                           = context->metasoc;
    }
    else
    {
        int pmu_voltage_estimate = calculate_pmu_voltage_estimate(&context->config, params);

        int corrected_battery_level =
            (context->config.enable_zero_repsoc_convergence ==
             ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE)
                ? params->battery_level
                : params->battery_level + context->config.csoc_correction_pct +
                      ((params->battery_level * context->config.csoc_gain) / CORRECTED_PCT_SCALE);
        int peak_voltage_droop_penalty = 0;
        int droop_penalty_scalar       = 0;

        if (context->config.enable_zero_repsoc_convergence ==
            ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE)
        {
            peak_voltage_droop_penalty = 0;
        }
        else if (context->config.enable_zero_repsoc_convergence ==
                 CONVERGENCE_THROUGH_PEAK_POWER_MANAGER)
        {
            peak_voltage_droop_penalty =
                compute_peak_voltage_droop_penalty(&context->config,
                                                   params->is_charging,
                                                   false, // Do not react to comparator tripped
                                                   context->peak_voltage_droop_penalty,
                                                   pmu_voltage_estimate);
        }
        else
        {
            peak_voltage_droop_penalty =
                compute_peak_voltage_droop_penalty(&context->config,
                                                   params->is_charging,
                                                   params->low_voltage_comp_tripped,
                                                   context->peak_voltage_droop_penalty,
                                                   pmu_voltage_estimate);
        }

        droop_penalty_scalar =
            calculate_droop_penalty_scalar(context->config.droop_penalty_coefficient,
                                           context->metasoc,
                                           context->config.low_battery_alert_threshold_high,
                                           context->config.low_battery_alert_threshold_low);

        if (context->qres_filtered == INT_MIN)
        {
            context->qres_filtered = params->qres * 100;
        }
        context->qres_filtered = (context->config.qres_gain * context->qres_filtered +
                                  (100 - context->config.qres_gain) * params->qres * 100) /
                                 100;
        soc_end_of_discharge = (context->config.enable_zero_repsoc_convergence ==
                                ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE)
                                   ? calculate_soc_eod_using_qres(&context->config,
                                                                  context,
                                                                  params,
                                                                  corrected_battery_level,
                                                                  context->qres_filtered)
                                   : calculate_soc_eod(&context->config,
                                                       context,
                                                       params,
                                                       corrected_battery_level,
                                                       peak_voltage_droop_penalty,
                                                       droop_penalty_scalar);

        context->remaining_capacity = calc_remaining_capacity(corrected_battery_level,
                                                              soc_end_of_discharge,
                                                              params->full_capacity_nominal_mah,
                                                              context->is_battery_voltage_low,
                                                              params->block_discharge);
        usoc                        = calculate_usoc(&context->config,
                              soc_end_of_discharge,
                              context->soc_end_of_charge,
                              corrected_battery_level,
                              pmu_voltage_estimate,
                              params->ocv_mv,
                              params->is_repsoc_zero,
                              params->is_charging,
                              &context->is_battery_voltage_low,
                              &context->time_is_battery_voltage_low);

        context->peak_voltage_droop_penalty = peak_voltage_droop_penalty;

        if (context->usoc_filtered == INT_MIN)
        {
            context->usoc_filtered = usoc;
        }
        context->usoc_filtered = (context->config.usoc_gain * context->usoc_filtered +
                                  (100 - context->config.usoc_gain) * usoc) /
                                 100;

        context->stats.corrected_battery_level = corrected_battery_level;
        context->stats.usoc                    = usoc;
        context->stats.usoc_filtered           = context->usoc_filtered;
        context->stats.remaining_capacity      = context->remaining_capacity;

        if (NUM_BATTERIES == 1)
        {
            METASOC_LOG(
                "metasoc: corrected battery level: %d, end of discharge: %d, end of charge: %d, nom_cap: %d, usoc_filtered: %d\n",
                corrected_battery_level,
                soc_end_of_discharge,
                context->soc_end_of_charge,
                params->full_capacity_nominal_mah,
                context->usoc_filtered);

            time_since_battery_voltage_low =
                context->config.utility_function.get_time_in_sec(context->config.private_data) -
                context->time_is_battery_voltage_low;
            new_metasoc = compute_metasoc(context->config.charge_soc_masking,
                                          context->config.enable_zero_repsoc_convergence,
                                          context->config.max_slew_rate,
                                          context->metasoc,
                                          params->is_charging,
                                          params->is_fully_charged,
                                          context->peak_voltage_droop_penalty,
                                          usoc,
                                          context->usoc_filtered,
                                          time_delta_s,
                                          context->config.low_battery_alert_threshold_high,
                                          context->config.low_battery_alert_threshold_low,
                                          context->is_battery_voltage_low,
                                          time_since_battery_voltage_low,
                                          params->temp_c);
        }
    }

    if (NUM_BATTERIES == 1)
    {
        if (new_metasoc != context->metasoc)
        {
            context->last_update_time_s =
                context->config.utility_function.get_time_in_sec(context->config.private_data);
            context->metasoc = new_metasoc;
        }

        // ensure to write shutdown persist data
        if (CAP_0_TO_100(context->metasoc / BATTERY_SCALE_100) == BATTERY_EMPTY)
        {
            metasoc_set_low_battery_shutdown(batt_id, params->battery_capacity_mah);
        }
    }

    return 0;
}

int metasoc_get_soc(uint8_t batt_id, uint8_t *soc)
{
    metasoc_context *context;
    // used to scale 2 decimal point scale to integer
    const int BATTERY_SCALE_100 = 100;

    if (batt_id != BATT_ID_0)
    {
        METASOC_LOG("metasoc: requires valid battery ID\n");
        return -1;
    }
    context = &ctx[batt_id];

    if (!context->is_init)
    {
        return -1;
    }

    if (!soc)
    {
        return -1;
    }

    *soc = CAP_0_TO_100(context->metasoc / BATTERY_SCALE_100);

    return 0;
}

int metasoc_get_internal_stats_fused(metasoc_internal_stats_fused *stats)
{
    if (!stats)
    {
        return -1;
    }

    stats->fused_usoc                       = fusion_ctx.fused_usoc;
    stats->fused_usoc_filtered              = fusion_ctx.usoc_filtered;
    stats->fused_peak_voltage_droop_penalty = fusion_ctx.peak_voltage_droop_penalty;

    return 0;
}

int metasoc_get_fused_soc(uint16_t *fused_soc)
{
    const int DIRECTION_SWITCH_TIME_DELTA_S = 20;
    const int BATTERY_SCALE_100             = 100;
    bool is_charging                        = true;
    bool is_fully_charged                   = true;
    bool is_battery_voltage_low             = true;
    bool charge_transition                  = true;
    bool prev_low_battery_shutdown          = true;
    int usoc_fused                          = 0;
    int total_capacity                      = 0;
    uint8_t batt_id                         = 0;
    int time_delta_s                        = 0;
    int metasoc                             = 0;
    uint16_t avg_voltage_fused_mv           = 0;
    int8_t temp_fused_c                     = 100;
    uint32_t time_since_battery_voltage_low = 0;

    if (NUM_BATTERIES == 1)
        return -1;

    time_delta_s =
        fusion_ctx.config->utility_function.get_time_in_sec(fusion_ctx.config->private_data) -
        fusion_ctx.last_update_time_s;

    for (batt_id = 0; batt_id < NUM_BATTERIES; batt_id++)
    {
        is_charging            = is_charging && ctx[batt_id].is_charging;
        is_fully_charged       = is_fully_charged && ctx[batt_id].is_fully_charged;
        is_battery_voltage_low = is_battery_voltage_low && ctx[batt_id].is_battery_voltage_low;

        // if prev low battery shutdown then usoc is not updated and stays at INT_MIN
        prev_low_battery_shutdown =
            prev_low_battery_shutdown && ctx[batt_id].usoc_filtered == INT_MIN;
        fusion_ctx.peak_voltage_droop_penalty =
            MIN(fusion_ctx.peak_voltage_droop_penalty, ctx[batt_id].peak_voltage_droop_penalty);
        usoc_fused     = usoc_fused + ctx[batt_id].stats.usoc * ctx[batt_id].remaining_capacity;
        total_capacity = total_capacity + ctx[batt_id].remaining_capacity;
        avg_voltage_fused_mv = MAX(avg_voltage_fused_mv, ctx[batt_id].avg_voltage_mv);
        temp_fused_c         = MIN(temp_fused_c, ctx[batt_id].temp_c);

        METASOC_LOG(
            "metasoc: BATT%d: charging: %d, charged: %d, droop_penalty: %d, voltage_low: %d, remcap: %d\n",
            batt_id,
            ctx[batt_id].is_charging,
            ctx[batt_id].is_fully_charged,
            ctx[batt_id].peak_voltage_droop_penalty,
            ctx[batt_id].is_battery_voltage_low,
            ctx[batt_id].remaining_capacity);
    }
    charge_transition = fusion_ctx.is_charging != is_charging;
    // if the battery switches between charging and discharging, we need to reset time delta so
    // it doesn't contrbute to a jump in metasoc
    if (charge_transition && time_delta_s > DIRECTION_SWITCH_TIME_DELTA_S)
    {
        time_delta_s           = 0;
        fusion_ctx.is_charging = is_charging;
    }

    if (total_capacity > 0)
    {
        usoc_fused = usoc_fused / total_capacity;
    }
    fusion_ctx.fused_usoc = usoc_fused;

    if (fusion_ctx.usoc_filtered == INT_MIN)
    {
        fusion_ctx.usoc_filtered = usoc_fused;
    }
    fusion_ctx.usoc_filtered = (fusion_ctx.config->usoc_gain * fusion_ctx.usoc_filtered +
                                (100 - fusion_ctx.config->usoc_gain) * usoc_fused) /
                               100;
    time_since_battery_voltage_low =
        MIN((ctx[0].config.utility_function.get_time_in_sec(ctx[0].config.private_data) -
             ctx[0].time_is_battery_voltage_low),
            (ctx[1].config.utility_function.get_time_in_sec(ctx[1].config.private_data) -
             ctx[1].time_is_battery_voltage_low));

    METASOC_LOG(
        "metasoc: charging: %d, charged: %d, droop_penalty: %d, voltage_low: %d, curr_metasoc:%d, time_since_batt_low: %lu\n",
        fusion_ctx.is_charging,
        is_fully_charged,
        fusion_ctx.peak_voltage_droop_penalty,
        is_battery_voltage_low,
        fusion_ctx.metasoc,
        (long unsigned int)time_since_battery_voltage_low);

    if (prev_low_battery_shutdown)
    {
        metasoc = ctx[0].config.soc_init_val * BATTERY_SCALE_100;
    }
    else
    {
        metasoc = compute_metasoc(fusion_ctx.config->charge_soc_masking,
                                  fusion_ctx.config->enable_zero_repsoc_convergence,
                                  fusion_ctx.config->max_slew_rate,
                                  fusion_ctx.metasoc,
                                  is_charging,
                                  is_fully_charged,
                                  fusion_ctx.peak_voltage_droop_penalty,
                                  usoc_fused,
                                  fusion_ctx.usoc_filtered,
                                  time_delta_s,
                                  fusion_ctx.config->low_battery_alert_threshold_high,
                                  fusion_ctx.config->low_battery_alert_threshold_low,
                                  is_battery_voltage_low,
                                  time_since_battery_voltage_low,
                                  temp_fused_c);
    }

    METASOC_LOG("metasoc: usoc_fused: %d, usoc_filtered: %d, time_delta_s: %d, metasoc: %d\n",
                usoc_fused,
                fusion_ctx.usoc_filtered,
                time_delta_s,
                metasoc);

    // Update the last update time for fusion context only if the metasoc has changed
    if (metasoc != fusion_ctx.metasoc || time_delta_s == 0)
    {
        fusion_ctx.last_update_time_s =
            fusion_ctx.config->utility_function.get_time_in_sec(fusion_ctx.config->private_data);
        fusion_ctx.metasoc = metasoc;
        // Also update the last update time for individual context

        for (batt_id = 0; batt_id < NUM_BATTERIES; batt_id++)
        {
            ctx[batt_id].last_update_time_s = ctx[batt_id].config.utility_function.get_time_in_sec(
                ctx[batt_id].config.private_data);
        }
    }

    // Update individual context with updated fusion context
    for (batt_id = 0; batt_id < NUM_BATTERIES; batt_id++)
    {
        ctx[batt_id].metasoc                    = metasoc;
        ctx[batt_id].peak_voltage_droop_penalty = fusion_ctx.peak_voltage_droop_penalty;
    }

    // ensure to write shutdown persist data
    if (CAP_0_TO_100(metasoc / BATTERY_SCALE_100) == 0)
    {
        metasoc_set_low_battery_shutdown(0, ctx[0].battery_capacity_mah);
        metasoc_set_low_battery_shutdown(1, ctx[1].battery_capacity_mah);
    }

    *fused_soc = metasoc / BATTERY_SCALE_100;

    return 0;
}

int metasoc_set_low_battery_shutdown(uint8_t batt_id, uint16_t battery_capacity_mah)
{
    metasoc_context *context;
    uint8_t persist_data = 0;

    if (batt_id >= NUM_BATTERIES)
    {
        METASOC_LOG("metasoc: requires valid battery ID\n");
        return -1;
    }
    context = &ctx[batt_id];

    if (!context->is_init)
    {
        return -1;
    }

    persist_data = metasoc_utility_pack_persist_data(
        true, battery_capacity_mah, context->metasoc_enabled, false);
    context->config.utility_function.write_persist_data(context->config.private_data, persist_data);
    context->stats.is_battery_empty = true;

    return 0;
}

int metasoc_get_internal_stats(uint8_t batt_id, metasoc_internal_stats *stats)
{
    metasoc_context *context;
    if (batt_id >= NUM_BATTERIES)
    {
        METASOC_LOG("metasoc: requires valid battery ID\n");
        return -1;
    }
    context = &ctx[batt_id];

    if (!context->is_init)
    {
        return -1;
    }

    if (!stats)
    {
        return -1;
    }

    memcpy(stats, &context->stats, sizeof(metasoc_internal_stats));

    return 0;
}

metasoc_config_id metasoc_get_config_id(uint8_t batt_id)
{
    metasoc_context *context;
    if (batt_id >= NUM_BATTERIES)
    {
        METASOC_LOG("metasoc: requires valid battery ID\n");
        return METASOC_CONFIG_ID_ERROR;
    }
    context = &ctx[batt_id];
    return context->config.config_id;
}
