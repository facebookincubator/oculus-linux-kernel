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

/**
 * DOC: Declare private data structures and APIs which shall be used
 * internally only in twt component.
 *
 * Note: This API should be never accessed out of twt component.
 */

#ifndef _WLAN_TWT_PRIV_H_
#define _WLAN_TWT_PRIV_H_

#include <wlan_twt_public_structs.h>
#include <wlan_twt_ext_defs.h>
#include <wlan_twt_ext_type.h>

/**
 * struct twt_tgt_caps -
 * @twt_requestor: twt requestor
 * @twt_responder: twt responder
 * @legacy_bcast_twt_support: legacy bcast twt support
 * @twt_bcast_req_support: bcast requestor support
 * @twt_bcast_res_support: bcast responder support
 * @twt_nudge_enabled: twt nudge enabled
 * @all_twt_enabled: all twt enabled
 * @twt_stats_enabled: twt stats enabled
 * @twt_ack_supported: twt ack supported
 * @restricted_twt_support: Restricted TWT supported
 */
struct twt_tgt_caps {
	bool twt_requestor;
	bool twt_responder;
	bool legacy_bcast_twt_support;
	bool twt_bcast_req_support;
	bool twt_bcast_res_support;
	bool twt_nudge_enabled;
	bool all_twt_enabled;
	bool twt_stats_enabled;
	bool twt_ack_supported;
	bool restricted_twt_support;
};

/**
 * struct twt_psoc_priv_obj -
 * @cfg_params: cfg params
 * @twt_caps: twt caps
 * @enable_context: enable context
 * @disable_context: disable context
 * @twt_pmo_disabled: twt pmo disabled
 */
struct twt_psoc_priv_obj {
	psoc_twt_ext_cfg_params_t cfg_params;
	struct twt_tgt_caps twt_caps;
	struct twt_en_dis_context enable_context;
	struct twt_en_dis_context disable_context;
	uint32_t twt_pmo_disabled;
};

/**
 * struct twt_vdev_priv_obj -
 * @twt_wait_for_notify: wait for notify
 * @dialog_id: TWT dialog id
 * @peer_macaddr: Peer mac address
 * @is_ps_disabled: Whether power save is disabled or not
 * @next_action: next action of TWT worker queue
 */
struct twt_vdev_priv_obj {
	bool twt_wait_for_notify;
	uint32_t dialog_id;
	struct qdf_mac_addr peer_macaddr;
	bool is_ps_disabled;
	enum HOST_TWT_NEXT_WORK_ACTION next_action;
};

/**
 * struct twt_session -
 * @dialog_id: dialog id
 * @state: state
 * @setup_done: setup done
 * @active_cmd: active command
 * @twt_ack_ctx: twt ack context
 * @wake_dur: TWT wake duration
 * @wake_interval: TWT wake interval
 */
struct twt_session {
	uint8_t dialog_id;
	uint8_t state;
	bool setup_done;
	enum wlan_twt_commands active_cmd;
	void *twt_ack_ctx;
	uint32_t wake_dur;
	uint32_t wake_interval;
};

/**
 * struct twt_peer_priv_obj -
 * @twt_peer_lock: peer lock
 * @peer_capability: peer capability
 * @num_twt_sessions: number of twt sessions
 * @session_info: session info
 */
struct twt_peer_priv_obj {
#ifdef WLAN_TWT_SPINLOCK
	qdf_spinlock_t twt_peer_lock;
#else
	qdf_mutex_t twt_peer_lock;
#endif
	uint8_t peer_capability;
	uint8_t num_twt_sessions;
	struct twt_session session_info
		[WLAN_MAX_TWT_SESSIONS_PER_PEER];
};

#ifdef WLAN_TWT_SPINLOCK
/**
 * twt_lock_create - Create TWT peer mutex/spinlock
 * @twt_lock: lock object
 *
 * Creates TWT peer mutex/spinlock
 *
 * Return: void
 */
static inline void
twt_lock_create(qdf_spinlock_t *twt_lock)
{
	qdf_spinlock_create(twt_lock);
}

/**
 * twt_lock_destroy - Destroy TWT mutex/spinlock
 * @twt_lock: lock object
 *
 * Destroy TWT peer mutex/spinlock
 *
 * Return: void
 */
static inline void
twt_lock_destroy(qdf_spinlock_t *twt_lock)
{
	qdf_spinlock_destroy(twt_lock);
}

/**
 * twt_lock_acquire - acquire TWT mutex/spinlock
 * @twt_lock: lock object
 *
 * acquire TWT mutex/spinlock
 *
 * return: void
 */
static inline void twt_lock_acquire(qdf_spinlock_t *twt_lock)
{
	qdf_spin_lock_bh(twt_lock);
}

/**
 * twt_lock_release - release TWT mutex/spinlock
 * @twt_lock: lock object
 *
 * release TWT mutex/spinlock
 *
 * return: void
 */
static inline void twt_lock_release(qdf_spinlock_t *twt_lock)
{
	qdf_spin_unlock_bh(twt_lock);
}
#else
static inline void
twt_lock_create(qdf_mutex_t *twt_lock)
{
	qdf_mutex_create(twt_lock);
}

static inline void
twt_lock_destroy(qdf_mutex_t *twt_lock)
{
	qdf_mutex_destroy(twt_lock);
}

static inline void twt_lock_acquire(qdf_mutex_t *twt_lock)
{
	qdf_mutex_acquire(twt_lock);
}

static inline void twt_lock_release(qdf_mutex_t *twt_lock)
{
	qdf_mutex_release(twt_lock);
}
#endif /* WLAN_TWT_SPINLOCK */

#endif /* End  of _WLAN_TWT_PRIV_H_ */

