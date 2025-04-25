/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __DP_RH_HTT_H
#define __DP_RH_HTT_H

#include <htt.h>
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_htt.h"
#include "qdf_mem.h"
#include "cdp_txrx_cmn_struct.h"

/* sizeof(struct htt_t2h_soft_umac_tx_compl_ind) */
#define HTT_SOFT_UMAC_TX_COMPL_IND_SIZE	(1 * 4) //in bytes

/* sizeof(struct htt_t2h_tx_msdu_info) */
#define HTT_TX_MSDU_INFO_SIZE	(8 * 4) //in bytes

/*
 * dp_htt_h2t_rx_ring_rfs_cfg() - RFS config for RX DATA indication
 * @htt_soc: Opaque htt SOC handle
 *
 * Return: QDF_STATUS success or failure
 */
QDF_STATUS dp_htt_h2t_rx_ring_rfs_cfg(struct htt_soc *soc);

/*
 * dp_htt_soc_initialize_rh() - SOC level HTT initialization
 * @htt_soc: Opaque htt SOC handle
 * @ctrl_psoc: Opaque ctrl SOC handle
 * @htc_soc: SOC level HTC handle
 * @hal_soc: Opaque HAL SOC handle
 * @osdev: QDF device
 *
 * Return: HTT handle on success; NULL on failure
 */
void *
dp_htt_soc_initialize_rh(struct htt_soc *htt_soc,
			 struct cdp_ctrl_objmgr_psoc *ctrl_psoc,
			 HTC_HANDLE htc_soc,
			 hal_soc_handle_t hal_soc_hdl, qdf_device_t osdev);
#endif
