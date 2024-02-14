/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
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

#ifndef _DP_IPA_H_
#define _DP_IPA_H_

#if defined(QCA_WIFI_KIWI) || defined(QCA_WIFI_KIWI_V2)
/* Index into soc->tcl_data_ring[] */
#define IPA_TCL_DATA_RING_IDX	3
#else
#define IPA_TCL_DATA_RING_IDX	2
#endif
/* Index into soc->tx_comp_ring[] */
#define IPA_TX_COMP_RING_IDX IPA_TCL_DATA_RING_IDX

#ifdef IPA_OFFLOAD

#define DP_IPA_MAX_IFACE	3
#define IPA_REO_DEST_RING_IDX	3
#define IPA_REO_DEST_RING_IDX_2	7

#define IPA_RX_REFILL_BUF_RING_IDX	2

#define IPA_ALT_REO_DEST_RING_IDX	2
#define IPA_RX_ALT_REFILL_BUF_RING_IDX	3

/* Adding delay before disabling ipa pipes if any Tx Completions are pending */
#define TX_COMP_DRAIN_WAIT_MS	50
#define TX_COMP_DRAIN_WAIT_TIMEOUT_MS	100

#ifdef IPA_WDI3_TX_TWO_PIPES
#if defined(QCA_WIFI_KIWI) || defined(QCA_WIFI_KIWI_V2)
/* Index into soc->tcl_data_ring[] and soc->tx_comp_ring[] */
#define IPA_TX_ALT_RING_IDX 4
#define IPA_TX_ALT_COMP_RING_IDX IPA_TX_ALT_RING_IDX
#else /* !KIWI */
#define IPA_TX_ALT_RING_IDX 1
/*
 * must be same as IPA_TX_ALT_RING_IDX as tcl and wbm ring
 * are initialized with same index as a pair.
 */
#define IPA_TX_ALT_COMP_RING_IDX 1
#endif /* KIWI */

#define IPA_SESSION_ID_SHIFT 1
#endif /* IPA_WDI3_TX_TWO_PIPES */

/**
 * struct dp_ipa_uc_tx_hdr - full tx header registered to IPA hardware
 * @eth:     ether II header
 */
struct dp_ipa_uc_tx_hdr {
	struct ethhdr eth;
} __packed;

/**
 * struct dp_ipa_uc_tx_hdr - full tx header registered to IPA hardware
 * @eth:     ether II header
 */
struct dp_ipa_uc_tx_vlan_hdr {
	struct vlan_ethhdr eth;
} __packed;

/**
 * struct dp_ipa_uc_rx_hdr - full rx header registered to IPA hardware
 * @eth:     ether II header
 */
struct dp_ipa_uc_rx_hdr {
	struct ethhdr eth;
} __packed;

#define DP_IPA_UC_WLAN_TX_HDR_LEN      sizeof(struct dp_ipa_uc_tx_hdr)
#define DP_IPA_UC_WLAN_TX_VLAN_HDR_LEN sizeof(struct dp_ipa_uc_tx_vlan_hdr)
#define DP_IPA_UC_WLAN_RX_HDR_LEN      sizeof(struct dp_ipa_uc_rx_hdr)
/* 28 <bytes of rx_msdu_end_tlv> + 16 <bytes of attn tlv> +
 * 52 <bytes of rx_mpdu_start_tlv> + <L2 Header>
 */
#define DP_IPA_UC_WLAN_RX_HDR_LEN_AST  110
#define DP_IPA_UC_WLAN_RX_HDR_LEN_AST_VLAN 114
#define DP_IPA_UC_WLAN_HDR_DES_MAC_OFFSET	0

#define DP_IPA_HDL_INVALID	0xFF
#define DP_IPA_HDL_FIRST	0
#define DP_IPA_HDL_SECOND	1
/**
 * wlan_ipa_get_hdl() - Get ipa handle from IPA component
 * @psoc - control psoc object
 * @pdev_id - pdev id
 *
 * IPA componenet will return the IPA handle based on pdev_id
 *
 * Return: IPA handle
 */
qdf_ipa_wdi_hdl_t wlan_ipa_get_hdl(void *psoc, uint8_t pdev_id);

/**
 * dp_ipa_get_resource() - Client request resource information
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 *
 *  IPA client will request IPA UC related resource information
 *  Resource information will be distributed to IPA module
 *  All of the required resources should be pre-allocated
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_get_resource(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

/**
 * dp_ipa_set_doorbell_paddr () - Set doorbell register physical address to SRNG
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 *
 * Set TX_COMP_DOORBELL register physical address to WBM Head_Ptr_MemAddr_LSB
 * Set RX_READ_DOORBELL register physical address to REO Head_Ptr_MemAddr_LSB
 *
 * Return: none
 */
QDF_STATUS dp_ipa_set_doorbell_paddr(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id);

/**
 * dp_ipa_iounmap_doorbell_vaddr() - unmap ipa RX db vaddr
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 *
 * Return: none
 */
QDF_STATUS dp_ipa_iounmap_doorbell_vaddr(struct cdp_soc_t *soc_hdl,
					 uint8_t pdev_id);

/**
 * dp_ipa_op_response() - Handle OP command response from firmware
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 * @op_msg: op response message from firmware
 *
 * Return: none
 */
QDF_STATUS dp_ipa_op_response(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			      uint8_t *op_msg);

/**
 * dp_ipa_register_op_cb() - Register OP handler function
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 * @op_cb: handler function pointer
 *
 * Return: none
 */
QDF_STATUS dp_ipa_register_op_cb(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				 ipa_uc_op_cb_type op_cb, void *usr_ctxt);

/**
 * dp_ipa_register_op_cb() - Deregister OP handler function
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 *
 * Return: none
 */
void dp_ipa_deregister_op_cb(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

/**
 * dp_ipa_get_stat() - Get firmware wdi status
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 *
 * Return: none
 */
QDF_STATUS dp_ipa_get_stat(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

/**
 * dp_tx_send_ipa_data_frame() - send IPA data frame
 * @soc_hdl: datapath soc handle
 * @vdev_id: virtual device/interface id
 * @skb: skb
 *
 * Return: skb/ NULL is for success
 */
qdf_nbuf_t dp_tx_send_ipa_data_frame(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				     qdf_nbuf_t skb);

/**
 * dp_ipa_enable_autonomy() – Enable autonomy RX path
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 *
 * Set all RX packet route to IPA REO ring
 * Program Destination_Ring_Ctrl_IX_0 REO register to point IPA REO ring
 * Return: none
 */
QDF_STATUS dp_ipa_enable_autonomy(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

/**
 * dp_ipa_disable_autonomy() – Disable autonomy RX path
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 *
 * Disable RX packet routing to IPA REO
 * Program Destination_Ring_Ctrl_IX_0 REO register to disable
 * Return: none
 */
QDF_STATUS dp_ipa_disable_autonomy(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)
/**
 * dp_ipa_setup() - Setup and connect IPA pipes
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 * @ipa_i2w_cb: IPA to WLAN callback
 * @ipa_w2i_cb: WLAN to IPA callback
 * @ipa_wdi_meter_notifier_cb: IPA WDI metering callback
 * @ipa_desc_size: IPA descriptor size
 * @ipa_priv: handle to the HTT instance
 * @is_rm_enabled: Is IPA RM enabled or not
 * @tx_pipe_handle: pointer to Tx pipe handle
 * @rx_pipe_handle: pointer to Rx pipe handle
 * @is_smmu_enabled: Is SMMU enabled or not
 * @sys_in: parameters to setup sys pipe in mcc mode
 * @hdl: IPA handle
 * @id: IPA instance id
 * @ipa_ast_notify_cb: IPA to WLAN callback for ast create and update
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_setup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			void *ipa_i2w_cb, void *ipa_w2i_cb,
			void *ipa_wdi_meter_notifier_cb,
			uint32_t ipa_desc_size, void *ipa_priv,
			bool is_rm_enabled, uint32_t *tx_pipe_handle,
			uint32_t *rx_pipe_handle,
			bool is_smmu_enabled,
			qdf_ipa_sys_connect_params_t *sys_in, bool over_gsi,
			qdf_ipa_wdi_hdl_t hdl, qdf_ipa_wdi_hdl_t id,
			void *ipa_ast_notify_cb);
#else /* CONFIG_IPA_WDI_UNIFIED_API */
/**
 * dp_ipa_setup() - Setup and connect IPA pipes
 * @soc_hdl - data path soc handle
 * @pdev_id - device instance id
 * @ipa_i2w_cb: IPA to WLAN callback
 * @ipa_w2i_cb: WLAN to IPA callback
 * @ipa_wdi_meter_notifier_cb: IPA WDI metering callback
 * @ipa_desc_size: IPA descriptor size
 * @ipa_priv: handle to the HTT instance
 * @is_rm_enabled: Is IPA RM enabled or not
 * @tx_pipe_handle: pointer to Tx pipe handle
 * @rx_pipe_handle: pointer to Rx pipe handle
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_setup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			void *ipa_i2w_cb, void *ipa_w2i_cb,
			void *ipa_wdi_meter_notifier_cb,
			uint32_t ipa_desc_size, void *ipa_priv,
			bool is_rm_enabled, uint32_t *tx_pipe_handle,
			uint32_t *rx_pipe_handle);
#endif /* CONFIG_IPA_WDI_UNIFIED_API */
QDF_STATUS dp_ipa_cleanup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			  uint32_t tx_pipe_handle,
			  uint32_t rx_pipe_handle, qdf_ipa_wdi_hdl_t hdl);
QDF_STATUS dp_ipa_setup_iface(char *ifname, uint8_t *mac_addr,
		qdf_ipa_client_type_t prod_client,
		qdf_ipa_client_type_t cons_client,
		uint8_t session_id, bool is_ipv6_enabled,
		qdf_ipa_wdi_hdl_t hdl);
QDF_STATUS dp_ipa_cleanup_iface(char *ifname, bool is_ipv6_enabled,
				qdf_ipa_wdi_hdl_t hdl);

/**
 * dp_ipa_uc_enable_pipes() - Enable and resume traffic on Tx/Rx pipes
 * @soc_hdl - handle to the soc
 * @pdev_id - pdev id number, to get the handle
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_enable_pipes(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			       qdf_ipa_wdi_hdl_t hdl);

/**
 * dp_ipa_disable_pipes() – Suspend traffic and disable Tx/Rx pipes
 * @soc_hdl - handle to the soc
 * @pdev_id - pdev id number, to get the handle
 * @hdl: IPA handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_disable_pipes(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				qdf_ipa_wdi_hdl_t hdl);
QDF_STATUS dp_ipa_set_perf_level(int client, uint32_t max_supported_bw_mbps,
				 qdf_ipa_wdi_hdl_t hdl);
#ifdef IPA_OPT_WIFI_DP
QDF_STATUS dp_ipa_rx_super_rule_setup(struct cdp_soc_t *soc_hdl,
				      void *flt_params);
int dp_ipa_pcie_link_up(struct cdp_soc_t *soc_hdl);
void dp_ipa_pcie_link_down(struct cdp_soc_t *soc_hdl);
#endif

/**
 * dp_ipa_rx_intrabss_fwd() - Perform intra-bss fwd for IPA RX path
 *
 * @soc_hdl: data path soc handle
 * @vdev_id: virtual device/interface id
 * @nbuf: pointer to skb of ethernet packet received from IPA RX path
 * @fwd_success: pointer to indicate if skb succeeded in intra-bss TX
 *
 * This function performs intra-bss forwarding for WDI 3.0 IPA RX path.
 *
 * Return: true if packet is intra-bss fwd-ed and no need to pass to
 *	   network stack. false if packet needs to be passed to network stack.
 */
bool dp_ipa_rx_intrabss_fwd(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			    qdf_nbuf_t nbuf, bool *fwd_success);
int dp_ipa_uc_detach(struct dp_soc *soc, struct dp_pdev *pdev);
int dp_ipa_uc_attach(struct dp_soc *soc, struct dp_pdev *pdev);
int dp_ipa_ring_resource_setup(struct dp_soc *soc,
		struct dp_pdev *pdev);
bool dp_reo_remap_config(struct dp_soc *soc, uint32_t *remap0,
			 uint32_t *remap1, uint32_t *remap2);
bool dp_ipa_is_mdm_platform(void);

qdf_nbuf_t dp_ipa_handle_rx_reo_reinject(struct dp_soc *soc, qdf_nbuf_t nbuf);

QDF_STATUS dp_ipa_handle_rx_buf_smmu_mapping(struct dp_soc *soc,
					     qdf_nbuf_t nbuf,
					     uint32_t size,
					     bool create,
					     const char *func,
					     uint32_t line);
/**
 * dp_ipa_tx_buf_smmu_mapping() - Create SMMU mappings for IPA
 *				  allocated TX buffers
 * @soc_hdl: handle to the soc
 * @pdev_id: pdev id number, to get the handle
 * @func: caller function
 * @line: line number
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_tx_buf_smmu_mapping(struct cdp_soc_t *soc_hdl,
				      uint8_t pdev_id, const char *func,
				      uint32_t line);

/**
 * dp_ipa_tx_buf_smmu_unmapping() - Release SMMU mappings for IPA
 *				    allocated TX buffers
 * @soc_hdl: handle to the soc
 * @pdev_id: pdev id number, to get the handle
 * @func: caller function
 * @line: line number
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_tx_buf_smmu_unmapping(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id, const char *func,
					uint32_t line);
QDF_STATUS dp_ipa_rx_buf_pool_smmu_mapping(struct cdp_soc_t *soc_hdl,
					   uint8_t pdev_id,
					   bool create,
					   const char *func,
					   uint32_t line);
QDF_STATUS dp_ipa_set_smmu_mapped(struct cdp_soc_t *soc, int val);
int dp_ipa_get_smmu_mapped(struct cdp_soc_t *soc);

#ifndef QCA_OL_DP_SRNG_LOCK_LESS_ACCESS
static inline void
dp_ipa_rx_buf_smmu_mapping_lock(struct dp_soc *soc)
{
	if (soc->ipa_rx_buf_map_lock_initialized)
		qdf_spin_lock_bh(&soc->ipa_rx_buf_map_lock);
}

static inline void
dp_ipa_rx_buf_smmu_mapping_unlock(struct dp_soc *soc)
{
	if (soc->ipa_rx_buf_map_lock_initialized)
		qdf_spin_unlock_bh(&soc->ipa_rx_buf_map_lock);
}

static inline void
dp_ipa_reo_ctx_buf_mapping_lock(struct dp_soc *soc,
				uint32_t reo_ring_num)
{
	if (!soc->ipa_reo_ctx_lock_required[reo_ring_num])
		return;

	qdf_spin_lock_bh(&soc->ipa_rx_buf_map_lock);
}

static inline void
dp_ipa_reo_ctx_buf_mapping_unlock(struct dp_soc *soc,
				  uint32_t reo_ring_num)
{
	if (!soc->ipa_reo_ctx_lock_required[reo_ring_num])
		return;

	qdf_spin_unlock_bh(&soc->ipa_rx_buf_map_lock);
}
#else

static inline void
dp_ipa_rx_buf_smmu_mapping_lock(struct dp_soc *soc)
{
}

static inline void
dp_ipa_rx_buf_smmu_mapping_unlock(struct dp_soc *soc)
{
}

static inline void
dp_ipa_reo_ctx_buf_mapping_lock(struct dp_soc *soc,
				uint32_t reo_ring_num)
{
}

static inline void
dp_ipa_reo_ctx_buf_mapping_unlock(struct dp_soc *soc,
				  uint32_t reo_ring_num)
{
}
#endif

#ifdef IPA_WDS_EASYMESH_FEATURE
/**
 * dp_ipa_ast_create() - Create/update AST entry in AST table
 *			 for learning/roaming packets from IPA
 * @soc: data path soc handle
 * @data: Structure used for updating the AST table
 *
 * Create/update AST entry in AST table for learning/roaming packets from IPA
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_ast_create(struct cdp_soc_t *soc_hdl,
			     qdf_ipa_ast_info_type_t *data);

/**
 * dp_ipa_ast_notify_cb() - Provide ast notify cb to IPA
 * @pipe_in: WDI conn pipe in params
 * @ipa_ast_notify_cb: ipa ast notify cb
 *
 * Return: None
 */
static inline void
dp_ipa_ast_notify_cb(qdf_ipa_wdi_conn_in_params_t *pipe_in,
		     void *ipa_ast_notify_cb)
{
	QDF_IPA_WDI_CONN_IN_PARAMS_AST_NOTIFY(pipe_in) = ipa_ast_notify_cb;
}
#else
static inline void
dp_ipa_ast_notify_cb(qdf_ipa_wdi_conn_in_params_t *pipe_in,
		     void *ipa_ast_notify_cb)
{
}
#endif

#ifdef QCA_ENHANCED_STATS_SUPPORT
QDF_STATUS dp_ipa_txrx_get_peer_stats(struct cdp_soc_t *soc, uint8_t vdev_id,
				      uint8_t *peer_mac,
				      struct cdp_peer_stats *peer_stats);
int dp_ipa_txrx_get_vdev_stats(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			       void *buf, bool is_aggregate);
QDF_STATUS dp_ipa_txrx_get_pdev_stats(struct cdp_soc_t *soc, uint8_t pdev_id,
				      struct cdp_pdev_stats *pdev_stats);
QDF_STATUS dp_ipa_update_peer_rx_stats(struct cdp_soc_t *soc, uint8_t pdev_id,
				       uint8_t *peer_mac, qdf_nbuf_t nbuf);
#endif
#ifdef IPA_OPT_WIFI_DP
static inline void dp_ipa_opt_dp_ixo_remap(uint8_t *ix0_map)
{
	ix0_map[0] = REO_REMAP_SW1;
	ix0_map[1] = REO_REMAP_SW1;
	ix0_map[2] = REO_REMAP_SW2;
	ix0_map[3] = REO_REMAP_SW3;
	ix0_map[4] = REO_REMAP_SW4;
	ix0_map[5] = REO_REMAP_RELEASE;
	ix0_map[6] = REO_REMAP_FW;
	ix0_map[7] = REO_REMAP_FW;
}
#else
static inline void dp_ipa_opt_dp_ixo_remap(uint8_t *ix0_map)
{
}
#endif

#else
static inline int dp_ipa_uc_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline int dp_ipa_uc_attach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline int dp_ipa_ring_resource_setup(struct dp_soc *soc,
					     struct dp_pdev *pdev)
{
	return 0;
}

static inline QDF_STATUS dp_ipa_handle_rx_buf_smmu_mapping(struct dp_soc *soc,
							   qdf_nbuf_t nbuf,
							   uint32_t size,
							   bool create,
							   const char *func,
							   uint32_t line)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_ipa_rx_buf_smmu_mapping_lock(struct dp_soc *soc)
{
}

static inline void
dp_ipa_rx_buf_smmu_mapping_unlock(struct dp_soc *soc)
{
}

static inline void
dp_ipa_reo_ctx_buf_mapping_lock(struct dp_soc *soc,
				uint32_t reo_ring_num)
{
}

static inline void
dp_ipa_reo_ctx_buf_mapping_unlock(struct dp_soc *soc,
				  uint32_t reo_ring_num)
{
}

static inline qdf_nbuf_t dp_ipa_handle_rx_reo_reinject(struct dp_soc *soc,
						       qdf_nbuf_t nbuf)
{
	return nbuf;
}

static inline QDF_STATUS dp_ipa_tx_buf_smmu_mapping(struct cdp_soc_t *soc_hdl,
						    uint8_t pdev_id,
						    const char *func,
						    uint32_t line)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_ipa_tx_buf_smmu_unmapping(struct cdp_soc_t *soc_hdl,
						      uint8_t pdev_id,
						      const char *func,
						      uint32_t line)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_ipa_rx_buf_pool_smmu_mapping(
						      struct cdp_soc_t *soc_hdl,
						      uint8_t pdev_id,
						      bool create,
						      const char *func,
						      uint32_t line)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_ipa_set_smmu_mapped(struct cdp_soc_t *soc, int val)
{
	return QDF_STATUS_SUCCESS;
}

static inline int dp_ipa_get_smmu_mapped(struct cdp_soc_t *soc)
{
	return QDF_STATUS_SUCCESS;
}

#ifdef IPA_WDS_EASYMESH_FEATURE
static inline QDF_STATUS dp_ipa_ast_create(struct cdp_soc_t *soc_hdl,
					   qdf_ipa_ast_info_type_t *data)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#endif
#endif /* _DP_IPA_H_ */
