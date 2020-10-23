/*
 * Copyright (c) 2015-2017 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 * Host WMI unified implementation
 */
#include "athdefs.h"
#include "osapi_linux.h"
#include "a_types.h"
#include "a_debug.h"
#include "ol_if_athvar.h"
#include "ol_defines.h"
#include "htc_api.h"
#include "htc_api.h"
#include "dbglog_host.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_param.h"

#include <linux/debugfs.h>

/* This check for CONFIG_WIN temporary added due to redeclaration compilation
error in MCL. Error is caused due to inclusion of wmi.h in wmi_unified_api.h
which gets included here through ol_if_athvar.h. Eventually it is expected that
wmi.h will be removed from wmi_unified_api.h after cleanup, which will need
WMI_CMD_HDR to be defined here. */
#ifdef CONFIG_WIN
/* Copied from wmi.h */
#undef MS
#define MS(_v, _f) (((_v) & _f##_MASK) >> _f##_LSB)
#undef SM
#define SM(_v, _f) (((_v) << _f##_LSB) & _f##_MASK)
#undef WO
#define WO(_f)      ((_f##_OFFSET) >> 2)

#undef GET_FIELD
#define GET_FIELD(_addr, _f) MS(*((A_UINT32 *)(_addr) + WO(_f)), _f)
#undef SET_FIELD
#define SET_FIELD(_addr, _f, _val)  \
	    (*((A_UINT32 *)(_addr) + WO(_f)) = \
		(*((A_UINT32 *)(_addr) + WO(_f)) & ~_f##_MASK) | SM(_val, _f))

#define WMI_GET_FIELD(_msg_buf, _msg_type, _f) \
	    GET_FIELD(_msg_buf, _msg_type ## _ ## _f)

#define WMI_SET_FIELD(_msg_buf, _msg_type, _f, _val) \
	    SET_FIELD(_msg_buf, _msg_type ## _ ## _f, _val)

#define WMI_EP_APASS           0x0
#define WMI_EP_LPASS           0x1
#define WMI_EP_SENSOR          0x2

/*
 *  * Control Path
 *   */
typedef PREPACK struct {
	A_UINT32	commandId:24,
			reserved:2, /* used for WMI endpoint ID */
			plt_priv:6; /* platform private */
} POSTPACK WMI_CMD_HDR;        /* used for commands and events */

#define WMI_CMD_HDR_COMMANDID_LSB           0
#define WMI_CMD_HDR_COMMANDID_MASK          0x00ffffff
#define WMI_CMD_HDR_COMMANDID_OFFSET        0x00000000
#define WMI_CMD_HDR_WMI_ENDPOINTID_MASK        0x03000000
#define WMI_CMD_HDR_WMI_ENDPOINTID_OFFSET      24
#define WMI_CMD_HDR_PLT_PRIV_LSB               24
#define WMI_CMD_HDR_PLT_PRIV_MASK              0xff000000
#define WMI_CMD_HDR_PLT_PRIV_OFFSET            0x00000000
/* end of copy wmi.h */
#endif /* CONFIG_WIN */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0))
/* TODO Cleanup this backported function */
static int qcacld_bp_seq_printf(struct seq_file *m, const char *f, ...)
{
	va_list args;

	va_start(args, f);
	seq_printf(m, f, args);
	va_end(args);

	return m->count;
}

#define seq_printf(m, fmt, ...) qcacld_bp_seq_printf((m), fmt, ##__VA_ARGS__)
#endif

#define WMI_MIN_HEAD_ROOM 64

#ifdef WMI_INTERFACE_EVENT_LOGGING
#ifndef MAX_WMI_INSTANCES
#ifdef CONFIG_MCL
#define MAX_WMI_INSTANCES 1
#else
#define MAX_WMI_INSTANCES 3
#endif
#endif

/* WMI commands */
uint32_t g_wmi_command_buf_idx = 0;
struct wmi_command_debug wmi_command_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

/* WMI commands TX completed */
uint32_t g_wmi_command_tx_cmp_buf_idx = 0;
struct wmi_command_debug
	wmi_command_tx_cmp_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

/* WMI events when processed */
uint32_t g_wmi_event_buf_idx = 0;
struct wmi_event_debug wmi_event_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

/* WMI events when queued */
uint32_t g_wmi_rx_event_buf_idx = 0;
struct wmi_event_debug wmi_rx_event_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

#define WMI_COMMAND_RECORD(h, a, b) {					\
	if (wmi_log_max_entry <=					\
		*(h->log_info.wmi_command_log_buf_info.p_buf_tail_idx))	\
		*(h->log_info.wmi_command_log_buf_info.p_buf_tail_idx) = 0;\
	((struct wmi_command_debug *)h->log_info.wmi_command_log_buf_info.buf)\
		[*(h->log_info.wmi_command_log_buf_info.p_buf_tail_idx)]\
						.command = a;		\
	qdf_mem_copy(((struct wmi_command_debug *)h->log_info.		\
				wmi_command_log_buf_info.buf)		\
		[*(h->log_info.wmi_command_log_buf_info.p_buf_tail_idx)].data,\
			b, wmi_record_max_length);			\
	((struct wmi_command_debug *)h->log_info.wmi_command_log_buf_info.buf)\
		[*(h->log_info.wmi_command_log_buf_info.p_buf_tail_idx)].\
		time = qdf_get_log_timestamp();			\
	(*(h->log_info.wmi_command_log_buf_info.p_buf_tail_idx))++;	\
	h->log_info.wmi_command_log_buf_info.length++;			\
}

#define WMI_COMMAND_TX_CMP_RECORD(h, a, b) {				\
	if (wmi_log_max_entry <=					\
		*(h->log_info.wmi_command_tx_cmp_log_buf_info.p_buf_tail_idx))\
		*(h->log_info.wmi_command_tx_cmp_log_buf_info.p_buf_tail_idx) = 0;\
	((struct wmi_command_debug *)h->log_info.			\
		wmi_command_tx_cmp_log_buf_info.buf)			\
		[*(h->log_info.wmi_command_tx_cmp_log_buf_info.		\
				p_buf_tail_idx)].			\
							command	= a;	\
	qdf_mem_copy(((struct wmi_command_debug *)h->log_info.		\
				wmi_command_tx_cmp_log_buf_info.buf)	\
		[*(h->log_info.wmi_command_tx_cmp_log_buf_info.		\
			p_buf_tail_idx)].				\
		data, b, wmi_record_max_length);			\
	((struct wmi_command_debug *)h->log_info.			\
		wmi_command_tx_cmp_log_buf_info.buf)			\
		[*(h->log_info.wmi_command_tx_cmp_log_buf_info.		\
				p_buf_tail_idx)].			\
		time = qdf_get_log_timestamp();				\
	(*(h->log_info.wmi_command_tx_cmp_log_buf_info.p_buf_tail_idx))++;\
	h->log_info.wmi_command_tx_cmp_log_buf_info.length++;		\
}

#define WMI_EVENT_RECORD(h, a, b) {					\
	if (wmi_log_max_entry <=					\
		*(h->log_info.wmi_event_log_buf_info.p_buf_tail_idx))	\
		*(h->log_info.wmi_event_log_buf_info.p_buf_tail_idx) = 0;\
	((struct wmi_event_debug *)h->log_info.wmi_event_log_buf_info.buf)\
		[*(h->log_info.wmi_event_log_buf_info.p_buf_tail_idx)].	\
		event = a;						\
	qdf_mem_copy(((struct wmi_event_debug *)h->log_info.		\
				wmi_event_log_buf_info.buf)		\
		[*(h->log_info.wmi_event_log_buf_info.p_buf_tail_idx)].data, b,\
		wmi_record_max_length);					\
	((struct wmi_event_debug *)h->log_info.wmi_event_log_buf_info.buf)\
		[*(h->log_info.wmi_event_log_buf_info.p_buf_tail_idx)].time =\
		qdf_get_log_timestamp();				\
	(*(h->log_info.wmi_event_log_buf_info.p_buf_tail_idx))++;	\
	h->log_info.wmi_event_log_buf_info.length++;			\
}

#define WMI_RX_EVENT_RECORD(h, a, b) {					\
	if (wmi_log_max_entry <=					\
		*(h->log_info.wmi_rx_event_log_buf_info.p_buf_tail_idx))\
		*(h->log_info.wmi_rx_event_log_buf_info.p_buf_tail_idx) = 0;\
	((struct wmi_event_debug *)h->log_info.wmi_rx_event_log_buf_info.buf)\
		[*(h->log_info.wmi_rx_event_log_buf_info.p_buf_tail_idx)].\
		event = a;						\
	qdf_mem_copy(((struct wmi_event_debug *)h->log_info.		\
				wmi_rx_event_log_buf_info.buf)		\
		[*(h->log_info.wmi_rx_event_log_buf_info.p_buf_tail_idx)].\
			data, b, wmi_record_max_length);		\
	((struct wmi_event_debug *)h->log_info.wmi_rx_event_log_buf_info.buf)\
		[*(h->log_info.wmi_rx_event_log_buf_info.p_buf_tail_idx)].\
		time =	qdf_get_log_timestamp();			\
	(*(h->log_info.wmi_rx_event_log_buf_info.p_buf_tail_idx))++;	\
	h->log_info.wmi_rx_event_log_buf_info.length++;			\
}

uint32_t g_wmi_mgmt_command_buf_idx = 0;
struct
wmi_command_debug wmi_mgmt_command_log_buffer[WMI_MGMT_EVENT_DEBUG_MAX_ENTRY];

/* wmi_mgmt commands TX completed */
uint32_t g_wmi_mgmt_command_tx_cmp_buf_idx = 0;
struct wmi_command_debug
wmi_mgmt_command_tx_cmp_log_buffer[WMI_MGMT_EVENT_DEBUG_MAX_ENTRY];

/* wmi_mgmt events when processed */
uint32_t g_wmi_mgmt_event_buf_idx = 0;
struct wmi_event_debug
wmi_mgmt_event_log_buffer[WMI_MGMT_EVENT_DEBUG_MAX_ENTRY];

#define WMI_MGMT_COMMAND_RECORD(a, b, c, d, e) {			     \
	if (WMI_MGMT_EVENT_DEBUG_MAX_ENTRY <=				     \
		g_wmi_mgmt_command_buf_idx)				     \
		g_wmi_mgmt_command_buf_idx = 0;				     \
	wmi_mgmt_command_log_buffer[g_wmi_mgmt_command_buf_idx].command = a; \
	wmi_mgmt_command_log_buffer[g_wmi_mgmt_command_buf_idx].data[0] = b; \
	wmi_mgmt_command_log_buffer[g_wmi_mgmt_command_buf_idx].data[1] = c; \
	wmi_mgmt_command_log_buffer[g_wmi_mgmt_command_buf_idx].data[2] = d; \
	wmi_mgmt_command_log_buffer[g_wmi_mgmt_command_buf_idx].data[3] = e; \
	wmi_mgmt_command_log_buffer[g_wmi_mgmt_command_buf_idx].time =	     \
		qdf_get_log_timestamp();				     \
	g_wmi_mgmt_command_buf_idx++;					     \
}

#define WMI_MGMT_COMMAND_TX_CMP_RECORD(h, a, b) {			\
	if (wmi_mgmt_log_max_entry <=					\
		*(h->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.	\
			p_buf_tail_idx))				\
		*(h->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.	\
			p_buf_tail_idx) = 0;				\
	((struct wmi_command_debug *)h->log_info.			\
			wmi_mgmt_command_tx_cmp_log_buf_info.buf)	\
		[*(h->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.	\
				p_buf_tail_idx)].command = a;		\
	qdf_mem_copy(((struct wmi_command_debug *)h->log_info.		\
				wmi_mgmt_command_tx_cmp_log_buf_info.buf)\
		[*(h->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.	\
			p_buf_tail_idx)].data, b,			\
			wmi_record_max_length);				\
	((struct wmi_command_debug *)h->log_info.			\
			wmi_mgmt_command_tx_cmp_log_buf_info.buf)	\
		[*(h->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.	\
				p_buf_tail_idx)].time =			\
		qdf_get_log_timestamp();				\
	(*(h->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.		\
			p_buf_tail_idx))++;				\
	h->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.length++;	\
}

#define WMI_MGMT_EVENT_RECORD(h, a, b) {				\
	if (wmi_mgmt_log_max_entry <=					\
		*(h->log_info.wmi_mgmt_event_log_buf_info.p_buf_tail_idx))\
		*(h->log_info.wmi_mgmt_event_log_buf_info.p_buf_tail_idx) = 0;\
	((struct wmi_event_debug *)h->log_info.wmi_mgmt_event_log_buf_info.buf)\
		[*(h->log_info.wmi_mgmt_event_log_buf_info.p_buf_tail_idx)]\
					.event = a;			\
	qdf_mem_copy(((struct wmi_event_debug *)h->log_info.		\
				wmi_mgmt_event_log_buf_info.buf)	\
		[*(h->log_info.wmi_mgmt_event_log_buf_info.p_buf_tail_idx)].\
			data, b, wmi_record_max_length);		\
	((struct wmi_event_debug *)h->log_info.wmi_mgmt_event_log_buf_info.buf)\
		[*(h->log_info.wmi_mgmt_event_log_buf_info.p_buf_tail_idx)].\
			time = qdf_get_log_timestamp();			\
	(*(h->log_info.wmi_mgmt_event_log_buf_info.p_buf_tail_idx))++;	\
	h->log_info.wmi_mgmt_event_log_buf_info.length++;		\
}

/* These are defined to made it as module param, which can be configured */
uint32_t wmi_log_max_entry = WMI_EVENT_DEBUG_MAX_ENTRY;
uint32_t wmi_mgmt_log_max_entry = WMI_MGMT_EVENT_DEBUG_MAX_ENTRY;
uint32_t wmi_record_max_length = WMI_EVENT_DEBUG_ENTRY_MAX_LENGTH;
uint32_t wmi_display_size = 100;

/**
 * wmi_log_init() - Initialize WMI event logging
 * @wmi_handle: WMI handle.
 *
 * Return: Initialization status
 */
#ifdef CONFIG_MCL
static QDF_STATUS wmi_log_init(struct wmi_unified *wmi_handle)
{
	struct wmi_log_buf_t *cmd_log_buf =
			&wmi_handle->log_info.wmi_command_log_buf_info;
	struct wmi_log_buf_t *cmd_tx_cmpl_log_buf =
			&wmi_handle->log_info.wmi_command_tx_cmp_log_buf_info;

	struct wmi_log_buf_t *event_log_buf =
			&wmi_handle->log_info.wmi_event_log_buf_info;
	struct wmi_log_buf_t *rx_event_log_buf =
			&wmi_handle->log_info.wmi_rx_event_log_buf_info;

	struct wmi_log_buf_t *mgmt_cmd_log_buf =
			&wmi_handle->log_info.wmi_mgmt_command_log_buf_info;
	struct wmi_log_buf_t *mgmt_cmd_tx_cmp_log_buf =
		&wmi_handle->log_info.wmi_mgmt_command_tx_cmp_log_buf_info;
	struct wmi_log_buf_t *mgmt_event_log_buf =
			&wmi_handle->log_info.wmi_mgmt_event_log_buf_info;

	/* WMI commands */
	cmd_log_buf->length = 0;
	cmd_log_buf->buf_tail_idx = 0;
	cmd_log_buf->buf = wmi_command_log_buffer;
	cmd_log_buf->p_buf_tail_idx = &g_wmi_command_buf_idx;

	/* WMI commands TX completed */
	cmd_tx_cmpl_log_buf->length = 0;
	cmd_tx_cmpl_log_buf->buf_tail_idx = 0;
	cmd_tx_cmpl_log_buf->buf = wmi_command_tx_cmp_log_buffer;
	cmd_tx_cmpl_log_buf->p_buf_tail_idx = &g_wmi_command_tx_cmp_buf_idx;

	/* WMI events when processed */
	event_log_buf->length = 0;
	event_log_buf->buf_tail_idx = 0;
	event_log_buf->buf = wmi_event_log_buffer;
	event_log_buf->p_buf_tail_idx = &g_wmi_event_buf_idx;

	/* WMI events when queued */
	rx_event_log_buf->length = 0;
	rx_event_log_buf->buf_tail_idx = 0;
	rx_event_log_buf->buf = wmi_rx_event_log_buffer;
	rx_event_log_buf->p_buf_tail_idx = &g_wmi_rx_event_buf_idx;

	/* WMI Management commands */
	mgmt_cmd_log_buf->length = 0;
	mgmt_cmd_log_buf->buf_tail_idx = 0;
	mgmt_cmd_log_buf->buf = wmi_mgmt_command_log_buffer;
	mgmt_cmd_log_buf->p_buf_tail_idx = &g_wmi_mgmt_command_buf_idx;

	/* WMI Management commands Tx completed*/
	mgmt_cmd_tx_cmp_log_buf->length = 0;
	mgmt_cmd_tx_cmp_log_buf->buf_tail_idx = 0;
	mgmt_cmd_tx_cmp_log_buf->buf = wmi_mgmt_command_tx_cmp_log_buffer;
	mgmt_cmd_tx_cmp_log_buf->p_buf_tail_idx =
		&g_wmi_mgmt_command_tx_cmp_buf_idx;

	/* WMI Management events when processed*/
	mgmt_event_log_buf->length = 0;
	mgmt_event_log_buf->buf_tail_idx = 0;
	mgmt_event_log_buf->buf = wmi_mgmt_event_log_buffer;
	mgmt_event_log_buf->p_buf_tail_idx = &g_wmi_mgmt_event_buf_idx;

	qdf_spinlock_create(&wmi_handle->log_info.wmi_record_lock);
	wmi_handle->log_info.wmi_logging_enable = 1;

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS wmi_log_init(struct wmi_unified *wmi_handle)
{
	struct wmi_log_buf_t *cmd_log_buf =
			&wmi_handle->log_info.wmi_command_log_buf_info;
	struct wmi_log_buf_t *cmd_tx_cmpl_log_buf =
			&wmi_handle->log_info.wmi_command_tx_cmp_log_buf_info;

	struct wmi_log_buf_t *event_log_buf =
			&wmi_handle->log_info.wmi_event_log_buf_info;
	struct wmi_log_buf_t *rx_event_log_buf =
			&wmi_handle->log_info.wmi_rx_event_log_buf_info;

	struct wmi_log_buf_t *mgmt_cmd_log_buf =
			&wmi_handle->log_info.wmi_mgmt_command_log_buf_info;
	struct wmi_log_buf_t *mgmt_cmd_tx_cmp_log_buf =
		&wmi_handle->log_info.wmi_mgmt_command_tx_cmp_log_buf_info;
	struct wmi_log_buf_t *mgmt_event_log_buf =
			&wmi_handle->log_info.wmi_mgmt_event_log_buf_info;

	wmi_handle->log_info.wmi_logging_enable = 0;

	/* WMI commands */
	cmd_log_buf->length = 0;
	cmd_log_buf->buf_tail_idx = 0;
	cmd_log_buf->buf = (struct wmi_command_debug *) qdf_mem_malloc(
		wmi_log_max_entry * sizeof(struct wmi_command_debug));

	if (!cmd_log_buf->buf) {
		qdf_print("no memory for WMI command log buffer..\n");
		return QDF_STATUS_E_NOMEM;
	}
	cmd_log_buf->p_buf_tail_idx = &cmd_log_buf->buf_tail_idx;

	/* WMI commands TX completed */
	cmd_tx_cmpl_log_buf->length = 0;
	cmd_tx_cmpl_log_buf->buf_tail_idx = 0;
	cmd_tx_cmpl_log_buf->buf = (struct wmi_command_debug *) qdf_mem_malloc(
		wmi_log_max_entry * sizeof(struct wmi_command_debug));

	if (!cmd_tx_cmpl_log_buf->buf) {
		qdf_print("no memory for WMI Command Tx Complete log buffer..\n");
		return QDF_STATUS_E_NOMEM;
	}
	cmd_tx_cmpl_log_buf->p_buf_tail_idx =
		&cmd_tx_cmpl_log_buf->buf_tail_idx;

	/* WMI events when processed */
	event_log_buf->length = 0;
	event_log_buf->buf_tail_idx = 0;
	event_log_buf->buf = (struct wmi_event_debug *) qdf_mem_malloc(
		wmi_log_max_entry * sizeof(struct wmi_event_debug));

	if (!event_log_buf->buf) {
		qdf_print("no memory for WMI Event log buffer..\n");
		return QDF_STATUS_E_NOMEM;
	}
	event_log_buf->p_buf_tail_idx = &event_log_buf->buf_tail_idx;

	/* WMI events when queued */
	rx_event_log_buf->length = 0;
	rx_event_log_buf->buf_tail_idx = 0;
	rx_event_log_buf->buf = (struct wmi_event_debug *) qdf_mem_malloc(
		wmi_log_max_entry * sizeof(struct wmi_event_debug));

	if (!rx_event_log_buf->buf) {
		qdf_print("no memory for WMI Event Rx log buffer..\n");
		return QDF_STATUS_E_NOMEM;
	}
	rx_event_log_buf->p_buf_tail_idx = &rx_event_log_buf->buf_tail_idx;

	/* WMI Management commands */
	mgmt_cmd_log_buf->length = 0;
	mgmt_cmd_log_buf->buf_tail_idx = 0;
	mgmt_cmd_log_buf->buf = (struct wmi_command_debug *) qdf_mem_malloc(
		wmi_mgmt_log_max_entry *
		sizeof(struct wmi_command_debug));

	if (!mgmt_cmd_log_buf->buf) {
		qdf_print("no memory for WMI Management Command log buffer..\n");
		return QDF_STATUS_E_NOMEM;
	}
	mgmt_cmd_log_buf->p_buf_tail_idx = &mgmt_cmd_log_buf->buf_tail_idx;

	/* WMI Management commands Tx completed*/
	mgmt_cmd_tx_cmp_log_buf->length = 0;
	mgmt_cmd_tx_cmp_log_buf->buf_tail_idx = 0;
	mgmt_cmd_tx_cmp_log_buf->buf = (struct wmi_command_debug *)
		qdf_mem_malloc(
		wmi_mgmt_log_max_entry *
		sizeof(struct wmi_command_debug));

	if (!mgmt_cmd_tx_cmp_log_buf->buf) {
		qdf_print("no memory for WMI Management Command Tx complete log buffer..\n");
		return QDF_STATUS_E_NOMEM;
	}
	mgmt_cmd_tx_cmp_log_buf->p_buf_tail_idx =
		&mgmt_cmd_tx_cmp_log_buf->buf_tail_idx;

	/* WMI Management events when processed*/
	mgmt_event_log_buf->length = 0;
	mgmt_event_log_buf->buf_tail_idx = 0;

	mgmt_event_log_buf->buf = (struct wmi_event_debug *) qdf_mem_malloc(
		wmi_mgmt_log_max_entry *
		sizeof(struct wmi_event_debug));

	if (!mgmt_event_log_buf->buf) {
		qdf_print("no memory for WMI Management Event log buffer..\n");
		return QDF_STATUS_E_NOMEM;
	}
	mgmt_event_log_buf->p_buf_tail_idx = &mgmt_event_log_buf->buf_tail_idx;

	qdf_spinlock_create(&wmi_handle->log_info.wmi_record_lock);
	wmi_handle->log_info.wmi_logging_enable = 1;

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wmi_log_buffer_free() - Free all dynamic allocated buffer memory for
 * event logging
 * @wmi_handle: WMI handle.
 *
 * Return: None
 */
#ifndef CONFIG_MCL
static inline void wmi_log_buffer_free(struct wmi_unified *wmi_handle)
{
	if (wmi_handle->log_info.wmi_command_log_buf_info.buf)
		qdf_mem_free(wmi_handle->log_info.wmi_command_log_buf_info.buf);
	if (wmi_handle->log_info.wmi_command_tx_cmp_log_buf_info.buf)
		qdf_mem_free(
		wmi_handle->log_info.wmi_command_tx_cmp_log_buf_info.buf);
	if (wmi_handle->log_info.wmi_event_log_buf_info.buf)
		qdf_mem_free(wmi_handle->log_info.wmi_event_log_buf_info.buf);
	if (wmi_handle->log_info.wmi_rx_event_log_buf_info.buf)
		qdf_mem_free(
			wmi_handle->log_info.wmi_rx_event_log_buf_info.buf);
	if (wmi_handle->log_info.wmi_mgmt_command_log_buf_info.buf)
		qdf_mem_free(
			wmi_handle->log_info.wmi_mgmt_command_log_buf_info.buf);
	if (wmi_handle->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.buf)
		qdf_mem_free(
		wmi_handle->log_info.wmi_mgmt_command_tx_cmp_log_buf_info.buf);
	if (wmi_handle->log_info.wmi_mgmt_event_log_buf_info.buf)
		qdf_mem_free(
			wmi_handle->log_info.wmi_mgmt_event_log_buf_info.buf);
	wmi_handle->log_info.wmi_logging_enable = 0;
	qdf_spinlock_destroy(&wmi_handle->log_info.wmi_record_lock);
}
#else
static inline void wmi_log_buffer_free(struct wmi_unified *wmi_handle)
{
	wmi_handle->log_info.wmi_logging_enable = 0;
	qdf_spinlock_destroy(&wmi_handle->log_info.wmi_record_lock);
}
#endif

#ifdef CONFIG_MCL
const int8_t * const debugfs_dir[] = {"WMI0", "WMI1", "WMI2"};
#else
const int8_t * const debugfs_dir[] = {"WMI0"};
#endif

/* debugfs routines*/

/**
 * debug_wmi_##func_base##_show() - debugfs functions to display content of
 * command and event buffers. Macro uses max buffer length to display
 * buffer when it is wraparound.
 *
 * @m: debugfs handler to access wmi_handle
 * @v: Variable arguments (not used)
 *
 * Return: Length of characters printed
 */
#define GENERATE_COMMAND_DEBUG_SHOW_FUNCS(func_base, wmi_ring_size)	\
	static int debug_wmi_##func_base##_show(struct seq_file *m,	\
						void *v)		\
	{								\
		wmi_unified_t wmi_handle = (wmi_unified_t) m->private;	\
		struct wmi_log_buf_t *wmi_log =				\
			&wmi_handle->log_info.wmi_##func_base##_buf_info;\
		int pos, nread, outlen;					\
		int i;							\
									\
		if (!wmi_log->length)					\
			return seq_printf(m,				\
			"no elements to read from ring buffer!\n");	\
									\
		if (wmi_log->length <= wmi_ring_size)			\
			nread = wmi_log->length;			\
		else							\
			nread = wmi_ring_size;				\
									\
		if (*(wmi_log->p_buf_tail_idx) == 0)			\
			/* tail can be 0 after wrap-around */		\
			pos = wmi_ring_size - 1;			\
		else							\
			pos = *(wmi_log->p_buf_tail_idx) - 1;		\
									\
		outlen = 0;						\
		qdf_spin_lock(&wmi_handle->log_info.wmi_record_lock);	\
		while (nread--) {					\
			struct wmi_command_debug *wmi_record;		\
									\
			wmi_record = (struct wmi_command_debug *)	\
			&(((struct wmi_command_debug *)wmi_log->buf)[pos]);\
			outlen += seq_printf(m, "CMD ID = %x\n",	\
				(wmi_record->command));			\
			outlen += seq_printf(m, "CMD = ");		\
			for (i = 0; i < (wmi_record_max_length/		\
					sizeof(uint32_t)); i++)		\
				outlen += seq_printf(m, "%x ",		\
					wmi_record->data[i]);		\
			outlen += seq_printf(m, "\n");			\
									\
			if (pos == 0)					\
				pos = wmi_ring_size - 1;		\
			else						\
				pos--;					\
		}							\
		outlen += seq_printf(m, "Length = %d\n", wmi_log->length);\
		qdf_spin_unlock(&wmi_handle->log_info.wmi_record_lock);	\
									\
		return outlen;						\
	}								\

#define GENERATE_EVENT_DEBUG_SHOW_FUNCS(func_base, wmi_ring_size)	\
	static int debug_wmi_##func_base##_show(struct seq_file *m,	\
						void *v)		\
	{								\
		wmi_unified_t wmi_handle = (wmi_unified_t) m->private;	\
		struct wmi_log_buf_t *wmi_log =				\
			&wmi_handle->log_info.wmi_##func_base##_buf_info;\
		int pos, nread, outlen;					\
		int i;							\
									\
		if (!wmi_log->length)					\
			return seq_printf(m,				\
			"no elements to read from ring buffer!\n");	\
									\
		if (wmi_log->length <= wmi_ring_size)			\
			nread = wmi_log->length;			\
		else							\
			nread = wmi_ring_size;				\
									\
		if (*(wmi_log->p_buf_tail_idx) == 0)			\
			/* tail can be 0 after wrap-around */		\
			pos = wmi_ring_size - 1;			\
		else							\
			pos = *(wmi_log->p_buf_tail_idx) - 1;		\
									\
		outlen = 0;						\
		qdf_spin_lock(&wmi_handle->log_info.wmi_record_lock);	\
		while (nread--) {					\
			struct wmi_event_debug *wmi_record;		\
									\
			wmi_record = (struct wmi_event_debug *)		\
			&(((struct wmi_event_debug *)wmi_log->buf)[pos]);\
			outlen += seq_printf(m, "Event ID = %x\n",	\
				(wmi_record->event));			\
			outlen += seq_printf(m, "CMD = ");		\
			for (i = 0; i < (wmi_record_max_length/		\
					sizeof(uint32_t)); i++)		\
				outlen += seq_printf(m, "%x ",		\
					wmi_record->data[i]);		\
			outlen += seq_printf(m, "\n");			\
									\
			if (pos == 0)					\
				pos = wmi_ring_size - 1;		\
			else						\
				pos--;					\
		}							\
		outlen += seq_printf(m, "Length = %d\n", wmi_log->length);\
		qdf_spin_unlock(&wmi_handle->log_info.wmi_record_lock);	\
									\
		return outlen;						\
	}

GENERATE_COMMAND_DEBUG_SHOW_FUNCS(command_log, wmi_display_size);
GENERATE_COMMAND_DEBUG_SHOW_FUNCS(command_tx_cmp_log, wmi_display_size);
GENERATE_EVENT_DEBUG_SHOW_FUNCS(event_log, wmi_display_size);
GENERATE_EVENT_DEBUG_SHOW_FUNCS(rx_event_log, wmi_display_size);
GENERATE_COMMAND_DEBUG_SHOW_FUNCS(mgmt_command_log, wmi_display_size);
GENERATE_COMMAND_DEBUG_SHOW_FUNCS(mgmt_command_tx_cmp_log,
					wmi_display_size);
GENERATE_EVENT_DEBUG_SHOW_FUNCS(mgmt_event_log, wmi_display_size);

/**
 * debug_wmi_enable_show() - debugfs functions to display enable state of
 * wmi logging feature.
 *
 * @m: debugfs handler to access wmi_handle
 * @v: Variable arguments (not used)
 *
 * Return: always 1
 */
static int debug_wmi_enable_show(struct seq_file *m, void *v)
{
	wmi_unified_t wmi_handle = (wmi_unified_t) m->private;

	return seq_printf(m, "%d\n", wmi_handle->log_info.wmi_logging_enable);

}

/**
 * debug_wmi_log_size_show() - debugfs functions to display configured size of
 * wmi logging command/event buffer and management command/event buffer.
 *
 * @m: debugfs handler to access wmi_handle
 * @v: Variable arguments (not used)
 *
 * Return: Length of characters printed
 */
static int debug_wmi_log_size_show(struct seq_file *m, void *v)
{

	seq_printf(m, "WMI command/event log max size:%d\n", wmi_log_max_entry);
	return seq_printf(m, "WMI management command/events log max size:%d\n",
				wmi_mgmt_log_max_entry);
}

/**
 * debug_wmi_##func_base##_write() - debugfs functions to clear
 * wmi logging command/event buffer and management command/event buffer.
 *
 * @file: file handler to access wmi_handle
 * @buf: received data buffer
 * @count: length of received buffer
 * @ppos: Not used
 *
 * Return: count
 */
#define GENERATE_DEBUG_WRITE_FUNCS(func_base, wmi_ring_size, wmi_record_type)\
	static ssize_t debug_wmi_##func_base##_write(struct file *file,	\
				const char __user *buf,			\
				size_t count, loff_t *ppos)		\
	{								\
		int k, ret;						\
		wmi_unified_t wmi_handle = file->private_data;		\
		struct wmi_log_buf_t *wmi_log = &wmi_handle->log_info.	\
				wmi_##func_base##_buf_info;		\
									\
		ret = sscanf(buf, "%d", &k);				\
		if ((ret != 1) || (k != 0)) {				\
			qdf_print("Wrong input, echo 0 to clear the wmi	buffer\n");\
			return -EINVAL;					\
		}							\
									\
		qdf_spin_lock(&wmi_handle->log_info.wmi_record_lock);	\
		qdf_mem_zero(wmi_log->buf, wmi_ring_size *		\
				sizeof(struct wmi_record_type));	\
		wmi_log->length = 0;					\
		*(wmi_log->p_buf_tail_idx) = 0;				\
		qdf_spin_unlock(&wmi_handle->log_info.wmi_record_lock);	\
									\
		return count;						\
	}

GENERATE_DEBUG_WRITE_FUNCS(command_log, wmi_log_max_entry,
					wmi_command_debug);
GENERATE_DEBUG_WRITE_FUNCS(command_tx_cmp_log, wmi_log_max_entry,
					wmi_command_debug);
GENERATE_DEBUG_WRITE_FUNCS(event_log, wmi_log_max_entry,
					wmi_event_debug);
GENERATE_DEBUG_WRITE_FUNCS(rx_event_log, wmi_log_max_entry,
					wmi_event_debug);
GENERATE_DEBUG_WRITE_FUNCS(mgmt_command_log, wmi_mgmt_log_max_entry,
					wmi_command_debug);
GENERATE_DEBUG_WRITE_FUNCS(mgmt_command_tx_cmp_log,
		wmi_mgmt_log_max_entry, wmi_command_debug);
GENERATE_DEBUG_WRITE_FUNCS(mgmt_event_log, wmi_mgmt_log_max_entry,
					wmi_event_debug);

/**
 * debug_wmi_enable_write() - debugfs functions to enable/disable
 * wmi logging feature.
 *
 * @file: file handler to access wmi_handle
 * @buf: received data buffer
 * @count: length of received buffer
 * @ppos: Not used
 *
 * Return: count
 */
static ssize_t debug_wmi_enable_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	wmi_unified_t wmi_handle =  file->private_data;
	int k, ret;

	ret = sscanf(buf, "%d", &k);
	if ((ret != 1) || ((k != 0) && (k != 1)))
		return -EINVAL;

	wmi_handle->log_info.wmi_logging_enable = k;
	return count;
}

/**
 * debug_wmi_log_size_write() - reserved.
 *
 * @file: file handler to access wmi_handle
 * @buf: received data buffer
 * @count: length of received buffer
 * @ppos: Not used
 *
 * Return: count
 */
static ssize_t debug_wmi_log_size_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

/* Structure to maintain debug information */
struct wmi_debugfs_info {
	const char *name;
	struct dentry *de[MAX_WMI_INSTANCES];
	const struct file_operations *ops;
};

#define DEBUG_FOO(func_base) { .name = #func_base,			\
	.ops = &debug_##func_base##_ops }

/**
 * debug_##func_base##_open() - Open debugfs entry for respective command
 * and event buffer.
 *
 * @inode: node for debug dir entry
 * @file: file handler
 *
 * Return: open status
 */
#define GENERATE_DEBUG_STRUCTS(func_base)				\
	static int debug_##func_base##_open(struct inode *inode,	\
						struct file *file)	\
	{								\
		return single_open(file, debug_##func_base##_show,	\
				inode->i_private);			\
	}								\
									\
									\
	static struct file_operations debug_##func_base##_ops = {	\
		.open		= debug_##func_base##_open,		\
		.read		= seq_read,				\
		.llseek		= seq_lseek,				\
		.write		= debug_##func_base##_write,		\
		.release	= single_release,			\
	};

GENERATE_DEBUG_STRUCTS(wmi_command_log);
GENERATE_DEBUG_STRUCTS(wmi_command_tx_cmp_log);
GENERATE_DEBUG_STRUCTS(wmi_event_log);
GENERATE_DEBUG_STRUCTS(wmi_rx_event_log);
GENERATE_DEBUG_STRUCTS(wmi_mgmt_command_log);
GENERATE_DEBUG_STRUCTS(wmi_mgmt_command_tx_cmp_log);
GENERATE_DEBUG_STRUCTS(wmi_mgmt_event_log);
GENERATE_DEBUG_STRUCTS(wmi_enable);
GENERATE_DEBUG_STRUCTS(wmi_log_size);

struct wmi_debugfs_info wmi_debugfs_infos[] = {
	DEBUG_FOO(wmi_command_log),
	DEBUG_FOO(wmi_command_tx_cmp_log),
	DEBUG_FOO(wmi_event_log),
	DEBUG_FOO(wmi_rx_event_log),
	DEBUG_FOO(wmi_mgmt_command_log),
	DEBUG_FOO(wmi_mgmt_command_tx_cmp_log),
	DEBUG_FOO(wmi_mgmt_event_log),
	DEBUG_FOO(wmi_enable),
	DEBUG_FOO(wmi_log_size),
};

#define NUM_DEBUG_INFOS (sizeof(wmi_debugfs_infos) /			\
		sizeof(wmi_debugfs_infos[0]))

/**
 * wmi_debugfs_create() - Create debug_fs entry for wmi logging.
 *
 * @wmi_handle: wmi handle
 * @par_entry: debug directory entry
 * @id: Index to debug info data array
 *
 * Return: none
 */
static void wmi_debugfs_create(wmi_unified_t wmi_handle,
		struct dentry *par_entry, int id)
{
	int i;

	if (par_entry == NULL || (id < 0) || (id >= MAX_WMI_INSTANCES))
		goto out;

	for (i = 0; i < NUM_DEBUG_INFOS; ++i) {

		wmi_debugfs_infos[i].de[id] = debugfs_create_file(
				wmi_debugfs_infos[i].name, 0644, par_entry,
				wmi_handle, wmi_debugfs_infos[i].ops);

		if (wmi_debugfs_infos[i].de[id] == NULL) {
			qdf_print("%s: debug Entry creation failed!\n",
					__func__);
			goto out;
		}
	}

	return;

out:
	qdf_print("%s: debug Entry creation failed!\n", __func__);
	wmi_log_buffer_free(wmi_handle);
	return;
}

/**
 * wmi_debugfs_remove() - Remove debugfs entry for wmi logging.
 * @wmi_handle: wmi handle
 * @dentry: debugfs directory entry
 * @id: Index to debug info data array
 *
 * Return: none
 */
static void wmi_debugfs_remove(wmi_unified_t wmi_handle)
{
	int i;
	struct dentry *dentry = wmi_handle->log_info.wmi_log_debugfs_dir;
	int id = wmi_handle->log_info.wmi_instance_id;

	if (dentry && (!(id < 0) || (id >= MAX_WMI_INSTANCES))) {
		for (i = 0; i < NUM_DEBUG_INFOS; ++i) {
			if (wmi_debugfs_infos[i].de[id])
				wmi_debugfs_infos[i].de[id] = NULL;
		}
	}

	if (dentry)
		debugfs_remove_recursive(dentry);
}

/**
 * wmi_debugfs_init() - debugfs functions to create debugfs directory and to
 * create debugfs enteries.
 *
 * @h: wmi handler
 *
 * Return: init status
 */
static QDF_STATUS wmi_debugfs_init(wmi_unified_t wmi_handle)
{
	static int wmi_index;

	if (wmi_index < MAX_WMI_INSTANCES)
		wmi_handle->log_info.wmi_log_debugfs_dir =
			debugfs_create_dir(debugfs_dir[wmi_index], NULL);

	if (wmi_handle->log_info.wmi_log_debugfs_dir == NULL) {
		qdf_print("error while creating debugfs dir for %s\n",
				debugfs_dir[wmi_index]);
		return QDF_STATUS_E_FAILURE;
	}

	wmi_debugfs_create(wmi_handle, wmi_handle->log_info.wmi_log_debugfs_dir,
				wmi_index);
	wmi_handle->log_info.wmi_instance_id = wmi_index++;

	return QDF_STATUS_SUCCESS;
}

/**
 * wmi_mgmt_cmd_record() - Wrapper function for mgmt command logging macro
 *
 * @wmi_handle: wmi handle
 * @cmd: mgmt command
 * @header: pointer to 802.11 header
 * @vdev_id: vdev id
 * @chanfreq: channel frequency
 *
 * Return: none
 */
void wmi_mgmt_cmd_record(wmi_unified_t wmi_handle, uint32_t cmd,
			void *header, uint32_t vdev_id, uint32_t chanfreq)
{
	qdf_spin_lock_bh(&wmi_handle->log_info.wmi_record_lock);

	WMI_MGMT_COMMAND_RECORD(cmd,
				((struct wmi_command_header *)header)->type,
				((struct wmi_command_header *)header)->sub_type,
				vdev_id, chanfreq);

	qdf_spin_unlock_bh(&wmi_handle->log_info.wmi_record_lock);
}
#else
/**
 * wmi_debugfs_remove() - Remove debugfs entry for wmi logging.
 * @wmi_handle: wmi handle
 * @dentry: debugfs directory entry
 * @id: Index to debug info data array
 *
 * Return: none
 */
static void wmi_debugfs_remove(wmi_unified_t wmi_handle) { }
void wmi_mgmt_cmd_record(wmi_unified_t wmi_handle, uint32_t cmd,
			void *header, uint32_t vdev_id, uint32_t chanfreq) { }
#endif /*WMI_INTERFACE_EVENT_LOGGING */

int wmi_get_host_credits(wmi_unified_t wmi_handle);
/* WMI buffer APIs */

#ifdef MEMORY_DEBUG
wmi_buf_t
wmi_buf_alloc_debug(wmi_unified_t wmi_handle, uint16_t len, uint8_t *file_name,
			uint32_t line_num)
{
	wmi_buf_t wmi_buf;

	if (roundup(len + WMI_MIN_HEAD_ROOM, 4) > wmi_handle->max_msg_len) {
		QDF_ASSERT(0);
		return NULL;
	}

	wmi_buf = qdf_nbuf_alloc_debug(NULL,
					roundup(len + WMI_MIN_HEAD_ROOM, 4),
					WMI_MIN_HEAD_ROOM, 4, false, file_name,
					line_num);

	if (!wmi_buf)
		return NULL;

	/* Clear the wmi buffer */
	OS_MEMZERO(qdf_nbuf_data(wmi_buf), len);

	/*
	 * Set the length of the buffer to match the allocation size.
	 */
	qdf_nbuf_set_pktlen(wmi_buf, len);

	return wmi_buf;
}

void wmi_buf_free(wmi_buf_t net_buf)
{
	qdf_nbuf_free(net_buf);
}
#else
wmi_buf_t wmi_buf_alloc(wmi_unified_t wmi_handle, uint16_t len)
{
	wmi_buf_t wmi_buf;

	if (roundup(len + WMI_MIN_HEAD_ROOM, 4) > wmi_handle->max_msg_len) {
		QDF_ASSERT(0);
		return NULL;
	}

	wmi_buf = qdf_nbuf_alloc(NULL, roundup(len + WMI_MIN_HEAD_ROOM, 4),
				WMI_MIN_HEAD_ROOM, 4, false);
	if (!wmi_buf)
		return NULL;

	/* Clear the wmi buffer */
	OS_MEMZERO(qdf_nbuf_data(wmi_buf), len);

	/*
	 * Set the length of the buffer to match the allocation size.
	 */
	qdf_nbuf_set_pktlen(wmi_buf, len);
	return wmi_buf;
}

void wmi_buf_free(wmi_buf_t net_buf)
{
	qdf_nbuf_free(net_buf);
}
#endif

/**
 * wmi_get_max_msg_len() - get maximum WMI message length
 * @wmi_handle: WMI handle.
 *
 * This function returns the maximum WMI message length
 *
 * Return: maximum WMI message length
 */
uint16_t wmi_get_max_msg_len(wmi_unified_t wmi_handle)
{
	return wmi_handle->max_msg_len - WMI_MIN_HEAD_ROOM;
}

#ifndef WMI_CMD_STRINGS
static uint8_t *wmi_id_to_name(uint32_t wmi_command)
{
	return "Invalid WMI cmd";
}
#endif


#ifndef WMI_NON_TLV_SUPPORT
static inline void wma_log_cmd_id(uint32_t cmd_id, uint32_t tag)
{
	WMI_LOGD("Send WMI command:%s command_id:%d htc_tag:%d",
		 wmi_id_to_name(cmd_id), cmd_id, tag);
}
#endif

#ifndef WMI_NON_TLV_SUPPORT
/**
 * wmi_is_pm_resume_cmd() - check if a cmd is part of the resume sequence
 * @cmd_id: command to check
 *
 * Return: true if the command is part of the resume sequence.
 */
static bool wmi_is_pm_resume_cmd(uint32_t cmd_id)
{
	switch (cmd_id) {
	case WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID:
	case WMI_PDEV_RESUME_CMDID:
		return true;

	default:
		return false;
	}
}
#else
static bool wmi_is_pm_resume_cmd(uint32_t cmd_id)
{
	return false;
}
#endif

/**
 * wmi_unified_cmd_send() - WMI command API
 * @wmi_handle: handle to wmi
 * @buf: wmi buf
 * @len: wmi buffer length
 * @cmd_id: wmi command id
 *
 * Note, it is NOT safe to access buf after calling this function!
 *
 * Return: 0 on success
 */
QDF_STATUS wmi_unified_cmd_send(wmi_unified_t wmi_handle, wmi_buf_t buf,
				uint32_t len, uint32_t cmd_id)
{
	HTC_PACKET *pkt;
	A_STATUS status;
	uint16_t htc_tag = 0;

	if (wmi_get_runtime_pm_inprogress(wmi_handle)) {
		htc_tag =
			(A_UINT16)wmi_handle->ops->wmi_set_htc_tx_tag(
						wmi_handle, buf, cmd_id);
	} else if (qdf_atomic_read(&wmi_handle->is_target_suspended) &&
		(!wmi_is_pm_resume_cmd(cmd_id))) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
				  "%s: Target is suspended", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_BUSY;
	}
	if (wmi_handle->wmi_stopinprogress) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
			"WMI  stop in progress\n");
		return QDF_STATUS_E_INVAL;
	}

	/* Do sanity check on the TLV parameter structure */
#ifndef WMI_NON_TLV_SUPPORT
	if (wmi_handle->target_type == WMI_TLV_TARGET) {
		void *buf_ptr = (void *)qdf_nbuf_data(buf);

		if (wmitlv_check_command_tlv_params(NULL, buf_ptr, len, cmd_id)
			!= 0) {
			QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
			"\nERROR: %s: Invalid WMI Param Buffer for Cmd:%d",
				__func__, cmd_id);
			return QDF_STATUS_E_INVAL;
		}
	}
#endif

	if (qdf_nbuf_push_head(buf, sizeof(WMI_CMD_HDR)) == NULL) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
			 "%s, Failed to send cmd %x, no memory",
			 __func__, cmd_id);
		return QDF_STATUS_E_NOMEM;
	}

	WMI_SET_FIELD(qdf_nbuf_data(buf), WMI_CMD_HDR, COMMANDID, cmd_id);

	qdf_atomic_inc(&wmi_handle->pending_cmds);
	if (qdf_atomic_read(&wmi_handle->pending_cmds) >= WMI_MAX_CMDS) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
		    "\n%s: hostcredits = %d", __func__,
		wmi_get_host_credits(wmi_handle));
		htc_dump_counter_info(wmi_handle->htc_handle);
		qdf_atomic_dec(&wmi_handle->pending_cmds);
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
			"%s: MAX %d WMI Pending cmds reached.", __func__,
			WMI_MAX_CMDS);
		QDF_BUG(0);
		return QDF_STATUS_E_BUSY;
	}

	pkt = qdf_mem_malloc(sizeof(*pkt));
	if (!pkt) {
		qdf_atomic_dec(&wmi_handle->pending_cmds);
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
			 "%s, Failed to alloc htc packet %x, no memory",
			 __func__, cmd_id);
		return QDF_STATUS_E_NOMEM;
	}

	SET_HTC_PACKET_INFO_TX(pkt,
			       NULL,
			       qdf_nbuf_data(buf), len + sizeof(WMI_CMD_HDR),
			       wmi_handle->wmi_endpoint_id, htc_tag);

	SET_HTC_PACKET_NET_BUF_CONTEXT(pkt, buf);
#ifndef WMI_NON_TLV_SUPPORT
	wma_log_cmd_id(cmd_id, htc_tag);
#endif

#ifdef WMI_INTERFACE_EVENT_LOGGING
	if (wmi_handle->log_info.wmi_logging_enable) {
		qdf_spin_lock_bh(&wmi_handle->log_info.wmi_record_lock);
		if (!wmi_handle->log_info.is_management_record(cmd_id)) {
			WMI_COMMAND_RECORD(wmi_handle, cmd_id,
			((uint32_t *) qdf_nbuf_data(buf) +
			 wmi_handle->log_info.buf_offset_command));
		}
		qdf_spin_unlock_bh(&wmi_handle->log_info.wmi_record_lock);
	}
#endif

	status = htc_send_pkt(wmi_handle->htc_handle, pkt);

	if (A_OK != status) {
		qdf_atomic_dec(&wmi_handle->pending_cmds);
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
		   "%s %d, htc_send_pkt failed", __func__, __LINE__);
		qdf_mem_free(pkt);

	}
	if (status)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

/**
 * wmi_unified_get_event_handler_ix() - gives event handler's index
 * @wmi_handle: handle to wmi
 * @event_id: wmi  event id
 *
 * Return: event handler's index
 */
static int wmi_unified_get_event_handler_ix(wmi_unified_t wmi_handle,
					    uint32_t event_id)
{
	uint32_t idx = 0;
	int32_t invalid_idx = -1;

	for (idx = 0; (idx < wmi_handle->max_event_idx &&
		       idx < WMI_UNIFIED_MAX_EVENT); ++idx) {
		if (wmi_handle->event_id[idx] == event_id &&
		    wmi_handle->event_handler[idx] != NULL) {
			return idx;
		}
	}

	return invalid_idx;
}

/**
 * wmi_unified_register_event_handler() - register wmi event handler
 * @wmi_handle: handle to wmi
 * @event_id: wmi event id
 * @handler_func: wmi event handler function
 * @rx_ctx: rx execution context for wmi rx events
 *
 * Return: 0 on success
 */
int wmi_unified_register_event_handler(wmi_unified_t wmi_handle,
				       uint32_t event_id,
				       wmi_unified_event_handler handler_func,
				       uint8_t rx_ctx)
{
	uint32_t idx = 0;
	uint32_t evt_id;

#ifdef WMI_TLV_AND_NON_TLV_SUPPORT
	if (event_id >= wmi_events_max ||
		wmi_handle->wmi_events[event_id] == WMI_EVENT_ID_INVALID) {
		qdf_print("%s: Event id %d is unavailable\n",
				 __func__, event_id);
		return QDF_STATUS_E_FAILURE;
	}
	evt_id = wmi_handle->wmi_events[event_id];
#else
	evt_id = event_id;
#endif
	if (wmi_unified_get_event_handler_ix(wmi_handle, evt_id) != -1) {
		qdf_print("%s : event handler already registered 0x%x\n",
		       __func__, evt_id);
		return QDF_STATUS_E_FAILURE;
	}
	if (wmi_handle->max_event_idx == WMI_UNIFIED_MAX_EVENT) {
		qdf_print("%s : no more event handlers 0x%x\n",
		       __func__, evt_id);
		return QDF_STATUS_E_FAILURE;
	}
	idx = wmi_handle->max_event_idx;
	wmi_handle->event_handler[idx] = handler_func;
	wmi_handle->event_id[idx] = evt_id;
	qdf_spin_lock_bh(&wmi_handle->ctx_lock);
	wmi_handle->ctx[idx] = rx_ctx;
	qdf_spin_unlock_bh(&wmi_handle->ctx_lock);
	wmi_handle->max_event_idx++;

	return 0;
}

/**
 * wmi_unified_unregister_event_handler() - unregister wmi event handler
 * @wmi_handle: handle to wmi
 * @event_id: wmi event id
 *
 * Return: 0 on success
 */
int wmi_unified_unregister_event_handler(wmi_unified_t wmi_handle,
					 uint32_t event_id)
{
	uint32_t idx = 0;
	uint32_t evt_id;

#ifdef WMI_TLV_AND_NON_TLV_SUPPORT
	if (event_id >= wmi_events_max ||
		wmi_handle->wmi_events[event_id] == WMI_EVENT_ID_INVALID) {
		qdf_print("%s: Event id %d is unavailable\n",
				 __func__, event_id);
		return QDF_STATUS_E_FAILURE;
	}
	evt_id = wmi_handle->wmi_events[event_id];
#else
	evt_id = event_id;
#endif

	idx = wmi_unified_get_event_handler_ix(wmi_handle, evt_id);
	if (idx == -1) {
		qdf_print("%s : event handler is not registered: evt id 0x%x\n",
		       __func__, evt_id);
		return QDF_STATUS_E_FAILURE;
	}
	wmi_handle->event_handler[idx] = NULL;
	wmi_handle->event_id[idx] = 0;
	--wmi_handle->max_event_idx;
	wmi_handle->event_handler[idx] =
		wmi_handle->event_handler[wmi_handle->max_event_idx];
	wmi_handle->event_id[idx] =
		wmi_handle->event_id[wmi_handle->max_event_idx];

	return 0;
}

/**
 * wmi_process_fw_event_default_ctx() - process in default caller context
 * @wmi_handle: handle to wmi
 * @htc_packet: pointer to htc packet
 * @exec_ctx: execution context for wmi fw event
 *
 * Event process by below function will be in default caller context.
 * wmi internally provides rx work thread processing context.
 *
 * Return: none
 */
static void wmi_process_fw_event_default_ctx(struct wmi_unified *wmi_handle,
		       HTC_PACKET *htc_packet, uint8_t exec_ctx)
{
	wmi_buf_t evt_buf;
	evt_buf = (wmi_buf_t) htc_packet->pPktContext;

#ifdef WMI_NON_TLV_SUPPORT
	wmi_handle->rx_ops.wma_process_fw_event_handler_cbk
		(wmi_handle->scn_handle, evt_buf, exec_ctx);
#else
	wmi_handle->rx_ops.wma_process_fw_event_handler_cbk(wmi_handle,
					 evt_buf, exec_ctx);
#endif

	return;
}

/**
 * wmi_process_fw_event_worker_thread_ctx() - process in worker thread context
 * @wmi_handle: handle to wmi
 * @htc_packet: pointer to htc packet
 *
 * Event process by below function will be in worker thread context.
 * Use this method for events which are not critical and not
 * handled in protocol stack.
 *
 * Return: none
 */
static void wmi_process_fw_event_worker_thread_ctx
		(struct wmi_unified *wmi_handle, HTC_PACKET *htc_packet)
{
	wmi_buf_t evt_buf;
	uint32_t id;
	uint8_t *data;

	evt_buf = (wmi_buf_t) htc_packet->pPktContext;
	id = WMI_GET_FIELD(qdf_nbuf_data(evt_buf), WMI_CMD_HDR, COMMANDID);
	data = qdf_nbuf_data(evt_buf);

#ifdef WMI_INTERFACE_EVENT_LOGGING
	if (wmi_handle->log_info.wmi_logging_enable) {
		qdf_spin_lock_bh(&wmi_handle->log_info.wmi_record_lock);
		/* Exclude 4 bytes of TLV header */
		WMI_RX_EVENT_RECORD(wmi_handle, id, ((uint8_t *) data +
				wmi_handle->log_info.buf_offset_event));
		qdf_spin_unlock_bh(&wmi_handle->log_info.wmi_record_lock);
	}
#endif
	qdf_spin_lock_bh(&wmi_handle->eventq_lock);
	qdf_nbuf_queue_add(&wmi_handle->event_queue, evt_buf);
	qdf_spin_unlock_bh(&wmi_handle->eventq_lock);
	schedule_work(&wmi_handle->rx_event_work);
	return;
}

/**
 * wmi_control_rx() - process fw events callbacks
 * @ctx: handle to wmi
 * @htc_packet: pointer to htc packet
 *
 * Return: none
 */
static void wmi_control_rx(void *ctx, HTC_PACKET *htc_packet)
{
	struct wmi_unified *wmi_handle = (struct wmi_unified *)ctx;
	wmi_buf_t evt_buf;
	uint32_t id;
	uint32_t idx = 0;
	enum wmi_rx_exec_ctx exec_ctx;

	evt_buf = (wmi_buf_t) htc_packet->pPktContext;
	id = WMI_GET_FIELD(qdf_nbuf_data(evt_buf), WMI_CMD_HDR, COMMANDID);
	idx = wmi_unified_get_event_handler_ix(wmi_handle, id);
	if (qdf_unlikely(idx == A_ERROR)) {
		qdf_print
		("%s :event handler is not registered: event id 0x%x\n",
			__func__, id);
		qdf_nbuf_free(evt_buf);
		return;
	}
	qdf_spin_lock_bh(&wmi_handle->ctx_lock);
	exec_ctx = wmi_handle->ctx[idx];
	qdf_spin_unlock_bh(&wmi_handle->ctx_lock);

	if (exec_ctx == WMI_RX_WORK_CTX) {
		wmi_process_fw_event_worker_thread_ctx
					(wmi_handle, htc_packet);
	} else if (exec_ctx > WMI_RX_WORK_CTX) {
		wmi_process_fw_event_default_ctx
					(wmi_handle, htc_packet, exec_ctx);
	} else {
		qdf_print("%s :Invalid event context %d\n", __func__, exec_ctx);
		qdf_nbuf_free(evt_buf);
	}

}

/**
 * wmi_process_fw_event() - process any fw event
 * @wmi_handle: wmi handle
 * @evt_buf: fw event buffer
 *
 * This function process fw event in caller context
 *
 * Return: none
 */
void wmi_process_fw_event(struct wmi_unified *wmi_handle, wmi_buf_t evt_buf)
{
	__wmi_control_rx(wmi_handle, evt_buf);
}

/**
 * __wmi_control_rx() - process serialize wmi event callback
 * @wmi_handle: wmi handle
 * @evt_buf: fw event buffer
 *
 * Return: none
 */
void __wmi_control_rx(struct wmi_unified *wmi_handle, wmi_buf_t evt_buf)
{
	uint32_t id;
	uint8_t *data;
	uint32_t len;
	void *wmi_cmd_struct_ptr = NULL;
#ifndef WMI_NON_TLV_SUPPORT
	int tlv_ok_status = 0;
#endif
	uint32_t idx = 0;

	id = WMI_GET_FIELD(qdf_nbuf_data(evt_buf), WMI_CMD_HDR, COMMANDID);

	if (qdf_nbuf_pull_head(evt_buf, sizeof(WMI_CMD_HDR)) == NULL)
		goto end;

	data = qdf_nbuf_data(evt_buf);
	len = qdf_nbuf_len(evt_buf);

#ifndef WMI_NON_TLV_SUPPORT
	if (wmi_handle->target_type == WMI_TLV_TARGET) {
		/* Validate and pad(if necessary) the TLVs */
		tlv_ok_status =
			wmitlv_check_and_pad_event_tlvs(wmi_handle->scn_handle,
							data, len, id,
							&wmi_cmd_struct_ptr);
		if (tlv_ok_status != 0) {
			QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
				"%s: Error: id=0x%d, wmitlv check status=%d\n",
				__func__, id, tlv_ok_status);
			goto end;
		}
	}
#endif

	idx = wmi_unified_get_event_handler_ix(wmi_handle, id);
	if (idx == A_ERROR) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_ERROR,
		   "%s : event handler is not registered: event id 0x%x\n",
			__func__, id);
		goto end;
	}
#ifdef WMI_INTERFACE_EVENT_LOGGING
	if (wmi_handle->log_info.wmi_logging_enable) {
		qdf_spin_lock_bh(&wmi_handle->log_info.wmi_record_lock);
		/* Exclude 4 bytes of TLV header */
		if (wmi_handle->log_info.is_management_record(id)) {
			WMI_MGMT_EVENT_RECORD(wmi_handle, id, ((uint8_t *) data
				+ wmi_handle->log_info.buf_offset_event));
		} else {
			WMI_EVENT_RECORD(wmi_handle, id, ((uint8_t *) data +
					wmi_handle->log_info.buf_offset_event));
		}
		qdf_spin_unlock_bh(&wmi_handle->log_info.wmi_record_lock);
	}
#endif
	/* Call the WMI registered event handler */
	if (wmi_handle->target_type == WMI_TLV_TARGET)
		wmi_handle->event_handler[idx] (wmi_handle->scn_handle,
			wmi_cmd_struct_ptr, len);
	else
		wmi_handle->event_handler[idx] (wmi_handle->scn_handle,
			data, len);

end:
	/* Free event buffer and allocated event tlv */
#ifndef WMI_NON_TLV_SUPPORT
	if (wmi_handle->target_type == WMI_TLV_TARGET)
		wmitlv_free_allocated_event_tlvs(id, &wmi_cmd_struct_ptr);
#endif
	qdf_nbuf_free(evt_buf);

}

/**
 * wmi_rx_event_work() - process rx event in rx work queue context
 * @work: rx work queue struct
 *
 * This function process any fw event to serialize it through rx worker thread.
 *
 * Return: none
 */
static void wmi_rx_event_work(struct work_struct *work)
{
	struct wmi_unified *wmi = container_of(work, struct wmi_unified,
					rx_event_work);
	wmi_buf_t buf;

	qdf_spin_lock_bh(&wmi->eventq_lock);
	buf = qdf_nbuf_queue_remove(&wmi->event_queue);
	qdf_spin_unlock_bh(&wmi->eventq_lock);
	while (buf) {
		__wmi_control_rx(wmi, buf);
		qdf_spin_lock_bh(&wmi->eventq_lock);
		buf = qdf_nbuf_queue_remove(&wmi->event_queue);
		qdf_spin_unlock_bh(&wmi->eventq_lock);
	}
}

#ifdef FEATURE_RUNTIME_PM
/**
 * wmi_runtime_pm_init() - initialize runtime pm wmi variables
 * @wmi_handle: wmi context
 */
static void wmi_runtime_pm_init(struct wmi_unified *wmi_handle)
{
	qdf_atomic_init(&wmi_handle->runtime_pm_inprogress);
}

/**
 * wmi_set_runtime_pm_inprogress() - set runtime pm progress flag
 * @wmi_handle: wmi context
 * @val: runtime pm progress flag
 */
void wmi_set_runtime_pm_inprogress(wmi_unified_t wmi_handle, A_BOOL val)
{
	qdf_atomic_set(&wmi_handle->runtime_pm_inprogress, val);
}

/**
 * wmi_get_runtime_pm_inprogress() - get runtime pm progress flag
 * @wmi_handle: wmi context
 */
inline bool wmi_get_runtime_pm_inprogress(wmi_unified_t wmi_handle)
{
	return qdf_atomic_read(&wmi_handle->runtime_pm_inprogress);
}
#else
static void wmi_runtime_pm_init(struct wmi_unified *wmi_handle)
{
}
#endif

/**
 * wmi_unified_attach() -  attach for unified WMI
 * @scn_handle: handle to SCN
 * @osdev: OS device context
 * @target_type: TLV or not-TLV based target
 * @use_cookie: cookie based allocation enabled/disabled
 * @ops: umac rx callbacks
 *
 * @Return: wmi handle.
 */
void *wmi_unified_attach(void *scn_handle,
			 osdev_t osdev, enum wmi_target_type target_type,
			 bool use_cookie, struct wmi_rx_ops *rx_ops)
{
	struct wmi_unified *wmi_handle;

#ifndef WMI_NON_TLV_SUPPORT
	wmi_handle =
		(struct wmi_unified *)os_malloc(NULL,
				sizeof(struct wmi_unified),
				GFP_ATOMIC);
#else
	wmi_handle =
		(struct wmi_unified *) qdf_mem_malloc(
			sizeof(struct wmi_unified));
#endif
	if (wmi_handle == NULL) {
		qdf_print("allocation of wmi handle failed %zu\n",
			sizeof(struct wmi_unified));
		return NULL;
	}
	OS_MEMZERO(wmi_handle, sizeof(struct wmi_unified));
	wmi_handle->scn_handle = scn_handle;
	qdf_atomic_init(&wmi_handle->pending_cmds);
	qdf_atomic_init(&wmi_handle->is_target_suspended);
	wmi_runtime_pm_init(wmi_handle);
	qdf_spinlock_create(&wmi_handle->eventq_lock);
	qdf_nbuf_queue_init(&wmi_handle->event_queue);
	INIT_WORK(&wmi_handle->rx_event_work, wmi_rx_event_work);
#ifdef WMI_INTERFACE_EVENT_LOGGING
	if (QDF_STATUS_SUCCESS == wmi_log_init(wmi_handle)) {
		wmi_debugfs_init(wmi_handle);
	}
#endif
	/* Attach mc_thread context processing function */
	wmi_handle->rx_ops.wma_process_fw_event_handler_cbk =
				rx_ops->wma_process_fw_event_handler_cbk;
	wmi_handle->target_type = target_type;
	if (target_type == WMI_TLV_TARGET)
		wmi_tlv_attach(wmi_handle);
	else
		wmi_non_tlv_attach(wmi_handle);
	/* Assign target cookie capablity */
	wmi_handle->use_cookie = use_cookie;
	wmi_handle->osdev = osdev;
	wmi_handle->wmi_stopinprogress = 0;
	qdf_spinlock_create(&wmi_handle->ctx_lock);

	return wmi_handle;
}

/**
 * wmi_unified_detach() -  detach for unified WMI
 *
 * @wmi_handle  : handle to wmi.
 *
 * @Return: none.
 */
void wmi_unified_detach(struct wmi_unified *wmi_handle)
{
	wmi_buf_t buf;

	cancel_work_sync(&wmi_handle->rx_event_work);

	wmi_debugfs_remove(wmi_handle);

	buf = qdf_nbuf_queue_remove(&wmi_handle->event_queue);
	while (buf) {
		qdf_nbuf_free(buf);
		buf = qdf_nbuf_queue_remove(&wmi_handle->event_queue);
	}

#ifdef WMI_INTERFACE_EVENT_LOGGING
	wmi_log_buffer_free(wmi_handle);
#endif

	qdf_spinlock_destroy(&wmi_handle->eventq_lock);
	qdf_spinlock_destroy(&wmi_handle->ctx_lock);
	OS_FREE(wmi_handle);
	wmi_handle = NULL;
}

/**
 * wmi_unified_remove_work() - detach for WMI work
 * @wmi_handle: handle to WMI
 *
 * A function that does not fully detach WMI, but just remove work
 * queue items associated with it. This is used to make sure that
 * before any other processing code that may destroy related contexts
 * (HTC, etc), work queue processing on WMI has already been stopped.
 *
 * Return: None
 */
void
wmi_unified_remove_work(struct wmi_unified *wmi_handle)
{
	wmi_buf_t buf;

	QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_INFO,
		"Enter: %s", __func__);
	cancel_work_sync(&wmi_handle->rx_event_work);
	qdf_spin_lock_bh(&wmi_handle->eventq_lock);
	buf = qdf_nbuf_queue_remove(&wmi_handle->event_queue);
	while (buf) {
		qdf_nbuf_free(buf);
		buf = qdf_nbuf_queue_remove(&wmi_handle->event_queue);
	}
	qdf_spin_unlock_bh(&wmi_handle->eventq_lock);
	QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_INFO,
		"Done: %s", __func__);
}

/**
 * wmi_htc_tx_complete() - Process htc tx completion
 *
 * @ctx: handle to wmi
 * @htc_packet: pointer to htc packet
 *
 * @Return: none.
 */
static void wmi_htc_tx_complete(void *ctx, HTC_PACKET *htc_pkt)
{
	struct wmi_unified *wmi_handle = (struct wmi_unified *)ctx;
	wmi_buf_t wmi_cmd_buf = GET_HTC_PACKET_NET_BUF_CONTEXT(htc_pkt);
	u_int8_t *buf_ptr;
	u_int32_t len;
#ifdef WMI_INTERFACE_EVENT_LOGGING
	uint32_t cmd_id;
#endif

	ASSERT(wmi_cmd_buf);
#ifdef WMI_INTERFACE_EVENT_LOGGING
	if (wmi_handle->log_info.wmi_logging_enable) {
		cmd_id = WMI_GET_FIELD(qdf_nbuf_data(wmi_cmd_buf),
				WMI_CMD_HDR, COMMANDID);

	WMI_LOGD("Sent WMI command:%s command_id:0x%x over dma and recieved tx complete interupt",
		 wmi_id_to_name(cmd_id), cmd_id);

	qdf_spin_lock_bh(&wmi_handle->log_info.wmi_record_lock);
	/* Record 16 bytes of WMI cmd tx complete data
	- exclude TLV and WMI headers */
	if (wmi_handle->log_info.is_management_record(cmd_id)) {
		WMI_MGMT_COMMAND_TX_CMP_RECORD(wmi_handle, cmd_id,
			((uint32_t *) qdf_nbuf_data(wmi_cmd_buf) +
			wmi_handle->log_info.buf_offset_command));
	} else {
		WMI_COMMAND_TX_CMP_RECORD(wmi_handle, cmd_id,
			((uint32_t *) qdf_nbuf_data(wmi_cmd_buf) +
			wmi_handle->log_info.buf_offset_command));
	}

	qdf_spin_unlock_bh(&wmi_handle->log_info.wmi_record_lock);
	}
#endif
	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_cmd_buf);
	len = qdf_nbuf_len(wmi_cmd_buf);
	qdf_mem_zero(buf_ptr, len);
	qdf_nbuf_free(wmi_cmd_buf);
	qdf_mem_free(htc_pkt);
	qdf_atomic_dec(&wmi_handle->pending_cmds);
}

/**
 * wmi_get_host_credits() -  WMI API to get updated host_credits
 *
 * @wmi_handle: handle to WMI.
 *
 * @Return: updated host_credits.
 */
int
wmi_unified_connect_htc_service(struct wmi_unified *wmi_handle,
				void *htc_handle)
{

	int status;
	HTC_SERVICE_CONNECT_RESP response;
	HTC_SERVICE_CONNECT_REQ connect;

	OS_MEMZERO(&connect, sizeof(connect));
	OS_MEMZERO(&response, sizeof(response));

	/* meta data is unused for now */
	connect.pMetaData = NULL;
	connect.MetaDataLength = 0;
	/* these fields are the same for all service endpoints */
	connect.EpCallbacks.pContext = wmi_handle;
	connect.EpCallbacks.EpTxCompleteMultiple =
		NULL /* Control path completion ar6000_tx_complete */;
	connect.EpCallbacks.EpRecv = wmi_control_rx /* Control path rx */;
	connect.EpCallbacks.EpRecvRefill = NULL /* ar6000_rx_refill */;
	connect.EpCallbacks.EpSendFull = NULL /* ar6000_tx_queue_full */;
	connect.EpCallbacks.EpTxComplete =
		wmi_htc_tx_complete /* ar6000_tx_queue_full */;

	/* connect to control service */
	connect.service_id = WMI_CONTROL_SVC;
	status = htc_connect_service(htc_handle, &connect,
				&response);

	if (status != EOK) {
		qdf_print
			("Failed to connect to WMI CONTROL service status:%d \n",
			status);
		return status;
	}
	wmi_handle->wmi_endpoint_id = response.Endpoint;
	wmi_handle->htc_handle = htc_handle;
	wmi_handle->max_msg_len = response.MaxMsgLength;

	return EOK;
}

/**
 * wmi_get_host_credits() -  WMI API to get updated host_credits
 *
 * @wmi_handle: handle to WMI.
 *
 * @Return: updated host_credits.
 */
int wmi_get_host_credits(wmi_unified_t wmi_handle)
{
	int host_credits = 0;

	htc_get_control_endpoint_tx_host_credits(wmi_handle->htc_handle,
						 &host_credits);
	return host_credits;
}

/**
 * wmi_get_pending_cmds() - WMI API to get WMI Pending Commands in the HTC
 *                          queue
 *
 * @wmi_handle: handle to WMI.
 *
 * @Return: Pending Commands in the HTC queue.
 */
int wmi_get_pending_cmds(wmi_unified_t wmi_handle)
{
	return qdf_atomic_read(&wmi_handle->pending_cmds);
}

/**
 * wmi_set_target_suspend() -  WMI API to set target suspend state
 *
 * @wmi_handle: handle to WMI.
 * @val: suspend state boolean.
 *
 * @Return: none.
 */
void wmi_set_target_suspend(wmi_unified_t wmi_handle, A_BOOL val)
{
	qdf_atomic_set(&wmi_handle->is_target_suspended, val);
}

/**
 * WMI API to set crash injection state
 * @param wmi_handle:	handle to WMI.
 * @param val:		crash injection state boolean.
 */
void wmi_tag_crash_inject(wmi_unified_t wmi_handle, A_BOOL flag)
{
	wmi_handle->tag_crash_inject = flag;
}

/**
 * WMI API to set bus suspend state
 * @param wmi_handle:	handle to WMI.
 * @param val:		suspend state boolean.
 */
void wmi_set_is_wow_bus_suspended(wmi_unified_t wmi_handle, A_BOOL val)
{
	qdf_atomic_set(&wmi_handle->is_wow_bus_suspended, val);
}

/**
 * wmi_set_tgt_assert() - set target assert configuration
 *
 * @wmi_handle      : handle to WMI.
 * @val             : target assert config value
 *
 * @Return: none.
 */
void wmi_set_tgt_assert(wmi_unified_t wmi_handle, bool val)
{
	wmi_handle->tgt_force_assert_enable = val;
}
#ifdef WMI_NON_TLV_SUPPORT
/**
 * API to flush all the previous packets  associated with the wmi endpoint
 *
 * @param wmi_handle      : handle to WMI.
 */
void
wmi_flush_endpoint(wmi_unified_t wmi_handle)
{
	htc_flush_endpoint(wmi_handle->htc_handle,
		wmi_handle->wmi_endpoint_id, 0);
}

/**
 * generic function to block unified WMI command
 * @param wmi_handle      : handle to WMI.
 * @return 0  on success and -ve on failure.
 */
int
wmi_stop(wmi_unified_t wmi_handle)
{
	QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_INFO,
			"WMI Stop\n");
	wmi_handle->wmi_stopinprogress = 1;
	return 0;
}
#endif
