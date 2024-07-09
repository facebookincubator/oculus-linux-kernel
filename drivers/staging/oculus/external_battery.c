// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "external_battery.h"
#include "usbvdm.h"

#define DEFAULT_EXT_BATT_VID 0x2833
#define MOUNT_WORK_DELAY_SECONDS 30

#define DEFAULT_SRC_ENABLE_SOC_THRESHOLD 95
#define DEFAULT_SRC_CURRENT_LIMIT_MAX_UA 1500000

#define STATUS_FULLY_DISCHARGED BIT(4)
#define STATUS_FULLY_CHARGED BIT(5)
#define STATUS_DISCHARGING BIT(6)

/*
 * Period in minutes external battery firmware will use to send VDM traffic for
 * ON/OFF head mount status. 0 corresponds to the minimum period of 1 minute.
 * This parameter may be set in increments of 1 minute.
 */
#define EXT_BATT_FW_VDM_PERIOD_ON_HEAD 0
#define EXT_BATT_FW_VDM_PERIOD_OFF_HEAD 14

/* Factors to convert 2h/h time units to seconds */
#define HR_TO_SECONDS			3600
#define TWO_HRS_TO_SECONDS		7200

/* Wait time before considering a mount state VDM unreceived */
#define MOUNT_STATE_ACK_TIMEOUT_MS 2000

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

static void ext_batt_pr_swap(struct work_struct *work);
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
		strncpy(pd->params.battery_status, battery_status_text[NOT_CHARGING], sizeof(pd->params.battery_status));
	else if ((status & STATUS_FULLY_CHARGED) || !(status & STATUS_DISCHARGING))
		strncpy(pd->params.battery_status, battery_status_text[CHARGING], sizeof(pd->params.battery_status));
	else
		strncpy(pd->params.battery_status, battery_status_text[UNKNOWN], sizeof(pd->params.battery_status));
}

static void ext_batt_reset(struct ext_batt_pd *pd)
{
	mutex_lock(&pd->lock);
	pd->connected = false;
	pd->first_broadcast_data_received = false;
	pd->last_dock_ack = EXT_BATT_FW_DOCK_STATE_UNKNOWN;
	pd->params.charger_plugged = 0;
	pd->params.cycle_count = 0;
	pd->params.fcc = 0;
	pd->params.fw_version = 0;
	pd->params.icurrent = 0;
	pd->params.remaining_capacity = 0;
	pd->params.rsoc = 0;
	pd->params.soh = 0;
	pd->params.temp_fg = 0;
	pd->params.voltage = 0;
	pd->params.batt_status = 0x0000;

	snprintf(pd->params.battery_status, sizeof(pd->params.battery_status), "%s", battery_status_text[UNKNOWN]);
	snprintf(pd->params.device_name, sizeof(pd->params.device_name), "");
	snprintf(pd->params.pack_assembly_pn, sizeof(pd->params.pack_assembly_pn), "");
	snprintf(pd->params.serial_battery, sizeof(pd->params.serial_battery), "");

	/* Set default values for lifetime data blocks common across lehua and molokini*/
	snprintf(pd->params.manufacturer_info_a.data, sizeof(pd->params.manufacturer_info_a.data), "");
	snprintf(pd->params.manufacturer_info_b.data, sizeof(pd->params.manufacturer_info_b.data), "");
	snprintf(pd->params.manufacturer_info_c.data, sizeof(pd->params.manufacturer_info_c.data), "");
	memset(&pd->params.lifetime1.values, 0, sizeof(pd->params.lifetime1.values));
	memset(pd->params.error_conditions, 0, sizeof(pd->params.error_conditions));

	/* Set default values for temp zones based on the batt_id*/
	switch (pd->batt_id) {
	case EXT_BATT_ID_LEHUA:
		memset(pd->params.temp_zones.tz_lehua, 0, sizeof(pd->params.temp_zones.tz_lehua));
		memset(&pd->params.lifetime2.values, 0, sizeof(pd->params.lifetime2.values));
		memset(&pd->params.lifetime7.values, 0, sizeof(pd->params.lifetime7.values));
		memset(&pd->params.lifetime8.values, 0, sizeof(pd->params.lifetime8.values));
		break;

	case EXT_BATT_ID_MOLOKINI:
		memset(pd->params.temp_zones.tz_molokini, 0, sizeof(pd->params.temp_zones.tz_molokini));
		memset(&pd->params.lifetime4.values, 0, sizeof(pd->params.lifetime4.values));
		break;

	default:
		break;
	}
	mutex_unlock(&pd->lock);
}

int external_battery_send_vdm(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	return usbvdm_subscriber_vdm(pd->sub, vdm_hdr, vdos, num_vdos);
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
	pd->first_broadcast_data_received = false;
	pd->last_dock_ack = EXT_BATT_FW_DOCK_STATE_UNKNOWN;
	pd->params.rsoc = 0;
	pd->params.rsoc_test_enabled = false;
	mutex_unlock(&pd->lock);

	schedule_work(&pd->mount_state_work);
	schedule_work(&pd->dock_state_work);

	/* Force re-evaluation */
	ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
			pd->battery_psy);
}

void ext_batt_vdm_disconnect(struct ext_batt_pd *pd)
{
	dev_dbg(pd->dev, "%s: enter", __func__);

	/*
	 * Client notifications only occur during connect/disconnect
	 * state transitions. This should not be called when no external
	 * battery is connected.
	 */
	if (!pd->connected)
		return;

	ext_batt_reset(pd);

	cancel_work_sync(&pd->mount_state_work);
	cancel_work_sync(&pd->dock_state_work);
	flush_workqueue(pd->wq);

	/* Re-enable USB input charging path if disabled */
	set_usb_charging_state(pd, CHARGING_RESUME);
}

static void vdm_memcpy(struct ext_batt_pd *pd,
		u32 param, int num_vdos,
		void *dest, const void *src, size_t count)
{
	if (count > num_vdos * sizeof(u32)) {
		dev_warn(pd->dev,
				"Received VDM (0x%02x) of insufficent length: %lu",
				param, count);
		return;
	}

	memcpy(dest, src, count);
}

static void process_lifetime_data_blocks_molokini(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	u32 parameter_type = VDMH_PARAMETER(vdm_hdr);
	u32 high_bytes = VDMH_HIGH(vdm_hdr);

	switch (parameter_type) {
	case EXT_BATT_FW_LDB1:
		vdm_memcpy(pd, parameter_type, num_vdos,
			pd->params.lifetime1.values.lower, vdos,
			sizeof(pd->params.lifetime1.values.lower));
		break;
	case EXT_BATT_FW_LDB3:
		pd->params.lifetime3 = vdos[0];
		break;
	case EXT_BATT_FW_LDB4:
		vdm_memcpy(pd, parameter_type, num_vdos,
			pd->params.lifetime4.values.lower, vdos,
			sizeof(pd->params.lifetime4.values.lower));
		break;
	case EXT_BATT_FW_LDB6:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[0],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[0] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB7:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[1],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[1] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB8:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[2],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[2] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB9:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[3],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[3] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB10:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
					pd->params.temp_zones.tz_molokini[4],
					vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[4] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB11:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[5],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[5] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB12:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[6],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_molokini[6] +
				VDOS_MAX_BYTES / sizeof(u32),
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_MANUFACTURER_INFO1:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_a.values.lower, vdos,
				sizeof(pd->params.manufacturer_info_a.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_a.values.higher, vdos,
				sizeof(pd->params.manufacturer_info_a.values.higher));
		break;
	case EXT_BATT_FW_MANUFACTURER_INFO2:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_b.values.lower, vdos,
				sizeof(pd->params.manufacturer_info_b.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_b.values.higher, vdos,
				sizeof(pd->params.manufacturer_info_b.values.higher));
		break;
	case EXT_BATT_FW_MANUFACTURER_INFO3:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_c.values.lower, vdos,
				sizeof(pd->params.manufacturer_info_c.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_c.values.higher, vdos,
				sizeof(pd->params.manufacturer_info_c.values.higher));
		break;
	default:
		dev_err(pd->dev, "Unknown Molokini lifetime data block\n");
		break;
	}
}

static void process_lifetime_data_blocks_lehua(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	u32 parameter_type = VDMH_PARAMETER(vdm_hdr);
	u32 high_bytes = VDMH_HIGH(vdm_hdr);

	switch (parameter_type) {
	case EXT_BATT_FW_LDB1:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime1.values.lower, vdos,
				sizeof(pd->params.lifetime1.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime1.values.higher, vdos,
				sizeof(pd->params.lifetime1.values.higher));
		break;
	case EXT_BATT_FW_LDB2:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime2.values.lower, vdos,
				sizeof(pd->params.lifetime2.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime2.values.higher, vdos,
				sizeof(pd->params.lifetime2.values.higher));
		break;
	case EXT_BATT_FW_LDB3:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_lehua[0],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_lehua[1],
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB4:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_lehua[2],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_lehua[3],
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB5:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_lehua[4],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_lehua[5],
				vdos, VDOS_MAX_BYTES);
		break;
	case EXT_BATT_FW_LDB6:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.temp_zones.tz_lehua[6],
				vdos, VDOS_MAX_BYTES);
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime6.values, vdos,
				sizeof(pd->params.lifetime6.values));
		break;
	case EXT_BATT_FW_LDB7:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime7.values.lower, vdos,
				sizeof(pd->params.lifetime7.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime7.values.higher, vdos,
				sizeof(pd->params.lifetime7.values.higher));
		break;
	case EXT_BATT_FW_LDB8:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime8.values.lower, vdos,
				sizeof(pd->params.lifetime8.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.lifetime8.values.higher, vdos,
				sizeof(pd->params.lifetime8.values.higher));
		break;
	case EXT_BATT_FW_MANUFACTURER_INFOA:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_a.values.lower, vdos,
				sizeof(pd->params.manufacturer_info_a.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_a.values.higher, vdos,
				sizeof(pd->params.manufacturer_info_a.values.higher));
		break;
	default:
		dev_err(pd->dev, "Unknown Lehua lifetime data block: 0x%x\n",
			parameter_type);
		break;
	}
}

static void process_lifetime_data_blocks(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	if (pd->batt_id == EXT_BATT_ID_MOLOKINI)
		process_lifetime_data_blocks_molokini(pd, vdm_hdr, vdos, num_vdos);
	else if (pd->batt_id == EXT_BATT_ID_LEHUA)
		process_lifetime_data_blocks_lehua(pd, vdm_hdr, vdos, num_vdos);
}

void ext_batt_vdm_received(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	u32 protocol_type, parameter_type, sb, high_bytes, acked;
	int rc = 0;

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
			dev_dbg(pd->dev,
				"Received mount status ack response code 0x%x",
				vdos[0]);

			complete(&pd->mount_state_ack);
		} else if (parameter_type == EXT_BATT_FW_HMD_DOCKED) {
			struct ext_batt_pr_swap_work *prs_work;

			dev_dbg(pd->dev,
				"Received dock status ack response code 0x%x", vdos[0]);

			prs_work = kzalloc(sizeof(*prs_work), GFP_KERNEL);
			if (!prs_work) {
				dev_err(pd->dev, "unable to allow prs_work");
				return;
			}
			prs_work->vdo = vdos[0];
			prs_work->pd = pd;

			INIT_WORK(&prs_work->work, ext_batt_pr_swap);
			queue_work(pd->wq, &prs_work->work);
		} else {
			dev_warn(pd->dev, "Unsupported response parameter 0x%x",
					parameter_type);
		}
		return;
	} else if (protocol_type == VDM_REQUEST &&
				parameter_type >= EXT_BATT_FW_ERROR_UFP_LPD &&
				parameter_type <= EXT_BATT_FW_ERROR_DRP_OVP) {

		u32 error_state_header = 0;
		u32 error_state_vdo = vdos[0];
		int index = parameter_type - EXT_BATT_FW_ERROR_UFP_LPD;

		// svid, ack, proto, high, size, param
		error_state_header =
			VDMH_CONSTRUCT(
				VDM_SVID_META,
				1, VDM_RESPONSE, 0, 1,
				parameter_type);

		rc = external_battery_send_vdm(pd, error_state_header,
			&error_state_vdo, 1);

		if (rc != 0) {
			dev_err(pd->dev,
				"Error sending HMD response for error 0x%x rc=%d",
				parameter_type, rc);
		} else {
			/*
			 * track frequency of error conditions so that they could
			 * be exposed as SysFs and can be sampled appropriately
			 */
			dev_dbg(pd->dev, "Error condition index=%d\n", index);
			if (index >= 0 && index < EXT_BATT_FW_ERROR_CONDITIONS && error_state_vdo)
				pd->params.error_conditions[index]++;
		}
		return;
	}

	if (protocol_type != VDM_BROADCAST) {
		dev_warn(pd->dev, "Unsupported Protocol=%d\n", protocol_type);
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
		vdm_memcpy(pd, parameter_type, num_vdos,
			pd->params.serial,
			vdos, sizeof(pd->params.serial));
		break;
	case EXT_BATT_FW_SERIAL_BATTERY:
		vdm_memcpy(pd, parameter_type, num_vdos,
			pd->params.serial_battery,
			vdos, sizeof(pd->params.serial_battery));
		break;
	case EXT_BATT_FW_SERIAL_SYSTEM:
		vdm_memcpy(pd, parameter_type, num_vdos,
			pd->params.serial_system,
			vdos, sizeof(pd->params.serial_system));
		break;
	case EXT_BATT_FW_TEMP_FG:
		pd->params.temp_fg = vdos[0];
		break;
	case EXT_BATT_FW_VOLTAGE:
		pd->params.voltage = vdos[0];
		break;
	case EXT_BATT_FW_BATT_STATUS:
		pd->params.batt_status = vdos[0];
		convert_battery_status(pd, pd->params.batt_status);
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
	case EXT_BATT_FW_PACK_ASSEMBLY_PN:
		vdm_memcpy(pd, parameter_type, num_vdos,
			pd->params.pack_assembly_pn,
			vdos, sizeof(pd->params.pack_assembly_pn));
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
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.device_name,
				vdos, vdm_size_bytes[sb]);
		}
		break;
	case EXT_BATT_FW_LDB1 ... EXT_BATT_FW_MANUFACTURER_INFO3:
		process_lifetime_data_blocks(pd, vdm_hdr, vdos, num_vdos);
		/*
		 * This is a reasonable place to set first_broadcast_data_received
		 * as majority of fuel gauge broadcast data from the battery pack
		 * arrives as life time data blocks. So once we start receiving these
		 * life time data blocks, we can confidently assume that first
		 * batch of broadcast data has been received (after connection);
		 * so can switch back to default on/off head vdm period
		 */
		if (!pd->first_broadcast_data_received) {
			pd->first_broadcast_data_received = true;
			schedule_work(&pd->mount_state_work);
		}
		break;
	/*
	 * Handle manufacturer info B data block here since this is not
	 * defined within the LDB range (for Lehua)
	 */
	case EXT_BATT_FW_MANUFACTURER_INFOB:
		if (!high_bytes)
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_b.values.lower, vdos,
				sizeof(pd->params.manufacturer_info_b.values.lower));
		else
			vdm_memcpy(pd, parameter_type, num_vdos,
				pd->params.manufacturer_info_b.values.higher, vdos,
				sizeof(pd->params.manufacturer_info_b.values.higher));
		break;
	case EXT_BATT_FW_CHARGER_PLUGGED:
		pd->params.charger_plugged = vdos[0];
		/* Force re-evaluation on charger status update */
		power_supply_changed(pd->usb_psy);
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
	power_supply_changed(pd->usb_psy);
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

	if (pd->connected)
		schedule_work(&pd->mount_state_work);

	mutex_unlock(&pd->lock);

	/* Force re-evaluation */
	ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
			pd->battery_psy);
	return count;
}
static DEVICE_ATTR_RW(mount_state);

static u32 scale_rsoc(u32 rsoc, u32 min, u32 max) {
	if (rsoc < min)
		return 0;
	else if (rsoc > max)
		return 100;

	/* Multiply by 100 first to avoid int division issues */
	return (100 * (rsoc - min)) / (max - min);
}

static ssize_t rsoc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	u32 rsoc = pd->params.rsoc;

	if (pd->params.rsoc_test_enabled)
		rsoc = pd->params.rsoc_test;
	else if (pd->rsoc_scaling_enabled) {
		rsoc = scale_rsoc(rsoc,
			pd->rsoc_scaling_min_level,
			pd->rsoc_scaling_max_level);
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", rsoc);
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

	pd->params.rsoc_test_enabled = true;
	pd->params.rsoc_test = temp;

	return count;
}
static DEVICE_ATTR_RW(rsoc);

static ssize_t rsoc_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.rsoc);
}
static DEVICE_ATTR_RO(rsoc_raw);

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

static ssize_t reboot_into_bootloader_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ext_batt_pd *pd = dev_get_drvdata(dev);
	u32 vdm_hdr;
	bool send_command;
	int rc;

	if (kstrtobool(buf, &send_command))
		return -EINVAL;

	if (!send_command)
		return count;

	rc = mutex_lock_interruptible(&pd->lock);
	if (rc != 0) {
		dev_warn(dev, "%s: failed to grab lock, rc=%d", __func__, rc);
		return rc;
	}

	if (!pd->connected) {
		mutex_unlock(&pd->lock);
		return count;
	}

	vdm_hdr = VDMH_CONSTRUCT(VDM_SVID_META, 0, 1, 0, 1,
			EXT_BATT_FW_REBOOT_INTO_BOOTLOADER);
	rc = external_battery_send_vdm(pd, vdm_hdr, NULL, 0);
	if (rc) {
		dev_err(dev, "%s: failed to send vdm, rc=%d", __func__, rc);
		mutex_unlock(&pd->lock);
		return rc;
	}

	mutex_unlock(&pd->lock);

	return count;
}
static DEVICE_ATTR_WO(reboot_into_bootloader);

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

static ssize_t serial_system_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", pd->params.serial_system);
}
static DEVICE_ATTR_RO(serial_system);

static ssize_t pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%04x\n", pd->pid);
}
static DEVICE_ATTR_RO(pid);

static ssize_t vid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%04x\n", pd->vid);
}
static DEVICE_ATTR_RO(vid);

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

static ssize_t pack_assembly_pn_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", pd->params.pack_assembly_pn);
}
static DEVICE_ATTR_RO(pack_assembly_pn);

static ssize_t battery_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%04x\n", pd->params.batt_status);
}
static DEVICE_ATTR_RO(battery_status);

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

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		pd->params.lifetime1.ldb_molokini.cell_1_max_voltage);
}
static DEVICE_ATTR_RO(cell_1_max_voltage);

static ssize_t cell_1_min_voltage_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		pd->params.lifetime1.ldb_molokini.cell_1_min_voltage);
}
static DEVICE_ATTR_RO(cell_1_min_voltage);

static ssize_t max_charge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_lehua.max_charge_current);
	else
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_molokini.max_charge_current);
}
static DEVICE_ATTR_RO(max_charge_current);

static ssize_t max_discharge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_lehua.max_discharge_current);
	else
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_molokini.max_discharge_current);
}
static DEVICE_ATTR_RO(max_discharge_current);

static ssize_t max_avg_discharge_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_lehua.max_avg_discharge_current);
	else
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_molokini.max_avg_discharge_current);
}
static DEVICE_ATTR_RO(max_avg_discharge_current);

static ssize_t max_avg_dsg_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_lehua.max_avg_dsg_power);
	else
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_molokini.max_avg_dsg_power);
}
static DEVICE_ATTR_RO(max_avg_dsg_power);

static ssize_t max_temp_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_lehua.max_temp_cell);
	else
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_molokini.max_temp_cell);
}
static DEVICE_ATTR_RO(max_temp_cell);

static ssize_t min_temp_cell_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_lehua.min_temp_cell);
	else
		return scnprintf(buf, PAGE_SIZE, "0x%x\n",
			pd->params.lifetime1.ldb_molokini.min_temp_cell);
}
static DEVICE_ATTR_RO(min_temp_cell);

static ssize_t max_temp_int_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n",
		pd->params.lifetime1.ldb_molokini.max_temp_int_sensor);
}
static DEVICE_ATTR_RO(max_temp_int_sensor);

static ssize_t min_temp_int_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n",
		pd->params.lifetime1.ldb_molokini.min_temp_int_sensor);
}
static DEVICE_ATTR_RO(min_temp_int_sensor);

static ssize_t total_fw_runtime_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "%u\n",
			TWO_HRS_TO_SECONDS *
			pd->params.lifetime2.ldb_lehua.total_fw_runtime);
	else
		return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.lifetime3);
}
static DEVICE_ATTR_RO(total_fw_runtime);

static ssize_t num_valid_charge_terminations_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "%u\n",
			pd->params.lifetime8.ldb_lehua.num_valid_charge_terminations);
	else
		return scnprintf(buf, PAGE_SIZE, "%u\n",
			pd->params.lifetime4.ldb_molokini.num_valid_charge_terminations);
}
static DEVICE_ATTR_RO(num_valid_charge_terminations);

static ssize_t last_valid_charge_term_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	if (pd->batt_id == EXT_BATT_ID_LEHUA)
		return scnprintf(buf, PAGE_SIZE, "%u\n",
			pd->params.lifetime8.ldb_lehua.last_valid_charge_term);
	else
		return scnprintf(buf, PAGE_SIZE, "%u\n",
			pd->params.lifetime4.ldb_molokini.last_valid_charge_term);
}
static DEVICE_ATTR_RO(last_valid_charge_term);

static ssize_t num_qmax_updates_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		pd->params.lifetime4.ldb_molokini.num_qmax_updates);
}
static DEVICE_ATTR_RO(num_qmax_updates);

static ssize_t last_qmax_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		pd->params.lifetime4.ldb_molokini.last_qmax_update);
}
static DEVICE_ATTR_RO(last_qmax_update);

static ssize_t num_ra_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		pd->params.lifetime4.ldb_molokini.num_ra_update);
}
static DEVICE_ATTR_RO(num_ra_update);

static ssize_t last_ra_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		pd->params.lifetime4.ldb_molokini.last_ra_update);
}
static DEVICE_ATTR_RO(last_ra_update);

/* Temp zones might need to be scaled to convert time from hourly
 * units (2h/h) to seconds
 */
#define TEMPZONE_RSOC_PARAM(_name, _sname, _tz, _soc, _scale)			\
static ssize_t _name##_show(struct device *dev,				\
	struct device_attribute *attr, char *buf)			\
{									\
	struct ext_batt_pd *pd =	\
		(struct ext_batt_pd *) dev_get_drvdata(dev);	\
	return scnprintf(buf, PAGE_SIZE, "%u\n",	\
		_scale * pd->params.temp_zones.tz_##_sname[_tz][_soc]);	\
}									\
static DEVICE_ATTR_RO(_name)

/* Molokini temp zones - time units are already in seconds */

/* Molokini Temp zones naming format: t_{_tzlabel}_rsoc_{_soclabel} */
#define MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, _soclabel, _soc)			\
	TEMPZONE_RSOC_PARAM(t_##_tzlabel##_rsoc_##_soclabel, molokini, _tz, _soc, 1)

#define MOLOKINI_TEMPZONE_RSOC_PARAMS(_tzlabel, _tz)			\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, a, 0);		\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, b, 1);		\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, c, 2);		\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, d, 3);		\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, e, 4);		\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, f, 5);		\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, g, 6);		\
	MOLOKINI_TEMPZONE_RSOC_PARAM(_tzlabel, _tz, h, 7)

MOLOKINI_TEMPZONE_RSOC_PARAMS(ut,  0);
MOLOKINI_TEMPZONE_RSOC_PARAMS(lt,  1);
MOLOKINI_TEMPZONE_RSOC_PARAMS(stl, 2);
MOLOKINI_TEMPZONE_RSOC_PARAMS(rt,  3);
MOLOKINI_TEMPZONE_RSOC_PARAMS(sth, 4);
MOLOKINI_TEMPZONE_RSOC_PARAMS(ht,  5);
MOLOKINI_TEMPZONE_RSOC_PARAMS(ot,  6);

static struct attribute *ext_batt_molokini_attrs[] = {
	&dev_attr_charger_plugged.attr,
	&dev_attr_connected.attr,
	&dev_attr_mount_state.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_status.attr,
	&dev_attr_charging_suspend_disable.attr,
	&dev_attr_serial.attr,
	&dev_attr_serial_battery.attr,
	&dev_attr_serial_system.attr,
	&dev_attr_pid.attr,
	&dev_attr_vid.attr,
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
ATTRIBUTE_GROUPS(ext_batt_molokini);

/* Lehua+ SysFs nodes */

#define LIFETIME_DATA_BLOCK_PARAM(_name, _sname, _ldb, _fmt, _scale)			\
static ssize_t _name##_show(struct device *dev,				\
	struct device_attribute *attr, char *buf)			\
{									\
	struct ext_batt_pd *pd =	\
		(struct ext_batt_pd *) dev_get_drvdata(dev);	\
	return scnprintf(buf, PAGE_SIZE, _fmt"\n",	\
		_scale * pd->params.lifetime##_ldb.ldb_##_sname._name);	\
}									\
static DEVICE_ATTR_RO(_name)

#define LEHUA_LIFETIME_DATA_BLOCK_PARAM(_name, _ldb, _fmt, _scale)			\
	LIFETIME_DATA_BLOCK_PARAM(_name, lehua, _ldb, _fmt, _scale)

#define LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16_SCALED(_name, _ldb, _scale)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM(_name, _ldb, "%u", _scale)

#define LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16(_name, _ldb)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16_SCALED(_name, _ldb, 1)

#define LEHUA_LIFETIME_DATA_BLOCK_PARAM_HEX(_name, _ldb)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM(_name, _ldb, "0x%x", 1)

/* LDB 1 */

#define LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(_name)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16(_name, 1)

LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(max_voltage_cell_1);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(max_voltage_cell_2);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(max_voltage_cell_3);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(max_voltage_cell_4);

LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(min_voltage_cell_1);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(min_voltage_cell_2);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(min_voltage_cell_3);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(min_voltage_cell_4);

LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(max_delta_cell_voltage);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(max_temp_delta);
LEHUA_LIFETIME1_DATA_BLOCK_PARAM_U16(max_temp_fet);

/* LDB 2 */

#define LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16(_name)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16(_name, 2)

#define LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(_name, _scale)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16_SCALED(_name, 2, _scale)

#define LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(_name)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_HEX(_name, 2)

LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(num_shutdowns);

LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(avg_temp_cell);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(min_temp_cell_a);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(min_temp_cell_b);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(min_temp_cell_c);

LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(max_temp_cell_a);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(max_temp_cell_b);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_HEX(max_temp_cell_c);

LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_ut,  TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_lt,  TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_stl, TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_rt,  TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_sth, TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_ht,  TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_ot,  TWO_HRS_TO_SECONDS);

LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_hvlt, HR_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_hvmt, HR_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(time_spent_hvht, HR_TO_SECONDS);
LEHUA_LIFETIME2_DATA_BLOCK_PARAM_U16_SCALED(using_time_pf,   HR_TO_SECONDS);

/* LDB 6 */

#define LEHUA_LIFETIME6_DATA_BLOCK_PARAM_U16_SCALED(_name, _scale)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16_SCALED(_name, 6, _scale)

LEHUA_LIFETIME6_DATA_BLOCK_PARAM_U16_SCALED(cb_time_cell_1, TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME6_DATA_BLOCK_PARAM_U16_SCALED(cb_time_cell_2, TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME6_DATA_BLOCK_PARAM_U16_SCALED(cb_time_cell_3, TWO_HRS_TO_SECONDS);
LEHUA_LIFETIME6_DATA_BLOCK_PARAM_U16_SCALED(cb_time_cell_4, TWO_HRS_TO_SECONDS);

/* LDB 7 */

#define LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(_name)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16(_name, 7)

LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_cov_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_cov_event);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_cuv_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_cuv_event);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_ocd_1_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_ocd_1_event);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_ocd_2_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_ocd_2_event);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_occ_1_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_occ_1_event);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_occ_2_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_occ_2_event);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_aold_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_aold_event);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(num_ascd_events);
LEHUA_LIFETIME7_DATA_BLOCK_PARAM_U16(last_ascd_event);

/* LDB 8 */

#define LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(_name)			\
	LEHUA_LIFETIME_DATA_BLOCK_PARAM_U16(_name, 8)

LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(num_ascc_events);
LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(last_ascc_event);
LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(num_otc_events);
LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(last_otc_event);
LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(num_otd_events);
LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(last_otd_event);
LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(num_otf_events);
LEHUA_LIFETIME8_DATA_BLOCK_PARAM_U16(last_otf_event);

/* Lehua temp zones - time units are in 2h so convert to seconds */

/* Lehua Temp zones naming format: st_temp{_tz}_rsoc_{_soc} */
#define LEHUA_TEMPZONE_RSOC_PARAM(_tz, _soc)			\
	TEMPZONE_RSOC_PARAM(st_temp##_tz##_rsoc_##_soc, lehua, 	\
						_tz, _soc, TWO_HRS_TO_SECONDS)

#define LEHUA_TEMPZONE_RSOC_PARAMS(_tz)			\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 0);		\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 1);		\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 2);		\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 3);		\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 4);		\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 5);		\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 6);		\
	LEHUA_TEMPZONE_RSOC_PARAM(_tz, 7)

LEHUA_TEMPZONE_RSOC_PARAMS(0);
LEHUA_TEMPZONE_RSOC_PARAMS(1);
LEHUA_TEMPZONE_RSOC_PARAMS(2);
LEHUA_TEMPZONE_RSOC_PARAMS(3);
LEHUA_TEMPZONE_RSOC_PARAMS(4);
LEHUA_TEMPZONE_RSOC_PARAMS(5);
LEHUA_TEMPZONE_RSOC_PARAMS(6);

/* LDBs as nodes */
static ssize_t ldb_show_common(char *buf, struct ldb_values *ldb)
{
	int i, len = 0;

	for (i = MAX_VDOS_PER_VDM-1; i >= 0; i--)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0x%08x ",
				ldb->higher[i]);
	for (i = MAX_VDOS_PER_VDM-1; i >= 0; i--)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0x%08x ",
				ldb->lower[i]);
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

#define LDB_PARAM(_name, _ldb)			\
static ssize_t _name##_show(struct device *dev,				\
	struct device_attribute *attr, char *buf)			\
{									\
	struct ext_batt_pd *pd =	\
		(struct ext_batt_pd *) dev_get_drvdata(dev);	\
	return ldb_show_common(buf, &pd->params.lifetime##_ldb.values);	\
}									\
static DEVICE_ATTR_RO(_name)

#define LEHUA_LDB_PARAM(_ldb)			\
	LDB_PARAM(ldb##_ldb, _ldb)

LEHUA_LDB_PARAM(1);
LEHUA_LDB_PARAM(2);
LEHUA_LDB_PARAM(7);
LEHUA_LDB_PARAM(8);

static ssize_t manufacturer_info_a_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		pd->params.manufacturer_info_a.data);
}
static DEVICE_ATTR_RO(manufacturer_info_a);

static ssize_t manufacturer_info_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int i, len = 0;

	for (i = MAX_VDOS_PER_VDM-1; i >= 0; i--)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0x%08x ",
				pd->params.manufacturer_info_b.values.higher[i]);
	for (i = MAX_VDOS_PER_VDM-1; i >= 0; i--)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0x%08x ",
				pd->params.manufacturer_info_b.values.lower[i]);
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;

}
static DEVICE_ATTR_RO(manufacturer_info_b);

/* Lehua+ error conditions reported to HMD */

static ssize_t error_common(struct device *dev,
		struct device_attribute *attr, char *buf, int param)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);
	int index = param - EXT_BATT_FW_ERROR_UFP_LPD;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		pd->params.error_conditions[index]);
}

static ssize_t error_ufp_lpd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_UFP_LPD);
}
static DEVICE_ATTR_RO(error_ufp_lpd);

static ssize_t error_ufp_ocp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_UFP_OCP);
}
static DEVICE_ATTR_RO(error_ufp_ocp);

static ssize_t error_drp_ocp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_DRP_OCP);
}
static DEVICE_ATTR_RO(error_drp_ocp);

static ssize_t error_ufp_otp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_UFP_OTP);
}
static DEVICE_ATTR_RO(error_ufp_otp);

static ssize_t error_drp_otp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_DRP_OTP);
}
static DEVICE_ATTR_RO(error_drp_otp);

static ssize_t error_batt_otp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_BATT_OTP);
}
static DEVICE_ATTR_RO(error_batt_otp);

static ssize_t error_pcm_otp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_PCM_OTP);
}
static DEVICE_ATTR_RO(error_pcm_otp);

static ssize_t error_drp_scp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_DRP_SCP);
}
static DEVICE_ATTR_RO(error_drp_scp);

static ssize_t error_ufp_ovp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_UFP_OVP);
}
static DEVICE_ATTR_RO(error_ufp_ovp);

static ssize_t error_drp_ovp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return error_common(dev, attr, buf, EXT_BATT_FW_ERROR_DRP_OVP);
}
static DEVICE_ATTR_RO(error_drp_ovp);

static struct attribute *ext_batt_lehua_attrs[] = {
	&dev_attr_charger_plugged.attr,
	&dev_attr_charging_suspend_disable.attr,
	&dev_attr_connected.attr,
	&dev_attr_mount_state.attr,
	&dev_attr_reboot_into_bootloader.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_rsoc_raw.attr,
	&dev_attr_status.attr,
	&dev_attr_battery_status.attr,
	&dev_attr_serial.attr,
	&dev_attr_serial_battery.attr,
	&dev_attr_serial_system.attr,
	&dev_attr_pid.attr,
	&dev_attr_vid.attr,
	&dev_attr_temp_fg.attr,
	&dev_attr_voltage.attr,
	&dev_attr_icurrent.attr,
	&dev_attr_remaining_capacity.attr,
	&dev_attr_fcc.attr,
	&dev_attr_pack_assembly_pn.attr,
	&dev_attr_cycle_count.attr,
	&dev_attr_soh.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_device_name.attr,

	/* LDB 1 */
	&dev_attr_max_voltage_cell_1.attr,
	&dev_attr_max_voltage_cell_2.attr,
	&dev_attr_max_voltage_cell_3.attr,
	&dev_attr_max_voltage_cell_4.attr,
	&dev_attr_min_voltage_cell_1.attr,
	&dev_attr_min_voltage_cell_2.attr,
	&dev_attr_min_voltage_cell_3.attr,
	&dev_attr_min_voltage_cell_4.attr,
	&dev_attr_max_delta_cell_voltage.attr,
	&dev_attr_max_charge_current.attr,
	&dev_attr_max_discharge_current.attr,
	&dev_attr_max_avg_discharge_current.attr,
	&dev_attr_max_avg_dsg_power.attr,
	&dev_attr_max_temp_cell.attr,
	&dev_attr_min_temp_cell.attr,
	&dev_attr_max_temp_delta.attr,
	&dev_attr_max_temp_fet.attr,

	/* LDB 2 */
	&dev_attr_num_shutdowns.attr,
	&dev_attr_avg_temp_cell.attr,
	&dev_attr_min_temp_cell_a.attr,
	&dev_attr_min_temp_cell_b.attr,
	&dev_attr_min_temp_cell_c.attr,
	&dev_attr_max_temp_cell_a.attr,
	&dev_attr_max_temp_cell_b.attr,
	&dev_attr_max_temp_cell_c.attr,
	&dev_attr_time_spent_ut.attr,
	&dev_attr_time_spent_lt.attr,
	&dev_attr_time_spent_stl.attr,
	&dev_attr_time_spent_rt.attr,
	&dev_attr_time_spent_sth.attr,
	&dev_attr_time_spent_ht.attr,
	&dev_attr_time_spent_ot.attr,
	&dev_attr_time_spent_hvlt.attr,
	&dev_attr_time_spent_hvmt.attr,
	&dev_attr_time_spent_hvht.attr,
	&dev_attr_total_fw_runtime.attr,
	&dev_attr_using_time_pf.attr,

	/* LDB 6 */
	&dev_attr_cb_time_cell_1.attr,
	&dev_attr_cb_time_cell_2.attr,
	&dev_attr_cb_time_cell_3.attr,
	&dev_attr_cb_time_cell_4.attr,

	/* LDB 7 */
	&dev_attr_num_cov_events.attr,
	&dev_attr_last_cov_event.attr,
	&dev_attr_num_cuv_events.attr,
	&dev_attr_last_cuv_event.attr,
	&dev_attr_num_ocd_1_events.attr,
	&dev_attr_last_ocd_1_event.attr,
	&dev_attr_num_ocd_2_events.attr,
	&dev_attr_last_ocd_2_event.attr,
	&dev_attr_num_occ_1_events.attr,
	&dev_attr_last_occ_1_event.attr,
	&dev_attr_num_occ_2_events.attr,
	&dev_attr_last_occ_2_event.attr,
	&dev_attr_num_aold_events.attr,
	&dev_attr_last_aold_event.attr,
	&dev_attr_num_ascd_events.attr,
	&dev_attr_last_ascd_event.attr,

	/* LDB 8 */
	&dev_attr_num_ascc_events.attr,
	&dev_attr_last_ascc_event.attr,
	&dev_attr_num_otc_events.attr,
	&dev_attr_last_otc_event.attr,
	&dev_attr_num_otd_events.attr,
	&dev_attr_last_otd_event.attr,
	&dev_attr_num_otf_events.attr,
	&dev_attr_last_otf_event.attr,
	&dev_attr_num_valid_charge_terminations.attr,
	&dev_attr_last_valid_charge_term.attr,

	/* Temp zone 0 storage times */
	&dev_attr_st_temp0_rsoc_0.attr,
	&dev_attr_st_temp0_rsoc_1.attr,
	&dev_attr_st_temp0_rsoc_2.attr,
	&dev_attr_st_temp0_rsoc_3.attr,
	&dev_attr_st_temp0_rsoc_4.attr,
	&dev_attr_st_temp0_rsoc_5.attr,
	&dev_attr_st_temp0_rsoc_6.attr,
	&dev_attr_st_temp0_rsoc_7.attr,

	/* Temp zone 1 storage times */
	&dev_attr_st_temp1_rsoc_0.attr,
	&dev_attr_st_temp1_rsoc_1.attr,
	&dev_attr_st_temp1_rsoc_2.attr,
	&dev_attr_st_temp1_rsoc_3.attr,
	&dev_attr_st_temp1_rsoc_4.attr,
	&dev_attr_st_temp1_rsoc_5.attr,
	&dev_attr_st_temp1_rsoc_6.attr,
	&dev_attr_st_temp1_rsoc_7.attr,

	/* Temp zone 2 storage times */
	&dev_attr_st_temp2_rsoc_0.attr,
	&dev_attr_st_temp2_rsoc_1.attr,
	&dev_attr_st_temp2_rsoc_2.attr,
	&dev_attr_st_temp2_rsoc_3.attr,
	&dev_attr_st_temp2_rsoc_4.attr,
	&dev_attr_st_temp2_rsoc_5.attr,
	&dev_attr_st_temp2_rsoc_6.attr,
	&dev_attr_st_temp2_rsoc_7.attr,

	/* Temp zone 3 storage times */
	&dev_attr_st_temp3_rsoc_0.attr,
	&dev_attr_st_temp3_rsoc_1.attr,
	&dev_attr_st_temp3_rsoc_2.attr,
	&dev_attr_st_temp3_rsoc_3.attr,
	&dev_attr_st_temp3_rsoc_4.attr,
	&dev_attr_st_temp3_rsoc_5.attr,
	&dev_attr_st_temp3_rsoc_6.attr,
	&dev_attr_st_temp3_rsoc_7.attr,

	/* Temp zone 4 storage times */
	&dev_attr_st_temp4_rsoc_0.attr,
	&dev_attr_st_temp4_rsoc_1.attr,
	&dev_attr_st_temp4_rsoc_2.attr,
	&dev_attr_st_temp4_rsoc_3.attr,
	&dev_attr_st_temp4_rsoc_4.attr,
	&dev_attr_st_temp4_rsoc_5.attr,
	&dev_attr_st_temp4_rsoc_6.attr,
	&dev_attr_st_temp4_rsoc_7.attr,

	/* Temp zone 5 storage times */
	&dev_attr_st_temp5_rsoc_0.attr,
	&dev_attr_st_temp5_rsoc_1.attr,
	&dev_attr_st_temp5_rsoc_2.attr,
	&dev_attr_st_temp5_rsoc_3.attr,
	&dev_attr_st_temp5_rsoc_4.attr,
	&dev_attr_st_temp5_rsoc_5.attr,
	&dev_attr_st_temp5_rsoc_6.attr,
	&dev_attr_st_temp5_rsoc_7.attr,

	/* Temp zone 6 storage times */
	&dev_attr_st_temp6_rsoc_0.attr,
	&dev_attr_st_temp6_rsoc_1.attr,
	&dev_attr_st_temp6_rsoc_2.attr,
	&dev_attr_st_temp6_rsoc_3.attr,
	&dev_attr_st_temp6_rsoc_4.attr,
	&dev_attr_st_temp6_rsoc_5.attr,
	&dev_attr_st_temp6_rsoc_6.attr,
	&dev_attr_st_temp6_rsoc_7.attr,

	/* LDBs */
	&dev_attr_ldb1.attr,
	&dev_attr_ldb2.attr,
	&dev_attr_ldb7.attr,
	&dev_attr_ldb8.attr,

	&dev_attr_manufacturer_info_a.attr,
	&dev_attr_manufacturer_info_b.attr,

	/* Error conditions reported to HMD */
	&dev_attr_error_ufp_lpd.attr,
	&dev_attr_error_ufp_ocp.attr,
	&dev_attr_error_drp_ocp.attr,
	&dev_attr_error_ufp_otp.attr,
	&dev_attr_error_drp_otp.attr,
	&dev_attr_error_batt_otp.attr,
	&dev_attr_error_pcm_otp.attr,
	&dev_attr_error_drp_scp.attr,
	&dev_attr_error_ufp_ovp.attr,
	&dev_attr_error_drp_ovp.attr,

	NULL,
};
ATTRIBUTE_GROUPS(ext_batt_lehua);

static void ext_batt_create_sysfs(struct ext_batt_pd *pd)
{
	int result = 0;

	/* Mount relevant nodes */
	if (pd->batt_id == EXT_BATT_ID_MOLOKINI)
		result = sysfs_create_groups(&pd->dev->kobj, ext_batt_molokini_groups);
	else if (pd->batt_id == EXT_BATT_ID_LEHUA)
		result = sysfs_create_groups(&pd->dev->kobj, ext_batt_lehua_groups);
	if (result != 0)
		dev_err(pd->dev, "Error creating sysfs entries: %d\n", result);
}

static void ext_batt_mount_status_work(struct work_struct *work)
{
	struct ext_batt_pd *pd =
		container_of(work, struct ext_batt_pd, mount_state_work);
	int result;
	u32 mount_state_header = 0;
	u32 mount_state_vdo = pd->mount_state;
	u32 vdm_period = 0;

	dev_dbg(pd->dev, "%s: enter", __func__);

	mutex_lock(&pd->lock);
	if (!pd->connected) {
		mutex_unlock(&pd->lock);
		return;
	}

	mount_state_header =
		VDMH_CONSTRUCT(
			VDM_SVID_META,
			0, 1, 0, 1,
			EXT_BATT_FW_HMD_MOUNTED);

	if (pd->mount_state == 0 && pd->first_broadcast_data_received)
		vdm_period = EXT_BATT_FW_VDM_PERIOD_OFF_HEAD;
	else
		vdm_period = EXT_BATT_FW_VDM_PERIOD_ON_HEAD;
	mount_state_vdo |= (vdm_period << 8);

	reinit_completion(&pd->mount_state_ack);
	result = external_battery_send_vdm(pd, mount_state_header,
		&mount_state_vdo, 1);
	if (result != 0) {
		dev_err(pd->dev, "Error sending HMD mount request: %d", result);
		mutex_unlock(&pd->lock);
		return;
	}

	result = wait_for_completion_timeout(&pd->mount_state_ack,
			msecs_to_jiffies(MOUNT_STATE_ACK_TIMEOUT_MS));
	if (!result) {
		dev_err(pd->dev, "Timed out waiting for mount state ACK, retrying");
		schedule_work(&pd->mount_state_work);
	}

	mutex_unlock(&pd->lock);
}

static void ext_batt_dock_state_work(struct work_struct *work)
{
	struct ext_batt_pd *pd =
		container_of(work, struct ext_batt_pd, dock_state_work);
	union power_supply_propval val;
	int rc;
	u32 dock_state_hdr, dock_state_vdo;

	dev_dbg(pd->dev, "%s: enter", __func__);

	mutex_lock(&pd->lock);
	if (!pd->connected || !pd->cypd_psy)
		goto out;

	rc = power_supply_get_property(pd->cypd_psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (rc < 0) {
		dev_err(pd->dev, "couldn't read cypd status: rc=%d\n", rc);
		goto out;
	}

	pd->dock_state = (val.intval == POWER_SUPPLY_STATUS_CHARGING);

	if (pd->dock_state == pd->last_dock_ack)
		goto out;

	dock_state_hdr =
		VDMH_CONSTRUCT(
			VDM_SVID_META,
			0, VDM_REQUEST, 0, 1,
			EXT_BATT_FW_HMD_DOCKED);

	dock_state_vdo = pd->dock_state;

	dev_dbg(pd->dev, "sending dock VDM: dock_state=%d", dock_state_vdo);
	rc = external_battery_send_vdm(pd, dock_state_hdr, &dock_state_vdo, 1);
	if (rc < 0)
		dev_err(pd->dev, "error sending dock VDM: rc=%d", rc);

out:
	mutex_unlock(&pd->lock);
}

static void ext_batt_pr_swap(struct work_struct *work)
{
	struct ext_batt_pr_swap_work *prs_work =
		container_of(work, struct ext_batt_pr_swap_work, work);
	struct ext_batt_pd *pd = prs_work->pd;
	union power_supply_propval power_role = {0};
	int rc = 0;

	mutex_lock(&pd->lock);
	pd->last_dock_ack = prs_work->vdo;

	/*
	 * dock_state (0=undocked, 1=docked) conveniently matches the power
	 * role we'd like to switch to (0=sink, 1=source)
	 */
	power_role.intval = prs_work->vdo;
	/*
	 * Only attempt PR_SWAP in two cases:
	 * - we recently docked, attempt PR_SWAP so HMD is source
	 * - we were docked, but have since undocked, attempt PR_SWAP
	 *   so HMD is sink
	 */
	if (pd->last_dock_ack == EXT_BATT_FW_UNDOCKED) {
		if (!pd->recently_docked)
			goto out;
		pd->recently_docked = false;
	} else if (pd->last_dock_ack == EXT_BATT_FW_DOCKED) {
		pd->recently_docked = true;
	}

	dev_info(pd->dev, "Sending PR_SWAP to role=%d", power_role.intval);
	rc = power_supply_set_property(pd->usb_psy,
				       POWER_SUPPLY_PROP_TYPEC_POWER_ROLE,
				       &power_role);
	if (rc)
		dev_err(pd->dev, "Failed sending PR_SWAP: rc=%d", rc);
	mutex_unlock(&pd->lock);

	/* Force re-evaluation, so source current can be set correctly */
	ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
				   pd->battery_psy);

	kfree(prs_work);
	return;
out:
	mutex_unlock(&pd->lock);
	kfree(prs_work);
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

	if (pd->usb_psy_charging_state == state) {
		dev_dbg(pd->dev, "Current state is %s, no action needed\n",
				usb_charging_state(state));
		return;
	}

	if (IS_ERR_OR_NULL(pd->usb_psy)) {
		dev_err(pd->dev, "USB power supply handle is invalid.\n");
		return;
	}

	val.intval = state;

	result = power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
	/* the default timeout is too short, ignore it */
	if (result && result != -ETIMEDOUT) {
		dev_err(pd->dev, "Unable to update USB charging state: %d\n", result);
		return;
	}

	pd->usb_psy_charging_state = state;

	dev_dbg(pd->dev,
			"USB charging state updated successfully. New state: %s\n",
			usb_charging_state(state));
}

static void set_usb_output_current_limit(struct ext_batt_pd *pd, int current_ua)
{
	union power_supply_propval val = {current_ua};
	int result = 0;

	if (IS_ERR_OR_NULL(pd->usb_psy)) {
		dev_err(pd->dev, "USB power supply handle is invalid.");
		return;
	}

	dev_dbg(pd->dev, "Setting source current to %d uA", val.intval);

	result = power_supply_set_property(pd->usb_psy,
				POWER_SUPPLY_PROP_OUTPUT_CURRENT_LIMIT, &val);

	/* the default timeout is too short, ignore it */
	if (result && result != -ETIMEDOUT) {
		dev_err(pd->dev, "Unable to set output current limit: rc=%d\n", result);
		return;
	}

	pd->source_current = current_ua;
}

static void handle_battery_capacity_change(struct ext_batt_pd *pd,
		u32 capacity)
{
	dev_dbg(pd->dev,
		"Battery capacity change event. Capacity = %d, connected = %d, mount state = %d, charger plugged = %d\n",
		capacity, pd->connected, pd->mount_state,
		pd->params.charger_plugged);

	if (pd->src_current_control_enabled) {
		if (capacity >= pd->src_enable_soc_threshold)
			set_usb_output_current_limit(pd, pd->src_current_limit_max_uA);
		else
			set_usb_output_current_limit(pd, 0);
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

	if (pd->cypd_psy && psy == pd->cypd_psy)
		schedule_work(&pd->dock_state_work);

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
	pd->battery_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(pd->battery_psy)) {
		dev_dbg(pd->dev, "Failed to get battery power supply\n");
		return -EPROBE_DEFER;
	}

	pd->usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(pd->usb_psy)) {
		dev_dbg(pd->dev, "Failed to get USB power supply\n");
		return -EPROBE_DEFER;
	}

	if (IS_ENABLED(CONFIG_CHARGER_CYPD3177)) {
		pd->cypd_psy = power_supply_get_by_name("cypd3177");
		if (!pd->cypd_psy) {
			dev_dbg(pd->dev, "Failed to get CYPD psy");
			return -EPROBE_DEFER;
		}
	}

	return 0;
}

static void external_battery_usbvdm_connect(struct usbvdm_subscription *sub,
		u16 vid, u16 pid)
{
	struct ext_batt_pd *pd = usbvdm_subscriber_get_drvdata(sub);

	ext_batt_vdm_connect(pd, true);
}

static void external_battery_usbvdm_disconnect(struct usbvdm_subscription *sub)
{
	struct ext_batt_pd *pd = usbvdm_subscriber_get_drvdata(sub);

	ext_batt_vdm_disconnect(pd);
}

static void external_battery_usbvdm_vdm_rx(struct usbvdm_subscription *sub,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	struct ext_batt_pd *pd = usbvdm_subscriber_get_drvdata(sub);

	ext_batt_vdm_received(pd, vdm_hdr, vdos, num_vdos);
}

static int ext_batt_probe(struct platform_device *pdev)
{
	int result = 0;
	struct ext_batt_pd *pd;
	const char *batt_name;
	u16 usb_id[2];
	struct usbvdm_subscriber_ops ops = {
		.connect = external_battery_usbvdm_connect,
		.disconnect = external_battery_usbvdm_disconnect,
		.vdm = external_battery_usbvdm_vdm_rx,
	};

	dev_dbg(&pdev->dev, "%s: enter", __func__);

	pd = devm_kzalloc(
			&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

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

	pd->rsoc_scaling_enabled = of_property_read_bool(pdev->dev.of_node,
			"rsoc-scaling");
	dev_dbg(&pdev->dev, "rsoc scaling enabled=%d\n", pd->rsoc_scaling_enabled);

	if (pd->rsoc_scaling_enabled) {
		result = of_property_read_u32(pdev->dev.of_node,
				"rsoc-scaling-min-level", &pd->rsoc_scaling_min_level);
		if (result < 0) {
			dev_err(&pdev->dev,
					"rsoc scaling min value unavailable, failing probe, result:%d\n",
					result);
			return result;
		}
		dev_dbg(&pdev->dev, "rsoc scaling min value=%d\n",
				pd->rsoc_scaling_min_level);

		result = of_property_read_u32(pdev->dev.of_node,
			"rsoc-scaling-max-level", &pd->rsoc_scaling_max_level);
		if (result < 0) {
			dev_err(&pdev->dev,
					"rsoc scaling max level unavailable, failing probe, result:%d\n",
					result);
			return result;
		}
		dev_dbg(&pdev->dev, "rsoc scaling max level=%d\n",
				pd->rsoc_scaling_max_level);
	}

	pd->src_current_control_enabled = of_property_read_bool(pdev->dev.of_node,
			"src-current-control");
	dev_dbg(&pdev->dev, "src current control enabled=%d\n", pd->src_current_control_enabled);

	result = of_property_read_u32(pdev->dev.of_node,
			"src-enable-soc-threshold", &pd->src_enable_soc_threshold);
	if (result < 0) {
		pd->src_enable_soc_threshold = DEFAULT_SRC_ENABLE_SOC_THRESHOLD;
		result = 0;
	}

	result = of_property_read_u32(pdev->dev.of_node,
			"src-current-limit-max-uA", &pd->src_current_limit_max_uA);
	if (result < 0) {
		pd->src_current_limit_max_uA = DEFAULT_SRC_CURRENT_LIMIT_MAX_UA;
		result = 0;
	}


	pd->batt_id = EXT_BATT_ID_MOLOKINI;
	result = of_property_read_string(pdev->dev.of_node, "batt-name", &batt_name);
	if (result < 0)
		dev_dbg(&pdev->dev, "Could not find battery name\n");
	else if (strcmp(batt_name, "lehua") == 0)
		pd->batt_id = EXT_BATT_ID_LEHUA;

	mutex_init(&pd->lock);

	init_completion(&pd->mount_state_ack);
	pd->last_dock_ack = EXT_BATT_FW_DOCK_STATE_UNKNOWN;
	INIT_WORK(&pd->mount_state_work, ext_batt_mount_status_work);
	INIT_WORK(&pd->dock_state_work, ext_batt_dock_state_work);
	INIT_WORK(&pd->psy_notifier_work, ext_batt_psy_notifier_work);
	result = ext_batt_psy_init(pd);
	if (result == -EPROBE_DEFER)
		return result;

	pd->wq = alloc_ordered_workqueue("ext_batt_wq", WQ_FREEZABLE);
	if (!pd->wq)
		return -ENOMEM;

	pd->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, pd);

	result = device_property_read_u16_array(pd->dev, "meta,usb-id", usb_id, 2);
	if (result < 0) {
		dev_err(pd->dev, "Couldn't read usb-id property: %d", result);
		return result;
	}

	pd->vid = usb_id[0];
	pd->pid = usb_id[1];
	pd->sub = usbvdm_subscribe(pd->dev, pd->vid, pd->pid, ops);
	if (IS_ERR_OR_NULL(pd->sub))
		return pd->sub ? PTR_ERR(pd->sub) : -ENODEV;

	usbvdm_subscriber_set_drvdata(pd->sub, pd);

	/* Query power supply and register for events */
	pd->nb.notifier_call = ext_batt_psy_notifier_call;
	ext_batt_psy_register_notifier(pd);

	/* Create nodes here. */
	ext_batt_create_sysfs(pd);
	ext_batt_reset(pd);

	return result;
}

static int ext_batt_remove(struct platform_device *pdev)
{
	struct ext_batt_pd *pd = platform_get_drvdata(pdev);

	dev_dbg(pd->dev, "%s: enter", __func__);

	mutex_lock(&pd->lock);
	pd->connected = false;
	mutex_unlock(&pd->lock);
	usbvdm_unsubscribe(pd->sub);
	cancel_work_sync(&pd->mount_state_work);
	cancel_work_sync(&pd->dock_state_work);
	destroy_workqueue(pd->wq);
	set_usb_charging_state(pd, CHARGING_RESUME);
	ext_batt_psy_unregister_notifier(pd);
	if (pd->cypd_psy)
		power_supply_put(pd->cypd_psy);
	mutex_destroy(&pd->lock);

	if (pd->batt_id == EXT_BATT_ID_MOLOKINI)
		sysfs_remove_groups(&pd->dev->kobj, ext_batt_molokini_groups);
	else if (pd->batt_id == EXT_BATT_ID_LEHUA)
		sysfs_remove_groups(&pd->dev->kobj, ext_batt_lehua_groups);

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
