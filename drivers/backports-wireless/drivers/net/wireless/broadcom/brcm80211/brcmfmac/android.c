/*
 * Copyright 2017, Cypress Semiconductor Corporation or a subsidiary of
 * Cypress Semiconductor Corporation. All rights reserved.
 * This software, including source code, documentation and related
 * materials ("Software"), is owned by Cypress Semiconductor
 * Corporation or one of its subsidiaries ("Cypress") and is protected by
 * and subject to worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA"). If no EULA applies, Cypress hereby grants
 * you a personal, nonexclusive, non-transferable license to copy, modify,
 * and compile the Software source code solely for use in connection with
 * Cypress's integrated circuit products. Any reproduction, modification,
 * translation, compilation, or representation of this Software except as
 * specified above is prohibited without the express written permission of
 * Cypress.
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */
#include <linux/mmc/card.h>
#if defined(CPTCFG_BRCMFMAC_ANDROID)
#include <linux/wakelock.h>
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
#include <defs.h>
#include <brcmu_utils.h>
#include "core.h"
#include "android.h"
#include "cfg80211.h"
#include "debug.h"
#include "sdio.h"
#include "fwil.h"
#include "vendor.h"
#include "brcmu_utils.h"
#include "brcmu_wifi.h"

#define CMD_START		"START"
#define CMD_STOP		"STOP"
#define CMD_SCAN_ACTIVE		"SCAN-ACTIVE"
#define CMD_SCAN_PASSIVE	"SCAN-PASSIVE"
#define CMD_RSSI		"RSSI"
#define CMD_LINKSPEED		"LINKSPEED"
#define CMD_RXFILTER_START	"RXFILTER-START"
#define CMD_RXFILTER_STOP	"RXFILTER-STOP"
#define CMD_RXFILTER_ADD	"RXFILTER-ADD"
#define CMD_RXFILTER_REMOVE	"RXFILTER-REMOVE"
#define CMD_BTCOEXSCAN_START	"BTCOEXSCAN-START"
#define CMD_BTCOEXSCAN_STOP	"BTCOEXSCAN-STOP"
#define CMD_BTCOEXMODE		"BTCOEXMODE"
#define CMD_SETSUSPENDOPT	"SETSUSPENDOPT"
#define CMD_SETSUSPENDMODE	"SETSUSPENDMODE"
#define CMD_MAXDTIM_IN_SUSPEND	"MAX_DTIM_IN_SUSPEND"
#define CMD_P2P_DEV_ADDR	"P2P_DEV_ADDR"
#define CMD_SETFWPATH		"SETFWPATH"
#define CMD_SETBAND		"SETBAND"
#define CMD_GETBAND		"GETBAND"
#define CMD_COUNTRY		"COUNTRY"
#define CMD_P2P_SET_NOA		"P2P_SET_NOA"
#define CMD_MIRACAST		"MIRACAST"
#define CMD_INTERFACE_CREATE	"INTERFACE_CREATE"
#define CMD_INTERFACE_DELETE	"INTERFACE_DELETE"
#define CMD_DATARATE		"DATARATE"
#define CMD_ADDIE		"ADD_IE"
#define CMD_DELIE		"DEL_IE"
#define CMD_SET_WPS_P2PIE	"SET_AP_WPS_P2P_IE"
#define CMD_ASSOC_CLIENTS	"ASSOCLIST"
#define CMD_GET_MODE		"MODE"
#define CMD_SET_AMPDU_MPDU	"AMPDU_MPDU"
#define CMD_GET_CHANSPEC	"CHANSPEC"
#define CMD_HAPD_MAC_FILTER	"HAPD_MAC_FILTER"
#define CMD_SET_HAPD_AUTO_CHANNEL	"HAPD_AUTO_CHANNEL"
#define CMD_HIDDEN_SSID		"HIDE_SSID"
#define CMD_SET_CSA		"SETCSA"
#define CMD_COUNTRYREV_GET	"GETCOUNTRYREV"
#define CMD_COUNTRYREV_SET	"SETCOUNTRYREV"

#define DEFAULT_WIFI_TURNON_DELAY	200

/* miracast related definition */
#define MIRACAST_MODE_OFF		0
#define MIRACAST_MODE_SOURCE		1
#define MIRACAST_MODE_SINK		2

#define MIRACAST_AMPDU_SIZE		8

#define DOT11_MNG_PROPR_ID	221
#define VNDR_IE_HDR_LEN		2	/* id + len field */
#define VNDR_IE_MIN_LEN		3	/* size of the oui field */
#define VNDR_IE_MAX_LEN		256	/* verdor IE max length */

#define MAX_NUM_OF_ASSOCLIST	64
#define ETHER_ADDR_LEN		6	/* length of an Ethernet address */

/* For ioctls that take a list of MAC addresses */
struct maclist {
	uint count;		/**< number of MAC addresses */
	struct ether_addr ea[1]; /**< variable length array of MAC addresses */
};

/* Bandwidth */
#define WL_CH_BANDWIDTH_20MHZ 20
#define WL_CH_BANDWIDTH_40MHZ 40
#define WL_CH_BANDWIDTH_80MHZ 80
#define WL_CH_BANDWIDTH_160MHZ 160

#define CSA_BROADCAST_ACTION_FRAME	0	/* csa broadcast action frame */
#define CSA_UNICAST_ACTION_FRAME	1	/* csa unicast action frame */

/* Channel Switch Announcement param */
struct brcmf_chan_switch {
	u8 mode;	/* value 0 or 1 */
	u8 count;	/* count # of beacons before switching */
	u16 chspec;	/* chanspec */
	u8 reg;		/* regulatory class */
	u8 frame_type;	/* csa frame type, unicast or broadcast */
};

u16 chan_to_chanspec(struct brcmu_d11inf *d11inf,
		     u16 channel)
{
	struct brcmu_chan ch_inf;

	ch_inf.chnum = channel;
	ch_inf.bw = BRCMU_CHAN_BW_20;
	d11inf->encchspec(&ch_inf);

	return ch_inf.chspec;
}

#define WLC_IOCTL_SMLEN		256	/* "small" len ioctl buffer required */
#define WLC_CNTRY_BUF_SZ	4	/* Country string is 3 bytes + NUL */

struct brcmf_country {
	char country_abbrev[WLC_CNTRY_BUF_SZ];	/* nul-terminated country
						 * code used in the Country
						 * IE
						 */
	int rev;			/* revision specifier for ccode on
					 * set, -1 indicates unspecified on
					 * get, rev >= 0
					 */
	char ccode[WLC_CNTRY_BUF_SZ];	/* nul-terminated built-in country
					 * code. variable length, but fixed
					 * size in struct allows simple
					 * allocation for expected country
					 * strings <= 3 chars.
					 */
};

/* hostap mac mode */
#define MACLIST_MODE_DISABLED	0
#define MACLIST_MODE_DENY	1
#define MACLIST_MODE_ALLOW	2

/* max number of mac filter list
 * restrict max number to 10 as maximum cmd string size is 255
 */
#define MAX_NUM_MAC_FILT	10

/* miracast related definition */
#define MIRACAST_MODE_OFF	0
#define MIRACAST_MODE_SOURCE	1
#define MIRACAST_MODE_SINK	2

#ifndef MIRACAST_MCHAN_ALGO
#define MIRACAST_MCHAN_ALGO     1
#endif

#ifndef MIRACAST_MCHAN_BW
#define MIRACAST_MCHAN_BW	25
#endif

#ifndef MIRACAST_AMPDU_SIZE
#define MIRACAST_AMPDU_SIZE	8
#endif

#define PM_OFF	0

static u8 miracast_cur_mode;

/* Used to get specific STA parameters */
struct scb_val {
	uint val;
	struct ether_addr ea;
};

#if defined(CPTCFG_BRCMFMAC_ANDROID)
int brcmf_android_wifi_on(struct brcmf_pub *drvr, struct net_device *ndev)
{
	int ret = 0;
	struct brcmf_android *android = drvr->android;

	brcmf_dbg(ANDROID, "enter\n");

	if (!ndev) {
		brcmf_err("net device is null\n");
		return -EINVAL;
	}

	if (!android) {
		brcmf_err("not supported\n");
		return -EOPNOTSUPP;
	}

	if (!(android->wifi_on)) {
		ret = brcmf_set_power(true, DEFAULT_WIFI_TURNON_DELAY);
		if (ret) {
			brcmf_err("power up wifi chip failed, err=%d\n", ret);
			return ret;
		}
		android->wifi_on = true;
	}

	return ret;
}

int
brcmf_android_wifi_off(struct brcmf_pub *drvr, struct net_device *ndev)
{
	struct brcmf_if *ifp =  netdev_priv(ndev);
	struct brcmf_android *android = drvr->android;
	int ret = 0;

	brcmf_dbg(ANDROID, "enter\n");

	if (!android) {
		brcmf_err("not supported\n");
		return -EOPNOTSUPP;
	}

	if (android->wifi_on) {
		if (android->init_done)
			ret = brcmf_fil_cmd_int_set(ifp, BRCMF_C_DOWN, 1);
		brcmf_set_power(false, 0);
		android->wifi_on = false;
	}

	return ret;
}
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */

static int brcmf_android_set_suspendmode(struct net_device *ndev,
					 char *command, int total_len)
{
	int ret = 0;

#if !defined(CONFIG_HAS_EARLYSUSPEND)
	int suspend_flag;

	brcmf_dbg(ANDROID, "enter\n");

	suspend_flag = *(command + strlen(CMD_SETSUSPENDMODE) + 1) - '0';
	if (suspend_flag != 0 && suspend_flag != 1)
		return -EINVAL;

	ret = brcmf_pktfilter_enable(ndev, (bool)suspend_flag);
	if (ret)
		brcmf_err("suspend failed\n");
#endif

	return ret;
}

static int brcmf_android_set_country(struct net_device *ndev, char *command,
				     int total_len)
{
#if defined(CPTCFG_BRCMFMAC_ANDROID)
	struct brcmf_if *ifp =  netdev_priv(ndev);
	struct brcmf_pub *drvr = ifp->drvr;
	struct brcmf_android *android = drvr->android;
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
	char *country_code = command + strlen(CMD_COUNTRY) + 1;
	int ret = 0;

	ret = brcmf_set_country(ndev, country_code);

#if defined(CPTCFG_BRCMFMAC_ANDROID)
	if (!ret)
		strncpy(android->country, country_code, 2);
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */

	return ret;
}

static
int brcmf_android_set_btcoexmode(struct net_device *ndev, char *command,
				 int total_len)
{
	int ret = 0;
	int btcoex_mode = 0;

	btcoex_mode = *(command + strlen(CMD_BTCOEXMODE) + 1) - '0';

	if (btcoex_mode == 1) {
	/* Enable to BTCOEXMODE with DHCP setup */
		ret = brcmf_crit_proto_start(ndev);
	} else if (btcoex_mode == 0) {
	/* Disable to BTCOEXMODE with DHCP setup */
		ret = brcmf_crit_proto_stop(ndev);
	} else if (btcoex_mode == 2) {
	/* Set the coex mode back to its default value (Post DHCP setup) */
		ret = brcmf_crit_proto_stop(ndev);
	} else {
		brcmf_err("unknown btcode mode(%d)\n", btcoex_mode);
		ret = -EINVAL;
	}

	return ret;
}

static
int brcmf_android_pktfilter_add(struct net_device *ndev, char *command,
				int total_len)
{
	int ret = 0;
	int filter_num = *(command + strlen(CMD_RXFILTER_ADD) + 1) - '0';

	ret = brcmf_pktfilter_add_remove(ndev, filter_num, true);

	return ret;
}

static
int brcmf_android_pktfilter_remove(struct net_device *ndev, char *command,
				   int total_len)
{
	int ret = 0;
	int filter_num = *(command + strlen(CMD_RXFILTER_REMOVE) + 1) - '0';

	ret = brcmf_pktfilter_add_remove(ndev, filter_num, false);

	return ret;
}

static
int brcmf_privcmd_get_band(struct net_device *ndev, char *command,
			   int total_len)
{
	struct brcmf_if *ifp =  netdev_priv(ndev);
	uint band;
	int ret;
	int err = 0;

	err = brcmf_fil_cmd_int_get(ifp, BRCMF_C_GET_BAND, &band);
	if (err)
		return err;

	ret = snprintf(command, total_len, "Band:%d", band);

	return ret;
}

static
void brcmf_privcmd_set_passive_scan(struct brcmf_if *ifp,
				    char *command, int total_len)
{
	struct brcmf_pub *drvr = ifp->drvr;
	struct brcmf_cfg80211_info *cfg = NULL;

	if (drvr) {
		cfg =  drvr->config;
	} else {
		brcmf_err("drvr is null\n");
		return;
	}

	brcmf_err("cmd:%s\n", command);

	if (strcmp(command, CMD_SCAN_ACTIVE) == 0)
		cfg->active_scan = true;
	else if (strcmp(command, CMD_SCAN_PASSIVE) == 0)
		cfg->active_scan = false;
	else
		brcmf_err("%s unknown cmd\n", command);
}

static
int brcmf_get_rssi(struct brcmf_if *ifp,
		   struct brcmf_scb_val_le *scb_val)
{
	int err;

	err = brcmf_fil_cmd_data_get(ifp, BRCMF_C_GET_RSSI,
				     scb_val, sizeof(struct brcmf_scb_val_le));
	if (err)
		brcmf_err("Could not get rssi (%d)\n", err);
	return err;
}

static
int brcmf_privcmd_get_rssi(struct brcmf_if *ifp, char *command, int total_len)
{
	int bytes_written = 0;
	int error = 0;
	struct brcmf_scb_val_le scbval;
	char *tmp = NULL;

	tmp = strchr(command, ' ');
	/* For Ap mode rssi command would be
	 * driver rssi <sta_mac_addr>
	 * for STA/GC mode
	 */
	if (tmp) {
		/* Ap/GO mode
		 * driver rssi <sta_mac_addr>
		 */
		int i;

		brcmf_dbg(TRACE, "cmd:%s\n", tmp);
		/* skip space from delim after finding char */
		tmp++;
		if (!(brcmu_ether_atoe((tmp), scbval.ea)))
			return -1;
		scbval.val = 0;
		for (i = 0; i < 6; i++)
			brcmf_err("%02x\n", scbval.ea[i]);
	} else {
		/* STA/GC mode */
		memset(&scbval, 0, sizeof(struct brcmf_scb_val_le));
	}
	error = brcmf_get_rssi(ifp, &scbval);
	if (error)
		return -1;

	bytes_written += snprintf(command, total_len, "rssi:%d", scbval.val);
	command[bytes_written] = '\0';
	return bytes_written;
}

static
int brcmf_privcmd_get_link_speed(struct brcmf_if *ifp,
				 char *command, int total_len)
{
	int err;
	int plink_speed = 0;
	int bytes_written = 0;

	err = brcmf_fil_cmd_data_get(ifp, BRCMF_C_GET_RATE,
				     &plink_speed, sizeof(int));
	if (err) {
		brcmf_err("Could not get link speed (%d)\n", err);
		return err;
	}

	/* Convert internal 500Kbps to android mbps */
	plink_speed = plink_speed / 2;
	bytes_written = snprintf(command, total_len, "link_speed %d",
				 plink_speed);

	return bytes_written;
}

static
int brcmf_privcmd_get_data_rate(struct brcmf_if *ifp,
				char *command, int total_len)
{
	int err;
	int data_rate = 0;
	int bytes_written = 0;

	err = brcmf_fil_cmd_data_get(ifp, BRCMF_C_GET_RATE,
				     &data_rate, sizeof(int));
	if (err) {
		brcmf_err("Could not get data rate (%d)\n", err);
		return err;
	}

	bytes_written = snprintf(command, total_len, "datarate:%d",
				 (data_rate / 2));

	return bytes_written;
}

static
int brcmf_privcmd_add_ie(struct brcmf_if *ifp,
			 char *command, int total_len)
{
	struct brcmf_cfg80211_vif *vif = ifp->vif;
	int datalen = 0, idx = 0, tot_len = 0;
	u8 ie_buf[VNDR_IE_MAX_LEN];
	char *pcmd = NULL;
	char hex[] = "XXX";
	u16 pktflag;
	s32 err = 0;
	struct tlv_t *vndr_ie = NULL;

	pcmd = kzalloc((total_len + 1), GFP_KERNEL);
	if (!pcmd)
		return -ENOMEM;

	pcmd = command + strlen(CMD_ADDIE) + 1;

	pktflag = simple_strtoul(pcmd, &pcmd, 10);

	pcmd = pcmd + 1;

	for (idx = 0; idx < DOT11_OUI_LEN; idx++) {
		hex[0] = *pcmd++;
		hex[1] = *pcmd++;
		hex[2] = '\0';
		brcmf_err("hex:%s\n", hex);
		err = kstrtou8(&hex[0], 16, &ie_buf[idx]);
		if (err) {
			brcmf_err("kstrou8 failed %d\n", err);
			kfree(pcmd);
			return err;
		}
	}
	pcmd++;
	brcmf_err("pcmd:%s\n", pcmd);
	while ((*pcmd != '\0') && (idx < VNDR_IE_MAX_LEN)) {
		hex[0] = *pcmd++;
		hex[1] = *pcmd++;
		hex[2] = '\0';
		brcmf_err("hex:%s\n", hex);
		err = kstrtou8(&hex[0], 16, &ie_buf[idx]);
		brcmf_err("ie_buf:%02X\n", ie_buf[idx]);
		idx++;
		datalen++;
	}
	tot_len = sizeof(struct tlv_t) + (datalen - 1);
	vndr_ie = kzalloc(tot_len, GFP_KERNEL);
	if (!vndr_ie) {
		brcmf_err(("IE memory alloc failed\n"));
		kfree(pcmd);
		return -ENOMEM;
	}
	vndr_ie->id = DOT11_MNG_PROPR_ID;

	vndr_ie->len = datalen;
	memcpy(vndr_ie->oui, &ie_buf[0], DOT11_OUI_LEN);
	vndr_ie->oui_type = 4;
	memcpy(vndr_ie->data, &ie_buf[DOT11_OUI_LEN], datalen);

	err = brcmf_vif_set_mgmt_ie(vif, pktflag, (const u8 *)vndr_ie, tot_len);
	if (err) {
		brcmf_err("set ie failed with err:%d\n", err);
		err = -EINVAL;
	}

	kfree(vndr_ie);
	kfree(pcmd);
	return err;
}

static
int brcmf_privcmd_del_ie(struct brcmf_if *ifp, char *command,
			 int total_len)
{
	struct brcmf_cfg80211_vif *vif = ifp->vif;
	u16 pktflag;
	char *pcmd = NULL;
	s32 err = 0;
	char hex[] = "XXX";

	pcmd = command + strlen(CMD_DELIE) + 1;
	hex[0] = *pcmd;
	hex[1] = '\0';

	err = kstrtou16(&hex[0], 10, &pktflag);
	if (err) {
		brcmf_err("kstrou16 failed %d\n", err);
		kfree(pcmd);
		return err;
	}
	err = brcmf_vif_set_mgmt_ie(vif, pktflag, NULL, 0);
	if (err) {
		brcmf_err("delete ie failed:%d\n", err);
		err = -EINVAL;
	}
	memset(&vif->saved_ie, 0, sizeof(vif->saved_ie));
	return err;
}

int
brcmf_privcmd_get_assoclist(struct brcmf_if *ifp, char *command,
			    int total_len)
{
	int  error = 0;
	int bytes_written = 0;
	uint i;
	char mac_buf[MAX_NUM_OF_ASSOCLIST *
		sizeof(struct ether_addr) + sizeof(uint)] = {0};

	struct maclist *assoc_maclist = (struct maclist *)mac_buf;

	brcmf_info("%s: ENTER\n", __func__);

	assoc_maclist->count = MAX_NUM_OF_ASSOCLIST;

	error = brcmf_fil_cmd_data_get(ifp, BRCMF_C_GET_ASSOCLIST,
				       assoc_maclist, sizeof(mac_buf));
	if (error)
		return -1;

	assoc_maclist->count = assoc_maclist->count;
	bytes_written = snprintf(command, total_len, "%slistcount:%d Stations:",
				 CMD_ASSOC_CLIENTS,
				 assoc_maclist->count);

	for (i = 0; i < assoc_maclist->count; i++) {
		bytes_written += snprintf(command + bytes_written, total_len,
					  " " "%02X:%02X:%02X:%02X:%02X:%02X",
					  assoc_maclist->ea[i].octet[0],
					  assoc_maclist->ea[i].octet[1],
					  assoc_maclist->ea[i].octet[2],
					  assoc_maclist->ea[i].octet[3],
					  assoc_maclist->ea[i].octet[4],
					  assoc_maclist->ea[i].octet[5]);
	}
	return bytes_written;
}

int
brcmf_privcmd_get_chanspec(struct brcmf_if *ifp, char *command,
			   int total_len)
{
	int err = 0;
	unsigned int chanspec;
	int band = 0;
	int bw = 0;
	u8 channel;
	u8 sb = 0;
	int bytes_written = 0;

	err = brcmf_fil_iovar_int_get(ifp, "chanspec", &chanspec);
	if (err) {
		brcmf_err("chanspec failed (%d)\n", err);
		return err;
	}
	brcmf_dbg(TRACE, "chanspec (%x)\n", chanspec);
	channel = CHSPEC_CHANNEL(chanspec);
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);

	switch (bw) {
	case WL_CHANSPEC_BW_80:
			bw = WL_CH_BANDWIDTH_80MHZ;
			break;
	case WL_CHANSPEC_BW_40:
			bw = WL_CH_BANDWIDTH_40MHZ;
			break;
	case WL_CHANSPEC_BW_20:
			bw = WL_CH_BANDWIDTH_20MHZ;
			break;
	default:
		bw = WL_CH_BANDWIDTH_20MHZ;
		break;
	}

	if (bw == WL_CH_BANDWIDTH_40MHZ) {
		if (CHSPEC_SB_UPPER(chanspec))
			channel += CH_10MHZ_APART;
		else
			channel -= CH_10MHZ_APART;
	} else if (bw == WL_CH_BANDWIDTH_80MHZ) {
		sb = chanspec & WL_CHANSPEC_CTL_SB_MASK;
		if (sb == WL_CHANSPEC_CTL_SB_LL)
			channel -= (CH_10MHZ_APART + CH_20MHZ_APART);
		else if (sb == WL_CHANSPEC_CTL_SB_LU)
			channel -= CH_10MHZ_APART;
		else if (sb == WL_CHANSPEC_CTL_SB_UL)
			channel += CH_10MHZ_APART;
		else
			/* WL_CHANSPEC_CTL_SB_UU */
			channel += (CH_10MHZ_APART + CH_20MHZ_APART);
	}
	brcmf_info("band:%x bw:%d\n", band, bw);
	bytes_written = snprintf(command, total_len,
				 "%s:channel=%d band=%s bw=%d",
				 CMD_GET_CHANSPEC, channel, band ==
				 WL_CHANSPEC_BAND_5G ? "5G" : "2G", bw);

	return bytes_written;
}

int
brcmf_privcmd_80211_get_mode(struct brcmf_if *ifp,
			     char *command, int total_len)
{
	int err = 0;
	int band = 0;
	char cap[4];
	int bw = 0;
	int channel = 0;
	unsigned int chanspec;
	u8 *buf = NULL;
	int bytes_written = 0;
	//	struct wl_bss_info *bss = NULL;
	struct brcmf_bss_info_le *bss = NULL;

	buf = kzalloc(WL_BSS_INFO_MAX, GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		return err;
	}
	*(__le32 *)buf = cpu_to_le32(WL_BSS_INFO_MAX);

	err = brcmf_fil_cmd_data_get(ifp, BRCMF_C_GET_BSS_INFO,
				     buf, WL_BSS_INFO_MAX);
	if (err) {
		brcmf_err("WLC_GET_BSS_INFO failed: %d\n", err);
		kfree(buf);
		buf = NULL;
		return err;
	}

	bss = (struct brcmf_bss_info_le  *)(buf + 4);
	chanspec = le16_to_cpu(bss->chanspec);
	brcmf_err("chanspec (%x)\n", chanspec);
	channel = CHSPEC_CHANNEL(chanspec);
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);
	brcmf_dbg(INFO, "band:%x bw:%d\n", band, bw);

	if (band == WL_CHANSPEC_BAND_2G) {
		if (bss->n_cap)
			strcpy(cap, "n");
		else
			strcpy(cap, "bg");
	} else if (band == WL_CHANSPEC_BAND_5G) {
		if (bw == WL_CHANSPEC_BW_80) {
			strcpy(cap, "ac");
		} else if (bw == WL_CHANSPEC_BW_40 ||
			   bw == WL_CHANSPEC_BW_20) {
			if ((bss->nbss_cap & 0xf00) && bss->n_cap)
				strcpy(cap, "n|ac");
			else if (bss->vht_cap)
				strcpy(cap, "ac");
			else if (bss->n_cap)
				strcpy(cap, "n");
			else
				strcpy(cap, "a");
		} else {
			brcmf_err("%s:Mode get failed\n", __func__);
			kfree(buf);
			buf = NULL;
			return -EINVAL;
		}
	}
	bytes_written = snprintf(command, total_len, "%s %s",
				 CMD_GET_MODE, cap);
	kfree(buf);
	buf = NULL;
	return bytes_written;
}

int
brcmf_privcmd_ampdu_mpdu(struct brcmf_if *ifp, char *command, int total_len)
{
	int err = 0;
	int ampdu_mpdu;
	int skip = strlen(CMD_SET_AMPDU_MPDU) + 1;

	err = kstrtoint((command + skip), 10, &ampdu_mpdu);
	if (err) {
		brcmf_err("strtoint failed with err:%d\n", err);
		return err;
	}

	if (ampdu_mpdu > 32) {
		brcmf_err("ampdu_mpdu MAX value is 32.\n");
		return -1;
	}
	err = brcmf_fil_iovar_data_set(ifp, "ampdu_mpdu", &ampdu_mpdu,
				       sizeof(int));
	if (err < 0) {
		brcmf_err("ampdu_mpdu set error. %d\n", err);
		return -1;
	}

	return 0;
}

static int brcmf_privcmd_set_csa(struct brcmf_if *ifp, char *cmd, int total_len)
{
	struct brcmf_chan_switch csa_arg;
	struct brcmf_cfg80211_info *cfg = ifp->drvr->config;
	int err = 0;
	char str[3];
	u16 chnsp;
	u8 channel = 0;

	brcmf_err("%s:cmd:%d\n", cmd, __LINE__);

	cmd = (cmd + strlen(CMD_SET_CSA));

	/* Order is mode, count, channel
	 * DRIVER SETCSA 0 10 1 u
	 */
	if (!*++cmd) {
		brcmf_err("%d:error missing arguments\n", __LINE__);
		return -1;
	}

	memset(&str, 0, 5);

	str[0] = *cmd++;
	str[1] = '\0';

	err = kstrtou8(str, 10, &csa_arg.mode);

	if (err) {
		brcmf_err("%d:kstrtou8 failed %d\n", __LINE__, err);
		return err;
	}

	if (csa_arg.mode != 0 && csa_arg.mode != 1) {
		brcmf_err("%d:Invalid mode\n", __LINE__);
		return -1;
	}

	if (!*++cmd) {
		brcmf_err("%d:error missing count\n", __LINE__);
		return -1;
	}

	str[0] = *cmd++;
	if (*cmd != ' ') {
		str[1] = *cmd++;
		str[2] = '\0';
	} else {
		str[1] = '\0';
	}

	err = kstrtou8(str, 10, &csa_arg.count);
	if (err) {
		brcmf_err("%d:kstrtoul failed %d\n", __LINE__, err);
		return err;
	}

	csa_arg.reg = 0;
	csa_arg.chspec = 0;

	if (!*++cmd) {
		brcmf_err("%d:error missing channel\n", __LINE__);
		return -1;
	}

	str[0] = *cmd++;
	if (*cmd != ' ') {
		str[1] = *cmd++;
		str[2] = '\0';
	} else {
		str[1] = '\0';
	}

	err = kstrtou8(str, 10, &channel);
	if (err) {
		brcmf_err("%d:kstrtou16 failed %d\n", __LINE__, err);
		return err;
	}

	chnsp = chan_to_chanspec(&cfg->d11inf, channel);

	if (chnsp == 0) {
		brcmf_err("%d:chsp is not correct\n", __LINE__);
		return -1;
	}
	csa_arg.chspec = chnsp;

	/* csa action frame type */
	if (*++cmd) {
		if (strcmp(cmd, "u") == 0) {
			csa_arg.frame_type = CSA_UNICAST_ACTION_FRAME;
		} else {
			brcmf_err("%d:error: invalid frame type: %s\n",
				  __LINE__, cmd);
			return -1;
		}
	} else {
		csa_arg.frame_type = CSA_BROADCAST_ACTION_FRAME;
	}

	if (csa_arg.chspec & WL_CHANSPEC_BAND_5G) {
		int chanspec = csa_arg.chspec;

		err = brcmf_fil_iovar_data_get(ifp, "per_chan_info", &chanspec,
					       sizeof(int));
		if (!err) {
			if ((chanspec & WL_CHAN_RADAR) ||
			    (chanspec & WL_CHAN_PASSIVE)) {
				brcmf_err("%d:Channel is radar sensitive\n",
					  __LINE__);
				return -1;
			}
			if (chanspec == 0) {
				brcmf_err("%d:Invalid hw channel\n", __LINE__);
				return -1;
			}
		} else {
			brcmf_err("%d:does not support per_chan_info\n",
				  __LINE__);
			return -1;
		}
		brcmf_info("%d:non radar sensitivity\n", __LINE__);
	}

	err = brcmf_fil_iovar_data_set(ifp, "csa", &csa_arg, sizeof(csa_arg));
	if (err < 0) {
		brcmf_err("%d:set csa failed:%d\n", __LINE__, err);
		return -1;
	}
	return 0;
}

static int
brcmf_privcmd_set_band(struct brcmf_if *ifp, u32 band)
{
	int err = 0;
	struct brcmf_cfg80211_info *cfg = ifp->drvr->config;
	struct wiphy *wiphy = NULL;

	if (band == WLC_BAND_AUTO ||
	    band == WLC_BAND_5G ||
	    band == WLC_BAND_2G)
		err = brcmf_fil_cmd_int_set(ifp, BRCMF_C_SET_BAND, band);
	if (!err) {
		if (cfg && cfg->wiphy) {
			wiphy = cfg->wiphy;
			err = brcmf_setup_wiphybands(wiphy);
		}
	} else {
		brcmf_err("setband failed with:%d\n", band);
	}
	return err;
}

static int
brcmf_privcmd_hidden_ssid(struct brcmf_if *ifp, char *cmd, int tot_len)
{
	int bytes_written = 0;
	int hidden_ssid = 0;
	int err = 0;
	struct brcmf_pub *drvr = ifp->drvr;
	struct brcmf_cfg80211_info *cfg = NULL;

	if (drvr && drvr->config) {
		cfg =  drvr->config;
	} else {
		brcmf_err("drvr is null\n");
		return -1;
	}
	if (*(cmd + strlen(CMD_HIDDEN_SSID)) == '\0') {
		bytes_written += snprintf(cmd, tot_len, "hidden:%d",
					  cfg->hidden_ssid);
		cmd[bytes_written] = '\0';
		return bytes_written;
	}
	err = kstrtoint((cmd + strlen(CMD_HIDDEN_SSID) + 1), 10, &hidden_ssid);
	if (err) {
		brcmf_err("strtoint failed with err:%d\n", err);
		return err;
	}
	cfg->hidden_ssid = hidden_ssid;

	err = brcmf_fil_iovar_int_set(ifp, "closednet", hidden_ssid);
	if (err)
		brcmf_err("failed with:%d\n", err);

	return err;
}

static int brcmf_privcmd_get_country_rev(
	struct brcmf_if *ifp, char *cmd, int total_len)
{
	struct brcmf_country cspec;
	int bytes_written;
	int err = 0;
	char smbuf[WLC_IOCTL_SMLEN];

	/* Usage
	 * DRIVER GETCOUNTRYREV
	 */
	brcmf_dbg(TRACE, "Line:%d cmd:%s\n", __LINE__, cmd);

	err = brcmf_fil_iovar_data_get(ifp, "country", smbuf, sizeof(smbuf));

	if (!err) {
		memcpy(&cspec, smbuf, sizeof(cspec));
		brcmf_info("Line:%d get country '%c%c %d'\n",
			   __LINE__, cspec.ccode[0],
			   cspec.ccode[1], cspec.rev);
	} else {
		brcmf_err("Line:%d get country rev failed with err:%d\n",
			  __LINE__, err);
		return -1;
	}

	bytes_written = snprintf(cmd, total_len, "%s %c%c %d",
				 CMD_COUNTRYREV_GET, cspec.ccode[0],
				 cspec.ccode[1], cspec.rev);

	return bytes_written;
}

static int
brcmf_privcmd_set_country_rev(struct brcmf_if *ifp, char *cmd, int total_len)
{
	struct brcmf_pub *drvr = ifp->drvr;
	struct brcmf_cfg80211_info *cfg = drvr->config;
	struct wiphy *wiphy = cfg_to_wiphy(cfg);
	struct brcmf_country cspec = {{0}, 0, {0} };
	char country_code[WLC_CNTRY_BUF_SZ];
	int rev = 0;
	int err = 0;

	/* Usage
	 * DRIVER SETCOUNTRYREV US
	 */
	memset(country_code, 0, sizeof(country_code));
	err = sscanf(cmd + sizeof("SETCOUNTRYREV"),
		     "%10s %10d", country_code, &rev);
	if (!err) {
		brcmf_err("Line:%d: Failed to get Parameter\n", __LINE__);
		return -1;
	}

	brcmf_dbg(TRACE, "Line:%d country_code = %s, rev = %d\n",
		  __LINE__, country_code, rev);

	memcpy(cspec.country_abbrev, country_code, sizeof(country_code));
	memcpy(cspec.ccode, country_code, sizeof(country_code));
	cspec.rev = rev;

	err = brcmf_fil_iovar_data_set(ifp, "country", &cspec, sizeof(cspec));

	if (err) {
		brcmf_err("Line:%d set country '%s/%d' failed error code %d\n",
			  __LINE__, cspec.ccode, cspec.rev, err);
	} else {
		brcmf_setup_wiphybands(wiphy);
		brcmf_dbg(TRACE, "Line:%d set country '%s/%d'\n",
			  __LINE__, cspec.ccode, cspec.rev);
	}

	return err;
}

int
brcmf_privcmd_set_ap_mac_list(struct brcmf_if *ifp, int macmode,
			      struct maclist *maclist)
{
	int i, j, match;
	int ret = 0;
	int err = 0;
	char mac_buf[MAX_NUM_OF_ASSOCLIST *
		sizeof(struct ether_addr) + sizeof(uint)] = {0};
	struct maclist *assoc_maclist = (struct maclist *)mac_buf;

	/* set filtering mode */
	err = brcmf_fil_cmd_int_set(ifp, BRCMF_C_SET_MACMODE, macmode);
	if (err) {
		brcmf_err("set macmode err %d\n", err);
		return err;
	}

	if (macmode != MACLIST_MODE_DISABLED) {
		/* set the MAC filter list */
		err = brcmf_fil_cmd_data_set(ifp, BRCMF_C_SET_MACLIST, maclist,
					     sizeof(int) +
					     sizeof(struct ether_addr) *
					     maclist->count);

		if (err) {
			brcmf_err("set maclist err %d\n", err);
			return err;
		}

		/* get the current list of associated STAs */
		assoc_maclist->count = MAX_NUM_OF_ASSOCLIST;
		err = brcmf_fil_cmd_data_get(ifp, BRCMF_C_GET_ASSOCLIST,
					     assoc_maclist, sizeof(mac_buf));

		if (err) {
			brcmf_err("get assoclist err %d\n", err);
			return err;
		}

		/* do we have any STA associated? */
		if (assoc_maclist->count) {
			/* iterate each associated STA */
			for (i = 0; i < assoc_maclist->count; i++) {
				match = 0;
				/* compare with each entry */
				for (j = 0; j < maclist->count; j++) {
					if (memcmp(assoc_maclist->ea[i].octet,
						   maclist->ea[j].octet,
						   ETHER_ADDR_LEN) == 0) {
						match = 1;
						break;
					}
				}
				/* do conditional deauth */
				/* "if not in the allow list" */
				/* or "if in the deny list" */
				if ((macmode == MACLIST_MODE_ALLOW && !match) ||
				    (macmode == MACLIST_MODE_DENY && match)) {
					struct scb_val scbval;

					scbval.val = 1;
					memcpy(&scbval.ea,
					       &assoc_maclist->ea[i],
					       ETHER_ADDR_LEN);
					err = brcmf_fil_cmd_data_set(ifp, BRCMF_C_SCB_DEAUTHENTICATE_FOR_REASON,
								     &scbval,
								     sizeof(
								     struct
								     scb_val));
					if (err)
						return -1;
				}
			}
		}
	}
	return ret;
}

/* HAPD_MAC_FILTER mac_mode mac_cnt mac_addr1 mac_addr2 ..
 */
static int
brcmf_privcmd_set_mac_address_filter(struct brcmf_if *ifp, char *cmd,
				     int total_len)
{
	int i;
	int macnum = 0;
	int macmode = MACLIST_MODE_DISABLED;
	int err = 0;
	struct maclist *list;
	char *token;
	char str[3];

	/* Usage
	 * string should look like below (macmode/macnum/maclist)
	 * 1 2 00:11:22:33:44:55 00:11:22:33:44:ff
	 * DRIVER HAPD_MAC_FILTER 1 2 00:11:22:33:44:55 00:11:22:33:44:ff
	 */

	brcmf_dbg(TRACE, "Line:%d cmd:%s\n", __LINE__, cmd);

	cmd = (cmd + strlen(CMD_HAPD_MAC_FILTER));
	if (!*++cmd) {
		brcmf_err("Line:%d:error missing arguments\n", __LINE__);
		return -1;
	}

	memset(&str, 0, 3);

	/* get the MAC filter mode */
	str[0] = *cmd++;
	str[1] = '\0';
	err = kstrtoint(str, 10, &macmode);
	if (err) {
		brcmf_err("Line:%d kstrtoint failed with error:%d\n",
			  __LINE__, err);
		return err;
	}

	if (macmode < MACLIST_MODE_DISABLED || macmode > MACLIST_MODE_ALLOW) {
		brcmf_err("Line:%d invalid macmode %d\n", __LINE__, macmode);
		return -1;
	}

	cmd++;

	/* get the macnum */
	str[0] = *cmd++;
	if (*cmd != ' ') {
		str[1] = *cmd++;
		str[2] = '\0';
	} else {
		str[1] = '\0';
	}

	err = kstrtoint(str, 10, &macnum);
	if (err) {
		brcmf_err("Line:%d kstrtoint failed with error:%d\n",
			  __LINE__, err);
		return err;
	}

	if (macnum <= 0 || macnum > MAX_NUM_MAC_FILT) {
		brcmf_err("Line:%d : invalid number of MAC address entries:%d\n",
			  __LINE__, macnum);
		return -1;
	}

	/* allocate memory for the MAC list */
	list = kmalloc(sizeof(int) +
		sizeof(struct ether_addr) * macnum, GFP_KERNEL);
	if (!list)
		return -1;

	/* prepare the MAC list */
	list->count = macnum;

	for (i = 0; i < list->count; i++) {
		token = strsep((char **)&cmd, " ");
		if (!(brcmu_ether_atoe(cmd, list->ea[i].octet))) {
			kfree(list);
			return -1;
		}
		brcmf_dbg(TRACE, "Line:%d: %d/%d MACADDR=%s",
			  __LINE__, i, list->count, cmd);
	}

	/* set the list */
	err = brcmf_privcmd_set_ap_mac_list(ifp, macmode, list);
	if (err) {
		brcmf_err("Line:%d Setting MAC list failed error:%d\n",
			  __LINE__, err);
	}

	kfree(list);

	return 0;
}

/* SoftAP feature */
#define APCS_BAND_2G_LEGACY1	20
#define APCS_BAND_2G_LEGACY2	0
#define APCS_BAND_AUTO			"band=auto"
#define APCS_BAND_2G			"band=2g"
#define APCS_BAND_5G			"band=5g"
#define APCS_MAX_RETRY			10
#define APCS_DEFAULT_2G_CH		1
#define APCS_DEFAULT_5G_CH		149

static int
brcmf_privcmd_set_auto_channel(struct brcmf_if *ifp, const char *cmd_str,
			       char *command, int total_len)
{
	int channel = 0;
	int chosen = 0;
	int retry = 0;
	int ret = 0;
	uint spect = 0;
	u8 *reqbuf = NULL;
	const char *ch_str = NULL;
	u32 band = WLC_BAND_2G;
	u32 buf_size;
	int err = 0;

	if (cmd_str) {
		if (strncasecmp(cmd_str, APCS_BAND_AUTO,
				strlen(APCS_BAND_AUTO)) == 0) {
			band = WLC_BAND_AUTO;
		} else if (strncasecmp(cmd_str, APCS_BAND_5G,
				       strlen(APCS_BAND_5G)) == 0) {
			band = WLC_BAND_5G;
		} else if (strncasecmp(cmd_str, APCS_BAND_2G,
				       strlen(APCS_BAND_2G)) == 0) {
			band = WLC_BAND_2G;
		} else {
			/** For backward compatibility: Some platforms
			 *	used to issue argument 20 or 0
			 *  to enforce the 2G channel selection
			 */
			ch_str = cmd_str +
					 strlen(CMD_SET_HAPD_AUTO_CHANNEL) + 1;
			err = kstrtoint(ch_str, 10, &channel);
			if (err) {
				brcmf_err("strtoint failed with err:%d\n", err);
				return err;
			}

			if (channel == APCS_BAND_2G_LEGACY1 ||
			    channel == APCS_BAND_2G_LEGACY2) {
				band = WLC_BAND_2G;
			} else {
				brcmf_err("Invalid argument\n");
				return -EINVAL;
			}
		}
	} else {
		/* If no argument is provided, default to 2G */
		brcmf_err("No argument given default to 2.4G scan\n");
		band = WLC_BAND_2G;
	}

	err = brcmf_fil_cmd_int_get(ifp, BRCMF_C_GET_SPECT_MANAGEMENT, &spect);
	if (err) {
		brcmf_err("ACS: error getting the spect\n");
		goto done;
	}

	if (spect > 0) {
		/* If STA is connected, return is STA channel,
		 * else ACS can be issued,
		 * set spect to 0 and proceed with ACS
		 */
		channel = brcmf_cfg80211_get_sta_channel(ifp);
		if (channel) {
			channel = (channel <= CH_MAX_2G_CHANNEL) ?
						channel :
						APCS_DEFAULT_2G_CH;
			goto done2;
		}

		ret = brcmf_cfg80211_set_spect(ifp, 0);
		if (ret < 0) {
			brcmf_err("ACS: error while setting spect\n");
			goto done;
		}
	}

	reqbuf = kzalloc(BRCMF_DCMD_MEDLEN, GFP_KERNEL);
	if (!reqbuf)
		return -ENOMEM;

	if (band == WLC_BAND_AUTO) {
		reqbuf[0] = 0;
	} else if (band == WLC_BAND_5G) {
		ret = brcmf_cfg80211_get_chanspecs_5g(ifp, reqbuf,
						      BRCMF_DCMD_MEDLEN);
		if (ret < 0) {
			brcmf_err("ACS 5g chanspec retreival failed!\n");
			goto done;
		}
	} else if (band == WLC_BAND_2G) {
		/**
		 * If channel argument is not provided/ argument 20 is provided,
		 * Restrict channel to 2GHz, 20MHz BW, No SB
		 */
		ret = brcmf_cfg80211_get_chanspecs_2g(ifp, reqbuf,
						      BRCMF_DCMD_MEDLEN);
		if (ret < 0) {
			brcmf_err("ACS 2g chanspec retreival failed!\n");
			goto done;
		}
	} else {
		brcmf_err("ACS: No band chosen\n");
		goto done2;
	}

	/* Start auto channel selection scan. */
	buf_size = (band == WLC_BAND_AUTO) ? sizeof(int) : CHANSPEC_BUF_SIZE;
	err = brcmf_fil_cmd_data_set(ifp, BRCMF_C_START_CHANNEL_SEL,
				     reqbuf, buf_size);
	if (ret < 0) {
		brcmf_err("can't start auto channel scan, err = %d\n", err);
		channel = 0;
		goto done;
	}

	/* Wait for auto channel selection, max 3000 ms */
	if (band == WLC_BAND_2G || band == WLC_BAND_5G) {
		msleep(500);
	} else {
		/**
		 * Full channel scan at the minimum takes 1.2secs
		 * even with parallel scan. max wait time: 3500ms
		 */
		msleep(1000);
	}

	retry = APCS_MAX_RETRY;
	while (retry--) {
		err = brcmf_fil_cmd_int_get(ifp, BRCMF_C_GET_CHANNEL_SEL,
					    &chosen);
		if (err) {
			brcmf_err("get auto channel err %d\n", err);
			chosen = 0;
		}

		if (chosen) {
			int chosen_band;
			int apcs_band;

			channel = CHSPEC_CHANNEL(chosen);

			apcs_band = (band == WLC_BAND_AUTO) ?
				WLC_BAND_2G :
				band;

			chosen_band = (channel <= CH_MAX_2G_CHANNEL) ?
				WLC_BAND_2G :
				WLC_BAND_5G;

			if (apcs_band == chosen_band) {
				brcmf_err("selected channel = %d\n", channel);
				break;
			}
		}
		brcmf_info("%d tried, ret = %d, chosen = 0x%x\n",
			   (APCS_MAX_RETRY - retry), ret, chosen);
		msleep(250);
	}

done:
	if (retry == 0 || ret < 0) {
		/* On failure, fallback to a default channel */
		if (band == WLC_BAND_5G)
			channel = APCS_DEFAULT_5G_CH;
		else
			channel = APCS_DEFAULT_2G_CH;

		brcmf_err("ACS failed. Fall back to default channel (%d)\n",
			  channel);
	}
done2:
	if (spect > 0) {
		ret = brcmf_cfg80211_set_spect(ifp, spect);
		if (ret < 0)
			brcmf_err("ACS: error while setting spect\n");
	}

	kfree(reqbuf);

	if (channel) {
		snprintf(command, 4, "%d", channel);
		brcmf_info("command result is %s\n", command);
		return strlen(command);
	} else {
		return ret;
	}
}

/* force update cfg80211 to keep power save mode in sync.
 */
void brcmf_cfg80211_update_power_mode(struct brcmf_if *ifp,
				      struct net_device *dev)
{
	int err, pm = -1;

	err = brcmf_fil_cmd_int_get(ifp, BRCMF_C_GET_PM, &pm);
	if (err)
		brcmf_err("Line:%d error (%d)\n", __LINE__, err);
	else if (pm != -1 && dev->ieee80211_ptr)
		dev->ieee80211_ptr->ps = (pm == PM_OFF) ? false : true;
}

static int
brcmf_privcmd_set_miracast(struct brcmf_if *ifp, char *cmd,
			   int total_len)
{
	int mode, val;
	int err = 0;
	char str[3];

	/* Usage
	 * DRIVER MIRACAST <mode>
	 */
	brcmf_dbg(TRACE, "set miracast cmd %s\n", cmd);

	cmd = (cmd + strlen(CMD_MIRACAST));

	memset(&str, 0, 3);

	if (!*++cmd) {
		brcmf_err("missing arguments\n");
		return -1;
	}

	/* get the miracast mode */
	str[0] = *cmd++;
	str[1] = '\0';
	err = kstrtoint(str, 10, &mode);
	if (err) {
		brcmf_err("kstrtoint failed err %d\n", err);
		return err;
	}

	brcmf_dbg(INFO, "enter miracast mode:%d\n", mode);

	if (miracast_cur_mode == mode)
		return 0;

	/* TODO: currently android and suspend resume support is not present
	 * in FMAC. This shall be taken up once required support is present
	 */
	/* brmcf_privcmd_android_iolist_resume(dev, &miracast_resume_list); */

	miracast_cur_mode = MIRACAST_MODE_OFF;

	switch (mode) {
	case MIRACAST_MODE_SOURCE:
		/* setting mchan_algo to platform specific value */

		/* XXX check for station's beacon interval(BI)
		 * If BI is over 100ms, don't use mchan_algo
		 */

		err = brcmf_fil_cmd_int_get(ifp, BRCMF_C_GET_BCNPRD, &val);

		if (!err && val > 100) {
			brcmf_err("get beacon period %d err %d\n", val, err);
			return err;
		}

		err = brcmf_fil_iovar_int_set(ifp, "mchan_algo",
					      MIRACAST_MCHAN_ALGO);
		if (err) {
			brcmf_err("set mchan_algo err %d\n", err);
			return err;
		}

		/* setting mchan_bw to platform specific value */
		err = brcmf_fil_iovar_int_set(ifp, "mchan_bw",
					      MIRACAST_MCHAN_BW);

		if (err) {
			brcmf_err("set mchan_bw err %d\n", err);
			return err;
		}

		err = brcmf_fil_iovar_int_set(ifp, "ampdu_mpdu",
					      MIRACAST_AMPDU_SIZE);
		if (err) {
			brcmf_err("set ampdu_mpdu err %d\n", err);
			return err;
		}
		/* FALLTROUGH */
		/* Source mode shares most configurations with sink mode.
		 * Fall through here to avoid code duplication
		 */
	case MIRACAST_MODE_SINK:
		/* disable internal roaming */
		err = brcmf_fil_iovar_int_set(ifp, "roam_off",
					      1);
		if (err) {
			brcmf_err("set roam off err %d\n", err);
			return err;
		}

		/* turn off pm */
		err = brcmf_fil_cmd_int_get(ifp, BRCMF_C_GET_PM, &val);
		if (err) {
			brcmf_err("get PM mode err %d\n", err);
			return err;
		}

		if (val != PM_OFF) {
			val = PM_OFF;
			err = brcmf_fil_cmd_int_set(ifp, BRCMF_C_SET_PM, val);
			if (err) {
				brcmf_err("set PM mode err %d\n", err);
				return err;
			}
			brcmf_cfg80211_update_power_mode(ifp, ifp->ndev);
		}
		break;
	case MIRACAST_MODE_OFF:
	default:
		break;
	}
	miracast_cur_mode = mode;
	return 0;
}

int
brcmf_handle_private_cmd(struct brcmf_pub *drvr, struct net_device *ndev,
			 char *command, u32 cmd_len)
{
	int bytes_written = 0;
#if defined(CPTCFG_BRCMFMAC_ANDROID)
	struct brcmf_android *android = drvr->android;
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
	struct brcmf_android_wifi_priv_cmd priv_cmd;
	struct brcmf_if *ifp = netdev_priv(ndev);

	brcmf_dbg(TRACE, "enter  command: %s  cmd_len:%d\n", command, cmd_len);

#if defined(CPTCFG_BRCMFMAC_ANDROID)
	if (!android) {
		brcmf_err("not supported\n");
		return -EOPNOTSUPP;
	}

	if (!(android->wifi_on)) {
		brcmf_err("ignore cmd \"%s\" - iface is down\n", command);
		return 0;
	}
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */

	memset(&priv_cmd, 0, sizeof(struct brcmf_android_wifi_priv_cmd));
	priv_cmd.total_len = cmd_len;

	if (strncasecmp(command, CMD_SETSUSPENDMODE,
			strlen(CMD_SETSUSPENDMODE)) == 0) {
		bytes_written =
		    brcmf_android_set_suspendmode(ndev, command,
						  priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_COUNTRY,
			       strlen(CMD_COUNTRY)) == 0) {
		bytes_written =
		    brcmf_android_set_country(ndev, command,
					      priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_BTCOEXMODE,
		   strlen(CMD_BTCOEXMODE)) == 0) {
		bytes_written =
		    brcmf_android_set_btcoexmode(ndev, command,
						 priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_RXFILTER_START,
		   strlen(CMD_RXFILTER_START)) == 0) {
		bytes_written =
		    brcmf_pktfilter_enable(ndev, true);
	} else if (strncasecmp(command, CMD_RXFILTER_STOP,
		   strlen(CMD_RXFILTER_STOP)) == 0) {
		bytes_written =
		    brcmf_pktfilter_enable(ndev, false);
	} else if (strncasecmp(command, CMD_RXFILTER_ADD,
		   strlen(CMD_RXFILTER_ADD)) == 0) {
		bytes_written =
		    brcmf_android_pktfilter_add(ndev, command,
						priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_RXFILTER_REMOVE,
		   strlen(CMD_RXFILTER_REMOVE)) == 0) {
		bytes_written =
		    brcmf_android_pktfilter_remove(ndev, command,
						   priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_BTCOEXSCAN_START,
		   strlen(CMD_BTCOEXSCAN_START)) == 0) {
		//TODO: Handle BTCOEXSCAN_START command
	} else if (strncasecmp(command, CMD_BTCOEXSCAN_STOP,
		   strlen(CMD_BTCOEXSCAN_STOP)) == 0) {
		//TODO: Handle BTCOEXSCAN_STOP command
	} else if (strncasecmp(command, CMD_GETBAND,
		   strlen(CMD_GETBAND)) == 0) {
		bytes_written =
		    brcmf_privcmd_get_band(ndev, command, priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_SCAN_ACTIVE,
			   strlen(CMD_SCAN_ACTIVE)) == 0) {
		brcmf_privcmd_set_passive_scan(ifp, command,
					       priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_SCAN_PASSIVE,
			   strlen(CMD_SCAN_PASSIVE)) == 0) {
		brcmf_privcmd_set_passive_scan(ifp, command,
					       priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_RSSI,
			   strlen(CMD_RSSI)) == 0) {
		bytes_written = brcmf_privcmd_get_rssi(ifp, command,
						       priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_LINKSPEED,
			   strlen(CMD_LINKSPEED)) == 0) {
		bytes_written =
			brcmf_privcmd_get_link_speed(ifp, command,
						     priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_DATARATE,
			   strlen(CMD_DATARATE)) == 0) {
		bytes_written = brcmf_privcmd_get_data_rate(ifp, command,
							    priv_cmd.total_len);
	} else if (strncasecmp(command, CMD_ADDIE,
			   strlen(CMD_ADDIE)) == 0) {
		bytes_written = brcmf_privcmd_add_ie(ifp, command, cmd_len);
	} else if (strncasecmp(command, CMD_DELIE,
		   strlen(CMD_DELIE)) == 0) {
		bytes_written = brcmf_privcmd_del_ie(ifp, command, cmd_len);
	} else if (strncasecmp(command, CMD_SET_WPS_P2PIE,
		   strlen(CMD_SET_WPS_P2PIE)) == 0) {
		int skip = strlen(CMD_SET_WPS_P2PIE) + 3;

		bytes_written =
			brcmf_cfg80211_set_ap_wps_p2p_ie(ifp->vif,
							 command + skip,
							 (priv_cmd.total_len -
							 skip),
							 *(command + skip - 2)
							 - '0');
	} else if (strncasecmp(command, CMD_ASSOC_CLIENTS,
			   strlen(CMD_ASSOC_CLIENTS)) == 0) {
		bytes_written = brcmf_privcmd_get_assoclist(ifp, command,
							    cmd_len);
	} else if (strncasecmp(command, CMD_GET_CHANSPEC,
			   strlen(CMD_GET_CHANSPEC)) == 0) {
		bytes_written = brcmf_privcmd_get_chanspec(ifp, command,
							   cmd_len);
	} else if (strncasecmp(command, CMD_GET_MODE,
			   strlen(CMD_GET_MODE)) == 0) {
		bytes_written = brcmf_privcmd_80211_get_mode(ifp, command,
							     cmd_len);
	} else if (strncasecmp(command, CMD_SET_AMPDU_MPDU,
			   strlen(CMD_SET_AMPDU_MPDU)) == 0) {
		bytes_written = brcmf_privcmd_ampdu_mpdu(ifp, command,
							 cmd_len);
	} else if (strncasecmp(command, CMD_SETBAND,
			       strlen(CMD_SETBAND)) == 0) {
		u32 band;

		band = *(command + strlen(CMD_SETBAND) + 1) - '0';
		bytes_written = brcmf_privcmd_set_band(ifp, band);
	} else if (strncasecmp(command, CMD_HIDDEN_SSID,
			   strlen(CMD_HIDDEN_SSID)) == 0) {
		bytes_written = brcmf_privcmd_hidden_ssid(ifp, command,
							  cmd_len);
	} else if (strncasecmp(command, CMD_SET_CSA,
			   strlen(CMD_SET_CSA)) == 0) {
		bytes_written = brcmf_privcmd_set_csa(ifp, command, cmd_len);
	} else if (strncasecmp(command, CMD_COUNTRYREV_GET,
			   strlen(CMD_COUNTRYREV_GET)) == 0) {
		bytes_written =
			brcmf_privcmd_get_country_rev(ifp, command, cmd_len);
	} else if (strncasecmp(command, CMD_COUNTRYREV_SET,
			   strlen(CMD_COUNTRYREV_SET)) == 0) {
		bytes_written =
			brcmf_privcmd_set_country_rev(ifp, command, cmd_len);
	} else if (strncasecmp(command, CMD_HAPD_MAC_FILTER,
			 strlen(CMD_HAPD_MAC_FILTER)) == 0) {
		bytes_written =
			brcmf_privcmd_set_mac_address_filter(ifp, command,
							     cmd_len);
	} else if (strncasecmp(command, CMD_MIRACAST,
			   strlen(CMD_MIRACAST)) == 0) {
		bytes_written =
			brcmf_privcmd_set_miracast(ifp, command, cmd_len);
	} else if (strncasecmp(command, CMD_SET_HAPD_AUTO_CHANNEL,
			   strlen(CMD_SET_HAPD_AUTO_CHANNEL)) == 0) {
		int skip = strlen(CMD_SET_HAPD_AUTO_CHANNEL) + 1;

		bytes_written = brcmf_privcmd_set_auto_channel(
			ifp, (const char *)command + skip, command, cmd_len);
	} else {
		brcmf_err("unknown PRIVATE command %s - ignored\n", command);
		snprintf(command, 5, "FAIL");
		bytes_written = strlen("FAIL");
	}

	return bytes_written;
}

#if defined(CPTCFG_BRCMFMAC_ANDROID)
int brcmf_android_attach(struct brcmf_pub *drvr)
{
	struct brcmf_android *android = NULL;

	if (brcmf_android_is_attached(drvr))
		return 0;

	brcmf_dbg(TRACE, "enter\n");

	android = kzalloc(sizeof(*android), GFP_KERNEL);
	if (!android)
		return -ENOMEM;

	drvr->android = android;
	android->drvr = drvr;
	android->wifi_on = true;
	android->wifi_reset = false;
	android->init_done = false;
	android->wakelock_counter = 0;
	android->wakelock_waive = false;
	android->wakelock_waive_counter = 0;
	android->country[0] = 0;
	android->country[1] = 0;
	android->country[2] = 0;
	wake_lock_init(&android->wakelock, WAKE_LOCK_SUSPEND,
		       "brcm_wlan_wake");
	wake_lock_init(&android->rx_wakelock, WAKE_LOCK_SUSPEND,
		       "brcm_wlan_rxwake");
	spin_lock_init(&android->wakelock_spinlock);

	return 0;
}

int brcmf_android_detach(struct brcmf_pub *drvr)
{
	struct brcmf_android *android = drvr->android;

	brcmf_dbg(TRACE, "enter\n");

	wake_lock_destroy(&android->wakelock);
	wake_lock_destroy(&android->rx_wakelock);
	kfree(drvr->android);
	drvr->android = NULL;

	return 0;
}

bool brcmf_android_is_attached(struct brcmf_pub *drvr)
{
	return !!(drvr->android);
}

bool brcmf_android_is_built_in(struct brcmf_pub *drvr)
{
#ifdef BRCMFMAC_ANDROID_BUILT_IN
	return brcmf_android_is_attached(drvr);
#else
	return false;
#endif
}

bool brcmf_android_wifi_is_on(struct brcmf_pub *drvr)
{
	if (!brcmf_android_is_attached(drvr))
		return false;

	return drvr->android->wifi_on;
}

bool brcmf_android_in_reset(struct brcmf_pub *drvr)
{
	if (!brcmf_android_is_attached(drvr))
		return false;

	return drvr->android->wifi_reset;
}

void brcmf_android_set_reset(struct brcmf_pub *drvr, bool is_reset)
{
	brcmf_dbg(TRACE, "enter\n");

	if (!brcmf_android_is_attached(drvr))
		return;

	drvr->android->wifi_reset = is_reset;
}

int brcmf_android_wake_unlock_timeout(struct brcmf_pub *drvr)
{
	struct brcmf_android *android = drvr->android;
	unsigned long flags;
	unsigned int ret = 0;

	brcmf_dbg(TRACE, "enter\n");

	if (!brcmf_android_is_attached(drvr))
		return -EOPNOTSUPP;

	spin_lock_irqsave(&android->wakelock_spinlock, flags);
	wake_lock_timeout(&android->rx_wakelock, msecs_to_jiffies(200));
	spin_unlock_irqrestore(&android->wakelock_spinlock, flags);

	ret = brcmf_android_wake_unlock(drvr);

	return ret;
}

int brcmf_android_wake_lock(struct brcmf_pub *drvr)
{
	struct brcmf_android *android = drvr->android;
	unsigned long flags;
	unsigned int ret = 0;

	brcmf_dbg(TRACE, "enter\n");

	if (!brcmf_android_is_attached(drvr))
		return -EOPNOTSUPP;

	if (brcmf_android_wake_lock_is_waive(drvr)) {
		spin_lock_irqsave(&android->wakelock_spinlock, flags);
		android->wakelock_waive_counter++;
		spin_unlock_irqrestore(&android->wakelock_spinlock, flags);
	} else {
		spin_lock_irqsave(&android->wakelock_spinlock, flags);

		if (android->wakelock_counter == 0)
			wake_lock(&android->wakelock);

		android->wakelock_counter++;
		ret = android->wakelock_counter;

		spin_unlock_irqrestore(&android->wakelock_spinlock, flags);
	}

	return ret;
}

int brcmf_android_wake_unlock(struct brcmf_pub *drvr)
{
	struct brcmf_android *android = drvr->android;
	unsigned long flags;
	unsigned int ret = 0;

	brcmf_dbg(TRACE, "enter\n");

	if (!brcmf_android_is_attached(drvr))
		return -EOPNOTSUPP;

	spin_lock_irqsave(&android->wakelock_spinlock, flags);

	if (android->wakelock_waive_counter > 0) {
		android->wakelock_waive_counter--;
	} else {
		if (android->wakelock_counter > 0)
			android->wakelock_counter--;

		if (android->wakelock_counter == 0)
			wake_unlock(&android->wakelock);

		ret = android->wakelock_counter;
	}

	spin_unlock_irqrestore(&android->wakelock_spinlock, flags);

	return ret;
}

void brcmf_android_wake_lock_waive(struct brcmf_pub *drvr, bool is_waive)
{
	struct brcmf_android *android = drvr->android;
	unsigned long flags;

	brcmf_dbg(TRACE, "enter\n");

	if (!brcmf_android_is_attached(drvr))
		return;

	spin_lock_irqsave(&android->wakelock_spinlock, flags);
	android->wakelock_waive = is_waive;
	spin_unlock_irqrestore(&android->wakelock_spinlock, flags);
}

bool brcmf_android_wake_lock_is_waive(struct brcmf_pub *drvr)
{
	struct brcmf_android *android = drvr->android;
	unsigned long flags;

	bool is_waive = false;

	if (!brcmf_android_is_attached(drvr))
		return false;

	spin_lock_irqsave(&android->wakelock_spinlock, flags);
	is_waive = android->wakelock_waive;
	spin_unlock_irqrestore(&android->wakelock_spinlock, flags);

	return is_waive;
}

int brcmf_android_init(struct brcmf_pub *drvr)
{
	int err = 0;
#ifdef CPTCFG_BRCM_INSMOD_NO_FW
	err = brcmf_cfg80211_register_if(drvr);
	if (err) {
		brcmf_err("init failed, err=%d\n", err);
		return err;
	}

	brcmf_android_set_reset(drvr, true);
	brcmf_android_wifi_off(drvr, NULL);
	g_drvr = drvr;
#endif
	return err;
}

int brcmf_android_reset_country(struct brcmf_pub *drvr)
{
	struct brcmf_android *android = drvr->android;
	char *country = NULL;
	int ret = 0;

	if (!brcmf_android_is_attached(drvr))
		return false;

	country = android->country;

	if (country[0] && country[1])
		ret = brcmf_set_country(drvr->iflist[0]->ndev, country);

	return ret;
}

int brcmf_android_set_extra_wiphy(struct wiphy *wiphy, struct brcmf_if *ifp)
{
	struct cfg80211_wowlan *brcmf_wowlan_config = NULL;

	brcmf_dbg(TRACE, "enter\n");

	brcmf_set_vndr_cmd(wiphy);

	if (!wiphy->wowlan_config) {
		brcmf_wowlan_config = kzalloc(sizeof(*brcmf_wowlan_config),
					      GFP_KERNEL);
		if (brcmf_wowlan_config) {
			brcmf_wowlan_config->disconnect = true;
			brcmf_wowlan_config->gtk_rekey_failure = true;
			brcmf_wowlan_config->eap_identity_req = true;
			brcmf_wowlan_config->four_way_handshake = true;
			brcmf_wowlan_config->patterns = NULL;
			brcmf_wowlan_config->n_patterns = 0;
			brcmf_wowlan_config->tcp = NULL;
		} else {
			brcmf_err("Can not allocate memory for brcmf_wowlan_config\n");
			return -ENOMEM;
		}
		wiphy->wowlan_config = brcmf_wowlan_config;
	}

	return 0;
}
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
