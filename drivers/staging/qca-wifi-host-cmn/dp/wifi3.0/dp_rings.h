/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _DP_RINGS_H_
#define _DP_RINGS_H_

#include <dp_types.h>
#include <dp_internal.h>
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif

#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
static inline bool dp_is_mon_mask_valid(struct dp_soc *soc,
					struct dp_intr *intr_ctx)
{
	if (intr_ctx->rx_mon_ring_mask)
		return true;

	return false;
}
#else
static inline bool dp_is_mon_mask_valid(struct dp_soc *soc,
					struct dp_intr *intr_ctx)
{
	return false;
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED

/**
 * dp_srng_get_cpu() - Get the smp processor id for srng processing
 *
 * Return: smp processor id
 */
static inline int dp_srng_get_cpu(void)
{
	return qdf_get_cpu();
}

#else /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_srng_get_cpu() - Get the smp processor id for srng processing
 *
 * Return: smp processor id
 */
static inline int dp_srng_get_cpu(void)
{
	return 0;
}

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_interrupt_timer() - timer poll for interrupts
 * @arg: SoC Handle
 *
 * Return:
 *
 */
void dp_interrupt_timer(void *arg);

/**
 * dp_soc_print_inactive_objects() - prints inactive peer and vdev list
 * @soc: DP SOC handle
 *
 */
void dp_soc_print_inactive_objects(struct dp_soc *soc);

/**
 * dp_get_tx_pending() - read pending tx
 * @pdev_handle: Datapath PDEV handle
 *
 * Return: outstanding tx
 */
int32_t dp_get_tx_pending(struct cdp_pdev *pdev_handle);

/**
 * dp_find_missing_tx_comp() - check for leaked descriptor in tx path
 * @soc: DP SOC context
 *
 * Parse through descriptors in all pools and validate magic number and
 * completion time. Trigger self recovery if magic value is corrupted.
 *
 * Return: None.
 */
void dp_find_missing_tx_comp(struct dp_soc *soc);

void dp_enable_verbose_debug(struct dp_soc *soc);

QDF_STATUS dp_peer_legacy_setup(struct dp_soc *soc, struct dp_peer *peer);

uint32_t dp_service_srngs_wrapper(void *dp_ctx, uint32_t dp_budget, int cpu);

void dp_soc_interrupt_map_calculate(struct dp_soc *soc, int intr_ctx_num,
				    int *irq_id_map, int *num_irq);
void dp_srng_msi_setup(struct dp_soc *soc, struct dp_srng *srng,
		       struct hal_srng_params *ring_params,
		       int ring_type, int ring_num);
void
dp_srng_configure_interrupt_thresholds(struct dp_soc *soc,
				       struct hal_srng_params *ring_params,
				       int ring_type, int ring_num,
				       int num_entries);

int dp_process_lmac_rings(struct dp_intr *int_ctx, int total_budget);

/**
 * dp_service_lmac_rings()- timer to reap lmac rings
 * @arg: SoC Handle
 *
 * Return:
 *
 */
void dp_service_lmac_rings(void *arg);

/**
 * dp_service_srngs() - Top level interrupt handler for DP Ring interrupts
 * @dp_ctx: DP SOC handle
 * @dp_budget: Number of frames/descriptors that can be processed in one shot
 * @cpu: CPU on which this instance is running
 *
 * Return: remaining budget/quota for the soc device
 */
uint32_t dp_service_srngs(void *dp_ctx, uint32_t dp_budget, int cpu);

/**
 * dp_soc_set_interrupt_mode() - Set the interrupt mode in soc
 * @soc: DP soc handle
 *
 * Set the appropriate interrupt mode flag in the soc
 */
void dp_soc_set_interrupt_mode(struct dp_soc *soc);

/**
 * dp_srng_find_ring_in_mask() - find which ext_group a ring belongs
 * @ring_num: ring num of the ring being queried
 * @grp_mask: the grp_mask array for the ring type in question.
 *
 * The grp_mask array is indexed by group number and the bit fields correspond
 * to ring numbers.  We are finding which interrupt group a ring belongs to.
 *
 * Return: the index in the grp_mask array with the ring number.
 * -QDF_STATUS_E_NOENT if no entry is found
 */
static inline int dp_srng_find_ring_in_mask(int ring_num, uint8_t *grp_mask)
{
	int ext_group_num;
	uint8_t mask = 1 << ring_num;

	for (ext_group_num = 0; ext_group_num < WLAN_CFG_INT_NUM_CONTEXTS;
	     ext_group_num++) {
		if (mask & grp_mask[ext_group_num])
			return ext_group_num;
	}

	return -QDF_STATUS_E_NOENT;
}

/* MCL specific functions */
#if defined(DP_CON_MON)

#ifdef DP_CON_MON_MSI_ENABLED
/**
 * dp_soc_get_mon_mask_for_interrupt_mode() - get mon mode mask for intr mode
 * @soc: pointer to dp_soc handle
 * @intr_ctx_num: interrupt context number for which mon mask is needed
 *
 * For MCL, monitor mode rings are being processed in timer contexts (polled).
 * This function is returning 0, since in interrupt mode(softirq based RX),
 * we donot want to process monitor mode rings in a softirq.
 *
 * So, in case packet log is enabled for SAP/STA/P2P modes,
 * regular interrupt processing will not process monitor mode rings. It would be
 * done in a separate timer context.
 *
 * Return: 0
 */
static inline uint32_t
dp_soc_get_mon_mask_for_interrupt_mode(struct dp_soc *soc, int intr_ctx_num)
{
	return wlan_cfg_get_rx_mon_ring_mask(soc->wlan_cfg_ctx, intr_ctx_num);
}
#else
/**
 * dp_soc_get_mon_mask_for_interrupt_mode() - get mon mode mask for intr mode
 * @soc: pointer to dp_soc handle
 * @intr_ctx_num: interrupt context number for which mon mask is needed
 *
 * For MCL, monitor mode rings are being processed in timer contexts (polled).
 * This function is returning 0, since in interrupt mode(softirq based RX),
 * we donot want to process monitor mode rings in a softirq.
 *
 * So, in case packet log is enabled for SAP/STA/P2P modes,
 * regular interrupt processing will not process monitor mode rings. It would be
 * done in a separate timer context.
 *
 * Return: 0
 */
static inline uint32_t
dp_soc_get_mon_mask_for_interrupt_mode(struct dp_soc *soc, int intr_ctx_num)
{
	return 0;
}
#endif

#else

/**
 * dp_soc_get_mon_mask_for_interrupt_mode() - get mon mode mask for intr mode
 * @soc: pointer to dp_soc handle
 * @intr_ctx_num: interrupt context number for which mon mask is needed
 *
 * Return: mon mask value
 */
static inline
uint32_t dp_soc_get_mon_mask_for_interrupt_mode(struct dp_soc *soc,
						int intr_ctx_num)
{
	return wlan_cfg_get_rx_mon_ring_mask(soc->wlan_cfg_ctx, intr_ctx_num);
}
#endif

#ifdef DISABLE_MON_RING_MSI_CFG
/**
 * dp_skip_msi_cfg() - Check if msi cfg has to be skipped for ring_type
 * @soc: DP SoC context
 * @ring_type: sring type
 *
 * Return: True if msi cfg should be skipped for srng type else false
 */
static inline bool dp_skip_msi_cfg(struct dp_soc *soc, int ring_type)
{
	if (ring_type == RXDMA_MONITOR_STATUS)
		return true;

	return false;
}
#else
#ifdef DP_CON_MON_MSI_ENABLED
#ifdef WLAN_SOFTUMAC_SUPPORT
static inline bool dp_skip_msi_cfg(struct dp_soc *soc, int ring_type)
{
	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() == QDF_GLOBAL_MONITOR_MODE) {
		if (ring_type != RXDMA_MONITOR_STATUS)
			return true;
	} else if (ring_type == RXDMA_MONITOR_STATUS &&
		   !dp_mon_mode_local_pkt_capture(soc)) {
		return true;
	}

	return false;
}
#else
static inline bool dp_skip_msi_cfg(struct dp_soc *soc, int ring_type)
{
	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() == QDF_GLOBAL_MONITOR_MODE) {
		if (ring_type == REO_DST || ring_type == RXDMA_DST)
			return true;
	} else if (ring_type == RXDMA_MONITOR_STATUS &&
		  !dp_mon_mode_local_pkt_capture(soc)) {
		return true;
	}

	return false;
}
#endif
#else
static inline bool dp_skip_msi_cfg(struct dp_soc *soc, int ring_type)
{
	return false;
}
#endif /* DP_CON_MON_MSI_ENABLED */
#endif /* DISABLE_MON_RING_MSI_CFG */

/**
 * dp_is_msi_group_number_invalid() - check msi_group_number valid or not
 * @soc: dp_soc
 * @msi_group_number: MSI group number.
 * @msi_data_count: MSI data count.
 *
 * Return: true if msi_group_number is invalid.
 */
static inline bool dp_is_msi_group_number_invalid(struct dp_soc *soc,
						  int msi_group_number,
						  int msi_data_count)
{
	if (soc && soc->osdev && soc->osdev->dev &&
	    pld_is_one_msi(soc->osdev->dev))
		return false;

	return msi_group_number > msi_data_count;
}

#ifndef WLAN_SOFTUMAC_SUPPORT
/**
 * dp_soc_attach_poll() - Register handlers for DP interrupts
 * @txrx_soc: DP SOC handle
 *
 * Host driver will register for “DP_NUM_INTERRUPT_CONTEXTS” number of NAPI
 * contexts. Each NAPI context will have a tx_ring_mask , rx_ring_mask ,and
 * rx_monitor_ring mask to indicate the rings that are processed by the handler.
 *
 * Return: 0 for success, nonzero for failure.
 */
QDF_STATUS dp_soc_attach_poll(struct cdp_soc_t *txrx_soc);

/**
 * dp_soc_interrupt_attach() - Register handlers for DP interrupts
 * @txrx_soc: DP SOC handle
 *
 * Host driver will register for “DP_NUM_INTERRUPT_CONTEXTS” number of NAPI
 * contexts. Each NAPI context will have a tx_ring_mask , rx_ring_mask ,and
 * rx_monitor_ring mask to indicate the rings that are processed by the handler.
 *
 * Return: 0 for success. nonzero for failure.
 */
QDF_STATUS dp_soc_interrupt_attach(struct cdp_soc_t *txrx_soc);

/**
 * dp_hw_link_desc_ring_free() - Free h/w link desc rings
 * @soc: DP SOC handle
 *
 * Return: none
 */
void dp_hw_link_desc_ring_free(struct dp_soc *soc);

/**
 * dp_hw_link_desc_ring_alloc() - Allocate hw link desc rings
 * @soc: DP SOC handle
 *
 * Allocate memory for WBM_IDLE_LINK srng ring if the number of
 * link descriptors is less then the max_allocated size. else
 * allocate memory for wbm_idle_scatter_buffer.
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_NO_MEM: No memory (Failure)
 */
QDF_STATUS dp_hw_link_desc_ring_alloc(struct dp_soc *soc);

/**
 * dp_hw_link_desc_ring_init() - Initialize hw link desc rings
 * @soc: DP SOC handle
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_FAILURE: failure
 */
QDF_STATUS dp_hw_link_desc_ring_init(struct dp_soc *soc);

/**
 * dp_hw_link_desc_ring_deinit() - Reset hw link desc rings
 * @soc: DP SOC handle
 *
 * Return: None
 */
void dp_hw_link_desc_ring_deinit(struct dp_soc *soc);

/**
 * dp_ipa_hal_tx_init_alt_data_ring() - IPA hal init data rings
 * @soc: DP SOC handle
 *
 * Return: None
 */
void dp_ipa_hal_tx_init_alt_data_ring(struct dp_soc *soc);

/*
 * dp_soc_reset_ring_map() - Reset cpu ring map
 * @soc: Datapath soc handler
 *
 * This api resets the default cpu ring map
 */
void dp_soc_reset_cpu_ring_map(struct dp_soc *soc);

/*
 * dp_soc_reset_ipa_vlan_intr_mask() - reset interrupt mask for IPA offloaded
 * ring for vlan tagged traffic
 * @dp_soc - DP Soc handle
 *
 * Return: Return void
 */
void dp_soc_reset_ipa_vlan_intr_mask(struct dp_soc *soc);

/*
 * dp_soc_reset_intr_mask() - reset interrupt mask
 * @dp_soc - DP Soc handle
 *
 * Return: Return void
 */
void dp_soc_reset_intr_mask(struct dp_soc *soc);

/*
 * dp_reo_frag_dst_set() - configure reo register to set the
 *                        fragment destination ring
 * @soc : Datapath soc
 * @frag_dst_ring : output parameter to set fragment destination ring
 *
 * Based on offload_radio below fragment destination rings is selected
 * 0 - TCL
 * 1 - SW1
 * 2 - SW2
 * 3 - SW3
 * 4 - SW4
 * 5 - Release
 * 6 - FW
 * 7 - alternate select
 *
 * return: void
 */
void dp_reo_frag_dst_set(struct dp_soc *soc, uint8_t *frag_dst_ring);

/**
 * dp_dscp_tid_map_setup(): Initialize the dscp-tid maps
 * @pdev:  DP_PDEV handle
 *
 * Return: void
 */
void
dp_dscp_tid_map_setup(struct dp_pdev *pdev);

/**
 * dp_pcp_tid_map_setup(): Initialize the pcp-tid maps
 * @pdev: DP_PDEV handle
 *
 * Return: void
 */
void
dp_pcp_tid_map_setup(struct dp_pdev *pdev);

/**
 * dp_soc_deinit() - Deinitialize txrx SOC
 * @txrx_soc: Opaque DP SOC handle
 *
 * Return: None
 */
void dp_soc_deinit(void *txrx_soc);

#ifdef QCA_HOST2FW_RXBUF_RING
void
dp_htt_setup_rxdma_err_dst_ring(struct dp_soc *soc, int mac_id,
				int lmac_id);
#endif

/*
 * dp_peer_setup_wifi3() - initialize the peer
 * @soc_hdl: soc handle object
 * @vdev_id : vdev_id of vdev object
 * @peer_mac: Peer's mac address
 * @peer_setup_info: peer setup info for MLO
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_peer_setup_wifi3(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		    uint8_t *peer_mac,
		    struct cdp_peer_setup_info *setup_info);

uint32_t dp_get_tx_rings_grp_bitmap(struct cdp_soc_t *soc_hdl);

/*
 * dp_set_ba_aging_timeout() - set ba aging timeout per AC
 * @txrx_soc: cdp soc handle
 * @ac: Access category
 * @value: timeout value in millisec
 *
 * Return: void
 */
void dp_set_ba_aging_timeout(struct cdp_soc_t *txrx_soc,
			     uint8_t ac, uint32_t value);

/*
 * dp_get_ba_aging_timeout() - get ba aging timeout per AC
 * @txrx_soc: cdp soc handle
 * @ac: access category
 * @value: timeout value in millisec
 *
 * Return: void
 */
void dp_get_ba_aging_timeout(struct cdp_soc_t *txrx_soc,
			     uint8_t ac, uint32_t *value);

/*
 * dp_set_pdev_reo_dest() - set the reo destination ring for this pdev
 * @txrx_soc: cdp soc handle
 * @pdev_id: id of physical device object
 * @val: reo destination ring index (1 - 4)
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_set_pdev_reo_dest(struct cdp_soc_t *txrx_soc, uint8_t pdev_id,
		     enum cdp_host_reo_dest_ring val);

/*
 * dp_get_pdev_reo_dest() - get the reo destination for this pdev
 * @txrx_soc: cdp soc handle
 * @pdev_id: id of physical device object
 *
 * Return: reo destination ring index
 */
enum cdp_host_reo_dest_ring
dp_get_pdev_reo_dest(struct cdp_soc_t *txrx_soc, uint8_t pdev_id);

/**
 * dp_set_pdev_pcp_tid_map_wifi3(): update pcp tid map in pdev
 * @psoc: dp soc handle
 * @pdev_id: id of DP_PDEV handle
 * @pcp: pcp value
 * @tid: tid value passed by the user
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS dp_set_pdev_pcp_tid_map_wifi3(ol_txrx_soc_handle psoc,
					 uint8_t pdev_id,
					 uint8_t pcp, uint8_t tid);

/**
 * dp_set_vdev_pcp_tid_map_wifi3(): update pcp tid map in vdev
 * @soc_hdl: DP soc handle
 * @vdev_id: id of DP_VDEV handle
 * @pcp: pcp value
 * @tid: tid value passed by the user
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS dp_set_vdev_pcp_tid_map_wifi3(struct cdp_soc_t *soc_hdl,
					 uint8_t vdev_id,
					 uint8_t pcp, uint8_t tid);

/* *
 * dp_dump_wbm_idle_hptp() -dump wbm idle ring, hw hp tp info.
 * @soc: dp soc.
 * @pdev: dp pdev.
 *
 * Return: None.
 */
void
dp_dump_wbm_idle_hptp(struct dp_soc *soc, struct dp_pdev *pdev);

/**
 * dp_display_srng_info() - Dump the srng HP TP info
 * @soc_hdl: CDP Soc handle
 *
 * This function dumps the SW hp/tp values for the important rings.
 * HW hp/tp values are not being dumped, since it can lead to
 * READ NOC error when UMAC is in low power state. MCC does not have
 * device force wake working yet.
 *
 * Return: rings are empty
 */
bool dp_display_srng_info(struct cdp_soc_t *soc_hdl);

#if defined(DP_POWER_SAVE) || defined(FEATURE_RUNTIME_PM)
QDF_STATUS dp_drain_txrx(struct cdp_soc_t *soc_handle, uint8_t rx_only);

/*
 * dp_update_ring_hptp() - update dp rings hptp
 * @soc: dp soc handler
 * @force_flush_tx: force flush the Tx ring hp
 */
void dp_update_ring_hptp(struct dp_soc *soc, bool force_flush_tx);
#endif

/*
 * dp_flush_tcl_ring() - flush TCL ring hp
 * @pdev: dp pdev
 * @ring_id: TCL ring id
 *
 * Return: 0 on success and error code on failure
 */
int dp_flush_tcl_ring(struct dp_pdev *pdev, int ring_id);

#ifdef WLAN_FEATURE_STATS_EXT
/**
 * dp_request_rx_hw_stats - request rx hardware stats
 * @soc_hdl: soc handle
 * @vdev_id: vdev id
 *
 * Return: None
 */
QDF_STATUS
dp_request_rx_hw_stats(struct cdp_soc_t *soc_hdl, uint8_t vdev_id);
#endif

/**
 * dp_reset_rx_hw_ext_stats - Reset rx hardware ext stats
 * @soc_hdl: soc handle
 *
 * Return: None
 */
void dp_reset_rx_hw_ext_stats(struct cdp_soc_t *soc_hdl);

void dp_pdev_set_default_reo(struct dp_pdev *pdev);

/**
 * dp_soc_init() - Initialize txrx SOC
 * @soc: Opaque DP SOC handle
 * @htc_handle: Opaque HTC handle
 * @hif_handle: Opaque HIF handle
 *
 * Return: DP SOC handle on success, NULL on failure
 */
void *dp_soc_init(struct dp_soc *soc, HTC_HANDLE htc_handle,
		  struct hif_opaque_softc *hif_handle);

void dp_tx_init_cmd_credit_ring(struct dp_soc *soc);

/**
 * dp_soc_srng_deinit() - de-initialize soc srng rings
 * @soc: Datapath soc handle
 *
 */
void dp_soc_srng_deinit(struct dp_soc *soc);

/**
 * dp_soc_srng_init() - Initialize soc level srng rings
 * @soc: Datapath soc handle
 *
 * return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS dp_soc_srng_init(struct dp_soc *soc);

/**
 * dp_soc_srng_free() - free soc level srng rings
 * @soc: Datapath soc handle
 *
 */
void dp_soc_srng_free(struct dp_soc *soc);

/**
 * dp_soc_srng_alloc() - Allocate memory for soc level srng rings
 * @soc: Datapath soc handle
 *
 * return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_NOMEM on failure
 */
QDF_STATUS dp_soc_srng_alloc(struct dp_soc *soc);

/**
 * dp_soc_cfg_attach() - set target specific configuration in
 *			 dp soc cfg.
 * @soc: dp soc handle
 */
void dp_soc_cfg_attach(struct dp_soc *soc);

#else /* WLAN_SOFTUMAC_SUPPORT */
static inline void dp_hw_link_desc_ring_free(struct dp_soc *soc)
{
}

static inline QDF_STATUS dp_hw_link_desc_ring_alloc(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_hw_link_desc_ring_init(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_hw_link_desc_ring_deinit(struct dp_soc *soc)
{
}

static inline void dp_ipa_hal_tx_init_alt_data_ring(struct dp_soc *soc)
{
}

static inline void dp_soc_reset_cpu_ring_map(struct dp_soc *soc)
{
}

static inline void dp_soc_reset_ipa_vlan_intr_mask(struct dp_soc *soc)
{
}

static inline void dp_soc_reset_intr_mask(struct dp_soc *soc)
{
}

static inline
void dp_reo_frag_dst_set(struct dp_soc *soc, uint8_t *frag_dst_ring)
{
}

/**
 * dp_dscp_tid_map_setup(): Initialize the dscp-tid maps
 * @pdev: DP_PDEV handle
 *
 * Return: void
 */
static inline void
dp_dscp_tid_map_setup(struct dp_pdev *pdev)
{
}

/**
 * dp_pcp_tid_map_setup(): Initialize the pcp-tid maps
 * @pdev: DP_PDEV handle
 *
 * Return: void
 */
static inline void
dp_pcp_tid_map_setup(struct dp_pdev *pdev)
{
}

#ifdef QCA_HOST2FW_RXBUF_RING
static inline void
dp_htt_setup_rxdma_err_dst_ring(struct dp_soc *soc, int mac_id,
				int lmac_id)
{
	if ((soc->cdp_soc.ol_ops->get_con_mode &&
	     soc->cdp_soc.ol_ops->get_con_mode() == QDF_GLOBAL_MONITOR_MODE) &&
	    soc->rxdma_err_dst_ring[lmac_id].hal_srng)
		htt_srng_setup(soc->htt_handle, mac_id,
			       soc->rxdma_err_dst_ring[lmac_id].hal_srng,
			       RXDMA_DST);
}
#endif

/* *
 * dp_dump_wbm_idle_hptp() -dump wbm idle ring, hw hp tp info.
 * @soc: dp soc.
 * @pdev: dp pdev.
 *
 * Return: None.
 */
static inline void
dp_dump_wbm_idle_hptp(struct dp_soc *soc, struct dp_pdev *pdev)
{
}

static inline void dp_pdev_set_default_reo(struct dp_pdev *pdev)
{
}

static inline void dp_tx_init_cmd_credit_ring(struct dp_soc *soc)
{
}

/**
 * dp_soc_srng_deinit() - de-initialize soc srng rings
 * @soc: Datapath soc handle
 *
 */
static inline void dp_soc_srng_deinit(struct dp_soc *soc)
{
}

/**
 * dp_soc_srng_init() - Initialize soc level srng rings
 * @soc: Datapath soc handle
 *
 * return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAILURE on failure
 */
static inline QDF_STATUS dp_soc_srng_init(struct dp_soc *soc)
{
	dp_enable_verbose_debug(soc);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_soc_srng_free() - free soc level srng rings
 * @soc: Datapath soc handle
 *
 */
static inline void dp_soc_srng_free(struct dp_soc *soc)
{
}

/**
 * dp_soc_srng_alloc() - Allocate memory for soc level srng rings
 * @soc: Datapath soc handle
 *
 * return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_NOMEM on failure
 */
static inline QDF_STATUS dp_soc_srng_alloc(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_display_srng_info() - Dump the ring Read/Write idx info
 * @soc_hdl: CDP Soc handle
 *
 * This function dumps the SW Read/Write idx for the important rings.
 *
 * Return:  rings are empty
 */
static inline bool dp_display_srng_info(struct cdp_soc_t *soc_hdl)
{
/*TODO add support display SOFTUMAC data rings info*/
	return true;
}

#if defined(DP_POWER_SAVE) || defined(FEATURE_RUNTIME_PM)
static inline QDF_STATUS dp_drain_txrx(struct cdp_soc_t *soc_handle,
				       uint8_t rx_only)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif /* WLAN_SOFTUMAC_SUPPORT */

#if defined(WLAN_FEATURE_NEAR_FULL_IRQ) && !defined(WLAN_SOFTUMAC_SUPPORT)
void dp_srng_msi2_setup(struct dp_soc *soc,
			struct hal_srng_params *ring_params,
			int ring_type, int ring_num, int nf_msi_grp_num);
void dp_srng_set_msi2_ring_params(struct dp_soc *soc,
				  struct hal_srng_params *ring_params,
				  qdf_dma_addr_t msi2_addr,
				  uint32_t msi2_data);
uint8_t *dp_srng_get_near_full_irq_mask(struct dp_soc *soc,
					enum hal_ring_type ring_type,
					int ring_num);
void
dp_srng_configure_nf_interrupt_thresholds(struct dp_soc *soc,
					  struct hal_srng_params *ring_params,
					  int ring_type);
#else
static inline void
dp_srng_msi2_setup(struct dp_soc *soc,
		   struct hal_srng_params *ring_params,
		   int ring_type, int ring_num, int nf_msi_grp_num)
{
}

static inline void
dp_srng_set_msi2_ring_params(struct dp_soc *soc,
			     struct hal_srng_params *ring_params,
			     qdf_dma_addr_t msi2_addr,
			     uint32_t msi2_data)
{
}

static inline
uint8_t *dp_srng_get_near_full_irq_mask(struct dp_soc *soc,
					enum hal_ring_type ring_type,
					int ring_num)
{
	return NULL;
}

static inline void
dp_srng_configure_nf_interrupt_thresholds(struct dp_soc *soc,
					  struct hal_srng_params *ring_params,
					  int ring_type)
{
}
#endif

#ifdef WLAN_SUPPORT_DPDK
/*
 * dp_soc_reset_dpdk_intr_mask() - reset interrupt mask
 * @dp_soc - DP Soc handle
 *
 * Return: Return void
 */
void dp_soc_reset_dpdk_intr_mask(struct dp_soc *soc);
#else
static inline void dp_soc_reset_dpdk_intr_mask(struct dp_soc *soc)
{ }
#endif
#endif /* _DP_RINGS_H_ */
