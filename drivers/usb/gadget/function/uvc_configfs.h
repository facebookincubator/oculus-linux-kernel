// SPDX-License-Identifier: GPL-2.0
/*
 * uvc_configfs.h
 *
 * Configfs support for the uvc function.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 */
#ifndef UVC_CONFIGFS_H
#define UVC_CONFIGFS_H

struct f_uvc_opts;

/* -----------------------------------------------------------------------------
 * control/extensions/<NAME>
 */

struct uvcg_extension_unit_descriptor {
       u8 bLength;
       u8 bDescriptorType;
       u8 bDescriptorSubType;
       u8 bUnitID;
       u8 guidExtensionCode[16];
       u8 bNumControls;
       u8 bNrInPins;
       u8 *baSourceID;
       u8 bControlSize;
       u8 *bmControls;
       u8 iExtension;
} __packed;

struct uvcg_extension {
       struct config_item item;
       struct list_head list;
       u8 string_descriptor_index;
       struct uvcg_extension_unit_descriptor desc;
};

static inline struct uvcg_extension *to_uvcg_extension(struct config_item *item)
{
       return container_of(item, struct uvcg_extension, item);
}

int uvcg_attach_configfs(struct f_uvc_opts *opts);

#endif /* UVC_CONFIGFS_H */
