/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_reg_services_api.h
 * This file provides prototypes of the routines needed for the
 * external components to utilize the services provided by the
 * regulatory component.
 */

#ifndef __WLAN_REG_SERVICES_API_H
#define __WLAN_REG_SERVICES_API_H

#include <reg_services_public_struct.h>

/**
 * wlan_reg_max_5ghz_ch_num() - Get maximum 5GHz channel number
 *
 * Return: Maximum 5GHz channel number
 */
#define WLAN_REG_MAX_5GHZ_CH_NUM wlan_reg_max_5ghz_ch_num()
uint8_t wlan_reg_max_5ghz_ch_num(void);

#ifdef CONFIG_CHAN_FREQ_API
/**
 * wlan_reg_min_24ghz_chan_freq() - Get minimum 2.4GHz channel frequency
 *
 * Return: Minimum 2.4GHz channel frequency
 */
#define WLAN_REG_MIN_24GHZ_CHAN_FREQ wlan_reg_min_24ghz_chan_freq()
qdf_freq_t wlan_reg_min_24ghz_chan_freq(void);

/**
 * wlan_reg_max_24ghz_chan_freq() - Get maximum 2.4GHz channel frequency
 *
 * Return: Maximum 2.4GHz channel frequency
 */
#define WLAN_REG_MAX_24GHZ_CHAN_FREQ wlan_reg_max_24ghz_chan_freq()
qdf_freq_t wlan_reg_max_24ghz_chan_freq(void);

/**
 * wlan_reg_min_5ghz_chan_freq() - Get minimum 5GHz channel frequency
 *
 * Return: Minimum 5GHz channel frequency
 */
#define WLAN_REG_MIN_5GHZ_CHAN_FREQ wlan_reg_min_5ghz_chan_freq()
qdf_freq_t wlan_reg_min_5ghz_chan_freq(void);

/**
 * wlan_reg_max_5ghz_chan_freq() - Get maximum 5GHz channel frequency
 *
 * Return: Maximum 5GHz channel frequency
 */
#define WLAN_REG_MAX_5GHZ_CHAN_FREQ wlan_reg_max_5ghz_chan_freq()
qdf_freq_t wlan_reg_max_5ghz_chan_freq(void);
#endif /* CONFIG_CHAN_FREQ_API */

/**
 * wlan_reg_is_24ghz_ch_freq() - Check if the given channel frequency is 2.4GHz
 * @freq: Channel frequency
 *
 * Return: true if channel frequency is 2.4GHz, else false
 */
#define WLAN_REG_IS_24GHZ_CH_FREQ(freq) wlan_reg_is_24ghz_ch_freq(freq)
bool wlan_reg_is_24ghz_ch_freq(qdf_freq_t freq);

/**
 * wlan_reg_is_5ghz_ch_freq() - Check if the given channel frequency is 5GHz
 * @freq: Channel frequency
 *
 * Return: true if channel frequency is 5GHz, else false
 */
#define WLAN_REG_IS_5GHZ_CH_FREQ(freq) wlan_reg_is_5ghz_ch_freq(freq)
bool wlan_reg_is_5ghz_ch_freq(qdf_freq_t freq);

/**
 * wlan_reg_is_range_overlap_2g() - Check if the given low_freq and high_freq
 * is in the 2G range.
 *
 * @low_freq - Low frequency.
 * @high_freq - High frequency.
 *
 * Return: Return true if given low_freq and high_freq overlaps 2G range,
 * else false.
 */
bool wlan_reg_is_range_overlap_2g(qdf_freq_t low_freq, qdf_freq_t high_freq);

/**
 * wlan_reg_is_range_overlap_5g() - Check if the given low_freq and high_freq
 * is in the 5G range.
 *
 * @low_freq - Low frequency.
 * @high_freq - High frequency.
 *
 * Return: Return true if given low_freq and high_freq overlaps 5G range,
 * else false.
 */
bool wlan_reg_is_range_overlap_5g(qdf_freq_t low_freq, qdf_freq_t high_freq);

/**
 * wlan_reg_is_freq_indoor() - Check if a frequency is indoor.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency.
 *
 * Return: Return true if a frequency is indoor, else false.
 */
bool wlan_reg_is_freq_indoor(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_get_min_chwidth() - Return min chanwidth supported by freq.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency.
 *
 * Return: Min chwidth supported by freq as per regulatory DB.
 */
uint16_t wlan_reg_get_min_chwidth(struct wlan_objmgr_pdev *pdev,
				  qdf_freq_t freq);

/**
 * wlan_reg_get_max_chwidth() - Return max chanwidth supported by freq.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency.
 *
 * Return: Max chwidth supported by freq as per regulatory DB.
 */
uint16_t wlan_reg_get_max_chwidth(struct wlan_objmgr_pdev *pdev,
				  qdf_freq_t freq);

/**
 * wlan_reg_get_next_lower_bandwidth() - Get next lower bandwdith
 * @ch_width: channel bandwdith
 *
 * Return: Return next lower bandwidth of input channel bandwidth
 */
enum phy_ch_width
wlan_reg_get_next_lower_bandwidth(enum phy_ch_width ch_width);

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_is_freq_indoor_in_secondary_list() - Check if the input frequency is
 * an indoor frequency in the secondary list
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency.
 *
 * Return: Return true if a frequency is indoor, else false.
 */
bool wlan_reg_is_freq_indoor_in_secondary_list(struct wlan_objmgr_pdev *pdev,
					       qdf_freq_t freq);
#endif

#ifdef CONFIG_BAND_6GHZ
/**
 * wlan_reg_is_6ghz_chan_freq() - Check if the given channel frequency is 6GHz
 * @freq: Channel frequency
 *
 * Return: true if channel frequency is 6GHz, else false
 */
#define WLAN_REG_IS_6GHZ_CHAN_FREQ(freq) wlan_reg_is_6ghz_chan_freq(freq)
bool wlan_reg_is_6ghz_chan_freq(uint16_t freq);

#ifdef CONFIG_6G_FREQ_OVERLAP
/**
 * wlan_reg_is_range_only6g() - Check if the given low_freq and high_freq
 * is in the 6G range.
 * @low_freq - Low frequency.
 * @high_freq - High frequency.
 *
 * Return: Return true if given low_freq and high_freq overlaps 6G range,
 * else false.
 */
bool wlan_reg_is_range_only6g(qdf_freq_t low_freq, qdf_freq_t high_freq);

/**
 * wlan_reg_is_range_overlap_6g() - Check if the given low_freq and high_freq
 * is in the 6G range.
 *
 * @low_freq - Low frequency.
 * @high_freq - High frequency.
 *
 * Return: Return true if given low_freq and high_freq overlaps 6G range,
 * else false.
 */
bool wlan_reg_is_range_overlap_6g(qdf_freq_t low_freq, qdf_freq_t high_freq);
#else
static inline bool wlan_reg_is_range_only6g(qdf_freq_t low_freq,
					    qdf_freq_t high_freq)
{
	return false;
}

static inline bool wlan_reg_is_range_overlap_6g(qdf_freq_t low_freq,
						qdf_freq_t high_freq)
{
	return false;
}
#endif

/**
 * wlan_reg_get_6g_ap_master_chan_list() - provide  the appropriate ap master
 * channel list
 * @pdev: pdev pointer
 * @ap_pwr_type: The ap power type (LPI/VLP/SP)
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_get_6g_ap_master_chan_list(
					struct wlan_objmgr_pdev *pdev,
					enum reg_6g_ap_type ap_pwr_type,
					struct regulatory_channel *chan_list);

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_get_power_string () - wlan reg get power type string
 * @power_type: power type enum
 *
 * Return: power type string
 */
const char *wlan_reg_get_power_string(enum reg_6g_ap_type power_type);
#endif

/**
 * wlan_reg_is_6ghz_psc_chan_freq() - Check if the given 6GHz channel frequency
 * is preferred scanning channel frequency.
 * @freq: Channel frequency
 *
 * Return: true if given 6GHz channel frequency is preferred scanning channel
 * frequency, else false
 */
#define WLAN_REG_IS_6GHZ_PSC_CHAN_FREQ(freq) \
	wlan_reg_is_6ghz_psc_chan_freq(freq)
bool wlan_reg_is_6ghz_psc_chan_freq(uint16_t freq);

/**
 * wlan_reg_min_6ghz_chan_freq() - Get minimum 6GHz channel center frequency
 *
 * Return: Minimum 6GHz channel center frequency
 */
#define WLAN_REG_MIN_6GHZ_CHAN_FREQ wlan_reg_min_6ghz_chan_freq()
uint16_t wlan_reg_min_6ghz_chan_freq(void);

/**
 * wlan_reg_max_6ghz_chan_freq() - Get maximum 6GHz channel center frequency
 *
 * Return: Maximum 6GHz channel center frequency
 */
#define WLAN_REG_MAX_6GHZ_CHAN_FREQ wlan_reg_max_6ghz_chan_freq()
uint16_t wlan_reg_max_6ghz_chan_freq(void);

/**
 * wlan_reg_is_6g_freq_indoor() - Check if a 6GHz frequency is indoor.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency.
 *
 * Return: Return true if a 6GHz frequency is indoor, else false.
 */
#define WLAN_REG_IS_6GHZ_FREQ_INDOOR(pdev, freq) \
					wlan_reg_is_6g_freq_indoor(pdev, freq)
bool wlan_reg_is_6g_freq_indoor(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_get_max_txpower_for_6g_tpe() - Get max txpower for 6G TPE IE.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency.
 * @bw: Channel bandwidth.
 * @reg_ap: Regulatory 6G AP type.
 * @reg_client: Regulatory client type.
 * @is_psd: True if txpower is needed in PSD format, and false if needed in EIRP
 * format.
 * @tx_power: Pointer to tx-power.
 *
 * Return: Return QDF_STATUS_SUCCESS, if tx_power is filled for 6G TPE IE
 * else return QDF_STATUS_E_FAILURE.
 */
QDF_STATUS
wlan_reg_get_max_txpower_for_6g_tpe(struct wlan_objmgr_pdev *pdev,
				    qdf_freq_t freq, uint8_t bw,
				    enum reg_6g_ap_type reg_ap,
				    enum reg_6g_client_type reg_client,
				    bool is_psd,
				    uint8_t *tx_power);

/**
 * wlan_reg_get_superchan_entry() - Get the address of the super channel list
 * entry for a given input channel index.
 *
 * @pdev: pdev ptr
 * @chan_enum: Channel enum
 * @p_sup_chan_entry: Pointer to address of *p_sup_chan_entry
 *
 * Return: QDF_STATUS_SUCCESS if super channel entry is available for the input
 * chan_enum else QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_reg_get_superchan_entry(
		struct wlan_objmgr_pdev *pdev,
		enum channel_enum chan_enum,
		const struct super_chan_info **p_sup_chan_entry);
#else

#define WLAN_REG_IS_6GHZ_CHAN_FREQ(freq) (false)
static inline bool wlan_reg_is_6ghz_chan_freq(uint16_t freq)
{
	return false;
}

static inline bool wlan_reg_is_range_only6g(qdf_freq_t low_freq,
					    qdf_freq_t high_freq)
{
	return false;
}

#define WLAN_REG_IS_6GHZ_PSC_CHAN_FREQ(freq) (false)
static inline bool wlan_reg_is_6ghz_psc_chan_freq(uint16_t freq)
{
	return false;
}

#define WLAN_REG_MIN_6GHZ_CHAN_FREQ (false)
static inline uint16_t wlan_reg_min_6ghz_chan_freq(void)
{
	return 0;
}

#define WLAN_REG_MAX_6GHZ_CHAN_FREQ (false)
static inline uint16_t wlan_reg_max_6ghz_chan_freq(void)
{
	return 0;
}

#define WLAN_REG_IS_6GHZ_FREQ_INDOOR(pdev, freq) (false)
static inline bool
wlan_reg_is_6g_freq_indoor(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	return false;
}

static inline bool wlan_reg_is_range_overlap_6g(qdf_freq_t low_freq,
						qdf_freq_t high_freq)
{
	return false;
}

static inline QDF_STATUS
wlan_reg_get_max_txpower_for_6g_tpe(struct wlan_objmgr_pdev *pdev,
				    qdf_freq_t freq, uint8_t bw,
				    enum reg_6g_ap_type reg_ap,
				    enum reg_6g_client_type reg_client,
				    bool is_psd,
				    uint8_t *tx_power)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
wlan_reg_get_6g_ap_master_chan_list(struct wlan_objmgr_pdev *pdev,
				    enum reg_6g_ap_type ap_pwr_type,
				    struct regulatory_channel *chan_list)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
QDF_STATUS wlan_reg_get_superchan_entry(
		struct wlan_objmgr_pdev *pdev,
		enum channel_enum chan_enum,
		const struct super_chan_info **p_sup_chan_entry)
{
	*p_sup_chan_entry = NULL;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
const char *wlan_reg_get_power_string(enum reg_6g_ap_type power_type)
{
	return "INVALID";
}
#endif /* CONFIG_BAND_6GHZ */

/**
 * wlan_reg_get_band_channel_list() - Get channel list based on the band_mask
 * @pdev: pdev ptr
 * @band_mask: Input bitmap with band set
 * @channel_list: Pointer to Channel List
 *
 * Get the given channel list and number of channels from the current channel
 * list based on input band bitmap.
 *
 * Return: Number of channels, else 0 to indicate error
 */
uint16_t
wlan_reg_get_band_channel_list(struct wlan_objmgr_pdev *pdev,
			       uint8_t band_mask,
			       struct regulatory_channel *channel_list);

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_get_band_channel_list_for_pwrmode() - Get channel list based on the
 * band_mask and input 6G power mode.
 * @pdev: pdev ptr
 * @band_mask: Input bitmap with band set
 * @channel_list: Pointer to Channel List
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 *
 * Get the given channel list and number of channels from the current channel
 * list based on input band bitmap.
 *
 * Return: Number of channels, else 0 to indicate error
 */
uint16_t
wlan_reg_get_band_channel_list_for_pwrmode(struct wlan_objmgr_pdev *pdev,
					   uint8_t band_mask,
					   struct regulatory_channel
					   *channel_list,
					   enum supported_6g_pwr_types
					   in_6g_pwr_type);
#endif

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_get_secondary_band_channel_list() - Get secondary channel list for
 * SAP based on the band_mask
 * @pdev: pdev ptr
 * @band_mask: Input bitmap with band set
 * @channel_list: Pointer to Channel List
 *
 * Get the given channel list and number of channels from the secondary current
 * channel list based on input band bitmap.
 *
 * Return: Number of channels, else 0 to indicate error
 */
uint16_t
wlan_reg_get_secondary_band_channel_list(struct wlan_objmgr_pdev *pdev,
					 uint8_t band_mask,
					 struct regulatory_channel
					 *channel_list);
#endif

/**
 * wlan_reg_chan_band_to_freq - Return channel frequency based on the channel
 * number and band.
 * @pdev: pdev ptr
 * @chan: Channel Number
 * @band_mask: Bitmap for bands
 *
 * Return: Return channel frequency or return 0, if the channel is disabled or
 * if the input channel number or band_mask is invalid. Composite bands are
 * supported only for 2.4Ghz and 5Ghz bands. For other bands the following
 * priority is given: 1) 6Ghz 2) 5Ghz 3) 2.4Ghz.
 */
qdf_freq_t wlan_reg_chan_band_to_freq(struct wlan_objmgr_pdev *pdev,
				      uint8_t chan,
				      uint8_t band_mask);

#ifdef CONFIG_49GHZ_CHAN
/**
 * wlan_reg_is_49ghz_freq() - Check if the given channel frequency is 4.9GHz
 * @freq: Channel frequency
 *
 * Return: true if channel frequency is 4.9GHz, else false
 */
#define WLAN_REG_IS_49GHZ_FREQ(freq) wlan_reg_is_49ghz_freq(freq)
bool wlan_reg_is_49ghz_freq(qdf_freq_t freq);

#else

#define WLAN_REG_IS_49GHZ_FREQ(freq) (false)
static inline bool wlan_reg_is_49ghz_freq(qdf_freq_t freq)
{
	return false;
}
#endif /* CONFIG_49GHZ_CHAN */

/**
 * wlan_reg_ch_num() - Get channel number from channel enum
 * @ch_enum: Channel enum
 *
 * Return: channel number
 */
#define WLAN_REG_CH_NUM(ch_enum) wlan_reg_ch_num(ch_enum)
uint8_t wlan_reg_ch_num(uint32_t ch_enum);

/**
 * wlan_reg_ch_to_freq() - Get channel frequency from channel enum
 * @ch_enum: Channel enum
 *
 * Return: channel frequency
 */
#define WLAN_REG_CH_TO_FREQ(ch_enum) wlan_reg_ch_to_freq(ch_enum)
qdf_freq_t wlan_reg_ch_to_freq(uint32_t ch_enum);

/**
 * wlan_reg_read_default_country() - Read the default country for the regdomain
 * @country: pointer to the country code.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_read_default_country(struct wlan_objmgr_psoc *psoc,
				   uint8_t *country);

/**
 * wlan_reg_get_ctry_idx_max_bw_from_country_code() - Get the max 5G
 * bandwidth from country code
 * @pdev: pdev pointer
 * @cc: Country Code
 * @max_bw_5g: Max 5G bandwidth supported by the country
 *
 * Return: QDF_STATUS
 */

QDF_STATUS wlan_reg_get_max_5g_bw_from_country_code(
					struct wlan_objmgr_pdev *pdev,
					uint16_t cc,
					uint16_t *max_bw_5g);

/**
 * wlan_reg_get_max_5g_bw_from_regdomain() - Get the max 5G bandwidth
 * supported by the regdomain
 * @pdev: pdev pointer
 * @orig_regdmn: Regdomain Pair value
 * @max_bw_5g: Max 5G bandwidth supported by the country
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_get_max_5g_bw_from_regdomain(
					struct wlan_objmgr_pdev *pdev,
					uint16_t regdmn,
					uint16_t *max_bw_5g);

/**
 * wlan_reg_get_max_bw_5G_for_fo() - get max_5g_bw for FullOffload
 * @pdev: PDEV object
 *
 * API to get max_bw_5g from pdev object
 *
 * Return: @max_bw_5g
 */
QDF_STATUS wlan_reg_get_max_bw_5G_for_fo(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_is_regdb_offloaded() - get offload_enabled
 * @psoc: Psoc object
 *
 * API to get offload_enabled from psoc.
 *
 * Return: true if offload enabled
 */

bool wlan_reg_is_regdb_offloaded(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_reg_get_fcc_constraint() - Check FCC constraint on given frequency
 * @pdev: physical dev to get
 * @freq: frequency to be checked
 *
 * Return: If FCC constraint is on applied given frequency return true
 *	   else return false.
 */
bool wlan_reg_get_fcc_constraint(struct wlan_objmgr_pdev *pdev, uint32_t freq);

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_read_current_country() - Read the current country for the regdomain
 * @country: pointer to the country code.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_read_current_country(struct wlan_objmgr_psoc *psoc,
				   uint8_t *country);

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_get_6g_power_type_for_ctry() - Return power type for 6G based
 * on country IE
 * @psoc: pointer to psoc
 * @pdev: pointer to pdev
 * @ap_ctry: pointer to country string in country IE
 * @sta_ctry: pointer to sta programmed country
 * @pwr_type_6g: pointer to 6G power type
 * @ctry_code_match: Check for country IE and sta country code match
 * @ap_pwr_type: AP's power type for 6G as advertised in HE ops IE
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_6g_power_type_for_ctry(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_pdev *pdev,
				    uint8_t *ap_ctry, uint8_t *sta_ctry,
				    enum reg_6g_ap_type *pwr_type_6g,
				    bool *ctry_code_match,
				    enum reg_6g_ap_type ap_pwr_type);
#endif

#ifdef CONFIG_CHAN_FREQ_API
/**
 * wlan_reg_is_etsi13_srd_chan_for_freq () - Checks if the ch is ETSI13 srd ch
 * or not
 * @pdev: pdev ptr
 * @freq: channel center frequency
 *
 * Return: true or false
 */
bool wlan_reg_is_etsi13_srd_chan_for_freq(struct wlan_objmgr_pdev *pdev,
					  qdf_freq_t freq);
#endif /*CONFIG_CHAN_FREQ_API*/

/**
 * wlan_reg_is_etsi13_regdmn() - Checks if current reg domain is ETSI13 or not
 * @pdev: pdev ptr
 *
 * Return: true or false
 */
bool wlan_reg_is_etsi13_regdmn(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_is_etsi13_srd_chan_allowed_master_mode() - Checks if regdmn is
 * ETSI13 and SRD channels are allowed in master mode or not.
 *
 * @pdev: pdev ptr
 *
 * Return: true or false
 */
bool wlan_reg_is_etsi13_srd_chan_allowed_master_mode(struct wlan_objmgr_pdev
						     *pdev);
#endif

/**
 * wlan_reg_is_world() - reg is world mode
 * @country: The country information
 *
 * Return: true or false
 */
bool wlan_reg_is_world(uint8_t *country);

/**
 * wlan_reg_get_dfs_region () - Get the current dfs region
 * @dfs_reg: pointer to dfs region
 *
 * Return: Status
 */
QDF_STATUS wlan_reg_get_dfs_region(struct wlan_objmgr_pdev *pdev,
			     enum dfs_reg *dfs_reg);

/**
 * wlan_reg_is_chan_disabled_and_not_nol() - In the regulatory channel list, a
 * channel may be disabled by the regulatory/device or by radar. Radar is
 * temporary and a radar disabled channel does not mean that the channel is
 * permanently disabled. The API checks if the channel is disabled, but not due
 * to radar.
 * @chan - Regulatory channel object
 *
 * Return - True,  the channel is disabled, but not due to radar, else false.
 */
bool wlan_reg_is_chan_disabled_and_not_nol(struct regulatory_channel *chan);

/**
 * wlan_reg_get_current_chan_list() - provide the pdev current channel list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
		struct regulatory_channel *chan_list);

/**
 * wlan_reg_is_freq_enabled() - Checks if the given frequency is enabled on the
 * given power mode or not. If the frequency is not a 6G frequency then the
 * input power mode is ignored and only current channel list is searched.
 *
 * @pdev: pdev pointer.
 * @freq: input frequency.
 * @in_6g_pwr_mode: Power mode on which the freq is enabled or not is to be
 * checked.
 *
 * Return: True if the frequency is present in the given power mode channel
 * list.
 */
bool wlan_reg_is_freq_enabled(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq,
			      enum supported_6g_pwr_types in_6g_pwr_mode);

/**
 * wlan_reg_is_freq_idx_enabled() - Checks if the given frequency index is
 * enabled on the given power mode or not. If the frequency index is not a 6G
 * frequency then the input power mode is ignored and only current channel list
 * is searched.
 *
 * @pdev: pdev pointer.
 * @freq_idx: input frequency index.
 * @in_6g_pwr_mode: Power mode on which the frequency index is enabled or not
 * is to be checked.
 *
 * Return: True if the frequency index is present in the given power mode
 * channel list.
 */
bool wlan_reg_is_freq_idx_enabled(struct wlan_objmgr_pdev *pdev,
				  enum channel_enum freq_idx,
				  enum supported_6g_pwr_types in_6g_pwr_mode);

/**
 * wlan_reg_get_pwrmode_chan_list() - Get the modified channel list. A modified
 * current channel list consists of 2G and 5G portions of the current channel
 * list and the 6G portion of the current channel list is derived from the input
 * 6g power type.
 * @pdev: Pointer to pdev
 * @chan_list: Pointer to buffer which stores list of regulatory_channels.
 * @in_6g_pwr_mode: 6GHz power type
 *
 * Return:
 * QDF_STATUS_SUCCESS: Success
 * QDF_STATUS_E_INVAL: Failed to get channel list
 */
QDF_STATUS wlan_reg_get_pwrmode_chan_list(struct wlan_objmgr_pdev *pdev,
					  struct regulatory_channel *chan_list,
					  enum supported_6g_pwr_types
					  in_6g_pwr_mode);

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_get_secondary_current_chan_list() - provide the pdev secondary
 * current channel list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_get_secondary_current_chan_list(
					struct wlan_objmgr_pdev *pdev,
					struct regulatory_channel *chan_list);
#endif

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
/**
 * wlan_reg_get_6g_afc_chan_list() - provide the pdev afc channel list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_get_6g_afc_chan_list(struct wlan_objmgr_pdev *pdev,
					 struct regulatory_channel *chan_list);

/**
 * wlan_reg_get_6g_afc_mas_chan_list() - provide the pdev afc master channel
 * list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_6g_afc_mas_chan_list(struct wlan_objmgr_pdev *pdev,
				  struct regulatory_channel *chan_list);

/**
 * wlan_reg_is_afc_power_event_received() - Checks if AFC power event is
 * received from the FW.
 *
 * @pdev: pdev ptr
 *
 * Return: true if AFC power event is received from the FW or false otherwise
 */
bool wlan_reg_is_afc_power_event_received(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_is_afc_done() - Check if AFC response enables the given frequency.
 * @pdev: pdev ptr
 * @freq: given frequency.
 *
 * Return: True if frequency is enabled, false otherwise.
 */
bool wlan_reg_is_afc_done(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_get_afc_req_id() - Get the AFC request ID
 * @pdev: pdev pointer
 * @req_id: Pointer to request id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_get_afc_req_id(struct wlan_objmgr_pdev *pdev,
				   uint64_t *req_id);

/**
 * wlan_reg_is_afc_expiry_event_received() - Checks if AFC power event is
 * received from the FW.
 *
 * @pdev: pdev ptr
 *
 * Return: true if AFC exipry event is received from the FW or false otherwise
 */
bool wlan_reg_is_afc_expiry_event_received(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_is_noaction_on_afc_pwr_evt() - Checks whether driver needs to
 * take action for AFC action or the response should be handled by the
 * user application.
 *
 * @pdev: pdev ptr
 *
 * Return: true if driver need not take action for AFC resp, false otherwise.
 */
bool
wlan_reg_is_noaction_on_afc_pwr_evt(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_get_afc_dev_deploy_type() - Get AFC device deployment type
 * @pdev: pdev pointer
 * @afc_dev_type: Pointer to afc device deployment type
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_afc_dev_deploy_type(struct wlan_objmgr_pdev *pdev,
				 enum reg_afc_dev_deploy_type *afc_dev_type);

/**
 * wlan_reg_is_sta_connect_allowed() - Check if STA connection allowed
 * @pdev: pdev pointer
 * @root_ap_pwr_mode: power mode of the Root AP.
 *
 * Return : True if STA Vap connection is allowed.
 */
bool
wlan_reg_is_sta_connect_allowed(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type root_ap_pwr_mode);

/**
 * wlan_reg_is_6ghz_freq_txable() - Check if the given frequency is tx-able.
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @in_6ghz_pwr_type: Input AP power type
 *
 * An SP channel is tx-able if the channel is present in the AFC response.
 * In case of non-OUTDOOR mode a channel is always tx-able (Assuming it is
 * enabled by regulatory).
 *
 * Return: True if the frequency is tx-able, else false.
 */
bool
wlan_reg_is_6ghz_freq_txable(struct wlan_objmgr_pdev *pdev,
			     qdf_freq_t freq,
			     enum supported_6g_pwr_types in_6ghz_pwr_mode);
#else
static inline bool
wlan_reg_is_afc_power_event_received(struct wlan_objmgr_pdev *pdev)
{
	return false;
}

static inline bool
wlan_reg_is_afc_done(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	return true;
}

static inline QDF_STATUS
wlan_reg_get_6g_afc_chan_list(struct wlan_objmgr_pdev *pdev,
			      struct regulatory_channel *chan_list)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool
wlan_reg_is_sta_connect_allowed(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type root_ap_pwr_mode)
{
	return true;
}

static inline bool
wlan_reg_is_6ghz_freq_txable(struct wlan_objmgr_pdev *pdev,
			     qdf_freq_t freq,
			     enum supported_6g_pwr_types in_6ghz_pwr_mode)
{
	return false;
}
#endif

/**
 * wlan_reg_get_bonded_channel_state_for_freq() - Get bonded channel freq state
 * @freq: channel frequency
 * @bw: channel band width
 * @sec_freq: secondary frequency
 *
 * Return: channel state
 */
enum channel_state
wlan_reg_get_bonded_channel_state_for_freq(struct wlan_objmgr_pdev *pdev,
					   qdf_freq_t freq,
					   enum phy_ch_width bw,
					   qdf_freq_t sec_freq);

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_get_bonded_channel_state_for_pwrmode() - Get bonded channel freq
 * state
 * @freq: channel frequency
 * @bw: channel band width
 * @sec_freq: secondary frequency
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 * @input_puncture_bitmap: input puncture bitmap
 *
 * Return: channel state
 */
enum channel_state
wlan_reg_get_bonded_channel_state_for_pwrmode(struct wlan_objmgr_pdev *pdev,
					      qdf_freq_t freq,
					      enum phy_ch_width bw,
					      qdf_freq_t sec_freq,
					      enum supported_6g_pwr_types
					      in_6g_pwr_mode,
					      uint16_t input_puncture_bitmap);
#endif

/**
 * wlan_reg_set_dfs_region() - set the dfs region
 * @pdev: pdev ptr
 * @dfs_reg: dfs region
 *
 * Return: void
 */
void wlan_reg_set_dfs_region(struct wlan_objmgr_pdev *pdev,
			     enum dfs_reg dfs_reg);

/**
 * wlan_reg_get_bw_value() - provide the channel center freq
 * @chan_num: chennal number
 *
 * Return: int
 */
uint16_t wlan_reg_get_bw_value(enum phy_ch_width bw);

/**
 * wlan_reg_get_domain_from_country_code() - provide the channel center freq
 * @reg_domain_ptr: regulatory domain ptr
 * @country_alpha2: country alpha2
 * @source: alpha2 source
 *
 * Return: int
 */
QDF_STATUS wlan_reg_get_domain_from_country_code(v_REGDOMAIN_t *reg_domain_ptr,
						 const uint8_t *country_alpha2,
						 enum country_src source);

/**
 * wlan_reg_dmn_get_opclass_from_channel() - provide the channel center freq
 * @country: country alpha2
 * @channel: channel number
 * @offset: offset
 *
 * Return: int
 */
uint16_t wlan_reg_dmn_get_opclass_from_channel(uint8_t *country,
					       uint8_t channel,
					       uint8_t offset);

/**
 * wlan_reg_get_opclass_from_freq_width() - Get operating class from frequency
 * @country: Country code.
 * @freq: Channel center frequency.
 * @ch_width: Channel width.
 * @behav_limit: Behaviour limit.
 *
 * Return: Error code.
 */
uint8_t wlan_reg_get_opclass_from_freq_width(uint8_t *country,
					     qdf_freq_t freq,
					     uint16_t ch_width,
					     uint16_t behav_limit);

/**
 * wlan_reg_get_band_cap_from_op_class() - Return band capability bitmap
 * @country: Pointer to Country code.
 * @num_of_opclass: Number of Operating class.
 * @opclass: Pointer to opclass.
 *
 * Return supported band bitmap based on the input operating class list
 * provided.
 *
 * Return: Return supported band capability
 */
uint8_t wlan_reg_get_band_cap_from_op_class(const uint8_t *country,
					    uint8_t num_of_opclass,
					    const uint8_t *opclass);

/**
 * wlan_reg_dmn_print_channels_in_opclass() - Print channels in op-class
 * @country: country alpha2
 * @opclass: oplcass
 *
 * Return: void
 */
void wlan_reg_dmn_print_channels_in_opclass(uint8_t *country,
					    uint8_t opclass);


/**
 * wlan_reg_dmn_get_chanwidth_from_opclass() - get channel width from
 *                                             operating class
 * @country: country alpha2
 * @channel: channel number
 * @opclass: operating class
 *
 * Return: int
 */
uint16_t wlan_reg_dmn_get_chanwidth_from_opclass(uint8_t *country,
						 uint8_t channel,
						 uint8_t opclass);

/**
 * wlan_reg_dmn_get_chanwidth_from_opclass_auto() - get channel width from
 * operating class. If opclass not found then search in global opclass.
 * @country: country alpha2
 * @channel: channel number
 * @opclass: operating class
 *
 * Return: int
 */
uint16_t wlan_reg_dmn_get_chanwidth_from_opclass_auto(uint8_t *country,
						      uint8_t channel,
						      uint8_t opclass);

/**
 * wlan_reg_dmn_set_curr_opclasses() - set operating class
 * @num_classes: number of classes
 * @class: operating class
 *
 * Return: int
 */
uint16_t wlan_reg_dmn_set_curr_opclasses(uint8_t num_classes,
					 uint8_t *class);

/**
 * wlan_reg_dmn_get_curr_opclasses() - get current oper classes
 * @num_classes: number of classes
 * @class: operating class
 *
 * Return: int
 */
uint16_t wlan_reg_dmn_get_curr_opclasses(uint8_t *num_classes,
					 uint8_t *class);


/**
 * wlan_reg_get_opclass_details() - Get details about the current opclass table.
 * @pdev: Pointer to pdev.
 * @reg_ap_cap: Pointer to reg_ap_cap.
 * @n_opclasses: Pointer to number of opclasses.
 * @max_supp_op_class: Maximum number of operating classes supported.
 * @global_tbl_lookup: Whether to lookup global op class tbl.
 *
 * Return: QDF_STATUS_SUCCESS if success, else return QDF_STATUS_FAILURE.
 */
QDF_STATUS
wlan_reg_get_opclass_details(struct wlan_objmgr_pdev *pdev,
			     struct regdmn_ap_cap_opclass_t *reg_ap_cap,
			     uint8_t *n_opclasses,
			     uint8_t max_supp_op_class,
			     bool global_tbl_lookup);

/**
 * wlan_reg_get_opclass_for_cur_hwmode() - Get details about the
 * opclass table for the current hwmode.
 * @pdev: Pointer to pdev.
 * @reg_ap_cap: Pointer to reg_ap_cap.
 * @n_opclasses: Pointer to number of opclasses.
 * @max_supp_op_class: Maximum number of operating classes supported.
 * @global_tbl_lookup: Whether to lookup global op class tbl.
 * @max_chwidth: Maximum channel width supported by cur hwmode
 * @is_80p80_supp: Bool to indicate if 80p80 is supported.
 *
 * Return: QDF_STATUS_SUCCESS if success, else return QDF_STATUS_FAILURE.
 */
QDF_STATUS
wlan_reg_get_opclass_for_cur_hwmode(struct wlan_objmgr_pdev *pdev,
				    struct regdmn_ap_cap_opclass_t *reg_ap_cap,
				    uint8_t *n_opclasses,
				    uint8_t max_supp_op_class,
				    bool global_tbl_lookup,
				    enum phy_ch_width max_chwidth,
				    bool is_80p80_supp);
/**
 * wlan_reg_get_cc_and_src () - get country code and src
 * @psoc: psoc ptr
 * @alpha2: country code alpha2
 *
 * Return: country_src
 */
enum country_src wlan_reg_get_cc_and_src(struct wlan_objmgr_psoc *psoc,
					 uint8_t *alpha);

/**
 * wlan_regulatory_init() - init regulatory component
 *
 * Return: Success or Failure
 */
QDF_STATUS wlan_regulatory_init(void);

/**
 * wlan_regulatory_deinit() - deinit regulatory component
 *
 * Return: Success or Failure
 */
QDF_STATUS wlan_regulatory_deinit(void);

/**
 * regulatory_psoc_open() - open regulatory component
 *
 * Return: Success or Failure
 */
QDF_STATUS regulatory_psoc_open(struct wlan_objmgr_psoc *psoc);


/**
 * regulatory_psoc_close() - close regulatory component
 *
 * Return: Success or Failure
 */
QDF_STATUS regulatory_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * regulatory_pdev_open() - Open regulatory component
 * @pdev: Pointer to pdev structure
 *
 * Return: Success or Failure
 */
QDF_STATUS regulatory_pdev_open(struct wlan_objmgr_pdev *pdev);

/**
 * regulatory_pdev_close() - Close regulatory component
 * @pdev: Pointer to pdev structure.
 *
 * Return: Success or Failure
 */
QDF_STATUS regulatory_pdev_close(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_freq_to_chan () - convert channel freq to channel number
 * @pdev: The physical dev to set current country for
 * @freq: frequency
 *
 * Return: true or false
 */
uint8_t wlan_reg_freq_to_chan(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq);

/**
 * wlan_reg_legacy_chan_to_freq () - convert chan to freq, for 2G and 5G
 * @chan: channel number
 *
 * Return: frequency
 */
qdf_freq_t wlan_reg_legacy_chan_to_freq(struct wlan_objmgr_pdev *pdev,
					uint8_t chan);

/**
 * wlan_reg_is_us() - reg is us country
 * @country: The country information
 *
 * Return: true or false
 */
bool wlan_reg_is_us(uint8_t *country);

/**
 * wlan_reg_is_etsi() - reg is a country in EU
 * @country: The country information
 *
 * Return: true or false
 */
bool wlan_reg_is_etsi(uint8_t *country);


/**
 * wlan_reg_ctry_support_vlp() - Country supports VLP or not
 * @country: The country information
 *
 * Return: true or false
 */
bool wlan_reg_ctry_support_vlp(uint8_t *country);

/**
 * wlan_reg_set_country() - Set the current regulatory country
 * @pdev: The physical dev to set current country for
 * @country: The country information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_set_country(struct wlan_objmgr_pdev *pdev,
				uint8_t *country);

/**
 * wlan_reg_set_11d_country() - Set the 11d regulatory country
 * @pdev: The physical dev to set current country for
 * @country: The country information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_set_11d_country(struct wlan_objmgr_pdev *pdev,
				    uint8_t *country);

/**
 * wlan_reg_register_chan_change_callback () - add chan change cbk
 * @psoc: psoc ptr
 * @cbk: callback
 * @arg: argument
 *
 * Return: true or false
 */
void wlan_reg_register_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					    void *cbk, void *arg);

/**
 * wlan_reg_unregister_chan_change_callback () - remove chan change cbk
 * @psoc: psoc ptr
 * @cbk:callback
 *
 * Return: true or false
 */
void wlan_reg_unregister_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					      void *cbk);

/**
 * wlan_reg_register_ctry_change_callback () - add country change cbk
 * @psoc: psoc ptr
 * @cbk: callback
 *
 * Return: None
 */
void wlan_reg_register_ctry_change_callback(struct wlan_objmgr_psoc *psoc,
					    void *cbk);

/**
 * wlan_reg_unregister_ctry_change_callback () - remove country change cbk
 * @psoc: psoc ptr
 * @cbk:callback
 *
 * Return: None
 */
void wlan_reg_unregister_ctry_change_callback(struct wlan_objmgr_psoc *psoc,
					      void *cbk);

/**
 * wlan_reg_is_11d_offloaded() - 11d offloaded supported
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool wlan_reg_is_11d_offloaded(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_reg_11d_enabled_on_host() - 11d enabled don host
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool wlan_reg_11d_enabled_on_host(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_reg_get_chip_mode() - get supported chip mode
 * @pdev: pdev pointer
 * @chip_mode: chip mode
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_reg_get_chip_mode(struct wlan_objmgr_pdev *pdev,
		uint64_t *chip_mode);

/**
 * wlan_reg_is_11d_scan_inprogress() - checks 11d scan status
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool wlan_reg_is_11d_scan_inprogress(struct wlan_objmgr_psoc *psoc);
/**
 * wlan_reg_get_freq_range() - Get 2GHz and 5GHz frequency range
 * @pdev: pdev pointer
 * @low_2g: low 2GHz frequency range
 * @high_2g: high 2GHz frequency range
 * @low_5g: low 5GHz frequency range
 * @high_5g: high 5GHz frequency range
 *
 * Return: QDF status
 */
QDF_STATUS wlan_reg_get_freq_range(struct wlan_objmgr_pdev *pdev,
		qdf_freq_t *low_2g,
		qdf_freq_t *high_2g,
		qdf_freq_t *low_5g,
		qdf_freq_t *high_5g);
/**
 * wlan_reg_get_tx_ops () - get regulatory tx ops
 * @psoc: psoc ptr
 *
 */
struct wlan_lmac_if_reg_tx_ops *
wlan_reg_get_tx_ops(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_reg_get_curr_regdomain() - Get current regdomain in use
 * @pdev: pdev pointer
 * @cur_regdmn: Current regdomain info
 *
 * Return: QDF status
 */
QDF_STATUS wlan_reg_get_curr_regdomain(struct wlan_objmgr_pdev *pdev,
		struct cur_regdmn_info *cur_regdmn);

#ifdef WLAN_REG_PARTIAL_OFFLOAD
/**
 * wlan_reg_is_regdmn_en302502_applicable() - Find if ETSI EN302_502 radar
 * pattern is applicable in the current regulatory domain.
 * @pdev:    Pdev ptr.
 *
 * Return: Boolean.
 * True:  If EN302_502 is applicable.
 * False: otherwise.
 */
bool wlan_reg_is_regdmn_en302502_applicable(struct wlan_objmgr_pdev *pdev);
#endif

/**
 * wlan_reg_modify_pdev_chan_range() - Compute current channel list for the
 * modified channel range in the regcap.
 * @pdev: pointer to wlan_objmgr_pdev.
 *
 * Return : QDF_STATUS
 */
QDF_STATUS wlan_reg_modify_pdev_chan_range(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_get_phybitmap() - Get phybitmap from regulatory pdev_priv_obj
 * @pdev: pdev pointer
 * @phybitmap: pointer to phybitmap
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_reg_get_phybitmap(struct wlan_objmgr_pdev *pdev,
				  uint16_t *phybitmap);

/**
 * wlan_reg_update_pdev_wireless_modes() - Update the wireless_modes in the
 * pdev_priv_obj with the input wireless_modes
 * @pdev: pointer to wlan_objmgr_pdev.
 * @wireless_modes: Wireless modes.
 *
 * Return : QDF_STATUS
 */
QDF_STATUS wlan_reg_update_pdev_wireless_modes(struct wlan_objmgr_pdev *pdev,
					       uint64_t wireless_modes);
/**
 * wlan_reg_disable_chan_coex() - Disable Coexisting channels based on the input
 * bitmask
 * @pdev: pointer to wlan_objmgr_pdev.
 * unii_5g_bitmap: UNII 5G bitmap.
 *
 * Return : QDF_STATUS
 */
#ifdef DISABLE_UNII_SHARED_BANDS
QDF_STATUS wlan_reg_disable_chan_coex(struct wlan_objmgr_pdev *pdev,
				      uint8_t unii_5g_bitmap);
#else
static inline QDF_STATUS
wlan_reg_disable_chan_coex(struct wlan_objmgr_pdev *pdev,
			   uint8_t unii_5g_bitmap)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_GET_USABLE_CHAN_LIST
/**
 * wlan_reg_get_usable_channel() - Get usable channels
 * @pdev: Pointer to pdev
 * @req_msg: Request msg
 * @res_msg: Response msg
 * @count: no of usable channels
 *
 * Return: qdf status
 */
QDF_STATUS
wlan_reg_get_usable_channel(struct wlan_objmgr_pdev *pdev,
			    struct get_usable_chan_req_params req_msg,
			    struct get_usable_chan_res_params *res_msg,
			    uint32_t *count);
#endif

#ifdef CONFIG_CHAN_FREQ_API
/**
 * wlan_reg_is_same_band_freqs() - Check if two channel frequencies
 * have same band
 * @freq1: Frequency 1
 * @freq2: Frequency 2
 *
 * Return: true if both the channel frequency has the same band.
 */
#define WLAN_REG_IS_SAME_BAND_FREQS(freq1, freq2) \
	wlan_reg_is_same_band_freqs(freq1, freq2)
bool wlan_reg_is_same_band_freqs(qdf_freq_t freq1, qdf_freq_t freq2);

/**
 * wlan_reg_get_chan_enum_for_freq() - Get channel enum for given channel center
 * frequency
 * @freq: Channel center frequency
 *
 * Return: Channel enum
 */
enum channel_enum wlan_reg_get_chan_enum_for_freq(qdf_freq_t freq);

/**
 * wlan_reg_get_min_max_bw_for_chan_index() - To get min and max BW supported
 * by channel enum
 * @pdev: pointer to pdev
 * @chn_idx: enum channel_enum
 * @min bw: min bw
 * @max bw: max bw
 *
 * Return: SUCCESS/FAILURE
 */
QDF_STATUS
wlan_reg_get_min_max_bw_for_chan_index(struct wlan_objmgr_pdev *pdev,
				       enum channel_enum chan_idx,
				       uint16_t *min_bw, uint16_t *max_bw);

/**
 * wlan_reg_is_freq_present_in_cur_chan_list() - Check if channel is present
 * in the current channel list
 * @pdev: pdev pointer
 * @freq: Channel center frequency
 *
 * Return: true if channel is present in current channel list
 */
bool wlan_reg_is_freq_present_in_cur_chan_list(struct wlan_objmgr_pdev *pdev,
					       qdf_freq_t freq);

/**
 * wlan_reg_update_nol_history_ch_for_freq() - Set nol-history flag for the
 * channels in the list.
 *
 * @pdev: Pdev ptr
 * @ch_list: Input channel list.
 * @num_ch: Number of channels.
 * @nol_history_ch: Nol history value.
 *
 * Return: void
 */
void wlan_reg_update_nol_history_ch_for_freq(struct wlan_objmgr_pdev *pdev,
					     uint16_t *ch_list,
					     uint8_t num_ch,
					     bool nol_history_ch);

/**
 * wlan_reg_chan_has_dfs_attribute_for_freq() - check channel has dfs
 * attribute flag
 * @freq: channel center frequency.
 *
 * This API get chan initial dfs attribute from regdomain
 *
 * Return: true if chan is dfs, otherwise false
 */
bool
wlan_reg_chan_has_dfs_attribute_for_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t freq);

/**
 * wlan_reg_get_channel_list_with_power_for_freq() - Provide the channel list
 * with power
 * @ch_list: pointer to the channel list.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_channel_list_with_power_for_freq(struct wlan_objmgr_pdev *pdev,
					      struct channel_power *ch_list,
					      uint8_t *num_chan);
/**
 * wlan_reg_get_5g_bonded_channel_state_for_freq() - Get 5G bonded channel state
 * @pdev: The physical dev to program country code or regdomain
 * @freq: channel frequency.
 * @bw: channel band width
 *
 * Return: channel state
 */
enum channel_state
wlan_reg_get_5g_bonded_channel_state_for_freq(struct wlan_objmgr_pdev *pdev,
					      qdf_freq_t freq,
					      enum phy_ch_width bw);

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_get_5g_bonded_channel_state_for_pwrmode() - Get 5G bonded channel
 * state.
 * @pdev: The physical dev to program country code or regdomain
 * @freq: channel frequency.
 * @ch_params: channel parameters
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 *
 * Return: channel state
 */
enum channel_state
wlan_reg_get_5g_bonded_channel_state_for_pwrmode(struct wlan_objmgr_pdev *pdev,
						 qdf_freq_t freq,
						 struct ch_params *ch_params,
						 enum supported_6g_pwr_types
						 in_6g_pwr_type);
#endif

/**
 * wlan_reg_get_2g_bonded_channel_state_for_freq() - Get 2G bonded channel state
 * @pdev: The physical dev to program country code or regdomain
 * @freq: channel center frequency.
 * @sec_ch_freq: Secondary channel center frequency.
 * @bw: channel band width
 *
 * Return: channel state
 */
enum channel_state
wlan_reg_get_2g_bonded_channel_state_for_freq(struct wlan_objmgr_pdev *pdev,
					      qdf_freq_t freq,
					      qdf_freq_t sec_ch_freq,
					      enum phy_ch_width bw);

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_get_channel_state_for_pwrmode() - Get channel state from regulatory
 * @pdev: Pointer to pdev
 * @freq: channel center frequency.
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 *
 * Return: channel state
 */
enum channel_state
wlan_reg_get_channel_state_for_pwrmode(
				    struct wlan_objmgr_pdev *pdev,
				    qdf_freq_t freq,
				    enum supported_6g_pwr_types in_6g_pwr_type);
#endif

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_get_channel_state_from_secondary_list_for_freq() - Get channel state
 * from secondary regulatory current channel list
 * @pdev: Pointer to pdev
 * @freq: channel center frequency.
 *
 * Return: channel state
 */
enum channel_state wlan_reg_get_channel_state_from_secondary_list_for_freq(
						struct wlan_objmgr_pdev *pdev,
						qdf_freq_t freq);

/**
 * wlan_reg_get_channel_list_with_power() - Provide channel list with tx power
 * @ch_list: pointer to the channel list.
 * @num_chan: Number of channels which has been filed in ch_list
 * @in_6g_pwr_type: 6G power type corresponding to which 6G channels needs to
 * be provided
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_channel_list_with_power(
				struct wlan_objmgr_pdev *pdev,
				struct channel_power *ch_list,
				uint8_t *num_chan,
				enum supported_6g_pwr_types in_6g_pwr_type);
#endif

#ifdef WLAN_FEATURE_11BE
/**
 * wlan_reg_is_punc_bitmap_valid() - is puncture bitmap valid or not
 * @bw: Input channel width.
 * @puncture_bitmap Input puncture bitmap.
 *
 * Return: true if given puncture bitmap is valid
 */
bool wlan_reg_is_punc_bitmap_valid(enum phy_ch_width bw,
				   uint16_t puncture_bitmap);

#ifdef QCA_DFS_BW_PUNCTURE
/**
 * wlan_reg_find_nearest_puncture_pattern() - is proposed bitmap valid or not
 * @bw: Input channel width.
 * @proposed_bitmap: Input puncture bitmap.
 *
 * Return: Radar bitmap if it is valid.
 */
uint16_t wlan_reg_find_nearest_puncture_pattern(enum phy_ch_width bw,
						uint16_t proposed_bitmap);
#else
static inline
uint16_t wlan_reg_find_nearest_puncture_pattern(enum phy_ch_width bw,
						uint16_t proposed_bitmap)
{
	return 0;
}
#endif /* QCA_DFS_BW_PUNCTURE */

/**
 * wlan_reg_extract_puncture_by_bw() - generate new puncture bitmap from
 *                                     original puncture bitmap and bandwidth
 *                                     based on new bandwidth
 * @ori_bw: original bandwidth
 * @ori_puncture_bitmap: original puncture bitmap
 * @freq: frequency of primary channel
 * @cen320_freq: center frequency of 320 MHZ if channel width is 320
 * @new_bw new bandwidth
 * @new_puncture_bitmap: output of puncture bitmap
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_extract_puncture_by_bw(enum phy_ch_width ori_bw,
					   uint16_t ori_puncture_bitmap,
					   qdf_freq_t freq,
					   qdf_freq_t cen320_freq,
					   enum phy_ch_width new_bw,
					   uint16_t *new_puncture_bitmap);

/**
 * wlan_reg_set_create_punc_bitmap() - set is_create_punc_bitmap of ch_params
 * @ch_params: ch_params to set
 * @is_create_punc_bitmap: is create punc bitmap
 *
 * Return: NULL
 */
void wlan_reg_set_create_punc_bitmap(struct ch_params *ch_params,
				     bool is_create_punc_bitmap);

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_apply_puncture() - apply puncture to regulatory
 * @pdev: pdev
 * @puncture_bitmap: puncture bitmap
 * @freq: sap operation freq
 * @bw: band width
 * @cen320_freq: 320 MHz center freq
 *
 * When start ap, apply puncture to regulatory, set static puncture flag
 * for all 20 MHz sub channels of current bonded channel in master channel list
 * of pdev, and disable 20 MHz sub channel in current channel list if static
 * puncture flag is set.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_apply_puncture(struct wlan_objmgr_pdev *pdev,
				   uint16_t puncture_bitmap,
				   qdf_freq_t freq,
				   enum phy_ch_width bw,
				   qdf_freq_t cen320_freq);

/**
 * wlan_reg_remove_puncture() - Remove puncture from regulatory
 * @pdev: pdev
 *
 * When stop ap, remove puncture from regulatory, clear static puncture flag
 * for all 20 MHz sub channels in master channel list of pdev, and don't disable
 * 20 MHz sub channel in current channel list if static puncture flag is not
 * set.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_remove_puncture(struct wlan_objmgr_pdev *pdev);
#else
static inline
QDF_STATUS wlan_reg_apply_puncture(struct wlan_objmgr_pdev *pdev,
				   uint16_t puncture_bitmap,
				   qdf_freq_t freq,
				   enum phy_ch_width bw,
				   qdf_freq_t cen320_freq)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_reg_remove_puncture(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_fill_channel_list_for_pwrmode() - Fills the reg_channel_list
 * (list of channels)
 * @pdev: Pointer to struct wlan_objmgr_pdev.
 * @freq: Center frequency of the primary channel in MHz
 * @sec_ch_2g_freq: Secondary channel center frequency.
 * @ch_width: Channel width of type 'enum phy_ch_width'.
 * @band_center_320: Center frequency of 320MHZ channel.
 * @chan_list: Pointer to struct reg_channel_list to be filled (Output param).
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 * @treat_nol_chan_as_disabled: bool to treat nol channel as enabled or
 * disabled. If set to true, nol chan is considered as disabled in chan search.
 *
 * Return: None
 */
void wlan_reg_fill_channel_list_for_pwrmode(
				struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq,
				qdf_freq_t sec_ch_2g_freq,
				enum phy_ch_width ch_width,
				qdf_freq_t band_center_320,
				struct reg_channel_list *chan_list,
				enum supported_6g_pwr_types in_6g_pwr_type,
				bool treat_nol_chan_as_disabled);
#endif
#else
static inline
QDF_STATUS wlan_reg_extract_puncture_by_bw(enum phy_ch_width ori_bw,
					   uint16_t ori_puncture_bitmap,
					   qdf_freq_t freq,
					   enum phy_ch_width new_bw,
					   uint16_t *new_puncture_bitmap)
{
	return QDF_STATUS_SUCCESS;
}

static inline void wlan_reg_set_create_punc_bitmap(struct ch_params *ch_params,
						   bool is_create_punc_bitmap)
{
}

static inline
QDF_STATUS wlan_reg_apply_puncture(struct wlan_objmgr_pdev *pdev,
				   uint16_t puncture_bitmap,
				   qdf_freq_t freq,
				   enum phy_ch_width bw,
				   qdf_freq_t cen320_freq)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_reg_remove_puncture(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
uint16_t wlan_reg_find_nearest_puncture_pattern(enum phy_ch_width bw,
						uint16_t proposed_bitmap)
{
	return 0;
}
#endif

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_set_channel_params_for_pwrmode() - Sets channel parameteres for
 * given bandwidth
 * @pdev: The physical dev to program country code or regdomain
 * @freq: channel center frequency.
 * @sec_ch_2g_freq: Secondary channel center frequency.
 * @ch_params: pointer to the channel parameters.
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 *
 * Return: None
 */
void wlan_reg_set_channel_params_for_pwrmode(struct wlan_objmgr_pdev *pdev,
					     qdf_freq_t freq,
					     qdf_freq_t sec_ch_2g_freq,
					     struct ch_params *ch_params,
					     enum supported_6g_pwr_types
					     in_6g_pwr_mode);
#endif

/**
 * wlan_reg_get_channel_cfreq_reg_power_for_freq() - Provide the channel
 * regulatory power
 * @freq: channel center frequency
 *
 * Return: int
 */
uint8_t wlan_reg_get_channel_reg_power_for_freq(struct wlan_objmgr_pdev *pdev,
						qdf_freq_t freq);

/**
 * wlan_reg_get_bonded_chan_entry() - Fetch the bonded channel pointer given a
 * frequency and channel width.
 * @freq: Input frequency in MHz.
 * @chwidth: Input channel width of enum phy_ch_width.
 * @cen320_freq: 320 MHz center frequency in MHz. In 6GHz band 320 MHz channel
 *               are overlapping. The exact band should be therefore identified
 *               by the center frequency of the 320 Mhz channel.
 *
 * Return: A valid bonded channel pointer if found, else NULL.
 */
const struct bonded_channel_freq *
wlan_reg_get_bonded_chan_entry(qdf_freq_t freq, enum phy_ch_width chwidth,
			       qdf_freq_t cen320_freq);

/**
 * wlan_reg_update_nol_ch_for_freq () - set nol channel
 * @pdev: pdev ptr
 * @chan_freq_list: channel list to be returned
 * @num_ch: number of channels
 * @nol_ch: nol flag
 *
 * Return: void
 */
void wlan_reg_update_nol_ch_for_freq(struct wlan_objmgr_pdev *pdev,
				     uint16_t *chan_freq_list,
				     uint8_t num_ch,
				     bool nol_ch);

/**
 * wlan_reg_is_dfs_freq() - Checks the channel state for DFS
 * @freq: Channel center frequency
 *
 * Return: true or false
 */
bool wlan_reg_is_dfs_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_is_dsrc_freq() - Checks if the channel is dsrc channel or not
 * @freq: Channel center frequency
 *
 * Return: true or false
 */
bool wlan_reg_is_dsrc_freq(qdf_freq_t freq);

/**
 * wlan_reg_is_passive_or_disable_for_pwrmode() - Checks chan state for passive
 * and disabled
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 * @in_6g_pwr_mode: Input 6GHz power mode
 *
 * Return: true or false
 */
bool wlan_reg_is_passive_or_disable_for_pwrmode(
				struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq,
				enum supported_6g_pwr_types in_6g_pwr_mode);

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_is_disable_for_pwrmode() - Checks chan state for disabled
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 *
 * Return: true or false
 */
bool wlan_reg_is_disable_for_pwrmode(
				  struct wlan_objmgr_pdev *pdev,
				  qdf_freq_t freq,
				  enum supported_6g_pwr_types in_6g_pwr_mode);
#endif

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_is_disable_in_secondary_list_for_freq() - Checks in the secondary
 * channel list to see if chan state is disabled
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 *
 * Return: true or false
 */
bool wlan_reg_is_disable_in_secondary_list_for_freq(
						struct wlan_objmgr_pdev *pdev,
						qdf_freq_t freq);

/**
 * wlan_reg_is_enable_in_secondary_list_for_freq() - Checks in the secondary
 * channel list to see if chan state is enabled
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 *
 * Return: true or false
 */
bool wlan_reg_is_enable_in_secondary_list_for_freq(
						struct wlan_objmgr_pdev *pdev,
						qdf_freq_t freq);

/**
 * wlan_reg_is_dfs_in_secondary_list_for_freq() - hecks the channel state for
 * DFS from the secondary channel list
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 *
 * Return: true or false
 */
bool wlan_reg_is_dfs_in_secondary_list_for_freq(struct wlan_objmgr_pdev *pdev,
						qdf_freq_t freq);

/**
 * wlan_reg_get_chan_pwr_attr_from_secondary_list_for_freq() - get channel
 * power attributions from secondary channel list
 * @pdev: pdev ptr
 * @freq: channel center frequency
 * @is_psd: pointer to retrieve value whether channel power is psd
 * @tx_power: pointer to retrieve value of channel eirp tx power
 * @psd_eirp: pointer to retrieve value of channel psd eirp power
 * @flags: pointer to retrieve value of channel flags
 *
 * Return: QDF STATUS
 */
QDF_STATUS
wlan_reg_get_chan_pwr_attr_from_secondary_list_for_freq(
				struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				bool *is_psd, uint16_t *tx_power,
				uint16_t *psd_eirp, uint32_t *flags);

/**
 * wlan_reg_decide_6ghz_power_within_bw_for_freq() - decide minimum tx power in
 * bandwidth and 6 GHz power type
 * @pdev: pdev ptr
 * @freq: channel center frequency
 * @bw: channel bandwidth
 * @is_psd: pointer to retrieve value whether channel power is psd
 * @min_tx_power: pointer to retrieve minimum tx power in bandwidth
 * @min_psd_eirp: pointer to retrieve minimum psd eirp in bandwidth
 * @power_type: pointer to retrieve 6 GHz power type
 *
 * Return: QDF STATUS
 */
QDF_STATUS
wlan_reg_decide_6ghz_power_within_bw_for_freq(struct wlan_objmgr_pdev *pdev,
					      qdf_freq_t freq,
					      enum phy_ch_width bw,
					      bool *is_psd,
					      uint16_t *min_tx_power,
					      int16_t *min_psd_eirp,
					      enum reg_6g_ap_type *power_type);
#endif

/**
 * wlan_reg_is_passive_for_freq() - Check the channel flags to see if the
 * passive flag is set
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 *
 * Return: true or false
 */
bool wlan_reg_is_passive_for_freq(struct wlan_objmgr_pdev *pdev,
				  qdf_freq_t freq);

/**
 * wlan_reg_freq_to_band() - Get band from channel number
 * @freq:Channel frequency in MHz
 *
 * Return: wifi band
 */
enum reg_wifi_band wlan_reg_freq_to_band(qdf_freq_t freq);

/**
 * wlan_reg_min_chan_freq() - Minimum channel frequency supported
 *
 * Return: frequency
 */
qdf_freq_t wlan_reg_min_chan_freq(void);

/**
 * wlan_reg_max_chan_freq() - Return max. frequency
 *
 * Return: frequency
 */
qdf_freq_t wlan_reg_max_chan_freq(void);

/**
 * wlan_reg_freq_width_to_chan_op_class() -Get op class from freq
 * @pdev: pdev ptr
 * @freq: channel frequency
 * @chan_width: channel width
 * @global_tbl_lookup: whether to look up global table
 * @behav_limit: behavior limit
 * @op_class: operating class
 * @chan_num: channel number
 *
 * Return: void
 */
void wlan_reg_freq_width_to_chan_op_class(struct wlan_objmgr_pdev *pdev,
					  qdf_freq_t freq,
					  uint16_t chan_width,
					  bool global_tbl_lookup,
					  uint16_t behav_limit,
					  uint8_t *op_class,
					  uint8_t *chan_num);

/**
 * wlan_reg_freq_width_to_chan_op_class_auto() - convert frequency to
 * operating class,channel
 * @pdev: pdev pointer
 * @freq: channel frequency in mhz
 * @chan_width: channel width
 * @global_tbl_lookup: whether to lookup global op class tbl
 * @behav_limit: behavior limit
 * @op_class: operating class
 * @chan_num: channel number
 *
 * Return: Void.
 */
void wlan_reg_freq_width_to_chan_op_class_auto(struct wlan_objmgr_pdev *pdev,
					       qdf_freq_t freq,
					       uint16_t chan_width,
					       bool global_tbl_lookup,
					       uint16_t behav_limit,
					       uint8_t *op_class,
					       uint8_t *chan_num);

/**
 * wlan_reg_freq_to_chan_and_op_class() - Converts freq to oper class
 * @pdev: pdev ptr
 * @freq: channel frequency
 * @global_tbl_lookup: whether to look up global table
 * @behav_limit: behavior limit
 * @op_class: operating class
 * @chan_num: channel number
 *
 * Return: void
 */
void wlan_reg_freq_to_chan_op_class(struct wlan_objmgr_pdev *pdev,
				    qdf_freq_t freq,
				    bool global_tbl_lookup,
				    uint16_t behav_limit,
				    uint8_t *op_class,
				    uint8_t *chan_num);

/**
 * wlan_reg_is_freq_in_country_opclass() - checks frequency in (ctry, op class)
 *                                         pair
 * @pdev: pdev ptr
 * @country: country information
 * @op_class: operating class
 * @chan_freq: channel frequency
 *
 * Return: bool
 */
bool wlan_reg_is_freq_in_country_opclass(struct wlan_objmgr_pdev *pdev,
					 const uint8_t country[3],
					 uint8_t op_class,
					 qdf_freq_t chan_freq);
/**
 * wlan_reg_get_5g_bonded_channel_and_state_for_freq()- Return the channel
 * state for a 5G or 6G channel frequency based on the channel width and
 * bonded channel.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @bw Channel Width.
 * @bonded_chan_ptr_ptr: Pointer to bonded_channel_freq.
 *
 * Return: Channel State
 */
enum channel_state
wlan_reg_get_5g_bonded_channel_and_state_for_freq(struct wlan_objmgr_pdev *pdev,
						  uint16_t freq,
						  enum phy_ch_width bw,
						  const
						  struct bonded_channel_freq
						  **bonded_chan_ptr_ptr);

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * wlan_reg_get_5g_bonded_channel_and_state_for_pwrmode()- Return the channel
 * state for a 5G or 6G channel frequency based on the channel width and
 * bonded channel.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @bw Channel Width.
 * @bonded_chan_ptr_ptr: Pointer to bonded_channel_freq.
 * @in_6g_pwr_type: 6g power type which decides 6G channel list lookup.
 * @input_puncture_bitmap: Input puncture bitmap
 *
 * Return: Channel State
 */
enum channel_state
wlan_reg_get_5g_bonded_channel_and_state_for_pwrmode(
						  struct wlan_objmgr_pdev *pdev,
						  uint16_t freq,
						  enum phy_ch_width bw,
						  const
						  struct bonded_channel_freq
						  **bonded_chan_ptr_ptr,
						  enum supported_6g_pwr_types
						  in_6g_pwr_mode,
						  uint16_t input_puncture_bitmap);
#endif
#endif /*CONFIG_CHAN_FREQ_API */

/**
 * wlan_reg_get_op_class_width() - Get operating class chan width
 * @pdev: pdev ptr
 * @freq: channel frequency
 * @global_tbl_lookup: whether to look up global table
 * @op_class: operating class
 * @chan_num: channel number
 *
 * Return: channel width of op class
 */
uint16_t wlan_reg_get_op_class_width(struct wlan_objmgr_pdev *pdev,
				     uint8_t op_class,
				     bool global_tbl_lookup);

/**
 * wlan_reg_is_5ghz_op_class() - Check if the input opclass is a 5GHz opclass.
 * @country: Country code.
 * @op_class: Operating class.
 *
 * Return: Return true if input the opclass is a 5GHz opclass,
 * else return false.
 */
bool wlan_reg_is_5ghz_op_class(const uint8_t *country, uint8_t op_class);

/**
 * wlan_reg_is_2ghz_op_class() - Check if the input opclass is a 2.4GHz opclass.
 * @country: Country code.
 * @op_class: Operating class.
 *
 * Return: Return true if input the opclass is a 2.4GHz opclass,
 * else return false.
 */
bool wlan_reg_is_2ghz_op_class(const uint8_t *country, uint8_t op_class);

/**
 * wlan_reg_is_6ghz_op_class() - Whether 6ghz oper class
 * @pdev: pdev ptr
 * @op_class: operating class
 *
 * Return: bool
 */
bool wlan_reg_is_6ghz_op_class(struct wlan_objmgr_pdev *pdev,
			       uint8_t op_class);

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_is_6ghz_supported() - Whether 6ghz is supported
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool wlan_reg_is_6ghz_supported(struct wlan_objmgr_psoc *psoc);
#endif

#ifdef HOST_OPCLASS_EXT
/**
 * wlan_reg_country_chan_opclass_to_freq() - Convert channel number to
 * frequency based on country code and op class
 * @pdev: pdev object.
 * @country: country code.
 * @chan: IEEE Channel Number.
 * @op_class: Opclass.
 * @strict: flag to find channel from matched operating class code.
 *
 * Look up (channel, operating class) pair in country operating class tables
 * and return the channel frequency.
 * If not found and "strict" flag is false, try to get frequency (Mhz) by
 * channel number only.
 *
 * Return: Channel center frequency else return 0.
 */
qdf_freq_t
wlan_reg_country_chan_opclass_to_freq(struct wlan_objmgr_pdev *pdev,
				      const uint8_t country[3],
				      uint8_t chan, uint8_t op_class,
				      bool strict);
#endif

/**
 * reg_chan_opclass_to_freq() - Convert channel number and opclass to frequency
 * @chan: IEEE Channel Number.
 * @op_class: Opclass.
 * @global_tbl_lookup: Global table lookup.
 *
 * Return: Channel center frequency else return 0.
 */
uint16_t wlan_reg_chan_opclass_to_freq(uint8_t chan,
				       uint8_t op_class,
				       bool global_tbl_lookup);

/**
 * wlan_reg_chan_opclass_to_freq_auto() - Convert channel number and opclass to
 * frequency
 * @chan: IEEE channel number
 * @op_class: Operating class of channel
 * @global_tbl_lookup: Flag to determine if global table has to be looked up
 *
 * Return: Channel center frequency if valid, else zero
 */

qdf_freq_t wlan_reg_chan_opclass_to_freq_auto(uint8_t chan, uint8_t op_class,
					      bool global_tbl_lookup);

#ifdef CHECK_REG_PHYMODE
/**
 * wlan_reg_get_max_phymode() - Find the best possible phymode given a
 * phymode, a frequency, and per-country regulations
 * @pdev: pdev pointer
 * @phy_in: phymode that the user requested
 * @freq: current operating center frequency
 *
 * Return: maximum phymode allowed in current country that is <= phy_in
 */
enum reg_phymode wlan_reg_get_max_phymode(struct wlan_objmgr_pdev *pdev,
					  enum reg_phymode phy_in,
					  qdf_freq_t freq);
#else
static inline enum reg_phymode
wlan_reg_get_max_phymode(struct wlan_objmgr_pdev *pdev,
			 enum reg_phymode phy_in,
			 qdf_freq_t freq)
{
	return REG_PHYMODE_INVALID;
}
#endif /* CHECK_REG_PHYMODE */

#ifdef CONFIG_REG_CLIENT
/**
 * wlan_reg_band_bitmap_to_band_info() - Convert the band_bitmap to a
 *	band_info enum
 * @band_bitmap: bitmap on top of reg_wifi_band of bands enabled
 *
 * Return: BAND_ALL if both 2G and 5G band is enabled
 *	BAND_2G if 2G is enabled but 5G isn't
 *	BAND_5G if 5G is enabled but 2G isn't
 */
enum band_info wlan_reg_band_bitmap_to_band_info(uint32_t band_bitmap);

/**
 * wlan_reg_update_tx_power_on_ctry_change() - Update tx power during
 * country code change (without channel change) OR if fcc constraint is set
 * @pdev: Pointer to pdev
 * @vdev_id: vdev ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_update_tx_power_on_ctry_change(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id);

/**
 * wlan_reg_modify_indoor_concurrency() - Update the indoor concurrency list
 * in regulatory pdev context
 *
 * @pdev: pointer to pdev
 * @vdev_id: vdev id
 * @freq: frequency
 * @width: channel width
 * @add: add or delete entry
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_modify_indoor_concurrency(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id, uint32_t freq,
				   enum phy_ch_width width, bool add);

/**
 * wlan_reg_recompute_current_chan_list() - Recompute the current channel list
 * based on the regulatory change
 *
 * @psoc: pointer to psoc
 * @pdev: pointer to pdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_recompute_current_chan_list(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_pdev *pdev);
#endif

#if defined(CONFIG_BAND_6GHZ)
/**
 * wlan_reg_get_cur_6g_ap_pwr_type() - Get the current 6G regulatory AP power
 * type.
 * @pdev: Pointer to PDEV object.
 * @reg_cur_6g_ap_pwr_type: The current regulatory 6G AP power type ie.
 * LPI/SP/VLP.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_get_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type *reg_cur_6g_ap_pwr_type);

/**
 * wlan_reg_get_cur_6g_client_type() - Get the current 6G regulatory client
 * type.
 * @pdev: Pointer to PDEV object.
 * @reg_cur_6g_client_mobility_type: The current regulatory 6G client type ie.
 * default/subordinate.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_get_cur_6g_client_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_client_type
				*reg_cur_6g_client_mobility_type);

/**
 * wlan_reg_set_cur_6ghz_client_type() - Set the cur 6 GHz regulatory client
 * type to the given value.
 * @pdev: Pointer to PDEV object.
 * @in_6ghz_client_type: Input Client type to be set ie. default/subordinate.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_set_cur_6ghz_client_type(struct wlan_objmgr_pdev *pdev,
				  enum reg_6g_client_type in_6ghz_client_type);

/**
 * wlan_reg_set_6ghz_client_type_from_target() - Set the current 6 GHz
 * regulatory client type to the value received from target.
 * @pdev: Pointer to PDEV object.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_set_6ghz_client_type_from_target(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_get_rnr_tpe_usable() - Tells if RNR IE is applicable for current
 * domain.
 * @pdev: Pointer to PDEV object.
 * @reg_rnr_tpe_usable: Pointer to hold the bool value, true if RNR IE is
 * applicable, else false.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wlan_reg_get_rnr_tpe_usable(struct wlan_objmgr_pdev *pdev,
				       bool *reg_rnr_tpe_usable);

/**
 * wlan_reg_get_unspecified_ap_usable() - Tells if AP type unspecified by 802.11
 * can be used or not.
 * @pdev: Pointer to PDEV object.
 * @reg_unspecified_ap_usable: Pointer to hold the bool value, true if
 * unspecified AP types can be used in the IE, else false.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wlan_reg_get_unspecified_ap_usable(struct wlan_objmgr_pdev *pdev,
					      bool *reg_unspecified_ap_usable);

/**
 * wlan_reg_is_6g_psd_power() - Checks if given freq is PSD power
 *
 * @pdev: pdev ptr
 * @freq: channel frequency
 *
 * Return: true if channel is PSD power or false otherwise
 */
bool wlan_reg_is_6g_psd_power(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_get_6g_chan_ap_power() - Finds the AP TX power for the given channel
 *	frequency
 *
 * @pdev: pdev ptr
 * @chan_freq: channel frequency
 * @is_psd: is channel PSD or not
 * @tx_power: transmit power to fill for chan_freq
 * @eirp_psd_power: EIRP power, will only be filled if is_psd is true
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_get_6g_chan_ap_power(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t chan_freq, bool *is_psd,
					 uint16_t *tx_power,
					 uint16_t *eirp_psd_power);

/**
 * wlan_reg_get_client_power_for_connecting_ap() - Find the channel information
 *	when device is operating as a client
 *
 * @pdev: pdev ptr
 * @ap_type: type of AP that device is connected to
 * @chan_freq: channel frequency
 * @is_psd: is channel PSD or not
 * @tx_power: transmit power to fill for chan_freq
 * @eirp_psd_power: EIRP power, will only be filled if is_psd is true
 *
 * This function is meant to be called to find the channel frequency power
 * information for a client when the device is operating as a client. It will
 * fill in the parameters tx_power and eirp_psd_power. eirp_psd_power will
 * only be filled if the channel is PSD.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_client_power_for_connecting_ap(struct wlan_objmgr_pdev *pdev,
					    enum reg_6g_ap_type ap_type,
					    qdf_freq_t chan_freq,
					    bool is_psd, uint16_t *tx_power,
					    uint16_t *eirp_psd_power);

/**
 * wlan_reg_get_client_power_for_6ghz_ap() - Find the channel information when
 *	device is operating as a 6GHz AP
 *
 * @pdev: pdev ptr
 * @client_type: type of client that is connected to our AP
 * @chan_freq: channel frequency
 * @is_psd: is channel PSD or not
 * @tx_power: transmit power to fill for chan_freq
 * @eirp_psd_power: EIRP power, will only be filled if is_psd is true
 *
 * This function is meant to be called to find the channel frequency power
 * information for a client when the device is operating as an AP. It will fill
 * in the parameter is_psd, tx_power, and eirp_psd_power. eirp_psd_power will
 * only be filled if the channel is PSD.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_client_power_for_6ghz_ap(struct wlan_objmgr_pdev *pdev,
				      enum reg_6g_client_type client_type,
				      qdf_freq_t chan_freq,
				      bool *is_psd, uint16_t *tx_power,
				      uint16_t *eirp_psd_power);

/**
 * wlan_reg_decide_6g_ap_pwr_type() - Decide which power mode AP should operate
 * in
 *
 * @pdev: pdev ptr
 *
 * Return: AP power type
 */
enum reg_6g_ap_type
wlan_reg_decide_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_set_ap_pwr_and_update_chan_list() - Set the AP power mode and
 * recompute the current channel list
 *
 * @pdev: pdev ptr
 * @ap_pwr_type: the AP power type to update to
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_set_ap_pwr_and_update_chan_list(struct wlan_objmgr_pdev *pdev,
					 enum reg_6g_ap_type ap_pwr_type);

/**
 * wlan_reg_get_best_6g_pwr_type() - Returns the best 6g power type supported
 * for a given frequency.
 * @pdev: pdev pointer
 * @freq: input frequency.
 *
 * Return: supported_6g_pwr_types enum.
 */
enum supported_6g_pwr_types
wlan_reg_get_best_6g_pwr_type(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_conv_6g_ap_type_to_supported_6g_pwr_types() - Converts the 6G AP
 * power type to 6g supported power type enum.
 * @ap_pwr_type: input 6G AP power type.
 *
 * Return: supported_6g_pwr_types enum.
 */
enum supported_6g_pwr_types
wlan_reg_conv_6g_ap_type_to_supported_6g_pwr_types(enum reg_6g_ap_type
						   ap_pwr_type);

/**
 * wlan_reg_conv_supported_6g_pwr_type_to_ap_pwr_type() - The supported 6G power
 * type is a combination of AP and client power types. This API return the 6G AP
 * power type portion of the supported 6G power type.
 * @in_6g_pwr_type: input 6G supported power type.
 *
 * Return: 6G AP power type.
 */
enum reg_6g_ap_type
wlan_reg_conv_supported_6g_pwr_type_to_ap_pwr_type(enum supported_6g_pwr_types
						  in_6g_pwr_type);
#else /* !CONFIG_BAND_6GHZ */
static inline QDF_STATUS
wlan_reg_get_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type *reg_cur_6g_ap_pwr_type)
{
	*reg_cur_6g_ap_pwr_type = REG_CURRENT_MAX_AP_TYPE;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_reg_get_cur_6g_client_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_client_type
				*reg_cur_6g_client_mobility_type)
{
	*reg_cur_6g_client_mobility_type = REG_SUBORDINATE_CLIENT;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_reg_set_cur_6ghz_client_type(struct wlan_objmgr_pdev *pdev,
				  enum reg_6g_client_type in_6ghz_client_type)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_reg_set_6ghz_client_type_from_target(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS wlan_reg_get_rnr_tpe_usable(struct wlan_objmgr_pdev *pdev,
				       bool *reg_rnr_tpe_usable)
{
	*reg_rnr_tpe_usable = false;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS wlan_reg_get_unspecified_ap_usable(struct wlan_objmgr_pdev *pdev,
					      bool *reg_unspecified_ap_usable)
{
	*reg_unspecified_ap_usable = false;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
bool wlan_reg_is_6g_psd_power(struct wlan_objmgr_pdev *pdev)
{
	return false;
}

static inline
QDF_STATUS wlan_reg_get_6g_chan_ap_power(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t chan_freq, bool *is_psd,
					 uint16_t *tx_power,
					 uint16_t *eirp_psd_power)
{
	*is_psd = false;
	*tx_power = 0;
	*eirp_psd_power = 0;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_reg_get_client_power_for_connecting_ap(struct wlan_objmgr_pdev *pdev,
					    enum reg_6g_ap_type ap_type,
					    qdf_freq_t chan_freq,
					    bool is_psd, uint16_t *tx_power,
					    uint16_t *eirp_psd_power)
{
	*tx_power = 0;
	*eirp_psd_power = 0;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_reg_get_client_power_for_6ghz_ap(struct wlan_objmgr_pdev *pdev,
				      enum reg_6g_client_type client_type,
				      qdf_freq_t chan_freq,
				      bool *is_psd, uint16_t *tx_power,
				      uint16_t *eirp_psd_power)
{
	*is_psd = false;
	*tx_power = 0;
	*eirp_psd_power = 0;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline enum reg_6g_ap_type
wlan_reg_decide_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev)
{
	return REG_INDOOR_AP;
}

static inline QDF_STATUS
wlan_reg_set_ap_pwr_and_update_chan_list(struct wlan_objmgr_pdev *pdev,
					 enum reg_6g_ap_type ap_pwr_type)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline enum supported_6g_pwr_types
wlan_reg_get_best_6g_pwr_type(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	return REG_INVALID_PWR_MODE;
}

static inline enum supported_6g_pwr_types
wlan_reg_conv_6g_ap_type_to_supported_6g_pwr_types(enum reg_6g_ap_type
						   ap_pwr_type)
{
	return REG_INVALID_PWR_MODE;
}

static inline enum reg_6g_ap_type
wlan_reg_conv_supported_6g_pwr_type_to_ap_pwr_type(enum supported_6g_pwr_types
						   in_6g_pwr_type)
{
	return REG_MAX_AP_TYPE;
}
#endif /* CONFIG_BAND_6GHZ */

/**
 * wlan_reg_is_ext_tpc_supported() - Checks if FW supports new WMI cmd for TPC
 *
 * @psoc: psoc ptr
 *
 * Return: true if FW supports new command or false otherwise
 */
bool wlan_reg_is_ext_tpc_supported(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_reg_is_chwidth_supported() - Check if given channel width is supported
 * on a given pdev
 * @pdev: pdev pointer
 * @ch_width: channel width.
 * @is_supported: whether the channel width is supported
 *
 * Return QDF_STATUS_SUCCESS of operation
 */
QDF_STATUS wlan_reg_is_chwidth_supported(struct wlan_objmgr_pdev *pdev,
					 enum phy_ch_width ch_width,
					 bool *is_supported);

#ifdef CONFIG_BAND_6GHZ
/**
 * wlan_reg_get_thresh_priority_freq() - Get the prioritized frequency value
 * @pdev: pdev pointer
 */
qdf_freq_t wlan_reg_get_thresh_priority_freq(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_psd_2_eirp() - Calculate EIRP from PSD and bandwidth
 * channel list
 * @pdev: pdev pointer
 * @psd: Power Spectral Density in dBm/MHz
 * @ch_bw: Bandwidth of a channel in MHz (20/40/80/160/320 etc)
 * @eirp:  EIRP power  in dBm
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_psd_2_eirp(struct wlan_objmgr_pdev *pdev,
			       int16_t psd,
			       uint16_t ch_bw,
			       int16_t *eirp);

/**
 * wlan_reg_eirp_2_psd() - Calculate PSD poewr from EIRP and bandwidth
 * @pdev: pdev pointer
 * @ch_bw: Bandwidth of a channel in MHz (20/40/80/160/320 etc)
 * @eirp:  EIRP power  in dBm
 * @psd: Power Spectral Density in dBm/MHz
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_eirp_2_psd(struct wlan_objmgr_pdev *pdev,
			       uint16_t ch_bw,
			       int16_t eirp,
			       int16_t *psd);

/**
 * wlan_reg_get_best_pwr_mode() - Get the best power mode based on input freq
 * and bandwidth. The mode that provides the best EIRP is the best power mode.
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @cen320: 320 MHz band center frequency. For other BW, this param is
 * ignored while processing
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: input puncture pattern
 *
 * Return: Best power mode
 */
enum reg_6g_ap_type
wlan_reg_get_best_pwr_mode(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			   qdf_freq_t cen320, uint16_t bw,
			   uint16_t in_punc_pattern);

/**
 * wlan_reg_get_eirp_pwr() - Get eirp power based on the AP power mode
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @cen320: 320 MHz Band center frequency
 * @bw: Bandwidth in MHz
 * @ap_pwr_type: AP power type
 * @in_punc_pattern: Input puncture pattern
 * @is_client_list_lookup_needed: Boolean to indicate if client list lookup is
 * needed
 * @client_type: Client power type
 *
 * Return: EIRP power
 */
uint8_t wlan_reg_get_eirp_pwr(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			      qdf_freq_t cen320, uint16_t bw,
			      enum reg_6g_ap_type ap_pwr_type,
			      uint16_t in_punc_pattern,
			      bool is_client_list_lookup_needed,
			      enum reg_6g_client_type client_type);
#else
static inline
qdf_freq_t wlan_reg_get_thresh_priority_freq(struct wlan_objmgr_pdev *pdev)
{
	return 0;
}

static inline enum reg_6g_ap_type
wlan_reg_get_best_pwr_mode(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			   qdf_freq_t cen320,
			   uint16_t bw,
			   uint16_t in_punc_pattern)
{
	return REG_MAX_AP_TYPE;
}

static inline QDF_STATUS wlan_reg_psd_2_eirp(struct wlan_objmgr_pdev *pdev,
					     int16_t psd,
					     uint16_t ch_bw,
					     int16_t *eirp)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS wlan_reg_eirp_2_psd(struct wlan_objmgr_pdev *pdev,
					     uint16_t ch_bw,
					     int16_t eirp,
					     int16_t *psd)
{
	return QDF_STATUS_E_FAILURE;
}

static inline uint8_t
wlan_reg_get_eirp_pwr(struct wlan_objmgr_pdev *pdev,
		      qdf_freq_t freq,
		      qdf_freq_t cen320, uint16_t bw,
		      enum reg_6g_ap_type ap_pwr_type,
		      uint16_t in_punc_pattern,
		      bool is_client_list_lookup_needed,
		      enum reg_6g_client_type client_type)
{
	return 0;
}
#endif /* CONFIG_BAND_6GHZ */
/**
 * wlan_reg_find_chwidth_from_bw () - Gets channel width for given
 * bandwidth
 * @bw: Bandwidth
 *
 * Return: phy_ch_width
 */
enum phy_ch_width wlan_reg_find_chwidth_from_bw(uint16_t bw);

/**
 * wlan_reg_get_chan_state_for_320() - Get the channel state of a 320 MHz
 * bonded channel.
 * @pdev: Pointer to wlan_objmgr_pdev
 * @freq: Primary frequency
 * @center_320: Band center of 320 MHz
 * @ch_width: Channel width
 * @bonded_chan_ptr_ptr: Pointer to bonded channel pointer
 * @treat_nol_chan_as_disabled: Bool to treat nol chan as enabled/disabled
 * @in_pwr_type: Input 6g power type
 * @input_puncture_bitmap: Input puncture bitmap
 *
 * Return: Channel state
 */
#ifdef WLAN_FEATURE_11BE
enum channel_state
wlan_reg_get_chan_state_for_320(struct wlan_objmgr_pdev *pdev,
				uint16_t freq,
				qdf_freq_t center_320,
				enum phy_ch_width ch_width,
				const struct bonded_channel_freq
				**bonded_chan_ptr_ptr,
				enum supported_6g_pwr_types in_6g_pwr_type,
				bool treat_nol_chan_as_disabled,
				uint16_t input_puncture_bitmap);
#else
static inline enum channel_state
wlan_reg_get_chan_state_for_320(struct wlan_objmgr_pdev *pdev,
				uint16_t freq,
				qdf_freq_t center_320,
				enum phy_ch_width ch_width,
				const struct bonded_channel_freq
				**bonded_chan_ptr_ptr,
				enum supported_6g_pwr_types in_6g_pwr_type,
				bool treat_nol_chan_as_disabled,
				uint16_t input_puncture_bitmap)
{
	return CHANNEL_STATE_INVALID;
}
#endif

#ifdef CONFIG_BAND_6GHZ
/**
 * wlan_is_sup_chan_entry_afc_done() - Checks if the super chan entry of given
 * channel idx and power mode has REGULATORY_CHAN_AFC_NOT_DONE flag cleared.
 *
 * @pdev: pdev pointer
 * @freq: input channel idx
 * @in_6g_pwr_mode: input power mode
 *
 * Return: True if REGULATORY_CHAN_AFC_NOT_DONE flag is clear for the super
 * chan entry.
 */
bool
wlan_is_sup_chan_entry_afc_done(struct wlan_objmgr_pdev *pdev,
				enum channel_enum chan_idx,
				enum supported_6g_pwr_types in_6g_pwr_mode);

/**
 * wlan_reg_display_super_chan_list() - Display super channel list for all modes
 * @pdev: Pointer to pdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_display_super_chan_list(struct wlan_objmgr_pdev *pdev);

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
/**
 * wlan_reg_get_afc_freq_range_and_psd_limits() - Get freq range and psd
 * limits from afc server response.
 *
 * @pdev: Pointer to pdev
 * @num_freq_obj: Number of frequency objects
 * @afc_obj: Pointer to struct afc_freq_obj
 *
 * Return: QDF_STATUS
 */

QDF_STATUS
wlan_reg_get_afc_freq_range_and_psd_limits(struct wlan_objmgr_pdev *pdev,
					   uint8_t num_freq_obj,
					   struct afc_freq_obj *afc_obj);

/**
 * wlan_reg_get_num_afc_freq_obj() - Get number of afc frequency objects
 *
 * @pdev: Pointer to pdev
 * @num_freq_obj: Number of frequency objects
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_reg_get_num_afc_freq_obj(struct wlan_objmgr_pdev *pdev,
			      uint8_t *num_freq_obj);

/**
 * wlan_reg_set_afc_power_event_received() - Set power event received flag with
 * given val.
 * @pdev: pdev pointer.
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_reg_set_afc_power_event_received(struct wlan_objmgr_pdev *pdev,
						 bool val);
#endif

#else
static inline bool
wlan_is_sup_chan_entry_afc_done(struct wlan_objmgr_pdev *pdev,
				enum channel_enum chan_idx,
				enum supported_6g_pwr_types in_6g_pwr_mode)
{
	return false;
}

static inline QDF_STATUS
wlan_reg_display_super_chan_list(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

#endif

/**
 * wlan_reg_get_num_rules_of_ap_pwr_type() - Get the number of reg rules
 * present for a given ap power type
 * @pdev: Pointer to pdev
 * @ap_pwr_type: AP power type
 *
 * Return: Return the number of reg rules for a given ap power type
 */
uint8_t
wlan_reg_get_num_rules_of_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				      enum reg_6g_ap_type ap_pwr_type);
#endif
