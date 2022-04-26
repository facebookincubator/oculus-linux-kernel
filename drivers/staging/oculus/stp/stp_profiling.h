/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef STP_PROFILING_H
#define STP_PROFILING_H

#include "stp_os.h"

#define STP_PROFILING_DECLARE_AVERAGE(label) \
u64 stpp_start_time_us_##label; \
u64 stpp_end_time_us_##label; \
static u64 stpp_total_time_us_##label; \
static unsigned int stpp_count_##label

#define STP_PROFILING_DECLARE_COUNT(label) \
u64 stpp_start_time_us_##label; \
static unsigned int stpp_count_##label

#define STP_PROFILING_DECLARE_EXCEEDED(label) \
u64 stpp_start_time_us_##label; \
u64 stpp_end_time_us_##label; \
static unsigned int stpp_counter_exceeded_##label; \
static unsigned int stpp_counter_total_##label

#define STP_PROFILING_DECLARE_TOTAL(label) \
u64 stpp_start_time_us_##label; \
static unsigned int stpp_total_##label

#define STP_PROFILING_DECLARE_MONITOR(label) \
u64 stpp_start_time_us_##label

#define STP_PROFILING_GET_TIME(time) \
{ \
	struct timespec crt_time = {0}; \
	get_monotonic_boottime(&crt_time); \
	time = crt_time.tv_sec*1000*1000 + crt_time.tv_nsec/1000; \
}

#define STP_PROFILING_START(label) \
{ \
	STP_PROFILING_GET_TIME(stpp_start_time_us_##label); \
}

#define STP_PROFILING_END(label) \
{ \
	STP_PROFILING_GET_TIME(stpp_end_time_us_##label); \
}

// This reports the average time between STP_PROFILING_START
// and STP_PROFILING_END for n_count iterations
#define STP_PROFILING_REPORT_AVERAGE(label, message, n_count) \
do { \
	if (stpp_end_time_us_##label == 0) \
		STP_PROFILING_END(lebel); \
	if (stpp_count_##label < n_count) { \
		stpp_total_time_us_##label += \
			(stpp_end_time_us_##label - \
			stpp_start_time_us_##label); \
		stpp_count_##label++; \
	} else { \
		STP_LOG_INFO("%s: %lu us (per %d iterations)\n", \
			message, \
			stpp_total_time_us_##label/stpp_count_##label, \
			stpp_count_##label); \
		stpp_total_time_us_##label = 0; \
		stpp_count_##label = 0; \
	} \
} while (0)

// This report the number of iterations in a period of time
#define STP_PROFILING_REPORT_COUNT(label, message, period_ms) \
do { \
	u64 crt_time_us; \
	\
	if (stpp_start_time_us_##label == 0) { \
		STP_PROFILING_START(label); \
	} \
	STP_PROFILING_GET_TIME(crt_time_us); \
	if (crt_time_us - stpp_start_time_us_##label < period_ms) { \
		stpp_count_##label++; \
	} else { \
		STP_LOG_INFO("%s: %d/%d ms\n", message, \
			stpp_count_##label, period_ms); \
		stpp_count_##label = 0; \
		stpp_start_time_us_##label = crt_time_us; \
} while (0)

// This report when time between STP_PROFILING_START and
// STP_PROFILING_END exceed max value
#define STP_PROFILING_REPORT_EXCEEDED(label, message, limit_us) \
do { \
	unsigned int over_us; \
	\
	if (stpp_end_time_us_##label == 0) { \
		STP_PROFILING_END(label); \
	} \
	stpp_counter_total_##label++; \
	if ((stpp_end_time_us_##label - \
		stpp_start_time_us_##label >= limit_us) { \
		stpp_counter_exceeded_##label++; \
		over_us = (unsigned int) \
			(stpp_end_time_us_##label \
			- stpp_start_time_us_##label; \
			STP_LOG_INFO("%s %d us: %d us (%d out of %d)\n", \
				message, limit_us, over_us, \
				stpp_freq_##label, \
			stpp_counter_exceeded_##label, \
				stpp_counter_total_##label); \
	} \
} while (0)

// This report the total sym of values in a period of time
#define STP_PROFILING_REPORT_TOTAL(label, message, period_us, value) \
do { \
	u64 crt_time_us; \
	\
	if (stpp_start_time_us_##label == 0) { \
		STP_PROFILING_START(label); \
	} \
	STP_PROFILING_GET_TIME(crt_time_us); \
	if (crt_time_us - stpp_start_time_us_##label <= period_us) { \
		stpp_total_##label += value; \
	} else { \
		STP_LOG_INFO("%s: %d/%d us\n", message, stpp_total, \
			period_us); \
		stpp_total_##label = 0; \
		stpp_start_time_us_##label = crt_time_us; \
	} \
} while (0)

// This monitor a value and report it every sec or when it exceed a limit
#define STP_PROFILING_REPORT_MONITOR_OVER(label, message, value, period_us, limit) \
do { \
	u64 crt_time_us; \
	\
	if (stpp_start_time_us_##label == 0) { \
		STP_PROFILING_START(label); \
	} \
	STP_PROFILING_GET_TIME(crt_time_us); \
	if (crt_time_us - stpp_start_time_us_##label > period_us) { \
		STP_LOG_INFO("%s: %d\n", message, value); \
		stpp_start_time_us_##label = ctrt_time_us; \
	} else if (value > limit) { \
		STP_LOG_INFO("%s: %d\n", message, value); \
	} \
} while (0)

#endif
