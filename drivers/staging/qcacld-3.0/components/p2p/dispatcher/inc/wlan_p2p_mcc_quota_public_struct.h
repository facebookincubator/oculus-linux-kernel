/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Contains mcc quota event public data structure definitions
 */

#ifndef _WLAN_P2P_MCC_QUOTA_PUBLIC_STRUCT_H_
#define _WLAN_P2P_MCC_QUOTA_PUBLIC_STRUCT_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;
struct wlan_objmgr_vdev;

/*
 * Max possible unique home channel numbers that host can receive to
 * struct mcc_quota_info from FW event. In real case, for two MACs DBS,
 * each MAC has two unique home channels, max home channel can't exceed 4.
 */
#define MAX_MCC_QUOTA_CH_NUM 4

/**
 * enum mcc_quota_type - mcc channel quota type
 * @QUOTA_TYPE_CLEAR: target exits MCC state and clear mcc quota information
 * @QUOTA_TYPE_FIXED: channel time quota is fixed and will not be changed
 * @QUOTA_TYPE_DYNAMIC: channel time quota is dynamic and targe may change
 *   the quota based on the data activity
 * @QUOTA_TYPE_UNKNOWN: unknown type
 */
enum mcc_quota_type {
	QUOTA_TYPE_CLEAR,
	QUOTA_TYPE_FIXED,
	QUOTA_TYPE_DYNAMIC,
	QUOTA_TYPE_UNKNOWN = 0xff
};

/**
 * struct channel_quota - mcc channel quota
 * @chan_mhz: frequency of the channel for which the quota is set
 * @channel_time_quota: channel time quota expressed as percentage
 */
struct channel_quota {
	uint32_t chan_mhz;
	uint32_t channel_time_quota;
};

/**
 * struct mcc_quota_info - mcc quota information
 * @type: mcc quota type
 * @num_chan_quota: number of channel quota in chan_quota
 * @chan_quota: channel quota array
 */
struct mcc_quota_info {
	enum mcc_quota_type type;
	uint32_t num_chan_quota;
	struct channel_quota chan_quota[MAX_MCC_QUOTA_CH_NUM];
};
#endif /* _WLAN_P2P_MCC_QUOTA_PUBLIC_STRUCT_H_ */
