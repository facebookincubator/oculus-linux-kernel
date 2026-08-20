/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM stp-channel

#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR stp_channel

#if !defined(_TRACE_EVENT_STP_CHANNEL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_STP_CHANNEL_H

#include <linux/tracepoint.h>


DECLARE_EVENT_CLASS(stp_simple_event_template,
	TP_PROTO(u8 opcode),

	TP_ARGS(opcode),

	TP_STRUCT__entry(
		__field(	u8,	opcode		)
	),

	TP_fast_assign(
		__entry->opcode	= opcode;
	),

	TP_printk("val=%d", __entry->opcode)
);
DEFINE_EVENT(stp_simple_event_template, stp_switch_to_stpraw,
	TP_PROTO(u8 opcode),
	TP_ARGS(opcode));
DEFINE_EVENT(stp_simple_event_template, stp_switch_to_stp,
	TP_PROTO(u8 opcode),
	TP_ARGS(opcode));
DEFINE_EVENT(stp_simple_event_template, stp_mcu_request_transaction,
	TP_PROTO(u8 opcode),
	TP_ARGS(opcode));
DEFINE_EVENT(stp_simple_event_template, stp_set_soc_has_data,
	TP_PROTO(u8 opcode),
	TP_ARGS(opcode));
DEFINE_EVENT(stp_simple_event_template, stp_schedule_wdt_work,
	TP_PROTO(u8 opcode),
	TP_ARGS(opcode));
DEFINE_EVENT(stp_simple_event_template, stp_cancel_wdt_work,
	TP_PROTO(u8 opcode),
	TP_ARGS(opcode));
DEFINE_EVENT(stp_simple_event_template, stp_wdt_bark,
	TP_PROTO(u8 opcode),
	TP_ARGS(opcode));

DECLARE_EVENT_CLASS(stp_channel_simple_event_template,
	TP_PROTO(u8 channel),

	TP_ARGS(channel),

	TP_STRUCT__entry(
		__field(	u8,	channel		)
	),

	TP_fast_assign(
		__entry->channel	= channel;
	),

	TP_printk("ch%d", __entry->channel)
);

DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_signal_write,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_signal_read,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_signal_fsync,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_reset_fsync,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_signal_open,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_unset_inuse,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_device_nonblocking_open,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_device_blocking_open,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_device_open_done,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));
DEFINE_EVENT(stp_channel_simple_event_template, stp_channel_interrupt,
	TP_PROTO(u8 channel),
	TP_ARGS(channel));


TRACE_EVENT(stp_channel_controller_open,

        TP_PROTO(u8 channel, int controller_rval, int rval),

        TP_ARGS(channel, controller_rval, rval),

        TP_STRUCT__entry(
		__field(	u8,	channel		)
		__field(	int,	controller_rval	)
		__field(	int,	rval		)
        ),

        TP_fast_assign(
		__entry->channel	= channel;
		__entry->controller_rval= controller_rval;
		__entry->rval		= rval;
        ),

        TP_printk("ch%d, controller_rval=%d, rval=%d)",
		__entry->channel,
		__entry->controller_rval,
		__entry->rval)
);

TRACE_EVENT(stp_channel_set_inuse,

        TP_PROTO(u8 channel, u8 ntries),

        TP_ARGS(channel, ntries),

        TP_STRUCT__entry(
		__field(	u8,	channel		)
		__field(	u8,	ntries		)
        ),

        TP_fast_assign(
		__entry->channel	= channel;
		__entry->ntries		= ntries;
        ),

        TP_printk("ch%d (ntries=%d)", __entry->channel, __entry->ntries)
);

TRACE_EVENT(stp_channel_read,

        TP_PROTO(u8 channel, u8 blocking, size_t pending_count, size_t count, size_t read_count, int controller_rval, int rval),

        TP_ARGS(channel, blocking, pending_count, count, read_count, controller_rval, rval),

        TP_STRUCT__entry(
		__field(	u8,	channel		)
		__field(	u8,	blocking	)
		__field(	size_t,	pending_count	)
		__field(	size_t,	count		)
		__field(	size_t,	read_count	)
		__field(	int,	controller_rval	)
		__field(	int,	rval		)
        ),

        TP_fast_assign(
		__entry->channel	= channel;
		__entry->blocking	= blocking;
		__entry->pending_count	= pending_count;
		__entry->count		= count;
		__entry->read_count	= read_count;
		__entry->controller_rval= controller_rval;
		__entry->rval		= rval;
        ),

        TP_printk("ch%d, blocking=%d, pending_count=%zu, count=%zu, read_count=%zu, controller_rval=%d, rval=%d",
			__entry->channel,
			__entry->blocking,
			__entry->pending_count,
			__entry->count,
			__entry->read_count,
			__entry->controller_rval,
			__entry->rval)
);

TRACE_EVENT(stp_channel_read_exit,

        TP_PROTO(u8 channel, ssize_t rval),

        TP_ARGS(channel, rval),

        TP_STRUCT__entry(
		__field(	u8,	channel		)
		__field(	ssize_t,rval		)
        ),

        TP_fast_assign(
		__entry->channel	= channel;
		__entry->rval		= rval;
        ),

        TP_printk("ch%d, return %zd",
			__entry->channel,
			__entry->rval)
);

TRACE_EVENT(stp_channel_write,

        TP_PROTO(u8 channel, u8 blocking, size_t pending_count, size_t count, size_t send_count, int controller_rval, int rval),

        TP_ARGS(channel, blocking, pending_count, count, send_count, controller_rval, rval),

        TP_STRUCT__entry(
		__field(	u8,	channel		)
		__field(	u8,	blocking	)
		__field(	size_t,	pending_count	)
		__field(	size_t,	count		)
		__field(	size_t,	send_count	)
		__field(	int,	controller_rval	)
		__field(	int,	rval		)
        ),

        TP_fast_assign(
		__entry->channel	= channel;
		__entry->blocking	= blocking;
		__entry->pending_count	= pending_count;
		__entry->count		= count;
		__entry->send_count	= send_count;
		__entry->controller_rval= controller_rval;
		__entry->rval		= rval;
        ),

        TP_printk("ch%d, blocking=%d, pending_count=%zu, count=%zu, send_count=%zu, controller_rval=%d, rval=%d",
			__entry->channel,
			__entry->blocking,
			__entry->pending_count,
			__entry->count,
			__entry->send_count,
			__entry->controller_rval,
			__entry->rval)
);

TRACE_EVENT(stp_channel_write_exit,

        TP_PROTO(u8 channel, ssize_t rval),

        TP_ARGS(channel, rval),

        TP_STRUCT__entry(
		__field(	u8,	channel		)
		__field(	ssize_t,rval		)
        ),

        TP_fast_assign(
		__entry->channel	= channel;
		__entry->rval		= rval;
        ),

        TP_printk("ch%d, return %zd",
			__entry->channel,
			__entry->rval)
);

DECLARE_EVENT_CLASS(stp_channel_poll_event_template,
	TP_PROTO(u8 channel, u64 events),

	TP_ARGS(channel, events),

	TP_STRUCT__entry(
		__field(	u8,	channel		)
		__field(	u64,	events		)
	),

	TP_fast_assign(
		__entry->channel	= channel;
		__entry->events		= events;
	),

	TP_printk("ch%d, events=%s",
		__entry->channel,
		__print_flags(__entry->events, "|",
			{          EPOLLIN, "EPOLLIN" },
			{         EPOLLPRI, "EPOLLPRI" },
			{         EPOLLOUT, "EPOLLOUT" },
			{         EPOLLERR, "EPOLLERR" },
			{         EPOLLHUP, "EPOLLHUP" },
			{        EPOLLNVAL, "EPOLLNVAL" },
			{      EPOLLRDNORM, "EPOLLRDNORM" },
			{      EPOLLRDBAND, "EPOLLRDBAND" },
			{      EPOLLWRNORM, "EPOLLWRNORM" },
			{      EPOLLWRBAND, "EPOLLWRBAND" },
			{         EPOLLMSG, "EPOLLMSG" },
			{       EPOLLRDHUP, "EPOLLRDHUP" },
#ifdef EPOLL_URING_WAKE
			{ EPOLL_URING_WAKE, "EPOLL_URING_WAKE" },
#endif
			{   EPOLLEXCLUSIVE, "EPOLLEXCLUSIVE" },
			{      EPOLLWAKEUP, "EPOLLWAKEUP" },
			{     EPOLLONESHOT, "EPOLLONESHOT" },
			{          EPOLLET, "EPOLLET" }))
);

DEFINE_EVENT(stp_channel_poll_event_template, stp_channel_poll_enter,
	TP_PROTO(u8 channel, u64 events),
	TP_ARGS(channel, events));
DEFINE_EVENT(stp_channel_poll_event_template, stp_channel_poll_exit,
	TP_PROTO(u8 channel, u64 events),
	TP_ARGS(channel, events));


DECLARE_EVENT_CLASS(stp_buffer_dump_event_template,
	TP_PROTO(u8 *buf, size_t len),

	TP_ARGS(buf, len),

	TP_STRUCT__entry(
                __dynamic_array(u8, buf, len)
                __field(size_t, len)
	),

	TP_fast_assign(
		memcpy(__get_dynamic_array(buf), buf, len);
		__entry->len	= len;
	),

	TP_printk("data=%s",
                  __print_hex_dump("", DUMP_PREFIX_NONE, 16, 1,
                                   __get_dynamic_array(buf),
                                   __get_dynamic_array_len(buf), false))
);
DEFINE_EVENT(stp_buffer_dump_event_template, stp_spi_send_data,
	TP_PROTO(u8 *buf, size_t len),
	TP_ARGS(buf, len));
DEFINE_EVENT(stp_buffer_dump_event_template, stp_spi_recv_data,
	TP_PROTO(u8 *buf, size_t len),
	TP_ARGS(buf, len));

/***** NOTICE! The #if protection ends here. *****/
#endif


#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH device

#define TRACE_INCLUDE_FILE stp_channel_events

#include <trace/define_trace.h>
