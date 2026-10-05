// SPDX-License-Identifier: GPL-2.0
/*
 * STMicroelectronics STUSB4500 USB PD controller driver.
 */

#include <linux/bitfield.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/gpio.h>

#include "stusb4500.h"

struct stusb4500 {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	struct mutex irq_complete;
	bool resume_completed;
	bool irq_waiting;
	int irq;
	uint8_t nvm_config[SECTOR_NUM_MAX][SECTOR_BLOCK_SIZE];
};

static const struct regmap_config stusb4500_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
	.max_register = STUSB4500_REG_MAX,
	.use_single_read = true,
	.use_single_write = true,
};

static int stusb4500_wait_for_execution(struct stusb4500 *chip)
{
	int i, ret;
	uint32_t val;

	for (i = 0; i < 10; i++) {
		ret = regmap_read(chip->regmap, FTP_CTRL_0_REG, &val);
		if (ret) {
			dev_err(chip->dev, "Failed to get completion status: %d", ret);
			return ret;
		}
		if (!(val & FTP_CUST_REQ))
			break;
		usleep_range(10000, 10100);
	}

	if (i == 10) {
		dev_err(chip->dev, "Timed out while waiting for execution");
		return -ETIMEDOUT;
	}

	return 0;
}

static int stusb4500_write_nvm_sector(struct stusb4500 *chip, char sector_num,
				      uint8_t *sector_data)
{
	struct device *dev = chip->dev;
	int ret;

	regmap_bulk_write(chip->regmap, RW_BUFFER_REG, sector_data, 8);

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, FTP_CUST_PWR | FTP_CUST_RST_N);
	if (ret) {
		dev_err(dev, "Failed to power up the NVM: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_1_REG, WRITE_PL & FTP_CUST_OPCODE_MASK);
	if (ret) {
		dev_err(dev, "Failed to program the load register: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG,
			   FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ);
	if (ret) {
		dev_err(dev, "Failed to load write to PL sectors opcode: %d", ret);
		return ret;
	}

	ret = stusb4500_wait_for_execution(chip);
	if (ret)
		return ret;

	ret = regmap_write(chip->regmap, FTP_CTRL_1_REG, PROG_SECTOR & FTP_CUST_OPCODE_MASK);
	if (ret) {
		dev_err(dev, "Failed to set prog sectors opcode: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG,
		(sector_num & FTP_CUST_SECT) | FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ);
	if (ret) {
		dev_err(dev, "Failed to load prog sectors opcode: %d", ret);
		return ret;
	}

	ret = stusb4500_wait_for_execution(chip);
	if (ret)
		return ret;

	return 0;
}

static int stusb4500_enter_write_mode(struct stusb4500 *chip, uint8_t erased_sector)
{
	struct device *dev = chip->dev;
	int ret, ser_opcode;

	ret = regmap_write(chip->regmap, FTP_CUST_PASSWORD_REG, FTP_CUST_PASSWORD);
	if (ret) {
		dev_err(dev, "Failed to set password: %d", ret);
		return ret;
	}

	/* This register must be NULL for the partial erase feature */
	ret = regmap_write(chip->regmap, RW_BUFFER_REG, 0);
	if (ret) {
		dev_err(dev, "Failed to set null for RW_BUFFER_REG: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, 0);
	if (ret) {
		dev_err(dev, "Failed to set null for FTP_CTRL_0_REG: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, FTP_CUST_PWR | FTP_CUST_RST_N);
	if (ret) {
		dev_err(dev, "Failed to reset NVM: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, FTP_CUST_RST_N);
	if (ret) {
		dev_err(dev, "Failed to set RST_N bit after reset: %d", ret);
		return ret;
	}

	/* Load 0xF1 to FTP_CUST_SER to erase all sectors of FTP and set write SER opcode */
	ser_opcode = ((erased_sector << 3) & FTP_CUST_SER) | (WRITE_SER & FTP_CUST_OPCODE_MASK);
	ret = regmap_write(chip->regmap, FTP_CTRL_1_REG, ser_opcode);
	if (ret) {
		dev_err(dev, "Failed to set write SER opcode: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG,
			   FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ);
	if (ret) {
		dev_err(dev, "Failed to set write SER opcode: %d", ret);
		return ret;
	}

	ret = stusb4500_wait_for_execution(chip);
	if (ret)
		return ret;

	ret = regmap_write(chip->regmap, FTP_CTRL_1_REG, SOFT_PROG_SECTOR & FTP_CUST_OPCODE_MASK);
	if (ret) {
		dev_err(dev, "Failed to set write SER opcode: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG,
			   FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ);
	if (ret) {
		dev_err(dev, "Failed to load soft prog opcode: %d", ret);
		return ret;
	}

	ret = stusb4500_wait_for_execution(chip);
	if (ret)
		return ret;

	ret = regmap_write(chip->regmap, FTP_CTRL_1_REG, ERASE_SECTOR & FTP_CUST_OPCODE_MASK);
	if (ret) {
		dev_err(dev, "Failed to set write SER opcode: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG,
			   FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ);
	if (ret) {
		dev_err(dev, "Failed to load erase sectors SER opcode: %d", ret);
		return ret;
	}

	ret = stusb4500_wait_for_execution(chip);
	if (ret)
		return ret;

	return 0;
}

static int stusb4500_read_nvm_sector(struct stusb4500 *chip,
				     uint8_t sector_num, char *buf)
{
	struct device *dev = chip->dev;
	int ret, i, len = 0;
	uint32_t sec_opcode;
	uint32_t val;

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, FTP_CUST_PWR | FTP_CUST_RST_N);
	if (ret) {
		dev_err(dev, "Failed to set FTP_CTRL_0_REG: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_1_REG, READ_TAG & FTP_CUST_OPCODE_MASK);
	if (ret) {
		dev_err(dev, "Failed to set read sector opcode: %d", ret);
		return ret;
	}

	sec_opcode = (sector_num & FTP_CUST_SECT) | FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ;
	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, sec_opcode);
	if (ret) {
		dev_err(dev, "Failed to load read sector opcode: %d", ret);
		return ret;
	}

	ret = stusb4500_wait_for_execution(chip);
	if (ret) {
		dev_err(dev, "Failed to complete execution ret=%d", ret);
		return ret;
	}

	for (i = 0; i < SECTOR_BLOCK_SIZE; i++) {
		ret = regmap_read(chip->regmap, RW_BUFFER_REG + i, &val);
		if (ret) {
			dev_err(dev, "Failed to read register 0x%x: %d", RW_BUFFER_REG + i, ret);
			return ret;
		}
		len += snprintf(buf + len, REG_RAW_VAL_SIZE, "0x%02x", (uint8_t)val);
		buf[len++] = ' ';
	}
	buf[len++] = '\n';

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, 0);
	if (ret) {
		dev_err(dev, "Failed to clear FTP_CTRL_0_REG: %d", ret);
		return ret;
	}

	return len;
}

static int stusb4500_enter_read_mode(struct stusb4500 *chip)
{
	struct device *dev = chip->dev;
	int ret = 0;

	ret = regmap_write(chip->regmap, FTP_CUST_PASSWORD_REG, FTP_CUST_PASSWORD);
	if (ret) {
		dev_err(dev, "Failed to set password: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, FTP_CUST_RST_N | FTP_CUST_PWR);
	if (ret) {
		dev_err(dev, "Failed to set PWR and RST_N bit: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CTRL_0_REG, 0);
	if (ret) {
		dev_err(dev, "Failed to reset NVM: %d", ret);
		return ret;
	}

	return 0;
}

static int stusb4500_exit_test_mode(struct stusb4500 *chip)
{
	struct device *dev = chip->dev;
	int ret;
	uint8_t clear_buf[2] = {FTP_CUST_RST_N, 0};

	ret = regmap_bulk_write(chip->regmap, FTP_CTRL_0_REG, clear_buf, 2);
	if (ret) {
		dev_err(dev, "Failed to clear registers: %d", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, FTP_CUST_PASSWORD_REG, 0);
	if (ret) {
		dev_err(dev, "Failed to clear password: %d", ret);
		return ret;
	}

	return 0;
}

static int stusb4500_program_nvm(struct stusb4500 *chip)
{
	struct device *dev = chip->dev;
	int ret, i;

	ret = stusb4500_enter_write_mode(chip,
			SECTOR_0 | SECTOR_1 | SECTOR_2 | SECTOR_3 | SECTOR_4);
	if (ret) {
		dev_err(dev, "Failed to enter write mode: %d", ret);
		return ret;
	}

	for (i = 0; i < SECTOR_NUM_MAX; i++) {
		ret = stusb4500_write_nvm_sector(chip, i, chip->nvm_config[i]);
		if (ret) {
			dev_err(dev, "Failed to write NVM sector: %d", ret);
			return ret;
		}
	}

	ret = stusb4500_exit_test_mode(chip);
	if (ret) {
		dev_err(dev, "Failed to exit test mode: %d", ret);
		return ret;
	}

	dev_info(chip->dev, "Programmed the NVM successfully");

	return 0;
}

static int stusb4500_read_nvm(struct stusb4500 *chip, char *buf)
{
	struct device *dev = chip->dev;
	int ret, len = 0;
	uint8_t i;

	dev_info(dev, "Start reading the NVM");

	ret = stusb4500_enter_read_mode(chip);
	if (ret) {
		dev_err(dev, "Failed to enter read mode: %d", ret);
		return ret;
	}

	for (i = 0; i < SECTOR_NUM_MAX; i++) {
		len += stusb4500_read_nvm_sector(chip, i, buf + len);
		if (len < 0) {
			dev_err(dev, "Failed to read NVM sector %d: %d", i, ret);
			return -EIO;
		}
	}

	ret = stusb4500_exit_test_mode(chip);
	if (ret) {
		dev_err(dev, "Failed to exit test mode: %d", ret);
		return ret;
	}

	dev_info(dev, "Read %d bytes from the NVM", len);

	return len;
}

static int stusb4500_sw_reset(struct stusb4500 *chip)
{
	struct device *dev = chip->dev;
	int ret;
	uint32_t val;

	mutex_lock(&chip->irq_complete);

	ret = regmap_write(chip->regmap, STUSB4500_RESET_CTRL, 1);
	if (ret) {
		dev_err(dev, "Failed to set the SW reset control register: %d", ret);
		goto sw_reset_exit;
	}

	ret = regmap_read(chip->regmap, STUSB4500_ALERT_STATUS_1, &val);
	if (ret) {
		dev_err(dev, "Failed to read the alert status register: %d", ret);
		goto sw_reset_exit;
	}

	ret = regmap_write(chip->regmap, STUSB4500_RESET_CTRL, 0);
	if (ret) {
		dev_err(dev, "Failed to clear the SW reset control register: %d", ret);
		goto sw_reset_exit;
	}

	dev_info(dev, "Chip reset completed");

sw_reset_exit:
	mutex_unlock(&chip->irq_complete);
	return ret;
}

static ssize_t device_attached_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);
	int ret, val;
	static const char * const name[] = {
		"none", "sink", "source", "debug", "audio", "power accessory",
	};

	ret = regmap_read(chip->regmap, STUSB4500_PORT_STATUS_1, &val);
	if (ret) {
		dev_err(dev, "Failed to get the attached device status: %d", ret);
		return ret;
	}

	val = (STUSB4500_PORT_ATTACHED_DEVICE & val) >> 5;
	if (val < 0 || val >= ARRAY_SIZE(name)) {
		dev_err(dev, "Invalid attached device detected");
		return -EINVAL;
	}

	/* Max buffer size is PAGE_SIZE, see Documentation/filesystems/sysfs.txt */
	ret = snprintf(buf, PAGE_SIZE, "%s\n", name[val]);
	if (ret > PAGE_SIZE)
		ret = PAGE_SIZE;
	return ret;
}
static DEVICE_ATTR_RO(device_attached);

static ssize_t program_nvm_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);
	int ret, val;

	if (sscanf(buf, "%d\n", &val) != 1 || val != 1)
		return -EINVAL;

	ret = stusb4500_program_nvm(chip);
	if (ret) {
		dev_err(chip->dev,
			"Failed to program the NVM: %d", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(program_nvm);

static ssize_t read_nvm_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);
	int ret;

	ret = stusb4500_read_nvm(chip, buf);
	if (ret < 0)
		dev_err(dev, "Failed to read NVM: %d", ret);
	return ret;
}
static DEVICE_ATTR_RO(read_nvm);

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);
	int ret, val;

	if (sscanf(buf, "%d\n", &val) != 1 || val != 1)
		return -EINVAL;

	ret = stusb4500_sw_reset(chip);
	if (ret) {
		dev_err(dev, "Failed to reset the chip: %d", ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(reset);

static ssize_t cc_status_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);
	int ret, val;

	ret = regmap_read(chip->regmap, STUSB4500_CC_STATUS, &val);
	if (ret) {
		dev_err(dev, "Failed to get STUSB4500_CC_STATUS: %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "STUSB4500_CC_STATUS register value: 0x%x\n", val);

	if (!(val & STUSB4500_CONNECT_RESULT)) {
		ret = snprintf(buf, PAGE_SIZE, "not connected\n");
		goto exit;
	}

	switch ((val & STUSB4500_CC1_STATE) | (val & STUSB4500_CC2_STATE)) {
	case 0x1:
		ret = snprintf(buf, PAGE_SIZE, "CC1, USB default\n");
		break;
	case 0x2:
		ret = snprintf(buf, PAGE_SIZE, "CC1, 1.5A\n");
		break;
	case 0x3:
		ret = snprintf(buf, PAGE_SIZE, "CC1, 3A\n");
		break;
	case 0x4:
		ret = snprintf(buf, PAGE_SIZE, "CC2, USB default\n");
		break;
	case 0x8:
		ret = snprintf(buf, PAGE_SIZE, "CC2, 1.5A\n");
		break;
	case 0xc:
		ret = snprintf(buf, PAGE_SIZE, "CC2, 3A\n");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "device not supported\n");
		break;
	}

exit:
	if (ret > PAGE_SIZE)
		ret = PAGE_SIZE;
	return ret;
}
static DEVICE_ATTR_RO(cc_status);

static int stusb4500_create_attrs(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_device_attached);
	ret |= device_create_file(dev, &dev_attr_program_nvm);
	ret |= device_create_file(dev, &dev_attr_read_nvm);
	ret |= device_create_file(dev, &dev_attr_reset);
	ret |= device_create_file(dev, &dev_attr_cc_status);
	return ret;
}

static irqreturn_t stusb4500_irq_handler_thread(int irq, void *private)
{
	struct stusb4500 *chip = private;
	struct device *dev = chip->dev;
	int ret;
	uint32_t val;

	mutex_lock(&chip->irq_complete);

	chip->irq_waiting = true;

	if (!chip->resume_completed) {
		dev_dbg(dev, "IRQ triggered before device resume");
		disable_irq_nosync(irq);
		mutex_unlock(&chip->irq_complete);
		return IRQ_HANDLED;
	}

	ret = regmap_read(chip->regmap, STUSB4500_ALERT_STATUS_1, &val);
	if (ret) {
		dev_err(dev, "Failed to read alert status register: %d", ret);
		goto irq_out;
	}

	if (val & STUSB4500_PRT_STATUS)
		dev_info(dev, "Received PRT_STATUS alert");
	else if (val & STUSB4500_CC_HW_FAULT_STATUS)
		dev_info(dev, "Received CC_HW_FAULT_STATUS alert");
	else if (val & STUSB4500_TYPEC_MONITORING_STATUS)
		dev_info(dev, "Received TYPEC_MONITORING_STATUS alert");
	else if (val & STUSB4500_PORT_STATUS)
		dev_info(dev, "Received PORT_STATUS alert");
	else
		dev_warn(dev, "Received invalid alert");

irq_out:
	chip->irq_waiting = false;
	mutex_unlock(&chip->irq_complete);
	return IRQ_HANDLED;
}

static int stusb4500_chip_init(struct stusb4500 *chip)
{
	struct device *dev = chip->dev;
	int ret;
	uint8_t buf[16] = {0};
	uint32_t reg;

	ret = stusb4500_create_attrs(dev);
	if (ret) {
		dev_err(dev, "Failed to create attributes ret=%d", ret);
		return ret;
	}

	ret = regmap_bulk_read(chip->regmap, STUSB4500_PORT_STATUS_0, buf, 10);
	ret |= regmap_write(chip->regmap, STUSB4500_ALERT_STATUS_1_MASK, 0xff);
	if (ret) {
		dev_err(dev, "Failed to clear alert status: %d", ret);
		return ret;
	}

	ret = regmap_read(chip->regmap, STUSB4500_DEVICE_ID, &reg);
	if (ret) {
		dev_err(dev, "Failed to get device ID: %d", ret);
		return ret;
	}

	if (reg != STUSB4500_DEVICE_ID_VAL) {
		dev_err(dev, "Invalid device ID 0x%x read", reg);
		return -ENODEV;
	}
	dev_dbg(dev, "STUSB4500 device ID: 0x%x", reg);

	return 0;
}

static const char stusb4500_name[] = "stusb4500";
static const char stusb4500l_name[] = "stusb4500l";

static const struct of_device_id stusb4500_of_match[] = {
	{ .compatible = "st,stusb4500", .data = stusb4500_name },
	{ .compatible = "st,stusb4500l", .data = stusb4500l_name },
	{ },
};

static int stusb4500_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct stusb4500 *chip;
	const struct of_device_id *of_id;
	const char *dev_name;
	int ret;

	of_id = of_match_device(stusb4500_of_match, dev);
	if (!of_id) {
		dev_err(dev, "Failed to match the device tree node");
		return -EINVAL;
	}
	dev_name = of_id->data;

	dev_dbg(dev, "%s probe enter", dev_name);

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	dev_set_drvdata(dev, chip);

	ret = of_property_read_u8_array(dev->of_node, "nvm-config",
			(uint8_t *)chip->nvm_config,
			SECTOR_BLOCK_SIZE * SECTOR_NUM_MAX);
	if (ret) {
		dev_err(dev, "Failed to read the nvm-config property ret=%d", ret);
		return ret;
	}

	chip->regmap = devm_regmap_init_i2c(client, &stusb4500_regmap_config);
	if (IS_ERR_OR_NULL(chip->regmap)) {
		dev_err(dev, "Failed to allocate register map: %ld", PTR_ERR(chip->regmap));
		return PTR_ERR(chip->regmap);
	}

	chip->dev = dev;
	chip->irq = client->irq;

	mutex_init(&chip->lock);
	mutex_init(&chip->irq_complete);

	if (chip->irq) {
		ret = devm_request_threaded_irq(dev, chip->irq, NULL,
						stusb4500_irq_handler_thread,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"stusb4500_alert", chip);
		if (ret) {
			dev_err(dev, "IRQ request failed: %d", ret);
			return ret;
		}
		enable_irq_wake(client->irq);
	}

	ret = stusb4500_chip_init(chip);
	if (ret) {
		dev_err(dev, "Failed to init the USB controller info: %d", ret);
		return ret;
	}

	device_init_wakeup(dev, true);

	dev_info(dev, "%s probe complete", dev_name);

	return 0;
}

static int stusb4500_remove(struct i2c_client *client)
{
	device_init_wakeup(&client->dev, false);

	return 0;
}

static int stusb4500_suspend(struct device *dev)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = false;
	mutex_unlock(&chip->irq_complete);

	return 0;
}

static int stusb4500_suspend_noirq(struct device *dev)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);

	if (chip->irq_waiting) {
		dev_err(dev, "Aborting suspend, an interrupt was detected while suspending");
		return -EBUSY;
	}

	return 0;
}

static int stusb4500_resume(struct device *dev)
{
	struct stusb4500 *chip = dev_get_drvdata(dev);

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = true;
	mutex_unlock(&chip->irq_complete);
	if (chip->irq_waiting) {
		stusb4500_irq_handler_thread(chip->irq, chip);
		enable_irq(chip->irq);
	}

	return 0;
}

static const struct dev_pm_ops stusb4500_pm_ops = {
	.suspend =  stusb4500_suspend,
	.suspend_noirq = stusb4500_suspend_noirq,
	.resume =  stusb4500_resume,
};

static struct i2c_driver stusb4500_driver = {
	.driver = {
		.name = "stusb4500",
		.of_match_table = stusb4500_of_match,
		.owner = THIS_MODULE,
		.pm = &stusb4500_pm_ops,
	},
	.probe = stusb4500_probe,
	.remove = stusb4500_remove,
};

module_i2c_driver(stusb4500_driver);

MODULE_DESCRIPTION("STMicroelectronics STUSB4500 and STUSB4500L USB Controller Driver");
MODULE_LICENSE("GPL v2");
