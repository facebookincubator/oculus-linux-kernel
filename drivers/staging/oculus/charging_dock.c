// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "charging_dock.h"
#include "../usbvdm.h"

static void charging_dock_send_vdm_request(
	struct charging_dock_device_t *ddev, u32 parameter, u32 vdo)
{
	u32 vdm_header = 0;
	u32 vdm_vdo = 0;
	int num_vdos = 0;
	int result = -1;
	u16 svid = ddev->intf_type == INTF_TYPE_USBPD ? ddev->usbpd_vdm_handler.svid : ddev->cypd_vdm_handler.svid;

	/* header(32) = svid(16) ack(1) proto(2) high(1) size(3) param(8) */
	vdm_header = VDMH_CONSTRUCT(svid, 0, VDM_REQUEST, 0, 0, parameter);

	if (parameter == PARAMETER_TYPE_BROADCAST_PERIOD ||
			parameter == PARAMETER_TYPE_LOG_TRANSMIT ||
			parameter == PARAMETER_TYPE_LOG_CHUNK) {
		vdm_vdo = vdo;
		num_vdos = 1;
	}

	if (ddev->intf_type == INTF_TYPE_USBPD)
		result = usbpd_send_vdm(ddev->upd, vdm_header, &vdm_vdo, num_vdos);
	else if (ddev->intf_type == INTF_TYPE_CYPD)
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
	if (!ddev->docked || ddev->intf_type == INTF_TYPE_UNKNOWN) {
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

static void vdm_connect_common(struct charging_dock_device_t *ddev, enum charging_dock_intf_type intf_type)
{
	/*
	 * Client notifications only occur during docked/undocked
	 * state transitions. This should not be called when state is
	 * is already docked.
	 */
	if (ddev->docked) {
		dev_warn(ddev->dev, "Already docked: ignoring connect callback\n");
		return;
	}

	mutex_lock(&ddev->lock);
	ddev->docked = true;
	ddev->intf_type = intf_type;
	mutex_unlock(&ddev->lock);

	sysfs_notify(&ddev->dev->kobj, NULL, "docked");

	schedule_delayed_work(&ddev->dwork, 0);
}

static void cypd_vdm_connect(struct cypd_svid_handler *hdlr)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, cypd_vdm_handler);
	dev_info(ddev->dev, "cypd vdm connect\n");
	vdm_connect_common(ddev, INTF_TYPE_CYPD);
}

static void usbpd_vdm_connect(struct usbpd_svid_handler *hdlr, bool supports_usb_comm)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, usbpd_vdm_handler);
	dev_info(ddev->dev, "usbpd vdm connect: usb_comm=%d\n", supports_usb_comm);
	vdm_connect_common(ddev, INTF_TYPE_USBPD);
}

static void vdm_disconnect_common(struct charging_dock_device_t *ddev, enum charging_dock_intf_type intf_type)
{
	/*
	 * If a disconnect callback is received from another interface, simply ignore it
	 * as only interface type is supported at a given point in time
	 */
	if (ddev->intf_type == INTF_TYPE_UNKNOWN || ddev->intf_type != intf_type) {
		dev_warn(ddev->dev, "Received disconnect from a mis-matched interface, ignoring\n");
		return;
	}

	mutex_lock(&ddev->lock);
	ddev->docked = false;
	ddev->intf_type = INTF_TYPE_UNKNOWN;
	mutex_unlock(&ddev->lock);

	sysfs_notify(&ddev->dev->kobj, NULL, "docked");

	cancel_delayed_work_sync(&ddev->dwork);
}

static void cypd_vdm_disconnect(struct cypd_svid_handler *hdlr)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, cypd_vdm_handler);
	dev_info(ddev->dev, "cypd vdm disconnect\n");
	vdm_disconnect_common(ddev, INTF_TYPE_CYPD);
}

static void usbpd_vdm_disconnect(struct usbpd_svid_handler *hdlr)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, usbpd_vdm_handler);
	dev_info(ddev->dev, "usbpd vdm disconnect\n");
	vdm_disconnect_common(ddev, INTF_TYPE_USBPD);
}

#define PORT_CONFIG_VOLTAGE_MV_CONVERSION 50
#define PORT_CONFIG_CURRENT_MA_CONVERSION 10
static void parse_port_config(struct charging_dock_device_t *ddev, const u32 *vdos, int num_vdos)
{
	int port_num;

	if (num_vdos != 4) {
		dev_err(ddev->dev, "Error parsing port config message: wrong number of vdos: %d", num_vdos);
		return;
	}
	port_num = (vdos[0] >> 24) & 0xFF;
	if (port_num < 0 || port_num >= NUM_CHARGING_DOCK_PORTS) {
		dev_err(ddev->dev, "Error parsing port config message: invalid port_num: %d", port_num);
		return;
	}
	ddev->params.port_config[port_num].state = (vdos[0] >> 16) & 0xFF;
	ddev->params.port_config[port_num].vid = vdos[0] & 0xFFFF;
	ddev->params.port_config[port_num].pid = (vdos[1] >> 16) & 0xFFFF;
	ddev->params.port_config[port_num].voltage_mv =
		(vdos[1] & 0xFFFF) * PORT_CONFIG_VOLTAGE_MV_CONVERSION;
	ddev->params.port_config[port_num].current_ma =
		((vdos[2] >> 16) & 0xFFFF) * PORT_CONFIG_CURRENT_MA_CONVERSION;

	dev_dbg(ddev->dev, "Port config: port_num=%d state=%d vid=%d pid=%d voltage_mv=%d current_ma=%d",
			port_num,
			ddev->params.port_config[port_num].state,
			ddev->params.port_config[port_num].vid,
			ddev->params.port_config[port_num].pid,
			ddev->params.port_config[port_num].voltage_mv,
			ddev->params.port_config[port_num].current_ma);
}

/*
 * TODO(T126954787) Replace this UPA serial number parsing when we receive
 * new ones with updated firmware. Currently, serial numbers are reported with
 * flipped endianness. This will be fixed in the next firmware.
 *
 * After switching the endianness, the first two bytes are the port # and
 * parameter type. The remaining 14 bytes are the serial number.
 */
static void parse_connected_devices_serial_number(char *out, const char *in)
{
	int i;
	char temp[16];

  /* copy input to temp buffer */
	memcpy(temp, in, 16);

	/* swap endianness */
	for (i = 0; i < 16; i++) {
		if (i % 4 == 0)
			swap(temp[i], temp[i+3]);
		else if (i % 4 == 1)
			swap(temp[i], temp[i+1]);
	}

	/* copy over serial number */
	memcpy(out, &temp[2], 14);
}

static void parse_connected_devices(struct charging_dock_device_t *ddev, const u32 *vdos, int num_vdos)
{
	int port_num;
	int parameter_type;

	if (num_vdos != 4) {
		dev_err(ddev->dev, "Error parsing connected devices message: wrong number of vdos: %d", num_vdos);
		return;
	}
	port_num = (vdos[0] >> 24) & 0xFF;
	if (port_num < 0 || port_num > 4) {
		dev_err(ddev->dev, "Error parsing connected devices message: invalid port_num: %d", port_num);
		return;
	}
	parameter_type = (vdos[0] >> 16) & 0xFF;
	switch (parameter_type) {
	case PARAMETER_TYPE_FW_VERSION_NUMBER:
		ddev->params.port_config[port_num].fw_version = (((vdos[0] & 0xFFFF) << 16) | ((vdos[1] >> 16) & 0xFFFF));
		dev_dbg(ddev->dev, "connected devices message: port_num=%d fw_version=%d", port_num, ddev->params.port_config[port_num].fw_version);
		break;
	case PARAMETER_TYPE_SERIAL_NUMBER_MLB:
		parse_connected_devices_serial_number(ddev->params.port_config[port_num].serial_number_mlb, (char *) vdos);
		dev_dbg(ddev->dev, "connected devices message: port_num=%d serial_number_mlb: %s", port_num, ddev->params.port_config[port_num].serial_number_mlb);
		break;
	case PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM:
		parse_connected_devices_serial_number(ddev->params.port_config[port_num].serial_number_system, (char *) vdos);
		dev_dbg(ddev->dev, "connected devices message: port_num=%d serial_number_system: %s", port_num, ddev->params.port_config[port_num].serial_number_system);
		break;
	default:
		dev_err(ddev->dev, "Error invalid parameter type: %d", parameter_type);
	}
}

static void vdm_received_common(struct charging_dock_device_t *ddev, u32 vdm_hdr,
		const u32 *vdos, int num_vdos)
{
	u32 protocol_type, parameter_type, sb;
	bool acked;
	int log_chunk_size;
	int log_chunk_offset;
	int new_log_size;

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
		case PARAMETER_TYPE_LOG_TRANSMIT:
			/* Receiving a response to one of two messages
			 * - TRANSMIT_STOP
			 *   - log size will be 0
			 * - TRANSMIT_START
			 *   - log size will be >= 0
			 */

			new_log_size = vdos[0];
			if (new_log_size > MAX_LOG_SIZE) {
				dev_err(ddev->dev,
						"Error: log size is greater than 4KB: %d\n",
						new_log_size);
				break;
			}

			/* this is either a response to TRANSMIT_START, stating there is no log
			 * to gather, or a response to TRANSMIT_STOP, which always is size 0
			 */
			if (new_log_size == 0)
				break;

			/* Reset to start receiving new log */
			if (ddev->log)
				devm_kfree(ddev->dev, ddev->log);
			ddev->log = NULL;

			ddev->log = devm_kzalloc(ddev->dev, new_log_size, GFP_KERNEL);
			if (!ddev->log)
				break;

			ddev->params.log_size = new_log_size;
			ddev->gathering_log = true;
			ddev->log_chunk_num = 0;
			break;
		case PARAMETER_TYPE_LOG_CHUNK:
			if (!ddev->gathering_log) {
				dev_err(ddev->dev,
						"Error: log chunk received, but gathering_log is false\n");
				break;
			}
			log_chunk_offset = ddev->log_chunk_num * MAX_VDO_SIZE;
			if (ddev->params.log_size - log_chunk_offset <= MAX_VDO_SIZE) {
				/* last log chunk */
				log_chunk_size = ddev->params.log_size - log_chunk_offset;
				/* send end logging message */
				ddev->gathering_log = false;
			} else {
				/* standard log chunk */
				log_chunk_size = MAX_VDO_SIZE;
				ddev->log_chunk_num++;
			}
			if (log_chunk_size > (num_vdos * sizeof(vdos[0]))) {
				ddev->gathering_log = false;
				dev_err(ddev->dev, "Error: less log chunk data: %lu than expected %d",
						num_vdos * sizeof(vdos[0]),
						log_chunk_size);
				break;
			}
			memcpy(ddev->log + log_chunk_offset, vdos, log_chunk_size);
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
		case PARAMETER_TYPE_PORT_CONFIGURATION:
			dev_dbg(ddev->dev, "Received port configuration: 0x%x",
				parameter_type);
			parse_port_config(ddev, vdos, num_vdos);
			break;
		case PARAMETER_TYPE_CONNECTED_DEVICES:
			dev_dbg(ddev->dev, "Received connected devices: 0x%x",
				parameter_type);
			parse_connected_devices(ddev, vdos, num_vdos);
			break;
		default:
			dev_err(ddev->dev, "Unsupported broadcast parameter 0x%x",
				parameter_type);
			break;
		}
	}
}

static void cypd_vdm_received(struct cypd_svid_handler *hdlr, u32 vdm_hdr,
		const u32 *vdos, int num_vdos)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, cypd_vdm_handler);
	vdm_received_common(ddev, vdm_hdr, vdos, num_vdos);
}

static void usbpd_vdm_received(struct usbpd_svid_handler *hdlr, u32 vdm_hdr,
		const u32 *vdos, int num_vdos)
{
	struct charging_dock_device_t *ddev =
		container_of(hdlr, struct charging_dock_device_t, usbpd_vdm_handler);
	vdm_received_common(ddev, vdm_hdr, vdos, num_vdos);
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

static ssize_t log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	size_t size = 0;

	mutex_lock(&ddev->lock);

	if (ddev->gathering_log) {
		/* currently gathering, return nothing */
		dev_err(ddev->dev, "Error: currently gathering log");
		goto log_show_unlock;
	} else if (!ddev->log) {
		/* no log */
		dev_warn(ddev->dev, "No log to show");
		goto log_show_unlock;
	}

	/* there is a log */
	size = ddev->params.log_size;

	/*
	 * returns 4KB max, which is PAGESIZE on seacliff
	 * PAGESIZE is the max you can return from a sysfs node
	 */
	memcpy(buf, ddev->log, size);

log_show_unlock:
	mutex_unlock(&ddev->lock);
	return size;
}

/* reads log when written to */
static ssize_t log_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int last_log_chunk = -1;

	mutex_lock(&ddev->lock);

	if (!ddev->docked || ddev->gathering_log) {
		dev_err(ddev->dev, "Error: not docked or already gathering log");
		goto log_store_unlock;
	}

	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_LOG_TRANSMIT, VDO_LOG_TRANSMIT_START);

	/* we likely didn't get a response to TRANSMIT_START or the log_size was 0 */
	if (!ddev->gathering_log) {
		dev_err(ddev->dev, "Error: either no response to TRANSMIT_START or there is no log to gather");
		goto log_store_unlock;
	}

	while (ddev->gathering_log) {
		if (last_log_chunk == ddev->log_chunk_num) {
			ddev->params.log_size = ddev->log_chunk_num * MAX_VDO_SIZE;
			dev_err(ddev->dev, "Error: didn't receive response to log chunk request");
			break;
		}
		last_log_chunk = ddev->log_chunk_num;
		charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_LOG_CHUNK, ddev->log_chunk_num);
	}

	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_LOG_TRANSMIT, VDO_LOG_TRANSMIT_STOP);

log_store_unlock:
	ddev->gathering_log = false;
	mutex_unlock(&ddev->lock);
	return count;
}
static DEVICE_ATTR_RW(log);

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

static ssize_t log_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int result;

	result = scnprintf(buf, PAGE_SIZE, "%lu\n",
		ddev->params.log_size);

	return result;
}
static DEVICE_ATTR_RO(log_size);

static ssize_t gathering_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int result;

	result = scnprintf(buf, PAGE_SIZE, "%d\n",
		ddev->gathering_log);

	return result;
}
static DEVICE_ATTR_RO(gathering_log);

static ssize_t get_port_state(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		ddev->params.port_config[port_num].state);
}

static ssize_t port_0_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_state(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_state);

static ssize_t port_1_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_state(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_state);

static ssize_t port_2_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_state(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_state);

static ssize_t port_3_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_state(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_state);

static ssize_t get_port_vid(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		ddev->params.port_config[port_num].vid);
}

static ssize_t port_0_vid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_vid(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_vid);

static ssize_t port_1_vid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_vid(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_vid);

static ssize_t port_2_vid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_vid(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_vid);

static ssize_t port_3_vid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_vid(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_vid);

static ssize_t get_port_pid(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		ddev->params.port_config[port_num].pid);
}

static ssize_t port_0_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_pid(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_pid);

static ssize_t port_1_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_pid(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_pid);

static ssize_t port_2_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_pid(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_pid);

static ssize_t port_3_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_pid(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_pid);

static ssize_t get_port_voltage_mv(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		ddev->params.port_config[port_num].voltage_mv);
}

static ssize_t port_0_voltage_mv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_voltage_mv(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_voltage_mv);

static ssize_t port_1_voltage_mv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_voltage_mv(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_voltage_mv);

static ssize_t port_2_voltage_mv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_voltage_mv(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_voltage_mv);

static ssize_t port_3_voltage_mv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_voltage_mv(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_voltage_mv);

static ssize_t get_port_current_ma(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		ddev->params.port_config[port_num].current_ma);
}

static ssize_t port_0_current_ma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_current_ma(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_current_ma);

static ssize_t port_1_current_ma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_current_ma(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_current_ma);

static ssize_t port_2_current_ma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_current_ma(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_current_ma);

static ssize_t port_3_current_ma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_current_ma(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_current_ma);

static ssize_t get_port_fw_version(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		ddev->params.port_config[port_num].fw_version);
}

static ssize_t port_0_fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_fw_version(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_fw_version);

static ssize_t port_1_fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_fw_version(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_fw_version);

static ssize_t port_2_fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_fw_version(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_fw_version);

static ssize_t port_3_fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_fw_version(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_fw_version);

static ssize_t get_port_serial_number_mlb(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		ddev->params.port_config[port_num].serial_number_mlb);
}

static ssize_t port_0_serial_number_mlb_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_mlb(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_serial_number_mlb);

static ssize_t port_1_serial_number_mlb_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_mlb(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_serial_number_mlb);

static ssize_t port_2_serial_number_mlb_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_mlb(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_serial_number_mlb);

static ssize_t port_3_serial_number_mlb_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_mlb(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_serial_number_mlb);

static ssize_t get_port_serial_number_system(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		ddev->params.port_config[port_num].serial_number_system);
}

static ssize_t port_0_serial_number_system_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_system(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_serial_number_system);

static ssize_t port_1_serial_number_system_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_system(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_serial_number_system);

static ssize_t port_2_serial_number_system_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_system(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_serial_number_system);

static ssize_t port_3_serial_number_system_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return get_port_serial_number_system(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_serial_number_system);

static struct attribute *charging_dock_attrs[] = {
	&dev_attr_docked.attr,
	&dev_attr_broadcast_period.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_serial_number_mlb.attr,
	&dev_attr_board_temp.attr,
	&dev_attr_serial_number_system.attr,
	&dev_attr_log.attr,
	&dev_attr_log_size.attr,
	&dev_attr_gathering_log.attr,
	&dev_attr_port_0_state.attr,
	&dev_attr_port_1_state.attr,
	&dev_attr_port_2_state.attr,
	&dev_attr_port_3_state.attr,
	&dev_attr_port_0_vid.attr,
	&dev_attr_port_1_vid.attr,
	&dev_attr_port_2_vid.attr,
	&dev_attr_port_3_vid.attr,
	&dev_attr_port_0_pid.attr,
	&dev_attr_port_1_pid.attr,
	&dev_attr_port_2_pid.attr,
	&dev_attr_port_3_pid.attr,
	&dev_attr_port_0_voltage_mv.attr,
	&dev_attr_port_1_voltage_mv.attr,
	&dev_attr_port_2_voltage_mv.attr,
	&dev_attr_port_3_voltage_mv.attr,
	&dev_attr_port_0_current_ma.attr,
	&dev_attr_port_1_current_ma.attr,
	&dev_attr_port_2_current_ma.attr,
	&dev_attr_port_3_current_ma.attr,
	&dev_attr_port_0_fw_version.attr,
	&dev_attr_port_1_fw_version.attr,
	&dev_attr_port_2_fw_version.attr,
	&dev_attr_port_3_fw_version.attr,
	&dev_attr_port_0_serial_number_mlb.attr,
	&dev_attr_port_1_serial_number_mlb.attr,
	&dev_attr_port_2_serial_number_mlb.attr,
	&dev_attr_port_3_serial_number_mlb.attr,
	&dev_attr_port_0_serial_number_system.attr,
	&dev_attr_port_1_serial_number_system.attr,
	&dev_attr_port_2_serial_number_system.attr,
	&dev_attr_port_3_serial_number_system.attr,
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

	// Interface type is not known until we get a vdm_connect callback
	ddev->intf_type = INTF_TYPE_UNKNOWN;

	// usbpd: return on failure only if interface is enabled on the platform
	ddev->upd = devm_usbpd_get_by_phandle(&pdev->dev, "charger-usbpd");
	if (IS_ERR_OR_NULL(ddev->upd)) {
		dev_err(&pdev->dev, "devm_usbpd_get_by_phandle failed: %ld\n",
				PTR_ERR(ddev->upd));
#if IS_ENABLED(CONFIG_USB_PD_POLICY)
		return PTR_ERR(ddev->upd);
#endif
	}

	// cypd: return on failure only if interface is enabled on the platform
	ddev->cpd = devm_cypd_get_by_phandle(&pdev->dev, "charger-cypd");
	if (IS_ERR_OR_NULL(ddev->cpd)) {
		dev_err(&pdev->dev, "devm_cypd_get_by_phandle failed: %ld\n",
				PTR_ERR(ddev->cpd));
#if IS_ENABLED(CONFIG_CYPD_POLICY_ENGINE)
		return PTR_ERR(ddev->cpd);
#endif
	}

	if (!IS_ERR_OR_NULL(ddev->upd)) {
		ddev->usbpd_vdm_handler.connect = usbpd_vdm_connect;
		ddev->usbpd_vdm_handler.disconnect = usbpd_vdm_disconnect;
		ddev->usbpd_vdm_handler.vdm_received = usbpd_vdm_received;

		result = of_property_read_u16(pdev->dev.of_node,
			"svid-usbpd", &ddev->usbpd_vdm_handler.svid);
		if (result < 0) {
			dev_err(&pdev->dev,
				"Charging dock usbpd SVID unavailable, failing probe, result:%d\n",
					result);
			return result;
		}
		dev_dbg(&pdev->dev, "Property svid-usbpd=%x", ddev->usbpd_vdm_handler.svid);
	}

	if (!IS_ERR_OR_NULL(ddev->cpd)) {
		ddev->cypd_vdm_handler.connect = cypd_vdm_connect;
		ddev->cypd_vdm_handler.disconnect = cypd_vdm_disconnect;
		ddev->cypd_vdm_handler.vdm_received = cypd_vdm_received;

		result = of_property_read_u16(pdev->dev.of_node,
			"svid-cypd", &ddev->cypd_vdm_handler.svid);
		if (result < 0) {
			dev_err(&pdev->dev,
				"Charging dock cypd SVID unavailable, failing probe, result:%d\n",
				    result);
			return result;
		}
		dev_dbg(&pdev->dev, "Property svid-cypd=%x", ddev->cypd_vdm_handler.svid);
	}

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
	if (!IS_ERR_OR_NULL(ddev->upd)) {
		result = usbpd_register_svid(ddev->upd, &ddev->usbpd_vdm_handler);
		if (result != 0) {
			dev_err(&pdev->dev, "Failed to register usbpd vdm handler: %d(0x%x)", result, result);
			return result;
		}
		dev_dbg(&pdev->dev, "Registered usbpd svid 0x%04x", ddev->usbpd_vdm_handler.svid);
	}
	if (!IS_ERR_OR_NULL(ddev->cpd)) {
		result = cypd_register_svid(ddev->cpd, &ddev->cypd_vdm_handler);
		if (result != 0) {
			dev_err(&pdev->dev, "Failed to register cypd vdm handler: %d(0x%x)", result, result);
			return result;
		}
		dev_dbg(&pdev->dev, "Registered cypd svid 0x%04x", ddev->cypd_vdm_handler.svid);
	}

	ddev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, ddev);

	charging_dock_create_sysfs(ddev);

	return result;
}

static int charging_dock_remove(struct platform_device *pdev)
{
	struct charging_dock_device_t *ddev = platform_get_drvdata(pdev);

	dev_dbg(ddev->dev, "enter\n");

	if (ddev->upd != NULL)
		usbpd_unregister_svid(ddev->upd, &ddev->usbpd_vdm_handler);

	if (ddev->cpd != NULL)
		cypd_unregister_svid(ddev->cpd, &ddev->cypd_vdm_handler);

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
