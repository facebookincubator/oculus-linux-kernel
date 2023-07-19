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
	u32 data[MAX_NUM_VDOS];
	u32 size;
};

struct vdm_glink_notify_msg {
	struct pmic_glink_hdr	hdr;
	u32			notification;
	u32			receiver;
	u32			reserved;
};

struct vdm_glink_dev {
	struct device			*dev;
	struct pmic_glink_client	*client;

	struct mutex			state_lock;
	struct vdm_glink_vdm_msg	rx_vdm;
	enum pmic_glink_state		state;
	struct usbpd_svid_handler	*svid_handler;
};

int vdm_glink_register_handler(void *udev, struct usbpd_svid_handler *handler)
{
	struct vdm_glink_dev *vdm_glink_udev = (struct vdm_glink_dev *) udev;

	if (udev == NULL)
		return -EINVAL;

	mutex_lock(&vdm_glink_udev->state_lock);
	vdm_glink_udev->svid_handler = handler;
	mutex_unlock(&vdm_glink_udev->state_lock);
	return 0;
}
EXPORT_SYMBOL(vdm_glink_register_handler);

int vdm_glink_unregister_handler(void *udev)
{
	struct vdm_glink_dev *vdm_glink_udev = (struct vdm_glink_dev *) udev;

	if (udev == NULL)
		return -EINVAL;

	mutex_lock(&vdm_glink_udev->state_lock);
	vdm_glink_udev->svid_handler = NULL;
	mutex_unlock(&vdm_glink_udev->state_lock);
	return 0;
}
EXPORT_SYMBOL(vdm_glink_unregister_handler);

int vdm_glink_send_vdm(void *udev, u32 vdm_hdr, const u32 *vdos, u32 num_vdos)
{
	struct vdm_glink_vdm_msg vdm_msg = { { 0 } };
	struct vdm_glink_dev *vdm_glink_udev = (struct vdm_glink_dev *) udev;
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
	u32 *vdos;
	u32 num_vdos = 0;
	u32 vdm_hdr;

	if (len != sizeof(udev->rx_vdm)) {
		dev_err(udev->dev, "Incorrect received length %zu expected %lu\n", len,
			sizeof(udev->rx_vdm));
		return -EINVAL;
	}

	mutex_lock(&udev->state_lock);
	if (!udev->svid_handler) {
		dev_err(udev->dev, "error svid_hanlder undefined");
		mutex_unlock(&udev->state_lock);
		return 0;
	}

	memcpy(&udev->rx_vdm, data, sizeof(udev->rx_vdm));

	/* set up vdm message */
	vdm_hdr = udev->rx_vdm.data[0];
	vdos = &(udev->rx_vdm.data[1]);
	num_vdos = udev->rx_vdm.size - 1;

	dev_dbg(udev->dev, "recv VDM message size=%d", udev->rx_vdm.size);

	mutex_unlock(&udev->state_lock);

	udev->svid_handler->vdm_received(udev->svid_handler, vdm_hdr, vdos, num_vdos);

	return 0;
}

static int handle_vdm_glink_notify(struct vdm_glink_dev *udev, void *data, size_t len)
{
	struct vdm_glink_notify_msg *msg_ptr;

	if (len != sizeof(*msg_ptr)) {
		dev_err(udev->dev, "Incorrect received length %zu expected %lu\n", len,
			sizeof(*msg_ptr));
		return -EINVAL;
	}

	mutex_lock(&udev->state_lock);
	if (!udev->svid_handler) {
		dev_err(udev->dev, "error svid_hanlder undefined");
		mutex_unlock(&udev->state_lock);
		return -EINVAL;
	}

	msg_ptr = data;

	dev_dbg(udev->dev, "notification received=%d", msg_ptr->notification);

	if (msg_ptr->notification == OEM_NOTIFICATION_CONNECTED)
		udev->svid_handler->connect(udev->svid_handler, false);
	else
		udev->svid_handler->disconnect(udev->svid_handler);

	mutex_unlock(&udev->state_lock);

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

static int vdm_glink_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data;
	struct vdm_glink_dev *udev;
	int rc = 0;

	udev = devm_kzalloc(dev, sizeof(*udev), GFP_KERNEL);
	if (!udev)
		return -ENOMEM;

	udev->svid_handler = NULL;

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
