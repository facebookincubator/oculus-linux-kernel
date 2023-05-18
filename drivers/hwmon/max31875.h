

/*******************************************************************************
 * @file max31875.h
 *
 * @brief Driver for the temperature sensor MAX31875
 *
 * @details
 *
  *******************************************************************************
 * Copyright (C) 2019 Facebook, Inc. and its affilates. All Rights reserved.
 ******************************************************************************/
#ifndef _MAX31875_H_
#define _MAX31875_H_


/*******************************************************************************
 * Constants Definition
 ******************************************************************************/
#define MAX31875_REG_TEMPERATURE    0x00
#define MAX31875_REG_CONFIGURATION  0x01
#define MAX31875_REG_THYST_LOW_TRIP 0x02
#define MAX31875_REG_TOS_HIGH_TRIP  0x03
#define MAX31875_REG_MAX            0x03

#define MAX31875_CFG_CONV_RATE_MASK (0x03 << 1)
#define MAX31875_CFG_CONV_RATE_0_25 (0x00 << 1)     /* 0.25 conversions/sec */
#define MAX31875_CFG_CONV_RATE_1    (0x01 << 1)     /* 1.0 conversions/sec */
#define MAX31875_CFG_CONV_RATE_4    (0x02 << 1)     /* 4.0 conversions/sec */
#define MAX31875_CFG_CONV_RATE_8    (0x03 << 1)     /* 8.0 conversions/sec */

#define MAX31875_CFG_RESOLUTION_MASK  (0x03 << 5)
#define MAX31875_CFG_RESOLUTION_8BIT  (0x00 << 5)
#define MAX31875_CFG_RESOLUTION_9BIT  (0x01 << 5)
#define MAX31875_CFG_RESOLUTION_10BIT (0x02 << 5)
#define MAX31875_CFG_RESOLUTION_12BIT (0x03 << 5)

#define MAX31875_CFG_FORMAT_MASK     (0X01 << 7)
#define MAX31875_CFG_FORMAT_NORMAL   (0X00 << 7)
#define MAX31875_CFG_FORMAT_EXTENDED (0X01 << 7)

#define MAX31875_CFG_OPMODE_MASK        (0X01 << 8)
#define MAX31875_CFG_OPMODE_CONTINUOUS  (0X00 << 8)
#define MAX31875_CFG_OPMODE_SHUTDOWN    (0X01 << 8)

#define MAX31875_CFG_COMPMODE_MASK          (0X01 << 9)
#define MAX31875_CFG_COMPMODE_COMPARATOR    (0x00 << 9)
#define MAX31875_CFG_COMPMODE_INTERRUPT     (0X01 << 9)

#define MAX31875_CFG_OVERTEMP_MASK      (0x01 << 15)
#define MAX31875_CFG_OVERTEMP_ALARM     (0x01 << 15)
#define MAX31875_CFG_OVERTEMP_NOALARM   (0x00 << 15)

#define CONFIG_MAX31875_DEFAULT_FORMAT      (MAX31875_CFG_FORMAT_NORMAL)
#define CONFIG_MAX31875_DEFAULT_RESOLUTION  (MAX31875_CFG_RESOLUTION_10BIT)
#define CONFIG_MAX31875_DEFAULT_RATE        (MAX31875_CFG_CONV_RATE_1)
#define CONFIG_MAX31875_DEFAULT_COMPMODE    (MAX31875_CFG_COMPMODE_COMPARATOR)
#define CONFIG_MAX31875_DEFAULT_OPMODE      (MAX31875_CFG_OPMODE_CONTINUOUS)


#define MAX31875_CFG_DEFAULT                \
    (CONFIG_MAX31875_DEFAULT_FORMAT |       \
     CONFIG_MAX31875_DEFAULT_RESOLUTION |   \
     CONFIG_MAX31875_DEFAULT_RATE |         \
     CONFIG_MAX31875_DEFAULT_COMPMODE |     \
     CONFIG_MAX31875_DEFAULT_OPMODE)

#define MAX31875_CF_NORMAL_FORMAT   (390625)
#define MAX31875_CF_EXTENDED_FORMAT (781250)

#define MAX31875_TEMP_MIN_MC    -50000  /* Minimum millicelsius. */
#define MAX31875_TEMP_MAX_MC    127937  /* Maximum millicelsius. */

#define MAX31875_CONVERSION_TIME_MS     35     /* in milli-seconds */
#define MAX31875_CONVERSION_TIME_FT_MS 180     /* in milli-seconds; first time conversion */

#endif // _MAX31875_H_
