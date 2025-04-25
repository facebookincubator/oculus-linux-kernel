/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2007-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * DOC: This file has Zero CAC DFS APIs.
 */

#ifndef _DFS_ZERO_CAC_H_
#define _DFS_ZERO_CAC_H_

#include "dfs.h"
#include <wlan_dfs_tgt_api.h>

#ifdef WLAN_FEATURE_11BE
#define TREE_DEPTH_320                    5
#define TREE_DEPTH_MAX                    TREE_DEPTH_320
#else
#define TREE_DEPTH_MAX                    TREE_DEPTH_160
#endif

#define TREE_DEPTH_160                    4
#define TREE_DEPTH_80                     3
#define TREE_DEPTH_40                     2
#define TREE_DEPTH_20                     1
#define N_SUBCHANS_FOR_80BW               4
#define N_SUBCHANS_FOR_160BW              8

#define INITIAL_20_CHAN_OFFSET           -6
#define INITIAL_40_CHAN_OFFSET           -4
#define INITIAL_80_CHAN_OFFSET            0

#define NEXT_20_CHAN_OFFSET               4
#define NEXT_40_CHAN_OFFSET               8
#define NEXT_80_CHAN_OFFSET              16

#define DFS_CHWIDTH_20_VAL               20
#define DFS_CHWIDTH_40_VAL               40
#define DFS_CHWIDTH_80_VAL               80
#define DFS_CHWIDTH_160_VAL             160
#define DFS_CHWIDTH_165_VAL             165
#define DFS_CHWIDTH_240_VAL             240
#define DFS_CHWIDTH_320_VAL             320

#define WEATHER_CHAN_START              120
#define WEATHER_CHAN_END                128

/* PreCAC timeout durations in ms. */
#define MIN_PRECAC_DURATION                   (6 * 60 * 1000) /* 6 mins */
#define MIN_WEATHER_PRECAC_DURATION          (60 * 60 * 1000) /* 1 hour */
#define MAX_PRECAC_DURATION              (4 * 60 * 60 * 1000) /* 4 hours */
#define MAX_WEATHER_PRECAC_DURATION     (24 * 60 * 60 * 1000) /* 24 hours */
#define MIN_RCAC_DURATION                     (62 * 1000) /* 62 seconds */
#define MAX_RCAC_DURATION                     0xffffffff

#define PCAC_DFS_INDEX_ZERO               0
#define PCAC_TIMER_NOT_RUNNING            0
#define PRECAC_NOT_STARTED                0

/* While building precac tree, the center of the 165MHz channel or the
 * restricted 80p80 channel(which includes channels 132, 136, 140, 144,
 * 149, 153, 157 and 161) is assumed to be 146(center channel) or
 * 5730(center frequency).
 */
#define RESTRICTED_80P80_CHAN_CENTER_FREQ     5730
#define RESTRICTED_80P80_LEFT_80_CENTER_CHAN  138
#define RESTRICTED_80P80_RIGHT_80_CENTER_CHAN 155
#define RESTRICTED_80P80_LEFT_80_CENTER_FREQ  5690
#define RESTRICTED_80P80_RIGHT_80_CENTER_FREQ 5775

/* While building the precac tree with 320 MHz root, the center of the
 * right side 160 MHz channel(which includes real IEEE channels 132, 136,
 * 140, 144 and pseudo IEEE channels 148, 152, 156, 160)
 */
#define CENTER_OF_320_MHZ                     5650
#define CENTER_OF_PSEUDO_160                  5730
#define LAST_20_CENTER_OF_FIRST_160           5320
#define FIRST_20_CENTER_OF_LAST_80            5745

/* Depth of the tree of a given bandwidth. */
#define DEPTH_320_ROOT                           0
#define DEPTH_160_ROOT                           1
#define DEPTH_80_ROOT                            2
#define DEPTH_40_ROOT                            3
#define DEPTH_20_ROOT                            4

#ifdef QCA_DFS_BW_EXPAND
/* Column of the phymode_decoupler array */
enum phymode_decoupler_col {
	CH_WIDTH_COL = 1
};
#endif /* QCA_DFS_BW_EXPAND */

/**
 * struct precac_tree_node - Individual tree node structure for every node in
 *                           the precac forest maintained.
 * @left_child:        Pointer to the left child of the node.
 * @right_child:       Pointer to the right child of the node.
 * @ch_ieee:           Center channel ieee value.
 * @ch_freq:           Center channel frequency value (BSTree node key value).
 * @n_caced_subchs:    Number of CACed subchannels of the ch_ieee.
 * @n_nol_subchs:      Number of subchannels of the ch_ieee in NOL.
 * @n_valid_subchs:    Number of subchannels of the ch_ieee available (as per
 *                     the country's channel list).
 * @bandwidth:         Bandwidth of the ch_ieee (in the current node).
 * @depth:             Depth of the precac tree node.
 */
struct precac_tree_node {
	struct precac_tree_node *left_child;
	struct precac_tree_node *right_child;
	uint8_t ch_ieee;
	uint8_t n_caced_subchs;
	uint8_t n_nol_subchs;
	uint8_t n_valid_subchs;
	uint8_t depth;
	uint16_t bandwidth;
	uint16_t ch_freq;
};

/**
 * enum precac_chan_state - Enum for PreCAC state of a channel.
 * @PRECAC_ERR:            Invalid preCAC state.
 * @PRECAC_REQUIRED:       preCAC need to be done on the channel.
 * @PRECAC_NOW:            preCAC is running on the channel.
 * @PRECAC_DONE:           preCAC is done and channel is clear.
 * @PRECAC_NOL:            preCAC is done and radar is detected.
 */
enum precac_chan_state {
	PRECAC_ERR      = -1,
	PRECAC_REQUIRED,
	PRECAC_NOW,
	PRECAC_DONE,
	PRECAC_NOL,
};

/**
 * struct dfs_precac_entry - PreCAC entry.
 * @pe_list:           PreCAC entry.
 * @vht80_ch_ieee:     VHT80 centre channel IEEE value.
 * @vht80_ch_freq:     VHT80 centre channel frequency value.
 * @center_ch_ieee:    Center channel IEEE value of given bandwidth 20/40/80/
 *                     160. For 165MHz channel, the value is 146.
 * @center_ch_freq:    Center frequency of given bandwidth 20/40/80/160. For
 *                     165MHz channel, the value is 5730.
 * @bw:                Bandwidth of the precac entry.
 * @dfs:               Pointer to wlan_dfs structure.
 * @tree_root:         Tree root node with 80MHz channel key.
 * @non_dfs_subch_count: Number of non DFS subchannels in the entry.
 */
struct dfs_precac_entry {
	TAILQ_ENTRY(dfs_precac_entry) pe_list;
	uint8_t             vht80_ch_ieee;
	uint16_t            vht80_ch_freq;
	uint8_t             center_ch_ieee;
	uint16_t            center_ch_freq;
	uint16_t            bw;
	struct wlan_dfs     *dfs;
	struct precac_tree_node *tree_root;
	uint8_t             non_dfs_subch_count;
};

/**
 * dfs_zero_cac_timer_init() - Initialize zero-cac timers
 * @dfs_soc_obj: Pointer to DFS SOC object structure.
 */
#if defined(ATH_SUPPORT_ZERO_CAC_DFS) && !defined(MOBILE_DFS_SUPPORT)
void dfs_zero_cac_timer_init(struct dfs_soc_priv_obj *dfs_soc_obj);
#else
static inline void
dfs_zero_cac_timer_init(struct dfs_soc_priv_obj *dfs_soc_obj)
{
}
#endif
/**
 * dfs_print_precaclists() - Print precac list.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
void dfs_print_precaclists(struct wlan_dfs *dfs);
#else
static inline void dfs_print_precaclists(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_reset_precac_lists() - Resets the precac lists.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
void dfs_reset_precac_lists(struct wlan_dfs *dfs);
#else
static inline void dfs_reset_precac_lists(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_reset_precaclists() - Clears and initializes precac_list.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
void dfs_reset_precaclists(struct wlan_dfs *dfs);
#else
static inline void dfs_reset_precaclists(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_deinit_precac_list() - Clears the precac list.
 * @dfs: Pointer to wlan_dfs dtructure.
 */
void dfs_deinit_precac_list(struct wlan_dfs *dfs);

/**
 * dfs_zero_cac_detach() - Free zero_cac memory.
 * @dfs: Pointer to wlan_dfs dtructure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && defined(ATH_SUPPORT_ZERO_CAC_DFS)
void dfs_zero_cac_detach(struct wlan_dfs *dfs);
#else
static inline void dfs_zero_cac_detach(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_init_precac_list() - Init precac list.
 * @dfs: Pointer to wlan_dfs dtructure.
 */
void dfs_init_precac_list(struct wlan_dfs *dfs);

#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
/**
 * dfs_start_precac_timer_for_freq() - Start precac timer.
 * @dfs: Pointer to wlan_dfs structure.
 * @precac_chan_freq: Frequency to start precac timer.
 */
#ifdef CONFIG_CHAN_FREQ_API
void dfs_start_precac_timer_for_freq(struct wlan_dfs *dfs,
				     uint16_t precac_chan_freq);
#endif
#else
#ifdef CONFIG_CHAN_FREQ_API
static inline
void dfs_start_precac_timer_for_freq(struct wlan_dfs *dfs,
				     uint16_t precac_chan_freq)
{
}
#endif
#endif

/**
 * dfs_cancel_precac_timer() - Cancel the precac timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && defined(ATH_SUPPORT_ZERO_CAC_DFS)
void dfs_cancel_precac_timer(struct wlan_dfs *dfs);
#else
static inline void dfs_cancel_precac_timer(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_zero_cac_attach() - Initialize dfs zerocac variables.
 * @dfs: Pointer to DFS structure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && defined(ATH_SUPPORT_ZERO_CAC_DFS)
void dfs_zero_cac_attach(struct wlan_dfs *dfs);
#else
static inline void dfs_zero_cac_attach(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_zero_cac_reset() - Reset Zero cac DFS variables.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && defined(ATH_SUPPORT_ZERO_CAC_DFS)
void dfs_zero_cac_reset(struct wlan_dfs *dfs);
#else
static inline void dfs_zero_cac_reset(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_zero_cac_timer_detach() - Free Zero cac DFS variables.
 * @dfs_soc_obj: Pointer to dfs_soc_priv_obj structure.
 */
#if defined(ATH_SUPPORT_ZERO_CAC_DFS) && !defined(MOBILE_DFS_SUPPORT)
void dfs_zero_cac_timer_detach(struct dfs_soc_priv_obj *dfs_soc_obj);
#else
static inline void
dfs_zero_cac_timer_detach(struct dfs_soc_priv_obj *dfs_soc_obj)
{
}
#endif

/**
 * dfs_is_precac_done() - Is precac done.
 * @dfs: Pointer to wlan_dfs structure.
 * @chan: Pointer to dfs_channel for which preCAC done is checked.
 *
 * Return:
 * * True:  If precac is done on channel.
 * * False: If precac is not done on channel.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
bool dfs_is_precac_done(struct wlan_dfs *dfs, struct dfs_channel *chan);
#else
static inline bool dfs_is_precac_done(struct wlan_dfs *dfs,
				      struct dfs_channel *chan)
{
	return false;
}
#endif

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
/**
 * dfs_decide_precac_preferred_chan_for_freq() - Choose operating channel among
 *                                      configured DFS channel and
 *                                      intermediate channel based on
 *                                      precac status of configured
 *                                      DFS channel.
 * @dfs: Pointer to wlan_dfs structure.
 * @pref_chan_freq: Configured DFS channel frequency
 * @mode: Configured PHY mode.
 *
 * Return: True if intermediate channel needs to configure. False otherwise.
 */

#ifdef CONFIG_CHAN_FREQ_API
bool
dfs_decide_precac_preferred_chan_for_freq(struct wlan_dfs *dfs,
					  uint16_t *pref_chan_freq,
					  enum wlan_phymode mode);
#endif
#else
#ifdef CONFIG_CHAN_FREQ_API
static inline void
dfs_decide_precac_preferred_chan_for_freq(struct wlan_dfs *dfs,
					  uint8_t *pref_chan,
					  enum wlan_phymode mode)
{
}
#endif
#endif

/**
 * dfs_get_ieeechan_for_precac_for_freq() - Get chan of required bandwidth from
 *                                 precac_list.
 * @dfs:                 Pointer to wlan_dfs structure.
 * @exclude_pri_chan_freq: Primary channel freq to be excluded for preCAC.
 * @exclude_sec_chan_freq: Secondary channel freq to be excluded for preCAC.
 * @bandwidth:           Bandwidth of requested channel.
 */
#ifdef CONFIG_CHAN_FREQ_API
uint16_t dfs_get_ieeechan_for_precac_for_freq(struct wlan_dfs *dfs,
					      uint16_t exclude_pri_chan_freq,
					      uint16_t exclude_sec_chan_freq,
					      uint16_t bandwidth);
#endif

/**
 * dfs_override_precac_timeout() - Override the default precac timeout.
 * @dfs: Pointer to wlan_dfs structure.
 * @precac_timeout: Precac timeout value.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
int dfs_override_precac_timeout(struct wlan_dfs *dfs,
		int precac_timeout);
#else
static inline int dfs_override_precac_timeout(struct wlan_dfs *dfs,
		int precac_timeout)
{
	return 0;
}
#endif

/**
 * dfs_get_override_precac_timeout() - Get precac timeout.
 * @dfs: Pointer wlan_dfs structure.
 * @precac_timeout: Get precac timeout value in this variable.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
int dfs_get_override_precac_timeout(struct wlan_dfs *dfs,
		int *precac_timeout);
#else
static inline int dfs_get_override_precac_timeout(struct wlan_dfs *dfs,
		int *precac_timeout)
{
	return 0;
}
#endif

#if defined(QCA_SUPPORT_AGILE_DFS)
/**
 * dfs_find_pdev_for_agile_precac() - Find pdev to select channel for precac.
 * @pdev: Pointer to wlan_objmgr_pdev structure.
 * @cur_agile_dfs_index: current agile dfs index
 */
void dfs_find_pdev_for_agile_precac(struct wlan_objmgr_pdev *pdev,
				    uint8_t *cur_agile_dfs_index);

/**
 * dfs_prepare_agile_precac_chan() - Send Agile set request for given pdev.
 * @dfs: Pointer to wlan_dfs structure.
 * @is_chan_found: True if a channel is available for PreCAC, false otherwise.
 */
void dfs_prepare_agile_precac_chan(struct wlan_dfs *dfs, bool *is_chan_found);

/**
 * dfs_process_ocac_complete() - Process Off-Channel CAC complete indication.
 * @pdev :Pointer to wlan_objmgr_pdev structure.
 * @ocac_status: Off channel CAC complete status
 * @center_freq1 : For 20/40/80/160Mhz, it is the center of the corresponding
 * band. For 80P80/165MHz, it is the center of the left 80MHz.
 * @center_freq2 : It is valid and non-zero only for 80P80/165MHz. It indicates
 * the Center Frequency of the right 80MHz segment.
 * @chwidth : Width of the channel for which OCAC completion is received.
 */
void dfs_process_ocac_complete(struct wlan_objmgr_pdev *pdev,
			       enum ocac_status_type ocac_status,
			       uint32_t center_freq1,
			       uint32_t center_freq2,
			       enum phy_ch_width chwidth);

/*
 * dfs_is_ocac_complete_event_for_cur_agile_chan() - Check if the OCAC
 * completion event from FW is received for the currently configured agile
 * channel in host.
 *
 * @dfs: Pointer to dfs structure.
 * @center_freq_mhz1: Center frequency of the band when the precac width is
 * 20/40/80/160MHz and center frequency of the left 80MHz in case of restricted
 * 80P80/165MHz.
 * @center_freq_mhz2: Center frequency of the right 80MHz in case of restricted
 * 80P80/165MHz. It is zero for other channel widths.
 * @chwidth: Agile channel width for which the completion event is received.
 *
 * return: True if the channel on which OCAC completion event received is same
 * as currently configured agile channel in host. False otherwise.
 */
bool dfs_is_ocac_complete_event_for_cur_agile_chan(struct wlan_dfs *dfs);
/**
 * dfs_set_agilecac_chan_for_freq() - Find chan freq for agile CAC.
 * @dfs:         Pointer to wlan_dfs structure.
 * @chan_freq:     Pointer to channel freq for agile set request.
 * @pri_chan_freq: Current primary IEEE channel freq.
 * @sec_chan_freq: Current secondary IEEE channel freq (in HT80_80 mode).
 *
 * Find an IEEE channel freq for agileCAC which is not the current operating
 * channels (indicated by pri_chan_freq, sec_chan_freq).
 */
#ifdef CONFIG_CHAN_FREQ_API
void dfs_set_agilecac_chan_for_freq(struct wlan_dfs *dfs,
				    uint16_t *chan_freq,
				    uint16_t pri_chan_freq,
				    uint16_t sec_chan_freq);
#endif

/**
 * dfs_compute_agile_and_curchan_width() - Compute the agile/current channel
 * width from dfs structure.
 * @dfs: Pointer to wlan_dfs structure.
 * @agile_ch_width: Agile channel width.
 * @cur_ch_width: Current home channel width.
 */
void
dfs_compute_agile_and_curchan_width(struct wlan_dfs *dfs,
				    enum phy_ch_width *agile_ch_width,
				    enum phy_ch_width *cur_ch_width);

/**
 * dfs_agile_precac_start() - Start agile precac.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_agile_precac_start(struct wlan_dfs *dfs);

/**
 * dfs_start_agile_precac_timer() - Start precac timer for the given channel.
 * @dfs:         Pointer to wlan_dfs structure.
 * @ocac_status: Status of the off channel CAC.
 * @adfs_param:  Agile DFS CAC parameters.
 *
 * Start the precac timer with proper timeout values based on the channel to
 * be preCACed. The preCAC channel number and chwidth information is present
 * in the adfs_param argument. Once the timer is started, update the timeout
 * fields in adfs_param.
 */
void dfs_start_agile_precac_timer(struct wlan_dfs *dfs,
				  enum ocac_status_type ocac_status,
				  struct dfs_agile_cac_params *adfs_param);

/**
 * dfs_set_fw_adfs_support() - Set FW aDFS support in dfs object.
 * @dfs: Pointer to wlan_dfs structure.
 * @fw_adfs_support_160: aDFS enabled when pdev is on 160/80P80MHz.
 * @fw_adfs_support_non_160: aDFS enabled when pdev is on 20/40/80MHz.
 * @fw_adfs_support_320: aDFS enabled when pdev is on 320 MHz.
 *
 * Return: void.
 */
void dfs_set_fw_adfs_support(struct wlan_dfs *dfs,
			     bool fw_adfs_support_160,
			     bool fw_adfs_support_non_160,
			     bool fw_adfs_support_320);
#else
static inline void dfs_find_pdev_for_agile_precac(struct wlan_objmgr_pdev *pdev,
						  uint8_t *cur_agile_dfs_index)
{
}

static inline void dfs_prepare_agile_precac_chan(struct wlan_dfs *dfs,
						 bool *is_chan_found)
{
}

static inline void
dfs_process_ocac_complete(struct wlan_objmgr_pdev *pdev,
			  enum ocac_status_type ocac_status,
			  uint32_t center_freq1,
			  uint32_t center_freq2,
			  enum phy_ch_width chwidth)
{
}

static inline bool
dfs_is_ocac_complete_event_for_cur_agile_chan(struct wlan_dfs *dfs)
{
	return false;
}

#ifdef CONFIG_CHAN_FREQ_API
static inline void
dfs_set_agilecac_chan_for_freq(struct wlan_dfs *dfs,
			       uint16_t *chan_freq,
			       uint16_t pri_chan_freq,
			       uint16_t sec_chan_freq)
{
}
#endif

static inline void
dfs_compute_agile_and_curchan_width(struct wlan_dfs *dfs,
				    enum phy_ch_width *agile_ch_width,
				    enum phy_ch_width *cur_ch_width)
{
}

static inline void dfs_agile_precac_start(struct wlan_dfs *dfs)
{
}

static inline void
dfs_start_agile_precac_timer(struct wlan_dfs *dfs,
			     enum ocac_status_type ocac_status,
			     struct dfs_agile_cac_params *adfs_param)
{
}

static inline void
dfs_set_fw_adfs_support(struct wlan_dfs *dfs,
			bool fw_adfs_support_160,
			bool fw_adfs_support_non_160,
			bool fw_adfs_support_320)
{
}
#endif

#if defined(QCA_SUPPORT_AGILE_DFS) || defined(ATH_SUPPORT_ZERO_CAC_DFS)
/**
 * dfs_agile_soc_obj_init() - Initialize soc obj for agile precac.
 * @dfs: Pointer to wlan_dfs structure.
 * @psoc: Pointer to psoc object
 */
void dfs_agile_soc_obj_init(struct wlan_dfs *dfs,
			    struct wlan_objmgr_psoc *psoc);
#else
static inline void dfs_agile_soc_obj_init(struct wlan_dfs *dfs,
					  struct wlan_objmgr_psoc *psoc)
{
}
#endif

/**
 * dfs_set_precac_enable() - Set precac enable flag.
 * @dfs: Pointer to wlan_dfs structure.
 * @value: input value for dfs_legacy_precac_ucfg flag.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
void dfs_set_precac_enable(struct wlan_dfs *dfs,
		uint32_t value);
#else
static inline void dfs_set_precac_enable(struct wlan_dfs *dfs,
		uint32_t value)
{
}
#endif

/**
 * dfs_is_agile_precac_enabled() - Check if agile preCAC is enabled for the DFS.
 * @dfs: Pointer to the wlan_dfs object.
 *
 * Return: True if agile DFS is enabled, else false.
 *
 * For agile preCAC to be enabled,
 * 1. User configuration should be set.
 * 2. Target should support aDFS.
 */
#ifdef QCA_SUPPORT_AGILE_DFS
bool dfs_is_agile_precac_enabled(struct wlan_dfs *dfs);
#else
static inline bool dfs_is_agile_precac_enabled(struct wlan_dfs *dfs)
{
	return false;
}
#endif

/**
 * dfs_is_precac_domain() - Check if current DFS domain supports preCAC.
 * @dfs: Pointer to the wlan_dfs object.
 *
 * Return: True if current DFS domain supports preCAC, else false.
 *
 * preCAC is currently supported in,
 * 1. ETSI domain.
 *
 */
#if defined(QCA_SUPPORT_AGILE_DFS) || defined(ATH_SUPPORT_ZERO_CAC_DFS)
bool dfs_is_precac_domain(struct wlan_dfs *dfs);
#else
static inline bool dfs_is_precac_domain(struct wlan_dfs *dfs)
{
	return false;
}
#endif

/**
 * dfs_is_rcac_domain() - Check if current DFS domain supports agile RCAC.
 * @dfs: Pointer to the wlan_dfs object.
 *
 * Return: True if current DFS domain supports RCAC, else false.
 *
 * preCAC is currently supported in,
 * 1. FCC domain.
 * 2. MKK domain.
 * 3. MKKN domain.
 *
 */
#if defined(QCA_SUPPORT_ADFS_RCAC)
bool dfs_is_rcac_domain(struct wlan_dfs *dfs);
#else
static inline bool dfs_is_rcac_domain(struct wlan_dfs *dfs)
{
	return false;
}
#endif

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
/**
 * dfs_set_precac_intermediate_chan() - Set intermediate chan to be used while
 *                                      doing precac.
 * @dfs: Pointer to wlan_dfs structure.
 * @value: input value for dfs_legacy_precac_ucfg flag.
 *
 * Return:
 * * 0       - Successfully set intermediate channel.
 * * -EINVAL - Invalid channel.
 */
int32_t dfs_set_precac_intermediate_chan(struct wlan_dfs *dfs,
					 uint32_t value);
#else
static inline int32_t dfs_set_precac_intermediate_chan(struct wlan_dfs *dfs,
						       uint32_t value)
{
	return 0;
}
#endif

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
/**
 * dfs_get_precac_intermediate_chan() - Get configured precac
 *					intermediate channel.
 * @dfs: Pointer to wlan_dfs structure.
 *
 * Return: Configured intermediate channel number.
 */
uint32_t dfs_get_precac_intermediate_chan(struct wlan_dfs *dfs);
#else
static inline uint32_t dfs_get_intermediate_chan(struct wlan_dfs *dfs)
{
	return 0;
}
#endif

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT

/**
 * dfs_get_precac_chan_state_for_freq() - Get precac status of a given channel.
 * @dfs: Pointer to wlan_dfs structure.
 * @precac_chan_freq: Channel freq for which precac state need to be checked.
 */

#ifdef CONFIG_CHAN_FREQ_API
enum precac_chan_state
dfs_get_precac_chan_state_for_freq(struct wlan_dfs *dfs,
				   uint16_t precac_chan_freq);
#endif

#else
#ifdef CONFIG_CHAN_FREQ_API
static inline enum precac_chan_state
dfs_get_precac_chan_state_for_freq(struct wlan_dfs *dfs,
				   uint16_t precac_chan_freq)
{
	return PRECAC_REQUIRED;
}
#endif
#endif

/**
 * dfs_reinit_precac_lists() - Reinit DFS preCAC lists.
 * @src_dfs: Source DFS from which the preCAC list is copied.
 * @dest_dfs: Destination DFS to which the preCAC list is copied.
 * @low_5g_freq: Low 5G frequency value of the destination DFS.
 * @high_5g_freq: High 5G frequency value of the destination DFS.
 *
 * Copy all the preCAC list entries from the source DFS to the destination DFS
 * which fall within the frequency range of low_5g_freq and high_5g_freq.
 *
 * Return: None (void).
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
void dfs_reinit_precac_lists(struct wlan_dfs *src_dfs,
			     struct wlan_dfs *dest_dfs,
			     uint16_t low_5g_freq,
			     uint16_t high_5g_freq);
#else
static inline void dfs_reinit_precac_lists(struct wlan_dfs *src_dfs,
					   struct wlan_dfs *dest_dfs,
					   uint16_t low_5g_freq,
					   uint16_t high_5g_freq)
{
}
#endif

/**
 * dfs_is_precac_done_on_non_80p80_chan_for_freq() - Is precac done on
 * a 20/40/80/160/165/320 MHz channel.
 * @dfs: Pointer to wlan_dfs structure.
 * @chan_freq: Channel frequency
 *
 * Return:
 * * True:  If CAC is done on channel.
 * * False: If CAC is not done on channel.
 */
#ifdef CONFIG_CHAN_FREQ_API
bool
dfs_is_precac_done_on_non_80p80_chan_for_freq(struct wlan_dfs *dfs,
					      uint16_t chan_freq);
#endif

/**
 * dfs_is_precac_done_on_80p80_chan() - Is precac done on 80+80 MHz channel.
 * @dfs: Pointer to wlan_dfs structure.
 * @chan: Pointer to dfs_channel for which preCAC done is checked.
 *
 * Return:
 * * True:  If CAC is done on channel.
 * * False: If CAC is not done on channel.
 */
bool dfs_is_precac_done_on_80p80_chan(struct wlan_dfs *dfs,
				      struct dfs_channel *chan);

#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
#ifdef CONFIG_CHAN_FREQ_API
/**
 * dfs_find_curchwidth_and_center_chan_for_freq() - Find the channel width
 *                                                  enum, primary and secondary
 *                                                  center channel value of
 *                                                  the current channel.
 * @dfs:                  Pointer to wlan_dfs structure.
 * @chwidth:              Channel width enum of current channel.
 * @primary_chan_freq:    Primary IEEE channel freq.
 * @secondary_chan_freq:  Secondary IEEE channel freq (in HT80_80 mode).
 */
void
dfs_find_curchwidth_and_center_chan_for_freq(struct wlan_dfs *dfs,
					     enum phy_ch_width *chwidth,
					     uint16_t *primary_chan_freq,
					     uint16_t *secondary_chan_freq);
#endif

#ifdef CONFIG_CHAN_FREQ_API
/**
 * dfs_mark_precac_done_for_freq() - Mark the channel as preCAC done.
 * @dfs:             Pointer to wlan_dfs structure.
 * @pri_chan_freq:   Primary channel IEEE freq.
 * @sec_chan_freq:   Secondary channel IEEE freq(only in HT80_80 mode).
 * @chan_width:      Channel width enum.
 */
void dfs_mark_precac_done_for_freq(struct wlan_dfs *dfs,
				   uint16_t pri_chan_freq,
				   uint16_t sec_chan_freq,
				   enum phy_ch_width chan_width);
#endif

/**
 * dfs_mark_precac_nol_for_freq() - Mark the precac channel as radar.
 * @dfs:                              Pointer to wlan_dfs structure.
 * @is_radar_found_on_secondary_seg:  Radar found on secondary seg for Cascade.
 * @detector_id:                      detector id which found RADAR in HW.
 * @freq_list:                         Array of radar found frequencies.
 * @num_channels:                     Number of radar found subchannels.
 */
#ifdef CONFIG_CHAN_FREQ_API
void dfs_mark_precac_nol_for_freq(struct wlan_dfs *dfs,
				  uint8_t is_radar_found_on_secondary_seg,
				  uint8_t detector_id,
				  uint16_t *freq_list,
				  uint8_t num_channels);
#endif

/**
 * dfs_unmark_precac_nol_for_freq() - Unmark the precac channel as radar.
 * @dfs:      Pointer to wlan_dfs structure.
 * @chan_freq:  channel freq marked as radar.
 */
#ifdef CONFIG_CHAN_FREQ_API
void dfs_unmark_precac_nol_for_freq(struct wlan_dfs *dfs, uint16_t chan_freq);
#endif

#else

#ifdef CONFIG_CHAN_FREQ_API
static inline void
dfs_find_curchwidth_and_center_chan_for_freq(struct wlan_dfs *dfs,
					     enum phy_ch_width *chwidth,
					     uint16_t *primary_chan_freq,
					     uint16_t *secondary_chan_freq)
{
}
#endif

#ifdef CONFIG_CHAN_FREQ_API
static inline void dfs_mark_precac_done_for_freq(struct wlan_dfs *dfs,
						 uint16_t pri_chan_freq,
						 uint16_t sec_chan_freq,
						 enum phy_ch_width chan_width)
{
}
#endif

#ifdef CONFIG_CHAN_FREQ_API
static inline void
dfs_mark_precac_nol_for_freq(struct wlan_dfs *dfs,
			     uint8_t is_radar_found_on_secondary_seg,
			     uint8_t detector_id,
			     uint16_t *freq,
			     uint8_t num_channels)
{
}
#endif

#ifdef CONFIG_CHAN_FREQ_API
static inline void dfs_unmark_precac_nol_for_freq(struct wlan_dfs *dfs,
						  uint16_t chan_freq)
{
}
#endif
#endif

/**
 * dfs_is_precac_timer_running() - Check whether precac timer is running.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if !defined(MOBILE_DFS_SUPPORT) && (defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
				     defined(QCA_SUPPORT_AGILE_DFS))
bool dfs_is_precac_timer_running(struct wlan_dfs *dfs);
#else
static inline bool dfs_is_precac_timer_running(struct wlan_dfs *dfs)
{
	return false;
}
#endif

#ifdef CONFIG_CHAN_FREQ_API
#define VHT160_FREQ_DIFF 80

#define INITIAL_20_CHAN_FREQ_OFFSET           -70
#define INITIAL_40_CHAN_FREQ_OFFSET           -60
#define INITIAL_80_CHAN_FREQ_OFFSET           -40
#define INITIAL_160_CHAN_FREQ_OFFSET            0

#define NEXT_20_CHAN_FREQ_OFFSET               20
#define NEXT_40_CHAN_FREQ_OFFSET               40
#define NEXT_80_CHAN_FREQ_OFFSET               80
#define NEXT_160_CHAN_FREQ_OFFSET             160
#define NEXT_320_CHAN_FREQ_OFFSET             320

#define WEATHER_CHAN_START_FREQ              5600
#define WEATHER_CHAN_END_FREQ                5640

#endif

/**
 * dfs_set_rcac_enable() - Set rcac enable flag.
 * @dfs: Pointer to wlan_dfs structure.
 * @rcac_en: input value to configure rolling cac feature.
 */
#ifdef QCA_SUPPORT_ADFS_RCAC
QDF_STATUS dfs_set_rcac_enable(struct wlan_dfs *dfs,
			       bool rcac_en);
#else
static inline QDF_STATUS
dfs_set_rcac_enable(struct wlan_dfs *dfs,
		    bool rcac_en)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dfs_get_rcac_enable() - Get rcac enable flag.
 * @dfs: Pointer to wlan_dfs structure.
 * @rcac_en: Variable to hold the current rcac config.
 */
#ifdef QCA_SUPPORT_ADFS_RCAC
QDF_STATUS dfs_get_rcac_enable(struct wlan_dfs *dfs,
			       bool *rcac_en);
#else
static inline QDF_STATUS
dfs_get_rcac_enable(struct wlan_dfs *dfs,
		    bool *rcac_en)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dfs_set_rcac_freq() - Set user configured rolling CAC frequency.
 * @dfs: Pointer to wlan_dfs structure.
 * @rcac_freq: User preferred rolling cac frequency.
 */
#ifdef QCA_SUPPORT_ADFS_RCAC
QDF_STATUS dfs_set_rcac_freq(struct wlan_dfs *dfs,
			     qdf_freq_t rcac_freq);
#else
static inline QDF_STATUS
dfs_set_rcac_freq(struct wlan_dfs *dfs,
		  qdf_freq_t rcac_freq)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dfs_get_rcac_freq() - Get user configured rolling CAC frequency.
 * @dfs: Pointer to wlan_dfs structure.
 * @rcac_freq: Variable to store the user preferred rolling cac frequency.
 */
#ifdef QCA_SUPPORT_ADFS_RCAC
QDF_STATUS dfs_get_rcac_freq(struct wlan_dfs *dfs,
			     qdf_freq_t *rcac_freq);
#else
static inline QDF_STATUS
dfs_get_rcac_freq(struct wlan_dfs *dfs,
		  qdf_freq_t *rcac_freq)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dfs_rcac_timer_init() - Initialize rolling cac timer.
 * @dfs_soc_obj: Pointer to DFS SOC object structure.
 */
#ifdef QCA_SUPPORT_ADFS_RCAC
void dfs_rcac_timer_init(struct dfs_soc_priv_obj *dfs_soc_obj);
#else
static inline void
dfs_rcac_timer_init(struct dfs_soc_priv_obj *dfs_soc_obj)
{
}
#endif

/**
 * dfs_rcac_timer_deinit() - Free rolling cac timer object.
 * @dfs_soc_obj: Pointer to dfs_soc_priv_obj structure.
 */
#ifdef QCA_SUPPORT_ADFS_RCAC
void dfs_rcac_timer_deinit(struct dfs_soc_priv_obj *dfs_soc_obj);
#else
static inline void
dfs_rcac_timer_deinit(struct dfs_soc_priv_obj *dfs_soc_obj)
{
}
#endif

#ifdef QCA_SUPPORT_AGILE_DFS
#define DFS_AGILE_SM_SPIN_LOCK(_soc_obj) \
	qdf_spin_lock_bh(&((_soc_obj)->dfs_agile_sm_lock))
#define DFS_AGILE_SM_SPIN_UNLOCK(_soc_obj) \
	qdf_spin_unlock_bh(&((_soc_obj)->dfs_agile_sm_lock))

/**
 * dfs_agile_sm_deliver_evt() - Deliver the event to AGILE SM.
 * @dfs_soc_obj: Pointer to DFS soc object that holds the SM handle.
 * @event: Event ID.
 * @event_data_len: Length of event data.
 * @event_data: pointer to event data.
 *
 * Return: Success if event is handled, else failure.
 */
QDF_STATUS dfs_agile_sm_deliver_evt(struct dfs_soc_priv_obj *dfs_soc_obj,
				    enum dfs_agile_sm_evt event,
				    uint16_t event_data_len,
				    void *event_data);

/**
 * dfs_agile_sm_create() - Create the AGILE state machine.
 * @dfs_soc_obj: Pointer to dfs_soc object that holds the SM handle.
 *
 * Return: QDF_STATUS_SUCCESS if successful, else failure status.
 */
QDF_STATUS dfs_agile_sm_create(struct dfs_soc_priv_obj *dfs_soc_obj);

/**
 * dfs_agile_sm_destroy() - Destroy the AGILE state machine.
 * @dfs_soc_obj: Pointer to dfs_soc object that holds the SM handle.
 *
 * Return: QDF_STATUS_SUCCESS if successful, else failure status.
 */
QDF_STATUS dfs_agile_sm_destroy(struct dfs_soc_priv_obj *dfs_soc_obj);

/**
 * dfs_is_agile_cac_enabled() - Determine if Agile PreCAC/RCAC is enabled.
 * @dfs: Pointer to struct wlan_dfs.
 *
 * Return: True if either Agile PreCAC/RCAC is enabled, false otherwise.
 */
bool dfs_is_agile_cac_enabled(struct wlan_dfs *dfs);

/* dfs_translate_chwidth_enum2val() - Translate the given channel width enum
 *                                    to it's value.
 * @dfs:     Pointer to WLAN DFS structure.
 * @chwidth: Channel width enum of the pdev's current channel.
 *
 * Return: The Bandwidth value for the given channel width enum.
 */
uint16_t
dfs_translate_chwidth_enum2val(struct wlan_dfs *dfs,
			       enum phy_ch_width chwidth);
#else

static inline
QDF_STATUS dfs_agile_sm_deliver_evt(struct dfs_soc_priv_obj *dfs_soc_obj,
				    enum dfs_agile_sm_evt event,
				    uint16_t event_data_len,
				    void *event_data)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dfs_agile_sm_create(struct dfs_soc_priv_obj *dfs_soc_obj)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dfs_agile_sm_destroy(struct dfs_soc_priv_obj *dfs_soc_obj)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool dfs_is_agile_cac_enabled(struct wlan_dfs *dfs)
{
	return false;
}

static inline uint16_t dfs_translate_chwidth_enum2val(struct wlan_dfs *dfs,
						      enum phy_ch_width chwidth)
{
	return false;
}
#endif /* QCA_SUPPORT_AGILE_DFS */

#ifdef QCA_SUPPORT_ADFS_RCAC
/**
 * dfs_is_agile_rcac_enabled() - Determine if Rolling CAC is enabled or not.
 * @dfs: Pointer to struct wlan_dfs.
 *
 * Following are the conditions needed to assertain that rolling CAC
 * is enabled:
 * 1. DFS domain of the PDEV must be FCC or MKK.
 * 2. User has enabled Rolling CAC configuration.
 * 3. FW capability to support ADFS. Only non-160 capability is checked here.
 *    If we happen to choose the next RCAC channel as 160/80-80,
 *    'dfs_fw_adfs_support_160' is also verified.
 *
 *
 * Return: True if RCAC support is enabled, false otherwise.
 */
bool dfs_is_agile_rcac_enabled(struct wlan_dfs *dfs);

/**
 * dfs_prepare_agile_rcac_channel() - Prepare agile RCAC channel.
 * @dfs: Pointer to struct wlan_dfs.
 * @is_rcac_chan_available: Flag to indicate if a valid RCAC channel is
 *                          found.
 */
void dfs_prepare_agile_rcac_channel(struct wlan_dfs *dfs,
				    bool *is_rcac_chan_available);
/**
 * dfs_start_agile_rcac_timer() - Start Agile RCAC timer.
 * @dfs: Pointer to struct wlan_dfs.
 *
 */
void dfs_start_agile_rcac_timer(struct wlan_dfs *dfs);

/**
 * dfs_stop_agile_rcac_timer() - Stop Agile RCAC timer.
 * @dfs: Pointer to struct wlan_dfs.
 *
 */
void dfs_stop_agile_rcac_timer(struct wlan_dfs *dfs);

/**
 * dfs_agile_cleanup_rcac() - Reset parameters of wlan_dfs relatewd to RCAC
 *
 * @dfs: Pointer to struct wlan_dfs.
 */
void dfs_agile_cleanup_rcac(struct wlan_dfs *dfs);
#else
static inline bool dfs_is_agile_rcac_enabled(struct wlan_dfs *dfs)
{
	return false;
}

static inline void dfs_agile_cleanup_rcac(struct wlan_dfs *dfs)
{
}

static inline void
dfs_prepare_agile_rcac_channel(struct wlan_dfs *dfs,
			       bool *is_rcac_chan_available)
{
}

static inline void dfs_start_agile_rcac_timer(struct wlan_dfs *dfs)
{
}

static inline void dfs_stop_agile_rcac_timer(struct wlan_dfs *dfs)
{
}
#endif /* QCA_SUPPORT_ADFS_RCAC */

#if defined(QCA_SUPPORT_AGILE_DFS) || defined(ATH_SUPPORT_ZERO_CAC_DFS) || \
	defined(QCA_SUPPORT_ADFS_RCAC)
/**
 * dfs_process_radar_ind_on_agile_chan() - Process radar indication event on
 * agile channel.
 * @dfs: Pointer to wlan_dfs structure.
 * @radar_found: Pointer to radar_found_info structure.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dfs_process_radar_ind_on_agile_chan(struct wlan_dfs *dfs,
				    struct radar_found_info *radar_found);
#else
static inline QDF_STATUS
dfs_process_radar_ind_on_agile_chan(struct wlan_dfs *dfs,
				    struct radar_found_info *radar_found)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

#ifdef ATH_SUPPORT_ZERO_CAC_DFS
/**
 * dfs_precac_status_for_channel() - Find the preCAC status of the given
 * channel.
 *
 * @dfs: Pointer to wlan_dfs dfs.
 * @deschan: DFS channel to check preCAC status.
 *
 * Return:
 * DFS_NO_PRECAC_COMPLETED_CHANS - 0 preCAC completed channels.
 * DFS_PRECAC_COMPLETED_CHAN - Given channel is preCAC completed.
 * DFS_PRECAC_REQUIRED_CHAN - Given channel requires preCAC.
 */
enum precac_status_for_chan
dfs_precac_status_for_channel(struct wlan_dfs *dfs,
			      struct dfs_channel *deschan);
#else
static inline enum precac_status_for_chan
dfs_precac_status_for_channel(struct wlan_dfs *dfs,
			      struct dfs_channel *deschan)
{
	return DFS_INVALID_PRECAC_STATUS;
}
#endif

#if (defined(QCA_SUPPORT_AGILE_DFS) || defined(QCA_SUPPORT_ADFS_RCAC)) && \
	defined(WLAN_DFS_TRUE_160MHZ_SUPPORT) && defined(WLAN_DFS_FULL_OFFLOAD)
/**
 * dfs_translate_radar_params_for_agile_chan() - Translate radar params from
 * 160MHz synthesizer model to 80MHz synthesizer model for Agile channel.
 * @dfs: Pointer to wlan_dfs dfs.
 * @r_info: Radar found parameters received from FW that are converted to 80MHz
 * syntesizer model(both input and output).
 *
 * Return: void.
 */

void dfs_translate_radar_params_for_agile_chan(struct wlan_dfs *dfs,
					       struct radar_found_info *r_info);
#else
static inline void
dfs_translate_radar_params_for_agile_chan(struct wlan_dfs *dfs,
					  struct radar_found_info *r_info)
{
}
#endif

/**
 * dfs_is_subset_channel_for_freq() - Find out if prev channel and current
 * channel are subsets of each other.
 * @old_subchans_freq: Pointer to previous sub-channels freq.
 * @old_n_chans: Number of previous sub-channels.
 * @new_subchans_freq: Pointer to new sub-channels freq.
 * @new_n_chans:  Number of new sub-channels
 */
#ifdef CONFIG_CHAN_FREQ_API
bool
dfs_is_subset_channel_for_freq(uint16_t *old_subchans_freq,
			       uint8_t old_n_chans,
			       uint16_t *new_subchans_freq,
			       uint8_t new_n_chans);
#endif

#ifdef QCA_DFS_BW_EXPAND
/**
 * dfs_bwexpand_find_usr_cnf_chan() - Find the User configured channel for
 * BW Expand.
 * @dfs: Pointer to wlan_dfs object.
 *
 * Return: User configured frequency.
 */
qdf_freq_t dfs_bwexpand_find_usr_cnf_chan(struct wlan_dfs *dfs);

/**
 * dfs_bwexpand_try_jumping_to_target_subchan() - Expand the current channel
 * bandwidth or jump to a (subset of) user configured target channel.
 * Example: Current channel is 60 HT20 and user configured target channel is
 * 100 HT160. Agile SM runs on the subchans with 20Mhz BW of 100 HT160, here
 * Agile SM runs on 100HT20 and after completion of agile CAC, it checks
 * the API dfs_bwexpand_try_jumping_to_target_subchan for possibility of
 * BW Expansion and only 20Mhz subchan is available. There is no possible for
 * higher bandwidth channel. Then agile CAC runs on the adjacent subchannel
 * 104 HT20. After agile CAC completion, the API is checked again for possible
 * bandwidth expansion and 102 HT40 is available. The API invokes channel change
 * to higher bandwidth.
 * @dfs: Pointer to wlan_dfs object.
 *
 * Return: TRUE, if Bandwidth expansion is success.
 * FALSE, if Bandwidth expansion is failure.
 */
bool dfs_bwexpand_try_jumping_to_target_subchan(struct wlan_dfs *dfs);

/**
 * dfs_is_rcac_cac_done()- Check RCAC is completed on the subset of the
 * user configured target channel.
 * @dfs: Pointer to wlan_dfs.
 * @chan: Pointer to dfs_channel object of user configured target channel.
 * @subset_chan: Pointer to dfs_channel object of subchannel in which RCAC is
 * completed.
 *
 * Return: Boolean value.
 */
bool dfs_is_rcac_cac_done(struct wlan_dfs *dfs,
			  struct dfs_channel *chan,
			  struct dfs_channel *subset_chan);

/*
 * dfs_get_configured_bwexpand_dfs_chan() - Get a DFS chan when frequency and
 * phymode is provided.
 * @dfs: pointer to wlan_dfs.
 * @user_chan: pointer to dfs_channel.
 * @target_mode: phymode of type wlan_phymode.
 */
bool dfs_get_configured_bwexpand_dfs_chan(struct wlan_dfs *dfs,
					  struct dfs_channel *user_chan,
					  enum wlan_phymode target_mode);
#else
static inline
qdf_freq_t dfs_bwexpand_find_usr_cnf_chan(struct wlan_dfs *dfs)
{
	return 0;
}

static inline
bool dfs_bwexpand_try_jumping_to_target_subchan(struct wlan_dfs *dfs)
{
	return false;
}

static inline
bool dfs_is_rcac_cac_done(struct wlan_dfs *dfs,
			  struct dfs_channel *chan,
			  struct dfs_channel *subset_chan)
{
	return false;
}

static inline
bool dfs_get_configured_bwexpand_dfs_chan(struct wlan_dfs *dfs,
					  struct dfs_channel *user_chan,
					  enum wlan_phymode target_mode)
{
	return false;
}
#endif /* QCA_DFS_BW_EXPAND */

#if defined(QCA_DFS_BW_PUNCTURE) && !defined(CONFIG_REG_CLIENT)
/**
 * dfs_create_punc_sm() - Wrapper API to Create DFS puncture state machine.
 * @dfs: pointer to wlan_dfs.
 *
 * Return: Nothing.
 */
void dfs_create_punc_sm(struct wlan_dfs *dfs);

/**
 * dfs_destroy_punc_sm() - Wrapper API to Destroy DFS puncture state machine.
 * @dfs: pointer to wlan_dfs.
 *
 * Return: Nothing.
 */
void dfs_destroy_punc_sm(struct wlan_dfs *dfs);

/**
 * dfs_punc_sm_stop_all() - API to stop all puncture SM object.
 * @dfs: pointer to wlan_dfs.
 *
 * Return: Nothing.
 */
void dfs_punc_sm_stop_all(struct wlan_dfs *dfs);

/**
 * dfs_punc_sm_stop() - Stop DFS puncture state machine.
 * @dfs:           Pointer to wlan_dfs.
 * @indx:          Index of DFS puncture state machine.
 * @dfs_punc_arr:  Pointer to DFS puncture state machine object.
 *
 * Return: Nothing.
 */
void dfs_punc_sm_stop(struct wlan_dfs *dfs,
		      uint8_t indx,
		      struct dfs_punc_obj *dfs_punc_arr);

/**
 * dfs_punc_sm_create() - Create DFS puncture state machine.
 * @dfs_punc:             Pointer to DFS puncture state machine object.
 *
 * Return: Success if SM is created.
 */
QDF_STATUS dfs_punc_sm_create(struct dfs_punc_obj *dfs_punc);

/**
 * dfs_punc_sm_destroy() - Destroy DFS puncture state machine.
 * @dfs_punc:              Pointer to DFS puncture state machine object.
 *
 * Return: Success if SM is destroyed.
 */
QDF_STATUS dfs_punc_sm_destroy(struct dfs_punc_obj *dfs_punc);

/**
 * dfs_punc_cac_timer_attach() - Attach puncture CAC timer to DFS puncture
 *                               state machine.
 * @dfs:                         Pointer to wlan_dfs.
 * @dfs_punc_arr:                Pointer to DFS puncture state machine object.
 *
 * Return: Nothing.
 */
void dfs_punc_cac_timer_attach(struct wlan_dfs *dfs,
			       struct dfs_punc_obj *dfs_punc_arr);

/**
 * dfs_handle_dfs_puncture_unpuncture() - Handles DFS puncture and unpuncturing.
 * @dfs:                                  Pointer to wlan_dfs.
 *
 * Return: Nothing.
 */
void dfs_handle_dfs_puncture_unpuncture(struct wlan_dfs *dfs);

/**
 * dfs_punc_cac_timer_reset() - Reset puncture CAC timer.
 * @dfs_punc_arr:               Pointer to DFS puncture state machine object.
 *
 * Return: Nothing.
 */
void dfs_punc_cac_timer_reset(struct dfs_punc_obj *dfs_punc_arr);

/**
 * dfs_punc_cac_timer_detach() - Detach puncture CAC timer from DFS puncture
 *                               state machine.
 * @dfs_punc_arr:                Pointer to DFS puncture state machine object.
 *
 * Return: Nothing.
 */
void dfs_punc_cac_timer_detach(struct dfs_punc_obj *dfs_punc_arr);

/**
 * dfs_start_punc_cac_timer() - Start puncture CAC timer.
 * @dfs_punc_arr:               Pointer to DFS puncture state machine object.
 * @is_weather_chan:            check if the channel is weather channel.
 *
 * Return: Nothing.
 */
void dfs_start_punc_cac_timer(struct dfs_punc_obj *dfs_punc_arr,
			      bool is_weather_chan);

/**
 * dfs_cancel_punc_cac_timer() - Cancel puncture CAC timer.
 * @dfs_punc_arr:                Pointer to DFS puncture state machine object.
 *
 * Return: Nothing.
 */
void dfs_cancel_punc_cac_timer(struct dfs_punc_obj *dfs_punc_arr);

/**
 * utils_dfs_puncturing_sm_deliver_evt() - Utility API to post events to DFS
 *                                         puncture state machine.
 * @pdev:                          Pointer to DFS pdev object.
 * @sm_indx:                       Index of state machine.
 * @event:                         Event to be posted to DFS Puncturing SM.
 *
 * Return: Nothing.
 */
void utils_dfs_puncturing_sm_deliver_evt(struct wlan_objmgr_pdev *pdev,
					 uint8_t sm_indx,
					 enum dfs_punc_sm_evt event);
/**
 * dfs_puncturing_sm_deliver_evt() - API to post events to DFS puncture
 *                                   state machine.
 * @dfs:                           Pointer to wlan_dfs.
 * @event:                         Event to be posted to DFS Puncturing SM.
 * @event_data_len:                Size of event data.
 * @event_data:                    Event data.
 *
 * Return: Nothing.
 */
QDF_STATUS dfs_puncturing_sm_deliver_evt(struct wlan_dfs *dfs,
					 enum dfs_punc_sm_evt event,
					 uint16_t event_data_len,
					 void *event_data);

/**
 * dfs_handle_nol_puncture() - Send SM event post NOL expiry.
 * @dfs:           Pointer to wlan_dfs.
 * @nolfreq:       NOL channel frequency.
 *
 * Return: Nothing.
 */
void dfs_handle_nol_puncture(struct wlan_dfs *dfs, qdf_freq_t nolfreq);

/**
 * dfs_is_ignore_radar_for_punctured_chans() - Store the radar bitmap and check
 *                                             if radar is found in already
 *                                             punctured channel and ignore the
 *                                             radar.
 * @dfs:                       Wlan_dfs structure
 * @dfs_curr_radar_bitmap:     Variable to store radar bitmap.
 *
 * Return: If radar is found on punctured channel then return true.
 * Else return false.
 */
bool dfs_is_ignore_radar_for_punctured_chans(struct wlan_dfs *dfs,
					     uint16_t dfs_curr_radar_bitmap);
#else
static inline
void dfs_create_punc_sm(struct wlan_dfs *dfs)
{
}

static inline
void dfs_destroy_punc_sm(struct wlan_dfs *dfs)
{
}

static inline
void dfs_punc_sm_stop_all(struct wlan_dfs *dfs)
{
}

static inline
void dfs_punc_sm_stop(struct wlan_dfs *dfs,
		      uint8_t indx,
		      struct dfs_punc_obj *dfs_punc_arr)
{
}

static inline
QDF_STATUS dfs_punc_sm_create(struct dfs_punc_obj *dfs_punc)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
QDF_STATUS dfs_punc_sm_destroy(struct dfs_punc_obj *dfs_punc)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
void dfs_punc_cac_timer_attach(struct wlan_dfs *dfs,
			       struct dfs_punc_obj *dfs_punc_arr)
{
}

static inline
void dfs_handle_dfs_puncture_unpuncture(struct wlan_dfs *dfs)
{
}

static inline
void dfs_punc_cac_timer_reset(struct dfs_punc_obj *dfs_punc_arr)
{
}

static inline
void dfs_punc_cac_timer_detach(struct dfs_punc_obj *dfs_punc_arr)
{
}

static inline
void dfs_start_punc_cac_timer(struct dfs_punc_obj *dfs_punc_arr,
			      bool is_weather_chan)
{
}

static inline
void dfs_cancel_punc_cac_timer(struct dfs_punc_obj *dfs_punc_arr)
{
}

static inline
void utils_dfs_puncturing_sm_deliver_evt(struct wlan_objmgr_pdev *pdev,
					 uint8_t sm_indx,
					 enum dfs_punc_sm_evt event)
{
}

static inline
QDF_STATUS dfs_puncturing_sm_deliver_evt(struct wlan_dfs *dfs,
					 enum dfs_punc_sm_evt event,
					 uint16_t event_data_len,
					 void *event_data)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
void dfs_handle_nol_puncture(struct wlan_dfs *dfs, qdf_freq_t nolfreq)
{
}

static inline
bool dfs_is_ignore_radar_for_punctured_chans(struct wlan_dfs *dfs,
					     uint16_t dfs_curr_radar_bitmap)
{
	return false;
}
#endif /* DFS_BW_PUNCTURE */
#endif /* _DFS_ZERO_CAC_H_ */
