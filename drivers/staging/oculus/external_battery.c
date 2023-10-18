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

#define ENABLE_OTG_SOC_THRESHOLD 95
#define OTG_CURRENT_LIMIT_UA 1500000

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

/*
 * TODO(T158272135): This struct will be made non-specific to Glink when
 * PID-based registration is added to USBPD and CYPD, likely using a union of
 * the three handler types
 */
struct glink_svid_handler_info {
	struct ext_batt_pd *pd;
	struct glink_svid_handler handler;
	struct list_head entry;
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


static void usbpd_vdm_connect(struct usbpd_svid_handler *hdlr, bool usb_comm)
{
	struct ext_batt_pd *pd = container_of(hdlr, struct ext_batt_pd, usbpd_hdlr);

	ext_batt_vdm_connect(pd, usb_comm);
}

static void usbpd_vdm_disconnect(struct usbpd_svid_handler *hdlr)
{
	struct ext_batt_pd *pd = container_of(hdlr, struct ext_batt_pd, usbpd_hdlr);

	ext_batt_vdm_disconnect(pd);
}

static void usbpd_vdm_received(struct usbpd_svid_handler *hdlr,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	struct ext_batt_pd *pd = container_of(hdlr, struct ext_batt_pd, usbpd_hdlr);

	ext_batt_vdm_received(pd, vdm_hdr, vdos, num_vdos);
}

static void glink_vdm_connect(struct glink_svid_handler *hdlr, bool usb_comm)
{
	struct glink_svid_handler_info *handler_info =
			container_of(hdlr, struct glink_svid_handler_info, handler);

	ext_batt_vdm_connect(handler_info->pd, usb_comm);
}

static void glink_vdm_disconnect(struct glink_svid_handler *hdlr)
{
	struct glink_svid_handler_info *handler_info =
			container_of(hdlr, struct glink_svid_handler_info, handler);

	ext_batt_vdm_disconnect(handler_info->pd);
}

static void glink_vdm_received(struct glink_svid_handler *hdlr,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	struct glink_svid_handler_info *handler_info =
			container_of(hdlr, struct glink_svid_handler_info, handler);

	ext_batt_vdm_received(handler_info->pd, vdm_hdr, vdos, num_vdos);
}

int external_battery_register_svid_handlers(struct ext_batt_pd *pd)
{
	int i, num_supported;
	u16 *supported_devices;
	struct glink_svid_handler_info *handler_info;
	int rc;

	if (IS_ENABLED(CONFIG_USB_PD_POLICY)) {
		pd->usbpd_hdlr.svid = pd->svid;
		pd->usbpd_hdlr.connect = usbpd_vdm_connect;
		pd->usbpd_hdlr.disconnect = usbpd_vdm_disconnect;
		pd->usbpd_hdlr.vdm_received = usbpd_vdm_received;

		return usbpd_register_svid(pd->intf_usbpd, &pd->usbpd_hdlr);
	}

	if (IS_ENABLED(CONFIG_OCULUS_VDM_GLINK)) {
		INIT_LIST_HEAD(&pd->glink_handlers);

		num_supported = device_property_read_u16_array(pd->dev,
				"supported-devices", NULL, 0);
		if (num_supported <= 0 || (num_supported % 2) != 0) {
			dev_err(pd->dev,
					"supported-devices must be specified as a list of SVID/PID pairs, rc=%d",
					num_supported);
			return -EINVAL;
		}

		supported_devices = kcalloc(num_supported, sizeof(u16), GFP_KERNEL);
		if (!supported_devices)
			return -ENOMEM;

		rc = device_property_read_u16_array(pd->dev,
				"supported-devices", supported_devices, num_supported);
		if (rc) {
			dev_err(pd->dev,
					"failed reading supported-devices, rc=%d num_supported=%d",
					rc, num_supported);
			kfree(supported_devices);
			return rc;
		}

		for (i = 0; i < num_supported; i += 2) {
			handler_info = devm_kzalloc(pd->dev,
					sizeof(*handler_info), GFP_KERNEL);
			if (!handler_info) {
				kfree(supported_devices);
				return -ENOMEM;
			}

			handler_info->pd = pd;
			handler_info->handler.svid = supported_devices[i];
			handler_info->handler.pid = supported_devices[i+1];
			handler_info->handler.connect = glink_vdm_connect;
			handler_info->handler.disconnect = glink_vdm_disconnect;
			handler_info->handler.vdm_received = glink_vdm_received;
			list_add_tail(&handler_info->entry, &pd->glink_handlers);

			rc = vdm_glink_register_handler(pd->intf_glink, &handler_info->handler);
			if (rc != 0) {
				dev_err(pd->dev,
						"failed registering glink handler, SVID/PID = 0x%04x/0x%04x",
						handler_info->handler.svid, handler_info->handler.pid);
				kfree(supported_devices);
				return rc;
			}
			dev_dbg(pd->dev, "Registered glink SVID/PID 0x%04x/0x%04x",
					handler_info->handler.svid, handler_info->handler.pid);
		}

		kfree(supported_devices);
		return 0;
	}

	return -EINVAL;
}

void external_battery_unregister_svid_handlers(struct ext_batt_pd *pd)
{
	struct glink_svid_handler_info *handler_info;

	if (IS_ENABLED(CONFIG_USB_PD_POLICY))
		usbpd_unregister_svid(pd->intf_usbpd, &pd->usbpd_hdlr);

	if (IS_ENABLED(CONFIG_OCULUS_VDM_GLINK))
		list_for_each_entry(handler_info, &pd->glink_handlers, entry)
			vdm_glink_unregister_handler(pd->intf_glink, &handler_info->handler);
}

int external_battery_send_vdm(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	if (IS_ENABLED(CONFIG_USB_PD_POLICY))
		return usbpd_send_vdm(pd->intf_usbpd, vdm_hdr, vdos, num_vdos);

	if (IS_ENABLED(CONFIG_OCULUS_VDM_GLINK))
		return vdm_glink_send_vdm(pd->intf_glink, vdm_hdr, vdos, num_vdos);

	return -EINVAL;
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
	pd->last_dock_ack = EXT_BATT_FW_DOCK_STATE_UNKNOWN;
	pd->params.rsoc = 0;
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
	pd->last_dock_ack = EXT_BATT_FW_DOCK_STATE_UNKNOWN;
	pd->params.rsoc = 0;
	mutex_unlock(&pd->lock);

	cancel_delayed_work_sync(&pd->mount_state_work);
	cancel_delayed_work_sync(&pd->dock_state_dwork);

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

			complete(&pd->mount_state_ack);
		} else if (parameter_type == EXT_BATT_FW_HMD_DOCKED) {
			dev_info(pd->dev,
				"Received dock status ack response code 0x%x", vdos[0]);
			pd->last_dock_ack = vdos[0];

			/*
			 * dock_state (0=undocked, 1=docked) conveniently matches the power
			 * role we'd like to switch to (0=sink, 1=source)
			 */
			power_role.intval = vdos[0];

			/*
			 * don't send PR_SWAP to sink if the ext batt's soc is <= 1 and we haven't had
			 * a chance to charge it (source_current is 0). On Lehua, this indicates it's
			 * in a low power state and needs to be connected as a source to stay awake
			 */
			if (power_role.intval == EXT_BATT_FW_UNDOCKED && pd->params.rsoc <= 1 &&
					pd->source_current == 0)
				return;

			dev_info(pd->dev, "Sending PR_SWAP to role=%d", power_role.intval);
			rc = power_supply_set_property(pd->usb_psy,
					POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &power_role);
			/* the default timeout is too short for PR_SWAP, ignore it */
			if (rc && rc != -ETIMEDOUT)
				dev_err(pd->dev, "Failed sending PR_SWAP: rc=%d", rc);

			/* Force re-evaluation, so source current can be set correctly */
			ext_batt_psy_notifier_call(&pd->nb, PSY_EVENT_PROP_CHANGED,
				pd->battery_psy);
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
				pd->svid,
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
		break;
	/* Handle manufaturer info B data block here since this is not
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

	if (pd->rsoc_scaling_enabled) {
		rsoc = scale_rsoc(rsoc,
			pd->rsoc_scaling_min_level,
			pd->rsoc_scaling_max_level);
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", rsoc);
}
static DEVICE_ATTR_RO(rsoc);

static ssize_t rsoc_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ext_batt_pd *pd =
		(struct ext_batt_pd *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pd->params.rsoc);
}

static ssize_t rsoc_raw_store(struct device *dev,
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
static DEVICE_ATTR_RW(rsoc_raw);

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
	&dev_attr_connected.attr,
	&dev_attr_mount_state.attr,
	&dev_attr_rsoc.attr,
	&dev_attr_rsoc_raw.attr,
	&dev_attr_status.attr,
	&dev_attr_battery_status.attr,
	&dev_attr_serial.attr,
	&dev_attr_serial_battery.attr,
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

	mount_state_header =
		VDMH_CONSTRUCT(
			pd->svid,
			0, 1, 0, 1,
			EXT_BATT_FW_HMD_MOUNTED);

	if (pd->mount_state == 0)
		mount_state_vdo |= (EXT_BATT_FW_VDM_PERIOD_OFF_HEAD << 8);
	else
		mount_state_vdo |= (EXT_BATT_FW_VDM_PERIOD_ON_HEAD << 8);

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
		schedule_delayed_work(&pd->mount_state_work, 0);
	}

	mutex_unlock(&pd->lock);
}

static void ext_batt_dock_state_work(struct work_struct *work)
{
	struct ext_batt_pd *pd =
		container_of(work, struct ext_batt_pd, dock_state_dwork.work);
	int rc;
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

	if (pd->dock_state == pd->last_dock_ack)
		goto out;

	dock_state_hdr =
		VDMH_CONSTRUCT(
			pd->svid,
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

	if (capacity >= ENABLE_OTG_SOC_THRESHOLD)
		set_usb_output_current_limit(pd, OTG_CURRENT_LIMIT_UA);
	else
		set_usb_output_current_limit(pd, 0);

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
		dev_dbg(pd->dev, "Failed to get battery power supply\n");
		return -EPROBE_DEFER;
	}

	pd->usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(pd->usb_psy)) {
		dev_dbg(pd->dev, "Failed to get USB power supply\n");
		return -EPROBE_DEFER;
	}

	#ifdef CONFIG_CHARGER_CYPD3177
	pd->cypd_psy = power_supply_get_by_name("cypd3177");
	if (!pd->cypd_psy) {
		dev_dbg(pd->dev, "Failed to get CYPD psy");
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
	const char *batt_name;

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

	pd->batt_id = EXT_BATT_ID_MOLOKINI;
	result = of_property_read_string(pdev->dev.of_node, "batt-name", &batt_name);
	if (result < 0)
		dev_dbg(&pdev->dev, "Could not find battery name\n");
	else if (strcmp(batt_name, "lehua") == 0)
		pd->batt_id = EXT_BATT_ID_LEHUA;

	mutex_init(&pd->lock);

	init_completion(&pd->mount_state_ack);
	pd->last_dock_ack = EXT_BATT_FW_DOCK_STATE_UNKNOWN;
	INIT_DELAYED_WORK(&pd->mount_state_work, ext_batt_mount_status_work);

	#ifdef CONFIG_CHARGER_CYPD3177
	pd->cypd_pd_active_chan = iio_channel_get(NULL, "cypd_pd_active");
	if (IS_ERR(pd->cypd_pd_active_chan)) {
		if (PTR_ERR(pd->cypd_pd_active_chan) != -EPROBE_DEFER)
			dev_dbg(pd->dev, "cypd pd_active channel unavailable");
		return -EPROBE_DEFER;
	}
	#endif
	INIT_DELAYED_WORK(&pd->dock_state_dwork, ext_batt_dock_state_work);

	pd->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, pd);

	pd->intf_usbpd = devm_usbpd_get_by_phandle(pd->dev, "battery-usbpd");
	if (IS_ENABLED(CONFIG_USB_PD_POLICY) && IS_ERR(pd->intf_usbpd))
		return PTR_ERR(pd->intf_usbpd);

	pd->intf_glink = devm_vdm_glink_get_by_phandle(pd->dev, "vdm-glink");
	if (IS_ENABLED(CONFIG_OCULUS_VDM_GLINK) && IS_ERR(pd->intf_glink))
		return PTR_ERR(pd->intf_glink);

	/* Register VDM callbacks */
	result = external_battery_register_svid_handlers(pd);
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

	external_battery_unregister_svid_handlers(pd);

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
