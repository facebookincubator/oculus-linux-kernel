/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _CHARGING_DOCK_H__
#define _CHARGING_DOCK_H__

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "usbvdm/subscriber.h"

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
#define PARAMETER_TYPE_REBOOT_INTO_BOOTLOADER 0xF0

#define VDO_LOG_TRANSMIT_STOP 0x00
#define VDO_LOG_TRANSMIT_START 0x01

#define NUM_CHARGING_DOCK_PORTS 4

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

/* Items reported by the dock */
struct charging_dock_params_t {
	u32 fw_version;
	char serial_number_mlb[16];
	u16 board_temp;
	char serial_number_system[16];
	size_t log_size;
	struct port_config_t port_config[NUM_CHARGING_DOCK_PORTS];
	int moisture_detected_count;
};

struct usbvdm_subscription_data {
	u16 vid;
	u16 pid;
	struct usbvdm_subscription *sub;

	struct list_head entry;
};

/**
 * struct charging_dock_device_t - structure for charging dock data
 *
 * @dev: platform device handle
 * @sub_list: list of USBVDM subscription handles
 * @current_svid: USB Standard or Vendor ID of connected dock
 * @current_pid: USB Product ID of connected dock
 * @docked: docked/undocked status
 * @broadcast_period: duration in mins at which charging dock should send broadcast VDM
 * @work: work for sending broadcast period to dock
 * @workqueue: workqueue for @work
 * @ack_parameter: VDM request parameter for which ack is received
 * @req_ack_timeout_ms: duration to wait for an ACK from the dock
 * @rx_complete: VDM response completion
 * @params: items reported by the dock
 * @log: buffer for dock log
 * @log_chunk_num: current log chunk being received
 * @gathering_log: flag for whether log is being received
 * @battery_psy: power supply object handle for internal HMD battery
 * @nb: notifier block for handling power supply change events
 * @system_battery_capacity: system-wide battery capacity (internal + external)
 * @send_state_of_charge: flag to tell if state of charge needs to be sent to dock
 * @state_of_charge: current system state of charge
 * @work_soc: work for sending state of charge to dock
 * @lock: lock for modifying device struct
 */
struct charging_dock_device_t {
	struct device *dev;
	struct list_head sub_list;

	u16 current_svid;
	u16 current_pid;
	bool docked;

	u8 broadcast_period;
	struct work_struct work;
	struct workqueue_struct	*workqueue;

	u32 ack_parameter;
	u32 req_ack_timeout_ms;
	struct completion rx_complete;
	struct charging_dock_params_t params;

	char *log;
	u32 log_chunk_num;
	bool gathering_log;

	struct power_supply *battery_psy;
	struct notifier_block nb;

	u8 system_battery_capacity;
	bool send_state_of_charge;
	enum state_of_charge_t state_of_charge;
	struct work_struct work_soc;

	struct mutex lock;
};

#endif /* _CHARGING_DOCK_H__ */
