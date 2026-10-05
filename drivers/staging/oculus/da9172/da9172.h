// SPDX-License-Identifier: GPL-2.0+

#ifndef __DA9172_I2C_DRIVER_H__
#define __DA9172_I2C_DRIVER_H__

// PMIC read/write control
#define DA9172_MAX_READ_RETRIES 6
#define DA9172_MAX_WRITE_RETRIES 6

// I2C settings and prototypes
#define MIN_DELAY_US 100000
#define MAX_DELAY_US 200000
#define WRITE_DELAY_MS 10
#define I2C_DELAY_US 500

#define DA9172_REG_VIRT_STATUS 0x1080
#define DA9172_REG_MASK_REV 0x1100

#define DA9172_REG_MFSM_PWR_STATE_STATUS 0x1502
#define DA9172_REG_MFSM_PWR_STATE_TARGET 0x1503

#define DA9172_REG_GPADC_EN 0x1B00
#define DA9172_REG_GPADC_CTRL 0x1B01
#define DA9172_REG_GPADC_RES 0x1B02

// PWR_SEQ block (0x2700..0x2714)
#define DA9172_REG_PWRONOFF_BUCK_EN 0x2700
#define DA9172_REG_PWRONOFF_LDO_EN 0x2701
#define DA9172_REG_PWRONOFF_VNEG1_EN 0x2702
#define DA9172_REG_PWRONOFF_VNEG2_EN 0x2703
#define DA9172_REG_PWRONOFF_VNEG3_EN 0x2704
#define DA9172_REG_PWRONOFF_TPORB_EN 0x2705
#define DA9172_REG_PWRONOFF_SLOT_TIMINGS_PU_BASE 0x2707  // 4 bytes, 2 slots/byte
#define DA9172_REG_PWRONOFF_SLOT_TIMINGS_PD_BASE 0x270B  // 4 bytes, 2 slots/byte
#define DA9172_REG_PWRONOFF_BUCK_SLOT 0x270F
#define DA9172_REG_PWRONOFF_LDO_SLOT 0x2710
#define DA9172_REG_PWRONOFF_VNEG1_SLOT 0x2711
#define DA9172_REG_PWRONOFF_VNEG2_SLOT 0x2712
#define DA9172_REG_PWRONOFF_VNEG3_SLOT 0x2713
#define DA9172_REG_PWRONOFF_TPORB_SLOT 0x2714

#define DA9172_PWRONOFF_EN_READY BIT(0)
#define DA9172_PWRONOFF_EN_ACTIVE BIT(1)
#define DA9172_PWRONOFF_UP_SLOT_MASK GENMASK(2, 0)
#define DA9172_PWRONOFF_DN_SLOT_MASK GENMASK(6, 4)

// VNEGx control / VOUT (n = 0..2 for VNEG1..VNEG3)
#define DA9172_REG_VNEG_BASE(n) (0x3000 + (n) * 0x100)
#define DA9172_REG_VNEG_VOUT(n) (DA9172_REG_VNEG_BASE(n) + 0x09)
#define DA9172_REG_VNEG_VMAX(n) (DA9172_REG_VNEG_BASE(n) + 0x0A)
#define DA9172_REG_VNEG_VMIN(n) (DA9172_REG_VNEG_BASE(n) + 0x0B)
#define DA9172_VNEG_STEP_UV 25000  // V = -25 mV * code

// BUCK VOUT (10 mV positive step). LDO has fixed 1.8 V output, no register.
#define DA9172_REG_BUCK_VOUT_VSEL 0x170C
#define DA9172_REG_BUCK_MINV 0x170E
#define DA9172_REG_BUCK_MAXV 0x170F
#define DA9172_BUCK_STEP_UV 10000  // V = +10 mV * code

// Lock register (highest configurable address)
#define DA9172_REG_LOCK_VOUT_LOCK_CTRL 0xFFFD
#define DA9172_VOUT_LOCK_BIT BIT(0)

struct da9172_data {
  struct i2c_client* i2c;
  struct device* dev;
  struct i2c_adapter* adapter;
  unsigned short addr;
  bool is_enabled;
  u8 pmic_rev;
  u8 pmic_err;
  struct gpio_desc* disp_en_gpio;
  struct gpio_desc* pmic_en_gpio;
  struct regmap* regmap;
  struct mutex gpadc_lock;
  struct thermal_zone_device* tz;
  u32 pwr_state_timeout_us;
  /*
   * Cached DT VNEG VOUT overrides (see CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE),
   * parsed once in probe() and reapplied on each power-on. vneg_vout_uv[n] is
   * valid only when vneg_override[n] is set.
   */
  s32 vneg_vout_uv[3];
  bool vneg_override[3];
  /*
   * Bridging reference on vin, taken under continuous splash and handed back on
   * the first disable. Deferred to a work item: regulator ops run under the
   * core's ww_mutex.
   */
  struct regulator* vin;
  bool vin_ref_held;
  struct work_struct vin_release_work;
};

int da9172_i2c_write(struct da9172_data* pmic, u16 reg, void* buffer, unsigned int len);
int da9172_i2c_read(struct da9172_data* pmic, u16 reg, void* buffer, unsigned int len);

#endif // __DA9172_I2C_DRIVER_H__
