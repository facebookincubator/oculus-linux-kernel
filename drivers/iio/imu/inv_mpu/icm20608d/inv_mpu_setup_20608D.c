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

static int inv_set_batchmode(struct inv_mpu_state *st, bool enable)
{
	if (enable)
		st->cntl2 |= BATCH_MODE_EN;

	return 0;
}

static int inv_calc_engine_dur(struct inv_engine_info *ei)
{
	if (!ei->running_rate)
		return -EINVAL;
	ei->dur = ei->base_time / ei->orig_rate;
	ei->dur *= ei->divider;

	return 0;
}

static int inv_batchmode_calc(struct inv_mpu_state *st)
{
	int b, timeout;
	int i, bps;
	enum INV_ENGINE eng;

	bps = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			bps += (st->sensor[i].sample_size + 2) *
			    st->sensor[i].rate;
		}
	}
	if (bps) {
		b = st->batch.timeout * bps;
		if ((b > (FIFO_SIZE * MSEC_PER_SEC)) &&
		    (!st->batch.overflow_on))
			timeout = FIFO_SIZE * MSEC_PER_SEC / bps;
		else
			timeout = st->batch.timeout;
	} else {
		if (st->chip_config.step_detector_on ||
		    st->step_counter_l_on ||
		    st->step_counter_wake_l_on ||
		    st->chip_config.activity_eng_on) {
			timeout = st->batch.timeout;
		} else {
			return -EINVAL;
		}
	}
	if (st->chip_config.gyro_enable)
		eng = ENGINE_GYRO;
	else
		eng = ENGINE_ACCEL;
	b = st->eng_info[eng].dur / USEC_PER_MSEC;
	st->batch.engine_base = eng;
	st->batch.counter = timeout * USEC_PER_MSEC / b;

	if (st->batch.counter)
		st->batch.on = true;

	return 0;
}

static int inv_set_default_batch(struct inv_mpu_state *st)
{
	if (st->batch.max_rate > DEFAULT_BATCH_RATE) {
		st->batch.default_on = true;
		st->batch.counter = DEFAULT_BATCH_TIME * NSEC_PER_MSEC /
		    st->eng_info[ENGINE_GYRO].dur;
	}

	return 0;
}

int inv_batchmode_setup(struct inv_mpu_state *st)
{
	int r;
	bool on;

	st->batch.default_on = false;
	if (st->batch.timeout > 0) {
		r = inv_batchmode_calc(st);
		if (r)
			return r;
	} else {
		r = inv_set_default_batch(st);
		if (r)
			return r;
	}

	on = (st->batch.on || st->batch.default_on);

	if (on) {
		r = write_be32_to_mem(st, 0, BM_BATCH_CNTR);
		if (r)
			return r;
		r = write_be32_to_mem(st, st->batch.counter, BM_BATCH_THLD);
		if (r)
			return r;
	}

	r = inv_set_batchmode(st, on);

	return r;
}

static int inv_turn_on_fifo(struct inv_mpu_state *st)
{
	u8 w, x;
	int r;

	r = inv_plat_single_write(st, REG_USER_CTRL,
						(BIT_FIFO_RST | BIT_DMP_RST));
	if (r)
		return r;
	w = 0;
	x = 0;
	r = inv_plat_single_write(st, REG_FIFO_EN, 0);
	if (r)
		return r;

	/* turn on user ctrl register */
	w = BIT_DMP_RST;
	r = inv_plat_single_write(st, REG_USER_CTRL, w | st->i2c_dis);
	if (r)
		return r;
	msleep(DMP_RESET_TIME);

	w = BIT_DMP_INT_EN | BIT_FIFO_OFLOW_EN;
	r = inv_plat_single_write(st, REG_INT_ENABLE, w);
	if (r)
		return r;

	w = BIT_FIFO_EN;
	w |= BIT_DMP_EN;
	r = inv_plat_single_write(st, REG_USER_CTRL, w | st->i2c_dis);

	return r;
}

/*
 *  inv_reset_fifo() - Reset FIFO related registers.
 */
int inv_reset_fifo(struct inv_mpu_state *st, bool turn_off)
{
	int r, i;
	struct inv_timestamp_algo *ts_algo = &st->ts_algo;

	r = inv_turn_on_fifo(st);
	if (r)
		return r;

	ts_algo->last_run_time = get_time_ns();
	ts_algo->reset_ts = ts_algo->last_run_time;

	st->last_temp_comp_time = ts_algo->last_run_time;
	st->left_over_size = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		st->sensor[i].calib_flag = 0;
		st->sensor[i].time_calib = ts_algo->last_run_time;
	}

	ts_algo->calib_counter = 0;

	return 0;
}

static int inv_turn_on_engine(struct inv_mpu_state *st)
{
	u8 w, v;
	int r;

	if (st->chip_config.gyro_enable | st->chip_config.accel_enable) {
		w = 0;
		if (!st->chip_config.gyro_enable)
			w |= BIT_PWR_GYRO_STBY;
		if (!st->chip_config.accel_enable)
			w |= BIT_PWR_ACCEL_STBY;
	} else {
		w = (BIT_PWR_GYRO_STBY | BIT_PWR_ACCEL_STBY);
	}
	r = inv_plat_read(st, REG_PWR_MGMT_2, 1, &v);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_PWR_MGMT_2, w);
	pr_debug("turn on engine REG %X\n", w);
	if (r)
		return r;
	if (st->chip_config.gyro_enable && (v & BIT_PWR_GYRO_STBY)) {
		msleep(INV_ICM20608_GYRO_START_TIME);
	}
	if (st->chip_config.accel_enable && (v & BIT_PWR_ACCEL_STBY)) {
		msleep(INV_ICM20608_ACCEL_START_TIME);
	}
	if (st->chip_config.has_compass) {
		if (st->chip_config.compass_enable)
			r = st->slave_compass->resume(st);
		else
			r = st->slave_compass->suspend(st);
		if (r)
			return r;
	}
	if (st->chip_config.has_als) {
		if (st->chip_config.als_enable)
			r = st->slave_als->resume(st);
		else
			r = st->slave_als->suspend(st);
		if (r)
			return r;
	}
	if (st->chip_config.has_pressure) {
		if (st->chip_config.pressure_enable)
			r = st->slave_pressure->resume(st);
		else
			r = st->slave_pressure->suspend(st);
		if (r)
			return r;
	}

	return 0;
}

static int inv_setup_dmp_rate(struct inv_mpu_state *st)
{
	int i, result;
	int div[SENSOR_NUM_MAX];
	bool d_flag;

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (!st->sensor[i].rate) {
				pr_err("sensor %d rate is zero\n", i);
				return -EINVAL;
			}
			pr_debug(
			"[Before]sensor %d rate [%d], running_rate %d\n",
				i, st->sensor[i].rate,
			  st->eng_info[st->sensor[i].engine_base].running_rate);

			div[i] =
			    st->eng_info[st->sensor[i].engine_base].
			    running_rate / st->sensor[i].rate;
			if (!div[i])
				div[i] = 1;
			st->sensor[i].rate = st->eng_info
			    [st->sensor[i].engine_base].running_rate / div[i];

			pr_debug(
			"sensor %d rate [%d] div [%d] running_rate [%d]\n",
				i, st->sensor[i].rate, div[i],
			st->eng_info[st->sensor[i].engine_base].running_rate);
		}
	}
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			st->cntl |= st->sensor[i].output;
			st->sensor[i].dur =
			    st->eng_info[st->sensor[i].engine_base].dur *
			    div[i];
			st->sensor[i].div = div[i];
			result = inv_write_2bytes(st,
					st->sensor[i].odr_addr, div[i] - 1);

			if (result)
				return result;
			result = inv_write_2bytes(st,
					st->sensor[i].counter_addr, 0);
			if (result)
				return result;
		}
	}

	d_flag = 0;
	for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
		if (st->sensor_accuracy[i].on)
			st->cntl2 |= st->sensor_accuracy[i].output;
		d_flag |= st->sensor_accuracy[i].on;
	}
	d_flag |= st->chip_config.activity_eng_on;
	d_flag |= st->chip_config.pick_up_enable;
	d_flag |= st->chip_config.eis_enable;
	if (d_flag)
		st->cntl |= HEADER2_SET;

	if (st->chip_config.step_indicator_on)
		st->cntl |= PED_STEPIND_SET;
	if (st->chip_config.step_detector_on)
		st->cntl |= PED_STEPDET_SET;
	if (st->chip_config.activity_eng_on) {
		st->cntl2 |= ACT_RECOG_SET;
		st->cntl2 |= SECOND_SEN_OFF_SET;
	}
	if (st->chip_config.pick_up_enable)
		st->cntl2 |= FLIP_PICKUP_SET;

	if (st->chip_config.eis_enable)
		st->cntl2 |= FSYNC_SET;

	st->batch.on = false;
	if (!st->chip_config.dmp_event_int_on) {
		result = inv_batchmode_setup(st);
		if (result)
			return result;
	}

	return 0;
}

/*
 *  inv_set_lpf() - set low pass filer based on fifo rate.
 */
static int inv_set_lp_config(struct inv_mpu_state *st, int rate)
{
	const short accel_rate[] = {0, 0, 0, 1, 3, 7, 15, 31, 62, 125, 250, 500};
	const short gyro_rate[] = {7, 14, 27, 54, 108, 211, 391, 622};
	int i, h, result, data = 0;

	h =  rate;

	if(st->chip_config.gyro_enable && !st->chip_config.eis_enable){
		data |= BIT_GYRO_CYCLE;
		i = 0;
		while ((h > gyro_rate[i]) && (i < ARRAY_SIZE(gyro_rate) - 1))
			i++;
		data |= (7 - i) << 4; // G_AVGCFG
		i = 0;
		while ((h > accel_rate[i]) && (i < ARRAY_SIZE(accel_rate) - 1))
			i++;
		data |= i;// LPOSC_CLKSEL
	} else {
		data = 0;
	}

	pr_debug("lp_mode_cfg = %d\n", data);

	result = inv_plat_single_write(st, REG_LP_MODE_CFG, data);
	if (result)
		return result;

	return 0;
}

/*
 *  inv_set_lpf() - set low pass filer based on fifo rate.
 */
static int inv_set_lpf(struct inv_mpu_state *st, int rate)
{
	const short hz[] = {176, 92, 41, 20, 10, 5};
	const int   d[] = {INV_FILTER_188HZ, INV_FILTER_98HZ,
			INV_FILTER_42HZ, INV_FILTER_20HZ,
			INV_FILTER_10HZ, INV_FILTER_5HZ};
	int i, h, data, result;

	h = rate >> 1;
	i = 0;
	while ((h < hz[i]) && (i < ARRAY_SIZE(d) - 1))
		i++;
	data = d[i];
	if (st->chip_config.eis_enable)
		data |= EXT_SYNC_SET;

	pr_debug("lpf = %d\n", data);

	result = inv_plat_single_write(st, REG_CONFIG, data);
	if (result)
		return result;

	result = inv_set_lp_config(st, rate);

	st->chip_config.lpf = data;
	return 0;
}

static int inv_set_div(struct inv_mpu_state *st, int a_d, int g_d)
{
	int result, div;

	if (st->chip_config.gyro_enable)
		div = g_d;
	else
		div = a_d;
	if (st->chip_config.eis_enable)
		div = 0;

	pr_debug("div= %d\n", div);
	result = inv_plat_single_write(st, REG_SAMPLE_RATE_DIV, div);

	return result;
}

static int inv_set_rate(struct inv_mpu_state *st)
{
	int g_d, a_d, result;

	result = inv_setup_dmp_rate(st);
	if (result)
		return result;

	g_d = st->eng_info[ENGINE_GYRO].divider - 1;
	a_d = st->eng_info[ENGINE_ACCEL].divider - 1;
	result = inv_set_div(st, a_d, g_d);
	if (result)
		return result;
	result = inv_set_lpf(st, st->eng_info[ENGINE_GYRO].running_rate);

	return result;
}

static int inv_set_fifo_size(struct inv_mpu_state *st)
{
	return 0;
}

static void inv_enable_accel_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	if (enable)
		st->motion_event_cntl |= (ACCEL_CAL_EN);

	return;
}

static void inv_enable_gyro_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	if (enable)
		st->motion_event_cntl |= (GYRO_CAL_EN);

	return;
}

static int inv_set_wom(struct inv_mpu_state *st)
{
	return 0;
}

static void inv_setup_events(struct inv_mpu_state *st)
{
	if (st->ped.engine_on)
		st->motion_event_cntl |= (PEDOMETER_EN);
	if (st->smd.on)
		st->motion_event_cntl |= (SMD_EN);
	if (st->ped.int_on)
		st->motion_event_cntl |= (PEDOMETER_INT_EN);
	if (st->chip_config.pick_up_enable)
		st->motion_event_cntl |= (FLIP_PICKUP_EN);
	if (st->chip_config.geomag_enable)
		st->motion_event_cntl |= GEOMAG_RV_EN;
	if (!st->chip_config.activity_eng_on)
		st->motion_event_cntl |= BAC_ACCEL_ONLY_EN;
}

static int inv_setup_sensor_interrupt(struct inv_mpu_state *st)
{
	int i, ind, rate;
	u16 cntl;

	cntl = 0;
	ind = -1;
	rate = 0;

	if (st->batch.on) {
		for (i = 0; i < SENSOR_NUM_MAX; i++) {
			if (st->sensor[i].on)
				cntl |= st->sensor[i].output;
		}
	}
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (st->sensor[i].rate > rate) {
				ind = i;
				rate = st->sensor[i].rate;
			}
		}
	}

	if (ind != -1)
		cntl |= st->sensor[ind].output;
	if (st->chip_config.step_detector_on)
		cntl |= PED_STEPDET_SET;

	return 0;
}

static int inv_mpu_reset_pickup(struct inv_mpu_state *st)
{
	return 0;
}

static int inv_setup_dmp(struct inv_mpu_state *st)
{
	int result;

	result = inv_setup_sensor_interrupt(st);
	if (result)
		return result;

	inv_enable_accel_cal_V3(st, st->accel_cal_enable);
	inv_enable_gyro_cal_V3(st, st->gyro_cal_enable);

	if (st->ped.engine_on) {
		result = write_be32_to_mem(st, 0, DMPRATE_CNTR);
		if (result)
			return result;
		result = write_be16_to_mem(st, 0, PEDSTEP_IND);
		if (result)
			return result;
	}

	if (st->chip_config.pick_up_enable) {
		result = inv_mpu_reset_pickup(st);
		if (result)
			return result;
	}

	inv_setup_events(st);

	result = inv_set_wom(st);
	if (result)
		return result;

	/* all set */
	result = inv_dataout_control1(st, st->cntl);
	if (result)
		return result;
	result = inv_dataout_control2(st, st->cntl2);
	if (result)
		return result;
	result = inv_motion_interrupt_control(st, st->motion_event_cntl);
	if (result)
		return result;

	pr_debug("setup DMP  cntl [%04X] cntl2 [%04X] motion_event [%04X]",
				st->cntl, st->cntl2, st->motion_event_cntl);

	return result;
}

static int inv_determine_engine(struct inv_mpu_state *st)
{
	int i;
	bool a_en, g_en, data_on, ped_on;
	int accel_rate, gyro_rate;
	u32 base_time;

	a_en = false;
	g_en = false;
	ped_on = false;
	data_on = false;

	st->chip_config.geomag_enable = 0;

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			data_on = true;
			a_en |= st->sensor[i].a_en;
			g_en |= st->sensor[i].g_en;
		}
	}

	st->chip_config.activity_eng_on = (st->chip_config.activity_on |
					   st->chip_config.tilt_enable);
	if (st->step_detector_l_on ||
	    st->step_detector_wake_l_on || (st->ped.on && st->batch.timeout))
		st->chip_config.step_detector_on = true;
	else
		st->chip_config.step_detector_on = false;
	if (st->chip_config.step_detector_on ||
	    st->chip_config.step_indicator_on ||
	    st->chip_config.activity_eng_on) {
		ped_on = true;
		data_on = true;
	}
	if (st->smd.on)
		ped_on = true;
	if (st->ped.on && (!st->batch.timeout))
		st->ped.int_on = 1;
	else
		st->ped.int_on = 0;

	if (st->ped.on || ped_on)
		st->ped.engine_on = true;
	else
		st->ped.engine_on = false;
	if (st->ped.engine_on)
		a_en = true;

	if (st->chip_config.pick_up_enable)
		a_en = true;

	if (st->chip_config.eis_enable) {
		g_en = true;
		gyro_rate = MPU_DEFAULT_DMP_FREQ;
		st->eis.frame_count = 0;
	} else {
		st->eis.eis_triggered = false;
	}

	if (data_on)
		st->chip_config.dmp_event_int_on = 0;
	else
		st->chip_config.dmp_event_int_on = 1;

	if (st->chip_config.dmp_event_int_on)
		st->chip_config.wom_on = 1;
	else
		st->chip_config.wom_on = 0;

	accel_rate = MPU_DEFAULT_DMP_FREQ;
	gyro_rate = MPU_DEFAULT_DMP_FREQ;

	if (g_en)
		st->ts_algo.clock_base = ENGINE_GYRO;
	else
		st->ts_algo.clock_base = ENGINE_ACCEL;

	if (st->chip_config.eis_enable) {
		st->eng_info[ENGINE_GYRO].running_rate = BASE_SAMPLE_RATE;
		st->eng_info[ENGINE_ACCEL].running_rate = BASE_SAMPLE_RATE;
		/* engine divider for pressure and compass is set later */
		st->eng_info[ENGINE_GYRO].divider = 1;
		st->eng_info[ENGINE_ACCEL].divider = 1;
	} else {
		st->eng_info[ENGINE_GYRO].running_rate = gyro_rate;
		st->eng_info[ENGINE_ACCEL].running_rate = accel_rate;
		/* engine divider for pressure and compass is set later */
		st->eng_info[ENGINE_GYRO].divider =
			(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
			(MPU_DEFAULT_DMP_FREQ /
			st->eng_info[ENGINE_GYRO].running_rate);
		st->eng_info[ENGINE_ACCEL].divider =
			(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
			(MPU_DEFAULT_DMP_FREQ /
			st->eng_info[ENGINE_ACCEL].running_rate);
	}

	for ( i = 0 ; i < SENSOR_L_NUM_MAX ; i++ )
		st->sensor_l[i].counter = 0;

	base_time = NSEC_PER_SEC;
	if (!st->batch.timeout)
		base_time += NSEC_PER_SEC / INV_ODR_OVER_FACTOR;

	st->eng_info[ENGINE_GYRO].base_time = base_time;
	st->eng_info[ENGINE_ACCEL].base_time = base_time;
	st->eng_info[ENGINE_I2C].base_time = base_time;

	inv_calc_engine_dur(&st->eng_info[ENGINE_GYRO]);
	inv_calc_engine_dur(&st->eng_info[ENGINE_ACCEL]);

	if (st->debug_determine_engine_on)
		return 0;

	pr_debug("gen: %d aen: %d grate: %d arate: %d\n",
					g_en, a_en, gyro_rate, accel_rate);

	pr_debug("to= %d inton= %d pon= %d\n", st->batch.timeout,
		st->ped.int_on, st->ped.on);

	st->chip_config.gyro_enable = g_en;
	st->gyro_cal_enable = g_en;

	st->chip_config.accel_enable = a_en;
	st->accel_cal_enable = a_en;

	st->chip_config.dmp_on = 1;

	/* setting up accuracy output */
	if (st->sensor[SENSOR_ACCEL].on || st->sensor[SENSOR_SIXQ].on)
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = true;
	else
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = false;

	if (st->sensor[SENSOR_SIXQ].on || st->sensor[SENSOR_THREEQ].on)
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = true;
	else
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = false;

	st->cntl = 0;
	st->cntl2 = 0;
	st->motion_event_cntl = 0;
	st->send_raw_gyro = false;

	return 0;
}

int inv_enable_gyro_cal(struct inv_mpu_state *st, bool en)
{
	u8 reg[3] = {0xc2, 0xc5, 0xc7};
	int result;

	if (!en) {
		reg[0] = 0xf1;
		reg[1] = 0xf1;
		reg[2] = 0xf1;
	}

	result = mem_w(CFG_EXT_GYRO_BIAS, 3, &reg[0]);

	return result;
}

int inv_enable_pedometer_interrupt(struct inv_mpu_state *st, bool en)
{
	u8 reg[3];

	if (en) {
		reg[0] = 0xf4;
		reg[1] = 0x44;
		reg[2] = 0xf1;

	} else {
		reg[0] = 0xf1;
		reg[1] = 0xf1;
		reg[2] = 0xf1;
	}

	return mem_w(CFG_PED_INT, ARRAY_SIZE(reg), reg);
}

int inv_enable_pedometer(struct inv_mpu_state *st, bool en)
{
	u8 d[1];

	if (en)
		d[0] = 0xf1;
	else
		d[0] = 0xff;

	return mem_w(CFG_PED_ENABLE, ARRAY_SIZE(d), d);
}

int inv_send_stepdet_data(struct inv_mpu_state *st, bool enable)
{
	u8 reg[3] = {0xa3, 0xa3, 0xa3};
	int result;

	/* turning off step detect jumps to STEPDET_END */
	if (!enable) {
		reg[0] = 0xf4;
		reg[1] = (STEPDET_END >> 8) & 0xff;
		reg[2] = STEPDET_END & 0xff;
	}

	result = mem_w(CFG_OUT_STEPDET, 3, &reg[0]);

	return result;
}


int inv_add_step_indicator(struct inv_mpu_state *st, bool enable)
{
	u8 reg[3] = {0xf3, 0xf3, 0xf3};
	int result;

	/* turning off step indicator jumps to PED_STEP_COUNT2_DETECTED */
	if (!enable) {
		reg[0] = 0xf4;
		reg[1] = (PED_STEP_COUNT2_DETECTED >> 8) & 0xff;
		reg[2] = PED_STEP_COUNT2_DETECTED & 0xff;
	}

	result = mem_w(CFG_PEDSTEP_DET, ARRAY_SIZE(reg), reg);

	return result;
}

int inv_enable_smd(struct inv_mpu_state *st, bool en)
{
	u8 d[1];

	if (en)
		d[0] = 1;
	else
		d[0] = 0;

	return mem_w(D_SMD_ENABLE, ARRAY_SIZE(d), d);
}


int inv_send_gyro_data(struct inv_mpu_state *st, bool enable)
{
	u8 reg[3] = {0xa3, 0xa3, 0xa3};
	int result;

	/* turning off gyro jumps to PREV_PQUAT */
	if (!enable) {
		reg[0] = 0xf4;
		reg[1] = (PREV_PQUAT >> 8) & 0xff;
		reg[2] = PREV_PQUAT & 0xff;
	}
	result = mem_w(CFG_OUT_GYRO, ARRAY_SIZE(reg), reg);

	return result;
}

static int inv_out_fsync(struct inv_mpu_state *st, bool enable)
{
	u8 reg[3] = {0xf3, 0xf3, 0xf3};
	int result;

	/* turning off fsync jumps to FSYNC_END */
	if (!enable) {
		reg[0] = 0xf4;
		reg[1] = (FSYNC_END >> 8) & 0xff;
		reg[2] = FSYNC_END & 0xff;
	}

	result = mem_w(CFG_OUT_FSYNC, ARRAY_SIZE(reg), reg);

	return result;
}

static int inv_enable_eis(struct inv_mpu_state *st, bool enable)
{
	u8 reg[2] = {0};
	int result;

	if (enable)
		reg[1] = 0x1;
	result = mem_w(D_EIS_ENABLE, ARRAY_SIZE(reg), reg);

	return result;
}

int inv_send_accel_data(struct inv_mpu_state *st, bool enable)
{
	u8 reg[3] = {0xa3, 0xa3, 0xa3};
	int result;

	/* turning off accel jumps to GYRO_FIFO_RATE */
	if (!enable) {
		reg[0] = 0xf4;
		reg[1] = (GYRO_FIFO_RATE >> 8) & 0xff;
		reg[2] = GYRO_FIFO_RATE & 0xff;
	}
	result = mem_w(CFG_OUT_ACCL, ARRAY_SIZE(reg), reg);

	return result;
}

int inv_send_six_q_data(struct inv_mpu_state *st, bool enable)
{
	u8 reg[3] = {0xa3, 0xa3, 0xa3};
	int result;

	/* turning off 6-axis jumps to PQUAT_FIFO_RATE */
	if (!enable) {
		reg[0] = 0xf4;
		reg[1] = (PQUAT_FIFO_RATE >> 8) & 0xff;
		reg[2] = PQUAT_FIFO_RATE & 0xff;
	}
	result = mem_w(CFG_OUT_6QUAT, ARRAY_SIZE(reg), reg);

	return result;
}

int inv_send_three_q_data(struct inv_mpu_state *st, bool enable)
{
        u8 reg[3] = {0xa3, 0xa3, 0xa3};
        int result;

        /* turning off LPQ jumps to QUAT6_FIFO_RATE */
        if (!enable) {
                reg[0] = 0xf4;
                reg[1] = (QUAT6_FIFO_RATE >> 8) & 0xff;
                reg[2] = QUAT6_FIFO_RATE & 0xff;
        }
        result = mem_w(CFG_OUT_3QUAT, ARRAY_SIZE(reg), reg);

        return result;
}

int inv_send_ped_q_data(struct inv_mpu_state *st, bool enable)
{
	u8 reg[3] = {0xa3, 0xa3, 0xa3};
	int result;

	/* turning off pquat jumps to ACCEL_FIFO_RATE */
	if (!enable) {
		reg[0] = 0xf4;
		reg[1] = (ACCEL_FIFO_RATE >> 8) & 0xff;
		reg[2] = ACCEL_FIFO_RATE & 0xff;
	}
	result = mem_w(CFG_OUT_PQUAT, ARRAY_SIZE(reg), reg);

	return result;
}

int inv_enable_batch(struct inv_mpu_state *st, bool on)
{
	u8 d[] = {0};
	int result;

	d[0] = on;
	result = mem_w(D_BM_ENABLE, ARRAY_SIZE(d), d);

	return result;
}


int inv_dataout_control1(struct inv_mpu_state *st, u16 cntl1)
{
	int result = 0;

	if (cntl1 & ACCEL_SET)
		result = inv_send_accel_data(st, true);
	else
		result = inv_send_accel_data(st, false);

	if (cntl1 & GYRO_SET)
		result |= inv_send_gyro_data(st, true);
	else
		result |= inv_send_gyro_data(st, false);

	if (cntl1 & QUAT6_SET)
		result |= inv_send_six_q_data(st, true);
	else
		result |= inv_send_six_q_data(st, false);

	if (cntl1 & LPQ_SET)
		result |= inv_send_three_q_data(st, true);
	else
		result |= inv_send_three_q_data(st, false);

	if (cntl1 & PQUAT6_SET)
		result |= inv_send_ped_q_data(st, true);
	else
		result |= inv_send_ped_q_data(st, false);

	if ((cntl1 & PED_STEPDET_SET)) {
		result |= inv_enable_pedometer(st, true);
		result |= inv_enable_pedometer_interrupt(st, true);
		result |= inv_send_stepdet_data(st, true);
	} else {
		result |= inv_enable_pedometer(st, false);
		result |= inv_enable_pedometer_interrupt(st, false);
		result |= inv_send_stepdet_data(st, false);
	}

	return result;
}

int inv_dataout_control2(struct inv_mpu_state *st, u16 cntl2)
{
	int result = 0;
	bool en;

	if (cntl2 & BATCH_MODE_EN)
		en = true;
	else
		en = false;
	result = inv_enable_batch(st, en);
	if (result)
		return result;

	if (cntl2 & FSYNC_SET)
		en = true;
	else
		en = false;
	result = inv_enable_eis(st, en);
	if (result)
		return result;

	result = inv_out_fsync(st, en);
	if (result)
		return result;

	return result;
}

int inv_motion_interrupt_control(struct inv_mpu_state *st,
						u16 motion_event_cntl)
{
	int result = 0;

	if (motion_event_cntl & PEDOMETER_EN)
		result = inv_enable_pedometer(st, true);
	else
		result = inv_enable_pedometer(st, false);

	if (motion_event_cntl & PEDOMETER_INT_EN)
		result = inv_enable_pedometer_interrupt(st, true);
	else
		result = inv_enable_pedometer_interrupt(st, false);

	if (motion_event_cntl & SMD_EN)
		result = inv_enable_smd(st, true);
	else
		result = inv_enable_smd(st, false);

	if (motion_event_cntl & GYRO_CAL_EN)
		result = inv_enable_gyro_cal(st, true);
	else
		result = inv_enable_gyro_cal(st, false);

	return result;
}

/*
 *  set_inv_enable() - enable function.
 */
int set_inv_enable(struct iio_dev *indio_dev)
{
	int result;
	struct inv_mpu_state *st = iio_priv(indio_dev);

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = inv_stop_dmp(st);
	if (result)
		return result;
	inv_determine_engine(st);
	result = inv_set_rate(st);
	if (result) {
		pr_err("inv_set_rate error\n");
		return result;
	}
	result = inv_setup_dmp(st);
	if (result) {
		pr_err("setup dmp error\n");
		return result;
	}
	result = inv_turn_on_engine(st);
	if (result) {
		pr_err("inv_turn_on_engine error\n");
		return result;
	}
	result = inv_set_fifo_size(st);
	if (result) {
		pr_err("inv_set_fifo_size error\n");
		return result;
	}
	result = inv_reset_fifo(st, false);
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);
	if ((!st->chip_config.gyro_enable) && (!st->chip_config.accel_enable)) {
		inv_set_power(st, false);
		return 0;
	}
	return result;
}

int inv_setup_dmp_firmware(struct inv_mpu_state *st)
{
	int result = 0;

	result = inv_dataout_control1(st, 0);
	if (result)
		return result;
	result = inv_dataout_control2(st, 0);
	if (result)
		return result;
	result = inv_motion_interrupt_control(st, 0);


	return result;
}
