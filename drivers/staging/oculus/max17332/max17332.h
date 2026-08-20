/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __MAX17332_MFD_H__
#define __MAX17332_MFD_H__

/* MAX17332  Top Devices */
#define MAX17332_NAME			"max17332"

/* MAX17332 Devices */
#define MAX17332_CHARGER_NAME				MAX17332_NAME "-charger"
#define MAX17332_BATTERY_NAME				MAX17332_NAME "-battery"
#define MAX17332_CDEV_NAME				MAX17332_NAME "-cdev"

#define MAX17332_NVM_BASE_ADDR 0x80
#define MAX17332_NVM_HIGH_ADDR 0xEF

/* Function Commands */
#define MAX17332_COMMAND_COPY_NVM (0xE904)
#define MAX17332_COMMAND_FULL_RESET (0x000F)
#define MAX17332_COMMAND_RECALL_HISTORY_REMAINING_WRITES (0xE29B)

#define REG_REPSOC          0x06
#define REG_COMMAND			0x60
#define REG_COMMSTAT		0x61
#define REG_CONFIG			0x0B
#define REG_CONFIG2			0xAB

#define REG_STATUS          0x00
#define REG_STATUS_MASK     0xFFEE

#define REG_PROTALRTS		0xAF
#define REG_PROTALRTS_MASK	0xFFFF
#define REG_PROTSTATUS      0xD9

#define REG_VCELLREP 0x12
#define REG_FPROTSTAT 0xDA

#define REG_QH				0x4D
#define REG_REPCAP    0x05
#define REG_FULLCAPREP	0x10
#define REG_CYCLES			0x17
#define REG_DEVNAME			0x21
#define DEVNAME_DEFAULT_VAL		0x4130

#define REG_NPROTCFG	0XD7

/* Nonvolatile Memory Registers */
#define REG_NROMID0_NVM		0xBC
#define REG_NROMID3_NVM		0xBF
#define REG_REMAINING_UPDATES_NVM	0xFD
#define REG_RSENSE_NVM 0x9C
#define REG_SLACK           0x6B

#define REG_N_FULL_CAP_NOM  0xA5
#define REG_N_FULL_CAP_REP  0xA9

/* ProtStatus register bits for MAX17332 */
#define BIT_SHDN_INT		BIT(0)
#define BIT_TOOCOLDD_INT	BIT(1)
#define BIT_ODCP_INT		BIT(2)
#define BIT_UVP_INT			BIT(3)
#define BIT_TOOHOTD_INT		BIT(4)
#define BIT_DIEHOT_INT		BIT(5)
#define BIT_PERMFAIL_INT	BIT(6)
#define BIT_PREQF_INT		BIT(8)
#define BIT_QOVFLW_INT		BIT(9)
#define BIT_OCCP_INT		BIT(10)
#define BIT_OVP_INT			BIT(11)
#define BIT_TOOCOLDC_INT	BIT(12)
#define BIT_FULL_INT		BIT(13)
#define BIT_TOOHOTC_INT		BIT(14)
#define BIT_CHGWDT_INT		BIT(15)

/* ProtAlrt register bits for MAX17332 */
#define BIT_PROTALRT_ODCP_INT		BIT(2)
#define BIT_PROTALRT_UVP_INT		BIT(3)
#define BIT_PROTALRT_TOOHOTD_INT	BIT(4)
#define BIT_PROTALRT_DIEHOT_INT		BIT(5)
#define BIT_PROTALRT_TEMPCHANGE_INT	BIT(6)
#define BIT_PROTALRT_QOVFLW_INT		BIT(9)
#define BIT_PROTALRT_OCCP_INT		BIT(10)
#define BIT_PROTALRT_OVP_INT		BIT(11)
#define BIT_PROTALRT_TOOCOLDC_INT	BIT(12)
#define BIT_PROTALRT_FULL_INT		BIT(13)
#define BIT_PROTALRT_TOOHOTC_INT	BIT(14)
#define BIT_PROTALRT_CHGWDT_INT		BIT(15)

/* Status register bits for MAX17332 */
#define BIT_STATUS_PA		BIT(15)
#define BIT_STATUS_SMX		BIT(14)
#define BIT_STATUS_TMX		BIT(13)
#define BIT_STATUS_VMX		BIT(12)
#define BIT_STATUS_CA		BIT(11)
#define BIT_STATUS_SMN		BIT(10)
#define BIT_STATUS_TMN		BIT(9)
#define BIT_STATUS_VMN		BIT(8)
#define BIT_STATUS_DSOCI	BIT(7)
#define BIT_STATUS_IMX		BIT(6)
#define BIT_STATUS_ALLOWCHGB	BIT(5)
#define BIT_STATUS_BST		BIT(3)
#define BIT_STATUS_IMN		BIT(2)
#define BIT_STATUS_POR		BIT(1)

/*ChgStat register bits for MAX17332 */
#define BIT_STATUS_DROPOUT	BIT(15)
#define BIT_STATUS_CP		BIT(3)
#define BIT_STATUS_CT		BIT(2)
#define BIT_STATUS_CC		BIT(1)
#define BIT_STATUS_CV		BIT(0)

/* CommStat register bits for MAX17332 */
#define MAX17332_COMMSTAT_NVERROR BIT(2)
#define MAX17332_COMMSTAT_NVBUSY BIT(3)
#define MAX17332_COMMSTAT_CHGOFF_POS 8
#define MAX17332_COMMSTAT_CHGOFF BIT(8)
#define MAX17332_COMMSTAT_DISOFF BIT(9)

#define MAX17332_COMMSTAT_WP_1_5_MASK 0x00F8
#define MAX17332_COMMSTAT_WP_GLOBAL_MASK 0x0001

/* CONFIG2 register bits for MAX17332*/
#define MAX17332_CONFIG2_POR_CMD BIT(15)

/* CONFIG register bits for MAX17332*/
// Manual Charging
#define MAX17332_CONFIG_MANCHG BIT(15)

/* nProtCfg register bits for MAX17332 */
#define MAX17332_NPROTCFG_CMOVRDEN_POS 10
#define MAX17332_NPROTCFG_CMOVRDEN BIT(10)

/* ProtStat register bits for MAX17332 */
#define MAX17332_PROTSTAT_ISDIS_POS 5
#define MAX17332_PROTSTAT_ISDIS BIT(5)

#if (IS_ENABLED(CONFIG_BATTERY_CAPACITY_EMULATION))
#define INVALID_CUSTOM_HEADROOM -1
#endif

/* Chip Interrupts */
enum {
	MAX17332_FG_POR_INT = 0,
	MAX17332_FG_IMN_INT,
	MAX17332_FG_BST_INT,
	MAX17332_FG_ALLOWCHGB_INT,
	MAX17332_FG_IMX_INT,
	MAX17332_FG_DSOCI_INT,
	MAX17332_FG_VMN_INT,
	MAX17332_FG_TMN_INT,
	MAX17332_FG_SMN_INT,
	MAX17332_FG_CA_INT,
	MAX17332_FG_VMX_INT,
	MAX17332_FG_TMX_INT,
	MAX17332_FG_SMX_INT,
	MAX17332_FG_PROT_INT,

	MAX17332_FG_PROT_INT_START,
	MAX17332_FG_PROT_INT_ODCP = MAX17332_FG_PROT_INT_START,
	MAX17332_FG_PROT_INT_UVP,
	MAX17332_FG_PROT_INT_TOOHOTD,
	MAX17332_FG_PROT_INT_DIEHOTD,
	MAX17332_FG_PROT_INT_PERMFAIL,
	MAX17332_FG_PROT_INT_QOVFLW,
	MAX17332_FG_PROT_INT_OCCP,
	MAX17332_FG_PROT_INT_OVP,
	MAX17332_FG_PROT_INT_TOOCOLDC,
	MAX17332_FG_PROT_INT_FULL,
	MAX17332_FG_PROT_INT_TOOHOTC,
	MAX17332_FG_PROT_INT_CHGWDT,

	MAX17332_NUM_OF_INTS,
};


/*******************************************************************************
 * Useful Macros
 ******************************************************************************/

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
		((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
						((_x) & 0x04 ? 2 : 3)) :\
		((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
						((_x) & 0x40 ? 6 : 7)))

#undef  FFS
#define FFS(_x) \
		((_x) ? __CONST_FFS(_x) : 0)

#undef  BIT_RSVD
#define BIT_RSVD  0

#undef  BITS
#define BITS(_end, _start) \
	((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_word, _mask, _shift) \
	(((_word) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_word, _bit) \
	__BITS_GET(_word, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_word, _mask, _shift, _val) \
	(((_word) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_word, _bit, _val) \
	__BITS_SET(_word, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_word, _bit) \
	(((_word) & (_bit)) == (_bit))

struct regulator_dev
{
	struct mutex lock;
	bool bob_enabled;
	struct regulator *charger_bob;
	int vout_step;
	int vout_max;
	int vout_min;
	int vout_delta;
	int round_up;
	int custom_headroom_mv;
};

struct max17332_dev {
	struct mutex				lock;
	struct mutex				lock_write;
	struct device				*dev;

	int					irq;
	int					irq_gpio;
	bool				in_active_discharge;

	struct regmap_irq_chip_data	*irqc_intsrc;

	struct i2c_client	*pmic;			/* 0x6C, MODEL GAUGE */
	struct i2c_client	*nvm;			/* 0x16, NVM */

	struct regmap		*regmap_pmic;			/* CHARGER */
	struct regmap		*regmap_nvm;			/* NVM */

	struct max17332_cache               *read_failure_cache;

	struct max17332_pmic_platform_data  *pdata;

	struct regulator_dev *regulator_dev;
	char charger_name[32];
	char battery_name[32];

	bool lock_en;

	atomic_t charge_throttled;

#ifdef CONFIG_MAX17332_VOLTAGE_ADJUSTMENT
	struct max17332_voltage_adjustment *voltage_adjustment;
#endif
};

/*******************************************************************************
 * Platform Data
 ******************************************************************************/

struct max17332_pmic_platform_data {
	int irq; /* system interrupt number for PMIC */
	u16 rsense;
};

/*******************************************************************************
 * Chip IO
 ******************************************************************************/
int max17332_read(struct regmap *regmap, u8 addr, u16 *val);
int max17332_write(struct regmap *regmap, u8 addr, u16 val);
int max17332_write_unlock(struct max17332_dev *max17332, struct regmap *regmap, u8 addr, u16 val);
int max17332_update_bits(struct regmap *regmap, u8 addr, u16 mask, u16 val);

int max17332_lock_write_protection(struct max17332_dev *dev, bool lock_en);

int max17332_headroom_management(struct max17332_dev *pdev);
int max17332_set_active_discharge(struct max17332_dev *pdev, bool enter);
int max17332_overcharge_protection(struct max17332_dev *pdev, int ocv_threshold_uv);
int max17332_get_charger_bob(struct max17332_dev *pdev);
int max17332_set_vsys_voltage(struct max17332_dev *pdev, int target_vol);
int max17332_raw_voltage_to_uvolts(u16 lsb);
bool is_max17332_charger_bob_active(struct max17332_dev *pdev);

int max17332_read_cached(struct regmap *regmap, struct max17332_cache *cache, u8 addr, u16 *val);

struct i2c_fails {
	int i2c_read_fail_count;
	int i2c_write_fail_count;
};

int max17332_get_i2c_read_fail_count(void);
int max17332_get_i2c_write_fail_count(void);

#endif /* !__MAX17332_MFD_H__ */
