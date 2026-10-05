// SPDX-License-Identifier: GPL+
/*****************************************************************************
 * @file arfw_jazz_usb_host_main.c
 *
 * @brief usb device (host) driver for arfw jazz prototyping
 *
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include <ar_common.h>
#include <arfw_shim.h>
#include <arfw_log.h>

#include "arfw_usb_int.h"
#include "arfw_usb_ctrl.h"
#include "arfw_usb_queue.h"
#include "arfw_usb_ops.h"

static void arfw_usb_cdev_exit(struct arfw_usb_driver *driver)
{
	int err;
	char intf_name[ARFW_DEVICE_ID_MAX_LEN];
	size_t intf_idx;

	AR_ASSERT(driver);
	AR_ASSERT(driver->intf);

	for (intf_idx = 0; intf_idx < ARFW_USB_INTF_SYS_MAX; intf_idx++) {
		if (!driver->intf_sys[intf_idx])
			continue;
		snprintf(intf_name, ARFW_DEVICE_ID_MAX_LEN, "%s%s",
			 ARFW_USB_INTF_PREFIX, driver->intf_sys[intf_idx]);
		err = arfw_shim_cdev_unregister(intf_name,
						/* hw_device = */ NULL,
						/* module_remove = */ true);
		if (err)
			AR_LOG_USB_DEV_INFO(
				&driver->intf->dev, AR_LOG_SHUTDOWN,
				"Failed to unregister client cdev=/dev/%s, err=%d!",
				intf_name, err);
	}
}

static int arfw_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
	int err;
	struct arfw_usb_driver *driver;
	struct device *sysdev;
	char intf_name[ARFW_DEVICE_ID_MAX_LEN];
	size_t intf_name_len, intf_idx = 0;
	char *intf_desc, *intf_token;

	AR_ASSERT(intf);
	sysdev = interface_to_usbdev(intf)->bus->sysdev;
	AR_ASSERT(sysdev);
	AR_ASSERT(id);

	// This happens if firmware is misconfigured.
	// Our protocol specifies that we extract names from the interface strings.
	// These names are used for character devices, but no need to crash - fail probe.
	if (!intf->cur_altsetting || !intf->cur_altsetting->string) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT, "No intf strings");
		err = -EINVAL;
		goto error_intf_strings;
	}

	driver = devm_kzalloc(&intf->dev, sizeof(*driver), GFP_KERNEL);
	if (!driver) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT,
				   "Failed to allocate driver");
		err = -ENOMEM;
		goto error_driver_alloc;
	}

	driver->intf = intf;
	usb_set_intfdata(intf, driver);
	INIT_DELAYED_WORK(&driver->reset_work, arfw_usb_sys_reset_offload);

	// For now use one single threaded workqueue per interface/driver.
	// Means each subsystem gets a single completion thread with normal policy.
	// This we should revisit once we start tweaking/optimizing performance.
	// TODO(T230243591): consider using threads with RT policy here and more than one.
	driver->offload_queue = alloc_ordered_workqueue(
		"%s%d", WQ_HIGHPRI, ARFW_USB_OFFLOAD_QUEUE_PREFIX,
		intf->cur_altsetting->desc.bInterfaceNumber);
	if (!driver->offload_queue) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT,
				   "Failed to allocate driver offload queue");
		err = -ENOMEM;
		goto error_offload_queue_alloc;
	}

	// Copy the interface descriptor since strsep mutates the string.
	intf_desc = devm_kstrdup(&intf->dev, intf->cur_altsetting->string,
				 GFP_KERNEL);
	if (!intf_desc) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT,
				   "Failed to copy interface descriptor");
		err = -ENOMEM;
		goto error_copy_intf_desc;
	}
	driver->intf_desc = intf_desc;

	// Init control endpoints before we attempt handshake.
	// No need to init data endpoints at this point.
	err = arfw_usb_ctrl_init(driver);
	if (err) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT,
				   "Failed to init ctrl err=%d", err);
		goto error_ctrl_init;
	}

	AR_LOG_USB_DEV_INFO(&intf->dev, AR_LOG_INIT,
			    "Configured ctrl eps: in=%d, out=%d",
			    driver->ep_ctrl_in->num, driver->ep_ctrl_out->num);

	// This is our handshake with firmware, exchange info messages.
	// We save info from it into the driver struct, so all fields are accessible.
	// The fields will not change in runtime.
	err = arfw_usb_ops_fetch_info(driver);
	if (err) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT,
				   "Failed to handshake with info err=%d", err);
		goto error_info_handshake;
	}

	AR_LOG_USB_DEV_INFO(
		&intf->dev, AR_LOG_INIT,
		"Fetched firmware info: "
		"ver=%d.%d, rings_max=%d, inline_max=0x%x, inline_eps=0x%x",
		driver->cp_version.major, driver->cp_version.minor,
		driver->cp_rings_max, driver->cp_inline_max,
		driver->cp_inline_eps);

	// At this point it is safe to init data endpoints.
	// We should also do this before we spin flow control messages.
	err = arfw_usb_queue_init(driver);
	if (err) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT,
				   "Failed to init queues err=%d", err);
		goto error_queue_init;
	}

	// This needs to happen as the last step of host init.
	// Flow control via room sync uses all parts of the system (ctrl, data).
	// Hence needs those initialized before we can spin it.
	err = arfw_usb_ctrl_sync_room_init(driver);
	if (err) {
		AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT,
				   "Failed to init room sync err=%d", err);
		goto error_sync_room;
	}

	// Iterate over all the subsystems under this interface.
	// We create a cdev/client for each, this way routing is static.
	// Even if we decide to change things up.
	while ((intf_token = strsep(&intf_desc, ","))) {
		if (intf_idx >= ARFW_USB_INTF_SYS_MAX) {
			AR_LOG_USB_DEV_ERR(
				&intf->dev, AR_LOG_INIT,
				"Too many subsystems per interface, max=%d",
				ARFW_USB_INTF_SYS_MAX);
			err = -ENOMEM;
			goto error_client_register;
		}
		intf_name_len =
			sizeof(ARFW_USB_INTF_PREFIX) + strlen(intf_token);
		if (intf_name_len >= ARFW_DEVICE_ID_MAX_LEN) {
			AR_LOG_USB_DEV_ERR(
				&intf->dev, AR_LOG_INIT,
				"Interface name is too long to register max=%d, got=%zu",
				ARFW_DEVICE_ID_MAX_LEN, intf_name_len);
			err = -ENOMEM;
			goto error_client_register;
		}
		snprintf(intf_name, ARFW_DEVICE_ID_MAX_LEN, "%s%s",
			 ARFW_USB_INTF_PREFIX, intf_token);
		err = arfw_shim_cdev_register(sysdev, intf_name,
					      arfw_usb_ops_get(),
					      &driver->client_ops, intf);
		if (err) {
			AR_LOG_USB_DEV_ERR(
				&intf->dev, AR_LOG_INIT,
				"Failed to register client cdev=/dev/%s, err=%d",
				intf_name, err);
			goto error_client_register;
		}
		AR_ASSERT(driver->client_ops);
		driver->intf_sys[intf_idx++] = intf_token;
		AR_LOG_USB_DEV_DBG(&intf->dev, AR_LOG_INIT,
				   "Registered client cdev=/dev/%s", intf_name);
	}

	AR_LOG_USB_DEV_INFO(&intf->dev, AR_LOG_INIT,
			    "Probed successfully ver=%d.%d",
			    driver->ap_version.major, driver->ap_version.minor);

	return 0;

error_client_register:
	arfw_usb_cdev_exit(driver);
error_sync_room:
	arfw_usb_queue_exit(driver);
error_queue_init:
error_info_handshake:
	arfw_usb_ctrl_exit(driver);
error_ctrl_init:
	devm_kfree(&intf->dev, driver->intf_desc);
error_copy_intf_desc:
	destroy_workqueue(driver->offload_queue);
	driver->offload_queue = NULL;
error_offload_queue_alloc:
	devm_kfree(&intf->dev, driver);
	driver = NULL;
error_driver_alloc:
error_intf_strings:
	AR_LOG_USB_DEV_ERR(&intf->dev, AR_LOG_INIT, "Failed to probe, err=%d",
			   err);

	return err;
}

static void arfw_usb_disconnect(struct usb_interface *intf)
{
	struct arfw_usb_driver *driver;
	bool was_reset;

	AR_ASSERT(intf);
	AR_ASSERT(intf->cur_altsetting);
	AR_ASSERT(intf->cur_altsetting->string);
	driver = usb_get_intfdata(intf);
	AR_ASSERT(driver);
	was_reset = atomic_read(&driver->reset);

	// Unregister generic first to prevent new requests.
	// The existing clients will wait to release the open files.
	arfw_usb_cdev_exit(driver);

	// Cancel any pending system reset work before tearing down.
	// This must happen before we free any resources the reset work touches.
	cancel_delayed_work_sync(&driver->reset_work);

	// Release the copied interface descriptor string.
	// strsep mutated the local copy, so intf_sys[] tokens point into it
	// but we saved the original allocation pointer in intf_desc.
	if (driver->intf_desc)
		devm_kfree(&driver->intf->dev, driver->intf_desc);

	arfw_usb_queue_exit(driver);
	arfw_usb_ctrl_exit(driver);

	destroy_workqueue(driver->offload_queue);
	driver->offload_queue = NULL;
	devm_kfree(&intf->dev, driver);

	// Queue a device reset here (driver removed).
	// This ensures we do not have any stale state on CP side.
	// And lines up with how we start things in PCIe as well.
	// Only needed if this was not triggered by the system reset.
	// Cannot call usb_reset_device directly from disconnect context
	// since USB core holds device/interface locks that usb_reset_device
	// would try to reacquire, risking deadlock.
	if (!was_reset)
		usb_queue_reset_device(intf);

	AR_LOG_USB_DEV_INFO(&intf->dev, AR_LOG_SHUTDOWN,
			    "Disconnected successfully");
}

static const struct usb_device_id arfw_usb_table[] = {
	{ USB_DEVICE(META_USB_VENDOR_ID, META_USB_PRODUCT_ID) },
	{},
};

static struct usb_driver arfw_usb_driver = {
	.name = "ARFW Jazz USB Device",
	.id_table = arfw_usb_table,
	.probe = arfw_usb_probe,
	.disconnect = arfw_usb_disconnect,
};

MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(usb, arfw_usb_table);
module_usb_driver(arfw_usb_driver);
