// SPDX-License-Identifier: GPL-2.0+
/*
 *	uvc_gadget.c  --  USB Video Class Gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/g_uvc.h>
#include <linux/usb/video.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-event.h>

#include "u_uvc.h"
#include "uvc.h"
#include "uvc_configfs.h"
#include "uvc_v4l2.h"
#include "uvc_video.h"

unsigned int uvc_gadget_trace_param;
module_param_named(trace, uvc_gadget_trace_param, uint, 0644);
MODULE_PARM_DESC(trace, "Trace level bitmask");

/* --------------------------------------------------------------------------
 * Function descriptors
 */

/* string IDs are assigned dynamically */

#define UVC_STRING_CONTROL_IDX			0
#define UVC_STRING_STREAMING_IDX		1

static struct usb_string uvc_en_us_strings[] = {
	/* [UVC_STRING_CONTROL_IDX].s = DYNAMIC, */
	[UVC_STRING_STREAMING_IDX].s = "Video Streaming",
	{  }
};

static struct usb_gadget_strings uvc_stringtab = {
	.language = 0x0409,	/* en-us */
	.strings = uvc_en_us_strings,
};

static struct usb_gadget_strings *uvc_function_strings[] = {
	&uvc_stringtab,
	NULL,
};

#define UVC_INTF_VIDEO_CONTROL			0
#define UVC_INTF_VIDEO_STREAMING		1

#define UVC_STATUS_MAX_PACKET_SIZE		16	/* 16 bytes status */

static struct usb_interface_assoc_descriptor uvc_iad = {
	.bLength		= sizeof(uvc_iad),
	.bDescriptorType	= USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface	= 0,
	.bInterfaceCount	= 2,
	.bFunctionClass		= USB_CLASS_VIDEO,
	.bFunctionSubClass	= UVC_SC_VIDEO_INTERFACE_COLLECTION,
	.bFunctionProtocol	= 0x00,
	.iFunction		= 0,
};

static struct usb_interface_descriptor uvc_control_intf = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= UVC_INTF_VIDEO_CONTROL,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_VIDEO,
	.bInterfaceSubClass	= UVC_SC_VIDEOCONTROL,
	.bInterfaceProtocol	= 0x01,
	.iInterface		= 0,
};

static struct usb_endpoint_descriptor uvc_control_ep = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize		= cpu_to_le16(UVC_STATUS_MAX_PACKET_SIZE),
	.bInterval		= 8,
};

static struct usb_ss_ep_comp_descriptor uvc_ss_control_comp = {
	.bLength		= sizeof(uvc_ss_control_comp),
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	/* The following 3 values can be tweaked if necessary. */
	.bMaxBurst		= 0,
	.bmAttributes		= 0,
	.wBytesPerInterval	= cpu_to_le16(UVC_STATUS_MAX_PACKET_SIZE),
};

static struct uvc_control_endpoint_descriptor uvc_control_cs_ep = {
	.bLength		= UVC_DT_CONTROL_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_CS_ENDPOINT,
	.bDescriptorSubType	= UVC_EP_INTERRUPT,
	.wMaxTransferSize	= cpu_to_le16(UVC_STATUS_MAX_PACKET_SIZE),
};

static struct usb_interface_descriptor uvc_streaming_intf_alt0 = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= UVC_INTF_VIDEO_STREAMING,
	.bAlternateSetting	= 0,
	.bInterfaceClass	= USB_CLASS_VIDEO,
	.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
	.bInterfaceProtocol	= 0x00,
	.iInterface		= 0,
	/* bNumEndpoints will be initialized from module parameters */
};

static struct usb_interface_descriptor uvc_streaming_intf_alt1 = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= UVC_INTF_VIDEO_STREAMING,
	.bAlternateSetting	= 1,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_VIDEO,
	.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
	.bInterfaceProtocol	= 0x00,
	.iInterface		= 0,
};

static struct usb_endpoint_descriptor uvc_fs_streaming_ep = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	/* The wMaxPacketSize, bInterval, and bmAttributes values will be
	 * initialized from module parameters.
	 */
};

static struct usb_endpoint_descriptor uvc_hs_streaming_ep = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	/* The wMaxPacketSize, bInterval, and bmAttributes values will be
	 * initialized from module parameters.
	 */
};

static struct usb_endpoint_descriptor uvc_ss_streaming_ep = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,

	.bEndpointAddress	= USB_DIR_IN,
	/* The wMaxPacketSize, bInterval, and bmAttributes values will be
	 * initialized from module parameters.
	 */
};

static struct usb_ss_ep_comp_descriptor uvc_ss_streaming_comp = {
	.bLength		= sizeof(uvc_ss_streaming_comp),
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	/* The bMaxBurst, bmAttributes and wBytesPerInterval values will be
	 * initialized from module parameters.
	 */
};

static const struct usb_descriptor_header * const uvc_fs_streaming_isoc[] = {
	(struct usb_descriptor_header *) &uvc_streaming_intf_alt1,
	(struct usb_descriptor_header *) &uvc_fs_streaming_ep,
	NULL,
};

static const struct usb_descriptor_header * const uvc_hs_streaming_isoc[] = {
	(struct usb_descriptor_header *) &uvc_streaming_intf_alt1,
	(struct usb_descriptor_header *) &uvc_hs_streaming_ep,
	NULL,
};

static const struct usb_descriptor_header * const uvc_ss_streaming_isoc[] = {
	(struct usb_descriptor_header *) &uvc_streaming_intf_alt1,
	(struct usb_descriptor_header *) &uvc_ss_streaming_ep,
	(struct usb_descriptor_header *) &uvc_ss_streaming_comp,
	NULL,
};

static const struct usb_descriptor_header * const uvc_fs_streaming_bulk[] = {
	(struct usb_descriptor_header *) &uvc_fs_streaming_ep,
	NULL,
};

static const struct usb_descriptor_header * const uvc_hs_streaming_bulk[] = {
	(struct usb_descriptor_header *) &uvc_hs_streaming_ep,
	NULL,
};

static const struct usb_descriptor_header * const uvc_ss_streaming_bulk[] = {
	(struct usb_descriptor_header *) &uvc_ss_streaming_ep,
	(struct usb_descriptor_header *) &uvc_ss_streaming_comp,
	NULL,
};

void uvc_set_trace_param(unsigned int trace)
{
	uvc_gadget_trace_param = trace;
}
EXPORT_SYMBOL(uvc_set_trace_param);

/* --------------------------------------------------------------------------
 * Stream on/off control
 *
 * While the UVC specification is clear about how stream on/off should work
 * with an isochronous video streaming endpoint (via alternate settings), it
 * doesn't specify how this should work with a bulk endpoint. UVC host drivers
 * have generally adopted the Windows approach which is to turn the stream on
 * by sending a UVC VS commit control and turn the stream off by sending a
 * CLEAR_FEATURE(ENDPOINT_HALT) standard request. In linux, this request is
 * handled by the device controller driver and the gadget driver is not
 * notified in any way. As a result, we emulate this behavior here by
 * monitoring the time to complete queued V4L video frames. If we detect that
 * frames are not being completed within a number of frame periods, we assume
 * that the stream has been turned off.
 */

static int uvc_vs_ep_enable(struct usb_function *f)
{
	struct uvc_device *uvc = to_uvc(f);
	int ret;

	if (!uvc->video.ep)
		return -EINVAL;

	uvcg_info(f, "reset UVC\n");
	usb_ep_disable(uvc->video.ep);

	ret = config_ep_by_speed(f->config->cdev->gadget,
			&(uvc->func), uvc->video.ep);
	if (ret)
		return ret;

	usb_ep_enable(uvc->video.ep);

	return 0;
}

static void uvc_stream_on(struct usb_function *f)
{
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;

	if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK) {
		uvc->video.watchdog.check_interval =
			nsecs_to_jiffies(uvc->video.watchdog.frame_interval_ns * 2);
		uvc->video.watchdog.timeout =
			nsecs_to_jiffies(uvc->video.watchdog.frame_interval_ns * 5);

		mod_timer(&uvc->video.watchdog.timer,
			jiffies + uvc->video.watchdog.check_interval);
	}

	memset(&v4l2_event, 0, sizeof(v4l2_event));
	v4l2_event.type = UVC_EVENT_STREAMON;
	v4l2_event_queue(&uvc->vdev, &v4l2_event);
}

static void uvc_stream_off(struct usb_function *f)
{
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;

	if (uvc->state != UVC_STATE_STREAMING)
		return;

	memset(&v4l2_event, 0, sizeof(v4l2_event));
	v4l2_event.type = UVC_EVENT_STREAMOFF;
	v4l2_event_queue(&uvc->vdev, &v4l2_event);
}

void uvc_stream_watchdog(struct timer_list *timer)
{
	struct uvc_video_watchdog *watchdog = from_timer(watchdog, timer, timer);
	struct uvc_device *uvc = watchdog->uvc;
	struct uvc_request *req;
	struct uvc_buffer *buf;
	bool buf_queued = false;
	unsigned long queue_time;
	unsigned long flags;

	if (uvc->state != UVC_STATE_STREAMING)
		return;

	/* First check if any uvc_buffers are currently owned by a uvc_request.
	 * These are frames which have been fully encoded and queued to the device
	 * controller, so they will be older than buffers in the irqqueue.
	 */
	spin_lock_irqsave(&uvc->video.req_lock, flags);

	list_for_each_entry(req, &uvc->video.ureqs, list){
		if (req->last_buf) {
			if (!buf_queued)
				queue_time = req->last_buf->queue_time;
			else if (time_before(req->last_buf->queue_time, queue_time))
				queue_time = req->last_buf->queue_time;

			buf_queued = true;
		}
	}

	spin_unlock_irqrestore(&uvc->video.req_lock, flags);

	/* Next check if any uvc_buffers are waiting to be encoded in the irqqueue
	 * if we haven't already found one above. In this case, the head of the
	 * queue will be the oldest buffer owned by the UVC driver.
	 */
	if (!buf_queued) {
		spin_lock_irqsave(&uvc->video.queue.irqlock, flags);

		buf = uvcg_queue_head(&uvc->video.queue);
		if (buf) {
			buf_queued = true;
			queue_time = buf->queue_time;
		}

		spin_unlock_irqrestore(&uvc->video.queue.irqlock, flags);
	}

	if (buf_queued &&
		time_after(jiffies, queue_time + watchdog->timeout)) {
		uvc_stream_off(&uvc->func);
		return;
	}

	mod_timer(timer, jiffies + watchdog->check_interval);
}

/* --------------------------------------------------------------------------
 * Control requests
 */

static void
uvc_function_ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct usb_function *f = req->context;
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;

	if (uvc->event_setup_out) {
		uvc->event_setup_out = 0;

		memset(&v4l2_event, 0, sizeof(v4l2_event));
		v4l2_event.type = UVC_EVENT_DATA;
		uvc_event->data.length = req->actual;
		memcpy(&uvc_event->data.data, req->buf, req->actual);
		v4l2_event_queue(&uvc->vdev, &v4l2_event);

		if (uvc->vs_commit_control) {
			uvc->vs_commit_control = 0;
			if (uvc->state == UVC_STATE_CONNECTED) {
				/* Determine the frame period based on dwFrameInterval */
				uvc->video.watchdog.frame_interval_ns =
					le32_to_cpup((__le32 *)&req->buf[4]) * 100;
				uvc_stream_on(f);
			}
		}
	}
}

static int
uvc_function_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	unsigned int selector;
	unsigned int interface;

	if ((ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS) {
		uvcg_info(f, "invalid request type\n");
		return -EINVAL;
	}

	/* Stall too big requests. */
	if (le16_to_cpu(ctrl->wLength) > UVC_MAX_REQUEST_SIZE)
		return -EINVAL;

	/* Tell the complete callback to generate an event for the next request
	 * that will be enqueued by UVCIOC_SEND_RESPONSE.
	 */
	uvc->event_setup_out = !(ctrl->bRequestType & USB_DIR_IN);
	uvc->event_length = le16_to_cpu(ctrl->wLength);

	memset(&v4l2_event, 0, sizeof(v4l2_event));
	v4l2_event.type = UVC_EVENT_SETUP;
	memcpy(&uvc_event->req, ctrl, sizeof(uvc_event->req));
	v4l2_event_queue(&uvc->vdev, &v4l2_event);

	if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK &&
		uvc->event_setup_out) {
		interface = le16_to_cpu(ctrl->wIndex) & 0xff;
		selector = le16_to_cpu(ctrl->wValue) >> 8 & 0xff;
		if (interface == uvc->streaming_intf &&
			selector == UVC_VS_COMMIT_CONTROL) {
			uvc->vs_commit_control = 1;
		}
	}

	return 0;
}

void uvc_function_setup_continue(struct uvc_device *uvc, int disable_ep)
{
	struct usb_composite_dev *cdev = uvc->func.config->cdev;

	if (uvc->video.transfer == UVC_STREAM_TRANSFER_ISOC) {
		if (disable_ep && uvc->video.ep)
			usb_ep_disable(uvc->video.ep);

		usb_composite_setup_continue(cdev);
	}
}

static int
uvc_function_get_alt(struct usb_function *f, unsigned interface)
{
	struct uvc_device *uvc = to_uvc(f);

	uvcg_info(f, "%s(%u)\n", __func__, interface);

	if (interface == uvc->control_intf)
		return 0;
	else if (interface != uvc->streaming_intf)
		return -EINVAL;
	else {
		if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK)
			return 0;

		return uvc->video.ep->enabled ? 1 : 0;
	}
}

static int
uvc_function_set_alt(struct usb_function *f, unsigned interface, unsigned alt)
{
	struct uvc_device *uvc = to_uvc(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	int ret;

	uvcg_info(f, "%s(%u, %u)\n", __func__, interface, alt);

	if (interface == uvc->control_intf) {
		if (alt)
			return -EINVAL;

		uvcg_info(f, "reset UVC Control\n");
		usb_ep_disable(uvc->control_ep);

		if (!uvc->control_ep->desc)
			if (config_ep_by_speed(cdev->gadget, f, uvc->control_ep))
				return -EINVAL;

		usb_ep_enable(uvc->control_ep);

		if (uvc->state == UVC_STATE_DISCONNECTED) {
			memset(&v4l2_event, 0, sizeof(v4l2_event));
			v4l2_event.type = UVC_EVENT_CONNECT;
			uvc_event->speed = cdev->gadget->speed;
			v4l2_event_queue(&uvc->vdev, &v4l2_event);

			uvc->state = UVC_STATE_CONNECTED;
		}

		return 0;
	}

	if (interface != uvc->streaming_intf)
		return -EINVAL;


	if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK) {
		if (alt != 0)
			return -EINVAL;

		return uvc_vs_ep_enable(f);
	}

	switch (alt) {
	case 0:
		if (uvc->state != UVC_STATE_STREAMING)
			return 0;

		uvc_stream_off(f);
		return USB_GADGET_DELAYED_STATUS;

	case 1:
		if (uvc->state != UVC_STATE_CONNECTED)
			return 0;

		ret = uvc_vs_ep_enable(f);
		if (ret)
			return ret;

		uvc_stream_on(f);
		return USB_GADGET_DELAYED_STATUS;

	default:
		return -EINVAL;
	}
}

static void
uvc_function_disable(struct usb_function *f)
{
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;

	uvcg_info(f, "%s()\n", __func__);

	memset(&v4l2_event, 0, sizeof(v4l2_event));
	v4l2_event.type = UVC_EVENT_DISCONNECT;
	v4l2_event_queue(&uvc->vdev, &v4l2_event);

	uvc->state = UVC_STATE_DISCONNECTED;

	usb_ep_disable(uvc->video.ep);
	usb_ep_disable(uvc->control_ep);
}

/* --------------------------------------------------------------------------
 * Connection / disconnection
 */

void
uvc_function_connect(struct uvc_device *uvc)
{
	int ret;

	if ((ret = usb_function_activate(&uvc->func)) < 0)
		uvcg_info(&uvc->func, "UVC connect failed with %d\n", ret);
}

void
uvc_function_disconnect(struct uvc_device *uvc)
{
	int ret;

	if ((ret = usb_function_deactivate(&uvc->func)) < 0)
		uvcg_info(&uvc->func, "UVC disconnect failed with %d\n", ret);
}

/* --------------------------------------------------------------------------
 * USB probe and disconnect
 */

static ssize_t function_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct uvc_device *uvc = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", uvc->func.fi->group.cg_item.ci_name);
}

static DEVICE_ATTR_RO(function_name);

static void uvc_video_device_release(struct video_device *vdev)
{
	struct uvc_device *uvc = container_of(vdev, struct uvc_device, vdev);

	memset(vdev, 0, sizeof(*vdev));
	complete(&uvc->unbind_ok);
}

static int
uvc_register_video(struct uvc_device *uvc)
{
	struct usb_composite_dev *cdev = uvc->func.config->cdev;
	int ret;

	/* TODO reference counting. */
	memset(&uvc->vdev, 0, sizeof(uvc->vdev));
	uvc->vdev.v4l2_dev = &uvc->v4l2_dev;
	uvc->vdev.v4l2_dev->dev = &cdev->gadget->dev;
	uvc->vdev.fops = &uvc_v4l2_fops;
	uvc->vdev.ioctl_ops = &uvc_v4l2_ioctl_ops;
	uvc->vdev.release = uvc_video_device_release;
	uvc->vdev.vfl_dir = VFL_DIR_TX;
	uvc->vdev.lock = &uvc->video.mutex;
	uvc->vdev.device_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING;
	strlcpy(uvc->vdev.name, cdev->gadget->name, sizeof(uvc->vdev.name));

	video_set_drvdata(&uvc->vdev, uvc);

	ret = video_register_device(&uvc->vdev, VFL_TYPE_GRABBER, -1);
	if (ret < 0)
		return ret;

	ret = device_create_file(&uvc->vdev.dev, &dev_attr_function_name);
	if (ret < 0) {
		video_unregister_device(&uvc->vdev);
		return ret;
	}

	return 0;
}

#define UVC_COPY_DESCRIPTOR(mem, dst, desc) \
	do { \
		memcpy(mem, desc, (desc)->bLength); \
		*(dst)++ = mem; \
		mem += (desc)->bLength; \
	} while (0);

#define UVC_COPY_DESCRIPTORS(mem, dst, src) \
	do { \
		const struct usb_descriptor_header * const *__src; \
		for (__src = src; *__src; ++__src) { \
			memcpy(mem, *__src, (*__src)->bLength); \
			*dst++ = mem; \
			mem += (*__src)->bLength; \
		} \
	} while (0)

#define UVC_COPY_XU_DESCRIPTOR(mem, dst, desc)					\
	do {									\
		*(dst)++ = mem;							\
		memcpy(mem, desc, 22); /* bLength to bNrInPins */		\
		mem += 22;							\
										\
		memcpy(mem, (desc)->baSourceID, (desc)->bNrInPins);		\
		mem += (desc)->bNrInPins;					\
										\
		memcpy(mem, &(desc)->bControlSize, 1);				\
		mem++;								\
										\
		memcpy(mem, (desc)->bmControls, (desc)->bControlSize);		\
		mem += (desc)->bControlSize;					\
										\
		memcpy(mem, &(desc)->iExtension, 1);				\
		mem++;								\
	} while (0)

static struct usb_descriptor_header **
uvc_copy_descriptors(struct uvc_device *uvc, enum usb_device_speed speed)
{
	struct uvc_input_header_descriptor *uvc_streaming_header;
	struct uvc_header_descriptor *uvc_control_header;
	const struct uvc_descriptor_header * const *uvc_control_desc;
	const struct uvc_descriptor_header * const *uvc_streaming_cls;
	const struct usb_descriptor_header * const *uvc_streaming_std;
	const struct usb_descriptor_header * const *src;
	struct usb_descriptor_header **dst;
	struct usb_descriptor_header **hdr;
	struct uvcg_extension *xu;
	unsigned int control_size;
	unsigned int streaming_size;
	unsigned int n_desc;
	unsigned int bytes;
	void *mem;

	switch (speed) {
	case USB_SPEED_SUPER_PLUS:
	case USB_SPEED_SUPER:
		uvc_control_desc = uvc->desc.ss_control;
		uvc_streaming_cls = uvc->desc.ss_streaming;
		if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK)
			uvc_streaming_std = uvc_ss_streaming_bulk;
		else
			uvc_streaming_std = uvc_ss_streaming_isoc;
		break;

	case USB_SPEED_HIGH:
		uvc_control_desc = uvc->desc.fs_control;
		uvc_streaming_cls = uvc->desc.hs_streaming;
		if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK)
			uvc_streaming_std = uvc_hs_streaming_bulk;
		else
			uvc_streaming_std = uvc_hs_streaming_isoc;
		break;

	case USB_SPEED_FULL:
	default:
		uvc_control_desc = uvc->desc.fs_control;
		uvc_streaming_cls = uvc->desc.fs_streaming;
		if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK)
			uvc_streaming_std = uvc_fs_streaming_bulk;
		else
			uvc_streaming_std = uvc_fs_streaming_isoc;
		break;
	}

	if (!uvc_control_desc || !uvc_streaming_cls)
		return ERR_PTR(-ENODEV);

	/* Descriptors layout
	 *
	 * uvc_iad
	 * uvc_control_intf
	 * Class-specific UVC control descriptors
	 * uvc_control_ep
	 * uvc_control_cs_ep
	 * uvc_ss_control_comp (for SS only)
	 * uvc_streaming_intf_alt0
	 * Class-specific UVC streaming descriptors
	 * uvc_{fs|hs}_streaming
	 */

	/* Count descriptors and compute their size. */
	control_size = 0;
	streaming_size = 0;
	bytes = uvc_iad.bLength + uvc_control_intf.bLength
	      + uvc_control_ep.bLength + uvc_control_cs_ep.bLength
	      + uvc_streaming_intf_alt0.bLength;

	if (speed >= USB_SPEED_SUPER) {
		bytes += uvc_ss_control_comp.bLength;
		n_desc = 6;
	} else {
		n_desc = 5;
	}

	for (src = (const struct usb_descriptor_header **)uvc_control_desc;
	     *src; ++src) {
		control_size += (*src)->bLength;
		bytes += (*src)->bLength;
		n_desc++;
	}

	list_for_each_entry(xu, uvc->desc.extension_units, list) {
		control_size += xu->desc.bLength;
		bytes += xu->desc.bLength;
		n_desc++;
	}

	for (src = (const struct usb_descriptor_header **)uvc_streaming_cls;
	     *src; ++src) {
		streaming_size += (*src)->bLength;
		bytes += (*src)->bLength;
		n_desc++;
	}
	for (src = uvc_streaming_std; *src; ++src) {
		bytes += (*src)->bLength;
		n_desc++;
	}

	mem = kmalloc((n_desc + 1) * sizeof(*src) + bytes, GFP_KERNEL);
	if (mem == NULL)
		return NULL;

	hdr = mem;
	dst = mem;
	mem += (n_desc + 1) * sizeof(*src);

	/* Copy the descriptors. */
	UVC_COPY_DESCRIPTOR(mem, dst, &uvc_iad);
	UVC_COPY_DESCRIPTOR(mem, dst, &uvc_control_intf);

	uvc_control_header = mem;
	UVC_COPY_DESCRIPTORS(mem, dst,
		(const struct usb_descriptor_header **)uvc_control_desc);

	list_for_each_entry(xu, uvc->desc.extension_units, list)
		UVC_COPY_XU_DESCRIPTOR(mem, dst, &xu->desc);

	uvc_control_header->wTotalLength = cpu_to_le16(control_size);
	uvc_control_header->bInCollection = 1;
	uvc_control_header->baInterfaceNr[0] = uvc->streaming_intf;

	UVC_COPY_DESCRIPTOR(mem, dst, &uvc_control_ep);
	if (speed >= USB_SPEED_SUPER)
		UVC_COPY_DESCRIPTOR(mem, dst, &uvc_ss_control_comp);

	UVC_COPY_DESCRIPTOR(mem, dst, &uvc_control_cs_ep);
	UVC_COPY_DESCRIPTOR(mem, dst, &uvc_streaming_intf_alt0);

	uvc_streaming_header = mem;
	UVC_COPY_DESCRIPTORS(mem, dst,
		(const struct usb_descriptor_header**)uvc_streaming_cls);
	uvc_streaming_header->wTotalLength = cpu_to_le16(streaming_size);
	uvc_streaming_header->bEndpointAddress = uvc->video.ep->address;

	UVC_COPY_DESCRIPTORS(mem, dst, uvc_streaming_std);

	*dst = NULL;
	return hdr;
}

static int
uvc_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct uvc_device *uvc = to_uvc(f);
	struct uvcg_extension *xu;
	struct usb_string *us;
	unsigned int max_packet_mult;
	unsigned int max_packet_size;
	struct usb_ep *ep;
	struct f_uvc_opts *opts;
	int ret = -EINVAL;

	uvcg_info(f, "%s()\n", __func__);

	opts = fi_to_f_uvc_opts(f->fi);
	/* Sanity check the streaming endpoint module parameters.
	 */
	opts->streaming_interval = clamp(opts->streaming_interval, 1U, 16U);
	opts->streaming_maxpacket = clamp(opts->streaming_maxpacket, 1U, 3072U);
	opts->streaming_maxburst = min(opts->streaming_maxburst, 15U);
	opts->streaming_maxpayload = clamp(opts->streaming_maxpayload, 1024U, 65536U);
	if (!opts->streaming_transfer)
		opts->streaming_transfer = USB_ENDPOINT_XFER_ISOC | USB_ENDPOINT_SYNC_ASYNC;

	/* Initialize bmAttributes based on our transfer type isoc/bulk */
	uvc_fs_streaming_ep.bmAttributes = opts->streaming_transfer;
	uvc_hs_streaming_ep.bmAttributes = opts->streaming_transfer;
	uvc_ss_streaming_ep.bmAttributes = opts->streaming_transfer;

	uvc_ss_streaming_comp.bMaxBurst = opts->streaming_maxburst;

	if (opts->streaming_transfer & USB_ENDPOINT_XFER_BULK)
		uvc->video.transfer = UVC_STREAM_TRANSFER_BULK;
	else
		uvc->video.transfer = UVC_STREAM_TRANSFER_ISOC;

	if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK) {
		/* FS bulk endpoints support 8/16/32/64 bytes */
		uvc_fs_streaming_ep.wMaxPacketSize = cpu_to_le16(opts->streaming_maxpacket);
		uvc_fs_streaming_ep.bInterval = 0;

		uvc_hs_streaming_ep.wMaxPacketSize = cpu_to_le16(512);
		uvc_hs_streaming_ep.bInterval = 0;

		uvc_ss_streaming_ep.wMaxPacketSize = cpu_to_le16(1024);
		uvc_ss_streaming_ep.bInterval = 0;

		uvc_ss_streaming_comp.bmAttributes = 0;
		uvc_ss_streaming_comp.wBytesPerInterval = 0;

		uvc->video.max_payload_size = opts->streaming_maxpayload;

	} else {
		/* For SS, wMaxPacketSize has to be 1024 if bMaxBurst is not 0 */
		if (opts->streaming_maxburst &&
		  (opts->streaming_maxpacket % 1024) != 0) {
			opts->streaming_maxpacket = roundup(opts->streaming_maxpacket, 1024);
			uvcg_info(f, "overriding streaming_maxpacket to %d\n",
				  opts->streaming_maxpacket);
		}

		/* Fill in the FS/HS/SS Video Streaming specific descriptors from the
			* module parameters.
			*
			* NOTE: We assume that the user knows what they are doing and won't
			* give parameters that their UDC doesn't support.
			*/
		if (opts->streaming_maxpacket <= 1024) {
			max_packet_mult = 1;
			max_packet_size = opts->streaming_maxpacket;
		} else if (opts->streaming_maxpacket <= 2048) {
			max_packet_mult = 2;
			max_packet_size = opts->streaming_maxpacket / 2;
		} else {
			max_packet_mult = 3;
			max_packet_size = opts->streaming_maxpacket / 3;
		}

		uvc_fs_streaming_ep.wMaxPacketSize =
			cpu_to_le16(min(opts->streaming_maxpacket, 1023U));
		uvc_fs_streaming_ep.bInterval = opts->streaming_interval;

		uvc_hs_streaming_ep.wMaxPacketSize =
			cpu_to_le16(max_packet_size | ((max_packet_mult - 1) << 11));
		uvc_hs_streaming_ep.bInterval = opts->streaming_interval;

		uvc_ss_streaming_ep.wMaxPacketSize = cpu_to_le16(max_packet_size);
		uvc_ss_streaming_ep.bInterval = opts->streaming_interval;
		uvc_ss_streaming_comp.bmAttributes = max_packet_mult - 1;
		uvc_ss_streaming_comp.bMaxBurst = opts->streaming_maxburst;
		uvc_ss_streaming_comp.wBytesPerInterval =
			cpu_to_le16(max_packet_size * max_packet_mult *
					(opts->streaming_maxburst + 1));

		uvc->video.max_payload_size = 0;
	}

	/* Allocate endpoints. */
	ep = usb_ep_autoconfig(cdev->gadget, &uvc_control_ep);
	if (!ep) {
		uvcg_info(f, "Unable to allocate control EP\n");
		goto error;
	}
	uvc->control_ep = ep;

	if (gadget_is_superspeed(c->cdev->gadget))
		ep = usb_ep_autoconfig_ss(cdev->gadget, &uvc_ss_streaming_ep,
					  &uvc_ss_streaming_comp);
	else if (gadget_is_dualspeed(cdev->gadget))
		ep = usb_ep_autoconfig(cdev->gadget, &uvc_hs_streaming_ep);
	else
		ep = usb_ep_autoconfig(cdev->gadget, &uvc_fs_streaming_ep);

	if (!ep) {
		uvcg_info(f, "Unable to allocate streaming EP\n");
		goto error;
	}
	uvc->video.ep = ep;

	uvc_fs_streaming_ep.bEndpointAddress = uvc->video.ep->address;
	uvc_hs_streaming_ep.bEndpointAddress = uvc->video.ep->address;
	uvc_ss_streaming_ep.bEndpointAddress = uvc->video.ep->address;

	/*
	 * XUs can have an arbitrary string descriptor describing them. If they
	 * have one pick up the ID.
	 */
	list_for_each_entry(xu, &opts->extension_units, list)
		if (xu->string_descriptor_index)
			xu->desc.iExtension = cdev->usb_strings[xu->string_descriptor_index].id;

	/*
	 * We attach the hard-coded defaults incase the user does not provide
	 * any more appropriate strings through configfs.
	 */
	uvc_en_us_strings[UVC_STRING_CONTROL_IDX].s = opts->function_name;
	us = usb_gstrings_attach(cdev, uvc_function_strings,
				 ARRAY_SIZE(uvc_en_us_strings));
	if (IS_ERR(us)) {
		ret = PTR_ERR(us);
		goto error;
	}

	uvc_iad.iFunction = opts->iad_index ? cdev->usb_strings[opts->iad_index].id :
			    us[UVC_STRING_CONTROL_IDX].id;
	uvc_streaming_intf_alt0.iInterface = opts->vs0_index ?
					     cdev->usb_strings[opts->vs0_index].id :
					     us[UVC_STRING_STREAMING_IDX].id;
	uvc_streaming_intf_alt1.iInterface = opts->vs1_index ?
					     cdev->usb_strings[opts->vs1_index].id :
					     us[UVC_STRING_STREAMING_IDX].id;

	if (uvc->video.transfer == UVC_STREAM_TRANSFER_BULK)
		uvc_streaming_intf_alt0.bNumEndpoints = 1;
	else
		uvc_streaming_intf_alt0.bNumEndpoints = 0;

	/* Allocate interface IDs. */
	if ((ret = usb_interface_id(c, f)) < 0)
		goto error;
	uvc_iad.bFirstInterface = ret;
	uvc_control_intf.bInterfaceNumber = ret;
	uvc->control_intf = ret;
	opts->control_interface = ret;

	if ((ret = usb_interface_id(c, f)) < 0)
		goto error;
	uvc_streaming_intf_alt0.bInterfaceNumber = ret;
	uvc_streaming_intf_alt1.bInterfaceNumber = ret;
	uvc->streaming_intf = ret;
	opts->streaming_interface = ret;

	/* Copy descriptors */
	f->fs_descriptors = uvc_copy_descriptors(uvc, USB_SPEED_FULL);
	if (IS_ERR(f->fs_descriptors)) {
		ret = PTR_ERR(f->fs_descriptors);
		f->fs_descriptors = NULL;
		goto error;
	}
	if (gadget_is_dualspeed(cdev->gadget)) {
		f->hs_descriptors = uvc_copy_descriptors(uvc, USB_SPEED_HIGH);
		if (IS_ERR(f->hs_descriptors)) {
			ret = PTR_ERR(f->hs_descriptors);
			f->hs_descriptors = NULL;
			goto error;
		}
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		f->ss_descriptors = uvc_copy_descriptors(uvc, USB_SPEED_SUPER);
		if (IS_ERR(f->ss_descriptors)) {
			ret = PTR_ERR(f->ss_descriptors);
			f->ss_descriptors = NULL;
			goto error;
		}
	}
	if (gadget_is_superspeed_plus(c->cdev->gadget)) {
		f->ssp_descriptors = uvc_copy_descriptors(uvc,
						USB_SPEED_SUPER_PLUS);
		if (IS_ERR(f->ssp_descriptors)) {
			ret = PTR_ERR(f->ssp_descriptors);
			f->ssp_descriptors = NULL;
			goto error;
		}
	}

	/* Preallocate control endpoint request. */
	uvc->control_req = usb_ep_alloc_request(cdev->gadget->ep0, GFP_KERNEL);
	uvc->control_buf = kmalloc(UVC_MAX_REQUEST_SIZE, GFP_KERNEL);
	if (uvc->control_req == NULL || uvc->control_buf == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	uvc->control_req->buf = uvc->control_buf;
	uvc->control_req->complete = uvc_function_ep0_complete;
	uvc->control_req->context = f;

	if (v4l2_device_register(&cdev->gadget->dev, &uvc->v4l2_dev)) {
		uvcg_err(f, "failed to register V4L2 device\n");
		goto error;
	}

	/* Initialise video. */
	ret = uvcg_video_init(&uvc->video, uvc);
	if (ret < 0)
		goto v4l2_error;

	/* Register a V4L2 device. */
	ret = uvc_register_video(uvc);
	if (ret < 0) {
		uvcg_err(f, "failed to register video device\n");
		goto v4l2_error;
	}

	reinit_completion(&uvc->unbind_ok);
	uvc->wait_for_close = false;
	return 0;

v4l2_error:
	v4l2_device_unregister(&uvc->v4l2_dev);
error:
	if (uvc->control_req)
		usb_ep_free_request(cdev->gadget->ep0, uvc->control_req);
	kfree(uvc->control_buf);

	usb_free_all_descriptors(f);
	return ret;
}

/* --------------------------------------------------------------------------
 * USB gadget function
 */

static void uvc_free_inst(struct usb_function_instance *f)
{
	struct f_uvc_opts *opts = fi_to_f_uvc_opts(f);

	mutex_destroy(&opts->lock);
	kfree(opts);
}

static struct usb_function_instance *uvc_alloc_inst(void)
{
	struct f_uvc_opts *opts;
	struct uvc_camera_terminal_descriptor *cd;
	struct uvc_processing_unit_descriptor *pd;
	struct uvc_output_terminal_descriptor *od;
	struct uvc_color_matching_descriptor *md;
	struct uvc_descriptor_header **ctl_cls;
	int ret;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	opts->func_inst.free_func_inst = uvc_free_inst;
	mutex_init(&opts->lock);

	cd = &opts->uvc_camera_terminal;
	cd->bLength			= UVC_DT_CAMERA_TERMINAL_SIZE(3);
	cd->bDescriptorType		= USB_DT_CS_INTERFACE;
	cd->bDescriptorSubType		= UVC_VC_INPUT_TERMINAL;
	cd->bTerminalID			= 1;
	cd->wTerminalType		= cpu_to_le16(0x0201);
	cd->bAssocTerminal		= 0;
	cd->iTerminal			= 0;
	cd->wObjectiveFocalLengthMin	= cpu_to_le16(0);
	cd->wObjectiveFocalLengthMax	= cpu_to_le16(0);
	cd->wOcularFocalLength		= cpu_to_le16(0);
	cd->bControlSize		= 3;
	cd->bmControls[0]		= 62;
	cd->bmControls[1]		= 126;
	cd->bmControls[2]		= 10;

	pd = &opts->uvc_processing;
	pd->bLength			= UVC_DT_PROCESSING_UNIT_SIZE(3);
	pd->bDescriptorType		= USB_DT_CS_INTERFACE;
	pd->bDescriptorSubType		= UVC_VC_PROCESSING_UNIT;
	pd->bUnitID			= 2;
	pd->bSourceID			= 1;
	pd->wMaxMultiplier		= cpu_to_le16(16*1024);
	pd->bControlSize		= 3;
	pd->bmControls[0]		= 91;
	pd->bmControls[1]		= 23;
	pd->bmControls[2]		= 4;
	pd->iProcessing			= 0;
	pd->bmVideoStandards		= 0;

	od = &opts->uvc_output_terminal;
	od->bLength			= UVC_DT_OUTPUT_TERMINAL_SIZE;
	od->bDescriptorType		= USB_DT_CS_INTERFACE;
	od->bDescriptorSubType		= UVC_VC_OUTPUT_TERMINAL;
	od->bTerminalID			= 3;
	od->wTerminalType		= cpu_to_le16(0x0101);
	od->bAssocTerminal		= 0;
	od->bSourceID			= 2;
	od->iTerminal			= 0;

	/*
	 * With the ability to add XUs to the UVC function graph, we need to be
	 * able to allocate unique unit IDs to them. The IDs are 1-based, with
	 * the CT, PU and OT above consuming the first 3.
	 */
	opts->last_unit_id              = 3;

	md = &opts->uvc_color_matching;
	md->bLength			= UVC_DT_COLOR_MATCHING_SIZE;
	md->bDescriptorType		= USB_DT_CS_INTERFACE;
	md->bDescriptorSubType		= UVC_VS_COLORFORMAT;
	md->bColorPrimaries		= 1;
	md->bTransferCharacteristics	= 1;
	md->bMatrixCoefficients		= 4;

	/* Prepare fs control class descriptors for configfs-based gadgets */
	ctl_cls = opts->uvc_fs_control_cls;
	ctl_cls[0] = NULL;	/* assigned elsewhere by configfs */
	ctl_cls[1] = (struct uvc_descriptor_header *)cd;
	ctl_cls[2] = (struct uvc_descriptor_header *)pd;
	ctl_cls[3] = (struct uvc_descriptor_header *)od;
	ctl_cls[4] = NULL;	/* NULL-terminate */
	opts->fs_control =
		(const struct uvc_descriptor_header * const *)ctl_cls;

	/* Prepare hs control class descriptors for configfs-based gadgets */
	ctl_cls = opts->uvc_ss_control_cls;
	ctl_cls[0] = NULL;	/* assigned elsewhere by configfs */
	ctl_cls[1] = (struct uvc_descriptor_header *)cd;
	ctl_cls[2] = (struct uvc_descriptor_header *)pd;
	ctl_cls[3] = (struct uvc_descriptor_header *)od;
	ctl_cls[4] = NULL;	/* NULL-terminate */
	opts->ss_control =
		(const struct uvc_descriptor_header * const *)ctl_cls;

	INIT_LIST_HEAD(&opts->extension_units);

	opts->streaming_interval = 1;
	opts->streaming_maxpacket = 1024;
	snprintf(opts->function_name, sizeof(opts->function_name), "UVC Camera");

	ret = uvcg_attach_configfs(opts);
	if (ret < 0) {
		kfree(opts);
		return ERR_PTR(ret);
	}

	return &opts->func_inst;
}

static void uvc_free(struct usb_function *f)
{
	struct uvc_device *uvc = to_uvc(f);
	struct f_uvc_opts *opts = container_of(f->fi, struct f_uvc_opts,
					       func_inst);
	--opts->refcnt;
	kfree(uvc);
}

static void uvc_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;
	struct uvc_video *video = &uvc->video;
	long wait_ret = 1;

	uvcg_info(f, "%s\n", __func__);

	memset(&v4l2_event, 0, sizeof(v4l2_event));
	v4l2_event.type = UVC_EVENT_UNBIND;
	v4l2_event_queue(&uvc->vdev, &v4l2_event);

	if (video->async_wq)
		destroy_workqueue(video->async_wq);

	/*
	 * If we know we're connected via v4l2, then there should be a cleanup
	 * of the device from userspace either via UVC_EVENT_DISCONNECT or
	 * though the video device removal uevent. Allow some time for the
	 * application to close out before things get deleted.
	 */
	if (uvc->func_connected) {
		uvcg_dbg(f, "waiting for clean disconnect\n");
		wait_ret = wait_event_interruptible_timeout(uvc->func_connected_queue,
				uvc->func_connected == false, msecs_to_jiffies(500));
		uvcg_dbg(f, "done waiting with ret: %ld\n", wait_ret);
	}

	device_remove_file(&uvc->vdev.dev, &dev_attr_function_name);
	video_unregister_device(&uvc->vdev);
	v4l2_device_unregister(&uvc->v4l2_dev);

	if (uvc->wait_for_close)
		wait_for_completion(&uvc->unbind_ok);

	if (uvc->func_connected) {
		/* Wait for the release to occur to ensure there are no longer any
		 * pending operations that may cause panics when resources are cleaned
		 * up.
		 */
		uvcg_warn(f, "%s no clean disconnect, wait for release\n", __func__);
		wait_ret = wait_event_interruptible_timeout(uvc->func_connected_queue,
				uvc->func_connected == false, msecs_to_jiffies(1000));
		uvcg_dbg(f, "done waiting for release with ret: %ld\n", wait_ret);
	}

	usb_ep_free_request(cdev->gadget->ep0, uvc->control_req);
	kfree(uvc->control_buf);

	usb_free_all_descriptors(f);
}

static struct usb_function *uvc_alloc(struct usb_function_instance *fi)
{
	struct uvc_device *uvc;
	struct f_uvc_opts *opts;
	struct uvc_descriptor_header **strm_cls;

	uvc = kzalloc(sizeof(*uvc), GFP_KERNEL);
	if (uvc == NULL)
		return ERR_PTR(-ENOMEM);

	init_completion(&uvc->unbind_ok);
	mutex_init(&uvc->video.mutex);
	uvc->state = UVC_STATE_DISCONNECTED;
	init_waitqueue_head(&uvc->func_connected_queue);
	opts = fi_to_f_uvc_opts(fi);

	mutex_lock(&opts->lock);
	if (opts->uvc_fs_streaming_cls) {
		strm_cls = opts->uvc_fs_streaming_cls;
		opts->fs_streaming =
			(const struct uvc_descriptor_header * const *)strm_cls;
	}
	if (opts->uvc_hs_streaming_cls) {
		strm_cls = opts->uvc_hs_streaming_cls;
		opts->hs_streaming =
			(const struct uvc_descriptor_header * const *)strm_cls;
	}
	if (opts->uvc_ss_streaming_cls) {
		strm_cls = opts->uvc_ss_streaming_cls;
		opts->ss_streaming =
			(const struct uvc_descriptor_header * const *)strm_cls;
	}

	uvc->desc.fs_control = opts->fs_control;
	uvc->desc.ss_control = opts->ss_control;
	uvc->desc.fs_streaming = opts->fs_streaming;
	uvc->desc.hs_streaming = opts->hs_streaming;
	uvc->desc.ss_streaming = opts->ss_streaming;

	uvc->desc.extension_units = &opts->extension_units;

	++opts->refcnt;
	mutex_unlock(&opts->lock);

	/* Register the function. */
	uvc->func.name = "uvc";
	uvc->func.bind = uvc_function_bind;
	uvc->func.unbind = uvc_unbind;
	uvc->func.get_alt = uvc_function_get_alt;
	uvc->func.set_alt = uvc_function_set_alt;
	uvc->func.disable = uvc_function_disable;
	uvc->func.setup = uvc_function_setup;
	uvc->func.free_func = uvc_free;
	uvc->func.bind_deactivated = true;

	/* Initialize the stream watchdog timer */
	uvc->video.watchdog.uvc = uvc;
	timer_setup(&uvc->video.watchdog.timer, uvc_stream_watchdog, 0);

	return &uvc->func;
}

DECLARE_USB_FUNCTION_INIT(uvc, uvc_alloc_inst, uvc_alloc);

static int uvc_init(void)
{
	return usb_function_register(&uvcusb_func);
}
module_init(uvc_init);

static void __exit uvc_exit(void)
{
	usb_function_unregister(&uvcusb_func);
}
module_exit(uvc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Laurent Pinchart");
