// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_usb_ops.c
 *
 * @brief implementation of the arfw hw device driver ops for USB
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include "arfw_usb_ops.h"

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/usb.h>

#include <ar_common.h>
#include <ar_future.h>
#include <ar_utils.h>
#include <arfw_log.h>

#include "arfw_usb_int.h"
#include "arfw_usb_ctrl.h"
#include "arfw_usb_queue.h"

static unsigned long info_timeout_ms = 1000;
module_param(info_timeout_ms, ulong, 0664);
static unsigned long create_timeout_ms = 1000;
module_param(create_timeout_ms, ulong, 0664);
static unsigned long destroy_timeout_ms = 1000;
module_param(destroy_timeout_ms, ulong, 0664);
static unsigned long recv_ring_pend_buf_max = 32;
module_param(recv_ring_pend_buf_max, ulong, 0664);

/**
 * Driver entry point to handle an arfw_io_request.
 *
 * Note: the driver may assume calls to this function are serialized within a
 * given queue but possibly concurrent across queues.
 *
 * @param[in]  queue_context The hardware queue context.
 * @param[in]  request       Pointer to arfw_io_request.
 * @param[out] consumed      Should the data be marked as consumed.
 *                           If set to false, arfw_request_consumed must be called later.
 *
 * @retval     true/false    Polling should continue (true) or stop (false).
 */
static bool
arfw_usb_handle_send_queue_request(void *queue_context,
				   const struct arfw_io_request *request,
				   bool *consumed)
{
	int err;
	struct arfw_usb_queue *queue = queue_context;

	AR_ASSERT(queue);
	AR_ASSERT(request);
	AR_ASSERT(consumed);

	// We mark the slot as not consumed only if we succeeded to submit it.
	// This allows clients to re-use the slot again, otherwise we wait for DMA to finish.
	// Polling needs to continue as long as the request succeeded.
	err = arfw_usb_queue_send(queue, request);
	*consumed = !!err;

	return !err;
}

/**
 * Driver callback when the client has consumed a buffer from a receive queue.
 *
 * Note: this may be required if the driver is sharing the queue directly to hardware
 * as a DMA target and read indexes need to be updated.
 *
 * @param[in] client        Handle for the client queue.
 * @param[in] queue_context The hardware queue context.
 */
static void arfw_usb_handle_recv_queue_consume(const arfw_client_queue_t client,
					       void *queue_context)
{
	struct arfw_usb_queue *queue = queue_context;

	AR_ASSERT(client);
	AR_ASSERT(queue);
	AR_ASSERT(queue->client == client);

	arfw_usb_queue_recv_consume(queue);
}

/**
 * Driver entry point when a client allocates memory for the queue data.
 *
 * Note: the queue data context is returned and could be used to get information about
 * the queue data.
 *
 * @param[in] base_context The base hw dev context.
 * @param[in] params       Queue information.
 *
 * @retval                 Pointer to the queue data memory region in case of success,
 *                         otherwise NULL.
 */
static struct arfw_queue_mem_region *arfw_usb_handle_queue_data_alloc(
	void *base_context,
	const struct arfw_client_queue_create_params *params)
{
	struct usb_interface *intf = base_context;
	struct arfw_queue_mem_region *queue_data;
	struct arfw_usb_queue_data *data_context;
	struct device *sysdev;

	AR_ASSERT(intf);
	AR_ASSERT(params);

	queue_data = kzalloc(sizeof(*queue_data), GFP_KERNEL);
	if (!queue_data)
		goto fail_data_alloc;

	intf = usb_get_intf(intf);
	if (!intf)
		goto fail_get_intf;

	// You are probably asking yourself why are we not using usb_alloc_coherent here.
	// The reason is that usb_alloc_coherent is trying to do some "smart" things with
	// DMA pools and it is not reliable if you want to also mmap the region into user space.
	// This way we can control it better since we know how it got allocated.
	// We only need to make sure we use the host controller device to feed into DMA ops.
	sysdev = interface_to_usbdev(intf)->bus->sysdev;
	AR_ASSERT(sysdev);
	data_context = devm_kzalloc(sysdev, sizeof(*data_context), GFP_KERNEL);
	if (!data_context)
		goto fail_data_context_alloc;
	queue_data->context = data_context;
	queue_data->size =
		AR_ROUNDUP(params->element_size * params->depth, PAGE_SIZE);
	queue_data->ptr = kzalloc(queue_data->size, GFP_KERNEL);
	if (!queue_data->ptr)
		goto fail_dma_alloc;
	data_context->dma_addr = dma_map_single(
		sysdev, queue_data->ptr, queue_data->size, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(sysdev, data_context->dma_addr))
		goto fail_dma_map;
	kref_init(&queue_data->refcnt);

	return queue_data;

fail_dma_map:
	kfree(queue_data->ptr);
fail_dma_alloc:
	devm_kfree(sysdev, data_context);
fail_data_context_alloc:
	usb_put_intf(intf);
fail_get_intf:
	kfree(queue_data);
fail_data_alloc:
	return NULL;
}

/**
 * Driver entry point when a client frees memory for the queue data.
 *
 * @param[in] base_context The base hw dev context.
 * @param[in] queue_data The queue data memory region to clean up.
 */
static void
arfw_usb_handle_queue_data_free(void *base_context,
				struct arfw_queue_mem_region *queue_data)
{
	struct usb_interface *intf = base_context;
	struct arfw_usb_queue_data *data_context;
	struct device *sysdev;

	AR_ASSERT(intf);
	sysdev = interface_to_usbdev(intf)->bus->sysdev;
	AR_ASSERT(sysdev);
	AR_ASSERT(queue_data);
	data_context = queue_data->context;
	AR_ASSERT(data_context);

	dma_unmap_single(sysdev, data_context->dma_addr, queue_data->size,
			 DMA_BIDIRECTIONAL);
	usb_put_intf(intf);
	devm_kfree(sysdev, data_context);
	// queue_data and queue_data->ptr should be freed outside the device specific
	// context. The generic layer decide when to clean up it, based on the last
	// reference in the user space.
}

/**
 * Driver entry point when a client queue is created.
 *
 * Note: this serves as a point for the driver to allocate a context and set it for
 * the client queue.
 *
 * @param[in]  queue         Client queue created for the request.
 * @param[in]  base_context  The base hw dev context, passed into arfw_device_unregister.
 * @param[in]  params        Queue information.
 * @param[in]  data_context  The context associated with the queue data.
 * @param[out] queue_context The context associated with the queue.
 *
 * @retval     0             No error.
 * @retval    -E...          Otherwise.
 */
static int arfw_usb_handle_queue_create_request(
	const arfw_client_queue_t queue, void *base_context,
	const struct arfw_client_queue_create_params *params,
	struct arfw_queue_mem_region *queue_data, void **queue_context)
{
	ar_future_t *future;
	struct usb_interface *intf = base_context;
	struct arfw_usb_driver *driver;

	AR_ASSERT(intf);
	AR_ASSERT(queue_context);
	driver = usb_get_intfdata(intf);
	AR_ASSERT(driver);

	future = arfw_usb_queue_create(driver, params, queue, queue_data,
				       create_timeout_ms);
	return IS_ERR(future) ? PTR_ERR(future) :
				      ar_future_wait_data(future, queue_context);
}
/**
 * Driver entry point when a queue is destroyed.
 *
 * Note: this allows the driver a chance to cancel any outstanding IO, and
 * release hardware resources.
 *
 * @param[in] base_context  The base context from arfw_device_register.
 * @param[in] queue_context The context associated with this queue.
 *
 * @retval    0             No error.
 * @retval   -E...          Otherwise.
 */
static int arfw_usb_handle_queue_destroy_request(void *base_context,
						 void *queue_context)
{
	ar_future_t *future;
	struct arfw_usb_queue *queue = queue_context;

	AR_ASSERT(base_context);
	AR_ASSERT(queue);

	future = arfw_usb_queue_destroy(queue, destroy_timeout_ms);
	return IS_ERR(future) ? PTR_ERR(future) : ar_future_wait(future);
}

/**
 * Driver entry point the device information query.
 *
 * @param[in]  base_context The base context from arfw_device_register.
 * @param[out] info         The device information structure to populate.
 *
 * @retval     0            The structure was populated.
 * @retval    -E...         Otherwise.
 */
static int
arfw_usb_handle_get_device_information(void *base_context,
				       struct arfw_device_information_req *info)
{
	struct usb_interface *intf = base_context;
	struct arfw_usb_driver *driver;

	AR_ASSERT(intf);
	driver = usb_get_intfdata(intf);
	AR_ASSERT(driver);
	AR_ASSERT(info);

	info->transport_header_size = sizeof(struct arfw_usb_queue_entry);
	info->inline_data_offset =
		offsetof(struct arfw_usb_queue_entry, message.data);
	info->send_ring_max = driver->cp_rings_max / META_USB_RING_TYPES_MAX;
	info->rcv_ring_max = driver->cp_rings_max / META_USB_RING_TYPES_MAX;
	info->require_contiguous_memory_for_queues = true;
	info->rcv_ring_pend_buff_count_max = recv_ring_pend_buf_max;

	return 0;
}

/**
 * Driver entry point to pend a receive external payload.
 *
 * @param[in] queue_context The hardware queue context.
 * @param[in] payload       External payload requested to pend.
 *
 * @retval    0             No error.
 * @retval   -E...          Otherwise.
 */
static int arfw_usb_handle_receive_payload_pend(
	void *queue_context, const struct arfw_payload_pend_req *payload)
{
	struct arfw_usb_queue *queue = queue_context;

	AR_ASSERT(queue);
	AR_ASSERT(payload);

	return arfw_usb_queue_buffer_pend(queue, payload);
}

/*
 * Sometimes even if there are a few elements in the sgt table, the memory
 * space is sequential. For instance this is the case for q35 device + intel-iommu.
 * If this is the case check it and return true, otherwise return false.
 */
static bool arfw_usb_sgt_is_seq(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg;
	dma_addr_t addr;

	AR_ASSERT(sgt);

	addr = sg_dma_address(sgt->sgl);
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		if (addr != sg_dma_address(sg))
			return false;
		addr = sg_dma_address(sg) + sg_dma_len(sg);
	}

	return true;
}

/**
 * Driver quirk for dma allocations.
 *
 * @param[in]  client        Handle for the current client queue.
 * @param[in]  base_context  The base context from arfw_device_register.
 * @param[in]  queue_context The context associated with the queue, NULL if no queue.
 * @param[in]  direction     The device to/from direction used for DMA.
 * @param[in]  sgt           The scatter-gatherer table used for DMA.
 * @param[in]  pages         Pages to DMA map.
 * @param[in]  num_pages     Number of pages to DMA map.
 * @param[out] dma_addr      DMA address of the memory.
 *
 * @retval     0             On success.
 * @retval    -E...          Otherwise.
 */
static int arfw_usb_handle_dma_map_quirk(const arfw_client_queue_t client,
					 void *base_context,
					 void *queue_context,
					 enum dma_data_direction direction,
					 struct sg_table *sgt,
					 struct page **pages, int num_pages,
					 dma_addr_t *dma_addr)
{
	int err;
	unsigned int nents;
	struct usb_interface *intf = base_context;
	struct device *sysdev;
	struct arfw_usb_queue *queue = queue_context;
	struct arfw_usb_ep *ep;

	AR_ASSERT(client);
	AR_ASSERT(sgt);
	AR_ASSERT(pages);
	AR_ASSERT(dma_addr);
	AR_ASSERT(intf);
	sysdev = interface_to_usbdev(intf)->bus->sysdev;
	AR_ASSERT(sysdev);
	AR_ASSERT(queue);
	ep = queue->ep_ext;
	AR_ASSERT(ep);

	err = sg_alloc_table_from_pages(sgt, pages, num_pages, 0,
					num_pages * PAGE_SIZE, GFP_KERNEL);
	if (err) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_DMA_MAP_MEM,
			"Failed to allocate sg table from pages, err=%d", err);
		return err;
	}

	nents = dma_map_sg(sysdev, sgt->sgl, sgt->nents, direction);
	if (nents == 0) {
		err = -EFAULT;
		AR_LOG_USB_QUEUE_ERR(ep, queue, AR_LOG_DMA_MAP_MEM,
				     "Failed to dma map sg list, err=%d", err);
		goto error_dma_map;
	}
	AR_LOG_USB_QUEUE_DBG(ep, queue, AR_LOG_DMA_MAP_MEM,
			     "Succeeded to dma map sg list, size=%lu, nents=%u",
			     num_pages * PAGE_SIZE, nents);

	if (nents > 1 && !arfw_usb_sgt_is_seq(sgt)) {
		AR_LOG_USB_QUEUE_ERR(
			ep, queue, AR_LOG_DMA_MAP_MEM,
			"Unexpected mapping result, nents=%u, non-sequential, mapping_size=%lu",
			nents, num_pages * PAGE_SIZE);
		err = -ENOSPC;
		goto error_sgt_check;
	}

	*dma_addr = sg_dma_address(sgt->sgl);
	return 0;

error_sgt_check:
	dma_unmap_sg(sysdev, sgt->sgl, sgt->nents, direction);
error_dma_map:
	sg_free_table(sgt);
	return err;
}

/**
 * Driver quirk for dma frees.
 *
 * @param[in] client        Handle for the current client queue.
 * @param[in] base_context  The base context from arfw_device_register.
 * @param[in] queue_context The context associated with the queue, NULL if no queue.
 * @param[in] direction     The device to/from direction used for DMA.
 * @param[in] sgt           The scatter-gatherer table used for DMA.
 * @param[in] va            Virtual address of the memory (should be page aligned).
 * @param[in] size          Size of the memory (should be a multiple of PAGE_SIZE).
 * @param[in] dma_addr      DMA address of the memory.
 */
static void arfw_usb_handle_dma_unmap_quirk(const arfw_client_queue_t client,
					    void *base_context,
					    void *queue_context,
					    enum dma_data_direction direction,
					    struct sg_table *sgt, void *va,
					    size_t size, dma_addr_t dma_addr)
{
	struct usb_interface *intf = base_context;
	struct device *sysdev;

	(void)client;
	(void)va;
	(void)size;
	(void)dma_addr;
	AR_ASSERT(sgt);
	AR_ASSERT(intf);
	sysdev = interface_to_usbdev(intf)->bus->sysdev;
	AR_ASSERT(sysdev);

	dma_unmap_sg(sysdev, sgt->sgl, sgt->nents, direction);
	sg_free_table(sgt);
}

/**
 * Driver API to register a region.
 *
 * @param[in] base_context  The base context from arfw_device_register.
 * @param[in] queue_context The context associated with the queue, NULL if no queue.
 * @param[in] mapping_id    The id for this mapping that is scoped to the queue
 * @param[in] dma_size      DMA size of the memory.
 * @param[in] dma_addr      DMA address of the memory.
 *
 * @retval     0            On success.
 * @retval    -E...         Otherwise.
 */
static int arfw_usb_handle_region_add(void *base_context, void *queue_context,
				      uint16_t mapping_id, size_t dma_size,
				      dma_addr_t dma_addr)
{
	int err;
	struct arfw_usb_queue *queue = queue_context;
	struct arfw_usb_ep *ep;

	AR_ASSERT(base_context);
	AR_ASSERT(queue);
	ep = queue->ep_ext;
	AR_ASSERT(dma_size);
	AR_ASSERT(dma_addr);

	err = arfw_usb_queue_buffer_add(queue, mapping_id, dma_size, dma_addr);
	if (err)
		AR_LOG_USB_QUEUE_ERR(ep, queue, AR_LOG_DMA_MAP_MEM,
				     "Failed to add buffer to queue, err=%d",
				     err);

	return err;
}

/**
 * Driver API to unregister a region.
 *
 * @param[in] base_context  The base context from arfw_device_register.
 * @param[in] queue_context The context associated with the queue, NULL if no queue.
 * @param[in] mapping_id    The id for this mapping that is scoped to the queue
 */
static void arfw_usb_handle_region_del(void *base_context, void *queue_context,
				       uint16_t mapping_id)
{
	struct arfw_usb_queue *queue = queue_context;

	AR_ASSERT(base_context);

	// This can happen when we remove the queue when buffers are still registered.
	// No worries, at this point the queue was deleted and the buffers as well.
	// We can skip this step.
	if (queue)
		arfw_usb_queue_buffer_del(queue, mapping_id);
}

/**
 * Driver API to notify until the queue is constructed.
 *
 * @param[in] queue_context The hardware queue context.
 */
static void arfw_usb_handle_notify_queue_ready(void *queue_context)
{
	struct arfw_usb_queue *queue = queue_context;

	AR_ASSERT(queue);
	WRITE_ONCE(queue->active, true);

	if (queue->params.queue_direction == AR_QUEUE_FW_TO_HLOS)
		arfw_usb_queue_recv(queue);
}

/**
 * Mmap the DMA address region to the user space.
 *
 * @param[in] base_context The device context which is used to map memory.
 * @param[in] queue_data Pointer to the queue data context to map.
 * @param[in] vma          VMA to map in.
 *
 * @retval    0            On success.
 * @retval   -E...         Otherwise.
 */
static int
arfw_usb_handle_queue_data_mmap(void *base_context,
				struct arfw_queue_mem_region *queue_data,
				struct vm_area_struct *vma)
{
	struct usb_interface *intf = base_context;
	struct device *sysdev;

	AR_ASSERT(intf);
	sysdev = interface_to_usbdev(intf)->bus->sysdev;
	AR_ASSERT(sysdev);
	AR_ASSERT(queue_data);

	return remap_pfn_range(vma, vma->vm_start,
			       page_to_pfn(virt_to_page(queue_data->ptr)),
			       queue_data->size, vma->vm_page_prot);
}

static const struct arfw_driver_ops arfw_usb_ops = {
	.handle_send_queue = arfw_usb_handle_send_queue_request,
	.handle_rcv_queue_consume = arfw_usb_handle_recv_queue_consume,
	.handle_queue_data_alloc = arfw_usb_handle_queue_data_alloc,
	.handle_queue_data_free = arfw_usb_handle_queue_data_free,
	.handle_queue_create = arfw_usb_handle_queue_create_request,
	.handle_queue_destroy = arfw_usb_handle_queue_destroy_request,
	.handle_get_device_information = arfw_usb_handle_get_device_information,
	.handle_receive_payload_pend = arfw_usb_handle_receive_payload_pend,
	.handle_notify_queue_ready = arfw_usb_handle_notify_queue_ready,
	.handle_queue_data_mmap = arfw_usb_handle_queue_data_mmap,
	.handle_dma_map_quirk = arfw_usb_handle_dma_map_quirk,
	.handle_dma_unmap_quirk = arfw_usb_handle_dma_unmap_quirk,
	.handle_region_add = arfw_usb_handle_region_add,
	.handle_region_del = arfw_usb_handle_region_del,
};

const struct arfw_driver_ops *arfw_usb_ops_get(void)
{
	return &arfw_usb_ops;
}

int arfw_usb_ops_fetch_info(struct arfw_usb_driver *driver)
{
	int err;
	ar_future_t *future;
	meta_usb_query_info_msg_in_t *info = NULL;
	struct usb_interface *intf;

	AR_ASSERT(driver);
	AR_ASSERT(driver->ep_ctrl_in);
	intf = driver->intf;
	AR_ASSERT(intf);

	future = arfw_usb_ctrl_info_query(driver, info_timeout_ms);
	if (IS_ERR(future))
		return PTR_ERR(future);

	err = ar_future_wait_data(future, (void **)&info);
	if (err)
		return err;

	AR_ASSERT(info);
	if (info->cp_rings_max > ARFW_USB_MAX_RINGS ||
	    info->cp_rings_max / META_USB_RING_TYPES_MAX == 0) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Incorrect cp_rings_max value=%d",
				  info->cp_rings_max);
		err = -EINVAL;
		goto out_free_info;
	}
	if (!info->cp_inline_max) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Incorrect cp_inline_max value=%d",
				  info->cp_inline_max);
		err = -EINVAL;
		goto out_free_info;
	}
	if (!info->cp_inline_eps) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Incorrect cp_inline_eps value=%d",
				  info->cp_inline_eps);
		err = -EINVAL;
		goto out_free_info;
	}
	if (info->cp_sid_max && (info->cp_sid_max + 1) < ARFW_USB_SID_NUM_MIN) {
		AR_LOG_USB_EP_ERR(driver->ep_ctrl_in, AR_LOG_CTRL,
				  "Incorrect cp_sid_max value=%d",
				  info->cp_sid_max);
		err = -EINVAL;
		goto out_free_info;
	}

	// Cache all the fields in subsystem driver struct.
	// This will not change in runtime, so reading it once is fine.
	driver->cp_version = info->cp_version;
	driver->cp_rings_max = info->cp_rings_max;
	driver->cp_inline_max = info->cp_inline_max;
	driver->cp_inline_eps = info->cp_inline_eps;
	driver->cp_inline_sid = info->cp_inline_sid;
	driver->cp_sid_max = info->cp_sid_max;

out_free_info:
	devm_kfree(&intf->dev, info);
	return err;
}
