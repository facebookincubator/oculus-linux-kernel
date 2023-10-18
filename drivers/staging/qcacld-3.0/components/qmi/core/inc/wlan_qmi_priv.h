/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: wlan_qmi_priv.h
 *
 * Declare various struct, macros which are used private to QMI component.
 */

#ifndef _WLAN_QMI_PRIV_H_
#define _WLAN_QMI_PRIV_H_

#include "wlan_qmi_public_struct.h"
#include "wlan_objmgr_psoc_obj.h"

/**
 * struct wlan_qmi_psoc_context - psoc related data required for QMI
 * @psoc: object manager psoc context
 * @qmi_cbs: QMI callbacks
 */
struct wlan_qmi_psoc_context {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_qmi_psoc_callbacks qmi_cbs;
};
#endif
