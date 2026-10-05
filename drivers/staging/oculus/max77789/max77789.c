// SPDX-License-Identifier: GPL-2.0+

#include <linux/device.h>
#include <linux/extcon-provider.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/usb/dwc3-msm.h>
#include <linux/usb/phy.h>
#include <linux/pm_wakeup.h>

#include "max77789.h"

#define DRIVER_NAME "max77789"
#define ADC_SAMPLE_WINDOW 5
#define I2C_JITTER_DELAY_MS (100)
#define WAIT_FOR_DTS_MS 1500
#define RESET_STATE_MACHINE BIT(5)
#define USB3_PHY_RETRY_DELAY_MS 2000
#define USB3_PHY_RETRY_MAX 15

struct max77789_chip {
	struct regmap *regmap;
	struct device *dev;
	struct extcon_dev *edev;
	struct regulator_desc regulator_desc;
	struct regulator_dev *regulator;
	struct gpio_desc *dts_det_gpio;
	struct gpio_desc *intb_irq_gpio;
	struct work_struct irq_work;
	struct mutex mutex;
	struct iio_channel *cc1_chan;
	struct iio_channel *cc2_chan;
	struct iio_channel *vsys_chan;
	struct usb_phy *usb3_phy;
	struct notifier_block usb3_nb;
	struct work_struct usb3_work;
	unsigned int curr_capability_mA;
	bool is_superspeed;
	bool dts_online;
	bool lock_en;
	struct power_supply *usb_charger;
	int vsys_convert_coeff;
	struct delayed_work dts_work;
	struct delayed_work usb3_retry_work;
	int usb3_retry_count;
	int bc_int;
	unsigned int cc1_lvl;
	unsigned int cc2_lvl;
};

static const unsigned int max77789_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_CHG_USB_FAST,
	EXTCON_CHG_USB_SLOW,
	EXTCON_NONE,
};

static const struct reg_sequence max77789_reg_default[] = {
	{ MAX77789_REG_CC_CTRL3, MAX77789_DEFAULT_CC_CTRL3 },
	{ MAX77789_REG_CHG_CNFG_15, MAX77789_DEFAULT_CHG_CNFG_15 },
	{ MAX77789_REG_TOP_INT_MASK, MAX77789_DEFAULT_TOP_INT_MASK },
	{ MAX77789_REG_CHG_INT_MASK, MAX77789_DEFAULT_CHG_INT_MASK },
	{ MAX77789_REG_CHG_CNFG_00, MAX77789_DEFAULT_CHG_CNFG_00 },
	{ MAX77789_REG_CHG_CNFG_01, MAX77789_DEFAULT_CHG_CNFG_01 },
	{ MAX77789_REG_CHG_CNFG_12, MAX77789_DEFAULT_CHG_CNFG_12 },
	{ MAX77789_REG_CHG_CNFG_13, MAX77789_DEFAULT_CHG_CNFG_13 },
};

// Forward declarations
static int max77789_usb3_notifier(struct notifier_block *nb, unsigned long val,
                                  void *priv);
static void max77789_usb3_work(struct work_struct *data);


static int max77789_set_standby_regmap(struct max77789_chip *pchip, bool enable)
{
	int ret;
	unsigned int val;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_13, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg config 13 failed %d\n", __func__, ret);
		return ret;
	}

	if (enable)
		val = val | MAX77789_MASK_STBY_EN;
	else
		val = val & ~MAX77789_MASK_STBY_EN;

	ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_13, val);
	if (ret < 0)
		dev_err(pchip->dev,
			"%s:set standby to %d failed %d\n", __func__, enable, ret);

	return ret;
};

static int max77789_set_no_autoiset_regmap(struct max77789_chip *pchip, bool enable)
{
	int ret;
	unsigned int val;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_12, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg config 12 failed %d\n", __func__, ret);
		return ret;
	}

	if (enable)
		val = val | MAX77789_MASK_NO_AUTOISET;
	else
		val = val & ~MAX77789_MASK_NO_AUTOISET;

	ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_12, val);
	if (ret < 0)
		dev_err(pchip->dev,
			"%s:set no autoiset to %d failed %d\n", __func__, enable, ret);

	return ret;
};

static int max77789_enable_regmap(struct regulator_dev *rdev)
{
	struct max77789_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_00, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg config 00 failed %d\n", __func__, ret);
		return ret;
	}

	if ((val & MAX77789_MASK_CHG_CNFG_00) == MAX77789_BUCK_EN) {
		dev_info(pchip->dev, "%s:already enabled!\n", __func__);
		return 0;
	}

	val = (val & ~MAX77789_MASK_CHG_CNFG_00) | MAX77789_BUCK_EN;
	ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_00, val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:write chg config 00 %02X failed %d\n", __func__, val, ret);
		return ret;
	}

	return 0;
};

static int max77789_disable_regmap(struct regulator_dev *rdev)
{
	struct max77789_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_00, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg config 00 failed %d\n", __func__, ret);
		return ret;
	}

	if ((val & MAX77789_MASK_CHG_CNFG_00) == MAX77789_BUCK_DIS) {
		dev_info(pchip->dev, "%s:already disabled!\n", __func__);
		return 0;
	}

	val = (val & ~MAX77789_MASK_CHG_CNFG_00);
	ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_00, val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:write chg config 00 %02X failed %d\n", __func__, val, ret);
		return ret;
	}

	return 0;
};

static int max77789_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct max77789_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_00, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg config 00 failed %d\n", __func__, ret);
		return ret;
	}

	return (val & MAX77789_MASK_CHG_CNFG_00) == MAX77789_BUCK_EN;
};

static int max77789_set_voltage_regmap(struct regulator_dev *rdev,
				      int min_uV, int max_uV,
				      unsigned int *selector)
{
	struct max77789_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int sel_v, min_mV, max_mV;
	int ret;

	/* convert to mV*/
	min_mV = min_uV / 1000;
	max_mV = max_uV / 1000;

	if (min_mV < MAX77789_VSYS_MIN_MV || max_mV > MAX77789_VSYS_MAX_MV) {
		dev_err(pchip->dev,
			"voltage %d:%d mV over the limit %d:%d mV @ function %s",
			min_mV, max_mV, MAX77789_VSYS_MIN_MV, MAX77789_VSYS_MAX_MV,
			__func__);
		return -EINVAL;
	}

	if (min_mV < MAX77789_VSYS_STEP_THOLD_MV) {
		sel_v = (min_mV - MAX77789_VSYS_MIN_MV) / MAX77789_VSYS_STEP1_MV;
	} else {
		sel_v = MAX77789_VSYS_STEP_THOLD_REG_VALUE;
		sel_v += (min_mV - MAX77789_VSYS_STEP_THOLD_MV) / MAX77789_VSYS_STEP2_MV;
	}

	ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_04, sel_v);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:write chg config 04 %02X failed %d\n", __func__, sel_v, ret);
		return ret;
	}
	return 0;
};

static int max77789_get_voltage_regmap(struct regulator_dev *rdev)
{
	struct max77789_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_04, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg config 00 failed %d\n", __func__, ret);
		return ret;
	}

	val &= MAX77789_CHG_CNFG_04_CHG_CV_PRM;

	if (val <= MAX77789_VSYS_STEP_THOLD_REG_VALUE) {
		ret = MAX77789_VSYS_MIN_MV + MAX77789_VSYS_STEP1_MV * val;
	} else {
		ret = MAX77789_VSYS_STEP_THOLD_MV;
		ret += (val - MAX77789_VSYS_STEP_THOLD_REG_VALUE) * MAX77789_VSYS_STEP2_MV;
	}

	/* convert to uV*/
	return ret * 1000;
};

static int max77789_get_ilim_regmap(struct regulator_dev *rdev)
{
	struct max77789_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int val, addr;
	int ret;
	/* Check NO_AUTOISET
	 * if NO_AUTOISET is 0, the current limit is USB_ILIM_DTLS (0xC5)
	 * if NO_AUTOISET is 1, the current limit is CHG_CNFG_09 (0xC0)
	 */

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_12, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg config 12 failed %d\n", __func__, ret);
		return ret;
	}
	if (val & MAX77789_MASK_NO_AUTOISET)
		addr = MAX77789_REG_CHG_CNFG_09;
	else
		addr = MAX77789_REG_USB_ILIM_DTLS;

	ret = regmap_read(pchip->regmap, addr, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg %02X failed %d\n", __func__, addr, ret);
		return ret;
	}

	if (val <= MAX77789_ILIM_MIN_REG_VALUE) {
		ret = MAX77789_ILIM_MIN_MA;
	} else {
		ret = MAX77789_ILIM_MIN_MA;
		ret += (val - MAX77789_ILIM_MIN_REG_VALUE) * MAX77789_ILIM_STEP_MA;
	}

	/* convert to uA*/
	return ret * 1000;
};

static int max77789_set_ilim_regmap(struct regulator_dev *rdev,
					 int min_mA, int max_mA)
{
	struct max77789_chip *pchip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	if (max_mA < MAX77789_ILIM_MIN_MA || max_mA > MAX77789_ILIM_MAX_MA)
		return -EINVAL;

	val = MAX77789_ILIM_MIN_REG_VALUE;
	val += (max_mA - MAX77789_ILIM_MIN_MA) / MAX77789_ILIM_STEP_MA;

	ret = max77789_set_no_autoiset_regmap(pchip, true);
	if (ret && ret != -ENOTCONN) {
		dev_err(pchip->dev, "Disable autoiset failed:%d\n", ret);
		return ret;
	}
	if (ret == -ENOTCONN) {
		dev_info(pchip->dev, "chip not connected\n");
		return 0;
	}

	ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_09, val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:write chg current limit %02X failed %d\n",
			__func__, val, ret);
	}

	return 0;
};

static int max77789_get_status(struct regulator_dev *rdev)
{
	return 0;
};

static const struct regulator_ops max77789_ops = {
	.enable = max77789_enable_regmap,
	.disable = max77789_disable_regmap,
	.is_enabled = max77789_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage = max77789_set_voltage_regmap,
	.get_voltage = max77789_get_voltage_regmap,
	.get_current_limit = max77789_get_ilim_regmap,
	.get_status = max77789_get_status,
};

static const struct regmap_config max77789_regmap_config = {
	.name = DRIVER_NAME,
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = MAX77789_REG_CHG_CNFG_15,
	.use_single_read = true,
	.use_single_write = true,
};

static int max77789_init_regulator(struct max77789_chip *pchip)
{
	int ret;
	struct regulator_init_data init_data = {
		.constraints = {
			.valid_ops_mask =
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
			.min_uV = MAX77789_VSYS_MIN_MV * 1000,
			.max_uV = MAX77789_VSYS_MAX_MV * 1000,
			.min_uA = MAX77789_ILIM_MIN_MA * 1000,
			.max_uA = MAX77789_ILIM_MAX_MA * 1000,
			.keep_on = true,
		},
	};
	struct regulator_config config = {};

	config.regmap = pchip->regmap;
	config.driver_data = pchip;
	config.dev = pchip->dev;
	config.init_data = &init_data;

	pchip->regulator_desc.name =  "vout";
	pchip->regulator_desc.id = 0;
	pchip->regulator_desc.type = REGULATOR_VOLTAGE;
	pchip->regulator_desc.owner = THIS_MODULE;
	pchip->regulator_desc.min_uV = MAX77789_VSYS_MIN_MV * 1000;
	pchip->regulator_desc.uV_step = MAX77789_VSYS_STEP2_MV * 1000;
	pchip->regulator_desc.linear_min_sel = 1;
	pchip->regulator_desc.ops = &max77789_ops;
	pchip->regulator =
		devm_regulator_register(pchip->dev, &pchip->regulator_desc, &config);

	if (IS_ERR(pchip->regulator)) {
		ret = PTR_ERR(pchip->regulator);
		return ret;
	}

	ret = devm_regulator_debug_register(pchip->dev, pchip->regulator);
	if (ret)
		dev_err(pchip->dev, "Failed to register debug regulator, rc=%d\n", ret);

	return 0;
};

static int cc_voltage_level_check(int voltage)
{
	unsigned int cc_lvl;

	if (voltage < CC_V_LOW_MID_THOLD_MV)
		cc_lvl = CC_LEVEL_LOW;
	else if (voltage < CC_V_MID_HIGH_THOLD_MV)
		cc_lvl = CC_LEVEL_MID;
	else if (voltage < CC_V_ABNORMALLY_HIGH_THOLD_MV)
		cc_lvl = CC_LEVEL_HIGH;
	else
		cc_lvl = CC_LEVEL_TOO_HIGH;

	return cc_lvl;
};

static int median_avg_filter(int arr[], int size)
{
	int i, sum = 0, max = arr[0], min = arr[0];

	for (i = 0; i < size; i++) {
		sum += arr[i];
		if (arr[i] > max)
			max = arr[i];
		if (arr[i] < min)
			min = arr[i];
	}

	return (sum - max - min) / (size - 2);
}

static int max77789_check_cc_lvl(struct max77789_chip *pchip)
{
        int ret, cc1_voltage[ADC_SAMPLE_WINDOW], cc2_voltage[ADC_SAMPLE_WINDOW];
        int i, avg_cc1, avg_cc2;

        for (i = 0; i < ADC_SAMPLE_WINDOW; i++) {
                ret = iio_read_channel_processed(pchip->cc1_chan, &cc1_voltage[i]);
                if (ret < 0) {
                        dev_err(pchip->dev, "Read CC1 voltage failed %d", ret);
                        return -EINVAL;
                }
                ret = iio_read_channel_processed(pchip->cc2_chan, &cc2_voltage[i]);
                if (ret < 0) {
                        dev_err(pchip->dev, "Read CC2 voltage failed %d", ret);
                        return  -EINVAL;
                }
                msleep(5);
        }
        avg_cc1 = median_avg_filter(cc1_voltage, ADC_SAMPLE_WINDOW);
        avg_cc2 = median_avg_filter(cc2_voltage, ADC_SAMPLE_WINDOW);

        pchip->cc1_lvl = cc_voltage_level_check(avg_cc1);
        pchip->cc2_lvl = cc_voltage_level_check(avg_cc2);
        dev_dbg(pchip->dev, "CC1/CC2 voltage:%d,%d  CC1/CC2 LVL:%d,%d",
                        avg_cc1, avg_cc2, pchip->cc1_lvl, pchip->cc2_lvl);
        return ret;
}

static int max77789_dts_curr_check(struct max77789_chip *pchip)
{
	int ret;
	unsigned int curr_lvl;

	ret = max77789_check_cc_lvl(pchip);
	if (ret < 0) {
		dev_err(pchip->dev, "max77789 check cc lvl error %d\n", ret);
		return ret;
	}

	if (pchip->cc1_lvl == CC_LEVEL_TOO_HIGH ||
			pchip->cc2_lvl == CC_LEVEL_TOO_HIGH) {
		dev_info(pchip->dev, "CC voltage abnormal, max77789 reset machine state");
		//When CC voltage too high, reset state machine
		regmap_update_bits(pchip->regmap, MAX77789_REG_CC_CTRL3,
				RESET_STATE_MACHINE, RESET_STATE_MACHINE);
		msleep(100);
		ret = max77789_check_cc_lvl(pchip);
		//Reset done, restore to default setting
		regmap_update_bits(pchip->regmap, MAX77789_REG_CC_CTRL3,
				RESET_STATE_MACHINE, 0);
	}

	if (pchip->cc1_lvl == CC_LEVEL_HIGH && pchip->cc2_lvl == CC_LEVEL_LOW)
		curr_lvl = DTS_CURR_3000MA;
	else if (pchip->cc1_lvl == CC_LEVEL_MID && pchip->cc2_lvl == CC_LEVEL_LOW)
		curr_lvl = DTS_CURR_1500MA;
	else
		curr_lvl = DTS_CURR_DEFAULT;

	return curr_lvl;
};

static int get_vsys_avg(struct max77789_chip *pchip)
{
	int vsys_val, ret;
	u64 vsys_avg;
	int vsys_sum = 0, i = 0;

	for (; i < 10; i++) {
		ret = iio_read_channel_processed(pchip->vsys_chan, &vsys_val);
		if (ret < 0) {
			dev_err(pchip->dev, "Read vsys voltage failed %d", ret);
			return ret;
		}
		vsys_sum += vsys_val;
	}
	vsys_avg = vsys_sum / 10;

	vsys_avg *= pchip->vsys_convert_coeff;
	vsys_avg /= 1000;

	return vsys_avg;
}

static ssize_t chip_id_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(pchip->dev,
			"%s:read chg chip id failed %d\n", __func__, ret);
		return ret;
	}

	ret = snprintf(buf, PAGE_SIZE, "0x%02x\n", val);
	return ret;
};

static DEVICE_ATTR_RO(chip_id);

static ssize_t input_ilim_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);
	int curr;
	int ret;

	curr = max77789_get_ilim_regmap(pchip->regulator);

	if (curr < 0) {
		dev_err(pchip->dev, "failed to read input current limit %d\n", curr);
		return curr;
	}

	ret = snprintf(buf, PAGE_SIZE, "%duA\n", curr);
	return ret;
};

static DEVICE_ATTR_RO(input_ilim);

static ssize_t curr_capability_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%dmA\n", pchip->curr_capability_mA);
};

static DEVICE_ATTR_RO(curr_capability);

static ssize_t usb_chg_type_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);
	unsigned int reg_data, chg_type;
	int ret;

	ret = regmap_read(pchip->regmap, MAX77789_REG_USB_TYPE_DTLS, &reg_data);
	if (ret < 0) {
		dev_err(pchip->dev, "failed to read USB TYPE DTLS %d\n", ret);
		return ret;
	}

	chg_type = (reg_data & MAX77789_MASK_CHG_TYP) >> MAX77789_CHG_TYP_SHIFT;
	switch (chg_type) {
	case MAX77789_NO_ADAPTOR:
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "none");
		break;
	case MAX77789_CHG_SDP:
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "sdp");
		break;
	case MAX77789_CHG_CDP:
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "cdp");
		break;
	case MAX77789_CHG_DCP:
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "dcp");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "unknown");
		break;
	}

	return ret;
};

static DEVICE_ATTR_RO(usb_chg_type);

static ssize_t cc_status_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);
	unsigned int reg_data, ccpin_stat, cci_stat, cc_stat, dts_curr_lvl;
	char str_buff[64];
	int ret, gpio_val;

	/*  check DTS status firstly */
	gpio_val = gpiod_get_value_cansleep(pchip->dts_det_gpio);
	if (!gpio_val) {
		strcat(str_buff, "dts:");
		dts_curr_lvl = max77789_dts_curr_check(pchip);
		switch (dts_curr_lvl) {
		case DTS_CURR_3000MA:
			strcat(str_buff, "3000mA:");
			break;
		case DTS_CURR_1500MA:
			strcat(str_buff, "1500mA:");
			break;
		case DTS_CURR_DEFAULT:
			strcat(str_buff, "default:");
			break;
		default:
			strcat(str_buff, "unknow:");
			break;
		}
		strcat(str_buff, "unknow");

		return snprintf(buf, PAGE_SIZE, "%s\n", str_buff);
	}

	ret = regmap_read(pchip->regmap, MAX77789_REG_CC_STATUS1, &reg_data);
	if (ret < 0) {
		dev_err(pchip->dev, "failed to read CC STATUS1 %d\n", ret);
		return ret;
	}

	ccpin_stat = (reg_data & MAX77789_MASK_CCPIN_STAT) >> MAX77789_CCPIN_STAT_SHIFT;
	switch (ccpin_stat) {
	case MAX77789_CCPIN_NO_DET:
		strcat(str_buff, "none:");
		break;
	case MAX77789_CC1_ACTIVE:
		strcat(str_buff, "cc1 active:");
		break;
	case MAX77789_CC2_ACTIVE:
		strcat(str_buff, "cc2 active:");
		break;
	case MAX77789_CC_RFU:
		strcat(str_buff, "rfu:");
		break;
	default:
		strcat(str_buff, "unknown:");
		break;
	}

	cci_stat = (reg_data & MAX77789_MASK_CCI_STAT) >> MAX77789_CCI_STAT_SHIFT;
	switch (cci_stat) {
	case MAX77789_CCI_NO_UFP:
		strcat(str_buff, "not in UFP:");
		break;
	case MAX77789_CCI_500MA:
		strcat(str_buff, "500mA:");
		break;
	case MAX77789_CCI_1500MA:
		strcat(str_buff, "1500mA:");
		break;
	case MAX77789_CCI_3000MA:
		strcat(str_buff, "3000mA:");
		break;
	default:
		strcat(str_buff, "unknown:");
		break;
	}

	cc_stat = reg_data & MAX77789_MASK_CC_STAT;
	switch (cc_stat) {
	case MAX77789_CC_NO_CONN:
		strcat(str_buff, "No Connection");
		break;
	case MAX77789_CC_SINK:
		strcat(str_buff, "sink");
		break;
	case MAX77789_CC_SOURCE:
		strcat(str_buff, "source");
		break;
	case MAX77789_CC_RFU:
		strcat(str_buff, "rfu");
		break;
	default:
		strcat(str_buff, "unknown");
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", str_buff);
};

static DEVICE_ATTR_RO(cc_status);

static ssize_t buck_enable_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
				max77789_is_enabled_regmap(pchip->regulator));
}

static ssize_t buck_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);
	int rc, val;

	rc = kstrtoint(buf, 0, &val);
	if (rc)
		return rc;
	if (val != 0 && val != 1)
		return -EINVAL;

	if (val)
		max77789_enable_regmap(pchip->regulator);
	else
		max77789_disable_regmap(pchip->regulator);

	return size;
}
static DEVICE_ATTR_RW(buck_enable);


static ssize_t usb_connected_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    struct max77789_chip *pchip = dev_get_drvdata(dev);
    return snprintf(buf, PAGE_SIZE, "%d\n", IS_ERR_OR_NULL(pchip->usb3_phy));
}
static DEVICE_ATTR_RO(usb_connected);

static ssize_t trigger_usb_check_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t size)
{
    struct max77789_chip *pchip = dev_get_drvdata(dev);
    if (IS_ERR_OR_NULL(pchip->usb3_phy)) {
        pchip->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);
        if (!IS_ERR_OR_NULL(pchip->usb3_phy)) {
            INIT_WORK(&pchip->usb3_work, max77789_usb3_work);
            pchip->usb3_nb.notifier_call = max77789_usb3_notifier;
            usb_register_notifier(pchip->usb3_phy, &pchip->usb3_nb);
            dev_info(pchip->dev, "get USB3 Phy successfully\n");
            if (pchip->usb3_phy->last_event == USB_EVENT_ENUMERATED) {
                if (pchip->usb3_phy->flags & DEVICE_IN_SS_MODE)
                    pchip->is_superspeed = true;
                else
                    pchip->is_superspeed = false;
                schedule_work(&pchip->usb3_work);
            }
        }
    }
    return size;
}
static DEVICE_ATTR_WO(trigger_usb_check);

static ssize_t vsys_read_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct max77789_chip *pchip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%duV\n", get_vsys_avg(pchip));
}
static DEVICE_ATTR_RO(vsys_read);

static void max77789_update_chgin_ilim(struct max77789_chip *pchip)
{
	int ret = 0;

	dev_dbg(pchip->dev,
		"update_chgin_ilim: dts_online=%d, curr_capability=%dmA, is_superspeed=%d\n",
		pchip->dts_online, pchip->curr_capability_mA,
		pchip->is_superspeed);
	/**
	 * IF nDTS = 1 (not DTS)
	 *   Use the automatic setting provided by the MAX77789 and if
	 *   host current capability is 900mA we should manual set
	 *   NOAUTOIISET to 1, and 0xC0 to 900mA
	 */
	if (!pchip->dts_online) {
		if (pchip->curr_capability_mA == HOST_CURR_900_MA)
			ret = max77789_set_ilim_regmap(pchip->regulator,
					 HOST_CURR_900_MA, HOST_CURR_900_MA);
		if (ret < 0)
			dev_err(pchip->dev, "update chgin ilim failed %d\n", ret);
		return;
	}
	/**
	 * IF nDTS = 0
	 *   IF HOST_curr_capability  >= 1.5A
	 *     set MAX77789 limit to 1.5A
	 *   ELSE IF HOST_curr_capability  > 0
	 *     set MAX77789 limit = HOST_curr_capability
	 */
	if (pchip->curr_capability_mA >= HOST_CURR_1500_MA)
		ret = max77789_set_ilim_regmap(pchip->regulator,
					 HOST_CURR_1500_MA, HOST_CURR_1500_MA);
	else if (pchip->curr_capability_mA > HOST_CURR_0_MA)
		ret = max77789_set_ilim_regmap(pchip->regulator,
			pchip->curr_capability_mA, pchip->curr_capability_mA);
	else
		dev_warn(pchip->dev, "host current capability is 0mA\n");

	if (ret < 0)
		dev_err(pchip->dev, "update chgin ilim failed %d\n", ret);
};

static void max77789_check_state(struct max77789_chip *pchip)
{
	int ret, chg_type;
	unsigned int reg_data, cc_stat, dts_curr_lvl;

	/* USB connect, exit standby */
	max77789_set_standby_regmap(pchip, false);

	/* get charge type firstly */
	ret = regmap_read(pchip->regmap, MAX77789_REG_USB_TYPE_DTLS, &reg_data);
	if (ret < 0) {
		dev_err(pchip->dev, "failed to read USB TYPE DTLS %d\n", ret);
		return;
	}
	chg_type = (reg_data & MAX77789_MASK_CHG_TYP) >> MAX77789_CHG_TYP_SHIFT;
	/* check if DTS board connected */
	if (pchip->dts_online) {
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB_HOST, false);
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB, true);
		dts_curr_lvl = max77789_dts_curr_check(pchip);
		switch (dts_curr_lvl) {
		case DTS_CURR_3000MA:
			pchip->curr_capability_mA = HOST_CURR_3000_MA;
			break;
		case DTS_CURR_1500MA:
			pchip->curr_capability_mA = HOST_CURR_1500_MA;
			break;
		case DTS_CURR_DEFAULT:
			if (pchip->is_superspeed ||
					chg_type == MAX77789_CHG_CDP ||
					chg_type == MAX77789_CHG_DCP)
				pchip->curr_capability_mA = HOST_CURR_900_MA;
			else
				pchip->curr_capability_mA = HOST_CURR_500_MA;
			break;
		default:
			pchip->curr_capability_mA = HOST_CURR_0_MA;
			break;
		}
		goto update_chgin_ilim;
	}
	/* check usb role */
	ret = regmap_read(pchip->regmap, MAX77789_REG_CC_STATUS1, &reg_data);
	cc_stat = reg_data & MAX77789_MASK_CC_STAT;
	switch (cc_stat) {
	case MAX77789_CC_NO_CONN:
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB_HOST, false);
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB, false);
		break;
	case MAX77789_CC_SINK:
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB_HOST, false);
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB, true);
		break;
	case MAX77789_CC_SOURCE:
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB_HOST, true);
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB, false);
		break;
	case MAX77789_CC_RFU:
	default:
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB_HOST, false);
		extcon_set_state_sync(pchip->edev,
					EXTCON_USB, false);
		break;
	}
	/* check usb current
	 * Without DTS board, the USB_ILIM_DTLS (0xC5) indicated the final
	 * input current limit, we need read it then decide the host curret
	 * capability, and if the 0xC5 is 500mA and USB 3.0 we should manual
	 * set NOAUTOIISET to 1, and 0xC0 to 900mA
	 */
	ret = regmap_read(pchip->regmap, MAX77789_REG_USB_ILIM_DTLS, &reg_data);
	if (ret < 0) {
		dev_err(pchip->dev, "failed to read USB ILIM DTLS %d\n", ret);
		return;
	}

	switch (reg_data) {
	case MAX77789_USB_ILIM_3000MA:
		pchip->curr_capability_mA = HOST_CURR_3000_MA;
		break;
	case MAX77789_USB_ILIM_1500MA:
		pchip->curr_capability_mA = HOST_CURR_1500_MA;
		break;
	case MAX77789_USB_ILIM_500MA:
		if (pchip->is_superspeed)
			pchip->curr_capability_mA = HOST_CURR_900_MA;
		else
			pchip->curr_capability_mA = HOST_CURR_500_MA;
		break;
	case MAX77789_CC_CURR_NO:
	default:
		pchip->curr_capability_mA = HOST_CURR_0_MA;
		break;
	}

	dev_dbg(pchip->dev,
		"check_state: USB_ILIM_DTLS=0x%02x, is_superspeed=%d, curr_capability=%dmA\n",
		reg_data, pchip->is_superspeed, pchip->curr_capability_mA);

update_chgin_ilim:

	/**
	 * Check host current capability
	 * HOST_curr_capability  <= 900mA
	 *   Set MAX17332 max current = Low
	 * HOST_curr_capability  >= 1500mA
	 *   Set MAX17332 max current = High
	 */
	if (pchip->curr_capability_mA < HOST_CURR_1500_MA) {
		extcon_set_state_sync(pchip->edev,
					EXTCON_CHG_USB_FAST, false);
		extcon_set_state_sync(pchip->edev,
					EXTCON_CHG_USB_SLOW, true);
	} else {
		extcon_set_state_sync(pchip->edev,
					EXTCON_CHG_USB_FAST, true);
		extcon_set_state_sync(pchip->edev,
					EXTCON_CHG_USB_SLOW, false);
	}

	max77789_update_chgin_ilim(pchip);

	/* if host current capability is 0, and DTS present
	 * we need disable max77789 and set output voltage
	 * to 0V. and if DTS is not present, only set to 0V.
	 */
	if (pchip->curr_capability_mA <= 0) {
		if (pchip->dts_online)
			max77789_disable_regmap(pchip->regulator);
		regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_04, 0x00);
	}
};

static void max77789_irq_work(struct work_struct *work)
{
	struct max77789_chip *pchip = container_of(work,
			struct max77789_chip, irq_work);
	unsigned int cc_int, bc_stat1;
	int ret, retry = 0;

	/* Clear interrupt. Read would clear the register */
	mutex_lock(&pchip->mutex);

	/*
	 * When usb insert, wait i2c awake from sleep
	 * the maximum waitting time is 300ms
	 */
	while(1) {
		if (retry == 5)
			break;
		ret = regmap_read(pchip->regmap, MAX77789_REG_BC_INT, &pchip->bc_int);
		if (ret > 0 || ret == 0)
			break;
		msleep(50);
		retry ++;
	}
	ret |= regmap_read(pchip->regmap, MAX77789_REG_CC_INT, &cc_int);
	ret |= regmap_read(pchip->regmap, MAX77789_REG_BC_STATUS1, &bc_stat1);
	if (ret < 0) {
		dev_err(pchip->dev, "failed to read BC_INT/CC_INT/BC_STATUS1:%d\n", ret);
		mutex_unlock(&pchip->mutex);
		return;
	}
	dev_info(pchip->dev, "max77789 BC irq: %02X\n", pchip->bc_int);
	dev_info(pchip->dev, "max77789 CC irq: %02X\n", cc_int);
	dev_info(pchip->dev, "max77789 BC status_1: %02X\n", bc_stat1);

	/**
	 * First check BC_VBUSDet of BC_STATUS1
	 * BC_VBUSDet is 0, USB disconnected
	 *   if curr_capability_mA is 0
	 *     exit
	 *   else
	 *     update status directly
	 */
	if ((bc_stat1 & MAX77789_MASK_BC_VBUSDET) == 0) {
		if (pchip->curr_capability_mA == HOST_CURR_0_MA) {
			goto exit_check;
		}
		goto usb_disconnected;
	}

	schedule_delayed_work(&pchip->dts_work, msecs_to_jiffies(WAIT_FOR_DTS_MS));
	return;

usb_disconnected:
	dev_info(pchip->dev, "USB disconnected\n");

	/* ensure system is awake when usb disconnect */
	pm_wakeup_event(pchip->dev, I2C_JITTER_DELAY_MS);
	max77789_set_ilim_regmap(pchip->regulator,
		HOST_CURR_500_MA, HOST_CURR_500_MA);
	/* Disable bypass USBC control for INLIM  */
	max77789_set_no_autoiset_regmap(pchip, false);

	/* enable max77789 buck mode */
	if (!max77789_is_enabled_regmap(pchip->regulator))
		max77789_enable_regmap(pchip->regulator);
	/* set max77789 output to default 4.2V */
	max77789_set_voltage_regmap(pchip->regulator,
				      4200000, 4200000, NULL);
	/* Enable standby to minimize power consumption */
	max77789_set_standby_regmap(pchip, true);
	extcon_set_state_sync(pchip->edev,
					EXTCON_CHG_USB_FAST, false);
	extcon_set_state_sync(pchip->edev,
					EXTCON_CHG_USB_SLOW, false);
	extcon_set_state_sync(pchip->edev,
					EXTCON_USB_HOST, false);
	extcon_set_state_sync(pchip->edev,
					EXTCON_USB, false);
	pchip->is_superspeed = false;
	pchip->curr_capability_mA = HOST_CURR_0_MA;
	pchip->dts_online = false;

exit_check:
	mutex_unlock(&pchip->mutex);
}

static void max77789_work_sync_and_put(void *data)
{
	struct max77789_chip *pchip = data;

	cancel_work_sync(&pchip->irq_work);
	cancel_delayed_work_sync(&pchip->dts_work);
	cancel_delayed_work_sync(&pchip->usb3_retry_work);
}

static irqreturn_t max77789_intb_irq_handler(int irq, void *data)
{
	struct max77789_chip *pchip = data;

	dev_info(pchip->dev, "Received INTB IRQ\n");
	/*
	 * if USB is connected, delay a bit to process irq,
	 * ensure system stays awake during processing irq_work.
	 * if USB is disconnect, process irq directly */
	schedule_work(&pchip->irq_work);

	return IRQ_HANDLED;
};

static int gpio_init_helper(struct device *dev, struct gpio_desc **desc,
						const char *name, enum gpiod_flags flags)
{
	*desc = devm_gpiod_get(dev, name, flags);
	if (IS_ERR_OR_NULL(*desc)) {
		dev_err(dev, "Failed to acquire %s gpio\n", name);
		return PTR_ERR(*desc);
	}
	return 0;
};

static int max77789_parse_dt(struct max77789_chip *pchip)
{
	int ret = 0, chan_num = 0;
	const char *temp_string = NULL;

	chan_num =  of_property_count_strings(pchip->dev->of_node,
		"io-channel-names");
	if (chan_num != 3) {
		dev_err(pchip->dev, "iio chan number must be 3, chan_num = %d",
				chan_num);
		return -EINVAL;
	}
	/* get cc1 voltage channel*/
	ret = of_property_read_string_index(pchip->dev->of_node, "io-channel-names",
			0, &temp_string);
	if (ret < 0) {
		dev_err(pchip->dev, "Failed to read cc1 io-channel-names: %d",
				ret);
		return ret;
	}

	pchip->cc1_chan = iio_channel_get(pchip->dev, temp_string);
	if (IS_ERR(pchip->cc1_chan)) {
		dev_err(pchip->dev, "channel %s iio_channel_get error: %ld",
			temp_string,
			PTR_ERR(pchip->cc1_chan));
		return ret;
	}
	dev_err(pchip->dev, "channel %s iio_channel_get successfully",
			temp_string);

	/* get cc2 voltage channel*/
	ret = of_property_read_string_index(pchip->dev->of_node, "io-channel-names",
			1, &temp_string);
	if (ret < 0) {
		dev_err(pchip->dev, "Failed to read cc2 io-channel-names: %d",
				ret);
		return ret;
	}

	pchip->cc2_chan = iio_channel_get(pchip->dev, temp_string);
	if (IS_ERR(pchip->cc2_chan)) {
		dev_err(pchip->dev, "channel %s iio_channel_get error: %ld",
			temp_string,
			PTR_ERR(pchip->cc2_chan));
		return ret;
	}
	dev_dbg(pchip->dev, "channel %s iio_channel_get successfully",
			temp_string);

	/* get vsys voltage channel */
	ret = of_property_read_string_index(pchip->dev->of_node, "io-channel-names",
			2, &temp_string);
	if (ret < 0) {
		dev_err(pchip->dev, "Failed to read vsys io-channel-names: %d",
				ret);
		return ret;
	}

	pchip->vsys_chan = iio_channel_get(pchip->dev, temp_string);
	if (IS_ERR(pchip->vsys_chan)) {
		dev_err(pchip->dev, "channel %s iio_channel_get error: %ld",
			temp_string,
			PTR_ERR(pchip->vsys_chan));
		return ret;
	}
	dev_dbg(pchip->dev, "channel %s iio_channel_get successfully",
			temp_string);

	/* get vsys convert coefficient */
	ret = of_property_read_u32(pchip->dev->of_node, "vsys-convert-coeff",
			&pchip->vsys_convert_coeff);
	if (ret)
		dev_err(pchip->dev, "Failed to read vsys-convert-coeff: %d\n",
				ret);

	return 0;
}

int max77789_lock_write_protection(struct max77789_chip *pchip, bool lock_en)
{
	int ret;
	unsigned int val;

	if (lock_en == pchip->lock_en) {
		dev_warn(pchip->dev, "%s: Write protection is already %s, skip!\n",
				__func__, lock_en ? "On" : "Off");
		return 0;
	}

	if (lock_en) {
		/* lock write protection */
		ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_06, 0x00);
	} else {
		/* unlock write protection */
		ret = regmap_write(pchip->regmap, MAX77789_REG_CHG_CNFG_06, 0x0C);
	}
	if (ret < 0)
		goto err;

	ret = regmap_read(pchip->regmap, MAX77789_REG_CHG_CNFG_06, &val);
	if (ret < 0)
		goto err;


	/* sync local flag with chip state */
	if (((val & MAX77789_MASK_CHGPROT) >> MAX77789_CHGPROT_SHIFT) == 0x3)
		pchip->lock_en = false;
	else
		pchip->lock_en = true;

	if (lock_en != pchip->lock_en) {
		dev_err(pchip->dev,
			"%s: Write protection state %s not reached: req/cur: %d/%d\n",
			__func__, lock_en ? "On " : "Off", lock_en, pchip->lock_en);
	}

	return 0;
err:
	pr_err("<%s> failed\n", __func__);
	return ret;
};

static int max77789_usb3_notifier(struct notifier_block *nb, unsigned long val,
				void *priv)
{
	struct max77789_chip *pchip =
			container_of(nb, struct max77789_chip, usb3_nb);
	/**
	 * USB_EVENT_ENUMERATED means USB enumerated and the flags will indicate
	 * SS mode if speed is greater or equal than super speed
	 */
	if (pchip->usb3_phy->last_event == USB_EVENT_ENUMERATED) {
		if (pchip->usb3_phy->flags & DEVICE_IN_SS_MODE)
			pchip->is_superspeed = true;
		else
			pchip->is_superspeed = false;
		schedule_work(&pchip->usb3_work);
	}
	return NOTIFY_OK;
};

static void max77789_usb3_work(struct work_struct *data)
{
	struct max77789_chip *pchip = container_of(data,
			struct max77789_chip, usb3_work);

	/* check if enumerates as SuperSpeed*/
	if (!pchip->is_superspeed) {
		dev_info(pchip->dev, "USB2.0 connected\n");
		return;
	}
	dev_info(pchip->dev, "USB3.0 connected\n");
	/**
	 * If boot up with USB cable, host current capability will not
	 * be 0. And if host current capability is 500mA, update it to
	 * 900mA, then update chgin_ilim
	 */
	mutex_lock(&pchip->mutex);
	if (pchip->curr_capability_mA == HOST_CURR_500_MA) {
		pchip->curr_capability_mA = HOST_CURR_900_MA;
	/* only update current limit manually when DTS exist */
		max77789_set_ilim_regmap(pchip->regulator,
			pchip->curr_capability_mA,
			pchip->curr_capability_mA);
	}
	mutex_unlock(&pchip->mutex);
};

static void max77789_dts_work(struct work_struct *data)
{
	int gpio_val;
	struct max77789_chip *pchip = container_of(data,
			struct max77789_chip, dts_work.work);

	/**
	 * Check BC_VBUSDetI of BC_INT
	 * BC_VBUSDetI is triggered, USB connected
	 *   if DTS connected
	 *     update status directly
	 *   else
	 *     continue
	 */
	if ((pchip->bc_int & MAX77789_MASK_BC_VBUSDETI)  != 0) {
		gpio_val = gpiod_get_value_cansleep(pchip->dts_det_gpio);
		dev_info(pchip->dev, "max77789 dts_det_gpio: %d\n", gpio_val);
		if (!gpio_val) {
			dev_info(pchip->dev, "DTS connected\n");
			pchip->dts_online = true;
			max77789_check_state(pchip);
			goto exit_check;
		}
	}
	/**
	 * Check BC_CHGTYPL Status Interrupt of BC_INT
	 * BC_CHGTYPL interrupt is triggered and curr_capability_mA is 0
	 *   update status directly
	 * else
	 *   exit
	 */
	if ((pchip->bc_int & MAX77789_MASK_BC_CHGTYPL)  != 0 &&
                        pchip->curr_capability_mA == HOST_CURR_0_MA) {
                /* check usb status*/
                dev_info(pchip->dev, "USB connected\n");
                max77789_check_state(pchip);
        }
exit_check:
	mutex_unlock(&pchip->mutex);
}

static void max77789_usb3_retry_work(struct work_struct *data)
{
	struct max77789_chip *pchip = container_of(data,
			struct max77789_chip, usb3_retry_work.work);

	if (!IS_ERR_OR_NULL(pchip->usb3_phy))
		return;

	pchip->usb3_phy = devm_usb_get_phy(pchip->dev, USB_PHY_TYPE_USB3);
	if (IS_ERR_OR_NULL(pchip->usb3_phy)) {
		pchip->usb3_retry_count++;
		if (pchip->usb3_retry_count < USB3_PHY_RETRY_MAX) {
			dev_info(pchip->dev,
				"USB3 Phy not available, retry %d/%d\n",
				pchip->usb3_retry_count, USB3_PHY_RETRY_MAX);
			schedule_delayed_work(&pchip->usb3_retry_work,
				msecs_to_jiffies(USB3_PHY_RETRY_DELAY_MS));
		} else {
			dev_warn(pchip->dev,
				"USB3 Phy not available after %d retries, giving up\n",
				USB3_PHY_RETRY_MAX);
		}
		return;
	}

	INIT_WORK(&pchip->usb3_work, max77789_usb3_work);
	pchip->usb3_nb.notifier_call = max77789_usb3_notifier;
	usb_register_notifier(pchip->usb3_phy, &pchip->usb3_nb);
	dev_info(pchip->dev, "get USB3 Phy successfully (deferred, retry %d)\n",
		pchip->usb3_retry_count);

	if (pchip->usb3_phy->last_event == USB_EVENT_ENUMERATED) {
		pchip->is_superspeed =
			!!(pchip->usb3_phy->flags & DEVICE_IN_SS_MODE);
		dev_info(pchip->dev,
			"USB3 already enumerated (deferred), is_superspeed=%d\n",
			pchip->is_superspeed);
		schedule_work(&pchip->usb3_work);
	}
}

static int max77789_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct max77789_chip *pchip;
	union power_supply_propval prop;
	int ret, intb_irq, gpio_val;
	unsigned int val;

	pchip = devm_kzalloc(dev, sizeof(struct max77789_chip), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	i2c_set_clientdata(client, pchip);
	pchip->dev = dev;
	pchip->lock_en = true;

	mutex_init(&pchip->mutex);
	INIT_WORK(&pchip->irq_work, max77789_irq_work);
	INIT_DELAYED_WORK(&pchip->dts_work, max77789_dts_work);

	pchip->regmap = devm_regmap_init_i2c(client, &max77789_regmap_config);
	if (IS_ERR(pchip->regmap)) {
		ret = PTR_ERR(pchip->regmap);
		dev_err(dev, "failed to initialize regmap: %d", ret);
		return ret;
	}

	max77789_lock_write_protection(pchip, false);

	regmap_register_patch(pchip->regmap, max77789_reg_default,
			ARRAY_SIZE(max77789_reg_default));

	ret = max77789_init_regulator(pchip);
	if (ret < 0) {
		dev_err(dev, "failed to register regulator: %d", ret);
		return ret;
	}

	ret = gpio_init_helper(dev, &pchip->dts_det_gpio, "dts",
							GPIOD_IN);
	if (ret) {
		dev_err(dev, "failed to init DTS detect gpio: %d", ret);
		return ret;
	}

	ret = gpio_init_helper(dev, &pchip->intb_irq_gpio, "intb",
							GPIOD_IN);
	if (ret) {
		dev_err(dev, "failed to init INTB gpio: %d", ret);
		return ret;
	}

	intb_irq = gpiod_to_irq(pchip->intb_irq_gpio);
	ret = devm_request_irq(dev, intb_irq, &max77789_intb_irq_handler,
			IRQF_TRIGGER_FALLING, "intb_irq", pchip);
	if (ret) {
		dev_err(dev, "Failed to request the INTB IRQ ret=%d\n", ret);
		return ret;
	}
	ret = enable_irq_wake(intb_irq);
	if (ret) {
		dev_err(dev, "Failed to set INTB IRQ wake ret=%d", ret);
		return ret;
	}

	/* clear BC/CC INT register then enable the interrupt */
	regmap_read(pchip->regmap, MAX77789_REG_BC_INT, &val);
	dev_dbg(pchip->dev, "max77789 BC irq: %02X\n", val);
	regmap_read(pchip->regmap, MAX77789_REG_CC_INT, &val);
	dev_dbg(pchip->dev, "max77789 CC irq: %02X\n", val);
	regmap_write(pchip->regmap, MAX77789_REG_BC_INTMASK, MAX77789_DEFAULT_BC_INTMASK);
	regmap_write(pchip->regmap, MAX77789_REG_CC_INTMASK, MAX77789_DEFAULT_CC_INTMASK);

	/* Allocate extcon device */
	pchip->edev = devm_extcon_dev_allocate(pchip->dev, max77789_extcon_cable);
	if (IS_ERR(pchip->edev)) {
		dev_err(pchip->dev, "failed to allocate memory for extcon\n");
		return -ENOMEM;
	}

	/* Register extcon device */
	ret = devm_extcon_dev_register(pchip->dev, pchip->edev);
	if (ret) {
		dev_err(pchip->dev, "failed to register extcon device\n");
		return ret;
	}

	ret = max77789_parse_dt(pchip);
	if (ret != 0) {
		dev_err(pchip->dev, "failed to parse dt:%d\n", ret);
		return ret;
	}

	/* Get USB Phy */
	pchip->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);
	if (!IS_ERR_OR_NULL(pchip->usb3_phy)) {
		INIT_WORK(&pchip->usb3_work, max77789_usb3_work);
		pchip->usb3_nb.notifier_call = max77789_usb3_notifier;
		usb_register_notifier(pchip->usb3_phy, &pchip->usb3_nb);
		dev_info(pchip->dev, "get USB3 Phy successfully\n");
		/* Catch enumeration that completed before notifier registration */
		if (pchip->usb3_phy->last_event == USB_EVENT_ENUMERATED)
			pchip->is_superspeed =
				!!(pchip->usb3_phy->flags & DEVICE_IN_SS_MODE);
	} else {
		dev_warn(dev, "USB3 Phy not available, scheduling deferred retry\n");
		INIT_DELAYED_WORK(&pchip->usb3_retry_work,
				max77789_usb3_retry_work);
		schedule_delayed_work(&pchip->usb3_retry_work,
				msecs_to_jiffies(1000));
	}

	ret = device_create_file(dev, &dev_attr_chip_id);
	ret |= device_create_file(dev, &dev_attr_usb_chg_type);
	ret |= device_create_file(dev, &dev_attr_cc_status);
	ret |= device_create_file(dev, &dev_attr_input_ilim);
	ret |= device_create_file(dev, &dev_attr_curr_capability);
	ret |= device_create_file(dev, &dev_attr_buck_enable);
	ret |= device_create_file(dev, &dev_attr_vsys_read);
	ret |= device_create_file(dev, &dev_attr_trigger_usb_check);
    ret |= device_create_file(dev, &dev_attr_usb_connected);
	if (ret < 0) {
		dev_err(dev, "failed to create sysfs file: %d", ret);
		return ret;
	}

	ret = devm_add_action_or_reset(dev, max77789_work_sync_and_put, pchip);
	if (ret)
		return ret;
	/* check current status when do first probe */
	mutex_lock(&pchip->mutex);
	/* check DTS status*/
	gpio_val = gpiod_get_value_cansleep(pchip->dts_det_gpio);
	if (!gpio_val) {
		dev_info(pchip->dev, "DTS connected\n");
		pchip->dts_online = true;
	}
	pchip->usb_charger = power_supply_get_by_name("usb-charger");
	if (IS_ERR_OR_NULL(pchip->usb_charger)) {
		dev_err(pchip->dev,
			"%s : Failed to find usb power supply device\n", __func__);
	}
	ret = power_supply_get_property(pchip->usb_charger, POWER_SUPPLY_PROP_ONLINE, &prop);
	if (!ret && prop.intval)
		max77789_check_state(pchip);
	if (!ret && !prop.intval) {
		/* Boot system with usb disconnected, enter standby mode */
		max77789_set_standby_regmap(pchip, true);
	}
	/* set device able to wake up system */
	device_init_wakeup(dev, true);
	enable_irq_wake((unsigned int)intb_irq);
	mutex_unlock(&pchip->mutex);

	dev_info(pchip->dev, "max77789 init done");

	return 0;
};

static int max77789_remove(struct i2c_client *client)
{
	struct max77789_chip *pchip;
	struct device *dev;

	pchip = i2c_get_clientdata(client);
	dev = pchip->dev;

	device_remove_file(dev, &dev_attr_chip_id);
	device_remove_file(dev, &dev_attr_usb_chg_type);
	device_remove_file(dev, &dev_attr_cc_status);
	device_remove_file(dev, &dev_attr_input_ilim);
	device_remove_file(dev, &dev_attr_curr_capability);
	device_remove_file(dev, &dev_attr_buck_enable);
	device_remove_file(dev, &dev_attr_vsys_read);
	device_remove_file(dev, &dev_attr_trigger_usb_check);
    device_remove_file(dev, &dev_attr_usb_connected);

	dev_info(pchip->dev, "max77789 removed");
	return 0;
};

static const struct i2c_device_id max77789_i2c_id[] = {{DRIVER_NAME}, {}};
MODULE_DEVICE_TABLE(i2c, max77789_i2c_id);

static const struct of_device_id max77789_of_match[] = {
	{.compatible = "maxim,max77789"},
	{},
};
MODULE_DEVICE_TABLE(of, max77789_of_match);

static struct i2c_driver max77789_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(max77789_of_match),
	},
	.probe = max77789_probe,
	.remove = max77789_remove,
	.id_table = max77789_i2c_id,
};
module_i2c_driver(max77789_driver);

MODULE_DESCRIPTION("Device Driver for Maxim MAX77789");
MODULE_LICENSE("GPL v2");
