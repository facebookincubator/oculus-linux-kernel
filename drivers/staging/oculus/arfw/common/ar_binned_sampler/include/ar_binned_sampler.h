/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file ar_binned_sampler.h
 *
 * @brief Binned sampler header. A binned sampler is for sampling a stream of
 * integers and recording approximate information about their distribution.
 * How much information can be retained depends on the thresholds chosen - they
 * should differentiate the expected input stream with as much granularity as
 * the caller desires. Memory allocation is left to the caller.
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#pragma once

#include <ar_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ar_binned_sampler {
  int min;
  int max;
  int num_samples;
  int num_thresholds;
  size_t thresholds_offset;
  size_t counts_offset;
  size_t sums_offset;
} ar_binned_sampler_t;

/**
 * The required size is calculated to hold:
 * - ar_binned_sampler_t
 * - num_thresholds integer values to store the thresholds
 * - num_thresholds + 1 uint64 values to store the counter values
 * - num_thresholds + 1 uint64 values to store the sum values
 */

#define AR_BINNED_SAMPLER_SIZE(num) \
  (sizeof(ar_binned_sampler_t) + sizeof(int) * (num) + sizeof(uint64_t) * ((num) + 1) * 2)

/**
 * Returs the array of threshold values.
 *
 * @retval Array of threshold values.
 */
int* ar_binned_get_thresholds(const ar_binned_sampler_t* bs);

/**
 * Returs the array of count values.
 *
 * @retval Array of count values.
 */
uint64_t* ar_binned_get_counts(const ar_binned_sampler_t* bs);

/**
 * Returs the array of sums.
 *
 * @retval Array of sum values.
 */
uint64_t* ar_binned_get_sums(const ar_binned_sampler_t* bs);

/**
 * Initialize a binned sampler.
 *
 * @param[in] buf Pointer to the binned sampler to initialize.
 * @param[in] num_thresholds Number of thresholds to use.
 * @param[in] thresholds Array of threshold values.
 */
void ar_binned_sampler_init(void* sampler, const int* thresholds, int num_thresholds);

/**
 * Add a sample to the binned sampler. Sample is assumed to be in the range
 * [0, INT_MAX].
 *
 * @param[in] buf Pointer to the binned sampler to update.
 * @param[in] sample Sample value to add.
 */
void ar_binned_sampler_add_sample(void* sampler, int sample);

/**
 * Gets the threshold for the given percentile. This means that the values up to
 * the requested percentile all were less than the threshold value. This can
 * also return the maximum seen value if the value for the percentile is larger
 * than the largest threshold value.
 *
 * @param[in] buf Pointer to the binned sampler to query.
 * @param[in] percentile Percentile to query.
 * @retval Threshold value for the given percentile. -1 if it doesn't exist.
 */
int ar_binned_sampler_get_threshold(const void* sampler, int percentile);

int ar_binned_sampler_get_min(const void* sampler);

int ar_binned_sampler_get_max(const void* sampler);

uint64_t ar_binned_sampler_get_num_samples(const void* sampler);

#ifdef __cplusplus
} // extern "C"
#endif
