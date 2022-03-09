/*
 * Copyright (c) 2014 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/vmalloc.h>
#include <net/cfg80211.h>
#include <net/netlink.h>

#include <brcmu_wifi.h>
#include "fwil_types.h"
#include "core.h"
#include "p2p.h"
#include "debug.h"
#include "cfg80211.h"
#include "vendor.h"
#include "fwil.h"
#include "feature.h"
#if defined(CPTCFG_BRCMFMAC_ANDROID)
#include <linux/wakelock.h>
#include "android.h"
#include "bus.h"
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */

#if defined(CPTCFG_BRCMFMAC_ANDROID)
#define UNKNOWN_VER_STR		"Unknown version"
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */

static int brcmf_cfg80211_vndr_cmds_dcmd_handler(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data, int len)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	const struct brcmf_vndr_dcmd_hdr *cmdhdr = data;
	struct sk_buff *reply;
	int ret, payload, ret_len;
	void *dcmd_buf = NULL, *wr_pointer;
	u16 msglen, maxmsglen = PAGE_SIZE - 0x100;

	if (len < sizeof(*cmdhdr)) {
		brcmf_err("vendor command too short: %d\n", len);
		return -EINVAL;
	}

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	brcmf_dbg(TRACE, "ifidx=%d, cmd=%d\n", ifp->ifidx, cmdhdr->cmd);

	if (cmdhdr->offset > len) {
		brcmf_err("bad buffer offset %d > %d\n", cmdhdr->offset, len);
		return -EINVAL;
	}

#if defined(CPTCFG_BRCMFMAC_ANDROID)
	brcmf_android_wake_lock(ifp->drvr);
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */

	len -= cmdhdr->offset;
	ret_len = cmdhdr->len;
	if (ret_len > 0 || len > 0) {
		if (len > BRCMF_DCMD_MAXLEN) {
			brcmf_err("oversize input buffer %d\n", len);
			len = BRCMF_DCMD_MAXLEN;
		}
		if (ret_len > BRCMF_DCMD_MAXLEN) {
			brcmf_err("oversize return buffer %d\n", ret_len);
			ret_len = BRCMF_DCMD_MAXLEN;
		}
		payload = max(ret_len, len) + 1;
		dcmd_buf = vzalloc(payload);
		if (!dcmd_buf) {
#if defined(CPTCFG_BRCMFMAC_ANDROID)
			brcmf_android_wake_unlock(ifp->drvr);
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
			return -ENOMEM;
		}

		memcpy(dcmd_buf, (void *)cmdhdr + cmdhdr->offset, len);
		*(char *)(dcmd_buf + len)  = '\0';
	}

	if (cmdhdr->cmd == BRCMF_C_SET_AP) {
		if (*(int *)(dcmd_buf) == 1) {
			ifp->vif->wdev.iftype = NL80211_IFTYPE_AP;
			brcmf_net_setcarrier(ifp, true);
		} else {
			ifp->vif->wdev.iftype = NL80211_IFTYPE_STATION;
		}
	}

	if (cmdhdr->set)
		ret = brcmf_fil_cmd_data_set(ifp, cmdhdr->cmd, dcmd_buf,
					     ret_len);
	else
		ret = brcmf_fil_cmd_data_get(ifp, cmdhdr->cmd, dcmd_buf,
					     ret_len);

	if (ret != 0) {
		brcmf_dbg(INFO, "error(%d), return -EPERM\n", ret);
		ret = -EPERM;
		goto exit;
	}

	wr_pointer = dcmd_buf;
	while (ret_len > 0) {
		msglen = ret_len > maxmsglen ? maxmsglen : ret_len;
		ret_len -= msglen;
		payload = msglen + sizeof(msglen);
		reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
		if (!reply) {
			ret = -ENOMEM;
			break;
		}

		if (nla_put(reply, BRCMF_NLATTR_DATA, msglen, wr_pointer) ||
		    nla_put_u16(reply, BRCMF_NLATTR_LEN, msglen)) {
			kfree_skb(reply);
			ret = -ENOBUFS;
			break;
		}

		ret = cfg80211_vendor_cmd_reply(reply);
		if (ret)
			break;

		wr_pointer += msglen;
	}

exit:
	vfree(dcmd_buf);
#if defined(CPTCFG_BRCMFMAC_ANDROID)
	brcmf_android_wake_unlock(ifp->drvr);
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
	return ret;
}

#if defined(CPTCFG_BRCMFMAC_ANDROID)
static int
brcmf_cfg80211_gscan_get_channel_list_handler(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int len)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct sk_buff *reply;
	int ret, gscan_band, i;
	struct ieee80211_supported_band *band_2g, *band_5g;
	uint *channels;
	uint num_channels = 0;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	brcmf_android_wake_lock(ifp->drvr);

	brcmf_dbg(TRACE, "ifidx=%d, enter\n", ifp->ifidx);

	if (nla_type(data) == GSCAN_ATTRIBUTE_BAND) {
		gscan_band = nla_get_u32(data);
		if ((gscan_band & GSCAN_BAND_MASK) == 0) {
			ret = -EINVAL;
			goto exit;
		}
	} else {
		ret =  -EINVAL;
		goto exit;
	}

	band_2g = wiphy->bands[NL80211_BAND_2GHZ];
	band_5g = wiphy->bands[NL80211_BAND_5GHZ];
	channels = vzalloc((band_2g->n_channels + band_5g->n_channels) *
			   sizeof(uint));
	if (!channels) {
		ret = -ENOMEM;
		goto exit;
	}

	if (gscan_band & GSCAN_BG_BAND_MASK) {
		for (i = 0; i < band_2g->n_channels; i++) {
			if (band_2g->channels[i].flags &
			    IEEE80211_CHAN_DISABLED)
				continue;
			if (!(gscan_band & GSCAN_DFS_MASK) &&
			    (band_2g->channels[i].flags &
			     (IEEE80211_CHAN_RADAR | IEEE80211_CHAN_NO_IR)))
				continue;

			channels[num_channels] =
			    band_2g->channels[i].center_freq;
			num_channels++;
		}
	}
	if (gscan_band & GSCAN_A_BAND_MASK) {
		for (i = 0; i < band_5g->n_channels; i++) {
			if (band_5g->channels[i].flags &
			    IEEE80211_CHAN_DISABLED)
				continue;
			if (!(gscan_band & GSCAN_DFS_MASK) &&
			    (band_5g->channels[i].flags &
			     (IEEE80211_CHAN_RADAR | IEEE80211_CHAN_NO_IR)))
				continue;

			channels[num_channels] =
			    band_5g->channels[i].center_freq;
			num_channels++;
		}
	}

	reply =
	    cfg80211_vendor_cmd_alloc_reply_skb(wiphy, ((num_channels + 1) *
							sizeof(uint)));
	if (!reply) {
		ret = -ENOMEM;
		goto skb_alloc_failed;
	}

	nla_put_u32(reply, GSCAN_ATTRIBUTE_NUM_CHANNELS, num_channels);
	nla_put(reply, GSCAN_ATTRIBUTE_CHANNEL_LIST,
		num_channels * sizeof(uint), channels);
	ret = cfg80211_vendor_cmd_reply(reply);

skb_alloc_failed:
	vfree(channels);

exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_get_feature_set_handler(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int len)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct sk_buff *reply;
	int ret;
	int feature_set = 0;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	brcmf_android_wake_lock(ifp->drvr);

	brcmf_dbg(TRACE, "ifidx=%d, enter\n", ifp->ifidx);

	if (brcmf_feat_is_enabled(ifp, BRCMF_FEAT_STA))
		feature_set |= WIFI_FEATURE_INFRA;
	if (brcmf_feat_is_enabled(ifp, BRCMF_FEAT_DUALBAND))
		feature_set |= WIFI_FEATURE_INFRA_5G;
	if (brcmf_feat_is_enabled(ifp, BRCMF_FEAT_P2P))
		feature_set |= WIFI_FEATURE_P2P;
	if (brcmf_feat_is_enabled(ifp, BRCMF_FEAT_TDLS))
		feature_set |= WIFI_FEATURE_TDLS;
	if (brcmf_feat_is_enabled(ifp, BRCMF_FEAT_VSDB))
		feature_set |= WIFI_FEATURE_TDLS_OFFCHANNEL;
	if (wdev->iftype == NL80211_IFTYPE_AP ||
	    wdev->iftype == NL80211_IFTYPE_P2P_GO)
		feature_set |= WIFI_FEATURE_SOFT_AP;
	if (brcmf_feat_is_enabled(ifp, BRCMF_FEAT_NAN)) {
		feature_set |= WIFI_FEATURE_NAN;
		if (brcmf_feat_is_enabled(ifp, BRCMF_FEAT_D2DRTT))
			feature_set |= WIFI_FEATURE_D2D_RTT;

	}
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(int));
	if (!reply) {
		ret = -ENOMEM;
		goto exit;
	}

	nla_put_nohdr(reply, sizeof(int), &feature_set);
	ret = cfg80211_vendor_cmd_reply(reply);

exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_set_country_handler(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int len)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct net_device *ndev;
	int ret;
	char *country_code;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;
	ndev = ifp->ndev;

	brcmf_android_wake_lock(ifp->drvr);

	brcmf_dbg(TRACE, "ifidx=%d, enter\n", ifp->ifidx);

	if (nla_type(data) == ANDR_WIFI_ATTRIBUTE_COUNTRY) {
		country_code = nla_data(data);
		brcmf_err("country=%s\n", country_code);
		if (strlen(country_code) != 2)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	ret = brcmf_set_country(ndev, country_code);
	if (ret)
		brcmf_err("set country code %s failed, ret=%d\n",
			  country_code, ret);

	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_set_rand_mac_oui(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct brcmf_pfn_macaddr_cfg pfn_cfg;
	int ret = 0;
	int type;
	char *oui;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	brcmf_android_wake_lock(ifp->drvr);

	brcmf_dbg(TRACE, "ifidx=%d, enter\n", ifp->ifidx);

	type = nla_type(data);
	if (type == ANDR_WIFI_ATTRIBUTE_RANDOM_MAC_OUI) {
		if (nla_len(data) != DOT11_OUI_LEN) {
			brcmf_err("nla_len not matched.\n");
			ret = -EINVAL;
			goto exit;
		}

		oui = nla_data(data);
		memcpy(&pfn_cfg.macaddr, oui, DOT11_OUI_LEN);
		/* Clear multi bit */
		pfn_cfg.macaddr.octet[0] &= 0xFE;
		/* Set locally administered */
		pfn_cfg.macaddr.octet[0] |= 0x02;

		/* Set version and flags */
		pfn_cfg.ver = BRCMF_PFN_MACADDR_CFG_VER;
		pfn_cfg.flags = (BRCMF_PFN_MAC_OUI_ONLY_MASK |
				 BRCMF_PFN_SET_MAC_UNASSOC_MASK);

		ret = brcmf_fil_iovar_data_set(ifp, "pfn_macaddr",
					       &pfn_cfg, sizeof(pfn_cfg));
		if (ret) {
			brcmf_err("pfn_macaddr failed, err=%d\n", ret);
			goto exit;
		}
		brcmf_dbg(INFO, "configured random mac: mac=%pM\n",
				  pfn_cfg.macaddr.octet);
	} else {
		ret = -EINVAL;
	}

exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_config_nd_offload(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void  *data, int len)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	int ret = 0, rem, type;
	const struct nlattr *iter;
	bool enable = false;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	if (!vif || !vif->ifp)
		return -EFAULT;
	ifp = vif->ifp;

	brcmf_android_wake_lock(ifp->drvr);

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case ANDR_WIFI_ATTRIBUTE_ND_OFFLOAD_VALUE:
			enable = nla_get_u8(iter);
			break;

		default:
			brcmf_err("Unknown type: %d\n", type);
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = brcmf_configure_arp_nd_offload(ifp, enable);
	if (ret) {
		brcmf_err("configure ARP ND offload failed, ret=%d\n", ret);
		ret = -EIO;
		goto exit;
	}
exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_dbg_start_logging(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void  *data, int len)
{
	brcmf_dbg(TRACE, "return UNSUPPORTED\n");
	return -ENOTSUPP;
}

static int
brcmf_cfg80211_andr_dbg_reset_logging(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void  *data, int len)
{
	brcmf_dbg(TRACE, "return UNSUPPORTED\n");
	return -ENOTSUPP;
}

static int
brcmf_cfg80211_andr_dbg_trigger_mem_dump(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void  *data, int len)
{
	struct brcmf_android *android;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct brcmf_bus *bus;
	struct sk_buff *reply;
	void *dump;
	size_t ramsize;
	int err;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;
	if (!ifp || !ifp->drvr || !ifp->drvr->bus_if || !ifp->drvr->android)
		return -ENODEV;

	brcmf_android_wake_lock(ifp->drvr);

	bus = ifp->drvr->bus_if;
	android = ifp->drvr->android;

	if (android->fw_mem_dump) {
		vfree(android->fw_mem_dump);
		android->fw_mem_dump = NULL;
		android->fw_mem_dump_len = 0;
	}

	ramsize = brcmf_bus_get_ramsize(bus);
	if (!ramsize) {
		err = -ENOTSUPP;
		goto exit;
	}

	dump = vzalloc(ramsize);
	if (!dump) {
		err = -ENOMEM;
		goto exit;
	}

	err = brcmf_bus_get_memdump(bus, dump, ramsize);
	if (err) {
		err = -EIO;
		goto free_mem;
	}

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32));
	if (!reply) {
		err = -ENOMEM;
		goto free_mem;
	}

	nla_put_u32(reply, DEBUG_ATTRIBUTE_FW_DUMP_LEN, (u32)ramsize);

	err = cfg80211_vendor_cmd_reply(reply);
	if (err)
		goto free_mem;

	android->fw_mem_dump = dump;
	android->fw_mem_dump_len = ramsize;

free_mem:
	if (err)
		vfree(dump);

exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return err;
}

static int
brcmf_cfg80211_andr_dbg_get_mem_dump(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	struct brcmf_android *android;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct brcmf_bus *bus;
	struct sk_buff *reply;
	const struct nlattr *iter;
	int type;
	int rem;
	int err = 0;
	void *user_buf = NULL;
	u32 user_buf_len = 0;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;
	if (!ifp || !ifp->drvr || !ifp->drvr->bus_if || !ifp->drvr->android)
		return -ENODEV;

	brcmf_android_wake_lock(ifp->drvr);

	bus = ifp->drvr->bus_if;
	android = ifp->drvr->android;

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case DEBUG_ATTRIBUTE_FW_DUMP_LEN:
			user_buf_len = nla_get_u32(iter);
			if (user_buf_len > android->fw_mem_dump_len)
				user_buf_len = android->fw_mem_dump_len;
			break;
		case DEBUG_ATTRIBUTE_FW_DUMP_DATA:
			user_buf = (void *)(unsigned long)nla_get_u64(iter);
			break;
		default:
			brcmf_err("Unknown type: %d\n", type);
			err = -EINVAL;
			goto exit;
		}
	}
	if (user_buf_len == 0 || !user_buf) {
		err = -EINVAL;
		goto exit;
	}

	if (android->fw_mem_dump_len == 0 || !android->fw_mem_dump) {
		err = -ENODATA;
		goto exit;
	}

	err = copy_to_user(user_buf, android->fw_mem_dump, user_buf_len);
	if (err)
		goto exit;

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(int));
	if (!reply) {
		err = -ENOMEM;
		goto exit;
	}
	nla_put(reply, DEBUG_ATTRIBUTE_FW_DUMP_DATA, sizeof(int), &err);
	err = cfg80211_vendor_cmd_reply(reply);
	if (err)
		goto exit;

exit:
	if (android->fw_mem_dump) {
		vfree(android->fw_mem_dump);
		android->fw_mem_dump = NULL;
		android->fw_mem_dump_len = 0;
	}
	brcmf_android_wake_unlock(ifp->drvr);

	return err;
}

static int
brcmf_cfg80211_andr_dbg_get_version(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int len)
{
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct sk_buff *reply;
	int ret = 0, rem, type;
	char buf[512];
	bool fmac_ver = false;
	const struct nlattr *iter;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	if (!vif || !vif->ifp)
		return -EFAULT;
	ifp = vif->ifp;

	brcmf_android_wake_lock(ifp->drvr);

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case DEBUG_ATTRIBUTE_GET_DRIVER:
			fmac_ver = true;
			break;

		case DEBUG_ATTRIBUTE_GET_FW:
			fmac_ver = false;
			break;

		default:
			brcmf_err("Unknown type: %d\n", type);
			ret = -EINVAL;
			goto exit;
		}
	}

	memset(buf, 0, sizeof(buf));
	if (fmac_ver) {
#if defined(CPTCFG_KERNEL_VERSION)
		strncpy(buf, CPTCFG_KERNEL_VERSION,
			sizeof(CPTCFG_KERNEL_VERSION));
#else
		strncpy(buf, UNKNOWN_VER_STR, sizeof(UNKNOWN_VER_STR));
#endif /* defined(CPTCFG_KERNEL_VERSION) */
	} else {
		ret = brcmf_fil_iovar_data_get(ifp, "ver", buf, sizeof(buf));
		if (ret) {
			brcmf_err("get ver error, ret = %d\n", ret);
			ret = -EIO;
			goto exit;
		}
	}
	brcmf_dbg(INFO, "Version : %s\n", buf);
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, strlen(buf));
	if (!reply) {
		ret = -ENOMEM;
		goto exit;
	}
	nla_put_nohdr(reply, strlen(buf), buf);
	ret = cfg80211_vendor_cmd_reply(reply);

exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_dbg_get_ring_status(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void  *data, int len)
{
	brcmf_dbg(TRACE, "return UNSUPPORTED\n");
	return -ENOTSUPP;
}

static int
brcmf_cfg80211_andr_dbg_get_ring_data(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void  *data, int len)
{
	brcmf_dbg(TRACE, "return UNSUPPORTED\n");
	return -ENOTSUPP;
}

static int
brcmf_cfg80211_andr_dbg_get_feature(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void  *data, int len)
{
	int ret = 0;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct sk_buff *reply;
	u32 supported_features = 0;
	char caps[512];

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	brcmf_android_wake_lock(ifp->drvr);

	/* Add firmware memory dump feature in default */
	supported_features |= DBG_MEMORY_DUMP_SUPPORTED;

	/* Query firmware features */
	ret = brcmf_fil_iovar_data_get(ifp, "cap", caps, sizeof(caps));
	if (!ret) {
		if (strnstr(caps, "logtrace", sizeof(caps))) {
			supported_features |= DBG_CONNECT_EVENT_SUPPORTED;
			supported_features |= DBG_VERBOSE_LOG_SUPPORTED;
		}
	} else {
		brcmf_err("get capa error, ret = %d\n", ret);
	}

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32));
	if (!reply) {
		ret = -ENOMEM;
		goto exit;
	}

	nla_put_nohdr(reply, sizeof(u32), &supported_features);
	ret = cfg80211_vendor_cmd_reply(reply);

exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_get_wake_reason_stats(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int len)
{
	brcmf_dbg(TRACE, "return UNSUPPORTED\n");
	return -ENOTSUPP;
}

static int
brcmf_cfg80211_andr_apf_get_capabilities(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int len)
{
	int ret = 0, ver;
	struct brcmf_cfg80211_vif *vif;
	struct brcmf_if *ifp;
	struct sk_buff *reply;

	vif = container_of(wdev, struct brcmf_cfg80211_vif, wdev);
	ifp = vif->ifp;

	brcmf_android_wake_lock(ifp->drvr);
	/**
	 * Notify Android framework that APF is not supported by setting
	 * version as zero.
	 */
	ver = 0;

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(int));
	if (!reply) {
		ret = -ENOMEM;
		goto exit;
	}
	nla_put_u32(reply, APF_ATTRIBUTE_VERSION, ver);
	ret = cfg80211_vendor_cmd_reply(reply);

exit:
	brcmf_android_wake_unlock(ifp->drvr);

	return ret;
}

static int
brcmf_cfg80211_andr_apf_set_filter(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int len)
{
	/* TODO:
	 * Add here to brcmf_pktfilter_add_remove function
	 */
	brcmf_dbg(TRACE, "return UNSUPPORTED\n");
	return -ENOTSUPP;
}

#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */

s32
brcmf_wiphy_phy_temp_evt_handler(struct brcmf_if *ifp,
				 const struct brcmf_event_msg *e, void *data)

{
	struct brcmf_cfg80211_info *cfg = ifp->drvr->config;
	struct wiphy *wiphy = cfg_to_wiphy(cfg);
	struct sk_buff *skb;
	struct nlattr *phy_temp_data;
	u32 version, temp, tempdelta;
	struct brcmf_phy_temp_evt *phy_temp_evt;

	phy_temp_evt = (struct brcmf_phy_temp_evt *)data;

	version = le32_to_cpu(phy_temp_evt->version);
	temp = le32_to_cpu(phy_temp_evt->temp);
	tempdelta = le32_to_cpu(phy_temp_evt->tempdelta);

	skb = cfg80211_vendor_event_alloc(wiphy, NULL,
					  sizeof(*phy_temp_evt),
					  BRCMF_VNDR_EVTS_PHY_TEMP,
					  GFP_KERNEL);

	if (!skb) {
		brcmf_dbg(EVENT, "NO MEM: can't allocate skb for vendor PHY_TEMP_EVENT\n");
		return -ENOMEM;
	}

	phy_temp_data = nla_nest_start(skb, NL80211_ATTR_VENDOR_EVENTS);
	if (!phy_temp_data) {
		nla_nest_cancel(skb, phy_temp_data);
		kfree_skb(skb);
		brcmf_dbg(EVENT, "skb could not nest vendor attributes\n");
		return -EMSGSIZE;
	}

	if (nla_put_u32(skb, BRCMF_NLATTR_VERS, version) ||
	    nla_put_u32(skb, BRCMF_NLATTR_PHY_TEMP, temp) ||
	    nla_put_u32(skb, BRCMF_NLATTR_PHY_TEMPDELTA, tempdelta)) {
		kfree_skb(skb);
		brcmf_dbg(EVENT, "NO ROOM in skb for vendor PHY_TEMP_EVENT\n");
		return -EMSGSIZE;
	}

	nla_nest_end(skb, phy_temp_data);

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;
}

s32
brcmf_broadcast_sta_update(struct wiphy *wiphy, struct net_device *ndev,
			   const u8 *mac, struct nl80211_sta_flag_update flags)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_event_alloc(wiphy, ndev->ieee80211_ptr, 0,
					  BRCMF_VNDR_EVTS_STA_UPDATE,
					  GFP_KERNEL);
	if (!skb) {
		brcmf_dbg(EVENT, "NO MEM: can't allocate skb for vendor STA_UPDATE\n");
		return -ENOMEM;
	}

	if (nla_put(skb, BRCMF_NLATTR_STA_MAC, ETH_ALEN, mac) ||
	    nla_put(skb, BRCMF_NLATTR_STA_FLAGS, sizeof(flags), &flags)) {
		kfree_skb(skb);
		brcmf_dbg(EVENT, "NO ROOM in skb for vendor STA_UPDATE\n");
		return -EMSGSIZE;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return 0;
}

const struct wiphy_vendor_command brcmf_vendor_cmds[] = {
	{
		{
			.vendor_id = BROADCOM_OUI,
			.subcmd = BRCMF_VNDR_CMDS_DCMD
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_vndr_cmds_dcmd_handler
	},
#if defined(CPTCFG_BRCMFMAC_ANDROID)
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = GSCAN_SUBCMD_GET_CHANNEL_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_gscan_get_channel_list_handler
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = ANDR_WIFI_SET_COUNTRY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_set_country_handler
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = ANDR_WIFI_SUBCMD_GET_FEATURE_SET
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_get_feature_set_handler
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = ANDR_WIFI_RANDOM_MAC_OUI
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_set_rand_mac_oui
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = ANDR_WIFI_SUBCMD_CONFIG_ND_OFFLOAD
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_config_nd_offload
	},

	/* DEBUG ABILITY */
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_START_LOGGING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_start_logging
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_TRIGGER_MEM_DUMP
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_trigger_mem_dump
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_GET_MEM_DUMP
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_get_mem_dump
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_GET_VER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_get_version
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_GET_RING_STATUS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_get_ring_status
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_GET_RING_DATA
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_get_ring_data
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_GET_FEATURE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_get_feature
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_RESET_LOGGING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_dbg_reset_logging
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = DEBUG_GET_WAKE_REASON_STATS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_get_wake_reason_stats
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = APF_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_apf_get_capabilities
	},
	{
		{
			.vendor_id = GOOGLE_OUI,
			.subcmd = APF_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = brcmf_cfg80211_andr_apf_set_filter
	}

#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
};

const struct nl80211_vendor_cmd_info brcmf_vendor_events[] = {
	{
		.vendor_id = BROADCOM_OUI,
		.subcmd = BRCMF_VNDR_EVTS_PHY_TEMP,
	},
	{
		.vendor_id = BROADCOM_OUI,
		.subcmd = BRCMF_VNDR_EVTS_STA_UPDATE,
	},
};

#if defined(CPTCFG_BRCMFMAC_ANDROID)
void brcmf_set_vndr_cmd(struct wiphy *wiphy)
{
	wiphy->vendor_commands = brcmf_vendor_cmds;
	wiphy->n_vendor_commands = ARRAY_SIZE(brcmf_vendor_cmds);
}
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
