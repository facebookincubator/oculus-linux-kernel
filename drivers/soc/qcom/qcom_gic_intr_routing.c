// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "qcom-gic-router: %s: " fmt, __func__

#include <linux/bits.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/irqnr.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include <linux/irqchip/arm-gic-v3.h>
#include <trace/hooks/gic_v3.h>

#include "../../../kernel/irq/internals.h"

#include <linux/tracepoint.h>
#include <trace/events/irq.h>

#define NUM_CLASS_CPUS	NR_CPUS
#define QCOM_GIC_INTERRUPT_ROUTING_MODE	BIT(31)
#define QCOM_GICD_ICLAR2	0xE008
#define QCOM_GICD_SETCLASSR	0x28
#define QCOM_GICD_TYPER_1_OF_N	BIT(25)
#define QCOM_MAX_IRQS	1020U
#define QCOM_GIC_V3_NAME	"GICv3"

struct qcom_gic_intr_routing_data {
	struct irq_chip *gic_chip;
	cpumask_t gic_routing_class0_cpus;
	cpumask_t gic_routing_class1_cpus;
	cpumask_t class0_active_cpus;
	enum cpuhp_state gic_affinity_cpuhp_state;
	bool gic_is_virtual;
	bool gic_supports_1_of_N;
	bool gic_1_of_N_init_done;
	atomic_t abort_balancing;
	atomic_t affinity_initialized;
};

static struct qcom_gic_intr_routing_data qcom_gic_routing_data;

static DEFINE_SPINLOCK(qcom_gic_class_lock);
static DEFINE_SPINLOCK(qcom_gic_init_lock);

static DECLARE_BITMAP(qcom_active_gic_class0, QCOM_MAX_IRQS);
static DECLARE_BITMAP(qcom_active_gic_class1, QCOM_MAX_IRQS);
static DECLARE_BITMAP(qcom_gic_class_initialized, QCOM_MAX_IRQS);
static DECLARE_BITMAP(qcom_gic_saved_class0, QCOM_MAX_IRQS);

static void qcom_affinity_initialize_workfn(struct work_struct *work);
static DECLARE_DELAYED_WORK(qcom_affinity_initialize_work, qcom_affinity_initialize_workfn);

struct qcom_gic_quirk {
	const char *desc;
	bool (*init)(void __iomem *base);
	u32 iidr;
	u32 mask;
};

static bool qcom_gicd_typer_1_of_N_supported(void __iomem *base)
{
	return !(readl_relaxed(base + GICD_TYPER) & QCOM_GICD_TYPER_1_OF_N);
}

static bool qcom_gic_enable_virtual_1_of_N(void __iomem *base)
{
	qcom_gic_routing_data.gic_is_virtual = true;
	qcom_gic_routing_data.gic_supports_1_of_N =
		qcom_gicd_typer_1_of_N_supported(base);
	return true;
}

static bool qcom_gic_enable_1_of_N(void __iomem *base)
{
	qcom_gic_routing_data.gic_supports_1_of_N =
		qcom_gicd_typer_1_of_N_supported(base);
	return true;
}

static const struct qcom_gic_quirk qcom_gic_quirks[] = {
	{
		.desc   = "Virtual GIC",
		.init   = qcom_gic_enable_virtual_1_of_N,
		.iidr   = 0x47000070,
		.mask   = 0xff000fff,
	},
	{
		/* GIC 600 */
		.desc   = "Physical GIC",
		.iidr   = 0x0200043b,
		.mask   = 0xff000fff,
		.init   = qcom_gic_enable_1_of_N,
	},
	{
		/* GIC 700 */
		.desc   = "Physical GIC",
		.iidr   = 0x0400043b,
		.mask   = 0xff000fff,
		.init   = qcom_gic_enable_1_of_N,
	},
	{
	}
};

void qcom_gic_enable_quirks(u32 iidr, const struct qcom_gic_quirk *quirks,
			    void __iomem *data)
{
	for (; quirks->desc; quirks++) {
		if (quirks->iidr != (quirks->mask & iidr))
			continue;

		if (quirks->init(data))
			pr_info("QGIC: enabling affinity routing for %s\n",
				quirks->desc);
	}
}


/*
 * Check whether class update is needed.
 * Hypervisor sets the initial class for each irq to class 0.
 * So, for virtual GIC, for the first update of class, for
 * a SPI, skip class update, if irq affinity maps to class 0.
 * For next class update, check the current class setting, and
 * skip class update, if it hasn't changed.
 */
static bool __maybe_unused qcom_gic_need_class_update(u32 irq, bool is_class0, bool is_class1)
{
	pr_debug("initialized: %d is_class0: %d test-class0: %d is_class1: %d test-class1: %d\n",
		test_bit(irq, qcom_gic_class_initialized), is_class0,
		test_bit(irq, qcom_active_gic_class0),
		is_class1, test_bit(irq, qcom_active_gic_class1));
	if (qcom_gic_routing_data.gic_is_virtual &&
	    !test_bit(irq, qcom_gic_class_initialized) &&
	    is_class0 && !is_class1)
		return false;

	if (test_bit(irq, qcom_gic_class_initialized) &&
	    (is_class0 == !!test_bit(irq, qcom_active_gic_class0)) &&
	    (is_class1 == !!test_bit(irq, qcom_active_gic_class1))) {
		return false;
	}

	return true;
}

void __maybe_unused qcom_gic_do_class_update_virtual(
		void __iomem *base, u32 hwirq,
		bool is_class0, bool is_class1)
{
	void __iomem *reg = base + QCOM_GICD_SETCLASSR;
	int val = hwirq & GENMASK(12, 0);

	if (is_class0)
		val |= BIT(30);
	if (is_class1)
		val |= BIT(31);
	pr_debug("Set class of hwirq: %d class: %#x\n", hwirq, val);
	writel_relaxed(val, reg);
}

void __maybe_unused qcom_gic_do_class_update_physical(
		void __iomem *base, u32 irq,
		bool is_class0, bool is_class1)
{
	void __iomem *reg = base + QCOM_GICD_ICLAR2 + (irq / 16) * 4;
	int val, offset, class_bits_val = 0;

	if (is_class0)
		class_bits_val = 0x2;
	if (is_class1)
		class_bits_val |= 0x1;

	spin_lock(&qcom_gic_class_lock);
	val = readl_relaxed(reg);
	offset = (irq % 16) << 2;
	val &= ~(0x3 << offset);
	val |= class_bits_val << offset;
	writel_relaxed(val, reg);
	spin_unlock(&qcom_gic_class_lock);
	pr_debug("Set class of hwirq: %d class: %#x\n", (irq + 32), val);
}

void __maybe_unused qcom_gic_do_class_update(
	void __iomem *base, u32 irq, bool is_class0,
	bool is_class1)
{
	if (qcom_gic_routing_data.gic_is_virtual)
		qcom_gic_do_class_update_virtual(base, irq + 32, is_class0,
						 is_class1);
	else
		qcom_gic_do_class_update_physical(base, irq, is_class0,
						  is_class1);
}

/** IRQ Balancing Design
 *
 * 1. At module load time, queue a work (qcom_affinity_initialize_work)
 *    to set InterruptRoutingMode for all SPIs.
 *
 *      1.1. In addition, set the class for all SPIs, based on the current
 *           affinity mask.
 *           For ex. assuming class 0 contains cpus 0-3, and class 1 4-7.
 *
 *           a. All of below affinity masks map to class 0
 *              0xf, 0x3, 0x7.
 *
 *           b. Similarly, below affinity masks maps to class 1:
 *              0xf0, 0x30, 0x70
 *
 *           c. Any combination of affinity mask containing cpus from both
 *              classes will map to both classes:
 *              0x11 , 0x31 , 0x13 , 0xff
 *
 *
 *      InterruptRoutingMode and class is not set in following scenarios:
 *
 *      a. Affinity mask contains single cpu
 *         Note: for an irq, where single cpu is online, out of the multi-cpu
 *         mask, affinity mask still contains original set affinity mask.
 *         So, class and IRM setting are retained for these irqs.
 *
 *      b. Broken, affinity, when all cpus in the affinity mask, goes offline.
 *         Class is retained in this case. However IRM setting is cleared.
 *
 * 2. Hotplug behavior
 *
 *    2.1.  When all cpus of class 0 goes offline; a snapshot of the irqs in
 *          class 0 is taken.
 *
 *    2.2   When first cpu of class 0 comes online. Affinity mask for all irqs
 *          in class 0, is set to all class 0 cpus.
 *
 *    Note: we do not spread back Gold (class 1) irqs, for the all gold cores
 *    hotplug case. This behavior matches what current irq balancers provide.
 *    In case of a need for this additional functionality, we can add that in
 *    future.
 *
 * 3. Unhandled corner cases:
 *
 *    3.1  Any irq with affinity mask containing subset of class 0 cpus are
 *         not spread, if only cpus of that affinity mask go offline and
 *         comes back online.
 *
 *    3.2  Any class0 irq, for which affinity is broken, and the new
 *         effective affinity CPU (CPU4 in our example) goes offline; such
 *         irqs won't be spread to class 0 cpus, once those CPUs come back
 *         online. This is not a problem for cases where due to some
 *         constraint, CPU4 is never hotplugged.
 */
static void qcom_trace_gic_v3_set_affinity(void *unused, struct irq_data *d,
					const struct cpumask *mask_val, u64 *affinity,
					bool force, void __iomem *base)
{
	const struct cpumask *cpu_affinity = mask_val;
	bool is_class0 = false, is_class1 = false;
	u32 irq = d->hwirq - 32;
	bool __maybe_unused need_class_update = false;
	const struct cpumask *current_affinity = irq_data_get_affinity_mask(d);
	int cpu = smp_processor_id();

	pr_debug("irq : %d mask: %*pb current affinity\n",
		d->hwirq, cpumask_pr_args(cpu_affinity),
		cpumask_pr_args(current_affinity));

	if (!qcom_gic_routing_data.gic_1_of_N_init_done) {
		spin_lock(&qcom_gic_init_lock);
		if (!qcom_gic_routing_data.gic_1_of_N_init_done) {
			qcom_gic_enable_quirks(readl_relaxed(base + GICD_IIDR),
				       qcom_gic_quirks, base);
			WRITE_ONCE(qcom_gic_routing_data.gic_chip, d->chip);
			/* Order readers of .gic_chip */
			smp_wmb();
			WRITE_ONCE(qcom_gic_routing_data.gic_1_of_N_init_done,
				   true);
		}
		spin_unlock(&qcom_gic_init_lock);
	}

	if (!qcom_gic_routing_data.gic_supports_1_of_N)
		return;

	if (cpumask_subset(current_affinity,
	    &qcom_gic_routing_data.gic_routing_class0_cpus)) {
		if (!cpumask_intersects(cpu_online_mask,
		    &qcom_gic_routing_data.gic_routing_class0_cpus) &&
		    !cpu_online(cpu) &&
		    cpumask_test_cpu(cpu,
		    &qcom_gic_routing_data.gic_routing_class0_cpus)) {
			pr_debug("Affinity broken class 0 irq: %d\n", d->hwirq);
			return;
		}
	}

	if (cpumask_subset(current_affinity,
	    &qcom_gic_routing_data.gic_routing_class1_cpus)) {
		if (!cpumask_intersects(cpu_online_mask,
		    &qcom_gic_routing_data.gic_routing_class1_cpus) &&
		    !cpu_online(cpu) &&
		    cpumask_test_cpu(cpu,
		    &qcom_gic_routing_data.gic_routing_class1_cpus)){
			pr_debug("Affinity broken class 1 irq: %d\n", d->hwirq);
			return;
		}
	}

	/* Do not set InterruptRouting for single CPU affinity mask */
	if (cpumask_weight(cpu_affinity) <= 1)
		goto clear_class;

	if (cpumask_any_and(cpu_affinity, cpu_online_mask) >= nr_cpu_ids)
		cpu_affinity = cpu_online_mask;

	if (cpumask_subset(cpu_affinity,
	    &qcom_gic_routing_data.gic_routing_class0_cpus)) {
		is_class0 = true;
	} else if (cpumask_subset(cpu_affinity,
	    &qcom_gic_routing_data.gic_routing_class1_cpus)) {
		is_class1 = true;
	} else {
		is_class1 = is_class0 = true;
	}

	if (!(is_class0 || is_class1))
		goto clear_class;

	*affinity |= QCOM_GIC_INTERRUPT_ROUTING_MODE;

	need_class_update = qcom_gic_need_class_update(irq, is_class0, is_class1);
	spin_lock(&qcom_gic_class_lock);
	set_bit(irq, qcom_gic_class_initialized);
	if (is_class0)
		set_bit(irq, qcom_active_gic_class0);
	else
		clear_bit(irq, qcom_active_gic_class0);
	if (is_class1)
		set_bit(irq, qcom_active_gic_class1);
	else
		clear_bit(irq, qcom_active_gic_class1);
	spin_unlock(&qcom_gic_class_lock);

	if (need_class_update)
		qcom_gic_do_class_update(base, irq, is_class0, is_class1);
	return;

clear_class:
	spin_lock(&qcom_gic_class_lock);
	clear_bit(irq, qcom_active_gic_class0);
	clear_bit(irq, qcom_active_gic_class1);
	spin_unlock(&qcom_gic_class_lock);
}

static bool qcom_is_gic_chip(struct irq_desc *desc, struct irq_chip *gic_chip)
{
	struct irq_data *data = irq_desc_get_irq_data(desc);
	struct irq_chip *chip = irq_data_get_irq_chip(data);

	if (!chip)
		return false;

	if (gic_chip)
		return (gic_chip == chip);
	else
		return !strcmp(chip->name, QCOM_GIC_V3_NAME);
}

static bool qcom_need_affinity_setting(struct irq_desc *desc,
					struct irq_chip *gic_chip,
					bool check_saved_class)
{
	bool need_affinity;
	struct irq_data *data = irq_desc_get_irq_data(desc);
	u32 irq = data->hwirq - 32;

	if (irq < 0 || irq >= QCOM_MAX_IRQS)
		return false;

	need_affinity = qcom_is_gic_chip(desc, gic_chip);

	if (!need_affinity)
		return false;
	if (check_saved_class &&
		!bitmap_empty(qcom_gic_saved_class0, QCOM_MAX_IRQS)) {
		spin_lock(&qcom_gic_class_lock);
		need_affinity = test_bit(irq, qcom_active_gic_class0) &&
				!test_bit(irq, qcom_active_gic_class1);
		spin_unlock(&qcom_gic_class_lock);
	}
	return need_affinity;
}

static void qcom_affinity_initialize_workfn(struct work_struct *work)
{
	struct irq_chip *gic_chip = NULL;
	struct irq_desc *desc;
	unsigned long flags;
	int i, err;
	bool need_affinity_setting = false;
	cpumask_t affinity = { CPU_BITS_NONE };
	struct irq_data *d;

	if (READ_ONCE(qcom_gic_routing_data.gic_1_of_N_init_done)) {
		/* Order .gic_1_of_N_init_done and .gic_chip read */
		smp_rmb();
		gic_chip = READ_ONCE(qcom_gic_routing_data.gic_chip);
	}

	for (i = 1; i < nr_irqs; i++) {
		if (atomic_add_return(0,
		    &qcom_gic_routing_data.abort_balancing))
			return;
		/* .abort_balancing read before affinity setting */
		smp_mb__after_atomic();
		local_irq_save(flags);
		rcu_read_lock();
		desc = irq_to_desc(i);
		if (!desc)
			goto out_rcu_lock;
		raw_spin_lock(&desc->lock);
		d = irq_desc_get_irq_data(desc);
		need_affinity_setting = qcom_need_affinity_setting(
			desc, gic_chip, true);
		if (!bitmap_empty(qcom_gic_saved_class0, QCOM_MAX_IRQS))
			cpumask_copy(&affinity,
				&qcom_gic_routing_data.gic_routing_class0_cpus);
		else
			cpumask_copy(&affinity, desc->irq_common_data.affinity);
		if (need_affinity_setting) {
			if (cpumask_any_and(&affinity, cpu_online_mask) >=
			    nr_cpu_ids)
				cpumask_copy(&affinity, cpu_online_mask);
			err = irq_do_set_affinity(d, &affinity, false);
			if (err)
				pr_warn_ratelimited(
					"IRQ%u: affinity initialize failed(%d).\n",
					d->irq, err);
		}
		raw_spin_unlock(&desc->lock);
out_rcu_lock:
		rcu_read_unlock();
		local_irq_restore(flags);
	}
	/* All affinity settings completion before .affinity_initialized = 1 */
	smp_mb__before_atomic();
	atomic_set(&qcom_gic_routing_data.affinity_initialized, 1);
}

static int qcom_gic_affinity_cpu_online(unsigned int cpu)
{
	if (cpumask_test_cpu(cpu,
	    &qcom_gic_routing_data.gic_routing_class0_cpus)) {
		if (cpumask_empty(&qcom_gic_routing_data.class0_active_cpus))
			/*
			 * Use a sane delay (matches existing irq balancers
			 * delay)
			 */
			schedule_delayed_work(&qcom_affinity_initialize_work,
				msecs_to_jiffies(5000));
		cpumask_set_cpu(cpu,
			&qcom_gic_routing_data.class0_active_cpus);
	}
	return 0;
}

static int qcom_gic_affinity_cpu_offline(unsigned int cpu)
{
	unsigned long flags;

	if (!cpumask_test_cpu(cpu,
	    &qcom_gic_routing_data.gic_routing_class0_cpus))
		return 0;

	cpumask_clear_cpu(cpu, &qcom_gic_routing_data.class0_active_cpus);
	if (cpumask_empty(&qcom_gic_routing_data.class0_active_cpus)) {
		/* Use RmW op to get the current value */
		if (!atomic_add_return(0,
		    &qcom_gic_routing_data.affinity_initialized)) {
			flush_delayed_work(&qcom_affinity_initialize_work);
			/*
			 * Flush initial work before qcom_gic_saved_class0
			 * update below.
			 */
			smp_mb();
		} else {
			atomic_set(&qcom_gic_routing_data.abort_balancing, 1);
			/* .abort_balancing write before cancel work */
			smp_mb__after_atomic();
			cancel_delayed_work_sync(&qcom_affinity_initialize_work);
			/* .abort_balancing write after cancel work */
			smp_mb__before_atomic();
			atomic_set(&qcom_gic_routing_data.abort_balancing, 0);
		}

		spin_lock_irqsave(&qcom_gic_class_lock, flags);
		bitmap_andnot(qcom_gic_saved_class0,
			      qcom_active_gic_class0,
			      qcom_active_gic_class1, QCOM_MAX_IRQS);
		spin_unlock_irqrestore(&qcom_gic_class_lock, flags);
	}
	return 0;
}

void qcom_gic_irq_handler_entry_notifer(void *ignore, int irq,
					struct irqaction *action)
{
	struct irq_chip *gic_chip = NULL;
	struct irq_desc *desc;
	int cpu;
	struct irq_data *data;
	u32 hwirq, hwirq_bitpos;
	struct cpumask *effective_affinity;

	if (!action->thread_fn)
		return;

	desc = irq_to_desc(irq);
	if (!desc)
		return;

	data = irq_desc_get_irq_data(desc);
	hwirq = data->hwirq;

	if (hwirq < 32 || hwirq >= QCOM_MAX_IRQS)
		return;

	if (READ_ONCE(qcom_gic_routing_data.gic_1_of_N_init_done)) {
		/* Order .gic_1_of_N_init_done and .gic_chip read */
		smp_rmb();
		gic_chip = READ_ONCE(qcom_gic_routing_data.gic_chip);
	}
	if (!qcom_is_gic_chip(desc, gic_chip))
		return;

	hwirq_bitpos = hwirq - 32;
	if (!test_bit(hwirq_bitpos, qcom_active_gic_class0) &&
	    !test_bit(hwirq_bitpos, qcom_active_gic_class1))
		return;

	if (raw_spin_trylock(&desc->lock)) {
		effective_affinity =
			irq_data_get_effective_affinity_mask(data);
		cpu = raw_smp_processor_id();
		if (!cpumask_equal(effective_affinity, cpumask_of(cpu))) {
			irq_data_update_effective_affinity(data,
				cpumask_of(cpu));
			pr_debug("Update effective affinity %d cpu :%d irq: %d\n",
				 hwirq, cpu, irq);
			set_bit(IRQTF_AFFINITY, &action->thread_flags);
		}
		raw_spin_unlock(&desc->lock);
	}
}

static int qcom_gic_intr_routing_probe(struct platform_device *pdev)
{

	int i, cpus_len;
	int rc = 0;
	u32 class0_cpus[NUM_CLASS_CPUS] = {0};
	u32 class1_cpus[NUM_CLASS_CPUS] = {0};

	cpus_len = of_property_read_variable_u32_array(
					pdev->dev.of_node,
					"qcom,gic-class0-cpus",
					class0_cpus, 0, NUM_CLASS_CPUS);
	for (i = 0; i < cpus_len; i++)
		if (class0_cpus[i] < num_possible_cpus())
			cpumask_set_cpu(class0_cpus[i],
			&qcom_gic_routing_data.gic_routing_class0_cpus);

	cpus_len = of_property_read_variable_u32_array(
					pdev->dev.of_node,
					"qcom,gic-class1-cpus",
					class1_cpus, 0, NUM_CLASS_CPUS);
	for (i = 0; i < cpus_len; i++)
		if (class1_cpus[i] < num_possible_cpus())
			cpumask_set_cpu(class1_cpus[i],
			&qcom_gic_routing_data.gic_routing_class1_cpus);

	register_trace_android_rvh_gic_v3_set_affinity(
		qcom_trace_gic_v3_set_affinity, NULL);

	register_trace_irq_handler_entry(qcom_gic_irq_handler_entry_notifer, NULL);
	rc = cpuhp_setup_state(
		CPUHP_AP_ONLINE_DYN, "qcom/gic_affinity_setting:online",
		qcom_gic_affinity_cpu_online, qcom_gic_affinity_cpu_offline);
	if (rc < 0)
		pr_err(
		  "Failed to register CPUHP state: qcom/gic_affinity_setting:online\n");

	qcom_gic_routing_data.gic_affinity_cpuhp_state = rc;
	pr_info("qcom GIC interrupt routing driver registered\n");
	return 0;
}

static int qcom_gic_intr_routing_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id qcom_gic_intr_routing_of_match[] = {
	{ .compatible = "qcom,gic-intr-routing"},
	{}
};
MODULE_DEVICE_TABLE(of, qcom_gic_intr_routing_of_match);

static struct platform_driver qcom_gic_intr_routing_driver = {
	.probe = qcom_gic_intr_routing_probe,
	.remove = qcom_gic_intr_routing_remove,
	.driver = {
		.name = "qcom_gic_intr_routing",
		.of_match_table = qcom_gic_intr_routing_of_match,
	},
};

static int __init qcom_gic_intr_routing_init(void)
{
	int ret;

	ret = platform_driver_register(&qcom_gic_intr_routing_driver);
	if (ret)
		pr_err("%s: qcom_gic_intr_routing register failed %d\n",
			__func__, ret);
	return ret;
}
module_init(qcom_gic_intr_routing_init);
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. GIC Interrupt Routing Driver");
MODULE_LICENSE("GPL v2");
