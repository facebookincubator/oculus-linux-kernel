/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains commnon ieee80211 definitions
 */

#ifndef _WLAN_CMN_IEEE80211_H_
#define _WLAN_CMN_IEEE80211_H_
#include <qdf_types.h>
#include <osdep.h>

/* Subfields in Frame Control field (except Type and Subtype) */
#define WLAN_FC0_PVER      0x0003
#define WLAN_FC1_DIR_MASK  0x03
#define WLAN_FC1_TODS      0x01
#define WLAN_FC1_FROMDS    0x02
#define WLAN_FC1_DSTODS    0x03
#define WLAN_FC1_MOREFRAG  0x04
#define WLAN_FC1_RETRY     0x08
#define WLAN_FC1_PWRMGT    0x10
#define WLAN_FC1_MOREDATA  0x20
#define WLAN_FC1_ISWEP     0x40
#define WLAN_FC1_ORDER     0x80

/* Definitions for Type subfield in Frame Control field */
#define WLAN_FC0_TYPE_MGMT        0
#define WLAN_FC0_TYPE_CTRL        1
#define WLAN_FC0_TYPE_DATA        2

/* Definitions for management frame subtypes in Frame Control field */
#define WLAN_FC0_STYPE_ASSOC_REQ      0
#define WLAN_FC0_STYPE_ASSOC_RESP     1
#define WLAN_FC0_STYPE_REASSOC_REQ    2
#define WLAN_FC0_STYPE_REASSOC_RESP   3
#define WLAN_FC0_STYPE_PROBE_REQ      4
#define WLAN_FC0_STYPE_PROBE_RESP     5
#define WLAN_FC0_STYPE_BEACON         8
#define WLAN_FC0_STYPE_ATIM           9
#define WLAN_FC0_STYPE_DISASSOC      10
#define WLAN_FC0_STYPE_AUTH          11
#define WLAN_FC0_STYPE_DEAUTH        12
#define WLAN_FC0_STYPE_ACTION        13

/* Definitions for control frame subtypes in Frame Control field */
#define WLAN_FC0_STYPE_PSPOLL        10
#define WLAN_FC0_STYPE_RTS           11
#define WLAN_FC0_STYPE_CTS           12
#define WLAN_FC0_STYPE_ACK           13
#define WLAN_FC0_STYPE_CFEND         14
#define WLAN_FC0_STYPE_CFENDACK      15

/* Definitions for data frame subtypes in Frame Control field */
#define WLAN_FC0_STYPE_DATA                0
#define WLAN_FC0_STYPE_DATA_CFACK          1
#define WLAN_FC0_STYPE_DATA_CFPOLL         2
#define WLAN_FC0_STYPE_DATA_CFACKPOLL      3
#define WLAN_FC0_STYPE_NULLFUNC            4
#define WLAN_FC0_STYPE_CFACK               5
#define WLAN_FC0_STYPE_CFPOLL              6
#define WLAN_FC0_STYPE_CFACKPOLL           7
#define WLAN_FC0_STYPE_QOS_DATA            8
#define WLAN_FC0_STYPE_QOS_DATA_CFACK      9
#define WLAN_FC0_STYPE_QOS_DATA_CFPOLL    10
#define WLAN_FC0_STYPE_QOS_DATA_CFACKPOLL 11
#define WLAN_FC0_STYPE_QOS_NULL           12
#define WLAN_FC0_STYPE_QOS_CFPOLL         14
#define WLAN_FC0_STYPE_QOS_CFACKPOLL      15

/* Get Type/Subtype subfields in Frame Control field */
#define WLAN_FC0_GET_TYPE(fc)    (((fc) & 0x0c) >> 2)
#define WLAN_FC0_GET_STYPE(fc)   (((fc) & 0xf0) >> 4)

/* Definitions related to sequence number processing, TID, etc. */
#define WLAN_INVALID_MGMT_SEQ   0xffff
#define WLAN_SEQ_MASK           0x0fff
#define WLAN_GET_SEQ_FRAG(seq) ((seq) & (BIT(3) | BIT(2) | BIT(1) | BIT(0)))
#define WLAN_GET_SEQ_SEQ(seq) \
	(((seq) & (~(BIT(3) | BIT(2) | BIT(1) | BIT(0)))) >> 4)
#define WLAN_QOS_TID_MASK       0x0f
#define WLAN_TID_SIZE           17
#define WLAN_NONQOS_SEQ         16

/* Length of Timestamp field */
#define WLAN_TIMESTAMP_LEN         8

/* Length of Beacon Interval field */
#define WLAN_BEACONINTERVAL_LEN    2

/* Length of Capability Information field */
#define WLAN_CAPABILITYINFO_LEN    2

/* Length of Listen Interval field */
#define WLAN_LISTENINTERVAL_LEN    2

/* Length of Status code field */
#define WLAN_STATUSCODE_LEN        2

/* Length of AID field */
#define WLAN_AID_LEN               2

/* Assoc resp IE offset Capability(2) + Status Code(2) + AID(2) */
#define WLAN_ASSOC_RSP_IES_OFFSET \
	(WLAN_CAPABILITYINFO_LEN  + WLAN_STATUSCODE_LEN + WLAN_AID_LEN)

/* Assoc req IE offset - Capability(2) + LI(2) */
#define WLAN_ASSOC_REQ_IES_OFFSET \
	(WLAN_CAPABILITYINFO_LEN + WLAN_LISTENINTERVAL_LEN)

/* Reassoc req IE offset - Capability(2) + LI(2) + current AP address(6) */
#define WLAN_REASSOC_REQ_IES_OFFSET \
	(WLAN_CAPABILITYINFO_LEN + WLAN_LISTENINTERVAL_LEN + QDF_MAC_ADDR_SIZE)

/* Probe response IE offset - timestamp(8) + Beacon Int(2) + Cap info(2) */
#define WLAN_PROBE_RESP_IES_OFFSET \
	(WLAN_TIMESTAMP_LEN + WLAN_BEACONINTERVAL_LEN + WLAN_CAPABILITYINFO_LEN)

/* Beacon IE offset - timestamp(8) + Beacon Int(2) + Cap info(2) */
#define WLAN_BEACON_IES_OFFSET \
	(WLAN_TIMESTAMP_LEN + WLAN_BEACONINTERVAL_LEN + WLAN_CAPABILITYINFO_LEN)

/* Length (in bytes) of MAC header in 3 address format */
#define WLAN_MAC_HDR_LEN_3A 24

#define IEEE80211_CCMP_HEADERLEN    8
#define IEEE80211_HT_CTRL_LEN       4
#define IEEE80211_CCMP_MICLEN       8
#define WLAN_IEEE80211_GCMP_HEADERLEN    8
#define WLAN_IEEE80211_GCMP_MICLEN       16

#define IEEE80211_FC1_RETRY         0x08
#define IEEE80211_FC1_WEP           0x40
#define IEEE80211_FC1_ORDER         0x80

#define WLAN_HDR_IV_LEN            3
#define WLAN_HDR_EXT_IV_BIT        0x20
#define WLAN_HDR_EXT_IV_LEN        4

#define WLAN_SEQ_SEQ_SHIFT 4

#define P2P_WFA_OUI {0x50, 0x6f, 0x9a}
#define P2P_WFA_VER 0x09

#define WSC_OUI 0x0050f204
#define MBO_OCE_OUI 0x506f9a16
#define MBO_OCE_OUI_SIZE 4
#define REDUCED_WAN_METRICS_ATTR 103
#define OCE_DISALLOW_ASSOC_ATTR  0x4
#define AP_TX_PWR_ATTR 107
#define OCE_SUBNET_ID_ATTR 108
#define OCE_SUBNET_ID_LEN 6
#define OSEN_OUI 0x506f9a12

/* WCN IE */
/* Microsoft OUI */
#define WCN_OUI 0xf25000
/* WCN */
#define WCN_OUI_TYPE 0x04
#define WME_OUI 0xf25000
#define WME_OUI_TYPE 0x02
#define WME_PARAM_OUI_SUBTYPE 0x01
#define WME_INFO_OUI_SUBTYPE 0x00
 /* Atheros OUI */
#define ATH_OUI 0x7f0300
#define ATH_OUI_TYPE 0x01
/* Atheros Extended Cap Type */
#define ATH_OUI_EXTCAP_TYPE 0x04
/* QCA Bandwidth NSS Mapping Type */
#define ATH_OUI_BW_NSS_MAP_TYPE 0x05
#define SFA_OUI 0x964000
#define SFA_OUI_TYPE 0x14
/* QCA OUI (in little endian) */
#define QCA_OUI 0xf0fd8c
#define QCN_OUI_TYPE_CMN 0x01
#define QCA_OUI_WHC_TYPE  0x00
#define QCA_OUI_WHC_REPT_TYPE 0x01

/**
 * enum qcn_attribute_id: QCN IE attribute ID
 * @QCN_ATTRIB_VERSION: QCN version
 * @QCN_ATTRIB_VHT_MCS10_11_SUPP: VHT MCS 10-11 support
 * @QCN_ATTRIB_HE_400NS_SGI_SUPP: HE 400 NS SGI support
 * @QCN_ATTRIB_HE_2XLTF_160_80P80_SUPP: HE 2X LTF 160 80P80 support
 * @QCN_ATTRIB_HE_DL_OFDMA_SUPP: HE DL OFDMA support
 * @QCN_ATTRIB_TRANSITION_REASON: Transition reason
 * @QCN_ATTRIB_TRANSITION_REJECTION: Transition rejection
 * @QCN_ATTRIB_HE_DL_MUMIMO_SUPP: DL MUMIMO support
 * @QCN_ATTRIB_HE_MCS12_13_SUPP: MCS 12-13 support
 * @QCN_ATTRIB_REPEATER_INFO: Repeater information
 * @QCN_ATTRIB_HE_240_MHZ_SUPP: HE 240 MHZ support
 * @QCN_ATTRIB_ECSA_SUPP: ECSA support
 * @QCN_ATTRIB_EDCA_PIFS_PARAM: EDCA PIFS param
 * @QCN_ATTRIB_MAX: Maximum attribute
 */
enum qcn_attribute_id {
	QCN_ATTRIB_VERSION                  = 0x01,
	QCN_ATTRIB_VHT_MCS10_11_SUPP        = 0X02,
	QCN_ATTRIB_HE_400NS_SGI_SUPP        = 0X03,
	QCN_ATTRIB_HE_2XLTF_160_80P80_SUPP  = 0X04,
	QCN_ATTRIB_HE_DL_OFDMA_SUPP         = 0X05,
	QCN_ATTRIB_TRANSITION_REASON        = 0x06,
	QCN_ATTRIB_TRANSITION_REJECTION     = 0x07,
	QCN_ATTRIB_HE_DL_MUMIMO_SUPP        = 0X08,
	QCN_ATTRIB_HE_MCS12_13_SUPP         = 0X09,
	QCN_ATTRIB_REPEATER_INFO            = 0X0A,
	QCN_ATTRIB_HE_240_MHZ_SUPP          = 0X0B,
	QCN_ATTRIB_ECSA_SUPP                = 0X0C,
	QCN_ATTRIB_EDCA_PIFS_PARAM          = 0X0D,
	QCN_ATTRIB_MAX                      = 0x0E
};

/* Extender vendor specific IE */
#define QCA_OUI_EXTENDER_TYPE           0x03

#define ADAPTIVE_11R_OUI      0x964000
#define ADAPTIVE_11R_OUI_TYPE 0x2C

#define OUI_LENGTH              4
#define OUI_TYPE_BITS           24
#define MAX_ADAPTIVE_11R_IE_LEN 8

/* Minimum length of Non-Inheritance element (inclusive of the IE header) */
#define MIN_NONINHERITANCEELEM_LEN 5

/*
 * sae single pmk vendor specific IE details
 * Category     Data
 * Type         0xDD
 * Length       0x05
 * OUI          0x00 40 96
 * Type         0x03
 * Data         Don’t care (EX, 0x05)
 */
#define SAE_SINGLE_PMK_OUI          0x964000
#define SAE_SINGLE_PMK_TYPE         0x03
#define MAX_SAE_SINGLE_PMK_IE_LEN   8

/* Temporary vendor specific IE for 11n pre-standard interoperability */
#define VENDOR_HT_OUI       0x00904c
#define VENDOR_HT_CAP_ID    51
#define VENDOR_HT_INFO_ID   52

#define VHT_INTEROP_OUI 0x00904c
#define VHT_INTEROP_TYPE 0x04
#define VHT_INTEROP_OUI_SUBTYPE 0x08
#define VHT_INTEROP_OUI_SUBTYPE_VENDORSPEC 0x18

/* ATH HE OUI ( in little endian) */
#define ATH_HE_OUI                  0x741300
#define ATH_HE_CAP_SUBTYPE          0x01
#define ATH_HE_OP_SUBTYPE           0x02

/* EPR information element flags */
#define ERP_NON_ERP_PRESENT   0x01
#define ERP_USE_PROTECTION    0x02
#define ERP_LONG_PREAMBLE     0x04

#define QCA_OUI_WHC_AP_INFO_SUBTYPE 0x00

#define WLAN_MAX_IE_LEN                255
#define WLAN_RSN_IE_LEN                22

/* Individual element IEs length checks */

/* Maximum supported basic/mandatory rates are 12 */
#define WLAN_SUPPORTED_RATES_IE_MAX_LEN          12
#define WLAN_FH_PARAM_IE_MAX_LEN                 5
#define WLAN_DS_PARAM_IE_MAX_LEN                 1
#define WLAN_CF_PARAM_IE_MAX_LEN                 6
#define WLAN_COUNTRY_IE_MIN_LEN                  3
#define WLAN_QUIET_IE_MAX_LEN                    6
#define WLAN_CSA_IE_MAX_LEN                      3
#define WLAN_XCSA_IE_MAX_LEN                     4
#define WLAN_SECCHANOFF_IE_MAX_LEN               1
#define WLAN_EXT_SUPPORTED_RATES_IE_MAX_LEN      12

#define WLAN_EXTCAP_IE_MAX_LEN                   15
#define WLAN_FILS_INDICATION_IE_MIN_LEN          2
#define WLAN_MOBILITY_DOMAIN_IE_MAX_LEN          3
#define WLAN_OPMODE_IE_MAX_LEN                   1
#define WLAN_IBSSDFS_IE_MIN_LEN                  7
#define WLAN_IBSS_IE_MAX_LEN                     2
#define WLAN_REQUEST_IE_MAX_LEN                  255
#define WLAN_RM_CAPABILITY_IE_MAX_LEN            5
#define WLAN_RNR_IE_MIN_LEN                      5
#define WLAN_RNR_TBTT_OFFSET_INVALID             255
#define WLAN_TPE_IE_MIN_LEN                      2
#define WLAN_MAX_NUM_TPE_IE                      8

/* Wide band channel switch IE length */
#define WLAN_WIDE_BW_CHAN_SWITCH_IE_LEN          3

/* Number of max TX power elements supported plus size of Transmit Power
 * Information element.
 */
#define WLAN_TPE_IE_MAX_LEN                      9

#ifdef WLAN_FEATURE_11BE
/* Bandwidth indication element IE maximum length */
#define WLAN_BW_IND_IE_MAX_LEN              9

/* header length is id(1) + length(1)*/
#define WLAN_IE_HDR_LEN                     2

/* 20MHz Operating Channel width */
#define IEEE80211_11BEOP_CHWIDTH_20              0
/* 40MHz Operating Channel width */
#define IEEE80211_11BEOP_CHWIDTH_40              1
/* 80MHz Operating Channel width */
#define IEEE80211_11BEOP_CHWIDTH_80              2
/* 160 MHz Operating Channel width */
#define IEEE80211_11BEOP_CHWIDTH_160             3
/* 320 MHz Operating Channel width */
#define IEEE80211_11BEOP_CHWIDTH_320             4
#endif

/* Max channel switch time IE length */
#define WLAN_MAX_CHAN_SWITCH_TIME_IE_LEN         4

#define WLAN_MAX_SRP_IE_LEN                      21
#define WLAN_MAX_MUEDCA_IE_LEN                   14
#define WLAN_MAX_HE_6G_CAP_IE_LEN                3
#define WLAN_MAX_HEOP_IE_LEN                     16
#define WLAN_HEOP_OUI_TYPE                       "\x24"
#define WLAN_HEOP_OUI_SIZE                       1

/* HT capability flags */
#define WLAN_HTCAP_C_ADVCODING             0x0001
#define WLAN_HTCAP_C_CHWIDTH40             0x0002
/* Capable of SM Power Save (Static) */
#define WLAN_HTCAP_C_SMPOWERSAVE_STATIC    0x0000
/* Capable of SM Power Save (Dynamic) */
#define WLAN_HTCAP_C_SMPOWERSAVE_DYNAMIC   0x0004
/* Reserved */
#define WLAN_HTCAP_C_SM_RESERVED           0x0008
/* SM enabled, no SM Power Save */
#define WLAN_HTCAP_C_SMPOWERSAVE_DISABLED            0x000c
#define WLAN_HTCAP_C_GREENFIELD            0x0010
#define WLAN_HTCAP_C_SHORTGI20             0x0020
#define WLAN_HTCAP_C_SHORTGI40             0x0040
#define WLAN_HTCAP_C_TXSTBC                0x0080
#define WLAN_HTCAP_C_TXSTBC_S                   7
/* 2 bits */
#define WLAN_HTCAP_C_RXSTBC                0x0300
#define WLAN_HTCAP_C_RXSTBC_S                   8
#define WLAN_HTCAP_C_DELAYEDBLKACK         0x0400
/* 1 = 8K, 0 = 3839B */
#define WLAN_HTCAP_C_MAXAMSDUSIZE          0x0800
#define WLAN_HTCAP_C_DSSSCCK40             0x1000
#define WLAN_HTCAP_C_PSMP                  0x2000
#define WLAN_HTCAP_C_INTOLERANT40          0x4000
#define WLAN_HTCAP_C_LSIGTXOPPROT          0x8000
/* Spatial Multiplexing (SM) capabitlity bitmask */
#define WLAN_HTCAP_C_SM_MASK               0x000c

/* VHT Operation  */
/* 20/40 MHz Operating Channel */
#define WLAN_VHTOP_CHWIDTH_2040          0
/* 80 MHz Operating Channel */
#define WLAN_VHTOP_CHWIDTH_80            1
/* 160 MHz Operating Channel */
#define WLAN_VHTOP_CHWIDTH_160           2
/* 80 + 80 MHz Operating Channel */
#define WLAN_VHTOP_CHWIDTH_80_80         3
/* 160 MHz Operating Channel  (revised signalling) */
#define WLAN_VHTOP_CHWIDTH_REVSIG_160    1
/* 80 + 80 MHz Operating Channel  (revised signalling) */
#define WLAN_VHTOP_CHWIDTH_REVSIG_80_80  1

#define WLAN_HEOP_FIXED_PARAM_LENGTH       7
#define WLAN_HEOP_VHTOP_LENGTH             3
#define WLAN_HEOP_CO_LOCATED_BSS_LENGTH    1

#define WLAN_HEOP_VHTOP_PRESENT_MASK       0x00004000  /* B14 */
#define WLAN_HEOP_CO_LOCATED_BSS_MASK      0x00008000  /* B15 */
#define WLAN_HEOP_6GHZ_INFO_PRESENT_MASK   0X00020000  /* B17 */

#define WLAN_HE_6GHZ_CHWIDTH_20           0 /* 20MHz Oper Ch width */
#define WLAN_HE_6GHZ_CHWIDTH_40           1 /* 40MHz Oper Ch width */
#define WLAN_HE_6GHZ_CHWIDTH_80           2 /* 80MHz Oper Ch width */
#define WLAN_HE_6GHZ_CHWIDTH_160_80_80    3 /* 160/80+80 MHz Oper Ch width */

#define WLAN_HE_NON_SRG_PD_SR_DISALLOWED 0x02
#define WLAN_HE_NON_SRG_OFFSET_PRESENT 0x04
#define WLAN_HE_SIGA_SR_VAL15_ALLOWED  0x10

#ifdef WLAN_FEATURE_11BE
#define WLAN_EHT_CHWIDTH_20           0 /* 20MHz Oper Ch width */
#define WLAN_EHT_CHWIDTH_40           1 /* 40MHz Oper Ch width */
#define WLAN_EHT_CHWIDTH_80           2 /* 80MHz Oper Ch width */
#define WLAN_EHT_CHWIDTH_160          3 /* 160MHz Oper Ch width */
#define WLAN_EHT_CHWIDTH_320          4 /* 320MHz Oper Ch width */
#endif

/* Max length of 802.11 subelement */
#define WLAN_MAX_SUBELEM_LEN          255

#define WLAN_RATE_VAL              0x7f
#define WLAN_BASIC_RATE_MASK       0x80

#define WLAN_RV(v)     ((v) & WLAN_RATE_VAL)

#define WLAN_BSS_MEMBERSHIP_SELECTOR_HT_PHY       127
#define WLAN_BSS_MEMBERSHIP_SELECTOR_VHT_PHY      126
#define WLAN_BSS_MEMBERSHIP_SELECTOR_GLK          125
#define WLAN_BSS_MEMBERSHIP_SELECTOR_EPD          124
#define WLAN_BSS_MEMBERSHIP_SELECTOR_SAE_H2E      123
#define WLAN_BSS_MEMBERSHIP_SELECTOR_HE_PHY       122

/* EXT cap bit definitions based on IEEE 802.11az D4.0 - 9.4.2.26 */
#define WLAN_EXT_CAPA11_NTB_RANGING_RESPONDER          BIT(2)
#define WLAN_EXT_CAPA11_TB_RANGING_RESPONDER           BIT(3)
#define WLAN_EXT_CAPA11_PASSIVE_TB_RANGING_RESPONDER   BIT(4)
#define WLAN_EXT_CAPA11_PASSIVE_TB_RANGING_INITIATOR   BIT(5)

#define WLAN_CHAN_IS_5GHZ(chanidx) \
	((chanidx > 30) ? true : false)
#define WLAN_CHAN_IS_2GHZ(chanidx) \
	(((chanidx > 0) && (chanidx < 15)) ? true : false)

/* Check if revised signalling is being used for VHT160 in vhtop */
#define WLAN_IS_REVSIG_VHT160(vhtop) (((vhtop)->vht_op_chwidth == \
	WLAN_VHTOP_CHWIDTH_REVSIG_160) && \
	((vhtop)->vht_op_ch_freq_seg2 != 0) && \
	(abs((vhtop)->vht_op_ch_freq_seg2 - (vhtop)->vht_op_ch_freq_seg1) == 8))

/* Check if revised signalling is being used for VHT80p80 in vhtop */
#define WLAN_IS_REVSIG_VHT80_80(vhtop) (((vhtop)->vht_op_chwidth == \
	WLAN_VHTOP_CHWIDTH_REVSIG_80_80) && \
	((vhtop)->vht_op_ch_freq_seg2 != 0) && \
	(abs((vhtop)->vht_op_ch_freq_seg2 - (vhtop)->vht_op_ch_freq_seg1) > 8))

/* Check if channel width is HE160 in HE 6ghz params */
#define WLAN_IS_HE160(he_6g_param) (((he_6g_param)->width == \
	WLAN_HE_6GHZ_CHWIDTH_160_80_80) && \
	((he_6g_param)->chan_freq_seg1 != 0) && \
	(abs((he_6g_param)->chan_freq_seg1 - \
	(he_6g_param)->chan_freq_seg0) == 8))

/* Check if channel width is HE80p80 in HE 6ghz params */
#define WLAN_IS_HE80_80(he_6g_param) (((he_6g_param)->width == \
	WLAN_HE_6GHZ_CHWIDTH_160_80_80) && \
	((he_6g_param)->chan_freq_seg1 != 0) && \
	(abs((he_6g_param)->chan_freq_seg1 - \
	(he_6g_param)->chan_freq_seg0) > 8))

#define LE_READ_2(p) \
	((uint16_t)\
	((((const uint8_t *)(p))[0]) |\
	(((const uint8_t *)(p))[1] <<  8)))

#define LE_READ_4(p) \
	((uint32_t)\
	((((const uint8_t *)(p))[0]) |\
	(((const uint8_t *)(p))[1] <<  8) |  \
	(((const uint8_t *)(p))[2] << 16) |\
	(((const uint8_t *)(p))[3] << 24)))

#define BE_READ_4(p) \
	((uint32_t)\
	((((const uint8_t *)(p))[0] << 24) |\
	(((const uint8_t *)(p))[1] << 16) |\
	(((const uint8_t *)(p))[2] <<  8) |\
	(((const uint8_t *)(p))[3])))

/**
 * enum ext_chan_offset: extension channel offset
 * @WLAN_HTINFO_EXTOFFSET_NA: no extension channel is present
 * @WLAN_HTINFO_EXTOFFSET_ABOVE: above control channel
 * @WLAN_HTINFO_EXTOFFSET_UNDEF: undefined
 * @WLAN_HTINFO_EXTOFFSET_BELOW: below control channel
 */
enum ext_chan_offset {
	WLAN_HTINFO_EXTOFFSET_NA    = 0,
	WLAN_HTINFO_EXTOFFSET_ABOVE = 1,
	WLAN_HTINFO_EXTOFFSET_UNDEF = 2,
	WLAN_HTINFO_EXTOFFSET_BELOW = 3
};

/**
 * enum element_ie :- Management information element
 * @WLAN_ELEMID_SSID: ssid IE
 * @WLAN_ELEMID_RATES: Rates IE
 * @WLAN_ELEMID_FHPARMS: FH param IE
 * @WLAN_ELEMID_DSPARMS: DS Param IE
 * @WLAN_ELEMID_CFPARMS : CF Param IE
 * @WLAN_ELEMID_TIM: TIM IE
 * @WLAN_ELEMID_IBSSPARMS: Ibss params IE
 * @WLAN_ELEMID_COUNTRY: Country code IE
 * @WLAN_ELEMID_REQINFO: Req Info IE
 * @WLAN_ELEMID_QBSS_LOAD: Qbss load IE
 * @WLAN_ELEMID_TCLAS: TCLAS IE
 * @WLAN_ELEMID_CHALLENGE: Challenge IE
 * @WLAN_ELEMID_PWRCNSTR: Power cn IE
 * @WLAN_ELEMID_PWRCAP: power cap IE
 * @WLAN_ELEMID_TPCREQ: TPC req IE
 * @WLAN_ELEMID_TPCREP: TPC rsp IE
 * @WLAN_ELEMID_SUPPCHAN: Supported channel IE
 * @WLAN_ELEMID_CHANSWITCHANN: Channel switch IE
 * @WLAN_ELEMID_MEASREQ: Measurement request IE
 * @WLAN_ELEMID_MEASREP: Measurement Resp IE
 * @WLAN_ELEMID_QUIET: Quiet IE
 * @WLAN_ELEMID_IBSSDFS: IBSS DFS IE
 * @WLAN_ELEMID_ERP: ERP IE
 * @WLAN_ELEMID_TCLAS_PROCESS: TCLAS process IE
 * @WLAN_ELEMID_HTCAP_ANA: HTT Capability IE
 * @WLAN_ELEMID_RSN: RSN IE
 * @WLAN_ELEMID_XRATES: Extended rate IE
 * @WLAN_ELEMID_HTCAP_VENDOR: HT cap vendor IE
 * @WLAN_ELEMID_HTINFO_VENDOR: HT info vendor IE
 * @WLAN_ELEMID_MOBILITY_DOMAIN: MD IE
 * @WLAN_ELEMID_FT: FT IE
 * @WLAN_ELEMID_TIMEOUT_INTERVAL: Timeout interval IE
 * @WLAN_ELEMID_SUPP_OP_CLASS: OP class IE
 * @WLAN_ELEMID_EXTCHANSWITCHANN: Extended Channel switch IE
 * @WLAN_ELEMID_HTINFO_ANA: HT info IE
 * @WLAN_ELEMID_SECCHANOFFSET: Sec channel Offset IE
 * @WLAN_ELEMID_WAPI: WAPI IE
 * @WLAN_ELEMID_TIME_ADVERTISEMENT: Time IE
 * @WLAN_ELEMID_RRM: Radio resource measurement IE
 * @WLAN_ELEMID_MULTIPLE_BSSID: Multiple BSSID IE
 * @WLAN_ELEMID_2040_COEXT: 20-40 COext ext IE
 * @WLAN_ELEMID_2040_INTOL:20-40 INT OL IE
 * @WLAN_ELEMID_OBSS_SCAN: OBSS scan IE
 * @WLAN_ELEMID_MMIE: 802.11w Management MIC IE
 * @WLAN_ELEMID_NONTX_BSSID_CAP: Nontransmitted BSSID Capability IE
 * @WLAN_ELEMID_MULTI_BSSID_IDX: Multiple BSSID index
 * @WLAN_ELEMID_FMS_DESCRIPTOR: 802.11v FMS descriptor IE
 * @WLAN_ELEMID_FMS_REQUEST: 802.11v FMS request IE
 * @WLAN_ELEMID_FMS_RESPONSE: 802.11v FMS response IE
 * @WLAN_ELEMID_BSSMAX_IDLE_PERIOD = 90: BSS MAX IDLE PERIOD
 * @WLAN_ELEMID_TFS_REQUEST: TFS req IE
 * @WLAN_ELEMID_TFS_RESPONSE: TFS resp IE
 * @WLAN_ELEMID_TIM_BCAST_REQUEST: TIM bcast req IE
 * @WLAN_ELEMID_TIM_BCAST_RESPONSE: TIM bcast resp IE
 * @WLAN_ELEMID_INTERWORKING: Interworking IE
 * @WLAN_ELEMID_QOS_MAP: QOS MAP IE
 * @WLAN_ELEMID_XCAPS: Extended capability IE
 * @WLAN_ELEMID_TPC: TPC IE
 * @WLAN_ELEMID_CCKM: CCKM IE
 * @WLAN_ELEMID_VHTCAP: VHT Capabilities
 * @WLAN_ELEMID_VHTOP: VHT Operation
 * @WLAN_ELEMID_EXT_BSS_LOAD: Extended BSS Load
 * @WLAN_ELEMID_WIDE_BAND_CHAN_SWITCH: Wide Band Channel Switch
 * @WLAN_ELEMID_VHT_TX_PWR_ENVLP: VHT Transmit Power Envelope
 * @WLAN_ELEMID_CHAN_SWITCH_WRAP: Channel Switch Wrapper
 * @WLAN_ELEMID_AID: AID
 * @WLAN_ELEMID_QUIET_CHANNEL: Quiet Channel
 * @WLAN_ELEMID_OP_MODE_NOTIFY: Operating Mode Notification
 * @WLAN_ELEMID_TWT: Target wake time IE
 * @WLAN_ELEMID_VENDOR: vendor private
 * @WLAN_ELEMID_FRAGMENT: Fragment
 * @WLAN_ELEMID_EXTN_ELEM: extended IE
 */
enum element_ie {
	WLAN_ELEMID_SSID             = 0,
	WLAN_ELEMID_RATES            = 1,
	WLAN_ELEMID_FHPARMS          = 2,
	WLAN_ELEMID_DSPARMS          = 3,
	WLAN_ELEMID_CFPARMS          = 4,
	WLAN_ELEMID_TIM              = 5,
	WLAN_ELEMID_IBSSPARMS        = 6,
	WLAN_ELEMID_COUNTRY          = 7,
	/* 8-9 reserved */
	WLAN_ELEMID_REQINFO          = 10,
	WLAN_ELEMID_QBSS_LOAD        = 11,
	WLAN_ELEMID_EDCAPARMS        = 12,
	WLAN_ELEMID_TCLAS            = 14,
	WLAN_ELEMID_CHALLENGE        = 16,
	/* 17-31 reserved for challenge text extension */
	WLAN_ELEMID_PWRCNSTR         = 32,
	WLAN_ELEMID_PWRCAP           = 33,
	WLAN_ELEMID_TPCREQ           = 34,
	WLAN_ELEMID_TPCREP           = 35,
	WLAN_ELEMID_SUPPCHAN         = 36,
	WLAN_ELEMID_CHANSWITCHANN    = 37,
	WLAN_ELEMID_MEASREQ          = 38,
	WLAN_ELEMID_MEASREP          = 39,
	WLAN_ELEMID_QUIET            = 40,
	WLAN_ELEMID_IBSSDFS          = 41,
	WLAN_ELEMID_ERP              = 42,
	WLAN_ELEMID_TCLAS_PROCESS    = 44,
	WLAN_ELEMID_HTCAP_ANA        = 45,
	WLAN_ELEMID_QOS_CAPABILITY   = 46,
	WLAN_ELEMID_RSN              = 48,
	WLAN_ELEMID_XRATES           = 50,
	WLAN_ELEMID_HTCAP_VENDOR     = 51,
	WLAN_ELEMID_HTINFO_VENDOR    = 52,
	WLAN_ELEMID_MOBILITY_DOMAIN  = 54,
	WLAN_ELEMID_FT               = 55,
	WLAN_ELEMID_TIMEOUT_INTERVAL = 56,
	WLAN_ELEMID_SUPP_OP_CLASS    = 59,
	WLAN_ELEMID_EXTCHANSWITCHANN = 60,
	WLAN_ELEMID_HTINFO_ANA       = 61,
	WLAN_ELEMID_SECCHANOFFSET    = 62,
	WLAN_ELEMID_WAPI             = 68,
	WLAN_ELEMID_TIME_ADVERTISEMENT = 69,
	WLAN_ELEMID_RRM              = 70,
	WLAN_ELEMID_MULTIPLE_BSSID   = 71,
	WLAN_ELEMID_2040_COEXT       = 72,
	WLAN_ELEMID_2040_INTOL       = 73,
	WLAN_ELEMID_OBSS_SCAN        = 74,
	WLAN_ELEMID_MMIE             = 76,
	WLAN_ELEMID_NONTX_BSSID_CAP  = 83,
	WLAN_ELEMID_MULTI_BSSID_IDX  = 85,
	WLAN_ELEMID_FMS_DESCRIPTOR   = 86,
	WLAN_ELEMID_FMS_REQUEST      = 87,
	WLAN_ELEMID_FMS_RESPONSE     = 88,
	WLAN_ELEMID_BSSMAX_IDLE_PERIOD = 90,
	WLAN_ELEMID_TFS_REQUEST      = 91,
	WLAN_ELEMID_TFS_RESPONSE     = 92,
	WLAN_ELEMID_TIM_BCAST_REQUEST  = 94,
	WLAN_ELEMID_TIM_BCAST_RESPONSE = 95,
	WLAN_ELEMID_INTERWORKING     = 107,
	WLAN_ELEMID_QOS_MAP          = 110,
	WLAN_ELEMID_XCAPS            = 127,
	WLAN_ELEMID_TPC              = 150,
	WLAN_ELEMID_CCKM             = 156,
	WLAN_ELEMID_VHTCAP           = 191,
	WLAN_ELEMID_VHTOP            = 192,
	WLAN_ELEMID_EXT_BSS_LOAD     = 193,
	WLAN_ELEMID_WIDE_BAND_CHAN_SWITCH = 194,
	WLAN_ELEMID_VHT_TX_PWR_ENVLP = 195,
	WLAN_ELEMID_CHAN_SWITCH_WRAP = 196,
	WLAN_ELEMID_AID              = 197,
	WLAN_ELEMID_QUIET_CHANNEL    = 198,
	WLAN_ELEMID_OP_MODE_NOTIFY   = 199,
	WLAN_ELEMID_REDUCED_NEIGHBOR_REPORT = 201,
	WLAN_ELEMID_TWT              = 216,
	WLAN_ELEMID_VENDOR           = 221,
	WLAN_ELEMID_FILS_INDICATION  = 240,
	WLAN_ELEMID_FRAGMENT         = 242,
	WLAN_ELEMID_RSNXE            = 244,
	WLAN_ELEMID_EXTN_ELEM        = 255,
};

/**
 * enum extn_element_ie :- extended management information element
 * @WLAN_EXTN_ELEMID_HECAP:  HE capabilities IE
 * @WLAN_EXTN_ELEMID_HEOP:   HE Operation IE
 * @WLAN_EXTN_ELEMID_UORA: UL OFDMA-based random access Parameter Set element
 * @WLAN_EXTN_ELEMID_MUEDCA: MU-EDCA IE
 * @WLAN_EXTN_ELEMID_HE_6G_CAP: HE 6GHz Band Capabilities IE
 * @WLAN_EXTN_ELEMID_SRP:    spatial reuse parameter IE
 * @WLAN_EXTN_ELEMID_BSS_COLOR_CHANGE_ANNOUNCE: BSS Color Change Announcement IE
 * @WLAN_EXTN_ELEMID_MAX_CHAN_SWITCH_TIME: Maximum Channel Switch Time IE
 * @WLAN_EXTN_ELEMID_NONINHERITANCE: Non inheritance IE
 * @WLAN_EXTN_ELEMID_EHTOP: EHT Operation IE
 * @WLAN_EXTN_ELEMID_MULTI_LINK: Multi-Link IE
 * @WLAN_EXTN_ELEMID_EHTCAP: EHT Capabilities IE
 * @WLAN_EXTN_ELEMID_T2LM: TID-to-link mapping IE
 * @WLAN_EXTN_ELEMID_MULTI_LINK_TRAFFIC_IND: Multi-link Traffic Indication IE
 * @WLAN_EXTN_ELEMID_BW_IND: Bandwidth Indication Element Sub IE
 */
enum extn_element_ie {
	WLAN_EXTN_ELEMID_HECAP       = 35,
	WLAN_EXTN_ELEMID_HEOP        = 36,
	WLAN_EXTN_ELEMID_UORA        = 37,
	WLAN_EXTN_ELEMID_MUEDCA      = 38,
	WLAN_EXTN_ELEMID_SRP         = 39,
	WLAN_EXTN_ELEMID_BSS_COLOR_CHANGE_ANNOUNCE = 42,
	WLAN_EXTN_ELEMID_MAX_CHAN_SWITCH_TIME = 52,
	WLAN_EXTN_ELEMID_NONINHERITANCE = 56,
	WLAN_EXTN_ELEMID_HE_6G_CAP   = 59,
	WLAN_EXTN_ELEMID_ESP         = 11,
#ifdef WLAN_FEATURE_11BE
	WLAN_EXTN_ELEMID_EHTOP       = 106,
#endif
#ifdef WLAN_FEATURE_11BE_MLO
	WLAN_EXTN_ELEMID_MULTI_LINK  = 107,
#endif
#ifdef WLAN_FEATURE_11BE
	WLAN_EXTN_ELEMID_EHTCAP      = 108,
#endif
	WLAN_EXTN_ELEMID_T2LM        = 109,
	WLAN_EXTN_ELEMID_MULTI_LINK_TRAFFIC_IND = 110,
#ifdef WLAN_FEATURE_11BE
	WLAN_EXTN_ELEMID_BW_IND = 135,
#endif
};

/**
 * enum wlan_reason_code - wlan reason codes Reason codes
 * (IEEE Std 802.11-2016, 9.4.1.7, Table 9-45)
 * @REASON_UNSPEC_FAILURE: Unspecified reason
 * @REASON_PREV_AUTH_NOT_VALID: Previous authentication no longer valid
 * @REASON_DEAUTH_NETWORK_LEAVING: Deauthenticated because sending station
 * is leaving (or has left) IBSS or ESS
 * @REASON_DISASSOC_DUE_TO_INACTIVITY: Disassociated due to inactivity
 * @REASON_DISASSOC_AP_BUSY: Disassociated because AP is unable
 * to handle all currently associated STAs
 * @REASON_CLASS2_FRAME_FROM_NON_AUTH_STA: Class 2 frame received from
 * nonauthenticated station
 * @REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA: Class 3 frame received from
 * nonassociated station
 * @REASON_DISASSOC_NETWORK_LEAVING: Disassociated because sending station
 * is leaving (or has left) BSS
 * @REASON_STA_NOT_AUTHENTICATED: Station requesting (re)association
 * is not authenticated with responding station
 * @REASON_BAD_PWR_CAPABILITY: Disassociated because the
 * information in the Power Capability element is unacceptable
 * @REASON_BAD_SUPPORTED_CHANNELS: Disassociated because the
 * information in the Supported Channels element is unacceptable
 * @REASON_DISASSOC_BSS_TRANSITION: Disassociated due to BSS transition
 * management
 * @REASON_INVALID_IE: Invalid element, i.e., an element defined in this
 * standard for which the content does not meet the specifications in Clause 9
 * @REASON_MIC_FAILURE: Message integrity code (MIC) failure
 * @REASON_4WAY_HANDSHAKE_TIMEOUT: 4-Way Handshake timeout
 * @REASON_GROUP_KEY_UPDATE_TIMEOUT: Group Key Handshake timeout
 * @REASON_IN_4WAY_DIFFERS: Information element in 4-Way Handshake
 * different from (Re)Association Request/Probe Response/Beacon frame
 * @REASON_INVALID_GROUP_CIPHER: Invalid group cipher
 * @REASON_INVALID_PAIRWISE_CIPHER: Invalid pairwise cipher
 * @REASON_INVALID_AKMP: Invalid AKMP
 * @REASON_UNSUPPORTED_RSNE_VER: Unsupported RSNE version
 * @REASON_INVALID_RSNE_CAPABILITIES: Invalid RSNE capabilities
 * @REASON_1X_AUTH_FAILURE: IEEE 802.1X authentication failed
 * @REASON_CIPHER_SUITE_REJECTED: Cipher suite rejected because of the
 * security policy
 * @REASON_TDLS_PEER_UNREACHABLE: TDLS direct-link teardown due to TDLS
 * peer STA unreachable via the TDLS direct link
 * @REASON_TDLS_UNSPEC: TDLS direct-link teardown for unspecified
 * reason
 * @REASON_DISASSOC_SSP_REQUESTED: Disassociated because session terminated
 * by SSP request
 * @REASON_NO_SSP_ROAMING_AGREEMENT: Disassociated because of lack of SSP
 * roaming agreement
 * REASON_BAD_CIPHER_OR_AKM: Requested service rejected because of SSP
 * cipher suite or AKM requirement
 * @REASON_LOCATION_NOT_AUTHORIZED: Requested service not authorized in
 * this location
 * @REASON_SERVICE_CHANGE_PRECLUDES_TS: TS deleted because QoS AP
 * lacks sufficient bandwidth for this QoS STA due to a change in BSS service
 * characteristics or operational mode (e.g., an HT BSS change from 40 MHz
 * channel to 20 MHz channel)
 * @REASON_QOS_UNSPECIFIED: Disassociated for unspecified, QoS-related
 * reason
 * @REASON_NO_BANDWIDTH: Disassociated because QoS AP lacks sufficient
 * bandwidth for this QoS STA
 * @REASON_XS_UNACKED_FRAMES: Disassociated because excessive number of
 * frames need to be acknowledged, but are not acknowledged due to AP
 * transmissions and/or poor channel conditions
 * @REASON_EXCEEDED_TXOP: Disassociated because STA is transmitting outside
 * the limits of its TXOPs
 * @REASON_STA_LEAVING: Requested from peer STA as the STA is leaving the
 * BSS (or resetting)
 * @REASON_END_TS_BA_DLS: Requesting STA is no longer using the stream
 * or session
 * @REASON_UNKNOWN_TS_BA: Requesting STA received frames using a
 * mechanism for which setup has not been completed
 * @REASON_TIMEDOUT:  Requested from peer STA due to timeout
 * @REASON_PEERKEY_MISMATCH: Peer STA does not support the requested
 * cipher suite
 * @REASON_AUTHORIZED_ACCESS_LIMIT_REACHED: Disassociated because
 * authorized access limit reached
 * @REASON_EXTERNAL_SERVICE_REQUIREMENTS: Disassociated due to external
 * service requirements
 * @REASON_INVALID_FT_ACTION_FRAME_COUNT: Invalid FT Action frame count
 * @REASON_INVALID_PMKID: Invalid pairwise master key identifier (PMKID)
 * @REASON_INVALID_MDE: Invalid MDE
 * @REASON_INVALID_FTE: Invalid FTE
 * @REASON_MESH_PEERING_CANCELLED: Mesh peering canceled for unknown
 * reasons
 * @REASON_MESH_MAX_PEERS: The mesh STA has reached the supported maximum
 * number of peer mesh STAs
 * @REASON_MESH_CONFIG_POLICY_VIOLATION: The received information violates
 * the Mesh Configuration policy configured in the mesh STA profile
 * @REASON_MESH_CLOSE_RCVD: The mesh STA has received a Mesh Peering Close
 * frame requesting to close the mesh peering
 * @REASON_MESH_MAX_RETRIES: The mesh STA has resent dot11MeshMaxRetries
 * Mesh Peering Open frames, without receiving a Mesh Peering Confirm frame
 * @REASON_MESH_CONFIRM_TIMEOUT: The confirmTimer for the mesh peering
 * instance times out.
 * @REASON_MESH_INVALID_GTK: The mesh STA fails to unwrap the GTK or
 * the values in the wrapped contents do not match
 * @REASON_MESH_INCONSISTENT_PARAMS: The mesh STA receives inconsistent
 * information about the mesh parameters between mesh peering Management frames
 * @REASON_MESH_INVALID_SECURITY_CAP: The mesh STA fails the authenticated
 * mesh peering exchange because due to failure in selecting either the pairwise
 * ciphersuite or group ciphersuite
 * @REASON_MESH_PATH_ERROR_NO_PROXY_INFO: The mesh STA does not have proxy
 * information for this external destination.
 * @REASON_MESH_PATH_ERROR_NO_FORWARDING_INFO: The mesh STA does not have
 * forwarding information for this destination.
 * @REASON_MESH_PATH_ERROR_DEST_UNREACHABLE: The mesh STA determines that
 * the link to the next hop of an active path in its forwarding information is
 * no longer usable.
 * @REASON_MAC_ADDRESS_ALREADY_EXISTS_IN_MBSS: The Deauthentication frame
 * was sent because the MAC address of the STA already exists in the mesh BSS
 * @REASON_MESH_CHANNEL_SWITCH_REGULATORY_REQ: The mesh STA performs
 * channel switch to meet regulatory requirements.
 * @REASON_MESH_CHANNEL_SWITCH_UNSPECIFIED: The mesh STA performs channel
 * switching with unspecified reason.
 * @REASON_POOR_RSSI_CONDITIONS: Disassociated due to poor RSSI conditions
 *
 *
 * Internal reason codes: Add any internal reason code just after
 * REASON_PROP_START and decrease the value of REASON_PROP_START
 * accordingly.
 *
 * @REASON_PROP_START: Start of prop reason code
 * @REASON_HOST_TRIGGERED_LINK_DELETE: Dynamic link removal
 * @REASON_OCI_MISMATCH: Reason OCI Mismatch happens
 * @REASON_HOST_TRIGGERED_ROAM_FAILURE: Reason host triggered roam failed
 * @REASON_FW_TRIGGERED_ROAM_FAILURE: Firmware triggered roam failed
 * @REASON_GATEWAY_REACHABILITY_FAILURE: Due to NUD failure
 * @REASON_UNSUPPORTED_CHANNEL_CSA: due to unsuppoerted channel in CSA
 * @REASON_OPER_CHANNEL_DISABLED_INDOOR: as channel is disabled in indoor
 * @REASON_OPER_CHANNEL_USER_DISABLED: due to channel disabled by user
 * @REASON_DEVICE_RECOVERY: due to SSR
 * @REASON_KEY_TIMEOUT: due to key Timeout
 * @REASON_OPER_CHANNEL_BAND_CHANGE: due to change in BAND
 * @REASON_IFACE_DOWN: as interface is going down
 * @REASON_PEER_XRETRY_FAIL: due to sta kickout with reason no ACK
 * @REASON_PEER_INACTIVITY: due to sta kickout with reason inactivity
 * @REASON_SA_QUERY_TIMEOUT: due to sta kickout due to SA query timeout
 * @REASON_CHANNEL_SWITCH_FAILED: as channel switch failed
 * @REASON_BEACON_MISSED: due to beacon miss
 * @REASON_USER_TRIGGERED_ROAM_FAILURE: Reason user triggered roam failed
 * @REASON_HOST_TRIGGERED_SILENT_DEAUTH: deauth without sending deauth frame
 */
enum wlan_reason_code {
	REASON_UNSPEC_FAILURE = 1,
	REASON_PREV_AUTH_NOT_VALID = 2,
	REASON_DEAUTH_NETWORK_LEAVING = 3,
	REASON_DISASSOC_DUE_TO_INACTIVITY = 4,
	REASON_DISASSOC_AP_BUSY = 5,
	REASON_CLASS2_FRAME_FROM_NON_AUTH_STA = 6,
	REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA = 7,
	REASON_DISASSOC_NETWORK_LEAVING = 8,
	REASON_STA_NOT_AUTHENTICATED = 9,
	REASON_BAD_PWR_CAPABILITY = 10,
	REASON_BAD_SUPPORTED_CHANNELS = 11,
	REASON_DISASSOC_BSS_TRANSITION = 12,
	REASON_INVALID_IE = 13,
	REASON_MIC_FAILURE = 14,
	REASON_4WAY_HANDSHAKE_TIMEOUT = 15,
	REASON_GROUP_KEY_UPDATE_TIMEOUT = 16,
	REASON_IN_4WAY_DIFFERS = 17,
	REASON_INVALID_GROUP_CIPHER = 18,
	REASON_INVALID_PAIRWISE_CIPHER = 19,
	REASON_INVALID_AKMP = 20,
	REASON_UNSUPPORTED_RSNE_VER = 21,
	REASON_INVALID_RSNE_CAPABILITIES = 22,
	REASON_1X_AUTH_FAILURE = 23,
	REASON_CIPHER_SUITE_REJECTED = 24,
	REASON_TDLS_PEER_UNREACHABLE = 25,
	REASON_TDLS_UNSPEC = 26,
	REASON_DISASSOC_SSP_REQUESTED = 27,
	REASON_NO_SSP_ROAMING_AGREEMENT = 28,
	REASON_BAD_CIPHER_OR_AKM = 29,
	REASON_LOCATION_NOT_AUTHORIZED = 30,
	REASON_SERVICE_CHANGE_PRECLUDES_TS = 31,
	REASON_QOS_UNSPECIFIED = 32,
	REASON_NO_BANDWIDTH = 33,
	REASON_XS_UNACKED_FRAMES = 34,
	REASON_EXCEEDED_TXOP = 35,
	REASON_STA_LEAVING = 36,
	REASON_END_TS_BA_DLS = 37,
	REASON_UNKNOWN_TS_BA = 38,
	REASON_TIMEDOUT = 39,
	REASON_PEERKEY_MISMATCH = 45,
	REASON_AUTHORIZED_ACCESS_LIMIT_REACHED = 46,
	REASON_EXTERNAL_SERVICE_REQUIREMENTS = 47,
	REASON_INVALID_FT_ACTION_FRAME_COUNT = 48,
	REASON_INVALID_PMKID = 49,
	REASON_INVALID_MDE = 50,
	REASON_INVALID_FTE = 51,
	REASON_MESH_PEERING_CANCELLED = 52,
	REASON_MESH_MAX_PEERS = 53,
	REASON_MESH_CONFIG_POLICY_VIOLATION = 54,
	REASON_MESH_CLOSE_RCVD = 55,
	REASON_MESH_MAX_RETRIES = 56,
	REASON_MESH_CONFIRM_TIMEOUT = 57,
	REASON_MESH_INVALID_GTK = 58,
	REASON_MESH_INCONSISTENT_PARAMS = 59,
	REASON_MESH_INVALID_SECURITY_CAP = 60,
	REASON_MESH_PATH_ERROR_NO_PROXY_INFO = 61,
	REASON_MESH_PATH_ERROR_NO_FORWARDING_INFO = 62,
	REASON_MESH_PATH_ERROR_DEST_UNREACHABLE = 63,
	REASON_MAC_ADDRESS_ALREADY_EXISTS_IN_MBSS = 64,
	REASON_MESH_CHANNEL_SWITCH_REGULATORY_REQ = 65,
	REASON_MESH_CHANNEL_SWITCH_UNSPECIFIED = 66,
	REASON_POOR_RSSI_CONDITIONS = 71,
	/* 72–65535 reserved */

	/* Internal reason codes */

	/*
	 * Internal reason codes: Add any internal reason code just after
	 * REASON_PROP_START and decrease the value of REASON_PROP_START
	 * accordingly.
	 */
	REASON_PROP_START = 65516,
	REASON_HOST_TRIGGERED_LINK_DELETE = 65517,
	REASON_OCI_MISMATCH = 65518,
	REASON_HOST_TRIGGERED_ROAM_FAILURE  = 65519,
	REASON_FW_TRIGGERED_ROAM_FAILURE = 65520,
	REASON_GATEWAY_REACHABILITY_FAILURE = 65521,
	REASON_UNSUPPORTED_CHANNEL_CSA = 65522,
	REASON_OPER_CHANNEL_DISABLED_INDOOR = 65523,
	REASON_OPER_CHANNEL_USER_DISABLED = 65524,
	REASON_DEVICE_RECOVERY = 65525,
	REASON_KEY_TIMEOUT = 65526,
	REASON_OPER_CHANNEL_BAND_CHANGE = 65527,
	REASON_IFACE_DOWN = 65528,
	REASON_PEER_XRETRY_FAIL = 65529,
	REASON_PEER_INACTIVITY = 65530,
	REASON_SA_QUERY_TIMEOUT = 65531,
	REASON_CHANNEL_SWITCH_FAILED = 65532,
	REASON_BEACON_MISSED = 65533,
	REASON_USER_TRIGGERED_ROAM_FAILURE = 65534,
	REASON_HOST_TRIGGERED_SILENT_DEAUTH = 65535,
};

/**
 * enum wlan_status_code - wlan status codes
 * (IEEE Std 802.11-2016, 9.4.1.9, Table 9-46)
 * @STATUS_SUCCESS: Success full
 * @STATUS_UNSPECIFIED_FAILURE: Unspecified failure.
 * @STATUS_TDLS_WAKEUP_REJECT: TDLS wakeup schedule rejected but alternative
 * schedule provided.
 * @STATUS_SECURITY_DISABLED: Security disabled.
 * @STATUS_UNACCEPTABLE_LIFETIME: Unacceptable lifetime.
 * @STATUS_NOT_IN_SAME_BSS: Not in same BSS.
 * @STATUS_CAPS_UNSUPPORTED: Cannot support all requested capabilities in the
 * Capability Information field.
 * @STATUS_REASSOC_NO_ASSOC: Reassociation denied due to inability to confirm
 * that association exists.
 * @STATUS_ASSOC_DENIED_UNSPEC: Association denied due to reason outside the
 * scope of this standard.
 * @STATUS_NOT_SUPPORTED_AUTH_ALG: Responding STA does not support the specified
 * authentication algorithm.
 * @STATUS_UNKNOWN_AUTH_TRANSACTION: Received an Authentication frame with
 * authentication transaction sequence number out of expected sequence.
 * @STATUS_CHALLENGE_FAIL: Authentication rejected because of challenge failure.
 * @STATUS_AUTH_TIMEOUT: Authentication rejected due to timeout waiting for next
 * frame in sequence.
 * @STATUS_AP_UNABLE_TO_HANDLE_NEW_STA: Association denied because AP is unable
 * to handle additional associated STAs.
 * @STATUS_ASSOC_DENIED_RATES: Association denied due to requesting STA not
 * supporting all of the data rates in the BSSBasicRateSet parameter,
 * the Basic HT-MCS Set field of the HT Operation parameter, or the Basic
 * VHT-MCS and NSS Set field in the VHT Operation parameter.
 * @STATUS_ASSOC_DENIED_NOSHORT: Association denied due to requesting
 * STA not supporting the short preamble option.
 * @STATUS_SPEC_MGMT_REQUIRED: Association request rejected because Spectrum
 * Management capability is required.
 * @STATUS_PWR_CAPABILITY_NOT_VALID: Association request rejected because the
 * information in the Power Capability element is unacceptable.
 * @STATUS_SUPPORTED_CHANNEL_NOT_VALID: Association request rejected because
 * the information in the Supported Channels element is unacceptable.
 * @STATUS_ASSOC_DENIED_NO_SHORT_SLOT_TIME: Association denied due to requesting
 * STA not supporting the Short Slot Time option.
 * @STATUS_ASSOC_DENIED_NO_HT: Association denied because the requesting STA
 * does not support HT features.
 * @STATUS_R0KH_UNREACHABLE: R0KH unreachable.
 * @STATUS_ASSOC_DENIED_NO_PCO: Association denied because the requesting STA
 * does not support the phased coexistence operation (PCO) transition time
 * required by the AP.
 * @STATUS_ASSOC_REJECTED_TEMPORARILY: Association request rejected temporarily,
 * try again later.
 * @STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION: Robust management frame policy
 * violation.
 * @STATUS_UNSPECIFIED_QOS_FAILURE: Unspecified, QoS-related failure.
 * @STATUS_DENIED_INSUFFICIENT_BANDWIDTH: Association denied because QoS AP or
 * PCP has insufficient bandwidth to handle another QoS STA.
 * @STATUS_DENIED_POOR_CHANNEL_CONDITIONS: Association denied due to excessive
 * frame loss rates and/or poor conditions on current operating channel.
 * @STATUS_DENIED_QOS_NOT_SUPPORTED: Association (with QoS BSS) denied because
 * the requesting STA does not support the QoS facility.
 * @STATUS_REQUEST_DECLINED: The request has been declined.
 * @STATUS_INVALID_PARAMETERS: The request has not been successful as one
 * or more parameters have invalid values.
 * @STATUS_REJECTED_WITH_SUGGESTED_CHANGES: The allocation or TS has not been
 * created because the request cannot be honored; however, a suggested TSPEC/DMG
 * TSPEC is provided so that the initiating STA can attempt to set another
 * allocation or TS with the suggested changes to the TSPEC/DMG TSPEC
 * @STATUS_INVALID_IE: Invalid element, i.e., an element defined in this
 * standard for which the content does not meet the specifications in Clause 9.
 * @STATUS_GROUP_CIPHER_NOT_VALID: Invalid group cipher.
 * @STATUS_PAIRWISE_CIPHER_NOT_VALID: Invalid pairwise cipher.
 * @STATUS_AKMP_NOT_VALID: Invalid AKMP.
 * @STATUS_UNSUPPORTED_RSN_IE_VERSION: Unsupported RSNE version.
 * @STATUS_INVALID_RSN_IE_CAPAB: Invalid RSNE capabilities.
 * @STATUS_CIPHER_REJECTED_PER_POLICY: Cipher suite rejected because of security
 * policy.
 * @STATUS_TS_NOT_CREATED: The TS or allocation has not been created; however,
 * the HC or PCP might be capable of creating a TS or allocation, in response to
 * a request, after the time indicated in the TS Delay element.
 * @STATUS_DIRECT_LINK_NOT_ALLOWED: Direct link is not allowed in the BSS by
 * policy.
 * @STATUS_DEST_STA_NOT_PRESENT: The Destination STA is not present within this
 * BSS.
 * @STATUS_DEST_STA_NOT_QOS_STA: The Destination STA is not a QoS STA.
 * @STATUS_ASSOC_DENIED_LISTEN_INT_TOO_LARGE: Association denied because the
 * listen interval is too large.
 * @STATUS_INVALID_FT_ACTION_FRAME_COUNT: Invalid FT Action frame count.
 * @STATUS_INVALID_PMKID: Invalid pairwise master key identifier (PMKID).
 *
 * Internal status codes: Add any internal status code just after
 * STATUS_PROP_START and decrease the value of STATUS_PROP_START
 * accordingly.
 *
 * @STATUS_PROP_START: Start of prop status codes.
 * @STATUS_NO_NETWORK_FOUND: No network found
 * @STATUS_AUTH_TX_FAIL: Failed to sent AUTH on air
 * @STATUS_AUTH_NO_ACK_RECEIVED: No ack received for Auth tx
 * @STATUS_AUTH_NO_RESP_RECEIVED: No Auth response for Auth tx
 * @STATUS_ASSOC_TX_FAIL: Failed to sent Assoc on air
 * @STATUS_ASSOC_NO_ACK_RECEIVED: No ack received for Assoc tx
 * @STATUS_ASSOC_NO_RESP_RECEIVED: No Assoc response for Assoc tx
 */
enum wlan_status_code {
	STATUS_SUCCESS = 0,
	STATUS_UNSPECIFIED_FAILURE = 1,
	STATUS_TDLS_WAKEUP_REJECT = 3,
	STATUS_SECURITY_DISABLED = 5,
	STATUS_UNACCEPTABLE_LIFETIME = 6,
	STATUS_NOT_IN_SAME_BSS = 7,
	STATUS_CAPS_UNSUPPORTED = 10,
	STATUS_REASSOC_NO_ASSOC = 11,
	STATUS_ASSOC_DENIED_UNSPEC = 12,
	STATUS_NOT_SUPPORTED_AUTH_ALG = 13,
	STATUS_UNKNOWN_AUTH_TRANSACTION = 14,
	STATUS_CHALLENGE_FAIL = 15,
	STATUS_AUTH_TIMEOUT = 16,
	STATUS_AP_UNABLE_TO_HANDLE_NEW_STA = 17,
	STATUS_ASSOC_DENIED_RATES = 18,
	STATUS_ASSOC_DENIED_NOSHORT = 19,
	STATUS_SPEC_MGMT_REQUIRED = 22,
	STATUS_PWR_CAPABILITY_NOT_VALID = 23,
	STATUS_SUPPORTED_CHANNEL_NOT_VALID = 24,
	STATUS_ASSOC_DENIED_NO_SHORT_SLOT_TIME = 25,
	STATUS_ASSOC_DENIED_NO_HT = 27,
	STATUS_R0KH_UNREACHABLE = 28,
	STATUS_ASSOC_DENIED_NO_PCO = 29,
	STATUS_ASSOC_REJECTED_TEMPORARILY = 30,
	STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION = 31,
	STATUS_UNSPECIFIED_QOS_FAILURE = 32,
	STATUS_DENIED_INSUFFICIENT_BANDWIDTH = 33,
	STATUS_DENIED_POOR_CHANNEL_CONDITIONS = 34,
	STATUS_DENIED_QOS_NOT_SUPPORTED = 35,
	STATUS_REQUEST_DECLINED = 37,
	STATUS_INVALID_PARAMETERS = 38,
	STATUS_REJECTED_WITH_SUGGESTED_CHANGES = 39,
	STATUS_INVALID_IE = 40,
	STATUS_GROUP_CIPHER_NOT_VALID = 41,
	STATUS_PAIRWISE_CIPHER_NOT_VALID = 42,
	STATUS_AKMP_NOT_VALID = 43,
	STATUS_UNSUPPORTED_RSN_IE_VERSION = 44,
	STATUS_INVALID_RSN_IE_CAPAB = 45,
	STATUS_CIPHER_REJECTED_PER_POLICY = 46,
	STATUS_TS_NOT_CREATED = 47,
	STATUS_DIRECT_LINK_NOT_ALLOWED = 48,
	STATUS_DEST_STA_NOT_PRESENT = 49,
	STATUS_DEST_STA_NOT_QOS_STA = 50,
	STATUS_ASSOC_DENIED_LISTEN_INT_TOO_LARGE = 51,
	STATUS_INVALID_FT_ACTION_FRAME_COUNT = 52,
	STATUS_INVALID_PMKID = 53,

	/* Error STATUS code for intenal usage*/
	STATUS_PROP_START = 65528,
	STATUS_NO_NETWORK_FOUND = 65528,
	STATUS_AUTH_TX_FAIL = 65529,
	STATUS_AUTH_NO_ACK_RECEIVED = 65530,
	STATUS_AUTH_NO_RESP_RECEIVED = 65531,
	STATUS_ASSOC_TX_FAIL = 65532,
	STATUS_ASSOC_NO_ACK_RECEIVED = 65533,
	STATUS_ASSOC_NO_RESP_RECEIVED = 65534,
};

#define WLAN_OUI_SIZE 4
#define WLAN_MAX_CIPHER 6
#define WLAN_RSN_SELECTOR_LEN 4
#define WLAN_WPA_SELECTOR_LEN 4
#define PMKID_LEN 16
#define MAX_PMK_LEN 64
#define MAX_PMKID 4
#define MAX_KEK_LENGTH 64
#define MAX_KCK_LEN 32
#define REPLAY_CTR_LEN 8
#define KCK_KEY_LEN 16
#define KEK_KEY_LEN 16
#define KCK_192BIT_KEY_LEN 24
#define KEK_256BIT_KEY_LEN 32

#define WLAN_MAX_SECURE_LTF_KEYSEED_LEN 48
#define WLAN_MIN_SECURE_LTF_KEYSEED_LEN 32

#define WLAN_WPA_OUI 0xf25000
#define WLAN_WPA_OUI_TYPE 0x01
#define WPA_VERSION 1
#define WLAN_WPA_SEL(x) (((x) << 24) | WLAN_WPA_OUI)

#define WLAN_RSN_OUI 0xac0f00
#define WLAN_CCKM_OUI 0x964000
#define WLAN_CCKM_ASE_UNSPEC 0
#define WLAN_WPA_CCKM_AKM 0x00964000
#define WLAN_RSN_CCKM_AKM 0x00964000
#define WLAN_RSN_DPP_AKM 0x029A6F50
#define WLAN_RSN_OSEN_AKM 0x019A6F50

#define RSN_VERSION 1
#define WLAN_RSN_SEL(x) (((x) << 24) | WLAN_RSN_OUI)
#define WLAN_CCKM_SEL(x) (((x) << 24) | WLAN_CCKM_OUI)

#define WLAN_CSE_NONE                    0x00
#define WLAN_CSE_WEP40                   0x01
#define WLAN_CSE_TKIP                    0x02
#define WLAN_CSE_RESERVED                0x03
#define WLAN_CSE_CCMP                    0x04
#define WLAN_CSE_WEP104                  0x05
#define WLAN_CSE_AES_CMAC                0x06
#define WLAN_CSE_GCMP_128                0x08
#define WLAN_CSE_GCMP_256                0x09
#define WLAN_CSE_CCMP_256                0x0A
#define WLAN_CSE_BIP_GMAC_128            0x0B
#define WLAN_CSE_BIP_GMAC_256            0x0C
#define WLAN_CSE_BIP_CMAC_256            0x0D

#define WLAN_AKM_IEEE8021X        0x01
#define WLAN_AKM_PSK              0x02
#define WLAN_AKM_FT_IEEE8021X     0x03
#define WLAN_AKM_FT_PSK           0x04
#define WLAN_AKM_SHA256_IEEE8021X 0x05
#define WLAN_AKM_SHA256_PSK       0x06
#define WLAN_AKM_SAE              0x08
#define WLAN_AKM_FT_SAE           0x09
#define WLAN_AKM_SUITEB_EAP_SHA256 0x0B
#define WLAN_AKM_SUITEB_EAP_SHA384 0x0C
#define WLAN_AKM_FT_SUITEB_EAP_SHA384 0x0D
#define WLAN_AKM_FILS_SHA256      0x0E
#define WLAN_AKM_FILS_SHA384      0x0F
#define WLAN_AKM_FILS_FT_SHA256   0x10
#define WLAN_AKM_FILS_FT_SHA384   0x11
#define WLAN_AKM_OWE              0x12
#define WLAN_AKM_SAE_EXT_KEY      0x18
#define WLAN_AKM_FT_SAE_EXT_KEY   0x19

#define WLAN_ASE_NONE                    0x00
#define WLAN_ASE_8021X_UNSPEC            0x01
#define WLAN_ASE_8021X_PSK               0x02
#define WLAN_ASE_FT_IEEE8021X            0x20
#define WLAN_ASE_FT_PSK                  0x40
#define WLAN_ASE_SHA256_IEEE8021X        0x80
#define WLAN_ASE_SHA256_PSK              0x100
#define WLAN_ASE_WPS                     0x200

#define RSN_CAP_MFP_CAPABLE 0x80
#define RSN_CAP_MFP_REQUIRED 0x40

/* FILS Discovery */
#define WLAN_ACTION_FILS_DISCOVERY         34
#define WLAN_FD_FRAMECNTL_CAP              0x0020
#define WLAN_FD_FRAMECNTL_SHORTSSID        0x0040
#define WLAN_FD_FRAMECNTL_CH_CENTERFREQ    0x0200
#define WLAN_FD_FRAMECNTL_LEN_PRES         0x1000
#define WLAN_FD_FRAMECNTL_SHORTSSID_LEN    0x0003

#define WLAN_FD_SSID_LEN_PRES(_v)          ((_v) & 0x1F)
#define WLAN_FD_IS_CAP_PRESENT(_v)         ((_v) & WLAN_FD_FRAMECNTL_CAP)
#define WLAN_FD_IS_LEN_PRESENT(_v)         ((_v) & WLAN_FD_FRAMECNTL_LEN_PRES)
#define WLAN_FD_IS_FRAMECNTL_CH_CENTERFREQ(_v)\
					((_v) & WLAN_FD_FRAMECNTL_CH_CENTERFREQ)

#define WLAN_FD_CAP_LEN                    2
#define WLAN_FD_CAP_ESS_S                  0
#define WLAN_FD_CAP_PRIVACY_S              1
#define WLAN_FD_CAP_BSS_CHWIDTH_S          2
#define WLAN_FD_CAP_NSS_S                  5
#define WLAN_FD_CAP_PHY_INDEX_S            2
#define WLAN_FD_CAP_MIN_RATE_S             5

#define WLAN_FD_CHWIDTH_20                 0
#define WLAN_FD_CHWIDTH_40                 1
#define WLAN_FD_CHWIDTH_80                 2
#define WLAN_FD_CHWIDTH_160_80_80          3
#define WLAN_FD_CHWIDTH_320                4

#define WLAN_FD_CAP_NSS_GTE_5              4
#define WLAN_FD_CAP_NSS_MODE_1             1
#define WLAN_FD_CAP_NSS_MODE_2             2
#define WLAN_FD_CAP_NSS_MODE_3             3
#define WLAN_FD_CAP_NSS_MODE_4             4
#define WLAN_FD_CAP_NSS_MODE_5             5
#define WLAN_FD_CAP_NSS_MODE_6             6
#define WLAN_FD_CAP_NSS_MODE_7             7
#define WLAN_FD_CAP_NSS_MODE_8             8

#define WLAN_FD_CAP_ESS_ENABLE             1

#define WLAN_FD_CAP_PHY_INDEX_NON_HT_OFDM  1
#define WLAN_FD_CAP_PHY_INDEX_HT           2
#define WLAN_FD_CAP_PHY_INDEX_VHT          3
#define WLAN_FD_CAP_PHY_INDEX_HE           4
#ifdef WLAN_FEATURE_11BE
#define WLAN_FD_CAP_PHY_INDEX_EHT          5
#endif /* WLAN_FEATURE_11BE */

#define WLAN_FD_CAP_MIN_RATE               0

/**
 * struct element_info - defines length of a memory block and memory block
 * @len: length of memory block
 * @ptr: memory block pointer
 */
struct element_info {
	uint32_t len;
	uint8_t *ptr;
};

/**
 * struct wlan_rsn_ie_hdr: rsn ie header
 * @elem_id: RSN element id WLAN_ELEMID_RSN.
 * @len: rsn ie length
 * @version: RSN ver
 */
struct wlan_rsn_ie_hdr {
	u8 elem_id;
	u8 len;
	u8 version[2];
};

#define WLAN_RSN_IE_MIN_LEN             2
#define WLAN_WAPI_IE_MIN_LEN            20

/**
 * struct wlan_wpa_ie_hdr: wpa ie header
 * @elem_id: Wpa element id, vendor specific.
 * @len: wpa ie length
 * @oui: 24-bit OUI followed by 8-bit OUI type
 * @version: wpa ver
 */
struct wlan_wpa_ie_hdr {
	u8 elem_id;
	u8 len;
	u8 oui[4];
	u8 version[2];
};

#define WAPI_VERSION 1
#define WLAN_WAPI_OUI 0x721400

#define WLAN_WAPI_SEL(x) (((x) << 24) | WLAN_WAPI_OUI)

#define WLAN_WAI_CERT_OR_SMS4 0x01
#define WLAN_WAI_PSK 0x02

/**
 * struct wlan_frame_hdr: generic IEEE 802.11 frames
 * @i_fc: frame control
 * @i_dur: duration field
 * @i_addr1: mac address 1
 * @i_addr2: mac address 2
 * @i_addr3: mac address 3
 * @i_seq: seq info
 */
struct wlan_frame_hdr {
	uint8_t i_fc[2];
	uint8_t i_dur[2];
	union {
		struct {
			uint8_t i_addr1[QDF_MAC_ADDR_SIZE];
			uint8_t i_addr2[QDF_MAC_ADDR_SIZE];
			uint8_t i_addr3[QDF_MAC_ADDR_SIZE];
		};
		uint8_t i_addr_all[3 * QDF_MAC_ADDR_SIZE];
	};
	uint8_t i_seq[2];
} qdf_packed;

struct wlan_frame_hdr_qos {
	uint8_t i_fc[2];
	uint8_t i_dur[2];
	union {
		struct {
			uint8_t i_addr1[QDF_MAC_ADDR_SIZE];
			uint8_t i_addr2[QDF_MAC_ADDR_SIZE];
			uint8_t i_addr3[QDF_MAC_ADDR_SIZE];
		};
		uint8_t i_addr_all[3 * QDF_MAC_ADDR_SIZE];
	};
	uint8_t i_seq[2];
	uint8_t i_qos[2];
} qdf_packed;

struct wlan_frame_hdr_qos_addr4 {
	uint8_t i_fc[2];
	uint8_t i_dur[2];
	union {
		struct {
			uint8_t i_addr1[QDF_MAC_ADDR_SIZE];
			uint8_t i_addr2[QDF_MAC_ADDR_SIZE];
			uint8_t i_addr3[QDF_MAC_ADDR_SIZE];
		};
		uint8_t i_addr_all[3 * QDF_MAC_ADDR_SIZE];
	};
	uint8_t i_seq[2];
	uint8_t i_addr4[QDF_MAC_ADDR_SIZE];
	uint8_t i_qos[2];
} qdf_packed;

/* sequence number offset base on begin of mac header */
#define WLAN_SEQ_CTL_OFFSET         22
#define WLAN_LOW_SEQ_NUM_MASK       0x000F
#define WLAN_HIGH_SEQ_NUM_MASK      0x0FF0
#define WLAN_HIGH_SEQ_NUM_OFFSET    4

/**
 * struct wlan_seq_ctl: sequence number control
 * @frag_num: frag number
 * @seq_num_lo: sequence number low byte
 * @seq_num_hi: sequence number high byte
 */
struct wlan_seq_ctl {
	uint8_t frag_num:4;
	uint8_t seq_num_lo:4;
	uint8_t seq_num_hi:8;
} qdf_packed;

/**
 * union wlan_capability : wlan_capability info
 * @value: capability value
 */
union wlan_capability {
	struct caps {
		uint16_t ess:1;
		uint16_t ibss:1;
		uint16_t cf_pollable:1;
		uint16_t cf_poll_request:1;
		uint16_t privacy:1;
		uint16_t short_preamble:1;
		uint16_t pbcc:1;
		uint16_t channel_agility:1;
		uint16_t spectrum_management:1;
		uint16_t qos:1;
		uint16_t short_slot_time:1;
		uint16_t apsd:1;
		uint16_t reserved2:1;
		uint16_t dsss_ofdm:1;
		uint16_t del_block_ack:1;
		uint16_t immed_block_ack:1;
	} wlan_caps;
	uint16_t value;
} qdf_packed;

/**
 * struct ie_header : IE header
 * @ie_id: Element Id
 * @ie_len: IE Length
 */
struct ie_header {
	uint8_t ie_id;
	uint8_t ie_len;
} qdf_packed;

/**
 * struct extn_ie_header : Extension IE header
 * @ie_id: Element Id
 * @ie_len: IE Length
 * @ie_extn_id: extension id
 */
struct extn_ie_header {
	uint8_t ie_id;
	uint8_t ie_len;
	uint8_t ie_extn_id;
} qdf_packed;


/**
 * struct ie_ssid : ssid IE
 * @ssid_id: SSID Element Id
 * @ssid_len: SSID IE Length
 * @ssid: ssid value
 */
struct ie_ssid {
	uint8_t ssid_id;
	uint8_t ssid_len;
	uint8_t ssid[WLAN_SSID_MAX_LEN];
} qdf_packed;

/**
 * struct ds_ie : ds IE
 * @ie: DS Element Id
 * @len: DS IE Length
 * @cur_chan: channel info
 */
struct ds_ie {
	uint8_t ie;
	uint8_t len;
	uint8_t cur_chan;
} qdf_packed;

/**
 * struct erp_ie: ERP IE
 * @ie: ERP Element Id
 * @len: ERP IE Length
 * @value: EP Info
 */
struct erp_ie {
	uint8_t ie;
	uint8_t len;
	uint8_t value;
} qdf_packed;

/**
 * struct htcap_cmn_ie: HT common IE info
 * @hc_cap: HT capabilities
 * @ampdu_param: ampdu params
 * @mcsset: supported MCS set
 * @extcap: extended HT capabilities
 * @txbf_cap: txbf capabilities
 * @antenna: antenna capabilities
 */
struct htcap_cmn_ie {
	uint16_t hc_cap;
	uint8_t ampdu_param;
	uint8_t mcsset[16];
	uint16_t extcap;
	uint32_t txbf_cap;
	uint8_t antenna;
} qdf_packed;

/**
 * struct htcap_ie: HT Capability IE
 * @id: HT IE
 * @len: HT IE LEN
 * @ie: HT cap info
 */
struct htcap_ie {
	uint8_t id;
	uint8_t len;
	struct htcap_cmn_ie ie;
} qdf_packed;

/**
 * struct tbtt_information_header - TBTT information header
 * @tbbt_info_fieldtype: TBTT information field type
 * @filter_neighbor_ap: filtered neighbor ap
 * @tbbt_info_count: TBTT information count
 * @tbtt_info_length: TBTT information length
 */
struct tbtt_information_header {
	uint16_t tbbt_info_fieldtype:2,
		 filtered_neighbor_ap:1,
		 reserved:1,
		 tbtt_info_count:4,
		 tbtt_info_length:8;
} qdf_packed;

/**
 * struct fils_indication_ie: FILS indication IE element
 * @id: id
 * @len: len
 * @public_key_identifiers_cnt: public key identifiers count
 * @realm_identifiers_cnt: realm identifiers count
 * @is_ip_config_supported: whether ip config is supported in AP
 * @is_cache_id_present: whether cache identifier is present
 * @is_hessid_present: whether hessid is present
 * @is_fils_sk_auth_supported: FILS shared key authentication is supported
 * @is_fils_sk_auth_pfs_supported: FILS shared key auth with PFS is supported
 * @is_pk_auth_supported: FILS public key authentication is supported
 * @reserved: reserved
 * @variable_data: pointer to data depends on initial variables
 */
struct fils_indication_ie {
	uint8_t id;
	uint8_t len;
	uint16_t public_key_identifiers_cnt:3;
	uint16_t realm_identifiers_cnt:3;
	uint16_t is_ip_config_supported:1;
	uint16_t is_cache_id_present:1;
	uint16_t is_hessid_present:1;
	uint16_t is_fils_sk_auth_supported:1;
	uint16_t is_fils_sk_auth_pfs_supported:1;
	uint16_t is_pk_auth_supported:1;
	uint16_t reserved:4;
	uint8_t variable_data[253];
} qdf_packed;

#define WLAN_VENDOR_HT_IE_OFFSET_LEN    4

/**
 * struct wlan_vendor_ie_htcap: vendor private HT Capability IE
 * @id: HT IE
 * @hlen: HT IE len
 * @oui: vendor OUI
 * @oui_type: Oui type
 * @ie: HT cap info
 */
struct wlan_vendor_ie_htcap {
	uint8_t id;
	uint8_t hlen;
	uint8_t oui[3];
	uint8_t oui_type;
	struct htcap_cmn_ie ie;
} qdf_packed;

/**
 * struct wlan_ie_htinfo_cmn: ht info command
 * @hi_ctrlchannel: control channel
 * @hi_extchoff: B0-1 extension channel offset
 * @hi_txchwidth: B2 recommended xmiss width set
 * @hi_rifsmode: rifs mode
 * @hi_ctrlaccess: controlled access only
 * @hi_serviceinterval: B5-7 svc interval granularity
 * @uhi_opmode: B0-1 operating mode
 * @hi_nongfpresent: B2 non greenfield devices present
 * @hi_txburstlimit: B3 transmit burst limit
 * @hi_obssnonhtpresent: B4 OBSS non-HT STA present
 * @hi_reserved0: B5-15 reserved
 * @hi_reserved2: B0-5 reserved
 * @hi_dualbeacon: B6 dual beacon
 * @hi_dualctsprot: B7 dual CTS protection
 * @hi_stbcbeacon: B8 STBC beacon
 * @hi_lsigtxopprot: B9 l-sig txop protection full support
 * @hi_pcoactive: B10 pco active
 * @hi_pcophase: B11 pco phase
 * @hi_reserved1: B12-15 reserved
 * @hi_basicmcsset[16]: basic MCS set
 */
struct wlan_ie_htinfo_cmn {
	uint8_t hi_ctrlchannel;
	uint8_t hi_extchoff:2,
		hi_txchwidth:1,
		hi_rifsmode:1,
		hi_ctrlaccess:1,
		hi_serviceinterval:3;
	uint16_t hi_opmode:2,
		hi_nongfpresent:1,
		hi_txburstlimit:1,
		hi_obssnonhtpresent:1,
		hi_reserved0:11;
	uint16_t hi_reserved2:6,
		hi_dualbeacon:1,
		hi_dualctsprot:1,
		hi_stbcbeacon:1,
		hi_lsigtxopprot:1,
		hi_pcoactive:1,
		hi_pcophase:1,
		hi_reserved1:4;
	uint8_t  hi_basicmcsset[16];
} qdf_packed;

/**
 * struct wlan_ie_htinfo: HT info IE
 * @hi_id: HT info IE
 * @hi_len: HT info IE len
 * @hi_ie: HT info info
 */
struct wlan_ie_htinfo {
	uint8_t hi_id;
	uint8_t hi_len;
	struct wlan_ie_htinfo_cmn  hi_ie;
} qdf_packed;

/**
 * struct wlan_ie_htinfo: vendor private HT info IE
 * @hi_id: HT info IE
 * @hi_len: HT info IE len
 * @hi_oui: vendor OUI
 * @hi_ouitype: Oui type
 * @hi_ie: HT info info
 */
struct wlan_vendor_ie_htinfo {
	uint8_t hi_id;
	uint8_t hi_len;
	uint8_t hi_oui[3];
	uint8_t hi_ouitype;
	struct wlan_ie_htinfo_cmn hi_ie;
} qdf_packed;

#define WLAN_VENDOR_VHTCAP_IE_OFFSET    7
#define WLAN_VENDOR_VHTOP_IE_OFFSET     21

/**
 * struct wlan_ie_vhtcaps - VHT capabilities
 * @elem_id: VHT caps IE
 * @elem_len: VHT caps IE len
 * @max_mpdu_len: MPDU length
 * @supported_channel_widthset: channel width set
 * @ldpc_coding: LDPC coding capability
 * @shortgi80: short GI 80 support
 * @shortgi160and80plus80: short Gi 160 & 80+80 support
 * @tx_stbc; Tx STBC cap
 * @tx_stbc: Rx STBC cap
 * @su_beam_former: SU beam former cap
 * @su_beam_formee: SU beam formee cap
 * @csnof_beamformer_antSup: Antenna support for beamforming
 * @num_soundingdim: Sound dimensions
 * @mu_beam_former: MU beam former cap
 * @mu_beam_formee: MU beam formee cap
 * @vht_txops: TXOP power save
 * @htc_vhtcap: HTC VHT capability
 * @max_ampdu_lenexp: AMPDU length
 * @vht_link_adapt: VHT link adapatation capable
 * @rx_antpattern: Rx Antenna pattern
 * @tx_antpattern: Tx Antenna pattern
 * @rx_mcs_map: RX MCS map
 * @rx_high_sup_data_rate : highest RX supported data rate
 * @tx_mcs_map: TX MCS map
 * @tx_sup_data_rate: highest TX supported data rate
 */
struct wlan_ie_vhtcaps {
	uint8_t elem_id;
	uint8_t elem_len;
	uint32_t max_mpdu_len:2;
	uint32_t supported_channel_widthset:2;
	uint32_t ldpc_coding:1;
	uint32_t shortgi80:1;
	uint32_t shortgi160and80plus80:1;
	uint32_t tx_stbc:1;
	uint32_t rx_stbc:3;
	uint32_t su_beam_former:1;
	uint32_t su_beam_formee:1;
	uint32_t csnof_beamformer_antSup:3;
	uint32_t num_soundingdim:3;
	uint32_t mu_beam_former:1;
	uint32_t mu_beam_formee:1;
	uint32_t vht_txops:1;
	uint32_t htc_vhtcap:1;
	uint32_t max_ampdu_lenexp:3;
	uint32_t vht_link_adapt:2;
	uint32_t rx_antpattern:1;
	uint32_t tx_antpattern:1;
	uint32_t unused:2;
	uint16_t rx_mcs_map;
	uint16_t rx_high_sup_data_rate:13;
	uint16_t reserved2:3;
	uint16_t tx_mcs_map;
	uint16_t tx_sup_data_rate:13;
	uint16_t reserved3:3;
} qdf_packed;

/**
 * struct wlan_ie_vhtop: VHT op IE
 * @elem_id: VHT op IE
 * @elem_len: VHT op IE len
 * @vht_op_chwidth: BSS Operational Channel width
 * @vht_op_ch_freq_seg1: Channel Center frequency
 * @vht_op_ch_freq_seg2: Channel Center frequency for 80+80MHz
 * @vhtop_basic_mcs_set: Basic MCS set
 */
struct wlan_ie_vhtop {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t vht_op_chwidth;
	uint8_t vht_op_ch_freq_seg1;
	uint8_t vht_op_ch_freq_seg2;
	uint16_t vhtop_basic_mcs_set;
} qdf_packed;

#define WLAN_HE_PHYCAP_SU_BFER_OFFSET 3
#define WLAN_HE_PHYCAP_SU_BFER_IDX 7
#define WLAN_HE_PHYCAP_SU_BFER_BITS 1

#define WLAN_HE_PHYCAP_160_SUPPORT BIT(2)
#define WLAN_HE_PHYCAP_80_80_SUPPORT BIT(3)
#define WLAN_HE_MACCAP_LEN 6
#define WLAN_HE_PHYCAP_LEN 11
#define WLAN_HE_MAX_MCS_MAPS 3
/**
 * struct wlan_ie_hecaps - HT capabilities
 * @elem_id: HE caps IE
 * @elem_len: HE caps IE len
 * @elem_id_extn: HE caps extension id
 * @he_mac_cap: HE mac capabilities
 * @he_phy_cap: HE phy capabilities
 * @phy_cap_bytes: HT phy capability bytes
 * @supported_ch_width_set: Supported channel width set
 * @mcs_bw_map: MCS NSS map per bandwidth
 * @rx_mcs_map: RX MCS map
 * @tx_mcs_map: TX MCS map
 */
struct wlan_ie_hecaps {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t elem_id_extn;
	uint8_t he_mac_cap[WLAN_HE_MACCAP_LEN];
	union {
		uint8_t phy_cap_bytes[WLAN_HE_PHYCAP_LEN];
		struct {
			uint32_t reserved:1;
			uint32_t supported_ch_width_set:7;
		} qdf_packed;
	} qdf_packed he_phy_cap;
	struct {
		uint16_t rx_mcs_map;
		uint16_t tx_mcs_map;
	} qdf_packed mcs_bw_map[WLAN_HE_MAX_MCS_MAPS];
} qdf_packed;

/**
 * struct subelem_header: Subelement header
 * @subelem_id: Subelement ID
 * @subelem_len: Subelement length
 */
struct subelem_header {
	uint8_t subelem_id;
	uint8_t subelem_len;
} qdf_packed;

#ifdef WLAN_FEATURE_11BE
#define WLAN_EHT_MACCAP_LEN 2
#define WLAN_EHT_PHYCAP_LEN 9

#define WLAN_EHT_MAX_MCS_MAPS 3

#define EHTCAP_MAC_EPCSPRIACCESS_IDX                    0
#define EHTCAP_MAC_EPCSPRIACCESS_BITS                   1
#define EHTCAP_MAC_EHTOMCTRL_IDX                        1
#define EHTCAP_MAC_EHTOMCTRL_BITS                       1
#define EHTCAP_MAC_TRIGGERED_TXOP_MODE1_IDX             2
#define EHTCAP_MAC_TRIGGERED_TXOP_MODE1_BITS            1
#define EHTCAP_MAC_TRIGGERED_TXOP_MODE2_IDX             3
#define EHTCAP_MAC_TRIGGERED_TXOP_MODE2_BITS            1
#define EHTCAP_MAC_RESTRICTED_TWT_IDX                   4
#define EHTCAP_MAC_RESTRICTED_TWT_BITS                  1
#define EHTCAP_MAC_SCS_TRAFFIC_DESC_IDX                 5
#define EHTCAP_MAC_SCS_TRAFFIC_DESC_BITS                1
#define EHTCAP_MAC_MAX_MPDU_LEN_IDX                     6
#define EHTCAP_MAC_MAX_MPDU_LEN_BITS                    2
#define EHTCAP_MAC_MAX_A_MPDU_LEN_IDX                   8
#define EHTCAP_MAC_MAX_A_MPDU_LEN_BITS                  1
#define EHTCAP_MAC_TRS_SUPPORT_IDX                      9
#define EHTCAP_MAC_TRS_SUPPORT_BITS                     1
#define EHTCAP_MAC_TXOP_RET_SUPPP_IN_SHARING_MODE2_IDX  10
#define EHTCAP_MAC_TXOP_RET_SUPPP_IN_SHARING_MODE2_BITS 1
#define EHTCAP_MAC_TWO_BQRS_SUPP_IDX                    11
#define EHTCAP_MAC_TWO_BQRS_SUPP_BITS                   1
#define EHTCAP_MAC_EHT_LINK_ADAPTATION_SUPP_IDX         12
#define EHTCAP_MAC_EHT_LINK_ADAPTATION_SUPP_BITS        2

#define EHTCAP_PHY_320MHZIN6GHZ_IDX                     1
#define EHTCAP_PHY_320MHZIN6GHZ_BITS                    1
#define EHTCAP_PHY_242TONERUBWLT20MHZ_IDX               2
#define EHTCAP_PHY_242TONERUBWLT20MHZ_BITS              1
#define EHTCAP_PHY_NDP4XEHTLTFAND320NSGI_IDX            3
#define EHTCAP_PHY_NDP4XEHTLTFAND320NSGI_BITS           1
#define EHTCAP_PHY_PARTIALBWULMU_IDX                    4
#define EHTCAP_PHY_PARTIALBWULMU_BITS                   1
#define EHTCAP_PHY_SUBFMR_IDX                           5
#define EHTCAP_PHY_SUBFMR_BITS                          1
#define EHTCAP_PHY_SUBFME_IDX                           6
#define EHTCAP_PHY_SUBFME_BITS                          1
#define EHTCAP_PHY_BFMESSLT80MHZ_IDX                    7
#define EHTCAP_PHY_BFMESSLT80MHZ_BITS                   3
#define EHTCAP_PHY_BFMESS160MHZ_IDX                     10
#define EHTCAP_PHY_BFMESS160MHZ_BITS                    3
#define EHTCAP_PHY_BFMESS320MHZ_IDX                     13
#define EHTCAP_PHY_BFMESS320MHZ_BITS                    3
#define EHTCAP_PHY_NUMSOUNDLT80MHZ_IDX                  16
#define EHTCAP_PHY_NUMSOUNDLT80MHZ_BITS                 3
#define EHTCAP_PHY_NUMSOUND160MHZ_IDX                   19
#define EHTCAP_PHY_NUMSOUND160MHZ_BITS                  3
#define EHTCAP_PHY_NUMSOUND320MHZ_IDX                   22
#define EHTCAP_PHY_NUMSOUND320MHZ_BITS                  3
#define EHTCAP_PHY_NG16SUFB_IDX                         25
#define EHTCAP_PHY_NG16SUFB_BITS                        1
#define EHTCAP_PHY_NG16MUFB_IDX                         26
#define EHTCAP_PHY_NG16MUFB_BITS                        1
#define EHTCAP_PHY_CODBK42SUFB_IDX                      27
#define EHTCAP_PHY_CODBK42SUFB_BITS                     1
#define EHTCAP_PHY_CODBK75MUFB_IDX                      28
#define EHTCAP_PHY_CODBK75MUFB_BITS                     1
#define EHTCAP_PHY_TRIGSUBFFB_IDX                       29
#define EHTCAP_PHY_TRIGSUBFFB_BITS                      1
#define EHTCAP_PHY_TRIGMUBFPARTBWFB_IDX                 30
#define EHTCAP_PHY_TRIGMUBFPARTBWFB_BITS                1
#define EHTCAP_PHY_TRIGCQIFB_IDX                        31
#define EHTCAP_PHY_TRIGCQIFB_BITS                       1

#define EHTCAP_PHY_PARTBWDLMUMIMO_IDX                   32
#define EHTCAP_PHY_PARTBWDLMUMIMO_BITS                  1
#define EHTCAP_PHY_PSRSR_IDX                            33
#define EHTCAP_PHY_PSRSR_BITS                           1
#define EHTCAP_PHY_PWRBSTFACTOR_IDX                     34
#define EHTCAP_PHY_PWRBSTFACTOR_BITS                    1
#define EHTCAP_PHY_4XEHTLTFAND800NSGI_IDX               35
#define EHTCAP_PHY_4XEHTLTFAND800NSGI_BITS              1
#define EHTCAP_PHY_MAXNC_IDX                            36
#define EHTCAP_PHY_MAXNC_BITS                           4
#define EHTCAP_PHY_NONTRIGCQIFB_IDX                     40
#define EHTCAP_PHY_NONTRIGCQIFB_BITS                    1
#define EHTCAP_PHY_TX1024AND4096QAMLS242TONERU_IDX      41
#define EHTCAP_PHY_TX1024AND4096QAMLS242TONERU_BITS     1
#define EHTCAP_PHY_RX1024AND4096QAMLS242TONERU_IDX      42
#define EHTCAP_PHY_RX1024AND4096QAMLS242TONERU_BITS     1
#define EHTCAP_PHY_PPETHRESPRESENT_IDX                  43
#define EHTCAP_PHY_PPETHRESPRESENT_BITS                 1
#define EHTCAP_PHY_CMNNOMPKTPAD_IDX                     44
#define EHTCAP_PHY_CMNNOMPKTPAD_BITS                    2
#define EHTCAP_PHY_MAXNUMEHTLTF_IDX                     46
#define EHTCAP_PHY_MAXNUMEHTLTF_BITS                    5
#define EHTCAP_PHY_SUPMCS15_IDX                         51
#define EHTCAP_PHY_SUPMCS15_BITS                        4
#define EHTCAP_PHY_EHTDUPIN6GHZ_IDX                     55
#define EHTCAP_PHY_EHTDUPIN6GHZ_BITS                    1
#define EHTCAP_PHY_20MHZOPSTARXNDPWIDERBW_IDX           56
#define EHTCAP_PHY_20MHZOPSTARXNDPWIDERBW_BITS          1
#define EHTCAP_PHY_NONOFDMAULMUMIMOLT80MHZ_IDX          57
#define EHTCAP_PHY_NONOFDMAULMUMIMOLT80MHZ_BITS         1
#define EHTCAP_PHY_NONOFDMAULMUMIMO160MHZ_IDX           58
#define EHTCAP_PHY_NONOFDMAULMUMIMO160MHZ_BITS          1
#define EHTCAP_PHY_NONOFDMAULMUMIMO320MHZ_IDX           59
#define EHTCAP_PHY_NONOFDMAULMUMIMO320MHZ_BITS          1
#define EHTCAP_PHY_MUBFMRLT80MHZ_IDX                    60
#define EHTCAP_PHY_MUBFMRLT80MHZ_BITS                   1
#define EHTCAP_PHY_MUBFMR160MHZ_IDX                     61
#define EHTCAP_PHY_MUBFMR160MHZ_BITS                    1
#define EHTCAP_PHY_MUBFMR320MHZ_IDX                     62
#define EHTCAP_PHY_MUBFMR320MHZ_BITS                    1
#define EHTCAP_PHY_TB_SOUNDING_FEEDBACK_RL_IDX          63
#define EHTCAP_PHY_TB_SOUNDING_FEEDBACK_RL_BITS         1
#define EHTCAP_PHY_RX_1K_QAM_IN_WIDER_BW_DL_OFDMA_IDX   64
#define EHTCAP_PHY_RX_1K_QAM_IN_WIDER_BW_DL_OFDMA_BITS  1
#define EHTCAP_PHY_RX_4K_QAM_IN_WIDER_BW_DL_OFDMA_IDX   65
#define EHTCAP_PHY_RX_4K_QAM_IN_WIDER_BW_DL_OFDMA_BITS  1
#define EHTCAP_PHY_20MHZ_ONLY_CAPS_IDX                  66
#define EHTCAP_PHY_20MHZ_ONLY_CAPS_BITS                 1
#define EHTCAP_PHY_20MHZ_ONLY_TRIGGER_MUBF_FL_BW_FB_DLMUMIMO_IDX  67
#define EHTCAP_PHY_20MHZ_ONLY_TRIGGER_MUBF_FL_BW_FB_DLMUMIMO_BITS 1
#define EHTCAP_PHY_20MHZ_ONLY_MRU_SUPP_IDX              68
#define EHTCAP_PHY_20MHZ_ONLY_MRU_SUPP_BITS             1

#define EHTCAP_RX_MCS_NSS_MAP_IDX                       0
#define EHTCAP_RX_MCS_NSS_MAP_BITS                      4
#define EHTCAP_TX_MCS_NSS_MAP_IDX                       4
#define EHTCAP_TX_MCS_NSS_MAP_BITS                      4

#define EHTCAP_PPET_NSS_IDX               0
#define EHTCAP_PPET_NSS_BITS              4
#define EHTCAP_PPET_RU_INDEX_BITMASK_IDX  4
#define EHTCAP_PPET_RU_INDEX_BITMASK_BITS 5

#define EHTOP_INFO_PRESENT_IDX                           0
#define EHTOP_INFO_PRESENT_BITS                          1
#define EHTOP_PARAM_DISABLED_SC_BITMAP_PRESENT_IDX       1
#define EHTOP_PARAM_DISABLED_SC_BITMAP_PRESENT_BITS      1
#define EHTOP_DEFAULT_PE_DURATION_IDX                    2
#define EHTOP_DEFAULT_PE_DURATION_BITS                   1
#define EHTOP_GRP_ADDRESSED_BU_IND_LIMIT_IDX             3
#define EHTOP_GRP_ADDRESSED_BU_IND_LIMIT_BITS            1
#define EHTOP_GRP_ADDRESSED_BU_IND_EXPONENT_IDX          4
#define EHTOP_GRP_ADDRESSED_BU_IND_EXPONENT_BITS         2

#define EHTOP_INFO_CHAN_WIDTH_IDX          0
#define EHTOP_INFO_CHAN_WIDTH_BITS         3

#define BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_IDX       1
#define BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_BITS      1

#define BW_IND_CHAN_WIDTH_IDX              0
#define BW_IND_CHAN_WIDTH_BITS             3

#define EHTOP_RX_MCS_NSS_MAP_IDX                       0
#define EHTOP_RX_MCS_NSS_MAP_BITS                      4
#define EHTOP_TX_MCS_NSS_MAP_IDX                       4
#define EHTOP_TX_MCS_NSS_MAP_BITS                      4

#define MAX_EHT_MCS_NSS_MAP_LEN 9

/**
 * struct eht_basic_mcs_nss_set - EHT Basic mcs nss set
 * @max_nss_mcs_0_7: Rx, Tx Max Nss That Supports EHT-MCS 0-7
 * @max_nss_mcs_8_9: Rx, Tx Max Nss That Supports EHT-MCS 8-9
 * @max_nss_mcs_10_11: Rx, Tx Max Nss That Supports EHT-MCS 10-11
 * @max_nss_mcs_12_13: Rx, Tx Max Nss That Supports EHT-MCS 12-13
 */
struct eht_basic_mcs_nss_set {
	uint8_t max_nss_mcs_0_7;
	uint8_t max_nss_mcs_8_9;
	uint8_t max_nss_mcs_10_11;
	uint8_t max_nss_mcs_12_13;
} qdf_packed;

/**
 * struct wlan_ie_ehtcaps - EHT capabilities
 * @elem_id: EHT caps IE
 * @elem_len: EHT caps IE len
 * @elem_id_extn: EHT caps extension id
 * @eht_mac_cap: EHT mac capabilities
 * @eht_phy_cap: EHT phy capabilities
 * @phy_cap_bytes: EHT phy capability bytes
 * @supported_ch_width_set: Supported channel width set
 * @mcs_bw_map: MCS NSS map per bandwidth
 * @rx_mcs_map: RX MCS map
 * @tx_mcs_map: TX MCS map
 */
struct wlan_ie_ehtcaps {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t elem_id_extn;
	uint8_t eht_mac_cap[WLAN_EHT_MACCAP_LEN];
	union {
		uint8_t phy_cap_bytes[WLAN_EHT_PHYCAP_LEN];
		struct {
			uint32_t reserved:1;
			uint32_t supported_ch_width_set:7;
		} qdf_packed;
	} qdf_packed eht_phy_cap;
	union {
		struct {
			uint8_t max_nss_mcs_0_9;
			uint8_t max_nss_mcs_10_11;
			uint8_t max_nss_mcs_12_13;
		} qdf_packed mcs_bw_map[WLAN_EHT_MAX_MCS_MAPS];
		struct eht_basic_mcs_nss_set mcs_bw_map_20_sta;
		uint8_t mcs_nss_map_bytes[MAX_EHT_MCS_NSS_MAP_LEN];
	} qdf_packed;
} qdf_packed;

/**
 * struct wlan_ie_ehtops - EHT operation element
 * @elem_id: EHT caps IE
 * @elem_len: EHT caps IE len
 * @elem_id_extn: EHT caps extension id
 * @ehtop_param: EHT Operation Parameters
 * @basic_mcs_nss_set: EHT basic mcs nss set
 * @control: Control field in EHT Operation Information
 * @ccfs0: EHT Channel Centre Frequency Segment0 information
 * @ccfs1: EHT Channel Centre Frequency Segment1 information
 * @disabled_sub_chan_bitmap: Bitmap to indicate 20MHz subchannel is punctured
 *                            or not
 */
struct wlan_ie_ehtops {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t elem_id_extn;
	uint8_t ehtop_param;
	struct eht_basic_mcs_nss_set basic_mcs_nss_set;
	uint8_t control;
	uint8_t ccfs0;
	uint8_t ccfs1;
	uint8_t disabled_sub_chan_bitmap[2];
} qdf_packed;

/**
 * struct wlan_ie_bw_ind - Bandwidth Indication Element
 * @elem_id: Element ID
 * @elem_len: Element length
 * @elem_id_extn: Element ID extension
 * @bw_ind_param: bw indication element parameters
 * @control: Control field in bw_ind Operation Information
 * @ccfs0: EHT Channel Centre Frequency Segment0 information
 * @ccfs1: EHT Channel Centre Frequency Segment1 information
 * @disabled_sub_chan_bitmap: Bitmap to indicate 20MHz subchannel
 *                            is punctured or not
 */
struct wlan_ie_bw_ind {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t elem_id_extn;
	uint8_t bw_ind_param;
	uint8_t control;
	uint8_t ccfs0;
	uint8_t ccfs1;
	uint8_t disabled_sub_chan_bitmap[2];
} qdf_packed;

#ifdef WLAN_FEATURE_11BE_MLO
#define WLAN_MLO_MAX_VDEVS 2

/* Size in octets of the BSS Parameters Change Count (sub)field */
#define WLAN_ML_BSSPARAMCHNGCNT_SIZE                    1

/**
 * struct rnr_mld_info - Reduced Neighbor Report MLD information
 * @mld_id: MLD ID
 * @link_id: Link ID
 * @bss_param_change_cnt: BSS parameters change count
 * @all_updates_included: All Updates Included
 */
struct rnr_mld_info {
	uint8_t mld_id;
	uint16_t link_id: 4,
		 bss_param_change_cnt: 8,
		 all_updates_included: 1,
		 reserved: 3;
} qdf_packed;

/**
 * struct wlan_ie_multilink - Fixed fields in Multi-Link IE
 * @elem_id: Element ID
 * @elem_len: Element length
 * @elem_id_ext: Element ID extension
 * @mlcontrol: Multi-Link element Control field
 */
struct wlan_ie_multilink {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t elem_id_ext;
	uint16_t mlcontrol;
} qdf_packed;

/**
 * struct wlan_sta_profile - Per STA profile IE
 * @sub_elem_id: Sub element ID
 * @per_sta_len: Per STA profile length
 * @sta_control: STA control field
 * @num_data: Number of valid data bytes
 * @data: Per STA profile IE elements
 */
struct wlan_sta_profile {
	uint8_t sub_elem_id;
	uint8_t per_sta_len;
	uint16_t sta_control;
	uint8_t num_data;
	uint8_t *data;
} qdf_packed;

/**
 * struct wlan_ml_probe_req - Multi-Link Probe req IE
 * @ml_ie_ff: Multi Link IE fixed field
 * @common_info_len: Common info length
 * @mld_id: MLD ID
 * @sta_profile: STA profile IE
 */
struct wlan_ml_probe_req {
	struct wlan_ie_multilink ml_ie_ff;
	uint8_t common_info_len;
	uint8_t mld_id;
	struct wlan_sta_profile sta_profile[WLAN_MLO_MAX_VDEVS];
} qdf_packed;
/* The above fixed fields may be followed by:
 * Common Info (variable size)
 * Link Info (variable size)
 */

/* Definitions related to Multi-Link element Control field applicable across
 * variants.
 */

/* The below fields and subfields have been transitioned to D1.5, and the rest
 * will be checked and transitioned to D1.5 separately:
 * 1. Presence bitmap subfield in the Multi-Link Control field.
 * 2. Common Info Length subfield of Common Info field.
 * 3. STA Control field in Per-STA Profile subelement in Basic variant
 *    Multi-Link element Link Info field.
 * 4. STA Info Length subfield in STA Info field in Per-STA Profile subelement
 *    in Basic variant Multi-Link element Link Info field.
 * 5. EML Capabilities subfield of Common Info field.
 * 6. MLD Capabilities subfield of Common Info field.
 */

/* Size in octets of Multi-Link element Control field */
#define WLAN_ML_CTRL_SIZE                                          2

/* Definitions for subfields in Multi-Link element Control field. Any unused
 * bits are reserved.
 */
/* Type */
#define WLAN_ML_CTRL_TYPE_IDX                                       0
#define WLAN_ML_CTRL_TYPE_BITS                                      3
/* Presence Bitmap */
#define WLAN_ML_CTRL_PBM_IDX                                        4
#define WLAN_ML_CTRL_PBM_BITS                                       12

/**
 * enum wlan_ml_variant - Encoding for Type subfield in Multi-Link element
 * Control field, which provides the Multi-Link element variant.
 * Note: In case of holes in the enumeration, scheme for invalid value
 * determination should be changed.
 * @WLAN_ML_VARIANT_BASIC: Basic variant
 * @WLAN_ML_VARIANT_PROBEREQ: Probe Request variant
 * @WLAN_ML_VARIANT_RECONFIG: Reconfiguration variant
 * @WLAN_ML_VARIANT_TDLS: TDLS variant
 * @WLAN_ML_VARIANT_PRIORITYACCESS: Priority Access variant
 * @WLAN_ML_VARIANT_INVALIDSTART: Start of invalid value range
 */
enum wlan_ml_variant {
	WLAN_ML_VARIANT_BASIC = 0,
	WLAN_ML_VARIANT_PROBEREQ = 1,
	WLAN_ML_VARIANT_RECONFIG = 2,
	WLAN_ML_VARIANT_TDLS = 3,
	WLAN_ML_VARIANT_PRIORITYACCESS = 4,
	WLAN_ML_VARIANT_INVALIDSTART,
};

/**
 *  enum wlan_ml_linfo_subelementid - IDs for subelements in Multi-Link element
 *  Link Info field.
 *  @WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE: Per-STA Profile
 *  @WLAN_ML_LINFO_SUBELEMID_VENDOR: Vendor specific
 *  @WLAN_ML_LINFO_SUBELEMID_FRAGMENT: Fragment
 */
enum wlan_ml_linfo_subelementid {
	WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE  = 0,
	WLAN_ML_LINFO_SUBELEMID_VENDOR = 221,
	WLAN_ML_LINFO_SUBELEMID_FRAGMENT = 254,
};

/* End of definitions related to Multi-Link element Control field applicable
 * across variants.
 */

/* Definitions related to Basic variant Multi-Link element. */

/* Definitions for bits in the Presence Bitmap subfield in Basic variant
 * Multi-Link element Control field. Any unused bits are reserved.
 */
/* Link ID Info Present */
#define WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P               ((uint16_t)BIT(0))
/* BSS Parameters Change Count Present */
#define WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P        ((uint16_t)BIT(1))
/* Medium Synchronization Delay Information Present */
#define WLAN_ML_BV_CTRL_PBM_MEDIUMSYNCDELAYINFO_P      ((uint16_t)BIT(2))
/* EML Capabilities Present */
#define WLAN_ML_BV_CTRL_PBM_EMLCAP_P                   ((uint16_t)BIT(3))
/* MLD Capabilities and operation Present */
#define WLAN_ML_BV_CTRL_PBM_MLDCAPANDOP_P              ((uint16_t)BIT(4))
/* MLD ID Present */
#define WLAN_ML_BV_CTRL_PBM_MLDID_P                    ((uint16_t)BIT(5))

/* Definitions related to Basic variant Multi-Link element Common Info field */

/* Size in octets of Common Info Length subfield of Common Info field in
 * Basic variant Multi-Link element.
 */
/* Common Info Length  */
#define WLAN_ML_BV_CINFO_LENGTH_SIZE                               1

/* Size in octets of Link ID Info subfield in Basic variant Multi-Link element
 * Common Info field.
 */
#define WLAN_ML_BV_CINFO_LINKIDINFO_SIZE                            1

/* Definitions for sub-sub fields in Link ID Info subfield in Basic variant
 * Multi-Link element Common Info field. Any unused bits are reserved.
 */
/* Link ID */
#define WLAN_ML_BV_CINFO_LINKIDINFO_LINKID_IDX                      0
#define WLAN_ML_BV_CINFO_LINKIDINFO_LINKID_BITS                     4

/* Size in octets of Medium Synchronization Delay Information subfield in Basic
 * variant Multi-Link element Common Info field.
 */
#define WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE                      2

/* Definitions for sub-sub fields in Medium Synchronization Delay Information
 * subfield in Basic variant Multi-Link element Common Info field.
 */
/* Medium Synchronization Duration */
#define WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_DURATION_IDX             0
#define WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_DURATION_BITS            8
/* Medium Synchronization OFDM ED Threshold  */
#define WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_OFDMEDTHRESH_IDX         8
#define WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_OFDMEDTHRESH_BITS        4
/* Medium Synchronization Maximum Number Of TXOPs  */
#define WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_MAXTXOPS_IDX             12
#define WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_MAXTXOPS_BITS            4

/* Size in octets of EML Capabilities subfield in Basic variant Multi-Link
 * element Common Info field.
 */
#define WLAN_ML_BV_CINFO_EMLCAP_SIZE                                2

/* Definitions for sub-sub fields in EML Capabilities subfield in Basic variant
 * Multi-Link element Common Info field as per IEEE P802.11be/D1.5.
 * Any unused bits are reserved.
 */
/* EMLSR Support */
#define WLAN_ML_BV_CINFO_EMLCAP_EMLSRSUPPORT_IDX                    0
#define WLAN_ML_BV_CINFO_EMLCAP_EMLSRSUPPORT_BITS                   1
/* EMLSR Delay */
#define WLAN_ML_BV_CINFO_EMLCAP_EMLSR_PADDINGDELAY_IDX              1
#define WLAN_ML_BV_CINFO_EMLCAP_EMLSR_PADDINGDELAY_BITS             3
/* EMLSR Transition Delay */
#define WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_IDX                 4
#define WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_BITS                3
/* EMLMR Support */
#define WLAN_ML_BV_CINFO_EMLCAP_EMLMRSUPPORT_IDX                    7
#define WLAN_ML_BV_CINFO_EMLCAP_EMLMRSUPPORT_BITS                   1
/* EMLMR Delay */
#define WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_IDX                      8
#define WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_BITS                     3
/* Transition Timeout */
#define WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_IDX                    11
#define WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_BITS                   4

/**
 * wlan_ml_bv_cinfo_emlcap_emlsr_padding_delay - Encoding for EMLSR Padding
 * Delay sub field in EML Capabilities subfield in Basic variant Multi-Link
 * element Common Info field.
 * Note: In case of holes in the enumeration, scheme for invalid value
 * determination should be changed.
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_0US: EMLSR delay of 0 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_32US: EMLSR delay of 32 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_64US: EMLSR delay of 64 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_128US: EMLSR delay of 128 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_256US: EMLSR delay of 256 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_INVALIDSTART: Start of invalid value
 * range
 */
enum wlan_ml_bv_cinfo_emlcap_emlsr_padding_delay {
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_0US = 0,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_32US = 1,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_64US = 2,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_128US = 3,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_256US = 4,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_INVALIDSTART,
};

/**
 * wlan_ml_bv_cinfo_emlsr_transition_delay - Encoding for EMLSR
 * Transition delay in EML Capabilities subfield in Basic variant
 * Multi-Link element Common Info field.
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_0US: EMLSR transition delay of 0 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_16US: EMLSR transition delay of
 * 16 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_32US: EMLSR transition delay of
 * 32 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_64US: EMLSR transition delay of
 * 64 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_128US: EMLSR transition delay of
 * 128 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_256US: EMLSR transition delay of
 * 256 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_INVALIDSTART: Start of invalid
 * value range
 */
enum wlan_ml_bv_cinfo_emlsr_transition_delay {
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_0US = 0,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_16US = 1,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_32US = 2,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_64US = 3,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_128US = 4,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_256US = 5,
	WLAN_ML_BV_CINFO_EMLCAP_EMLSRTRANSDELAY_INVALIDSTART,
};

/**
 * wlan_ml_bv_cinfo_emlcap_emlmrdelay - Encoding for EMLMR Delay sub-sub field
 * in EML Capabilities subfield in Basic variant Multi-Link element Common Info
 * field.
 * Note: In case of holes in the enumeration, scheme for invalid value
 * determination should be changed.
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_0US: EMLMR delay of 0 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_32US: EMLMR delay of 32 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_64US: EMLMR delay of 64 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_128US: EMLMR delay of 128 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_256US: EMLMR delay of 256 us
 * @WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_INVALIDSTART: Start of invalid
 * value range
 */
enum wlan_ml_bv_cinfo_emlcap_emlmrdelay {
	WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_0US = 0,
	WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_32US = 1,
	WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_64US = 2,
	WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_128US = 3,
	WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_256US = 4,
	WLAN_ML_BV_CINFO_EMLCAP_EMLMRDELAY_INVALIDSTART,
};

/**
 * wlan_ml_bv_cinfo_emlcap_transtimeout - Encoding for Transition Timeout
 * sub-sub field in EML Capabilities subfield in Basic variant Multi-Link
 * element Common Info field.
 * Note: a) In case of holes in the enumeration, scheme for invalid value
 * determination should be changed. b) A mathematical formula could have been
 * used instead of an enumeration. However, the standard explicitly lists out
 * values instead of using a formula, and we reflect this accordingly using an
 * enumeration.
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_0TU: Transition Timeout value of 0 TUs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_128MU: Transition Timeout value of 128μs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_256MU: Transition Timeout value of 256μs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_512MU: Transition Timeout value of 512μs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_1TU: Transition Timeout value of 1 TU
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_2TU: Transition Timeout value of 2 TUs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_4TU: Transition Timeout value of 4 TUs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_8TU: Transition Timeout value of 8 TUs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_16TU: Transition Timeout value of 16
 * TUs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_32TU: Transition Timeout value of 32
 * TUs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_64TU: Transition Timeout value of 64
 * TUs
 * WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_128TU: Transition Timeout value of 128
 * TUs
 * @WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_INVALIDSTART: Start of invalid value
 * range
 */
enum wlan_ml_bv_cinfo_emlcap_transtimeout {
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_0TU = 0,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_128MU = 1,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_256MU = 2,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_512MU = 3,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_1TU = 4,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_2TU = 5,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_4TU = 6,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_8TU = 7,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_16TU = 8,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_32TU = 9,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_64TU = 10,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_128TU = 11,
	WLAN_ML_BV_CINFO_EMLCAP_TRANSTIMEOUT_INVALIDSTART,
};

/* Size in octets of MLD Capabilities and operation subfield in Basic variant
 * Multi-Link element Common Info field as per IEEE P802.11be/D1.5.
 */
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE                                2

/* Definitions for sub-sub fields in MLD Capabilities subfield in Basic variant
 * Multi-Link element Common Info field. Any unused bits are reserved.
 */
/* Maximum Number Of Simultaneous Links */
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_MAXSIMULLINKS_IDX                   0
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_MAXSIMULLINKS_BITS                  4
/* SRS Support */
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_SRSSUPPORT_IDX                      4
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_SRSSUPPORT_BITS                     1
/* TID-To-Link Mapping Negotiation Supported */
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_TIDTOLINKMAPNEGSUPPORT_IDX          5
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_TIDTOLINKMAPNEGSUPPORT_BITS         2
/* Frequency Separation For STR */
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_STRFREQSEPARATION_IDX               7
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_STRFREQSEPARATION_BITS              5
/* AAR Support */
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_AARSUPPORT_IDX                      12
#define WLAN_ML_BV_CINFO_MLDCAPANDOP_AARSUPPORT_BITS                     1

/* Size in octets of MLD ID subfield in Basic variant Multi-Link
 * element Common Info field.
 */
#define WLAN_ML_BV_CINFO_MLDID_SIZE                                      1

/* Max value in octets of Common Info Length subfield of Common Info field in
 * Basic variant Multi-Link element
 */
#define WLAN_ML_BV_CINFO_LENGTH_MAX \
	(WLAN_ML_BV_CINFO_LENGTH_SIZE + \
	 QDF_MAC_ADDR_SIZE + \
	 WLAN_ML_BV_CINFO_LINKIDINFO_SIZE + \
	 WLAN_ML_BSSPARAMCHNGCNT_SIZE + \
	 WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE + \
	 WLAN_ML_BV_CINFO_EMLCAP_SIZE + \
	 WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE + \
	 WLAN_ML_BV_CINFO_MLDID_SIZE)

/* End of definitions related to Basic variant Multi-Link element Common Info
 * field.
 */

/* Definitions related to Basic variant Multi-Link element Link Info field */

/* Basic variant Multi-Link element Link Info field contains zero or more
 * subelements.
 */

/**
 * struct wlan_ml_bv_linfo_perstaprof - Fixed fields of Per-STA Profile
 * subelement in Basic variant Multi-Link element Link Info field
 * @subelem_id: Subelement ID
 * @subelem_len: Subelement length
 * @stacontrol: STA Control
 */
struct wlan_ml_bv_linfo_perstaprof {
	uint8_t subelem_id;
	uint8_t subelem_len;
	uint16_t stacontrol;
} qdf_packed;

/* The above fixed fields may be followed by:
 * STA Info (variable size)
 * STA Profile (variable size)
 */

/* Size in octets of STA Control field of Per-STA Profile subelement in Basic
 * variant Multi-Link element Link Info field.
 */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_SIZE                   2

/* Definitions for subfields in STA Control field of Per-STA Profile subelement
 * in Basic variant Multi-Link element Link Info field. Any unused bits are
 * reserved.
 */
/* Link ID */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX              0
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS             4
/* Complete Profile */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_IDX           4
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_BITS          1
/* MAC Address Present */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_MACADDRP_IDX            5
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_MACADDRP_BITS           1
/* Beacon Interval Present */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BCNINTP_IDX             6
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BCNINTP_BITS            1
/* TSF Offset Present */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_TSFOFFSETP_IDX          7
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_TSFOFFSETP_BITS         1
/* DTIM Info Present */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_DTIMINFOP_IDX           8
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_DTIMINFOP_BITS          1
/* NSTR Link Pair Present */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRLINKPRP_IDX         9
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRLINKPRP_BITS        1
/* NSTR Bitmap Size */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_IDX            10
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_BITS           1
/* BSS Parameters Change Count Present */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BSSPARAMCHNGCNTP_IDX    11
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BSSPARAMCHNGCNTP_BITS   1

/* Definitions for subfields in STA Info field of Per-STA Profile subelement
 * in Basic variant Multi-Link element Link Info field.
 */
/* STA Info Length */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE             1

/* Size in octets of the TSF Offset in STA info field of Per-STA Profile
 * subelement in Basic variant Multi-Link element Link Info field.
 */
#define WLAN_ML_TSF_OFFSET_SIZE             8

/**
 * wlan_ml_bv_linfo_perstaprof_stactrl_nstrbmsz - Encoding for NSTR Bitmap Size
 * in STA Control field of Per-STA Profile subelement in Basic variant
 * Multi-Link element Link Info field.
 * Note: In case of holes in the enumeration, scheme for invalid value
 * determination should be changed.
 * @WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_1_OCTET: NSTR Indication
 * Bitmap size of 1 octet
 * @WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_2_OCTETS: NSTR Indication
 * Bitmap size of 2 octets
 * @WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_INVALIDSTART: Start of invalid
 * value range
 */
enum wlan_ml_bv_linfo_perstaprof_stactrl_nstrbmsz {
	WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_1_OCTET = 0,
	WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_2_OCTETS = 1,
	WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_INVALIDSTART,
};

/* Max size in octets of the NSTR Bitmap in STA Control field of Per-STA Profile
 * subelement in Basic variant Multi-Link element Link Info field.
 */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_MAX 2

/**
 * struct wlan_ml_bv_linfo_perstaprof_stainfo_dtiminfo - DTIM info in STA info
 * in Per-STA Profile subelement in Basic variant Multi-Link element Link Info
 * field.
 * @dtimcount: DTIM Count
 * @dtimperiod: DTIM Period
 */
struct wlan_ml_bv_linfo_perstaprof_stainfo_dtiminfo {
	uint8_t dtimcount;
	uint8_t dtimperiod;
} qdf_packed;

/* Max value in octets of STA Info Length in STA Info field of Per-STA Profile
 * subelement in Basic variant Multi-Link element Link Info field.
 */
#define WLAN_ML_BV_LINFO_PERSTAPROF_STAINFO_LENGTH_MAX \
	(WLAN_ML_BV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE + \
	 QDF_MAC_ADDR_SIZE + \
	 WLAN_BEACONINTERVAL_LEN + \
	 WLAN_ML_TSF_OFFSET_SIZE + \
	 sizeof(struct wlan_ml_bv_linfo_perstaprof_stainfo_dtiminfo) + \
	 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_MAX + \
	 WLAN_ML_BSSPARAMCHNGCNT_SIZE)

/* End of definitions related to Basic variant Multi-Link element Link Info
 * field.
 */

/* End of definitions related to Basic variant Multi-Link element. */

/* Definitions related to Probe Req Multi-Link element Link Info field */

/* Probe Req variant Multi-Link element Link Info field contains zero or more
 * subelements.
 */

/**
 * struct wlan_ml_prv_linfo_perstaprof - Fixed fields of Per-STA Profile
 * subelement in Probe Request variant Multi-Link element Link Info field
 * @subelem_id: Subelement ID
 * @subelem_len: Subelement length
 * @stacontrol: STA Control
 */
struct wlan_ml_prv_linfo_perstaprof {
	uint8_t subelem_id;
	uint8_t subelem_len;
	uint16_t stacontrol;
} qdf_packed;

/* The above fixed fields may be followed by:
 * STA Profile (variable size)
 */

/* Size in octets of STA Control field of Per-STA Profile subelement in Basic
 * variant Multi-Link element Link Info field.
 */
#define WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_SIZE                   2

/* Definitions for subfields in STA Control field of Per-STA Profile subelement
 * in Probe Req variant Multi-Link element Link Info field. Any unused bits are
 * reserved.
 */
/* Link ID */
#define WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX              0
#define WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS             4
/* Complete Profile */
#define WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_IDX           4
#define WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_BITS          1

/* Definitions for bits in the Presence Bitmap subfield in Probe Req variant
 * Multi-Link element Control field. Any unused bits are reserved.
 */
/* MLD ID Present */
#define WLAN_ML_PRV_CTRL_PBM_MLDID_P               ((uint16_t)BIT(0))
/* Size in octets of Common Info Length subfield of Common Info field in
 * Probe Req variant Multi-Link element.
 */
/* Common Info Length  */
#define WLAN_ML_PRV_CINFO_LENGTH_SIZE                               1

/* Size in octets of MLD ID subfield in Probe Req variant Multi-Link
 * element Common Info field.
 */
#define WLAN_ML_PRV_CINFO_MLDID_SIZE                                1

/* Max value in octets of Common Info Length subfield of Common Info field in
 * Probe Req variant Multi-Link element
 */
#define WLAN_ML_PRV_CINFO_LENGTH_MAX \
	(WLAN_ML_PRV_CINFO_LENGTH_SIZE + \
	 WLAN_ML_PRV_CINFO_MLDID_SIZE)

/* End of definitions related to Probe Request variant Multi-Link element. */

/* Definitions related to Reconfiguration variant Multi-Link element (per
 * IEEE802.11be D3.0)
 */

/* Definitions for bits in the Presence Bitmap subfield in Reconfiguration
 * variant Multi-Link element Control field. Any unused bits are reserved.
 */
/* MLD MAC Address Present */
#define WLAN_ML_RV_CTRL_PBM_MLDMACADDR_P               ((uint16_t)BIT(0))

/* Definitions related to Reconfiguration variant Multi-Link element Common Info
 * field.
 */

/* Size in octets of Common Info Length subfield of Common Info field in
 * Reconfiguration variant Multi-Link element.
 */
#define WLAN_ML_RV_CINFO_LENGTH_SIZE                               1

/* Max value in octets of Common Info Length subfield of Common Info field in
 * Reconfiguration variant Multi-Link element
 */
#define WLAN_ML_RV_CINFO_LENGTH_MAX \
	(WLAN_ML_RV_CINFO_LENGTH_SIZE + \
	 QDF_MAC_ADDR_SIZE)

/* End of definitions related to Reconfiguration variant Multi-Link element
 * Common Info field.
 */

/* Definitions related to Reconfiguration variant Multi-Link element Link Info
 * field
 */

/**
 * struct wlan_ml_rv_linfo_perstaprof - Fixed fields of Per-STA Profile
 * subelement in Reconfiguration variant Multi-Link element Link Info field
 * @subelem_id: Subelement ID
 * @subelem_len: Subelement length
 * @stacontrol: STA Control
 */
struct wlan_ml_rv_linfo_perstaprof {
	uint8_t subelem_id;
	uint8_t subelem_len;
	uint16_t stacontrol;
} qdf_packed;

/* The above fixed fields may be followed by:
 * STA Info (variable size)
 */

/* Size in octets of STA Control field of Per-STA Profile subelement in
 * Reconfiguration variant Multi-Link element Link Info field.
 */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_SIZE                   2

/* Definitions for subfields in STA Control field of Per-STA Profile subelement
 * in Reconfiguration variant Multi-Link element Link Info field. Any unused
 * bits are reserved.
 */
/* Link ID */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX              0
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS             4
/* Complete Profile */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_IDX           4
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_BITS          1
/* STA MAC Address Present */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_STAMACADDRP_IDX         5
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_STAMACADDRP_BITS        1
/* AP Removal Timer Present */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_APREMOVALTIMERP_IDX     6
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_APREMOVALTIMERP_BITS    1
/* Operation Update Type */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_OPUPDATETYPE_IDX        7
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_OPUPDATETYPE_BITS       4
/* Operation Parameters Present */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_OPPARAMSP_IDX           11
#define WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_OPPARAMSP_BITS          1

/**
 * enum wlan_ml_operation_update_type - Encoding for the Operation Update Type
 * subfield in STA Control field of Per-STA Profile subelement in
 * Reconfiguration variant Multi-Link element Link Info field. Note: In case of
 * future holes in the enumeration, scheme for reserved value determination
 * should be changed.
 * @WLAN_ML_OPERATION_UPDATE_TYPE_OPPARAMUPDATE: Operation Parameter Update
 * @WLAN_ML_OPERATION_UPDATE_TYPE_RESERVEDSTART: Start of reserved value range
 */
enum wlan_ml_operation_update_type {
	WLAN_ML_OPERATION_UPDATE_TYPE_OPPARAMUPDATE = 0,
	WLAN_ML_OPERATION_UPDATE_TYPE_RESERVEDSTART,
};

/* Definitions for subfields in STA Info field of Per-STA Profile subelement
 * in Reconfiguration variant Multi-Link element Link Info field.
 */

/* STA Info Length */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE             1

/* Size in octets of the AP Removal Timer subfield in STA info field of Per-STA
 * Profile subelement in Reconfiguration variant Multi-Link element Link Info
 * field.
 */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE     2

/**
 * struct wlan_ml_rv_linfo_perstaprof_stainfo_opparams - Operation Parameters in
 * STA info in Per-STA Profile subelement in Reconfiguration variant Multi-Link
 * element Link Info field.
 * @presence_ind: Presence Indication
 * @opparam_info: Operation Parameter Info
 */
struct wlan_ml_rv_linfo_perstaprof_stainfo_opparams {
	uint8_t presence_ind;
	uint16_t opparam_info;
} qdf_packed;

/* Definitions for bits in the Presence Indication subfield in Operation
 * Parameters in STA info in Per-STA Profile subelement in Reconfiguration
 * variant Multi-Link element Link Info field. Any unused bits are reserved.
 */
/* Maximum MPDU Length Present */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_OPPARAMS_PIND_MAXMPDULEN_P  \
	((uint8_t)BIT(0))
/* Maximum A-MSDU Length Present */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_OPPARAMS_PIND_MAXAMSDULEN_P  \
	((uint8_t)BIT(1))

/* Note: The contents are variable in the Operation Parameter Info subfield in
 * Operation Parameters in STA info in Per-STA Profile subelement in
 * Reconfiguration variant Multi-Link element Link Info field. Please refer to
 * the IEEE802.11be standard.
 */

/* Max value in octets of STA Info Length in STA Info field of Per-STA Profile
 * subelement in Reconfiguration variant Multi-Link element Link Info field.
 */
#define WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_LENGTH_MAX \
	(WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE + \
	 QDF_MAC_ADDR_SIZE + \
	 WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE + \
	 sizeof(struct wlan_ml_rv_linfo_perstaprof_stainfo_opparams))

/* End of definitions related to Reconfiguration variant Multi-Link element Link
 * Info field.
 */

/* End of definitions related to Reconfiguration variant Multi-Link element. */

/*
 * Definitions related to MLO specific aspects of Reduced Neighbor Report
 * element.
 */

/*
 * Definitions for MLD Parameters subfield in TBTT Information field present as
 * part of TBTT Information Set in Neighbor AP Information field of Reduced
 * Neighbor Report element.
 */
/* MLD ID */
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_MLDID_IDX                  0
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_MLDID_BITS                 8
/* Link ID */
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_LINKID_IDX                 8
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_LINKID_BITS                4
/* BSS Parameters Change Count */
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_BSSPARAMCHANGECNT_IDX      12
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_BSSPARAMCHANGECNT_BITS     8
/* All Updates Included */
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_ALLUPDATESINC_IDX          20
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_ALLUPDATESINC_BITS         1
/* Disabled link indication */
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_DISABLEDLINKIND_IDX        21
#define WLAN_RNR_NBRAPINFO_TBTTINFO_MLDPARAMS_DISABLEDLINKIND_BITS       1

/*
 * End of definitions related to MLO specific aspects of Reduced Neighbor Report
 * element.
 */
#endif /* WLAN_FEATURE_11BE_MLO */
#endif /* WLAN_FEATURE_11BE */

/**
 * struct wlan_ie_tid_to_link_mapping - TID-to-link mapping IE
 * @elem_id: T2LM IE
 * @elem_len: T2LM IE len
 * @elem_id_extn: T2LM extension id
 * @data: Variable length data described below
 */
struct wlan_ie_tid_to_link_mapping {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t elem_id_extn;
	uint8_t data[];
} qdf_packed;

/* Size in octets of Tid to Link mapping control */
#define WLAN_T2LM_CTRL_SIZE                                     2
/* Size in octets of Mapping switch time size */
#define WLAN_T2LM_MAPPING_SWITCH_TIME_SIZE                      2
/* Size in octets of Expected duration size */
#define WLAN_T2LM_EXPECTED_DURATION_SIZE                        3
/* Size in octets of Link mapping of TID 0-7 size */
#define WLAN_T2LM_LINK_MAPPING_SIZE                             2

/* The variable length data in wlan_ie_tid_to_link_mapping structure has the
 * following fields.
 * - TID-to-link mapping control (2 octet)
 * - Mapping switch time (0 or 2 octet)
 * - Expected duration (0 or 3 octet)
 * - Link mapping presence indicator (0 or 1 octet)
 * - Link mapping of TID 0(optional) to TID 7(optional). Each field has 0 or 1
 *   or 2 octets.
 */

/* Definitions related TID-to-link mapping control*/
/* Direction */
#define WLAN_T2LM_CONTROL_DIRECTION_IDX                         0
#define WLAN_T2LM_CONTROL_DIRECTION_BITS                        2
/* Default link mapping */
#define WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_IDX              2
#define WLAN_T2LM_CONTROL_DEFAULT_LINK_MAPPING_BITS             1
/* Mapping switch time present bit */
#define WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_IDX       3
#define WLAN_T2LM_CONTROL_MAPPING_SWITCH_TIME_PRESENT_BITS      1
/* Expected duration present bit */
#define WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_IDX         4
#define WLAN_T2LM_CONTROL_EXPECTED_DURATION_PRESENT_BITS        1
/* Link Mapping size bit */
#define WLAN_T2LM_CONTROL_LINK_MAPPING_SIZE_IDX                 5
#define WLAN_T2LM_CONTROL_LINK_MAPPING_SIZE_BITS                1
/* Bits 5-7 are reserved */
/* Link mapping presence indicator */
#define WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_IDX   8
#define WLAN_T2LM_CONTROL_LINK_MAPPING_PRESENCE_INDICATOR_BITS  8

/**
 * struct wlan_ie_multi_link_traffic_indication - Multi-link traffic indication
 * element
 * @elem_id: Multi-link traffic indication IE
 * @elem_len: Multi-link traffic indication IE length
 * @elem_id_extn: Multi-link traffic indication extension ID
 * @ml_traffic_ind_control: Multi-link traffic indication control
 * @per_link_traffic_ind_list: Indicates the per-link traffic indication. Each
 *                             bit in the per_link_traffic_ind_list corresponds
 *                             to a link of the MLD.
 */
struct wlan_ie_multi_link_traffic_indication {
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t elem_id_extn;
	uint16_t ml_traffic_ind_control;
	uint16_t per_link_traffic_ind_list[];
} qdf_packed;

/**
 * struct wlan_action - Generic action frame format
 * @category: Action frame category
 * @action: action
 */
struct wlan_action_frame {
	int8_t category;
	int8_t action;
} __packed;

/**
 * struct wlan_action_frame_args - Generic action frame arguments
 * @category: Action frame category
 * @action: action
 * @arg1: argument1
 * @arg2: argument2
 * @arg3: argument3
 * @arg4: Pointer to argument4
 */
struct wlan_action_frame_args {
	uint8_t category;
	uint8_t action;
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;
	uint8_t *arg4;
};

/**
 * struct he_oper_6g_param: 6 Ghz params for HE
 * @primary_channel: HE 6GHz Primary channel number
 * @width: HE 6GHz BSS Channel Width
 * @duplicate_beacon: HE 6GHz Duplicate beacon field
 * @reserved: Reserved bits
 * @chan_freq_seg0: HE 6GHz Channel Centre Frequency Segment 0
 * @chan_freq_seg1: HE 6GHz Channel Centre Frequency Segment 1
 * @minimum_rate: HE 6GHz Minimum Rate
 */
struct he_oper_6g_param {
	uint8_t primary_channel;
	uint8_t width:2,
		duplicate_beacon:1,
		reserved:5;
	uint8_t chan_freq_seg0;
	uint8_t chan_freq_seg1;
	uint8_t minimum_rate;
} qdf_packed;

/**
 * struct wlan_country_ie: country IE
 * @ie: country IE
 * @len: IE len
 * @cc: country code
 */
struct wlan_country_ie {
	uint8_t ie;
	uint8_t len;
	uint8_t cc[3];
} qdf_packed;

/**
 * struct wlan_country_ie: country IE
 * @ie: QBSS IE
 * @len: IE len
 * @station_count: number of station associated
 * @qbss_chan_load: qbss channel load
 * @qbss_load_avail: qbss_load_avail
 */
struct qbss_load_ie {
	uint8_t ie;
	uint8_t len;
	uint16_t station_count;
	uint8_t qbss_chan_load;
	uint16_t qbss_load_avail;
} qdf_packed;

/**
 * struct wlan_bcn_frame: beacon frame fixed params
 * @timestamp: the value of sender's TSFTIMER
 * @beacon_interval: beacon interval
 * @capability: capability
 * @ie: variable IE
 */
struct wlan_bcn_frame {
	uint8_t timestamp[8];
	uint16_t beacon_interval;
	union wlan_capability capability;
	struct ie_header ie;
} qdf_packed;

#define WLAN_TIM_IE_MIN_LENGTH             4

/**
 * struct wlan_tim_ie: tim IE
 * @tim_ie: Time IE
 * @tim_len: TIM IE len
 * @tim_count: dtim count
 * @tim_period: dtim period
 * @tim_bitctl: bitmap control
 * @tim_bitmap: variable length bitmap
 */
struct wlan_tim_ie {
	uint8_t tim_ie;         /* WLAN_ELEMID_TIM */
	uint8_t tim_len;
	uint8_t tim_count;      /* DTIM count */
	uint8_t tim_period;     /* DTIM period */
	uint8_t tim_bitctl;     /* bitmap control */
	uint8_t tim_bitmap[251];  /* variable-length bitmap */
} qdf_packed;

/**
 * struct rsn_mdie: mobility domain IE
 * @rsn_id: RSN IE id
 * @rsn_len: RSN IE len
 * @mobility_domain: mobility domain info
 * @ft_capab: ft capability
 *
 * Reference 9.4.2.47 Mobility Domain element (MDE) of 802.11-2016
 */
struct rsn_mdie {
	uint8_t rsn_id;
	uint8_t rsn_len;
	uint8_t mobility_domain[2];
	uint8_t ft_capab;
} qdf_packed;

/**
 * struct srp_ie: Spatial reuse parameter IE
 * @srp_id: SRP IE id
 * @srp_len: SRP IE len
 * @srp_id_extn: SRP Extension ID
 * @sr_control: sr control
 * @non_srg_obsspd_max_offset: non srg obsspd max offset
 * @srg_obss_pd_min_offset: srg obss pd min offset
 * @srg_obss_pd_max_offset: srg obss pd max offset
 * @srg_bss_color_bitmap: srg bss color bitmap
 * @srg_partial_bssid_bitmap: srg partial bssid bitmap
 */
struct wlan_srp_ie {
	uint8_t srp_id;
	uint8_t srp_len;
	uint8_t srp_id_extn;
	uint8_t sr_control;
	union {
		struct {
			uint8_t non_srg_obsspd_max_offset;
			uint8_t srg_obss_pd_min_offset;
			uint8_t srg_obss_pd_max_offset;
			uint8_t srg_bss_color_bitmap[8];
			uint8_t srg_partial_bssid_bitmap[8];
		} qdf_packed nonsrg_srg_info;
		struct {
			uint8_t non_srg_obsspd_max_offset;
		} qdf_packed nonsrg_info;
		struct {
			uint8_t srg_obss_pd_min_offset;
			uint8_t srg_obss_pd_max_offset;
			uint8_t srg_bss_color_bitmap[8];
			uint8_t srg_partial_bssid_bitmap[8];
		} qdf_packed srg_info;
	};
} qdf_packed;

#define ESP_INFORMATION_LIST_LENGTH 3
#define MAX_ESP_INFORMATION_FIELD 4
/*
 * enum access_category: tells about access category in ESP paramameter
 * @ESP_AC_BK: ESP access category for background
 * @ESP_AC_BE: ESP access category for best effort
 * @ESP_AC_VI: ESP access category for video
 * @ESP_AC_VO: ESP access category for Voice
 */
enum access_category {
	ESP_AC_BK,
	ESP_AC_BE,
	ESP_AC_VI,
	ESP_AC_VO,

};
/*
 * struct wlan_esp_info: structure for Esp information parameter
 * @access_category: access category info
 * @reserved: reserved
 * @data_format: two bits in length and tells about data format
 * i.e. 0 = No aggregation is expected to be performed for MSDUs or MPDUs with
 * the Type subfield equal to Data for the corresponding AC
 * 1 = A-MSDU aggregation is expected to be performed for MSDUs for the
 * corresponding AC, but A-MPDU aggregation is not expected to be performed
 * for MPDUs with the Type subfield equal to Data for the corresponding AC
 * 2 = A-MPDU aggregation is expected to be performed for MPDUs with the Type
 * subfield equal to Data for the corresponding AC, but A-MSDU aggregation is
 * not expected to be performed for MSDUs for the corresponding AC
 * 3 = A-MSDU aggregation is expected to be performed for MSDUs for the
 * corresponding AC and A-MPDU aggregation is expected to be performed for
 * MPDUs with the Type subfield equal to Data for the corresponding AC
 * @ba_window_size: BA Window Size subfield is three bits in length and
 * indicates the size of the Block Ack window that is
 * expected for the corresponding access category
 * @estimated_air_fraction: Estimated Air Time Fraction subfield is 8 bits in
 * length and contains an unsigned integer that represents
 * the predicted percentage of time, linearly scaled with 255 representing
 * 100%, that a new STA joining the
 * BSS will be allocated for PPDUs that contain only
 * MPDUs with the Type
 * subfield equal to Data of the
 * corresponding access category for that STA.
 * @ppdu_duration: Data PPDU Duration Target field
 * is 8 bits in length and is
 * an unsigned integer that indicates the
 * expected target duration of PPDUs that contain only MPDUs with the Type
 * subfield equal to Data for the
 * corresponding access category in units of 50 μs
 */
struct wlan_esp_info {
	uint8_t access_category:2;
	uint8_t reserved:1;
	uint8_t data_format:2;
	uint8_t ba_window_size:3;
	uint8_t estimated_air_fraction;
	uint8_t ppdu_duration;
};

/**
 * struct wlan_esp_ie: struct for ESP information
 * @esp_id: ESP IE id
 * @esp_len: ESP IE len
 * @esp_id_extn: ESP Extension ID
 * @esp_info_AC_BK: ESP information related to BK category
 * @esp_info_AC_BE: ESP information related to BE category
 * @esp_info_AC_VI: ESP information related to VI category
 * @esp_info_AC_VO: ESP information related to VO category
 */
struct wlan_esp_ie {
	uint8_t esp_id;
	uint8_t esp_len;
	uint8_t esp_id_extn;
	struct wlan_esp_info esp_info_AC_BK;
	struct wlan_esp_info esp_info_AC_BE;
	struct wlan_esp_info esp_info_AC_VI;
	struct wlan_esp_info esp_info_AC_VO;
} qdf_packed;

/**
 * struct wlan_ext_cap_ie - struct for extended capabilities information
 * @ext_cap_id: Extended capabilities id
 * @ext_cap_len: Extended capabilities IE len
 * @ext_caps: Variable length extended capabilities information
 */
struct wlan_ext_cap_ie {
	uint8_t ext_cap_id;
	uint8_t ext_cap_len;
	uint8_t ext_caps[];
} qdf_packed;

/* EHT caps fixed field = 2 bytes (EHT mac caps) + 9 bytes (EHT phy caps) */
#define EHT_CAP_FIXED_FIELDS         11
#define EHT_CAP_320M_MCS_MAP_LEN      3
#define EHT_CAP_160M_MCS_MAP_LEN      3
#define EHT_CAP_80M_MCS_MAP_LEN       3
#define EHT_CAP_20M_MCS_MAP_LEN       4
#define EHT_CAP_OUI_LEN               3

/**
 * struct wlan_eht_cap_info - struct for eht capabilities information
 * @EPCS_pri_access: EPCS priority access support
 * @eht_om_ctl: EHT OM control support
 * @triggered_txop_sharing_mode1: Triggered TXOP sharing support mode 1 support
 * @triggered_txop_sharing_mode2: Triggered TXOP sharing mode 2 support
 * @restricted_twt: Restricted TWT support
 * @scs_traffic_desc: SCS traffic description support
 * @max_mpdu_len: Maximum MPDU length
 * @max_a_mpdu_len_exponent_ext: Maximum A-MPDU Length Exponent Extension
 * @eht_trs_support: EHT TRS SUPPORT
 * @txop_return_support_txop_share_m2: TXOP Return Support in TXOP Share Mode 2
 * @two_bqrs_support: Two BQRs Support
 * @eht_link_adaptation_support: EHT Link Adaptation Support
 * @reserved: reserved bits
 * @reserved2: reserved bits
 * @support_320mhz_6ghz: support 320mhz in 6gz
 * @ru_242tone_wt_20mhz: Support For 242-tone RU In BW Wider Than 20 MHz
 * @ndp_4x_eht_ltf_3dot2_us_gi: NDP With 4 EHT-LTF And 3.2 μs GI
 * @partial_bw_mu_mimo: Partial Bandwidth UL MU-MIMO
 * @su_beamformer: SU Beamformer
 * @su_beamformee: SU Beamformer
 * @bfee_ss_le_80mhz: Beamformee SS (≤ 80 MHz)
 * @bfee_ss_160mhz: Beamformee SS (= 160 MHz)
 * @bfee_ss_320mhz: Beamformee SS (= 320 MHz)
 * @num_sounding_dim_le_80mhz: Number Of Sounding Dimensions (≤ 80 MHz)
 * @num_sounding_dim_160mhz: Number Of Sounding Dimensions (= 160 MHz)
 * @num_sounding_dim_320mhz: Number Of Sounding Dimensions (= 320 MHz)
 * @ng_16_su_feedback: Ng = 16 SU Feedback
 * @ng_16_mu_feedback: Ng = 16 MU Feedback
 * @cb_sz_4_2_su_feedback: Codebook Size SU Feedback
 * @cb_sz_7_5_su_feedback: Codebook Size SU Feedback
 * @trig_su_bforming_feedback: Triggered SU Beamforming Feedback
 * @trig_mu_bforming_partial_bw_feedback: Triggered MU Partial
   Beamforming Feedback
 * @triggered_cqi_feedback: Triggered SU Beamforming Feedback
 * @partial_bw_dl_mu_mimo: Partial Bandwidth DL MU-MIMO
 * @psr_based_sr: PSR-based SR Support
 * @power_boost_factor: Power Boost Factor Support
 * @eht_mu_ppdu_4x_ltf_0_8_us_gi: EHT MU PPDU With 4 EHT-LTF And 0.8 μs GI
 * @max_nc: Max Nc
 * @non_trig_cqi_feedback: Non-Triggered CQI Feedback
 * @tx_1024_4096_qam_lt_242_tone_ru: Tx 1024-QAM And 4096-QAM < 242-tone
   RU Support
 * @rx_1024_4096_qam_lt_242_tone_ru: Rx 1024-QAM And 4096-QAM < 242-tone
   RU Support
 * @ppet_present: PPE Thresholds Present
 * @common_nominal_pkt_padding: Common Nominal Packet Padding
 * @max_num_eht_ltf: Maximum Number Of Supported EHT-LTFs
 * @mcs_15: Support Of MCS 15
 * @eht_dup_6ghz: Support Of EHT DUP In 6 GHz
 * @op_sta_rx_ndp_wider_bw_20mhz: Support For 20 MHz Operating STA
   Receiving NDP With Wider Bandwidth
 * @non_ofdma_ul_mu_mimo_le_80mhz: Non-OFDMA UL MU-MIMO (BW ≤ 80 MHz)
 * @non_ofdma_ul_mu_mimo_160mhz: Non-OFDMA UL MU-MIMO (BW ≤ 160 MHz)
 * @non_ofdma_ul_mu_mimo_320mhz: Non-OFDMA UL MU-MIMO (BW ≤ 320 MHz)
 * @mu_bformer_le_80mhz: MU Beamformer (BW ≤ 80 MHz)
 * @mu_bformer_160mhz: MU Beamformer (BW ≤ 160 MHz)
 * @mu_bformer_320mhz: MU Beamformer (BW ≤ 320 MHz)
 * @tb_sounding_feedback_rl: TB sounding feedback rate limit
 * @rx_1k_qam_in_wider_bw_dl_ofdma: Rx 1024-QAM in wider bandwidth DL
 *                                 OFDMA support
 * @rx_4k_qam_in_wider_bw_dl_ofdma: Rx 4096-QAM in wider bandwidth DL
 *                                 OFDMA support
 * @limited_cap_support_20mhz: 20 MHz-Only Limited Capabilities Support
 * @triggered_mu_bf_full_bw_fb_and_dl_mumimo: 20 MHz-Only Triggered MU Beam-
 *                                   forming Full BW Feedback And DL MU-MIMO
 * @mru_support_20mhz: 20 MHz-Only M-RU Support
 * @reserved3: reserved bits
 * @bw_20_rx_max_nss_for_mcs_0_to_7: Max Rx NSS for MCS 0 to 7 (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_0_to_7: Max Tx NSS for MCS 0 to 7 (BW = 20MHz)
 * @bw_20_rx_max_nss_for_mcs_8_and_9: Max Rx NSS for MCS 8 and 9 (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_8_and_9: Max Tx NSS for MCS 8 and 9 (BW = 20MHz)
 * @bw_20_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                      (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                      (BW = 20MHz)
 * @bw_20_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                      (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                      (BW = 20MHz)
 * @bw_le_80_rx_max_nss_for_mcs_0_to_9: Max Rx NSS for MCS 0 to 9 (BW <= 80MHz)
 * @bw_le_80_tx_max_nss_for_mcs_0_to_9: Max Tx NSS for MCS 0 to 9 (BW <= 80MHz)
 * @bw_le_80_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                        (BW <= 80MHz)
 * @bw_le_80_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                        (BW <= 80MHz)
 * @bw_le_80_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                        (BW <= 80MHz)
 * @bw_le_80_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                        (BW <= 80MHz)
 * @bw_160_rx_max_nss_for_mcs_0_to_9: Max Rx NSS for MCS 0 to 9 (BW = 160MHz)
 * @bw_160_tx_max_nss_for_mcs_0_to_9: Max Tx NSS for MCS 0 to 9 (BW = 160MHz)
 * @bw_160_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                      (BW = 160MHz)
 * @bw_160_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                      (BW = 160MHz)
 * @bw_160_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                      (BW = 160MHz)
 * @bw_160_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                      (BW = 160MHz)
 * @bw_320_rx_max_nss_for_mcs_0_to_9: Max Rx NSS for MCS 0 to 9 (BW = 320MHz)
 * @bw_320_tx_max_nss_for_mcs_0_to_9: Max Tx NSS for MCS 0 to 9 (BW = 320MHz)
 * @bw_320_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                      (BW = 320MHz)
 * @bw_320_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                      (BW = 320MHz)
 * @bw_320_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                      (BW = 320MHz)
 * @bw_320_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                      (BW = 320MHz)
 */
struct wlan_eht_cap_info {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint16_t reserved:2;
	uint16_t eht_link_adaptation_support:2;
	uint16_t two_bqrs_support:1;
	uint16_t txop_return_support_txop_share_m2:1;
	uint16_t eht_trs_support:1;
	uint16_t max_a_mpdu_len_exponent_ext:1;
	uint16_t max_mpdu_len:2;
	uint16_t scs_traffic_desc:1;
	uint16_t restricted_twt:1;
	uint16_t triggered_txop_sharing_mode2:1;
	uint16_t triggered_txop_sharing_mode1:1;
	uint16_t eht_om_ctl:1;
	uint16_t epcs_pri_access:1;

	uint32_t triggered_cqi_feedback:1;
	uint32_t trig_mu_bforming_partial_bw_feedback:1;
	uint32_t trig_su_bforming_feedback:1;
	uint32_t cb_sz_7_5_su_feedback:1;
	uint32_t cb_sz_4_2_su_feedback:1;
	uint32_t ng_16_mu_feedback:1;
	uint32_t ng_16_su_feedback:1;
	uint32_t num_sounding_dim_320mhz:3;
	uint32_t num_sounding_dim_160mhz:3;
	uint32_t num_sounding_dim_le_80mhz:3;
	uint32_t bfee_ss_320mhz:3;
	uint32_t bfee_ss_160mhz:3;
	uint32_t bfee_ss_le_80mhz:3;
	uint32_t su_beamformee:1;
	uint32_t su_beamformer:1;
	uint32_t partial_bw_mu_mimo:1;
	uint32_t ndp_4x_eht_ltf_3dot2_us_gi:1;
	uint32_t ru_242tone_wt_20mhz:1;
	uint32_t support_320mhz_6ghz:1;
	uint32_t reserved2:1;

	uint32_t tb_sounding_feedback_rl:1;
	uint32_t mu_bformer_320mhz:1;
	uint32_t mu_bformer_160mhz:1;
	uint32_t mu_bformer_le_80mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_320mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_160mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_le_80mhz:1;
	uint32_t op_sta_rx_ndp_wider_bw_20mhz:1;
	uint32_t eht_dup_6ghz:1;
	uint32_t mcs_15:4;
	uint32_t max_num_eht_ltf:5;
	uint32_t common_nominal_pkt_padding:2;
	uint32_t ppet_present:1;
	uint32_t rx_1024_4096_qam_lt_242_tone_ru:1;
	uint32_t tx_1024_4096_qam_lt_242_tone_ru:1;
	uint32_t non_trig_cqi_feedback:1;
	uint32_t max_nc:4;
	uint32_t eht_mu_ppdu_4x_ltf_0_8_us_gi:1;
	uint32_t power_boost_factor:1;
	uint32_t psr_based_sr:1;
	uint32_t partial_bw_dl_mu_mimo:1;

	uint8_t reserved3:3;
	uint8_t mru_support_20mhz:1;
	uint8_t triggered_mu_bf_full_bw_fb_and_dl_mumimo:1;
	uint8_t limited_cap_support_20mhz:1;
	uint8_t rx_4k_qam_in_wider_bw_dl_ofdma:1;
	uint8_t rx_1k_qam_in_wider_bw_dl_ofdma:1;

	uint32_t bw_20_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_20_tx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_20_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_20_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_20_rx_max_nss_for_mcs_8_and_9:4;
	uint32_t bw_20_tx_max_nss_for_mcs_8_and_9:4;
	uint32_t bw_20_rx_max_nss_for_mcs_0_to_7:4;
	uint32_t bw_20_tx_max_nss_for_mcs_0_to_7:4;

	uint32_t bw_le_80_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_le_80_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_le_80_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_160_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_160_tx_max_nss_for_mcs_12_and_13:4;

	uint32_t bw_160_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_160_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_160_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_160_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_320_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_320_tx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_320_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_320_tx_max_nss_for_mcs_10_and_11:4;

	uint8_t bw_320_rx_max_nss_for_mcs_0_to_9:4;
	uint8_t bw_320_tx_max_nss_for_mcs_0_to_9:4;

#else
	uint16_t epcs_pri_access:1;
	uint16_t eht_om_ctl:1;
	uint16_t triggered_txop_sharing_mode1:1;
	uint16_t triggered_txop_sharing_mode2:1;
	uint16_t restricted_twt:1;
	uint16_t scs_traffic_desc:1;
	uint16_t max_mpdu_len:2;
	uint16_t max_a_mpdu_len_exponent_ext:1;
	uint16_t eht_trs_support:1;
	uint16_t txop_return_support_txop_share_m2:1;
	uint16_t two_bqrs_support:1;
	uint16_t eht_link_adaptation_support:2;
	uint16_t reserved:2;

	uint32_t reserved2:1;
	uint32_t support_320mhz_6ghz:1;
	uint32_t ru_242tone_wt_20mhz:1;
	uint32_t ndp_4x_eht_ltf_3dot2_us_gi:1;
	uint32_t partial_bw_mu_mimo:1;
	uint32_t su_beamformer:1;
	uint32_t su_beamformee:1;
	uint32_t bfee_ss_le_80mhz:3;
	uint32_t bfee_ss_160mhz:3;
	uint32_t bfee_ss_320mhz:3;
	uint32_t num_sounding_dim_le_80mhz:3;
	uint32_t num_sounding_dim_160mhz:3;
	uint32_t num_sounding_dim_320mhz:3;
	uint32_t ng_16_su_feedback:1;
	uint32_t ng_16_mu_feedback:1;
	uint32_t cb_sz_4_2_su_feedback:1;
	uint32_t cb_sz_7_5_su_feedback:1;
	uint32_t trig_su_bforming_feedback:1;
	uint32_t trig_mu_bforming_partial_bw_feedback:1;
	uint32_t triggered_cqi_feedback:1;

	uint32_t partial_bw_dl_mu_mimo:1;
	uint32_t psr_based_sr:1;
	uint32_t power_boost_factor:1;
	uint32_t eht_mu_ppdu_4x_ltf_0_8_us_gi:1;
	uint32_t max_nc:4;
	uint32_t non_trig_cqi_feedback:1;
	uint32_t tx_1024_4096_qam_lt_242_tone_ru:1;
	uint32_t rx_1024_4096_qam_lt_242_tone_ru:1;
	uint32_t ppet_present:1;
	uint32_t common_nominal_pkt_padding:2;
	uint32_t max_num_eht_ltf:5;
	uint32_t mcs_15:4;
	uint32_t eht_dup_6ghz:1;
	uint32_t op_sta_rx_ndp_wider_bw_20mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_le_80mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_160mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_320mhz:1;
	uint32_t mu_bformer_le_80mhz:1;
	uint32_t mu_bformer_160mhz:1;
	uint32_t mu_bformer_320mhz:1;
	uint32_t tb_sounding_feedback_rl:1;

	uint8_t limited_cap_support_20mhz:1;
	uint8_t triggered_mu_bf_full_bw_fb_and_dl_mumimo:1;
	uint8_t mru_support_20mhz:1;
	uint8_t rx_1k_qam_in_wider_bw_dl_ofdma:1;
	uint8_t rx_4k_qam_in_wider_bw_dl_ofdma:1;
	uint8_t reserved3:3;

	uint32_t bw_20_rx_max_nss_for_mcs_0_to_7:4;
	uint32_t bw_20_tx_max_nss_for_mcs_0_to_7:4;
	uint32_t bw_20_rx_max_nss_for_mcs_8_and_9:4;
	uint32_t bw_20_tx_max_nss_for_mcs_8_and_9:4;
	uint32_t bw_20_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_20_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_20_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_20_tx_max_nss_for_mcs_12_and_13:4;

	uint32_t bw_le_80_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_le_80_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_le_80_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_12_and_13:4;

	uint32_t bw_160_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_160_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_160_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_160_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_160_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_160_tx_max_nss_for_mcs_12_and_13:4;

	uint32_t bw_320_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_320_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_320_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_320_tx_max_nss_for_mcs_10_and_11:4;
	uint8_t bw_320_rx_max_nss_for_mcs_12_and_13:4;
	uint8_t bw_320_tx_max_nss_for_mcs_12_and_13:4;
#endif
} qdf_packed;

/**
 * wlan_eht_cap_info_network_endian - struct for eht capabilities information
 * @epcs_pri_access: EPCS priority access support
 * @eht_om_ctl: EHT OM control support
 * @triggered_txop_sharing_mode1: Triggered TXOP sharing mode 1 support
 * @triggered_txop_sharing_mode2: Triggered TXOP sharing mode 2 support
 * @restricted_twt: Restricted TWT support
 * @scs_traffic_desc: SCS traffic description support
 * @max_mpdu_len: Maximum MPDU length
 * @max_a_mpdu_len_exponent_ext: Maximum A-MPDU Length Exponent Extension
 * @eht_trs_support: EHT TRS SUPPORT
 * @txop_return_support_txop_share_m2: TXOP Return Support in TXOP Share Mode 2
 * @two_bqrs_support: Two BQRs Support
 * @eht_link_adaptation_support: EHT Link Adaptation Support
 * @reserved: reserved bits
 * @reserved2: reserved bits
 * @support_320mhz_6ghz: support 320mhz in 6gz
 * @ru_242tone_wt_20mhz: Support For 242-tone RU In BW Wider Than 20 MHz
 * @ndp_4x_eht_ltf_3dot2_us_gi: NDP With 4 EHT-LTF And 3.2 μs GI
 * @partial_bw_mu_mimo: Partial Bandwidth UL MU-MIMO
 * @su_beamformer: SU Beamformer
 * @su_beamformee: SU Beamformer
 * @bfee_ss_le_80mhz: Beamformee SS (≤ 80 MHz)
 * @bfee_ss_160mhz: Beamformee SS (= 160 MHz)
 * @bfee_ss_320mhz: Beamformee SS (= 320 MHz)
 * @num_sounding_dim_le_80mhz: Number Of Sounding Dimensions (≤ 80 MHz)
 * @num_sounding_dim_160mhz: Number Of Sounding Dimensions (= 160 MHz)
 * @num_sounding_dim_320mhz: Number Of Sounding Dimensions (= 320 MHz)
 * @ng_16_su_feedback: Ng = 16 SU Feedback
 * @ng_16_mu_feedback: Ng = 16 MU Feedback
 * @cb_sz_4_2_su_feedback: Codebook Size SU Feedback
 * @cb_sz_7_5_su_feedback: Codebook Size SU Feedback
 * @trig_su_bforming_feedback: Triggered SU Beamforming Feedback
 * @trig_mu_bforming_partial_bw_feedback: Triggered MU Partial
   Beamforming Feedback
 * @triggered_cqi_feedback: Triggered SU Beamforming Feedback
 * @partial_bw_dl_mu_mimo: Partial Bandwidth DL MU-MIMO
 * @psr_based_sr: PSR-based SR Support
 * @power_boost_factor: Power Boost Factor Support
 * @eht_mu_ppdu_4x_ltf_0_8_us_gi: EHT MU PPDU With 4 EHT-LTF And 0.8 μs GI
 * @max_nc: Max Nc
 * @non_trig_cqi_feedback: Non-Triggered CQI Feedback
 * @tx_1024_4096_qam_lt_242_tone_ru: Tx 1024-QAM And 4096-QAM < 242-tone
   RU Support
 * @rx_1024_4096_qam_lt_242_tone_ru: Rx 1024-QAM And 4096-QAM < 242-tone
   RU Support
 * @ppet_present: PPE Thresholds Present
 * @common_nominal_pkt_padding: Common Nominal Packet Padding
 * @max_num_eht_ltf: Maximum Number Of Supported EHT-LTFs
 * @mcs_15: Support Of MCS 15
 * @eht_dup_6ghz: Support Of EHT DUP In 6 GHz
 * @op_sta_rx_ndp_wider_bw_20mhz: Support For 20 MHz Operating STA
   Receiving NDP With Wider Bandwidth
 * @non_ofdma_ul_mu_mimo_le_80mhz: Non-OFDMA UL MU-MIMO (BW ≤ 80 MHz)
 * @non_ofdma_ul_mu_mimo_160mhz: Non-OFDMA UL MU-MIMO (BW ≤ 160 MHz)
 * @non_ofdma_ul_mu_mimo_320mhz: Non-OFDMA UL MU-MIMO (BW ≤ 320 MHz)
 * @mu_bformer_le_80mhz: MU Beamformer (BW ≤ 80 MHz)
 * @mu_bformer_160mhz: MU Beamformer (BW ≤ 160 MHz)
 * @mu_bformer_320mhz: MU Beamformer (BW ≤ 320 MHz)
 * @tb_sounding_feedback_rl: TB sounding feedback rate limit
 * @rx_1k_qam_in_wider_bw_dl_ofdma: Rx 1024-QAM in wider bandwidth DL
 *                                 OFDMA support
 * @rx_4k_qam_in_wider_bw_dl_ofdma: Rx 4096-QAM in wider bandwidth DL
 *                                 OFDMA support
 * @limited_cap_support_20mhz: 20 MHz-Only Limited Capabilities Support
 * @triggered_mu_bf_full_bw_fb_and_dl_mumimo: 20 MHz-Only Triggered MU Beam-
 *                                   forming Full BW Feedback And DL MU-MIMO
 * @mru_support_20mhz: 20 MHz-Only M-RU Support
 * @reserved3: reserved bits
 * @bw_20_rx_max_nss_for_mcs_0_to_7: Max Rx NSS for MCS 0 to 7 (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_0_to_7: Max Tx NSS for MCS 0 to 7 (BW = 20MHz)
 * @bw_20_rx_max_nss_for_mcs_8_and_9: Max Rx NSS for MCS 8 and 9 (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_8_and_9: Max Tx NSS for MCS 8 and 9 (BW = 20MHz
 * @bw_20_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                      (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                      (BW = 20MHz)
 * @bw_20_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                      (BW = 20MHz)
 * @bw_20_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                      (BW = 20MHz)
 * @bw_le_80_rx_max_nss_for_mcs_0_to_9: Max Rx NSS for MCS 0 to 9 (BW <= 80MHz)
 * @bw_le_80_tx_max_nss_for_mcs_0_to_9: Max Tx NSS for MCS 0 to 9 (BW <= 80MHz)
 * @bw_le_80_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                        (BW <= 80MHz)
 * @bw_le_80_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                        (BW <= 80MHz)
 * @bw_le_80_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                        (BW <= 80MHz)
 * @bw_le_80_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                        (BW <= 80MHz)
 * @bw_160_rx_max_nss_for_mcs_0_to_9: Max Rx NSS for MCS 0 to 9 (BW = 160MHz)
 * @bw_160_tx_max_nss_for_mcs_0_to_9: Max Tx NSS for MCS 0 to 9 (BW = 160MHz)
 * @bw_160_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                      (BW = 160MHz)
 * @bw_160_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                      (BW = 160MHz)
 * @bw_160_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                      (BW = 160MHz)
 * @bw_160_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                      (BW = 160MHz)
 * @bw_320_rx_max_nss_for_mcs_0_to_9: Max Rx NSS for MCS 0 to 9 (BW = 320MHz)
 * @bw_320_tx_max_nss_for_mcs_0_to_9: Max Tx NSS for MCS 0 to 9 (BW = 320MHz)
 * @bw_320_rx_max_nss_for_mcs_10_and_11: Max Rx NSS for MCS 10 and 11
 *                                      (BW = 320MHz)
 * @bw_320_tx_max_nss_for_mcs_10_and_11: Max Tx NSS for MCS 10 and 11
 *                                      (BW = 320MHz)
 * @bw_320_rx_max_nss_for_mcs_12_and_13: Max Rx NSS for MCS 12 and 13
 *                                      (BW = 320MHz)
 * @bw_320_tx_max_nss_for_mcs_12_and_13: Max Tx NSS for MCS 12 and 13
 *                                      (BW = 320MHz)
 */
struct wlan_eht_cap_info_network_endian {
	uint16_t epcs_pri_access:1;
	uint16_t eht_om_ctl:1;
	uint16_t triggered_txop_sharing_mode1:1;
	uint16_t triggered_txop_sharing_mode2:1;
	uint16_t restricted_twt:1;
	uint16_t scs_traffic_desc:1;
	uint16_t max_mpdu_len:2;
	uint16_t max_a_mpdu_len_exponent_ext:1;
	uint16_t eht_trs_support:1;
	uint16_t txop_return_support_txop_share_m2:1;
	uint16_t two_bqrs_support:1;
	uint16_t eht_link_adaptation_support:2;
	uint16_t reserved:2;

	uint32_t reserved2:1;
	uint32_t support_320mhz_6ghz:1;
	uint32_t ru_242tone_wt_20mhz:1;
	uint32_t ndp_4x_eht_ltf_3dot2_us_gi:1;
	uint32_t partial_bw_mu_mimo:1;
	uint32_t su_beamformer:1;
	uint32_t su_beamformee:1;
	uint32_t bfee_ss_le_80mhz:3;
	uint32_t bfee_ss_160mhz:3;
	uint32_t bfee_ss_320mhz:3;
	uint32_t num_sounding_dim_le_80mhz:3;
	uint32_t num_sounding_dim_160mhz:3;
	uint32_t num_sounding_dim_320mhz:3;
	uint32_t ng_16_su_feedback:1;
	uint32_t ng_16_mu_feedback:1;
	uint32_t cb_sz_4_2_su_feedback:1;
	uint32_t cb_sz_7_5_su_feedback:1;
	uint32_t trig_su_bforming_feedback:1;
	uint32_t trig_mu_bforming_partial_bw_feedback:1;
	uint32_t triggered_cqi_feedback:1;

	uint32_t partial_bw_dl_mu_mimo:1;
	uint32_t psr_based_sr:1;
	uint32_t power_boost_factor:1;
	uint32_t eht_mu_ppdu_4x_ltf_0_8_us_gi:1;
	uint32_t max_nc:4;
	uint32_t non_trig_cqi_feedback:1;
	uint32_t tx_1024_4096_qam_lt_242_tone_ru:1;
	uint32_t rx_1024_4096_qam_lt_242_tone_ru:1;
	uint32_t ppet_present:1;
	uint32_t common_nominal_pkt_padding:2;
	uint32_t max_num_eht_ltf:5;
	uint32_t mcs_15:4;
	uint32_t eht_dup_6ghz:1;
	uint32_t op_sta_rx_ndp_wider_bw_20mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_le_80mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_160mhz:1;
	uint32_t non_ofdma_ul_mu_mimo_320mhz:1;
	uint32_t mu_bformer_le_80mhz:1;
	uint32_t mu_bformer_160mhz:1;
	uint32_t mu_bformer_320mhz:1;
	uint32_t tb_sounding_feedback_rl:1;

	uint8_t rx_1k_qam_in_wider_bw_dl_ofdma:1;
	uint8_t rx_4k_qam_in_wider_bw_dl_ofdma:1;
	uint8_t mru_support_20mhz:1;
	uint8_t triggered_mu_bf_full_bw_fb_and_dl_mumimo:1;
	uint8_t limited_cap_support_20mhz:1;
	uint8_t reserved3:3;

	uint32_t bw_20_rx_max_nss_for_mcs_0_to_7:4;
	uint32_t bw_20_tx_max_nss_for_mcs_0_to_7:4;
	uint32_t bw_20_rx_max_nss_for_mcs_8_and_9:4;
	uint32_t bw_20_tx_max_nss_for_mcs_8_and_9:4;
	uint32_t bw_20_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_20_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_20_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_20_tx_max_nss_for_mcs_12_and_13:4;

	uint32_t bw_le_80_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_le_80_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_le_80_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_le_80_tx_max_nss_for_mcs_12_and_13:4;

	uint32_t bw_160_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_160_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_160_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_160_tx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_160_rx_max_nss_for_mcs_12_and_13:4;
	uint32_t bw_160_tx_max_nss_for_mcs_12_and_13:4;

	uint32_t bw_320_rx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_320_tx_max_nss_for_mcs_0_to_9:4;
	uint32_t bw_320_rx_max_nss_for_mcs_10_and_11:4;
	uint32_t bw_320_tx_max_nss_for_mcs_10_and_11:4;
	uint8_t bw_320_rx_max_nss_for_mcs_12_and_13:4;
} qdf_packed;

/**
 * struct edca_param: struct for edca_param
 * @acvo_aifsn: ac vo aifsn
 * @acvo_acm: ac vo acm
 * @acvo_aci: ac vo aci
 * @unused: unused bit
 * @acvo_cwmin: ac vo cwmin
 * @acvo_cwmax: ac vo cwmax
 * @acvo_txoplimit: ac vo txoplimit
 */
struct edca_param {
	uint8_t acvo_aifsn:4;
	uint8_t acvo_acm:1;
	uint8_t acvo_aci:2;
	uint8_t unused:1;
	uint8_t acvo_cwmin:4;
	uint8_t acvo_cwmax:4;
	uint16_t acvo_txoplimit;
};

/**
 * struct pifs_param: struct for pifs_param
 * @sap_pifs_offset: sap pifs offset
 * @leb_pifs_offset: left earbud offset
 * @reb_pifs_offset: right earbud offset
 */
struct pifs_param {
	uint8_t sap_pifs_offset;
	uint8_t leb_pifs_offset;
	uint8_t reb_pifs_offset;
};

/**
 * struct wlan_edca_pifs_param_ie: struct for QCN_ATTRIB_EDCA_PIFS_PARAM
 * @edca_param_type: edca param type
 * @eparam: structure for edca_param
 * @pparam: structure for pifs_param
 */
struct wlan_edca_pifs_param_ie {
	uint8_t edca_param_type;
	union {
		struct edca_param eparam; /* edca_param_type = 0 */
		struct pifs_param pparam; /* edca_param_type = 1 */
	} qdf_packed edca_pifs_param;
} qdf_packed;

/**
 * struct csa_ie: Channel Switch Announcement IE
 * @id: CSA IE
 * @len: CSA IE len
 * @switch_mode: Channel Switch Mode
 * @new_channel: New channel to which CSA is announced
 * @tbtt_count: CSA count in beacon intervals
 */
struct csa_ie {
	uint8_t id;
	uint8_t len;
	uint8_t switch_mode;
	uint8_t new_channel;
	uint8_t tbtt_count;
} qdf_packed;

/**
 * struct xcsa_ie: Extended Channel Switch Announcement IE
 * @id: CSA IE
 * @len: CSA IE len
 * @switch_mode: Channel Switch Mode
 * @new_class: New operating class
 * @new_channel: New channel to which CSA is announced
 * @tbtt_count: CSA count in beacon intervals
 */
struct xcsa_ie {
	uint8_t id;
	uint8_t len;
	uint8_t switch_mode;
	uint8_t new_class;
	uint8_t new_channel;
	uint8_t tbtt_count;
} qdf_packed;

/**
 * struct oce_reduced_wan_metrics: struct for oce wan metrics
 * @downlink_av_cap: Download available capacity
 * @uplink_av_cap: Upload available capacity
 */
struct oce_reduced_wan_metrics {
	uint8_t downlink_av_cap:4;
	uint8_t uplink_av_cap:4;
};

/**
 * is_wpa_oui() - If vendor IE is WPA type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is WPA
 *
 * Return: true if its WPA IE
 */
static inline bool
is_wpa_oui(uint8_t *frm)
{
	return (frm[1] > 3) && (LE_READ_4(frm + 2) ==
		((WLAN_WPA_OUI_TYPE << 24) | WLAN_WPA_OUI));
}

/**
 * is_wps_oui() - If vendor IE is WPS type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is WPS
 *
 * Return: true if its WPS IE
 */
static inline bool
is_wps_oui(const uint8_t *frm)
{
	return frm[1] > 3 && BE_READ_4(frm + 2) == WSC_OUI;
}

/**
 * is_mbo_oce_oui() - If vendor IE is MBO/OCE type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is MBO/OCE
 *
 * Return: true if its MBO/OCE IE
 */
static inline bool
is_mbo_oce_oui(const uint8_t *frm)
{
	return frm[1] > 3 && BE_READ_4(frm + 2) == MBO_OCE_OUI;
}

/**
 * is_wcn_oui() - If vendor IE is WCN type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is WCN
 *
 * Return: true if its WCN IE
 */
static inline bool
is_wcn_oui(uint8_t *frm)
{
	return (frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((WCN_OUI_TYPE << 24) | WCN_OUI));
}

static inline bool
is_qcn_oui(uint8_t *frm)
{
	return ((frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((QCN_OUI_TYPE_CMN << 24) | QCA_OUI)));
}

#define WLAN_VENDOR_WME_IE_LEN 24
/**
 * is_wme_param() - If vendor IE is WME param type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is WME param
 *
 * Return: true if its WME param IE
 */
static inline bool
is_wme_param(const uint8_t *frm)
{
	return (frm[1] > 5) && (LE_READ_4(frm + 2) ==
			((WME_OUI_TYPE << 24) | WME_OUI)) &&
			(frm[6] == WME_PARAM_OUI_SUBTYPE);
}

/**
 * is_wme_info() - If vendor IE is WME info type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is WME info
 *
 * Return: true if its WME info IE
 */
static inline bool
is_wme_info(const uint8_t *frm)
{
	return (frm[1] > 5) && (LE_READ_4(frm + 2) ==
		((WME_OUI_TYPE << 24) | WME_OUI)) &&
		(frm[6] == WME_INFO_OUI_SUBTYPE);
}

#define WLAN_VENDOR_ATHCAPS_IE_LEN 9
/**
 * is_atheros_oui() - If vendor IE is Atheros type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is Atheros
 *
 * Return: true if its Atheros IE
 */
static inline bool
is_atheros_oui(const uint8_t *frm)
{
	return (frm[1] > 3) && LE_READ_4(frm + 2) ==
		((ATH_OUI_TYPE << 24) | ATH_OUI);
}

#define WLAN_VENDOR_ATH_EXTCAP_IE_LEN 10
/**
 * is_atheros_extcap_oui() - If vendor IE is Atheros ext cap
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is Atheros ext cap
 *
 * Return: true if its Atheros ext cap IE
 */
static inline int
is_atheros_extcap_oui(uint8_t *frm)
{
	return (frm[1] > 3) && (LE_READ_4(frm + 2) ==
		((ATH_OUI_EXTCAP_TYPE << 24) | ATH_OUI));
}

#define WLAN_VENDOR_SFA_IE_LEN 5
/**
 * is_sfa_oui() - If vendor IE is SFA type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is SFA
 *
 * Return: true if its SFA IE
 */
static inline bool
is_sfa_oui(uint8_t *frm)
{
	return (frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((SFA_OUI_TYPE << 24) | SFA_OUI));
}

/**
 * is_p2p_oui() - If vendor IE is P2P type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is P2P
 *
 * Return: true if its P2P IE
 */
static inline bool
is_p2p_oui(const uint8_t *frm)
{
	const uint8_t wfa_oui[3] = P2P_WFA_OUI;

	return (frm[1] >= 4) &&
		(frm[2] == wfa_oui[0]) &&
		(frm[3] == wfa_oui[1]) &&
		(frm[4] == wfa_oui[2]) &&
		(frm[5] == P2P_WFA_VER);
}

/**
 * is_qca_son_oui() - If vendor IE is QCA WHC type
 * @frm: vendor IE pointer
 * @whc_subtype: subtype
 *
 * API to check if vendor IE is QCA WHC
 *
 * Return: true if its QCA WHC IE
 */
static inline bool
is_qca_son_oui(uint8_t *frm, uint8_t whc_subtype)
{
	return (frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((QCA_OUI_WHC_TYPE << 24) | QCA_OUI)) &&
		(*(frm + 6) == whc_subtype);
}

/**
 * is_qca_son_rept_oui() - If vendor IE is QCA WHC repeater type
 * @frm: vendor IE pointer
 * @whc_subtype: subtype
 *
 * API to check if vendor IE is QCA WHC REPT
 *
 * Return: true if its QCA WHC REPT IE
 */
static inline bool
is_qca_son_rept_oui(u_int8_t *frm, u_int8_t whc_subtype)
{
	return ((frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((QCA_OUI_WHC_REPT_TYPE << 24) | QCA_OUI)) &&
		(*(frm + 6) == whc_subtype));
}

/**
 * is_ht_cap() - If vendor IE is vendor HT cap type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is vendor HT cap
 *
 * Return: true if its vendor HT cap IE
 */
static inline bool
is_ht_cap(uint8_t *frm)
{
	return (frm[1] > 3) && (BE_READ_4(frm + 2) ==
		((VENDOR_HT_OUI << 8) | VENDOR_HT_CAP_ID));
}

/**
 * is_ht_info() - If vendor IE is vendor HT info type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is vendor HT info
 *
 * Return: true if its vendor HT info IE
 */
static inline bool
is_ht_info(uint8_t *frm)
{
	return (frm[1] > 3) && (BE_READ_4(frm + 2) ==
		((VENDOR_HT_OUI << 8) | VENDOR_HT_INFO_ID));
}

/**
 * is_interop_vht() - If vendor IE is VHT interop
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is VHT interop
 *
 * Return: true if its VHT interop IE
 */
static inline bool
is_interop_vht(uint8_t *frm)
{
	return (frm[1] > 12) && (BE_READ_4(frm + 2) ==
		((VHT_INTEROP_OUI << 8) | VHT_INTEROP_TYPE)) &&
		((*(frm + 6) == VHT_INTEROP_OUI_SUBTYPE) ||
		(*(frm + 6) == VHT_INTEROP_OUI_SUBTYPE_VENDORSPEC));
}

/**
 * is_bwnss_oui() - If vendor IE is BW NSS type
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is BW NSS
 *
 * Return: true if its BW NSS IE
 */
static inline bool
is_bwnss_oui(uint8_t *frm)
{
	return (frm[1] > 3) && (LE_READ_4(frm + 2) ==
		((ATH_OUI_BW_NSS_MAP_TYPE << 24) | ATH_OUI));
}

#define WLAN_BWNSS_MAP_OFFSET 7

/**
 * is_he_cap_oui() - If vendor IE is HE CAP OUI
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is HE CAP
 *
 * Return: true if its HE CAP IE
 */
static inline bool
is_he_cap_oui(uint8_t *frm)
{
	return (frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((ATH_HE_CAP_SUBTYPE << 24) | ATH_HE_OUI));
}

/**
 * is_he_op_oui() - If vendor IE is HE OP OUI
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is HE OP OUT
 *
 * Return: true if its HE OP OUI
 */
static inline bool
is_he_op_oui(uint8_t *frm)
{
	return (frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((ATH_HE_OP_SUBTYPE << 24) | ATH_HE_OUI));
}

/**
 * is_extender_oui() - If vendor IE is EXTENDER OUI
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is EXTENDER OUI
 *
 * Return: true if its EXTENDER OUI
 */
static inline bool
is_extender_oui(uint8_t *frm)
{
	return (frm[1] > 4) && (LE_READ_4(frm + 2) ==
		((QCA_OUI_EXTENDER_TYPE << 24) | QCA_OUI));
}

/**
 * is_adaptive_11r_oui() - Function to check if vendor IE is ADAPTIVE 11R OUI
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is ADAPTIVE 11R OUI
 *
 * Return: true if its ADAPTIVE 11r OUI
 */
static inline bool
is_adaptive_11r_oui(uint8_t *frm)
{
	return (frm[1] > OUI_LENGTH) && (LE_READ_4(frm + 2) ==
		((ADAPTIVE_11R_OUI_TYPE << OUI_TYPE_BITS) | ADAPTIVE_11R_OUI));
}

/**
 * is_sae_single_pmk_oui() - Fun to check if vendor IE is sae single pmk OUI
 * @frm: vendor IE pointer
 *
 * API to check if vendor IE is sae single pmk OUI
 *
 * Return: true if its sae single pmk OUI
 */
static inline bool
is_sae_single_pmk_oui(uint8_t *frm)
{
	return (frm[1] > OUI_LENGTH) && (LE_READ_4(frm + 2) ==
		((SAE_SINGLE_PMK_TYPE << OUI_TYPE_BITS) | SAE_SINGLE_PMK_OUI));
}

/**
 * wlan_parse_oce_reduced_wan_metrics_ie() - parse oce wan metrics
 * @mbo_oce_ie: MBO/OCE ie ptr
 * @wan_metrics: out structure for the reduced wan metric
 *
 * API, function to parse reduced wan metric
 *
 * Return: true if oce wan metrics is present
 */
static inline bool
wlan_parse_oce_reduced_wan_metrics_ie(uint8_t *mbo_oce_ie,
	struct oce_reduced_wan_metrics *wan_metrics)
{
	uint8_t len, attribute_len, attribute_id;
	uint8_t *ie;

	if (!mbo_oce_ie)
		return false;

	ie = mbo_oce_ie;
	len = ie[1];
	ie += 2;

	if (len <= MBO_OCE_OUI_SIZE)
		return false;

	ie += MBO_OCE_OUI_SIZE;
	len -= MBO_OCE_OUI_SIZE;

	while (len > 2) {
		attribute_id = ie[0];
		attribute_len = ie[1];
		len -= 2;
		if (attribute_len > len)
			return false;

		if (attribute_id == REDUCED_WAN_METRICS_ATTR) {
			wan_metrics->downlink_av_cap = ie[2] & 0xff;
			wan_metrics->uplink_av_cap = ie[2] >> 4;
			return true;
		}

		ie += (attribute_len + 2);
		len -= attribute_len;
	}

	return false;
}

/**
 * wlan_parse_oce_subnet_id_ie() - parse oce subnet id IE
 * @mbo_oce_ie: MBO/OCE IE pointer
 *
 * While parsing vendor IE, is_mbo_oce_oui() API does sanity of
 * length and attribute ID for MBO_OCE_OUI and after passing the
 * sanity only mbo_oce IE is stored in scan cache.
 * It is a callers responsibility to get the mbo_oce_ie pointer
 * using util_scan_entry_mbo_oce() API, which points to mbo_oce
 * stored in scan cache. Thus caller is responsible for ensuring
 * the length of the IE is consistent with the embedded length.
 *
 * Return: true if oce subnet id is present, else false
 */
static inline bool
wlan_parse_oce_subnet_id_ie(uint8_t *mbo_oce_ie)
{
	uint8_t len, attribute_len, attribute_id;
	uint8_t *ie;

	if (!mbo_oce_ie)
		return false;

	ie = mbo_oce_ie;
	len = ie[1];
	ie += 2;

	if (len <= MBO_OCE_OUI_SIZE)
		return false;

	ie += MBO_OCE_OUI_SIZE;
	len -= MBO_OCE_OUI_SIZE;

	while (len > 2) {
		attribute_id = ie[0];
		attribute_len = ie[1];
		len -= 2;
		if (attribute_len > len)
			return false;

		if (attribute_id == OCE_SUBNET_ID_ATTR)
			return true;

		ie += (attribute_len + 2);
		len -= attribute_len;
	}

	return false;
}

/**
 * wlan_parse_oce_assoc_disallowed_ie() - parse oce assoc disallowed IE
 * @mbo_oce_ie: MBO/OCE ie ptr
 * @reason: reason for disallowing assoc.
 *
 * API, function to parse OCE assoc disallowed param from the OCE MBO IE
 *
 * Return: true if assoc disallowed field is present in the IE
 */
static inline bool
wlan_parse_oce_assoc_disallowed_ie(uint8_t *mbo_oce_ie, uint8_t *reason)
{
	uint8_t len, attribute_len, attribute_id;
	uint8_t *ie;

	if (!mbo_oce_ie)
		return false;

	ie = mbo_oce_ie;
	len = ie[1];
	ie += 2;

	if (len <= MBO_OCE_OUI_SIZE)
		return false;

	ie += MBO_OCE_OUI_SIZE;
	len -= MBO_OCE_OUI_SIZE;

	while (len > 2) {
		attribute_id = ie[0];
		attribute_len = ie[1];
		len -= 2;
		if (attribute_len > len)
			return false;

		if (attribute_id == OCE_DISALLOW_ASSOC_ATTR) {
			*reason = ie[2];
			return true;
		}

		ie += (attribute_len + 2);
		len -= attribute_len;
	}

	return false;
}

/*
 * wlan_parse_oce_ap_tx_pwr_ie() - parse oce ap tx pwr
 * @mbo_oce_ie: MBO/OCE ie ptr
 * @ap_tx_pwr: pointer to hold value of ap_tx_pwr in dbm
 *
 * Return: true if oce ap tx pwr is present, else false
 */
static inline bool
wlan_parse_oce_ap_tx_pwr_ie(uint8_t *mbo_oce_ie, int8_t *ap_tx_pwr_dbm)
{
	uint8_t len, attribute_len, attribute_id;
	uint8_t *ie;
	int8_t ap_tx_power_in_2_complement;

	if (!mbo_oce_ie)
		return false;

	ie = mbo_oce_ie;
	len = ie[1];
	ie += 2;

	if (len <= MBO_OCE_OUI_SIZE)
		return false;

	ie += MBO_OCE_OUI_SIZE;
	len -= MBO_OCE_OUI_SIZE;

	while (len > 2) {
		attribute_id = ie[0];
		attribute_len = ie[1];
		len -= 2;
		if (attribute_len > len)
			return false;

		if (attribute_id == AP_TX_PWR_ATTR) {
			ap_tx_power_in_2_complement = ie[2];
			*ap_tx_pwr_dbm =
				(int8_t)(256 - ap_tx_power_in_2_complement);
			return true;
		}

		ie += (attribute_len + 2);
		len -= attribute_len;
	}

	return false;
}

/**
 * enum mlme_csa_event_ies_present_flag - IE present flag in CSA event
 * @MLME_CSA_IE_PRESENT: CSA IE is present
 * @MLME_XCSA_IE_PRESENT: extend CSA IE is present
 * @MLME_WBW_IE_PRESENT: wide bandwidth channel switch IE is present
 * @MLME_CSWRAP_IE_EXTENDED_PRESENT: channel switch wrapper IE is present
 * @MLME_CSWRAP_IE_EXT_V2_PRESENT: channel switch wrapper IE V2 is present
 */
enum mlme_csa_event_ies_present_flag {
	MLME_CSA_IE_PRESENT    = 0x00000001,
	MLME_XCSA_IE_PRESENT   = 0x00000002,
	MLME_WBW_IE_PRESENT    = 0x00000004,
	MLME_CSWRAP_IE_EXTENDED_PRESENT = 0x00000008,
	MLME_CSWRAP_IE_EXT_V2_PRESENT    = 0x00000010,
};

/**
 * struct csa_offload_params - STA CSA offload request parameters
 * @channel: channel
 * @switch_mode: switch mode
 * @sec_chan_offset: secondary channel offset
 * @new_ch_width: new channel width
 * @new_punct_bitmap: new puncture bitmap based on new channel width
 * @new_ch_freq_seg1: channel center freq 1
 * @new_ch_freq_seg2: channel center freq 2
 * @ies_present_flag: BIT MAP of MLME_CSA_EVENT_IES_PRESENT_FLAG
 * @bssid: BSSID which triggers CSA
 */
struct csa_offload_params {
	uint8_t channel;
	uint32_t csa_chan_freq;
	uint8_t switch_mode;
	uint8_t sec_chan_offset;
	enum phy_ch_width new_ch_width;
#ifdef WLAN_FEATURE_11BE
	uint16_t new_punct_bitmap;
#endif
	uint8_t new_op_class;
	uint8_t new_ch_freq_seg1;
	uint8_t new_ch_freq_seg2;
	uint32_t ies_present_flag;
	struct qdf_mac_addr bssid;
};
#endif /* _WLAN_CMN_IEEE80211_DEFS_H_ */
