/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wma_eht.c
 *
 * WLAN Host Device Driver 802.11be - Extremely High Throughput Implementation
 */

#include "wma_eht.h"
#include "wmi_unified.h"
#include "service_ready_param.h"
#include "target_if.h"
#include "wma_internal.h"

#if defined(WLAN_FEATURE_11BE)
/* MCS Based EHT rate table */
/* MCS parameters with Nss = 1*/
static const struct index_eht_data_rate_type eht_mcs_nss1[] = {
/* MCS,  {dcm0:0.8/1.6/3.2}, {dcm1:0.8/1.6/3.2} */
	{0,  {{86,   81,   73}, {0} }, /* EHT20 */
	     {{172,  163,  146}, {0} }, /* EHT40 */
	     {{360,  340,  306}, {0} }, /* EHT80 */
	     {{721,  681,  613}, {0} }, /* EHT160 */
	     {{1441,  1361,  1225}, {0}}} , /* EHT320 */
	{1,  {{172,  163,  146 }, {0} },
	     {{344,  325,  293 }, {0} },
	     {{721,  681,  613 }, {0} },
	     {{1441, 1361, 1225}, {0} },
	     {{2882, 2722, 2450}, {0} } } ,
	{2,  {{258,  244,  219 }, {0} },
	     {{516,  488,  439 }, {0} },
	     {{1081, 1021, 919 }, {0} },
	     {{2162, 2042, 1838}, {0} },
	     {{4324, 4083, 3675}, {0} } } ,
	{3,  {{344,  325,  293 }, {0} },
	     {{688,  650,  585 }, {0} },
	     {{1441, 1361, 1225}, {0} },
	     {{2882, 2722, 2450}, {0} },
	     {{5765, 5444, 4900}, {0} } } ,
	{4,  {{516,  488,  439 }, {0} },
	     {{1032, 975,  878 }, {0} },
	     {{2162, 2042, 1838}, {0} },
	     {{4324, 4083, 3675}, {0} },
	     {{8647, 8167, 7350}, {0} } } ,
	{5,  {{688,  650,  585 }, {0} },
	     {{1376, 1300, 1170}, {0} },
	     {{2882, 2722, 2450}, {0} },
	     {{5765, 5444, 4900}, {0} },
	     {{11529, 10889, 9800}, {0}}} ,
	{6,  {{774,  731,  658 }, {0} },
	     {{1549, 1463, 1316}, {0} },
	     {{3243, 3063, 2756}, {0} },
	     {{6485, 6125, 5513}, {0} },
	     {{12971, 12250, 11025}, {0} } } ,
	{7,  {{860,  813,  731 }, {0} },
	     {{1721, 1625, 1463}, {0} },
	     {{3603, 3403, 3063}, {0} },
	     {{7206, 6806, 6125}, {0} },
	     {{14412, 13611, 12250}, {0} } } ,
	{8,  {{1032, 975,  878 }, {0} },
	     {{2065, 1950, 1755}, {0} },
	     {{4324, 4083, 3675}, {0} },
	     {{8647, 8167, 7350}, {0} },
	     {{17294, 16333, 14700}, {0} }} ,
	{9,  {{1147, 1083, 975 }, {0} },
	     {{2294, 2167, 1950}, {0} },
	     {{4804, 4537, 4083}, {0} },
	     {{9608, 9074, 8167}, {0} },
	     {{19216, 18148, 16333}, {0} } } ,
	{10, {{1290, 1219, 1097}, {0} },
	     {{2581, 2438, 2194}, {0} },
	     {{5404, 5104, 4594}, {0} },
	     {{10809, 10208, 9188}, {0} },
	     {{21618, 20417, 18375}, {0} } } ,
	{11, {{1434, 1354, 1219}, {0} },
	     {{2868, 2708, 2438}, {0} },
	     {{6005, 5671, 5104}, {0} },
	     {{12010, 11342, 10208}, {0} },
	     {{24020, 22685, 20417}, {0} }} ,
	{12, {{1549, 1463, 1316}, {0} },
	     {{3097, 2925, 2633}, {0} },
	     {{6485, 6125, 5513}, {0} },
	     {{12971, 12250, 11025}, {0} },
	     {{25941, 24500, 22050}, {0} }} ,
	{13, {{1721, 1625, 1463}, {0} },
	     {{3441, 3250, 2925}, {0} },
	     {{7206, 6806, 6125}, {0} },
	     {{14412, 13611, 12250}, {0}},
	     {{28824, 27222, 24500}, {0}}} ,
};

/*MCS parameters with Nss = 2*/
static const struct index_eht_data_rate_type eht_mcs_nss2[] = {
/* MCS,  {dcm0:0.8/1.6/3.2}, {dcm1:0.8/1.6/3.2} */
	{0,  {{172,   162,   146 }, {0} }, /* EHT20 */
	     {{344,   326,   292 }, {0} }, /* EHT40 */
	     {{720,   680,   612 }, {0} }, /* EHT80 */
	     {{1442, 1362, 1226},   {0} }, /* EHT160 */
	     {{2882, 2722, 2450},   {0} } } , /* EHT320 */
	{1,  {{344,   326,   292 }, {0} },
	     {{688,   650,   586 }, {0} },
	     {{1442,  1362,  1226}, {0} },
	     {{2882, 2722, 2450},   {0}},
	     {{5764, 5444, 4900},   {0} }} ,
	{2,  {{516,   488,   438 }, {0} },
	     {{1032,  976,   878 }, {0} },
	     {{2162,  2042,  1838}, {0} },
	     {{4324, 4084, 3676}, {0} },
	     {{8648, 8166, 7350}, {0} } } ,
	{3,  {{688,   650,   586 }, {0} },
	     {{1376,  1300,  1170}, {0} },
	     {{2882,  2722,  2450}, {0} },
	     {{5764, 5444, 4900}, {0} },
	     {{11530, 10888, 9800}, {0}} } ,
	{4,  {{1032,  976,   878 }, {0} },
	     {{2064,  1950,  1756}, {0} },
	     {{4324,  4083,  36756}, {0} },
	     {{8648, 8166, 7350}, {0} },
	     {{17294, 16334, 14700}, {0}}},
	{5,  {{1376,  1300,  1170}, {0} },
	     {{2752,  2600,  2340}, {0} },
	     {{5764,  5444,  4900}, {0} },
	     {{11530, 10888, 9800}, {0} },
	     {{23058, 21778, 19600}, {0} }} ,
	{6,  {{1548,  1462,  1316}, {0} },
	     {{3098,  2926,  2632}, {0} },
	     {{6486,  6126,  5512}, {0} },
	     {{12970, 12250, 11026}, {0} },
	     {{25942, 24500, 22050}, {0} }} ,
	{7,  {{1720,  1626,  1462}, {0} },
	     {{3442,  3250,  2926}, {0} },
	     {{7206,  6806,  61256}, {0} },
	     {{14412, 13612, 12250}, {0} },
	     {{28824, 27222, 24500}, {0} }} ,
	{8,  {{2064,  1950,  1756}, {0} },
	     {{4130,  3900,  3510}, {0} },
	     {{8648,  8166,  7350}, {0} },
	     {{17294, 16334, 14700}, {0} },
	     {{34588, 32666, 29400}, {0} }} ,
	{9,  {{2294,  2166,  1950}, {0} },
	     {{4588,  4334,  3900}, {0} },
	     {{9608,  9074,  8166}, {0} },
	     {{19216, 18148, 16334}, {0} },
	     {{38432, 36296, 32666}, {0} }} ,
	{10, {{2580,  2438,  2194}, {0} },
	     {{5162,  4876,  4388}, {0} },
	     {{10808, 10208, 9188}, {0} },
	     {{21618, 20416, 18376}, {0} },
	     {{43236, 40834, 36750}, {0} }} ,
	{11, {{2868,  2708,  2438}, {0} },
	     {{5736,  5416,  4876}, {0} },
	     {{12010, 11342, 10208}, {0} },
	     {{24020, 22686, 20416}, {0} },
	     {{48040, 45370, 40834}, {0} }} ,
	{12, {{3098,  2926,  2632}, {0} },
	     {{6194,  5850,  5266}, {0} },
	     {{12970, 12250, 11026}, {0} },
	     {{25942, 24500, 22050}, {0} },
	     {{51882, 49000, 44100}, {0} }} ,
	{13, {{3442,  3250,  2926}, {0} },
	     {{6882,  6500,  5850}, {0} },
	     {{14412, 13611, 12250}, {0} },
	     {{28824, 27222, 24500}, {0} },
	     {{57648, 54444, 49000}, {0} }}
};

/**
 * wma_convert_eht_cap() - convert EHT capabilities into dot11f structure
 * @eht_cap: pointer to dot11f structure
 * @mac_cap: Received EHT MAC capability
 * @phy_cap: Received EHT PHY capability
 *
 * This function converts various EHT capability received as part of extended
 * service ready event into dot11f structure.
 *
 * Return: None
 */
static void wma_convert_eht_cap(tDot11fIEeht_cap *eht_cap, uint32_t *mac_cap,
				uint32_t *phy_cap)
{
	eht_cap->present = true;

	/* EHT MAC capabilities */
	eht_cap->epcs_pri_access = WMI_EHTCAP_MAC_NSEPPRIACCESS_GET(mac_cap);
	eht_cap->eht_om_ctl = WMI_EHTCAP_MAC_EHTOMCTRL_GET(mac_cap);
	eht_cap->triggered_txop_sharing_mode1 =
				WMI_EHTCAP_MAC_TRIGTXOPMODE1_GET(mac_cap);
	eht_cap->triggered_txop_sharing_mode2 =
				WMI_EHTCAP_MAC_TRIGTXOPMODE2_GET(mac_cap);
	eht_cap->restricted_twt = WMI_EHTCAP_MAC_RESTRICTTWT_GET(mac_cap);
	eht_cap->scs_traffic_desc = WMI_EHTCAP_MAC_SCSTRAFFICDESC_GET(mac_cap);
	eht_cap->max_mpdu_len = WMI_EHTCAP_MAC_MAXMPDULEN_GET(mac_cap);
	eht_cap->max_a_mpdu_len_exponent_ext =
			WMI_EHTCAP_MAC_MAXAMPDULEN_EXP_GET(mac_cap);
	eht_cap->eht_trs_support =
			WMI_EHTCAP_MAC_TRS_SUPPORT_GET(mac_cap);
	eht_cap->txop_return_support_txop_share_m2 =
			WMI_EHTCAP_MAC_TXOP_RETURN_SUPP_IN_SHARINGMODE2_GET(mac_cap);

	/* EHT PHY capabilities */
	eht_cap->support_320mhz_6ghz = WMI_EHTCAP_PHY_320MHZIN6GHZ_GET(phy_cap);
	eht_cap->ru_242tone_wt_20mhz = WMI_EHTCAP_PHY_242TONERUBWLT20MHZ_GET(
			phy_cap);
	eht_cap->ndp_4x_eht_ltf_3dot2_us_gi =
		WMI_EHTCAP_PHY_NDP4XEHTLTFAND320NSGI_GET(phy_cap);
	eht_cap->partial_bw_mu_mimo = WMI_EHTCAP_PHY_PARTIALBWULMU_GET(phy_cap);
	eht_cap->su_beamformer = WMI_EHTCAP_PHY_SUBFMR_GET(phy_cap);
	eht_cap->su_beamformee = WMI_EHTCAP_PHY_SUBFME_GET(phy_cap);
	eht_cap->bfee_ss_le_80mhz = WMI_EHTCAP_PHY_BFMESSLT80MHZ_GET(phy_cap);
	eht_cap->bfee_ss_160mhz = WMI_EHTCAP_PHY_BFMESS160MHZ_GET(phy_cap);
	eht_cap->bfee_ss_320mhz = WMI_EHTCAP_PHY_BFMESS320MHZ_GET(phy_cap);
	eht_cap->num_sounding_dim_le_80mhz = WMI_EHTCAP_PHY_NUMSOUNDLT80MHZ_GET(
			phy_cap);
	eht_cap->num_sounding_dim_160mhz = WMI_EHTCAP_PHY_NUMSOUND160MHZ_GET(
			phy_cap);
	eht_cap->num_sounding_dim_320mhz = WMI_EHTCAP_PHY_NUMSOUND320MHZ_GET(
			phy_cap);
	eht_cap->ng_16_su_feedback = WMI_EHTCAP_PHY_NG16SUFB_GET(phy_cap);
	eht_cap->ng_16_mu_feedback = WMI_EHTCAP_PHY_NG16MUFB_GET(phy_cap);
	eht_cap->cb_sz_4_2_su_feedback = WMI_EHTCAP_PHY_CODBK42SUFB_GET(
			phy_cap);
	eht_cap->cb_sz_7_5_su_feedback = WMI_EHTCAP_PHY_CODBK75MUFB_GET(
			phy_cap);
	eht_cap->trig_su_bforming_feedback = WMI_EHTCAP_PHY_TRIGSUBFFB_GET(
			phy_cap);
	eht_cap->trig_mu_bforming_partial_bw_feedback =
		WMI_EHTCAP_PHY_TRIGMUBFPARTBWFB_GET(phy_cap);
	eht_cap->triggered_cqi_feedback = WMI_EHTCAP_PHY_TRIGCQIFB_GET(phy_cap);
	eht_cap->partial_bw_dl_mu_mimo = WMI_EHTCAP_PHY_PARTBWDLMUMIMO_GET(
			phy_cap);
	eht_cap->psr_based_sr = WMI_EHTCAP_PHY_PSRSR_GET(phy_cap);
	eht_cap->power_boost_factor = WMI_EHTCAP_PHY_PWRBSTFACTOR_GET(phy_cap);
	eht_cap->eht_mu_ppdu_4x_ltf_0_8_us_gi =
		WMI_EHTCAP_PHY_4XEHTLTFAND800NSGI_GET(phy_cap);
	eht_cap->max_nc = WMI_EHTCAP_PHY_MAXNC_GET(phy_cap);
	eht_cap->non_trig_cqi_feedback = WMI_EHTCAP_PHY_NONTRIGCQIFB_GET(
			phy_cap);
	eht_cap->tx_1024_4096_qam_lt_242_tone_ru =
		WMI_EHTCAP_PHY_TX1024AND4096QAMLS242TONERU_GET(phy_cap);
	eht_cap->rx_1024_4096_qam_lt_242_tone_ru =
		WMI_EHTCAP_PHY_RX1024AND4096QAMLS242TONERU_GET(phy_cap);
	eht_cap->ppet_present = WMI_EHTCAP_PHY_PPETHRESPRESENT_GET(phy_cap);
	eht_cap->common_nominal_pkt_padding = WMI_EHTCAP_PHY_CMNNOMPKTPAD_GET(
			phy_cap);
	eht_cap->max_num_eht_ltf = WMI_EHTCAP_PHY_MAXNUMEHTLTF_GET(phy_cap);
	eht_cap->mcs_15 = WMI_EHTCAP_PHY_SUPMCS15_GET(phy_cap);
	eht_cap->eht_dup_6ghz = WMI_EHTCAP_PHY_EHTDUPIN6GHZ_GET(phy_cap);
	eht_cap->op_sta_rx_ndp_wider_bw_20mhz =
		WMI_EHTCAP_PHY_20MHZOPSTARXNDPWIDERBW_GET(phy_cap);
	eht_cap->non_ofdma_ul_mu_mimo_le_80mhz =
		WMI_EHTCAP_PHY_NONOFDMAULMUMIMOLT80MHZ_GET(phy_cap);
	eht_cap->non_ofdma_ul_mu_mimo_160mhz =
		WMI_EHTCAP_PHY_NONOFDMAULMUMIMO160MHZ_GET(phy_cap);
	eht_cap->non_ofdma_ul_mu_mimo_320mhz =
		WMI_EHTCAP_PHY_NONOFDMAULMUMIMO320MHZ_GET(phy_cap);
	eht_cap->mu_bformer_le_80mhz = WMI_EHTCAP_PHY_MUBFMRLT80MHZ_GET(
			phy_cap);
	eht_cap->mu_bformer_160mhz = WMI_EHTCAP_PHY_MUBFMR160MHZ_GET(phy_cap);
	eht_cap->mu_bformer_320mhz = WMI_EHTCAP_PHY_MUBFMR320MHZ_GET(phy_cap);
	eht_cap->tb_sounding_feedback_rl =
			WMI_EHTCAP_PHY_TBSUNDFBRATELIMIT_GET(phy_cap);
	eht_cap->rx_1k_qam_in_wider_bw_dl_ofdma =
			WMI_EHTCAP_PHY_RX1024QAMWIDERBWDLOFDMA_GET(phy_cap);
	eht_cap->rx_4k_qam_in_wider_bw_dl_ofdma =
			WMI_EHTCAP_PHY_RX4096QAMWIDERBWDLOFDMA_GET(phy_cap);

	/* TODO: MCS map and PPET */
}

void wma_eht_update_tgt_services(struct wmi_unified *wmi_handle,
				 struct wma_tgt_services *cfg)
{
	if (wmi_service_enabled(wmi_handle, wmi_service_11be)) {
		cfg->en_11be = true;
		wma_set_fw_wlan_feat_caps(DOT11BE);
		wma_debug("11be is enabled");
	} else {
		cfg->en_11be = false;
		wma_debug("11be is not enabled");
	}
}

static void
wma_update_eht_cap_support_for_320mhz(struct target_psoc_info *tgt_hdl,
				      tDot11fIEeht_cap *eht_cap)
{
	struct wlan_psoc_host_mac_phy_caps_ext2 *cap;

	cap = target_psoc_get_mac_phy_cap_ext2_for_mode(
			tgt_hdl, WMI_HOST_HW_MODE_SINGLE);
	if (!cap) {
		wma_debug("HW_MODE_SINGLE does not exist");
		return;
	}

	eht_cap->support_320mhz_6ghz = WMI_EHTCAP_PHY_320MHZIN6GHZ_GET(
			cap->eht_cap_phy_info_5G);
	wma_debug("Support for 320MHz 0x%01x", eht_cap->support_320mhz_6ghz);
}

static void
wma_update_eht_20mhz_only_mcs(uint32_t *mcs_2g_20, tDot11fIEeht_cap *eht_cap)
{
	eht_cap->bw_20_rx_max_nss_for_mcs_0_to_7 |= QDF_GET_BITS(*mcs_2g_20, 0, 4);
	eht_cap->bw_20_tx_max_nss_for_mcs_0_to_7 |= QDF_GET_BITS(*mcs_2g_20, 4, 4);
	eht_cap->bw_20_rx_max_nss_for_mcs_8_and_9 |= QDF_GET_BITS(*mcs_2g_20, 8, 4);
	eht_cap->bw_20_tx_max_nss_for_mcs_8_and_9 |=
						QDF_GET_BITS(*mcs_2g_20, 12, 4);
	eht_cap->bw_20_rx_max_nss_for_mcs_10_and_11 |=
						QDF_GET_BITS(*mcs_2g_20, 16, 4);
	eht_cap->bw_20_tx_max_nss_for_mcs_10_and_11 |=
						QDF_GET_BITS(*mcs_2g_20, 20, 4);
	eht_cap->bw_20_rx_max_nss_for_mcs_12_and_13 |=
						QDF_GET_BITS(*mcs_2g_20, 24, 4);
	eht_cap->bw_20_tx_max_nss_for_mcs_12_and_13 |=
						QDF_GET_BITS(*mcs_2g_20, 28, 4);
}

static void
wma_update_eht_le_80mhz_mcs(uint32_t *mcs_le_80, tDot11fIEeht_cap *eht_cap)
{
	eht_cap->bw_le_80_rx_max_nss_for_mcs_0_to_9 |=
						QDF_GET_BITS(*mcs_le_80, 0, 4);
	eht_cap->bw_le_80_tx_max_nss_for_mcs_0_to_9 |=
						QDF_GET_BITS(*mcs_le_80, 4, 4);
	eht_cap->bw_le_80_rx_max_nss_for_mcs_10_and_11 |=
						QDF_GET_BITS(*mcs_le_80, 8, 4);
	eht_cap->bw_le_80_tx_max_nss_for_mcs_10_and_11 |=
						QDF_GET_BITS(*mcs_le_80, 12, 4);
	eht_cap->bw_le_80_rx_max_nss_for_mcs_12_and_13 |=
						QDF_GET_BITS(*mcs_le_80, 16, 4);
	eht_cap->bw_le_80_tx_max_nss_for_mcs_12_and_13 |=
						QDF_GET_BITS(*mcs_le_80, 20, 4);
}

static void
wma_update_eht_160mhz_mcs(uint32_t *mcs_160mhz, tDot11fIEeht_cap *eht_cap)
{
	eht_cap->bw_160_rx_max_nss_for_mcs_0_to_9 |=
						QDF_GET_BITS(*mcs_160mhz, 0, 4);
	eht_cap->bw_160_tx_max_nss_for_mcs_0_to_9 |=
						QDF_GET_BITS(*mcs_160mhz, 4, 4);
	eht_cap->bw_160_rx_max_nss_for_mcs_10_and_11 |=
						QDF_GET_BITS(*mcs_160mhz, 8, 4);
	eht_cap->bw_160_tx_max_nss_for_mcs_10_and_11 |=
					       QDF_GET_BITS(*mcs_160mhz, 12, 4);
	eht_cap->bw_160_rx_max_nss_for_mcs_12_and_13 |=
					       QDF_GET_BITS(*mcs_160mhz, 16, 4);
	eht_cap->bw_160_tx_max_nss_for_mcs_12_and_13 |=
					       QDF_GET_BITS(*mcs_160mhz, 20, 4);
}

static void
wma_update_eht_320mhz_mcs(uint32_t *mcs_320mhz, tDot11fIEeht_cap *eht_cap)
{
	eht_cap->bw_320_rx_max_nss_for_mcs_0_to_9 |=
						QDF_GET_BITS(*mcs_320mhz, 0, 4);
	eht_cap->bw_320_tx_max_nss_for_mcs_0_to_9 |=
						QDF_GET_BITS(*mcs_320mhz, 4, 4);
	eht_cap->bw_320_rx_max_nss_for_mcs_10_and_11 |=
						QDF_GET_BITS(*mcs_320mhz, 8, 4);
	eht_cap->bw_320_tx_max_nss_for_mcs_10_and_11 |=
					       QDF_GET_BITS(*mcs_320mhz, 12, 4);
	eht_cap->bw_320_rx_max_nss_for_mcs_12_and_13 |=
					       QDF_GET_BITS(*mcs_320mhz, 16, 4);
	eht_cap->bw_320_tx_max_nss_for_mcs_12_and_13 |=
					       QDF_GET_BITS(*mcs_320mhz, 20, 4);
}

void wma_update_target_ext_eht_cap(struct target_psoc_info *tgt_hdl,
				   struct wma_tgt_cfg *tgt_cfg)
{
	tDot11fIEeht_cap *eht_cap = &tgt_cfg->eht_cap;
	tDot11fIEeht_cap *eht_cap_2g = &tgt_cfg->eht_cap_2g;
	tDot11fIEeht_cap *eht_cap_5g = &tgt_cfg->eht_cap_5g;
	int i, num_hw_modes, total_mac_phy_cnt;
	tDot11fIEeht_cap eht_cap_mac;
	struct wlan_psoc_host_mac_phy_caps_ext2 *mac_phy_cap, *mac_phy_caps2;
	struct wlan_psoc_host_mac_phy_caps *host_cap;
	uint32_t supported_bands;
	uint32_t *mcs_supp;

	qdf_mem_zero(eht_cap_2g, sizeof(tDot11fIEeht_cap));
	qdf_mem_zero(eht_cap_5g, sizeof(tDot11fIEeht_cap));
	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);
	mac_phy_cap = target_psoc_get_mac_phy_cap_ext2(tgt_hdl);
	host_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	if (!mac_phy_cap || !host_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		eht_cap->present = false;
		return;
	}

	if (!num_hw_modes) {
		wma_err("No extended EHT cap for current SOC");
		eht_cap->present = false;
		return;
	}

	if (!tgt_cfg->services.en_11be) {
		wma_info("Target does not support 11BE");
		eht_cap->present = false;
		return;
	}

	for (i = 0; i < total_mac_phy_cnt; i++) {
		supported_bands = host_cap[i].supported_bands;
		qdf_mem_zero(&eht_cap_mac, sizeof(tDot11fIEeht_cap));
		mac_phy_caps2 = &mac_phy_cap[i];
		if (supported_bands & WLAN_2G_CAPABILITY) {
			wma_convert_eht_cap(&eht_cap_mac,
					    mac_phy_caps2->eht_cap_info_2G,
					    mac_phy_caps2->eht_cap_phy_info_2G);
				/* TODO: PPET */
			/* WMI_EHT_SUPP_MCS_20MHZ_ONLY */
			mcs_supp = &mac_phy_caps2->eht_supp_mcs_ext_2G[0];
			wma_update_eht_20mhz_only_mcs(mcs_supp, &eht_cap_mac);
			/* WMI_EHT_SUPP_MCS_LE_80MHZ */
			mcs_supp = &mac_phy_caps2->eht_supp_mcs_ext_2G[1];
			wma_update_eht_le_80mhz_mcs(mcs_supp, &eht_cap_mac);

			qdf_mem_copy(eht_cap_2g, &eht_cap_mac,
				     sizeof(tDot11fIEeht_cap));
		}

		if (supported_bands & WLAN_5G_CAPABILITY) {
			qdf_mem_zero(&eht_cap_mac, sizeof(tDot11fIEeht_cap));
			wma_convert_eht_cap(&eht_cap_mac,
					    mac_phy_caps2->eht_cap_info_5G,
					    mac_phy_caps2->eht_cap_phy_info_5G);

			/* WMI_EHT_SUPP_MCS_20MHZ_ONLY */
			mcs_supp = &mac_phy_caps2->eht_supp_mcs_ext_5G[0];
			wma_update_eht_20mhz_only_mcs(mcs_supp, &eht_cap_mac);
			/* WMI_EHT_SUPP_MCS_LE_80MHZ */
			mcs_supp = &mac_phy_caps2->eht_supp_mcs_ext_5G[1];
			wma_update_eht_le_80mhz_mcs(mcs_supp, &eht_cap_mac);

			/* WMI_EHT_SUPP_MCS_160MHZ */
			mcs_supp = &mac_phy_caps2->eht_supp_mcs_ext_5G[2];
			wma_update_eht_160mhz_mcs(mcs_supp, &eht_cap_mac);
			/* WMI_EHT_SUPP_MCS_320MHZ */
			mcs_supp = &mac_phy_caps2->eht_supp_mcs_ext_5G[3];
			wma_update_eht_320mhz_mcs(mcs_supp, &eht_cap_mac);

			qdf_mem_copy(eht_cap_5g, &eht_cap_mac,
				     sizeof(tDot11fIEeht_cap));
		}
	}
	qdf_mem_copy(eht_cap, &eht_cap_mac, sizeof(tDot11fIEeht_cap));

	wma_update_eht_cap_support_for_320mhz(tgt_hdl, eht_cap);
	wma_update_eht_cap_support_for_320mhz(tgt_hdl, eht_cap_5g);

	wma_print_eht_cap(eht_cap);
}

void wma_update_vdev_eht_ops(uint32_t *eht_ops, tDot11fIEeht_op *eht_op)
{
}

void wma_print_eht_cap(tDot11fIEeht_cap *eht_cap)
{
	if (!eht_cap->present)
		return;

	wma_debug("EHT Capabilities:");

	/* EHT MAC Capabilities */
	wma_nofl_debug("\tEPCS Priority Access: 0x%01x",
		       eht_cap->epcs_pri_access);
	wma_nofl_debug("\tOM Control: 0x%01x", eht_cap->eht_om_ctl);
	wma_nofl_debug("\tTriggered TXOP Sharing mode-1: 0x%01x",
		       eht_cap->triggered_txop_sharing_mode1);
	wma_nofl_debug("\tTriggered TXOP Sharing mode-2: 0x%01x",
		       eht_cap->triggered_txop_sharing_mode2);
	wma_nofl_debug("\tRestricted TWT support: 0x%01x",
		       eht_cap->restricted_twt);
	wma_nofl_debug("\tSCS Traffic Description Support: 0x%01x",
		       eht_cap->scs_traffic_desc);
	wma_nofl_debug("\tMaximum MPDU Length: 0x%01x",
		       eht_cap->max_mpdu_len);
	wma_nofl_debug("\tMaximum A-MPDU Length Exponent Extension: 0x%01x",
		       eht_cap->max_a_mpdu_len_exponent_ext);
	wma_nofl_debug("\tEHT TRS SUPPORT: 0x%01x",
		       eht_cap->eht_trs_support);
	wma_nofl_debug("\tTXOP Return Support in TXOP Sharing Mode 2: 0x%01x",
		       eht_cap->txop_return_support_txop_share_m2);

	/* EHT PHY Capabilities */
	wma_nofl_debug("\t320 MHz In 6 GHz: 0x%01x",
		       eht_cap->support_320mhz_6ghz);
	wma_nofl_debug("\t242-tone RU In BW Wider Than 20 MHz: 0x%01x",
		       eht_cap->ru_242tone_wt_20mhz);
	wma_nofl_debug("\tNDP With 4x EHT-LTF And 3.2 us GI: 0x%01x",
		       eht_cap->ndp_4x_eht_ltf_3dot2_us_gi);
	wma_nofl_debug("\tPartial Bandwidth UL MU-MIMO: 0x%01x",
		       eht_cap->partial_bw_mu_mimo);
	wma_nofl_debug("\tSU Beamformer: 0x%01x", eht_cap->su_beamformer);
	wma_nofl_debug("\tSU Beamformee: 0x%01x", eht_cap->su_beamformee);
	wma_nofl_debug("\tBeamformee SS <= 80 MHz: 0x%03x",
		       eht_cap->bfee_ss_le_80mhz);
	wma_nofl_debug("\tBeamformee SS = 160 MHz: 0x%03x",
		       eht_cap->bfee_ss_160mhz);
	wma_nofl_debug("\tBeamformee SS = 320 MHz: 0x%03x",
		       eht_cap->bfee_ss_320mhz);
	wma_nofl_debug("\tNumber Of Sounding Dimensions <= 80 MHz: 0x%03x",
		       eht_cap->num_sounding_dim_le_80mhz);
	wma_nofl_debug("\tNumber Of Sounding Dimensions = 160 MHz: 0x%03x",
		       eht_cap->num_sounding_dim_160mhz);
	wma_nofl_debug("\tNumber Of Sounding Dimensions = 320 MHz: 0x%03x",
		       eht_cap->num_sounding_dim_320mhz);
	wma_nofl_debug("\tNg = 16 SU Feedback: 0x%01x",
		       eht_cap->ng_16_su_feedback);
	wma_nofl_debug("\tNg = 16 MU Feedback: 0x%01x",
		       eht_cap->ng_16_mu_feedback);
	wma_nofl_debug("\tCodebook Size 4 2 SU Feedback: 0x%01x",
		       eht_cap->cb_sz_4_2_su_feedback);
	wma_nofl_debug("\tCodebook Size 7 5 MU Feedback: 0x%01x",
		       eht_cap->cb_sz_7_5_su_feedback);
	wma_nofl_debug("\tTriggered SU Beamforming Feedback: 0x%01x",
		       eht_cap->trig_su_bforming_feedback);
	wma_nofl_debug("\tTriggered MU Beamforming Partial BW Feedback: 0x%01x",
		       eht_cap->trig_mu_bforming_partial_bw_feedback);
	wma_nofl_debug("\tTriggered CQI Feedback: 0x%01x",
		       eht_cap->triggered_cqi_feedback);
	wma_nofl_debug("\tPartial Bandwidth DL MU-MIMO: 0x%01x",
		       eht_cap->partial_bw_dl_mu_mimo);
	wma_nofl_debug("\tPSR-Based SR: 0x%01x", eht_cap->psr_based_sr);
	wma_nofl_debug("\tPower Boost Factor: 0x%01x",
		       eht_cap->power_boost_factor);
	wma_nofl_debug("\tMU PPDU With 4x EHT-LTF 0.8 us GI: 0x%01x",
		       eht_cap->eht_mu_ppdu_4x_ltf_0_8_us_gi);
	wma_nofl_debug("\tMax Nc: 0x%04x", eht_cap->max_nc);
	wma_nofl_debug("\tNon-Triggered CQI Feedback: 0x%01x",
		       eht_cap->non_trig_cqi_feedback);
	wma_nofl_debug("\tTx 1024-QAM 4096-QAM < 242-tone RU: 0x%01x",
		       eht_cap->tx_1024_4096_qam_lt_242_tone_ru);
	wma_nofl_debug("\tRx 1024-QAM 4096-QAM < 242-tone RU: 0x%01x",
		       eht_cap->rx_1024_4096_qam_lt_242_tone_ru);
	wma_nofl_debug("\tPPE Thresholds Present: 0x%01x",
		       eht_cap->ppet_present);
	wma_nofl_debug("\tCommon Nominal Packet Padding: 0x%02x",
		       eht_cap->common_nominal_pkt_padding);
	wma_nofl_debug("\tMaximum Number Of Supported EHT-LTFs: 0x%05x",
		       eht_cap->max_num_eht_ltf);
	wma_nofl_debug("\tSupport of MCS 15: 0x%04x", eht_cap->mcs_15);
	wma_nofl_debug("\tSupport Of EHT DUP In 6 GHz: 0x%01x",
		       eht_cap->eht_dup_6ghz);
	wma_nofl_debug("\t20 MHz STA RX NDP With Wider BW: 0x%01x",
		       eht_cap->op_sta_rx_ndp_wider_bw_20mhz);
	wma_nofl_debug("\tNon-OFDMA UL MU-MIMO BW <= 80 MHz: 0x%01x",
		       eht_cap->non_ofdma_ul_mu_mimo_le_80mhz);
	wma_nofl_debug("\tNon-OFDMA UL MU-MIMO BW = 160 MHz: 0x%01x",
		       eht_cap->non_ofdma_ul_mu_mimo_160mhz);
	wma_nofl_debug("\tNon-OFDMA UL MU-MIMO BW = 320 MHz: 0x%01x",
		       eht_cap->non_ofdma_ul_mu_mimo_320mhz);
	wma_nofl_debug("\tMU Beamformer BW <= 80 MHz: 0x%01x",
		       eht_cap->mu_bformer_le_80mhz);
	wma_nofl_debug("\tMU Beamformer BW = 160 MHz: 0x%01x",
		       eht_cap->mu_bformer_160mhz);
	wma_nofl_debug("\tMU Beamformer BW = 320 MHz: 0x%01x",
		       eht_cap->mu_bformer_320mhz);
	wma_nofl_debug("\tTB sounding feedback rate limit: 0x%01x",
		       eht_cap->tb_sounding_feedback_rl);
	wma_nofl_debug("\tRx 1024-QAM in wider bandwidth DL OFDMA support: 0x%01x",
		       eht_cap->rx_1k_qam_in_wider_bw_dl_ofdma);
	wma_nofl_debug("\tRx 4096-QAM in wider bandwidth DL OFDMA support: 0x%01x",
		       eht_cap->rx_4k_qam_in_wider_bw_dl_ofdma);
	wma_nofl_debug("\t EHT MCS 20 rx 0-7 0x%x",
		       eht_cap->bw_20_rx_max_nss_for_mcs_0_to_7);
	wma_nofl_debug("\t EHT MCS 20 tx 0-7 0x%x",
		       eht_cap->bw_20_tx_max_nss_for_mcs_0_to_7);
	wma_nofl_debug("\t EHT MCS 20 rx 8-9 0x%x",
		       eht_cap->bw_20_rx_max_nss_for_mcs_8_and_9);
	wma_nofl_debug("\t EHT MCS 20 tx 8-9 0x%x",
		       eht_cap->bw_20_tx_max_nss_for_mcs_8_and_9);
	wma_nofl_debug("\t EHT MCS 20 rx 10-11 0x%x",
		       eht_cap->bw_20_rx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 20 tx 10-11 0x%x",
		       eht_cap->bw_20_tx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 20 rx 12-13 0x%x",
		       eht_cap->bw_20_rx_max_nss_for_mcs_12_and_13);
	wma_nofl_debug("\t EHT MCS 20 tx 12-13 0x%x",
		       eht_cap->bw_20_tx_max_nss_for_mcs_12_and_13);
	wma_nofl_debug("\t EHT MCS 80 rx 0-9 0x%x",
		       eht_cap->bw_le_80_rx_max_nss_for_mcs_0_to_9);
	wma_nofl_debug("\t EHT MCS 80 tx 0-9 0x%x",
		       eht_cap->bw_le_80_tx_max_nss_for_mcs_0_to_9);
	wma_nofl_debug("\t EHT MCS 80 rx 10-11 0x%x",
		       eht_cap->bw_le_80_rx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 80 tx 10-11 0x%x",
		       eht_cap->bw_le_80_tx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 80 rx 12-13 0x%x",
		       eht_cap->bw_le_80_rx_max_nss_for_mcs_12_and_13);
	wma_nofl_debug("\t EHT MCS 80 tx 12-13 0x%x",
		       eht_cap->bw_le_80_tx_max_nss_for_mcs_12_and_13);
	wma_nofl_debug("\t EHT MCS 160 rx 0-9 0x%x",
		       eht_cap->bw_160_rx_max_nss_for_mcs_0_to_9);
	wma_nofl_debug("\t EHT MCS 160 tx 0-9 0x%x",
		       eht_cap->bw_160_tx_max_nss_for_mcs_0_to_9);
	wma_nofl_debug("\t EHT MCS 160 rx 10-11 0x%x",
		       eht_cap->bw_160_rx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 160 tx 10-11 0x%x",
		       eht_cap->bw_160_tx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 160 rx 12-13 0x%x",
		       eht_cap->bw_160_rx_max_nss_for_mcs_12_and_13);
	wma_nofl_debug("\t EHT MCS 160 rx 12-13 0x%x",
		       eht_cap->bw_160_tx_max_nss_for_mcs_12_and_13);
	wma_nofl_debug("\t EHT MCS 320 rx 0-9 0x%x",
		       eht_cap->bw_320_rx_max_nss_for_mcs_0_to_9);
	wma_nofl_debug("\t EHT MCS 320 tx 0-9 0x%x",
		       eht_cap->bw_320_tx_max_nss_for_mcs_0_to_9);
	wma_nofl_debug("\t EHT MCS 320 rx 10-11 0x%x",
		       eht_cap->bw_320_rx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 320 tx 10-11 0x%x",
		       eht_cap->bw_320_tx_max_nss_for_mcs_10_and_11);
	wma_nofl_debug("\t EHT MCS 320 rx 12-13 0x%x",
		       eht_cap->bw_320_rx_max_nss_for_mcs_12_and_13);
	wma_nofl_debug("\t EHT MCS 320 tx 12-13 0x%x",
		       eht_cap->bw_320_tx_max_nss_for_mcs_12_and_13);
}

void wma_print_eht_phy_cap(uint32_t *phy_cap)
{
	wma_debug("EHT PHY Capabilities:");

	wma_nofl_debug("\t320 MHz In 6 GHz: 0x%01x",
		       WMI_EHTCAP_PHY_320MHZIN6GHZ_GET(phy_cap));
	wma_nofl_debug("\t242-tone RU In BW Wider Than 20 MHz: 0x%01x",
		       WMI_EHTCAP_PHY_242TONERUBWLT20MHZ_GET(phy_cap));
	wma_nofl_debug("\tNDP With 4x EHT-LTF And 3.2 us GI: 0x%01x",
		       WMI_EHTCAP_PHY_NDP4XEHTLTFAND320NSGI_GET(phy_cap));
	wma_nofl_debug("\tPartial Bandwidth UL MU-MIMO: 0x%01x",
		       WMI_EHTCAP_PHY_PARTIALBWULMU_GET(phy_cap));
	wma_nofl_debug("\tSU Beamformer: 0x%01x",
		       WMI_EHTCAP_PHY_SUBFMR_GET(phy_cap));
	wma_nofl_debug("\tSU Beamformee: 0x%01x",
		       WMI_EHTCAP_PHY_SUBFME_GET(phy_cap));
	wma_nofl_debug("\tBeamformee SS <= 80 MHz: 0x%03x",
		       WMI_EHTCAP_PHY_BFMESSLT80MHZ_GET(phy_cap));
	wma_nofl_debug("\tBeamformee SS = 160 MHz: 0x%03x",
		       WMI_EHTCAP_PHY_BFMESS160MHZ_GET(phy_cap));
	wma_nofl_debug("\tBeamformee SS = 320 MHz: 0x%03x",
		       WMI_EHTCAP_PHY_BFMESS320MHZ_GET(phy_cap));
	wma_nofl_debug("\tNumber Of Sounding Dimensions <= 80 MHz: 0x%03x",
		       WMI_EHTCAP_PHY_NUMSOUNDLT80MHZ_GET(phy_cap));
	wma_nofl_debug("\tNumber Of Sounding Dimensions = 160 MHz: 0x%03x",
		       WMI_EHTCAP_PHY_NUMSOUND160MHZ_GET(phy_cap));
	wma_nofl_debug("\tNumber Of Sounding Dimensions = 320 MHz: 0x%03x",
		       WMI_EHTCAP_PHY_NUMSOUND320MHZ_GET(phy_cap));
	wma_nofl_debug("\tNg = 16 SU Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_NG16SUFB_GET(phy_cap));
	wma_nofl_debug("\tNg = 16 MU Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_NG16MUFB_GET(phy_cap));
	wma_nofl_debug("\tCodebook Size 4 2 SU Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_CODBK42SUFB_GET(phy_cap));
	wma_nofl_debug("\tCodebook Size 7 5 MU Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_CODBK75MUFB_GET(phy_cap));
	wma_nofl_debug("\tTriggered SU Beamforming Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_TRIGSUBFFB_GET(phy_cap));
	wma_nofl_debug("\tTriggered MU Beamforming Partial BW Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_TRIGMUBFPARTBWFB_GET(phy_cap));
	wma_nofl_debug("\tTriggered CQI Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_TRIGCQIFB_GET(phy_cap));
	wma_nofl_debug("\tPartial Bandwidth DL MU-MIMO: 0x%01x",
		       WMI_EHTCAP_PHY_TRIGMUBFPARTBWFB_GET(phy_cap));
	wma_nofl_debug("\tPSR-Based SR: 0x%01x",
		       WMI_EHTCAP_PHY_PSRSR_GET(phy_cap));
	wma_nofl_debug("\tPower Boost Factor: 0x%01x",
		       WMI_EHTCAP_PHY_PWRBSTFACTOR_GET(phy_cap));
	wma_nofl_debug("\tMU PPDU With 4x EHT-LTF 0.8 us GI: 0x%01x",
		       WMI_EHTCAP_PHY_4XEHTLTFAND800NSGI_GET(phy_cap));
	wma_nofl_debug("\tMax Nc: 0x%04x", WMI_EHTCAP_PHY_MAXNC_GET(phy_cap));
	wma_nofl_debug("\tNon-Triggered CQI Feedback: 0x%01x",
		       WMI_EHTCAP_PHY_NONTRIGCQIFB_GET(phy_cap));
	wma_nofl_debug("\tTx 1024-QAM 4096-QAM < 242-tone RU: 0x%01x",
		       WMI_EHTCAP_PHY_TX1024AND4096QAMLS242TONERU_GET(phy_cap));
	wma_nofl_debug("\tRx 1024-QAM 4096-QAM < 242-tone RU: 0x%01x",
		       WMI_EHTCAP_PHY_RX1024AND4096QAMLS242TONERU_GET(phy_cap));
	wma_nofl_debug("\tPPE Thresholds Present: 0x%01x",
		       WMI_EHTCAP_PHY_PPETHRESPRESENT_GET(phy_cap));
	wma_nofl_debug("\tCommon Nominal Packet Padding: 0x%02x",
		       WMI_EHTCAP_PHY_CMNNOMPKTPAD_GET(phy_cap));
	wma_nofl_debug("\tMaximum Number Of Supported EHT-LTFs: 0x%05x",
		       WMI_EHTCAP_PHY_MAXNUMEHTLTF_GET(phy_cap));
	wma_nofl_debug("\tSupport of MCS 15: 0x%04x",
		       WMI_EHTCAP_PHY_SUPMCS15_GET(phy_cap));
	wma_nofl_debug("\tSupport Of EHT DUP In 6 GHz: 0x%01x",
		       WMI_EHTCAP_PHY_EHTDUPIN6GHZ_GET(phy_cap));
	wma_nofl_debug("\t20 MHz STA RX NDP With Wider BW: 0x%01x",
		       WMI_EHTCAP_PHY_20MHZOPSTARXNDPWIDERBW_GET(phy_cap));
	wma_nofl_debug("\tNon-OFDMA UL MU-MIMO BW <= 80 MHz: 0x%01x",
		       WMI_EHTCAP_PHY_NONOFDMAULMUMIMOLT80MHZ_GET(phy_cap));
	wma_nofl_debug("\tNon-OFDMA UL MU-MIMO BW = 160 MHz: 0x%01x",
		       WMI_EHTCAP_PHY_NONOFDMAULMUMIMO160MHZ_GET(phy_cap));
	wma_nofl_debug("\tNon-OFDMA UL MU-MIMO BW = 320 MHz: 0x%01x",
		       WMI_EHTCAP_PHY_NONOFDMAULMUMIMO320MHZ_GET(phy_cap));
	wma_nofl_debug("\tMU Beamformer BW <= 80 MHz: 0x%01x",
		       WMI_EHTCAP_PHY_MUBFMRLT80MHZ_GET(phy_cap));
	wma_nofl_debug("\tMU Beamformer BW = 160 MHz: 0x%01x",
		       WMI_EHTCAP_PHY_MUBFMR160MHZ_GET(phy_cap));
	wma_nofl_debug("\tMU Beamformer BW = 320 MHz: 0x%01x",
		       WMI_EHTCAP_PHY_MUBFMR320MHZ_GET(phy_cap));
	wma_nofl_debug("\tTB sounding feedback rate limit: 0x%01x",
		       WMI_EHTCAP_PHY_TBSUNDFBRATELIMIT_GET(phy_cap));
	wma_nofl_debug("\tRx 1024-QAM in wider bandwidth DL OFDMA support: 0x%01x",
		       WMI_EHTCAP_PHY_RX1024QAMWIDERBWDLOFDMA_GET(phy_cap));
	wma_nofl_debug("\tRx 4096-QAM in wider bandwidth DL OFDMA support: 0x%01x",
		       WMI_EHTCAP_PHY_RX4096QAMWIDERBWDLOFDMA_GET(phy_cap));
}

void wma_print_eht_mac_cap(uint32_t *mac_cap)
{
	wma_debug("EHT MAC Capabilities:");

	wma_nofl_debug("\tEPCS Priority Access: 0x%01x",
		       WMI_EHTCAP_MAC_EPCSPRIACCESS_GET(mac_cap));
	wma_nofl_debug("\tOM Control: 0x%01x",
		       WMI_EHTCAP_MAC_EHTOMCTRL_GET(mac_cap));
	wma_nofl_debug("\tTriggered TXOP Sharing mode 1 support: 0x%01x",
		       WMI_EHTCAP_MAC_TRIGTXOPMODE1_GET(mac_cap));
	wma_nofl_debug("\tTriggered TXOP Sharing mode 2 support: 0x%01x",
		       WMI_EHTCAP_MAC_TRIGTXOPMODE2_GET(mac_cap));
	wma_nofl_debug("\tRestricted TWT support: 0x%01x",
		       WMI_EHTCAP_MAC_RESTRICTTWT_GET(mac_cap));
	wma_nofl_debug("\tSCS Traffic Description Support: 0x%01x",
		       WMI_EHTCAP_MAC_SCSTRAFFICDESC_GET(mac_cap));
	wma_nofl_debug("\tMaximum MPDU Length: 0x%01x",
		       WMI_EHTCAP_MAC_MAXMPDULEN_GET(mac_cap));
}

void wma_print_eht_op(tDot11fIEeht_op *eht_ops)
{
}

void wma_populate_peer_eht_cap(struct peer_assoc_params *peer,
			       tpAddStaParams params)
{
	tDot11fIEeht_cap *eht_cap = &params->eht_config;
	uint32_t *phy_cap = peer->peer_eht_cap_phyinfo;
	uint32_t *mac_cap = peer->peer_eht_cap_macinfo;
	struct supported_rates *rates;

	if (!params->eht_capable)
		return;

	peer->eht_flag = 1;
	peer->qos_flag = 1;

	/* EHT MAC Capabilities */
	WMI_EHTCAP_MAC_EPCSPRIACCESS_SET(mac_cap, eht_cap->epcs_pri_access);
	WMI_EHTCAP_MAC_EHTOMCTRL_SET(mac_cap, eht_cap->eht_om_ctl);
	WMI_EHTCAP_MAC_TRIGTXOPMODE1_SET(mac_cap,
					 eht_cap->triggered_txop_sharing_mode1);
	WMI_EHTCAP_MAC_TRIGTXOPMODE2_SET(mac_cap,
					 eht_cap->triggered_txop_sharing_mode2);
	WMI_EHTCAP_MAC_RESTRICTTWT_SET(mac_cap,
				       eht_cap->restricted_twt);
	WMI_EHTCAP_MAC_SCSTRAFFICDESC_SET(mac_cap,
					  eht_cap->scs_traffic_desc);
	WMI_EHTCAP_MAC_MAXMPDULEN_SET(mac_cap,
				      eht_cap->max_mpdu_len);
	WMI_EHTCAP_MAC_MAXAMPDULEN_EXP_SET(mac_cap,
					   eht_cap->max_a_mpdu_len_exponent_ext);
	WMI_EHTCAP_MAC_TRS_SUPPORT_SET(mac_cap,
				       eht_cap->eht_trs_support);
	WMI_EHTCAP_MAC_TXOP_RETURN_SUPP_IN_SHARINGMODE2_SET(mac_cap,
				eht_cap->txop_return_support_txop_share_m2);

	/* EHT PHY Capabilities */
	WMI_EHTCAP_PHY_320MHZIN6GHZ_SET(phy_cap, eht_cap->support_320mhz_6ghz);
	WMI_EHTCAP_PHY_242TONERUBWLT20MHZ_SET(phy_cap,
					      eht_cap->ru_242tone_wt_20mhz);
	WMI_EHTCAP_PHY_NDP4XEHTLTFAND320NSGI_SET(
			phy_cap, eht_cap->ndp_4x_eht_ltf_3dot2_us_gi);
	WMI_EHTCAP_PHY_PARTIALBWULMU_SET(phy_cap, eht_cap->partial_bw_mu_mimo);
	WMI_EHTCAP_PHY_SUBFMR_SET(phy_cap, eht_cap->su_beamformer);
	WMI_EHTCAP_PHY_SUBFME_SET(phy_cap, eht_cap->su_beamformee);
	WMI_EHTCAP_PHY_BFMESSLT80MHZ_SET(phy_cap, eht_cap->bfee_ss_le_80mhz);
	WMI_EHTCAP_PHY_BFMESS160MHZ_SET(phy_cap, eht_cap->bfee_ss_160mhz);
	WMI_EHTCAP_PHY_BFMESS320MHZ_SET(phy_cap, eht_cap->bfee_ss_320mhz);
	WMI_EHTCAP_PHY_NUMSOUNDLT80MHZ_SET(
			phy_cap, eht_cap->num_sounding_dim_le_80mhz);
	WMI_EHTCAP_PHY_NUMSOUND160MHZ_SET(phy_cap,
					  eht_cap->num_sounding_dim_160mhz);
	WMI_EHTCAP_PHY_NUMSOUND320MHZ_SET(phy_cap,
					  eht_cap->num_sounding_dim_320mhz);
	WMI_EHTCAP_PHY_NG16SUFB_SET(phy_cap, eht_cap->ng_16_su_feedback);
	WMI_EHTCAP_PHY_NG16MUFB_SET(phy_cap, eht_cap->ng_16_mu_feedback);
	WMI_EHTCAP_PHY_CODBK42SUFB_SET(phy_cap, eht_cap->cb_sz_4_2_su_feedback);
	WMI_EHTCAP_PHY_CODBK75MUFB_SET(phy_cap, eht_cap->cb_sz_7_5_su_feedback);
	WMI_EHTCAP_PHY_TRIGSUBFFB_SET(phy_cap,
				      eht_cap->trig_su_bforming_feedback);
	WMI_EHTCAP_PHY_TRIGMUBFPARTBWFB_SET(
			phy_cap, eht_cap->trig_mu_bforming_partial_bw_feedback);
	WMI_EHTCAP_PHY_TRIGCQIFB_SET(phy_cap, eht_cap->triggered_cqi_feedback);
	WMI_EHTCAP_PHY_PARTBWDLMUMIMO_SET(phy_cap,
					  eht_cap->partial_bw_dl_mu_mimo);
	WMI_EHTCAP_PHY_PSRSR_SET(phy_cap, eht_cap->psr_based_sr);
	WMI_EHTCAP_PHY_PWRBSTFACTOR_SET(phy_cap, eht_cap->power_boost_factor);
	WMI_EHTCAP_PHY_4XEHTLTFAND800NSGI_SET(
			phy_cap, eht_cap->eht_mu_ppdu_4x_ltf_0_8_us_gi);
	WMI_EHTCAP_PHY_MAXNC_SET(phy_cap, eht_cap->max_nc);
	WMI_EHTCAP_PHY_NONTRIGCQIFB_SET(phy_cap,
					eht_cap->non_trig_cqi_feedback);
	WMI_EHTCAP_PHY_TX1024AND4096QAMLS242TONERU_SET(
			phy_cap, eht_cap->tx_1024_4096_qam_lt_242_tone_ru);
	WMI_EHTCAP_PHY_RX1024AND4096QAMLS242TONERU_SET(
			phy_cap, eht_cap->rx_1024_4096_qam_lt_242_tone_ru);
	WMI_EHTCAP_PHY_PPETHRESPRESENT_SET(phy_cap, eht_cap->ppet_present);
	WMI_EHTCAP_PHY_CMNNOMPKTPAD_SET(phy_cap,
					eht_cap->common_nominal_pkt_padding);
	WMI_EHTCAP_PHY_MAXNUMEHTLTF_SET(phy_cap, eht_cap->max_num_eht_ltf);
	WMI_EHTCAP_PHY_SUPMCS15_SET(phy_cap, eht_cap->mcs_15);
	WMI_EHTCAP_PHY_EHTDUPIN6GHZ_SET(phy_cap, eht_cap->eht_dup_6ghz);
	WMI_EHTCAP_PHY_20MHZOPSTARXNDPWIDERBW_SET(
			phy_cap, eht_cap->op_sta_rx_ndp_wider_bw_20mhz);
	WMI_EHTCAP_PHY_NONOFDMAULMUMIMOLT80MHZ_SET(
			phy_cap, eht_cap->non_ofdma_ul_mu_mimo_le_80mhz);
	WMI_EHTCAP_PHY_NONOFDMAULMUMIMO160MHZ_SET(
			phy_cap, eht_cap->non_ofdma_ul_mu_mimo_160mhz);
	WMI_EHTCAP_PHY_NONOFDMAULMUMIMO320MHZ_SET(
			phy_cap, eht_cap->non_ofdma_ul_mu_mimo_320mhz);
	WMI_EHTCAP_PHY_MUBFMRLT80MHZ_SET(phy_cap, eht_cap->mu_bformer_le_80mhz);
	WMI_EHTCAP_PHY_MUBFMR160MHZ_SET(phy_cap, eht_cap->mu_bformer_160mhz);
	WMI_EHTCAP_PHY_MUBFMR320MHZ_SET(phy_cap, eht_cap->mu_bformer_320mhz);
	WMI_EHTCAP_PHY_TBSUNDFBRATELIMIT_SET(phy_cap,
					eht_cap->tb_sounding_feedback_rl);
	WMI_EHTCAP_PHY_RX1024QAMWIDERBWDLOFDMA_SET(phy_cap,
				eht_cap->rx_1k_qam_in_wider_bw_dl_ofdma);
	WMI_EHTCAP_PHY_RX4096QAMWIDERBWDLOFDMA_SET(phy_cap,
				eht_cap->rx_4k_qam_in_wider_bw_dl_ofdma);

	peer->peer_eht_mcs_count = 0;
	rates = &params->supportedRates;

	/*
	 * Convert eht mcs to firmware understandable format
	 * BITS 0:3 indicates support for mcs 0 to 7
	 * BITS 4:7 indicates support for mcs 8 and 9
	 * BITS 8:11 indicates support for mcs 10 and 11
	 * BITS 12:15 indicates support for mcs 12 and 13
	 */
	switch (params->ch_width) {
	case CH_WIDTH_320MHZ:
		peer->peer_eht_mcs_count++;
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     0, 4, rates->bw_320_rx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     0, 4, rates->bw_320_tx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     4, 4, rates->bw_320_rx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     4, 4, rates->bw_320_tx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     8, 4, rates->bw_320_rx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     8, 4, rates->bw_320_tx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     12, 4, rates->bw_320_rx_max_nss_for_mcs_12_and_13);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX2],
			     12, 4, rates->bw_320_tx_max_nss_for_mcs_12_and_13);
		fallthrough;
	case CH_WIDTH_160MHZ:
		peer->peer_eht_mcs_count++;
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     0, 4, rates->bw_160_rx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     0, 4, rates->bw_160_tx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     4, 4, rates->bw_160_rx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     4, 4, rates->bw_160_tx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     8, 4, rates->bw_160_rx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     8, 4, rates->bw_160_rx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     12, 4, rates->bw_160_rx_max_nss_for_mcs_12_and_13);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX1],
			     12, 4, rates->bw_160_tx_max_nss_for_mcs_12_and_13);
		fallthrough;
	case CH_WIDTH_80MHZ:
	case CH_WIDTH_40MHZ:
		peer->peer_eht_mcs_count++;
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     0, 4, rates->bw_le_80_rx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     0, 4, rates->bw_le_80_tx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     4, 4, rates->bw_le_80_rx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     4, 4, rates->bw_le_80_tx_max_nss_for_mcs_0_to_9);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     8, 4, rates->bw_le_80_rx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     8, 4, rates->bw_le_80_tx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     12, 4, rates->bw_le_80_rx_max_nss_for_mcs_12_and_13);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     12, 4, rates->bw_le_80_rx_max_nss_for_mcs_12_and_13);
		break;
	case CH_WIDTH_20MHZ:
		peer->peer_eht_mcs_count++;
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     0, 4, rates->bw_20_rx_max_nss_for_mcs_0_to_7);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     0, 4, rates->bw_20_tx_max_nss_for_mcs_0_to_7);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     4, 4, rates->bw_20_rx_max_nss_for_mcs_8_and_9);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     4, 4, rates->bw_20_tx_max_nss_for_mcs_8_and_9);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     8, 4, rates->bw_20_rx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     8, 4, rates->bw_20_tx_max_nss_for_mcs_10_and_11);
		QDF_SET_BITS(peer->peer_eht_rx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     12, 4, rates->bw_20_rx_max_nss_for_mcs_12_and_13);
		QDF_SET_BITS(peer->peer_eht_tx_mcs_set[EHTCAP_TXRX_MCS_NSS_IDX0],
			     12, 4, rates->bw_20_tx_max_nss_for_mcs_12_and_13);
		break;
	default:
		break;
	}

	wma_print_eht_cap(eht_cap);
	wma_debug("Peer EHT Capabilities:");
	wma_print_eht_phy_cap(phy_cap);
	wma_print_eht_mac_cap(mac_cap);
}

void wma_vdev_set_eht_bss_params(tp_wma_handle wma, uint8_t vdev_id,
				 struct vdev_mlme_eht_ops_info *eht_info)
{
	if (!eht_info->eht_ops)
		return;
}

QDF_STATUS wma_get_eht_capabilities(struct eht_capability *eht_cap)
{
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(eht_cap->phy_cap,
		     &wma_handle->eht_cap.phy_cap,
		     WMI_MAX_EHTCAP_PHY_SIZE);
	eht_cap->mac_cap = wma_handle->eht_cap.mac_cap;
	return QDF_STATUS_SUCCESS;
}

void wma_set_peer_assoc_params_bw_320(struct peer_assoc_params *params,
				      enum phy_ch_width ch_width)
{
	if (ch_width == CH_WIDTH_320MHZ)
		params->bw_320 = 1;
}

void wma_set_eht_txbf_cfg(struct mac_context *mac, uint8_t vdev_id)
{
	wma_set_eht_txbf_params(
		vdev_id, mac->mlme_cfg->eht_caps.dot11_eht_cap.su_beamformer,
		mac->mlme_cfg->eht_caps.dot11_eht_cap.su_beamformee,
		mac->mlme_cfg->eht_caps.dot11_eht_cap.mu_bformer_le_80mhz ||
		mac->mlme_cfg->eht_caps.dot11_eht_cap.mu_bformer_160mhz ||
		mac->mlme_cfg->eht_caps.dot11_eht_cap.mu_bformer_320mhz);
}

void wma_set_eht_txbf_params(uint8_t vdev_id, bool su_bfer,
			     bool su_bfee, bool mu_bfer)
{
	uint32_t ehtmu_mode = 0;
	QDF_STATUS status;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma)
		return;

	if (su_bfer)
		WMI_VDEV_EHT_SUBFER_ENABLE(ehtmu_mode);
	if (su_bfee) {
		WMI_VDEV_EHT_SUBFEE_ENABLE(ehtmu_mode);
		WMI_VDEV_EHT_MUBFEE_ENABLE(ehtmu_mode);
	}
	if (mu_bfer)
		WMI_VDEV_EHT_MUBFER_ENABLE(ehtmu_mode);

	WMI_VDEV_EHT_DLOFDMA_ENABLE(ehtmu_mode);
	WMI_VDEV_EHT_ULOFDMA_ENABLE(ehtmu_mode);

	status = wma_vdev_set_param(wma->wmi_handle, vdev_id,
				    wmi_vdev_param_set_eht_mu_mode, ehtmu_mode);
	wma_debug("set EHTMU_MODE (ehtmu_mode = 0x%x)", ehtmu_mode);

	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to set EHTMU_MODE(status = %d)", status);
}

QDF_STATUS wma_set_bss_rate_flags_eht(enum tx_rate_info *rate_flags,
				      struct bss_params *add_bss)
{
	if (!add_bss->eht_capable)
		return QDF_STATUS_E_NOSUPPORT;

	*rate_flags |= wma_get_eht_rate_flags(add_bss->ch_width);

	wma_debug("ehe_capable %d rate_flags 0x%x", add_bss->eht_capable,
		  *rate_flags);
	return QDF_STATUS_SUCCESS;
}

bool wma_get_bss_eht_capable(struct bss_params *add_bss)
{
	return add_bss->eht_capable;
}

enum tx_rate_info wma_get_eht_rate_flags(enum phy_ch_width ch_width)
{
	enum tx_rate_info rate_flags = 0;

	if (ch_width == CH_WIDTH_320MHZ)
		rate_flags |= TX_RATE_EHT320 | TX_RATE_EHT160 |
				TX_RATE_EHT80 | TX_RATE_EHT40 | TX_RATE_EHT20;
	else if (ch_width == CH_WIDTH_160MHZ || ch_width == CH_WIDTH_80P80MHZ)
		rate_flags |= TX_RATE_EHT160 | TX_RATE_EHT80 | TX_RATE_EHT40 |
				TX_RATE_EHT20;
	else if (ch_width == CH_WIDTH_80MHZ)
		rate_flags |= TX_RATE_EHT80 | TX_RATE_EHT40 | TX_RATE_EHT20;
	else if (ch_width)
		rate_flags |= TX_RATE_EHT40 | TX_RATE_EHT20;
	else
		rate_flags |= TX_RATE_EHT20;

	return rate_flags;
}

uint16_t wma_match_eht_rate(uint16_t raw_rate,
			    enum tx_rate_info rate_flags,
			    uint8_t *nss, uint8_t *dcm,
			    enum txrate_gi *guard_interval,
			    enum tx_rate_info *mcs_rate_flag,
			    uint8_t *p_index)
{
	uint8_t index;
	uint8_t dcm_index_max = 1;
	uint8_t dcm_index;
	uint16_t match_rate = 0;
	const uint16_t *nss1_rate;
	const uint16_t *nss2_rate;

	*p_index = 0;
	if (!(rate_flags & (TX_RATE_EHT320 | TX_RATE_EHT160 | TX_RATE_EHT80 |
	      TX_RATE_EHT40 | TX_RATE_EHT20)))
		return 0;

	for (index = 0; index < MAX_EHT_MCS_IDX; index++) {
		dcm_index_max = IS_MCS_HAS_DCM_RATE(index) ? 2 : 1;
		for (dcm_index = 0; dcm_index < dcm_index_max; dcm_index++) {
			if (rate_flags & TX_RATE_EHT320) {
				nss1_rate = &eht_mcs_nss1[index].supported_eht320_rate[dcm_index][0];
				nss2_rate = &eht_mcs_nss2[index].supported_eht320_rate[dcm_index][0];
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);
				if (match_rate)
					goto rate_found;
			}
			if (rate_flags & TX_RATE_EHT160) {
				nss1_rate = &eht_mcs_nss1[index].supported_eht160_rate[dcm_index][0];
				nss2_rate = &eht_mcs_nss2[index].supported_eht160_rate[dcm_index][0];
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);
				if (match_rate)
					goto rate_found;
			}

			if (rate_flags & (TX_RATE_EHT80 | TX_RATE_EHT160)) {
				nss1_rate = &eht_mcs_nss1[index].supported_eht80_rate[dcm_index][0];
				nss2_rate = &eht_mcs_nss2[index].supported_eht80_rate[dcm_index][0];
				/* check for he80 nss1/2 rate set */
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);
				if (match_rate) {
					*mcs_rate_flag &= ~TX_RATE_EHT160;
					goto rate_found;
				}
			}

			if (rate_flags & (TX_RATE_EHT40 | TX_RATE_EHT80 |
					  TX_RATE_EHT160)) {
				nss1_rate = &eht_mcs_nss1[index].supported_eht40_rate[dcm_index][0];
				nss2_rate = &eht_mcs_nss2[index].supported_eht40_rate[dcm_index][0];
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);

				if (match_rate) {
					*mcs_rate_flag &= ~(TX_RATE_EHT80 |
							    TX_RATE_EHT160);
					goto rate_found;
				}
			}

			if (rate_flags & (TX_RATE_EHT80 | TX_RATE_EHT40 |
				TX_RATE_EHT20 | TX_RATE_EHT160)) {
				nss1_rate = &eht_mcs_nss1[index].supported_eht20_rate[dcm_index][0];
				nss2_rate = &eht_mcs_nss2[index].supported_eht20_rate[dcm_index][0];
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);

				if (match_rate) {
					*mcs_rate_flag &= TX_RATE_EHT20;
					goto rate_found;
				}
			}
		}
	}

rate_found:
	if (match_rate) {
		if (dcm_index == 1)
			*dcm = 1;
		*p_index = index;
	}
	return match_rate;
}

QDF_STATUS
wma_set_eht_txbf_vdev_params(struct mac_context *mac, uint32_t *mode)
{
	uint32_t ehtmu_mode = 0;
	bool su_bfer = mac->mlme_cfg->eht_caps.dot11_eht_cap.su_beamformer;
	bool su_bfee = mac->mlme_cfg->eht_caps.dot11_eht_cap.su_beamformee;
	bool mu_bfer =
		(mac->mlme_cfg->eht_caps.dot11_eht_cap.mu_bformer_le_80mhz ||
		 mac->mlme_cfg->eht_caps.dot11_eht_cap.mu_bformer_160mhz ||
		 mac->mlme_cfg->eht_caps.dot11_eht_cap.mu_bformer_320mhz);

	if (su_bfer)
		WMI_VDEV_EHT_SUBFER_ENABLE(ehtmu_mode);
	if (su_bfee) {
		WMI_VDEV_EHT_SUBFEE_ENABLE(ehtmu_mode);
		WMI_VDEV_EHT_MUBFEE_ENABLE(ehtmu_mode);
	}
	if (mu_bfer)
		WMI_VDEV_EHT_MUBFER_ENABLE(ehtmu_mode);
	WMI_VDEV_EHT_DLOFDMA_ENABLE(ehtmu_mode);
	WMI_VDEV_EHT_ULOFDMA_ENABLE(ehtmu_mode);
	wma_debug("set EHTMU_MODE (ehtmu_mode = 0x%x)",
		  ehtmu_mode);
	*mode = ehtmu_mode;

	return QDF_STATUS_SUCCESS;
}
#endif
