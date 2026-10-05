// SPDX-License-Identifier: GPL-2.0+
//
// da9172.c  - regulator driver for DA9172 PMIC
//
// Copyright (c) Meta Platforms, Inc. and affiliates.
//

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>

#include "da9172.h"

/*
 * According to the datasheet, the equation to convert from adc to current[A]:
 * I[A] = (result/4095 - 0.0977)/0.178
 * To avoid using floats, we convert to mA by multiplying by 1000
 */
#define ADC_BYTES_TO_MA(adc_bytes) (((adc_bytes) * 10000 / 4095 - 977) * 1000 / 1780)

/*
 * According to the datasheet (section 6.10.1.1), we can determine the
 * AVDD and VBUCK voltages from ADC readings using the following formula:
 * V [in Volts] = 5.11875 * (adc_result / 4095)
 * The macro below converts the ADC reading to mV by multiplying the
 * equation by 1000.
 */
#define ADC_BYTES_TO_MV(adc_bytes) (((adc_bytes) * 5119) / 4095)

/*
 * According to the datasheet (section 6.10.1.4), we can determine the
 * temperature from ADC reading using the following formula:
 * T [in deg C] = 348.27 - 506.81 * (adc_result / 4095)
 * The macro below converts the ADC reading to mDegC by multiplying the
 * equation by 1000.
 */
#define ADC_BYTES_TO_MDEGC(adc_bytes) (348270 - (adc_bytes) * 506810 / 4095)

#define GPADC_MAX_RETRIES 5
#define GPADC_REPEAT_COUNT 10

#define MFSM_PWR_STATE_TARGET_STATE_MASK GENMASK(1, 0)

#define MFSM_PWR_STATE_STATUS_TRANS BIT(7)
#define MFSM_PWR_STATE_STATUS_CUR_STATE_MASK GENMASK(1, 0)
#define MFSM_PWR_STATE_STATUS_PREV_STATE_MASK GENMASK(4, 3)

#define DA9172_PWR_STATE_POLL_INTERVAL_US 10000
#define DA9172_PWR_STATE_POLL_TIMEOUT_US 250000

enum da9172_gpadc_channel {
  DA9172_GPADC_CH_REV = 0,
  DA9172_GPADC_CH_AVDD = 1,
  DA9172_GPADC_CH_VBUCK = 2,
  DA9172_GPADC_CH_VNEG1_A = 3,
  DA9172_GPADC_CH_VNEG1_B = 4,
  DA9172_GPADC_CH_VNEG2_A = 5,
  DA9172_GPADC_CH_VNEG2_B = 6,
  DA9172_GPADC_CH_VNEG3_A = 7,
  DA9172_GPADC_CH_VNEG3_B = 8,
  DA9172_GPADC_CH_IBUCK = 9,
  DA9172_GPADC_CH_VTJUNC = 10
};

enum da9172_power_state {
  DA9172_PWR_STATE_PRE_BOOT = 0,
  DA9172_PWR_STATE_READY = 1,
  DA9172_PWR_STATE_ACTIVE = 2,
  DA9172_PWR_STATE_UNUSED = 3,
};

static const struct regmap_config da9172_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .val_format_endian = REGMAP_ENDIAN_BIG,
    .max_register = DA9172_REG_LOCK_VOUT_LOCK_CTRL,
    .cache_type = REGCACHE_NONE,
};

#ifdef CONFIG_DA9172_PWRSEQ_DEBUG
/* Sequencer slot timer encoding (Table 21). Index = 4-bit T_SLOT_PUx/PDx. */
static const u32 da9172_slot_delay_us[16] = {
    0,    32,   64,    128,   320,   640,   1280,  2560,
    0,    500,  1000,  2000,  5000,  10000, 20000, 50000,
};

struct da9172_rail_info {
  const char* name;
  u16 en_reg;     // PWRONOFF_<rail>_EN
  u16 slot_reg;   // PWRONOFF_<rail>_SLOT
  /*
   * Voltage decoder. Three modes, in order of precedence:
   *   - fixed_uv != 0: rail has a hardwired output (e.g. LDO at 1.8 V).
   *     vout_reg/vmin_reg/vmax_reg are ignored.
   *   - vout_reg != 0: rail has a programmable VOUT. step_uv is the per-LSB
   *     voltage; negative=true means each code is more-negative (VNEG style).
   *   - all zero: rail has no voltage (e.g. TPORB control signal).
   */
  u16 vout_reg;
  u16 vmin_reg;
  u16 vmax_reg;
  u32 step_uv;
  int fixed_uv;
  bool negative;
};

static const struct da9172_rail_info da9172_rails[] = {
    {
        .name = "BUCK",
        .en_reg = DA9172_REG_PWRONOFF_BUCK_EN,
        .slot_reg = DA9172_REG_PWRONOFF_BUCK_SLOT,
        .vout_reg = DA9172_REG_BUCK_VOUT_VSEL,
        .vmin_reg = DA9172_REG_BUCK_MINV,
        .vmax_reg = DA9172_REG_BUCK_MAXV,
        .step_uv = DA9172_BUCK_STEP_UV,
    },
    {
        .name = "LDO",
        .en_reg = DA9172_REG_PWRONOFF_LDO_EN,
        .slot_reg = DA9172_REG_PWRONOFF_LDO_SLOT,
        .fixed_uv = 1800000, /* DA9172 LDO is hardwired to 1.8 V (datasheet 5.7). */
    },
    {
        .name = "VNEG1",
        .en_reg = DA9172_REG_PWRONOFF_VNEG1_EN,
        .slot_reg = DA9172_REG_PWRONOFF_VNEG1_SLOT,
        .vout_reg = DA9172_REG_VNEG_VOUT(0),
        .vmin_reg = DA9172_REG_VNEG_VMIN(0),
        .vmax_reg = DA9172_REG_VNEG_VMAX(0),
        .step_uv = DA9172_VNEG_STEP_UV,
        .negative = true,
    },
    {
        .name = "VNEG2",
        .en_reg = DA9172_REG_PWRONOFF_VNEG2_EN,
        .slot_reg = DA9172_REG_PWRONOFF_VNEG2_SLOT,
        .vout_reg = DA9172_REG_VNEG_VOUT(1),
        .vmin_reg = DA9172_REG_VNEG_VMIN(1),
        .vmax_reg = DA9172_REG_VNEG_VMAX(1),
        .step_uv = DA9172_VNEG_STEP_UV,
        .negative = true,
    },
    {
        .name = "VNEG3",
        .en_reg = DA9172_REG_PWRONOFF_VNEG3_EN,
        .slot_reg = DA9172_REG_PWRONOFF_VNEG3_SLOT,
        .vout_reg = DA9172_REG_VNEG_VOUT(2),
        .vmin_reg = DA9172_REG_VNEG_VMIN(2),
        .vmax_reg = DA9172_REG_VNEG_VMAX(2),
        .step_uv = DA9172_VNEG_STEP_UV,
        .negative = true,
    },
    {
        .name = "TPORB",
        .en_reg = DA9172_REG_PWRONOFF_TPORB_EN,
        .slot_reg = DA9172_REG_PWRONOFF_TPORB_SLOT,
        /* TPORB is a control signal, not a regulator. */
    },
};
#endif /* CONFIG_DA9172_PWRSEQ_DEBUG */

static ssize_t
revision_show(struct device* dev, struct device_attribute* attr, char* buf) {
  struct da9172_data* pmic = dev_get_drvdata(dev);
  return sysfs_emit(buf, "0x%02x\n", pmic->pmic_rev);
}

static ssize_t error_show(struct device* dev, struct device_attribute* attr, char* buf) {
  struct da9172_data* pmic = dev_get_drvdata(dev);
  return sysfs_emit(buf, "0x%02x\n", pmic->pmic_err);
}

static ssize_t
pmic_status_show(struct device* dev, struct device_attribute* attr, char* buf) {
  u8 value[10];
  int ret;

  struct da9172_data* pmic = dev_get_drvdata(dev);
  ret = da9172_i2c_read(pmic, DA9172_REG_VIRT_STATUS, &value[0], 10);
  if (ret < 0)
    return ret;

  return sysfs_emit(
      buf,
      "0x1080: "
      "%02X %02X %02X %02X "
      "%02X %02X %02X %02X "
      "%02X %02X\n",
      value[0],
      value[1],
      value[2],
      value[3],
      value[4],
      value[5],
      value[6],
      value[7],
      value[8],
      value[9]);
}

static int da9172_gpadc_read_raw(struct da9172_data* pmic, u8 ctrl, long* raw) {
  int ret;
  int retry = 0;
  int count = 0;
  long value = 0;
  u8 result[2];
  u8 en = 0x01;

  ret = da9172_i2c_write(pmic, DA9172_REG_GPADC_EN, &en, 1);
  if (ret < 0)
    return ret;

  while (count < GPADC_REPEAT_COUNT && retry < GPADC_REPEAT_COUNT + GPADC_MAX_RETRIES) {
    udelay(I2C_DELAY_US);
    ret = da9172_i2c_write(pmic, DA9172_REG_GPADC_CTRL, &ctrl, 1);
    if (ret < 0)
      return ret;
    udelay(I2C_DELAY_US);
    ret = da9172_i2c_read(pmic, DA9172_REG_GPADC_RES, &result[0], 2);
    if (ret < 0)
      return ret;
    if (!(result[1] & 0x80)) {
      value += (long)(result[0] << 4 | (result[1] & 0x0F));
      count++;
    }
    retry++;
  }
  dev_dbg(pmic->dev, "da9172_gpadc_read_raw ctrl: 0x%02x count: %d value: %ld\n",
          ctrl, count, value);
  if (count < GPADC_REPEAT_COUNT / 2)
    return -ENXIO;

  *raw = value / count;
  return 0;
}

static int da9172_gpadc_read_vneg(struct da9172_data* pmic,
                                  enum da9172_gpadc_channel ch_a,
                                  enum da9172_gpadc_channel ch_b,
                                  long* adc) {
  int ret;
  long raw_a, raw_b;
  u8 ctrl;

  ctrl = 0x80 | ch_a;
  ret = da9172_gpadc_read_raw(pmic, ctrl, &raw_a);
  if (ret < 0)
    return ret;
  // Reset GPADC mux to ch0 before reading node B
  ctrl = 0x80 | DA9172_GPADC_CH_REV;
  da9172_i2c_write(pmic, DA9172_REG_GPADC_CTRL, &ctrl, 1);
  udelay(I2C_DELAY_US);
  ctrl = 0x80 | ch_b;
  ret = da9172_gpadc_read_raw(pmic, ctrl, &raw_b);
  if (ret < 0)
    return ret;
  // |VNEGx| = 5 * (2 * A - B), convert to mV
  *adc = 5 * (2 * raw_a - raw_b) * 1000 / 4095;
  return 0;
}

static int da9172_gpadc_read(struct da9172_data* pmic, enum da9172_gpadc_channel channel, long* adc) {
  int ret;
  long raw;
  u8 ctrl;

  mutex_lock(&pmic->gpadc_lock);

  switch (channel) {
    case DA9172_GPADC_CH_AVDD:
    case DA9172_GPADC_CH_VBUCK:
      ctrl = 0x80 | channel;
      ret = da9172_gpadc_read_raw(pmic, ctrl, &raw);
      if (ret == 0)
        *adc = ADC_BYTES_TO_MV(raw);
      break;
    case DA9172_GPADC_CH_VNEG1_A:
    case DA9172_GPADC_CH_VNEG1_B:
      ret = da9172_gpadc_read_vneg(pmic, DA9172_GPADC_CH_VNEG1_A,
                                   DA9172_GPADC_CH_VNEG1_B, adc);
      break;
    case DA9172_GPADC_CH_VNEG2_A:
    case DA9172_GPADC_CH_VNEG2_B:
      ret = da9172_gpadc_read_vneg(pmic, DA9172_GPADC_CH_VNEG2_A,
                                   DA9172_GPADC_CH_VNEG2_B, adc);
      break;
    case DA9172_GPADC_CH_VNEG3_A:
    case DA9172_GPADC_CH_VNEG3_B:
      ret = da9172_gpadc_read_vneg(pmic, DA9172_GPADC_CH_VNEG3_A,
                                   DA9172_GPADC_CH_VNEG3_B, adc);
      break;
    case DA9172_GPADC_CH_IBUCK:
      ctrl = 0x80 | channel;
      ret = da9172_gpadc_read_raw(pmic, ctrl, &raw);
      if (ret == 0)
        *adc = ADC_BYTES_TO_MA(raw);
      break;
    case DA9172_GPADC_CH_VTJUNC:
      ctrl = 0x80 | channel;
      ret = da9172_gpadc_read_raw(pmic, ctrl, &raw);
      if (ret == 0)
        *adc = ADC_BYTES_TO_MDEGC(raw);
      break;
    default:
      ret = -EINVAL;
      break;
  }

  mutex_unlock(&pmic->gpadc_lock);
  return ret;
}

static ssize_t ibuck_show(struct device* dev, struct device_attribute* attr, char* buf) {
  long value;
  int ret;
  struct da9172_data* pmic = dev_get_drvdata(dev);
  if (!pmic->is_enabled) {
    return sysfs_emit(buf, "invalid state\n");
  }
  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_IBUCK, &value);
  if (ret < 0) {
    return sysfs_emit(buf, "invalid value\n");
  }
  return sysfs_emit(buf, "curr[mA]: %ld\n", value);
}

static ssize_t avdd_show(struct device* dev, struct device_attribute* attr, char* buf) {
  long value;
  int ret;
  struct da9172_data* pmic = dev_get_drvdata(dev);
  if (!pmic->is_enabled) {
    return sysfs_emit(buf, "invalid state\n");
  }
  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_AVDD, &value);
  if (ret < 0) {
    return sysfs_emit(buf, "invalid value\n");
  }
  return sysfs_emit(buf, "volt[mV]: %ld\n", value);
}

static ssize_t vbuck_show(struct device* dev, struct device_attribute* attr, char* buf) {
  long value;
  int ret;
  struct da9172_data* pmic = dev_get_drvdata(dev);
  if (!pmic->is_enabled) {
    return sysfs_emit(buf, "invalid state\n");
  }
  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_VBUCK, &value);
  if (ret < 0) {
    return sysfs_emit(buf, "invalid value\n");
  }
  return sysfs_emit(buf, "volt[mV]: %ld\n", value);
}

static ssize_t vneg1_show(struct device* dev, struct device_attribute* attr, char* buf) {
  long value;
  int ret;
  struct da9172_data* pmic = dev_get_drvdata(dev);
  if (!pmic->is_enabled) {
    return sysfs_emit(buf, "invalid state\n");
  }
  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_VNEG1_A, &value);
  if (ret < 0) {
    return sysfs_emit(buf, "invalid value\n");
  }
  return sysfs_emit(buf, "volt[mV]: %ld\n", value);
}

static ssize_t vneg2_show(struct device* dev, struct device_attribute* attr, char* buf) {
  long value;
  int ret;
  struct da9172_data* pmic = dev_get_drvdata(dev);
  if (!pmic->is_enabled) {
    return sysfs_emit(buf, "invalid state\n");
  }
  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_VNEG2_A, &value);
  if (ret < 0) {
    return sysfs_emit(buf, "invalid value\n");
  }
  return sysfs_emit(buf, "volt[mV]: %ld\n", value);
}

static ssize_t vneg3_show(struct device* dev, struct device_attribute* attr, char* buf) {
  long value;
  int ret;
  struct da9172_data* pmic = dev_get_drvdata(dev);
  if (!pmic->is_enabled) {
    return sysfs_emit(buf, "invalid state\n");
  }
  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_VNEG3_A, &value);
  if (ret < 0) {
    return sysfs_emit(buf, "invalid value\n");
  }
  return sysfs_emit(buf, "volt[mV]: %ld\n", value);
}

static ssize_t temp_show(struct device* dev, struct device_attribute* attr, char* buf) {
  long value;
  int ret;
  struct da9172_data* pmic = dev_get_drvdata(dev);
  if (!pmic->is_enabled) {
    return sysfs_emit(buf, "invalid state\n");
  }
  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_VTJUNC, &value);
  if (ret < 0) {
    return sysfs_emit(buf, "invalid value\n");
  }
  return sysfs_emit(buf, "temp[C]: %ld.%ld\n", value / 1000, (value % 1000) / 100);
}

static const char* da9172_power_state_name(unsigned int state) {
  switch (state) {
    case DA9172_PWR_STATE_PRE_BOOT:
      return "PRE_BOOT";
    case DA9172_PWR_STATE_READY:
      return "READY";
    case DA9172_PWR_STATE_ACTIVE:
      return "ACTIVE";
    default:
      return "UNKNOWN";
  }
}

static ssize_t
power_state_show(struct device* dev, struct device_attribute* attr, char* buf) {
  struct da9172_data* pmic = dev_get_drvdata(dev);
  unsigned int val, cur, prev;
  int ret;

  ret = regmap_read(pmic->regmap, DA9172_REG_MFSM_PWR_STATE_STATUS, &val);
  if (ret < 0)
    return ret;

  cur = FIELD_GET(MFSM_PWR_STATE_STATUS_CUR_STATE_MASK, val);
  prev = FIELD_GET(MFSM_PWR_STATE_STATUS_PREV_STATE_MASK, val);

  return sysfs_emit(
      buf,
      "0x%02x (cur: %s, prev: %s)\n",
      (u8)val,
      da9172_power_state_name(cur),
      da9172_power_state_name(prev));
}

#ifdef CONFIG_DA9172_PWRSEQ_DEBUG
static ssize_t pwrseq_show(struct device* dev, struct device_attribute* attr, char* buf) {
  struct da9172_data* pmic = dev_get_drvdata(dev);
  unsigned int mfsm;
  u8 t_pu_packed[4], t_pd_packed[4];
  u8 t_slot_pu[8], t_slot_pd[8];
  int len = 0, i, ret;

  ret = regmap_read(pmic->regmap, DA9172_REG_MFSM_PWR_STATE_STATUS, &mfsm);
  if (ret == 0)
    len += sysfs_emit_at(buf, len, "MFSM state: %s\n\n",
                         da9172_power_state_name(
                             FIELD_GET(MFSM_PWR_STATE_STATUS_CUR_STATE_MASK, mfsm)));

  ret = da9172_i2c_read(pmic, DA9172_REG_PWRONOFF_SLOT_TIMINGS_PU_BASE,
                        t_pu_packed, 4);
  if (ret < 0)
    return ret;
  ret = da9172_i2c_read(pmic, DA9172_REG_PWRONOFF_SLOT_TIMINGS_PD_BASE,
                        t_pd_packed, 4);
  if (ret < 0)
    return ret;

  /* Each timings byte holds two 4-bit slot codes: low nibble = even slot. */
  for (i = 0; i < 4; i++) {
    t_slot_pu[2 * i]     = t_pu_packed[i] & 0x0F;
    t_slot_pu[2 * i + 1] = t_pu_packed[i] >> 4;
    t_slot_pd[2 * i]     = t_pd_packed[i] & 0x0F;
    t_slot_pd[2 * i + 1] = t_pd_packed[i] >> 4;
  }

  len += sysfs_emit_at(buf, len, "Slot timers (us):\n  UP:");
  for (i = 0; i < 8; i++)
    len += sysfs_emit_at(buf, len, " PU%d=%u", i,
                         da9172_slot_delay_us[t_slot_pu[i]]);
  len += sysfs_emit_at(buf, len, "\n  DN:");
  for (i = 0; i < 8; i++)
    len += sysfs_emit_at(buf, len, " PD%d=%u", i,
                         da9172_slot_delay_us[t_slot_pd[i]]);

  /*
   * VMIN/VMAX columns mirror the register names; for negative rails (VNEG*)
   * VMIN holds the most-negative limit (largest |V|) and VMAX the
   * least-negative. For positive rails (BUCK) VMIN/VMAX work as their names
   * suggest. LDO has a fixed output and shows n/a.
   */
  len += sysfs_emit_at(buf, len,
                       "\n\nRail config (negative rails: VMIN=most-negative, VMAX=least-negative limit):\n"
                       "  %-6s %-5s %-6s %-7s %-7s %-10s %-10s %-10s\n",
                       "rail", "READY", "ACTIVE", "UP_SLOT", "DN_SLOT",
                       "vmin_uV", "vmax_uV", "vout_uV");

  for (i = 0; i < ARRAY_SIZE(da9172_rails); i++) {
    const struct da9172_rail_info* r = &da9172_rails[i];
    u8 en = 0, slot = 0;
    bool en_ok, slot_ok;
    char en_ready[4] = "?", en_active[4] = "?";
    char up_slot_s[4] = "?", dn_slot_s[4] = "?";
    char vmin_s[16] = "n/a", vmax_s[16] = "n/a", vout_s[16] = "n/a";

    en_ok = da9172_i2c_read(pmic, r->en_reg, &en, 1) == 0;
    if (!en_ok)
      dev_warn(pmic->dev, "pwrseq: read %s EN reg 0x%04x failed\n",
               r->name, r->en_reg);

    slot_ok = da9172_i2c_read(pmic, r->slot_reg, &slot, 1) == 0;
    if (!slot_ok)
      dev_warn(pmic->dev, "pwrseq: read %s SLOT reg 0x%04x failed\n",
               r->name, r->slot_reg);

    if (en_ok) {
      scnprintf(en_ready, sizeof(en_ready), "%d",
                !!(en & DA9172_PWRONOFF_EN_READY));
      scnprintf(en_active, sizeof(en_active), "%d",
                !!(en & DA9172_PWRONOFF_EN_ACTIVE));
    }
    if (slot_ok) {
      scnprintf(up_slot_s, sizeof(up_slot_s), "%u",
                (unsigned int)FIELD_GET(DA9172_PWRONOFF_UP_SLOT_MASK, slot));
      scnprintf(dn_slot_s, sizeof(dn_slot_s), "%u",
                (unsigned int)FIELD_GET(DA9172_PWRONOFF_DN_SLOT_MASK, slot));
    }

    if (r->fixed_uv) {
      scnprintf(vout_s, sizeof(vout_s), "%d", r->fixed_uv);
    } else if (r->vout_reg) {
      u8 vmax, vmin, vout;
      int sign = r->negative ? -1 : 1;
      if (da9172_i2c_read(pmic, r->vmax_reg, &vmax, 1) == 0)
        scnprintf(vmax_s, sizeof(vmax_s), "%d", sign * (int)vmax * (int)r->step_uv);
      if (da9172_i2c_read(pmic, r->vmin_reg, &vmin, 1) == 0)
        scnprintf(vmin_s, sizeof(vmin_s), "%d", sign * (int)vmin * (int)r->step_uv);
      if (da9172_i2c_read(pmic, r->vout_reg, &vout, 1) == 0)
        scnprintf(vout_s, sizeof(vout_s), "%d", sign * (int)vout * (int)r->step_uv);
    }

    len += sysfs_emit_at(buf, len,
                         "  %-6s %-5s %-6s %-7s %-7s %-10s %-10s %-10s\n",
                         r->name, en_ready, en_active,
                         up_slot_s, dn_slot_s,
                         vmin_s, vmax_s, vout_s);
  }

  return len;
}

/*
 * Read the current VOUT setpoint of VNEG channel n (0..2) in microvolts.
 * Returns the negative microvolt value, or a negative errno on read failure.
 * Note: errno collisions are not possible because valid output is 0..-6_375_000.
 */
static int da9172_vneg_get_uv(struct da9172_data* pmic, int n) {
  u8 code;
  int ret = da9172_i2c_read(pmic, DA9172_REG_VNEG_VOUT(n), &code, 1);
  if (ret < 0)
    return ret;
  return -(int)code * DA9172_VNEG_STEP_UV;
}
#endif /* CONFIG_DA9172_PWRSEQ_DEBUG */

#if defined(CONFIG_DA9172_PWRSEQ_DEBUG) || defined(CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE)
/*
 * Set VNEG channel n (0..2) to the requested microvolt level (must be <= 0).
 * Validates the resulting register code against the chip's VMAX/VMIN clamps
 * and refuses writes outside the window. VOUT_LOCK is cleared on the enable
 * path by da9172_apply_vout_config(), so this path does no lock dance.
 */
static int da9172_vneg_set_uv(struct da9172_data* pmic, int n, int uv) {
  u8 code, vmin, vmax;
  int ret;

  if (uv > 0 || uv < -255 * DA9172_VNEG_STEP_UV) {
    dev_err(pmic->dev, "VNEG%d: %d uV out of range\n", n + 1, uv);
    return -EINVAL;
  }
  code = (u8)((-uv) / DA9172_VNEG_STEP_UV);

  ret = da9172_i2c_read(pmic, DA9172_REG_VNEG_VMAX(n), &vmax, 1);
  if (ret < 0)
    return ret;
  ret = da9172_i2c_read(pmic, DA9172_REG_VNEG_VMIN(n), &vmin, 1);
  if (ret < 0)
    return ret;
  /* Larger code = more negative. VMAX caps the smallest |V|; VMIN the largest. */
  if (code < vmax || code > vmin) {
    dev_err(pmic->dev,
            "VNEG%d: code 0x%02x outside [vmax 0x%02x .. vmin 0x%02x]\n",
            n + 1, code, vmax, vmin);
    return -ERANGE;
  }

  ret = da9172_i2c_write(pmic, DA9172_REG_VNEG_VOUT(n), &code, 1);
  if (ret < 0)
    return ret;
  dev_info(pmic->dev, "VNEG%d: set to %d uV (code 0x%02x)\n", n + 1, uv, code);
  return 0;
}

/* Apply the VNEG VOUT configuration on every power cycle. */
static void da9172_apply_vout_config(struct da9172_data* pmic) {
  u8 lock;

  if (da9172_i2c_read(pmic, DA9172_REG_LOCK_VOUT_LOCK_CTRL, &lock, 1) == 0
      && (lock & DA9172_VOUT_LOCK_BIT)) {
    lock &= ~DA9172_VOUT_LOCK_BIT;
    if (da9172_i2c_write(pmic, DA9172_REG_LOCK_VOUT_LOCK_CTRL, &lock, 1))
      dev_warn(pmic->dev, "failed to clear VOUT_LOCK\n");
  }

#ifdef CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE
  /*
   * Reapply the VNEG VOUT overrides cached from the device tree in probe()
   * (the CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE block). Rails without an override
   * keep their OTP default. Intended for use until the OTP profile is finalized.
   */
  {
    int n;
    for (n = 0; n < 3; n++) {
      if (!pmic->vneg_override[n])
        continue;
      if (da9172_vneg_set_uv(pmic, n, pmic->vneg_vout_uv[n]))
        dev_warn(pmic->dev, "VNEG%d: failed to apply DT override\n", n + 1);
    }
  }
#endif /* CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE */
}
#else
static inline void da9172_apply_vout_config(struct da9172_data* pmic) {}
#endif /* CONFIG_DA9172_PWRSEQ_DEBUG || CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE */

#ifdef CONFIG_DA9172_PWRSEQ_DEBUG
/* The sysfs attribute name carries the channel index in its first numeric char. */
static int da9172_vneg_index_from_attr(struct device_attribute* attr) {
  /* Attr names are "vneg1_vout", "vneg2_vout", "vneg3_vout". */
  if (attr->attr.name[4] >= '1' && attr->attr.name[4] <= '3')
    return attr->attr.name[4] - '1';
  return -EINVAL;
}

static ssize_t vneg_vout_show(struct device* dev, struct device_attribute* attr,
                              char* buf) {
  struct da9172_data* pmic = dev_get_drvdata(dev);
  int n = da9172_vneg_index_from_attr(attr);
  int uv;
  if (n < 0)
    return n;
  uv = da9172_vneg_get_uv(pmic, n);
  /* da9172_vneg_get_uv returns either uv (<=0) or a negative errno (-EIO etc).
   * Distinguish: errnos are small negatives (> -1000), valid uv ranges to -6.4M. */
  if (uv > -1000 && uv < 0)
    return uv;
  return sysfs_emit(buf, "%d\n", uv);
}

static ssize_t vneg_vout_store(struct device* dev, struct device_attribute* attr,
                               const char* buf, size_t count) {
  struct da9172_data* pmic = dev_get_drvdata(dev);
  int n = da9172_vneg_index_from_attr(attr);
  int uv, ret;
  if (n < 0)
    return n;
  ret = kstrtoint(buf, 0, &uv);
  if (ret)
    return ret;
  ret = da9172_vneg_set_uv(pmic, n, uv);
  return ret < 0 ? ret : count;
}
#endif /* CONFIG_DA9172_PWRSEQ_DEBUG */

static DEVICE_ATTR_RO(revision);
static DEVICE_ATTR_RO(error);
static DEVICE_ATTR_RO(pmic_status);
static DEVICE_ATTR_RO(ibuck);
static DEVICE_ATTR_RO(avdd);
static DEVICE_ATTR_RO(vbuck);
static DEVICE_ATTR_RO(vneg1);
static DEVICE_ATTR_RO(vneg2);
static DEVICE_ATTR_RO(vneg3);
static DEVICE_ATTR_RO(temp);
static DEVICE_ATTR_RO(power_state);
#ifdef CONFIG_DA9172_PWRSEQ_DEBUG
static DEVICE_ATTR_RO(pwrseq);
static struct device_attribute dev_attr_vneg1_vout =
    __ATTR(vneg1_vout, 0644, vneg_vout_show, vneg_vout_store);
static struct device_attribute dev_attr_vneg2_vout =
    __ATTR(vneg2_vout, 0644, vneg_vout_show, vneg_vout_store);
static struct device_attribute dev_attr_vneg3_vout =
    __ATTR(vneg3_vout, 0644, vneg_vout_show, vneg_vout_store);
#endif /* CONFIG_DA9172_PWRSEQ_DEBUG */

static struct attribute* da9172_attrs[] = {
    &dev_attr_revision.attr,
    &dev_attr_error.attr,
    &dev_attr_pmic_status.attr,
    &dev_attr_ibuck.attr,
    &dev_attr_avdd.attr,
    &dev_attr_vbuck.attr,
    &dev_attr_vneg1.attr,
    &dev_attr_vneg2.attr,
    &dev_attr_vneg3.attr,
    &dev_attr_temp.attr,
    &dev_attr_power_state.attr,
#ifdef CONFIG_DA9172_PWRSEQ_DEBUG
    &dev_attr_pwrseq.attr,
    &dev_attr_vneg1_vout.attr,
    &dev_attr_vneg2_vout.attr,
    &dev_attr_vneg3_vout.attr,
#endif /* CONFIG_DA9172_PWRSEQ_DEBUG */
    NULL,
};
ATTRIBUTE_GROUPS(da9172);

int da9172_i2c_write(struct da9172_data* pmic, u16 reg, void* buffer, unsigned int len) {
  int ret = 0;
  int retry = 0;
  struct regmap* regmap = pmic->regmap;

  ret = regmap_bulk_write(regmap, (unsigned int)reg, buffer, len);
  while (ret < 0 && retry < DA9172_MAX_WRITE_RETRIES) {
    dev_warn(pmic->dev, "regmap_write failed with error %d\n", ret);
    udelay(I2C_DELAY_US);
    ret = regmap_bulk_write(regmap, (unsigned int)reg, buffer, len);
    retry++;
  }

  if (ret < 0)
    dev_err(pmic->dev, "regmap_write failed with error %d after retries\n", ret);

  return ret;
}

int da9172_i2c_read(struct da9172_data* pmic, u16 reg, void* buffer, unsigned int len) {
  int ret = 0;
  int retry = 0;
  struct regmap* regmap = pmic->regmap;

  ret = regmap_bulk_read(regmap, (unsigned int)reg, buffer, len);
  while (ret < 0 && retry < DA9172_MAX_READ_RETRIES) {
    dev_warn(pmic->dev, "regmap_read failed with error %d\n", ret);
    udelay(I2C_DELAY_US);
    ret = regmap_bulk_read(regmap, (unsigned int)reg, buffer, len);
    retry++;
  }

  if (ret < 0) {
    dev_err(pmic->dev, "regmap_read failed with error %d after retries\n", ret);
    return ret;
  }

  return ret;
}

static void power_transition_delay(bool enable) {
  const unsigned long delay_us = enable ? 10000 : 2000;
  usleep_range(delay_us, delay_us + 50);
}

static void da9172_set_enable_using_gpios(struct da9172_data* pmic, bool enable) {
  const int gpio_val = enable ? 1 : 0;

  dev_dbg(pmic->dev, "da9172_set_enable_using_gpios %d\n", enable);

  if (pmic->disp_en_gpio != NULL) {
    gpiod_set_value(pmic->disp_en_gpio, gpio_val);
  }
  power_transition_delay(enable);
  gpiod_set_value(pmic->pmic_en_gpio, gpio_val);
}

static int da9172_set_enable_using_i2c(struct regulator_dev* rdev, bool enable) {
  struct da9172_data* pmic = rdev_get_drvdata(rdev);

  dev_dbg(pmic->dev, "da9172_set_enable_using_i2c %d\n", enable);
  if (enable)
    return regulator_enable_regmap(rdev);

  return regulator_disable_regmap(rdev);
}

static int da9172_wait_for_power_state(struct da9172_data* pmic, unsigned int target) {
  unsigned int val = 0;
  ktime_t start = ktime_get();
  int ret;

  /*
   * The current-state field is only meaningful once the FSM has settled, so
   * wait for the transition flag to clear before matching against target.
   */
  ret = regmap_read_poll_timeout(
      pmic->regmap, DA9172_REG_MFSM_PWR_STATE_STATUS, val,
      !(val & MFSM_PWR_STATE_STATUS_TRANS)
          && FIELD_GET(MFSM_PWR_STATE_STATUS_CUR_STATE_MASK, val) == target,
      DA9172_PWR_STATE_POLL_INTERVAL_US, pmic->pwr_state_timeout_us);
  if (ret == 0) {
    dev_dbg(pmic->dev, "reached power state %s in %lld ms\n",
            da9172_power_state_name(target),
            ktime_to_ms(ktime_sub(ktime_get(), start)));
  } else if (ret == -ETIMEDOUT) {
    if (val & MFSM_PWR_STATE_STATUS_TRANS)
      dev_warn(pmic->dev,
               "timed out waiting for power state %s; FSM still transitioning (status 0x%02x)\n",
               da9172_power_state_name(target), (u8)val);
    else
      dev_warn(pmic->dev, "timed out waiting for power state %s (cur: %s)\n",
               da9172_power_state_name(target),
               da9172_power_state_name(
                   FIELD_GET(MFSM_PWR_STATE_STATUS_CUR_STATE_MASK, val)));
  } else {
    dev_err(pmic->dev, "failed to read power state: %d\n", ret);
  }
  return ret;
}

static void da9172_vin_put_ref(struct da9172_data* pmic) {
  if (pmic->vin_ref_held) {
    pmic->vin_ref_held = false;
    regulator_disable(pmic->vin);
  }
}

static void da9172_vin_release_work(struct work_struct* work) {
  da9172_vin_put_ref(container_of(work, struct da9172_data, vin_release_work));
}

/* devm puts the handle but never disables it; a held reference strands vin on. */
static void da9172_vin_release(void* priv) {
  struct da9172_data* pmic = priv;

  cancel_work_sync(&pmic->vin_release_work);
  da9172_vin_put_ref(pmic);
}

/*
 * The temp sensors sharing this load switch drop vin during their own probe,
 * before the panel driver claims it. Own handle, not rdev->supply, which the
 * core already refcounts against its own enables.
 */
static int da9172_adopt_vin(struct da9172_data* pmic) {
  int ret;

  pmic->vin = devm_regulator_get_optional(pmic->dev, "vin");
  if (IS_ERR(pmic->vin)) {
    ret = PTR_ERR(pmic->vin);
    pmic->vin = NULL;
    return ret;
  }

  INIT_WORK(&pmic->vin_release_work, da9172_vin_release_work);
  ret = devm_add_action_or_reset(pmic->dev, da9172_vin_release, pmic);
  if (ret)
    return ret;

  ret = regulator_enable(pmic->vin);
  if (ret)
    return ret;

  pmic->vin_ref_held = true;
  return 0;
}

static int da9172_enable(struct regulator_dev* rdev) {
  int ret = 0;

  struct da9172_data* pmic = rdev_get_drvdata(rdev);

  if (pmic->is_enabled) {
    dev_dbg(pmic->dev, "da9172 already enabled\n");
    return ret;
  }

  if (pmic->pmic_en_gpio != NULL) {
    da9172_set_enable_using_gpios(pmic, true);
  } else {
    da9172_apply_vout_config(pmic);
    ret = da9172_set_enable_using_i2c(rdev, true);
    if (ret) {
      dev_err(pmic->dev, "failed to enable da9172 via i2c: %d\n", ret);
      return ret;
    }
    da9172_wait_for_power_state(pmic, DA9172_PWR_STATE_ACTIVE);
  }
  pmic->is_enabled = true;

  return ret;
}

static int da9172_disable(struct regulator_dev* rdev) {
  int ret = 0;
  struct da9172_data* pmic = rdev_get_drvdata(rdev);

  if (!pmic->is_enabled) {
    dev_dbg(pmic->dev, "da9172 already disabled\n");
    return ret;
  }

  // TODO load PMIC Error register and store in pmic->pmic_err
  if (pmic->pmic_en_gpio != NULL) {
    da9172_set_enable_using_gpios(pmic, false);
  } else {
    ret = da9172_set_enable_using_i2c(rdev, false);
    if (ret) {
      dev_err(pmic->dev, "failed to disable da9172 via i2c: %d\n", ret);
      return ret;
    }
    da9172_wait_for_power_state(pmic, DA9172_PWR_STATE_READY);
  }

  pmic->is_enabled = false;

  /* Hand back the continuous-splash reference so vin can drop with us. */
  if (pmic->vin_ref_held)
    schedule_work(&pmic->vin_release_work);

  return ret;
}

static int da9172_is_enabled(struct regulator_dev* rdev) {
  struct da9172_data* pmic = rdev_get_drvdata(rdev);
  return pmic->is_enabled;
}

static const struct regulator_ops da9172_regulator_ops = {
    .is_enabled = da9172_is_enabled,
    .enable = da9172_enable,
    .disable = da9172_disable,
};

static const struct regulator_desc da9172_regulator_desc = {
    .name = "pmicDA9172",
    .ops = &da9172_regulator_ops,
    .owner = THIS_MODULE,
    .enable_reg = DA9172_REG_MFSM_PWR_STATE_TARGET,
    .enable_mask = MFSM_PWR_STATE_TARGET_STATE_MASK,
    .enable_val = DA9172_PWR_STATE_ACTIVE,
    .disable_val = DA9172_PWR_STATE_READY,
    .supply_name = "vin",
};

static int da9172_thermal_get_temp(void* data, int* temp) {
  struct da9172_data* pmic = data;
  long value;
  int ret;

  if (!pmic->is_enabled)
    return -ENODATA;

  ret = da9172_gpadc_read(pmic, DA9172_GPADC_CH_VTJUNC, &value);
  if (ret < 0)
    return ret;

  *temp = (int)value;
  return 0;
}

static const struct thermal_zone_of_device_ops da9172_thermal_ops = {
    .get_temp = da9172_thermal_get_temp,
};

static const struct of_device_id da9172_of_match[] = {
    {.compatible = "pmic-i2c-da9172"},
    {},
};

static int da9172_probe(struct i2c_client* const i2c, const struct i2c_device_id* id) {
  struct device* dev = &i2c->dev;
  struct regulator_dev* rdev;
  struct regulator_init_data* init_data;
  struct da9172_data* pmic;
  struct regulator_config config = {};
  bool is_cont_splash_enabled;
  int ret;
  dev_info(dev, "da9172_probe start\n");

  if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
    dev_err(&i2c->dev, "No I2C functionality present\n");
    return -ENODEV;
  }

  is_cont_splash_enabled = of_property_read_bool(i2c->dev.of_node, "continuous-splash");
  pmic = devm_kzalloc(dev, sizeof(*pmic), GFP_KERNEL);
  if (pmic == NULL)
    return -ENOMEM;
  pmic->i2c = i2c;
  pmic->dev = dev;
  pmic->adapter = i2c->adapter;
  pmic->addr = i2c->addr;
  if (of_property_read_u32(dev->of_node, "meta,pwr-state-timeout-us",
                          &pmic->pwr_state_timeout_us))
    pmic->pwr_state_timeout_us = DA9172_PWR_STATE_POLL_TIMEOUT_US;
  /* If continuous splash is enabled, PMIC will be turned on by UEFI */
  pmic->is_enabled = is_cont_splash_enabled;
  mutex_init(&pmic->gpadc_lock);
  pmic->disp_en_gpio = devm_gpiod_get_index(dev, "disp", 0, GPIOD_ASIS);
  if (IS_ERR(pmic->disp_en_gpio)) {
    ret = PTR_ERR(pmic->disp_en_gpio);
    dev_warn(dev, "devm_gpiod_get_index 'disp' failed with ret: %d\n", ret);
    pmic->disp_en_gpio = NULL;
  }
  pmic->pmic_en_gpio = devm_gpiod_get_index(dev, "pmic", 0, GPIOD_ASIS);
  if (IS_ERR(pmic->pmic_en_gpio)) {
    ret = PTR_ERR(pmic->pmic_en_gpio);
    dev_warn(dev, "devm_gpiod_get_index 'pmic' failed with ret: %d\n", ret);
    pmic->pmic_en_gpio = NULL;
  }

  pmic->regmap = devm_regmap_init_i2c(i2c, &da9172_regmap_config);
  if (IS_ERR(pmic->regmap)) {
    ret = PTR_ERR(pmic->regmap);
    dev_err(dev, "Error %d in initializing regmap\n", ret);
    return ret;
  }

  i2c_set_clientdata(i2c, pmic);

#ifdef CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE
  {
    int n;
    for (n = 0; n < 3; n++) {
      char prop[32];
      s32 uv;
      snprintf(prop, sizeof(prop), "meta,vneg%d-vout-microvolt", n + 1);
      if (of_property_read_s32(dev->of_node, prop, &uv))
        continue;
      pmic->vneg_vout_uv[n] = uv;
      pmic->vneg_override[n] = true;
    }
  }
#endif /* CONFIG_DA9172_VNEG_VOUT_DT_OVERRIDE */

  ret = da9172_i2c_read(pmic, DA9172_REG_MASK_REV, &pmic->pmic_rev, 1);
  if (ret < 0)
    dev_warn(dev, "Failed to read PMIC revision: %d\n", ret);

  config.dev = dev;
  init_data = of_get_regulator_init_data(dev, dev->of_node, &da9172_regulator_desc);
  if (!init_data) {
    dev_err(dev, "Failed to get regulator init data\n");
    return -ENOMEM;
  }
  config.init_data = init_data;
  init_data->constraints.keep_on = 1;
  init_data->constraints.valid_ops_mask = REGULATOR_CHANGE_STATUS;
  config.regmap = pmic->regmap;
  config.driver_data = pmic;
  config.of_node = dev->of_node;
  rdev = devm_regulator_register(dev, &da9172_regulator_desc, &config);
  if (IS_ERR(rdev)) {
    dev_err(dev, "Failed to register regulator da9172.\n");
    return PTR_ERR(rdev);
  }

  if (is_cont_splash_enabled) {
    ret = da9172_adopt_vin(pmic);
    if (ret == -EPROBE_DEFER)
      return ret;
    if (ret) {
      /* Do not claim a rail whose input we failed to hold: make enable do real work. */
      dev_warn(dev, "failed to adopt vin for continuous splash: %d\n", ret);
      pmic->is_enabled = false;
    }
  }

  ret = devm_regulator_debug_register(dev, rdev);
  if (ret)
    dev_err(dev, "Failed to register debug regulator, rc=%d\n", ret);

  pmic->tz = devm_thermal_zone_of_sensor_register(dev, 0, pmic, &da9172_thermal_ops);
  if (IS_ERR(pmic->tz)) {
    dev_warn(dev, "Failed to register thermal zone: %ld\n", PTR_ERR(pmic->tz));
    pmic->tz = NULL;
  }

  dev_info(dev, "probe successful\n");
  return 0;
}

static int da9172_remove(struct i2c_client* i2c) {
  return 0;
}

static const struct i2c_device_id da9172_id[] = {
    {
        "da9172",
    },
    {},
};
MODULE_DEVICE_TABLE(i2c, da9172_id);

static struct i2c_driver da9172_driver = {
    .driver =
        {
            .name = "da9172-driver",
            .of_match_table = da9172_of_match,
            .dev_groups = da9172_groups,
        },
    .probe = da9172_probe,
    .remove = da9172_remove,
    .id_table = da9172_id,
};
module_i2c_driver(da9172_driver);

MODULE_DESCRIPTION("da9172 PMIC regulator driver");
MODULE_LICENSE("GPL");
