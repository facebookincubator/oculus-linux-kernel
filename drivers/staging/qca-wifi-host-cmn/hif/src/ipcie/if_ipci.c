/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/if_arp.h>
#ifdef CONFIG_PCI_MSM
#include <linux/msm_pcie.h>
#endif
#include "hif_io32.h"
#include "if_ipci.h"
#include "hif.h"
#include "target_type.h"
#include "hif_main.h"
#include "ce_main.h"
#include "ce_api.h"
#include "ce_internal.h"
#include "ce_reg.h"
#include "ce_bmi.h"
#include "regtable.h"
#include "hif_hw_version.h"
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "qdf_status.h"
#include "qdf_atomic.h"
#include "pld_common.h"
#include "mp_dev.h"
#include "hif_debug.h"

#include "ce_tasklet.h"
#include "targaddrs.h"
#include "hif_exec.h"

#include "ipci_api.h"

void hif_ipci_enable_power_management(struct hif_softc *hif_sc,
				      bool is_packet_log_enabled)
{
}

void hif_ipci_disable_power_management(struct hif_softc *hif_ctx)
{
}

void hif_ipci_display_stats(struct hif_softc *hif_ctx)
{
	hif_display_ce_stats(hif_ctx);
}

void hif_ipci_clear_stats(struct hif_softc *hif_ctx)
{
	struct hif_ipci_softc *ipci_ctx = HIF_GET_IPCI_SOFTC(hif_ctx);

	if (!ipci_ctx) {
		HIF_ERROR("%s, hif_ctx null", __func__);
		return;
	}
	hif_clear_ce_stats(&ipci_ctx->ce_sc);
}

QDF_STATUS hif_ipci_open(struct hif_softc *hif_ctx, enum qdf_bus_type bus_type)
{
	struct hif_ipci_softc *sc = HIF_GET_IPCI_SOFTC(hif_ctx);

	hif_ctx->bus_type = bus_type;

	qdf_spinlock_create(&sc->irq_lock);

	return hif_ce_open(hif_ctx);
}

int hif_ipci_bus_configure(struct hif_softc *hif_sc)
{
	int status = 0;
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(hif_sc);

	hif_ce_prepare_config(hif_sc);

	/* initialize sleep state adjust variables */
	hif_state->sleep_timer_init = true;
	hif_state->keep_awake_count = 0;
	hif_state->fake_sleep = false;
	hif_state->sleep_ticks = 0;

	status = hif_wlan_enable(hif_sc);
	if (status) {
		HIF_ERROR("%s: hif_wlan_enable error = %d",
			  __func__, status);
		goto timer_free;
	}

	A_TARGET_ACCESS_LIKELY(hif_sc);

	status = hif_config_ce(hif_sc);
	if (status)
		goto disable_wlan;

	status = hif_configure_irq(hif_sc);
	if (status < 0)
		goto unconfig_ce;

	A_TARGET_ACCESS_UNLIKELY(hif_sc);

	return status;

unconfig_ce:
	hif_unconfig_ce(hif_sc);
disable_wlan:
	A_TARGET_ACCESS_UNLIKELY(hif_sc);
	hif_wlan_disable(hif_sc);

timer_free:
	qdf_timer_stop(&hif_state->sleep_timer);
	qdf_timer_free(&hif_state->sleep_timer);
	hif_state->sleep_timer_init = false;

	HIF_ERROR("%s: failed, status = %d", __func__, status);
	return status;
}

void hif_ipci_close(struct hif_softc *hif_sc)
{
	hif_ce_close(hif_sc);
}

/**
 * hif_ce_srng_msi_free_irq(): free CE msi IRQ
 * @scn: struct hif_softc
 *
 * Return: ErrorNo
 */
static int hif_ce_srng_msi_free_irq(struct hif_softc *scn)
{
	int ret;
	int ce_id, irq;
	uint32_t msi_data_start;
	uint32_t msi_data_count;
	uint32_t msi_irq_start;
	struct HIF_CE_state *ce_sc = HIF_GET_CE_STATE(scn);

	ret = pld_get_user_msi_assignment(scn->qdf_dev->dev, "CE",
					  &msi_data_count, &msi_data_start,
					  &msi_irq_start);
	if (ret)
		return ret;

	/* needs to match the ce_id -> irq data mapping
	 * used in the srng parameter configuration
	 */
	for (ce_id = 0; ce_id < scn->ce_count; ce_id++) {
		unsigned int msi_data;

		if (!ce_sc->tasklets[ce_id].inited)
			continue;

		msi_data = (ce_id % msi_data_count) + msi_irq_start;
		irq = pld_get_msi_irq(scn->qdf_dev->dev, msi_data);

		hif_debug("%s: (ce_id %d, msi_data %d, irq %d)", __func__,
			  ce_id, msi_data, irq);

		free_irq(irq, &ce_sc->tasklets[ce_id]);
	}

	return ret;
}

/**
 * hif_ipci_deconfigure_grp_irq(): deconfigure HW block IRQ
 * @scn: struct hif_softc
 *
 * Return: none
 */
void hif_ipci_deconfigure_grp_irq(struct hif_softc *scn)
{
	int i, j, irq;
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct hif_exec_context *hif_ext_group;

	for (i = 0; i < hif_state->hif_num_extgroup; i++) {
		hif_ext_group = hif_state->hif_ext_group[i];
		if (hif_ext_group->irq_requested) {
			hif_ext_group->irq_requested = false;
			for (j = 0; j < hif_ext_group->numirq; j++) {
				irq = hif_ext_group->os_irq[j];
				free_irq(irq, hif_ext_group);
			}
			hif_ext_group->numirq = 0;
		}
	}
}

void hif_ipci_nointrs(struct hif_softc *scn)
{
	int ret;
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);

	ce_unregister_irq(hif_state, CE_ALL_BITMAP);

	if (scn->request_irq_done == false)
		return;

	hif_ipci_deconfigure_grp_irq(scn);

	ret = hif_ce_srng_msi_free_irq(scn);
	if (ret != -EINVAL) {
		/* ce irqs freed in hif_ce_srng_msi_free_irq */

		if (scn->wake_irq)
			free_irq(scn->wake_irq, scn);
		scn->wake_irq = 0;
	}

	scn->request_irq_done = false;
}

void hif_ipci_disable_bus(struct hif_softc *scn)
{
	struct hif_ipci_softc *sc = HIF_GET_IPCI_SOFTC(scn);
	void __iomem *mem;

	/* Attach did not succeed, all resources have been
	 * freed in error handler
	 */
	if (!sc)
		return;

	mem = (void __iomem *)sc->mem;
	if (mem) {
		hif_dump_pipe_debug_count(scn);
		if (scn->athdiag_procfs_inited) {
			athdiag_procfs_remove();
			scn->athdiag_procfs_inited = false;
		}
		scn->mem = NULL;
	}
	HIF_INFO("%s: X", __func__);
}

#if defined(CONFIG_PCI_MSM)
void hif_ipci_prevent_linkdown(struct hif_softc *scn, bool flag)
{
	int errno;

	HIF_INFO("wlan: %s pcie power collapse", flag ? "disable" : "enable");

	errno = pld_wlan_pm_control(scn->qdf_dev->dev, flag);
	if (errno)
		HIF_ERROR("%s: Failed pld_wlan_pm_control; errno %d",
			  __func__, errno);
}
#else
void hif_ipci_prevent_linkdown(struct hif_softc *scn, bool flag)
{
	HIF_INFO("wlan: %s pcie power collapse", (flag ? "disable" : "enable"));
}
#endif

int hif_ipci_bus_suspend(struct hif_softc *scn)
{
	QDF_STATUS ret;

	hif_apps_irqs_disable(GET_HIF_OPAQUE_HDL(scn));

	ret = hif_try_complete_tasks(scn);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hif_apps_irqs_enable(GET_HIF_OPAQUE_HDL(scn));
		return -EBUSY;
	}

	return 0;
}

int hif_ipci_bus_resume(struct hif_softc *scn)
{
	hif_apps_irqs_enable(GET_HIF_OPAQUE_HDL(scn));

	return 0;
}

int hif_ipci_bus_suspend_noirq(struct hif_softc *scn)
{
	if (hif_can_suspend_link(GET_HIF_OPAQUE_HDL(scn)))
		qdf_atomic_set(&scn->link_suspended, 1);

	hif_apps_wake_irq_enable(GET_HIF_OPAQUE_HDL(scn));

	return 0;
}

int hif_ipci_bus_resume_noirq(struct hif_softc *scn)
{
	hif_apps_wake_irq_disable(GET_HIF_OPAQUE_HDL(scn));

	if (hif_can_suspend_link(GET_HIF_OPAQUE_HDL(scn)))
		qdf_atomic_set(&scn->link_suspended, 0);

	return 0;
}

void hif_ipci_disable_isr(struct hif_softc *scn)
{
	struct hif_ipci_softc *sc = HIF_GET_IPCI_SOFTC(scn);

	hif_exec_kill(&scn->osc);
	hif_nointrs(scn);
	/* Cancel the pending tasklet */
	ce_tasklet_kill(scn);
	tasklet_kill(&sc->intr_tq);
	qdf_atomic_set(&scn->active_tasklet_cnt, 0);
	qdf_atomic_set(&scn->active_grp_tasklet_cnt, 0);
}

int hif_ipci_dump_registers(struct hif_softc *hif_ctx)
{
	int status;
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);

	status = hif_dump_ce_registers(scn);

	if (status)
		HIF_ERROR("%s: Dump CE Registers Failed", __func__);

	return 0;
}

/**
 * hif_ce_interrupt_handler() - interrupt handler for copy engine
 * @irq: irq number
 * @context: tasklet context
 *
 * Return: irqreturn_t
 */
static irqreturn_t hif_ce_interrupt_handler(int irq, void *context)
{
	struct ce_tasklet_entry *tasklet_entry = context;

	return ce_dispatch_interrupt(tasklet_entry->ce_id, tasklet_entry);
}

extern const char *ce_name[];

/**
 * hif_ce_msi_map_ce_to_irq() - map CE to IRQ
 * @scn: hif context
 * @ce_id: CE Id
 *
 * Return: IRQ number
 */
static int hif_ce_msi_map_ce_to_irq(struct hif_softc *scn, int ce_id)
{
	struct hif_ipci_softc *ipci_scn = HIF_GET_IPCI_SOFTC(scn);

	return ipci_scn->ce_msi_irq_num[ce_id];
}

/* hif_ce_srng_msi_irq_disable() - disable the irq for msi
 * @hif_sc: hif context
 * @ce_id: which ce to disable copy complete interrupts for
 *
 * @Return: none
 */
static void hif_ce_srng_msi_irq_disable(struct hif_softc *hif_sc, int ce_id)
{
	disable_irq_nosync(hif_ce_msi_map_ce_to_irq(hif_sc, ce_id));
}

/* hif_ce_srng_msi_irq_enable() - enable the irq for msi
 * @hif_sc: hif context
 * @ce_id: which ce to enable copy complete interrupts for
 *
 * @Return: none
 */
static void hif_ce_srng_msi_irq_enable(struct hif_softc *hif_sc, int ce_id)
{
	enable_irq(hif_ce_msi_map_ce_to_irq(hif_sc, ce_id));
}

/* hif_ce_msi_configure_irq() - configure the irq
 * @scn: hif context
 *
 * @Return: none
 */
static int hif_ce_msi_configure_irq(struct hif_softc *scn)
{
	int ret;
	int ce_id, irq;
	uint32_t msi_data_start;
	uint32_t msi_data_count;
	uint32_t msi_irq_start;
	struct HIF_CE_state *ce_sc = HIF_GET_CE_STATE(scn);
	struct hif_ipci_softc *ipci_sc = HIF_GET_IPCI_SOFTC(scn);

	/* do wake irq assignment */
	ret = pld_get_user_msi_assignment(scn->qdf_dev->dev, "WAKE",
					  &msi_data_count, &msi_data_start,
					  &msi_irq_start);
	if (ret)
		return ret;

	scn->wake_irq = pld_get_msi_irq(scn->qdf_dev->dev, msi_irq_start);
	ret = request_irq(scn->wake_irq, hif_wake_interrupt_handler,
			  IRQF_NO_SUSPEND, "wlan_wake_irq", scn);
	if (ret)
		return ret;

	/* do ce irq assignments */
	ret = pld_get_user_msi_assignment(scn->qdf_dev->dev, "CE",
					  &msi_data_count, &msi_data_start,
					  &msi_irq_start);
	if (ret)
		goto free_wake_irq;

	scn->bus_ops.hif_irq_disable = &hif_ce_srng_msi_irq_disable;
	scn->bus_ops.hif_irq_enable = &hif_ce_srng_msi_irq_enable;
	scn->bus_ops.hif_map_ce_to_irq = &hif_ce_msi_map_ce_to_irq;

	/* needs to match the ce_id -> irq data mapping
	 * used in the srng parameter configuration
	 */
	for (ce_id = 0; ce_id < scn->ce_count; ce_id++) {
		unsigned int msi_data = (ce_id % msi_data_count) +
			msi_irq_start;
		irq = pld_get_msi_irq(scn->qdf_dev->dev, msi_data);
		HIF_DBG("%s: (ce_id %d, msi_data %d, irq %d tasklet %pK)",
			__func__, ce_id, msi_data, irq,
			&ce_sc->tasklets[ce_id]);

		/* implies the ce is also initialized */
		if (!ce_sc->tasklets[ce_id].inited)
			continue;

		ipci_sc->ce_msi_irq_num[ce_id] = irq;
		ret = request_irq(irq, hif_ce_interrupt_handler,
				  IRQF_SHARED,
				  ce_name[ce_id],
				  &ce_sc->tasklets[ce_id]);
		if (ret)
			goto free_irq;
	}

	return ret;

free_irq:
	/* the request_irq for the last ce_id failed so skip it. */
	while (ce_id > 0 && ce_id < scn->ce_count) {
		unsigned int msi_data;

		ce_id--;
		msi_data = (ce_id % msi_data_count) + msi_irq_start;
		irq = pld_get_msi_irq(scn->qdf_dev->dev, msi_data);
		free_irq(irq, &ce_sc->tasklets[ce_id]);
	}

free_wake_irq:
	free_irq(scn->wake_irq, scn->qdf_dev->dev);
	scn->wake_irq = 0;

	return ret;
}

/**
 * hif_exec_grp_irq_disable() - disable the irq for group
 * @hif_ext_group: hif exec context
 *
 * Return: none
 */
static void hif_exec_grp_irq_disable(struct hif_exec_context *hif_ext_group)
{
	int i;

	for (i = 0; i < hif_ext_group->numirq; i++)
		disable_irq_nosync(hif_ext_group->os_irq[i]);
}

/**
 * hif_exec_grp_irq_enable() - enable the irq for group
 * @hif_ext_group: hif exec context
 *
 * Return: none
 */
static void hif_exec_grp_irq_enable(struct hif_exec_context *hif_ext_group)
{
	int i;

	for (i = 0; i < hif_ext_group->numirq; i++)
		enable_irq(hif_ext_group->os_irq[i]);
}

const char *hif_ipci_get_irq_name(int irq_no)
{
	return "pci-dummy";
}

int hif_ipci_configure_grp_irq(struct hif_softc *scn,
			       struct hif_exec_context *hif_ext_group)
{
	int ret = 0;
	int irq = 0;
	int j;

	hif_ext_group->irq_enable = &hif_exec_grp_irq_enable;
	hif_ext_group->irq_disable = &hif_exec_grp_irq_disable;
	hif_ext_group->irq_name = &hif_ipci_get_irq_name;
	hif_ext_group->work_complete = &hif_dummy_grp_done;

	for (j = 0; j < hif_ext_group->numirq; j++) {
		irq = hif_ext_group->irq[j];

		hif_info("request_irq = %d for grp %d",
			 irq, hif_ext_group->grp_id);
		ret = request_irq(irq,
				  hif_ext_group_interrupt_handler,
				  IRQF_SHARED | IRQF_NO_SUSPEND,
				  "wlan_EXT_GRP",
				  hif_ext_group);
		if (ret) {
			HIF_ERROR("%s: request_irq failed ret = %d",
				  __func__, ret);
			return -EFAULT;
		}
		hif_ext_group->os_irq[j] = irq;
	}
	hif_ext_group->irq_requested = true;
	return 0;
}

int hif_configure_irq(struct hif_softc *scn)
{
	int ret = 0;

	HIF_TRACE("%s: E", __func__);

	if (hif_is_polled_mode_enabled(GET_HIF_OPAQUE_HDL(scn))) {
		scn->request_irq_done = false;
		return 0;
	}

	ret = hif_ce_msi_configure_irq(scn);
	if (ret == 0)
		goto end;

	if (ret < 0) {
		HIF_ERROR("%s: hif_ipci_configure_irq error = %d",
			  __func__, ret);
		return ret;
	}
end:
	scn->request_irq_done = true;
	return 0;
}

/**
 * hif_ipci_get_soc_info_pld() - get soc info for ipcie bus from pld target
 * @sc: ipci context
 * @dev: device structure
 *
 * Return: none
 */
static void hif_ipci_get_soc_info_pld(struct hif_ipci_softc *sc,
				      struct device *dev)
{
	struct pld_soc_info info;

	pld_get_soc_info(dev, &info);
	sc->mem = info.v_addr;
	sc->ce_sc.ol_sc.mem    = info.v_addr;
	sc->ce_sc.ol_sc.mem_pa = info.p_addr;
}

/**
 * hif_ipci_get_soc_info_nopld() - get soc info for ipcie bus for non pld target
 * @sc: ipci context
 * @dev: device structure
 *
 * Return: none
 */
static void hif_ipci_get_soc_info_nopld(struct hif_ipci_softc *sc,
					struct device *dev)
{}

/**
 * hif_is_pld_based_target() - verify if the target is pld based
 * @sc: ipci context
 * @device_id: device id
 *
 * Return: none
 */
static bool hif_is_pld_based_target(struct hif_ipci_softc *sc,
				    int device_id)
{
	if (!pld_have_platform_driver_support(sc->dev))
		return false;

	switch (device_id) {
#ifdef QCA_WIFI_QCA6750
	case QCA6750_DEVICE_ID:
#endif
		return true;
	}
	return false;
}

/**
 * hif_ipci_init_deinit_ops_attach() - attach ops for ipci
 * @sc: ipci context
 * @device_id: device id
 *
 * Return: none
 */
static void hif_ipci_init_deinit_ops_attach(struct hif_ipci_softc *sc,
					    int device_id)
{
	if (hif_is_pld_based_target(sc, device_id))
		sc->hif_ipci_get_soc_info = hif_ipci_get_soc_info_pld;
	else
		sc->hif_ipci_get_soc_info = hif_ipci_get_soc_info_nopld;
}

QDF_STATUS hif_ipci_enable_bus(struct hif_softc *ol_sc,
			       struct device *dev, void *bdev,
			       const struct hif_bus_id *bid,
			       enum hif_enable_type type)
{
	int ret = 0;
	uint32_t hif_type, target_type;
	struct hif_ipci_softc *sc = HIF_GET_IPCI_SOFTC(ol_sc);
	struct hif_opaque_softc *hif_hdl = GET_HIF_OPAQUE_HDL(ol_sc);
	uint16_t revision_id = 0;
	struct pci_dev *pdev = bdev;
	struct hif_target_info *tgt_info;
	int device_id = QCA6750_DEVICE_ID;

	if (!ol_sc) {
		HIF_ERROR("%s: hif_ctx is NULL", __func__);
		return QDF_STATUS_E_NOMEM;
	}

	sc->dev = dev;
	tgt_info = hif_get_target_info_handle(hif_hdl);
	hif_ipci_init_deinit_ops_attach(sc, device_id);
	sc->hif_ipci_get_soc_info(sc, dev);
	HIF_TRACE("%s: hif_enable_pci done", __func__);

	device_disable_async_suspend(&pdev->dev);

	ret = hif_get_device_type(device_id, revision_id,
				  &hif_type, &target_type);
	if (ret < 0) {
		HIF_ERROR("%s: invalid device id/revision_id", __func__);
		return QDF_STATUS_E_ABORTED;
	}
	HIF_TRACE("%s: hif_type = 0x%x, target_type = 0x%x",
		  __func__, hif_type, target_type);

	hif_register_tbl_attach(ol_sc, hif_type);
	hif_target_register_tbl_attach(ol_sc, target_type);
	sc->use_register_windowing = false;
	tgt_info->target_type = target_type;

	if (!ol_sc->mem_pa) {
		HIF_ERROR("%s: ERROR - BAR0 uninitialized", __func__);
		ret = -EIO;
		return QDF_STATUS_E_ABORTED;
	}

	return 0;
}

bool hif_ipci_needs_bmi(struct hif_softc *scn)
{
	return !ce_srng_based(scn);
}

#ifdef FORCE_WAKE
int hif_force_wake_request(struct hif_opaque_softc *hif_handle)
{
	uint32_t timeout = 0, value;
	struct hif_softc *scn = (struct hif_softc *)hif_handle;
	struct hif_ipci_softc *ipci_scn = HIF_GET_IPCI_SOFTC(scn);

	if (pld_force_wake_request(scn->qdf_dev->dev)) {
		hif_err("force wake request send failed");
		return -EINVAL;
	}

	HIF_STATS_INC(ipci_scn, mhi_force_wake_request_vote, 1);
	while (!pld_is_device_awake(scn->qdf_dev->dev) &&
	       timeout <= FORCE_WAKE_DELAY_TIMEOUT_MS) {
		qdf_mdelay(FORCE_WAKE_DELAY_MS);
		timeout += FORCE_WAKE_DELAY_MS;
	}

	if (pld_is_device_awake(scn->qdf_dev->dev) <= 0) {
		hif_err("Unable to wake up mhi");
		HIF_STATS_INC(ipci_scn, mhi_force_wake_failure, 1);
		return -EINVAL;
	}
	HIF_STATS_INC(ipci_scn, mhi_force_wake_success, 1);
	hif_write32_mb(scn,
		       scn->mem +
		       PCIE_SOC_PCIE_REG_PCIE_SCRATCH_0_SOC_PCIE_REG,
		       0);
	hif_write32_mb(scn,
		       scn->mem +
		       PCIE_PCIE_LOCAL_REG_PCIE_SOC_WAKE_PCIE_LOCAL_REG,
		       1);

	HIF_STATS_INC(ipci_scn, soc_force_wake_register_write_success, 1);
	/*
	 * do not reset the timeout
	 * total_wake_time = MHI_WAKE_TIME + PCI_WAKE_TIME < 50 ms
	 */
	do {
		value =
		hif_read32_mb(scn,
			      scn->mem +
			      PCIE_SOC_PCIE_REG_PCIE_SCRATCH_0_SOC_PCIE_REG);
		if (value)
			break;
		qdf_mdelay(FORCE_WAKE_DELAY_MS);
		timeout += FORCE_WAKE_DELAY_MS;
	} while (timeout <= FORCE_WAKE_DELAY_TIMEOUT_MS);

	if (!value) {
		hif_err("failed handshake mechanism");
		HIF_STATS_INC(ipci_scn, soc_force_wake_failure, 1);
		return -ETIMEDOUT;
	}

	HIF_STATS_INC(ipci_scn, soc_force_wake_success, 1);

	return 0;
}

int hif_force_wake_release(struct hif_opaque_softc *hif_handle)
{
	int ret;
	struct hif_softc *scn = (struct hif_softc *)hif_handle;
	struct hif_ipci_softc *ipci_scn = HIF_GET_IPCI_SOFTC(scn);

	ret = pld_force_wake_release(scn->qdf_dev->dev);
	if (ret) {
		hif_err("force wake release failure");
		HIF_STATS_INC(ipci_scn, mhi_force_wake_release_failure, 1);
		return ret;
	}

	HIF_STATS_INC(ipci_scn, mhi_force_wake_release_success, 1);
	hif_write32_mb(scn,
		       scn->mem +
		       PCIE_PCIE_LOCAL_REG_PCIE_SOC_WAKE_PCIE_LOCAL_REG,
		       0);
	HIF_STATS_INC(ipci_scn, soc_force_wake_release_success, 1);
	return 0;
}

void hif_print_ipci_stats(struct hif_ipci_softc *ipci_handle)
{
	hif_debug("mhi_force_wake_request_vote: %d",
		  ipci_handle->stats.mhi_force_wake_request_vote);
	hif_debug("mhi_force_wake_failure: %d",
		  ipci_handle->stats.mhi_force_wake_failure);
	hif_debug("mhi_force_wake_success: %d",
		  ipci_handle->stats.mhi_force_wake_success);
	hif_debug("soc_force_wake_register_write_success: %d",
		  ipci_handle->stats.soc_force_wake_register_write_success);
	hif_debug("soc_force_wake_failure: %d",
		  ipci_handle->stats.soc_force_wake_failure);
	hif_debug("soc_force_wake_success: %d",
		  ipci_handle->stats.soc_force_wake_success);
	hif_debug("mhi_force_wake_release_failure: %d",
		  ipci_handle->stats.mhi_force_wake_release_failure);
	hif_debug("mhi_force_wake_release_success: %d",
		  ipci_handle->stats.mhi_force_wake_release_success);
	hif_debug("oc_force_wake_release_success: %d",
		  ipci_handle->stats.soc_force_wake_release_success);
}
#endif /* FORCE_WAKE */
