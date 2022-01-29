/*
 * BQ27xxx battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 * Copyright (C) 2017 Liam Breck <kernel@networkimprov.net>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Datasheets:
 * http://www.ti.com/product/bq27000
 * http://www.ti.com/product/bq27200
 * http://www.ti.com/product/bq27010
 * http://www.ti.com/product/bq27210
 * http://www.ti.com/product/bq27500
 * http://www.ti.com/product/bq27510-g1
 * http://www.ti.com/product/bq27510-g2
 * http://www.ti.com/product/bq27510-g3
 * http://www.ti.com/product/bq27520-g1
 * http://www.ti.com/product/bq27520-g2
 * http://www.ti.com/product/bq27520-g3
 * http://www.ti.com/product/bq27520-g4
 * http://www.ti.com/product/bq27530-g1
 * http://www.ti.com/product/bq27531-g1
 * http://www.ti.com/product/bq27541-g1
 * http://www.ti.com/product/bq27542-g1
 * http://www.ti.com/product/bq27546-g1
 * http://www.ti.com/product/bq27742-g1
 * http://www.ti.com/product/bq27545-g1
 * http://www.ti.com/product/bq27421-g1
 * http://www.ti.com/product/bq27425-g1
 * http://www.ti.com/product/bq27426
 * http://www.ti.com/product/bq27411-g1
 * http://www.ti.com/product/bq27441-g1
 * http://www.ti.com/product/bq27621-g1
 */

#include <linux/device.h>
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
#include <linux/debugfs.h>
#include <linux/uaccess.h>

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

/* BQ27z561 has different layout for Flags register */
#define BQ27Z561_FLAG_FC	BIT(5)
#define BQ27Z561_FLAG_DSG	BIT(6) /* Charge state flag */

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
#define ARC_BATTERY_HEAD	"LGCSWD"	//Head byte of arc battery

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
		[BQ27XXX_REG_AI] = 0x0c,
		[BQ27XXX_REG_FLAGS] = 0x0a,
		[BQ27XXX_REG_TTE] = 0x16,
		[BQ27XXX_REG_TTF] = 0x18,
		[BQ27XXX_REG_TTES] = INVALID_REG_ADDR,
		[BQ27XXX_REG_TTECP] = INVALID_REG_ADDR,
		[BQ27XXX_REG_NAC] = 0x0c,
		[BQ27XXX_REG_FCC] = 0x12,
		[BQ27XXX_REG_CYCT] = 0x2a,
		[BQ27XXX_REG_AE] = INVALID_REG_ADDR,
		[BQ27XXX_REG_SOC] = 0x2c,
		[BQ27XXX_REG_DCAP] = 0x3c,
		[BQ27XXX_REG_AP] = 0x22,
		BQ27XXX_DM_REG_ROWS,
	},
#define bq27542_regs bq27541_regs
#define bq27546_regs bq27541_regs
#define bq27742_regs bq27541_regs
#define bq27z561_regs bq27541_regs
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
	};
#define bq27425_regs bq27421_regs
#define bq27426_regs bq27421_regs
#define bq27441_regs bq27421_regs
#define bq27621_regs bq27421_regs

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
	POWER_SUPPLY_PROP_MANUFACTURER_INFO_A,
	POWER_SUPPLY_PROP_MANUFACTURER_INFO_B,
	POWER_SUPPLY_PROP_MANUFACTURER_INFO_C,
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
#define bq27425_props bq27421_props
#define bq27426_props bq27421_props
#define bq27441_props bq27421_props
#define bq27621_props bq27421_props

enum bq27xxx_manufacturer_info_type {
	MANUFACTURER_INFO_A,
	MANUFACTURER_INFO_B,
	MANUFACTURER_INFO_C,
};

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
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_SOH,
	POWER_SUPPLY_PROP_BQ27Z561_INTERTEMP,
	POWER_SUPPLY_PROP_BQ27Z561_REMAININGCAPACITY,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MANUFACTURER_INFO_A,
	POWER_SUPPLY_PROP_MANUFACTURER_INFO_B,
	POWER_SUPPLY_PROP_MANUFACTURER_INFO_C,
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
#define bq27z561_dm_regs 0

#if 0 /* not yet tested */
static struct bq27xxx_dm_reg bq27545_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 48, 23, 2,    0, 32767 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 48, 25, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 80, 67, 2, 2800,  3700 },
};
#else
#define bq27545_dm_regs 0
#endif

static struct bq27xxx_dm_reg bq27421_dm_regs[] = {
	[BQ27XXX_DM_DESIGN_CAPACITY]   = { 82, 10, 2,    0,  8000 },
	[BQ27XXX_DM_DESIGN_ENERGY]     = { 82, 12, 2,    0, 32767 },
	[BQ27XXX_DM_TERMINATE_VOLTAGE] = { 82, 16, 2, 2500,  3700 },
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

#define BQ27XXX_O_ZERO	0x00000001
#define BQ27XXX_O_OTDC	0x00000002 /* has OTC/OTD overtemperature flags */
#define BQ27XXX_O_UTOT  0x00000004 /* has OT overtemperature flag */
#define BQ27XXX_O_CFGUP	0x00000008
#define BQ27XXX_O_RAM	0x00000010
#define BQ27XXX_O_Z 0x80000000

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
	[BQ27000]   = BQ27XXX_DATA(bq27000,   0         , BQ27XXX_O_ZERO),
	[BQ27010]   = BQ27XXX_DATA(bq27010,   0         , BQ27XXX_O_ZERO),
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
	[BQ27421]   = BQ27XXX_DATA(bq27421,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27425]   = BQ27XXX_DATA(bq27425,   0x04143672, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP),
	[BQ27426]   = BQ27XXX_DATA(bq27426,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27441]   = BQ27XXX_DATA(bq27441,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27621]   = BQ27XXX_DATA(bq27621,   0x80008000, BQ27XXX_O_UTOT | BQ27XXX_O_CFGUP | BQ27XXX_O_RAM),
	[BQ27Z561]  = BQ27XXX_DATA(bq27z561,  0, BQ27XXX_O_UTOT | BQ27XXX_O_Z),
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
};


static bool bq27xxx_dt_to_nvm = true;
module_param_named(dt_monitored_battery_updates_nvm, bq27xxx_dt_to_nvm, bool, 0444);
MODULE_PARM_DESC(dt_monitored_battery_updates_nvm,
	"Devicetree monitored-battery config updates data memory on NVM/flash chips.\n"
	"Users must set this =0 when installing a different type of battery!\n"
	"Default is =1."
#ifndef CONFIG_BATTERY_BQ27XXX_DT_UPDATES_NVM
	"\nSetting this affects future kernel updates, not the current configuration."
#endif
);

static int poll_interval_param_set(const char *val, const struct kernel_param *kp)
{
	struct bq27xxx_device_info *di;
	unsigned int prev_val = *(unsigned int *) kp->arg;
	int ret;

	ret = param_set_uint(val, kp);
	if (ret < 0 || prev_val == *(unsigned int *) kp->arg)
		return ret;

	mutex_lock(&bq27xxx_list_lock);
	list_for_each_entry(di, &bq27xxx_battery_devices, list) {
		cancel_delayed_work_sync(&di->work);
		schedule_delayed_work(&di->work, 0);
	}
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

	ret = bq27xxx_write_block(di, BQ27XXX_DM_DATA, buf->data, BQ27XXX_DM_SZ);
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

	if (bq27xxx_battery_unseal(di) < 0)
		return;

	if (info->charge_full_design_uah != -EINVAL &&
	    info->energy_full_design_uwh != -EINVAL) {
		bq27xxx_battery_read_dm_block(di, &bd);
		/* assume design energy & capacity are in same block */
		bq27xxx_battery_update_dm_block(di, &bd,
					BQ27XXX_DM_DESIGN_CAPACITY,
					info->charge_full_design_uah / 1000);
		bq27xxx_battery_update_dm_block(di, &bd,
					BQ27XXX_DM_DESIGN_ENERGY,
					info->energy_full_design_uwh / 1000);
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

	if (di->opts & BQ27XXX_O_ZERO)
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
	int flags;

	if (di->opts & BQ27XXX_O_ZERO) {
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, true);
		if (flags >= 0 && (flags & BQ27000_FLAG_CI))
			return -ENODATA;
	}

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
 * Read an average power register.
 * Return < 0 if something fails.
 */
static int bq27xxx_battery_read_pwr_avg(struct bq27xxx_device_info *di)
{
	int tval;

	tval = bq27xxx_read(di, BQ27XXX_REG_AP, false);
	if (tval < 0) {
		dev_err(di->dev, "error reading average power register  %02x: %d\n",
			BQ27XXX_REG_AP, tval);
		return tval;
	}

	if (di->opts & BQ27XXX_O_ZERO)
		return (tval * BQ27XXX_POWER_CONSTANT) / BQ27XXX_RS;
	else
		return tval;
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
	else
		return flags & (BQ27XXX_FLAG_SOC1 | BQ27XXX_FLAG_SOCF);
}

/*
 * Read flag register.
 * Return < 0 if something fails.
 */
static int bq27xxx_battery_read_health(struct bq27xxx_device_info *di)
{
	int flags;
	bool has_singe_flag = di->opts & BQ27XXX_O_ZERO;

	flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, has_singe_flag);
	if (flags < 0) {
		dev_err(di->dev, "error reading flag register:%d\n", flags);
		return flags;
	}

	/* Unlikely but important to return first */
	if (unlikely(bq27xxx_battery_overtemp(di, flags)))
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (unlikely(bq27xxx_battery_undertemp(di, flags)))
		return POWER_SUPPLY_HEALTH_COLD;
	if (unlikely(bq27xxx_battery_dead(di, flags)))
		return POWER_SUPPLY_HEALTH_DEAD;

	return POWER_SUPPLY_HEALTH_GOOD;
}

void bq27xxx_battery_update(struct bq27xxx_device_info *di)
{
	struct bq27xxx_reg_cache cache = {0, };
	bool has_ci_flag = di->opts & BQ27XXX_O_ZERO;
	bool has_singe_flag = di->opts & BQ27XXX_O_ZERO;

	cache.flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, has_singe_flag);
	if ((cache.flags & 0xff) == 0xff)
		cache.flags = -1; /* read error */
	if (cache.flags >= 0) {
		cache.temperature = bq27xxx_battery_read_temperature(di);
		if (has_ci_flag && (cache.flags & BQ27000_FLAG_CI)) {
			dev_info_once(di->dev, "battery is not calibrated! ignoring capacity values\n");
			cache.capacity = -ENODATA;
			cache.energy = -ENODATA;
			cache.time_to_empty = -ENODATA;
			cache.time_to_empty_avg = -ENODATA;
			cache.time_to_full = -ENODATA;
			cache.charge_full = -ENODATA;
			cache.health = -ENODATA;
		} else {
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
			cache.health = bq27xxx_battery_read_health(di);
		}
		if (di->regs[BQ27XXX_REG_CYCT] != INVALID_REG_ADDR)
			cache.cycle_count = bq27xxx_battery_read_cyct(di);
		if (di->regs[BQ27XXX_REG_AP] != INVALID_REG_ADDR)
			cache.power_avg = bq27xxx_battery_read_pwr_avg(di);

		/* We only have to read charge design full once */
		if (di->charge_design_full <= 0)
			di->charge_design_full = bq27xxx_battery_read_dcap(di);
	}

	/* Lifetime data table read */
	if (di->opts & BQ27XXX_O_Z)
		bq27xxx_update_lifetime(di);

	if (di->cache.capacity != cache.capacity)
		power_supply_changed(di->bat);

	if (memcmp(&di->cache, &cache, sizeof(cache)) != 0)
		di->cache = cache;

	di->last_update = jiffies;
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_update);

static void bq27xxx_battery_poll(struct work_struct *work)
{
	struct bq27xxx_device_info *di =
			container_of(work, struct bq27xxx_device_info,
				     work.work);

	bq27xxx_battery_update(di);

	if (poll_interval > 0)
		schedule_delayed_work(&di->work, poll_interval * HZ);
}

/*
 * Return the battery average current in µA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27xxx_battery_current(struct bq27xxx_device_info *di,
				   union power_supply_propval *val)
{
	int curr;
	int flags;

	curr = bq27xxx_read(di, BQ27XXX_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}

	if (di->opts & BQ27XXX_O_ZERO) {
		flags = bq27xxx_read(di, BQ27XXX_REG_FLAGS, true);
		if (flags & BQ27000_FLAG_CHGS) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}

		val->intval = curr * BQ27XXX_CURRENT_CONSTANT / BQ27XXX_RS;
	} else {
		/* Other gauges return signed value */
		val->intval = (int)((s16)curr) * 1000;
	}

	return 0;
}

static int bq27xxx_battery_status(struct bq27xxx_device_info *di,
				  union power_supply_propval *val)
{
	int status;

	if (di->opts & BQ27XXX_O_ZERO) {
		if (di->cache.flags & BQ27000_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27000_FLAG_CHGS)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else if (power_supply_am_i_supplied(di->bat) > 0)
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else if (di->opts & BQ27XXX_O_Z) {
		if (di->cache.flags & BQ27Z561_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27Z561_FLAG_DSG)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (di->cache.flags & BQ27XXX_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27XXX_FLAG_DSC)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	}

	val->intval = status;

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
	int ret;
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

static int bq27z561_get_soh(struct bq27xxx_device_info *di,
				union power_supply_propval *val)
{
	int reg_data;

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_SOH_ADDR);
	if (reg_data < 0) {
		dev_err(di->dev, "get soh error\n");
		return reg_data;
	}

	val->intval = reg_data;
	return 0;
}

static int bq27z561_get_inter_temp(struct bq27xxx_device_info *di,
				union power_supply_propval *val)
{
	int reg_data;

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_INTER_TEMP_ADDR);
	if (reg_data < 0) {
		dev_err(di->dev, "get internal temp error\n");
		return reg_data;
	}

	/* return signed value */
	val->intval = reg_data;
	return 0;
}

static int bq27z561_get_remaining_capacity(struct bq27xxx_device_info *di,
				union power_supply_propval *val)
{
	int reg_data;

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_REMAIN_CAPACITY_ADDR);
	if (reg_data < 0) {
		dev_err(di->dev, "get remaining capacity error\n");
		return reg_data;
	}

	val->intval = reg_data;
	return 0;
}

static int bq27z561_get_manufacturer_info(struct bq27xxx_device_info *di,
				enum bq27xxx_manufacturer_info_type type,
				union power_supply_propval *val)
{
	int ret;
	char buf[BQ27Z561_MAC_BLOCK_LEN + 1];
	char tmp_str[7];

	val->strval = NULL;
	memset(di->mac_buf, 0x00, BQ27Z561_MAC_LEN);
	mutex_lock(&bq27xxx_list_lock);
	ret = bq27z561_battery_read_mac_block(di,
			BQ27Z561_MAC_CMD_MI_A, buf,
			BQ27Z561_MAC_BLOCK_LEN + 1);
	if (ret < 0) {
		dev_err(di->dev, "get manufacturer info error\n");
		mutex_unlock(&bq27xxx_list_lock);
		return ret;
	}

	if (type == MANUFACTURER_INFO_A) {
		strlcat(di->mac_buf, buf, BQ27Z561_MAC_LEN);
		val->strval = di->mac_buf;
		mutex_unlock(&bq27xxx_list_lock);
		return ret;
	} else if (type == MANUFACTURER_INFO_B &&
		strnstr(buf, ARC_BATTERY_HEAD, 6)) {
		ret = bq27z561_battery_read_mac_block(di,
				BQ27Z561_MAC_CMD_MI_B, buf,
				BQ27Z561_MAC_BLOCK_LEN + 1);
		if (ret < 0) {
			dev_err(di->dev, "get manufacturer info error\n");
			mutex_unlock(&bq27xxx_list_lock);
			return ret;
		}
		/* Refer to Battery Pack Genealogy ERS document
		 * (https://fburl.com/diff/uo2prae3) for more information.
		 *
		 * The first three bytes of manufacturer info block B are hex
		 * convert them to string after read these three bytes out
		 */
		if (buf[0] < '0') {
			memset(tmp_str, 0x00, sizeof(tmp_str));
			snprintf(tmp_str, sizeof(tmp_str) - 1, "%02X%02X%02X",
					buf[0], buf[1], buf[2]);
			strlcat(di->mac_buf, tmp_str, BQ27Z561_MAC_LEN);
			strlcat(di->mac_buf, buf + 3, BQ27Z561_MAC_LEN);
		} else
			strlcat(di->mac_buf, buf, BQ27Z561_MAC_LEN);
		val->strval = di->mac_buf;
		mutex_unlock(&bq27xxx_list_lock);
		return ret;
	} else if (type == MANUFACTURER_INFO_C &&
		strnstr(buf, ARC_BATTERY_HEAD, 6)) {
		ret = bq27z561_battery_read_mac_block(di,
				BQ27Z561_MAC_CMD_MI_C, buf,
				BQ27Z561_MAC_BLOCK_LEN + 1);
		if (ret < 0) {
			dev_err(di->dev, "get manufacturer info error\n");
			mutex_unlock(&bq27xxx_list_lock);
			return ret;
		}
		strlcat(di->mac_buf, buf, BQ27Z561_MAC_LEN);
		val->strval = di->mac_buf;
	}
	mutex_unlock(&bq27xxx_list_lock);
	return 0;
}

static int bq27xxx_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);

	mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ)) {
		cancel_delayed_work_sync(&di->work);
		bq27xxx_battery_poll(&di->work.work);
	}
	mutex_unlock(&di->lock);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq27xxx_battery_status(di, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27xxx_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->cache.flags < 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq27xxx_battery_current(di, val);
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
		ret = bq27xxx_simple_value(di->cache.power_avg, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = bq27xxx_simple_value(di->cache.health, val);
		break;
	case POWER_SUPPLY_PROP_SOH:
		ret = bq27z561_get_soh(di, val);
		break;
	case POWER_SUPPLY_PROP_BQ27Z561_INTERTEMP:
		ret = bq27z561_get_inter_temp(di, val);
		break;
	case POWER_SUPPLY_PROP_BQ27Z561_REMAININGCAPACITY:
		ret = bq27z561_get_remaining_capacity(di, val);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = BQ27XXX_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER_INFO_A:
		ret = bq27z561_get_manufacturer_info(di, MANUFACTURER_INFO_A, val);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER_INFO_B:
		ret = bq27z561_get_manufacturer_info(di, MANUFACTURER_INFO_B, val);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER_INFO_C:
		ret = bq27z561_get_manufacturer_info(di, MANUFACTURER_INFO_C, val);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static void bq27xxx_external_power_changed(struct power_supply *psy)
{
	struct bq27xxx_device_info *di = power_supply_get_drvdata(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

#ifdef CONFIG_DEBUG_FS
static ssize_t reg_data_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bq27xxx_device_info *di = file->private_data;
	int reg_data;
	char val[10];

	reg_data = bq27z561_battery_read_reg(di, di->reg_addr);

	snprintf(val, sizeof(val) - 1, "0x:%02X\n", reg_data);

	return simple_read_from_buffer(user_buf, count, ppos,
		val, strlen(val));
}

static ssize_t reg_data_write_file(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long value;
	struct bq27xxx_device_info *di = file->private_data;
	int ret;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	while (*start == ' ')
		start++;
	if (kstrtoul(start, 16, &value))
		return -EINVAL;

	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_STILL_OK);

	ret = bq27z561_battery_write_reg(di, di->reg_addr, value);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t read_block_to_str(struct bq27xxx_device_info *di,
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

static ssize_t _debugfs_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos, u16 mac_cmd)
{
	struct bq27xxx_device_info *di = file->private_data;
	int ret;
	char val[80];

	ret = read_block_to_str(di, mac_cmd, val, sizeof(val));
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos,
		val, strlen(val));
}

static ssize_t _debugfs_write_file(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos, u16 mac_cmd)
{
	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long value;
	struct bq27xxx_device_info *di = file->private_data;
	int ret;
	u8 command[2];

	command[0] = mac_cmd & 0x00FF;
	command[1] = (mac_cmd & 0xFF00) >> 8;

	buf_size = min(count, (sizeof(buf) - 1));

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = '\0';

	while (*start == ' ')
		start++;
	if (*start == '\0')
		return -EINVAL;
	if (kstrtoul(start, 10, &value))
		return -EINVAL;
	if (value != 1)
		return -EINVAL;
	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_STILL_OK);

	ret = bq27z561_write_block(di, BQ27Z561_MAC_CMD_ADDR, command, 2);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t mac_data_read_file(struct file *file, char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct bq27xxx_device_info *di = file->private_data;
	int ret;
	char val[80];

	if (di->reg_addr != BQ27Z561_MAC_CMD_ADDR)
		return -EINVAL;

	ret = read_block_to_str(di, di->reg_data, val, sizeof(val));
	if (ret < 0)
		return ret;
	dev_dbg(di->dev, "manufacturer data string lens : %s\n", val);

	return simple_read_from_buffer(user_buf, count, ppos,
		val, strlen(val));
}

static ssize_t mac_data_write_file(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t at_rate_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bq27xxx_device_info *di = file->private_data;
	int reg_data;
	char val[10];

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_ATRATE_ADDR);

	snprintf(val, sizeof(val) - 1, "%d\n", reg_data);

	return simple_read_from_buffer(user_buf, count, ppos,
		val, strlen(val));
}

static ssize_t at_rate_write_file(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long value;
	struct bq27xxx_device_info *di = file->private_data;
	int ret;
	bool negative;

	buf_size = min(count, (sizeof(buf)-1));

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = '\0';

	while (*start == ' ')
		start++;
	if (*start == '-') {
		negative = true;
		start++;
	} else
		negative = false;
	if (*start == '\0')
		return -EINVAL;
	if (kstrtoul(start, 10, &value))
		return -EINVAL;
	if (negative)
		value = -1 * value;
	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_STILL_OK);

	ret = bq27z561_battery_write_reg(di, BQ27Z561_ATRATE_ADDR, value);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t at_rate_time_to_empty_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bq27xxx_device_info *di = file->private_data;
	int reg_data;
	char val[10];

	reg_data = bq27z561_battery_read_reg(di, BQ27Z561_ATRATETTE_ADDR);

	snprintf(val, sizeof(val) - 1, "%d\n", reg_data);

	return simple_read_from_buffer(user_buf, count, ppos,
		val, strlen(val));
}

static ssize_t device_type_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_DT);
}

static ssize_t firmware_version_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_FV);
}

static ssize_t hardware_version_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_HV);
}

static ssize_t instruction_flash_signature_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_IFS);
}

static ssize_t static_df_signature_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_SDFS);
}

static ssize_t chemical_id_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_CID);
}

static ssize_t static_chem_df_signature_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_SCDFS);
}

static ssize_t all_df_signature_read_file(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_ADFS);
}

static ssize_t device_reset_write_file(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	return _debugfs_write_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_RESET);
}

static ssize_t lifetime_data_collection_write_file(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	int ret;
	int status;
	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long value;
	struct bq27xxx_device_info *di = file->private_data;
	char mac_buf[40];
	u8 command[2];

	command[0] = BQ27Z561_MAC_CMD_LDC & 0x00FF;
	command[1] = (BQ27Z561_MAC_CMD_LDC & 0xFF00) >> 8;

	buf_size = min(count, (sizeof(buf) - 1));

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = '\0';

	while (*start == ' ')
		start++;
	if (*start == '\0')
		return -EINVAL;
	if (kstrtoul(start, 10, &value))
		return -EINVAL;
	if (value != 1 || value != 0)
		return -EINVAL;

	/*   read Manufacturing Status to check Lifetime Data Collection*/
	ret = bq27z561_battery_read_mac_block(di, BQ27Z561_MAC_CMD_MS,
			mac_buf, sizeof(mac_buf));
	if (ret < 0)
		return ret;
	/* LF_EN bit:5 of Manufacturing Status*/
	status = mac_buf[0] & 0x20;
	if (status == value)
		return buf_size;

	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_STILL_OK);

	ret = bq27z561_write_block(di, BQ27Z561_MAC_CMD_ADDR, command, 2);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t lifetime_data_reset_write_file(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	return _debugfs_write_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_LDR);
}

static ssize_t seal_device_read_file(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct bq27xxx_device_info *di = file->private_data;
	int ret;
	int sealed_status;
	char val[10];
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

	snprintf(val, sizeof(val) - 1, "%s\n",
			bq27z561_sealed_status_str[sealed_status]);

	return simple_read_from_buffer(user_buf, count, ppos,
		val, strlen(val));
}

static ssize_t seal_device_write_file(struct file *file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	return _debugfs_write_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_SD);
}

static ssize_t device_name_read_file(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_DN);
}

static ssize_t device_chem_read_file(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_DC);
}

static ssize_t manufacturer_name_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_MN);
}

static ssize_t manufacturer_date_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_MD);
}

static ssize_t serial_number_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_SN);
}

static ssize_t operation_status_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_OS);
}

static ssize_t charging_status_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_CS);
}

static ssize_t gauging_status_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_GS);
}

static ssize_t manufacturing_status_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_MS);
}

static ssize_t lifetime_data_block_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_LDB1);
}

static ssize_t manufacturer_info_a_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_MI_A);
}

static ssize_t manufacturer_info_b_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_MI_B);
}

static ssize_t manufacturer_info_c_read_file(struct file *file,
				char __user *user_buf,
				size_t count, loff_t *ppos)
{
	return _debugfs_read_file(file, user_buf, count, ppos,
				BQ27Z561_MAC_CMD_MI_C);
}

static const struct file_operations reg_data_fops = {
	.open = simple_open,
	.read = reg_data_read_file,
	.write = reg_data_write_file,
};

static const struct file_operations mac_data_fops = {
	.open = simple_open,
	.read = mac_data_read_file,
	.write = mac_data_write_file,
};

static const struct file_operations at_rate_fops = {
	.open = simple_open,
	.read = at_rate_read_file,
	.write = at_rate_write_file,
};

static const struct file_operations at_rate_time_to_empty_fops = {
	.open = simple_open,
	.read = at_rate_time_to_empty_read_file,
};

static const struct file_operations device_type_fops = {
	.open = simple_open,
	.read = device_type_read_file,
};

static const struct file_operations firmware_version_fops = {
	.open = simple_open,
	.read = firmware_version_read_file,
};

static const struct file_operations hardware_version_fops = {
	.open = simple_open,
	.read = hardware_version_read_file,
};

static const struct file_operations instruction_flash_signature_fops = {
	.open = simple_open,
	.read = instruction_flash_signature_read_file,
};

static const struct file_operations static_df_signature_fops = {
	.open = simple_open,
	.read = static_df_signature_read_file,
};

static const struct file_operations chemical_id_fops = {
	.open = simple_open,
	.read = chemical_id_read_file,
};

static const struct file_operations static_chem_df_signature_fops = {
	.open = simple_open,
	.read = static_chem_df_signature_read_file,
};

static const struct file_operations all_df_signature_fops = {
	.open = simple_open,
	.read = all_df_signature_read_file,
};

static const struct file_operations device_reset_fops = {
	.open = simple_open,
	.write = device_reset_write_file,
};

static const struct file_operations lifetime_data_collection_fops = {
	.open = simple_open,
	.write = lifetime_data_collection_write_file,
};

static const struct file_operations lifetime_data_reset_fops = {
	.open = simple_open,
	.write = lifetime_data_reset_write_file,
};

static const struct file_operations seal_device_fops = {
	.open = simple_open,
	.read = seal_device_read_file,
	.write = seal_device_write_file,
};

static const struct file_operations device_name_fops = {
	.open = simple_open,
	.read = device_name_read_file,
};

static const struct file_operations device_chem_fops = {
	.open = simple_open,
	.read = device_chem_read_file,
};

static const struct file_operations manufacturer_name_fops = {
	.open = simple_open,
	.read = manufacturer_name_read_file,
};

static const struct file_operations manufacturer_date_fops = {
	.open = simple_open,
	.read = manufacturer_date_read_file,
};

static const struct file_operations serial_number_fops = {
	.open = simple_open,
	.read = serial_number_read_file,
};

static const struct file_operations operation_status_fops = {
	.open = simple_open,
	.read = operation_status_read_file,
};

static const struct file_operations charging_status_fops = {
	.open = simple_open,
	.read = charging_status_read_file,
};

static const struct file_operations gauging_status_fops = {
	.open = simple_open,
	.read = gauging_status_read_file,
};

static const struct file_operations manufacturing_status_fops = {
	.open = simple_open,
	.read = manufacturing_status_read_file,
};

static const struct file_operations lifetime_data_block_fops = {
	.open = simple_open,
	.read = lifetime_data_block_read_file,
};

static const struct file_operations manufacturer_info_a_fops = {
	.open = simple_open,
	.read = manufacturer_info_a_read_file,
};

static const struct file_operations manufacturer_info_b_fops = {
	.open = simple_open,
	.read = manufacturer_info_b_read_file,
};

static const struct file_operations manufacturer_info_c_fops = {
	.open = simple_open,
	.read = manufacturer_info_c_read_file,
};

static void bq27z561_create_debugfs(struct bq27xxx_device_info *di)
{
	di->debugfs = debugfs_create_dir("bq27z561", NULL);
	if (IS_ERR_OR_NULL(di->debugfs)) {
		if (PTR_ERR(di->debugfs) == -ENODEV)
			pr_err("debugfs is not enabled in the kernel\n");
		else
			pr_err("error creating bq27z561 debugfs rc=%ld\n",
			       (long)di->debugfs);
	}

	debugfs_create_x8("address", 0600, di->debugfs,
				    &di->reg_addr);
	debugfs_create_file("data", 0600, di->debugfs,
				    di, &reg_data_fops);
	debugfs_create_x16("mac_cmd", 0600, di->debugfs,
				    &di->reg_data);
	debugfs_create_file("mac_data", 0600, di->debugfs,
				    di, &mac_data_fops);
	debugfs_create_file("at_rate", 0600, di->debugfs,
				    di, &at_rate_fops);
	debugfs_create_file("at_rate_time_to_empty", 0600, di->debugfs,
				di, &at_rate_time_to_empty_fops);
	debugfs_create_file("device_type", 0400, di->debugfs,
				di, &device_type_fops);
	debugfs_create_file("firmware_version", 0400, di->debugfs,
					di, &firmware_version_fops);
	debugfs_create_file("hardware_version", 0400, di->debugfs,
					di, &hardware_version_fops);
	debugfs_create_file("instruction_flash_signature", 0400, di->debugfs,
					di, &instruction_flash_signature_fops);
	debugfs_create_file("static_df_signature", 0400, di->debugfs,
					di, &static_df_signature_fops);
	debugfs_create_file("chemical_id", 0400, di->debugfs,
					di, &chemical_id_fops);
	debugfs_create_file("static_chem_df_signature", 0400, di->debugfs,
					di, &static_chem_df_signature_fops);
	debugfs_create_file("all_df_signature", 0400, di->debugfs,
					di, &all_df_signature_fops);
	debugfs_create_file("device_reset", 0200, di->debugfs,
					di, &device_reset_fops);
	debugfs_create_file("lifetime_data_collection", 0200, di->debugfs,
					di, &lifetime_data_collection_fops);
	debugfs_create_file("lifetime_data_reset", 0200, di->debugfs,
					di, &lifetime_data_reset_fops);
	debugfs_create_file("seal_device", 0600, di->debugfs,
					di, &seal_device_fops);
	debugfs_create_file("device_name", 0400, di->debugfs,
					di, &device_name_fops);
	debugfs_create_file("device_chem", 0400, di->debugfs,
					di, &device_chem_fops);
	debugfs_create_file("manufacturer_name", 0400, di->debugfs,
					di, &manufacturer_name_fops);
	debugfs_create_file("manufacturer_date", 0400, di->debugfs,
					di, &manufacturer_date_fops);
	debugfs_create_file("serial_number", 0400, di->debugfs,
					di, &serial_number_fops);
	debugfs_create_file("operation_status", 0400, di->debugfs,
					di, &operation_status_fops);
	debugfs_create_file("charging_status", 0400, di->debugfs,
					di, &charging_status_fops);
	debugfs_create_file("gauging_status", 0400, di->debugfs,
					di, &gauging_status_fops);
	debugfs_create_file("manufacturing_status", 0400, di->debugfs,
					di, &manufacturing_status_fops);
	debugfs_create_file("lifetime_data_block", 0400, di->debugfs,
					di, &lifetime_data_block_fops);
	debugfs_create_file("manufacturer_info_a", 0400, di->debugfs,
					di, &manufacturer_info_a_fops);
	debugfs_create_file("manufacturer_info_b", 0400, di->debugfs,
					di, &manufacturer_info_b_fops);
	debugfs_create_file("manufacturer_info_c", 0400, di->debugfs,
					di, &manufacturer_info_c_fops);

	/* lifetime nodes */
	debugfs_create_u16("cell_1_max_voltage", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_lower[0]);
	debugfs_create_u16("cell_1_min_voltage", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_lower[1]);
	debugfs_create_x16("max_charge_current", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_lower[2]);
	debugfs_create_x16("max_discharge_current", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_lower[3]);
	debugfs_create_x16("max_avg_discharge_current", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_lower[4]);
	debugfs_create_x16("max_avg_dsg_power", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_lower[5]);
	debugfs_create_x8("max_temp_cell", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_higher[0]);
	debugfs_create_x8("min_temp_cell", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_higher[1]);
	debugfs_create_x8("max_temp_int_sensor", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_higher[2]);
	debugfs_create_x8("min_temp_int_sensor", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime1_higher[3]);

	debugfs_create_u32("total_fw_runtime", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime3);

	debugfs_create_u16("num_valid_charge_terminations", 0400,
			di->debugfs, &di->lifetime_blocks.lifetime4[0]);
	debugfs_create_u16("last_valid_charge_term", 0400,
			di->debugfs, &di->lifetime_blocks.lifetime4[1]);
	debugfs_create_u16("num_qmax_updates", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime4[2]);
	debugfs_create_u16("last_qmax_update", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime4[3]);
	debugfs_create_u16("num_ra_update", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime4[4]);
	debugfs_create_u16("last_ra_update", 0400, di->debugfs,
			&di->lifetime_blocks.lifetime4[5]);

	debugfs_create_u32("t_ut_rsoc_a", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][0]);
	debugfs_create_u32("t_ut_rsoc_b", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][1]);
	debugfs_create_u32("t_ut_rsoc_c", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][2]);
	debugfs_create_u32("t_ut_rsoc_d", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][3]);
	debugfs_create_u32("t_ut_rsoc_e", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][4]);
	debugfs_create_u32("t_ut_rsoc_f", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][5]);
	debugfs_create_u32("t_ut_rsoc_g", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][6]);
	debugfs_create_u32("t_ut_rsoc_h", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[0][7]);
	debugfs_create_u32("t_lt_rsoc_a", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][0]);
	debugfs_create_u32("t_lt_rsoc_b", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][1]);
	debugfs_create_u32("t_lt_rsoc_c", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][2]);
	debugfs_create_u32("t_lt_rsoc_d", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][3]);
	debugfs_create_u32("t_lt_rsoc_e", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][4]);
	debugfs_create_u32("t_lt_rsoc_f", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][5]);
	debugfs_create_u32("t_lt_rsoc_g", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][6]);
	debugfs_create_u32("t_lt_rsoc_h", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[1][7]);
	debugfs_create_u32("t_stl_rsoc_a", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][0]);
	debugfs_create_u32("t_stl_rsoc_b", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][1]);
	debugfs_create_u32("t_stl_rsoc_c", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][2]);
	debugfs_create_u32("t_stl_rsoc_d", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][3]);
	debugfs_create_u32("t_stl_rsoc_e", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][4]);
	debugfs_create_u32("t_stl_rsoc_f", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][5]);
	debugfs_create_u32("t_stl_rsoc_g", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][6]);
	debugfs_create_u32("t_stl_rsoc_h", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[2][7]);
	debugfs_create_u32("t_rt_rsoc_a", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][0]);
	debugfs_create_u32("t_rt_rsoc_b", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][1]);
	debugfs_create_u32("t_rt_rsoc_c", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][2]);
	debugfs_create_u32("t_rt_rsoc_d", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][3]);
	debugfs_create_u32("t_rt_rsoc_e", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][4]);
	debugfs_create_u32("t_rt_rsoc_f", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][5]);
	debugfs_create_u32("t_rt_rsoc_g", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][6]);
	debugfs_create_u32("t_rt_rsoc_h", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[3][7]);
	debugfs_create_u32("t_sth_rsoc_a", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][0]);
	debugfs_create_u32("t_sth_rsoc_b", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][1]);
	debugfs_create_u32("t_sth_rsoc_c", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][2]);
	debugfs_create_u32("t_sth_rsoc_d", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][3]);
	debugfs_create_u32("t_sth_rsoc_e", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][4]);
	debugfs_create_u32("t_sth_rsoc_f", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][5]);
	debugfs_create_u32("t_sth_rsoc_g", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][6]);
	debugfs_create_u32("t_sth_rsoc_h", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[4][7]);
	debugfs_create_u32("t_ht_rsoc_a", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][0]);
	debugfs_create_u32("t_ht_rsoc_b", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][1]);
	debugfs_create_u32("t_ht_rsoc_c", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][2]);
	debugfs_create_u32("t_ht_rsoc_d", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][3]);
	debugfs_create_u32("t_ht_rsoc_e", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][4]);
	debugfs_create_u32("t_ht_rsoc_f", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][5]);
	debugfs_create_u32("t_ht_rsoc_g", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][6]);
	debugfs_create_u32("t_ht_rsoc_h", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[5][7]);
	debugfs_create_u32("t_ot_rsoc_a", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][0]);
	debugfs_create_u32("t_ot_rsoc_b", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][1]);
	debugfs_create_u32("t_ot_rsoc_c", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][2]);
	debugfs_create_u32("t_ot_rsoc_d", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][3]);
	debugfs_create_u32("t_ot_rsoc_e", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][4]);
	debugfs_create_u32("t_ot_rsoc_f", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][5]);
	debugfs_create_u32("t_ot_rsoc_g", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][6]);
	debugfs_create_u32("t_ot_rsoc_h", 0400, di->debugfs,
			&di->lifetime_blocks.temp_zones[6][7]);
}

static void bq27z561_remove_debugfs(struct bq27xxx_device_info *di)
{
	debugfs_remove_recursive(di->debugfs);
}
#endif

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
	if (IS_ERR(di->bat)) {
		dev_err(di->dev, "failed to register battery\n");
		return PTR_ERR(di->bat);
	}

	di->mac_buf = kmalloc(BQ27Z561_MAC_LEN, GFP_KERNEL);
	if (!di->mac_buf)
		return -ENOMEM;

#ifdef CONFIG_DEBUG_FS
	bq27z561_create_debugfs(di);
#endif

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
	/*
	 * power_supply_unregister call bq27xxx_battery_get_property which
	 * call bq27xxx_battery_poll.
	 * Make sure that bq27xxx_battery_poll will not call
	 * schedule_delayed_work again after unregister (which cause OOPS).
	 */
	poll_interval = 0;

	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(di->bat);

	kfree(di->mac_buf);

#ifdef CONFIG_DEBUG_FS
	bq27z561_remove_debugfs(di);
#endif

	mutex_lock(&bq27xxx_list_lock);
	list_del(&di->list);
	mutex_unlock(&bq27xxx_list_lock);

	mutex_destroy(&di->lock);
}
EXPORT_SYMBOL_GPL(bq27xxx_battery_teardown);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27xxx battery monitor driver");
MODULE_LICENSE("GPL");
