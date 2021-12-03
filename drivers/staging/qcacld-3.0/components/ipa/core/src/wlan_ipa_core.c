/*
 * Copyright (c) 2013-2018, 2021 The Linux Foundation. All rights reserved.
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

/* Include Files */
#include "wlan_ipa_core.h"
#include "wlan_ipa_main.h"
#include <ol_txrx.h>
#include "cdp_txrx_ipa.h"
#include "wal_rx_desc.h"
#include "qdf_str.h"

static struct wlan_ipa_priv *gp_ipa;

static struct wlan_ipa_iface_2_client {
	qdf_ipa_client_type_t cons_client;
	qdf_ipa_client_type_t prod_client;
} wlan_ipa_iface_2_client[WLAN_IPA_MAX_IFACE] = {
	{
		QDF_IPA_CLIENT_WLAN2_CONS, QDF_IPA_CLIENT_WLAN1_PROD
	}, {
		QDF_IPA_CLIENT_WLAN3_CONS, QDF_IPA_CLIENT_WLAN1_PROD
	}, {
		QDF_IPA_CLIENT_WLAN4_CONS, QDF_IPA_CLIENT_WLAN1_PROD
	}
};

/* Local Function Prototypes */
static void wlan_ipa_i2w_cb(void *priv, qdf_ipa_dp_evt_type_t evt,
			    unsigned long data);
static void wlan_ipa_w2i_cb(void *priv, qdf_ipa_dp_evt_type_t evt,
			    unsigned long data);

/**
 * wlan_ipa_uc_sta_is_enabled() - Is STA mode IPA uC offload enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if STA mode IPA uC offload is enabled, false otherwise
 */
static inline bool wlan_ipa_uc_sta_is_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg, WLAN_IPA_UC_STA_ENABLE_MASK);
}

/**
 * wlan_ipa_is_pre_filter_enabled() - Is IPA pre-filter enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if pre-filter is enabled, otherwise false
 */
static inline
bool wlan_ipa_is_pre_filter_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg,
					 WLAN_IPA_PRE_FILTER_ENABLE_MASK);
}

/**
 * wlan_ipa_is_ipv6_enabled() - Is IPA IPv6 enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if IPv6 is enabled, otherwise false
 */
static inline bool wlan_ipa_is_ipv6_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg, WLAN_IPA_IPV6_ENABLE_MASK);
}

/**
 * wlan_ipa_msg_free_fn() - Free an IPA message
 * @buff: pointer to the IPA message
 * @len: length of the IPA message
 * @type: type of IPA message
 *
 * Return: None
 */
static void wlan_ipa_msg_free_fn(void *buff, uint32_t len, uint32_t type)
{
	ipa_debug("msg type:%d, len:%d", type, len);
	qdf_mem_free(buff);
}

/**
 * wlan_ipa_uc_loaded_uc_cb() - IPA UC loaded event callback
 * @priv_ctxt: IPA context
 *
 * Will be called by IPA context.
 * It's atomic context, then should be scheduled to kworker thread
 *
 * Return: None
 */
static void wlan_ipa_uc_loaded_uc_cb(void *priv_ctxt)
{
	struct wlan_ipa_priv *ipa_ctx;
	struct op_msg_type *msg;
	struct uc_op_work_struct *uc_op_work;

	if (!priv_ctxt) {
		ipa_err("Invalid IPA context");
		return;
	}

	ipa_ctx = priv_ctxt;
	ipa_ctx->uc_loaded = true;

	uc_op_work = &ipa_ctx->uc_op_work[WLAN_IPA_UC_OPCODE_UC_READY];
	if (!list_empty(&uc_op_work->work.work.entry)) {
		/* uc_op_work is not initialized yet */
		return;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg) {
		ipa_err("op_msg allocation fails");
		return;
	}

	msg->op_code = WLAN_IPA_UC_OPCODE_UC_READY;

	/* When the same uC OPCODE is already pended, just return */
	if (uc_op_work->msg)
		goto done;

	uc_op_work->msg = msg;
	qdf_sched_work(0, &uc_op_work->work);

	/* work handler will free the msg buffer */
	return;

done:
	qdf_mem_free(msg);
}

/**
 * wlan_ipa_uc_send_wdi_control_msg() - Set WDI control message
 * @ctrl: WDI control value
 *
 * Send WLAN_WDI_ENABLE for ctrl = true and WLAN_WDI_DISABLE otherwise.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_ipa_uc_send_wdi_control_msg(bool ctrl)
{
	struct wlan_ipa_priv *ipa_ctx = gp_ipa;
	qdf_ipa_msg_meta_t meta;
	qdf_ipa_wlan_msg_t *ipa_msg;
	int ret = 0;

	/* WDI enable message to IPA */
	QDF_IPA_MSG_META_MSG_LEN(&meta) = sizeof(*ipa_msg);
	ipa_msg = qdf_mem_malloc(QDF_IPA_MSG_META_MSG_LEN(&meta));
	if (!ipa_msg) {
		ipa_err("msg allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	if (ctrl) {
		QDF_IPA_SET_META_MSG_TYPE(&meta, QDF_WDI_ENABLE);
		ipa_ctx->stats.event[QDF_WDI_ENABLE]++;
	} else {
		QDF_IPA_SET_META_MSG_TYPE(&meta, QDF_WDI_DISABLE);
		ipa_ctx->stats.event[QDF_WDI_DISABLE]++;
	}

	ipa_debug("ipa_send_msg(Evt:%d)", QDF_IPA_MSG_META_MSG_TYPE(&meta));
	ret = qdf_ipa_send_msg(&meta, ipa_msg, wlan_ipa_msg_free_fn);
	if (ret) {
		ipa_err("ipa_send_msg(Evt:%d)-fail=%d",
			QDF_IPA_MSG_META_MSG_TYPE(&meta), ret);
		qdf_mem_free(ipa_msg);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

struct wlan_ipa_priv *wlan_ipa_get_obj_context(void)
{
	return gp_ipa;
}

/**
 * wlan_ipa_send_pkt_to_tl() - Send an IPA packet to TL
 * @iface_context: interface-specific IPA context
 * @ipa_tx_desc: packet data descriptor
 *
 * Return: None
 */
static void wlan_ipa_send_pkt_to_tl(
		struct wlan_ipa_iface_context *iface_context,
		qdf_ipa_rx_data_t *ipa_tx_desc)
{
	struct wlan_ipa_priv *ipa_ctx = iface_context->ipa_ctx;
	qdf_nbuf_t skb;
	struct wlan_ipa_tx_desc *tx_desc;

	qdf_spin_lock_bh(&iface_context->interface_lock);
	/*
	 * During CAC period, data packets shouldn't be sent over the air so
	 * drop all the packets here
	 */
	if (iface_context->device_mode == QDF_SAP_MODE ||
	    iface_context->device_mode == QDF_P2P_GO_MODE) {
		if (ipa_ctx->dfs_cac_block_tx) {
			ipa_free_skb(ipa_tx_desc);
			qdf_spin_unlock_bh(&iface_context->interface_lock);
			iface_context->stats.num_tx_cac_drop++;
			wlan_ipa_wdi_rm_try_release(ipa_ctx);
			return;
		}
	}
	qdf_spin_unlock_bh(&iface_context->interface_lock);

	skb = QDF_IPA_RX_DATA_SKB(ipa_tx_desc);

	qdf_mem_zero(skb->cb, sizeof(skb->cb));

	/* Store IPA Tx buffer ownership into SKB CB */
	qdf_nbuf_ipa_owned_set(skb);
	if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config)) {
		qdf_nbuf_mapped_paddr_set(skb,
					  QDF_IPA_RX_DATA_DMA_ADDR(ipa_tx_desc)
					  + WLAN_IPA_WLAN_FRAG_HEADER
					  + WLAN_IPA_WLAN_IPA_HEADER);
		QDF_IPA_RX_DATA_SKB_LEN(ipa_tx_desc) -=
			WLAN_IPA_WLAN_FRAG_HEADER + WLAN_IPA_WLAN_IPA_HEADER;
	} else
		qdf_nbuf_mapped_paddr_set(skb, ipa_tx_desc->dma_addr);

	qdf_spin_lock_bh(&ipa_ctx->q_lock);
	/* get free Tx desc and assign ipa_tx_desc pointer */
	if (ipa_ctx->tx_desc_list.count &&
	    qdf_list_remove_front(&ipa_ctx->tx_desc_list,
				  (qdf_list_node_t **)&tx_desc) ==
							QDF_STATUS_SUCCESS) {
		tx_desc->ipa_tx_desc_ptr = ipa_tx_desc;
		ipa_ctx->stats.num_tx_desc_q_cnt++;
		qdf_spin_unlock_bh(&ipa_ctx->q_lock);
		/* Store Tx Desc index into SKB CB */
		QDF_NBUF_CB_TX_IPA_PRIV(skb) = tx_desc->id;
	} else {
		ipa_ctx->stats.num_tx_desc_error++;
		qdf_spin_unlock_bh(&ipa_ctx->q_lock);
		qdf_ipa_free_skb(ipa_tx_desc);
		wlan_ipa_wdi_rm_try_release(ipa_ctx);
		return;
	}

	skb = cdp_ipa_tx_send_data_frame(cds_get_context(QDF_MODULE_ID_SOC),
			(struct cdp_vdev *)iface_context->tl_context,
			QDF_IPA_RX_DATA_SKB(ipa_tx_desc));
	if (skb) {
		qdf_nbuf_free(skb);
		iface_context->stats.num_tx_err++;
		return;
	}

	atomic_inc(&ipa_ctx->tx_ref_cnt);

	iface_context->stats.num_tx++;
}

#ifdef CONFIG_IPA_WDI_UNIFIED_API

/*
 * TODO: Get WDI version through FW capabilities
 */
#ifdef CONFIG_LITHIUM
static inline void wlan_ipa_wdi_get_wdi_version(struct wlan_ipa_priv *ipa_ctx)
{
	ipa_ctx->wdi_version = IPA_WDI_3;
}
#elif defined(QCA_WIFI_3_0)
static inline void wlan_ipa_wdi_get_wdi_version(struct wlan_ipa_priv *ipa_ctx)
{
	ipa_ctx->wdi_version = IPA_WDI_2;
}
#else
static inline void wlan_ipa_wdi_get_wdi_version(struct wlan_ipa_priv *ipa_ctx)
{
	ipa_ctx->wdi_version = IPA_WDI_1;
}
#endif

static inline bool wlan_ipa_wdi_is_smmu_enabled(struct wlan_ipa_priv *ipa_ctx,
					       qdf_device_t osdev)
{
	return ipa_ctx->is_smmu_enabled && qdf_mem_smmu_s1_enabled(osdev);
}

static inline QDF_STATUS
wlan_ipa_wdi_setup(struct wlan_ipa_priv *ipa_ctx,
		   qdf_device_t osdev)
{
	qdf_ipa_sys_connect_params_t *sys_in = NULL;
	int i;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	sys_in = qdf_mem_malloc(sizeof(*sys_in) * WLAN_IPA_MAX_IFACE);
	if (!sys_in) {
		ipa_err("sys_in allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++)
		qdf_mem_copy(sys_in + i,
			     &ipa_ctx->sys_pipe[i].ipa_sys_params,
			     sizeof(qdf_ipa_sys_connect_params_t));

	qdf_status = cdp_ipa_setup(ipa_ctx->dp_soc, ipa_ctx->dp_pdev,
				   wlan_ipa_i2w_cb, wlan_ipa_w2i_cb,
				   wlan_ipa_wdi_meter_notifier_cb,
				   ipa_ctx->config->desc_size,
				   ipa_ctx,
				   wlan_ipa_is_rm_enabled(ipa_ctx->config),
				   &ipa_ctx->tx_pipe_handle,
				   &ipa_ctx->rx_pipe_handle,
				   wlan_ipa_wdi_is_smmu_enabled(ipa_ctx, osdev),
				   sys_in);

	qdf_mem_free(sys_in);

	return qdf_status;
}

#ifdef FEATURE_METERING
/**
 * wlan_ipa_wdi_init_metering() - IPA WDI metering init
 * @ipa_ctx: IPA context
 * @in: IPA WDI in param
 *
 * Return: QDF_STATUS
 */
static inline void wlan_ipa_wdi_init_metering(struct wlan_ipa_priv *ipa_ctxt,
					      qdf_ipa_wdi_init_in_params_t *in)
{
	QDF_IPA_WDI_INIT_IN_PARAMS_WDI_NOTIFY(in) =
				wlan_ipa_wdi_meter_notifier_cb;
}
#else
static inline void wlan_ipa_wdi_init_metering(struct wlan_ipa_priv *ipa_ctxt,
					      qdf_ipa_wdi_init_in_params_t *in)
{
}
#endif

/**
 * wlan_ipa_wdi_init() - IPA WDI init
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS wlan_ipa_wdi_init(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_ipa_wdi_init_in_params_t in;
	qdf_ipa_wdi_init_out_params_t out;
	int ret;

	ipa_ctx->uc_loaded = false;

	qdf_mem_zero(&in, sizeof(in));
	qdf_mem_zero(&out, sizeof(out));

	QDF_IPA_WDI_INIT_IN_PARAMS_WDI_VERSION(&in) = ipa_ctx->wdi_version;
	QDF_IPA_WDI_INIT_IN_PARAMS_NOTIFY(&in) = wlan_ipa_uc_loaded_uc_cb;
	QDF_IPA_WDI_INIT_IN_PARAMS_PRIV(&in) = ipa_ctx;
	wlan_ipa_wdi_init_metering(ipa_ctx, &in);

	ret = qdf_ipa_wdi_init(&in, &out);
	if (ret) {
		ipa_err("ipa_wdi_init failed with ret=%d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_IPA_WDI_INIT_OUT_PARAMS_IS_UC_READY(&out)) {
		ipa_debug("IPA uC READY");
		ipa_ctx->uc_loaded = true;
		ipa_ctx->is_smmu_enabled =
			QDF_IPA_WDI_INIT_OUT_PARAMS_IS_SMMU_ENABLED(&out);
		ipa_debug("is_smmu_enabled=%d", ipa_ctx->is_smmu_enabled);
	} else {
		return QDF_STATUS_E_BUSY;
	}

	return QDF_STATUS_SUCCESS;
}

static inline int wlan_ipa_wdi_cleanup(void)
{
	int ret;

	ret = qdf_ipa_wdi_cleanup();
	if (ret)
		ipa_info("ipa_wdi_cleanup failed ret=%d", ret);
	return ret;
}

static inline int wlan_ipa_wdi_setup_sys_pipe(struct wlan_ipa_priv *ipa_ctx,
					     struct ipa_sys_connect_params *sys,
					     uint32_t *handle)
{
	return 0;
}

static inline int wlan_ipa_wdi_teardown_sys_pipe(struct wlan_ipa_priv *ipa_ctx,
						uint32_t handle)
{
	return 0;
}

/**
 * wlan_ipa_pm_flush() - flush queued packets
 * @work: pointer to the scheduled work
 *
 * Called during PM resume to send packets to TL which were queued
 * while host was in the process of suspending.
 *
 * Return: None
 */
static void wlan_ipa_pm_flush(void *data)
{
	struct wlan_ipa_priv *ipa_ctx = (struct wlan_ipa_priv *)data;
	struct wlan_ipa_pm_tx_cb *pm_tx_cb = NULL;
	qdf_nbuf_t skb;
	uint32_t dequeued = 0;

	qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	while (((skb = qdf_nbuf_queue_remove(&ipa_ctx->pm_queue_head)) !=
	       NULL)) {
		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

		pm_tx_cb = (struct wlan_ipa_pm_tx_cb *)skb->cb;
		dequeued++;

		if (pm_tx_cb->exception) {
			if (ipa_ctx->softap_xmit &&
			    pm_tx_cb->iface_context->dev) {
				ipa_ctx->softap_xmit(skb,
						pm_tx_cb->iface_context->dev);
				ipa_ctx->stats.num_tx_fwd_ok++;
			} else {
				dev_kfree_skb_any(skb);
			}
		} else {
			wlan_ipa_send_pkt_to_tl(pm_tx_cb->iface_context,
						pm_tx_cb->ipa_tx_desc);
		}

		qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	}
	qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

	ipa_ctx->stats.num_tx_dequeued += dequeued;
	if (dequeued > ipa_ctx->stats.num_max_pm_queue)
		ipa_ctx->stats.num_max_pm_queue = dequeued;
}

int wlan_ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	if (!num_buf) {
		ipa_info("No buffers to map/unmap");
		return 0;
	}

	if (map)
		return qdf_ipa_wdi_create_smmu_mapping(num_buf, buf_arr);
	else
		return qdf_ipa_wdi_release_smmu_mapping(num_buf, buf_arr);
}

#else /* CONFIG_IPA_WDI_UNIFIED_API */

static inline void wlan_ipa_wdi_get_wdi_version(struct wlan_ipa_priv *ipa_ctx)
{
}

static inline int wlan_ipa_wdi_is_smmu_enabled(struct wlan_ipa_priv *ipa_ctx,
					       qdf_device_t osdev)
{
	return qdf_mem_smmu_s1_enabled(osdev);
}

static inline QDF_STATUS wlan_ipa_wdi_setup(struct wlan_ipa_priv *ipa_ctx,
					    qdf_device_t osdev)
{
	return cdp_ipa_setup(ipa_ctx->dp_soc, ipa_ctx->dp_pdev,
			     wlan_ipa_i2w_cb, wlan_ipa_w2i_cb,
			     wlan_ipa_wdi_meter_notifier_cb,
			     ipa_ctx->config->desc_size,
			     ipa_ctx, wlan_ipa_is_rm_enabled(ipa_ctx->config),
			     &ipa_ctx->tx_pipe_handle,
			     &ipa_ctx->rx_pipe_handle);
}

static inline QDF_STATUS wlan_ipa_wdi_init(struct wlan_ipa_priv *ipa_ctx)
{
	struct ipa_wdi_uc_ready_params uc_ready_param;

	ipa_ctx->uc_loaded = false;
	uc_ready_param.priv = (void *)ipa_ctx;
	uc_ready_param.notify = wlan_ipa_uc_loaded_uc_cb;
	if (qdf_ipa_uc_reg_rdyCB(&uc_ready_param)) {
		ipa_info("UC Ready CB register fail");
		return QDF_STATUS_E_FAILURE;
	}

	if (true == uc_ready_param.is_uC_ready) {
		ipa_info("UC Ready");
		ipa_ctx->uc_loaded = true;
	} else {
		return QDF_STATUS_E_BUSY;
	}

	return QDF_STATUS_SUCCESS;
}

static inline int wlan_ipa_wdi_cleanup(void)
{
	int ret;

	ret = qdf_ipa_uc_dereg_rdyCB();
	if (ret)
		ipa_info("UC Ready CB deregister fail");
	return ret;
}

static inline int wlan_ipa_wdi_setup_sys_pipe(
		struct wlan_ipa_priv *ipa_ctx,
		struct ipa_sys_connect_params *sys, uint32_t *handle)
{
	return qdf_ipa_setup_sys_pipe(sys, handle);
}

static inline int wlan_ipa_wdi_teardown_sys_pipe(
		struct wlan_ipa_priv *ipa_ctx,
		uint32_t handle)
{
	return qdf_ipa_teardown_sys_pipe(handle);
}

/**
 * wlan_ipa_pm_flush() - flush queued packets
 * @work: pointer to the scheduled work
 *
 * Called during PM resume to send packets to TL which were queued
 * while host was in the process of suspending.
 *
 * Return: None
 */
static void wlan_ipa_pm_flush(void *data)
{
	struct wlan_ipa_priv *ipa_ctx = (struct wlan_ipa_priv *)data;
	struct wlan_ipa_pm_tx_cb *pm_tx_cb = NULL;
	qdf_nbuf_t skb;
	uint32_t dequeued = 0;

	qdf_wake_lock_acquire(&ipa_ctx->wake_lock,
			      WIFI_POWER_EVENT_WAKELOCK_IPA);
	qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	while (((skb = qdf_nbuf_queue_remove(&ipa_ctx->pm_queue_head)) !=
	       NULL)) {
		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

		pm_tx_cb = (struct wlan_ipa_pm_tx_cb *)skb->cb;
		dequeued++;

		if (pm_tx_cb->exception) {
			if (ipa_ctx->softap_xmit &&
			    pm_tx_cb->iface_context->dev) {
				ipa_ctx->softap_xmit(skb,
						pm_tx_cb->iface_context->dev);
				ipa_ctx->stats.num_tx_fwd_ok++;
			} else {
				dev_kfree_skb_any(skb);
			}
		} else {
			wlan_ipa_send_pkt_to_tl(pm_tx_cb->iface_context,
						pm_tx_cb->ipa_tx_desc);
		}

		qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	}
	qdf_spin_unlock_bh(&ipa_ctx->pm_lock);
	qdf_wake_lock_release(&ipa_ctx->wake_lock,
			      WIFI_POWER_EVENT_WAKELOCK_IPA);

	ipa_ctx->stats.num_tx_dequeued += dequeued;
	if (dequeued > ipa_ctx->stats.num_max_pm_queue)
		ipa_ctx->stats.num_max_pm_queue = dequeued;
}

int wlan_ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	if (!num_buf) {
		ipa_info("No buffers to map/unmap");
		return 0;
	}

	if (map)
		return qdf_ipa_create_wdi_mapping(num_buf, buf_arr);
	else
		return qdf_ipa_release_wdi_mapping(num_buf, buf_arr);
}

#endif /* CONFIG_IPA_WDI_UNIFIED_API */

/**
 * wlan_ipa_send_skb_to_network() - Send skb to kernel
 * @skb: network buffer
 * @iface_ctx: IPA interface context
 *
 * Called when a network buffer is received which should not be routed
 * to the IPA module.
 *
 * Return: None
 */
static void
wlan_ipa_send_skb_to_network(qdf_nbuf_t skb,
			     struct wlan_ipa_iface_context *iface_ctx)
{
	struct wlan_ipa_priv *ipa_ctx = gp_ipa;

	if (!iface_ctx->dev) {
		ipa_debug_rl("Invalid interface");
		ipa_ctx->ipa_rx_internal_drop_count++;
		dev_kfree_skb_any(skb);
		return;
	}

	skb->destructor = wlan_ipa_uc_rt_debug_destructor;

	if (ipa_ctx->send_to_nw)
		ipa_ctx->send_to_nw(skb, iface_ctx->dev);

	ipa_ctx->ipa_rx_net_send_count++;
}

/**
 * wlan_ipa_forward() - handle packet forwarding to wlan tx
 * @ipa_ctx: pointer to ipa ipa context
 * @iface_ctx: interface context
 * @skb: data pointer
 *
 * if exception packet has set forward bit, copied new packet should be
 * forwarded to wlan tx. if wlan subsystem is in suspend state, packet should
 * put into pm queue and tx procedure will be differed
 *
 * Return: None
 */
static void wlan_ipa_forward(struct wlan_ipa_priv *ipa_ctx,
			     struct wlan_ipa_iface_context *iface_ctx,
			     qdf_nbuf_t skb)
{
	struct wlan_ipa_pm_tx_cb *pm_tx_cb;

	qdf_spin_lock_bh(&ipa_ctx->pm_lock);

	/* Set IPA ownership for intra-BSS Tx packets to avoid skb_orphan */
	qdf_nbuf_ipa_owned_set(skb);

	/* WLAN subsystem is in suspend, put in queue */
	if (ipa_ctx->suspended) {
		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);
		ipa_info_rl("Tx in suspend, put in queue");
		qdf_mem_zero(skb->cb, sizeof(skb->cb));
		pm_tx_cb = (struct wlan_ipa_pm_tx_cb *)skb->cb;
		pm_tx_cb->exception = true;
		pm_tx_cb->iface_context = iface_ctx;
		qdf_spin_lock_bh(&ipa_ctx->pm_lock);
		qdf_nbuf_queue_add(&ipa_ctx->pm_queue_head, skb);
		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);
		ipa_ctx->stats.num_tx_queued++;
	} else {
		/* Resume, put packet into WLAN TX */
		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

		if (ipa_ctx->softap_xmit) {
			if (ipa_ctx->softap_xmit(skb, iface_ctx->dev)) {
				ipa_err_rl("packet Tx fail");
				ipa_ctx->stats.num_tx_fwd_err++;
			} else {
				ipa_ctx->stats.num_tx_fwd_ok++;
			}
		} else {
			dev_kfree_skb_any(skb);
		}
	}
}

/**
 * wlan_ipa_intrabss_forward() - Forward intra bss packets.
 * @ipa_ctx: pointer to IPA IPA struct
 * @iface_ctx: ipa interface context
 * @desc: Firmware descriptor
 * @skb: Data buffer
 *
 * Return:
 *      WLAN_IPA_FORWARD_PKT_NONE
 *      WLAN_IPA_FORWARD_PKT_DISCARD
 *      WLAN_IPA_FORWARD_PKT_LOCAL_STACK
 *
 */

static enum wlan_ipa_forward_type wlan_ipa_intrabss_forward(
		struct wlan_ipa_priv *ipa_ctx,
		struct wlan_ipa_iface_context *iface_ctx,
		uint8_t desc,
		qdf_nbuf_t skb)
{
	int ret = WLAN_IPA_FORWARD_PKT_NONE;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct ol_txrx_pdev_t *pdev = cds_get_context(QDF_MODULE_ID_TXRX);

	if ((desc & FW_RX_DESC_FORWARD_M)) {
		if (!ol_txrx_fwd_desc_thresh_check(
			(struct ol_txrx_vdev_t *)cdp_get_vdev_from_vdev_id(soc,
						(struct cdp_pdev *)pdev,
						iface_ctx->session_id))) {
			/* Drop the packet*/
			ipa_ctx->stats.num_tx_fwd_err++;
			dev_kfree_skb_any(skb);
			ret = WLAN_IPA_FORWARD_PKT_DISCARD;
			return ret;
		}
		ipa_debug_rl("Forward packet to Tx (fw_desc=%d)", desc);
		ipa_ctx->ipa_tx_forward++;

		if ((desc & FW_RX_DESC_DISCARD_M)) {
			wlan_ipa_forward(ipa_ctx, iface_ctx, skb);
			ipa_ctx->ipa_rx_internal_drop_count++;
			ipa_ctx->ipa_rx_discard++;
			ret = WLAN_IPA_FORWARD_PKT_DISCARD;
		} else {
			struct sk_buff *cloned_skb = skb_clone(skb, GFP_ATOMIC);

			if (cloned_skb)
				wlan_ipa_forward(ipa_ctx, iface_ctx,
						 cloned_skb);
			else
				ipa_err_rl("tx skb alloc failed");
			ret = WLAN_IPA_FORWARD_PKT_LOCAL_STACK;
		}
	}

	return ret;
}

/**
 * wlan_ipa_eapol_intrabss_fwd_check() - Check if eapol pkt intrabss fwd is
 *  allowed or not
 * @ipa_ctx: IPA global context
 * @vdev_id: vdev id
 * @nbuf: network buffer
 *
 * Return: true if intrabss fwd is allowed for eapol else false
 */
static bool
wlan_ipa_eapol_intrabss_fwd_check(struct wlan_ipa_priv *ipa_ctx,
				  uint8_t vdev_id, qdf_nbuf_t nbuf)
{
	struct cdp_vdev *vdev;
	uint8_t *vdev_mac_addr;

	vdev = cdp_get_vdev_from_vdev_id(ipa_ctx->dp_soc, ipa_ctx->dp_pdev,
					 vdev_id);
	if (!vdev) {
		ipa_err_rl("txrx vdev is NULL for vdev_id = %d", vdev_id);
		return false;
	}

	vdev_mac_addr = cdp_get_vdev_mac_addr(ipa_ctx->dp_soc, vdev);

	if (!vdev_mac_addr)
		return false;

	if (qdf_mem_cmp(qdf_nbuf_data(nbuf) + QDF_NBUF_DEST_MAC_OFFSET,
			vdev_mac_addr, QDF_MAC_ADDR_SIZE))
		return false;

	return true;
}

/**
 * __wlan_ipa_w2i_cb() - WLAN to IPA callback handler
 * @priv: pointer to private data registered with IPA (we register a
 *	pointer to the global IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
static void __wlan_ipa_w2i_cb(void *priv, qdf_ipa_dp_evt_type_t evt,
			      unsigned long data)
{
	struct wlan_ipa_priv *ipa_ctx = NULL;
	qdf_nbuf_t skb;
	uint8_t iface_id;
	uint8_t session_id;
	struct wlan_ipa_iface_context *iface_context;
	uint8_t fw_desc;
	void *peer;
	bool is_eapol_wapi = false;
	struct qdf_mac_addr peer_mac_addr = QDF_MAC_ADDR_ZERO_INIT;
	uint8_t sta_idx;

	ipa_ctx = (struct wlan_ipa_priv *)priv;
	if (!ipa_ctx) {
		if (evt == IPA_RECEIVE) {
			skb = (qdf_nbuf_t)data;
			dev_kfree_skb_any(skb);
		}
		return;
	}

	if (qdf_is_module_state_transitioning()) {
		ipa_err_rl("Module transition in progress");
		if (evt == IPA_RECEIVE) {
			skb = (qdf_nbuf_t)data;
			ipa_ctx->ipa_rx_internal_drop_count++;
			dev_kfree_skb_any(skb);
		}
		return;
	}

	switch (evt) {
	case IPA_RECEIVE:
		skb = (qdf_nbuf_t) data;

		if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			session_id = (uint8_t)skb->cb[0];
			iface_id = ipa_ctx->vdev_to_iface[session_id];
		} else {
			iface_id = WLAN_IPA_GET_IFACE_ID(skb->data);
		}
		if (iface_id >= WLAN_IPA_MAX_IFACE) {
			ipa_err_rl("Invalid iface_id: %u", iface_id);
			ipa_ctx->ipa_rx_internal_drop_count++;
			dev_kfree_skb_any(skb);
			return;
		}

		iface_context = &ipa_ctx->iface_context[iface_id];
		if (!iface_context->tl_context) {
			ipa_err_rl("TL context of iface_id %u is NULL",
				   iface_id);
			ipa_ctx->ipa_rx_internal_drop_count++;
			dev_kfree_skb_any(skb);
			return;
		}

		if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			ipa_ctx->stats.num_rx_excep++;
			qdf_nbuf_pull_head(skb, WLAN_IPA_UC_WLAN_CLD_HDR_LEN);
		} else {
			qdf_nbuf_pull_head(skb, WLAN_IPA_WLAN_CLD_HDR_LEN);
		}

		iface_context->stats.num_rx_ipa_excep++;

		if (iface_context->device_mode == QDF_STA_MODE)
			qdf_copy_macaddr(&peer_mac_addr, &iface_context->bssid);
		else if (iface_context->device_mode == QDF_SAP_MODE)
			qdf_mem_copy(&peer_mac_addr.bytes[0],
				     qdf_nbuf_data(skb) +
				     QDF_NBUF_SRC_MAC_OFFSET,
				     QDF_MAC_ADDR_SIZE);

		if (qdf_nbuf_is_ipv4_eapol_pkt(skb)) {
			is_eapol_wapi = true;
			if (iface_context->device_mode == QDF_SAP_MODE &&
			    !wlan_ipa_eapol_intrabss_fwd_check(ipa_ctx,
					      iface_context->session_id, skb)) {
				ipa_err_rl("EAPOL intrabss fwd drop DA: %pM",
					   qdf_nbuf_data(skb) +
					   QDF_NBUF_DEST_MAC_OFFSET);
				ipa_ctx->ipa_rx_internal_drop_count++;
				dev_kfree_skb_any(skb);
				return;
			}
		} else if (qdf_nbuf_is_ipv4_wapi_pkt(skb)) {
			is_eapol_wapi = true;
		}

		peer = cdp_peer_find_by_addr(ipa_ctx->dp_soc, ipa_ctx->dp_pdev,
					     peer_mac_addr.bytes, &sta_idx);

		/*
		 * Check for peer authorized state before allowing
		 * non-EAPOL/WAPI frames to be intrabss forwarded
		 * or submitted to stack.
		 */

		if (peer && cdp_peer_state_get(ipa_ctx->dp_soc, peer) !=
		    OL_TXRX_PEER_STATE_AUTH && !is_eapol_wapi) {
			ipa_err_rl("Non EAPOL/WAPI packet received when peer %pM is unauthorized",
				   peer_mac_addr.bytes);
			ipa_ctx->ipa_rx_internal_drop_count++;
			dev_kfree_skb_any(skb);
			return;
		}

		/* Disable to forward Intra-BSS Rx packets when
		 * ap_isolate=1 in hostapd.conf
		 */
		if (!ipa_ctx->ap_intrabss_fwd) {
			/*
			 * When INTRA_BSS_FWD_OFFLOAD is enabled, FW will send
			 * all Rx packets to IPA uC, which need to be forwarded
			 * to other interface.
			 * And, IPA driver will send back to WLAN host driver
			 * through exception pipe with fw_desc field set by FW.
			 * Here we are checking fw_desc field for FORWARD bit
			 * set, and forward to Tx. Then copy to kernel stack
			 * only when DISCARD bit is not set.
			 */
			fw_desc = (uint8_t)skb->cb[1];
			if (WLAN_IPA_FORWARD_PKT_DISCARD ==
			    wlan_ipa_intrabss_forward(ipa_ctx, iface_context,
						     fw_desc, skb))
				break;
		} else {
			ipa_debug_rl("Intra-BSS forwarding is disabled");
		}

		wlan_ipa_send_skb_to_network(skb, iface_context);
		break;

	default:
		ipa_err_rl("w2i cb wrong event: 0x%x", evt);
		return;
	}
}

/**
 * wlan_ipa_w2i_cb() - SSR wrapper for __wlan_ipa_w2i_cb
 * @priv: pointer to private data registered with IPA (we register a
 *	pointer to the global IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
static void wlan_ipa_w2i_cb(void *priv, qdf_ipa_dp_evt_type_t evt,
			    unsigned long data)
{
	qdf_ssr_protect(__func__);
	__wlan_ipa_w2i_cb(priv, evt, data);
	qdf_ssr_unprotect(__func__);
}

/**
 * __wlan_ipa_i2w_cb() - IPA to WLAN callback
 * @priv: pointer to private data registered with IPA (we register a
 *	pointer to the interface-specific IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
static void __wlan_ipa_i2w_cb(void *priv, qdf_ipa_dp_evt_type_t evt,
			      unsigned long data)
{
	struct wlan_ipa_priv *ipa_ctx = NULL;
	qdf_ipa_rx_data_t *ipa_tx_desc;
	struct wlan_ipa_iface_context *iface_context;
	qdf_nbuf_t skb;
	struct wlan_ipa_pm_tx_cb *pm_tx_cb = NULL;

	iface_context = (struct wlan_ipa_iface_context *)priv;
	ipa_tx_desc = (qdf_ipa_rx_data_t *)data;
	ipa_ctx = iface_context->ipa_ctx;

	if (qdf_is_module_state_transitioning()) {
		ipa_err_rl("Module transition in progress");
		ipa_free_skb(ipa_tx_desc);
		iface_context->stats.num_tx_drop++;
		return;
	}

	if (evt != IPA_RECEIVE) {
		ipa_err_rl("Event is not IPA_RECEIVE");
		ipa_free_skb(ipa_tx_desc);
		iface_context->stats.num_tx_drop++;
		return;
	}

	skb = QDF_IPA_RX_DATA_SKB(ipa_tx_desc);

	/*
	 * If PROD resource is not requested here then there may be cases where
	 * IPA hardware may be clocked down because of not having proper
	 * dependency graph between WLAN CONS and modem PROD pipes. Adding the
	 * workaround to request PROD resource while data is going over CONS
	 * pipe to prevent the IPA hardware clockdown.
	 */
	wlan_ipa_wdi_rm_request(ipa_ctx);

	qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	/*
	 * If host is still suspended then queue the packets and these will be
	 * drained later when resume completes. When packet is arrived here and
	 * host is suspended, this means that there is already resume is in
	 * progress.
	 */
	if (ipa_ctx->suspended) {
		qdf_mem_zero(skb->cb, sizeof(skb->cb));
		pm_tx_cb = (struct wlan_ipa_pm_tx_cb *)skb->cb;
		pm_tx_cb->iface_context = iface_context;
		pm_tx_cb->ipa_tx_desc = ipa_tx_desc;
		qdf_nbuf_queue_add(&ipa_ctx->pm_queue_head, skb);
		ipa_ctx->stats.num_tx_queued++;

		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);
		return;
	}

	qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

	/*
	 * If we are here means, host is not suspended, wait for the work queue
	 * to finish.
	 */
	qdf_flush_work(&ipa_ctx->pm_work);

	return wlan_ipa_send_pkt_to_tl(iface_context, ipa_tx_desc);
}

/**
 * wlan_ipa_i2w_cb() - IPA to WLAN callback
 * @priv: pointer to private data registered with IPA (we register a
 *	pointer to the interface-specific IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
static void wlan_ipa_i2w_cb(void *priv, qdf_ipa_dp_evt_type_t evt,
			    unsigned long data)
{
	qdf_ssr_protect(__func__);
	__wlan_ipa_i2w_cb(priv, evt, data);
	qdf_ssr_unprotect(__func__);
}

QDF_STATUS wlan_ipa_suspend(struct wlan_ipa_priv *ipa_ctx)
{
	/*
	 * Check if IPA is ready for suspend, If we are here means, there is
	 * high chance that suspend would go through but just to avoid any race
	 * condition after suspend started, these checks are conducted before
	 * allowing to suspend.
	 */
	if (atomic_read(&ipa_ctx->tx_ref_cnt))
		return QDF_STATUS_E_AGAIN;

	if (!wlan_ipa_is_rm_released(ipa_ctx))
		return QDF_STATUS_E_AGAIN;

	qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	ipa_ctx->suspended = true;
	qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

	if (ipa_ctx->config->ipa_force_voting &&
	    !ipa_ctx->ipa_pipes_down)
		wlan_ipa_set_perf_level(ipa_ctx,
					ipa_ctx->config->bus_bw_high,
					ipa_ctx->config->bus_bw_high);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_ipa_resume(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_sched_work(0, &ipa_ctx->pm_work);

	qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	ipa_ctx->suspended = false;
	qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_ipa_uc_enable_pipes(struct wlan_ipa_priv *ipa_ctx)
{
	int result;

	ipa_debug("enter");

	if (!ipa_ctx->ipa_pipes_down) {
		/*
		 * IPA WDI Pipes are already activated due to
		 * rm deferred resources grant
		 */
		ipa_warn("IPA WDI Pipes are already activated");
		goto end;
	}

	result = cdp_ipa_enable_pipes(ipa_ctx->dp_soc,
				      ipa_ctx->dp_pdev);
	if (result) {
		ipa_err("Enable IPA WDI PIPE failed: ret=%d", result);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_event_reset(&ipa_ctx->ipa_resource_comp);
	ipa_ctx->ipa_pipes_down = false;

	cdp_ipa_enable_autonomy(ipa_ctx->dp_soc,
				ipa_ctx->dp_pdev);

end:
	ipa_debug("exit: ipa_pipes_down=%d", ipa_ctx->ipa_pipes_down);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_ipa_uc_disable_pipes(struct wlan_ipa_priv *ipa_ctx)
{
	int result;

	ipa_debug("enter");

	if (ipa_ctx->ipa_pipes_down) {
		ipa_info("IPA WDI Pipes are already deactivated");
		goto end;
	}

	qdf_spin_lock_bh(&ipa_ctx->pipes_down_lock);
	if (ipa_ctx->pipes_down_in_progress || ipa_ctx->ipa_pipes_down) {
		qdf_spin_unlock_bh(&ipa_ctx->pipes_down_lock);
		ipa_warn("IPA WDI Pipes down already in progress");
		return QDF_STATUS_E_ALREADY;
	}
	ipa_ctx->pipes_down_in_progress = true;
	qdf_spin_unlock_bh(&ipa_ctx->pipes_down_lock);

	cdp_ipa_disable_autonomy(ipa_ctx->dp_soc,
				 ipa_ctx->dp_pdev);

	result = cdp_ipa_disable_pipes(ipa_ctx->dp_soc,
				       ipa_ctx->dp_pdev);
	if (result) {
		ipa_err("Disable IPA WDI PIPE failed: ret=%d", result);
		ipa_ctx->pipes_down_in_progress = false;
		return QDF_STATUS_E_FAILURE;
	}

	ipa_ctx->ipa_pipes_down = true;
	ipa_ctx->pipes_down_in_progress = false;

end:
	ipa_debug("exit: ipa_pipes_down=%d", ipa_ctx->ipa_pipes_down);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_ipa_uc_find_add_assoc_sta() - Find associated station
 * @ipa_ctx: Global IPA IPA context
 * @sta_add: Should station be added
 * @sta_id: ID of the station being queried
 *
 * Return: true if the station was found
 */
static bool wlan_ipa_uc_find_add_assoc_sta(struct wlan_ipa_priv *ipa_ctx,
					   bool sta_add, uint8_t sta_id,
					   uint8_t *mac_addr)
{
	bool sta_found = false;
	uint8_t idx;

	for (idx = 0; idx < WLAN_IPA_MAX_STA_COUNT; idx++) {
		if ((ipa_ctx->assoc_stas_map[idx].is_reserved) &&
		    (ipa_ctx->assoc_stas_map[idx].sta_id == sta_id)) {
			sta_found = true;
			break;
		}
	}
	if (sta_add && sta_found) {
		ipa_err("STA ID %d already exist, cannot add", sta_id);
		return sta_found;
	}
	if (sta_add) {
		for (idx = 0; idx < WLAN_IPA_MAX_STA_COUNT; idx++) {
			if (!ipa_ctx->assoc_stas_map[idx].is_reserved) {
				ipa_ctx->assoc_stas_map[idx].is_reserved = true;
				ipa_ctx->assoc_stas_map[idx].sta_id = sta_id;
				qdf_mem_copy(&ipa_ctx->assoc_stas_map[idx].
					     mac_addr, mac_addr,
					     QDF_NET_ETH_LEN);
				return sta_found;
			}
		}
	}
	if (!sta_add && !sta_found) {
		ipa_info("STA ID %d does not exist, cannot delete", sta_id);
		return sta_found;
	}
	if (!sta_add) {
		for (idx = 0; idx < WLAN_IPA_MAX_STA_COUNT; idx++) {
			if ((ipa_ctx->assoc_stas_map[idx].is_reserved) &&
			    (ipa_ctx->assoc_stas_map[idx].sta_id == sta_id)) {
				ipa_ctx->assoc_stas_map[idx].is_reserved =
					false;
				ipa_ctx->assoc_stas_map[idx].sta_id = 0xFF;
				qdf_mem_zero(
					&ipa_ctx->assoc_stas_map[idx].mac_addr,
					QDF_NET_ETH_LEN);
				return sta_found;
			}
		}
	}

	return sta_found;
}

/**
 * wlan_ipa_get_ifaceid() - Get IPA context interface ID
 * @ipa_ctx: IPA context
 * @session_id: Session ID
 *
 * Return: None
 */
static int wlan_ipa_get_ifaceid(struct wlan_ipa_priv *ipa_ctx,
				uint8_t session_id)
{
	struct wlan_ipa_iface_context *iface_ctx;
	int i;

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_ctx = &ipa_ctx->iface_context[i];
		if (iface_ctx->session_id == session_id)
			break;
	}

	return i;
}

/**
 * wlan_ipa_cleanup_iface() - Cleanup IPA on a given interface
 * @iface_context: interface-specific IPA context
 *
 * Return: None
 */
static void wlan_ipa_cleanup_iface(struct wlan_ipa_iface_context *iface_context)
{
	struct wlan_ipa_priv *ipa_ctx = iface_context->ipa_ctx;

	ipa_debug("enter");

	if (!iface_context->tl_context)
		return;

	cdp_ipa_cleanup_iface(ipa_ctx->dp_soc,
			      iface_context->dev->name,
			      wlan_ipa_is_ipv6_enabled(ipa_ctx->config));

	qdf_spin_lock_bh(&iface_context->interface_lock);
	iface_context->tl_context = NULL;
	iface_context->dev = NULL;
	iface_context->device_mode = QDF_MAX_NO_OF_MODE;
	iface_context->session_id = WLAN_IPA_MAX_SESSION;
	iface_context->sta_id = WLAN_IPA_MAX_STA_COUNT;
	qdf_spin_unlock_bh(&iface_context->interface_lock);
	iface_context->ifa_address = 0;
	qdf_zero_macaddr(&iface_context->bssid);
	if (!iface_context->ipa_ctx->num_iface) {
		ipa_err("NUM INTF 0, Invalid");
		QDF_ASSERT(0);
	}
	iface_context->ipa_ctx->num_iface--;
	ipa_debug("exit: num_iface=%d", iface_context->ipa_ctx->num_iface);
}

/**
 * wlan_ipa_setup_iface() - Setup IPA on a given interface
 * @ipa_ctx: IPA IPA global context
 * @net_dev: Interface net device
 * @device_mode: Net interface device mode
 * @adapter: Interface upon which IPA is being setup
 * @sta_id: Station ID of the API instance
 * @session_id: Station ID of the API instance
 *
 * Return: QDF STATUS
 */
static QDF_STATUS wlan_ipa_setup_iface(struct wlan_ipa_priv *ipa_ctx,
				       qdf_netdev_t net_dev,
				       uint8_t device_mode, uint8_t sta_id,
				       uint8_t session_id)
{
	struct wlan_ipa_iface_context *iface_context = NULL;
	void *tl_context = NULL;
	int i;
	QDF_STATUS status;

	/* Lower layer may send multiple START_BSS_EVENT in DFS mode or during
	 * channel change indication. Since these indications are sent by lower
	 * layer as SAP updates and IPA doesn't have to do anything for these
	 * updates so ignoring!
	 */
	if (device_mode == QDF_SAP_MODE) {
		for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
			iface_context = &(ipa_ctx->iface_context[i]);
			if (iface_context->dev == net_dev) {
				if (iface_context->device_mode ==
				    device_mode)
					return QDF_STATUS_SUCCESS;

				ipa_err("Obsolete iface %u found, device_mode %u, will remove it.",
					i,
					iface_context->device_mode);
				wlan_ipa_cleanup_iface(iface_context);
			}
		}
	}

	if (WLAN_IPA_MAX_IFACE == ipa_ctx->num_iface) {
		ipa_err("Max interface reached %d", WLAN_IPA_MAX_IFACE);
		status = QDF_STATUS_E_NOMEM;
		QDF_ASSERT(0);
		goto end;
	}

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		if (ipa_ctx->iface_context[i].tl_context == NULL) {
			iface_context = &(ipa_ctx->iface_context[i]);
			break;
		}
	}

	if (iface_context == NULL) {
		ipa_err("All the IPA interfaces are in use");
		status = QDF_STATUS_E_NOMEM;
		QDF_ASSERT(0);
		goto end;
	}

	iface_context->sta_id = sta_id;
	tl_context = (void *)cdp_peer_get_vdev_by_sta_id(ipa_ctx->dp_soc,
							 ipa_ctx->dp_pdev,
							 sta_id);
	if (tl_context == NULL) {
		ipa_err("Not able to get TL context sta_id: %d", sta_id);
		status = QDF_STATUS_E_INVAL;
		goto end;
	}

	iface_context->tl_context = tl_context;
	iface_context->dev = net_dev;
	iface_context->device_mode = device_mode;
	iface_context->session_id = session_id;

	status = cdp_ipa_setup_iface(ipa_ctx->dp_soc, net_dev->name,
				     net_dev->dev_addr,
				     iface_context->prod_client,
				     iface_context->cons_client,
				     session_id,
				     wlan_ipa_is_ipv6_enabled(ipa_ctx->config));
	if (status != QDF_STATUS_SUCCESS)
		goto end;

	ipa_ctx->num_iface++;

	ipa_debug("exit: num_iface=%d", ipa_ctx->num_iface);

	return status;

end:
	if (iface_context)
		wlan_ipa_cleanup_iface(iface_context);

	return status;
}

/**
 * wlan_ipa_uc_handle_first_con() - Handle first uC IPA connection
 * @ipa_ctx: IPA context
 *
 * Return: QDF STATUS
 */
static QDF_STATUS wlan_ipa_uc_handle_first_con(struct wlan_ipa_priv *ipa_ctx)
{
	ipa_debug("enter");

	ipa_ctx->activated_fw_pipe = 0;
	ipa_ctx->resource_loading = true;

	/* If RM feature enabled
	 * Request PROD Resource first
	 * PROD resource may return sync or async manners
	 */
	if (wlan_ipa_is_rm_enabled(ipa_ctx->config)) {
		if (!wlan_ipa_wdi_rm_request_resource(ipa_ctx,
						IPA_RM_RESOURCE_WLAN_PROD)) {
			/* RM PROD request sync return
			 * enable pipe immediately
			 */
			if (wlan_ipa_uc_enable_pipes(ipa_ctx)) {
				ipa_err("IPA WDI Pipe activation failed");
				ipa_ctx->resource_loading = false;
				return QDF_STATUS_E_BUSY;
			}
		} else {
			ipa_err("IPA WDI Pipe activation deferred");
		}
	} else {
		/* RM Disabled
		 * Just enabled all the PIPEs
		 */
		if (wlan_ipa_uc_enable_pipes(ipa_ctx)) {
			ipa_err("IPA WDI Pipe activation failed");
			ipa_ctx->resource_loading = false;
			return QDF_STATUS_E_BUSY;
		}
		ipa_ctx->resource_loading = false;
	}

	ipa_debug("exit");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_ipa_uc_handle_last_discon() - Handle last uC IPA disconnection
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
static void wlan_ipa_uc_handle_last_discon(struct wlan_ipa_priv *ipa_ctx)
{
	ipa_debug("enter");

	ipa_ctx->resource_unloading = true;
	qdf_event_reset(&ipa_ctx->ipa_resource_comp);
	ipa_info("Disable FW RX PIPE");
	cdp_ipa_set_active(ipa_ctx->dp_soc, ipa_ctx->dp_pdev, false, false);

	ipa_debug("exit: IPA WDI Pipes deactivated");
}

static inline
bool wlan_sap_no_client_connected(struct wlan_ipa_priv *ipa_ctx)
{
	return !(ipa_ctx->sap_num_connected_sta);
}

static inline
bool wlan_sta_is_connected(struct wlan_ipa_priv *ipa_ctx)
{
	return ipa_ctx->sta_connected;
}

static inline
bool wlan_ipa_uc_is_loaded(struct wlan_ipa_priv *ipa_ctx)
{
	return ipa_ctx->uc_loaded;
}

/**
 * wlan_ipa_uc_offload_enable_disable() - wdi enable/disable notify to fw
 * @ipa_ctx: global IPA context
 * @offload_type: MCC or SCC
 * @session_id: Session Id
 * @enable: TX offload enable or disable
 *
 * Return: none
 */
static void wlan_ipa_uc_offload_enable_disable(struct wlan_ipa_priv *ipa_ctx,
					       uint32_t offload_type,
					       uint8_t session_id,
					       bool enable)
{

	struct ipa_uc_offload_control_params req = {0};

	if (session_id >= WLAN_IPA_MAX_SESSION) {
		ipa_err("invalid session id: %d", session_id);
		return;
	}

	if (enable == ipa_ctx->vdev_offload_enabled[session_id]) {
		ipa_info("IPA offload status is already set");
		ipa_info("offload_type=%d, vdev_id=%d, enable=%d",
			 offload_type, session_id, enable);
		return;
	}

	ipa_info("offload_type=%d, session_id=%d, enable=%d",
		 offload_type, session_id, enable);

	req.offload_type = offload_type;
	req.vdev_id = session_id;
	req.enable = enable;

	if (QDF_STATUS_SUCCESS !=
	    ipa_send_uc_offload_enable_disable(ipa_ctx->pdev, &req)) {
		ipa_err("Fail to enable IPA offload");
		ipa_err("offload type=%d, vdev_id=%d, enable=%d",
			offload_type, session_id, enable);
	} else {
		ipa_ctx->vdev_offload_enabled[session_id] = enable;
	}
}

static inline void
wlan_ipa_save_bssid_iface_ctx(struct wlan_ipa_priv *ipa_ctx, uint8_t iface_id,
			      uint8_t *mac_addr)
{
	qdf_mem_copy(ipa_ctx->iface_context[iface_id].bssid.bytes,
		     mac_addr, QDF_MAC_ADDR_SIZE);
}

/**
 * __wlan_ipa_wlan_evt() - IPA event handler
 * @net_dev: Interface net device
 * @device_mode: Net interface device mode
 * @sta_id: station id for the event
 * @session_id: session id for the event
 * @type: event enum of type ipa_wlan_event
 * @mac_address: MAC address associated with the event
 *
 * This function is meant to be called from within wlan_ipa_ctx.c
 *
 * Return: QDF STATUS
 */
static QDF_STATUS __wlan_ipa_wlan_evt(qdf_netdev_t net_dev, uint8_t device_mode,
				      uint8_t sta_id, uint8_t session_id,
				      qdf_ipa_wlan_event type,
				      uint8_t *mac_addr)
{
	struct wlan_ipa_priv *ipa_ctx = gp_ipa;
	struct wlan_ipa_iface_context *iface_ctx = NULL;
	qdf_ipa_msg_meta_t meta;
	qdf_ipa_wlan_msg_t *msg;
	qdf_ipa_wlan_msg_ex_t *msg_ex = NULL;
	int i;
	QDF_STATUS status;
	uint8_t sta_session_id = WLAN_IPA_MAX_SESSION;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;

	ipa_debug("%s: EVT: %d, MAC: %pM, sta_id: %d",
		  net_dev->name, type, mac_addr, sta_id);

	if (type >= QDF_IPA_WLAN_EVENT_MAX)
		return QDF_STATUS_E_INVAL;

	if (wlan_ipa_uc_is_enabled(ipa_ctx->config) &&
	    !wlan_ipa_uc_sta_is_enabled(ipa_ctx->config) &&
	    (device_mode != QDF_SAP_MODE)) {
		return QDF_STATUS_SUCCESS;
	}

	pdev = ipa_ctx->pdev;
	psoc = wlan_pdev_get_psoc(pdev);
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, session_id,
						    WLAN_IPA_ID);
	QDF_BUG(session_id < WLAN_IPA_MAX_SESSION);

	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_IPA_ID);
	else
		ipa_err("vdev is NULL, session_id: %u", session_id);

	if (ipa_ctx->sta_connected) {
		iface_ctx = wlan_ipa_get_iface(ipa_ctx, QDF_STA_MODE);
		if (iface_ctx)
			sta_session_id = iface_ctx->session_id;
		else
			ipa_err("sta iface_ctx is NULL");
	}

	/*
	 * During IPA UC resource loading/unloading new events can be issued.
	 */
	if (wlan_ipa_uc_is_enabled(ipa_ctx->config) &&
	    (ipa_ctx->resource_loading || ipa_ctx->resource_unloading)) {
		unsigned int pending_event_count;
		struct wlan_ipa_uc_pending_event *pending_event = NULL;

		ipa_info("Event:%d IPA resource %s inprogress", type,
			 ipa_ctx->resource_loading ?
			 "load" : "unload");

		/* Wait until completion of the long/unloading */
		status = qdf_wait_for_event_completion(
				&ipa_ctx->ipa_resource_comp,
				IPA_RESOURCE_COMP_WAIT_TIME);
		if (status != QDF_STATUS_SUCCESS) {
			/*
			 * If timed out, store the events separately and
			 * handle them later.
			 */
			ipa_info("IPA resource %s timed out",
				  ipa_ctx->resource_loading ?
				  "load" : "unload");

			if (type == QDF_IPA_AP_DISCONNECT) {
				wlan_ipa_uc_offload_enable_disable(ipa_ctx,
						SIR_AP_RX_DATA_OFFLOAD,
						session_id, false);
			} else if (type == QDF_IPA_CLIENT_CONNECT_EX &&
				   wlan_sap_no_client_connected(ipa_ctx)) {
				if (wlan_sta_is_connected(ipa_ctx) &&
				    wlan_ipa_uc_is_loaded(ipa_ctx) &&
				    wlan_ipa_uc_sta_is_enabled(ipa_ctx->
							       config)) {
					wlan_ipa_uc_offload_enable_disable(
							ipa_ctx,
							SIR_STA_RX_DATA_OFFLOAD,
							sta_session_id, true);
				}
			}

			qdf_mutex_acquire(&ipa_ctx->ipa_lock);

			pending_event_count =
				qdf_list_size(&ipa_ctx->pending_event);
			if (pending_event_count >=
			    WLAN_IPA_MAX_PENDING_EVENT_COUNT) {
				ipa_info("Reached max pending evt count");
				qdf_list_remove_front(
					&ipa_ctx->pending_event,
					(qdf_list_node_t **)&pending_event);
			} else {
				pending_event =
					(struct wlan_ipa_uc_pending_event *)
					qdf_mem_malloc(sizeof(
					struct wlan_ipa_uc_pending_event));
			}

			if (!pending_event) {
				ipa_err("Pending event memory alloc fail");
				qdf_mutex_release(&ipa_ctx->ipa_lock);
				return QDF_STATUS_E_NOMEM;
			}

			pending_event->net_dev = net_dev;
			pending_event->device_mode = device_mode;
			pending_event->sta_id = sta_id;
			pending_event->session_id = session_id;
			pending_event->type = type;
			pending_event->is_loading = ipa_ctx->resource_loading;
			qdf_mem_copy(pending_event->mac_addr,
				     mac_addr, QDF_MAC_ADDR_SIZE);
			qdf_list_insert_back(&ipa_ctx->pending_event,
					     &pending_event->node);

			qdf_mutex_release(&ipa_ctx->ipa_lock);

			/* Cleanup interface */
			if (type == QDF_IPA_STA_DISCONNECT ||
			    type == QDF_IPA_AP_DISCONNECT) {
				for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
					iface_ctx = &ipa_ctx->iface_context[i];
					if (iface_ctx->dev == net_dev) {
						wlan_ipa_cleanup_iface(
								iface_ctx);
						break;
					}
				}
			}

			return QDF_STATUS_SUCCESS;
		}
		ipa_info("IPA resource %s completed",
			 ipa_ctx->resource_loading ?
			 "load" : "unload");
	}

	ipa_ctx->stats.event[type]++;

	QDF_IPA_SET_META_MSG_TYPE(&meta, type);
	switch (type) {
	case QDF_IPA_STA_CONNECT:
		qdf_mutex_acquire(&ipa_ctx->event_lock);

		/* STA already connected and without disconnect, connect again
		 * This is Roaming scenario
		 */
		if (ipa_ctx->sta_connected) {
			iface_ctx = wlan_ipa_get_iface(ipa_ctx, QDF_STA_MODE);
			if (iface_ctx)
				wlan_ipa_cleanup_iface(iface_ctx);
		}

		status = wlan_ipa_setup_iface(ipa_ctx, net_dev, device_mode,
					   sta_id, session_id);
		if (status != QDF_STATUS_SUCCESS) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			goto end;
		}

		if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config) &&
		    (ipa_ctx->sap_num_connected_sta > 0) &&
		    !ipa_ctx->sta_connected) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			wlan_ipa_uc_offload_enable_disable(ipa_ctx,
				SIR_STA_RX_DATA_OFFLOAD, session_id,
				true);
			qdf_mutex_acquire(&ipa_ctx->event_lock);
		}

		ipa_ctx->vdev_to_iface[session_id] =
				wlan_ipa_get_ifaceid(ipa_ctx, session_id);

		wlan_ipa_save_bssid_iface_ctx(ipa_ctx,
					     ipa_ctx->vdev_to_iface[session_id],
					     mac_addr);

		ipa_ctx->sta_connected = 1;

		qdf_mutex_release(&ipa_ctx->event_lock);

		ipa_debug("sta_connected=%d", ipa_ctx->sta_connected);
		break;

	case QDF_IPA_AP_CONNECT:
		qdf_mutex_acquire(&ipa_ctx->event_lock);

		/* For DFS channel we get two start_bss event (before and after
		 * CAC). Also when ACS range includes both DFS and non DFS
		 * channels, we could possibly change channel many times due to
		 * RADAR detection and chosen channel may not be a DFS channels.
		 * So dont return error here. Just discard the event.
		 */
		if (ipa_ctx->vdev_to_iface[session_id] !=
				WLAN_IPA_MAX_SESSION) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			return 0;
		}

		status = wlan_ipa_setup_iface(ipa_ctx, net_dev, device_mode,
					   sta_id, session_id);
		if (status != QDF_STATUS_SUCCESS) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			ipa_err("%s: Evt: %d, Interface setup failed",
				msg_ex->name, QDF_IPA_MSG_META_MSG_TYPE(&meta));
			goto end;
		}

		if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			wlan_ipa_uc_offload_enable_disable(ipa_ctx,
				SIR_AP_RX_DATA_OFFLOAD, session_id, true);
			qdf_mutex_acquire(&ipa_ctx->event_lock);
		}

		ipa_ctx->vdev_to_iface[session_id] =
				wlan_ipa_get_ifaceid(ipa_ctx, session_id);
		qdf_mutex_release(&ipa_ctx->event_lock);
		break;

	case QDF_IPA_STA_DISCONNECT:
		qdf_mutex_acquire(&ipa_ctx->event_lock);

		if (!ipa_ctx->sta_connected) {
			struct wlan_ipa_iface_context *iface;

			qdf_mutex_release(&ipa_ctx->event_lock);
			ipa_info("%s: Evt: %d, STA already disconnected",
				 msg_ex->name,
				 QDF_IPA_MSG_META_MSG_TYPE(&meta));

			iface = wlan_ipa_get_iface(ipa_ctx, QDF_STA_MODE);
			if (iface && (iface->dev == net_dev))
				wlan_ipa_cleanup_iface(iface);

			return QDF_STATUS_E_INVAL;
		}

		ipa_ctx->sta_connected = 0;

		if (!wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			ipa_debug("%s: IPA UC OFFLOAD NOT ENABLED",
				  msg_ex->name);
		} else {
			/* Disable IPA UC TX PIPE when STA disconnected */
			if ((ipa_ctx->num_iface == 1) &&
			    wlan_ipa_is_fw_wdi_activated(ipa_ctx) &&
			    !ipa_ctx->ipa_pipes_down &&
			    (ipa_ctx->resource_unloading == false)) {
				if (cds_is_driver_unloading()) {
					/*
					 * We disable WDI pipes directly here
					 * since IPA_OPCODE_TX/RX_SUSPEND
					 * message will not be processed when
					 * unloading WLAN driver is in progress
					 */
					wlan_ipa_uc_disable_pipes(ipa_ctx);
				} else {
					wlan_ipa_uc_handle_last_discon(ipa_ctx);
				}
			}
		}

		if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config) &&
		    (ipa_ctx->sap_num_connected_sta > 0)) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			wlan_ipa_uc_offload_enable_disable(ipa_ctx,
				SIR_STA_RX_DATA_OFFLOAD, session_id, false);
			qdf_mutex_acquire(&ipa_ctx->event_lock);
		}

		ipa_ctx->vdev_to_iface[session_id] = WLAN_IPA_MAX_SESSION;

		for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
			iface_ctx = &ipa_ctx->iface_context[i];

			if (iface_ctx->dev == net_dev)
				break;
		}
		if (i < WLAN_IPA_MAX_IFACE)
			wlan_ipa_cleanup_iface(iface_ctx);

		qdf_mutex_release(&ipa_ctx->event_lock);

		ipa_debug("sta_connected=%d", ipa_ctx->sta_connected);
		break;

	case QDF_IPA_AP_DISCONNECT:
		qdf_mutex_acquire(&ipa_ctx->event_lock);

		if ((ipa_ctx->num_iface == 1) &&
		    wlan_ipa_is_fw_wdi_activated(ipa_ctx) &&
		    !ipa_ctx->ipa_pipes_down &&
		    (ipa_ctx->resource_unloading == false)) {
			if (cds_is_driver_unloading()) {
				/*
				 * We disable WDI pipes directly here since
				 * IPA_OPCODE_TX/RX_SUSPEND message will not be
				 * processed when unloading WLAN driver is in
				 * progress
				 */
				wlan_ipa_uc_disable_pipes(ipa_ctx);
			} else {
				/*
				 * This shouldn't happen :
				 * No interface left but WDI pipes are still
				 * active - force close WDI pipes
				 */
				ipa_err("No interface left but WDI pipes are still active");
				wlan_ipa_uc_handle_last_discon(ipa_ctx);
			}
		}

		if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			wlan_ipa_uc_offload_enable_disable(ipa_ctx,
				SIR_AP_RX_DATA_OFFLOAD, session_id, false);
			qdf_mutex_acquire(&ipa_ctx->event_lock);
			ipa_ctx->vdev_to_iface[session_id] =
				WLAN_IPA_MAX_SESSION;
		}

		for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
			iface_ctx = &ipa_ctx->iface_context[i];
			if (iface_ctx->dev == net_dev) {
				wlan_ipa_cleanup_iface(iface_ctx);
				break;
			}
		}

		qdf_mutex_release(&ipa_ctx->event_lock);
		break;

	case QDF_IPA_CLIENT_CONNECT_EX:
		if (!wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			ipa_debug("%s: Evt: %d, IPA UC OFFLOAD NOT ENABLED",
				  net_dev->name, type);
			return QDF_STATUS_SUCCESS;
		}

		qdf_mutex_acquire(&ipa_ctx->event_lock);
		if (wlan_ipa_uc_find_add_assoc_sta(ipa_ctx, true, sta_id,
						   mac_addr)) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			ipa_err("%s: STA ID %d found", net_dev->name, sta_id);
			return QDF_STATUS_SUCCESS;
		}

		/* Enable IPA UC Data PIPEs when first STA connected */
		if (ipa_ctx->sap_num_connected_sta == 0 &&
				ipa_ctx->uc_loaded == true) {

			if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config) &&
			    ipa_ctx->sta_connected) {
				qdf_mutex_release(&ipa_ctx->event_lock);
				wlan_ipa_uc_offload_enable_disable(ipa_ctx,
							SIR_STA_RX_DATA_OFFLOAD,
							sta_session_id, true);
				qdf_mutex_acquire(&ipa_ctx->event_lock);
			}

			status = wlan_ipa_uc_handle_first_con(ipa_ctx);
			if (status != QDF_STATUS_SUCCESS) {
				ipa_info("%s: handle 1st con fail",
					 net_dev->name);

				if (wlan_ipa_uc_sta_is_enabled(
					ipa_ctx->config) &&
				    ipa_ctx->sta_connected) {
					qdf_mutex_release(&ipa_ctx->event_lock);
					wlan_ipa_uc_offload_enable_disable(
							ipa_ctx,
							SIR_STA_RX_DATA_OFFLOAD,
							sta_session_id, false);
				} else {
					qdf_mutex_release(&ipa_ctx->event_lock);
				}

				return status;
			}
			ipa_info("first sap client connected");
		}

		ipa_ctx->sap_num_connected_sta++;

		qdf_mutex_release(&ipa_ctx->event_lock);

		QDF_IPA_SET_META_MSG_TYPE(&meta, type);
		QDF_IPA_MSG_META_MSG_LEN(&meta) =
			(sizeof(qdf_ipa_wlan_msg_ex_t) +
				sizeof(qdf_ipa_wlan_hdr_attrib_val_t));
		msg_ex = qdf_mem_malloc(QDF_IPA_MSG_META_MSG_LEN(&meta));

		if (msg_ex == NULL) {
			ipa_err("msg_ex allocation failed");
			return QDF_STATUS_E_NOMEM;
		}
		strlcpy(msg_ex->name, net_dev->name,
			IPA_RESOURCE_NAME_MAX);
		msg_ex->num_of_attribs = 1;
		msg_ex->attribs[0].attrib_type = WLAN_HDR_ATTRIB_MAC_ADDR;
		if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			msg_ex->attribs[0].offset =
				WLAN_IPA_UC_WLAN_HDR_DES_MAC_OFFSET;
		} else {
			msg_ex->attribs[0].offset =
				WLAN_IPA_WLAN_HDR_DES_MAC_OFFSET;
		}
		memcpy(msg_ex->attribs[0].u.mac_addr, mac_addr,
		       IPA_MAC_ADDR_SIZE);

		if (qdf_ipa_send_msg(&meta, msg_ex, wlan_ipa_msg_free_fn)) {
			ipa_info("%s: Evt: %d send ipa msg fail",
				 net_dev->name, type);
			qdf_mem_free(msg_ex);
			return QDF_STATUS_E_FAILURE;
		}
		ipa_ctx->stats.num_send_msg++;

		ipa_debug("sap_num_connected_sta=%d",
			  ipa_ctx->sap_num_connected_sta);

		return QDF_STATUS_SUCCESS;

	case WLAN_CLIENT_DISCONNECT:
		if (!wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			ipa_debug("%s: IPA UC OFFLOAD NOT ENABLED",
				  msg_ex->name);
			return QDF_STATUS_SUCCESS;
		}

		qdf_mutex_acquire(&ipa_ctx->event_lock);
		if (!ipa_ctx->sap_num_connected_sta) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			ipa_debug("%s: Evt: %d, Client already disconnected",
				  msg_ex->name,
				  QDF_IPA_MSG_META_MSG_TYPE(&meta));

			return QDF_STATUS_SUCCESS;
		}
		if (!wlan_ipa_uc_find_add_assoc_sta(ipa_ctx, false,
						    sta_id, mac_addr)) {
			qdf_mutex_release(&ipa_ctx->event_lock);
			ipa_debug("%s: STA ID %d NOT found, not valid",
				  msg_ex->name, sta_id);

			return QDF_STATUS_SUCCESS;
		}
		ipa_ctx->sap_num_connected_sta--;

		/* Disable IPA UC TX PIPE when last STA disconnected */
		if (!ipa_ctx->sap_num_connected_sta &&
				ipa_ctx->uc_loaded == true) {
			if ((false == ipa_ctx->resource_unloading) &&
			    wlan_ipa_is_fw_wdi_activated(ipa_ctx) &&
			    !ipa_ctx->ipa_pipes_down) {
				if (cds_is_driver_unloading()) {
					/*
					 * We disable WDI pipes directly here
					 * since IPA_OPCODE_TX/RX_SUSPEND
					 * message will not be processed when
					 * unloading WLAN driver is in progress
					 */
					wlan_ipa_uc_disable_pipes(ipa_ctx);
				} else {
					wlan_ipa_uc_handle_last_discon(ipa_ctx);
				}
				ipa_info("last sap client disconnected");
			}

			if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config) &&
			    ipa_ctx->sta_connected) {
				qdf_mutex_release(&ipa_ctx->event_lock);
				wlan_ipa_uc_offload_enable_disable(ipa_ctx,
							SIR_STA_RX_DATA_OFFLOAD,
							sta_session_id, false);
			} else {
				qdf_mutex_release(&ipa_ctx->event_lock);
			}
		} else {
			qdf_mutex_release(&ipa_ctx->event_lock);
		}

		ipa_debug("sap_num_connected_sta=%d",
			  ipa_ctx->sap_num_connected_sta);
		break;

	default:
		return QDF_STATUS_SUCCESS;
	}

	QDF_IPA_MSG_META_MSG_LEN(&meta) = sizeof(qdf_ipa_wlan_msg_t);
	msg = qdf_mem_malloc(QDF_IPA_MSG_META_MSG_LEN(&meta));
	if (!msg) {
		ipa_err("msg allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	QDF_IPA_SET_META_MSG_TYPE(&meta, type);
	strlcpy(QDF_IPA_WLAN_MSG_NAME(msg), net_dev->name,
		IPA_RESOURCE_NAME_MAX);
	qdf_mem_copy(QDF_IPA_WLAN_MSG_MAC_ADDR(msg), mac_addr, QDF_NET_ETH_LEN);

	ipa_debug("%s: Evt: %d", QDF_IPA_WLAN_MSG_NAME(msg),
		  QDF_IPA_MSG_META_MSG_TYPE(&meta));

	if (qdf_ipa_send_msg(&meta, msg, wlan_ipa_msg_free_fn)) {

		ipa_err("%s: Evt: %d fail",
			QDF_IPA_WLAN_MSG_NAME(msg),
			QDF_IPA_MSG_META_MSG_TYPE(&meta));
		qdf_mem_free(msg);

		return QDF_STATUS_E_FAILURE;
	}

	ipa_ctx->stats.num_send_msg++;

end:
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_host_to_ipa_wlan_event() - convert wlan_ipa_wlan_event to ipa_wlan_event
 * @wlan_ipa_event_type: event to be converted to an ipa_wlan_event
 *
 * Return: qdf_ipa_wlan_event representing the wlan_ipa_wlan_event
 */
static qdf_ipa_wlan_event
wlan_host_to_ipa_wlan_event(enum wlan_ipa_wlan_event wlan_ipa_event_type)
{
	qdf_ipa_wlan_event ipa_event;

	switch (wlan_ipa_event_type) {
	case WLAN_IPA_CLIENT_CONNECT:
		ipa_event = QDF_IPA_CLIENT_CONNECT;
		break;
	case WLAN_IPA_CLIENT_DISCONNECT:
		ipa_event = QDF_IPA_CLIENT_DISCONNECT;
		break;
	case WLAN_IPA_AP_CONNECT:
		ipa_event = QDF_IPA_AP_CONNECT;
		break;
	case WLAN_IPA_AP_DISCONNECT:
		ipa_event = QDF_IPA_AP_DISCONNECT;
		break;
	case WLAN_IPA_STA_CONNECT:
		ipa_event = QDF_IPA_STA_CONNECT;
		break;
	case WLAN_IPA_STA_DISCONNECT:
		ipa_event = QDF_IPA_STA_DISCONNECT;
		break;
	case WLAN_IPA_CLIENT_CONNECT_EX:
		ipa_event = QDF_IPA_CLIENT_CONNECT_EX;
		break;
	case WLAN_IPA_WLAN_EVENT_MAX:
	default:
		ipa_event =  QDF_IPA_WLAN_EVENT_MAX;
		break;
	}

	return ipa_event;
}

/**
 * wlan_ipa_wlan_evt() - SSR wrapper for __wlan_ipa_wlan_evt
 * @net_dev: Interface net device
 * @device_mode: Net interface device mode
 * @sta_id: station id for the event
 * @session_id: session id for the event
 * @ipa_event_type: event enum of type wlan_ipa_wlan_event
 * @mac_address: MAC address associated with the event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_wlan_evt(qdf_netdev_t net_dev, uint8_t device_mode,
		      uint8_t sta_id, uint8_t session_id,
		      enum wlan_ipa_wlan_event ipa_event_type,
		      uint8_t *mac_addr)
{
	qdf_ipa_wlan_event type = wlan_host_to_ipa_wlan_event(ipa_event_type);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Data path offload only support for STA and SAP mode */
	if ((device_mode == QDF_STA_MODE) ||
	    (device_mode == QDF_SAP_MODE))
		status  = __wlan_ipa_wlan_evt(net_dev, device_mode, sta_id,
					      session_id, type, mac_addr);

	return status;
}

/**
 * wlan_ipa_uc_proc_pending_event() - Process IPA uC pending events
 * @ipa_ctx: Global IPA IPA context
 * @is_loading: Indicate if invoked during loading
 *
 * Return: None
 */
static void
wlan_ipa_uc_proc_pending_event(struct wlan_ipa_priv *ipa_ctx, bool is_loading)
{
	unsigned int pending_event_count;
	struct wlan_ipa_uc_pending_event *pending_event = NULL;

	pending_event_count = qdf_list_size(&ipa_ctx->pending_event);
	ipa_debug("Pending Event Count %d",  pending_event_count);
	if (!pending_event_count) {
		ipa_debug("No Pending Event");
		return;
	}

	qdf_list_remove_front(&ipa_ctx->pending_event,
			(qdf_list_node_t **)&pending_event);
	while (pending_event != NULL) {
		struct wlan_objmgr_pdev *pdev = ipa_ctx->pdev;
		struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
		struct wlan_objmgr_vdev *vdev =
				wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
					pending_event->session_id,
					WLAN_IPA_ID);
		if (pending_event->is_loading == is_loading && vdev) {
			__wlan_ipa_wlan_evt(pending_event->net_dev,
					   pending_event->device_mode,
					   pending_event->sta_id,
					   pending_event->session_id,
					   pending_event->type,
					   pending_event->mac_addr);
		}

		if (vdev)
			wlan_objmgr_vdev_release_ref(vdev, WLAN_IPA_ID);
		qdf_mem_free(pending_event);
		pending_event = NULL;
		qdf_list_remove_front(&ipa_ctx->pending_event,
				      (qdf_list_node_t **)&pending_event);
	}
}

/**
 * wlan_ipa_free_tx_desc_list() - Free IPA Tx desc list
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
static inline void wlan_ipa_free_tx_desc_list(struct wlan_ipa_priv *ipa_ctx)
{
	struct wlan_ipa_tx_desc *tmp_desc;
	qdf_ipa_rx_data_t *ipa_tx_desc;
	qdf_list_node_t *node;

	while (ipa_ctx->tx_desc_list.count &&
	       qdf_list_remove_front(&ipa_ctx->tx_desc_list, &node) ==
							QDF_STATUS_SUCCESS) {
		tmp_desc = qdf_container_of(node, struct wlan_ipa_tx_desc,
					    node);

		ipa_tx_desc = tmp_desc->ipa_tx_desc_ptr;
		if (ipa_tx_desc)
			qdf_ipa_free_skb(ipa_tx_desc);

		qdf_mem_free(tmp_desc);
		tmp_desc = NULL;
	}
}

/**
 * wlan_ipa_alloc_tx_desc_list() - Allocate IPA Tx desc list
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_ipa_alloc_tx_desc_list(struct wlan_ipa_priv *ipa_ctx)
{
	int i;
	uint32_t max_desc_cnt;
	struct wlan_ipa_tx_desc *tmp_desc;

	max_desc_cnt = ipa_ctx->config->txbuf_count;

	qdf_list_create(&ipa_ctx->tx_desc_list, max_desc_cnt);

	qdf_spin_lock_bh(&ipa_ctx->q_lock);
	for (i = 0; i < max_desc_cnt; i++) {
		tmp_desc = qdf_mem_malloc(sizeof(*tmp_desc));

		if (!tmp_desc) {
			qdf_spin_unlock_bh(&ipa_ctx->q_lock);
			goto alloc_fail;
		}

		tmp_desc->id = i;
		tmp_desc->ipa_tx_desc_ptr = NULL;
		qdf_list_insert_back(&ipa_ctx->tx_desc_list,
				     &tmp_desc->node);
		tmp_desc++;
	}

	ipa_ctx->stats.num_tx_desc_q_cnt = 0;
	ipa_ctx->stats.num_tx_desc_error = 0;

	qdf_spin_unlock_bh(&ipa_ctx->q_lock);

	return QDF_STATUS_SUCCESS;

alloc_fail:
	wlan_ipa_free_tx_desc_list(ipa_ctx);
	return QDF_STATUS_E_NOMEM;

}

#ifndef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * wlan_ipa_setup_tx_sys_pipe() - Setup IPA Tx system pipes
 * @ipa_ctx: Global IPA IPA context
 * @desc_fifo_sz: Number of descriptors
 *
 * Return: 0 on success, negative errno on error
 */
static int wlan_ipa_setup_tx_sys_pipe(struct wlan_ipa_priv *ipa_ctx,
				     int32_t desc_fifo_sz)
{
	int i, ret = 0;
	qdf_ipa_sys_connect_params_t *ipa;

	/*setup TX pipes */
	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		ipa = &ipa_ctx->sys_pipe[i].ipa_sys_params;

		ipa->client = wlan_ipa_iface_2_client[i].cons_client;
		ipa->desc_fifo_sz = desc_fifo_sz;
		ipa->priv = &ipa_ctx->iface_context[i];
		ipa->notify = wlan_ipa_i2w_cb;

		if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config)) {
			ipa->ipa_ep_cfg.hdr.hdr_len =
				WLAN_IPA_UC_WLAN_TX_HDR_LEN;
			ipa->ipa_ep_cfg.nat.nat_en = IPA_BYPASS_NAT;
			ipa->ipa_ep_cfg.hdr.hdr_ofst_pkt_size_valid = 1;
			ipa->ipa_ep_cfg.hdr.hdr_ofst_pkt_size = 0;
			ipa->ipa_ep_cfg.hdr.hdr_additional_const_len =
				WLAN_IPA_UC_WLAN_8023_HDR_SIZE;
			ipa->ipa_ep_cfg.hdr_ext.hdr_little_endian = true;
		} else {
			ipa->ipa_ep_cfg.hdr.hdr_len = WLAN_IPA_WLAN_TX_HDR_LEN;
		}
		ipa->ipa_ep_cfg.mode.mode = IPA_BASIC;

		ret = wlan_ipa_wdi_setup_sys_pipe(ipa_ctx, ipa,
				&ipa_ctx->sys_pipe[i].conn_hdl);
		if (ret) {
			ipa_err("Failed for pipe %d ret: %d", i, ret);
			return ret;
		}
		ipa_ctx->sys_pipe[i].conn_hdl_valid = 1;
	}

	return ret;
}
#else
/**
 * wlan_ipa_setup_tx_sys_pipe() - Setup IPA Tx system pipes
 * @ipa_ctx: Global IPA IPA context
 * @desc_fifo_sz: Number of descriptors
 *
 * Return: 0 on success, negative errno on error
 */
static int wlan_ipa_setup_tx_sys_pipe(struct wlan_ipa_priv *ipa_ctx,
				     int32_t desc_fifo_sz)
{
	/*
	 * The Tx system pipes are not needed for MCC when TX_FLOW_CONTROL_V2
	 * is enabled, where per vdev descriptors are supported in firmware.
	 */
	return 0;
}
#endif

/**
 * wlan_ipa_setup_rx_sys_pipe() - Setup IPA Rx system pipes
 * @ipa_ctx: Global IPA IPA context
 * @desc_fifo_sz: Number of descriptors
 *
 * Return: 0 on success, negative errno on error
 */
static int wlan_ipa_setup_rx_sys_pipe(struct wlan_ipa_priv *ipa_ctx,
				     int32_t desc_fifo_sz)
{
	int ret = 0;
	qdf_ipa_sys_connect_params_t *ipa;

	/*
	 * Hard code it here, this can be extended if in case
	 * PROD pipe is also per interface.
	 * Right now there is no advantage of doing this.
	 */
	ipa = &ipa_ctx->sys_pipe[WLAN_IPA_RX_PIPE].ipa_sys_params;

	ipa->client = IPA_CLIENT_WLAN1_PROD;

	ipa->desc_fifo_sz = desc_fifo_sz;
	ipa->priv = ipa_ctx;
	ipa->notify = wlan_ipa_w2i_cb;

	ipa->ipa_ep_cfg.nat.nat_en = IPA_BYPASS_NAT;
	ipa->ipa_ep_cfg.hdr.hdr_len = WLAN_IPA_WLAN_RX_HDR_LEN;
	ipa->ipa_ep_cfg.hdr.hdr_ofst_metadata_valid = 1;
	ipa->ipa_ep_cfg.mode.mode = IPA_BASIC;

	ret = qdf_ipa_setup_sys_pipe(ipa,
			&ipa_ctx->sys_pipe[WLAN_IPA_RX_PIPE].conn_hdl);
	if (ret) {
		ipa_err("Failed for RX pipe: %d", ret);
		return ret;
	}
	ipa_ctx->sys_pipe[WLAN_IPA_RX_PIPE].conn_hdl_valid = 1;

	return ret;
}

/**
 * wlan_ipa_teardown_sys_pipe() - Tear down all IPA Sys pipes
 * @ipa_ctx: Global IPA IPA context
 *
 * Return: None
 */
static void wlan_ipa_teardown_sys_pipe(struct wlan_ipa_priv *ipa_ctx)
{
	int ret, i;

	if (!ipa_ctx)
		return;

	for (i = 0; i < WLAN_IPA_MAX_SYSBAM_PIPE; i++) {
		if (ipa_ctx->sys_pipe[i].conn_hdl_valid) {
			ret = wlan_ipa_wdi_teardown_sys_pipe(ipa_ctx,
							     ipa_ctx->sys_pipe[i].conn_hdl);
			if (ret)
				ipa_err("Failed:%d", ret);

			ipa_ctx->sys_pipe[i].conn_hdl_valid = 0;
		}
	}

	wlan_ipa_free_tx_desc_list(ipa_ctx);
}

/**
 * wlan_ipa_setup_sys_pipe() - Setup all IPA system pipes
 * @ipa_ctx: Global IPA IPA context
 *
 * Return: 0 on success, negative errno on error
 */
static int wlan_ipa_setup_sys_pipe(struct wlan_ipa_priv *ipa_ctx)
{
	int ret = 0;
	uint32_t desc_fifo_sz;

	/* The maximum number of descriptors that can be provided to a BAM at
	 * once is one less than the total number of descriptors that the buffer
	 * can contain.
	 * If max_num_of_descriptors = (BAM_PIPE_DESCRIPTOR_FIFO_SIZE / sizeof
	 * (SPS_DESCRIPTOR)), then (max_num_of_descriptors - 1) descriptors can
	 * be provided at once.
	 * Because of above requirement, one extra descriptor will be added to
	 * make sure hardware always has one descriptor.
	 */
	desc_fifo_sz = ipa_ctx->config->desc_size
		       + SPS_DESC_SIZE;

	ret = wlan_ipa_setup_tx_sys_pipe(ipa_ctx, desc_fifo_sz);
	if (ret) {
		ipa_err("Failed for TX pipe: %d", ret);
		goto setup_sys_pipe_fail;
	}

	if (!wlan_ipa_uc_sta_is_enabled(ipa_ctx->config)) {
		ret = wlan_ipa_setup_rx_sys_pipe(ipa_ctx, desc_fifo_sz);
		if (ret) {
			ipa_err("Failed for RX pipe: %d", ret);
			goto setup_sys_pipe_fail;
		}
	}

       /* Allocate free Tx desc list */
	ret = wlan_ipa_alloc_tx_desc_list(ipa_ctx);
	if (ret)
		goto setup_sys_pipe_fail;

	return ret;

setup_sys_pipe_fail:
	wlan_ipa_teardown_sys_pipe(ipa_ctx);

	return ret;
}

#ifndef QCA_LL_TX_FLOW_CONTROL_V2
QDF_STATUS wlan_ipa_send_mcc_scc_msg(struct wlan_ipa_priv *ipa_ctx,
				     bool mcc_mode)
{
	qdf_ipa_msg_meta_t meta;
	qdf_ipa_wlan_msg_t *msg;
	int ret;

	if (!wlan_ipa_uc_sta_is_enabled(ipa_ctx->config))
		return QDF_STATUS_SUCCESS;

	/* Send SCC/MCC Switching event to IPA */
	QDF_IPA_MSG_META_MSG_LEN(&meta) = sizeof(*msg);
	msg = qdf_mem_malloc(QDF_IPA_MSG_META_MSG_LEN(&meta));
	if (msg == NULL) {
		ipa_err("msg allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	if (mcc_mode) {
		QDF_IPA_SET_META_MSG_TYPE(&meta, QDF_SWITCH_TO_MCC);
		ipa_ctx->stats.event[QDF_SWITCH_TO_MCC]++;
	} else {
		QDF_IPA_SET_META_MSG_TYPE(&meta, QDF_SWITCH_TO_SCC);
		ipa_ctx->stats.event[QDF_SWITCH_TO_SCC]++;
	}

	WLAN_IPA_LOG(QDF_TRACE_LEVEL_DEBUG,
		    "ipa_send_msg(Evt:%d)",
		    QDF_IPA_MSG_META_MSG_TYPE(&meta));

	ret = qdf_ipa_send_msg(&meta, msg, wlan_ipa_msg_free_fn);

	if (ret) {
		ipa_err("ipa_send_msg(Evt:%d) - fail=%d",
			QDF_IPA_MSG_META_MSG_TYPE(&meta), ret);
		qdf_mem_free(msg);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static void wlan_ipa_mcc_work_handler(void *data)
{
	struct wlan_ipa_priv *ipa_ctx = (struct wlan_ipa_priv *)data;

	wlan_ipa_send_mcc_scc_msg(ipa_ctx, ipa_ctx->mcc_mode);
}
#endif

/**
 * wlan_ipa_setup() - IPA initialization function
 * @ipa_ctx: IPA context
 * @ipa_cfg: IPA config
 *
 * Allocate ipa_ctx resources, ipa pipe resource and register
 * wlan interface with IPA module.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS wlan_ipa_setup(struct wlan_ipa_priv *ipa_ctx,
			  struct wlan_ipa_config *ipa_cfg)
{
	int ret, i;
	struct wlan_ipa_iface_context *iface_context = NULL;
	QDF_STATUS status;

	ipa_debug("enter");

	gp_ipa = ipa_ctx;
	ipa_ctx->num_iface = 0;
	ipa_ctx->config = ipa_cfg;

	wlan_ipa_wdi_get_wdi_version(ipa_ctx);

	/* Create the interface context */
	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_context = &ipa_ctx->iface_context[i];
		iface_context->ipa_ctx = ipa_ctx;
		iface_context->cons_client =
			wlan_ipa_iface_2_client[i].cons_client;
		iface_context->prod_client =
			wlan_ipa_iface_2_client[i].prod_client;
		iface_context->iface_id = i;
		iface_context->dev = NULL;
		iface_context->device_mode = QDF_MAX_NO_OF_MODE;
		iface_context->tl_context = NULL;
		qdf_spinlock_create(&iface_context->interface_lock);
	}

	qdf_create_work(0, &ipa_ctx->pm_work, wlan_ipa_pm_flush, ipa_ctx);
	qdf_spinlock_create(&ipa_ctx->pm_lock);
	qdf_spinlock_create(&ipa_ctx->q_lock);
	qdf_spinlock_create(&ipa_ctx->pipes_down_lock);
	qdf_nbuf_queue_init(&ipa_ctx->pm_queue_head);
	qdf_list_create(&ipa_ctx->pending_event, 1000);
	qdf_mutex_create(&ipa_ctx->event_lock);
	qdf_mutex_create(&ipa_ctx->ipa_lock);

	status = wlan_ipa_wdi_setup_rm(ipa_ctx);
	if (status != QDF_STATUS_SUCCESS)
		goto fail_setup_rm;

	for (i = 0; i < WLAN_IPA_MAX_SYSBAM_PIPE; i++)
		qdf_mem_zero(&ipa_ctx->sys_pipe[i],
			     sizeof(struct wlan_ipa_sys_pipe));

	if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
		qdf_mem_zero(&ipa_ctx->stats, sizeof(ipa_ctx->stats));
		ipa_ctx->sap_num_connected_sta = 0;
		ipa_ctx->ipa_tx_packets_diff = 0;
		ipa_ctx->ipa_rx_packets_diff = 0;
		ipa_ctx->ipa_p_tx_packets = 0;
		ipa_ctx->ipa_p_rx_packets = 0;
		ipa_ctx->resource_loading = false;
		ipa_ctx->resource_unloading = false;
		ipa_ctx->sta_connected = 0;
		ipa_ctx->ipa_pipes_down = true;
		ipa_ctx->pipes_down_in_progress = false;
		ipa_ctx->wdi_enabled = false;

		status = wlan_ipa_wdi_init(ipa_ctx);
		if (status == QDF_STATUS_SUCCESS) {
			/* Setup IPA system pipes */
			if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config)) {
				ret = wlan_ipa_setup_sys_pipe(ipa_ctx);
				if (ret)
					goto ipa_wdi_destroy;

				qdf_create_work(0, &ipa_ctx->mcc_work,
						wlan_ipa_mcc_work_handler,
						ipa_ctx);
			}
		} else if (status == QDF_STATUS_E_BUSY) {
			ret = wlan_ipa_uc_send_wdi_control_msg(false);
			if (ret) {
				ipa_err("IPA WDI msg send failed: ret=%d", ret);
				goto ipa_wdi_destroy;
			}
		} else {
			ipa_err("IPA WDI init failed: ret=%d", status);
			goto ipa_wdi_destroy;
		}
	} else {
		ret = wlan_ipa_setup_sys_pipe(ipa_ctx);
		if (ret)
			goto ipa_wdi_destroy;
	}

	qdf_event_create(&ipa_ctx->ipa_resource_comp);

	ipa_debug("exit: success");

	return QDF_STATUS_SUCCESS;

ipa_wdi_destroy:
	wlan_ipa_wdi_destroy_rm(ipa_ctx);

fail_setup_rm:
	qdf_spinlock_destroy(&ipa_ctx->pm_lock);
	qdf_spinlock_destroy(&ipa_ctx->q_lock);
	qdf_spinlock_destroy(&ipa_ctx->pipes_down_lock);
	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_context = &ipa_ctx->iface_context[i];
		qdf_spinlock_destroy(&iface_context->interface_lock);
	}
	qdf_mutex_destroy(&ipa_ctx->event_lock);
	qdf_mutex_destroy(&ipa_ctx->ipa_lock);
	qdf_list_destroy(&ipa_ctx->pending_event);
	gp_ipa = NULL;
	ipa_debug("exit: fail");

	return QDF_STATUS_E_FAILURE;
}

void wlan_ipa_flush(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_nbuf_t skb;
	struct wlan_ipa_pm_tx_cb *pm_tx_cb;

	if (!wlan_ipa_is_enabled(ipa_ctx->config))
		return;

	qdf_cancel_work(&ipa_ctx->pm_work);

	qdf_spin_lock_bh(&ipa_ctx->pm_lock);

	while (((skb = qdf_nbuf_queue_remove(&ipa_ctx->pm_queue_head))
	       != NULL)) {
		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

		pm_tx_cb = (struct wlan_ipa_pm_tx_cb *)skb->cb;

		if (pm_tx_cb->exception) {
			dev_kfree_skb_any(skb);
		} else {
			if (pm_tx_cb->ipa_tx_desc)
				ipa_free_skb(pm_tx_cb->ipa_tx_desc);
		}

		qdf_spin_lock_bh(&ipa_ctx->pm_lock);
	}
	qdf_spin_unlock_bh(&ipa_ctx->pm_lock);
}

QDF_STATUS wlan_ipa_cleanup(struct wlan_ipa_priv *ipa_ctx)
{
	struct wlan_ipa_iface_context *iface_context;
	int i;

	if (!wlan_ipa_uc_is_enabled(ipa_ctx->config))
		wlan_ipa_teardown_sys_pipe(ipa_ctx);

	/* Teardown IPA sys_pipe for MCC */
	if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config)) {
		wlan_ipa_teardown_sys_pipe(ipa_ctx);
		qdf_cancel_work(&ipa_ctx->mcc_work);
	}

	wlan_ipa_wdi_destroy_rm(ipa_ctx);

	wlan_ipa_flush(ipa_ctx);

	qdf_spinlock_destroy(&ipa_ctx->pm_lock);
	qdf_spinlock_destroy(&ipa_ctx->q_lock);
	qdf_spinlock_destroy(&ipa_ctx->pipes_down_lock);

	/* destroy the interface lock */
	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_context = &ipa_ctx->iface_context[i];
		qdf_spinlock_destroy(&iface_context->interface_lock);
	}

	if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
		wlan_ipa_wdi_cleanup();
		qdf_mutex_destroy(&ipa_ctx->event_lock);
		qdf_mutex_destroy(&ipa_ctx->ipa_lock);
		qdf_list_destroy(&ipa_ctx->pending_event);

	}

	gp_ipa = NULL;

	return QDF_STATUS_SUCCESS;
}

struct wlan_ipa_iface_context
*wlan_ipa_get_iface(struct wlan_ipa_priv *ipa_ctx, uint8_t mode)
{
	struct wlan_ipa_iface_context *iface_ctx = NULL;
	int i;

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_ctx = &ipa_ctx->iface_context[i];

		if (iface_ctx->device_mode == mode)
			return iface_ctx;
	}

	return NULL;
}

void wlan_ipa_set_mcc_mode(struct wlan_ipa_priv *ipa_ctx, bool mcc_mode)
{
	if (!wlan_ipa_uc_sta_is_enabled(ipa_ctx->config))
		return;

	if (ipa_ctx->mcc_mode == mcc_mode)
		return;

	ipa_ctx->mcc_mode = mcc_mode;
	qdf_sched_work(0, &ipa_ctx->mcc_work);
}

/**
 * wlan_ipa_uc_loaded_handler() - Process IPA uC loaded indication
 * @ipa_ctx: ipa ipa local context
 *
 * Will handle IPA UC image loaded indication comes from IPA kernel
 *
 * Return: None
 */
static void wlan_ipa_uc_loaded_handler(struct wlan_ipa_priv *ipa_ctx)
{
	struct wlan_objmgr_pdev *pdev = ipa_ctx->pdev;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	qdf_device_t qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	QDF_STATUS status;

	ipa_info("UC READY");

	if (!qdf_dev) {
		ipa_err("qdf device is NULL!");
		return;
	}

	if (true == ipa_ctx->uc_loaded) {
		ipa_info("UC already loaded");
		return;
	}

	if (!qdf_dev) {
		ipa_err("qdf_dev is null");
		return;
	}

	if (wlan_ipa_uc_sta_is_enabled(ipa_ctx->config)) {
		/* Setup IPA system pipes */
		status = wlan_ipa_setup_sys_pipe(ipa_ctx);
		if (status) {
			ipa_err("Fail to setup sys pipes (status=%d)", status);
			return;
		}
		qdf_create_work(0, &ipa_ctx->mcc_work,
				wlan_ipa_mcc_work_handler, ipa_ctx);
	}

	/* Connect pipe */
	status = wlan_ipa_wdi_setup(ipa_ctx, qdf_dev);
	if (status) {
		ipa_err("Failure to setup IPA pipes (status=%d)",
			status);
		return;
	}

	cdp_ipa_set_doorbell_paddr(ipa_ctx->dp_soc, ipa_ctx->dp_pdev);

	/* If already any STA connected, enable IPA/FW PIPEs */
	if (ipa_ctx->sap_num_connected_sta) {
		ipa_debug("Client already connected, enable IPA/FW PIPEs");
		wlan_ipa_uc_handle_first_con(ipa_ctx);
	}
}

/**
 * wlan_ipa_uc_op_cb() - IPA uC operation callback
 * @op_msg: operation message received from firmware
 * @usr_ctxt: user context registered with TL (we register the IPA Global
 *	context)
 *
 * Return: None
 */
static void wlan_ipa_uc_op_cb(struct op_msg_type *op_msg,
			      struct wlan_ipa_priv *ipa_ctx)
{
	struct op_msg_type *msg = op_msg;
	struct ipa_uc_fw_stats *uc_fw_stat;

	if (!op_msg) {
		ipa_err("INVALID ARG");
		return;
	}

	if (msg->op_code >= WLAN_IPA_UC_OPCODE_MAX) {
		ipa_err("INVALID OPCODE %d",  msg->op_code);
		qdf_mem_free(op_msg);
		return;
	}

	ipa_debug("OPCODE=%d", msg->op_code);

	if ((msg->op_code == WLAN_IPA_UC_OPCODE_TX_RESUME) ||
	    (msg->op_code == WLAN_IPA_UC_OPCODE_RX_RESUME)) {
		qdf_mutex_acquire(&ipa_ctx->ipa_lock);
		ipa_ctx->activated_fw_pipe++;
		if (wlan_ipa_is_fw_wdi_activated(ipa_ctx)) {
			ipa_ctx->resource_loading = false;
			qdf_event_set(&ipa_ctx->ipa_resource_comp);
			if (ipa_ctx->wdi_enabled == false) {
				ipa_ctx->wdi_enabled = true;
				if (wlan_ipa_uc_send_wdi_control_msg(true) == 0)
					wlan_ipa_send_mcc_scc_msg(ipa_ctx,
							ipa_ctx->mcc_mode);
			}
			wlan_ipa_uc_proc_pending_event(ipa_ctx, true);
			if (ipa_ctx->pending_cons_req)
				wlan_ipa_wdi_rm_notify_completion(
						QDF_IPA_RM_RESOURCE_GRANTED,
						QDF_IPA_RM_RESOURCE_WLAN_CONS);
			ipa_ctx->pending_cons_req = false;
		}
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	} else if ((msg->op_code == WLAN_IPA_UC_OPCODE_TX_SUSPEND) ||
	    (msg->op_code == WLAN_IPA_UC_OPCODE_RX_SUSPEND)) {
		qdf_mutex_acquire(&ipa_ctx->ipa_lock);

		if (msg->op_code == WLAN_IPA_UC_OPCODE_RX_SUSPEND) {
			wlan_ipa_uc_disable_pipes(ipa_ctx);
			ipa_info("Disable FW TX PIPE");
			cdp_ipa_set_active(ipa_ctx->dp_soc, ipa_ctx->dp_pdev,
					   false, true);
		}

		ipa_ctx->activated_fw_pipe--;
		if (!ipa_ctx->activated_fw_pipe) {
			/*
			 * Async return success from FW
			 * Disable/suspend all the PIPEs
			 */
			ipa_ctx->resource_unloading = false;
			qdf_event_set(&ipa_ctx->ipa_resource_comp);
			if (wlan_ipa_is_rm_enabled(ipa_ctx->config))
				wlan_ipa_wdi_rm_release_resource(ipa_ctx,
						QDF_IPA_RM_RESOURCE_WLAN_PROD);
			wlan_ipa_uc_proc_pending_event(ipa_ctx, false);
			ipa_ctx->pending_cons_req = false;
		}
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	} else if ((msg->op_code == WLAN_IPA_UC_OPCODE_STATS) &&
		(ipa_ctx->stat_req_reason == WLAN_IPA_UC_STAT_REASON_DEBUG)) {
		uc_fw_stat = (struct ipa_uc_fw_stats *)
			((uint8_t *)op_msg + sizeof(struct op_msg_type));

		/* WLAN FW WDI stats */
		wlan_ipa_print_fw_wdi_stats(ipa_ctx, uc_fw_stat);
	} else if ((msg->op_code == WLAN_IPA_UC_OPCODE_STATS) &&
		(ipa_ctx->stat_req_reason == WLAN_IPA_UC_STAT_REASON_BW_CAL)) {
		/* STATs from FW */
		uc_fw_stat = (struct ipa_uc_fw_stats *)
			((uint8_t *)op_msg + sizeof(struct op_msg_type));
		qdf_mutex_acquire(&ipa_ctx->ipa_lock);
		ipa_ctx->ipa_tx_packets_diff = BW_GET_DIFF(
			uc_fw_stat->tx_pkts_completed,
			ipa_ctx->ipa_p_tx_packets);
		ipa_ctx->ipa_rx_packets_diff = BW_GET_DIFF(
			(uc_fw_stat->rx_num_ind_drop_no_space +
			uc_fw_stat->rx_num_ind_drop_no_buf +
			uc_fw_stat->rx_num_pkts_indicated),
			ipa_ctx->ipa_p_rx_packets);

		ipa_ctx->ipa_p_tx_packets = uc_fw_stat->tx_pkts_completed;
		ipa_ctx->ipa_p_rx_packets =
			(uc_fw_stat->rx_num_ind_drop_no_space +
			uc_fw_stat->rx_num_ind_drop_no_buf +
			uc_fw_stat->rx_num_pkts_indicated);
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	} else if (msg->op_code == WLAN_IPA_UC_OPCODE_UC_READY) {
		qdf_mutex_acquire(&ipa_ctx->ipa_lock);
		wlan_ipa_uc_loaded_handler(ipa_ctx);
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	} else if (wlan_ipa_uc_op_metering(ipa_ctx, op_msg)) {
		ipa_err("Invalid message: op_code=%d, reason=%d",
			msg->op_code, ipa_ctx->stat_req_reason);
	}

	qdf_mem_free(op_msg);
}

/**
 * __wlan_ipa_uc_fw_op_event_handler - IPA uC FW OPvent handler
 * @data: uC OP work
 *
 * Return: None
 */
static void __wlan_ipa_uc_fw_op_event_handler(void *data)
{
	struct op_msg_type *msg;
	struct uc_op_work_struct *uc_op_work =
				(struct uc_op_work_struct *)data;
	struct wlan_ipa_priv *ipa_ctx = gp_ipa;

	if (qdf_is_module_state_transitioning()) {
		ipa_err("Module transition in progress");
		return;
	}

	msg = uc_op_work->msg;
	uc_op_work->msg = NULL;
	ipa_debug("posted msg %d", msg->op_code);

	wlan_ipa_uc_op_cb(msg, ipa_ctx);
}

/**
 * wlan_ipa_uc_fw_op_event_handler - SSR wrapper for
 * __wlan_ipa_uc_fw_op_event_handler
 * @data: uC OP work
 *
 * Return: None
 */
static void wlan_ipa_uc_fw_op_event_handler(void *data)
{
	qdf_ssr_protect(__func__);
	__wlan_ipa_uc_fw_op_event_handler(data);
	qdf_ssr_unprotect(__func__);
}

/**
 * wlan_ipa_uc_op_event_handler() - IPA UC OP event handler
 * @op_msg: operation message received from firmware
 * @ipa_ctx: Global IPA context
 *
 * Return: None
 */
static void wlan_ipa_uc_op_event_handler(uint8_t *op_msg, void *ctx)
{
	struct wlan_ipa_priv *ipa_ctx = (struct wlan_ipa_priv *)ctx;
	struct op_msg_type *msg;
	struct uc_op_work_struct *uc_op_work;

	if (!ipa_ctx)
		goto end;

	msg = (struct op_msg_type *)op_msg;

	if (msg->op_code >= WLAN_IPA_UC_OPCODE_MAX) {
		ipa_err("Invalid OP Code (%d)", msg->op_code);
		goto end;
	}

	uc_op_work = &ipa_ctx->uc_op_work[msg->op_code];
	if (uc_op_work->msg) {
		/* When the same uC OPCODE is already pended, just return */
		goto end;
	}

	uc_op_work->msg = msg;
	qdf_sched_work(0, &uc_op_work->work);
	return;

end:
	qdf_mem_free(op_msg);
}

QDF_STATUS wlan_ipa_uc_ol_init(struct wlan_ipa_priv *ipa_ctx,
			       qdf_device_t osdev)
{
	uint8_t i;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!wlan_ipa_uc_is_enabled(ipa_ctx->config))
		return QDF_STATUS_SUCCESS;

	ipa_debug("enter");

	for (i = 0; i < WLAN_IPA_MAX_SESSION; i++) {
		ipa_ctx->vdev_to_iface[i] = WLAN_IPA_MAX_SESSION;
		ipa_ctx->vdev_offload_enabled[i] = false;
	}

	if (cdp_ipa_get_resource(ipa_ctx->dp_soc, ipa_ctx->dp_pdev)) {
		ipa_err("IPA UC resource alloc fail");
		status = QDF_STATUS_E_FAILURE;
		goto fail_return;
	}

	if (true == ipa_ctx->uc_loaded) {
		status = wlan_ipa_wdi_setup(ipa_ctx, osdev);
		if (status) {
			ipa_err("Failure to setup IPA pipes (status=%d)",
				status);
			status = QDF_STATUS_E_FAILURE;
			goto fail_return;
		}

		cdp_ipa_set_doorbell_paddr(ipa_ctx->dp_soc, ipa_ctx->dp_pdev);
		wlan_ipa_init_metering(ipa_ctx);

		if (wlan_ipa_init_perf_level(ipa_ctx) != QDF_STATUS_SUCCESS)
			ipa_err("Failed to init perf level");
	}

	cdp_ipa_register_op_cb(ipa_ctx->dp_soc, ipa_ctx->dp_pdev,
			       wlan_ipa_uc_op_event_handler, (void *)ipa_ctx);

	for (i = 0; i < WLAN_IPA_UC_OPCODE_MAX; i++) {
		qdf_create_work(0, &ipa_ctx->uc_op_work[i].work,
				wlan_ipa_uc_fw_op_event_handler,
				&ipa_ctx->uc_op_work[i]);
		ipa_ctx->uc_op_work[i].msg = NULL;
	}

fail_return:
	ipa_debug("exit: status=%d", status);
	return status;
}

/**
 * wlan_ipa_cleanup_pending_event() - Cleanup IPA pending event list
 * @ipa_ctx: pointer to IPA IPA struct
 *
 * Return: none
 */
static void wlan_ipa_cleanup_pending_event(struct wlan_ipa_priv *ipa_ctx)
{
	struct wlan_ipa_uc_pending_event *pending_event = NULL;

	while (qdf_list_remove_front(&ipa_ctx->pending_event,
		(qdf_list_node_t **)&pending_event) == QDF_STATUS_SUCCESS)
		qdf_mem_free(pending_event);
}

QDF_STATUS wlan_ipa_uc_ol_deinit(struct wlan_ipa_priv *ipa_ctx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	int i;

	ipa_debug("enter");

	if (!wlan_ipa_uc_is_enabled(ipa_ctx->config))
		return status;

	if (!ipa_ctx->ipa_pipes_down)
		wlan_ipa_uc_disable_pipes(ipa_ctx);

	if (true == ipa_ctx->uc_loaded) {
		status = cdp_ipa_cleanup(ipa_ctx->dp_soc,
					 ipa_ctx->tx_pipe_handle,
					 ipa_ctx->rx_pipe_handle);
		if (status)
			ipa_err("Failure to cleanup IPA pipes (status=%d)",
				status);
	}

	qdf_mutex_acquire(&ipa_ctx->ipa_lock);
	wlan_ipa_cleanup_pending_event(ipa_ctx);
	qdf_mutex_release(&ipa_ctx->ipa_lock);

	for (i = 0; i < WLAN_IPA_UC_OPCODE_MAX; i++) {
		qdf_cancel_work(&ipa_ctx->uc_op_work[i].work);
		qdf_mem_free(ipa_ctx->uc_op_work[i].msg);
		ipa_ctx->uc_op_work[i].msg = NULL;
	}

	ipa_debug("exit: ret=%d", status);
	return status;
}

bool wlan_ipa_is_fw_wdi_activated(struct wlan_ipa_priv *ipa_ctx)
{
	return (WLAN_IPA_UC_NUM_WDI_PIPE == ipa_ctx->activated_fw_pipe);
}

/**
 * wlan_ipa_uc_send_evt() - send event to ipa
 * @net_dev: Interface net device
 * @type: event type
 * @mac_addr: pointer to mac address
 *
 * Send event to IPA driver
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_ipa_uc_send_evt(qdf_netdev_t net_dev,
				       qdf_ipa_wlan_event type,
				       uint8_t *mac_addr)
{
	struct wlan_ipa_priv *ipa_ctx = gp_ipa;
	qdf_ipa_msg_meta_t meta;
	qdf_ipa_wlan_msg_t *msg;

	QDF_IPA_MSG_META_MSG_LEN(&meta) = sizeof(qdf_ipa_wlan_msg_t);
	msg = qdf_mem_malloc(QDF_IPA_MSG_META_MSG_LEN(&meta));
	if (!msg) {
		ipa_err("msg allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	QDF_IPA_SET_META_MSG_TYPE(&meta, type);
	qdf_str_lcopy(QDF_IPA_WLAN_MSG_NAME(msg), net_dev->name,
		      IPA_RESOURCE_NAME_MAX);
	qdf_mem_copy(QDF_IPA_WLAN_MSG_MAC_ADDR(msg), mac_addr, QDF_NET_ETH_LEN);

	if (qdf_ipa_send_msg(&meta, msg, wlan_ipa_msg_free_fn)) {
		ipa_err("%s: Evt: %d fail",
			QDF_IPA_WLAN_MSG_NAME(msg),
			QDF_IPA_MSG_META_MSG_TYPE(&meta));
		qdf_mem_free(msg);

		return QDF_STATUS_E_FAILURE;
	}

	ipa_ctx->stats.num_send_msg++;

	return QDF_STATUS_SUCCESS;
}

void wlan_ipa_uc_cleanup_sta(struct wlan_ipa_priv *ipa_ctx,
			     qdf_netdev_t net_dev)
{
	struct wlan_ipa_iface_context *iface_ctx;
	int i;

	ipa_debug("enter");

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_ctx = &ipa_ctx->iface_context[i];
		if (iface_ctx && iface_ctx->device_mode == QDF_STA_MODE &&
		    iface_ctx->dev == net_dev && iface_ctx->tl_context) {
			wlan_ipa_uc_send_evt(net_dev, QDF_IPA_STA_DISCONNECT,
					     net_dev->dev_addr);
			wlan_ipa_cleanup_iface(iface_ctx);
		}
	}

	ipa_debug("exit");
}

QDF_STATUS wlan_ipa_uc_disconnect_ap(struct wlan_ipa_priv *ipa_ctx,
				     qdf_netdev_t net_dev)
{
	struct wlan_ipa_iface_context *iface_ctx;
	QDF_STATUS status;

	ipa_debug("enter");

	iface_ctx = wlan_ipa_get_iface(ipa_ctx, QDF_SAP_MODE);
	if (iface_ctx)
		status = wlan_ipa_uc_send_evt(net_dev, QDF_IPA_AP_DISCONNECT,
					      net_dev->dev_addr);
	else
		return QDF_STATUS_E_INVAL;

	ipa_debug("exit :%d", status);

	return status;
}

void wlan_ipa_cleanup_dev_iface(struct wlan_ipa_priv *ipa_ctx,
				qdf_netdev_t net_dev)
{
	struct wlan_ipa_iface_context *iface_ctx;
	int i;

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_ctx = &ipa_ctx->iface_context[i];
		if (iface_ctx->dev == net_dev) {
			wlan_ipa_cleanup_iface(iface_ctx);
			break;
		}
	}
}

void wlan_ipa_uc_ssr_cleanup(struct wlan_ipa_priv *ipa_ctx)
{
	struct wlan_ipa_iface_context *iface;
	int i;

	ipa_info("enter");

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface = &ipa_ctx->iface_context[i];
		if (iface->dev) {
			if (iface->device_mode == QDF_SAP_MODE)
				wlan_ipa_uc_send_evt(iface->dev,
						     QDF_IPA_AP_DISCONNECT,
						     iface->dev->dev_addr);
			else if (iface->device_mode == QDF_STA_MODE)
				wlan_ipa_uc_send_evt(iface->dev,
						     QDF_IPA_STA_DISCONNECT,
						     iface->dev->dev_addr);
			wlan_ipa_cleanup_iface(iface);
		}
	}
}

void wlan_ipa_fw_rejuvenate_send_msg(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_ipa_msg_meta_t meta;
	qdf_ipa_wlan_msg_t *msg;
	int ret;

	meta.msg_len = sizeof(*msg);
	msg = qdf_mem_malloc(meta.msg_len);
	if (!msg) {
		ipa_debug("msg allocation failed");
		return;
	}

	QDF_IPA_SET_META_MSG_TYPE(&meta, QDF_FWR_SSR_BEFORE_SHUTDOWN);
	ipa_debug("ipa_send_msg(Evt:%d)",
		  meta.msg_type);
	ret = qdf_ipa_send_msg(&meta, msg, wlan_ipa_msg_free_fn);

	if (ret) {
		ipa_err("ipa_send_msg(Evt:%d)-fail=%d",
			meta.msg_type, ret);
		qdf_mem_free(msg);
	}
	ipa_ctx->stats.num_send_msg++;
}
