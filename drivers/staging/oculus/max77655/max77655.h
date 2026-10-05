// SPDX-License-Identifier: GPL-2.0+

#ifndef __MAX77655_I2C_DRIVER_H__
#define __MAX77655_I2C_DRIVER_H__

// PMIC read/write control
#define MAX77655_MAX_READ_RETRIES 6
#define MAX77655_MAX_WRITE_RETRIES 6

// DEVICE SELECTION
// Use "pmic-i2c-OP03010" for OP03010 projector and "pmic-i2c-OP02220" for bare panel or OP02220
#define MAX77655_DEV_NAME "pmic-i2c-OP03010"
#define MAX77655_REGULATOR_NAME "pmicOP03010"

// I2C settings and prototypes
#define MIN_DELAY_US 100000
#define MAX_DELAY_US 200000
#define WRITE_DELAY_MS 10
#define I2C_DELAY_US 500

#define MAX77655_PMIC_ERR_REG 0x05
#define MAX77655_PMIC_REV_REG 0x06

struct max77655_data {
	struct i2c_client            *i2c;
	struct device                *dev;
	struct i2c_adapter           *adapter;
	unsigned short               addr;
	bool                         is_enabled;
	bool                         is_push_button_mode;
	u8                           pmic_rev;
	u8                           pmic_err;
	struct gpio_desc             *pmic_en_gpio;
	struct regmap                *regmap;
};

int max77655_i2c_write(struct max77655_data *pmic, u8 reg, void *buffer, unsigned int len);
int max77655_i2c_read(struct max77655_data *pmic, u8 reg, void *buffer, unsigned int len);

#endif // __MAX77655_I2C_DRIVER_H__
