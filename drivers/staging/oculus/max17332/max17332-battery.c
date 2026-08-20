// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Maxim Integrated Products, Inc.
 * Author: Maxim Integrated <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/unaligned.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/version.h>
#include "max17332.h"
#include "max17332-battery.h"
#include "max17332-voltage-adjustment.h"
#include "gpio_low_volt.h"
#include <linux/pinctrl/pinctrl.h>

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#if (IS_ENABLED(CONFIG_METASOC))
#include "metasoc.h"
#include "metasoc_config_data.h"
#endif

#define PCKP_RAW_TO_UV(v) ((v * 625) / 2)  /* 312.5 uV per bit */
#define RAW_CAP_TO_UAMPH(c, r) (c * 5000 / ((int)r))
#define RAW_TIMER_TO_MILLISECONDS(t) (t * 1758 / 10) /* 175.8 ms LSB divided by 10 to convert to ms */
#define RAW_TIMERH_TO_MILLISECONDS(th) (th * 11520 * 1000) /* 3.2 hours LSB multiplied by 3600 * 1000 to convert to ms */
#define RAW_TIMERH_TO_HOURS_TIMES_TEN(th) (th * 32) /* lsb is 3.2 hours, multiplying by ten to get 1 decimal point of precision */
#define RAW_POWER_TO_UWATTS(p, r) (p * (800 * 10 / ((int)r))) /* lsb is 800uW for 10mOhm Rsense; 400uW for 20mOhm Rsense */
/* convert SOC 1/256 to uSoC */
#define ONE_DIV_256 3906
#define SOC_TO_USOC(soc) (soc * ONE_DIV_256)

#define DEFAULT_CAPACITY_CRITICAL_LEVEL (3)
#define DEFAULT_CAPACITY_LOW_LEVEL (10)
#define DEFAULT_CAPACITY_HIGH_LEVEL (90)
#define BATT_CAP_CRITICAL_LVL_UPPER_LIMIT (15)
#define BATT_CAP_LOW_LVL_UPPER_LIMIT (BATT_CAP_CRITICAL_LVL_UPPER_LIMIT + 1)
#define DEFAULT_OCCP_THRESHOLD (-1) /* mA */

#define I2C_JITTER_DELAY_MS (850)

/* set vreg_out to default while usb disconnect */
#define BOB_INIT_VOLTAGE_CHG_DELTA_DEFAULT	250000

// Battery Remap Variables
#define BATTERY_LEVEL_FULL_THRESHOLD (95)
#define BATTERY_LEVEL_FULL (100)
#define BATTERY_LEVEL_REMAP_MIN (56)
// Remap 56%~94% to 57%~99% by skip 56%,66%,76%,86%,96%
int BATTERY_MAP_FROM_56_TO_94[] = {
    57, 58, 59, 60, 61, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73, 74, 75, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 87, 88, 89, 90, 91, 92, 93, 94, 95, 97, 98, 99,
};

const int kVoltageAlertScaleFactor = 20000; /* unit = uV */
const int kCurrentAlertScaleFactor = 400000; /* unit = uV -> converted to uA by dividing by rsense */

// QScale capacity step sizes in uAh (microampere-hours)
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

static u8 sip_serial_num_regs[] = {
	0xBA,
	0xE0,
	0xE1,
	0xE6,
	0xE7,
};

static u8 pack_serial_num_regs[] = {
	0xE9,
	0xEA,
	0xEB,
	0xEC,
	0xED,
	0xEE,
	0xEF,
};

static char *batt_supplied_to[] = {
	"max17332-battery",
};


#if (IS_ENABLED(CONFIG_METASOC))
static uint8_t read_persist_data(void *pdata);
static void write_persist_data(void *pdata, uint8_t data);
static uint32_t get_time_in_sec(void *pata);

static metasoc_utility_function_impl utilities = {
	.get_time_in_sec    = get_time_in_sec,
	.read_persist_data  = read_persist_data,
	.write_persist_data = write_persist_data,
};
#endif

static int update_temperature_thresholds(struct max17332_fg_chip *chip, int temp);
static int update_voltage_thresholds(struct max17332_fg_chip *chip, int voltage);
static int max17332_get_mix_soc(struct max17332_fg_chip *chip, u16 *mix_soc);
static int max17332_get_voltage_ocv(struct max17332_fg_chip *chip, int* voltage_ocv);
static int max17332_configure_manual_charging(struct max17332_fg_chip *chip, int manual_charging);
static int max17332_set_voltage_lower_limit(struct max17332_fg_chip *chip,
						int voltage);
static int max17332_set_voltage_upper_limit(struct max17332_fg_chip *chip,
						int voltage);
static void max17332_fg_config_update_free_memory(struct max17332_fg_config_update_data *data);
static int max17332_resolve_pack(struct max17332_fg_chip *chip);
static int max173322_get_ini_rev(struct max17332_fg_chip *chip);

static inline int max17332_raw_current_to_uamps(struct max17332_fg_chip *chip,
												int curr)
{
	return curr * 15625 / ((int)chip->rsense * 10);
}

static enum power_supply_property max17332_fg_battery_props[] = {
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
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_INI_VERSION
};

static struct device_attribute max17332_fg_attrs[] = {
	MAX17332_FG_ATTR(max17332_program_nvm),
	MAX17332_FG_ATTR(nvm_updates_remaining),
	MAX17332_FG_ATTR(voltage_pack_now),
	MAX17332_FG_ATTR(coulomb_counter),
	MAX17332_FG_ATTR(charge_full_nom),
	MAX17332_FG_ATTR(rsense),
	MAX17332_FG_ATTR(voltage_alert_max),
	MAX17332_FG_ATTR(voltage_alert_min),
	MAX17332_FG_ATTR(current_alert_max),
	MAX17332_FG_ATTR(current_alert_min),
	MAX17332_FG_ATTR(timer),
	MAX17332_FG_ATTR(timerh),
	MAX17332_FG_ATTR(battery_chgstat),
	MAX17332_FG_ATTR(batt_status),
	MAX17332_FG_ATTR(prot_status),
	MAX17332_FG_ATTR(prot_alrt),
	MAX17332_FG_ATTR(fet_status),
	MAX17332_FG_ATTR(batt_config),
	MAX17332_FG_ATTR(batt_config2),
	MAX17332_FG_ATTR(comm_status),
	MAX17332_FG_ATTR(slack),
	MAX17332_FG_ATTR(ini_rev),
	MAX17332_FG_ATTR(sip_sn),
	MAX17332_FG_ATTR(pack_sn),
	MAX17332_FG_ATTR(bmu_smt_date),
	MAX17332_FG_ATTR(full_cap),
	MAX17332_FG_ATTR(av_cap),
	MAX17332_FG_ATTR(av_soc),
	MAX17332_FG_ATTR(mix_cap),
	MAX17332_FG_ATTR(mix_soc),
	MAX17332_FG_ATTR(vfrem_cap),
	MAX17332_FG_ATTR(vf_soc),
	MAX17332_FG_ATTR(q_residual),
	MAX17332_FG_ATTR(qr_table_00),
	MAX17332_FG_ATTR(qr_table_10),
	MAX17332_FG_ATTR(qr_table_20),
	MAX17332_FG_ATTR(qr_table_30),
	MAX17332_FG_ATTR(rcomp0),
	MAX17332_FG_ATTR(temp_co),
	MAX17332_FG_ATTR(lock),
	MAX17332_FG_ATTR(suspend_battery_pct),
	MAX17332_FG_ATTR(suspend_charge_counter),
	MAX17332_FG_ATTR(suspend_voltage),
	MAX17332_FG_ATTR(change_counter_cp),
	MAX17332_FG_ATTR(change_counter_dropout),
	MAX17332_FG_ATTR(change_counter_ovp),
	MAX17332_FG_ATTR(change_counter_occp),
	MAX17332_FG_ATTR(cycle_count_frac),
	MAX17332_FG_ATTR(controlled_charge_on),
	MAX17332_FG_ATTR(learn_stage),
	MAX17332_FG_ATTR(timer_seconds),
	MAX17332_FG_ATTR(i2c_read_failure_count),
	MAX17332_FG_ATTR(i2c_write_failure_count),
	MAX17332_FG_ATTR(ncgain),
	MAX17332_FG_ATTR(rcell),
	MAX17332_FG_ATTR(manual_charging_on),
	MAX17332_FG_ATTR(last_batt_status),
	MAX17332_FG_ATTR(last_prot_status),
#ifdef CONFIG_MAX17332_VOLTAGE_ADJUSTMENT
	MAX17332_FG_ATTR(vreg_out_delta_rise),
	MAX17332_FG_ATTR(vreg_out_delta_fall),
	MAX17332_FG_ATTR(vreg_uv_max),
	MAX17332_FG_ATTR(vreg_dropout_min),
	MAX17332_FG_ATTR(vsys_change_period_min),
#endif
#if (IS_ENABLED(CONFIG_METASOC))
	MAX17332_FG_ATTR(rep_soc),
	MAX17332_FG_ATTR(meta_soc),
	MAX17332_FG_ATTR(meta_soc_init_val),
	MAX17332_FG_ATTR(meta_soc_init_time),
	MAX17332_FG_ATTR(meta_soc_enabled),
	MAX17332_FG_ATTR(meta_soc_version),
	MAX17332_FG_ATTR(meta_soc_init),
	MAX17332_FG_ATTR(meta_soc_usoc),
	MAX17332_FG_ATTR(meta_soc_eoc),
	MAX17332_FG_ATTR(meta_soc_eod),
	MAX17332_FG_ATTR(meta_soc_low_batt_shutdown),
	MAX17332_FG_ATTR(meta_soc_config_id),
	MAX17332_FG_ATTR(meta_soc_low_volt_comp_tripped),
	MAX17332_FG_ATTR(meta_soc_usoc_filtered),
	MAX17332_FG_ATTR(meta_soc_peak_voltage_droop_penalty),
	MAX17332_FG_ATTR(meta_soc_remaining_capacity),
#endif
	MAX17332_FG_ATTR(trim1),
	MAX17332_FG_ATTR(target_chg_voltage),
	MAX17332_FG_ATTR(target_chg_current),
	MAX17332_FG_ATTR(batt_cap_low_lvl),
	MAX17332_FG_ATTR(batt_cap_critical_lvl),
	MAX17332_FG_ATTR(design_cap),
	MAX17332_FG_ATTR(n_design_cap),
	MAX17332_FG_ATTR(n_cycles),
	MAX17332_FG_ATTR(n_full_cap_nom),
	MAX17332_FG_ATTR(n_full_cap_rep),
	MAX17332_FG_ATTR(n_timerh),
	MAX17332_FG_ATTR(dqacc),
	MAX17332_FG_ATTR(dpacc),
	MAX17332_FG_ATTR(age),
	MAX17332_FG_ATTR(is_virtual_battery),
	MAX17332_FG_ATTR(fg_config_update_algo_version),
	MAX17332_FG_ATTR(fg_config_update_counter),
	MAX17332_FG_ATTR(fg_config_update_fail_counter),
	MAX17332_FG_ATTR(fg_config_update_success_counter),
	MAX17332_FG_ATTR(fg_config_update_write_fail_register),
	MAX17332_FG_ATTR(fg_config_update_write_fail_register_value),
	MAX17332_FG_ATTR(fg_config_update_entry_reason),
	MAX17332_FG_ATTR(n_design_voltage),
	MAX17332_FG_ATTR(unmapped_capacity),
	MAX17332_FG_ATTR(battery_name),
	MAX17332_FG_ATTR(dietemp),
	MAX17332_FG_ATTR(fstat),
	MAX17332_FG_ATTR(fstat2),
	MAX17332_FG_ATTR(hprotcfg),
	MAX17332_FG_ATTR(fotpstat),
	MAX17332_FG_ATTR(fprotstat),
	MAX17332_FG_ATTR(n_battstatus),
};

static struct max17332_fg_config_update_data fg_config_for_update = {
	"fg-config", NULL, 0, "fg-config-ignore", NULL, 0
};
static struct max17332_fg_config_update_data fg_config_for_validate = {
	"fg-config", NULL, 0, "fg-validate-ignore", NULL, 0
};

static unsigned int hex2dec(char *hex_data)
{
	int len;
	unsigned int num = 0;
	int temp, bits, i;

	len = strlen(hex_data);

	for (temp = 0, i = 0; i < len; i++, temp = 0) {
		if (isdigit(hex_data[i]))
			temp = hex_data[i] - 48;
		else if (hex_data[i] < 'A' ||
				(hex_data[i] > 'F' && hex_data[i] < 'a') || hex_data[i] > 'z')
			continue;

		if (isalpha(hex_data[i]))
			temp = isupper(hex_data[i]) ? hex_data[i] - 55 : hex_data[i] - 87;

		bits = (len - i - 1) * 4;
		temp = temp << bits;
		num += temp;
	}

	return num;
}

static ssize_t program_nvm_memory_store(struct max17332_fg_chip *chip,
						const char *buf, size_t len)
{
	int ret = 0, i = 0;
	int size;
	u16 data;
	const u8 *ptr;
	char *fw_path;
	char addr_buf[8] = {0}, data_buf[8] = {0};
	u16 memory_data = 0;
	u8 memory_addr = 0, try_counter = 0;
	const struct firmware *fw;

	fw_path = kstrdup(buf, GFP_ATOMIC);
	if (!fw_path)
		return -EINVAL;

	fw_path = strim(fw_path);

	if (len < 1)
		return -EINVAL;

	ret = request_firmware(&fw, fw_path, chip->dev);

	if (ret < 0 || !fw->data || !fw->size) {
		pr_err("%s : Failed to get ini file\n", __func__);
		return ret;
	}

	size = fw->size;
	ptr = fw->data;

	if (size != ((MAX17332_NVM_HIGH_ADDR - MAX17332_NVM_BASE_ADDR + 1) * 15)) {
		pr_err("%s : Firmware size is not correct!\n", __func__);
		release_firmware(fw);
		return ret;
	}

	size = (MAX17332_NVM_HIGH_ADDR - MAX17332_NVM_BASE_ADDR + 1);
	mutex_lock(&chip->lock);

	ret = max17332_lock_write_protection(chip->max17332, false);
	if (ret < 0) {
		pr_err("%s: fail to unlock protection on max17332: %d\n",
			__func__, ret);
		return ret;
	}

nvm_block_copy:
	// Break infinite loop
	if (try_counter > 3) {
		pr_err("%s: fail to write nvm memory after trying three times.\n", __func__);
		ret = -EAGAIN;
		goto error;
	}
	try_counter++;

	for (i = 0; i < size; i++) {
		strncpy(addr_buf, (char *)&ptr[(i * 15) + 3], 2);
		strncpy(data_buf, (char *)&ptr[(i * 15) + 10], 4);
		memory_addr = hex2dec(addr_buf);
		memory_data = hex2dec(data_buf);

		if ((memory_addr >= REG_NROMID0_NVM) && (memory_addr <= REG_NROMID3_NVM))
			continue;

		ret = max17332_write(chip->regmap_nvm, memory_addr, memory_data);
		if (ret < 0) {
			pr_err("%s: fail to write reg 0x%X\n", __func__, memory_addr);
			goto error;
		}

		// Verify NVM locations
		ret = max17332_read(chip->regmap_nvm, memory_addr, &data);
		if (ret < 0) {
			pr_err("%s: fail to read reg 0x%X\n", __func__, memory_addr);
			goto error;
		}

		if (data != memory_data) {
			pr_err("Verify failed, memory addr: [0x%X]\n", memory_addr);
			goto nvm_block_copy;
		}
	}

	// Clear CommStat.NVError
	ret = max17332_read(chip->regmap, REG_COMMSTAT, &data);
	ret |= max17332_write(chip->regmap, REG_COMMSTAT, (data & ~(MAX17332_COMMSTAT_NVERROR)));
	if (ret < 0) {
		pr_err("%s: fail to access REG_COMMSTAT\n", __func__);
		goto error;
	}

	// Initiate Block Copy
	ret = max17332_write(chip->regmap, REG_COMMAND, MAX17332_COMMAND_COPY_NVM);
	if (ret < 0) {
		pr_err("%s: fail to initiate block copy\n", __func__);
		goto error;
	}

	// Wait t_block for Copy to Complete
	// ! Maximum time for block programming is 7360 ms
	mdelay(1000);

	// Check CommStat.NVError bit
	ret = max17332_read(chip->regmap, REG_COMMSTAT, &data);
	if (ret < 0) {
		pr_err("%s: fail to read REG_COMMSTAT\n", __func__);
		goto error;
	}

	if ((data & MAX17332_COMMSTAT_NVERROR) == MAX17332_COMMSTAT_NVERROR) {
		pr_err("%s: fail to clear CommStat.NVError\n", __func__);
		goto error;
	}

	// Initiate Full Reset
	ret = max17332_write(chip->regmap, REG_COMMAND, MAX17332_COMMAND_FULL_RESET);
	if (ret < 0) {
		pr_err("%s: fail to sent Full Reset command\n", __func__);
		goto error;
	}

	// Wait 10ms for IC to Rest
	mdelay(10);

	// Verify All Nonvolatile Memory Locations Recalled Correctly
	for (i = 0; i < size; i++) {
		strncpy(addr_buf, (char *)&ptr[(i * 15) + 3], 2);
		strncpy(data_buf, (char *)&ptr[(i * 15) + 10], 4);
		memory_addr = hex2dec(addr_buf);
		memory_data = hex2dec(data_buf);
		if ((memory_addr >= REG_NROMID0_NVM) && (memory_addr <= REG_NROMID3_NVM))
			continue;

		ret = max17332_read(chip->regmap_nvm, memory_addr, &data);
		if (ret < 0) {
			pr_err("%s: fail to read register 0x%X\n", __func__, memory_addr);
			goto error;
		}

		if (data != memory_data) {
			pr_err("%s: Failed to verify register [0x%X], ini_data: [0x%04X], read_data: [0x%04X]\n",
				__func__, memory_addr, memory_data, data);
			goto error;
		}
	}

	// Clear CommStat.NVError
	ret = max17332_read(chip->regmap, REG_COMMSTAT, &data);
	ret |= max17332_write(chip->regmap, REG_COMMSTAT, (data & ~(MAX17332_COMMSTAT_NVERROR)));
	if (ret < 0) {
		pr_err("%s: fail to access REG_COMMSTAT\n", __func__);
		goto error;
	}

	// FuelGauge Reset Command
	max17332_update_bits(chip->regmap, REG_CONFIG2,
		MAX17332_CONFIG2_POR_CMD,
		MAX17332_CONFIG2_POR_CMD);

	// Wait 500 ms for POR_CMD to clear;
	mdelay(500);

	ret = max17332_lock_write_protection(chip->max17332, true);
	if (ret < 0) {
		pr_err("%s: fail to lock protection on max17332: %d\n",
			__func__, ret);
	}

	mutex_unlock(&chip->lock);

	pr_info("%s: NVM programmed successfully!\n", __func__);

	return len;
error:
	ret = max17332_lock_write_protection(chip->max17332, true);
	if (ret < 0) {
		pr_err("%s: fail to lock protection on max17332: %d\n",
			__func__, ret);
	}

	mutex_unlock(&chip->lock);
	return ret;
}

static int read_reg_raw_value(int reg, struct max17332_fg_chip *chip,
		char* buf)
{
	u16 reg_val;
	int ret;

	ret = max17332_read(chip->regmap, reg, &reg_val);
	if (ret < 0)
		return ret;

	return snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg_val);
}

static int read_mult_reg_char_values(u8 *regs, size_t num_regs,
		struct regmap *chip_regmap, char *chip_buffer, size_t buffer_size)
{
	u16 reg_val;
	int ret;
	u8 reg;
	char upper_byte, lower_byte;
	int reg_ind = 0, buff_ind = 0;

	size_t buffer_str_size = buffer_size - 2; /* last two elements are used for newline and null termination */

	/* Each register value is 16 bits -> 2 chars. Store 1 or 2 chars into buffer limited by buffer size */
	for (; reg_ind < num_regs && buff_ind < buffer_str_size ; reg_ind++) {
		reg = regs[reg_ind];

		ret = max17332_read(chip_regmap, reg, &reg_val);
		if (ret < 0)
			return ret;

		upper_byte = (char) (reg_val >> 8);
		lower_byte = (char) (reg_val & 0xFF);

		chip_buffer[buff_ind] = upper_byte;
		buff_ind++;

		/* Check if buffer can fit the lower byte before storing */
		if (buff_ind < buffer_str_size) {
			chip_buffer[buff_ind] = lower_byte;
			buff_ind++;
		}
	}

	chip_buffer[buff_ind] = '\n';
	buff_ind++;

	return buff_ind;
}

static int64_t getTimerTotalMs(struct max17332_fg_chip *chip) {
	u16 timer_16_bit;
	u16 timerh_16_bit = 0;
	u16 timerh2_16_bit = 1;
	long int timer;
	long int timerh;
	int ret;
	int counter = 0;

	while (timerh_16_bit != timerh2_16_bit && counter < 5){
		// Read TimerH Register
		ret = max17332_read(chip->regmap, REG_TIMERH, &timerh_16_bit);
		if (ret < 0)
			return ret;

		// Read Timer Register
		ret = max17332_read(chip->regmap, REG_TIMER, &timer_16_bit);
		if (ret < 0)
			return ret;

		// Read TimerH Register again
		ret = max17332_read(chip->regmap, REG_TIMERH, &timerh2_16_bit);
		if (ret < 0)
			return ret;

		// Increment exit counter
		counter++;
	}
	timer = RAW_TIMER_TO_MILLISECONDS((int64_t)timer_16_bit);
	timerh = RAW_TIMERH_TO_MILLISECONDS(((int64_t)timerh_16_bit) );

	// Get total milliseconds
	return timer + timerh;
}

static ssize_t remaining_nmv_updates_show(struct max17332_fg_chip *chip, char *buf)
{
	int rc = 0, ret = 0;
	u16 val = 0, logical_val = 0;
	u8 number_of_used = 0;

	// Recall indicator flags to determine remaining configuration memory writes
	ret = max17332_write_unlock(chip->max17332, chip->regmap,
			REG_COMMAND, MAX17332_COMMAND_RECALL_HISTORY_REMAINING_WRITES);
	if (ret < 0) {
		pr_err("%s: fail to sent Recall History command\n", __func__);
		return ret;
	}

	// Wait t_recall
	mdelay(5);

	ret = max17332_read(chip->regmap_nvm, REG_REMAINING_UPDATES_NVM, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_REMAINING_UPDATES_NVM\n", __func__);
		return ret;
	}

	logical_val = ((val & 0xFF) | ((val >> 8) & 0xFF));
	number_of_used = fls(logical_val);

	rc += snprintf(buf + rc, PAGE_SIZE - rc, "%d\n", (8 - number_of_used));
	return rc;
}

ssize_t max17332_uevent_nonpsy_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0, i, bufind = 0;
	size_t prop_str_len;
	struct device_attribute *dev_attr;
	char *prop_buf;

	prop_buf = (char *) get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (i = MAX17332_FG_REMAINING_NVM_UPDATES; i < (int)ARRAY_SIZE(max17332_fg_attrs); i++) {
		dev_attr = &max17332_fg_attrs[i];

		ret = max17332_fg_show_attrs(dev, dev_attr, prop_buf);
		if (ret < 0)
			continue;

		prop_str_len = strlen(dev_attr->attr.name) + 11 + ret; /* length includes prefix "max17332_" + name + "=" + value */
		if (bufind + prop_str_len > PAGE_SIZE - 1) {
			pr_err("%s: Exceeded buffer length for uevent_nonpsy\n", __func__);
			ret = -ENOMEM;
			goto out;
		}

		bufind += snprintf(buf + bufind, prop_str_len, "max17332_%s=%s", dev_attr->attr.name, prop_buf);
	}

	ret = bufind;

out:
	free_page((unsigned long)prop_buf);

	return ret;
}

static struct device_attribute max17332_uevent_nonpsy =
{
	.attr = {.name = "uevent_nonpsy", .mode = 0440},
	.show = max17332_uevent_nonpsy_show,
};

#if (IS_ENABLED(CONFIG_METASOC))
int update_metasoc_enabled_persist(struct max17332_fg_chip *chip) {
	// Update metasoc register to enable/disable metasoc
	int ret = 0;
	u16 reg;
	ret = max17332_read(chip->regmap_nvm, REG_N_DESGIN_VOLT, &reg);
	if (ret < 0) {
		dev_err(chip->dev, "%s: failed to read REG_N_DESGIN_VOLT\n", __func__);
		return ret;
	}
	if (chip->metasoc_enabled) {
		// Set BIT4 to 1
		reg |= BIT(4);
		ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm, REG_N_DESGIN_VOLT, reg);
	} else {
		// Set BIT4 to 0
		reg &= ~BIT(4);
		ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm, REG_N_DESGIN_VOLT, reg);
	}
	if (ret < 0) {
		dev_err(chip->dev, "%s: failed to write REG_N_DESGIN_VOLT\n", __func__);
		return ret;
	}
	return 0;
}

int handle_low_battery_shutdown(struct max17332_fg_chip *chip) {
	int capacity = 0;
	u16 val = 0;
	int ret = max17332_read(chip->regmap, REG_AV_CAP, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_AV_CAP\n", __func__);
		return ret;
	}

	capacity = RAW_CAP_TO_UAMPH(sign_extend32(val, 15), chip->rsense) / 1000;
	metasoc_set_low_battery_shutdown(0, capacity);

	return 0;
}
#endif

static int
max17332_fg_get_voltage_alert_threshold(struct max17332_fg_chip *chip,
					int *min, int *max)
{
	int ret;
	u16 reg;

	ret = max17332_read(chip->regmap, REG_VALRTTH, &reg);
	if (ret < 0)
		return ret;

	if (min) {
		*min = reg & 0xFF;
		*min *= kVoltageAlertScaleFactor;
	}

	if (max) {
		*max = reg >> 8;
		*max *= kVoltageAlertScaleFactor;
	}

	return 0;
}

static void max17332_remap_capacity(int* capacity) {
  int remapped_capacity = *capacity;
  if (*capacity >= BATTERY_LEVEL_FULL_THRESHOLD) {
    remapped_capacity = BATTERY_LEVEL_FULL;
  } else if (*capacity >= BATTERY_LEVEL_REMAP_MIN) {
    remapped_capacity = BATTERY_MAP_FROM_56_TO_94[*capacity - BATTERY_LEVEL_REMAP_MIN];
  }
  *capacity = remapped_capacity;
}

ssize_t max17332_fg_show_attrs(struct device* dev, struct device_attribute* attr, char* buf) {
  struct power_supply* psy = dev_get_drvdata(dev);
  struct max17332_fg_chip* chip = power_supply_get_drvdata(psy);
  const ptrdiff_t offset = attr - max17332_fg_attrs;
  int ret = 0, intval = 0;
  int64_t longintval = 0;
  u16 reg;
  unsigned int past = jiffies_to_msecs(jiffies - chip->usb_disc_time_stamp);

	dev_dbg(chip->dev, "%s\n", __func__);

	if (atomic_read(&chip->fg_update_in_progress)) {
		dev_err(chip->dev, "%s: can't shot attr due to fg update", __func__);
		return -EBUSY;
	}

	if (chip->i2c_jitter && past < I2C_JITTER_DELAY_MS)
		msleep(I2C_JITTER_DELAY_MS - past);

	switch (offset) {
	case MAX17332_FG_REMAINING_NVM_UPDATES:
		ret = remaining_nmv_updates_show(chip, buf);
		break;
	case MAX17332_FG_VOLTAGE_PACK_NOW:
		ret = max17332_read(chip->regmap, REG_PCKP, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", PCKP_RAW_TO_UV(reg));
		break;
	case MAX17332_FG_COULOMB_COUNTER:
		ret = max17332_read(chip->regmap, REG_QH, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense));
		break;
	case MAX17332_FG_CHARGE_FULL_NOM:
		ret = max17332_read(chip->regmap, REG_FULLCAPNOM, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense));
		break;
	case MAX17332_FG_RSENSE:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->rsense);
		break;
	case MAX17332_FG_VOLTAGE_ALERT_MAX:
		ret = max17332_fg_get_voltage_alert_threshold(chip, NULL,
								&intval);
		if (ret)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_VOLTAGE_ALERT_MIN:
		ret = max17332_fg_get_voltage_alert_threshold(chip, &intval,
								NULL);
		if (ret)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_CURRENT_ALERT_MAX:
		ret = max17332_read(chip->regmap, REG_IALRTTH, &reg);
		if (ret < 0)
			return ret;
		reg >>= 8;
		intval = sign_extend32(reg, 7) * kCurrentAlertScaleFactor / ((int) chip->rsense);
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_CURRENT_ALERT_MIN:
		ret = max17332_read(chip->regmap, REG_IALRTTH, &reg);
		if (ret < 0)
			return ret;
		reg &= 0xFF;
		intval = sign_extend32(reg, 7) * kCurrentAlertScaleFactor / ((int) chip->rsense);
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_TIMER:
		ret = max17332_read(chip->regmap, REG_TIMER, &reg);
		if (ret < 0)
			return ret;
		intval = RAW_TIMER_TO_MILLISECONDS(reg);
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_TIMERH:
		ret = max17332_read(chip->regmap, REG_TIMERH, &reg);
		if (ret < 0)
			return ret;
		intval = RAW_TIMERH_TO_HOURS_TIMES_TEN(reg);
		ret = snprintf(buf, MAX_INT_DIGITS, "%d.%d\n", intval / 10, intval % 10);
		break;
	case MAX17332_FG_TIMER_SECONDS:
		longintval = getTimerTotalMs(chip);
		/* Printing total timer value in seconds */
		if (longintval >= 0) {
			ret = snprintf(buf, MAX_INT_DIGITS, "%lld\n", longintval / 1000);
		} else {
			ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", 0);
		}
		break;
	case MAX17332_FG_BATTERY_CHGSTAT:
		ret = max17332_read(chip->regmap, REG_CHGSTAT, &reg);
		if (ret < 0)
			return ret;

		ret = snprintf(buf,
				CHG_STAT_SIZE,
				"Dropout:%d CP:%d CT:%d CC:%d CV:%d\n",
				reg & BIT_STATUS_DROPOUT ? 1 : 0,
				reg & BIT_STATUS_CP ? 1 : 0,
				reg & BIT_STATUS_CT ? 1 : 0,
				reg & BIT_STATUS_CC ? 1 : 0,
				reg & BIT_STATUS_CV ? 1 : 0);
		break;
	case MAX17332_FG_BATT_STATUS:
		ret = read_reg_raw_value(REG_STATUS, chip, buf);
		if (ret < 0)
			return ret;
		break;
	case MAX17332_FG_PROT_STATUS:
		ret = read_reg_raw_value(REG_PROTSTATUS, chip, buf);
		if (ret < 0)
			return ret;
		break;
	case MAX17332_FG_PROT_ALERT:
		ret = read_reg_raw_value(REG_PROTALRTS, chip, buf);
		if (ret < 0)
			return ret;
		break;
	case MAX17332_FG_FET_STATUS:
		ret = read_reg_raw_value(REG_PROTCFG2, chip, buf);
		if (ret < 0)
			return ret;
		break;
	case MAX17332_FG_BATT_CONFIG:
		ret = read_reg_raw_value(REG_CONFIG, chip, buf);
		if (ret < 0)
			return ret;
		break;
	case MAX17332_FG_BATT_CONFIG2:
		ret = read_reg_raw_value(REG_CONFIG2, chip, buf);
		if (ret < 0)
			return ret;
		break;
	case MAX17332_FG_COMM_STATUS:
		ret = read_reg_raw_value(REG_COMMSTAT, chip, buf);
		if (ret < 0)
			return ret;
		break;
	case MAX17332_FG_SLACK:
		ret = max17332_read(chip->regmap_nvm, REG_SLACK, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_INI_REV:
		if (chip->ini_rev == 0) {
			ret = max173322_get_ini_rev(chip);
			if (ret < 0)
				dev_err(chip->dev, "%s : Failed to read ini rev\n", __func__);
		}
		ret = snprintf(buf, MAX_INT_DIGITS, "%u\n", chip->ini_rev);
		break;
	case MAX17332_FG_SIP_SN:
		if (chip->sip_serial_number[0] == '\0') {
			ret = read_mult_reg_char_values(sip_serial_num_regs, ARRAY_SIZE(sip_serial_num_regs), chip->regmap_nvm,
					chip->sip_serial_number, SIP_SERIAL_NUMBER_SIZE);
			if (ret < 0)
				return ret;
		}
		ret = snprintf(buf, SIP_SERIAL_NUMBER_SIZE, "%s", chip->sip_serial_number);
		break;
	case MAX17332_FG_PACK_SN:
		if (chip->pack_serial_number[0] == '\0') {
			ret = read_mult_reg_char_values(pack_serial_num_regs, ARRAY_SIZE(pack_serial_num_regs), chip->regmap_nvm,
					chip->pack_serial_number, PACK_SERIAL_NUMBER_SIZE);
			if (ret < 0)
				return ret;
		}
		ret = snprintf(buf, PACK_SERIAL_NUMBER_SIZE, "%s", chip->pack_serial_number);
		break;
	case MAX17332_FG_BMU_SMT_DATE:
		ret = max17332_read(chip->regmap_nvm, REG_BMU_SMT_DATE, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_FULL_CAP:
		ret = max17332_read(chip->regmap, REG_FULL_CAP, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense));
		break;
	case MAX17332_FG_AV_CAP:
		ret = max17332_read(chip->regmap, REG_AV_CAP, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense));
		break;
	case MAX17332_FG_AC_SOC:
		ret = max17332_read(chip->regmap, REG_AC_SOC, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg >> 8);
		break;
	case MAX17332_FG_MIX_CAP:
		ret = max17332_read(chip->regmap, REG_MIX_CAP, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense));
		break;
	case MAX17332_FG_MIX_SOC:
		ret = max17332_get_mix_soc(chip, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg);
		break;
#if (IS_ENABLED(CONFIG_METASOC))
	case MAX17332_FG_REP_SOC:
		ret = max17332_read_cached(chip->regmap, chip->cache, REG_REPSOC, &reg);
		reg = reg >> 8; /* RepSOC LSB: 1/256 % */
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg);
		break;
	case MAX17332_FG_META_SOC:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc);
		break;
	case MAX17332_FG_META_SOC_INIT_VAL:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_init_val);
		break;
	case MAX17332_FG_META_SOC_INIT_TIME:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_init_time);
		break;
	case MAX17332_FG_META_SOC_ENABLED:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_enabled);
		break;
	case MAX17332_FG_META_SOC_VERSION:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_version);
		break;
	case MAX17332_FG_META_SOC_INIT:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_init);
		break;
	case MAX17332_FG_META_SOC_USOC:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_stats.usoc);
		break;
	case MAX17332_FG_META_SOC_EOC:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_stats.soc_end_of_charge);
		break;
	case MAX17332_FG_META_SOC_EOD:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_stats.soc_end_of_discharge);
		break;
	case MAX17332_FG_META_SOC_CONFIG_ID:
		ret = snprintf(buf, MAX_INT_DIGITS, "%u\n", metasoc_get_config_id(0));
		break;
	case MAX17332_FG_META_SOC_LOW_VOLT_COMP_TRIPPED:
		ret = snprintf(buf, MAX_INT_DIGITS, "%ld\n", chip->low_volt_comp_tripped);
		break;
	case MAX17332_FG_META_SOC_USOC_FILTERED:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_stats.usoc_filtered);
		break;
	case MAX17332_FG_META_SOC_PEAK_VOLTAGE_DROOP_PENALTY:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->metasoc_stats.droop_penalty);
		break;
	case MAX17332_FG_META_SOC_REMAINING_CAPACITY:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",  chip->metasoc_stats.remaining_capacity);
		break;
#endif
	case MAX17332_FG_VFREM_CAP:
		ret = max17332_read(chip->regmap, REG_VFREM_CAP, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense));
		break;
	case MAX17332_FG_VF_SOC:
		ret = max17332_read(chip->regmap, REG_VF_SOC, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg >> 8);
		break;
	case MAX17332_FG_Q_RESIDUAL:
		ret = max17332_read(chip->regmap, REG_Q_RESIDUAL, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense));
		break;
	case MAX17332_FG_QR_TABLE_00:
		ret = max17332_read(chip->regmap_nvm, REG_QR_TABLE_00, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_QR_TABLE_10:
		ret = max17332_read(chip->regmap_nvm, REG_QR_TABLE_10, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_QR_TABLE_20:
		ret = max17332_read(chip->regmap_nvm, REG_QR_TABLE_20, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_QR_TABLE_30:
		ret = max17332_read(chip->regmap_nvm, REG_QR_TABLE_30, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_RCOMP0:
		ret = max17332_read(chip->regmap_nvm, REG_RCOMP0, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_TEMP_CO:
		ret = max17332_read(chip->regmap_nvm, REG_TEMP_CO, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_LOCK:
		ret = max17332_read(chip->regmap, REG_LOCK, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_SUSPEND_BATTERY_PCT:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",chip->suspend_battery_pct);
		break;
	case MAX17332_FG_SUSPEND_CHARGE_COUNTER:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->suspend_battery_charge_counter);
		break;
	case MAX17332_FG_SUSPEND_VOLTAGE:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->suspend_voltage);
		break;
	case MAX17332_FG_CP_CHANGE_COUNTER:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->cp_change_counter);
		break;
	case MAX17332_FG_DROPOUT_CHANGE_COUNTER:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->dropout_change_counter);
		break;
	case MAX17332_FG_OVP_CHANGE_COUNTER:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->ovp_change_counter);
		break;
	case MAX17332_FG_OCCP_CHANGE_COUNTER:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->occp_change_counter);
		break;
	case MAX17332_FG_CYCLE_COUNT_FRAC:
		ret = max17332_read(chip->regmap, REG_CYCLES, &reg);
		if (ret < 0)
			return ret;
		reg = reg * 25;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d.%d\n", reg / 100, reg % 100);
		break;
	case MAX17332_FG_CONTROLLED_CHARGE_ON:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->controlled_charge_on_written);
		break;
	case MAX17332_FG_LEARN_STAGE:
		ret = max17332_read(chip->regmap, REG_LEARNCFG, &reg);
		if (ret < 0)
			return ret;
		reg = (reg >> 4) & 0b111;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg);
		break;
	case MAX17332_FG_I2C_READ_FAILURE_COUNT:
		intval = max17332_get_i2c_read_fail_count();
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_I2C_WRITE_FAILURE_COUNT:
		intval = max17332_get_i2c_write_fail_count();
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_NCGAIN:
		ret = max17332_read(chip->regmap_nvm, REG_NCGAIN, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%x\n", reg);
		break;
	case MAX17332_FG_RCELL:
		ret = max17332_read(chip->regmap, REG_RCELL, &reg);
		if (ret < 0)
			return ret;
		longintval = (uint64_t)reg * 1000000 / 4096; /* lsb is 1/4096 Ohms*/
		ret = snprintf(buf, MAX_INT_DIGITS, "%lld\n", longintval);
		break;
	case MAX17332_FG_MANUAL_CHARGING_ON:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->manual_charging_on_written);
		break;
	case MAX17332_FG_LAST_BATT_STATUS:
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", chip->cached_status_register_val);
		break;
	case MAX17332_FG_LAST_PROT_STATUS:
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", chip->cached_prot_status_register_val);
		break;
#ifdef CONFIG_MAX17332_VOLTAGE_ADJUSTMENT
	case MAX17332_FG_VREG_OUT_DELTA_RAISE:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",
				chip->max17332->voltage_adjustment->vreg_out_delta_rise);
		break;
	case MAX17332_FG_VREG_OUT_DELTA_FALL:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",
				chip->max17332->voltage_adjustment->vreg_out_delta_fall);
		break;
	case MAX17332_FG_VREG_UV_MAX:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",
				chip->max17332->voltage_adjustment->vreg_uv_max);
		break;
	case MAX17332_FG_VREG_DROPOUT_MIN:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",
				chip->max17332->voltage_adjustment->vreg_dropout_min);
		break;
	case MAX17332_FG_VREG_VSYS_CHANGE_PERIOD_MIN:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",
				chip->max17332->voltage_adjustment->vsys_change_period_min);
		break;
#endif
	case MAX17332_FG_TRIM1:
		ret = max17332_read(chip->regmap, REG_TRIM1, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg & MAX17332_TRIM1_MASK);
		break;
	case MAX17332_FG_TARGET_CHG_VOLTAGE:
		ret = max17332_read(chip->regmap, REG_TARGET_CHG_V, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",
			max17332_raw_voltage_to_uvolts(reg));
		break;
	case MAX17332_FG_TARGET_CHG_CURRENT:
		ret = max17332_read(chip->regmap, REG_TARGET_CHG_I, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n",
			max17332_raw_current_to_uamps(chip, sign_extend32(reg, 15)));
		break;
	case MAX17332_FG_BATT_CAP_LOW_LVL:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->pdata->cap_low_lvl);
		break;
	case MAX17332_FG_BATT_CAP_CRITICAL_LVL:
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", chip->pdata->cap_critical_lvl);
		break;
	case MAX17332_FG_DESIGN_CAP:
		ret = max17332_read(chip->regmap, REG_DESIGN_CAP, &reg);
		if (ret < 0)
			return ret;
		if (chip->rsense == 0)
			return -EINVAL;
		ret = (reg * 5 * 1000) / chip->rsense; // design capacity in uAh
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", ret);
		break;
	case MAX17332_FG_N_DESIGN_CAP:
		ret = max17332_read(chip->regmap_nvm, REG_N_DESIGN_CAP, &reg);
		if (ret < 0)
			return ret;
		if (chip->rsense == 0)
			return -EINVAL;
		ret = (reg & 0xFFC0) >> 6; // get the design capacity
		reg = reg & 0x0007; // get the qscale
		ret = (ret * qscale_capacity_step_size_uah[reg] * 10) / chip->rsense; // design capacity in uAh
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", ret);
		break;
	case MAX17332_FG_N_CYCLES:
		ret = max17332_read(chip->regmap_nvm, REG_N_CYCLES, &reg);
		if (ret < 0)
			return ret;
		intval = reg >> 3; // raw_cycles
		ret = max17332_read(chip->regmap_nvm, REG_N_NV_CFG2, &reg);
		if (ret < 0)
			return ret;
		reg = reg & 0x3; // fib_scl
		intval = intval * (1 << reg) * 25/100; // cycles
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_N_FULL_CAP_NOM:
		ret = max17332_read(chip->regmap_nvm, REG_N_FULL_CAP_NOM, &reg);
		if (ret < 0)
			return ret;
		if (chip->rsense == 0)
			return -EINVAL;
		ret = (reg * 5 * 1000) / chip->rsense; // full capacity in uAh
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", ret);
		break;
	case MAX17332_FG_N_FULL_CAP_REP:
		ret = max17332_read(chip->regmap_nvm, REG_N_FULL_CAP_REP, &reg);
		if (ret < 0)
			return ret;
		if (chip->rsense == 0)
			return -EINVAL;
		ret = (reg * 5 * 1000) / chip->rsense; // full capacity in uAh
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", ret);
		break;
	case MAX17332_FG_N_TIMERH:
		ret = max17332_read(chip->regmap_nvm, REG_N_TIMERH, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg * 32/10);
		break;
	case MAX17332_FG_DQACC:
		ret = max17332_read(chip->regmap, REG_QACC, &reg);
		if (ret < 0)
			return ret;
		if (chip->rsense == 0)
			return -EINVAL;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg * 2000 * 10/chip->rsense);
		break;
	case MAX17332_FG_DPACC:
		ret = max17332_read(chip->regmap, REG_PACC, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg / 16);
		break;
	case MAX17332_FG_AGE:
		ret = max17332_read(chip->regmap, REG_AGE, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg >> 8);
		break;
	case MAX17332_IS_VIRTUAL_BATTERY:
		// MAX17332 DRIVER CAN ONLY SUPPORT ONE BATTERY, SO THIS IS ALWAYS 1
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", 1);
		break;
	case MAX17332_FG_CONFIG_UPDATE_ALGO_VER:
		ret = snprintf(buf, MAX_INT_DIGITS, "%u\n", chip->fg_config_update_work_params.fg_config_update_algo_version);
		break;
	case MAX17332_FG_CONFIG_UPDATE_CNTR:
		ret = snprintf(buf, MAX_INT_DIGITS, "%u\n", chip->fg_config_update_work_params.fg_config_update_counter);
		break;
	case MAX17332_FG_CONFIG_UPDATE_FAIL_CNTR:
		ret = snprintf(buf, MAX_INT_DIGITS, "%u\n", chip->fg_config_update_work_params.fg_config_update_fail_counter);
		break;
	case MAX17332_FG_CONFIG_UPDATE_SUCCESS_CNTR:
		ret = snprintf(buf, MAX_INT_DIGITS, "%u\n", chip->fg_config_update_work_params.fg_config_update_success_counter);
		break;
	case MAX17332_FG_CONFIG_UPDATE_WRITE_FAIL_REG:
		ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", chip->fg_config_update_work_params.fg_config_update_write_fail_register);
		break;
	case MAX17332_FG_CONFIG_UPDATE_WRITE_FAIL_REG_VAL:
		ret = snprintf(buf, MAX_INT_DIGITS, "%04x\n", chip->fg_config_update_work_params.fg_config_update_write_fail_register_value);
		break;
	case MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON:
		ret = snprintf(buf, MAX_INT_DIGITS, "%u\n", chip->fg_config_update_work_params.fg_config_update_entry_reason);
		break;
	case MAX17332_FG_N_DESIGN_VOLTAGE:
		ret = max17332_read(chip->regmap_nvm, REG_N_DESGIN_VOLT, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", reg);
		break;
	case MAX17332_FG_CONFIG_UNMAPPED_CAPACITY:
		max17332_get_batt_capacity(chip, &intval, NULL);
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_BATTERY_NAME:
		// This is simply the battery ID, e.g., 0, 1, etc.
		// We simply return '0'
		ret = snprintf(buf, MAX_INT_DIGITS, "0\n");
		break;
	case MAX17332_FG_DIETEMP:
		ret = max17332_read(chip->regmap, REG_DIETEMP, &reg);
		if (ret < 0)
			return ret;
		// The DieTemp register is signed and represents increments of 1/256-deg.
		// We convert to decidegrees.
		intval = (int16_t)reg;			// Get signed 32-bit
		intval = intval * 10 / 256;
		ret = snprintf(buf, MAX_INT_DIGITS, "%d\n", intval);
		break;
	case MAX17332_FG_FSTAT:
		ret = max17332_read(chip->regmap, REG_FSTAT, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_FSTAT2:
		ret = max17332_read(chip->regmap, REG_FSTAT2, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_HPROTCFG:
		ret = max17332_read(chip->regmap, REG_HPROTCFG, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_FOTPSTAT:
		ret = max17332_read(chip->regmap, REG_FOTPSTAT, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_FPROTSTAT:
		ret = max17332_read(chip->regmap, REG_FPROTSTAT, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	case MAX17332_FG_N_BATTSTATUS:
		ret = max17332_read(chip->regmap_nvm, REG_N_BATTSTATUS, &reg);
		if (ret < 0)
			return ret;
		ret = snprintf(buf, REG_RAW_VAL_SIZE, "%04x\n", reg);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

ssize_t max17332_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17332_fg_chip *chip = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max17332_fg_attrs;
	int ret;
	int controlled_charge;
	int manual_charging;
	u16 data;
	int value;
	int alert_min, alert_max;

	dev_dbg(chip->dev, "%s offset=%ld\n", __func__, offset);

	if (atomic_read(&chip->fg_update_in_progress)) {
		dev_err(chip->dev, "%s: can't shot attr due to fg update", __func__);
		return -EBUSY;
	}

	switch (offset) {
	case MAX17332_FG_PROGRAM_NVM:
		program_nvm_memory_store(chip, buf, count);
		ret = count;
		break;
	case MAX17332_FG_CONTROLLED_CHARGE_ON:
		ret = kstrtoint(buf, 10, &controlled_charge);
		if (ret >= 0 && (controlled_charge == 0 || controlled_charge == 1 )) {

			// Case 1: controlled charge is changing to 1
			if (chip->controlled_charge_on_written == 0 && controlled_charge == 1) {
				// Write to nVChgCfg1
				data = NVCHGCFG1_CONTROLLED_CHARGE_ON;
				ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm,
						REG_N_VCHG_CFG1, data);
				if (ret < 0) {
					pr_err("%s: fail to write REG_N_VCHG_CFG1\n", __func__);
					return ret;
				}

				// Write to nStepVolt
				data = NVSTEPVOLT_CONTROLLED_CHARGE_ON;
				ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm,
						REG_N_STEP_V, data);
				if (ret < 0) {
					pr_err("%s: fail to write REG_N_STEP_V\n", __func__);
					return ret;
				}

				// Store the state of controlled charge
				chip->controlled_charge_on_written = controlled_charge;
			}

			// Case 2: controlled charge is changing to 0
			if (chip->controlled_charge_on_written == 1 && controlled_charge == 0) {

				// Write to nVChgCfg1
				data = NVCHGCFG1_CONTROLLED_CHARGE_OFF;
				ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm,
						REG_N_VCHG_CFG1, data);
				if (ret < 0) {
					pr_err("%s: fail to write REG_N_VCHG_CFG1\n", __func__);
					return ret;
				}

				// Write to nStepVolt
				data = NVSTEPVOLT_CONTROLLED_CHARGE_OFF;
				ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm,
						REG_N_STEP_V, data);
				if (ret < 0) {
					pr_err("%s: fail to write REG_N_STEP_V\n", __func__);
					return ret;
				}

				// Store the state of controlled charge
				chip->controlled_charge_on_written = controlled_charge;
			}
		} else {
			dev_err(chip->dev, "%s: invalid controlled_charge_on value: %d \n", __func__, controlled_charge);
		}
		ret = count;
		break;
	case MAX17332_FG_MANUAL_CHARGING_ON:
		ret = kstrtoint(buf, 10, &manual_charging);
		if(chip->manual_charging_on_written != manual_charging && manual_charging >= 0 && manual_charging <= 1) {
			if(max17332_configure_manual_charging(chip, manual_charging) != 0) {
				pr_err("%s : failed to enable Manual Charging\n", __func__);
			}
			ret = count;
		}
		else {	// Invalid value
			ret = -EINVAL;
		}
		break;
#ifdef CONFIG_MAX17332_VOLTAGE_ADJUSTMENT
	case MAX17332_FG_VREG_OUT_DELTA_RAISE:
		ret = kstrtoint(buf, 10, &value);
		chip->max17332->voltage_adjustment->vreg_out_delta_rise = value;
		ret = count;
		break;
	case MAX17332_FG_VREG_OUT_DELTA_FALL:
		ret = kstrtoint(buf, 10, &value);
		chip->max17332->voltage_adjustment->vreg_out_delta_fall = value;
		ret = count;
		break;
	case MAX17332_FG_VREG_UV_MAX:
		ret = kstrtoint(buf, 10, &value);
		chip->max17332->voltage_adjustment->vreg_uv_max = value;
		ret = count;
		break;
	case MAX17332_FG_VREG_DROPOUT_MIN:
		ret = kstrtoint(buf, 10, &value);
		chip->max17332->voltage_adjustment->vreg_dropout_min = value;
		ret = count;
		break;
	case MAX17332_FG_VREG_VSYS_CHANGE_PERIOD_MIN:
		ret = kstrtoint(buf, 10, &value);
		chip->max17332->voltage_adjustment->vsys_change_period_min = value;
		ret = count;
		break;
#endif
#if (IS_ENABLED(CONFIG_METASOC))
	case MAX17332_FG_META_SOC_INIT_VAL:
		ret = 0;
		// only allow setting the init value once
		if (chip->metasoc_init_val == -1) {
			ret = kstrtoint(buf, 10, &chip->metasoc_init_val);
		}
		if (!ret)
			ret = count;
		break;
	case MAX17332_FG_META_SOC_INIT_TIME:
		ret = 0;
		// only allow setting the init time once
		if (chip->metasoc_init_time == -1) {
			ret = kstrtoint(buf, 10, &chip->metasoc_init_time);
		}
		if (!ret)
			ret = count;
		break;
	case MAX17332_FG_META_SOC_ENABLED:
		ret = kstrtoint(buf, 10, &chip->metasoc_enabled);
		if (!ret)
			ret = count;
		update_metasoc_enabled_persist(chip);
		break;
	case MAX17332_FG_META_SOC_VERSION:
		ret = kstrtoint(buf, 10, &chip->metasoc_version);
		if (!ret)
			ret = count;
		break;
	case MAX17332_FG_META_SOC_INIT:
		ret = kstrtoint(buf, 10, &chip->metasoc_init);
		if (!ret) {
			ret = count;
		}
		break;
	case MAX17332_FG_META_LOW_BATT_SHUTDOWN:
		ret = kstrtoint(buf, 10, &value);
		if (ret)
			break;

		ret = count;
		if (value)
			handle_low_battery_shutdown(chip);
		break;
#endif
	case MAX17332_FG_VOLTAGE_ALERT_MIN:
		ret = kstrtoint(buf, 10, &value);
		if (ret)
			break;

		value /= 1000;
		if (value < MAX17332_VOLTAGE_THRESHOLD_MIN ||
			value > MAX17332_VOLTAGE_THRESHOLD_MAX) {
			ret = -EINVAL;
			break;
		}

		ret = max17332_fg_get_voltage_alert_threshold(chip, NULL,
								&alert_max);
		if (ret)
			break;

		if (value > alert_max) {
			ret = -EINVAL;
			break;
		}

		ret = max17332_set_voltage_lower_limit(chip, value);
		if (ret)
			break;

		ret = count;
		break;
	case MAX17332_FG_VOLTAGE_ALERT_MAX:
		ret = kstrtoint(buf, 10, &value);
		if (ret)
			break;

		value /= 1000;
		if (value < MAX17332_VOLTAGE_THRESHOLD_MIN ||
			value > MAX17332_VOLTAGE_THRESHOLD_MAX) {
			ret = -EINVAL;
			break;
		}

		ret = max17332_fg_get_voltage_alert_threshold(chip, &alert_min,
								NULL);
		if (ret)
			break;

		if (value < alert_min) {
			ret = -EINVAL;
			break;
		}

		ret = max17332_set_voltage_upper_limit(chip, value);
		if (ret)
			break;

		ret = count;
		break;
	case MAX17332_FG_BATT_CAP_LOW_LVL:
		ret = kstrtoint(buf, 10, &value);
		if (ret < 0) {
			dev_err(dev, "Failed to read batt_cap_low_lvl from buffer: %d\n", ret);
		}

		if (value >= BATT_CAP_LOW_LVL_UPPER_LIMIT) {
			dev_err(dev, "The proposed capacity low level was greater than %d\n",
					BATT_CAP_LOW_LVL_UPPER_LIMIT);
		}

		if (value <= chip->pdata->cap_critical_lvl) {
			dev_err(dev, "The low capacity level was less than the critical level");
		}

		chip->pdata->cap_low_lvl = value;
		ret = count;
		power_supply_changed(chip->battery);
		break;
	case MAX17332_FG_BATT_CAP_CRITICAL_LVL:
		ret = kstrtoint(buf, 10, &value);
		if (ret < 0) {
			dev_err(dev, "Failed to read batt_cap_critical_lvl from buffer: %d\n", ret);
			break;
		}

		if (value >= BATT_CAP_CRITICAL_LVL_UPPER_LIMIT) {
			dev_err(dev, "The proposed capacity critical level was greater than %d\n",
					BATT_CAP_CRITICAL_LVL_UPPER_LIMIT);
			ret = -EINVAL;
			break;
		}

		if (value >= chip->pdata->cap_low_lvl) {
			dev_err(dev,
				"The critical capacity level was greater than the low level");
			ret = -EINVAL;
			break;
		}

		ret = count;
		chip->pdata->cap_critical_lvl = value;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int max17332_configure_manual_charging(struct max17332_fg_chip *chip, int manual_charging)
{
	int ret;
	u16 data;

	if(manual_charging == 1) {
		// Reduce current
		ret = max17332_read(chip->regmap, REG_CONFIG, &data);
		if (ret < 0) {
			pr_err("%s : fail to read REG_CONFIG\n", __func__);
		}
		data |= MAX17332_CONFIG_MANCHG;
		ret = max17332_write_unlock(chip->max17332, chip->regmap,
				REG_CONFIG, data);
		if (ret < 0) {
			pr_err("%s : fail to write REG_CONFIG\n", __func__);
		}

		// Set ChargingVoltage to ensure 4.1V Max
		data = NVCHGVOLT_MANUAL_CHARGE_4_1V;
		ret = max17332_write_unlock(chip->max17332, chip->regmap,
				REG_CHGVOLTAGE, data);
		if (ret < 0) {
			pr_err("%s: fail to write REG_CHGVOLTAGE\n", __func__);
		}

		data = chip->pdata->manual_charge_current_reg_val;
		ret = max17332_write_unlock(chip->max17332, chip->regmap,
				REG_CHGCURRENT, data);
		if (ret < 0) {
			pr_err("%s: fail to write REG_CHGCURRENT\n", __func__);
		}

		// Retain the value in sysfs
		chip->manual_charging_on_written = manual_charging;

	} else {
		// Enable autonomous charging
		ret = max17332_read(chip->regmap, REG_CONFIG, &data);
		if (ret < 0) {
			pr_err("%s : fail to read REG_CONFIG\n", __func__);
		}
		data &= ~MAX17332_CONFIG_MANCHG;
		ret = max17332_write_unlock(chip->max17332, chip->regmap,
				REG_CONFIG, data);
		if (ret < 0) {
			pr_err("%s : fail to write REG_CONFIG\n", __func__);
		}
		// Retain the value in sysfs
		chip->manual_charging_on_written = manual_charging;
	}
	return ret;
}


static int max17332_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(max17332_fg_attrs); i++) {
		rc = device_create_file(dev, &max17332_fg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}

	rc = device_create_file(dev, &max17332_uevent_nonpsy);
	if (rc) {
		dev_err(dev, "%s: failed to create uevent_nonpsy (%d)\n", __func__, rc);
		device_remove_file(dev, &max17332_uevent_nonpsy);
	}

	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max17332_fg_attrs[i]);
	return rc;
}

static int max17332_get_fullcapnom(struct max17332_fg_chip *chip)
{
	int ret = 0;
	u16 reg = 0;

	ret = max17332_read(chip->regmap, REG_FULLCAPNOM, &reg);
	if (ret < 0)
		return ret;
	return RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense);
}

static int max17332_get_fullcaprep(struct max17332_fg_chip *chip)
{
	int ret = 0;
	u16 reg = 0;

	ret = max17332_read(chip->regmap, REG_FULLCAPREP, &reg);
	if (ret < 0)
		return ret;
	return RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense);
}

static int max17332_get_nfullcapnom(struct max17332_fg_chip *chip)
{
	int ret = 0;
	u16 reg = 0;

	ret = max17332_read(chip->regmap_nvm, REG_N_FULL_CAP_NOM, &reg);
	if (ret < 0)
		return ret;
	return RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense);
}

static int max17332_get_nfullcaprep(struct max17332_fg_chip *chip)
{
	int ret = 0;
	u16 reg = 0;

	ret = max17332_read(chip->regmap_nvm, REG_N_FULL_CAP_REP, &reg);
	if (ret < 0)
		return ret;
	return RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense);
}

static int max17332_get_temperature(struct max17332_fg_chip *chip, int *temp)
{
	int ret;
	u16 val;

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	if (chip->emul_batt_temperature > INVALID_EMUL_BATT_TEMPERATURE) {
		*temp = chip->emul_batt_temperature;
		return 0;
	}
#endif

	ret = max17332_read(chip->regmap, REG_TEMP, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_TEMP\n", __func__);
		return ret;
	}

	*temp = sign_extend32(val, 15);
	/* The value is converted into centigrade scale */
	/* Units of LSB = 1 / 256 degree Celsius */
	*temp = (*temp * 10) >> 8;
	return 0;
}

static int max17332_get_temperature_alert_min(struct max17332_fg_chip *chip,
											int *temp)
{
	int ret;
	u16 val;

	ret = max17332_read(chip->regmap, REG_TALRTTH, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_TALRTTH\n", __func__);
		return ret;
	}

	/* Convert 1DegreeC LSB to 0.1DegreeC LSB */
	*temp = sign_extend32(val & 0xff, 7) * 10;

	return 0;
}

static int max17332_get_temperature_alert_max(struct max17332_fg_chip *chip,
											int *temp)
{
	int ret;
	u16 val;

	ret = max17332_read(chip->regmap, REG_TALRTTH, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_TALRTTH\n", __func__);
		return ret;
	}

	/* Convert 1DegreeC LSB to 0.1DegreeC LSB */
	*temp = sign_extend32(val >> 8, 7) * 10;

	return 0;
}

static int max17332_get_battery_health(struct max17332_fg_chip *chip, int *health)
{
	int ret;
	u16 val;

	ret = max17332_read(chip->regmap, REG_PROTSTATUS, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_PROTSTATUS\n", __func__);
		return ret;
	}

	if (val & BIT_PREQF_INT) {
		*health = POWER_SUPPLY_HEALTH_UNKNOWN;
	} else if ((val & BIT_TOOHOTC_INT) ||
			(val & BIT_TOOHOTD_INT) ||
			(val & BIT_DIEHOT_INT)) {
		*health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if ((val & BIT_UVP_INT) ||
			(val & BIT_PERMFAIL_INT) ||
			(val & BIT_SHDN_INT)) {
		*health = POWER_SUPPLY_HEALTH_DEAD;
	} else if ((val & BIT_TOOCOLDC_INT) ||
			(val & BIT_TOOCOLDD_INT)) {
		*health = POWER_SUPPLY_HEALTH_COLD;
	} else if (val & BIT_OVP_INT) {
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if ((val & BIT_QOVFLW_INT) ||
			(val & BIT_OCCP_INT) ||
			(val & BIT_ODCP_INT)) {
		*health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	} else if (val & BIT_CHGWDT_INT) {
		*health = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
	} else {
		*health = POWER_SUPPLY_HEALTH_GOOD;
	}

	return 0;
}

static int max17332_set_temp_lower_limit(struct max17332_fg_chip *chip,
										int temp)
{
	int ret;
	u16 data;

	ret = max17332_read(chip->regmap, REG_TALRTTH, &data);
	if (ret < 0) {
		pr_err("%s: fail to read REG_TALRTTH\n", __func__);
		return ret;
	}

	/* Input in deci-centigrade, convert to centigrade */
	temp /= 10;

	data &= 0xFF00;
	data |= (temp & 0xFF);

	ret = max17332_write_unlock(chip->max17332, chip->regmap, REG_TALRTTH, data);
	if (ret < 0) {
		pr_err("%s: fail to write REG_TALRTTH\n", __func__);
		return ret;
	}

	return 0;
}

static int max17332_set_temp_upper_limit(struct max17332_fg_chip *chip,
										int temp)
{
	int ret;
	u16 data;

	ret = max17332_read(chip->regmap, REG_TALRTTH, &data);
	if (ret < 0) {
		pr_err("%s: fail to read REG_TALRTTH\n", __func__);
		return ret;
	}

	/* Input in deci-centigrade, convert to centigrade */
	temp /= 10;

	data &= 0xFF;
	data |= ((temp << 8) & 0xFF00);

	ret = max17332_write_unlock(chip->max17332, chip->regmap, REG_TALRTTH, data);
	if (ret < 0) {
		pr_err("%s: fail to write REG_TALRTTH\n", __func__);
		return ret;
	}

	return 0;
}

static int max17332_set_voltage_lower_limit(struct max17332_fg_chip *chip,
										int voltage)
{
	int ret;
	u16 data;

	dev_info(chip->dev, "%s: setting voltage alert, vmin: %d\n", __func__,
		voltage);

	ret = max17332_read(chip->regmap, REG_VALRTTH, &data);
	if (ret < 0) {
		pr_err("%s: fail to read REG_VALRTTH\n", __func__);
		return ret;
	}

	// 20 mv is the LSB, so we should divide by 20
	voltage /= 20;

	data &= 0xFF00;
	data |= (voltage & 0xFF);

	ret = max17332_write_unlock(chip->max17332, chip->regmap, REG_VALRTTH, data);
	if (ret < 0) {
		pr_err("%s: fail to write REG_VALRTTH\n", __func__);
		return ret;
	}

	return 0;
}

static int max17332_set_voltage_upper_limit(struct max17332_fg_chip *chip,
										int voltage)
{
	int ret;
	u16 data;

	ret = max17332_read(chip->regmap, REG_VALRTTH, &data);
	if (ret < 0) {
		pr_err("%s: fail to read REG_VALRTTH\n", __func__);
		return ret;
	}

	dev_info(chip->dev, "%s: setting voltage alert, vmax: %d\n", __func__,
		voltage);

	// 20 mv is the LSB, so we should divide by 20
	voltage /= 20;

	data &= 0xFF;
	data |= ((voltage << 8) & 0xFF00);

	ret = max17332_write_unlock(chip->max17332, chip->regmap, REG_VALRTTH, data);
	if (ret < 0) {
		pr_err("%s: fail to write REG_VALRTTH\n", __func__);
		return ret;
	}

	return 0;
}

static int max17332_set_min_capacity_alert_th(struct max17332_fg_chip *chip,
											unsigned int th)
{
	int ret;
	u16 data;

	ret = max17332_read(chip->regmap, REG_SALRTTH, &data);
	if (ret < 0) {
		pr_err("%s: fail to read REG_SALRTTH\n", __func__);
		return ret;
	}

	data &= 0xFF00;
	data |= (th & 0xFF);

	ret = max17332_write(chip->regmap, REG_SALRTTH, data);
	if (ret < 0) {
		pr_err("%s: fail to write REG_SALRTTH\n", __func__);
		return ret;
	}

	return 0;
}

static int max17332_set_max_capacity_alert_th(struct max17332_fg_chip *chip,
											unsigned int th)
{
	int ret;
	u16 data;

	ret = max17332_read(chip->regmap, REG_SALRTTH, &data);
	if (ret < 0) {
		pr_err("%s: fail to read REG_SALRTTH\n", __func__);
		return ret;
	}

	data &= 0xFF;
	data |= ((th & 0xFF) << 8);

	ret = max17332_write(chip->regmap, REG_SALRTTH, data);
	if (ret < 0) {
		pr_err("%s: fail to write REG_SALRTTH\n", __func__);
		return ret;
	}

	return 0;
}

#if (IS_ENABLED(CONFIG_METASOC))
// Fuel Gauge has three states: charging, discharging, and relaxing
static bool max17332_is_discharging(struct max17332_dev *dev)
{
	u16 val = 0;
	int ret = max17332_read(dev->regmap_pmic, REG_FPROTSTAT, &val);

	if (ret < 0)
		return false;

	return (val & MAX17332_PROTSTAT_ISDIS);
}

static void low_voltage_comparator_update(void *pdata, bool is_throttle)
{
	struct max17332_fg_chip *chip = (struct max17332_fg_chip *) pdata;

	if (is_throttle)
		test_and_set_bit(0, &chip->low_volt_comp_tripped);
}

static uint8_t read_persist_data(void *pdata)
{
	u16 reg = 0;
	uint8_t data = 0;
	int ret;
	struct max17332_fg_chip *chip = (struct max17332_fg_chip *) pdata;
	// Read data from metasoc persist register
	ret = max17332_read(chip->regmap_nvm, REG_N_DESGIN_VOLT, &reg);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to read metasoc register REG_N_DESGIN_VOLT: %d\n", __func__, ret);
	}
	// we only care about the 6 lsb
	data = reg & 0x3F;
	return data;
}

static void write_persist_data(void *pdata, uint8_t data)
{
	int ret;
	u16 reg;
	struct max17332_fg_chip *chip = (struct max17332_fg_chip *) pdata;

	// Read current register value to preserve upper 8 bits
	ret = max17332_read(chip->regmap_nvm, REG_N_DESGIN_VOLT, &reg);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to read metasoc register REG_N_DESGIN_VOLT: %d\n", __func__, ret);
		return;
	}

	// Lower 6 bits would be overwritten
	reg &= ~0x3F;

	// we want to write to the 6 LSb
	data &= 0x3F;

	// if metasoc is enabled, set bit 4 to 1
	if (chip->metasoc_enabled)
		data |= BIT(4);
	else
		data &= ~BIT(4);

	// Merge new data with preserved upper bits
	reg |= data;

	ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm, REG_N_DESGIN_VOLT, reg);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to write metasoc register REG_N_DESGIN_VOLT: %d\n", __func__, ret);
	}
}

static uint32_t get_time_in_sec(void *pdata)
{
	struct max17332_fg_chip *chip = (struct max17332_fg_chip *) pdata;
	int64_t time = getTimerTotalMs(chip) / 1000;

	if (time < 0)
		time = 0;

	return (uint32_t) (time & 0xFFFFFFFF);
}

static int get_metasoc_params(struct max17332_fg_chip *chip, uint8_t repsoc, bool is_full, metasoc_param *params)
{
	u16 val = 0;
	int vavg = 0, vnow = 0, curr = 0, temp = 0, level = 0, rbatt = 0,
		capacity = 0, ocv = 0, qres = 0;
	bool is_charging = false;
	union power_supply_propval psu;

	int ret = max17332_read(chip->regmap, REG_AVGVCELL, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_AVGVCELL\n", __func__);
		return ret;
	}
	vavg = (max17332_raw_voltage_to_uvolts(val) / 1000);

	ret = max17332_read(chip->regmap, REG_VCELL, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_VCELL\n", __func__);
		return ret;
	}
	vnow = (max17332_raw_voltage_to_uvolts(val) / 1000);

	ret = max17332_read(chip->regmap, REG_CURRENT, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_CURRENT\n", __func__);
		return ret;
	}
	curr = max17332_raw_current_to_uamps(chip, sign_extend32(val, 15)) / 1000;

	ret = max17332_get_temperature(chip, &temp);
	if (ret < 0)
		return ret;

	ret = max17332_read(chip->regmap, REG_MIX_SOC, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_MIX_SOC\n", __func__);
		return ret;
	}
	/* convert to soc with 2 digits of resolution past dec point*/
	val = (SOC_TO_USOC(val)) / 10000 ;
	level = val;

	ret = max17332_read(chip->regmap, REG_RCELL, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_RCELL\n", __func__);
		return ret;
	}
	rbatt = (int) (val * 1000) / 4096; /* lsb is 1/4096 Ohms*/

	ret = max17332_read(chip->regmap, REG_AV_CAP, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_AV_CAP\n", __func__);
		return ret;
	}
	capacity = RAW_CAP_TO_UAMPH(sign_extend32(val, 15), chip->rsense) / 1000;

	ret = max17332_read(chip->regmap, REG_Q_RESIDUAL, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_Q_RESIDUAL\n", __func__);
		return ret;
	}
	qres = RAW_CAP_TO_UAMPH(sign_extend32(val, 15), chip->rsense);

	ret = max17332_read(chip->regmap, REG_VFOCV, &val);
	if (ret < 0) {
		pr_err("%s: fail to read VFOCV\n", __func__);
		return ret;
	}

	ocv = max17332_raw_voltage_to_uvolts(val);

	ret = power_supply_get_property(chip->usb_charger,
			POWER_SUPPLY_PROP_ONLINE, &psu);
	if (ret) {
		dev_err(chip->dev, "%s: fail to read usb status: %d\n", __func__, ret);
		return ret;
	}
	is_charging = !max17332_is_discharging(chip->max17332) && !!psu.intval;

	params->avg_voltage_mv = vavg;
	params->inst_voltage_mv = vnow;
	params->inst_current_ma = curr;
	params->temp_c = temp / 10;
	params->battery_level = level;
	params->battery_impedance_mohm = rbatt;
	params->battery_capacity_mah = capacity;
	params->is_charging = is_charging;
	params->is_fully_charged = is_full;
	params->low_voltage_comp_tripped = test_and_clear_bit(0, &chip->low_volt_comp_tripped);
	params->ocv_mv = ocv / 1000;
	params->is_repsoc_zero = (repsoc == 0);
	params->full_capacity_nominal_mah = max17332_get_nfullcapnom(chip) / 1000; // convert to mAh
	params->block_discharge = 0;
	params->qres = qres / 1000; // convert to mAh

	return ret;
}

static int get_metasoc(struct max17332_fg_chip *chip, uint8_t repsoc, bool is_full, uint8_t *metasoc)
{
	int err = -1;
	metasoc_param params;
	metasoc_config_data *config_data;

	/* Pick configurations based on pack id */
	if (chip->pdata->pack_id == MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0) {
		config_data = &config_data_sc50_cellv_0_packv_0;
	}
	else if (chip->pdata->pack_id == MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0){
		config_data = &config_data_hammerheadpack_cellv_0_packv_0;
	}
	else if (chip->pdata->pack_id == MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0){
		config_data = &config_data_sc50_cellv_2_packv_0;
	}
	else {
		dev_err(chip->dev, "%s: invalid pack id: %d\n", __func__, chip->pdata->pack_id);
		return err;
	}

	if (!chip->metasoc_init)
		return err;

	if (chip->metasoc_needs_init) {
		config_data->utility_function = utilities;

		if (chip->metasoc_init_val != -1)
			config_data->last_metasoc = chip->metasoc_init_val;
		else
			config_data->last_metasoc = repsoc;

		if (chip->metasoc_init_time != -1)
			config_data->last_update_time_s = chip->metasoc_init_time;
		else
			config_data->last_update_time_s = 0;


		// Default to v4
		pr_info("%s: metasoc version: %d\n", __func__, chip->metasoc_version);
		if(chip->metasoc_version == 2)
			config_data->enable_zero_repsoc_convergence = DISABLE_ZERO_REPSOC_CONVERGENCE;
		else if(chip->metasoc_version == 3)
			config_data->enable_zero_repsoc_convergence = ENABLE_DELAYED_ZERO_REPSOC_CONVERGENCE;
		else if(chip->metasoc_version == 4)
			config_data->enable_zero_repsoc_convergence = ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE;
		else if(chip->metasoc_version == 5)
			config_data->enable_zero_repsoc_convergence = CONVERGENCE_THROUGH_PEAK_POWER_MANAGER;
		else
			config_data->enable_zero_repsoc_convergence = ENABLE_QRES_BASED_ZERO_REPSOC_CONVERGENCE;

		config_data->private_data = chip;
		err = metasoc_init(0, config_data);

		if (!err) {
			chip->metasoc_needs_init = false;
		} else {
			pr_err("%s: could not init metasoc\n", __func__);
			return err;
		}
	}

	err = get_metasoc_params(chip, repsoc, is_full, &params);
	if (err) {
		pr_err("%s: could not get metasoc params\n", __func__);
		return err;
	}

	err = metasoc_update_soc(0, &params);
	if (err) {
		pr_err("%s: could not update metasoc\n", __func__);
		return err;
	}

	err = metasoc_get_soc(0, metasoc);
	if (err) {
		pr_err("%s: could not get metasoc\n", __func__);
		return err;
	}

	err = metasoc_get_internal_stats(0, &chip->metasoc_stats);
	if (err) {
		pr_err("%s: could not get metasoc stats\n", __func__);
		return err;
	}

	return err;
}
#endif

int max17332_get_batt_capacity(struct max17332_fg_chip *chip,
									int *capacity, bool *is_full)
{
	int ret = 0;
	u16 reg = 0;
	u16 protstat = 0;
	int local_capacity = 0;
	static bool last_full_state = false;
	bool is_batt_full = false;

#if (IS_ENABLED(CONFIG_METASOC))
	uint8_t metasoc = 0;
	int metasoc_err = 0;
	metasoc_config_data *config_data;
#endif

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	if (chip->emul_batt_capacity >= 0) {
		*capacity = chip->emul_batt_capacity;
		return 0;
	}
#endif

	max17332_read(chip->regmap, REG_PROTSTATUS, &protstat);
	is_batt_full = protstat & BIT_PROTSTATUS_FULL;
	if (is_full) {
		*is_full = is_batt_full;
	}

	ret = max17332_read_cached(chip->regmap, chip->cache, REG_REPSOC, &reg);
	if (ret < 0)
		return ret;

	local_capacity = reg >> 8; /* repsoc lsb: 1/256 % */

	/* Setting all capacities to repsoc as a default */
	*capacity =	local_capacity;
	chip->cached_battery_pct = local_capacity;

#if (IS_ENABLED(CONFIG_METASOC))

	chip->cached_repsoc = local_capacity;

	/* Pick configurations based on pack id */
	if (chip->pdata->pack_id == MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0) {
		config_data = &config_data_sc50_cellv_0_packv_0;
	}
	else if (chip->pdata->pack_id == MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0) {
		config_data = &config_data_hammerheadpack_cellv_0_packv_0;
	}
	else if (chip->pdata->pack_id == MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0) {
		config_data = &config_data_sc50_cellv_2_packv_0;
	}
	else {
		dev_err(chip->dev, "%s: invalid pack id: %d\n", __func__, chip->pdata->pack_id);
		return 0; //return 0 to avoid breaking other code, repsoc would be returned instead of metasoc
	}

	if (chip->metasoc_init) {
		metasoc_err = get_metasoc(chip, local_capacity, is_batt_full, &metasoc);
		chip->metasoc = metasoc;
		if (metasoc_err) {
			pr_err("%s: could not read metasoc\n", __func__);
		}

		if (!metasoc_err && chip->metasoc_enabled)
			local_capacity = metasoc;
	} else {
		if (local_capacity < config_data->soc_init_val)
			local_capacity = config_data->soc_init_val;
	}
#endif

	*capacity = local_capacity;

	if (last_full_state != (bool)(protstat & BIT_PROTSTATUS_FULL)) {
		last_full_state = (bool) (protstat & BIT_PROTSTATUS_FULL);
		if (chip->requires_fg_update) {
			if (last_full_state) {
				unsigned int delay_ms = 3 * 60 * 1000; // 3min

				pr_info("%s: battery full, scheduling fg update in 3 min\n", __func__);
				/* schedule delayed work for 3 minutes after fully charged */

				/* Block suspend until the update thread runs */
				pm_wakeup_event(chip->max17332->dev, delay_ms + 1000);
				schedule_delayed_work(&chip->fg_config_update_work, msecs_to_jiffies(delay_ms));
			} else {
				pr_info("%s: cancelling fg update\n", __func__);
				cancel_delayed_work(&chip->fg_config_update_work);
			}
		}
	}

	// Caching battery capacity to use in suspend telemetry
	chip->cached_battery_pct = *capacity;

	return 0;
}

static int max17332_get_mix_soc(struct max17332_fg_chip *chip, u16 *mix_soc)
{
	int ret = 0;
	u16 reg = 0;

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	if (chip->emul_mix_soc > INVALID_EMUL_MIX_SOC) {
		*mix_soc = chip->emul_mix_soc;
		return 0;
	}
#endif

	ret = max17332_read(chip->regmap, REG_MIX_SOC, &reg);
	if (ret < 0)
		return ret;

	*mix_soc = reg >> 8; /* MIX_SOC LSB: 1/256 % */

	return 0;
}

static int max17332_get_voltage_ocv(struct max17332_fg_chip *chip, int *voltage_ocv)
{
	int ret = 0;
	u16 reg = 0;

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	if (chip->emul_voltage_ocv > INVALID_EMUL_VOLTAGE_OCV) {
		*voltage_ocv = chip->emul_voltage_ocv;
		return 0;
	}
#endif

	ret = max17332_read(chip->regmap, REG_VFOCV, &reg);
	if (ret < 0)
		return ret;

	*voltage_ocv = max17332_raw_voltage_to_uvolts(reg);

	return 0;
}

#ifdef MAX17332_VBAT_SUPPORT
static unsigned int max17332_get_vbat(struct max17332_fg_chip *chip)
{
	int vavg, vbatt, ret;
	u16 val;

	ret = max17332_read(chip->regmap, REG_AVGVCELL, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_AVGVCELL\n", __func__);
		return ret;
	}

	/* bits [0-3] unused */
	vavg = max17332_raw_voltage_to_uvolts(val);
	/* Convert to millivolts */
	vavg /= 1000;

	ret = max17332_read(chip->regmap, REG_VCELL, &val);
	if (ret < 0) {
		pr_err("%s: fail to read REG_VCELL\n", __func__);
		return ret;
	}

	/* bits [0-3] unused */
	vbatt = max17332_raw_voltage_to_uvolts(val);
	/* Convert to millivolts */
	vbatt /= 1000;

	pr_info("%s vavg = %dmV, vbat = %dmV(0x%4x)\n",
					__func__, vavg, vbatt, val);

	return vbatt;
}
#endif

static int max17332_update_fg_status(struct max17332_fg_chip *chip)
{
	int ret = 0;
	int capacity = 0;
	bool is_full = false;
	union power_supply_propval val;

	ret = power_supply_get_property(chip->usb_charger,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (ret) {
		dev_err(chip->dev, "Unable to read USB power supply status: %d\n", ret);
		return ret;
	}

	max17332_get_batt_capacity(chip, &capacity, &is_full);

	mutex_lock(&chip->lock);

	/* update to charging only if usb online and bob enabled */
	chip->fg_status = (val.intval && is_max17332_charger_bob_active(chip->max17332)) ?
		POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_DISCHARGING;

	if (!chip->custom_voltage_adjustment_enabled) {
		/* adjust headroom when charger is connected */
		if (chip->fg_status == POWER_SUPPLY_STATUS_CHARGING)
			max17332_headroom_management(chip->max17332);
	}

	if (val.intval && is_full)
		chip->fg_status = POWER_SUPPLY_STATUS_FULL;

	mutex_unlock(&chip->lock);
	return 0;
}

static int max17332_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr)
{
	struct power_supply *psy = ptr;
	struct max17332_fg_chip *chip = NULL;
	unsigned int delay_ms = 0;
	union power_supply_propval prop;
	int ret;

	if (IS_ERR_OR_NULL(nb))
		return NOTIFY_BAD;

	chip = container_of(nb, struct max17332_fg_chip, nb);
	if (IS_ERR_OR_NULL(chip) || IS_ERR_OR_NULL(chip->usb_charger)) {
		dev_err(chip->dev, "PSY notifier provided object handle is invalid\n");
		return NOTIFY_BAD;
	}

	if (psy != chip->usb_charger || ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &prop);
	if (!ret && !prop.intval) {
			// if USB is disconnected, delay a bit to notify
			delay_ms = I2C_JITTER_DELAY_MS;
			mutex_lock(&chip->lock);
			chip->i2c_jitter = true;
			chip->usb_disc_time_stamp = jiffies;
			mutex_unlock(&chip->lock);
			// ensure system stays awake during processing notify_work
			pm_wakeup_event(chip->dev, I2C_JITTER_DELAY_MS + 100);
	}

	schedule_delayed_work(&chip->notify_work, msecs_to_jiffies(delay_ms));

	return NOTIFY_OK;
}

static void notify_work(struct work_struct *work)
{
	int ret, ocv_threshold_uv;
	union power_supply_propval prop;
	struct max17332_fg_chip *chip =
		container_of(work, struct max17332_fg_chip, notify_work.work);

	mutex_lock(&chip->lock);
	chip->i2c_jitter = false;
	mutex_unlock(&chip->lock);

	ret = power_supply_get_property(chip->usb_charger, POWER_SUPPLY_PROP_ONLINE, &prop);
	if (!ret && prop.intval) {
		u16 prot_status;
		ocv_threshold_uv = MAX17332_DEFAULT_BATT_OVRCHG_THRESHOLD_UV;
		if(chip->pdata)
		{
			switch (chip->pdata->pack_id) {
				case MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0:
					ocv_threshold_uv = MAX17332_HAMMERHEAD_BATT_OVRCHG_THRESHOLD_UV;
					break;
				case MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0:
				case MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0:
					ocv_threshold_uv = MAX17332_SC50_BATT_OVRCHG_THRESHOLD_UV;
					break;
				default:
					break;
			}
		}

		// When power cable is connected, make sure an enabled charger bob is available, so
		// headroom management and overcharging protection could use it to regulate
		// the voltage or enable/disable the charger.
		ret = max17332_get_charger_bob(chip->max17332);
		if (ret < 0)
			dev_err(chip->dev,
					"Failed to get charger BOB when power cable is connected: %d", ret);

		/*
		 * When charger is connected and OVP/OCCP is set in PROTSTATUS,
		 * then, the overcharge_protection interrupt might have not completed
		 * due to charger disconnect/connect.
		 * As charging won't work if these bits are set, recover the state
		 */
		ret = max17332_read(chip->regmap, REG_PROTSTATUS, &prot_status);
		if (ret == 0 && (prot_status & (BIT_OVP_INT | BIT_OCCP_INT)))
			max17332_overcharge_protection(chip->max17332, ocv_threshold_uv);
		else if (chip->custom_voltage_adjustment_enabled) {
			max17332_set_usb_insert_event(chip->max17332, true);
			max17332_headroom_management(chip->max17332);
		}
	}

	if (chip->custom_voltage_adjustment_enabled) {
		/**
		* The default Vreg_out voltage upon cable insertion should be Vcell + 250mV.
		* Set the vreg_delta to 250mV when usb disconnect, and when usb connected,
		* we do not change vreg_delta, just set: vsys = vcell + vreg_delta(250mV)
		*/
		if (!ret && !prop.intval) {
			chip->max17332->regulator_dev->vout_delta = BOB_INIT_VOLTAGE_CHG_DELTA_DEFAULT;
			dev_info(chip->dev, "%s: usb disconnect, vout_delta changed to :%d\n",
						__func__, chip->max17332->regulator_dev->vout_delta);
		}
	}

	ret = max17332_update_fg_status(chip);
	if (ret) {
		dev_err(chip->dev, "Unable to update fg status: %d\n", ret);
		return;
	}

	power_supply_changed(chip->battery);
}

static bool include_reg(uint16_t* ignore_data, int len, uint16_t reg)
{
	int i = 0;
	if (len <= 0 || !ignore_data)
		return true;

	for (i = 0; i < len; i++) {
		if (reg == ignore_data[i]) {
			pr_info("%s: skipping 0x%x\n", __func__, reg);
			return false;
		}
	}

	return true;
}

/**
 * max17332_fg_config_update_allocate_memory_and_read_fg_config - Allocate memory and read fuel gauge config data
 *
 * @chip: Pointer to max17332 fuel gauge chip data structure
 * @data: Pointer to max17332 fuel gauge config data structure
 *
 * This function allocates memory for the fuel gauge config data and ignore data,
 * then reads the corresponding properties from the device tree.
 *
 * Returns 0 if successful, negative error code otherwise. Note that if this function returns
 * 0, memory has been allocated for the config data and must be freed by calling
 * max17332_fg_config_update_free_memory() when it is no longer needed.
 */
static int max17332_fg_config_update_allocate_memory_and_read_fg_config(
	struct max17332_fg_chip *chip,
	struct max17332_fg_config_update_data *data)
{
	int ret = 0;
	struct device_node *np = NULL, *max17332_node = NULL, *pack_node = NULL;
	struct property *prop = NULL, *ignore = NULL;

	if(!chip->pdata) {
		pr_err("%s: chip->pdata not found\n", __func__);
		return -EINVAL;
	}

	max17332_node = of_find_node_by_name(NULL, "max17332");
	if (max17332_node == NULL) {
		pr_err("%s: max17332_node not foundL\n", __func__);
		return -ENODEV;
	}

	np = of_find_node_by_name(max17332_node, "battery");
	if (np == NULL) {
		pr_err("%s: battery node not found\n", __func__);
		return -ENODEV;
	}

	/* Look for the pack node */
	switch (chip->pdata->pack_id) {
	case MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0:
		pack_node = of_find_node_by_name(np, "hammerheadpack-cellv-0-packv-0");
		break;
	case MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0:
		pack_node = of_find_node_by_name(np, "sc50-cellv-0-packv-0");
		break;
	case MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0:
		pack_node = of_find_node_by_name(np, "sc50-cellv-2-packv-0");
		break;
	default:
		pack_node = NULL;
		break;
	}
	if (pack_node == NULL) {
		pr_err("%s pack node not found\n", __func__);
		return -ENODEV;
	}

	prop = of_find_property(pack_node, data->config_name, NULL);
	if (prop == NULL || prop->length <= 0) {
		pr_info("%s: no update data found: %s\n", __func__, data->config_name);
		return -ENOENT;
	}

	ignore = of_find_property(np, data->ignore_name, NULL);
	if (ignore == NULL || ignore->length <= 0) {
		pr_info("%s: no ignore data found\n", __func__);
		data->ignore_size_u16 = 0;
	} else {
		data->ignore_size_u16 = ignore->length / 2;
	}

	/* convert bytes to u16 size */
	data->config_size_u16 = prop->length / 2;

	data->config_data =
		kmalloc(data->config_size_u16 * sizeof(uint16_t), GFP_KERNEL);
	if (data->config_data == NULL) {
		pr_err("%s: failed to alloc memory for config\n", __func__);
		ret = -ENOMEM;
		goto free_mem;
	}

	if (data->ignore_size_u16 > 0) {
		data->ignore_data = kmalloc(
			data->ignore_size_u16 * sizeof(uint16_t), GFP_KERNEL);
		if (data->ignore_data == NULL) {
			pr_err("%s: failed to alloc memory for ignore\n", __func__);
			ret = -ENOMEM;
			goto free_mem;
		}
	}

	ret = of_property_read_u16_array(pack_node, data->config_name,
					data->config_data,
					data->config_size_u16);
	if (ret < 0) {
		pr_info("%s: failed to read config data, err: %d\n", __func__,
			ret);
		goto free_mem;
	}

	if (data->ignore_data) {
		ret = of_property_read_u16_array(np, data->ignore_name,
						data->ignore_data,
						data->ignore_size_u16);
		if (ret < 0) {
			pr_info("%s: failed to read ignore data, err: %d\n",
				__func__, ret);
			goto free_mem;
		}
	}

	return 0;

free_mem:
	max17332_fg_config_update_free_memory(data);
	return ret;
}

static void max17332_fg_config_update_free_memory(struct max17332_fg_config_update_data *data)
{
	if (data != NULL) {
		if (data->config_data != NULL) {
			kfree(data->config_data);
			data->config_data = NULL;
		}
		if (data->ignore_data != NULL) {
			kfree(data->ignore_data);
			data->ignore_data = NULL;
		}
		data->ignore_size_u16 = 0;
		data->config_size_u16 = 0;
	}
}

static void fg_config_update_work(struct work_struct *work)
{
	int retry_intervals[] = {2, 4, 16, 64, 1024}; //in seconds
	struct max17332_fg_chip *chip =
		container_of(work, struct max17332_fg_chip, fg_config_update_work.work);
	union power_supply_propval val;
	int ret = 0, capacity = 0, cnt = 0;
	bool is_full = false, is_charging = false, ver_failed = false;
	uint16_t data = 0, commstat = 0;
	struct max17332_fg_config_update_data *fg_data = &fg_config_for_update;

	if (!fg_data) {
		dev_err(chip->dev, "%s: Config structure not available for updates\n", __func__);
		return;
	}

	ret = power_supply_get_property(chip->usb_charger,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (ret) {
		dev_err(chip->dev, "%s: fail to read usb status: %d\n", __func__, ret);
		return;
	}

	max17332_get_batt_capacity(chip, &capacity, &is_full);
	is_charging = !!val.intval;

	/* only update the fg config when charging and battery is full */
	if (!is_charging || !is_full) {
		pr_info("%s: aborted updating config, conditions unmet\n", __func__);
		return;
	}

	if (max17332_fg_config_update_allocate_memory_and_read_fg_config(chip, fg_data) < 0) {
		pr_info("%s: aborted updating config, read_and_allocate failed\n",
			__func__);
		return;
	}

	pr_info("%s: updating fuel gauge\n", __func__);
	/* to avoid conflicts while writing we flush outstanding work
	 * and disable the interrupts
	 */
	disable_irq(chip->max17332->irq);
	flush_delayed_work(&chip->notify_work);
	flush_delayed_work(&chip->alert_work);
	flush_delayed_work(&chip->alert_enable_work);

	atomic_set(&chip->fg_update_in_progress, 1);

	ret = max17332_read(chip->regmap, REG_COMMSTAT, &commstat);
	if (ret) {
		pr_info("%s: failed to read commstat, err: %d", __func__, ret);
		goto err1;
	}

	/* unlock pages for writing */
	ret = max17332_write(chip->regmap, REG_COMMSTAT, 0x0000);
	ret |= max17332_write(chip->regmap, REG_COMMSTAT, 0x0000);
	if (ret) {
		pr_info("%s: failed to unlock fuel gauge, err: %d", __func__, ret);
		goto err2;
	}

	for (cnt = 0; cnt < fg_data->config_size_u16; cnt += 2) {
		if (include_reg(fg_data->ignore_data, fg_data->ignore_size_u16, fg_data->config_data[cnt])) {
			ret |= max17332_write(chip->regmap_nvm,
				(uint8_t)(fg_data->config_data[cnt] & 0xFF), fg_data->config_data[cnt + 1]);
			ret |= max17332_read(chip->regmap_nvm, (uint8_t)(fg_data->config_data[cnt] & 0xFF), &data);
			if (ret || data != fg_data->config_data[cnt + 1]) {
					pr_info("%s: config does not match, addr; 0x%x\n", __func__,
							fg_data->config_data[cnt]);
					ver_failed = true;
					chip->fg_config_update_work_params.fg_config_update_write_fail_register = (uint8_t)(fg_data->config_data[cnt] & 0xFF);
					chip->fg_config_update_work_params.fg_config_update_write_fail_register_value = data;
					chip->fg_config_update_work_params.fg_config_update_fail_counter++;
					break;
			}
		}
	}

	chip->fg_update_retries++;
	if (ret || ver_failed) {
		if (chip->fg_update_retries <= ARRAY_SIZE(retry_intervals)) {

			unsigned int delay_ms = retry_intervals[chip->fg_update_retries - 1] * 1000;

			/* schedule again in 30 seconds if we can't verify the data */
			pr_info("%s: fg update failed, retrying in %ds\n", __func__, retry_intervals[chip->fg_update_retries - 1]),

			/* Block suspend until the update thread runs */
			pm_wakeup_event(chip->max17332->dev, delay_ms + 1000);

			schedule_delayed_work(&chip->fg_config_update_work, msecs_to_jiffies(delay_ms));
		} else {
			pr_info("%s: fg update failed, not retrying\n", __func__),
			chip->fg_update_retries = 0;
		}
		goto err2;
	}
	else {
		chip->fg_config_update_work_params.fg_config_update_success_counter++;
	}

	pr_info("%s: resetting fuel gauge\n", __func__);
	mutex_lock(&chip->lock);
	chip->ini_rev = 0;
	max17332_update_bits(chip->regmap, REG_CONFIG2,
				MAX17332_CONFIG2_POR_CMD,
				MAX17332_CONFIG2_POR_CMD);
	// Wait 1000 ms for POR_CMD to clear;
	msleep(1000);
	mutex_unlock(&chip->lock);
	chip->fg_update_retries = 0;
	chip->requires_fg_update = false;

err2:
	// lock pages for writing
	ret = max17332_write(chip->regmap, REG_COMMSTAT, commstat);
	ret |= max17332_write(chip->regmap, REG_COMMSTAT, commstat);
	if (ret) {
		pr_info("%s: failed to lock fuel gauge err: %d, continuing",
			__func__, ret);
	}

err1:
	max17332_fg_config_update_free_memory(fg_data);
	atomic_set(&chip->fg_update_in_progress, 0);
	enable_irq(chip->max17332->irq);
	chip->fg_config_update_work_params.fg_config_update_counter++;
	power_supply_changed(chip->battery);
	pr_info("%s: leaving fg_update framework\n", __func__);
	return;
}

/* With batteries that have been used for charge testing, it is possible
 * to get stuck in an overcharge protection loop if the charger is
 * disconnected and reconnected after the overcharge condition clears. This
 * is due to the full bit not getting set before OVP is detected, preventing
 * the battery from properly discharging.
 *
 * To prevent this, the battery capacity level will be forced to be reported
 * as full to allow the device to enter smart charging to discharge the
 * battery if these conditions are met:
 *
 * 1. Overcharge condition is set in PROTSTATUS (OVP or OCCP bit is set)
 * 2. OCV voltage is greater than or equal to 4.43v
 */
static bool overcharge_detected(struct max17332_fg_chip *chip) {
	bool is_overcharged = false;
	int ret, ocv, ocv_threshold_uv;
	u16 prot_status, val;

	if (!chip) {
		if(!chip->pdata)
		{
			pr_err("%s: invalid chip passed\n", __func__);
			return false;
		}
	}

	switch (chip->pdata->pack_id) {
		case MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0:
			ocv_threshold_uv = MAX17332_HAMMERHEAD_BATT_OVRCHG_THRESHOLD_UV;
			break;
		case MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0:
		case MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0:
			ocv_threshold_uv = MAX17332_SC50_BATT_OVRCHG_THRESHOLD_UV;
			break;
		default:
			ocv_threshold_uv = MAX17332_DEFAULT_BATT_OVRCHG_THRESHOLD_UV;
			break;
	}

	ret = max17332_read(chip->regmap, REG_PROTSTATUS, &prot_status);
	if (ret == 0 && (prot_status & (BIT_OVP_INT | BIT_OCCP_INT))) {
		ret = max17332_read(chip->regmap, REG_VFOCV, &val);
		if (ret < 0) {
			dev_err(chip->dev, "%s: failed to read VFOCV register: %d\n", __func__, ret);
			return false;
		}

		ocv = max17332_raw_voltage_to_uvolts(val);
		if (ocv > ocv_threshold_uv) {
			is_overcharged = true;
		}
	}

	return is_overcharged;
}

static int max17332_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	u16 reg;
	struct max17332_fg_chip *chip =
		power_supply_get_drvdata(psy);
	int ret;
	unsigned int past = jiffies_to_msecs(jiffies - chip->usb_disc_time_stamp);

	if (chip->i2c_jitter && past < I2C_JITTER_DELAY_MS)
		msleep(I2C_JITTER_DELAY_MS - past);

  switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
      /* force re-evaluation */
      ret = max17332_update_fg_status(chip);
      if (ret < 0)
        return ret;
      val->intval = chip->fg_status;
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
      ret = max17332_read(chip->regmap, REG_VCELL, &reg);
      if (ret < 0)
        return ret;
      // Caching voltage to use in suspend telemetry
      chip->cached_voltage = reg;
      val->intval = max17332_raw_voltage_to_uvolts(reg);
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_REP:
      ret = max17332_read(chip->regmap, REG_VCELL_REP, &reg);
      if (ret < 0)
        return ret;
      val->intval = max17332_raw_voltage_to_uvolts(reg);
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_AVG:
      ret = max17332_read(chip->regmap, REG_AVGVCELL, &reg);
      if (ret < 0)
        return ret;
      val->intval = max17332_raw_voltage_to_uvolts(reg);
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
      ret = max17332_read(chip->regmap, REG_VEMPTY, &reg);
      if (ret < 0)
        return ret;
      val->intval = reg >> 7;
      val->intval *= 10000; /* Units of LSB = 1000uV */
      break;
    case POWER_SUPPLY_PROP_VOLTAGE_OCV:
    {
      int voltage_ocv;
      ret = max17332_get_voltage_ocv(chip, &voltage_ocv);
      if (ret < 0) {
        return ret;
      }
      val->intval = voltage_ocv;
      break;
    }
    case POWER_SUPPLY_PROP_CAPACITY:
      max17332_get_batt_capacity(chip, &val->intval, NULL);
#if (IS_ENABLED(CONFIG_METASOC))
      if (!chip->metasoc_enabled)
        max17332_remap_capacity(&val->intval);
#else
      max17332_remap_capacity(&val->intval);
#endif
      break;
    case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
      ret = max17332_read(chip->regmap, REG_SALRTTH, &reg);
      if (ret < 0)
        return ret;
      val->intval = reg & 0xFF;
      break;
    case POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX:
      ret = max17332_read(chip->regmap, REG_SALRTTH, &reg);
      if (ret < 0)
        return ret;
      val->intval = (reg >> 8) & 0xFF;
      break;
    case POWER_SUPPLY_PROP_HEALTH:
      ret = max17332_get_battery_health(chip, &val->intval);
      if (ret < 0)
        return ret;
      break;
    case POWER_SUPPLY_PROP_TEMP:
      ret = max17332_get_temperature(chip, &val->intval);
      if (ret < 0)
        return ret;
      break;
    case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
      ret = max17332_get_temperature_alert_min(chip, &val->intval);
      if (ret < 0)
        return ret;
      break;
    case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
      ret = max17332_get_temperature_alert_max(chip, &val->intval);
      if (ret < 0)
        return ret;
      break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
      ret = max17332_read(chip->regmap, REG_CURRENT, &reg);
      if (ret < 0)
        return ret;
      val->intval = max17332_raw_current_to_uamps(chip, sign_extend32(reg, 15));
      break;
    case POWER_SUPPLY_PROP_CURRENT_REP:
      ret = max17332_read(chip->regmap, REG_CURRENT_REP, &reg);
      if (ret < 0)
        return ret;
      val->intval = max17332_raw_current_to_uamps(chip, sign_extend32(reg, 15));
      break;
    case POWER_SUPPLY_PROP_CURRENT_AVG:
      ret = max17332_read(chip->regmap, REG_AVGCURRENT, &reg);
      if (ret < 0)
        return ret;
      val->intval = max17332_raw_current_to_uamps(chip, sign_extend32(reg, 15));
      break;
    case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
      ret = max17332_read(chip->regmap, REG_TTE, &reg);
      if (ret < 0)
        return ret;
      val->intval = (reg * 45) >> 3; /* TTE LSB: 5.625 sec */
      break;
    case POWER_SUPPLY_PROP_TECHNOLOGY:
      val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
      break;
    case POWER_SUPPLY_PROP_CHARGE_COUNTER:
      ret = max17332_read(chip->regmap, REG_REPCAP, &reg);
      if (ret < 0)
        return ret;

		/* The value of this register is signed */
		val->intval = RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense);
		// Caching battery level to use in suspend telemetry
		chip->cached_battery_charge_counter = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = max17332_read(chip->regmap, REG_FULLCAPREP, &reg);
		if (ret < 0)
			return ret;
		val->intval = RAW_CAP_TO_UAMPH(sign_extend32(reg, 15), chip->rsense);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = max17332_read(chip->regmap, REG_CYCLES, &reg);
		if (ret < 0)
			return ret;
		val->intval = reg * 25 / 100; /* lsb is 25% */
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		/* If the driver is up, battery should be present */
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	{
		int cap;
		bool is_full = false;

		ret =  max17332_get_batt_capacity(chip, &cap, &is_full);
		if (ret < 0)
			return ret;

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
		// To make sure we don't break any tests, force the capacity level to
		// full only when emulated battery capacity level has NOT been set
		if (overcharge_detected(chip) &&
			chip->emul_batt_capacity == INVALID_EMUL_BATT_CAPACITY) {
#else
		if (overcharge_detected(chip)) {
#endif
			pr_info("%s: battery in overcharge protection, setting level to FULL\n", __func__);
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		} else {
			if (cap <= chip->pdata->cap_critical_lvl) {
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
				pr_warn("%s: battery is at critical level, and system might shutdown\n",
						__func__);
			} else if (cap <= chip->pdata->cap_low_lvl)
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
			else if (is_full)
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
			else if (cap >= chip->pdata->cap_high_lvl)
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
			else
				val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		}
		break;
	}
	case POWER_SUPPLY_PROP_POWER_NOW:
		ret = max17332_read(chip->regmap, REG_POWER, &reg);
		if (ret < 0)
			return ret;
		val->intval = RAW_POWER_TO_UWATTS(sign_extend32(reg, 15), chip->rsense);
		break;
	case POWER_SUPPLY_PROP_POWER_AVG:
		ret = max17332_read(chip->regmap, REG_POWER_AVG, &reg);
		if (ret < 0)
			return ret;
		val->intval = RAW_POWER_TO_UWATTS(sign_extend32(reg, 15), chip->rsense);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_MIX_SOC:
	{
		u16 reg;
		ret = max17332_get_mix_soc(chip, &reg);
		if (ret < 0) {
			return ret;
		}
		val->intval = reg;
		break;
	}
	case POWER_SUPPLY_PROP_INI_VERSION:
		if (chip->ini_rev == 0) {
			ret = max173322_get_ini_rev(chip);
			if (ret < 0) {
				dev_err(chip->dev, "%s : Failed to read ini rev\n", __func__);
				return ret;
			}
		}
		val->intval = chip->ini_rev;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17332_fg_set_property(struct power_supply *psy,
								enum power_supply_property psp,
								const union power_supply_propval *val)
{
	struct max17332_fg_chip *chip =
		power_supply_get_drvdata(psy);
	int ret = 0;
	unsigned int past = jiffies_to_msecs(jiffies - chip->usb_disc_time_stamp);

	if (chip->i2c_jitter && past < I2C_JITTER_DELAY_MS)
		msleep(I2C_JITTER_DELAY_MS - past);

	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = max17332_set_temp_lower_limit(chip, val->intval);
		if (ret < 0)
			dev_err(chip->dev, "temp alert min set fail:%d\n",
					ret);
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		ret = max17332_set_temp_upper_limit(chip, val->intval);
		if (ret < 0)
			dev_err(chip->dev, "temp alert max set fail:%d\n",
					ret);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
		ret = max17332_set_min_capacity_alert_th(chip, val->intval);
		if (ret < 0)
			dev_err(chip->dev, "capacity alert min set fail:%d\n",
					ret);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX:
		ret = max17332_set_max_capacity_alert_th(chip, val->intval);
		if (ret < 0)
			dev_err(chip->dev, "capacity alert max set fail:%d\n",
					ret);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int max17332_property_is_writeable(struct power_supply *psy,
										enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN:
	case POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static void max17332_set_alert_thresholds(struct max17332_fg_chip *chip)
{
	int ret;
	struct max17332_fg_platform_data *pdata = chip->pdata;
	u16 val;
	int temp;
	int voltage;

	/* Set VAlrtTh */
	ret = max17332_read(chip->regmap, REG_VCELL, &val);
	if (ret < 0) {
		dev_err(chip->dev, "could not read battery voltage");
	} else {
		voltage = max17332_raw_voltage_to_uvolts(val);
		voltage /= 1000;
		dev_info(chip->dev, "VOLTAGE AFTER VMAX: %d", voltage);
		update_voltage_thresholds(chip, voltage);
	}

	/* Set TAlrtTh */
	ret = max17332_get_temperature(chip, &temp);
	if (ret) {
		dev_err(chip->dev, "could not read battery temperature");
	} else {
		update_temperature_thresholds(chip, temp);
	}

	/* Set SAlrtTh */
	val = pdata->soc_min;
	val |= (pdata->soc_max << 8);
	ret = max17332_write(chip->regmap, REG_SALRTTH, val);
	if (ret < 0) {
		pr_err("%s: fail to write REG_SALRTTH\n", __func__);
		return;
	}

	/* Set IAlrtTh */
	val = (pdata->curr_min * (int)chip->rsense / 400) & 0xFF;
	val |= (((pdata->curr_max * (int)chip->rsense / 400) & 0xFF) << 8);
	ret = max17332_write(chip->regmap, REG_IALRTTH, val);
	if (ret < 0) {
		pr_err("%s: fail to write REG_IALRTTH\n", __func__);
		return;
	}

	if (pdata->occp_threshold > 0) {
	/* Set OCCP(OverCharge Current-Protection) Threshold. Bit 6-15 of the register */
		ret = max17332_read(chip->regmap_nvm, REG_N_IPRT_TH1, &val);
		if (ret < 0) {
			dev_err(chip->dev, "could not read REG_N_IPRT_TH1");
			return;
		}
		val &= 0x3F;
		val |= (pdata->occp_threshold / 5) << 6;
		ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm, REG_N_IPRT_TH1, val);
		if (ret < 0) {
			dev_err(chip->dev, "%s: fail to write REG_N_IPRT_TH1\n", __func__);
			return;
		}
	}
}

static void max17332_set_learncfg(struct max17332_fg_chip *chip)
{
	int ret;
	u16 val;

	ret = max17332_read(chip->regmap, REG_LEARNCFG, &val);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read LEARNCFG\n",
			__func__);
		return;
	}

	if ((val & MAX17332_LEARNCFG_LEARNSTAGE_MASK)
		== MAX17332_LEARNCFG_LEARNSTAGE_FULL)
		return;

	/* Set Bit 4~6 */
	val |= MAX17332_LEARNCFG_LEARNSTAGE_MASK;

	/* Ignore error */
	max17332_write_unlock(chip->max17332, chip->regmap, REG_LEARNCFG, val);
}

static void max17332_set_offset(struct max17332_fg_chip *chip)
{
	int ret;
	u16 val;
	u16 ncgain = MAX17332_NCGAIN_DEFAULT;

	ret = max17332_read(chip->regmap, REG_VERSION, &val);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read REG_VERSION\n",
			__func__);
		return;
	}

	/* We only update offset for P3ROM version */
	if (val != MAX17332_VERSION_P3ROM)
		return;

	ret = max17332_read(chip->regmap, REG_TRIM1, &val);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read REG_TRIM1\n",
			__func__);
		return;
	}

	if ((val & MAX17332_TRIM1_MASK) == 2)
		ncgain = MAX17332_NCGAIN_TRIM1_2;

	max17332_write_unlock(chip->max17332,
		chip->regmap_nvm, REG_NCGAIN, ncgain);

	dev_info(chip->dev, "trim1: %d, ncgain: 0x%x\n",
		val & MAX17332_TRIM1_MASK, ncgain);
}

static int max17332_fg_initialize(struct max17332_fg_chip *chip)
{
	int ret;
	u16 val;
	u16 fgrev;

	ret = max17332_read(chip->regmap, REG_VERSION, &fgrev);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to read REG_VERSION\n", __func__);
		return ret;
	}

	dev_info(chip->dev, "IC Version: 0x%04x\n", fgrev);

	/* Check NVError bit, log if found, then clear */
	ret = max17332_read(chip->regmap, REG_COMMSTAT, &val);
	if (ret < 0) {
		dev_err(chip->dev, "%s: Failed to read REG_COMMSTAT\n", __func__);
		return ret;
	}

	if ((val & MAX17332_COMMSTAT_NVERROR) == MAX17332_COMMSTAT_NVERROR) {
		dev_info(chip->dev, "%s: NVError bit set. CommStat: %04x Clearing...\n", __func__, val);
		ret = max17332_write_unlock(chip->max17332, chip->regmap,
				REG_COMMSTAT, (val & ~(MAX17332_COMMSTAT_NVERROR)));
		if (ret < 0) {
			dev_err(chip->dev, "%s: Failed to write REG_COMMSTAT\n", __func__);
			return ret;
		}
		ret = max17332_read(chip->regmap, REG_COMMSTAT, &val);
		if (ret < 0) {
			dev_err(chip->dev, "%s: Failed to read REG_COMMSTAT\n", __func__);
			return ret;
		}
		if ((val & MAX17332_COMMSTAT_NVERROR) == MAX17332_COMMSTAT_NVERROR) {
			dev_err(chip->dev, "%s: Failed to clear CommStat.NVError\n", __func__);
			return 0; /* Allowing boot, but will mask future NVM operation errors. Can be monitored in telemetry. */
		}
		dev_info(chip->dev, "%s: NVError bit cleared. CommStat: %04x\n", __func__, val);
	}

	/* Optional step - alert threshold initialization */
	max17332_set_alert_thresholds(chip);

	/* Set learcfg to advacne to the final stage */
	max17332_set_learncfg(chip);

	/* Update offset */
	max17332_set_offset(chip);

	return 0;
}

#ifdef CONFIG_OF
static int max17332_fg_parse_dt(struct max17332_fg_chip *battery)
{
	struct device_node *max17332_node, *np;
	struct max17332_fg_platform_data *pdata;
	int ret = 0;

	pr_debug("%s start\n", __func__);
	pdata = battery->pdata;
	if (unlikely(pdata == NULL))
		return -ENOMEM;

	pr_debug("%s irq info\n", __func__);
	/* reset, irq gpio info */

	max17332_node = of_find_node_by_name(NULL, "max17332");
	if (max17332_node == NULL) {
		pr_err("%s max17332_node NULL\n", __func__);
		return -EINVAL;
	}

	np = of_find_node_by_name(max17332_node, "battery");
	if (np == NULL) {
		pr_err("%s battery node NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s ialrt-min\n", __func__);
	ret = of_property_read_s32(np, "ialrt-min", &pdata->curr_min);
	if (ret < 0)
		pdata->curr_min = -5120; /* mA */ /* Disable alert */

	pr_debug("%s salrt-min\n", __func__);
	ret = of_property_read_u32(np, "salrt-min", &pdata->soc_min);
	if (ret < 0)
		pdata->soc_min = 0; /* Percent */ /* Disable alert */

	pr_debug("%s salrt-max\n", __func__);
	ret = of_property_read_u32(np, "salrt-max", &pdata->soc_max);
	if (ret < 0)
		pdata->soc_max = 255; /* Percent */ /* Disable alert */

	pr_debug("%s cap-critical-level\n", __func__);
	ret = of_property_read_u8(np, "cap-critical-level", &pdata->cap_critical_lvl);
	if (ret < 0)
		pdata->cap_critical_lvl = DEFAULT_CAPACITY_CRITICAL_LEVEL;

	pr_debug("%s, cap-low-level\n", __func__);
	ret = of_property_read_u8(np, "cap-low-level", &pdata->cap_low_lvl);
	if (ret < 0)
		pdata->cap_low_lvl = DEFAULT_CAPACITY_LOW_LEVEL;

	pr_debug("%s, cap-high-level\n", __func__);
	ret = of_property_read_u8(np, "cap-high-level", &pdata->cap_high_lvl);
	if (ret < 0)
		pdata->cap_high_lvl = DEFAULT_CAPACITY_HIGH_LEVEL;

	ret = of_property_read_s32(np,
		"max17332,int-interval-ms", &pdata->int_interval_ms);
	if (ret < 0)
		pdata->int_interval_ms = MAX17332_DEFAULT_INT_INTERVAL_MS;

	pr_debug("%s, occp-threshold\n", __func__);
	ret = of_property_read_s32(np, "occp-threshold", &pdata->occp_threshold);
	if (ret < 0)
		pdata->occp_threshold = DEFAULT_OCCP_THRESHOLD;

	battery->fet_thermal_mitigation_enabled =
			of_property_read_bool(np, "battery,enable-fet-thermal-mitigation");
	pr_debug("FET thermal mitigation enabled: %d\n", battery->fet_thermal_mitigation_enabled);

	battery->custom_voltage_adjustment_enabled =
			of_property_read_bool(np, "battery,custom-voltage-adjustment");
	pr_debug("Custom voltage adjustment enabled: %d\n", battery->custom_voltage_adjustment_enabled);

	/* Pack specific settings are read here - get pack node first */
	/* Setting some default values if something is not read correctly */
	pdata->fg_config_version = 0;
	pdata->curr_max = 5080; /* mA */ /* Disable alert */

	switch (battery->pdata->pack_id) {
	case MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0:
		np = of_find_node_by_name(np, "hammerheadpack-cellv-0-packv-0");
		break;
	case MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0:
		np = of_find_node_by_name(np, "sc50-cellv-0-packv-0");
		break;
	case MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0:
		np = of_find_node_by_name(np, "sc50-cellv-2-packv-0");
		break;
	default:
		np = NULL;
		break;
	}
	if (np == NULL) {
		pr_err("%s pack node not found\n", __func__);
		/* If we cannot find the pack node, we can continue with the default values */
		return 0;
	}

	pr_debug("%s, fg-config-version\n", __func__);
	of_property_read_u8(np, "fg-config-version", &pdata->fg_config_version);

	pr_debug("%s ialrt-max\n", __func__);
	of_property_read_s32(np, "ialrt-max", &pdata->curr_max);

	pr_debug("%s, manual-charge-current\n", __func__);
	ret = of_property_read_u16(np, "manual-charge-current", &pdata->manual_charge_current_reg_val);
	if (ret < 0)
		pdata->manual_charge_current_reg_val = NVCHGCURR_MANUAL_CHARGE_DEFAULT;

	pr_debug("%s finished\n", __func__);
	return 0;

}
#endif

static int max17332_handle_prot_alert(struct max17332_fg_chip *chip)
{
	int ret, ocv_threshold_uv;
	u16 val;

	dev_dbg(chip->dev, "Protection Interrupt Handler!\n");

	ret = max17332_read(chip->regmap, REG_PROTALRTS, &val);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to read REG_PROTALRTS\n", __func__);
		return IRQ_HANDLED;
	}

	/*
	 * storing status register value in a variable to log it in sysfs
	 * this is done since the value will be cleared later
	 */

	chip->cached_prot_status_register_val = val;

	/*
	 * Clear alerts first just in case that when an alert is being handled, the next one
	 * comes and no alert will be triggered.
	 * To avoid race conditions, we will only clear the alerts in the current reading.
	 * FG will ignore 1s, so as long as the bits to be cleared have 0s, it will be ok.
	 */
	ret = max17332_write_unlock(chip->max17332, chip->regmap,
			REG_PROTALRTS, (u16)(~(val & REG_PROTALRTS_MASK)));
	if (ret < 0)
		dev_err(chip->dev, "%s: fail to write REG_PROTALRTS\n", __func__);
	if (val & BIT_PROTALRT_CHGWDT_INT)
		dev_info(chip->dev, "Protection Alert: Charge Watch Dog Timer!\n");
	if (val & BIT_PROTALRT_TOOHOTC_INT)
		dev_info(chip->dev, "Protection Alert: Overtemperature for Charging!\n");
	if (val & BIT_PROTALRT_FULL_INT)
		dev_info(chip->dev, "Protection Alert: Full Detection!\n");
	if (val & BIT_PROTALRT_TOOCOLDC_INT)
		dev_info(chip->dev, "Protection Alert: Undertemperature!\n");
	if ((val & BIT_PROTALRT_OVP_INT) || (val & BIT_PROTALRT_OCCP_INT)) {
		if (val & BIT_PROTALRT_OVP_INT) {
			dev_info(chip->dev, "Protection Alert: Overvoltage!\n");
			chip->ovp_change_counter++;
		}
		if (val & BIT_PROTALRT_OCCP_INT) {
			dev_info(chip->dev, "Protection Alert: Overcharge Current!\n");
			chip->occp_change_counter++;
		}

		ocv_threshold_uv = MAX17332_DEFAULT_BATT_OVRCHG_THRESHOLD_UV;
		if(chip->pdata)
		{
			switch (chip->pdata->pack_id) {
				case MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0:
					ocv_threshold_uv = MAX17332_HAMMERHEAD_BATT_OVRCHG_THRESHOLD_UV;
					break;
				case MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0:
				case MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0:
					ocv_threshold_uv = MAX17332_SC50_BATT_OVRCHG_THRESHOLD_UV;
					break;
				default:
					break;
			}
		}

		ret = max17332_overcharge_protection(chip->max17332, ocv_threshold_uv);
		if (ret < 0)
		{
			dev_err(chip->dev,
					"%s: failed to pause charger bob to resolve OVP or OCCP issue!\n",
					__func__);
		}
	}
	if (val & BIT_PROTALRT_QOVFLW_INT)
		dev_info(chip->dev, "Protection Alert: Q Overflow!\n");
	if (val & BIT_PROTALRT_DIEHOT_INT)
		dev_info(chip->dev, "Protection Alert: Overtemperature for die temperature!\n");
	if (val & BIT_PROTALRT_TOOHOTD_INT)
		dev_info(chip->dev, "Protection Alert: Overtemperature for Discharging!\n");
	if (val & BIT_PROTALRT_TEMPCHANGE_INT)
		dev_info(chip->dev, "Protection Alert: Temperature Change!\n");
	if (val & BIT_PROTALRT_UVP_INT)
		dev_info(chip->dev, "Protection Alert: Undervoltage Protection!\n");
	if (val & BIT_PROTALRT_ODCP_INT)
		dev_info(chip->dev, "Protection Alert: Overdischarge current!\n");

	return ret;
}

static void alert_enable_work(struct work_struct *work)
{
	struct max17332_fg_chip *chip =
		container_of(work, struct max17332_fg_chip, alert_enable_work.work);

	enable_irq(chip->max17332->irq);
}

static int update_temperature_thresholds(struct max17332_fg_chip *chip, int temp)
{
	static const int temp_thresholds[] = {
		MAX17332_TEMP_THRESHOLD_0,
		MAX17332_TEMP_THRESHOLD_5,
		MAX17332_TEMP_THRESHOLD_10,
		MAX17332_TEMP_THRESHOLD_15,
		MAX17332_TEMP_THRESHOLD_20,
		MAX17332_TEMP_THRESHOLD_30,
		MAX17332_TEMP_THRESHOLD_40,
		MAX17332_TEMP_THRESHOLD_42,
		MAX17332_TEMP_THRESHOLD_45,
		MAX17332_TEMP_THRESHOLD_50,
		MAX17332_TEMP_THRESHOLD_55,
		MAX17332_TEMP_THRESHOLD_60,
		MAX17332_TEMP_THRESHOLD_MAX,
	};
	static const int temp_threshold_count = sizeof(temp_thresholds) / sizeof(temp_thresholds[0]);
	int ret = -EINVAL;
	int tmin, tmax;
	int i;

	if (temp < MAX17332_TEMP_THRESHOLD_0) {
		/* Handle the special case that the min temp threshold is floored */
		tmin = MAX17332_TEMP_THRESHOLD_MIN;
		tmax = MAX17332_TEMP_THRESHOLD_0;
		ret = 0;
	} else {
		static const int tmin_hysterisis_dc = 10;
		dev_info(chip->dev, "%s: Entered TALRT, temp is: %d\n", __func__, temp);
		if (temp > MAX17332_TEMP_THRESHOLD_MAX) {
			temp = MAX17332_TEMP_THRESHOLD_MAX;
		}

		/* Locate the temperature thresholds tmin and tmax */
		for (i = 0; i < temp_threshold_count - 1; ++i) {
			if (temp >= temp_thresholds[i] && temp < temp_thresholds[i + 1]) {
				ret = 0;
				tmin = temp_thresholds[i];
				tmax = temp_thresholds[i + 1];
				break;
			}
		}
		if (ret) {
			dev_err(chip->dev, "%s: couldn't locate temperature threshold: %d\n", __func__, temp);
			return ret;
		}
		/* subtract 1C of lower temperature offset to account for temperature threshold crossing */
		tmin = tmin - tmin_hysterisis_dc;
	}

	ret = max17332_set_temp_lower_limit(chip, tmin);
	if (ret) {
		dev_err(chip->dev, "could not set TMIN alert to %d\n", tmin);
		return ret;
	}
	ret = max17332_set_temp_upper_limit(chip, tmax);
	if (ret) {
		dev_err(chip->dev, "could not set TMAX alert to %d\n", tmax);
		return ret;
	}
	dev_info(chip->dev, "%s: setting temperature alert, tmin: %d tmax: %d\n", __func__, tmin, tmax);
	return 0;
}

static int update_voltage_thresholds(struct max17332_fg_chip *chip, int voltage)
{
	static const int voltage_thresholds[] = {
		MAX17332_VOLTAGE_THRESHOLD_MIN,
		MAX17332_VOLTAGE_THRESHOLD_3_00,
		MAX17332_VOLTAGE_THRESHOLD_3_40,
		MAX17332_VOLTAGE_THRESHOLD_3_60,
		MAX17332_VOLTAGE_THRESHOLD_3_80,
		MAX17332_VOLTAGE_THRESHOLD_4_00,
		MAX17332_VOLTAGE_THRESHOLD_4_20,
		MAX17332_VOLTAGE_THRESHOLD_4_30,
		MAX17332_VOLTAGE_THRESHOLD_4_34,
		MAX17332_VOLTAGE_THRESHOLD_4_40,
		MAX17332_VOLTAGE_THRESHOLD_MAX
	};
	static const int voltage_threshold_count = sizeof(voltage_thresholds) / sizeof(voltage_thresholds[0]);
	int ret = -EINVAL;
	int vmin, vmax;
	int i;

	dev_info(chip->dev, "%s: Entered VALRT, voltage is: %d\n", __func__, voltage);
	if (voltage < MAX17332_VOLTAGE_THRESHOLD_MIN) {
		voltage = MAX17332_VOLTAGE_THRESHOLD_MIN;
	} else if (voltage > MAX17332_VOLTAGE_THRESHOLD_MAX) {
		voltage = MAX17332_VOLTAGE_THRESHOLD_MAX;
	}

	/* Locate the voltage thresholds vmin and vmax */
	for (i = 0; i < voltage_threshold_count - 1; ++i) {
		if (voltage >= voltage_thresholds[i] && voltage < voltage_thresholds[i + 1]) {
			ret = 0;
			vmin = voltage_thresholds[i];
			vmax = voltage_thresholds[i + 1];
			break;
		}
	}

	/* we are disabling the voltage thresholds to unblock day0 builds
	 * as we saw a power regression
	 */
	vmin = MAX17332_VOLTAGE_THRESHOLD_MIN;
	vmax = MAX17332_VOLTAGE_THRESHOLD_MAX;

	ret = max17332_set_voltage_lower_limit(chip, vmin);
	if (ret) {
		dev_err(chip->dev, "could not set VMIN alert: %d\n", ret);
		return ret;
	}
	ret = max17332_set_voltage_upper_limit(chip, vmax);
	if (ret) {
		dev_err(chip->dev, "could not set VMAX alert: %d\n", ret);
		return ret;
	}
	return 0;
}

static void update_active_discharge_logic(struct max17332_fg_chip *chip, int temp)
{
	int capacity = 0;
	int ret = 0;
	ret = max17332_get_batt_capacity(chip, &capacity, NULL);
	if (ret) {
		dev_err(chip->dev, "could not read battery capacity");
		return;
	}

	if (capacity >= MAX17332_ACTIVE_DISCHARGE_SOC && temp >= MAX17332_ACTIVE_DISCHARGE_START_TEMP) {
		/* no need for update */
		if (chip->in_active_discharge)
			return;

		dev_err(chip->dev, "disabling charging due to high charge at high temp\n");
		/* enter active discharge mode - ignore errors since we could fail to
		 * disable the BOB which is expected when not charging */
		max17332_set_active_discharge(chip->max17332, true);
		chip->in_active_discharge = true;
	}

	if ((capacity < MAX17332_ACTIVE_DISCHARGE_SOC || temp <= MAX17332_ACTIVE_DISCHARGE_END_TEMP)) {
		/* no need for update */
		if (!chip->in_active_discharge)
			return;

		dev_err(chip->dev, "active discharge done, reenabling charging\n");
		/* exit active discharge mode - ignore errors since we could fail to
		 * enable the BOB which is expected when not charging */
		max17332_set_active_discharge(chip->max17332, false);
		chip->in_active_discharge = false;
	}
}

static void process_ca_alert(struct max17332_fg_chip *chip, u16 chg_val)
{
	int ret;

	if (chg_val & BIT_STATUS_CP) {
		dev_info(chip->dev, "Charging Alert: Heat limit!\n");
		ret = max17332_headroom_management(chip->max17332);
		if (ret < 0)
			dev_err(chip->dev,
					"%s, failed to reduce voltage for heat limit, and skip once\n",
					__func__);
		chip->cp_change_counter++;
	}
	if (chg_val & BIT_STATUS_CT) {
		dev_info(chip->dev, "Charging Alert: FET Temperature limit!\n");
		if (chip->fet_thermal_mitigation_enabled) {
		ret = max17332_headroom_management(chip->max17332);
		if (ret < 0)
			dev_err(chip->dev,
					"%s, failed to reduce voltage for temperature limit, and skip once\n",
					__func__);
		}
	}
	if (chg_val & BIT_STATUS_DROPOUT) {
		dev_info(chip->dev, "Charging Alert: Dropout!\n");
		ret = max17332_headroom_management(chip->max17332);
		if (ret < 0)
			dev_err(chip->dev,
					"%s, failed to increase voltage for drop out, and skip once\n",
					__func__);
		chip->dropout_change_counter++;
	}
}

static void alert_work(struct work_struct *work)
{
	struct max17332_fg_chip *chip =
		container_of(work, struct max17332_fg_chip, alert_work.work);
	int ret;
	u16 stat_val, chg_val;
	int temp = 0;
	union power_supply_propval prop;
	bool usb_is_online = false;

	ret = power_supply_get_property(chip->usb_charger, POWER_SUPPLY_PROP_ONLINE, &prop);
	if (!ret && prop.intval)
		usb_is_online = true;

	dev_dbg(chip->dev, "STATUS : Interrupt Handler!\n");

	/* Check alert type */
	ret = max17332_read(chip->regmap, REG_STATUS, &stat_val);
	if (ret < 0) {
		dev_err(chip->dev, "%s: fail to read REG_STATUS\n", __func__);
		goto exit;
	}

	/*
	 * storing status register value in a variable to log it in sysfs
	 * this is done since the value will be cleared later
	 */
	chip->cached_status_register_val = stat_val;

	if (stat_val == 0)
		goto exit;

	/*
	 * Clear alerts first. If anything goes wrong, the alerts could be triggered
	 * again since the alerts have been cleared.
	 * To avoid race conditions (i.e., when an alert is being handled in the
	 * interrupt handler, another alert is updated. We do not overwrite
	 * each other, so just clear the alerts that have been serviced.
	 * FG firmware will ignore the 1s.
	 */
	ret = max17332_write_unlock(chip->max17332, chip->regmap,
			REG_STATUS, (u16)(~(stat_val & REG_STATUS_MASK)));
	if (ret < 0)
		dev_err(chip->dev, "%s, failed to clear REG_STATUS: %d\n", __func__, ret);

	if (stat_val & BIT_STATUS_PA) {
		dev_dbg(chip->dev, "Alert: Prot Alert!\n");
		ret = max17332_handle_prot_alert(chip);
		if (ret < 0)
			dev_err(chip->dev,
					"max17332_handle_prot_alert failed to handle protection alert: %d\n",
					ret);
	}
	if (stat_val & BIT_STATUS_SMX)
		dev_dbg(chip->dev, "Alert: SOC MAX!\n");
	if (stat_val & BIT_STATUS_SMN)
		dev_dbg(chip->dev, "Alert: SOC MIN!\n");
	if (stat_val & (BIT_STATUS_TMN | BIT_STATUS_TMX)) {
		dev_dbg(chip->dev, "Alert: TEMP!\n");
		ret = max17332_get_temperature(chip, &temp);
		if (ret) {
			dev_err(chip->dev, "could not read battery temperature");
		} else {
			update_temperature_thresholds(chip, temp);
			update_active_discharge_logic(chip, temp);
		}
	}
	if (stat_val & BIT_STATUS_IMX)
		dev_dbg(chip->dev, "Alert: CURR MAX!\n");
	if (stat_val & BIT_STATUS_IMN)
		dev_dbg(chip->dev, "Alert: CURR MIN!\n");
	if (stat_val & BIT_STATUS_DSOCI) {
		dev_dbg(chip->dev, "Alert: SOC CHANGE!\n");
		if (chip->custom_voltage_adjustment_enabled) {
			// When there is a change of 1% in battery RSoC
			// check if current_avg < 70% of current_target:
			// if it is, increase Vreg_out
			if (usb_is_online) {
				ret = max17332_check_current_avg(chip->max17332);
				if (ret)
					max17332_headroom_management(chip->max17332);
			}
		}
		ret = max17332_get_temperature(chip, &temp);
		if (ret) {
			dev_err(chip->dev, "could not read battery temperature");
		} else {
			update_active_discharge_logic(chip, temp);
		}
	}
	if (stat_val & BIT_STATUS_CA) {
		dev_dbg(chip->dev, "Alert: CHARGING!\n");

		/* Check Charging type */
		ret = max17332_read(chip->regmap, REG_CHGSTAT, &chg_val);

		if (chip->custom_voltage_adjustment_enabled) {
			/**
			* Sometimes the Dropout Alert occurs after removing the USB.
			* We should only handle the alerts when USB is connected.
			* So add a USB status check before processing.
			*/
			if (usb_is_online) {
				// Set alert type and processs the alert
				max17332_set_alert_type(chip->max17332, chg_val);
				process_ca_alert(chip, chg_val);
			}
		} else {
			max17332_set_alert_type(chip->max17332, chg_val);
			process_ca_alert(chip, chg_val);
		}
	}

	power_supply_changed(chip->battery);

exit:
	if (chip->pdata->is_irq_level_trigger)
		schedule_delayed_work(&chip->alert_enable_work,
			msecs_to_jiffies(chip->pdata->int_interval_ms));
}

static irqreturn_t max17332_fg_irq_isr(int irq, void *data)
{
	struct max17332_fg_chip *chip = data;
	int delay_ms = 0;
	unsigned int past = jiffies_to_msecs(jiffies - chip->usb_disc_time_stamp);
	if (chip->i2c_jitter && past < I2C_JITTER_DELAY_MS) {
		delay_ms = I2C_JITTER_DELAY_MS - past;
		// ensure system stays awake during processing alert_work
		pm_wakeup_event(chip->dev, delay_ms);
	}
	schedule_delayed_work(&chip->alert_work, msecs_to_jiffies(delay_ms));

	if (chip->pdata->is_irq_level_trigger)
		disable_irq_nosync(chip->max17332->irq);

	return IRQ_HANDLED;
}

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
static ssize_t
emul_batt_capacity_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17332_fg_chip *chip;
	int ret;
	int emul_batt_capacity = -1;
	char *env[2];

	chip = dev_get_drvdata(dev);
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

	chip->emul_batt_capacity = emul_batt_capacity;


	// Sending uevent to health HAL to update battery
	env[0] = "SUBSYSTEM=power_supply";
	env[1] = NULL;

	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, env);

	return count;
}
static DEVICE_ATTR_WO(emul_batt_capacity);

static ssize_t
emul_batt_temperature_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17332_fg_chip *chip;
	int ret;
	int emul_batt_temperature = -1;

	chip = dev_get_drvdata(dev);
	ret = kstrtos32(buf, 10, &emul_batt_temperature);
	if (ret < 0) {
		dev_err(dev, "Failed to read emul_batt_temperature from buffer: %d\n", ret);

		// Intentionally return 'count' so this update won't keep being retried
		return count;
	}

	chip->emul_batt_temperature = emul_batt_temperature;

	return count;
}
static DEVICE_ATTR_WO(emul_batt_temperature);

static ssize_t
emul_mix_soc_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17332_fg_chip *chip;
	int ret;
	int emul_mix_soc = -1;

	chip = dev_get_drvdata(dev);
	ret = kstrtos32(buf, 10, &emul_mix_soc);
	if (ret < 0) {
		dev_err(dev, "Failed to read emul_mix_soc from buffer: %d\n", ret);

		// Intentionally return 'count' so this update won't keep being retried
		return count;
	}

	if (emul_mix_soc > 100) {
		dev_err(dev, "The proposed mix_soc level was greater than 100\n");
		return count;
	}

	chip->emul_mix_soc = emul_mix_soc;

	return count;
}
static DEVICE_ATTR_WO(emul_mix_soc);

static ssize_t
emul_voltage_ocv_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct max17332_fg_chip *chip;
	int ret;
	int emul_voltage_ocv = -1;

	chip = dev_get_drvdata(dev);
	ret = kstrtos32(buf, 10, &emul_voltage_ocv);
	if (ret < 0) {
		dev_err(dev, "Failed to read emul_voltage_ocv from buffer: %d\n", ret);

		// Intentionally return 'count' so this update won't keep being retried
		return count;
	}

	chip->emul_voltage_ocv = emul_voltage_ocv;

	return count;
}
static DEVICE_ATTR_WO(emul_voltage_ocv);
#endif

static int max17332_get_temp(void *data, int *state)
{
	struct max17332_fg_chip *chip =
		(struct max17332_fg_chip *) data;

	int ret = max17332_get_temperature(chip, state);
	// convert from millicelsius to decicelsius
	*state *= 100;

	return ret;
}

static struct attribute *attributes[] = {
#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	&dev_attr_emul_batt_capacity.attr,
	&dev_attr_emul_batt_temperature.attr,
	&dev_attr_emul_mix_soc.attr,
	&dev_attr_emul_voltage_ocv.attr,
#endif
	NULL,
};

static const struct attribute_group attr_group = {
	.name = "max17332-ctrl",
	.attrs = attributes
};

static struct thermal_zone_of_device_ops thermal_ops = {
	.get_temp = max17332_get_temp,
};

static int init_thermal_zone(struct platform_device *pdev, struct max17332_fg_chip *chip)
{
	chip->tzd = devm_thermal_zone_of_sensor_register(pdev->dev.parent,
						0,
						chip,
						&thermal_ops);
	if (IS_ERR(chip->tzd)) {
		return PTR_ERR(chip->tzd);
	}

	return 0;
}



static bool max17332_is_current_fg_config_different_than_hardcoded_fg_config(
	struct max17332_fg_chip *chip)
{
	int ret = 0, i;
	uint16_t data = 0;
	struct max17332_fg_config_update_data *fg_data = &fg_config_for_validate;

	if (!fg_data) {
		pr_err("%s: fg_data is NULL\n", __func__);
		return false; // Don't trigger FG update if config not found
	}

	ret = max17332_fg_config_update_allocate_memory_and_read_fg_config(chip, fg_data);
	if (ret < 0) {
		pr_info("%s: aborted validating config, read_and_allocate failed\n", __func__);
		return false; // Don't trigger FG update if config not found
	}

	for (i = 0; i < fg_data->config_size_u16; i += 2) {
		if (include_reg(fg_data->ignore_data, fg_data->ignore_size_u16, fg_data->config_data[i])) {
			ret |= max17332_read(chip->regmap_nvm, (uint8_t)(fg_data->config_data[i] & 0xFF), &data);
			if (ret || data != fg_data->config_data[i + 1]) {
				pr_info("%s: FG config does not match, addr: 0x%x, data: 0x%x\n", __func__, fg_data->config_data[i], data);
				max17332_fg_config_update_free_memory(fg_data);
				return true; // Config is different
			}
		}
	}

	max17332_fg_config_update_free_memory(fg_data);
	return false; // Config is the same
}

static int max17332_fg_config_update_get_fullcap_thresholds(
	struct max17332_fg_chip *chip, u32 *fullcapnom_threshold,
	u32 *fullcaprep_threshold)
{
	int ret;
	struct device_node *max17332_node = NULL;
	const struct property *prop = NULL;

	*fullcapnom_threshold = FULLCAPNOM_DEFAULT_THRESHOLD;
	*fullcaprep_threshold = FULLCAPREP_DEFAULT_THRESHOLD;

	if(!chip->pdata) {
		pr_err("%s platform data pointer is null\n", __func__);
		return -EINVAL;
	}

	max17332_node = of_find_node_by_name(NULL, "max17332");
	if (max17332_node == NULL) {
		pr_err("%s max17332_node not found\n", __func__);
		return -EINVAL;
	}

	max17332_node = of_find_node_by_name(max17332_node, "battery");
	if (max17332_node == NULL) {
		pr_err("%s battery node not found\n", __func__);
		return -EINVAL;
	}

	switch (chip->pdata->pack_id) {
		case MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0:
			max17332_node = of_find_node_by_name(max17332_node, "hammerheadpack-cellv-0-packv-0");
			break;
		case MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0:
			max17332_node = of_find_node_by_name(max17332_node, "sc50-cellv-0-packv-0");
			break;
		case MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0:
			max17332_node = of_find_node_by_name(max17332_node, "sc50-cellv-2-packv-0");
			break;
		default:
			ret = -EINVAL;
			break;
	}
	if (max17332_node == NULL) {
		pr_err("%s pack node not found\n", __func__);
		return -EINVAL;
	}

	prop = of_find_property(max17332_node,
				"fullcapnom-max-threshold", NULL);
	if (prop == NULL || prop->length <= 0) {
		pr_info("%s: property fullcapnom-threshold not found\n",
			__func__);
		return -EINVAL;
	}

	prop = of_find_property(max17332_node,
				"fullcaprep-max-threshold", NULL);
	if (prop == NULL || prop->length <= 0) {
		pr_info("%s: property fullcaprep-threshold not found\n",
			__func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(max17332_node,
				"fullcapnom-max-threshold",
				fullcapnom_threshold);
	if (ret < 0) {
		pr_err("%s: property fullcapnom_threshold value not found\n",
			__func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(max17332_node,
				"fullcaprep-max-threshold",
				fullcaprep_threshold);
	if (ret < 0) {
		pr_err("%s: property fullcaprep_threshold value not found\n",
			__func__);
		return -EINVAL;
	}

	pr_info("%s: fullcapnom_threshold: %u, fullcaprep_threshold= %u ", __func__, *fullcapnom_threshold, *fullcaprep_threshold);
	return 0;
}

static int check_fg_config_version(struct max17332_fg_chip *chip) {
	if (chip->pdata->fg_config_version != 0 && chip->ini_rev < chip->pdata->fg_config_version) {
		dev_info(chip->dev, "%s: FG ini version < hardcoded version, triggering fg_update\n", __func__);
		return MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_INI_REV_MISMATCH;
	}
	return 0;
}

static int check_capacity_thresholds(struct max17332_fg_chip *chip) {
	u32 fullcapnom_threshold, fullcaprep_threshold;
	max17332_fg_config_update_get_fullcap_thresholds(chip, &fullcapnom_threshold, &fullcaprep_threshold);
	if ((0 > max17332_get_fullcapnom(chip)) ||
		(fullcapnom_threshold < (u32)max17332_get_fullcapnom(chip)) ||
		(0 > max17332_get_fullcaprep(chip)) ||
		(fullcaprep_threshold < (u32)max17332_get_fullcaprep(chip)) ||
		(0 > max17332_get_nfullcapnom(chip)) ||
		(fullcapnom_threshold < (u32)max17332_get_nfullcapnom(chip)) ||
		(0 > max17332_get_nfullcaprep(chip)) ||
		(fullcaprep_threshold < (u32)max17332_get_nfullcaprep(chip))) {
		dev_info(chip->dev, "%s: Capacity registers out of bounds, triggering fg_update\n", __func__);
		return MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_FULLCAP_MISMATCH;
	}
	return 0;
}

static int check_config_mismatch(struct max17332_fg_chip *chip) {
	if (max17332_is_current_fg_config_different_than_hardcoded_fg_config(chip)) {
		dev_info(chip->dev, "%s: Config registers do not match hard coded ini, triggering fg_update\n", __func__);
		return MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_CONFIG_MISMATCH;
	}
	return 0;
}

static int max17332_resolve_pack(struct max17332_fg_chip *chip)
{
	int ret;
	uint16_t nDesignCap, nDesignVolt;
	uint8_t qscale;
	uint32_t design_capacity;
	uint8_t pack_vendor, cell_vendor;

	// Read the nDesignCap register
	ret = max17332_read(chip->regmap_nvm, REG_N_DESIGN_CAP, &nDesignCap);
	if (ret) {
		pr_err("%s: Failed to read nDesignCap register\n", __func__);
		chip->pdata->pack_id = MAX17332_FG_PACK_ID_UNKNOWN;
		return ret;
	}

	// Read the nDesignVoltage register
	ret = max17332_read(chip->regmap_nvm, REG_N_DESGIN_VOLT, &nDesignVolt);
	if (ret) {
		pr_err("%s: Failed to read nDesignVolt register\n", __func__);
		chip->pdata->pack_id = MAX17332_FG_PACK_ID_UNKNOWN;
		return ret;
	}

	// Extract pack and cell vendor information
	pack_vendor = (nDesignVolt >> 8) & 0x0F;
	cell_vendor = (nDesignVolt >> 12) & 0x0F;
	// Extract D6-D15 and convert to decimal
	design_capacity = (nDesignCap >> 6) & 0x03FF;

	// Extract Qscale (D0-D2)
	qscale = nDesignCap & 0x0007;

	// Calculate the design capacity based on Qscale
	design_capacity = (design_capacity * qscale_capacity_step_size_uah[qscale] * 10) / chip->rsense;
	pr_info("%s: MAX17332 design capacity = %u\n", __func__, design_capacity);

	// Determine platform based on design capacity
	if (design_capacity == PACK_DESIGN_CAPACITY_SC50_UAH && pack_vendor == BATTERY_PACK_VENDOR_0 && cell_vendor == BATTERY_CELL_VENDOR_0) {
		chip->pdata->pack_id = MAX17332_FG_PACK_ID_SC50_CELLV_0_PACKV_0;
	} else if (design_capacity == PACK_DESIGN_CAPACITY_HAMMERHEADPACK_UAH && pack_vendor == BATTERY_PACK_VENDOR_0 && cell_vendor == BATTERY_CELL_VENDOR_0) {
		chip->pdata->pack_id = MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0;
	} else if (design_capacity == PACK_DESIGN_CAPACITY_SC50_UAH && pack_vendor == BATTERY_PACK_VENDOR_0 && cell_vendor == BATTERY_CELL_VENDOR_2) {
		chip->pdata->pack_id = MAX17332_FG_PACK_ID_SC50_CELLV_2_PACKV_0;
	} else {
		chip->pdata->pack_id = MAX17332_FG_PACK_ID_UNKNOWN;
	}

	return 0;
}

static int max173322_get_ini_rev(struct max17332_fg_chip *chip) {
	u16 reg_value;
	u8 ini_revision;
	int ret;
	chip->ini_rev = 0;

	if(!chip->pdata)
	{
		dev_err(chip->dev, "%s: chip->pdata should be initialized\n", __func__);
		return -EINVAL;
	}

	// Read the register value from the NVM region
	ret = max17332_read(chip->regmap_nvm, REG_RSENSE_NVM, &reg_value);
	if (ret) {
		dev_err(chip->dev, "%s: Failed to read register 0x19C\n", __func__);
		return ret;
	}

	// Extract the upper 8 bits
	ini_revision = (reg_value >> 8) & 0xFF;

	// Check if the platform is Hammerhead
	if (chip->pdata->pack_id == MAX17332_FG_PACK_ID_HAMMERHEAD_CELLV_0_PACKV_0) {
		// Convert ASCII to hex if necessary
		if (ini_revision >= '0' && ini_revision <= '9') {
			ini_revision -= '0';
		} else if (ini_revision >= 'A' && ini_revision <= 'F') {
			ini_revision = ini_revision - 'A' + 10;
		} else {
			dev_err(chip->dev, "%s: Invalid ASCII character for ini revision\n", __func__);
			return -EINVAL;
		}
	}

	// Preserve the ini revision in chip->ini_rev
	chip->ini_rev = ini_revision;

	return 0;
}

static int max17332_fg_probe(struct platform_device *pdev)
{
	struct max17332_dev *max17332 = dev_get_drvdata(pdev->dev.parent);
	struct max17332_fg_platform_data *pdata =
		dev_get_platdata(max17332->dev);
	struct max17332_fg_chip *chip;
	int ret = 0;
	struct power_supply_config fg_cfg = {};
	union power_supply_propval prop;
	u16 data;
	int reason = 0;

	pr_info("%s: MAX17332 Fuelgauge Driver Loading\n", __func__);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(!pdata)) {
		pr_err("%s: out of memory\n", __func__);
		pdata = ERR_PTR(-ENOMEM);
		return -ENOMEM;
	}

	chip->usb_charger = power_supply_get_by_name("usb-charger");
	if (IS_ERR_OR_NULL(chip->usb_charger)) {
		dev_err(&pdev->dev,
			"%s : Failed to find usb power supply device\n", __func__);
		ret = -EPROBE_DEFER;
		goto error;
	}

	mutex_init(&chip->lock);

	chip->dev = &pdev->dev;
	chip->max17332 = max17332;
	chip->regmap = max17332->regmap_pmic;
	chip->regmap_nvm = max17332->regmap_nvm;
	chip->pdata = pdata;
	chip->rsense = max17332->pdata->rsense;
	chip->cache = max17332->read_failure_cache;
#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
	chip->emul_batt_capacity = INVALID_EMUL_BATT_CAPACITY;
	chip->emul_batt_temperature = INVALID_EMUL_BATT_TEMPERATURE;
	chip->emul_mix_soc = INVALID_EMUL_MIX_SOC;
	chip->emul_voltage_ocv = INVALID_EMUL_VOLTAGE_OCV;
#endif
	max17332_resolve_pack(chip);

#if defined(CONFIG_OF)
	ret = max17332_fg_parse_dt(chip);
	if (ret < 0) {
		pr_err("%s not found fuelgauge dt! ret[%d]\n",
				__func__, ret);
	}
#else
	pdata = dev_get_platdata(&pdev->dev);
#endif

	platform_set_drvdata(pdev, chip);
	chip->psy_batt_d.name     = max17332->battery_name;
	chip->psy_batt_d.type     = POWER_SUPPLY_TYPE_BATTERY;
	chip->psy_batt_d.properties   = max17332_fg_battery_props;
	chip->psy_batt_d.get_property = max17332_fg_get_property;
	chip->psy_batt_d.set_property = max17332_fg_set_property;
	chip->psy_batt_d.property_is_writeable   = max17332_property_is_writeable;
	chip->psy_batt_d.num_properties =
		ARRAY_SIZE(max17332_fg_battery_props);
	chip->psy_batt_d.no_thermal = true;
	fg_cfg.drv_data = chip;
	batt_supplied_to[0] = (char *)chip->psy_batt_d.name;
	fg_cfg.supplied_to = batt_supplied_to;
	fg_cfg.of_node = max17332->dev->of_node;
	fg_cfg.num_supplicants = ARRAY_SIZE(batt_supplied_to);

	chip->fg_config_update_work_params.fg_config_update_algo_version = 1;
	chip->fg_config_update_work_params.fg_config_update_counter = 0;
	chip->fg_config_update_work_params.fg_config_update_fail_counter = 0;
	chip->fg_config_update_work_params.fg_config_update_success_counter = 0;
	chip->fg_config_update_work_params.fg_config_update_write_fail_register = 0;
	chip->fg_config_update_work_params.fg_config_update_write_fail_register_value = 0;
	chip->fg_config_update_work_params.fg_config_update_entry_reason =
		MAX17332_FG_CONFIG_UPDATE_ENTRY_REASON_UNKNOWN;

	ret = max173322_get_ini_rev(chip);
	if (ret < 0)
		dev_err(chip->dev,
			"%s : Failed to read ini rev\n", __func__);
	else
		dev_info(chip->dev,
			"%s : ini rev: %u, desired rev: %d\n", __func__,
			chip->ini_rev, chip->pdata->fg_config_version);

	reason = check_fg_config_version(chip);
	if (!reason) {
		reason = check_capacity_thresholds(chip);
	}
	if (!reason) {
		reason = check_config_mismatch(chip);
	}
	if (reason) {
		chip->fg_config_update_work_params.fg_config_update_entry_reason = reason;
		chip->requires_fg_update = true;
	}

	ret = init_thermal_zone(pdev, chip);
	if (ret) {
		pr_err("couldn't create thermal zone for max17332 rc=%d", ret);
	}

	INIT_DELAYED_WORK(&chip->notify_work, notify_work);
	INIT_DELAYED_WORK(&chip->alert_work, alert_work);
	INIT_DELAYED_WORK(&chip->alert_enable_work, alert_enable_work);
	INIT_DELAYED_WORK(&chip->fg_config_update_work, fg_config_update_work);
	atomic_set(&chip->fg_update_in_progress, 0);

	chip->battery =
		power_supply_register(max17332->dev,
				&chip->psy_batt_d,
				&fg_cfg);
	if (IS_ERR(chip->battery)) {
		ret = PTR_ERR(chip->battery);
		pr_err("Couldn't register battery ret=%d\n", ret);
		goto error_psy_put;
	}

	if (max17332->irq > 0) {
		int irq_flags;
		struct pinctrl *pinctrl;

		irq_flags = irq_get_trigger_type(max17332->irq);
		if (!irq_flags)
			irq_flags = IRQF_TRIGGER_FALLING;

		if (irq_flags == IRQ_TYPE_LEVEL_LOW ||
			irq_flags == IRQ_TYPE_LEVEL_HIGH)
			chip->pdata->is_irq_level_trigger = true;

		ret = devm_request_threaded_irq(max17332->dev, max17332->irq, NULL,
			max17332_fg_irq_isr,
			irq_flags | IRQF_ONESHOT | IRQF_SHARED,
			"fuelgauge-irq", chip);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			goto error_fg_irq;
		}
		dev_dbg(&pdev->dev,
			"MAX17332 Fuel-Gauge irq requested %d\n", max17332->irq);

		pinctrl = devm_pinctrl_get(max17332->dev);
		if (!IS_ERR(pinctrl)) {
			struct pinctrl_state *alert_enable_state;

			alert_enable_state =
				pinctrl_lookup_state(pinctrl, "alert_enable");
			if (!IS_ERR(alert_enable_state)) {
				ret = pinctrl_select_state(pinctrl,
							alert_enable_state);
				if (ret < 0) {
					pr_err("alert_enable set fail %d\n",
									ret);
					devm_pinctrl_put(pinctrl);
					goto error_init;
				}
			}
			devm_pinctrl_put(pinctrl);
		}
	}

	ret = max17332_fg_initialize(chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Error: Initializing fuel-gauge\n");
		goto error_init;
	}

	/*
	 * Enable alerts only after fg_initialize() succeeds. Enabling alerts
	 * before this point allows IRQs to fire and schedule delayed work
	 * (alert_work) while the driver is not fully initialized. If probe
	 * subsequently fails, the error path frees the chip struct without
	 * cancelling pending work, leading to a use-after-free kernel panic
	 * (NULL pointer dereference in call_timer_fn).
	 */
	max17332_update_bits(chip->regmap, REG_CONFIG,
		BIT_CONFIG_ALRT_EN,
		BIT_CONFIG_ALRT_EN);

	ret = max17332_fg_create_attrs(&chip->battery->dev);
	if (ret) {
		dev_err(chip->dev,
			"%s : Failed to create_attrs\n", __func__);
	}

	chip->nb.notifier_call = max17332_psy_notifier_call;
	ret = power_supply_reg_notifier(&chip->nb);
	if (ret) {
		dev_err(chip->dev,
			"%s : Failed to register psy notifier\n", __func__);
		goto error_init;
	}

	/* Clear alerts */
	max17332_write_unlock(chip->max17332, chip->regmap,
			REG_PROTALRTS, (u16)~REG_PROTALRTS_MASK);
	max17332_write_unlock(chip->max17332, chip->regmap, REG_STATUS, (u16)~REG_STATUS_MASK);

	/* Enable charger bob */
	max17332_get_charger_bob(max17332);

	ret = sysfs_create_group(&pdev->dev.kobj, &attr_group);
	if (ret < 0) {
		dev_err(chip->dev, "%s : Failed to expose battery capacity level to sysfs: %d\n",
				__func__, ret);
		goto error_init;
	}

	/* Making sure controlled charge is disabled on probe */
	max17332_read(chip->regmap_nvm, REG_N_VCHG_CFG1, &data);
	if (data == (u16)NVCHGCFG1_CONTROLLED_CHARGE_ON){
		// controlled charge did not exit properly, reset nVChgCfg1

		// Write to nVChgCfg1
		data = NVCHGCFG1_CONTROLLED_CHARGE_OFF;
		ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm,
				REG_N_VCHG_CFG1, data);
		if (ret < 0) {
			pr_err("%s: fail to write REG_N_VCHG_CFG1\n", __func__);
			goto error_init;
		}

		// Write to nStepVolt
		data = NVSTEPVOLT_CONTROLLED_CHARGE_OFF;
		ret = max17332_write_unlock(chip->max17332, chip->regmap_nvm,
				REG_N_STEP_V, data);
		if (ret < 0) {
			pr_err("%s: fail to write REG_N_STEP_V\n", __func__);
			goto error_init;
		}
	}

	// Disable Manual Charging on probe
	if(max17332_configure_manual_charging(chip, 0) != 0) {
		pr_err("%s : failed to disable Manual Charging\n", __func__);
	}

	/**
	 * Check if the USB supply is enabled and trigger the notifier manually
	 * since we may have missed it.
	 */
	ret = power_supply_get_property(chip->usb_charger,
			POWER_SUPPLY_PROP_ONLINE, &prop);
	if (!ret && prop.intval)
		schedule_delayed_work(&chip->notify_work, 0);

#if (IS_ENABLED(CONFIG_METASOC))
	chip->metasoc_needs_init = true;
	chip->metasoc_init_val = -1;
	chip->metasoc_init_time = -1;
	low_volt_register_callback(low_voltage_comparator_update, chip);
#endif

	pr_info("%s: Done to Load MAX17332 Fuelgauge Driver\n", __func__);

	return 0;

error_init:
	if (max17332->irq)
		free_irq(max17332->irq, chip);
	/*
	 * Cancel any pending delayed works after disabling IRQ. Freeing the
	 * IRQ first ensures no new work can be scheduled while we cancel.
	 * Without cancellation, a previously scheduled delayed work timer
	 * fires after kfree(chip), causing a NULL pointer dereference in
	 * call_timer_fn (use-after-free on the freed delayed_work struct).
	 */
	cancel_delayed_work_sync(&chip->notify_work);
	cancel_delayed_work_sync(&chip->alert_work);
	cancel_delayed_work_sync(&chip->alert_enable_work);
	cancel_delayed_work_sync(&chip->fg_config_update_work);
error_fg_irq:
	power_supply_unregister(chip->battery);
error_psy_put:
	power_supply_put(chip->usb_charger);
error:
	kfree(chip);
	return ret;
}

static int max17332_fg_remove(struct platform_device *pdev)
{
	struct max17332_fg_chip *chip = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&chip->notify_work);
	cancel_delayed_work_sync(&chip->alert_work);
	cancel_delayed_work_sync(&chip->alert_enable_work);
	cancel_delayed_work_sync(&chip->fg_config_update_work);

	/*
	 * Safe to call unreg notifier even if notifier block wasn't registered
	 * with the kernel due to some prior failure.
	 */
	power_supply_unreg_notifier(&chip->nb);

	power_supply_put(chip->usb_charger);
	power_supply_unregister(chip->battery);
	sysfs_remove_group(&pdev->dev.kobj, &attr_group);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM
static int max17332_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct max17332_fg_chip *chip = platform_get_drvdata(pdev);

	if (atomic_read(&chip->fg_update_in_progress))
		return -EBUSY;

	// WRITE THE BATTERY STATS TO THE STRUCT
	chip->suspend_voltage = chip->cached_voltage;
	chip->suspend_battery_charge_counter = chip->cached_battery_charge_counter;
	chip->suspend_battery_pct = chip->cached_battery_pct;

	flush_delayed_work(&chip->notify_work);
	flush_delayed_work(&chip->alert_work);
	flush_delayed_work(&chip->alert_enable_work);

	return 0;
}

static int max17332_resume(struct platform_device *pdev)
{
#if (IS_ENABLED(CONFIG_METASOC))
	struct max17332_fg_chip *chip = platform_get_drvdata(pdev);

	// force metasoc to be recomputed if repsoc is 0%
	if (chip->cached_repsoc == 0)
		power_supply_changed(chip->battery);
#endif

	return 0;
}

#else
#define max17332_suspend NULL
#define max17332_resume NULL
#endif

static const struct platform_device_id max17332_fg_id[] = {
	{ "max17332-battery", 0, },
	{ "max17332-battery-l", 0, },
	{ "max17332-battery-r", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max17332_fg_id);

static struct platform_driver max17332_fg_driver = {
	.driver = {
		.name = "max17332-battery",
	},
	.probe = max17332_fg_probe,
	.remove = max17332_fg_remove,
	.id_table = max17332_fg_id,
	.suspend =  max17332_suspend,
	.resume = max17332_resume,
};
module_platform_driver(max17332_fg_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("opensource@maximintegrated.com ");
MODULE_DESCRIPTION("MAX17332 Fuel Gauge");
MODULE_VERSION("1.2");
