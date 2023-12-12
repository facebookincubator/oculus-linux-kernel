// SPDX-License-Identifier: GPL-2.0

#ifndef _EXT_BATT_H__
#define _EXT_BATT_H__

#include <linux/iio/consumer.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/usb/usbpd.h>
#include <linux/workqueue.h>

#include "vdm_glink.h"

/* Mount states */
enum ext_batt_fw_mount_state {
	EXT_BATT_FW_OFF_HEAD = 0,
	EXT_BATT_FW_ON_HEAD,
	EXT_BATT_FW_UNKNOWN,
};

/* Battery identifier */
enum ext_batt_id {
	EXT_BATT_ID_MOLOKINI = 0,
	EXT_BATT_ID_LEHUA,
};

enum ext_batt_fw_dock_state {
	EXT_BATT_FW_UNDOCKED = 0,
	EXT_BATT_FW_DOCKED,
	EXT_BATT_FW_DOCK_STATE_UNKNOWN,
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
#define EXT_BATT_FW_PACK_ASSEMBLY_PN 0x20
#define EXT_BATT_FW_MANUFACTURER_INFOB 0x23
#define EXT_BATT_FW_CYCLE_COUNT 0x2A
#define EXT_BATT_FW_RSOC 0x2C
#define EXT_BATT_FW_SOH 0x2E
#define EXT_BATT_FW_DEVICE_NAME 0x4A
#define EXT_BATT_FW_LDB1 0x60
#define EXT_BATT_FW_LDB2 0x61
#define EXT_BATT_FW_LDB3 0x62 /* L+ TEMP ZONES 0-1 */
#define EXT_BATT_FW_LDB4 0x63 /* L+ TEMP ZONES 2-3 */
#define EXT_BATT_FW_LDB5 0x64 /* L+ TEMP ZONES 4-5 */
#define EXT_BATT_FW_LDB6 0x65 /* M+ TEMP ZONE 0 / L+ Temp ZONE 6 */
#define EXT_BATT_FW_LDB7 0x66 /* M+ TEMP ZONE 1 */
#define EXT_BATT_FW_LDB8 0x67 /* M+ TEMP ZONE 2 */
#define EXT_BATT_FW_LDB9 0x68 /* M+ TEMP ZONE 3 */
#define EXT_BATT_FW_LDB10 0x69 /* M+ TEMP ZONE 4 */
#define EXT_BATT_FW_LDB11 0x6A /* M+ TEMP ZONE 5 */
#define EXT_BATT_FW_LDB12 0x6B /* M+ TEMP ZONE 6 */
#define EXT_BATT_FW_MANUFACTURER_INFO1 0x70
#define EXT_BATT_FW_MANUFACTURER_INFOA 0x70
#define EXT_BATT_FW_MANUFACTURER_INFO2 0x7A
#define EXT_BATT_FW_MANUFACTURER_INFO3 0x7B
#define EXT_BATT_FW_HMD_MOUNTED 0x80
#define EXT_BATT_FW_SERIAL_SYSTEM 0x81
#define EXT_BATT_FW_CHARGER_PLUGGED 0x82
#define EXT_BATT_FW_HMD_DOCKED 0x84

/* Error conditions reported to HMD */
#define EXT_BATT_FW_ERROR_CONDITIONS 10

#define EXT_BATT_FW_ERROR_UFP_LPD 0xA0
#define EXT_BATT_FW_ERROR_UFP_OCP 0xA1
#define EXT_BATT_FW_ERROR_DRP_OCP 0xA2
#define EXT_BATT_FW_ERROR_UFP_OTP 0xA3
#define EXT_BATT_FW_ERROR_DRP_OTP 0xA4
#define EXT_BATT_FW_ERROR_BATT_OTP 0xA5
#define EXT_BATT_FW_ERROR_PCM_OTP 0xA6
#define EXT_BATT_FW_ERROR_DRP_SCP 0xA7
#define EXT_BATT_FW_ERROR_UFP_OVP 0xA8
#define EXT_BATT_FW_ERROR_DRP_OVP 0xA9

/* Vendor Defined Object Section */
#define NUM_TEMP_ZONE 7
#define TEMP_ZONE_LEN 8
#define MAX_BYTES_PER_VDM 16
#define MAX_BYTES_PER_LDB 32
#define MAX_VDOS_PER_VDM 4

/* Each LDB is 32-bytes max, 16-bytes per uVDM */
struct ldb_values {
	u32 lower[MAX_VDOS_PER_VDM];
	u32 higher[MAX_VDOS_PER_VDM];
};

/* LDB 1 */
struct lifetime1_molokini {
	u16 cell_1_max_voltage;
	u16 cell_1_min_voltage;
	u16 max_charge_current;
	u16 max_discharge_current;
	u16 max_avg_discharge_current;
	u16 max_avg_dsg_power;
	u8 max_temp_cell;
	u8 min_temp_cell;
	u8 max_temp_int_sensor;
	u8 min_temp_int_sensor;
};

struct lifetime1_lehua {
	u16 max_voltage_cell_1;
	u16 max_voltage_cell_2;
	u16 max_voltage_cell_3;
	u16 max_voltage_cell_4;
	u16 min_voltage_cell_1;
	u16 min_voltage_cell_2;
	u16 min_voltage_cell_3;
	u16 min_voltage_cell_4;
	u16 max_delta_cell_voltage;
	u16 max_charge_current;
	u16 max_discharge_current;
	u16 max_avg_discharge_current;
	u16 max_avg_dsg_power;
	u8 max_temp_cell;
	u8 min_temp_cell;
	u8 max_temp_delta;
	u8 reserved0;
	u8 reserved1;
	u8 max_temp_fet;
};

union lifetime1_data_block {
	struct ldb_values values;
	struct lifetime1_molokini ldb_molokini;
	struct lifetime1_lehua ldb_lehua;
};

/* LDB 2 */
struct lifetime2_lehua {
	u8 num_shutdowns;
	u8 avg_temp_cell;
	u8 min_temp_cell_a;
	u8 min_temp_cell_b;
	u8 min_temp_cell_c;
	u8 max_temp_cell_a;
	u8 max_temp_cell_b;
	u8 max_temp_cell_c;
	u16 time_spent_ut;
	u16 time_spent_lt;
	u16 time_spent_stl;
	u16 time_spent_rt;
	u16 time_spent_sth;
	u16 time_spent_ht;
	u16 time_spent_ot;
	u16 time_spent_hvlt;
	u16 time_spent_hvmt;
	u16 time_spent_hvht;
	u16 total_fw_runtime;
	u16 using_time_pf;
};

union lifetime2_data_block {
	struct ldb_values values;
	struct lifetime2_lehua ldb_lehua;
};

/* LDB 4 */
struct lifetime4_molokini {
	u16 num_valid_charge_terminations;
	u16 last_valid_charge_term;
	u16 num_qmax_updates;
	u16 last_qmax_update;
	u16 num_ra_update;
	u16 last_ra_update;
};

union lifetime4_data_block {
	struct ldb_values values;
	struct lifetime4_molokini ldb_molokini;
};

union temperature_zones {
	u32 tz_molokini[NUM_TEMP_ZONE][TEMP_ZONE_LEN];
	u16 tz_lehua[NUM_TEMP_ZONE][TEMP_ZONE_LEN];
};

/* LDB 6 */
struct lifetime6_lehua {
	u16 cb_time_cell_1;
	u16 cb_time_cell_2;
	u16 cb_time_cell_3;
	u16 cb_time_cell_4;
};

union lifetime6_data_block {
	u8 values[MAX_BYTES_PER_VDM];
	struct lifetime6_lehua ldb_lehua;
};

/* LDB 7 */
struct lifetime7_lehua {
	u16 num_cov_events;
	u16 last_cov_event;
	u16 num_cuv_events;
	u16 last_cuv_event;
	u16 num_ocd_1_events;
	u16 last_ocd_1_event;
	u16 num_ocd_2_events;
	u16 last_ocd_2_event;
	u16 num_occ_1_events;
	u16 last_occ_1_event;
	u16 num_occ_2_events;
	u16 last_occ_2_event;
	u16 num_aold_events;
	u16 last_aold_event;
	u16 num_ascd_events;
	u16 last_ascd_event;
};

union lifetime7_data_block {
	struct ldb_values values;
	struct lifetime7_lehua ldb_lehua;
};

/* LDB 8 */
struct lifetime8_lehua {
	u16 num_ascc_events;
	u16 last_ascc_event;
	u16 num_otc_events;
	u16 last_otc_event;
	u16 num_otd_events;
	u16 last_otd_event;
	u16 num_otf_events;
	u16 last_otf_event;
	u16 num_valid_charge_terminations;
	u16 last_valid_charge_term;
};

union lifetime8_data_block {
	struct ldb_values values;
	struct lifetime8_lehua ldb_lehua;
};

union manufacturer_info_data_block {
	struct ldb_values values;
	char data[MAX_BYTES_PER_LDB+1]; /* holds string representation */
};

/* ext_batt TI fuel gauge parameters */
struct ext_batt_parameters {
	/* Manufacturing serial parameters */
	char serial[16];
	char serial_battery[16];
	char serial_system[16];

	/* Standard parameters */
	u16 temp_fg;
	u16 voltage;
	char battery_status[16];
	u16 batt_status;
	u16 icurrent; /* negative range */
	u16 remaining_capacity;
	u16 fcc;
	u16 cycle_count;
	u8 rsoc;
	u8 rsoc_test;
	bool rsoc_test_enabled;
	u8 soh;
	u32 fw_version;
	char device_name[16];
	bool charger_plugged;
	char pack_assembly_pn[12];

	/* Lifetime data blocks */
	union lifetime1_data_block lifetime1;
	union lifetime2_data_block lifetime2;
	u32 lifetime3;
	union lifetime4_data_block lifetime4;
	union temperature_zones temp_zones;
	union lifetime6_data_block lifetime6;
	union lifetime7_data_block lifetime7;
	union lifetime8_data_block lifetime8;

	/* Manufacturer info */
	union manufacturer_info_data_block manufacturer_info_a;
	union manufacturer_info_data_block manufacturer_info_b;
	union manufacturer_info_data_block manufacturer_info_c;

	/* Error conditions/states reported to HMD */
	u8 error_conditions[EXT_BATT_FW_ERROR_CONDITIONS];
};

struct ext_batt_pd {
	/* platform device handle */
	struct device *dev;

	/* Vendor ID, needed to construct VDM messages */
	u16 svid;

	/* usbpd interface instance */
	struct usbpd *intf_usbpd;
	/* glink interface instance */
	struct vdm_glink_dev *intf_glink;

	/* handler to register with usbpd interface */
	struct usbpd_svid_handler usbpd_hdlr;
	/* list of handlers to register with glink interface */
	struct list_head glink_handlers;

	/* ext_batt connection status */
	bool connected;

	/* lock for modifying ext_batt_pd struct */
	struct mutex lock;
	/* ext_batt TI fuel gauge debugfs directory */
	struct dentry *debug_root;

	struct ext_batt_parameters params;

	/* mount state held locally, messaged as u32 vdo */
	u32 mount_state;
	/* signals that a mount state update was acked */
	struct completion mount_state_ack;
	/* work for periodically processing HMD mount state */
	struct work_struct mount_state_work;
	/* 0/1 for undocked/docked */
	int dock_state;
	/* last ACK received for dock state */
	int last_dock_ack;
	/* work for periodically processing HMD dock state */
	struct work_struct dock_state_work;
	/* work for handling the power_supply notifier callback logic */
	struct work_struct psy_notifier_work;
	/* power supply object handle for internal HMD battery */
	struct power_supply *battery_psy;
	/* power supply object handle for USB power supply */
	struct power_supply	*usb_psy;
	/* power supply object handle for CYPD power supply */
	struct power_supply	*cypd_psy;
	/* notifier block for handling power supply change events */
	struct notifier_block nb;
	/* iio channel to retrieve dock state from CYPD */
	struct iio_channel	*cypd_pd_active_chan;
	/* ext_batt on-demand charging suspend disable */
	bool charging_suspend_disable;
	/* battery capacity threshold for charging suspend */
	u32 charging_suspend_threshold;
	/* battery capacity threshold for charging resume */
	u32 charging_resume_threshold;
	/* USB power supply charging state, 0/1 for resume/suspend */
	int usb_psy_charging_state;
	/* identifier to distinguish between battery packs */
	enum ext_batt_id batt_id;
	/* RSOC Scaling Settings */
	bool rsoc_scaling_enabled;
	u32 rsoc_scaling_min_level;
	u32 rsoc_scaling_max_level;
	/* SRC Current Control */
	bool src_current_control_enabled;
	u32 src_enable_soc_threshold;
	u32 src_current_limit_max_uA;
	u32 source_current;
	/* first batch of broadcast data from battery pack */
	bool first_broadcast_data_received;
	/* flag for PR_SWAP */
	bool recently_docked;
};

int external_battery_register_svid_handlers(struct ext_batt_pd *pd);
void external_battery_unregister_svid_handlers(struct ext_batt_pd *pd);

int external_battery_send_vdm(struct ext_batt_pd *pd, u32 vdm_hdr, const u32 *vdos, int num_vdos);

void ext_batt_vdm_connect(struct ext_batt_pd *pd, bool usb_comm);
void ext_batt_vdm_disconnect(struct ext_batt_pd *pd);
void ext_batt_vdm_received(struct ext_batt_pd *pd,
		u32 vdm_hdr, const u32 *vdos, int num_vdos);

#endif /* _EXT_BATT_H__ */
