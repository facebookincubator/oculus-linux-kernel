// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#include "gpio_low_volt.h"
#include "metabattery.h"
#include "metabattery_types.h"

#define TEMP_CENTIGRADE_TO_MC(t) ((t) * 100)

#define METABATTERY_PROP_STR_LEN 14

static enum power_supply_property metabattery_battery_props[] = {
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_CHARGE_COUNTER,
    POWER_SUPPLY_PROP_CHARGE_FULL,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_VOLTAGE_REP,
    POWER_SUPPLY_PROP_VOLTAGE_AVG,
    POWER_SUPPLY_PROP_VOLTAGE_OCV,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CURRENT_REP,
    POWER_SUPPLY_PROP_CURRENT_AVG,
    POWER_SUPPLY_PROP_POWER_NOW,
    POWER_SUPPLY_PROP_POWER_AVG,
    POWER_SUPPLY_PROP_TEMP,
    POWER_SUPPLY_PROP_CYCLE_COUNT,
    POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
    POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
    POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX,
    POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_CAPACITY_LEVEL};

const u32 qscale_capacity_step_size_uah[] = {
  1250,  // 1.25 mAh in uAh
  2500,  // 2.5 mAh in uAh
  5000,  // 5.0 mAh in uAh
    10000, // 10.0 mAh in uAh
    12500, // 12.5 mAh in uAh
    20000, // 20.0 mAh in uAh
    25000, // 25.0 mAh in uAh
  50000  // 50.0 mAh in uAh
};

const int kCurrentAlertScaleFactor =
    400000; /* unit = uV -> converted to uA by dividing by rsense */

// SIP serial number register constants
uint16_t sip_serial_num_regs[] = {
    METABATTERY_TTF_CONFIG_REG,
    METABATTERY_USER_MEMORY_REG,
    METABATTERY_SC_OCV_LIM_REG,
    METABATTERY_MANFCTR_DATE_REG,
    METABATTERY_FIRST_USED_REG,
};

// PACK serial number register constants
uint16_t pack_serial_num_regs[] = {
    METABATTERY_SERIAL_NUM_1_REG,
    METABATTERY_SERIAL_NUM_2_REG,
    METABATTERY_DEVICE_NAME_0_REG,
    METABATTERY_DEVICE_NAME_1_REG,
    METABATTERY_MANFCTR_NAME_0_REG,
    METABATTERY_MANFCTR_NAME_1_REG,
    METABATTERY_MANFCTR_NAME_2_REG,
};

int metabattery_get_stp_interface(struct metabattery_data* data) {
  data->stp_interface_node = of_parse_phandle(data->dev->of_node, "stp-interface", 0);
  if (!data->stp_interface_node) {
    dev_err(data->dev, "%s: unable to get stp-interface device_node\n", __func__);
    return -ENOENT;
  }
  return 0;
}

static irqreturn_t gpio_irq_handler(int irq, void* dev_id) {
  return IRQ_HANDLED;
}

int metabattery_xfer(
    struct metabattery_data* data,
    uint8_t mode_read,
    uint16_t metabattery_type,
    uint16_t battery_id,
    int* val) {
  int ret = 0;
  int payload_len = 0;
  int stp_msg_len = 0;
  struct stp_interface_msg_header* header = NULL;
  struct stp_interface_msg_payload* payload = NULL;
  uint8_t* buf = NULL;

  payload_len = sizeof(*payload);
  if (!mode_read)
    payload_len += METABATTERY_MAX_MESSAGE_SIZE_BYTES;
  stp_msg_len = sizeof(*header) + payload_len;

  if (!data->stp_interface_node) {
    ret = metabattery_get_stp_interface(data);
    if (ret)
      goto err_exit;
  }

  // this is freed inside stp_interface_xfer
  header = kmalloc(stp_msg_len, GFP_KERNEL);
  if (!header) {
    ret = -ENOMEM;
    goto err_exit;
  }

  buf = kmalloc(METABATTERY_MAX_MESSAGE_SIZE_BYTES, GFP_KERNEL);
  if (!buf) {
    ret = -ENOMEM;
    goto err_exit;
  }

  header->csum = 0;
  header->payload_len = cpu_to_le16(payload_len);

  payload = (struct stp_interface_msg_payload*)&header[1];
  payload->flags = PAYLOAD_TYPE_DATALEN;
  payload->dev_addr = cpu_to_le16(battery_id);
  payload->reg_addr = cpu_to_le16(metabattery_type);
  payload->mode_read = mode_read;
  payload->u.datalen = cpu_to_le16(METABATTERY_MAX_MESSAGE_SIZE_BYTES);
  if (!mode_read)
    memcpy(&payload->data, val, METABATTERY_MAX_MESSAGE_SIZE_BYTES);
  ret = stp_interface_xfer(
      data->stp_interface_node,
      buf,
      header,
      payload,
      stp_msg_len,
      METABATTERY_MAX_MESSAGE_SIZE_BYTES,
      METABATTERY_MAGIC);
  if (ret) {
    dev_err(data->dev, "stp_interface_xfer failed: %d\n", ret);
    data->battery_present = 0;
  } else {
    memcpy(val, buf, METABATTERY_MAX_MESSAGE_SIZE_BYTES);
  }

  kfree(buf);
err_exit:
  return ret;
}

int metabattery_batch_xfer(
    struct metabattery_data* data,
    uint16_t metabattery_type,
    uint16_t battery_id,
    metabattery_batch_update_t* batch_update) {
  int ret = 0;
  int payload_len = 0;
  int stp_msg_len = 0;
  struct stp_interface_msg_header* header = NULL;
  struct stp_interface_msg_payload* payload = NULL;
  uint8_t* buf = NULL;

  payload_len = sizeof(*payload);
  stp_msg_len = sizeof(*header) + payload_len;

  if (!data->stp_interface_node) {
    ret = metabattery_get_stp_interface(data);
    if (ret)
      goto err_exit;
  }

  // this is freed inside stp_interface_xfer
  header = kmalloc(stp_msg_len, GFP_KERNEL);
  if (!header) {
    ret = -ENOMEM;
    goto err_exit;
  }

  buf = kmalloc(METABATTERY_BATCH_UPDATE_MESSAGE_SIZE_BYTES, GFP_KERNEL);
  if (!buf) {
    ret = -ENOMEM;
    goto err_exit;
  }

  header->csum = 0;
  header->payload_len = cpu_to_le16(payload_len);

  payload = (struct stp_interface_msg_payload*)&header[1];
  payload->flags = PAYLOAD_TYPE_DATALEN;
  payload->dev_addr = cpu_to_le16(battery_id);
  payload->reg_addr = cpu_to_le16(metabattery_type);
  payload->mode_read = 1;
  payload->u.datalen = cpu_to_le16(METABATTERY_BATCH_UPDATE_MESSAGE_SIZE_BYTES);
  ret = stp_interface_xfer(
      data->stp_interface_node,
      buf,
      header,
      payload,
      stp_msg_len,
      METABATTERY_BATCH_UPDATE_MESSAGE_SIZE_BYTES,
      METABATTERY_MAGIC);
  if (ret) {
    dev_err(data->dev, "stp_interface_xfer failed: %d\n", ret);
  } else {
    memcpy(batch_update, buf, sizeof(metabattery_batch_update_t));
  }

  kfree(buf);
err_exit:
  return ret;
}

static bool metabattery_data_is_cached(uint16_t metabattery_type) {
  switch (metabattery_type) {
    case METABATTERY_TYPES_CAPACITY:
    case METABATTERY_TYPES_TEMP:
    case METABATTERY_MIX_SOC:
    case METABATTERY_REP_SOC:
    case METABATTERY_TYPES_HEALTH:
    case METABATTERY_TYPES_VOLTAGE_NOW:
    case METABATTERY_TYPES_VOLTAGE_OCV:
    case METABATTERY_TYPES_CURRENT_NOW:
    case METABATTERY_TYPES_CURRENT_AVG:
    case METABATTERY_TYPES_CHARGE_FULL:
    case METABATTERY_TYPES_POWER_AVG:
    case METABATTERY_TYPES_CHARGE_COUNTER:
    case METABATTERY_TYPES_CYCLE_COUNT:
    case METABATTERY_IS_FULL:
      return true;
  }
  return false;
}

static int metabattery_get_cached_value(struct metabattery_data* data, uint16_t metabattery_type, int* val) {
  // Check if the cached value is still valid
  unsigned int current_timestamp = jiffies_to_msecs(jiffies);
  if (current_timestamp - data->data_cache->timestamp > METABATTERY_DEFAULT_CACHING_REFRESH_INTERVAL_MS)
    return -ESTALE;

  switch (metabattery_type) {
    case METABATTERY_TYPES_CAPACITY:
      if (data->data_cache->capacity == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->capacity;
      break;
    case METABATTERY_TYPES_TEMP:
      if (data->data_cache->temperature == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->temperature;
      break;
    case METABATTERY_MIX_SOC:
      if (data->data_cache->mix_soc == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->mix_soc;
      break;
    case METABATTERY_REP_SOC:
      if (data->data_cache->rep_soc == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->rep_soc;
      break;
    case METABATTERY_TYPES_HEALTH:
      if (data->data_cache->health == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->health;
      break;
    case METABATTERY_TYPES_VOLTAGE_NOW:
      if (data->data_cache->voltage_now == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->voltage_now;
      break;
    case METABATTERY_TYPES_VOLTAGE_OCV:
      if (data->data_cache->voltage_ocv == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->voltage_ocv;
      break;
    case METABATTERY_TYPES_CHARGE_FULL:
      if (data->data_cache->charge_full == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->charge_full;
      break;
    case METABATTERY_TYPES_CURRENT_NOW:
      if (data->data_cache->current_now == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->current_now;
      break;
    case METABATTERY_TYPES_CURRENT_AVG:
      if (data->data_cache->current_avg == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->current_avg;
      break;
    case METABATTERY_TYPES_POWER_AVG:
      if (data->data_cache->power_avg == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->power_avg;
      break;
    case METABATTERY_TYPES_CHARGE_COUNTER:
      if (data->data_cache->charge_counter == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->charge_counter;
      break;
    case METABATTERY_TYPES_CYCLE_COUNT:
      if (data->data_cache->cycle_count == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->cycle_count;
      break;
    case METABATTERY_IS_FULL:
      if (data->data_cache->is_full == METABATTERY_INVALID_CACHE_VALUE)
        return -ENODATA;
      *val = data->data_cache->is_full;
      break;
    default:
      return -EINVAL;
  }
  return 0;
}

static int metabattery_set_cached_values(struct metabattery_data* data) {
  // Batch update the values
  metabattery_batch_update_t batch_update;
  int ret = metabattery_batch_xfer(data, METABATTERY_TYPES_BATCH_UPDATE, data->virtual_battery_id, &batch_update);

  if (ret == 0) {
    // Update cache with batch update values
    data->data_cache->capacity = batch_update.capacity_raw;
    data->data_cache->temperature = batch_update.temperature_raw;
    data->data_cache->mix_soc = batch_update.mix_soc_raw;
    data->data_cache->rep_soc = batch_update.rep_soc_raw;
    data->data_cache->health = batch_update.health_raw;
    data->data_cache->voltage_now = batch_update.voltage_now_raw;
    data->data_cache->voltage_ocv = batch_update.voltage_ocv_raw;
    data->data_cache->charge_full = batch_update.charge_full_raw;
    data->data_cache->current_now = batch_update.current_now_raw;
    data->data_cache->current_avg = batch_update.current_avg_raw;
    data->data_cache->power_avg = batch_update.power_avg_raw;
    data->data_cache->charge_counter = batch_update.charge_counter_raw;
    data->data_cache->cycle_count = batch_update.cycle_count_raw;
    data->data_cache->is_full = batch_update.is_full_raw;

    // Update cache timestamp
    data->data_cache->timestamp = jiffies_to_msecs(jiffies);

    return 0;
  }

  return ret;
}

static int metabattery_get_data(
    struct metabattery_data* data,
    uint16_t metabattery_type,
    uint16_t battery_id,
    int* val) {
  int ret = 0;

  // Try to use cached value if available
  if (data->caching_enabled &&
        battery_id == data->virtual_battery_id &&
        metabattery_data_is_cached(metabattery_type)) {
    if (metabattery_get_cached_value(data, metabattery_type, val) == 0) {
      // return 0 if there is a cache hit
      data->cache_hits++;
      return 0;
    } else if (metabattery_set_cached_values(data) == 0 &&
          metabattery_get_cached_value(data, metabattery_type, val) == 0) {
      // update cache values and return 0 if there is a cache hit
      return 0;
    }
  }

  ret = metabattery_xfer(data, 1, metabattery_type, battery_id, val);

  return ret;
}

static int metabattery_set_data(
    struct metabattery_data* data,
    uint16_t metabattery_type,
    uint16_t battery_id,
    int* val) {
  int ret = metabattery_xfer(data, 0, metabattery_type, battery_id, val);
  return ret;
}

static int metabattery_get_rsense(struct metabattery_data* data, int battery_id) {
  int ret = 0;
  int regval;

  if (battery_id >= MAX_NUM_BATTERIES) {
    // Map common ID to batt 0, which is always valid
    battery_id = 0;
  }

  if (data->rsense[battery_id] == RSENSE_INVALID_VALUE) {
    ret = metabattery_get_data(data, METABATTERY_RSENSE, battery_id, &regval);
    if (ret) {
      dev_err(
          data->dev,
          "Failed to get rsense from metabattery, using default rsense=%d\n",
          DEFAULT_RSENSE);
      // Return placeholder value, but don't store it
      return DEFAULT_RSENSE;
    } else {
      // Succeeded, save the value
      data->rsense[battery_id] = regval;
    }
  }
  return data->rsense[battery_id];
}

static int metabattery_set_manual_charging(
    struct metabattery_data* data,
    uint16_t metabattery_type,
    uint16_t battery_id,
    int* val) {
  int ret = 0;
  ret = metabattery_set_data(data, metabattery_type, battery_id, val);
  if (ret) {
    dev_err(
        data->dev,
        "%s : Failed to toggle manual charging on toggle=%d ret=%d\n",
        __func__,
        *val,
        ret);
  } else {
    data->man_charge_enabled = *val;
  }
  return ret;
}

static int metabattery_get_temp(struct metabattery_data* data, int battery_id, int* temp) {
  int ret = 0;

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
  if (battery_id == data->virtual_battery_id &&
      data->emul_virtual_battery_temperature > INVALID_EMUL_BATTERY_TEMPERATURE) {
    *temp = data->emul_virtual_battery_temperature;
    return 0;
  }
#endif

  // MCU returns decidegrees
  ret = metabattery_get_data(data, METABATTERY_TYPES_TEMP, battery_id, temp);
  if (ret) {
    // if we fail to read the virtual battery's temperature, then return the cached value
    if (battery_id == data->virtual_battery_id && data->virtual_battery_temperature_cache_time) {
      *temp = data->virtual_battery_temperature_cache;
      dev_dbg(
          data->dev,
          "%s: Used cached virtual battery temperature=%d from time=%ums\n",
          __func__,
          *temp,
          jiffies_to_msecs(data->virtual_battery_temperature_cache_time));
      ret = 0;
    }
  } else if (battery_id == data->virtual_battery_id) {
    // if we successfully read the virtual battery's temperature, then cache it
    data->virtual_battery_temperature_cache = *temp;
    data->virtual_battery_temperature_cache_time = jiffies;
  }

  // Sanity check: reject temperatures outside valid battery range
  // to prevent garbage values from triggering spurious thermal shutdowns
  if (!ret && (*temp < METABATTERY_TEMP_MIN_VALID_DECIDEGC ||
               *temp > METABATTERY_TEMP_MAX_VALID_DECIDEGC)) {
    dev_err(data->dev,
        "%s: temperature %d decidegrees out of valid range [%d, %d], rejecting\n",
        __func__, *temp,
        METABATTERY_TEMP_MIN_VALID_DECIDEGC,
        METABATTERY_TEMP_MAX_VALID_DECIDEGC);
    ret = -EINVAL;
  }

  return ret;
}

static int metabattery_get_temp_mc(void* data, int* state) {
  int ret = 0;
  struct metabattery_tz_data* tz_data = NULL;
  struct metabattery_data* dev_data = NULL;

  tz_data = (struct metabattery_tz_data*)data;
  if (!tz_data) {
    return -EINVAL;
  }
  dev_data = (struct metabattery_data*)tz_data->data;
  if (!dev_data) {
    return -EINVAL;
  }

  ret = metabattery_get_temp(dev_data, tz_data->battery_id, state);
  if (!ret)
    *state = TEMP_CENTIGRADE_TO_MC(*state);
  return ret;
}

static int metabattery_get_capacity(struct metabattery_data* data, int battery_id, int* capacity) {
  int ret = 0;
  int intval = 0;

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
  if (battery_id == data->virtual_battery_id &&
      data->emul_virtual_battery_capacity > INVALID_EMUL_BATTERY_CAPACITY) {
    *capacity = data->emul_virtual_battery_capacity;
    return 0;
  }
#endif

  ret = metabattery_get_data(data, METABATTERY_TYPES_CAPACITY, battery_id, capacity);
  if (ret) {
    // if we fail to read the virtual battery's capacity, then return the cached value
    if (battery_id == data->virtual_battery_id && data->virtual_battery_capacity_cache_time) {
      *capacity = data->virtual_battery_capacity_cache;
      dev_dbg(
          data->dev,
          "%s: Used cached virtual battery capacity=%d from time=%ums\n",
          __func__,
          *capacity,
          jiffies_to_msecs(data->virtual_battery_capacity_cache_time));
      return 0;
    }
  } else if (battery_id == data->virtual_battery_id) {
    // if we successfully read the virtual battery's capacity, then cache it
    data->virtual_battery_capacity_cache = *capacity;
    data->virtual_battery_capacity_cache_time = jiffies;
  }

  if (data->retail_demo_enabled) {
    int retail_demo_battery_id = battery_id;
    // For devices with multiple batteries we should utilize common battery id to control all of them together
    if (data->battery_count > 1) {
      retail_demo_battery_id = METABATTERY_BATTERY_ID_COMMON;
    }
    // if retail demo is enabled, then we need to enable or disable manual charging based on
    // capacity
    if (!data->man_charge_enabled && *capacity > METABATTERY_MAN_CHARGE_THRESHOLD) {
      intval = 1;
      ret |= metabattery_set_manual_charging(
          data, METABATTERY_MANUAL_CHARGING_ON, retail_demo_battery_id, &intval);
    } else if (data->man_charge_enabled && *capacity <= METABATTERY_MAN_CHARGE_THRESHOLD) {
      intval = 0;
      ret |= metabattery_set_manual_charging(
          data, METABATTERY_MANUAL_CHARGING_ON, retail_demo_battery_id, &intval);
    }
  }

  return ret;
}

static int metabattery_get_mix_soc(struct metabattery_data* data, int battery_id, int* mix_soc) {
  int ret = 0;

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
  if (battery_id == data->virtual_battery_id &&
      data->emul_virtual_battery_mix_soc > INVALID_EMUL_BATTERY_MIX_SOC) {
    *mix_soc = data->emul_virtual_battery_mix_soc;
    return 0;
  }
#endif

  ret = metabattery_get_data(data, METABATTERY_MIX_SOC, battery_id, mix_soc);
  if (ret) {
    // if we fail to read the virtual battery's mixsoc, then return the cached value
    if (battery_id == data->virtual_battery_id && data->virtual_battery_mix_soc_cache_time) {
      *mix_soc = data->virtual_battery_mix_soc_cache;
      dev_dbg(
          data->dev,
          "%s: Used cached virtual battery mixsoc=%d from time=%ums\n",
          __func__,
          *mix_soc,
          jiffies_to_msecs(data->virtual_battery_mix_soc_cache_time));
      ret = 0;
    }
  } else if (battery_id == data->virtual_battery_id) {
    // if we successfully read the virtual battery's mixsoc, then cache it
    data->virtual_battery_mix_soc_cache = *mix_soc;
    data->virtual_battery_mix_soc_cache_time = jiffies;
  }
  return ret;
}

static int
metabattery_is_battery_present(struct metabattery_data* data, int battery_id, int* is_present) {
  int ret = 0;

  if (data == NULL) {
    *is_present = 0;
    return -EINVAL;
  }

  if (data->battery_present) {
    *is_present = 1;
    return 0;
  }

  ret = metabattery_get_data(data, METABATTERY_TYPES_PRESENT, battery_id, is_present);
  if (!ret) {
    if (*is_present) {
      int capacity = 0, voltage = 0;
      int cap_ret = metabattery_get_data(
          data, METABATTERY_TYPES_CAPACITY, battery_id, &capacity);
      int vol_ret = metabattery_get_data(
          data, METABATTERY_TYPES_VOLTAGE_NOW, battery_id, &voltage);
      if (!cap_ret && !vol_ret && capacity == 0 &&
          voltage > 0) {
        dev_warn(data->dev,
            "%s: capacity=0 at voltage=%duV, fuel gauge data invalid, "
            "reporting battery not present\n", __func__, voltage);
        *is_present = 0;
      }
    }
    data->battery_present = *is_present;
  } else {
    dev_err(data->dev,
        "%s: STP failed (%d), returning error\n", __func__, ret);
    *is_present = 0;
  }

  return ret;
}

static int read_serial_number(
    struct metabattery_data* data,
    uint16_t battery_id,
    char* serial_buffer,
    size_t buffer_size,
    bool is_sip_serial) {
  int ret;
  size_t buffer_index = 0;
  size_t max_data_size = buffer_size - 2; /* Reserve space for newline and null termination */
  uint16_t* reg_addresses;
  size_t num_regs;
  size_t reg_index = 0;
  char upper_byte;
  char lower_byte;

  /* Validate input parameters */
  if (!data || !serial_buffer || buffer_size < 2)
    return -EINVAL;

  /* Select register addresses based on serial number type */
  if (is_sip_serial) {
    reg_addresses = sip_serial_num_regs;
    num_regs = ARRAY_SIZE(sip_serial_num_regs);
  } else {
    reg_addresses = pack_serial_num_regs;
    num_regs = ARRAY_SIZE(pack_serial_num_regs);
  }

  /* Process each register using the individual register constants */
  for (reg_index = 0; reg_index < num_regs && buffer_index < max_data_size; reg_index++) {
    int reg_value;
    uint16_t current_reg_constant = reg_addresses[reg_index];

    /* Read register value using metabattery interface with individual register constant */
    ret = metabattery_get_data(data, current_reg_constant, battery_id, &reg_value);
    if (ret < 0) {
      dev_err(
          data->dev,
          "Failed to read %s register constant %d, ret=%d\n",
          is_sip_serial ? "SIP" : "PACK",
          current_reg_constant,
          ret);
      return ret;
    }

    /* Extract and store upper byte */
    upper_byte = (char)((reg_value >> 8) & 0xFF);
    serial_buffer[buffer_index++] = upper_byte;

    /* Store lower byte if buffer has space */
    if (buffer_index < max_data_size) {
      lower_byte = (char)(reg_value & 0xFF);
      serial_buffer[buffer_index++] = lower_byte;
    }
  }

  /* Add newline terminator */
  serial_buffer[buffer_index++] = '\n';

  /* Null terminate the string if there's space */
  if (buffer_index < buffer_size) {
    serial_buffer[buffer_index] = '\0';
  }
  return buffer_index;
}

static int metabattery_get_property(
    struct power_supply* psy,
    enum power_supply_property psp,
    union power_supply_propval* val) {
  int ret;
  uint16_t battery_id;
  int xfer_data = 0;
  int cap = 0;
  union power_supply_propval psp_val;
  bool is_full = false;
  struct device* dev = &psy->dev;
  struct metabattery_data* data = power_supply_get_drvdata(psy);

  // check which battery we are requesting info for
  if (!strncmp(dev->kobj.name, METABATTERY_COMMON_NAME, METABATTERY_COMMON_NAME_STRLN)) {
    battery_id = METABATTERY_BATTERY_ID_COMMON;
  } else if (
      strncmp(dev->kobj.name, METABATTERY_BATTERY_0_NAME, METABATTERY_BATTERY_NAME_STRLEN) == 0) {
    battery_id = METABATTERY_BATTERY_ID_0;
  } else if (
      strncmp(dev->kobj.name, METABATTERY_BATTERY_1_NAME, METABATTERY_BATTERY_NAME_STRLEN) == 0) {
    battery_id = METABATTERY_BATTERY_ID_1;
  } else {
    dev_err(dev, "%s : Invalid battery id\n", __func__);
    return -ENODEV;
  }

  switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      ret =
          metabattery_get_data(data, METABATTERY_IS_FULL, battery_id, &xfer_data);
      ret |= power_supply_get_property(data->dc_charger, POWER_SUPPLY_PROP_ONLINE, &psp_val);
      if (ret) {
        val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
        break;
      }
      is_full = (xfer_data == 1) ? true : false;

      val->intval =
          (psp_val.intval) ? POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_DISCHARGING;

      if (psp_val.intval && is_full)
        val->intval = POWER_SUPPLY_STATUS_FULL;
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
      // MCU returns microvolts
      ret = metabattery_get_data(data, METABATTERY_TYPES_VOLTAGE_NOW, battery_id, &xfer_data);
      if (ret)
        return ret;
      val->intval = xfer_data;
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_REP:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(data, METABATTERY_TYPES_VOLTAGE_REP, battery_id, &xfer_data);
      if (ret)
        return ret;
      // MCU returns unsigned microvolts
      val->intval = xfer_data;
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_AVG:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(data, METABATTERY_TYPES_VOLTAGE_AVG, battery_id, &xfer_data);
      if (ret)
        return ret;
      // MCU returns unsigned microvolts
      val->intval = xfer_data;
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(
          data, METABATTERY_TYPES_VOLTAGE_MIN_DESIGN, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_OCV:
      // MCU returns microvolts
      ret = metabattery_get_data(data, METABATTERY_TYPES_VOLTAGE_OCV, battery_id, &xfer_data);
      if (ret)
        return ret;
      val->intval = xfer_data;
      break;
    case POWER_SUPPLY_PROP_CAPACITY:
      ret = metabattery_get_capacity(data, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(
          data, METABATTERY_TYPES_CAPACITY_ALERT_MIN, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(
          data, METABATTERY_TYPES_CAPACITY_ALERT_MAX, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_HEALTH:
      ret = metabattery_get_data(data, METABATTERY_TYPES_HEALTH, battery_id, &xfer_data);
      if (ret) {
        val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
        break;
      }
      switch (xfer_data) {
        case METABATTERY_HEALTH_UNKNOWN:
          val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
          break;
        case METABATTERY_HEALTH_OVERHEAT:
          val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
          break;
        case METABATTERY_HEALTH_DEAD:
          val->intval = POWER_SUPPLY_HEALTH_DEAD;
          break;
        case METABATTERY_HEALTH_COLD:
          val->intval = POWER_SUPPLY_HEALTH_COLD;
          break;
        case METABATTERY_HEALTH_OVERVOLTAGE:
          val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
          break;
        case METABATTERY_HEALTH_UNSPEC_FAILURE:
          val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
          break;
        case METABATTERY_HEALTH_WATCHDOG_TIMER_EXPIRE:
          val->intval = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
          break;
        case METABATTERY_HEALTH_GOOD:
          val->intval = POWER_SUPPLY_HEALTH_GOOD;
          break;
        default:
          val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
      }
      break;
    case POWER_SUPPLY_PROP_TEMP:
      ret = metabattery_get_temp(data, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(data, METABATTERY_TYPES_TEMP_ALERT_MIN, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(data, METABATTERY_TYPES_TEMP_ALERT_MAX, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
      // MCU returns microamps
      ret = metabattery_get_data(data, METABATTERY_TYPES_CURRENT_NOW, battery_id, &xfer_data);
      if (ret)
        return ret;
      val->intval = xfer_data;
      break;
    case POWER_SUPPLY_PROP_CURRENT_REP:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret = metabattery_get_data(data, METABATTERY_TYPES_CURRENT_REP, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CURRENT_AVG:
      // MCU returns microamps
      ret = metabattery_get_data(data, METABATTERY_TYPES_CURRENT_AVG, battery_id, &xfer_data);
      if (ret)
        return ret;
      val->intval = xfer_data;
      break;
    case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      ret =
          metabattery_get_data(data, METABATTERY_TYPES_TIME_TO_EMPTY_AVG, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_TECHNOLOGY:
      val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
      break;
    case POWER_SUPPLY_PROP_CHARGE_COUNTER:
      // MCU returns microamp-hours for any battery ID
      ret = metabattery_get_data(data, METABATTERY_TYPES_CHARGE_COUNTER, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CHARGE_FULL:
      // MCU returns microamp-hours
      ret = metabattery_get_data(data, METABATTERY_TYPES_CHARGE_FULL, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CYCLE_COUNT:
      ret = metabattery_get_data(data, METABATTERY_TYPES_CYCLE_COUNT, battery_id, &val->intval);
      // MCU returns whole cycles
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_PRESENT:
      ret = metabattery_is_battery_present(data, battery_id, &val->intval);
      if (ret) {
        return ret;
      }
      break;
    case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
      // if the battery is not present, return unknown
      ret = metabattery_is_battery_present(data, battery_id, &xfer_data);
      if (ret || xfer_data == 0) {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
        break;
      }

      ret = metabattery_get_capacity(data, battery_id, &cap);

      // TODO: @gino: clean up return values from stp_interface_xfer (T213517037)
      if (ret == -ETIMEDOUT || ret == -EFAULT || ret == -EIO) {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
        break;
      } else if (ret) {
        return ret;
      }
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      ret =
          metabattery_get_data(data, METABATTERY_IS_FULL, METABATTERY_BATTERY_ID_0, &xfer_data);
      // TODO: @gino: clean up return values from stp_interface_xfer (T213517037)
      if (ret == -ETIMEDOUT || ret == -EFAULT || ret == -EIO) {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
        break;
      } else if (ret)
        return ret;
      is_full = (xfer_data == 1) ? true : false;
      // Debounce is_full to prevent oscillation at top of charge.
      // MCU aggregates is_full across all cells (T264284761)
      if (is_full && cap >= 100) {
        data->full_seen = true;
      } else if (cap < 100) {
        data->full_seen = false;
      }

      if (cap <= METABATTERY_CAP_CRITICAL_LEVEL) {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
        dev_err(data->dev, "%s : Battery capacity was set to critical, device may shut down", __func__);
      } else if (cap <= METABATTERY_CAP_LOW_LEVEL) {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
      } else if (is_full || data->full_seen) {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
      } else if (cap <= METABATTERY_CAP_HIGH_LEVEL) {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
      } else {
        val->intval = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
      }
      break;
    case POWER_SUPPLY_PROP_POWER_NOW:
      // use battery 0 for common battery
      // TODO: @gino: check if METABATTERY_BATTERY_ID_0 is good enough for this (T213497484)
      if (battery_id == METABATTERY_BATTERY_ID_COMMON) {
        battery_id = METABATTERY_BATTERY_ID_0;
      }
      // MCU returns signed microwatts
      ret = metabattery_get_data(data, METABATTERY_TYPES_POWER_NOW, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_POWER_AVG:
      // MCU returns microwatts
      ret = metabattery_get_data(data, METABATTERY_TYPES_POWER_AVG, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CAPACITY_MIX_SOC:
      ret = metabattery_get_mix_soc(data, battery_id, &val->intval);
      if (ret)
        return ret;
      break;
    default:
      return -EINVAL;
  }
  return 0;
}

static struct device_attribute metabattery_attrs[] = {
    METABATTERY_ATTR(metabattery_program_nvm),
    METABATTERY_ATTR(nvm_updates_remaining),
    METABATTERY_ATTR(voltage_pack_now),
    METABATTERY_ATTR(coulomb_counter),
    METABATTERY_ATTR(charge_full_nom),
    METABATTERY_ATTR(rsense),
    METABATTERY_ATTR(voltage_alert_max),
    METABATTERY_ATTR(voltage_alert_min),
    METABATTERY_ATTR(current_alert_max),
    METABATTERY_ATTR(current_alert_min),
    METABATTERY_ATTR(timer),
    METABATTERY_ATTR(timerh),
    METABATTERY_ATTR(battery_chgstat),
    METABATTERY_ATTR(batt_status),
    METABATTERY_ATTR(prot_status),
    METABATTERY_ATTR(prot_alrt),
    METABATTERY_ATTR(fet_status),
    METABATTERY_ATTR(batt_config),
    METABATTERY_ATTR(batt_config2),
    METABATTERY_ATTR(comm_status),
    METABATTERY_ATTR(slack),
    METABATTERY_ATTR(ini_rev),
    METABATTERY_ATTR(sip_sn),
    METABATTERY_ATTR(pack_sn),
    METABATTERY_ATTR(bmu_smt_date),
    METABATTERY_ATTR(full_cap),
    METABATTERY_ATTR(av_cap),
    METABATTERY_ATTR(av_soc),
    METABATTERY_ATTR(mix_cap),
    METABATTERY_ATTR(mix_soc),
    METABATTERY_ATTR(vfrem_cap),
    METABATTERY_ATTR(vf_soc),
    METABATTERY_ATTR(q_residual),
    METABATTERY_ATTR(qr_table_00),
    METABATTERY_ATTR(qr_table_10),
    METABATTERY_ATTR(qr_table_20),
    METABATTERY_ATTR(qr_table_30),
    METABATTERY_ATTR(rcomp0),
    METABATTERY_ATTR(temp_co),
    METABATTERY_ATTR(lock),
    METABATTERY_ATTR(suspend_battery_pct),
    METABATTERY_ATTR(suspend_charge_counter),
    METABATTERY_ATTR(suspend_voltage),
    METABATTERY_ATTR(change_counter_cp),
    METABATTERY_ATTR(change_counter_dropout),
    METABATTERY_ATTR(change_counter_ovp),
    METABATTERY_ATTR(change_counter_occp),
    METABATTERY_ATTR(cycle_count_frac),
    METABATTERY_ATTR(controlled_charge_on),
    METABATTERY_ATTR(learn_stage),
    METABATTERY_ATTR(timer_seconds),
    METABATTERY_ATTR(i2c_read_failure_count),
    METABATTERY_ATTR(i2c_write_failure_count),
    METABATTERY_ATTR(ncgain),
    METABATTERY_ATTR(rcell),
    METABATTERY_ATTR(manual_charging_on),
    METABATTERY_ATTR(last_batt_status),
    METABATTERY_ATTR(last_prot_status),
    METABATTERY_ATTR(rep_soc),
    METABATTERY_ATTR(meta_soc),
    METABATTERY_ATTR(meta_soc_init_val),
    METABATTERY_ATTR(meta_soc_init_time),
    METABATTERY_ATTR(meta_soc_enabled),
    METABATTERY_ATTR(meta_soc_init),
    METABATTERY_ATTR(meta_soc_usoc),
    METABATTERY_ATTR(meta_soc_eoc),
    METABATTERY_ATTR(meta_soc_eod),
    METABATTERY_ATTR(meta_soc_low_batt_shutdown),
    METABATTERY_ATTR(trim1),
    METABATTERY_ATTR(target_chg_voltage),
    METABATTERY_ATTR(target_chg_current),
    METABATTERY_ATTR(batt_cap_low_lvl),
    METABATTERY_ATTR(is_full),
    METABATTERY_ATTR(block_discharge),
    METABATTERY_ATTR(full_cap_current_mah),
    METABATTERY_ATTR(full_cap_nominal_mah),
    METABATTERY_ATTR(design_cap),
    METABATTERY_ATTR(n_design_cap),
    METABATTERY_ATTR(n_cycles),
    METABATTERY_ATTR(n_full_cap_nom),
    METABATTERY_ATTR(n_full_cap_rep),
    METABATTERY_ATTR(n_timerh),
    METABATTERY_ATTR(dqacc),
    METABATTERY_ATTR(dpacc),
    METABATTERY_ATTR(age),
    METABATTERY_ATTR(is_virtual_battery),
    METABATTERY_ATTR(low_volt_comparator_triggered),
    METABATTERY_ATTR(battery_driver_id),
    METABATTERY_ATTR(hot_car_state),
    METABATTERY_ATTR(enable_charge_discharge),
    METABATTERY_ATTR(enable_regulator),
    METABATTERY_ATTR(manual_set_voltage),
    METABATTERY_ATTR(clear_manual_charge_control),
    METABATTERY_ATTR(retail_demo_enabled),
    METABATTERY_ATTR(manual_set_current),
    METABATTERY_ATTR(set_parallel_battery_management),
    METABATTERY_ATTR(clear_block_charging),
    METABATTERY_ATTR(get_block_charging),
    METABATTERY_ATTR(get_allow_charge_block),
    METABATTERY_ATTR(limit_charging_rate),
    METABATTERY_ATTR(unmapped_capacity),
    METABATTERY_ATTR(dietemp),
    METABATTERY_ATTR(fstat2),
    METABATTERY_ATTR(fstat),
    METABATTERY_ATTR(fotpstat),
    METABATTERY_ATTR(fprotstat),
    METABATTERY_ATTR(hprotcfg),
    METABATTERY_ATTR(n_battstatus),
    METABATTERY_ATTR(meta_soc_version),
    METABATTERY_ATTR(meta_soc_config_id),
    METABATTERY_ATTR(meta_soc_low_volt_comp_tripped),
    METABATTERY_ATTR(meta_soc_usoc_filtered),
    METABATTERY_ATTR(meta_soc_peak_voltage_droop_penalty),
    METABATTERY_ATTR(meta_soc_remaining_capacity),
    METABATTERY_ATTR(n_nv_config2),
    METABATTERY_ATTR(ttf_config_reg),
    METABATTERY_ATTR(user_memory_reg),
    METABATTERY_ATTR(sc_ocv_lim_reg),
    METABATTERY_ATTR(manfctr_date_reg),
    METABATTERY_ATTR(first_used_reg),
    METABATTERY_ATTR(serial_num_1_reg),
    METABATTERY_ATTR(serial_num_2_reg),
    METABATTERY_ATTR(device_name_0_reg),
    METABATTERY_ATTR(device_name_1_reg),
    METABATTERY_ATTR(manfctr_name_0_reg),
    METABATTERY_ATTR(manfctr_name_1_reg),
    METABATTERY_ATTR(manfctr_name_2_reg),
    METABATTERY_ATTR(fg_config_update_algo_ver),
    METABATTERY_ATTR(fg_config_update_cntr),
    METABATTERY_ATTR(fg_config_update_fail_cntr),
    METABATTERY_ATTR(fg_config_update_success_cntr),
    METABATTERY_ATTR(fg_config_update_write_fail_reg),
    METABATTERY_ATTR(fg_config_update_write_fail_reg_val),
    METABATTERY_ATTR(fg_config_update_entry_reason),
    METABATTERY_ATTR(caching_enabled),
    METABATTERY_ATTR(caching_refresh_interval_ms),
    METABATTERY_ATTR(batch_update),
    METABATTERY_ATTR(por_count),
    METABATTERY_ATTR(cache_hits),
    METABATTERY_ATTR(mixsoc_corrected),
    METABATTERY_ATTR(change_counter_odcp),
};

bool is_battery_id_common_supported(int id)
{
   bool supported = false;

   switch(id) {
     case METABATTERY_PROGRAM_NVM:
     case METABATTERY_COULOMB_COUNTER:
     case METABATTERY_RSENSE:
     case METABATTERY_MIX_SOC:
     case METABATTERY_SUSPEND_BATTERY_PCT:
     case METABATTERY_SUSPEND_CHARGE_COUNTER:
     case METABATTERY_SUSPEND_VOLTAGE:
     case METABATTERY_CP_CHANGE_COUNTER:
     case METABATTERY_DROPOUT_CHANGE_COUNTER:
     case METABATTERY_OVP_CHANGE_COUNTER:
     case METABATTERY_OCCP_CHANGE_COUNTER:
     case METABATTERY_CONTROLLED_CHARGE_ON:
     case METABATTERY_TIMER_SECONDS:
     case METABATTERY_MANUAL_CHARGING_ON:
     case METABATTERY_LAST_BATT_STATUS:
     case METABATTERY_LAST_PROT_STATUS:
     case METABATTERY_REP_SOC:
     case METABATTERY_META_SOC:
     case METABATTERY_META_SOC_INIT_VAL:
     case METABATTERY_META_SOC_INIT_TIME:
     case METABATTERY_META_SOC_ENABLED:
     case METABATTERY_META_SOC_INIT:
     case METABATTERY_META_SOC_USOC:
     case METABATTERY_META_SOC_EOC:
     case METABATTERY_META_SOC_EOD:
     case METABATTERY_META_SOC_LOW_BATT_SHUTDOWN:
     case METABATTERY_BATT_CAP_LOW_LVL:
     case METABATTERY_IS_FULL:
     case METABATTERY_IS_VIRTUAL_BATTERY:
     case METABATTERY_LOW_VOLT_COMPARATOR_TRIGGERED:
     case METABATTERY_BATTERY_DRIVER_ID:
     case METABATTERY_HOT_CAR_STATE:
     case METABATTERY_ENABLE_CHARGE_DISCHARGE:
     case METABATTERY_ENABLE_REGULATOR:
     case METABATTERY_MANUAL_SET_VOLTAGE:
     case METABATTERY_CLEAR_MANUAL_CHARGE_CONTROL:
     case METABATTERY_RETAIL_DEMO_ENABLED:
     case METABATTERY_MANUAL_SET_CURRENT:
     case METABATTERY_SET_PARALLEL_BATTERY_MANAGEMENT:
     case METABATTERY_CLEAR_BLOCK_CHARGING:
     case METABATTERY_LIMIT_CHARGING_RATE:
     case METABATTERY_UNMAPPED_CAPACITY:
     case METABATTERY_META_SOC_VERSION:
     case METABATTERY_META_SOC_CONFIG_ID:
     case METABATTERY_META_SOC_LOW_VOLT_COMP_TRIPPED:
     case METABATTERY_META_SOC_USOC_FILTERED:
     case METABATTERY_META_SOC_PEAK_VOLTAGE_DROOP_PENALTY:
     case METABATTERY_META_SOC_REMAINING_CAPACITY:
     case METABATTERY_CACHING_ENABLED:
     case METABATTERY_CACHING_REFRESH_RATE:
     case METABATTERY_CACHE_HITS:
     case METABATTERY_POR_COUNT:
     case METABATTERY_TYPES_CAPACITY:
     case METABATTERY_TYPES_CHARGE_COUNTER:
     case METABATTERY_TYPES_CHARGE_FULL:
     case METABATTERY_TYPES_VOLTAGE_NOW:
     case METABATTERY_TYPES_VOLTAGE_REP:
     case METABATTERY_TYPES_VOLTAGE_AVG:
     case METABATTERY_TYPES_VOLTAGE_OCV:
     case METABATTERY_TYPES_CURRENT_NOW:
     case METABATTERY_TYPES_CURRENT_REP:
     case METABATTERY_TYPES_CURRENT_AVG:
     case METABATTERY_TYPES_POWER_NOW:
     case METABATTERY_TYPES_POWER_AVG:
     case METABATTERY_TYPES_TEMP:
     case METABATTERY_TYPES_CYCLE_COUNT:
     case METABATTERY_TYPES_TEMP_ALERT_MAX:
     case METABATTERY_TYPES_TEMP_ALERT_MIN:
     case METABATTERY_TYPES_CAPACITY_ALERT_MAX:
     case METABATTERY_TYPES_CAPACITY_ALERT_MIN:
     case METABATTERY_TYPES_STATUS:
     case METABATTERY_TYPES_VOLTAGE_MIN_DESIGN:
     case METABATTERY_TYPES_HEALTH:
     case METABATTERY_TYPES_TIME_TO_EMPTY_AVG:
     case METABATTERY_TYPES_TECHNOLOGY:
     case METABATTERY_TYPES_PRESENT:
     case METABATTERY_TYPES_CAPACITY_LEVEL:
     case METABATTERY_TYPES_CAPACITY_MIX_SOC:
     case METABATTERY_TYPES_BATCH_UPDATE:
       supported = true;
       break;

     default:
       supported = false;
       break;
   }

   return supported;
}

ssize_t metabattery_show_attrs(struct device* dev, struct device_attribute* attr, char* buf) {
  struct power_supply* psy = dev_get_drvdata(dev);
  struct metabattery_data* data = power_supply_get_drvdata(psy);
  const ptrdiff_t offset = attr - metabattery_attrs;
  int ret = 0;
  int intval = 0;

  int battery_id = METABATTERY_BATTERY_ID_COMMON;

  metabattery_batch_update_t batch_update;
  memset(&batch_update, 0, sizeof(batch_update));

  // check which battery we are requesting info for
  if (!strncmp(dev->kobj.name, METABATTERY_COMMON_NAME, METABATTERY_COMMON_NAME_STRLN)) {
    battery_id = METABATTERY_BATTERY_ID_COMMON;
  } else if (
      strncmp(dev->kobj.name, METABATTERY_BATTERY_0_NAME, METABATTERY_BATTERY_NAME_STRLEN) == 0) {
    battery_id = METABATTERY_BATTERY_ID_0;
  } else if (
      strncmp(dev->kobj.name, METABATTERY_BATTERY_1_NAME, METABATTERY_BATTERY_NAME_STRLEN) == 0) {
    battery_id = METABATTERY_BATTERY_ID_1;
  } else {
    dev_err(dev, "%s : Invalid battery id\n", __func__);
    return -ENODEV;
  }

  if (battery_id == METABATTERY_BATTERY_ID_COMMON &&
      !is_battery_id_common_supported(offset))
    return -EOPNOTSUPP;

  switch (offset) {
    case METABATTERY_CURRENT_ALERT_MAX:
      // MCU returns microamps.
      ret = metabattery_get_data(data, METABATTERY_CURRENT_ALERT_MAX, battery_id, &intval);
      if (ret)
        return ret;
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_CURRENT_ALERT_MIN:
      // MCU returns microamps.
      ret = metabattery_get_data(data, METABATTERY_CURRENT_ALERT_MIN, battery_id, &intval);
      if (ret)
        return ret;
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_SIP_SN: {
      char sip_sn_buf[SIP_SERIAL_NUMBER_SIZE] = {0};

      // Use the read_serial_number function to read the SIP serial number
      ret = read_serial_number(data, battery_id, sip_sn_buf, sizeof(sip_sn_buf), true);
      if (ret <= 0) {
        dev_err(data->dev, "Failed to read SIP_SN, ret=%d\n", ret);
        return ret;
      }

      // Format the serial number into the output buffer using snprintf
      // This ensures proper formatting for the sysfs node
      ret = snprintf(
          buf,
          SIP_SERIAL_NUMBER_SIZE,
          "%.*s",
          ret,
          sip_sn_buf); // Include newline for proper terminal display

    } break;
    case METABATTERY_PACK_SN: {
      char pack_sn_buf[PACK_SERIAL_NUMBER_SIZE] = {0};

      // Use the read_serial_number function to read the PACK serial number
      ret = read_serial_number(data, battery_id, pack_sn_buf, sizeof(pack_sn_buf), false);
      if (ret <= 0) {
        dev_err(data->dev, "Failed to read PACK_SN, ret=%d\n", ret);
        return ret;
      }

      // Format the serial number into the output buffer using snprintf
      // This ensures proper formatting for the sysfs node
      ret = snprintf(
          buf,
          PACK_SERIAL_NUMBER_SIZE,
          "%.*s",
          ret,
          pack_sn_buf); // Include newline for proper terminal display

    } break;
    case METABATTERY_SUSPEND_BATTERY_PCT:
    case METABATTERY_SUSPEND_CHARGE_COUNTER:
    case METABATTERY_SUSPEND_VOLTAGE:
    case METABATTERY_CP_CHANGE_COUNTER:
    case METABATTERY_DROPOUT_CHANGE_COUNTER:
    case METABATTERY_LAST_BATT_STATUS:
    case METABATTERY_LAST_PROT_STATUS:
      ret = metabattery_get_data(data, offset, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_REMAINING_NVM_UPDATES:

      ret = -EIO;
      break;
    case METABATTERY_VOLTAGE_PACK_NOW:
      ret = metabattery_get_data(data, METABATTERY_VOLTAGE_PACK_NOW, battery_id, &intval);
      if (!ret) {
        // MCU returns microamps
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_COULOMB_COUNTER:
      ret = metabattery_get_data(data, METABATTERY_COULOMB_COUNTER, battery_id, &intval);
      if (!ret) {
        // For all batt IDs the MCU returns microamp-hours
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_CHARGE_FULL_NOM:
      ret = metabattery_get_data(data, METABATTERY_CHARGE_FULL_NOM, battery_id, &intval);
      if (!ret) {
        // MCU returns uAh
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_RSENSE:
      intval = metabattery_get_rsense(data, battery_id);
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_VOLTAGE_ALERT_MAX:
      ret = metabattery_get_data(data, METABATTERY_VOLTAGE_ALERT_MAX, battery_id, &intval);
      if (!ret) {
        // MCU returns microvolts
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_VOLTAGE_ALERT_MIN:
      ret = metabattery_get_data(data, METABATTERY_VOLTAGE_ALERT_MIN, battery_id, &intval);
      if (!ret) {
        // MCU returns microvolts
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_TIMER:
      metabattery_get_data(data, METABATTERY_TIMER, battery_id, &intval);
      // MCU returns milliseconds
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_TIMERH:
      metabattery_get_data(data, METABATTERY_TIMERH, battery_id, &intval);
      // MCU returns decihours
      ret = snprintf(buf, MAX_INT_DIGITS, "%d.%d\n", intval / 10, intval % 10);
      break;
    case METABATTERY_BATTERY_CHGSTAT:
      ret = metabattery_get_data(data, METABATTERY_BATTERY_CHGSTAT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf,
            35,
            "Dropout:%d CP:%d CT:%d CC:%d CV:%d\n",
            intval & BIT_STATUS_DROPOUT ? 1 : 0,
            intval & BIT_STATUS_CP ? 1 : 0,
            intval & BIT_STATUS_CT ? 1 : 0,
            intval & BIT_STATUS_CC ? 1 : 0,
            intval & BIT_STATUS_CV ? 1 : 0);
      }
      break;
    case METABATTERY_BATT_STATUS:
      ret = metabattery_get_data(data, METABATTERY_BATT_STATUS, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_PROT_STATUS:
      ret = metabattery_get_data(data, METABATTERY_PROT_STATUS, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_PROT_ALERT:
      ret = metabattery_get_data(data, METABATTERY_PROT_ALERT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_FET_STATUS:
      ret = metabattery_get_data(data, METABATTERY_FET_STATUS, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_BATT_CONFIG:
      ret = metabattery_get_data(data, METABATTERY_BATT_CONFIG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_BATT_CONFIG2:
      ret = metabattery_get_data(data, METABATTERY_BATT_CONFIG2, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_COMM_STATUS:
      ret = metabattery_get_data(data, METABATTERY_COMM_STATUS, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_SLACK:
      ret = metabattery_get_data(data, METABATTERY_SLACK, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_INI_REV:
      ret = metabattery_get_data(data, METABATTERY_INI_REV, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_BMU_SMT_DATE:
      ret = metabattery_get_data(data, METABATTERY_BMU_SMT_DATE, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_FULL_CAP:
      // Conversion to uAh is now done on the MCU
      ret = metabattery_get_data(data, METABATTERY_FULL_CAP, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_AV_CAP:
      ret = metabattery_get_data(data, METABATTERY_AV_CAP, battery_id, &intval);
      if (!ret) {
        // Conversion to uAh is now done on the MCU
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_AC_SOC:
      ret = metabattery_get_data(data, METABATTERY_AC_SOC, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_MIX_CAP:
      ret = metabattery_get_data(data, METABATTERY_MIX_CAP, battery_id, &intval);
      if (!ret) {
        // Conversion to uAh is now done on the MCU
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_MIX_SOC:
      ret = metabattery_get_mix_soc(data, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_REP_SOC:
      ret = metabattery_get_data(data, METABATTERY_REP_SOC, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC:
      ret = metabattery_get_data(data, METABATTERY_META_SOC, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_INIT_VAL:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_INIT_VAL, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_INIT_TIME:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_INIT_TIME, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_ENABLED:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_ENABLED, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_VERSION:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_VERSION, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_INIT:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_INIT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_USOC:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_USOC, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_EOC:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_EOC, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_EOD:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_EOD, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_CONFIG_ID:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_CONFIG_ID, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_LOW_VOLT_COMP_TRIPPED:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_LOW_VOLT_COMP_TRIPPED, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_USOC_FILTERED:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_USOC_FILTERED, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_PEAK_VOLTAGE_DROOP_PENALTY:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_PEAK_VOLTAGE_DROOP_PENALTY, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_META_SOC_REMAINING_CAPACITY:
      ret = metabattery_get_data(data, METABATTERY_META_SOC_REMAINING_CAPACITY, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_VFREM_CAP:
      ret = metabattery_get_data(data, METABATTERY_VFREM_CAP, battery_id, &intval);
      if (!ret) {
        // Conversion to uAh is now done on the MCU
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_VF_SOC:
      ret = metabattery_get_data(data, METABATTERY_VF_SOC, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_Q_RESIDUAL:
      ret = metabattery_get_data(data, METABATTERY_Q_RESIDUAL, battery_id, &intval);
      if (!ret) {
        // Conversion to uAh is now done on the MCU
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_RCOMP0:
      ret = metabattery_get_data(data, METABATTERY_RCOMP0, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_TEMP_CO:
      ret = metabattery_get_data(data, METABATTERY_TEMP_CO, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_LOCK:
      ret = metabattery_get_data(data, METABATTERY_LOCK, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_CYCLE_COUNT_FRAC:
      ret = metabattery_get_data(data, METABATTERY_CYCLE_COUNT_FRAC, battery_id, &intval);
      if (!ret) {
        // MCU returns hundredths of cycle
        ret = snprintf(buf, MAX_INT_DIGITS, "%d.%d\n", intval / 100, intval % 100);
      }
      break;
    case METABATTERY_LEARN_STAGE:
      ret = metabattery_get_data(data, METABATTERY_LEARN_STAGE, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_NCGAIN:
      ret = metabattery_get_data(data, METABATTERY_NCGAIN, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%x\n", intval);
      }
      break;
    case METABATTERY_RCELL:
      ret = metabattery_get_data(data, METABATTERY_RCELL, battery_id, &intval);
      if (!ret) {
        // MCU returns micro-ohms
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_TRIM1:
      ret = metabattery_get_data(data, METABATTERY_TRIM1, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_TARGET_CHG_VOLTAGE:
      ret = metabattery_get_data(data, METABATTERY_TARGET_CHG_VOLTAGE, battery_id, &intval);
      if (!ret) {
        // MCU returns unsigned microvolts
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_TARGET_CHG_CURRENT:
      ret = metabattery_get_data(data, METABATTERY_TARGET_CHG_CURRENT, battery_id, &intval);
      if (!ret) {
        // MCU returns microamps
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_BLOCK_DISCHARGE:
      ret = metabattery_get_data(data, METABATTERY_BLOCK_DISCHARGE, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_FULL_CAP_CURRENT_MAH:
      ret = metabattery_get_data(data, METABATTERY_FULL_CAP_CURRENT_MAH, battery_id, &intval);
      if (!ret) {
        // MCU returns mAh
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_FULL_CAP_NOMINAL_MAH:
      // Conversion to mAh is now done on the MCU
      ret = metabattery_get_data(data, METABATTERY_FULL_CAP_NOMINAL_MAH, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_DESIGN_CAP:
      ret = metabattery_get_data(data, METABATTERY_DESIGN_CAP, battery_id, &intval);
      if (!ret) {
        // Conversion to uAh is now done on the MCU
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_N_DESIGN_CAP:
      ret = metabattery_get_data(data, METABATTERY_N_DESIGN_CAP, battery_id, &intval);
      if (ret)
        return ret;
      // MCU returns microamp-hours
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_N_CYCLES:
      ret = metabattery_get_data(data, METABATTERY_N_CYCLES, battery_id, &intval);
      if (ret)
        return ret;
      // MCU returns whole cycles
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_N_FULL_CAP_NOM:
      ret = metabattery_get_data(data, METABATTERY_N_FULL_CAP_NOM, battery_id, &intval);
      if(ret)
        return ret;
      // MCU returns microamp-hours
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_N_FULL_CAP_REP:
      ret = metabattery_get_data(data, METABATTERY_N_FULL_CAP_REP, battery_id, &intval);
      if (ret)
        return ret;
      // MCU returns microamp-hours
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_N_TIMERH:
      ret = metabattery_get_data(data, METABATTERY_N_TIMERH, battery_id, &intval);
      if (!ret) {
        // MCU returns hours
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_DQACC:
      ret = metabattery_get_data(data, METABATTERY_DQACC, battery_id, &intval);
      if (ret)
        return ret;
      // MCU returns microamp-hours
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_DPACC:
      ret = metabattery_get_data(data, METABATTERY_DPACC, battery_id, &intval);
      if (!ret) {
        // MCU returns percent
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_AGE:
      ret = metabattery_get_data(data, METABATTERY_AGE, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_IS_VIRTUAL_BATTERY:
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", battery_id == data->virtual_battery_id);
      break;
    case METABATTERY_BATTERY_DRIVER_ID:
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", battery_id);
      break;
    case METABATTERY_MANUAL_CHARGING_ON:
      intval = (int)data->man_charge_enabled;
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_RETAIL_DEMO_ENABLED:
      intval = (int)data->retail_demo_enabled;
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_GET_BLOCK_CHARGING:
      ret = metabattery_get_data(data, METABATTERY_GET_BLOCK_CHARGING, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_GET_ALLOW_CHARGE_BLOCK:
      ret = metabattery_get_data(data, METABATTERY_GET_ALLOW_CHARGE_BLOCK, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_LIMIT_CHARGING_RATE:
      intval = (int)data->limit_charging_rate_enabled;
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      break;
    case METABATTERY_OVP_CHANGE_COUNTER:
      ret = metabattery_get_data(data, METABATTERY_OVP_CHANGE_COUNTER, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_OCCP_CHANGE_COUNTER:
      ret = metabattery_get_data(data, METABATTERY_OCCP_CHANGE_COUNTER, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_ODCP_CHANGE_COUNTER:
      ret = metabattery_get_data(data, METABATTERY_ODCP_CHANGE_COUNTER, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_QR_TABLE_00:
      ret = metabattery_get_data(data, METABATTERY_QR_TABLE_00, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_QR_TABLE_10:
      ret = metabattery_get_data(data, METABATTERY_QR_TABLE_10, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_QR_TABLE_20:
      ret = metabattery_get_data(data, METABATTERY_QR_TABLE_20, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_QR_TABLE_30:
      ret = metabattery_get_data(data, METABATTERY_QR_TABLE_30, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_UNMAPPED_CAPACITY:
      ret = metabattery_get_data(data, METABATTERY_UNMAPPED_CAPACITY, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_DIETEMP:
      ret = metabattery_get_data(data, METABATTERY_DIETEMP, battery_id, &intval);
      if (!ret) {
        // MCU returns decidegrees
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_FSTAT2:
      ret = metabattery_get_data(data, METABATTERY_FSTAT2, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_FSTAT:
      ret = metabattery_get_data(data, METABATTERY_FSTAT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_FOTPSTAT:
      ret = metabattery_get_data(data, METABATTERY_FOTPSTAT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_FPROTSTAT:
      ret = metabattery_get_data(data, METABATTERY_FPROTSTAT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_HPROTCFG:
      ret = metabattery_get_data(data, METABATTERY_HPROTCFG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_N_BATTSTATUS:
      ret = metabattery_get_data(data, METABATTERY_N_BATTSTATUS, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_IS_FULL:
      ret = metabattery_get_data(data, METABATTERY_IS_FULL, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_I2C_READ_FAILURE_COUNT:
      ret = metabattery_get_data(data, METABATTERY_I2C_READ_FAILURE_COUNT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_I2C_WRITE_FAILURE_COUNT:
      ret = metabattery_get_data(data, METABATTERY_I2C_WRITE_FAILURE_COUNT, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_N_NV_CONFIG2:
      ret = metabattery_get_data(data, METABATTERY_N_NV_CONFIG2, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_TTF_CONFIG_REG:
      ret = metabattery_get_data(data, METABATTERY_TTF_CONFIG_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_USER_MEMORY_REG:
      ret = metabattery_get_data(data, METABATTERY_USER_MEMORY_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_SC_OCV_LIM_REG:
      ret = metabattery_get_data(data, METABATTERY_SC_OCV_LIM_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_MANFCTR_DATE_REG:
      ret = metabattery_get_data(data, METABATTERY_MANFCTR_DATE_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_FIRST_USED_REG:
      ret = metabattery_get_data(data, METABATTERY_FIRST_USED_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_SERIAL_NUM_1_REG:
      ret = metabattery_get_data(data, METABATTERY_SERIAL_NUM_1_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_SERIAL_NUM_2_REG:
      ret = metabattery_get_data(data, METABATTERY_SERIAL_NUM_2_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_DEVICE_NAME_0_REG:
      ret = metabattery_get_data(data, METABATTERY_DEVICE_NAME_0_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_DEVICE_NAME_1_REG:
      ret = metabattery_get_data(data, METABATTERY_DEVICE_NAME_1_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_MANFCTR_NAME_0_REG:
      ret = metabattery_get_data(data, METABATTERY_MANFCTR_NAME_0_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_MANFCTR_NAME_1_REG:
      ret = metabattery_get_data(data, METABATTERY_MANFCTR_NAME_1_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_MANFCTR_NAME_2_REG:
      ret = metabattery_get_data(data, METABATTERY_MANFCTR_NAME_2_REG, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", intval);
      }
      break;
    case METABATTERY_FG_CONFIG_UPDATE_ALGO_VER:
    case METABATTERY_FG_CONFIG_UPDATE_CNTR:
    case METABATTERY_FG_CONFIG_UPDATE_FAIL_CNTR:
    case METABATTERY_FG_CONFIG_UPDATE_SUCCESS_CNTR:
    case METABATTERY_FG_CONFIG_UPDATE_WRITE_FAIL_REG:
    case METABATTERY_FG_CONFIG_UPDATE_WRITE_FAIL_REG_VAL:
    case METABATTERY_FG_CONFIG_UPDATE_ENTRY_REASON:
    case METABATTERY_POR_COUNT:
    case METABATTERY_MIXSOC_CORRECTED:
      ret = metabattery_get_data(data, offset, battery_id, &intval);
      if (!ret) {
        ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
      }
      break;
    case METABATTERY_CACHING_ENABLED:
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", data->caching_enabled ? 1 : 0);
      break;
    case METABATTERY_CACHING_REFRESH_RATE:
      ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", data->caching_refresh_interval_ms);
      break;
    case METABATTERY_CACHE_HITS:
      ret = snprintf(buf, MAX_INT_DIGITS, "%lu\n", data->cache_hits);
      break;
    case METABATTERY_TYPES_BATCH_UPDATE:
      metabattery_batch_xfer(data, METABATTERY_TYPES_BATCH_UPDATE, battery_id, &batch_update);
      ret = snprintf(
          buf,
          sizeof(batch_update) * 15, // large enough buffer
          "capacity_raw=%u\n"
          "temperature_raw=%d\n"
          "mix_soc_raw=%u\n"
          "rep_soc_raw=%u\n"
          "health_raw=%u\n"
          "voltage_now_raw=%d\n"
          "voltage_ocv_raw=%d\n"
          "charge_full_raw=%d\n"
          "current_now_raw=%d\n"
          "current_avg_raw=%d\n"
          "power_avg_raw=%d\n"
          "charge_counter_raw=%d\n"
          "cycle_count_raw=%u\n"
          "is_full_raw=%u\n",
          batch_update.capacity_raw,
          batch_update.temperature_raw,
          batch_update.mix_soc_raw,
          batch_update.rep_soc_raw,
          batch_update.health_raw,
          batch_update.voltage_now_raw,
          batch_update.voltage_ocv_raw,
          batch_update.charge_full_raw,
          batch_update.current_now_raw,
          batch_update.current_avg_raw,
          batch_update.power_avg_raw,
          batch_update.charge_counter_raw,
          batch_update.cycle_count_raw,
          batch_update.is_full_raw);
      break;
    default:
      return -EINVAL;
  }

  return ret;
}

ssize_t metabattery_store_attrs(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {
  struct power_supply* psy = dev_get_drvdata(dev);
  struct metabattery_data* data = power_supply_get_drvdata(psy);
  const ptrdiff_t offset = attr - metabattery_attrs;
  int ret = 0;
  int intval = 0;
  int battery_id = METABATTERY_BATTERY_ID_COMMON;

  // check which battery we are requesting info for
  if (!strncmp(dev->kobj.name, METABATTERY_COMMON_NAME, METABATTERY_COMMON_NAME_STRLN)) {
    battery_id = METABATTERY_BATTERY_ID_COMMON;
  } else if (
      strncmp(dev->kobj.name, METABATTERY_BATTERY_0_NAME, METABATTERY_BATTERY_NAME_STRLEN) == 0) {
    battery_id = METABATTERY_BATTERY_ID_0;
  } else if (
      strncmp(dev->kobj.name, METABATTERY_BATTERY_1_NAME, METABATTERY_BATTERY_NAME_STRLEN) == 0) {
    battery_id = METABATTERY_BATTERY_ID_1;
  } else {
    dev_err(dev, "%s : Invalid battery id\n", __func__);
    return -ENODEV;
  }

  switch (offset) {
    case METABATTERY_CONTROLLED_CHARGE_ON:
      ret = kstrtos32(buf, 10, &intval);
      ret = metabattery_set_data(data, METABATTERY_CONTROLLED_CHARGE_ON, battery_id, &intval);
      if (ret) {
        dev_err(
            dev,
            "%s : Failed to toggle controlled charge on toggle=%d ret=%d\n",
            __func__,
            intval,
            ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_MANUAL_CHARGING_ON:
      ret = kstrtos32(buf, 10, &intval);
      ret = metabattery_set_manual_charging(data, METABATTERY_MANUAL_CHARGING_ON, battery_id, &intval);
      if (ret) {
        dev_err(
            dev,
            "%s : Failed to toggle manual charge on toggle=%d ret=%d\n",
            __func__,
            intval,
            ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_RETAIL_DEMO_ENABLED:
      data->retail_demo_enabled = 1;
      ret = count;
      break;
    case METABATTERY_ENABLE_CHARGE_DISCHARGE:
      ret = kstrtos32(buf, 10, &intval);
      ret = metabattery_set_data(data, METABATTERY_ENABLE_CHARGE_DISCHARGE, battery_id, &intval);
      if (ret) {
        dev_err(
            dev,
            "%s : Failed to enable charge_discharge toggle=%d ret=%d\n",
            __func__,
            intval,
            ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_ENABLE_REGULATOR:
      ret = kstrtos32(buf, 10, &intval);
      ret = metabattery_set_data(data, METABATTERY_ENABLE_REGULATOR, battery_id, &intval);
      if (ret) {
        dev_err(dev, "%s : Failed to enable regulator toggle=%d ret=%d\n", __func__, intval, ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_MANUAL_SET_VOLTAGE:
      ret = kstrtos32(buf, 10, &intval);
      ret = metabattery_set_data(data, METABATTERY_MANUAL_SET_VOLTAGE, battery_id, &intval);
      if (ret) {
        dev_err(dev, "%s : Failed to manually set voltage val=%d ret=%d\n", __func__, intval, ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_CLEAR_MANUAL_CHARGE_CONTROL:
      ret = kstrtos32(buf, 10, &intval);
      ret =
          metabattery_set_data(data, METABATTERY_CLEAR_MANUAL_CHARGE_CONTROL, battery_id, &intval);
      if (ret) {
        dev_err(
            dev,
            "%s : Failed to clear manual charge control toggle=%d ret=%d\n",
            __func__,
            intval,
            ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_SET_PARALLEL_BATTERY_MANAGEMENT:
      ret = kstrtos32(buf, 10, &intval);
      ret |= metabattery_set_data(
          data, METABATTERY_SET_PARALLEL_BATTERY_MANAGEMENT, battery_id, &intval);
      if (ret) {
        dev_err(
            dev, "%s : Failed to set parallel batt mgmt toggle=%d ret=%d\n", __func__, intval, ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_CLEAR_BLOCK_CHARGING:
      ret = kstrtos32(buf, 10, &intval);
      ret |= metabattery_set_data(data, METABATTERY_CLEAR_BLOCK_CHARGING, battery_id, &intval);
      if (ret) {
        dev_err(
            dev, "%s : Failed to clear block charging toggle=%d ret=%d\n", __func__, intval, ret);
      } else {
        ret = count;
      }
      break;
    case METABATTERY_LIMIT_CHARGING_RATE:
      ret = kstrtos32(buf, 10, &intval);
      if (intval != 0 && intval != 1) {
        dev_err(dev, "%s : Incorrect input for limiting charging rate=%d \n", __func__, intval);
        return -EINVAL;
      }
      ret |= metabattery_set_data(data, METABATTERY_LIMIT_CHARGING_RATE, battery_id, &intval);
      if (ret) {
        dev_err(
            dev, "%s : Failed to limit charging rate toggle=%d ret=%d\n", __func__, intval, ret);
        data->limit_charging_rate_enabled = -1;
      } else {
        // store limit charging rate enabled state
        data->limit_charging_rate_enabled = (bool)intval;
        ret = count;
      }
      break;
    case METABATTERY_META_SOC_LOW_BATT_SHUTDOWN:
      ret = kstrtos32(buf, 10, &intval);
      if (intval != 1) {
        dev_err(dev, "%s : Incorrect input for metasoc low battery shutdown %d \n", __func__, intval);
        return -EINVAL;
      }
      ret |= metabattery_set_data(data, METABATTERY_META_SOC_LOW_BATT_SHUTDOWN, battery_id, &intval);
      if (ret) {
        dev_err(
            dev, "%s : Failed to set low battery shutdown %d ret=%d\n", __func__, intval, ret);
      }
      break;
    case METABATTERY_CACHING_ENABLED:
      ret = kstrtos32(buf, 10, &intval);
      if (intval != 0 && intval != 1) {
        dev_err(dev, "%s : Incorrect input for caching enabled=%d \n", __func__, intval);
        return -EINVAL;
      }
      data->caching_enabled = (bool)intval;
      ret = count;
      break;
    case METABATTERY_CACHING_REFRESH_RATE:
      ret = kstrtos32(buf, 10, &intval);
      if (intval < 0) {
        dev_err(dev, "%s : Incorrect input for caching refresh rate=%d \n", __func__, intval);
        return -EINVAL;
      }
      data->caching_refresh_interval_ms = intval;
      ret = count;
      break;
    default:
      ret = -EINVAL;
  }
  return ret;
}

ssize_t
metabattery_uevent_nonpsy_show(struct device* dev, struct device_attribute* attr, char* buf) {
  int ret = 0, i, bufind = 0;
  size_t prop_str_len;
  struct device_attribute* dev_attr;
  char* prop_buf;

  prop_buf = (char*)get_zeroed_page(GFP_KERNEL);
  if (!prop_buf)
    return -ENOMEM;

  for (i = METABATTERY_REMAINING_NVM_UPDATES; i < (int)ARRAY_SIZE(metabattery_attrs); i++) {
    dev_attr = &metabattery_attrs[i];

    ret = metabattery_show_attrs(dev, dev_attr, prop_buf);
    if (ret < 0)
      continue;

    prop_str_len = strlen(dev_attr->attr.name) + METABATTERY_PROP_STR_LEN +
        ret; /* length includes prefix "metabattery_" + name + "=" + value */
    if (bufind + prop_str_len > PAGE_SIZE - 1) {
      pr_err("%s: Exceeded buffer length for uevent_nonpsy\n", __func__);
      ret = -ENOMEM;
      goto out;
    }

    bufind +=
        snprintf(buf + bufind, prop_str_len, "metabattery_%s=%s", dev_attr->attr.name, prop_buf);
  }

  ret = bufind;

out:
  free_page((unsigned long)prop_buf);

  return ret;
}

static struct device_attribute metabattery_uevent_nonpsy = {
    .attr = {.name = "uevent_nonpsy", .mode = 0444},
    .show = metabattery_uevent_nonpsy_show,
};

static struct thermal_zone_of_device_ops thermal_ops = {
    .get_temp = metabattery_get_temp_mc,
};

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
static ssize_t emul_batt_capacity_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {
  int ret;
  int emul_batt_capacity = -1;
  char* env[2];
  struct power_supply* psy = dev_get_drvdata(dev);
  struct metabattery_data* data = power_supply_get_drvdata(psy);

  ret = kstrtos32(buf, 10, &emul_batt_capacity);
  if (ret < 0) {
    dev_err(dev, "Failed to read emul_batt_capacity from buffer: %d\n", ret);

    // Intentionally return 'count' so this update won't keep being retried
    return count;
  }

  if (emul_batt_capacity > 100) {
    dev_err(dev, "The proposed capacity level was greater than 100\n");
    return count;
  }

  data->emul_virtual_battery_capacity = emul_batt_capacity;

  // Sending uevent to health HAL to update battery
  env[0] = "SUBSYSTEM=power_supply";
  env[1] = NULL;

  kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, env);

  return count;
}
static DEVICE_ATTR_WO(emul_batt_capacity);

static ssize_t emul_batt_temperature_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {
  int ret;
  int emul_batt_temperature = -1;
  struct power_supply* psy = dev_get_drvdata(dev);
  struct metabattery_data* data = power_supply_get_drvdata(psy);

  ret = kstrtos32(buf, 10, &emul_batt_temperature);
  if (ret < 0) {
    dev_err(dev, "Failed to read emul_batt_temperature from buffer: %d\n", ret);

    // Intentionally return 'count' so this update won't keep being retried
    return count;
  }

  data->emul_virtual_battery_temperature = emul_batt_temperature;

  return count;
}
static DEVICE_ATTR_WO(emul_batt_temperature);

static ssize_t emul_batt_mix_soc_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {
  int ret;
  int emul_mix_soc = -1;
  struct power_supply* psy = dev_get_drvdata(dev);
  struct metabattery_data* data = power_supply_get_drvdata(psy);

  ret = kstrtos32(buf, 10, &emul_mix_soc);
  if (ret < 0) {
    dev_err(dev, "Failed to read emul_mix_soc from buffer: %d\n", ret);

    // Intentionally return 'count' so this update won't keep being retried
    return count;
  }

  data->emul_virtual_battery_mix_soc = emul_mix_soc;

  return count;
}
static DEVICE_ATTR_WO(emul_batt_mix_soc);

static struct attribute* emulation_attributes[] = {
    &dev_attr_emul_batt_capacity.attr,
    &dev_attr_emul_batt_temperature.attr,
    &dev_attr_emul_batt_mix_soc.attr,
    NULL,
};

static const struct attribute_group emulation_attr_group = {
    .name = "emulation",
    .attrs = emulation_attributes};
#endif

static int metabattery_init_thermal_zone(
    struct platform_device* pdev,
    struct metabattery_data* data) {
  int ret = 0;
  int battery_id = 0;
  struct metabattery_tz_data* tz_data = NULL;

  for (battery_id = 0; battery_id < data->battery_count; battery_id++) {
    tz_data = devm_kzalloc(&pdev->dev, sizeof(*tz_data), GFP_KERNEL);
    if (!tz_data) {
      ret = -ENOMEM;
      break;
    }
    tz_data->battery_id = battery_id;
    tz_data->data = data;

    tz_data->tzd =
        devm_thermal_zone_of_sensor_register(&pdev->dev, battery_id, tz_data, &thermal_ops);
    if (IS_ERR(tz_data->tzd)) {
      ret = PTR_ERR(tz_data->tzd);
      break;
    }
  }

  return ret;
}

static int metabattery_create_attrs(struct device* dev) {
  int i, rc;

  for (i = 0; i < (int)ARRAY_SIZE(metabattery_attrs); i++) {
    rc = device_create_file(dev, &metabattery_attrs[i]);
    if (rc)
      goto create_attrs_failed;
  }
  dev_err(dev, "%s : create_attrs Success\n", __func__);
  rc = device_create_file(dev, &metabattery_uevent_nonpsy);
  if (rc) {
    dev_err(dev, "%s: failed to create uevent_nonpsy (%d)\n", __func__, rc);
    device_remove_file(dev, &metabattery_uevent_nonpsy);
  }

  return rc;

create_attrs_failed:
  dev_err(dev, "%s: failed (%d)\n", __func__, rc);
  while (i--)
    device_remove_file(dev, &metabattery_attrs[i]);
  return rc;
}

static int metabattery_set_property(
    struct power_supply* psy,
    enum power_supply_property psp,
    const union power_supply_propval* val) {
  switch (psp) {
    default:
      return -EINVAL;
  }

  return 0;
}

static int metabattery_property_is_writeable(
    struct power_supply* psy,
    enum power_supply_property psp) {
  int ret;

  switch (psp) {
    default:
      ret = 0;
  }

  return ret;
}

static char* batt_supplied_to[] = {
    "metabattery",
};

static void low_voltage_comparator_update(void* pdata, bool is_throttle) {
  int xfer_data = 0;
  int ret = 0;
  struct metabattery_data* data = (struct metabattery_data*)pdata;

  xfer_data = is_throttle;
  ret = metabattery_set_data(
      data, METABATTERY_LOW_VOLT_COMPARATOR_TRIGGERED, METABATTERY_BATTERY_ID_0, &xfer_data);
  if (ret) {
    dev_err(
        data->dev,
        "%s: Failed to send low_volt_comp_triggered status=%d, trying again\n",
        __func__,
        is_throttle);
    ret = metabattery_set_data(
        data, METABATTERY_LOW_VOLT_COMPARATOR_TRIGGERED, METABATTERY_BATTERY_ID_0, &xfer_data);
    dev_err(
        data->dev,
        "%s: Attempted to send low_volt_comp_triggered status=%d one more time with ret=%d\n",
        __func__,
        is_throttle,
        ret);
  }
}

static int metabattery_probe(struct platform_device* pdev) {
  int mux_pin;
  int irq_flags;
  struct device* dev = &pdev->dev;
  struct metabattery_data* data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
  int ret = 0;
  int i = 0;

  struct power_supply_config metabattery_cfg = {};

  if (!data) {
    ret = -ENOMEM;
    goto err_exit_no_phandle;
  }

  data->dev = dev;

  data->stp_interface_node = of_parse_phandle(pdev->dev.of_node, "stp-interface", 0);
  if (!data->stp_interface_node) {
    dev_err(&pdev->dev, "%s: unable to get stp-interface device_node\n", __func__);
    ret = -EPROBE_DEFER;
    goto err_exit_no_phandle;
  }

  ret = of_property_read_u32(pdev->dev.of_node, "battery-count", &data->battery_count);
  if (ret) {
    dev_err(&pdev->dev, "%s: unable to get battery-count\n", __func__);
    ret = -EINVAL;
    goto err_exit;
  }

  mux_pin = of_get_named_gpio(pdev->dev.of_node, "mux-gpio", 0);
  if (gpio_is_valid(mux_pin)) {
    // Setting the mux to the MCU
    ret = gpio_direction_output(mux_pin, 0);
    if (ret)
      dev_err(&pdev->dev, "%s: unable to set mux-gpio\n", __func__);
  } else
    dev_dbg(&pdev->dev, "%s: mux-gpio doesn't exist\n", __func__);

  // Initialize caching variables
  data->caching_enabled = of_property_read_bool(pdev->dev.of_node, "caching-enabled");

  ret = of_property_read_u32(pdev->dev.of_node, "caching-refresh-interval-ms", &data->caching_refresh_interval_ms);
  if (ret) {
    dev_dbg(&pdev->dev, "%s: 'caching-refresh-interval-ms' not found, defaulting to %d\n",
        __func__, METABATTERY_DEFAULT_CACHING_REFRESH_INTERVAL_MS);
    data->caching_refresh_interval_ms = METABATTERY_DEFAULT_CACHING_REFRESH_INTERVAL_MS;
    ret = 0; // Not a fatal error
  }

  data->data_cache = devm_kzalloc(dev, sizeof(struct metabattery_cache), GFP_KERNEL);
  if (!data->data_cache) {
    ret = -ENOMEM;
    goto err_exit;
  }

  data->data_cache->capacity = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->temperature = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->mix_soc = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->rep_soc = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->health = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->voltage_now = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->voltage_ocv = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->charge_full = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->current_now = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->current_avg = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->power_avg = METABATTERY_INVALID_CACHE_VALUE;
  data->data_cache->charge_counter = METABATTERY_INVALID_CACHE_VALUE;

  // Initialize power supply
  data->battery[0] = NULL;
  data->rsense[0] = RSENSE_INVALID_VALUE; // Assure that we will query the MCU for the true value
  data->psy_batt_d[0].name = kasprintf(GFP_KERNEL, METABATTERY_BATTERY_0_NAME);
  data->psy_batt_d[0].type = POWER_SUPPLY_TYPE_BATTERY;
  data->psy_batt_d[0].properties = metabattery_battery_props;
  data->psy_batt_d[0].get_property = metabattery_get_property;
  data->psy_batt_d[0].set_property = metabattery_set_property;
  data->psy_batt_d[0].property_is_writeable = metabattery_property_is_writeable;
  data->psy_batt_d[0].num_properties = ARRAY_SIZE(metabattery_battery_props);
  data->psy_batt_d[0].no_thermal = true;
  metabattery_cfg.drv_data = data;
  metabattery_cfg.supplied_to = batt_supplied_to;
  metabattery_cfg.of_node = data->dev->of_node;
  metabattery_cfg.num_supplicants = ARRAY_SIZE(batt_supplied_to);
  data->battery[0] = devm_power_supply_register(data->dev, &data->psy_batt_d[i], &metabattery_cfg);
  if (IS_ERR(data->battery[0])) {
    dev_err(
        &pdev->dev, "metabattery: Couldn't register battery rc=%ld\n", PTR_ERR(data->battery[0]));
    ret = PTR_ERR(data->battery[0]);
    goto err_exit;
  }

  // TO-DO(@gino): Create a separate set of attributes for each battery
  ret = metabattery_create_attrs(&data->battery[0]->dev);
  if (ret) {
    dev_err(data->dev, "%s : Failed to create_attrs\n", __func__);
  }

  data->virtual_battery_id = METABATTERY_BATTERY_ID_0;

  if (data->battery_count >= 2) {
    for (i = 1; i < data->battery_count; i++) {
      data->battery[i] = NULL;
      data->rsense[i] = RSENSE_INVALID_VALUE; // Assure that we will query the MCU for the true value
      data->psy_batt_d[i].name = kasprintf(GFP_KERNEL, "metabattery_%d", i);
      data->psy_batt_d[i].type = POWER_SUPPLY_TYPE_BATTERY;
      data->psy_batt_d[i].properties = metabattery_battery_props;
      data->psy_batt_d[i].get_property = metabattery_get_property;
      data->psy_batt_d[i].set_property = metabattery_set_property;
      data->psy_batt_d[i].property_is_writeable = metabattery_property_is_writeable;
      data->psy_batt_d[i].num_properties = ARRAY_SIZE(metabattery_battery_props);
      data->psy_batt_d[i].no_thermal = true;
      metabattery_cfg.drv_data = data;
      metabattery_cfg.supplied_to = batt_supplied_to;
      metabattery_cfg.of_node = data->dev->of_node;
      metabattery_cfg.num_supplicants = ARRAY_SIZE(batt_supplied_to);
      data->battery[i] =
          devm_power_supply_register(data->dev, &data->psy_batt_d[i], &metabattery_cfg);
      if (IS_ERR(data->battery[i])) {
        dev_err(
            &pdev->dev,
            "metabattery: Couldn't register battery rc=%ld\n",
            PTR_ERR(data->battery[i]));
        ret = PTR_ERR(data->battery[i]);
        goto err_exit;
      }

      // TO-DO(@gino): Create a separate set of attributes for each battery
      ret = metabattery_create_attrs(&data->battery[i]->dev);
      if (ret) {
        dev_err(data->dev, "%s : Failed to create_attrs\n", __func__);
      }
    }

    // Set virtual battery id to common
    data->virtual_battery_id = METABATTERY_BATTERY_ID_COMMON;

    data->psy_batt_d_common.name = METABATTERY_COMMON_NAME;
    data->psy_batt_d_common.type = POWER_SUPPLY_TYPE_BATTERY;
    data->psy_batt_d_common.properties = metabattery_battery_props;
    data->psy_batt_d_common.get_property = metabattery_get_property;
    data->psy_batt_d_common.set_property = metabattery_set_property;
    data->psy_batt_d_common.property_is_writeable = metabattery_property_is_writeable;
    data->psy_batt_d_common.num_properties = ARRAY_SIZE(metabattery_battery_props);
    data->psy_batt_d_common.no_thermal = true;
    metabattery_cfg.drv_data = data;
    metabattery_cfg.supplied_to = batt_supplied_to;
    metabattery_cfg.of_node = data->dev->of_node;
    metabattery_cfg.num_supplicants = ARRAY_SIZE(batt_supplied_to);
    data->battery_common =
        devm_power_supply_register(data->dev, &data->psy_batt_d_common, &metabattery_cfg);
    if (IS_ERR(data->battery_common)) {
      dev_err(
          &pdev->dev,
          "metabattery: Couldn't register battery rc=%ld\n",
          PTR_ERR(data->battery_common));
      ret = PTR_ERR(data->battery_common);
      goto err_exit;
    }

    ret = metabattery_create_attrs(&data->battery_common->dev);
    if (ret) {
      dev_err(data->dev, "%s : Failed to create_attrs\n", __func__);
    }
  }

  data->dc_charger = power_supply_get_by_name("dc-charger");
  if (IS_ERR_OR_NULL(data->dc_charger)) {
    dev_err(&pdev->dev, "%s: Couldn't get power supply dc-charger\n", __func__);
    ret = -EPROBE_DEFER;
    goto err_exit;
  }

  ret = metabattery_init_thermal_zone(pdev, data);
  if (ret) {
    dev_err(&pdev->dev, "%s: Couldn't init thermal zone\n", __func__);
  }

  data->metabattery_gpiod = devm_gpiod_get_optional(dev, "metabattery-alert", GPIOD_IN);
  if (!IS_ERR(data->metabattery_gpiod)) {
    // initialize alert
    data->gpio_irq = gpiod_to_irq(data->metabattery_gpiod);
    if (data->gpio_irq < 0) {
      dev_err(&pdev->dev, "%s: failed to get IRQ GPIO\n", __func__);
    } else {
      irq_flags = irq_get_trigger_type(data->gpio_irq);
      if (!irq_flags) {
        irq_flags = IRQF_TRIGGER_FALLING;
      }
      // set up irq
      devm_request_threaded_irq(
          dev,
          data->gpio_irq,
          NULL,
          gpio_irq_handler,
          irq_flags | IRQF_ONESHOT | IRQF_SHARED,
          "metabattery-alert-irq",
          data);
    }
  }

  // initialize low voltage comparator callback
  ret = low_volt_register_callback(low_voltage_comparator_update, data);
  if (ret) {
    dev_err(&pdev->dev, "%s: failed to register low voltage comparator callback\n", __func__);
    goto err_exit_psy_put;
  }

  // initialize battery presence to 0
  data->battery_present = 0;

  // initialize retail demo bools to 0
  data->retail_demo_enabled = 0;
  data->man_charge_enabled = 0;

  // initialize emulation
#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
  data->emul_virtual_battery_capacity = INVALID_EMUL_BATTERY_CAPACITY;
  data->emul_virtual_battery_temperature = INVALID_EMUL_BATTERY_TEMPERATURE;
  data->emul_virtual_battery_mix_soc = INVALID_EMUL_BATTERY_MIX_SOC;

  if (data->virtual_battery_id == METABATTERY_BATTERY_ID_COMMON) {
    // register it to the common node
    ret = sysfs_create_group(&data->battery_common->dev.kobj, &emulation_attr_group);
  } else {
    // register it to the virtual_battery_id node
    ret = sysfs_create_group(&data->battery[0]->dev.kobj, &emulation_attr_group);
  }
  if (ret) {
    dev_err(&pdev->dev, "%s: failed to create sysfs group for emulation ret=%d\n", __func__, ret);
    goto err_exit_psy_put;
  }
#endif

  dev_err(dev, "successfully probed\n");

  return 0;

err_exit_psy_put:
  power_supply_put(data->dc_charger);
err_exit:
  of_node_put(data->stp_interface_node);
err_exit_no_phandle:
  return ret;
}

static int metabattery_remove(struct platform_device* pdev) {
  int i = 0;
  struct metabattery_data* data = platform_get_drvdata(pdev);

  of_node_put(data->stp_interface_node);
  if (data->battery_count > 1) {
    for (i = 0; i < data->battery_count; i++) {
      power_supply_unregister(data->battery[i]);
    }
  }
  power_supply_unregister(data->battery_common);
  devm_kfree(data->dev, data);
  return 0;
}

static const struct of_device_id metabattery_match_table[] = {
    {
        .compatible = "meta,metabattery",
    },
    {},
};

static struct platform_driver metabattery_driver = {
    .probe = metabattery_probe,
    .remove = metabattery_remove,
    .driver =
        {
            .name = DRIVER_NAME,
            .owner = THIS_MODULE,
            .of_match_table = of_match_ptr(metabattery_match_table),
        },
};

static int __init metabattery_init(void) {
  int rc = 0;

  rc = platform_driver_register(&metabattery_driver);
  if (rc) {
    pr_err("Failed to register metabattery driver: %d\n", rc);
  }
  return rc;
}

static void __exit metabattery_exit(void) {
  platform_driver_unregister(&metabattery_driver);
}
module_init(metabattery_init);
module_exit(metabattery_exit);

MODULE_DESCRIPTION("MetaBattery driver");
MODULE_LICENSE("GPL");
