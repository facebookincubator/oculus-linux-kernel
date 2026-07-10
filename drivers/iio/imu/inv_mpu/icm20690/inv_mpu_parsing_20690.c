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
#include <linux/math64.h>

#include "inv_mpu_iio.h"

static char iden[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

static int inv_process_gyro(struct inv_mpu_state *st, u8 *d, u64 t)
{
	s16 raw[3];
	s32 calib[3];
	int i;
#define BIAS_UNIT 2859

	for (i = 0; i < 3; i++)
		raw[i] = be16_to_cpup((__be16 *) (d + i * 2));

	for (i = 0; i < 3; i++)
		calib[i] = (raw[i] << 15);

	inv_push_gyro_data(st, raw, calib, t);

	return 0;
}
static int inv_icm20690_set_lp_dis(struct inv_mpu_state *st, bool on)
{
	u8 v;
	int res;

	if (st->bus_type != BUS_SPI)
		return 0;

	if (!st->batch.fifo_wm_th
		&& (st->chip_config.compass_enable
		|| st->chip_config.gyro_enable
		|| st->mode_1k_on)
	)
		return 0;
	v = 0;
	if (st->chip_config.gyro_enable | st->chip_config.accel_enable) {
		if (!st->chip_config.gyro_enable)
			v |= BIT_PWR_GYRO_STBY;
		if (!st->chip_config.accel_enable)
			v |= BIT_PWR_ACCEL_STBY;
	} else if (st->chip_config.compass_enable) {
		v |= BIT_PWR_GYRO_STBY;
	}
	if (on)
		v |= BIT_FIFO_LP_EN;
	res = inv_plat_single_write(st, REG_PWR_MGMT_2, v);
	if (res)
		return res;

	return res;
}
static int inv_apply_soft_iron(struct inv_mpu_state *st, s16 *out_1, s32 *out_2)
{
	int *r, i, j;
	s64 tmp;

	r = st->final_compass_matrix;
	for (i = 0; i < THREE_AXES; i++) {
		tmp = 0;
		for (j = 0; j < THREE_AXES; j++)
			tmp  +=
			(s64)r[i * THREE_AXES + j] * (((int)out_1[j]) << 16);
		out_2[i] = (int)(tmp >> 30);
	}

	return 0;
}

int inv_check_fsync(struct inv_mpu_state *st, u8 fsync_status)
{
	u8 data[2];
	u16 fsync_counter;

	if (!st->chip_config.eis_enable)
		return 0;

	if ((fsync_status & INV_FSYNC_TEMP_BIT) && (st->eis.prev_state == 0)) {
		inv_plat_read(st, REG_ODR_DLY_CNT_HI, ODR_DLY_REG_COUNT, data);

		fsync_counter = data[0];
		fsync_counter <<= 8;
		fsync_counter += data[1];

		pr_debug("fsync= %d\n", fsync_counter);
		st->eis.eis_triggered = true;
		st->eis.prev_state = 1;
		st->eis.frame_count++;
		st->eis.eis_frame = true;
		inv_process_eis(st, fsync_counter);
	} else if (fsync_status & INV_FSYNC_TEMP_BIT) {
		st->eis.prev_state = 1;
	} else {
		st->eis.prev_state = 0;
	}

	return 0;
}

static int inv_push_sensor(struct inv_mpu_state *st, int ind, u64 t, u8 *d)
{
	int res, i;
	s16 out_1[3];
	s32 out_2[3];
#ifdef ACCEL_BIAS_TEST
	s16 acc[3], avg[3];
#endif

	res = 0;
	switch (ind) {
	case SENSOR_ACCEL:
		inv_convert_and_push_8bytes(st, ind, d, t, iden);
#ifdef ACCEL_BIAS_TEST
		acc[0] = be16_to_cpup((__be16 *) (d));
		acc[1] = be16_to_cpup((__be16 *) (d + 2));
		acc[2] = be16_to_cpup((__be16 *) (d + 4));
		if(inv_get_3axis_average(acc, avg, 0)){
			pr_debug("accel 200 samples average = %5d, %5d, %5d\n", avg[0], avg[1], avg[2]);
		}
#endif
		break;
	case SENSOR_TEMP:
		st->eis.gyro_counter++;
		inv_check_fsync(st, d[1]);
		break;
	case SENSOR_GYRO:
		inv_process_gyro(st, d, t);
		break;
	case SENSOR_COMPASS:
		if (d[0] != 1) {
			pr_debug("Bad compass data= %x\n", d[0]);
			return -EINVAL;
		}

		for (i = 0; i < 3; i++)
			out_1[i] = be16_to_cpup((__be16 *) (d + i * 2 + 2));
		inv_apply_soft_iron(st, out_1, out_2);
		inv_push_16bytes_buffer(st, ind, t, out_2, 0);
		break;
	default:
		break;
	}

	return res;
}

static int inv_push_20690_data(struct inv_mpu_state *st, u8 *d)
{
	u8 *dptr;
	int i;

	dptr = d;
	st->eis.eis_frame = false;

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			inv_get_dmp_ts(st, i);
			if (st->sensor[i].send && (!st->ts_algo.first_sample)) {
				st->sensor[i].sample_calib++;
				inv_push_sensor(st, i, st->sensor[i].ts, dptr);
			}
			dptr += st->sensor[i].sample_size;
		}
	}
	if (st->ts_algo.first_sample)
		st->ts_algo.first_sample--;
	st->header_count--;

	return 0;
}

static int inv_process_20690_data(struct inv_mpu_state *st)
{
	int total_bytes, tmp, res, fifo_count, pk_size, i;
	u8 *dptr, *d;
	u8 data[2];
	bool done_flag;
	u8 v;

	if(st->gesture_only_on && (!st->batch.timeout)) {
		res = inv_plat_read(st, REG_INT_STATUS, 1, data);
		if (res)
			return res;
		pr_debug("ges cnt=%d, statu=%x\n",
						st->gesture_int_count, data[0]);
		if (data[0] & (BIT_WOM_ALL_INT_EN)) {
			if (!st->gesture_int_count) {
				res = inv_plat_single_write(st, REG_INT_ENABLE,
					BIT_WOM_ALL_INT_EN | BIT_DATA_RDY_EN);
				if (res)
					return res;
				v = 0;
				if (st->chip_config.gyro_enable)
					v |= BITS_GYRO_FIFO_EN;

				if (st->chip_config.accel_enable)
					v |= BIT_ACCEL_FIFO_EN;
				res = inv_plat_single_write(st, REG_FIFO_EN, v);
				if (res)
					return res;
				/* First time wake up from WOM.
					We don't need data in the FIFO */
				res = inv_reset_fifo(st, true);
				st->gesture_int_count = WOM_DELAY_THRESHOLD;

				return res;
			}
			st->gesture_int_count = WOM_DELAY_THRESHOLD;
		} else {
			if (!st->gesture_int_count) {
				res = inv_plat_single_write(st, REG_FIFO_EN, 0);
				res = inv_plat_single_write(st, REG_INT_ENABLE,
					BIT_WOM_ALL_INT_EN);
				if (res)
					return res;
				return 0;
			}
			st->gesture_int_count--;
		}
	}

	if (st->batch.timeout || st->chip_config.eis_enable) {
		res = inv_plat_read(st, REG_DMP_INT_STATUS, 1, data);
		if (res)
			return res;
	}
	fifo_count = inv_get_last_run_time_non_dmp_record_mode(st);

	pr_debug("fifc= %d\n", fifo_count);
	if (st->mag_divider && st->mag_start_flag) {
		if (!fifo_count) {
			st->mag_start_flag = false;
		}
	}
	if (!fifo_count) {
		pr_debug("REG_FIFO_COUNT_H size is 0\n");
		return 0;
	}
	pk_size = st->batch.pk_size;
	if (!pk_size)
		return -EINVAL;
	fifo_count *= st->batch.pk_size;
	st->fifo_count = fifo_count;

	d = st->fifo_data_store;
	dptr = d;
	total_bytes = fifo_count;

	while (total_bytes > 0) {
		if (total_bytes < pk_size * MAX_FIFO_PACKET_READ)
			tmp = total_bytes;
		else
			tmp = pk_size * MAX_FIFO_PACKET_READ;
		res = inv_plat_read(st, REG_FIFO_R_W, tmp, dptr);
		if (res < 0) {
			pr_err("read REG_FIFO_R_W is failed\n");
			return res;
		}
		dptr += tmp;
		total_bytes -= tmp;
	}
	dptr = d;
	pr_debug("dd: %x, %x, %x, %x, %x, %x, %x, %x\n", d[0], d[1], d[2],
						d[3], d[4], d[5], d[6], d[7]);
	pr_debug("dd2: %x, %x, %x, %x, %x, %x, %x, %x\n", d[8], d[9], d[10],
					d[11], d[12], d[13], d[14], d[15]);
	total_bytes = fifo_count;
	if (st->mag_divider && st->mag_start_flag) {
		return 0;
	}

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			st->sensor[i].count =  total_bytes / pk_size;
		}
	}

	st->header_count = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on)
			st->header_count = max(st->header_count,
							st->sensor[i].count);
	}

	st->ts_algo.calib_counter++;
	inv_bound_timestamp(st);

	dptr = d;
	done_flag = false;

	while (!done_flag) {
		if (total_bytes >= pk_size) {
			res = inv_push_20690_data(st, dptr);
			if (res)
				return res;
			total_bytes -= pk_size;
			dptr += pk_size;
		} else {
			done_flag = true;
		}
	}

	return 0;
}

/*
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
irqreturn_t inv_read_fifo(int irq, void *dev_id)
{

	struct inv_mpu_state *st = (struct inv_mpu_state *)dev_id;
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int result;

	result = wait_event_interruptible_timeout(st->wait_queue,
					st->resume_state, msecs_to_jiffies(300));
	if (result <= 0)
		return IRQ_HANDLED;
	mutex_lock(&indio_dev->mlock);

	inv_icm20690_set_lp_dis(st, true);
	result = inv_process_20690_data(st);
	if (result)
		goto err_reset_fifo;
	inv_icm20690_set_lp_dis(st, false);
	mutex_unlock(&indio_dev->mlock);

	if (st->wake_sensor_received)
		wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));
	return IRQ_HANDLED;

err_reset_fifo:
	if ((!st->chip_config.gyro_enable) &&
	    (!st->chip_config.accel_enable) &&
	    (!st->chip_config.slave_enable) &&
	    (!st->chip_config.pressure_enable)) {
		inv_set_power(st, false);
		mutex_unlock(&indio_dev->mlock);

		return IRQ_HANDLED;
	}

	pr_err("error to reset fifo\n");
	inv_reset_fifo(st, true);
	inv_icm20690_set_lp_dis(st, false);
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;

}

int inv_flush_batch_data(struct iio_dev *indio_dev, int data)
{

	struct inv_mpu_state *st = iio_priv(indio_dev);

	st->wake_sensor_received = 0;
	inv_icm20690_set_lp_dis(st, true);
	inv_process_20690_data(st);
	inv_icm20690_set_lp_dis(st, false);

	if (st->wake_sensor_received)
		wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));

	inv_push_marker_to_buffer(st, END_MARKER, data);

	return 0;
}

