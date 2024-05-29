/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: offload lmac interface APIs definitions for crypto
 */

#include <qdf_mem.h>
#include <qdf_status.h>
#include <target_if_crypto.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include <wlan_objmgr_psoc_obj.h>
#include <target_if.h>
#include <wlan_crypto_global_def.h>
#include <wlan_crypto_global_api.h>
#include <wlan_objmgr_vdev_obj.h>
#include <cdp_txrx_cmn_struct.h>
#include <cds_api.h>
#include <cdp_txrx_cmn.h>
#include <wmi_unified_api.h>
#include <wmi_unified_crypto_api.h>
#include <cdp_txrx_peer_ops.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include "wlan_crypto_def_i.h"
#include "wlan_crypto_obj_mgr_i.h"

#ifdef FEATURE_WLAN_WAPI
#ifdef FEATURE_WAPI_BIG_ENDIAN
/*
 * All lithium firmware expects WAPI in big endian
 * format , whereas helium firmware's expect otherwise
 */

static void wlan_crypto_set_wapi_key(struct wlan_objmgr_vdev *vdev,
				     bool pairwise,
				     enum wlan_crypto_cipher_type cipher_type,
				     struct set_key_params *params)
{
	static const unsigned char tx_iv[16] = {0x5c, 0x36, 0x5c, 0x36, 0x5c,
						0x36, 0x5c, 0x36, 0x5c, 0x36,
						0x5c, 0x36, 0x5c, 0x36, 0x5c,
						0x36};

	static const unsigned char rx_iv[16] = {0x5c, 0x36, 0x5c, 0x36, 0x5c,
						0x36, 0x5c, 0x36, 0x5c, 0x36,
						0x5c, 0x36, 0x5c, 0x36, 0x5c,
						0x37};

	if (cipher_type != WLAN_CRYPTO_CIPHER_WAPI_SMS4 &&
	    cipher_type != WLAN_CRYPTO_CIPHER_WAPI_GCM4)
		return;

	if (vdev->vdev_mlme.vdev_opmode == QDF_SAP_MODE ||
	    vdev->vdev_mlme.vdev_opmode == QDF_P2P_GO_MODE) {
			qdf_mem_copy(&params->rx_iv, &tx_iv,
					 WLAN_CRYPTO_WAPI_IV_SIZE);
			qdf_mem_copy(params->tx_iv, &rx_iv,
					 WLAN_CRYPTO_WAPI_IV_SIZE);
	} else {
			qdf_mem_copy(params->rx_iv, &rx_iv,
					 WLAN_CRYPTO_WAPI_IV_SIZE);
			qdf_mem_copy(params->tx_iv, &tx_iv,
					 WLAN_CRYPTO_WAPI_IV_SIZE);
		}

	params->key_txmic_len = WLAN_CRYPTO_MIC_LEN;
	params->key_rxmic_len = WLAN_CRYPTO_MIC_LEN;
}
#else
static void wlan_crypto_set_wapi_key(struct wlan_objmgr_vdev *vdev,
				     bool pairwise,
				     enum wlan_crypto_cipher_type cipher_type,
				     struct set_key_params *params)
{
	static const unsigned char tx_iv[16] = {0x36, 0x5c, 0x36, 0x5c, 0x36,
						0x5c, 0x36, 0x5c, 0x36, 0x5c,
						0x36, 0x5c, 0x36, 0x5c, 0x36,
						0x5c};

	static const unsigned char rx_iv[16] = {0x5c, 0x36, 0x5c, 0x36, 0x5c,
						0x36, 0x5c, 0x36, 0x5c, 0x36,
						0x5c, 0x36, 0x5c, 0x36, 0x5c,
						0x37};

	if (cipher_type != WLAN_CRYPTO_CIPHER_WAPI_SMS4 &&
	    cipher_type != WLAN_CRYPTO_CIPHER_WAPI_GCM4)
		return;

	qdf_mem_copy(&params->rx_iv, &rx_iv,
		     WLAN_CRYPTO_WAPI_IV_SIZE);
	qdf_mem_copy(&params->tx_iv, &tx_iv,
		     WLAN_CRYPTO_WAPI_IV_SIZE);

	if (vdev->vdev_mlme.vdev_opmode == QDF_SAP_MODE) {
		if (pairwise)
			params->tx_iv[0] = 0x37;

		params->rx_iv[WLAN_CRYPTO_WAPI_IV_SIZE - 1] = 0x36;
	} else {
		if (!pairwise)
			params->rx_iv[WLAN_CRYPTO_WAPI_IV_SIZE - 1] = 0x36;
	}

	params->key_txmic_len = WLAN_CRYPTO_MIC_LEN;
	params->key_rxmic_len = WLAN_CRYPTO_MIC_LEN;
}
#endif /* FEATURE_WAPI_BIG_ENDIAN */
#else
static inline void wlan_crypto_set_wapi_key(struct wlan_objmgr_vdev *vdev,
					    bool pairwise,
					    enum wlan_crypto_cipher_type cipher,
					    struct set_key_params *params)
{
}
#endif /* FEATURE_WLAN_WAPI */

QDF_STATUS
target_if_crypto_vdev_set_param(struct wlan_objmgr_psoc *psoc, uint32_t vdev_id,
				uint32_t param_id, uint32_t param_value)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	struct vdev_set_params param = {0};

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	if (vdev_id >= WLAN_MAX_VDEVS) {
		target_if_err("vdev_id: %d is invalid, reject the req: param id %d val %d",
			      vdev_id, param_id, param_value);
		return QDF_STATUS_E_INVAL;
	}

	param.vdev_id = vdev_id;
	param.param_id = param_id;
	param.param_value = param_value;

	return wmi_unified_vdev_set_param_send(wmi_handle, &param);
}

QDF_STATUS target_if_crypto_set_key(struct wlan_objmgr_vdev *vdev,
				    struct wlan_crypto_key *req,
				    enum wlan_crypto_key_type key_type)
{
	struct set_key_params params = {0};
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_peer *peer;
	enum cdp_sec_type sec_type = cdp_sec_type_none;
	enum wlan_peer_type peer_type = 0;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t pn[4] = {0, 0, 0, 0};
	bool peer_exist = false;
	uint8_t def_tx_idx;
	wmi_unified_t pdev_wmi_handle;
	bool pairwise;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		target_if_err("Invalid PDEV");
		return QDF_STATUS_E_FAILURE;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("Invalid PSOC");
		return QDF_STATUS_E_FAILURE;
	}
	soc = wlan_psoc_get_dp_handle(psoc);
	if (!soc) {
		target_if_err("Invalid DP Handle");
		return QDF_STATUS_E_FAILURE;
	}
	params.vdev_id = wlan_vdev_get_id(vdev);
	params.key_idx = req->keyix;
	qdf_mem_copy(params.peer_mac, req->macaddr, QDF_MAC_ADDR_SIZE);
	pdev_wmi_handle = GET_WMI_HDL_FROM_PDEV(pdev);
	if (!pdev_wmi_handle) {
		target_if_err("Invalid PDEV WMI handle");
		return QDF_STATUS_E_FAILURE;
	}

	params.key_flags = req->flags;
	if (key_type != WLAN_CRYPTO_KEY_TYPE_UNICAST) {
		pairwise = false;
		params.key_flags |= GROUP_USAGE;

	} else {
		pairwise = true;
		params.key_flags |= PAIRWISE_USAGE;
	}
	qdf_mem_copy(&params.key_rsc_counter,
		     &req->keyrsc[0], sizeof(uint64_t));

	target_if_debug("key_type %d, mac: %02x:%02x:%02x:%02x:%02x:%02x",
			key_type, req->macaddr[0], req->macaddr[1],
			req->macaddr[2], req->macaddr[3], req->macaddr[4],
			req->macaddr[5]);

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_NAN_DISC_MODE) {
		peer_exist = cdp_find_peer_exist(soc,
						 pdev->pdev_objmgr.wlan_pdev_id,
						 req->macaddr);

		peer = wlan_objmgr_get_peer_by_mac(psoc, req->macaddr,
						   WLAN_CRYPTO_ID);
		if (peer) {
			peer_type = wlan_peer_get_peer_type(peer);
			if (peer_type == WLAN_PEER_RTT_PASN &&
			    key_type == WLAN_CRYPTO_KEY_TYPE_UNICAST)
				peer_exist = true;

			wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
		}

		if ((key_type == WLAN_CRYPTO_KEY_TYPE_UNICAST) && !peer_exist) {
			target_if_err("Invalid peer");
			return QDF_STATUS_E_FAILURE;
		}
	}

	params.key_cipher = wlan_crypto_cipher_to_wmi_cipher(req->cipher_type);
	sec_type = wlan_crypto_cipher_to_cdp_sec_type(req->cipher_type);
	wlan_crypto_set_wapi_key(vdev, pairwise, req->cipher_type, &params);

	switch (req->cipher_type) {
	case WLAN_CRYPTO_CIPHER_WEP:
	case WLAN_CRYPTO_CIPHER_WEP_40:
	case WLAN_CRYPTO_CIPHER_WEP_104:
		def_tx_idx = wlan_crypto_get_default_key_idx(vdev, false);
		if (pairwise && params.key_idx == def_tx_idx)
			params.key_flags |= TX_USAGE;
		else if ((vdev->vdev_mlme.vdev_opmode == QDF_SAP_MODE) &&
			 (params.key_idx == def_tx_idx))
			params.key_flags |= TX_USAGE;
		break;
	case WLAN_CRYPTO_CIPHER_TKIP:
		params.key_txmic_len = WLAN_CRYPTO_MIC_LEN;
		params.key_rxmic_len = WLAN_CRYPTO_MIC_LEN;
		break;
	default:
		break;
	}

	qdf_mem_copy(&params.key_data[0], &req->keyval[0], req->keylen);
	params.key_len = req->keylen;

	/* Set PN check & security type in data path */
	qdf_mem_copy(&pn[0], &params.key_rsc_counter, sizeof(uint64_t));

	if (peer_type == WLAN_PEER_RTT_PASN)
		goto send_install_key;

	cdp_set_pn_check(soc, vdev->vdev_objmgr.vdev_id, req->macaddr,
			 sec_type, pn);

	cdp_set_key_sec_type(soc, vdev->vdev_objmgr.vdev_id, req->macaddr,
			     sec_type, pairwise);

	cdp_set_key(soc, vdev->vdev_objmgr.vdev_id, req->macaddr, pairwise,
		    (uint32_t *)(req->keyval + WLAN_CRYPTO_IV_SIZE +
		     WLAN_CRYPTO_MIC_LEN));
send_install_key:
	target_if_debug("vdev_id:%d, key: idx:%d,len:%d", params.vdev_id,
			params.key_idx, params.key_len);
	target_if_debug("peer mac "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(params.peer_mac));
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_CRYPTO, QDF_TRACE_LEVEL_DEBUG,
			   &params.key_rsc_counter, sizeof(uint64_t));
	status = wmi_unified_setup_install_key_cmd(pdev_wmi_handle, &params);

	/* Zero-out local key variables */
	qdf_mem_zero(&params, sizeof(struct set_key_params));

	return status;
}

/**
 * target_if_crypto_install_key_comp_evt_handler() - install key complete
 *   handler
 * @handle: wma handle
 * @event: event data
 * @len: data length
 *
 * This event is sent by fw once WPA/WPA2 keys are installed in fw.
 *
 * Return: 0 for success or error code
 */
static int
target_if_crypto_install_key_comp_evt_handler(void *handle, uint8_t *event,
					      uint32_t len)
{
	struct wlan_crypto_comp_priv *priv_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct wmi_install_key_comp_event params;
	QDF_STATUS status;
	wmi_unified_t wmi_handle;
	struct crypto_add_key_result result;

	if (!event || !handle) {
		target_if_err("invalid param");
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(handle);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("invalid wmi handle");
		return -EINVAL;
	}

	status = wmi_extract_install_key_comp_event(wmi_handle, event,
						    len, &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("received invalid buf from target");
		return -EINVAL;
	}

	target_if_debug("vdev %d mac " QDF_MAC_ADDR_FMT " ix %x flags %x status %d",
			params.vdev_id,
			QDF_MAC_ADDR_REF(params.peer_macaddr),
			params.key_ix, params.key_flags, params.status);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, params.vdev_id,
						    WLAN_CRYPTO_ID);
	if (!vdev) {
		target_if_err("vdev %d is null", params.vdev_id);
		return -EINVAL;
	}

	priv_obj = wlan_get_vdev_crypto_obj(vdev);
	if (!priv_obj) {
		target_if_err("priv_obj is null");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CRYPTO_ID);
		return -EINVAL;
	}

	result.vdev_id = params.vdev_id;
	result.key_ix = params.key_ix;
	result.key_flags = params.key_flags;
	result.status = params.status;
	qdf_mem_copy(result.peer_macaddr, params.peer_macaddr,
		     QDF_MAC_ADDR_SIZE);

	if (priv_obj->add_key_cb)
		priv_obj->add_key_cb(priv_obj->add_key_ctx, &result);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_CRYPTO_ID);

	return 0;
}

static QDF_STATUS
target_if_crypto_register_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	if (!psoc || !GET_WMI_HDL_FROM_PSOC(psoc)) {
		target_if_err("psoc or psoc->tgt_if_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_vdev_install_key_complete_event_id,
			target_if_crypto_install_key_comp_evt_handler,
			WMI_RX_WORK_CTX);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("register_event_handler failed: err %d", status);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
target_if_crypto_deregister_events(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc || !GET_WMI_HDL_FROM_PSOC(psoc)) {
		target_if_err("psoc or psoc->tgt_if_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_vdev_install_key_complete_event_id);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
target_if_crypto_set_ltf_keyseed(struct wlan_objmgr_psoc *psoc,
				 struct wlan_crypto_ltf_keyseed_data *data)
{
	QDF_STATUS status;
	wmi_unified_t wmi = GET_WMI_HDL_FROM_PSOC(psoc);

	if (!psoc || !wmi) {
		target_if_err("%s is null", !psoc ? "psoc" : "wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_send_vdev_set_ltf_key_seed_cmd(wmi, data);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("set LTF keyseed failed");

	return status;
}

QDF_STATUS target_if_crypto_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_crypto_tx_ops *crypto;

	if (!tx_ops) {
		target_if_err("txops NULL");
		return QDF_STATUS_E_FAILURE;
	}
	crypto = &tx_ops->crypto_tx_ops;

	crypto->set_key = target_if_crypto_set_key;
	crypto->set_ltf_keyseed = target_if_crypto_set_ltf_keyseed;
	crypto->set_vdev_param  = target_if_crypto_vdev_set_param;
	crypto->register_events = target_if_crypto_register_events;
	crypto->deregister_events = target_if_crypto_deregister_events;

	return QDF_STATUS_SUCCESS;
}

