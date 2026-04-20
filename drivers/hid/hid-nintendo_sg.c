// SPDX-License-Identifier: GPL-2.0+
/*
 * HID driver for Nintendo Switch Joy-Cons and Pro Controllers
 *
 * Copyright (c) 2019 Daniel J. Ogorchock <djogorchock@gmail.com>
 *
 * The following resources/projects were referenced for this driver:
 *   https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
 *   https://gitlab.com/pjranki/joycon-linux-kernel (Peter Rankin)
 *   https://github.com/FrotBot/SwitchProConLinuxUSB
 *   https://github.com/MTCKC/ProconXInput
 *   hid-wiimote kernel hid driver
 *   hid-logitech-hidpp driver
 *
 * This driver supports the Nintendo Switch Joy-Cons and Pro Controllers. The
 * Pro Controllers can either be used over USB or Bluetooth.
 *
 * The driver will retrieve the factory calibration info from the controllers,
 * so little to no user calibration should be required.
 *
 */

#include "hid-ids.h"
#include <asm/unaligned.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

/*
 * Reference the url below for the following HID report defines:
 * https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
 */

#define JOYCON_MAX_DEVICES 4
#define JOYCON_IMU_PACKETS 3

/* Output Reports */
static const u8 JC_OUTPUT_RUMBLE_AND_SUBCMD	= 0x01;
static const u8 JC_OUTPUT_FW_UPDATE_PKT		= 0x03;
static const u8 JC_OUTPUT_RUMBLE_ONLY		= 0x10;
static const u8 JC_OUTPUT_MCU_DATA		= 0x11;
static const u8 JC_OUTPUT_USB_CMD		= 0x80;

/* Subcommand IDs */
static const u8 JC_SUBCMD_STATE			/*= 0x00*/;
static const u8 JC_SUBCMD_MANUAL_BT_PAIRING	= 0x01;
static const u8 JC_SUBCMD_REQ_DEV_INFO		= 0x02;
static const u8 JC_SUBCMD_SET_REPORT_MODE	= 0x03;
static const u8 JC_SUBCMD_TRIGGERS_ELAPSED	= 0x04;
static const u8 JC_SUBCMD_GET_PAGE_LIST_STATE	= 0x05;
static const u8 JC_SUBCMD_SET_HCI_STATE		= 0x06;
static const u8 JC_SUBCMD_RESET_PAIRING_INFO	= 0x07;
static const u8 JC_SUBCMD_LOW_POWER_MODE	= 0x08;
static const u8 JC_SUBCMD_SPI_FLASH_READ	= 0x10;
static const u8 JC_SUBCMD_SPI_FLASH_WRITE	= 0x11;
static const u8 JC_SUBCMD_RESET_MCU		= 0x20;
static const u8 JC_SUBCMD_SET_MCU_CONFIG	= 0x21;
static const u8 JC_SUBCMD_SET_MCU_STATE		= 0x22;
static const u8 JC_SUBCMD_SET_PLAYER_LIGHTS	= 0x30;
static const u8 JC_SUBCMD_GET_PLAYER_LIGHTS	= 0x31;
static const u8 JC_SUBCMD_SET_HOME_LIGHT	= 0x38;
static const u8 JC_SUBCMD_ENABLE_IMU		= 0x40;
static const u8 JC_SUBCMD_SET_IMU_SENSITIVITY	= 0x41;
static const u8 JC_SUBCMD_WRITE_IMU_REG		= 0x42;
static const u8 JC_SUBCMD_READ_IMU_REG		= 0x43;
static const u8 JC_SUBCMD_ENABLE_VIBRATION	= 0x48;
static const u8 JC_SUBCMD_GET_REGULATED_VOLTAGE	= 0x50;

/* Input Reports */
static const u8 JC_INPUT_BUTTON_EVENT		= 0x3F;
static const u8 JC_INPUT_SUBCMD_REPLY		= 0x21;
static const u8 JC_INPUT_IMU_DATA		= 0x30;
static const u8 JC_INPUT_MCU_DATA		= 0x31;
static const u8 JC_INPUT_USB_RESPONSE		= 0x81;

/* Feature Reports */
static const u8 JC_FEATURE_LAST_SUBCMD		= 0x02;
static const u8 JC_FEATURE_OTA_FW_UPGRADE	= 0x70;
static const u8 JC_FEATURE_SETUP_MEM_READ	= 0x71;
static const u8 JC_FEATURE_MEM_READ		= 0x72;
static const u8 JC_FEATURE_ERASE_MEM_SECTOR	= 0x73;
static const u8 JC_FEATURE_MEM_WRITE		= 0x74;
static const u8 JC_FEATURE_LAUNCH		= 0x75;

/* USB Commands */
static const u8 JC_USB_CMD_CONN_STATUS		= 0x01;
static const u8 JC_USB_CMD_HANDSHAKE		= 0x02;
static const u8 JC_USB_CMD_BAUDRATE_3M		= 0x03;
static const u8 JC_USB_CMD_NO_TIMEOUT		= 0x04;
static const u8 JC_USB_CMD_EN_TIMEOUT		= 0x05;
static const u8 JC_USB_RESET			= 0x06;
static const u8 JC_USB_PRE_HANDSHAKE		= 0x91;
static const u8 JC_USB_SEND_UART		= 0x92;

/* Magic value denoting presence of user calibration */
#define JC_CAL_USR_MAGIC_0		 0xB2
#define JC_CAL_USR_MAGIC_1		 0xA1
#define JC_CAL_USR_MAGIC_SIZE		 2

/* SPI storage addresses of factory calibration data */
static const u16 JC_CAL_DATA_START		= 0x603d;
static const u16 JC_CAL_DATA_END		= 0x604e;
#define JC_CAL_DATA_SIZE	(JC_CAL_DATA_END - JC_CAL_DATA_START + 1)

/* SPI storage addresses of IMU factory calibration data */
#define JC_IMU_CAL_FCT_DATA_ADDR	 0x6020
#define JC_IMU_CAL_FCT_DATA_END	 0x6037
#define JC_IMU_CAL_DATA_SIZE \
	(JC_IMU_CAL_FCT_DATA_END - JC_IMU_CAL_FCT_DATA_ADDR + 1)
/* SPI storage addresses of IMU user calibration data */
#define JC_IMU_CAL_USR_MAGIC_ADDR	 0x8026
#define JC_IMU_CAL_USR_DATA_ADDR	 0x8028

/* The raw analog joystick values will be mapped in terms of this magnitude */
static const u16 JC_MAX_STICK_MAG		= 32767;
static const u16 JC_STICK_FUZZ			= 250;
static const u16 JC_STICK_FLAT			= 500;

#define JC_IMU_PREC_RANGE_SCALE	1000

/* IMU mode constants */
static const u8 JC_IMU_MODE_ENABLE              = 0x01;
static const u8 JC_IMU_MODE_DISABLE             = 0x00;

/* States for controller state machine */
enum joycon_ctlr_state {
	JOYCON_CTLR_STATE_INIT,
	JOYCON_CTLR_STATE_READ,
};

struct joycon_stick_cal {
	s32 max;
	s32 min;
	s32 center;
};

struct joycon_imu_cal {
	s16 offset[3];
	s16 scale[3];
};

/*
 * All the controller's button values are stored in a u32.
 * They can be accessed with bitwise ANDs.
 */
static const u32 JC_BTN_Y	= BIT(0);
static const u32 JC_BTN_X	= BIT(1);
static const u32 JC_BTN_B	= BIT(2);
static const u32 JC_BTN_A	= BIT(3);
static const u32 JC_BTN_SR_R	= BIT(4);
static const u32 JC_BTN_SL_R	= BIT(5);
static const u32 JC_BTN_R	= BIT(6);
static const u32 JC_BTN_ZR	= BIT(7);
static const u32 JC_BTN_MINUS	= BIT(8);
static const u32 JC_BTN_PLUS	= BIT(9);
static const u32 JC_BTN_RSTICK	= BIT(10);
static const u32 JC_BTN_LSTICK	= BIT(11);
static const u32 JC_BTN_HOME	= BIT(12);
static const u32 JC_BTN_CAP	= BIT(13); /* capture button */
static const u32 JC_BTN_DOWN	= BIT(16);
static const u32 JC_BTN_UP	= BIT(17);
static const u32 JC_BTN_RIGHT	= BIT(18);
static const u32 JC_BTN_LEFT	= BIT(19);
static const u32 JC_BTN_SR_L	= BIT(20);
static const u32 JC_BTN_SL_L	= BIT(21);
static const u32 JC_BTN_L	= BIT(22);
static const u32 JC_BTN_ZL	= BIT(23);

enum joycon_msg_type {
	JOYCON_MSG_TYPE_NONE,
	JOYCON_MSG_TYPE_USB,
	JOYCON_MSG_TYPE_SUBCMD,
};

struct joycon_subcmd_request {
	u8 output_id; /* must be 0x01 for subcommand, 0x10 for rumble only */
	u8 packet_num; /* incremented every send */
	u8 rumble_data[8];
	u8 subcmd_id;
	u8 data[0]; /* length depends on the subcommand */
} __packed;

struct joycon_subcmd_reply {
	u8 ack; /* MSB 1 for ACK, 0 for NACK */
	u8 id; /* id of requested subcmd */
	u8 data[0]; /* will be at most 35 bytes */
} __packed;

struct joycon_imu_msg {
	u8 accel_x[2];
	u8 accel_y[2];
	u8 accel_z[2];
	u8 gyro_x[2];
	u8 gyro_y[2];
	u8 gyro_z[2];
} __packed;

struct joycon_input_report {
	u8 id;
	u8 timer;
	u8 bat_con; /* battery and connection info */
	u8 button_status[3];
	u8 left_stick[3];
	u8 right_stick[3];
	u8 vibrator_report;
	union {
		struct joycon_subcmd_reply reply;
		struct joycon_imu_msg imu[JOYCON_IMU_PACKETS];
	} extra;
} __packed;

struct joycon_imu_data {
	int32_t accel[3];
	int32_t gyro[3];
} __packed;

struct joycon_msg_data {
	u32 buttons;
	struct joycon_imu_data imu[JOYCON_IMU_PACKETS];
	u8 bat_con;
} __packed;

#define JC_MAX_RESP_SIZE	(sizeof(struct joycon_input_report) + 35)

#define MAX_JOYCON_CONNECTIONS 4

struct joycon_module {
	struct class *class;
	struct cdev cdev;
	atomic_t ctlr_count;
	dev_t devno;
	struct device *dev;
	struct joycon_ctlr *ctlr[MAX_JOYCON_CONNECTIONS];
	struct mutex mutex;
	bool busy;
};

/* Each physical controller is associated with a joycon_ctlr struct */
struct joycon_ctlr {
	struct hid_device *hdev;
	struct input_dev *input;
	enum joycon_ctlr_state ctlr_state;

	struct mutex read_mutex;
	unsigned int index;
	struct joycon_msg_data data;
	wait_queue_head_t wq;
	bool data_dirty;

	/* The following members are used for synchronous sends/receives */
	enum joycon_msg_type msg_type;
	u8 subcmd_num;
	struct mutex output_mutex;
	u8 input_buf[JC_MAX_RESP_SIZE];
	wait_queue_head_t wait;
	bool received_resp;
	u8 usb_ack_match;
	u8 subcmd_ack_match;

	/* factory calibration data */
	struct joycon_stick_cal left_stick_cal_x;
	struct joycon_stick_cal left_stick_cal_y;
	struct joycon_stick_cal right_stick_cal_x;
	struct joycon_stick_cal right_stick_cal_y;

	struct joycon_imu_cal accel_cal;
	struct joycon_imu_cal gyro_cal;

	/* prevents needlessly recalculating these divisors every sample */
	s32 imu_cal_accel_divisor[3];
	s32 imu_cal_gyro_divisor[3];
};

static struct joycon_module driver;

static int __joycon_hid_send(struct hid_device *hdev, u8 *data, size_t len)
{
	u8 *buf;
	int ret;

	buf = kmemdup(data, len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	ret = hid_hw_output_report(hdev, buf, len);
	kfree(buf);
	if (ret < 0)
		hid_dbg(hdev, "Failed to send output report ret=%d\n", ret);
	return ret;
}

static int joycon_hid_send_sync(struct joycon_ctlr *ctlr, u8 *data, size_t len)
{
	int ret;

	ret = __joycon_hid_send(ctlr->hdev, data, len);
	if (ret < 0) {
		memset(ctlr->input_buf, 0, JC_MAX_RESP_SIZE);
		return ret;
	}

	if (!wait_event_timeout(ctlr->wait, ctlr->received_resp, HZ)) {
		hid_dbg(ctlr->hdev, "synchronous send/receive timed out\n");
		memset(ctlr->input_buf, 0, JC_MAX_RESP_SIZE);
		return -ETIMEDOUT;
	}

	ctlr->received_resp = false;
	return 0;
}

static int joycon_send_usb(struct joycon_ctlr *ctlr, u8 cmd)
{
	int ret;
	u8 buf[2] = {JC_OUTPUT_USB_CMD};

	buf[1] = cmd;
	ctlr->usb_ack_match = cmd;
	ctlr->msg_type = JOYCON_MSG_TYPE_USB;
	ret = joycon_hid_send_sync(ctlr, buf, sizeof(buf));
	if (ret)
		hid_dbg(ctlr->hdev, "send usb command failed; ret=%d\n", ret);
	return ret;
}

static int joycon_send_subcmd(struct joycon_ctlr *ctlr,
			      struct joycon_subcmd_request *subcmd,
			      size_t data_len)
{
	int ret;

	subcmd->output_id = JC_OUTPUT_RUMBLE_AND_SUBCMD;
	subcmd->packet_num = ctlr->subcmd_num;
	if (++ctlr->subcmd_num > 0xF)
		ctlr->subcmd_num = 0;
	ctlr->subcmd_ack_match = subcmd->subcmd_id;
	ctlr->msg_type = JOYCON_MSG_TYPE_SUBCMD;

	ret = joycon_hid_send_sync(ctlr, (u8 *)subcmd,
				   sizeof(*subcmd) + data_len);
	if (ret < 0)
		hid_dbg(ctlr->hdev, "send subcommand failed; ret=%d\n", ret);
	else
		ret = 0;
	return ret;
}

/* Supply nibbles for flash and on. Ones correspond to active */
static int joycon_set_player_leds(struct joycon_ctlr *ctlr, u8 flash, u8 on)
{
	struct joycon_subcmd_request *req;
	u8 buffer[sizeof(*req) + 1] = { 0 };

	req = (struct joycon_subcmd_request *)buffer;
	req->subcmd_id = JC_SUBCMD_SET_PLAYER_LIGHTS;
	req->data[0] = (flash << 4) | on;

	hid_dbg(ctlr->hdev, "setting player leds\n");
	return joycon_send_subcmd(ctlr, req, 1);
}

static int joycon_request_spi_flash_read(struct joycon_ctlr *ctlr,
					 u32 start_addr, u8 size, u8 **reply)
{
	struct joycon_subcmd_request *req;
	struct joycon_input_report *report;
	u8 buffer[sizeof(*req) + 5] = { 0 };
	u8 *data;
	int ret;

	if (!reply)
		return -EINVAL;

	req = (struct joycon_subcmd_request *)buffer;
	req->subcmd_id = JC_SUBCMD_SPI_FLASH_READ;
	data = req->data;
	put_unaligned_le32(start_addr, data);
	data[4] = size;

	hid_dbg(ctlr->hdev, "requesting SPI flash data\n");
	ret = joycon_send_subcmd(ctlr, req, 5);
	if (ret) {
		hid_err(ctlr->hdev, "failed reading SPI flash; ret=%d\n", ret);
	} else {
		report = (struct joycon_input_report *)ctlr->input_buf;
		/* The read data starts at the 6th byte */
		*reply = &report->extra.reply.data[5];
	}
	return ret;
}

/*
 * User calibration's presence is denoted with a magic byte preceding it.
 * returns 0 if magic val is present, 1 if not present, < 0 on error
 */
static int joycon_check_for_cal_magic(struct joycon_ctlr *ctlr, u32 flash_addr)
{
	int ret;
	u8 *reply;

	ret = joycon_request_spi_flash_read(ctlr, flash_addr,
					    JC_CAL_USR_MAGIC_SIZE, &reply);
	if (ret)
		return ret;

	return reply[0] != JC_CAL_USR_MAGIC_0 || reply[1] != JC_CAL_USR_MAGIC_1;
}

static const u16 DFLT_STICK_CAL_CEN = 2000;
static const u16 DFLT_STICK_CAL_MAX = 3500;
static const u16 DFLT_STICK_CAL_MIN = 500;
static int joycon_request_calibration(struct joycon_ctlr *ctlr)
{
	struct joycon_subcmd_request *req;
	u8 buffer[sizeof(*req) + 5] = { 0 };
	struct joycon_input_report *report;
	struct joycon_stick_cal *cal_x;
	struct joycon_stick_cal *cal_y;
	s32 x_max_above;
	s32 x_min_below;
	s32 y_max_above;
	s32 y_min_below;
	u8 *data;
	u8 *raw_cal;
	int ret;

	req = (struct joycon_subcmd_request *)buffer;
	req->subcmd_id = JC_SUBCMD_SPI_FLASH_READ;
	data = req->data;
	data[0] = 0xFF & JC_CAL_DATA_START;
	data[1] = 0xFF & (JC_CAL_DATA_START >> 8);
	data[2] = 0xFF & (JC_CAL_DATA_START >> 16);
	data[3] = 0xFF & (JC_CAL_DATA_START >> 24);
	data[4] = JC_CAL_DATA_SIZE;

	hid_dbg(ctlr->hdev, "requesting cal data\n");
	ret = joycon_send_subcmd(ctlr, req, 5);
	if (ret) {
		hid_warn(ctlr->hdev,
			 "Failed to read stick cal, using defaults; ret=%d\n",
			 ret);

		ctlr->left_stick_cal_x.center = DFLT_STICK_CAL_CEN;
		ctlr->left_stick_cal_x.max = DFLT_STICK_CAL_MAX;
		ctlr->left_stick_cal_x.min = DFLT_STICK_CAL_MIN;

		ctlr->left_stick_cal_y.center = DFLT_STICK_CAL_CEN;
		ctlr->left_stick_cal_y.max = DFLT_STICK_CAL_MAX;
		ctlr->left_stick_cal_y.min = DFLT_STICK_CAL_MIN;

		ctlr->right_stick_cal_x.center = DFLT_STICK_CAL_CEN;
		ctlr->right_stick_cal_x.max = DFLT_STICK_CAL_MAX;
		ctlr->right_stick_cal_x.min = DFLT_STICK_CAL_MIN;

		ctlr->right_stick_cal_y.center = DFLT_STICK_CAL_CEN;
		ctlr->right_stick_cal_y.max = DFLT_STICK_CAL_MAX;
		ctlr->right_stick_cal_y.min = DFLT_STICK_CAL_MIN;

		return ret;
	}

	report = (struct joycon_input_report *)ctlr->input_buf;
	raw_cal = &report->extra.reply.data[5];

	/* left stick calibration parsing */
	cal_x = &ctlr->left_stick_cal_x;
	cal_y = &ctlr->left_stick_cal_y;

	x_max_above = hid_field_extract(ctlr->hdev, (raw_cal + 0), 0, 12);
	y_max_above = hid_field_extract(ctlr->hdev, (raw_cal + 1), 4, 12);
	cal_x->center = hid_field_extract(ctlr->hdev, (raw_cal + 3), 0, 12);
	cal_y->center = hid_field_extract(ctlr->hdev, (raw_cal + 4), 4, 12);
	x_min_below = hid_field_extract(ctlr->hdev, (raw_cal + 6), 0, 12);
	y_min_below = hid_field_extract(ctlr->hdev, (raw_cal + 7), 4, 12);
	cal_x->max = cal_x->center + x_max_above;
	cal_x->min = cal_x->center - x_min_below;
	cal_y->max = cal_y->center + y_max_above;
	cal_y->min = cal_y->center - y_min_below;

	/* right stick calibration parsing */
	raw_cal += 9;
	cal_x = &ctlr->right_stick_cal_x;
	cal_y = &ctlr->right_stick_cal_y;

	cal_x->center = hid_field_extract(ctlr->hdev, (raw_cal + 0), 0, 12);
	cal_y->center = hid_field_extract(ctlr->hdev, (raw_cal + 1), 4, 12);
	x_min_below = hid_field_extract(ctlr->hdev, (raw_cal + 3), 0, 12);
	y_min_below = hid_field_extract(ctlr->hdev, (raw_cal + 4), 4, 12);
	x_max_above = hid_field_extract(ctlr->hdev, (raw_cal + 6), 0, 12);
	y_max_above = hid_field_extract(ctlr->hdev, (raw_cal + 7), 4, 12);
	cal_x->max = cal_x->center + x_max_above;
	cal_x->min = cal_x->center - x_min_below;
	cal_y->max = cal_y->center + y_max_above;
	cal_y->min = cal_y->center - y_min_below;

	hid_dbg(ctlr->hdev, "calibration:\n"
			    "l_x_c=%d l_x_max=%d l_x_min=%d\n"
			    "l_y_c=%d l_y_max=%d l_y_min=%d\n"
			    "r_x_c=%d r_x_max=%d r_x_min=%d\n"
			    "r_y_c=%d r_y_max=%d r_y_min=%d\n",
			    ctlr->left_stick_cal_x.center,
			    ctlr->left_stick_cal_x.max,
			    ctlr->left_stick_cal_x.min,
			    ctlr->left_stick_cal_y.center,
			    ctlr->left_stick_cal_y.max,
			    ctlr->left_stick_cal_y.min,
			    ctlr->right_stick_cal_x.center,
			    ctlr->right_stick_cal_x.max,
			    ctlr->right_stick_cal_x.min,
			    ctlr->right_stick_cal_y.center,
			    ctlr->right_stick_cal_y.max,
			    ctlr->right_stick_cal_y.min);

	return 0;
}

/*
 * These divisors are calculated once rather than for each sample. They are only
 * dependent on the IMU calibration values. They are used when processing the
 * IMU input reports.
 */
static void joycon_calc_imu_cal_divisors(struct joycon_ctlr *ctlr)
{
	int i;

	for (i = 0; i < 3; i++) {
		ctlr->imu_cal_accel_divisor[i] = ctlr->accel_cal.scale[i] -
						ctlr->accel_cal.offset[i];
		ctlr->imu_cal_gyro_divisor[i] = ctlr->gyro_cal.scale[i] -
						ctlr->gyro_cal.offset[i];
	}
}

static const s16 DFLT_ACCEL_OFFSET /*= 0*/;
static const s16 DFLT_ACCEL_SCALE = 16384;
static const s16 DFLT_GYRO_OFFSET /*= 0*/;
static const s16 DFLT_GYRO_SCALE  = 13371;
static int joycon_request_imu_calibration(struct joycon_ctlr *ctlr)
{
	u16 imu_cal_addr = JC_IMU_CAL_FCT_DATA_ADDR;
	u8 *raw_cal;
	int ret;
	int i;

	/* check if user calibration exists */
	if (!joycon_check_for_cal_magic(ctlr, JC_IMU_CAL_USR_MAGIC_ADDR)) {
		imu_cal_addr = JC_IMU_CAL_USR_DATA_ADDR;
		hid_info(ctlr->hdev, "using user cal for IMU\n");
	} else {
		hid_info(ctlr->hdev, "using factory cal for IMU\n");
	}

	/* request IMU calibration data */
	hid_dbg(ctlr->hdev, "requesting IMU cal data\n");
	ret = joycon_request_spi_flash_read(ctlr, imu_cal_addr,
					    JC_IMU_CAL_DATA_SIZE, &raw_cal);
	if (ret) {
		hid_warn(ctlr->hdev,
			 "Failed to read IMU cal, using defaults; ret=%d\n",
			 ret);

		for (i = 0; i < 3; i++) {
			ctlr->accel_cal.offset[i] = DFLT_ACCEL_OFFSET;
			ctlr->accel_cal.scale[i] = DFLT_ACCEL_SCALE;
			ctlr->gyro_cal.offset[i] = DFLT_GYRO_OFFSET;
			ctlr->gyro_cal.scale[i] = DFLT_GYRO_SCALE;
		}
		joycon_calc_imu_cal_divisors(ctlr);
		return ret;
	}

	/* IMU calibration parsing */
	for (i = 0; i < 3; i++) {
		int j = i * 2;

		ctlr->accel_cal.offset[i] = get_unaligned_le16(raw_cal + j);
		ctlr->accel_cal.scale[i] = get_unaligned_le16(raw_cal + j + 6);
		ctlr->gyro_cal.offset[i] = get_unaligned_le16(raw_cal + j + 12);
		ctlr->gyro_cal.scale[i] = get_unaligned_le16(raw_cal + j + 18);
	}

	joycon_calc_imu_cal_divisors(ctlr);

	hid_dbg(ctlr->hdev, "IMU calibration:\n"
			    "a_o[0]=%d a_o[1]=%d a_o[2]=%d\n"
			    "a_s[0]=%d a_s[1]=%d a_s[2]=%d\n"
			    "g_o[0]=%d g_o[1]=%d g_o[2]=%d\n"
			    "g_s[0]=%d g_s[1]=%d g_s[2]=%d\n",
			    ctlr->accel_cal.offset[0],
			    ctlr->accel_cal.offset[1],
			    ctlr->accel_cal.offset[2],
			    ctlr->accel_cal.scale[0],
			    ctlr->accel_cal.scale[1],
			    ctlr->accel_cal.scale[2],
			    ctlr->gyro_cal.offset[0],
			    ctlr->gyro_cal.offset[1],
			    ctlr->gyro_cal.offset[2],
			    ctlr->gyro_cal.scale[0],
			    ctlr->gyro_cal.scale[1],
			    ctlr->gyro_cal.scale[2]);

	return 0;
}

static int joycon_set_report_mode(struct joycon_ctlr *ctlr)
{
	struct joycon_subcmd_request *req;
	u8 buffer[sizeof(*req) + 1] = { 0 };

	req = (struct joycon_subcmd_request *)buffer;
	req->subcmd_id = JC_SUBCMD_SET_REPORT_MODE;
	req->data[0] = 0x30; /* standard, full report mode */

	hid_dbg(ctlr->hdev, "setting controller report mode\n");
	return joycon_send_subcmd(ctlr, req, 1);
}

static int joycon_set_imu_mode(struct joycon_ctlr *ctlr, u8 mode)
{
	struct joycon_subcmd_request *req;
	u8 buffer[sizeof(*req) + 1] = { 0 };

	req = (struct joycon_subcmd_request *)buffer;
	req->subcmd_id = JC_SUBCMD_ENABLE_IMU;
	req->data[0] = mode;

	hid_dbg(ctlr->hdev, "setting controller report mode\n");
	return joycon_send_subcmd(ctlr, req, 1);
}

static int joycon_set_imu_data_rates(struct joycon_ctlr *ctlr)
{
	struct joycon_subcmd_request *req;
	u8 buffer[sizeof(*req) + 4] = { 0 };

	req = (struct joycon_subcmd_request *)buffer;
	req->subcmd_id = JC_SUBCMD_SET_IMU_SENSITIVITY;
	req->data[0] = 0x03; // +-2000dps
	req->data[1] = 0x00; // +-8G
	req->data[2] = 0x01; // 208Hz
	req->data[3] = 0x01; // 100Hz

	hid_dbg(ctlr->hdev, "setting controller report mode\n");
	return joycon_send_subcmd(ctlr, req, 4);
}

static inline int16_t unpack_imu_sample(u8 bytes[2])
{
	return (int16_t)((bytes[1] << 8) | bytes[0]);
}

static void joycon_parse_imu_report(struct joycon_ctlr *ctlr,
				    struct joycon_input_report *rep)
{
	u8 i, j;
	int16_t sign =
		ctlr->hdev->product == USB_DEVICE_ID_NINTENDO_JOYCONR ? -1 : 1;
	for (i = 0; i < JOYCON_IMU_PACKETS; i++) {
		ctlr->data.imu[i].accel[0] =
			unpack_imu_sample(rep->extra.imu[i].accel_x);
		ctlr->data.imu[i].accel[1] =
			unpack_imu_sample(rep->extra.imu[i].accel_y);
		ctlr->data.imu[i].accel[2] =
			unpack_imu_sample(rep->extra.imu[i].accel_z);

		ctlr->data.imu[i].gyro[0] =
			unpack_imu_sample(rep->extra.imu[i].gyro_x);
		ctlr->data.imu[i].gyro[1] =
			unpack_imu_sample(rep->extra.imu[i].gyro_y);
		ctlr->data.imu[i].gyro[2] =
			unpack_imu_sample(rep->extra.imu[i].gyro_z);

		/*
		 * These calculations (which use the controller's calibration
		 * settings to improve the final values) are based on those
		 * found in the community's reverse-engineering repo (linked at
		 * top of driver). For hid-nintendo, we make sure that the final
		 * value given to userspace is always in terms of the axis
		 * resolution we provided.
		 *
		 * Currently only the gyro calculations subtract the calibration
		 * offsets from the raw value itself. In testing, doing the same
		 * for the accelerometer raw values decreased accuracy.
		 *
		 * Note that the gyro values are multiplied by the
		 * precision-saving scaling factor to prevent large inaccuracies
		 * due to truncation of the resolution value which would
		 * otherwise occur. To prevent overflow (without resorting to 64
		 * bit integer math), the mult_frac macro is used.
		 */
		for (j = 0; j < 3; j++) {
			ctlr->data.imu[i].gyro[j] =
				mult_frac((JC_IMU_PREC_RANGE_SCALE *
				      (ctlr->data.imu[i].gyro[j] -
				       ctlr->gyro_cal.offset[j])),
				     ctlr->gyro_cal.scale[j],
				     ctlr->imu_cal_gyro_divisor[j]);

			ctlr->data.imu[i].accel[j] =
				(ctlr->data.imu[i].accel[j] *
			    ctlr->accel_cal.scale[j]) /
			    ctlr->imu_cal_accel_divisor[j];
		}

		/*
		 * The right joy-con has 2 axes negated, Y and Z. This is due to
		 * the orientation of the IMU in the controller. We negate those
		 * axes' values in order to be consistent with the left joy-con
		 * and the pro controller:
		 *   X: positive is pointing toward the triggers
		 *   Y: positive is pointing to the left
		 *   Z: positive is pointing up (out of the buttons/sticks)
		 * The axes follow the right-hand rule.
		 */
		for (j = 1; j < 3; j++) {
			ctlr->data.imu[i].gyro[j] *= sign;
			ctlr->data.imu[i].accel[j] *= sign;
		}
	}
}

static s32 joycon_map_stick_val(struct joycon_stick_cal *cal, s32 val)
{
	s32 center = cal->center;
	s32 min = cal->min;
	s32 max = cal->max;
	s32 new_val;

	if (val > center) {
		new_val = (val - center) * JC_MAX_STICK_MAG;
		new_val /= (max - center);
	} else {
		new_val = (center - val) * -JC_MAX_STICK_MAG;
		new_val /= (center - min);
	}
	new_val = clamp(new_val, (s32)-JC_MAX_STICK_MAG, (s32)JC_MAX_STICK_MAG);
	return new_val;
}

static void joycon_parse_report(struct joycon_ctlr *ctlr,
				struct joycon_input_report *rep)
{
	struct input_dev *dev = ctlr->input;
	struct joycon_stick_cal *xCal, *yCal;
	u8 *stick;
	u16 raw_x, raw_y;
	s32 x, y;
	u32 btns;
	u32 id = ctlr->hdev->product;

	btns = hid_field_extract(ctlr->hdev, rep->button_status, 0, 24);

	if (id == USB_DEVICE_ID_NINTENDO_JOYCONL || id == USB_DEVICE_ID_NINTENDO_JOYCONR) {
		if (id == USB_DEVICE_ID_NINTENDO_JOYCONL) {
			xCal = &ctlr->left_stick_cal_x;
			yCal = &ctlr->left_stick_cal_y;
			stick = rep->left_stick;
		} else {
			xCal = &ctlr->right_stick_cal_x;
			yCal = &ctlr->right_stick_cal_y;
			stick = rep->right_stick;
		}

		raw_x = hid_field_extract(ctlr->hdev, stick, 0, 12);
		raw_y = hid_field_extract(ctlr->hdev, stick + 1, 4, 12);

		x = joycon_map_stick_val(xCal, raw_x);
		y = joycon_map_stick_val(yCal, raw_y);

		if (x > (JC_MAX_STICK_MAG - 8000))
			btns |= 0x08000000;
		if (x < (-JC_MAX_STICK_MAG + 8000))
			btns |= 0x01000000;
		if (y > (JC_MAX_STICK_MAG - 8000))
			btns |= 0x02000000;
		if (y < (-JC_MAX_STICK_MAG + 8000))
			btns |= 0x04000000;

		mutex_lock(&ctlr->read_mutex);
		ctlr->data_dirty = true;
		ctlr->data.buttons = btns;

		joycon_parse_imu_report(ctlr, rep);
		ctlr->data.bat_con = rep->bat_con;
		mutex_unlock(&ctlr->read_mutex);

		wake_up_interruptible(&ctlr->wq);
	}

	input_sync(dev);
}

static const unsigned int joycon_button_inputs_l[] = {
	BTN_SELECT, BTN_Z, BTN_THUMBL,
	BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT,
	BTN_TL, BTN_TL2,
	0 /* 0 signals end of array */
};

static const unsigned int joycon_button_inputs_r[] = {
	BTN_START, BTN_MODE, BTN_THUMBR,
	BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST,
	BTN_TR, BTN_TR2,
	0 /* 0 signals end of array */
};

static int joycon_input_create(struct joycon_ctlr *ctlr)
{
	struct hid_device *hdev;
	const char *name;
	int ret;
	int i;

	hdev = ctlr->hdev;

	switch (hdev->product) {
	case USB_DEVICE_ID_NINTENDO_PROCON:
		name = "Nintendo Switch Pro Controller";
		break;
	case USB_DEVICE_ID_NINTENDO_JOYCONL:
		name = "Nintendo Switch Left Joy-Con";
		break;
	case USB_DEVICE_ID_NINTENDO_JOYCONR:
		name = "Nintendo Switch Right Joy-Con";
		break;
	default: /* Should be impossible */
		hid_err(hdev, "Invalid hid product\n");
		return -EINVAL;
	}

	ctlr->input = devm_input_allocate_device(&hdev->dev);
	if (!ctlr->input)
		return -ENOMEM;
	ctlr->input->id.bustype = hdev->bus;
	ctlr->input->id.vendor = hdev->vendor;
	ctlr->input->id.product = hdev->product;
	ctlr->input->id.version = hdev->version;
	ctlr->input->name = name;
	input_set_drvdata(ctlr->input, ctlr);

	/* set up sticks */
	if (hdev->product != USB_DEVICE_ID_NINTENDO_JOYCONR) {
		input_set_abs_params(ctlr->input, ABS_X,
				     -JC_MAX_STICK_MAG, JC_MAX_STICK_MAG,
				     JC_STICK_FUZZ, JC_STICK_FLAT);
		input_set_abs_params(ctlr->input, ABS_Y,
				     -JC_MAX_STICK_MAG, JC_MAX_STICK_MAG,
				     JC_STICK_FUZZ, JC_STICK_FLAT);
	}
	if (hdev->product != USB_DEVICE_ID_NINTENDO_JOYCONL) {
		input_set_abs_params(ctlr->input, ABS_RX,
				     -JC_MAX_STICK_MAG, JC_MAX_STICK_MAG,
				     JC_STICK_FUZZ, JC_STICK_FLAT);
		input_set_abs_params(ctlr->input, ABS_RY,
				     -JC_MAX_STICK_MAG, JC_MAX_STICK_MAG,
				     JC_STICK_FUZZ, JC_STICK_FLAT);
	}

	/* set up buttons */
	if (hdev->product != USB_DEVICE_ID_NINTENDO_JOYCONR) {
		for (i = 0; joycon_button_inputs_l[i] > 0; i++)
			input_set_capability(ctlr->input, EV_KEY,
					     joycon_button_inputs_l[i]);
	}
	if (hdev->product != USB_DEVICE_ID_NINTENDO_JOYCONL) {
		for (i = 0; joycon_button_inputs_r[i] > 0; i++)
			input_set_capability(ctlr->input, EV_KEY,
					     joycon_button_inputs_r[i]);
	}

	ret = input_register_device(ctlr->input);
	if (ret)
		return ret;

	/* Set the default controller player leds based on controller number */
	mutex_lock(&ctlr->output_mutex);
	ret = joycon_set_player_leds(ctlr, 0, 0xF >> (3 - ctlr->index));
	if (ret)
		hid_warn(ctlr->hdev, "Failed to set leds; ret=%d\n", ret);
	mutex_unlock(&ctlr->output_mutex);
	return 0;
}

/* Common handler for parsing inputs */
static int joycon_ctlr_read_handler(struct joycon_ctlr *ctlr, u8 *data,
							      int size)
{
	int ret = 0;

	if (data[0] == JC_INPUT_SUBCMD_REPLY || data[0] == JC_INPUT_IMU_DATA ||
	    data[0] == JC_INPUT_MCU_DATA) {
		if (size >= 12) /* make sure it contains the input report */
			joycon_parse_report(ctlr,
					    (struct joycon_input_report *)data);
	}

	return ret;
}

static int joycon_ctlr_handle_event(struct joycon_ctlr *ctlr, u8 *data,
							      int size)
{
	int ret = 0;
	bool match = false;
	struct joycon_input_report *report;

	if (unlikely(mutex_is_locked(&ctlr->output_mutex)) &&
	    ctlr->msg_type != JOYCON_MSG_TYPE_NONE) {
		switch (ctlr->msg_type) {
		case JOYCON_MSG_TYPE_USB:
			if (size < 2)
				break;
			if (data[0] == JC_INPUT_USB_RESPONSE &&
			    data[1] == ctlr->usb_ack_match)
				match = true;
			break;
		case JOYCON_MSG_TYPE_SUBCMD:
			if (size < sizeof(struct joycon_input_report) ||
			    data[0] != JC_INPUT_SUBCMD_REPLY)
				break;
			report = (struct joycon_input_report *)data;
			if (report->extra.reply.id == ctlr->subcmd_ack_match)
				match = true;
			break;
		default:
			break;
		}

		if (match) {
			memcpy(ctlr->input_buf, data,
			       min(size, (int)JC_MAX_RESP_SIZE));
			ctlr->msg_type = JOYCON_MSG_TYPE_NONE;
			ctlr->received_resp = true;
			wake_up(&ctlr->wait);

			/* This message has been handled */
			return 1;
		}
	}

	if (ctlr->ctlr_state == JOYCON_CTLR_STATE_READ)
		ret = joycon_ctlr_read_handler(ctlr, data, size);

	return ret;
}

static int nintendo_hid_event(struct hid_device *hdev,
			      struct hid_report *report, u8 *raw_data, int size)
{
	struct joycon_ctlr *ctlr = hid_get_drvdata(hdev);

	if (size < 1)
		return -EINVAL;

	return joycon_ctlr_handle_event(ctlr, raw_data, size);
}

#if 0
static struct joycon_ctlr *nintendo_get_device(struct inode *node, struct file *filep)
{
	unsigned int index;
	struct joycon_module *drv;
	struct joycon_ctlr *device;

	drv = container_of(node->i_cdev, struct joycon_module, cdev);

	index = iminor(node);
	if (index >= JOYCON_MAX_DEVICES)
		return NULL;

	mutex_lock(&drv->mutex);
	device = drv->ctlr[index];
	mutex_unlock(&drv->mutex);

	return device;
}
#endif

static int nintendo_dev_open(struct inode *node, struct file *filep)
{
	struct joycon_ctlr *device;
	struct joycon_module *drv;

	drv = container_of(node->i_cdev, struct joycon_module, cdev);

	mutex_lock(&drv->mutex);
	if (drv->busy) {
		mutex_unlock(&drv->mutex);
		return -EBUSY;
	}

	device = drv->ctlr[0];
	filep->private_data = drv;
	mutex_unlock(&drv->mutex);

	if (device == NULL)
		return 0;

	mutex_lock(&device->read_mutex);

	memset(&device->data, 0, sizeof(device->data));
	device->data_dirty = false;

	mutex_unlock(&device->read_mutex);
	return 0;
}

static int nintendo_dev_release(struct inode *node, struct file *filep)
{
	struct joycon_module *drv;

	drv = container_of(node->i_cdev, struct joycon_module, cdev);

	mutex_lock(&drv->mutex);
	drv->busy = false;
	mutex_unlock(&drv->mutex);

	filep->private_data = NULL;
	return 0;
}

static ssize_t nintendo_dev_read(struct file *filep, char *buffer, size_t length, loff_t *offset)
{
	struct joycon_ctlr *device;
	ssize_t count = 0;
	struct joycon_module *drv = filep->private_data;

	if (drv == NULL)
		return -ENODEV;

	// only support a single device for now
	mutex_lock(&drv->mutex);
	device = drv->ctlr[0];
	if (device == NULL || device->hdev == NULL) {
		mutex_unlock(&drv->mutex);
		return -ENODEV;
	}
	mutex_unlock(&drv->mutex);

	if (length != sizeof(struct joycon_msg_data))
		return -EINVAL;
	if (filep->f_flags & O_NONBLOCK)
		return -EINVAL;

	mutex_lock(&device->read_mutex);

	if (device->data_dirty) {
		if (copy_to_user(buffer, &device->data, length)) {
			pr_err("Failed to copy all data to user\n");
			mutex_unlock(&device->read_mutex);
			return -EFAULT;
		}
		count = length;
	}
	device->data_dirty = false;

	mutex_unlock(&device->read_mutex);
	return count;
}

static unsigned int nintendo_dev_poll(struct file *filep, poll_table *wait)
{
	struct joycon_ctlr *device;
	unsigned int flags = 0;
	struct joycon_module *drv = filep->private_data;

	if (drv == NULL)
		return -ENODEV;

	// only support a single device for now
	mutex_lock(&drv->mutex);
	device = drv->ctlr[0];
	if (device == NULL || device->hdev == NULL) {
		mutex_unlock(&drv->mutex);
		return flags;
	}
	mutex_unlock(&drv->mutex);

	poll_wait(filep, &device->wq, wait);

	mutex_lock(&device->read_mutex);

	if (device->data_dirty)
		flags = POLLIN | POLLRDNORM;

	mutex_unlock(&device->read_mutex);
	return flags;
}

static struct file_operations nintendo_fops = {
	.owner = THIS_MODULE,
	.open = nintendo_dev_open,
	.read = nintendo_dev_read,
	.poll = nintendo_dev_poll,
	.release = nintendo_dev_release,
};

static int nintendo_hid_probe(struct hid_device *hdev,
			    const struct hid_device_id *id)
{
	int ret;
	struct joycon_ctlr *ctlr;
	unsigned int index;
	hid_dbg(hdev, "probe - start\n");

	index = atomic_inc_return(&driver.ctlr_count) - 1;

	mutex_lock(&driver.mutex);
	ctlr = driver.ctlr[index];
	if (ctlr == NULL) {
		ctlr = kzalloc(sizeof(*ctlr), GFP_KERNEL);
		if (!ctlr) {
			ret = -ENOMEM;
			goto err;
		}
		driver.ctlr[index] = ctlr;
	}
	ctlr->index = index;
	ctlr->hdev = hdev;
	mutex_unlock(&driver.mutex);

	ctlr->ctlr_state = JOYCON_CTLR_STATE_INIT;
	hid_set_drvdata(hdev, ctlr);
	mutex_init(&ctlr->output_mutex);
	init_waitqueue_head(&ctlr->wait);

	init_waitqueue_head(&ctlr->wq);
	mutex_init(&ctlr->read_mutex);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "HID parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret) {
		hid_err(hdev, "HW start failed\n");
		goto err;
	}

	ret = hid_hw_open(hdev);
	if (ret) {
		hid_err(hdev, "cannot start hardware I/O\n");
		goto err_stop;
	}

	hid_device_io_start(hdev);

	/* Initialize the controller */
	mutex_lock(&ctlr->output_mutex);
	/* if handshake command fails, assume ble pro controller */
	if (hdev->product == USB_DEVICE_ID_NINTENDO_PROCON &&
	    !joycon_send_usb(ctlr, JC_USB_CMD_HANDSHAKE)) {
		hid_dbg(hdev, "detected USB controller\n");
		/* set baudrate for improved latency */
		ret = joycon_send_usb(ctlr, JC_USB_CMD_BAUDRATE_3M);
		if (ret) {
			hid_err(hdev, "Failed to set baudrate; ret=%d\n", ret);
			goto err_mutex;
		}
		/* handshake */
		ret = joycon_send_usb(ctlr, JC_USB_CMD_HANDSHAKE);
		if (ret) {
			hid_err(hdev, "Failed handshake; ret=%d\n", ret);
			goto err_mutex;
		}
		/*
		 * Set no timeout (to keep controller in USB mode).
		 * This doesn't send a response, so ignore the timeout.
		 */
		joycon_send_usb(ctlr, JC_USB_CMD_NO_TIMEOUT);
	}

	/* get controller calibration data, and parse it */
	ret = joycon_request_calibration(ctlr);
	if (ret) {
		/*
		 * We can function with default calibration, but it may be
		 * inaccurate. Provide a warning, and continue on.
		 */
		hid_warn(hdev, "Analog stick positions may be inaccurate\n");
	}

	/* get IMU calibration data, and parse it */
	ret = joycon_request_imu_calibration(ctlr);
	if (ret) {
		/*
		 * We can function with default calibration, but it may be
		 * inaccurate. Provide a warning, and continue on.
		 */
		hid_warn(hdev, "Unable to read IMU calibration data\n");
	}

	/* Set the reporting mode to 0x30, which is the full report mode */
	ret = joycon_set_report_mode(ctlr);
	if (ret) {
		hid_err(hdev, "Failed to set report mode; ret=%d\n", ret);
		goto err_mutex;
	}

	ret = joycon_set_imu_mode(ctlr, JC_IMU_MODE_ENABLE);
	if (ret) {
		hid_err(hdev, "Failed to imu mode; ret=%d\n", ret);
		goto err_mutex;
	}

	ret = joycon_set_imu_data_rates(ctlr);
	if (ret) {
		hid_err(hdev, "Failed to imu mode; ret=%d\n", ret);
		goto err_mutex;
	}

	mutex_unlock(&ctlr->output_mutex);

	ret = joycon_input_create(ctlr);
	if (ret) {
		hid_err(hdev, "Failed to create input device; ret=%d\n", ret);
		goto err_close;
	}

	ctlr->ctlr_state = JOYCON_CTLR_STATE_READ;

	hid_dbg(hdev, "probe - success\n");
	return 0;

err_mutex:
	mutex_unlock(&ctlr->output_mutex);
err_close:
	hid_hw_close(hdev);
err_stop:
	hid_hw_stop(hdev);
err:
	hid_err(hdev, "probe - fail = %d\n", ret);
	return ret;
}

static void nintendo_hid_remove(struct hid_device *hdev)
{
	struct joycon_ctlr *device = hid_get_drvdata(hdev);
	if (device) {
		mutex_lock(&driver.mutex);
		driver.ctlr[device->index]->hdev = NULL;
		mutex_unlock(&driver.mutex);
	}

	atomic_dec(&driver.ctlr_count);

	hid_dbg(hdev, "remove\n");
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static int __init nintendo_hid_init(void)
{
	int ret, i;

	mutex_init(&driver.mutex);

	atomic_set(&driver.ctlr_count, 0);
	for (i = 0; i < JOYCON_MAX_DEVICES; i++)
		driver.ctlr[i] = NULL;

	driver.class = class_create(THIS_MODULE, "joycon");
	if (IS_ERR(driver.class)) {
		pr_err("failed to create joycon class\n");
		ret = PTR_ERR(driver.class);
		goto err_class_create;
	}

	ret = alloc_chrdev_region(&driver.devno, 0, JOYCON_MAX_DEVICES, "joycon");
	if (ret) {
		pr_err("failed to allocate character device region\n");
		goto err_alloc_region;
	}

	cdev_init(&driver.cdev, &nintendo_fops);
	driver.cdev.owner = THIS_MODULE;

	ret = cdev_add(&driver.cdev, driver.devno, JOYCON_MAX_DEVICES);
	if (ret) {
		pr_err("failed to add joycon to cdev\n");
		goto err_cdev_add;
	}
	pr_info("creating joycon0 device");
	driver.dev = device_create(driver.class, NULL, MKDEV(MAJOR(driver.devno), 0), &driver, "joycon0");
	if (IS_ERR(driver.dev)) {
		ret = -ENODEV;
		pr_err("failed to create joycon device\n");
		goto err_device_create;
	}
	return 0;

err_device_create:
	cdev_del(&driver.cdev);
err_alloc_region:
	class_destroy(driver.class);
err_cdev_add:
	unregister_chrdev_region(driver.devno, JOYCON_MAX_DEVICES);
err_class_create:
	return ret;
}

static void __exit nintendo_hid_exit(void)
{
	int i;
	for (i = 0; i < JOYCON_MAX_DEVICES; i++) {
		if (driver.ctlr[i]) {
			kfree(driver.ctlr[i]);
			driver.ctlr[i] = NULL;
		}
	}

	cdev_del(&driver.cdev);
	class_destroy(driver.class);
	unregister_chrdev_region(driver.devno, JOYCON_MAX_DEVICES);
}

static const struct hid_device_id nintendo_hid_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_NINTENDO,
			 USB_DEVICE_ID_NINTENDO_PROCON) },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_NINTENDO,
			 USB_DEVICE_ID_NINTENDO_PROCON) },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_NINTENDO,
			 USB_DEVICE_ID_NINTENDO_JOYCONL) },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_NINTENDO,
			 USB_DEVICE_ID_NINTENDO_JOYCONR) },
	{ }
};
MODULE_DEVICE_TABLE(hid, nintendo_hid_devices);

static struct hid_driver nintendo_hid_driver = {
	.name		= "nintendo_sg",
	.id_table	= nintendo_hid_devices,
	.probe		= nintendo_hid_probe,
	.remove		= nintendo_hid_remove,
	.raw_event	= nintendo_hid_event,
};
module_hid_driver(nintendo_hid_driver);

module_init(nintendo_hid_init);
module_exit(nintendo_hid_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel J. Ogorchock <djogorchock@gmail.com>");
MODULE_DESCRIPTION("Driver for Nintendo Switch Controllers");
