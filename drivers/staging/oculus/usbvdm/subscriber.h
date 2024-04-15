/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __USBVDM_SUBSCRIBER_H
#define __USBVDM_SUBSCRIBER_H

#include <linux/device.h>

struct usbvdm_subscription;

struct usbvdm_subscriber_ops {
	void (*connect)(struct usbvdm_subscription *sub, u16 svid, u16 pid);
	void (*disconnect)(struct usbvdm_subscription *sub);
	void (*vdm)(struct usbvdm_subscription *sub,
		u32 vdm_hdr, const u32 *vdos, int num_vdos);
	void (*ext_msg)(struct usbvdm_subscription *sub,
		u8 msg_type, const u8 *data, size_t data_len);
};

#if IS_ENABLED(CONFIG_META_USBVDM)
struct usbvdm_subscription *usbvdm_subscribe(struct device *dev,
		u16 svid, u16 pid, struct usbvdm_subscriber_ops ops);
void usbvdm_unsubscribe(struct usbvdm_subscription *sub);
int usbvdm_subscriber_vdm(struct usbvdm_subscription *sub,
		u32 vdm_hdr, const u32 *vdos, int num_vdos);
int usbvdm_subscriber_ext_msg(struct usbvdm_subscription *sub,
		u8 msg_type, const u8 *data, size_t data_len);
void usbvdm_subscriber_set_drvdata(struct usbvdm_subscription *sub, void *priv);
void *usbvdm_subscriber_get_drvdata(struct usbvdm_subscription *sub);
#else
inline struct usbvdm_subscription *usbvdm_subscribe(struct device *dev,
		u16 svid, u16 pid, struct usbvdm_subscriber_ops ops)
{
	return ERR_PTR(-EOPNOTSUPP);
}
inline void usbvdm_unsubscribe(struct usbvdm_subscription *sub) { }
inline int usbvdm_subscriber_ext_msg(struct usbvdm_subscription *sub,
		u8 msg_type, const u8 *data, size_t data_len) { }
inline int usbvdm_subscriber_vdm(struct usbvdm_subscription *sub,
		u32 vdm_hdr, const u32 *vdos, int num_vdos) { return -EOPNOTSUPP; };
inline void usbvdm_subscriber_set_drvdata(struct usbvdm_subscription *sub,
		void *priv) { }
inline void *usbvdm_subscriber_get_drvdata(struct usbvdm_subscription *sub)
{
		return ERR_PTR(-EOPNOTSUPP);
}
#endif /* CONFIG_META_USBVDM */

#endif /* __USBVDM_SUBSCRIBER_H */
