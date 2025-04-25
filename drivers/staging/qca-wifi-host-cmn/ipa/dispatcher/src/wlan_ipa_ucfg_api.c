/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: public API related to the wlan ipa called by north bound HDD/OSIF
 */

#include "wlan_ipa_ucfg_api.h"
#include "wlan_ipa_main.h"
#include "cfg_ucfg_api.h"
#include "qdf_module.h"

void ucfg_ipa_set_pld_enable(bool flag)
{
	ipa_set_pld_enable(flag);
}

qdf_export_symbol(ucfg_ipa_set_pld_enable);

bool ucfg_ipa_get_pld_enable(void)
{
	return ipa_get_pld_enable();
}

qdf_export_symbol(ucfg_ipa_get_pld_enable);

bool ucfg_ipa_is_present(void)
{
	return ipa_is_hw_support();
}

qdf_export_symbol(ucfg_ipa_is_present);

bool ucfg_ipa_is_ready(void)
{
	return ipa_cb_is_ready();
}

qdf_export_symbol(ucfg_ipa_is_ready);

bool ucfg_ipa_is_enabled(void)
{
	return ipa_config_is_enabled();
}

qdf_export_symbol(ucfg_ipa_is_enabled);

bool ucfg_ipa_uc_is_enabled(void)
{
	return ipa_config_is_uc_enabled();
}

qdf_export_symbol(ucfg_ipa_uc_is_enabled);

bool ucfg_ipa_is_vlan_enabled(void)
{
	return ipa_config_is_vlan_enabled();
}

qdf_export_symbol(ucfg_ipa_is_vlan_enabled);

void ucfg_ipa_set_pdev_id(struct wlan_objmgr_psoc *psoc,
			  uint8_t pdev_id)
{
	return ipa_set_pdev_id(psoc, pdev_id);
}

qdf_export_symbol(ucfg_ipa_set_pdev_id);

void ucfg_ipa_set_dp_handle(struct wlan_objmgr_psoc *psoc,
				     void *dp_soc)
{
	return ipa_set_dp_handle(psoc, dp_soc);
}

qdf_export_symbol(ucfg_ipa_set_dp_handle);

QDF_STATUS ucfg_ipa_set_perf_level(struct wlan_objmgr_pdev *pdev,
				   uint64_t tx_packets, uint64_t rx_packets)
{
	return ipa_rm_set_perf_level(pdev, tx_packets, rx_packets);
}

qdf_export_symbol(ucfg_ipa_set_perf_level);

void ucfg_ipa_uc_info(struct wlan_objmgr_pdev *pdev)
{
	return ipa_uc_info(pdev);
}

qdf_export_symbol(ucfg_ipa_uc_info);

void ucfg_ipa_uc_stat(struct wlan_objmgr_pdev *pdev)
{
	return ipa_uc_stat(pdev);
}

qdf_export_symbol(ucfg_ipa_uc_stat);

void ucfg_ipa_uc_rt_debug_host_dump(struct wlan_objmgr_pdev *pdev)
{
	return ipa_uc_rt_debug_host_dump(pdev);
}

qdf_export_symbol(ucfg_ipa_uc_rt_debug_host_dump);

void ucfg_ipa_dump_info(struct wlan_objmgr_pdev *pdev)
{
	return ipa_dump_info(pdev);
}

qdf_export_symbol(ucfg_ipa_dump_info);

void ucfg_ipa_uc_stat_request(struct wlan_objmgr_pdev *pdev,
			      uint8_t reason)
{
	return ipa_uc_stat_request(pdev, reason);
}

qdf_export_symbol(ucfg_ipa_uc_stat_request);

void ucfg_ipa_uc_stat_query(struct wlan_objmgr_pdev *pdev,
			    uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff)
{
	return ipa_uc_stat_query(pdev, ipa_tx_diff, ipa_rx_diff);
}

qdf_export_symbol(ucfg_ipa_uc_stat_query);

void ucfg_ipa_reg_sap_xmit_cb(struct wlan_objmgr_pdev *pdev,
			      wlan_ipa_softap_xmit cb)
{
	return ipa_reg_sap_xmit_cb(pdev, cb);
}

qdf_export_symbol(ucfg_ipa_reg_sap_xmit_cb);

void ucfg_ipa_reg_send_to_nw_cb(struct wlan_objmgr_pdev *pdev,
				wlan_ipa_send_to_nw cb)
{
	return ipa_reg_send_to_nw_cb(pdev, cb);
}

qdf_export_symbol(ucfg_ipa_reg_send_to_nw_cb);

#if defined(QCA_CONFIG_RPS) && !defined(MDM_PLATFORM)
void ucfg_ipa_reg_rps_enable_cb(struct wlan_objmgr_pdev *pdev,
				wlan_ipa_rps_enable cb)
{
	return ipa_reg_rps_enable_cb(pdev, cb);
}

qdf_export_symbol(ucfg_ipa_reg_rps_enable_cb);
#endif

void ucfg_ipa_reg_is_driver_unloading_cb(struct wlan_objmgr_pdev *pdev,
					 wlan_ipa_driver_unloading cb)
{
	return ipa_reg_is_driver_unloading_cb(pdev, cb);
}

void ucfg_ipa_set_mcc_mode(struct wlan_objmgr_pdev *pdev, bool mcc_mode)
{
	return ipa_set_mcc_mode(pdev, mcc_mode);
}

qdf_export_symbol(ucfg_ipa_set_mcc_mode);

void ucfg_ipa_set_dfs_cac_tx(struct wlan_objmgr_pdev *pdev, bool tx_block)
{
	return ipa_set_dfs_cac_tx(pdev, tx_block);
}

qdf_export_symbol(ucfg_ipa_set_dfs_cac_tx);

void ucfg_ipa_set_ap_ibss_fwd(struct wlan_objmgr_pdev *pdev, uint8_t session_id,
			      bool intra_bss)
{
	return ipa_set_ap_ibss_fwd(pdev, session_id, intra_bss);
}

qdf_export_symbol(ucfg_ipa_set_ap_ibss_fwd);

void ucfg_ipa_uc_force_pipe_shutdown(struct wlan_objmgr_pdev *pdev)
{
	return ipa_uc_force_pipe_shutdown(pdev);
}

qdf_export_symbol(ucfg_ipa_uc_force_pipe_shutdown);

void ucfg_ipa_flush(struct wlan_objmgr_pdev *pdev)
{
	return ipa_flush(pdev);
}

qdf_export_symbol(ucfg_ipa_flush);

QDF_STATUS ucfg_ipa_suspend(struct wlan_objmgr_pdev *pdev)
{
	return ipa_suspend(pdev);
}

qdf_export_symbol(ucfg_ipa_suspend);

QDF_STATUS ucfg_ipa_resume(struct wlan_objmgr_pdev *pdev)
{
	return ipa_resume(pdev);
}

qdf_export_symbol(ucfg_ipa_resume);

QDF_STATUS ucfg_ipa_uc_ol_init(struct wlan_objmgr_pdev *pdev,
			       qdf_device_t osdev)
{
	return ipa_uc_ol_init(pdev, osdev);
}

qdf_export_symbol(ucfg_ipa_uc_ol_init);

QDF_STATUS ucfg_ipa_uc_ol_deinit(struct wlan_objmgr_pdev *pdev)
{
	return ipa_uc_ol_deinit(pdev);
}

qdf_export_symbol(ucfg_ipa_uc_ol_deinit);

bool ucfg_ipa_is_tx_pending(struct wlan_objmgr_pdev *pdev)
{
	return ipa_is_tx_pending(pdev);
}

qdf_export_symbol(ucfg_ipa_is_tx_pending);

QDF_STATUS ucfg_ipa_send_mcc_scc_msg(struct wlan_objmgr_pdev *pdev,
				     bool mcc_mode)
{
	return ipa_send_mcc_scc_msg(pdev, mcc_mode);
}

qdf_export_symbol(ucfg_ipa_send_mcc_scc_msg);

QDF_STATUS ucfg_ipa_wlan_evt(struct wlan_objmgr_pdev *pdev,
			     qdf_netdev_t net_dev, uint8_t device_mode,
			     uint8_t session_id,
			     enum wlan_ipa_wlan_event ipa_event_type,
			     const uint8_t *mac_addr, bool is_2g_iface)
{
	return ipa_wlan_evt(pdev, net_dev, device_mode, session_id,
			    ipa_event_type, mac_addr, is_2g_iface);
}

qdf_export_symbol(ucfg_ipa_wlan_evt);

int ucfg_ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	return ipa_uc_smmu_map(map, num_buf, buf_arr);
}

qdf_export_symbol(ucfg_ipa_uc_smmu_map);

bool ucfg_ipa_is_fw_wdi_activated(struct wlan_objmgr_pdev *pdev)
{
	return ipa_is_fw_wdi_activated(pdev);
}

qdf_export_symbol(ucfg_ipa_is_fw_wdi_activated);

void ucfg_ipa_uc_cleanup_sta(struct wlan_objmgr_pdev *pdev,
			     qdf_netdev_t net_dev, uint8_t session_id)
{
	return ipa_uc_cleanup_sta(pdev, net_dev, session_id);
}

qdf_export_symbol(ucfg_ipa_uc_cleanup_sta);

QDF_STATUS ucfg_ipa_uc_disconnect_ap(struct wlan_objmgr_pdev *pdev,
				     qdf_netdev_t net_dev)
{
	return ipa_uc_disconnect_ap(pdev, net_dev);
}

qdf_export_symbol(ucfg_ipa_uc_disconnect_ap);

void ucfg_ipa_cleanup_dev_iface(struct wlan_objmgr_pdev *pdev,
				qdf_netdev_t net_dev, uint8_t session_id)
{
	return ipa_cleanup_dev_iface(pdev, net_dev, session_id);
}

qdf_export_symbol(ucfg_ipa_cleanup_dev_iface);

void ucfg_ipa_uc_ssr_cleanup(struct wlan_objmgr_pdev *pdev)
{
	return ipa_uc_ssr_cleanup(pdev);
}

qdf_export_symbol(ucfg_ipa_uc_ssr_cleanup);

void ucfg_ipa_fw_rejuvenate_send_msg(struct wlan_objmgr_pdev *pdev)
{
	return ipa_fw_rejuvenate_send_msg(pdev);
}

qdf_export_symbol(ucfg_ipa_fw_rejuvenate_send_msg);

void ucfg_ipa_component_config_update(struct wlan_objmgr_psoc *psoc)
{
	ipa_component_config_update(psoc);
}

qdf_export_symbol(ucfg_ipa_component_config_update);

void ucfg_ipa_component_config_free(void)
{
	ipa_component_config_free();
}

qdf_export_symbol(ucfg_ipa_component_config_free);

uint32_t ucfg_ipa_get_tx_buf_count(void)
{
	return ipa_get_tx_buf_count();
}

qdf_export_symbol(ucfg_ipa_get_tx_buf_count);

void ucfg_ipa_update_tx_stats(struct wlan_objmgr_pdev *pdev, uint64_t sta_tx,
			      uint64_t ap_tx)
{
	ipa_update_tx_stats(pdev, sta_tx, ap_tx);
}

qdf_export_symbol(ucfg_ipa_update_tx_stats);

void ucfg_ipa_flush_pending_vdev_events(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id)
{
	ipa_flush_pending_vdev_events(pdev, vdev_id);
}

qdf_export_symbol(ucfg_ipa_flush_pending_vdev_events);

bool ucfg_ipa_is_wds_enabled(void)
{
	return ipa_is_wds_enabled();
}

qdf_export_symbol(ucfg_ipa_is_wds_enabled);

QDF_STATUS ucfg_ipa_get_alt_pipe(struct wlan_objmgr_pdev *pdev,
				 uint8_t vdev_id,
				 bool *alt_pipe)
{
	return ipa_get_alt_pipe(pdev, vdev_id, alt_pipe);
}

qdf_export_symbol(ucfg_ipa_get_alt_pipe);

bool ucfg_ipa_set_perf_level_bw_enabled(struct wlan_objmgr_pdev *pdev)
{
	return ipa_set_perf_level_bw_enabled(pdev);
}

qdf_export_symbol(ucfg_ipa_set_perf_level_bw_enabled);

void ucfg_ipa_set_perf_level_bw(struct wlan_objmgr_pdev *pdev,
				enum wlan_ipa_bw_level lvl)
{
	ipa_set_perf_level_bw(pdev, lvl);
}

qdf_export_symbol(ucfg_ipa_set_perf_level_bw);
