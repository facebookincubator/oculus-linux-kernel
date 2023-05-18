// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>

#include "molokini.h"
#include "usbvdm.h"

#define DEFAULT_MOLOKINI_VID 0x2833
#define MOUNT_WORK_DELAY_SECONDS 30

#define STATUS_FULLY_DISCHARGED BIT(4)
#define STATUS_FULLY_CHARGED BIT(5)
#define STATUS_DISCHARGING BIT(6)

/*
 * Period in minutes Molokini firmware will use to send VDM traffic for
 * ON/OFF head mount status. 0 corresponds to the minimum period of 1 minute.
 * This parameter may be set in increments of 1 minute.
 */
#define MOLOKINI_FW_VDM_PERIOD_ON_HEAD 0
#define MOLOKINI_FW_VDM_PERIOD_OFF_HEAD 14

static const char * const battery_status_text[] = {
	"Not charging",
	"Charging",
	"Unknown"
};

enum battery_status_value {
	NOT_CHARGING = 0,
	CHARGING,
	UNKNOWN,
};

enum usb_charging_state {
	CHARGING_RESUME = 0,
	CHARGING_SUSPEND = 1,
};

static void molokini_mount_status_work(struct work_struct *work);
static void molokini_psy_register_notifier(struct molokini_pd *mpd);
static int molokini_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr);
static void molokini_psy_unregister_notifier(struct molokini_pd *mpd);
static void set_usb_charging_state(struct molokini_pd *mpd,
		enum usb_charging_state state);

static void convert_battery_status(struct molokini_pd *mpd, u16 status)
{
	if (!(status & STATUS_FULLY_DISCHARGED) && (status & STATUS_DISCHARGING))
		strcpy(mpd->params.battery_status, battery_status_text[NOT_CHARGING]);
	else if ((status & STATUS_FULLY_CHARGED) || !(status & STATUS_DISCHARGING))
		strcpy(mpd->params.battery_status, battery_status_text[CHARGING]);
	else
		strcpy(mpd->params.battery_status, battery_status_text[UNKNOWN]);
}

static void vdm_connect(struct usbpd_svid_handler *hdlr,
		bool supports_usb_comm)
{
	struct molokini_pd *mpd =
		container_of(hdlr, struct molokini_pd, vdm_handler);

	dev_dbg(mpd->dev, "enter: usb-com=%d\n", supports_usb_comm);

	/*
	 * Client notifications only occur during connect/disconnect
	 * state transitions. This should not be called when Molokini
	 * is already connected.
	 */
	WARN_ON(mpd->connected);

	mutex_lock(&mpd->lock);
	mpd->connected = true;
	mpd->last_mount_ack = MOLOKINI_FW_UNKNOWN;
	mutex_unlock(&mpd->lock);

	schedule_delayed_work(&mpd->dwork, 0);
	molokini_psy_register_notifier(mpd);

	/* Force re-evaluation */
	molokini_psy_notifier_call(&mpd->nb, PSY_EVENT_PROP_CHANGED,
			mpd->battery_psy);
}

static void vdm_disconnect(struct usbpd_svid_handler *hdlr)
{
	struct molokini_pd *mpd =
		container_of(hdlr, struct molokini_pd, vdm_handler);

	dev_dbg(mpd->dev, "enter\n");

	/*
	 * Client notifications only occur during connect/disconnect
	 * state transitions. This should not be called when Molokini
	 * is already disconnected.
	 */
	WARN_ON(!mpd->connected);

	mutex_lock(&mpd->lock);
	mpd->connected = false;
	mpd->last_mount_ack = MOLOKINI_FW_UNKNOWN;
	mutex_unlock(&mpd->lock);

	cancel_delayed_work_sync(&mpd->dwork);
	molokini_psy_unregister_notifier(mpd);

	/* Re-enable USB input charging path if disabled */
	set_usb_charging_state(mpd, CHARGING_RESUME);
}

static void vdm_received(struct usbpd_svid_handler *hdlr, u32 vdm_hdr,
		const u32 *vdos, int num_vdos)
{
	u32 protocol_type, parameter_type, sb, high_bytes, acked;
	struct molokini_pd *mpd =
		container_of(hdlr, struct molokini_pd, vdm_handler);

	dev_dbg(mpd->dev,
		"enter: vdm_hdr=0x%x, vdos?=%d, num_vdos=%d\n",
		vdm_hdr, vdos != NULL, num_vdos);

	protocol_type = VDMH_PROTOCOL(vdm_hdr);
	parameter_type = VDMH_PARAMETER(vdm_hdr);

	if (protocol_type == VDM_RESPONSE) {
		acked = VDMH_ACK(vdm_hdr);
		if (parameter_type != MOLOKINI_FW_HMD_MOUNTED || !acked) {
			dev_warn(mpd->dev, "Unsupported response parameter 0x%x or NACK(ack=%d)",
				parameter_type, acked);
			return;
		}

		dev_info(mpd->dev,
			"Received mount status ack response code 0x%x",
			vdos[0]);

		mpd->last_mount_ack = VDO_MOUNT_STATE_ACK_STATUS(vdos[0]);

		return;
	}

	if (protocol_type != VDM_BROADCAST) {
		dev_warn(mpd->dev, "Usupported Protocol=%d\n", protocol_type);
		return;
	}

	if (num_vdos == 0) {
		dev_warn(mpd->dev, "Empty VDO packets vdm_hdr=0x%x\n", vdm_hdr);
		return;
	}

	sb = VDMH_SIZE(vdm_hdr);
	if (sb >= ARRAY_SIZE(vdm_size_bytes)) {
		dev_warn(mpd->dev, "Invalid size byte code, sb=%d", sb);
		return;
	}

	high_bytes = VDMH_HIGH(vdm_hdr);
	switch (parameter_type) {
	case MOLOKINI_FW_SERIAL:
		memcpy(mpd->params.serial,
			vdos, sizeof(mpd->params.serial));
		break;
	case MOLOKINI_FW_SERIAL_BATTERY:
		memcpy(mpd->params.serial_battery,
			vdos, sizeof(mpd->params.serial_battery));
		break;
	case MOLOKINI_FW_TEMP_FG:
		mpd->params.temp_fg = vdos[0];
		break;
	case MOLOKINI_FW_VOLTAGE:
		mpd->params.voltage = vdos[0];
		break;
	case MOLOKINI_FW_BATT_STATUS:
		convert_battery_status(mpd, vdos[0]);
		break;
	case MOLOKINI_FW_CURRENT:
		mpd->params.icurrent = vdos[0]; /* negative range */
		break;
	case MOLOKINI_FW_REMAINING_CAPACITY:
		mpd->params.remaining_capacity = vdos[0];
		break;
	case MOLOKINI_FW_FCC:
		mpd->params.fcc = vdos[0];
		break;
	case MOLOKINI_FW_CYCLE_COUNT:
		mpd->params.cycle_count = vdos[0];
		break;
	case MOLOKINI_FW_RSOC:
		mpd->params.rsoc = vdos[0];
		break;
	case MOLOKINI_FW_SOH:
		mpd->params.soh = vdos[0];
		break;
	case MOLOKINI_FW_DEVICE_NAME:
		if (mpd->params.device_name[0] == '\0' &&
			vdm_size_bytes[sb] <=
				sizeof(u32) * num_vdos &&
			vdm_size_bytes[sb] <=
				sizeof(mpd->params.device_name)) {
			memcpy(mpd->params.device_name,
				vdos, vdm_size_bytes[sb]);
		}
		break;
	case MOLOKINI_FW_LDB1:
		memcpy(mpd->params.lifetime1_lower, vdos,
			sizeof(mpd->params.lifetime1_lower));
		memcpy(mpd->params.lifetime1_higher,
			vdos + LIFETIME_1_LOWER_LEN,
			sizeof(mpd->params.lifetime1_higher));
		break;
	case MOLOKINI_FW_LDB3:
		mpd->params.lifetime3 = vdos[0];
		break;
	case MOLOKINI_FW_LDB4:
		memcpy(mpd->params.lifetime4,
			vdos, sizeof(mpd->params.lifetime4));
		break;
	case MOLOKINI_FW_LDB6:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[0],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[0] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB7:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[1],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[1] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB8:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[2],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[2] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB9:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[3],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[3] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB10:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[4],
					vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[4] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB11:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[5],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[5] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB12:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[6],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[6] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_MANUFACTURER_INFO1:
		mpd->params.manufacturer_info_a = vdos[0];
		break;
	case MOLOKINI_FW_MANUFACTURER_INFO2:
		mpd->params.manufacturer_info_b = vdos[0];
		break;
	case MOLOKINI_FW_MANUFACTURER_INFO3:
		mpd->params.manufacturer_info_c = vdos[0];
		break;
	case MOLOKINI_FW_CHARGER_PLUGGED:
		mpd->params.charger_plugged = vdos[0];
		/* Force re-evaluation on charger status update */
		molokini_psy_notifier_call(&mpd->nb, PSY_EVENT_PROP_CHANGED,
				mpd->battery_psy);
		break;
	}
}

static ssize_t charger_plugged_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", mpd->params.charger_plugged);
}

static ssize_t charger_plugged_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for charger_plugged: %s", buf);
		return result;
	}

	mpd->params.charger_plugged = temp;

	/* Force re-evaluation */
	molokini_psy_notifier_call(&mpd->nb, PSY_EVENT_PROP_CHANGED,
			mpd->battery_psy);
	return count;
}
static DEVICE_ATTR_RW(charger_plugged);

static ssize_t connected_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", mpd->connected);
}

static ssize_t connected_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for connected: %s", buf);
		return result;
	}

	mpd->connected = temp;

	return count;
}
static DEVICE_ATTR_RW(connected);

static ssize_t mount_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->mount_state);
}

static ssize_t mount_state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);
	int result;
	u32 temp;

	result = kstrtou32(buf, 10, &temp);
	if (result < 0 || temp > MOLOKINI_FW_UNKNOWN) {
		dev_err(dev, "Illegal input for mount_state: %s", buf);
		return result;
	}

	result = mutex_lock_interruptible(&mpd->lock);
	if (result != 0) {
		dev_err(dev, "Failed to get mutex: %d", result);
		return result;
	}

	mpd->mount_state = temp;

	if (mpd->connected) {
		cancel_delayed_work(&mpd->dwork);
		schedule_delayed_work(&mpd->dwork, 0);
	}
	mutex_unlock(&mpd->lock);

	/* Force re-evaluation */
	molokini_psy_notifier_call(&mpd->nb, PSY_EVENT_PROP_CHANGED,
			mpd->battery_psy);
	return count;
}
static DEVICE_ATTR_RW(mount_state);

static ssize_t rsoc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.rsoc);
}

static ssize_t rsoc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);
	int result;
	u8 temp;

	result = kstrtou8(buf, 10, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input forrsoc: %s", buf);
		return result;
	}

	mpd->params.rsoc = temp;

	return count;
}
static DEVICE_ATTR_RW(rsoc);

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);
	int result;

	result = mutex_lock_interruptible(&mpd->lock);
	if (result != 0) {
		dev_err(dev, "Failed to get mutex: %d", result);
		return result;
	}

	result = scnprintf(buf, PAGE_SIZE, "%s\n", mpd->params.battery_status);
	mutex_unlock(&mpd->lock);

	return result;
}

static ssize_t status_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);
	int result;

	result = mutex_lock_interruptible(&mpd->lock);
	if (result != 0) {
		dev_err(dev, "Failed to get mutex: %d", result);
		return result;
	}

	snprintf(mpd->params.battery_status, sizeof(mpd->params.battery_status), "%s", buf);
	mutex_unlock(&mpd->lock);

	return count;
}
static DEVICE_ATTR_RW(status);

static ssize_t charging_suspend_disable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", mpd->charging_suspend_disable);
}

static ssize_t charging_suspend_disable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for charging suspend disable: %s", buf);
		return result;
	}

	mpd->charging_suspend_disable = temp;

	/* Force re-evaluation */
	molokini_psy_notifier_call(&mpd->nb, PSY_EVENT_PROP_CHANGED,
			mpd->battery_psy);

	return count;
}
static DEVICE_ATTR_RW(charging_suspend_disable);

static ssize_t serial_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", mpd->params.serial);
}
static DEVICE_ATTR_RO(serial);

static ssize_t serial_battery_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", mpd->params.serial_battery);
}
static DEVICE_ATTR_RO(serial_battery);

static ssize_t temp_fg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_fg);
}
static DEVICE_ATTR_RO(temp_fg);

static ssize_t voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.voltage);
}
static DEVICE_ATTR_RO(voltage);

static ssize_t icurrent_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.icurrent);
}
static DEVICE_ATTR_RO(icurrent);

static ssize_t remaining_capacity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.remaining_capacity);
}
static DEVICE_ATTR_RO(remaining_capacity);

static ssize_t fcc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.fcc);
}
static DEVICE_ATTR_RO(fcc);

static ssize_t cycle_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.cycle_count);
}
static DEVICE_ATTR_RO(cycle_count);

static ssize_t soh_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.soh);
}
static DEVICE_ATTR_RO(soh);

static ssize_t device_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", mpd->params.device_name);
}
static DEVICE_ATTR_RO(device_name);

static ssize_t cell_1_max_voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime1_lower[0]);
}
static DEVICE_ATTR_RO(cell_1_max_voltage);

static ssize_t cell_1_min_voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime1_lower[1]);
}
static DEVICE_ATTR_RO(cell_1_min_voltage);

static ssize_t max_charge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_lower[2]);
}
static DEVICE_ATTR_RO(max_charge_current);

static ssize_t max_discharge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_lower[3]);
}
static DEVICE_ATTR_RO(max_discharge_current);

static ssize_t max_avg_discharge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_lower[4]);
}
static DEVICE_ATTR_RO(max_avg_discharge_current);

static ssize_t max_avg_dsg_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_lower[5]);
}
static DEVICE_ATTR_RO(max_avg_dsg_power);

static ssize_t max_temp_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_higher[0]);
}
static DEVICE_ATTR_RO(max_temp_cell);

static ssize_t min_temp_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_higher[1]);
}
static DEVICE_ATTR_RO(min_temp_cell);

static ssize_t max_temp_int_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_higher[2]);
}
static DEVICE_ATTR_RO(max_temp_int_sensor);

static ssize_t min_temp_int_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", mpd->params.lifetime1_higher[3]);
}
static DEVICE_ATTR_RO(min_temp_int_sensor);

static ssize_t total_fw_runtime_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime3);
}
static DEVICE_ATTR_RO(total_fw_runtime);

static ssize_t num_valid_charge_terminations_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime4[0]);
}
static DEVICE_ATTR_RO(num_valid_charge_terminations);

static ssize_t last_valid_charge_term_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime4[1]);
}
static DEVICE_ATTR_RO(last_valid_charge_term);

static ssize_t num_qmax_updates_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime4[2]);
}
static DEVICE_ATTR_RO(num_qmax_updates);

static ssize_t last_qmax_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime4[3]);
}
static DEVICE_ATTR_RO(last_qmax_update);

static ssize_t num_ra_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime4[4]);
}
static DEVICE_ATTR_RO(num_ra_update);

static ssize_t last_ra_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.lifetime4[5]);
}
static DEVICE_ATTR_RO(last_ra_update);

// ut

static ssize_t t_ut_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][0]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_a);

static ssize_t t_ut_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][1]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_b);

static ssize_t t_ut_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][2]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_c);

static ssize_t t_ut_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][3]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_d);

static ssize_t t_ut_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][4]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_e);

static ssize_t t_ut_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][5]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_f);

static ssize_t t_ut_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][6]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_g);

static ssize_t t_ut_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[0][7]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_h);

// lt

static ssize_t t_lt_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][0]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_a);

static ssize_t t_lt_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][1]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_b);

static ssize_t t_lt_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][2]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_c);

static ssize_t t_lt_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][3]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_d);

static ssize_t t_lt_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][4]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_e);

static ssize_t t_lt_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][5]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_f);

static ssize_t t_lt_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][6]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_g);

static ssize_t t_lt_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[1][7]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_h);

// stl

static ssize_t t_stl_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][0]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_a);

static ssize_t t_stl_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][1]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_b);

static ssize_t t_stl_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][2]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_c);

static ssize_t t_stl_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][3]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_d);

static ssize_t t_stl_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][4]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_e);

static ssize_t t_stl_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][5]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_f);

static ssize_t t_stl_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][6]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_g);

static ssize_t t_stl_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[2][7]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_h);

// rt

static ssize_t t_rt_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][0]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_a);

static ssize_t t_rt_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][1]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_b);

static ssize_t t_rt_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][2]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_c);

static ssize_t t_rt_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][3]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_d);

static ssize_t t_rt_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][4]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_e);

static ssize_t t_rt_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][5]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_f);

static ssize_t t_rt_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][6]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_g);

static ssize_t t_rt_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[3][7]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_h);

// sth

static ssize_t t_sth_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][0]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_a);

static ssize_t t_sth_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][1]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_b);

static ssize_t t_sth_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][2]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_c);

static ssize_t t_sth_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][3]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_d);

static ssize_t t_sth_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][4]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_e);

static ssize_t t_sth_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][5]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_f);

static ssize_t t_sth_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][6]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_g);

static ssize_t t_sth_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[4][7]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_h);

// ht

static ssize_t t_ht_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][0]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_a);

static ssize_t t_ht_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][1]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_b);

static ssize_t t_ht_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][2]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_c);

static ssize_t t_ht_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][3]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_d);

static ssize_t t_ht_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][4]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_e);

static ssize_t t_ht_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][5]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_f);

static ssize_t t_ht_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][6]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_g);

static ssize_t t_ht_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[5][7]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_h);

// ot

static ssize_t t_ot_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][0]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_a);

static ssize_t t_ot_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][1]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_b);

static ssize_t t_ot_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][2]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_c);

static ssize_t t_ot_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][3]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_d);

static ssize_t t_ot_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][4]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_e);

static ssize_t t_ot_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][5]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_f);

static ssize_t t_ot_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][6]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_g);

static ssize_t t_ot_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct molokini_pd *mpd =
		(struct molokini_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", mpd->params.temp_zones[6][7]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_h);

static struct attribute *molokini_attrs[] = {
	&dev_attr_charger_plugged.attr,
	&dev_attr_connected.attr,
	&dev_attr_mount_state.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_status.attr,
	&dev_attr_charging_suspend_disable.attr,
	&dev_attr_serial.attr,
	&dev_attr_serial_battery.attr,
	&dev_attr_temp_fg.attr,
	&dev_attr_voltage.attr,
	&dev_attr_icurrent.attr,
	&dev_attr_remaining_capacity.attr,
	&dev_attr_fcc.attr,
	&dev_attr_cycle_count.attr,
	&dev_attr_soh.attr,
	&dev_attr_device_name.attr,
	&dev_attr_cell_1_max_voltage.attr,
	&dev_attr_cell_1_min_voltage.attr,
	&dev_attr_max_charge_current.attr,
	&dev_attr_max_discharge_current.attr,
	&dev_attr_max_avg_discharge_current.attr,
	&dev_attr_max_avg_dsg_power.attr,
	&dev_attr_max_temp_cell.attr,
	&dev_attr_min_temp_cell.attr,
	&dev_attr_max_temp_int_sensor.attr,
	&dev_attr_min_temp_int_sensor.attr,

	&dev_attr_total_fw_runtime.attr,
	&dev_attr_num_valid_charge_terminations.attr,
	&dev_attr_last_valid_charge_term.attr,
	&dev_attr_num_qmax_updates.attr,
	&dev_attr_last_qmax_update.attr,
	&dev_attr_num_ra_update.attr,
	&dev_attr_last_ra_update.attr,

	// ut
	&dev_attr_t_ut_rsoc_a.attr,
	&dev_attr_t_ut_rsoc_b.attr,
	&dev_attr_t_ut_rsoc_c.attr,
	&dev_attr_t_ut_rsoc_d.attr,
	&dev_attr_t_ut_rsoc_e.attr,
	&dev_attr_t_ut_rsoc_f.attr,
	&dev_attr_t_ut_rsoc_g.attr,
	&dev_attr_t_ut_rsoc_h.attr,

	// lt
	&dev_attr_t_lt_rsoc_a.attr,
	&dev_attr_t_lt_rsoc_b.attr,
	&dev_attr_t_lt_rsoc_c.attr,
	&dev_attr_t_lt_rsoc_d.attr,
	&dev_attr_t_lt_rsoc_e.attr,
	&dev_attr_t_lt_rsoc_f.attr,
	&dev_attr_t_lt_rsoc_g.attr,
	&dev_attr_t_lt_rsoc_h.attr,

	// stl
	&dev_attr_t_stl_rsoc_a.attr,
	&dev_attr_t_stl_rsoc_b.attr,
	&dev_attr_t_stl_rsoc_c.attr,
	&dev_attr_t_stl_rsoc_d.attr,
	&dev_attr_t_stl_rsoc_e.attr,
	&dev_attr_t_stl_rsoc_f.attr,
	&dev_attr_t_stl_rsoc_g.attr,
	&dev_attr_t_stl_rsoc_h.attr,

	// rt
	&dev_attr_t_rt_rsoc_a.attr,
	&dev_attr_t_rt_rsoc_b.attr,
	&dev_attr_t_rt_rsoc_c.attr,
	&dev_attr_t_rt_rsoc_d.attr,
	&dev_attr_t_rt_rsoc_e.attr,
	&dev_attr_t_rt_rsoc_f.attr,
	&dev_attr_t_rt_rsoc_g.attr,
	&dev_attr_t_rt_rsoc_h.attr,

	// sth
	&dev_attr_t_sth_rsoc_a.attr,
	&dev_attr_t_sth_rsoc_b.attr,
	&dev_attr_t_sth_rsoc_c.attr,
	&dev_attr_t_sth_rsoc_d.attr,
	&dev_attr_t_sth_rsoc_e.attr,
	&dev_attr_t_sth_rsoc_f.attr,
	&dev_attr_t_sth_rsoc_g.attr,
	&dev_attr_t_sth_rsoc_h.attr,

	// ht
	&dev_attr_t_ht_rsoc_a.attr,
	&dev_attr_t_ht_rsoc_b.attr,
	&dev_attr_t_ht_rsoc_c.attr,
	&dev_attr_t_ht_rsoc_d.attr,
	&dev_attr_t_ht_rsoc_e.attr,
	&dev_attr_t_ht_rsoc_f.attr,
	&dev_attr_t_ht_rsoc_g.attr,
	&dev_attr_t_ht_rsoc_h.attr,

	// ot
	&dev_attr_t_ot_rsoc_a.attr,
	&dev_attr_t_ot_rsoc_b.attr,
	&dev_attr_t_ot_rsoc_c.attr,
	&dev_attr_t_ot_rsoc_d.attr,
	&dev_attr_t_ot_rsoc_e.attr,
	&dev_attr_t_ot_rsoc_f.attr,
	&dev_attr_t_ot_rsoc_g.attr,
	&dev_attr_t_ot_rsoc_h.attr,

	NULL,
};
ATTRIBUTE_GROUPS(molokini);

static void molokini_create_sysfs(struct molokini_pd *mpd)
{
	int result;

	/* Mount state node */
	result = sysfs_create_groups(&mpd->dev->kobj, molokini_groups);
	if (result != 0)
		dev_err(mpd->dev, "Error creating sysfs entries: %d\n", result);
}

static void molokini_default_sysfs_values(struct molokini_pd *mpd)
{
	strcpy(mpd->params.battery_status, battery_status_text[UNKNOWN]);
}

static void molokini_mount_status_work(struct work_struct *work)
{
	struct molokini_pd *mpd =
		container_of(work, struct molokini_pd, dwork.work);
	int result;
	u32 mount_state_header = 0;
	u32 mount_state_vdo = mpd->mount_state;

	dev_dbg(mpd->dev, "%s: enter", __func__);

	mutex_lock(&mpd->lock);
	if (!mpd->connected) {
		mutex_unlock(&mpd->lock);
		return;
	}

	/* Send HMD Mount request if needed */
	if (mpd->mount_state != mpd->last_mount_ack) {
		mount_state_header =
			VDMH_CONSTRUCT(
				mpd->vdm_handler.svid,
				0, 1, 0, 1,
				MOLOKINI_FW_HMD_MOUNTED);

		if (mpd->mount_state == 0)
			mount_state_vdo |= (MOLOKINI_FW_VDM_PERIOD_OFF_HEAD << 8);
		else
			mount_state_vdo |= (MOLOKINI_FW_VDM_PERIOD_ON_HEAD << 8);

		result = usbpd_send_vdm(mpd->upd, mount_state_header,
			&mount_state_vdo, 1);
		if (result != 0)
			dev_err(mpd->dev,
				"Error sending HMD mount request: %d", result);
		else
			dev_dbg(mpd->dev,
				"Sent HMD mount request: header=0x%x mount_state_vdo=0x%x",
				mount_state_header, mount_state_vdo);
	}

	/*
	 * Schedule periodically to ensure that molokini ACKs the
	 * current mount state.
	 */
	schedule_delayed_work(&mpd->dwork, MOUNT_WORK_DELAY_SECONDS * HZ);

	mutex_unlock(&mpd->lock);
}

static inline const char *usb_charging_state(enum usb_charging_state state)
{
	switch (state) {
	case CHARGING_RESUME:
		return "charging resumed";
	case CHARGING_SUSPEND:
		return "charging suspended";
	default:
		return "unknown";
	}
}

static void set_usb_charging_state(struct molokini_pd *mpd,
		enum usb_charging_state state)
{
	union power_supply_propval val = {0};
	int result = 0;

	if (IS_ERR_OR_NULL(mpd->usb_psy)) {
		dev_err(mpd->dev, "USB power supply handle is invalid.\n");
		return;
	}

	result = power_supply_get_property(mpd->usb_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
	if (result) {
		dev_err(mpd->dev, "Unable to read charging state: %d\n", result);
		return;
	}

	if (state == val.intval) {
		dev_dbg(mpd->dev, "Current USB charging state is %s. No action needed.\n",
				usb_charging_state(state));
		return;
	}

	val.intval = state;

	result = power_supply_set_property(mpd->usb_psy,
				POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
	if (result) {
		dev_err(mpd->dev, "Unable to update USB charging state: %d\n", result);
		return;
	}

	dev_dbg(mpd->dev,
			"USB charging state updated successfully. New state: %s\n",
			usb_charging_state(state));
}

static void handle_battery_capacity_change(struct molokini_pd *mpd,
		u32 capacity)
{
	dev_dbg(mpd->dev,
			"Battery capacity change event. Capacity = %d, connected = %d, mount state = %d, charger plugged = %d\n",
			capacity, mpd->connected, mpd->mount_state,
			mpd->params.charger_plugged);

	if ((mpd->charging_suspend_disable) ||
		(!mpd->connected) ||
		(mpd->mount_state != 0) ||
		(mpd->params.charger_plugged) ||
		(capacity < mpd->charging_resume_threshold)) {
		/* Attempt to resume charging if suspended previously */
		set_usb_charging_state(mpd, CHARGING_RESUME);
		return;
	}

	if (capacity >= mpd->charging_suspend_threshold) {
		/* Attempt to suspend charging if not suspended previously */
		set_usb_charging_state(mpd, CHARGING_SUSPEND);
	}
}

static int molokini_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr)
{
	int result = 0;
	struct power_supply *psy = ptr;
	struct molokini_pd *mpd = NULL;
	union power_supply_propval val;

	if (IS_ERR_OR_NULL(nb))
		return NOTIFY_BAD;

	mpd = container_of(nb, struct molokini_pd, nb);
	if (IS_ERR_OR_NULL(mpd) || IS_ERR_OR_NULL(mpd->battery_psy)) {
		dev_err(mpd->dev, "PSY notifier provided object handle is invalid\n");
		return NOTIFY_BAD;
	}

	if (psy != mpd->battery_psy || ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	result = power_supply_get_property(mpd->battery_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
	if (result) {
		/*
		 * Log failure and return okay for this call with the hope
		 * that we can read battery capacity next time around.
		 */
		dev_err(mpd->dev, "Unable to read battery capacity: %d\n", result);
		return NOTIFY_OK;
	}

	handle_battery_capacity_change(mpd, val.intval);

	return NOTIFY_OK;
}

static void molokini_psy_register_notifier(struct molokini_pd *mpd)
{
	int result = 0;

	if (mpd->nb.notifier_call == NULL) {
		dev_err(mpd->dev, "Notifier block's notifier call is NULL\n");
		return;
	}

	result = power_supply_reg_notifier(&mpd->nb);
	if (result != 0) {
		dev_err(mpd->dev, "Failed to register power supply notifier, result:%d\n",
				result);
	}
}

static void molokini_psy_unregister_notifier(struct molokini_pd *mpd)
{
	/*
	 * Safe to call unreg notifier even if notifier block wasn't registered
	 * with the kernel due to some prior failure.
	 */
	power_supply_unreg_notifier(&mpd->nb);
}

static void molokini_psy_init(struct molokini_pd *mpd)
{
	mpd->nb.notifier_call = NULL;
	mpd->battery_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(mpd->battery_psy)) {
		dev_err(mpd->dev, "Failed to get battery power supply\n");
		return;
	}

	mpd->usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(mpd->usb_psy)) {
		dev_err(mpd->dev, "Failed to get USB power supply\n");
		return;
	}
	mpd->nb.notifier_call = molokini_psy_notifier_call;
}

static int molokini_probe(struct platform_device *pdev)
{
	int result = 0;
	struct molokini_pd *mpd;

	dev_dbg(&pdev->dev, "enter\n");

	mpd = devm_kzalloc(
			&pdev->dev, sizeof(*mpd), GFP_KERNEL);
	if (!mpd)
		return -ENOMEM;

	mpd->upd = devm_usbpd_get_by_phandle(&pdev->dev, "battery-usbpd");
	if (IS_ERR_OR_NULL(mpd->upd)) {
		dev_err(&pdev->dev, "devm_usbpd_get_by_phandle failed: %ld\n",
				PTR_ERR(mpd->upd));
		return PTR_ERR(mpd->upd);
	}

	mpd->vdm_handler.connect = vdm_connect;
	mpd->vdm_handler.disconnect = vdm_disconnect;
	mpd->vdm_handler.vdm_received = vdm_received;

	result = of_property_read_u16(pdev->dev.of_node,
		"svid", &mpd->vdm_handler.svid);
	if (result < 0) {
		mpd->vdm_handler.svid = DEFAULT_MOLOKINI_VID;
		dev_err(&pdev->dev,
			"svid unavailable, defaulting to svid=%x, result:%d\n",
			DEFAULT_MOLOKINI_VID, result);
	}
	dev_dbg(&pdev->dev, "Property svid=%x", mpd->vdm_handler.svid);

	result = of_property_read_u32(pdev->dev.of_node,
		"charging-suspend-thresh", &mpd->charging_suspend_threshold);
	if (result < 0) {
		dev_err(&pdev->dev,
			"Charging suspend threshold unavailable, failing probe, result:%d\n",
				result);
		return result;
	}
	dev_dbg(&pdev->dev, "Charging suspend threshold=%d\n",
		mpd->charging_suspend_threshold);

	result = of_property_read_u32(pdev->dev.of_node,
		"charging-resume-thresh", &mpd->charging_resume_threshold);
	if (result < 0) {
		dev_err(&pdev->dev,
			"Charging resume threshold unavailable, failing probe, result:%d\n",
				result);
		return result;
	}
	dev_dbg(&pdev->dev, "Charging resume threshold=%d\n",
		mpd->charging_resume_threshold);

	if (mpd->charging_resume_threshold >= mpd->charging_suspend_threshold) {
		dev_err(&pdev->dev, "Invalid charging thresholds set, failing probe\n");
		return -EINVAL;
	}

	mutex_init(&mpd->lock);

	mpd->last_mount_ack = MOLOKINI_FW_UNKNOWN;
	INIT_DELAYED_WORK(&mpd->dwork, molokini_mount_status_work);

	/* Register VDM callbacks */
	result = usbpd_register_svid(mpd->upd, &mpd->vdm_handler);
	if (result != 0) {
		dev_err(&pdev->dev, "Failed to register vdm handler");
		return result;
	}
	dev_dbg(&pdev->dev, "Registered svid 0x%04x",
		mpd->vdm_handler.svid);

	mpd->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, mpd);

	/* Query power supply and register for events */
	molokini_psy_init(mpd);

	/* Create nodes here. */
	molokini_create_sysfs(mpd);
	molokini_default_sysfs_values(mpd);

	return result;
}

static int molokini_remove(struct platform_device *pdev)
{
	struct molokini_pd *mpd = platform_get_drvdata(pdev);

	dev_dbg(mpd->dev, "enter\n");

	if (mpd->upd != NULL)
		usbpd_unregister_svid(mpd->upd, &mpd->vdm_handler);

	mutex_lock(&mpd->lock);
	mpd->connected = false;
	mutex_unlock(&mpd->lock);
	cancel_delayed_work_sync(&mpd->dwork);
	set_usb_charging_state(mpd, CHARGING_RESUME);
	molokini_psy_unregister_notifier(mpd);
	mutex_destroy(&mpd->lock);

	sysfs_remove_groups(&mpd->dev->kobj, molokini_groups);

	return 0;
}

/* Driver Info */
static const struct of_device_id molokini_match_table[] = {
		{ .compatible = "oculus,molokini", },
		{ },
};

static struct platform_driver molokini_driver = {
	.driver = {
		.name = "oculus,molokini",
		.of_match_table = molokini_match_table,
		.owner = THIS_MODULE,
	},
	.probe = molokini_probe,
	.remove = molokini_remove,
};

static int __init molokini_init(void)
{
	platform_driver_register(&molokini_driver);
	return 0;
}

static void __exit molokini_exit(void)
{
	platform_driver_unregister(&molokini_driver);
}

module_init(molokini_init);
module_exit(molokini_exit);

MODULE_DESCRIPTION("Molokini driver");
MODULE_LICENSE("GPL");
