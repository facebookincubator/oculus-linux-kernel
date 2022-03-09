/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/bitmap.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spmi.h>

/* PMIC Arbiter configuration registers */
#define PMIC_ARB_VERSION		0x0000
#define PMIC_ARB_VERSION_V2_MIN		0x20010000
#define PMIC_ARB_VERSION_V3_MIN		0x30000000
#define PMIC_ARB_VERSION_V5_MIN		0x50000000
#define PMIC_ARB_INT_EN			0x0004

/* PMIC Arbiter channel registers offsets */
#define PMIC_ARB_CMD			0x00
#define PMIC_ARB_CONFIG			0x04
#define PMIC_ARB_STATUS			0x08
#define PMIC_ARB_WDATA0			0x10
#define PMIC_ARB_WDATA1			0x14
#define PMIC_ARB_RDATA0			0x18
#define PMIC_ARB_RDATA1			0x1C

/* Mapping Table */
#define SPMI_MAPPING_TABLE_REG(N)	(0x0B00 + (4 * (N)))
#define SPMI_MAPPING_BIT_INDEX(X)	(((X) >> 18) & 0xF)
#define SPMI_MAPPING_BIT_IS_0_FLAG(X)	(((X) >> 17) & 0x1)
#define SPMI_MAPPING_BIT_IS_0_RESULT(X)	(((X) >> 9) & 0xFF)
#define SPMI_MAPPING_BIT_IS_1_FLAG(X)	(((X) >> 8) & 0x1)
#define SPMI_MAPPING_BIT_IS_1_RESULT(X)	(((X) >> 0) & 0xFF)

#define SPMI_MAPPING_TABLE_TREE_DEPTH	16	/* Maximum of 16-bits */
#define PMIC_ARB_MAX_PPID		BIT(12) /* PPID is 12bit */
#define PMIC_ARB_CHAN_VALID		BIT(15)
#define PMIC_ARB_CHAN_IS_IRQ_OWNER(reg)	((reg) & BIT(24))
#define INVALID_EE			(-1)

/* Ownership Table */
#define SPMI_OWNERSHIP_TABLE_REG(N)	(0x0700 + (4 * (N)))
#define SPMI_OWNERSHIP_PERIPH2OWNER(X)	((X) & 0x7)

#define SPMI_PROTOCOL_IRQ_STATUS	0x6000

/* Channel Status fields */
enum pmic_arb_chnl_status {
	PMIC_ARB_STATUS_DONE	= BIT(0),
	PMIC_ARB_STATUS_FAILURE	= BIT(1),
	PMIC_ARB_STATUS_DENIED	= BIT(2),
	PMIC_ARB_STATUS_DROPPED	= BIT(3),
};

/* Command register fields */
#define PMIC_ARB_CMD_MAX_BYTE_COUNT	8

/* Command Opcodes */
enum pmic_arb_cmd_op_code {
	PMIC_ARB_OP_EXT_WRITEL = 0,
	PMIC_ARB_OP_EXT_READL = 1,
	PMIC_ARB_OP_EXT_WRITE = 2,
	PMIC_ARB_OP_RESET = 3,
	PMIC_ARB_OP_SLEEP = 4,
	PMIC_ARB_OP_SHUTDOWN = 5,
	PMIC_ARB_OP_WAKEUP = 6,
	PMIC_ARB_OP_AUTHENTICATE = 7,
	PMIC_ARB_OP_MSTR_READ = 8,
	PMIC_ARB_OP_MSTR_WRITE = 9,
	PMIC_ARB_OP_EXT_READ = 13,
	PMIC_ARB_OP_WRITE = 14,
	PMIC_ARB_OP_READ = 15,
	PMIC_ARB_OP_ZERO_WRITE = 16,
};

/*
 * PMIC arbiter version 5 uses different register offsets for read/write vs
 * observer channels.
 */
enum pmic_arb_channel {
	PMIC_ARB_CHANNEL_RW,
	PMIC_ARB_CHANNEL_OBS,
};

/* Maximum number of support PMIC peripherals */
#define PMIC_ARB_MAX_PERIPHS		512
#define PMIC_ARB_TIMEOUT_US		100
#define PMIC_ARB_MAX_TRANS_BYTES	(8)

#define PMIC_ARB_APID_MASK		0xFF
#define PMIC_ARB_PPID_MASK		0xFFF

/* interrupt enable bit */
#define SPMI_PIC_ACC_ENABLE_BIT		BIT(0)

#define HWIRQ(slave_id, periph_id, irq_id, apid) \
	((((slave_id) & 0xF)   << 28) | \
	(((periph_id) & 0xFF)  << 20) | \
	(((irq_id)    & 0x7)   << 16) | \
	(((apid)      & 0x1FF) << 0))

#define HWIRQ_SID(hwirq)  (((hwirq) >> 28) & 0xF)
#define HWIRQ_PER(hwirq)  (((hwirq) >> 20) & 0xFF)
#define HWIRQ_IRQ(hwirq)  (((hwirq) >> 16) & 0x7)
#define HWIRQ_APID(hwirq) (((hwirq) >> 0)  & 0x1FF)

struct pmic_arb_ver_ops;

struct apid_data {
	u16		ppid;
	u8		write_owner;
	u8		irq_owner;
};

/**
 * spmi_pmic_arb - SPMI PMIC Arbiter object
 *
 * @rd_base:		on v1 "core", on v2 "observer" register base off DT.
 * @wr_base:		on v1 "core", on v2 "chnls"    register base off DT.
 * @intr:		address of the SPMI interrupt control registers.
 * @acc_status:		address of SPMI ACC interrupt status registers.
 * @cnfg:		address of the PMIC Arbiter configuration registers.
 * @lock:		lock to synchronize accesses.
 * @channel:		execution environment channel to use for accesses.
 * @irq:		PMIC ARB interrupt.
 * @ee:			the current Execution Environment
 * @min_apid:		minimum APID (used for bounding IRQ search)
 * @max_apid:		maximum APID
 * @max_periph:		maximum number of PMIC peripherals supported by HW.
 * @mapping_table:	in-memory copy of PPID -> APID mapping table.
 * @domain:		irq domain object for PMIC IRQ domain
 * @spmic:		SPMI controller object
 * @ver_ops:		version dependent operations.
 * @ppid_to_apid	in-memory copy of PPID -> channel (APID) mapping table.
 *			v2 only.
 * @ahb_bus_wa:		Use AHB bus workaround to avoid write transaction
 *			corruption on some PMIC arbiter v5 platforms.
 */
struct spmi_pmic_arb {
	void __iomem		*rd_base;
	void __iomem		*wr_base;
	void __iomem		*intr;
	void __iomem		*acc_status;
	void __iomem		*cnfg;
	void __iomem		*core;
	resource_size_t		core_size;
	raw_spinlock_t		lock;
	u8			channel;
	int			irq;
	u8			ee;
	u16			min_apid;
	u16			max_apid;
	u16			max_periph;
	u32			*mapping_table;
	DECLARE_BITMAP(mapping_table_valid, PMIC_ARB_MAX_PERIPHS);
	struct irq_domain	*domain;
	struct spmi_controller	*spmic;
	const struct pmic_arb_ver_ops *ver_ops;
	u16			*ppid_to_apid;
	u16			last_apid;
	struct apid_data	apid_data[PMIC_ARB_MAX_PERIPHS];
	bool			ahb_bus_wa;
};

/**
 * pmic_arb_ver: version dependent functionality.
 *
 * @ver_str:		version string.
 * @ppid_to_apid:	finds the apid for a given ppid.
 * @mode:		access rights to specified pmic peripheral.
 * @non_data_cmd:	on v1 issues an spmi non-data command.
 *			on v2 no HW support, returns -EOPNOTSUPP.
 * @offset:		on v1 offset of per-ee channel.
 *			on v2 offset of per-ee and per-ppid channel.
 * @fmt_cmd:		formats a GENI/SPMI command.
 * @owner_acc_status:	on v1 offset of PMIC_ARB_SPMI_PIC_OWNERm_ACC_STATUSn
 *			on v2 offset of SPMI_PIC_OWNERm_ACC_STATUSn.
 * @acc_enable:		on v1 offset of PMIC_ARB_SPMI_PIC_ACC_ENABLEn
 *			on v2 offset of SPMI_PIC_ACC_ENABLEn.
 * @irq_status:		on v1 offset of PMIC_ARB_SPMI_PIC_IRQ_STATUSn
 *			on v2 offset of SPMI_PIC_IRQ_STATUSn.
 * @irq_clear:		on v1 offset of PMIC_ARB_SPMI_PIC_IRQ_CLEARn
 *			on v2 offset of SPMI_PIC_IRQ_CLEARn.
 * @channel_map_offset:	offset of PMIC_ARB_REG_CHNLn
 */
struct pmic_arb_ver_ops {
	const char *ver_str;
	int (*ppid_to_apid)(struct spmi_pmic_arb *pa, u8 sid, u16 addr,
			u16 *apid);
	int (*mode)(struct spmi_pmic_arb *dev, u8 sid, u16 addr,
			mode_t *mode);
	/* spmi commands (read_cmd, write_cmd, cmd) functionality */
	int (*offset)(struct spmi_pmic_arb *dev, u8 sid, u16 addr,
			enum pmic_arb_channel ch_type, u32 *offset);
	u32 (*fmt_cmd)(u8 opc, u8 sid, u16 addr, u8 bc);
	int (*non_data_cmd)(struct spmi_controller *ctrl, u8 opc, u8 sid);
	/* Interrupts controller functionality (offset of PIC registers) */
	u32 (*owner_acc_status)(u8 m, u16 n);
	u32 (*acc_enable)(u16 n);
	u32 (*irq_status)(u16 n);
	u32 (*irq_clear)(u16 n);
	u32 (*channel_map_offset)(u16 n);
};

static inline void pmic_arb_base_write(struct spmi_pmic_arb *pa,
				       u32 offset, u32 val)
{
	if (pa->ahb_bus_wa) {
		/* AHB bus register dummy read for workaround. */
		readl_relaxed(pa->cnfg + SPMI_PROTOCOL_IRQ_STATUS);
		/*
		 * Ensure that the read completes before initiating the
		 * subsequent register write.
		 */
		mb();
	}

	writel_relaxed(val, pa->wr_base + offset);
}

static inline void pmic_arb_set_rd_cmd(struct spmi_pmic_arb *pa,
				       u32 offset, u32 val)
{
	writel_relaxed(val, pa->rd_base + offset);
}

/**
 * pa_read_data: reads pmic-arb's register and copy 1..4 bytes to buf
 * @bc:		byte count -1. range: 0..3
 * @reg:	register's address
 * @buf:	output parameter, length must be bc + 1
 */
static void pa_read_data(struct spmi_pmic_arb *pa, u8 *buf, u32 reg, u8 bc)
{
	u32 data = __raw_readl(pa->rd_base + reg);

	memcpy(buf, &data, (bc & 3) + 1);
}

/**
 * pa_write_data: write 1..4 bytes from buf to pmic-arb's register
 * @bc:		byte-count -1. range: 0..3.
 * @reg:	register's address.
 * @buf:	buffer to write. length must be bc + 1.
 */
static void
pa_write_data(struct spmi_pmic_arb *pa, const u8 *buf, u32 reg, u8 bc)
{
	u32 data = 0;

	memcpy(&data, buf, (bc & 3) + 1);
	pmic_arb_base_write(pa, reg, data);
}

static int pmic_arb_wait_for_done(struct spmi_controller *ctrl,
				  void __iomem *base, u8 sid, u16 addr,
				  enum pmic_arb_channel ch_type)
{
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	u32 status = 0;
	u32 timeout = PMIC_ARB_TIMEOUT_US;
	u32 offset;
	int rc;

	rc = pa->ver_ops->offset(pa, sid, addr, ch_type, &offset);
	if (rc)
		return rc;

	offset += PMIC_ARB_STATUS;

	while (timeout--) {
		status = readl_relaxed(base + offset);

		if (status & PMIC_ARB_STATUS_DONE) {
			if (status & PMIC_ARB_STATUS_DENIED) {
				dev_err(&ctrl->dev,
					"%s: transaction denied (0x%x)\n",
					__func__, status);
				return -EPERM;
			}

			if (status & PMIC_ARB_STATUS_FAILURE) {
				dev_err(&ctrl->dev,
					"%s: transaction failed (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			if (status & PMIC_ARB_STATUS_DROPPED) {
				dev_err(&ctrl->dev,
					"%s: transaction dropped (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			return 0;
		}
		udelay(1);
	}

	dev_err(&ctrl->dev,
		"%s: timeout, status 0x%x\n",
		__func__, status);
	return -ETIMEDOUT;
}

static int
pmic_arb_non_data_cmd_v1(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	unsigned long flags;
	u32 cmd;
	int rc;
	u32 offset;

	rc = pa->ver_ops->offset(pa, sid, 0, PMIC_ARB_CHANNEL_RW, &offset);
	if (rc)
		return rc;

	cmd = ((opc | 0x40) << 27) | ((sid & 0xf) << 20);

	raw_spin_lock_irqsave(&pa->lock, flags);
	pmic_arb_base_write(pa, offset + PMIC_ARB_CMD, cmd);
	rc = pmic_arb_wait_for_done(ctrl, pa->wr_base, sid, 0,
				    PMIC_ARB_CHANNEL_RW);
	raw_spin_unlock_irqrestore(&pa->lock, flags);

	return rc;
}

static int
pmic_arb_non_data_cmd_v2(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	return -EOPNOTSUPP;
}

/* Non-data command */
static int pmic_arb_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid)
{
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);

	dev_dbg(&ctrl->dev, "cmd op:0x%x sid:%d\n", opc, sid);

	/* Check for valid non-data command */
	if (opc < SPMI_CMD_RESET || opc > SPMI_CMD_WAKEUP)
		return -EINVAL;

	return pa->ver_ops->non_data_cmd(ctrl, opc, sid);
}

static int pmic_arb_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			     u16 addr, u8 *buf, size_t len)
{
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	unsigned long flags;
	u8 bc = len - 1;
	u32 cmd;
	int rc;
	u32 offset;
	mode_t mode;

	rc = pa->ver_ops->offset(pa, sid, addr, PMIC_ARB_CHANNEL_OBS, &offset);
	if (rc)
		return rc;

	rc = pa->ver_ops->mode(pa, sid, addr, &mode);
	if (rc)
		return rc;

	if (!(mode & 0400)) {
		dev_err(&pa->spmic->dev,
			"error: impermissible read from peripheral sid:%d addr:0x%x\n",
			sid, addr);
		return -ENODEV;
	}

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(&ctrl->dev,
			"pmic-arb supports 1..%d bytes per trans, but:%zu requested",
			PMIC_ARB_MAX_TRANS_BYTES, len);
		return  -EINVAL;
	}

	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7F)
		opc = PMIC_ARB_OP_READ;
	else if (opc >= 0x20 && opc <= 0x2F)
		opc = PMIC_ARB_OP_EXT_READ;
	else if (opc >= 0x38 && opc <= 0x3F)
		opc = PMIC_ARB_OP_EXT_READL;
	else
		return -EINVAL;

	cmd = pa->ver_ops->fmt_cmd(opc, sid, addr, bc);

	raw_spin_lock_irqsave(&pa->lock, flags);
	pmic_arb_set_rd_cmd(pa, offset + PMIC_ARB_CMD, cmd);
	rc = pmic_arb_wait_for_done(ctrl, pa->rd_base, sid, addr,
				    PMIC_ARB_CHANNEL_OBS);
	if (rc)
		goto done;

	pa_read_data(pa, buf, offset + PMIC_ARB_RDATA0,
		     min_t(u8, bc, 3));

	if (bc > 3)
		pa_read_data(pa, buf + 4, offset + PMIC_ARB_RDATA1, bc - 4);

done:
	raw_spin_unlock_irqrestore(&pa->lock, flags);
	return rc;
}

static int pmic_arb_write_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			      u16 addr, const u8 *buf, size_t len)
{
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	unsigned long flags;
	u8 bc = len - 1;
	u32 cmd;
	int rc;
	u32 offset;
	mode_t mode;

	rc = pa->ver_ops->offset(pa, sid, addr, PMIC_ARB_CHANNEL_RW, &offset);
	if (rc)
		return rc;

	rc = pa->ver_ops->mode(pa, sid, addr, &mode);
	if (rc)
		return rc;

	if (!(mode & 0200)) {
		dev_err(&pa->spmic->dev,
			"error: impermissible write to peripheral sid:%d addr:0x%x\n",
			sid, addr);
		return -ENODEV;
	}

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(&ctrl->dev,
			"pmic-arb supports 1..%d bytes per trans, but:%zu requested",
			PMIC_ARB_MAX_TRANS_BYTES, len);
		return  -EINVAL;
	}

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIC_ARB_OP_WRITE;
	else if (opc >= 0x00 && opc <= 0x0F)
		opc = PMIC_ARB_OP_EXT_WRITE;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIC_ARB_OP_EXT_WRITEL;
	else if (opc >= 0x80)
		opc = PMIC_ARB_OP_ZERO_WRITE;
	else
		return -EINVAL;

	cmd = pa->ver_ops->fmt_cmd(opc, sid, addr, bc);

	/* Write data to FIFOs */
	raw_spin_lock_irqsave(&pa->lock, flags);
	pa_write_data(pa, buf, offset + PMIC_ARB_WDATA0, min_t(u8, bc, 3));
	if (bc > 3)
		pa_write_data(pa, buf + 4, offset + PMIC_ARB_WDATA1, bc - 4);

	/* Start the transaction */
	pmic_arb_base_write(pa, offset + PMIC_ARB_CMD, cmd);
	rc = pmic_arb_wait_for_done(ctrl, pa->wr_base, sid, addr,
				    PMIC_ARB_CHANNEL_RW);
	raw_spin_unlock_irqrestore(&pa->lock, flags);

	return rc;
}

enum qpnpint_regs {
	QPNPINT_REG_RT_STS		= 0x10,
	QPNPINT_REG_SET_TYPE		= 0x11,
	QPNPINT_REG_POLARITY_HIGH	= 0x12,
	QPNPINT_REG_POLARITY_LOW	= 0x13,
	QPNPINT_REG_LATCHED_CLR		= 0x14,
	QPNPINT_REG_EN_SET		= 0x15,
	QPNPINT_REG_EN_CLR		= 0x16,
	QPNPINT_REG_LATCHED_STS		= 0x18,
};

struct spmi_pmic_arb_qpnpint_type {
	u8 type; /* 1 -> edge */
	u8 polarity_high;
	u8 polarity_low;
} __packed;

/* Simplified accessor functions for irqchip callbacks */
static void qpnpint_spmi_write(struct irq_data *d, u8 reg, void *buf,
			       size_t len)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 sid = HWIRQ_SID(d->hwirq);
	u8 per = HWIRQ_PER(d->hwirq);

	if (pmic_arb_write_cmd(pa->spmic, SPMI_CMD_EXT_WRITEL, sid,
			       (per << 8) + reg, buf, len))
		dev_err_ratelimited(&pa->spmic->dev,
				"failed irqchip transaction on %x\n",
				    d->irq);
}

static void qpnpint_spmi_read(struct irq_data *d, u8 reg, void *buf, size_t len)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 sid = HWIRQ_SID(d->hwirq);
	u8 per = HWIRQ_PER(d->hwirq);

	if (pmic_arb_read_cmd(pa->spmic, SPMI_CMD_EXT_READL, sid,
			      (per << 8) + reg, buf, len))
		dev_err_ratelimited(&pa->spmic->dev,
				"failed irqchip transaction on %x\n",
				    d->irq);
}

static void cleanup_irq(struct spmi_pmic_arb *pa, u16 apid, int id)
{
	u16 ppid = pa->apid_data[apid].ppid;
	u8 sid = ppid >> 8;
	u8 per = ppid & 0xFF;
	u8 irq_mask = BIT(id);

	dev_err_ratelimited(&pa->spmic->dev,
		"cleanup_irq apid=%d sid=0x%x per=0x%x irq=%d\n",
		apid, sid, per, id);
	writel_relaxed(irq_mask, pa->intr + pa->ver_ops->irq_clear(apid));
}

static void periph_interrupt(struct spmi_pmic_arb *pa, u16 apid)
{
	unsigned int irq;
	u32 status, id;
	u8 sid = (pa->apid_data[apid].ppid >> 8) & 0xF;
	u8 per = pa->apid_data[apid].ppid & 0xFF;

	status = readl_relaxed(pa->intr + pa->ver_ops->irq_status(apid));
	while (status) {
		id = ffs(status) - 1;
		status &= ~BIT(id);
		irq = irq_find_mapping(pa->domain, HWIRQ(sid, per, id, apid));
		if (irq == 0) {
			cleanup_irq(pa, apid, id);
			continue;
		}
		generic_handle_irq(irq);
	}
}

static void pmic_arb_chained_irq(struct irq_desc *desc)
{
	struct spmi_pmic_arb *pa = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int first = pa->min_apid >> 5;
	int last = pa->max_apid >> 5;
	u32 status, enable;
	int i, id, apid;
	/* status based dispatch */
	bool acc_valid = false;
	u32 irq_status = 0;

	chained_irq_enter(chip, desc);

	for (i = first; i <= last; ++i) {
		status = readl_relaxed(pa->acc_status +
				      pa->ver_ops->owner_acc_status(pa->ee, i));
		if (status)
			acc_valid = true;

		while (status) {
			id = ffs(status) - 1;
			status &= ~BIT(id);
			apid = id + i * 32;
			if (apid < pa->min_apid || apid > pa->max_apid) {
				WARN_ONCE(true, "spurious spmi irq received for apid=%d\n",
					apid);
				continue;
			}
			enable = readl_relaxed(pa->intr +
					pa->ver_ops->acc_enable(apid));
			if (enable & SPMI_PIC_ACC_ENABLE_BIT)
				periph_interrupt(pa, apid);
		}
	}

	/* ACC_STATUS is empty but IRQ fired check IRQ_STATUS */
	if (!acc_valid) {
		for (i = pa->min_apid; i <= pa->max_apid; i++) {
			/* skip if APPS is not irq owner */
			if (pa->apid_data[i].irq_owner != pa->ee)
				continue;

			irq_status = readl_relaxed(pa->intr +
						pa->ver_ops->irq_status(i));
			if (irq_status) {
				enable = readl_relaxed(pa->intr +
						pa->ver_ops->acc_enable(i));
				if (enable & SPMI_PIC_ACC_ENABLE_BIT) {
					dev_dbg(&pa->spmic->dev,
						"Dispatching IRQ for apid=%d status=%x\n",
						i, irq_status);
					periph_interrupt(pa, i);
				}
			}
		}
	}

	chained_irq_exit(chip, desc);
}

static void qpnpint_irq_ack(struct irq_data *d)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u16 apid = HWIRQ_APID(d->hwirq);
	u8 data;

	writel_relaxed(BIT(irq), pa->intr + pa->ver_ops->irq_clear(apid));

	data = BIT(irq);
	qpnpint_spmi_write(d, QPNPINT_REG_LATCHED_CLR, &data, 1);
}

static void qpnpint_irq_mask(struct irq_data *d)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 data = BIT(irq);

	qpnpint_spmi_write(d, QPNPINT_REG_EN_CLR, &data, 1);
}

static void qpnpint_irq_unmask(struct irq_data *d)
{
	struct spmi_pmic_arb *pa = irq_data_get_irq_chip_data(d);
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u16 apid = HWIRQ_APID(d->hwirq);
	u8 buf[2];

	writel_relaxed(SPMI_PIC_ACC_ENABLE_BIT,
		pa->intr + pa->ver_ops->acc_enable(apid));

	qpnpint_spmi_read(d, QPNPINT_REG_EN_SET, &buf[0], 1);
	if (!(buf[0] & BIT(irq))) {
		/*
		 * Since the interrupt is currently disabled, write to both the
		 * LATCHED_CLR and EN_SET registers so that a spurious interrupt
		 * cannot be triggered when the interrupt is enabled
		 */
		buf[0] = BIT(irq);
		buf[1] = BIT(irq);
		qpnpint_spmi_write(d, QPNPINT_REG_LATCHED_CLR, &buf, 2);
	}
}

static int qpnpint_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct spmi_pmic_arb_qpnpint_type type;
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 bit_mask_irq = BIT(irq);

	qpnpint_spmi_read(d, QPNPINT_REG_SET_TYPE, &type, sizeof(type));

	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		type.type |= bit_mask_irq;
		if (flow_type & IRQF_TRIGGER_RISING)
			type.polarity_high |=  bit_mask_irq;
		else
			type.polarity_high &= ~bit_mask_irq;
		if (flow_type & IRQF_TRIGGER_FALLING)
			type.polarity_low  |=  bit_mask_irq;
		else
			type.polarity_low  &= ~bit_mask_irq;
	} else {
		if ((flow_type & (IRQF_TRIGGER_HIGH)) &&
		    (flow_type & (IRQF_TRIGGER_LOW)))
			return -EINVAL;

		type.type &= ~bit_mask_irq; /* level trig */
		if (flow_type & IRQF_TRIGGER_HIGH) {
			type.polarity_high |=  bit_mask_irq;
			type.polarity_low  &= ~bit_mask_irq;
		} else {
			type.polarity_low  |=  bit_mask_irq;
			type.polarity_high &= ~bit_mask_irq;
		}
	}

	qpnpint_spmi_write(d, QPNPINT_REG_SET_TYPE, &type, sizeof(type));

	if (flow_type & IRQ_TYPE_EDGE_BOTH)
		irq_set_handler_locked(d, handle_edge_irq);
	else
		irq_set_handler_locked(d, handle_level_irq);

	return 0;
}

static int qpnpint_get_irqchip_state(struct irq_data *d,
				     enum irqchip_irq_state which,
				     bool *state)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 status = 0;

	if (which != IRQCHIP_STATE_LINE_LEVEL)
		return -EINVAL;

	qpnpint_spmi_read(d, QPNPINT_REG_RT_STS, &status, 1);
	*state = !!(status & BIT(irq));

	return 0;
}

static int qpnpint_irq_request_resources(struct irq_data *d)
{
	struct spmi_pmic_arb *pmic_arb = irq_data_get_irq_chip_data(d);
	u16 periph = HWIRQ_PER(d->hwirq);
	u16 apid = HWIRQ_APID(d->hwirq);
	u16 sid = HWIRQ_SID(d->hwirq);
	u16 irq = HWIRQ_IRQ(d->hwirq);

	if (pmic_arb->apid_data[apid].irq_owner != pmic_arb->ee) {
		dev_err(&pmic_arb->spmic->dev, "failed to xlate sid = %#x, periph = %#x, irq = %u: ee=%u but owner=%u\n",
			sid, periph, irq, pmic_arb->ee,
			pmic_arb->apid_data[apid].irq_owner);
		return -ENODEV;
	}

	return 0;
}

static struct irq_chip pmic_arb_irqchip = {
	.name		= "pmic_arb",
	.irq_ack	= qpnpint_irq_ack,
	.irq_mask	= qpnpint_irq_mask,
	.irq_unmask	= qpnpint_irq_unmask,
	.irq_set_type	= qpnpint_irq_set_type,
	.irq_get_irqchip_state	= qpnpint_get_irqchip_state,
	.irq_request_resources = qpnpint_irq_request_resources,
	.flags		= IRQCHIP_MASK_ON_SUSPEND
			| IRQCHIP_SKIP_SET_WAKE,
};

static void qpnpint_irq_domain_activate(struct irq_domain *domain,
					struct irq_data *d)
{
	u8 irq = HWIRQ_IRQ(d->hwirq);
	u8 buf;

	buf = BIT(irq);
	qpnpint_spmi_write(d, QPNPINT_REG_EN_CLR, &buf, 1);
	qpnpint_spmi_write(d, QPNPINT_REG_LATCHED_CLR, &buf, 1);
}

static int qpnpint_irq_domain_dt_translate(struct irq_domain *d,
					   struct device_node *controller,
					   const u32 *intspec,
					   unsigned int intsize,
					   unsigned long *out_hwirq,
					   unsigned int *out_type)
{
	struct spmi_pmic_arb *pa = d->host_data;
	int rc;
	u16 apid;

	dev_dbg(&pa->spmic->dev,
		"intspec[0] 0x%1x intspec[1] 0x%02x intspec[2] 0x%02x\n",
		intspec[0], intspec[1], intspec[2]);

	if (irq_domain_get_of_node(d) != controller)
		return -EINVAL;
	if (intsize != 4)
		return -EINVAL;
	if (intspec[0] > 0xF || intspec[1] > 0xFF || intspec[2] > 0x7)
		return -EINVAL;

	rc = pa->ver_ops->ppid_to_apid(pa, intspec[0],
			(intspec[1] << 8), &apid);
	if (rc < 0) {
		dev_err(&pa->spmic->dev,
		"failed to xlate sid = 0x%x, periph = 0x%x, irq = %u rc = %d\n",
		intspec[0], intspec[1], intspec[2], rc);
		return rc;
	}

	/* Keep track of {max,min}_apid for bounding search during interrupt */
	if (apid > pa->max_apid)
		pa->max_apid = apid;
	if (apid < pa->min_apid)
		pa->min_apid = apid;

	*out_hwirq = HWIRQ(intspec[0], intspec[1], intspec[2], apid);
	*out_type  = intspec[3] & IRQ_TYPE_SENSE_MASK;

	dev_dbg(&pa->spmic->dev, "out_hwirq = %lu\n", *out_hwirq);

	return 0;
}

static int qpnpint_irq_domain_map(struct irq_domain *d,
				  unsigned int virq,
				  irq_hw_number_t hwirq)
{
	struct spmi_pmic_arb *pa = d->host_data;

	dev_dbg(&pa->spmic->dev, "virq = %u, hwirq = %lu\n", virq, hwirq);

	irq_set_chip_and_handler(virq, &pmic_arb_irqchip, handle_level_irq);
	irq_set_chip_data(virq, d->host_data);
	irq_set_noprobe(virq);
	return 0;
}

static int
pmic_arb_ppid_to_apid_v1(struct spmi_pmic_arb *pa, u8 sid, u16 addr, u16 *apid)
{
	u16 ppid = sid << 8 | ((addr >> 8) & 0xFF);
	u32 *mapping_table = pa->mapping_table;
	int index = 0, i;
	u16 apid_valid;
	u32 data;

	apid_valid = pa->ppid_to_apid[ppid];
	if (apid_valid & PMIC_ARB_CHAN_VALID) {
		*apid = (apid_valid & ~PMIC_ARB_CHAN_VALID);
		return 0;
	}

	for (i = 0; i < SPMI_MAPPING_TABLE_TREE_DEPTH; ++i) {
		if (!test_and_set_bit(index, pa->mapping_table_valid))
			mapping_table[index] = readl_relaxed(pa->cnfg +
						SPMI_MAPPING_TABLE_REG(index));

		data = mapping_table[index];

		if (ppid & BIT(SPMI_MAPPING_BIT_INDEX(data))) {
			if (SPMI_MAPPING_BIT_IS_1_FLAG(data)) {
				index = SPMI_MAPPING_BIT_IS_1_RESULT(data);
			} else {
				*apid = SPMI_MAPPING_BIT_IS_1_RESULT(data);
				pa->ppid_to_apid[ppid]
					= *apid | PMIC_ARB_CHAN_VALID;
				pa->apid_data[*apid].ppid = ppid;
				return 0;
			}
		} else {
			if (SPMI_MAPPING_BIT_IS_0_FLAG(data)) {
				index = SPMI_MAPPING_BIT_IS_0_RESULT(data);
			} else {
				*apid = SPMI_MAPPING_BIT_IS_0_RESULT(data);
				pa->ppid_to_apid[ppid]
					= *apid | PMIC_ARB_CHAN_VALID;
				pa->apid_data[*apid].ppid = ppid;
				return 0;
			}
		}
	}

	return -ENODEV;
}

static int
pmic_arb_mode_v1_v3(struct spmi_pmic_arb *pa, u8 sid, u16 addr, mode_t *mode)
{
	*mode = 0600;
	return 0;
}

/* v1 offset per ee */
static int
pmic_arb_offset_v1(struct spmi_pmic_arb *pa, u8 sid, u16 addr,
		   enum pmic_arb_channel ch_type, u32 *offset)
{
	*offset = 0x800 + 0x80 * pa->channel;
	return 0;
}

static u16 pmic_arb_find_apid(struct spmi_pmic_arb *pa, u16 ppid)
{
	u32 regval, offset;
	u16 apid;
	u16 id;

	/*
	 * PMIC_ARB_REG_CHNL is a table in HW mapping channel to ppid.
	 * ppid_to_apid is an in-memory invert of that table.
	 */
	for (apid = pa->last_apid; apid < pa->max_periph; apid++) {
		regval = readl_relaxed(pa->cnfg +
				      SPMI_OWNERSHIP_TABLE_REG(apid));
		pa->apid_data[apid].irq_owner
			= SPMI_OWNERSHIP_PERIPH2OWNER(regval);
		pa->apid_data[apid].write_owner = pa->apid_data[apid].irq_owner;

		offset = pa->ver_ops->channel_map_offset(apid);
		if (offset >= pa->core_size)
			break;

		regval = readl_relaxed(pa->core + offset);
		if (!regval)
			continue;

		id = (regval >> 8) & PMIC_ARB_PPID_MASK;
		pa->ppid_to_apid[id] = apid | PMIC_ARB_CHAN_VALID;
		pa->apid_data[apid].ppid = id;
		if (id == ppid) {
			apid |= PMIC_ARB_CHAN_VALID;
			break;
		}
	}
	pa->last_apid = apid & ~PMIC_ARB_CHAN_VALID;

	return apid;
}

static int
pmic_arb_ppid_to_apid_v2(struct spmi_pmic_arb *pa, u8 sid, u16 addr, u16 *apid)
{
	u16 ppid = (sid << 8) | (addr >> 8);
	u16 apid_valid;

	apid_valid = pa->ppid_to_apid[ppid];
	if (!(apid_valid & PMIC_ARB_CHAN_VALID))
		apid_valid = pmic_arb_find_apid(pa, ppid);
	if (!(apid_valid & PMIC_ARB_CHAN_VALID))
		return -ENODEV;

	*apid = (apid_valid & ~PMIC_ARB_CHAN_VALID);
	return 0;
}

static int pmic_arb_read_apid_map_v5(struct spmi_pmic_arb *pa)
{
	u32 regval, offset;
	u16 apid, prev_apid, ppid;
	bool valid, is_irq_owner;

	/*
	 * PMIC_ARB_REG_CHNL is a table in HW mapping APID (channel) to PPID.
	 * ppid_to_apid is an in-memory invert of that table.  In order to allow
	 * multiple EE's to write to a single PPID in arbiter version 5, there
	 * is more than one APID mapped to each PPID.  The owner field for each
	 * of these mappings specifies the EE which is allowed to write to the
	 * APID.  The owner of the last (highest) APID which has the IRQ owner
	 * bit set for a given PPID will receive interrupts from the PPID.
	 */
	for (apid = 0; apid < pa->max_periph; apid++) {
		offset = pa->ver_ops->channel_map_offset(apid);
		if (offset >= pa->core_size)
			break;

		regval = readl_relaxed(pa->core + offset);
		if (!regval)
			continue;
		ppid = (regval >> 8) & PMIC_ARB_PPID_MASK;
		is_irq_owner = PMIC_ARB_CHAN_IS_IRQ_OWNER(regval);

		regval = readl_relaxed(pa->cnfg +
				      SPMI_OWNERSHIP_TABLE_REG(apid));
		pa->apid_data[apid].write_owner
			= SPMI_OWNERSHIP_PERIPH2OWNER(regval);

		pa->apid_data[apid].irq_owner = is_irq_owner ?
			pa->apid_data[apid].write_owner : INVALID_EE;

		valid = pa->ppid_to_apid[ppid] & PMIC_ARB_CHAN_VALID;
		prev_apid = pa->ppid_to_apid[ppid] & ~PMIC_ARB_CHAN_VALID;

		if (!valid || pa->apid_data[apid].write_owner == pa->ee) {
			/* First PPID mapping or one for this EE */
			pa->ppid_to_apid[ppid] = apid | PMIC_ARB_CHAN_VALID;
		} else if (valid && is_irq_owner &&
		    pa->apid_data[prev_apid].write_owner == pa->ee) {
			/*
			 * Duplicate PPID mapping after the one for this EE;
			 * override the irq owner
			 */
			pa->apid_data[prev_apid].irq_owner
				= pa->apid_data[apid].irq_owner;
		}

		pa->apid_data[apid].ppid = ppid;
		pa->last_apid = apid;
	}

	/* Dump the mapping table for debug purposes. */
	dev_dbg(&pa->spmic->dev, "PPID APID Write-EE IRQ-EE\n");
	for (ppid = 0; ppid < PMIC_ARB_MAX_PPID; ppid++) {
		valid = pa->ppid_to_apid[ppid] & PMIC_ARB_CHAN_VALID;
		apid = pa->ppid_to_apid[ppid] & ~PMIC_ARB_CHAN_VALID;

		if (valid)
			dev_dbg(&pa->spmic->dev, "0x%03X %3u %2u %2u\n",
				ppid, apid, pa->apid_data[apid].write_owner,
				pa->apid_data[apid].irq_owner);
	}

	return 0;
}

static int
pmic_arb_ppid_to_apid_v5(struct spmi_pmic_arb *pa, u8 sid, u16 addr, u16 *apid)
{
	u16 ppid = (sid << 8) | (addr >> 8);

	if (!(pa->ppid_to_apid[ppid] & PMIC_ARB_CHAN_VALID))
		return -ENODEV;

	*apid = pa->ppid_to_apid[ppid] & ~PMIC_ARB_CHAN_VALID;

	return 0;
}

static int
pmic_arb_mode_v2(struct spmi_pmic_arb *pa, u8 sid, u16 addr, mode_t *mode)
{
	u16 apid;
	int rc;

	rc = pa->ver_ops->ppid_to_apid(pa, sid, addr, &apid);
	if (rc < 0)
		return rc;

	*mode = 0;
	*mode |= 0400;

	if (pa->ee == pa->apid_data[apid].write_owner)
		*mode |= 0200;
	return 0;
}

/* v2 offset per ppid and per ee */
static int
pmic_arb_offset_v2(struct spmi_pmic_arb *pa, u8 sid, u16 addr,
		   enum pmic_arb_channel ch_type, u32 *offset)
{
	u16 apid;
	int rc;

	rc = pmic_arb_ppid_to_apid_v2(pa, sid, addr, &apid);
	if (rc < 0)
		return rc;

	*offset = 0x1000 * pa->ee + 0x8000 * apid;
	return 0;
}

/*
 * v5 offset per ee and per apid for observer channels and per apid for
 * read/write channels.
 */
static int
pmic_arb_offset_v5(struct spmi_pmic_arb *pa, u8 sid, u16 addr,
		   enum pmic_arb_channel ch_type, u32 *offset)
{
	u16 apid;
	int rc;

	rc = pmic_arb_ppid_to_apid_v5(pa, sid, addr, &apid);
	if (rc < 0)
		return rc;

	*offset = (ch_type == PMIC_ARB_CHANNEL_OBS)
			? 0x10000 * pa->ee + 0x80 * apid
			: 0x10000 * apid;
	return 0;
}

static u32 pmic_arb_fmt_cmd_v1(u8 opc, u8 sid, u16 addr, u8 bc)
{
	return (opc << 27) | ((sid & 0xf) << 20) | (addr << 4) | (bc & 0x7);
}

static u32 pmic_arb_fmt_cmd_v2(u8 opc, u8 sid, u16 addr, u8 bc)
{
	return (opc << 27) | ((addr & 0xff) << 4) | (bc & 0x7);
}

static u32 pmic_arb_owner_acc_status_v1(u8 m, u16 n)
{
	return 0x20 * m + 0x4 * n;
}

static u32 pmic_arb_owner_acc_status_v2(u8 m, u16 n)
{
	return 0x100000 + 0x1000 * m + 0x4 * n;
}

static u32 pmic_arb_owner_acc_status_v3(u8 m, u16 n)
{
	return 0x200000 + 0x1000 * m + 0x4 * n;
}

static u32 pmic_arb_owner_acc_status_v5(u8 m, u16 n)
{
	return 0x10000 * m + 0x4 * n;
}

static u32 pmic_arb_acc_enable_v1(u16 n)
{
	return 0x200 + 0x4 * n;
}

static u32 pmic_arb_acc_enable_v2(u16 n)
{
	return 0x1000 * n;
}

static u32 pmic_arb_acc_enable_v5(u16 n)
{
	return 0x100 + 0x10000 * n;
}

static u32 pmic_arb_irq_status_v1(u16 n)
{
	return 0x600 + 0x4 * n;
}

static u32 pmic_arb_irq_status_v2(u16 n)
{
	return 0x4 + 0x1000 * n;
}

static u32 pmic_arb_irq_status_v5(u16 n)
{
	return 0x104 + 0x10000 * n;
}

static u32 pmic_arb_irq_clear_v1(u16 n)
{
	return 0xA00 + 0x4 * n;
}

static u32 pmic_arb_irq_clear_v2(u16 n)
{
	return 0x8 + 0x1000 * n;
}

static u32 pmic_arb_irq_clear_v5(u16 n)
{
	return 0x108 + 0x10000 * n;
}

static u32 pmic_arb_channel_map_offset_v2(u16 n)
{
	return 0x800 + 0x4 * n;
}

static u32 pmic_arb_channel_map_offset_v5(u16 n)
{
	return 0x900 + 0x4 * n;
}

static const struct pmic_arb_ver_ops pmic_arb_v1 = {
	.ver_str		= "v1",
	.ppid_to_apid		= pmic_arb_ppid_to_apid_v1,
	.mode			= pmic_arb_mode_v1_v3,
	.non_data_cmd		= pmic_arb_non_data_cmd_v1,
	.offset			= pmic_arb_offset_v1,
	.fmt_cmd		= pmic_arb_fmt_cmd_v1,
	.owner_acc_status	= pmic_arb_owner_acc_status_v1,
	.acc_enable		= pmic_arb_acc_enable_v1,
	.irq_status		= pmic_arb_irq_status_v1,
	.irq_clear		= pmic_arb_irq_clear_v1,
	.channel_map_offset	= pmic_arb_channel_map_offset_v2,
};

static const struct pmic_arb_ver_ops pmic_arb_v2 = {
	.ver_str		= "v2",
	.ppid_to_apid		= pmic_arb_ppid_to_apid_v2,
	.mode			= pmic_arb_mode_v2,
	.non_data_cmd		= pmic_arb_non_data_cmd_v2,
	.offset			= pmic_arb_offset_v2,
	.fmt_cmd		= pmic_arb_fmt_cmd_v2,
	.owner_acc_status	= pmic_arb_owner_acc_status_v2,
	.acc_enable		= pmic_arb_acc_enable_v2,
	.irq_status		= pmic_arb_irq_status_v2,
	.irq_clear		= pmic_arb_irq_clear_v2,
	.channel_map_offset	= pmic_arb_channel_map_offset_v2,
};

static const struct pmic_arb_ver_ops pmic_arb_v3 = {
	.ver_str		= "v3",
	.ppid_to_apid		= pmic_arb_ppid_to_apid_v2,
	.mode			= pmic_arb_mode_v1_v3,
	.non_data_cmd		= pmic_arb_non_data_cmd_v2,
	.offset			= pmic_arb_offset_v2,
	.fmt_cmd		= pmic_arb_fmt_cmd_v2,
	.owner_acc_status	= pmic_arb_owner_acc_status_v3,
	.acc_enable		= pmic_arb_acc_enable_v2,
	.irq_status		= pmic_arb_irq_status_v2,
	.irq_clear		= pmic_arb_irq_clear_v2,
	.channel_map_offset	= pmic_arb_channel_map_offset_v2,
};

static const struct pmic_arb_ver_ops pmic_arb_v5 = {
	.ver_str		= "v5",
	.ppid_to_apid		= pmic_arb_ppid_to_apid_v5,
	.mode			= pmic_arb_mode_v2,
	.non_data_cmd		= pmic_arb_non_data_cmd_v2,
	.offset			= pmic_arb_offset_v5,
	.fmt_cmd		= pmic_arb_fmt_cmd_v2,
	.owner_acc_status	= pmic_arb_owner_acc_status_v5,
	.acc_enable		= pmic_arb_acc_enable_v5,
	.irq_status		= pmic_arb_irq_status_v5,
	.irq_clear		= pmic_arb_irq_clear_v5,
	.channel_map_offset	= pmic_arb_channel_map_offset_v5,
};

static const struct irq_domain_ops pmic_arb_irq_domain_ops = {
	.map	= qpnpint_irq_domain_map,
	.xlate	= qpnpint_irq_domain_dt_translate,
	.activate	= qpnpint_irq_domain_activate,
};

static int spmi_pmic_arb_probe(struct platform_device *pdev)
{
	struct spmi_pmic_arb *pa;
	struct spmi_controller *ctrl;
	struct resource *res;
	void __iomem *core;
	u32 channel, ee, hw_ver;
	int err;

	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*pa));
	if (!ctrl)
		return -ENOMEM;

	pa = spmi_controller_get_drvdata(ctrl);
	pa->spmic = ctrl;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "core");
	if (!res) {
		dev_err(&pdev->dev, "core resource not specified\n");
		err = -EINVAL;
		goto err_put_ctrl;
	}

	pa->core_size = resource_size(res);
	if (pa->core_size <= 0x800) {
		dev_err(&pdev->dev, "core_size is smaller than 0x800. Failing Probe\n");
		err = -EINVAL;
		goto err_put_ctrl;
	}

	core = devm_ioremap_resource(&ctrl->dev, res);
	if (IS_ERR(core)) {
		err = PTR_ERR(core);
		goto err_put_ctrl;
	}

	pa->ppid_to_apid = devm_kcalloc(&ctrl->dev, PMIC_ARB_MAX_PPID,
					sizeof(*pa->ppid_to_apid), GFP_KERNEL);
	if (!pa->ppid_to_apid) {
		err = -ENOMEM;
		goto err_put_ctrl;
	}

	hw_ver = readl_relaxed(core + PMIC_ARB_VERSION);

	if (hw_ver < PMIC_ARB_VERSION_V2_MIN) {
		pa->ver_ops = &pmic_arb_v1;
		pa->wr_base = core;
		pa->rd_base = core;
	} else {
		pa->core = core;

		if (hw_ver < PMIC_ARB_VERSION_V3_MIN)
			pa->ver_ops = &pmic_arb_v2;
		else if (hw_ver < PMIC_ARB_VERSION_V5_MIN)
			pa->ver_ops = &pmic_arb_v3;
		else
			pa->ver_ops = &pmic_arb_v5;

		/* the apid to ppid table starts at PMIC_ARB_REG_CHNL0 */
		pa->max_periph
		     = (pa->core_size - pa->ver_ops->channel_map_offset(0)) / 4;

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "obsrvr");
		pa->rd_base = devm_ioremap_resource(&ctrl->dev, res);
		if (IS_ERR(pa->rd_base)) {
			err = PTR_ERR(pa->rd_base);
			goto err_put_ctrl;
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "chnls");
		pa->wr_base = devm_ioremap_resource(&ctrl->dev, res);
		if (IS_ERR(pa->wr_base)) {
			err = PTR_ERR(pa->wr_base);
			goto err_put_ctrl;
		}
	}

	dev_info(&ctrl->dev, "PMIC arbiter version %s (0x%x)\n",
		 pa->ver_ops->ver_str, hw_ver);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "intr");
	pa->intr = devm_ioremap_resource(&ctrl->dev, res);
	if (IS_ERR(pa->intr)) {
		err = PTR_ERR(pa->intr);
		goto err_put_ctrl;
	}
	pa->acc_status = pa->intr;

	/*
	 * PMIC arbiter v5 groups the IRQ control registers in the same hardware
	 * module as the read/write channels.
	 */
	if (hw_ver >= PMIC_ARB_VERSION_V5_MIN)
		pa->intr = pa->wr_base;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cnfg");
	pa->cnfg = devm_ioremap_resource(&ctrl->dev, res);
	if (IS_ERR(pa->cnfg)) {
		err = PTR_ERR(pa->cnfg);
		goto err_put_ctrl;
	}

	pa->irq = platform_get_irq_byname(pdev, "periph_irq");
	if (pa->irq < 0) {
		err = pa->irq;
		goto err_put_ctrl;
	}

	err = of_property_read_u32(pdev->dev.of_node, "qcom,channel", &channel);
	if (err) {
		dev_err(&pdev->dev, "channel unspecified.\n");
		goto err_put_ctrl;
	}

	if (channel > 5) {
		dev_err(&pdev->dev, "invalid channel (%u) specified.\n",
			channel);
		err = -EINVAL;
		goto err_put_ctrl;
	}

	pa->channel = channel;

	err = of_property_read_u32(pdev->dev.of_node, "qcom,ee", &ee);
	if (err) {
		dev_err(&pdev->dev, "EE unspecified.\n");
		goto err_put_ctrl;
	}

	if (ee > 5) {
		dev_err(&pdev->dev, "invalid EE (%u) specified\n", ee);
		err = -EINVAL;
		goto err_put_ctrl;
	}

	pa->ee = ee;

	pa->ahb_bus_wa = of_property_read_bool(pdev->dev.of_node,
					"qcom,enable-ahb-bus-workaround");

	pa->mapping_table = devm_kcalloc(&ctrl->dev, PMIC_ARB_MAX_PERIPHS - 1,
					sizeof(*pa->mapping_table), GFP_KERNEL);
	if (!pa->mapping_table) {
		err = -ENOMEM;
		goto err_put_ctrl;
	}

	/* Initialize max_apid/min_apid to the opposite bounds, during
	 * the irq domain translation, we are sure to update these */
	pa->max_apid = 0;
	pa->min_apid = PMIC_ARB_MAX_PERIPHS - 1;

	platform_set_drvdata(pdev, ctrl);
	raw_spin_lock_init(&pa->lock);

	ctrl->cmd = pmic_arb_cmd;
	ctrl->read_cmd = pmic_arb_read_cmd;
	ctrl->write_cmd = pmic_arb_write_cmd;

	if (hw_ver >= PMIC_ARB_VERSION_V5_MIN) {
		err = pmic_arb_read_apid_map_v5(pa);
		if (err) {
			dev_err(&pdev->dev, "could not read APID->PPID mapping table, rc= %d\n",
				err);
			goto err_put_ctrl;
		}
	}

	dev_dbg(&pdev->dev, "adding irq domain\n");
	pa->domain = irq_domain_add_tree(pdev->dev.of_node,
					 &pmic_arb_irq_domain_ops, pa);
	if (!pa->domain) {
		dev_err(&pdev->dev, "unable to create irq_domain\n");
		err = -ENOMEM;
		goto err_put_ctrl;
	}

	irq_set_chained_handler_and_data(pa->irq, pmic_arb_chained_irq, pa);
	enable_irq_wake(pa->irq);

	err = spmi_controller_add(ctrl);
	if (err)
		goto err_domain_remove;

	return 0;

err_domain_remove:
	irq_set_chained_handler_and_data(pa->irq, NULL, NULL);
	irq_domain_remove(pa->domain);
err_put_ctrl:
	spmi_controller_put(ctrl);
	return err;
}

static int spmi_pmic_arb_remove(struct platform_device *pdev)
{
	struct spmi_controller *ctrl = platform_get_drvdata(pdev);
	struct spmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);

	spmi_controller_remove(ctrl);
	irq_set_chained_handler_and_data(pa->irq, NULL, NULL);
	irq_domain_remove(pa->domain);
	spmi_controller_put(ctrl);
	return 0;
}

static const struct of_device_id spmi_pmic_arb_match_table[] = {
	{ .compatible = "qcom,spmi-pmic-arb", },
	{},
};
MODULE_DEVICE_TABLE(of, spmi_pmic_arb_match_table);

static struct platform_driver spmi_pmic_arb_driver = {
	.probe		= spmi_pmic_arb_probe,
	.remove		= spmi_pmic_arb_remove,
	.driver		= {
		.name	= "spmi_pmic_arb",
		.of_match_table = spmi_pmic_arb_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

int __init spmi_pmic_arb_init(void)
{
	return platform_driver_register(&spmi_pmic_arb_driver);
}
arch_initcall(spmi_pmic_arb_init);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:spmi_pmic_arb");
