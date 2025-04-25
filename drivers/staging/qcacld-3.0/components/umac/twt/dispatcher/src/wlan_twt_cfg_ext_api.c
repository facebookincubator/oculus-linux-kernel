/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <wlan_twt_cfg_ext_api.h>
#include "twt/core/src/wlan_twt_cfg.h"
#include "wlan_mlme_api.h"

QDF_STATUS
wlan_twt_cfg_get_req_flag(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_requestor_flag(psoc, val);
}

QDF_STATUS
wlan_twt_cfg_get_res_flag(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_responder_flag(psoc, val);
}

QDF_STATUS
wlan_twt_cfg_get_support_in_11n(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_support_in_11n_mode(psoc, val);
}

QDF_STATUS
wlan_twt_get_requestor_cfg(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_requestor(psoc, val);
}

QDF_STATUS
wlan_twt_get_responder_cfg(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_responder(psoc, val);
}

QDF_STATUS
wlan_twt_cfg_get_support_requestor(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_requestor(psoc, val);
}

QDF_STATUS
wlan_twt_get_rtwt_support(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_get_restricted_support(psoc, val);
}

QDF_STATUS
wlan_twt_get_bcast_requestor_cfg(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_bcast_requestor(psoc, val);
}

QDF_STATUS
wlan_twt_get_bcast_responder_cfg(struct wlan_objmgr_psoc *psoc, bool *val)
{
	return wlan_twt_cfg_get_bcast_responder(psoc, val);
}

#ifdef FEATURE_SET
void wlan_twt_get_feature_info(struct wlan_objmgr_psoc *psoc,
			       struct wlan_twt_features *twt_feature_set)
{
	twt_feature_set->enable_twt = wlan_twt_cfg_is_twt_enabled(psoc);
	if (twt_feature_set->enable_twt) {
		wlan_twt_cfg_get_bcast_requestor(
					psoc,
					&twt_feature_set->enable_twt_broadcast);
		wlan_twt_cfg_get_requestor(
					psoc,
					&twt_feature_set->enable_twt_requester);
		twt_feature_set->enable_twt_flexible = true;
	}
}
#endif
