/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2019-2021 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************/

#ifndef _NFC_I2C_DRV_H_
#define _NFC_I2C_DRV_H_
#include <linux/i2c.h>

#define NFC_I2C_DRV_STR   "qcom,sn-nci"	/*kept same as dts */
#define NFC_I2C_DEV_ID	  "sn-i2c"

struct nfc_dev;

//Interface specific parameters
struct i2c_dev {
	struct i2c_client *client;
	/*IRQ parameters */
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	/* NFC_IRQ wake-up state */
	bool irq_wake_up;
};

long nfc_i2c_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg);
int nfc_i2c_dev_probe(struct i2c_client *client,
		      const struct i2c_device_id *id);
int nfc_i2c_dev_remove(struct i2c_client *client);
int nfc_i2c_dev_suspend(struct device *device);
int nfc_i2c_dev_resume(struct device *device);

#if IS_ENABLED(CONFIG_NFC_QTI_I2C)

int i2c_enable_irq(struct nfc_dev *dev);
int i2c_disable_irq(struct nfc_dev *dev);
int i2c_write(struct nfc_dev *dev, const char *buf, size_t count,
						int max_retry_cnt);
int i2c_read(struct nfc_dev *dev, char *buf, size_t count, int timeout);

#else

static inline int i2c_enable_irq(struct nfc_dev *dev)
{
	return -ENXIO;
}

static inline int i2c_disable_irq(struct nfc_dev *dev)
{
	return -ENXIO;
}

static inline int i2c_write(struct nfc_dev *dev, const char *buf,
					size_t count, int max_retry_cnt)
{
	return -ENXIO;
}

static inline int i2c_read(struct nfc_dev *dev, char *buf, size_t count, int timeout)
{
	return -ENXIO;
}

#endif

#endif //_NFC_I2C_DRV_H_
