/*
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
#ifndef __DP_MLO_H
#define __DP_MLO_H

#include <dp_types.h>
#include <dp_peer.h>

/* Max number of chips that can participate in MLO */
#define DP_MAX_MLO_CHIPS WLAN_MAX_MLO_CHIPS

/* Max number of peers supported */
#define DP_MAX_MLO_PEER 512

/* Max number of chips supported */
#define DP_MLO_MAX_DEST_CHIP_ID 3

/*
 * dp_mlo_ctxt
 *
 * @ctrl_ctxt: opaque handle of cp mlo mgr
 * @ml_soc_list: list of socs which are mlo enabled. This also maintains
 *               mlo_chip_id to dp_soc mapping
 * @ml_soc_cnt: number of SOCs
 * @ml_soc_list_lock: lock to protect ml_soc_list
 * @mld_peer_hash: peer hash table for ML peers
 *           Associated peer with this MAC address)
 * @mld_peer_hash_lock: lock to protect mld_peer_hash
 * @link_to_pdev_map: link to pdev mapping
 * @rx_fst: pointer to rx_fst handle
 * @rx_fst_ref_cnt: ref count of rx_fst
 */
struct dp_mlo_ctxt {
	struct cdp_ctrl_mlo_mgr *ctrl_ctxt;
	struct dp_soc *ml_soc_list[DP_MAX_MLO_CHIPS];
	uint8_t ml_soc_cnt;
	qdf_spinlock_t ml_soc_list_lock;
	struct {
		uint32_t mask;
		uint32_t idx_bits;

		TAILQ_HEAD(, dp_peer) * bins;
	} mld_peer_hash;

	qdf_spinlock_t mld_peer_hash_lock;
	uint32_t toeplitz_hash_ipv4[LRO_IPV4_SEED_ARR_SZ];
	uint32_t toeplitz_hash_ipv6[LRO_IPV6_SEED_ARR_SZ];
	struct dp_pdev_be *link_to_pdev_map[WLAN_MAX_MLO_CHIPS *
		WLAN_MAX_MLO_LINKS_PER_SOC];
	struct dp_rx_fst *rx_fst;
	uint8_t rx_fst_ref_cnt;
};

/**
 * dp_mlo_ctx_to_cdp() - typecast dp mlo context to CDP context
 * @mlo_ctxt: DP MLO context
 *
 * Return: struct cdp_mlo_ctxt pointer
 */
static inline
struct cdp_mlo_ctxt *dp_mlo_ctx_to_cdp(struct dp_mlo_ctxt *mlo_ctxt)
{
	return (struct cdp_mlo_ctxt *)mlo_ctxt;
}

/**
 * cdp_mlo_ctx_to_dp() - typecast cdp_soc_t to
 * dp soc handle
 * @psoc: CDP psoc handle
 *
 * Return: struct dp_soc pointer
 */
static inline
struct dp_mlo_ctxt *cdp_mlo_ctx_to_dp(struct cdp_mlo_ctxt *mlo_ctxt)
{
	return (struct dp_mlo_ctxt *)mlo_ctxt;
}

/**
 * dp_soc_mlo_fill_params() - update SOC mlo params
 * @soc: DP soc
 * @params: soc attach params
 *
 * Return: struct dp_soc pointer
 */
void dp_soc_mlo_fill_params(struct dp_soc *soc,
			    struct cdp_soc_attach_params *params);

/**
 * dp_pdev_mlo_fill_params() - update PDEV mlo params
 * @pdev: DP PDEV
 * @params: PDEV attach params
 *
 * Return: struct dp_soc pointer
 */
void dp_pdev_mlo_fill_params(struct dp_pdev *pdev,
			     struct cdp_pdev_attach_params *params);
struct dp_soc*
dp_mlo_get_soc_ref_by_chip_id(struct dp_mlo_ctxt *ml_ctxt, uint8_t chip_id);

/**
 * dp_mlo_get_rx_hash_key() - Get Rx hash key from MLO context
 * @soc: DP SOC
 * @lro_hash: Hash params
 *
 */
void dp_mlo_get_rx_hash_key(struct dp_soc *soc,
			    struct cdp_lro_hash_config *lro_hash);

/**
 * dp_mlo_rx_fst_deref() - decrement rx_fst
 * @soc: dp soc
 *
 * return: soc cnt
 */
uint8_t dp_mlo_rx_fst_deref(struct dp_soc *soc);

/**
 * dp_mlo_rx_fst_ref() - increment ref of rx_fst
 * @soc: dp soc
 *
 */
void dp_mlo_rx_fst_ref(struct dp_soc *soc);

/**
 * dp_mlo_get_rx_fst() - Get Rx FST from MLO context
 * @soc: DP SOC
 *
 * Return: struct dp_rx_fst pointer
 */
struct dp_rx_fst *dp_mlo_get_rx_fst(struct dp_soc *soc);

/**
 * dp_mlo_set_rx_fst() - Set Rx FST in MLO context
 * @soc: DP SOC
 * @fst: pointer dp_rx_fst
 *
 */
void dp_mlo_set_rx_fst(struct dp_soc *soc, struct dp_rx_fst *fst);

/**
 * dp_mlo_update_link_to_pdev_map : map link-id to pdev mapping
 * @soc: DP SOC
 * @pdev: DP PDEV
 *
 * Return: none
 */
void dp_mlo_update_link_to_pdev_map(struct dp_soc *soc, struct dp_pdev *pdev);

/**
 * dp_mlo_update_link_to_pdev_unmap : unmap link-id to pdev mapping
 * @soc: DP SOC
 * @pdev: DP PDEV
 *
 * Return: none
 */
void dp_mlo_update_link_to_pdev_unmap(struct dp_soc *soc, struct dp_pdev *pdev);

/**
 * dp_mlo_get_delta_tsf2_wrt_mlo_offset() - Get delta between mlo timestamp
 *                                          offset and delta tsf2
 * @soc: DP SOC
 * @hw_link_id: link id
 *
 * Return: int32_t
 */
int32_t dp_mlo_get_delta_tsf2_wrt_mlo_offset(struct dp_soc *soc,
					     uint8_t hw_link_id);

/**
 * dp_mlo_get_delta_tqm_wrt_mlo_offset() - Get delta between mlo timestamp
 *                                         offset and delta tqm
 * @soc: DP SOC
 *
 * Return: int32_t
 */
int32_t dp_mlo_get_delta_tqm_wrt_mlo_offset(struct dp_soc *soc);
#endif /* __DP_MLO_H */
