/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2013, 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_USB_QDSS_H
#define __LINUX_USB_QDSS_H

#include <linux/kernel.h>

#define USB_QDSS_CH_MDM	"qdss_mdm"
#define USB_QDSS_CH_MSM	"qdss"

struct qdss_request {
	char *buf;
	int length;
	int actual;
	int status;
	void *context;
	struct scatterlist *sg;
	unsigned int num_sgs;
	unsigned int num_mapped_sgs;
};

struct usb_qdss_ch {
	const char *name;
	struct list_head list;
	void (*notify)(void *priv, unsigned int event,
		struct qdss_request *d_req, struct usb_qdss_ch *ch);
	void *priv;
	void *priv_usb;
	int app_conn;
};

enum qdss_state {
	USB_QDSS_CONNECT,
	USB_QDSS_DISCONNECT,
	USB_QDSS_CTRL_READ_DONE,
	USB_QDSS_DATA_WRITE_DONE,
	USB_QDSS_CTRL_WRITE_DONE,
};

#if IS_ENABLED(CONFIG_USB_F_QDSS)
struct usb_qdss_ch *usb_qdss_open(const char *name, void *priv,
	void (*notify)(void *priv, unsigned int event,
		struct qdss_request *d_req, struct usb_qdss_ch *ch));
void usb_qdss_close(struct usb_qdss_ch *ch);
int usb_qdss_alloc_req(struct usb_qdss_ch *ch, int n_write, int n_read);
void usb_qdss_free_req(struct usb_qdss_ch *ch);
int usb_qdss_read(struct usb_qdss_ch *ch, struct qdss_request *d_req);
int usb_qdss_write(struct usb_qdss_ch *ch, struct qdss_request *d_req);
int usb_qdss_ctrl_write(struct usb_qdss_ch *ch, struct qdss_request *d_req);
int usb_qdss_ctrl_read(struct usb_qdss_ch *ch, struct qdss_request *d_req);
#else
static inline struct usb_qdss_ch *usb_qdss_open(const char *name, void *priv,
		void (*n)(void *, unsigned int event,
		struct qdss_request *d, struct usb_qdss_ch *c))
{
	return ERR_PTR(-ENODEV);
}

static inline int usb_qdss_read(struct usb_qdss_ch *c, struct qdss_request *d)
{
	return -ENODEV;
}

static inline int usb_qdss_write(struct usb_qdss_ch *c, struct qdss_request *d)
{
	return -ENODEV;
}

static inline int usb_qdss_ctrl_write(struct usb_qdss_ch *c,
		struct qdss_request *d)
{
	return -ENODEV;
}

static inline int usb_qdss_ctrl_read(struct usb_qdss_ch *c,
		struct qdss_request *d)
{
	return -ENODEV;
}
static inline int usb_qdss_alloc_req(struct usb_qdss_ch *c, int n_wr, int n_rd)
{
	return -ENODEV;
}


static inline void usb_qdss_close(struct usb_qdss_ch *ch) { }

static inline void usb_qdss_free_req(struct usb_qdss_ch *ch) { }
#endif /* CONFIG_USB_F_QDSS */

#endif
