/*
 * Copyright (C) 2012 Invensense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/spi/spi.h>

#include "inv_mpu_iio.h"

#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
#endif

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{128, "ICM20608D"},
	{128, "ICM20690"},
	{128, "ICM20602"},
};

static int debug_mem_read_addr = 0x900;
static char debug_reg_addr = 0x6;

const char sensor_l_info[][30] = {
	"SENSOR_L_ACCEL",
	"SENSOR_L_GYRO",
	"SENSOR_L_MAG",
	"SENSOR_L_ALS",
	"SENSOR_L_SIXQ",
	"SENSOR_L_THREEQ",
	"SENSOR_L_NINEQ",
	"SENSOR_L_PEDQ",
	"SENSOR_L_GEOMAG",
	"SENSOR_L_PRESSURE",
	"SENSOR_L_GYRO_CAL",
	"SENSOR_L_MAG_CAL",
	"SENSOR_L_EIS_GYRO",
	"SENSOR_L_ACCEL_WAKE",
	"SENSOR_L_GYRO_WAKE",
	"SENSOR_L_MAG_WAKE",
	"SENSOR_L_ALS_WAKE",
	"SENSOR_L_SIXQ_WAKE",
	"SENSOR_L_NINEQ_WAKE",
	"SENSOR_L_PEDQ_WAKE",
	"SENSOR_L_GEOMAG_WAKE",
	"SENSOR_L_PRESSURE_WAKE",
	"SENSOR_L_GYRO_CAL_WAKE",
	"SENSOR_L_MAG_CAL_WAKE",
	"SENSOR_L_NUM_MAX",
};

/*
 * inv_firmware_loaded_store() -  calling this function will change
 *                        firmware load
 */
static ssize_t inv_firmware_loaded_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;

	if (data)
		return -EINVAL;
	st->chip_config.firmware_loaded = 0;

	return count;

}

static int inv_dry_run_dmp(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);

	st->smd.on = 1;
	inv_check_sensor_on(st);
	st->trigger_state = EVENT_TRIGGER;
	set_inv_enable(indio_dev);
	msleep(DRY_RUN_TIME);
	st->smd.on = 0;
	inv_check_sensor_on(st);
	st->trigger_state = EVENT_TRIGGER;
	set_inv_enable(indio_dev);

	return 0;
}

static int _dmp_bias_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data, output;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto dmp_bias_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_ACCEL_X_DMP_BIAS:
		return -ENOSYS;
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:
		return -ENOSYS;
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:
		return -ENOSYS;
	case ATTR_DMP_GYRO_X_DMP_BIAS:
		if (data)
			st->sensor_acurracy_flag[SENSOR_GYRO_ACCURACY] = true;
		result = write_be32_to_mem(st, data, D_EXT_GYRO_BIAS_X);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[0] = data;
		break;
	case ATTR_DMP_GYRO_Y_DMP_BIAS:
		result = write_be32_to_mem(st, data, D_EXT_GYRO_BIAS_Y);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[1] = data;
		break;
	case ATTR_DMP_GYRO_Z_DMP_BIAS:
		result = write_be32_to_mem(st, data, D_EXT_GYRO_BIAS_Z);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[2] = data;
		break;
	case ATTR_DMP_SC_AUTH:
	case ATTR_DMP_EIS_AUTH:
		result = write_be32_to_mem(st, data, st->aut_key_in);
		if (result)
			goto dmp_bias_store_fail;
		result = write_be32_to_mem(st, 0, st->aut_key_out);
		if (result)
			goto dmp_bias_store_fail;
		/* dry run DMP to get the auth key output */
		inv_dry_run_dmp(indio_dev);
		result = inv_switch_power_in_lp(st, true);
		if (result)
			goto dmp_bias_store_fail;
		result = read_be32_from_mem(st, &output, st->aut_key_out);
		if (result)
			goto dmp_bias_store_fail;
		inv_push_marker_to_buffer(st, EIS_CALIB_HDR, output);
		break;
	case ATTR_DMP_MAGN_X_DMP_BIAS:
		return -ENODEV;
	case ATTR_DMP_MAGN_Y_DMP_BIAS:
		return -ENODEV;
	case ATTR_DMP_MAGN_Z_DMP_BIAS:
		return -ENODEV;
	case ATTR_DMP_MISC_GYRO_RECALIBRATION:
		return -ENOSYS;
	case ATTR_DMP_MISC_ACCEL_RECALIBRATION:
		return -ENOSYS;
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD:
		return -ENOSYS;
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE:
		return -ENOSYS;
	case ATTR_DMP_DEBUG_MEM_READ:
		debug_mem_read_addr = data;
		break;
	case ATTR_DMP_DEBUG_MEM_WRITE:
		inv_write_2bytes(st, debug_mem_read_addr, data);
		break;
	default:
		break;
	}

dmp_bias_store_fail:
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}

static int inv_set_accel_bias_reg(struct inv_mpu_state *st,
				  int accel_bias, int axis)
{
	int accel_reg_bias;
	u8 addr;
	u8 d[2];
	int result = 0;

	switch (axis) {
	case 0:
		/* X */
		addr = REG_XA_OFFS_H;
		break;
	case 1:
		/* Y */
		addr = REG_YA_OFFS_H;
		break;
	case 2:
		/* Z* */
		addr = REG_ZA_OFFS_H;
		break;
	default:
		result = -EINVAL;
		goto accel_bias_set_err;
	}

	result = inv_plat_read(st, addr, 2, d);
	if (result)
		goto accel_bias_set_err;
	accel_reg_bias = ((int)d[0] << 8) | d[1];

	/* accel_bias is 2g scaled by 1<<16.
	 * Convert to 16g, and mask bit0 */
	accel_reg_bias -= ((accel_bias / 8 / 65536) & ~1);

	d[0] = (accel_reg_bias >> 8) & 0xff;
	d[1] = (accel_reg_bias) & 0xff;
	result = inv_plat_single_write(st, addr, d[0]);
	if (result)
		goto accel_bias_set_err;
	result = inv_plat_single_write(st, addr + 1, d[1]);
	if (result)
		goto accel_bias_set_err;

accel_bias_set_err:
	return result;
}

static int inv_set_gyro_bias_reg(struct inv_mpu_state *st,
				 const int gyro_bias, int axis)
{
	int gyro_reg_bias;
	u8 addr;
	u8 d[2];
	int result = 0;

	switch (axis) {
	case 0:
		/* X */
		addr = REG_XG_OFFS_USR_H;
		break;
	case 1:
		/* Y */
		addr = REG_YG_OFFS_USR_H;
		break;
	case 2:
		/* Z */
		addr = REG_ZG_OFFS_USR_H;
		break;
	default:
		result = -EINVAL;
		goto gyro_bias_set_err;
	}

	/* gyro_bias is 2000dps scaled by 1<<16.
	 * Convert to 1000dps */
	gyro_reg_bias = (-gyro_bias * 2 / 65536);

	d[0] = (gyro_reg_bias >> 8) & 0xff;
	d[1] = (gyro_reg_bias) & 0xff;
	result = inv_plat_single_write(st, addr, d[0]);
	if (result)
		goto gyro_bias_set_err;
	result = inv_plat_single_write(st, addr + 1, d[1]);
	if (result)
		goto gyro_bias_set_err;

gyro_bias_set_err:
	return result;
}

static int _bias_store(struct device *dev,
		       struct device_attribute *attr, const char *buf,
		       size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto bias_store_fail;
	switch (this_attr->address) {
	case ATTR_ACCEL_X_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 0);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[0] = data;
		break;
	case ATTR_ACCEL_Y_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 1);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[1] = data;
		break;
	case ATTR_ACCEL_Z_OFFSET:
		result = inv_set_accel_bias_reg(st, data, 2);
		if (result)
			goto bias_store_fail;
		st->input_accel_bias[2] = data;
		break;
	case ATTR_GYRO_X_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 0);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[0] = data;
		break;
	case ATTR_GYRO_Y_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 1);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[1] = data;
		break;
	case ATTR_GYRO_Z_OFFSET:
		result = inv_set_gyro_bias_reg(st, data, 2);
		if (result)
			goto bias_store_fail;
		st->input_gyro_bias[2] = data;
		break;
	default:
		break;
	}

bias_store_fail:
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}

static ssize_t inv_dmp_bias_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _dmp_bias_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_bias_store(struct device *dev,
			  struct device_attribute *attr, const char *buf,
			  size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _bias_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_debug_store(struct device *dev,
			   struct device_attribute *attr, const char *buf,
			   size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;

	switch (this_attr->address) {
	case ATTR_DMP_LP_EN_OFF:
		st->chip_config.lp_en_mode_off = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DMP_CLK_SEL:
		st->chip_config.clk_sel = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DEBUG_REG_ADDR:
		debug_reg_addr = data;
		break;
	case ATTR_DEBUG_REG_WRITE:
		inv_plat_single_write(st, debug_reg_addr, data);
		break;
	case ATTR_DEBUG_WRITE_CFG:
		break;
	}
	return count;
}

static int _misc_attr_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_LOW_POWER_GYRO_ON:
		st->chip_config.low_power_gyro_on = !!data;
		break;
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		st->debug_determine_engine_on = !!data;
		break;
	case ATTR_GYRO_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.fsr = data;
		result = inv_set_gyro_sf(st);
		return result;
	case ATTR_ACCEL_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.accel_fs = data;
		result = inv_set_accel_sf(st);
		return result;
	case ATTR_DMP_PED_INT_ON:
		result  = inv_enable_pedometer_interrupt(st, !!data);
		if (result)
			return result;
		st->ped.int_on = !!data;
		return 0;
	case ATTR_DMP_PED_STEP_THRESH:
		st->ped.step_thresh = data;
		return 0;
	case ATTR_DMP_PED_INT_THRESH:
		st->ped.int_thresh = data;
		return 0;
	default:
		return -EINVAL;
	}
	st->trigger_state = MISC_TRIGGER;
	result = set_inv_enable(indio_dev);

	return result;
}

/*
 * inv_misc_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_misc_attr_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _misc_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

static int _debug_attr_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	if (!st->debug_determine_engine_on)
		return -EINVAL;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE:
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = !!data;
		break;
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		return -ENOSYS;
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		return -ENOSYS;
	case ATTR_DMP_GYRO_CAL_ENABLE:
		st->gyro_cal_enable = !!data;
		break;
	case ATTR_DMP_EVENT_INT_ON:
		st->chip_config.dmp_event_int_on = !!data;
		break;
	case ATTR_DMP_ON:
		st->chip_config.dmp_on = !!data;
		break;
	case ATTR_GYRO_ENABLE:
		st->chip_config.gyro_enable = !!data;
		break;
	case ATTR_ACCEL_ENABLE:
		st->chip_config.accel_enable = !!data;
		break;
	case ATTR_COMPASS_ENABLE:
		return -ENODEV;
	default:
		return -EINVAL;
	}
	st->trigger_state = DEBUG_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_debug_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_debug_attr_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _debug_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static ssize_t inv_sensor_rate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return snprintf(buf, MAX_WR_SZ, "%d\n",
					st->sensor_l[this_attr->address].rate);
}

static ssize_t inv_sensor_rate_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, rate, ind;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data <= 0) {
		pr_err("sensor_rate_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	rate = inv_rate_convert(st, ind, data);

	pr_debug("sensor [%s] requested  rate %d input [%d]\n",
						sensor_l_info[ind], rate, data);

	if (rate == st->sensor_l[ind].rate)
		return count;
	mutex_lock(&indio_dev->mlock);
	st->sensor_l[ind].rate = rate;
	st->trigger_state = DATA_TRIGGER;
	inv_check_sensor_on(st);
	result = set_inv_enable(indio_dev);
	pr_debug("%s rate %d div %d\n", sensor_l_info[ind],
				st->sensor_l[ind].rate, st->sensor_l[ind].div);
	mutex_unlock(&indio_dev->mlock);

	return count;
}

static ssize_t inv_sensor_on_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return snprintf(buf, MAX_WR_SZ, "%d\n", st->sensor_l[this_attr->address].on);
}

static ssize_t inv_sensor_on_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, on, ind;
	int result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data < 0) {
		pr_err("sensor_on_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	on = !!data;

	pr_debug("sensor [%s] requested  %s, input [%d]\n",
			sensor_l_info[ind], (on == 1) ? "On" : "Off", data);

	if (on == st->sensor_l[ind].on) {
		pr_debug("sensor [%s] is already %s, input [%d]\n",
			sensor_l_info[ind], (on == 1) ? "On" : "Off", data);
		return count;
	}

	mutex_lock(&indio_dev->mlock);
	st->sensor_l[ind].on = on;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_on(st);
	result = set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
	return result;

	pr_debug("Sensor [%s] is %s by sysfs\n",
				sensor_l_info[ind], (on == 1) ? "On" : "Off");
	return count;
}

static int inv_check_l_step(struct inv_mpu_state *st)
{
	if (st->step_counter_l_on || st->step_counter_wake_l_on)
		st->ped.on = true;
	else
		st->ped.on = false;

	return 0;
}

#ifdef CONFIG_INV_MPU_IIO_ICM20608D
static int inv_send_pedo_steps(struct inv_mpu_state *st)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int step;

	inv_switch_power_in_lp(st, true);
	sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_pedometer");
	inv_get_pedometer_steps(st, &step);
	inv_switch_power_in_lp(st, false);
	inv_send_steps(st, step, get_time_ns());

	return 0;
}
#endif

static int _basic_attr_store(struct device *dev,
			     struct device_attribute *attr, const char *buf,
			     size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	int result;
	u32 power_on_data;

	result = kstrtoint(buf, 10, &data);
	if (result || (data < 0))
		return -EINVAL;

	switch (this_attr->address) {
	case ATTR_DMP_PED_ON:
		if ((!!data) == st->ped.on)
			return count;
		st->ped.on = !!data;
		break;
	case ATTR_DMP_SMD_ENABLE:
		if ((!!data) == st->smd.on) {
			pr_debug("SMD is %s\n  Same",
						st->smd.on ? "On" : "Off");
			return count;
		}
		st->smd.on = !!data;
		pr_debug("SMD is %s\n", st->smd.on ? "On" : "Off");
		break;
	case ATTR_DMP_TILT_ENABLE:
		if ((!!data) == st->chip_config.tilt_enable)
			return count;
		st->chip_config.tilt_enable = !!data;
		pr_info("Tile %s\n",
			st->chip_config.tilt_enable ==
			1 ? "Enabled" : "Disabled");
		break;
	case ATTR_DMP_PICK_UP_ENABLE:
		if ((!!data) == st->chip_config.pick_up_enable) {
			pr_info("Pick_up enable already %s\n",
				st->chip_config.pick_up_enable ==
				1 ? "Enabled" : "Disabled");
			return count;
		}
		st->chip_config.pick_up_enable = !!data;
		pr_info("Pick up %s\n",
			st->chip_config.pick_up_enable ==
			1 ? "Enable" : "Disable");
		break;
	case ATTR_IN_POWER_ON:
		{
			u8 p0[2];
			u8 p1[2];
			struct spi_transfer power_on;
			struct spi_message msg;

			power_on_data = (u32)data;
			memset(&power_on, 0, sizeof(struct spi_transfer));

			power_on.bits_per_word = 8;
			power_on.len = 2;

			p0[0] = (power_on_data & 0xff);
			p0[1] = ((power_on_data >> 8) & 0xff);
			p1[0] = ((power_on_data >> 16) & 0xff);
			p1[1] = ((power_on_data >> 24) & 0xff);

			power_on.tx_buf = p0;
			power_on.rx_buf = p1;
			spi_message_init(&msg);
			spi_message_add_tail(&power_on, &msg);
			spi_sync(to_spi_device(st->dev), &msg);

			st->power_on_data = ((p0[0] << 24) | (p0[1] << 16) |
							(p1[0] << 8) | p1[1]);
			return count;
		}
	case ATTR_DMP_EIS_ENABLE:
		if ((!!data) == st->chip_config.eis_enable)
			return count;
		st->chip_config.eis_enable = !!data;
		pr_info("Eis %s\n",
			st->chip_config.eis_enable == 1 ? "Enable" : "Disable");
		break;
	case ATTR_DMP_STEP_DETECTOR_ON:
		st->step_detector_l_on = !!data;
		break;
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		st->step_detector_wake_l_on = !!data;
		break;
	case ATTR_DMP_ACTIVITY_ON:
		return -ENODEV;
	case ATTR_DMP_STEP_COUNTER_ON:
		st->step_counter_l_on = !!data;
#ifdef CONFIG_INV_MPU_IIO_ICM20608D
		if (st->step_counter_l_on)
			inv_send_pedo_steps(st);
#endif
		break;
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		st->step_counter_wake_l_on = !!data;
#ifdef CONFIG_INV_MPU_IIO_ICM20608D
		if (st->step_counter_wake_l_on)
			inv_send_pedo_steps(st);
#endif
		break;
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		if (data == st->batch.timeout)
			return count;
		st->batch.timeout = data;
		break;
	default:
		return -EINVAL;
	};
	inv_check_l_step(st);
	inv_check_sensor_on(st);

	st->trigger_state = EVENT_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_basic_attr_store() -  calling this function will store current
 *                        non-dmp parameter settings
 */
static ssize_t inv_basic_attr_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _basic_attr_store(dev, attr, buf, count);

	mutex_unlock(&indio_dev->mlock);

	return result;
}

/*
 * inv_attr_bias_show() -  calling this function will show current
 *                        dmp gyro/accel bias.
 */
static int _attr_bias_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int axes, addr, result, dmp_bias;
	int sensor_type;

	switch (this_attr->address) {
	case ATTR_DMP_ACCEL_X_DMP_BIAS:
		return -ENOSYS;
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:
		return -ENOSYS;
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:
		return -ENOSYS;
	case ATTR_DMP_GYRO_X_DMP_BIAS:
		axes = 0;
		addr = D_EXT_GYRO_BIAS_X;
		sensor_type = SENSOR_GYRO;
		break;
	case ATTR_DMP_GYRO_Y_DMP_BIAS:
		axes = 1;
		addr = D_EXT_GYRO_BIAS_Y;
		sensor_type = SENSOR_GYRO;
		break;
	case ATTR_DMP_GYRO_Z_DMP_BIAS:
		axes = 2;
		addr = D_EXT_GYRO_BIAS_Z;
		sensor_type = SENSOR_GYRO;
		break;
	case ATTR_DMP_MAGN_X_DMP_BIAS:
		return -ENODEV;
	case ATTR_DMP_MAGN_Y_DMP_BIAS:
		return -ENODEV;
	case ATTR_DMP_MAGN_Z_DMP_BIAS:
		return -ENODEV;
	case ATTR_DMP_SC_AUTH:
	case ATTR_DMP_EIS_AUTH:
		axes = 0;
		addr = st->aut_key_out;
		sensor_type = -1;
		break;
	default:
		return -EINVAL;
	}
	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = read_be32_from_mem(st, &dmp_bias, addr);
	if (result)
		return result;
	inv_switch_power_in_lp(st, false);
	if (SENSOR_GYRO == sensor_type)
		st->input_gyro_dmp_bias[axes] = dmp_bias;
	else if (SENSOR_ACCEL == sensor_type)
		st->input_accel_dmp_bias[axes] = dmp_bias;
	else if (sensor_type != -1)
		return -EINVAL;

	return snprintf(buf, MAX_WR_SZ, "%d\n", dmp_bias);
}

static ssize_t inv_attr_bias_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _attr_bias_show(dev, attr, buf);

	mutex_unlock(&indio_dev->mlock);

	return result;
}

/*
 * inv_attr_show() -  calling this function will show current
 *                        dmp parameters.
 */
static ssize_t inv_attr_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	s8 *m;

	switch (this_attr->address) {
	case ATTR_GYRO_SCALE:
		{
			const s16 gyro_scale[] = { 250, 500, 1000, 2000 };

			return snprintf(buf, MAX_WR_SZ, "%d\n",
				       gyro_scale[st->chip_config.fsr]);
		}
	case ATTR_ACCEL_SCALE:
		{
			const s16 accel_scale[] = { 2, 4, 8, 16 };
			return snprintf(buf, MAX_WR_SZ, "%d\n",
				       accel_scale[st->chip_config.accel_fs]);
		}
	case ATTR_COMPASS_SCALE:
		return -ENODEV;
	case ATTR_COMPASS_SENSITIVITY_X:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_info.compass_sens[0]);
	case ATTR_COMPASS_SENSITIVITY_Y:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_info.compass_sens[1]);
	case ATTR_COMPASS_SENSITIVITY_Z:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_info.compass_sens[2]);
	case ATTR_GYRO_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->chip_config.gyro_enable);
	case ATTR_ACCEL_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->chip_config.accel_enable);
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		return -ENOSYS;
	case ATTR_IN_POWER_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->power_on_data);
	case ATTR_DMP_GYRO_CAL_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_cal_enable);
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->debug_determine_engine_on);
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD:
		return -ENOSYS;
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE:
		return -ENOSYS;
	case ATTR_FIRMWARE_LOADED:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
					st->chip_config.firmware_loaded);
	case ATTR_DMP_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->chip_config.dmp_on);
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->batch.timeout);
	case ATTR_DMP_EVENT_INT_ON:
		return snprintf(buf, MAX_WR_SZ,
				"%d\n", st->chip_config.dmp_event_int_on);
	case ATTR_DMP_PED_INT_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.int_on);
	case ATTR_DMP_PED_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.on);
	case ATTR_DMP_PED_STEP_THRESH:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.step_thresh);
	case ATTR_DMP_PED_INT_THRESH:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->ped.int_thresh);
	case ATTR_DMP_SMD_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->smd.on);
	case ATTR_DMP_TILT_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_config.tilt_enable);
	case ATTR_DMP_PICK_UP_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_config.pick_up_enable);
	case ATTR_DMP_EIS_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->chip_config.eis_enable);
	case ATTR_DMP_LOW_POWER_GYRO_ON:
		return -ENOSYS;
	case ATTR_DMP_LP_EN_OFF:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
						st->chip_config.lp_en_mode_off);
	case ATTR_COMPASS_ENABLE:
		return -ENODEV;
	case ATTR_DMP_STEP_COUNTER_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_counter_l_on);
	case ATTR_DMP_STEP_COUNTER_WAKE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_counter_wake_l_on);
	case ATTR_DMP_STEP_DETECTOR_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_detector_l_on);
	case ATTR_DMP_STEP_DETECTOR_WAKE_ON:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->step_detector_wake_l_on);
	case ATTR_DMP_ACTIVITY_ON:
		return -ENODEV;
	case ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE:
		return snprintf(buf, MAX_WR_SZ, "%d\n",
			       st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on);
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		return -ENOSYS;
	case ATTR_GYRO_MATRIX:
		m = st->plat_data.orientation;
		return snprintf(buf, MAX_WR_SZ, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			       m[8]);
	case ATTR_ACCEL_MATRIX:
		m = st->plat_data.orientation;
		return snprintf(buf, MAX_WR_SZ, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			       m[8]);
	case ATTR_COMPASS_MATRIX:
		return -ENODEV;
	case ATTR_SECONDARY_NAME:
		return -ENODEV;
	case ATTR_DMP_DEBUG_MEM_READ:
		{
			int out;

			inv_switch_power_in_lp(st, true);
			result =
			    read_be32_from_mem(st, &out, debug_mem_read_addr);
			if (result)
				return result;
			inv_switch_power_in_lp(st, false);
			return snprintf(buf, MAX_WR_SZ, "0x%x\n", out);
		}
	case ATTR_DMP_MAGN_ACCURACY:
		return -ENODEV;
	case ATTR_GYRO_SF:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_sf);
	case ATTR_ANGLVEL_X_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_st_bias[0]);
	case ATTR_ANGLVEL_Y_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_st_bias[1]);
	case ATTR_ANGLVEL_Z_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->gyro_st_bias[2]);
	case ATTR_ACCEL_X_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_st_bias[0]);
	case ATTR_ACCEL_Y_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_st_bias[1]);
	case ATTR_ACCEL_Z_ST_CALIBBIAS:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->accel_st_bias[2]);
	case ATTR_GYRO_X_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_gyro_bias[0]);
	case ATTR_GYRO_Y_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_gyro_bias[1]);
	case ATTR_GYRO_Z_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_gyro_bias[2]);
	case ATTR_ACCEL_X_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_accel_bias[0]);
	case ATTR_ACCEL_Y_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_accel_bias[1]);
	case ATTR_ACCEL_Z_OFFSET:
		return snprintf(buf, MAX_WR_SZ, "%d\n", st->input_accel_bias[2]);
	default:
		return -EPERM;
	}
}

static ssize_t inv_attr64_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u64 tmp;
	u32 ped;

	mutex_lock(&indio_dev->mlock);
	result = 0;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		inv_switch_power_in_lp(st, true);
		result = inv_get_pedometer_steps(st, &ped);
		result |= inv_read_pedometer_counter(st);
		tmp = (u64) st->ped.step + (u64) ped;
		inv_switch_power_in_lp(st, false);
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		inv_switch_power_in_lp(st, true);
		result = inv_get_pedometer_time(st, &ped);
		tmp = (u64) st->ped.time + ((u64) ped) * MS_PER_PED_TICKS;
		inv_switch_power_in_lp(st, false);
		break;
	case ATTR_DMP_PEDOMETER_COUNTER:
		tmp = st->ped.last_step_time;
		break;
	default:
		tmp = 0;
		result = -EINVAL;
		break;
	}

	mutex_unlock(&indio_dev->mlock);
	if (result)
		return -EINVAL;
	return snprintf(buf, MAX_WR_SZ, "%lld\n", tmp);
}

static ssize_t inv_attr64_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u8 d[4] = { 0, 0, 0, 0 };
	u64 data;

	mutex_lock(&indio_dev->mlock);
	if (!st->chip_config.firmware_loaded) {
		mutex_unlock(&indio_dev->mlock);
		return -EINVAL;
	}
	result = inv_switch_power_in_lp(st, true);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}
	result = kstrtoull(buf, 10, &data);
	if (result)
		goto attr64_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		result = mem_w(PEDSTD_STEPCTR, ARRAY_SIZE(d), d);
		if (result)
			goto attr64_store_fail;
		st->ped.step = data;
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		result = mem_w(PEDSTD_TIMECTR, ARRAY_SIZE(d), d);
		if (result)
			goto attr64_store_fail;
		st->ped.time = data;
		break;
	default:
		result = -EINVAL;
		break;
	}
attr64_store_fail:
	mutex_unlock(&indio_dev->mlock);
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}

static ssize_t inv_self_test(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int res;

	mutex_lock(&indio_dev->mlock);
	res = inv_hw_self_test(st);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return snprintf(buf, MAX_WR_SZ, "%d\n", res);
}

/*
 *  inv_temperature_show() - Read temperature data directly from registers.
 */
static ssize_t inv_temperature_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

/*
 * inv_smd_show() -  calling this function showes smd interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_smd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_WR_SZ, "1\n");
}

/*
 * inv_ped_show() -  calling this function showes pedometer interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_ped_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_WR_SZ, "1\n");
}

static ssize_t inv_activity_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

static ssize_t inv_tilt_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

static ssize_t inv_pick_up_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return -ENODEV;
}

/*
 *  inv_reg_dump_show() - Register dump for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int ii;
	char data;
	int bytes_printed = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	mutex_lock(&indio_dev->mlock);
	bytes_printed += snprintf(buf + bytes_printed, MAX_WR_SZ, "bank 0\n");

	for (ii = 0; ii < 0x7F; ii++) {
		/* don't read fifo r/w register */
		if ((ii == REG_MEM_R_W) || (ii == REG_FIFO_R_W))
			data = 0;
		else
			inv_plat_read(st, ii, 1, &data);
		bytes_printed += snprintf(buf + bytes_printed, MAX_WR_SZ,
				"%#2x: %#2x\n", ii, data);
	}
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return bytes_printed;
}

static ssize_t inv_flush_batch_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;

	mutex_lock(&indio_dev->mlock);
	result = inv_flush_batch_data(indio_dev, data);
	mutex_unlock(&indio_dev->mlock);

	return count;
}

static const struct iio_chan_spec inv_mpu_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_MPU_SCAN_TIMESTAMP),
};

static DEVICE_ATTR(poll_smd, S_IRUGO, inv_smd_show, NULL);
static DEVICE_ATTR(poll_pedometer, S_IRUGO, inv_ped_show, NULL);
static DEVICE_ATTR(poll_activity, S_IRUGO, inv_activity_show, NULL);
static DEVICE_ATTR(poll_tilt, S_IRUGO, inv_tilt_show, NULL);
static DEVICE_ATTR(poll_pick_up, S_IRUGO, inv_pick_up_show, NULL);

/* special run time sysfs entry, read only */
static DEVICE_ATTR(debug_reg_dump, S_IRUGO | S_IWUGO, inv_reg_dump_show, NULL);
static DEVICE_ATTR(out_temperature, S_IRUGO | S_IWUGO,
		   inv_temperature_show, NULL);
static DEVICE_ATTR(misc_self_test, S_IRUGO | S_IWUGO, inv_self_test, NULL);

static IIO_DEVICE_ATTR(info_anglvel_matrix, S_IRUGO, inv_attr_show, NULL,
		       ATTR_GYRO_MATRIX);
static IIO_DEVICE_ATTR(info_accel_matrix, S_IRUGO, inv_attr_show, NULL,
		       ATTR_ACCEL_MATRIX);

static IIO_DEVICE_ATTR(info_gyro_sf, S_IRUGO, inv_attr_show, NULL,
		       ATTR_GYRO_SF);
/* write only sysfs */
static DEVICE_ATTR(misc_flush_batch, S_IWUGO, NULL, inv_flush_batch_store);

/* sensor on/off sysfs control */
static IIO_DEVICE_ATTR(in_accel_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store, SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store, SENSOR_L_GYRO);
static IIO_DEVICE_ATTR(in_calib_anglvel_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_GYRO_CAL);
static IIO_DEVICE_ATTR(in_eis_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_EIS_GYRO);
static IIO_DEVICE_ATTR(in_accel_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_ACCEL_WAKE);
static IIO_DEVICE_ATTR(in_anglvel_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_GYRO_WAKE);
static IIO_DEVICE_ATTR(in_calib_anglvel_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_GYRO_CAL_WAKE);
static IIO_DEVICE_ATTR(in_6quat_enable, S_IRUGO | S_IWUGO, inv_sensor_on_show,
		       inv_sensor_on_store, SENSOR_L_SIXQ);
static IIO_DEVICE_ATTR(in_3quat_enable, S_IRUGO | S_IWUGO, inv_sensor_on_show,
                       inv_sensor_on_store, SENSOR_L_THREEQ);
static IIO_DEVICE_ATTR(in_6quat_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_SIXQ_WAKE);
static IIO_DEVICE_ATTR(in_p6quat_enable, S_IRUGO | S_IWUGO, inv_sensor_on_show,
		       inv_sensor_on_store, SENSOR_L_PEDQ);
static IIO_DEVICE_ATTR(in_p6quat_wake_enable, S_IRUGO | S_IWUGO,
		       inv_sensor_on_show, inv_sensor_on_store,
		       SENSOR_L_PEDQ_WAKE);

/* sensor rate sysfs control */
static IIO_DEVICE_ATTR(in_accel_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_rate, S_IRUGO | S_IWUGO, inv_sensor_rate_show,
		       inv_sensor_rate_store, SENSOR_L_GYRO);
static IIO_DEVICE_ATTR(in_calib_anglvel_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_GYRO_CAL);
static IIO_DEVICE_ATTR(in_eis_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_EIS_GYRO);
static IIO_DEVICE_ATTR(in_accel_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_ACCEL_WAKE);
static IIO_DEVICE_ATTR(in_anglvel_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_GYRO_WAKE);
static IIO_DEVICE_ATTR(in_calib_anglvel_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_GYRO_CAL_WAKE);
static IIO_DEVICE_ATTR(in_6quat_rate, S_IRUGO | S_IWUGO, inv_sensor_rate_show,
		       inv_sensor_rate_store, SENSOR_L_SIXQ);
static IIO_DEVICE_ATTR(in_3quat_rate, S_IRUGO | S_IWUGO, inv_sensor_rate_show,
                       inv_sensor_rate_store, SENSOR_L_THREEQ);
static IIO_DEVICE_ATTR(in_6quat_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_SIXQ_WAKE);
static IIO_DEVICE_ATTR(in_p6quat_rate, S_IRUGO | S_IWUGO, inv_sensor_rate_show,
		       inv_sensor_rate_store, SENSOR_L_PEDQ);
static IIO_DEVICE_ATTR(in_p6quat_wake_rate, S_IRUGO | S_IWUGO,
		       inv_sensor_rate_show, inv_sensor_rate_store,
		       SENSOR_L_PEDQ_WAKE);

/* debug determine engine related sysfs */
static IIO_DEVICE_ATTR(debug_anglvel_accuracy_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store,
		       ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_accuracy_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store,
		       ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_gyro_cal_enable, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_GYRO_CAL_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_cal_enable, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_ACCEL_CAL_ENABLE);

static IIO_DEVICE_ATTR(debug_gyro_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store, ATTR_GYRO_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store, ATTR_ACCEL_ENABLE);
static IIO_DEVICE_ATTR(debug_compass_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_debug_attr_store,
		       ATTR_COMPASS_ENABLE);
static IIO_DEVICE_ATTR(debug_dmp_on, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_ON);
static IIO_DEVICE_ATTR(debug_dmp_event_int_on, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_attr_store, ATTR_DMP_EVENT_INT_ON);
static IIO_DEVICE_ATTR(debug_mem_read, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_dmp_bias_store, ATTR_DMP_DEBUG_MEM_READ);
static IIO_DEVICE_ATTR(debug_mem_write, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_dmp_bias_store, ATTR_DMP_DEBUG_MEM_WRITE);

static IIO_DEVICE_ATTR(misc_batchmode_timeout, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_BATCHMODE_TIMEOUT);

static IIO_DEVICE_ATTR(info_firmware_loaded, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_firmware_loaded_store, ATTR_FIRMWARE_LOADED);

/* engine scale */
static IIO_DEVICE_ATTR(in_accel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_misc_attr_store, ATTR_ACCEL_SCALE);
static IIO_DEVICE_ATTR(in_anglvel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_misc_attr_store, ATTR_GYRO_SCALE);

static IIO_DEVICE_ATTR(debug_low_power_gyro_on, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_misc_attr_store,
		       ATTR_DMP_LOW_POWER_GYRO_ON);
static IIO_DEVICE_ATTR(debug_lp_en_off, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DMP_LP_EN_OFF);
static IIO_DEVICE_ATTR(debug_clock_sel, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DMP_CLK_SEL);
static IIO_DEVICE_ATTR(debug_reg_write, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DEBUG_REG_WRITE);
static IIO_DEVICE_ATTR(debug_cfg_write, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DEBUG_WRITE_CFG);
static IIO_DEVICE_ATTR(debug_reg_write_addr, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_debug_store, ATTR_DEBUG_REG_ADDR);

static IIO_DEVICE_ATTR(in_accel_x_dmp_bias, S_IRUGO | S_IWUGO,
		       inv_attr_bias_show, inv_dmp_bias_store,
		       ATTR_DMP_ACCEL_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_y_dmp_bias, S_IRUGO | S_IWUGO,
		       inv_attr_bias_show, inv_dmp_bias_store,
		       ATTR_DMP_ACCEL_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_z_dmp_bias, S_IRUGO | S_IWUGO,
		       inv_attr_bias_show, inv_dmp_bias_store,
		       ATTR_DMP_ACCEL_Z_DMP_BIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_dmp_bias, S_IRUGO | S_IWUGO,
		       inv_attr_bias_show, inv_dmp_bias_store,
		       ATTR_DMP_GYRO_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_dmp_bias, S_IRUGO | S_IWUGO,
		       inv_attr_bias_show, inv_dmp_bias_store,
		       ATTR_DMP_GYRO_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_dmp_bias, S_IRUGO | S_IWUGO,
		       inv_attr_bias_show, inv_dmp_bias_store,
		       ATTR_DMP_GYRO_Z_DMP_BIAS);

static IIO_DEVICE_ATTR(in_accel_x_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_X_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_Y_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ACCEL_Z_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_X_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_Y_ST_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_st_calibbias, S_IRUGO | S_IWUGO,
		       inv_attr_show, NULL, ATTR_ANGLVEL_Z_ST_CALIBBIAS);

static IIO_DEVICE_ATTR(in_accel_x_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_ACCEL_X_OFFSET);
static IIO_DEVICE_ATTR(in_accel_y_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_ACCEL_Y_OFFSET);
static IIO_DEVICE_ATTR(in_accel_z_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_ACCEL_Z_OFFSET);

static IIO_DEVICE_ATTR(in_anglvel_x_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_GYRO_X_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_y_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_GYRO_Y_OFFSET);
static IIO_DEVICE_ATTR(in_anglvel_z_offset, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_bias_store, ATTR_GYRO_Z_OFFSET);

static IIO_DEVICE_ATTR(in_sc_auth, S_IRUGO | S_IWUGO,
		inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_SC_AUTH);

static IIO_DEVICE_ATTR(in_eis_auth, S_IRUGO | S_IWUGO,
		inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_EIS_AUTH);

static IIO_DEVICE_ATTR(debug_determine_engine_on, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_misc_attr_store,
		       ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON);
static IIO_DEVICE_ATTR(misc_gyro_recalibration, S_IRUGO | S_IWUGO, NULL,
		       inv_dmp_bias_store, ATTR_DMP_MISC_GYRO_RECALIBRATION);
static IIO_DEVICE_ATTR(misc_accel_recalibration, S_IRUGO | S_IWUGO, NULL,
		       inv_dmp_bias_store, ATTR_DMP_MISC_ACCEL_RECALIBRATION);
static IIO_DEVICE_ATTR(params_accel_calibration_threshold, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_dmp_bias_store,
		       ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD);
static IIO_DEVICE_ATTR(params_accel_calibration_rate, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_dmp_bias_store,
		       ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE);

static IIO_DEVICE_ATTR(in_step_detector_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_STEP_DETECTOR_ON);
static IIO_DEVICE_ATTR(in_step_detector_wake_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_STEP_DETECTOR_WAKE_ON);
static IIO_DEVICE_ATTR(in_step_counter_enable, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_basic_attr_store, ATTR_DMP_STEP_COUNTER_ON);
static IIO_DEVICE_ATTR(in_step_counter_wake_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_STEP_COUNTER_WAKE_ON);
static IIO_DEVICE_ATTR(in_activity_enable, S_IRUGO | S_IWUGO, inv_attr_show,
		       inv_basic_attr_store, ATTR_DMP_ACTIVITY_ON);

static IIO_DEVICE_ATTR(event_smd_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_SMD_ENABLE);

static IIO_DEVICE_ATTR(event_tilt_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_TILT_ENABLE);

static IIO_DEVICE_ATTR(event_eis_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_EIS_ENABLE);

static IIO_DEVICE_ATTR(event_pick_up_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store,
		       ATTR_DMP_PICK_UP_ENABLE);

static IIO_DEVICE_ATTR(in_power_on, S_IRUGO | S_IWUGO,
			inv_attr_show, inv_basic_attr_store,
			ATTR_IN_POWER_ON);

static IIO_DEVICE_ATTR(params_pedometer_int_on, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_INT_ON);
static IIO_DEVICE_ATTR(event_pedometer_enable, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_basic_attr_store, ATTR_DMP_PED_ON);
static IIO_DEVICE_ATTR(params_pedometer_step_thresh, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_misc_attr_store,
		       ATTR_DMP_PED_STEP_THRESH);
static IIO_DEVICE_ATTR(params_pedometer_int_thresh, S_IRUGO | S_IWUGO,
		       inv_attr_show, inv_misc_attr_store,
		       ATTR_DMP_PED_INT_THRESH);

static IIO_DEVICE_ATTR(out_pedometer_steps, S_IRUGO | S_IWUGO, inv_attr64_show,
		       inv_attr64_store, ATTR_DMP_PEDOMETER_STEPS);
static IIO_DEVICE_ATTR(out_pedometer_time, S_IRUGO | S_IWUGO, inv_attr64_show,
		       inv_attr64_store, ATTR_DMP_PEDOMETER_TIME);
static IIO_DEVICE_ATTR(out_pedometer_counter, S_IRUGO | S_IWUGO,
		       inv_attr64_show, NULL, ATTR_DMP_PEDOMETER_COUNTER);

static const struct attribute *inv_raw_attributes[] = {
	&dev_attr_debug_reg_dump.attr,
	&dev_attr_out_temperature.attr,
	&dev_attr_misc_flush_batch.attr,
	&dev_attr_misc_self_test.attr,
	&iio_dev_attr_in_sc_auth.dev_attr.attr,
	&iio_dev_attr_in_power_on.dev_attr.attr,
	&iio_dev_attr_in_eis_auth.dev_attr.attr,
	&iio_dev_attr_in_accel_enable.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_enable.dev_attr.attr,
	&iio_dev_attr_info_accel_matrix.dev_attr.attr,
	&iio_dev_attr_in_accel_scale.dev_attr.attr,
	&iio_dev_attr_misc_batchmode_timeout.dev_attr.attr,
	&iio_dev_attr_in_accel_rate.dev_attr.attr,
	&iio_dev_attr_in_accel_wake_rate.dev_attr.attr,
	&iio_dev_attr_debug_mem_read.dev_attr.attr,
	&iio_dev_attr_debug_mem_write.dev_attr.attr,
};

static const struct attribute *inv_debug_attributes[] = {
	&iio_dev_attr_debug_accel_enable.dev_attr.attr,
	&iio_dev_attr_debug_dmp_event_int_on.dev_attr.attr,
	&iio_dev_attr_debug_low_power_gyro_on.dev_attr.attr,
	&iio_dev_attr_debug_lp_en_off.dev_attr.attr,
	&iio_dev_attr_debug_clock_sel.dev_attr.attr,
	&iio_dev_attr_debug_reg_write.dev_attr.attr,
	&iio_dev_attr_debug_reg_write_addr.dev_attr.attr,
	&iio_dev_attr_debug_cfg_write.dev_attr.attr,
	&iio_dev_attr_debug_dmp_on.dev_attr.attr,
	&iio_dev_attr_debug_accel_cal_enable.dev_attr.attr,
	&iio_dev_attr_debug_accel_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_debug_determine_engine_on.dev_attr.attr,
	&iio_dev_attr_debug_gyro_enable.dev_attr.attr,
	&iio_dev_attr_debug_gyro_cal_enable.dev_attr.attr,
	&iio_dev_attr_debug_anglvel_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_debug_compass_enable.dev_attr.attr,
	&iio_dev_attr_params_pedometer_step_thresh.dev_attr.attr,
	&iio_dev_attr_params_pedometer_int_thresh.dev_attr.attr,
	&iio_dev_attr_misc_gyro_recalibration.dev_attr.attr,
	&iio_dev_attr_misc_accel_recalibration.dev_attr.attr,
	&iio_dev_attr_params_accel_calibration_threshold.dev_attr.attr,
	&iio_dev_attr_params_accel_calibration_rate.dev_attr.attr,
};

static const struct attribute *inv_gyro_attributes[] = {
	&iio_dev_attr_info_anglvel_matrix.dev_attr.attr,
	&iio_dev_attr_in_anglvel_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_rate.dev_attr.attr,
	&iio_dev_attr_in_eis_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale.dev_attr.attr,
	&iio_dev_attr_in_eis_rate.dev_attr.attr,
	&iio_dev_attr_in_anglvel_wake_rate.dev_attr.attr,
	&iio_dev_attr_info_gyro_sf.dev_attr.attr,
};

static const struct attribute *inv_dmp_attributes[] = {
	&iio_dev_attr_in_calib_anglvel_enable.dev_attr.attr,
	&iio_dev_attr_in_calib_anglvel_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_calib_anglvel_rate.dev_attr.attr,
	&iio_dev_attr_in_calib_anglvel_wake_rate.dev_attr.attr,
	&iio_dev_attr_in_accel_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_info_firmware_loaded.dev_attr.attr,
	&iio_dev_attr_in_activity_enable.dev_attr.attr,
	&iio_dev_attr_in_6quat_enable.dev_attr.attr,
	&iio_dev_attr_in_6quat_rate.dev_attr.attr,
	&iio_dev_attr_in_3quat_enable.dev_attr.attr,
	&iio_dev_attr_in_3quat_rate.dev_attr.attr,
	&iio_dev_attr_in_6quat_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_6quat_wake_rate.dev_attr.attr,
	&iio_dev_attr_in_p6quat_enable.dev_attr.attr,
	&iio_dev_attr_in_p6quat_rate.dev_attr.attr,
	&iio_dev_attr_in_p6quat_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_p6quat_wake_rate.dev_attr.attr,
	&dev_attr_poll_activity.attr,
};

static const struct attribute *inv_bias_attributes[] = {
	&iio_dev_attr_in_accel_x_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_accel_z_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_st_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_offset.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_offset.dev_attr.attr,
};

static const struct attribute *inv_pedometer_attributes[] = {
	&dev_attr_poll_pedometer.attr,
	&dev_attr_poll_tilt.attr,
	&dev_attr_poll_pick_up.attr,
	&iio_dev_attr_params_pedometer_int_on.dev_attr.attr,
	&iio_dev_attr_event_pedometer_enable.dev_attr.attr,
	&iio_dev_attr_event_tilt_enable.dev_attr.attr,
	&iio_dev_attr_event_eis_enable.dev_attr.attr,
	&iio_dev_attr_event_pick_up_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_enable.dev_attr.attr,
	&iio_dev_attr_in_step_counter_wake_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_wake_enable.dev_attr.attr,
	&iio_dev_attr_out_pedometer_steps.dev_attr.attr,
	&iio_dev_attr_out_pedometer_time.dev_attr.attr,
	&iio_dev_attr_out_pedometer_counter.dev_attr.attr,
};

static const struct attribute *inv_smd_attributes[] = {
	&dev_attr_poll_smd.attr,
	&iio_dev_attr_event_smd_enable.dev_attr.attr,
};

static struct attribute *inv_attributes[ARRAY_SIZE(inv_raw_attributes) +
					ARRAY_SIZE(inv_debug_attributes) +
					ARRAY_SIZE(inv_gyro_attributes) +
					ARRAY_SIZE(inv_dmp_attributes) +
					ARRAY_SIZE(inv_bias_attributes) +
					ARRAY_SIZE(inv_pedometer_attributes) +
					ARRAY_SIZE(inv_smd_attributes) +
					 + 1];

static const struct attribute_group inv_attribute_group = {
	.name = "mpu",
	.attrs = inv_attributes
};

static const struct iio_info mpu_info = {
	.driver_module = THIS_MODULE,
	.attrs = &inv_attribute_group,
};

/*
 *  inv_check_chip_type() - check and setup chip type.
 */
int inv_check_chip_type(struct iio_dev *indio_dev, const char *name)
{
	int result;
	int t_ind;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;
	struct inv_mpu_state *st;

	st = iio_priv(indio_dev);
	conf = &st->chip_config;
	plat = &st->plat_data;

	if (!strcmp(name, "icm20690"))
		st->chip_type = ICM20690;
	else if (!strcmp(name, "icm20602"))
		st->chip_type = ICM20602;
	else if (!strcmp(name, "icm20608d"))
		st->chip_type = ICM20608D;
	else
		return -EPERM;
#ifdef CONFIG_INV_MPU_IIO_ICM20608D
	if (st->chip_type == ICM20608D) {
		st->dmp_image_size = DMP_IMAGE_SIZE_20608D;
		st->dmp_start_address = DMP_START_ADDR_20608D;
		st->aut_key_in = SC_AUT_INPUT_20608D;
		st->aut_key_out = SC_AUT_OUTPUT_20608D;
	}
#endif
	st->chip_config.has_gyro = 1;

	st->hw = &hw_info[st->chip_type];
	result = inv_mpu_initialize(st);
	if (result)
		return result;

	t_ind = 0;
	memcpy(&inv_attributes[t_ind], inv_raw_attributes,
					sizeof(inv_raw_attributes));
	t_ind += ARRAY_SIZE(inv_raw_attributes);

	memcpy(&inv_attributes[t_ind], inv_pedometer_attributes,
				sizeof(inv_pedometer_attributes));
	t_ind += ARRAY_SIZE(inv_pedometer_attributes);

	memcpy(&inv_attributes[t_ind], inv_gyro_attributes,
				sizeof(inv_gyro_attributes));
	t_ind += ARRAY_SIZE(inv_gyro_attributes);

	memcpy(&inv_attributes[t_ind], inv_bias_attributes,
				sizeof(inv_bias_attributes));
	t_ind += ARRAY_SIZE(inv_bias_attributes);

	memcpy(&inv_attributes[t_ind], inv_debug_attributes,
					sizeof(inv_debug_attributes));
	t_ind += ARRAY_SIZE(inv_debug_attributes);

	if (st->chip_type != ICM20602) {
		memcpy(&inv_attributes[t_ind], inv_smd_attributes,
					sizeof(inv_smd_attributes));
		t_ind += ARRAY_SIZE(inv_smd_attributes);

		memcpy(&inv_attributes[t_ind], inv_dmp_attributes,
					sizeof(inv_dmp_attributes));
		t_ind += ARRAY_SIZE(inv_dmp_attributes);
	}

	inv_attributes[t_ind] = NULL;

	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu_channels);

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;

	return result;
}

/*
 * inv_dmp_firmware_write() -  calling this function will load the firmware.
 */
static ssize_t inv_dmp_firmware_write(struct file *fp, struct kobject *kobj,
				  struct bin_attribute *attr, char *buf,
				  loff_t pos, size_t size)
{
	int result, offset;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;
	u32 crc_value;
/* /system/vendor/firmware/inv_dmpfirmware.bin */
/* from vanadium-host-hal/sources/vendor/invensense/hammerhead/proprietary */
#ifndef INV_IPL
#define FIRMWARE_CRC_20608D           0xa4ca781d
#else
#define FIRMWARE_CRC_20608D           0x954601ed
#endif

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->firmware) {
		st->firmware = kmalloc(st->dmp_image_size, GFP_KERNEL);
		if (!st->firmware) {
			pr_err("no memory while loading firmware\n");
			return -ENOMEM;
		}
	}
	offset = pos;
	memcpy(st->firmware + pos, buf, size);
	if ((!size) && (st->dmp_image_size != pos)) {
		pr_err("wrong size for DMP firmware 0x%08x vs 0x%08x\n",
		       offset, st->dmp_image_size);
		kfree(st->firmware);
		st->firmware = 0;
		return -EINVAL;
	}
	if (st->dmp_image_size == (pos + size)) {
		result = ~crc32(~0, st->firmware, st->dmp_image_size);
		crc_value = FIRMWARE_CRC_20608D;
		if (crc_value != result) {
			pr_info("%s: firmware CRC error -0x%08x vs 0x%08x\n",
				__func__, result, crc_value);
			return -EINVAL;
		} else {
			pr_info("%s: CRC matched  -0x%08x vs 0x%08x\n",
				__func__, result, crc_value);
		}

		pr_info("%s: firmware CRC is 0x%08x\n", __func__, result);
		mutex_lock(&indio_dev->mlock);
		result = inv_firmware_load(st);
		kfree(st->firmware);
		st->firmware = 0;
		mutex_unlock(&indio_dev->mlock);
		if (result) {
			pr_err("firmware load failed\n");
			return result;
		}
	}

	return size;
}

static ssize_t inv_dmp_firmware_read(struct file *filp,
				 struct kobject *kobj,
				 struct bin_attribute *bin_attr,
				 char *buf, loff_t off, size_t count)
{
	int result, offset;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	offset = off;
	result = inv_dmp_read(st, offset, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

int inv_accel_covariance_write(struct file *fp, struct kobject *kobj,
			       struct bin_attribute *attr, char *buf,
			       loff_t pos, size_t size)
{
	int i;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (size != ACCEL_COVARIANCE_SIZE)
		return -EINVAL;

	for (i = 0; i < COVARIANCE_SIZE; i++)
		memcpy((u8 *) &st->accel_covariance[i],
		       &buf[i * sizeof(int)], sizeof(int));

	return size;
}

static int inv_dmp_covar_read(struct inv_mpu_state *st,
			      int off, int size, u8 *buf)
{
	int data, result, i;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	inv_stop_dmp(st);
	for (i = 0; i < COVARIANCE_SIZE * sizeof(int); i += sizeof(int)) {
		result = read_be32_from_mem(st, (u32 *) &data, off + i);
		if (result)
			return result;
		memcpy(buf + i, (u8 *) &data, sizeof(int));
	}

	return 0;
}

int inv_accel_covariance_read(struct file *filp,
			      struct kobject *kobj,
			      struct bin_attribute *bin_attr,
			      char *buf, loff_t off, size_t count)
{
	int result;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	result = inv_dmp_covar_read(st, ACCEL_COVARIANCE, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

/*
 *  inv_create_dmp_sysfs() - create binary sysfs dmp entry.
 */
static struct bin_attribute dmp_firmware = {
	.attr = {
		 .name = "misc_bin_dmp_firmware",
		 .mode = S_IRUGO | S_IWUGO},
	.read = inv_dmp_firmware_read,
	.write = inv_dmp_firmware_write,
};

int inv_create_dmp_sysfs(struct iio_dev *ind)
{
	int result = 0;
	struct inv_mpu_state *st = iio_priv(ind);

	if (st->chip_type == ICM20608D) {
		dmp_firmware.size = st->dmp_image_size;
		result = sysfs_create_bin_file(&ind->dev.kobj, &dmp_firmware);
	}

	return result;
}
