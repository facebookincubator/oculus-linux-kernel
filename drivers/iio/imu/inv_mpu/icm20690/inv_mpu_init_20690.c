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
#include "inv_mpu_iio.h"

static int inv_calc_gyro_sf(s8 pll)
{
	int a, r;
	int value, t;

	t = 102870L + 81L * pll;
	a = (1L << 30) / t;
	r = (1L << 30) - a * t;
	value = a * 797 * DMP_DIVIDER;
	value += (s64) ((a * 1011387LL * DMP_DIVIDER) >> 20);
	value += r * 797L * DMP_DIVIDER / t;
	value += (s32) ((s64) ((r * 1011387LL * DMP_DIVIDER) >> 20)) / t;
	value <<= 1;

	return value;
}

static int inv_read_timebase(struct inv_mpu_state *st)
{

	inv_plat_single_write(st, REG_CONFIG, 3);

	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_ACCEL].base_time_1k = NSEC_PER_SEC;
	/* talor expansion to calculate base time unit */
	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_GYRO].base_time_1k = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time_1k = NSEC_PER_SEC;

	st->eng_info[ENGINE_ACCEL].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_GYRO].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_I2C].orig_rate = BASE_SAMPLE_RATE;

	st->gyro_sf = inv_calc_gyro_sf(0);

	return 0;
}

int inv_set_gyro_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_plat_single_write(st, REG_GYRO_CONFIG,
				   st->chip_config.fsr << SHIFT_GYRO_FS_SEL);

	return result;
}

int inv_set_accel_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_plat_single_write(st, REG_ACCEL_CONFIG,
				st->chip_config.accel_fs << SHIFT_ACCEL_FS);
	return result;
}

int inv_set_accel_intel(struct inv_mpu_state *st)
{
	int result = 0;
	u8 w;

#define ACCEL_WOM_THR		7

	w = ACCEL_WOM_THR;
	result = inv_plat_single_write(st, REG_ACCEL_WOM_THR, w);
	result |= inv_plat_single_write(st, REG_ACCEL_WOM_X_THR, w);
	result |= inv_plat_single_write(st, REG_ACCEL_WOM_Y_THR, w);
	result |= inv_plat_single_write(st, REG_ACCEL_WOM_Z_THR, w);

	return result;
}

static int inv_init_secondary(struct inv_mpu_state *st)
{
	st->slv_reg[0].addr = REG_I2C_SLV0_ADDR;
	st->slv_reg[0].reg = REG_I2C_SLV0_REG;
	st->slv_reg[0].ctrl = REG_I2C_SLV0_CTRL;
	st->slv_reg[0].d0 = REG_I2C_SLV0_DO;

	st->slv_reg[1].addr = REG_I2C_SLV1_ADDR;
	st->slv_reg[1].reg = REG_I2C_SLV1_REG;
	st->slv_reg[1].ctrl = REG_I2C_SLV1_CTRL;
	st->slv_reg[1].d0 = REG_I2C_SLV1_DO;

	st->slv_reg[2].addr = REG_I2C_SLV2_ADDR;
	st->slv_reg[2].reg = REG_I2C_SLV2_REG;
	st->slv_reg[2].ctrl = REG_I2C_SLV2_CTRL;
	st->slv_reg[2].d0 = REG_I2C_SLV2_DO;

	return 0;
}

static void inv_init_sensor_struct(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	st->sensor[SENSOR_ACCEL].sample_size = BYTES_PER_SENSOR;
	st->sensor[SENSOR_TEMP].sample_size = BYTES_FOR_TEMP;
	st->sensor[SENSOR_GYRO].sample_size = BYTES_PER_SENSOR;
	st->sensor[SENSOR_COMPASS].sample_size = BYTES_FOR_COMPASS;

	st->sensor[SENSOR_ACCEL].a_en = true;
	st->sensor[SENSOR_GYRO].a_en = false;

	st->sensor[SENSOR_ACCEL].g_en = false;
	st->sensor[SENSOR_GYRO].g_en = true;

	st->sensor[SENSOR_ACCEL].c_en = false;
	st->sensor[SENSOR_GYRO].c_en = false;
	st->sensor[SENSOR_COMPASS].c_en = true;

	st->sensor[SENSOR_ACCEL].p_en = false;
	st->sensor[SENSOR_GYRO].p_en = false;

	st->sensor[SENSOR_ACCEL].engine_base = ENGINE_ACCEL;
	st->sensor[SENSOR_GYRO].engine_base = ENGINE_GYRO;

	st->sensor_l[SENSOR_L_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GESTURE_ACCEL].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_GYRO_CAL].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_EIS_GYRO].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_SIXQ].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_PEDQ].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_NINEQ].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_MAG].base = SENSOR_COMPASS;
	st->sensor_l[SENSOR_L_MAG_CAL].base = SENSOR_COMPASS;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_GEOMAG_WAKE].base = SENSOR_ACCEL;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].base = SENSOR_GYRO;
	st->sensor_l[SENSOR_L_MAG_WAKE].base = SENSOR_COMPASS;
	st->sensor_l[SENSOR_L_MAG_CAL_WAKE].base = SENSOR_COMPASS;

	st->sensor_l[SENSOR_L_ACCEL].header = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GESTURE_ACCEL].header = ACCEL_HDR;
	st->sensor_l[SENSOR_L_GYRO].header = GYRO_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL].header = GYRO_CALIB_HDR;
	st->sensor_l[SENSOR_L_MAG].header = COMPASS_HDR;
	st->sensor_l[SENSOR_L_MAG_CAL].header = COMPASS_CALIB_HDR;
	st->sensor_l[SENSOR_L_EIS_GYRO].header = EIS_GYRO_HDR;
	st->sensor_l[SENSOR_L_SIXQ].header = SIXQUAT_HDR;
	st->sensor_l[SENSOR_L_NINEQ].header = NINEQUAT_HDR;
	st->sensor_l[SENSOR_L_PEDQ].header = PEDQUAT_HDR;
	st->sensor_l[SENSOR_L_PRESSURE].header = PRESSURE_HDR;
	st->sensor_l[SENSOR_L_ALS].header = ALS_HDR;
	st->sensor_l[SENSOR_L_GEOMAG].header = GEOMAG_HDR;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].header = ACCEL_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_WAKE].header = GYRO_WAKE_HDR;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].header = GYRO_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_WAKE].header = COMPASS_WAKE_HDR;
	st->sensor_l[SENSOR_L_MAG_CAL_WAKE].header = COMPASS_CALIB_WAKE_HDR;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].header = SIXQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].header = NINEQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].header = PEDQUAT_WAKE_HDR;
	st->sensor_l[SENSOR_L_PRESSURE_WAKE].header = PRESSURE_WAKE_HDR;
	st->sensor_l[SENSOR_L_ALS_WAKE].header = ALS_WAKE_HDR;
	st->sensor_l[SENSOR_L_GEOMAG_WAKE].header = GEOMAG_WAKE_HDR;

	st->sensor_l[SENSOR_L_ACCEL].wake_on = false;
	st->sensor_l[SENSOR_L_GYRO].wake_on = false;
	st->sensor_l[SENSOR_L_GYRO_CAL].wake_on = false;
	st->sensor_l[SENSOR_L_MAG].wake_on = false;
	st->sensor_l[SENSOR_L_MAG_CAL].wake_on = false;
	st->sensor_l[SENSOR_L_EIS_GYRO].wake_on = false;
	st->sensor_l[SENSOR_L_SIXQ].wake_on = false;
	st->sensor_l[SENSOR_L_NINEQ].wake_on = false;
	st->sensor_l[SENSOR_L_PEDQ].wake_on = false;
	st->sensor_l[SENSOR_L_PRESSURE].wake_on = false;
	st->sensor_l[SENSOR_L_ALS].wake_on = false;
	st->sensor_l[SENSOR_L_GEOMAG].wake_on = false;

	st->sensor_l[SENSOR_L_ACCEL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GYRO_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GYRO_CAL_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_MAG_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_SIXQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_NINEQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_PEDQ_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_PRESSURE_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_ALS_WAKE].wake_on = true;
	st->sensor_l[SENSOR_L_GEOMAG_WAKE].wake_on = true;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].sample_size =
	    ACCEL_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].sample_size =
	    GYRO_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].sample_size =
	    CPASS_ACCURACY_SZ;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].output = ACCEL_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].output = GYRO_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].output =
	    CPASS_ACCURACY_SET;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].header = ACCEL_ACCURACY_HDR;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].header = GYRO_ACCURACY_HDR;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].header =
	    COMPASS_ACCURACY_HDR;
}

static int inv_init_config(struct inv_mpu_state *st)
{
	int res, i;

	st->batch.overflow_on = 0;
	st->chip_config.fsr = MPU_INIT_GYRO_SCALE;
	st->chip_config.accel_fs = MPU_INIT_ACCEL_SCALE;
	st->ped.int_thresh = MPU_INIT_PED_INT_THRESH;
	st->ped.step_thresh = MPU_INIT_PED_STEP_THRESH;
	st->chip_config.low_power_gyro_on = 1;
	st->eis.count_precision = NSEC_PER_USEC;
	st->firmware = 0;
	st->ts_algo.gyro_ts_shift = 1500 * NSEC_PER_USEC;

	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;

	inv_init_sensor_struct(st);
	inv_init_secondary(st);
	res = inv_read_timebase(st);
	if (res)
		return res;

	res = inv_set_gyro_sf(st);
	if (res)
		return res;
	res = inv_set_accel_sf(st);
	if (res)
		return res;
	res =  inv_set_accel_intel(st);
	if (res)
		return res;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].ts = 0;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].previous_ts = 0;

	return res;
}

static int inv_compass_dmp_cal(struct inv_mpu_state *st)
{
        s8 compass_m[NINE_ELEM], m[NINE_ELEM];
        s8 trans[NINE_ELEM];
        s32 tmp_m[NINE_ELEM];
        int i, j, k;
        int sens[THREE_AXES];
        int *adj;
        int scale, shift;

	for (i = 0; i < NINE_ELEM; i++) {
		compass_m[i] = 0;
		m[i] = 0;
		trans[i] = 0;
	}
	compass_m[0] = 1;
	m[0] = 1;
	trans[0] = 1;

	compass_m[4] = 1;
	m[4] = 1;
	trans[4] = 1;

	compass_m[8] = 1;
	m[8] = 1;
	trans[8] = 1;

        adj = st->current_compass_matrix;
        st->slave_compass->get_scale(st, &scale);
	/* scale = (1 << 30); */

        if ((COMPASS_ID_AK8975 == st->plat_data.sec_slave_id) ||
            (COMPASS_ID_AK8972 == st->plat_data.sec_slave_id) ||
            (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id) ||
            (COMPASS_ID_AK09912 == st->plat_data.sec_slave_id) ||
            (COMPASS_ID_AK09916 == st->plat_data.sec_slave_id))
                shift = AK89XX_SHIFT;
        else
                shift = AK99XX_SHIFT;

        for (i = 0; i < THREE_AXES; i++) {
                sens[i] = st->chip_info.compass_sens[i] + 128;
                sens[i] = inv_q30_mult(sens[i] << shift, scale);
        }

        for (i = 0; i < NINE_ELEM; i++) {
                adj[i] = compass_m[i] * sens[i % THREE_AXES];
                tmp_m[i] = 0;
        }
        for (i = 0; i < THREE_AXES; i++)
                for (j = 0; j < THREE_AXES; j++)
                        for (k = 0; k < THREE_AXES; k++)
                                tmp_m[THREE_AXES * i + j] +=
                                    trans[THREE_AXES * i + k] *
                                    adj[THREE_AXES * k + j];

        for (i = 0; i < NINE_ELEM; i++)
                st->final_compass_matrix[i] = adj[i];

        return 0;
}
int inv_mpu_initialize(struct inv_mpu_state *st)
{
	u8 v;
	int result;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;

	conf = &st->chip_config;
	plat = &st->plat_data;

	/* verify whoami */
	result = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	if (result)
		return result;
	pr_info("whoami= %x\n", v);
	if (v == 0x00 || v == 0xff)
		return -ENODEV;

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		return result;
	usleep_range(REG_UP_TIME_USEC, REG_UP_TIME_USEC);
	msleep(100);
	/* toggle power state */
	result = inv_set_power(st, false);
	if (result)
		return result;
	result = inv_set_power(st, true);
	if (result)
		return result;

	// hotfix - ois stall issue
	msleep(10);

	// hotfix - accel offset shift issue
	result = inv_plat_single_write(st, 0x76, 0x20);
	if (result)
		return result;
	result = inv_plat_read(st, REG_CONFIG, 1, &v);
	if (result)
		return result;
	v&=0x7F;
	result = inv_plat_single_write(st, REG_CONFIG, v);
	if (result)
		return result;
	result = inv_plat_single_write(st, 0x76, 0x00);
	if (result)
		return result;

	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	result = inv_init_config(st);
	if (result)
		return result;

	if (SECONDARY_SLAVE_TYPE_COMPASS == plat->sec_slave_type)
		st->chip_config.has_compass = 1;
	else
		st->chip_config.has_compass = 0;

	if (st->chip_config.has_compass) {
		result = inv_mpu_setup_compass_slave(st);
		if (result)
			pr_err("compass setup failed\n");
		inv_compass_dmp_cal(st);
	}
	result = mem_r(MPU_SOFT_REV_ADDR, 1, &v);
	pr_info("sw_rev=%x, res=%d\n", v, result);
	if (result)
		return result;
	st->chip_config.lp_en_mode_off = 0;

	pr_info("%s: Mask %X, v = %X, lp mode off= %d\n", __func__,
		MPU_SOFT_REV_MASK, v, st->chip_config.lp_en_mode_off);
	result = inv_set_power(st, false);

	pr_info("%s: initialize result is %d....\n", __func__, result);
	return 0;
}
