// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "charging_dock.h"
#include "../usbvdm.h"

/*
 * Value for green LED when battery is at or above this percent
 * This is defined at framework level for HMD so we will borrow
 * it from there (config_notificationsBatteryNearlyFullLevel)
 */
#define BATTERY_NEARLY_FULL_LEVEL	98

struct charging_dock_request_work_item {
	struct work_struct work;
	u32 parameter;
	u32 vdo;
	int num_vdos;
	struct charging_dock_device_t *ddev;
};

static int batt_psy_notifier_call(struct notifier_block *nb,
	unsigned long ev, void *ptr);

static void charging_dock_send_vdm_request(
	struct charging_dock_device_t *ddev, u32 parameter, u32 vdo, int num_vdos)
{
	u32 vdm_header = 0;
	int result = -1;
	struct usbvdm_subscription_data *pos, *sub_data = NULL;

	if (num_vdos > 1) {
		dev_err(ddev->dev,
			"Error: send_vdm_request only supports a max of 1 VDO");
		return;
	}

	/* header(32) = svid(16) ack(1) proto(2) high(1) size(3) param(8) */
	vdm_header = VDMH_CONSTRUCT(ddev->current_svid, 0, VDM_REQUEST, 0, 0, parameter);

	list_for_each_entry(pos, &ddev->sub_list, entry) {
		if (pos->vid == ddev->current_svid && pos->pid == ddev->current_pid)
			sub_data = pos;
	}

	if (!sub_data)
		return;

	reinit_completion(&ddev->rx_complete);

	result = usbvdm_subscriber_vdm(sub_data->sub, vdm_header, &vdo, num_vdos);
	if (result != 0) {
		dev_err(ddev->dev,
			"Error sending vdm, parameter type:%d, result:%d", parameter, result);
		return;
	}

	dev_dbg(ddev->dev, "Sent vdm: header=0x%x, num_vdos= %d, vdo=0x%x",
			vdm_header, num_vdos, vdo);

	ddev->ack_parameter = PARAMETER_TYPE_UNKNOWN;

	result = wait_for_completion_timeout(&ddev->rx_complete, msecs_to_jiffies(ddev->req_ack_timeout_ms));

	if (!result || (result && ddev->ack_parameter != parameter)) {
		dev_err(ddev->dev, "%s: failed to receive ack, ret=%d ack_param=%d sent_param=%d\n", __func__,
			result, ddev->ack_parameter, parameter);
		return;
	}
}

static void charging_dock_handle_queued_request_work(struct work_struct *work)
{
	struct charging_dock_request_work_item *devwork =
		container_of(work, struct charging_dock_request_work_item, work);

	dev_dbg(devwork->ddev->dev, "%s: enter", __func__);

	mutex_lock(&devwork->ddev->lock);
	if (!devwork->ddev->docked) {
		mutex_unlock(&devwork->ddev->lock);
		return;
	}

	charging_dock_send_vdm_request(devwork->ddev, devwork->parameter, devwork->vdo, devwork->num_vdos);

	mutex_unlock(&devwork->ddev->lock);

	kfree(devwork);
}

static void charging_dock_queue_vdm_request(struct charging_dock_device_t *ddev, u32 parameter, u32 vdo, int num_vdos)
{
	struct charging_dock_request_work_item *work = kzalloc(sizeof(*work), GFP_KERNEL);

	dev_dbg(ddev->dev, "%s: enter", __func__);

	work->parameter = parameter;
	work->vdo = vdo;
	work->num_vdos = num_vdos;
	work->ddev = ddev;

	INIT_WORK(&work->work, charging_dock_handle_queued_request_work);

	queue_work(ddev->workqueue, &work->work);
}

static void charging_dock_queue_periodic_requests(struct work_struct *work)
{
	struct charging_dock_device_t *ddev =
		container_of(work, struct charging_dock_device_t, periodic_work.work);

	dev_dbg(ddev->dev, "%s: enter", __func__);

	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 0, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 1, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 2, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 3, 1);

	schedule_delayed_work(&ddev->periodic_work, msecs_to_jiffies(ddev->broadcast_period * 60 * 1000));
}

static void charging_dock_handle_work(struct work_struct *work)
{
	struct charging_dock_device_t *ddev =
		container_of(work, struct charging_dock_device_t, work);

	dev_dbg(ddev->dev, "%s: enter", __func__);

	mutex_lock(&ddev->lock);
	if (!ddev->docked) {
		mutex_unlock(&ddev->lock);
		return;
	}

	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_FW_VERSION_NUMBER, 0, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_SERIAL_NUMBER_MLB, 0, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 0, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM, 0, 0);
	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_BROADCAST_PERIOD,
		ddev->broadcast_period, 1);

	mutex_unlock(&ddev->lock);
}

static void charging_dock_handle_work_soc(struct work_struct *work)
{
	struct charging_dock_device_t *ddev =
		container_of(work, struct charging_dock_device_t, work_soc);
	int result = 0;
	union power_supply_propval status = {0};
	enum state_of_charge_t soc = NOT_CHARGING;

	dev_dbg(ddev->dev, "%s: enter", __func__);

	mutex_lock(&ddev->lock);
	if (!ddev->docked)
		goto out;

	/* Battery status */
	result = power_supply_get_property(ddev->battery_psy,
			POWER_SUPPLY_PROP_STATUS, &status);
	if (result) {
		/*
		 * Log failure and return okay for this call with the hope
		 * that we can read battery status next time around.
		 */
		dev_err(ddev->dev, "Unable to read battery status: %d\n", result);
		goto out;
	}

	if (status.intval == POWER_SUPPLY_STATUS_CHARGING ||
		status.intval == POWER_SUPPLY_STATUS_FULL) {
		dev_dbg(ddev->dev, "system_battery_capacity=%d\n", ddev->system_battery_capacity);
		soc = (ddev->system_battery_capacity >= BATTERY_NEARLY_FULL_LEVEL) ? CHARGED : CHARGING;
	}

	if (soc != ddev->state_of_charge) {
		ddev->state_of_charge = soc;

		/* Send State of Charge only if charging or fully charged */
		if (soc != NOT_CHARGING) {
			dev_info(ddev->dev, "Sending SOC = 0x%x\n", soc);
			charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_STATE_OF_CHARGE,
				ddev->state_of_charge, 1);
		}
	}

out:
	mutex_unlock(&ddev->lock);
}

static void charging_dock_queue_initial_requests(struct charging_dock_device_t *ddev)
{
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_FW_VERSION_NUMBER, 0, 0);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_SERIAL_NUMBER_MLB, 0, 0);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM, 0, 0);
	/* request temperature for each port (0=UPA, 1=HMD, 2/3=Controller) */
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 0, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 1, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 2, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_BOARD_TEMPERATURE, 3, 1);
	/* request port config for each port (0=UPA, 2/3=Controller) */
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_PORT_CONFIGURATION, 0, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_PORT_CONFIGURATION, 2, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_PORT_CONFIGURATION, 3, 1);
	/* request connected devices info for port 0 */
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_CONNECTED_DEVICES, (0 << 8) | PARAMETER_TYPE_FW_VERSION_NUMBER, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_CONNECTED_DEVICES, (0 << 8) | PARAMETER_TYPE_SERIAL_NUMBER_MLB, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_CONNECTED_DEVICES, (0 << 8) | PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM, 1);
	/* request moisture each USBC port (0=UPA and 1=HMD) */
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_MOISTURE_DETECTED, 0, 1);
	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_MOISTURE_DETECTED, 1, 1);
}

static void charging_dock_usbvdm_connect(struct usbvdm_subscription *sub,
		u16 vid, u16 pid)
{
	struct charging_dock_device_t *ddev = usbvdm_subscriber_get_drvdata(sub);

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
	ddev->current_svid = vid;
	ddev->current_pid = pid;
	mutex_unlock(&ddev->lock);

	sysfs_notify(&ddev->dev->kobj, NULL, "docked");

	if (pid == VDM_PID_MOKU_APP) {
		charging_dock_queue_initial_requests(ddev);
		schedule_delayed_work(&ddev->periodic_work, msecs_to_jiffies(ddev->broadcast_period * 60 * 1000));
	} else {
		schedule_work(&ddev->work);

		if (ddev->send_state_of_charge) {
			ddev->state_of_charge = NOT_CHARGING;
			/* Force re-evaluation */
			batt_psy_notifier_call(&ddev->nb, PSY_EVENT_PROP_CHANGED,
					ddev->battery_psy);
		}
	}
}

static void charging_dock_usbvdm_disconnect(struct usbvdm_subscription *sub)
{
	struct charging_dock_device_t *ddev = usbvdm_subscriber_get_drvdata(sub);

	mutex_lock(&ddev->lock);
	ddev->docked = false;
	ddev->current_svid = 0;
	ddev->current_pid = 0;
	memset(&ddev->params, 0, sizeof(ddev->params));
	mutex_unlock(&ddev->lock);

	sysfs_notify(&ddev->dev->kobj, NULL, "docked");

	cancel_work_sync(&ddev->work);
	cancel_delayed_work_sync(&ddev->periodic_work);

	if (ddev->send_state_of_charge)
		cancel_work_sync(&ddev->work_soc);
}
static void parse_port_board_temp(struct charging_dock_device_t *ddev, const u32 vdo)
{
	int port_num;
	u16 board_temp;

	switch (ddev->current_pid) {
	case VDM_PID_MOKU_APP:
		// Format: byte 0: port_num, byte 1: board temp
		port_num = (vdo & 0xff);
		board_temp = (vdo >> 8) & 0xff;
		break;
	default:
		// Format: bytes[1:0]: board_temp
		port_num = 0;
		board_temp = (vdo & 0xffff);
		break;
	}

	if (port_num < 0 || port_num >= NUM_CHARGING_DOCK_PORTS) {
		dev_err(ddev->dev, "Error parsing port board temp: invalid port_num: %d", port_num);
		return;
	}
	ddev->params.port_board_temp[port_num] = board_temp;
	dev_dbg(ddev->dev, "Board temperature: port_num=%d value=%d(0x%x) vdo=0x%04x", port_num, board_temp, board_temp, vdo);
}

static void parse_port_moisture_detection(struct charging_dock_device_t *ddev, const u32 vdo)
{
	int port_num;
	int moisture_detected;

	switch (ddev->current_pid) {
	case VDM_PID_MOKU_APP:
		// Format: byte 0: port_num, byte 1: moisture_detected (0/1)
		port_num = (vdo & 0xff);
		moisture_detected = (vdo >> 8) & 0xff;
		break;
	default:
		// Format: byte 0: moisture detected (0/1)
		port_num = 0;
		moisture_detected = (vdo & 0xff);
		break;
	}

	if (port_num < 0 || port_num >= NUM_MOISTURE_DETECTION_PORTS) {
		dev_err(ddev->dev, "Error parsing port moisture detection: invalid port_num: %d", port_num);
		return;
	}
	ddev->params.moisture_detected_counts[port_num] += moisture_detected;
	dev_dbg(ddev->dev, "Moisture detected: port_num=%d value=%d", port_num, moisture_detected);
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

static void vdm_memcpy(struct charging_dock_device_t *ddev,
		u32 param, int num_vdos,
		void *dest, const void *src, size_t count)
{
	if (count > num_vdos * sizeof(u32)) {
		dev_warn(ddev->dev,
				"Received VDM (0x%02x) of insufficent length: %lu",
				param, count);
		return;
	}

	memcpy(dest, src, count);
}

static void charging_dock_usbvdm_vdm_rx(struct usbvdm_subscription *sub,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	struct charging_dock_device_t *ddev = usbvdm_subscriber_get_drvdata(sub);
	u32 protocol_type, parameter_type, sb;
	u32 attn_param, attn_param_data;
	bool acked;
	int log_chunk_size;
	int log_chunk_offset;
	int new_log_size;

	dev_dbg(ddev->dev,
		"enter: vdm_hdr=0x%x, vdos?=%d, num_vdos=%d\n",
		vdm_hdr, vdos != NULL, num_vdos);

	if (VDM_TYPE(vdm_hdr) == VDM_TYPE_STRUCTURED) {
		if (SVDM_COMMAND(vdm_hdr) != SVDM_COMMAND_ATTENTION) {
			dev_err(ddev->dev, "SVDM is not attention\n");
			return;
		}

		if (num_vdos == 0) {
			dev_err(ddev->dev, "No VDOs included in attention message\n");
			return;
		}

		attn_param_data = (vdos[0] >> 8) & 0xFF;
		attn_param = vdos[0] & 0xFF;

		dev_dbg(ddev->dev, "Attention received vdo[0]=0x%x attn_param=0x%x attn_param_data=0x%x\n",
				vdos[0], attn_param, attn_param_data);

		if (attn_param != PARAMETER_TYPE_CONNECTED_DEVICES &&
				attn_param != PARAMETER_TYPE_MOISTURE_DETECTED &&
				attn_param != PARAMETER_TYPE_PORT_CONFIGURATION) {
			dev_err(ddev->dev, "Attention received with invalid parameter: %d", attn_param);
			return;
		}
		charging_dock_queue_vdm_request(ddev, attn_param, attn_param_data, 1);
		return;
	}

	protocol_type = VDMH_PROTOCOL(vdm_hdr);
	parameter_type = VDMH_PARAMETER(vdm_hdr);

	dev_dbg(ddev->dev, "VDM protocol type = %d, VDM parameter = 0x%02x\n",
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

	if (!(protocol_type == VDM_RESPONSE || protocol_type == VDM_BROADCAST)) {
		dev_err(ddev->dev, "Error: invalid protocol_type: %d", protocol_type);
		return;
	}

	acked = VDMH_ACK(vdm_hdr);
	if (protocol_type == VDM_RESPONSE && !acked) {
		dev_warn(ddev->dev, "Unsupported request parameter 0x%x or NACK",
			parameter_type);
		return;
	}

	switch (parameter_type) {
	case PARAMETER_TYPE_FW_VERSION_NUMBER:
		if (ddev->current_pid == VDM_PID_MOKU_APP) {
			if (num_vdos != 2) {
				dev_err(ddev->dev, "Error: expected 2 VDOs for Moku FW Version but received %d", num_vdos);
			} else {
				ddev->params.fw_version = (((u64) vdos[0]) << 32) | vdos[1];
			}
		} else {
			ddev->params.legacy_fw_version = vdos[0];
		}
		break;
	case PARAMETER_TYPE_SERIAL_NUMBER_MLB:
		vdm_memcpy(ddev, parameter_type, num_vdos,
			ddev->params.serial_number_mlb,
			vdos, sizeof(ddev->params.serial_number_mlb));
		break;
	case PARAMETER_TYPE_BOARD_TEMPERATURE:
		if (num_vdos == 1)
			parse_port_board_temp(ddev, vdos[0]);
		else
			dev_err(ddev->dev, "Port board temp: wrong number of vdos: %d", num_vdos);
		break;
	case PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM:
		vdm_memcpy(ddev, parameter_type, num_vdos,
			ddev->params.serial_number_system,
			vdos, sizeof(ddev->params.serial_number_system));
		break;
	case PARAMETER_TYPE_BROADCAST_PERIOD:
		if (vdos[0] != ddev->broadcast_period)
			dev_err(ddev->dev,
			"Error: Broadcast period received: %d != sent: %d\n",
			vdos[0],
			ddev->broadcast_period);
		break;
	case PARAMETER_TYPE_STATE_OF_CHARGE:
		dev_dbg(ddev->dev, "Received State of Charge: 0x%x value=0x%02x",
			parameter_type, vdos[0]);
		if ((vdos[0] & 0x01) != ddev->state_of_charge)
			dev_err(ddev->dev,
				"Error: State-of-Charge received: %d != sent: %d\n",
				(vdos[0] & 0x01), ddev->state_of_charge);
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
		vdm_memcpy(ddev, parameter_type, num_vdos,
				ddev->log + log_chunk_offset,
				vdos, log_chunk_size);
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
	case PARAMETER_TYPE_MOISTURE_DETECTED:
		dev_dbg(ddev->dev, "Received moisture detected: 0x%x vdo=0x%04x",
			parameter_type, vdos[0]);
		if (num_vdos == 1)
			parse_port_moisture_detection(ddev, vdos[0]);
		else
			dev_err(ddev->dev, "Moisture detection: wrong number of vdos: %d", num_vdos);
		break;
	default:
		dev_err(ddev->dev, "Unsupported parameter 0x%x",
			parameter_type);
		break;
	}

	if (protocol_type == VDM_RESPONSE) {
		ddev->ack_parameter = parameter_type;
		complete(&ddev->rx_complete);
	}
}

static ssize_t reboot_into_bootloader_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	bool send_command;

	if (kstrtobool(buf, &send_command))
		return -EINVAL;

	if (!send_command)
		return count;

	charging_dock_queue_vdm_request(ddev, PARAMETER_TYPE_REBOOT_INTO_BOOTLOADER, 0, 0);

	return count;
}
static DEVICE_ATTR_WO(reboot_into_bootloader);

static ssize_t docked_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ddev->docked);
}
static DEVICE_ATTR_RO(docked);

static ssize_t vid_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%04x\n", ddev->current_svid);
}
static DEVICE_ATTR_RO(vid);

static ssize_t pid_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%04x\n", ddev->current_pid);
}
static DEVICE_ATTR_RO(pid);

static ssize_t broadcast_period_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ddev->broadcast_period);
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
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, result);
		return result;
	}

	ddev->broadcast_period = temp;

	if (ddev->docked)
		schedule_work(&ddev->work);

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

	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_LOG_TRANSMIT, VDO_LOG_TRANSMIT_START, 1);

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
		charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_LOG_CHUNK, ddev->log_chunk_num, 1);
	}

	charging_dock_send_vdm_request(ddev, PARAMETER_TYPE_LOG_TRANSMIT, VDO_LOG_TRANSMIT_STOP, 1);

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

	if (ddev->current_pid == VDM_PID_MOKU_APP) {
		return scnprintf(buf, PAGE_SIZE, "%llu\n", ddev->params.fw_version);
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", (u32) ddev->params.legacy_fw_version);
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

static ssize_t dock_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int dock_type;

	switch (ddev->current_pid) {
	case VDM_PID_BURU:
	case VDM_PID_MOKU_APP:
	case VDM_PID_UPA_18W:
	case VDM_PID_UPA_45W:
		dock_type = 1;
		break;
	case VDM_PID_SKELLIG:
	case VDM_PID_MAUI:
		dock_type = 2;
		break;
	default:
		dock_type = 0;
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", dock_type);
}
static DEVICE_ATTR_RO(dock_type);

static ssize_t port_0_moisture_detected_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ddev->params.moisture_detected_counts[0]);
}
static DEVICE_ATTR_RO(port_0_moisture_detected_count);

static ssize_t port_1_moisture_detected_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ddev->params.moisture_detected_counts[1]);
}
static DEVICE_ATTR_RO(port_1_moisture_detected_count);

static ssize_t system_battery_capacity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ddev->system_battery_capacity);
}

static ssize_t system_battery_capacity_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 0, &val)) {
		dev_err(ddev->dev, "Illegal input for system battery capacity: %s", buf);
		return -EINVAL;
	}

	if (ddev->system_battery_capacity != val) {
		ddev->system_battery_capacity = val;

		if (ddev->send_state_of_charge)
			schedule_work(&ddev->work_soc);
	}

	return count;
}
static DEVICE_ATTR_RW(system_battery_capacity);

static ssize_t port_board_temp(struct device *dev,
		char *buf, int port_num)
{
	struct charging_dock_device_t *ddev =
		(struct charging_dock_device_t *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		ddev->params.port_board_temp[port_num]);
}

static ssize_t port_0_board_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return port_board_temp(dev, buf, 0);
}
static DEVICE_ATTR_RO(port_0_board_temp);

static ssize_t port_1_board_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return port_board_temp(dev, buf, 1);
}
static DEVICE_ATTR_RO(port_1_board_temp);

static ssize_t port_2_board_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return port_board_temp(dev, buf, 2);
}
static DEVICE_ATTR_RO(port_2_board_temp);

static ssize_t port_3_board_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return port_board_temp(dev, buf, 3);
}
static DEVICE_ATTR_RO(port_3_board_temp);

static struct attribute *charging_dock_attrs[] = {
	&dev_attr_docked.attr,
	&dev_attr_broadcast_period.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_serial_number_mlb.attr,
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
	&dev_attr_port_0_board_temp.attr,
	&dev_attr_port_1_board_temp.attr,
	&dev_attr_port_2_board_temp.attr,
	&dev_attr_port_3_board_temp.attr,
	&dev_attr_dock_type.attr,
	&dev_attr_port_0_moisture_detected_count.attr,
	&dev_attr_port_1_moisture_detected_count.attr,
	&dev_attr_system_battery_capacity.attr,
	&dev_attr_vid.attr,
	&dev_attr_pid.attr,
	&dev_attr_reboot_into_bootloader.attr,
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

static int batt_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr)
{
	struct power_supply *psy = ptr;
	struct charging_dock_device_t *ddev = NULL;

	if (IS_ERR_OR_NULL(nb))
		return NOTIFY_BAD;

	ddev = container_of(nb, struct charging_dock_device_t, nb);
	if (IS_ERR_OR_NULL(ddev) || IS_ERR_OR_NULL(ddev->battery_psy))
		return NOTIFY_BAD;

	/* Only proceed if we are docked to Maui */
	if (!ddev->docked || ddev->current_pid != VDM_PID_MAUI)
		return NOTIFY_OK;

	if (psy != ddev->battery_psy || ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	schedule_work(&ddev->work_soc);

	return NOTIFY_OK;
}

static void batt_psy_register_notifier(struct charging_dock_device_t *ddev)
{
	int result = 0;

	if (ddev->nb.notifier_call == NULL) {
		dev_err(ddev->dev, "Notifier block's notifier call is NULL\n");
		return;
	}

	result = power_supply_reg_notifier(&ddev->nb);
	if (result != 0) {
		dev_err(ddev->dev, "Failed to register power supply notifier, result:%d\n",
				result);
	}
}

static void batt_psy_unregister_notifier(struct charging_dock_device_t *ddev)
{
	/*
	 * Safe to call unreg notifier even if notifier block wasn't registered
	 * with the kernel due to some prior failure.
	 */
	power_supply_unreg_notifier(&ddev->nb);
}

static int batt_psy_init(struct charging_dock_device_t *ddev)
{
	ddev->battery_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(ddev->battery_psy))
		return PTR_ERR_OR_ZERO(ddev->battery_psy);

	ddev->nb.notifier_call = batt_psy_notifier_call;

	return 0;
}

static int charging_dock_probe(struct platform_device *pdev)
{
	int i, result = 0;
	struct charging_dock_device_t *ddev;
	u32 temp_val = 0;
	u16 *usb_ids = NULL;
	int num_ids = 0;
	struct usbvdm_subscription_data *sub_data = NULL;
	const struct usbvdm_subscriber_ops ops = {
		.connect = charging_dock_usbvdm_connect,
		.disconnect = charging_dock_usbvdm_disconnect,
		.vdm = charging_dock_usbvdm_vdm_rx
	};

	dev_dbg(&pdev->dev, "enter\n");

	ddev = devm_kzalloc(&pdev->dev, sizeof(*ddev), GFP_KERNEL);
	if (!ddev)
		return -ENOMEM;

	ddev->send_state_of_charge = of_property_read_bool(pdev->dev.of_node,
			"send-state-of-charge");
	dev_dbg(&pdev->dev, "Send state of charge=%d\n", ddev->send_state_of_charge);

	if (ddev->send_state_of_charge &&
		(batt_psy_init(ddev) || !ddev->battery_psy))
			return -EPROBE_DEFER;

	ddev->current_svid = 0;

	result = of_property_read_u32(pdev->dev.of_node,
		"broadcast-period-min", &temp_val);
	if (result < 0 || temp_val > MAX_BROADCAST_PERIOD_MIN) {
		dev_err(&pdev->dev,
			"Broadcast period unavailable/invalid, failing probe, result:%d\n",
			result);
		return result;
	}
	ddev->broadcast_period = (u8)temp_val;
	dev_dbg(&pdev->dev, "Broadcast period=%d\n", ddev->broadcast_period);

	result = of_property_read_u32(pdev->dev.of_node,
		"req-ack-timeout-ms", &temp_val);
	if (result < 0) {
		dev_dbg(&pdev->dev,
			"req-ack-timeout-ms not defined, using default: %d\n",
			result);
		temp_val = REQ_ACK_TIMEOUT_MS;
	}
	ddev->req_ack_timeout_ms = temp_val;
	dev_dbg(&pdev->dev, "Req ack timeout ms=%d\n", ddev->req_ack_timeout_ms);

	mutex_init(&ddev->lock);
	INIT_WORK(&ddev->work, charging_dock_handle_work);

	ddev->workqueue = alloc_ordered_workqueue("charging_dock_request_wq", WQ_FREEZABLE | WQ_HIGHPRI);
	if (!ddev->workqueue)
		return -ENOMEM;

	INIT_DELAYED_WORK(&ddev->periodic_work, charging_dock_queue_periodic_requests);

	if (ddev->send_state_of_charge) {
		INIT_WORK(&ddev->work_soc, charging_dock_handle_work_soc);
		ddev->state_of_charge = NOT_CHARGING;
	}
	init_completion(&ddev->rx_complete);

	ddev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, ddev);

	num_ids = device_property_read_u16_array(ddev->dev, "meta,usb-ids", NULL, 0);
	if (num_ids <= 0) {
		dev_err(ddev->dev, "Error reading usb-ids property: %d", num_ids);
		return num_ids < 0 ? num_ids : -EINVAL;
	}

	usb_ids = kzalloc(sizeof(u16) * num_ids, GFP_KERNEL);
	if (!usb_ids)
		return -ENOMEM;

	result = device_property_read_u16_array(ddev->dev, "meta,usb-ids", usb_ids, num_ids);
	if (result < 0) {
		dev_err(ddev->dev, "Error reading usb-ids property: %d", result);
		kfree(usb_ids);
		return result;
	}

	INIT_LIST_HEAD(&ddev->sub_list);
	for (i = 0; i < num_ids; i += 2) {
		sub_data = devm_kzalloc(ddev->dev, sizeof(*sub_data), GFP_KERNEL);
		if (!sub_data) {
			kfree(usb_ids);
			return -ENOMEM;
		}

		sub_data->vid = usb_ids[i];
		sub_data->pid = usb_ids[i + 1];
		sub_data->sub = usbvdm_subscribe(ddev->dev, sub_data->vid, sub_data->pid, ops);
		if (IS_ERR_OR_NULL(sub_data->sub)) {
			dev_err(ddev->dev, "Failed to subscribe to VID/PID 0x%04x/0x%04x: %d",
					sub_data->vid, sub_data->pid, PTR_ERR_OR_ZERO(sub_data->sub));
			kfree(usb_ids);
			return IS_ERR(sub_data->sub) ? PTR_ERR(sub_data) : -ENODATA;
		}

		usbvdm_subscriber_set_drvdata(sub_data->sub, ddev);
		list_add(&sub_data->entry, &ddev->sub_list);
	}

	kfree(usb_ids);

	if (ddev->send_state_of_charge)
		batt_psy_register_notifier(ddev);

	charging_dock_create_sysfs(ddev);

	return result;
}

static int charging_dock_remove(struct platform_device *pdev)
{
	struct charging_dock_device_t *ddev = platform_get_drvdata(pdev);
	struct usbvdm_subscription_data *pos;

	dev_dbg(ddev->dev, "enter\n");

	cancel_work_sync(&ddev->work);
	if (ddev->send_state_of_charge) {
		batt_psy_unregister_notifier(ddev);
		cancel_work_sync(&ddev->work_soc);
	}
	list_for_each_entry(pos, &ddev->sub_list, entry)
		usbvdm_unsubscribe(pos->sub);
	mutex_destroy(&ddev->lock);
	sysfs_remove_groups(&ddev->dev->kobj, charging_dock_groups);

	/* wake up sleeping threads */
	complete_all(&ddev->rx_complete);
	destroy_workqueue(ddev->workqueue);

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
