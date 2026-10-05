/* SPDX-License-Identifier: GPL+																							*/
/*******************************************************************************
 * @file arfw_usb_int.h
 *
 * @brief internal symbols and functions used for USB protocol implementation
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef ARFW_USB_INT_H
#define ARFW_USB_INT_H

#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/workqueue.h>
#include <linux/xarray.h>

#include <ar_future.h>
#include <arfw_ops.h>

#include "meta_usb_protocol_shared.h"

// This is defined by Janus and USB protocol per device:
//   SYST [EP0      ] is  reserved for USB   control transfers.
//   CTRL [EP1..EP2 ] are reserved for Janus control transfers.
//   DATA [EP3..EP32] are reserved for Janus data    transfers.
#define ARFW_USB_EP_CTRL_NUM (2)
#define ARFW_USB_EP_DATA_NUM (30)
#define ARFW_USB_EP_ALL_NUM (ARFW_USB_EP_CTRL_NUM + ARFW_USB_EP_DATA_NUM)

// When streams are enabled we require at least two for work.
// We assume control messages are on a separate endpoint.
// And require inline/external separation.
// This just specifies number needed, default stream (sid=0) can be included.
#define ARFW_USB_SID_NUM_MIN (2)

// For now we will only be using a single workqueue for all
// offloaded work on all endpoints. This might get changed later with benchmarks.
// We do make a queue per interface, let's name them with interface number included.
#define ARFW_USB_OFFLOAD_QUEUE_PREFIX "arfw_usb_wq_"

// Used to register character devices with.
// Need to fit into ARFW_DEVICE_ID_MAX_LEN with the dynamic subsystem name added.
// That size check is done in runtime, here just sanity checking the bounds.
#define ARFW_USB_INTF_PREFIX "AR-JAZZ-USB-"
static_assert(sizeof(ARFW_USB_INTF_PREFIX) < ARFW_DEVICE_ID_MAX_LEN);
#define ARFW_USB_INTF_SYS_MAX (5)

// This is just a limit to allocate some static resources.
// For instance pool of packets for IN endpoints of CTRL flow.
// The others will be allocated dynamically after CP tells us the real number.
#define ARFW_USB_MAX_RINGS (32)

// Each endpoint will be using a separate pool of buffers.
// We can tweak how many each endpoint can have, but this is the upper limit.
#define ARFW_USB_PACKET_POOL_MAX_SIZE (64)

struct arfw_usb_driver;
struct arfw_usb_ep;

struct arfw_usb_packet {
	struct arfw_usb_ep *ep;
	struct arfw_usb_packet_pool *pool;
	size_t pool_index;
	size_t queue_index;
	uint8_t buffer_index;
	struct urb urb;
	struct usb_anchor *urb_anchor;
	struct workqueue_struct *offload_queue;
	struct delayed_work offload_work;
	size_t buffer_size;
	dma_addr_t buffer_dma_addr;
	void *buffer;
	bool buffer_owner;
	bool can_sleep;
};

struct arfw_usb_packet_pool {
	struct arfw_usb_driver *driver;
	struct arfw_usb_ep *ep;
	bool released;
	size_t pool_size;
	struct arfw_usb_packet *packets;
	size_t buffer_size;
	dma_addr_t buffers_dma_addr;
	void *buffers;
	atomic64_t inactive;
};

struct arfw_usb_ep {
	uint8_t num;
	uint8_t offset;
	bool active;
	bool ctrl;
	struct arfw_usb_driver *driver;
	struct arfw_usb_packet_pool *pool;
	struct usb_anchor urbs;
	atomic_t active_queues;
	struct workqueue_struct *offload_queue;
};

struct arfw_usb_rings {
	struct xarray queues;
	uint16_t *room;
};

struct arfw_usb_room_sync_context {
	bool delayed;
};

struct arfw_usb_driver {
	struct usb_interface *intf;
	char *intf_desc;
	char *intf_sys[ARFW_USB_INTF_SYS_MAX];
	const struct arfw_client_ops *client_ops;
	struct arfw_usb_ep eps[ARFW_USB_EP_ALL_NUM];
	struct arfw_usb_rings rings[META_USB_RING_TYPES_MAX];
	atomic_t ctrl_seq_num;
	struct list_head ctrl_reply_list;
	spinlock_t ctrl_reply_lock;
	meta_usb_protocol_version_t ap_version;
	meta_usb_protocol_version_t cp_version;
	uint16_t cp_rings_max;
	uint16_t cp_inline_max;
	uint32_t cp_inline_eps;
	uint16_t cp_inline_sid;
	uint16_t cp_sid_max;
	struct arfw_usb_ep *ep_ctrl_in;
	struct arfw_usb_ep *ep_ctrl_out;
	struct workqueue_struct *offload_queue;
	struct arfw_usb_packet_pool *sync_pool;
	struct arfw_usb_room_sync_context *sync_context;
	atomic_t sync_burst_total;
	size_t sync_burst_max;
	struct delayed_work reset_work;
	atomic_t reset;
};

/**
 * Re-submit the packet on the same endpoint.
 * Works for any type of packet: IN/OUT.
 *
 * @param[in] packet Struct that holds USB request and metadata.
 */
void arfw_usb_packet_resubmit(struct arfw_usb_packet *packet);

/**
 * Check if the packet can be processed.
 * If this returns TRUE it just means that at the transport
 * level it can, protocol level can still have other issues.
 *
 * NOTE:
 * In case of transport errors the packet will be returned.
 * Also the subsystem will be reset.
 *
 * @param[in] packet Struct that holds USB request and metadata.
 *
 * @retval    TRUE   Packet can be processed.
 * @retval    FALSE  Transport errors, system reset.
 */
bool arfw_usb_packet_can_consume(struct arfw_usb_packet *packet);

/**
 * Allocate pool of a given size and bind completion for packets.
 * Pool needs to be initialized still by attaching it to an endpoint.
 *
 * NOTE:
 * If buffer_size is set to zero - no USB coherent buffers are allocated.
 * It is expected by the caller to fill them out then.
 *
 * @param[in] driver      Struct that holds subsystem context.
 * @param[in] complete    Packet completion callback function.
 * @param[in] pool_size   How many packets in this pool.
 * @param[in] buffer_size Size of each packet buffer (DMA coherent).
 *
 * @retval    PTR         No error.
 * @retval    PTR_ERR     Otherwise.
 */
struct arfw_usb_packet_pool *
arfw_usb_packet_pool_alloc(struct arfw_usb_driver *driver, work_func_t complete,
			   size_t pool_size, size_t buffer_size);

/**
 * Initialize pool by attaching it to an endpoint.
 *
 * @param[in] pool Uninitialized pool.
 * @param[in] ep   Endpoint to bind it to.
 */
void arfw_usb_packet_pool_init(struct arfw_usb_packet_pool *pool,
			       struct arfw_usb_ep *ep);

/**
 * Release pool.
 * This kills all the packets first, cancels all delayed work.
 * This is done sync, then all the DMA coherent memory released.
 *
 * @param[in] pool Initialized pool.
 */
void arfw_usb_packet_pool_free(struct arfw_usb_packet_pool *pool);

/**
 * Acquire a packet from the pool.
 * Non-blocking, will fail if all packets are active.
 *
 * NOTE:
 * Built on FFS algorithm, so currently limited to 64 packets.
 * Can be extended, but does not need it most likely.
 *
 * @param[in] pool    Initialized pool.
 *
 * @retval    PTR     No error.
 * @retval    PTR_ERR Otherwise.
 */
struct arfw_usb_packet *
arfw_usb_packet_pool_get(struct arfw_usb_packet_pool *pool);

/**
 * Allocate a packet (no pool).
 * This orphaned packet needs to be released the same way as
 * the ones coming from a pool.
 *
 * NOTE:
 * This might sleep (GFP_KERNEL on allocations).
 *
 * @param[in] ep       Initialized endpoint.
 * @param[in] complete Packet completion callback function.
 * @param[in] size     Size of each packet buffer (DMA coherent).
 * @param[in] anchor   Request list for tracking.
 *
 * @retval    PTR     No error.
 * @retval    PTR_ERR Otherwise.
 */
struct arfw_usb_packet *arfw_usb_packet_get(struct arfw_usb_ep *ep,
					    work_func_t complete, size_t size,
					    struct usb_anchor *anchor);

/**
 * Release packet.
 * Will only release underlying buffer if caller owns it.
 * Pool DMA coherenent memory will not be released.
 *
 * @param[in] packet Struct that holds USB request and metadata.
 */
void arfw_usb_packet_put(struct arfw_usb_packet *packet);

/**
 * Schedule a packet on OUT pipe.
 * Uses attached endpoint to pick the pipe.
 *
 * @param[in] packet  Struct that holds USB request and metadata.
 * @param[in] sid     Stream ID to attach to this packet, 0 for none.
 * @param[in] context Additional data to attach to this packet.
 *
 * @retval    0     No error.
 * @retval   -E...  Otherwise.
 */
int arfw_usb_packet_send(struct arfw_usb_packet *packet, uint16_t sid,
			 void *context);

/**
 * Schedule a packet on IN pipe.
 * Uses attached endpoint to pick the pipe.
 *
 * @param[in] packet  Struct that holds USB request and metadata.
 * @param[in] sid     Stream ID to attach to this packet, 0 for none.
 * @param[in] context Additional data to attach to this packet.
 *
 * @retval    0       No error.
 * @retval   -E...    Otherwise.
 */
int arfw_usb_packet_recv(struct arfw_usb_packet *packet, uint16_t sid,
			 void *context);

/**
 * Schedule all packets on IN pipe from the endpoint pool.
 *
 * @param[in] ep      Initialized IN endpoint.
 * @param[in] sid     Stream ID to attach to this packet, 0 for none.
 * @param[in] context Additional data to attach to this packet.
 *
 * @retval    0       No error.
 * @retval   -E...    Otherwise.
 */
int arfw_usb_packet_recv_all(struct arfw_usb_ep *ep, uint16_t sid,
			     void *context);

/**
 * Initialize endpoint (shared bits).
 * Sets it to active state at the end.
 *
 * @param[in] ep      Uninitialized endpoint.
 *
 * @retval    0       No error.
 * @retval   -E...    Otherwise.
 */
int arfw_usb_ep_init(struct arfw_usb_ep *ep);

/**
 * Release endpoint (shared bits).
 * Will kill all linked requests and flush the work queue.
 * Also releases the attached packet pool.
 *
 * @param[in] ep Initialized endpoint.
 */
void arfw_usb_ep_exit(struct arfw_usb_ep *ep);

/**
 * Reset the subsystem (schedule).
 * It leads to disconnect/probe being invoked.
 *
 * NOTE:
 * It is offloaded on events work queue.
 *
 * @param[in] driver Struct that holds subsystem context.
 */
void arfw_usb_sys_reset(struct arfw_usb_driver *driver);

/**
 * Reset the subsystem (schedule).
 * It leads to disconnect/probe being invoked.
 * Same as the one above but only triggers on fault/fatals.
 * For instance, if a packet was killed, does not mean we need to reset.
 *
 * NOTE:
 * It is offloaded on events work queue.
 *
 * @param[in] driver Struct that holds subsystem context.
 * @param[in] err    Error to conditionally reset on.
 */
void arfw_usb_sys_reset_on_fault(struct arfw_usb_driver *driver, int err);

/**
 * Reset the subsystem (offloaded).
 * This does the actual reset.
 * We do not try to recover from STALL here, for simplicity.
 *
 * @param[in] work Struct that holds scheduled work data.
 */
void arfw_usb_sys_reset_offload(struct work_struct *work);

/**
 * Check if a given endpoint is inline.
 * There is no way to tell from endpoint descriptors.
 * So firmware sends us a bitmap, check against it.
 *
 * @param[in] ep    Initialized endpoint.
 *
 * @retval    TRUE  Endpoint is INLINE.
 * @retval    FALSE Endpoint is not INLINE.
 */
bool arfw_usb_ep_is_inline(const struct arfw_usb_ep *ep);

#endif // !ARFW_USB_INT_H
