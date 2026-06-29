// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define CREATE_TRACE_POINTS
#include "trace-bus-prof.h"


EXPORT_TRACEPOINT_SYMBOL(memory_miss_last_sample);
EXPORT_TRACEPOINT_SYMBOL(llcc_occupancy_last_sample);
EXPORT_TRACEPOINT_SYMBOL(memory_lat_last_sample);

MODULE_LICENSE("GPL");
