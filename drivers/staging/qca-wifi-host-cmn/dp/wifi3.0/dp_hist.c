/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <qdf_util.h>
#include <qdf_mem.h>
#include <cdp_txrx_hist_struct.h>
#include "dp_hist.h"

#ifndef WLAN_CONFIG_TX_DELAY
/*
 * dp_hist_sw_enq_dbucket: Software enqueue delay bucket in ms
 * @index_0 = 0_1 ms
 * @index_1 = 1_2 ms
 * @index_2 = 2_3 ms
 * @index_3 = 3_4 ms
 * @index_4 = 4_5 ms
 * @index_5 = 5_6 ms
 * @index_6 = 6_7 ms
 * @index_7 = 7_8 ms
 * @index_8 = 8_9 ms
 * @index_9 = 9_10 ms
 * @index_10 = 10_11 ms
 * @index_11 = 11_12 ms
 * @index_12 = 12+ ms
 */
static uint16_t dp_hist_sw_enq_dbucket[CDP_HIST_BUCKET_MAX] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

/*
 * cdp_hist_fw2hw_dbucket: HW enqueue to Completion Delay
 * @index_0 = 0_10 ms
 * @index_1 = 10_20 ms
 * @index_2 = 20_30ms
 * @index_3 = 30_40 ms
 * @index_4 = 40_50 ms
 * @index_5 = 50_60 ms
 * @index_6 = 60_70 ms
 * @index_7 = 70_80 ms
 * @index_8 = 80_90 ms
 * @index_9 = 90_100 ms
 * @index_10 = 100_250 ms
 * @index_11 = 250_500 ms
 * @index_12 = 500+ ms
 */
static uint16_t dp_hist_fw2hw_dbucket[CDP_HIST_BUCKET_MAX] = {
	0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 250, 500};
#else
/*
 * dp_hist_sw_enq_dbucket: Software enqueue delay bucket in us
 * @index_0 = 0_250 us
 * @index_1 = 250_500 us
 * @index_2 = 500_750 us
 * @index_3 = 750_1000 us
 * @index_4 = 1000_1500 us
 * @index_5 = 1500_2000 us
 * @index_6 = 2000_2500 us
 * @index_7 = 2500_5000 us
 * @index_8 = 5000_6000 us
 * @index_9 = 6000_7000 us
 * @index_10 = 7000_8000 us
 * @index_11 = 8000_9000 us
 * @index_12 = 9000+ us
 */
static uint16_t dp_hist_sw_enq_dbucket[CDP_HIST_BUCKET_MAX] = {
	0, 250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000};

/*
 * cdp_hist_fw2hw_dbucket: HW enqueue to Completion Delay in us
 * @index_0 = 0_250 us
 * @index_1 = 250_500 us
 * @index_2 = 500_750 us
 * @index_3 = 750_1000 us
 * @index_4 = 1000_1500 us
 * @index_5 = 1500_2000 us
 * @index_6 = 2000_2500 us
 * @index_7 = 2500_5000 us
 * @index_8 = 5000_6000 us
 * @index_9 = 6000_7000 us
 * @index_10 = 7000_8000 us
 * @index_11 = 8000_9000 us
 * @index_12 = 9000+ us
 */

static uint16_t dp_hist_fw2hw_dbucket[CDP_HIST_BUCKET_MAX] = {
	0, 250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000};
#endif

/*
 * dp_hist_reap2stack_bucket: Reap to stack bucket
 * @index_0 = 0_5 ms
 * @index_1 = 5_10 ms
 * @index_2 = 10_15 ms
 * @index_3 = 15_20 ms
 * @index_4 = 20_25 ms
 * @index_5 = 25_30 ms
 * @index_6 = 30_35 ms
 * @index_7 = 35_40 ms
 * @index_8 = 40_45 ms
 * @index_9 = 46_50 ms
 * @index_10 = 51_55 ms
 * @index_11 = 56_60 ms
 * @index_12 = 60+ ms
 */
static uint16_t dp_hist_reap2stack_bucket[CDP_HIST_BUCKET_MAX] = {
	0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60};

/*
 * dp_hist_hw_tx_comp_dbucket: tx hw completion delay bucket in us
 * @index_0 = 0_250 us
 * @index_1 = 250_500 us
 * @index_2 = 500_750 us
 * @index_3 = 750_1000 us
 * @index_4 = 1000_1500 us
 * @index_5 = 1500_2000 us
 * @index_6 = 2000_2500 us
 * @index_7 = 2500_5000 us
 * @index_8 = 5000_6000 us
 * @index_9 = 6000_7000 us
 * @index_10 = 7000_8000 us
 * @index_11 = 8000_9000 us
 * @index_12 = 9000+ us
 */
static uint16_t dp_hist_hw_tx_comp_dbucket[CDP_HIST_BUCKET_MAX] = {
	0, 250, 500, 750, 1000, 1500, 2000, 2500, 5000, 6000, 7000, 8000, 9000};

static const char *dp_hist_hw_tx_comp_dbucket_str[CDP_HIST_BUCKET_MAX + 1] = {
	"0 to 250 us", "250 to 500 us",
	"500 to 750 us", "750 to 1000 us",
	"1000 to 1500 us", "1500 to 2000 us",
	"2000 to 2500 us", "2500 to 5000 us",
	"5000 to 6000 us", "6000 to 7000 ms",
	"7000 to 8000 us", "8000 to 9000 us", "9000+ us"
};

const char *dp_hist_tx_hw_delay_str(uint8_t index)
{
	if (index > CDP_HIST_BUCKET_MAX)
		return "Invalid index";
	return dp_hist_hw_tx_comp_dbucket_str[index];
}

/*
 * dp_hist_delay_percentile_dbucket: tx hw completion delay bucket in delay
 * bound percentile
 * @index_0 = 0_10
 * @index_1 = 10_20
 * @index_2 = 20_30
 * @index_3 = 30_40
 * @index_4 = 40_50
 * @index_5 = 50_60
 * @index_6 = 60_70
 * @index_7 = 70_80
 * @index_8 = 80_100
 * @index_9 = 90_100
 * @index_10 = 100_150
 * @index_11 = 150_200
 * @index_12 = 200+
 */
static uint16_t dp_hist_delay_percentile_dbucket[CDP_HIST_BUCKET_MAX] = {
	0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 150, 200};

static
const char *dp_hist_delay_percentile_dbucket_str[CDP_HIST_BUCKET_MAX + 1] = {
	"0 to 10%", "10 to 20%",
	"20 to  30%", "30 to 40%",
	"40 to 50%", "50 to 60%",
	"60 to 70%", "70 to 80%",
	"80 to 90% ", "90 to 100%",
	"100 to 150% ", "150 to 200%", "200+%"
};

const char *dp_hist_delay_percentile_str(uint8_t index)
{
	if (index > CDP_HIST_BUCKET_MAX)
		return "Invalid index";
	return dp_hist_delay_percentile_dbucket_str[index];
}

/**
 * dp_hist_find_bucket_idx() - Find the bucket index
 * @bucket_array: Bucket array
 * @value: Frequency value
 *
 * Return: The bucket index
 */
static int dp_hist_find_bucket_idx(int16_t *bucket_array, int value)
{
	uint8_t idx = CDP_HIST_BUCKET_0;

	for (; idx < (CDP_HIST_BUCKET_MAX - 1); idx++) {
		if (value < bucket_array[idx + 1])
			break;
	}

	return idx;
}

/**
 * dp_hist_fill_buckets() - Fill the histogram frequency buckets
 * @hist_bucket: Histogram bukcets
 * @value: Frequency value
 *
 * Return: void
 */
static void dp_hist_fill_buckets(struct cdp_hist_bucket *hist_bucket, int value)
{
	enum cdp_hist_types hist_type;
	int idx = CDP_HIST_BUCKET_MAX;

	if (qdf_unlikely(!hist_bucket))
		return;

	hist_type = hist_bucket->hist_type;

	/* Identify the bucket the bucket and update. */
	switch (hist_type) {
	case CDP_HIST_TYPE_SW_ENQEUE_DELAY:
		idx =  dp_hist_find_bucket_idx(&dp_hist_sw_enq_dbucket[0],
					       value);
		break;
	case CDP_HIST_TYPE_HW_COMP_DELAY:
		idx =  dp_hist_find_bucket_idx(&dp_hist_fw2hw_dbucket[0],
					       value);
		break;
	case CDP_HIST_TYPE_REAP_STACK:
		idx =  dp_hist_find_bucket_idx(
				&dp_hist_reap2stack_bucket[0], value);
		break;
	case CDP_HIST_TYPE_HW_TX_COMP_DELAY:
		idx =  dp_hist_find_bucket_idx(
				&dp_hist_hw_tx_comp_dbucket[0], value);
		break;
	case CDP_HIST_TYPE_DELAY_PERCENTILE:
		idx =  dp_hist_find_bucket_idx(
				&dp_hist_delay_percentile_dbucket[0], value);
		break;
	default:
		break;
	}

	if (idx == CDP_HIST_BUCKET_MAX)
		return;

	hist_bucket->freq[idx]++;
}

void dp_hist_update_stats(struct cdp_hist_stats *hist_stats, int value)
{
	if (qdf_unlikely(!hist_stats))
		return;

	/*
	 * Fill the histogram buckets according to the delay
	 */
	dp_hist_fill_buckets(&hist_stats->hist, value);

	/*
	 * Compute the min, max and average. Average computed is weighted
	 * average
	 */
	if (value < hist_stats->min)
		hist_stats->min = value;

	if (value > hist_stats->max)
		hist_stats->max = value;

	if (qdf_unlikely(!hist_stats->avg))
		hist_stats->avg = value;
	else
		hist_stats->avg = (hist_stats->avg + value) / 2;
}

void dp_copy_hist_stats(struct cdp_hist_stats *src_hist_stats,
			struct cdp_hist_stats *dst_hist_stats)
{
	uint8_t index;

	for (index = 0; index < CDP_HIST_BUCKET_MAX; index++)
		dst_hist_stats->hist.freq[index] =
			src_hist_stats->hist.freq[index];
	dst_hist_stats->min = src_hist_stats->min;
	dst_hist_stats->max = src_hist_stats->max;
	dst_hist_stats->avg = src_hist_stats->avg;
}

void dp_accumulate_hist_stats(struct cdp_hist_stats *src_hist_stats,
			      struct cdp_hist_stats *dst_hist_stats)
{
	uint8_t index, hist_stats_valid = 0;

	for (index = 0; index < CDP_HIST_BUCKET_MAX; index++) {
		dst_hist_stats->hist.freq[index] +=
			src_hist_stats->hist.freq[index];
		if (src_hist_stats->hist.freq[index])
			hist_stats_valid = 1;
	}
	/*
	 * If at least one hist-bucket has non-zero count,
	 * proceed with the detailed calculation.
	 */
	if (hist_stats_valid) {
		dst_hist_stats->min = QDF_MIN(src_hist_stats->min,
					      dst_hist_stats->min);
		dst_hist_stats->max = QDF_MAX(src_hist_stats->max,
					      dst_hist_stats->max);
		dst_hist_stats->avg = (src_hist_stats->avg +
				       dst_hist_stats->avg) >> 1;
	}
}

void dp_hist_init(struct cdp_hist_stats *hist_stats,
		  enum cdp_hist_types hist_type)
{
	qdf_mem_zero(hist_stats, sizeof(*hist_stats));
	hist_stats->min =  INT_MAX;
	hist_stats->hist.hist_type = hist_type;
}
