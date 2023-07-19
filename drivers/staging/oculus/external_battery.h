// SPDX-License-Identifier: GPL-2.0

#ifndef _EXT_BATT_H__
#define _EXT_BATT_H__

#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/usb/usbpd.h>
#include <linux/workqueue.h>

/* Mount states */
enum ext_batt_fw_mount_state {
	EXT_BATT_FW_OFF_HEAD = 0,
	EXT_BATT_FW_ON_HEAD,
	EXT_BATT_FW_UNKNOWN,
};

/* Parameter types */
#define EXT_BATT_FW_VERSION_NUMBER 0x01
#define EXT_BATT_FW_SERIAL 0x02
#define EXT_BATT_FW_SERIAL_BATTERY 0x03
#define EXT_BATT_FW_TEMP_BOARD 0x04
#define EXT_BATT_FW_TEMP_BATT 0x05
#define EXT_BATT_FW_TEMP_FG 0x06
#define EXT_BATT_FW_VOLTAGE 0x08
#define EXT_BATT_FW_BATT_STATUS 0x0A
#define EXT_BATT_FW_CURRENT 0x0C
#define EXT_BATT_FW_REMAINING_CAPACITY 0x10
#define EXT_BATT_FW_FCC 0x12
#define EXT_BATT_FW_CYCLE_COUNT 0x2A
#define EXT_BATT_FW_RSOC 0x2C
#define EXT_BATT_FW_SOH 0x2E
#define EXT_BATT_FW_DEVICE_NAME 0x4A
#define EXT_BATT_FW_LDB1 0x60
#define EXT_BATT_FW_LDB3 0x62
#define EXT_BATT_FW_LDB4 0x63
#define EXT_BATT_FW_LDB6 0x65 /* TEMP ZONE 0 */
#define EXT_BATT_FW_LDB7 0x66 /* TEMP ZONE 1 */
#define EXT_BATT_FW_LDB8 0x67 /* TEMP ZONE 2 */
#define EXT_BATT_FW_LDB9 0x68 /* TEMP ZONE 3 */
#define EXT_BATT_FW_LDB10 0x69 /* TEMP ZONE 4 */
#define EXT_BATT_FW_LDB11 0x6A /* TEMP ZONE 5 */
#define EXT_BATT_FW_LDB12 0x6B /* TEMP ZONE 6 */
#define EXT_BATT_FW_MANUFACTURER_INFO1 0x70
#define EXT_BATT_FW_MANUFACTURER_INFO2 0x7A
#define EXT_BATT_FW_MANUFACTURER_INFO3 0x7B
#define EXT_BATT_FW_HMD_MOUNTED 0x80
#define EXT_BATT_FW_CHARGER_PLUGGED 0x82

/* Vendor Defined Object Section */
#define LIFETIME_1_LOWER_LEN 6
#define LIFETIME_1_HIGHER_LEN 4
#define LIFETIME_4_LEN 6
#define NUM_TEMP_ZONE 7
#define TEMP_ZONE_LEN 8

/* ext_batt TI fuel gauge parameters */
struct ext_batt_parameters {
	/* Manufacturing serial parameters */
	char serial[32];
	char serial_battery[32];

	/* Standard parameters */
	u16 temp_fg;
	u16 voltage;
	char battery_status[16];
	u16 icurrent; /* negative range */
	u16 remaining_capacity;
	u16 fcc;
	u16 cycle_count;
	u8 rsoc;
	u8 soh;
	char device_name[16];
	bool charger_plugged;

	/* Lifetime data blocks */
	u16 lifetime1_lower[LIFETIME_1_LOWER_LEN];
	u8 lifetime1_higher[LIFETIME_1_HIGHER_LEN];
	u32 lifetime3;
	u16 lifetime4[LIFETIME_4_LEN];
	u32 temp_zones[NUM_TEMP_ZONE][TEMP_ZONE_LEN];

	/* Manufacturer info */
	u32 manufacturer_info_a;
	u32 manufacturer_info_b;
	u32 manufacturer_info_c;
};

struct ext_batt_pd {
	/* platform device handle */
	struct device *dev;

	/* client struct to register with usbpd engine */
	struct usbpd_svid_handler vdm_handler;

	/* ext_batt connection status */
	bool connected;

	/* lock for modifying ext_batt_pd struct */
	struct mutex lock;
	/* ext_batt TI fuel gauge debugfs directory */
	struct dentry *debug_root;

	struct ext_batt_parameters params;

	/* mount state held locally, messaged as u32 vdo */
	u32 mount_state;
	/* last ACK received from Molokini for mount status */
	enum ext_batt_fw_mount_state last_mount_ack;
	/* work for periodically processing HMD mount state */
	struct delayed_work mount_state_work;
	/* work for handling the power_supply notifier callback logic */
	struct work_struct psy_notifier_work;
	/* power supply object handle for internal HMD battery */
	struct power_supply *battery_psy;
	/* power supply object handle for USB power supply */
	struct power_supply	*usb_psy;
	/* notifier block for handling power supply change events */
	struct notifier_block nb;
	/* ext_batt on-demand charging suspend disable */
	bool charging_suspend_disable;
	/* battery capacity threshold for charging suspend */
	u32 charging_suspend_threshold;
	/* battery capacity threshold for charging resume */
	u32 charging_resume_threshold;
};

int external_battery_register_svid_handler(struct ext_batt_pd *pd);
int external_battery_unregister_svid_handler(struct ext_batt_pd *pd);

int external_battery_send_vdm(struct ext_batt_pd *pd, u32 vdm_hdr, const u32 *vdos, int num_vdos);

#endif /* _EXT_BATT_H__ */
