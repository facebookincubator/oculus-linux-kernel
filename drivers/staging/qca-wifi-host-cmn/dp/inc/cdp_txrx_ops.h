/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @file cdp_txrx_ops.h
 * @brief Define the host data path converged API functions
 * called by the host control SW and the OS interface module
 */
#ifndef _CDP_TXRX_CMN_OPS_H_
#define _CDP_TXRX_CMN_OPS_H_

#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_stats_struct.h>
#include "cdp_txrx_handle.h"
#include <cdp_txrx_mon_struct.h>
#include "wlan_objmgr_psoc_obj.h"
#include <wmi_unified_api.h>
#include <wdi_event_api.h>

#ifdef IPA_OFFLOAD
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)
#include <qdf_ipa_wdi3.h>
#else
#include <qdf_ipa.h>
#endif
#endif

/**
 * bitmap values to indicate special handling of peer_delete
 */
#define CDP_PEER_DELETE_NO_SPECIAL             0
#define CDP_PEER_DO_NOT_START_UNMAP_TIMER      1

struct hif_opaque_softc;
/**
 * cdp_ctrl_mlo_mgr - opaque handle for mlo manager context
 */
struct cdp_ctrl_mlo_mgr;


/* same as ieee80211_nac_param */
enum cdp_nac_param_cmd {
	/* IEEE80211_NAC_PARAM_ADD */
	CDP_NAC_PARAM_ADD = 1,
	/* IEEE80211_NAC_PARAM_DEL */
	CDP_NAC_PARAM_DEL,
	/* IEEE80211_NAC_PARAM_LIST */
	CDP_NAC_PARAM_LIST,
};

/**
 * enum cdp_tx_filter_action - TX peer filtering action
 * @CDP_TX_FILTER_ACTION_ADD: add peer
 * @CDP_TX_FILTER_ACTION_DEL: delete peer
 *
 * whether add or delete
 */
enum cdp_tx_filter_action {
	/* add peer mac address*/
	CDP_TX_FILTER_ACTION_ADD = 1,
	/* delete peer mac address */
	CDP_TX_FILTER_ACTION_DEL,
	/* Enable Tx peer filtering */
	CDP_TX_FILTER_ACTION_ENABLE_FILTERING,
	/* Clear Tx peer filtering */
	CDP_TX_FILTER_ACTION_CLEAR_FILTERING,
};

#define CDP_DELBA_INTERVAL_MS 3000
/**
 * enum cdp_delba_rcode - CDP reason code for sending DELBA
 * @CDP_DELBA_REASON_NONE: None
 * @CDP_DELBA_2K_JUMP: Sending DELBA from 2k_jump_handle
 */
enum cdp_delba_rcode {
	CDP_DELBA_REASON_NONE = 0,
	CDP_DELBA_2K_JUMP,
};

/**
 * enum vdev_peer_protocol_enter_exit - whether ingress or egress
 * @CDP_VDEV_PEER_PROTOCOL_IS_INGRESS: ingress
 * @CDP_VDEV_PEER_PROTOCOL_IS_EGRESS: egress
 *
 * whether ingress or egress
 */
enum vdev_peer_protocol_enter_exit {
	CDP_VDEV_PEER_PROTOCOL_IS_INGRESS,
	CDP_VDEV_PEER_PROTOCOL_IS_EGRESS
};

/**
 * enum vdev_peer_protocol_tx_rx - whether tx or rx
 * @CDP_VDEV_PEER_PROTOCOL_IS_TX: tx
 * @CDP_VDEV_PEER_PROTOCOL_IS_RX: rx
 *
 * whether tx or rx
 */
enum vdev_peer_protocol_tx_rx {
	CDP_VDEV_PEER_PROTOCOL_IS_TX,
	CDP_VDEV_PEER_PROTOCOL_IS_RX
};

/**
 * enum vdev_ll_conn_actions - Actions to informvdev about
 *			       low latency connection.
 * @CDP_VDEV_LL_CONN_ADD: Add Low latency connection
 * @CDP_VDEV_LL_CONN_DEL: Delete Low latency connection
 */
enum vdev_ll_conn_actions {
	CDP_VDEV_LL_CONN_ADD,
	CDP_VDEV_LL_CONN_DEL
};

/**
 * enum cdp_peer_txq_flush_policy: Values for peer TX TID queues flush policy
 * @CDP_PEER_TXQ_FLUSH_POLICY_NONE: No flush policy configured
 * @CDP_PEER_TXQ_FLUSH_POLICY_IMMEDIATE: Flush peer TX TID queue(s) immediately
 * @CDP_PEER_TXQ_FLUSH_POLICY_TWT_SP_END: Flush peer TX TID queue(s) at SP end
 *
 * This is used to map the 'flush_policy' in WMI_PEER_FLUSH_POLICY_CMDID
 */
enum cdp_peer_txq_flush_policy {
	CDP_PEER_TXQ_FLUSH_POLICY_NONE = 0,
	CDP_PEER_TXQ_FLUSH_POLICY_IMMEDIATE = 1,
	CDP_PEER_TXQ_FLUSH_POLICY_TWT_SP_END = 2,

	/* keep last */
	CDP_PEER_TXQ_FLUSH_POLICY_INVALID,
};

/**
 * struct cdp_mlo_ops - MLO ops for multichip
 * @mlo_soc_setup: setup DP mlo for SOC
 * @mlo_soc_teardown: teardown DP mlo for SOC
 * @mlo_setup_complete: indication to DP that all SOCs mlo is setup
 * @mlo_update_delta_tsf2: update delta tsf2 for link
 * @mlo_update_delta_tqm: update delta tqm for SOC
 * @mlo_update_mlo_ts_offset: update MLO timestamp offset for SOC
 * @mlo_ctxt_attach: Attach DP MLO context
 * @mlo_ctxt_detach: Detach DP MLO context
 */
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
struct cdp_mlo_ops {
	void (*mlo_soc_setup)(struct cdp_soc_t *cdp_soc,
			      struct cdp_mlo_ctxt *mlo_ctxt);
	void (*mlo_soc_teardown)(struct cdp_soc_t *cdp_soc,
				 struct cdp_mlo_ctxt *mlo_ctxt,
				 bool is_force_down);
	QDF_STATUS (*update_mlo_ptnr_list)(struct cdp_soc_t *soc_hdl,
					   int8_t *vdev_ids, uint8_t num_vdevs,
					   uint8_t vdev_id);
	void (*mlo_setup_complete)(struct cdp_mlo_ctxt *mlo_ctxt);
	struct cdp_mlo_ctxt *(*mlo_ctxt_attach)(struct cdp_ctrl_mlo_mgr *m_ctx);
	void (*mlo_ctxt_detach)(struct cdp_mlo_ctxt *mlo_ctxt);
	void (*mlo_update_delta_tsf2)(struct cdp_soc_t *soc_hdl,
				      uint8_t pdev_id,
				      uint64_t delta_tsf2);
	void (*mlo_update_delta_tqm)(struct cdp_soc_t *soc_hdl,
				     uint64_t delta_tqm);
	void (*mlo_update_mlo_ts_offset)(struct cdp_soc_t *soc_hdl,
					 uint64_t offset);
};
#endif

/******************************************************************************
 *
 * Control Interface (A Interface)
 *
 *****************************************************************************/

struct cdp_cmn_ops {

	QDF_STATUS (*txrx_soc_attach_target)(ol_txrx_soc_handle soc);

	int (*txrx_pdev_attach_target)(ol_txrx_soc_handle soc, uint8_t pdev_id);

	QDF_STATUS (*txrx_vdev_attach)
		(struct cdp_soc_t *soc, uint8_t pdev_id,
		 struct cdp_vdev_info *vdev_info);

	QDF_STATUS
	(*txrx_vdev_detach)(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
			    ol_txrx_vdev_delete_cb callback,
			    void *cb_context);

	QDF_STATUS (*txrx_pdev_attach)
		(ol_txrx_soc_handle soc,
		 struct cdp_pdev_attach_params *params);

	int (*txrx_pdev_post_attach)(struct cdp_soc_t *soc, uint8_t pdev_id);

	void
	(*txrx_pdev_pre_detach)(struct cdp_soc_t *soc, uint8_t pdev_id,
				int force);

	QDF_STATUS
	(*txrx_pdev_detach)(struct cdp_soc_t *psoc, uint8_t pdev_id,
			    int force);

	/**
	 * txrx_pdev_deinit() - Deinitialize pdev and dp ring memory
	 * @soc: soc dp handle
	 * @pdev_id: id of Dp pdev handle
	 * @force: Force deinit or not
	 *
	 * Return: QDF_STATUS
	 */
	QDF_STATUS
	(*txrx_pdev_deinit)(struct cdp_soc_t *soc, uint8_t pdev_id,
			    int force);

	QDF_STATUS
	(*txrx_peer_create)
		(ol_txrx_soc_handle soc, uint8_t vdev_id,
		 uint8_t *peer_mac_addr, enum cdp_peer_type peer_type);

	QDF_STATUS
	(*txrx_peer_setup)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			   uint8_t *peer_mac,
			   struct cdp_peer_setup_info *setup_info);

	QDF_STATUS
	(*txrx_cp_peer_del_response)
		(ol_txrx_soc_handle soc, uint8_t vdev_id,
		 uint8_t *peer_mac_addr);

	QDF_STATUS
	(*txrx_peer_teardown)
		(struct cdp_soc_t *soc, uint8_t vdev_id, uint8_t *peer_mac);

	int (*txrx_peer_add_ast)
		(ol_txrx_soc_handle soc, uint8_t vdev_id, uint8_t *peer_mac,
		uint8_t *mac_addr, enum  cdp_txrx_ast_entry_type type,
		uint32_t flags);

	int (*txrx_peer_update_ast)
		(ol_txrx_soc_handle soc, uint8_t vdev_id, uint8_t *peer_mac,
		uint8_t *mac_addr, uint32_t flags);

	bool (*txrx_peer_get_ast_info_by_soc)
		(ol_txrx_soc_handle soc, uint8_t *ast_mac_addr,
		 struct cdp_ast_entry_info *ast_entry_info);

	bool (*txrx_peer_get_ast_info_by_pdev)
		(ol_txrx_soc_handle soc, uint8_t *ast_mac_addr,
		 uint8_t pdev_id,
		 struct cdp_ast_entry_info *ast_entry_info);

	QDF_STATUS (*txrx_peer_ast_delete_by_soc)
		(ol_txrx_soc_handle soc, uint8_t *ast_mac_addr,
		txrx_ast_free_cb callback,
		void *cookie);

	QDF_STATUS (*txrx_peer_ast_delete_by_pdev)
		(ol_txrx_soc_handle soc, uint8_t *ast_mac_addr,
		 uint8_t pdev_id,
		txrx_ast_free_cb callback,
		void *cookie);

	QDF_STATUS
	(*txrx_peer_delete)(struct cdp_soc_t *soc, uint8_t vdev_id,
			    uint8_t *peer_mac, uint32_t bitmap,
			    enum cdp_peer_type peer_type);

	QDF_STATUS (*txrx_set_monitor_mode)(struct cdp_soc_t *soc,
					    uint8_t vdev_id,
					    uint8_t smart_monitor);
	void (*txrx_peer_delete_sync)(struct cdp_soc_t *soc, uint8_t vdev_id,
				      uint8_t *peer_mac,
				      QDF_STATUS(*delete_cb)(
						uint8_t vdev_id,
						uint32_t peerid_cnt,
						uint16_t *peerid_list),
				      uint32_t bitmap);

	void (*txrx_peer_unmap_sync_cb_set)(struct cdp_soc_t *soc_hdl,
					    uint8_t pdev_id,
					    ol_txrx_peer_unmap_sync_cb
					    peer_unmap_sync);

	QDF_STATUS
	(*txrx_get_peer_mac_from_peer_id)
		(struct cdp_soc_t *cdp_soc,
		 uint32_t peer_id, uint8_t *peer_mac);

	void
	(*txrx_vdev_tx_lock)(struct cdp_soc_t *soc, uint8_t vdev_id);

	void
	(*txrx_vdev_tx_unlock)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
	(*txrx_ath_getstats)(struct cdp_soc_t *soc, uint8_t id,
			     struct cdp_dev_stats *stats, uint8_t type);

	QDF_STATUS
	(*txrx_set_gid_flag)(struct cdp_soc_t *soc, uint8_t pdev_id,
			     u_int8_t *mem_status,
			     u_int8_t *user_position);

	uint32_t (*txrx_fw_supported_enh_stats_version)(struct cdp_soc_t *soc,
							uint8_t pdev_id);

	QDF_STATUS
	(*txrx_if_mgmt_drain)(struct cdp_soc_t *soc, uint8_t pdev_id,
			      int force);

	QDF_STATUS
	(*txrx_set_curchan)(struct cdp_soc_t *soc, uint8_t pdev_id,
			    uint32_t chan_mhz);

	QDF_STATUS
	(*txrx_set_privacy_filters)
		(struct cdp_soc_t *soc, uint8_t vdev_id, void *filter,
		 uint32_t num);

	uint32_t (*txrx_get_cfg)(struct cdp_soc_t *soc, enum cdp_dp_cfg cfg);

	/********************************************************************
	 * Data Interface (B Interface)
	 ********************************************************************/

	QDF_STATUS
	(*txrx_vdev_register)(struct cdp_soc_t *soc, uint8_t vdev_id,
			      ol_osif_vdev_handle osif_vdev,
			      struct ol_txrx_ops *txrx_ops);

	int (*txrx_mgmt_send)(struct cdp_soc_t *soc, uint8_t vdev_id,
			      qdf_nbuf_t tx_mgmt_frm, uint8_t type);

	int (*txrx_mgmt_send_ext)(struct cdp_soc_t *soc, uint8_t vdev_id,
				  qdf_nbuf_t tx_mgmt_frm, uint8_t type,
				  uint8_t use_6mbps, uint16_t chanfreq);

	/**
	 * ol_txrx_mgmt_tx_cb - tx management delivery notification
	 * callback function
	 */

	QDF_STATUS
	(*txrx_mgmt_tx_cb_set)(struct cdp_soc_t *soc, uint8_t pdev_id,
			       uint8_t type,
			       ol_txrx_mgmt_tx_cb download_cb,
			       ol_txrx_mgmt_tx_cb ota_ack_cb,
			       void *ctxt);

	/**
	 * ol_txrx_data_tx_cb - Function registered with the data path
	 * that is called when tx frames marked as "no free" are
	 * done being transmitted
	 */

	void (*txrx_data_tx_cb_set)(struct cdp_soc_t *soc, uint8_t vdev_id,
				    ol_txrx_data_tx_cb callback, void *ctxt);

	qdf_nbuf_t (*tx_send_exc)
		(ol_txrx_soc_handle soc, uint8_t vdev_id, qdf_nbuf_t msdu_list,
		 struct cdp_tx_exception_metadata *tx_exc_metadata);

	/*******************************************************************
	 * Statistics and Debugging Interface (C Interface)
	 ********************************************************************/

	int (*txrx_aggr_cfg)(struct cdp_soc_t *soc, uint8_t vdev_id,
			     int max_subfrms_ampdu,
			     int max_subfrms_amsdu);

	A_STATUS
	(*txrx_fw_stats_get)(struct cdp_soc_t *soc, uint8_t vdev_id,
			     struct ol_txrx_stats_req *req,
			     bool per_vdev, bool response_expected);

	int (*txrx_debug)(struct cdp_soc_t *soc, uint8_t vdev_id,
			  int debug_specs);

	QDF_STATUS
	(*txrx_fw_stats_cfg)(struct cdp_soc_t *soc, uint8_t vdev_id,
			     uint8_t cfg_stats_type, uint32_t cfg_val);

	void (*txrx_print_level_set)(unsigned level);

	/**
	 * ol_txrx_get_vdev_mac_addr() - Return mac addr of vdev
	 * @soc: datapath soc handle
	 * @vdev_id: vdev id
	 *
	 * Return: vdev mac address
	 */
	uint8_t * (*txrx_get_vdev_mac_addr)(struct cdp_soc_t *soc,
					    uint8_t vdev_id);

	/**
	 * ol_txrx_get_ctrl_pdev_from_vdev() - Return control pdev of vdev
	 * @soc: datapath soc handle
	 * @vdev_id: vdev id
	 *
	 * Return: Handle to control pdev
	 */
	struct cdp_cfg *(*txrx_get_ctrl_pdev_from_vdev)(struct cdp_soc_t *soc,
							uint8_t vdev_id);

	/**
	 * txrx_get_mon_vdev_from_pdev() - Return monitor mode vdev
	 * @soc: datapath soc handle
	 * @pdev: pdev id
	 *
	 * Return: vdev_id
	 */
	uint8_t (*txrx_get_mon_vdev_from_pdev)(struct cdp_soc_t *soc,
					       uint8_t pdev_id);

	void (*txrx_soc_detach)(struct cdp_soc_t *soc);

	/**
	 * txrx_soc_deinit() - Deinitialize dp soc and dp ring memory
	 * @soc: Opaque Dp handle
	 *
	 * Return None
	 */
	void (*txrx_soc_deinit)(struct cdp_soc_t *soc);

	/**
	 * txrx_soc_init() - Initialize dp soc and dp ring memory
	 * @soc: Opaque Dp handle
	 * @ctrl_psoc: Opaque Cp handle
	 * @htchdl: Opaque htc handle
	 * @hifhdl: Opaque hif handle
	 *
	 * Return: None
	 */
	void *(*txrx_soc_init)(struct cdp_soc_t *soc,
			       struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
			       struct hif_opaque_softc *hif_handle,
			       HTC_HANDLE htc_handle, qdf_device_t qdf_osdev,
			       struct ol_if_ops *ol_ops, uint16_t device_id);

	QDF_STATUS (*txrx_pdev_init)(ol_txrx_soc_handle soc,
				     HTC_HANDLE htc_handle,
				     qdf_device_t qdf_osdev,
				     uint8_t pdev_id);

	/**
	 * txrx_tso_soc_attach() - TSO attach handler triggered during
	 * dynamic tso activation
	 * @soc: Opaque Dp handle
	 *
	 * Return: QDF status
	 */
	QDF_STATUS (*txrx_tso_soc_attach)(struct cdp_soc_t *soc);

	/**
	 * txrx_tso_soc_detach() - TSO detach handler triggered during
	 * dynamic tso de-activation
	 * @soc: Opaque Dp handle
	 *
	 * Return: QDF status
	 */
	QDF_STATUS (*txrx_tso_soc_detach)(struct cdp_soc_t *soc);
	int (*addba_resp_tx_completion)(struct cdp_soc_t *cdp_soc,
					uint8_t *peer_mac,
					uint16_t vdev_id, uint8_t tid,
					int status);

	int (*addba_requestprocess)(struct cdp_soc_t *cdp_soc,
				    uint8_t *peer_mac,
				    uint16_t vdev_id,
				    uint8_t dialogtoken,
				    uint16_t tid, uint16_t batimeout,
				    uint16_t buffersize,
				    uint16_t startseqnum);

	QDF_STATUS
	(*addba_responsesetup)(struct cdp_soc_t *cdp_soc,
			       uint8_t *peer_mac,
			       uint16_t vdev_id, uint8_t tid,
			       uint8_t *dialogtoken, uint16_t *statuscode,
			       uint16_t *buffersize, uint16_t *batimeout);

	int (*delba_process)(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			     uint16_t vdev_id, int tid, uint16_t reasoncode);

	QDF_STATUS (*tid_update_ba_win_size)(ol_txrx_soc_handle soc,
					     uint8_t *peer_mac,
					     uint16_t vdev_id, uint8_t tid,
					     uint16_t buffersize);
	/**
	 * delba_tx_completion() - Indicate delba tx status
	 * @cdp_soc: soc handle
	 * @peer_mac: Peer mac address
	 * @vdev_id: vdev id
	 * @tid: Tid number
	 * @status: Tx completion status
	 *
	 * Return: 0 on Success, 1 on failure
	 */
	int (*delba_tx_completion)(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
				   uint16_t vdev_id,
				   uint8_t tid, int status);

	QDF_STATUS
	(*set_addba_response)(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			      uint16_t vdev_id, uint8_t tid,
			      uint16_t statuscode);

	QDF_STATUS
	(*set_vdev_dscp_tid_map)(struct cdp_soc_t *soc_handle,
				 uint8_t vdev_id, uint8_t map_id);
	int (*txrx_get_total_per)(struct cdp_soc_t *soc, uint8_t pdev_id);

	void (*flush_cache_rx_queue)(void);

	QDF_STATUS (*set_pdev_dscp_tid_map)(struct cdp_soc_t *soc_handle,
					    uint8_t pdev_id,
					    uint8_t map_id,
					    uint8_t tos, uint8_t tid);

	QDF_STATUS (*txrx_stats_request)(struct cdp_soc_t *soc_handle,
					 uint8_t vdev_id,
					 struct cdp_txrx_stats_req *req);

	QDF_STATUS (*display_stats)(struct cdp_soc_t *psoc, uint16_t value,
				    enum qdf_stats_verbosity_level level);

	QDF_STATUS (*txrx_intr_attach)(struct cdp_soc_t *soc_handle);
	void (*txrx_intr_detach)(struct cdp_soc_t *soc_handle);
	void (*txrx_ppeds_stop)(struct cdp_soc_t *soc_handle);
	QDF_STATUS  (*set_pn_check)(struct cdp_soc_t *soc_handle,
				    uint8_t vdev_id, uint8_t *peermac,
				    enum cdp_sec_type sec_type,
				    uint32_t *rx_pn);

	QDF_STATUS(*set_key_sec_type)(struct cdp_soc_t *soc_handle,
				      uint8_t vdev_id, uint8_t *peermac,
				      enum cdp_sec_type sec_type,
				      bool is_unicast);

	QDF_STATUS (*update_config_parameters)(struct cdp_soc *psoc,
			struct cdp_config_params *params);

	void *(*get_dp_txrx_handle)(ol_txrx_soc_handle soc, uint8_t pdev_id);
	void (*set_dp_txrx_handle)(ol_txrx_soc_handle soc, uint8_t pdev_id,
				   void *dp_hdl);

	void *(*get_vdev_dp_ext_txrx_handle)(struct cdp_soc_t *soc,
					     uint8_t vdev_id);
	QDF_STATUS (*set_vdev_dp_ext_txrx_handle)(struct cdp_soc_t *soc,
						  uint8_t vdev_id,
						  uint16_t size);

	void *(*get_soc_dp_txrx_handle)(struct cdp_soc *soc_handle);
	void (*set_soc_dp_txrx_handle)(struct cdp_soc *soc_handle,
			void *dp_txrx_handle);

	QDF_STATUS (*map_pdev_to_lmac)(ol_txrx_soc_handle soc, uint8_t pdev_id,
				       uint32_t lmac_id);

	QDF_STATUS (*handle_mode_change)(ol_txrx_soc_handle soc,
					 uint8_t pdev_id, uint32_t lmac_id);

	QDF_STATUS (*set_pdev_status_down)(struct cdp_soc_t *soc_handle,
					   uint8_t pdev_id, bool is_pdev_down);

	QDF_STATUS (*txrx_peer_reset_ast)
		(ol_txrx_soc_handle soc, uint8_t *ast_macaddr,
		 uint8_t *peer_macaddr, uint8_t vdev_id);

	QDF_STATUS (*txrx_peer_reset_ast_table)(ol_txrx_soc_handle soc,
						uint8_t vdev_id);

	void (*txrx_peer_flush_ast_table)(ol_txrx_soc_handle soc);
	void (*txrx_set_ba_aging_timeout)(struct cdp_soc_t *soc_handle,
					  uint8_t ac, uint32_t value);
	void (*txrx_get_ba_aging_timeout)(struct cdp_soc_t *soc_handle,
					  uint8_t ac, uint32_t *value);

	QDF_STATUS (*txrx_peer_map_attach)(ol_txrx_soc_handle soc,
					   uint32_t num_peers,
					   uint32_t max_ast_index,
					   uint8_t peer_map_unmap_v);

	QDF_STATUS (*set_soc_param)(ol_txrx_soc_handle soc,
				    enum cdp_soc_param_t param,
				    uint32_t value);

	ol_txrx_tx_fp tx_send;

	/* tx_fast_send will be called only in AP mode when all the
	 * transmit path features are disabled including extended stats
	 */
	ol_txrx_tx_fast_fp tx_fast_send;

	void (*set_tx_pause)(ol_txrx_soc_handle soc, bool flag);

	/**
	 * txrx_get_os_rx_handles_from_vdev() - Return function, osif vdev
	 *					to deliver pkt to stack.
	 * @soc: datapath soc handle
	 * @vdev: vdev id
	 * @stack_fn: pointer to - function pointer to deliver RX pkt to stack
	 * @osif_vdev: pointer to - osif vdev to deliver RX packet to.
	 */
	void (*txrx_get_os_rx_handles_from_vdev)
					(ol_txrx_soc_handle soc,
					 uint8_t vdev_id,
					 ol_txrx_rx_fp *stack_fn,
					 ol_osif_vdev_handle *osif_vdev);

	void (*set_rate_stats_ctx)(struct cdp_soc_t *soc,
				   void *ctx);

	int (*txrx_classify_update)
		(struct cdp_soc_t *soc, uint8_t vdev_id, qdf_nbuf_t skb,
		 enum txrx_direction, struct ol_txrx_nbuf_classify *nbuf_class);

	bool (*get_dp_capabilities)(struct cdp_soc_t *soc,
				    enum cdp_capabilities dp_caps);
	void* (*get_rate_stats_ctx)(struct cdp_soc_t *soc);
	QDF_STATUS (*txrx_peer_flush_rate_stats)(struct cdp_soc_t *soc,
						 uint8_t pdev_id,
						 void *buf);
	void* (*txrx_peer_get_peerstats_ctx)(struct cdp_soc_t *soc,
					     uint8_t vdev_id,
					     uint8_t *mac_addr);

	QDF_STATUS (*txrx_flush_rate_stats_request)(struct cdp_soc_t *soc,
						    uint8_t pdev_id);
	QDF_STATUS (*set_pdev_pcp_tid_map)(struct cdp_soc_t *soc,
					   uint8_t pdev_id,
					   uint8_t pcp, uint8_t tid);
	QDF_STATUS (*set_vdev_pcp_tid_map)(struct cdp_soc_t *soc,
					   uint8_t vdev_id,
					   uint8_t pcp, uint8_t tid);
#ifdef DP_RX_UDP_OVER_PEER_ROAM
	QDF_STATUS (*txrx_update_roaming_peer)(struct cdp_soc_t *soc,
					       uint8_t vdev_id,
					       uint8_t *peer_mac,
					       uint32_t auth_status);
#endif

#ifdef QCA_MULTIPASS_SUPPORT
	QDF_STATUS (*set_vlan_groupkey)(struct cdp_soc_t *soc, uint8_t vdev_id,
					uint16_t vlan_id, uint16_t group_key);
#endif

	uint16_t (*get_peer_mac_list)
		 (ol_txrx_soc_handle soc, uint8_t vdev_id,
		  u_int8_t newmac[][QDF_MAC_ADDR_SIZE], uint16_t mac_cnt,
		  bool limit);
	uint16_t (*get_peer_id)(ol_txrx_soc_handle soc,
				uint8_t vdev_id,
				uint8_t *mac);
#ifdef QCA_SUPPORT_WDS_EXTENDED
	QDF_STATUS (*set_wds_ext_peer_rx)(ol_txrx_soc_handle soc,
					  uint8_t vdev_id,
					  uint8_t *mac,
					  ol_txrx_rx_fp rx,
					  ol_osif_peer_handle osif_peer);
#endif /* QCA_SUPPORT_WDS_EXTENDED */
	void (*txrx_drain)(ol_txrx_soc_handle soc);
	int (*get_free_desc_poolsize)(struct cdp_soc_t *soc);
#ifdef WLAN_SYSFS_DP_STATS
	QDF_STATUS (*txrx_sysfs_fill_stats)(ol_txrx_soc_handle soc,
					    char *buf, uint32_t buf_size);
	QDF_STATUS (*txrx_sysfs_set_stat_type)(ol_txrx_soc_handle soc,
					       uint32_t stat_type,
					       uint32_t mac_id);
#endif /* WLAN_SYSFS_DP_STATS */
#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
	void (*set_pkt_capture_mode)(struct cdp_soc_t *soc, bool val);
#endif

#ifdef FEATURE_RUNTIME_PM
	void (*set_rtpm_tput_policy)(struct cdp_soc_t *soc, bool val);
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
	void (*txrx_recovery_vdev_flush_peers)(struct cdp_soc_t *soc,
					       uint8_t vdev_id,
					       bool mlo_peers_only);
#endif
	QDF_STATUS (*txrx_umac_reset_deinit)(ol_txrx_soc_handle soc);
	void (*txrx_get_tsf_time)(struct cdp_soc_t *soc_hdl, uint32_t tsf_id,
				  uint32_t mac_id, uint64_t *tsf,
				  uint64_t *tsf_sync_soc_time);

	void (*txrx_get_tsf2_offset)(struct cdp_soc_t *soc_hdl, uint8_t mac_id,
				     uint64_t *value);
	void (*txrx_get_tqm_offset)(struct cdp_soc_t *soc_hdl, uint64_t *value);
};

struct cdp_ctrl_ops {

	int
		(*txrx_mempools_attach)(ol_txrx_soc_handle dp_soc);
	int
		(*txrx_update_filter_neighbour_peers)(
				struct cdp_soc_t *soc, uint8_t vdev_id,
				uint32_t cmd, uint8_t *macaddr);

	/* Is this similar to ol_txrx_peer_state_update() in MCL */
	/**
	 * @brief Update the authorize peer object at association time
	 * @details
	 * For the host-based implementation of rate-control, it
	 * updates the peer/node-related parameters within rate-control
	 * context of the peer at association.
	 *
	 * @param soc_hdl - pointer to the soc object
	 * @param vdev_id - id of the virtual object
	 * @param peer_mac - mac address of the node's object
	 * @authorize - either to authorize or unauthorize peer
	 *
	 * @return QDF_STATUS
	 */
	QDF_STATUS
		(*txrx_peer_authorize)(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *peer_mac,
				       u_int32_t authorize);
	bool
		(*txrx_peer_get_authorize)(struct cdp_soc_t *soc_hdl,
					   uint8_t vdev_id,
					   uint8_t *peer_mac);

	void (*tx_flush_buffers)(struct cdp_soc_t *soc, uint8_t vdev_id);

	int (*txrx_is_target_ar900b)(struct cdp_soc_t *soc_hdl);

	QDF_STATUS
	(*txrx_set_vdev_param)(struct cdp_soc_t *soc, uint8_t vdev_id,
			       enum cdp_vdev_param_type param,
			       cdp_config_param_type val);

	/**
	 * @brief Set the reo dest ring num of the radio
	 * @details
	 *  Set the reo destination ring no on which we will receive
	 *  pkts for this radio.
	 *
	 * @txrx_soc - soc handle
	 * @param pdev_id - id of physical device
	 * @return the reo destination ring number
	 * @param reo_dest_ring_num - value ranges between 1 - 4
	 */
	QDF_STATUS (*txrx_set_pdev_reo_dest)(
			struct cdp_soc_t *txrx_soc,
			uint8_t pdev_id,
			enum cdp_host_reo_dest_ring reo_dest_ring_num);

	/**
	 * @brief Get the reo dest ring num of the radio
	 * @details
	 *  Get the reo destination ring no on which we will receive
	 *  pkts for this radio.
	 *
	 * @txrx_soc - soc handle
	 * @param pdev_id - id of physical device
	 * @return the reo destination ring number
	 */
	enum cdp_host_reo_dest_ring (*txrx_get_pdev_reo_dest)(
				     struct cdp_soc_t *txrx_soc,
				     uint8_t pdev_id);

	int (*txrx_wdi_event_sub)(struct cdp_soc_t *soc, uint8_t pdev_id,
				  wdi_event_subscribe *event_cb_sub,
				  uint32_t event);

	int (*txrx_wdi_event_unsub)(struct cdp_soc_t *soc, uint8_t pdev_id,
				    wdi_event_subscribe *event_cb_sub,
				    uint32_t event);

	int (*txrx_get_sec_type)(ol_txrx_soc_handle soc, uint8_t vdev_id,
				 uint8_t *peer_mac, uint8_t sec_idx);

	QDF_STATUS
	(*txrx_update_mgmt_txpow_vdev)(struct cdp_soc_t *soc,
				       uint8_t vdev_id,
				       uint8_t subtype, uint8_t tx_power);

	/**
	 * txrx_set_pdev_param() - callback to set pdev parameter
	 * @soc: opaque soc handle
	 * @pdev_id:id of data path pdev handle
	 * @val: value of pdev_tx_capture
	 *
	 * Return: status: 0 - Success, non-zero: Failure
	 */
	QDF_STATUS (*txrx_set_pdev_param)(struct cdp_soc_t *soc,
					  uint8_t pdev_id,
					  enum cdp_pdev_param_type type,
					  cdp_config_param_type val);

	QDF_STATUS (*txrx_get_pdev_param)(struct cdp_soc_t *soc,
					  uint8_t pdev_id,
					  enum cdp_pdev_param_type type,
					  cdp_config_param_type *val);

	QDF_STATUS (*txrx_set_peer_param)(struct cdp_soc_t *soc,
					  uint8_t vdev_id, uint8_t *peer_mac,
					  enum cdp_peer_param_type param,
					  cdp_config_param_type val);

	QDF_STATUS (*txrx_get_peer_param)(struct cdp_soc_t *soc,
					  uint8_t vdev_id, uint8_t *peer_mac,
					  enum cdp_peer_param_type param,
					  cdp_config_param_type *val);

	void * (*txrx_get_pldev)(struct cdp_soc_t *soc, uint8_t pdev_id);
	void (*txrx_peer_flush_frags)(struct cdp_soc_t *soc, uint8_t vdev_id,
				      uint8_t *peer_mac);

#ifdef VDEV_PEER_PROTOCOL_COUNT
	void (*txrx_peer_protocol_cnt)(struct cdp_soc_t *soc,
				       int8_t vdev_id,
				       qdf_nbuf_t nbuf,
				       bool is_egress,
				       bool is_rx);
#endif
#ifdef ATH_SUPPORT_NAC_RSSI
	QDF_STATUS (*txrx_vdev_config_for_nac_rssi)(struct cdp_soc_t *cdp_soc,
						    uint8_t vdev_id,
						    enum cdp_nac_param_cmd cmd,
						    char *bssid,
						    char *client_macaddr,
						    uint8_t chan_num);

	QDF_STATUS (*txrx_vdev_get_neighbour_rssi)(struct cdp_soc_t *cdp_soc,
						   uint8_t vdev_id,
						   char *macaddr,
						   uint8_t *rssi);
#endif

#ifdef WLAN_SUPPORT_MSCS
	QDF_STATUS
		(*txrx_record_mscs_params) (
				struct cdp_soc_t *soc, uint8_t *macaddr,
				uint8_t vdev_id,
				struct cdp_mscs_params *mscs_params,
				bool active);
#endif

	QDF_STATUS
	(*set_key)(struct cdp_soc_t *soc, uint8_t vdev_id, uint8_t *mac,
		   bool is_unicast, uint32_t *key);

	QDF_STATUS (*txrx_get_vdev_param)(struct cdp_soc_t *soc,
					  uint8_t vdev_id,
					  enum cdp_vdev_param_type param,
					  cdp_config_param_type *val);
	int (*enable_peer_based_pktlog)(struct cdp_soc_t *cdp_soc,
					uint8_t pdev_id,
					uint8_t *macaddr, uint8_t enb_dsb);

	QDF_STATUS
	(*calculate_delay_stats)(struct cdp_soc_t *cdp_soc,
				 uint8_t vdev_id, qdf_nbuf_t nbuf);
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
	QDF_STATUS (*txrx_update_pdev_rx_protocol_tag)(
			struct cdp_soc_t  *soc, uint8_t pdev_id,
			uint32_t protocol_mask, uint16_t protocol_type,
			uint16_t tag);
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
	void (*txrx_dump_pdev_rx_protocol_tag_stats)(
				struct cdp_soc_t  *soc, uint8_t pdev_id,
				uint16_t protocol_type);
#endif /* WLAN_SUPPORT_RX_TAG_STATISTICS */
#endif /* WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG */
#ifdef WLAN_SUPPORT_RX_FLOW_TAG
	QDF_STATUS (*txrx_set_rx_flow_tag)(
		struct cdp_soc_t *cdp_soc, uint8_t pdev_id,
		struct cdp_rx_flow_info *flow_info);
	QDF_STATUS (*txrx_dump_rx_flow_tag_stats)(
		struct cdp_soc_t *cdp_soc, uint8_t pdev_id,
		struct cdp_rx_flow_info *flow_info);
#endif /* WLAN_SUPPORT_RX_FLOW_TAG */
#ifdef QCA_MULTIPASS_SUPPORT
	void (*txrx_peer_set_vlan_id)(struct cdp_soc_t *cdp_soc,
				      uint8_t vdev_id, uint8_t *peer_mac,
				      uint16_t vlan_id);
#endif
#if defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(WLAN_RX_PKT_CAPTURE_ENH)
	QDF_STATUS (*txrx_update_peer_pkt_capture_params)(
			ol_txrx_soc_handle soc, uint8_t pdev_id,
			bool is_rx_pkt_cap_enable, uint8_t is_tx_pkt_cap_enable,
			uint8_t *peer_mac);
#endif /* WLAN_TX_PKT_CAPTURE_ENH || WLAN_RX_PKT_CAPTURE_ENH */
	QDF_STATUS
	(*txrx_set_psoc_param)(struct cdp_soc_t *soc,
			       enum cdp_psoc_param_type param,
			       cdp_config_param_type val);

	QDF_STATUS (*txrx_get_psoc_param)(ol_txrx_soc_handle soc,
					  enum cdp_psoc_param_type type,
					  cdp_config_param_type *val);
#ifdef VDEV_PEER_PROTOCOL_COUNT
	/*
	 * Enable per-peer protocol counters
	 */
	void (*txrx_enable_peer_protocol_count)(struct cdp_soc_t *soc,
						int8_t vdev_id, bool enable);
	void (*txrx_set_peer_protocol_drop_mask)(struct cdp_soc_t *soc,
						 int8_t vdev_id, int mask);
	int (*txrx_is_peer_protocol_count_enabled)(struct cdp_soc_t *soc,
						   int8_t vdev_id);
	int (*txrx_get_peer_protocol_drop_mask)(struct cdp_soc_t *soc,
						int8_t vdev_id);

#endif

#if defined(WLAN_FEATURE_TSF_UPLINK_DELAY) || defined(WLAN_CONFIG_TX_DELAY)
	void (*txrx_set_delta_tsf)(struct cdp_soc_t *soc, uint8_t vdev_id,
				   uint32_t delta_tsf);
#endif
#ifdef WLAN_FEATURE_TSF_UPLINK_DELAY
	QDF_STATUS (*txrx_set_tsf_ul_delay_report)(struct cdp_soc_t *soc,
						   uint8_t vdev_id,
						   bool enable);
	QDF_STATUS (*txrx_get_uplink_delay)(struct cdp_soc_t *soc,
					    uint8_t vdev_id,
					    uint32_t *val);
#endif
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	QDF_STATUS (*txrx_set_pdev_phyrx_error_mask)(struct cdp_soc_t *soc,
						     uint8_t pdev_id,
						     uint32_t mask,
						     uint32_t mask_cont);
	QDF_STATUS (*txrx_get_pdev_phyrx_error_mask)(struct cdp_soc_t *soc,
						     uint8_t pdev_id,
						     uint32_t *mask,
						     uint32_t *mask_cont);
#endif
	QDF_STATUS (*txrx_update_mon_mac_filter)(struct cdp_soc_t *soc,
						 uint8_t vdev_id,
						 uint32_t cmd);
};

struct cdp_me_ops {

	void (*tx_me_alloc_descriptor)(struct cdp_soc_t *soc,
				       uint8_t pdev_id);

	void (*tx_me_free_descriptor)(struct cdp_soc_t *soc, uint8_t pdev_id);

	uint16_t (*tx_me_convert_ucast)(struct cdp_soc_t *soc, uint8_t vdev_id,
					qdf_nbuf_t wbuf, u_int8_t newmac[][6],
					uint8_t newmaccnt, uint8_t tid,
					bool is_igmp, bool is_dms_pkt);
};

/**
 * struct cdp_mon_ops - host mon ops
 * @txrx_reset_monitor_mode: Handler to reset monitor mode
 * @txrx_deliver_tx_mgmt: deliver mgmt tx frame
 * @txrx_set_advance_monitor_filter: set advanced monitor mode
 * @config_full_mon_mode: configure full monitor mode
 * @txrx_enable_mon_reap_timer: Enable/Disable reap timer of monitor status ring
 * @txrx_get_lite_mon_legacy_feature_enabled: returns the legacy filter enabled
 */
struct cdp_mon_ops {

	QDF_STATUS (*txrx_reset_monitor_mode)
		(ol_txrx_soc_handle soc, uint8_t pdev_id, u_int8_t smart_monitor);

	QDF_STATUS (*txrx_deliver_tx_mgmt)
		(struct cdp_soc_t *cdp_soc, uint8_t pdev_id, qdf_nbuf_t nbuf);

	/* HK advance monitor filter support */
	QDF_STATUS (*txrx_set_advance_monitor_filter)
		(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		 struct cdp_monitor_filter *filter_val);

	/* Configure full monitor mode */
	QDF_STATUS
		(*config_full_mon_mode)(struct cdp_soc_t *soc, uint8_t val);
	QDF_STATUS (*soc_config_full_mon_mode)(struct cdp_pdev *cdp_pdev,
					       uint8_t val);

	/* Get monitor mode pdev stats */
	QDF_STATUS
		(*get_mon_pdev_rx_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
					 struct cdp_pdev_mon_stats *stats);

	/* Configure monitor status srng reap timer */
	bool (*txrx_enable_mon_reap_timer)(struct cdp_soc_t *soc_hdl,
					   enum cdp_mon_reap_source source,
					   bool enable);

#ifdef QCA_SUPPORT_LITE_MONITOR
	/* set lite monitor config */
	QDF_STATUS
	(*txrx_set_lite_mon_config)(
			struct cdp_soc_t *soc,
			struct cdp_lite_mon_filter_config *mon_config,
			uint8_t pdev_id);

	/* get lite monitor config */
	QDF_STATUS
	(*txrx_get_lite_mon_config)(
			struct cdp_soc_t *soc,
			struct cdp_lite_mon_filter_config *mon_config,
			uint8_t pdev_id);

	/* set lite monitor peer config */
	QDF_STATUS
	(*txrx_set_lite_mon_peer_config)(
			struct cdp_soc_t *soc,
			struct cdp_lite_mon_peer_config *peer_config,
			uint8_t pdev_id);

	/* get lite monitor peer list */
	QDF_STATUS
	(*txrx_get_lite_mon_peer_config)(
			struct cdp_soc_t *soc,
			struct cdp_lite_mon_peer_info *info,
			uint8_t pdev_id);

	/* get lite monitor enable/disable status */
	int
	(*txrx_is_lite_mon_enabled)(struct cdp_soc_t *soc,
				    uint8_t pdev_id,
				    uint8_t direction);

	/* get the legacy feature enabled */
	int
	(*txrx_get_lite_mon_legacy_feature_enabled)(struct cdp_soc_t *soc,
						    uint8_t pdev_id,
						    uint8_t direction);
#endif
	/*To set RSSI dbm conversion params in monitor pdev */
	QDF_STATUS (*txrx_set_mon_pdev_params_rssi_dbm_conv)
		(struct cdp_soc_t *soc,
		 struct cdp_rssi_db2dbm_param_dp *params);
};

struct cdp_host_stats_ops {
	int (*txrx_host_stats_get)(struct cdp_soc_t *soc, uint8_t vdev_id,
				   struct ol_txrx_stats_req *req);

	QDF_STATUS (*txrx_host_stats_clr)(struct cdp_soc_t *soc,
					  uint8_t vdev_id);

	QDF_STATUS
	(*txrx_host_ce_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	int (*txrx_stats_publish)(struct cdp_soc_t *soc, uint8_t pdev_id,
				  struct cdp_stats_extd *buf);
	/**
	 * @brief Enable enhanced stats functionality.
	 *
	 * @param soc - the soc handle
	 * @param pdev_id - pdev_id of pdev
	 * @return - QDF_STATUS
	 */
	QDF_STATUS (*txrx_enable_enhanced_stats)(struct cdp_soc_t *soc,
						 uint8_t pdev_id);

	/**
	 * @brief Disable enhanced stats functionality.
	 *
	 * @param soc - the soc handle
	 * @param pdev_id - pdev_id of pdev
	 * @return - QDF_STATUS
	 */
	QDF_STATUS (*txrx_disable_enhanced_stats)(struct cdp_soc_t *soc,
						  uint8_t pdev_id);

	QDF_STATUS
		(*tx_print_tso_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
		(*tx_rst_tso_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
		(*tx_print_sg_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
		(*tx_rst_sg_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
		(*print_rx_cksum_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
		(*rst_rx_cksum_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
		(*txrx_host_me_stats)(struct cdp_soc_t *soc, uint8_t vdev_id);

	QDF_STATUS
		(*txrx_per_peer_stats)(struct cdp_soc_t *soc, uint8_t *addr);

	int (*txrx_host_msdu_ttl_stats)(struct cdp_soc_t *soc, uint8_t vdev_id,
					struct ol_txrx_stats_req *req);

	int (*ol_txrx_update_peer_stats)(struct cdp_soc_t *soc,
					 uint8_t pdev_id,
					 uint8_t *addr, void *stats,
					 uint32_t last_tx_rate_mcs,
					 uint32_t stats_id);

	QDF_STATUS
		(*get_fw_peer_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
				     uint8_t *addr,
				     uint32_t cap, uint32_t copy_stats);

	QDF_STATUS
		(*get_htt_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
				 void *data,
				 uint32_t data_len);
	QDF_STATUS
		(*txrx_update_pdev_stats)(struct cdp_soc_t *soc,
					  uint8_t pdev_id, void *data,
					  uint16_t stats_id);
	QDF_STATUS
		(*txrx_get_peer_stats_param)(struct cdp_soc_t *soc,
					     uint8_t vdev_id,
					     uint8_t *peer_mac,
					     enum cdp_peer_stats_type type,
					     cdp_peer_stats_param_t *buf);
	QDF_STATUS
		(*txrx_get_peer_stats)(struct cdp_soc_t *soc, uint8_t vdev_id,
				       uint8_t *peer_mac,
				       struct cdp_peer_stats *peer_stats);
	QDF_STATUS
		(*txrx_get_soc_stats)(struct cdp_soc_t *soc,
				      struct cdp_soc_stats *soc_stats);
	QDF_STATUS
		(*txrx_reset_peer_ald_stats)(struct cdp_soc_t *soc,
					     uint8_t vdev_id,
					     uint8_t *peer_mac);
	QDF_STATUS
		(*txrx_reset_peer_stats)(struct cdp_soc_t *soc,
					 uint8_t vdev_id, uint8_t *peer_mac);
	int
		(*txrx_get_vdev_stats)(struct cdp_soc_t *soc, uint8_t vdev_id,
				       void *buf, bool is_aggregate);
	int
		(*txrx_process_wmi_host_vdev_stats)(ol_txrx_soc_handle soc,
						    void *data, uint32_t len,
						    uint32_t stats_id);
	int
		(*txrx_get_vdev_extd_stats)(struct cdp_soc_t *soc,
					    uint8_t vdev_id,
					    wmi_host_vdev_extd_stats *buffer);
	QDF_STATUS
		(*txrx_update_vdev_stats)(struct cdp_soc_t *soc,
					  uint8_t vdev_id, void *buf,
					  uint16_t stats_id);
	int
		(*txrx_get_radio_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
					void *buf);
	QDF_STATUS
		(*txrx_get_pdev_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
				       struct cdp_pdev_stats *buf);
	int
		(*txrx_get_ratekbps)(int preamb, int mcs,
				     int htflag, int gintval);

	QDF_STATUS
	(*txrx_update_peer_stats)(struct cdp_soc_t *soc, uint8_t vdev_id,
				  uint8_t *peer_mac, void *stats,
				  uint32_t last_tx_rate_mcs,
				  uint32_t stats_id);

#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
	QDF_STATUS
	(*txrx_get_scan_spcl_vap_stats)(struct cdp_soc_t *soc, uint8_t vdev_id,
					struct cdp_scan_spcl_vap_stats *stats);
#endif

	QDF_STATUS
	(*txrx_get_peer_delay_stats)(struct cdp_soc_t *soc, uint8_t vdev_id,
				     uint8_t *peer_mac,
				     struct cdp_delay_tid_stats *delay_stats);

	QDF_STATUS
	(*txrx_get_peer_jitter_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
				      uint8_t vdev_id, uint8_t *peer_mac,
				      struct cdp_peer_tid_stats *tid_stats);

	QDF_STATUS
	(*txrx_alloc_vdev_stats_id)(struct cdp_soc_t *soc,
				    uint8_t *vdev_stats_id);

	void (*txrx_reset_vdev_stats_id)(struct cdp_soc_t *soc,
					 uint8_t vdev_stats_id);

#ifdef WLAN_TX_PKT_CAPTURE_ENH
	QDF_STATUS
	(*get_peer_tx_capture_stats)(struct cdp_soc_t *soc, uint8_t vdev_id,
				     uint8_t *peer_mac,
				     struct cdp_peer_tx_capture_stats *stats);

	QDF_STATUS
	(*get_pdev_tx_capture_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
				     struct cdp_pdev_tx_capture_stats *stats);
#endif /* WLAN_TX_PKT_CAPTURE_ENH */
#ifdef HW_TX_DELAY_STATS_ENABLE
	void
	(*enable_disable_vdev_tx_delay_stats)(struct cdp_soc_t *soc,
					      uint8_t vdev_id,
					      uint8_t value);
	uint8_t (*is_tx_delay_stats_enabled)(struct cdp_soc_t *soc_hdl,
					     uint8_t vdev_id);
#endif
	QDF_STATUS
	(*txrx_get_pdev_tid_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
				   struct cdp_tid_stats_intf *tid_stats);
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
	QDF_STATUS
		(*txrx_pdev_telemetry_stats)(
				struct cdp_soc_t *soc,
				uint8_t pdev_id,
				struct cdp_pdev_telemetry_stats *stats);
	QDF_STATUS
		(*txrx_peer_telemetry_stats)(
				struct cdp_soc_t *soc,
				uint8_t *addr,
				struct cdp_peer_telemetry_stats *stats);
#endif
	QDF_STATUS
		(*txrx_get_peer_extd_rate_link_stats)
				(struct cdp_soc_t *soc, uint8_t *mac_addr);
	QDF_STATUS
		(*get_pdev_obss_stats)(struct cdp_soc_t *soc, uint8_t pdev_id,
				       struct cdp_pdev_obss_pd_stats_tlv *buf,
				       struct cdp_txrx_stats_req *req);
	QDF_STATUS (*clear_pdev_obss_pd_stats)(struct cdp_soc_t *soc,
					       uint8_t pdev_id,
					       struct cdp_txrx_stats_req *req);
};

struct cdp_wds_ops {
	QDF_STATUS
	(*txrx_set_wds_rx_policy)(struct cdp_soc_t *soc, uint8_t vdev_id,
				  u_int32_t val);
	QDF_STATUS
	(*txrx_wds_peer_tx_policy_update)(struct cdp_soc_t *soc,
					  uint8_t vdev_id, uint8_t *peer_mac,
					  int wds_tx_ucast, int wds_tx_mcast);
	int (*vdev_set_wds)(struct cdp_soc_t *soc, uint8_t vdev_id,
			    uint32_t val);
};

struct cdp_raw_ops {
	QDF_STATUS
	(*rsim_get_astentry)(struct cdp_soc_t *soc, uint8_t vdev_id,
			     qdf_nbuf_t *pnbuf, struct cdp_raw_ast *raw_ast);
};

#ifdef PEER_FLOW_CONTROL
struct cdp_pflow_ops {
	uint32_t (*pflow_update_pdev_params)(struct cdp_soc_t *soc,
					     uint8_t pdev_id,
					     enum _dp_param_t,
					     uint32_t, void *);
};
#endif /* PEER_FLOW_CONTROL */

#define LRO_IPV4_SEED_ARR_SZ 5
#define LRO_IPV6_SEED_ARR_SZ 11

/**
 * struct cdp_lro_hash_config - set rx_offld(LRO/GRO) init parameters
 * @lro_enable: indicates whether rx_offld is enabled
 * @tcp_flag: If the TCP flags from the packet do not match
 * the values in this field after masking with TCP flags mask
 * below, packet is not rx_offld eligible
 * @tcp_flag_mask: field for comparing the TCP values provided
 * above with the TCP flags field in the received packet
 * @toeplitz_hash_ipv4: contains seed needed to compute the flow id
 * 5-tuple toeplitz hash for ipv4 packets
 * @toeplitz_hash_ipv6: contains seed needed to compute the flow id
 * 5-tuple toeplitz hash for ipv6 packets
 */
struct cdp_lro_hash_config {
	uint32_t lro_enable;
	uint32_t tcp_flag:9,
		tcp_flag_mask:9;
	uint32_t toeplitz_hash_ipv4[LRO_IPV4_SEED_ARR_SZ];
	uint32_t toeplitz_hash_ipv6[LRO_IPV6_SEED_ARR_SZ];
};

struct ol_if_ops {
	void
	(*peer_set_default_routing)(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
				    uint8_t pdev_id, uint8_t *peer_macaddr,
				    uint8_t vdev_id,
				    bool hash_based, uint8_t ring_num,
				    uint8_t lmac_peer_id_msb);
	QDF_STATUS
	(*peer_rx_reorder_queue_setup)(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
				       uint8_t pdev_id,
				       uint8_t vdev_id, uint8_t *peer_mac,
				       qdf_dma_addr_t hw_qdesc, int tid,
				       uint16_t queue_num,
				       uint8_t ba_window_size_valid,
				       uint16_t ba_window_size);
	QDF_STATUS
	(*peer_rx_reorder_queue_remove)(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
					uint8_t pdev_id,
					uint8_t vdev_id, uint8_t *peer_macaddr,
					uint32_t tid_mask);
	int (*peer_unref_delete)(struct cdp_ctrl_objmgr_psoc *psoc,
				 uint8_t pdev_id,
				 uint8_t *peer_mac,
				 uint8_t *vdev_mac, enum wlan_op_mode opmode);
	bool (*is_hw_dbs_capable)(struct wlan_objmgr_psoc *psoc);
	int (*peer_add_wds_entry)(struct cdp_ctrl_objmgr_psoc *soc,
				  uint8_t vdev_id,
				  uint8_t *peer_macaddr,
				  uint16_t peer_id,
				  const uint8_t *dest_macaddr,
				  uint8_t *next_node_mac,
				  uint32_t flags,
				  uint8_t type);
	int (*peer_update_wds_entry)(struct cdp_ctrl_objmgr_psoc *soc,
				     uint8_t vdev_id,
				     uint8_t *dest_macaddr,
				     uint8_t *peer_macaddr,
				     uint32_t flags);
	void (*peer_del_wds_entry)(struct cdp_ctrl_objmgr_psoc *soc,
				   uint8_t vdev_id,
				   uint8_t *wds_macaddr,
				   uint8_t type,
				   uint8_t delete_in_fw);
#ifdef WLAN_FEATURE_MULTI_AST_DEL
	void (*peer_del_multi_wds_entry)(
			struct cdp_ctrl_objmgr_psoc *soc,
			uint8_t vdev_id,
			struct peer_del_multi_wds_entries *wds_list);
#endif
	QDF_STATUS
	(*lro_hash_config)(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t pdev_id,
			   struct cdp_lro_hash_config *rx_offld_hash);

	void (*update_dp_stats)(void *soc, void *stats, uint16_t id,
			uint8_t type);
#ifdef RX_PEER_INVALID_ENH
	uint8_t (*rx_invalid_peer)(struct cdp_ctrl_objmgr_psoc *soc,
				   uint8_t pdev_id, void *msg);
#else
	uint8_t (*rx_invalid_peer)(uint8_t vdev_id, void *wh);
#endif

	int  (*peer_map_event)(struct cdp_ctrl_objmgr_psoc *psoc,
			       uint16_t peer_id, uint16_t hw_peer_id,
			       uint8_t vdev_id, uint8_t *peer_mac_addr,
			       enum cdp_txrx_ast_entry_type peer_type,
			       uint32_t tx_ast_hashidx);

	int (*peer_unmap_event)(struct cdp_ctrl_objmgr_psoc *psoc,
				uint16_t peer_id,
				uint8_t vdev_id, uint8_t *mac_addr);

	int (*get_dp_cfg_param)(struct cdp_ctrl_objmgr_psoc *psoc,
				enum cdp_cfg_param_type param_num);

	void (*rx_mic_error)(struct cdp_ctrl_objmgr_psoc *psoc,
			     uint8_t pdev_id,
			     struct cdp_rx_mic_err_info *info);

	bool (*rx_frag_tkip_demic)(struct cdp_ctrl_objmgr_psoc *psoc,
				   uint8_t vdev_id, uint8_t *peer_mac_addr,
				   qdf_nbuf_t nbuf,
				   uint16_t hdr_space);

	uint8_t (*freq_to_channel)(struct cdp_ctrl_objmgr_psoc *psoc,
				   uint8_t pdev_id, uint16_t freq);

	uint8_t (*freq_to_band)(struct cdp_ctrl_objmgr_psoc *psoc,
				uint8_t pdev_id, uint16_t freq);

	QDF_STATUS(*set_mec_timer)(struct cdp_ctrl_objmgr_psoc *psoc,
				   uint8_t vdev_id, uint16_t mec_timer_val);

#ifdef INTRA_BSS_FWD_OFFLOAD
	void (*vdev_set_intra_bss)(struct cdp_ctrl_objmgr_psoc *psoc,
				   uint16_t vdev_id, bool enable);
#endif
#ifdef ATH_SUPPORT_NAC_RSSI
	int (*config_fw_for_nac_rssi)(struct cdp_ctrl_objmgr_psoc *psoc,
				      uint8_t pdev_id,
				      u_int8_t vdev_id,
				      enum cdp_nac_param_cmd cmd, char *bssid,
				      char *client_macaddr, uint8_t chan_num);

	int
	(*config_bssid_in_fw_for_nac_rssi)(struct cdp_ctrl_objmgr_psoc *psoc,
					   uint8_t pdev_id, u_int8_t vdev_id,
					   enum cdp_nac_param_cmd cmd,
					   char *bssid, char *client_mac);
#endif
	int (*peer_sta_kickout)(struct cdp_ctrl_objmgr_psoc *psoc,
				uint16_t pdev_id, uint8_t *peer_macaddr);

	/**
	 * send_delba() - Send delba to peer
	 * @psoc: Objmgr soc handle
	 * @vdev_id: dp vdev id
	 * @peer_macaddr: Peer mac addr
	 * @tid: Tid number
	 * @reason_code: Reason code
	 * @cdp_rcode: CDP reason code for sending DELBA
	 *
	 * Return: 0 for success, non-zero for failure
	 */
	int (*send_delba)(struct cdp_ctrl_objmgr_psoc *psoc, uint8_t vdev_id,
			  uint8_t *peer_macaddr, uint8_t tid,
			  uint8_t reason_code, uint8_t cdp_rcode);

	int
	(*peer_delete_multiple_wds_entries)(struct cdp_ctrl_objmgr_psoc *psoc,
					    uint8_t vdev_id,
					    uint8_t *dest_macaddr,
					    uint8_t *peer_macaddr,
					    uint32_t flags);
	int
	(*pdev_update_lmac_n_target_pdev_id)(struct cdp_ctrl_objmgr_psoc *psoc,
					    uint8_t *pdev_id,
					    uint8_t *lmac_id,
					    uint8_t *target_pdev_id);
	bool (*is_roam_inprogress)(uint32_t vdev_id);
	enum QDF_GLOBAL_MODE (*get_con_mode)(void);
#ifdef QCA_PEER_MULTIQ_SUPPORT
	int (*peer_ast_flowid_map)(struct cdp_ctrl_objmgr_psoc *ol_soc_handle,
				   uint16_t peer_id, uint8_t vdev_id,
				   uint8_t *peer_mac_addr);
#endif
#ifdef DP_MEM_PRE_ALLOC
	void *(*dp_prealloc_get_context)(uint32_t ctxt_type, size_t ctxt_size);

	QDF_STATUS(*dp_prealloc_put_context)(uint32_t ctxt_type, void *vaddr);
	void *(*dp_prealloc_get_consistent)(uint32_t *size,
					    void **base_vaddr_unaligned,
					    qdf_dma_addr_t *paddr_unaligned,
					    qdf_dma_addr_t *paddr_aligned,
					    uint32_t align,
					    uint32_t ring_type);
	void (*dp_prealloc_put_consistent)(qdf_size_t size,
					   void *vaddr_unligned,
					   qdf_dma_addr_t paddr);
	void (*dp_get_multi_pages)(uint32_t desc_type,
				   size_t element_size,
				   uint16_t element_num,
				   struct qdf_mem_multi_page_t *pages,
				   bool cacheable);
	void (*dp_put_multi_pages)(uint32_t desc_type,
				   struct qdf_mem_multi_page_t *pages);
#endif
	int (*get_soc_nss_cfg)(struct cdp_ctrl_objmgr_psoc *ol_soc_handle);

	char *(*get_device_name)(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
				 uint8_t pdev_id);
	QDF_STATUS(*nss_stats_clr)(struct cdp_ctrl_objmgr_psoc *psoc,
				   uint8_t vdev_id);
	int (*dp_rx_get_pending)(ol_txrx_soc_handle soc);
	void (*dp_rx_sched_refill_thread)(ol_txrx_soc_handle soc);
	/* TODO: Add any other control path calls required to OL_IF/WMA layer */
#ifdef QCA_SUPPORT_WDS_EXTENDED
	void (*rx_wds_ext_peer_learn)(struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
				      uint16_t peer_id, uint8_t vdev_id,
				      uint8_t *peer_macaddr);
#endif /* QCA_SUPPORT_WDS_EXTENDED */
#ifdef WLAN_SUPPORT_MESH_LATENCY
	QDF_STATUS(*peer_update_mesh_latency_params)(
			     struct cdp_ctrl_objmgr_psoc *psoc,
				   uint8_t vdev_id, uint8_t *peer_mac, uint8_t tid,
				   uint32_t service_interval_dl, uint32_t burst_size_dl,
				   uint32_t service_interval_ul, uint32_t burst_size_ul,
				   uint8_t add_or_sub, uint8_t ac);
#endif
#ifdef CONFIG_SAWF
	QDF_STATUS
	(*peer_update_sawf_ul_params)(struct cdp_ctrl_objmgr_psoc *soc,
				      uint8_t vdev_id, uint8_t *peer_mac,
				      uint8_t tid, uint8_t ac,
				      uint32_t service_interval,
				      uint32_t burst_size,
				      uint8_t add_sub);
#endif
	uint32_t (*dp_get_tx_inqueue)(ol_txrx_soc_handle soc);
	QDF_STATUS(*dp_send_unit_test_cmd)(uint32_t vdev_id,
					   uint32_t module_id,
					   uint32_t arg_count, uint32_t *arg);

#ifdef QCA_SUPPORT_LITE_MONITOR
	int (*config_lite_mon_peer)(struct cdp_ctrl_objmgr_psoc *psoc,
				    uint8_t pdev_id,
				    uint8_t vdev_id,
				    enum cdp_nac_param_cmd cmd,
				    uint8_t *peer_mac);
	int (*config_lite_mon_tx_peer)(struct cdp_ctrl_objmgr_psoc *psoc,
				       uint8_t pdev_id, uint8_t vdev_id,
				       enum cdp_tx_filter_action cmd,
				       uint8_t *peer_mac);
#endif
#ifdef WLAN_SUPPORT_SCS
	bool (*peer_scs_rule_match)(struct cdp_ctrl_objmgr_psoc *psoc,
				    uint8_t vdev_id, uint32_t rule_id,
				    uint8_t *peer_mac);
#endif
#ifdef DP_UMAC_HW_RESET_SUPPORT
	void (*dp_update_tx_hardstart)(struct cdp_ctrl_objmgr_psoc *psoc,
				       uint8_t vdev_id,
				       struct ol_txrx_hardtart_ctxt *ctxt);
#endif
#if defined(IPA_WDS_EASYMESH_FEATURE) && defined(FEATURE_AST)
void (*peer_send_wds_disconnect)(struct cdp_ctrl_objmgr_psoc *psoc,
				 uint8_t *mac_addr, uint8_t vdev_id);
#endif
#ifdef WLAN_SUPPORT_PPEDS
	QDF_STATUS
	(*peer_set_ppeds_default_routing)(struct cdp_ctrl_objmgr_psoc *psoc,
					  uint8_t *peer_macaddr,
					  uint16_t service_code,
					  uint8_t priority_valid,
					  uint16_t src_info,
					  uint8_t vdev_id, uint8_t use_ppe,
					  uint8_t routing_enabled);
#endif /* WLAN_SUPPORT_PPEDS */
#ifdef CONFIG_SAWF_DEF_QUEUES
	int (*disable_sawf_svc)(uint8_t svc_id);
#endif
};

#ifdef DP_PEER_EXTENDED_API
/**
 * struct cdp_misc_ops - mcl ops not classified
 * @set_ibss_vdev_heart_beat_timer: Update ibss vdev heart beat timer
 * @set_wmm_param: set wmm parameters
 * @bad_peer_txctl_set_setting: configure bad peer tx limit setting
 * @bad_peer_txctl_update_threshold: configure bad peer tx threshold limit
 * @hl_tdls_flag_reset: reset tdls flag for vdev
 * @tx_non_std: Allow the control-path SW to send data frames
 * @get_vdev_id: get vdev id
 * @set_wisa_mode: set wisa mode for a vdev
 * @txrx_data_stall_cb_register: register data stall callback
 * @txrx_data_stall_cb_deregister: deregister data stall callback
 * @txrx_post_data_stall_event: post data stall event
 * @runtime_suspend: ensure TXRX is ready to runtime suspend
 * @runtime_resume: ensure TXRX is ready to runtime resume
 * @get_opmode: get operation mode of vdev
 * @mark_first_wakeup_packet: set flag to indicate that fw is compatible for
			      marking first packet after wow wakeup
 * @update_mac_id: update mac_id for vdev
 * @flush_rx_frames: flush rx frames on the queue
 * @get_intra_bss_fwd_pkts_count: to get the total tx and rx packets that
				  has been forwarded from txrx layer
				  without going to upper layers
 * @pkt_log_init: handler to initialize packet log
 * @pkt_log_con_service: handler to connect packet log service
 * @get_num_rx_contexts: handler to get number of RX contexts
 * @register_packetdump_cb: register callback for different pktlog
 * @unregister_packetdump_cb: unregister callback for different pktlog
 * @pdev_reset_driver_del_ack: reset driver delayed ack enabled flag
 * @vdev_set_driver_del_ack_enable: set driver delayed ack enabled flag
 *
 * @vdev_inform_ll_conn: inform DP to add/delete a latency critical connection
 *			 for this particular vdev.
 * @set_swlm_enable: Enable or Disable Software Latency Manager.
 * @is_swlm_enabled: Check if Software latency manager is enabled or not.
 * @display_txrx_hw_info: Dump the DP rings info
 * @set_tx_flush_pending: Configures the ac/tid to be flushed and policy
 *			  to flush.
 *
 * set_bus_vote_lvl_high: The bus lvl is set to high or low based on tput
 * get_bus_vote_lvl_high: Get bus lvl to determine whether or not get
 *                        rx rate stats
 * @evaluate_update_tx_ilp_cfg: Evaluate and update DP TX ILP configuration
 *
 * Function pointers for miscellaneous soc/pdev/vdev related operations.
 */
struct cdp_misc_ops {
	uint16_t (*set_ibss_vdev_heart_beat_timer)(struct cdp_soc_t *soc_hdl,
						   uint8_t vdev_id,
						   uint16_t timer_value_sec);
	void (*set_wmm_param)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			      struct ol_tx_wmm_param_t wmm_param);
	void (*bad_peer_txctl_set_setting)(struct cdp_soc_t *soc_hdl,
					   uint8_t pdev_id, int enable,
					   int period, int txq_limit);
	void (*bad_peer_txctl_update_threshold)(struct cdp_soc_t *soc_hdl,
						uint8_t pdev_id,
						int level, int tput_thresh,
						int tx_limit);
	void (*hl_tdls_flag_reset)(struct cdp_soc_t *soc_hdl,
				   uint8_t vdev_id, bool flag);
	qdf_nbuf_t (*tx_non_std)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				 enum ol_tx_spec tx_spec, qdf_nbuf_t msdu_list);
	uint16_t (*get_vdev_id)(struct cdp_vdev *vdev);
	uint32_t (*get_tx_ack_stats)(struct cdp_soc_t *soc_hdl,
				     uint8_t vdev_id);
	QDF_STATUS (*set_wisa_mode)(struct cdp_soc_t *soc_hdl,
				    uint8_t vdev_id, bool enable);
	QDF_STATUS (*txrx_data_stall_cb_register)(struct cdp_soc_t *soc_hdl,
						  uint8_t pdev_id,
						  data_stall_detect_cb cb);
	QDF_STATUS (*txrx_data_stall_cb_deregister)(struct cdp_soc_t *soc_hdl,
						    uint8_t pdev_id,
						    data_stall_detect_cb cb);
	void (*txrx_post_data_stall_event)(
			struct cdp_soc_t *soc_hdl,
			enum data_stall_log_event_indicator indicator,
			enum data_stall_log_event_type data_stall_type,
			uint32_t pdev_id, uint32_t vdev_id_bitmap,
			enum data_stall_log_recovery_type recovery_type);
	QDF_STATUS (*runtime_suspend)(struct cdp_soc_t *soc_hdl,
				      uint8_t pdev_id);
	QDF_STATUS (*runtime_resume)(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id);
	int (*get_opmode)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id);
	void (*mark_first_wakeup_packet)(struct cdp_soc_t *soc_hdl,
					 uint8_t pdev_id, uint8_t value);
	void (*update_mac_id)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			      uint8_t mac_id);
	void (*flush_rx_frames)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				void *peer, bool drop);
	A_STATUS(*get_intra_bss_fwd_pkts_count)(struct cdp_soc_t *soc_hdl,
						uint8_t vdev_id,
						uint64_t *fwd_tx_packets,
						uint64_t *fwd_rx_packets);
	void (*pkt_log_init)(struct cdp_soc_t *soc_hdl, uint8_t pdev,
			     void *scn);
	void (*pkt_log_con_service)(struct cdp_soc_t *soc_hdl,
				    uint8_t pdev_id, void *scn);
	void (*pkt_log_exit)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);
	int (*get_num_rx_contexts)(struct cdp_soc_t *soc_hdl);
	void (*register_pktdump_cb)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				    ol_txrx_pktdump_cb tx_cb,
				    ol_txrx_pktdump_cb rx_cb);
	void (*unregister_pktdump_cb)(struct cdp_soc_t *soc_hdl,
				      uint8_t pdev_id);
	void (*pdev_reset_driver_del_ack)(struct cdp_soc_t *soc_hdl,
					  uint8_t pdev_id);
	void (*vdev_set_driver_del_ack_enable)(struct cdp_soc_t *soc_hdl,
					       uint8_t vdev_id,
					       unsigned long rx_packets,
					       uint32_t time_in_ms,
					       uint32_t high_th,
					       uint32_t low_th);
	void (*vdev_set_bundle_require_flag)(uint8_t vdev_id,
					     unsigned long tx_bytes,
					     uint32_t time_in_ms,
					     uint32_t high_th,
					     uint32_t low_th);
	void (*pdev_reset_bundle_require_flag)(struct cdp_soc_t *soc_hdl,
					       uint8_t pdev_id);
	QDF_STATUS (*txrx_ext_stats_request)(struct cdp_soc_t *soc_hdl,
					     uint8_t pdev_id,
					     struct cdp_txrx_ext_stats *req);
	QDF_STATUS (*request_rx_hw_stats)(struct cdp_soc_t *soc_hdl,
					  uint8_t vdev_id);
	void (*reset_rx_hw_ext_stats)(struct cdp_soc_t *soc_hdl);
	QDF_STATUS (*vdev_inform_ll_conn)(struct cdp_soc_t *soc_hdl,
					  uint8_t vdev_id,
					  enum vdev_ll_conn_actions action);
	QDF_STATUS (*set_swlm_enable)(struct cdp_soc_t *soc_hdl,
				      uint8_t val);
	uint8_t (*is_swlm_enabled)(struct cdp_soc_t *soc_hdl);
	void (*display_txrx_hw_info)(struct cdp_soc_t *soc_hdl);
	uint32_t (*get_tx_rings_grp_bitmap)(struct cdp_soc_t *soc_hdl);
#ifdef WLAN_FEATURE_PEER_TXQ_FLUSH_CONF
	int (*set_peer_txq_flush_config)(struct cdp_soc_t *soc_hdl,
					 uint8_t vdev_id, uint8_t *addr,
					 uint8_t ac, uint32_t tid,
					 enum cdp_peer_txq_flush_policy policy);
#endif
#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
	void (*set_bus_vote_lvl_high)(struct cdp_soc_t *soc_hdl, bool high);
	bool (*get_bus_vote_lvl_high)(struct cdp_soc_t *soc_hdl);
#endif
#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
	bool (*evaluate_update_tx_ilp_cfg)(struct cdp_soc_t *soc_hdl,
					   uint8_t num_msdu_idx_map,
					   uint8_t *msdu_idx_map_arr);
#endif
};

/**
 * struct cdp_ocb_ops - mcl ocb ops
 * @set_ocb_chan_info: set OCB channel info
 * @get_ocb_chan_info: get OCB channel info
 *
 * Function pointers for operations related to OCB.
 */
struct cdp_ocb_ops {
	void (*set_ocb_chan_info)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				  struct ol_txrx_ocb_set_chan ocb_set_chan);
	struct ol_txrx_ocb_chan_info *(*get_ocb_chan_info)(
				struct cdp_soc_t *soc_hdl, uint8_t vdev_id);
};

/**
 * struct cdp_peer_ops - mcl peer related ops
 * @register_peer:
 * @clear_peer:
 * @find_peer_exist
 * @find_peer_exist_on_vdev
 * @find_peer_exist_on_other_vdev
 * @peer_state_update:
 * @get_vdevid:
 * @register_ocb_peer:
 * @peer_get_peer_mac_addr:
 * @get_peer_state:
 * @update_ibss_add_peer_num_of_vdev:
 * @copy_mac_addr_raw:
 * @add_last_real_peer:
 * @is_vdev_restore_last_peer:
 * @update_last_real_peer:
 */
struct cdp_peer_ops {
	QDF_STATUS (*register_peer)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				    struct ol_txrx_desc_type *sta_desc);
	QDF_STATUS (*clear_peer)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				 struct qdf_mac_addr peer_addr);
	bool (*find_peer_exist)(struct cdp_soc_t *soc, uint8_t pdev_id,
				uint8_t *peer_addr);
	bool (*find_peer_exist_on_vdev)(struct cdp_soc_t *soc, uint8_t vdev_id,
					uint8_t *peer_addr);
	bool (*find_peer_exist_on_other_vdev)(struct cdp_soc_t *soc,
					      uint8_t vdev_id,
					      uint8_t *peer_addr,
					      uint16_t max_bssid);
	QDF_STATUS (*peer_state_update)(struct cdp_soc_t *soc,
					uint8_t *peer_addr,
					enum ol_txrx_peer_state state);
	QDF_STATUS (*get_vdevid)(struct cdp_soc_t *soc_hdl, uint8_t *peer_mac,
				 uint8_t *vdev_id);
	struct cdp_vdev * (*get_vdev_by_peer_addr)(struct cdp_pdev *pdev,
			struct qdf_mac_addr peer_addr);
	QDF_STATUS (*register_ocb_peer)(uint8_t *mac_addr);
	uint8_t * (*peer_get_peer_mac_addr)(void *peer);
	int (*get_peer_state)(struct cdp_soc_t *soc, uint8_t vdev_id,
			      uint8_t *peer_mac);
	struct cdp_vdev * (*get_vdev_for_peer)(void *peer);
	int16_t (*update_ibss_add_peer_num_of_vdev)(struct cdp_soc_t *soc,
						    uint8_t vdev_id,
						    int16_t peer_num_delta);
	void (*remove_peers_for_vdev)(struct cdp_vdev *vdev,
			ol_txrx_vdev_peer_remove_cb callback,
			void *callback_context, bool remove_last_peer);
	void (*remove_peers_for_vdev_no_lock)(struct cdp_vdev *vdev,
			ol_txrx_vdev_peer_remove_cb callback,
			void *callback_context);
	void (*copy_mac_addr_raw)(struct cdp_soc_t *soc, uint8_t vdev_id,
				  uint8_t *bss_addr);
	void (*add_last_real_peer)(struct cdp_soc_t *soc, uint8_t pdev_id,
				   uint8_t vdev_id);
	bool (*is_vdev_restore_last_peer)(struct cdp_soc_t *soc,
					  uint8_t vdev_id,
					  uint8_t *peer_mac);
	void (*update_last_real_peer)(struct cdp_soc_t *soc, uint8_t pdev_id,
				      uint8_t vdev_id, bool restore_last_peer);
	void (*peer_detach_force_delete)(struct cdp_soc_t *soc_hdl,
					 uint8_t vdev_id, uint8_t *peer_addr);
	void (*set_tdls_offchan_enabled)(struct cdp_soc_t *soc, uint8_t vdev_id,
					 uint8_t *peer_mac, bool val);
	void (*set_peer_as_tdls_peer)(struct cdp_soc_t *soc, uint8_t vdev_id,
				      uint8_t *peer_mac, bool val);
	void (*peer_flush_frags)(struct cdp_soc_t *soc_hdl,
				 uint8_t vdev_id, uint8_t *peer_mac);
};

/**
 * struct cdp_mob_stats_ops - mcl mob stats ops
 * @clear_stats: handler to clear ol txrx stats
 * @stats: handler to update ol txrx stats
 */
struct cdp_mob_stats_ops {
	QDF_STATUS(*clear_stats)(struct cdp_soc_t *soc_hdl,
				 uint8_t pdev_id, uint8_t bitmap);
	int (*stats)(uint8_t vdev_id, char *buffer, unsigned buf_len);
};

/**
 * struct cdp_pmf_ops - mcl protected management frame ops
 * @get_pn_info: handler to get pn info from peer
 *
 * Function pointers for pmf related operations.
 */
struct cdp_pmf_ops {
	void (*get_pn_info)(struct cdp_soc_t *soc, uint8_t *peer_mac,
			    uint8_t vdev_id, uint8_t **last_pn_valid,
			    uint64_t **last_pn, uint32_t **rmf_pn_replays);
};
#endif


#ifdef DP_FLOW_CTL
/**
 * struct cdp_cfg_ops - mcl configuration ops
 * @set_cfg_rx_fwd_disabled: set rx_fwd_disabled flag
 * @set_cfg_packet_log_enabled: set is_packet_log_enabled flag
 * @cfg_attach: hardcode the configuration parameters
 * @vdev_rx_set_intrabss_fwd: set disable_intrabss_fwd flag
 * @is_rx_fwd_disabled: get the rx_fwd_disabled flag,
 *                      1 enabled, 0 disabled.
 * @tx_set_is_mgmt_over_wmi_enabled: set is_mgmt_over_wmi_enabled flag to
 *                                   indicate that mgmt over wmi is enabled
 *                                   or not,
 *                                   1 for enabled, 0 for disable
 * @is_high_latency: get device is high or low latency device,
 *                   1 high latency bus, 0 low latency bus
 * @set_flow_control_parameters: set flow control parameters
 * @set_flow_steering: set flow_steering_enabled flag
 * @set_ptp_rx_opt_enabled: set is_ptp_rx_opt_enabled flag
 * @set_new_htt_msg_format: set new_htt_msg_format flag
 * @set_peer_unmap_conf_support: set enable_peer_unmap_conf_support flag
 * @get_peer_unmap_conf_support: get enable_peer_unmap_conf_support flag
 * @set_tx_compl_tsf64: set enable_tx_compl_tsf64 flag,
 *                      1 enabled, 0 disabled.
 * @get_tx_compl_tsf64: get enable_tx_compl_tsf64 flag,
 *                      1 enabled, 0 disabled.
 */
struct cdp_cfg_ops {
	void (*set_cfg_rx_fwd_disabled)(struct cdp_cfg *cfg_pdev,
		uint8_t disable_rx_fwd);
	void (*set_cfg_packet_log_enabled)(struct cdp_cfg *cfg_pdev,
		uint8_t val);
	struct cdp_cfg * (*cfg_attach)(qdf_device_t osdev, void *cfg_param);
	void (*vdev_rx_set_intrabss_fwd)(struct cdp_soc_t *soc_hdl,
					 uint8_t vdev_id, bool val);
	uint8_t (*is_rx_fwd_disabled)(struct cdp_vdev *vdev);
	void (*tx_set_is_mgmt_over_wmi_enabled)(uint8_t value);
	int (*is_high_latency)(struct cdp_cfg *cfg_pdev);
	void (*set_flow_control_parameters)(struct cdp_cfg *cfg_pdev,
		void *param);
	void (*set_flow_steering)(struct cdp_cfg *cfg_pdev, uint8_t val);
	void (*set_ptp_rx_opt_enabled)(struct cdp_cfg *cfg_pdev, uint8_t val);
	void (*set_new_htt_msg_format)(uint8_t val);
	void (*set_peer_unmap_conf_support)(bool val);
	bool (*get_peer_unmap_conf_support)(void);
	void (*set_tx_compl_tsf64)(bool val);
	bool (*get_tx_compl_tsf64)(void);
};

/**
 * struct cdp_flowctl_ops - mcl flow control
 * @flow_pool_map_handler: handler to map flow_id and pool descriptors
 * @flow_pool_unmap_handler: handler to unmap flow_id and pool descriptors
 * @register_pause_cb: handler to register tx pause callback
 * @set_desc_global_pool_size: handler to set global pool size
 * @dump_flow_pool_info: handler to dump global and flow pool info
 * @tx_desc_thresh_reached: handler to set tx desc threshold
 *
 * Function pointers for operations related to flow control
 */
struct cdp_flowctl_ops {
	QDF_STATUS (*flow_pool_map_handler)(struct cdp_soc_t *soc,
					    uint8_t pdev_id,
					    uint8_t vdev_id);
	void (*flow_pool_unmap_handler)(struct cdp_soc_t *soc,
					uint8_t pdev_id,
					uint8_t vdev_id);
	QDF_STATUS (*register_pause_cb)(struct cdp_soc_t *soc,
					tx_pause_callback);
	void (*set_desc_global_pool_size)(uint32_t num_msdu_desc);

	void (*dump_flow_pool_info)(struct cdp_soc_t *soc_hdl);

	bool (*tx_desc_thresh_reached)(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id);
};

/**
 * struct cdp_lflowctl_ops - mcl legacy flow control ops
 * @register_tx_flow_control: Register tx flow control callback
 * @set_vdev_tx_desc_limit:  Set tx descriptor limit for a vdev
 * @set_vdev_os_queue_status: Set vdev queue status
 * @deregister_tx_flow_control_cb: Deregister tx flow control callback
 * @flow_control_cb: Call osif flow control callback
 * @get_tx_resource: Get tx resources and compare with watermark
 * @ll_set_tx_pause_q_depth: set pause queue depth
 * @vdev_flush: Flush all packets on a particular vdev
 * @vdev_pause: Pause a particular vdev
 * @vdev_unpause: Unpause a particular vdev
 *
 * Function pointers for operations related to flow control
 */
struct cdp_lflowctl_ops {
#ifdef QCA_HL_NETDEV_FLOW_CONTROL
	int (*register_tx_flow_control)(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id,
					tx_pause_callback flowcontrol);
	int (*set_vdev_tx_desc_limit)(struct cdp_soc_t *soc_hdl,
				      uint8_t vdev_id, uint32_t chan_freq);
	int (*set_vdev_os_queue_status)(struct cdp_soc_t *soc_hdl,
					uint8_t vdev_id,
					enum netif_action_type action);
#else
	int (*register_tx_flow_control)(
		struct cdp_soc_t *soc_hdl,
		uint8_t vdev_id,
		ol_txrx_tx_flow_control_fp flowControl, void *osif_fc_ctx,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause);
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */
	int (*deregister_tx_flow_control_cb)(struct cdp_soc_t *soc_hdl,
					     uint8_t vdev_id);
	void (*flow_control_cb)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				bool tx_resume);
	bool (*get_tx_resource)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				struct qdf_mac_addr peer_addr,
				unsigned int low_watermark,
				unsigned int high_watermark_offset);
	int (*ll_set_tx_pause_q_depth)(struct cdp_soc_t *soc, uint8_t vdev_id,
				       int pause_q_depth);
	void (*vdev_flush)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id);
	void (*vdev_pause)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			   uint32_t reason, uint32_t pause_type);
	void (*vdev_unpause)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			     uint32_t reason, uint32_t pause_type);
};

/**
 * struct cdp_throttle_ops - mcl throttle ops
 * @throttle_init_period: handler to initialize tx throttle time
 * @throttle_set_level: handler to set tx throttle level
 */
struct cdp_throttle_ops {
	void (*throttle_init_period)(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id, int period,
				     uint8_t *dutycycle_level);
	void (*throttle_set_level)(struct cdp_soc_t *soc_hdl,
				   uint8_t pdev_id, int level);
};
#endif

#ifdef IPA_OFFLOAD
/**
 * struct cdp_ipa_ops - mcl ipa data path ops
 * @ipa_get_resource:
 * @ipa_set_doorbell_paddr:
 * @ipa_iounmap_doorbell_vaddr: I/O unmap ipa doorbell vaddr
 * @ipa_set_active:
 * @ipa_op_response:
 * @ipa_register_op_cb:
 * @ipa_get_stat:
 * @ipa_tx_data_frame:
 * @ipa_set_uc_tx_partition_base:
 * @ipa_update_peer_rx_stats:
 * @ipa_uc_get_share_stats:
 * @ipa_uc_set_quota:
 * @ipa_pcie_link_up: Hold PCIe link in L0
 * @ipa_pcie_link_down: Release PCIe link L0 hold
 * @ipa_enable_autonomy:
 * @ipa_disable_autonomy:
 * @ipa_setup:
 * @ipa_cleanup:
 * @ipa_setup_iface:
 * @ipa_cleanup_iface:
 * @ipa_enable_pipes:
 * @ipa_disable_pipes:
 * @ipa_set_perf_level:
 * @ipa_rx_intrabss_fwd:
 * @ipa_tx_buf_smmu_mapping: Create SMMU mappings for Tx
 * @ipa_tx_buf_smmu_unmapping: Release SMMU mappings for Tx
 * buffers to IPA
 * @ipa_rx_super_rule_setup: Setup cce super rules based on filter tuple
 * @ipa_ast_create: Create/Update ast entry
 */
struct cdp_ipa_ops {
	QDF_STATUS (*ipa_get_resource)(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id);
	QDF_STATUS (*ipa_set_doorbell_paddr)(struct cdp_soc_t *soc_hdl,
					     uint8_t pdev_id);
	QDF_STATUS (*ipa_iounmap_doorbell_vaddr)(struct cdp_soc_t *soc_hdl,
						 uint8_t pdev_id);
	QDF_STATUS (*ipa_set_active)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				     bool uc_active, bool is_tx);
	QDF_STATUS (*ipa_op_response)(struct cdp_soc_t *soc_hdl,
				      uint8_t pdev_id, uint8_t *op_msg);
	QDF_STATUS (*ipa_register_op_cb)(struct cdp_soc_t *soc_hdl,
					 uint8_t pdev_id,
					 void (*ipa_uc_op_cb_type)
					 (uint8_t *op_msg, void *osif_ctxt),
					 void *usr_ctxt);
	void (*ipa_deregister_op_cb)(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id);
	QDF_STATUS (*ipa_get_stat)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);
	qdf_nbuf_t (*ipa_tx_data_frame)(struct cdp_soc_t *soc_hdl,
					uint8_t vdev_id, qdf_nbuf_t skb);
	void (*ipa_set_uc_tx_partition_base)(struct cdp_cfg *pdev,
		uint32_t value);
	QDF_STATUS (*ipa_update_peer_rx_stats)(struct cdp_soc_t *soc_hdl,
					       uint8_t vdev_id,
					       uint8_t *peer_mac,
					       qdf_nbuf_t nbuf);
#ifdef FEATURE_METERING
	QDF_STATUS (*ipa_uc_get_share_stats)(struct cdp_soc_t *soc_hdl,
					     uint8_t pdev_id,
					     uint8_t reset_stats);
	QDF_STATUS (*ipa_uc_set_quota)(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id, uint64_t quota_bytes);
#endif
#ifdef IPA_OPT_WIFI_DP
	int (*ipa_pcie_link_up)(struct cdp_soc_t *soc_hdl);
	void (*ipa_pcie_link_down)(struct cdp_soc_t *soc_hdl);
#endif
	QDF_STATUS (*ipa_enable_autonomy)(struct cdp_soc_t *soc_hdl,
					  uint8_t pdev_id);
	QDF_STATUS (*ipa_disable_autonomy)(struct cdp_soc_t *soc_hdl,
					   uint8_t pdev_id);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)
	QDF_STATUS (*ipa_setup)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				void *ipa_i2w_cb, void *ipa_w2i_cb,
				void *ipa_wdi_meter_notifier_cb,
				uint32_t ipa_desc_size, void *ipa_priv,
				bool is_rm_enabled, uint32_t *tx_pipe_handle,
				uint32_t *rx_pipe_handle, bool is_smmu_enabled,
				qdf_ipa_sys_connect_params_t *sys_in,
				bool over_gsi, qdf_ipa_wdi_hdl_t hdl,
				qdf_ipa_wdi_hdl_t id,
				void *ipa_ast_notify_cb);
#else /* CONFIG_IPA_WDI_UNIFIED_API */
	QDF_STATUS (*ipa_setup)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				void *ipa_i2w_cb, void *ipa_w2i_cb,
				void *ipa_wdi_meter_notifier_cb,
				uint32_t ipa_desc_size, void *ipa_priv,
				bool is_rm_enabled, uint32_t *tx_pipe_handle,
				uint32_t *rx_pipe_handle);
#endif /* CONFIG_IPA_WDI_UNIFIED_API */
	QDF_STATUS (*ipa_cleanup)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				  uint32_t tx_pipe_handle,
				  uint32_t rx_pipe_handle,
				  qdf_ipa_wdi_hdl_t hdl);
	QDF_STATUS (*ipa_setup_iface)(char *ifname, uint8_t *mac_addr,
				      qdf_ipa_client_type_t prod_client,
				      qdf_ipa_client_type_t cons_client,
				      uint8_t session_id, bool is_ipv6_enabled,
				      qdf_ipa_wdi_hdl_t hdl);
	QDF_STATUS (*ipa_cleanup_iface)(char *ifname, bool is_ipv6_enabled,
					qdf_ipa_wdi_hdl_t hdl);
	QDF_STATUS (*ipa_enable_pipes)(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id, qdf_ipa_wdi_hdl_t hdl);
	QDF_STATUS (*ipa_disable_pipes)(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id, qdf_ipa_wdi_hdl_t hdl);
	QDF_STATUS (*ipa_set_perf_level)(int client,
					 uint32_t max_supported_bw_mbps,
					 qdf_ipa_wdi_hdl_t hdl);
	bool (*ipa_rx_intrabss_fwd)(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				    qdf_nbuf_t nbuf, bool *fwd_success);
	QDF_STATUS (*ipa_tx_buf_smmu_mapping)(struct cdp_soc_t *soc_hdl,
					      uint8_t pdev_id,
					      const char *func,
					      uint32_t line);
	QDF_STATUS (*ipa_tx_buf_smmu_unmapping)(struct cdp_soc_t *soc_hdl,
						uint8_t pdev_id,
						const char *func,
						uint32_t line);
#ifdef IPA_OPT_WIFI_DP
	QDF_STATUS (*ipa_rx_super_rule_setup)(struct cdp_soc_t *soc_hdl,
					      void *flt_params);
#endif
#ifdef IPA_WDS_EASYMESH_FEATURE
	QDF_STATUS (*ipa_ast_create)(struct cdp_soc_t *soc_hdl,
				     qdf_ipa_ast_info_type_t *data);
#endif
};
#endif

#ifdef DP_POWER_SAVE
/**
 * struct cdp_tx_delay_ops - mcl tx delay ops
 * @tx_delay: handler to get tx packet delay
 * @tx_delay_hist: handler to get tx packet delay histogram
 * @tx_packet_count: handler to get tx packet count
 * @tx_set_compute_interval: update compute interval period for TSM stats
 *
 * Function pointer for operations related to tx delay.
 */
struct cdp_tx_delay_ops {
	void (*tx_delay)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			 uint32_t *queue_delay_microsec,
			 uint32_t *tx_delay_microsec, int category);
	void (*tx_delay_hist)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			      uint16_t *bin_values, int category);
	void (*tx_packet_count)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				uint16_t *out_packet_count,
				uint16_t *out_packet_loss_count, int category);
	void (*tx_set_compute_interval)(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id, uint32_t interval);
};

/**
 * struct cdp_bus_ops - mcl bus suspend/resume ops
 * @bus_suspend: handler for bus suspend
 * @bus_resume: handler for bus resume
 * @process_wow_ack_rsp: handler for wow ack response
 * @process_target_suspend_req: handler for target suspend request
 */
struct cdp_bus_ops {
	QDF_STATUS (*bus_suspend)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);
	QDF_STATUS (*bus_resume)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);
	void (*process_wow_ack_rsp)(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);
	void (*process_target_suspend_req)(struct cdp_soc_t *soc_hdl,
					   uint8_t pdev_id);
};
#endif

#ifdef RECEIVE_OFFLOAD
/**
 * struct cdp_rx_offld_ops - mcl host receive offload ops
 * @register_rx_offld_flush_cb:
 * @deregister_rx_offld_flush_cb:
 */
struct cdp_rx_offld_ops {
	void (*register_rx_offld_flush_cb)(void (rx_offld_flush_cb)(void *));
	void (*deregister_rx_offld_flush_cb)(void);
};
#endif

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
/**
 * struct cdp_cfr_ops - host cfr ops
 * @txrx_cfr_filter: Handler to configure host rx monitor status ring
 * @txrx_get_cfr_rcc: Handler to get CFR mode
 * @txrx_set_cfr_rcc: Handler to enable/disable CFR mode
 * @txrx_get_cfr_dbg_stats: Handler to get debug statistics for CFR mode
 * @txrx_clear_cfr_dbg_stats: Handler to clear debug statistics for CFR mode
 */
struct cdp_cfr_ops {
	void (*txrx_cfr_filter)(struct cdp_soc_t *soc_hdl,
				uint8_t pdev_id,
				bool enable,
				struct cdp_monitor_filter *filter_val,
				bool cfr_enable_monitor_mode);
	bool (*txrx_get_cfr_rcc)(struct cdp_soc_t *soc_hdl,
				 uint8_t pdev_id);
	void (*txrx_set_cfr_rcc)(struct cdp_soc_t *soc_hdl,
				 uint8_t pdev_id,
				 bool enable);
	void (*txrx_get_cfr_dbg_stats)(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id,
				       struct cdp_cfr_rcc_stats *buf);
	void (*txrx_clear_cfr_dbg_stats)(struct cdp_soc_t *soc_hdl,
					 uint8_t pdev_id);
};
#endif

#ifdef WLAN_SUPPORT_MSCS
/**
 * struct cdp_mscs_ops - data path ops for MSCS
 * @mscs_peer_lookup_n_get_priority:
 */
struct cdp_mscs_ops {
	int (*mscs_peer_lookup_n_get_priority)(struct cdp_soc_t *soc,
			uint8_t *src_mac,
			uint8_t *dst_mac,
			qdf_nbuf_t nbuf);
};
#endif

#ifdef WLAN_SUPPORT_MESH_LATENCY
/**
 * struct cdp_mesh_latency_ops - data path ops for Mesh latency
 * @mesh_latency_update_peer_parameter:
 */
struct cdp_mesh_latency_ops {
	QDF_STATUS (*mesh_latency_update_peer_parameter)(
			struct cdp_soc_t *soc,
			uint8_t *dest_mac, uint32_t service_interval_dl,
			uint32_t burst_size_dl, uint32_t service_interval_ul,
			uint32_t burst_size_ul, uint16_t priority,
			uint8_t add_or_sub);
};
#endif

#ifdef WLAN_SUPPORT_SCS
/**
 * struct cdp_scs_ops - data path ops for SCS
 * @scs_peer_lookup_n_rule_match : Handler for peer lookup and scs rule match
 */
struct cdp_scs_ops {
	bool (*scs_peer_lookup_n_rule_match)(struct cdp_soc_t *soc,
					     uint32_t rule_id,
					     uint8_t *dst_mac_addr);
};
#endif

#ifdef CONFIG_SAWF_DEF_QUEUES
struct cdp_sawf_ops {
	QDF_STATUS
	(*sawf_def_queues_map_req)(struct cdp_soc_t *soc, uint8_t *mac_addr,
				   uint8_t svc_class_id);
	QDF_STATUS
	(*sawf_def_queues_unmap_req)(struct cdp_soc_t *soc, uint8_t *mac_addr,
				     uint8_t svc_class_id);
	QDF_STATUS
	(*sawf_def_queues_get_map_report)(struct cdp_soc_t *soc,
					  uint8_t *mac_addr);
#ifdef CONFIG_SAWF
	QDF_STATUS
	(*txrx_get_peer_sawf_delay_stats)(struct cdp_soc_t *soc,
					  uint32_t svc_id, uint8_t *mac,
					  void *data);
	QDF_STATUS
	(*txrx_get_peer_sawf_tx_stats)(struct cdp_soc_t *soc,
				       uint32_t svc_id, uint8_t *mac,
				       void *data);
	QDF_STATUS
	(*sawf_mpdu_stats_req)(struct cdp_soc_t *soc, uint8_t enable);
	QDF_STATUS
	(*sawf_mpdu_details_stats_req)(struct cdp_soc_t *soc, uint8_t enable);
	QDF_STATUS
	(*txrx_sawf_set_mov_avg_params)(uint32_t num_pkt, uint32_t num_win);
	QDF_STATUS
	(*txrx_sawf_set_sla_params)(uint32_t num_pkt, uint32_t time_secs);
	QDF_STATUS
	(*txrx_sawf_init_telemtery_params)(void);
	QDF_STATUS
	(*telemetry_get_throughput_stats)(void *arg, uint64_t *in_bytes,
					  uint64_t *in_cnt, uint64_t *tx_bytes,
					  uint64_t *tx_cnt, uint8_t tid,
					  uint8_t msduq);
	QDF_STATUS
	(*telemetry_get_mpdu_stats)(void *arg, uint64_t *svc_int_pass,
				    uint64_t *svc_int_fail,
				    uint64_t *burst_pass, uint64_t *burst_fail,
				    uint8_t tid, uint8_t msduq);
	QDF_STATUS
	(*telemetry_get_drop_stats)(void *arg, uint64_t *pass, uint64_t *drop,
				    uint64_t *drop_ttl, uint8_t tid,
				    uint8_t msduq);
	QDF_STATUS
	(*peer_config_ul)(struct cdp_soc_t *hdl, uint8_t *mac_addr, uint8_t tid,
			  uint32_t service_interval, uint32_t burst_size,
			  uint8_t add_or_sub);
	bool
	(*swaf_peer_is_sla_configured)(struct cdp_soc_t *soc,
				       uint8_t *mac_addr);

#endif
};
#endif

#ifdef WLAN_SUPPORT_PPEDS
struct cdp_ppeds_txrx_ops {
	QDF_STATUS
	(*ppeds_entry_attach)(struct cdp_soc_t *soc,
			      uint8_t vdev_id, void *vpai,
			      int32_t *ppe_vp_num);
	QDF_STATUS
	(*ppeds_enable_pri2tid)(struct cdp_soc_t *soc,
				uint8_t vdev_id, bool val);
	void (*ppeds_entry_detach)(struct cdp_soc_t *soc,
				   uint8_t vdev_id);
	void (*ppeds_set_int_pri2tid)(struct cdp_soc_t *soc,
				      uint8_t *pri2tid);
	void (*ppeds_update_int_pri2tid)(struct cdp_soc_t *soc,
					 uint8_t pri, uint8_t tid);
	void (*ppeds_entry_dump)(struct cdp_soc_t *soc);
};
#endif /* WLAN_SUPPORT_PPEDS */

struct cdp_ops {
	struct cdp_cmn_ops          *cmn_drv_ops;
	struct cdp_ctrl_ops         *ctrl_ops;
	struct cdp_me_ops           *me_ops;
	struct cdp_mon_ops          *mon_ops;
	struct cdp_host_stats_ops   *host_stats_ops;
	struct cdp_wds_ops          *wds_ops;
	struct cdp_raw_ops          *raw_ops;
	struct cdp_pflow_ops        *pflow_ops;
#ifdef DP_PEER_EXTENDED_API
	struct cdp_misc_ops         *misc_ops;
	struct cdp_peer_ops         *peer_ops;
	struct cdp_ocb_ops          *ocb_ops;
	struct cdp_mob_stats_ops    *mob_stats_ops;
	struct cdp_pmf_ops          *pmf_ops;
#endif
#ifdef DP_FLOW_CTL
	struct cdp_cfg_ops          *cfg_ops;
	struct cdp_flowctl_ops      *flowctl_ops;
	struct cdp_lflowctl_ops     *l_flowctl_ops;
	struct cdp_throttle_ops     *throttle_ops;
#endif
#ifdef DP_POWER_SAVE
	struct cdp_bus_ops          *bus_ops;
	struct cdp_tx_delay_ops     *delay_ops;
#endif
#ifdef IPA_OFFLOAD
	struct cdp_ipa_ops          *ipa_ops;
#endif
#ifdef RECEIVE_OFFLOAD
	struct cdp_rx_offld_ops     *rx_offld_ops;
#endif
#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
	struct cdp_cfr_ops          *cfr_ops;
#endif
#ifdef WLAN_SUPPORT_MSCS
	struct cdp_mscs_ops         *mscs_ops;
#endif
#ifdef WLAN_SUPPORT_MESH_LATENCY
	struct cdp_mesh_latency_ops         *mesh_latency_ops;
#endif
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
	struct cdp_mlo_ops  *mlo_ops;
#endif
#ifdef CONFIG_SAWF_DEF_QUEUES
	struct cdp_sawf_ops  *sawf_ops;
#endif
#ifdef WLAN_SUPPORT_SCS
	struct cdp_scs_ops   *scs_ops;
#endif
#ifdef WLAN_SUPPORT_PPEDS
	struct cdp_ppeds_txrx_ops *ppeds_ops;
#endif
};
#endif
