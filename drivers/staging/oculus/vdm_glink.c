// SPDX-License-Identifier: GPL-2.0-only

#include <linux/device.h>
#include <linux/ipc_logging.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/usb/typec.h>

#include "usbvdm.h"
#include "vdm_glink.h"

/* OEM specific definitions */
#define PMIC_GLINK_MSG_OWNER_OEM 32782
#define MAX_NUM_VDOS 7

#define OEM_OPCODE_SEND_VDM	0x10002
#define OEM_OPCODE_RECV_VDM	0x10003
#define OEM_OPCODE_NOTIFY	0x10004

#define OEM_NOTIFICATION_CONNECTED	0x1
#define OEM_NOTIFICATION_DISCONNECTED	0x2

#define MSG_TYPE_REQ_RESP 1
#define MSG_TYPE_NOTIFY 2

struct vdm_glink_vdm_msg {
	struct pmic_glink_hdr hdr;
	u16 svid;
	u16 pid;
	u32 data[MAX_NUM_VDOS];
	u32 size;
};

struct vdm_glink_notify_msg {
	struct pmic_glink_hdr	hdr;
	u32			notification;
	u32			receiver;
	u16			svid;
	u16			pid;
	u32			reserved;
};

struct vdm_glink_dev {
	struct device			*dev;
	struct pmic_glink_client	*client;

	struct mutex			state_lock;
	enum pmic_glink_state		state;

	struct mutex		svid_handler_lock;
	struct list_head	svid_handlers;
};

int vdm_glink_register_handler(struct vdm_glink_dev *udev,
		struct glink_svid_handler *handler)
{
	if (udev == NULL)
		return -EINVAL;

	/* require connect/disconnect callbacks be implemented */
	if (!handler->connect || !handler->disconnect) {
		dev_err(udev->dev,
				"SVID/PID 0x%04x/0x%04x connect/disconnect must be non-NULL",
				handler->svid, handler->pid);
		return -EINVAL;
	}

	dev_dbg(udev->dev, "registered handler for SVID/PID 0x%04x/0x%04x",
			handler->svid, handler->pid);
	mutex_lock(&udev->svid_handler_lock);
	list_add_tail(&handler->entry, &udev->svid_handlers);
	mutex_unlock(&udev->svid_handler_lock);

	return 0;
}
EXPORT_SYMBOL(vdm_glink_register_handler);

void vdm_glink_unregister_handler(struct vdm_glink_dev *udev,
		struct glink_svid_handler *hdlr)
{
	dev_dbg(udev->dev, "unregistered handler(%pK) for SVID/PID 0x%04x/0x%04x",
			hdlr, hdlr->svid, hdlr->pid);
	mutex_lock(&udev->svid_handler_lock);
	list_del_init(&hdlr->entry);
	mutex_unlock(&udev->svid_handler_lock);
}
EXPORT_SYMBOL(vdm_glink_unregister_handler);

int vdm_glink_send_vdm(struct vdm_glink_dev *vdm_glink_udev,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos)
{
	struct vdm_glink_vdm_msg vdm_msg = { { 0 } };
	int rc = 0;

	mutex_lock(&vdm_glink_udev->state_lock);
	if (vdm_glink_udev->state == PMIC_GLINK_STATE_DOWN) {
		dev_err(vdm_glink_udev->dev, "attempting to send VDM when glink is down");
		rc = -ENOTCONN;
		goto out;
	}

	vdm_msg.hdr.owner = PMIC_GLINK_MSG_OWNER_OEM;
	vdm_msg.hdr.type = MSG_TYPE_NOTIFY;
	vdm_msg.hdr.opcode = OEM_OPCODE_SEND_VDM;

	/* set up vdm message */
	vdm_msg.data[0] = vdm_hdr;
	if (vdos && num_vdos)
		memcpy(&vdm_msg.data[1], vdos, num_vdos * sizeof(u32));
	vdm_msg.size = num_vdos + 1; /* include the header */

	dev_dbg(vdm_glink_udev->dev, "send VDM message size=%d", vdm_msg.size);

	rc = pmic_glink_write(vdm_glink_udev->client, &vdm_msg,
					sizeof(vdm_msg));

	if (rc < 0)
		dev_err(vdm_glink_udev->dev, "Error in sending message rc=%d\n", rc);

out:
	mutex_unlock(&vdm_glink_udev->state_lock);
	return rc;
}
EXPORT_SYMBOL(vdm_glink_send_vdm);

static int handle_vdm_glink_recv_vdm(struct vdm_glink_dev *udev, void *data, size_t len)
{
	struct vdm_glink_vdm_msg *rx_vdm;
	struct glink_svid_handler *handler;
	u32 *vdos;
	u32 num_vdos = 0;
	u32 vdm_hdr;

	if (len != sizeof(*rx_vdm)) {
		dev_err(udev->dev, "Incorrect received length %zu expected %lu\n", len,
			sizeof(*rx_vdm));
		return -EINVAL;
	}

	rx_vdm = data;
	dev_dbg(udev->dev, "recv VDM: svid=0x%04x, pid=0x%04x, message size=%d",
			rx_vdm->svid, rx_vdm->pid, rx_vdm->size);

	/* set up vdm message */
	vdm_hdr = rx_vdm->data[0];
	vdos = &rx_vdm->data[1];
	num_vdos = rx_vdm->size - 1;

	mutex_lock(&udev->svid_handler_lock);
	list_for_each_entry(handler, &udev->svid_handlers, entry)
		if (rx_vdm->svid == handler->svid && rx_vdm->pid == handler->pid)
			handler->vdm_received(handler, vdm_hdr, vdos, num_vdos);
	mutex_unlock(&udev->svid_handler_lock);

	return 0;
}

static int handle_vdm_glink_notify(struct vdm_glink_dev *udev, void *data, size_t len)
{
	struct vdm_glink_notify_msg *msg_ptr;
	struct glink_svid_handler *handler;

	if (len != sizeof(*msg_ptr)) {
		dev_err(udev->dev, "Incorrect received length %zu expected %lu\n", len,
			sizeof(*msg_ptr));
		return -EINVAL;
	}

	msg_ptr = data;
	dev_dbg(udev->dev,
			"recv notification: notification=%d, svid=0x%04x, pid=0x%04x",
			msg_ptr->notification, msg_ptr->svid, msg_ptr->pid);

	mutex_lock(&udev->svid_handler_lock);
	list_for_each_entry(handler, &udev->svid_handlers, entry)
		if (msg_ptr->svid == handler->svid && msg_ptr->pid == handler->pid)
			msg_ptr->notification == OEM_NOTIFICATION_CONNECTED ?
				handler->connect(handler, false) :
				handler->disconnect(handler);
	mutex_unlock(&udev->svid_handler_lock);

	return 0;
}

static int vdm_glink_msg_cb(void *priv, void *data, size_t len)
{
	struct pmic_glink_hdr *hdr = data;
	struct vdm_glink_dev *udev = priv;

	dev_dbg(udev->dev, "glink msg received: owner: %u type: %u opcode: %u len:%zu\n", hdr->owner,
		hdr->type, hdr->opcode, len);

	if (hdr->opcode == OEM_OPCODE_RECV_VDM)
		handle_vdm_glink_recv_vdm(udev, data, len);
	else if (hdr->opcode == OEM_OPCODE_NOTIFY)
		handle_vdm_glink_notify(udev, data, len);
	else
		dev_err(udev->dev, "Unknown message opcode: %d\n", hdr->opcode);

	return 0;
}

static void vdm_glink_state_cb(void *priv, enum pmic_glink_state state)
{
	struct vdm_glink_dev *udev = priv;

	dev_dbg(udev->dev, "state: %d\n", state);

	mutex_lock(&udev->state_lock);
	udev->state = state;

	switch (state) {
	case PMIC_GLINK_STATE_DOWN:
		/* maybe disconnect here? */
		break;
	case PMIC_GLINK_STATE_UP:
		break;
	default:
		break;
	}
	mutex_unlock(&udev->state_lock);
}


static void devm_vdm_glink_put(struct device *dev, void *res)
{
	struct vdm_glink_dev **ppd = res;

	put_device((*ppd)->dev);
}

struct vdm_glink_dev *devm_vdm_glink_get_by_phandle(struct device *dev,
		const char *phandle)
{
	struct vdm_glink_dev **ptr = NULL, *vdm_glink = NULL;
	struct device_node *dev_np;
	struct platform_device *pdev;

	if (!dev->of_node)
		return ERR_PTR(-EINVAL);

	dev_np = of_parse_phandle(dev->of_node, phandle, 0);
	if (!dev_np)
		return ERR_PTR(-ENXIO);

	pdev = of_find_device_by_node(dev_np);
	of_node_put(dev_np);
	if (!pdev)
		/* device was found but maybe hadn't probed yet, so defer */
		return ERR_PTR(-EPROBE_DEFER);

	ptr = devres_alloc(devm_vdm_glink_put, sizeof(*ptr), GFP_KERNEL);
	if (!ptr) {
		platform_device_put(pdev);
		return ERR_PTR(-ENOMEM);
	}

	vdm_glink = dev_get_drvdata(&pdev->dev);
	if (!vdm_glink) {
		platform_device_put(pdev);
		return ERR_PTR(-EPROBE_DEFER);
	}

	*ptr = vdm_glink;
	devres_add(dev, ptr);

	return vdm_glink;
}
EXPORT_SYMBOL(devm_vdm_glink_get_by_phandle);

static int vdm_glink_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data;
	struct vdm_glink_dev *udev;
	int rc = 0;

	udev = devm_kzalloc(dev, sizeof(*udev), GFP_KERNEL);
	if (!udev)
		return -ENOMEM;

	mutex_init(&udev->state_lock);

	udev->state = PMIC_GLINK_STATE_UP;

	client_data.id = PMIC_GLINK_MSG_OWNER_OEM;
	client_data.name = "oculus_vdm";
	client_data.msg_cb = vdm_glink_msg_cb;
	client_data.priv = udev;
	client_data.state_cb = vdm_glink_state_cb;

	udev->client = pmic_glink_register_client(dev, &client_data);
	if (IS_ERR(udev->client)) {
		rc = PTR_ERR(udev->client);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Error in registering with pmic_glink rc=%d\n",
				rc);
		return rc;
	}

	mutex_init(&udev->svid_handler_lock);
	INIT_LIST_HEAD(&udev->svid_handlers);

	platform_set_drvdata(pdev, udev);
	udev->dev = dev;

	return rc;
}

static int vdm_glink_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vdm_glink_dev *udev = dev_get_drvdata(dev);
	int rc;

	rc = pmic_glink_unregister_client(udev->client);
	if (rc < 0)
		dev_err(dev, "pmic_glink_unregister_client failed rc=%d\n",
			rc);

	mutex_destroy(&udev->svid_handler_lock);
	list_del(&udev->svid_handlers);

	return rc;
}

static const struct of_device_id vdm_glink_match_table[] = {
	{.compatible = "oculus,vdm_glink"},
	{},
};

static struct platform_driver vdm_glink_driver = {
	.driver	= {
		.name = "vdm_glink",
		.of_match_table = vdm_glink_match_table,
	},
	.probe	= vdm_glink_probe,
	.remove	= vdm_glink_remove,
};

module_platform_driver(vdm_glink_driver);

MODULE_DESCRIPTION("Oculus VDM Glink driver");
MODULE_LICENSE("GPL");
