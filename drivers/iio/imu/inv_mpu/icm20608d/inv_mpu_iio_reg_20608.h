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

#ifndef _INV_MPU_IIO_REG_20608_H_
#define _INV_MPU_IIO_REG_20608_H_

/*register and associated bit definition*/
#define REG_XA_OFFS_H		0x77
#define REG_YA_OFFS_H		0x7A
#define REG_ZA_OFFS_H           0x7D
#define REG_XA_OFFS_L_TC        0x7
#define REG_PRODUCT_ID          0xC
#define REG_ST_GCT_X            0xD
#define REG_XG_OFFS_USR_H        0x13
#define REG_YG_OFFS_USR_H        0x15
#define REG_ZG_OFFS_USR_H        0x17
#define REG_SAMPLE_RATE_DIV     0x19

#define REG_CONFIG              0x1A
#define EXT_SYNC_SET                      8

#define REG_GYRO_CONFIG		0x1B
#define BITS_SELF_TEST_EN		0xE0
#define SHIFT_GYRO_FS_SEL		0x03

#define REG_ACCEL_CONFIG		0x1C
#define SHIFT_ACCEL_FS			0x03
#define REG_ACCEL_MOT_THR       0x1F
#define REG_ACCEL_MOT_DUR       0x20

#define REG_ACCEL_CONFIG_2  0x1D
#define BIT_ACCEL_FCHOCIE_B              0x08

#define REG_FIFO_EN		0x23
#define BITS_TEMP_FIFO_EN                   0x80
#define BITS_GYRO_FIFO_EN		    0x70
#define BIT_ACCEL_FIFO_EN	            0x08
#define BIT_SLV_0_FIFO_EN                   0x01

#define REG_I2C_SLV0_ADDR       0x25
#define REG_I2C_SLV0_REG        0x26
#define REG_I2C_SLV0_CTRL       0x27
#define REG_I2C_SLV1_ADDR       0x28
#define REG_I2C_SLV1_REG        0x29
#define REG_I2C_SLV1_CTRL       0x2A
#define REG_I2C_SLV2_ADDR       0x2B
#define REG_I2C_SLV2_REG        0x2C
#define REG_I2C_SLV2_CTRL       0x2D

#define REG_I2C_SLV4_CTRL       0x34

#define REG_FSYNC_INT		0x36
#define BIT_FSYNC_INT                   0x80

#define REG_INT_PIN_CFG		0x37
#define BIT_BYPASS_EN		0x2

#define REG_INT_ENABLE		0x38
#define BIT_WOM_INT_EN		        0xe0
#define BIT_FIFO_OFLOW_EN	        0x10
#define BIT_FSYNC_INT_EN	        0x8
#define BIT_DMP_INT_EN		        0x02
#define BIT_DATA_RDY_EN		        0x01

#define REG_DMP_INT_STATUS      0x39
#define BIT_DMP_INT_CI          0x01

#define REG_INT_STATUS          0x3A
#define BIT_WOM_INT                    0xE0

#define REG_RAW_ACCEL           0x3B
#define REG_EXT_SLV_SENS_DATA_00         0x49

#define REG_ACCEL_INTEL_STATUS  0x61

#define REG_I2C_SLV0_DO          0x63
#define REG_I2C_SLV1_DO          0x64
#define REG_I2C_SLV2_DO          0x65

#define REG_I2C_MST_DELAY_CTRL   0x67
#define BIT_I2C_SLV1_DELAY_EN                   0x02
#define BIT_I2C_SLV0_DELAY_EN                   0x01

#define REG_USER_CTRL            0x6A
#define BIT_COND_RST				0x01
#define BIT_FIFO_RST				0x04
#define BIT_DMP_RST				0x08
#define BIT_I2C_IF_DIS				0x10
#define BIT_I2C_MST_EN                          0x20
#define BIT_FIFO_EN				0x40
#define BIT_DMP_EN				0x80

#define REG_PWR_MGMT_1	         0x6B
#define BIT_H_RESET				0x80
#define BIT_SLEEP				0x40
#define BIT_LP_EN                       	0x20
#define BIT_CLK_PLL				0x01
#define BIT_CLK_MASK				0x07

#define REG_PWR_MGMT_2			0x6C
#define BIT_PWR_ACCEL_STBY		0x38
#define BIT_PWR_GYRO_STBY		0x07
#define BIT_PWR_ALL_OFF			0x3F
#define BIT_FIFO_LP_EN			0x80
#define BIT_DMP_LP_DIS			0x40

#define REG_MEM_BANK_SEL		0x6D
#define REG_MEM_START_ADDR	0x6E
#define REG_MEM_R_W				0x6F
#define REG_PRGM_START_ADDRH	0x70

#define REG_FIFO_COUNT_H        0x72
#define REG_FIFO_R_W            0x74
#define REG_WHO_AM_I              0x75

#define REG_6500_XG_ST_DATA     0x0
#define REG_6500_XA_ST_DATA     0xD
#define REG_6500_XA_OFFS_H      0x77
#define REG_6500_YA_OFFS_H      0x7A
#define REG_6500_ZA_OFFS_H      0x7D
#define REG_6500_ACCEL_CONFIG2  0x1D
#define BIT_ACCEL_FCHOCIE_B              0x08
#define BIT_FIFO_SIZE_1K                 0x40

#define REG_LP_MODE_CFG		0x1E
#define BIT_GYRO_CYCLE			0x80
#define BIT_G_AVGCFG_MASK		0x70
#define BIT_LPOSC_CLKSEL_MASK	0x0F

#define REG_6500_LP_ACCEL_ODR   0x1E
#define REG_6500_ACCEL_WOM_THR  0x1F

#define REG_SELF_TEST1                0x00
#define REG_SELF_TEST2                0x01
#define REG_SELF_TEST3                0x02
#define REG_SELF_TEST4                0x0D
#define REG_SELF_TEST5                0x0E
#define REG_SELF_TEST6                0x0F

#define INV_MPU_BIT_SLV_EN      0x80
#define INV_MPU_BIT_BYTE_SW     0x40
#define INV_MPU_BIT_REG_DIS     0x20
#define INV_MPU_BIT_GRP         0x10
#define INV_MPU_BIT_I2C_READ    0x80

/* data output control reg 2 */
#define ACCEL_ACCURACY_SET  0x4000
#define GYRO_ACCURACY_SET   0x2000
#define CPASS_ACCURACY_SET  0x1000
#define FSYNC_SET           0x0800
#define FLIP_PICKUP_SET     0x0400
#define BATCH_MODE_EN       0x0100
#define ACT_RECOG_SET       0x0080
#define SECOND_SEN_OFF_SET  0x0040

#define INV_FSYNC_TEMP_BIT     0x1

#define ACCEL_COVARIANCE 0

/* dummy definitions */
#define BANK_SEL_0                      0x00
#define BANK_SEL_1                      0x10
#define BANK_SEL_2                      0x20
#define BANK_SEL_3                      0x30

/* data definitions */
#define BYTES_PER_SENSOR         6
#define BYTES_FOR_COMPASS        10
#define FIFO_COUNT_BYTE          2
#define HARDWARE_FIFO_SIZE       512
#define FIFO_SIZE                (HARDWARE_FIFO_SIZE * 7 / 8)
#define POWER_UP_TIME            100
#define REG_UP_TIME_USEC         100
#define DMP_RESET_TIME           20
#define GYRO_ENGINE_UP_TIME      50
#define MPU_MEM_BANK_SIZE        256
#define IIO_BUFFER_BYTES         8
#define HEADERED_NORMAL_BYTES    8
#define HEADERED_Q_BYTES         16
#define LEFT_OVER_BYTES          128
#define BASE_SAMPLE_RATE         1000
#define DRY_RUN_TIME             50
#define INV_ICM20608_GYRO_START_TIME 80
#define INV_ICM20608_ACCEL_START_TIME 10

#ifdef BIAS_CONFIDENCE_HIGH
#define DEFAULT_ACCURACY         3
#else
#define DEFAULT_ACCURACY         1
#endif

/* enum for sensor
   The sequence is important.
   It represents the order of apperance from DMP */
enum INV_SENSORS {
	SENSOR_ACCEL = 0,
	SENSOR_TEMP,
	SENSOR_GYRO,
	SENSOR_COMPASS,
	SENSOR_SIXQ,
	SENSOR_THREEQ,
	SENSOR_PEDQ,
	SENSOR_NUM_MAX,
	SENSOR_INVALID,
};

enum inv_filter_e {
	INV_FILTER_256HZ_NOLPF2 = 0,
	INV_FILTER_188HZ,
	INV_FILTER_98HZ,
	INV_FILTER_42HZ,
	INV_FILTER_20HZ,
	INV_FILTER_10HZ,
	INV_FILTER_5HZ,
	INV_FILTER_2100HZ_NOLPF,
	NUM_FILTER
};

#define MPU_DEFAULT_DMP_FREQ     200
#define PEDOMETER_FREQ           (MPU_DEFAULT_DMP_FREQ >> 2)

#define DMP_OFFSET               0x20
#ifndef INV_IPL
#define DMP_IMAGE_SIZE_20608D         (3535 + DMP_OFFSET)
#else
#define DMP_IMAGE_SIZE_20608D         (3532 + DMP_OFFSET)
#endif

/* initial rate is important. For DMP mode, it is set as 1 since DMP decimate*/
#define MPU_INIT_SENSOR_RATE     1
#define MIN_MST_ODR_CONFIG       4
#define MAX_MST_ODR_CONFIG       5
#define MIN_COMPASS_RATE         4
#define MAX_COMPASS_RATE    100
#define MAX_MST_NON_COMPASS_ODR_CONFIG 7
#define THREE_AXES               3
#define NINE_ELEM                (THREE_AXES * THREE_AXES)
#define MPU_TEMP_SHIFT           16
#define DMP_DIVIDER              (BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ)
#define MAX_5_BIT_VALUE          0x1F
#define BAD_COMPASS_DATA         0x7FFF
#define BAD_CAL_COMPASS_DATA     0x7FFF0000
#define DEFAULT_BATCH_RATE       400
#define DEFAULT_BATCH_TIME    (MSEC_PER_SEC / DEFAULT_BATCH_RATE)
#define NINEQ_DEFAULT_COMPASS_RATE 25

#define DATA_AKM_99_BYTES_DMP  10
#define DATA_AKM_89_BYTES_DMP  9
#define DATA_ALS_BYTES_DMP     8
#define APDS9900_AILTL_REG      0x04
#define BMP280_DIG_T1_LSB_REG                0x88
#define TEMPERATURE_SCALE  3340827L
#define TEMPERATURE_OFFSET 1376256L
#define SECONDARY_INIT_WAIT 100
#define MPU_SOFT_REV_ADDR               0x86
#define MPU_SOFT_REV_MASK               0xf
#define SW_REV_LP_EN_MODE               4
#define AK99XX_SHIFT                    23
#define AK89XX_SHIFT                    22

/* this is derived from 1000 divided by 55, which is the pedometer
   running frequency */
#define MS_PER_PED_TICKS         18

/* data limit definitions */
#define MIN_FIFO_RATE            4
#define MAX_FIFO_RATE            MPU_DEFAULT_DMP_FREQ
#define MAX_DMP_OUTPUT_RATE      MPU_DEFAULT_DMP_FREQ

#define MAX_MPU_MEM              8192
#define MAX_PRS_RATE             281

enum inv_devices {
	ICM20608D,
	ICM20690,
	ICM20602,
	INV_NUM_PARTS,
};
#endif
