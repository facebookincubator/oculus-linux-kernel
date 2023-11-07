/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _CHARGING_DOCK_H__
#define _CHARGING_DOCK_H__

#include <linux/cypd.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/usb/usbpd.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "vdm_glink.h"

#define REQ_ACK_TIMEOUT_MS 200
#define MAX_BROADCAST_PERIOD_MIN 60
#define MAX_LOG_SIZE 4096
#define MAX_VDO_SIZE 16

/* Vendor Defined Object Section */
#define PARAMETER_TYPE_UNKNOWN 0x0
#define PARAMETER_TYPE_FW_VERSION_NUMBER 0x01
#define PARAMETER_TYPE_SERIAL_NUMBER_MLB 0x02
#define PARAMETER_TYPE_BOARD_TEMPERATURE 0x04
#define PARAMETER_TYPE_BROADCAST_PERIOD 0x80
#define PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM 0x81
#define PARAMETER_TYPE_PORT_CONFIGURATION 0x84
#define PARAMETER_TYPE_CONNECTED_DEVICES 0x85
#define PARAMETER_TYPE_LOG_TRANSMIT 0x86
#define PARAMETER_TYPE_LOG_CHUNK 0x87
#define PARAMETER_TYPE_STATE_OF_CHARGE 0x88
#define PARAMETER_TYPE_MOISTURE_DETECTED 0xA0

#define VDO_LOG_TRANSMIT_STOP 0x00
#define VDO_LOG_TRANSMIT_START 0x01

#define NUM_CHARGING_DOCK_PORTS 4

enum charging_dock_intf_type {
	INTF_TYPE_UNKNOWN,
	INTF_TYPE_USBPD, // USB-PD
	INTF_TYPE_CYPD,	// CYPRESS-PD
	INTF_TYPE_GLINK, // QTI PMIC GLINK
};

enum port_state_t {
	NOT_CONNECTED = 0x00,
	TYPEC_900MA_SOURCE = 0x01,
	TYPEC_1500MA_SOURCE = 0x02,
	TYPEC_3000MA_SOURCE = 0x03,
	USBPD_SOURCE = 0x04,
	TYPEC_900MA_SINK = 0x11,
	TYPEC_1500MA_SINK = 0x12,
	TYPEC_3000MA_SINK = 0x13,
	USB_PD_SINK = 0x14
};

struct port_config_t {
	enum port_state_t state;
	u16 vid;
	u16 pid;
	int voltage_mv;
	int current_ma;
	u32 fw_version;
	char serial_number_mlb[16];
	char serial_number_system[16];
};

enum state_of_charge_t {
	CHARGING,
	CHARGED,
	NOT_CHARGING
};

struct charging_dock_params_t {
	u32 fw_version;
	char serial_number_mlb[16];
	u16 board_temp;
	char serial_number_system[16];
	/* Duration in mins at which charging dock should send broadcast VDM */
	u8 broadcast_period;
	size_t log_size;
	struct port_config_t port_config[NUM_CHARGING_DOCK_PORTS];
	int moisture_detected_count;
	enum state_of_charge_t state_of_charge;
};

struct charging_dock_device_t {
	/* platform device handle */
	struct device *dev;
	/* usbpd protocol engine handle */
	struct usbpd *upd;
	/* cypd protocol engine handle */
	struct cypd *cpd;
	/* glink protocol engine handle */
	struct vdm_glink_dev *gpd;
	/* client struct to register with usbpd engine */
	struct usbpd_svid_handler usbpd_vdm_handler;
	/* client struct to register with cypd engine */
	struct cypd_svid_handler cypd_vdm_handler;
	/* client struct to register with cypd engine (alternate) */
	struct cypd_svid_handler cypd_vdm_handler_alt;
	/* list of client structs to register with glink engine */
	struct list_head glink_handlers;
	/* docked/undocked status */
	bool docked;
	/* lock for modifying device struct */
	struct mutex lock;
	struct charging_dock_params_t params;
	/* VDM response wait queue */
	wait_queue_head_t tx_waitq;
	/* work for sending broadcast period to dock */
	struct work_struct work;
	/* VDM request parameter for which ack is received */
	u32 ack_parameter;
	/* Interface type as driver can support multiple interfaces */
	enum charging_dock_intf_type intf_type;
	char *log;
	u32 log_chunk_num;
	bool gathering_log;
	/* power supply object handle for internal HMD battery */
	struct power_supply *battery_psy;
	/* notifier block for handling power supply change events */
	struct notifier_block nb;
	u16 current_svid;
	/* work for sending state of charge to dock */
	struct work_struct work_soc;
	/* flag to tell if state of charge needs to be sent to dock*/
	bool send_state_of_charge;
	/* system-wide battery capacity (internal + external) */
	u8 system_battery_capacity;
};

#endif /* _CHARGING_DOCK_H__ */
