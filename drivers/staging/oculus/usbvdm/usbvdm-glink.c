// SPDX-License-Identifier: GPL-2.0

#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/slab.h>
#include <linux/usb/pd_vdo.h>

#include "engine.h"

#define PMIC_GLINK_MSG_OWNER_OEM 32782

#define MSG_TYPE_REQ_RESP	1
#define MSG_TYPE_NOTIFY		2

#define OEM_OPCODE_SEND_VDM	0x10002
#define OEM_OPCODE_RECV_VDM	0x10003
#define OEM_OPCODE_NOTIFY	0x10004
#define OEM_OPCODE_SEND_EXT_MSG	0x10005
#define OEM_OPCODE_RECV_EXT_MSG	0x10006

#define OEM_NOTIFICATION_CONNECTED		0x1
#define OEM_NOTIFICATION_DISCONNECTED	0x2

#define PD_MAX_EXTENDED_MSG_LEN	260

struct usbvdm_dev {
	struct device *dev;
	struct usbvdm_engine *engine;

	struct pmic_glink_client *client;
	enum pmic_glink_state state;
	struct mutex state_lock;

	struct work_struct setup_work;
};

struct ext_msg_work {
	u8 msg_type;
	u8 data[PD_MAX_EXTENDED_MSG_LEN];
	u32 data_len;

	struct usbvdm_dev *uv_dev;
	struct work_struct work;
};

struct vdm_work {
	u32 vdm_hdr;
	u32 vdos[VDO_MAX_SIZE];
	u32 num_vdos;

	struct usbvdm_dev *uv_dev;
	struct work_struct work;
};

/* As defined in DSP */
struct pmic_glink_oem_notify_msg {
	struct pmic_glink_hdr hdr;

	u32			notification;
	u32			receiver;
	u16			svid;
	u16			pid;
	u32			reserved;
};

/* As defined in DSP */
struct pmic_glink_oem_ext_msg {
	struct pmic_glink_hdr hdr;

	u8 msg_type;
	u8 data[PD_MAX_EXTENDED_MSG_LEN];
	u32 data_len;
};

/* As defined in DSP */
struct pmic_glink_oem_vdm_msg {
	struct pmic_glink_hdr hdr;

	u16 svid;
	u16 pid;
	u32 data[VDO_MAX_SIZE];
	u32 size;
};

static void usbvdm_glink_ext_msg_work(struct work_struct *work)
{
	struct ext_msg_work *em_work = container_of(work, struct ext_msg_work, work);
	struct usbvdm_dev *uv_dev = em_work->uv_dev;
	struct pmic_glink_hdr msg_hdr = {};
	struct pmic_glink_oem_ext_msg msg = {};
	int rc;

	mutex_lock(&uv_dev->state_lock);

	if (uv_dev->state == PMIC_GLINK_STATE_DOWN) {
		dev_err(uv_dev->dev, "Can't send ext msg, Glink down");
		goto out;
	}

	msg_hdr.owner = PMIC_GLINK_MSG_OWNER_OEM;
	msg_hdr.type = MSG_TYPE_REQ_RESP;
	msg_hdr.opcode = OEM_OPCODE_SEND_EXT_MSG;

	msg.hdr = msg_hdr;
	msg.msg_type = em_work->msg_type;
	memcpy(msg.data, em_work->data, em_work->data_len);
	msg.data_len = em_work->data_len;

	rc = pmic_glink_write(uv_dev->client, &msg, sizeof(msg));
	if (rc) {
		dev_err(uv_dev->dev, "Failed writing to pmic_glink, rc=%d", rc);
		goto out;
	}

out:
	mutex_unlock(&uv_dev->state_lock);
	kfree(em_work);
}

static int usbvdm_glink_ext_msg(struct usbvdm_engine *engine,
		u8 msg_type, const u8 *data, size_t data_len)
{
	struct usbvdm_dev *uv_dev = usbvdm_engine_get_drvdata(engine);

	struct ext_msg_work *em_work = kzalloc(sizeof(*em_work), GFP_KERNEL);
	if (!em_work)
		return -ENOMEM;

	INIT_WORK(&em_work->work, usbvdm_glink_ext_msg_work);
	em_work->uv_dev = uv_dev;
	em_work->msg_type = msg_type;
	memcpy(em_work->data, data, data_len);
	em_work->data_len = data_len;

	schedule_work(&em_work->work);
	return 0;
}

static void usbvdm_glink_vdm_work(struct work_struct *work)
{
	struct vdm_work *vwork = container_of(work, struct vdm_work, work);
	struct usbvdm_dev *uv_dev = vwork->uv_dev;
	struct pmic_glink_hdr msg_hdr = {};
	struct pmic_glink_oem_vdm_msg msg = {};
	u32 vdm_hdr = vwork->vdm_hdr;
	u32 *vdos = vwork->vdos;
	int num_vdos = vwork->num_vdos;
	int rc;

	mutex_lock(&uv_dev->state_lock);

	if (uv_dev->state == PMIC_GLINK_STATE_DOWN) {
		dev_err(uv_dev->dev, "Can't send vdm, Glink down");
		goto out;
	}

	msg_hdr.owner = PMIC_GLINK_MSG_OWNER_OEM;
	msg_hdr.type = MSG_TYPE_REQ_RESP;
	msg_hdr.opcode = OEM_OPCODE_SEND_VDM;

	msg.hdr = msg_hdr;
	msg.data[0] = vdm_hdr;
	memcpy(&msg.data[1], vdos, num_vdos * sizeof(*vdos));
	msg.size = num_vdos + 1;

	rc = pmic_glink_write(uv_dev->client, &msg, sizeof(msg));
	if (rc) {
		dev_err(uv_dev->dev, "Failed writing to pmic_glink, rc=%d", rc);
		goto out;
	}

out:
	mutex_unlock(&uv_dev->state_lock);
	kfree(vwork);
}

static int usbvdm_glink_vdm(struct usbvdm_engine *engine,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos)
{
	struct usbvdm_dev *uv_dev = usbvdm_engine_get_drvdata(engine);

	struct vdm_work *vwork = kzalloc(sizeof(*vwork), GFP_KERNEL);
	if (!vwork)
		return -ENOMEM;

	INIT_WORK(&vwork->work, usbvdm_glink_vdm_work);
	vwork->uv_dev = uv_dev;
	vwork->vdm_hdr = vdm_hdr;
	memcpy(vwork->vdos, vdos, num_vdos * sizeof(*vdos));
	vwork->num_vdos = num_vdos;

	schedule_work(&vwork->work);
	return 0;
}

static const struct usbvdm_engine_ops usbvdm_glink_ops = {
	.ext_msg = usbvdm_glink_ext_msg,
	.vdm = usbvdm_glink_vdm
};

static int usbvdm_glink_setup(struct usbvdm_dev *uv_dev)
{
	mutex_lock(&uv_dev->state_lock);
	uv_dev->engine = usbvdm_register(uv_dev->dev, usbvdm_glink_ops, uv_dev);
	if (IS_ERR(uv_dev->engine)) {
		dev_err(uv_dev->dev, "Error in registering with usbvdm, rc=%ld",
				PTR_ERR(uv_dev->engine));
		mutex_unlock(&uv_dev->state_lock);
		return PTR_ERR(uv_dev->engine);
	}
	mutex_unlock(&uv_dev->state_lock);

	return 0;
}

static void usbvdm_glink_setup_work(struct work_struct *work)
{
	struct usbvdm_dev *uv_dev = container_of(work, struct usbvdm_dev,
			setup_work);

	usbvdm_glink_setup(uv_dev);
}

static void usbvdm_glink_handle_notify(struct usbvdm_dev *uv_dev,
		void *data, size_t len)
{
	struct pmic_glink_oem_notify_msg *msg = data;

	if (len != sizeof(*msg)) {
		dev_err(uv_dev->dev, "Incorrect len received: %lu (exp. %lu)",
				len, sizeof(*msg));
		return;
	}

	switch (msg->notification) {
	case OEM_NOTIFICATION_CONNECTED:
		usbvdm_connect(uv_dev->engine, msg->svid, msg->pid);
		break;
	case OEM_NOTIFICATION_DISCONNECTED:
		usbvdm_disconnect(uv_dev->engine);
		break;
	default:
		dev_err(uv_dev->dev, "Unable to handle notification type %d",
				msg->notification);
		break;
	}
}

static void usbvdm_glink_handle_recv_ext_msg(struct usbvdm_dev *uv_dev,
		void *data, size_t len)
{
	struct pmic_glink_oem_ext_msg *msg = data;

	if (len != sizeof(*msg)) {
		dev_err(uv_dev->dev, "Incorrect len received: %lu (exp. %lu)",
				len, sizeof(*msg));
		return;
	}

	usbvdm_engine_ext_msg(uv_dev->engine, msg->msg_type,
			msg->data, msg->data_len);
}

static void usbvdm_glink_handle_recv_vdm(struct usbvdm_dev *uv_dev,
		void *data, size_t len)
{
	struct pmic_glink_oem_vdm_msg *msg = data;
	u32 vdm_hdr, *vdos, num_vdos;

	if (len != sizeof(*msg)) {
		dev_err(uv_dev->dev, "Incorrect len received: %lu (exp. %lu)",
				len, sizeof(*msg));
		return;
	}

	vdm_hdr = msg->data[0];
	vdos = &msg->data[1];
	num_vdos = msg->size - 1;

	usbvdm_engine_vdm(uv_dev->engine, vdm_hdr, vdos, num_vdos);
}

static int usbvdm_glink_msg_cb(void *priv, void *data, size_t len)
{
	struct usbvdm_dev *uv_dev = priv;
	struct pmic_glink_hdr *hdr = data;

	if (hdr->owner != PMIC_GLINK_MSG_OWNER_OEM) {
		dev_err(uv_dev->dev, "Received msg for unknown owner %d", hdr->owner);
		return 0;
	}

	switch (hdr->opcode) {
	case OEM_OPCODE_NOTIFY:
		usbvdm_glink_handle_notify(uv_dev, data, len);
		break;
	case OEM_OPCODE_RECV_VDM:
		usbvdm_glink_handle_recv_vdm(uv_dev, data, len);
		break;
	case OEM_OPCODE_RECV_EXT_MSG:
		usbvdm_glink_handle_recv_ext_msg(uv_dev, data, len);
		break;
	default:
		dev_err(uv_dev->dev, "Unable to handle opcode %x", hdr->opcode);
		break;
	}

	return 0;
}

static void usbvdm_glink_state_cb(void *priv, enum pmic_glink_state state)
{
	struct usbvdm_dev *uv_dev = priv;

	mutex_lock(&uv_dev->state_lock);

	if (uv_dev->state == state) {
		dev_dbg(uv_dev->dev, "glink state %d unchanged, do nothing", state);
		goto out;
	}
	uv_dev->state = state;

	switch (state) {
	case PMIC_GLINK_STATE_UP:
		schedule_work(&uv_dev->setup_work);
		break;
	case PMIC_GLINK_STATE_DOWN:
		usbvdm_unregister(uv_dev->engine);
		break;
	default:
		break;
	}

out:
	mutex_unlock(&uv_dev->state_lock);
}

static int usbvdm_glink_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data = {};
	struct usbvdm_dev *uv_dev;
	int rc;

	uv_dev = devm_kzalloc(dev, sizeof(*uv_dev), GFP_KERNEL);
	if (!uv_dev)
		return -ENOMEM;

	INIT_WORK(&uv_dev->setup_work, usbvdm_glink_setup_work);
	mutex_init(&uv_dev->state_lock);
	uv_dev->state = PMIC_GLINK_STATE_DOWN;

	platform_set_drvdata(pdev, uv_dev);
	uv_dev->dev = dev;

	rc = usbvdm_glink_setup(uv_dev);
	if (rc) {
		dev_err(dev, "Failed to register with usbvdm, rc=%d", rc);
		return rc;
	}

	client_data.id = PMIC_GLINK_MSG_OWNER_OEM;
	client_data.name = "usbvdm";
	client_data.msg_cb = usbvdm_glink_msg_cb;
	client_data.state_cb = usbvdm_glink_state_cb;
	client_data.priv = uv_dev;
	uv_dev->client = pmic_glink_register_client(dev, &client_data);
	uv_dev->state = PMIC_GLINK_STATE_UP;
	if (IS_ERR(uv_dev->client)) {
		usbvdm_unregister(uv_dev->engine);
		rc = PTR_ERR(uv_dev->client);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Error in registering with pmic_glink, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int usbvdm_glink_remove(struct platform_device *pdev)
{
	struct usbvdm_dev *uv_dev = platform_get_drvdata(pdev);

	pmic_glink_unregister_client(uv_dev->client);
	usbvdm_unregister(uv_dev->engine);

	mutex_destroy(&uv_dev->state_lock);

	return 0;
}

static const struct of_device_id usbvdm_glink_match_table[] = {
	{.compatible = "meta,usbvdm-glink"},
	{},
};

static struct platform_driver usbvdm_glink_driver = {
	.driver	= {
		.name = "usbvdm_glink",
		.of_match_table = usbvdm_glink_match_table,
	},
	.probe	= usbvdm_glink_probe,
	.remove	= usbvdm_glink_remove,
};

module_platform_driver(usbvdm_glink_driver);

MODULE_DESCRIPTION("Meta USBVDM Glink driver");
MODULE_LICENSE("GPL v2");
