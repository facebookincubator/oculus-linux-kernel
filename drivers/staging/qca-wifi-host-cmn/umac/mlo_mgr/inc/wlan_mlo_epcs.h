/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains EPCS APIs
 */

#ifndef _WLAN_MLO_EPCS_H_
#define _WLAN_MLO_EPCS_H_

#include <wlan_cmn_ieee80211.h>
#include <wlan_mlo_mgr_public_structs.h>
#ifdef WMI_AP_SUPPORT
#include <wlan_cmn.h>
#endif

struct wlan_mlo_peer_context;

/**
 * enum wlan_epcs_category - epcs category
 *
 * @WLAN_EPCS_CATEGORY_NONE: none
 * @WLAN_EPCS_CATEGORY_REQUEST: EPCS request
 * @WLAN_EPCS_CATEGORY_RESPONSE: EPCS response
 * @WLAN_EPCS_CATEGORY_TEARDOWN: EPCS teardown
 * @WLAN_EPCS_CATEGORY_INVALID: Invalid
 */
enum wlan_epcs_category {
	WLAN_EPCS_CATEGORY_NONE = 0,
	WLAN_EPCS_CATEGORY_REQUEST = 3,
	WLAN_EPCS_CATEGORY_RESPONSE = 4,
	WLAN_EPCS_CATEGORY_TEARDOWN = 5,
	WLAN_EPCS_CATEGORY_INVALID,
};

/**
 * struct ml_pa_partner_link_info - Priority Access ML partner information
 * @link_id: Link ID
 * @edca_ie_present: EDCA IE present
 * @muedca_ie_present: MU EDCA IE present
 * @ven_wme_ie_present: WME IE present
 * @edca: EDCA IE
 * @muedca: MU EDCA IE
 * @ven_wme_ie_bytes: WME IE
 */
struct ml_pa_partner_link_info {
	uint8_t link_id;
	uint8_t edca_ie_present:1,
		muedca_ie_present:1,
		ven_wme_ie_present:1;
	union {
		struct edca_ie edca;
		uint8_t ven_wme_ie_bytes[WLAN_VENDOR_WME_IE_LEN + 2];
	};
	struct muedca_ie muedca;
};

/**
 * struct ml_pa_info - priority access ML info
 * @mld_mac_addr: MLD mac address
 * @num_links: Number of Links
 * @link_info: Partner link information
 */
struct ml_pa_info {
	struct qdf_mac_addr mld_mac_addr;
	uint8_t num_links;
	struct ml_pa_partner_link_info link_info[WLAN_UMAC_MLO_MAX_VDEVS];
};

/**
 * struct wlan_epcs_info - EPCS information of frame
 * @cat: frame category
 * @dialog_token: dialog token
 * @status: status
 * @pa_info: Priority access ML info
 */
struct wlan_epcs_info {
	enum wlan_epcs_category cat;
	uint8_t dialog_token;
	uint16_t status;
	struct ml_pa_info pa_info;
};

/**
 * enum peer_epcs_state - epcs stat of peer
 * @EPCS_DOWN: EPCS state down
 * @EPCS_ENABLE: EPCS state enabled
 */
enum peer_epcs_state {
	EPCS_DOWN,
	EPCS_ENABLE
};

/**
 * struct wlan_mlo_peer_epcs_info - Peer EPCS information
 * @epcs_dev_peer_lock: epcs dev peer lock
 * @state: EPCS state of peer
 * @self_gen_dialog_token: selfgenerated dialog token
 */
struct wlan_mlo_peer_epcs_info {
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t epcs_dev_peer_lock;
#else
	qdf_mutex_t epcs_dev_peer_lock;
#endif
	enum peer_epcs_state state;
	uint8_t self_gen_dialog_token;
};

#define EPCS_MAX_AUTHORIZE_MAC_ADDR 32
/**
 * struct epcs_peer_authorize_info - EPCS authorized mac addresses
 * @valid: valid index if set t0 true
 * @peer_mld_mac: mld mac address
 */
struct epcs_peer_authorize_info {
	bool valid;
	uint8_t peer_mld_mac[QDF_MAC_ADDR_SIZE];
};

/**
 * struct wlan_epcs_context - EPCS context if MLD
 * @epcs_dev_lock: epcs dev context lock
 * @authorize_info: Array of Authorization info containing peer mac address
 */
struct wlan_epcs_context {
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t epcs_dev_lock;
#else
	qdf_mutex_t epcs_dev_lock;
#endif
	struct epcs_peer_authorize_info
			authorize_info[EPCS_MAX_AUTHORIZE_MAC_ADDR];
};

/**
 * struct epcs_frm - EPCS action frame format
 * @category: category
 * @protected_eht_action: Protected EHT Action
 * @dialog_token: Dialog Token
 * @status_code: Status Code
 * @req: Request frame
 * @resp: Response frame
 * @bytes: Priority Access Multi-Link element bytes
 */
struct epcs_frm {
	uint8_t category;
	uint8_t protected_eht_action;
	uint8_t dialog_token;
	union {
		struct {
			uint8_t bytes[0];
		} req;
		struct {
			uint8_t status_code[2];
			uint8_t bytes[0];
		} resp;
	};
};

/* MIN EPCS request frame length */
#define EPCS_REQ_MIN_LENGTH 3

/* MIN EPCS response frame length */
#define EPCS_RESP_MIN_LENGTH 5

#define epcs_alert(format, args...) \
		QDF_TRACE_FATAL(QDF_MODULE_ID_EPCS, format, ## args)

#define epcs_err(format, args...) \
		QDF_TRACE_ERROR(QDF_MODULE_ID_EPCS, format, ## args)

#define epcs_warn(format, args...) \
		QDF_TRACE_WARN(QDF_MODULE_ID_EPCS, format, ## args)

#define epcs_info(format, args...) \
		QDF_TRACE_INFO(QDF_MODULE_ID_EPCS, format, ## args)

#define epcs_debug(format, args...) \
		QDF_TRACE_DEBUG(QDF_MODULE_ID_EPCS, format, ## args)

#define epcs_rl_debug(format, args...) \
		QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_EPCS, format, ## args)

#ifdef WLAN_MLO_USE_SPINLOCK
/**
 * epcs_dev_lock_create - Create EPCS device mutex/spinlock
 * @epcs_ctx: EPCS context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline void
epcs_dev_lock_create(struct wlan_epcs_context *epcs_ctx)
{
	qdf_spinlock_create(&epcs_ctx->epcs_dev_lock);
}

/**
 * epcs_dev_lock_destroy - Destroy EPCS mutex/spinlock
 * @epcs_ctx: EPCS context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
epcs_dev_lock_destroy(struct wlan_epcs_context *epcs_ctx)
{
	qdf_spinlock_destroy(&epcs_ctx->epcs_dev_lock);
}

/**
 * epcs_dev_lock_acquire - acquire EPCS mutex/spinlock
 * @epcs_ctx: EPCS context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void epcs_dev_lock_acquire(struct wlan_epcs_context *epcs_ctx)
{
	qdf_spin_lock_bh(&epcs_ctx->epcs_dev_lock);
}

/**
 * epcs_dev_lock_release - release EPCS dev mutex/spinlock
 * @epcs_ctx: EPCS context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void epcs_dev_lock_release(struct wlan_epcs_context *epcs_ctx)
{
	qdf_spin_unlock_bh(&epcs_ctx->epcs_dev_lock);
}
#else /* WLAN_MLO_USE_SPINLOCK */
static inline
void epcs_dev_lock_create(struct wlan_epcs_context *epcs_ctx)
{
	qdf_mutex_create(&epcs_ctx->epcs_dev_lock);
}

static inline
void epcs_dev_lock_destroy(struct wlan_epcs_context *epcs_ctx)
{
	qdf_mutex_destroy(&epcs_ctx->epcs_dev_lock);
}

static inline void epcs_dev_lock_acquire(struct wlan_epcs_context *epcs_ctx)
{
	qdf_mutex_acquire(&epcs_ctx->epcs_dev_lock);
}

static inline void epcs_dev_lock_release(struct wlan_epcs_context *epcs_ctx)
{
	qdf_mutex_release(&epcs_ctx->epcs_dev_lock);
}
#endif

#ifdef WLAN_MLO_USE_SPINLOCK
/**
 * epcs_dev_peer_lock_create - Create EPCS device mutex/spinlock
 * @epcs_info: EPCS info
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline
void epcs_dev_peer_lock_create(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_spinlock_create(&epcs_info->epcs_dev_peer_lock);
}

/**
 * epcs_dev_peer_lock_destroy - Destroy EPCS mutex/spinlock
 * @epcs_info: EPCS info
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline
void epcs_dev_peer_lock_destroy(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_spinlock_destroy(&epcs_info->epcs_dev_peer_lock);
}

/**
 * epcs_dev_peer_lock_acquire - acquire EPCS mutex/spinlock
 * @epcs_info: EPCS info
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void epcs_dev_peer_lock_acquire(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_spin_lock_bh(&epcs_info->epcs_dev_peer_lock);
}

/**
 * epcs_dev_peer_lock_release - release EPCS dev mutex/spinlock
 * @epcs_info: EPCS info
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void epcs_dev_peer_lock_release(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_spin_unlock_bh(&epcs_info->epcs_dev_peer_lock);
}
#else /* WLAN_MLO_USE_SPINLOCK */
static inline
void epcs_dev_peer_lock_create(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_mutex_create(&epcs_info->epcs_dev_peer_lock);
}

static inline
void epcs_dev_peer_lock_destroy(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_mutex_destroy(&epcs_info->epcs_dev_peer_lock);
}

static inline
void epcs_dev_peer_lock_acquire(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_mutex_acquire(&epcs_info->epcs_dev_peer_lock);
}

static inline
void epcs_dev_peer_lock_release(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	qdf_mutex_release(&epcs_info->epcs_dev_peer_lock);
}
#endif

/**
 * wlan_mlo_add_epcs_action_frame() - API to add EPCS action frame
 * @frm: Pointer to a frame to add EPCS information
 * @args: EPCS action frame related info
 * @buf: Pointer to EPCS IE values
 *
 * Return: Pointer to the updated frame buffer
 */
uint8_t *wlan_mlo_add_epcs_action_frame(uint8_t *frm,
					struct wlan_action_frame_args *args,
					uint8_t *buf);

/**
 * wlan_mlo_parse_epcs_action_frame() - API to parse EPCS action frame
 * @epcs: Pointer to EPCS information
 * @action_frm: EPCS action frame
 * @frm_len: frame length
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlo_parse_epcs_action_frame(struct wlan_epcs_info *epcs,
				 struct wlan_action_frame *action_frm,
				 uint32_t frm_len);

/**
 * wlan_mlo_peer_rcv_cmd() - API to process EPCS command
 * @ml_peer: Pointer to ML peer received
 * @epcs: Pointer to EPCS information
 * @updparam: pointer to fill update parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlo_peer_rcv_cmd(struct wlan_mlo_peer_context *ml_peer,
		      struct wlan_epcs_info *epcs,
		      bool *updparam);

/**
 * wlan_mlo_peer_rcv_action_frame() - API to process EPCS frame receive event
 * @ml_peer: Pointer to ML peer received
 * @epcs: Pointer to EPCS information
 * @respond: pointer to fill response required or not
 * @updparam: pointer to fill update parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlo_peer_rcv_action_frame(struct wlan_mlo_peer_context *ml_peer,
			       struct wlan_epcs_info *epcs,
			       bool *respond,
			       bool *updparam);

/**
 * wlan_mlo_update_authorize_epcs_mac_addr() - API to authorize mac addr
 * @vdev: pointer to vdev
 * @peer_mld_mac: mld mac address
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlo_update_authorize_epcs_mac_addr(struct wlan_objmgr_vdev *vdev,
					uint8_t *peer_mld_mac);

/**
 * wlan_mlo_update_deauthorize_epcs_mac_addr() - API to deauthorize mac addr
 * @vdev: pointer to vdev
 * @peer_mld_mac: mld mac address
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlo_update_deauthorize_epcs_mac_addr(struct wlan_objmgr_vdev *vdev,
					  uint8_t *peer_mld_mac);
#endif /* _WLAN_MLO_EPCS_H_ */
