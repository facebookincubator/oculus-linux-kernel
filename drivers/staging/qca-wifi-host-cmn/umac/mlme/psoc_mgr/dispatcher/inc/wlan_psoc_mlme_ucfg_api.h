/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_psoc_mlme_ucfg_api.h
 * This file provides PSOC mlme ucfg apis.
 */
#ifndef _WLAN_PSOC_MLME_UCFG_API_H_
#define _WLAN_PSOC_MLME_UCFG_API_H_

#include <wlan_objmgr_psoc_obj.h>

/**
 * ucfg_psoc_mlme_get_11be_capab() - Get the 11be capability for target
 * @psoc: psoc handle
 * @val: pointer to the output variable
 *
 * return: QDF_STATUS
 */
QDF_STATUS
ucfg_psoc_mlme_get_11be_capab(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * ucfg_psoc_mlme_set_11be_capab() - Set the 11be capability for target
 * @psoc: psoc handle
 * @val: pointer to the output variable
 *
 * return: QDF_STATUS
 */
QDF_STATUS
ucfg_psoc_mlme_set_11be_capab(struct wlan_objmgr_psoc *psoc, bool val);

#endif /* _WLAN_PSOC_MLME_UCFG_API_H_ */

