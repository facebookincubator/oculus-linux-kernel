# MAX17332 Battery Driver Sysfs Nodes Documentation

This document explains all sysfs nodes exposed by the MAX17332 battery fuel gauge driver.

---

### capacity
**Access:** Read-only
**Register:** REG_REPSOC (0x006) / Metasoc Value
**Description:** Reports the battery state of charge (SoC) as a percentage (0-100%). The value is read from the RepSOC register (bits 15:8) or from the Metasoc algorithm. The driver apply capacity remapping based on configuration and may use MetaSOC algorithm if enabled. It would also output an emulated value if emulation is happening.
**Units:** Percentage (%)

### charge_counter
**Access:** Read-only
**Register:** REG_REPCAP (0x005)
**Description:** Reported capacity in microampere-hours (μAh). This is calculated from the RepCap register using the formula: `value * 5000 / rsense`, where rsense is the sense resistor value in milliohms.
**Units:** Microampere-hours (μAh)

### charge_full
**Access:** Read-only
**Register:** REG_FULLCAPREP (0x010)
**Description:** Full capacity of the battery as reported by the fuel gauge. This represents the full capacity that the battery can hold at the current temperature and age.
**Units:** Microampere-hours (μAh)
**Conversion:** Same as CHARGE_COUNTER

### voltage_now
**Access:** Read-only
**Register:** REG_VCELL (0x01A)
**Description:** Instantaneous battery voltage measured at the terminals. The register value is converted using: `value * 78.125μV`.
**Units:** Microvolts (μV)
**Conversion:** LSB = 78.125μV (1.25mV/16)

### voltage_rep
**Access:** Read-only
**Register:** REG_VCELL_REP (0x012)
**Description:** Representative voltage - a filtered/averaged voltage value that's more stable than instantaneous readings.
**Units:** Microvolts (μV)
**Conversion:** LSB = 78.125μV

### voltage_avg
**Access:** Read-only
**Register:** REG_AVGVCELL (0x019)
**Description:** Average battery voltage calculated over a rolling window. This provides a smoothed voltage reading less susceptible to transient changes.
**Units:** Microvolts (μV)
**Conversion:** LSB = 78.125μV

### voltage_ocv
**Access:** Read-only
**Register:** REG_VFOCV (0x0FB)
**Description:** Open Circuit Voltage (OCV) - the estimated voltage of the battery at rest with no load. This is used for SoC estimation.
**Units:** Microvolts (μV)
**Conversion:** LSB = 78.125μV

### current_now
**Access:** Read-only
**Register:** REG_CURRENT (0x01C)
**Description:** Instantaneous battery current. Positive values indicate charging, negative values indicate discharging.
**Units:** Microamperes (μA)
**Conversion:** `value * 15625 / (rsense * 10)` where value is sign-extended 16-bit

### current_rep
**Access:** Read-only
**Register:** REG_CURRENT_REP (0x022)
**Description:** Representative current - a filtered current value.
**Units:** Microamperes (μA)
**Conversion:** Same as CURRENT_NOW

### current_avg
**Access:** Read-only
**Register:** REG_AVGCURRENT (0x01D)
**Description:** Average current over a time window.
**Units:** Microamperes (μA)
**Conversion:** Same as CURRENT_NOW

### power_now
**Access:** Read-only
**Register:** REG_POWER (0x0B1)
**Description:** Instantaneous power consumption/generation.
**Units:** Microwatts (μW)
**Conversion:** `value * (800 * 10 / rsense)` - LSB is 800μW for 10mΩ Rsense

### power_avg
**Access:** Read-only
**Register:** REG_POWER_AVG (0x0B3)
**Description:** Average power over a time window.
**Units:** Microwatts (μW)
**Conversion:** Same as POWER_NOW

### temp
**Access:** Read-only
**Register:** REG_TEMP (0x01B)
**Description:** Battery temperature measured by the fuel gauge.
**Units:** Decidegrees Celsius (0.1°C)
**Conversion:** `(value * 10) >> 8` - LSB = 1/256°C

### cycle_count
**Access:** Read-only
**Register:** REG_CYCLES (0x017)
**Description:** Number of charge/discharge cycles the battery has undergone.
**Units:** Cycles
**Conversion:** `value * 25 / 100` - LSB = 25% of a cycle

### temp_alert_max
**Access:** Read-write
**Register:** REG_TALRTTH (0x002) - upper byte
**Description:** Upper temperature threshold for alerts.
**Units:** Decidegrees Celsius (0.1°C)
**Conversion:** LSB = 1°C (converted to 0.1°C for userspace)

### temp_alert_min
**Access:** Read-write
**Register:** REG_TALRTTH (0x002) - lower byte
**Description:** Lower temperature threshold for alerts.
**Units:** Decidegrees Celsius (0.1°C)
**Conversion:** LSB = 1°C (converted to 0.1°C for userspace)

### capacity_alert_max
**Access:** Read-write
**Register:** REG_SALRTTH (0x003) - upper byte
**Description:** Upper SoC threshold for alerts.
**Units:** Percentage (%)

### capacity_alert_min
**Access:** Read-write
**Register:** REG_SALRTTH (0x003) - lower byte
**Description:** Lower SoC threshold for alerts.
**Units:** Percentage (%)

### status
**Access:** Read-only
**Register:** Multiple (USB status, BOB status, ProtStatus for FULL bit)
**Description:** Overall charging status of the battery.
**Values:**
- `Charging`: USB connected and BOB active
- `Discharging`: USB not connected or BOB inactive
- `Full`: Battery is fully charged (based on ProtStatus FULL bit)

### voltage_min_design
**Access:** Read-only
**Register:** REG_VEMPTY (0x03A)
**Description:** Empty voltage threshold.
**Units:** Microvolts (μV)
**Conversion:** `(value >> 7) * 10000` - LSB = 10mV

### health
**Access:** Read-only
**Register:** REG_PROTSTATUS (0x0D9)
**Description:** Battery health status based on protection alerts.
**Values:**
- `Unknown`: Pre-qualification failure
- `Overheat`: Temperature too high
- `Dead`: Undervoltage, permanent fail, or shutdown
- `Cold`: Temperature too low
- `Overvoltage`: Overvoltage protection triggered
- `Unspecified Failure`: Overflow, overcharge current, or overdischarge current
- `Watchdog Timer Expire`: Charge watchdog timeout
- `Good`: No protection issues

### time_to_empty_avg
**Access:** Read-only
**Register:** REG_TTE (0x011)
**Description:** Estimated time until battery is empty at current discharge rate.
**Units:** Seconds
**Conversion:** `(value * 45) >> 3` - LSB = 5.625 seconds

### technology
**Access:** Read-only
**Register:** N/A
**Description:** Battery chemistry type.
**Value:** Always returns "Li-ion" (POWER_SUPPLY_TECHNOLOGY_LION)

### present
**Access:** Read-only
**Register:** N/A
**Description:** Indicates if battery is present.
**Value:** Always returns 1 (battery present) when driver is loaded

### capacity_level
**Access:** Read-only
**Register:** N/A
**Description:** Discrete battery capacity level.
**Values:**
- `Critical`: ≤ cap_critical_lvl (default 3%)
- `Low`: ≤ cap_low_lvl (default 10%)
- `Normal`: Between low and high thresholds
- `High`: ≥ cap_high_lvl (default 90%)
- `Full`: Battery full bit set or overcharge condition detected

### ini_version
**Access:** Read-only
**Register:** REG_RSENSE_NVM (0x19C) - upper byte
**Description:** INI configuration file revision number programmed into NVM.
**Units:** Version number (0-255)

### max17332_program_nvm
**Access:** Write-only
**Register:** N/A
**Description:** Programs non-volatile memory with new configuration from a firmware file. This is a critical operation that updates the fuel gauge's persistent configuration. The firmware file should contain properly formatted register address/value pairs.
**Usage:** Echo firmware file path to this node.

### nvm_updates_remaining
**Access:** Read-only
**Register:** REG_REMAINING_UPDATES_NVM (0x1FD)
**Description:** Number of remaining NVM write cycles available. The MAX17332 has a limited number of NVM write cycles (typically 8).
**Units:** Count (0-8)
**Determination:** Uses recall history command (0xE29B) then reads bit pattern to count remaining writes.

### voltage_pack_now
**Access:** Read-only
**Register:** REG_PCKP (0x0DB)
**Description:** Pack voltage (total voltage across all cells in series).
**Units:** Microvolts (μV)
**Conversion:** `(value * 625) / 2` = 312.5μV per bit

### coulomb_counter
**Access:** Read-only
**Register:** REG_QH (0x04D)
**Description:** Accumulated charge counter tracking total charge passed through the battery.
**Units:** Microampere-hours (μAh)
**Conversion:** `value * 5000 / rsense`

### charge_full_nom
**Access:** Read-only
**Register:** REG_FULLCAPNOM (0x023)
**Description:** Nominal full capacity - the capacity at which the battery is considered full at nominal conditions.
**Units:** Microampere-hours (μAh)
**Conversion:** Same as coulomb_counter

### rsense
**Access:** Read-only
**Description:** Current sense resistor value used for current and capacity calculations.
**Units:** Milliohms (mΩ)
**Source:** Platform data configuration

### voltage_alert_max
**Access:** Read-write
**Register:** REG_VALRTTH (0x001) - upper byte
**Description:** Upper voltage threshold for alerts.
**Units:** Microvolts (μV)
**Conversion:** LSB = 20mV (20000μV)

### voltage_alert_min
**Access:** Read-write
**Register:** REG_VALRTTH (0x001) - lower byte
**Description:** Lower voltage threshold for alerts.
**Units:** Microvolts (μV)
**Conversion:** LSB = 20mV (20000μV)

### current_alert_max
**Access:** Read-only
**Register:** REG_IALRTTH (0x0AC) - upper byte
**Description:** Maximum current threshold for alerts.
**Units:** Microamperes (μA)
**Conversion:** `sign_extend32(value, 7) * 400000 / rsense`

### current_alert_min
**Access:** Read-only
**Register:** REG_IALRTTH (0x0AC) - lower byte
**Description:** Minimum current threshold for alerts.
**Units:** Microamperes (μA)
**Conversion:** Same as current_alert_max

### timer
**Access:** Read-only
**Register:** REG_TIMER (0x03E)
**Description:** Lower 16-bit timer value tracking elapsed time.
**Units:** Milliseconds
**Conversion:** `value * 1758 / 10` = 175.8ms per LSB

### timerh
**Access:** Read-only
**Register:** REG_TIMERH (0x0BE)
**Description:** Upper timer value for extended time tracking.
**Units:** Hours (with one decimal place)
**Conversion:** `value * 32` (displayed as X.X hours) - LSB = 3.2 hours

### battery_chgstat
**Access:** Read-only
**Register:** REG_CHGSTAT (0x0A3)
**Description:** Charging status register decoded into human-readable format.
**Format:** `Dropout:X CP:X CT:X CC:X CV:X` where X is 0 or 1
**Bits:**
- Dropout: Dropout condition (bit 15)
- CP: Charge Pump active (bit 3)
- CT: Charge Temperature limit (bit 2)
- CC: Constant Current mode (bit 1)
- CV: Constant Voltage mode (bit 0)

### batt_status
**Access:** Read-only
**Register:** REG_STATUS (0x000)
**Description:** Battery status register value in hexadecimal.
**Format:** 4-digit hex value (e.g., "0x1234")

### prot_status
**Access:** Read-only
**Register:** REG_PROTSTATUS (0x0D9)
**Description:** Protection status register showing various protection conditions.
**Format:** 4-digit hex value

### prot_alrt
**Access:** Read-only
**Register:** REG_PROTALRTS (0x0AF)
**Description:** Protection alerts register.
**Format:** 4-digit hex value

### fet_status
**Access:** Read-only
**Register:** REG_PROTCFG2 (0x0F1)
**Description:** FET (Field Effect Transistor) status and configuration.
**Format:** 4-digit hex value
**Note:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - This register's detailed bit definitions need verification from datasheet.

### batt_config
**Access:** Read-only
**Register:** REG_CONFIG (0x00B)
**Description:** Main configuration register.
**Format:** 4-digit hex value

### batt_config2
**Access:** Read-only
**Register:** REG_CONFIG2 (0x0AB)
**Description:** Secondary configuration register.
**Format:** 4-digit hex value

### comm_status
**Access:** Read-only
**Register:** REG_COMMSTAT (0x061)
**Description:** Communication status including NV error status, charge/discharge FET status.
**Format:** 4-digit hex value
**Important Bits:**
- Bit 2: NVError - NVM write error
- Bit 3: NVBusy - NVM operation in progress
- Bit 8: ChgOff - Charge FET disabled
- Bit 9: DisOff - Discharge FET disabled

### slack
**Access:** Read-only
**Register:** REG_SLACK (0x16B) in NVM
**Description:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - Slack register purpose needs datasheet reference.
**Format:** 4-digit hex value

### ini_rev
**Access:** Read-only
**Register:** REG_RSENSE_NVM (0x19C) - upper byte
**Description:** INI configuration revision number.
**Units:** Integer (0-255)

### sip_sn
**Access:** Read-only
**Registers:** 0x1BA, 0x1E0, 0x1E1, 0x1E6, 0x1E7 (NVM)
**Description:** System-in-Package (SIP) serial number as character string.
**Format:** ASCII string (10 characters + newline)

### pack_sn
**Access:** Read-only
**Registers:** 0x1E9, 0x1EA, 0x1EB, 0x1EC, 0x1ED, 0x1EE, 0x1EF (NVM)
**Description:** Battery pack serial number.
**Format:** ASCII string (14 characters + newline)

### bmu_smt_date
**Access:** Read-only
**Register:** REG_BMU_SMT_DATE (0x1E8) in NVM
**Description:** Battery Management Unit Surface Mount Technology date code.
**Format:** 4-digit hex value
**Note:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - Date encoding format needs verification.

### full_cap
**Access:** Read-only
**Register:** REG_FULL_CAP (0x035)
**Description:** Full capacity register.
**Units:** Microampere-hours (μAh)
**Conversion:** `value * 5000 / rsense`

### av_cap
**Access:** Read-only
**Register:** REG_AV_CAP (0x01F)
**Description:** Available capacity.
**Units:** Microampere-hours (μAh)
**Conversion:** Same as full_cap

### av_soc
**Access:** Read-only
**Register:** REG_AC_SOC (0x00E)
**Description:** Available state of charge.
**Units:** Percentage (%)
**Conversion:** Upper byte (bits 15:8)

### mix_cap
**Access:** Read-only
**Register:** REG_MIX_CAP (0x02B)
**Description:** Mixed capacity (combination of voltage-based and coulomb-counting).
**Units:** Microampere-hours (μAh)
**Conversion:** Same as full_cap

### mix_soc
**Access:** Read-only
**Register:** REG_MIX_SOC (0x00D)
**Description:** Mixed state of charge.
**Units:** Percentage (%)
**Conversion:** Upper byte (bits 15:8)

### vfrem_cap
**Access:** Read-only
**Register:** REG_VFREM_CAP (0x04A)
**Description:** Voltage-based remaining capacity.
**Units:** Microampere-hours (μAh)
**Conversion:** Same as full_cap

### vf_soc
**Access:** Read-only
**Register:** REG_VF_SOC (0x0FF)
**Description:** Voltage-based state of charge.
**Units:** Percentage (%)
**Conversion:** Upper byte (bits 15:8)

### q_residual
**Access:** Read-only
**Register:** REG_Q_RESIDUAL (0x00C)
**Description:** Charge residual - difference between mixed and coulomb counter.
**Units:** Microampere-hours (μAh)
**Conversion:** Same as full_cap

### qr_table_00, qr_table_10, qr_table_20, qr_table_30
**Access:** Read-only
**Registers:** REG_QR_TABLE_00 (0x1A0), REG_QR_TABLE_10 (0x1A1), REG_QR_TABLE_20 (0x1A2), REG_QR_TABLE_30 (0x1A3) in NVM
**Description:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - QResidual table entries at different temperatures/SoC points. Refer to datasheet for interpretation.
**Format:** 4-digit hex values

### rcomp0
**Access:** Read-only
**Register:** REG_RCOMP0 (0x1A6) in NVM
**Description:** Temperature compensation parameter for battery resistance at 20°C.
**Format:** 4-digit hex value

### temp_co
**Access:** Read-only
**Register:** REG_TEMP_CO (0x1A7) in NVM
**Description:** Temperature coefficient for compensation.
**Format:** 4-digit hex value

### lock
**Access:** Read-only
**Register:** REG_LOCK (0x07F)
**Description:** Write protection lock register.
**Format:** 4-digit hex value
**Note:** Used internally to protect configuration registers from accidental writes.

### suspend_battery_pct
**Access:** Read-only
**Description:** Battery percentage at last system suspend.
**Units:** Percentage (%)
**Source:** Cached value from suspend path

### suspend_charge_counter
**Access:** Read-only
**Description:** Charge counter value at last system suspend.
**Units:** Microampere-hours (μAh)
**Source:** Cached value from suspend path

### suspend_voltage
**Access:** Read-only
**Description:** Battery voltage at last system suspend.
**Units:** Raw register value (needs conversion)
**Source:** Cached voltage register value

### change_counter_cp, change_counter_dropout, change_counter_ovp, change_counter_occp
**Access:** Read-only
**Description:** Event counters tracking how many times each protection condition has occurred:
- `cp`: Charge Pump events
- `dropout`: Dropout events
- `ovp`: Overvoltage Protection events
- `occp`: Overcharge Current Protection events
**Units:** Count

### cycle_count_frac
**Access:** Read-only
**Register:** REG_CYCLES (0x017)
**Description:** Cycle count with fractional precision.
**Format:** "XX.YY" where XX is cycles and YY is fractional part
**Conversion:** `(value * 25) / 100` giving 0.25 cycle resolution

### controlled_charge_on
**Access:** Read-write
**Registers:** REG_N_VCHG_CFG1 (0x1CC), REG_N_STEP_V (0x1C5) in NVM
**Description:** Enable/disable controlled charging mode for retail demo or special charging profiles.
**Values:** 0 = off, 1 = on
**Note:** Modifies NVM registers, use carefully

### learn_stage
**Access:** Read-only
**Register:** REG_LEARNCFG (0x0A1)
**Description:** Current learning stage of the fuel gauge algorithm.
**Units:** Integer (0-7)
**Bits:** Extracted from bits 6:4 of LEARNCFG register

### timer_seconds
**Access:** Read-only
**Registers:** REG_TIMER (0x03E), REG_TIMERH (0x0BE)
**Description:** Combined timer value in seconds. Reads both TIMER and TIMERH registers with atomicity check.
**Units:** Seconds
**Conversion:** Combines low and high timer registers to total milliseconds, then divides by 1000

### i2c_read_failure_count, i2c_write_failure_count
**Access:** Read-only
**Description:** Diagnostic counters tracking I2C communication failures.
**Units:** Count
**Source:** Internal driver statistics

### ncgain
**Access:** Read-only
**Register:** REG_NCGAIN (0x1C8) in NVM
**Description:** Gain compensation value for coulomb counter.
**Format:** Hexadecimal value

### rcell
**Access:** Read-only
**Register:** REG_RCELL (0x014)
**Description:** Internal resistance of the battery cell.
**Units:** Microohms (μΩ)
**Conversion:** `(value * 1000000) / 4096` - LSB = 1/4096 Ohms

### manual_charging_on
**Access:** Read-write
**Registers:** REG_CONFIG (0x00B), REG_CHGVOLTAGE (0x02A), REG_CHGCURRENT (0x028)
**Description:** Enable/disable manual charging mode with fixed 4.1V limit.
**Values:** 0 = autonomous charging, 1 = manual 4.1V charging
**Note:** Used for special charging scenarios

### last_batt_status
**Access:** Read-only
**Description:** Last cached battery status register value before it was cleared.
**Format:** 4-digit hex value
**Source:** Cached from interrupt handler

### last_prot_status
**Access:** Read-only
**Description:** Last cached protection status register value before it was cleared.
**Format:** 4-digit hex value
**Source:** Cached from interrupt handler

### trim1
**Access:** Read-only
**Register:** REG_TRIM1 (0x051)
**Description:** Factory trim value.
**Units:** Integer (masked to bits 1:0)
**Format:** Decimal value

### target_chg_voltage
**Access:** Read-only
**Register:** REG_TARGET_CHG_V (0x02A)
**Description:** Target charging voltage.
**Units:** Microvolts (μV)
**Conversion:** Using raw_voltage_to_uvolts function

### target_chg_current
**Access:** Read-only
**Register:** REG_TARGET_CHG_I (0x028)
**Description:** Target charging current.
**Units:** Microamperes (μA)
**Conversion:** Using raw_current_to_uamps function

### batt_cap_low_lvl
**Access:** Read-write
**Description:** Battery capacity threshold for "low" level indication.
**Units:** Percentage (%)
**Default:** 10%
**Range:** Must be greater than cap_critical_lvl and less than 16%

### batt_cap_critical_lvl
**Access:** Read-write
**Description:** Battery capacity threshold for "critical" level indication.
**Units:** Percentage (%)
**Default:** 3%
**Range:** Must be less than cap_low_lvl and less than 15%

### design_cap
**Access:** Read-only
**Register:** REG_DESIGN_CAP (0x018)
**Description:** Design capacity of the battery.
**Units:** Microampere-hours (μAh)
**Conversion:** `(value * 5 * 1000) / rsense`

### n_design_cap
**Access:** Read-only
**Register:** REG_N_DESIGN_CAP (0x1B3) in NVM
**Description:** Non-volatile design capacity with QScale encoding.
**Units:** Microampere-hours (μAh)
**Conversion:** `((value >> 6) * qscale_capacity_step_size[value & 0x07] * 10) / rsense`

### is_virtual_battery
**Access:** Read-only
**Register:** N/A
**Description:** Indicates if this is a virtual/emulated battery.
**Value:** Always returns 1 (driver supports one battery only)

### fg_config_update_algo_version
**Access:** Read-only
**Register:** N/A
**Description:** Version number of the fuel gauge config update algorithm.
**Units:** Integer
**Source:** Driver internal version tracking

### fg_config_update_counter
**Access:** Read-only
**Register:** N/A
**Description:** Total number of times fuel gauge config update has been attempted.
**Units:** Count
**Source:** Driver statistics

### fg_config_update_fail_counter
**Access:** Read-only
**Register:** N/A
**Description:** Number of failed fuel gauge config update attempts.
**Units:** Count
**Source:** Driver statistics

### fg_config_update_success_counter
**Access:** Read-only
**Register:** N/A
**Description:** Number of successful fuel gauge config updates.
**Units:** Count
**Source:** Driver statistics

### fg_config_update_write_fail_register
**Access:** Read-only
**Register:** N/A
**Description:** Register address where last config update write failure occurred.
**Format:** 4-digit hex value
**Source:** Driver diagnostics

### fg_config_update_write_fail_register_value
**Access:** Read-only
**Register:** N/A
**Description:** Value read back from register after failed write.
**Format:** 4-digit hex value
**Source:** Driver diagnostics

### fg_config_update_entry_reason
**Access:** Read-only
**Register:** N/A
**Description:** Reason why fuel gauge config update was triggered.
**Values:**
- 0: Unknown
- 1: INI revision mismatch
- 2: Full capacity mismatch
- 3: Config register mismatch

### unmapped_capacity
**Access:** Read-only
**Register:** N/A
**Description:** Raw battery capacity before any remapping is applied.
**Units:** Percentage (%)
**Source:** RepSOC register before remapping logic

### battery_name
**Access:** Read-only
**Register:** N/A
**Description:** Battery identifier/name.
**Value:** Always returns "0" (single battery system)

### dietemp
**Access:** Read-only
**Register:** REG_DIETEMP (0x034)
**Description:** Die temperature of the MAX17332 IC itself.
**Units:** Decidegrees Celsius (0.1°C)
**Conversion:** `(signed_value * 10) / 256` - LSB = 1/256°C, signed 16-bit

### fstat
**Access:** Read-only
**Register:** REG_FSTAT (0x03D)
**Description:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - Fuel gauge status register, detailed bit meanings need datasheet.
**Format:** 4-digit hex value

### fstat2
**Access:** Read-only
**Register:** REG_FSTAT2 (0x039)
**Description:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - Secondary fuel gauge status register, needs datasheet reference.
**Format:** 4-digit hex value

### hprotcfg
**Access:** Read-only
**Register:** REG_HPROTCFG (0x0F0)
**Description:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - Hardware protection configuration, needs datasheet reference.
**Format:** 4-digit hex value

### fotpstat
**Access:** Read-only
**Register:** REG_FOTPSTAT (0x0BB)
**Description:** I DON'T KNOW THIS! YOU DO THIS INSTEAD - Fuel gauge OTP status, needs datasheet reference.
**Format:** 4-digit hex value

### fprotstat
**Access:** Read-only
**Register:** REG_FPROTSTAT (0x0DA)
**Description:** Fuel gauge protection status.
**Format:** 4-digit hex value
**Note:** Different from main PROTSTATUS, likely for internal FG protection states

### n_battstatus
**Access:** Read-only
**Register:** REG_N_BATTSTATUS (0x1A8) in NVM
**Description:** Non-volatile battery status saved to NVM.
**Format:** 4-digit hex value

### vreg_out_delta_rise
**Access:** Read-write
**Register:** N/A
**Description:** Voltage delta increase step when adjusting regulator output.
**Units:** Microvolts (μV)
**Source:** Voltage adjustment module

### vreg_out_delta_fall
**Access:** Read-write
**Register:** N/A
**Description:** Voltage delta decrease step when adjusting regulator output.
**Units:** Microvolts (μV)
**Source:** Voltage adjustment module

### vreg_uv_max
**Access:** Read-write
**Register:** N/A
**Description:** Maximum voltage regulator output.
**Units:** Microvolts (μV)
**Source:** Voltage adjustment module

### vreg_dropout_min
**Access:** Read-write
**Register:** N/A
**Description:** Minimum dropout voltage for regulator.
**Units:** Microvolts (μV)
**Source:** Voltage adjustment module

### vsys_change_period_min
**Access:** Read-write
**Register:** N/A
**Description:** Minimum period between system voltage changes.
**Units:** Milliseconds
**Source:** Voltage adjustment module

### rep_soc
**Access:** Read-only
**Register:** REG_REPSOC (0x06)
**Description:** Representative SoC from fuel gauge (before MetaSOC processing).
**Units:** Percentage (%)
**Conversion:** Upper byte (bits 15:8)

### meta_soc
**Access:** Read-only
**Register:** N/A
**Description:** State of charge calculated by MetaSOC algorithm.
**Units:** Percentage (%)
**Source:** MetaSOC algorithm output

### meta_soc_init_val
**Access:** Read-write (write once)
**Register:** N/A
**Description:** Initial SoC value for MetaSOC initialization.
**Units:** Percentage (%)
**Note:** Can only be set once, used for calibration

### meta_soc_init_time
**Access:** Read-write (write once)
**Register:** N/A
**Description:** Initial time for MetaSOC algorithm.
**Units:** Seconds
**Note:** Can only be set once

### meta_soc_enabled
**Access:** Read-write
**Register:** N/A
**Description:** Enable/disable MetaSOC algorithm.
**Values:** 0 = disabled, 1 = enabled
**Note:** Persists to NVM bit 4 of REG_N_DESGIN_VOLT

### meta_soc_version
**Access:** Read-write
**Register:** N/A
**Description:** MetaSOC algorithm version selection.
**Values:**
- 2: Disable zero RepSOC convergence
- 3: Enable delayed zero RepSOC convergence
- 4: Enable QRes-based zero RepSOC convergence (default)
- 5: Convergence through peak power manager

### meta_soc_init
**Access:** Read-write
**Register:** N/A
**Description:** MetaSOC initialization trigger.
**Values:** 0/1 to control initialization

### meta_soc_usoc
**Access:** Read-only
**Register:** N/A
**Description:** Unfiltered State of Charge (uSOC) from MetaSOC.
**Units:** Percentage (scaled)
**Source:** MetaSOC internal stats

### meta_soc_eoc
**Access:** Read-only
**Register:** N/A
**Description:** End-of-charge SoC from MetaSOC.
**Units:** Percentage
**Source:** MetaSOC internal stats

### meta_soc_eod
**Access:** Read-only
**Register:** N/A
**Description:** End-of-discharge SoC from MetaSOC.
**Units:** Percentage
**Source:** MetaSOC internal stats

### meta_soc_low_batt_shutdown
**Access:** Write-only
**Register:** N/A
**Description:** Trigger low battery shutdown handling.
**Usage:** Write non-zero value to trigger

### meta_soc_config_id
**Access:** Read-only
**Register:** N/A
**Description:** MetaSOC configuration identifier for current pack.
**Units:** Integer
**Source:** MetaSOC config

### meta_soc_low_volt_comp_tripped
**Access:** Read-only
**Register:** N/A
**Description:** Indicates if low voltage comparator was tripped.
**Values:** 0 = not tripped, 1 = tripped
**Source:** Low voltage comparator callback

### meta_soc_usoc_filtered
**Access:** Read-only
**Register:** N/A
**Description:** Filtered uSOC value from MetaSOC.
**Units:** Percentage (scaled)
**Source:** MetaSOC internal stats

### meta_soc_peak_voltage_droop_penalty
**Access:** Read-only
**Register:** N/A
**Description:** Voltage droop penalty applied by MetaSOC.
**Units:** Percentage
**Source:** MetaSOC internal stats

### meta_soc_remaining_capacity
**Access:** Read-only
**Register:** N/A
**Description:** Remaining capacity calculated by MetaSOC.
**Units:** Milliampere-hours (mAh)
**Source:** MetaSOC internal stats

---
