/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __USBVDM_ENGINE_H
#define __USBVDM_ENGINE_H

#include <linux/device.h>

struct usbvdm_engine;

struct usbvdm_engine_ops {
	void (*subscribe)(struct usbvdm_engine *engine, u16 svid, u16 pid);
	void (*unsubscribe)(struct usbvdm_engine *engine, u16 svid, u16 pid);
	int (*vdm)(struct usbvdm_engine *engine,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos);
};

#if IS_ENABLED(CONFIG_META_USBVDM)
struct usbvdm_engine *usbvdm_register(struct device *dev,
		struct usbvdm_engine_ops ops, void *drvdata);
void usbvdm_unregister(struct usbvdm_engine *engine);

void usbvdm_connect(struct usbvdm_engine *engine,
		u16 svid, u16 pid);
void usbvdm_disconnect(struct usbvdm_engine *engine);
void usbvdm_engine_vdm(struct usbvdm_engine *engine,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos);

void usbvdm_engine_set_drvdata(struct usbvdm_engine *engine, void *priv);
void *usbvdm_engine_get_drvdata(struct usbvdm_engine *engine);
#else
inline struct usbvdm_engine *usbvdm_register(struct device *dev,
		struct usbvdm_engine_ops ops, void *drvdata)
{
		return ERR_PTR(-EOPNOTSUPP)
}
inline void usbvdm_unregister(struct usbvdm_engine *engine) { }

inline void usbvdm_connect(struct usbvdm_engine *engine,
		u16 svid, u16 pid) { }
inline void usbvdm_disconnect(struct usbvdm_engine *engine) { }
inline void usbvdm_engine_vdm(struct usbvdm_engine *engine,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos) { }

inline void usbvdm_engine_set_drvdata(struct usbvdm_engine *engine,
		void *priv) { }
inline void *usbvdm_engine_get_drvdata(struct usbvdm_engine *engine)
{
		return ERR_PTR(-EOPNOTSUPP);
}
#endif /* CONFIG_META_USBVDM */

#endif /* __USBVDM_ENGINE_H */
