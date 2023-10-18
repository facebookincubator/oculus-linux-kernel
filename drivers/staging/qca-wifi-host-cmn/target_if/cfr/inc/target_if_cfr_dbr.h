/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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

#ifndef _TARGET_IF_CFR_DBR_H_
#define _TARGET_IF_CFR_DBR_H_

#define STREAMFS_MAX_SUBBUF_8S 8500
#define STREAMFS_NUM_SUBBUF_8S 255

/* Values used for computing number of tones */
#define TONES_IN_20MHZ  256
#define TONES_IN_40MHZ  512
#define TONES_IN_80MHZ  1024
#define TONES_IN_160MHZ 2048 /* 160 MHz isn't supported yet */
#define TONES_INVALID   0

#ifdef WLAN_CFR_DBR
/**
 * cfr_dbr_init_pdev() - Inits cfr pdev and registers necessary handlers.
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 *
 * Return: Registration status for necessary handlers
 */
QDF_STATUS cfr_dbr_init_pdev(struct wlan_objmgr_psoc *psoc,
			     struct wlan_objmgr_pdev *pdev);

/**
 * cfr_dbr_deinit_pdev() - De-inits corresponding pdev and handlers.
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 *
 * Return: De-registration status for necessary handlers
 */
QDF_STATUS cfr_dbr_deinit_pdev(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_pdev *pdev);
#else
static inline
QDF_STATUS cfr_dbr_init_pdev(struct wlan_objmgr_psoc *psoc,
			     struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS cfr_dbr_deinit_pdev(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif

