// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <net/cfg80211.h>
#include <wl_cfg80211.h>
#include <wl_cfgvendor_meta.h>
#include <net/netlink.h>
#include <bcmutils.h>
#include <wldev_common.h>

static int wl_cfgvendor_meta_set_txchain(struct net_device *ndev,
					const struct nlattr *attr);
static int wl_cfgvendor_meta_set_elna_bypass(struct net_device *ndev,
					const struct nlattr *attr);

/* META_SUBCMD_SET_WIFI_CONFIG policy */
const struct nla_policy
meta_atrribute_set_wifi_config_policy[META_VENDOR_ATTR_CONFIG_SIZE] = {
	[META_VENDOR_ATTR_CONFIG_TXCHAIN]     = { .type = NLA_U32 },
	[META_VENDOR_ATTR_CONFIG_ELNA_BYPASS] = { .type = NLA_U32 },
};

typedef int (*wifi_config_fn)(struct net_device *ndev,
				const struct nlattr *attr);

struct wifi_config_cb_entry {
	u32 id;
	wifi_config_fn cb;
};

static const struct wifi_config_cb_entry wifi_config_cb_table[] = {
	{META_VENDOR_ATTR_CONFIG_TXCHAIN, wl_cfgvendor_meta_set_txchain},
	{META_VENDOR_ATTR_CONFIG_ELNA_BYPASS, wl_cfgvendor_meta_set_elna_bypass},
};

static int
wl_cfgvendor_meta_set_txchain(struct net_device *ndev,
				const struct nlattr *attr)
{
	int err = 0;
	u32 tx_requested_mask;
	u32 tx_available_mask;
	u32 tx_available_antennas;
	struct wiphy *wiphy = ndev->ieee80211_ptr->wiphy;

	if (!wiphy->available_antennas_tx)
		return -EINVAL;

	tx_available_antennas = wiphy->available_antennas_tx;
	tx_available_mask = (1U << tx_available_antennas) - 1;
	tx_requested_mask = nla_get_u32(attr) & tx_available_mask;

	err = wldev_iovar_setint(ndev, "txchain", (s32)tx_requested_mask);
	return err;
}

static int
wl_cfgvendor_meta_set_elna_bypass(struct net_device *ndev,
				const struct nlattr *attr)
{
	int err = 0;
	s32 requested_bypass = (nla_get_u32(attr) ? 1 : 0);

	err = wldev_iovar_setint(ndev, "phy_elnabypass_force", requested_bypass);
	return err;
}

int
wl_cfgvendor_meta_set_wifi_config(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int len)
{
	int i, err = 0, table_sz;
	u32 id;
	struct net_device *ndev = wdev->netdev;
	struct nlattr *tb[META_VENDOR_ATTR_CONFIG_SIZE];
	wifi_config_fn callback;

	if (nla_parse(tb, META_VENDOR_ATTR_CONFIG_MAX, data, len,
		meta_atrribute_set_wifi_config_policy, NULL)) {
		WL_ERR(("invalid attr\n"));
		return  -EINVAL;
	}

	if (!ndev) {
		WL_ERR(("network device is NULL\n"));
		return -EINVAL;
	}

	table_sz = ARRAY_SIZE(wifi_config_cb_table);
	for (i = 0; i < table_sz; i++) {
		id = wifi_config_cb_table[i].id;

		if (!tb[id])
			continue;

		WL_INFORM(("Set wifi config %u\n", id));
		callback = wifi_config_cb_table[i].cb;
		err = callback(ndev, tb[id]);
		if (err) {
			WL_ERR(("Failed to config attr %d: ret: %d\n", i, err));
			break;
		}
	}

	return err;
}
