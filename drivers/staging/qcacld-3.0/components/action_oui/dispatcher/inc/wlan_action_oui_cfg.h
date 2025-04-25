/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: This file contains centralized definitions of action oui configuration.
 */
#ifndef __WLAN_ACTION_OUI_CFG_H__
#define __WLAN_ACTION_OUI_CFG_H__
/*
 * Start of action oui inis
 *
 * To enable action oui feature, set gEnableActionOUI
 *
 * Each action oui is expected in the following format:
 * <Extension 1> <Extension 2> ..... <Extension 10> (maximum 10)
 *
 * whereas, each Extension is separated by space and have the following format:
 * <Token1> <Token2> <Token3> <Token4> <Token5> <Token6> <Token7> <Token8>
 * where each Token is a string of hexa-decimal digits and
 * following are the details about each token
 *
 * Token1 = OUI
 * Token2 = Data_Length
 * Token3 = Data
 * Token4 = Data_Mask
 * Token5 = Info_Presence_Bit
 * Token6 = MAC_Address
 * Token7 = Mac_Address Mask
 * Token8 = Capability
 *
 * <OUI> is mandatory and it can be either 3 or 5 bytes means 6 or 10
 * hexa-decimal characters
 * If the OUI and Data checks needs to be ignored, the oui FFFFFF
 * needs to be provided as OUI and bit 0 of Info_Presence_Bit should
 * be set to 0.
 *
 * <Data_Length> is mandatory field and should give length of
 * the <Data> if present else zero
 *
 * Presence of <Data> is controlled by <Data_Length>, if <Data_Length> is 0,
 * then <Data> is not expected else Data of the size Data Length bytes are
 * expected which means the length of Data string is 2 * Data Length,
 * since every byte constitutes two hexa-decimal characters.
 *
 * <Data_Mask> is mandatory if <Data> is present and length of the
 * Data mask string depends on the <Data Length>
 * If <Data Length> is 06, then length of Data Mask string is
 * 2 characters (represents 1 byte)
 * data_mask_length = ((Data_Length - (Data_Length % 8)) / 8) +
 *		      ((Data_Length % 8) ? 1 : 0)
 * and <Data_Mask> has to be constructed from left to right.
 *
 * Presence of <Mac_Address> and <Capability> is
 * controlled by <Info_Presence_Bit> which is mandatory
 * <Info_Presence_Bit> will give the information for
 *   OUI – bit 0 Should be set to 1
 *		 Setting to 0 will ignore OUI and data check
 *   Mac Address present – bit 1
 *   NSS – bit 2
 *   HT check – bit 3
 *   VHT check – bit 4
 *   Band info – bit 5
 *   reserved – bit 6 (should always be zero)
 *   reserved – bit 7 (should always be zero)
 * and should be constructed from right to left (b7b6b5b4b3b2b1b0)
 *
 * <Mac_Address_Mask> for <Mac_Address> should be constructed from left to right
 *
 * <Capability> is 1 byte long and it contains the below info
 *   NSS – 4 bits starting from LSB (b0 – b3)
 *   HT enabled – bit 4
 *   VHT enabled – bit 5
 *   2G band – bit 6
 *   5G band – bit 7
 * and should be constructed from right to left (b7b6b5b4b3b2b1b0)
 * <Capability> is present if at least one of the bit is set
 * from b2 - b6 in <Info_Presence_Bit>
 *
 * Example 1:
 *
 * OUI is 00-10-18, data length is 05 (hex form), data is 02-11-04-5C-DE and
 * need to consider first 3 bytes and last byte of data for comparison
 * mac-addr EE-1A-59-FE-FD-AF is present and first 3 bytes and last byte of
 * mac address should be considered for comparison
 * capability is not present
 * then action OUI for gActionOUIITOExtension is as follows:
 *
 * gActionOUIITOExtension=001018 05 0211045CDE E8 03 EE1A59FEFDAF E4
 *
 * data mask calculation in above example:
 * Data[0] = 02 ---- d0 = 1
 * Data[1] = 11 ---- d1 = 1
 * Data[2] = 04 ---- d2 = 1
 * Data[3] = 5C ---- d3 = 0
 * Data[4] = DE ---- d4 = 1
 * data_mask = d0d1d2d3d4 + append with zeros to complete 8-bit = 11101000 = E8
 *
 * mac mask calculation in above example:
 * mac_addr[0] = EE ---- m0 = 1
 * mac_addr[1] = 1A ---- m1 = 1
 * mac_addr[2] = 59 ---- m2 = 1
 * mac_addr[3] = FE ---- m3 = 0
 * mac_addr[4] = FD ---- m4 = 0
 * mac_addr[5] = AF ---- m5 = 1
 * mac_mask = m0m1m2m3m4m5 + append with zeros to complete 8-bit = 11100100 = E4
 *
 * Example 2:
 *
 * OUI is 00-10-18, data length is 00 and no Mac Address and capability
 *
 * gActionOUIITOExtension=001018 00 01
 *
 */

/*
 * <ini>
 * gEnableActionOUI - Enable/Disable action oui feature
 * @Min: 0 (disable)
 * @Max: 1 (enable)
 * @Default: 1 (enable)
 *
 * This ini is used to enable the action oui feature to control
 * mode of connection, connected AP's in-activity time, Tx rate etc.,
 *
 * Related: If gEnableActionOUI is set, then at least one of the following inis
 * must be set with the proper action oui extensions:
 * gActionOUIConnect1x1, gActionOUIITOExtension, gActionOUICCKM1X1
 *
 * Supported Feature: action ouis
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_ACTION_OUI CFG_INI_BOOL( \
	"gEnableActionOUI", \
	1, \
	"Enable/Disable action oui feature")

/*
 * <ini>
 * gActionOUIConnect1x1 - Used to specify action OUIs for 1x1 connection
 * @Default: 000C43 00 25 C2 001018 06 02FFF02C0000 BC 25 42 001018 06 02FF040C0000 BC 25 42 00037F 00 35 6C 001018 06 02FF009C0000 BC 25 48
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1 : 000C43
 *   OUI data Len : 00
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: C2 - NSS == 2 && Band == 2G || Band == 5G
 * OUI 2 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF02C0000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: 42 - NSS == 2 && Band == 2G
 * OUI 3 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FF040C0000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: 42 - NSS == 2 && Band == 2G
 * OUI 4 : 00037F
 *   OUI data Len : 00
 *   Info Mask : 35 - Check for NSS, VHT Caps and Band
 *   Capabilities: 6C - (NSS == 3 or 4) && VHT Caps Preset && Band == 2G
 * OUI 5 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FF009C0000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: 48 - NSS == 4 && Band == 2G
 *
 * This ini is used to specify the AP OUIs with which only 1x1 connection
 * is allowed.
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_CONNECT_1X1 CFG_INI_STRING( \
	"gActionOUIConnect1x1", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"000C43 00 25 C2 001018 06 02FFF02C0000 BC 25 42 001018 06 02FF040C0000 BC 25 42 00037F 00 35 6C 001018 06 02FF009C0000 BC 25 48", \
	"Used to specify action OUIs for 1x1 connection")

/*
 * <ini>
 * gActionOUIITOExtension - Used to extend in-activity time for specified APs
 * @Default: 00037F 06 01010000FF7F FC 01 000AEB 02 0100 C0 01 000B86 03 010408 E0 01
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1: 00037F
 *   OUI data Len: 06
 *   OUI Data: 01010000FF7F
 *   OUI data Mask: FC - 11111100
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 2: 000AEB
 *   OUI data Len: 02
 *   OUI Data: 0100
 *   OUI data Mask: C0 - 11000000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 3: 000B86
 *   OUI data Len: 03
 *   OUI Data: 010408
 *   OUI data Mask: E0 - 11100000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * This ini is used to specify AP OUIs using which station's in-activity time
 * can be extended with the respective APs
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_ITO_EXTENSION CFG_INI_STRING( \
	"gActionOUIITOExtension", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"00037F 06 01010000FF7F FC 01 000AEB 02 0100 C0 01 000B86 03 010408 E0 01", \
	"Used to extend in-activity time for specified APs")

/*
 * <ini>
 * gActionOUICCKM1X1 - Used to specify action OUIs to control station's TX rates
 *
 * This ini is used to specify AP OUIs for which station's CCKM TX rates
 * should be 1x1 only.
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_CCKM_1X1 CFG_INI_STRING( \
	"gActionOUICCKM1X1", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"Used to specify action OUIs to control station's TX rates")

/*
 * <ini>
 * gActionOUIITOAlternate - Used to specify action OUIs to have alternate ITO in
 * weak RSSI state
 *
 * This ini is used to specify AP OUIs for which the stations will have
 * alternate ITOs for the case when the RSSI is weak.
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_ITO_ALTERNATE CFG_INI_STRING( \
	"gActionOUIITOAlternate", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"001018 06 0202001c0000 FC 01", \
	"Used to specify action OUIs to have alternate ITO")

/*
 * <ini>
 * gActionOUISwitchTo11nMode - Used to specify action OUIs for switching to 11n
 *
 * This ini is used to specify which AP for which the connection has to be
 * made in 2x2 mode with HT capabilities only and not VHT.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1 : 00904C
 *   OUI data Len : 03
 *   OUI Data : 0418BF
 *   OUI data Mask: E0 - 11100000
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_SWITCH_TO_11N_MODE CFG_INI_STRING( \
	"gActionOUISwitchTo11nMode", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"00904C 05 0418BF0CB2 F8 21 40", \
	"Used to specify action OUIs for switching to 11n")

/*
 * <ini>
 * gActionOUIConnect1x1with1TxRxChain - Used to specify action OUIs for
 *					 1x1 connection with one Tx/Rx Chain
 * @Default:
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF0040000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * OUI 2 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF0050000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * OUI 3 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF4050000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * This ini is used to specify the AP OUIs with which only 1x1 connection
 * with one Tx/Rx Chain is allowed.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN CFG_INI_STRING( \
	 "gActionOUIConnect1x1with1TxRxChain", \
	 0, \
	 ACTION_OUI_MAX_STR_LEN, \
	 "001018 06 02FFF0040000 BC 21 40 001018 06 02FFF0050000 BC 21 40 001018 06 02FFF4050000 BC 21 40", \
	 "Used to specify action OUIs for 1x1 connection with one Tx/Rx Chain")

/*
 * <ini>
 * gActionOUIDisableAggressiveTX - Used to specify action OUIs to disable
 * Aggressive TX feature when operating in softap.
 *
 * @Default:
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs:
 *
 * OUI 1 : FFFFFF
 *   OUI data Len : 00
 *   OUI Data: No data
 *   OUI data Mask: No data mask
 *   Info Mask:  2A - Check for mac-addr, HT capability and Band
 *   Mac-addr: F8:59:71:00:00:00 - first 3 bytes
 *   Mac-mask: E0 - Match only first 3 bytes of peer mac-addr
 *   Capabilities: 50 – HT should be enabled, and band should be 2.4GHz
 *
 * OUI 2 : FFFFFF
 *   OUI data Len : 00
 *   OUI Data: No data
 *   OUI data Mask: No data mask
 *   Info Mask:  2A - Check for mac-addr, HT capability and Band
 *   Mac-addr: 14:AB:C5:00:00:00 - first 3 bytes
 *   Mac-mask: E0 - Match only first 3 bytes of peer mac-addr
 *   Capabilities: 50 – HT should be enabled, and band should be 2.4GHz
 *
 * When operating in Softap mode, this ini is used to specify
 * STA (peer) OUIs/mac-addr for which aggressive tx is disabled after
 * association is successful.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_DISABLE_AGGRESSIVE_TX CFG_INI_STRING( \
	"gActionOUIDisableAggressiveTX", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"FFFFFF 00 2A F85971000000 E0 50 FFFFFF 00 2A 14ABC5000000 E0 50", \
	"Used to specify action OUIs to disable aggressive TX")

/*
 * <ini>
 * gActionOUIDisableAggressiveEDCA - Used to specify action OUIs to control
 * EDCA configuration when join the candidate AP
 *
 * @Default: NULL
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * This ini is used to specify AP OUIs. The station's EDCA should follow the
 * APs' when connecting to those AP, even if the gEnableEdcaParams is set.
 * For example, it follows the AP's EDCA whose OUI is 0050F2 with the
 * following setting:
 *     gActionOUIDisableAggressiveEDCA=0050F2 00 01
 *          Explain: 0050F2: OUI
 *                   00: data length is 0
 *                   01: info mask, only OUI present in Info mask
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableEdcaParams, gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_DISABLE_AGGRESSIVE_EDCA CFG_INI_STRING( \
	"gActionOUIDisableAggressiveEDCA", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"Used to specify action OUIs to control edca configuration")

/*
 * <ini>
 * gActionOUIExtendWowITO - Used to extend ITO(Inactivity Time-Out) value under
 * WoWLAN mode for specified APs.
 *
 * @Default: NULL
 *
 * Some APs sometimes don't honor Qos null frames under WoWLAN mode if
 * station's ITO is too small. This ini is used to specify AP OUIs which
 * exhibit this behavior. When connected to such an AP, the station's ITO
 * value will be extended when in WoWLAN mode.
 * For example, it extends the ITO value(under WoWLAN mode) when connected
 * to AP whose OUI is 001018 and vendor specific data is 0201009C0000 with
 * the following setting:
 *     gActionOUIExtendWowITO=001018 06 0201009C0000 FC 01
 *         OUI: 001018
 *         OUI data Len : 06
 *         OUI Data : 0201009C0000
 *         OUI data Mask: FC - 11111100
 *         Info Mask : 01 - only OUI present in Info mask
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_EXTEND_WOW_ITO CFG_INI_STRING( \
	"gActionOUIExtendWowITO", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"Used to extend inactivity time out under WoWLAN mode for specified APs")

/*
 * <ini>
 * gActionOUIReconnAssocTimeout - Used to specify action OUIs to
 * reconnect to same BSSID when wait for association response timeout
 *
 * This ini is used to specify AP OUIs. Some of AP doesn't response our
 * first association request, but it would response our second association
 * request. Add such OUI configuration INI to apply reconnect logic when
 * association timeout happends with such AP.
 * For default:
 *     gActionOUIReconnAssocTimeout=00E04C 00 01
 *          Explain: 00E04C: OUI
 *                   00: data length is 0
 *                   01: info mask, only OUI present in Info mask
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_RECONN_ASSOCTIMEOUT CFG_INI_STRING( \
	"gActionOUIReconnAssocTimeout", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"00E04C 00 01", \
	"Used to specify action OUIs to reconnect when assoc timeout")

/*
 * <ini>
 * gActionOUIDisableTWT - Used to specify action OUIs to control TWT param
 * while joining the candidate AP
 *
 * This ini is used to specify AP OUIs. Some APs advertise TWT but do not
 * follow through when the STA reaches out to them. Thus, TWT will be
 * disabled when we receive OUIs of those APs.
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1: 001018
 *   OUI data Len: 00
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 2: 000986
 *   OUI data Len: 00
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 3: 000ce7
 *   OUI data Len: 00
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 4: 00e0fc
 *   OUI data Len: 00
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_DISABLE_TWT CFG_INI_STRING( \
	"gActionOUIDisableTWT", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"001018 00 01 000986 00 01 000ce7 00 01 00e0fc 00 01", \
	"Used to specify action OUIs to control TWT configuration")

/*
 * <ini>
 * gActionOUITakeAllBandInfo - Used to specify action OUIs to check
 * whether country ie need take all band channel information.
 *
 * This ini is used to specify STA association request OUIs. Some STA
 * need AP country ie take all band channel information when do BSS
 * transition across band. Thus, AP will take all band channel info
 * when we receive association request with this OUIs.
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1: 0017f2
 *   OUI data Len: 01
 *   OUI Data : 0a
 *   OUI data Mask: 80 - 10000000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_TAKE_ALL_BAND_INFO CFG_INI_STRING( \
	"gActionOUITakeAllBandInfo", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"0017f2 01 0a 80 01", \
	"Used to specify action OUIs to control country ie")

/*
 * <ini>
 * g11be_oui_allow_list - Used to specify 802.11be allowed ap oui list
 *
 * This ini is used to specify AP OUIs for which station can connect
 * in 802.11be mode with the 802.11be AP.
 * If no OUI set, then allow STA to connect to All 802.11be AP in 802.11be
 * mode.
 * If INI is set to "ffffff 00 01", then STA is not allowed to connect to
 * any AP in 802.11be mode.
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_11BE_ALLOW_LIST CFG_INI_STRING( \
	"g11be_oui_allow_list", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"Used to specify 11be allowed ap oui list")

/*
 * <ini>
 * gActionOUIDisableDynamicQosNullTxRate - Used to turn off FW's dynamic qos
 * null tx rate feature if specific vendor OUI received in beacon
 *
 * Some APs sometimes don't honor Qos null frames with some specific rate.
 * This ini will disable dynamic qos null tx rate feature for specified APs.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1: 00e04c
 *   OUI data Len: 03
 *   OUI Data : 020160
 *   OUI data Mask: E0 - 11100000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 2: 001018
 *   OUI data Len : 06
 *   OUI Data : 02FF009C0000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 01 - only OUI present in Info mask
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_DISABLE_DYNAMIC_QOS_NULL_TX_RATE CFG_INI_STRING( \
	"gActionOUIDisableDynamicQosNullTxRate", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"00e04c 03 020160 E0 01 001018 06 02FF009c0000 BC 01", \
	"Used to turn off FW's dynamic qos null tx rate for specified APs")

/*
 * <ini>
 * gActionOUIAuthAssoc6Mbps2GHz - Used to send auth/assoc req with 6 Mbps rate
 * on 2.4 GHz for specified AP
 *
 * Some AP sometimes doesn't honor auth/assoc with CCK rate.
 * This ini will provide 6 Mbps rate for auth/assoc in 2.4 GHz.
 *
 * Example OUIs: (All values in Hex)
 * OUI 1: 000c43
 *       OUI data Len: 04
 *       OUI Data : 03000000
 *       OUI data Mask: F0 - 11110000
 *       Info Mask : 01 - only OUI present in Info mask
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_AUTH_ASSOC_6MBPS_2GHZ CFG_INI_STRING( \
	"gActionOUIAuthAssoc6Mbps2GHz", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"send auth/assoc req with 6 Mbps rate on 2.4 GHz for specified APs")

/*
 * <ini>
 * CFG_ACTION_OUI_DISABLE_BFORMEE - Used to disable SU/MU beamformee
 * capability for specified AP with some conditions
 *
 * Example OUIs: (All values in Hex)
 * OUI 1: 000c43
 *       OUI data Len: 04
 *       OUI Data : 03000000
 *       OUI data Mask: F0 - 11110000
 *       Info Mask : 01 - only OUI present in Info mask
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_DISABLE_BFORMEE CFG_INI_STRING( \
	"gActionOUIDisableBFORMEE", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"disable SU/MU beamformee capability for specified AP")

/*
 * <ini>
 * gActionOUIEnableCTS2SelfWithQoSNull - Used to enable CTS2SELF with QoS null
 * frame for specified APs
 *
 * Sample OUIs: (All values in Hex)
 * OUI 1: 000c43
 *   OUI data Len: 04
 *   OUI Data : 03000000
 *   OUI data Mask: F0 - 11110000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * gActionOUIEnableCTS2SelfWithQoSNull=000c43 04 03000000 F0 01
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_ENABLE_CTS2SELF_WITH_QOS_NULL CFG_INI_STRING( \
	"gActionOUIEnableCTS2SelfWithQoSNull", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"Used to enable CTS2SELF with QoS null frame for specified APs")

/*
 * <ini>
 * g_action_oui_enable_cts_2_self - Used to enable CTS2SELF for specified APs
 *
 * Default OUIs: (All values in Hex)
 * OUI 1: 000C43
 * OUI data Len: 04
 * OUI Data : 07000000
 * OUI data Mask: F0 - 11110000
 * Info Mask : 21 - 0010 0001 Check for OUI and Band
 * Capabilities: C0 - 1100 0000 Band == 2 GHz || Band == 5 GHz
 *
 * OUI 2 : 000C43
 * OUI data Len : 04
 * OUI Data : 03000000
 * OUI data Mask: F0 - 11110000
 * Info Mask : 21 - 0010 0001 Check for OUI and Band
 * Capabilities: C0 - 1100 0000 Band == 2 GHz || Band == 5 GHz
 *
 * g_action_oui_enable_cts_2_self=000C43 04 07000000 F0 21 C0 000C43 04 03000000 F0 21 C0
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_ENABLE_CTS2SELF CFG_INI_STRING( \
	"g_action_oui_enable_cts_2_self", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"000C43 04 07000000 F0 21 C0 000C43 04 03000000 F0 21 C0", \
	"Used to enable CTS2SELF frame for specified APs")

/*
 * <ini>
 * gActionOUISendSMPSFrameWithOMN - Used to send SMPS frame along with OMN
 * for specified APs
 *
 * Sample OUIs: (All values in Hex)
 * OUI 1: 000ce7
 *   OUI data Len: 04
 *   OUI Data : 88000000
 *   OUI data Mask: F0 - 11110000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * gActionOUISendSMPSFrameWithOMN=000ce7 04 88000000 F0 01
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_SEND_SMPS_FRAME_WITH_OMN CFG_INI_STRING( \
	"gActionOUISendSMPSFrameWithOMN", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"Used to send SMPS frame along with OMN for specified APs")

/*
 * <ini>
 * gActionOUIRestrictMaxMLOLinks - Used to downgrade 3 link to 2 link ML
 * connection for specific AP build version.
 *
 * Sample OUIs: (All values in Hex)
 * OUI 3 : 8CFDF0
 *   OUI data Len : 13
 *   OUI Data : 040000494c510302097201cb17000009110000
 *   OUI data Mask: FFFFE0 - 1111 1111 1111 1111 1110 0000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * gActionOUIRestrictMaxMLOLinks=8CFDF0 13 040000494c510c00203000cb17000009110000 FFFFE0 01
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_RESTRICT_MAX_MLO_LINKS CFG_INI_STRING( \
	"gActionOUIRestrictMaxMLOLinks", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"8CFDF0 13 040000494c510302097201cb17000009110000 FFFFE0 01", \
	"To restrict matching OUI APs to two link connection at max")

/*
 * <ini>
 * CFG_ACTION_OUI_LIMIT_BW - Used to limit BW for specified AP
 *
 * Example OUIs: (All values in Hex)
 * OUI 1: 00904c
 *       OUI data Len: 04
 *       OUI Data : 0201009C
 *       OUI data Mask: F0 - 11110000
 *       Info Mask : 01 - only OUI present in Info mask
 *
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_LIMIT_BW CFG_INI_STRING( \
	"gActionOUILimitBW", \
	0, \
	ACTION_OUI_MAX_STR_LEN, \
	"", \
	"Limit BW for specified AP")

#define CFG_ACTION_OUI \
	CFG(CFG_ACTION_OUI_CCKM_1X1) \
	CFG(CFG_ACTION_OUI_CONNECT_1X1) \
	CFG(CFG_ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN) \
	CFG(CFG_ACTION_OUI_ITO_ALTERNATE) \
	CFG(CFG_ACTION_OUI_ITO_EXTENSION) \
	CFG(CFG_ACTION_OUI_DISABLE_AGGRESSIVE_TX) \
	CFG(CFG_ACTION_OUI_DISABLE_AGGRESSIVE_EDCA) \
	CFG(CFG_ACTION_OUI_EXTEND_WOW_ITO) \
	CFG(CFG_ACTION_OUI_SWITCH_TO_11N_MODE) \
	CFG(CFG_ACTION_OUI_RECONN_ASSOCTIMEOUT) \
	CFG(CFG_ACTION_OUI_DISABLE_TWT) \
	CFG(CFG_ACTION_OUI_TAKE_ALL_BAND_INFO) \
	CFG(CFG_ACTION_OUI_11BE_ALLOW_LIST) \
	CFG(CFG_ACTION_OUI_DISABLE_DYNAMIC_QOS_NULL_TX_RATE) \
	CFG(CFG_ACTION_OUI_ENABLE_CTS2SELF) \
	CFG(CFG_ACTION_OUI_ENABLE_CTS2SELF_WITH_QOS_NULL) \
	CFG(CFG_ACTION_OUI_RESTRICT_MAX_MLO_LINKS) \
	CFG(CFG_ACTION_OUI_SEND_SMPS_FRAME_WITH_OMN) \
	CFG(CFG_ACTION_OUI_AUTH_ASSOC_6MBPS_2GHZ) \
	CFG(CFG_ACTION_OUI_DISABLE_BFORMEE) \
	CFG(CFG_ACTION_OUI_LIMIT_BW) \
	CFG(CFG_ENABLE_ACTION_OUI)
#endif
