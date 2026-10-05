/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file ar_binned_sampler.c
 *
 * @brief Binned sampler implementation. Memory allocation is left to the caller.
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#include "ar_binned_sampler.h"

int* ar_binned_get_thresholds(const ar_binned_sampler_t* bs) {
  return (int*)((uintptr_t)bs + bs->thresholds_offset);
}

uint64_t* ar_binned_get_counts(const ar_binned_sampler_t* bs) {
  return (uint64_t*)((uintptr_t)bs + bs->counts_offset);
}

uint64_t* ar_binned_get_sums(const ar_binned_sampler_t* bs) {
  return (uint64_t*)((uintptr_t)bs + bs->sums_offset);
}

void ar_binned_sampler_init(void* sampler, const int* init_thresholds, int num_thresholds) {
  ar_binned_sampler_t* bs = (ar_binned_sampler_t*)sampler;
  int* thresholds;
  uint64_t* counts;
  uint64_t* sums;

  AR_ASSERT(bs != NULL);

  bs->min = -1;
  bs->max = -1;
  bs->num_samples = 0;
  bs->num_thresholds = num_thresholds;

  /**
   * Memory layout of a ar_binned_sampler_t is first the struct ar_binned_sampler, followed by the
   * threshold values, followed by the bin counts. Note that there is exactly 1 more bin counter
   * than number of thresholds. (to account for values above the maximum threshold)
   */
  bs->thresholds_offset = sizeof(ar_binned_sampler_t);
  thresholds = ar_binned_get_thresholds(bs);
  // copy threshold values to the proper location
  memcpy(thresholds, init_thresholds, sizeof(int) * num_thresholds);

  bs->counts_offset = bs->thresholds_offset + sizeof(int) * (num_thresholds);
  counts = ar_binned_get_counts(bs);
  // zero out all the bin counts.
  memset(counts, 0, sizeof(uint64_t) * (num_thresholds + 1));

  bs->sums_offset = bs->counts_offset + sizeof(uint64_t) * (num_thresholds + 1);
  sums = ar_binned_get_sums(bs);
  memset(sums, 0, sizeof(uint64_t) * (num_thresholds + 1));
}
EXPORT_SYMBOL(ar_binned_sampler_init);

void ar_binned_sampler_add_sample(void* sampler, int sample) {
  uint64_t counter = 0;
  ar_binned_sampler_t* bs = (ar_binned_sampler_t*)sampler;
  int* thresholds;
  uint64_t* counts;
  uint64_t* sums;

  AR_ASSERT(bs != NULL);
  thresholds = ar_binned_get_thresholds(bs);
  counts = ar_binned_get_counts(bs);
  sums = ar_binned_get_sums(bs);

  if (bs->min == -1 || sample < bs->min) {
    bs->min = sample;
  }

  if (bs->max == -1 || sample > bs->max) {
    bs->max = sample;
  }

  for (int i = 0; i < bs->num_thresholds; ++i) {
    if (sample < thresholds[i]) {
      break;
    }

    counter++;
  }

  counts[counter]++;
  sums[counter] += sample;
  bs->num_samples++;
}
EXPORT_SYMBOL(ar_binned_sampler_add_sample);

int ar_binned_sampler_get_threshold(const void* sampler, int percentile) {
  uint64_t cum_count = 0, ord_rank = 0, bin_residual = 0;
  int ceil = 0, i = 0;
  const ar_binned_sampler_t* bs = (const ar_binned_sampler_t*)sampler;
  int* thresholds;
  uint64_t* counts;
  uint64_t* sums;

  AR_ASSERT(bs != NULL);

  if (percentile <= 0 || percentile > 100 || bs->num_samples == 0) {
    return -1;
  }

  thresholds = ar_binned_get_thresholds(bs);
  counts = ar_binned_get_counts(bs);
  sums = ar_binned_get_sums(bs);

  /**
   * ordinal rank is which sample (if we are ordering them from least to greatest)
   * that the nth percentile will fall at. If there are only 100 samples, this is
   * clearly n. We round up, as the definition of percentile rounds up the ordinal
   * rank as well.
   */
  ord_rank = (bs->num_samples * percentile + 99) / 100;

  /**
   * Since we don't keep track of the actual samples, we instead figure out which bucket the
   * ordinal falls in, and use the ceiling of that bucket as the threshold.
   */
  for (i = 0; i < bs->num_thresholds; ++i) {
    cum_count += counts[i];
    if (cum_count >= ord_rank) {
      bin_residual = cum_count - ord_rank;
      AR_ASSERT(bin_residual < counts[i]);
      if (bs->max != -1 && bs->max < thresholds[i]) {
        ceil = bs->max;
      } else {
        ceil = thresholds[i];
      }

      /**
       * we can get a better approximation of the value by using the stored
       * sum to calculate an average rather than rounding up to the threshold
       * value. However, for ord_rank values "high" in the bucket, we should
       * consider reporting a value closer to the next threshold / max value.
       * We correct for this by producing a weighted average of the bucket
       * average and the next "ceil" value. It's not perfect, but it should
       * generally provide an accurate over-estimation.
       *
       * T195735659: Sometimes our counter can give a value greater than ceil
       */
      if (AR_LIKELY((int)(sums[i] / counts[i]) <= ceil)) {
        return ((sums[i] * bin_residual / counts[i]) + ceil * (counts[i] - bin_residual)) /
            counts[i];
      } else {
        return ceil;
      }
    }
  }

  /**
   * We do the same weighting system for values beyond the max threshold, since
   * we know the average value of values beyond the max threshold.
   */
  bin_residual = ord_rank - cum_count;
  return ((sums[i] * (counts[i] - bin_residual) / counts[i]) + bs->max * bin_residual) / counts[i];
}
EXPORT_SYMBOL(ar_binned_sampler_get_threshold);

int ar_binned_sampler_get_min(const void* sampler) {
  const ar_binned_sampler_t* bs = (const ar_binned_sampler_t*)sampler;
  AR_ASSERT(bs != NULL);
  return bs->min;
}
EXPORT_SYMBOL(ar_binned_sampler_get_min);

int ar_binned_sampler_get_max(const void* sampler) {
  const ar_binned_sampler_t* bs = (const ar_binned_sampler_t*)sampler;
  AR_ASSERT(bs != NULL);
  return bs->max;
}
EXPORT_SYMBOL(ar_binned_sampler_get_max);

uint64_t ar_binned_sampler_get_num_samples(const void* sampler) {
  const ar_binned_sampler_t* bs = (const ar_binned_sampler_t*)sampler;
  AR_ASSERT(bs != NULL);
  return bs->num_samples;
}
EXPORT_SYMBOL(ar_binned_sampler_get_num_samples);
