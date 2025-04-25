/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_sta_bitrates.c
 *
 * implementation for creating sysfs file sta_bitrates and sap_bitrates
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_bitrates.h>
#include "osif_psoc_sync.h"
#include "osif_vdev_sync.h"
#include "sme_api.h"
#include "qc_sap_ioctl.h"
#include "wma_api.h"

static int wlan_hdd_sta_set_11n_rate(struct hdd_adapter *adapter, int rate_code)
{
	uint8_t preamble = 0, nss = 0, rix = 0;
	int ret;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	enum wlan_phymode peer_phymode;
	uint8_t *peer_mac = adapter->deflink->session.station.conn_info.bssid.bytes;

	hdd_debug("Rate code %d", rate_code);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	if (rate_code != 0xffff) {
		rix = RC_2_RATE_IDX(rate_code);
		if (rate_code & 0x80) {
			preamble = WMI_RATE_PREAMBLE_HT;
			nss = HT_RC_2_STREAMS(rate_code) - 1;
		} else {
			status = ucfg_mlme_get_peer_phymode(hdd_ctx->psoc,
							    peer_mac,
							    &peer_phymode);
			if (QDF_IS_STATUS_ERROR(status)) {
				hdd_err("Failed to set rate");
				return -EINVAL;
			}
			if (IS_WLAN_PHYMODE_HE(peer_phymode)) {
				hdd_err("Do not set legacy rate %d in HE mode",
					rate_code);
				return -EINVAL;
			}
			nss = 0;
			rix = RC_2_RATE_IDX(rate_code);
			if (rate_code & 0x10) {
				preamble = WMI_RATE_PREAMBLE_CCK;
				if (rix != 0x3)
					/* Enable Short preamble
					 * always for CCK except 1mbps
					 */
					rix |= 0x4;
			} else {
				preamble = WMI_RATE_PREAMBLE_OFDM;
			}
		}
		rate_code = hdd_assemble_rate_code(preamble, nss, rix);
	}

	hdd_debug("wmi_vdev_param_fixed_rate val %d rix %d preamble %x nss %d",
		  rate_code, rix, preamble, nss);

	ret = wma_cli_set_command(adapter->deflink->vdev_id,
				  wmi_vdev_param_fixed_rate,
				  rate_code, VDEV_CMD);

	return ret;
}

static int wlan_hdd_sta_set_vht_rate(struct hdd_adapter *adapter, int rate_code)
{
	uint8_t preamble = 0, nss = 0, rix = 0;
	int ret;

	hdd_debug("Rate code %d", rate_code);

	if (rate_code != 0xffff) {
		rix = RC_2_RATE_IDX_11AC(rate_code);
		preamble = WMI_RATE_PREAMBLE_VHT;
		nss = HT_RC_2_STREAMS_11AC(rate_code) - 1;
		rate_code = hdd_assemble_rate_code(preamble, nss, rix);
	}

	hdd_debug("wmi_vdev_param_fixed_rate val %d rix %d preamble %x nss %d",
		  rate_code, rix, preamble, nss);

	ret = wma_cli_set_command(adapter->deflink->vdev_id,
				  wmi_vdev_param_fixed_rate,
				  rate_code, VDEV_CMD);

	return ret;
}

static int wlan_hdd_sta_set_11ax_rate(struct hdd_adapter *adapter, int rate)
{
	return hdd_set_11ax_rate(adapter, rate, NULL);
}

static int wlan_hdd_sap_set_11n_rate(struct hdd_adapter *adapter, int rate_code)
{
	uint8_t preamble = 0, nss = 0, rix = 0;
	struct sap_config *config = &adapter->deflink->session.ap.sap_config;
	int ret;

	hdd_debug("SET_HT_RATE val %d", rate_code);

	if (rate_code != 0xff) {
		rix = RC_2_RATE_IDX(rate_code);
		if (rate_code & 0x80) {
			if (config->SapHw_mode == eCSR_DOT11_MODE_11b ||
			    config->SapHw_mode == eCSR_DOT11_MODE_11b_ONLY ||
			    config->SapHw_mode == eCSR_DOT11_MODE_11g ||
			    config->SapHw_mode == eCSR_DOT11_MODE_11g_ONLY ||
			    config->SapHw_mode == eCSR_DOT11_MODE_abg ||
			    config->SapHw_mode == eCSR_DOT11_MODE_11a) {
				hdd_err("Not valid mode for HT");
				ret = -EIO;
				return ret;
				}
			preamble = WMI_RATE_PREAMBLE_HT;
			nss = HT_RC_2_STREAMS(rate_code) - 1;
		} else if (rate_code & 0x10) {
			if (config->SapHw_mode == eCSR_DOT11_MODE_11a) {
				hdd_err("Not valid for cck");
				ret = -EIO;
				return ret;
				}
			preamble = WMI_RATE_PREAMBLE_CCK;
			/* Enable Short preamble always for CCK except 1mbps */
			if (rix != 0x3)
				rix |= 0x4;
		} else {
			if (config->SapHw_mode == eCSR_DOT11_MODE_11b ||
			    config->SapHw_mode == eCSR_DOT11_MODE_11b_ONLY) {
				hdd_err("Not valid for OFDM");
				ret = -EIO;
				return ret;
				}
			preamble = WMI_RATE_PREAMBLE_OFDM;
		}
		hdd_debug("SET_HT_RATE val %d rix %d preamble %x nss %d",
			  rate_code, rix, preamble, nss);
		ret = wma_cli_set_command(adapter->deflink->vdev_id,
					  wmi_vdev_param_fixed_rate,
					  rate_code, VDEV_CMD);
		return ret;
	}

	return -EINVAL;
}

static int wlan_hdd_sap_set_vht_rate(struct hdd_adapter *adapter, int rate_code)
{
	uint8_t preamble = 0, nss = 0, rix = 0;
	int ret;
	struct sap_config *config = &adapter->deflink->session.ap.sap_config;

	if (config->SapHw_mode < eCSR_DOT11_MODE_11ac ||
	    config->SapHw_mode == eCSR_DOT11_MODE_11ax_ONLY ||
	    config->SapHw_mode == eCSR_DOT11_MODE_11be_ONLY) {
		hdd_err("SET_VHT_RATE: SapHw_mode= 0x%x, ch_freq: %d",
			config->SapHw_mode, config->chan_freq);
		ret = -EIO;
		return ret;
	}

	if (rate_code != 0xff) {
		rix = RC_2_RATE_IDX_11AC(rate_code);
		preamble = WMI_RATE_PREAMBLE_VHT;
		nss = HT_RC_2_STREAMS_11AC(rate_code) - 1;

		rate_code = hdd_assemble_rate_code(preamble, nss, rix);
	}
	hdd_debug("SET_VHT_RATE val %d rix %d preamble %x nss %d",
		  rate_code, rix, preamble, nss);

	ret = wma_cli_set_command(adapter->deflink->vdev_id,
				  wmi_vdev_param_fixed_rate,
				  rate_code, VDEV_CMD);

	return ret;
}

static int
wlan_hdd_sap_set_11ax_rate(struct hdd_adapter *adapter, int rate_code)
{
	struct sap_config *config = &adapter->deflink->session.ap.sap_config;

	return  hdd_set_11ax_rate(adapter, rate_code, config);
}

static ssize_t
__hdd_sysfs_sta_bitrates_store(struct net_device *net_dev,
			       char const *buf, size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	int ret, rate, rate_code;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);

	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	hdd_debug("sta_bitrates: count %zu buf_local:(%s)", count, buf_local);

	/* Get rate_type */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtoint(token, 0, &rate))
		return -EINVAL;

	/* Get rate */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtoint(token, 0, &rate_code))
		return -EINVAL;

	switch (rate) {
	case SET_11N_RATES:
		ret = wlan_hdd_sta_set_11n_rate(adapter, rate_code);
		if (ret) {
			hdd_err_rl("failed to set 11n rates");
			return ret;
		}
		break;
	case SET_11AC_RATES:
		ret = wlan_hdd_sta_set_vht_rate(adapter, rate_code);
		if (ret) {
			hdd_err_rl("failed to set 11ac rates");
			return ret;
		}
		break;
	case SET_11AX_RATES:
		ret = wlan_hdd_sta_set_11ax_rate(adapter, rate_code);
		if (ret) {
			hdd_err_rl("failed to set 11ax rates");
			return ret;
		}
		break;
	default:
		hdd_err("Invalid rate mode %u", rate);
		return -EINVAL;
	}

	return count;
}

static ssize_t
__hdd_sysfs_sap_bitrates_store(struct net_device *net_dev,
			       char const *buf, size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	int ret, rate, rate_code;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);

	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	hdd_debug("sta_bitrates: count %zu buf_local:(%s)", count, buf_local);

	/* Get rate_type */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtoint(token, 0, &rate))
		return -EINVAL;

	/* Get rate */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtoint(token, 0, &rate_code))
		return -EINVAL;

	switch (rate) {
	case SET_11N_RATES:
		ret = wlan_hdd_sap_set_11n_rate(adapter, rate_code);
		if (ret) {
			hdd_err_rl("failed to set 11n rates");
			return ret;
		}
		break;
	case SET_11AC_RATES:
		ret = wlan_hdd_sap_set_vht_rate(adapter, rate_code);
		if (ret) {
			hdd_err_rl("failed to set 11ac rates");
			return ret;
		}
		break;
	case SET_11AX_RATES:
		ret = wlan_hdd_sap_set_11ax_rate(adapter, rate_code);
		if (ret) {
			hdd_err_rl("failed to set 11ax rates");
			return ret;
		}
		break;
	default:
		hdd_err("Invalid rate mode %u", rate);
		return -EINVAL;
	}

	return count;
}

static ssize_t
hdd_sysfs_sap_bitrates_store(struct device *dev,
			     struct device_attribute *attr,
			     char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_sap_bitrates_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static ssize_t
hdd_sysfs_sta_bitrates_store(struct device *dev,
			     struct device_attribute *attr,
			     char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_sta_bitrates_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(sta_bitrates, 0220,
		   NULL, hdd_sysfs_sta_bitrates_store);

static DEVICE_ATTR(sap_bitrates, 0220,
		   NULL, hdd_sysfs_sap_bitrates_store);

int hdd_sysfs_sta_bitrates_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_sta_bitrates);
	if (error)
		hdd_err("could not create sta_bitrates sysfs file");

	return error;
}

int hdd_sysfs_sap_bitrates_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_sap_bitrates);
	if (error)
		hdd_err("could not create sap_bitrates sysfs file");

	return error;
}
void hdd_sysfs_sta_bitrates_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_sta_bitrates);
}

void hdd_sysfs_sap_bitrates_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_sap_bitrates);
}
