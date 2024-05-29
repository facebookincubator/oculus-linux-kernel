/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
#ifndef __DP_LI_H
#define __DP_LI_H

#include <dp_types.h>
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif
#include <hal_li_tx.h>
#include <hal_li_rx.h>

/* WBM2SW ring id for rx release */
#define WBM2SW_REL_ERR_RING_NUM 3

/**
 * struct dp_soc_li - Extended DP soc for LI targets
 * @soc: dp soc structure
 */
struct dp_soc_li {
	struct dp_soc soc;
};

/**
 * struct dp_pdev_li - Extended DP pdev for LI targets
 * @pdev: dp_pdev structure
 */
struct dp_pdev_li {
	struct dp_pdev pdev;
};

/**
 * struct dp_vdev_li - Extended DP vdev for LI targets
 * @vdev: dp_vdev structure
 */
struct dp_vdev_li {
	struct dp_vdev vdev;
};

/**
 * struct dp_peer_li - Extended DP peer for LI targets
 * @peer: dp_peer structure
 */
struct dp_peer_li {
	struct dp_peer peer;
};

/**
 * dp_get_soc_context_size_li() - get context size for dp_soc_li
 *
 * Return: value in bytes for LI specific soc structure
 */
qdf_size_t dp_get_soc_context_size_li(void);

/**
 * dp_initialize_arch_ops_li() - initialize LI specific arch ops
 * @arch_ops: arch ops pointer
 *
 * Return: none
 */
void dp_initialize_arch_ops_li(struct dp_arch_ops *arch_ops);

/**
 * dp_get_context_size_li() - get LI specific size for peer/vdev/pdev/soc
 * @context_type: DP context type for which the size is needed
 *
 * Return: size in bytes for the context_type
 */

qdf_size_t dp_get_context_size_li(enum dp_context_type context_type);
#endif
