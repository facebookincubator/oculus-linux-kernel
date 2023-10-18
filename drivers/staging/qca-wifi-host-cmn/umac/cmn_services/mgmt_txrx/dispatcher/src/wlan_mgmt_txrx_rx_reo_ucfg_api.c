/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_mgmt_txrx_rx_reo_ucfg_api.c
 * This file contains mgmt rx re-ordering ucfg layer related
 * function definitions
 */

#include <wlan_mgmt_txrx_rx_reo_ucfg_api.h>
#include "../../core/src/wlan_mgmt_txrx_rx_reo_i.h"
#include <cfg_ucfg_api.h>

QDF_STATUS
ucfg_wlan_mgmt_rx_reo_sim_start(uint8_t ml_grp_id)
{
	QDF_STATUS status;

	if (!wlan_mgmt_rx_reo_is_simulation_in_progress(ml_grp_id)) {
		mgmt_rx_reo_debug("Starting rx reo simulation");
		status = wlan_mgmt_rx_reo_sim_start(ml_grp_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			mgmt_rx_reo_err("Failed to start rx reo sim");
			return status;
		}
	}

	mgmt_rx_reo_debug("Simulation is running");
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(ucfg_wlan_mgmt_rx_reo_sim_start);

QDF_STATUS
ucfg_wlan_mgmt_rx_reo_sim_stop(uint8_t ml_grp_id)
{
	QDF_STATUS status;

	if (wlan_mgmt_rx_reo_is_simulation_in_progress(ml_grp_id)) {
		mgmt_rx_reo_debug("Stopping simulation");
		status = wlan_mgmt_rx_reo_sim_stop(ml_grp_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			mgmt_rx_reo_err("Failed to stop rx reo sim");
			return status;
		}
	}

	mgmt_rx_reo_debug("Simulation is not running");
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(ucfg_wlan_mgmt_rx_reo_sim_stop);

bool
ucfg_wlan_mgmt_rx_reo_is_simulation_in_progress(uint8_t ml_grp_id)
{
	return wlan_mgmt_rx_reo_is_simulation_in_progress(ml_grp_id);
}

qdf_export_symbol(ucfg_wlan_mgmt_rx_reo_is_simulation_in_progress);
