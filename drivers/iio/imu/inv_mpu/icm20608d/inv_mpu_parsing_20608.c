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
static char fsync_delay[] = {4, 5, 1, 2, 3};

static int inv_push_accuracy(struct inv_mpu_state *st, int ind, u16 accur)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	u16 hdr;

	hdr = st->sensor_accuracy[ind].header;
	if (st->sensor_acurracy_flag[ind]) {
		if (!accur)
			accur = DEFAULT_ACCURACY;
		else
			st->sensor_acurracy_flag[ind] = 0;
	}
	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(buf + sizeof(hdr), &accur, sizeof(accur));
	iio_push_to_buffers(indio_dev, buf);

	pr_debug("Accuracy for sensor [%d] is [%d]\n", ind, accur);

	return 0;
}

static int inv_process_gyro(struct inv_mpu_state *st, u8 *d, u64 t)
{
	s16 raw[3];
	s32 bias[3];
	s32 scaled_bias[3];
	s32 calib[3];
	int i;
#define BIAS_UNIT 2859

	for (i = 0; i < 3; i++)
		raw[i] = be16_to_cpup((__be16 *) (d + i * 2));
	for (i = 0; i < 3; i++)
		bias[i] = be32_to_int(d + 6 + i * 4);

	for (i = 0; i < 3; i++) {
		scaled_bias[i] = ((bias[i] << 14) / BIAS_UNIT) << 1;
		calib[i] = (raw[i] << 15) - scaled_bias[i];
	}

	if ((scaled_bias[0] != st->gyro_bias[0]) && scaled_bias[0]) {
		for (i = 0; i < 3; i++)
			st->gyro_bias[i] = scaled_bias[i];
		inv_push_accuracy(st, SENSOR_GYRO_ACCURACY, 3);
	}

	inv_push_gyro_data(st, raw, calib, t);

	return 0;
}
static int inv_check_fsync(struct inv_mpu_state *st, u8 fsync_status)
{
	if (!st->chip_config.eis_enable)
		return 0;
	if ((fsync_status & INV_FSYNC_TEMP_BIT) && (st->eis.prev_state == 0)) {
		pr_debug("fsync\n");
		st->eis.eis_triggered = true;
		st->eis.fsync_delay = 1;
		st->eis.prev_state = 1;
		st->eis.frame_count++;
		st->eis.eis_frame = true;
	} else if (fsync_status & INV_FSYNC_TEMP_BIT) {
		st->eis.prev_state = 1;
	} else {
		st->eis.prev_state = 0;
	}

	return 0;
}
static int inv_push_sensor(struct inv_mpu_state *st, int ind, u64 t, u8 *d)
{
#ifdef ACCEL_BIAS_TEST
	s16 acc[3], avg[3];
#endif
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
		inv_check_fsync(st, d[1]);
		break;
	case SENSOR_GYRO:
		inv_process_gyro(st, d, t);
		break;
	case SENSOR_SIXQ:
		inv_convert_and_push_16bytes(st, ind, d + 2, t, iden);
		break;
	case SENSOR_THREEQ:
                inv_convert_and_push_16bytes(st, ind, d + 2, t, iden);
                break;
	case SENSOR_PEDQ:
		inv_convert_and_push_8bytes(st, ind, d, t, iden);
		break;
	default:
		break;
	}

	return 0;
}

int inv_get_packet_size(struct inv_mpu_state *st, u16 hdr,
			       u32 *pk_size, u8 *dptr)
{
	int i, size;

	size = HEADER_SZ;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if ((hdr & ~PED_STEPIND_SET) == st->sensor[i].output) {
			if (st->sensor[i].on)
				size += st->sensor[i].sample_size;
			else
				return -EINVAL;
		}
	}

	if (hdr == PED_STEPDET_SET) {
		if (st->chip_config.step_detector_on) {
			size += HEADER_SZ;
			size += PED_STEPDET_TIMESTAMP_SZ;
		} else {
			pr_err("ERROR: step detector should not be here\n");
			return -EINVAL;
		}
	}
	if (hdr == FSYNC_HDR) {
		if (st->chip_config.eis_enable) {
			size += FSYNC_PK_SZ;
		} else {
			pr_err("ERROR: eis packet should not be here\n");
			return -EINVAL;
		}
	}

	*pk_size = size;

	return 0;
}

int inv_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr)
{
	int i;
	u64 t;
	bool data_header;
	u16 delay;

	t = 0;
	pr_debug("hdr= %x\n", hdr);
	data_header = false;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if ((hdr & ~PED_STEPIND_SET) == st->sensor[i].output) {
			inv_get_dmp_ts(st, i);
			st->sensor[i].sample_calib++;
			inv_push_sensor(st, i, st->sensor[i].ts, dptr);
			dptr += st->sensor[i].sample_size;
			t = st->sensor[i].ts;
			data_header = true;
		}
	}
	if (data_header)
		st->header_count--;
	if (hdr == PED_STEPDET_SET) {
		dptr += HEADER_SZ;
		inv_process_step_det(st, dptr);
		dptr += PED_STEPDET_TIMESTAMP_SZ;
		st->step_det_count--;
	}

	if (hdr & PED_STEPIND_SET)
		inv_push_step_indicator(st, t);
	if (hdr == FSYNC_HDR) {
		st->eis.current_sync = true;
		st->eis.eis_triggered = true;
		st->eis.frame_count++;
		st->eis.eis_frame = true;
		delay = be16_to_cpup((__be16 *) (dptr));
		delay ^= 0x7bcf;
		if (delay > 4) {
			pr_info("ERROR FSYNC value= %d\n", delay);
			delay = 4;
		}
		st->eis.fsync_delay = fsync_delay[delay];
		dptr += FSYNC_PK_SZ;
	}


	return 0;
}

int inv_pre_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr)
{
	int i;
	bool data_header;

	data_header = false;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if ((hdr & ~PED_STEPIND_SET) == st->sensor[i].output) {
			st->sensor[i].count++;
			dptr += st->sensor[i].sample_size;
			data_header = true;
		}
	}
	if (data_header)
		st->header_count++;
	if (hdr == PED_STEPDET_SET) {
		st->step_det_count++;
		dptr += HEADER_SZ;
		dptr += PED_STEPDET_TIMESTAMP_SZ;
	}
	if (hdr == FSYNC_HDR)
		dptr += FSYNC_PK_SZ;

	return 0;
}

static int inv_process_dmp_interrupt(struct inv_mpu_state *st)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 d[1];
	int result, step;

#define DMP_INT_SMD		0x04
#define DMP_INT_PED		0x08

	if ((!st->smd.on) && (!st->ped.on))
		return 0;

	result = inv_plat_read(st, REG_DMP_INT_STATUS, 1, d);
	if (result) {
		pr_info("REG_DMP_INT_STATUS result [%d]\n", result);
		return result;
	}

	if (d[0] & DMP_INT_SMD) {
		pr_info("Sinificant motion detected\n");
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_smd");
		st->smd.on = false;
		st->trigger_state = EVENT_TRIGGER;
		set_inv_enable(indio_dev);
		st->wake_sensor_received = true;
	}

	step = -1;
	if (st->ped.on && (!st->batch.on)) {
		if (st->ped.int_on) {
			if (d[0] & DMP_INT_PED) {
				sysfs_notify(&indio_dev->dev.kobj, NULL,
					     "poll_pedometer");
				inv_get_pedometer_steps(st, &step);
			}
		} else {
			inv_get_pedometer_steps(st, &step);
		}

		if ((step != -1) && (step != st->prev_steps)) {
			inv_send_steps(st, step, st->ts_algo.last_run_time);
			st->prev_steps = step;
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
	u8 d[1];

	result = wait_event_interruptible_timeout(st->wait_queue,
					st->resume_state, msecs_to_jiffies(300));
	if (result <= 0)
		return IRQ_HANDLED;
	mutex_lock(&indio_dev->mlock);

	st->ts_algo.last_run_time = get_time_ns();
	st->wake_sensor_received = false;

	if (st->chip_config.is_asleep)
		goto end_read_fifo;

	st->activity_size = 0;
	/* 20608 is operating under non low power mode, no need to switch */
	result = inv_process_dmp_interrupt(st);
	if (result)
		goto end_read_fifo;

	result = inv_process_dmp_data(st);

	/* enforce clock source switch after read FIFO */
	result |= inv_plat_read(st, REG_PWR_MGMT_1, 1, d);
	result |= inv_plat_single_write(st, REG_PWR_MGMT_1, d[0]);

	if (st->activity_size > 0)
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_activity");
	if (result)
		goto err_reset_fifo;

end_read_fifo:
	mutex_unlock(&indio_dev->mlock);

	if (st->wake_sensor_received)
		wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));

	return IRQ_HANDLED;

err_reset_fifo:
	if ((!st->chip_config.gyro_enable) &&
	    (!st->chip_config.accel_enable) &&
	    (!st->chip_config.slave_enable) &&
	    (!st->chip_config.pressure_enable)) {
		mutex_unlock(&indio_dev->mlock);

		return IRQ_HANDLED;
	}

	pr_err("error to reset fifo\n");
	inv_reset_fifo(st, true);
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;

}

int inv_flush_batch_data(struct iio_dev *indio_dev, int data)
{

	struct inv_mpu_state *st = iio_priv(indio_dev);

	st->wake_sensor_received = 0;
	if (inv_process_dmp_data(st))
		pr_err("error on batch.. need reset fifo\n");

	if (st->wake_sensor_received)
		wake_lock_timeout(&st->wake_lock, msecs_to_jiffies(200));

	inv_push_marker_to_buffer(st, END_MARKER, data);

	return 0;
}

