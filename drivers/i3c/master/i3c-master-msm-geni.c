// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/i3c/master.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/msm-geni-se.h>
#include <linux/pinctrl/consumer.h>
#include <linux/ipc_logging.h>
#include <linux/pinctrl/qcom-pinctrl.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>

#define SE_I3C_SCL_HIGH			0x268
#define SE_I3C_TX_TRANS_LEN		0x26C
#define SE_I3C_RX_TRANS_LEN		0x270
#define SE_I3C_DELAY_COUNTER		0x274
#define SE_I2C_SCL_COUNTERS		0x278
#define SE_I3C_SCL_CYCLE		0x27C
#define SE_GENI_HW_IRQ_EN		0x920
#define SE_GENI_HW_IRQ_IGNORE_ON_ACTIVE	0x924
#define SE_GENI_HW_IRQ_CMD_PARAM_0	0x930
/* IBI_C registers */
#define IBI_GEN_CONFIG			0x0000
#define IBI_SCL_OD_TYPE			0x0004
#define IBI_SCL_PP_TIMING_CONFIG	0x0008
#define IBI_GPII_IBI_EN			0x000c
#define IBI_GEN_IRQ_STATUS		0x0010
#define IBI_GEN_IRQ_EN			0x0014
#define IBI_GEN_IRQ_CLR			0x0018
#define IBI_HW_PARAM			0x001c
#define IBI_HW_VERSION			0x0020
#define IBI_RX_DATA_DELAY		0x0024
#define IBI_UNEXPECT_IBI_INFO		0x0028
#define IBI_LEGACY_MODE			0x002c
#define IBI_SW_RESET			0x0030
#define IBI_TEST_BUS_SEL		0x0100
#define IBI_TEST_BUS_EN			0x0104
#define IBI_TEST_BUS_REG		0x0108
#define IBI_HW_EVENTS_MUX_CFG		0x010c
#define IBI_CHAR_CFG			0x0180
#define IBI_CHAR_DATA			0x0184
#define IBI_CHAR_OE			0x0188
#define IBI_CMD(n)			(0x1000 + (0x1000*n))
#define IBI_IRQ_STATUS(n)		(0x1004 + (0x1000*n))
#define IBI_IRQ_EN(n)			(0x1008 + (0x1000*n))
#define IBI_IRQ_CLR(n)			(0x100C + (0x1000*n))
#define IBI_RCVD_IBI_STATUS(n)		(0x1010 + (0x1000*n))
#define IBI_RCVD_IBI_CLR(n)		(0x1014 + (0x1000*n))
#define IBI_ALLOCATED_ENTRIES_GPII(n)	(0x1018 + (0x1000*n))
#define IBI_CONFIG_ENTRY(n, k)		(0x1800 + (0x1000*n) + (0x40*k))
#define IBI_RCVD_IBI_INFO_ENTRY(n, k)	(0x1804 + (0x1000*n) + (0x40*k))
#define IBI_RCVD_IBI_DATA_ENTRY(n, k)	(0x1808 + (0x1000*n) + (0x40*k))
#define IBI_RCVD_IBI_TS_LSB_ENTRY(n, k)	(0x180C + (0x1000*n) + (0x40*k))
#define IBI_RCVD_IBI_TS_MSB_ENTRY(n, k)	(0x1810 + (0x1000*n) + (0x40*k))

/* SE_GENI_M_CLK_CFG field shifts */
#define CLK_DEV_VALUE_SHFT	4
#define SER_CLK_EN_SHFT		0

/* SE_GENI_HW_IRQ_CMD_PARAM_0 field shifts */
#define M_IBI_IRQ_PARAM_7E_SHFT		0
#define M_IBI_IRQ_PARAM_STOP_STALL_SHFT	1

/* SE_I2C_SCL_COUNTERS field shifts */
#define I2C_SCL_HIGH_COUNTER_SHFT	20
#define I2C_SCL_LOW_COUNTER_SHFT	10

#define	SE_I3C_ERR  (M_CMD_OVERRUN_EN | M_ILLEGAL_CMD_EN | M_CMD_FAILURE_EN |\
	M_CMD_ABORT_EN | M_GP_IRQ_0_EN | M_GP_IRQ_1_EN | M_GP_IRQ_2_EN | \
	M_GP_IRQ_3_EN | M_GP_IRQ_4_EN)

/* M_CMD OP codes for I2C/I3C */
#define I3C_READ_IBI_HW			0
#define I2C_WRITE			1
#define I2C_READ			2
#define I2C_WRITE_READ			3
#define I2C_ADDR_ONLY			4
#define I3C_INBAND_RESET		5
#define I2C_BUS_CLEAR			6
#define I2C_STOP_ON_BUS			7
#define I3C_HDR_DDR_EXIT		8
#define I3C_PRIVATE_WRITE		9
#define I3C_PRIVATE_READ		10
#define I3C_HDR_DDR_WRITE		11
#define I3C_HDR_DDR_READ		12
#define I3C_DIRECT_CCC_ADDR_ONLY	13
#define I3C_BCAST_CCC_ADDR_ONLY		14
#define I3C_READ_IBI			15
#define I3C_BCAST_CCC_WRITE		16
#define I3C_DIRECT_CCC_WRITE		17
#define I3C_DIRECT_CCC_READ		18
/* M_CMD params for I3C */
#define PRE_CMD_DELAY		BIT(0)
#define TIMESTAMP_BEFORE	BIT(1)
#define STOP_STRETCH		BIT(2)
#define TIMESTAMP_AFTER		BIT(3)
#define POST_COMMAND_DELAY	BIT(4)
#define IGNORE_ADD_NACK		BIT(6)
#define READ_FINISHED_WITH_ACK	BIT(7)
#define CONTINUOUS_MODE_DAA	BIT(8)
#define SLV_ADDR_MSK		GENMASK(15, 9)
#define SLV_ADDR_SHFT		9
#define CCC_HDR_CMD_MSK		GENMASK(23, 16)
#define CCC_HDR_CMD_SHFT	16
#define IBI_NACK_TBL_CTRL	BIT(24)
#define USE_7E			BIT(25)
#define BYPASS_ADDR_PHASE	BIT(26)

/* IBI_HW_PARAM fields */
#define I3C_IBI_NUM_GPII_MSK	(GENMASK(11, 8))
#define I3C_IBI_NUM_GPII_SHFT	(8)
#define I3C_IBI_TABLE_DEPTH_MSK	(GENMASK(4, 0))

/* IBI_IRQ_STATUS(n) fields */
#define COMMAND_DONE			BIT(0)
#define CFG_TABLE_FULL			BIT(2)
#define IBI_RECEIVED			BIT(8)
#define ADDR_ASSOCIATED_W_OTHER_GPII	BIT(21)

/* IBI_GEN_IRQ_EN fields */
#define ENABLE_CHANGE_IRQ_EN		BIT(0)
#define UNEXPECT_IBI_ADDR_IRQ_EN	BIT(1)
#define HOT_JOIN_IRQ_EN			BIT(2)
#define SW_RESET_DONE_EN		BIT(3)
#define BUS_ERROR_EN			BIT(4)

/* IBI_IRQ_EN fields */
#define COMMAND_DONE_IRQ_EN		BIT(0)
#define INVALID_I3C_SLAVE_ADDR_IRQ_EN	BIT(1)
#define CFG_TABLE_FULL_IRQ_EN		BIT(2)
#define CFG_FAIL_IRQ_EN			BIT(3)
#define CFG_W_IBI_DIS_IRQ_EN		BIT(4)
#define IBI_RECEIVED_IRQ_EN		BIT(8)
#define CFG_FAIL_ZERO_NUM_MDB_EN	BIT(16)
#define CFG_FAIL_MASK_EN_DIFF_EN	BIT(17)
#define CFG_FAIL_NUM_MDB_DIFF_EN	BIT(18)
#define CFG_FAIL_NACK_DIFF_EN		BIT(19)
#define CFG_FAIL_STALL_DIFF_EN		BIT(20)
#define ADDR_ASSOCIATED_W_OTHER_GPII_EN	BIT(21)

/* Enable bits for GPIIn,  n:[0-11] */
#define GPIIn_IBI_EN(n)	      BIT(n)

/* IBI_CMD fields */
#define IBI_CMD_OPCODE        BIT(0)
#define I3C_SLAVE_RW          BIT(15)
#define STALL                 BIT(21)
#define I3C_SLAVE_ADDR_SHIFT  8
#define I3C_SLAVE_MASK        0x7f
#define NUM_OF_MDB_SHIFT      16
#define IBI_NUM_OF_MDB_MSK    GENMASK(18, 16)

/* IBI_GEN_CONFIG fields */
#define IBI_C_ENABLE	BIT(0)

/* IBI_CONFIG_ENTRY fields */
#define IBI_VALID	BIT(0)

#define SE_I3C_IBI_ERR  (INVALID_I3C_SLAVE_ADDR_IRQ_EN |\
			CFG_TABLE_FULL_IRQ_EN | CFG_FAIL_IRQ_EN |\
			CFG_W_IBI_DIS_IRQ_EN | CFG_FAIL_ZERO_NUM_MDB_EN |\
			CFG_FAIL_MASK_EN_DIFF_EN | CFG_FAIL_NUM_MDB_DIFF_EN |\
			CFG_FAIL_NACK_DIFF_EN | CFG_FAIL_STALL_DIFF_EN |\
			ADDR_ASSOCIATED_W_OTHER_GPII_EN)

enum geni_i3c_err_code {
	RD_TERM,
	NACK,
	CRC_ERR,
	BUS_PROTO,
	NACK_7E,
	NACK_IBI,
	GENI_OVERRUN,
	GENI_ILLEGAL_CMD,
	GENI_ABORT_DONE,
	GENI_TIMEOUT,
};

#define DM_I3C_CB_ERR   ((BIT(NACK) | BIT(BUS_PROTO) | BIT(NACK_7E)) << 5)

#define I3C_AUTO_SUSPEND_DELAY	250
#define KHZ(freq)		(1000 * freq)
#define PACKING_BYTES_PW	4
#define XFER_TIMEOUT		HZ
#define DFS_INDEX_MAX		7

#define I3C_DDR_READ_CMD BIT(7)
#define I3C_ADDR_MASK	0x7f
#define I3C_MAX_GPII_NUM 12
#define TLMM_I3C_MODE	0x24
#define IBI_SW_RESET_MIN_SLEEP 1000
#define IBI_SW_RESET_MAX_SLEEP 2000

#define MAX_I3C_SE		2

enum i3c_trans_dir {
	WRITE_TRANSACTION = 0,
	READ_TRANSACTION = 1
};

enum i3c_bus_phase {
	OPEN_DRAIN_MODE  = 0,
	PUSH_PULL_MODE   = 1
};

struct geni_se {
	void __iomem *base;
	void __iomem *ibi_base;
	struct device *dev;
	struct se_geni_rsc i3c_rsc;
};

struct rcvd_ibi_data {
	union {
		struct {
			u32 slave_add   : 7;
			u32 rw          : 1;
			u32 num_bytes   : 3;
			u32 resvd1      : 1;
			u32 nack        : 1;
			u32 resvd2      : 18;
			u32 valid       : 1;
		} fields;
		u32 info;
	} info;
	u32 ts;
	u32 payload;
};


struct geni_i3c_ver_info {
	int hw_major_ver;
	int hw_minor_ver;
	int hw_step_ver;
	int m_fw_ver;
	int s_fw_ver;
};

struct geni_ibi {
	bool hw_support;
	bool is_init;
	unsigned int num_slots;
	unsigned int num_gpi;
	struct i3c_dev_desc **slots;
	spinlock_t lock;
	int mngr_irq;
	struct completion done;
	int gpii_irq[I3C_MAX_GPII_NUM];
	int err;
	u8 ctrl_id;
	struct rcvd_ibi_data data;
};

struct geni_i3c_dev {
	struct geni_se se;
	unsigned int tx_wm;
	int irq;
	int err;
	struct i3c_master_controller ctrlr;
	void *ipcl;
	struct completion done;
	struct mutex lock;
	spinlock_t spinlock;
	u32 clk_src_freq;
	u32 dfs_idx;
	u8 *cur_buf;
	enum i3c_trans_dir cur_rnw;
	int cur_len;
	int cur_idx;
	unsigned long newaddrslots[(I3C_ADDR_MASK + 1) / BITS_PER_LONG];
	const struct geni_i3c_clk_fld *clk_fld;
	const struct geni_i3c_clk_fld *clk_od_fld;
	struct geni_ibi ibi;
	struct workqueue_struct *hj_wq;
	struct work_struct hj_wd;
	struct wakeup_source *hj_wl;
	struct pinctrl_state *i3c_gpio_disable;
	struct geni_i3c_ver_info ver_info;
};

struct geni_i3c_i2c_dev_data {
	u16 id;
	s16 ibi;
	struct i3c_generic_ibi_pool *ibi_pool;
};

struct i3c_xfer_params {
	enum se_xfer_mode mode;
	u32 m_cmd;
	u32 m_param;
};

struct geni_i3c_err_log {
	int err;
	const char *msg;
};

static struct geni_i3c_err_log gi3c_log[] = {
	[RD_TERM] = { -EINVAL, "I3C slave early read termination" },
	[NACK] = { -ENOTCONN, "NACK: slave unresponsive, check power/reset" },
	[CRC_ERR] = { -EINVAL, "CRC or parity error" },
	[BUS_PROTO] = { -EPROTO, "Bus proto err, noisy/unexpected start/stop" },
	[NACK_7E] = { -EBUSY, "NACK on 7E, unexpected protocol error" },
	[NACK_IBI] = { -EINVAL, "NACK on IBI" },
	[GENI_OVERRUN] = { -EIO, "Cmd overrun, check GENI cmd-state machine" },
	[GENI_ILLEGAL_CMD] = { -EILSEQ,
				"Illegal cmd, check GENI cmd-state machine" },
	[GENI_ABORT_DONE] = { -ETIMEDOUT, "Abort after timeout successful" },
	[GENI_TIMEOUT] = { -ETIMEDOUT, "I3C transaction timed out" },
};

struct geni_i3c_clk_fld {
	u32 clk_freq_out;
	u32 clk_src_freq;
	u8  clk_div;
	u8  i2c_t_high_cnt;
	u8  i2c_t_low_cnt;
	u8  i3c_t_high_cnt;
	u8  i3c_t_cycle_cnt;
	u32 i2c_t_cycle_cnt;
};

static void geni_i3c_enable_ibi_ctrl(struct geni_i3c_dev *gi3c, bool enable);
static void geni_i3c_enable_ibi_irq(struct geni_i3c_dev *gi3c, bool enable);

static struct geni_i3c_dev *i3c_geni_dev[MAX_I3C_SE];
static int i3c_nos;

static struct geni_i3c_dev*
to_geni_i3c_master(struct i3c_master_controller *master)
{
	return container_of(master, struct geni_i3c_dev, ctrlr);
}

/*
 * Hardware uses the underlying formula to calculate time periods of
 * SCL clock cycle. Firmware uses some additional cycles excluded from the
 * below formula and it is confirmed that the time periods are within
 * specification limits.
 *
 * time of high period of I2C SCL:
 *         i2c_t_high = (i2c_t_high_cnt * clk_div) / source_clock
 * time of low period of I2C SCL:
 *         i2c_t_low = (i2c_t_low_cnt * clk_div) / source_clock
 * time of full period of I2C SCL:
 *         i2c_t_cycle = (i2c_t_cycle_cnt * clk_div) / source_clock
 * time of high period of I3C SCL:
 *         i3c_t_high = (i3c_t_high_cnt * clk_div) / source_clock
 * time of full period of I3C SCL:
 *         i3c_t_cycle = (i3c_t_cycle_cnt * clk_div) / source_clock
 * clk_freq_out = t / t_cycle
 */
static const struct geni_i3c_clk_fld geni_i3c_clk_map[] = {
	{ KHZ(100),    19200,  7, 10, 11,  0,  0,  26},
	{ KHZ(400),    19200,  1, 72, 168, 6,  7, 300},
	{ KHZ(1000),   19200,  1,  3,  9,  7,  0,  18},
	{ KHZ(1920),   19200,  1,  4,  9,  7,  8,  19},
	{ KHZ(3500),   19200,  1, 72, 168, 3, 4,  300},
	{ KHZ(370),   100000, 20,  4,  7,  8, 14,  14},
	{ KHZ(12500), 100000,  1, 72, 168, 6,  7, 300},
};

static int geni_i3c_clk_map_idx(struct geni_i3c_dev *gi3c)
{
	int i;
	struct i3c_master_controller *m = &gi3c->ctrlr;
	const struct geni_i3c_clk_fld *itr = geni_i3c_clk_map;
	struct i3c_bus *bus = i3c_master_get_bus(m);

	for (i = 0; i < ARRAY_SIZE(geni_i3c_clk_map); i++, itr++) {
		if ((!bus ||
			 itr->clk_freq_out == bus->scl_rate.i3c) &&
			 KHZ(itr->clk_src_freq) == gi3c->clk_src_freq) {
			gi3c->clk_fld = itr;
		}

		if (itr->clk_freq_out == bus->scl_rate.i2c)
			gi3c->clk_od_fld = itr;
	}

	if ((!gi3c->clk_fld) || (!gi3c->clk_od_fld)) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"%s : clk mapping failed", __func__);
		return -EINVAL;
	}

	return 0;
}

static void set_new_addr_slot(unsigned long *addrslot, u8 addr)
{
	unsigned long *ptr;

	if (addr > I3C_ADDR_MASK)
		return;

	ptr = addrslot + (addr / BITS_PER_LONG);
	*ptr |= 1 << (addr % BITS_PER_LONG);
}

static void clear_new_addr_slot(unsigned long *addrslot, u8 addr)
{
	unsigned long *ptr;

	if (addr > I3C_ADDR_MASK)
		return;

	ptr = addrslot + (addr / BITS_PER_LONG);
	*ptr &= ~(1 << (addr % BITS_PER_LONG));
}

static bool is_new_addr_slot_set(unsigned long *addrslot, u8 addr)
{
	unsigned long *ptr;

	if (addr > I3C_ADDR_MASK)
		return false;

	ptr = addrslot + (addr / BITS_PER_LONG);
	return ((*ptr & (1 << (addr % BITS_PER_LONG))) != 0);
}

static void qcom_geni_i3c_conf(struct geni_i3c_dev *gi3c,
	enum i3c_bus_phase bus_phase)
{
	const struct geni_i3c_clk_fld *itr = gi3c->clk_fld;
	u32 val;
	unsigned long freq;
	int ret = 0;

	if (bus_phase == OPEN_DRAIN_MODE)
		itr = gi3c->clk_od_fld;

	ret = geni_se_clk_freq_match(&gi3c->se.i3c_rsc,
			KHZ(itr->clk_src_freq),
			&gi3c->dfs_idx, &freq, false);
	if (ret)
		gi3c->dfs_idx = 0;

	writel_relaxed(gi3c->dfs_idx, gi3c->se.base + SE_GENI_CLK_SEL);

	val = itr->clk_div << CLK_DEV_VALUE_SHFT;
	val |= 1 << SER_CLK_EN_SHFT;
	writel_relaxed(val, gi3c->se.base + GENI_SER_M_CLK_CFG);

	val = itr->i2c_t_high_cnt << I2C_SCL_HIGH_COUNTER_SHFT;
	val |= itr->i2c_t_low_cnt << I2C_SCL_LOW_COUNTER_SHFT;
	val |= itr->i2c_t_cycle_cnt;
	writel_relaxed(val, gi3c->se.base + SE_I2C_SCL_COUNTERS);

	writel_relaxed(itr->i3c_t_cycle_cnt, gi3c->se.base + SE_I3C_SCL_CYCLE);
	writel_relaxed(itr->i3c_t_high_cnt, gi3c->se.base + SE_I3C_SCL_HIGH);

}

static void geni_i3c_err(struct geni_i3c_dev *gi3c, int err)
{
	if (gi3c->cur_rnw == WRITE_TRANSACTION)
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"%s:Error: Write, len:%d\n", __func__, gi3c->cur_len);
	else
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"%s:Error: Read, len:%d\n", __func__, gi3c->cur_len);

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev, "%s\n", gi3c_log[err].msg);
	gi3c->err = gi3c_log[err].err;

	geni_se_dump_dbg_regs(&gi3c->se.i3c_rsc, gi3c->se.base, gi3c->ipcl);
}

static void geni_i3c_hotjoin(struct work_struct *work)
{
	int ret;
	struct geni_i3c_dev *gi3c =
			container_of(work, struct geni_i3c_dev, hj_wd);

	pm_stay_awake(gi3c->se.dev);

	ret = i3c_master_do_daa(&gi3c->ctrlr);
	if (ret)
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"hotjoin:daa failed %d\n", ret);

	pm_relax(gi3c->se.dev);
}

static void geni_i3c_handle_received_ibi(struct geni_i3c_dev *gi3c)
{
	struct geni_i3c_i2c_dev_data *data;
	struct i3c_ibi_slot *slot;
	struct i3c_dev_desc *dev = gi3c->ibi.slots[0];
	u32 val, i;

	val = readl_relaxed(gi3c->se.ibi_base + IBI_RCVD_IBI_STATUS(0));

	data = i3c_dev_get_master_data(dev);
	slot = i3c_generic_ibi_get_free_slot(data->ibi_pool);
	if (!slot) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev, "no free slot\n");
		goto no_free_slot;
	}

	for (i = 0; i < gi3c->ibi.num_slots; i++) {
		if (!(val & (1u << i)))
			continue;

		gi3c->ibi.data.info.info =
			readl_relaxed(gi3c->se.ibi_base
				+ IBI_RCVD_IBI_INFO_ENTRY(0, i));
		gi3c->ibi.data.ts =
			readl_relaxed(gi3c->se.ibi_base
				+ IBI_RCVD_IBI_TS_LSB_ENTRY(0, i));
		gi3c->ibi.data.payload =
			readl_relaxed(gi3c->se.ibi_base
				+ IBI_RCVD_IBI_DATA_ENTRY(0, i));

		if (slot->data)
			memcpy(slot->data, &gi3c->ibi.data.payload,
				dev->ibi->max_payload_len);

		slot->len = min_t(unsigned int,
				gi3c->ibi.data.info.fields.num_bytes,
				dev->ibi->max_payload_len);

		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"IBI: info: 0x%x, ts: 0x%x, Data: 0x%x\n",
			gi3c->ibi.data.info.info, gi3c->ibi.data.ts,
			gi3c->ibi.data.payload);
	}

	i3c_master_queue_ibi(dev, slot);
no_free_slot:
	writel_relaxed(val, gi3c->se.ibi_base + IBI_RCVD_IBI_CLR(0));
}

static irqreturn_t geni_i3c_ibi_irq(int irq, void *dev)
{
	struct geni_i3c_dev *gi3c = dev;
	unsigned long flags;
	u32 m_stat = 0, m_stat_mask = 0;
	bool cmd_done = false;

	spin_lock_irqsave(&gi3c->ibi.lock, flags);

	if (irq == gi3c->ibi.mngr_irq) {
		m_stat_mask = readl_relaxed(gi3c->se.ibi_base + IBI_GEN_IRQ_EN);
		m_stat = readl_relaxed(gi3c->se.ibi_base
				+ IBI_GEN_IRQ_STATUS) & m_stat_mask;

		if ((m_stat & UNEXPECT_IBI_ADDR_IRQ_EN) ||
			(m_stat & BUS_ERROR_EN))
			gi3c->ibi.err = m_stat;

		if ((m_stat & ENABLE_CHANGE_IRQ_EN) ||
			(m_stat & SW_RESET_DONE_EN))
			cmd_done = true;

		if (m_stat & HOT_JOIN_IRQ_EN) {
			/* Queue worker to service hot-join request*/
			queue_work(gi3c->hj_wq, &gi3c->hj_wd);
		}
		/* clear interrupts */
		if (m_stat)
			writel_relaxed(m_stat, gi3c->se.ibi_base
				+ IBI_GEN_IRQ_CLR);
	} else if (irq == gi3c->ibi.gpii_irq[0]) {
		m_stat = readl_relaxed(gi3c->se.ibi_base + IBI_IRQ_STATUS(0));

		if (m_stat & SE_I3C_IBI_ERR)
			gi3c->ibi.err = m_stat;

		if (m_stat & IBI_RECEIVED)
			geni_i3c_handle_received_ibi(gi3c);

		if (m_stat & COMMAND_DONE)
			cmd_done = true;

		/* clear interrupts */
		if (m_stat)
			writel_relaxed(m_stat, gi3c->se.ibi_base
				+ IBI_IRQ_CLR(0));
	}

	if (cmd_done)
		complete(&gi3c->ibi.done);
	spin_unlock_irqrestore(&gi3c->ibi.lock, flags);
	return IRQ_HANDLED;
}

static void geni_i3c_handle_err(struct geni_i3c_dev *gi3c, u32 status)
{
	if (status & M_GP_IRQ_0_EN)
		geni_i3c_err(gi3c, RD_TERM);
	if (status & M_GP_IRQ_1_EN)
		geni_i3c_err(gi3c, NACK);
	if (status & M_GP_IRQ_2_EN)
		geni_i3c_err(gi3c, CRC_ERR);
	if (status & M_GP_IRQ_3_EN)
		geni_i3c_err(gi3c, BUS_PROTO);
	if (status & M_GP_IRQ_4_EN)
		geni_i3c_err(gi3c, NACK_7E);
	if (status & M_CMD_OVERRUN_EN)
		geni_i3c_err(gi3c, GENI_OVERRUN);
	if (status & M_ILLEGAL_CMD_EN)
		geni_i3c_err(gi3c, GENI_ILLEGAL_CMD);
	if (status & M_CMD_ABORT_EN)
		geni_i3c_err(gi3c, GENI_ABORT_DONE);
}

static irqreturn_t geni_i3c_irq(int irq, void *dev)
{
	struct geni_i3c_dev *gi3c = dev;
	int j;
	u32 m_stat, m_stat_mask, rx_st;
	u32 dm_tx_st, dm_rx_st, dma;
	unsigned long flags;

	spin_lock_irqsave(&gi3c->spinlock, flags);

	m_stat = readl_relaxed(gi3c->se.base + SE_GENI_M_IRQ_STATUS);
	m_stat_mask = readl_relaxed(gi3c->se.base + SE_GENI_M_IRQ_EN);
	rx_st = readl_relaxed(gi3c->se.base + SE_GENI_RX_FIFO_STATUS);
	dm_tx_st = readl_relaxed(gi3c->se.base + SE_DMA_TX_IRQ_STAT);
	dm_rx_st = readl_relaxed(gi3c->se.base + SE_DMA_RX_IRQ_STAT);
	dma = readl_relaxed(gi3c->se.base + SE_GENI_DMA_MODE_EN);

	if ((m_stat & SE_I3C_ERR) || (dm_rx_st & DM_I3C_CB_ERR)) {
		geni_i3c_handle_err(gi3c, m_stat);

		/* Disable the TX Watermark interrupt to stop TX */
		if (!dma)
			writel_relaxed(0, gi3c->se.base +
				SE_GENI_TX_WATERMARK_REG);
		goto irqret;
	}

	if (dma) {
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"i3c dma tx:0x%x, dma rx:0x%x\n", dm_tx_st, dm_rx_st);
		goto irqret;
	}

	if ((m_stat & (M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN)) &&
		(gi3c->cur_rnw == READ_TRANSACTION) &&
		gi3c->cur_buf) {
		u32 rxcnt = rx_st & RX_FIFO_WC_MSK;

		for (j = 0; j < rxcnt; j++) {
			u32 val;
			int p = 0;

			val = readl_relaxed(gi3c->se.base + SE_GENI_RX_FIFOn);
			while (gi3c->cur_idx < gi3c->cur_len &&
				 p < sizeof(val)) {
				gi3c->cur_buf[gi3c->cur_idx++] = val & 0xff;
				val >>= 8;
				p++;
			}
			if (gi3c->cur_idx == gi3c->cur_len)
				break;
		}
	} else if ((m_stat & M_TX_FIFO_WATERMARK_EN) &&
		(gi3c->cur_rnw == WRITE_TRANSACTION) &&
		(gi3c->cur_buf)) {
		for (j = 0; j < gi3c->tx_wm; j++) {
			u32 temp;
			u32 val = 0;
			int p = 0;

			while (gi3c->cur_idx < gi3c->cur_len &&
					p < sizeof(val)) {
				temp = gi3c->cur_buf[gi3c->cur_idx++];
				val |= temp << (p * 8);
				p++;
			}
			writel_relaxed(val, gi3c->se.base + SE_GENI_TX_FIFOn);
			if (gi3c->cur_idx == gi3c->cur_len) {
				writel_relaxed(0, gi3c->se.base +
					SE_GENI_TX_WATERMARK_REG);
				break;
			}
		}
	}
irqret:
	if (m_stat)
		writel_relaxed(m_stat, gi3c->se.base + SE_GENI_M_IRQ_CLEAR);

	if (dma) {
		if (dm_tx_st)
			writel_relaxed(dm_tx_st,
				gi3c->se.base + SE_DMA_TX_IRQ_CLR);
		if (dm_rx_st)
			writel_relaxed(dm_rx_st,
				gi3c->se.base + SE_DMA_RX_IRQ_CLR);
	}
	/* if this is err with done-bit not set, handle that through timeout. */
	if (m_stat & M_CMD_DONE_EN || m_stat & M_CMD_ABORT_EN) {
		writel_relaxed(0, gi3c->se.base + SE_GENI_TX_WATERMARK_REG);
		complete(&gi3c->done);
	} else if ((dm_tx_st & TX_DMA_DONE) ||
		(dm_rx_st & RX_DMA_DONE) ||
		(dm_rx_st & RX_RESET_DONE) ||
		(dm_tx_st & TX_RESET_DONE)) {

		complete(&gi3c->done);
	}

	spin_unlock_irqrestore(&gi3c->spinlock, flags);
	return IRQ_HANDLED;
}

static int i3c_geni_runtime_get_mutex_lock(struct geni_i3c_dev *gi3c)
{
	int ret;

	mutex_lock(&gi3c->lock);

	reinit_completion(&gi3c->done);
	if (!pm_runtime_enabled(gi3c->se.dev))
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
				"PM runtime disabled\n");

	ret = pm_runtime_get_sync(gi3c->se.dev);
	if (ret < 0) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"error turning on SE resources:%d\n", ret);
		pm_runtime_put_noidle(gi3c->se.dev);
		/* Set device in suspended since resume failed */
		pm_runtime_set_suspended(gi3c->se.dev);

		mutex_unlock(&gi3c->lock);
		return ret;
	}

	return 0; /* return 0 to indicate SUCCESS */
}

static void i3c_geni_runtime_put_mutex_unlock(struct geni_i3c_dev *gi3c)
{
	pm_runtime_mark_last_busy(gi3c->se.dev);
	pm_runtime_put_autosuspend(gi3c->se.dev);
	mutex_unlock(&gi3c->lock);
}

static int _i3c_geni_execute_command
(
	struct geni_i3c_dev *gi3c,
	struct i3c_xfer_params *xfer
)
{
	dma_addr_t tx_dma = 0;
	dma_addr_t rx_dma = 0;
	int ret = 0, time_remaining = 0;
	enum i3c_trans_dir rnw = gi3c->cur_rnw;
	u32 len = gi3c->cur_len;

	reinit_completion(&gi3c->done);
	geni_se_select_mode(gi3c->se.base, xfer->mode);

	gi3c->err = 0;
	gi3c->cur_idx = 0;

	if (rnw == READ_TRANSACTION) {
		writel_relaxed(len, gi3c->se.base + SE_I3C_RX_TRANS_LEN);
		geni_setup_m_cmd(gi3c->se.base, xfer->m_cmd, xfer->m_param);

		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"I3C cmd:0x%x param:0x%x READ len:%d, m_cmd: 0x%x\n",
			xfer->m_cmd, xfer->m_param, len,
			geni_read_reg(gi3c->se.base, SE_GENI_M_CMD0));

		if (xfer->mode == SE_DMA) {
			ret = geni_se_rx_dma_prep(gi3c->se.i3c_rsc.wrapper_dev,
					gi3c->se.base, gi3c->cur_buf,
					len, &rx_dma);
			if (ret) {
				GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
					"DMA Err:%d, FIFO mode enabled\n", ret);
				xfer->mode = FIFO_MODE;
				geni_se_select_mode(gi3c->se.base, xfer->mode);
			}
		}
	} else {
		writel_relaxed(len, gi3c->se.base + SE_I3C_TX_TRANS_LEN);
		geni_setup_m_cmd(gi3c->se.base, xfer->m_cmd, xfer->m_param);

		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"I3C cmd:0x%x param:0x%x WRITE len:%d, m_cmd: 0x%x\n",
			xfer->m_cmd, xfer->m_param, len,
			geni_read_reg(gi3c->se.base, SE_GENI_M_CMD0));

		if (xfer->mode == SE_DMA) {
			ret = geni_se_tx_dma_prep(gi3c->se.i3c_rsc.wrapper_dev,
					gi3c->se.base, gi3c->cur_buf,
					len, &tx_dma);
			if (ret) {
				GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
					"DMA Err:%d, FIFO mode enabled\n", ret);
				xfer->mode = FIFO_MODE;
				geni_se_select_mode(gi3c->se.base, xfer->mode);
			}
		}
		if (xfer->mode == FIFO_MODE && len > 0) /* Get FIFO IRQ */
			writel_relaxed(1, gi3c->se.base +
				SE_GENI_TX_WATERMARK_REG);
	}

	time_remaining = wait_for_completion_timeout(&gi3c->done, XFER_TIMEOUT);
	if (!time_remaining) {
		unsigned long flags;

		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"got wait_for_completion timeout\n");
		geni_i3c_err(gi3c, GENI_TIMEOUT);
		gi3c->cur_buf = NULL;
		gi3c->cur_len = gi3c->cur_idx = 0;
		gi3c->cur_rnw = 0;

		reinit_completion(&gi3c->done);

		spin_lock_irqsave(&gi3c->spinlock, flags);
		geni_cancel_m_cmd(gi3c->se.base);
		spin_unlock_irqrestore(&gi3c->spinlock, flags);

		time_remaining = wait_for_completion_timeout(&gi3c->done, HZ);
		if (!time_remaining) {
			GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
				"%s:Cancel cmd failed : Aborting\n", __func__);

			reinit_completion(&gi3c->done);
			spin_lock_irqsave(&gi3c->spinlock, flags);
			geni_abort_m_cmd(gi3c->se.base);
			spin_unlock_irqrestore(&gi3c->spinlock, flags);
			time_remaining =
			wait_for_completion_timeout(&gi3c->done, XFER_TIMEOUT);
			if (!time_remaining)
				GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
				"%s:Abort Failed\n", __func__);
		}
	}

	if (xfer->mode == SE_DMA) {
		if (gi3c->err) {
			reinit_completion(&gi3c->done);
			if (rnw == READ_TRANSACTION)
				writel_relaxed(1, gi3c->se.base +
					SE_DMA_RX_FSM_RST);
			else
				writel_relaxed(1, gi3c->se.base +
					SE_DMA_TX_FSM_RST);
			time_remaining =
			wait_for_completion_timeout(&gi3c->done, XFER_TIMEOUT);
			if (!time_remaining)
				GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
					"Timeout:FSM Reset, rnw:%d\n", rnw);
		}
		geni_se_rx_dma_unprep(gi3c->se.i3c_rsc.wrapper_dev,
				rx_dma, len);
		geni_se_tx_dma_unprep(gi3c->se.i3c_rsc.wrapper_dev,
				tx_dma, len);
	}

	if (gi3c->err) {
		ret = (gi3c->err == -EBUSY) ? I3C_ERROR_M2 : gi3c->err;
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"I3C transaction error :%d\n", gi3c->err);
	}

	gi3c->cur_buf = NULL;
	gi3c->cur_len = gi3c->cur_idx = 0;
	gi3c->cur_rnw = 0;
	gi3c->err = 0;

	return ret;
}

static int i3c_geni_execute_read_command
(
	struct geni_i3c_dev *gi3c,
	struct i3c_xfer_params *xfer,
	u8 *buf,
	u32 len
)
{
	gi3c->cur_rnw = READ_TRANSACTION;
	gi3c->cur_buf = buf;
	gi3c->cur_len = len;
	return _i3c_geni_execute_command(gi3c, xfer);
}

static int i3c_geni_execute_write_command
(
	struct geni_i3c_dev *gi3c,
	struct i3c_xfer_params *xfer,
	u8 *buf,
	u32 len
)
{
	gi3c->cur_rnw = WRITE_TRANSACTION;
	gi3c->cur_buf = buf;
	gi3c->cur_len = len;
	return _i3c_geni_execute_command(gi3c, xfer);
}

static void geni_i3c_perform_daa(struct geni_i3c_dev *gi3c)
{
	struct i3c_master_controller *m = &gi3c->ctrlr;
	int ret;

	while (1) {
		u8 rx_buf[8], tx_buf[8];
		struct i3c_xfer_params xfer = { FIFO_MODE };
		struct i3c_dev_boardinfo *i3cboardinfo = NULL;
		struct i3c_dev_desc *i3cdev = NULL;
		u64 pid;
		u8 bcr, dcr, init_dyn_addr = 0, addr = 0;
		bool enum_slv = false;

		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"i3c entdaa read\n");

		xfer.m_cmd = I2C_READ;
		xfer.m_param = STOP_STRETCH | CONTINUOUS_MODE_DAA | USE_7E |
				IBI_NACK_TBL_CTRL;

		ret = i3c_geni_execute_read_command(gi3c, &xfer, rx_buf, 8);
		if (ret)
			break;

		dcr = rx_buf[7];
		bcr = rx_buf[6];
		pid = ((u64)rx_buf[0] << 40) |
			((u64)rx_buf[1] << 32) |
			((u64)rx_buf[2] << 24) |
			((u64)rx_buf[3] << 16) |
			((u64)rx_buf[4] <<  8) |
			((u64)rx_buf[5]);

		list_for_each_entry(i3cboardinfo, &m->boardinfo.i3c, node) {
			if (pid == i3cboardinfo->pid) {
				GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
				"PID 0x:%x matched with boardinfo\n", pid);
				break;
			}
		}

		if (i3cboardinfo == NULL) {
			GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
					"Invalid i3cboardinfo\n");
			goto daa_err;
		}

		addr = init_dyn_addr = i3cboardinfo->init_dyn_addr;
		addr = ret = i3c_master_get_free_addr(m, addr);

		if (ret < 0) {
			GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"error during get_free_addr ret:%d for pid:0x:%x\n"
				, ret, pid);
			goto daa_err;
		} else if (ret == init_dyn_addr) {
			GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
				"assigning requested addr:0x%x for pid:0x:%x\n"
				, ret, pid);
		} else if (init_dyn_addr) {
			i3c_bus_for_each_i3cdev(&m->bus, i3cdev) {
				if (i3cdev->info.pid == pid) {
					enum_slv = true;
					break;
				}
			}
			if (enum_slv) {
				addr = i3cdev->info.dyn_addr;
				GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
					"assigning requested addr:0x%x for pid:0x:%x\n"
					, addr, pid);
			} else {
				GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
				"new dev: assigning addr:0x%x for pid:x:%x\n"
				, ret, pid);
			}
		} else {
			GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
				"assigning addr:0x%x for pid:x:%x\n", ret, pid);
		}

		if (!i3cboardinfo->init_dyn_addr)
			i3cboardinfo->init_dyn_addr = addr;

		if (!enum_slv)
			set_new_addr_slot(gi3c->newaddrslots, addr);

		tx_buf[0] = (addr & I3C_ADDR_MASK) << 1;
		tx_buf[0] |= ~(hweight8(addr & I3C_ADDR_MASK) & 1);

		/* calculate crc */
		if (tx_buf[0]) {
			u32 slaveid = addr;
			u32 ret = slaveid & 1u;
			u32 final = 0;

			while (slaveid) {
				slaveid >>= 1;
				ret = ret ^ (slaveid & 1u);
			}

			ret = ret ^ 1u;
			final = (addr << 1) | ret;
			tx_buf[0] = final;
		}

		xfer.m_cmd = I2C_WRITE;
		xfer.m_param = STOP_STRETCH | BYPASS_ADDR_PHASE |
				IBI_NACK_TBL_CTRL;

		ret = i3c_geni_execute_write_command(gi3c, &xfer, tx_buf, 1);
		if (ret)
			break;
	}
daa_err:
	return;
}

static int geni_i3c_master_send_ccc_cmd
(
	struct i3c_master_controller *m,
	struct i3c_ccc_cmd *cmd
)
{
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int i, ret;

	if (!(cmd->id & I3C_CCC_DIRECT) && (cmd->ndests != 1))
		return -EINVAL;

	ret = i3c_geni_runtime_get_mutex_lock(gi3c);
	if (ret)
		return ret;

	qcom_geni_i3c_conf(gi3c, OPEN_DRAIN_MODE);

	for (i = 0; i < cmd->ndests; i++) {
		int stall = (i < (cmd->ndests - 1)) ||
			(cmd->id == I3C_CCC_ENTDAA);
		struct i3c_xfer_params xfer = { FIFO_MODE };

		xfer.m_param  = (stall ? STOP_STRETCH : 0);
		xfer.m_param |= (cmd->id << CCC_HDR_CMD_SHFT);
		xfer.m_param |= IBI_NACK_TBL_CTRL;
		if (cmd->id & I3C_CCC_DIRECT) {
			xfer.m_param |= ((cmd->dests[i].addr & I3C_ADDR_MASK)
					<< SLV_ADDR_SHFT);
			if (cmd->rnw) {
				if (i == 0)
					xfer.m_cmd = I3C_DIRECT_CCC_READ;
				else
					xfer.m_cmd = I3C_PRIVATE_READ;
			} else {
				if (i == 0)
					xfer.m_cmd =
					   (cmd->dests[i].payload.len > 0) ?
						I3C_DIRECT_CCC_WRITE :
						I3C_DIRECT_CCC_ADDR_ONLY;
				else
					xfer.m_cmd = I3C_PRIVATE_WRITE;
			}
		} else {
			if (cmd->dests[i].payload.len > 0)
				xfer.m_cmd = I3C_BCAST_CCC_WRITE;
			else
				xfer.m_cmd = I3C_BCAST_CCC_ADDR_ONLY;
		}

		if (i == 0)
			xfer.m_param |= USE_7E;

		if (cmd->rnw)
			ret = i3c_geni_execute_read_command(gi3c, &xfer,
				cmd->dests[i].payload.data,
				cmd->dests[i].payload.len);
		else
			ret = i3c_geni_execute_write_command(gi3c, &xfer,
				cmd->dests[i].payload.data,
				cmd->dests[i].payload.len);
		if (ret)
			break;

		if (cmd->id == I3C_CCC_ENTDAA)
			geni_i3c_perform_daa(gi3c);
	}

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
		"i3c ccc: txn ret:%d\n", ret);

	i3c_geni_runtime_put_mutex_unlock(gi3c);

	return ret;
}

static int geni_i3c_master_priv_xfers
(
	struct i3c_dev_desc *dev,
	struct i3c_priv_xfer *xfers,
	int nxfers
)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int i, ret;
	bool use_7e = true;

	if (nxfers <= 0)
		return 0;

	ret = i3c_geni_runtime_get_mutex_lock(gi3c);
	if (ret)
		return ret;

	qcom_geni_i3c_conf(gi3c, PUSH_PULL_MODE);

	for (i = 0; i < nxfers; i++) {
		bool stall = (i < (nxfers - 1));
		struct i3c_xfer_params xfer = { FIFO_MODE };

		xfer.m_param  = (stall ? STOP_STRETCH : 0);
		xfer.m_param |= ((dev->info.dyn_addr & I3C_ADDR_MASK)
				<< SLV_ADDR_SHFT);
		xfer.m_param |= (use_7e) ? USE_7E : 0;

		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
		"%s: stall:%d,use_7e:%d, nxfers:%d,i:%d,m_param:0x%x,rnw:%d\n",
		__func__, stall, use_7e, nxfers, i, xfer.m_param, xfers[i].rnw);

		/* Update use_7e status for next loop iteration */
		use_7e = !stall;

		if (xfers[i].rnw) {
			xfer.m_cmd = I3C_PRIVATE_READ;
			ret = i3c_geni_execute_read_command(gi3c, &xfer,
				(u8 *)xfers[i].data.in,
				xfers[i].len);
		} else {
			xfer.m_cmd = I3C_PRIVATE_WRITE;
			ret = i3c_geni_execute_write_command(gi3c, &xfer,
				(u8 *)xfers[i].data.out,
				xfers[i].len);
		}

		if (ret)
			break;
	}

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
		"i3c priv: txn ret:%d\n", ret);

	i3c_geni_runtime_put_mutex_unlock(gi3c);

	return ret;
}

static int geni_i3c_master_i2c_xfers
(
	struct i2c_dev_desc *dev,
	const struct i2c_msg *msgs,
	int num
)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int i, ret;

	ret = i3c_geni_runtime_get_mutex_lock(gi3c);
	if (ret)
		return ret;

	qcom_geni_i3c_conf(gi3c, PUSH_PULL_MODE);

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
		"i2c xfer:num:%d, msgs:len:%d,flg:%d\n",
		num, msgs[0].len, msgs[0].flags);

	for (i = 0; i < num; i++) {
		struct i3c_xfer_params xfer;

		xfer.m_cmd    = (msgs[i].flags & I2C_M_RD) ? I2C_READ :
							I2C_WRITE;
		xfer.m_param  = (i < (num - 1)) ? STOP_STRETCH : 0;
		xfer.m_param |= ((msgs[i].addr & I3C_ADDR_MASK)
				<< SLV_ADDR_SHFT);
		xfer.mode     = msgs[i].len > 32 ? SE_DMA : FIFO_MODE;
		if (msgs[i].flags & I2C_M_RD)
			ret = i3c_geni_execute_read_command(gi3c, &xfer,
						msgs[i].buf, msgs[i].len);
		else
			ret = i3c_geni_execute_write_command(gi3c, &xfer,
						msgs[i].buf, msgs[i].len);
		if (ret)
			break;
	}

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev, "i2c: txn ret:%d\n", ret);

	i3c_geni_runtime_put_mutex_unlock(gi3c);

	return ret;
}

static int geni_i3c_master_attach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct geni_i3c_i2c_dev_data *data;

	data = devm_kzalloc(gi3c->se.dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev, "%s\n", __func__);
	i2c_dev_set_master_data(dev, data);

	return 0;
}

static void geni_i3c_master_detach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct geni_i3c_i2c_dev_data *data = i2c_dev_get_master_data(dev);

	i2c_dev_set_master_data(dev, NULL);
	devm_kfree(gi3c->se.dev, data);
}

static int geni_i3c_master_attach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct geni_i3c_i2c_dev_data *data;
	struct i3c_dev_boardinfo *i3cboardinfo;

	data = devm_kzalloc(gi3c->se.dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->ibi = -1;
	i3c_dev_set_master_data(dev, data);
	if (!dev->boardinfo) {
		list_for_each_entry(i3cboardinfo, &m->boardinfo.i3c, node) {
			if (dev->info.pid == i3cboardinfo->pid)
				dev->boardinfo = i3cboardinfo;
		}
	}

	return 0;
}

static int geni_i3c_master_reattach_i3c_dev
(
	struct i3c_dev_desc *dev,
	u8 old_dyn_addr
)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct i3c_dev_boardinfo *i3cboardinfo;

	if (!dev->boardinfo) {
		list_for_each_entry(i3cboardinfo, &m->boardinfo.i3c, node) {
			if (dev->info.pid == i3cboardinfo->pid)
				dev->boardinfo = i3cboardinfo;
		}
	}

	return 0;
}

static void geni_i3c_master_detach_i3c_dev(struct i3c_dev_desc *dev)
{

	i3c_dev_set_master_data(dev, NULL);
}

static int geni_i3c_master_entdaa_locked(struct geni_i3c_dev *gi3c)
{
	struct i3c_master_controller *m = &gi3c->ctrlr;
	u8 addr;
	int ret;

	ret = i3c_master_entdaa_locked(m);
	if (ret && ret != I3C_ERROR_M2)
		return ret;

	for (addr = 0; addr <= I3C_ADDR_MASK; addr++) {
		if (is_new_addr_slot_set(gi3c->newaddrslots, addr)) {
			clear_new_addr_slot(gi3c->newaddrslots, addr);
			i3c_master_add_i3c_dev_locked(m, addr);
		}
	}

	i3c_master_enec_locked(m, I3C_BROADCAST_ADDR,
				      I3C_CCC_EVENT_MR |
				      I3C_CCC_EVENT_HJ);

	return 0;
}

static int geni_i3c_master_do_daa(struct i3c_master_controller *m)
{
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);

	return geni_i3c_master_entdaa_locked(gi3c);
}

static int geni_i3c_master_bus_init(struct i3c_master_controller *m)
{
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct i3c_bus *bus = i3c_master_get_bus(m);
	struct i3c_device_info info = { };
	int ret;

	ret = pm_runtime_get_sync(gi3c->se.dev);
	if (ret < 0) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"%s: error turning SE resources:%d\n", __func__, ret);
		pm_runtime_put_noidle(gi3c->se.dev);
		/* Set device in suspended since resume failed */
		pm_runtime_set_suspended(gi3c->se.dev);
		return ret;
	}

	ret = geni_i3c_clk_map_idx(gi3c);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"Invalid clk frequency %d Hz src or %ld Hz bus: %d\n",
			gi3c->clk_src_freq, bus->scl_rate.i3c,
			ret);
		goto err_cleanup;
	}

	qcom_geni_i3c_conf(gi3c, OPEN_DRAIN_MODE);

	/* Get an address for the master. */
	ret = i3c_master_get_free_addr(m, 0);
	if (ret < 0) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
				"%s: error No free addr:%d\n", __func__, ret);
		goto err_cleanup;
	}

	info.dyn_addr = ret;
	info.dcr = I3C_DCR_GENERIC_DEVICE;
	info.bcr = I3C_BCR_I3C_MASTER | I3C_BCR_HDR_CAP;
	info.pid = 0;

	ret = i3c_master_set_info(&gi3c->ctrlr, &info);

err_cleanup:
	/*As framework calls multiple exposed API's after this API, we cannot
	 *use mutex protected internal put/get sync API. Hence forcefully
	 *disabling clocks and decrementing usage count.
	 */
	disable_irq(gi3c->irq);
	se_geni_resources_off(&gi3c->se.i3c_rsc);
	pm_runtime_disable(gi3c->se.dev);
	pm_runtime_put_noidle(gi3c->se.dev);
	pm_runtime_set_suspended(gi3c->se.dev);
	pm_runtime_enable(gi3c->se.dev);

	return ret;
}

static void geni_i3c_master_bus_cleanup(struct i3c_master_controller *m)
{
}

static bool geni_i3c_master_supports_ccc_cmd
(
	struct i3c_master_controller *m,
	const struct i3c_ccc_cmd *cmd
)
{
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);

	switch (cmd->id) {
	case I3C_CCC_ENEC(true):
	/* fallthrough */
	case I3C_CCC_ENEC(false):
	/* fallthrough */
	case I3C_CCC_DISEC(true):
	/* fallthrough */
	case I3C_CCC_DISEC(false):
	/* fallthrough */
	case I3C_CCC_ENTAS(0, true):
	/* fallthrough */
	case I3C_CCC_ENTAS(0, false):
	/* fallthrough */
	case I3C_CCC_RSTDAA(true):
	/* fallthrough */
	case I3C_CCC_RSTDAA(false):
	/* fallthrough */
	case I3C_CCC_ENTDAA:
	/* fallthrough */
	case I3C_CCC_SETMWL(true):
	/* fallthrough */
	case I3C_CCC_SETMWL(false):
	/* fallthrough */
	case I3C_CCC_SETMRL(true):
	/* fallthrough */
	case I3C_CCC_SETMRL(false):
	/* fallthrough */
	case I3C_CCC_DEFSLVS:
	/* fallthrough */
	case I3C_CCC_ENTHDR(0):
	/* fallthrough */
	case I3C_CCC_SETDASA:
	/* fallthrough */
	case I3C_CCC_SETNEWDA:
	/* fallthrough */
	case I3C_CCC_GETMWL:
	/* fallthrough */
	case I3C_CCC_GETMRL:
	/* fallthrough */
	case I3C_CCC_GETPID:
	/* fallthrough */
	case I3C_CCC_GETBCR:
	/* fallthrough */
	case I3C_CCC_GETDCR:
	/* fallthrough */
	case I3C_CCC_GETSTATUS:
	/* fallthrough */
	case I3C_CCC_GETACCMST:
	/* fallthrough */
	case I3C_CCC_GETMXDS:
	/* fallthrough */
	case I3C_CCC_GETHDRCAP:
		return true;
	default:
		break;
	}

	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"%s: Unsupported cmnd\n", __func__);
	return false;
}

static int geni_i3c_master_enable_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int ret = 0;

	if (!gi3c->ibi.hw_support && !gi3c->ibi.is_init)
		return -EPERM;

	ret = i3c_master_enec_locked(m, dev->info.dyn_addr,
				      I3C_CCC_EVENT_SIR);
	if (ret)
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"%s: error while i3c_master_enec_locked\n", __func__);

	return ret;
}

static int geni_i3c_master_disable_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	int ret = 0;

	if (!gi3c->ibi.hw_support && !gi3c->ibi.is_init)
		return -EPERM;

	ret = i3c_master_disec_locked(m, dev->info.dyn_addr, I3C_CCC_EVENT_SIR);
	if (ret)
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"%s: error while i3c_master_disec_locked\n", __func__);

	return ret;
}

static void qcom_geni_i3c_ibi_conf(struct geni_i3c_dev *gi3c)
{
	gi3c->ibi.err = 0;
	reinit_completion(&gi3c->ibi.done);

	/* set the configuration for 100Khz OD speed */
	geni_write_reg(0x5FD74322, gi3c->se.ibi_base, IBI_SCL_PP_TIMING_CONFIG);

	geni_i3c_enable_ibi_ctrl(gi3c, true);
	geni_i3c_enable_ibi_irq(gi3c, true);
	gi3c->ibi.is_init = true;
}

static int geni_i3c_master_request_ibi(struct i3c_dev_desc *dev,
	const struct i3c_ibi_setup *req)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct geni_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long i, flags;
	unsigned int payload_len = req->max_payload_len;

	if (!gi3c->ibi.hw_support)
		return -EPERM;

	if (!gi3c->ibi.is_init)
		qcom_geni_i3c_ibi_conf(gi3c);

	data->ibi_pool = i3c_generic_ibi_alloc_pool(dev, req);
	if (IS_ERR(data->ibi_pool)) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"Error creating a generic IBI pool %d\n",
			PTR_ERR(data->ibi_pool));
		return PTR_ERR(data->ibi_pool);
	}

	spin_lock_irqsave(&gi3c->ibi.lock, flags);
	for (i = 0; i < gi3c->ibi.num_slots; i++) {
		if (!gi3c->ibi.slots[i]) {
			data->ibi = i;
			gi3c->ibi.slots[i] = dev;
			break;
		}
	}
	spin_unlock_irqrestore(&gi3c->ibi.lock, flags);

	if (i < gi3c->ibi.num_slots) {
		u32 cmd, timeout;

		gi3c->ibi.err = 0;
		reinit_completion(&gi3c->ibi.done);

		cmd = ((dev->info.dyn_addr & I3C_SLAVE_MASK)
			<< I3C_SLAVE_ADDR_SHIFT) | I3C_SLAVE_RW | STALL;
		cmd |= ((payload_len << NUM_OF_MDB_SHIFT) & IBI_NUM_OF_MDB_MSK);
		geni_write_reg(cmd, gi3c->se.ibi_base, IBI_CMD(0));

		/* wait for adding slave IBI */
		timeout = wait_for_completion_timeout(&gi3c->ibi.done,
				XFER_TIMEOUT);
		if (!timeout) {
			gi3c->ibi.err = -ETIMEDOUT;
			GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
				"timeout while adding slave IBI\n");
		}

		if (!gi3c->ibi.err)
			return 0;

		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"error while adding slave IBI 0x%x\n", gi3c->ibi.err);
	}

	GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
		"ibi.num_slots ran out %d: %d\n", i, gi3c->ibi.num_slots);

	i3c_generic_ibi_free_pool(data->ibi_pool);
	data->ibi_pool = NULL;

	return -ENOSPC;
}

static int qcom_deallocate_ibi_table_entry(struct geni_i3c_dev *gi3c)
{
	u32 i, timeout;

	for (i = 0; i < gi3c->ibi.num_slots; i++) {
		u32 entry;

		gi3c->ibi.err = 0;
		reinit_completion(&gi3c->ibi.done);

		entry = geni_read_reg(gi3c->se.ibi_base,
				IBI_CONFIG_ENTRY(0, i));

		/* if valid entry */
		if (entry & IBI_VALID) {
			/* send remove command */
			entry &= ~IBI_CMD_OPCODE;
			geni_write_reg(entry, gi3c->se.ibi_base, IBI_CMD(0));

			/* wait for removing slave IBI */
			timeout = wait_for_completion_timeout(&gi3c->ibi.done,
					XFER_TIMEOUT);
			if (!timeout) {
				GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
					"timeout while adding slave IBI\n");
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

static void geni_i3c_enable_hotjoin_irq(struct geni_i3c_dev *gi3c, bool enable)
{
	u32 val;

	//Disable hot-join, until next probe happens
	val = geni_read_reg(gi3c->se.ibi_base, IBI_GEN_IRQ_EN);
	if (enable)
		val |= HOT_JOIN_IRQ_EN;
	else
		val &= ~HOT_JOIN_IRQ_EN;
	geni_write_reg(val, gi3c->se.ibi_base, IBI_GEN_IRQ_EN);

	GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
		"%s:%s\n", __func__, (enable) ? "Enabled" : "Disabled");
}

static void geni_i3c_enable_ibi_irq(struct geni_i3c_dev *gi3c, bool enable)
{
	u32 val;

	if (enable) {
		/* enable manager interrupts : HPG sec 4.1 */
		val = geni_read_reg(gi3c->se.ibi_base, IBI_GEN_IRQ_EN);
		val |= (val & 0x1B);
		geni_write_reg(val, gi3c->se.ibi_base, IBI_GEN_IRQ_EN);

		/* Enable GPII0 interrupts */
		geni_write_reg(GPIIn_IBI_EN(0), gi3c->se.ibi_base,
							IBI_GPII_IBI_EN);
		geni_write_reg(~0u, gi3c->se.ibi_base, IBI_IRQ_EN(0));
	} else {
		geni_write_reg(0, gi3c->se.ibi_base, IBI_GPII_IBI_EN);
		geni_write_reg(0, gi3c->se.ibi_base, IBI_IRQ_EN(0));
		geni_write_reg(0, gi3c->se.ibi_base, IBI_GEN_IRQ_EN);
	}
}

static void geni_i3c_enable_ibi_ctrl(struct geni_i3c_dev *gi3c, bool enable)
{
	u32 val, timeout;

	if (enable) {
		reinit_completion(&gi3c->ibi.done);

		/* enable ENABLE_CHANGE */
		val = geni_read_reg(gi3c->se.ibi_base, IBI_GEN_IRQ_EN);
		val |= IBI_C_ENABLE;
		geni_write_reg(val, gi3c->se.ibi_base, IBI_GEN_IRQ_EN);

		/* Enable I3C IBI controller, if not in enabled state */
		val = geni_read_reg(gi3c->se.ibi_base, IBI_GEN_CONFIG);
		if (!(val & IBI_C_ENABLE)) {
			val |= IBI_C_ENABLE;
			geni_write_reg(val, gi3c->se.ibi_base, IBI_GEN_CONFIG);

			/* wait for ENABLE_CHANGE */
			timeout = wait_for_completion_timeout(&gi3c->ibi.done,
								XFER_TIMEOUT);
			if (!timeout) {
				GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
					"timeout while ENABLE_CHANGE bit\n");
				return;
			}
			GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
					"%s: IBI ctrl enabled\n", __func__);
		}
	} else {
		 /* Disable IBI controller */

		/* check if any IBI is enabled, if not then disable IBI ctrl */
		val = geni_read_reg(gi3c->se.ibi_base, IBI_GPII_IBI_EN);
		if (!val) {
			gi3c->ibi.err = 0;
			reinit_completion(&gi3c->ibi.done);

			val = geni_read_reg(gi3c->se.ibi_base, IBI_GEN_CONFIG);
			val &= ~IBI_C_ENABLE;
			geni_write_reg(val, gi3c->se.ibi_base, IBI_GEN_CONFIG);

			/* wait for ENABLE change */
			timeout = wait_for_completion_timeout(&gi3c->ibi.done,
					XFER_TIMEOUT);
			if (!timeout) {
				GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
				"timeout disabling IBI: 0x%x\n", gi3c->ibi.err);
				return;
			}
			GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
					"%s: IBI ctrl disabled\n",  __func__);
		}
	}
}


static void qcom_geni_i3c_ibi_unconf(struct geni_i3c_dev *gi3c)
{
	u32 val;
	int ret = 0;

	val = geni_read_reg(gi3c->se.ibi_base, IBI_ALLOCATED_ENTRIES_GPII(0));
	if (val) {
		ret = qcom_deallocate_ibi_table_entry(gi3c);
		if (ret)
			return;
	}

	gi3c->ibi.is_init = false;
}

static void geni_i3c_master_free_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct geni_i3c_dev *gi3c = to_geni_i3c_master(m);
	struct geni_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;

	if (!gi3c->ibi.hw_support && !gi3c->ibi.is_init)
		return;

	qcom_geni_i3c_ibi_unconf(gi3c);

	spin_lock_irqsave(&gi3c->ibi.lock, flags);
	gi3c->ibi.slots[data->ibi] = NULL;
	data->ibi = -1;
	spin_unlock_irqrestore(&gi3c->ibi.lock, flags);

	i3c_generic_ibi_free_pool(data->ibi_pool);
}

static void geni_i3c_master_recycle_ibi_slot
(
	struct i3c_dev_desc *dev,
	struct i3c_ibi_slot *slot
)
{
	struct geni_i3c_i2c_dev_data *data = i3c_dev_get_master_data(dev);

	i3c_generic_ibi_recycle_slot(data->ibi_pool, slot);
}

static const struct i3c_master_controller_ops geni_i3c_master_ops = {
	.bus_init = geni_i3c_master_bus_init,
	.bus_cleanup = geni_i3c_master_bus_cleanup,
	.do_daa = geni_i3c_master_do_daa,
	.attach_i3c_dev = geni_i3c_master_attach_i3c_dev,
	.reattach_i3c_dev = geni_i3c_master_reattach_i3c_dev,
	.detach_i3c_dev = geni_i3c_master_detach_i3c_dev,
	.attach_i2c_dev = geni_i3c_master_attach_i2c_dev,
	.detach_i2c_dev = geni_i3c_master_detach_i2c_dev,
	.supports_ccc_cmd = geni_i3c_master_supports_ccc_cmd,
	.send_ccc_cmd = geni_i3c_master_send_ccc_cmd,
	.priv_xfers = geni_i3c_master_priv_xfers,
	.i2c_xfers = geni_i3c_master_i2c_xfers,
	.enable_ibi = geni_i3c_master_enable_ibi,
	.disable_ibi = geni_i3c_master_disable_ibi,
	.request_ibi = geni_i3c_master_request_ibi,
	.free_ibi = geni_i3c_master_free_ibi,
	.recycle_ibi_slot = geni_i3c_master_recycle_ibi_slot,
};

static int i3c_geni_rsrcs_clk_init(struct geni_i3c_dev *gi3c)
{
	int ret;

	gi3c->se.i3c_rsc.ctrl_dev = gi3c->se.dev;
	gi3c->se.i3c_rsc.se_clk = devm_clk_get(gi3c->se.dev, "se-clk");
	if (IS_ERR(gi3c->se.i3c_rsc.se_clk)) {
		ret = PTR_ERR(gi3c->se.i3c_rsc.se_clk);
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"Error getting SE Core clk %d\n", ret);
		return ret;
	}

	gi3c->se.i3c_rsc.m_ahb_clk = devm_clk_get(gi3c->se.dev, "m-ahb");
	if (IS_ERR(gi3c->se.i3c_rsc.m_ahb_clk)) {
		ret = PTR_ERR(gi3c->se.i3c_rsc.m_ahb_clk);
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"Error getting M AHB clk %d\n", ret);
		return ret;
	}

	gi3c->se.i3c_rsc.s_ahb_clk = devm_clk_get(gi3c->se.dev, "s-ahb");
	if (IS_ERR(gi3c->se.i3c_rsc.s_ahb_clk)) {
		ret = PTR_ERR(gi3c->se.i3c_rsc.s_ahb_clk);
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"Error getting S AHB clk %d\n", ret);
		return ret;
	}

	return 0;
}

static int i3c_geni_rsrcs_init(struct geni_i3c_dev *gi3c,
			struct platform_device *pdev)
{
	struct resource *res;
	struct platform_device *wrapper_pdev;
	struct device_node *wrapper_ph_node;
	int ret;

	/* base register address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Err getting IO region\n");
		return -EINVAL;
	}

	gi3c->se.base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gi3c->se.base))
		return PTR_ERR(gi3c->se.base);

	wrapper_ph_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,wrapper-core", 0);
	if (IS_ERR_OR_NULL(wrapper_ph_node)) {
		ret = PTR_ERR(wrapper_ph_node);
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"No wrapper core defined\n");
		return ret;
	}

	wrapper_pdev = of_find_device_by_node(wrapper_ph_node);
	of_node_put(wrapper_ph_node);
	if (IS_ERR_OR_NULL(wrapper_pdev)) {
		ret = PTR_ERR(wrapper_pdev);
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Cannot retrieve wrapper device\n");
		return ret;
	}

	gi3c->se.i3c_rsc.wrapper_dev = &wrapper_pdev->dev;

	ret = geni_se_resources_init(&gi3c->se.i3c_rsc, I3C_CORE2X_VOTE,
				     (DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));
	if (ret) {
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
				"geni_se_resources_init Failed:%d\n", ret);
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev, "se-clock-frequency",
		&gi3c->clk_src_freq);
	if (ret) {
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			"SE clk freq not specified, default to 100 MHz.\n");
		gi3c->clk_src_freq = 100000000;
	}

	ret = device_property_read_u32(&pdev->dev, "dfs-index", &gi3c->dfs_idx);
	if (ret)
		gi3c->dfs_idx = 0xf;

	gi3c->se.i3c_rsc.geni_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(gi3c->se.i3c_rsc.geni_pinctrl)) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error no pinctrl config specified\n");
		ret = PTR_ERR(gi3c->se.i3c_rsc.geni_pinctrl);
		return ret;
	}
	gi3c->se.i3c_rsc.geni_gpio_active =
		pinctrl_lookup_state(gi3c->se.i3c_rsc.geni_pinctrl, "default");
	if (IS_ERR(gi3c->se.i3c_rsc.geni_gpio_active)) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error no pinctr default config specified\n");
		ret = PTR_ERR(gi3c->se.i3c_rsc.geni_gpio_active);
		return ret;
	}
	gi3c->se.i3c_rsc.geni_gpio_sleep =
		pinctrl_lookup_state(gi3c->se.i3c_rsc.geni_pinctrl, "sleep");
	if (IS_ERR(gi3c->se.i3c_rsc.geni_gpio_sleep)) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error no pinctrl sleep config specified\n");
		ret = PTR_ERR(gi3c->se.i3c_rsc.geni_gpio_sleep);
		return ret;
	}
	gi3c->i3c_gpio_disable =
		pinctrl_lookup_state(gi3c->se.i3c_rsc.geni_pinctrl, "disable");
	if (IS_ERR(gi3c->i3c_gpio_disable)) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error no pinctrl disable config specified\n");
		ret = PTR_ERR(gi3c->i3c_gpio_disable);
		return ret;
	}

	return 0;
}

static int i3c_ibi_rsrcs_init(struct geni_i3c_dev *gi3c,
		struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	if (of_property_read_u8(pdev->dev.of_node, "qcom,ibi-ctrl-id",
		&gi3c->ibi.ctrl_id)) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"IBI controller instance id is not defined\n");
		return -ENXIO;
	}

	/* Enable TLMM I3C MODE registers */
	msm_qup_write(gi3c->ibi.ctrl_id, TLMM_I3C_MODE);

	/* IBI register address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;

	gi3c->se.ibi_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gi3c->se.ibi_base))
		return PTR_ERR(gi3c->se.ibi_base);

	gi3c->ibi.hw_support = (geni_read_reg(gi3c->se.base, SE_HW_PARAM_0)
				& GEN_I3C_IBI_CTRL);
	if (!gi3c->ibi.hw_support) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"IBI controller support not present\n");
		return -ENODEV;
	}

	init_completion(&gi3c->ibi.done);
	spin_lock_init(&gi3c->ibi.lock);
	gi3c->ibi.num_slots = ((geni_read_reg(gi3c->se.ibi_base, IBI_HW_PARAM)
				& I3C_IBI_TABLE_DEPTH_MSK));
	gi3c->ibi.slots = devm_kcalloc(&pdev->dev, gi3c->ibi.num_slots,
				sizeof(*gi3c->ibi.slots), GFP_KERNEL);
	if (!gi3c->ibi.slots)
		return -ENOMEM;

	/* Register IBI_C manager interrupt */
	gi3c->ibi.mngr_irq = platform_get_irq(pdev, 1);
	if (gi3c->ibi.mngr_irq < 0) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"IRQ error for ibi_c manager\n");
		return gi3c->ibi.mngr_irq;
	}

	ret = devm_request_irq(&pdev->dev, gi3c->ibi.mngr_irq, geni_i3c_ibi_irq,
			IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), gi3c);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Request_irq:%d: err:%d\n", gi3c->ibi.mngr_irq, ret);
		return ret;
	}

	/* set mngr irq as wake-up irq */
	ret = irq_set_irq_wake(gi3c->ibi.mngr_irq, 1);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Failed to set mngr IRQ(%d) wake: err:%d\n",
			gi3c->ibi.mngr_irq, ret);
		return ret;
	}

	/* Register GPII interrupt */
	gi3c->ibi.gpii_irq[0] = platform_get_irq(pdev, 2);
	if (gi3c->ibi.gpii_irq[0] < 0) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"IRQ error for ibi_c gpii\n");
		return gi3c->ibi.gpii_irq[0];
	}

	ret = devm_request_irq(&pdev->dev, gi3c->ibi.gpii_irq[0],
				geni_i3c_ibi_irq, IRQF_TRIGGER_HIGH,
				dev_name(&pdev->dev), gi3c);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
		"Request_irq failed:%d: err:%d\n", gi3c->ibi.gpii_irq[0], ret);
		return ret;
	}

	/* set gpii irq as wake-up irq */
	ret = irq_set_irq_wake(gi3c->ibi.gpii_irq[0], 1);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Failed to set gpii IRQ(%d) wake: err:%d\n",
			gi3c->ibi.gpii_irq[0], ret);
		return ret;
	}

	qcom_geni_i3c_ibi_conf(gi3c);

	return 0;
}

static void geni_i3c_get_ver_info(struct geni_i3c_dev *gi3c)
{
	int hw_ver;
	unsigned int major, minor, step;

	hw_ver = geni_se_qupv3_hw_version(gi3c->se.i3c_rsc.wrapper_dev,
					&major, &minor, &step);
	if (hw_ver)
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
		"%s:Error reading HW version %d\n", __func__, hw_ver);
	else
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
		"%s:Major:%d Minor:%d step:%d\n", __func__, major, minor, step);

	gi3c->ver_info.m_fw_ver = get_se_m_fw(gi3c->se.base);
	gi3c->ver_info.s_fw_ver = get_se_s_fw(gi3c->se.base);
	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev, "%s:FW Ver:0x%x%x\n",
		__func__, gi3c->ver_info.m_fw_ver, gi3c->ver_info.s_fw_ver);
}

static int geni_i3c_probe(struct platform_device *pdev)
{
	struct geni_i3c_dev *gi3c;
	u32 proto, tx_depth;
	int ret;
	u32 se_mode;

	gi3c = devm_kzalloc(&pdev->dev, sizeof(*gi3c), GFP_KERNEL);
	if (!gi3c)
		return -ENOMEM;

	gi3c->se.dev = &pdev->dev;
	gi3c->ipcl = ipc_log_context_create(4, dev_name(gi3c->se.dev), 0);
	if (!gi3c->ipcl)
		dev_info(&pdev->dev, "Error creating IPC Log\n");

	if (i3c_nos < MAX_I3C_SE)
		i3c_geni_dev[i3c_nos++] = gi3c;

	ret = i3c_geni_rsrcs_init(gi3c, pdev);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error:%d i3c_geni_rsrcs_init\n", ret);
		goto cleanup_init;
	}

	ret = i3c_geni_rsrcs_clk_init(gi3c);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error:%d i3c_geni_rsrcs_clk_init\n", ret);
		goto cleanup_init;
	}

	gi3c->irq = platform_get_irq(pdev, 0);
	if (gi3c->irq < 0) {
		ret = gi3c->irq;
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"IRQ error=%d for i3c-master-geni\n", ret);
		goto cleanup_init;
	}

	init_completion(&gi3c->done);
	mutex_init(&gi3c->lock);
	spin_lock_init(&gi3c->spinlock);
	platform_set_drvdata(pdev, gi3c);

	/* Keep interrupt disabled so the system can enter low-power mode */
	irq_set_status_flags(gi3c->irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&pdev->dev, gi3c->irq, geni_i3c_irq,
		IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), gi3c);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"i3c irq failed:%d: err:%d\n", gi3c->irq, ret);
		goto cleanup_init;
	}

	ret = se_geni_resources_on(&gi3c->se.i3c_rsc);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error turning on resources %d\n", ret);
		goto cleanup_init;
	}

	proto = get_se_proto(gi3c->se.base);
	if (proto != I3C) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Invalid proto %d\n", proto);
		ret = -ENXIO;
		goto geni_resources_off;
	} else {
		geni_i3c_get_ver_info(gi3c);
	}

	se_mode = geni_read_reg(gi3c->se.base, GENI_IF_FIFO_DISABLE_RO);
	if (se_mode) {
		/* GSI mode not supported */
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Non supported mode %d\n", se_mode);
		ret = -ENXIO;
		goto geni_resources_off;
	}

	tx_depth = get_tx_fifo_depth(gi3c->se.base);
	gi3c->tx_wm = tx_depth - 1;
	geni_se_init(gi3c->se.base, gi3c->tx_wm, tx_depth);
	se_config_packing(gi3c->se.base, BITS_PER_BYTE, PACKING_BYTES_PW, true);

	ret = i3c_ibi_rsrcs_init(gi3c, pdev);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
			"Error: %d, i3c_ibi_rsrcs_init\n", ret);
		goto geni_resources_off;
	}

	se_geni_resources_off(&gi3c->se.i3c_rsc);
	GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
		"i3c fifo/se-dma mode. fifo depth:%d\n", tx_depth);

	pm_runtime_set_suspended(gi3c->se.dev);
	pm_runtime_set_autosuspend_delay(gi3c->se.dev, I3C_AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(gi3c->se.dev);
	pm_runtime_enable(gi3c->se.dev);


	ret = i3c_master_register(&gi3c->ctrlr, &pdev->dev,
		&geni_i3c_master_ops, false);
	if (ret) {
		GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev,
			"I3C master registration failed=%d, continue\n", ret);

		/* NOTE : This may fail on 7E NACK, but should return 0 */
		ret = 0;
	}

	// hot-join
	gi3c->hj_wl = wakeup_source_register(gi3c->se.dev,
					     dev_name(gi3c->se.dev));
	if (!gi3c->hj_wl) {
		GENI_SE_ERR(gi3c->ipcl, false, gi3c->se.dev,
					"wakeup source registration failed\n");
		se_geni_resources_off(&gi3c->se.i3c_rsc);
		return -ENOMEM;
	}

	INIT_WORK(&gi3c->hj_wd, geni_i3c_hotjoin);
	gi3c->hj_wq = alloc_workqueue("%s", 0, 0, dev_name(gi3c->se.dev));
	geni_i3c_enable_hotjoin_irq(gi3c, true);

	GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev, "I3C probed:%d\n", ret);
	return ret;

geni_resources_off:
	se_geni_resources_off(&gi3c->se.i3c_rsc);

cleanup_init:
	GENI_SE_ERR(gi3c->ipcl, true, gi3c->se.dev, "I3C probe failed\n");
	return ret;
}

static int geni_i3c_remove(struct platform_device *pdev)
{
	struct geni_i3c_dev *gi3c = platform_get_drvdata(pdev);
	int ret = 0, i;

	//Disable hot-join, until next probe happens
	geni_i3c_enable_hotjoin_irq(gi3c, false);
	destroy_workqueue(gi3c->hj_wq);
	wakeup_source_unregister(gi3c->hj_wl);

	if (gi3c->ibi.is_init)
		qcom_geni_i3c_ibi_unconf(gi3c);
	geni_i3c_enable_ibi_ctrl(gi3c, false);

	/* Potentially to be done before pinctrl change */
	geni_i3c_enable_ibi_irq(gi3c, false);

	/*force suspend to avoid the auto suspend caused by driver removal*/
	pm_runtime_force_suspend(gi3c->se.dev);
	ret = pinctrl_select_state(gi3c->se.i3c_rsc.geni_pinctrl,
			gi3c->i3c_gpio_disable);
	if (ret)
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
			" i3c: pinctrl_select_state failed\n");

	ret = i3c_master_unregister(&gi3c->ctrlr);
	/* TBD : If we need debug for previous session, Don't delete logs */
	if (gi3c->ipcl)
		ipc_log_context_destroy(gi3c->ipcl);

	for (i = 0; i < i3c_nos; i++)
		i3c_geni_dev[i] = NULL;
	i3c_nos = 0;

	return ret;
}

static int geni_i3c_resume_early(struct device *dev)
{
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int geni_i3c_runtime_suspend(struct device *dev)
{
	struct geni_i3c_dev *gi3c = dev_get_drvdata(dev);

	disable_irq(gi3c->irq);
	se_geni_resources_off(&gi3c->se.i3c_rsc);
	return 0;
}

static int geni_i3c_runtime_resume(struct device *dev)
{
	int ret;
	struct geni_i3c_dev *gi3c = dev_get_drvdata(dev);

	ret = se_geni_resources_on(&gi3c->se.i3c_rsc);
	if (ret)
		return ret;

	enable_irq(gi3c->irq);

	/* Enable TLMM I3C MODE registers */
	return 0;
}

static int geni_i3c_suspend_late(struct device *dev)
{
	struct geni_i3c_dev *gi3c = dev_get_drvdata(dev);

	if (!pm_runtime_status_suspended(dev)) {
		GENI_SE_DBG(gi3c->ipcl, false, gi3c->se.dev,
				"%s: Forced suspend\n", __func__);
		geni_i3c_runtime_suspend(dev);
		pm_runtime_disable(dev);
		pm_runtime_put_noidle(dev);
		pm_runtime_set_suspended(dev);
		pm_runtime_enable(dev);
	}
	return 0;
}
#else
static int geni_i3c_runtime_suspend(struct device *dev)
{
	return 0;
}

static int geni_i3c_runtime_resume(struct device *dev)
{
	return 0;
}

static int geni_i3c_suspend_late(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops geni_i3c_pm_ops = {
	.suspend_late = geni_i3c_suspend_late,
	.resume_early = geni_i3c_resume_early,
	.runtime_suspend = geni_i3c_runtime_suspend,
	.runtime_resume  = geni_i3c_runtime_resume,
};

static const struct of_device_id geni_i3c_dt_match[] = {
	{ .compatible = "qcom,geni-i3c" },
	{ }
};
MODULE_DEVICE_TABLE(of, geni_i3c_dt_match);

static struct platform_driver geni_i3c_master = {
	.probe  = geni_i3c_probe,
	.remove = geni_i3c_remove,
	.driver = {
		.name = "geni_i3c_master",
		.pm = &geni_i3c_pm_ops,
		.of_match_table = geni_i3c_dt_match,
	},
};

static int __init i3c_dev_init(void)
{
	return platform_driver_register(&geni_i3c_master);
}

static void __exit i3c_dev_exit(void)
{
	platform_driver_unregister(&geni_i3c_master);
}

module_init(i3c_dev_init);
module_exit(i3c_dev_exit);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:geni_i3c_master");
