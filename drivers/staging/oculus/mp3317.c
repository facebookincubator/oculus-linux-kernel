// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
#include <linux/math.h>
#endif

#define MODE_CTRL_CURR_LIM_REG 0x00
#define CHEN_REG 0x01
#define OVP_IMAX_REG 0x02
#define TPWM_REG 0x03
#define ILEDL_L_REG 0x04
#define ILEDL_H_REG 0x05
#define ILEDR_L_REG 0x06
#define ILEDR_H_REG 0x07
#define VOUT_REG 0x08
#define FREQ_SW_REG 0x09
#define HEADROOM_VSTEP_REG 0x0A
#define FAULT0_REG 0x0B
#define FAULT1_REG 0x0C
#define ID_REG 0x0D

#define ILED_FULL_SCALE 0xFFF
#define NUM_IMAX_VALUES 8
#define NUM_REGISTERS 14
#define MIN_REGISTER MODE_CTRL_CURR_LIM_REG
#define MAX_REGISTER ID_REG
#define MAX_WRITEABLE_REGISTER HEADROOM_VSTEP_REG

#define EN_MASK 0x80
#define ILED_SETL_VAL(_iled) (_iled & 0xff)
#define ILED_SETH_VAL(_iled) ((_iled & 0xfff) >> 8)
#define IMAX_BITMASK 0x7
#define ISET_EN(_val) (_val & 0x40)

#define EN_GPIO_DELAY_US (2000)

/* MP3317  - 60K */
#define ISET_RESISTOR_MAX (60000)

#define MP3317_REGULATOR_ID_LEFT	0
#define MP3317_REGULATOR_ID_RIGHT	1
#define MP3317_MAX_REGULATORS		2
#define LEFT_NAME  "ledl"
#define RIGHT_NAME "ledr"

/* FAULT_1 (0x0B) */
#define FT_LEDG BIT(6)
#define FT_OPT BIT(5)
#define FT_OCP_CBC BIT(4)
#define FT_OCP_IN BIT(3)
#define FT_OVP BIT(2)
#define FT_LEDO BIT(1)
#define FT_LEDS BIT(0)

/* FAULT_2 (0x0B) */
#define FT_OCLATCH BIT(2)
#define FT_ISETO BIT(1)
#define FT_ISETS BIT(0)

static const char *mp3317_register_names[NUM_REGISTERS] = {
	"MODE_CL",
	"EN_CTRL",
	"OVP_IMAX",
	"TPWM",
	"ILEDL_L",
	"ILEDL_H",
	"ILEDR_L",
	"ILEDR_H",
	"VOUT",
	"HR_TH",
	"STEP_HR",
	"FAULT_1",
	"FAULT_2",
	"ID",
};

static bool mp3317_readable(struct device *dev, unsigned int reg)
{
	return reg >= MIN_REGISTER && reg <= MAX_REGISTER;
}

static bool mp3317_volatile(struct device *dev, unsigned int reg)
{
	return reg == FAULT0_REG || reg == FAULT1_REG;
}

static bool mp3317_writeable(struct device *dev, unsigned int reg)
{
	return reg >= MIN_REGISTER && reg <= MAX_WRITEABLE_REGISTER;
}

static const struct reg_default mp3317_reg_defs[NUM_REGISTERS] = {
	{ MODE_CTRL_CURR_LIM_REG, 0x95 },
	{ CHEN_REG, 0xFF },
	{ OVP_IMAX_REG, 0x7B },
	{ TPWM_REG, 0x08 },
	{ ILEDL_L_REG, 0xFF },
	{ ILEDL_H_REG, 0x0F },
	{ ILEDR_L_REG, 0xFF },
	{ ILEDR_H_REG, 0x0F },
	{ VOUT_REG, 0x68 },
	{ FREQ_SW_REG, 0xF9 },
	{ HEADROOM_VSTEP_REG, 0x41 },
	{ FAULT0_REG, 0x00 },
	{ FAULT1_REG, 0x00 },
	{ ID_REG, 0x11 },
};

/* IMAX Values(uA) */
static const unsigned int imax_values[NUM_IMAX_VALUES] = {
	5000, 10000, 20000, 40000, 80000, 160000, 160000, 160000,
};

struct mp3317_priv {
	struct device *dev;
	struct regmap *regmap;

	/* LEDL/LEDR channel enable */
	atomic_t ch_enabled[MP3317_MAX_REGULATORS];

	/* Enable GPIO */
	int en_gpio;

	/* Bootloader enabled regulator */
	bool boot_on;
	/* External resistor value */
	unsigned int iset_ext_resistor;
	/* IMAX values based on external resistor when ISET is enabled */
	unsigned int imax_ua[NUM_IMAX_VALUES];
};

static const struct regmap_config mp3317_regmap_config = {
	.name = "mp3317",
	.reg_bits = 8,
	.val_bits = 8,
	.reg_stride = 1,
	.max_register = MAX_REGISTER,
	.readable_reg = mp3317_readable,
	.volatile_reg = mp3317_volatile,
	.writeable_reg = mp3317_writeable,
	.cache_type = REGCACHE_FLAT,
	.use_single_read = true,
	.use_single_write = true,
	.reg_defaults = mp3317_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(mp3317_reg_defs),
};

static int parse_dt(struct device *dev, struct mp3317_priv *bld,
		    struct reg_default *initial_regs,
		    unsigned int *num_initial_regs)
{
	unsigned char default_reg_values[NUM_REGISTERS * 2];
	int i, num_defaults;

	if (!dev->of_node) {
		dev_err(dev, "%s: No device tree found\n", __func__);
		return -EINVAL;
	}

	bld->en_gpio = of_get_named_gpio(dev->of_node, "enable-gpio", 0);
	if (gpio_is_valid(bld->en_gpio)) {
		if (devm_gpio_request(dev, bld->en_gpio, "mp3317-enable-gpio"))
			dev_err(dev, "%s: Failed to request enable GPIO\n",
				__func__);
	} else
		dev_warn(dev, "%s: No enable GPIO defined\n", __func__);

	if (of_property_read_u32(dev->of_node, "iset-ext-resistor",
				 &bld->iset_ext_resistor) < 0) {
		dev_err(dev, "%s: Can not find iset-ext-resistor value\n",
			__func__);
		return -EINVAL;
	}

	bld->boot_on = of_property_read_bool(dev->of_node, "regulator-boot-on");
	for (i = 0; i < MP3317_MAX_REGULATORS; ++i)
		atomic_set(&bld->ch_enabled[i], bld->boot_on);

	*num_initial_regs = 0;
	num_defaults = of_property_read_variable_u8_array(dev->of_node,
							  "regs-defaults",
							  default_reg_values, 2,
							  NUM_REGISTERS * 2);
	if (num_defaults > 0) {
		num_defaults /= 2;
		for (i = 0; i < num_defaults; ++i) {
			initial_regs[i].reg = default_reg_values[i * 2];
			initial_regs[i].def = default_reg_values[i * 2 + 1];
		}
		*num_initial_regs = num_defaults;
	}

	return 0;
}

static void mp3317_read_initial_state(struct mp3317_priv *bld)
{
	unsigned int initial_state[NUM_REGISTERS];
	unsigned int reg, reg_val;

	for (reg = 0; reg < NUM_REGISTERS; reg++) {
		if (!mp3317_readable(NULL, reg) || mp3317_volatile(NULL, reg))
			continue;

		/*
		 * Bypass cached defaults in case the bootloader configured
		 * the chip before the kernel and read in the current values
		 * directly from the hardware instead.
		 */
		regcache_cache_bypass(bld->regmap, true);
		regmap_read(bld->regmap, reg, &reg_val);
		initial_state[reg] = reg_val;
		regcache_cache_bypass(bld->regmap, false);
	}

	/*
	 * Write back initial hardware state into cache only since the hardware
	 * already has these values, there is no need to write through.
	 */
	regcache_cache_only(bld->regmap, true);
	for (reg = 0; reg < NUM_REGISTERS; reg++) {
		if (!mp3317_writeable(NULL, reg) || mp3317_volatile(NULL, reg))
			continue;

		regmap_write(bld->regmap, reg, initial_state[reg]);
	}
	regcache_cache_only(bld->regmap, false);
}

static void mp3317_set_initial_state_from_dt(struct mp3317_priv *bld,
					     struct reg_default *initial_regs,
					     unsigned int num_initial_regs)
{
	int i;

	/*
	 * Write initial state to cache only, since we are not enabled at
	 * time of probe. Register state will be synchronized when the
	 * regulator is enabled.
	 */
	for (i = 0; i < num_initial_regs; i++) {
		regcache_cache_only(bld->regmap, true);
		regmap_write(bld->regmap, initial_regs[i].reg,
			     initial_regs[i].def);
		regcache_cache_only(bld->regmap, false);
	}
}

static int mp3317_enable(struct regulator_dev *rdev)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	int id = rdev->desc->id;
	int rc = 0;

	dev_dbg(bld->dev, "%s(id: %d)\n", __func__, rdev->desc->id);

	atomic_set(&bld->ch_enabled[id], 1);

	/* Enable chip only when both regulators are up */
	if (atomic_read(&bld->ch_enabled[MP3317_REGULATOR_ID_LEFT]) &&
		atomic_read(&bld->ch_enabled[MP3317_REGULATOR_ID_RIGHT])) {
		if (gpio_is_valid(bld->en_gpio)) {
			gpio_direction_output(bld->en_gpio, 1);
			usleep_range(EN_GPIO_DELAY_US, EN_GPIO_DELAY_US + 1000);
		}

		/* Take cache out of cache-only mode since HW is now on */
		regcache_cache_only(bld->regmap, false);
		rc = regcache_sync(bld->regmap);
		if (rc < 0) {
			dev_err(bld->dev,
				"%s: Failed to sync register state, ret=%d\n", __func__,
				rc);
			return rc;
		}

		rc = regmap_update_bits(bld->regmap, CHEN_REG, EN_MASK, EN_MASK);
		if (rc < 0) {
			dev_err(bld->dev, "%s: Failed to write reg 0x%x ret=%d\n",
				__func__, CHEN_REG, rc);
		}
	}

	return rc;
}

static int mp3317_disable(struct regulator_dev *rdev)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	int rc = 0;
	int id = rdev->desc->id;

	dev_dbg(bld->dev, "%s(id: %d)\n", __func__, rdev->desc->id);

	atomic_set(&bld->ch_enabled[id], 0);

	/* Disable chip only when both regulators are down */
	if (!atomic_read(&bld->ch_enabled[MP3317_REGULATOR_ID_LEFT]) &&
		!atomic_read(&bld->ch_enabled[MP3317_REGULATOR_ID_RIGHT])) {
		rc = regmap_update_bits(bld->regmap, CHEN_REG, EN_MASK, 0);
		if (rc < 0) {
			dev_err(bld->dev, "%s: Failed to write reg 0x%x ret=%d\n",
				__func__, CHEN_REG, rc);
		}

		/* Put cache into cache-only mode since HW will be off */
		regcache_cache_only(bld->regmap, true);
		regcache_mark_dirty(bld->regmap);

		if (gpio_is_valid(bld->en_gpio))
			gpio_direction_output(bld->en_gpio, 0);
	}

	return rc;
}

static int mp3317_is_enabled(struct regulator_dev *rdev)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	int id = rdev->desc->id;
	int enabled;

	enabled = atomic_read(&bld->ch_enabled[id]);
	dev_dbg(bld->dev, "%s(id: %d) -> %d\n", __func__, rdev->desc->id, enabled);

	return enabled;
}

static int mp3317_set_load(struct regulator_dev *rdev, int uA_load)
{
	struct mp3317_priv *bld = rdev_get_drvdata(rdev);
	int i, rc, imax_reg;
	unsigned int reg_val, imax, target_iled;

	dev_dbg(bld->dev, "%s(id: %d, uA_load: %d)\n", __func__, rdev->desc->id, uA_load);

	/* Check for if ISET_EN is set */
	regmap_read(bld->regmap, CHEN_REG, &reg_val);

	if (ISET_EN(reg_val)) {
		/* Find imax reg setting for given load */
		for (i = 0; i < NUM_IMAX_VALUES; ++i) {
			if (uA_load < bld->imax_ua[i])
				break;
		}

		if (i == NUM_IMAX_VALUES) {
			dev_err(bld->dev,
				"%s: target current %d uA exceeds the device capability\n",
				__func__, uA_load);
			return -1;
		}

		imax_reg = i;
		imax = bld->imax_ua[imax_reg];
	} else {
		imax_reg = NUM_IMAX_VALUES - 1;
		imax = imax_values[imax_reg];
	}

	target_iled = ILED_FULL_SCALE * uA_load / imax;

	rc = regmap_update_bits(bld->regmap, OVP_IMAX_REG, IMAX_BITMASK,
				imax_reg);
	if (rc < 0) {
		dev_err(bld->dev,
			"%s: Failed to write current to OVP_IMAX_REG ret=%d\n",
			__func__, rc);
	}
	if (rdev->desc->id == MP3317_REGULATOR_ID_LEFT) {
		rc = regmap_write(bld->regmap, ILEDL_L_REG, ILED_SETL_VAL(target_iled));
		if (rc < 0) {
			dev_err(bld->dev,
				"%s: Failed to write current to ILEDL_L_REG ret=%d\n",
				__func__, rc);
		}

		rc |= regmap_write(bld->regmap, ILEDL_H_REG,
			   ILED_SETH_VAL(target_iled));
		if (rc < 0) {
			dev_err(bld->dev,
				"%s: Failed to write current to ILEDL_H_REG ret=%d\n",
				__func__, rc);
		}
	}

	if (rdev->desc->id == MP3317_REGULATOR_ID_RIGHT) {
		rc = regmap_write(bld->regmap, ILEDR_L_REG, ILED_SETL_VAL(target_iled));
		if (rc < 0) {
			dev_err(bld->dev,
				"%s: Failed to write current to ILEDR_L_REG ret=%d\n",
				__func__, rc);
		}

		rc |= regmap_write(bld->regmap, ILEDR_H_REG,
			   ILED_SETH_VAL(target_iled));
		if (rc < 0) {
			dev_err(bld->dev,
				"%s: Failed to write current to ILEDR_H_REG ret=%d\n",
				__func__, rc);
		}
	}

	return rc;
}

static struct regulator_ops mp3317_reg_ops = {
	.enable = mp3317_enable,
	.disable = mp3317_disable,
	.is_enabled = mp3317_is_enabled,
	.set_load = mp3317_set_load,
};

#define MP3317_REGULATOR_DESC(_id, _name)		\
	[MP3317_REGULATOR_ID_##_id] = {		\
		.name = "mp3317-"#_name,		\
		.supply_name = "led",			\
		.id = MP3317_REGULATOR_ID_##_id,	\
		.of_match = of_match_ptr(#_name),	\
		.ops = &mp3317_reg_ops,		\
		.type = REGULATOR_CURRENT,		\
		.owner = THIS_MODULE,			\
	}

static const struct regulator_desc mp3317_reg_desc[MP3317_MAX_REGULATORS] = {
	MP3317_REGULATOR_DESC(LEFT, ledl),
	MP3317_REGULATOR_DESC(RIGHT, ledr),
};

static ssize_t dbg_regs_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct mp3317_priv *bld = file->private_data;
	const int bufsz = SZ_4K;
	char *buf;
	unsigned int reg_val;
	int i, rc, pos = 0;

	buf = kzalloc(bufsz, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < NUM_REGISTERS; i++) {
		regmap_read(bld->regmap, i, &reg_val);
		pos += scnprintf(buf + pos, bufsz - pos, "%2d %-9s: 0x%02x\n", i,
			mp3317_register_names[i], reg_val);
	}

	rc = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	kfree(buf);
	return rc;
}

static ssize_t dbg_faults_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct mp3317_priv *bld = file->private_data;
	const int bufsz = SZ_4K;
	char *buf;
	unsigned int fault0, fault1;
	int rc, pos = 0;

	buf = kzalloc(bufsz, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	regmap_read(bld->regmap, FAULT0_REG, &fault0);
	regmap_read(bld->regmap, FAULT1_REG, &fault1);

#define CHECK_FAULT(reg, fault) do { \
	if ((reg) & (fault)) \
		pos += scnprintf(buf + pos, bufsz - pos, "%s ", (#fault)); \
} while (0)

	CHECK_FAULT(fault0, FT_LEDG);
	CHECK_FAULT(fault0, FT_OPT);
	CHECK_FAULT(fault0, FT_OCP_CBC);
	CHECK_FAULT(fault0, FT_OCP_IN);
	CHECK_FAULT(fault0, FT_OVP);
	CHECK_FAULT(fault0, FT_LEDO);
	CHECK_FAULT(fault0, FT_LEDS);

	CHECK_FAULT(fault1, FT_OCLATCH);
	CHECK_FAULT(fault1, FT_ISETO);
	CHECK_FAULT(fault1, FT_ISETS);

#undef CHECK_FAULT

	pos += scnprintf(buf + pos, bufsz - pos, "\n");

	rc = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	kfree(buf);
	return rc;
}

static const struct file_operations dbg_regs_fops = {
	.open		= simple_open,
	.llseek		= default_llseek,
	.read		= dbg_regs_read,
};

static const struct file_operations dbg_faults_fops = {
	.open		= simple_open,
	.llseek		= default_llseek,
	.read		= dbg_faults_read,
};

static int mp3317_setup_debugfs(struct mp3317_priv *bld)
{
	struct dentry *dbg;

	dbg = debugfs_create_dir("mp3317", NULL);
	if (!dbg)
		return -1;

	debugfs_create_file("regs", 0444,
						dbg, bld,
						&dbg_regs_fops);

	debugfs_create_file("faults", 0444,
						dbg, bld,
						&dbg_faults_fops);

	return 0;
}

static int mp3317_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	int i, rc = 0;
	struct mp3317_priv *bld;
	struct regulator_config reg_cfg = {};
	struct regulator_dev *reg_dev;
	struct reg_default initial_regs[NUM_REGISTERS];
	unsigned int num_initial_regs = 0, imult;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "No I2C functionality present\n");
		return -ENODEV;
	}

	bld = devm_kzalloc(dev, sizeof(struct mp3317_priv), GFP_KERNEL);
	if (!bld)
		return -ENOMEM;

	bld->dev = dev;
	i2c_set_clientdata(i2c, bld);

	rc = parse_dt(dev, bld, initial_regs, &num_initial_regs);
	if (rc < 0)
		return rc;

	bld->regmap = devm_regmap_init_i2c(i2c, &mp3317_regmap_config);
	if (IS_ERR(bld->regmap)) {
		dev_err(dev, "Failed to set up MP3317 register map\n");
		return PTR_ERR(bld->regmap);
	}

	/*
	 * Force a re-read of hardware state into the cache in case something
	 * else has configured the chip before the kernel starts.
	 * Only allow configuring initial state from the device tree if the
	 * hardware is not configured before the kernel starts.
	 */
	if (bld->boot_on)
		mp3317_read_initial_state(bld);
	else
		mp3317_set_initial_state_from_dt(bld, initial_regs,
						 num_initial_regs);

	imult = DIV_ROUND_CLOSEST(ISET_RESISTOR_MAX, bld->iset_ext_resistor);

	for (i = 0; i < NUM_IMAX_VALUES; ++i)
		bld->imax_ua[i] = imult * imax_values[i];

	for (i = 0; i < MP3317_MAX_REGULATORS; i++) {
		reg_cfg.dev = dev;
		reg_cfg.driver_data = bld;

		reg_dev = devm_regulator_register(dev, &mp3317_reg_desc[i], &reg_cfg);
		if (IS_ERR(reg_dev)) {
			rc = PTR_ERR(reg_dev);
			dev_err(dev, "%s: Failed to register regulator, ret=%d\n",
				__func__, rc);
			return rc;
		}
	}

	rc = mp3317_setup_debugfs(bld);
	if (rc < 0)
		return rc;

	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "meta,mp3317" },
	{}
};

static struct i2c_driver mp3317 = {
	.driver = {
		.name = "mp3317",
		.of_match_table = match_table,
	},
	.probe = mp3317_probe,
};

module_i2c_driver(mp3317);

MODULE_DESCRIPTION("MP3317 I2C driver");
MODULE_LICENSE("GPL");
