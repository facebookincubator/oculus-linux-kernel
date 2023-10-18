/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
 * DOC: Declare private API which shall be used internally only
 * in action_oui component. This file shall include prototypes of
 * various notification handlers and logging functions.
 *
 * Note: This API should be never accessed out of action_oui component.
 */

#ifndef _WLAN_ACTION_OUI_MAIN_H_
#define _WLAN_ACTION_OUI_MAIN_H_

#include <qdf_types.h>
#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_priv.h"
#include "wlan_action_oui_objmgr.h"

#define action_oui_log(level, args...) \
	QDF_TRACE(QDF_MODULE_ID_ACTION_OUI, level, ## args)

#define action_oui_logfl(level, format, args...) \
	action_oui_log(level, FL(format), ## args)

#define action_oui_fatal(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define action_oui_err(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define action_oui_warn(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define action_oui_info(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define action_oui_debug(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

#define ACTION_OUI_ENTER() action_oui_debug("enter")
#define ACTION_OUI_EXIT() action_oui_debug("exit")

/**
 * action_oui_psoc_create_notification(): Handler for psoc create notify.
 * @psoc: psoc which is going to be created by objmgr
 * @arg: argument for notification handler.
 *
 * Allocate and attach psoc private object.
 *
 * Return: QDF_STATUS status in case of success else return error.
 */
QDF_STATUS
action_oui_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * action_oui_psoc_destroy_notification(): Handler for psoc destroy notify.
 * @psoc: psoc which is going to be destroyed by objmgr
 * @arg: argument for notification handler.
 *
 * Deallocate and detach psoc private object.
 *
 * Return QDF_STATUS status in case of success else return error
 */
QDF_STATUS
action_oui_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * wlan_action_oui_search() - Check for OUIs and related info in IE data.
 * @psoc: objmgr psoc object
 * @attr: pointer to structure containing type of action, beacon IE data etc.,
 * @action_id: type of action to be checked
 *
 * This is a wrapper function which invokes internal function to search
 * for OUIs and related info (specified from ini file) in vendor specific
 * data of beacon IE for given action.
 *
 * Return: If search is successful return true else false.
 */
#ifdef WLAN_FEATURE_ACTION_OUI
bool wlan_action_oui_search(struct wlan_objmgr_psoc *psoc,
			    struct action_oui_search_attr *attr,
			    enum action_oui_id action_id);

/**
 * wlan_action_oui_is_empty() - Check action oui present or not
 * @psoc: psoc object
 * @action_id: action oui id
 *
 * This function will check action oui present or not for specific action type.
 *
 * Return: True if no action oui for the action type.
 */
bool wlan_action_oui_is_empty(struct wlan_objmgr_psoc *psoc,
			      enum action_oui_id action_id);

/**
 * wlan_action_oui_cleanup() - Remove all of existing oui entry.
 * @psoc_priv: action oui objmgr private context
 * @action_id: type of action to be removed
 *
 * This is a wrapper function which invokes internal function to remove
 * all of existing oui entry.
 *
 * Return: QDF_STATUS_SUCCESS If remove is successful.
 */
QDF_STATUS
wlan_action_oui_cleanup(struct action_oui_psoc_priv *psoc_priv,
			enum action_oui_id action_id);

/**
 * action_oui_psoc_enable() - Notify action OUI psoc enable
 * @psoc: objmgr psoc object
 *
 * Return: void
 */
void action_oui_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * action_oui_psoc_disable() - Notify action OUI psoc disable
 * @psoc: objmgr psoc object
 *
 * Return: void
 */
void action_oui_psoc_disable(struct wlan_objmgr_psoc *psoc);

#else
static inline
bool wlan_action_oui_search(struct wlan_objmgr_psoc *psoc,
			    struct action_oui_search_attr *attr,
			    enum action_oui_id action_id)
{
	return false;
}

static inline
bool wlan_action_oui_is_empty(struct wlan_objmgr_psoc *psoc,
			      enum action_oui_id action_id)
{
	return true;
}

static inline QDF_STATUS
wlan_action_oui_cleanup(struct action_oui_psoc_priv *psoc_priv,
			enum action_oui_id action_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void action_oui_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void action_oui_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
}

#endif
#endif /* end  of _WLAN_ACTION_OUI_MAIN_H_ */
