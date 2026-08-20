// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Maxim Integrated Products, Inc.
 * Author: Maxim Integrated <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/firmware.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>

#include "max17332.h"
#include "max17332-battery.h"
#include "max17332-cache.h"
#include "max17332-voltage-adjustment.h"

#define DRIVER_DESC    "MAX17332 MFD Driver"
#define DRIVER_NAME    MAX17332_NAME
#define DRIVER_VERSION "1.2"
#define DRIVER_AUTHOR  "opensource@maximintegrated.com"

#define I2C_NVM_ADDR        "nvm-address"
#define I2C_MAX_RETRIES     5
#define I2C_RETRY_DELAY_MS  150

#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

#define BOB_INIT_VOLTAGE_CHG_DELTA_DEFAULT  250000
#define BATT_DISCHRG_WAIT_TIME_MS   500
#define MAX_NUM_BATT_DISCHRG_CHECKS 20

static const struct regmap_config max17332_regmap_config = {
	.reg_bits   = 8,
	.val_bits   = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config max17332_regmap_config_nvm = {
	.reg_bits   = 8,
	.val_bits   = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.cache_type = REGCACHE_NONE,
};

static const u8 skip_cache_expiry_check_list[] = {
	REG_REPSOC,
};

static const struct max17332_cache_config read_failure_cache_config = {
	.min_reg_addr = REG_REPSOC,
	.max_reg_addr = REG_REPSOC,
	.cache_expiry_seconds = 360,
	.reg_skip_cache_expiry_list = skip_cache_expiry_check_list,
	.num_reg_skip_cache_expiry_list = ARRAY_SIZE(skip_cache_expiry_check_list),
};

static struct i2c_fails i2c_fails_count;

/*******************************************************************************
 * Chip IO
 ******************************************************************************/
int max17332_read(struct regmap *regmap, u8 addr, u16 *val)
{
	unsigned int buf = 0;
	int rc = 0;
	int retry = 0;

	rc = regmap_read(regmap, (unsigned int)addr, &buf);
	while (rc < 0 && retry++ < I2C_MAX_RETRIES) {
		pr_err("%s: regmap_read returns error no: %d\n", __func__, rc);
		pr_info("retry %d...\n", retry);
		msleep(I2C_RETRY_DELAY_MS);
		rc = regmap_read(regmap, (unsigned int)addr, &buf);
	}
	if (rc < 0) {
		pr_err("%s: regmap_read returns error no: %d\n", __func__, rc);
		i2c_fails_count.i2c_read_fail_count++;
		return rc;
	}

	*val = (u16)buf;
	return rc;
}
EXPORT_SYMBOL(max17332_read);

int max17332_read_cached(struct regmap *regmap, struct max17332_cache *cache, u8 addr, u16 *val)
{
	int ret = max17332_read(regmap, addr, val);

	if (ret < 0) {
		ret = max17332_cache_read(cache, addr, val);

		if (ret < 0)
			pr_err("%s: max17332_cache_read returned error: %d\n", __func__, ret);

		return ret;
	}

	ret = max17332_cache_write(cache, addr, *val);

	if (ret < 0)
		pr_warn("%s: max17332_cache_write returned error: %d\n", __func__, ret);

	return 0;
}
EXPORT_SYMBOL(max17332_read_cached);

int max17332_write(struct regmap *regmap, u8 addr, u16 val)
{
	int ret;
	int retry = 0;

	ret = regmap_write(regmap, (unsigned int)addr, (unsigned int)val);
	while (ret < 0 && retry++ < I2C_MAX_RETRIES) {
		pr_err("%s: regmap_write returns error no: %d\n", __func__, ret);
		pr_info("retry %d...\n", retry);
		msleep(I2C_RETRY_DELAY_MS);
		ret = regmap_write(regmap, (unsigned int)addr, (unsigned int)val);
	}
	if (ret < 0) {
		i2c_fails_count.i2c_write_fail_count++;
		pr_err("%s: regmap_write returns error no: %d\n", __func__, ret);
	}
	return ret;
}
EXPORT_SYMBOL(max17332_write);

int max17332_write_unlock(struct max17332_dev *max17332,
			  struct regmap *regmap, u8 addr, u16 val)
{
	struct device *dev = max17332->dev;
	int ret;
	int retry = 0;

	mutex_lock(&max17332->lock_write);

	ret = max17332_lock_write_protection(max17332, false);
	if (ret) {
		dev_err(dev, "%s: fail to unlock write protection: %d\n",
				__func__, ret);
		mutex_unlock(&max17332->lock_write);
		return ret;
	}

	ret = regmap_write(regmap, (unsigned int)addr, (unsigned int)val);
	while (ret < 0 && retry++ < I2C_MAX_RETRIES) {
		pr_err("%s: regmap_write returns error no: %d\n", __func__, ret);
		pr_info("retry %d...\n", retry);
		msleep(I2C_RETRY_DELAY_MS);
		ret = regmap_write(regmap, (unsigned int)addr, (unsigned int)val);
	}
	if (ret < 0)
		pr_err("%s: regmap_write returns error no: %d\n", __func__, ret);

	ret = max17332_lock_write_protection(max17332, true);
	if (ret) {
		dev_err(dev, "%s: fail to lock write protection: %d\n",
				__func__, ret);
	}

	mutex_unlock(&max17332->lock_write);

	return ret;
}
EXPORT_SYMBOL(max17332_write_unlock);

int max17332_update_bits(struct regmap *regmap, u8 addr, u16 mask, u16 val)
{
	int ret;

	ret = regmap_update_bits(regmap, (unsigned int)addr,
							(unsigned int)mask, (unsigned int)val);
	if (ret < 0)
		pr_err("%s: regmap_update_bits returns error no: %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(max17332_update_bits);

/* Declare Interrupt */
static const struct regmap_irq max17332_intsrc_irqs[] = {
	{ .reg_offset = 0, .mask = BIT_STATUS_PA,},
	{ .reg_offset = 0, .mask = BIT_STATUS_SMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_TMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_VMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_CA,},
	{ .reg_offset = 0, .mask = BIT_STATUS_SMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_TMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_VMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_DSOCI,},
	{ .reg_offset = 0, .mask = BIT_STATUS_IMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_ALLOWCHGB,},
	{ .reg_offset = 0, .mask = BIT_STATUS_BST,},
	{ .reg_offset = 0, .mask = BIT_STATUS_IMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_POR,},
};

static const struct regmap_irq_chip max17332_intsrc_irq_chip = {
	.name = "max17332 intsrc",
	.status_base = REG_STATUS,
	.mask_base = REG_STATUS_MASK,
	.num_regs = 1,
	.irqs = max17332_intsrc_irqs,
	.num_irqs = ARRAY_SIZE(max17332_intsrc_irqs),
};

int max17332_lock_write_protection(struct max17332_dev *dev, bool lock_en)
{
	int ret;
	u16 val;

	if (lock_en == dev->lock_en) {
		dev_warn(dev->dev, "%s: Write protection is already %s, skip!\n",
				__func__, lock_en ? "On" : "Off");
		return 0;
	}

	ret = max17332_read(dev->regmap_pmic, REG_COMMSTAT, &val);
	if (ret < 0)
		goto err;

	if (lock_en) {
		/* lock write protection */
		ret = max17332_write(dev->regmap_pmic, REG_COMMSTAT, val | 0x00F1);
		ret |= max17332_write(dev->regmap_pmic, REG_COMMSTAT, val | 0x00F1);
	} else {
		/* unlock write protection */
		ret = max17332_write(dev->regmap_pmic, REG_COMMSTAT, val & 0xFF06);
		ret |= max17332_write(dev->regmap_pmic, REG_COMMSTAT, val & 0xFF06);
	}
	if (ret < 0)
		goto err;

	ret = max17332_read(dev->regmap_pmic, REG_COMMSTAT, &val);
	if (ret < 0)
		goto err;

	/* sync local flag with chip state */
	if (!(val & (MAX17332_COMMSTAT_WP_1_5_MASK | MAX17332_COMMSTAT_WP_GLOBAL_MASK)))
		dev->lock_en = false;
	else
		dev->lock_en = true;

	pr_debug("%s: CommStat : 0x%04X\n", __func__, val);

	if (lock_en != dev->lock_en) {
		dev_err(dev->dev,
			"%s: Write protection state %s not reached: req/cur: %d/%d\n",
			__func__, lock_en ? "On " : "Off", lock_en, dev->lock_en);
	}

	return 0;
err:
	pr_err("<%s> failed\n", __func__);
	return ret;
}
EXPORT_SYMBOL(max17332_lock_write_protection);

int max17332_map_irq(struct max17332_dev *max17332, int irq)
{
	return regmap_irq_get_virq(max17332->irqc_intsrc, irq);
}
EXPORT_SYMBOL_GPL(max17332_map_irq);

static int max17332_pmic_irq_int(struct max17332_dev *chip)
{
	struct device *dev = chip->dev;
	struct i2c_client *client = to_i2c_client(dev);
	int irq_flags;
	int rc = 0;

	/* disable all interrupt source */
	max17332_write_unlock(chip, chip->regmap_pmic,
			REG_PROTALRTS, 0xFFFF);

	irq_flags = irq_get_trigger_type(chip->irq);
	if (!irq_flags)
		irq_flags = IRQF_TRIGGER_FALLING;

	/* interrupt source */
	rc = regmap_add_irq_chip(chip->regmap_pmic, chip->irq,
			irq_flags | IRQF_ONESHOT |
			IRQF_SHARED, -1, &max17332_intsrc_irq_chip,
			&chip->irqc_intsrc);
	if (rc != 0) {
		dev_err(&client->dev, "failed to add insrc irq chip: %d\n",
			rc);
		goto out;
	}

	pr_info("<%s> IRQ initialize done\n", client->name);
	return 0;

out:
	return rc;
}

int max17332_get_i2c_read_fail_count () {
	return i2c_fails_count.i2c_read_fail_count;
}

int max17332_get_i2c_write_fail_count () {
	return i2c_fails_count.i2c_write_fail_count;
}

/*******************************************************************************
 *  device
 ******************************************************************************/
static int max17332_add_devices(struct max17332_dev *me,
		struct mfd_cell *cells, int n_devs)
{
	struct device *dev = me->dev;
	int rc;

	pr_info("%s: size %d\n", __func__, n_devs);
	rc = mfd_add_devices(dev, -1, cells, n_devs, NULL, 0, NULL);

	return rc;
}

/*
 * Keep charger at the last spot
 * We will have charger probing done at the last place
 * and enable the register write protection there
 */
static struct mfd_cell max17332_devices[] = {
	{ .name = MAX17332_BATTERY_NAME,	},
	{ .name = MAX17332_CHARGER_NAME,	},
#if IS_ENABLED(CONFIG_MAX17332_CDEV)
	{ .name = MAX17332_CDEV_NAME,		},
#endif
};

static int max17332_pre_init_data(struct max17332_dev *pmic)
{
	int rc = 0;

#ifdef CONFIG_OF
	int size = 0, cnt = 0;
	struct device *dev = pmic->dev;
	struct device_node *np = dev->of_node;

	u16 *init_data;

	/* unlock Write Protection */
	rc = max17332_lock_write_protection(pmic, false);
	if (rc != 0)
		goto out;

	pr_info("%s: Pmic initialize\n", __func__);
	size = device_property_read_u16_array(dev, "max17332,pmic-init", NULL, 0);
	if (size > 0) {
		init_data = kmalloc(size, GFP_KERNEL);
		of_property_read_u16_array(np, "max17332,pmic-init", init_data, size);
		for (cnt = 0; cnt < size; cnt += 2) {
			max17332_write(pmic->regmap_pmic,
				(u8)(init_data[cnt]), init_data[cnt+1]);
		}
		kfree(init_data);
	}

	pr_info("%s: nvm initialize\n", __func__);
	size = device_property_read_u16_array(dev, "max17332,nvm-init", NULL, 0);
	if (size > 0) {
		init_data = kmalloc(size, GFP_KERNEL);
		of_property_read_u16_array(np, "max17332,nvm-init", init_data, size);
		for (cnt = 0; cnt < size; cnt += 2) {
			if (((u8)(init_data[cnt]) >= REG_NROMID0_NVM) && ((u8)(init_data[cnt]) <= REG_NROMID3_NVM))
				continue;
			max17332_write(pmic->regmap_nvm,
				(u8)(init_data[cnt]), init_data[cnt+1]);
		}
		kfree(init_data);

		max17332_update_bits(pmic->regmap_pmic, REG_CONFIG2,
			MAX17332_CONFIG2_POR_CMD,
			MAX17332_CONFIG2_POR_CMD);

		// Wait 500 ms for POR_CMD to clear;
		msleep(500);
	}

	/* lock Write Protection */
	rc = max17332_lock_write_protection(pmic, true);
out:

#endif
	return rc;
}

static void *max17332_pmic_get_platdata(struct max17332_dev *pmic)
{
#ifdef CONFIG_OF
	struct device *dev = pmic->dev;
	struct i2c_client *client = to_i2c_client(dev);
	struct max17332_pmic_platform_data *pdata;
	int ret;

	pr_info("<%s>: get the pmic platform config of max17332\n", __func__);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(!pdata)) {
		pr_err("<%s> out of memory (%uB requested)\n", client->name,
				(unsigned int) sizeof(*pdata));
		pdata = ERR_PTR(-ENOMEM);
		goto out;
	}

	ret = max17332_read(pmic->regmap_nvm, REG_RSENSE_NVM, &pdata->rsense);
	if (ret < 0) {
		dev_err(dev,
				"<%s>: failed to read rsense from register %x: %d, use default value 20\n",
				__func__,
				REG_RSENSE_NVM,
				ret);
		pdata->rsense = 20;
	}

	// Only pick up the LSB of rsense
	pdata->rsense &= 0x00FF;
	dev_dbg(dev, "<%s>: rsense has been set as %d\n", __func__, pdata->rsense);

out:
	return pdata;
#else /* CONFIG_OF */
	return dev_get_platdata(pmic->dev) ?
		dev_get_platdata(pmic->dev) : ERR_PTR(-EINVAL);
#endif /* CONFIG_OF */
}

static int max17332_pmic_setup(struct max17332_dev *me)
{
	struct device *dev = me->dev;
	struct i2c_client *client = to_i2c_client(dev);
	int rc = 0;
	const struct property *property = NULL;
	const char *battery_node = NULL;
	bool is_dual_battery = false;
	char battery_name[32];
    char charger_name[32];
#if IS_ENABLED(CONFIG_MAX17332_CDEV)
    char cooler_name[32];
#endif

	pr_info("%s: max17332_pmic_irq_int\n", __func__);
	/* IRQ init */
	rc = max17332_pmic_irq_int(me);
	if (rc != 0) {
		dev_err(&client->dev, "failed to initialize irq: %d\n", rc);
		goto err_irq_init;
	}

	/*
	 * Keep adding devices as the last step as
	 * after all devices probing are done, we need to enable
	 * the write protection
	 */
	pr_info("%s: max17332_add_devices\n", __func__);

	is_dual_battery = of_property_read_bool(me->dev->of_node, "dual-battery");
    pr_info(
		"%s: is_dual_battery = %d\n",	__func__, is_dual_battery
	);

	if (is_dual_battery == true) {
		property = of_find_property(me->dev->of_node,
			"battery-node", NULL);
		if (property == NULL) {
			pr_info("%s: property battery-node not found\n",
				__func__);
			rc = -EINVAL;
			goto err_add_devices;
		}
		rc = of_property_read_string(me->dev->of_node,
			"battery-node",
			&battery_node);
		if (rc < 0) {
			pr_err("%s: property battery-node value not found\n",
					__func__);
			rc = -EINVAL;
			goto err_add_devices;
		}

		snprintf(battery_name, sizeof(battery_name), "max17332-battery-%s", battery_node);
        snprintf(charger_name, sizeof(charger_name), "max17332-charger-%s", battery_node);

		max17332_devices[0].name = battery_name;
		max17332_devices[1].name = charger_name;
#if IS_ENABLED(CONFIG_MAX17332_CDEV)
        snprintf(cooler_name, sizeof(cooler_name), "max17332-cdev-%s", battery_node);
		max17332_devices[2].name = cooler_name;
#endif
    }

	snprintf(me->battery_name, sizeof(me->battery_name), "%s", max17332_devices[0].name);
	snprintf(me->charger_name, sizeof(me->charger_name), "%s", max17332_devices[1].name);

	rc = max17332_add_devices(me, max17332_devices,
		ARRAY_SIZE(max17332_devices));
	if (rc < 0) {
		pr_err("<%s> failed to add sub-devices [%d]\n",
			client->name, rc);
		goto err_add_devices;
	}

	/* set device able to wake up system */
	device_init_wakeup(dev, true);
	enable_irq_wake((unsigned int)me->irq);

	pr_info("%s: Done\n", __func__);
	return 0;

err_add_devices:
	regmap_del_irq_chip(me->irq, me->irqc_intsrc);
err_irq_init:
	return rc;
}

/*******************************************************************************
 *** MAX17332 MFD Core
 ******************************************************************************/

static __always_inline void max17332_destroy(struct max17332_dev *me)
{
	struct device *dev = me->dev;

	mfd_remove_devices(me->dev);

	if (likely(me->irq > 0))
		regmap_del_irq_chip(me->irq, me->irqc_intsrc);

	if (likely(me->irq_gpio >= 0))
		gpio_free((unsigned int)me->irq_gpio);

#ifdef CONFIG_OF
	if (likely(me->pdata))
		devm_kfree(dev, me->pdata);
#endif /* CONFIG_OF */

	max17332_destroy_voltage_adjustment(me);

	if (likely(me->read_failure_cache))
		max17332_cache_exit(me->read_failure_cache);

	if (likely(me->regulator_dev)) {
		mutex_destroy(&me->regulator_dev->lock);
		devm_kfree(dev, me->regulator_dev);
	}

	mutex_destroy(&me->lock);
	mutex_destroy(&me->lock_write);
	devm_kfree(dev, me);
}

int max17332_raw_voltage_to_uvolts(u16 lsb)
{
	return lsb * 625 / 8; /* 78.125uV per bit */
}
EXPORT_SYMBOL(max17332_raw_voltage_to_uvolts);

int max17332_set_active_discharge(struct max17332_dev *pdev, bool enter)
{
	int rc = 0;

	mutex_lock(&pdev->regulator_dev->lock);

	if (pdev->in_active_discharge == enter) {
		goto abort;
	}

	pdev->in_active_discharge = enter;
	if (!pdev->regulator_dev->charger_bob) {
		dev_info(pdev->dev,
				"%s: No charger BOB configured, setting active discharge state",
				__func__);
		goto abort;
	}

	if (enter) {
		pm_stay_awake(pdev->dev);
		if (pdev->regulator_dev->bob_enabled) {
			rc = regulator_disable(pdev->regulator_dev->charger_bob);
			if (rc < 0)
			{
				dev_err(pdev->dev, "failed to disable BOB: %d\n", rc);
				goto abort;
			}
			pdev->regulator_dev->bob_enabled = false;
		}
	} else {
		pm_relax(pdev->dev);
		if (!pdev->regulator_dev->bob_enabled) {
			rc = regulator_enable(pdev->regulator_dev->charger_bob);
			if (rc < 0) {
				dev_err(pdev->dev, "Failed to enable BOB: %d\n", rc);
				goto abort;
			}
			pdev->regulator_dev->bob_enabled = true;
		}
	}

abort:
	mutex_unlock(&pdev->regulator_dev->lock);
	return rc;
}
EXPORT_SYMBOL(max17332_set_active_discharge);

int max17332_headroom_management(struct max17332_dev *pdev)
{
	int target_vol;
	u16 curr_vol;
	int rc = 0;
	int vout_delta = pdev->regulator_dev->vout_delta;

	if (atomic_read(&pdev->charge_throttled)) {
		dev_dbg(pdev->dev,
				"%s: charge throttled, skipping headroom management",
				__func__);
		return 0;
	}
#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	vout_delta = (pdev->regulator_dev->custom_headroom_mv > INVALID_CUSTOM_HEADROOM) ? pdev->regulator_dev->custom_headroom_mv * 1000 : vout_delta;
#endif

	mutex_lock(&pdev->regulator_dev->lock);

	if (pdev->in_active_discharge) {
		dev_err(pdev->dev,
				"%s: in active discharge, cannot do headroom management",
				__func__);
		goto abort;
	}

	if (!pdev->regulator_dev->charger_bob) {
		dev_err(pdev->dev,
				"%s: No charger BOB configured, cannot do headroom management",
				__func__);
		goto abort;
	}

	rc = regulator_is_enabled(pdev->regulator_dev->charger_bob);
	if (rc < 0) {
		dev_err(pdev->dev, "Failed to get enabled state of the regulator");
		goto abort;
	} else if (rc == 0) {
		dev_dbg(pdev->dev, "Regulator is disabled, do not attempt headroom management");
		goto abort;
	}

	rc = max17332_adjust_vsys_voltage(pdev);
	if (rc)
		goto abort;

	rc = max17332_read(pdev->regmap_pmic, REG_VCELLREP, &curr_vol);
	if (rc < 0) {
		dev_err(pdev->dev, "Failed to read REG_VCELLREP: %d\n", rc);
		goto abort;
	}

	target_vol = max17332_raw_voltage_to_uvolts(curr_vol) + vout_delta;
	rc = max17332_set_vsys_voltage(pdev, target_vol);

abort:
	mutex_unlock(&pdev->regulator_dev->lock);
	return rc;
}
EXPORT_SYMBOL(max17332_headroom_management);

int max17332_get_charger_bob(struct max17332_dev *pdev)
{
	int rc = 0;

	mutex_lock(&pdev->regulator_dev->lock);

	if (!pdev->regulator_dev->charger_bob) {
		// Charger might already exist, so try to get it
		pdev->regulator_dev->charger_bob = regulator_get_optional(pdev->dev, "vout");

		if (IS_ERR(pdev->regulator_dev->charger_bob)) {
			rc = PTR_ERR(pdev->regulator_dev->charger_bob);
			pdev->regulator_dev->charger_bob = NULL;
			dev_err(pdev->dev, "Failed to get charger BOB: %d\n", rc);
			goto abort;
		}

		rc = regulator_get_linear_step(pdev->regulator_dev->charger_bob);
		if (rc == 0) {
			dev_err(pdev->dev, "Charger BOB is not a linear regulator\n");
			rc = -EOPNOTSUPP;
			goto abort;
		}
		pdev->regulator_dev->vout_step = rc;
		dev_dbg(pdev->dev, "Charger BOB voltage step is %d uV\n", rc);

		rc = of_property_read_u32(pdev->dev->of_node, "max17332,max-bob-voltage-uv",
					  &pdev->regulator_dev->vout_max);
		if (rc) {
			dev_err(pdev->dev, "Failed to read the BOB max voltage: %d\n", rc);
			goto abort;
		}
		dev_dbg(pdev->dev, "Charger BOB max voltage is %d uV\n",
			pdev->regulator_dev->vout_max);

		// read voltage chg delta from device tree
		rc = of_property_read_u32(pdev->dev->of_node, "max17332,bob-voltage-delta-uv",
						&pdev->regulator_dev->vout_delta);
		if (rc) {
			pr_err("Failed to read the BOB voltage delta: %d\n", rc);
			// do not abort, the default value will be used
		}
		rc = of_property_read_u32(pdev->dev->of_node, "max17332,bob-voltage-round-up", &pdev->regulator_dev->round_up);
		if (rc) {
			pr_err("Did not find round up dt node, will round down\n");
			// do not abort, defaulting to rounding down
		}
		rc = of_property_read_u32(pdev->dev->of_node, "max17332,min-bob-voltage-uv",
					  &pdev->regulator_dev->vout_min);
		if (rc) {
			dev_err(pdev->dev, "Failed to read the BOB min voltage: %d\n", rc);
			// do not abort, will not use min voltage value
		}
		dev_err(pdev->dev, "Charger BOB min voltage is %d uV\n",
			pdev->regulator_dev->vout_min);
	}

	// Charger might be in disabled state.
	// For example, after disabling it, power is cut off, and charger BOB is disabled.
	// So when the power is reconnected, we need to enable it.
	if (!pdev->regulator_dev->bob_enabled) {
		rc = regulator_enable(pdev->regulator_dev->charger_bob);
		if (rc < 0) {
			dev_err(pdev->dev, "Failed to enable charger BOB: %d\n", rc);
			goto abort;
		}
		pdev->regulator_dev->bob_enabled = true;
	}

	if (pdev->in_active_discharge) {
		rc = regulator_disable(pdev->regulator_dev->charger_bob);
		if (rc < 0) {
			dev_err(pdev->dev, "Failed to enable charger BOB: %d\n", rc);
			goto abort;
		}
		rc = -1;
		pdev->regulator_dev->bob_enabled = false;
		dev_err(pdev->dev,
				"%s: in active discharge, disabling charger bob",
				__func__);
	}

abort:
	mutex_unlock(&pdev->regulator_dev->lock);
	return rc;
}
EXPORT_SYMBOL(max17332_get_charger_bob);

bool is_max17332_charger_bob_active(struct max17332_dev *pdev)
{
	if (IS_ERR_OR_NULL(pdev->regulator_dev->charger_bob))
		return false;
	if (regulator_is_enabled(pdev->regulator_dev->charger_bob) <= 0)
		return false;
	return pdev->regulator_dev->bob_enabled;
}
EXPORT_SYMBOL(is_max17332_charger_bob_active);

int max17332_overcharge_protection(struct max17332_dev *pdev, int ocv_threshold_uV)
{
	int rc = 0;
	int i;
	u16 val;
	int ocv;

	mutex_lock(&pdev->regulator_dev->lock);

	if (pdev->in_active_discharge) {
		dev_err(pdev->dev,
				"%s: in active discharge, cannot do overcharge protection",
				__func__);
		goto abort;
	}

	rc = max17332_read(pdev->regmap_pmic, REG_VFOCV, &val);
	if (rc == 0) {
		ocv = max17332_raw_voltage_to_uvolts(val);
	} else {
		dev_err(pdev->dev, "failed to read VFOCV register: %d\n", rc);
		ocv = ocv_threshold_uV;
	}

	/* Only perform overcharge mitigation by disabling the charger BOB if the
	 * battery OCV is below the OCV threshold
	 */
	if (ocv <= ocv_threshold_uV) {
		if (!pdev->regulator_dev->charger_bob) {
			dev_err(pdev->dev, "no available charger BOB, cannot do overcharge protection\n");
			goto abort;
		}

		if (pdev->regulator_dev->bob_enabled) {
			rc = regulator_disable(pdev->regulator_dev->charger_bob);
			if (rc < 0) {
				dev_err(pdev->dev, "failed to disable BOB: %d\n", rc);
				goto abort;
			}
			pdev->regulator_dev->bob_enabled = false;
		}

		// Wait for OVP/OCCP status is cleared
		for (i = 0; i < MAX_NUM_BATT_DISCHRG_CHECKS; i++) {
			rc = max17332_read(pdev->regmap_pmic, REG_PROTSTATUS, &val);
			if (rc < 0) {
				dev_err(pdev->dev, "failed to read PROTSTATUS register: %d\n", rc);
				continue;
			}

			if (!(val & (BIT_OVP_INT | BIT_OCCP_INT))) {
				dev_info(pdev->dev, "OVP and OCCP of BOB has been cleared\n");
				break;
			}

			dev_dbg(pdev->dev, "OVP or OCCP of BOB has not been cleared yet\n");
			msleep(BATT_DISCHRG_WAIT_TIME_MS);
		}

		if (i >= MAX_NUM_BATT_DISCHRG_CHECKS) {
			dev_err(pdev->dev,
					"OVP/OCCP of BOB has not been cleared after waiting for %dms, "
					"stop waiting and re-enable the charger bob\n",
					BATT_DISCHRG_WAIT_TIME_MS * MAX_NUM_BATT_DISCHRG_CHECKS);
		}

		if (!pdev->regulator_dev->bob_enabled) {
			rc = regulator_enable(pdev->regulator_dev->charger_bob);
			if (rc < 0) {
				dev_err(pdev->dev, "failed to enable BOB: %d\n", rc);
				goto abort;
			}
			pdev->regulator_dev->bob_enabled = true;
		}

		dev_info(pdev->dev, "BOB has been re-enabled\n");
	}

abort:
	mutex_unlock(&pdev->regulator_dev->lock);
	return rc;
}
EXPORT_SYMBOL(max17332_overcharge_protection);

int max17332_set_vsys_voltage(struct max17332_dev *pdev, int target_vol)
{
	int ret;
	int vout_step = pdev->regulator_dev->vout_step;
	int vout_max = pdev->regulator_dev->vout_max;

	if (pdev->regulator_dev->round_up) {
		target_vol = DIV_ROUND_UP_ULL(target_vol, vout_step) * vout_step;

		// Round up to minimum voltage if round up and min voltage were set
		if (pdev->regulator_dev->vout_min) {
			if (target_vol < pdev->regulator_dev->vout_min) {
				target_vol = pdev->regulator_dev->vout_min;
			}
		}
	} else {
		target_vol = DIV_ROUND_DOWN_ULL(target_vol, vout_step) * vout_step;
	}
	if (target_vol > vout_max)
		target_vol = vout_max;

	ret = regulator_set_voltage(pdev->regulator_dev->charger_bob, target_vol, target_vol);
	if (ret < 0) {
		dev_err(pdev->dev, "Failed to set vout of BOB to %d: %d\n",
				target_vol, ret);
	} else {
		dev_dbg(pdev->dev, "vout of BOB has been changed to %d\n",
				target_vol);
		max17332_set_vsys_change_time(pdev, jiffies);
	}
	return ret;
}

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
static ssize_t
custom_headroom_mv_show(struct device *dev, struct device_attribute *attr,
    char *buf)
{
	struct max17332_dev *me;
	me = dev_get_drvdata(dev);
	return sysfs_emit(buf, "%d\n", me->regulator_dev->custom_headroom_mv);
}

static ssize_t
custom_headroom_mv_store(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count)
{
	struct max17332_dev *me;
	int ret;
	int custom_headroom_mv = -1;

	me = dev_get_drvdata(dev);
	ret = kstrtos32(buf, 10, &custom_headroom_mv);
	if (ret < 0) {
		dev_err(dev, "Failed to read custom_headroom_mv from buffer: %d\n", ret);

		// Intentionally return 'count' so this update won't keep being retried
		return count;
	}
	if (custom_headroom_mv < INVALID_CUSTOM_HEADROOM) {
		dev_err(dev, "Invalid custom headroom, storing default value\n");
		return count;
	}

	me->regulator_dev->custom_headroom_mv = custom_headroom_mv;
	return count;
}
static DEVICE_ATTR_RW(custom_headroom_mv);

static struct attribute *attributes[] = {
  &dev_attr_custom_headroom_mv.attr,
  NULL,
};

static const struct attribute_group attr_group = {
	.name = "headroom-control",
	.attrs = attributes
};
#endif

static int max17332_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct max17332_dev *me;
	int rc;
	u8 i2c_nvm_addr;
	u16 read_val = 0;

	pr_info("%s: Max17332 I2C Driver Loading\n", __func__);

	rc = i2c_smbus_read_word_data(client, REG_DEVNAME);
	if (rc < 0) {
		/*
		 * We should detect potential bus error before calling
		 * devm_i2c_new_dummy_device(), which registers a new platform
		 * device. Otherwise returing -EPROBE_DEFER will cause
		 * infinite probe loop if the bus or fg never becomes available.
		 */
		pr_warn("%s: failed to read DEVNAME register, deferring probe, rc=%d\n",
			__func__, rc);
		return -EPROBE_DEFER;
	}

	pr_info("%s: DEVNAME reg val=0x%x (default=0x%x)\n", __func__, rc,
		DEVNAME_DEFAULT_VAL);

	me = devm_kzalloc(&client->dev, sizeof(*me), GFP_KERNEL);
	if (unlikely(!me)) {
		pr_err("<%s> out of memory (%uB requested)\n",
				client->name,
				(unsigned int) sizeof(*me));
		return -ENOMEM;
	}

	me->regulator_dev = devm_kzalloc(&client->dev, sizeof(*me->regulator_dev), GFP_KERNEL);
	if (unlikely(!me->regulator_dev)) {
		pr_err("<%s> out of memory (%uB requested)\n",
				client->name,
				(unsigned int) sizeof(*me->regulator_dev));
		return -ENOMEM;
	}

	i2c_set_clientdata(client, me);

	mutex_init(&me->lock);
	mutex_init(&me->lock_write);
	mutex_init(&me->regulator_dev->lock);
	me->regulator_dev->bob_enabled = false;
	me->regulator_dev->charger_bob = NULL;
	me->regulator_dev->vout_delta = BOB_INIT_VOLTAGE_CHG_DELTA_DEFAULT;
#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	me->regulator_dev->custom_headroom_mv = INVALID_CUSTOM_HEADROOM;
#endif
	me->dev      = &client->dev;
	me->irq      = client->irq;
	me->irq_gpio = -1;
	/* start with write ptotection on */
	me->lock_en = true;

	me->pmic = client;

	me->regmap_pmic = devm_regmap_init_i2c(client, &max17332_regmap_config);
	if (IS_ERR(me->regmap_pmic)) {
		rc = PTR_ERR(me->regmap_pmic);
		me->regmap_pmic = NULL;
		pr_err("<%s> failed to initialize i2c\n",
			client->name);
		pr_err("<%s> regmap pmic [%d]\n",
			client->name,	rc);
		goto abort;
	}

	rc = of_property_read_u8(me->dev->of_node, I2C_NVM_ADDR, &i2c_nvm_addr);
	if (rc < 0) {
		pr_err("<%s>: failed to get the address to access nvm registers [%d]\n",
				client->name, rc);
		goto abort;
	}

	me->nvm = devm_i2c_new_dummy_device(&client->dev, client->adapter, i2c_nvm_addr);
	if (IS_ERR(me->nvm)) {
		rc = PTR_ERR(me->nvm);
		goto abort;
	}

	i2c_set_clientdata(me->nvm, me);
	me->regmap_nvm = devm_regmap_init_i2c(me->nvm, &max17332_regmap_config_nvm);
	if (IS_ERR(me->regmap_nvm)) {
		rc = PTR_ERR(me->regmap_nvm);
		me->regmap_nvm = NULL;
		pr_err("<%s> failed to initialize i2c\n",
			client->name);
		pr_err("<%s> regmap nvm [%d]\n",
			client->name,	rc);
		goto abort;
	}

	me->pdata = max17332_pmic_get_platdata(me);
	if (IS_ERR(me->pdata)) {
		rc = PTR_ERR(me->pdata);
		me->pdata = NULL;
		pr_err("<%s> platform data is missing [%d]\n",
			client->name, rc);
		goto abort;
	}

	rc = max17332_pre_init_data(me);
	if (rc != 0)
		goto abort;

	me->read_failure_cache = max17332_cache_init(&read_failure_cache_config);
	if (IS_ERR(me->read_failure_cache)) {
		rc = PTR_ERR(me->read_failure_cache);
		me->read_failure_cache = NULL;
		pr_err("<%s> Failed to initialize read-failure cache\n", client->name);
		pr_err("<%s> max17332-cache init [%d]\n", client->name, rc);
		goto abort;
	}

	max17332_init_voltage_adjustment(me);

	/*
	 * Keep pmic setup as the last step of probing as it needs to
	 * add sub-devices and sub-devices need to refer to the data of
	 * the main device
	 */
	rc = max17332_pmic_setup(me);
	if (rc != 0) {
		pr_err("<%s> failed to set up interrupt\n",
			client->name);
		pr_err("<%s> and add sub-device [%d]\n",
			client->name,	rc);
		goto abort;
	}
	if (of_property_read_bool(me->dev->of_node, "max17332,force-enable-cmd-override")) {
		rc = max17332_read(me->regmap_pmic, REG_NPROTCFG, &read_val);
		if (rc == 0 | rc > 0) {
			read_val &= ~MAX17332_NPROTCFG_CMOVRDEN;
			read_val |= (1 << MAX17332_NPROTCFG_CMOVRDEN_POS);
			rc = max17332_write_unlock(me, me->regmap_pmic, REG_NPROTCFG, read_val);
			if (rc < 0) {
				pr_err("%s : Failed to write REG_NPROTCFG\n", __func__);
				goto abort;
			}
		} else {
			pr_err("%s : Failed to read REG_NPROTCFG\n", __func__);
			goto abort;
		}
	}

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	rc = sysfs_create_group(&me->dev->kobj, &attr_group);
	if (rc < 0) {
		dev_err(me->dev, "%s : Failed to expose custom headroom node to sysfs: %d\n",
				__func__, rc);
		goto abort;
	}
#endif

	pr_info("%s: Done\n", __func__);
	return 0;
abort:
	/*
	 * Failed to initialize i2c while probing, but after a successful
	 * DEVNAME register read. This can happen the bus becomes unavailable
	 * shortly after the DEVNAME register read. Defer the probe so
	 * this probe function to be called immediately after exit. If the
	 * inaccessible bus persists, the DEVNAME register-read catches and
	 * bails out to avoid probe hang.
	 */
	pr_err("%s: Error occured rc=%d, deferring probe\n", __func__, rc);

	i2c_set_clientdata(client, NULL);
	max17332_destroy(me);
	return -EPROBE_DEFER;
}

static int max17332_i2c_remove(struct i2c_client *client)
{
	struct max17332_dev *me = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);

	max17332_destroy(me);
#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	sysfs_remove_group(&me->dev->kobj, &attr_group);
#endif

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max17332_suspend(struct device *dev)
{
	struct max17332_dev *me = dev_get_drvdata(dev);
	struct i2c_client *client = to_i2c_client(dev);

	__lock(me);

	pr_debug("<%s> suspending\n", client->name);
	if (device_may_wakeup(dev))
		enable_irq_wake(me->irq);

	disable_irq(me->irq);

	__unlock(me);
	return 0;
}

static int max17332_resume(struct device *dev)
{
	struct max17332_dev *me = dev_get_drvdata(dev);
	struct i2c_client *client = to_i2c_client(dev);

	__lock(me);

	pr_debug("<%s> resuming\n", client->name);
	if (device_may_wakeup(dev))
		disable_irq_wake(me->irq);

	enable_irq(me->irq);

	__unlock(me);
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(max17332_pm, max17332_suspend, max17332_resume);

#ifdef CONFIG_OF
static const struct of_device_id max17332_of_id[] = {
	{ .compatible = "maxim,max17332"},
	{ },
};
MODULE_DEVICE_TABLE(of, max17332_of_id);
#endif /* CONFIG_OF */

static const struct i2c_device_id max17332_i2c_id[] = {
	{ MAX17332_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max17332_i2c_id);

static struct i2c_driver max17332_i2c_driver = {
	.driver.name            = DRIVER_NAME,
	.driver.owner           = THIS_MODULE,
	.driver.pm              = &max17332_pm,
#ifdef CONFIG_OF
	.driver.of_match_table  = max17332_of_id,
#endif /* CONFIG_OF */
	.id_table               = max17332_i2c_id,
	.probe                  = max17332_i2c_probe,
	.remove                 = max17332_i2c_remove,
};

static __init int max17332_init(void)
{
	int rc = -ENODEV;

	rc = i2c_add_driver(&max17332_i2c_driver);
	if (rc != 0)
		pr_err("Failed to register I2C driver: %d\n", rc);
	pr_info("%s: Added I2C Driver\n", __func__);
	return rc;
}
module_init(max17332_init);

static __exit void max17332_exit(void)
{
	i2c_del_driver(&max17332_i2c_driver);
}
module_exit(max17332_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);
