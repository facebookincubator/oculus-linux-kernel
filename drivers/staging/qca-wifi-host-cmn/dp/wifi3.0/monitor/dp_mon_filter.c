/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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
#include <dp_types.h>
#include <dp_htt.h>
#include <dp_internal.h>
#include <dp_rx_mon.h>
#include <dp_mon_filter.h>
#include <dp_mon.h>

/*
 * dp_mon_filter_mode_type_to_str
 * Monitor Filter mode to string
 */
int8_t *dp_mon_filter_mode_type_to_str[DP_MON_FILTER_MAX_MODE] = {
#ifdef QCA_ENHANCED_STATS_SUPPORT
	"DP MON FILTER ENHACHED STATS MODE",
#endif /* QCA_ENHANCED_STATS_SUPPORT */
#ifdef QCA_MCOPY_SUPPORT
	"DP MON FILTER MCOPY MODE",
#endif /* QCA_MCOPY_SUPPORT */
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
	"DP MON FILTER SMART MONITOR MODE",
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */
	"DP_MON FILTER MONITOR MODE",
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	"DP MON FILTER RX CAPTURE MODE",
#endif /* WLAN_RX_PKT_CAPTURE_ENH */
#ifdef WDI_EVENT_ENABLE
	"DP MON FILTER PKT LOG FULL MODE",
	"DP MON FILTER PKT LOG LITE MODE",
	"DP MON FILTER PKT LOG CBF MODE",
#ifdef BE_PKTLOG_SUPPORT
	"DP MON FILTER PKT LOG HYBRID MODE",
#endif
#endif /* WDI_EVENT_ENABLE */
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	"DP MON FILTER RX UNDECODED METADATA CAPTURE MODE",
#endif
};

void dp_mon_filter_show_filter(struct dp_mon_pdev *mon_pdev,
			       enum dp_mon_filter_mode mode,
			       struct dp_mon_filter *filter)
{
	struct htt_rx_ring_tlv_filter *tlv_filter = &filter->tlv_filter;

	DP_MON_FILTER_PRINT("[%s]: Valid: %d",
			    dp_mon_filter_mode_type_to_str[mode],
			    filter->valid);
	DP_MON_FILTER_PRINT("mpdu_start: %d", tlv_filter->mpdu_start);
	DP_MON_FILTER_PRINT("msdu_start: %d", tlv_filter->msdu_start);
	DP_MON_FILTER_PRINT("packet: %d", tlv_filter->packet);
	DP_MON_FILTER_PRINT("msdu_end: %d", tlv_filter->msdu_end);
	DP_MON_FILTER_PRINT("mpdu_end: %d", tlv_filter->mpdu_end);
	DP_MON_FILTER_PRINT("packet_header: %d",
			    tlv_filter->packet_header);
	DP_MON_FILTER_PRINT("attention: %d", tlv_filter->attention);
	DP_MON_FILTER_PRINT("ppdu_start: %d", tlv_filter->ppdu_start);
	DP_MON_FILTER_PRINT("ppdu_end: %d", tlv_filter->ppdu_end);
	DP_MON_FILTER_PRINT("ppdu_end_user_stats: %d",
			    tlv_filter->ppdu_end_user_stats);
	DP_MON_FILTER_PRINT("ppdu_end_user_stats_ext: %d",
			    tlv_filter->ppdu_end_user_stats_ext);
	DP_MON_FILTER_PRINT("ppdu_end_status_done: %d",
			    tlv_filter->ppdu_end_status_done);
	DP_MON_FILTER_PRINT("ppdu_start_user_info: %d",
			    tlv_filter->ppdu_start_user_info);
	DP_MON_FILTER_PRINT("header_per_msdu: %d", tlv_filter->header_per_msdu);
	DP_MON_FILTER_PRINT("enable_fp: %d", tlv_filter->enable_fp);
	DP_MON_FILTER_PRINT("enable_md: %d", tlv_filter->enable_md);
	DP_MON_FILTER_PRINT("enable_mo: %d", tlv_filter->enable_mo);
	DP_MON_FILTER_PRINT("fp_mgmt_filter: 0x%x", tlv_filter->fp_mgmt_filter);
	DP_MON_FILTER_PRINT("mo_mgmt_filter: 0x%x", tlv_filter->mo_mgmt_filter);
	DP_MON_FILTER_PRINT("fp_ctrl_filter: 0x%x", tlv_filter->fp_ctrl_filter);
	DP_MON_FILTER_PRINT("mo_ctrl_filter: 0x%x", tlv_filter->mo_ctrl_filter);
	DP_MON_FILTER_PRINT("fp_data_filter: 0x%x", tlv_filter->fp_data_filter);
	DP_MON_FILTER_PRINT("mo_data_filter: 0x%x", tlv_filter->mo_data_filter);
	DP_MON_FILTER_PRINT("md_data_filter: 0x%x", tlv_filter->md_data_filter);
	DP_MON_FILTER_PRINT("md_mgmt_filter: 0x%x", tlv_filter->md_mgmt_filter);
	DP_MON_FILTER_PRINT("md_ctrl_filter: 0x%x", tlv_filter->md_ctrl_filter);
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	DP_MON_FILTER_PRINT("fp_phy_err: %d", tlv_filter->fp_phy_err);
	DP_MON_FILTER_PRINT("fp_phy_err_buf_src: %d",
			    tlv_filter->fp_phy_err_buf_src);
	DP_MON_FILTER_PRINT("fp_phy_err_buf_dest: %d",
			    tlv_filter->fp_phy_err_buf_dest);
	DP_MON_FILTER_PRINT("phy_err_mask: 0x%x", tlv_filter->phy_err_mask);
	DP_MON_FILTER_PRINT("phy_err_mask_cont: 0x%x",
			    tlv_filter->phy_err_mask_cont);
#endif
	DP_MON_FILTER_PRINT("mon_mac_filter: %d",
			    tlv_filter->enable_mon_mac_filter);
}

#ifdef QCA_UNDECODED_METADATA_SUPPORT
static inline void
dp_mon_set_fp_phy_err_filter(struct htt_rx_ring_tlv_filter *tlv_filter,
			     struct dp_mon_filter *mon_filter)
{
	if (mon_filter->tlv_filter.phy_err_filter_valid) {
		tlv_filter->fp_phy_err =
			mon_filter->tlv_filter.fp_phy_err;
		tlv_filter->fp_phy_err_buf_src =
			mon_filter->tlv_filter.fp_phy_err_buf_src;
		tlv_filter->fp_phy_err_buf_dest =
			mon_filter->tlv_filter.fp_phy_err_buf_dest;
		tlv_filter->phy_err_mask =
			mon_filter->tlv_filter.phy_err_mask;
		tlv_filter->phy_err_mask_cont =
			mon_filter->tlv_filter.phy_err_mask_cont;
		tlv_filter->phy_err_filter_valid =
			mon_filter->tlv_filter.phy_err_filter_valid;
	}
}
#else
static inline void
dp_mon_set_fp_phy_err_filter(struct htt_rx_ring_tlv_filter *tlv_filter,
			     struct dp_mon_filter *mon_filter)
{
}
#endif

void dp_mon_filter_h2t_setup(struct dp_soc *soc, struct dp_pdev *pdev,
			     enum dp_mon_filter_srng_type srng_type,
			     struct dp_mon_filter *filter)
{
	int32_t current_mode = 0;
	struct htt_rx_ring_tlv_filter *tlv_filter = &filter->tlv_filter;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	/*
	 * Loop through all the modes.
	 */
	for (current_mode = 0; current_mode < DP_MON_FILTER_MAX_MODE;
						current_mode++) {
		struct dp_mon_filter *mon_filter =
			&mon_pdev->filter[current_mode][srng_type];
		uint32_t src_filter = 0, dst_filter = 0;

		/*
		 * Check if the correct mode is enabled or not.
		 */
		if (!mon_filter->valid)
			continue;

		filter->valid = true;

		/*
		 * Set the super bit fields
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->tlv_filter, FILTER_TLV);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_TLV);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_TLV, dst_filter);

		/*
		 * Set the filter management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_FP_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_FP_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_FP_MGMT, dst_filter);

		/*
		 * Set the monitor other management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MO_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_MGMT, dst_filter);

		/*
		 * Set the filter pass control filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_FP_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_FP_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_FP_CTRL, dst_filter);

		/*
		 * Set the monitor other control filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MO_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_CTRL, dst_filter);

		/*
		 * Set the filter pass data filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_FP_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter,
					       FILTER_FP_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter,
				  FILTER_FP_DATA, dst_filter);

		/*
		 * Set the monitor other data filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MO_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_DATA, dst_filter);

		/*
		 * Set the monitor direct data filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MD_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter,
					       FILTER_MD_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter,
				  FILTER_MD_DATA, dst_filter);

		/*
		 * Set the monitor direct management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MD_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MD_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MD_MGMT, dst_filter);

		/*
		 * Set the monitor direct management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MD_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MD_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MD_CTRL, dst_filter);

		dp_mon_set_fp_phy_err_filter(tlv_filter, mon_filter);
		tlv_filter->enable_mon_mac_filter =
				mon_filter->tlv_filter.enable_mon_mac_filter;
	}

	dp_mon_filter_show_filter(mon_pdev, 0, filter);
}

QDF_STATUS
dp_mon_ht2_rx_ring_cfg(struct dp_soc *soc,
		       struct dp_pdev *pdev,
		       enum dp_mon_filter_srng_type srng_type,
		       struct htt_rx_ring_tlv_filter *tlv_filter)
{
	int mac_id;
	int max_mac_rings = wlan_cfg_get_num_mac_rings(pdev->wlan_cfg_ctx);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/*
	 * Overwrite the max_mac_rings for the status rings.
	 */
	if (srng_type == DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS)
		dp_update_num_mac_rings_for_dbs(soc, &max_mac_rings);

	dp_mon_filter_info("%pK: srng type %d Max_mac_rings %d ",
			   soc, srng_type, max_mac_rings);

	/*
	 * Loop through all MACs per radio and set the filter to the individual
	 * macs. For MCL
	 */
	for (mac_id = 0; mac_id < max_mac_rings; mac_id++) {
		int mac_for_pdev =
			dp_get_mac_id_for_pdev(mac_id, pdev->pdev_id);
		int lmac_id = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev->pdev_id);
		int hal_ring_type, ring_buf_size;
		hal_ring_handle_t hal_ring_hdl;

		switch (srng_type) {
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF:
			hal_ring_hdl = pdev->rx_mac_buf_ring[lmac_id].hal_srng;
			hal_ring_type = RXDMA_BUF;
			ring_buf_size = RX_DATA_BUFFER_SIZE;
			break;

		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS:
			/*
			 * If two back to back HTT msg sending happened in
			 * short time, the second HTT msg source SRNG HP
			 * writing has chance to fail, this has been confirmed
			 * by HST HW.
			 * for monitor mode, here is the last HTT msg for sending.
			 * if the 2nd HTT msg for monitor status ring sending failed,
			 * HW won't provide anything into 2nd monitor status ring.
			 * as a WAR, add some delay before 2nd HTT msg start sending,
			 * > 2us is required per HST HW, delay 100 us for safe.
			 */
			if (mac_id)
				qdf_udelay(100);

			hal_ring_hdl =
				soc->rxdma_mon_status_ring[lmac_id].hal_srng;
			hal_ring_type = RXDMA_MONITOR_STATUS;
			ring_buf_size = RX_MON_STATUS_BUF_SIZE;
			break;

		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF:
			hal_ring_hdl =
				soc->rxdma_mon_buf_ring[lmac_id].hal_srng;
			hal_ring_type = RXDMA_MONITOR_BUF;
			ring_buf_size = RX_DATA_BUFFER_SIZE;
			break;

		case DP_MON_FILTER_SRNG_TYPE_RXMON_DEST:
			hal_ring_hdl =
				soc->rxdma_mon_dst_ring[lmac_id].hal_srng;
			hal_ring_type = RXDMA_MONITOR_DST;
			ring_buf_size = RX_DATA_BUFFER_SIZE;
			break;
		default:
			return QDF_STATUS_E_FAILURE;
		}

		if (!hal_ring_hdl)
			continue;

		status = htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					     hal_ring_hdl, hal_ring_type,
					     ring_buf_size,
					     tlv_filter);
		if (status != QDF_STATUS_SUCCESS)
			return status;
	}

	return status;
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
void dp_mon_filter_setup_enhanced_stats(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_enhanced_stats)
		mon_ops->mon_filter_setup_enhanced_stats(pdev);
}

void dp_mon_filter_reset_enhanced_stats(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_enhanced_stats)
		mon_ops->mon_filter_reset_enhanced_stats(pdev);
}
#endif /* QCA_ENHANCED_STATS_SUPPORT */

#ifdef QCA_UNDECODED_METADATA_SUPPORT
void dp_mon_filter_setup_undecoded_metadata_mode(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_undecoded_metadata_capture)
		mon_ops->mon_filter_setup_undecoded_metadata_capture(pdev);
}

void dp_mon_filter_reset_undecoded_metadata_mode(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_undecoded_metadata_capture)
		mon_ops->mon_filter_reset_undecoded_metadata_capture(pdev);
}
#endif /* QCA_UNDECODED_METADATA_SUPPORT */

#ifdef QCA_MCOPY_SUPPORT
void dp_mon_filter_setup_mcopy_mode(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_mcopy_mode)
		mon_ops->mon_filter_setup_mcopy_mode(pdev);
}

void dp_mon_filter_reset_mcopy_mode(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_mcopy_mode)
		mon_ops->mon_filter_reset_mcopy_mode(pdev);
}
#endif /* QCA_MCOPY_SUPPORT */

#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
void dp_mon_filter_setup_smart_monitor(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_smart_monitor)
		mon_ops->mon_filter_setup_smart_monitor(pdev);
}

void dp_mon_filter_reset_smart_monitor(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_smart_monitor)
		mon_ops->mon_filter_reset_smart_monitor(pdev);
}
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */

void dp_mon_filter_set_reset_mon_mac_filter(struct dp_pdev *pdev, bool val)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_set_reset_mon_mac_filter)
		mon_ops->mon_filter_set_reset_mon_mac_filter(pdev, val);
}

#ifdef WLAN_RX_PKT_CAPTURE_ENH
void dp_mon_filter_setup_rx_enh_capture(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_rx_enh_capture)
		mon_ops->mon_filter_setup_rx_enh_capture(pdev);
}

void dp_mon_filter_reset_rx_enh_capture(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_rx_enh_capture)
		mon_ops->mon_filter_reset_rx_enh_capture(pdev);
}
#endif /* WLAN_RX_PKT_CAPTURE_ENH */

void dp_mon_filter_setup_mon_mode(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_rx_mon_mode)
		mon_ops->mon_filter_setup_rx_mon_mode(pdev);
}

void dp_mon_filter_setup_tx_mon_mode(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_tx_mon_mode)
		mon_ops->mon_filter_setup_tx_mon_mode(pdev);
}

void dp_mon_filter_reset_mon_mode(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_rx_mon_mode)
		mon_ops->mon_filter_reset_rx_mon_mode(pdev);
}

#ifdef WDI_EVENT_ENABLE
void dp_mon_filter_setup_rx_pkt_log_full(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_rx_pkt_log_full)
		mon_ops->mon_filter_setup_rx_pkt_log_full(pdev);
}

void dp_mon_filter_reset_rx_pkt_log_full(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_rx_pkt_log_full)
		mon_ops->mon_filter_reset_rx_pkt_log_full(pdev);
}

void dp_mon_filter_setup_rx_pkt_log_lite(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_rx_pkt_log_lite)
		mon_ops->mon_filter_setup_rx_pkt_log_lite(pdev);
}

void dp_mon_filter_reset_rx_pkt_log_lite(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_rx_pkt_log_lite)
		mon_ops->mon_filter_reset_rx_pkt_log_lite(pdev);
}

void dp_mon_filter_setup_rx_pkt_log_cbf(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_rx_pkt_log_cbf)
		mon_ops->mon_filter_setup_rx_pkt_log_cbf(pdev);
}

void dp_mon_filter_reset_rx_pktlog_cbf(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_rx_pkt_log_cbf)
		mon_ops->mon_filter_reset_rx_pkt_log_cbf(pdev);
}

#ifdef BE_PKTLOG_SUPPORT
void dp_mon_filter_setup_pktlog_hybrid(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_setup_pktlog_hybrid)
		mon_ops->mon_filter_setup_pktlog_hybrid(pdev);
}

void dp_mon_filter_reset_pktlog_hybrid(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_filter_reset_pktlog_hybrid)
		mon_ops->mon_filter_reset_pktlog_hybrid(pdev);
}
#endif
#endif /* WDI_EVENT_ENABLE */

QDF_STATUS dp_mon_filter_update(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!mon_ops) {
		dp_mon_filter_err("Mon ops uninitialized");
		return QDF_STATUS_E_FAILURE;
	}

	if (mon_ops && mon_ops->rx_mon_filter_update)
		mon_ops->rx_mon_filter_update(pdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_mon_filter_update(struct dp_pdev *pdev)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!mon_ops) {
		dp_mon_filter_err("Mon ops uninitialized");
		return QDF_STATUS_E_FAILURE;
	}

	if (mon_ops && mon_ops->tx_mon_filter_update)
		mon_ops->tx_mon_filter_update(pdev);

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
void dp_mon_filters_reset(struct dp_pdev *pdev)
{
	dp_mon_filter_reset_enhanced_stats(pdev);
	dp_mon_filter_reset_mon_mode(pdev);
	dp_mon_filter_update(pdev);
}
#else
void dp_mon_filters_reset(struct dp_pdev *pdev)
{
}
#endif

void
dp_mon_filter_reset_mon_srng(struct dp_soc *soc, struct dp_pdev *pdev,
			     enum dp_mon_filter_srng_type mon_srng_type)
{
	struct htt_rx_ring_tlv_filter tlv_filter = {0};

	if (dp_mon_ht2_rx_ring_cfg(soc, pdev, mon_srng_type,
				   &tlv_filter) != QDF_STATUS_SUCCESS) {
		dp_mon_filter_err("%pK: Monitor destination ring filter setting failed",
				  soc);
	}
}

/**
 * dp_mon_filter_adjust() - adjust the mon filters per target basis
 * @pdev: DP pdev handle
 * @filter: DP mon filter
 *
 * Return: None
 */
static inline
void dp_mon_filter_adjust(struct dp_pdev *pdev, struct dp_mon_filter *filter)
{
	struct dp_soc *soc = pdev->soc;

	switch (hal_get_target_type(soc->hal_soc)) {
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
		filter->tlv_filter.msdu_start = 0;
		filter->tlv_filter.mpdu_end = 0;
		filter->tlv_filter.packet_header = 0;
		filter->tlv_filter.attention = 0;
		break;
	default:
		break;
	}
}

void dp_mon_filter_set_mon_cmn(struct dp_pdev *pdev,
			       struct dp_mon_filter *filter)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 1;
	filter->tlv_filter.packet = 1;
	filter->tlv_filter.msdu_end = 1;
	filter->tlv_filter.mpdu_end = 1;
	filter->tlv_filter.packet_header = 1;
	filter->tlv_filter.attention = 1;
	filter->tlv_filter.ppdu_start = 0;
	filter->tlv_filter.ppdu_end = 0;
	filter->tlv_filter.ppdu_end_user_stats = 0;
	filter->tlv_filter.ppdu_end_user_stats_ext = 0;
	filter->tlv_filter.ppdu_end_status_done = 0;
	filter->tlv_filter.header_per_msdu = 1;
	filter->tlv_filter.enable_fp =
		(mon_pdev->mon_filter_mode & MON_FILTER_PASS) ? 1 : 0;
	filter->tlv_filter.enable_mo =
		(mon_pdev->mon_filter_mode & MON_FILTER_OTHER) ? 1 : 0;

	filter->tlv_filter.fp_mgmt_filter = mon_pdev->fp_mgmt_filter;
	filter->tlv_filter.fp_ctrl_filter = mon_pdev->fp_ctrl_filter;
	filter->tlv_filter.fp_data_filter = mon_pdev->fp_data_filter;
	filter->tlv_filter.mo_mgmt_filter = mon_pdev->mo_mgmt_filter;
	filter->tlv_filter.mo_ctrl_filter = mon_pdev->mo_ctrl_filter;
	filter->tlv_filter.mo_data_filter = mon_pdev->mo_data_filter;
	filter->tlv_filter.offset_valid = false;
	dp_mon_filter_adjust(pdev, filter);
}

void dp_mon_filter_set_status_cmn(struct dp_mon_pdev *mon_pdev,
				  struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 0;
	filter->tlv_filter.packet = 0;
	filter->tlv_filter.msdu_end = 0;
	filter->tlv_filter.mpdu_end = 0;
	filter->tlv_filter.attention = 0;
	filter->tlv_filter.ppdu_start = 1;
	filter->tlv_filter.ppdu_end = 1;
	filter->tlv_filter.ppdu_end_user_stats = 1;
	filter->tlv_filter.ppdu_end_user_stats_ext = 1;
	filter->tlv_filter.ppdu_end_status_done = 1;
	filter->tlv_filter.enable_fp = 1;
	filter->tlv_filter.enable_md = 0;
	filter->tlv_filter.fp_mgmt_filter = FILTER_MGMT_ALL;
	filter->tlv_filter.fp_ctrl_filter = FILTER_CTRL_ALL;
	filter->tlv_filter.fp_data_filter = FILTER_DATA_ALL;
	filter->tlv_filter.offset_valid = false;

	if (mon_pdev->mon_filter_mode & MON_FILTER_OTHER) {
		filter->tlv_filter.enable_mo = 1;
		filter->tlv_filter.mo_mgmt_filter = FILTER_MGMT_ALL;
		filter->tlv_filter.mo_ctrl_filter = FILTER_CTRL_ALL;
		filter->tlv_filter.mo_data_filter = FILTER_DATA_ALL;
	} else {
		filter->tlv_filter.enable_mo = 0;
	}
}

void dp_mon_filter_set_status_cbf(struct dp_pdev *pdev,
				  struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 0;
	filter->tlv_filter.packet = 0;
	filter->tlv_filter.msdu_end = 0;
	filter->tlv_filter.mpdu_end = 0;
	filter->tlv_filter.attention = 0;
	filter->tlv_filter.ppdu_start = 1;
	filter->tlv_filter.ppdu_end = 1;
	filter->tlv_filter.ppdu_end_user_stats = 1;
	filter->tlv_filter.ppdu_end_user_stats_ext = 1;
	filter->tlv_filter.ppdu_end_status_done = 1;
	filter->tlv_filter.enable_fp = 1;
	filter->tlv_filter.enable_md = 0;
	filter->tlv_filter.fp_mgmt_filter = FILTER_MGMT_ACT_NO_ACK;
	filter->tlv_filter.fp_ctrl_filter = 0;
	filter->tlv_filter.fp_data_filter = 0;
	filter->tlv_filter.offset_valid = false;
	filter->tlv_filter.enable_mo = 0;
}

void dp_mon_filter_set_cbf_cmn(struct dp_pdev *pdev,
			       struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 1;
	filter->tlv_filter.packet = 1;
	filter->tlv_filter.msdu_end = 1;
	filter->tlv_filter.mpdu_end = 1;
	filter->tlv_filter.attention = 1;
	filter->tlv_filter.ppdu_start = 0;
	filter->tlv_filter.ppdu_end =  0;
	filter->tlv_filter.ppdu_end_user_stats = 0;
	filter->tlv_filter.ppdu_end_user_stats_ext = 0;
	filter->tlv_filter.ppdu_end_status_done = 0;
	filter->tlv_filter.enable_fp = 1;
	filter->tlv_filter.enable_md = 0;
	filter->tlv_filter.fp_mgmt_filter = FILTER_MGMT_ACT_NO_ACK;
	filter->tlv_filter.offset_valid = false;
	filter->tlv_filter.enable_mo = 0;
}

void dp_mon_filter_dealloc(struct dp_mon_pdev *mon_pdev)
{
	enum dp_mon_filter_mode mode;
	struct dp_mon_filter **mon_filter = NULL;

	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev Context is null");
		return;
	}

	mon_filter = mon_pdev->filter;

	/*
	 * Check if the monitor filters are already allocated to the mon_pdev.
	 */
	if (!mon_filter) {
		dp_mon_filter_err("Found NULL memory for the Monitor filter");
		return;
	}

	/*
	 * Iterate through the every mode and free the filter object.
	 */
	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		if (!mon_filter[mode]) {
			continue;
		}

		qdf_mem_free(mon_filter[mode]);
		mon_filter[mode] = NULL;
	}

	qdf_mem_free(mon_filter);
	mon_pdev->filter = NULL;
}

struct dp_mon_filter **dp_mon_filter_alloc(struct dp_mon_pdev *mon_pdev)
{
	struct dp_mon_filter **mon_filter = NULL;
	enum dp_mon_filter_mode mode;

	if (!mon_pdev) {
		dp_mon_filter_err("pdev Context is null");
		return NULL;
	}

	mon_filter = (struct dp_mon_filter **)qdf_mem_malloc(
			(sizeof(struct dp_mon_filter *) *
			 DP_MON_FILTER_MAX_MODE));
	if (!mon_filter) {
		dp_mon_filter_err("Monitor filter mem allocation failed");
		return NULL;
	}

	qdf_mem_zero(mon_filter,
		     sizeof(struct dp_mon_filter *) * DP_MON_FILTER_MAX_MODE);

	/*
	 * Allocate the memory for filters for different srngs for each modes.
	 */
	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		mon_filter[mode] = qdf_mem_malloc(sizeof(struct dp_mon_filter) *
						  DP_MON_FILTER_SRNG_TYPE_MAX);
		/* Assign the mon_filter to the pdev->filter such
		 * that the dp_mon_filter_dealloc() can free up the filters. */
		if (!mon_filter[mode]) {
			mon_pdev->filter = mon_filter;
			goto fail;
		}
	}

	return mon_filter;
fail:
	dp_mon_filter_dealloc(mon_pdev);
	return NULL;
}
