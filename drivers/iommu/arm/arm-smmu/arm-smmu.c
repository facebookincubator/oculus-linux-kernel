// SPDX-License-Identifier: GPL-2.0-only
/*
 * IOMMU API for ARM architected SMMU implementations.
 *
 * Copyright (C) 2013 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 *
 * This driver currently supports:
 *	- SMMUv1 and v2 implementations
 *	- Stream-matching and stream-indexing
 *	- v7/v8 long-descriptor format
 *	- Non-secure access to the SMMU
 *	- Context fault reporting
 *	- Extended Stream ID (16 bit)
 *
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "arm-smmu: " fmt

#include <linux/acpi.h>
#include <linux/acpi_iort.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping-fast.h>
#include <linux/dma-map-ops.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_iommu.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/of_platform.h>
#include <linux/irq.h>
#include <linux/wait.h>

#include <linux/amba/bus.h>
#include <linux/fsl/mc.h>

#include "arm-smmu.h"
#include "../../iommu-logger.h"
#include "../../qcom-dma-iommu-generic.h"
#include <linux/qcom-iommu-util.h>
#include <linux/qcom-io-pgtable.h>

#define CREATE_TRACE_POINTS
#include "arm-smmu-trace.h"

/*
 * Apparently, some Qualcomm arm64 platforms which appear to expose their SMMU
 * global register space are still, in fact, using a hypervisor to mediate it
 * by trapping and emulating register accesses. Sadly, some deployed versions
 * of said trapping code have bugs wherein they go horribly wrong for stores
 * using r31 (i.e. XZR/WZR) as the source register.
 */
#define QCOM_DUMMY_VAL -1

#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)

#define ARM_SMMU_IMPL_DEF1(smmu) \
	((smmu)->base + (6 * (1 << (smmu)->pgshift)))

#define MSI_IOVA_BASE			0x8000000
#define MSI_IOVA_LENGTH			0x100000

static int force_stage;
module_param(force_stage, int, S_IRUGO);
MODULE_PARM_DESC(force_stage,
	"Force SMMU mappings to be installed at a particular stage of translation. A value of '1' or '2' forces the corresponding stage. All other values are ignored (i.e. no stage is forced). Note that selecting a specific stage will disable support for nested translation.");
static bool disable_bypass =
	IS_ENABLED(CONFIG_ARM_SMMU_DISABLE_BYPASS_BY_DEFAULT);
module_param(disable_bypass, bool, S_IRUGO);
MODULE_PARM_DESC(disable_bypass,
	"Disable bypass streams such that incoming transactions from devices that are not attached to an iommu domain will report an abort back to the device and will not be allowed to pass through the SMMU.");

#define s2cr_init_val (struct arm_smmu_s2cr){				\
	.type = disable_bypass ? S2CR_TYPE_FAULT : S2CR_TYPE_BYPASS,	\
}

#define INVALID_CBNDX			0xff
#define INVALID_ASID			0xffff
/*
 * In V7L and V8L with TTBCR2.AS == 0, ASID is 8 bits.
 * V8L 16 with TTBCR2.AS == 1 (16 bit ASID) isn't supported yet.
 */
#define MAX_ASID			0xff

#define ARM_SMMU_CB_ASID(smmu, cfg)		((cfg)->asid)
#define ARM_SMMU_CB_VMID(smmu, cfg) ((u16)(smmu)->cavium_id_base + \
							(cfg)->cbndx + 1)

struct arm_smmu_pte_info {
	void *virt_addr;
	size_t size;
	struct list_head entry;
};

struct arm_smmu_secure_pool_chunk {
	void *addr;
	size_t size;
	struct list_head list;
};

static bool using_legacy_binding, using_generic_binding;

struct arm_smmu_option_prop {
	u32 opt;
	const char *prop;
};

static struct arm_smmu_option_prop arm_smmu_options[] = {
	{ ARM_SMMU_OPT_FATAL_ASF, "qcom,fatal-asf" },
	{ ARM_SMMU_OPT_3LVL_TABLES, "qcom,use-3-lvl-tables" },
	{ ARM_SMMU_OPT_NO_ASID_RETENTION, "qcom,no-asid-retention" },
	{ ARM_SMMU_OPT_DISABLE_ATOS, "qcom,disable-atos" },
	{ ARM_SMMU_OPT_CONTEXT_FAULT_RETRY, "qcom,context-fault-retry" },
	{ 0, NULL},
};

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova);
static phys_addr_t arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
				struct qcom_iommu_atos_txn *txn);
static void arm_smmu_destroy_domain_context(struct iommu_domain *domain);

static int arm_smmu_assign_table(struct arm_smmu_domain *smmu_domain);
static void arm_smmu_unassign_table(struct arm_smmu_domain *smmu_domain);

static int arm_smmu_enable_s1_translations(struct arm_smmu_domain *smmu_domain);

static int arm_smmu_setup_default_domain(struct device *dev,
				struct iommu_domain *domain);
static int __arm_smmu_domain_set_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data);
static int arm_smmu_domain_get_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data);
static void arm_smmu_free_pgtable(void *cookie, void *virt, int order, bool deferred_free);
static void __arm_smmu_flush_iotlb_all(struct iommu_domain *domain, bool force);

static inline int arm_smmu_rpm_get(struct arm_smmu_device *smmu)
{
	if (pm_runtime_enabled(smmu->dev))
		return pm_runtime_resume_and_get(smmu->dev);

	return 0;
}

static inline void arm_smmu_rpm_put(struct arm_smmu_device *smmu)
{
	if (pm_runtime_enabled(smmu->dev)) {
		pm_runtime_mark_last_busy(smmu->dev);
		pm_runtime_put_autosuspend(smmu->dev);
	}
}

static struct arm_smmu_domain *to_smmu_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct arm_smmu_domain, domain);
}

static void parse_driver_options(struct arm_smmu_device *smmu)
{
	int i = 0;

	do {
		if (of_property_read_bool(smmu->dev->of_node,
						arm_smmu_options[i].prop)) {
			smmu->options |= arm_smmu_options[i].opt;
			dev_dbg(smmu->dev, "option %s\n",
				arm_smmu_options[i].prop);
		}
	} while (arm_smmu_options[++i].opt);
}

static bool is_iommu_pt_coherent(struct arm_smmu_domain *smmu_domain)
{
	if (test_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
		     smmu_domain->attributes))
		return true;
	else if (smmu_domain->smmu && smmu_domain->smmu->dev)
		return dev_is_dma_coherent(smmu_domain->smmu->dev);
	else
		return false;
}

static bool arm_smmu_has_secure_vmid(struct arm_smmu_domain *smmu_domain)
{
	return (smmu_domain->secure_vmid != VMID_INVAL);
}

static void arm_smmu_secure_domain_lock(struct arm_smmu_domain *smmu_domain)
{
	if (arm_smmu_has_secure_vmid(smmu_domain))
		mutex_lock(&smmu_domain->assign_lock);
}

static void arm_smmu_secure_domain_unlock(struct arm_smmu_domain *smmu_domain)
{
	if (arm_smmu_has_secure_vmid(smmu_domain))
		mutex_unlock(&smmu_domain->assign_lock);
}

#ifdef CONFIG_ARM_SMMU_SELFTEST

static int selftest;
module_param_named(selftest, selftest, int, 0644);
static int irq_count;

struct arm_smmu_cf_selftest_data {
	struct arm_smmu_device *smmu;
	int cbndx;
};

static DECLARE_WAIT_QUEUE_HEAD(wait_int);
static irqreturn_t arm_smmu_cf_selftest(int irq, void *data)
{
	u32 fsr;
	struct irq_data *irq_data = irq_get_irq_data(irq);
	struct arm_smmu_cf_selftest_data *cb_data = data;
	struct arm_smmu_device *smmu = cb_data->smmu;
	int idx = cb_data->cbndx;
	unsigned long hwirq = ULONG_MAX;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);

	irq_count++;
	if (irq_data)
		hwirq = irq_data->hwirq;
	pr_info("Interrupt (irq:%d hwirq:%ld) received, fsr:0x%x\n",
				irq, hwirq, fsr);

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);

	wake_up(&wait_int);
	return IRQ_HANDLED;
}

static void arm_smmu_interrupt_selftest(struct arm_smmu_device *smmu)
{
	int cb;
	int cb_count = 0;
	struct arm_smmu_cf_selftest_data *cb_data;

	if (!selftest)
		return;

	cb = smmu->num_s2_context_banks;

	if (smmu->version < ARM_SMMU_V2)
		return;

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data)
		return;
	cb_data->smmu = smmu;

	for_each_clear_bit_from(cb, smmu->context_map,
				smmu->num_context_banks) {
		int irq;
		int ret;
		u32 reg;
		u32 reg_orig;
		int irq_cnt;

		irq = smmu->irqs[smmu->num_global_irqs + cb];
		cb_data->cbndx = cb;

		ret = devm_request_threaded_irq(smmu->dev, irq, NULL,
				arm_smmu_cf_selftest,
				IRQF_ONESHOT | IRQF_SHARED,
				"arm-smmu-context-fault", cb_data);
		if (ret < 0) {
			dev_err(smmu->dev,
				"Failed to request cntx IRQ %d (%u)\n",
				cb, irq);
			continue;
		}

		cb_count++;
		irq_cnt = irq_count;

		reg_orig = arm_smmu_cb_read(smmu, cb, ARM_SMMU_CB_SCTLR);
		reg = reg_orig | ARM_SMMU_SCTLR_CFIE | ARM_SMMU_SCTLR_CFRE;

		arm_smmu_cb_write(smmu, cb, ARM_SMMU_CB_SCTLR, reg);
		dev_info(smmu->dev, "Testing cntx %d irq %d\n", cb, irq);

		/* Make sure ARM_SMMU_CB_SCTLR is configured */
		wmb();
		arm_smmu_cb_write(smmu, cb, ARM_SMMU_CB_FSRRESTORE,
				ARM_SMMU_FSR_TF);

		if (!wait_event_timeout(wait_int, (irq_count > irq_cnt),
					msecs_to_jiffies(1000))) {
			u32 fsr;

			fsr = arm_smmu_cb_read(smmu, cb, ARM_SMMU_CB_FSR);
			dev_info(smmu->dev, "timeout cb:%d, irq:%d, fsr:0x%x\n",
				 cb, irq_cnt, fsr);

			if (!fsr)
				dev_err(smmu->dev, "SCTLR  = 0x%08x\n",
					arm_smmu_cb_read(smmu, cb,
							 ARM_SMMU_CB_SCTLR));
		}

		/* Make sure ARM_SMMU_CB_FSRRESTORE is written to */
		wmb();
		arm_smmu_cb_write(smmu, cb, ARM_SMMU_CB_SCTLR, reg_orig);
		devm_free_irq(smmu->dev, irq, cb_data);
	}

	kfree(cb_data);
	dev_info(smmu->dev,
			"Interrupt selftest completed...\n");
	dev_info(smmu->dev,
			"Tested %d contexts, received %d interrupts\n",
			cb_count, irq_count);
	WARN_ON(cb_count != irq_count);
	irq_count = 0;
}
#else
static void arm_smmu_interrupt_selftest(struct arm_smmu_device *smmu)
{
}
#endif

static struct platform_driver arm_smmu_driver;
static struct qcom_iommu_ops arm_smmu_ops;

#ifdef CONFIG_ARM_SMMU_LEGACY_DT_BINDINGS
static int arm_smmu_bus_init(struct iommu_ops *ops);

static struct device_node *dev_get_dev_node(struct device *dev)
{
	if (dev_is_pci(dev)) {
		struct pci_bus *bus = to_pci_dev(dev)->bus;

		while (!pci_is_root_bus(bus))
			bus = bus->parent;
		return of_node_get(bus->bridge->parent->of_node);
	}

	return of_node_get(dev->of_node);
}

static int __arm_smmu_get_pci_sid(struct pci_dev *pdev, u16 alias, void *data)
{
	*((__be32 *)data) = cpu_to_be32(alias);
	return 0; /* Continue walking */
}

static int __find_legacy_master_phandle(struct device *dev, void *data)
{
	struct of_phandle_iterator *it = *(void **)data;
	struct device_node *np = it->node;
	int err;

	of_for_each_phandle(it, err, dev->of_node, "mmu-masters",
			    "#stream-id-cells", -1)
		if (it->node == np) {
			*(void **)data = dev;
			return 1;
		}
	it->node = np;
	return err == -ENOENT ? 0 : err;
}

static int arm_smmu_register_legacy_master(struct device *dev,
					   struct arm_smmu_device **smmu)
{
	struct device *smmu_dev;
	struct device_node *np;
	struct of_phandle_iterator it;
	void *data = &it;
	u32 *sids;
	__be32 pci_sid;
	int err = 0;

	memset(&it, 0, sizeof(it));
	np = dev_get_dev_node(dev);
	if (!np || !of_find_property(np, "#stream-id-cells", NULL)) {
		of_node_put(np);
		return -ENODEV;
	}

	it.node = np;
	err = driver_for_each_device(&arm_smmu_driver.driver, NULL, &data,
				     __find_legacy_master_phandle);
	smmu_dev = data;
	of_node_put(np);
	if (err == 0)
		return -ENODEV;
	if (err < 0)
		return err;

	if (dev_is_pci(dev)) {
		/* "mmu-masters" assumes Stream ID == Requester ID */
		pci_for_each_dma_alias(to_pci_dev(dev), __arm_smmu_get_pci_sid,
				       &pci_sid);
		it.cur = &pci_sid;
		it.cur_count = 1;
	}

	err = iommu_fwspec_init(dev, &smmu_dev->of_node->fwnode,
				&arm_smmu_ops.iommu_ops);
	if (err)
		return err;

	sids = kcalloc(it.cur_count, sizeof(*sids), GFP_KERNEL);
	if (!sids)
		return -ENOMEM;

	*smmu = dev_get_drvdata(smmu_dev);
	of_phandle_iterator_args(&it, sids, it.cur_count);
	err = iommu_fwspec_add_ids(dev, sids, it.cur_count);
	kfree(sids);
	return err;
}

/*
 * With the legacy DT binding in play, we have no guarantees about
 * probe order, but then we're also not doing default domains, so we can
 * delay setting bus ops until we're sure every possible SMMU is ready,
 * and that way ensure that no probe_device() calls get missed.
 */
static int arm_smmu_legacy_bus_init(void)
{
	if (using_legacy_binding)
		return arm_smmu_bus_init(&arm_smmu_ops.iommu_ops);
	return 0;
}
device_initcall_sync(arm_smmu_legacy_bus_init);
#else
static int arm_smmu_register_legacy_master(struct device *dev,
					   struct arm_smmu_device **smmu)
{
	return -ENODEV;
}
#endif /* CONFIG_ARM_SMMU_LEGACY_DT_BINDINGS */

int __arm_smmu_alloc_cb(unsigned long *map, int start, int end,
			struct device *dev, struct arm_smmu_domain *smmu_domain)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_master_cfg *cfg = dev_iommu_priv_get(dev);
	struct arm_smmu_device *smmu = cfg->smmu;
	int idx;
	int i;

	for_each_cfg_sme(cfg, fwspec, i, idx) {
		if (smmu->s2crs[idx].pinned)
			return smmu->s2crs[idx].cbndx;
	}

	return __arm_smmu_alloc_bitmap(map, start, end);
}

static void __arm_smmu_free_bitmap(unsigned long *map, int idx)
{
	clear_bit(idx, map);
}

/* Wait for any pending TLB invalidations to complete */
static int __arm_smmu_tlb_sync(struct arm_smmu_device *smmu, int page,
				int sync, int status)
{
	unsigned int inc, delay;
	u32 reg;

	/*
	 * Allowing an unbounded number of sync requests to be submitted when a
	 * TBU is not processing sync requests can cause a TBU's command queue
	 * to fill up. Once the queue is full, subsequent sync requests can
	 * stall the CPU indefinitely. Avoid this by gating subsequent sync
	 * requests after the first sync timeout on an SMMU.
	 */
	if (IS_ENABLED(CONFIG_IOMMU_TLBSYNC_DEBUG) &&
	    test_bit(0, &smmu->sync_timed_out))
		return -EINVAL;

	arm_smmu_writel(smmu, page, sync, QCOM_DUMMY_VAL);
	for (delay = 1, inc = 1; delay < TLB_LOOP_TIMEOUT; delay += inc) {
		reg = arm_smmu_readl(smmu, page, status);
		if (!(reg & ARM_SMMU_sTLBGSTATUS_GSACTIVE))
			return 0;

		cpu_relax();
		udelay(inc);
		if (inc < TLB_LOOP_INC_MAX)
			inc *= 2;
	}

	if (IS_ENABLED(CONFIG_IOMMU_TLBSYNC_DEBUG) &&
	    test_and_set_bit_lock(0, &smmu->sync_timed_out))
		goto out;

	trace_tlbsync_timeout(smmu->dev);
	if (smmu->impl && smmu->impl->tlb_sync_timeout)
		smmu->impl->tlb_sync_timeout(smmu);
out:
	return -EINVAL;
}

static void arm_smmu_tlb_sync_global(struct arm_smmu_device *smmu)
{
	unsigned long flags;

	spin_lock_irqsave(&smmu->global_sync_lock, flags);
	if (__arm_smmu_tlb_sync(smmu, ARM_SMMU_GR0, ARM_SMMU_GR0_sTLBGSYNC,
				ARM_SMMU_GR0_sTLBGSTATUS))
		dev_err_ratelimited(smmu->dev,
				    "TLB global sync failed!\n");
	spin_unlock_irqrestore(&smmu->global_sync_lock, flags);
}

static void arm_smmu_tlb_sync_context(struct arm_smmu_domain *smmu_domain)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = smmu_domain->cfg.cbndx;
	unsigned long flags;

	spin_lock_irqsave(&smmu_domain->sync_lock, flags);
	if (__arm_smmu_tlb_sync(smmu, ARM_SMMU_CB(smmu, idx),
				ARM_SMMU_CB_TLBSYNC, ARM_SMMU_CB_TLBSTATUS))
		dev_err_ratelimited(smmu->dev,
				"TLB sync on cb%d failed for device %s\n",
				smmu_domain->cfg.cbndx,
				dev_name(smmu_domain->dev));
	spin_unlock_irqrestore(&smmu_domain->sync_lock, flags);
}

static void arm_smmu_tlb_inv_context_s1(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	/*
	 * The TLBI write may be relaxed, so ensure that PTEs cleared by the
	 * current CPU are visible beforehand.
	 */
	wmb();
	trace_tlbi_start(smmu_domain);
	arm_smmu_cb_write(smmu_domain->smmu, smmu_domain->cfg.cbndx,
			  ARM_SMMU_CB_S1_TLBIASID, smmu_domain->cfg.asid);
	arm_smmu_tlb_sync_context(smmu_domain);
	trace_tlbi_end(smmu_domain);
}

static void arm_smmu_tlb_inv_context_s2(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	/* See above */
	wmb();
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIVMID, smmu_domain->cfg.vmid);
	arm_smmu_tlb_sync_global(smmu);
}

static void arm_smmu_tlb_inv_range_s1(unsigned long iova, size_t size,
				      size_t granule, void *cookie, int reg)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	int idx = cfg->cbndx;

	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	if (cfg->fmt != ARM_SMMU_CTX_FMT_AARCH64) {
		iova = (iova >> 12) << 12;
		iova |= cfg->asid;
		do {
			arm_smmu_cb_write(smmu, idx, reg, iova);
			iova += granule;
		} while (size -= granule);
	} else {
		iova >>= 12;
		iova |= (u64)cfg->asid << 48;
		do {
			arm_smmu_cb_writeq(smmu, idx, reg, iova);
			iova += granule >> 12;
		} while (size -= granule);
	}
}

static void arm_smmu_tlb_inv_range_s2(unsigned long iova, size_t size,
				      size_t granule, void *cookie, int reg)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = smmu_domain->cfg.cbndx;

	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	iova >>= 12;
	do {
		if (smmu_domain->cfg.fmt == ARM_SMMU_CTX_FMT_AARCH64)
			arm_smmu_cb_writeq(smmu, idx, reg, iova);
		else
			arm_smmu_cb_write(smmu, idx, reg, iova);
		iova += granule >> 12;
	} while (size -= granule);
}

static void arm_smmu_tlb_inv_walk_s1(unsigned long iova, size_t size,
				     size_t granule, void *cookie)
{
	arm_smmu_tlb_inv_range_s1(iova, size, granule, cookie,
				  ARM_SMMU_CB_S1_TLBIVA);
	arm_smmu_tlb_sync_context(cookie);
}

static void arm_smmu_tlb_add_page_s1(struct iommu_iotlb_gather *gather,
				     unsigned long iova, size_t granule,
				     void *cookie)
{
	arm_smmu_tlb_inv_range_s1(iova, granule, granule, cookie,
				  ARM_SMMU_CB_S1_TLBIVAL);
}

static void arm_smmu_tlb_inv_walk_s2(unsigned long iova, size_t size,
				     size_t granule, void *cookie)
{
	arm_smmu_tlb_inv_range_s2(iova, size, granule, cookie,
				  ARM_SMMU_CB_S2_TLBIIPAS2);
	arm_smmu_tlb_sync_context(cookie);
}

static void arm_smmu_tlb_add_page_s2(struct iommu_iotlb_gather *gather,
				     unsigned long iova, size_t granule,
				     void *cookie)
{
	arm_smmu_tlb_inv_range_s2(iova, granule, granule, cookie,
				  ARM_SMMU_CB_S2_TLBIIPAS2L);
}

static void arm_smmu_tlb_inv_walk_s2_v1(unsigned long iova, size_t size,
					size_t granule, void *cookie)
{
	arm_smmu_tlb_inv_context_s2(cookie);
}
/*
 * On MMU-401 at least, the cost of firing off multiple TLBIVMIDs appears
 * almost negligible, but the benefit of getting the first one in as far ahead
 * of the sync as possible is significant, hence we don't just make this a
 * no-op and call arm_smmu_tlb_inv_context_s2() from .iotlb_sync as you might
 * think.
 */
static void arm_smmu_tlb_add_page_s2_v1(struct iommu_iotlb_gather *gather,
					unsigned long iova, size_t granule,
					void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIVMID, smmu_domain->cfg.vmid);
}

static const struct iommu_flush_ops arm_smmu_s1_tlb_ops = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s1,
	.tlb_flush_walk	= arm_smmu_tlb_inv_walk_s1,
	.tlb_add_page	= arm_smmu_tlb_add_page_s1,
};

static const struct iommu_flush_ops arm_smmu_s2_tlb_ops_v2 = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s2,
	.tlb_flush_walk	= arm_smmu_tlb_inv_walk_s2,
	.tlb_add_page	= arm_smmu_tlb_add_page_s2,
};

static const struct iommu_flush_ops arm_smmu_s2_tlb_ops_v1 = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s2,
	.tlb_flush_walk	= arm_smmu_tlb_inv_walk_s2_v1,
	.tlb_add_page	= arm_smmu_tlb_add_page_s2_v1,
};

static void print_fault_regs(struct arm_smmu_domain *smmu_domain,
		struct arm_smmu_device *smmu, int idx)
{
	u32 fsr, fsynr0, fsynr1, cbfrsynra;
	unsigned long iova;
	struct arm_smmu_cfg *cfg = smmu->cbs[idx].cfg;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	fsynr0 = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR0);
	fsynr1 = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR1);
	iova = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_FAR);
	cbfrsynra = arm_smmu_gr1_read(smmu, ARM_SMMU_GR1_CBFRSYNRA(idx));

	dev_err(smmu->dev, "Unhandled arm-smmu context fault from %s!\n",
		dev_name(smmu_domain->dev));
	dev_err(smmu->dev, "FAR    = 0x%016lx\n",
		iova);
	dev_err(smmu->dev, "PAR    = 0x%pK\n",
		(void *) arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_PAR));

	dev_err(smmu->dev,
		"FSR    = 0x%08x [%s%s%s%s%s%s%s%s%s%s]\n",
		fsr,
		(fsr & ARM_SMMU_FSR_TF) ?  (fsynr0 & ARM_SMMU_FSYNR0_WNR ?
				 "TF W " : "TF R ") : "",
		(fsr & ARM_SMMU_FSR_AFF) ? "AFF " : "",
		(fsr & ARM_SMMU_FSR_PF) ? (fsynr0 & ARM_SMMU_FSYNR0_WNR ?
				"PF W " : "PF R ") : "",
		(fsr & ARM_SMMU_FSR_EF) ? "EF " : "",
		(fsr & ARM_SMMU_FSR_TLBMCF) ? "TLBMCF " : "",
		(fsr & ARM_SMMU_FSR_TLBLKF) ? "TLBLKF " : "",
		(fsr & ARM_SMMU_FSR_ASF) ? "ASF " : "",
		(fsr & ARM_SMMU_FSR_UUT) ? "UUT " : "",
		(fsr & ARM_SMMU_FSR_SS) ? "SS " : "",
		(fsr & ARM_SMMU_FSR_MULTI) ? "MULTI " : "");

	dev_err(smmu->dev, "FSYNR0    = 0x%x\n", fsynr0);
	dev_err(smmu->dev, "FSYNR1    = 0x%x\n", fsynr1);
	dev_err(smmu->dev, "context bank#    = 0x%x\n", idx);

	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
		dev_err(smmu->dev, "TTBR0  = 0x%pK\n",
			(void *) (unsigned long)
			arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_TTBR0));
		dev_err(smmu->dev, "TTBR1  = 0x%pK\n",
			(void *) (unsigned long)
			arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_TTBR1));
	} else {
		dev_err(smmu->dev, "TTBR0  = 0x%pK\n",
			(void *) arm_smmu_cb_readq(smmu, idx,
						   ARM_SMMU_CB_TTBR0));
		if (stage1)
			dev_err(smmu->dev, "TTBR1  = 0x%pK\n",
				(void *) arm_smmu_cb_readq(smmu, idx,
							   ARM_SMMU_CB_TTBR1));
	}

	dev_err(smmu->dev, "SCTLR  = 0x%08x ACTLR  = 0x%08x\n",
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_SCTLR),
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_ACTLR));
	dev_err(smmu->dev, "CBAR  = 0x%08x\n",
		arm_smmu_gr1_read(smmu, ARM_SMMU_GR1_CBAR(idx)));
	dev_err(smmu->dev, "MAIR0   = 0x%08x MAIR1   = 0x%08x\n",
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_S1_MAIR0),
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_S1_MAIR1));

	dev_err(smmu->dev, "SID = 0x%x\n",
		cbfrsynra & CBFRSYNRA_SID_MASK);
	dev_err(smmu->dev, "Client info: BID=0x%lx, PID=0x%lx, MID=0x%lx\n",
		FIELD_GET(ARM_SMMU_FSYNR1_BID, fsynr1),
		FIELD_GET(ARM_SMMU_FSYNR1_PID, fsynr1),
		FIELD_GET(ARM_SMMU_FSYNR1_MID, fsynr1));
}

/*
 * Iommu HW has generated a fault. If HW and SW states are in sync,
 * then a SW page table walk should yield 0.
 *
 * WARNING!!! This check is racy!!!!
 * For a faulting address x, the dma layer mapping may map address x
 * into the iommu page table in parallel to the fault handler running.
 * This is frequently seen due to dma-iommu's address reuse policy.
 * Thus, arm_smmu_iova_to_phys() returning nonzero is not necessarily
 * indicative of an issue.
 */
static void arm_smmu_verify_fault(struct arm_smmu_domain *smmu_domain,
	struct arm_smmu_device *smmu, int idx)
{
	u32 fsynr, cbfrsynra;
	unsigned long iova;
	struct iommu_domain *domain = &smmu_domain->domain;
	phys_addr_t phys_soft;
	phys_addr_t phys_stimu, phys_hard_priv, phys_stimu_post_tlbiall;
	unsigned long flags = 0;
	struct qcom_iommu_atos_txn txn = {0};

	fsynr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR0);
	iova = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_FAR);
	cbfrsynra = arm_smmu_gr1_read(smmu, ARM_SMMU_GR1_CBFRSYNRA(idx));

	phys_soft = arm_smmu_iova_to_phys(domain, iova);
	dev_err(smmu->dev, "soft iova-to-phys=%pa\n", &phys_soft);

	/* Get the transaction type */
	if (fsynr & ARM_SMMU_FSYNR0_WNR)
		flags |= IOMMU_TRANS_WRITE;
	if (fsynr & ARM_SMMU_FSYNR0_PNU)
		flags |= IOMMU_TRANS_PRIV;
	if (fsynr & ARM_SMMU_FSYNR0_IND)
		flags |= IOMMU_TRANS_INST;

	txn.addr = iova;
	txn.flags = flags;
	txn.id = cbfrsynra & CBFRSYNRA_SID_MASK;

	/* Now replicate the faulty transaction */
	phys_stimu = arm_smmu_iova_to_phys_hard(domain, &txn);

	/*
	 * If the replicated transaction fails, it could be due to legitimate
	 * unmapped access (translation fault) or stale TLB with insufficient
	 * privileges (permission fault). Try ATOS operation with full access
	 * privileges to rule out stale entry with insufficient privileges case.
	 */
	if (!phys_stimu) {
		txn.flags = QCOM_IOMMU_ATOS_TRANS_DEFAULT |
				QCOM_IOMMU_ATOS_TRANS_PRIV;
		phys_hard_priv = arm_smmu_iova_to_phys_hard(domain, &txn);
	}

	/* Now replicate the faulty transaction post tlbiall */
	iommu_flush_iotlb_all(domain);
	txn.flags = flags;
	phys_stimu_post_tlbiall = arm_smmu_iova_to_phys_hard(domain, &txn);

	if (!phys_stimu && phys_hard_priv) {
		dev_err(smmu->dev,
			"ATOS results differed across access privileges...\n"
			"Before: %pa After: %pa\n",
			&phys_stimu, &phys_hard_priv);
	}

	if (phys_stimu != phys_stimu_post_tlbiall) {
		dev_err(smmu->dev,
			"ATOS results differed across TLBIALL...\n"
			"Before: %pa After: %pa\n", &phys_stimu,
						&phys_stimu_post_tlbiall);
	}

	dev_err(smmu->dev, "hard iova-to-phys (ATOS)=%pa\n",
		phys_stimu ? &phys_stimu : &phys_stimu_post_tlbiall);
}

static int report_iommu_fault_helper(struct arm_smmu_domain *smmu_domain,
	struct arm_smmu_device *smmu, int idx)
{
	u32 fsr, fsynr;
	unsigned long iova;
	int flags;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	fsynr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR0);
	iova = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_FAR);

	flags = fsynr & ARM_SMMU_FSYNR0_WNR ?
		IOMMU_FAULT_WRITE : IOMMU_FAULT_READ;
	if (fsr & ARM_SMMU_FSR_TF)
		flags |= IOMMU_FAULT_TRANSLATION;
	if (fsr & ARM_SMMU_FSR_PF)
		flags |= IOMMU_FAULT_PERMISSION;
	if (fsr & ARM_SMMU_FSR_EF)
		flags |= IOMMU_FAULT_EXTERNAL;
	if (fsr & ARM_SMMU_FSR_SS)
		flags |= IOMMU_FAULT_TRANSACTION_STALLED;


	return report_iommu_fault(&smmu_domain->domain,
				 smmu->dev, iova, flags);
}

static int arm_smmu_get_fault_ids(struct iommu_domain *domain,
			struct qcom_iommu_fault_ids *f_ids)
{
	struct arm_smmu_domain *smmu_domain;
	struct arm_smmu_device *smmu;
	u32 fsr, fsynr1;
	int idx, ret;

	if (!domain || !f_ids)
		return -EINVAL;

	smmu_domain = to_smmu_domain(domain);
	smmu = smmu_domain->smmu;
	idx = smmu_domain->cfg.cbndx;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return ret;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);

	if (!(fsr & ARM_SMMU_FSR_FAULT)) {
		arm_smmu_rpm_put(smmu);
		return -EINVAL;
	}

	fsynr1 = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR1);
	arm_smmu_rpm_put(smmu);

	f_ids->bid = FIELD_GET(ARM_SMMU_FSYNR1_BID, fsynr1);
	f_ids->pid = FIELD_GET(ARM_SMMU_FSYNR1_PID, fsynr1);
	f_ids->mid = FIELD_GET(ARM_SMMU_FSYNR1_MID, fsynr1);

	return 0;
}

#ifdef CONFIG_ARM_SMMU_CONTEXT_FAULT_RETRY
/*
 * Retry faulting address after tlb invalidate.
 * Applicable to:  Waipio
 */
static irqreturn_t arm_smmu_context_fault_retry(struct arm_smmu_domain *smmu_domain)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = smmu_domain->cfg.cbndx;
	u64 iova;
	u32 fsr;

	if (!(smmu->options & ARM_SMMU_OPT_CONTEXT_FAULT_RETRY) ||
	    (test_bit(DOMAIN_ATTR_FAULT_MODEL_NO_STALL, smmu_domain->attributes)))
		return IRQ_NONE;

	iova = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_FAR);
	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);

	if (iova != smmu_domain->prev_fault_address ||
			!smmu_domain->fault_retry_counter) {
		smmu_domain->prev_fault_address = iova;
		smmu_domain->fault_retry_counter++;
		arm_smmu_tlb_inv_context_s1(smmu_domain);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);
		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation
		 */
		wmb();
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
					ARM_SMMU_RESUME_RESUME);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}
#else
static irqreturn_t arm_smmu_context_fault_retry(struct arm_smmu_domain *smmu_domain)
{
	return IRQ_NONE;
}
#endif

static irqreturn_t arm_smmu_context_fault(int irq, void *dev)
{
	u32 fsr;
	int ret;
	struct iommu_domain *domain = dev;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = smmu_domain->cfg.cbndx;
	static DEFINE_RATELIMIT_STATE(_rs,
				      DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return IRQ_NONE;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	if (!(fsr & ARM_SMMU_FSR_FAULT)) {
		ret = IRQ_NONE;
		goto out_power_off;
	}

	if ((smmu->options & ARM_SMMU_OPT_FATAL_ASF) &&
			(fsr & ARM_SMMU_FSR_ASF)) {
		dev_err(smmu->dev,
			"Took an address size fault.  Refusing to recover.\n");
		BUG();
	}

	ret = arm_smmu_context_fault_retry(smmu_domain);
	if (ret == IRQ_HANDLED)
		goto out_power_off;

	/*
	 * If the fault helper returns -ENOSYS, then no client fault helper was
	 * registered. In that case, print the default report.
	 *
	 * If the client returns -EBUSY, do not clear FSR and do not RESUME
	 * if stalled. This is required to keep the IOMMU client stalled on
	 * the outstanding fault. This gives the client a chance to take any
	 * debug action and then terminate the stalled transaction.
	 * So, the sequence in case of stall on fault should be:
	 * 1) Do not clear FSR or write to RESUME here
	 * 2) Client takes any debug action
	 * 3) Client terminates the stalled transaction and resumes the IOMMU
	 * 4) Client clears FSR. The FSR should only be cleared after 3) and
	 *    not before so that the fault remains outstanding. This ensures
	 *    SCTLR.HUPCF has the desired effect if subsequent transactions also
	 *    need to be terminated.
	 */
	ret = report_iommu_fault_helper(smmu_domain, smmu, idx);
	if (ret == -ENOSYS) {
		if (__ratelimit(&_rs)) {
			print_fault_regs(smmu_domain, smmu, idx);
			arm_smmu_verify_fault(smmu_domain, smmu, idx);
		}
		BUG_ON(!test_bit(DOMAIN_ATTR_NON_FATAL_FAULTS, smmu_domain->attributes));
	}
	if (ret != -EBUSY) {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);

		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation
		 */
		wmb();

		if (fsr & ARM_SMMU_FSR_SS)
			arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
				  ARM_SMMU_RESUME_TERMINATE);
	}

	ret = IRQ_HANDLED;
out_power_off:
	arm_smmu_rpm_put(smmu);
	return ret;
}

static irqreturn_t arm_smmu_global_fault(int irq, void *dev)
{
	u32 gfsr, gfsynr0, gfsynr1, gfsynr2;
	struct arm_smmu_device *smmu = dev;
	static DEFINE_RATELIMIT_STATE(rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);
	int ret;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return IRQ_NONE;

	gfsr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSR);
	gfsynr0 = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSYNR0);
	gfsynr1 = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSYNR1);
	gfsynr2 = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSYNR2);

	if (!gfsr) {
		arm_smmu_rpm_put(smmu);
		return IRQ_NONE;
	}

	if (__ratelimit(&rs)) {
		if (IS_ENABLED(CONFIG_ARM_SMMU_DISABLE_BYPASS_BY_DEFAULT) &&
		    (gfsr & ARM_SMMU_sGFSR_USF))
			dev_err(smmu->dev,
				"Blocked unknown Stream ID 0x%hx; boot with \"arm-smmu.disable_bypass=0\" to allow, but this may have security implications\n",
				(u16)gfsynr1);
		else
			dev_err(smmu->dev,
				"Unexpected global fault, this could be serious\n");
		dev_err(smmu->dev,
			"\tGFSR 0x%08x, GFSYNR0 0x%08x, GFSYNR1 0x%08x, GFSYNR2 0x%08x\n",
			gfsr, gfsynr0, gfsynr1, gfsynr2);
	}

	wmb();
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sGFSR, gfsr);
	arm_smmu_rpm_put(smmu);
	return IRQ_HANDLED;
}

static void arm_smmu_init_context_bank(struct arm_smmu_domain *smmu_domain,
				       struct io_pgtable_cfg *pgtbl_cfg)
{
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_cb *cb = &smmu_domain->smmu->cbs[cfg->cbndx];
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	unsigned long *attributes = smmu_domain->attributes;

	cb->cfg = cfg;

	/* TCR */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->tcr[0] = pgtbl_cfg->arm_v7s_cfg.tcr;
		} else {
			cb->tcr[0] = arm_smmu_lpae_tcr(pgtbl_cfg);
			cb->tcr[1] = arm_smmu_lpae_tcr2(pgtbl_cfg);
			if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
				cb->tcr[1] |= ARM_SMMU_TCR2_AS;
			else
				cb->tcr[0] |= ARM_SMMU_TCR_EAE;
		}
	} else {
		cb->tcr[0] = arm_smmu_lpae_vtcr(pgtbl_cfg);
	}

	/* TTBRs */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->ttbr[0] = pgtbl_cfg->arm_v7s_cfg.ttbr;
			cb->ttbr[1] = 0;
		} else {
			cb->ttbr[0] = FIELD_PREP(ARM_SMMU_TTBRn_ASID,
						 cfg->asid);
			cb->ttbr[1] = FIELD_PREP(ARM_SMMU_TTBRn_ASID,
						 cfg->asid);

			if (pgtbl_cfg->quirks & IO_PGTABLE_QUIRK_ARM_TTBR1)
				cb->ttbr[1] |= pgtbl_cfg->arm_lpae_s1_cfg.ttbr;
			else
				cb->ttbr[0] |= pgtbl_cfg->arm_lpae_s1_cfg.ttbr;
		}
	} else {
		cb->ttbr[0] = pgtbl_cfg->arm_lpae_s2_cfg.vttbr;
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->mair[0] = pgtbl_cfg->arm_v7s_cfg.prrr;
			cb->mair[1] = pgtbl_cfg->arm_v7s_cfg.nmrr;
		} else {
			cb->mair[0] = pgtbl_cfg->arm_lpae_s1_cfg.mair;
			cb->mair[1] = pgtbl_cfg->arm_lpae_s1_cfg.mair >> 32;
		}
	}

	memset(&cfg->sctlr, 0, sizeof(cfg->sctlr));
	/*
	 * Override cacheability, shareability, r/w allocation for
	 * clients who are io-coherent. Otherwise set NSH to force io-coherency
	 * to be disabled.
	 * These settings only take effect during bypass mode, when sctlr.M = 0
	 */
	if (of_dma_is_coherent(smmu_domain->dev->of_node)) {
		cfg->sctlr.wacfg = ARM_SMMU_SCTLR_WACFG_WA;
		cfg->sctlr.racfg = ARM_SMMU_SCTLR_RACFG_RA;
		cfg->sctlr.shcfg = ARM_SMMU_SCTLR_SHCFG_OSH;
		cfg->sctlr.mtcfg = 1;
		cfg->sctlr.memattr = ARM_SMMU_SCTLR_MEM_ATTR_OISH_WB_CACHE;
	} else {
		cfg->sctlr.shcfg = ARM_SMMU_SCTLR_SHCFG_NSH;
	}

	cfg->sctlr.cfre = !(test_bit(DOMAIN_ATTR_FAULT_MODEL_NO_CFRE, attributes));
	cfg->sctlr.cfcfg = !(test_bit(DOMAIN_ATTR_FAULT_MODEL_NO_STALL, attributes));
	cfg->sctlr.hupcf = test_bit(DOMAIN_ATTR_FAULT_MODEL_HUPCF, attributes);

	if ((!test_bit(DOMAIN_ATTR_S1_BYPASS, attributes) &&
	     !test_bit(DOMAIN_ATTR_EARLY_MAP, attributes)) || !stage1)
		cfg->sctlr.m = 1;

	cb->sctlr = arm_smmu_lpae_sctlr(cfg);
}

void arm_smmu_write_context_bank(struct arm_smmu_device *smmu, int idx)
{
	u32 reg;
	bool stage1;
	struct arm_smmu_cb *cb = &smmu->cbs[idx];
	struct arm_smmu_cfg *cfg = cb->cfg;

	/* Unassigned context banks only need disabling */
	if (!cfg) {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, 0);
		return;
	}

	stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	/* CBA2R */
	if (smmu->version > ARM_SMMU_V1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
			reg = ARM_SMMU_CBA2R_VA64;
		else
			reg = 0;
		/* 16-bit VMIDs live in CBA2R */
		if (smmu->features & ARM_SMMU_FEAT_VMID16)
			reg |= FIELD_PREP(ARM_SMMU_CBA2R_VMID16, cfg->vmid);

		arm_smmu_gr1_write(smmu, ARM_SMMU_GR1_CBA2R(idx), reg);
	}

	/* CBAR */
	reg = FIELD_PREP(ARM_SMMU_CBAR_TYPE, cfg->cbar);
	if (smmu->version < ARM_SMMU_V2)
		reg |= FIELD_PREP(ARM_SMMU_CBAR_IRPTNDX, cfg->irptndx);

	/*
	 * Use the weakest shareability/memory types, so they are
	 * overridden by the ttbcr/pte.
	 */
	if (stage1) {
		reg |= FIELD_PREP(ARM_SMMU_CBAR_S1_BPSHCFG,
				  ARM_SMMU_CBAR_S1_BPSHCFG_NSH) |
		       FIELD_PREP(ARM_SMMU_CBAR_S1_MEMATTR,
				  ARM_SMMU_CBAR_S1_MEMATTR_WB);
	} else if (!(smmu->features & ARM_SMMU_FEAT_VMID16)) {
		/* 8-bit VMIDs live in CBAR */
		reg |= FIELD_PREP(ARM_SMMU_CBAR_VMID, cfg->vmid);
	}
	arm_smmu_gr1_write(smmu, ARM_SMMU_GR1_CBAR(idx), reg);

	/*
	 * TCR
	 * We must write this before the TTBRs, since it determines the
	 * access behaviour of some fields (in particular, ASID[15:8]).
	 */
	if (stage1 && smmu->version > ARM_SMMU_V1)
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TCR2, cb->tcr[1]);
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TCR, cb->tcr[0]);

	/* TTBRs */
	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_CONTEXTIDR, cfg->asid);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TTBR0, cb->ttbr[0]);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TTBR1, cb->ttbr[1]);
	} else {
		arm_smmu_cb_writeq(smmu, idx, ARM_SMMU_CB_TTBR0, cb->ttbr[0]);
		if (stage1)
			arm_smmu_cb_writeq(smmu, idx, ARM_SMMU_CB_TTBR1,
					   cb->ttbr[1]);
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_S1_MAIR0, cb->mair[0]);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_S1_MAIR1, cb->mair[1]);
	}

	/* SCTLR */
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, cb->sctlr);
}

/* This function assumes that the domain's init mutex is held */
static int arm_smmu_get_dma_cookie(struct device *dev,
				    struct arm_smmu_domain *smmu_domain,
				    struct io_pgtable_ops *pgtbl_ops)
{
	bool is_fast = test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes);
	struct iommu_domain *domain = &smmu_domain->domain;
	int ret;

	if (domain->type == IOMMU_DOMAIN_DMA)
		return iommu_get_dma_cookie(domain);
	else if (is_fast) {
		ret = fast_smmu_init_mapping(dev, domain, pgtbl_ops);
		if (ret)
			return ret;
	}

	return 0;
}

static void arm_smmu_put_dma_cookie(struct iommu_domain *domain)
{
	int is_fast = 0;

	iommu_domain_get_attr(domain, DOMAIN_ATTR_FAST, &is_fast);
	if (domain->type == IOMMU_DOMAIN_DMA)
		iommu_put_dma_cookie(domain);
	else if (is_fast)
		fast_smmu_put_dma_cookie(domain);
}

static unsigned long arm_smmu_domain_get_qcom_quirks(
			struct arm_smmu_domain *smmu_domain,
			struct arm_smmu_device *smmu)
{
	/* These TCR register options are mutually exclusive */
	if (is_iommu_pt_coherent(smmu_domain))
		return 0;
	if (test_bit(DOMAIN_ATTR_USE_UPSTREAM_HINT, smmu_domain->attributes))
		return IO_PGTABLE_QUIRK_QCOM_USE_UPSTREAM_HINT;
	if (test_bit(DOMAIN_ATTR_USE_LLC_NWA, smmu_domain->attributes))
		return IO_PGTABLE_QUIRK_QCOM_USE_LLC_NWA;

	return 0;
}

static int arm_smmu_secure_pool_add(struct arm_smmu_domain *smmu_domain,
				     void *addr, size_t size)
{
	struct arm_smmu_secure_pool_chunk *chunk;

	chunk = kmalloc(sizeof(*chunk), GFP_ATOMIC);
	if (!chunk)
		return -ENOMEM;

	chunk->addr = addr;
	chunk->size = size;
	memset(addr, 0, size);
	list_add(&chunk->list, &smmu_domain->secure_pool_list);

	return 0;
}

static void arm_smmu_unprepare_pgtable(void *cookie, void *addr, size_t size)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_pte_info *pte_info;

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
		WARN(1, "Invalid VMID is set !!\n");
		return;
	}

	pte_info = kzalloc(sizeof(struct arm_smmu_pte_info), GFP_ATOMIC);
	if (!pte_info)
		return;

	pte_info->virt_addr = addr;
	pte_info->size = size;
	list_add_tail(&pte_info->entry, &smmu_domain->unassign_list);
}

static int arm_smmu_prepare_pgtable(void *addr, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_pte_info *pte_info;

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
		WARN(1, "Invalid VMID is set !!\n");
		return -EINVAL;
	}

	pte_info = kzalloc(sizeof(struct arm_smmu_pte_info), GFP_ATOMIC);
	if (!pte_info)
		return -ENOMEM;
	pte_info->virt_addr = addr;
	list_add_tail(&pte_info->entry, &smmu_domain->pte_info_list);
	return 0;
}

static void arm_smmu_secure_pool_destroy(struct arm_smmu_domain *smmu_domain)
{
	struct arm_smmu_secure_pool_chunk *it, *i;

	list_for_each_entry_safe(it, i, &smmu_domain->secure_pool_list, list) {
		arm_smmu_unprepare_pgtable(smmu_domain, it->addr, it->size);
		/* pages will be freed later (after being unassigned) */
		list_del(&it->list);
		kfree(it);
	}
}

static void *arm_smmu_secure_pool_remove(struct arm_smmu_domain *smmu_domain,
					size_t size)
{
	struct arm_smmu_secure_pool_chunk *it;

	list_for_each_entry(it, &smmu_domain->secure_pool_list, list) {
		if (it->size == size) {
			void *addr = it->addr;

			list_del(&it->list);
			kfree(it);
			return addr;
		}
	}

	return NULL;
}

static void *arm_smmu_alloc_pgtable(void *cookie, gfp_t gfp_mask, int order)
{
	int ret;
	struct page *page;
	void *page_addr;
	size_t size = (1UL << order) * PAGE_SIZE;
	struct arm_smmu_domain *smmu_domain = cookie;

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
		page = alloc_pages(gfp_mask, order);
		if (!page)
			return NULL;

		return page_address(page);
	}

	page_addr = arm_smmu_secure_pool_remove(smmu_domain, size);
	if (page_addr)
		return page_addr;

	page = alloc_pages(gfp_mask, order);
	if (!page)
		return NULL;

	page_addr = page_address(page);
	ret = arm_smmu_prepare_pgtable(page_addr, cookie);
	if (ret) {
		free_pages((unsigned long)page_addr, order);
		return NULL;
	}

	return page_addr;
}

static void __arm_smmu_free_pgtable(struct arm_smmu_domain *smmu_domain, void *virt, int order)
{
	size_t size = (1UL << order) * PAGE_SIZE;

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
		free_pages((unsigned long)virt, order);
		return;
	}

	if (arm_smmu_secure_pool_add(smmu_domain, virt, size))
		arm_smmu_unprepare_pgtable(smmu_domain, virt, size);
}

static void arm_smmu_free_pgtable(void *cookie, void *virt, int order,
				bool deferred_free)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	if (deferred_free) {
		unsigned long flags;
		struct page *page = virt_to_page(virt);

		spin_lock_irqsave(&smmu_domain->iotlb_gather_lock, flags);
		smmu_domain->deferred_flush = true;
		list_add(&page->lru, &smmu_domain->iotlb_gather_freelist);
		spin_unlock_irqrestore(&smmu_domain->iotlb_gather_lock, flags);
	} else {
		__arm_smmu_free_pgtable(smmu_domain, virt, order);
	}
}

static void arm_smmu_log_new_table(void *cookie, void *virt, unsigned long iova, size_t granule)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	trace_iommu_pgtable_add(smmu_domain, iova, __pa(virt), granule);
}

static void arm_smmu_log_remove_table(void *cookie, void *virt, unsigned long iova, size_t granule)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	trace_iommu_pgtable_remove(smmu_domain, iova, __pa(virt), granule);
}


static void arm_smmu_qcom_tlb_add_inv(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	unsigned long flags;

	spin_lock_irqsave(&smmu_domain->iotlb_gather_lock, flags);
	smmu_domain->deferred_flush = true;
	spin_unlock_irqrestore(&smmu_domain->iotlb_gather_lock, flags);
}

static void arm_smmu_qcom_tlb_sync(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	arm_smmu_rpm_get(smmu_domain->smmu);
	__arm_smmu_flush_iotlb_all(&smmu_domain->domain, false);
	arm_smmu_rpm_put(smmu_domain->smmu);
}

static const struct qcom_iommu_pgtable_ops arm_smmu_pgtable_ops = {
	.alloc = arm_smmu_alloc_pgtable,
	.free = arm_smmu_free_pgtable,
	.log_new_table = arm_smmu_log_new_table,
	.log_remove_table = arm_smmu_log_remove_table,
	.tlb_add_inv = arm_smmu_qcom_tlb_add_inv,
	.tlb_sync = arm_smmu_qcom_tlb_sync,
};

static int arm_smmu_alloc_context_bank(struct arm_smmu_domain *smmu_domain,
				       struct arm_smmu_device *smmu,
				       struct device *dev, unsigned int start)
{
	if (smmu->impl && smmu->impl->alloc_context_bank)
		return smmu->impl->alloc_context_bank(smmu_domain, smmu, dev, start);

	return __arm_smmu_alloc_cb(smmu->context_map, start, smmu->num_context_banks, dev,
				smmu_domain);
}

static int arm_smmu_init_domain_context(struct iommu_domain *domain,
					struct arm_smmu_device *smmu,
					struct device *dev)
{
	int irq, start, ret = 0;
	unsigned long ias, oas;
	struct io_pgtable_ops *pgtbl_ops;
	struct qcom_io_pgtable_info pgtbl_info;
	struct io_pgtable_cfg *pgtbl_cfg = &pgtbl_info.cfg;
	enum io_pgtable_fmt fmt;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	irqreturn_t (*context_fault)(int irq, void *dev);
	struct io_pgtable *iop;

	mutex_lock(&smmu_domain->init_mutex);
	if (smmu_domain->smmu)
		goto out_unlock;

	smmu_domain->dev = dev;
	ret = arm_smmu_setup_default_domain(dev, domain);
	if (ret) {
		dev_err(dev, "%s: default domain setup failed\n",
			__func__);
		goto out_unlock;
	}

	if (domain->type == IOMMU_DOMAIN_IDENTITY) {
		smmu_domain->stage = ARM_SMMU_DOMAIN_BYPASS;
		smmu_domain->smmu = smmu;
		goto out_unlock;
	}

	/*
	 * Mapping the requested stage onto what we support is surprisingly
	 * complicated, mainly because the spec allows S1+S2 SMMUs without
	 * support for nested translation. That means we end up with the
	 * following table:
	 *
	 * Requested        Supported        Actual
	 *     S1               N              S1
	 *     S1             S1+S2            S1
	 *     S1               S2             S2
	 *     S1               S1             S1
	 *     N                N              N
	 *     N              S1+S2            S2
	 *     N                S2             S2
	 *     N                S1             S1
	 *
	 * Note that you can't actually request stage-2 mappings.
	 */
	if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S1))
		smmu_domain->stage = ARM_SMMU_DOMAIN_S2;
	if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S2))
		smmu_domain->stage = ARM_SMMU_DOMAIN_S1;

	/*
	 * Choosing a suitable context format is even more fiddly. Until we
	 * grow some way for the caller to express a preference, and/or move
	 * the decision into the io-pgtable code where it arguably belongs,
	 * just aim for the closest thing to the rest of the system, and hope
	 * that the hardware isn't esoteric enough that we can't assume AArch64
	 * support to be a superset of AArch32 support...
	 */
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_L)
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH32_L;
	if (IS_ENABLED(CONFIG_IOMMU_IO_PGTABLE_ARMV7S) &&
	    !IS_ENABLED(CONFIG_64BIT) && !IS_ENABLED(CONFIG_ARM_LPAE) &&
	    (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_S) &&
	    (smmu_domain->stage == ARM_SMMU_DOMAIN_S1))
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH32_S;
	if ((IS_ENABLED(CONFIG_64BIT) || cfg->fmt == ARM_SMMU_CTX_FMT_NONE) &&
	    (smmu->features & (ARM_SMMU_FEAT_FMT_AARCH64_64K |
			       ARM_SMMU_FEAT_FMT_AARCH64_16K |
			       ARM_SMMU_FEAT_FMT_AARCH64_4K)))
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH64;

	if (cfg->fmt == ARM_SMMU_CTX_FMT_NONE) {
		ret = -EINVAL;
		goto out_unlock;
	}

	switch (smmu_domain->stage) {
	case ARM_SMMU_DOMAIN_S1:
		cfg->cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
		start = smmu->num_s2_context_banks;
		ias = smmu->va_size;
		oas = smmu->ipa_size;
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64) {
			fmt = QCOM_ARM_64_LPAE_S1;
			if (smmu->options & ARM_SMMU_OPT_3LVL_TABLES)
				ias = min(ias, 39UL);
		} else if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_L) {
			fmt = ARM_32_LPAE_S1;
			ias = min(ias, 32UL);
			oas = min(oas, 40UL);
		} else {
			fmt = ARM_V7S;
			ias = min(ias, 32UL);
			oas = min(oas, 32UL);
		}
		smmu_domain->flush_ops = &arm_smmu_s1_tlb_ops;
		break;
	case ARM_SMMU_DOMAIN_NESTED:
		/*
		 * We will likely want to change this if/when KVM gets
		 * involved.
		 */
	case ARM_SMMU_DOMAIN_S2:
		cfg->cbar = CBAR_TYPE_S2_TRANS;
		start = 0;
		ias = smmu->ipa_size;
		oas = smmu->pa_size;
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64) {
			fmt = ARM_64_LPAE_S2;
		} else {
			fmt = ARM_32_LPAE_S2;
			ias = min(ias, 40UL);
			oas = min(oas, 40UL);
		}
		if (smmu->version == ARM_SMMU_V2)
			smmu_domain->flush_ops = &arm_smmu_s2_tlb_ops_v2;
		else
			smmu_domain->flush_ops = &arm_smmu_s2_tlb_ops_v1;
		break;
	default:
		ret = -EINVAL;
		goto out_unlock;
	}

	if (test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes)) {
		fmt = ARM_V8L_FAST;
		ret = qcom_iommu_get_fast_iova_range(dev,
					&pgtbl_info.iova_base,
					&pgtbl_info.iova_end);
		if (ret < 0)
			goto out_unlock;
	}

	ret = arm_smmu_alloc_context_bank(smmu_domain, smmu, dev, start);
	if (ret < 0) {
		goto out_unlock;
	}

	smmu_domain->smmu = smmu;

	cfg->cbndx = ret;
	if (smmu->version < ARM_SMMU_V2) {
		cfg->irptndx = atomic_inc_return(&smmu->irptndx);
		cfg->irptndx %= smmu->num_context_irqs;
	} else {
		cfg->irptndx = cfg->cbndx;
	}

	if (smmu_domain->stage == ARM_SMMU_DOMAIN_S2)
		cfg->vmid = cfg->cbndx + 1;
	else
		cfg->asid = cfg->cbndx;

	pgtbl_info.iommu_pgtbl_ops = &arm_smmu_pgtable_ops;
	pgtbl_info.cfg = (struct io_pgtable_cfg) {
		.pgsize_bitmap	= smmu->pgsize_bitmap,
		.ias		= ias,
		.oas		= oas,
		.coherent_walk	= is_iommu_pt_coherent(smmu_domain),
		.tlb		= smmu_domain->flush_ops,
		.iommu_dev	= smmu->dev,
	};

	if (smmu->impl && smmu->impl->init_context) {
		ret = smmu->impl->init_context(smmu_domain,
					       pgtbl_cfg,
					       dev);
		if (ret)
			goto out_clear_smmu;
	}

	if (smmu_domain->non_strict)
		pgtbl_cfg->quirks |= IO_PGTABLE_QUIRK_NON_STRICT;
	pgtbl_cfg->quirks |= arm_smmu_domain_get_qcom_quirks(smmu_domain, smmu);

	pgtbl_ops = qcom_alloc_io_pgtable_ops(fmt, &pgtbl_info, smmu_domain);
	if (!pgtbl_ops) {
		ret = -ENOMEM;
		goto out_clear_smmu;
	}

	/* Update the domain's page sizes to reflect the page table format */
	domain->pgsize_bitmap = pgtbl_cfg->pgsize_bitmap;

	if (pgtbl_cfg->quirks & IO_PGTABLE_QUIRK_ARM_TTBR1) {
		domain->geometry.aperture_start = ~0UL << ias;
		domain->geometry.aperture_end = ~0UL;
	} else {
		domain->geometry.aperture_end = (1UL << ias) - 1;
	}

	domain->geometry.force_aperture = true;

	/* Initialise the context bank with our page table cfg */
	arm_smmu_init_context_bank(smmu_domain, pgtbl_cfg);
	if (smmu->impl && smmu->impl->init_context_bank)
		smmu->impl->init_context_bank(smmu_domain, dev);
	arm_smmu_write_context_bank(smmu, cfg->cbndx);

	/*
	 * Request context fault interrupt. Do this last to avoid the
	 * handler seeing a half-initialised domain state.
	 */
	irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];

	if (smmu->impl && smmu->impl->context_fault)
		context_fault = smmu->impl->context_fault;
	else
		context_fault = arm_smmu_context_fault;

	ret = devm_request_threaded_irq(smmu->dev, irq, NULL,
			context_fault, IRQF_ONESHOT | IRQF_SHARED,
			"arm-smmu-context-fault", domain);
	if (ret < 0) {
		dev_err(smmu->dev, "failed to request context IRQ %d (%u)\n",
			cfg->irptndx, irq);
		cfg->irptndx = ARM_SMMU_INVALID_IRPTNDX;
	}

	/*
	 * assign any page table memory that might have been allocated
	 * during alloc_io_pgtable_ops
	 */
	arm_smmu_secure_domain_lock(smmu_domain);
	ret = arm_smmu_assign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);
	if (ret) {
		dev_err(dev, "Failed to hyp-assign page table memory\n");
		goto out_clear_smmu;
	}

	iop = container_of(pgtbl_ops, struct io_pgtable, ops);
	ret = iommu_logger_register(&smmu_domain->logger, domain,
				    smmu_domain->dev, iop);
	if (ret) {
		dev_err(dev, "Log registration failed\n");
		goto out_clear_smmu;
	}

	ret = arm_smmu_get_dma_cookie(dev, smmu_domain, pgtbl_ops);
	if (ret)
		goto out_logger;

	/*
	 * Matches with call to arm_smmu_rpm_put in
	 * arm_smmu_destroy_domain_context.
	 */
	if (test_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes)) {
		smmu_domain->rpm_always_on = true;
		/* During device shutdown, if non atomic client is doing the register
		 * space access, it experiencing the unclocked access error becaue of
		 * force suspend from the device remove path, to avoid such case
		 * adding smmu atomic refcount, to know is there any atomic client or
		 * not, if there is no atomic client it will skip the force suspend and
		 * power off.
		 */
		smmu->atomic_pwr_refcount++;
		arm_smmu_rpm_get(smmu);
	}

	mutex_unlock(&smmu_domain->init_mutex);

	/* Publish page table ops for map/unmap */
	smmu_domain->pgtbl_ops = pgtbl_ops;
	return 0;

out_logger:
	iommu_logger_unregister(smmu_domain->logger);
	smmu_domain->logger = NULL;
out_clear_smmu:
	arm_smmu_destroy_domain_context(domain);
	smmu_domain->smmu = NULL;
out_unlock:
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static void arm_smmu_domain_reinit(struct arm_smmu_domain *smmu_domain)
{
	smmu_domain->cfg.irptndx = ARM_SMMU_INVALID_IRPTNDX;
	smmu_domain->cfg.cbndx = INVALID_CBNDX;
	smmu_domain->secure_vmid = VMID_INVAL;
}

static void arm_smmu_destroy_domain_context(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	int irq;
	int ret, i;
	bool pinned = false;

	if (!smmu || domain->type == IOMMU_DOMAIN_IDENTITY)
		return;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return;

	/*
	 * Matches with call to arm_smmu_rpm_get in
	 * arm_smmu_init_domain_contxt.
	 */
	if (smmu_domain->rpm_always_on) {
		smmu->atomic_pwr_refcount--;
		arm_smmu_rpm_put(smmu);
	}

	/*
	 * Disable the context bank and free the page tables before freeing
	 * it.
	 */
	smmu->cbs[cfg->cbndx].cfg = NULL;
	arm_smmu_write_context_bank(smmu, cfg->cbndx);

	if (cfg->irptndx != ARM_SMMU_INVALID_IRPTNDX) {
		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		devm_free_irq(smmu->dev, irq, domain);
	}

	qcom_free_io_pgtable_ops(smmu_domain->pgtbl_ops);
	arm_smmu_secure_domain_lock(smmu_domain);
	arm_smmu_secure_pool_destroy(smmu_domain);
	arm_smmu_unassign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);

	for (i = 0; i < smmu->num_mapping_groups; i++)
		if ((cfg->cbndx == smmu->s2crs[i].cbndx) &&
		    (smmu->s2crs[i].pinned)) {
			pinned = true;
		}

	if (!pinned)
		__arm_smmu_free_bitmap(smmu->context_map, cfg->cbndx);

	arm_smmu_rpm_put(smmu);
	arm_smmu_domain_reinit(smmu_domain);
}

static struct iommu_domain *arm_smmu_domain_alloc(unsigned type)
{
	struct arm_smmu_domain *smmu_domain;

	if (type != IOMMU_DOMAIN_UNMANAGED &&
	    type != IOMMU_DOMAIN_DMA &&
	    type != IOMMU_DOMAIN_IDENTITY)
		return NULL;
	/*
	 * Allocate the domain and initialise some of its data structures.
	 * We can't really do anything meaningful until we've added a
	 * master.
	 */
	smmu_domain = kzalloc(sizeof(*smmu_domain), GFP_KERNEL);
	if (!smmu_domain)
		return NULL;

	mutex_init(&smmu_domain->init_mutex);
	spin_lock_init(&smmu_domain->cb_lock);
	spin_lock_init(&smmu_domain->sync_lock);
	INIT_LIST_HEAD(&smmu_domain->pte_info_list);
	INIT_LIST_HEAD(&smmu_domain->unassign_list);
	mutex_init(&smmu_domain->assign_lock);
	INIT_LIST_HEAD(&smmu_domain->secure_pool_list);
	spin_lock_init(&smmu_domain->iotlb_gather_lock);
	INIT_LIST_HEAD(&smmu_domain->iotlb_gather_freelist);
	arm_smmu_domain_reinit(smmu_domain);

	return &smmu_domain->domain;
}

static void arm_smmu_domain_free(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	/*
	 * Free the domain resources. We assume that all devices have
	 * already been detached.
	 */
	arm_smmu_put_dma_cookie(domain);
	arm_smmu_destroy_domain_context(domain);
	iommu_logger_unregister(smmu_domain->logger);
	kfree(smmu_domain);
}

static void arm_smmu_write_smr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = FIELD_PREP(ARM_SMMU_SMR_ID, smr->id) |
		  FIELD_PREP(ARM_SMMU_SMR_MASK, smr->mask);

	if (!(smmu->features & ARM_SMMU_FEAT_EXIDS) && smr->valid) {
		reg |= ARM_SMMU_SMR_VALID;
		smr->state = SMR_PROGRAMMED;
	} else {
		smr->state = SMR_INVALID;
	}
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(idx), reg);
}


static void arm_smmu_write_s2cr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_s2cr *s2cr = smmu->s2crs + idx;
	u32 reg;

	if (smmu->impl && smmu->impl->write_s2cr) {
		smmu->impl->write_s2cr(smmu, idx);
		return;
	}

	reg = FIELD_PREP(ARM_SMMU_S2CR_TYPE, s2cr->type) |
	      FIELD_PREP(ARM_SMMU_S2CR_CBNDX, s2cr->cbndx) |
	      FIELD_PREP(ARM_SMMU_S2CR_PRIVCFG, s2cr->privcfg) |
	      FIELD_PREP(ARM_SMMU_S2CR_SHCFG, ARM_SMMU_S2CR_SHCFG_NSH);

	if (smmu->features & ARM_SMMU_FEAT_EXIDS && smmu->smrs &&
	    smmu->smrs[idx].valid)
		reg |= ARM_SMMU_S2CR_EXIDVALID;
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_S2CR(idx), reg);
}

static void arm_smmu_write_sme(struct arm_smmu_device *smmu, int idx)
{
	arm_smmu_write_s2cr(smmu, idx);
	if (smmu->smrs)
		arm_smmu_write_smr(smmu, idx);
}

/*
 * The width of SMR's mask field depends on sCR0_EXIDENABLE, so this function
 * should be called after sCR0 is written.
 */
static void arm_smmu_test_smr_masks(struct arm_smmu_device *smmu)
{
	u32 smr;
	int i;

	if (!smmu->smrs)
		return;
	/*
	 * If we've had to accommodate firmware memory regions, we may
	 * have live SMRs by now; tread carefully...
	 *
	 * Somewhat perversely, not having a free SMR for this test implies we
	 * can get away without it anyway, as we'll only be able to 'allocate'
	 * these SMRs for the ID/mask values we're already trusting to be OK.
	 */
	for (i = 0; i < smmu->num_mapping_groups; i++)
		if (!smmu->smrs[i].valid)
			goto smr_ok;
	return;
smr_ok:
	/*
	 * SMR.ID bits may not be preserved if the corresponding MASK
	 * bits are set, so check each one separately. We can reject
	 * masters later if they try to claim IDs outside these masks.
	 */
	smr = FIELD_PREP(ARM_SMMU_SMR_ID, smmu->streamid_mask);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(i), smr);
	smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(i));
	smmu->streamid_mask = FIELD_GET(ARM_SMMU_SMR_ID, smr);

	smr = FIELD_PREP(ARM_SMMU_SMR_MASK, smmu->streamid_mask);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(i), smr);
	smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(i));
	smmu->smr_mask_mask = FIELD_GET(ARM_SMMU_SMR_MASK, smr);
}

static int arm_smmu_find_sme(struct arm_smmu_device *smmu, u16 id, u16 mask)
{
	struct arm_smmu_smr *smrs = smmu->smrs;
	int i, free_idx = -ENOSPC;

	/* Stream indexing is blissfully easy */
	if (!smrs)
		return id;

	/* Validating SMRs is... less so */
	for (i = 0; i < smmu->num_mapping_groups; ++i) {
		if (!smrs[i].valid) {
			/*
			 * Note the first free entry we come across, which
			 * we'll claim in the end if nothing else matches.
			 */
			if (free_idx < 0)
				free_idx = i;
			continue;
		}
		/*
		 * If the new entry is _entirely_ matched by an existing entry,
		 * then reuse that, with the guarantee that there also cannot
		 * be any subsequent conflicting entries. In normal use we'd
		 * expect simply identical entries for this case, but there's
		 * no harm in accommodating the generalisation.
		 */
		if ((mask & smrs[i].mask) == mask &&
		    !((id ^ smrs[i].id) & ~smrs[i].mask))
			return i;
		/*
		 * If the new entry has any other overlap with an existing one,
		 * though, then there always exists at least one stream ID
		 * which would cause a conflict, and we can't allow that risk.
		 */
		if (!((id ^ smrs[i].id) & ~(smrs[i].mask | mask)))
			return -EINVAL;
	}

	return free_idx;
}

static bool arm_smmu_free_sme(struct arm_smmu_device *smmu, int idx)
{
	bool pinned = smmu->s2crs[idx].pinned;
	u8 cbndx = smmu->s2crs[idx].cbndx;

	if (--smmu->s2crs[idx].count)
		return false;

	smmu->s2crs[idx] = s2cr_init_val;
	if (pinned) {
		smmu->s2crs[idx].pinned = true;
		smmu->s2crs[idx].cbndx = cbndx;
	} else if (smmu->smrs) {
		smmu->smrs[idx].valid = false;
	}

	return true;
}

static int arm_smmu_master_alloc_smes(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_master_cfg *cfg = dev_iommu_priv_get(dev);
	struct arm_smmu_device *smmu = cfg->smmu;
	struct arm_smmu_smr *smrs = smmu->smrs;
	int i, idx, ret;

	mutex_lock(&smmu->iommu_group_mutex);
	mutex_lock(&smmu->stream_map_mutex);
	/* Figure out a viable stream map entry allocation */
	for_each_cfg_sme(cfg, fwspec, i, idx) {
		u16 sid = FIELD_GET(ARM_SMMU_SMR_ID, fwspec->ids[i]);
		u16 mask = FIELD_GET(ARM_SMMU_SMR_MASK, fwspec->ids[i]);

		if (idx != INVALID_SMENDX) {
			ret = -EEXIST;
			goto sme_err;
		}

		ret = arm_smmu_find_sme(smmu, sid, mask);
		if (ret < 0)
			goto sme_err;

		idx = ret;
		if (smrs && smmu->s2crs[idx].count == 0) {
			smrs[idx].id = sid;
			smrs[idx].mask = mask;
			smrs[idx].valid = true;
			smrs[idx].state = SMR_ALLOCATED;
		}
		smmu->s2crs[idx].count++;
		cfg->smendx[i] = (s16)idx;
	}
	mutex_unlock(&smmu->stream_map_mutex);

	/* It worked! Now, poke the actual hardware */
	for_each_cfg_sme(cfg, fwspec, i, idx) {
		arm_smmu_write_sme(smmu, idx);
	}

	mutex_unlock(&smmu->iommu_group_mutex);
	return 0;

sme_err:
	while (i--) {
		arm_smmu_free_sme(smmu, cfg->smendx[i]);
		cfg->smendx[i] = INVALID_SMENDX;
	}
	mutex_unlock(&smmu->stream_map_mutex);
	mutex_unlock(&smmu->iommu_group_mutex);
	return ret;
}

static void arm_smmu_master_free_smes(struct arm_smmu_master_cfg *cfg,
				      struct iommu_fwspec *fwspec)
{
	struct arm_smmu_device *smmu = cfg->smmu;
	int i, idx;

	mutex_lock(&smmu->stream_map_mutex);
	for_each_cfg_sme(cfg, fwspec, i, idx) {
		if (arm_smmu_free_sme(smmu, idx))
			arm_smmu_write_sme(smmu, idx);
		cfg->smendx[i] = INVALID_SMENDX;
	}
	mutex_unlock(&smmu->stream_map_mutex);
}

static int arm_smmu_domain_add_master(struct arm_smmu_domain *smmu_domain,
				      struct arm_smmu_master_cfg *cfg,
				      struct iommu_fwspec *fwspec)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_s2cr *s2cr = smmu->s2crs;
	u8 cbndx = smmu_domain->cfg.cbndx;
	enum arm_smmu_s2cr_type type;
	int i, idx;

	if (smmu_domain->stage == ARM_SMMU_DOMAIN_BYPASS)
		type = S2CR_TYPE_BYPASS;
	else
		type = S2CR_TYPE_TRANS;

	mutex_lock(&smmu->stream_map_mutex);
	for_each_cfg_sme(cfg, fwspec, i, idx) {
		if (type == s2cr[idx].type && cbndx == s2cr[idx].cbndx)
			continue;

		/* Don't bypasss pinned streams; leave them as they are */
		if (type == S2CR_TYPE_BYPASS && s2cr[idx].pinned)
			continue;

		s2cr[idx].type = type;
		s2cr[idx].privcfg = S2CR_PRIVCFG_DEFAULT;
		s2cr[idx].cbndx = cbndx;
		arm_smmu_write_s2cr(smmu, idx);
	}
	mutex_unlock(&smmu->stream_map_mutex);

	return 0;
}

static int arm_smmu_assign_table(struct arm_smmu_domain *smmu_domain)
{
	int ret = 0;
	int dest_vmids[2] = {VMID_HLOS, smmu_domain->secure_vmid};
	int dest_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ};
	int source_vmid = VMID_HLOS;
	struct arm_smmu_pte_info *pte_info, *temp;

	if (!arm_smmu_has_secure_vmid(smmu_domain))
		return ret;

	list_for_each_entry(pte_info, &smmu_domain->pte_info_list, entry) {
		ret = hyp_assign_phys(virt_to_phys(pte_info->virt_addr),
				      PAGE_SIZE, &source_vmid, 1,
				      dest_vmids, dest_perms, 2);
		if (WARN_ON(ret))
			break;
	}

	list_for_each_entry_safe(pte_info, temp, &smmu_domain->pte_info_list,
								entry) {
		list_del(&pte_info->entry);
		kfree(pte_info);
	}
	return ret;
}

static void arm_smmu_unassign_table(struct arm_smmu_domain *smmu_domain)
{
	int ret;
	int dest_vmids = VMID_HLOS;
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int source_vmlist[2] = {VMID_HLOS, smmu_domain->secure_vmid};
	struct arm_smmu_pte_info *pte_info, *temp;

	if (!arm_smmu_has_secure_vmid(smmu_domain))
		return;

	list_for_each_entry(pte_info, &smmu_domain->unassign_list, entry) {
		ret = hyp_assign_phys(virt_to_phys(pte_info->virt_addr),
				      PAGE_SIZE, source_vmlist, 2,
				      &dest_vmids, &dest_perms, 1);
		if (WARN_ON(ret))
			break;
		free_pages((unsigned long)pte_info->virt_addr,
			   get_order(pte_info->size));
	}

	list_for_each_entry_safe(pte_info, temp, &smmu_domain->unassign_list,
				 entry) {
		list_del(&pte_info->entry);
		kfree(pte_info);
	}
}

static struct device_node *arm_iommu_get_of_node(struct device *dev)
{
	struct device_node *np;

	if (!dev->of_node)
		return NULL;

	np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	return np ? np : dev->of_node;
}

static int arm_smmu_setup_default_domain(struct device *dev,
					 struct iommu_domain *domain)
{
	struct device_node *np;
	int ret;
	const char *str;
	int attr = 1;
	u32 val;

	np = arm_iommu_get_of_node(dev);
	if (!np)
		return 0;

	ret = of_property_read_string(np, "qcom,iommu-dma", &str);
	if (ret)
		str = "default";

	if (!strcmp(str, "bypass")) {
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_S1_BYPASS, &attr);
	/*
	 * Fallback to the upstream dma-allocator if fastmap is not enabled.
	 * "fastmap" implies "atomic" due to it not calling arm_smmu_rpm_get()
	 * in its map/unmap functions. Its clients may or may not actually
	 * use iommu apis from atomic context.
	 */
	} else if (!strcmp(str, "fastmap")) {
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_ATOMIC, &attr);
		if (IS_ENABLED(CONFIG_IOMMU_IO_PGTABLE_FAST))
			__arm_smmu_domain_set_attr(
				domain, DOMAIN_ATTR_FAST, &attr);
	} else if (!strcmp(str, "atomic")) {
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_ATOMIC, &attr);
	} else if (!strcmp(str, "disabled")) {
		/* DT properties only intended for use by default-domains */
		return 0;
	}

	/*
	 * default value:
	 * Stall-on-fault
	 * faults trigger kernel panic
	 * return abort
	 */
	if (of_property_match_string(np, "qcom,iommu-faults",
				     "stall-disable") >= 0)
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_FAULT_MODEL_NO_STALL, &attr);

	if (of_property_match_string(np, "qcom,iommu-faults", "no-CFRE") >= 0)
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_FAULT_MODEL_NO_CFRE, &attr);

	if (of_property_match_string(np, "qcom,iommu-faults", "HUPCF") >= 0)
		__arm_smmu_domain_set_attr(domain,
					   DOMAIN_ATTR_FAULT_MODEL_HUPCF,
					   &attr);

	if (of_property_match_string(np, "qcom,iommu-faults", "non-fatal") >= 0)
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_NON_FATAL_FAULTS, &attr);

	/* Default value: disabled */
	ret = of_property_read_u32(np, "qcom,iommu-vmid", &val);
	if (!ret) {
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_SECURE_VMID, &val);
	}

	/* Default value: disabled */
	ret = of_property_read_string(np, "qcom,iommu-pagetable", &str);
	if (ret)
		str = "disabled";
	if (!strcmp(str, "coherent"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT, &attr);
	else if (!strcmp(str, "LLC"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_USE_UPSTREAM_HINT, &attr);
	else if (!strcmp(str, "LLC_NWA"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_USE_LLC_NWA, &attr);


	/* Default value: disabled */
	if (of_property_read_bool(np, "qcom,iommu-earlymap"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_EARLY_MAP, &attr);
	return 0;
}

struct lookup_iommu_group_data {
	struct device_node *np;
	struct iommu_group *group;
};

/* This isn't a "fast lookup" since its N^2, but probably good enough */
static int __bus_lookup_iommu_group(struct device *dev, void *priv)
{
	struct lookup_iommu_group_data *data = priv;
	struct device_node *np;
	struct iommu_group *group;

	group = iommu_group_get(dev);
	if (!group)
		return 0;

	np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	if (np != data->np) {
		iommu_group_put(group);
		return 0;
	}

	data->group = group;
	return 1;
}

static struct iommu_group *of_get_device_group(struct device *dev)
{
	struct lookup_iommu_group_data data = {
		.np = NULL,
		.group = NULL,
	};
	struct iommu_group *group;
	int ret;

	data.np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	if (!data.np)
		return NULL;

	ret = bus_for_each_dev(&platform_bus_type, NULL, &data,
				__bus_lookup_iommu_group);
	if (ret > 0)
		return data.group;

#ifdef CONFIG_PCI
	ret = bus_for_each_dev(&pci_bus_type, NULL, &data,
				__bus_lookup_iommu_group);
	if (ret > 0)
		return data.group;
#endif

	group = generic_device_group(dev);
	if (IS_ERR(group))
		return NULL;
	return group;
}

static int arm_smmu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_master_cfg *cfg;
	struct arm_smmu_device *smmu;
	int ret;

	if (!fwspec || fwspec->ops != &arm_smmu_ops.iommu_ops) {
		dev_err(dev, "cannot attach to SMMU, is it on the same bus?\n");
		return -ENXIO;
	}

	/*
	 * FIXME: The arch/arm DMA API code tries to attach devices to its own
	 * domains between of_xlate() and probe_device() - we have no way to cope
	 * with that, so until ARM gets converted to rely on groups and default
	 * domains, just say no (but more politely than by dereferencing NULL).
	 * This should be at least a WARN_ON once that's sorted.
	 */
	cfg = dev_iommu_priv_get(dev);
	if (!cfg)
		return -ENODEV;

	smmu = cfg->smmu;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return ret;

	/* Ensure that the domain is finalised */
	ret = arm_smmu_init_domain_context(domain, smmu, dev);
	if (ret < 0)
		goto out_power_off;

	/*
	 * Sanity check the domain. We don't support domains across
	 * different SMMUs.
	 */
	if (smmu_domain->smmu != smmu) {
		dev_err(dev,
			"cannot attach to SMMU %s whilst already attached to domain on SMMU %s\n",
			dev_name(smmu_domain->smmu->dev), dev_name(smmu->dev));
		ret = -EINVAL;
		goto out_power_off;
	}

	/* Looks ok, so add the device to the domain */
	ret = arm_smmu_domain_add_master(smmu_domain, cfg, fwspec);

	/*
	 * Setup an autosuspend delay to avoid bouncing runpm state.
	 * Otherwise, if a driver for a suspended consumer device
	 * unmaps buffers, it will runpm resume/suspend for each one.
	 *
	 * For example, when used by a GPU device, when an application
	 * or game exits, it can trigger unmapping 100s or 1000s of
	 * buffers.  With a runpm cycle for each buffer, that adds up
	 * to 5-10sec worth of reprogramming the context bank, while
	 * the system appears to be locked up to the user.
	 */
	pm_runtime_set_autosuspend_delay(smmu->dev, 20);
	pm_runtime_use_autosuspend(smmu->dev);

out_power_off:
		arm_smmu_rpm_put(smmu);

	return ret;
}

static gfp_t arm_smmu_domain_gfp_flags(struct arm_smmu_domain *smmu_domain)
{
	/*
	 * The dma layer always uses GFP_ATOMIC, which isn't indicative of
	 * the actual client needs.
	 */
	if (test_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes))
		return GFP_ATOMIC;

	return GFP_KERNEL;
}

static int arm_smmu_map_pages(struct iommu_domain *domain, unsigned long iova,
			      phys_addr_t paddr, size_t pgsize, size_t pgcount,
			      int prot, gfp_t gfp, size_t *mapped)
{
	int ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = to_smmu_domain(domain)->pgtbl_ops;
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (!ops)
		return -ENODEV;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return ret;

	gfp = arm_smmu_domain_gfp_flags(smmu_domain);
	arm_smmu_secure_domain_lock(smmu_domain);
	ret = ops->map_pages(ops, iova, paddr, pgsize, pgcount, prot, gfp, mapped);

	if (ret)
		goto out;

	ret = arm_smmu_assign_table(smmu_domain);

out:
	arm_smmu_secure_domain_unlock(smmu_domain);
	arm_smmu_rpm_put(smmu);
	if (!ret)
		trace_map_pages(smmu_domain, iova, pgsize, pgcount);

	return ret;
}

static int arm_smmu_map_sg(struct iommu_domain *domain, unsigned long iova,
			   struct scatterlist *sg, unsigned int nents, int prot,
			   gfp_t gfp, size_t *mapped)
{
	int ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = to_smmu_domain(domain)->pgtbl_ops;
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (!ops)
		return -ENODEV;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return ret;

	gfp = arm_smmu_domain_gfp_flags(smmu_domain);
	arm_smmu_secure_domain_lock(smmu_domain);
	ret = ops->map_sg(ops, iova, sg, nents, prot, gfp, mapped);

	if (ret)
		goto out;

	ret = arm_smmu_assign_table(smmu_domain);

out:
	arm_smmu_secure_domain_unlock(smmu_domain);
	arm_smmu_rpm_put(smmu);
	if (!ret)
		trace_map_sg(smmu_domain, iova, sg, nents);

	return ret;
}

static size_t arm_smmu_unmap_pages(struct iommu_domain *domain, unsigned long iova,
				   size_t pgsize, size_t pgcount,
				   struct iommu_iotlb_gather *gather)
{
	size_t ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	if (!ops)
		return 0;

	arm_smmu_secure_domain_lock(smmu_domain);

	ret = ops->unmap_pages(ops, iova, pgsize, pgcount, gather);

	/*
	 * While splitting up block mappings, we might allocate page table
	 * memory during unmap, so the vmids needs to be assigned to the
	 * memory here as well.
	 */
	arm_smmu_assign_table(smmu_domain);
	/* Also unassign any pages that were free'd during unmap */
	arm_smmu_unassign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);

	if (ret)
		trace_unmap_pages(smmu_domain, iova, pgsize, pgcount);
	return ret;
}

static void arm_smmu_flush_iotlb_all(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu_domain->flush_ops) {
		arm_smmu_rpm_get(smmu);
		/* Secure pages may be freed to the secure pool after TLB maintenance. */
		arm_smmu_secure_domain_lock(smmu_domain);
		__arm_smmu_flush_iotlb_all(domain, true);
		arm_smmu_secure_domain_unlock(smmu_domain);
		arm_smmu_rpm_put(smmu);
	}
}

/*
 * Caller must call arm_smmu_rpm_get(). Secure context banks must also hold
 * arm_smmu_secure_domain_lock.
 */
static void __arm_smmu_flush_iotlb_all(struct iommu_domain *domain, bool force)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	LIST_HEAD(list);
	struct page *page, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&smmu_domain->iotlb_gather_lock, flags);
	/*
	 * iommu_flush_iotlb_all currently has 2 users which do not set
	 * deferred_flush through qcom_iommu_pgtable_ops->tlb_add_inv
	 * 1) GPU - old implementation uses upstream io-pgtable-arm.c
	 * 2) fastmap
	 * once these users have gone away, force parameter can be removed.
	 */
	if (!force && !smmu_domain->deferred_flush) {
		spin_unlock_irqrestore(&smmu_domain->iotlb_gather_lock, flags);
		return;
	}

	smmu_domain->flush_ops->tlb_flush_all(smmu_domain);

	list_splice_init(&smmu_domain->iotlb_gather_freelist, &list);
	smmu_domain->deferred_flush = false;

	list_for_each_entry_safe(page, tmp, &list, lru) {
		list_del(&page->lru);
		__arm_smmu_free_pgtable(smmu_domain, page_address(page), 0);
	}

	spin_unlock_irqrestore(&smmu_domain->iotlb_gather_lock, flags);
}

static void arm_smmu_iotlb_sync(struct iommu_domain *domain,
				struct iommu_iotlb_gather *gather)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (!smmu)
		return;

	arm_smmu_flush_iotlb_all(domain);
}

static phys_addr_t __arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
					      dma_addr_t iova)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;
	struct device *dev = smmu->dev;
	void __iomem *reg;
	u32 tmp;
	u64 phys;
	unsigned long va, flags;
	int idx  = cfg->cbndx;
	phys_addr_t addr = 0;

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	va = iova & ~0xfffUL;
	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
		arm_smmu_cb_writeq(smmu, idx, ARM_SMMU_CB_ATS1PR, va);
	else
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_ATS1PR, va);

	reg = arm_smmu_page(smmu, ARM_SMMU_CB(smmu, idx)) + ARM_SMMU_CB_ATSR;
	if (readl_poll_timeout_atomic(reg,tmp,
				      !(tmp & ARM_SMMU_ATSR_ACTIVE), 5, 50)) {
		spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
		phys = ops->iova_to_phys(ops, iova);
		dev_err(dev,

			"iova to phys timed out on %pad. Falling back to software table walk.\n",
			&iova);
		arm_smmu_rpm_put(smmu);
		return ops->iova_to_phys(ops, iova);
	}

	phys = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_PAR);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	if (phys & ARM_SMMU_CB_PAR_F) {
		dev_err(dev, "translation fault!\n");
		dev_err(dev, "PAR = 0x%llx\n", phys);
		goto out;
	}

	addr = (phys & GENMASK_ULL(39, 12)) | (iova & 0xfff);
out:

	return addr;
}

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	phys_addr_t phys;

	if (domain->type == IOMMU_DOMAIN_IDENTITY)
		return iova;

	if (!ops)
		return 0;

	if (smmu_domain->smmu->features & ARM_SMMU_FEAT_TRANS_OPS &&
	    smmu_domain->stage == ARM_SMMU_DOMAIN_S1) {
		if (arm_smmu_rpm_get(smmu) < 0)
			return 0;
		phys = __arm_smmu_iova_to_phys_hard(domain, iova);

		arm_smmu_rpm_put(smmu);

		return phys;
	}

	return ops->iova_to_phys(ops, iova);
}

/*
 * This function can sleep, and cannot be called from atomic context. Will
 * power on register block if required. This restriction does not apply to the
 * original iova_to_phys() op.
 */
static phys_addr_t arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
				    struct qcom_iommu_atos_txn *txn)
{
	phys_addr_t ret = 0;

	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu->options & ARM_SMMU_OPT_DISABLE_ATOS)
		return 0;

	if (arm_smmu_rpm_get(smmu) < 0)
		return 0;

	if (smmu->impl && smmu->impl->iova_to_phys_hard) {
		ret = smmu->impl->iova_to_phys_hard(smmu_domain, txn);
		goto out;
	}

	if (smmu_domain->smmu->features & ARM_SMMU_FEAT_TRANS_OPS &&
			smmu_domain->stage == ARM_SMMU_DOMAIN_S1)
		ret = __arm_smmu_iova_to_phys_hard(domain, txn->addr);


out:
	arm_smmu_rpm_put(smmu);

	return ret;
}

static bool arm_smmu_capable(enum iommu_cap cap)
{
	switch (cap) {
	case IOMMU_CAP_CACHE_COHERENCY:
		/*
		 * Return true here as the SMMU can always send out coherent
		 * requests.
		 */
		return true;
	case IOMMU_CAP_NOEXEC:
		return true;
	default:
		return false;
	}
}

static
struct arm_smmu_device *arm_smmu_get_by_fwnode(struct fwnode_handle *fwnode)
{
	struct device *dev = driver_find_device_by_fwnode(&arm_smmu_driver.driver,
							  fwnode);
	put_device(dev);
	return dev ? dev_get_drvdata(dev) : NULL;
}

static struct iommu_device * arm_smmu_probe_device(struct device *dev)
{
	struct arm_smmu_device *smmu = NULL;
	struct arm_smmu_master_cfg *cfg;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	int i, ret;

	if (using_legacy_binding) {
		ret = arm_smmu_register_legacy_master(dev, &smmu);

		/*
		 * If dev->iommu_fwspec is initally NULL, arm_smmu_register_legacy_master()
		 * will allocate/initialise a new one. Thus we need to update fwspec for
		 * later use.
		 */
		fwspec = dev_iommu_fwspec_get(dev);
		if (ret)
			goto out_free;
	} else if (fwspec && fwspec->ops == &arm_smmu_ops.iommu_ops) {
		smmu = arm_smmu_get_by_fwnode(fwspec->iommu_fwnode);
		if (!smmu)
			return ERR_PTR(-ENODEV);
	} else {
		return ERR_PTR(-ENODEV);
	}

	ret = -EINVAL;
	for (i = 0; i < fwspec->num_ids; i++) {
		u16 sid = FIELD_GET(ARM_SMMU_SMR_ID, fwspec->ids[i]);
		u16 mask = FIELD_GET(ARM_SMMU_SMR_MASK, fwspec->ids[i]);

		if (sid & ~smmu->streamid_mask) {
			dev_err(dev, "stream ID 0x%x out of range for SMMU (0x%x)\n",
				sid, smmu->streamid_mask);
			goto out_free;
		}
		if (mask & ~smmu->smr_mask_mask) {
			dev_err(dev, "SMR mask 0x%x out of range for SMMU (0x%x)\n",
				mask, smmu->smr_mask_mask);
			goto out_free;
		}
	}

	ret = -ENOMEM;
	cfg = kzalloc(offsetof(struct arm_smmu_master_cfg, smendx[i]),
		      GFP_KERNEL);
	if (!cfg)
		goto out_free;

	cfg->smmu = smmu;
	dev_iommu_priv_set(dev, cfg);
	while (i--)
		cfg->smendx[i] = INVALID_SMENDX;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		goto out_cfg_free;

	ret = arm_smmu_master_alloc_smes(dev);
	arm_smmu_rpm_put(smmu);

	if (ret)
		goto out_cfg_free;

	device_link_add(dev, smmu->dev,
			DL_FLAG_PM_RUNTIME | DL_FLAG_AUTOREMOVE_SUPPLIER);

	return &smmu->iommu;

out_cfg_free:
	kfree(cfg);
out_free:
	iommu_fwspec_free(dev);
	return ERR_PTR(ret);
}

static void arm_smmu_release_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_master_cfg *cfg;
	struct arm_smmu_device *smmu;
	int ret;

	if (!fwspec || fwspec->ops != &arm_smmu_ops.iommu_ops)
		return;

	cfg  = dev_iommu_priv_get(dev);
	smmu = cfg->smmu;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return;

	arm_smmu_master_free_smes(cfg, fwspec);

	arm_smmu_rpm_put(smmu);

	dev_iommu_priv_set(dev, NULL);
	kfree(cfg);
	iommu_fwspec_free(dev);
}

static struct iommu_group *arm_smmu_device_group(struct device *dev)
{
	struct arm_smmu_master_cfg *cfg = dev_iommu_priv_get(dev);
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_device *smmu = cfg->smmu;
	struct iommu_group *group = NULL;
	int i, idx;

	group = of_get_device_group(dev);
	if (group)
		goto finish;

	for_each_cfg_sme(cfg, fwspec, i, idx) {
		if (group && smmu->s2crs[idx].group &&
		    group != smmu->s2crs[idx].group) {
			dev_err(dev, "ID:%x IDX:%x is already in a group!\n",
				fwspec->ids[i], idx);
			return ERR_PTR(-EINVAL);
		}

		group = smmu->s2crs[idx].group;
	}

	if (group)
		iommu_group_ref_get(group);
	else {
		if (dev_is_pci(dev))
			group = pci_device_group(dev);
		else if (dev_is_fsl_mc(dev))
			group = fsl_mc_device_group(dev);
		else
			group = generic_device_group(dev);

		if (IS_ERR(group))
			return NULL;
	}
finish:
	if (!IS_ERR(group) && smmu->impl && smmu->impl->device_group &&
	    smmu->impl->device_group(dev, group)) {
		iommu_group_put(group);
		return ERR_PTR(-EINVAL);
	}

	/* Remember group for faster lookups */
	if (!IS_ERR(group))
		for_each_cfg_sme(cfg, fwspec, i, idx)
			smmu->s2crs[idx].group = group;

	return group;
}

static int arm_smmu_domain_get_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int ret = 0;
	unsigned long iommu_attr = (unsigned long)attr;

	mutex_lock(&smmu_domain->init_mutex);
	switch (iommu_attr) {
	case DOMAIN_ATTR_NESTING:
		*(int *)data = (smmu_domain->stage == ARM_SMMU_DOMAIN_NESTED);
		ret = 0;
		break;
	case DOMAIN_ATTR_DMA_USE_FLUSH_QUEUE:
		*(int *)data = smmu_domain->non_strict;
		ret = 0;
		break;
	case DOMAIN_ATTR_CONTEXT_BANK:
		/* context bank index isn't valid until we are attached */
		if (smmu_domain->smmu == NULL) {
			ret = -ENODEV;
			break;
		}
		*((unsigned int *) data) = smmu_domain->cfg.cbndx;
		ret = 0;
		break;
	case DOMAIN_ATTR_NON_FATAL_FAULTS:
		*((int *)data) = test_bit(DOMAIN_ATTR_NON_FATAL_FAULTS,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_S1_BYPASS:
		*((int *)data) = test_bit(DOMAIN_ATTR_S1_BYPASS,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_SECURE_VMID:
		*((int *)data) = smmu_domain->secure_vmid;
		ret = 0;
		break;
	case DOMAIN_ATTR_PGTBL_INFO: {
		struct iommu_pgtbl_info *info = data;

		if (!test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes)) {
			ret = -ENODEV;
			break;
		}
		info->ops = smmu_domain->pgtbl_ops;
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_FAST:
		*((int *)data) = test_bit(DOMAIN_ATTR_FAST,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_UPSTREAM_HINT:
		*((int *)data) = test_bit(DOMAIN_ATTR_USE_UPSTREAM_HINT,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_LLC_NWA:
		*((int *)data) = test_bit(DOMAIN_ATTR_USE_LLC_NWA,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_EARLY_MAP:
		*((int *)data) = test_bit(DOMAIN_ATTR_EARLY_MAP,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_PAGE_TABLE_IS_COHERENT:
		if (!smmu_domain->smmu) {
			ret = -ENODEV;
			break;
		}
		*((int *)data) = is_iommu_pt_coherent(smmu_domain);
		ret = 0;
		break;
	case DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT:
		*((int *)data) = test_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_FAULT_MODEL_NO_CFRE:
	case DOMAIN_ATTR_FAULT_MODEL_NO_STALL:
	case DOMAIN_ATTR_FAULT_MODEL_HUPCF:
		*((int *)data) = test_bit(attr, smmu_domain->attributes);
		ret = 0;
		break;
	default:
		ret = -ENODEV;
		break;
	}
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static int __arm_smmu_domain_set_attr2(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data);
static int __arm_smmu_domain_set_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	int ret = 0;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	unsigned long iommu_attr = (unsigned long)attr;

	switch (iommu_attr) {
	case DOMAIN_ATTR_NESTING:
		if (smmu_domain->smmu) {
			ret = -EPERM;
			goto out;
		}

		if (*(int *)data)
			smmu_domain->stage = ARM_SMMU_DOMAIN_NESTED;
		else
			smmu_domain->stage = ARM_SMMU_DOMAIN_S1;

		break;
	case DOMAIN_ATTR_DMA_USE_FLUSH_QUEUE:
		smmu_domain->non_strict = *(int *)data;
		break;
	case DOMAIN_ATTR_NON_FATAL_FAULTS: {
		u32 non_fatal_faults = *((int *)data);

		if (non_fatal_faults)
			set_bit(DOMAIN_ATTR_NON_FATAL_FAULTS,
				smmu_domain->attributes);
		else
			clear_bit(DOMAIN_ATTR_NON_FATAL_FAULTS,
				  smmu_domain->attributes);
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_S1_BYPASS: {
		int bypass = *((int *)data);

		/* bypass can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		if (bypass)
			set_bit(DOMAIN_ATTR_S1_BYPASS, smmu_domain->attributes);
		else
			clear_bit(DOMAIN_ATTR_S1_BYPASS,
				  smmu_domain->attributes);

		ret = 0;
		break;
	}
	case DOMAIN_ATTR_ATOMIC:
	{
		int atomic_ctx = *((int *)data);

		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		if (atomic_ctx)
			set_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes);
		else
			clear_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes);
		break;
	}
	case DOMAIN_ATTR_SECURE_VMID:
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}

		if (smmu_domain->secure_vmid != VMID_INVAL) {
			ret = -ENODEV;
			WARN(1, "secure vmid already set!");
			break;
		}
		smmu_domain->secure_vmid = *((int *)data);
		break;
		/*
		 * fast_smmu_unmap_page() and fast_smmu_alloc_iova() both
		 * expect that the bus/clock/regulator are already on. Thus also
		 * force DOMAIN_ATTR_ATOMIC to bet set.
		 */
	case DOMAIN_ATTR_FAST:
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}

		if (*((int *)data)) {
			if (IS_ENABLED(CONFIG_IOMMU_IO_PGTABLE_FAST)) {
				set_bit(DOMAIN_ATTR_FAST,
					smmu_domain->attributes);
				set_bit(DOMAIN_ATTR_ATOMIC,
					smmu_domain->attributes);
				ret = 0;
			} else {
				ret = -ENOTSUPP;
			}
		}
		break;
	default:
		ret = __arm_smmu_domain_set_attr2(domain, attr, data);
	}
out:
	return ret;
}

/* yeee-haw */
static int __arm_smmu_domain_set_attr2(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int ret = 0;
	unsigned long iommu_attr = (unsigned long)attr;

	switch (iommu_attr) {
	case DOMAIN_ATTR_USE_UPSTREAM_HINT:
	case DOMAIN_ATTR_USE_LLC_NWA:
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
		} else if (*((int *)data)) {
			set_bit(attr, smmu_domain->attributes);
			ret = 0;
		}
		break;
	case DOMAIN_ATTR_EARLY_MAP: {
		int early_map = *((int *)data);

		ret = 0;
		if (early_map) {
			set_bit(DOMAIN_ATTR_EARLY_MAP, smmu_domain->attributes);
		} else {
			if (smmu_domain->smmu)
				ret = arm_smmu_enable_s1_translations(
								smmu_domain);

			if (!ret)
				clear_bit(DOMAIN_ATTR_EARLY_MAP,
					  smmu_domain->attributes);
		}
		break;
	}
	case DOMAIN_ATTR_FAULT_MODEL_NO_CFRE:
	case DOMAIN_ATTR_FAULT_MODEL_NO_STALL:
	case DOMAIN_ATTR_FAULT_MODEL_HUPCF:
		if (*((int *)data))
			set_bit(attr, smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT: {
		int force_coherent = *((int *)data);

		if (smmu_domain->smmu != NULL) {
			dev_err(smmu_domain->smmu->dev,
			  "cannot change force coherent attribute while attached\n");
			ret = -EBUSY;
		} else if (force_coherent) {
			set_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
				smmu_domain->attributes);
			ret = 0;
		} else {
			clear_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
				  smmu_domain->attributes);
			ret = 0;
		}
		break;
	}
	default:
		ret = -ENODEV;
	}

	return ret;
}

static int arm_smmu_domain_set_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int ret;

	mutex_lock(&smmu_domain->init_mutex);
	ret = __arm_smmu_domain_set_attr(domain, attr, data);
	mutex_unlock(&smmu_domain->init_mutex);

	return ret;
}
static int arm_smmu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	u32 mask, fwid = 0;

	if (args->args_count > 0)
		fwid |= FIELD_PREP(ARM_SMMU_SMR_ID, args->args[0]);

	if (args->args_count > 1)
		fwid |= FIELD_PREP(ARM_SMMU_SMR_MASK, args->args[1]);
	else if (!of_property_read_u32(args->np, "stream-match-mask", &mask))
		fwid |= FIELD_PREP(ARM_SMMU_SMR_MASK, mask);

	return iommu_fwspec_add_ids(dev, &fwid, 1);
}

static void arm_smmu_get_resv_regions(struct device *dev,
				      struct list_head *head)
{
	struct iommu_resv_region *region;
	int prot = IOMMU_WRITE | IOMMU_NOEXEC | IOMMU_MMIO;

	region = iommu_alloc_resv_region(MSI_IOVA_BASE, MSI_IOVA_LENGTH,
					 prot, IOMMU_RESV_SW_MSI);
	if (!region)
		return;

	list_add_tail(&region->list, head);

	iommu_dma_get_resv_regions(dev, head);

	qcom_iommu_generate_resv_regions(dev, head);
}

static int arm_smmu_def_domain_type(struct device *dev)
{
	struct arm_smmu_master_cfg *cfg = dev_iommu_priv_get(dev);
	const struct arm_smmu_impl *impl;

	if (!cfg)
		return 0;

	impl = cfg->smmu->impl;
	if (impl && impl->def_domain_type)
		return impl->def_domain_type(dev);

	return 0;
}

static int arm_smmu_enable_s1_translations(struct arm_smmu_domain *smmu_domain)
{
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = cfg->cbndx;
	struct arm_smmu_cb *cb = &smmu->cbs[idx];
	int ret;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return ret;

	cfg->sctlr.m = 1;
	cb->sctlr = arm_smmu_lpae_sctlr(cfg);

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, cb->sctlr);
	arm_smmu_rpm_put(smmu);
	return ret;
}

static int __arm_smmu_sid_switch(struct device *dev, void *data)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_master_cfg *cfg = dev_iommu_priv_get(dev);
	struct arm_smmu_device *smmu;
	enum sid_switch_direction dir = (typeof(dir))data;
	int i, idx;

	if (!fwspec || !cfg)
		return 0;

	smmu = cfg->smmu;

	arm_smmu_rpm_get(smmu);
	for_each_cfg_sme(cfg, fwspec, i, idx) {
		if (dir == SID_RELEASE) {
			arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(idx), 0);
			arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_S2CR(idx), 0);
			/* Update smr structure to inline with actual operation */
			smmu->smrs[idx].state = SMR_ALLOCATED;
		} else {
			arm_smmu_write_sme(smmu, idx);
		}
	}
	 /* Add barrier to ensure that the SMR register writes is completed. */
	wmb();
	arm_smmu_rpm_put(smmu);
	return 0;
}

/*
 * Some devices support operation with different levels of security. In some
 * modes, HLOS is no longer responsible for managing the S1 translations for
 * a device. Unfortunately, the device may still use the same set of SIDS, so
 * to prevent a potential stream-match conflict fault, HLOS needs to remove
 * the SIDs fromits SMRs. Enforcement of this policy is implemented through
 * virtualization of the SMR/S2CR regigisters.
 */
static int arm_smmu_sid_switch(struct device *dev,
			       enum sid_switch_direction dir)
{
	struct iommu_group *group;
	int ret;

	group = iommu_group_get(dev);
	ret = iommu_group_for_each_dev(group, (void *)dir,
			__arm_smmu_sid_switch);
	iommu_group_put(group);

	return ret;
}

static int arm_smmu_get_asid_nr(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int ret;

	mutex_lock(&smmu_domain->init_mutex);
	if (!smmu_domain->smmu)
		ret = -EINVAL;
	else
		ret = smmu_domain->cfg.asid;
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static struct qcom_iommu_ops arm_smmu_ops = {
	.iova_to_phys_hard = arm_smmu_iova_to_phys_hard,
	.sid_switch		= arm_smmu_sid_switch,
	.get_fault_ids		= arm_smmu_get_fault_ids,
	.get_asid_nr		= arm_smmu_get_asid_nr,

	.iommu_ops = {
		.capable		= arm_smmu_capable,
		.domain_alloc		= arm_smmu_domain_alloc,
		.domain_free		= arm_smmu_domain_free,
		.attach_dev		= arm_smmu_attach_dev,
		.map_pages		= arm_smmu_map_pages,
		.map_sg			= arm_smmu_map_sg,
		.unmap_pages		= arm_smmu_unmap_pages,
		.flush_iotlb_all	= arm_smmu_flush_iotlb_all,
		.iotlb_sync		= arm_smmu_iotlb_sync,
		.iova_to_phys		= arm_smmu_iova_to_phys,
		.probe_device		= arm_smmu_probe_device,
		.release_device		= arm_smmu_release_device,
		.device_group		= arm_smmu_device_group,
		.domain_get_attr	= arm_smmu_domain_get_attr,
		.domain_set_attr	= arm_smmu_domain_set_attr,
		.of_xlate		= arm_smmu_of_xlate,
		.get_resv_regions	= arm_smmu_get_resv_regions,
		.put_resv_regions	= generic_iommu_put_resv_regions,
		.def_domain_type	= arm_smmu_def_domain_type,
		.pgsize_bitmap		= -1UL, /* Restricted during device attach */
	}
};

static void arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	int i;
	u32 reg;

	/* clear global FSR */
	reg = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSR);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sGFSR, reg);

	/*
	 * Reset stream mapping groups: Initial values mark all SMRn as
	 * invalid and all S2CRn as bypass unless overridden.
	 */
	for (i = 0; i < smmu->num_mapping_groups; ++i)
		arm_smmu_write_sme(smmu, i);

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		arm_smmu_write_context_bank(smmu, i);
		arm_smmu_cb_write(smmu, i, ARM_SMMU_CB_FSR, ARM_SMMU_FSR_FAULT);
	}

	/* Invalidate the TLB, just in case */
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIALLH, QCOM_DUMMY_VAL);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIALLNSNH, QCOM_DUMMY_VAL);

	reg = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sCR0);

	/* Enable fault reporting */
	reg |= (ARM_SMMU_sCR0_GFRE | ARM_SMMU_sCR0_GFIE |
		ARM_SMMU_sCR0_GCFGFRE | ARM_SMMU_sCR0_GCFGFIE);

	/* Disable TLB broadcasting. */
	reg |= (ARM_SMMU_sCR0_VMIDPNE | ARM_SMMU_sCR0_PTM);

	/* Enable client access, handling unmatched streams as appropriate */
	reg &= ~ARM_SMMU_sCR0_CLIENTPD;
	if (disable_bypass)
		reg |= ARM_SMMU_sCR0_USFCFG;
	else
		reg &= ~ARM_SMMU_sCR0_USFCFG;

	/* Disable forced broadcasting */
	reg &= ~ARM_SMMU_sCR0_FB;

	/* Don't upgrade barriers */
	reg &= ~(ARM_SMMU_sCR0_BSU);

	if (smmu->features & ARM_SMMU_FEAT_VMID16)
		reg |= ARM_SMMU_sCR0_VMID16EN;

	if (smmu->features & ARM_SMMU_FEAT_EXIDS)
		reg |= ARM_SMMU_sCR0_EXIDENABLE;

	/* Force bypass transaction to be Non-Shareable & not io-coherent */
	reg &= ~ARM_SMMU_sCR0_SHCFG;
	reg |= FIELD_PREP(ARM_SMMU_sCR0_SHCFG, ARM_SMMU_sCR0_SHCFG_NSH);

	if (smmu->impl && smmu->impl->reset)
		smmu->impl->reset(smmu);

	/* Push the button */
	arm_smmu_tlb_sync_global(smmu);
	wmb();
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sCR0, reg);
}

static int arm_smmu_id_size_to_bits(int size)
{
	switch (size) {
	case 0:
		return 32;
	case 1:
		return 36;
	case 2:
		return 40;
	case 3:
		return 42;
	case 4:
		return 44;
	case 5:
	default:
		return 48;
	}
}

static int arm_smmu_handoff_cbs(struct arm_smmu_device *smmu)
{
	u32 i, smr, s2cr;
	u32 index;
	struct arm_smmu_smr	smrs;
	struct arm_smmu_smr	*handoff_smrs;
	int num_handoff_smrs;
	const __be32 *cell;

	cell = of_get_property(smmu->dev->of_node, "qcom,handoff-smrs", NULL);
	if (!cell)
		return 0;

	num_handoff_smrs = of_property_count_elems_of_size(smmu->dev->of_node,
					"qcom,handoff-smrs", sizeof(u32) * 2);
	if (num_handoff_smrs < 0)
		return 0;

	handoff_smrs = kcalloc(num_handoff_smrs, sizeof(*handoff_smrs),
			       GFP_KERNEL);
	if (!handoff_smrs)
		return -ENOMEM;

	for (i = 0; i < num_handoff_smrs; i++) {
		handoff_smrs[i].id = of_read_number(cell++, 1);
		handoff_smrs[i].mask = of_read_number(cell++, 1);
		handoff_smrs[i].valid = true;
	}


	for (i = 0; i < smmu->num_mapping_groups; ++i) {

		smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(i));

		if (smmu->features & ARM_SMMU_FEAT_EXIDS) {
			s2cr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_S2CR(i));
			smrs.valid = FIELD_GET(ARM_SMMU_S2CR_EXIDVALID, s2cr);
			if (!smrs.valid)
				continue;

			smrs.id = FIELD_GET(ARM_SMMU_SMR_ID, smr);
			smrs.mask = FIELD_GET(ARM_SMMU_SMR_MASK, smr);
			smrs.state = SMR_PROGRAMMED;

		} else {
			smrs.valid = FIELD_GET(ARM_SMMU_SMR_VALID, smr);
			if (!smrs.valid)
				continue;

			smrs.id = FIELD_GET(ARM_SMMU_SMR_ID, smr);
			/*
			 * The SMR mask covers bits 30:16 when extended stream
			 * matching is not enabled.
			 */
			smrs.mask = FIELD_GET(ARM_SMMU_SMR_MASK,
					      smr & ~ARM_SMMU_SMR_VALID);
			smrs.state = SMR_PROGRAMMED;
		}

		for (index = 0; index < num_handoff_smrs; index++) {

			if (!handoff_smrs[index].valid)
				continue;

			if ((handoff_smrs[index].mask & smrs.mask) == handoff_smrs[index].mask &&
			    !((handoff_smrs[index].id ^ smrs.id) & ~smrs.mask)) {

				dev_dbg(smmu->dev,
					"handoff-smrs match idx %d, id, 0x%x, mask 0x%x\n",
					i, smrs.id, smrs.mask);

				s2cr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_S2CR(i));

				smmu->smrs[i] = smrs;

				smmu->s2crs[i].group = NULL;
				smmu->s2crs[i].count = 0;
				smmu->s2crs[i].type = FIELD_GET(ARM_SMMU_S2CR_TYPE, s2cr);
				smmu->s2crs[i].privcfg = FIELD_GET(ARM_SMMU_S2CR_PRIVCFG, s2cr);
				smmu->s2crs[i].cbndx = FIELD_GET(ARM_SMMU_S2CR_CBNDX, s2cr);

				smmu->s2crs[i].pinned = true;
				bitmap_set(smmu->context_map, smmu->s2crs[i].cbndx, 1);
				handoff_smrs[index].valid = false;

				break;

			} else {
				dev_dbg(smmu->dev,
					"handoff-smrs no match idx %d, id, 0x%x, mask 0x%x\n",
					i, smrs.id, smrs.mask);
			}
		}
	}

	kfree(handoff_smrs);

	return 0;
}

static int arm_smmu_parse_impl_def_registers(struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;
	int i, ntuples, ret;
	u32 *tuples;
	struct arm_smmu_impl_def_reg *regs, *regit;

	if (!of_find_property(dev->of_node, "attach-impl-defs", &ntuples))
		return 0;

	ntuples /= sizeof(u32);
	if (ntuples % 2) {
		dev_err(dev,
			"Invalid number of attach-impl-defs registers: %d\n",
			ntuples);
		return -EINVAL;
	}

	regs = devm_kmalloc(
		dev, sizeof(*smmu->impl_def_attach_registers) * ntuples,
		GFP_KERNEL);
	if (!regs)
		return -ENOMEM;

	tuples = devm_kmalloc(dev, sizeof(u32) * ntuples * 2, GFP_KERNEL);
	if (!tuples)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, "attach-impl-defs",
					tuples, ntuples);
	if (ret)
		return ret;

	for (i = 0, regit = regs; i < ntuples; i += 2, ++regit) {
		regit->offset = tuples[i];
		regit->value = tuples[i + 1];
	}

	devm_kfree(dev, tuples);

	smmu->impl_def_attach_registers = regs;
	smmu->num_impl_def_attach_registers = ntuples / 2;

	return 0;
}

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	unsigned int size;
	u32 id;
	bool cttw_reg, cttw_fw = smmu->features & ARM_SMMU_FEAT_COHERENT_WALK;
	int i, ret;
	unsigned int num_mapping_groups_override = 0;
	unsigned int num_context_banks_override = 0;

	dev_dbg(smmu->dev, "probing hardware configuration...\n");
	dev_dbg(smmu->dev, "SMMUv%d with:\n",
			smmu->version == ARM_SMMU_V2 ? 2 : 1);

	/* ID0 */
	id = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_ID0);

	/* Restrict available stages based on module parameter */
	if (force_stage == 1)
		id &= ~(ARM_SMMU_ID0_S2TS | ARM_SMMU_ID0_NTS);
	else if (force_stage == 2)
		id &= ~(ARM_SMMU_ID0_S1TS | ARM_SMMU_ID0_NTS);

	if (id & ARM_SMMU_ID0_S1TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S1;
		dev_dbg(smmu->dev, "\tstage 1 translation\n");
	}

	if (id & ARM_SMMU_ID0_S2TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S2;
		dev_dbg(smmu->dev, "\tstage 2 translation\n");
	}

	if (id & ARM_SMMU_ID0_NTS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_NESTED;
		dev_dbg(smmu->dev, "\tnested translation\n");
	}

	if (!(smmu->features &
		(ARM_SMMU_FEAT_TRANS_S1 | ARM_SMMU_FEAT_TRANS_S2))) {
		dev_err(smmu->dev, "\tno translation support!\n");
		return -ENODEV;
	}

	if ((id & ARM_SMMU_ID0_S1TS) &&
	    ((smmu->version < ARM_SMMU_V2) || !(id & ARM_SMMU_ID0_ATOSNS))) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_OPS;
		dev_dbg(smmu->dev, "\taddress translation ops\n");
	}

	/*
	 * In order for DMA API calls to work properly, we must defer to what
	 * the FW says about coherency, regardless of what the hardware claims.
	 * Fortunately, this also opens up a workaround for systems where the
	 * ID register value has ended up configured incorrectly.
	 */
	cttw_reg = !!(id & ARM_SMMU_ID0_CTTW);
	if (cttw_fw || cttw_reg)
		dev_notice(smmu->dev, "\t%scoherent table walk\n",
			   cttw_fw ? "" : "non-");
	if (cttw_fw != cttw_reg)
		dev_notice(smmu->dev,
			   "\t(IDR0.CTTW overridden by FW configuration)\n");

	/* Max. number of entries we have for stream matching/indexing */
	if (smmu->version == ARM_SMMU_V2 && id & ARM_SMMU_ID0_EXIDS) {
		smmu->features |= ARM_SMMU_FEAT_EXIDS;
		size = 1 << 16;
	} else {
		size = 1 << FIELD_GET(ARM_SMMU_ID0_NUMSIDB, id);
	}
	smmu->streamid_mask = size - 1;
	if (id & ARM_SMMU_ID0_SMS) {
		smmu->features |= ARM_SMMU_FEAT_STREAM_MATCH;
		size = FIELD_GET(ARM_SMMU_ID0_NUMSMRG, id);
		if (size == 0) {
			dev_err(smmu->dev,
				"stream-matching supported, but no SMRs present!\n");
			return -ENODEV;
		}

		/* Zero-initialised to mark as invalid */
		smmu->smrs = devm_kcalloc(smmu->dev, size, sizeof(*smmu->smrs),
					  GFP_KERNEL);
		if (!smmu->smrs)
			return -ENOMEM;

		dev_notice(smmu->dev,
			   "\tstream matching with %u register groups", size);
	}
	/* s2cr->type == 0 means translation, so initialise explicitly */
	smmu->s2crs = devm_kmalloc_array(smmu->dev, size, sizeof(*smmu->s2crs),
					 GFP_KERNEL);
	if (!smmu->s2crs)
		return -ENOMEM;
	for (i = 0; i < size; i++)
		smmu->s2crs[i] = s2cr_init_val;

	ret = of_property_read_u32(smmu->dev->of_node, "qcom,num-smr-override",
		&num_mapping_groups_override);
	if (!ret && size > num_mapping_groups_override) {
		dev_dbg(smmu->dev, "%d mapping groups overridden to %d\n",
			size, num_mapping_groups_override);

		size = min(size, num_mapping_groups_override);
	}

	smmu->num_mapping_groups = size;
	mutex_init(&smmu->stream_map_mutex);
	mutex_init(&smmu->iommu_group_mutex);
	spin_lock_init(&smmu->global_sync_lock);

	if (smmu->version < ARM_SMMU_V2 ||
	    !(id & ARM_SMMU_ID0_PTFS_NO_AARCH32)) {
		smmu->features |= ARM_SMMU_FEAT_FMT_AARCH32_L;
		if (!(id & ARM_SMMU_ID0_PTFS_NO_AARCH32S))
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH32_S;
	}

	/* ID1 */
	id = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ARM_SMMU_ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (FIELD_GET(ARM_SMMU_ID1_NUMPAGENDXB, id) + 1);
	if (smmu->numpage != 2 * size << smmu->pgshift)
		dev_warn(smmu->dev,
			"SMMU address space size (0x%x) differs from mapped region size (0x%x)!\n",
			2 * size << smmu->pgshift, smmu->numpage);
	/* Now properly encode NUMPAGE to subsequently derive SMMU_CB_BASE */
	smmu->numpage = size;

	smmu->num_s2_context_banks = FIELD_GET(ARM_SMMU_ID1_NUMS2CB, id);
	smmu->num_context_banks = FIELD_GET(ARM_SMMU_ID1_NUMCB, id);

	ret = of_property_read_u32(smmu->dev->of_node,
		"qcom,num-context-banks-override",
		&num_context_banks_override);

	if (!ret && smmu->num_context_banks > num_context_banks_override) {
		dev_dbg(smmu->dev, "%d context banks overridden to %d\n",
			smmu->num_context_banks,
			num_context_banks_override);

		smmu->num_context_banks = min(smmu->num_context_banks,
					num_context_banks_override);
	}

	if (smmu->num_s2_context_banks > smmu->num_context_banks) {
		dev_err(smmu->dev, "impossible number of S2 context banks!\n");
		return -ENODEV;
	}
	dev_dbg(smmu->dev, "\t%u context banks (%u stage-2 only)\n",
		   smmu->num_context_banks, smmu->num_s2_context_banks);
	smmu->cbs = devm_kcalloc(smmu->dev, smmu->num_context_banks,
				 sizeof(*smmu->cbs), GFP_KERNEL);
	if (!smmu->cbs)
		return -ENOMEM;

	/* ID2 */
	id = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits(FIELD_GET(ARM_SMMU_ID2_IAS, id));
	smmu->ipa_size = size;

	/* The output mask is also applied for bypass */
	size = arm_smmu_id_size_to_bits(FIELD_GET(ARM_SMMU_ID2_OAS, id));
	smmu->pa_size = size;

	if (id & ARM_SMMU_ID2_VMID16)
		smmu->features |= ARM_SMMU_FEAT_VMID16;

	/*
	 * What the page table walker can address actually depends on which
	 * descriptor format is in use, but since a) we don't know that yet,
	 * and b) it can vary per context bank, this will have to do...
	 */
	if (dma_set_mask_and_coherent(smmu->dev, DMA_BIT_MASK(size)))
		dev_warn(smmu->dev,
			 "failed to set DMA mask for table walker\n");

	if (smmu->version < ARM_SMMU_V2) {
		smmu->va_size = smmu->ipa_size;
		if (smmu->version == ARM_SMMU_V1_64K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_64K;
	} else {
		size = FIELD_GET(ARM_SMMU_ID2_UBS, id);
		smmu->va_size = arm_smmu_id_size_to_bits(size);
		if (id & ARM_SMMU_ID2_PTFS_4K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_4K;
		if (id & ARM_SMMU_ID2_PTFS_16K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_16K;
		if (id & ARM_SMMU_ID2_PTFS_64K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_64K;
	}

	if (smmu->impl && smmu->impl->cfg_probe) {
		ret = smmu->impl->cfg_probe(smmu);
		if (ret)
			return ret;
	}

	/* Now we've corralled the various formats, what'll it do? */
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_S)
		smmu->pgsize_bitmap |= SZ_4K | SZ_64K | SZ_1M | SZ_16M;
	if (smmu->features &
	    (ARM_SMMU_FEAT_FMT_AARCH32_L | ARM_SMMU_FEAT_FMT_AARCH64_4K))
		smmu->pgsize_bitmap |= SZ_4K | SZ_2M | SZ_1G;
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH64_16K)
		smmu->pgsize_bitmap |= SZ_16K | SZ_32M;
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH64_64K)
		smmu->pgsize_bitmap |= SZ_64K | SZ_512M;

	if (arm_smmu_ops.iommu_ops.pgsize_bitmap == -1UL)
		arm_smmu_ops.iommu_ops.pgsize_bitmap = smmu->pgsize_bitmap;
	else
		arm_smmu_ops.iommu_ops.pgsize_bitmap |= smmu->pgsize_bitmap;
	dev_dbg(smmu->dev, "\tSupported page sizes: 0x%08lx\n",
		   smmu->pgsize_bitmap);


	if (smmu->features & ARM_SMMU_FEAT_TRANS_S1)
		dev_dbg(smmu->dev, "\tStage-1: %lu-bit VA -> %lu-bit IPA\n",
			smmu->va_size, smmu->ipa_size);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S2)
		dev_dbg(smmu->dev, "\tStage-2: %lu-bit IPA -> %lu-bit PA\n",
			smmu->ipa_size, smmu->pa_size);

	return 0;
}

struct arm_smmu_match_data {
	enum arm_smmu_arch_version version;
	enum arm_smmu_implementation model;
};

#define ARM_SMMU_MATCH_DATA(name, ver, imp)	\
static const struct arm_smmu_match_data name = { .version = ver, .model = imp }

ARM_SMMU_MATCH_DATA(smmu_generic_v1, ARM_SMMU_V1, GENERIC_SMMU);
ARM_SMMU_MATCH_DATA(smmu_generic_v2, ARM_SMMU_V2, GENERIC_SMMU);
ARM_SMMU_MATCH_DATA(arm_mmu401, ARM_SMMU_V1_64K, GENERIC_SMMU);
ARM_SMMU_MATCH_DATA(arm_mmu500, ARM_SMMU_V2, ARM_MMU500);
ARM_SMMU_MATCH_DATA(cavium_smmuv2, ARM_SMMU_V2, CAVIUM_SMMUV2);
ARM_SMMU_MATCH_DATA(qcom_smmuv500, ARM_SMMU_V2, QCOM_SMMUV500);
ARM_SMMU_MATCH_DATA(qcom_smmuv2, ARM_SMMU_V2, QCOM_SMMUV2);

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "arm,smmu-v1", .data = &smmu_generic_v1 },
	{ .compatible = "arm,smmu-v2", .data = &smmu_generic_v2 },
	{ .compatible = "qcom,qsmmu-v500", .data = &qcom_smmuv500 },
	{ .compatible = "arm,mmu-400", .data = &smmu_generic_v1 },
	{ .compatible = "arm,mmu-401", .data = &arm_mmu401 },
	{ .compatible = "arm,mmu-500", .data = &arm_mmu500 },
	{ .compatible = "cavium,smmu-v2", .data = &cavium_smmuv2 },
	{ .compatible = "nvidia,smmu-500", .data = &arm_mmu500 },
	{ .compatible = "qcom,smmu-v2", .data = &qcom_smmuv2 },
	{ },
};
MODULE_DEVICE_TABLE(of, arm_smmu_of_match);

#ifdef CONFIG_ACPI
static int acpi_smmu_get_data(u32 model, struct arm_smmu_device *smmu)
{
	int ret = 0;

	switch (model) {
	case ACPI_IORT_SMMU_V1:
	case ACPI_IORT_SMMU_CORELINK_MMU400:
		smmu->version = ARM_SMMU_V1;
		smmu->model = GENERIC_SMMU;
		break;
	case ACPI_IORT_SMMU_CORELINK_MMU401:
		smmu->version = ARM_SMMU_V1_64K;
		smmu->model = GENERIC_SMMU;
		break;
	case ACPI_IORT_SMMU_V2:
		smmu->version = ARM_SMMU_V2;
		smmu->model = GENERIC_SMMU;
		break;
	case ACPI_IORT_SMMU_CORELINK_MMU500:
		smmu->version = ARM_SMMU_V2;
		smmu->model = ARM_MMU500;
		break;
	case ACPI_IORT_SMMU_CAVIUM_THUNDERX:
		smmu->version = ARM_SMMU_V2;
		smmu->model = CAVIUM_SMMUV2;
		break;
	default:
		ret = -ENODEV;
	}

	return ret;
}

static int arm_smmu_device_acpi_probe(struct platform_device *pdev,
				      struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;
	struct acpi_iort_node *node =
		*(struct acpi_iort_node **)dev_get_platdata(dev);
	struct acpi_iort_smmu *iort_smmu;
	int ret;

	/* Retrieve SMMU1/2 specific data */
	iort_smmu = (struct acpi_iort_smmu *)node->node_data;

	ret = acpi_smmu_get_data(iort_smmu->model, smmu);
	if (ret < 0)
		return ret;

	/* Ignore the configuration access interrupt */
	smmu->num_global_irqs = 1;

	if (iort_smmu->flags & ACPI_IORT_SMMU_COHERENT_WALK)
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;

	return 0;
}
#else
static inline int arm_smmu_device_acpi_probe(struct platform_device *pdev,
					     struct arm_smmu_device *smmu)
{
	return -ENODEV;
}
#endif


static int arm_smmu_device_dt_probe(struct platform_device *pdev,
				    struct arm_smmu_device *smmu)
{
	const struct arm_smmu_match_data *data;
	struct device *dev = &pdev->dev;
	bool legacy_binding;

	if (of_property_read_u32(dev->of_node, "#global-interrupts",
				 &smmu->num_global_irqs)) {
		dev_err(dev, "missing #global-interrupts property\n");
		return -ENODEV;
	}

	data = of_device_get_match_data(dev);
	smmu->version = data->version;
	smmu->model = data->model;

	legacy_binding = of_find_property(dev->of_node, "mmu-masters", NULL);
	if (legacy_binding && !using_generic_binding) {
		if (!using_legacy_binding) {
			pr_notice("deprecated \"mmu-masters\" DT property in use; %s support unavailable\n",
				IS_ENABLED(CONFIG_ARM_SMMU_LEGACY_DT_BINDINGS) ?
				"DMA API" : "SMMU");
		}
		using_legacy_binding = true;
	} else if (!legacy_binding && !using_legacy_binding) {
		using_generic_binding = true;
	} else {
		dev_err(dev, "not probing due to mismatched DT properties\n");
		return -ENODEV;
	}

	if (of_dma_is_coherent(dev->of_node))
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;

	return 0;
}


static int arm_smmu_bus_init(struct iommu_ops *ops)
{
	int err;

	/* Oh, for a proper bus abstraction */
	if (!iommu_present(&platform_bus_type)) {
		err = bus_set_iommu(&platform_bus_type, ops);
		if (err)
			return err;
	}
#ifdef CONFIG_ARM_AMBA
	if (!iommu_present(&amba_bustype)) {
		err = bus_set_iommu(&amba_bustype, ops);
		if (err)
			goto err_reset_platform_ops;
	}
#endif
#ifdef CONFIG_PCI
	if (!iommu_present(&pci_bus_type)) {
		err = bus_set_iommu(&pci_bus_type, ops);
		if (err)
			goto err_reset_amba_ops;
	}
#endif
#ifdef CONFIG_FSL_MC_BUS
	if (!iommu_present(&fsl_mc_bus_type)) {
		err = bus_set_iommu(&fsl_mc_bus_type, ops);
		if (err)
			goto err_reset_pci_ops;
	}
#endif
	return 0;

err_reset_pci_ops: __maybe_unused;
#ifdef CONFIG_PCI
	bus_set_iommu(&pci_bus_type, NULL);
#endif
err_reset_amba_ops: __maybe_unused;
#ifdef CONFIG_ARM_AMBA
	bus_set_iommu(&amba_bustype, NULL);
#endif
err_reset_platform_ops: __maybe_unused;
	bus_set_iommu(&platform_bus_type, NULL);
	return err;
}

static int arm_smmu_device_probe(struct platform_device *pdev)
{
	struct resource *res;
	resource_size_t ioaddr;
	struct arm_smmu_device *smmu;
	struct device *dev = &pdev->dev;
	int num_irqs, i, err;
	irqreturn_t (*global_fault)(int irq, void *dev);

	/* We depend on this device for fastmap */
	if (!qcom_dma_iommu_is_ready())
		return -EPROBE_DEFER;

	smmu = devm_kzalloc(dev, sizeof(*smmu), GFP_KERNEL);
	if (!smmu)
		return -ENOMEM;
	smmu->dev = dev;

	if (dev->of_node)
		err = arm_smmu_device_dt_probe(pdev, smmu);
	else
		err = arm_smmu_device_acpi_probe(pdev, smmu);

	if (err)
		return err;

	smmu->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(smmu->base))
		return PTR_ERR(smmu->base);
	ioaddr = res->start;
	/*
	 * The resource size should effectively match the value of SMMU_TOP;
	 * stash that temporarily until we know PAGESIZE to validate it with.
	 */
	smmu->numpage = resource_size(res);

	smmu = arm_smmu_impl_init(smmu);
	if (IS_ERR(smmu))
		return PTR_ERR(smmu);

	num_irqs = 0;
	while ((res = platform_get_resource(pdev, IORESOURCE_IRQ, num_irqs))) {
		num_irqs++;
		if (num_irqs > smmu->num_global_irqs)
			smmu->num_context_irqs++;
	}

	if (!smmu->num_context_irqs) {
		dev_err(dev, "found %d interrupts but expected at least %d\n",
			num_irqs, smmu->num_global_irqs + 1);
		return -ENODEV;
	}

	smmu->irqs = devm_kcalloc(dev, num_irqs, sizeof(*smmu->irqs),
				  GFP_KERNEL);
	if (!smmu->irqs) {
		dev_err(dev, "failed to allocate %d irqs\n", num_irqs);
		return -ENOMEM;
	}

	for (i = 0; i < num_irqs; ++i) {
		int irq = platform_get_irq(pdev, i);

		if (irq < 0) {
			dev_err(dev, "failed to get irq index %d\n", i);
			return -ENODEV;
		}
		smmu->irqs[i] = irq;
	}

	smmu->pwr = arm_smmu_init_power_resources(dev);
	if (IS_ERR(smmu->pwr))
		return PTR_ERR(smmu->pwr);

	/*
	 * We can't use arm_smmu_rpm_get() because pm-runtime isn't
	 * enabled yet.
	 */
	err = arm_smmu_power_on(smmu->pwr);
	if (err)
		return err;

	err = arm_smmu_device_cfg_probe(smmu);
	if (err)
		goto out_power_off;

	err = arm_smmu_parse_impl_def_registers(smmu);
	if (err)
		goto out_power_off;

	if (smmu->version == ARM_SMMU_V2) {
		if (smmu->num_context_banks > smmu->num_context_irqs) {
			dev_err(dev,
			      "found only %d context irq(s) but %d required\n",
			      smmu->num_context_irqs, smmu->num_context_banks);
			err = -ENODEV;
			goto out_power_off;
		}

		/* Ignore superfluous interrupts */
		smmu->num_context_irqs = smmu->num_context_banks;
	}

	if (smmu->impl && smmu->impl->global_fault)
		global_fault = smmu->impl->global_fault;
	else
		global_fault = arm_smmu_global_fault;

	for (i = 0; i < smmu->num_global_irqs; ++i) {
		err = devm_request_threaded_irq(smmu->dev, smmu->irqs[i],
				       NULL,
				       global_fault,
				       IRQF_ONESHOT | IRQF_SHARED,
				       "arm-smmu global fault",
				       smmu);
		if (err) {
			dev_err(dev, "failed to request global IRQ %d (%u)\n",
				i, smmu->irqs[i]);
			goto out_power_off;
		}
	}

	/* QCOM Additions */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	smmu->phys_addr = res->start;
	parse_driver_options(smmu);
	err = arm_smmu_handoff_cbs(smmu);
	if (err)
		goto out_power_off;

	err = iommu_device_sysfs_add(&smmu->iommu, smmu->dev, NULL,
				     "smmu.%pa", &ioaddr);
	if (err) {
		dev_err(dev, "Failed to register iommu in sysfs\n");
		goto out_power_off;
	}

	iommu_device_set_ops(&smmu->iommu, &arm_smmu_ops.iommu_ops);
	iommu_device_set_fwnode(&smmu->iommu, dev->fwnode);

	err = iommu_device_register(&smmu->iommu);
	if (err) {
		dev_err(dev, "Failed to register iommu\n");
		goto out_power_off;
	}

	platform_set_drvdata(pdev, smmu);
	arm_smmu_device_reset(smmu);
	arm_smmu_test_smr_masks(smmu);
	arm_smmu_interrupt_selftest(smmu);

	/*
	 * We want to avoid touching dev->power.lock in fastpaths unless
	 * it's really going to do something useful - pm_runtime_enabled()
	 * can serve as an ideal proxy for that decision. So, conditionally
	 * enable pm_runtime.
	 */
	/*
	 * QCOM's nonupstream gdsc driver doesn't support pm_domains.
	 * So check for presence of gdsc instead.
	 */
	if (smmu->pwr->num_gdscs) {
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
	}

	/*
	 * For ACPI and generic DT bindings, an SMMU will be probed before
	 * any device which might need it, so we want the bus ops in place
	 * ready to handle default domain setup as soon as any SMMU exists.
	 */
	if (!using_legacy_binding)
		return arm_smmu_bus_init(&arm_smmu_ops.iommu_ops);

	return 0;

out_power_off:
	arm_smmu_power_off(smmu, smmu->pwr);

	return err;
}

static int arm_smmu_device_remove(struct platform_device *pdev)
{
	struct arm_smmu_device *smmu = platform_get_drvdata(pdev);

	if (!smmu)
		return -ENODEV;

	if (!bitmap_empty(smmu->context_map, ARM_SMMU_MAX_CBS))
		dev_notice(&pdev->dev, "disabling translation\n");

	arm_smmu_bus_init(NULL);
	iommu_device_unregister(&smmu->iommu);
	iommu_device_sysfs_remove(&smmu->iommu);

	if (smmu->impl && smmu->impl->device_remove)
		smmu->impl->device_remove(smmu);

	arm_smmu_rpm_get(smmu);
	/* Turn the thing off */
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sCR0, ARM_SMMU_sCR0_CLIENTPD);
	arm_smmu_rpm_put(smmu);

	if (smmu->atomic_pwr_refcount > 0) {
		if (pm_runtime_enabled(smmu->dev))
			pm_runtime_force_suspend(smmu->dev);
		else
			arm_smmu_power_off(smmu, smmu->pwr);
	}

	return 0;
}

static void arm_smmu_device_shutdown(struct platform_device *pdev)
{
	arm_smmu_device_remove(pdev);
}

static int __maybe_unused arm_smmu_runtime_resume(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	int ret;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return ret;

	return 0;
}

static int __maybe_unused arm_smmu_runtime_suspend(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);

	arm_smmu_power_off(smmu, smmu->pwr);
	return 0;
}

static int __maybe_unused arm_smmu_pm_resume(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = arm_smmu_runtime_resume(dev);
	if (ret)
		return ret;

	/*
	 * QCOM HW supports register retention. So we really only need to
	 * re-program the registers for hibernation. Don't do this during
	 * runtime_resume to avoid latency.
	 */
	arm_smmu_device_reset(smmu);
	return 0;
}

static int __maybe_unused arm_smmu_pm_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return arm_smmu_runtime_suspend(dev);
}

static int arm_smmu_pm_prepare(struct device *dev)
{
	if (!of_device_is_compatible(dev->of_node, "qcom,adreno-smmu"))
		return 0;

	/*
	 * In case of GFX smmu, race between rpm_suspend and system suspend could
	 * cause a deadlock where cx vote is never put down causing timeout. So,
	 * abort system suspend here if dev->power.usage_count is 1 as this indicates
	 * rpm_suspend is in progress and prepare is the one incrementing this counter.
	 * Now rpm_suspend can continue and put down cx vote. System suspend will resume
	 * later and complete.
	 */
	if (pm_runtime_suspended(dev))
		return 0;

	return (atomic_read(&dev->power.usage_count) == 1) ? -EINPROGRESS : 0;
}

static const struct dev_pm_ops arm_smmu_pm_ops = {
	.prepare = arm_smmu_pm_prepare,
	SET_SYSTEM_SLEEP_PM_OPS(arm_smmu_pm_suspend, arm_smmu_pm_resume)
	SET_RUNTIME_PM_OPS(arm_smmu_runtime_suspend,
			   arm_smmu_runtime_resume, NULL)
};


static struct platform_driver arm_smmu_driver = {
	.driver	= {
		.name			= "arm-smmu",
		.of_match_table		= arm_smmu_of_match,
		.pm			= &arm_smmu_pm_ops,
		.suppress_bind_attrs    = true,
	},
	.probe	= arm_smmu_device_probe,
	.remove	= arm_smmu_device_remove,
	.shutdown = arm_smmu_device_shutdown,
};

static int __init arm_smmu_init(void)
{
	int ret;
	ktime_t cur;

	cur = ktime_get();
	ret = platform_driver_register(&qsmmuv500_tbu_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&arm_smmu_driver);
	if (ret) {
		platform_driver_unregister(&qsmmuv500_tbu_driver);
		return ret;
	}

	trace_smmu_init(ktime_us_delta(ktime_get(), cur));
	return ret;
}
subsys_initcall(arm_smmu_init);

static void __exit arm_smmu_exit(void)
{
	platform_driver_unregister(&arm_smmu_driver);
	platform_driver_unregister(&qsmmuv500_tbu_driver);
}
module_exit(arm_smmu_exit);

MODULE_DESCRIPTION("IOMMU API for ARM architected SMMU implementations");
MODULE_AUTHOR("Will Deacon <will@kernel.org>");
MODULE_LICENSE("GPL v2");
