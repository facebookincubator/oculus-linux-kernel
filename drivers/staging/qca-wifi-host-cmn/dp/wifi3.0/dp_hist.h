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

/**
 * DOC: dp_hist.h
 *      histogram header file
 */

#ifndef __DP_HIST_H_
#define __DP_HIST_H_

#define HIST_AVG_WEIGHT_DENOM 4

/**
 * dp_hist_update_stats() - Update histogram stats
 * @hist_stats: Delay histogram
 * @value: Delay value
 *
 * Return: void
 */
void dp_hist_update_stats(struct cdp_hist_stats *hist_stats, int value);

/**
 * dp_hist_init() - Initialize the histogram object
 * @hist_stats: Hist stats object
 * @hist_type: Histogram type
 */
void dp_hist_init(struct cdp_hist_stats *hist_stats,
		  enum cdp_hist_types hist_type);

/**
 * dp_accumulate_hist_stats() - Accumulate the hist src to dst
 * @src_hist_stats: Source histogram stats
 * @dst_hist_stats: Destination histogram stats
 *
 * Return: void
 */
void dp_accumulate_hist_stats(struct cdp_hist_stats *src_hist_stats,
			      struct cdp_hist_stats *dst_hist_stats);

/**
 * dp_copy_hist_stats() - Copy the histogram stats
 * @src_hist_stats: Source histogram stats
 * @dst_hist_stats: Destination histogram stats
 *
 * Return: void
 */
void dp_copy_hist_stats(struct cdp_hist_stats *src_hist_stats,
			struct cdp_hist_stats *dst_hist_stats);

const char *dp_hist_tx_hw_delay_str(uint8_t index);
const char *dp_hist_delay_percentile_str(uint8_t index);
#endif /* __DP_HIST_H_ */
