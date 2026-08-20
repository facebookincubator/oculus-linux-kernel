/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) Meta Platforms, Inc. and affiliates. */

/* linux kernel cannot include c standard headers */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function definition used to get current time in seconds
 */
typedef uint32_t (*metasoc_get_time_in_sec_func)(void *);

/**
 * Function definition used to read persist data for metasoc
 */
typedef uint8_t (*metasoc_read_persist_data_func)(void *);

/**
 * Function definition used to write persist data for metasoc
 */
typedef void (*metasoc_write_persist_data_func)(void *, uint8_t);

typedef struct
{
    // function implementation for getting time
    metasoc_get_time_in_sec_func get_time_in_sec;
    // function implementation for reading persist data
    metasoc_read_persist_data_func read_persist_data;
    // function implementation for writing persist data
    metasoc_write_persist_data_func write_persist_data;
} metasoc_utility_function_impl;

/**
 * \brief Packs data for writing to persist memory
 * \param low_battery_shutdown whether shutdown was caused due to low battery
 * \param battery_capacity battery capacity to be packed
 * \return packed data to be written
 */
uint8_t metasoc_utility_pack_persist_data(bool low_battery_shutdown,
                                          uint16_t battery_capacity,
                                          bool metasoc_enabled,
                                          bool oob_charging);

/**
 * \brief Unpacks data written to persist memory
 * \param persist_data persist data to be unpacked
 * \param low_battery_shutdown unpacked from persist_data
 * \param battery_capacity unpacked from persist_data
 */
void metasoc_utility_unpack_persist_data(uint8_t persist_data,
                                         bool *low_battery_shutdown,
                                         uint8_t *battery_capacity,
                                         bool *metasoc_enabled,
                                         bool *oob_charging);

/**
 * \brief Divides value by div and returns the ceiling of the result
 * \param value value to be divided
 * \param div value will be divided by div
 */
int metasoc_utility_ceil_div(int value, int div);

#ifdef __cplusplus
}
#endif
