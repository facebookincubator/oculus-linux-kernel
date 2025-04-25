/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#ifndef _DP_RX_TID_H_
#define _DP_RX_TID_H_

#include "dp_types.h"

/*
 * dp_rxtid_stats_cmd_cb - function pointer for peer
 *			rx tid stats cmd call_back
 * @soc:
 * @cb_ctxt:
 * @reo_status:
 */
typedef void (*dp_rxtid_stats_cmd_cb)(struct dp_soc *soc, void *cb_ctxt,
				      union hal_reo_status *reo_status);

#ifndef WLAN_SOFTUMAC_SUPPORT
void dp_rx_tid_stats_cb(struct dp_soc *soc, void *cb_ctxt,
			union hal_reo_status *reo_status);

/**
 * dp_peer_rx_cleanup() - Cleanup receive TID state
 * @vdev: Datapath vdev
 * @peer: Datapath peer
 *
 */
void dp_peer_rx_cleanup(struct dp_vdev *vdev, struct dp_peer *peer);

/**
 * dp_rx_tid_setup_wifi3() - Set up receive TID state
 * @peer: Datapath peer handle
 * @tid_bitmap: TIDs to be set up
 * @ba_window_size: BlockAck window size
 * @start_seq: Starting sequence number
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS dp_rx_tid_setup_wifi3(struct dp_peer *peer, uint32_t tid_bitmap,
				 uint32_t ba_window_size, uint32_t start_seq);

/**
 * dp_rx_tid_update_wifi3() - Update receive TID state
 * @peer: Datapath peer handle
 * @tid: TID
 * @ba_window_size: BlockAck window size
 * @start_seq: Starting sequence number
 * @bar_update: BAR update triggered
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS dp_rx_tid_update_wifi3(struct dp_peer *peer, int tid,
				  uint32_t ba_window_size, uint32_t start_seq,
				  bool bar_update);

/*
 * dp_rx_tid_delete_cb() - Callback to flush reo descriptor HW cache
 * after deleting the entries (ie., setting valid=0)
 *
 * @soc: DP SOC handle
 * @cb_ctxt: Callback context
 * @reo_status: REO command status
 */
void dp_rx_tid_delete_cb(struct dp_soc *soc,
			 void *cb_ctxt,
			 union hal_reo_status *reo_status);

#ifdef IPA_OFFLOAD
void dp_peer_update_tid_stats_from_reo(struct dp_soc *soc, void *cb_ctxt,
				       union hal_reo_status *reo_status);
int dp_peer_get_rxtid_stats_ipa(struct dp_peer *peer,
				dp_rxtid_stats_cmd_cb dp_stats_cmd_cb);
#endif

#ifdef DP_UMAC_HW_RESET_SUPPORT
void dp_reset_tid_q_setup(struct dp_soc *soc);
#endif

/**
 * dp_addba_resp_tx_completion_wifi3() - Update Rx Tid State
 *
 * @cdp_soc: Datapath soc handle
 * @peer_mac: Datapath peer mac address
 * @vdev_id: id of atapath vdev
 * @tid: TID number
 * @status: tx completion status
 * Return: 0 on success, error code on failure
 */
int dp_addba_resp_tx_completion_wifi3(struct cdp_soc_t *cdp_soc,
				      uint8_t *peer_mac,
				      uint16_t vdev_id,
				      uint8_t tid, int status);

/**
 * dp_addba_responsesetup_wifi3() - Process ADDBA request from peer
 * @cdp_soc: Datapath soc handle
 * @peer_mac: Datapath peer mac address
 * @vdev_id: id of atapath vdev
 * @tid: TID number
 * @dialogtoken: output dialogtoken
 * @statuscode: output dialogtoken
 * @buffersize: Output BA window size
 * @batimeout: Output BA timeout
 */
QDF_STATUS
dp_addba_responsesetup_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			     uint16_t vdev_id, uint8_t tid,
			     uint8_t *dialogtoken, uint16_t *statuscode,
			     uint16_t *buffersize, uint16_t *batimeout);

/**
 * dp_rx_tid_update_ba_win_size() - Update the DP tid BA window size
 * @cdp_soc: soc handle
 * @peer_mac: mac address of peer handle
 * @vdev_id: id of vdev handle
 * @tid: tid
 * @buffersize: BA window size
 *
 * Return: success/failure of tid update
 */
QDF_STATUS dp_rx_tid_update_ba_win_size(struct cdp_soc_t *cdp_soc,
					uint8_t *peer_mac, uint16_t vdev_id,
					uint8_t tid, uint16_t buffersize);

/**
 * dp_addba_requestprocess_wifi3() - Process ADDBA request from peer
 * @cdp_soc: Datapath soc handle
 * @peer_mac: Datapath peer mac address
 * @vdev_id: id of atapath vdev
 * @dialogtoken: dialogtoken from ADDBA frame
 * @tid: TID number
 * @batimeout: BA timeout
 * @buffersize: BA window size
 * @startseqnum: Start seq. number received in BA sequence control
 *
 * Return: 0 on success, error code on failure
 */
int dp_addba_requestprocess_wifi3(struct cdp_soc_t *cdp_soc,
				  uint8_t *peer_mac,
				  uint16_t vdev_id,
				  uint8_t dialogtoken,
				  uint16_t tid, uint16_t batimeout,
				  uint16_t buffersize,
				  uint16_t startseqnum);

/**
 * dp_set_addba_response() - Set a user defined ADDBA response status code
 * @cdp_soc: Datapath soc handle
 * @peer_mac: Datapath peer mac address
 * @vdev_id: id of atapath vdev
 * @tid: TID number
 * @statuscode: response status code to be set
 */
QDF_STATUS
dp_set_addba_response(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
		      uint16_t vdev_id, uint8_t tid, uint16_t statuscode);

/**
 * dp_delba_process_wifi3() - Process DELBA from peer
 * @cdp_soc: Datapath soc handle
 * @peer_mac: Datapath peer mac address
 * @vdev_id: id of atapath vdev
 * @tid: TID number
 * @reasoncode: Reason code received in DELBA frame
 *
 * Return: 0 on success, error code on failure
 */
int dp_delba_process_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			   uint16_t vdev_id, int tid, uint16_t reasoncode);

/**
 * dp_delba_tx_completion_wifi3() -  Handle delba tx completion
 * @cdp_soc: soc handle
 * @peer_mac: peer mac address
 * @vdev_id: id of the vdev handle
 * @tid: Tid number
 * @status: Tx completion status
 *
 * Indicate status of delba Tx to DP for stats update and retry
 * delba if tx failed.
 *
 * Return: 0 on success, error code on failure
 */
int dp_delba_tx_completion_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
				 uint16_t vdev_id,
				 uint8_t tid, int status);

/**
 * dp_set_pn_check_wifi3() - enable PN check in REO for security
 * @soc: Datapath soc handle
 * @vdev_id: id of atapath vdev
 * @peer_mac: Datapath peer mac address
 * @sec_type: security type
 * @rx_pn: Receive pn starting number
 *
 */
QDF_STATUS
dp_set_pn_check_wifi3(struct cdp_soc_t *soc, uint8_t vdev_id,
		      uint8_t *peer_mac, enum cdp_sec_type sec_type,
		      uint32_t *rx_pn);
QDF_STATUS
dp_rx_delba_ind_handler(void *soc_handle, uint16_t peer_id,
			uint8_t tid, uint16_t win_sz);

/**
 * dp_peer_rxtid_stats() - Retried Rx TID (REO queue) stats from HW
 * @peer: DP peer handle
 * @dp_stats_cmd_cb: REO command callback function
 * @cb_ctxt: Callback context
 *
 * Return: count of tid stats cmd send succeeded
 */
int dp_peer_rxtid_stats(struct dp_peer *peer,
			dp_rxtid_stats_cmd_cb dp_stats_cmd_cb,
			void *cb_ctxt);
QDF_STATUS dp_peer_rx_tids_create(struct dp_peer *peer);
void dp_peer_rx_tids_destroy(struct dp_peer *peer);

#ifdef DUMP_REO_QUEUE_INFO_IN_DDR
/**
 * dp_send_cache_flush_for_rx_tid() - Send cache flush cmd to REO per tid
 * @soc : dp_soc handle
 * @peer: peer
 *
 * This function is used to send cache flush cmd to reo and
 * to register the callback to handle the dumping of the reo
 * queue stas from DDR
 *
 * Return: none
 */
void dp_send_cache_flush_for_rx_tid(struct dp_soc *soc, struct dp_peer *peer);

/**
 * dp_get_rx_reo_queue_info() - Handler to get rx tid info
 * @soc_hdl : cdp_soc_t handle
 * @vdev_id: vdev id
 *
 * Handler to get rx tid info from DDR after h/w cache is
 * invalidated first using the cache flush cmd.
 *
 * Return: none
 */
void dp_get_rx_reo_queue_info(struct cdp_soc_t *soc_hdl, uint8_t vdev_id);

/**
 * dp_dump_rx_reo_queue_info() - Callback function to dump reo queue stats
 * @soc : dp_soc handle
 * @cb_ctxt: callback context
 * @reo_status: vdev id
 *
 * This is the callback function registered after sending the reo cmd
 * to flush the h/w cache and invalidate it. In the callback the reo
 * queue desc info is dumped from DDR.
 *
 * Return: none
 */
void dp_dump_rx_reo_queue_info(struct dp_soc *soc, void *cb_ctxt,
			       union hal_reo_status *reo_status);

#else /* DUMP_REO_QUEUE_INFO_IN_DDR */

static inline void dp_get_rx_reo_queue_info(struct cdp_soc_t *soc_hdl,
					    uint8_t vdev_id)
{
}
#endif /* DUMP_REO_QUEUE_INFO_IN_DDR */
void dp_peer_rx_tid_setup(struct dp_peer *peer);
#else
static inline void dp_rx_tid_stats_cb(struct dp_soc *soc, void *cb_ctxt,
				      union hal_reo_status *reo_status) {}
static inline void dp_peer_rx_cleanup(struct dp_vdev *vdev,
				      struct dp_peer *peer) {}
static inline int dp_addba_resp_tx_completion_wifi3(struct cdp_soc_t *cdp_soc,
						    uint8_t *peer_mac,
						    uint16_t vdev_id,
						    uint8_t tid, int status)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_addba_responsesetup_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			     uint16_t vdev_id, uint8_t tid,
			     uint8_t *dialogtoken, uint16_t *statuscode,
			     uint16_t *buffersize, uint16_t *batimeout)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_rx_tid_update_ba_win_size(struct cdp_soc_t *cdp_soc,
			     uint8_t *peer_mac, uint16_t vdev_id,
			     uint8_t tid, uint16_t buffersize)
{
	return QDF_STATUS_SUCCESS;
}

static inline int
dp_addba_requestprocess_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
			      uint16_t vdev_id, uint8_t dialogtoken,
			      uint16_t tid, uint16_t batimeout,
			      uint16_t buffersize, uint16_t startseqnum)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_set_addba_response(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
		      uint16_t vdev_id, uint8_t tid, uint16_t statuscode)
{
	return QDF_STATUS_SUCCESS;
}

static inline int
dp_delba_process_wifi3(struct cdp_soc_t *cdp_soc, uint8_t *peer_mac,
		       uint16_t vdev_id, int tid, uint16_t reasoncode)
{
	return QDF_STATUS_SUCCESS;
}

static inline int dp_delba_tx_completion_wifi3(struct cdp_soc_t *cdp_soc,
					       uint8_t *peer_mac,
					       uint16_t vdev_id,
					       uint8_t tid, int status)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_set_pn_check_wifi3(struct cdp_soc_t *soc, uint8_t vdev_id,
		      uint8_t *peer_mac, enum cdp_sec_type sec_type,
		      uint32_t *rx_pn)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
dp_rx_delba_ind_handler(void *soc_handle, uint16_t peer_id,
			uint8_t tid, uint16_t win_sz)
{
	return QDF_STATUS_SUCCESS;
}

static inline int
dp_peer_rxtid_stats(struct dp_peer *peer,
		    dp_rxtid_stats_cmd_cb dp_stats_cmd_cb,
		    void *cb_ctxt)
{
	return 0;
}

static inline QDF_STATUS dp_peer_rx_tids_create(struct dp_peer *peer)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_peer_rx_tids_destroy(struct dp_peer *peer) {}

static inline void dp_get_rx_reo_queue_info(struct cdp_soc_t *soc_hdl,
					    uint8_t vdev_id) {}
static inline void dp_peer_rx_tid_setup(struct dp_peer *peer) {}

static inline QDF_STATUS
dp_rx_tid_setup_wifi3(struct dp_peer *peer, int tid,
		      uint32_t ba_window_size, uint32_t start_seq)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif /* _DP_RX_TID_H_ */
