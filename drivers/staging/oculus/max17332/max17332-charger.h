/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __MAX17332_CHARGER_H_
#define __MAX17332_CHARGER_H_

struct thermal_cooling_device;

#include "max17332-current-override.h"

/* Model Gauge Registers */
#define REG_DESIGNCAP          0x18
#define REG_MAXMINVOLT         0x08
#define REG_MAXMINCURR         0X0A

/* NVM Registers */
#define REG_NDESIGNCAP_NVM      0xB3
#define REG_NICHGCFG1_NVM       0xCE
#define REG_NICHGCFG2_NVM       0xCF
#define REG_NVCHGCFG1_NVM       0xCC
#define REG_NRSENSE_NVM         0x9C
#define REG_NSTEPCURR_NVM       0xC4

#define REG_NCHGCFG2_NVM        0xE4
#define REG_NCHGCFG2_NVM_DEFAULT 0x5800

/* nIChgCfg1 register bits for MAX17332 */
#define MAX17332_NICHGCFG1_ROOMCHARGINGI_POS 5
#define MAX17332_NICHGCFG1_ROOMCHARGINGI (0x3F << MAX17332_NICHGCFG1_ROOMCHARGINGI_POS)

/* nVChgCfg1 register bits for MAX17332 */
#define MAX17332_NVCHGCFG1_ROOMCHARGINGV_POS 4
#define MAX17332_NVCHGCFG1_ROOMCHARGINGV (0xFF << MAX17332_NVCHGCFG1_ROOMCHARGINGV_POS)

/* nDesignCap register bits for MAX17332 */
#define MAX17332_DESIGNCAP_QSCALE_POS 0
#define MAX17332_DESIGNCAP_QSCALE (0x7 << MAX17332_DESIGNCAP_QSCALE_POS)
#define MAX17332_DESIGNCAP_VSCALE_POS 3
#define MAX17332_DESIGNCAP_VSCALE (0x1 << MAX17332_DESIGNCAP_VSCALE_POS)
#define MAX17332_DESIGNCAP_DESIGNCAP_POS 6


/* ini version config */
#define MAX17332_NRSENSE_USERMEMORY_POS	8
#define MAX17332_INI_REV_REG			GENMASK(15, 8)

enum {
	MAX17332_CHG_CURRENT = 0,
	MAX17332_CHG_EN,
	MAX17332_CHG_VOLTAGE,
	MAX17332_CHG_BOB_VOLTAGE_DELTA,
	MAX17332_CHG_BOB_CURRENT_LIMIT,
};

ssize_t max17332_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t max17332_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define MAX17332_CHG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = max17332_chg_show_attrs,			\
	.store = max17332_chg_store_attrs,			\
}

struct max17332_charger_data {
	struct device           *dev;
	struct max17332_dev     *max17332;
	struct regmap			*regmap;
	struct regmap			*regmap_nvm;

	struct power_supply		*psy_chg;
	struct power_supply_desc		psy_chg_d;

	/* mutex */
	struct mutex			lock;

	int cycles_reg_lsb_percent;
	/* rsense */
	unsigned int rsense;

	/* power supply object handle for USB/one-wire charger */
	struct power_supply *usb_charger;

#ifdef CONFIG_MAX17332_CURRENT_OVERRIDE
	/* Charging current override struct */
	struct max17332_charger_current_override current_override;
#endif

	/* Thermal cooling device for charge throttling */
	struct thermal_cooling_device *cdev;
	unsigned long cooling_state;
	u16 manual_charge_voltage;
	u16 manual_charge_current;
};

int max17332_set_chg_current(struct max17332_charger_data *charger, unsigned int chg_current);
#endif // __MAX17332_CHARGER_H_
