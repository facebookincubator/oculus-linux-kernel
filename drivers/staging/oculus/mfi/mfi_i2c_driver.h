/* SPDX-License-Identifier: GPL+ */

#ifndef __MFI_I2C_DRIVER_H__
#define __MFI_I2C_DRIVER_H__

// MFi register addresses
#define DEV_VER 0x00
#define AUTH_REV 0x01
#define AUTH_PROTO_MAJOR_VER 0x02
#define AUTH_PROTO_MINOR_VER 0x03
#define DEV_ID 0X04
#define ERR_CODE 0x05
#define AUTH_CTL_STAT 0x10
#define CHL_RESP_DATA_LEN 0x11
#define CHL_RESP_DATA 0x12
#define CHL_DATA_LEN 0x20
#define CHL_DATA 0x21
#define ACC_CERT_DATA_LEN 0x30
#define ACC_CERT_DATA1 0x31
#define ACC_CERT_DATA2 0x32
#define ACC_CERT_DATA3 0x33
#define ACC_CERT_DATA4 0x34
#define ACC_CERT_DATA5 0x35
#define SELF_TEST_STAT 0x40
#define DEV_CERT_SERNO 0x4E
#define SLEEP 0x60

// Some special values of MFi registers
#define CHL_RESP_READY 0x10
#define CHL_RESP_GEN_ERR 0x80

#define MAX_CHALLENGE_RETRIES 20

#define WAIT_CP_ACK_US 500

// MFi read/write control
#define MFI_MAX_READ_RETRIES 6
#define MFI_MAX_WRITE_RETRIES 6

#define MFI_DEV_NAME "mfi-i2c"
#define MFI_DEV_MAJOR 0   // Indicate to use dynamic major
#define MFI_DEV_MINOR 0   // Currently assuming there is only one instance of device

#define CHL_RESP_LEN 64         // The challenge response data length
#define MAX_ASSC_CERT_LEN 609   // The max accessory certificate data length

/* Use the magic number for joystick. Do not expect overlap would happen here */
#define IOCTL_MAGIC_NUM 'j'

/* ioctl used to get the certificate serial no */
#define GET_CERT_SERNO _IOR(IOCTL_MAGIC_NUM, 1, uint32_t)

/* ioctl used to get the accessory certificate */
#define GET_CERT _IOR(IOCTL_MAGIC_NUM, 2, struct acc_cert)

/* ioctl used to pass the challenge data and start the challenge */
#define START_CHL _IOW(IOCTL_MAGIC_NUM, 3, uint32_t)

/* ioctl used to get the challenge response */
#define GET_CHL_RESP _IOR(IOCTL_MAGIC_NUM, 4, struct chl_resp)

#endif // __MFI_I2C_DRIVER_H__
