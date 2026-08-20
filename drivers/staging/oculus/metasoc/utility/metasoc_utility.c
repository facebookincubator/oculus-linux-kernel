// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "metasoc_utility.h"
#include "metasoc_platform.h"

#define PERSIST_DATA_MASK 0x3F
// Format for persistent data
// bit 0 - low battery shutdown bit
// bit 1 - 3 - last capacity
// bit 4 - metasoc enabled bit
// bit 5 - out of band charging

#define LOW_BATTERY_SHUTDOWN_MASK 0x1
#define LOW_BATTERY_SHUTDOWN_SHIFT 0

#define CAPACITY_MASK 0x7
#define CAPACITY_SHIFT 1

#define METASOC_ENABLED_MASK 0x1
#define METASOC_ENABLED_SHIFT 4

#define OUT_OF_BAND_CHARGING_MASK 0x1
#define OUT_OF_BAND_CHARGING_SHIFT 5

// LSB for saved battery capacity is 5mAh
#define BATTERY_CAPACITY_MAH_LSB 5
#define MAX_STORED_BATTERY_CAPACITY 35

int metasoc_utility_ceil_div(int value, int div)
{
    return (value / div) + ((value % div) > 0);
}

uint8_t metasoc_utility_pack_persist_data(bool low_battery_shutdown,
                                          uint16_t battery_capacity,
                                          bool metasoc_enabled,
                                          bool oob_charging)
{
    uint8_t persist_data = low_battery_shutdown & LOW_BATTERY_SHUTDOWN_MASK;
    uint8_t capacity     = 0;
    battery_capacity     = MIN(battery_capacity, MAX_STORED_BATTERY_CAPACITY);

    capacity = metasoc_utility_ceil_div(battery_capacity, BATTERY_CAPACITY_MAH_LSB) & CAPACITY_MASK;
    persist_data |= capacity << CAPACITY_SHIFT;
    persist_data |= ((metasoc_enabled & METASOC_ENABLED_MASK) << METASOC_ENABLED_SHIFT);
    persist_data |= ((oob_charging & OUT_OF_BAND_CHARGING_MASK) << OUT_OF_BAND_CHARGING_SHIFT);

    return persist_data & PERSIST_DATA_MASK;
}

void metasoc_utility_unpack_persist_data(uint8_t persist_data,
                                         bool *low_battery_shutdown,
                                         uint8_t *battery_capacity,
                                         bool *metasoc_enabled,
                                         bool *oob_charging)
{
    if (low_battery_shutdown)
    {
        *low_battery_shutdown = persist_data & LOW_BATTERY_SHUTDOWN_MASK;
    }

    if (battery_capacity)
    {
        *battery_capacity = (persist_data >> CAPACITY_SHIFT) & CAPACITY_MASK;
        *battery_capacity *= BATTERY_CAPACITY_MAH_LSB;
    }

    if (metasoc_enabled)
    {
        *metasoc_enabled = (persist_data >> METASOC_ENABLED_SHIFT) & METASOC_ENABLED_MASK;
    }

    if (oob_charging)
    {
        *oob_charging = (persist_data >> OUT_OF_BAND_CHARGING_SHIFT) & OUT_OF_BAND_CHARGING_MASK;
    }
}
