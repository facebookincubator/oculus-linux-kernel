// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *
 * Common (transport-agnostic) engine for the arfw UART transport. Implements
 * the arfw_driver_ops a binding layer registers with the arfw common framework,
 * bridging inline-only messages over a tty through COBS+CRC32 framing and
 * babel_uart wire-header translation. The binding (e.g. the N_ARFW line
 * discipline) owns the tty and drives this engine via arfw_uart_dev.h.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include <ar_common.h>
#include <arfw_mem_util.h>
#include <arfw_ops.h>
#include <arfw_shim.h>
#include <ar_fw_message.h>

#include "arfw_uart_dev.h"
#include "arfw_uart_framing.h"

#define ARFW_UART_MAX_FRAME_SIZE 2048
#define ARFW_UART_RX_BUF_SIZE 4096
#define ARFW_UART_MAX_QUEUES 8

/*
 * TX drain: tty->ops->write() accepts up to the available room and returns the
 * count taken; loop until the whole frame is queued. Frames are inline-only
 * (<= a few hundred bytes) and the link is low-rate, so the retry path is rare.
 *
 * The retry sleeps run under tx_lock (see arfw_uart_handle_send_queue), so the
 * worst-case budget is also the worst-case time every other queue's TX is
 * blocked. Bound it to ~30ms: at 937500 baud a full ARFW_UART_MAX_FRAME_SIZE
 * (2048B) frame drains in ~22ms, so 30ms covers a full frame plus margin while
 * keeping the sleep-under-lock far below the previous 100ms.
 */
#define ARFW_UART_TX_RETRY_MS 2
#define ARFW_UART_TX_MAX_ATTEMPTS 15

/*
 * On-wire header expected by the peer's babel_uart transport. This layout is
 * duplicated against the firmware-side definition and must stay byte-for-byte
 * identical to it -- a field reorder or width change here silently corrupts
 * every frame on the wire. Do NOT change a field, width, or order without the
 * matching change on the peer.
 * Source of truth (keep in sync):
 *   fbsource arvr/firmware/wearables/libs/babel/babel_protocol.h
 *   (BABEL_HEADER_SIZE == 16)
 *
 * req->data arrives as [ar_firmware_message_header_t][payload]; the arfw header
 * is a library<->driver structure that must NOT be transmitted (see
 * ar_fw_message.h) and is translated to/from this babel header.
 *
 * The per-field offsetof() asserts below lock the exact wire layout: the
 * sizeof() assert alone catches a size change but NOT a field reorder that
 * happens to preserve the total size.
 */
#define ARFW_UART_BABEL_HEADER_SIZE 16
struct arfw_uart_babel_header {
	u16 dst; /* [0]  destination endpoint */
	u16 src; /* [2]  source endpoint */
	u8 tracknum; /* [4]  tracking number */
	u8 seqnum; /* [5]  sequence number */
	u16 msg_id; /* [6]  message id */
	u32 payload_bytes; /* [8]  payload size */
	u8 flags; /* [12] flags / reliability / qos */
	u8 reserved[3]; /* [13] */
} __packed;
static_assert(sizeof(struct arfw_uart_babel_header) ==
	      ARFW_UART_BABEL_HEADER_SIZE);
static_assert(offsetof(struct arfw_uart_babel_header, dst) == 0,
	      "babel header layout drift: dst");
static_assert(offsetof(struct arfw_uart_babel_header, src) == 2,
	      "babel header layout drift: src");
static_assert(offsetof(struct arfw_uart_babel_header, tracknum) == 4,
	      "babel header layout drift: tracknum");
static_assert(offsetof(struct arfw_uart_babel_header, seqnum) == 5,
	      "babel header layout drift: seqnum");
static_assert(offsetof(struct arfw_uart_babel_header, msg_id) == 6,
	      "babel header layout drift: msg_id");
static_assert(offsetof(struct arfw_uart_babel_header, payload_bytes) == 8,
	      "babel header layout drift: payload_bytes");
static_assert(offsetof(struct arfw_uart_babel_header, flags) == 12,
	      "babel header layout drift: flags");

struct arfw_uart_queue {
	struct arfw_uart_driver *drv;
	arfw_client_queue_t client;
	struct arfw_client_queue_create_params params;
	struct arfw_queue_mem_region *data;
	bool active;
	/* FW->HLOS produce cursor (see arfw_uart_rx_work_fn). */
	u32 wr_idx;
	/*
	 * Reserved-but-not-produced slots (ar_queue in_flight). The arfw layer
	 * pre-reserves the whole RX ring at create; produce decrements this and
	 * arfw_uart_handle_rcv_queue_consume replenishes it. Producing at 0 BUGs.
	 */
	u32 reserved;
	/*
	 * True while this FW->HLOS (receive) queue is holding the controller's
	 * runtime autosuspend off (a PM resume reference). A receive-only session has
	 * no host TX to resume the controller, so autosuspend would clock-gate it and
	 * drop the firmware's pushed stream. See handle_queue_create() for why
	 * autosuspend is disabled here rather than relying on wake-on-RX.
	 */
	bool pm_resume_held;
};

struct arfw_uart_driver {
	struct tty_struct *tty;
	struct device *dev;
	const char *dev_id;
	/*
	 * Underlying serial controller device. A runtime-PM-managed UART controller
	 * autosuspends when idle, and a kernel-side tty->ops->write() from a ldisc
	 * does not resume it (unlike a userspace write), so TX would be silently
	 * dropped.
	 * arfw_uart_tty_write() brackets each transfer with runtime-PM get/put so
	 * the port resumes for the transfer and still autosuspends when idle.
	 */
	struct device *uart_dev;
	const struct arfw_client_ops *client_ops;

	struct mutex tx_lock;
	/*
	 * Per-instance TX scratch, used only under tx_lock (see
	 * arfw_uart_handle_send_queue): tx_msg_buf holds [babel_header][payload]
	 * before framing, tx_frame_buf holds the COBS+CRC32 encoded frame. Avoids a
	 * per-message kmalloc/kfree on the send path.
	 */
	u8 tx_msg_buf[ARFW_UART_MAX_FRAME_SIZE];
	u8 tx_frame_buf[ARFW_UART_MAX_FRAME_SIZE];

	spinlock_t rx_lock;
	u8 rx_buf[ARFW_UART_RX_BUF_SIZE];
	size_t rx_pos;

	struct work_struct rx_work;
	u8 rx_work_buf[ARFW_UART_RX_BUF_SIZE];
	/*
	 * Per-instance RX decode scratch. Safe without extra locking: rx_work is
	 * single-threaded per instance (one work_struct, coalesced by schedule_work).
	 */
	u8 rx_decode_buf[ARFW_UART_MAX_FRAME_SIZE];

	/*
	 * Guards the queues[] table. The arfw common framework's queue lifetime
	 * and serial-work locks (see arfw_ops.h) serialize its own async work
	 * against create/destroy, but RX dispatch here runs from a driver-private
	 * work item (rx_work, scheduled from the tty receive path) that the
	 * framework does not know about. Without this lock, arfw_uart_deliver_frame()
	 * scans active/client/params while handle_queue_create()/destroy() mutate
	 * them. All three run in process/work context, so a mutex is sufficient.
	 */
	struct mutex queues_lock;
	struct arfw_uart_queue queues[ARFW_UART_MAX_QUEUES];

	bool registered;
};

/* ---- arfw_driver_ops implementation ---- */

static struct arfw_queue_mem_region *arfw_uart_handle_queue_data_alloc(
	void *base_context,
	const struct arfw_client_queue_create_params *params)
{
	struct arfw_queue_mem_region *region;

	AR_ASSERT(params);

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return NULL;

	region->size = PAGE_ALIGN((size_t)params->element_size * params->depth);
	/*
	 * vmalloc_user (not __get_free_pages): arfw_mem_region_free() frees vmalloc
	 * addrs with vfree(), and the queue mmap needs remap_vmalloc_range() (which
	 * requires the VM_USERMAP vmalloc_user sets). __get_free_pages memory is
	 * neither, and trips a slab BUG in arfw_release() on fd close.
	 */
	region->ptr = vmalloc_user(region->size);
	if (!region->ptr) {
		kfree(region);
		return NULL;
	}

	kref_init(&region->refcnt);
	return region;
}

static void
arfw_uart_handle_queue_data_free(void *base_context,
				 struct arfw_queue_mem_region *region)
{
	AR_ASSERT(region);
	/* region->ptr and region are freed by the arfw common layer via refcnt */
}

static int arfw_uart_handle_queue_create(
	arfw_client_queue_t client, void *base_context,
	const struct arfw_client_queue_create_params *params,
	struct arfw_queue_mem_region *data, void **queue_context)
{
	struct arfw_uart_driver *drv = base_context;
	struct arfw_uart_queue *q;
	int idx = -1;
	int i;

	AR_ASSERT(drv);
	AR_ASSERT(params);

	/*
	 * Find a free slot. Slots are reclaimed on destroy (client set to NULL),
	 * so reuse them rather than monotonically consuming the array. Otherwise
	 * repeated session create/destroy cycles exhaust ARFW_UART_MAX_QUEUES and
	 * every later create fails with -ENOSPC (a queue-slot leak).
	 */
	mutex_lock(&drv->queues_lock);
	for (i = 0; i < ARFW_UART_MAX_QUEUES; i++) {
		if (!drv->queues[i].client) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		mutex_unlock(&drv->queues_lock);
		AR_LOG_UART_ERR(AR_LOG_CREATE_QUEUE, "max queues exceeded");
		return -ENOSPC;
	}

	q = &drv->queues[idx];
	q->drv = drv;
	q->params = *params;
	q->data = data;
	q->active = false;
	q->wr_idx = 0;
	q->reserved = (params->queue_direction == AR_QUEUE_FW_TO_HLOS) ?
				    params->depth :
				    0;
	q->pm_resume_held = false;
	/*
	 * Disable the controller's runtime autosuspend for the lifetime of a receive
	 * (FW->HLOS) queue by holding a PM resume reference (released in
	 * handle_queue_destroy()). A receive-only session never makes the host
	 * transmit, so nothing resumes the geni controller, and a runtime-suspended
	 * controller is clock-gated -> no RX IRQ -> the firmware's pushed stream is
	 * silently dropped. A send (HLOS->FW) queue needs no hold:
	 * arfw_uart_tty_write() resumes per transfer.
	 *
	 * Wake-on-RX is deliberately not used to keep autosuspend on: the HS geni
	 * driver's wake path is a "wakeup byte" scheme that discards all RX until a
	 * magic byte arrives (lossy for a framed stream), the oatmeal UART node does
	 * not wire the wakeup IRQ, and at the display stream's rate (~200 Hz, ~5 ms
	 * between samples, far below the ~150 ms autosuspend delay) the controller
	 * never idles long enough to autosuspend anyway -- so wake-on-RX would add
	 * message loss for ~zero power savings. The hold is taken for every receive
	 * queue; a future per-queue opt-in could avoid pinning request/reply sessions
	 * whose reply arrives right after a resuming TX.
	 */
	if (params->queue_direction == AR_QUEUE_FW_TO_HLOS && drv->uart_dev) {
		int pm_ret = pm_runtime_get_sync(drv->uart_dev);
		/*
		 * get_sync increments the usage count even on error, so keep the hold
		 * (handle_queue_destroy's put balances it) and only log: a failure means
		 * the controller may not stay resumed and the pushed stream can stall.
		 */
		if (pm_ret < 0)
			AR_LOG_UART_DEV_ERR(
				drv->dev, AR_LOG_CREATE_QUEUE,
				"pm_runtime_get_sync failed (%d); receive stream may stall",
				pm_ret);
		q->pm_resume_held = true;
	}
	/* Publish client last: deliver_frame() keys off client being non-NULL. */
	q->client = client;
	mutex_unlock(&drv->queues_lock);

	*queue_context = q;

	AR_LOG_UART_DEV_INFO(
		drv->dev, AR_LOG_CREATE_QUEUE,
		"queue %d created: 0x%x%s0x%x elem=%u depth=%u", idx,
		params->hlos_endpoint_id,
		params->queue_direction == AR_QUEUE_HLOS_TO_FW ? "->" : "<-",
		params->fw_endpoint_id, params->element_size, params->depth);
	return 0;
}

static int arfw_uart_handle_queue_destroy(void *base_context,
					  void *queue_context)
{
	struct arfw_uart_queue *q = queue_context;
	struct arfw_uart_driver *drv;
	bool pm_held;

	AR_ASSERT(q);
	drv = q->drv;
	AR_ASSERT(drv);

	mutex_lock(&drv->queues_lock);
	q->active = false;
	q->client = NULL;
	pm_held = q->pm_resume_held;
	q->pm_resume_held = false;
	mutex_unlock(&drv->queues_lock);

	/* Release the receive-session resume hold taken in handle_queue_create(). */
	if (pm_held && drv->uart_dev) {
		pm_runtime_mark_last_busy(drv->uart_dev);
		pm_runtime_put_autosuspend(drv->uart_dev);
	}
	return 0;
}

/*
 * Drain a full frame into the tty. tty->ops->write() takes up to the available
 * room and returns the count accepted; loop (with a brief backoff) until the
 * whole frame is queued so a frame is never silently truncated under
 * back-pressure.
 */
static int arfw_uart_tty_write(struct arfw_uart_driver *drv, const u8 *buf,
			       size_t len)
{
	struct tty_struct *tty = drv->tty;
	size_t written = 0;
	int attempts = 0;
	int ret = 0;

	if (!tty || !tty->ops || !tty->ops->write)
		return -ENODEV;

	/*
	 * Resume the controller for the transfer, then let it autosuspend again. A
	 * kernel-side tty->ops->write() does not implicitly resume a runtime-suspended
	 * UART controller (unlike a userspace write), so without this the bytes are
	 * dropped while the port is runtime-suspended. put_autosuspend (not put_sync)
	 * keeps the port up through the autosuspend delay -- long enough to drain TX
	 * and to receive the reply -- before idling down, preserving power savings.
	 */
	if (drv->uart_dev)
		pm_runtime_get_sync(drv->uart_dev);

	while (written < len) {
		int n = tty->ops->write(tty, buf + written, len - written);

		if (n < 0) {
			ret = n;
			break;
		}
		if (n == 0) {
			if (++attempts > ARFW_UART_TX_MAX_ATTEMPTS) {
				ret = -ETIMEDOUT;
				break;
			}
			msleep(ARFW_UART_TX_RETRY_MS);
			continue;
		}
		attempts = 0;
		written += n;
	}

	if (drv->uart_dev) {
		pm_runtime_mark_last_busy(drv->uart_dev);
		pm_runtime_put_autosuspend(drv->uart_dev);
	}
	return ret;
}

static bool arfw_uart_handle_send_queue(void *queue_context,
					const struct arfw_io_request *req,
					bool *consumed)
{
	struct arfw_uart_queue *q = queue_context;
	struct arfw_uart_driver *drv;
	const ar_firmware_message_header_t *ah;
	const u8 *payload;
	size_t payload_len, msg_len, frame_len;
	struct arfw_uart_babel_header bh;
	int ret;

	AR_ASSERT(q);
	AR_ASSERT(req);
	AR_ASSERT(consumed);
	*consumed = true;

	drv = q->drv;
	AR_ASSERT(drv);

	/*
	 * req->data is [ar_firmware_message_header_t][payload]; translate the arfw
	 * header to a babel header (see struct arfw_uart_babel_header) and frame it.
	 */
	if (req->data_size < sizeof(ar_firmware_message_header_t)) {
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_WRITE,
				    "send buffer too small: %u",
				    req->data_size);
		return true;
	}

	ah = req->data;
	payload = (const u8 *)req->data + sizeof(*ah);
	payload_len = ah->data_size;
	msg_len = sizeof(bh) + payload_len;

	/*
	 * ah->data_size is attacker/caller-controlled and only bounded by
	 * ARFW_UART_MAX_FRAME_SIZE below, NOT by the actual request buffer.
	 * Reject if the advertised payload runs past req->data (the bytes the
	 * caller actually provided) before the memcpy reads it -- otherwise the
	 * memcpy below reads out of bounds. The earlier req->data_size <
	 * sizeof(ar_firmware_message_header_t) check guarantees
	 * req->data_size - sizeof(*ah) does not underflow. This is the TX-side
	 * twin of the RX over-length frame rejection in arfw_uart_deliver_frame().
	 */
	if (payload_len > req->data_size - sizeof(*ah)) {
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_WRITE,
				    "payload exceeds buffer: %zu > %zu",
				    payload_len, req->data_size - sizeof(*ah));
		return true;
	}

	if (msg_len > ARFW_UART_MAX_FRAME_SIZE) {
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_WRITE,
				    "message too large: %zu", msg_len);
		return true;
	}

	bh = (struct arfw_uart_babel_header){
		.dst = q->params.fw_endpoint_id,
		.src = q->params.hlos_endpoint_id,
		.tracknum = ah->tracking_id,
		.seqnum = ah->sequence_id,
		.msg_id = ah->msg_id,
		.payload_bytes = payload_len,
		.flags = 0,
	};

	/*
	 * tx_msg_buf/tx_frame_buf are shared per-instance scratch; send_queue can run
	 * concurrently across queues (see arfw_driver_ops contract in arfw_device.h),
	 * so serialize the whole build->encode->write under tx_lock. Framing is cheap
	 * and the single UART serializes TX anyway.
	 */
	mutex_lock(&drv->tx_lock);

	memcpy(drv->tx_msg_buf, &bh, sizeof(bh));
	if (payload_len)
		memcpy(drv->tx_msg_buf + sizeof(bh), payload, payload_len);

	ret = arfw_uart_frame_encode(drv->tx_msg_buf, msg_len,
				     drv->tx_frame_buf,
				     ARFW_UART_MAX_FRAME_SIZE, &frame_len);
	if (ret) {
		mutex_unlock(&drv->tx_lock);
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_WRITE,
				    "frame encode failed: %d", ret);
		return true;
	}

	ret = arfw_uart_tty_write(drv, drv->tx_frame_buf, frame_len);
	mutex_unlock(&drv->tx_lock);

	if (ret)
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_WRITE,
				    "tty write failed: %d", ret);
	return true;
}

static int arfw_uart_handle_get_device_information(
	void *base_context, struct arfw_device_information_req *info)
{
	AR_ASSERT(info);

	/*
	 * Queue-slot layout is [ar_firmware_message_header_t][inline payload];
	 * the babel header is wire-only (added/stripped by this transport). The
	 * inline payload therefore begins right after the arfw header.
	 */
	info->transport_header_size = sizeof(ar_firmware_message_header_t);
	info->inline_data_offset = sizeof(ar_firmware_message_header_t);
	info->send_ring_max = ARFW_UART_MAX_QUEUES / 2;
	info->rcv_ring_max = ARFW_UART_MAX_QUEUES / 2;
	info->require_contiguous_memory_for_queues = false;
	info->rcv_ring_pend_buff_count_max = 0;

	return 0;
}

static void arfw_uart_handle_notify_queue_ready(void *queue_context)
{
	struct arfw_uart_queue *q = queue_context;

	AR_ASSERT(q);
	WRITE_ONCE(q->active, true);
}

static void arfw_uart_handle_rcv_queue_consume(arfw_client_queue_t client,
					       void *queue_context)
{
	struct arfw_uart_queue *q = queue_context;
	struct arfw_uart_driver *drv;
	struct arfw_io_request io_req;

	AR_ASSERT(q);
	drv = q->drv;
	if (!drv || !drv->client_ops ||
	    !drv->client_ops->handle_client_queue_reserve_request)
		return;

	/*
	 * Userspace consumed one or more RX slots. Re-reserve every freed slot so
	 * the producer reservation count is replenished; otherwise it drains to
	 * zero (the RX ring is fully pre-reserved at create) and the next
	 * ar_fw_queue_slot_produce() BUGs on in_flight == 0. Mirrors
	 * arfw_usb_queue_recv_consume().
	 */
	while (drv->client_ops->handle_client_queue_reserve_request(
		       client, &io_req) == 0)
		q->reserved++;
}

/*
 * queue_context is a fixed-ABI driver callback slot
 * (arfw_driver_ops.handle_receive_payload_pend); const-qualifying it would
 * change the function-pointer type and break assignment into arfw_uart_ops.
 */
static int arfw_uart_handle_receive_payload_pend(
	// cppcheck-suppress constParameterCallback
	void *queue_context, const struct arfw_payload_pend_req *req)
{
	return -EOPNOTSUPP;
}

static int arfw_uart_handle_queue_data_mmap(void *base_context,
					    struct arfw_queue_mem_region *data,
					    struct vm_area_struct *vma)
{
	AR_ASSERT(data);
	return remap_vmalloc_range(vma, data->ptr, 0);
}

static const struct arfw_driver_ops arfw_uart_ops = {
	.handle_send_queue = arfw_uart_handle_send_queue,
	.handle_rcv_queue_consume = arfw_uart_handle_rcv_queue_consume,
	.handle_queue_data_alloc = arfw_uart_handle_queue_data_alloc,
	.handle_queue_data_free = arfw_uart_handle_queue_data_free,
	.handle_queue_create = arfw_uart_handle_queue_create,
	.handle_queue_destroy = arfw_uart_handle_queue_destroy,
	.handle_receive_payload_pend = arfw_uart_handle_receive_payload_pend,
	.handle_get_device_information =
		arfw_uart_handle_get_device_information,
	.handle_notify_queue_ready = arfw_uart_handle_notify_queue_ready,
	.handle_queue_data_mmap = arfw_uart_handle_queue_data_mmap,
};

/* ---- RX work: frame extraction and dispatch ---- */

/*
 * Deliver one decoded [babel_header][payload] message to the RX queue it is
 * addressed to. A reply's babel dst carries the originating HLOS endpoint, so
 * route to the FW->HLOS queue whose hlos_endpoint_id matches; this keeps
 * concurrent sessions on the same UART separated. The arfw header the client
 * library expects is reconstructed from the wire babel header.
 */
static void arfw_uart_deliver_frame(struct arfw_uart_driver *drv,
				    const u8 *decoded, size_t decoded_len)
{
	const struct arfw_uart_babel_header *bh;
	const u8 *rx_payload;
	size_t rx_payload_len, total;
	struct arfw_uart_queue *q = NULL;
	struct arfw_io_request io_req;
	ar_firmware_message_header_t ah;
	int i, ret;

	if (decoded_len < sizeof(*bh)) {
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_READ,
				    "rx frame too short: %zu", decoded_len);
		return;
	}

	bh = (const struct arfw_uart_babel_header *)decoded;
	rx_payload = decoded + sizeof(*bh);
	rx_payload_len = decoded_len - sizeof(*bh);

	/*
	 * Hold queues_lock for the whole select-then-produce sequence so the
	 * matched slot cannot be torn down by handle_queue_destroy() between the
	 * scan and the produce. This runs in work context and the produce
	 * callback is sleepable, so a mutex is fine.
	 *
	 * No ABBA risk against the framework's client lock (registered_devices.lock,
	 * see arfw_client_lock() in arfw_interface.c). The framework always takes
	 * that lock OUTSIDE queues_lock: arfw_queue_create() runs under it (ioctl
	 * path, arfw_interface.c registered_devices.lock held across
	 * handle_queue_create) and arfw_release() holds it across
	 * arfw_client_queue_destroy()->handle_queue_destroy(). The produce callback
	 * invoked below, handle_client_queue_produce_request()
	 * (arfw_client_queue_produce_request), takes only the per-client lifetime
	 * lock + per-queue serial_work_lock (lock_client_for_work) and the
	 * locked_events spinlock (arfw_queue_event_enqueue) -- never
	 * registered_devices.lock. So nesting produce under queues_lock adds no
	 * client-lock edge and queues_lock is never the outer lock over the client
	 * lock. detach() takes them in the framework order (client lock then
	 * queues_lock); see arfw_uart_dev_detach().
	 */
	mutex_lock(&drv->queues_lock);

	/*
	 * Match on the full (dst, src) endpoint pair, not dst alone. Multiple
	 * sessions are multiplexed over one host endpoint (e.g. every Acropolis
	 * session shares JANUS_ENDPOINT_XROS_HOST_INTERFACE and differs only in
	 * the peer/fw endpoint), so dst alone is ambiguous: it would steer every
	 * inbound frame to the first-registered ring and starve the others. The
	 * babel src carries the peer endpoint and disambiguates them.
	 */
	for (i = 0; i < ARFW_UART_MAX_QUEUES; i++) {
		struct arfw_uart_queue *cand = &drv->queues[i];

		if (cand->active && cand->client &&
		    cand->params.queue_direction == AR_QUEUE_FW_TO_HLOS &&
		    cand->params.hlos_endpoint_id == bh->dst &&
		    cand->params.fw_endpoint_id == bh->src) {
			q = cand;
			break;
		}
	}
	if (!q) {
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_READ,
				    "rx drop: no queue for dst 0x%x src 0x%x",
				    bh->dst, bh->src);
		goto out;
	}

	if (!drv->client_ops ||
	    !drv->client_ops->handle_client_queue_produce_request || !q->data ||
	    !q->data->ptr)
		goto out;

	if (q->reserved == 0) {
		/*
		 * Consumer is behind: the RX ring has no free slot. Drop rather
		 * than produce into an unreserved slot (which BUGs the ring).
		 */
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_READ,
				    "rx drop dst 0x%x: no slot", bh->dst);
		goto out;
	}

	ah = (ar_firmware_message_header_t){
		.msg_id = bh->msg_id,
		.tracking_id = bh->tracknum,
		.sequence_id = bh->seqnum,
		.data_size = rx_payload_len,
		.inline_msg_len = rx_payload_len,
		.data_location = ARFW_BUFFER_LOC_IN_LINE,
	};
	total = sizeof(ah) + rx_payload_len;
	if (total > q->params.element_size) {
		/*
		 * The decoded [header][payload] does not fit the destination
		 * slot. Drop the whole frame: truncating the payload while the
		 * header still advertises the full data_size/inline_msg_len
		 * would make the consumer read past the bytes actually copied.
		 * A truncated message is unusable, so reject rather than lie.
		 */
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_READ,
				    "rx drop dst 0x%x: frame %zu > slot %u",
				    bh->dst, total, q->params.element_size);
		goto out;
	}

	io_req.data =
		(u8 *)q->data->ptr + (size_t)q->params.element_size * q->wr_idx;
	io_req.index = q->wr_idx;
	io_req.data_size = total;
	memcpy(io_req.data, &ah, sizeof(ah));
	memcpy((u8 *)io_req.data + sizeof(ah), rx_payload, rx_payload_len);

	ret = drv->client_ops->handle_client_queue_produce_request(q->client,
								   &io_req);
	if (ret) {
		AR_LOG_UART_DEV_ERR(drv->dev, AR_LOG_READ,
				    "rx produce failed dst 0x%x: %d", bh->dst,
				    ret);
		goto out;
	}
	q->reserved--;
	q->wr_idx = (q->wr_idx + 1) % q->params.depth;

out:
	mutex_unlock(&drv->queues_lock);
}

/* Decode one wire frame (COBS+CRC32) and hand the payload to delivery. */
static void arfw_uart_process_frame(struct arfw_uart_driver *drv,
				    const u8 *frame, size_t frame_len)
{
	size_t decoded_len;
	int ret;

	ret = arfw_uart_frame_decode(frame, frame_len, drv->rx_decode_buf,
				     ARFW_UART_MAX_FRAME_SIZE, &decoded_len);
	if (ret == 0 && decoded_len > 0)
		arfw_uart_deliver_frame(drv, drv->rx_decode_buf, decoded_len);
	else
		AR_LOG_UART_DBG(AR_LOG_READ,
				"frame decode failed: ret=%d len=%zu", ret,
				frame_len);
}

static void arfw_uart_rx_work_fn(struct work_struct *work)
{
	struct arfw_uart_driver *drv =
		container_of(work, struct arfw_uart_driver, rx_work);
	unsigned long flags;
	size_t i, frame_start, len;

	spin_lock_irqsave(&drv->rx_lock, flags);
	len = drv->rx_pos;
	memcpy(drv->rx_work_buf, drv->rx_buf, len);
	drv->rx_pos = 0;
	spin_unlock_irqrestore(&drv->rx_lock, flags);

	frame_start = 0;
	for (i = 0; i < len; i++) {
		if (drv->rx_work_buf[i] != ARFW_UART_FRAME_DELIMITER)
			continue;

		if (i > frame_start)
			arfw_uart_process_frame(drv,
						drv->rx_work_buf + frame_start,
						i - frame_start);
		frame_start = i + 1;
	}

	/* If there's a partial frame at the end, put it back */
	if (frame_start < len) {
		size_t remaining = len - frame_start;

		spin_lock_irqsave(&drv->rx_lock, flags);
		/*
		 * arfw_uart_dev_receive() runs concurrently from the tty
		 * receive path and may have appended bytes (growing rx_pos)
		 * since we drained the buffer above. Re-prepending `remaining`
		 * bytes in front of the current rx_pos can overflow rx_buf, so
		 * bound-check first. On overflow drop the stale partial frame
		 * rather than corrupt memory; the peer retransmits or the next
		 * delimiter resynchronizes the stream.
		 */
		if (remaining + drv->rx_pos > ARFW_UART_RX_BUF_SIZE) {
			AR_LOG_UART_DEV_ERR(
				drv->dev, AR_LOG_READ,
				"rx drop partial %zu: buf full (rx_pos %zu)",
				remaining, (size_t)drv->rx_pos);
		} else {
			memmove(drv->rx_buf + remaining, drv->rx_buf,
				drv->rx_pos);
			memcpy(drv->rx_buf, drv->rx_work_buf + frame_start,
			       remaining);
			drv->rx_pos += remaining;
		}
		spin_unlock_irqrestore(&drv->rx_lock, flags);
	}
}

/* ---- Engine API (consumed by the binding layer) ---- */

struct arfw_uart_driver *arfw_uart_dev_register(struct device *dev,
						const char *dev_id)
{
	struct arfw_uart_driver *drv;
	int ret;

	/* Not devm: see arfw_uart_dev.h -- the instance outlives any bound tty. */
	drv = kzalloc(sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return NULL;

	drv->dev = dev;
	drv->dev_id = dev_id;
	mutex_init(&drv->tx_lock);
	mutex_init(&drv->queues_lock);
	spin_lock_init(&drv->rx_lock);
	INIT_WORK(&drv->rx_work, arfw_uart_rx_work_fn);

	ret = arfw_shim_cdev_register(dev, dev_id, &arfw_uart_ops,
				      &drv->client_ops, drv);
	if (ret) {
		AR_LOG_UART_DEV_ERR(dev, AR_LOG_DEV_REG,
				    "failed to register arfw cdev: %d", ret);
		kfree(drv);
		return NULL;
	}

	drv->registered = true;
	return drv;
}

void arfw_uart_dev_unregister(struct arfw_uart_driver *drv)
{
	if (!drv)
		return;

	if (drv->registered)
		arfw_shim_cdev_unregister(drv->dev_id, NULL, true);
	cancel_work_sync(&drv->rx_work);
	kfree(drv);
}

void arfw_uart_dev_attach(struct arfw_uart_driver *drv, struct tty_struct *tty,
			  struct device *dev, struct device *uart_dev)
{
	unsigned long flags;

	AR_ASSERT(drv);

	mutex_lock(&drv->tx_lock);
	drv->tty = tty;
	drv->dev = dev;
	drv->uart_dev = uart_dev;
	mutex_unlock(&drv->tx_lock);

	spin_lock_irqsave(&drv->rx_lock, flags);
	drv->rx_pos = 0;
	spin_unlock_irqrestore(&drv->rx_lock, flags);

	if (tty)
		tty->receive_room = ARFW_UART_RX_BUF_SIZE;
}

void arfw_uart_dev_detach(struct arfw_uart_driver *drv)
{
	if (!drv)
		return;

	/*
	 * Disconnect -- do NOT tear down. Mirror the PCIe link-down path: tell every
	 * active session the link is gone so userspace shuts the Janus session down
	 * and reconnects when the port comes back. The cdev and driver instance
	 * persist (freed only at module exit), so an in-flight arfw_write() can never
	 * use-after-free.
	 */
	if (drv->client_ops && drv->client_ops->handle_client_lock &&
	    drv->client_ops->handle_client_unlock &&
	    drv->client_ops->handle_client_queue_shutdown) {
		/*
		 * Lock order is client lock -> queues_lock, matching both the
		 * arfw common framework's FD-release/destroy path (arfw_release()
		 * and handle_ioctl_queue_destroy() hold registered_devices.lock --
		 * the client lock -- across handle_queue_destroy(), which takes
		 * queues_lock here) and the PCIe driver precedent
		 * (ar_pci_release_client_resource() takes handle_client_lock()
		 * before its per-driver queues lock). deliver_frame() does not
		 * participate in this ordering: handle_client_queue_produce_request()
		 * takes the per-client serial-work + lifetime locks, never the
		 * client lock, so holding queues_lock across produce adds no
		 * client-lock edge. Taking queues_lock before the client lock here
		 * would invert the framework's order and risk an ABBA deadlock
		 * against a concurrent FD close / destroy ioctl.
		 */
		int i;

		drv->client_ops->handle_client_lock();
		mutex_lock(&drv->queues_lock);
		for (i = 0; i < ARFW_UART_MAX_QUEUES; i++) {
			struct arfw_uart_queue *q = &drv->queues[i];

			if (q->client) {
				drv->client_ops->handle_client_queue_shutdown(
					q->client,
					AR_QUEUE_SHUTDOWN_LINK_STATE);
				q->active = false;
			}
		}
		mutex_unlock(&drv->queues_lock);
		drv->client_ops->handle_client_unlock();
	}

	/* Stop using the tty; sends now drop until the next attach reconnects. */
	mutex_lock(&drv->tx_lock);
	drv->tty = NULL;
	drv->uart_dev = NULL;
	mutex_unlock(&drv->tx_lock);

	cancel_work_sync(&drv->rx_work);
}

size_t arfw_uart_dev_receive(struct arfw_uart_driver *drv,
			     const unsigned char *cp, size_t count)
{
	unsigned long flags;
	size_t space;

	if (!drv || !count)
		return 0;

	spin_lock_irqsave(&drv->rx_lock, flags);
	space = ARFW_UART_RX_BUF_SIZE - drv->rx_pos;
	if (count > space)
		count = space;
	memcpy(drv->rx_buf + drv->rx_pos, cp, count);
	drv->rx_pos += count;
	spin_unlock_irqrestore(&drv->rx_lock, flags);

	schedule_work(&drv->rx_work);
	return count;
}
