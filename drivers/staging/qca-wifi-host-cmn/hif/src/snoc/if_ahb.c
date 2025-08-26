/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: if_ahb.c
 *
 * c file for ahb specific implementations.
 */

#include "hif.h"
#include "target_type.h"
#include "hif_main.h"
#include "hif_debug.h"
#include "hif_io32.h"
#include "ce_main.h"
#include "ce_api.h"
#include "ce_tasklet.h"
#include "if_ahb.h"
#include "if_pci.h"
#include "ahb_api.h"
#include "pci_api.h"
#include "hif_napi.h"
#include "qal_vbus_dev.h"
#include "qdf_irq.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
#define IRQF_DISABLED 0x00000020
#endif

#define HIF_IC_CE0_IRQ_OFFSET 4
#define HIF_IC_MAX_IRQ 58

static uint16_t ic_irqnum[HIF_IC_MAX_IRQ];
/* integrated chip irq names */
const char *ic_irqname[HIF_IC_MAX_IRQ] = {
"misc-pulse1",
"misc-latch",
"sw-exception",
"watchdog",
"ce0",
"ce1",
"ce2",
"ce3",
"ce4",
"ce5",
"ce6",
"ce7",
"ce8",
"ce9",
"ce10",
"ce11",
"host2wbm-desc-feed",
"host2reo-re-injection",
"host2reo-command",
"host2rxdma-monitor-ring3",
"host2rxdma-monitor-ring2",
"host2rxdma-monitor-ring1",
"reo2ost-exception",
"wbm2host-rx-release",
"reo2host-status",
"reo2host-destination-ring4",
"reo2host-destination-ring3",
"reo2host-destination-ring2",
"reo2host-destination-ring1",
"rxdma2host-monitor-destination-mac3",
"rxdma2host-monitor-destination-mac2",
"rxdma2host-monitor-destination-mac1",
"ppdu-end-interrupts-mac3",
"ppdu-end-interrupts-mac2",
"ppdu-end-interrupts-mac1",
"rxdma2host-monitor-status-ring-mac3",
"rxdma2host-monitor-status-ring-mac2",
"rxdma2host-monitor-status-ring-mac1",
"host2rxdma-host-buf-ring-mac3",
"host2rxdma-host-buf-ring-mac2",
"host2rxdma-host-buf-ring-mac1",
"rxdma2host-destination-ring-mac3",
"rxdma2host-destination-ring-mac2",
"rxdma2host-destination-ring-mac1",
"host2tcl-input-ring4",
"host2tcl-input-ring3",
"host2tcl-input-ring2",
"host2tcl-input-ring1",
"wbm2host-tx-completions-ring4",
"wbm2host-tx-completions-ring3",
"wbm2host-tx-completions-ring2",
"wbm2host-tx-completions-ring1",
"tcl2host-status-ring",
"txmon2host-monitor-destination-mac3",
"txmon2host-monitor-destination-mac2",
"txmon2host-monitor-destination-mac1",
"host2tx-monitor-ring1",
"umac_reset"
};

/**
 * hif_ahb_get_irq_name() - get irqname
 * @irq_no: irq number
 *
 * This function gives irqnumber to irqname
 * mapping.
 *
 * Return: irq name
 */
const char *hif_ahb_get_irq_name(int irq_no)
{
	return ic_irqname[irq_no];
}

/**
 * hif_ahb_disable_isr() - disable isr
 * @scn: struct hif_softc
 *
 * This function disables isr and kills tasklets
 *
 * Return: void
 */
void hif_ahb_disable_isr(struct hif_softc *scn)
{
	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(scn);
	hif_exec_kill(&scn->osc);
	hif_nointrs(scn);
	ce_tasklet_kill(scn);
	tasklet_kill(&sc->intr_tq);
	qdf_atomic_set(&scn->active_tasklet_cnt, 0);
	qdf_atomic_set(&scn->active_grp_tasklet_cnt, 0);
}

/**
 * hif_ahb_dump_registers() - dump bus debug registers
 * @hif_ctx: struct hif_opaque_softc
 *
 * This function dumps hif bus debug registers
 *
 * Return: 0 for success or error code
 */
int hif_ahb_dump_registers(struct hif_softc *hif_ctx)
{
	int status;
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);

	status = hif_dump_ce_registers(scn);
	if (status)
		hif_err("Dump CE Registers Failed status %d", status);

	return 0;
}

/**
 * hif_ahb_close() - hif_bus_close
 * @scn: pointer to the hif context.
 *
 * This is a callback function for hif_bus_close.
 *
 *
 * Return: n/a
 */
void hif_ahb_close(struct hif_softc *scn)
{
	hif_ce_close(scn);
}

/**
 * hif_ahb_open() - hif_ahb open
 * @hif_ctx: hif context
 * @bus_type: bus type
 *
 * This is a callback function for hif_bus_open.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hif_ahb_open(struct hif_softc *hif_ctx, enum qdf_bus_type bus_type)
{

	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(hif_ctx);

	qdf_spinlock_create(&sc->irq_lock);
	return hif_ce_open(hif_ctx);
}

/**
 * hif_ahb_bus_configure() - Configure the bus
 * @scn: pointer to the hif context.
 *
 * This function configure the ahb bus
 *
 * Return: 0 for success. nonzero for failure.
 */
int hif_ahb_bus_configure(struct hif_softc *scn)
{
	return hif_pci_bus_configure(scn);
}

static void hif_ahb_get_soc_info_pld(struct hif_pci_softc *sc,
				     struct device *dev)
{
	struct pld_soc_info info;
	int ret = 0;
	struct hif_softc *scn = HIF_GET_SOFTC(sc);

	ret = pld_get_soc_info(dev, &info);
	sc->mem = info.v_addr;
	sc->ce_sc.ol_sc.mem    = info.v_addr;
	sc->ce_sc.ol_sc.mem_pa = info.p_addr;
	/* dev_mem_info[0] is for CMEM */
	scn->cmem_start = info.dev_mem_info[0].start;
	scn->cmem_size = info.dev_mem_info[0].size;
}

int hif_ahb_configure_irq_by_ceid(struct hif_softc *scn, int ce_id)
{
	int ret = 0;
	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(scn);
	struct platform_device *pdev = (struct platform_device *)sc->pdev;
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	int irq = 0;

	if (ce_id >= CE_COUNT_MAX)
		return -EINVAL;

	ret = pfrm_get_irq(&pdev->dev, (struct qdf_pfm_hndl *)pdev,
			   ic_irqname[HIF_IC_CE0_IRQ_OFFSET + ce_id],
			   HIF_IC_CE0_IRQ_OFFSET + ce_id, &irq);
	if (ret) {
		dev_err(&pdev->dev, "get irq failed\n");
		ret = -EFAULT;
		goto end;
	}

	ic_irqnum[HIF_IC_CE0_IRQ_OFFSET + ce_id] = irq;
	ret = pfrm_request_irq(&pdev->dev, irq,
			       hif_ahb_interrupt_handler,
			       IRQF_TRIGGER_RISING,
			       ic_irqname[HIF_IC_CE0_IRQ_OFFSET + ce_id],
			       &hif_state->tasklets[ce_id]);
	if (ret) {
		dev_err(&pdev->dev, "ath_request_irq failed\n");
		ret = -EFAULT;
		goto end;
	}
	hif_ahb_irq_enable(scn, ce_id);

end:
	return ret;
}

int hif_ahb_configure_irq(struct hif_pci_softc *sc)
{
	int ret = 0;
	struct hif_softc *scn = HIF_GET_SOFTC(sc);
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct CE_attr *host_ce_conf = hif_state->host_ce_config;
	int i;

	/* configure per CE interrupts */
	for (i = 0; i < scn->ce_count; i++) {
		if (host_ce_conf[i].flags & CE_ATTR_DISABLE_INTR)
			continue;

		if (host_ce_conf[i].flags & CE_ATTR_INIT_ON_DEMAND)
			continue;

		ret = hif_ahb_configure_irq_by_ceid(scn, i);
		if (ret)
			goto end;
	}

end:
	return ret;
}

int hif_ahb_configure_grp_irq(struct hif_softc *scn,
			      struct hif_exec_context *hif_ext_group)
{
	int ret = 0;
	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(scn);
	struct platform_device *pdev = (struct platform_device *)sc->pdev;
	int irq = 0;
	int j;

	/* configure external interrupts */
	hif_ext_group->irq_enable = &hif_ahb_exec_grp_irq_enable;
	hif_ext_group->irq_disable = &hif_ahb_exec_grp_irq_disable;
	hif_ext_group->irq_name = &hif_ahb_get_irq_name;
	hif_ext_group->work_complete = &hif_dummy_grp_done;

	for (j = 0; j < hif_ext_group->numirq; j++) {
		ret = pfrm_get_irq(&pdev->dev, (struct qdf_pfm_hndl *)pdev,
				   ic_irqname[hif_ext_group->irq[j]],
				   hif_ext_group->irq[j], &irq);
		if (ret) {
			dev_err(&pdev->dev, "get irq failed\n");
			ret = -EFAULT;
			goto end;
		}
		ic_irqnum[hif_ext_group->irq[j]] = irq;
		hif_ext_group->os_irq[j] = irq;
	}

	for (j = 0; j < hif_ext_group->numirq; j++) {
		irq = hif_ext_group->os_irq[j];

		qdf_spin_lock_irqsave(&hif_ext_group->irq_lock);
		qdf_dev_set_irq_status_flags(irq, QDF_IRQ_DISABLE_UNLAZY);
		qdf_spin_unlock_irqrestore(&hif_ext_group->irq_lock);

		ret = pfrm_request_irq(scn->qdf_dev->dev,
				       irq, hif_ext_group_interrupt_handler,
				       IRQF_TRIGGER_RISING | IRQF_SHARED,
				       ic_irqname[hif_ext_group->irq[j]],
				       hif_ext_group);
		if (ret) {
			dev_err(&pdev->dev, "ath_request_irq failed\n");
			ret = -EFAULT;
			goto end;
		}
	}

	qdf_spin_lock_irqsave(&hif_ext_group->irq_lock);
	hif_ext_group->irq_requested = true;
	qdf_spin_unlock_irqrestore(&hif_ext_group->irq_lock);
end:
	return ret;
}

void hif_ahb_deconfigure_grp_irq(struct hif_softc *scn)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct hif_exec_context *hif_ext_group;
	int i, j;
	int irq = 0;

	/* configure external interrupts */
	for (i = 0; i < hif_state->hif_num_extgroup; i++) {
		hif_ext_group = hif_state->hif_ext_group[i];
		if (hif_ext_group->irq_requested == true) {
			qdf_spin_lock_irqsave(&hif_ext_group->irq_lock);
			hif_ext_group->irq_requested = false;
			for (j = 0; j < hif_ext_group->numirq; j++) {
				irq = hif_ext_group->os_irq[j];
				hif_ext_group->irq_enabled = false;
				qdf_dev_clear_irq_status_flags(
							irq,
							QDF_IRQ_DISABLE_UNLAZY);
			}
			qdf_spin_unlock_irqrestore(&hif_ext_group->irq_lock);

			/* Avoid holding the irq_lock while freeing the irq
			 * as the same lock is being held by the irq handler
			 * while disabling the irq. This causes a deadlock
			 * between free_irq and irq_handler.
			 */
			for (j = 0; j < hif_ext_group->numirq; j++) {
				irq = hif_ext_group->os_irq[j];
				pfrm_free_irq(scn->qdf_dev->dev,
					      irq, hif_ext_group);
			}
		}
	}
}

irqreturn_t hif_ahb_interrupt_handler(int irq, void *context)
{
	struct ce_tasklet_entry *tasklet_entry = context;
	return ce_dispatch_interrupt(tasklet_entry->ce_id, tasklet_entry);
}

/**
 * hif_ahb_disable_bus() - Disable the bus
 * @scn : pointer to the hif context
 *
 * This function disables the bus and helds the target in reset state
 *
 * Return: none
 */
void hif_ahb_disable_bus(struct hif_softc *scn)
{
	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(scn);
	void __iomem *mem;
	struct platform_device *pdev = (struct platform_device *)sc->pdev;
	struct resource *memres = NULL;
	int mem_pa_size = 0;
	struct hif_target_info *tgt_info = NULL;
	struct qdf_vbus_resource *vmres = NULL;
	QDF_STATUS status;

	tgt_info = &scn->target_info;
	/*Disable WIFI clock input*/
	if (sc->mem) {
		status = pfrm_platform_get_resource(
				scn->qdf_dev->dev,
				(struct qdf_pfm_hndl *)pdev, &vmres,
				IORESOURCE_MEM, 0);
		if (QDF_IS_STATUS_ERROR(status)) {
			hif_info("Failed to get IORESOURCE_MEM");
			return;
		}
		memres = (struct resource *)vmres;
		if (memres)
			mem_pa_size = memres->end - memres->start + 1;

		if (tgt_info->target_type == TARGET_TYPE_QCA5018 ||
		    tgt_info->target_type == TARGET_TYPE_QCA5332) {
			iounmap(sc->mem_ce);
			sc->mem_ce = NULL;
			scn->mem_ce = NULL;
		}
		if (sc->mem_pmm_base) {
			iounmap(sc->mem_pmm_base);
			sc->mem_pmm_base = NULL;
			scn->mem_pmm_base = NULL;
		}
		if (sc->mem_cmem) {
			iounmap(sc->mem_cmem);
			sc->mem_cmem = NULL;
			scn->mem_cmem = NULL;
		}
		mem = (void __iomem *)sc->mem;
		if (mem) {
			pfrm_devm_iounmap(&pdev->dev, mem);
			pfrm_devm_release_mem_region(&pdev->dev, scn->mem_pa,
						     mem_pa_size);
			sc->mem = NULL;
			pld_set_bar_addr(&pdev->dev, NULL);
		}
	}
	scn->mem = NULL;
}

/**
 * hif_ahb_enable_bus() - Enable the bus
 * @ol_sc: HIF context
 * @dev: dev
 * @bdev: bus dev
 * @bid: bus id
 * @type: bus type
 *
 * This function enables the radio bus by enabling necessary
 * clocks and waits for the target to get ready to proceed further
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hif_ahb_enable_bus(struct hif_softc *ol_sc,
		struct device *dev, void *bdev,
		const struct hif_bus_id *bid,
		enum hif_enable_type type)
{
	int ret = 0;
	int hif_type;
	int target_type;
	const struct platform_device_id *id = (struct platform_device_id *)bid;
	struct platform_device *pdev = bdev;
	struct hif_target_info *tgt_info = NULL;
	struct resource *memres = NULL;
	void __iomem *mem = NULL;
	uint32_t revision_id = 0;
	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(ol_sc);
	QDF_STATUS status;
	struct qdf_vbus_resource *vmres = NULL;

	sc->pdev = (struct pci_dev *)pdev;
	sc->dev = &pdev->dev;
	sc->devid = id->driver_data;

	ret = hif_get_device_type(id->driver_data, revision_id,
			&hif_type, &target_type);
	if (ret < 0) {
		hif_err("Invalid device ret %d id %d revision_id %d",
			ret, (int)id->driver_data, revision_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (target_type == TARGET_TYPE_QCN6122 ||
	    target_type == TARGET_TYPE_QCN9160) {
		hif_ahb_get_soc_info_pld(sc, dev);
	}

	/* 11BE SoC chipsets Need to call this function to get cmem addr */
	if (target_type == TARGET_TYPE_QCA5332)
		hif_ahb_get_soc_info_pld(sc, dev);

	if (target_type == TARGET_TYPE_QCN6122 ||
	    target_type == TARGET_TYPE_QCN9160) {
		hif_update_irq_ops_with_pci(ol_sc);
	} else {
		status = pfrm_platform_get_resource(&pdev->dev,
						    (struct qdf_pfm_hndl *)pdev,
						    &vmres,
						    IORESOURCE_MEM, 0);
		if (QDF_IS_STATUS_ERROR(status)) {
			hif_err("Failed to get IORESOURCE_MEM");
			return status;
		}
		memres = (struct resource *)vmres;
		if (!memres) {
			hif_err("Failed to get IORESOURCE_MEM");
			return QDF_STATUS_E_IO;
		}

		/* Arrange for access to Target SoC registers. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
		status = pfrm_devm_ioremap_resource(
					dev,
					(struct qdf_vbus_resource *)memres,
					&mem);
#else
		status = pfrm_devm_request_and_ioremap(
					dev,
					(struct qdf_vbus_resource *)memres,
					&mem);
#endif
		if (QDF_IS_STATUS_ERROR(status)) {
			hif_err("ath: ioremap error");
			ret = PTR_ERR(mem);
			goto err_cleanup1;
		}

		sc->mem = mem;
		pld_set_bar_addr(dev, mem);
		ol_sc->mem = mem;
		ol_sc->mem_pa = memres->start;
	}

	ret = pfrm_dma_set_mask(dev, 32);
	if (ret) {
		hif_err("ath: 32-bit DMA not available");
		status = QDF_STATUS_E_IO;
		goto err_cleanup1;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
	ret = pfrm_dma_set_mask_and_coherent(dev, 32);
#else
	ret = pfrm_dma_set_coherent_mask(dev, 32);
#endif
	if (ret) {
		hif_err("Failed to set dma mask error = %d", ret);
		return QDF_STATUS_E_IO;
	}

	tgt_info = hif_get_target_info_handle((struct hif_opaque_softc *)ol_sc);

	tgt_info->target_type = target_type;
	hif_register_tbl_attach(ol_sc, hif_type);
	hif_target_register_tbl_attach(ol_sc, target_type);
	/*
	 * In QCA5018 CE region moved to SOC outside WCSS block.
	 * Allocate separate I/O remap to access CE registers.
	 */
	if (tgt_info->target_type == TARGET_TYPE_QCA5018 ||
	    tgt_info->target_type == TARGET_TYPE_QCA5332) {
		struct hif_softc *scn = HIF_GET_SOFTC(sc);

		sc->mem_ce = ioremap_nocache(HOST_CE_ADDRESS, HOST_CE_SIZE);
		if (IS_ERR(sc->mem_ce)) {
			hif_err("CE: ioremap failed");
			return QDF_STATUS_E_IO;
		}
		ol_sc->mem_ce = sc->mem_ce;
	}

	if (tgt_info->target_type == TARGET_TYPE_QCA5332) {
		struct hif_softc *scn = HIF_GET_SOFTC(sc);

		/*
		 * In QCA5332 CMEM region is outside WCSS block.
		 * Allocate separate I/O remap to access CMEM address.
		 */
		sc->mem_cmem = ioremap_nocache(HOST_CMEM_ADDRESS,
					       HOST_CMEM_SIZE);
		if (IS_ERR(sc->mem_cmem)) {
			hif_err("CE: ioremap failed");
			return QDF_STATUS_E_IO;
		}
		ol_sc->mem_cmem = sc->mem_cmem;

		/*
		 * PMM SCRATCH Register for QCA5332
		 */
		sc->mem_pmm_base = ioremap_nocache(PMM_SCRATCH_BASE,
						   PMM_SCRATCH_SIZE);
		if (IS_ERR(sc->mem_pmm_base)) {
			hif_err("CE: ioremap failed");
			return QDF_STATUS_E_IO;
		}
		ol_sc->mem_pmm_base = sc->mem_pmm_base;
	}

	hif_info("X - hif_type = 0x%x, target_type = 0x%x",
		hif_type, target_type);

	return QDF_STATUS_SUCCESS;
err_cleanup1:
	return status;
}

/**
 * hif_ahb_nointrs() - disable IRQ
 *
 * @scn: struct hif_softc
 *
 * This function stops interrupt(s)
 *
 * Return: none
 */
void hif_ahb_nointrs(struct hif_softc *scn)
{
	int i;
	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(scn);
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct CE_attr *host_ce_conf = hif_state->host_ce_config;

	scn->free_irq_done = true;
	ce_unregister_irq(hif_state, CE_ALL_BITMAP);

	if (scn->request_irq_done == false)
		return;

	if (sc->num_msi_intrs > 0) {
		/* MSI interrupt(s) */
		for (i = 0; i < sc->num_msi_intrs; i++) {
			pfrm_free_irq(scn->qdf_dev->dev, sc->irq + i, sc);
		}
		sc->num_msi_intrs = 0;
	} else {
		if (!scn->per_ce_irq) {
			pfrm_free_irq(scn->qdf_dev->dev, sc->irq, sc);
		} else {
			for (i = 0; i < scn->ce_count; i++) {
				if (host_ce_conf[i].flags
						& CE_ATTR_DISABLE_INTR)
					continue;
				if (!hif_state->tasklets[i].inited)
					continue;
				pfrm_free_irq(
					scn->qdf_dev->dev,
					ic_irqnum[HIF_IC_CE0_IRQ_OFFSET + i],
					&hif_state->tasklets[i]);
			}
			hif_ahb_deconfigure_grp_irq(scn);
		}
	}
	scn->request_irq_done = false;

}

/**
 * hif_ahb_irq_enable() - enable copy engine IRQ
 * @scn: struct hif_softc
 * @ce_id: ce_id
 *
 * This function enables the interrupt for the radio.
 *
 * Return: N/A
 */
void hif_ahb_irq_enable(struct hif_softc *scn, int ce_id)
{
	uint32_t regval;
	uint32_t reg_offset = 0;
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct CE_pipe_config *target_ce_conf = &hif_state->target_ce_config[ce_id];
	struct hif_target_info *tgt_info = &scn->target_info;
	void *mem = scn->mem_ce ? scn->mem_ce : scn->mem;

	if (scn->per_ce_irq) {
		if (target_ce_conf->pipedir & PIPEDIR_OUT) {
			reg_offset = HOST_IE_ADDRESS;
			qdf_spin_lock_irqsave(&hif_state->irq_reg_lock);
			regval = hif_read32_mb(scn, mem + reg_offset);
			regval |= HOST_IE_REG1_CE_BIT(ce_id);
			hif_write32_mb(scn, mem + reg_offset, regval);
			qdf_spin_unlock_irqrestore(&hif_state->irq_reg_lock);
		}
		if (target_ce_conf->pipedir & PIPEDIR_IN) {
			reg_offset = HOST_IE_ADDRESS_2;
			qdf_spin_lock_irqsave(&hif_state->irq_reg_lock);
			regval = hif_read32_mb(scn, mem + reg_offset);
			regval |= HOST_IE_REG2_CE_BIT(ce_id);
			hif_write32_mb(scn, mem + reg_offset, regval);
			if (tgt_info->target_type == TARGET_TYPE_QCA8074 ||
			    tgt_info->target_type == TARGET_TYPE_QCA8074V2 ||
			    tgt_info->target_type == TARGET_TYPE_QCA9574 ||
			    tgt_info->target_type == TARGET_TYPE_QCA5332 ||
			    tgt_info->target_type == TARGET_TYPE_QCA5018 ||
			    tgt_info->target_type == TARGET_TYPE_QCA6018) {
				/* Enable destination ring interrupts for
				 * 8074, 8074V2, 6018 and 50xx
				 */
				regval = hif_read32_mb(scn, mem +
					HOST_IE_ADDRESS_3);
				regval |= HOST_IE_REG3_CE_BIT(ce_id);

				hif_write32_mb(scn, mem +
					       HOST_IE_ADDRESS_3, regval);
			}
			qdf_spin_unlock_irqrestore(&hif_state->irq_reg_lock);
		}
	} else {
		hif_pci_irq_enable(scn, ce_id);
	}
}

/**
 * hif_ahb_irq_disable() - disable copy engine IRQ
 * @scn: struct hif_softc
 * @ce_id: ce_id
 *
 * Return: N/A
 */
void hif_ahb_irq_disable(struct hif_softc *scn, int ce_id)
{
	uint32_t regval;
	uint32_t reg_offset = 0;
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct CE_pipe_config *target_ce_conf = &hif_state->target_ce_config[ce_id];
	struct hif_target_info *tgt_info = &scn->target_info;
	void *mem = scn->mem_ce ? scn->mem_ce : scn->mem;

	if (scn->per_ce_irq) {
		if (target_ce_conf->pipedir & PIPEDIR_OUT) {
			reg_offset = HOST_IE_ADDRESS;
			qdf_spin_lock_irqsave(&hif_state->irq_reg_lock);
			regval = hif_read32_mb(scn, mem + reg_offset);
			regval &= ~HOST_IE_REG1_CE_BIT(ce_id);
			hif_write32_mb(scn, mem + reg_offset, regval);
			qdf_spin_unlock_irqrestore(&hif_state->irq_reg_lock);
		}
		if (target_ce_conf->pipedir & PIPEDIR_IN) {
			reg_offset = HOST_IE_ADDRESS_2;
			qdf_spin_lock_irqsave(&hif_state->irq_reg_lock);
			regval = hif_read32_mb(scn, mem + reg_offset);
			regval &= ~HOST_IE_REG2_CE_BIT(ce_id);
			hif_write32_mb(scn, mem + reg_offset, regval);
			if (tgt_info->target_type == TARGET_TYPE_QCA8074 ||
			    tgt_info->target_type == TARGET_TYPE_QCA8074V2 ||
			    tgt_info->target_type == TARGET_TYPE_QCA9574 ||
			    tgt_info->target_type == TARGET_TYPE_QCA5332 ||
			    tgt_info->target_type == TARGET_TYPE_QCA5018 ||
			    tgt_info->target_type == TARGET_TYPE_QCA6018) {
				/* Disable destination ring interrupts for
				 * 8074, 8074V2, 6018 and 50xx
				 */
				regval = hif_read32_mb(scn, mem +
					HOST_IE_ADDRESS_3);
				regval &= ~HOST_IE_REG3_CE_BIT(ce_id);

				hif_write32_mb(scn, mem +
					       HOST_IE_ADDRESS_3, regval);
			}
			qdf_spin_unlock_irqrestore(&hif_state->irq_reg_lock);
		}
	}
}

void hif_ahb_exec_grp_irq_disable(struct hif_exec_context *hif_ext_group)
{
	int i;

	qdf_spin_lock_irqsave(&hif_ext_group->irq_lock);
	if (hif_ext_group->irq_enabled) {
		for (i = 0; i < hif_ext_group->numirq; i++) {
			disable_irq_nosync(hif_ext_group->os_irq[i]);
		}
		hif_ext_group->irq_enabled = false;
	}
	qdf_spin_unlock_irqrestore(&hif_ext_group->irq_lock);
}

void hif_ahb_exec_grp_irq_enable(struct hif_exec_context *hif_ext_group)
{
	int i;

	qdf_spin_lock_irqsave(&hif_ext_group->irq_lock);
	if (hif_ext_group->irq_requested && !hif_ext_group->irq_enabled) {
		for (i = 0; i < hif_ext_group->numirq; i++) {
			enable_irq(hif_ext_group->os_irq[i]);
		}
		hif_ext_group->irq_enabled = true;
	}
	qdf_spin_unlock_irqrestore(&hif_ext_group->irq_lock);
}

/**
 * hif_ahb_needs_bmi() - return true if the soc needs bmi through the driver
 * @scn: hif context
 *
 * Return: true if soc needs driver bmi otherwise false
 */
bool hif_ahb_needs_bmi(struct hif_softc *scn)
{
	return !ce_srng_based(scn);
}

/**
 * hif_display_ahb_irq_regs() - prints the host interrupt enable (IE) regs
 * @scn: hif context
 *
 * Return: None
 */

void hif_display_ahb_irq_regs(struct hif_softc *scn)
{
	uint32_t regval;
	void *mem = scn->mem_ce ? scn->mem_ce : scn->mem;
	struct hif_target_info *tgt_info = &scn->target_info;

	if (tgt_info->target_type == TARGET_TYPE_QCN6122 ||
	    tgt_info->target_type == TARGET_TYPE_QCN9160) {
		return;
	}
	if (scn->per_ce_irq) {
		regval = hif_read32_mb(scn, mem + HOST_IE_ADDRESS);
		hif_nofl_err("IRQ enable register value 0x%08x", regval);

		regval = hif_read32_mb(scn, mem + HOST_IE_ADDRESS_2);
		hif_nofl_err("IRQ enable register 2 value 0x%08x", regval);

		if (tgt_info->target_type == TARGET_TYPE_QCA8074 ||
		    tgt_info->target_type == TARGET_TYPE_QCA8074V2 ||
		    tgt_info->target_type == TARGET_TYPE_QCA9574 ||
		    tgt_info->target_type == TARGET_TYPE_QCA5332 ||
		    tgt_info->target_type == TARGET_TYPE_QCA5018 ||
		    tgt_info->target_type == TARGET_TYPE_QCA6018) {
			regval = hif_read32_mb(scn, mem +
					       HOST_IE_ADDRESS_3);
			hif_nofl_err("IRQ enable register 3 value 0x%08x",
				     regval);
		}
	}
}

void hif_ahb_display_stats(struct hif_softc *scn)
{
	if (!scn) {
		hif_err("hif_scn null");
		return;
	}
	hif_display_ahb_irq_regs(scn);
	hif_display_ce_stats(scn);
}

void hif_ahb_clear_stats(struct hif_softc *scn)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);

	if (!hif_state) {
		hif_err("hif_state null");
		return;
	}
	hif_clear_ce_stats(hif_state);
}
