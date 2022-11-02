/*
 * PTP 1588 clock for Freescale QorIQ 1588 timer
 *
 * Copyright (C) 2010 OMICRON electronics GmbH
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include <linux/fsl/ptp_qoriq.h>

/*
 * Register access functions
 */

/* Caller must hold qoriq_ptp->lock. */
static u64 tmr_cnt_read(struct qoriq_ptp *qoriq_ptp)
{
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;
	u64 ns;
	u32 lo, hi;

	lo = qoriq_read(&regs->ctrl_regs->tmr_cnt_l);
	hi = qoriq_read(&regs->ctrl_regs->tmr_cnt_h);
	ns = ((u64) hi) << 32;
	ns |= lo;
	return ns;
}

/* Caller must hold qoriq_ptp->lock. */
static void tmr_cnt_write(struct qoriq_ptp *qoriq_ptp, u64 ns)
{
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;
	u32 hi = ns >> 32;
	u32 lo = ns & 0xffffffff;

	qoriq_write(&regs->ctrl_regs->tmr_cnt_l, lo);
	qoriq_write(&regs->ctrl_regs->tmr_cnt_h, hi);
}

/* Caller must hold qoriq_ptp->lock. */
static void set_alarm(struct qoriq_ptp *qoriq_ptp)
{
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;
	u64 ns;
	u32 lo, hi;

	ns = tmr_cnt_read(qoriq_ptp) + 1500000000ULL;
	ns = div_u64(ns, 1000000000UL) * 1000000000ULL;
	ns -= qoriq_ptp->tclk_period;
	hi = ns >> 32;
	lo = ns & 0xffffffff;
	qoriq_write(&regs->alarm_regs->tmr_alarm1_l, lo);
	qoriq_write(&regs->alarm_regs->tmr_alarm1_h, hi);
}

/* Caller must hold qoriq_ptp->lock. */
static void set_fipers(struct qoriq_ptp *qoriq_ptp)
{
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;

	set_alarm(qoriq_ptp);
	qoriq_write(&regs->fiper_regs->tmr_fiper1, qoriq_ptp->tmr_fiper1);
	qoriq_write(&regs->fiper_regs->tmr_fiper2, qoriq_ptp->tmr_fiper2);
}

/*
 * Interrupt service routine
 */

static irqreturn_t isr(int irq, void *priv)
{
	struct qoriq_ptp *qoriq_ptp = priv;
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;
	struct ptp_clock_event event;
	u64 ns;
	u32 ack = 0, lo, hi, mask, val;

	val = qoriq_read(&regs->ctrl_regs->tmr_tevent);

	if (val & ETS1) {
		ack |= ETS1;
		hi = qoriq_read(&regs->etts_regs->tmr_etts1_h);
		lo = qoriq_read(&regs->etts_regs->tmr_etts1_l);
		event.type = PTP_CLOCK_EXTTS;
		event.index = 0;
		event.timestamp = ((u64) hi) << 32;
		event.timestamp |= lo;
		ptp_clock_event(qoriq_ptp->clock, &event);
	}

	if (val & ETS2) {
		ack |= ETS2;
		hi = qoriq_read(&regs->etts_regs->tmr_etts2_h);
		lo = qoriq_read(&regs->etts_regs->tmr_etts2_l);
		event.type = PTP_CLOCK_EXTTS;
		event.index = 1;
		event.timestamp = ((u64) hi) << 32;
		event.timestamp |= lo;
		ptp_clock_event(qoriq_ptp->clock, &event);
	}

	if (val & ALM2) {
		ack |= ALM2;
		if (qoriq_ptp->alarm_value) {
			event.type = PTP_CLOCK_ALARM;
			event.index = 0;
			event.timestamp = qoriq_ptp->alarm_value;
			ptp_clock_event(qoriq_ptp->clock, &event);
		}
		if (qoriq_ptp->alarm_interval) {
			ns = qoriq_ptp->alarm_value + qoriq_ptp->alarm_interval;
			hi = ns >> 32;
			lo = ns & 0xffffffff;
			spin_lock(&qoriq_ptp->lock);
			qoriq_write(&regs->alarm_regs->tmr_alarm2_l, lo);
			qoriq_write(&regs->alarm_regs->tmr_alarm2_h, hi);
			spin_unlock(&qoriq_ptp->lock);
			qoriq_ptp->alarm_value = ns;
		} else {
			qoriq_write(&regs->ctrl_regs->tmr_tevent, ALM2);
			spin_lock(&qoriq_ptp->lock);
			mask = qoriq_read(&regs->ctrl_regs->tmr_temask);
			mask &= ~ALM2EN;
			qoriq_write(&regs->ctrl_regs->tmr_temask, mask);
			spin_unlock(&qoriq_ptp->lock);
			qoriq_ptp->alarm_value = 0;
			qoriq_ptp->alarm_interval = 0;
		}
	}

	if (val & PP1) {
		ack |= PP1;
		event.type = PTP_CLOCK_PPS;
		ptp_clock_event(qoriq_ptp->clock, &event);
	}

	if (ack) {
		qoriq_write(&regs->ctrl_regs->tmr_tevent, ack);
		return IRQ_HANDLED;
	} else
		return IRQ_NONE;
}

/*
 * PTP clock operations
 */

static int ptp_qoriq_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	u64 adj, diff;
	u32 tmr_add;
	int neg_adj = 0;
	struct qoriq_ptp *qoriq_ptp = container_of(ptp, struct qoriq_ptp, caps);
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;

	if (scaled_ppm < 0) {
		neg_adj = 1;
		scaled_ppm = -scaled_ppm;
	}
	tmr_add = qoriq_ptp->tmr_add;
	adj = tmr_add;

	/* calculate diff as adj*(scaled_ppm/65536)/1000000
	 * and round() to the nearest integer
	 */
	adj *= scaled_ppm;
	diff = div_u64(adj, 8000000);
	diff = (diff >> 13) + ((diff >> 12) & 1);

	tmr_add = neg_adj ? tmr_add - diff : tmr_add + diff;

	qoriq_write(&regs->ctrl_regs->tmr_add, tmr_add);

	return 0;
}

static int ptp_qoriq_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	s64 now;
	unsigned long flags;
	struct qoriq_ptp *qoriq_ptp = container_of(ptp, struct qoriq_ptp, caps);

	spin_lock_irqsave(&qoriq_ptp->lock, flags);

	now = tmr_cnt_read(qoriq_ptp);
	now += delta;
	tmr_cnt_write(qoriq_ptp, now);
	set_fipers(qoriq_ptp);

	spin_unlock_irqrestore(&qoriq_ptp->lock, flags);

	return 0;
}

static int ptp_qoriq_gettime(struct ptp_clock_info *ptp,
			       struct timespec64 *ts)
{
	u64 ns;
	unsigned long flags;
	struct qoriq_ptp *qoriq_ptp = container_of(ptp, struct qoriq_ptp, caps);

	spin_lock_irqsave(&qoriq_ptp->lock, flags);

	ns = tmr_cnt_read(qoriq_ptp);

	spin_unlock_irqrestore(&qoriq_ptp->lock, flags);

	*ts = ns_to_timespec64(ns);

	return 0;
}

static int ptp_qoriq_settime(struct ptp_clock_info *ptp,
			       const struct timespec64 *ts)
{
	u64 ns;
	unsigned long flags;
	struct qoriq_ptp *qoriq_ptp = container_of(ptp, struct qoriq_ptp, caps);

	ns = timespec64_to_ns(ts);

	spin_lock_irqsave(&qoriq_ptp->lock, flags);

	tmr_cnt_write(qoriq_ptp, ns);
	set_fipers(qoriq_ptp);

	spin_unlock_irqrestore(&qoriq_ptp->lock, flags);

	return 0;
}

static int ptp_qoriq_enable(struct ptp_clock_info *ptp,
			      struct ptp_clock_request *rq, int on)
{
	struct qoriq_ptp *qoriq_ptp = container_of(ptp, struct qoriq_ptp, caps);
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;
	unsigned long flags;
	u32 bit, mask;

	switch (rq->type) {
	case PTP_CLK_REQ_EXTTS:
		switch (rq->extts.index) {
		case 0:
			bit = ETS1EN;
			break;
		case 1:
			bit = ETS2EN;
			break;
		default:
			return -EINVAL;
		}
		spin_lock_irqsave(&qoriq_ptp->lock, flags);
		mask = qoriq_read(&regs->ctrl_regs->tmr_temask);
		if (on)
			mask |= bit;
		else
			mask &= ~bit;
		qoriq_write(&regs->ctrl_regs->tmr_temask, mask);
		spin_unlock_irqrestore(&qoriq_ptp->lock, flags);
		return 0;

	case PTP_CLK_REQ_PPS:
		spin_lock_irqsave(&qoriq_ptp->lock, flags);
		mask = qoriq_read(&regs->ctrl_regs->tmr_temask);
		if (on)
			mask |= PP1EN;
		else
			mask &= ~PP1EN;
		qoriq_write(&regs->ctrl_regs->tmr_temask, mask);
		spin_unlock_irqrestore(&qoriq_ptp->lock, flags);
		return 0;

	default:
		break;
	}

	return -EOPNOTSUPP;
}

static const struct ptp_clock_info ptp_qoriq_caps = {
	.owner		= THIS_MODULE,
	.name		= "qoriq ptp clock",
	.max_adj	= 512000,
	.n_alarm	= 0,
	.n_ext_ts	= N_EXT_TS,
	.n_per_out	= 0,
	.n_pins		= 0,
	.pps		= 1,
	.adjfine	= ptp_qoriq_adjfine,
	.adjtime	= ptp_qoriq_adjtime,
	.gettime64	= ptp_qoriq_gettime,
	.settime64	= ptp_qoriq_settime,
	.enable		= ptp_qoriq_enable,
};

/**
 * qoriq_ptp_nominal_freq - calculate nominal frequency according to
 *			    reference clock frequency
 *
 * @clk_src: reference clock frequency
 *
 * The nominal frequency is the desired clock frequency.
 * It should be less than the reference clock frequency.
 * It should be a factor of 1000MHz.
 *
 * Return the nominal frequency
 */
static u32 qoriq_ptp_nominal_freq(u32 clk_src)
{
	u32 remainder = 0;

	clk_src /= 1000000;
	remainder = clk_src % 100;
	if (remainder) {
		clk_src -= remainder;
		clk_src += 100;
	}

	do {
		clk_src -= 100;

	} while (1000 % clk_src);

	return clk_src * 1000000;
}

/**
 * qoriq_ptp_auto_config - calculate a set of default configurations
 *
 * @qoriq_ptp: pointer to qoriq_ptp
 * @node: pointer to device_node
 *
 * If below dts properties are not provided, this function will be
 * called to calculate a set of default configurations for them.
 *   "fsl,tclk-period"
 *   "fsl,tmr-prsc"
 *   "fsl,tmr-add"
 *   "fsl,tmr-fiper1"
 *   "fsl,tmr-fiper2"
 *   "fsl,max-adj"
 *
 * Return 0 if success
 */
static int qoriq_ptp_auto_config(struct qoriq_ptp *qoriq_ptp,
				 struct device_node *node)
{
	struct clk *clk;
	u64 freq_comp;
	u64 max_adj;
	u32 nominal_freq;
	u32 remainder = 0;
	u32 clk_src = 0;

	qoriq_ptp->cksel = DEFAULT_CKSEL;

	clk = of_clk_get(node, 0);
	if (!IS_ERR(clk)) {
		clk_src = clk_get_rate(clk);
		clk_put(clk);
	}

	if (clk_src <= 100000000UL) {
		pr_err("error reference clock value, or lower than 100MHz\n");
		return -EINVAL;
	}

	nominal_freq = qoriq_ptp_nominal_freq(clk_src);
	if (!nominal_freq)
		return -EINVAL;

	qoriq_ptp->tclk_period = 1000000000UL / nominal_freq;
	qoriq_ptp->tmr_prsc = DEFAULT_TMR_PRSC;

	/* Calculate initial frequency compensation value for TMR_ADD register.
	 * freq_comp = ceil(2^32 / freq_ratio)
	 * freq_ratio = reference_clock_freq / nominal_freq
	 */
	freq_comp = ((u64)1 << 32) * nominal_freq;
	freq_comp = div_u64_rem(freq_comp, clk_src, &remainder);
	if (remainder)
		freq_comp++;

	qoriq_ptp->tmr_add = freq_comp;
	qoriq_ptp->tmr_fiper1 = DEFAULT_FIPER1_PERIOD - qoriq_ptp->tclk_period;
	qoriq_ptp->tmr_fiper2 = DEFAULT_FIPER2_PERIOD - qoriq_ptp->tclk_period;

	/* max_adj = 1000000000 * (freq_ratio - 1.0) - 1
	 * freq_ratio = reference_clock_freq / nominal_freq
	 */
	max_adj = 1000000000ULL * (clk_src - nominal_freq);
	max_adj = div_u64(max_adj, nominal_freq) - 1;
	qoriq_ptp->caps.max_adj = max_adj;

	return 0;
}

static int qoriq_ptp_probe(struct platform_device *dev)
{
	struct device_node *node = dev->dev.of_node;
	struct qoriq_ptp *qoriq_ptp;
	struct qoriq_ptp_registers *regs;
	struct timespec64 now;
	int err = -ENOMEM;
	u32 tmr_ctrl;
	unsigned long flags;
	void __iomem *base;

	qoriq_ptp = kzalloc(sizeof(*qoriq_ptp), GFP_KERNEL);
	if (!qoriq_ptp)
		goto no_memory;

	err = -EINVAL;

	qoriq_ptp->caps = ptp_qoriq_caps;

	if (of_property_read_u32(node, "fsl,cksel", &qoriq_ptp->cksel))
		qoriq_ptp->cksel = DEFAULT_CKSEL;

	if (of_property_read_u32(node,
				 "fsl,tclk-period", &qoriq_ptp->tclk_period) ||
	    of_property_read_u32(node,
				 "fsl,tmr-prsc", &qoriq_ptp->tmr_prsc) ||
	    of_property_read_u32(node,
				 "fsl,tmr-add", &qoriq_ptp->tmr_add) ||
	    of_property_read_u32(node,
				 "fsl,tmr-fiper1", &qoriq_ptp->tmr_fiper1) ||
	    of_property_read_u32(node,
				 "fsl,tmr-fiper2", &qoriq_ptp->tmr_fiper2) ||
	    of_property_read_u32(node,
				 "fsl,max-adj", &qoriq_ptp->caps.max_adj)) {
		pr_warn("device tree node missing required elements, try automatic configuration\n");

		if (qoriq_ptp_auto_config(qoriq_ptp, node))
			goto no_config;
	}

	err = -ENODEV;

	qoriq_ptp->irq = platform_get_irq(dev, 0);

	if (qoriq_ptp->irq < 0) {
		pr_err("irq not in device tree\n");
		goto no_node;
	}
	if (request_irq(qoriq_ptp->irq, isr, IRQF_SHARED, DRIVER, qoriq_ptp)) {
		pr_err("request_irq failed\n");
		goto no_node;
	}

	qoriq_ptp->rsrc = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!qoriq_ptp->rsrc) {
		pr_err("no resource\n");
		goto no_resource;
	}
	if (request_resource(&iomem_resource, qoriq_ptp->rsrc)) {
		pr_err("resource busy\n");
		goto no_resource;
	}

	spin_lock_init(&qoriq_ptp->lock);

	base = ioremap(qoriq_ptp->rsrc->start,
		       resource_size(qoriq_ptp->rsrc));
	if (!base) {
		pr_err("ioremap ptp registers failed\n");
		goto no_ioremap;
	}

	qoriq_ptp->base = base;

	if (of_device_is_compatible(node, "fsl,fman-ptp-timer")) {
		qoriq_ptp->regs.ctrl_regs = base + FMAN_CTRL_REGS_OFFSET;
		qoriq_ptp->regs.alarm_regs = base + FMAN_ALARM_REGS_OFFSET;
		qoriq_ptp->regs.fiper_regs = base + FMAN_FIPER_REGS_OFFSET;
		qoriq_ptp->regs.etts_regs = base + FMAN_ETTS_REGS_OFFSET;
	} else {
		qoriq_ptp->regs.ctrl_regs = base + CTRL_REGS_OFFSET;
		qoriq_ptp->regs.alarm_regs = base + ALARM_REGS_OFFSET;
		qoriq_ptp->regs.fiper_regs = base + FIPER_REGS_OFFSET;
		qoriq_ptp->regs.etts_regs = base + ETTS_REGS_OFFSET;
	}

	ktime_get_real_ts64(&now);
	ptp_qoriq_settime(&qoriq_ptp->caps, &now);

	tmr_ctrl =
	  (qoriq_ptp->tclk_period & TCLK_PERIOD_MASK) << TCLK_PERIOD_SHIFT |
	  (qoriq_ptp->cksel & CKSEL_MASK) << CKSEL_SHIFT;

	spin_lock_irqsave(&qoriq_ptp->lock, flags);

	regs = &qoriq_ptp->regs;
	qoriq_write(&regs->ctrl_regs->tmr_ctrl,   tmr_ctrl);
	qoriq_write(&regs->ctrl_regs->tmr_add,    qoriq_ptp->tmr_add);
	qoriq_write(&regs->ctrl_regs->tmr_prsc,   qoriq_ptp->tmr_prsc);
	qoriq_write(&regs->fiper_regs->tmr_fiper1, qoriq_ptp->tmr_fiper1);
	qoriq_write(&regs->fiper_regs->tmr_fiper2, qoriq_ptp->tmr_fiper2);
	set_alarm(qoriq_ptp);
	qoriq_write(&regs->ctrl_regs->tmr_ctrl,   tmr_ctrl|FIPERST|RTPE|TE|FRD);

	spin_unlock_irqrestore(&qoriq_ptp->lock, flags);

	qoriq_ptp->clock = ptp_clock_register(&qoriq_ptp->caps, &dev->dev);
	if (IS_ERR(qoriq_ptp->clock)) {
		err = PTR_ERR(qoriq_ptp->clock);
		goto no_clock;
	}
	qoriq_ptp->phc_index = ptp_clock_index(qoriq_ptp->clock);

	platform_set_drvdata(dev, qoriq_ptp);

	return 0;

no_clock:
	iounmap(qoriq_ptp->base);
no_ioremap:
	release_resource(qoriq_ptp->rsrc);
no_resource:
	free_irq(qoriq_ptp->irq, qoriq_ptp);
no_config:
no_node:
	kfree(qoriq_ptp);
no_memory:
	return err;
}

static int qoriq_ptp_remove(struct platform_device *dev)
{
	struct qoriq_ptp *qoriq_ptp = platform_get_drvdata(dev);
	struct qoriq_ptp_registers *regs = &qoriq_ptp->regs;

	qoriq_write(&regs->ctrl_regs->tmr_temask, 0);
	qoriq_write(&regs->ctrl_regs->tmr_ctrl,   0);

	ptp_clock_unregister(qoriq_ptp->clock);
	iounmap(qoriq_ptp->base);
	release_resource(qoriq_ptp->rsrc);
	free_irq(qoriq_ptp->irq, qoriq_ptp);
	kfree(qoriq_ptp);

	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "fsl,etsec-ptp" },
	{ .compatible = "fsl,fman-ptp-timer" },
	{},
};
MODULE_DEVICE_TABLE(of, match_table);

static struct platform_driver qoriq_ptp_driver = {
	.driver = {
		.name		= "ptp_qoriq",
		.of_match_table	= match_table,
	},
	.probe       = qoriq_ptp_probe,
	.remove      = qoriq_ptp_remove,
};

module_platform_driver(qoriq_ptp_driver);

MODULE_AUTHOR("Richard Cochran <richardcochran@gmail.com>");
MODULE_DESCRIPTION("PTP clock for Freescale QorIQ 1588 timer");
MODULE_LICENSE("GPL");
