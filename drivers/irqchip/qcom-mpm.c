// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/tick.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include<linux/ktime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/of_irq.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/cpu_pm.h>
#include <asm/arch_timer.h>
#include <soc/qcom/rpm-notifier.h>
#include <soc/qcom/lpm_levels.h>
#include <soc/qcom/mpm.h>
#define CREATE_TRACE_POINTS
#include "trace/events/mpm.h"

#define ARCH_TIMER_HZ (19200000)
#define MAX_MPM_PIN_PER_IRQ 2
#define CLEAR_INTR(reg, intr) (reg & ~(1 << intr))
#define ENABLE_INTR(reg, intr) (reg | (1 << intr))
#define CLEAR_TYPE(reg, type) (reg & ~(1 << type))
#define ENABLE_TYPE(reg, type) (reg | (1 << type))
#define MPM_REG_ENABLE 0
#define MPM_REG_FALLING_EDGE 1
#define MPM_REG_RISING_EDGE 2
#define MPM_REG_POLARITY 3
#define MPM_REG_STATUS 4
#define MPM_GPIO 0
#define MPM_GIC 1

#define QCOM_MPM_REG_WIDTH  DIV_ROUND_UP(num_mpm_irqs, 32)
#define MPM_REGISTER(reg, index) ((reg * QCOM_MPM_REG_WIDTH + index + 2) * (4))
#define GPIO_NO_WAKE_IRQ	~0U
#define MPM_NO_PARENT_IRQ	~0U

struct msm_mpm_device_data {
	struct device *dev;
	void __iomem *mpm_request_reg_base;
	void __iomem *mpm_ipc_reg;
	irq_hw_number_t ipc_irq;
	struct irq_domain *gic_chip_domain;
	struct irq_domain *gpio_chip_domain;
};

static int num_mpm_irqs = 64;
static struct msm_mpm_device_data msm_mpm_dev_data;
static unsigned int *mpm_to_irq;
static DEFINE_SPINLOCK(mpm_lock);

static irq_hw_number_t get_parent_hwirq(struct irq_domain *d,
						irq_hw_number_t hwirq)
{
	struct mpm_pin *mpm_data = NULL;
	int i = 0;

	if (!d)
		return GPIO_NO_WAKE_IRQ;

	if (d->host_data) {
		mpm_data = d->host_data;
		for (i = 0; (mpm_data[i].pin >= 0); i++) {
			if (mpm_data[i].pin == hwirq)
				return mpm_data[i].hwirq;
		}
	}
	return GPIO_NO_WAKE_IRQ;
}

static void msm_get_mpm_pin(struct irq_data *d, int *mpm_pin, bool is_mpmgic)
{
	struct mpm_pin *mpm_data = NULL;
	int i = 0, j = 0;
	irq_hw_number_t hwirq;

	if (!d || !d->domain)
		return;

	if (is_mpmgic && d->domain->host_data) {
		mpm_data = d->domain->host_data;
		hwirq = get_parent_hwirq(d->domain, d->hwirq);
		if (hwirq == GPIO_NO_WAKE_IRQ)
			return;

		for (i = 0; (mpm_data[i].pin >= 0) &&
				(j < MAX_MPM_PIN_PER_IRQ); i++) {
			if (mpm_data[i].hwirq == hwirq) {
				mpm_pin[j] = mpm_data[i].pin;
				mpm_to_irq[mpm_data[i].pin] = d->irq;
				j++;
			}
		}
	} else if (!is_mpmgic) {
		mpm_pin[j] = d->hwirq;
		mpm_to_irq[d->hwirq] = d->irq;
	}
}

static inline uint32_t msm_mpm_read(unsigned int reg, unsigned int subreg_index)
{
	unsigned int offset = MPM_REGISTER(reg, subreg_index);

	return readl_relaxed(msm_mpm_dev_data.mpm_request_reg_base + offset);
}

static inline void msm_mpm_write(unsigned int reg,
					unsigned int subreg_index,
					uint32_t value)
{
	void __iomem *mpm_reg_base = msm_mpm_dev_data.mpm_request_reg_base;
	/*
	 * Add 2 to offset to account for the 64 bit timer in the vMPM
	 * mapping
	 */
	unsigned int offset = MPM_REGISTER(reg, subreg_index);
	u32 r_value;

	writel_relaxed(value, mpm_reg_base + offset);

	do {
		r_value = readl_relaxed(mpm_reg_base + offset);
		udelay(5);
	} while (r_value != value);
}

static inline void msm_mpm_enable_irq(struct irq_data *d, bool on,
							bool is_mpmgic)
{
	int mpm_pin[MAX_MPM_PIN_PER_IRQ] = {-1, -1};
	unsigned long flags;
	int i = 0;
	u32 enable;
	unsigned int index, mask;
	unsigned int reg;

	reg = MPM_REG_ENABLE;
	msm_get_mpm_pin(d, mpm_pin, is_mpmgic);
	for (i = 0; i < MAX_MPM_PIN_PER_IRQ; i++) {
		if (mpm_pin[i] < 0)
			return;

		index = mpm_pin[i]/32;
		mask = mpm_pin[i]%32;
		spin_lock_irqsave(&mpm_lock, flags);
		enable = msm_mpm_read(reg, index);

		if (on)
			enable = ENABLE_INTR(enable, mask);
		else
			enable = CLEAR_INTR(enable, mask);

		msm_mpm_write(reg, index, enable);
		spin_unlock_irqrestore(&mpm_lock, flags);
	}
}

static void msm_mpm_program_set_type(bool set, unsigned int reg,
					unsigned int index, unsigned int mask)
{
	u32 type;

	type = msm_mpm_read(reg, index);
	if (set)
		type = ENABLE_TYPE(type, mask);
	else
		type = CLEAR_TYPE(type, mask);

	msm_mpm_write(reg, index, type);
}

static void msm_mpm_set_type(struct irq_data *d,
					unsigned int flowtype, bool is_mpmgic)
{
	int mpm_pin[MAX_MPM_PIN_PER_IRQ] = {-1, -1};
	unsigned long flags;
	int i = 0;
	unsigned int index, mask;
	unsigned int reg = 0;

	msm_get_mpm_pin(d, mpm_pin, is_mpmgic);
	for (i = 0; i < MAX_MPM_PIN_PER_IRQ; i++) {
		if (mpm_pin[i] < 0)
			return;

		index = mpm_pin[i]/32;
		mask = mpm_pin[i]%32;

		spin_lock_irqsave(&mpm_lock, flags);
		reg = MPM_REG_RISING_EDGE;
		if (flowtype & IRQ_TYPE_EDGE_RISING)
			msm_mpm_program_set_type(1, reg, index, mask);
		else
			msm_mpm_program_set_type(0, reg, index, mask);

		reg = MPM_REG_FALLING_EDGE;
		if (flowtype & IRQ_TYPE_EDGE_FALLING)
			msm_mpm_program_set_type(1, reg, index, mask);
		else
			msm_mpm_program_set_type(0, reg, index, mask);

		reg = MPM_REG_POLARITY;
		if (flowtype & IRQ_TYPE_LEVEL_HIGH)
			msm_mpm_program_set_type(1, reg, index, mask);
		else
			msm_mpm_program_set_type(0, reg, index, mask);
		spin_unlock_irqrestore(&mpm_lock, flags);
	}
}

static void msm_mpm_gpio_chip_mask(struct irq_data *d)
{
	if (d->hwirq == GPIO_NO_WAKE_IRQ)
		return;

	msm_mpm_enable_irq(d, false, MPM_GPIO);
}

static void msm_mpm_gpio_chip_unmask(struct irq_data *d)
{
	if (d->hwirq == GPIO_NO_WAKE_IRQ)
		return;

	msm_mpm_enable_irq(d, true, MPM_GPIO);
}

static int msm_mpm_gpio_chip_set_type(struct irq_data *d, unsigned int type)
{
	if (d->hwirq == GPIO_NO_WAKE_IRQ)
		return 0;

	msm_mpm_set_type(d, type, MPM_GPIO);

	return 0;
}

static void msm_mpm_gic_chip_mask(struct irq_data *d)
{
	msm_mpm_enable_irq(d, false, MPM_GIC);
	irq_chip_mask_parent(d);
}

static void msm_mpm_gic_chip_unmask(struct irq_data *d)
{
	msm_mpm_enable_irq(d, true, MPM_GIC);
	irq_chip_unmask_parent(d);
}

static int msm_mpm_gic_chip_set_type(struct irq_data *d, unsigned int type)
{
	msm_mpm_set_type(d, type, MPM_GIC);
	return irq_chip_set_type_parent(d, type);
}

int msm_mpm_gic_chip_set_affinity(struct irq_data *data,
				 const struct cpumask *dest, bool force)
{
	data = data->parent_data;
	if (data->chip->irq_set_affinity)
		return data->chip->irq_set_affinity(data, dest, force);

	return -ENXIO;
}

void msm_mpm_gic_chip_eoi(struct irq_data *data)
{
	data = data->parent_data;
	data->chip->irq_eoi(data);
}

static struct irq_chip msm_mpm_gic_chip = {
	.name		= "mpm-gic",
	.irq_eoi	= msm_mpm_gic_chip_eoi,
	.irq_mask	= msm_mpm_gic_chip_mask,
	.irq_disable	= msm_mpm_gic_chip_mask,
	.irq_unmask	= msm_mpm_gic_chip_unmask,
	.irq_set_type	= msm_mpm_gic_chip_set_type,
	.flags		= IRQCHIP_MASK_ON_SUSPEND | IRQCHIP_SKIP_SET_WAKE,
	.irq_set_affinity	= msm_mpm_gic_chip_set_affinity,
};

static struct irq_chip msm_mpm_gpio_chip = {
	.name		= "mpm-gpio",
	.irq_mask	= msm_mpm_gpio_chip_mask,
	.irq_disable	= msm_mpm_gpio_chip_mask,
	.irq_unmask	= msm_mpm_gpio_chip_unmask,
	.irq_set_type	= msm_mpm_gpio_chip_set_type,
	.flags		= IRQCHIP_MASK_ON_SUSPEND | IRQCHIP_SKIP_SET_WAKE,
};

static int msm_mpm_gpio_chip_translate(struct irq_domain *d,
		struct irq_fwspec *fwspec,
		unsigned long *hwirq,
		unsigned int *type)
{
	if (is_of_node(fwspec->fwnode)) {
		if (fwspec->param_count != 2)
			return -EINVAL;
		*hwirq = fwspec->param[0];
		*type = fwspec->param[1];
		return 0;
	}
	return -EINVAL;
}

static int msm_mpm_gpio_chip_alloc(struct irq_domain *domain,
		unsigned int virq,
		unsigned int nr_irqs,
		void *data)
{
	int ret = 0;
	struct irq_fwspec *fwspec = data;
	irq_hw_number_t hwirq;
	unsigned int type = IRQ_TYPE_NONE;

	ret = msm_mpm_gpio_chip_translate(domain, fwspec, &hwirq, &type);
	if (ret)
		return ret;

	irq_domain_set_hwirq_and_chip(domain, virq, hwirq,
				&msm_mpm_gpio_chip, NULL);

	return 0;
}
static int msm_mpm_gpio_chip_match(struct irq_domain *d,
				struct device_node *node,
				enum irq_domain_bus_token bus_token)
{
	return (bus_token == DOMAIN_BUS_WAKEUP);
}

static int msm_mpm_gpio_chip_select(struct irq_domain *d,
				struct irq_fwspec  *node,
				enum irq_domain_bus_token bus_token)
{
	return (bus_token == DOMAIN_BUS_WAKEUP);
}

static const struct irq_domain_ops msm_mpm_gpio_chip_domain_ops = {
	.translate	= msm_mpm_gpio_chip_translate,
	.alloc		= msm_mpm_gpio_chip_alloc,
	.free		= irq_domain_free_irqs_common,
	.match		= msm_mpm_gpio_chip_match,
	.select		= msm_mpm_gpio_chip_select,
};

static int msm_mpm_gic_chip_translate(struct irq_domain *d,
		struct irq_fwspec *fwspec,
		unsigned long *hwirq,
		unsigned int *type)
{
	if (is_of_node(fwspec->fwnode)) {
		if (fwspec->param_count != 2)
			return -EINVAL;
		*hwirq = fwspec->param[0];
		*type = fwspec->param[1];
		return 0;
	}
	return -EINVAL;
}

static int msm_mpm_gic_chip_alloc(struct irq_domain *domain,
					unsigned int virq,
					unsigned int nr_irqs,
					void *data)
{
	struct irq_fwspec *fwspec = data;
	struct irq_fwspec parent_fwspec;
	irq_hw_number_t hwirq, parent_hwirq;
	unsigned int type;
	int  ret;

	ret = msm_mpm_gic_chip_translate(domain, fwspec, &hwirq, &type);
	if (ret)
		return ret;

	irq_domain_set_hwirq_and_chip(domain, virq, hwirq,
						&msm_mpm_gic_chip, NULL);

	parent_hwirq = get_parent_hwirq(domain, hwirq);
	if (parent_hwirq == MPM_NO_PARENT_IRQ)
		return 0;

	parent_fwspec.fwnode      = domain->parent->fwnode;
	parent_fwspec.param_count = 3;
	parent_fwspec.param[0]    = 0;
	parent_fwspec.param[1]    = parent_hwirq;
	parent_fwspec.param[2]    = type;

	return irq_domain_alloc_irqs_parent(domain, virq, nr_irqs,
					    &parent_fwspec);
}

static const struct irq_domain_ops msm_mpm_gic_chip_domain_ops = {
	.translate	= msm_mpm_gic_chip_translate,
	.alloc		= msm_mpm_gic_chip_alloc,
	.free		= irq_domain_free_irqs_common,
};

static inline void msm_mpm_send_interrupt(void)
{
	writel_relaxed(2, msm_mpm_dev_data.mpm_ipc_reg);
	/* Ensure the write is complete before returning. */
	wmb();
}

void msm_mpm_timer_write(uint32_t *expiry)
{
	writel_relaxed(expiry[0], msm_mpm_dev_data.mpm_request_reg_base);
	writel_relaxed(expiry[1], msm_mpm_dev_data.mpm_request_reg_base + 0x4);
}

void msm_mpm_enter_sleep(struct cpumask *cpumask)
{
	int i = 0;
	struct irq_chip *irq_chip;
	struct irq_data *irq_data;


	for (i = 0; i < QCOM_MPM_REG_WIDTH; i++)
		msm_mpm_write(MPM_REG_STATUS, i, 0);

	msm_mpm_send_interrupt();


	irq_data = irq_get_irq_data(msm_mpm_dev_data.ipc_irq);
	if (!irq_data)
		return;

	irq_chip = irq_data->chip;

	if (!irq_data)
		return;

	if (cpumask)
		irq_chip->irq_set_affinity(irq_data, cpumask, true);
}

/*
 * Triggered by RPM when system resumes from deep sleep
 */
static irqreturn_t msm_mpm_irq(int irq, void *dev_id)
{
	unsigned long pending;
	uint32_t value[3];
	int i, k, apps_irq;
	unsigned int mpm_irq;
	struct irq_desc *desc = NULL;
	unsigned int reg = MPM_REG_ENABLE;

	for (i = 0; i < QCOM_MPM_REG_WIDTH; i++) {
		value[i] = msm_mpm_read(reg, i);
		trace_mpm_wakeup_enable_irqs(i, value[i]);
	}

	for (i = 0; i < QCOM_MPM_REG_WIDTH; i++) {
		pending = msm_mpm_read(MPM_REG_STATUS, i);
		pending &= (unsigned long)value[i];

		trace_mpm_wakeup_pending_irqs(i, pending);
		for_each_set_bit(k, &pending, 32) {
			mpm_irq = 32 * i + k;
			apps_irq = mpm_to_irq[mpm_irq];
			desc = apps_irq ?
				irq_to_desc(apps_irq) : NULL;

			if (desc && !irqd_is_level_type(&desc->irq_data))
				irq_set_irqchip_state(apps_irq,
						IRQCHIP_STATE_PENDING, true);

		}

	}
	return IRQ_HANDLED;
}

static int msm_mpm_init(struct device_node *node)
{
	struct msm_mpm_device_data *dev = &msm_mpm_dev_data;
	int ret = 0;
	int irq, index;

	index = of_property_match_string(node, "reg-names", "vmpm");
	if (index < 0) {
		ret = -EADDRNOTAVAIL;
		goto reg_base_err;
	}

	dev->mpm_request_reg_base = of_iomap(node, index);
	if (!dev->mpm_request_reg_base) {
		pr_err("Unable to iomap\n");
		ret = -EADDRNOTAVAIL;
		goto reg_base_err;
	}

	index = of_property_match_string(node, "reg-names", "ipc");
	if (index < 0) {
		ret = -EADDRNOTAVAIL;
		goto reg_base_err;
	}

	dev->mpm_ipc_reg = of_iomap(node, index);
	if (!dev->mpm_ipc_reg) {
		pr_err("Unable to iomap IPC register\n");
		ret = -EADDRNOTAVAIL;
		goto ipc_reg_err;
	}

	irq = of_irq_get(node, 0);
	if (irq <= 0) {
		pr_err("no IRQ resource info\n");
		ret = irq;
		goto ipc_irq_err;
	}
	dev->ipc_irq = irq;

	ret = request_irq(dev->ipc_irq, msm_mpm_irq,
		IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, "mpm",
		msm_mpm_irq);
	if (ret) {
		pr_err("request_irq failed errno: %d\n", ret);
		goto ipc_irq_err;
	}

	ret = irq_set_irq_wake(dev->ipc_irq, 1);
	if (ret) {
		pr_err("failed to set wakeup irq %lu: %d\n",
			dev->ipc_irq, ret);
		goto set_wake_irq_err;
	}

	return 0;
set_wake_irq_err:
	free_irq(dev->ipc_irq, msm_mpm_irq);
ipc_irq_err:
	iounmap(dev->mpm_ipc_reg);
ipc_reg_err:
	iounmap(dev->mpm_request_reg_base);
reg_base_err:
	return ret;
}

static const struct of_device_id mpm_gic_chip_data_table[] = {
	{
		.compatible = "qcom,mpm-gic-holi",
		.data = mpm_holi_gic_chip_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mpm_gic_chip_data_table);

static int msm_mpm_irqchip_init(struct device_node *node,
						struct device_node *parent)
{
	struct irq_domain *parent_domain;
	const struct of_device_id *id;
	int ret;

	if (!parent) {
		pr_err("%s(): no parent for mpm-gic\n", node->full_name);
		return -ENXIO;
	}

	parent_domain = irq_find_host(parent);
	if (!parent_domain) {
		pr_err("unable to obtain gic parent domain\n");
		return -ENXIO;
	}

	of_property_read_u32(node, "qcom,num-mpm-irqs", &num_mpm_irqs);

	mpm_to_irq = kcalloc(num_mpm_irqs, sizeof(*mpm_to_irq), GFP_KERNEL);
	if (!mpm_to_irq)
		return -ENOMEM;

	id = of_match_node(mpm_gic_chip_data_table, node);
	if (!id) {
		pr_err("can not find mpm_gic_data_table of_node\n");
		ret = -ENODEV;
		goto mpm_map_err;
	}

	msm_mpm_dev_data.gic_chip_domain = irq_domain_add_hierarchy(
			parent_domain, 0, num_mpm_irqs, node,
			&msm_mpm_gic_chip_domain_ops, (void *)id->data);
	if (!msm_mpm_dev_data.gic_chip_domain) {
		pr_err("gic domain add failed\n");
		ret = -ENOMEM;
		goto mpm_map_err;
	}

	msm_mpm_dev_data.gic_chip_domain->name = "qcom,mpm-gic";
	msm_mpm_dev_data.gpio_chip_domain = irq_domain_create_linear(
			of_node_to_fwnode(node), num_mpm_irqs,
			&msm_mpm_gpio_chip_domain_ops, NULL);

	if (!msm_mpm_dev_data.gpio_chip_domain)
		return -ENOMEM;

	msm_mpm_dev_data.gpio_chip_domain->name = "qcom,mpm-gpio";

	ret = msm_mpm_init(node);
	if (!ret)
		return ret;
	irq_domain_remove(msm_mpm_dev_data.gic_chip_domain);

mpm_map_err:
	kfree(mpm_to_irq);
	return ret;
}

#ifdef MODULE
static int mpm_driver_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node, *parent;

	parent = of_irq_find_parent(node);
	if (!parent) {
		pr_err("%s(): no parent for mpm-gic\n", node->full_name);
		return -ENXIO;
	}

	return msm_mpm_irqchip_init(node, parent);

}
static const struct of_device_id mpm_of_match[] = {
	{ .compatible = "qcom,mpm" },
	{},
};

struct platform_driver mpm_driver = {
	.probe = mpm_driver_probe,
	.driver  = {
		.name   = "qcom-mpm",
		.of_match_table	= mpm_of_match,
	},
};

module_platform_driver(mpm_driver);
#else
IRQCHIP_DECLARE(mpm_gpio_chip, "qcom,mpm", msm_mpm_irqchip_init);
#endif
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. (QTI) MPM Driver");
MODULE_LICENSE("GPL v2");
