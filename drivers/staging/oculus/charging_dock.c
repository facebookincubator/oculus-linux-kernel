// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>

#include "charging_dock.h"
#include "../usbvdm.h"

static void charging_dock_send_vdm_request(
	struct charging_dock_device_t *ddev, u32 parameter, u32 vdo)
{
	u32 vdm_header = 0;
	u32 vdm_vdo = 0;
	int num_vdos = 0;
	int result;

	vdm_header =
	VDMH_CONSTRUCT(ddev->vdm_handler.svid, 0, VDM_REQUEST, 0, 0, parameter);

	if (parameter == PARAMETER_TYPE_BROADCAST_PERIOD) {
		vdm_vdo = vdo;
		num_vdos = 1;
	}

	result = cypd_send_vdm(ddev->cpd, vdm_header, &vdm_vdo, num_vdos);
	if (result != 0) {
		dev_err(ddev->dev,
			"Error sending vdm, parameter type:%d, result:%d", parameter, result);
		return;
	}

	dev_dbg(ddev->dev, "Sent vdm: header=0x%x, num_vdos= %d, vdo=0x%x",
			vdm_header, num_vdos, vdo);

	ddev->ack_parameter = PARAMETER_TYPE_UNKNOWN;
	result = wait_event_interruptible_hrtimeout(ddev->tx_waitq,
		ddev->ack_parameter == parameter, ms_to_ktime(REQ_ACK_TIMEOUT_MS));
	if (result) {
		dev_err(ddev->dev, "%s: failed to receive ack, ret = %d\n", __func__,
			result);
		return;
	}
}

static void charging_dock_handle_work(struct work_struct *work)
{
	struct charging_dock_device_t *ddev =
		container_of(work, struct charging_dock_device_t, dwork.work);

	dev_dbg(ddev->dev, "%s: enter", __func__);

	mutex_lock(&ddev->lock);
	if (!ddev->docked) {
		mutex_unlock(&ddev->lock);
		return;
	}

	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_FW_VERSION_NUMBER, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_SERIAL_NUMBER_MLB, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_BROADCAST_PERIOD,
		ddev->params.broadcast_period);

	mutex_unlock(&ddev->lock);
}

static void vdm_connect(struct cypd_svid_handler *hdlr)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, vdm_handler);

	dev_dbg(ddev->dev, "Received vdm connect\n");

	/*
	 * Client notifications only occur during docked/undocked
	 * state transitions. This should not be called when state is
	 * is already docked.
	 */
	WARN_ON(ddev->docked);

	mutex_lock(&ddev->lock);
	ddev->docked = true;
	mutex_unlock(&ddev->lock);

	sysfs_notify(&ddev->dev->kobj, NULL, "docked");

	schedule_delayed_work(&ddev->dwork, 0);
}

static void vdm_disconnect(struct cypd_svid_handler *hdlr)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, vdm_handler);

	dev_dbg(ddev->dev, "Received vdm disconnect\n");

	/*
	 * Client notifications only occur during docked/undocked
	 * state transitions. This should not be called when state is
	 * is already undocked.
	 */
	WARN_ON(!ddev->docked);

	mutex_lock(&ddev->lock);
	ddev->docked = false;
	mutex_unlock(&ddev->lock);

	sysfs_notify(&ddev->dev->kobj, NULL, "docked");

	cancel_delayed_work_sync(&ddev->dwork);
}

static void vdm_received(struct cypd_svid_handler *hdlr, u32 vdm_hdr,
		const u32 *vdos, int num_vdos)
{
	u32 protocol_type, parameter_type, sb;
	bool acked;
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, vdm_handler);

	dev_dbg(ddev->dev,
		"enter: vdm_hdr=0x%x, vdos?=%d, num_vdos=%d\n",
		vdm_hdr, vdos != NULL, num_vdos);

	protocol_type = VDMH_PROTOCOL(vdm_hdr);
	parameter_type = VDMH_PARAMETER(vdm_hdr);

	dev_dbg(ddev->dev, "VDM protocol type = %d, VDM parameter = %d\n",
		protocol_type, parameter_type);

	if (num_vdos == 0) {
		dev_warn(ddev->dev, "Empty VDO packets vdm_hdr=0x%x\n", vdm_hdr);
		return;
	}

	sb = VDMH_SIZE(vdm_hdr);
	if (sb >= ARRAY_SIZE(vdm_size_bytes)) {
		dev_warn(ddev->dev, "Invalid size byte code, sb=%d", sb);
		return;
	}

	if (protocol_type == VDM_RESPONSE) {
		acked = VDMH_ACK(vdm_hdr);
		if (!acked) {
			dev_warn(ddev->dev, "Unsupported request parameter 0x%x or NACK",
				parameter_type);
			return;
		}

		switch (parameter_type) {
		case PARAMETER_TYPE_FW_VERSION_NUMBER:
			ddev->params.fw_version = vdos[0];
			break;
		case PARAMETER_TYPE_SERIAL_NUMBER_MLB:
			memcpy(ddev->params.serial_number_mlb,
				vdos, sizeof(ddev->params.serial_number_mlb));
			break;
		case PARAMETER_TYPE_BOARD_TEMPERATURE:
			ddev->params.board_temp = vdos[0];
			break;
		case PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM:
			memcpy(ddev->params.serial_number_system,
				vdos, sizeof(ddev->params.serial_number_system));
			break;
		case PARAMETER_TYPE_BROADCAST_PERIOD:
			if (vdos[0] != ddev->params.broadcast_period)
				dev_err(ddev->dev,
				"Error: Broadcast period received: %d != sent: %d\n",
				vdos[0],
				ddev->params.broadcast_period);
			break;
		default:
			dev_err(ddev->dev, "Unsupported response parameter 0x%x",
				parameter_type);
			break;
		}

		ddev->ack_parameter = parameter_type;
		wake_up(&ddev->tx_waitq);

	} else if (protocol_type == VDM_BROADCAST) {

		switch (parameter_type) {
		case PARAMETER_TYPE_BOARD_TEMPERATURE:
			ddev->params.board_temp = vdos[0];
			break;
		default:
			dev_err(ddev->dev, "Unsupported broadcast parameter 0x%x",
				parameter_type);
			break;
		}
	}
}

static ssize_t docked_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ddev->docked);
}
static DEVICE_ATTR_RO(docked);

static ssize_t broadcast_period_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ddev->params.broadcast_period);
}

static ssize_t broadcast_period_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int result;
	u8 temp;

	result = kstrtou8(buf, 10, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for broadcast period: %s", buf);
		return result;
	}

	result = mutex_lock_interruptible(&ddev->lock);
	if (result != 0) {
		dev_err(dev, "Failed to get mutex: %d", result);
		return result;
	}

	ddev->params.broadcast_period = temp;

	if (ddev->docked) {
		cancel_delayed_work(&ddev->dwork);
		schedule_delayed_work(&ddev->dwork, 0);
	}
	mutex_unlock(&ddev->lock);

	return count;
}
static DEVICE_ATTR_RW(broadcast_period);

static ssize_t fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ddev->params.fw_version);
}
static DEVICE_ATTR_RO(fw_version);

static ssize_t serial_number_mlb_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int result;

	result = scnprintf(buf, PAGE_SIZE, "%s\n", ddev->params.serial_number_mlb);

	return result;
}
static DEVICE_ATTR_RO(serial_number_mlb);

static ssize_t board_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ddev->params.board_temp);
}
static DEVICE_ATTR_RO(board_temp);

static ssize_t serial_number_system_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int result;

	result = scnprintf(buf, PAGE_SIZE, "%s\n",
		ddev->params.serial_number_system);

	return result;
}
static DEVICE_ATTR_RO(serial_number_system);

static struct attribute *charging_dock_attrs[] = {
	&dev_attr_docked.attr,
	&dev_attr_broadcast_period.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_serial_number_mlb.attr,
	&dev_attr_board_temp.attr,
	&dev_attr_serial_number_system.attr,
	NULL,
};
ATTRIBUTE_GROUPS(charging_dock);

static void charging_dock_create_sysfs(struct charging_dock_device_t *ddev)
{
	int result;

	/* Mount state node */
	result = sysfs_create_groups(&ddev->dev->kobj, charging_dock_groups);
	if (result != 0)
		dev_err(ddev->dev, "Error creating sysfs entries: %d\n", result);
}

static int charging_dock_probe(struct platform_device *pdev)
{
	int result = 0;
	struct charging_dock_device_t *ddev;
	u32 temp_val = 0;

	dev_dbg(&pdev->dev, "enter\n");

	ddev = devm_kzalloc(&pdev->dev, sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;

	ddev->cpd = devm_cypd_get_by_phandle(&pdev->dev, "charger-cypd");
	if (IS_ERR_OR_NULL(ddev->cpd)) {
		dev_err(&pdev->dev, "devm_cypd_get_by_phandle failed: %ld\n",
				PTR_ERR(ddev->cpd));
		return PTR_ERR(ddev->cpd);
	}

	ddev->vdm_handler.connect = vdm_connect;
	ddev->vdm_handler.disconnect = vdm_disconnect;
	ddev->vdm_handler.vdm_received = vdm_received;

	result = of_property_read_u16(pdev->dev.of_node,
		"svid", &ddev->vdm_handler.svid);
	if (result < 0) {
		dev_err(&pdev->dev,
			"Charging dock SVID unavailable, failing probe, result:%d\n",
				result);
		return result;
	}
	dev_dbg(&pdev->dev, "Property svid=%x", ddev->vdm_handler.svid);

	result = of_property_read_u32(pdev->dev.of_node,
		"broadcast-period-min", &temp_val);
	if (result < 0 || temp_val > MAX_BROADCAST_PERIOD_MIN) {
		dev_err(&pdev->dev,
			"Broadcast period unavailable/invalid, failing probe, result:%d\n",
			result);
		return result;
	}
	ddev->params.broadcast_period = (u8)temp_val;
	dev_dbg(&pdev->dev, "Broadcast period=%d\n", ddev->params.broadcast_period);

	mutex_init(&ddev->lock);
	INIT_DELAYED_WORK(&ddev->dwork, charging_dock_handle_work);
	init_waitqueue_head(&ddev->tx_waitq);

	/* Register VDM callbacks */
	result = cypd_register_svid(ddev->cpd, &ddev->vdm_handler);
	if (result != 0) {
		dev_err(&pdev->dev, "Failed to register vdm handler");
		return result;
	}
	dev_dbg(&pdev->dev, "Registered svid 0x%04x", ddev->vdm_handler.svid);

	ddev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, ddev);

	charging_dock_create_sysfs(ddev);

	return result;
}

static int charging_dock_remove(struct platform_device *pdev)
{
	struct charging_dock_device_t *ddev = platform_get_drvdata(pdev);

	dev_dbg(ddev->dev, "enter\n");

	if (ddev->cpd != NULL)
		cypd_unregister_svid(ddev->cpd, &ddev->vdm_handler);

	cancel_delayed_work_sync(&ddev->dwork);
	wake_up_all(&ddev->tx_waitq);
	mutex_destroy(&ddev->lock);
	sysfs_remove_groups(&ddev->dev->kobj, charging_dock_groups);

	return 0;
}

/* Driver Info */
static const struct of_device_id charging_dock_match_table[] = {
		{ .compatible = "oculus,charging-dock", },
		{ },
};

static struct platform_driver charging_dock_driver = {
	.driver = {
		.name = "oculus,charging-dock",
		.of_match_table = charging_dock_match_table,
		.owner = THIS_MODULE,
	},
	.probe = charging_dock_probe,
	.remove = charging_dock_remove,
};

static int __init charging_dock_init(void)
{
	platform_driver_register(&charging_dock_driver);
	return 0;
}

static void __exit charging_dock_exit(void)
{
	platform_driver_unregister(&charging_dock_driver);
}

module_init(charging_dock_init);
module_exit(charging_dock_exit);

MODULE_DESCRIPTION("Charging dock driver");
MODULE_LICENSE("GPL v2");
