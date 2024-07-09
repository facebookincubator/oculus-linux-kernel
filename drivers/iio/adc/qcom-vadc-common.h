/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Code shared between the different Qualcomm PMIC voltage ADCs
 */

#ifndef QCOM_VADC_COMMON_H
#define QCOM_VADC_COMMON_H

#define VADC_CONV_TIME_MIN_US			2000
#define VADC_CONV_TIME_MAX_US			2100

/* Min ADC code represents 0V */
#define VADC_MIN_ADC_CODE			0x6000
/* Max ADC code represents full-scale range of 1.8V */
#define VADC_MAX_ADC_CODE			0xa800

#define VADC_ABSOLUTE_RANGE_UV			625000
#define VADC_RATIOMETRIC_RANGE			1800

#define VADC_DEF_PRESCALING			0 /* 1:1 */
#define VADC_DEF_DECIMATION			0 /* 512 */
#define VADC_DEF_HW_SETTLE_TIME			0 /* 0 us */
#define VADC_DEF_AVG_SAMPLES			0 /* 1 sample */
#define VADC_DEF_CALIB_TYPE			VADC_CALIB_ABSOLUTE
#define VADC_DEF_VBAT_PRESCALING		1 /* 1:3 */

#define VADC_DEF_LUT_INDEX			0 /* Default or no LUT used */

#define VADC_DECIMATION_MIN			512
#define VADC_DECIMATION_MAX			4096
#define ADC5_DECIMATION_SHORT			250
#define ADC5_DECIMATION_MEDIUM			420
#define ADC5_DECIMATION_LONG			840
/* Default decimation - 1024 for rev2, 840 for pmic5 */
#define ADC_DECIMATION_DEFAULT			2
#define ADC_DECIMATION_SAMPLES_MAX		3

#define VADC_HW_SETTLE_DELAY_MAX		10000
#define VADC_HW_SETTLE_SAMPLES_MAX		16
#define VADC_AVG_SAMPLES_MAX			512
#define ADC5_AVG_SAMPLES_MAX			16

#define KELVINMIL_CELSIUSMIL			273150
#define PMIC5_CHG_TEMP_SCALE_FACTOR		377500
#define PMIC5_SMB_TEMP_CONSTANT			419400
#define PMIC5_SMB_TEMP_SCALE_FACTOR		356
#define PMIC5_SMB1398_TEMP_SCALE_FACTOR	340
#define PMIC5_SMB1398_TEMP_CONSTANT		268235

#define PMIC5_PM2250_S3_DIE_TEMP_SCALE_FACTOR	187263
#define PMIC5_PM2250_S3_DIE_TEMP_CONSTANT		720100

#define PMI_CHG_SCALE_1				-138890
#define PMI_CHG_SCALE_2				391750000000LL

#define VADC5_MAX_CODE				0x7fff
#define VADC5_FULL_SCALE_CODE			0x70e4
#define ADC_USR_DATA_CHECK			0x8000
#define ADC_HC_VDD_REF			1875000

#define IPC_LOGPAGES 10

#ifdef CONFIG_DEBUG_FS
#define ADC_IPC(idx, dev, msg, args...) do { \
		if (dev) { \
			if ((idx == 0) && (dev)->ipc_log0) \
				ipc_log_string((dev)->ipc_log0, \
					"%s: " msg, __func__, args); \
			else if ((idx == 1) && (dev)->ipc_log1) \
				ipc_log_string((dev)->ipc_log1, \
					"%s: " msg, __func__, args); \
			else \
				pr_debug("adc: invalid logging index\n"); \
		} \
	} while (0)
#define ADC_DBG(dev, msg, args...) do {				\
		ADC_IPC(0, dev, msg, args); \
		pr_debug(msg, ##args);	\
	} while (0)
#define ADC_DBG1(dev, msg, args...) do {				\
		ADC_IPC(1, dev, msg, args); \
		pr_debug(msg, ##args);	\
	} while (0)
#else
#define	ADC_DBG(dev, msg, args...)		pr_debug(msg, ##args)
#define	ADC_DBG1(dev, msg, args...)		pr_debug(msg, ##args)
#endif

#define R_PU_100K			100000
#define RATIO_MAX_ADC7		0x4000

#define DIE_TEMP_ADC7_SCALE_1				-60000
#define DIE_TEMP_ADC7_SCALE_2				20000
#define DIE_TEMP_ADC7_SCALE_FACTOR			1000
#define DIE_TEMP_ADC7_MAX				160000

/**
 * struct vadc_map_pt - Map the graph representation for ADC channel
 * @x: Represent the ADC digitized code.
 * @y: Represent the physical data which can be temperature, voltage,
 *     resistance.
 */
struct vadc_map_pt {
	s32 x;
	s32 y;
};

/*
 * VADC_CALIB_ABSOLUTE: uses the 625mV and 1.25V as reference channels.
 * VADC_CALIB_RATIOMETRIC: uses the reference voltage (1.8V) and GND for
 * calibration.
 */
enum vadc_calibration {
	VADC_CALIB_ABSOLUTE = 0,
	VADC_CALIB_RATIOMETRIC
};

/**
 * struct vadc_linear_graph - Represent ADC characteristics.
 * @dy: numerator slope to calculate the gain.
 * @dx: denominator slope to calculate the gain.
 * @gnd: A/D word of the ground reference used for the channel.
 *
 * Each ADC device has different offset and gain parameters which are
 * computed to calibrate the device.
 */
struct vadc_linear_graph {
	s32 dy;
	s32 dx;
	s32 gnd;
};

/**
 * struct vadc_prescale_ratio - Represent scaling ratio for ADC input.
 * @num: the inverse numerator of the gain applied to the input channel.
 * @den: the inverse denominator of the gain applied to the input channel.
 */
struct vadc_prescale_ratio {
	u32 num;
	u32 den;
};

/**
 * enum vadc_scale_fn_type - Scaling function to convert ADC code to
 *				physical scaled units for the channel.
 * SCALE_DEFAULT: Default scaling to convert raw adc code to voltage (uV).
 * SCALE_THERM_100K_PULLUP: Returns temperature in millidegC.
 *				 Uses a mapping table with 100K pullup.
 * SCALE_PMIC_THERM: Returns result in milli degree's Centigrade.
 * SCALE_XOTHERM: Returns XO thermistor voltage in millidegC.
 * SCALE_PMI_CHG_TEMP: Conversion for PMI CHG temp
 * SCALE_HW_CALIB_DEFAULT: Default scaling to convert raw adc code to
 *	voltage (uV) with hardware applied offset/slope values to adc code.
 * SCALE_HW_CALIB_THERM_100K_PULLUP: Returns temperature in millidegC using
 *	lookup table. The hardware applies offset/slope to adc code.
 * SCALE_HW_CALIB_XOTHERM: Returns XO thermistor voltage in millidegC using
 *	100k pullup. The hardware applies offset/slope to adc code.
 * SCALE_HW_CALIB_THERM_100K_PU_PM7: Returns temperature in millidegC using
 *	lookup table for PMIC7. The hardware applies offset/slope to adc code.
 * SCALE_HW_CALIB_PMIC_THERM: Returns result in milli degree's Centigrade.
 *	The hardware applies offset/slope to adc code.
 * SCALE_HW_CALIB_CUR: Returns result in uA for PMIC5.
 * SCALE_HW_CALIB_PMIC_THERM: Returns result in milli degree's Centigrade.
 *	The hardware applies offset/slope to adc code. This is for PMIC7.
 * SCALE_HW_CALIB_PM5_CHG_TEMP: Returns result in millidegrees for PMIC5
 *	charger temperature.
 * SCALE_HW_CALIB_PM5_SMB_TEMP: Returns result in millidegrees for PMIC5
 *	SMB1390 temperature.
 * SCALE_HW_CALIB_BATT_THERM_100K: Returns battery thermistor voltage in
 *	decidegC using 100k pullup. The hardware applies offset/slope to adc
 *	code.
 * SCALE_HW_CALIB_BATT_THERM_30K: Returns battery thermistor voltage in
 *	decidegC using 30k pullup. The hardware applies offset/slope to adc
 *	code.
 * SCALE_HW_CALIB_BATT_THERM_400K: Returns battery thermistor voltage in
 *	decidegC using 400k pullup. The hardware applies offset/slope to adc
 *	code.
 * SCALE_HW_CALIB_PM2250_S3_DIE_TEMP: Returns result in millidegrees for
 *	S3 die temperature channel on PM2250.
 */
enum vadc_scale_fn_type {
	SCALE_DEFAULT = 0,
	SCALE_THERM_100K_PULLUP,
	SCALE_PMIC_THERM,
	SCALE_XOTHERM,
	SCALE_PMI_CHG_TEMP,
	SCALE_HW_CALIB_DEFAULT,
	SCALE_HW_CALIB_THERM_100K_PULLUP,
	SCALE_HW_CALIB_XOTHERM,
	SCALE_HW_CALIB_PMIC_THERM,
	SCALE_HW_CALIB_CUR,
	SCALE_HW_CALIB_PM5_CHG_TEMP,
	SCALE_HW_CALIB_PM5_SMB_TEMP,
	SCALE_HW_CALIB_BATT_THERM_100K,
	SCALE_HW_CALIB_BATT_THERM_30K,
	SCALE_HW_CALIB_BATT_THERM_400K,
	SCALE_HW_CALIB_PM5_SMB1398_TEMP,
	SCALE_HW_CALIB_PM2250_S3_DIE_TEMP,
	SCALE_HW_CALIB_THERM_100K_PU_PM7,
	SCALE_HW_CALIB_PMIC_THERM_PM7,
	SCALE_BATT_THERM_QRD_215,
	SCALE_HW_CALIB_MAX,
};

struct adc_data {
	const u32	full_scale_code_volt;
	const u32	full_scale_code_cur;
	const struct adc_channels *adc_chans;
	unsigned int	*decimation;
	unsigned int	*hw_settle;
};

int qcom_vadc_scale(enum vadc_scale_fn_type scaletype,
		    const struct vadc_linear_graph *calib_graph,
		    const struct vadc_prescale_ratio *prescale,
		    bool absolute,
		    u16 adc_code, int *result_mdec);

int qcom_vadc_hw_scale(enum vadc_scale_fn_type scaletype,
		    const struct vadc_prescale_ratio *prescale,
		    const struct adc_data *data, unsigned int lut_index,
		    u16 adc_code, int *result_mdec);

int qcom_vadc_decimation_from_dt(u32 value);

int qcom_adc5_decimation_from_dt(u32 value, const unsigned int *decimation);

#endif /* QCOM_VADC_COMMON_H */
