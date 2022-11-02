/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * Copyright(c) 2016  Realtek Corporation.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#ifndef __RTL8822B_HW_H__
#define __RTL8822B_HW_H__

extern u8 rtl_channel5g[CHANNEL_MAX_NUMBER_5G];
extern u8 rtl_channel5g_80m[CHANNEL_MAX_NUMBER_5G_80M];

void rtl8822be_get_hw_reg(struct ieee80211_hw *hw, u8 variable, u8 *val);
void rtl8822be_read_eeprom_info(struct ieee80211_hw *hw,
				struct rtl_phydm_params *params);
void rtl8822be_read_eeprom_info_dummy(struct ieee80211_hw *hw);
void rtl8822be_interrupt_recognized(struct ieee80211_hw *hw, u32 *p_inta,
				    u32 *p_intb, u32 *p_intc, u32 *p_intd);
int rtl8822be_hw_init(struct ieee80211_hw *hw);
void rtl8822be_card_disable(struct ieee80211_hw *hw);
void rtl8822be_enable_interrupt(struct ieee80211_hw *hw);
void rtl8822be_disable_interrupt(struct ieee80211_hw *hw);
int rtl8822be_set_network_type(struct ieee80211_hw *hw,
			       enum nl80211_iftype type);
void rtl8822be_set_check_bssid(struct ieee80211_hw *hw, bool check_bssid);
void rtl8822be_set_qos(struct ieee80211_hw *hw, int aci);
void rtl8822be_set_beacon_related_registers(struct ieee80211_hw *hw);
void rtl8822be_set_beacon_interval(struct ieee80211_hw *hw);
void rtl8822be_update_interrupt_mask(struct ieee80211_hw *hw, u32 add_msr,
				     u32 rm_msr);
void rtl8822be_set_hw_reg(struct ieee80211_hw *hw, u8 variable, u8 *val);
void rtl8822be_update_hal_rate_tbl(struct ieee80211_hw *hw,
				   struct ieee80211_sta *sta, u8 rssi_level,
				   bool update_bw);
void rtl8822be_update_channel_access_setting(struct ieee80211_hw *hw);
bool rtl8822be_gpio_radio_on_off_checking(struct ieee80211_hw *hw, u8 *valid);
void rtl8822be_set_key(struct ieee80211_hw *hw, u32 key_index, u8 *p_macaddr,
		       bool is_group, u8 enc_algo, bool is_wepkey,
		       bool clear_all);
void rtl8822be_enable_hw_security_config(struct ieee80211_hw *hw);
void rtl8822be_read_bt_coexist_info_from_hwpg(struct ieee80211_hw *hw,
					      bool autoload_fail, u8 *hwinfo);
void rtl8822be_bt_reg_init(struct ieee80211_hw *hw);
void rtl8822be_suspend(struct ieee80211_hw *hw);
void rtl8822be_resume(struct ieee80211_hw *hw);
void rtl8822be_fw_clk_off_timer_callback(unsigned long data);
#endif
