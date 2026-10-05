/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*******************************************************************************
 * @file arfw_debug.h
 *
 * @brief Interface definition for binary formats exposed by arfw debug.
 *
 * @details
 *
 *******************************************************************************/

#pragma once

#include <linux/arfw_types.h>

#ifndef __KERNEL__
#include <stdbool.h>
#include <sys/ioctl.h>
#define TASK_COMM_LEN 16
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif // !__packed
#else // __KERNEL__
#include <linux/ioctl.h>
#include <linux/sched.h>
#endif // !__KERNEL__

struct __packed ar_queue_sampler_stats {
	uint64_t num_samples;
	uint32_t min;
	uint32_t max;
	uint32_t p50;
	uint32_t p90;
	uint32_t p95;
	uint32_t p99;
};

struct __packed ar_queue_debug {
	uint32_t id;
	ar_endpoint_id_t hlos_endpoint_id;
	ar_endpoint_id_t fw_endpoint_id;
	enum ar_queue_direction direction;
	bool mirror;
	uint16_t depth;

	uint16_t consumer_idx;
	uint16_t producer_idx;
	uint16_t producer_inflight;
	uint16_t consumer_iteration;
	uint16_t producer_iteration;

	uint32_t pid;
	char comm[TASK_COMM_LEN];
	uint64_t creation_ts;
	uint64_t destruction_ts;
	uint32_t shutdown_reason;
	uint64_t msg_count;
	uint64_t inline_count;
	uint64_t external_count;
	uint64_t drop_count;
	uint64_t consumed_count;
	uint64_t msg_activity_last_ts;
	uint64_t pend_count;
	uint64_t pend_error_count;
	uint64_t pend_active_count;
	uint64_t pend_req_count;
	uint64_t pend_last_ts;
	struct ar_queue_sampler_stats sys_latency_stats;
	uint64_t latency_threshold_last_ts;
	struct ar_queue_sampler_stats user_latency_stats;
	uint64_t user_latency_threshold_last_ts;
	struct ar_queue_sampler_stats capacity_stats;
};

struct __packed ar_queue_region_info {
	uint32_t size;
	uint16_t id;
	uint8_t location;
	uint8_t padding;
};
