/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _CHARGING_DOCK_H__
#define _CHARGING_DOCK_H__

#include <linux/cypd.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#define REQ_ACK_TIMEOUT_MS 200
#define MAX_BROADCAST_PERIOD_MIN 60

/* Vendor Defined Object Section */
#define PARAMETER_TYPE_UNKNOWN 0x0
#define PARAMETER_TYPE_FW_VERSION_NUMBER 0x01
#define PARAMETER_TYPE_SERIAL_NUMBER_MLB 0x02
#define PARAMETER_TYPE_BOARD_TEMPERATURE 0x04
#define PARAMETER_TYPE_BROADCAST_PERIOD 0x80
#define PARAMETER_TYPE_SERIAL_NUMBER_SYSTEM 0x81

struct charging_dock_params_t {
	u32 fw_version;
	char serial_number_mlb[16];
	u16 board_temp;
	char serial_number_system[16];
	/* Duration in mins at which charging dock should send broadcast VDM */
	u8 broadcast_period;
};

struct charging_dock_device_t {
	/* platform device handle */
	struct device *dev;
	/* pd protocol engine handle */
	struct cypd *cpd;
	/* client struct to register with usbpd engine */
	struct cypd_svid_handler vdm_handler;
	/* docked/undocked status */
	bool docked;
	/* lock for modifying device struct */
	struct mutex lock;
	struct charging_dock_params_t params;
	/* VDM response wait queue */
	wait_queue_head_t tx_waitq;
	/* work for sending broadcast period to dock */
	struct delayed_work dwork;
	/* VDM request parameter for which ack is received */
	u32 ack_parameter;
};

#endif /* _CHARGING_DOCK_H__ */
