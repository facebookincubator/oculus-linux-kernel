// SPDX-License-Identifier: GPL-2.0
/*
 * BQ27xxx battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali@kernel.org>
 * Copyright (C) 2017 Liam Breck <kernel@networkimprov.net>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Datasheets:
 * https://www.ti.com/product/bq27000
 * https://www.ti.com/product/bq27200
 * https://www.ti.com/product/bq27010
 * https://www.ti.com/product/bq27210
 * https://www.ti.com/product/bq27500
 * https://www.ti.com/product/bq27510-g1
 * https://www.ti.com/product/bq27510-g2
 * https://www.ti.com/product/bq27510-g3
 * https://www.ti.com/product/bq27520-g1
 * https://www.ti.com/product/bq27520-g2
 * https://www.ti.com/product/bq27520-g3
 * https://www.ti.com/product/bq27520-g4
 * https://www.ti.com/product/bq27530-g1
 * https://www.ti.com/product/bq27531-g1
 * https://www.ti.com/product/bq27541-g1
 * https://www.ti.com/product/bq27542-g1
 * https://www.ti.com/product/bq27546-g1
 * https://www.ti.com/product/bq27742-g1
 * https://www.ti.com/product/bq27545-g1
 * https://www.ti.com/product/bq27421-g1
 * https://www.ti.com/product/bq27425-g1
 * https://www.ti.com/product/bq27426
 * https://www.ti.com/product/bq27411-g1
 * https://www.ti.com/product/bq27441-g1
 * https://www.ti.com/product/bq27621-g1
 * https://www.ti.com/product/bq27z561
 * https://www.ti.com/product/bq28z610
 * https://www.ti.com/product/bq34z100-g1
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <linux/power/bq27xxx_battery.h>

#define BQ27XXX_MANUFACTURER	"Texas Instruments"

/* BQ27XXX Flags */
#define BQ27XXX_FLAG_DSC	BIT(0)
#define BQ27XXX_FLAG_SOCF	BIT(1) /* State-of-Charge threshold final */
#define BQ27XXX_FLAG_SOC1	BIT(2) /* State-of-Charge threshold 1 */
#define BQ27XXX_FLAG_CFGUP	BIT(4)
#define BQ27XXX_FLAG_FC		BIT(9)
#define BQ27XXX_FLAG_OTD	BIT(14)
#define BQ27XXX_FLAG_OTC	BIT(15)
#define BQ27XXX_FLAG_UT		BIT(14)
#define BQ27XXX_FLAG_OT		BIT(15)

/* BQ27000 has different layout for Flags register */
#define BQ27000_FLAG_EDVF	BIT(0) /* Final End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_EDV1	BIT(1) /* First End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_CI		BIT(4) /* Capacity Inaccurate flag */
#define BQ27000_FLAG_FC		BIT(5)
#define BQ27000_FLAG_CHGS	BIT(7) /* Charge state flag */

/* BQ27Z561 has different layout for Flags register */
#define BQ27Z561_FLAG_FDC	BIT(4) /* Battery fully discharged */
#define BQ27Z561_FLAG_FC	BIT(5) /* Battery fully charged */
#define BQ27Z561_FLAG_DIS_CH	BIT(6) /* Battery is discharging */

/* control register params */
#define BQ27XXX_SEALED			0x20
#define BQ27XXX_SET_CFGUPDATE		0x13
#define BQ27XXX_SOFT_RESET		0x42
#define BQ27XXX_RESET			0x41

#define BQ27XXX_RS			(20) /* Resistor sense mOhm */
#define BQ27XXX_POWER_CONSTANT		(29200) /* 29.2 µV^2 * 1000 */
#define BQ27XXX_CURRENT_CONSTANT	(3570) /* 3.57 µV * 1000 */

#define INVALID_REG_ADDR	0xff

#define BQ27Z561_ATRATE_ADDR 0x02
#define BQ27Z561_ATRATETTE_ADDR 0x04
#define BQ27Z561_STATUS_ADDR 0x0a
#define BQ27Z561_SOH_ADDR 0x2e
#define BQ27Z561_INTER_TEMP_ADDR 0x28
#define BQ27Z561_REMAIN_CAPACITY_ADDR 0x10

/* [TI FG] registers of single value for bq27z561 */
#define BQ27Z561_SET_TEMP_HI 0x6a
#define BQ27Z561_CLEAR_TEMP_HI 0x6b
#define BQ27Z561_SET_TEMP_LO 0x6c
#define BQ27Z561_CLEAR_TEMP_LO 0x6d

/* [TI FG] MAC data control for bq27z561 */
#define BQ27Z561_MAC_CMD_ADDR 0x3e
#define BQ27Z561_MAC_DATA_ADDR 0x40
#define BQ27Z561_MAC_CHECKSUM_ADDR 0x60
#define BQ27Z561_MAC_LEN_ADDR 0x61
#define BQ27Z561_MAC_BLOCK_LEN 32

/* [TI FG] MAC command to write for bq27z561  */
#define BQ27Z561_MAC_CMD_DT 0x0001		//Device Type
#define BQ27Z561_MAC_CMD_FV 0x0002		//Firmware version
#define BQ27Z561_MAC_CMD_HV 0x0003		//Hareware version
#define BQ27Z561_MAC_CMD_IFS 0x0004		//Instruction Flash Signature
#define BQ27Z561_MAC_CMD_SDFS 0x0005		//Static DF Signature
#define BQ27Z561_MAC_CMD_CID 0x0006		//Chemical ID
#define BQ27Z561_MAC_CMD_PMACW 0x0007		//Pre_MACWrite
#define BQ27Z561_MAC_CMD_SCDFS 0x0008		//Static Chem DF Signature
#define BQ27Z561_MAC_CMD_ADFS 0x0009		//All DF Signature
#define BQ27Z561_MAC_CMD_RESET 0x0012
#define BQ27Z561_MAC_CMD_GAUGING 0x0021
#define BQ27Z561_MAC_CMD_LDC 0x0023		//LifetimeDataCollection
#define BQ27Z561_MAC_CMD_LDR 0x0028		//LifetimeDataReset
#define BQ27Z561_MAC_CMD_CM 0x002d		//CalibrationMode
#define BQ27Z561_MAC_CMD_LDF 0x002e		//LifetimeDataFlush
#define BQ27Z561_MAC_CMD_SD 0x0030		//SealDevice
#define BQ27Z561_MAC_CMD_SK 0x0035		//Security Keys
#define BQ27Z561_MAC_CMD_AK 0x0037		//Authentication Key
#define BQ27Z561_MAC_CMD_RESET1 0x0041
#define BQ27Z561_MAC_CMD_SDS 0x0044		//SetDeepSleep
#define BQ27Z561_MAC_CMD_CDS 0x0045		//ClearDeepSleep
#define BQ27Z561_MAC_CMD_PG 0x0046		//PulseGPIO
#define BQ27Z561_MAC_CMD_TS 0x0047		//TambientSync
#define BQ27Z561_MAC_CMD_DN 0x004a		//Device Name
#define BQ27Z561_MAC_CMD_DC 0x004b		//Device Chem
#define BQ27Z561_MAC_CMD_MN 0x004c		//Manufacturer Name
#define BQ27Z561_MAC_CMD_MD 0x004d		//Manufacture Date
#define BQ27Z561_MAC_CMD_SN 0x004e		//Serial Number
#define BQ27Z561_MAC_CMD_OS 0x0054		//OperationStatus
#define BQ27Z561_MAC_CMD_CS 0x0055		//ChargingStatus
#define BQ27Z561_MAC_CMD_GS 0x0056		//GaugingStatus
#define BQ27Z561_MAC_CMD_MS 0x0057		//ManufacturingStatus
#define BQ27Z561_MAC_CMD_LDB1 0x0060		//Lifetime Data Block 1
#define BQ27Z561_MAC_CMD_MI_A 0x0070		//Manufacture info A

#define BQ27Z561_MAC_CMD_MI_B 0x007A		//Manufacture info B
#define BQ27Z561_MAC_CMD_MI_C 0x007B		//Manufacture info C

#define BQ27Z561_MAC_CMD_ROMMODE 0x0F00		//ROMMode
#define BQ27Z561_MAC_CMD_HDQ 0x7C40		//SwitchToHDQ
#define BQ27Z561_MAC_CMD_ECO 0xF080		//ExitCalibrationOutput
#define BQ27Z561_MAC_CMD_OCC 0xF081		//OutputCCandADCforCalibration

#define BQ27Z561_MAC_LEN	128
#define BQ27Z561_SUB_LEN	4	//2-byte command, 1-byte checksum and 1-byte length

static const char * const bq27z561_sealed_status_str[] = {
	"Reserved", "Full Access", "Unsealed", "Sealed"};

/*
 * bq27xxx_reg_index - Register names
 *
 * These are indexes into a device's register mapping array.
 */

enum bq27xxx_reg_index {
	BQ27XXX_REG_CTRL = 0,	/* Control */
	BQ27XXX_REG_TEMP,	/* Temperature */
	BQ27XXX_REG_INT_TEMP,	/* Internal Temperature */
	BQ27XXX_REG_VOLT,	/* Voltage */
	BQ27XXX_REG_AI,		/* Average Current */
	BQ27XXX_REG_FLAGS,	/* Flags */
	BQ27XXX_REG_TTE,	/* Time-to-Empty */
	BQ27XXX_REG_TTF,	/* Time-to-Full */
	BQ27XXX_REG_TTES,	/* Time-to-Empty Standby */
	BQ27XXX_REG_TTECP,	/* Time-to-Empty at Constant Power */
	BQ27XXX_REG_NAC,	/* Nominal Available Capacity */
	BQ27XXX_REG_FCC,	/* Full Charge Capacity */
	BQ27XXX_REG_CYCT,	/* Cycle Count */
	BQ27XXX_REG_AE,		/* Available Energy */
	BQ27XXX_REG_SOC,	/* State-of-Charge */
	BQ27XXX_REG_DCAP,	/* Design Capacity */
	BQ27XXX_REG_AP,		/* Average Power */
	BQ27XXX_DM_CTRL,	/* Block Data Control */
	BQ27XXX_DM_CLASS,	/* Data Class */
	BQ27XXX_DM_BLOCK,	/* Data Block */
	BQ27XXX_DM_DATA,	/* Block Data */
	BQ27XXX_DM_CKSUM,	/* Block Data Checksum */
	BQ27XXX_REG_MAX,	/* sentinel */
};

#define BQ27XXX_DM_REG_ROWS \
	[BQ27XXX_DM_CTRL] = 0x61,  \
	[BQ27XXX_DM_CLASS] = 0x3e, \
	[BQ27XXX_DM_BLOCK] = 0x3f, \
	[BQ27XXX_DM_DATA] = 0x40,  \
	[BQ27XXX_DM_CKSUM] = 0x60

/* Register mappings */
static u8
	bq27000_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = 0x1c,
		[BQ27XXX_REG_TTECP] = 0x26,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = 0x22,
		[BQ27XXX_REG_SOC] = 0x0b,
		[BQ27XXX_REG_DCAP] = 0x76,
		[BQ27XXX_REG_AP] = 0x24,
		[BQ27XXX_DM_CTRL] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CLASS] = INVALID_REG_ADDR,
		[BQ27XXX_DM_BLOCK] = INVALID_REG_ADDR,
		[BQ27XXX_DM_DATA] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CKSUM] = INVALID_REG_ADDR,
	},
	bq27010_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = 0x1c,
		[BQ27XXX_REG_TTECP] = 0x26,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x0b,
		[BQ27XXX_REG_DCAP] = 0x76,
		[BQ27XXX_REG_AP] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CTRL] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CLASS] = INVALID_REG_ADDR,
		[BQ27XXX_DM_BLOCK] = INVALID_REG_ADDR,
		[BQ27XXX_DM_DATA] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CKSUM] = INVALID_REG_ADDR,
	},
	bq2750x_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x28,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = 0x1a,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = INVALID_REG_ADDR,
		BQ27XXX_DM_REG_ROWS,
	},
#define bq2751x_regs bq27510g3_regs
#define bq2752x_regs bq27510g3_regs
	bq27500_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = 0x1c,
		[BQ27XXX_REG_TTECP] = 0x26,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = 0x22,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x24,
		BQ27XXX_DM_REG_ROWS,
	},
#define bq27510g1_regs bq27500_regs
#define bq27510g2_regs bq27500_regs
	bq27510g3_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x28,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = 0x1a,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x1e,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x20,
		[BQ27XXX_REG_DCAP] = 0x2e,
		[BQ27XXX_REG_AP] = INVALID_REG_ADDR,
		BQ27XXX_DM_REG_ROWS,
	},
	bq27520g1_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = 0x1c,
		[BQ27XXX_REG_TTECP] = 0x26,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = INVALID_REG_ADDR,
		[BQ27XXX_REG_AE] = 0x22,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x24,
		BQ27XXX_DM_REG_ROWS,
	},
	bq27520g2_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x36,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = 0x1c,
		[BQ27XXX_REG_TTECP] = 0x26,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = 0x22,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x24,
		BQ27XXX_DM_REG_ROWS,
	},
	bq27520g3_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x36,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = 0x1c,
		[BQ27XXX_REG_TTECP] = 0x26,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = 0x22,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x24,
		BQ27XXX_DM_REG_ROWS,
	},
	bq27520g4_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x28,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = 0x1c,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x1e,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x20,
		[BQ27XXX_REG_DCAP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_AP] = INVALID_REG_ADDR,
		BQ27XXX_DM_REG_ROWS,
	},
	bq27521_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x02,
		[BQ27XXX_REG_TEMP] = 0x0a,
		[BQ27XXX_REG_INT_TEMP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_VOLT] = 0x0c,
		[BQ27XXX_REG_AI] = 0x0e,
		[BQ27XXX_REG_FLAGS] = 0x08,
		[BQ27XXX_REG_TTE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = INVALID_REG_ADDR,
		[BQ27XXX_REG_FCC] = INVALID_REG_ADDR,
		[BQ27XXX_REG_CYCT] = INVALID_REG_ADDR,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = INVALID_REG_ADDR,
		[BQ27XXX_REG_DCAP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_AP] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CTRL] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CLASS] = INVALID_REG_ADDR,
		[BQ27XXX_DM_BLOCK] = INVALID_REG_ADDR,
		[BQ27XXX_DM_DATA] = INVALID_REG_ADDR,
		[BQ27XXX_DM_CKSUM] = INVALID_REG_ADDR,
	},
	bq27530_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x32,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_AP] = 0x24,
		BQ27XXX_DM_REG_ROWS,
	},
#define bq27531_regs bq27530_regs
	bq27541_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x28,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x24,
		BQ27XXX_DM_REG_ROWS,
	},
#define bq27542_regs bq27541_regs
#define bq27546_regs bq27541_regs
#define bq27742_regs bq27541_regs
	bq27545_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = 0x28,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_AP] = 0x24,
		BQ27XXX_DM_REG_ROWS,
	},
	bq27421_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x02,
		[BQ27XXX_REG_INT_TEMP] = 0x1e,
		[BQ27XXX_REG_VOLT] = 0x04,
		[BQ27XXX_REG_AI] = 0x10,
		[BQ27XXX_REG_FLAGS] = 0x06,
		[BQ27XXX_REG_TTE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTF] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x08,
		[BQ27XXX_REG_FCC] = 0x0e,
		[BQ27XXX_REG_CYCT] = INVALID_REG_ADDR,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x1c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x18,
		BQ27XXX_DM_REG_ROWS,
	},
#define bq27411_regs bq27421_regs
#define bq27425_regs bq27421_regs
#define bq27426_regs bq27421_regs
#define bq27441_regs bq27421_regs
#define bq27621_regs bq27421_regs
	bq27z561_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = INVALID_REG_ADDR,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = 0x22,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x22,
		BQ27XXX_DM_REG_ROWS,
	},
	bq28z610_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x06,
		[BQ27XXX_REG_INT_TEMP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x14,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = INVALID_REG_ADDR,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = 0x22,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x22,
		BQ27XXX_DM_REG_ROWS,
	},
	bq34z100_regs[BQ27XXX_REG_MAX] = {
		[BQ27XXX_REG_CTRL] = 0x00,
		[BQ27XXX_REG_TEMP] = 0x0c,
		[BQ27XXX_REG_INT_TEMP] = 0x2a,
		[BQ27XXX_REG_VOLT] = 0x08,
		[BQ27XXX_REG_AI] = 0x0a,
		[BQ27XXX_REG_FLAGS] = 0x0e,
		[BQ27XXX_REG_TTE] = 0x18,
		[BQ27XXX_REG_TTF] = 0x1a,
		[BQ27XXX_REG_TTES] = 0x1e,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = INVALID_REG_ADDR,
		[BQ27XXX_REG_FCC] = 0x06,
		[BQ27XXX_REG_CYCT] = 0x2c,
		[BQ27XXX_REG_AE] = 0x24,
		[BQ27XXX_REG_SOC] = 0x02,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x22,
		BQ27XXX_DM_REG_ROWS,
	};

static enum power_supply_property bq27000_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_property bq27010_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

#define bq2750x_props bq27510g3_props
#define bq2751x_props bq27510g3_props
#define bq2752x_props bq27510g3_props

static enum power_supply_property bq27500_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};
#define bq27510g1_props bq27500_props
#define bq27510g2_props bq27500_props

static enum power_supply_property bq27510g3_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_property bq27520g1_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

#define bq27520g2_props bq27500_props

static enum power_supply_property bq27520g3_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_property bq27520g4_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_property bq27521_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property bq27530_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_MANUFACTURER,
};
#define bq27531_props bq27530_props

static enum power_supply_property bq27541_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};
#define bq27542_props bq27541_props
#define bq27546_props bq27541_props
#define bq27742_props bq27541_props

static enum power_supply_property bq27545_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_property bq27421_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_MANUFACTURER,
};
#define bq27411_props bq27421_props
#define bq27425_props bq27421_props
#define bq27426_props bq27421_props
#define bq27441_props bq27421_props
#define bq27621_props bq27421_props

static enum power_supply_property bq27z561_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_property bq28z610_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static enum power_supply_property bq34z100_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

struct bq27xxx_dm_reg {
	u8 subclass_id;
	u8 offset;
	u8 bytes;
	u16 min, max;
};

enum bq27xxx_dm_reg_id {
	BQ27XXX_DM_DESIGN_CAPACITY = 0,
	BQ27XXX_DM_DESIGN_ENERGY,
	BQ27XXX_DM_TERMINATE_VOLTAGE,
#ifdef CONFIG_BATTERY_BQ27XXX_RESIST_TABLE_UPDATES_NVM
	BQ27XXX_DM_TAPER_RATE,
	BQ27XXX_DM_QMAX,
	BQ27XXX_RAM_R_a0_0,
	BQ27XXX_RAM_R_a0_1,
	BQ27XXX_RAM_R_a0_2,
	BQ27XXX_RAM_R_a0_3,
	BQ27XXX_RAM_R_a0_4,
	BQ27XXX_RAM_R_a0_5,
	BQ27XXX_RAM_R_a0_6,
	BQ27XXX_RAM_R_a0_7,
	BQ27XXX_RAM_R_a0_8,
	BQ27XXX_RAM_R_a0_9,
	BQ27XXX_RAM_R_a0_10,
	BQ27XXX_RAM_R_a0_11,
	BQ27XXX_RAM_R_a0_12,
	BQ27XXX_RAM_R_a0_13,
	BQ27XXX_RAM_R_a0_14,
#endif
};

#define bq27000_dm_regs 0
#define bq27010_dm_regs 0
#define bq2750x_dm_regs 0
#define bq2751x_dm_regs 0
#define bq2752x_dm_regs 0

#if 0 /* not yet tested */
static struct bq27xxx_dm_reg bq27500_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 48, 10, 2,    0, 65535 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { }, /* missing on chip */
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 80, 48, 2, 1000, 32767 },
};
#else
#define bq27500_dm_regs 0
#endif

/* todo create data memory definitions from datasheets and test on chips */
#define bq27510g1_dm_regs 0
#define bq27510g2_dm_regs 0
#define bq27510g3_dm_regs 0
#define bq27520g1_dm_regs 0
#define bq27520g2_dm_regs 0
#define bq27520g3_dm_regs 0
#define bq27520g4_dm_regs 0
#define bq27521_dm_regs 0
#define bq27530_dm_regs 0
#define bq27531_dm_regs 0
#define bq27541_dm_regs 0
#define bq27542_dm_regs 0
#define bq27546_dm_regs 0
#define bq27742_dm_regs 0

#if 0 /* not yet tested */
static struct bq27xxx_dm_reg bq27545_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 48, 23, 2,    0, 32767 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 48, 25, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 80, 67, 2, 2800,  3700 },
};
#else
#define bq27545_dm_regs 0
#endif

static struct bq27xxx_dm_reg bq27411_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 82, 10, 2,    0, 32767 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 82, 12, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 82, 16, 2, 2800,  3700 },
};

static struct bq27xxx_dm_reg bq27421_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 82, 10, 2,    0,  8000 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 82, 12, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 82, 16, 2, 2500,  3700 },
#ifdef CONFIG_BATTERY_BQ27XXX_RESIST_TABLE_UPDATES_NVM
	[BQ27XXX_DM_TAPER_RATE]        = { 82, 27, 2,    0,  2000 }, /* Taper rate */
	[BQ27XXX_DM_QMAX]              = { 82,  0, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_0]           = { 89,  0, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_1]           = { 89,  2, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_2]           = { 89,  4, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_3]           = { 89,  6, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_4]           = { 89,  8, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_5]           = { 89, 10, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_6]           = { 89, 12, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_7]           = { 89, 14, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_8]           = { 89, 16, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_9]           = { 89, 18, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_10]          = { 89, 20, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_11]          = { 89, 22, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_12]          = { 89, 24, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_13]          = { 89, 26, 2,    0, 32767 },
	[BQ27XXX_RAM_R_a0_14]          = { 89, 28, 2,    0, 32767 },
#endif
};

static struct bq27xxx_dm_reg bq27425_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 82, 12, 2,    0, 32767 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 82, 14, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 82, 18, 2, 2800,  3700 },
};

static struct bq27xxx_dm_reg bq27426_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 82,  6, 2,    0,  8000 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 82,  8, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 82, 10, 2, 2500,  3700 },
};

#if 0 /* not yet tested */
#define bq27441_dm_regs bq27421_dm_regs
#else
#define bq27441_dm_regs 0
#endif

#if 0 /* not yet tested */
static struct bq27xxx_dm_reg bq27621_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 82, 3, 2,    0,  8000 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 82, 5, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 82, 9, 2, 2500,  3700 },
};
#else
#define bq27621_dm_regs 0
#endif

#define bq27z561_dm_regs 0
#define bq28z610_dm_regs 0
#define bq34z100_dm_regs 0

#define BQ27XXX_O_ZERO		BIT(0)
#define BQ27XXX_O_OTDC		BIT(1) /* has OTC/OTD overtemperature flags */
#define BQ27XXX_O_UTOT		BIT(2) /* has OT overtemperature flag */
#define BQ27XXX_O_CFGUP		BIT(3)
#define BQ27XXX_O_RAM		BIT(4)
#define BQ27Z561_O_BITS		BIT(5)
#define BQ27XXX_O_SOC_SI	BIT(6) /* SoC is single register */
#define BQ27XXX_O_HAS_CI	BIT(7) /* has Capacity Inaccurate flag */
#define BQ27XXX_O_MUL_CHEM	BIT(8) /* multiple chemistries supported */

#define BQ27XXX_DATA(ref, key, opt) {		\
	.opts = (opt),				\
	.unseal_key = key,			\
	.regs  = ref##_regs,			\
	.dm_regs = ref##_dm_regs,		\
	.props = ref##_props,			\
	.props_size = ARRAY_SIZE(ref##_props) }

static struct {
	u32 opts;
	u32 unseal_key;
	u8 *regs;
	struct bq27xxx_dm_reg *dm_regs;
	enum power_supply_property *props;
	size_t props_size;
} bq27xxx_chip_data[] = {
	[BQ27000]   = BQ27XXX_DATA(bq27000,   0         , BQ27XXX_O_ZERO | BQ27XXX_O_SOC_SI | BQ27XXX_O_HAS_CI),
	[BQ27010]   = BQ27XXX_DATA(bq27010,   0         , BQ27XXX_O_ZERO | BQ27XXX_O_SOC_SI | BQ27XXX_O_HAS_CI),
	[BQ2750X]   = BQ27XXX_DATA(bq2750x,   0         , BQ27XXX_O_OTDC),
	[BQ2751X]   = BQ27XXX_DATA(bq2751x,   0         , BQ27XXX_O_OTDC),
	[BQ2752X]   = BQ27XXX_DATA(bq2752x,   0         , BQ27XXX_O_OTDC),
	[BQ27500]   = BQ27XXX_DATA(bq27500,   0x04143672, BQ27XXX_O_OTDC),
	[BQ27510G1] = BQ27XXX_DATA(bq27510g1, 0         , BQ27XXX_O_OTDC),
	[BQ27510G2] = BQ27XXX_DATA(bq27510g2, 0         , BQ27XXX_O_OTDC),
	[BQ27510G3] = BQ27XXX_DATA(bq27510g3, 0         , BQ27XXX_O_OTDC),
	[BQ27520G1] = BQ27XXX_DATA(bq27520g1, 0         , BQ27XXX_O_OTDC),
	[BQ27520G2] = BQ27XXX_DATA(bq27520g2, 0         , BQ27XXX_O_OTDC),
	[BQ27520G3] = BQ27XXX_DATA(bq27520g3, 0         , BQ27XXX_O_OTDC),
	[BQ27520G4] = BQ27XXX_DATA(bq27520g4, 0         , BQ27XXX_O_OTDC),
	[BQ27521]   = BQ27XXX_DATA(bq27521,   0         , 0),
	[BQ27530]   = BQ27XXX_DATA(bq27530,   0         , BQ27XXX_O_UTOT),
	[BQ27531]   = BQ27XXX_DATA(bq27531,   0         , BQ27XXX_O_UTOT),
	[BQ27541]   = BQ27XXX_DATA(bq27541,   0         , BQ27XXX_O_OTDC),
	[BQ27542]   = BQ27XXX_DATA(bq27542,   0         , BQ27XXX_O_OTDC),
	[BQ27546]   = BQ27XXX_DATA(bq27546,   0         , BQ27XXX_O_OTDC),
	[BQ27742]   = BQ27XXX_DATA(bq27742,   0         , BQ27XXX_O_OTDC),
	[BQ27545]   = BQ27XXX_DATA(bq27545,   0x04143672, BQ27XXX_O_OTDC),
	[BQ27411]   = BQ27XXX_DATA(bq27411,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27421]   = BQ27XXX_DATA(bq27421,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27425]   = BQ27XXX_DATA(bq27425,   0x04143672, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP),
	[BQ27426]   = BQ27XXX_DATA(bq27426,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27441]   = BQ27XXX_DATA(bq27441,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27621]   = BQ27XXX_DATA(bq27621,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27Z561]  = BQ27XXX_DATA(bq27z561,  0         , BQ27Z561_O_BITS),
	[BQ28Z610]  = BQ27XXX_DATA(bq28z610,  0         , BQ27Z561_O_BITS),
	[BQ34Z100]  = BQ27XXX_DATA(bq34z100,  0         , BQ27XXX_O_OTDC | BQ27XXX_O_SOC_SI | \
							  BQ27XXX_O_HAS_CI | BQ27XXX_O_MUL_CHEM),
};

static DEFINE_MUTEX(bq27xxx_list_lock);
static LIST_HEAD(bq27xxx_battery_devices);

#define BQ27XXX_MSLEEP(i) usleep_range((i)*1000, (i)*1000+500)

#define BQ27XXX_DM_SZ	32

/**
 * struct bq27xxx_dm_buf - chip data memory buffer
 * @class: data memory subclass_id
 * @block: data memory block number
 * @data: data from/for the block
 * @has_data: true if data has been filled by read
 * @dirty: true if data has changed since last read/write
 *
 * Encapsulates info required to manage chip data memory blocks.
 */
struct bq27xxx_dm_buf {
	u8 class;
	u8 block;
	u8 data[BQ27XXX_DM_SZ];
	bool has_data, dirty, sum_command;
};

#define BQ27XXX_DM_BUF(di, i) { \
	.class = (di)->dm_regs[i].subclass_id, \
	.block = (di)->dm_regs[i].offset / BQ27XXX_DM_SZ, \
}

static inline u16 *bq27xxx_dm_reg_ptr(struct bq27xxx_dm_buf *buf,
				      struct bq27xxx_dm_reg *reg)
{
	if (buf->class == reg->subclass_id &&
	    buf->block == reg->offset / BQ27XXX_DM_SZ)
		return (u16 *) (buf->data + reg->offset % BQ27XXX_DM_SZ);

	return NULL;
}

static const char * const bq27xxx_dm_reg_name[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY] = "design-capacity",
	[BQ27XXX_DM_DESIGN_ENERGY] = "design-energy",
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = "terminate-voltage",
#ifdef CONFIG_BATTERY_BQ27XXX_RESIST_TABLE_UPDATES_NVM
	[BQ27XXX_DM_TAPER_RATE] = "Taper-rate",
	[BQ27XXX_DM_QMAX]       = "QMAX-Cell",
	[BQ27XXX_RAM_R_a0_0]    = "R_a0_0",
	[BQ27XXX_RAM_R_a0_1]    = "R_a0_1",
	[BQ27XXX_RAM_R_a0_2]    = "R_a0_2",
	[BQ27XXX_RAM_R_a0_3]    = "R_a0_3",
	[BQ27XXX_RAM_R_a0_4]    = "R_a0_4",
	[BQ27XXX_RAM_R_a0_5]    = "R_a0_5",
	[BQ27XXX_RAM_R_a0_6]    = "R_a0_6",
	[BQ27XXX_RAM_R_a0_7]    = "R_a0_7",
	[BQ27XXX_RAM_R_a0_8]    = "R_a0_8",
	[BQ27XXX_RAM_R_a0_9]    = "R_a0_9",
	[BQ27XXX_RAM_R_a0_10]   = "R_a0_10",
	[BQ27XXX_RAM_R_a0_11]   = "R_a0_11",
	[BQ27XXX_RAM_R_a0_12]   = "R_a0_12",
	[BQ27XXX_RAM_R_a0_13]   = "R_a0_13",
	[BQ27XXX_RAM_R_a0_14]   = "R_a0_14",
#endif

};


static bool bq27xxx_dt_to_nvm;
module_param_named(dt_monitored_battery_updates_nvm, bq27xxx_dt_to_nvm, bool, 0444);
MODULE_PARM_DESC(dt_monitored_battery_updates_nvm,
	"Devicetree monitored-battery config updates data memory on NVM/flash chips.\n"
	"Users must set this =0 when installing a different type of battery!\n"
	"Default is =1."
#ifndef CONFIG_BATTERY_BQ27XXX_DT_UPDATES_NVM
	"\nSetting this affects future kernel updates, not the current configuration."
#endif
);

/*
 * id - file identifier
 *
 * These are ids to identify which file is requested
 */
enum {
	ADDRESS = 0,
	DATA,
	MAC_CMD,
	MAC_DATA,
	AT_RATE,
	AT_RATE_TIME_TO_EMPTY,
	DEVICE_TYPE,
	FIRMWARE_VERSION,
	HARDWARE_VERSION,
	INSTRUCTION_FLASH_SIGNATURE,
	STATIC_DF_SIGNATURE,
	CHEMICAL_ID,
	STATIC_CHEM_DF_SIGNATURE,
	ALL_DF_SIGNATURE,
	DEVICE_RESET,
	LIFETIME_DATA_COLLECTION,
	LIFETIME_DATA_RESET,
	SEAL_DEVICE,
	DEVICE_NAME,
	DEVICE_CHEM,
	MANUFACTURER_NAME,
	MANUFACTURER_DATE,
	SERIAL_NUMBER,
	OPERATION_STATUS,
	CHARGING_STATUS,
	GAUGING_STATUS,
	MANUFACTURING_STATUS,
	LIFETIME_DATA_BLOCK,
	LIFETIME1_LOWER,
	LIFETIME1_HIGHER,
	LIFETIME3,
	LIFETIME4,
	TEMP_ZONES,
	SOH,
	INTER_TEMP,
	REMAINING_CAPACITY,
	MFG_INFO_A,
	MFG_INFO_B,
	MFG_INFO_C,
};

/**
 * struct bq27xxx_attribute - sysfs attribute
 * @dattr: device attribute
 * @id: file id
 * @val1: data value depending on id
 * @val2: data value depending on id
 */
struct bq27xxx_attribute {
	struct device_attribute dattr;
	u32 id;
	u32 val1;
	u32 val2;
};

#define BQ27XXX_ATTR(_field, _mode, _show, _store, _id, _val1, _val2)	\
	struct bq27xxx_attribute bq27xxx_attr_##_field = {		\
		.dattr = __ATTR(_field, _mode,				\
				_show, _store),				\
		.id = _id,						\
		.val1 = _val1,						\
		.val2 = _val2,						\
	}

static int poll_interval_param_set(const char *val, const struct kernel_param *kp)
{
	struct bq27xxx_device_info *di;
	unsigned int prev_val = *(unsigned int *) kp->arg;
	int ret;

	ret = param_set_uint(val, kp);
	if (ret < 0 || prev_val == *(unsigned int *) kp->arg)
		return ret;

	mutex_lock(&bq27xxx_list_lock);
	list_for_each_entry(di, &bq27xxx_battery_devices, list)
		mod_delayed_work(system_wq, &di->work, 0);
	mutex_unlock(&bq27xxx_list_lock);

	return ret;
}

static const struct kernel_param_ops param_ops_poll_interval = {
	.get = param_get_uint,
	.set = poll_interval_param_set,
};

static unsigned int poll_interval = 360;
module_param_cb(poll_interval, &param_ops_poll_interval, &poll_interval, 0644);
MODULE_PARM_DESC(poll_interval,
		 "battery poll interval in seconds - 0 disables polling");

/*
 * Common code for BQ27xxx devices
 */

static inline int bq27xxx_read(struct bq27xxx_device_info *di, int reg_index,
			       bool single)
{
	int ret;

	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;

	ret = di->bus.read(di, di->regs[reg_index], single);
	if (ret < 0)
		dev_dbg(di->dev, "failed to read register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static inline int bq27xxx_write(struct bq27xxx_device_info *di, int reg_index,
				u16 value, bool single)
{
	int ret;

	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;

	if (!di->bus.write)
		return -EPERM;

	ret = di->bus.write(di, di->regs[reg_index], value, single);
	if (ret < 0)
		dev_dbg(di->dev, "failed to write register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static inline int bq27xxx_read_block(struct bq27xxx_device_info *di, int reg_index,
				     u8 *data, int len)
{
	int ret;

	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;

	if (!di->bus.read_bulk)
		return -EPERM;

	ret = di->bus.read_bulk(di, di->regs[reg_index], data, len);
	if (ret < 0)
		dev_dbg(di->dev, "failed to read_bulk register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static inline int bq27xxx_write_block(struct bq27xxx_device_info *di, int reg_index,
				      u8 *data, int len)
{
	int ret;

	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -EINVAL;

	if (!di->bus.write_bulk)
		return -EPERM;

	ret = di->bus.write_bulk(di, di->regs[reg_index], data, len);
	if (ret < 0)
		dev_dbg(di->dev, "failed to write_bulk register 0x%02x (index %d)\n",
			di->regs[reg_index], reg_index);

	return ret;
}

static inline int bq27z561_read(struct bq27xxx_device_info *di, u8 reg_addr,
			       bool single)
{
	int ret;

	ret = di->bus.read(di, reg_addr, single);
	if (ret < 0)
		dev_err(di->dev, "failed to read reg 0x%02X\n", reg_addr);

	return ret;
}

static inline int bq27z561_write(struct bq27xxx_device_info *di, u8 reg_addr,
			       u16 value, bool single)
{
	int ret;

	if (!di->bus.write)
		return -EPERM;

	ret = di->bus.write(di, reg_addr, value, single);
	if (ret < 0)
		dev_err(di->dev, "failed to write reg 0x%02X\n", reg_addr);

	return ret;
}

// TODO: why can't this be bq27xxxx_read_block?
static inline int bq27z561_read_block(struct bq27xxx_device_info *di,
				u8 reg_addr, u8 *data, int len)
{
	int ret;

	if (!di->bus.read_bulk)
		return -EPERM;
	ret = di->bus.read_bulk(di, reg_addr, data, len);
	if (ret < 0)
		dev_dbg(di->dev, "failed to read_bulk register 0x%02X\n",
			reg_addr);

	return ret;
}

static inline int bq27z561_write_block(struct bq27xxx_device_info *di,
				u8 reg_addr, u8 *data, int len)
{
	int ret;

	if (!di->bus.write_bulk)
		return -EPERM;

	ret = di->bus.write_bulk(di, reg_addr, data, len);
	if (ret < 0)
		dev_dbg(di->dev, "failed to write_bulk register 0x%02x\n",
			reg_addr);

	return ret;
}

static int bq27xxx_battery_seal(struct bq27xxx_device_info *di)
{
	int ret;

	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, BQ27XXX_SEALED, false);
	if (ret < 0) {
		dev_err(di->dev, "bus error on seal: %d\n", ret);
		return ret;
	}

	return 0;
}

static int bq27xxx_battery_unseal(struct bq27xxx_device_info *di)
{
	int ret;

	if (di->unseal_key == 0) {
		dev_err(di->dev, "unseal failed due to missing key\n");
		return -EINVAL;
	}

	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, (u16)(di->unseal_key >> 16), false);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, (u16)di->unseal_key, false);
	if (ret < 0)
		goto out;

	return 0;

out:
	dev_err(di->dev, "bus error on unseal: %d\n", ret);
	return ret;
}

static u8 bq27xxx_battery_checksum_dm_block(struct bq27xxx_dm_buf *buf)
{
	u16 sum = 0;
	int i;

	for (i = 0; i < BQ27XXX_DM_SZ; i++)
		sum += buf->data[i];

	if (buf->sum_command)
		sum += buf->class + buf->block;

	sum &= 0xff;

	return 0xff - sum;
}

static int bq27xxx_battery_read_dm_block(struct bq27xxx_device_info *di,
					 struct bq27xxx_dm_buf *buf)
{
	int ret;

	buf->has_data = false;

	ret = bq27xxx_write(di, BQ27XXX_DM_CLASS, buf->class, true);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_BLOCK, buf->block, true);
	if (ret < 0)
		goto out;

	BQ27XXX_MSLEEP(1);

	ret = bq27xxx_read_block(di, BQ27XXX_DM_DATA, buf->data, BQ27XXX_DM_SZ);
	if (ret < 0)
		goto out;

	ret = bq27xxx_read(di, BQ27XXX_DM_CKSUM, true);
	if (ret < 0)
		goto out;

	if ((u8)ret != bq27xxx_battery_checksum_dm_block(buf)) {
		ret = -EINVAL;
		goto out;
	}

	buf->has_data = true;
	buf->dirty = false;

	return 0;

out:
	dev_err(di->dev, "bus error reading chip memory: %d\n", ret);
	return ret;
}

static void bq27xxx_update_lifetime(struct bq27xxx_device_info *di)
{
	int ret, i;
	struct bq27xxx_dm_buf dm_buf = {
		.block = 0,
		.sum_command = true,
	};

	/* Lifetime1 */
	dm_buf.class = BQ27XXX_MAC_LDB1;
	memset(dm_buf.data, '\0', BQ27XXX_DM_SZ);
	ret = bq27xxx_battery_read_dm_block(di, &dm_buf);
	if (ret == 0) {
		memcpy(di->lifetime_blocks.lifetime1_lower, dm_buf.data,
			sizeof(u16) * BQ27XXX_LIFETIME_1_LOWER_LEN);
		memcpy(di->lifetime_blocks.lifetime1_higher, dm_buf.data
			+ sizeof(u16) * BQ27XXX_LIFETIME_1_LOWER_LEN,
			sizeof(u8) * BQ27XXX_LIFETIME_1_HIGHER_LEN);
	}

	/* Lifetime3 */
	dm_buf.class = BQ27XXX_MAC_LDB3;
	memset(dm_buf.data, '\0', BQ27XXX_DM_SZ);
	ret = bq27xxx_battery_read_dm_block(di, &dm_buf);
	if (ret == 0)
		memcpy(&di->lifetime_blocks.lifetime3, dm_buf.data,
				sizeof(u32));

	/* Lifetime4 */
	dm_buf.class = BQ27XXX_MAC_LDB4;
	memset(dm_buf.data, '\0', BQ27XXX_DM_SZ);
	ret = bq27xxx_battery_read_dm_block(di, &dm_buf);
	if (ret == 0)
		memcpy(di->lifetime_blocks.lifetime4, dm_buf.data,
				sizeof(u16) * BQ27XXX_LIFETIME_4_LEN);

	/* Lifetime temperature zones */
	dm_buf.class = BQ27XXX_MAC_LDB6;
	for (i = 0; i < BQ27XXX_NUM_TEMP_ZONE; i++) {
		memset(dm_buf.data, '\0', BQ27XXX_DM_SZ);

		ret = bq27xxx_battery_read_dm_block(di, &dm_buf);
		if (ret != 0)
			continue;

		memcpy(di->lifetime_blocks.temp_zones[i], dm_buf.data,
				sizeof(u32) * BQ27XXX_TEMP_ZONE_LEN);

		dm_buf.class++;
	}
}

static void bq27xxx_battery_update_dm_block(struct bq27xxx_device_info *di,
					    struct bq27xxx_dm_buf *buf,
					    enum bq27xxx_dm_reg_id reg_id,
					    unsigned int val)
{
	struct bq27xxx_dm_reg *reg = &di->dm_regs[reg_id];
	const char *str = bq27xxx_dm_reg_name[reg_id];
	u16 *prev = bq27xxx_dm_reg_ptr(buf, reg);

	if (prev == NULL) {
		dev_warn(di->dev, "buffer does not match %s dm spec\n", str);
		return;
	}

	if (reg->bytes != 2) {
		dev_warn(di->dev, "%s dm spec has unsupported byte size\n", str);
		return;
	}

	if (!buf->has_data)
		return;

	if (be16_to_cpup(prev) == val) {
		dev_info(di->dev, "%s has %u\n", str, val);
		return;
	}

#ifdef CONFIG_BATTERY_BQ27XXX_DT_UPDATES_NVM
	if (!(di->opts & BQ27XXX_O_RAM) && !bq27xxx_dt_to_nvm) {
#else
	if (!(di->opts & BQ27XXX_O_RAM)) {
#endif
		/* devicetree and NVM differ; defer to NVM */
		dev_warn(di->dev, "%s has %u; update to %u disallowed "
#ifdef CONFIG_BATTERY_BQ27XXX_DT_UPDATES_NVM
			 "by dt_monitored_battery_updates_nvm=0"
#else
			 "for flash/NVM data memory"
#endif
			 "\n", str, be16_to_cpup(prev), val);
		return;
	}

	dev_info(di->dev, "update %s to %u\n", str, val);

	*prev = cpu_to_be16(val);
	buf->dirty = true;
}

static int bq27xxx_battery_cfgupdate_priv(struct bq27xxx_device_info *di, bool active)
{
	const int limit = 100;
	u16 cmd = active ? BQ27XXX_SET_CFGUPDATE : BQ27XXX_SOFT_RESET;
	int ret, try = limit;

	ret = bq27xxx_write(di, BQ27XXX_REG_CTRL, cmd, false);
	if (ret < 0)
		return ret;

	do {
		BQ27XXX_MSLEEP(25);
		ret = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
		if (ret < 0)
			return ret;
	} while (!!(ret & BQ27XXX_FLAG_CFGUP) != active && --try);

	if (!try && di->chip != BQ27425) { // 425 has a bug
		dev_err(di->dev, "timed out waiting for cfgupdate flag %d\n", active);
		return -EINVAL;
	}

	if (limit - try > 3)
		dev_warn(di->dev, "cfgupdate %d, retries %d\n", active, limit - try);

	return 0;
}

static inline int bq27xxx_battery_set_cfgupdate(struct bq27xxx_device_info *di)
{
	int ret = bq27xxx_battery_cfgupdate_priv(di, true);
	if (ret < 0 && ret != -EINVAL)
		dev_err(di->dev, "bus error on set_cfgupdate: %d\n", ret);

	return ret;
}

static inline int bq27xxx_battery_soft_reset(struct bq27xxx_device_info *di)
{
	int ret = bq27xxx_battery_cfgupdate_priv(di, false);
	if (ret < 0 && ret != -EINVAL)
		dev_err(di->dev, "bus error on soft_reset: %d\n", ret);

	return ret;
}

static int bq27xxx_battery_write_dm_block(struct bq27xxx_device_info *di,
					  struct bq27xxx_dm_buf *buf)
{
	bool cfgup = di->opts & BQ27XXX_O_CFGUP;
	int ret;

	if (!buf->dirty)
		return 0;

	if (cfgup) {
		ret = bq27xxx_battery_set_cfgupdate(di);
		if (ret < 0)
			return ret;
	}

	ret = bq27xxx_write(di, BQ27XXX_DM_CTRL, 0, true);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_CLASS, buf->class, true);
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_BLOCK, buf->block, true);
	if (ret < 0)
		goto out;

	BQ27XXX_MSLEEP(1);

	ret = bq27xxx_write_block(di, BQ27XXX_DM_DATA, buf->data,
				  (BQ27XXX_DM_SZ-1));
	if (ret < 0)
		goto out;

	ret = bq27xxx_write(di, BQ27XXX_DM_CKSUM,
			    bq27xxx_battery_checksum_dm_block(buf), true);
	if (ret < 0)
		goto out;

	/* DO NOT read BQ27XXX_DM_CKSUM here to verify it! That may cause NVM
	 * corruption on the '425 chip (and perhaps others), which can damage
	 * the chip.
	 */

	if (cfgup) {
		BQ27XXX_MSLEEP(1);
		ret = bq27xxx_battery_soft_reset(di);
		if (ret < 0)
			return ret;
	} else {
		BQ27XXX_MSLEEP(100); /* flash DM updates in <100ms */
	}

	buf->dirty = false;

	return 0;

out:
	if (cfgup)
		bq27xxx_battery_soft_reset(di);

	dev_err(di->dev, "bus error writing chip memory: %d\n", ret);
	return ret;
}

static void bq27xxx_battery_set_config(struct bq27xxx_device_info *di,
				       struct power_supply_battery_info *info)
{
	struct bq27xxx_dm_buf bd = BQ27XXX_DM_BUF(di, BQ27XXX_DM_DESIGN_CAPACITY);
	struct bq27xxx_dm_buf bt = BQ27XXX_DM_BUF(di, BQ27XXX_DM_TERMINATE_VOLTAGE);
	bool updated;
#ifdef CONFIG_BATTERY_BQ27XXX_RESIST_TABLE_UPDATES_NVM
	struct bq27xxx_dm_buf rt = BQ27XXX_DM_BUF(di, BQ27XXX_RAM_R_a0_0);
	u32 i, taper_rate;
#endif

	if (bq27xxx_battery_unseal(di) < 0)
		return;

	if (info->charge_full_design_uah != -EINVAL &&
	    info->energy_full_design_uwh != -EINVAL) {
		bq27xxx_battery_read_dm_block(di, &bd);

		/* assume design energy, taper_rate & capacity are in same block */
		bq27xxx_battery_update_dm_block(di, &bd,
					BQ27XXX_DM_DESIGN_CAPACITY,
					info->charge_full_design_uah / 1000);
		bq27xxx_battery_update_dm_block(di, &bd,
					BQ27XXX_DM_DESIGN_ENERGY,
					info->energy_full_design_uwh / 1000);

#ifdef CONFIG_BATTERY_BQ27XXX_RESIST_TABLE_UPDATES_NVM
		bq27xxx_battery_read_dm_block(di, &rt);
		/* update Taper rate based on the capacity and term current */
		taper_rate = (u32)((info->charge_full_design_uah * 10) /
				    info->charge_term_current_ua);
		bq27xxx_battery_update_dm_block(di, &bd, BQ27XXX_DM_TAPER_RATE,
						taper_rate);
		/* update the QMAX-CELL0 and resistance table */
		bq27xxx_battery_update_dm_block(di, &bd, BQ27XXX_DM_QMAX,
						 di->qmax_cell0);
		for (i = 0 ; i < 15; i++)
			bq27xxx_battery_update_dm_block(di, &rt,
							(i + BQ27XXX_RAM_R_a0_0),
							di->resist_table[i]);
#endif
	}

	if (info->voltage_min_design_uv != -EINVAL) {
		bool same = bd.class == bt.class && bd.block == bt.block;
		if (!same)
			bq27xxx_battery_read_dm_block(di, &bt);
		bq27xxx_battery_update_dm_block(di, same ? &bd : &bt,
					BQ27XXX_DM_TERMINATE_VOLTAGE,
					info->voltage_min_design_uv / 1000);
	}

	updated = bd.dirty || bt.dirty;

	bq27xxx_battery_write_dm_block(di, &bd);
	bq27xxx_battery_write_dm_block(di, &bt);

#ifdef CONFIG_BATTERY_BQ27XXX_RESIST_TABLE_UPDATES_NVM
	bq27xxx_battery_write_dm_block(di, &rt);

	bq27xxx_battery_read_dm_block(di, &bd);
	for (i = 0; i < BQ27XXX_DM_SZ; i++)
		dev_dbg(di->dev, "BQ27xxx: DM_NVM[%d]: 0x%04x\n", i, bd.data[i]);

	bq27xxx_battery_read_dm_block(di, &rt);
	for (i = 0; i < BQ27XXX_DM_SZ; i++)
		dev_dbg(di->dev, "BQ27xxx: Resisiatnce table DM_NVM[%d]:0x%04x\n",
			i, rt.data[i]);
#endif

	bq27xxx_battery_seal(di);

	if (updated && !(di->opts & BQ27XXX_O_CFGUP)) {
		bq27xxx_write(di, BQ27XXX_REG_CTRL, BQ27XXX_RESET, false);
		BQ27XXX_MSLEEP(300); /* reset time is not documented */
	}
	/* assume bq27xxx_battery_update() is called hereafter */
}

static void bq27xxx_battery_settings(struct bq27xxx_device_info *di)
{
	struct power_supply_battery_info info = {};
	unsigned int min, max;

	if (power_supply_get_battery_info(di->bat, &info) < 0)
		return;

	if (!di->dm_regs) {
		dev_warn(di->dev, "data memory update not supported for chip\n");
		return;
	}

	if (info.energy_full_design_uwh != info.charge_full_design_uah) {
		if (info.energy_full_design_uwh == -EINVAL)
			dev_warn(di->dev, "missing battery:energy-full-design-microwatt-hours\n");
		else if (info.charge_full_design_uah == -EINVAL)
			dev_warn(di->dev, "missing battery:charge-full-design-microamp-hours\n");
	}

	/* assume min == 0 */
	max = di->dm_regs[BQ27XXX_DM_DESIGN_ENERGY].max;
	if (info.energy_full_design_uwh > max * 1000) {
		dev_err(di->dev, "invalid battery:energy-full-design-microwatt-hours %d\n",
			info.energy_full_design_uwh);
		info.energy_full_design_uwh = -EINVAL;
	}

	/* assume min == 0 */
	max = di->dm_regs[BQ27XXX_DM_DESIGN_CAPACITY].max;
	if (info.charge_full_design_uah > max * 1000) {
		dev_err(di->dev, "invalid battery:charge-full-design-microamp-hours %d\n",
			info.charge_full_design_uah);
		info.charge_full_design_uah = -EINVAL;
	}

	min = di->dm_regs[BQ27XXX_DM_TERMINATE_VOLTAGE].min;
	max = di->dm_regs[BQ27XXX_DM_TERMINATE_VOLTAGE].max;
	if ((info.voltage_min_design_uv < min * 1000 ||
	     info.voltage_min_design_uv > max * 1000) &&
	     info.voltage_min_design_uv != -EINVAL) {
		dev_err(di->dev, "invalid battery:voltage-min-design-microvolt %d\n",
			info.voltage_min_design_uv);
		info.voltage_min_design_uv = -EINVAL;
	}

	if ((info.energy_full_design_uwh != -EINVAL &&
	     info.charge_full_design_uah != -EINVAL) ||
	     info.voltage_min_design_uv  != -EINVAL)
		bq27xxx_battery_set_config(di, &info);
}

/*
 * Return the battery State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_soc(struct bq27xxx_device_info *di)
{
	int soc;

	if (di->opts & BQ27XXX_O_SOC_SI)
		soc = bq27xxx_read(di, BQ27XXX_REG_SOC, true);
	else
		soc = bq27xxx_read(di, BQ27XXX_REG_SOC, false);

	if (soc < 0)
		dev_dbg(di->dev, "error reading State-of-Charge\n");

	return soc;
}

/*
 * Return a battery charge value in µAh
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_charge(struct bq27xxx_device_info *di, u8 reg)
{
	int charge;

	charge = bq27xxx_read(di, reg, false);
	if (charge < 0) {
		dev_dbg(di->dev, "error reading charge register %02x: %d\n",
			reg, charge);
		return charge;
	}

	if (di->opts & BQ27XXX_O_ZERO)
		charge *= BQ27XXX_CURRENT_CONSTANT / BQ27XXX_RS;
	else
		charge *= 1000;

	return charge;
}

/*
 * Return the battery Nominal available capacity in µAh
 * Or < 0 if something fails.
 */
static inline int bq27xxx_battery_read_nac(struct bq27xxx_device_info *di)
{
	return bq27xxx_battery_read_charge(di, BQ27XXX_REG_NAC);
}

/*
 * Return the battery Full Charge Capacity in µAh
 * Or < 0 if something fails.
 */
static inline int bq27xxx_battery_read_fcc(struct bq27xxx_device_info *di)
{
	return bq27xxx_battery_read_charge(di, BQ27XXX_REG_FCC);
}

/*
 * Return the Design Capacity in µAh
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_dcap(struct bq27xxx_device_info *di)
{
	int dcap;

	if (di->opts & BQ27XXX_O_ZERO)
		dcap = bq27xxx_read(di, BQ27XXX_REG_DCAP, true);
	else
		dcap = bq27xxx_read(di, BQ27XXX_REG_DCAP, false);

	if (dcap < 0) {
		dev_dbg(di->dev, "error reading initial last measured discharge\n");
		return dcap;
	}

	if (di->opts & BQ27XXX_O_ZERO)
		dcap = (dcap << 8) * BQ27XXX_CURRENT_CONSTANT / BQ27XXX_RS;
	else
		dcap *= 1000;

	return dcap;
}

/*
 * Return the battery Available energy in µWh
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_energy(struct bq27xxx_device_info *di)
{
	int ae;

	ae = bq27xxx_read(di, BQ27XXX_REG_AE, false);
	if (ae < 0) {
		dev_dbg(di->dev, "error reading available energy\n");
		return ae;
	}

	if (di->opts & BQ27XXX_O_ZERO)
		ae *= BQ27XXX_POWER_CONSTANT / BQ27XXX_RS;
	else
		ae *= 1000;

	return ae;
}

/*
 * Return the battery temperature in tenths of degree Kelvin
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_temperature(struct bq27xxx_device_info *di)
{
	int temp;

	temp = bq27xxx_read(di, BQ27XXX_REG_TEMP, false);
	if (temp < 0) {
		dev_err(di->dev, "error reading temperature\n");
		return temp;
	}

	if (di->opts & BQ27XXX_O_ZERO)
		temp = 5 * temp / 2;

	return temp;
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_read_cyct(struct bq27xxx_device_info *di)
{
	int cyct;

	cyct = bq27xxx_read(di, BQ27XXX_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");

	return cyct;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
static int bq27xxx_battery_read_time(struct bq27xxx_device_info *di, u8 reg)
{
	int tval;

	tval = bq27xxx_read(di, reg, false);
	if (tval < 0) {
		dev_dbg(di->dev, "error reading time register %02x: %d\n",
			reg, tval);
		return tval;
	}

	if (tval == 65535)
		return -ENODATA;

	return tval * 60;
}

/*
 * Returns true if a battery over temperature condition is detected
 */
static bool bq27xxx_battery_overtemp(struct bq27xxx_device_info *di, u16 flags)
{
	if (di->opts & BQ27XXX_O_OTDC)
		return flags & (BQ27XXX_FLAG_OTC | BQ27XXX_FLAG_OTD);
        if (di->opts & BQ27XXX_O_UTOT)
		return flags & BQ27XXX_FLAG_OT;

	return false;
}

/*
 * Returns true if a battery under temperature condition is detected
 */
static bool bq27xxx_battery_undertemp(struct bq27xxx_device_info *di, u16 flags)
{
	if (di->opts & BQ27XXX_O_UTOT)
		return flags & BQ27XXX_FLAG_UT;

	return false;
}

/*
 * Returns true if a low state of charge condition is detected
 */
static bool bq27xxx_battery_dead(struct bq27xxx_device_info *di, u16 flags)
{
	if (di->opts & BQ27XXX_O_ZERO)
		return flags & (BQ27000_FLAG_EDV1 | BQ27000_FLAG_EDVF);
	else if (di->opts & BQ27Z561_O_BITS)
		return flags & BQ27Z561_FLAG_FDC;
	else
		return flags & (BQ27XXX_FLAG_SOC1 | BQ27XXX_FLAG_SOCF);
}

/*
 * Returns true if reported battery capacity is inaccurate
 */
static bool bq27xxx_battery_capacity_inaccurate(struct bq27xxx_device_info *di,
						 u16 flags)
{
	if (di->opts & BQ27XXX_O_HAS_CI)
		return (flags & BQ27000_FLAG_CI);
	else
		return false;
}

static int bq27xxx_battery_read_health(struct bq27xxx_device_info *di)
{
	/* Unlikely but important to return first */
	if (unlikely(bq27xxx_battery_overtemp(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (unlikely(bq27xxx_battery_undertemp(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_COLD;
	if (unlikely(bq27xxx_battery_dead(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_DEAD;
	if (unlikely(bq27xxx_battery_capacity_inaccurate(di, di->cache.flags)))
		return POWER_SUPPLY_HEALTH_CALIBRATION_REQUIRED;

	return POWER_SUPPLY_HEALTH_GOOD;
}

static bool bq27xxx_battery_is_full(struct bq27xxx_device_info *di, int flags)
{
	if (di->opts & BQ27XXX_O_ZERO)
		return (flags & BQ27000_FLAG_FC);
	else if (di->opts & BQ27Z561_O_BITS)
		return (flags & BQ27Z561_FLAG_FC);
	else
		return (flags & BQ27XXX_FLAG_FC);
}

/*
 * Return the battery average current in µA and the status
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27xxx_battery_current_and_status(
	struct bq27xxx_device_info *di,
	union power_supply_propval *val_curr,
	union power_supply_propval *val_status,
	struct bq27xxx_reg_cache *cache)
{
	bool single_flags = (di->opts & BQ27XXX_O_ZERO);
	int curr;
	int flags;

	curr = bq27xxx_read(di, BQ27XXX_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}

	if (cache) {
		flags = cache->flags;
	} else {
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, single_flags);
		if (flags < 0) {
			dev_err(di->dev, "error reading flags\n");
			return flags;
		}
	}

	if (di->opts & BQ27XXX_O_ZERO) {
		if (!(flags & BQ27000_FLAG_CHGS)) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}

		curr = curr * BQ27XXX_CURRENT_CONSTANT / BQ27XXX_RS;
	} else {
		/* Other gauges return signed value */
		curr = (int)((s16)curr) * 1000;
	}

	if (val_curr)
		val_curr->intval = curr;

	if (val_status) {
		if (curr > 0) {
			val_status->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else if (curr < 0) {
			val_status->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if (bq27xxx_battery_is_full(di, flags))
				val_status->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val_status->intval =
					POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
	}

	return 0;
}

static void bq27xxx_battery_update_unlocked(struct bq27xxx_device_info *di)
{
	union power_supply_propval status = di->last_status;
	struct bq27xxx_reg_cache cache = {0, };
	bool has_singe_flag = di->opts & BQ27XXX_O_ZERO;

	cache.flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, has_singe_flag);
	if ((cache.flags & 0xff) == 0xff)
		cache.flags = -1; /* read error */
	if (cache.flags >= 0) {
		cache.temperature = bq27xxx_battery_read_temperature(di);
		if (di->regs[BQ27XXX_REG_TTE] != INVALID_REG_ADDR)
			cache.time_to_empty = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTE);
		if (di->regs[BQ27XXX_REG_TTECP] != INVALID_REG_ADDR)
			cache.time_to_empty_avg = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTECP);
		if (di->regs[BQ27XXX_REG_TTF] != INVALID_REG_ADDR)
			cache.time_to_full = bq27xxx_battery_read_time(di, BQ27XXX_REG_TTF);

		cache.charge_full = bq27xxx_battery_read_fcc(di);
		cache.capacity = bq27xxx_battery_read_soc(di);
		if (di->regs[BQ27XXX_REG_AE] != INVALID_REG_ADDR)
			cache.energy = bq27xxx_battery_read_energy(di);
		di->cache.flags = cache.flags;
		cache.health = bq27xxx_battery_read_health(di);
		if (di->regs[BQ27XXX_REG_CYCT] != INVALID_REG_ADDR)
			cache.cycle_count = bq27xxx_battery_read_cyct(di);

		/*
		 * On gauges with signed current reporting the current must be
		 * checked to detect charging <-> discharging status changes.
		 */
		if (!(di->opts & BQ27XXX_O_ZERO))
			bq27xxx_battery_current_and_status(di, NULL, &status, &cache);

		/* We only have to read charge design full once */
		if (di->charge_design_full <= 0)
			di->charge_design_full = bq27xxx_battery_read_dcap(di);
	}

	/* Lifetime data table read */
	if (di->opts & BQ27Z561_O_BITS)
		bq27xxx_update_lifetime(di);


	if ((di->cache.capacity != cache.capacity) ||
	    (di->cache.flags != cache.flags) ||
	    (di->last_status.intval != status.intval)) {
		di->last_status.intval = status.intval;
		power_supply_changed(di->bat);
	}

	if (memcmp(&di->cache, &cache, sizeof(cache)) != 0)
		di->cache = cache;

	di->last_update = jiffies;

	if (!di->removed && poll_interval > 0)
		mod_delayed_work(system_wq, &di->work, poll_interval * HZ);
}

void bq27xxx_battery_update(struct bq27xxx_device_info *di)
{
	mutex_lock(&di->lock);
	bq27xxx_battery_update_unlocked(di);
	mutex_unlock(&di->lock);
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_update);

static void bq27xxx_battery_poll(struct work_struct *work)
{
	struct bq27xxx_device_info *di =
			container_of(work, struct bq27xxx_device_info,
				     work.work);

	bq27xxx_battery_update(di);
}

/*
 * Get the average power in µW
 * Return < 0 if something fails.
 */
static int bq27xxx_battery_pwr_avg(struct bq27xxx_device_info *di,
				   union power_supply_propval *val)
{
	int power;

	power = bq27xxx_read(di, BQ27XXX_REG_AP, false);
	if (power < 0) {
		dev_err(di->dev,
			"error reading average power register %02x: %d\n",
			BQ27XXX_REG_AP, power);
		return power;
	}

	if (di->opts & BQ27XXX_O_ZERO)
		val->intval = (power * BQ27XXX_POWER_CONSTANT) / BQ27XXX_RS;
	else
		/* Other gauges return a signed value in units of 10mW */
		val->intval = (int)((s16)power) * 10000;

	return 0;
}

static int bq27xxx_battery_capacity_level(struct bq27xxx_device_info *di,
					  union power_supply_propval *val)
{
	int level;

	if (di->opts & BQ27XXX_O_ZERO) {
		if (di->cache.flags & BQ27000_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27000_FLAG_EDV1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27000_FLAG_EDVF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else if (di->opts & BQ27Z561_O_BITS) {
		if (di->cache.flags & BQ27Z561_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27Z561_FLAG_FDC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else {
		if (di->cache.flags & BQ27XXX_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27XXX_FLAG_SOC1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27XXX_FLAG_SOCF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}

	val->intval = level;

	return 0;
}

/*
 * Return the battery Voltage in millivolts
 * Or < 0 if something fails.
 */
static int bq27xxx_battery_voltage(struct bq27xxx_device_info *di,
				   union power_supply_propval *val)
{
	int volt;

	volt = bq27xxx_read(di, BQ27XXX_REG_VOLT, false);
	if (volt < 0) {
		dev_err(di->dev, "error reading voltage\n");
		return volt;
	}

	val->intval = volt * 1000;

	return 0;
}

static int bq27xxx_simple_value(int value,
				union power_supply_propval *val)
{
	if (value < 0)
		return value;

	val->intval = value;

	return 0;
}

static int bq27z561_mac_data_to_str(struct bq27xxx_device_info *di,
				u16 mac_command, char *src, size_t src_len,
				char *dest, int dest_len)
{
	char pbuf[10];
	int i;
	int str_len;

	memset(dest, '\0', dest_len);
	if (mac_command == BQ27Z561_MAC_CMD_DN ||
			mac_command == BQ27Z561_MAC_CMD_DC ||
			mac_command == BQ27Z561_MAC_CMD_MN) {
		str_len = snprintf(dest, dest_len, "%s\n", src);
	} else {
		str_len = snprintf(dest, dest_len, "0x:%02X", src[0]);
		for (i = 1; i < src_len; i++) {
			memset(pbuf, '\0', sizeof(pbuf));
			snprintf(pbuf, sizeof(pbuf) - 1, "%02X", src[i]);
			str_len += strlcat(dest, pbuf, dest_len);
		}
		str_len += strlcat(dest, "\n", dest_len);
	}

	return str_len;
}

static int bq27z561_battery_read_mac_block(struct bq27xxx_device_info *di,
					 u16 mac_cmd, char *buf, int buf_len)
{
	int ret = 0;
	u8 lens;
	u8 command[2];

	command[0] = mac_cmd & 0x00FF;
	command[1] = (mac_cmd & 0xFF00) >> 8;

	if (mac_cmd == BQ27Z561_MAC_CMD_RESET ||
			mac_cmd == BQ27Z561_MAC_CMD_GAUGING ||
			mac_cmd == BQ27Z561_MAC_CMD_LDC ||
			mac_cmd == BQ27Z561_MAC_CMD_LDR ||
			mac_cmd == BQ27Z561_MAC_CMD_CM ||
			mac_cmd == BQ27Z561_MAC_CMD_LDF ||
			mac_cmd == BQ27Z561_MAC_CMD_SD ||
			mac_cmd == BQ27Z561_MAC_CMD_RESET1 ||
			mac_cmd == BQ27Z561_MAC_CMD_SDS ||
			mac_cmd == BQ27Z561_MAC_CMD_CDS ||
			mac_cmd == BQ27Z561_MAC_CMD_PG ||
			mac_cmd == BQ27Z561_MAC_CMD_TS ||
			mac_cmd == BQ27Z561_MAC_CMD_ROMMODE ||
			mac_cmd == BQ27Z561_MAC_CMD_HDQ) {
		dev_err(di->dev, "These command only support write : %02X\n",
					mac_cmd);
		return ret;
	}

	ret = bq27z561_write_block(di, BQ27Z561_MAC_CMD_ADDR, command, 2);
	if (ret < 0)
		goto out;

	if (mac_cmd == BQ27Z561_MAC_CMD_IFS ||
			mac_cmd == BQ27Z561_MAC_CMD_SDFS ||
			mac_cmd == BQ27Z561_MAC_CMD_SCDFS ||
			mac_cmd == BQ27Z561_MAC_CMD_ADFS)
		BQ27XXX_MSLEEP(250);
	else
		BQ27XXX_MSLEEP(10);

	if (buf_len < BQ27Z561_MAC_BLOCK_LEN) {
		ret = -EINVAL;
		goto out;
	}

	memset(buf, '\0', buf_len);

	ret = bq27z561_read_block(di, BQ27Z561_MAC_DATA_ADDR,
			buf, BQ27Z561_MAC_BLOCK_LEN);
	if (ret < 0)
		goto out;

	lens = bq27z561_read(di, BQ27Z561_MAC_LEN_ADDR, true);
	if (lens < 0) {
		ret = lens;
		goto out;
	}

	return lens - BQ27Z561_SUB_LEN;

out:
	dev_err(di->dev, "error reading MAC block memory: %d\n", ret);
	return ret;
}

static int bq27z561_battery_read_reg(struct bq27xxx_device_info *di,
					u8 reg_addr)
{
	int ret;

	if (reg_addr == BQ27Z561_SET_TEMP_HI ||
		reg_addr == BQ27Z561_CLEAR_TEMP_HI ||
		reg_addr == BQ27Z561_SET_TEMP_LO ||
		reg_addr == BQ27Z561_CLEAR_TEMP_LO)
		ret = bq27z561_read(di, reg_addr, true);
	else
		ret = bq27z561_read(di, reg_addr, false);
	return ret;
}

static int bq27z561_battery_write_reg(struct bq27xxx_device_info *di,
			u8 reg_addr, u16 value)
{
	int ret;
	u8 command[2];

	if (reg_addr == BQ27Z561_MAC_CMD_ADDR) {
		command[0] = value & 0x00FF;
		command[1] = (value & 0xFF00) >> 8;
		ret = bq27z561_write_block(di, reg_addr, command, 2);
		if (ret < 0) {
			dev_err(di->dev, "write mac cmd failed\n");
		}
	} else if (reg_addr == BQ27Z561_SET_TEMP_HI ||
		reg_addr == BQ27Z561_CLEAR_TEMP_HI ||
		reg_addr == BQ27Z561_SET_TEMP_LO ||
		reg_addr == BQ27Z561_CLEAR_TEMP_LO)
		ret = bq27z561_write(di, reg_addr, value, true);
	else
		ret = bq27z561_write(di, reg_addr, value, false);
	return ret;
}

static int bq27z561_get_soh(struct bq27xxx_device_info *di)
{
	int reg_data;

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_SOH_ADDR);
	if (reg_data < 0) {
		dev_err(di->dev, "get soh error\n");
	}

	return reg_data;
}

static int bq27z561_get_inter_temp(struct bq27xxx_device_info *di)
{
	int reg_data;

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_INTER_TEMP_ADDR);
	if (reg_data < 0) {
		dev_err(di->dev, "get internal temp error\n");
	}

	return reg_data;
}

static int bq27z561_get_remaining_capacity(struct bq27xxx_device_info *di)
{
	int reg_data;

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_REMAIN_CAPACITY_ADDR);
	if (reg_data < 0) {
		dev_err(di->dev, "get remaining capacity error\n");
	}

	return reg_data;
}

static int bq27xxx_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);

	mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ))
		bq27xxx_battery_update_unlocked(di);
	mutex_unlock(&di->lock);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq27xxx_battery_current_and_status(di, NULL, val, NULL);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27xxx_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->cache.flags < 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq27xxx_battery_current_and_status(di, val, NULL, NULL);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = bq27xxx_simple_value(di->cache.capacity, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = bq27xxx_battery_capacity_level(di, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq27xxx_simple_value(di->cache.temperature, val);
		if (ret == 0)
			val->intval -= 2731; /* convert decidegree k to c */
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = bq27xxx_simple_value(di->cache.time_to_empty, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = bq27xxx_simple_value(di->cache.time_to_empty_avg, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = bq27xxx_simple_value(di->cache.time_to_full, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		if (di->opts & BQ27XXX_O_MUL_CHEM)
			val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
		else
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = bq27xxx_simple_value(bq27xxx_battery_read_nac(di), val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = bq27xxx_simple_value(di->cache.charge_full, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = bq27xxx_simple_value(di->charge_design_full, val);
		break;
	/*
	 * TODO: Implement these to make registers set from
	 * power_supply_battery_info visible in sysfs.
	 */
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		return -EINVAL;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = bq27xxx_simple_value(di->cache.cycle_count, val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = bq27xxx_simple_value(di->cache.energy, val);
		break;
	case POWER_SUPPLY_PROP_POWER_AVG:
		ret = bq27xxx_battery_pwr_avg(di, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = bq27xxx_simple_value(di->cache.health, val);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ27XXX_MANUFACTURER;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static void bq27xxx_external_power_changed(struct power_supply *psy)
{
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);

	/* After charger plug in/out wait 0.5s for things to stabilize */
	mod_delayed_work(system_wq, &di->work, HZ / 2);
}

static ssize_t default_reg_data_read_file(struct bq27xxx_device_info *di,
		const char *format, u32 addr, char *buf, int buf_len)
{
	int reg_data;

	reg_data = bq27z561_battery_read_reg(di, addr);
	return scnprintf(buf, buf_len, format, reg_data);
}

static ssize_t default_read_block_to_str(struct bq27xxx_device_info *di,
					 u16 mac_cmd, char *buf, int buf_len)
{
	int ret;
	int mac_data_lens;
	char mac_buf[40];

	ret = bq27z561_battery_read_mac_block(di, mac_cmd,
			mac_buf, sizeof(mac_buf));
	if (ret < 0)
		return ret;

	mac_data_lens = ret;
	ret = bq27z561_mac_data_to_str(di, mac_cmd, mac_buf, mac_data_lens,
		buf, buf_len);
	return ret;
}

static ssize_t seal_device_read_block_to_str(struct bq27xxx_device_info *di,
					char *buf, int buf_len)
{
	int ret;
	int sealed_status;
	char mac_buf[40];

	/*   read Operation Status A to check sealed status*/
	ret = bq27z561_battery_read_mac_block(di, BQ27Z561_MAC_CMD_OS,
			mac_buf, sizeof(mac_buf));
	if (ret < 0)
		return ret;
	/* Operation Status A:
	 *	SEC1, SEC0 (Bits 9, 8): SECURITY Mode
	 *	0, 0 = Reserved
	 *	0, 1 = Full Access
	 *	1, 0 = Unsealed
	 *	1, 1 = Sealed
	 */
	sealed_status = mac_buf[1] & 0x03;

	return snprintf(buf, buf_len, "%s\n",
			bq27z561_sealed_status_str[sealed_status]);

}

static ssize_t bq27xxx_store(struct device *dev,
					struct device_attribute *dattr,
					const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct bq27xxx_device_info *di = i2c_get_clientdata(client);
	struct bq27xxx_attribute *attr =
		container_of(dattr, struct bq27xxx_attribute, dattr);
	u32 id = attr->id;
	bool negative = false;
	char *start = (char *)buf;
	unsigned long value;
	int ret;
	int status;
	u8 command[2];
	bool has_command = false;
	char mac_buf[40];

	while (*start == ' ')
		start++;
	if (*start == '-') {
		negative = true;
		start++;
	}

	if (id == ADDRESS || id == DATA || id == MAC_CMD) {
		if (kstrtoul(start, 16, &value))
			return -EINVAL;
	} else {
		if (kstrtoul(start, 10, &value))
			return -EINVAL;
	}

	switch (id) {
	case ADDRESS:
		di->reg_addr = value;
		break;
	case DATA:
		ret = bq27z561_battery_write_reg(di, di->reg_addr, value);
		if (ret < 0)
			return ret;
		break;
	case MAC_CMD:
		di->reg_data = value;
		break;
	case MAC_DATA:
		return 0;
	case AT_RATE:
		if (negative)
			value = -1 * value;

		ret = bq27z561_battery_write_reg(di, BQ27Z561_ATRATE_ADDR,
							value);
		if (ret < 0)
			return ret;
		break;
	case DEVICE_RESET:
		command[0] = BQ27Z561_MAC_CMD_RESET & 0x00FF;
		command[1] = (BQ27Z561_MAC_CMD_RESET & 0xFF00) >> 8;
		has_command = true;
		break;
	case LIFETIME_DATA_COLLECTION:
		if (value != 1 && value != 0)
			return -EINVAL;

		/* read Manufacturing Status to check Lifetime Data Collection*/
		ret = bq27z561_battery_read_mac_block(di, BQ27Z561_MAC_CMD_MS,
				mac_buf, sizeof(mac_buf));
		if (ret < 0)
			return ret;
		/* LF_EN bit:5 of Manufacturing Status*/
		status = (mac_buf[0] & 0x20) >> 5;
		if (status == value)
			return count;

		command[0] = BQ27Z561_MAC_CMD_LDC & 0x00FF;
		command[1] = (BQ27Z561_MAC_CMD_LDC & 0xFF00) >> 8;
		has_command = true;
		break;
	case LIFETIME_DATA_RESET:
		command[0] = BQ27Z561_MAC_CMD_LDR & 0x00FF;
		command[1] = (BQ27Z561_MAC_CMD_LDR & 0xFF00) >> 8;
		has_command = true;
		break;
	case SEAL_DEVICE:
		command[0] = BQ27Z561_MAC_CMD_SD & 0x00FF;
		command[1] = (BQ27Z561_MAC_CMD_SD & 0xFF00) >> 8;
		has_command = true;
		break;
	}
	if (has_command) {
		ret = bq27z561_write_block(di, BQ27Z561_MAC_CMD_ADDR,
						command, 2);
		if (ret < 0)
			return ret;
	}

	return count;
}

static ssize_t bq27xxx_show(struct device *dev,
				struct device_attribute *dattr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct bq27xxx_device_info *di = i2c_get_clientdata(client);
	struct bq27xxx_attribute *attr =
		container_of(dattr, struct bq27xxx_attribute, dattr);
	int val = 0;
	ssize_t count = 0, write_count = 0;
	u32 id = attr->id;
	u32 val1 = attr->val1;
	u32 val2 = attr->val2;
	char mac_buf[40];
	char tmp_str[5];

	switch (id) {
	case ADDRESS:
		val = di->reg_addr;
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case DATA:
		count = default_reg_data_read_file(di, "0x:%02X\n",
				di->reg_addr, buf, PAGE_SIZE);
		break;
	case MAC_CMD:
		val = di->reg_data;
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case MAC_DATA:
		if (di->reg_addr != BQ27Z561_MAC_CMD_ADDR)
			return -EINVAL;
		count = default_read_block_to_str(di,
					di->reg_data, buf, PAGE_SIZE);
		break;
	case AT_RATE:
		count = default_reg_data_read_file(di, "%d\n",
				BQ27Z561_ATRATE_ADDR, buf, PAGE_SIZE);
		break;
	case AT_RATE_TIME_TO_EMPTY:
		count = default_reg_data_read_file(di, "%d\n",
				BQ27Z561_ATRATETTE_ADDR, buf, PAGE_SIZE);
		break;
	case DEVICE_TYPE:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_DT, buf, PAGE_SIZE);
		break;
	case FIRMWARE_VERSION:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_FV, buf, PAGE_SIZE);
		break;
	case HARDWARE_VERSION:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_HV, buf, PAGE_SIZE);
		break;
	case INSTRUCTION_FLASH_SIGNATURE:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_IFS, buf, PAGE_SIZE);
		break;
	case STATIC_DF_SIGNATURE:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_SDFS, buf, PAGE_SIZE);
		break;
	case CHEMICAL_ID:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_CID, buf, PAGE_SIZE);
		break;
	case STATIC_CHEM_DF_SIGNATURE:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_SCDFS, buf, PAGE_SIZE);
		break;
	case ALL_DF_SIGNATURE:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_ADFS, buf, PAGE_SIZE);
		break;
	case SEAL_DEVICE:
		count = seal_device_read_block_to_str(di, buf, PAGE_SIZE);
		break;
	case DEVICE_NAME:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_DN, buf, PAGE_SIZE);
		break;
	case DEVICE_CHEM:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_DC, buf, PAGE_SIZE);
		break;
	case MANUFACTURER_NAME:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_MN, buf, PAGE_SIZE);
		break;
	case MANUFACTURER_DATE:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_MD, buf, PAGE_SIZE);
		break;
	case SERIAL_NUMBER:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_SN, buf, PAGE_SIZE);
		break;
	case OPERATION_STATUS:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_OS, buf, PAGE_SIZE);
	case CHARGING_STATUS:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_CS, buf, PAGE_SIZE);
		break;
	case GAUGING_STATUS:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_GS, buf, PAGE_SIZE);
		break;
	case MANUFACTURING_STATUS:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_MS, buf, PAGE_SIZE);
		break;
	case LIFETIME_DATA_BLOCK:
		count = default_read_block_to_str(di,
					BQ27Z561_MAC_CMD_LDB1, buf, PAGE_SIZE);
		break;
	case LIFETIME1_LOWER:
		val = di->lifetime_blocks.lifetime1_lower[val1];
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case LIFETIME1_HIGHER:
		val = di->lifetime_blocks.lifetime1_higher[val1];
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case LIFETIME3:
		val = di->lifetime_blocks.lifetime3;
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case LIFETIME4:
		val = di->lifetime_blocks.lifetime4[val1];
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case TEMP_ZONES:
		val = di->lifetime_blocks.temp_zones[val1][val2];
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case SOH:
		val = bq27z561_get_soh(di);
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case INTER_TEMP:
		val = bq27z561_get_inter_temp(di);
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case REMAINING_CAPACITY:
		val = bq27z561_get_remaining_capacity(di);
		count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
		break;
	case MFG_INFO_A:
		count = bq27z561_battery_read_mac_block(di, BQ27Z561_MAC_CMD_MI_A,
			buf, PAGE_SIZE);
		strlcat(buf, "\n", PAGE_SIZE);
		break;
	case MFG_INFO_B:
		memset(mac_buf, '\0', sizeof(mac_buf));
		count = bq27z561_battery_read_mac_block(di, BQ27Z561_MAC_CMD_MI_B,
			mac_buf, sizeof(mac_buf));
		/* Refer to Battery Pack Genealogy ERS document
		 * (https://fburl.com/diff/uo2prae3) for more information.
		 *
		 * The first three bytes of manufacturer info block B are hex
		 * convert them to string after read these three bytes out
		 */
		if (mac_buf[0] < '0') {
			memset(tmp_str, 0x00, sizeof(tmp_str));
			snprintf(tmp_str, sizeof(tmp_str) - 1, "%02X%02X%02X",
				mac_buf[0], mac_buf[1], mac_buf[2]);
			strlcat(buf, tmp_str, PAGE_SIZE);
			strlcat(buf, mac_buf + 3, PAGE_SIZE);
		} else {
			strlcat(buf, mac_buf, PAGE_SIZE);
		}
		strlcat(buf, "\n", PAGE_SIZE);
		break;
	case MFG_INFO_C:
		memset(mac_buf, '\0', sizeof(mac_buf));
		count = bq27z561_battery_read_mac_block(di, BQ27Z561_MAC_CMD_MI_C,
			mac_buf, sizeof(mac_buf));
		for (val = 0; val < count; val++) {
			if (mac_buf[val] != '\0') {
				buf[write_count] = mac_buf[val];
				write_count++;
			}
		}
		buf[write_count] = '\0';
		strlcat(buf, "\n", PAGE_SIZE);
		break;
	}

	return count;
}

static BQ27XXX_ATTR(address, 0664,
			bq27xxx_show, bq27xxx_store, ADDRESS, 0, 0);
static BQ27XXX_ATTR(data, 0664,
			bq27xxx_show, bq27xxx_store, DATA, 0, 0);
static BQ27XXX_ATTR(mac_cmd, 0664,
			bq27xxx_show, bq27xxx_store, MAC_CMD, 0, 0);
static BQ27XXX_ATTR(mac_data, 0664,
			bq27xxx_show, bq27xxx_store, MAC_DATA, 0, 0);
static BQ27XXX_ATTR(at_rate, 0664,
			bq27xxx_show, bq27xxx_store, AT_RATE, 0, 0);
static BQ27XXX_ATTR(at_rate_time_to_empty, 0664,
			bq27xxx_show, bq27xxx_store, AT_RATE_TIME_TO_EMPTY, 0, 0);
static BQ27XXX_ATTR(device_type, 0444,
			bq27xxx_show, NULL, DEVICE_TYPE, 0, 0);
static BQ27XXX_ATTR(firmware_version, 0444,
			bq27xxx_show, NULL, FIRMWARE_VERSION, 0, 0);
static BQ27XXX_ATTR(hardware_version, 0444,
			bq27xxx_show, NULL, HARDWARE_VERSION, 0, 0);
static BQ27XXX_ATTR(instruction_flash_signature, 0444,
			bq27xxx_show, NULL, INSTRUCTION_FLASH_SIGNATURE, 0, 0);
static BQ27XXX_ATTR(static_df_signature, 0444,
			bq27xxx_show, NULL, STATIC_DF_SIGNATURE, 0, 0);
static BQ27XXX_ATTR(chemical_id, 0444,
			bq27xxx_show, NULL, CHEMICAL_ID, 0, 0);
static BQ27XXX_ATTR(static_chem_df_signature, 0444,
			bq27xxx_show, NULL, STATIC_CHEM_DF_SIGNATURE, 0, 0);
static BQ27XXX_ATTR(all_df_signature, 0444,
			bq27xxx_show, NULL, ALL_DF_SIGNATURE, 0, 0);
static BQ27XXX_ATTR(device_reset, 0220,
			NULL, bq27xxx_store, DEVICE_RESET, 0, 0);
static BQ27XXX_ATTR(lifetime_data_collection, 0220,
			NULL, bq27xxx_store, LIFETIME_DATA_COLLECTION, 0, 0);
static BQ27XXX_ATTR(lifetime_data_reset, 0220,
			NULL, bq27xxx_store, LIFETIME_DATA_RESET, 0, 0);
static BQ27XXX_ATTR(seal_device, 0664,
			bq27xxx_show, bq27xxx_store, SEAL_DEVICE, 0, 0);
static BQ27XXX_ATTR(device_name, 0444,
			bq27xxx_show, NULL, DEVICE_NAME, 0, 0);
static BQ27XXX_ATTR(device_chem, 0444,
			bq27xxx_show, NULL, DEVICE_CHEM, 0, 0);
static BQ27XXX_ATTR(manufacturer_name, 0444,
			bq27xxx_show, NULL, MANUFACTURER_NAME, 0, 0);
static BQ27XXX_ATTR(manufacturer_date, 0444,
			bq27xxx_show, NULL, MANUFACTURER_DATE, 0, 0);
static BQ27XXX_ATTR(serial_number, 0444,
			bq27xxx_show, NULL, SERIAL_NUMBER, 0, 0);
static BQ27XXX_ATTR(operation_status, 0444,
			bq27xxx_show, NULL, OPERATION_STATUS, 0, 0);
static BQ27XXX_ATTR(charging_status, 0444,
			bq27xxx_show, NULL, CHARGING_STATUS, 0, 0);
static BQ27XXX_ATTR(gauging_status, 0444,
			bq27xxx_show, NULL, GAUGING_STATUS, 0, 0);
static BQ27XXX_ATTR(manufacturing_status, 0444,
			bq27xxx_show, NULL, MANUFACTURING_STATUS, 0, 0);
static BQ27XXX_ATTR(lifetime_data_block, 0444,
			bq27xxx_show, NULL, LIFETIME_DATA_BLOCK, 0, 0);
static BQ27XXX_ATTR(cell_1_max_voltage, 0444,
			bq27xxx_show, NULL, LIFETIME1_LOWER, 0, 0);
static BQ27XXX_ATTR(cell_1_min_voltage, 0444,
			bq27xxx_show, NULL, LIFETIME1_LOWER, 1, 0);
static BQ27XXX_ATTR(max_charge_current, 0444,
			bq27xxx_show, NULL, LIFETIME1_LOWER, 2, 0);
static BQ27XXX_ATTR(max_discharge_current, 0444,
			bq27xxx_show, NULL, LIFETIME1_LOWER, 3, 0);
static BQ27XXX_ATTR(max_avg_discharge_current, 0444,
			bq27xxx_show, NULL, LIFETIME1_LOWER, 4, 0);
static BQ27XXX_ATTR(max_avg_dsg_power, 0444,
			bq27xxx_show, NULL, LIFETIME1_LOWER, 5, 0);
static BQ27XXX_ATTR(max_temp_cell, 0444,
			bq27xxx_show, NULL, LIFETIME1_HIGHER, 0, 0);
static BQ27XXX_ATTR(min_temp_cell, 0444,
			bq27xxx_show, NULL, LIFETIME1_HIGHER, 1, 0);
static BQ27XXX_ATTR(max_temp_int_sensor, 0444,
			bq27xxx_show, NULL, LIFETIME1_HIGHER, 2, 0);
static BQ27XXX_ATTR(min_temp_int_sensor, 0444,
			bq27xxx_show, NULL, LIFETIME1_HIGHER, 3, 0);
static BQ27XXX_ATTR(total_fw_runtime, 0444,
			bq27xxx_show, NULL, LIFETIME3, 0, 0);
static BQ27XXX_ATTR(num_valid_charge_terminations, 0444,
			bq27xxx_show, NULL, LIFETIME4, 0, 0);
static BQ27XXX_ATTR(last_valid_charge_term, 0444,
			bq27xxx_show, NULL, LIFETIME4, 1, 0);
static BQ27XXX_ATTR(num_qmax_updates, 0444,
			bq27xxx_show, NULL, LIFETIME4, 2, 0);
static BQ27XXX_ATTR(last_qmax_update, 0444,
			bq27xxx_show, NULL, LIFETIME4, 3, 0);
static BQ27XXX_ATTR(num_ra_update, 0444,
			bq27xxx_show, NULL, LIFETIME4, 4, 0);
static BQ27XXX_ATTR(last_ra_update, 0444,
			bq27xxx_show, NULL, LIFETIME4, 5, 0);
static BQ27XXX_ATTR(t_ut_rsoc_a, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 0);
static BQ27XXX_ATTR(t_ut_rsoc_b, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 1);
static BQ27XXX_ATTR(t_ut_rsoc_c, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 2);
static BQ27XXX_ATTR(t_ut_rsoc_d, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 3);
static BQ27XXX_ATTR(t_ut_rsoc_e, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 4);
static BQ27XXX_ATTR(t_ut_rsoc_f, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 5);
static BQ27XXX_ATTR(t_ut_rsoc_g, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 6);
static BQ27XXX_ATTR(t_ut_rsoc_h, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 0, 7);
static BQ27XXX_ATTR(t_lt_rsoc_a, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 0);
static BQ27XXX_ATTR(t_lt_rsoc_b, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 1);
static BQ27XXX_ATTR(t_lt_rsoc_c, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 2);
static BQ27XXX_ATTR(t_lt_rsoc_d, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 3);
static BQ27XXX_ATTR(t_lt_rsoc_e, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 4);
static BQ27XXX_ATTR(t_lt_rsoc_f, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 5);
static BQ27XXX_ATTR(t_lt_rsoc_g, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 6);
static BQ27XXX_ATTR(t_lt_rsoc_h, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 1, 7);
static BQ27XXX_ATTR(t_stl_rsoc_a, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 0);
static BQ27XXX_ATTR(t_stl_rsoc_b, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 1);
static BQ27XXX_ATTR(t_stl_rsoc_c, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 2);
static BQ27XXX_ATTR(t_stl_rsoc_d, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 3);
static BQ27XXX_ATTR(t_stl_rsoc_e, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 4);
static BQ27XXX_ATTR(t_stl_rsoc_f, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 5);
static BQ27XXX_ATTR(t_stl_rsoc_g, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 6);
static BQ27XXX_ATTR(t_stl_rsoc_h, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 2, 7);
static BQ27XXX_ATTR(t_rt_rsoc_a, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 0);
static BQ27XXX_ATTR(t_rt_rsoc_b, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 1);
static BQ27XXX_ATTR(t_rt_rsoc_c, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 2);
static BQ27XXX_ATTR(t_rt_rsoc_d, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 3);
static BQ27XXX_ATTR(t_rt_rsoc_e, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 4);
static BQ27XXX_ATTR(t_rt_rsoc_f, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 5);
static BQ27XXX_ATTR(t_rt_rsoc_g, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 6);
static BQ27XXX_ATTR(t_rt_rsoc_h, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 3, 7);
static BQ27XXX_ATTR(t_sth_rsoc_a, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 0);
static BQ27XXX_ATTR(t_sth_rsoc_b, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 1);
static BQ27XXX_ATTR(t_sth_rsoc_c, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 2);
static BQ27XXX_ATTR(t_sth_rsoc_d, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 3);
static BQ27XXX_ATTR(t_sth_rsoc_e, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 4);
static BQ27XXX_ATTR(t_sth_rsoc_f, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 5);
static BQ27XXX_ATTR(t_sth_rsoc_g, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 6);
static BQ27XXX_ATTR(t_sth_rsoc_h, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 4, 7);
static BQ27XXX_ATTR(t_ht_rsoc_a, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 0);
static BQ27XXX_ATTR(t_ht_rsoc_b, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 1);
static BQ27XXX_ATTR(t_ht_rsoc_c, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 2);
static BQ27XXX_ATTR(t_ht_rsoc_d, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 3);
static BQ27XXX_ATTR(t_ht_rsoc_e, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 4);
static BQ27XXX_ATTR(t_ht_rsoc_f, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 5);
static BQ27XXX_ATTR(t_ht_rsoc_g, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 6);
static BQ27XXX_ATTR(t_ht_rsoc_h, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 5, 7);
static BQ27XXX_ATTR(t_ot_rsoc_a, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 0);
static BQ27XXX_ATTR(t_ot_rsoc_b, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 1);
static BQ27XXX_ATTR(t_ot_rsoc_c, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 2);
static BQ27XXX_ATTR(t_ot_rsoc_d, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 3);
static BQ27XXX_ATTR(t_ot_rsoc_e, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 4);
static BQ27XXX_ATTR(t_ot_rsoc_f, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 5);
static BQ27XXX_ATTR(t_ot_rsoc_g, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 6);
static BQ27XXX_ATTR(t_ot_rsoc_h, 0444,
			bq27xxx_show, NULL, TEMP_ZONES, 6, 7);
static BQ27XXX_ATTR(soh, 0444,
			bq27xxx_show, NULL, SOH, 0, 0);
static BQ27XXX_ATTR(inter_temp, 0444,
			bq27xxx_show, NULL, INTER_TEMP, 0, 0);
static BQ27XXX_ATTR(remaining_capacity, 0444,
			bq27xxx_show, NULL, REMAINING_CAPACITY, 0, 0);
static BQ27XXX_ATTR(manufacturer_info_a, 0444,
			bq27xxx_show, NULL, MFG_INFO_A, 0, 0);
static BQ27XXX_ATTR(manufacturer_info_b, 0444,
			bq27xxx_show, NULL, MFG_INFO_B, 0, 0);
static BQ27XXX_ATTR(manufacturer_info_c, 0444,
			bq27xxx_show, NULL, MFG_INFO_C, 0, 0);

static struct attribute *bq27xxx_attrs[] = {
	&bq27xxx_attr_address.dattr.attr,
	&bq27xxx_attr_data.dattr.attr,
	&bq27xxx_attr_mac_cmd.dattr.attr,
	&bq27xxx_attr_mac_data.dattr.attr,
	&bq27xxx_attr_at_rate.dattr.attr,
	&bq27xxx_attr_at_rate_time_to_empty.dattr.attr,
	&bq27xxx_attr_device_type.dattr.attr,
	&bq27xxx_attr_firmware_version.dattr.attr,
	&bq27xxx_attr_hardware_version.dattr.attr,
	&bq27xxx_attr_instruction_flash_signature.dattr.attr,
	&bq27xxx_attr_static_df_signature.dattr.attr,
	&bq27xxx_attr_chemical_id.dattr.attr,
	&bq27xxx_attr_static_chem_df_signature.dattr.attr,
	&bq27xxx_attr_all_df_signature.dattr.attr,
	&bq27xxx_attr_device_reset.dattr.attr,
	&bq27xxx_attr_lifetime_data_collection.dattr.attr,
	&bq27xxx_attr_lifetime_data_reset.dattr.attr,
	&bq27xxx_attr_seal_device.dattr.attr,
	&bq27xxx_attr_device_name.dattr.attr,
	&bq27xxx_attr_device_chem.dattr.attr,
	&bq27xxx_attr_manufacturer_name.dattr.attr,
	&bq27xxx_attr_manufacturer_date.dattr.attr,
	&bq27xxx_attr_serial_number.dattr.attr,
	&bq27xxx_attr_operation_status.dattr.attr,
	&bq27xxx_attr_charging_status.dattr.attr,
	&bq27xxx_attr_gauging_status.dattr.attr,
	&bq27xxx_attr_manufacturing_status.dattr.attr,
	&bq27xxx_attr_lifetime_data_block.dattr.attr,
	&bq27xxx_attr_cell_1_max_voltage.dattr.attr,
	&bq27xxx_attr_cell_1_min_voltage.dattr.attr,
	&bq27xxx_attr_max_charge_current.dattr.attr,
	&bq27xxx_attr_max_discharge_current.dattr.attr,
	&bq27xxx_attr_max_avg_discharge_current.dattr.attr,
	&bq27xxx_attr_max_avg_dsg_power.dattr.attr,
	&bq27xxx_attr_max_temp_cell.dattr.attr,
	&bq27xxx_attr_min_temp_cell.dattr.attr,
	&bq27xxx_attr_max_temp_int_sensor.dattr.attr,
	&bq27xxx_attr_min_temp_int_sensor.dattr.attr,
	&bq27xxx_attr_total_fw_runtime.dattr.attr,
	&bq27xxx_attr_num_valid_charge_terminations.dattr.attr,
	&bq27xxx_attr_last_valid_charge_term.dattr.attr,
	&bq27xxx_attr_num_qmax_updates.dattr.attr,
	&bq27xxx_attr_last_qmax_update.dattr.attr,
	&bq27xxx_attr_num_ra_update.dattr.attr,
	&bq27xxx_attr_last_ra_update.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_a.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_b.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_c.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_d.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_e.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_f.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_g.dattr.attr,
	&bq27xxx_attr_t_ut_rsoc_h.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_a.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_b.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_c.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_d.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_e.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_f.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_g.dattr.attr,
	&bq27xxx_attr_t_lt_rsoc_h.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_a.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_b.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_c.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_d.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_e.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_f.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_g.dattr.attr,
	&bq27xxx_attr_t_stl_rsoc_h.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_a.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_b.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_c.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_d.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_e.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_f.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_g.dattr.attr,
	&bq27xxx_attr_t_rt_rsoc_h.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_a.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_b.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_c.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_d.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_e.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_f.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_g.dattr.attr,
	&bq27xxx_attr_t_sth_rsoc_h.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_a.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_b.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_c.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_d.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_e.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_f.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_g.dattr.attr,
	&bq27xxx_attr_t_ht_rsoc_h.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_a.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_b.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_c.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_d.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_e.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_f.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_g.dattr.attr,
	&bq27xxx_attr_t_ot_rsoc_h.dattr.attr,
	&bq27xxx_attr_soh.dattr.attr,
	&bq27xxx_attr_inter_temp.dattr.attr,
	&bq27xxx_attr_remaining_capacity.dattr.attr,
	&bq27xxx_attr_manufacturer_info_a.dattr.attr,
	&bq27xxx_attr_manufacturer_info_b.dattr.attr,
	&bq27xxx_attr_manufacturer_info_c.dattr.attr,
	NULL
};

static const struct attribute_group bq27xxx_attr_group = {
	.attrs = bq27xxx_attrs,
};

static void bq27z561_create_sysfs(struct bq27xxx_device_info *di)
{
	int result = sysfs_create_group(&di->bat->dev.kobj, &bq27xxx_attr_group);

	if (result != 0)
		dev_err(di->dev, "Error creating sysfs entries: %d\n", result);
}

static void bq27z561_remove_sysfs(struct bq27xxx_device_info *di)
{
	sysfs_remove_group(&di->bat->dev.kobj, &bq27xxx_attr_group);
}

int bq27xxx_battery_setup(struct bq27xxx_device_info *di)
{
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {
		.of_node = di->dev->of_node,
		.drv_data = di,
	};

	INIT_DELAYED_WORK(&di->work, bq27xxx_battery_poll);
	mutex_init(&di->lock);

	di->regs       = bq27xxx_chip_data[di->chip].regs;
	di->unseal_key = bq27xxx_chip_data[di->chip].unseal_key;
	di->dm_regs    = bq27xxx_chip_data[di->chip].dm_regs;
	di->opts       = bq27xxx_chip_data[di->chip].opts;

	psy_desc = devm_kzalloc(di->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;

	psy_desc->name = di->name;
	psy_desc->type = POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = bq27xxx_chip_data[di->chip].props;
	psy_desc->num_properties = bq27xxx_chip_data[di->chip].props_size;
	psy_desc->get_property = bq27xxx_battery_get_property;
	psy_desc->external_power_changed = bq27xxx_external_power_changed;

	di->bat = power_supply_register_no_ws(di->dev, psy_desc, &psy_cfg);
	if (IS_ERR(di->bat))
		return dev_err_probe(di->dev, PTR_ERR(di->bat),
				     "failed to register battery\n");

	bq27z561_create_sysfs(di);

	bq27xxx_battery_settings(di);
	bq27xxx_battery_update(di);

	mutex_lock(&bq27xxx_list_lock);
	list_add(&di->list, &bq27xxx_battery_devices);
	mutex_unlock(&bq27xxx_list_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_setup);

void bq27xxx_battery_teardown(struct bq27xxx_device_info *di)
{
	mutex_lock(&bq27xxx_list_lock);
	list_del(&di->list);
	mutex_unlock(&bq27xxx_list_lock);

	/* Set removed to avoid bq27xxx_battery_update() re-queuing the work */
	mutex_lock(&di->lock);
	di->removed = true;
	mutex_unlock(&di->lock);

	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(di->bat);
	bq27z561_remove_sysfs(di);
	mutex_destroy(&di->lock);
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_teardown);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27xxx battery monitor driver");
MODULE_LICENSE("GPL");
