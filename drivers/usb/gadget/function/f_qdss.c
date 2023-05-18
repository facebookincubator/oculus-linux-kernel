// SPDX-License-Identifier: GPL-2.0-only
/*
 * f_qdss.c -- QDSS function Driver
 *
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb/usb_qdss.h>
#include <linux/usb/cdc.h>

#include "f_qdss.h"

static DEFINE_SPINLOCK(channel_lock);
static LIST_HEAD(usb_qdss_ch_list);

static struct usb_interface_descriptor qdss_data_intf_desc = {
	.bLength            =	sizeof(qdss_data_intf_desc),
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bAlternateSetting  =   0,
	.bNumEndpoints      =	1,
	.bInterfaceClass    =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	USB_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol =	0x70,
};

static struct usb_endpoint_descriptor qdss_hs_data_desc = {
	.bLength              =	 USB_DT_ENDPOINT_SIZE,
	.bDescriptorType      =	 USB_DT_ENDPOINT,
	.bEndpointAddress     =	 USB_DIR_IN,
	.bmAttributes         =	 USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize       =	 cpu_to_le16(512),
};

static struct usb_endpoint_descriptor qdss_ss_data_desc = {
	.bLength              =	 USB_DT_ENDPOINT_SIZE,
	.bDescriptorType      =	 USB_DT_ENDPOINT,
	.bEndpointAddress     =	 USB_DIR_IN,
	.bmAttributes         =  USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize       =	 cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor qdss_data_ep_comp_desc = {
	.bLength              =	 sizeof(qdss_data_ep_comp_desc),
	.bDescriptorType      =	 USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst            =	 1,
	.bmAttributes         =	 0,
	.wBytesPerInterval    =	 0,
};

static struct usb_interface_descriptor qdss_ctrl_intf_desc = {
	.bLength            =	sizeof(qdss_ctrl_intf_desc),
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bAlternateSetting  =   0,
	.bNumEndpoints      =	2,
	.bInterfaceClass    =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	USB_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol =	0x70,
};

static struct usb_endpoint_descriptor qdss_hs_ctrl_in_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_IN,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor qdss_ss_ctrl_in_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_IN,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor qdss_hs_ctrl_out_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_OUT,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor qdss_ss_ctrl_out_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_OUT,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	cpu_to_le16(0x400),
};

static struct usb_ss_ep_comp_descriptor qdss_ctrl_in_ep_comp_desc = {
	.bLength            =	sizeof(qdss_ctrl_in_ep_comp_desc),
	.bDescriptorType    =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst          =	0,
	.bmAttributes       =	0,
	.wBytesPerInterval  =	0,
};

static struct usb_ss_ep_comp_descriptor qdss_ctrl_out_ep_comp_desc = {
	.bLength            =	sizeof(qdss_ctrl_out_ep_comp_desc),
	.bDescriptorType    =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst          =	0,
	.bmAttributes       =	0,
	.wBytesPerInterval  =	0,
};

/* Full speed support */
static struct usb_endpoint_descriptor qdss_fs_data_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_IN,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	cpu_to_le16(64),
};

static struct usb_endpoint_descriptor qdss_fs_ctrl_in_desc  = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_IN,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	cpu_to_le16(64),
};

static struct usb_endpoint_descriptor qdss_fs_ctrl_out_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_OUT,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	cpu_to_le16(64),
};

static struct usb_descriptor_header *qdss_fs_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_fs_data_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_intf_desc,
	(struct usb_descriptor_header *) &qdss_fs_ctrl_in_desc,
	(struct usb_descriptor_header *) &qdss_fs_ctrl_out_desc,
	NULL,
};

static struct usb_descriptor_header *qdss_hs_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_hs_data_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_intf_desc,
	(struct usb_descriptor_header *) &qdss_hs_ctrl_in_desc,
	(struct usb_descriptor_header *) &qdss_hs_ctrl_out_desc,
	NULL,
};

static struct usb_descriptor_header *qdss_ss_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_ss_data_desc,
	(struct usb_descriptor_header *) &qdss_data_ep_comp_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_intf_desc,
	(struct usb_descriptor_header *) &qdss_ss_ctrl_in_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_in_ep_comp_desc,
	(struct usb_descriptor_header *) &qdss_ss_ctrl_out_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_out_ep_comp_desc,
	NULL,
};

static struct usb_descriptor_header *qdss_fs_data_only_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_fs_data_desc,
	NULL,
};

static struct usb_descriptor_header *qdss_hs_data_only_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_hs_data_desc,
	NULL,
};

static struct usb_descriptor_header *qdss_ss_data_only_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_ss_data_desc,
	(struct usb_descriptor_header *) &qdss_data_ep_comp_desc,
	NULL,
};

/* string descriptors: */
#define QDSS_DATA_IDX	0
#define QDSS_CTRL_IDX	1

static struct usb_string qdss_string_defs[] = {
	[QDSS_DATA_IDX].s = "QDSS DATA",
	[QDSS_CTRL_IDX].s = "QDSS CTRL",
	{}, /* end of list */
};

static struct usb_gadget_strings qdss_string_table = {
	.language =		0x0409,
	.strings =		qdss_string_defs,
};

static struct usb_gadget_strings *qdss_strings[] = {
	&qdss_string_table,
	NULL,
};

static inline struct f_qdss *func_to_qdss(struct usb_function *f)
{
	return container_of(f, struct f_qdss, port.function);
}

static
struct usb_qdss_opts *to_fi_usb_qdss_opts(struct usb_function_instance *fi)
{
	return container_of(fi, struct usb_qdss_opts, func_inst);
}
/*----------------------------------------------------------------------*/

static void qdss_write_complete(struct usb_ep *ep,
	struct usb_request *req)
{
	struct f_qdss *qdss = ep->driver_data;
	struct qdss_req *qreq = req->context;
	struct qdss_request *d_req = qreq->qdss_req;
	struct usb_ep *in;
	struct list_head *list_pool;
	enum qdss_state state;
	unsigned long flags;

	qdss_log("%s\n", __func__);

	in = qdss->port.data;
	list_pool = &qdss->data_write_pool;
	state = USB_QDSS_DATA_WRITE_DONE;

	spin_lock_irqsave(&qdss->lock, flags);
	list_del(&qreq->list);
	list_add_tail(&qreq->list, list_pool);
	complete(&qreq->write_done);
	if (req->length != 0) {
		d_req->actual = req->actual;
		d_req->status = req->status;
	}
	spin_unlock_irqrestore(&qdss->lock, flags);

	if (qdss->ch.notify)
		qdss->ch.notify(qdss->ch.priv, state, d_req, NULL);
}

void usb_qdss_free_req(struct usb_qdss_ch *ch)
{
	struct f_qdss *qdss;
	struct list_head *act, *tmp;
	struct qdss_req *qreq;
	unsigned long flags;

	spin_lock_irqsave(&channel_lock, flags);
	qdss = ch->priv_usb;
	if (!qdss) {
		pr_err("%s: qdss ctx is NULL\n", __func__);
		spin_unlock_irqrestore(&channel_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&channel_lock, flags);

	spin_lock_irqsave(&qdss->lock, flags);
	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);

	list_for_each_safe(act, tmp, &qdss->data_write_pool) {
		qreq = list_entry(act, struct qdss_req, list);
		list_del(&qreq->list);
		usb_ep_free_request(qdss->port.data, qreq->usb_req);
		kfree(qreq);

	}

	spin_unlock_irqrestore(&qdss->lock, flags);
}
EXPORT_SYMBOL(usb_qdss_free_req);

int usb_qdss_alloc_req(struct usb_qdss_ch *ch, int no_write_buf)
{
	struct f_qdss *qdss = ch->priv_usb;
	struct usb_request *req;
	struct usb_ep *in;
	struct list_head *list_pool;
	int i;
	struct qdss_req *qreq;
	unsigned long flags;

	qdss_log("%s\n", __func__);

	if (!qdss) {
		pr_err("%s: %s closed\n", __func__, ch->name);
		return -ENODEV;
	}

	in = qdss->port.data;
	list_pool = &qdss->data_write_pool;

	for (i = 0; i < no_write_buf; i++) {
		qreq = kzalloc(sizeof(struct qdss_req), GFP_KERNEL);
		if (!qreq)
			goto fail;

		req = usb_ep_alloc_request(in, GFP_ATOMIC);
		if (!req) {
			pr_err("%s: ctrl_in allocation err\n", __func__);
			kfree(qreq);
			goto fail;
		}
		spin_lock_irqsave(&qdss->lock, flags);
		qreq->usb_req = req;
		req->context = qreq;
		req->complete = qdss_write_complete;
		list_add_tail(&qreq->list, list_pool);
		init_completion(&qreq->write_done);
		spin_unlock_irqrestore(&qdss->lock, flags);
	}

	return 0;

fail:
	usb_qdss_free_req(ch);
	return -ENOMEM;
}
EXPORT_SYMBOL(usb_qdss_alloc_req);

static void clear_eps(struct usb_function *f)
{
	struct f_qdss *qdss = func_to_qdss(f);

	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);

	if (qdss->port.ctrl_in)
		qdss->port.ctrl_in->driver_data = NULL;
	if (qdss->port.ctrl_out)
		qdss->port.ctrl_out->driver_data = NULL;
	if (qdss->port.data)
		qdss->port.data->driver_data = NULL;
}

static void clear_desc(struct usb_gadget *gadget, struct usb_function *f)
{
	struct f_qdss *qdss = func_to_qdss(f);

	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);

	usb_free_all_descriptors(f);
}

static int qdss_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_gadget *gadget = c->cdev->gadget;
	struct f_qdss *qdss = func_to_qdss(f);
	struct usb_ep *ep;
	int iface, id, ret;

	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);

	/* Allocate data I/F */
	iface = usb_interface_id(c, f);
	if (iface < 0) {
		pr_err("interface allocation error\n");
		return iface;
	}
	qdss_data_intf_desc.bInterfaceNumber = iface;
	qdss->data_iface_id = iface;

	if (!qdss_string_defs[QDSS_DATA_IDX].id) {
		id = usb_string_id(c->cdev);
		if (id < 0)
			return id;
		qdss_string_defs[QDSS_DATA_IDX].id = id;
		qdss_data_intf_desc.iInterface = id;
	}

	if (qdss->debug_inface_enabled) {
		/* Allocate ctrl I/F */
		iface = usb_interface_id(c, f);
		if (iface < 0) {
			pr_err("interface allocation error\n");
			return iface;
		}
		qdss_ctrl_intf_desc.bInterfaceNumber = iface;
		qdss->ctrl_iface_id = iface;

		if (!qdss_string_defs[QDSS_CTRL_IDX].id) {
			id = usb_string_id(c->cdev);
			if (id < 0)
				return id;
			qdss_string_defs[QDSS_CTRL_IDX].id = id;
			qdss_ctrl_intf_desc.iInterface = id;
		}
	}

	/* for non-accelerated path keep tx fifo size 1k */
	if (!strcmp(qdss->ch.name, USB_QDSS_CH_MDM))
		qdss_data_ep_comp_desc.bMaxBurst = 0;

	ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_data_desc,
		&qdss_data_ep_comp_desc);
	if (!ep) {
		pr_err("%s: ep_autoconfig error\n", __func__);
		goto clear_ep;
	}
	qdss->port.data = ep;
	ep->driver_data = qdss;

	if (qdss->debug_inface_enabled) {
		ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_ctrl_in_desc,
			&qdss_ctrl_in_ep_comp_desc);
		if (!ep) {
			pr_err("%s: ep_autoconfig error\n", __func__);
			goto clear_ep;
		}

		qdss->port.ctrl_in = ep;
		ep->driver_data = qdss;

		ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_ctrl_out_desc,
			&qdss_ctrl_out_ep_comp_desc);
		if (!ep) {
			pr_err("%s: ep_autoconfig error\n", __func__);
			goto clear_ep;
		}
		qdss->port.ctrl_out = ep;
		ep->driver_data = qdss;
	}

	if (!strcmp(qdss->ch.name, USB_QDSS_CH_MSM)) {
		ret = alloc_sps_req(qdss->port.data);
		if (ret) {
			pr_err("%s: alloc_sps_req error (%d)\n",
							__func__, ret);
			goto clear_ep;
		}
	}

	/*update fs descriptors*/
	qdss_fs_data_desc.bEndpointAddress =
		qdss_ss_data_desc.bEndpointAddress;
	if (qdss->debug_inface_enabled) {
		qdss_fs_ctrl_in_desc.bEndpointAddress =
		qdss_ss_ctrl_in_desc.bEndpointAddress;
		qdss_fs_ctrl_out_desc.bEndpointAddress =
		qdss_ss_ctrl_out_desc.bEndpointAddress;
	}

	/*update descriptors*/
	qdss_hs_data_desc.bEndpointAddress =
		qdss_ss_data_desc.bEndpointAddress;
	if (qdss->debug_inface_enabled) {
		qdss_hs_ctrl_in_desc.bEndpointAddress =
		qdss_ss_ctrl_in_desc.bEndpointAddress;
		qdss_hs_ctrl_out_desc.bEndpointAddress =
		qdss_ss_ctrl_out_desc.bEndpointAddress;
	}

	if (qdss->debug_inface_enabled)
		ret = usb_assign_descriptors(f, qdss_fs_desc, qdss_hs_desc,
				qdss_ss_desc, qdss_ss_desc);
	else
		ret = usb_assign_descriptors(f, qdss_fs_data_only_desc,
				qdss_hs_data_only_desc, qdss_ss_data_only_desc,
				qdss_ss_data_only_desc);

	if (ret)
		goto fail;

	return 0;

fail:
	/* check if usb_request allocated */
	if (qdss->endless_req) {
		usb_ep_free_request(qdss->port.data,
				qdss->endless_req);
		qdss->endless_req = NULL;
	}

clear_ep:
	clear_eps(f);

	return -ENOTSUPP;
}


static void qdss_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_qdss  *qdss = func_to_qdss(f);
	struct usb_gadget *gadget = c->cdev->gadget;

	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);

	flush_workqueue(qdss->wq);

	if (qdss->endless_req) {
		usb_ep_free_request(qdss->port.data,
				qdss->endless_req);
		qdss->endless_req = NULL;
	}

	/* Reset string ids */
	qdss_string_defs[QDSS_DATA_IDX].id = 0;
	qdss_string_defs[QDSS_CTRL_IDX].id = 0;

	clear_eps(f);
	clear_desc(gadget, f);
}

static void qdss_eps_disable(struct usb_function *f)
{
	struct f_qdss  *qdss = func_to_qdss(f);

	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);

	if (qdss->ctrl_in_enabled) {
		usb_ep_disable(qdss->port.ctrl_in);
		qdss->ctrl_in_enabled = 0;
	}

	if (qdss->ctrl_out_enabled) {
		usb_ep_disable(qdss->port.ctrl_out);
		qdss->ctrl_out_enabled = 0;
	}

	if (qdss->data_enabled) {
		usb_ep_disable(qdss->port.data);
		qdss->port.data->endless = false;
		qdss->data_enabled = 0;
	}
}

static void usb_qdss_disconnect_work(struct work_struct *work)
{
	struct f_qdss *qdss;
	int status;

	qdss = container_of(work, struct f_qdss, disconnect_w);
	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);


	/* Notify qdss to cancel all active transfers */
	if (qdss->ch.notify)
		qdss->ch.notify(qdss->ch.priv,
			USB_QDSS_DISCONNECT,
			NULL,
			NULL);

	/* Uninitialized init data i.e. ep specific operation */
	if (qdss->ch.app_conn && !strcmp(qdss->ch.name, USB_QDSS_CH_MSM)) {
		status = uninit_data(qdss->port.data);
		if (status)
			pr_err("%s: uninit_data error\n", __func__);

		status = set_qdss_data_connection(qdss, 0);
		if (status)
			pr_err("qdss_disconnect error\n");
	}

	/*
	 * Decrement usage count which was incremented
	 * before calling connect work
	 */
	usb_gadget_autopm_put_async(qdss->gadget);
}

static void qdss_disable(struct usb_function *f)
{
	struct f_qdss	*qdss = func_to_qdss(f);
	unsigned long flags;

	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);
	spin_lock_irqsave(&qdss->lock, flags);
	if (!qdss->usb_connected) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		return;
	}

	qdss->usb_connected = 0;
	spin_unlock_irqrestore(&qdss->lock, flags);
	/*cancell all active xfers*/
	qdss_eps_disable(f);
	queue_work(qdss->wq, &qdss->disconnect_w);
}

static void usb_qdss_connect_work(struct work_struct *work)
{
	struct f_qdss *qdss;
	int status;
	struct usb_request *req = NULL;
	unsigned long flags;

	qdss = container_of(work, struct f_qdss, connect_w);

	/* If cable is already removed, discard connect_work */
	spin_lock_irqsave(&qdss->lock, flags);
	if (qdss->usb_connected == 0) {
		qdss_log("%s: discard connect_work\n", __func__);
		spin_unlock_irqrestore(&qdss->lock, flags);
		cancel_work_sync(&qdss->disconnect_w);
		return;
	}

	qdss_log("%s: channel name = %s\n", __func__, qdss->ch.name);

	if (!strcmp(qdss->ch.name, USB_QDSS_CH_MDM)) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		goto notify;
	}

	spin_unlock_irqrestore(&qdss->lock, flags);
	status = set_qdss_data_connection(qdss, 1);
	if (status) {
		pr_err("set_qdss_data_connection error(%d)\n", status);
		return;
	}

	spin_lock_irqsave(&qdss->lock, flags);
	req = qdss->endless_req;
	spin_unlock_irqrestore(&qdss->lock, flags);
	if (!req)
		return;

	status = usb_ep_queue(qdss->port.data, req, GFP_ATOMIC);
	if (status) {
		pr_err("%s: usb_ep_queue error (%d)\n", __func__, status);
		return;
	}

notify:
	if (qdss->ch.notify)
		qdss->ch.notify(qdss->ch.priv, USB_QDSS_CONNECT,
						NULL, &qdss->ch);
}

static int qdss_set_alt(struct usb_function *f, unsigned int intf,
				unsigned int alt)
{
	struct f_qdss  *qdss = func_to_qdss(f);
	struct usb_gadget *gadget = f->config->cdev->gadget;
	struct usb_qdss_ch *ch = &qdss->ch;
	int ret = 0;

	qdss_log("%s qdss pointer = %pK\n", __func__, qdss);
	qdss->gadget = gadget;

	if (alt != 0)
		goto fail1;

	if (gadget->speed < USB_SPEED_HIGH) {
		pr_err("%s: qdss doesn't support USB full or low speed\n",
								__func__);
		ret = -EINVAL;
		goto fail1;
	}

	if (intf == qdss->data_iface_id && !qdss->data_enabled) {
		/* Increment usage count on connect */
		usb_gadget_autopm_get_async(qdss->gadget);

		if (config_ep_by_speed(gadget, f, qdss->port.data)) {
			ret = -EINVAL;
			goto fail;
		}

		qdss->port.data->endless = true;
		ret = usb_ep_enable(qdss->port.data);
		if (ret) {
			qdss->port.data->endless = false;
			goto fail;
		}

		qdss->port.data->driver_data = qdss;
		qdss->data_enabled = 1;


	} else if ((intf == qdss->ctrl_iface_id) &&
	(qdss->debug_inface_enabled)) {

		if (config_ep_by_speed(gadget, f, qdss->port.ctrl_in)) {
			ret = -EINVAL;
			goto fail1;
		}

		ret = usb_ep_enable(qdss->port.ctrl_in);
		if (ret)
			goto fail1;

		qdss->port.ctrl_in->driver_data = qdss;
		qdss->ctrl_in_enabled = 1;

		if (config_ep_by_speed(gadget, f, qdss->port.ctrl_out)) {
			ret = -EINVAL;
			goto fail1;
		}


		ret = usb_ep_enable(qdss->port.ctrl_out);
		if (ret)
			goto fail1;

		qdss->port.ctrl_out->driver_data = qdss;
		qdss->ctrl_out_enabled = 1;
	}

	if (qdss->debug_inface_enabled) {
		if (qdss->ctrl_out_enabled && qdss->ctrl_in_enabled &&
			qdss->data_enabled) {
			qdss->usb_connected = 1;
			qdss_log("%s usb_connected INTF enabled\n", __func__);
		}
	} else {
		if (qdss->data_enabled) {
			qdss->usb_connected = 1;
			qdss_log("%s usb_connected INTF disabled\n", __func__);
		}
	}

	if (qdss->usb_connected && ch->app_conn)
		queue_work(qdss->wq, &qdss->connect_w);

	return 0;
fail:
	/* Decrement usage count in case of failure */
	usb_gadget_autopm_put_async(qdss->gadget);
fail1:
	pr_err("%s failed\n", __func__);
	qdss_eps_disable(f);
	return ret;
}

static struct f_qdss *alloc_usb_qdss(char *channel_name)
{
	struct f_qdss *qdss;
	int found = 0;
	struct usb_qdss_ch *ch;
	unsigned long flags;

	spin_lock_irqsave(&channel_lock, flags);
	list_for_each_entry(ch, &usb_qdss_ch_list, list) {
		if (!strcmp(channel_name, ch->name)) {
			found = 1;
			break;
		}
	}

	if (found) {
		spin_unlock_irqrestore(&channel_lock, flags);
		pr_err("%s: (%s) is already available.\n",
				__func__, channel_name);
		return ERR_PTR(-EEXIST);
	}

	spin_unlock_irqrestore(&channel_lock, flags);
	qdss = kzalloc(sizeof(struct f_qdss), GFP_KERNEL);
	if (!qdss) {
		pr_err("%s: Unable to allocate qdss device\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	qdss->wq = create_singlethread_workqueue(channel_name);
	if (!qdss->wq) {
		kfree(qdss);
		return ERR_PTR(-ENOMEM);
	}

	spin_lock_irqsave(&channel_lock, flags);
	ch = &qdss->ch;
	ch->name = channel_name;
	list_add_tail(&ch->list, &usb_qdss_ch_list);
	spin_unlock_irqrestore(&channel_lock, flags);

	spin_lock_init(&qdss->lock);
	INIT_LIST_HEAD(&qdss->data_write_pool);
	INIT_LIST_HEAD(&qdss->queued_data_pool);
	INIT_WORK(&qdss->connect_w, usb_qdss_connect_work);
	INIT_WORK(&qdss->disconnect_w, usb_qdss_disconnect_work);

	return qdss;
}

int usb_qdss_write(struct usb_qdss_ch *ch, struct qdss_request *d_req)
{
	struct f_qdss *qdss = ch->priv_usb;
	unsigned long flags;
	struct usb_request *req = NULL;
	struct qdss_req *qreq;

	qdss_log("usb_qdss_data_write\n");

	if (!qdss)
		return -ENODEV;

	spin_lock_irqsave(&qdss->lock, flags);

	if (qdss->qdss_close || qdss->usb_connected == 0) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EIO;
	}

	/* If data_write_pool is empty & debug inface is set
	 * then return -EINVAL instead of -EAGAIN.
	 * qdss_write is expecting data_write_pool to
	 * be populated. But data_write_pool is populated only
	 * when debug_inface_enbled is not set.
	 */
	if (list_empty(&qdss->data_write_pool) &&
	    qdss->debug_inface_enabled) {
		pr_err("%s:error: Invalid operation.\n", __func__);
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EINVAL;
	}

	if (list_empty(&qdss->data_write_pool)) {
		pr_err_ratelimited(
			"error: usb_qdss_data_write list is empty\n");
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EAGAIN;
	}

	qreq = list_first_entry(&qdss->data_write_pool, struct qdss_req,
		list);
	list_move_tail(&qreq->list, &qdss->queued_data_pool);
	spin_unlock_irqrestore(&qdss->lock, flags);

	qreq->qdss_req = d_req;
	req = qreq->usb_req;
	req->buf = d_req->buf;
	req->length = d_req->length;
	req->sg = d_req->sg;
	req->num_sgs = d_req->num_sgs;
	req->num_mapped_sgs = d_req->num_mapped_sgs;
	reinit_completion(&qreq->write_done);
	if (usb_ep_queue(qdss->port.data, req, GFP_ATOMIC)) {
		spin_lock_irqsave(&qdss->lock, flags);
		/* Remove from queued pool and add back to data pool */
		list_move_tail(&qreq->list, &qdss->data_write_pool);
		complete(&qreq->write_done);
		spin_unlock_irqrestore(&qdss->lock, flags);
		pr_err("qdss usb_ep_queue failed\n");
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(usb_qdss_write);

struct usb_qdss_ch *usb_qdss_open(const char *name, void *priv,
	void (*notify)(void *priv, unsigned int event,
		struct qdss_request *d_req, struct usb_qdss_ch *))
{
	struct usb_qdss_ch *ch;
	struct f_qdss *qdss;
	unsigned long flags;
	int found = 0;

	qdss_log("%s\n", __func__);

	if (!notify) {
		pr_err("%s: notification func is missing\n", __func__);
		return NULL;
	}

	spin_lock_irqsave(&channel_lock, flags);
	/* Check if we already have a channel with this name */
	list_for_each_entry(ch, &usb_qdss_ch_list, list) {
		if (!strcmp(name, ch->name)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		spin_unlock_irqrestore(&channel_lock, flags);
		qdss_log("%s failed as %s not found\n", __func__, name);
		return NULL;
	}

	qdss_log("%s: qdss ctx found\n", __func__);
	qdss = container_of(ch, struct f_qdss, ch);
	ch->priv_usb = qdss;
	ch->priv = priv;
	ch->notify = notify;
	ch->app_conn = 1;
	qdss->qdss_close = false;
	spin_unlock_irqrestore(&channel_lock, flags);

	/* the case USB cabel was connected before qdss called qdss_open */
	if (qdss->usb_connected == 1)
		queue_work(qdss->wq, &qdss->connect_w);

	return ch;
}
EXPORT_SYMBOL(usb_qdss_open);

void usb_qdss_close(struct usb_qdss_ch *ch)
{
	struct f_qdss *qdss = ch->priv_usb;
	struct usb_gadget *gadget;
	unsigned long flags;
	int status;
	struct qdss_req *qreq;

	qdss_log("%s\n", __func__);

	spin_lock_irqsave(&channel_lock, flags);
	if (!qdss)
		goto close;
	qdss->qdss_close = true;
	spin_lock(&qdss->lock);
	while (!list_empty(&qdss->queued_data_pool)) {
		qreq = list_first_entry(&qdss->queued_data_pool,
				struct qdss_req, list);
		spin_unlock(&qdss->lock);
		spin_unlock_irqrestore(&channel_lock, flags);
		qdss_log("dequeue req:%pK\n", qreq->usb_req);
		if (!usb_ep_dequeue(qdss->port.data, qreq->usb_req))
			wait_for_completion(&qreq->write_done);
		spin_lock_irqsave(&channel_lock, flags);
		spin_lock(&qdss->lock);
	}

	spin_unlock(&qdss->lock);
	spin_unlock_irqrestore(&channel_lock, flags);
	usb_qdss_free_req(ch);
	spin_lock_irqsave(&channel_lock, flags);
close:
	ch->priv_usb = NULL;
	ch->notify = NULL;
	if (!qdss || !qdss->usb_connected ||
			!strcmp(qdss->ch.name, USB_QDSS_CH_MDM)) {
		ch->app_conn = 0;
		spin_unlock_irqrestore(&channel_lock, flags);
		return;
	}

	if (qdss->endless_req) {
		spin_unlock_irqrestore(&channel_lock, flags);
		/* Flush connect work before proceeding with de-queue */
		flush_work(&qdss->connect_w);
		usb_ep_dequeue(qdss->port.data, qdss->endless_req);
		spin_lock_irqsave(&channel_lock, flags);
	}
	gadget = qdss->gadget;
	ch->app_conn = 0;
	spin_unlock_irqrestore(&channel_lock, flags);

	status = uninit_data(qdss->port.data);
	if (status)
		pr_err("%s: uninit_data error\n", __func__);

	status = set_qdss_data_connection(qdss, 0);
	if (status)
		pr_err("%s:qdss_disconnect error\n", __func__);
}
EXPORT_SYMBOL(usb_qdss_close);

static void qdss_cleanup(void)
{
	struct f_qdss *qdss;
	struct list_head *act, *tmp;
	struct usb_qdss_ch *_ch;
	unsigned long flags;

	qdss_log("%s\n", __func__);

	list_for_each_safe(act, tmp, &usb_qdss_ch_list) {
		_ch = list_entry(act, struct usb_qdss_ch, list);
		qdss = container_of(_ch, struct f_qdss, ch);
		spin_lock_irqsave(&channel_lock, flags);
		destroy_workqueue(qdss->wq);
		if (!_ch->priv) {
			list_del(&_ch->list);
			kfree(qdss);
		}
		spin_unlock_irqrestore(&channel_lock, flags);
	}
}

static void qdss_free_func(struct usb_function *f)
{
	struct f_qdss *qdss = func_to_qdss(f);

	qdss->debug_inface_enabled = false;
}

static inline struct usb_qdss_opts *to_f_qdss_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct usb_qdss_opts,
			func_inst.group);
}

static void qdss_attr_release(struct config_item *item)
{
	struct usb_qdss_opts *opts = to_f_qdss_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations qdss_item_ops = {
	.release	= qdss_attr_release,
};

static ssize_t qdss_enable_debug_inface_show(struct config_item *item,
			char *page)
{
	return snprintf(page, PAGE_SIZE, "%s\n",
		(to_f_qdss_opts(item)->usb_qdss->debug_inface_enabled) ?
		"Enabled" : "Disabled");
}

static ssize_t qdss_enable_debug_inface_store(struct config_item *item,
			const char *page, size_t len)
{
	struct f_qdss *qdss = to_f_qdss_opts(item)->usb_qdss;
	unsigned long flags;
	u8 stats;

	if (page == NULL) {
		pr_err("Invalid buffer\n");
		return len;
	}

	if (kstrtou8(page, 0, &stats) != 0 && (stats != 0 || stats != 1)) {
		pr_err("(%u)Wrong value. enter 0 to disable or 1 to enable.\n",
			stats);
		return len;
	}

	spin_lock_irqsave(&qdss->lock, flags);
	qdss->debug_inface_enabled = (stats == 1 ? true : false);
	spin_unlock_irqrestore(&qdss->lock, flags);
	return len;
}

CONFIGFS_ATTR(qdss_, enable_debug_inface);
static struct configfs_attribute *qdss_attrs[] = {
	&qdss_attr_enable_debug_inface,
	NULL,
};

static struct config_item_type qdss_func_type = {
	.ct_item_ops	= &qdss_item_ops,
	.ct_attrs	= qdss_attrs,
	.ct_owner	= THIS_MODULE,
};

static void usb_qdss_free_inst(struct usb_function_instance *fi)
{
	struct usb_qdss_opts *opts;

	opts = container_of(fi, struct usb_qdss_opts, func_inst);
	kfree(opts->usb_qdss);
	kfree(opts);
}

static int usb_qdss_set_inst_name(struct usb_function_instance *f,
				const char *name)
{
	struct usb_qdss_opts *opts =
		container_of(f, struct usb_qdss_opts, func_inst);
	char *ptr;
	size_t name_len;
	struct f_qdss *usb_qdss;

	/* get channel_name as expected input qdss.<channel_name> */
	name_len = strlen(name) + 1;
	if (name_len > 15)
		return -ENAMETOOLONG;

	/* get channel name */
	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr) {
		pr_err("error:%ld\n", PTR_ERR(ptr));
		return -ENOMEM;
	}

	opts->channel_name = ptr;
	qdss_log("qdss: channel_name:%s\n", opts->channel_name);

	usb_qdss = alloc_usb_qdss(opts->channel_name);
	if (IS_ERR(usb_qdss)) {
		pr_err("Failed to create usb_qdss port(%s)\n",
				opts->channel_name);
		return -ENOMEM;
	}

	opts->usb_qdss = usb_qdss;
	return 0;
}

static struct usb_function_instance *qdss_alloc_inst(void)
{
	struct usb_qdss_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	opts->func_inst.free_func_inst = usb_qdss_free_inst;
	opts->func_inst.set_inst_name = usb_qdss_set_inst_name;

	config_group_init_type_name(&opts->func_inst.group, "",
				    &qdss_func_type);
	return &opts->func_inst;
}

static struct usb_function *qdss_alloc(struct usb_function_instance *fi)
{
	struct usb_qdss_opts *opts = to_fi_usb_qdss_opts(fi);
	struct f_qdss *usb_qdss = opts->usb_qdss;

	usb_qdss->port.function.name = "usb_qdss";
	usb_qdss->port.function.strings = qdss_strings;
	usb_qdss->port.function.bind = qdss_bind;
	usb_qdss->port.function.unbind = qdss_unbind;
	usb_qdss->port.function.set_alt = qdss_set_alt;
	usb_qdss->port.function.disable = qdss_disable;
	usb_qdss->port.function.setup = NULL;
	usb_qdss->port.function.free_func = qdss_free_func;

	return &usb_qdss->port.function;
}

DECLARE_USB_FUNCTION(qdss, qdss_alloc_inst, qdss_alloc);
static int __init usb_qdss_init(void)
{
	int ret;

	_qdss_ipc_log = ipc_log_context_create(NUM_PAGES, "usb_qdss", 0);
	if (IS_ERR_OR_NULL(_qdss_ipc_log))
		_qdss_ipc_log =  NULL;

	INIT_LIST_HEAD(&usb_qdss_ch_list);
	ret = usb_function_register(&qdssusb_func);
	if (ret) {
		pr_err("%s: failed to register diag %d\n", __func__, ret);
		return ret;
	}
	return ret;
}

static void __exit usb_qdss_exit(void)
{
	ipc_log_context_destroy(_qdss_ipc_log);
	usb_function_unregister(&qdssusb_func);
	qdss_cleanup();
}

module_init(usb_qdss_init);
module_exit(usb_qdss_exit);
MODULE_DESCRIPTION("USB QDSS Function Driver");
MODULE_LICENSE("GPL v2");
