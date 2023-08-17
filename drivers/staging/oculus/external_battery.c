// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>

#include "external_battery.h"
#include "usbvdm.h"

#define DEFAULT_EXT_BATT_VID 0x2833
#define MOUNT_WORK_DELAY_SECONDS 30
#define PR_SWAP_SOC_THRESHOLD 95

#define STATUS_FULLY_DISCHARGED BIT(4)
#define STATUS_FULLY_CHARGED BIT(5)
#define STATUS_DISCHARGING BIT(6)

/*
 * Period in minutes Molokini firmware will use to send VDM traffic for
 * ON/OFF head mount status. 0 corresponds to the minimum period of 1 minute.
 * This parameter may be set in increments of 1 minute.
 */
#define EXT_BATT_FW_VDM_PERIOD_ON_HEAD 0
#define EXT_BATT_FW_VDM_PERIOD_OFF_HEAD 14

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

static void ext_batt_mount_status_work(struct work_struct *work);
static void ext_batt_psy_notifier_work(struct work_struct *work);
static void ext_batt_psy_register_notifier(struct ext_batt_pd *pd);
static int ext_batt_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr);
static void ext_batt_psy_unregister_notifier(struct ext_batt_pd *pd);
static void set_usb_charging_state(struct ext_batt_pd *pd,
		enum usb_charging_state state);

static void convert_battery_status(struct ext_batt_pd *pd, u16 status)
{
	if (!(status & STATUS_FULLY_DISCHARGED) && (status & STATUS_DISCHARGING))
		strcpy(pd->params.battery_status, battery_status_text[NOT_CHARGING]);
	else if ((status & STATUS_FULLY_CHARGED) || !(status & STATUS_DISCHARGING))
		strcpy(pd->params.battery_status, battery_status_text[CHARGING]);
	else
		strcpy(pd->params.battery_status, battery_status_text[UNKNOWN]);
}

void ext_batt_vdm_connect(struct ext_batt_pd *pd, bool usb_comm)
{
	dev_dbg(pd->dev, "%s: enter: usb-com=%d\n", __func__, usb_comm);

	/*
	 * Client notifications only occur during connect/disconnect
	 * state transitions. This should not be called when Molokini
	 * is already connected.
	 */
	WARN_ON(pd->connected);

	mutex_lock(&pd->lock);
	pd->connected = true;
	pd->last_mount_ack = EXT_BATT_FW_UNKNOWN;
	mutex_unlock(&pd->lock);

	schedule_delayed_work(&pd->mount_state_work, 0);

	cancel_delayed_work_sync(&pd->dock_state_dwork);
	schedule_delayed_work(&pd->dock_state_dwork, 0);

	/* Force re-evaluation */
	ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
			pd->battery_psy);
}

void ext_batt_vdm_disconnect(struct ext_batt_pd *pd)
{
	dev_dbg(pd->dev, "%s: enter", __func__);

	/*
	 * Client notifications only occur during connect/disconnect
	 * state transitions. This should not be called when Molokini
	 * is already disconnected.
	 */
	/* TODO(T153142293): prevent vdm_disconenct from being called if not previously connected */
	/* WARN_ON(!pd->connected); */

	mutex_lock(&pd->lock);
	pd->connected = false;
	pd->last_mount_ack = EXT_BATT_FW_UNKNOWN;
	mutex_unlock(&pd->lock);

	cancel_delayed_work_sync(&pd->mount_state_work);
	cancel_delayed_work_sync(&pd->dock_state_dwork);

	/* Re-enable USB input charging path if disabled */
	set_usb_charging_state(pd, CHARGING_RESUME);
}

void ext_batt_vdm_received(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	u32 protocol_type, parameter_type, sb, high_bytes, acked;
	int rc = 0;
	union power_supply_propval power_role = {0};

	dev_dbg(pd->dev,
		"%s: enter: vdm_hdr=0x%x, vdos?=%d, num_vdos=%d\n",
		__func__, vdm_hdr, vdos != NULL, num_vdos);

	protocol_type = VDMH_PROTOCOL(vdm_hdr);
	parameter_type = VDMH_PARAMETER(vdm_hdr);

	if (protocol_type == VDM_RESPONSE) {
		acked = VDMH_ACK(vdm_hdr);
		if (!acked) {
			dev_warn(pd->dev, "NACK(ack=%d)", acked);
		} else if (parameter_type == EXT_BATT_FW_HMD_MOUNTED) {
			dev_info(pd->dev,
				"Received mount status ack response code 0x%x",
				vdos[0]);

			pd->last_mount_ack = VDO_ACK_STATUS(vdos[0]);
		} else if (parameter_type == EXT_BATT_FW_HMD_DOCKED) {
			dev_info(pd->dev,
				"Received dock status ack response code 0x%x", vdos[0]);
			/* Recall the lie we told Lehua when sending the dock VDM */
			power_role.intval = VDO_ACK_STATUS(vdos[0]);

			dev_info(pd->dev, "Sending PR_SWAP to role=%d", power_role.intval);
			rc = power_supply_set_property(pd->usb_psy,
					POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &power_role);
			/* the default timeout is too short for PR_SWAP, ignore it */
			if (rc != -ETIMEDOUT)
				dev_err(pd->dev, "Failed sending PR_SWAP: rc=%d", rc);
		} else {
			dev_warn(pd->dev, "Unsupported response parameter 0x%x",
					parameter_type);
		}
		return;
	}

	if (protocol_type != VDM_BROADCAST) {
		dev_warn(pd->dev, "Usupported Protocol=%d\n", protocol_type);
		return;
	}

	if (num_vdos == 0) {
		dev_warn(pd->dev, "Empty VDO packets vdm_hdr=0x%x\n", vdm_hdr);
		return;
	}

	sb = VDMH_SIZE(vdm_hdr);
	if (sb >= ARRAY_SIZE(vdm_size_bytes)) {
		dev_warn(pd->dev, "Invalid size byte code, sb=%d", sb);
		return;
	}

	high_bytes = VDMH_HIGH(vdm_hdr);
	switch (parameter_type) {
	case EXT_BATT_FW_VERSION_NUMBER:
		pd->params.fw_version = vdos[0];
		break;
	case EXT_BATT_FW_SERIAL:
		memcpy(pd->params.serial,
			vdos, sizeof(pd->params.serial));
		break;
	case EXT_BATT_FW_SERIAL_BATTERY:
		memcpy(pd->params.serial_battery,
			vdos, sizeof(pd->params.serial_battery));
		break;
	case EXT_BATT_FW_TEMP_FG:
		pd->params.temp_fg = vdos[0];
		break;
	case EXT_BATT_FW_VOLTAGE:
		pd->params.voltage = vdos[0];
		break;
	case EXT_BATT_FW_BATT_STATUS:
		convert_battery_status(pd, vdos[0]);
		break;
	case EXT_BATT_FW_CURRENT:
		pd->params.icurrent = vdos[0]; /* negative range */
		break;
	case EXT_BATT_FW_REMAINING_CAPACITY:
		pd->params.remaining_capacity = vdos[0];
		break;
	case EXT_BATT_FW_FCC:
		pd->params.fcc = vdos[0];
		break;
	case EXT_BATT_FW_CYCLE_COUNT:
		pd->params.cycle_count = vdos[0];
		break;
	case EXT_BATT_FW_RSOC:
		pd->params.rsoc = vdos[0];
		break;
	case EXT_BATT_FW_SOH:
		pd->params.soh = vdos[0];
		break;
	case EXT_BATT_FW_DEVICE_NAME:
		if (pd->params.device_name[0] == '\0' &&
			vdm_size_bytes[sb] <=
				sizeof(u32) * num_vdos &&
			vdm_size_bytes[sb] <=
				sizeof(pd->params.device_name)) {
			memcpy(pd->params.device_name,
				vdos, vdm_size_bytes[sb]);
		}
		break;
	case EXT_BATT_FW_LDB1:
		memcpy(pd->params.lifetime1_lower, vdos,
			sizeof(pd->params.lifetime1_lower));
		memcpy(pd->params.lifetime1_higher,
			vdos + LIFETIME_1_LOWER_LEN,
			sizeof(pd->params.lifetime1_higher));
		break;
	case EXT_BATT_FW_LDB3:
		pd->params.lifetime3 = vdos[0];
		break;
	case EXT_BATT_FW_LDB4:
		memcpy(pd->params.lifetime4,
			vdos, sizeof(pd->params.lifetime4));
		break;
	case EXT_BATT_FW_LDB6:
		if (!high_bytes)
			memcpy(pd->params.temp_zones[0],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(pd->params.temp_zones[0] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB7:
		if (!high_bytes)
			memcpy(pd->params.temp_zones[1],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(pd->params.temp_zones[1] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB8:
		if (!high_bytes)
			memcpy(pd->params.temp_zones[2],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(pd->params.temp_zones[2] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB9:
		if (!high_bytes)
			memcpy(pd->params.temp_zones[3],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(pd->params.temp_zones[3] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB10:
		if (!high_bytes)
			memcpy(pd->params.temp_zones[4],
					vdos, VDOS_MAX_BYTES);
		else
			memcpy(pd->params.temp_zones[4] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB11:
		if (!high_bytes)
			memcpy(pd->params.temp_zones[5],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(pd->params.temp_zones[5] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB12:
		if (!high_bytes)
			memcpy(pd->params.temp_zones[6],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(pd->params.temp_zones[6] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_MANUFACTURER_INFO1:
		pd->params.manufacturer_info_a = vdos[0];
		break;
	case EXT_BATT_FW_MANUFACTURER_INFO2:
		pd->params.manufacturer_info_b = vdos[0];
		break;
	case EXT_BATT_FW_MANUFACTURER_INFO3:
		pd->params.manufacturer_info_c = vdos[0];
		break;
	case EXT_BATT_FW_CHARGER_PLUGGED:
		pd->params.charger_plugged = vdos[0];
		/* Force re-evaluation on charger status update */
		ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
				pd->battery_psy);
		break;
	}
}

static ssize_t charger_plugged_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pd->params.charger_plugged);
}

static ssize_t charger_plugged_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for charger_plugged: %s", buf);
		return result;
	}

	pd->params.charger_plugged = temp;

	/* Force re-evaluation */
	ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
			pd->battery_psy);
	return count;
}
static DEVICE_ATTR_RW(charger_plugged);

static ssize_t connected_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pd->connected);
}

static ssize_t connected_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for connected: %s", buf);
		return result;
	}

	pd->connected = temp;

	return count;
}
static DEVICE_ATTR_RW(connected);

static ssize_t mount_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->mount_state);
}

static ssize_t mount_state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int result;
	u32 temp;

	result = kstrtou32(buf, 10, &temp);
	if (result < 0 || temp > EXT_BATT_FW_UNKNOWN) {
		dev_err(dev, "Illegal input for mount_state: %s", buf);
		return result;
	}

	result = mutex_lock_interruptible(&pd->lock);
	if (result != 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, result);
		return result;
	}

	pd->mount_state = temp;

	if (pd->connected) {
		cancel_delayed_work(&pd->mount_state_work);
		schedule_delayed_work(&pd->mount_state_work, 0);
	}
	mutex_unlock(&pd->lock);

	/* Force re-evaluation */
	ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
			pd->battery_psy);
	return count;
}
static DEVICE_ATTR_RW(mount_state);

static ssize_t rsoc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.rsoc);
}

static ssize_t rsoc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int result;
	u8 temp;

	result = kstrtou8(buf, 10, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input forrsoc: %s", buf);
		return result;
	}

	pd->params.rsoc = temp;

	return count;
}
static DEVICE_ATTR_RW(rsoc);

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int result;

	result = mutex_lock_interruptible(&pd->lock);
	if (result != 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, result);
		return result;
	}

	result = scnprintf(buf, PAGE_SIZE, "%s\n", pd->params.battery_status);
	mutex_unlock(&pd->lock);

	return result;
}

static ssize_t status_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int result;

	result = mutex_lock_interruptible(&pd->lock);
	if (result != 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, result);
		return result;
	}

	snprintf(pd->params.battery_status, sizeof(pd->params.battery_status), "%s", buf);
	mutex_unlock(&pd->lock);

	return count;
}
static DEVICE_ATTR_RW(status);

static ssize_t charging_suspend_disable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pd->charging_suspend_disable);
}

static ssize_t charging_suspend_disable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for charging suspend disable: %s", buf);
		return result;
	}

	pd->charging_suspend_disable = temp;

	/* Force re-evaluation */
	ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
			pd->battery_psy);

	return count;
}
static DEVICE_ATTR_RW(charging_suspend_disable);

static ssize_t serial_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", pd->params.serial);
}
static DEVICE_ATTR_RO(serial);

static ssize_t serial_battery_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", pd->params.serial_battery);
}
static DEVICE_ATTR_RO(serial_battery);

static ssize_t temp_fg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_fg);
}
static DEVICE_ATTR_RO(temp_fg);

static ssize_t voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.voltage);
}
static DEVICE_ATTR_RO(voltage);

static ssize_t icurrent_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.icurrent);
}
static DEVICE_ATTR_RO(icurrent);

static ssize_t remaining_capacity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.remaining_capacity);
}
static DEVICE_ATTR_RO(remaining_capacity);

static ssize_t fcc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.fcc);
}
static DEVICE_ATTR_RO(fcc);

static ssize_t cycle_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.cycle_count);
}
static DEVICE_ATTR_RO(cycle_count);

static ssize_t soh_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.soh);
}
static DEVICE_ATTR_RO(soh);

static ssize_t fw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.fw_version);
}
static DEVICE_ATTR_RO(fw_version);

static ssize_t device_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", pd->params.device_name);
}
static DEVICE_ATTR_RO(device_name);

static ssize_t cell_1_max_voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime1_lower[0]);
}
static DEVICE_ATTR_RO(cell_1_max_voltage);

static ssize_t cell_1_min_voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime1_lower[1]);
}
static DEVICE_ATTR_RO(cell_1_min_voltage);

static ssize_t max_charge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_lower[2]);
}
static DEVICE_ATTR_RO(max_charge_current);

static ssize_t max_discharge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_lower[3]);
}
static DEVICE_ATTR_RO(max_discharge_current);

static ssize_t max_avg_discharge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_lower[4]);
}
static DEVICE_ATTR_RO(max_avg_discharge_current);

static ssize_t max_avg_dsg_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_lower[5]);
}
static DEVICE_ATTR_RO(max_avg_dsg_power);

static ssize_t max_temp_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_higher[0]);
}
static DEVICE_ATTR_RO(max_temp_cell);

static ssize_t min_temp_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_higher[1]);
}
static DEVICE_ATTR_RO(min_temp_cell);

static ssize_t max_temp_int_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_higher[2]);
}
static DEVICE_ATTR_RO(max_temp_int_sensor);

static ssize_t min_temp_int_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", pd->params.lifetime1_higher[3]);
}
static DEVICE_ATTR_RO(min_temp_int_sensor);

static ssize_t total_fw_runtime_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime3);
}
static DEVICE_ATTR_RO(total_fw_runtime);

static ssize_t num_valid_charge_terminations_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime4[0]);
}
static DEVICE_ATTR_RO(num_valid_charge_terminations);

static ssize_t last_valid_charge_term_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime4[1]);
}
static DEVICE_ATTR_RO(last_valid_charge_term);

static ssize_t num_qmax_updates_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime4[2]);
}
static DEVICE_ATTR_RO(num_qmax_updates);

static ssize_t last_qmax_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime4[3]);
}
static DEVICE_ATTR_RO(last_qmax_update);

static ssize_t num_ra_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime4[4]);
}
static DEVICE_ATTR_RO(num_ra_update);

static ssize_t last_ra_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime4[5]);
}
static DEVICE_ATTR_RO(last_ra_update);

// ut

static ssize_t t_ut_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][0]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_a);

static ssize_t t_ut_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][1]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_b);

static ssize_t t_ut_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][2]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_c);

static ssize_t t_ut_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][3]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_d);

static ssize_t t_ut_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][4]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_e);

static ssize_t t_ut_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][5]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_f);

static ssize_t t_ut_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][6]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_g);

static ssize_t t_ut_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[0][7]);
}
static DEVICE_ATTR_RO(t_ut_rsoc_h);

// lt

static ssize_t t_lt_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][0]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_a);

static ssize_t t_lt_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][1]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_b);

static ssize_t t_lt_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][2]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_c);

static ssize_t t_lt_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][3]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_d);

static ssize_t t_lt_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][4]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_e);

static ssize_t t_lt_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][5]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_f);

static ssize_t t_lt_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][6]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_g);

static ssize_t t_lt_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[1][7]);
}
static DEVICE_ATTR_RO(t_lt_rsoc_h);

// stl

static ssize_t t_stl_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][0]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_a);

static ssize_t t_stl_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][1]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_b);

static ssize_t t_stl_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][2]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_c);

static ssize_t t_stl_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][3]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_d);

static ssize_t t_stl_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][4]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_e);

static ssize_t t_stl_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][5]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_f);

static ssize_t t_stl_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][6]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_g);

static ssize_t t_stl_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[2][7]);
}
static DEVICE_ATTR_RO(t_stl_rsoc_h);

// rt

static ssize_t t_rt_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][0]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_a);

static ssize_t t_rt_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][1]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_b);

static ssize_t t_rt_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][2]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_c);

static ssize_t t_rt_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][3]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_d);

static ssize_t t_rt_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][4]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_e);

static ssize_t t_rt_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][5]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_f);

static ssize_t t_rt_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][6]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_g);

static ssize_t t_rt_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[3][7]);
}
static DEVICE_ATTR_RO(t_rt_rsoc_h);

// sth

static ssize_t t_sth_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][0]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_a);

static ssize_t t_sth_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][1]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_b);

static ssize_t t_sth_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][2]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_c);

static ssize_t t_sth_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][3]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_d);

static ssize_t t_sth_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][4]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_e);

static ssize_t t_sth_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][5]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_f);

static ssize_t t_sth_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][6]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_g);

static ssize_t t_sth_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[4][7]);
}
static DEVICE_ATTR_RO(t_sth_rsoc_h);

// ht

static ssize_t t_ht_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][0]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_a);

static ssize_t t_ht_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][1]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_b);

static ssize_t t_ht_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][2]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_c);

static ssize_t t_ht_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][3]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_d);

static ssize_t t_ht_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][4]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_e);

static ssize_t t_ht_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][5]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_f);

static ssize_t t_ht_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][6]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_g);

static ssize_t t_ht_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[5][7]);
}
static DEVICE_ATTR_RO(t_ht_rsoc_h);

// ot

static ssize_t t_ot_rsoc_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][0]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_a);

static ssize_t t_ot_rsoc_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][1]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_b);

static ssize_t t_ot_rsoc_c_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][2]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_c);

static ssize_t t_ot_rsoc_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][3]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_d);

static ssize_t t_ot_rsoc_e_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][4]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_e);

static ssize_t t_ot_rsoc_f_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][5]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_f);

static ssize_t t_ot_rsoc_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][6]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_g);

static ssize_t t_ot_rsoc_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.temp_zones[6][7]);
}
static DEVICE_ATTR_RO(t_ot_rsoc_h);

static struct attribute *ext_batt_attrs[] = {
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
	&dev_attr_fw_version.attr,
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
ATTRIBUTE_GROUPS(ext_batt);

static void ext_batt_create_sysfs(struct ext_batt_pd *pd)
{
	int result;

	/* Mount state node */
	result = sysfs_create_groups(&pd->dev->kobj, ext_batt_groups);
	if (result != 0)
		dev_err(pd->dev, "Error creating sysfs entries: %d\n", result);
}

static void ext_batt_default_sysfs_values(struct ext_batt_pd *pd)
{
	strcpy(pd->params.battery_status, battery_status_text[UNKNOWN]);
}

static void ext_batt_mount_status_work(struct work_struct *work)
{
	struct ext_batt_pd *pd =
		container_of(work, struct ext_batt_pd, mount_state_work.work);
	int result;
	u32 mount_state_header = 0;
	u32 mount_state_vdo = pd->mount_state;

	dev_dbg(pd->dev, "%s: enter", __func__);

	mutex_lock(&pd->lock);
	if (!pd->connected) {
		mutex_unlock(&pd->lock);
		return;
	}

	/* Send HMD Mount request if needed */
	if (pd->mount_state != pd->last_mount_ack) {
		mount_state_header =
			VDMH_CONSTRUCT(
				pd->svid,
				0, 1, 0, 1,
				EXT_BATT_FW_HMD_MOUNTED);

		if (pd->mount_state == 0)
			mount_state_vdo |= (EXT_BATT_FW_VDM_PERIOD_OFF_HEAD << 8);
		else
			mount_state_vdo |= (EXT_BATT_FW_VDM_PERIOD_ON_HEAD << 8);

		result = external_battery_send_vdm(pd, mount_state_header,
			&mount_state_vdo, 1);
		if (result != 0)
			dev_err(pd->dev,
				"Error sending HMD mount request: %d", result);
		else
			dev_dbg(pd->dev,
				"Sent HMD mount request: header=0x%x mount_state_vdo=0x%x",
				mount_state_header, mount_state_vdo);
	}

	/*
	 * Schedule periodically to ensure that ext_batt ACKs the
	 * current mount state.
	 */
	schedule_delayed_work(&pd->mount_state_work, MOUNT_WORK_DELAY_SECONDS * HZ);

	mutex_unlock(&pd->lock);
}

static void ext_batt_dock_state_work(struct work_struct *work)
{
	struct ext_batt_pd *pd =
		container_of(work, struct ext_batt_pd, dock_state_dwork.work);
	int rc;
	int new_power_role;
	u32 dock_state_hdr, dock_state_vdo;

	dev_dbg(pd->dev, "%s: enter", __func__);

	mutex_lock(&pd->lock);
	if (!pd->connected || !pd->cypd_pd_active_chan)
		goto out;

	rc = iio_read_channel_processed(pd->cypd_pd_active_chan, &pd->dock_state);
	if (rc < 0) {
		dev_err(pd->dev, "couldn't read cypd pd_active: rc=%d\n", rc);
		goto out;
	}

	new_power_role = (pd->dock_state && pd->hmd_soc >= PR_SWAP_SOC_THRESHOLD);

	dock_state_hdr =
		VDMH_CONSTRUCT(
			pd->svid,
			0, VDM_REQUEST, 0, 1,
			EXT_BATT_FW_HMD_DOCKED);
	/*
	 *	Greedy Lehua expects to be in SNK mode whenever the HMD is docked, but
	 *	we want to prioritize charging the internal battery first. Tell a white
	 *	lie and claim we're docked only when we're ready to supply power to it.
	 */
	dock_state_vdo = new_power_role;

	dev_dbg(pd->dev, "sending dock VDM: dock_state=%d", dock_state_vdo);
	rc = external_battery_send_vdm(pd, dock_state_hdr, &dock_state_vdo, 1);
	if (rc < 0)
		dev_err(pd->dev, "error sending dock VDM: rc=%d", rc);

out:
	mutex_unlock(&pd->lock);
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

static void set_usb_charging_state(struct ext_batt_pd *pd,
		enum usb_charging_state state)
{
	union power_supply_propval val = {0};
	int result = 0;

	if (IS_ERR_OR_NULL(pd->usb_psy)) {
		dev_err(pd->dev, "USB power supply handle is invalid.\n");
		return;
	}

	result = power_supply_get_property(pd->usb_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
	if (result) {
		dev_err(pd->dev, "Unable to read charging state: %d\n", result);
		return;
	}

	if (state == val.intval) {
		dev_dbg(pd->dev, "Current USB charging state is %s. No action needed.\n",
				usb_charging_state(state));
		return;
	}

	val.intval = state;

	result = power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
	if (result) {
		dev_err(pd->dev, "Unable to update USB charging state: %d\n", result);
		return;
	}

	dev_dbg(pd->dev,
			"USB charging state updated successfully. New state: %s\n",
			usb_charging_state(state));
}

static void handle_battery_capacity_change(struct ext_batt_pd *pd,
		u32 capacity)
{
	bool crossed_soc_threshold;

	dev_dbg(pd->dev,
			"Battery capacity change event. Capacity = %d, connected = %d, mount state = %d, charger plugged = %d\n",
			capacity, pd->connected, pd->mount_state,
			pd->params.charger_plugged);

	/* May need to swap power roles if the HMD battery ticks above or below */
	crossed_soc_threshold =
		(pd->hmd_soc < PR_SWAP_SOC_THRESHOLD && capacity >= PR_SWAP_SOC_THRESHOLD) ||
		(pd->hmd_soc >= PR_SWAP_SOC_THRESHOLD && capacity < PR_SWAP_SOC_THRESHOLD);
	pd->hmd_soc = capacity;

	if (crossed_soc_threshold) {
		cancel_delayed_work_sync(&pd->dock_state_dwork);
		schedule_delayed_work(&pd->dock_state_dwork, 0);
	}

	if ((pd->charging_suspend_disable) ||
		(!pd->connected) ||
		(pd->mount_state != 0) ||
		(pd->params.charger_plugged) ||
		(capacity < pd->charging_resume_threshold)) {
		/* Attempt to resume charging if suspended previously */
		set_usb_charging_state(pd, CHARGING_RESUME);
		return;
	}

	if (capacity >= pd->charging_suspend_threshold) {
		/* Attempt to suspend charging if not suspended previously */
		set_usb_charging_state(pd, CHARGING_SUSPEND);
	}
}

static void ext_batt_psy_notifier_work(struct work_struct *work)
{
	int result;
	union power_supply_propval val;
	struct ext_batt_pd *pd = container_of(work, struct ext_batt_pd, psy_notifier_work);

	mutex_lock(&pd->lock);
	if (!pd->connected) {
		mutex_unlock(&pd->lock);
		return;
	}
	mutex_unlock(&pd->lock);

	result = power_supply_get_property(pd->battery_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
	if (result) {
		/*
		 * Log failure and return okay for this call with the hope
		 * that we can read battery capacity next time around.
		 */
		dev_err(pd->dev, "Unable to read battery capacity: %d\n", result);
		return;
	}

	handle_battery_capacity_change(pd, val.intval);
}

static int ext_batt_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr)
{
	struct power_supply *psy = ptr;
	struct ext_batt_pd *pd = NULL;

	if (IS_ERR_OR_NULL(nb))
		return NOTIFY_BAD;

	pd = container_of(nb, struct ext_batt_pd, nb);
	if (IS_ERR_OR_NULL(pd) || IS_ERR_OR_NULL(pd->battery_psy)) {
		dev_err(pd->dev, "PSY notifier provided object handle is invalid\n");
		return NOTIFY_BAD;
	}

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (psy == pd->battery_psy)
		schedule_work(&pd->psy_notifier_work);

	if (pd->cypd_psy && psy == pd->cypd_psy) {
		cancel_delayed_work(&pd->dock_state_dwork);
		schedule_delayed_work(&pd->dock_state_dwork, 0);
	}

	return NOTIFY_OK;
}

static void ext_batt_psy_register_notifier(struct ext_batt_pd *pd)
{
	int result = 0;

	if (pd->nb.notifier_call == NULL) {
		dev_err(pd->dev, "Notifier block's notifier call is NULL\n");
		return;
	}

	result = power_supply_reg_notifier(&pd->nb);
	if (result != 0) {
		dev_err(pd->dev, "Failed to register power supply notifier, result:%d\n",
				result);
	}
}

static void ext_batt_psy_unregister_notifier(struct ext_batt_pd *pd)
{
	/*
	 * Safe to call unreg notifier even if notifier block wasn't registered
	 * with the kernel due to some prior failure.
	 */
	power_supply_unreg_notifier(&pd->nb);
}

static int ext_batt_psy_init(struct ext_batt_pd *pd)
{
	pd->nb.notifier_call = NULL;
	pd->battery_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(pd->battery_psy)) {
		dev_err(pd->dev, "Failed to get battery power supply\n");
		return -EPROBE_DEFER;
	}

	pd->usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(pd->usb_psy)) {
		dev_err(pd->dev, "Failed to get USB power supply\n");
		return -EPROBE_DEFER;
	}

	#ifdef CONFIG_CHARGER_CYPD3177
	pd->cypd_psy = power_supply_get_by_name("cypd3177");
	if (!pd->cypd_psy) {
		dev_err(pd->dev, "Failed to get CYPD psy");
		return -EPROBE_DEFER;
	}
	#endif

	pd->nb.notifier_call = ext_batt_psy_notifier_call;
	ext_batt_psy_register_notifier(pd);
	return 0;
}

static int ext_batt_probe(struct platform_device *pdev)
{
	int result = 0;
	struct ext_batt_pd *pd;

	dev_dbg(&pdev->dev, "%s: enter", __func__);

	pd = devm_kzalloc(
			&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	result = of_property_read_u16(pdev->dev.of_node,
		"svid", &pd->svid);
	if (result < 0) {
		pd->svid = DEFAULT_EXT_BATT_VID;
		dev_err(&pdev->dev,
			"svid unavailable, defaulting to svid=%x, result:%d\n",
			DEFAULT_EXT_BATT_VID, result);
	}
	dev_dbg(&pdev->dev, "Property svid=%x", pd->svid);

	result = of_property_read_u32(pdev->dev.of_node,
		"charging-suspend-thresh", &pd->charging_suspend_threshold);
	if (result < 0) {
		dev_err(&pdev->dev,
			"Charging suspend threshold unavailable, failing probe, result:%d\n",
				result);
		return result;
	}
	dev_dbg(&pdev->dev, "Charging suspend threshold=%d\n",
		pd->charging_suspend_threshold);

	result = of_property_read_u32(pdev->dev.of_node,
		"charging-resume-thresh", &pd->charging_resume_threshold);
	if (result < 0) {
		dev_err(&pdev->dev,
			"Charging resume threshold unavailable, failing probe, result:%d\n",
				result);
		return result;
	}
	dev_dbg(&pdev->dev, "Charging resume threshold=%d\n",
		pd->charging_resume_threshold);

	if (pd->charging_resume_threshold >= pd->charging_suspend_threshold) {
		dev_err(&pdev->dev, "Invalid charging thresholds set, failing probe\n");
		return -EINVAL;
	}

	mutex_init(&pd->lock);

	pd->last_mount_ack = EXT_BATT_FW_UNKNOWN;
	INIT_DELAYED_WORK(&pd->mount_state_work, ext_batt_mount_status_work);

	#ifdef CONFIG_CHARGER_CYPD3177
	pd->cypd_pd_active_chan = iio_channel_get(NULL, "cypd_pd_active");
	if (IS_ERR(pd->cypd_pd_active_chan)) {
		if (PTR_ERR(pd->cypd_pd_active_chan) != -EPROBE_DEFER)
			dev_err_ratelimited(pd->dev, "cypd pd_active channel unavailable");
		return -EPROBE_DEFER;
	}
	#endif
	INIT_DELAYED_WORK(&pd->dock_state_dwork, ext_batt_dock_state_work);

	pd->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, pd);

	/* Register VDM callbacks */
	result = external_battery_register_svid_handler(pd);
	if (result == -EPROBE_DEFER)
		return result;

	if (result != 0) {
		dev_err(&pdev->dev, "Failed to register vdm handler");
		return result;
	}
	dev_dbg(&pdev->dev, "Registered svid 0x%04x",
		pd->svid);

	/* Query power supply and register for events */
	INIT_WORK(&pd->psy_notifier_work, ext_batt_psy_notifier_work);
	result = ext_batt_psy_init(pd);
	if (result == -EPROBE_DEFER)
		return result;

	/* Create nodes here. */
	ext_batt_create_sysfs(pd);
	ext_batt_default_sysfs_values(pd);

	return result;
}

static int ext_batt_remove(struct platform_device *pdev)
{
	struct ext_batt_pd *pd = platform_get_drvdata(pdev);

	dev_dbg(pd->dev, "%s: enter", __func__);

	external_battery_unregister_svid_handler(pd);

	mutex_lock(&pd->lock);
	pd->connected = false;
	mutex_unlock(&pd->lock);
	cancel_delayed_work_sync(&pd->mount_state_work);
	cancel_delayed_work_sync(&pd->dock_state_dwork);
	set_usb_charging_state(pd, CHARGING_RESUME);
	ext_batt_psy_unregister_notifier(pd);
	if (pd->cypd_psy) {
		power_supply_put(pd->cypd_psy);
		iio_channel_release(pd->cypd_pd_active_chan);
	}
	mutex_destroy(&pd->lock);

	sysfs_remove_groups(&pd->dev->kobj, ext_batt_groups);

	return 0;
}

/* Driver Info */
static const struct of_device_id ext_batt_match_table[] = {
		{ .compatible = "oculus,external-battery", },
		{ },
};

static struct platform_driver ext_batt_driver = {
	.driver = {
		.name = "oculus,external-battery",
		.of_match_table = ext_batt_match_table,
		.owner = THIS_MODULE,
	},
	.probe = ext_batt_probe,
	.remove = ext_batt_remove,
};

static int __init ext_batt_init(void)
{
	platform_driver_register(&ext_batt_driver);
	return 0;
}

static void __exit ext_batt_exit(void)
{
	platform_driver_unregister(&ext_batt_driver);
}

module_init(ext_batt_init);
module_exit(ext_batt_exit);

MODULE_DESCRIPTION("External battery driver");
MODULE_LICENSE("GPL");
