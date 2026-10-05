#ifndef __METABATTERY_H__
#define __METABATTERY_H__

#include "../../stp/driver/stp-interface.h"
#include <linux/power_supply.h>

#define DRIVER_NAME "metabattery"
#define DEVICE_NAME DRIVER_NAME
#define CLASS_NAME DRIVER_NAME
#define MAJOR_NUM 0
#define MAX_INT_DIGITS 21
#define MAX_NUM_BATTERIES 2

#define METABATTERY_COMMON_NAME "common_metabattery"
#define METABATTERY_COMMON_NAME_STRLN 18
#define METABATTERY_BATTERY_0_NAME "metabattery_0"
#define METABATTERY_BATTERY_1_NAME "metabattery_1"
#define METABATTERY_BATTERY_NAME_STRLEN 13
#define SIP_SERIAL_NUMBER_SIZE 12
#define PACK_SERIAL_NUMBER_SIZE 16

#define METABATTERY_MAGIC 0xba77
#define METABATTERY_MAX_MESSAGE_SIZE 32
#define METABATTERY_MAX_MESSAGE_SIZE_BYTES 4
#define METABATTERY_BATCH_UPDATE_MESSAGE_SIZE_BYTES 37
#define DUAL_METABATTERY 1

#define DEFAULT_RSENSE 20
#define RSENSE_INVALID_VALUE (-1)

#define METABATTERY_CAP_CRITICAL_LEVEL 0
#define METABATTERY_CAP_LOW_LEVEL 10
#define METABATTERY_CAP_HIGH_LEVEL 90

#define METABATTERY_IS_FULL_BIT BIT(13)

#define METABATTERY_MAN_CHARGE_THRESHOLD 18

/*
 * Valid temperature range (in decidegrees Celsius) for Li-ion battery packs.
 * Values outside this range indicate corrupted STP data or an uninitialized
 * fuel gauge, and must be rejected to prevent spurious thermal shutdowns.
 * -40C to 90C covers the full extended operating + storage range for Li-ion.
 */
#define METABATTERY_TEMP_MIN_VALID_DECIDEGC (-400)
#define METABATTERY_TEMP_MAX_VALID_DECIDEGC 900

#define METABATTERY_DEFAULT_CACHING_REFRESH_INTERVAL_MS 350
#define METABATTERY_INVALID_CACHE_REFRESH_TIME 0
#define METABATTERY_INVALID_CACHE_VALUE -32767

/* ChgStat register bits */
#define BIT_STATUS_DROPOUT BIT(15)
#define BIT_STATUS_CP BIT(3)
#define BIT_STATUS_CT BIT(2)
#define BIT_STATUS_CC BIT(1)
#define BIT_STATUS_CV BIT(0)

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
#define INVALID_EMUL_BATTERY_CAPACITY -1
#define INVALID_EMUL_BATTERY_TEMPERATURE -1000
#define INVALID_EMUL_BATTERY_MIX_SOC -1
#endif

struct metabattery_cache {
  int capacity;
  int temperature;
  int mix_soc;
  int rep_soc;
  int health;
  int voltage_now;
  int voltage_ocv;
  int charge_full;
  int current_now;
  int current_avg;
  int power_avg;
  int charge_counter;
  int cycle_count;
  int is_full;
  unsigned int timestamp;
};

struct metabattery_data {
  struct device* dev;
  int battery_count;
  int stp_probe_fails;
  int rsense[MAX_NUM_BATTERIES];
  int gpio_irq;
  int battery_present;
  bool retail_demo_enabled;
  bool man_charge_enabled;
  bool caching_enabled;
  int caching_refresh_interval_ms;
  struct metabattery_cache* data_cache;
  unsigned long cache_hits;
  int virtual_battery_id;
  int virtual_battery_capacity_cache;
  int virtual_battery_temperature_cache;
  int virtual_battery_mix_soc_cache;
  bool limit_charging_rate_enabled;
  bool full_seen;
  unsigned long virtual_battery_capacity_cache_time;
  unsigned long virtual_battery_temperature_cache_time;
  unsigned long virtual_battery_mix_soc_cache_time;
  struct attribute_group* attr_grp;
  struct power_supply* battery_common;
  struct power_supply_desc psy_batt_d_common;
  struct power_supply* battery[MAX_NUM_BATTERIES];
  struct power_supply_desc psy_batt_d[MAX_NUM_BATTERIES];
  struct power_supply* dc_charger;
  struct device_node* stp_interface_node;
  struct stp_interface* stpi;
  // struct thermal_zone_device *tzd;
  struct gpio_desc* metabattery_gpiod;
#ifdef CONFIG_BATTERY_CAPACITY_EMULATION
  // emulated battery capacity, only valid when it's greater than INVALID_EMUL_BATTERY_CAPACITY
  int emul_virtual_battery_capacity;
  // emulated battery temperature, only valid when it's greater than
  // INVALID_EMUL_BATTERY_TEMPERATURE
  int emul_virtual_battery_temperature;
  // emulated battery mix_soc, only valid when it's greater than INVALID_EMUL_BATTERY_MIX_SOC
  int emul_virtual_battery_mix_soc;
#endif
};

struct metabattery_tz_data {
  struct metabattery_data* data;
  struct thermal_zone_device* tzd;
  int battery_id;
};

ssize_t metabattery_show_attrs(struct device* dev, struct device_attribute* attr, char* buf);

ssize_t metabattery_store_attrs(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count);

#define METABATTERY_ATTR(_name)                                             \
  {                                                                         \
    .attr = {.name = #_name, .mode = 0664}, .show = metabattery_show_attrs, \
    .store = metabattery_store_attrs,                                       \
  }

#endif
