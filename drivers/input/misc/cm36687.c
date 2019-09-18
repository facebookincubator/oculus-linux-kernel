/* drivers/input/misc/cm36687.c - cm36687 optical sensors driver
 *
 * Copyright (C) 2017 Vishay Capella Microsystems Limited
 * Author: Frank Hsieh <Frank.Hsieh@vishay.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <asm/setup.h>
#include <linux/capella_cm3602.h>
#include <linux/cm36687.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/jiffies.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define D(x...) pr_info(x)

#define I2C_RETRY_COUNT 10

#define CONTROL_INT_ISR_REPORT 0x00
#define CONTROL_PS 0x02

/* for debug usage to dump ps adc value */
#define CM36687_DEBUG_PS_ADC

static int record_init_fail;
static void sensor_irq_do_work(struct work_struct *work);
static DECLARE_WORK(sensor_irq_work, sensor_irq_do_work);

struct cm36687_info {
	struct class *cm36687_class;
	struct device *ps_dev;
	struct input_dev *ps_input_dev;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	struct i2c_client *i2c_client;
	struct workqueue_struct *lp_wq;

	int intr_pin;
	int ps_enable;
	int irq;
	int (*power)(int, uint8_t); /* power to the chip */

#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock ps_wake_lock;
#endif
	int psensor_opened;
	uint8_t slave_addr;
	uint16_t ps_close_thd_set;
	uint16_t ps_away_thd_set;
	uint16_t inte_cancel_set;
	uint16_t ps_conf1_val;
	uint16_t ps_conf3_val;
	uint16_t ps_conf5_val;
	struct regulator *vdd_core;
};
struct cm36687_info *lp_info;
static struct mutex ps_enable_mutex, ps_disable_mutex, ps_get_adc_mutex;
static struct mutex CM36687_control_mutex;
static int initial_cm36687(struct cm36687_info *lpi);
static void psensor_initial_cmd(struct cm36687_info *lpi);

static int control_and_report(struct cm36687_info *lpi, uint8_t mode,
	uint16_t param);

static int I2C_RxData(uint16_t slaveAddr, uint8_t cmd, uint8_t *rxData,
	int length)
{
	uint8_t loop_i;
	int val;
	struct cm36687_info *lpi = lp_info;
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr = slaveAddr,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
		{
			.addr = slaveAddr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		ret = i2c_transfer(lp_info->i2c_client->adapter, msgs, 2);
		if (ret > 0)
			break;

		val = gpio_get_value(lpi->intr_pin);
		/*check intr GPIO when i2c error*/
		if (loop_i == 0 || loop_i == I2C_RETRY_COUNT - 1)
			D("[PS][CM36687] %s, slaveAddr 0x%x gpio %d = %d rif %d ret %d\n",
				__func__, slaveAddr, lpi->intr_pin, val, record_init_fail, ret);

		usleep_range(10000, 10010);
	}
	if (loop_i >= I2C_RETRY_COUNT) {
		pr_err("[PS_ERR][CM36687 error] %s retry over %d\n",
			__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int I2C_TxData(uint16_t slaveAddr, uint8_t *txData, int length)
{
	uint8_t loop_i;
	int val;
	struct cm36687_info *lpi = lp_info;
	struct i2c_msg msg[] = {
		{
			.addr = slaveAddr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		if (i2c_transfer(lp_info->i2c_client->adapter, msg, 1) > 0)
			break;

		val = gpio_get_value(lpi->intr_pin);
		/*check intr GPIO when i2c error*/
		if (loop_i == 0 || loop_i == I2C_RETRY_COUNT - 1)
			D("[PS][CM36687] %s, slaveAddr 0x%x, value 0x%x, gpio%d=%d, rif %d\n",
				__func__, slaveAddr, txData[0], lpi->intr_pin, val, record_init_fail);

		usleep_range(10000, 10010);
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		pr_err("[PS_ERR][CM36687 error] %s retry over %d\n",
			__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int _cm36687_I2C_Read_Word(uint16_t slaveAddr, uint8_t cmd,
	uint16_t *pdata)
{
	uint8_t buffer[2];
	int ret = 0;

	if (pdata == NULL)
		return -EFAULT;

	ret = I2C_RxData(slaveAddr, cmd, buffer, 2);
	if (ret < 0) {
		pr_err(
			"[PS_ERR][CM36687 error]%s: I2C_RxData fail [0x%x, 0x%x]\n",
			__func__, slaveAddr, cmd);
		return ret;
	}

	*pdata = (buffer[1] << 8) | buffer[0];
	pr_debug("[CM36687] %s: I2C_RxData[0x%x, 0x%x] = 0x%x\n",
		__func__, slaveAddr, cmd, *pdata);

	return ret;
}

static int _cm36687_I2C_Write_Word(uint16_t SlaveAddress, uint8_t cmd,
	uint16_t data)
{
	char buffer[3];
	int ret = 0;

	pr_debug("%s: _cm36687_I2C_Write_Word[0x%x, 0x%x, 0x%x]\n",
		__func__, SlaveAddress, cmd, data);

	buffer[0] = cmd;
	buffer[1] = (uint8_t)(data & 0xff);
	buffer[2] = (uint8_t)((data & 0xff00) >> 8);

	ret = I2C_TxData(SlaveAddress, buffer, 3);
	if (ret < 0) {
		pr_err("[PS_ERR][CM36687 error]%s: I2C_TxData fail\n", __func__);
		return -EIO;
	}

	return ret;
}

static int get_ps_adc_value(uint16_t *data)
{
	int ret = 0;
	struct cm36687_info *lpi = lp_info;

	if (data == NULL)
		return -EFAULT;

	ret = _cm36687_I2C_Read_Word(lpi->slave_addr, PS_DATA, data);

	if (ret < 0) {
		pr_err(
			"[PS][CM36687 error]%s: _cm36687_I2C_Read_Word fail, ret: %d\n",
			__func__, ret);
		return -EIO;
	}

	return ret;
}

static void sensor_irq_do_work(struct work_struct *work)
{
	struct cm36687_info *lpi = lp_info;
	uint16_t dummy = 0;

	control_and_report(lpi, CONTROL_INT_ISR_REPORT, dummy);

	enable_irq(lpi->irq);
}

static irqreturn_t cm36687_irq_handler(int irq, void *data)
{
	struct cm36687_info *lpi = data;

	disable_irq_nosync(lpi->irq);
	queue_work(lpi->lp_wq, &sensor_irq_work);

	return IRQ_HANDLED;
}

static int ps_power(int enable)
{
	struct cm36687_info *lpi = lp_info;

	if (lpi->power)
		lpi->power(PS_PWR_ON, 1);

	return 0;
}

static void psensor_initial_cmd(struct cm36687_info *lpi)
{

	/*must disable p-sensor interrupt befrore IST create*/
	lpi->ps_conf1_val |= CM36687_PS_SD;
	lpi->ps_conf1_val &= CM36687_PS_INT_MASK;
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_CONF1,
		lpi->ps_conf1_val);
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_CONF3,
		lpi->ps_conf3_val);
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_CONF5,
		lpi->ps_conf5_val);
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_THDL,
		lpi->ps_away_thd_set);
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_THDH,
		lpi->ps_close_thd_set);

	D("[PS][CM36687] PS_CONF1: 0x%04x\n", lpi->ps_conf1_val);
	D("[PS][CM36687] %s, finish\n", __func__);
}

static int psensor_enable(struct cm36687_info *lpi)
{
	int ret = -EIO;

	mutex_lock(&ps_enable_mutex);
	D("[PS][CM36687] %s\n", __func__);

	if (lpi->ps_enable) {
		D("[PS][CM36687] %s: already enabled\n", __func__);
		ret = 0;
	} else
		ret = control_and_report(lpi, CONTROL_PS, 1);
	mutex_unlock(&ps_enable_mutex);

	return ret;
}

static int psensor_disable(struct cm36687_info *lpi)
{
	int ret = -EIO;

	mutex_lock(&ps_disable_mutex);
	D("[PS][CM36687] %s\n", __func__);

	if (lpi->ps_enable == 0) {
		D("[PS][CM36687] %s: already disabled\n", __func__);
		ret = 0;
	} else
		ret = control_and_report(lpi, CONTROL_PS, 0);
	mutex_unlock(&ps_disable_mutex);

	return ret;
}

static int psensor_open(struct inode *inode, struct file *file)
{
	struct cm36687_info *lpi = lp_info;

	D("[PS][CM36687] %s\n", __func__);

	if (lpi->psensor_opened)
		return -EBUSY;

	lpi->psensor_opened = 1;

	return 0;
}

static int psensor_release(struct inode *inode, struct file *file)
{
	struct cm36687_info *lpi = lp_info;

	D("[PS][CM36687] %s\n", __func__);

	lpi->psensor_opened = 0;

	return psensor_disable(lpi);
}

static long psensor_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int val;
	struct cm36687_info *lpi = lp_info;

	D("[PS][CM36687] %s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case CAPELLA_CM3602_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg))
			return -EFAULT;
		if (val)
			return psensor_enable(lpi);
		else
			return psensor_disable(lpi);
		break;
	case CAPELLA_CM3602_IOCTL_GET_ENABLED:
		return put_user(lpi->ps_enable, (unsigned long __user *)arg);
	default:
		pr_err("[PS][CM36687 error]%s: invalid cmd %d\n",
			__func__, _IOC_NR(cmd));
		return -EINVAL;
	}
}

static const struct file_operations psensor_fops = {
	.owner = THIS_MODULE,
	.open = psensor_open,
	.release = psensor_release,
	.unlocked_ioctl = psensor_ioctl
};

struct miscdevice psensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "proximity",
	.fops = &psensor_fops
};

static ssize_t ps_adc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint16_t value;
	int ret;
	struct cm36687_info *lpi = lp_info;
	int intr_val;

	intr_val = gpio_get_value(lpi->intr_pin);

	get_ps_adc_value(&value);

	D("[PS][CM36687]: DEC ADC[%d], ENABLE = %d, intr_pin = %d\n", value, lpi->ps_enable, intr_val);
	if (record_init_fail == 1)
		ret = snprintf(buf, 5, "%d\n", -1);
	else
		ret = snprintf(buf, 5, "%d\n", value);

	return ret;
}

static ssize_t ps_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	int ret = 0;
	struct cm36687_info *lpi = lp_info;

	ret = snprintf(buf, 5, "%d\n", lpi->ps_enable);

	return ret;
}

static ssize_t ps_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ps_en;
	struct cm36687_info *lpi = lp_info;

	ps_en = -1;
	if (kstrtoint(buf, 10, &ps_en) != 0)
		return -EINVAL;

	if (ps_en != 0 && ps_en != 1)
		return -EINVAL;

	D("[PS][CM36687] %s: ps_en=%d\n", __func__, ps_en);

	if (ps_en)
		psensor_enable(lpi);
	else
		psensor_disable(lpi);

	return count;
}

static ssize_t ps_conf_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36687_info *lpi = lp_info;

	return sprintf(buf, "PS_CONF1 = 0x%04x, PS_CONF3 = 0x%04x, PS_CONF5 = 0x%04x\n", lpi->ps_conf1_val, lpi->ps_conf3_val, lpi->ps_conf5_val);
}

static ssize_t ps_conf_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int code1, code2, code3;
	struct cm36687_info *lpi = lp_info;

	sscanf(buf, "0x%x 0x%x 0x%x", &code1, &code2, &code3);

	D("[PS]%s: store value PS conf1 reg = 0x%04x PS conf3 reg = 0x%04x, PS_CONF5 = 0x%04x\n", __func__, code1, code2, code3);

	lpi->ps_conf1_val = code1;
	lpi->ps_conf3_val = code2;
	lpi->ps_conf5_val = code3;

	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val);
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_CONF5, lpi->ps_conf5_val);
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);

	return count;
}

static ssize_t ps_thdh_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm36687_info *lpi = lp_info;

	ret = snprintf(buf, 5, "%d\n", lpi->ps_close_thd_set);
	return ret;
}

static ssize_t ps_thdh_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int code1;
	int ps_en;
	struct cm36687_info *lpi = lp_info;

	if (sscanf(buf, "0x%x", &code1) != 1)
		return 0;

	lpi->ps_close_thd_set = code1;

	ps_en = lpi->ps_enable;

	if (ps_en)
		psensor_disable(lpi);

	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_THDH,
			lpi->ps_close_thd_set);

	if (ps_en)
		psensor_enable(lpi);

	D("[PS][CM36687]%s: ps_close_thd_set = 0x%04x(%d)\n", __func__,
		lpi->ps_close_thd_set,
		lpi->ps_close_thd_set);

	return count;
}

static ssize_t ps_thdl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm36687_info *lpi = lp_info;

	ret = snprintf(buf, 5, "%d\n", lpi->ps_away_thd_set);
	return ret;
}

static ssize_t ps_thdl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int code2;
	int ps_en;
	struct cm36687_info *lpi = lp_info;

	if (sscanf(buf, "0x%x", &code2) == 0)
		return 0;

	lpi->ps_away_thd_set = code2;
	ps_en = lpi->ps_enable;

	if (ps_en)
		psensor_disable(lpi);

	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_THDL,
			lpi->ps_away_thd_set);

	if (ps_en)
		psensor_enable(lpi);

	D("[PS][CM36687]%s: ps_away_thd_set = 0x%04x(%d)\n", __func__,
		lpi->ps_away_thd_set,
		lpi->ps_away_thd_set);

	return count;
}

static ssize_t ps_thd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm36687_info *lpi = lp_info;
	ret = sprintf(buf, "[PS][CM36687]PS Hi/Low THD ps_close_thd_set = 0x%04x(%d), ps_away_thd_set = 0x%04x(%d)\n",
		lpi->ps_close_thd_set,
		lpi->ps_close_thd_set,
		lpi->ps_away_thd_set,
		lpi->ps_away_thd_set);
	return ret;
}

static ssize_t ps_thd_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int code1, code2;
	int ps_en;
	struct cm36687_info *lpi = lp_info;

	sscanf(buf, "0x%x 0x%x", &code1, &code2);

	lpi->ps_close_thd_set = code1;
	lpi->ps_away_thd_set = code2;
	ps_en = lpi->ps_enable;

	if (ps_en)
		psensor_disable(lpi);

	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_THDH,
			lpi->ps_close_thd_set);
	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_THDL,
			lpi->ps_away_thd_set);

	if (ps_en)
		psensor_enable(lpi);

	D("[PS][CM36687]%s: ps_close_thd_set = 0x%04x(%d), ps_away_thd_set = 0x%04x(%d)\n",
		__func__,
		lpi->ps_close_thd_set,
		lpi->ps_close_thd_set,
		lpi->ps_away_thd_set,
		lpi->ps_away_thd_set);

	return count;
}

static ssize_t ps_canc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct cm36687_info *lpi = lp_info;

	ret = sprintf(buf, "[PS][CM36687]PS_CANC = 0x%04x(%d)\n",
		lpi->inte_cancel_set,
		lpi->inte_cancel_set);

	return ret;
}

static ssize_t ps_canc_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int code;
	int ps_en;
	struct cm36687_info *lpi = lp_info;

	sscanf(buf, "0x%x", &code);

	D("[PS][CM36687]PS_CANC: store value = 0x%04x(%d)\n", code, code);

	lpi->inte_cancel_set = code;
	ps_en = lpi->ps_enable;

	if (ps_en)
		psensor_disable(lpi);

	_cm36687_I2C_Write_Word(lpi->slave_addr, PS_CANC, lpi->inte_cancel_set);

	if (ps_en)
		psensor_enable(lpi);

	return count;
}

static struct device_attribute dev_attr_ps_adc =
	__ATTR(ps_adc, 0444, ps_adc_show, NULL);

static struct device_attribute dev_attr_ps_enable =
	__ATTR(ps_enable, 0664, ps_enable_show, ps_enable_store);

static struct device_attribute dev_attr_ps_conf =
	__ATTR(ps_conf, 0664, ps_conf_show, ps_conf_store);

static struct device_attribute dev_attr_ps_thd =
	__ATTR(ps_thd, 0664, ps_thd_show, ps_thd_store);

static struct device_attribute dev_attr_ps_thdh =
	__ATTR(ps_thdh, 0664, ps_thdh_show, ps_thdh_store);

static struct device_attribute dev_attr_ps_thdl =
	__ATTR(ps_thdl, 0664, ps_thdl_show, ps_thdl_store);

static struct device_attribute dev_attr_ps_canc =
	__ATTR(ps_canc, 0664, ps_canc_show, ps_canc_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_ps_adc.attr,
	&dev_attr_ps_enable.attr,
	&dev_attr_ps_conf.attr,
	&dev_attr_ps_thd.attr,
	&dev_attr_ps_thdh.attr,
	&dev_attr_ps_thdl.attr,
	&dev_attr_ps_canc.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static int psensor_setup(struct cm36687_info *lpi)
{
	int ret;

	lpi->ps_input_dev = input_allocate_device();
	if (!lpi->ps_input_dev) {
		pr_err("[PS][CM36687 error]%s: could not allocate ps input device\n", __func__);
		return -ENOMEM;
	}

	lpi->ps_input_dev->name = "cm36687-ps";
	set_bit(EV_ABS, lpi->ps_input_dev->evbit);
	input_set_abs_params(lpi->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(lpi->ps_input_dev);
	if (ret < 0) {
		pr_err("[PS][CM36687 error]%s: could not register ps input device\n", __func__);
		goto err_free_ps_input_device;
	}

	ret = misc_register(&psensor_misc);
	if (ret < 0) {
		pr_err("[PS][CM36687 error]%s: could not register ps misc device\n", __func__);
		goto err_unregister_ps_input_device;
	}

	return ret;

err_unregister_ps_input_device:
	input_unregister_device(lpi->ps_input_dev);
err_free_ps_input_device:
	input_free_device(lpi->ps_input_dev);
	return ret;
}

static int initial_cm36687(struct cm36687_info *lpi)
{
	int val;
	int ret;
	uint16_t idReg;

	val = gpio_get_value(lpi->intr_pin);
	D("[PS][CM36687] %s, INTERRUPT GPIO val = %d\n", __func__, val);
	D("[PS][CM36687] address: %04x\n", lpi->slave_addr);

	ret = _cm36687_I2C_Read_Word(lpi->slave_addr, ID_REG, &idReg);
	idReg &= 0xFF;
	if ((ret < 0) || (idReg != 0x0088)) {
		if (record_init_fail == 0)
			record_init_fail = 1;
		D("[PS][CM36687] initial_cm36687: cannot read ID_REF\n");
	}

	D("[PS][CM36687]: ID_REG: %04x\n", idReg);
	return 0;
}

static int cm36687_setup(struct cm36687_info *lpi)
{
	int ret = 0;

	ps_power(1);
	usleep_range(5000, 5050);
	ret = gpio_request(lpi->intr_pin, "gpio_cm36687_intr");
	if (ret < 0) {
		pr_err("[PS][CM36687 error]%s: gpio %d request failed (%d)\n",
			__func__, lpi->intr_pin, ret);
		return ret;
	}

	ret = gpio_direction_input(lpi->intr_pin);
	if (ret < 0) {
		pr_err("[PS][CM36687 error]%s: fail to set gpio %d as input (%d)\n",
			__func__, lpi->intr_pin, ret);
		goto fail_free_intr_pin;
	}

	ret = initial_cm36687(lpi);
	if (ret < 0) {
		pr_err("[PS_ERR][CM36687 error]%s: fail to initial cm36687 (%d)\n",
			__func__, ret);
		goto fail_free_intr_pin;
	}

	/*Default disable P sensor*/
	psensor_initial_cmd(lpi);

	ret = request_any_context_irq(lpi->irq,
		cm36687_irq_handler,
		IRQF_TRIGGER_LOW,
		"cm36687",
		lpi);
	if (ret < 0) {
		pr_err("[PS][CM36687 error]%s: req_irq(%d) fail for gpio %d (%d)\n",
			__func__, lpi->irq,
			lpi->intr_pin, ret);
		goto fail_free_intr_pin;
	}

	return ret;

fail_free_intr_pin:
	gpio_free(lpi->intr_pin);
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cm36687_early_suspend(struct early_suspend *h)
{
	struct cm36687_info *lpi = lp_info;

	D("[PS][CM36687] %s\n", __func__);

	if (lpi->ps_enable)
		psensor_disable(lpi);
}

static void cm36687_late_resume(struct early_suspend *h)
{
	struct cm36687_info *lpi = lp_info;

	D("[PS][CM36687] %s\n", __func__);

	if (!lpi->ps_enable)
		psensor_enable(lpi);
}
#endif

#ifdef CONFIG_OF
static int cm36687_parse_dt(struct device *dev,
	struct cm36687_info *lpi)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int rc;

	D("[PS][CM36687] %s\n", __func__);

	rc = of_get_named_gpio_flags(np, "capella,intrpin-gpios",
		0, NULL);
	if (rc < 0) {
		dev_err(dev, "Unable to read interrupt pin number\n");
		return rc;
	}

	lpi->intr_pin = rc;
	D("[PS][CM36687]%s GET INTR PIN\n", __func__);

	rc = of_property_read_u32(np, "capella,slave_address", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read slave_address\n");
		return rc;
	}
	lpi->slave_addr = (uint8_t)temp_val;

	D("[PS][CM36687]%s PARSE OK\n", __func__);

	return 0;
}
#endif

static int cm36687_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct cm36687_info *lpi;
#ifndef CONFIG_OF
	struct cm36687_platform_data *pdata;
#endif

	D("[PS][CM36687] %s\n", __func__);

	lpi = kzalloc(sizeof(struct cm36687_info), GFP_KERNEL);
	if (!lpi)
		return -ENOMEM;

	lpi->i2c_client = client;
	lpi->irq = client->irq;
	i2c_set_clientdata(client, lpi);

	/* L28 regulator 1.8V enable for proximity sensor*/
	lpi->vdd_core = regulator_get(&client->dev, "cm36687,vdd-core");
	if (IS_ERR(lpi->vdd_core))
		D("[PS][CM36687]: L28 regulator getting error!!\n");

	ret = regulator_set_voltage(lpi->vdd_core, 1808000, 1808000);
	if (ret < 0)
		D("[PS][CM36687]: L28 regulator setting voltage failed!!\n");

	ret = regulator_enable(lpi->vdd_core);
	if (ret < 0)
		D("[PS][CM36687]: L28 regulator enable failed!!\n");

#ifndef CONFIG_OF
	pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("[PS][CM36687 error]%s: Assign platform_data error!!\n",
			__func__);
		ret = -EBUSY;
		goto err_platform_data_null;
	}

	lpi->intr_pin = pdata->intr;
	lpi->power = pdata->power;
	lpi->slave_addr = pdata->slave_addr;

	lpi->ps_away_thd_set = pdata->ps_away_thd_set;
	lpi->ps_close_thd_set = pdata->ps_close_thd_set;
	lpi->ps_conf1_val = pdata->ps_conf1_val;
	lpi->ps_conf3_val = pdata->ps_conf3_val;
	lpi->ps_conf5_val = pdata->ps_conf5_val;
#else
	if (cm36687_parse_dt(&client->dev, lpi) < 0) {
		ret = -EBUSY;
		goto err_platform_data_null;
	}

	lpi->ps_away_thd_set = 0x7;
	lpi->ps_close_thd_set = 0xF;
	/* PS_IT = PS_IT at 8T, PS_ITB = 1/2T, PS_MPS = 1,
	 * 8ms sample period, 1 interrupt persistence */
	lpi->ps_conf1_val = CM36687_PS_IT_8T | CM36687_PS_PERIOD_8 | CM36687_PS_PERS_1;
	/* Active Force Mode disabled, 25mA VCSEL current setting */
	lpi->ps_conf3_val = CM36687_LED_I_25;
	lpi->ps_conf5_val = CM36687_POR_S;
	lpi->power = NULL;
#endif
	lp_info = lpi;
	mutex_init(&CM36687_control_mutex);
	mutex_init(&ps_enable_mutex);
	mutex_init(&ps_disable_mutex);
	mutex_init(&ps_get_adc_mutex);

	ret = psensor_setup(lpi);
	if (ret < 0) {
		pr_err("[PS][CM36687 error]%s: psensor_setup error!!\n",
			__func__);
		goto err_psensor_setup;
	}

	lpi->lp_wq = create_singlethread_workqueue("cm36687_wq");
	if (!lpi->lp_wq) {
		pr_err("[PS][CM36687 error]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&(lpi->ps_wake_lock), WAKE_LOCK_SUSPEND, "proximity");
#endif

	ret = cm36687_setup(lpi);
	if (ret < 0) {
		pr_err("[PS_ERR][CM36687 error]%s: cm36687_setup error!\n", __func__);
		goto err_cm36687_setup;
	}

	lpi->cm36687_class = class_create(THIS_MODULE, "capella_sensors");
	if (IS_ERR(lpi->cm36687_class)) {
		ret = PTR_ERR(lpi->cm36687_class);
		lpi->cm36687_class = NULL;
		goto err_create_class;
	}

	/* register the attributes */
	lpi->ps_dev = device_create(lpi->cm36687_class,
		NULL, 0, "%s", "proximity");
	if (unlikely(IS_ERR(lpi->ps_dev))) {
		ret = PTR_ERR(lpi->ps_dev);
		lpi->ps_dev = NULL;
		goto err_create_ps_device;
	}

	/* register the attributes */
	ret = sysfs_create_group(&lpi->ps_input_dev->dev.kobj, &proximity_attribute_group);
	if (ret)
		goto err_sysfs_create_group_proximity;

#ifdef CONFIG_HAS_EARLYSUSPEND
	lpi->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	lpi->early_suspend.suspend = cm36687_early_suspend;
	lpi->early_suspend.resume = cm36687_late_resume;
	register_early_suspend(&lpi->early_suspend);
#endif

	D("[PS][CM36687] %s: Probe success!\n", __func__);

	return ret;

err_sysfs_create_group_proximity:
	device_destroy(lpi->cm36687_class, lpi->ps_dev->devt);
err_create_ps_device:
	class_destroy(lpi->cm36687_class);
err_create_class:
	gpio_free(lpi->intr_pin);
err_cm36687_setup:
	destroy_workqueue(lpi->lp_wq);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&(lpi->ps_wake_lock));
#endif
	input_unregister_device(lpi->ps_input_dev);
	input_free_device(lpi->ps_input_dev);
err_create_singlethread_workqueue:
err_psensor_setup:
	mutex_destroy(&CM36687_control_mutex);
	mutex_destroy(&ps_enable_mutex);
	mutex_destroy(&ps_disable_mutex);
	mutex_destroy(&ps_get_adc_mutex);
err_platform_data_null:
	kfree(lpi);
	return ret;
}

static int control_and_report(struct cm36687_info *lpi, uint8_t mode,
		uint16_t param)
{
	int ret = 0;
	uint16_t ps_data = 0;
	int val;

	mutex_lock(&CM36687_control_mutex);

	if (mode == CONTROL_INT_ISR_REPORT) {
		_cm36687_I2C_Read_Word(lpi->slave_addr, INT_FLAG, &param);
	} else if (mode == CONTROL_PS) {
		if (param) {
			lpi->ps_conf1_val &= CM36687_PS_SD_MASK;
			lpi->ps_conf1_val |= CM36687_PS_INT_IN_AND_OUT;
		} else {
			lpi->ps_conf1_val |= CM36687_PS_SD;
			lpi->ps_conf1_val &= CM36687_PS_INT_MASK;
		}

		ret = _cm36687_I2C_Write_Word(lpi->slave_addr, PS_CONF1,
				lpi->ps_conf1_val);
		lpi->ps_enable = param;

		if (param == 1)
			msleep(200);
	}

#define PS_CLOSE 1
#define PS_AWAY (1 << 1)
#define PS_CLOSE_AND_AWAY (PS_CLOSE + PS_AWAY)
	if (lpi->ps_enable) {
		int ps_status = 0;

		if (mode == CONTROL_PS)
			ps_status = PS_CLOSE_AND_AWAY;
		else if (mode == CONTROL_INT_ISR_REPORT) {
			if (param & INT_FLAG_PS_IF_CLOSE)
				ps_status |= PS_CLOSE;
			if (param & INT_FLAG_PS_IF_AWAY)
				ps_status |= PS_AWAY;
		}

		if (ps_status != 0) {
#ifdef CM36687_DEBUG_PS_ADC
			/* for debug usage */
			get_ps_adc_value(&ps_data);
			D("[PS][CM36687] PS DATA: %d\n", ps_data);
#endif

			switch (ps_status) {
			case PS_CLOSE_AND_AWAY:
#ifndef CM36687_DEBUG_PS_ADC
				get_ps_adc_value(&ps_data);
#endif
				val = (ps_data >= lpi->ps_close_thd_set) ? 0 : 1;
				break;
			case PS_AWAY:
				val = 1;
				D("[PS][CM36687] proximity detected object away\n");
				break;
			case PS_CLOSE:
				val = 0;
				D("[PS][CM36687] proximity detected object close\n");
				break;
			};

			input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, val);
			input_sync(lpi->ps_input_dev);
		}
	}

	mutex_unlock(&CM36687_control_mutex);
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int cm36687_suspend(struct device *dev)
{
	struct cm36687_info *lpi;

	lpi = dev_get_drvdata(dev);

/*
	 * Save sensor state and disable them,
	 * this is to ensure internal state flags are set correctly.
	 * device will power off after both sensors are disabled.
	 * P sensor will not be disabled because it is a wakeup sensor.
	 */

#ifdef CONFIG_HAS_WAKELOCK
	if (lpi->ps_enable)
		wake_lock(&lpi->ps_wake_lock);
#endif

	return 0;
}

static int cm36687_resume(struct device *dev)
{
	struct cm36687_info *lpi;

	lpi = dev_get_drvdata(dev);

#ifdef CONFIG_HAS_WAKELOCK
	if (lpi->ps_enable)
		wake_unlock(&(lpi->ps_wake_lock));
#endif

	return 0;
}
#endif

static UNIVERSAL_DEV_PM_OPS(cm36687_pm, cm36687_suspend, cm36687_resume, NULL);

static const struct i2c_device_id cm36687_i2c_id[] = {
	{ CM36687_I2C_NAME, 0 },
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id cm36687_match_table[] = {
	{
		.compatible = "capella,cm36687",
	},
	{},
};
#else
#define cm36687_match_table NULL
#endif

static struct i2c_driver cm36687_driver = {
	.id_table = cm36687_i2c_id,
	.probe = cm36687_probe,
	.driver = {
		.name = CM36687_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cm36687_pm,
		.of_match_table = of_match_ptr(cm36687_match_table),
	},
};

static int __init cm36687_init(void)
{
	return i2c_add_driver(&cm36687_driver);
}

static void __exit cm36687_exit(void)
{
	i2c_del_driver(&cm36687_driver);
}

module_init(cm36687_init);
module_exit(cm36687_exit);

MODULE_AUTHOR("Frank Hsieh <Frank.Hsieh@vishay.com>");
MODULE_DESCRIPTION("CM36687 Optical Sensor Driver");
MODULE_LICENSE("GPL v2");
