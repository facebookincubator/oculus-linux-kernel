// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "mfi_i2c_driver.h"

static atomic_t dev_opened;

struct mfi_priv {
	struct i2c_client *client;
	struct miscdevice miscdev;
	struct device *dev;
	struct i2c_adapter *adapter;
	unsigned short addr;
};

struct acc_cert {
	uint16_t len;
	uint8_t cert[MAX_ASSC_CERT_LEN];
};

struct chl_resp {
	int8_t result;	// 0: generated successfully, -1: error, -2: not done yet
	uint8_t resp[CHL_RESP_LEN]; // response always has 64 bytes
};

int mfi_write(struct mfi_priv *cdata, u8 reg, const void *buffer, unsigned int len)
{
	struct i2c_msg msg;
	u8 *write_buf;
	int retry = 0, ret = 0;

	write_buf = devm_kzalloc(cdata->dev, len + 1, GFP_KERNEL);
	write_buf[0] = reg;
	udelay(WAIT_CP_ACK_US);
	memcpy(write_buf + 1, buffer, len);
	udelay(WAIT_CP_ACK_US);

	msg.addr = cdata->addr;
	msg.flags = I2C_M_STOP;
	msg.len = len + 1;
	msg.buf = write_buf;

	do {
		++retry;

		ret = i2c_transfer(cdata->adapter, &msg, 1);
		if (ret < 0) {
			dev_info(cdata->dev, "Failed writing register %x with error %d", reg, ret);

			if (retry < MFI_MAX_WRITE_RETRIES)
				udelay(WAIT_CP_ACK_US);
		}
	} while (ret < 0 && retry < MFI_MAX_WRITE_RETRIES);

	if (ret < 0)
		dev_err(cdata->dev, "Failed to write register %x after %d retries",
				reg, MFI_MAX_WRITE_RETRIES);

	devm_kfree(cdata->dev, write_buf);
	return ret;
}
EXPORT_SYMBOL_GPL(mfi_write);

int mfi_read(struct mfi_priv *cdata, u8 reg, void *buffer, unsigned int len)
{
	struct i2c_msg msg;
	int retry = 0, ret = 0;

	ret = mfi_write(cdata, reg, NULL, 0);
	if (ret < 0)
		return ret;

	msg.addr = cdata->addr;
	msg.flags = I2C_M_RD | I2C_M_STOP;
	msg.len = len;
	msg.buf = (u8 *)buffer;

	do {
		++retry;

		ret = i2c_transfer(cdata->adapter, &msg, 1);
		if (ret < 0) {
			dev_info(cdata->dev, "Failed reading register %x with error %d", reg, ret);

			if (retry < MFI_MAX_READ_RETRIES)
				udelay(WAIT_CP_ACK_US);
		}
	} while (ret < 0 && retry < MFI_MAX_READ_RETRIES);

	if (ret < 0)
		dev_err(cdata->dev, "Failed to read register %x after %d retries",
				reg, MFI_MAX_READ_RETRIES);

	return ret;
}
EXPORT_SYMBOL_GPL(mfi_read);

/*
 * Called when a process tries to open a dev file
 */
static int mfi_dev_open(struct inode *inode, struct file *filp)
{
	if (atomic_xchg(&dev_opened, 1) == 1)
		return -EBUSY;
	return 0;
}

/*
 * Called when a process closes the device file
 */
static int mfi_dev_release(struct inode *inode, struct file *filp)
{
	atomic_xchg(&dev_opened, 0);
	return 0;
}

static int get_cert_serno(struct mfi_priv *cdata, u8 __user *cert_serno)
{
	int ret;
	u8 cert_serno_val[32];

	ret = mfi_read(cdata, DEV_CERT_SERNO, &cert_serno_val,
			sizeof(cert_serno_val));
	if (ret < 0)
		return ret;

	ret = copy_to_user(cert_serno, &cert_serno_val, sizeof(cert_serno_val));
	if (ret < 0) {
		pr_err("%s: Failed to copy %ul bytes to user space", __func__, ret);
		return -EFAULT;
	}

	return 0;
}

static int get_cert(struct mfi_priv *cdata, struct acc_cert __user *acc_cert)
{
	int ret;
	u8 cert_len[2];
	struct acc_cert acc_cert_val;

	// Get the length of the cert
	ret = mfi_read(cdata, ACC_CERT_DATA_LEN, &cert_len, sizeof(cert_len));
	if (ret < 0)
		return ret;

	acc_cert_val.len = cert_len[0] << 8 | cert_len[1];

	// Check the cert length is no more than 609 so to prevent
	// stack overflow hacker attach
	if (acc_cert_val.len > sizeof(acc_cert_val.cert)) {
		pr_err("%s: Invalid cert length %d was returned\n", __func__, acc_cert_val.len);
		return -EFAULT;
	}

	// zero the memory of the cert
	memset(&acc_cert_val.cert, 0, sizeof(acc_cert_val.cert));

	// Get the data of the cert
	ret = mfi_read(cdata, ACC_CERT_DATA1, &acc_cert_val.cert, acc_cert_val.len);
	if (ret < 0)
		return ret;

	ret = copy_to_user(acc_cert, &acc_cert_val, sizeof(acc_cert_val));
	if (ret < 0) {
		pr_err("%s: Failed to copy %ul bytes to user space", __func__, ret);
		return -EFAULT;
	}

	return 0;
}

static int start_chl(struct mfi_priv *cdata, const u8 __user *chl_data)
{
	int ret;
	u8 chl_data_val[32];
	uint8_t chl_status_ctrl;

	ret = copy_from_user(&chl_data_val, chl_data, sizeof(chl_data_val));
	if (ret < 0) {
		pr_err("%s: Failed to copy %ul bytes from user space", __func__, ret);
		return -EFAULT;
	}

	// Set the challenge data
	ret = mfi_write(cdata, CHL_DATA, &chl_data_val, sizeof(chl_data_val));
	if (ret < 0) {
		pr_err("%s: Failed to set the challenge data: %d\n", __func__, ret);
		return ret;
	}

	// Start a new challenge request process
	chl_status_ctrl = 0x01;
	ret = mfi_write(cdata, AUTH_CTL_STAT, &chl_status_ctrl,
			sizeof(chl_status_ctrl));
	if (ret < 0) {
		pr_err("%s: Failed to start a new challenge request process: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int get_chl_resp(struct mfi_priv *cdata, struct chl_resp __user *chl_resp)
{
	int ret, retry = 0;
	struct chl_resp chl_resp_val;
	uint8_t chl_status, err_code;

	do {
		++retry;

		// Check if challenge response has been generated
		ret = mfi_read(cdata, AUTH_CTL_STAT, &chl_status,
				sizeof(chl_status));
		if (ret < 0) {
			if (retry == MAX_CHALLENGE_RETRIES) {
				pr_err("%s: After %d retries, challenge status couldn't be read", __func__, retry);
				goto auth_error;
			} else {
				// wait and retry
				udelay(WAIT_CP_ACK_US);
			}
		}

		if (chl_status & CHL_RESP_READY) {
			// response ready, and pick it up
			chl_resp_val.result = 0;
			ret = mfi_read(cdata, CHL_RESP_DATA, &chl_resp_val.resp,
					sizeof(chl_resp_val.resp));
			if (ret < 0) {
				pr_err("%s: Failed to read challenge response data: %d\n", __func__, ret);
				goto auth_error;
			}
		} else if (chl_status & CHL_RESP_GEN_ERR) {
			// Failed to generate the response
			// Read the error code and clear it off from the register
			ret = mfi_read(cdata, ERR_CODE, &err_code, sizeof(err_code));
			if (ret < 0) {
				pr_err("%s: Failed to read auth error code: %d\n", __func__, ret);
				goto auth_error;
			}

			chl_resp_val.result = -2;
		} else {
			// Challenge response generation is still in progress
			if (retry == MAX_CHALLENGE_RETRIES) {
				pr_warn("%s: After %d retries, challenge response still has not been generated", __func__, retry);
				chl_resp_val.result = -1;
			} else {
				// wait and retry
				udelay(WAIT_CP_ACK_US);
			}
		}
	} while (retry < MAX_CHALLENGE_RETRIES);

	ret = copy_to_user(chl_resp, &chl_resp_val, sizeof(chl_resp_val));
	if (ret < 0)
		goto comm_error;

	return 0;

auth_error:
	pr_err("%s: Error happened when reading the challenge response", __func__);
	return ret;

comm_error:
	pr_err("%s: Failed to copy %ul bytes to user space", __func__, ret);
	return -EIO;

}

static inline struct mfi_priv *to_mfi_dev(struct file *file)
{
	return container_of(file->private_data, struct mfi_priv, miscdev);
}

/*
 * Called when a process execute the predefined IOCTL command
 */
static long mfi_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct mfi_priv *cdata = to_mfi_dev(filp);

	switch (cmd) {
	case GET_CERT_SERNO:
		return get_cert_serno(cdata, (u8 *)arg);
	case GET_CERT:
		return get_cert(cdata, (struct acc_cert *)arg);
	case START_CHL:
		return start_chl(cdata, (u8 *)arg);
	case GET_CHL_RESP:
		return get_chl_resp(cdata, (struct chl_resp *)arg);
	default:
		pr_err("%s: Unrecognized IOCTL %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations mfi_dev_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
	.open = mfi_dev_open,
	.release = mfi_dev_release,
	.unlocked_ioctl = mfi_dev_ioctl
};

static int mfi_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mfi_priv *cdata;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "No I2C functionality present\n");
		return -ENODEV;
	}

	cdata = devm_kzalloc(&client->dev, sizeof(struct mfi_priv), GFP_KERNEL);
	if (cdata == NULL)
		return -ENOMEM;

	cdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	cdata->miscdev.name = MFI_DEV_NAME;
	cdata->miscdev.fops = &mfi_dev_fops;
	cdata->miscdev.parent = &client->dev;
	ret = misc_register(&cdata->miscdev);
	if (ret)
		return ret;

	cdata->client = client;
	cdata->addr = client->addr;
	cdata->dev = &client->dev;
	cdata->adapter = client->adapter;
	i2c_set_clientdata(client, cdata);

	dev_err(cdata->dev, "%s probe done", MFI_DEV_NAME);
	return 0;
}

static int mfi_remove(struct i2c_client *client)
{
	struct mfi_priv *cdata = i2c_get_clientdata(client);

	if (atomic_xchg(&dev_opened, 1) == 1)
		msleep(10);

	misc_deregister(&cdata->miscdev);
	atomic_xchg(&dev_opened, 0);
	return 0;
}

static const struct of_device_id match_table[] = {
	{
		.compatible = "meta,mfi-i2c",
	},
	{}
};

static struct i2c_driver mfi_driver = {
	.driver = {
		.name = "mfi-i2c-driver",
		.of_match_table = match_table,
	},
	.probe = mfi_probe,
	.remove = mfi_remove,
};

module_i2c_driver(mfi_driver);

MODULE_DESCRIPTION("MFi I2C driver");
MODULE_LICENSE("GPL");
