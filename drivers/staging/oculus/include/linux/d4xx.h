/* SPDX-License-Identifier: GPL-2.0 */

#ifndef RSCC_D4XX_H
#define RSCC_D4XX_H

#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <linux/ds5_ioctl.h>

enum {
	DS5_FW_BUSY = 0,
	DS5_FW_READY = 1,
};

enum {
	DS5_DS5U,
};

struct ds5 {
	bool power;
	struct i2c_client *client;
	struct cdev ds5_cdev;
	struct class *ds5_class;
	int device_open_count;
	struct mutex lock;
	struct regmap *regmap;
	struct regulator *vdd_3p3;
	struct regulator *vdd_0p9;
	struct regulator *vcc;
	int prstn;
	int ready;
	struct ds5_stream_config sensor;
	u16 fw_version;
	u16 fw_build;
};

extern int ds5_get_fw_version(struct device *dev, u16 *fw_version,
		u16 *fw_build);
extern int ds5_get_stream_status(struct device *dev, struct ds5_status *status);
extern int ds5_set_stream_config(struct device *dev,
		struct ds5_stream_config *config);
extern int ds5_set_streaming(struct device *dev, int enabled);
extern int ds5_set_preset(struct device *dev, struct ds5_preset *preset);

#endif // RSCC_D4XX_H
