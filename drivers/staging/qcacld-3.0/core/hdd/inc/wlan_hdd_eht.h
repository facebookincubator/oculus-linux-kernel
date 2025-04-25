/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_eht.h
 *
 * WLAN Host Device Driver file for 802.11be (Extremely High Throughput)
 * support.
 *
 */

#if !defined(WLAN_HDD_EHT_H)
#define WLAN_HDD_EHT_H
#include "wlan_osif_features.h"

struct hdd_context;
struct wma_tgt_cfg;
struct hdd_beacon_data;
struct sap_config;

#if defined(WLAN_FEATURE_11BE) && defined(CFG80211_11BE_BASIC)
/**
 * hdd_update_tgt_eht_cap() - Update EHT related capabilities
 * @hdd_ctx: HDD context
 * @cfg: Target capabilities
 *
 * This function updates WNI CFG with Target capabilities received as part of
 * Default values present in WNI CFG are the values supported by FW/HW.
 * INI should be introduced if user control is required to control the value.
 *
 * Return: None
 */
void hdd_update_tgt_eht_cap(struct hdd_context *hdd_ctx,
			    struct wma_tgt_cfg *cfg);

/**
 * wlan_hdd_check_11be_support() - check if beacon IE and update hw mode
 * @beacon: beacon IE buffer
 * @config: pointer to sap config
 *
 * Check if EHT cap IE is present in beacon IE, if present update hw mode
 * to 11be.
 *
 * Return: None
 */
void wlan_hdd_check_11be_support(struct hdd_beacon_data *beacon,
				 struct sap_config *config);

/**
 * hdd_update_wiphy_eht_cap() - update the wiphy with eht capabilities
 * @hdd_ctx: HDD context
 *
 * update wiphy with the eht capabilities.
 *
 * Return: None
 */
void hdd_update_wiphy_eht_cap(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_get_mlo_link_id() - get link id and number of links
 * @beacon: beacon IE buffer
 * @link_id: link id to return
 * @num_link: total links
 *
 * Return: None
 */
void wlan_hdd_get_mlo_link_id(struct hdd_beacon_data *beacon,
			      uint8_t *link_id, uint8_t *num_link);

/**
 * hdd_set_11be_rate_code() - set 11be rate code
 * @adapter: net device adapter
 * @rate_code: new 11be rate code
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_11be_rate_code(struct hdd_adapter *adapter, uint16_t rate_code);

/**
 * wlan_hdd_fill_os_eht_rateflags() - Fill EHT related rate_info
 * @os_rate: rate info for os
 * @rate_flags: rate flags
 * @dcm: dcm from rate
 * @guard_interval: guard interval from rate
 *
 * Return: none
 */
void wlan_hdd_fill_os_eht_rateflags(struct rate_info *os_rate,
				    enum tx_rate_info rate_flags,
				    uint8_t dcm,
				    enum txrate_gi guard_interval);
#else
static inline
void hdd_update_tgt_eht_cap(struct hdd_context *hdd_ctx,
			    struct wma_tgt_cfg *cfg)
{
}

static inline void wlan_hdd_check_11be_support(struct hdd_beacon_data *beacon,
					       struct sap_config *config)
{
}

static inline
void hdd_update_wiphy_eht_cap(struct hdd_context *hdd_ctx)
{
}

static inline int
hdd_set_11be_rate_code(struct hdd_adapter *adapter, uint16_t rate_code)
{
	return 0;
}

static inline void wlan_hdd_get_mlo_link_id(struct hdd_beacon_data *beacon,
					    uint8_t *link_id, uint8_t *num_link)
{
}

static inline
void wlan_hdd_fill_os_eht_rateflags(struct rate_info *os_rate,
				    enum tx_rate_info rate_flags,
				    uint8_t dcm,
				    enum txrate_gi guard_interval)
{
}
#endif

#if defined(WLAN_FEATURE_11BE) && defined(CFG80211_11BE_BASIC) && \
	defined(FEATURE_RX_LINKSPEED_ROAM_TRIGGER)
/**
 * wlan_hdd_refill_os_eht_rateflags() - Refill EHT rate flag
 * @os_rate: rate info for os
 * @preamble: Use to acquire wlan mode, whether in EHT mode
 *
 * Fill out os ETH MCS rate flag according to preamble.
 *
 * Return: none
 */
void
wlan_hdd_refill_os_eht_rateflags(struct rate_info *os_rate, uint8_t preamble);

/**
 * wlan_hdd_refill_os_eht_bw() - Refill EHT bandwidth
 * @os_rate: rate info for os
 * @bw: Bandwidth of the frame
 *
 * Fill out os ETH BW flag according to CMN BW from driver.
 *
 * Return: none
 */
void
wlan_hdd_refill_os_eht_bw(struct rate_info *os_rate, enum rx_tlv_bw bw);
#else
static inline void
wlan_hdd_refill_os_eht_rateflags(struct rate_info *os_rate, uint8_t preamble)
{
}

static inline void
wlan_hdd_refill_os_eht_bw(struct rate_info *os_rate, enum rx_tlv_bw bw)
{
}
#endif
#endif /* if !defined(WLAN_HDD_EHT_H)*/
