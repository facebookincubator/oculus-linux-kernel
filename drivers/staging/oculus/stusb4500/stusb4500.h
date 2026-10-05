/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __STUSB4500_USB_H__
#define __STUSB4500_USB_H__

/**
 * I2C registers defined in:
 * https://www.st.com/resource/en/user_manual/um2650-the-stusb4500-software-programing-guide-stmicroelectronics.pdf
 */
#define STUSB4500_ALERT_STATUS_1		0x0B /* RO */
#define STUSB4500_ALERT_STATUS_1_MASK		0x0C /* RW */
#define STUSB4500_PORT_STATUS_0			0x0D /* RO */
#define STUSB4500_PORT_STATUS_1			0x0E /* RO */
#define STUSB4500_CC_STATUS			0x11 /* RO */
#define STUSB4500_RESET_CTRL			0x23 /* RW */
#define STUSB4500_DEVICE_ID			0x2F /* RO */

#define STUSB4500_REG_MAX			0x97

/**
 * NVM programming information is derived from:
 * https://github.com/usb-c/STUSB4500/tree/master/Firmware/Project
 */

/* NVM programming registers definition */
#define RW_BUFFER_REG				0x53
#define FTP_CUST_PASSWORD_REG			0x95
#define FTP_CTRL_0_REG				0x96
#define FTP_CTRL_1_REG				0x97

/* NVM programming constants */
#define FTP_CUST_PASSWORD			0x47
#define FTP_CUST_PWR				0x80
#define FTP_CUST_RST_N				0x40
#define FTP_CUST_REQ				0x10
#define FTP_CUST_SECT				0x07
#define FTP_CUST_SER				0xF8
#define FTP_CUST_OPCODE_MASK			0x07
#define READ_TAG				0x00
#define WRITE_PL				0x01
#define WRITE_SER				0x02
#define READ_PL					0x03
#define READ_SER				0x04
#define ERASE_SECTOR				0x05
#define PROG_SECTOR				0x06
#define SOFT_PROG_SECTOR			0x07
#define SECTOR_0				0x01
#define SECTOR_1				0x02
#define SECTOR_2				0x04
#define SECTOR_3				0x08
#define SECTOR_4				0x10
#define SECTOR_BLOCK_SIZE			8
#define SECTOR_NUM_MAX				5
#define REG_RAW_VAL_SIZE			6

/* STUSB4500_ALERT_STATUS_1/STUSB4500_ALERT_STATUS_1_MASK bitfields */
#define STUSB4500_PRT_STATUS			BIT(1)
#define STUSB4500_CC_HW_FAULT_STATUS		BIT(4)
#define STUSB4500_TYPEC_MONITORING_STATUS	BIT(5)
#define STUSB4500_PORT_STATUS			BIT(6)

/* STUSB4500_PORT_STATUS_1 bitfields */
#define STUSB4500_PORT_ATTACHED_DEVICE		GENMASK(7, 5)

/* STUSB4500_CC_STATUS bitfields */
#define STUSB4500_CC1_STATE			GENMASK(1, 0)
#define STUSB4500_CC2_STATE			GENMASK(3, 2)
#define STUSB4500_CONNECT_RESULT		BIT(4)

/* STUSB4500_DEVICE_ID value */
#define STUSB4500_DEVICE_ID_VAL			0x25

#endif /* __STUSB4500_USB_H__ */
