// SPDX-License-Identifier: GPL-2.0

#include <linux/cypd.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "engine.h"

#define to_handler_data(h) container_of(h, struct cypd_svid_handler_data, hdlr)

struct usbvdm_dev {
	struct device *dev;
	struct usbvdm_engine *engine;

	struct cypd *cypd;
	struct list_head handler_list;
	struct mutex handler_lock;
};

struct cypd_svid_handler_data {
	struct cypd_svid_handler hdlr;

	struct usbvdm_dev *uv_dev;
	struct list_head entry;
};

void usbvdm_cypd_connect(struct cypd_svid_handler *hdlr)
{
	struct cypd_svid_handler_data *hdlr_data = to_handler_data(hdlr);

	usbvdm_connect(hdlr_data->uv_dev->engine, hdlr->svid, hdlr->pid);
}

void usbvdm_cypd_disconnect(struct cypd_svid_handler *hdlr)
{
	struct cypd_svid_handler_data *hdlr_data = to_handler_data(hdlr);

	usbvdm_disconnect(hdlr_data->uv_dev->engine);
}

void usbvdm_cypd_vdm_received(struct cypd_svid_handler *hdlr,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	struct cypd_svid_handler_data *hdlr_data = to_handler_data(hdlr);

	usbvdm_engine_vdm(hdlr_data->uv_dev->engine, vdm_hdr, vdos, num_vdos);
}

void usbvdm_cypd_subscribe(struct usbvdm_engine *engine, u16 svid, u16 pid)
{
	struct usbvdm_dev *uv_dev = usbvdm_engine_get_drvdata(engine);
	struct cypd_svid_handler_data *hdlr_data;
	struct cypd_svid_handler hdlr = {
		.svid = svid,
		.pid = pid,
		.connect = usbvdm_cypd_connect,
		.disconnect = usbvdm_cypd_disconnect,
		.vdm_received = usbvdm_cypd_vdm_received
	};

	hdlr_data = devm_kzalloc(uv_dev->dev, sizeof(*hdlr_data), GFP_KERNEL);
	if (!hdlr_data)
		return;

	hdlr_data->uv_dev = uv_dev;
	hdlr_data->hdlr = hdlr;
	if (cypd_register_svid(uv_dev->cypd, &hdlr_data->hdlr)) {
		devm_kfree(uv_dev->dev, hdlr_data);
		return;
	}

	mutex_lock(&uv_dev->handler_lock);
	list_add(&hdlr_data->entry, &uv_dev->handler_list);
	mutex_unlock(&uv_dev->handler_lock);

	dev_dbg(uv_dev->dev, "Subscribed SVID/PID: 0x%04x/0x%04x", svid, pid);
}

void usbvdm_cypd_unsubscribe(struct usbvdm_engine *engine, u16 svid, u16 pid)
{
	struct usbvdm_dev *uv_dev = usbvdm_engine_get_drvdata(engine);
	struct cypd_svid_handler_data *pos, *tmp, *hdlr_data = NULL;

	mutex_lock(&uv_dev->handler_lock);
	list_for_each_entry_safe(pos, tmp, &uv_dev->handler_list, entry) {
		if (pos->hdlr.svid == svid && pos->hdlr.pid == pid) {
			hdlr_data = pos;
			list_del(&hdlr_data->entry);
			break;
		}
	}
	mutex_unlock(&uv_dev->handler_lock);

	if (!hdlr_data)
		return;

	cypd_unregister_svid(uv_dev->cypd, &hdlr_data->hdlr);
	devm_kfree(uv_dev->dev, hdlr_data);

	dev_dbg(uv_dev->dev, "Unsubscribed SVID/PID: 0x%04x/0x%04x", svid, pid);
}

int usbvdm_cypd_vdm(struct usbvdm_engine *engine,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos)
{
	struct usbvdm_dev *uv_dev = usbvdm_engine_get_drvdata(engine);

	return cypd_send_vdm(uv_dev->cypd, vdm_hdr, vdos, num_vdos);
}

static const struct usbvdm_engine_ops usbvdm_cypd_ops = {
	.subscribe = usbvdm_cypd_subscribe,
	.unsubscribe = usbvdm_cypd_unsubscribe,
	.vdm = usbvdm_cypd_vdm
};

static int usbvdm_cypd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usbvdm_dev *uv_dev;
	int rc;

	uv_dev = devm_kzalloc(dev, sizeof(*uv_dev), GFP_KERNEL);
	if (!uv_dev)
		return -ENOMEM;

	INIT_LIST_HEAD(&uv_dev->handler_list);
	mutex_init(&uv_dev->handler_lock);

	uv_dev->dev = dev;
	platform_set_drvdata(pdev, uv_dev);

	uv_dev->cypd = devm_cypd_get_by_phandle(dev, "cy,cypd");
	if (IS_ERR_OR_NULL(uv_dev->cypd)) {
		rc = PTR_ERR(uv_dev->cypd);
		dev_err_probe(&pdev->dev, rc,
				"devm_cypd_get_by_phandle failed: %d", rc);
		return rc;
	}

	uv_dev->engine = usbvdm_register(dev, usbvdm_cypd_ops, uv_dev);
	if (IS_ERR_OR_NULL(uv_dev->engine)) {
		rc = PTR_ERR(uv_dev->engine);
		dev_err_probe(uv_dev->dev, rc,
				"Error in registering with usbvdm, rc=%d", rc);
		return rc;
	}

	return 0;
}

static int usbvdm_cypd_remove(struct platform_device *pdev)
{
	struct cypd_svid_handler_data *pos;
	struct usbvdm_dev *uv_dev = platform_get_drvdata(pdev);

	mutex_lock(&uv_dev->handler_lock);
	list_for_each_entry(pos, &uv_dev->handler_list, entry)
		cypd_unregister_svid(uv_dev->cypd, &pos->hdlr);
	mutex_unlock(&uv_dev->handler_lock);

	usbvdm_unregister(uv_dev->engine);
	mutex_destroy(&uv_dev->handler_lock);

	return 0;
}

static const struct of_device_id usbvdm_cypd_match_table[] = {
	{.compatible = "meta,usbvdm-cypd"},
	{},
};

static struct platform_driver usbvdm_cypd_driver = {
	.driver	= {
		.name = "usbvdm_cypd",
		.of_match_table = usbvdm_cypd_match_table,
	},
	.probe	= usbvdm_cypd_probe,
	.remove	= usbvdm_cypd_remove,
};

module_platform_driver(usbvdm_cypd_driver);

MODULE_DESCRIPTION("Meta USBVDM CYPD driver");
MODULE_LICENSE("GPL v2");
