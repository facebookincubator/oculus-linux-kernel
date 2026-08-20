/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __MAX17332_BATTERY_H_
#define __MAX17332_BATTERY_H_

#include <linux/power_supply.h>
#include <linux/types.h>

#if (IS_ENABLED(CONFIG_METASOC))
#include "metasoc.h"
#endif

#define REG_VALRTTH         0x01
#define REG_TALRTTH         0x02
#define REG_SALRTTH         0x03
#define REG_TTE             0x11
#define REG_AVGVCELL        0x19
#define REG_VCELL           0x1A
#define REG_TEMP            0x1B
#define REG_CURRENT         0x1C
#define REG_AVGCURRENT      0x1D
#define REG_TTF             0x20
#define REG_VERSION         0x21
#define REG_FULLCAPNOM      0x23
#define REG_CHGCURRENT      0x28
#define REG_CHGVOLTAGE      0x2A
#define REG_VEMPTY          0x3A
#define REG_CHGSTAT         0xA3
#define REG_LEARNCFG        0xA1
#define REG_IALRTTH         0xAC
#define REG_POWER           0xB1
#define REG_POWER_AVG       0xB3
#define REG_VFOCV           0xFB
#define REG_PCKP            0xDB
#define REG_PROTCFG2        0xF1
#define REG_SLACK           0x6B
#define REG_BMU_SMT_DATE    0xE8
#define REG_TIMER           0x3E
#define REG_TIMERH          0xBE
#define REG_VCELL_REP       0x12
#define REG_CURRENT_REP     0x22
#define REG_FULL_CAP        0x35
#define REG_AV_CAP          0x1F
#define REG_AC_SOC          0x0E
#define REG_MIX_CAP         0x2B
#define REG_MIX_SOC         0x0D
#define REG_VFREM_CAP       0x4A
#define REG_VF_SOC          0xFF
#define REG_Q_RESIDUAL      0x0C
#define REG_QR_TABLE_00     0xA0
#define REG_QR_TABLE_10     0xA1
#define REG_QR_TABLE_20     0xA2
#define REG_QR_TABLE_30     0xA3
#define REG_RCOMP0          0xA6
#define REG_TEMP_CO         0xA7
#define REG_LOCK            0x7F
#define REG_N_VCHG_CFG1     0xCC
#define REG_N_STEP_V        0xC5
#define REG_NCGAIN          0xC8
#define REG_N_IPRT_TH1      0xD3
#define REG_N_DESGIN_VOLT   0xE3
#define REG_RCELL           0x14
#define REG_TARGET_CHG_V    0x2A
#define REG_TARGET_CHG_I    0x28
#define REG_DESIGN_CAP      0x18
#define REG_N_DESIGN_CAP    0xB3
#define REG_N_CYCLES        0xA4
#define REG_N_FULL_CAP_NOM  0xA5
#define REG_N_FULL_CAP_REP  0xA9
#define REG_N_TIMERH        0xAF
#define REG_QACC            0x45
#define REG_PACC            0x46
#define REG_N_NV_CFG2       0xDB
#define REG_AGE             0x07
#define REG_DIETEMP         0x34
#define REG_FSTAT         	0x3D
#define REG_FSTAT2         	0x39
#define REG_HPROTCFG        0xF0
#define REG_FOTPSTAT        0xBB
#define REG_N_BATTSTATUS    0xA8

/* For offset change */
#define REG_TRIM1           0x51
#define REG_MISCCFG         0x0F
#define REG_NMISCCFG        0xB2

/* Config register bits for MAX17332 */
#define BIT_CONFIG_ALRT_EN		BIT(2)
#define BIT_PROTSTATUS_FULL		BIT(13)
#define BIT_CONFIG_MANCHG	    BIT(15)

#define MAX17332_BATTERY_FULL	100
#define MAX17332_BATTERY_LOW	15
#define MAX17332_SC50_BATT_OVRCHG_THRESHOLD_UV 4430000 /* 4.43v */
#define MAX17332_HAMMERHEAD_BATT_OVRCHG_THRESHOLD_UV 4400000 /* 4.40v */
#define MAX17332_DEFAULT_BATT_OVRCHG_THRESHOLD_UV 4400000 /* 4.40v */

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
#define INVALID_EMUL_BATT_CAPACITY -1
#define INVALID_EMUL_BATT_TEMPERATURE -1000
#define INVALID_EMUL_MIX_SOC -1
#define INVALID_EMUL_VOLTAGE_OCV -1
#endif

/* Default interrupt interval in ms */
#define MAX17332_DEFAULT_INT_INTERVAL_MS 350

/* Active discharge thresholds */
#define MAX17332_ACTIVE_DISCHARGE_SOC 95
#define MAX17332_ACTIVE_DISCHARGE_START_TEMP 450
#define MAX17332_ACTIVE_DISCHARGE_END_TEMP 420
#define MAX17332_HIGH_TEMP_THRESHOLD 600
#define MAX17332_LOW_TEMP_THRESHOLD -200

/* Learn Stage */
#define MAX17332_LEARNCFG_LEARNSTAGE_FULL 0x70
#define MAX17332_LEARNCFG_LEARNSTAGE_MASK 0x70

/* nCGAIN */
#define MAX17332_NCGAIN_DEFAULT    0x4003
#define MAX17332_NCGAIN_TRIM1_2    0x4001

/* This is the sign extended two's compliment representation of -128 */
#define MAX17332_TEMP_THRESHOLD_MIN -1280
#define MAX17332_TEMP_THRESHOLD_0 0
#define MAX17332_TEMP_THRESHOLD_5 50
#define MAX17332_TEMP_THRESHOLD_10 100
#define MAX17332_TEMP_THRESHOLD_15 150
#define MAX17332_TEMP_THRESHOLD_20 200
#define MAX17332_TEMP_THRESHOLD_30 300
#define MAX17332_TEMP_THRESHOLD_40 400
#define MAX17332_TEMP_THRESHOLD_42 420
#define MAX17332_TEMP_THRESHOLD_45 450
#define MAX17332_TEMP_THRESHOLD_50 500
#define MAX17332_TEMP_THRESHOLD_55 550
#define MAX17332_TEMP_THRESHOLD_60 600
#define MAX17332_TEMP_THRESHOLD_MAX 1270

#define MAX17332_VOLTAGE_THRESHOLD_MIN 0
#define MAX17332_VOLTAGE_THRESHOLD_4_40 4400
#define MAX17332_VOLTAGE_THRESHOLD_4_34 4340
#define MAX17332_VOLTAGE_THRESHOLD_4_30 4300
#define MAX17332_VOLTAGE_THRESHOLD_4_20 4200
#define MAX17332_VOLTAGE_THRESHOLD_4_00 4000
#define MAX17332_VOLTAGE_THRESHOLD_3_80 3800
#define MAX17332_VOLTAGE_THRESHOLD_3_60 3600
#define MAX17332_VOLTAGE_THRESHOLD_3_40 3400
#define MAX17332_VOLTAGE_THRESHOLD_3_00 3000
#define MAX17332_VOLTAGE_THRESHOLD_MAX 5100

/* Register values for controlled charge */
#define NVCHGCFG1_CONTROLLED_CHARGE_ON 0x0280
#define NVSTEPVOLT_CONTROLLED_CHARGE_ON 0x0000

#define NVCHGCFG1_CONTROLLED_CHARGE_OFF 0x04b0
#define NVSTEPVOLT_CONTROLLED_CHARGE_OFF 0x05f0

/* Register Values to Manual Charge 4.1V */
#define NVCHGVOLT_MANUAL_CHARGE_4_1V 0xcd00
#define NVCHGCURR_MANUAL_CHARGE_DEFAULT 0x0180

/* Version for P3ROM Battery */
#define MAX17332_VERSION_P3ROM 0x4130

/* MISCCFG default setting */
#define MAX17332_MISCCFG_DEFAULT 0x3870

/* TRIM1[1:0] */
#define MAX17332_TRIM1_MASK ((u16)(BIT(0)|BIT(1)))

 /* Default capacity value in uAh to compare during fg update */
#define FULLCAPNOM_DEFAULT_THRESHOLD 3000000 /* 3 x 1Ah */
#define FULLCAPREP_DEFAULT_THRESHOLD 3000000
#define PACK_DESIGN_CAPACITY_SC50_UAH 220000
#define PACK_DESIGN_CAPACITY_HAMMERHEADPACK_UAH 155000

enum max17332_fg_pack_id {
	MAX17332_FG_PACK_ID_UNKNOWN = 0,
	MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0,
	MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0,
	MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0,
};

enum battery_pack_vendor {
	BATTERY_PACK_VENDOR_0 = 0,
	BATTERY_PACK_VENDOR_1,
	BATTERY_PACK_VENDOR_2,
	BATTERY_PACK_VENDOR_3,
	BATTERY_PACK_VENDOR_4,
	BATTERY_PACK_VENDOR_5,
	BATTERY_PACK_VENDOR_6,
	BATTERY_PACK_VENDOR_7
};

enum battery_cell_vendor {
	BATTERY_CELL_VENDOR_0 = 0,
	BATTERY_CELL_VENDOR_1,
	BATTERY_CELL_VENDOR_2,
	BATTERY_CELL_VENDOR_3,
	BATTERY_CELL_VENDOR_4,
	BATTERY_CELL_VENDOR_5
};

struct max17332_fg_platform_data {
	enum max17332_fg_pack_id pack_id;
	int soc_max;  /* in percent */
	int soc_min;  /* in percent */
	int curr_max; /* in mA */
	int curr_min; /* in mA */

	u8 cap_critical_lvl;
	u8 cap_low_lvl;
	u8 cap_high_lvl;
	u8 fg_config_version;

	u16 manual_charge_current_reg_val; /* in hex */

	int int_interval_ms; /* interrupt interval */
	bool is_irq_level_trigger;
	int occp_threshold; /* Overcharge current-protection threshold in mA */
};

/* Structure introduced to help with fg update framework */
struct max17332_fg_config_update_data {
	const char *config_name;
	uint16_t *config_data;
	size_t config_size_u16;
	const char *ignore_name;
	uint16_t *ignore_data;
	size_t ignore_size_u16;
};

enum {
	MAX17332_FG_PROGRAM_NVM = 0,
	/* ONLY ADD PROPERTIES TO BE READ IN UEVENT_NONPSP BELOW */
	MAX17332_FG_REMAINING_NVM_UPDATES,
	MAX17332_FG_VOLTAGE_PACK_NOW,
	MAX17332_FG_COULOMB_COUNTER,
	MAX17332_FG_CHARGE_FULL_NOM,
	MAX17332_FG_RSENSE,
	MAX17332_FG_VOLTAGE_ALERT_MAX,
	MAX17332_FG_VOLTAGE_ALERT_MIN,
	MAX17332_FG_CURRENT_ALERT_MAX,
	MAX17332_FG_CURRENT_ALERT_MIN,
	MAX17332_FG_TIMER,
	MAX17332_FG_TIMERH,
	MAX17332_FG_BATTERY_CHGSTAT,
	MAX17332_FG_BATT_STATUS,
	MAX17332_FG_PROT_STATUS,
	MAX17332_FG_PROT_ALERT,
	MAX17332_FG_FET_STATUS,
	MAX17332_FG_BATT_CONFIG,
	MAX17332_FG_BATT_CONFIG2,
	MAX17332_FG_COMM_STATUS,
	MAX17332_FG_SLACK,
	MAX17332_FG_INI_REV,
	MAX17332_FG_SIP_SN,
	MAX17332_FG_PACK_SN,
	MAX17332_FG_BMU_SMT_DATE,
	MAX17332_FG_FULL_CAP,
	MAX17332_FG_AV_CAP,
	MAX17332_FG_AC_SOC,
	MAX17332_FG_MIX_CAP,
	MAX17332_FG_MIX_SOC,
	MAX17332_FG_VFREM_CAP,
	MAX17332_FG_VF_SOC,
	MAX17332_FG_Q_RESIDUAL,
	MAX17332_FG_QR_TABLE_00,
	MAX17332_FG_QR_TABLE_10,
	MAX17332_FG_QR_TABLE_20,
	MAX17332_FG_QR_TABLE_30,
	MAX17332_FG_RCOMP0,
	MAX17332_FG_TEMP_CO,
	MAX17332_FG_LOCK,
	MAX17332_FG_SUSPEND_BATTERY_PCT,
	MAX17332_FG_SUSPEND_CHARGE_COUNTER,
	MAX17332_FG_SUSPEND_VOLTAGE,
	MAX17332_FG_CP_CHANGE_COUNTER,
	MAX17332_FG_DROPOUT_CHANGE_COUNTER,
	MAX17332_FG_OVP_CHANGE_COUNTER,
	MAX17332_FG_OCCP_CHANGE_COUNTER,
	MAX17332_FG_CYCLE_COUNT_FRAC,
	MAX17332_FG_CONTROLLED_CHARGE_ON,
	MAX17332_FG_LEARN_STAGE,
	MAX17332_FG_TIMER_SECONDS,
	MAX17332_FG_I2C_READ_FAILURE_COUNT,
	MAX17332_FG_I2C_WRITE_FAILURE_COUNT,
	MAX17332_FG_NCGAIN,
	MAX17332_FG_RCELL,
	MAX17332_FG_MANUAL_CHARGING_ON,
	MAX17332_FG_LAST_BATT_STATUS,
	MAX17332_FG_LAST_PROT_STATUS,
#ifdef CONFIG_MAX17332_VOLTAGE_ADJUSTMENT
	MAX17332_FG_VREG_OUT_DELTA_RAISE,
	MAX17332_FG_VREG_OUT_DELTA_FALL,
	MAX17332_FG_VREG_UV_MAX,
	MAX17332_FG_VREG_DROPOUT_MIN,
	MAX17332_FG_VREG_VSYS_CHANGE_PERIOD_MIN,
#endif
#if (IS_ENABLED(CONFIG_METASOC))
	MAX17332_FG_REP_SOC,
	MAX17332_FG_META_SOC,
	MAX17332_FG_META_SOC_INIT_VAL,
	MAX17332_FG_META_SOC_INIT_TIME,
	MAX17332_FG_META_SOC_ENABLED,
	MAX17332_FG_META_SOC_VERSION,
	MAX17332_FG_META_SOC_INIT,
	MAX17332_FG_META_SOC_USOC,
	MAX17332_FG_META_SOC_EOC,
	MAX17332_FG_META_SOC_EOD,
	MAX17332_FG_META_LOW_BATT_SHUTDOWN,
	MAX17332_FG_META_SOC_CONFIG_ID,
	MAX17332_FG_META_SOC_LOW_VOLT_COMP_TRIPPED,
	MAX17332_FG_META_SOC_USOC_FILTERED,
	MAX17332_FG_META_SOC_PEAK_VOLTAGE_DROOP_PENALTY,
	MAX17332_FG_META_SOC_REMAINING_CAPACITY,
#endif
	MAX17332_FG_TRIM1,
	MAX17332_FG_TARGET_CHG_VOLTAGE,
	MAX17332_FG_TARGET_CHG_CURRENT,
	MAX17332_FG_BATT_CAP_LOW_LVL,
	MAX17332_FG_BATT_CAP_CRITICAL_LVL,
	MAX17332_FG_DESIGN_CAP,
	MAX17332_FG_N_DESIGN_CAP,
	MAX17332_FG_N_CYCLES,
	MAX17332_FG_N_FULL_CAP_NOM,
	MAX17332_FG_N_FULL_CAP_REP,
	MAX17332_FG_N_TIMERH,
	MAX17332_FG_DQACC,
	MAX17332_FG_DPACC,
	MAX17332_FG_AGE,
	MAX17332_IS_VIRTUAL_BATTERY,
	MAX17332_FG_CONFIG_UPDATE_ALGO_VER,
	MAX17332_FG_CONFIG_UPDATE_CNTR,
	MAX17332_FG_CONFIG_UPDATE_FAIL_CNTR,
	MAX17332_FG_CONFIG_UPDATE_SUCCESS_CNTR,
	MAX17332_FG_CONFIG_UPDATE_WRITE_FAIL_REG,
	MAX17332_FG_CONFIG_UPDATE_WRITE_FAIL_REG_VAL,
	MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON,
	MAX17332_FG_N_DESIGN_VOLTAGE,
	MAX17332_FG_CONFIG_UNMAPPED_CAPACITY,
	MAX17332_FG_BATTERY_NAME,
	MAX17332_FG_DIETEMP,
	MAX17332_FG_FSTAT,
	MAX17332_FG_FSTAT2,
	MAX17332_FG_HPROTCFG,
	MAX17332_FG_FOTPSTAT,
	MAX17332_FG_FPROTSTAT,
	MAX17332_FG_N_BATTSTATUS,
};

ssize_t max17332_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t max17332_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define MAX17332_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = max17332_fg_show_attrs,			\
	.store = max17332_fg_store_attrs,			\
}

#define MAX_INT_DIGITS              21
#define CHG_STAT_SIZE               35
#define REG_RAW_VAL_SIZE            6
#define SIP_SERIAL_NUMBER_SIZE      12
#define PACK_SERIAL_NUMBER_SIZE     16

/* Structure to maintain stats for fg update framework */
struct fg_config_update_work_params {
	u32 fg_config_update_algo_version;
	u32 fg_config_update_counter;
	u32 fg_config_update_fail_counter;
	u32 fg_config_update_success_counter;
	u16 fg_config_update_write_fail_register;
	u16 fg_config_update_write_fail_register_value;
	u32 fg_config_update_entry_reason;
};

enum {
	MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_UNKNOWN = 0,
	MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_INI_REV_MISMATCH,
	MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_FULLCAP_MISMATCH,
	MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_CONFIG_MISMATCH,
};

struct max17332_fg_chip {
	struct device           *dev;
	struct max17332_dev     *max17332;
	struct regmap			*regmap;
	struct regmap			*regmap_nvm;
	struct attribute_group *attr_grp;

	int						fg_prot_irq;
	int						fg_status;
	int						fg_update_retries;
	struct power_supply		*battery;
	struct power_supply_desc		psy_batt_d;

	/* mutex */
	struct mutex			lock;

	/* rsense */
	unsigned int rsense;

	struct max17332_fg_platform_data	*pdata;

	/* power supply object handle for USB/one-wire charger */
	struct power_supply *usb_charger;

	/* usb disconnected event, last for 850ms*/
	bool i2c_jitter;
	unsigned long usb_disc_time_stamp;

	/* notifier block for handling power supply change events */
	struct notifier_block nb;

	u8 ini_rev;
	char sip_serial_number[SIP_SERIAL_NUMBER_SIZE];
	char pack_serial_number[PACK_SERIAL_NUMBER_SIZE];

	struct max17332_cache     *cache;

	struct delayed_work	notify_work;
	struct delayed_work alert_work;
	struct delayed_work alert_enable_work;
	struct delayed_work fg_config_update_work;
	atomic_t fg_update_in_progress;
	bool requires_fg_update;
	bool in_active_discharge;
	bool fet_thermal_mitigation_enabled;
	bool custom_voltage_adjustment_enabled;

	// Cached stats for suspend power state telemetry
	u16 cached_battery_charge_counter;
	u16 suspend_battery_charge_counter;
	u16 cached_battery_pct;
	u16 suspend_battery_pct;
	u16 cached_voltage;
	u16 suspend_voltage;

	// Counters to keep track of how often metrics are updated
	u32 cp_change_counter;
	u32 dropout_change_counter;
	u32 ovp_change_counter;
	u32 occp_change_counter;

	// Boolean to only write to retail demo once
	int controlled_charge_on_written;
	int manual_charging_on_written;

	// last status register value before being cleared
	u16 cached_status_register_val;
	u16 cached_prot_status_register_val;

#if (IS_ENABLED(CONFIG_METASOC))
	int metasoc_init_val;
	int metasoc_init_time;
	int metasoc_enabled;
	int metasoc_version;
	int metasoc_init;
	bool metasoc_needs_init;
	int metasoc;
	metasoc_internal_stats metasoc_stats;
	long low_volt_comp_tripped;
	u16 cached_repsoc;
#endif

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
  /* The emulated battery capacity. Only valid when it's positive */
  int emul_batt_capacity;
  /* The emulated battery temperature */
  int emul_batt_temperature;
  /* The emulated mix_soc */
  int emul_mix_soc;
  /* The emulated voltage_ocv */
  int emul_voltage_ocv;
#endif

  struct thermal_zone_device *tzd;
  struct fg_config_update_work_params fg_config_update_work_params;
};

int max17332_get_batt_capacity(struct max17332_fg_chip *chip,
	int *capacity, bool *is_full);

#endif // __MAX17332_BATTERY_H_
