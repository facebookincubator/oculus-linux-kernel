/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_BQ27X00_BATTERY_H__
#define __LINUX_BQ27X00_BATTERY_H__

enum bq27xxx_chip {
	BQ27000 = 1, /* bq27000, bq27200 */
	BQ27010, /* bq27010, bq27210 */
	BQ2750X, /* bq27500 deprecated alias */
	BQ2751X, /* bq27510, bq27520 deprecated alias */
	BQ2752X,
	BQ27500, /* bq27500/1 */
	BQ27510G1, /* bq27510G1 */
	BQ27510G2, /* bq27510G2 */
	BQ27510G3, /* bq27510G3 */
	BQ27520G1, /* bq27520G1 */
	BQ27520G2, /* bq27520G2 */
	BQ27520G3, /* bq27520G3 */
	BQ27520G4, /* bq27520G4 */
	BQ27521, /* bq27521 */
	BQ27530, /* bq27530, bq27531 */
	BQ27531,
	BQ27541, /* bq27541, bq27542, bq27546, bq27742 */
	BQ27542,
	BQ27546,
	BQ27742,
	BQ27545, /* bq27545 */
	BQ27421, /* bq27421, bq27441, bq27621 */
	BQ27425,
	BQ27426,
	BQ27441,
	BQ27621,
	BQ27Z561,
};

struct bq27xxx_device_info;
struct bq27xxx_access_methods {
	int (*read)(struct bq27xxx_device_info *di, u8 reg, bool single);
	int (*write)(struct bq27xxx_device_info *di, u8 reg, int value, bool single);
	int (*read_bulk)(struct bq27xxx_device_info *di, u8 reg, u8 *data, int len);
	int (*write_bulk)(struct bq27xxx_device_info *di, u8 reg, u8 *data, int len);
};

struct bq27xxx_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int energy;
	int flags;
	int power_avg;
	int health;
};

#define BQ27XXX_MAC_LDB1 0x60
#define BQ27XXX_MAC_LDB3 0x62
#define BQ27XXX_MAC_LDB4 0x63
#define BQ27XXX_MAC_LDB6 0x65
#define BQ27XXX_LIFETIME_1_LOWER_LEN 6
#define BQ27XXX_LIFETIME_1_HIGHER_LEN 4
#define BQ27XXX_LIFETIME_4_LEN 6
#define BQ27XXX_NUM_TEMP_ZONE 7
#define BQ27XXX_TEMP_ZONE_LEN 8
struct bq27xxx_reg_lifetime_blocks {
	u16 lifetime1_lower[BQ27XXX_LIFETIME_1_LOWER_LEN];
	u8 lifetime1_higher[BQ27XXX_LIFETIME_1_HIGHER_LEN];
	u32 lifetime3;
	u16 lifetime4[BQ27XXX_LIFETIME_4_LEN];
	u32 temp_zones[BQ27XXX_NUM_TEMP_ZONE][BQ27XXX_TEMP_ZONE_LEN];
};


#define BQ27XXX_MAX_FCT_TIME 2
#define BQ27XXX_MAX_FCT_STATE 6
#define BQ27XXX_MAX_FCT_WEIGHT 7
struct bq27xxx_device_info {
	struct device *dev;
	int id;
	enum bq27xxx_chip chip;
	u32 opts;
	const char *name;
	struct bq27xxx_dm_reg *dm_regs;
	u32 unseal_key;
	struct bq27xxx_access_methods bus;
	struct bq27xxx_reg_cache cache;
	int charge_design_full;
	unsigned long last_update;
	struct delayed_work work;
	struct power_supply *bat;
	struct list_head list;
	struct mutex lock;
	struct dentry *debugfs;
	struct bq27xxx_reg_lifetime_blocks lifetime_blocks;
	u8 *regs;
	u8 reg_addr;
	u16 reg_data;
	char *mac_buf;
	s64 fct; /*fct time in hours*/
	int fct_state; /*fct state defined in bq driver*/
	struct power_supply *bms_psy; /*fct profile node read */
	bool fct_config_valid; /* enable fct */
	u32 fct_missing_range[BQ27XXX_MAX_FCT_TIME];
	u32 fct_ok_range[BQ27XXX_MAX_FCT_TIME];
	u32 fct_warn_range_1[BQ27XXX_MAX_FCT_TIME];
	u32 fct_warn_range_2[BQ27XXX_MAX_FCT_TIME];
	u32 fct_warn_range_3[BQ27XXX_MAX_FCT_TIME];
	u32 fct_crit_range[BQ27XXX_MAX_FCT_TIME];
	u32 fct_ranges[BQ27XXX_MAX_FCT_STATE];
	u32 fct_weights[BQ27XXX_MAX_FCT_WEIGHT];
	u32 fct_weights_div[BQ27XXX_MAX_FCT_WEIGHT];
};

void bq27xxx_battery_update(struct bq27xxx_device_info *di);
int bq27xxx_battery_setup(struct bq27xxx_device_info *di);
void bq27xxx_battery_teardown(struct bq27xxx_device_info *di);

#endif
