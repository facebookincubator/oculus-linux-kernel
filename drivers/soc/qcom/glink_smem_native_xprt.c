/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ipc_logging.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>
#include <linux/wait.h>
#include <linux/cpumask.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/tracer_pkt.h>
#include "glink_core_if.h"
#include "glink_private.h"
#include "glink_xprt_if.h"

#define XPRT_NAME "smem"
#define FIFO_FULL_RESERVE 8
#define FIFO_ALIGNMENT 8
#define TX_BLOCKED_CMD_RESERVE 8 /* size of struct read_notif_request */
#define SMEM_CH_DESC_SIZE 32
#define RPM_TOC_ID 0x67727430
#define RPM_TX_FIFO_ID 0x61703272
#define RPM_RX_FIFO_ID 0x72326170
#define RPM_TOC_SIZE 256
#define RPM_MAX_TOC_ENTRIES 20
#define RPM_FIFO_ADDR_ALIGN_BYTES 3
#define TRACER_PKT_FEATURE BIT(2)
#define DEFERRED_CMDS_THRESHOLD 25
#define NUM_LOG_PAGES	4

/**
 * enum command_types - definition of the types of commands sent/received
 * @VERSION_CMD:		Version and feature set supported
 * @VERSION_ACK_CMD:		Response for @VERSION_CMD
 * @OPEN_CMD:			Open a channel
 * @CLOSE_CMD:			Close a channel
 * @OPEN_ACK_CMD:		Response to @OPEN_CMD
 * @RX_INTENT_CMD:		RX intent for a channel was queued
 * @RX_DONE_CMD:		Use of RX intent for a channel is complete
 * @RX_INTENT_REQ_CMD:		Request to have RX intent queued
 * @RX_INTENT_REQ_ACK_CMD:	Response for @RX_INTENT_REQ_CMD
 * @TX_DATA_CMD:		Start of a data transfer
 * @ZERO_COPY_TX_DATA_CMD:	Start of a data transfer with zero copy
 * @CLOSE_ACK_CMD:		Response for @CLOSE_CMD
 * @TX_DATA_CONT_CMD:		Continuation or end of a data transfer
 * @READ_NOTIF_CMD:		Request for a notification when this cmd is read
 * @RX_DONE_W_REUSE_CMD:	Same as @RX_DONE but also reuse the used intent
 * @SIGNALS_CMD:		Sideband signals
 * @TRACER_PKT_CMD:		Start of a Tracer Packet Command
 * @TRACER_PKT_CONT_CMD:	Continuation or end of a Tracer Packet Command
 */
enum command_types {
	VERSION_CMD,
	VERSION_ACK_CMD,
	OPEN_CMD,
	CLOSE_CMD,
	OPEN_ACK_CMD,
	RX_INTENT_CMD,
	RX_DONE_CMD,
	RX_INTENT_REQ_CMD,
	RX_INTENT_REQ_ACK_CMD,
	TX_DATA_CMD,
	ZERO_COPY_TX_DATA_CMD,
	CLOSE_ACK_CMD,
	TX_DATA_CONT_CMD,
	READ_NOTIF_CMD,
	RX_DONE_W_REUSE_CMD,
	SIGNALS_CMD,
	TRACER_PKT_CMD,
	TRACER_PKT_CONT_CMD,
};

/**
 * struct channel_desc - description of a channel fifo with a remote entity
 * @read_index:		The read index for the fifo where data should be
 *			consumed from.
 * @write_index:	The write index for the fifo where data should produced
 *			to.
 *
 * This structure resides in SMEM and contains the control information for the
 * fifo data pipes of the channel.  There is one physical channel between us
 * and a remote entity.
 */
struct channel_desc {
	uint32_t read_index;
	uint32_t write_index;
};

/**
 * struct mailbox_config_info - description of a mailbox tranposrt channel
 * @tx_read_index:	Offset into the tx fifo where data should be read from.
 * @tx_write_index:	Offset into the tx fifo where new data will be placed.
 * @tx_size:		Size of the transmit fifo in bytes.
 * @rx_read_index:	Offset into the rx fifo where data should be read from.
 * @rx_write_index:	Offset into the rx fifo where new data will be placed.
 * @rx_size:		Size of the receive fifo in bytes.
 * @fifo:		The fifos for the channel.
 */
struct mailbox_config_info {
	uint32_t tx_read_index;
	uint32_t tx_write_index;
	uint32_t tx_size;
	uint32_t rx_read_index;
	uint32_t rx_write_index;
	uint32_t rx_size;
	char fifo[]; /* tx fifo, then rx fifo */
};

/**
 * struct edge_info - local information for managing a single complete edge
 * @xprt_if:			The transport interface registered with the
 *				glink core associated with this edge.
 * @xprt_cfg:			The transport configuration for the glink core
 *				assocaited with this edge.
 * @intentless:			True if this edge runs in intentless mode.
 * @irq_disabled:		Flag indicating the whether interrupt is enabled
 *				or disabled.
 * @remote_proc_id:		The SMEM processor id for the remote side.
 * @rx_reset_reg:		Reference to the register to reset the rx irq
 *				line, if applicable.
 * @out_irq_reg:		Reference to the register to send an irq to the
 *				remote side.
 * @out_irq_mask:		Mask written to @out_irq_reg to trigger the
 *				correct irq.
 * @irq_line:			The incoming interrupt line.
 * @tx_irq_count:		Number of interrupts triggered.
 * @rx_irq_count:		Number of interrupts received.
 * @tx_ch_desc:			Reference to the channel description structure
 *				for tx in SMEM for this edge.
 * @rx_ch_desc:			Reference to the channel description structure
 *				for rx in SMEM for this edge.
 * @tx_fifo:			Reference to the transmit fifo in SMEM.
 * @rx_fifo:			Reference to the receive fifo in SMEM.
 * @tx_fifo_size:		Total size of @tx_fifo.
 * @rx_fifo_size:		Total size of @rx_fifo.
 * @read_from_fifo:		Memcpy for this edge.
 * @write_to_fifo:		Memcpy for this edge.
 * @write_lock:			Lock to serialize access to @tx_fifo.
 * @tx_blocked_queue:		Queue of entities waiting for the remote side to
 *				signal @tx_fifo has flushed and is now empty.
 * @tx_resume_needed:		A tx resume signal needs to be sent to the glink
 *				core once the remote side indicates @tx_fifo has
 *				flushed.
 * @tx_blocked_signal_sent:	Flag to indicate the flush signal has already
 *				been sent, and a response is pending from the
 *				remote side.  Protected by @write_lock.
 * @debug_mask			mask to set debugging level.
 * @kwork:			Work to be executed when an irq is received.
 * @kworker:			Handle to the entity processing of
				deferred commands.
 * @task:			Handle to the task context used to run @kworker.
 * @use_ref:			Active uses of this transport use this to grab
 *				a reference.  Used for ssr synchronization.
 * @in_ssr:			Signals if this transport is in ssr.
 * @rx_lock:			Used to serialize concurrent instances of rx
 *				processing.
 * @deferred_cmds:		List of deferred commands that need to be
 *				processed in process context.
 * @deferred_cmds_cnt:		Number of deferred commands in queue.
 * @rt_vote_lock:		Serialize access to RT rx votes
 * @rt_votes:			Vote count for RT rx thread priority
 * @num_pw_states:		Size of @ramp_time_us.
 * @ramp_time_us:		Array of ramp times in microseconds where array
 *				index position represents a power state.
 * @mailbox:			Mailbox transport channel description reference.
 * @log_ctx:			Pointer to log context.
 */
struct edge_info {
	struct glink_transport_if xprt_if;
	struct glink_core_transport_cfg xprt_cfg;
	bool intentless;
	bool irq_disabled;
	uint32_t remote_proc_id;
	void __iomem *rx_reset_reg;
	void __iomem *out_irq_reg;
	uint32_t out_irq_mask;
	uint32_t irq_line;
	uint32_t tx_irq_count;
	uint32_t rx_irq_count;
	struct channel_desc *tx_ch_desc;
	struct channel_desc *rx_ch_desc;
	void __iomem *tx_fifo;
	void __iomem *rx_fifo;
	uint32_t tx_fifo_size;
	uint32_t rx_fifo_size;
	void * (*read_from_fifo)(void *dest, const void *src, size_t num_bytes);
	void * (*write_to_fifo)(void *dest, const void *src, size_t num_bytes);
	spinlock_t write_lock;
	wait_queue_head_t tx_blocked_queue;
	bool tx_resume_needed;
	bool tx_blocked_signal_sent;
	unsigned int debug_mask;
	struct kthread_work kwork;
	struct kthread_worker kworker;
	struct task_struct *task;
	struct srcu_struct use_ref;
	bool in_ssr;
	spinlock_t rx_lock;
	struct list_head deferred_cmds;
	uint32_t deferred_cmds_cnt;
	spinlock_t rt_vote_lock;
	uint32_t rt_votes;
	uint32_t num_pw_states;
	uint32_t readback;
	unsigned long *ramp_time_us;
	struct mailbox_config_info *mailbox;
	void *log_ctx;
};

/**
 * struct deferred_cmd - description of a command to be processed later
 * @list_node:	Used to put this command on a list in the edge.
 * @id:		ID of the command.
 * @param1:	Parameter one of the command.
 * @param2:	Parameter two of the command.
 * @data:	Extra data associated with the command, if applicable.
 *
 * This structure stores the relevant information of a command that was removed
 * from the fifo but needs to be processed at a later time.
 */
struct deferred_cmd {
	struct list_head list_node;
	uint16_t id;
	uint16_t param1;
	uint32_t param2;
	void *data;
};

static uint32_t negotiate_features_v1(struct glink_transport_if *if_ptr,
				      const struct glink_core_version *version,
				      uint32_t features);
static void register_debugfs_info(struct edge_info *einfo);

static struct edge_info *edge_infos[NUM_SMEM_SUBSYSTEMS];
static DEFINE_MUTEX(probe_lock);
static struct glink_core_version versions[] = {
	{1, TRACER_PKT_FEATURE, negotiate_features_v1},
};

#define SMEM_IPC_LOG(einfo, str, id, param1, param2) do { \
	if ((glink_xprt_debug_mask & QCOM_GLINK_DEBUG_ENABLE) \
		&& (einfo->debug_mask & QCOM_GLINK_DEBUG_ENABLE)) \
		ipc_log_string(einfo->log_ctx, \
				"%s: Rx:%x:%x Tx:%x:%x Cmd:%x P1:%x P2:%x\n", \
				str, einfo->rx_ch_desc->read_index, \
				einfo->rx_ch_desc->write_index, \
				einfo->tx_ch_desc->read_index, \
				einfo->tx_ch_desc->write_index, \
				id, param1, param2); \
} while (0) \

enum {
	QCOM_GLINK_DEBUG_ENABLE = 1U << 0,
	QCOM_GLINK_DEBUG_DISABLE = 1U << 1,
};

static unsigned int glink_xprt_debug_mask = QCOM_GLINK_DEBUG_ENABLE;
module_param_named(debug_mask, glink_xprt_debug_mask,
		   uint, 0664);

/**
 * send_irq() - send an irq to a remote entity as an event signal
 * @einfo:	Which remote entity that should receive the irq.
 */
static void send_irq(struct edge_info *einfo)
{
	/*
	 * Any data associated with this event must be visable to the remote
	 * before the interrupt is triggered
	 */
	einfo->readback = einfo->tx_ch_desc->write_index;
	wmb();
	writel_relaxed(einfo->out_irq_mask, einfo->out_irq_reg);
	if (einfo->remote_proc_id != SMEM_SPSS)
		writel_relaxed(0, einfo->out_irq_reg);
	einfo->tx_irq_count++;
}

/**
 * read_from_fifo() - memcpy from fifo memory
 * @dest:	Destination address.
 * @src:	Source address.
 * @num_bytes:	Number of bytes to copy.
 *
 * Return: Destination address.
 */
static void *read_from_fifo(void *dest, const void *src, size_t num_bytes)
{
	memcpy_fromio(dest, src, num_bytes);
	return dest;
}

/**
 * write_to_fifo() - memcpy to fifo memory
 * @dest:	Destination address.
 * @src:	Source address.
 * @num_bytes:	Number of bytes to copy.
 *
 * Return: Destination address.
 */
static void *write_to_fifo(void *dest, const void *src, size_t num_bytes)
{
	memcpy_toio(dest, src, num_bytes);
	return dest;
}

/**
 * memcpy32_toio() - memcpy to word access only memory
 * @dest:	Destination address.
 * @src:	Source address.
 * @num_bytes:	Number of bytes to copy.
 *
 * Return: Destination address.
 */
static void *memcpy32_toio(void *dest, const void *src, size_t num_bytes)
{
	uint32_t *dest_local = (uint32_t *)dest;
	uint32_t *src_local = (uint32_t *)src;

	if (WARN_ON(num_bytes & RPM_FIFO_ADDR_ALIGN_BYTES))
		return ERR_PTR(-EINVAL);
	if (WARN_ON(!dest_local ||
			((uintptr_t)dest_local & RPM_FIFO_ADDR_ALIGN_BYTES)))
		return ERR_PTR(-EINVAL);
	if (WARN_ON(!src_local ||
			((uintptr_t)src_local & RPM_FIFO_ADDR_ALIGN_BYTES)))
		return ERR_PTR(-EINVAL);
	num_bytes /= sizeof(uint32_t);

	while (num_bytes--)
		__raw_writel_no_log(*src_local++, dest_local++);

	return dest;
}

/**
 * memcpy32_fromio() - memcpy from word access only memory
 * @dest:	Destination address.
 * @src:	Source address.
 * @num_bytes:	Number of bytes to copy.
 *
 * Return: Destination address.
 */
static void *memcpy32_fromio(void *dest, const void *src, size_t num_bytes)
{
	uint32_t *dest_local = (uint32_t *)dest;
	uint32_t *src_local = (uint32_t *)src;

	if (WARN_ON(num_bytes & RPM_FIFO_ADDR_ALIGN_BYTES))
		return ERR_PTR(-EINVAL);
	if (WARN_ON(!dest_local ||
			((uintptr_t)dest_local & RPM_FIFO_ADDR_ALIGN_BYTES)))
		return ERR_PTR(-EINVAL);
	if (WARN_ON(!src_local ||
			((uintptr_t)src_local & RPM_FIFO_ADDR_ALIGN_BYTES)))
		return ERR_PTR(-EINVAL);
	num_bytes /= sizeof(uint32_t);

	while (num_bytes--)
		*dest_local++ = __raw_readl_no_log(src_local++);

	return dest;
}

/**
 * fifo_read_avail() - how many bytes are available to be read from an edge
 * @einfo:	The concerned edge to query.
 *
 * Return: The number of bytes available to be read from edge.
 */
static uint32_t fifo_read_avail(struct edge_info *einfo)
{
	uint32_t read_index = einfo->rx_ch_desc->read_index;
	uint32_t write_index = einfo->rx_ch_desc->write_index;
	uint32_t fifo_size = einfo->rx_fifo_size;
	uint32_t bytes_avail;

	bytes_avail = write_index - read_index;
	if (write_index < read_index)
		/*
		 * Case:  W < R - Write has wrapped
		 * --------------------------------
		 * In this case, the write operation has wrapped past the end
		 * of the FIFO which means that now calculating the amount of
		 * data in the FIFO results in a negative number.  This can be
		 * easily fixed by adding the fifo_size to the value.  Even
		 * though the values are unsigned, subtraction is always done
		 * using 2's complement which means that the result will still
		 * be correct once the FIFO size has been added to the negative
		 * result.
		 *
		 * Example:
		 *     '-' = data in fifo
		 *     '.' = empty
		 *
		 *      0         1
		 *      0123456789012345
		 *     |-----w.....r----|
		 *      0               N
		 *
		 *     write = 5 = 101b
		 *     read = 11 = 1011b
		 *     Data in FIFO
		 *       (write - read) + fifo_size = (101b - 1011b) + 10000b
		 *                          = 11111010b + 10000b = 1010b = 10
		 */
		bytes_avail += fifo_size;

	return bytes_avail;
}

/**
 * fifo_write_avail() - how many bytes can be written to the edge
 * @einfo:	The concerned edge to query.
 *
 * Calculates the number of bytes that can be transmitted at this time.
 * Automatically reserves some space to maintain alignment when the fifo is
 * completely full, and reserves space so that the flush command can always be
 * transmitted when needed.
 *
 * Return: The number of bytes available to be read from edge.
 */
static uint32_t fifo_write_avail(struct edge_info *einfo)
{
	uint32_t read_index = einfo->tx_ch_desc->read_index;
	uint32_t write_index = einfo->tx_ch_desc->write_index;
	uint32_t fifo_size = einfo->tx_fifo_size;
	uint32_t bytes_avail = read_index - write_index;

	if (read_index <= write_index)
		bytes_avail += fifo_size;
	if (bytes_avail < FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE)
		bytes_avail = 0;
	else
		bytes_avail -= FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE;

	return bytes_avail;
}

/**
 * fifo_read() - read data from an edge
 * @einfo:	The concerned edge to read from.
 * @_data:	Buffer to copy the read data into.
 * @len:	The ammount of data to read in bytes.
 *
 * Return: The number of bytes read.
 */
static int fifo_read(struct edge_info *einfo, void *_data, int len)
{
	void *ptr;
	void *ret;
	void *data = _data;
	int orig_len = len;
	uint32_t read_index = einfo->rx_ch_desc->read_index;
	uint32_t write_index = einfo->rx_ch_desc->write_index;
	uint32_t fifo_size = einfo->rx_fifo_size;
	uint32_t n;

	if (read_index >= fifo_size || write_index >= fifo_size) {
		WARN_ON_ONCE(1);
		return -EINVAL;
	}
	while (len) {
		ptr = einfo->rx_fifo + read_index;
		if (read_index <= write_index)
			n = write_index - read_index;
		else
			n = fifo_size - read_index;

		if (n == 0)
			break;
		if (n > len)
			n = len;

		ret = einfo->read_from_fifo(data, ptr, n);
		if (IS_ERR(ret))
			return PTR_ERR(ret);

		data += n;
		len -= n;
		read_index += n;
		if (read_index >= fifo_size)
			read_index -= fifo_size;
	}
	einfo->rx_ch_desc->read_index = read_index;

	return orig_len - len;
}

/**
 * fifo_write_body() - Copy transmit data into an edge
 * @einfo:		The concerned edge to copy into.
 * @_data:		Buffer of data to copy from.
 * @len:		Size of data to copy in bytes.
 * @write_index:	Index into the channel where the data should be copied.
 *
 * Return: Number of bytes remaining to be copied into the edge.
 */
static int fifo_write_body(struct edge_info *einfo, const void *_data,
				int len, uint32_t *write_index)
{
	void *ptr;
	void *ret;
	const void *data = _data;
	uint32_t read_index = einfo->tx_ch_desc->read_index;
	uint32_t fifo_size = einfo->tx_fifo_size;
	uint32_t n;

	if (read_index >= fifo_size || *write_index >= fifo_size) {
		WARN_ON_ONCE(1);
		return -EINVAL;
	}
	while (len) {
		ptr = einfo->tx_fifo + *write_index;
		if (*write_index < read_index) {
			n = read_index - *write_index - FIFO_FULL_RESERVE;
		} else {
			if (read_index < FIFO_FULL_RESERVE)
				n = fifo_size + read_index - *write_index -
							FIFO_FULL_RESERVE;
			else
				n = fifo_size - *write_index;
		}

		if (n == 0)
			break;
		if (n > len)
			n = len;

		ret = einfo->write_to_fifo(ptr, data, n);
		if (IS_ERR(ret))
			return PTR_ERR(ret);

		data += n;
		len -= n;
		*write_index += n;
		if (*write_index >= fifo_size)
			*write_index -= fifo_size;
	}
	return len;
}

/**
 * fifo_write() - Write data into an edge
 * @einfo:	The concerned edge to write to.
 * @data:	Buffer of data to write.
 * @len:	Length of data to write, in bytes.
 *
 * Wrapper around fifo_write_body() to manage additional details that are
 * necessary for a complete write event.  Does not manage concurrency.  Clients
 * should use fifo_write_avail() to check if there is sufficent space before
 * calling fifo_write().
 *
 * Return: Number of bytes written to the edge.
 */
static int fifo_write(struct edge_info *einfo, const void *data, int len)
{
	int orig_len = len;
	uint32_t write_index = einfo->tx_ch_desc->write_index;

	len = fifo_write_body(einfo, data, len, &write_index);
	if (unlikely(len < 0))
		return len;

	/* All data writes need to be flushed to memory before the write index
	 * is updated. This protects against a race condition where the remote
	 * reads stale data because the write index was written before the data.
	 */
	wmb();
	einfo->tx_ch_desc->write_index = write_index;
	send_irq(einfo);

	return orig_len - len;
}

/**
 * fifo_write_complex() - writes a transaction of multiple buffers to an edge
 * @einfo:	The concerned edge to write to.
 * @data1:	The first buffer of data to write.
 * @len1:	The length of the first buffer in bytes.
 * @data2:	The second buffer of data to write.
 * @len2:	The length of the second buffer in bytes.
 * @data3:	The thirs buffer of data to write.
 * @len3:	The length of the third buffer in bytes.
 *
 * A variant of fifo_write() which optimizes the usecase found in tx().  The
 * remote side expects all or none of the transmitted data to be available.
 * This prevents the tx() usecase from calling fifo_write() multiple times.  The
 * alternative would be an allocation and additional memcpy to create a buffer
 * to copy all the data segments into one location before calling fifo_write().
 *
 * Return: Number of bytes written to the edge.
 */
static int fifo_write_complex(struct edge_info *einfo,
			      const void *data1, int len1,
			      const void *data2, int len2,
			      const void *data3, int len3)
{
	int orig_len = len1 + len2 + len3;
	uint32_t write_index = einfo->tx_ch_desc->write_index;

	len1 = fifo_write_body(einfo, data1, len1, &write_index);
	if (unlikely(len1 < 0))
		return len1;
	len2 = fifo_write_body(einfo, data2, len2, &write_index);
	if (unlikely(len2 < 0))
		return len2;
	len3 = fifo_write_body(einfo, data3, len3, &write_index);
	if (unlikely(len3 < 0))
		return len3;

	/* All data writes need to be flushed to memory before the write index
	 * is updated. This protects against a race condition where the remote
	 * reads stale data because the write index was written before the data.
	 */
	wmb();
	einfo->tx_ch_desc->write_index = write_index;
	send_irq(einfo);

	return orig_len - len1 - len2 - len3;
}

/**
 * send_tx_blocked_signal() - send the flush command as we are blocked from tx
 * @einfo:	The concerned edge which is blocked.
 *
 * Used to send a signal to the remote side that we have no more space to
 * transmit data and therefore need the remote side to signal us when they have
 * cleared some space by reading some data.  This function relies upon the
 * assumption that fifo_write_avail() will reserve some space so that the flush
 * signal command can always be put into the transmit fifo, even when "everyone"
 * else thinks that the transmit fifo is truely full.  This function assumes
 * that it is called with the write_lock already locked.
 */
static void send_tx_blocked_signal(struct edge_info *einfo)
{
	struct read_notif_request {
		uint16_t cmd;
		uint16_t reserved;
		uint32_t reserved2;
	};
	struct read_notif_request read_notif_req;

	read_notif_req.cmd = READ_NOTIF_CMD;
	read_notif_req.reserved = 0;
	read_notif_req.reserved2 = 0;

	SMEM_IPC_LOG(einfo, __func__, READ_NOTIF_CMD, 0, 0);
	if (!einfo->tx_blocked_signal_sent) {
		einfo->tx_blocked_signal_sent = true;
		fifo_write(einfo, &read_notif_req, sizeof(read_notif_req));
	}
}

/**
 * fifo_tx() - transmit data on an edge
 * @einfo:	The concerned edge to transmit on.
 * @data:	Buffer of data to transmit.
 * @len:	Length of data to transmit in bytes.
 *
 * This helper function is the preferred interface to fifo_write() and should
 * be used in the normal case for transmitting entities.  fifo_tx() will block
 * until there is sufficent room to transmit the requested ammount of data.
 * fifo_tx() will manage any concurrency between multiple transmitters on a
 * channel.
 *
 * Return: Number of bytes transmitted.
 */
static int fifo_tx(struct edge_info *einfo, const void *data, int len)
{
	unsigned long flags;
	int ret;

	DEFINE_WAIT(wait);

	spin_lock_irqsave(&einfo->write_lock, flags);
	while (fifo_write_avail(einfo) < len) {
		send_tx_blocked_signal(einfo);
		prepare_to_wait(&einfo->tx_blocked_queue, &wait,
							TASK_UNINTERRUPTIBLE);
		if (fifo_write_avail(einfo) < len && !einfo->in_ssr) {
			spin_unlock_irqrestore(&einfo->write_lock, flags);
			schedule();
			spin_lock_irqsave(&einfo->write_lock, flags);
		}
		finish_wait(&einfo->tx_blocked_queue, &wait);
		if (einfo->in_ssr) {
			spin_unlock_irqrestore(&einfo->write_lock, flags);
			return -EFAULT;
		}
	}
	ret = fifo_write(einfo, data, len);
	spin_unlock_irqrestore(&einfo->write_lock, flags);

	return ret;
}

/**
 * process_rx_data() - process received data from an edge
 * @einfo:	The edge the data was received on.
 * @cmd_id:	ID to specify the type of data.
 * @rcid:	The remote channel id associated with the data.
 * @intend_id:	The intent the data should be put in.
 */
static void process_rx_data(struct edge_info *einfo, uint16_t cmd_id,
			    uint32_t rcid, uint32_t intent_id)
{
	struct command {
		uint32_t frag_size;
		uint32_t size_remaining;
	};
	struct command cmd;
	struct glink_core_rx_intent *intent;
	char trash[FIFO_ALIGNMENT];
	int alignment;
	bool err = false;

	fifo_read(einfo, &cmd, sizeof(cmd));

	intent = einfo->xprt_if.glink_core_if_ptr->rx_get_pkt_ctx(
					&einfo->xprt_if, rcid, intent_id);
	if (intent == NULL) {
		GLINK_ERR("%s: no intent for ch %d liid %d\n", __func__, rcid,
								intent_id);
		err = true;
	} else if (intent->data == NULL) {
		if (einfo->intentless) {
			intent->data = kmalloc(cmd.frag_size,
						__GFP_ATOMIC | __GFP_HIGH);
			if (!intent->data) {
				err = true;
				GLINK_ERR(
				"%s: atomic alloc fail ch %d liid %d size %d\n",
						__func__, rcid, intent_id,
						cmd.frag_size);
			} else {
				intent->intent_size = cmd.frag_size;
			}
		} else {
			GLINK_ERR(
				"%s: intent for ch %d liid %d has no data buff\n",
						__func__, rcid, intent_id);
			err = true;
		}
	}

	if (!err &&
	    (intent->intent_size - intent->write_offset < cmd.frag_size ||
	    intent->write_offset + cmd.size_remaining > intent->intent_size)) {
		GLINK_ERR("%s: rx data size:%d and remaining:%d %s %d %s:%d\n",
							__func__,
							cmd.frag_size,
							cmd.size_remaining,
							"will overflow ch",
							rcid,
							"intent",
							intent_id);
		err = true;
	}

	if (err) {
		alignment = ALIGN(cmd.frag_size, FIFO_ALIGNMENT);
		alignment -= cmd.frag_size;
		while (cmd.frag_size) {
			if (cmd.frag_size > FIFO_ALIGNMENT) {
				fifo_read(einfo, trash, FIFO_ALIGNMENT);
				cmd.frag_size -= FIFO_ALIGNMENT;
			} else {
				fifo_read(einfo, trash, cmd.frag_size);
				cmd.frag_size = 0;
			}
		}
		if (alignment)
			fifo_read(einfo, trash, alignment);
		return;
	}
	fifo_read(einfo, intent->data + intent->write_offset, cmd.frag_size);
	intent->write_offset += cmd.frag_size;
	intent->pkt_size += cmd.frag_size;

	alignment = ALIGN(cmd.frag_size, FIFO_ALIGNMENT);
	alignment -= cmd.frag_size;
	if (alignment)
		fifo_read(einfo, trash, alignment);

	if (unlikely((cmd_id == TRACER_PKT_CMD ||
		      cmd_id == TRACER_PKT_CONT_CMD) && !cmd.size_remaining)) {
		tracer_pkt_log_event(intent->data, GLINK_XPRT_RX);
		intent->tracer_pkt = true;
	}

	einfo->xprt_if.glink_core_if_ptr->rx_put_pkt_ctx(&einfo->xprt_if,
							rcid,
							intent,
							cmd.size_remaining ?
								false : true);
}

/**
 * queue_cmd() - queue a deferred command for later processing
 * @einfo:	Edge to queue commands on.
 * @cmd:	Command to queue.
 * @data:	Command specific data to queue with the command.
 *
 * Return: True if queuing was successful, false otherwise.
 */
static bool queue_cmd(struct edge_info *einfo, void *cmd, void *data)
{
	struct command {
		uint16_t id;
		uint16_t param1;
		uint32_t param2;
	};
	struct command *_cmd = cmd;
	struct deferred_cmd *d_cmd;

	d_cmd = kmalloc(sizeof(*d_cmd), GFP_ATOMIC);
	if (!d_cmd) {
		GLINK_ERR("%s: Discarding cmd %d\n", __func__, _cmd->id);
		return false;
	}
	d_cmd->id = _cmd->id;
	d_cmd->param1 = _cmd->param1;
	d_cmd->param2 = _cmd->param2;
	d_cmd->data = data;
	list_add_tail(&d_cmd->list_node, &einfo->deferred_cmds);
	einfo->deferred_cmds_cnt++;
	kthread_queue_work(&einfo->kworker, &einfo->kwork);
	return true;
}

/**
 * get_rx_fifo() - Find the rx fifo for an edge
 * @einfo:	Edge to find the fifo for.
 *
 * Return: True if fifo was found, false otherwise.
 */
static bool get_rx_fifo(struct edge_info *einfo)
{
	if (einfo->mailbox) {
		einfo->rx_fifo = &einfo->mailbox->fifo[einfo->mailbox->tx_size];
		einfo->rx_fifo_size = einfo->mailbox->rx_size;
	} else {
		einfo->rx_fifo = smem_get_entry(SMEM_GLINK_NATIVE_XPRT_FIFO_1,
							&einfo->rx_fifo_size,
							einfo->remote_proc_id,
							SMEM_ITEM_CACHED_FLAG);
		if (!einfo->rx_fifo)
			einfo->rx_fifo = smem_get_entry(
						SMEM_GLINK_NATIVE_XPRT_FIFO_1,
							&einfo->rx_fifo_size,
							einfo->remote_proc_id,
							0);
		if (!einfo->rx_fifo)
			return false;
	}

	return true;
}

/**
 * tx_wakeup_worker() - worker function to wakeup tx blocked thread
 * @work:	kwork associated with the edge to process commands on.
 */
static void tx_wakeup_worker(struct edge_info *einfo)
{
	struct glink_transport_if xprt_if = einfo->xprt_if;
	bool trigger_wakeup = false;
	bool trigger_resume = false;
	unsigned long flags;

	if (einfo->in_ssr)
		return;

	spin_lock_irqsave(&einfo->write_lock, flags);
	if (fifo_write_avail(einfo)) {
		if (einfo->tx_blocked_signal_sent)
			einfo->tx_blocked_signal_sent = false;
		if (einfo->tx_resume_needed) {
			einfo->tx_resume_needed = false;
			trigger_resume = true;
		}
		if (waitqueue_active(&einfo->tx_blocked_queue))/* tx waiting ?*/
			trigger_wakeup = true;
	}
	spin_unlock_irqrestore(&einfo->write_lock, flags);
	if (trigger_wakeup)
		wake_up_all(&einfo->tx_blocked_queue);
	if (trigger_resume)
		xprt_if.glink_core_if_ptr->tx_resume(&xprt_if);
}

/**
 * __rx_worker() - process received commands on a specific edge
 * @einfo:	Edge to process commands on.
 * @atomic_ctx:	Indicates if the caller is in atomic context and requires any
 *		non-atomic operations to be deferred.
 */
static void __rx_worker(struct edge_info *einfo, bool atomic_ctx)
{
	struct command {
		uint16_t id;
		uint16_t param1;
		uint32_t param2;
	};
	struct intent_desc {
		uint32_t size;
		uint32_t id;
	};
	struct command cmd;
	struct intent_desc intent;
	struct intent_desc *intents;
	int i;
	bool granted;
	unsigned long flags;
	int rcu_id;
	uint16_t rcid;
	uint32_t name_len;
	uint32_t len;
	char *name;
	char trash[FIFO_ALIGNMENT];
	struct deferred_cmd *d_cmd;
	void *cmd_data;
	bool ret = false;

	rcu_id = srcu_read_lock(&einfo->use_ref);

	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	spin_lock_irqsave(&einfo->rx_lock, flags);
	if (!einfo->rx_fifo) {
		ret = get_rx_fifo(einfo);
		if (!ret) {
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			srcu_read_unlock(&einfo->use_ref, rcu_id);
			return;
		}
	}
	spin_unlock_irqrestore(&einfo->rx_lock, flags);
	if (ret)
		einfo->xprt_if.glink_core_if_ptr->link_up(&einfo->xprt_if);

	if ((atomic_ctx) && ((einfo->tx_resume_needed)
	    || (einfo->tx_blocked_signal_sent)
	    || (waitqueue_active(&einfo->tx_blocked_queue)))) /* tx waiting ?*/
		tx_wakeup_worker(einfo);

	/*
	 * Access to the fifo needs to be synchronized, however only the calls
	 * into the core from process_rx_data() are compatible with an atomic
	 * processing context.  For everything else, we need to do all the fifo
	 * processing, then unlock the lock for the call into the core.  Data
	 * in the fifo is allowed to be processed immediately instead of being
	 * ordered with the commands because the channel open process prevents
	 * intents from being queued (which prevents data from being sent) until
	 * all the channel open commands are processed by the core, thus
	 * eliminating a race.
	 */
	spin_lock_irqsave(&einfo->rx_lock, flags);
	while (fifo_read_avail(einfo) ||
			(!atomic_ctx && !list_empty(&einfo->deferred_cmds))) {
		if (einfo->in_ssr)
			break;

		if (atomic_ctx && !einfo->intentless &&
		    einfo->deferred_cmds_cnt >= DEFERRED_CMDS_THRESHOLD)
			break;

		if (!atomic_ctx && !list_empty(&einfo->deferred_cmds)) {
			d_cmd = list_first_entry(&einfo->deferred_cmds,
						struct deferred_cmd, list_node);
			list_del(&d_cmd->list_node);
			einfo->deferred_cmds_cnt--;
			cmd.id = d_cmd->id;
			cmd.param1 = d_cmd->param1;
			cmd.param2 = d_cmd->param2;
			cmd_data = d_cmd->data;
			kfree(d_cmd);
			SMEM_IPC_LOG(einfo, "kthread", cmd.id, cmd.param1,
								cmd.param2);
		} else {
			memset(&cmd, 0, sizeof(cmd));
			fifo_read(einfo, &cmd, sizeof(cmd));
			SMEM_IPC_LOG(einfo, "IRQ", cmd.id, cmd.param1,
								cmd.param2);
			cmd_data = NULL;
		}

		switch (cmd.id) {
		case VERSION_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_version(
								&einfo->xprt_if,
								cmd.param1,
								cmd.param2);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case VERSION_ACK_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_version_ack(
								&einfo->xprt_if,
								cmd.param1,
								cmd.param2);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case OPEN_CMD:
			rcid = cmd.param1;
			name_len = cmd.param2;

			if (cmd_data) {
				name = cmd_data;
			} else {
				len = ALIGN(name_len, FIFO_ALIGNMENT);
				name = kmalloc(len, GFP_ATOMIC);
				if (!name) {
					pr_err("No memory available to rx ch open cmd name.  Discarding cmd.\n");
					while (len) {
						fifo_read(einfo, trash,
								FIFO_ALIGNMENT);
						len -= FIFO_ALIGNMENT;
					}
					break;
				}
				fifo_read(einfo, name, len);
			}
			if (atomic_ctx) {
				if (!queue_cmd(einfo, &cmd, name))
					kfree(name);
				break;
			}

			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_remote_open(
								&einfo->xprt_if,
								rcid,
								name,
								SMEM_XPRT_ID);
			kfree(name);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case CLOSE_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->
							rx_cmd_ch_remote_close(
								&einfo->xprt_if,
								cmd.param1);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case OPEN_ACK_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_open_ack(
								&einfo->xprt_if,
								cmd.param1,
								SMEM_XPRT_ID);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case RX_INTENT_CMD:
			/*
			 * One intent listed with this command.  This is the
			 * expected case and can be optimized over the general
			 * case of an array of intents.
			 */
			if (cmd.param2 == 1) {
				if (cmd_data) {
					intent.id = ((struct intent_desc *)
								cmd_data)->id;
					intent.size = ((struct intent_desc *)
								cmd_data)->size;
					kfree(cmd_data);
				} else {
					memset(&intent, 0, sizeof(intent));
					fifo_read(einfo, &intent,
								sizeof(intent));
				}
				if (atomic_ctx) {
					cmd_data = kmalloc(sizeof(intent),
								GFP_ATOMIC);
					if (!cmd_data) {
						GLINK_ERR(
							"%s: dropping cmd %d\n",
							__func__, cmd.id);
						break;
					}
					((struct intent_desc *)cmd_data)->id =
								intent.id;
					((struct intent_desc *)cmd_data)->size =
								intent.size;
					if (!queue_cmd(einfo, &cmd, cmd_data))
						kfree(cmd_data);
					break;
				}
				spin_unlock_irqrestore(&einfo->rx_lock, flags);
				einfo->xprt_if.glink_core_if_ptr->
						rx_cmd_remote_rx_intent_put(
								&einfo->xprt_if,
								cmd.param1,
								intent.id,
								intent.size);
				spin_lock_irqsave(&einfo->rx_lock, flags);
				break;
			}

			/* Array of intents to process */
			if (cmd_data) {
				intents = cmd_data;
			} else {
				intents = kmalloc_array(cmd.param2,
						sizeof(*intents), GFP_ATOMIC);
				if (!intents) {
					for (i = 0; i < cmd.param2; ++i)
						fifo_read(einfo, &intent,
								sizeof(intent));
					break;
				}
				fifo_read(einfo, intents,
					sizeof(*intents) * cmd.param2);
			}
			if (atomic_ctx) {
				if (!queue_cmd(einfo, &cmd, intents))
					kfree(intents);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			for (i = 0; i < cmd.param2; ++i) {
				einfo->xprt_if.glink_core_if_ptr->
					rx_cmd_remote_rx_intent_put(
							&einfo->xprt_if,
							cmd.param1,
							intents[i].id,
							intents[i].size);
			}
			kfree(intents);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case RX_DONE_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_tx_done(
								&einfo->xprt_if,
								cmd.param1,
								cmd.param2,
								false);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case RX_INTENT_REQ_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->
						rx_cmd_remote_rx_intent_req(
								&einfo->xprt_if,
								cmd.param1,
								cmd.param2);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case RX_INTENT_REQ_ACK_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			granted = false;
			if (cmd.param2 == 1)
				granted = true;
			einfo->xprt_if.glink_core_if_ptr->
						rx_cmd_rx_intent_req_ack(
								&einfo->xprt_if,
								cmd.param1,
								granted);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case TX_DATA_CMD:
		case TX_DATA_CONT_CMD:
		case TRACER_PKT_CMD:
		case TRACER_PKT_CONT_CMD:
			process_rx_data(einfo, cmd.id, cmd.param1, cmd.param2);
			break;
		case CLOSE_ACK_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_ch_close_ack(
								&einfo->xprt_if,
								cmd.param1);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case READ_NOTIF_CMD:
			send_irq(einfo);
			break;
		case SIGNALS_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_remote_sigs(
								&einfo->xprt_if,
								cmd.param1,
								cmd.param2);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		case RX_DONE_W_REUSE_CMD:
			if (atomic_ctx) {
				queue_cmd(einfo, &cmd, NULL);
				break;
			}
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			einfo->xprt_if.glink_core_if_ptr->rx_cmd_tx_done(
								&einfo->xprt_if,
								cmd.param1,
								cmd.param2,
								true);
			spin_lock_irqsave(&einfo->rx_lock, flags);
			break;
		default:
			pr_err("Unrecognized command: %d\n", cmd.id);
			break;
		}
	}
	spin_unlock_irqrestore(&einfo->rx_lock, flags);
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * rx_worker() - worker function to process received commands
 * @work:	kwork associated with the edge to process commands on.
 */
static void rx_worker(struct kthread_work *work)
{
	struct edge_info *einfo;

	einfo = container_of(work, struct edge_info, kwork);
	__rx_worker(einfo, false);
}

irqreturn_t irq_handler(int irq, void *priv)
{
	struct edge_info *einfo = (struct edge_info *)priv;

	if (einfo->rx_reset_reg)
		writel_relaxed(einfo->out_irq_mask, einfo->rx_reset_reg);

	__rx_worker(einfo, true);
	einfo->rx_irq_count++;

	return IRQ_HANDLED;
}

/**
 * tx_cmd_version() - convert a version cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @version:	The version number to encode.
 * @features:	The features information to encode.
 */
static void tx_cmd_version(struct glink_transport_if *if_ptr, uint32_t version,
			   uint32_t features)
{
	struct command {
		uint16_t id;
		uint16_t version;
		uint32_t features;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = VERSION_CMD;
	cmd.version = version;
	cmd.features = features;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.version, cmd.features);
	fifo_tx(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * tx_cmd_version_ack() - convert a version ack cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @version:	The version number to encode.
 * @features:	The features information to encode.
 */
static void tx_cmd_version_ack(struct glink_transport_if *if_ptr,
			       uint32_t version,
			       uint32_t features)
{
	struct command {
		uint16_t id;
		uint16_t version;
		uint32_t features;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = VERSION_ACK_CMD;
	cmd.version = version;
	cmd.features = features;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.version, cmd.features);
	fifo_tx(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * set_version() - activate a negotiated version and feature set
 * @if_ptr:	The transport to configure.
 * @version:	The version to use.
 * @features:	The features to use.
 *
 * Return: The supported capabilities of the transport.
 */
static uint32_t set_version(struct glink_transport_if *if_ptr, uint32_t version,
			uint32_t features)
{
	struct edge_info *einfo;
	uint32_t ret;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return 0;
	}

	ret = einfo->intentless ?
				GCAP_INTENTLESS | GCAP_SIGNALS : GCAP_SIGNALS;

	if (features & TRACER_PKT_FEATURE)
		ret |= GCAP_TRACER_PKT;

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return ret;
}

/**
 * tx_cmd_ch_open() - convert a channel open cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @name:	The channel name to encode.
 * @req_xprt:	The transport the core would like to migrate this channel to.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_ch_open(struct glink_transport_if *if_ptr, uint32_t lcid,
			  const char *name, uint16_t req_xprt)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t length;
	};
	struct command cmd;
	struct edge_info *einfo;
	uint32_t buf_size;
	void *buf;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = OPEN_CMD;
	cmd.lcid = lcid;
	cmd.length = strlen(name) + 1;

	buf_size = ALIGN(sizeof(cmd) + cmd.length, FIFO_ALIGNMENT);

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf) {
		GLINK_ERR("%s: malloc fail for %d size buf\n",
				__func__, buf_size);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -ENOMEM;
	}

	memcpy(buf, &cmd, sizeof(cmd));
	memcpy(buf + sizeof(cmd), name, cmd.length);

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.length);
	fifo_tx(einfo, buf, buf_size);

	kfree(buf);

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_ch_close() - convert a channel close cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_ch_close(struct glink_transport_if *if_ptr, uint32_t lcid)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = CLOSE_CMD;
	cmd.lcid = lcid;
	cmd.reserved = 0;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.reserved);
	fifo_tx(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_ch_remote_open_ack() - convert a channel open ack cmd to wire format
 *				 and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 * @xprt_resp:	The response to a transport migration request.
 */
static void tx_cmd_ch_remote_open_ack(struct glink_transport_if *if_ptr,
				     uint32_t rcid, uint16_t xprt_resp)
{
	struct command {
		uint16_t id;
		uint16_t rcid;
		uint32_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = OPEN_ACK_CMD;
	cmd.rcid = rcid;
	cmd.reserved = 0;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.rcid, cmd.reserved);
	fifo_tx(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * tx_cmd_ch_remote_close_ack() - convert a channel close ack cmd to wire format
 *				  and transmit
 * @if_ptr:	The transport to transmit on.
 * @rcid:	The remote channel id to encode.
 */
static void tx_cmd_ch_remote_close_ack(struct glink_transport_if *if_ptr,
				       uint32_t rcid)
{
	struct command {
		uint16_t id;
		uint16_t rcid;
		uint32_t reserved;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = CLOSE_ACK_CMD;
	cmd.rcid = rcid;
	cmd.reserved = 0;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.rcid, cmd.reserved);
	fifo_tx(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * subsys_up() - process a subsystem up notification
 * @if_ptr:	The transport which is up
 *
 */
static void subsys_up(struct glink_transport_if *if_ptr)
{
	struct edge_info *einfo;
	unsigned long flags;
	bool ret = false;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	einfo->in_ssr = false;
	spin_lock_irqsave(&einfo->rx_lock, flags);
	if (!einfo->rx_fifo) {
		ret = get_rx_fifo(einfo);
		if (!ret) {
			spin_unlock_irqrestore(&einfo->rx_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&einfo->rx_lock, flags);
	if (ret)
		einfo->xprt_if.glink_core_if_ptr->link_up(&einfo->xprt_if);
}

/**
 * ssr() - process a subsystem restart notification of a transport
 * @if_ptr:	The transport to restart
 *
 * Return: 0 on success or standard Linux error code.
 */
static int ssr(struct glink_transport_if *if_ptr)
{
	struct edge_info *einfo;
	struct deferred_cmd *cmd;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	BUG_ON(einfo->remote_proc_id == SMEM_RPM);

	einfo->in_ssr = true;
	wake_up_all(&einfo->tx_blocked_queue);

	synchronize_srcu(&einfo->use_ref);

	while (!list_empty(&einfo->deferred_cmds)) {
		cmd = list_first_entry(&einfo->deferred_cmds,
						struct deferred_cmd, list_node);
		list_del(&cmd->list_node);
		kfree(cmd->data);
		kfree(cmd);
	}

	einfo->tx_resume_needed = false;
	einfo->tx_blocked_signal_sent = false;
	einfo->rx_fifo = NULL;
	einfo->rx_fifo_size = 0;
	einfo->tx_ch_desc->write_index = 0;
	einfo->rx_ch_desc->read_index = 0;
	einfo->xprt_if.glink_core_if_ptr->link_down(&einfo->xprt_if);

	return 0;
}

/**
 * int wait_link_down() - Check status of read/write indices
 * @if_ptr:	The transport to check
 *
 * Return: 1 if indices are all zero, 0 otherwise
 */
int wait_link_down(struct glink_transport_if *if_ptr)
{
	struct edge_info *einfo;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	if (einfo->tx_ch_desc->write_index == 0 &&
		einfo->tx_ch_desc->read_index == 0 &&
		einfo->rx_ch_desc->write_index == 0 &&
		einfo->rx_ch_desc->read_index == 0)
		return 1;
	else
		return 0;
}

/**
 * allocate_rx_intent() - allocate/reserve space for RX Intent
 * @if_ptr:	The transport the intent is associated with.
 * @size:	size of intent.
 * @intent:	Pointer to the intent structure.
 *
 * Assign "data" with the buffer created, since the transport creates
 * a linear buffer and "iovec" with the "intent" itself, so that
 * the data can be passed to a client that receives only vector buffer.
 * Note that returning NULL for the pointer is valid (it means that space has
 * been reserved, but the actual pointer will be provided later).
 *
 * Return: 0 on success or standard Linux error code.
 */
static int allocate_rx_intent(struct glink_transport_if *if_ptr, size_t size,
			      struct glink_core_rx_intent *intent)
{
	void *t;

	t = kmalloc(size, GFP_KERNEL);
	if (!t)
		return -ENOMEM;

	intent->data = t;
	intent->iovec = (void *)intent;
	intent->vprovider = rx_linear_vbuf_provider;
	intent->pprovider = NULL;
	return 0;
}

/**
 * deallocate_rx_intent() - Deallocate space created for RX Intent
 * @if_ptr:	The transport the intent is associated with.
 * @intent:	Pointer to the intent structure.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int deallocate_rx_intent(struct glink_transport_if *if_ptr,
				struct glink_core_rx_intent *intent)
{
	if (!intent || !intent->data)
		return -EINVAL;

	kfree(intent->data);
	intent->data = NULL;
	intent->iovec = NULL;
	intent->vprovider = NULL;
	return 0;
}

/**
 * tx_cmd_local_rx_intent() - convert an rx intent cmd to wire format and
 *			      transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @size:	The intent size to encode.
 * @liid:	The local intent id to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_local_rx_intent(struct glink_transport_if *if_ptr,
				  uint32_t lcid, size_t size, uint32_t liid)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t count;
		uint32_t size;
		uint32_t liid;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	if (size > UINT_MAX) {
		pr_err("%s: size %zu is too large to encode\n", __func__, size);
		return -EMSGSIZE;
	}

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	if (einfo->intentless)
		return -EOPNOTSUPP;

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = RX_INTENT_CMD;
	cmd.lcid = lcid;
	cmd.count = 1;
	cmd.size = size;
	cmd.liid = liid;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.count);
	fifo_tx(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_local_rx_done() - convert an rx done cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @liid:	The local intent id to encode.
 * @reuse:	Reuse the consumed intent.
 */
static void tx_cmd_local_rx_done(struct glink_transport_if *if_ptr,
				 uint32_t lcid, uint32_t liid, bool reuse)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t liid;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	if (einfo->intentless)
		return;

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return;
	}

	cmd.id = reuse ? RX_DONE_W_REUSE_CMD : RX_DONE_CMD;
	cmd.lcid = lcid;
	cmd.liid = liid;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.liid);
	fifo_tx(einfo, &cmd, sizeof(cmd));
	srcu_read_unlock(&einfo->use_ref, rcu_id);
}

/**
 * tx_cmd_rx_intent_req() - convert an rx intent request cmd to wire format and
 *			    transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @size:	The requested intent size to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_rx_intent_req(struct glink_transport_if *if_ptr,
				uint32_t lcid, size_t size)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t size;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	if (size > UINT_MAX) {
		pr_err("%s: size %zu is too large to encode\n", __func__, size);
		return -EMSGSIZE;
	}

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	if (einfo->intentless)
		return -EOPNOTSUPP;

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = RX_INTENT_REQ_CMD,
	cmd.lcid = lcid;
	cmd.size = size;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.size);
	fifo_tx(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_rx_intent_req_ack() - convert an rx intent request ack cmd to wire
 *				format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @granted:	The request response to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_remote_rx_intent_req_ack(struct glink_transport_if *if_ptr,
					   uint32_t lcid, bool granted)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t response;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	if (einfo->intentless)
		return -EOPNOTSUPP;

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = RX_INTENT_REQ_ACK_CMD,
	cmd.lcid = lcid;
	if (granted)
		cmd.response = 1;
	else
		cmd.response = 0;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.response);
	fifo_tx(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_cmd_set_sigs() - convert a signals ack cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @sigs:	The signals to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int tx_cmd_set_sigs(struct glink_transport_if *if_ptr, uint32_t lcid,
			   uint32_t sigs)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t sigs;
	};
	struct command cmd;
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	cmd.id = SIGNALS_CMD,
	cmd.lcid = lcid;
	cmd.sigs = sigs;

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.sigs);
	fifo_tx(einfo, &cmd, sizeof(cmd));

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * poll() - poll for data on a channel
 * @if_ptr:	The transport the channel exists on.
 * @lcid:	The local channel id.
 *
 * Return: 0 if no data available, 1 if data available.
 */
static int poll(struct glink_transport_if *if_ptr, uint32_t lcid)
{
	struct edge_info *einfo;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	if (fifo_read_avail(einfo)) {
		__rx_worker(einfo, true);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return 1;
	}

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * mask_rx_irq() - mask the receive irq for a channel
 * @if_ptr:	The transport the channel exists on.
 * @lcid:	The local channel id for the channel.
 * @mask:	True to mask the irq, false to unmask.
 * @pstruct:	Platform defined structure for handling the masking.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int mask_rx_irq(struct glink_transport_if *if_ptr, uint32_t lcid,
		       bool mask, void *pstruct)
{
	struct edge_info *einfo;
	struct irq_chip *irq_chip;
	struct irq_data *irq_data;
	int rcu_id;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	irq_chip = irq_get_chip(einfo->irq_line);
	if (!irq_chip) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -ENODEV;
	}

	irq_data = irq_get_irq_data(einfo->irq_line);
	if (!irq_data) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -ENODEV;
	}

	if (mask) {
		irq_chip->irq_mask(irq_data);
		einfo->irq_disabled = true;
		if (pstruct)
			irq_set_affinity(einfo->irq_line, pstruct);
	} else {
		irq_chip->irq_unmask(irq_data);
		einfo->irq_disabled = false;
	}

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return 0;
}

/**
 * tx_data() - convert a data/tracer_pkt to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @cmd_id:	The command ID to transmit.
 * @lcid:	The local channel id to encode.
 * @pctx:	The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx_data(struct glink_transport_if *if_ptr, uint16_t cmd_id,
		   uint32_t lcid, struct glink_core_tx_pkt *pctx)
{
	struct command {
		uint16_t id;
		uint16_t lcid;
		uint32_t riid;
		uint32_t size;
		uint32_t size_left;
	};
	struct command cmd;
	struct edge_info *einfo;
	uint32_t size;
	uint32_t zeros_size;
	const void *data_start;
	char zeros[FIFO_ALIGNMENT] = { 0 };
	unsigned long flags;
	size_t tx_size = 0;
	int rcu_id;
	int ret;

	if (pctx->size < pctx->size_remaining) {
		GLINK_ERR("%s: size remaining exceeds size.  Resetting.\n",
								__func__);
		pctx->size_remaining = pctx->size;
	}
	if (!pctx->size_remaining)
		return 0;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	rcu_id = srcu_read_lock(&einfo->use_ref);
	if (einfo->in_ssr) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EFAULT;
	}

	if (einfo->intentless &&
	    (pctx->size_remaining != pctx->size || cmd_id == TRACER_PKT_CMD)) {
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EINVAL;
	}

	if (cmd_id == TX_DATA_CMD) {
		if (pctx->size_remaining == pctx->size)
			cmd.id = TX_DATA_CMD;
		else
			cmd.id = TX_DATA_CONT_CMD;
	} else {
		if (pctx->size_remaining == pctx->size)
			cmd.id = TRACER_PKT_CMD;
		else
			cmd.id = TRACER_PKT_CONT_CMD;
	}
	cmd.lcid = lcid;
	cmd.riid = pctx->riid;
	data_start = get_tx_vaddr(pctx, pctx->size - pctx->size_remaining,
				  &tx_size);
	if (!data_start) {
		GLINK_ERR("%s: invalid data_start\n", __func__);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&einfo->write_lock, flags);
	size = fifo_write_avail(einfo);

	/* Intentless clients expect a complete commit or instant failure */
	if (einfo->intentless && size < sizeof(cmd) + pctx->size) {
		spin_unlock_irqrestore(&einfo->write_lock, flags);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -ENOSPC;
	}

	/* Need enough space to write the command and some data */
	if (size <= sizeof(cmd)) {
		einfo->tx_resume_needed = true;
		send_tx_blocked_signal(einfo);
		spin_unlock_irqrestore(&einfo->write_lock, flags);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return -EAGAIN;
	}
	size -= sizeof(cmd);
	if (size > tx_size)
		size = tx_size;

	cmd.size = size;
	pctx->size_remaining -= size;
	cmd.size_left = pctx->size_remaining;
	zeros_size = ALIGN(size, FIFO_ALIGNMENT) - cmd.size;
	if (cmd.id == TRACER_PKT_CMD)
		tracer_pkt_log_event((void *)(pctx->data), GLINK_XPRT_TX);

	ret = fifo_write_complex(einfo, &cmd, sizeof(cmd), data_start, size,
							zeros, zeros_size);
	if (ret < 0) {
		spin_unlock_irqrestore(&einfo->write_lock, flags);
		srcu_read_unlock(&einfo->use_ref, rcu_id);
		return ret;
	}

	SMEM_IPC_LOG(einfo, __func__, cmd.id, cmd.lcid, cmd.riid);
	GLINK_DBG("%s %s: lcid[%u] riid[%u] cmd[%d], size[%d], size_left[%d]\n",
		"<SMEM>", __func__, cmd.lcid, cmd.riid, cmd.id, cmd.size,
		cmd.size_left);
	spin_unlock_irqrestore(&einfo->write_lock, flags);

	/* Fake tx_done for intentless since its not supported over the wire */
	if (einfo->intentless) {
		spin_lock_irqsave(&einfo->rx_lock, flags);
		cmd.id = RX_DONE_CMD;
		cmd.lcid = pctx->rcid;
		queue_cmd(einfo, &cmd, NULL);
		spin_unlock_irqrestore(&einfo->rx_lock, flags);
	}

	srcu_read_unlock(&einfo->use_ref, rcu_id);
	return cmd.size;
}

/**
 * tx() - convert a data transmit cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @pctx:	The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx(struct glink_transport_if *if_ptr, uint32_t lcid,
	      struct glink_core_tx_pkt *pctx)
{
	return tx_data(if_ptr, TX_DATA_CMD, lcid, pctx);
}

/**
 * tx_cmd_tracer_pkt() - convert a tracer packet cmd to wire format and transmit
 * @if_ptr:	The transport to transmit on.
 * @lcid:	The local channel id to encode.
 * @pctx:	The data to encode.
 *
 * Return: Number of bytes written or standard Linux error code.
 */
static int tx_cmd_tracer_pkt(struct glink_transport_if *if_ptr, uint32_t lcid,
	      struct glink_core_tx_pkt *pctx)
{
	return tx_data(if_ptr, TRACER_PKT_CMD, lcid, pctx);
}

/**
 * get_power_vote_ramp_time() - Get the ramp time required for the power
 *				votes to be applied
 * @if_ptr:	The transport interface on which power voting is requested.
 * @state:	The power state for which ramp time is required.
 *
 * Return: The ramp time specific to the power state, standard error otherwise.
 */
static unsigned long get_power_vote_ramp_time(
				struct glink_transport_if *if_ptr,
				uint32_t state)
{
	struct edge_info *einfo;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);

	if (state >= einfo->num_pw_states || !(einfo->ramp_time_us))
		return (unsigned long)ERR_PTR(-EINVAL);

	return einfo->ramp_time_us[state];
}

/**
 * power_vote() - Update the power votes to meet qos requirement
 * @if_ptr:	The transport interface on which power voting is requested.
 * @state:	The power state for which the voting should be done.
 *
 * Return: 0 on Success, standard error otherwise.
 */
static int power_vote(struct glink_transport_if *if_ptr, uint32_t state)
{
	return 0;
}

/**
 * power_unvote() - Remove the all the power votes
 * @if_ptr:	The transport interface on which power voting is requested.
 *
 * Return: 0 on Success, standard error otherwise.
 */
static int power_unvote(struct glink_transport_if *if_ptr)
{
	return 0;
}

/**
 * rx_rt_vote() - Increment and RX thread RT vote
 * @if_ptr:	The transport interface on which power voting is requested.
 *
 * Return: 0 on Success, standard error otherwise.
 */
static int rx_rt_vote(struct glink_transport_if *if_ptr)
{
	struct edge_info *einfo;
	struct sched_param param = { .sched_priority = 1 };
	int ret = 0;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->rt_vote_lock, flags);
	if (!einfo->rt_votes)
		ret = sched_setscheduler_nocheck(einfo->task, SCHED_FIFO,
							&param);
	einfo->rt_votes++;
	spin_unlock_irqrestore(&einfo->rt_vote_lock, flags);
	return ret;
}

/**
 * rx_rt_unvote() - Remove a RX thread RT vote
 * @if_ptr:	The transport interface on which power voting is requested.
 *
 * Return: 0 on Success, standard error otherwise.
 */
static int rx_rt_unvote(struct glink_transport_if *if_ptr)
{
	struct edge_info *einfo;
	struct sched_param param = { .sched_priority = 0 };
	int ret = 0;
	unsigned long flags;

	einfo = container_of(if_ptr, struct edge_info, xprt_if);
	spin_lock_irqsave(&einfo->rt_vote_lock, flags);
	einfo->rt_votes--;
	if (!einfo->rt_votes)
		ret = sched_setscheduler_nocheck(einfo->task, SCHED_NORMAL,
							&param);
	spin_unlock_irqrestore(&einfo->rt_vote_lock, flags);
	return ret;
}

/**
 * negotiate_features_v1() - determine what features of a version can be used
 * @if_ptr:	The transport for which features are negotiated for.
 * @version:	The version negotiated.
 * @features:	The set of requested features.
 *
 * Return: What set of the requested features can be supported.
 */
static uint32_t negotiate_features_v1(struct glink_transport_if *if_ptr,
				      const struct glink_core_version *version,
				      uint32_t features)
{
	return features & version->features;
}

/**
 * init_xprt_if() - initialize the xprt_if for an edge
 * @einfo:	The edge to initialize.
 */
static void init_xprt_if(struct edge_info *einfo)
{
	einfo->xprt_if.tx_cmd_version = tx_cmd_version;
	einfo->xprt_if.tx_cmd_version_ack = tx_cmd_version_ack;
	einfo->xprt_if.set_version = set_version;
	einfo->xprt_if.tx_cmd_ch_open = tx_cmd_ch_open;
	einfo->xprt_if.tx_cmd_ch_close = tx_cmd_ch_close;
	einfo->xprt_if.tx_cmd_ch_remote_open_ack = tx_cmd_ch_remote_open_ack;
	einfo->xprt_if.tx_cmd_ch_remote_close_ack = tx_cmd_ch_remote_close_ack;
	einfo->xprt_if.ssr = ssr;
	einfo->xprt_if.subsys_up = subsys_up;
	einfo->xprt_if.allocate_rx_intent = allocate_rx_intent;
	einfo->xprt_if.deallocate_rx_intent = deallocate_rx_intent;
	einfo->xprt_if.tx_cmd_local_rx_intent = tx_cmd_local_rx_intent;
	einfo->xprt_if.tx_cmd_local_rx_done = tx_cmd_local_rx_done;
	einfo->xprt_if.tx = tx;
	einfo->xprt_if.tx_cmd_rx_intent_req = tx_cmd_rx_intent_req;
	einfo->xprt_if.tx_cmd_remote_rx_intent_req_ack =
						tx_cmd_remote_rx_intent_req_ack;
	einfo->xprt_if.tx_cmd_set_sigs = tx_cmd_set_sigs;
	einfo->xprt_if.poll = poll;
	einfo->xprt_if.mask_rx_irq = mask_rx_irq;
	einfo->xprt_if.wait_link_down = wait_link_down;
	einfo->xprt_if.tx_cmd_tracer_pkt = tx_cmd_tracer_pkt;
	einfo->xprt_if.get_power_vote_ramp_time = get_power_vote_ramp_time;
	einfo->xprt_if.power_vote = power_vote;
	einfo->xprt_if.power_unvote = power_unvote;
	einfo->xprt_if.rx_rt_vote = rx_rt_vote;
	einfo->xprt_if.rx_rt_unvote = rx_rt_unvote;
}

/**
 * init_xprt_cfg() - initialize the xprt_cfg for an edge
 * @einfo:	The edge to initialize.
 * @name:	The name of the remote side this edge communicates to.
 */
static void init_xprt_cfg(struct edge_info *einfo, const char *name)
{
	einfo->xprt_cfg.name = XPRT_NAME;
	einfo->xprt_cfg.edge = name;
	einfo->xprt_cfg.versions = versions;
	einfo->xprt_cfg.versions_entries = ARRAY_SIZE(versions);
	einfo->xprt_cfg.max_cid = SZ_64K;
	einfo->xprt_cfg.max_iid = SZ_2G;
}

/**
 * parse_qos_dt_params() - Parse the power states from DT
 * @dev:	Reference to the platform device for a specific edge.
 * @einfo:	Edge information for the edge probe function is called.
 *
 * Return: 0 on success, standard error code otherwise.
 */
static int parse_qos_dt_params(struct device_node *node,
				struct edge_info *einfo)
{
	int rc;
	int i;
	char *key;
	uint32_t *arr32;
	uint32_t num_states;

	key = "qcom,ramp-time";
	if (!of_find_property(node, key, &num_states))
		return -ENODEV;

	num_states /= sizeof(uint32_t);

	einfo->num_pw_states = num_states;

	arr32 = kmalloc_array(num_states, sizeof(uint32_t), GFP_KERNEL);
	if (!arr32)
		return -ENOMEM;

	einfo->ramp_time_us = kmalloc_array(num_states, sizeof(unsigned long),
					GFP_KERNEL);
	if (!einfo->ramp_time_us) {
		rc = -ENOMEM;
		goto mem_alloc_fail;
	}

	rc = of_property_read_u32_array(node, key, arr32, num_states);
	if (rc) {
		rc = -ENODEV;
		goto invalid_key;
	}
	for (i = 0; i < num_states; i++)
		einfo->ramp_time_us[i] = arr32[i];

	rc = 0;
	kfree(arr32);
	return rc;

invalid_key:
	kfree(einfo->ramp_time_us);
mem_alloc_fail:
	kfree(arr32);
	return rc;
}

/**
 * subsys_name_to_id() - translate a subsystem name to a processor id
 * @name:	The subsystem name to look up.
 *
 * Return: The processor id corresponding to @name or standard Linux error code.
 */
static int subsys_name_to_id(const char *name)
{
	if (!name)
		return -ENODEV;

	if (!strcmp(name, "apss"))
		return SMEM_APPS;
	if (!strcmp(name, "dsps"))
		return SMEM_DSPS;
	if (!strcmp(name, "lpass"))
		return SMEM_Q6;
	if (!strcmp(name, "mpss"))
		return SMEM_MODEM;
	if (!strcmp(name, "rpm"))
		return SMEM_RPM;
	if (!strcmp(name, "wcnss"))
		return SMEM_WCNSS;
	if (!strcmp(name, "spss"))
		return SMEM_SPSS;
	if (!strcmp(name, "cdsp"))
		return SMEM_CDSP;
	return -ENODEV;
}

static void glink_set_affinity(struct edge_info *einfo, u32 *arr, size_t size)
{
	struct cpumask cpumask;
	pid_t pid;
	int i;

	cpumask_clear(&cpumask);
	for (i = 0; i < size; i++) {
		if (arr[i] < num_possible_cpus())
			cpumask_set_cpu(arr[i], &cpumask);
	}
	if (irq_set_affinity(einfo->irq_line, &cpumask))
		pr_err("%s: Failed to set irq affinity\n", __func__);

	if (sched_setaffinity(einfo->task->pid, &cpumask))
		pr_err("%s: Failed to set rx cpu affinity\n", __func__);

	pid = einfo->xprt_cfg.tx_task->pid;
	if (sched_setaffinity(pid, &cpumask))
		pr_err("%s: Failed to set tx cpu affinity\n", __func__);
}

static int glink_smem_native_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct device_node *phandle_node;
	struct edge_info *einfo;
	int rc, cpu_size;
	char *key;
	const char *subsys_name;
	uint32_t irq_line;
	uint32_t irq_mask;
	struct resource *r;
	u32 *cpu_array;
	char log_name[GLINK_NAME_SIZE*2+7] = {0};

	node = pdev->dev.of_node;

	einfo = kzalloc(sizeof(*einfo), GFP_KERNEL);
	if (!einfo) {
		rc = -ENOMEM;
		goto edge_info_alloc_fail;
	}

	key = "label";
	subsys_name = of_get_property(node, key, NULL);
	if (!subsys_name) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "interrupts";
	irq_line = irq_of_parse_and_map(node, 0);
	if (!irq_line) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "qcom,irq-mask";
	rc = of_property_read_u32(node, key, &irq_mask);
	if (rc) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "irq-reg-base";
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!r) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	if (subsys_name_to_id(subsys_name) == -ENODEV) {
		pr_err("%s: unknown subsystem: %s\n", __func__, subsys_name);
		rc = -ENODEV;
		goto invalid_key;
	}
	einfo->remote_proc_id = subsys_name_to_id(subsys_name);

	init_xprt_cfg(einfo, subsys_name);
	init_xprt_if(einfo);
	spin_lock_init(&einfo->write_lock);
	init_waitqueue_head(&einfo->tx_blocked_queue);
	kthread_init_work(&einfo->kwork, rx_worker);
	kthread_init_worker(&einfo->kworker);
	einfo->read_from_fifo = read_from_fifo;
	einfo->write_to_fifo = write_to_fifo;
	init_srcu_struct(&einfo->use_ref);
	spin_lock_init(&einfo->rx_lock);
	INIT_LIST_HEAD(&einfo->deferred_cmds);
	spin_lock_init(&einfo->rt_vote_lock);
	einfo->rt_votes = 0;

	mutex_lock(&probe_lock);
	if (edge_infos[einfo->remote_proc_id]) {
		pr_err("%s: duplicate subsys %s is not valid\n", __func__,
								subsys_name);
		rc = -ENODEV;
		mutex_unlock(&probe_lock);
		goto invalid_key;
	}
	edge_infos[einfo->remote_proc_id] = einfo;
	mutex_unlock(&probe_lock);

	einfo->out_irq_mask = irq_mask;
	einfo->out_irq_reg = ioremap_nocache(r->start, resource_size(r));
	if (!einfo->out_irq_reg) {
		pr_err("%s: unable to map irq reg\n", __func__);
		rc = -ENOMEM;
		goto ioremap_fail;
	}

	einfo->task = kthread_run(kthread_worker_fn, &einfo->kworker,
						"smem_native_%s", subsys_name);
	if (IS_ERR(einfo->task)) {
		rc = PTR_ERR(einfo->task);
		pr_err("%s: kthread_run failed %d\n", __func__, rc);
		goto kthread_fail;
	}

	einfo->tx_ch_desc = smem_alloc(SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR,
							SMEM_CH_DESC_SIZE,
							einfo->remote_proc_id,
							0);
	if (PTR_ERR(einfo->tx_ch_desc) == -EPROBE_DEFER) {
		rc = -EPROBE_DEFER;
		goto smem_alloc_fail;
	}
	if (!einfo->tx_ch_desc) {
		pr_err("%s: smem alloc of ch descriptor failed\n", __func__);
		rc = -ENOMEM;
		goto smem_alloc_fail;
	}
	einfo->rx_ch_desc = einfo->tx_ch_desc + 1;

	einfo->tx_fifo_size = SZ_16K;
	einfo->tx_fifo = smem_alloc(SMEM_GLINK_NATIVE_XPRT_FIFO_0,
							einfo->tx_fifo_size,
							einfo->remote_proc_id,
							0);
	if (!einfo->tx_fifo) {
		pr_err("%s: smem alloc of tx fifo failed\n", __func__);
		rc = -ENOMEM;
		goto smem_alloc_fail;
	}

	key = "qcom,qos-config";
	phandle_node = of_parse_phandle(node, key, 0);
	if (phandle_node && !(of_get_glink_core_qos_cfg(phandle_node,
							&einfo->xprt_cfg)))
		parse_qos_dt_params(node, einfo);

	rc = glink_core_register_transport(&einfo->xprt_if, &einfo->xprt_cfg);
	if (rc == -EPROBE_DEFER)
		goto reg_xprt_fail;
	if (rc) {
		pr_err("%s: glink core register transport failed: %d\n",
								__func__, rc);
		goto reg_xprt_fail;
	}

	einfo->irq_line = irq_line;
	rc = request_irq(irq_line, irq_handler,
			IRQF_TRIGGER_RISING | IRQF_SHARED,
			node->name, einfo);
	if (rc < 0) {
		pr_err("%s: request_irq on %d failed: %d\n", __func__, irq_line,
									rc);
		goto request_irq_fail;
	}
	einfo->in_ssr = true;
	rc = enable_irq_wake(irq_line);
	if (rc < 0)
		pr_err("%s: enable_irq_wake() failed on %d\n", __func__,
								irq_line);

	key = "cpu-affinity";
	cpu_size = of_property_count_u32_elems(node, key);
	if (cpu_size > 0) {
		cpu_array = kmalloc_array(cpu_size, sizeof(u32), GFP_KERNEL);
		if (!cpu_array) {
			rc = -ENOMEM;
			goto request_irq_fail;
		}
		rc = of_property_read_u32_array(node, key, cpu_array, cpu_size);
		if (!rc)
			glink_set_affinity(einfo, cpu_array, cpu_size);
		kfree(cpu_array);
	}

	einfo->debug_mask = QCOM_GLINK_DEBUG_ENABLE;
	snprintf(log_name, sizeof(log_name), "%s_%s_xprt",
			einfo->xprt_cfg.edge, einfo->xprt_cfg.name);
	if (einfo->debug_mask & QCOM_GLINK_DEBUG_ENABLE)
		einfo->log_ctx =
			ipc_log_context_create(NUM_LOG_PAGES, log_name, 0);
	if (!einfo->log_ctx)
		GLINK_ERR("%s: unable to create log context for [%s:%s]\n",
			__func__, einfo->xprt_cfg.edge,
			einfo->xprt_cfg.name);
	register_debugfs_info(einfo);
	/* fake an interrupt on this edge to see if the remote side is up */
	irq_handler(0, einfo);
	return 0;

request_irq_fail:
	glink_core_unregister_transport(&einfo->xprt_if);
reg_xprt_fail:
smem_alloc_fail:
	kthread_flush_worker(&einfo->kworker);
	kthread_stop(einfo->task);
	einfo->task = NULL;
kthread_fail:
	iounmap(einfo->out_irq_reg);
ioremap_fail:
	mutex_lock(&probe_lock);
	edge_infos[einfo->remote_proc_id] = NULL;
	mutex_unlock(&probe_lock);
invalid_key:
missing_key:
	kfree(einfo);
edge_info_alloc_fail:
	return rc;
}

static int glink_rpm_native_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct edge_info *einfo;
	int rc;
	char *key;
	const char *subsys_name;
	uint32_t irq_line;
	uint32_t irq_mask;
	struct resource *irq_r;
	struct resource *msgram_r;
	void __iomem *msgram;
	char toc[RPM_TOC_SIZE];
	uint32_t *tocp;
	uint32_t num_toc_entries;
	char log_name[GLINK_NAME_SIZE*2+7] = {0};

	node = pdev->dev.of_node;

	einfo = kzalloc(sizeof(*einfo), GFP_KERNEL);
	if (!einfo) {
		rc = -ENOMEM;
		goto edge_info_alloc_fail;
	}

	subsys_name = "rpm";

	key = "interrupts";
	irq_line = irq_of_parse_and_map(node, 0);
	if (!irq_line) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "qcom,irq-mask";
	rc = of_property_read_u32(node, key, &irq_mask);
	if (rc) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "irq-reg-base";
	irq_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!irq_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "msgram";
	msgram_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!msgram_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	if (subsys_name_to_id(subsys_name) == -ENODEV) {
		pr_err("%s: unknown subsystem: %s\n", __func__, subsys_name);
		rc = -ENODEV;
		goto invalid_key;
	}
	einfo->remote_proc_id = subsys_name_to_id(subsys_name);

	init_xprt_cfg(einfo, subsys_name);
	init_xprt_if(einfo);
	spin_lock_init(&einfo->write_lock);
	init_waitqueue_head(&einfo->tx_blocked_queue);
	kthread_init_work(&einfo->kwork, rx_worker);
	kthread_init_worker(&einfo->kworker);
	einfo->intentless = true;
	einfo->read_from_fifo = memcpy32_fromio;
	einfo->write_to_fifo = memcpy32_toio;
	init_srcu_struct(&einfo->use_ref);
	spin_lock_init(&einfo->rx_lock);
	INIT_LIST_HEAD(&einfo->deferred_cmds);

	mutex_lock(&probe_lock);
	if (edge_infos[einfo->remote_proc_id]) {
		pr_err("%s: duplicate subsys %s is not valid\n", __func__,
								subsys_name);
		rc = -ENODEV;
		mutex_unlock(&probe_lock);
		goto invalid_key;
	}
	edge_infos[einfo->remote_proc_id] = einfo;
	mutex_unlock(&probe_lock);

	einfo->out_irq_mask = irq_mask;
	einfo->out_irq_reg = ioremap_nocache(irq_r->start,
							resource_size(irq_r));
	if (!einfo->out_irq_reg) {
		pr_err("%s: unable to map irq reg\n", __func__);
		rc = -ENOMEM;
		goto irq_ioremap_fail;
	}

	msgram = ioremap_nocache(msgram_r->start, resource_size(msgram_r));
	if (!msgram) {
		pr_err("%s: unable to map msgram\n", __func__);
		rc = -ENOMEM;
		goto msgram_ioremap_fail;
	}

	einfo->task = kthread_run(kthread_worker_fn, &einfo->kworker,
						"smem_native_%s", subsys_name);
	if (IS_ERR(einfo->task)) {
		rc = PTR_ERR(einfo->task);
		pr_err("%s: kthread_run failed %d\n", __func__, rc);
		goto kthread_fail;
	}

	memcpy32_fromio(toc, msgram + resource_size(msgram_r) - RPM_TOC_SIZE,
								RPM_TOC_SIZE);
	tocp = (uint32_t *)toc;
	if (*tocp != RPM_TOC_ID) {
		rc = -ENODEV;
		pr_err("%s: TOC id %d is not valid\n", __func__, *tocp);
		goto toc_init_fail;
	}
	++tocp;
	num_toc_entries = *tocp;
	if (num_toc_entries > RPM_MAX_TOC_ENTRIES) {
		rc = -ENODEV;
		pr_err("%s: %d is too many toc entries\n", __func__,
							num_toc_entries);
		goto toc_init_fail;
	}
	++tocp;

	for (rc = 0; rc < num_toc_entries; ++rc) {
		if (*tocp != RPM_TX_FIFO_ID) {
			tocp += 3;
			continue;
		}
		++tocp;
		einfo->tx_ch_desc = msgram + *tocp;
		einfo->tx_fifo = einfo->tx_ch_desc + 1;
		if ((uintptr_t)einfo->tx_fifo >
				(uintptr_t)(msgram + resource_size(msgram_r))) {
			pr_err("%s: invalid tx fifo address\n", __func__);
			einfo->tx_fifo = NULL;
			break;
		}
		++tocp;
		einfo->tx_fifo_size = *tocp;
		if (einfo->tx_fifo_size > resource_size(msgram_r) ||
			(uintptr_t)(einfo->tx_fifo + einfo->tx_fifo_size) >
				(uintptr_t)(msgram + resource_size(msgram_r))) {
			pr_err("%s: invalid tx fifo size\n", __func__);
			einfo->tx_fifo = NULL;
			break;
		}
		break;
	}
	if (!einfo->tx_fifo) {
		rc = -ENODEV;
		pr_err("%s: tx fifo not found\n", __func__);
		goto toc_init_fail;
	}

	tocp = (uint32_t *)toc;
	tocp += 2;
	for (rc = 0; rc < num_toc_entries; ++rc) {
		if (*tocp != RPM_RX_FIFO_ID) {
			tocp += 3;
			continue;
		}
		++tocp;
		einfo->rx_ch_desc = msgram + *tocp;
		einfo->rx_fifo = einfo->rx_ch_desc + 1;
		if ((uintptr_t)einfo->rx_fifo >
				(uintptr_t)(msgram + resource_size(msgram_r))) {
			pr_err("%s: invalid rx fifo address\n", __func__);
			einfo->rx_fifo = NULL;
			break;
		}
		++tocp;
		einfo->rx_fifo_size = *tocp;
		if (einfo->rx_fifo_size > resource_size(msgram_r) ||
			(uintptr_t)(einfo->rx_fifo + einfo->rx_fifo_size) >
				(uintptr_t)(msgram + resource_size(msgram_r))) {
			pr_err("%s: invalid rx fifo size\n", __func__);
			einfo->rx_fifo = NULL;
			break;
		}
		break;
	}
	if (!einfo->rx_fifo) {
		rc = -ENODEV;
		pr_err("%s: rx fifo not found\n", __func__);
		goto toc_init_fail;
	}

	einfo->tx_ch_desc->write_index = 0;
	einfo->rx_ch_desc->read_index = 0;

	rc = glink_core_register_transport(&einfo->xprt_if, &einfo->xprt_cfg);
	if (rc == -EPROBE_DEFER)
		goto reg_xprt_fail;
	if (rc) {
		pr_err("%s: glink core register transport failed: %d\n",
								__func__, rc);
		goto reg_xprt_fail;
	}

	einfo->irq_line = irq_line;
	rc = request_irq(irq_line, irq_handler,
			IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_SHARED,
			node->name, einfo);
	if (rc < 0) {
		pr_err("%s: request_irq on %d failed: %d\n", __func__, irq_line,
									rc);
		goto request_irq_fail;
	}
	rc = enable_irq_wake(irq_line);
	if (rc < 0)
		pr_err("%s: enable_irq_wake() failed on %d\n", __func__,
								irq_line);
	einfo->debug_mask = QCOM_GLINK_DEBUG_DISABLE;
	snprintf(log_name, sizeof(log_name), "%s_%s_xprt",
			einfo->xprt_cfg.edge, einfo->xprt_cfg.name);
	if (einfo->debug_mask & QCOM_GLINK_DEBUG_ENABLE)
		einfo->log_ctx =
			ipc_log_context_create(NUM_LOG_PAGES, log_name, 0);
	if (!einfo->log_ctx)
		GLINK_ERR("%s: unable to create log context for [%s:%s]\n",
			__func__, einfo->xprt_cfg.edge,
			einfo->xprt_cfg.name);
	register_debugfs_info(einfo);
	einfo->xprt_if.glink_core_if_ptr->link_up(&einfo->xprt_if);
	return 0;

request_irq_fail:
	glink_core_unregister_transport(&einfo->xprt_if);
reg_xprt_fail:
toc_init_fail:
	kthread_flush_worker(&einfo->kworker);
	kthread_stop(einfo->task);
	einfo->task = NULL;
kthread_fail:
	iounmap(msgram);
msgram_ioremap_fail:
	iounmap(einfo->out_irq_reg);
irq_ioremap_fail:
	mutex_lock(&probe_lock);
	edge_infos[einfo->remote_proc_id] = NULL;
	mutex_unlock(&probe_lock);
invalid_key:
missing_key:
	kfree(einfo);
edge_info_alloc_fail:
	return rc;
}

static int glink_mailbox_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct edge_info *einfo;
	int rc;
	char *key;
	const char *subsys_name;
	uint32_t irq_line;
	uint32_t irq_mask;
	struct resource *irq_r;
	struct resource *mbox_loc_r;
	struct resource *mbox_size_r;
	struct resource *rx_reset_r;
	void *mbox_loc;
	void *mbox_size;
	struct mailbox_config_info *mbox_cfg;
	uint32_t mbox_cfg_size;
	phys_addr_t cfg_p_addr;
	char log_name[GLINK_NAME_SIZE*2+7] = {0};

	node = pdev->dev.of_node;

	einfo = kzalloc(sizeof(*einfo), GFP_KERNEL);
	if (!einfo) {
		rc = -ENOMEM;
		goto edge_info_alloc_fail;
	}

	key = "label";
	subsys_name = of_get_property(node, key, NULL);
	if (!subsys_name) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "interrupts";
	irq_line = irq_of_parse_and_map(node, 0);
	if (!irq_line) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "qcom,irq-mask";
	rc = of_property_read_u32(node, key, &irq_mask);
	if (rc) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "irq-reg-base";
	irq_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!irq_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "mbox-loc-addr";
	mbox_loc_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!mbox_loc_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "mbox-loc-size";
	mbox_size_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!mbox_size_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "irq-rx-reset";
	rx_reset_r = platform_get_resource_byname(pdev, IORESOURCE_MEM, key);
	if (!rx_reset_r) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "qcom,tx-ring-size";
	rc = of_property_read_u32(node, key, &einfo->tx_fifo_size);
	if (rc) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	key = "qcom,rx-ring-size";
	rc = of_property_read_u32(node, key, &einfo->rx_fifo_size);
	if (rc) {
		pr_err("%s: missing key %s\n", __func__, key);
		rc = -ENODEV;
		goto missing_key;
	}

	if (subsys_name_to_id(subsys_name) == -ENODEV) {
		pr_err("%s: unknown subsystem: %s\n", __func__, subsys_name);
		rc = -ENODEV;
		goto invalid_key;
	}
	einfo->remote_proc_id = subsys_name_to_id(subsys_name);

	init_xprt_cfg(einfo, subsys_name);
	einfo->xprt_cfg.name = "mailbox";
	init_xprt_if(einfo);
	spin_lock_init(&einfo->write_lock);
	init_waitqueue_head(&einfo->tx_blocked_queue);
	kthread_init_work(&einfo->kwork, rx_worker);
	kthread_init_worker(&einfo->kworker);
	einfo->read_from_fifo = read_from_fifo;
	einfo->write_to_fifo = write_to_fifo;
	init_srcu_struct(&einfo->use_ref);
	spin_lock_init(&einfo->rx_lock);
	INIT_LIST_HEAD(&einfo->deferred_cmds);

	mutex_lock(&probe_lock);
	if (edge_infos[einfo->remote_proc_id]) {
		pr_err("%s: duplicate subsys %s is not valid\n", __func__,
								subsys_name);
		rc = -ENODEV;
		mutex_unlock(&probe_lock);
		goto invalid_key;
	}
	edge_infos[einfo->remote_proc_id] = einfo;
	mutex_unlock(&probe_lock);

	einfo->out_irq_mask = irq_mask;
	einfo->out_irq_reg = ioremap_nocache(irq_r->start,
							resource_size(irq_r));
	if (!einfo->out_irq_reg) {
		pr_err("%s: unable to map irq reg\n", __func__);
		rc = -ENOMEM;
		goto irq_ioremap_fail;
	}

	mbox_loc = ioremap_nocache(mbox_loc_r->start,
						resource_size(mbox_loc_r));
	if (!mbox_loc) {
		pr_err("%s: unable to map mailbox location reg\n", __func__);
		rc = -ENOMEM;
		goto mbox_loc_ioremap_fail;
	}

	mbox_size = ioremap_nocache(mbox_size_r->start,
						resource_size(mbox_size_r));
	if (!mbox_size) {
		pr_err("%s: unable to map mailbox size reg\n", __func__);
		rc = -ENOMEM;
		goto mbox_size_ioremap_fail;
	}

	einfo->rx_reset_reg = ioremap_nocache(rx_reset_r->start,
						resource_size(rx_reset_r));
	if (!einfo->rx_reset_reg) {
		pr_err("%s: unable to map rx reset reg\n", __func__);
		rc = -ENOMEM;
		goto rx_reset_ioremap_fail;
	}

	einfo->task = kthread_run(kthread_worker_fn, &einfo->kworker,
						"smem_native_%s", subsys_name);
	if (IS_ERR(einfo->task)) {
		rc = PTR_ERR(einfo->task);
		pr_err("%s: kthread_run failed %d\n", __func__, rc);
		goto kthread_fail;
	}

	mbox_cfg_size = sizeof(*mbox_cfg) + einfo->tx_fifo_size +
							einfo->rx_fifo_size;
	mbox_cfg = smem_alloc(SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR,
							mbox_cfg_size,
							einfo->remote_proc_id,
							0);
	if (PTR_ERR(mbox_cfg) == -EPROBE_DEFER) {
		rc = -EPROBE_DEFER;
		goto smem_alloc_fail;
	}
	if (!mbox_cfg) {
		pr_err("%s: smem alloc of mailbox struct failed\n", __func__);
		rc = -ENOMEM;
		goto smem_alloc_fail;
	}
	einfo->mailbox = mbox_cfg;
	einfo->tx_ch_desc = (struct channel_desc *)(&mbox_cfg->tx_read_index);
	einfo->rx_ch_desc = (struct channel_desc *)(&mbox_cfg->rx_read_index);
	mbox_cfg->tx_size = einfo->tx_fifo_size;
	mbox_cfg->rx_size = einfo->rx_fifo_size;
	einfo->tx_fifo = &mbox_cfg->fifo[0];

	rc = glink_core_register_transport(&einfo->xprt_if, &einfo->xprt_cfg);
	if (rc == -EPROBE_DEFER)
		goto reg_xprt_fail;
	if (rc) {
		pr_err("%s: glink core register transport failed: %d\n",
								__func__, rc);
		goto reg_xprt_fail;
	}

	einfo->irq_line = irq_line;
	rc = request_irq(irq_line, irq_handler,
			IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND | IRQF_SHARED,
			node->name, einfo);
	if (rc < 0) {
		pr_err("%s: request_irq on %d failed: %d\n", __func__, irq_line,
									rc);
		goto request_irq_fail;
	}
	rc = enable_irq_wake(irq_line);
	if (rc < 0)
		pr_err("%s: enable_irq_wake() failed on %d\n", __func__,
								irq_line);
	einfo->debug_mask = QCOM_GLINK_DEBUG_DISABLE;
	snprintf(log_name, sizeof(log_name), "%s_%s_xprt",
			einfo->xprt_cfg.edge, einfo->xprt_cfg.name);
	if (einfo->debug_mask & QCOM_GLINK_DEBUG_ENABLE)
		einfo->log_ctx =
			ipc_log_context_create(NUM_LOG_PAGES, log_name, 0);
	if (!einfo->log_ctx)
		GLINK_ERR("%s: unable to create log context for [%s:%s]\n",
			__func__, einfo->xprt_cfg.edge,
			einfo->xprt_cfg.name);
	register_debugfs_info(einfo);

	writel_relaxed(mbox_cfg_size, mbox_size);
	cfg_p_addr = smem_virt_to_phys(mbox_cfg);
	writel_relaxed(lower_32_bits(cfg_p_addr), mbox_loc);
	writel_relaxed(upper_32_bits(cfg_p_addr), mbox_loc + 4);
	einfo->in_ssr = true;
	send_irq(einfo);
	iounmap(mbox_size);
	iounmap(mbox_loc);
	return 0;

request_irq_fail:
	glink_core_unregister_transport(&einfo->xprt_if);
reg_xprt_fail:
smem_alloc_fail:
	kthread_flush_worker(&einfo->kworker);
	kthread_stop(einfo->task);
	einfo->task = NULL;
kthread_fail:
	iounmap(einfo->rx_reset_reg);
rx_reset_ioremap_fail:
	iounmap(mbox_size);
mbox_size_ioremap_fail:
	iounmap(mbox_loc);
mbox_loc_ioremap_fail:
	iounmap(einfo->out_irq_reg);
irq_ioremap_fail:
	mutex_lock(&probe_lock);
	edge_infos[einfo->remote_proc_id] = NULL;
	mutex_unlock(&probe_lock);
invalid_key:
missing_key:
	kfree(einfo);
edge_info_alloc_fail:
	return rc;
}

#if defined(CONFIG_DEBUG_FS)
/**
 * debug_edge() - generates formatted text output displaying current edge state
 * @s:	File to send the output to.
 */
static void debug_edge(struct seq_file *s)
{
	struct edge_info *einfo;
	struct glink_dbgfs_data *dfs_d;

	dfs_d = s->private;
	einfo = dfs_d->priv_data;

/*
 * formatted, human readable edge state output, ie:
 * TX/RX fifo information:
ID|EDGE      |TX READ   |TX WRITE  |TX SIZE   |RX READ   |RX WRITE  |RX SIZE
-------------------------------------------------------------------------------
01|mpss      |0x00000128|0x00000128|0x00000800|0x00000256|0x00000256|0x00001000
 *
 * Interrupt information:
 * EDGE      |TX INT    |RX INT
 * --------------------------------
 * mpss      |0x00000006|0x00000008
 */
	seq_puts(s, "TX/RX fifo information:\n");
	seq_printf(s, "%2s|%-10s|%-10s|%-10s|%-10s|%-10s|%-10s|%-10s\n",
								"ID",
								"EDGE",
								"TX READ",
								"TX WRITE",
								"TX SIZE",
								"RX READ",
								"RX WRITE",
								"RX SIZE");
	seq_puts(s,
		"-------------------------------------------------------------------------------\n");
	if (!einfo)
		return;

	seq_printf(s, "%02i|%-10s|", einfo->remote_proc_id,
					einfo->xprt_cfg.edge);
	if (!einfo->rx_fifo)
		seq_puts(s, "Link Not Up\n");
	else
		seq_printf(s, "0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X\n",
						einfo->tx_ch_desc->read_index,
						einfo->tx_ch_desc->write_index,
						einfo->tx_fifo_size,
						einfo->rx_ch_desc->read_index,
						einfo->rx_ch_desc->write_index,
						einfo->rx_fifo_size);

	seq_puts(s, "\nInterrupt information:\n");
	seq_printf(s, "%-10s|%-10s|%-10s\n", "EDGE", "TX INT", "RX INT");
	seq_puts(s, "--------------------------------\n");
	seq_printf(s, "%-10s|0x%08X|0x%08X\n", einfo->xprt_cfg.edge,
						einfo->tx_irq_count,
						einfo->rx_irq_count);
}

/**
 * register_debugfs_info() - initialize debugfs device entries
 * @einfo:	Pointer to specific edge_info for which register is called.
 */
static void register_debugfs_info(struct edge_info *einfo)
{
	struct glink_dbgfs dfs;
	char *curr_dir_name;
	int dir_name_len;

	dir_name_len = strlen(einfo->xprt_cfg.edge) +
				strlen(einfo->xprt_cfg.name) + 2;
	curr_dir_name = kmalloc(dir_name_len, GFP_KERNEL);
	if (!curr_dir_name) {
		GLINK_ERR("%s: Memory allocation failed\n", __func__);
		return;
	}

	snprintf(curr_dir_name, dir_name_len, "%s_%s",
				einfo->xprt_cfg.edge, einfo->xprt_cfg.name);
	dfs.curr_name = curr_dir_name;
	dfs.par_name = "xprt";
	dfs.b_dir_create = false;
	glink_debugfs_create("XPRT_INFO", debug_edge,
					&dfs, einfo, false);
	kfree(curr_dir_name);
}

#else
static void register_debugfs_info(struct edge_info *einfo)
{
}
#endif /* CONFIG_DEBUG_FS */

static const struct of_device_id smem_match_table[] = {
	{ .compatible = "qcom,glink-smem-native-xprt" },
	{},
};

static struct platform_driver glink_smem_native_driver = {
	.probe = glink_smem_native_probe,
	.driver = {
		.name = "msm_glink_smem_native_xprt",
		.owner = THIS_MODULE,
		.of_match_table = smem_match_table,
	},
};

static const struct of_device_id rpm_match_table[] = {
	{ .compatible = "qcom,glink-rpm-native-xprt" },
	{},
};

static struct platform_driver glink_rpm_native_driver = {
	.probe = glink_rpm_native_probe,
	.driver = {
		.name = "msm_glink_rpm_native_xprt",
		.owner = THIS_MODULE,
		.of_match_table = rpm_match_table,
	},
};

static const struct of_device_id mailbox_match_table[] = {
	{ .compatible = "qcom,glink-mailbox-xprt" },
	{},
};

static struct platform_driver glink_mailbox_driver = {
	.probe = glink_mailbox_probe,
	.driver = {
		.name = "msm_glink_mailbox_xprt",
		.owner = THIS_MODULE,
		.of_match_table = mailbox_match_table,
	},
};

static int __init glink_smem_native_xprt_init(void)
{
	int rc;

	rc = platform_driver_register(&glink_smem_native_driver);
	if (rc) {
		pr_err("%s: glink_smem_native_driver register failed %d\n",
								__func__, rc);
		return rc;
	}

	rc = platform_driver_register(&glink_rpm_native_driver);
	if (rc) {
		pr_err("%s: glink_rpm_native_driver register failed %d\n",
								__func__, rc);
		return rc;
	}

	rc = platform_driver_register(&glink_mailbox_driver);
	if (rc) {
		pr_err("%s: glink_mailbox_driver register failed %d\n",
								__func__, rc);
		return rc;
	}

	return 0;
}
arch_initcall(glink_smem_native_xprt_init);

MODULE_DESCRIPTION("MSM G-Link SMEM Native Transport");
MODULE_LICENSE("GPL v2");
