/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 *
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
 * DOC: reg_services.h
 * This file provides prototypes of the regulatory component
 * service functions
 */

#ifndef __REG_SERVICES_H_
#define __REG_SERVICES_H_

#include <qdf_types.h>
#include <qdf_status.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include "reg_db.h"
#include <reg_services_public_struct.h>

#define REG_MIN_24GHZ_CH_NUM channel_map[MIN_24GHZ_CHANNEL].chan_num
#define REG_MAX_24GHZ_CH_NUM channel_map[MAX_24GHZ_CHANNEL].chan_num
#define REG_MIN_5GHZ_CH_NUM channel_map[MIN_5GHZ_CHANNEL].chan_num
#define REG_MAX_5GHZ_CH_NUM channel_map[MAX_5GHZ_CHANNEL].chan_num

#ifdef WLAN_FEATURE_DSRC
#define REG_DSRC_START_FREQ channel_map[MIN_DSRC_CHANNEL].center_freq
#define REG_DSRC_END_FREQ   channel_map[MAX_DSRC_CHANNEL].center_freq
#endif

#define REG_ETSI13_SRD_START_FREQ 5745
#define REG_ETSI13_SRD_END_FREQ   5865

#define REG_IS_24GHZ_CH(chan_num) \
	((chan_num >= REG_MIN_24GHZ_CH_NUM) &&	\
	 (chan_num <= REG_MAX_24GHZ_CH_NUM))

#define REG_MIN_24GHZ_CH_FREQ channel_map[MIN_24GHZ_CHANNEL].center_freq
#define REG_MAX_24GHZ_CH_FREQ channel_map[MAX_24GHZ_CHANNEL].center_freq

#define REG_IS_24GHZ_CH_FREQ(freq) \
	((freq >= REG_MIN_24GHZ_CH_FREQ) &&   \
	(freq <= REG_MAX_24GHZ_CH_FREQ))

#ifndef CONFIG_LEGACY_CHAN_ENUM
#define REG_MIN_49GHZ_CH_FREQ channel_map[MIN_49GHZ_CHANNEL].center_freq
#define REG_MAX_49GHZ_CH_FREQ channel_map[MAX_49GHZ_CHANNEL].center_freq

#define REG_IS_49GHZ_FREQ(freq) \
	((freq >= REG_MIN_49GHZ_CH_FREQ) &&   \
	(freq <= REG_MAX_49GHZ_CH_FREQ))
#endif

#define REG_IS_5GHZ_CH(chan_num) \
	((chan_num >= REG_MIN_5GHZ_CH_NUM) &&	\
	 (chan_num <= REG_MAX_5GHZ_CH_NUM))

#define REG_IS_5GHZ_FREQ(freq) \
	((freq >= channel_map[MIN_5GHZ_CHANNEL].center_freq) &&	\
	 (freq <= channel_map[MAX_5GHZ_CHANNEL].center_freq))

#define REG_CH_NUM(ch_enum) channel_map[ch_enum].chan_num
#define REG_CH_TO_FREQ(ch_enum) channel_map[ch_enum].center_freq

#define REG_IS_CHANNEL_VALID_5G_SBS(curchan, newchan)	\
	(curchan > newchan ?				\
	 REG_CH_TO_FREQ(reg_get_chan_enum(curchan))	\
	 - REG_CH_TO_FREQ(reg_get_chan_enum(newchan))	\
	 > REG_SBS_SEPARATION_THRESHOLD :		\
	 REG_CH_TO_FREQ(reg_get_chan_enum(newchan))	\
	 - REG_CH_TO_FREQ(reg_get_chan_enum(curchan))	\
	 > REG_SBS_SEPARATION_THRESHOLD)

#define IS_VALID_PSOC_REG_OBJ(psoc_priv_obj) (NULL != psoc_priv_obj)
#define IS_VALID_PDEV_REG_OBJ(pdev_priv_obj) (NULL != pdev_priv_obj)

/* EEPROM setting is a country code */
#define    COUNTRY_ERD_FLAG     0x8000

/**
 * reg_is_world_ctry_code() - Check if the given country code is WORLD regdomain
 * @ctry_code: Country code value.
 *
 * Return: If country code is WORLD regdomain return true else false
 */
bool reg_is_world_ctry_code(uint16_t ctry_code);

extern const struct chan_map *channel_map;

enum channel_enum reg_get_chan_enum(uint32_t chan_num);

QDF_STATUS reg_get_channel_list_with_power(struct wlan_objmgr_pdev *pdev,
					   struct channel_power *ch_list,
					   uint8_t *num_chan);

enum channel_state reg_get_channel_state(struct wlan_objmgr_pdev *pdev,
					 uint32_t ch);

/**
 * reg_chan_has_dfs_attribute() - check channel has dfs attribue or not
 * @ch: channel number.
 *
 * This API get chan initial dfs attribue flag from regdomain
 *
 * Return: true if chan is dfs, otherwise false
 */
bool reg_chan_has_dfs_attribute(struct wlan_objmgr_pdev *pdev, uint32_t ch);

enum channel_state reg_get_5g_bonded_channel_state(struct wlan_objmgr_pdev
						   *pdev,
						   uint8_t ch,
						   enum phy_ch_width bw);

enum channel_state reg_get_2g_bonded_channel_state(struct wlan_objmgr_pdev
						   *pdev,
						   uint8_t oper_ch,
						   uint8_t sec_ch,
						   enum phy_ch_width bw);

void reg_set_channel_params(struct wlan_objmgr_pdev *pdev,
			    uint8_t ch, uint8_t sec_ch_2g,
			    struct ch_params *ch_params);

/**
 * reg_set_band() - Sets the band information for the PDEV
 * @pdev: The physical dev to set the band for
 * @band: The set band parameters to configure for the pysical device
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_set_band(struct wlan_objmgr_pdev *pdev, enum band_info band);

/**
 * reg_get_band() - Get the band information for the PDEV
 * @pdev: The physical dev to get the band for
 * @band: The band parameters of the physical device
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_get_band(struct wlan_objmgr_pdev *pdev, enum band_info *band);

/**
 * reg_restore_cached_channels() - Cache the current state of the channles
 * @pdev: The physical dev to cache the channels for
 */
#ifdef DISABLE_CHANNEL_LIST
QDF_STATUS reg_restore_cached_channels(struct wlan_objmgr_pdev *pdev);
#else
static inline
QDF_STATUS reg_restore_cached_channels(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * reg_cache_channel_state() - Cache the current state of the channles
 * @pdev: The physical dev to cache the channels for
 * @channel_list: List of the channels for which states needs to be cached
 * @num_channels: Number of channels in the list
 *
 */
#ifdef DISABLE_CHANNEL_LIST
QDF_STATUS reg_cache_channel_state(struct wlan_objmgr_pdev *pdev,
				   uint32_t *channel_list,
				   uint32_t num_channels);
#else
static inline
QDF_STATUS reg_cache_channel_state(struct wlan_objmgr_pdev *pdev,
				   uint32_t *channel_list,
				   uint32_t num_channels)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * reg_notify_sap_event() - Notify regulatory domain for sap event
 * @pdev: The physical dev to set the band for
 * @sap_state: true for sap start else false
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_notify_sap_event(struct wlan_objmgr_pdev *pdev,
			bool sap_state);
/**
 * reg_set_fcc_constraint() - Apply fcc constraints on channels 12/13
 * @pdev: The physical dev to set the band for
 *
 * This function reduces the transmit power on channels 12 and 13, to comply
 * with FCC regulations in the USA.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_set_fcc_constraint(struct wlan_objmgr_pdev *pdev,
		bool fcc_constraint);

/**
 * reg_get_fcc_constraint() - Check FCC constraint on given frequency
 * @pdev: physical dev to get
 * @freq: frequency to be checked
 *
 * Return: If FCC constraint is applied on given frequency return true
 *	   else return false.
 */
bool reg_get_fcc_constraint(struct wlan_objmgr_pdev *pdev, uint32_t freq);

/**
 * reg_read_default_country() - Get the default regulatory country
 * @psoc: The physical SoC to get default country from
 * @country_code: the buffer to populate the country code into
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_read_default_country(struct wlan_objmgr_psoc *psoc,
				   uint8_t *country_code);

/**
 * reg_read_current_country() - Get the current regulatory country
 * @psoc: The physical SoC to get current country from
 * @country_code: the buffer to populate the country code into
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_read_current_country(struct wlan_objmgr_psoc *psoc,
				   uint8_t *country_code);

/**
 * reg_set_default_country() - Set the default regulatory country
 * @psoc: The physical SoC to set default country for
 * @req: The country information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_set_default_country(struct wlan_objmgr_psoc *psoc,
		uint8_t *country);

/**
 * reg_is_world_alpha2 - is reg world mode
 * @alpha2: country code pointer
 *
 * Return: true or false
 */
bool reg_is_world_alpha2(uint8_t *alpha2);

/**
 * reg_is_us_alpha2 - is US country code
 * @alpha2: country code pointer
 *
 * Return: true or false
 */
bool reg_is_us_alpha2(uint8_t *alpha2);

/**
 * reg_set_country() - Set the current regulatory country
 * @pdev: pdev device for country information
 * @country: country value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_set_country(struct wlan_objmgr_pdev *pdev, uint8_t *country);

/**
 * reg_set_11d_country() - Set the 11d regulatory country
 * @pdev: pdev device for country information
 * @country: country value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_set_11d_country(struct wlan_objmgr_pdev *pdev, uint8_t *country);

/**
 * reg_reset_country() - Reset the regulatory country to default
 * @psoc: The physical SoC to reset country for
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_reset_country(struct wlan_objmgr_psoc *psoc);

/**
 * reg_enable_dfs_channels() - Enable the use of DFS channels
 * @pdev: The physical dev to enable/disable DFS channels for
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_enable_dfs_channels(struct wlan_objmgr_pdev *pdev, bool enable);


void reg_get_current_dfs_region(struct wlan_objmgr_pdev *pdev,
			enum dfs_reg *dfs_reg);

uint32_t reg_get_channel_reg_power(struct wlan_objmgr_pdev *pdev,
				   uint32_t chan_num);

uint32_t reg_get_channel_freq(struct wlan_objmgr_pdev *pdev,
			      uint32_t chan_num);


uint16_t reg_get_bw_value(enum phy_ch_width bw);

void reg_set_dfs_region(struct wlan_objmgr_pdev *pdev,
			enum dfs_reg dfs_reg);

QDF_STATUS reg_get_domain_from_country_code(v_REGDOMAIN_t *reg_domain_ptr,
					    const uint8_t *country_alpha2,
					    enum country_src source);

enum band_info reg_chan_to_band(uint32_t chan_num);

uint16_t reg_dmn_get_chanwidth_from_opclass(uint8_t *country,
					    uint8_t channel,
					    uint8_t opclass);

uint16_t reg_dmn_get_opclass_from_channel(uint8_t *country,
					  uint8_t channel,
					  uint8_t offset);

/**
 * reg_dmn_get_opclass_from_channe() - Print channels in op class.
 * @country: Country code.
 * @opclass: opclass.
 *
 * Return: Void.
 */
void reg_dmn_print_channels_in_opclass(uint8_t *country,
					uint8_t op_class);

uint16_t reg_dmn_set_curr_opclasses(uint8_t num_classes, uint8_t *class);

uint16_t reg_dmn_get_curr_opclasses(uint8_t *num_classes, uint8_t *class);


QDF_STATUS reg_process_master_chan_list(struct cur_regulatory_info *reg_info);

QDF_STATUS wlan_regulatory_psoc_obj_created_notification(
					    struct wlan_objmgr_psoc *psoc,
					    void *arg_list);

QDF_STATUS  wlan_regulatory_psoc_obj_destroyed_notification(
					    struct wlan_objmgr_psoc *psoc,
					    void *arg_list);

QDF_STATUS wlan_regulatory_pdev_obj_created_notification(
					    struct wlan_objmgr_pdev *pdev,
					    void *arg_list);

QDF_STATUS  wlan_regulatory_pdev_obj_destroyed_notification(
					    struct wlan_objmgr_pdev *pdev,
					    void *arg_list);

static inline struct wlan_lmac_if_reg_tx_ops *
reg_get_psoc_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	return &((psoc->soc_cb.tx_ops.reg_ops));
}

QDF_STATUS reg_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
				     struct regulatory_channel
				     *chan_list);

QDF_STATUS reg_program_chan_list(struct wlan_objmgr_pdev *pdev,
				 struct cc_regdmn_s *rd);

void reg_update_nol_ch(struct wlan_objmgr_pdev *pdev, uint8_t *ch_list,
		       uint8_t num_ch, bool nol_ch);

/**
 * reg_is_dfs_ch () - Checks the channel state for DFS
 * @chan: channel
 * @pdev: pdev ptr
 *
 * Return: true or false
 */
bool reg_is_dfs_ch(struct wlan_objmgr_pdev *pdev, uint32_t chan);

#ifdef WLAN_FEATURE_DSRC
/**
 * reg_is_dsrc_chan () - Checks the channel for DSRC or not
 * @chan: channel
 * @pdev: pdev ptr
 *
 * Return: true or false
 */
bool reg_is_dsrc_chan(struct wlan_objmgr_pdev *pdev, uint32_t chan);

static inline bool reg_is_etsi13_srd_chan(struct wlan_objmgr_pdev *pdev,
					  uint32_t chan)
{
	return false;
}

static inline bool reg_is_etsi13_regdmn(struct wlan_objmgr_pdev *pdev)
{
	return false;
}

static inline bool
reg_is_etsi13_srd_chan_allowed_master_mode(struct wlan_objmgr_pdev *pdev,
					   enum QDF_OPMODE vdev_opmode)
{
	return true;
}
#else
/**
 * reg_is_etsi13_regdmn () - Checks if the current reg domain is ETSI13 or not
 * @pdev: pdev ptr
 *
 * Return: true or false
 */
bool reg_is_etsi13_regdmn(struct wlan_objmgr_pdev *pdev);

/**
 * reg_is_etsi13_srd_chan () - Checks the channel for ETSI13 srd ch or not
 * @chan: channel
 * @pdev: pdev ptr
 *
 * Return: true or false
 */
bool reg_is_etsi13_srd_chan(struct wlan_objmgr_pdev *pdev, uint32_t chan);

/**
 * reg_is_etsi13_srd_chan_allowed_master_mode() - Checks if regdmn is ETSI13
 * and SRD channels are allowed in master mode or not for particular vdev
 *
 * @pdev: pdev ptr
 * @vdev_opmode: vdev opmode
 *
 * Return: true or false
 */
bool reg_is_etsi13_srd_chan_allowed_master_mode(struct wlan_objmgr_pdev *pdev,
						enum QDF_OPMODE vdev_opmode);

static inline bool reg_is_dsrc_chan(struct wlan_objmgr_pdev *pdev,
				    uint32_t chan)
{
	return false;
}
#endif

bool reg_is_passive_or_disable_ch(struct wlan_objmgr_pdev *pdev,
				  uint32_t chan);

bool reg_is_disable_ch(struct wlan_objmgr_pdev *pdev, uint32_t chan);

uint32_t reg_freq_to_chan(struct wlan_objmgr_pdev *pdev, uint32_t freq);

uint32_t reg_chan_to_freq(struct wlan_objmgr_pdev *pdev, uint32_t chan_num);

/**
 * reg_legacy_chan_to_freq() - Get freq from chan noumber, for 2G and 5G
 * @pdev: Pointer to pdev
 * @chan_num: Channel number
 *
 * Return: Channel frequency if success, otherwise 0
 */
uint16_t reg_legacy_chan_to_freq(struct wlan_objmgr_pdev *pdev,
				 uint8_t chan_num);

/**
 * reg_chan_is_49ghz() - Check if the input channel number is 4.9GHz
 * @pdev: Pdev pointer
 * @chan_num: Input channel number
 *
 * Return: true if the channel is 4.9GHz else false.
 */
bool reg_chan_is_49ghz(struct wlan_objmgr_pdev *pdev,
		uint8_t chan_num);

/**
 * reg_set_config_vars () - set configration variables
 * @psoc: psoc ptr
 * @config_vars: configuration struct
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_set_config_vars(struct wlan_objmgr_psoc *psoc,
			       struct reg_config_vars config_vars);

bool reg_is_regdb_offloaded(struct wlan_objmgr_psoc *psoc);

void reg_program_mas_chan_list(struct wlan_objmgr_psoc *psoc,
			       struct regulatory_channel *reg_channels,
			       uint8_t *alpha2,
			       enum dfs_reg dfs_region);

/**
 * reg_get_regd_rules() - provides the reg domain rules info
 * @pdev: pdev pointer
 * @reg_rules: regulatory rules
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_get_regd_rules(struct wlan_objmgr_pdev *pdev,
			      struct reg_rule_info *reg_rules);

/**
 * reg_reset_reg_rules() - provides the reg domain rules info
 * @reg_rules: reg rules pointer
 *
 * Return: None
 */
void reg_reset_reg_rules(struct reg_rule_info *reg_rules);

QDF_STATUS reg_program_default_cc(struct wlan_objmgr_pdev *pdev,
				  uint16_t regdmn);

QDF_STATUS reg_get_current_cc(struct wlan_objmgr_pdev *pdev,
			      struct cc_regdmn_s *rd);

QDF_STATUS reg_get_curr_band(struct wlan_objmgr_pdev *pdev,
		enum band_info *band);

typedef void (*reg_chan_change_callback)(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev,
		struct regulatory_channel *chan_list,
		struct avoid_freq_ind_data *avoid_freq_ind,
		void *arg);

void reg_register_chan_change_callback(struct wlan_objmgr_psoc *psoc,
				       reg_chan_change_callback cbk,
				       void *arg);

void reg_unregister_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					 reg_chan_change_callback cbk);


struct chan_change_cbk_entry {
	reg_chan_change_callback cbk;
	void *arg;
};

bool reg_is_11d_scan_inprogress(struct wlan_objmgr_psoc *psoc);

enum country_src reg_get_cc_and_src(struct wlan_objmgr_psoc *psoc,
				    uint8_t *alpha2);

/**
 * reg_save_new_11d_country() - Save the 11d new country
 * @psoc: psoc for country information
 * @country: country value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_save_new_11d_country(struct wlan_objmgr_psoc *psoc,
		uint8_t *country);

/**
 * reg_11d_original_enabled_on_host() - whether 11d original enabled on host
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool reg_11d_original_enabled_on_host(struct wlan_objmgr_psoc *psoc);

/**
 * reg_11d_enabled_on_host() - know whether 11d enabled on host
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool reg_11d_enabled_on_host(struct wlan_objmgr_psoc *psoc);

/**
 * reg_11d_vdev_delete_update() - update 11d state upon vdev delete
 * @vdev: vdev pointer
 *
 * Return: Success or Failure
 */
QDF_STATUS reg_11d_vdev_delete_update(struct wlan_objmgr_vdev *vdev);

/**
 * reg_11d_vdev_created_update() - update 11d state upon vdev create
 * @vdev: vdev pointer
 *
 * Return: Success or Failure
 */
QDF_STATUS reg_11d_vdev_created_update(struct wlan_objmgr_vdev *vdev);

/**
 * reg_get_psoc_obj() - Provides the reg component object pointer
 * @psoc: pointer to psoc object.
 *
 * Return: reg component object pointer
 */
struct wlan_regulatory_psoc_priv_obj *reg_get_psoc_obj(
		struct wlan_objmgr_psoc *psoc);

/**
 * reg_reset_ctry_pending_hints() - Reset all country pending hints
 * @soc_reg: regulatory private object
 *
 * Return: None
 */
void
reg_reset_ctry_pending_hints(struct wlan_regulatory_psoc_priv_obj *soc_reg);

/**
 * reg_set_regdb_offloaded() - set/clear regulatory offloaded flag
 *
 * @psoc: psoc pointer
 * Return: Success or Failure
 */
QDF_STATUS reg_set_regdb_offloaded(struct wlan_objmgr_psoc *psoc,
		bool val);

/**
 * reg_set_11d_offloaded() - set/clear 11d offloaded flag
 *
 * @psoc: psoc pointer
 * Return: Success or Failure
 */
QDF_STATUS reg_set_11d_offloaded(struct wlan_objmgr_psoc *psoc,
		bool val);

/**
 * reg_get_curr_regdomain() - Get current regdomain in use
 * @pdev: pdev pointer
 * @cur_regdmn: Current regdomain info
 *
 * Return: QDF status
 */
QDF_STATUS reg_get_curr_regdomain(struct wlan_objmgr_pdev *pdev,
				  struct cur_regdmn_info *cur_regdmn);

/**
 * reg_modify_chan_144() - Enable/Disable channel 144
 * @pdev: pdev pointer
 * @enable_chan_144: flag to disable/enable channel 144
 *
 * Return: Success or Failure
 */
QDF_STATUS reg_modify_chan_144(struct wlan_objmgr_pdev *pdev,
			       bool en_chan_144);

/**
 * reg_get_en_chan_144() - get en_chan_144 flag value
 * @pdev: pdev pointer
 *
 * Return: en_chan_144 flag value
 */
bool reg_get_en_chan_144(struct wlan_objmgr_pdev *pdev);

/**
 * reg_process_ch_avoid_event() - Process channel avoid event
 * @psoc: psoc for country information
 * @ch_avoid_event: channel avoid event buffer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_process_ch_avoid_event(struct wlan_objmgr_psoc *psoc,
		struct ch_avoid_ind_type *ch_avoid_event);

/**
 * reg_send_scheduler_msg_sb() - Start scheduler to call list of callbacks
 * registered whenever current chan list changes.
 * @psoc: Pointer to PSOC structure.
 * @pdev: Pointer to PDEV structure.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_send_scheduler_msg_sb(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev);

/**
 * reg_get_hal_reg_cap() - Get HAL REG capabilities
 * @psoc: psoc for country information
 *
 * Return: hal reg cap pointer
 */
struct wlan_psoc_host_hal_reg_capabilities_ext *reg_get_hal_reg_cap(
						struct wlan_objmgr_psoc *psoc);

/**
 * reg_set_hal_reg_cap() - Set HAL REG capabilities
 * @psoc: psoc for country information
 * @reg_cap: Regulatory caps pointer
 * @phy_cnt: number of phy
 *
 * Return: hal reg cap pointer
 */
QDF_STATUS reg_set_hal_reg_cap(struct wlan_objmgr_psoc *psoc,
		struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap,
		uint16_t phy_cnt);

/**
 * reg_chan_in_range() - Check if the given channel is in pdev's channel range
 * @chan_list: Pointer to regulatory channel list.
 * @low_freq_2g: Low frequency 2G.
 * @high_freq_2g: High frequency 2G.
 * @low_freq_5g: Low frequency 5G.
 * @high_freq_5g: High frequency 5G.
 * @ch_enum: Channel enum.
 *
 * Return: true if ch_enum is with in pdev's channel range, else false.
 */
bool reg_chan_in_range(struct regulatory_channel *chan_list,
		uint32_t low_freq_2g,
		uint32_t high_freq_2g,
		uint32_t low_freq_5g,
		uint32_t high_freq_5g,
		enum channel_enum ch_enum);

/**
 * reg_set_ignore_fw_reg_offload_ind() - Set if regdb offload indication
 * needs to be ignored
 * @psoc: Pointer to psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_set_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc);

/**
 * reg_get_ignore_fw_reg_offload_ind() - Check whether regdb offload indication
 * needs to be ignored
 *
 * @psoc: Pointer to psoc
 */
bool reg_get_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc);

#endif
