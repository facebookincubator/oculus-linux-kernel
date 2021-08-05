// SPDX-License-Identifier: GPL-2.0

#ifndef _MOLOKINI_H__
#define _MOLOKINI_H__

#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/usb/usbpd.h>
#include <linux/workqueue.h>

/* Mount states */
enum molokini_fw_mount_state {
	MOLOKINI_FW_OFF_HEAD = 0,
	MOLOKINI_FW_ON_HEAD,
	MOLOKINI_FW_UNKNOWN,
};

/* Protocol types */
enum molokini_fw_protocol {
	MOLOKINI_FW_BROADCAST = 0,
	MOLOKINI_FW_REQUEST,
	MOLOKINI_FW_RESPONSE,
};

/* Parameter types */
#define MOLOKINI_FW_VERSION_NUMBER 0x01
#define MOLOKINI_FW_SERIAL 0x02
#define MOLOKINI_FW_SERIAL_BATTERY 0x03
#define MOLOKINI_FW_TEMP_BOARD 0x04
#define MOLOKINI_FW_TEMP_BATT 0x05
#define MOLOKINI_FW_TEMP_FG 0x06
#define MOLOKINI_FW_VOLTAGE 0x08
#define MOLOKINI_FW_BATT_STATUS 0x0A
#define MOLOKINI_FW_CURRENT 0x0C
#define MOLOKINI_FW_REMAINING_CAPACITY 0x10
#define MOLOKINI_FW_FCC 0x12
#define MOLOKINI_FW_CYCLE_COUNT 0x2A
#define MOLOKINI_FW_RSOC 0x2C
#define MOLOKINI_FW_SOH 0x2E
#define MOLOKINI_FW_DEVICE_NAME 0x4A
#define MOLOKINI_FW_LDB1 0x60
#define MOLOKINI_FW_LDB3 0x62
#define MOLOKINI_FW_LDB4 0x63
#define MOLOKINI_FW_LDB6 0x65 /* TEMP ZONE 0 */
#define MOLOKINI_FW_LDB7 0x66 /* TEMP ZONE 1 */
#define MOLOKINI_FW_LDB8 0x67 /* TEMP ZONE 2 */
#define MOLOKINI_FW_LDB9 0x68 /* TEMP ZONE 3 */
#define MOLOKINI_FW_LDB10 0x69 /* TEMP ZONE 4 */
#define MOLOKINI_FW_LDB11 0x6A /* TEMP ZONE 5 */
#define MOLOKINI_FW_LDB12 0x6B /* TEMP ZONE 6 */
#define MOLOKINI_FW_MANUFACTURER_INFO1 0x70
#define MOLOKINI_FW_MANUFACTURER_INFO2 0x7A
#define MOLOKINI_FW_MANUFACTURER_INFO3 0x7B
#define MOLOKINI_FW_HMD_MOUNTED 0x80
#define MOLOKINI_FW_CHARGER_PLUGGED 0x82

/* Vendor Defined Object Section */
#define VDOS_MAX_BYTES 16

#define LIFETIME_1_LOWER_LEN 6
#define LIFETIME_1_HIGHER_LEN 4
#define LIFETIME_4_LEN 6
#define NUM_TEMP_ZONE 7
#define TEMP_ZONE_LEN 8

/* molokini TI fuel gauge parameters */
struct molokini_parameters {
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

struct molokini_pd {
	/* platform device handle */
	struct device *dev;

	/* usbpd protocol engine handle */
	struct usbpd *upd;

	/* client struct to register with usbpd engine */
	struct usbpd_svid_handler vdm_handler;

	/* molokini connection status */
	bool connected;

	/* lock for modifying molokini_pd struct */
	struct mutex lock;
	/* molokini TI fuel gauge debugfs directory */
	struct dentry *debug_root;

	struct molokini_parameters params;

	/* mount state held locally, messaged as u32 vdo */
	u32 mount_state;
	/* last ACK received from Molokini for mount status */
	enum molokini_fw_mount_state last_mount_ack;
	/* work for periodically processing HMD mount state */
	struct delayed_work dwork;
	/* power supply object handle for internal HMD battery */
	struct power_supply *battery_psy;
	/* power supply object handle for USB power supply */
	struct power_supply	*usb_psy;
	/* notifier block for handling power supply change events */
	struct notifier_block nb;
	/* molokini on-demand charging suspend disable */
	bool charging_suspend_disable;
	/* battery capacity threshold for charging suspend */
	u32 charging_suspend_threshold;
	/* battery capacity threshold for charging resume */
	u32 charging_resume_threshold;
};

#endif /* _MOLOKINI_H__ */
