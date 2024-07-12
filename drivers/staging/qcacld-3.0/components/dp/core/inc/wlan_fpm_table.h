/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_DP_FPM_H_
#define _WLAN_DP_FPM_H_

#include <dp_types.h>
#include <dp_internal.h>
#include <qdf_status.h>
#include <qdf_nbuf.h>
#include "wlan_dp_public_struct.h"
#include "wlan_dp_priv.h"

/*
 * struct policy_notifier_data - Policy operation notifier data
 * @flow: Flow tuple
 * @policy_id: Unique policy ID
 * @prio: Policy priority
 */
struct policy_notifier_data {
	struct flow_info flow;
	uint64_t policy_id;
	uint8_t prio;
};

/*
 * enum fpm_policy_event - Event types
 * @FPM_POLICY_ADD: Policy add event
 * @FPM_POLICY_DEL: Policy delete event
 * @FPM_POLICY_UPDATE: Policy update event
 */
enum fpm_policy_event {
	FPM_POLICY_INVALID = 0,
	FPM_POLICY_ADD,
	FPM_POLICY_DEL,
	FPM_POLICY_UPDATE,
};

/*
 * enum dp_fpm_status - Flow match status
 * @FLOW_MATCH_FOUND: Flow matched
 * @FLOW_MATCH_DO_NOT_FOUND: Flow doesn't match
 * @FLOW_MATCH_SKIP: Flow match skipped
 * @FLOW_MATCH_FAIL: Flow match failed
 */
enum dp_fpm_status {
	FLOW_MATCH_FOUND,
	FLOW_MATCH_DO_NOT_FOUND,
	FLOW_MATCH_SKIP,
	FLOW_MATCH_FAIL
};

/*
 * Flow policy management
 */
/*
 * fpm_policy_event_register_notifier() - FPM notifier register callback
 * @dp_intf: Interface level dp private structure
 * @nb: Notifier block
 *
 * Return: Return QDF_STATUS
 */
QDF_STATUS fpm_policy_event_register_notifier(struct wlan_dp_intf *dp_intf,
					      qdf_notif_block *nb);

/*
 * fpm_policy_event_unregister_notifier() - FPM notifier unregister callback
 * @dp_intf: Interface level dp private structure
 * @nb: Notifier block
 *
 * Return: Return QDF_STATUS
 */
QDF_STATUS fpm_policy_event_unregister_notifier(struct wlan_dp_intf *dp_intf,
						qdf_notif_block *nb);

/*
 * dp_fpm_init() - FPM init
 * @dp_intf: Interface level dp private structure
 *
 * Return: Return QDF_STATUS
 */
QDF_STATUS dp_fpm_init(struct wlan_dp_intf *dp_intf);

/*
 * dp_fpm_deinit() - FPM deinit
 * @dp_intf: Interface level dp private structure
 *
 * Return: void
 */
void dp_fpm_deinit(struct wlan_dp_intf *dp_intf);

/*
 * fpm_policy_flow_match() - Find flow match and get metadata
 * @dp_intf: Interface level dp private structure
 * @skb: skb
 * @metadata: Metadata to be filled if flow matches
 * @flow: Flow used for matching with configured FPM flows
 * @policy_id: Policy ID to be filled if flow matches
 *
 * Return: enum dp_fpm_status
 */
enum dp_fpm_status fpm_policy_flow_match(struct wlan_dp_intf *dp_intf,
					 qdf_nbuf_t skb, uint32_t *metadata,
					 struct flow_info *flow,
					 uint32_t *policy_id);

/*
 * dp_fpm_display_policy() - Display configured FPM flows
 * @dp_intf: Interface level dp private structure
 *
 * Return: void
 */
void dp_fpm_display_policy(struct wlan_dp_intf *dp_intf);

/*
 * fpm_policy_add() - FPM policy add
 * @fpm: FPM context
 * @policy: Flow Policy
 *
 * Return: 0 if successful
 */
QDF_STATUS fpm_policy_add(struct fpm_table *fpm, struct dp_policy *policy);

/*
 * fpm_policy_update() - FPM policy update
 * @fpm: FPM context
 * @policy: Flow Policy
 *
 * Return: 0 if successful
 */
QDF_STATUS fpm_policy_update(struct fpm_table *fpm, struct dp_policy *policy);

/*
 * fpm_policy_rem() - FPM policy remove
 * @fpm: FPM context
 * @policy: Flow Policy
 *
 * Return: 0 if successful
 */
QDF_STATUS fpm_policy_rem(struct fpm_table *fpm, uint64_t cookie);

/*
 * fpm_policy_get() - FPM policy list get
 * @fpm: FPM context
 * @policies: array of policies
 * @max_count: maximum policies
 *
 * Return: policy count
 */
uint8_t fpm_policy_get(struct fpm_table *fpm, struct dp_policy *policies,
		       uint8_t max_count);

/*
 * fpm_flow_regex_match() - Match first flow available parameters with second
 *			    flow parameters
 * @tflow: first flow tuple
 * @flow: second flow tuple
 *
 * Return: True if first flow tuple matches with second flow tuple
 */
bool fpm_flow_regex_match(struct flow_info *tflow,
			  struct flow_info *flow);

/*
 * fpm_is_tid_override() - Check TID override enabled
 * @nbuf: nbuf
 * @tid: filled with target tid if TID override is enabled
 *
 * Return: True if TID override is enabled for nbuf
 */
bool fpm_is_tid_override(qdf_nbuf_t nbuf, uint8_t *tid);

/*
 * fpm_check_tid_override_tagged() - Check if TID override is marked
 * @nbuf: nbuf
 *
 * Return: True if nbuf merked with TID override tag
 */
bool fpm_check_tid_override_tagged(qdf_nbuf_t nbuf);

#endif
