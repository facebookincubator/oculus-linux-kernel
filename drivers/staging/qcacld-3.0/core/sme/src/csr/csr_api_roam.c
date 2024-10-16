/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/*
 * DOC: csr_api_roam.c
 *
 * Implementation for the Common Roaming interfaces.
 */
#include "ani_global.h"          /* for struct mac_context **/
#include "wma_types.h"
#include "wma_if.h"          /* for STA_INVALID_IDX. */
#include "csr_inside_api.h"
#include <include/wlan_psoc_mlme.h>
#include "sme_trace.h"
#include "sme_qos_internal.h"
#include "sme_inside.h"
#include "host_diag_core_event.h"
#include "host_diag_core_log.h"
#include "csr_api.h"
#include "csr_internal.h"
#include "cds_reg_service.h"
#include "mac_trace.h"
#include "cds_regdomain.h"
#include "cds_utils.h"
#include "sir_types.h"
#include "cfg_ucfg_api.h"
#include "sme_power_save_api.h"
#include "wma.h"
#include "wlan_policy_mgr_api.h"
#include "sme_nan_datapath.h"
#include "pld_common.h"
#include "wlan_reg_services_api.h"
#include "qdf_crypto.h"
#include <wlan_logging_sock_svc.h>
#include "wlan_objmgr_psoc_obj.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_tdls_tgt_api.h>
#include <wlan_cfg80211_scan.h>
#include <wlan_scan_public_structs.h>
#include <wlan_action_oui_public_struct.h>
#include <wlan_action_oui_ucfg_api.h>
#include "wlan_mlme_api.h"
#include "wlan_mlme_ucfg_api.h"
#include <wlan_utility.h>
#include "cfg_mlme.h"
#include "wlan_mlme_public_struct.h"
#include <wlan_crypto_global_api.h>
#include "wlan_qct_sys.h"
#include "wlan_dlm_api.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_scan_utils_api.h"
#include "wlan_p2p_cfg_api.h"
#include "cfg_nan_api.h"
#include "nan_ucfg_api.h"
#include <../../core/src/wlan_cm_vdev_api.h>
#include "wlan_reg_ucfg_api.h"

#include <ol_defines.h>
#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_if_mgr_ucfg_api.h"
#include "wlan_if_mgr_roam.h"
#include "wlan_roam_debug.h"
#include "wlan_cm_roam_public_struct.h"
#include "wlan_mlme_twt_api.h"
#include <wlan_serialization_api.h>
#include <wlan_vdev_mlme_ser_if.h>
#include "wlan_mlo_mgr_sta.h"
#include "wlan_mlo_mgr_roam.h"

#define RSN_AUTH_KEY_MGMT_SAE           WLAN_RSN_SEL(WLAN_AKM_SAE)
#define MAX_PWR_FCC_CHAN_12 8
#define MAX_PWR_FCC_CHAN_13 2
#define MAX_CB_VALUE_IN_INI (2)

#define MAX_SOCIAL_CHANNELS  3

/* packet dump timer duration of 60 secs */
#define PKT_DUMP_TIMER_DURATION 60

#ifdef WLAN_FEATURE_SAE
/**
 * csr_sae_callback - Update SAE info to CSR roam session
 * @mac_ctx: MAC context
 * @msg_ptr: pointer to SAE message
 *
 * API to update SAE info to roam csr session
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_sae_callback(struct mac_context *mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	struct csr_roam_info *roam_info;
	uint32_t session_id;
	struct sir_sae_info *sae_info;

	sae_info = (struct sir_sae_info *) msg_ptr;
	if (!sae_info) {
		sme_err("SAE info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("vdev_id %d "QDF_MAC_ADDR_FMT,
		sae_info->vdev_id,
		QDF_MAC_ADDR_REF(sae_info->peer_mac_addr.bytes));

	session_id = sae_info->vdev_id;
	if (session_id == WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_INVAL;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_FAILURE;

	roam_info->sae_info = sae_info;

	csr_roam_call_callback(mac_ctx, session_id, roam_info,
			       eCSR_ROAM_SAE_COMPUTE, eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS csr_sae_callback(struct mac_context *mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static const uint32_t
social_channel_freq[MAX_SOCIAL_CHANNELS] = { 2412, 2437, 2462 };

static void init_config_param(struct mac_context *mac);
static QDF_STATUS csr_roam_open(struct mac_context *mac);
static QDF_STATUS csr_roam_close(struct mac_context *mac);
static QDF_STATUS csr_init11d_info(struct mac_context *mac, tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_init_channel_power_list(struct mac_context *mac,
					      tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_roam_free_connected_info(struct mac_context *mac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo);
static void csr_init_session(struct mac_context *mac, uint32_t sessionId);

static void csr_init_operating_classes(struct mac_context *mac);

static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan);
static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan);

#ifdef WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY
static struct csr_roam_session *csr_roam_roam_session;

/* Allocate and initialize global variables */
static QDF_STATUS csr_roam_init_globals(struct mac_context *mac)
{
	uint32_t buf_size;
	QDF_STATUS status;

	buf_size = WLAN_MAX_VDEVS * sizeof(struct csr_roam_session);

	csr_roam_roam_session = qdf_mem_malloc(buf_size);
	if (csr_roam_roam_session) {
		mac->roam.roamSession = csr_roam_roam_session;
		status = QDF_STATUS_SUCCESS;
	} else {
		status = QDF_STATUS_E_NOMEM;
	}

	return status;
}

/* Free memory allocated dynamically */
static inline void csr_roam_free_globals(void)
{
	qdf_mem_free(csr_roam_roam_session);
	csr_roam_roam_session = NULL;
}

#else /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */
static struct csr_roam_session csr_roam_roam_session[WLAN_MAX_VDEVS];

/* Initialize global variables */
static QDF_STATUS csr_roam_init_globals(struct mac_context *mac)
{
	qdf_mem_zero(&csr_roam_roam_session,
		     sizeof(csr_roam_roam_session));
	mac->roam.roamSession = csr_roam_roam_session;

	return QDF_STATUS_SUCCESS;
}

static inline void csr_roam_free_globals(void)
{
}
#endif /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */

static void csr_roam_de_init_globals(struct mac_context *mac)
{
	csr_roam_free_globals();
	mac->roam.roamSession = NULL;
}

/**
 * csr_roam_lost_link - Process lost link indication
 * @mac: MAC context
 * @session_id: Session id of the current session
 * @type: Type of the indication
 * @sir_msg: sme response message
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS csr_roam_lost_link(struct mac_context *mac, uint32_t session_id,
			      uint32_t type, tSirSmeRsp *sir_msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct deauth_ind *deauth_ind_msg = NULL;
	struct disassoc_ind *disassoc_ind_msg = NULL;

	sme_debug("vdev_id %d type %d ", session_id, type);

	if (type == eWNI_SME_DISASSOC_IND) {
		disassoc_ind_msg = (struct disassoc_ind *)sir_msg;
		status = csr_send_mb_disassoc_cnf_msg(mac, disassoc_ind_msg);
	} else if (type == eWNI_SME_DEAUTH_IND) {
		deauth_ind_msg = (struct deauth_ind *)sir_msg;
		status = csr_send_mb_deauth_cnf_msg(mac, deauth_ind_msg);
	}

	return status;
}

/**
 * csr_process_deauth_disassoc_cmd - Process deauth/disassoc command
 * @mac: MAC context
 * @sme_cmd: sme command which needs to be processed
 *
 * This function processes deauth or disassoc command
 *
 * Return: None
 */
static void csr_process_deauth_disassoc_cmd(struct mac_context *mac,
					    tSmeCmd *sme_cmd)
{
	if (sme_cmd->u.roamCmd.roamReason == eCsrForcedDisassocSta) {
		sme_debug("Disassoc vdev_id %d with reason: %d peer "
			  QDF_MAC_ADDR_FMT, sme_cmd->vdev_id,
			  sme_cmd->u.roamCmd.reason,
			  QDF_MAC_ADDR_REF(sme_cmd->u.roamCmd.peerMac));
		csr_send_mb_disassoc_req_msg(mac, sme_cmd->vdev_id,
					     sme_cmd->u.roamCmd.peerMac,
					     sme_cmd->u.roamCmd.reason);
	} else if (sme_cmd->u.roamCmd.roamReason == eCsrForcedDeauthSta) {
		sme_debug("Deauth vdev_id %d with reason: %d peer "
			  QDF_MAC_ADDR_FMT, sme_cmd->vdev_id,
			  sme_cmd->u.roamCmd.reason,
			  QDF_MAC_ADDR_REF(sme_cmd->u.roamCmd.peerMac));
		csr_send_mb_deauth_req_msg(mac, sme_cmd->vdev_id,
					   sme_cmd->u.roamCmd.peerMac,
					   sme_cmd->u.roamCmd.reason);
	} else {
		sme_info("Invalid command, vdev_id %d reason: %d peer "
			  QDF_MAC_ADDR_FMT, sme_cmd->vdev_id,
			  sme_cmd->u.roamCmd.reason,
			  QDF_MAC_ADDR_REF(sme_cmd->u.roamCmd.peerMac));
	}
}

/**
 * csr_process_wmm_status_change_cmd - Process wmm status change command
 * @mac: MAC context
 * @sme_cmd: sme command which needs to be processed
 *
 * Return: None
 */
static void csr_process_wmm_status_change_cmd(struct mac_context *mac,
					      tSmeCmd *sme_cmd)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tSirSmeRsp *sir_msg;

	if (sme_cmd->u.wmStatusChangeCmd.Type == eCsrDisassociated) {
		sir_msg =
			(tSirSmeRsp *)&sme_cmd->u.wmStatusChangeCmd.u.
			DisassocIndMsg;
		status = csr_roam_lost_link(mac, sme_cmd->vdev_id,
					    eWNI_SME_DISASSOC_IND,
					    sir_msg);
	} else if (sme_cmd->u.wmStatusChangeCmd.Type == eCsrDeauthenticated) {
		sir_msg = (tSirSmeRsp *)&sme_cmd->u.wmStatusChangeCmd.u.
								DeauthIndMsg;
		status = csr_roam_lost_link(mac, sme_cmd->vdev_id,
					    eWNI_SME_DEAUTH_IND,
					    sir_msg);
	}
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_info("Failed to issue lost link command, status %d",
			 status);
		/*
		 * As status returned is not success, there is nothing else
		 * left to do so release WM status change command here.
		 */
		csr_roam_wm_status_change_complete(mac, sme_cmd->vdev_id);
	}
}

/**
 * csr_get_active_peer_disconnect_command - Get active peer disconnect command
 * from serialization.
 * @mac: MAC context
 * @vdev_id: Vdev id for which active command needs to be return
 *
 * Return: None
 */
static tSmeCmd *csr_get_active_peer_disconnect_command(struct mac_context *mac,
						       uint8_t vdev_id)
{
	tSmeCmd *sme_cmd;

	sme_cmd = wlan_serialization_get_active_cmd(
						mac->psoc, vdev_id,
						WLAN_SER_CMD_FORCE_DEAUTH_STA);
	if (sme_cmd)
		return sme_cmd;

	sme_cmd = wlan_serialization_get_active_cmd(
					mac->psoc, vdev_id,
					WLAN_SER_CMD_FORCE_DISASSOC_STA);
	if (sme_cmd)
		return sme_cmd;

	return wlan_serialization_get_active_cmd(mac->psoc, vdev_id,
						 WLAN_SER_CMD_WM_STATUS_CHANGE);
}

/**
 * csr_continue_peer_disconnect_after_get_stats - Continue peer disconnect after
 * getting peer disconnect stats
 * @mac: MAC context
 *
 * Process the active serialization command after getting disconnect peer
 * stats or after peer stats request gets timed out
 *
 * Return: None
 */
static void
csr_continue_peer_disconnect_after_get_stats(struct mac_context *mac)
{
	tSmeCmd *sme_cmd;
	QDF_STATUS status;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj) {
		sme_err("NULL mlme psoc object");
		return;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("can't acquire sme global lock");
		return;
	}

	sme_cmd = csr_get_active_peer_disconnect_command(
				mac,
				mlme_obj->disconnect_stats_param.vdev_id);
	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		goto release_lock;
	}

	if (qdf_atomic_inc_return(
		&mlme_obj->disconnect_stats_param.is_disconn_stats_completed) >
		1) {
		sme_info("Command %d already in process", sme_cmd->command);
		goto release_lock;
	}

	switch (sme_cmd->command) {
	case eSmeCommandRoam:
		csr_process_deauth_disassoc_cmd(mac, sme_cmd);
		break;

	case eSmeCommandWmStatusChange:
		csr_process_wmm_status_change_cmd(mac, sme_cmd);
		break;
	default:
		sme_info("Invalid command %d vdev_id %d", sme_cmd->command,
			 mlme_obj->disconnect_stats_param.vdev_id);
	}
release_lock:
	sme_release_global_lock(&mac->sme);
}

/**
 * csr_disconnect_stats_timer_cb - Disconnect stats timer callback
 * @user_data: Void pointer to mac context
 *
 * Return: None
 */
static void csr_disconnect_stats_timer_cb(void *user_data)
{
	struct mac_context *mac = (struct mac_context *)user_data;

	if (!mac) {
		sme_err("Invalid mac ctx");
		return;
	}

	sme_debug("Disconnect peer stats timed out");
	csr_continue_peer_disconnect_after_get_stats(mac);
}

/**
 * csr_init_disconnect_stats_timer - Init Disconnect stats timer
 * @mac: MAC context
 *
 * Return: None
 */
static void csr_init_disconnect_stats_timer(struct mac_context *mac)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj) {
		sme_err("NULL mlme psoc object");
		return;
	}

	qdf_mc_timer_init(&mlme_obj->disconnect_stats_param.disconn_stats_timer,
			  QDF_TIMER_TYPE_SW,
			  csr_disconnect_stats_timer_cb, mac);

	qdf_atomic_init(
		&mlme_obj->disconnect_stats_param.is_disconn_stats_completed);
}

/**
 * csr_deinit_disconnect_stats_timer - Deinit Disconnect stats timer
 * @mac: MAC context
 *
 * Return: None
 */
static void csr_deinit_disconnect_stats_timer(struct mac_context *mac)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj) {
		sme_err("NULL mlme psoc object");
		return;
	}

	if (QDF_TIMER_STATE_RUNNING ==
		qdf_mc_timer_get_current_state(
			&mlme_obj->disconnect_stats_param.disconn_stats_timer))
		qdf_mc_timer_stop(
			&mlme_obj->disconnect_stats_param.disconn_stats_timer);

	qdf_mc_timer_destroy(
			&mlme_obj->disconnect_stats_param.disconn_stats_timer);
}

QDF_STATUS csr_open(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		/* Initialize CSR Roam Globals */
		status = csr_roam_init_globals(mac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			csr_roam_state_change(mac, eCSR_ROAMING_STATE_STOP, i);

		init_config_param(mac);
		status = csr_scan_open(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_free_globals();
			break;
		}
		status = csr_roam_open(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_free_globals();
			break;
		}
		csr_init_disconnect_stats_timer(mac);
	} while (0);

	return status;
}

QDF_STATUS csr_init_chan_list(struct mac_context *mac)
{
	QDF_STATUS status;
	uint8_t reg_cc[REG_ALPHA2_LEN + 1];

	wlan_reg_read_current_country(mac->psoc, reg_cc);
	sme_debug("init time country code %.2s", reg_cc);

	status = csr_get_channel_and_power_list(mac);

	return status;
}

QDF_STATUS csr_set_channels(struct mac_context *mac,
			    struct csr_config_params *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t index = 0;

	for (index = 0; index < mac->scan.base_channels.numChannels;
	     index++) {
		pParam->Csr11dinfo.Channels.channel_freq_list[index] =
			mac->scan.base_channels.channel_freq_list[index];
		pParam->Csr11dinfo.ChnPower[index].first_chan_freq =
			mac->scan.base_channels.channel_freq_list[index];
		pParam->Csr11dinfo.ChnPower[index].numChannels = 1;
		pParam->Csr11dinfo.ChnPower[index].maxtxPower =
			mac->scan.defaultPowerTable[index].tx_power;
	}
	pParam->Csr11dinfo.Channels.numChannels =
		mac->scan.base_channels.numChannels;

	return status;
}

QDF_STATUS csr_close(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_deinit_disconnect_stats_timer(mac);

	csr_roam_close(mac);
	csr_scan_close(mac);
	/* DeInit Globals */
	csr_roam_de_init_globals(mac);
	return status;
}

static int8_t
csr_find_channel_pwr(struct channel_power *pdefaultPowerTable,
		     uint32_t chan_freq)
{
	uint8_t i;
	/* TODO: if defaultPowerTable is guaranteed to be in ascending */
	/* order of channel numbers, we can employ binary search */
	for (i = 0; i < CFG_VALID_CHANNEL_LIST_LEN; i++) {
		if (pdefaultPowerTable[i].center_freq == chan_freq)
			return pdefaultPowerTable[i].tx_power;
	}
	/* could not find the channel list in default list */
	/* this should not have occurred */
	QDF_ASSERT(0);
	return 0;
}

/**
 * csr_roam_arrange_ch_list() - Updates the channel list modified with greedy
 * order for 5 Ghz preference and DFS channels.
 * @mac_ctx: pointer to mac context.
 * @chan_list:    channel list updated with greedy channel order.
 * @num_channel:  Number of channels in list
 *
 * To allow Early Stop Roaming Scan feature to co-exist with 5G preference,
 * this function moves 5G channels ahead of 2G channels. This function can
 * also move 2G channels, ahead of DFS channel or vice versa. Order is
 * maintained among same category channels
 *
 * Return: None
 */
static void csr_roam_arrange_ch_list(struct mac_context *mac_ctx,
			tSirUpdateChanParam *chan_list, uint8_t num_channel)
{
	bool prefer_5g = CSR_IS_ROAM_PREFER_5GHZ(mac_ctx);
	bool prefer_dfs = CSR_IS_DFS_CH_ROAM_ALLOWED(mac_ctx);
	int i, j = 0;
	tSirUpdateChanParam *tmp_list = NULL;

	if (!prefer_5g)
		return;

	tmp_list = qdf_mem_malloc(sizeof(tSirUpdateChanParam) * num_channel);
	if (!tmp_list)
		return;

	/* Fist copy Non-DFS 5g channels */
	for (i = 0; i < num_channel; i++) {
		if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_list[i].freq) &&
			!wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						  chan_list[i].freq)) {
			qdf_mem_copy(&tmp_list[j++],
				&chan_list[i], sizeof(tSirUpdateChanParam));
			chan_list[i].freq = 0;
		}
	}
	if (prefer_dfs) {
		/* next copy DFS channels (remaining channels in 5G) */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_list[i].freq)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].freq = 0;
			}
		}
	} else {
		/* next copy 2G channels */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_list[i].freq)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].freq = 0;
			}
		}
	}
	/* copy rest of the channels in same order to tmp list */
	for (i = 0; i < num_channel; i++) {
		if (chan_list[i].freq) {
			qdf_mem_copy(&tmp_list[j++], &chan_list[i],
				sizeof(tSirUpdateChanParam));
			chan_list[i].freq = 0;
		}
	}
	/* copy tmp list to original channel list buffer */
	qdf_mem_copy(chan_list, tmp_list,
				 sizeof(tSirUpdateChanParam) * num_channel);
	qdf_mem_free(tmp_list);
}

/**
 * csr_roam_sort_channel_for_early_stop() - Sort the channels
 * @mac_ctx:        mac global context
 * @chan_list:      Original channel list from the upper layers
 * @num_channel:    Number of original channels
 *
 * For Early stop scan feature, the channel list should be in an order,
 * where-in there is a maximum chance to detect an AP in the initial
 * channels in the list so that the scanning can be stopped early as the
 * feature demands.
 * Below fixed greedy channel list has been provided
 * based on most of the enterprise wifi installations across the globe.
 *
 * Identify all the greedy channels within the channel list from user space.
 * Identify all the non-greedy channels in the user space channel list.
 * Merge greedy channels followed by non-greedy channels back into the
 * chan_list.
 *
 * Return: None
 */
static void csr_roam_sort_channel_for_early_stop(struct mac_context *mac_ctx,
			tSirUpdateChanList *chan_list, uint8_t num_channel)
{
	tSirUpdateChanList *chan_list_greedy, *chan_list_non_greedy;
	uint8_t i, j;
	static const uint32_t fixed_greedy_freq_list[] = {2412, 2437, 2462,
		5180, 5240, 5200, 5220, 2457, 2417, 2452, 5745, 5785, 5805,
		2422, 2427, 2447, 5765, 5825, 2442, 2432, 5680, 5700, 5260,
		5580, 5280, 5520, 5320, 5300, 5500, 5600, 2472, 2484, 5560,
		5660, 5755, 5775};
	uint8_t num_fixed_greedy_chan;
	uint8_t num_greedy_chan = 0;
	uint8_t num_non_greedy_chan = 0;
	uint8_t match_found = false;
	uint32_t buf_size;

	buf_size = sizeof(tSirUpdateChanList) +
		(sizeof(tSirUpdateChanParam) * num_channel);
	chan_list_greedy = qdf_mem_malloc(buf_size);
	chan_list_non_greedy = qdf_mem_malloc(buf_size);
	if (!chan_list_greedy || !chan_list_non_greedy)
		goto scan_list_sort_error;
	/*
	 * fixed_greedy_freq_list is an evaluated freq list based on most of
	 * the enterprise wifi deployments and the order of the channels
	 * determines the highest possibility of finding an AP.
	 * chan_list is the channel list provided by upper layers based on the
	 * regulatory domain.
	 */
	num_fixed_greedy_chan = sizeof(fixed_greedy_freq_list) /
							sizeof(uint32_t);
	/*
	 * Browse through the chan_list and put all the non-greedy channels
	 * into a separate list by name chan_list_non_greedy
	 */
	for (i = 0; i < num_channel; i++) {
		for (j = 0; j < num_fixed_greedy_chan; j++) {
			if (chan_list->chanParam[i].freq ==
					 fixed_greedy_freq_list[j]) {
				match_found = true;
				break;
			}
		}
		if (!match_found) {
			qdf_mem_copy(
			  &chan_list_non_greedy->chanParam[num_non_greedy_chan],
			  &chan_list->chanParam[i],
			  sizeof(tSirUpdateChanParam));
			num_non_greedy_chan++;
		} else {
			match_found = false;
		}
	}
	/*
	 * Browse through the fixed_greedy_chan_list and put all the greedy
	 * channels in the chan_list into a separate list by name
	 * chan_list_greedy
	 */
	for (i = 0; i < num_fixed_greedy_chan; i++) {
		for (j = 0; j < num_channel; j++) {
			if (fixed_greedy_freq_list[i] ==
				chan_list->chanParam[j].freq) {
				qdf_mem_copy(
				  &chan_list_greedy->chanParam[num_greedy_chan],
				  &chan_list->chanParam[j],
				  sizeof(tSirUpdateChanParam));
				num_greedy_chan++;
				break;
			}
		}
	}
	sme_debug("greedy=%d, non-greedy=%d, tot=%d", num_greedy_chan,
		  num_non_greedy_chan, num_channel);
	if ((num_greedy_chan + num_non_greedy_chan) != num_channel) {
		sme_err("incorrect sorting of channels");
		goto scan_list_sort_error;
	}
	/* Copy the Greedy channels first */
	i = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_greedy->chanParam[i],
		num_greedy_chan * sizeof(tSirUpdateChanParam));
	/* Copy the remaining Non Greedy channels */
	i = num_greedy_chan;
	j = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_non_greedy->chanParam[j],
		num_non_greedy_chan * sizeof(tSirUpdateChanParam));

	/* Update channel list for 5g preference and allow DFS roam */
	csr_roam_arrange_ch_list(mac_ctx, chan_list->chanParam, num_channel);
scan_list_sort_error:
	qdf_mem_free(chan_list_greedy);
	qdf_mem_free(chan_list_non_greedy);
}

/**
 * csr_emu_chan_req() - update the required channel list for emulation
 * @channel: channel number to check
 *
 * To reduce scan time during emulation platforms, this function
 * restricts the scanning to be done on selected channels
 *
 * Return: QDF_STATUS enumeration
 */
#ifdef QCA_WIFI_EMULATION
#define SCAN_CHAN_LIST_5G_LEN 6
#define SCAN_CHAN_LIST_2G_LEN 3
static const uint16_t
csr_scan_chan_list_5g[SCAN_CHAN_LIST_5G_LEN] = { 5180, 5220, 5260, 5280, 5700, 5745 };
static const uint16_t
csr_scan_chan_list_2g[SCAN_CHAN_LIST_2G_LEN] = { 2412, 2437, 2462 };
static QDF_STATUS csr_emu_chan_req(uint32_t channel)
{
	int i;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_2g); i++) {
			if (csr_scan_chan_list_2g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	} else if (WLAN_REG_IS_5GHZ_CH_FREQ(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_5g); i++) {
			if (csr_scan_chan_list_5g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}
#else
static QDF_STATUS csr_emu_chan_req(uint32_t channel_num)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add len of social channels, before adding - num_chan:%hu",
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state_for_pwrmode(
				mac->pdev, social_channel_freq[i],
				REG_CURRENT_PWR_MODE) ==
					CHANNEL_STATE_ENABLE)
				no_chan++;
		}
	}
	*num_chan = no_chan;
	sme_debug("after adding - num_chan:%hu", *num_chan);
}

static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add social channels chan_list %pK, num_chan %hu", chan_list,
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state_for_pwrmode(
					mac->pdev, social_channel_freq[i],
					REG_CURRENT_PWR_MODE) !=
					CHANNEL_STATE_ENABLE)
				continue;
			chan_list->chanParam[no_chan].freq =
				social_channel_freq[i];
			chan_list->chanParam[no_chan].pwr =
				csr_find_channel_pwr(pScan->defaultPowerTable,
						social_channel_freq[i]);
			chan_list->chanParam[no_chan].dfsSet = false;
			if (cds_is_5_mhz_enabled())
				chan_list->chanParam[no_chan].quarter_rate
					= 1;
			else if (cds_is_10_mhz_enabled())
				chan_list->chanParam[no_chan].half_rate = 1;
			no_chan++;
		}
		sme_debug("after adding -num_chan %hu", no_chan);
	}
	*num_chan = no_chan;
}
#else
static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan)
{
	sme_debug("skip adding len of social channels");
}
static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	sme_debug("skip social channels");
}
#endif

/**
 * csr_scan_event_handler() - callback for scan event
 * @vdev: wlan objmgr vdev pointer
 * @event: scan event
 * @arg: global mac context pointer
 *
 * Return: void
 */
static void csr_scan_event_handler(struct wlan_objmgr_vdev *vdev,
					    struct scan_event *event,
					    void *arg)
{
	bool success = false;
	QDF_STATUS lock_status;
	struct mac_context *mac = arg;

	if (!mac)
		return;

	if (!util_is_scan_completed(event, &success))
		return;

	lock_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return;

	if (mac->scan.pending_channel_list_req)
		csr_update_channel_list(mac);
	sme_release_global_lock(&mac->sme);
}

/**
 * csr_update_roam_pcl_per_connected_sta_vdev() - Update roam pcl per connected
 *                                                STA
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
static void csr_update_roam_pcl_per_connected_sta_vdev(
						struct wlan_objmgr_psoc *psoc)
{
	struct wlan_objmgr_vdev *vdev;
	uint32_t vdev_id;

	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {
		vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						     WLAN_LEGACY_SME_ID);

		if (!vdev)
			continue;

		if (vdev->vdev_mlme.vdev_opmode != QDF_STA_MODE)
			goto next;

		if (!wlan_cm_is_vdev_connected(vdev))
			goto next;

		policy_mgr_set_pcl_for_existing_combo(psoc, PM_STA_MODE,
						      vdev_id);
next:
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	}
}

QDF_STATUS csr_update_channel_list(struct mac_context *mac)
{
	tSirUpdateChanList *pChanList;
	struct csr_scanstruct *pScan = &mac->scan;
	uint8_t numChan = pScan->base_channels.numChannels;
	uint8_t num_channel = 0;
	uint32_t bufLen;
	struct scheduler_msg msg = {0};
	uint8_t i;
	uint8_t channel_state;
	uint16_t unsafe_chan[NUM_CHANNELS];
	uint16_t unsafe_chan_cnt = 0;
	uint16_t cnt = 0;
	uint32_t  channel_freq;
	bool is_unsafe_chan;
	bool is_same_band;
	bool is_5mhz_enabled;
	bool is_10mhz_enabled;
	enum scm_scan_status scan_status;
	QDF_STATUS lock_status;
	struct rso_roam_policy_params *roam_policy;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx)
		return QDF_STATUS_E_FAILURE;

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;
	roam_policy = &mlme_obj->cfg.lfr.rso_user_config.policy_params;
	lock_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return lock_status;

	if (mac->mlme_cfg->reg.enable_pending_chan_list_req) {
		scan_status = wlan_get_pdev_status(mac->pdev);
		if (scan_status == SCAN_IS_ACTIVE ||
		    scan_status == SCAN_IS_ACTIVE_AND_PENDING) {
			mac->scan.pending_channel_list_req = true;
			sme_release_global_lock(&mac->sme);
			sme_debug("scan in progress postpone channel list req ");
			return QDF_STATUS_SUCCESS;
		}
		mac->scan.pending_channel_list_req = false;
	}
	sme_release_global_lock(&mac->sme);

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
		    &unsafe_chan_cnt,
		    sizeof(unsafe_chan));

	csr_add_len_of_social_channels(mac, &numChan);

	bufLen = sizeof(tSirUpdateChanList) +
		 (sizeof(tSirUpdateChanParam) * (numChan));

	csr_init_operating_classes(mac);
	pChanList = qdf_mem_malloc(bufLen);
	if (!pChanList)
		return QDF_STATUS_E_NOMEM;

	is_5mhz_enabled = cds_is_5_mhz_enabled();
	if (is_5mhz_enabled)
		sme_nofl_debug("quarter_rate enabled");
	is_10mhz_enabled = cds_is_10_mhz_enabled();
	if (is_10mhz_enabled)
		sme_nofl_debug("half_rate enabled");

	for (i = 0; i < pScan->base_channels.numChannels; i++) {
		if (QDF_STATUS_SUCCESS !=
			csr_emu_chan_req(pScan->base_channels.channel_freq_list[i]))
			continue;

		channel_freq = pScan->base_channels.channel_freq_list[i];
		/* Scan is not performed on DSRC channels*/
		if (wlan_reg_is_dsrc_freq(channel_freq))
			continue;

		channel_state =
			wlan_reg_get_channel_state_for_pwrmode(
				mac->pdev, channel_freq,
				REG_CURRENT_PWR_MODE);
		if ((CHANNEL_STATE_ENABLE == channel_state) ||
		    mac->scan.fEnableDFSChnlScan) {
			if (wlan_reg_is_6ghz_chan_freq(channel_freq) &&
			    !wlan_reg_is_6ghz_band_set(mac->pdev)) {
				sme_debug("skip 6ghz frequency %d",
					  channel_freq);
				continue;
			}
			if ((roam_policy->dfs_mode ==
				STA_ROAM_POLICY_DFS_DISABLED) &&
				(channel_state == CHANNEL_STATE_DFS)) {
				sme_debug("skip dfs channel frequency %d",
					  channel_freq);
				continue;
			}
			if (roam_policy->skip_unsafe_channels &&
			    unsafe_chan_cnt) {
				is_unsafe_chan = false;
				for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
					if (unsafe_chan[cnt] == channel_freq) {
						is_unsafe_chan = true;
						break;
					}
				}
				is_same_band =
					(WLAN_REG_IS_24GHZ_CH_FREQ(
							channel_freq) &&
					roam_policy->sap_operating_band ==
							BAND_2G) ||
					(WLAN_REG_IS_5GHZ_CH_FREQ(
							channel_freq) &&
					roam_policy->sap_operating_band ==
							BAND_5G);
				if (is_unsafe_chan && is_same_band) {
					sme_debug("ignoring unsafe channel freq %d",
						  channel_freq);
					continue;
				}
			}
			pChanList->chanParam[num_channel].freq =
				pScan->base_channels.channel_freq_list[i];
			pChanList->chanParam[num_channel].pwr =
				csr_find_channel_pwr(
				pScan->defaultPowerTable,
				pScan->base_channels.channel_freq_list[i]);

			if (pScan->fcc_constraint) {
				if (2467 ==
					pScan->base_channels.channel_freq_list[i]) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_12;
					sme_debug("txpow for channel 12 is %d",
						  MAX_PWR_FCC_CHAN_12);
				}
				if (2472 ==
					pScan->base_channels.channel_freq_list[i]) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_13;
					sme_debug("txpow for channel 13 is %d",
						  MAX_PWR_FCC_CHAN_13);
				}
			}

			if (!ucfg_is_nan_allowed_on_freq(mac->pdev,
				pChanList->chanParam[num_channel].freq))
				pChanList->chanParam[num_channel].nan_disabled =
					true;

			if (CHANNEL_STATE_DFS == channel_state)
				pChanList->chanParam[num_channel].dfsSet =
					true;

			pChanList->chanParam[num_channel].quarter_rate =
							is_5mhz_enabled;

			pChanList->chanParam[num_channel].half_rate =
							is_10mhz_enabled;

			num_channel++;
		}
	}

	csr_add_social_channels(mac, pChanList, pScan, &num_channel);

	if (mac->mlme_cfg->lfr.early_stop_scan_enable)
		csr_roam_sort_channel_for_early_stop(mac, pChanList,
						     num_channel);
	else
		sme_debug("Early Stop Scan Feature not supported");

	if ((mac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC_ONLY)) {
		pChanList->vht_en = true;
		if (mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band)
			pChanList->vht_24_en = true;
	}
	if ((mac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N_ONLY)) {
		pChanList->ht_en = true;
	}
	if ((mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (mac->roam.configParam.uCfgDot11Mode ==
	     eCSR_CFG_DOT11_MODE_11AX_ONLY))
		pChanList->he_en = true;
#ifdef WLAN_FEATURE_11BE
	if ((mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    CSR_IS_CFG_DOT11_PHY_MODE_11BE(
		mac->roam.configParam.uCfgDot11Mode) ||
	    CSR_IS_CFG_DOT11_PHY_MODE_11BE_ONLY(
		mac->roam.configParam.uCfgDot11Mode))
		pChanList->eht_en = true;
#endif

	pChanList->numChan = num_channel;
	mlme_store_fw_scan_channels(mac->psoc, pChanList);

	msg.type = WMA_UPDATE_CHAN_LIST_REQ;
	msg.reserved = 0;
	msg.bodyptr = pChanList;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		qdf_mem_free(pChanList);
		return QDF_STATUS_E_FAILURE;
	}

	csr_update_roam_pcl_per_connected_sta_vdev(mac->psoc);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_start(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			csr_roam_state_change(mac, eCSR_ROAMING_STATE_IDLE, i);

		mac->roam.sPendingCommands = 0;

		if (mac->mlme_cfg->reg.enable_pending_chan_list_req) {
			status = ucfg_scan_register_event_handler(mac->pdev,
					csr_scan_event_handler, mac);

			if (QDF_IS_STATUS_ERROR(status))
				sme_err("scan event registration failed ");
		}
	} while (0);
	return status;
}

QDF_STATUS csr_stop(struct mac_context *mac)
{
	uint32_t sessionId;

	if (mac->mlme_cfg->reg.enable_pending_chan_list_req)
		ucfg_scan_unregister_event_handler(mac->pdev,
						   csr_scan_event_handler,
						   mac);
	wlan_scan_psoc_set_disable(mac->psoc, REASON_SYSTEM_DOWN);

	/*
	 * purge all serialization command if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(mac);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		csr_prepare_vdev_delete(mac, sessionId, true);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			ucfg_scan_flush_results(mac->pdev, NULL);

	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++) {
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_STOP, sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_ready(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	/* If the gScanAgingTime is set to '0' then scan results aging timeout
	 * based  on timer feature is not enabled
	 */
	status = csr_apply_channel_and_power_list(mac);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("csr_apply_channel_and_power_list failed status: %d",
			status);

	return status;
}

void csr_set_default_dot11_mode(struct mac_context *mac)
{
	mac->mlme_cfg->dot11_mode.dot11_mode =
			csr_translate_to_wni_cfg_dot11_mode(mac,
					  mac->roam.configParam.uCfgDot11Mode);
}

void csr_set_global_cfgs(struct mac_context *mac)
{
	wlan_mlme_set_frag_threshold(mac->psoc, csr_get_frag_thresh(mac));
	wlan_mlme_set_rts_threshold(mac->psoc, csr_get_rts_thresh(mac));
	/* For now we will just use the 5GHz CB mode ini parameter to decide
	 * whether CB supported or not in Probes when there is no session
	 * Once session is established we will use the session related params
	 * stored in PE session for CB mode
	 */
	if (cfg_in_range(CFG_CHANNEL_BONDING_MODE_5GHZ,
			 mac->roam.configParam.channelBondingMode5GHz))
		ucfg_mlme_set_channel_bonding_5ghz(mac->psoc,
						   mac->roam.configParam.
						   channelBondingMode5GHz);
	if (cfg_in_range(CFG_CHANNEL_BONDING_MODE_24GHZ,
			 mac->roam.configParam.channelBondingMode24GHz))
		ucfg_mlme_set_channel_bonding_24ghz(mac->psoc,
						    mac->roam.configParam.
						    channelBondingMode24GHz);

	mac->mlme_cfg->timeouts.heart_beat_threshold =
			cfg_default(CFG_HEART_BEAT_THRESHOLD);

	/* Update the operating mode to configured value during
	 *  initialization, So that client can advertise full
	 *  capabilities in Probe request frame.
	 */
	csr_set_default_dot11_mode(mac);
}

#if defined(WLAN_LOGGING_SOCK_SVC_ENABLE) && \
	defined(CONNECTIVITY_PKTLOG)
/**
 * csr_packetdump_timer_handler() - packet dump timer
 * handler
 * @pv: user data
 *
 * This function is used to handle packet dump timer
 *
 * Return: None
 *
 */
static void csr_packetdump_timer_handler(void *pv)
{
	sme_debug("Invoking packetdump deregistration API");
	wlan_deregister_txrx_packetdump(OL_TXRX_PDEV_ID);
}

void csr_packetdump_timer_start(void)
{
	QDF_STATUS status;
	mac_handle_t mac_handle;
	struct mac_context *mac;
	QDF_TIMER_STATE cur_state;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac = MAC_CONTEXT(mac_handle);
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}
	cur_state = qdf_mc_timer_get_current_state(&mac->roam.packetdump_timer);
	if (cur_state == QDF_TIMER_STATE_STARTING ||
	    cur_state == QDF_TIMER_STATE_STARTING) {
		sme_debug("packetdump_timer is already started: %d", cur_state);
		return;
	}

	status = qdf_mc_timer_start(&mac->roam.packetdump_timer,
				    (PKT_DUMP_TIMER_DURATION *
				     QDF_MC_TIMER_TO_SEC_UNIT) /
				    QDF_MC_TIMER_TO_MS_UNIT);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_debug("cannot start packetdump timer status: %d", status);
}

void csr_packetdump_timer_stop(void)
{
	QDF_STATUS status;
	mac_handle_t mac_handle;
	struct mac_context *mac;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac = MAC_CONTEXT(mac_handle);
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}

	status = qdf_mc_timer_stop(&mac->roam.packetdump_timer);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("cannot stop packetdump timer");
}

static QDF_STATUS csr_packetdump_timer_init(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!mac) {
		QDF_ASSERT(0);
		return -EINVAL;
	}

	status = qdf_mc_timer_init(&mac->roam.packetdump_timer,
				   QDF_TIMER_TYPE_SW,
				   csr_packetdump_timer_handler,
				   mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("cannot allocate memory for packetdump timer");
		return status;
	}

	return status;
}

static void csr_packetdump_timer_deinit(struct mac_context *mac)
{
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}

	qdf_mc_timer_stop(&mac->roam.packetdump_timer);
	qdf_mc_timer_destroy(&mac->roam.packetdump_timer);
}
#else
static inline QDF_STATUS csr_packetdump_timer_init(struct mac_context *mac)
{
	return QDF_STATUS_SUCCESS;
}

static inline void csr_packetdump_timer_deinit(struct mac_context *mac) {}
#endif

static QDF_STATUS csr_roam_open(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = csr_packetdump_timer_init(mac);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	spin_lock_init(&mac->roam.roam_state_lock);

	return status;
}

static QDF_STATUS csr_roam_close(struct mac_context *mac)
{
	uint32_t sessionId;
	struct csr_roam_session *session;

	/*
	 * purge all serialization command if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(mac);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++) {
		session = CSR_GET_SESSION(mac, sessionId);
		if (!session)
			continue;

		csr_prepare_vdev_delete(mac, sessionId, true);
	}

	csr_packetdump_timer_deinit(mac);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS csr_roam_free_connected_info(struct mac_context *mac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (pConnectedInfo->pbFrames) {
		qdf_mem_free(pConnectedInfo->pbFrames);
		pConnectedInfo->pbFrames = NULL;
	}
	pConnectedInfo->nBeaconLength = 0;
	pConnectedInfo->nAssocReqLength = 0;
	pConnectedInfo->nAssocRspLength = 0;
	pConnectedInfo->nRICRspLength = 0;
#ifdef FEATURE_WLAN_ESE
	pConnectedInfo->nTspecIeLength = 0;
#endif
	return status;
}

void csr_release_command_roam(struct mac_context *mac, tSmeCmd *pCommand)
{
	csr_reinit_roam_cmd(mac, pCommand);
}

void csr_release_command_wm_status_change(struct mac_context *mac,
					  tSmeCmd *pCommand)
{
	csr_reinit_wm_status_change_cmd(mac, pCommand);
}

void csr_roam_substate_change(struct mac_context *mac,
		enum csr_roam_substate NewSubstate, uint32_t sessionId)
{
	if (sessionId >= WLAN_MAX_VDEVS) {
		sme_err("Invalid no of concurrent sessions %d",
			  sessionId);
		return;
	}
	if (mac->roam.curSubState[sessionId] == NewSubstate)
		return;
	sme_nofl_debug("CSR RoamSubstate: [ %s <== %s ]",
		       mac_trace_getcsr_roam_sub_state(NewSubstate),
		       mac_trace_getcsr_roam_sub_state(mac->roam.
		       curSubState[sessionId]));
	spin_lock(&mac->roam.roam_state_lock);
	mac->roam.curSubState[sessionId] = NewSubstate;
	spin_unlock(&mac->roam.roam_state_lock);
}

enum csr_roam_state csr_roam_state_change(struct mac_context *mac,
				    enum csr_roam_state NewRoamState,
				uint8_t sessionId)
{
	enum csr_roam_state PreviousState;

	PreviousState = mac->roam.curState[sessionId];

	if (NewRoamState == mac->roam.curState[sessionId])
		return PreviousState;

	sme_nofl_debug("CSR RoamState[%d]: [ %s <== %s ]", sessionId,
		       mac_trace_getcsr_roam_state(NewRoamState),
		       mac_trace_getcsr_roam_state(
		       mac->roam.curState[sessionId]));
	/*
	 * Whenever we transition OUT of the Roaming state,
	 * clear the Roaming substate.
	 */
	if (CSR_IS_ROAM_JOINING(mac, sessionId)) {
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	mac->roam.curState[sessionId] = NewRoamState;

	return PreviousState;
}

static void init_config_param(struct mac_context *mac)
{
	mac->roam.configParam.channelBondingMode24GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	mac->roam.configParam.channelBondingMode5GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;

	mac->roam.configParam.phyMode = eCSR_DOT11_MODE_AUTO;
	mac->roam.configParam.uCfgDot11Mode = eCSR_CFG_DOT11_MODE_AUTO;
	mac->roam.configParam.HeartbeatThresh50 = 40;
	mac->roam.configParam.Is11eSupportEnabled = true;
	mac->roam.configParam.WMMSupportMode = WMM_USER_MODE_AUTO;
	mac->roam.configParam.ProprietaryRatesEnabled = true;

	mac->roam.configParam.nVhtChannelWidth =
		WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ + 1;
}

/**
 * csr_flush_roam_scan_chan_lists() - Flush the roam channel lists
 * @mac: Global MAC context
 * @vdev_id: vdev id
 *
 * Flush the roam channel lists pref_chan_info and specific_chan_info.
 *
 * Return: None
 */
static void
csr_flush_roam_scan_chan_lists(struct mac_context *mac, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *cfg_params;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac->pdev, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	cfg_params = &rso_cfg->cfg_param;
	wlan_cm_flush_roam_channel_list(&cfg_params->pref_chan_info);
	wlan_cm_flush_roam_channel_list(&cfg_params->specific_chan_info);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

#ifdef FEATURE_WLAN_ESE
/**
 * csr_roam_is_ese_assoc() - is this ese association
 * @mac_ctx: Global MAC context
 * @session_id: session identifier
 *
 * Returns whether the current association is a ESE assoc or not.
 *
 * Return: true if ese association; false otherwise
 */
bool csr_roam_is_ese_assoc(struct mac_context *mac_ctx, uint32_t session_id)
{
	return wlan_cm_get_ese_assoc(mac_ctx->pdev, session_id);
}


/**
 * csr_tsm_stats_rsp_processor() - tsm stats response processor
 * @mac: Global MAC context
 * @pMsg: Message pointer
 *
 * Return: None
 */
static void csr_tsm_stats_rsp_processor(struct mac_context *mac, void *pMsg)
{
	tAniGetTsmStatsRsp *pTsmStatsRsp = (tAniGetTsmStatsRsp *) pMsg;

	if (pTsmStatsRsp) {
		/*
		 * Get roam Rssi request is backed up and passed back
		 * to the response, Extract the request message
		 * to fetch callback.
		 */
		tpAniGetTsmStatsReq reqBkp
			= (tAniGetTsmStatsReq *) pTsmStatsRsp->tsmStatsReq;

		if (reqBkp) {
			if (reqBkp->tsmStatsCallback) {
				((tCsrTsmStatsCallback)
				 (reqBkp->tsmStatsCallback))(pTsmStatsRsp->
							     tsmMetrics,
							     reqBkp->
							     pDevContext);
				reqBkp->tsmStatsCallback = NULL;
			}
			qdf_mem_free(reqBkp);
			pTsmStatsRsp->tsmStatsReq = NULL;
		} else {
			if (reqBkp) {
				qdf_mem_free(reqBkp);
				pTsmStatsRsp->tsmStatsReq = NULL;
			}
		}
	} else {
		sme_err("pTsmStatsRsp is NULL");
	}
}

/**
 * csr_send_ese_adjacent_ap_rep_ind() - ese send adjacent ap report
 * @mac: Global MAC context
 * @pSession: Session pointer
 *
 * Return: None
 */
static void csr_send_ese_adjacent_ap_rep_ind(struct mac_context *mac,
					struct csr_roam_session *pSession)
{
	uint32_t roamTS2 = 0;
	struct csr_roam_info *roam_info;
	struct pe_session *pe_session = NULL;
	uint8_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	struct qdf_mac_addr connected_bssid;

	if (!pSession) {
		sme_err("pSession is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roamTS2 = qdf_mc_timer_get_system_time();
	roam_info->tsmRoamDelay = roamTS2 - pSession->roamTS1;
	wlan_mlme_get_bssid_vdev_id(mac->pdev, pSession->vdev_id,
				    &connected_bssid);
	sme_debug("Bssid(" QDF_MAC_ADDR_FMT ") Roaming Delay(%u ms)",
		QDF_MAC_ADDR_REF(connected_bssid.bytes),
		roam_info->tsmRoamDelay);

	pe_session = pe_find_session_by_bssid(mac, connected_bssid.bytes,
					      &sessionId);
	if (!pe_session) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}

	pe_session->eseContext.tsm.tsmMetrics.RoamingDly
		= roam_info->tsmRoamDelay;

	csr_roam_call_callback(mac, pSession->vdev_id, roam_info,
			       eCSR_ROAM_ESE_ADJ_AP_REPORT_IND, 0);
	qdf_mem_free(roam_info);
}

/**
 * csr_get_tsm_stats() - get tsm stats
 * @mac: Global MAC context
 * @callback: TSM stats callback
 * @staId: Station id
 * @bssId: bssid
 * @pContext: pointer to context
 * @tid: traffic id
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_get_tsm_stats(struct mac_context *mac,
			     tCsrTsmStatsCallback callback,
			     struct qdf_mac_addr bssId,
			     void *pContext, uint8_t tid)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tAniGetTsmStatsReq *pMsg = NULL;

	pMsg = qdf_mem_malloc(sizeof(tAniGetTsmStatsReq));
	if (!pMsg) {
		return QDF_STATUS_E_NOMEM;
	}
	/* need to initiate a stats request to PE */
	pMsg->msgType = eWNI_SME_GET_TSM_STATS_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetTsmStatsReq);
	pMsg->tid = tid;
	qdf_copy_macaddr(&pMsg->bssId, &bssId);
	pMsg->tsmStatsCallback = callback;
	pMsg->pDevContext = pContext;
	status = umac_send_mb_message_to_mac(pMsg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Failed to send down the TSM req (status=%d)", status);
		/* pMsg is freed by cds_send_mb_message_to_mac */
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}
#endif /* FEATURE_WLAN_ESE */

/* The funcns csr_convert_cb_ini_value_to_phy_cb_state and
 * csr_convert_phy_cb_state_to_ini_value have been introduced
 * to convert the ini value to the ENUM used in csr and MAC for CB state
 * Ideally we should have kept the ini value and enum value same and
 * representing the same cb values as in 11n standard i.e.
 * Set to 1 (SCA) if the secondary channel is above the primary channel
 * Set to 3 (SCB) if the secondary channel is below the primary channel
 * Set to 0 (SCN) if no secondary channel is present
 * However, since our driver is already distributed we will keep the ini
 * definition as it is which is:
 * 0 - secondary none
 * 1 - secondary LOW
 * 2 - secondary HIGH
 * and convert to enum value used within the driver in
 * csr_change_default_config_param using this funcn
 * The enum values are as follows:
 * PHY_SINGLE_CHANNEL_CENTERED          = 0
 * PHY_DOUBLE_CHANNEL_LOW_PRIMARY   = 1
 * PHY_DOUBLE_CHANNEL_HIGH_PRIMARY  = 3
 */
ePhyChanBondState csr_convert_cb_ini_value_to_phy_cb_state(uint32_t cbIniValue)
{

	ePhyChanBondState phyCbState;

	switch (cbIniValue) {
	/* secondary none */
	case eCSR_INI_SINGLE_CHANNEL_CENTERED:
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		phyCbState =
			PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* If an invalid value is passed, disable CHANNEL BONDING */
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	}
	return phyCbState;
}

static
uint32_t csr_convert_phy_cb_state_to_ini_value(ePhyChanBondState phyCbState)
{
	uint32_t cbIniValue;

	switch (phyCbState) {
	/* secondary none */
	case PHY_SINGLE_CHANNEL_CENTERED:
		cbIniValue = eCSR_INI_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		cbIniValue =
		eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* return some invalid value */
		cbIniValue = eCSR_INI_CHANNEL_BONDING_STATE_MAX;
		break;
	}
	return cbIniValue;
}

#ifdef WLAN_FEATURE_11BE
void csr_update_session_eht_cap(struct mac_context *mac_ctx,
				struct csr_roam_session *session)
{
	tDot11fIEeht_cap *eht_cap;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	qdf_mem_copy(&mlme_priv->eht_config,
		     &mac_ctx->mlme_cfg->eht_caps.dot11_eht_cap,
		     sizeof(mlme_priv->eht_config));
	eht_cap = &mlme_priv->eht_config;
	eht_cap->present = true;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#endif

#ifdef WLAN_FEATURE_11AX
void csr_update_session_he_cap(struct mac_context *mac_ctx,
			       struct csr_roam_session *session)
{
	enum QDF_OPMODE persona;
	tDot11fIEhe_cap *he_cap;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	qdf_mem_copy(&mlme_priv->he_config,
		     &mac_ctx->mlme_cfg->he_caps.dot11_he_cap,
		     sizeof(mlme_priv->he_config));
	he_cap = &mlme_priv->he_config;
	he_cap->present = true;
	/*
	 * Do not advertise requester role for SAP & responder role
	 * for STA
	 */
	persona = wlan_vdev_mlme_get_opmode(vdev);
	if (persona == QDF_SAP_MODE || persona == QDF_P2P_GO_MODE) {
		he_cap->twt_request = 0;
	} else if (persona == QDF_STA_MODE || persona == QDF_P2P_CLIENT_MODE) {
		he_cap->twt_responder = 0;
	}

	if (he_cap->ppet_present) {
		/* till now operating channel is not decided yet, use 5g cap */
		qdf_mem_copy(he_cap->ppet.ppe_threshold.ppe_th,
			     mac_ctx->mlme_cfg->he_caps.he_ppet_5g,
			     WNI_CFG_HE_PPET_LEN);
		he_cap->ppet.ppe_threshold.num_ppe_th =
			lim_truncate_ppet(he_cap->ppet.ppe_threshold.ppe_th,
					  WNI_CFG_HE_PPET_LEN);
	} else {
		he_cap->ppet.ppe_threshold.num_ppe_th = 0;
	}
	mlme_priv->he_sta_obsspd = mac_ctx->mlme_cfg->he_caps.he_sta_obsspd;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#endif

QDF_STATUS csr_change_default_config_param(struct mac_context *mac,
					   struct csr_config_params *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (pParam) {
		mac->roam.configParam.is_force_1x1 =
			pParam->is_force_1x1;
		mac->roam.configParam.WMMSupportMode = pParam->WMMSupportMode;
		mac->mlme_cfg->wmm_params.wme_enabled =
			(pParam->WMMSupportMode == WMM_USER_MODE_NO_QOS) ? 0 : 1;
		mac->roam.configParam.Is11eSupportEnabled =
			pParam->Is11eSupportEnabled;

		mac->roam.configParam.fenableMCCMode = pParam->fEnableMCCMode;
		mac->roam.configParam.mcc_rts_cts_prot_enable =
						pParam->mcc_rts_cts_prot_enable;
		mac->roam.configParam.mcc_bcast_prob_resp_enable =
					pParam->mcc_bcast_prob_resp_enable;
		mac->roam.configParam.fAllowMCCGODiffBI =
			pParam->fAllowMCCGODiffBI;

		/* channelBondingMode5GHz plays a dual role right now
		 * INFRA STA will use this non zero value as CB enabled
		 * and SOFTAP will use this non-zero value to determine
		 * the secondary channel offset. This is how
		 * channelBondingMode5GHz works now and this is kept intact
		 * to avoid any cfg.ini change.
		 */
		if (pParam->channelBondingMode24GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 2.4GHz band %d, CB DISABLED",
				pParam->channelBondingMode24GHz);
		mac->roam.configParam.channelBondingMode24GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
						channelBondingMode24GHz);
		if (pParam->channelBondingMode5GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 5GHz band %d, CB DISABLED",
				pParam->channelBondingMode5GHz);
		mac->roam.configParam.channelBondingMode5GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
							channelBondingMode5GHz);
		mac->roam.configParam.phyMode = pParam->phyMode;
		mac->roam.configParam.HeartbeatThresh50 =
			pParam->HeartbeatThresh50;
		mac->roam.configParam.ProprietaryRatesEnabled =
			pParam->ProprietaryRatesEnabled;

		mac->roam.configParam.wep_tkip_in_he = pParam->wep_tkip_in_he;

		mac->roam.configParam.uCfgDot11Mode =
			csr_get_cfg_dot11_mode_from_csr_phy_mode(false,
							mac->roam.configParam.
							phyMode);

		/* Assign this before calling csr_init11d_info */
		if (wlan_reg_11d_enabled_on_host(mac->psoc))
			status = csr_init11d_info(mac, &pParam->Csr11dinfo);

		/* Initialize the power + channel information if 11h is
		 * enabled. If 11d is enabled this information has already
		 * been initialized
		 */
		if (csr_is11h_supported(mac) &&
				!wlan_reg_11d_enabled_on_host(mac->psoc))
			csr_init_channel_power_list(mac, &pParam->Csr11dinfo);

		mac->scan.fEnableDFSChnlScan = pParam->fEnableDFSChnlScan;
		mac->roam.configParam.send_smps_action =
			pParam->send_smps_action;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		mac->roam.configParam.cc_switch_mode = pParam->cc_switch_mode;
#endif
		mac->roam.configParam.obssEnabled = pParam->obssEnabled;
		mac->roam.configParam.conc_custom_rule1 =
			pParam->conc_custom_rule1;
		mac->roam.configParam.conc_custom_rule2 =
			pParam->conc_custom_rule2;
		mac->roam.configParam.is_sta_connection_in_5gz_enabled =
			pParam->is_sta_connection_in_5gz_enabled;

		/* update interface configuration */
		mac->sme.max_intf_count = pParam->max_intf_count;

		mac->f_sta_miracast_mcc_rest_time_val =
			pParam->f_sta_miracast_mcc_rest_time_val;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		mac->sap.sap_channel_avoidance =
			pParam->sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	}
	return status;
}

QDF_STATUS csr_get_config_param(struct mac_context *mac,
				struct csr_config_params *pParam)
{
	struct csr_config *cfg_params = &mac->roam.configParam;

	if (!pParam)
		return QDF_STATUS_E_INVAL;

	pParam->is_force_1x1 = cfg_params->is_force_1x1;
	pParam->WMMSupportMode = cfg_params->WMMSupportMode;
	pParam->Is11eSupportEnabled = cfg_params->Is11eSupportEnabled;
	pParam->channelBondingMode24GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode24GHz);
	pParam->channelBondingMode5GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode5GHz);
	pParam->phyMode = cfg_params->phyMode;
	pParam->HeartbeatThresh50 = cfg_params->HeartbeatThresh50;
	pParam->ProprietaryRatesEnabled = cfg_params->ProprietaryRatesEnabled;
	pParam->fEnableDFSChnlScan = mac->scan.fEnableDFSChnlScan;
	pParam->fEnableMCCMode = cfg_params->fenableMCCMode;
	pParam->fAllowMCCGODiffBI = cfg_params->fAllowMCCGODiffBI;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	pParam->cc_switch_mode = cfg_params->cc_switch_mode;
#endif
	pParam->wep_tkip_in_he = cfg_params->wep_tkip_in_he;
	csr_set_channels(mac, pParam);
	pParam->obssEnabled = cfg_params->obssEnabled;
	pParam->conc_custom_rule1 = cfg_params->conc_custom_rule1;
	pParam->conc_custom_rule2 = cfg_params->conc_custom_rule2;
	pParam->is_sta_connection_in_5gz_enabled =
		cfg_params->is_sta_connection_in_5gz_enabled;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	pParam->sap_channel_avoidance = mac->sap.sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	pParam->max_intf_count = mac->sme.max_intf_count;
	pParam->f_sta_miracast_mcc_rest_time_val =
		mac->f_sta_miracast_mcc_rest_time_val;
	pParam->send_smps_action = mac->roam.configParam.send_smps_action;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_prune_ch_list() - prunes the channel list to keep only a type of channels
 * @ch_lst:        existing channel list
 * @is_24_GHz:     indicates if 2.5 GHz or 5 GHz channels are required
 *
 * Return: void
 */
static void csr_prune_ch_list(struct csr_channel *ch_lst, bool is_24_GHz)
{
	uint8_t idx = 0, num_channels = 0;

	for ( ; idx < ch_lst->numChannels; idx++) {
		if (is_24_GHz) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_lst->channel_freq_list[idx])) {
				ch_lst->channel_freq_list[num_channels] =
					ch_lst->channel_freq_list[idx];
				num_channels++;
			}
		} else {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(ch_lst->channel_freq_list[idx])) {
				ch_lst->channel_freq_list[num_channels] =
					ch_lst->channel_freq_list[idx];
				num_channels++;
			}
		}
	}
	/*
	 * Cleanup the rest of channels. Note we only need to clean up the
	 * channels if we had to trim the list. Calling qdf_mem_set() with a 0
	 * size is going to throw asserts on the debug builds so let's be a bit
	 * smarter about that. Zero out the reset of the channels only if we
	 * need to. The amount of memory to clear is the number of channesl that
	 * we trimmed (ch_lst->numChannels - num_channels) times the size of a
	 * channel in the structure.
	 */
	if (ch_lst->numChannels > num_channels) {
		qdf_mem_zero(&ch_lst->channel_freq_list[num_channels],
			     sizeof(ch_lst->channel_freq_list[0]) *
			     (ch_lst->numChannels - num_channels));
	}
	ch_lst->numChannels = num_channels;
}

/**
 * csr_prune_channel_list_for_mode() - prunes the channel list
 * @mac_ctx:       global mac context
 * @ch_lst:        existing channel list
 *
 * Prunes the channel list according to band stored in mac_ctx
 *
 * Return: void
 */
void csr_prune_channel_list_for_mode(struct mac_context *mac_ctx,
				     struct csr_channel *ch_lst)
{
	/* for dual band NICs, don't need to trim the channel list.... */
	if (CSR_IS_OPEARTING_DUAL_BAND(mac_ctx))
		return;
	/*
	 * 2.4 GHz band operation requires the channel list to be trimmed to
	 * the 2.4 GHz channels only
	 */
	if (CSR_IS_24_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, true);
	else if (CSR_IS_5G_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, false);
}

#define INFRA_AP_DEFAULT_CHAN_FREQ 2437

QDF_STATUS csr_get_channel_and_power_list(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t num20MHzChannelsFound = 0;
	QDF_STATUS qdf_status;
	uint8_t Index = 0;

	qdf_status = wlan_reg_get_channel_list_with_power_for_freq(
				mac->pdev,
				mac->scan.defaultPowerTable,
				&num20MHzChannelsFound);

	if ((QDF_STATUS_SUCCESS != qdf_status) ||
	    (num20MHzChannelsFound == 0)) {
		sme_err("failed to get channels");
		status = QDF_STATUS_E_FAILURE;
	} else {
		if (num20MHzChannelsFound > CFG_VALID_CHANNEL_LIST_LEN)
			num20MHzChannelsFound = CFG_VALID_CHANNEL_LIST_LEN;
		mac->scan.numChannelsDefault = num20MHzChannelsFound;
		/* Move the channel list to the global data */
		/* structure -- this will be used as the scan list */
		for (Index = 0; Index < num20MHzChannelsFound; Index++)
			mac->scan.base_channels.channel_freq_list[Index] =
				mac->scan.defaultPowerTable[Index].center_freq;
		mac->scan.base_channels.numChannels =
			num20MHzChannelsFound;
	}
	return status;
}

QDF_STATUS csr_apply_channel_and_power_list(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_prune_channel_list_for_mode(mac, &mac->scan.base_channels);
	csr_save_channel_power_for_band(mac, false);
	csr_save_channel_power_for_band(mac, true);
	csr_apply_channel_power_info_to_fw(mac,
					   &mac->scan.base_channels);

	csr_init_operating_classes(mac);
	return status;
}

static QDF_STATUS csr_init11d_info(struct mac_context *mac, tCsr11dinfo *ps11dinfo)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t index;
	uint32_t count = 0;
	struct pwr_channel_info *pChanInfo;
	struct pwr_channel_info *pChanInfoStart;
	bool applyConfig = true;

	if (!ps11dinfo)
		return status;

	if (ps11dinfo->Channels.numChannels
	    && (CFG_VALID_CHANNEL_LIST_LEN >=
		ps11dinfo->Channels.numChannels)) {
		mac->scan.base_channels.numChannels =
			ps11dinfo->Channels.numChannels;
		qdf_mem_copy(mac->scan.base_channels.channel_freq_list,
			     ps11dinfo->Channels.channel_freq_list,
			     ps11dinfo->Channels.numChannels);
	} else {
		/* No change */
		return QDF_STATUS_SUCCESS;
	}
	/* legacy maintenance */

	/* need to add the max power channel list */
	pChanInfo =
		qdf_mem_malloc(sizeof(struct pwr_channel_info) *
			       CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo) {
		pChanInfoStart = pChanInfo;
		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->first_freq = ps11dinfo->ChnPower[index].first_chan_freq;
			pChanInfo->num_chan =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->max_tx_pwr =
				ps11dinfo->ChnPower[index].maxtxPower;
			pChanInfo++;
			count++;
		}
		if (count) {
			status = csr_save_to_channel_power2_g_5_g(mac,
						count *
						sizeof(struct pwr_channel_info),
						pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}
	/* Only apply them to CFG when not in STOP state.
	 * Otherwise they will be applied later
	 */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		for (index = 0; index < WLAN_MAX_VDEVS; index++) {
			if ((CSR_IS_SESSION_VALID(mac, index))
			    && CSR_IS_ROAM_STOP(mac, index)) {
				applyConfig = false;
			}
		}

		if (true == applyConfig) {
			/* Apply the base channel list, power info,
			 * and set the Country code.
			 */
			csr_apply_channel_power_info_to_fw(mac,
							   &mac->scan.
							   base_channels);
		}
	}
	return status;
}

/* Initialize the Channel + Power List in the local cache and in the CFG */
QDF_STATUS csr_init_channel_power_list(struct mac_context *mac,
					tCsr11dinfo *ps11dinfo)
{
	uint8_t index;
	uint32_t count = 0;
	struct pwr_channel_info *pChanInfo;
	struct pwr_channel_info *pChanInfoStart;

	if (!ps11dinfo || !mac)
		return QDF_STATUS_E_FAILURE;

	pChanInfo =
		qdf_mem_malloc(sizeof(struct pwr_channel_info) *
			       CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo) {
		pChanInfoStart = pChanInfo;

		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->first_freq = ps11dinfo->ChnPower[index].first_chan_freq;
			pChanInfo->num_chan =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->max_tx_pwr =
				ps11dinfo->ChnPower[index].maxtxPower;
			pChanInfo++;
			count++;
		}
		if (count) {
			csr_save_to_channel_power2_g_5_g(mac,
						count *
						sizeof(struct pwr_channel_info),
						pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
#ifdef WLAN_UNIT_TEST
void csr_cm_get_sta_cxn_info(struct mac_context *mac_ctx, uint8_t vdev_id,
			     char *buf, uint32_t buf_sz)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("vdev object is NULL for vdev %d", vdev_id);
		return;
	}

	cm_get_sta_cxn_info(vdev, buf, buf_sz);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

}
#endif
#endif

QDF_STATUS csr_roam_call_callback(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_info *roam_info,
				  eRoamCmdStatus u1, eCsrRoamResult u2)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession;
	qdf_freq_t chan_freq;

	if (!CSR_IS_SESSION_VALID(mac, sessionId)) {
		sme_err("Session ID: %d is not valid", sessionId);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
	pSession = CSR_GET_SESSION(mac, sessionId);

	if (false == pSession->sessionActive) {
		sme_debug("Session is not Active");
		return QDF_STATUS_E_FAILURE;
	}
	chan_freq = wlan_get_operation_chan_freq_vdev_id(mac->pdev, sessionId);

	if (mac->session_roam_complete_cb)
		status = mac->session_roam_complete_cb(mac->psoc, sessionId,
						       roam_info, u1, u2);

	return status;
}

static bool csr_peer_mac_match_cmd(tSmeCmd *sme_cmd,
				   struct qdf_mac_addr peer_macaddr,
				   uint8_t vdev_id)
{
	if (sme_cmd->command == eSmeCommandRoam &&
	    (sme_cmd->u.roamCmd.roamReason == eCsrForcedDisassocSta ||
	     sme_cmd->u.roamCmd.roamReason == eCsrForcedDeauthSta) &&
	    !qdf_mem_cmp(peer_macaddr.bytes, sme_cmd->u.roamCmd.peerMac,
			 QDF_MAC_ADDR_SIZE))
		return true;

	if (sme_cmd->command == eSmeCommandWmStatusChange) {
		struct wmstatus_changecmd *wms_cmd;

		wms_cmd = &sme_cmd->u.wmStatusChangeCmd;
		if (wms_cmd->Type == eCsrDisassociated &&
		    !qdf_mem_cmp(peer_macaddr.bytes,
				 wms_cmd->u.DisassocIndMsg.peer_macaddr.bytes,
				 QDF_MAC_ADDR_SIZE))
			return true;

		if (wms_cmd->Type == eCsrDeauthenticated &&
		    !qdf_mem_cmp(peer_macaddr.bytes,
				 wms_cmd->u.DeauthIndMsg.peer_macaddr.bytes,
				 QDF_MAC_ADDR_SIZE))
			return true;
	}

	return false;
}

static bool
csr_is_deauth_disassoc_in_pending_q(struct mac_context *mac_ctx,
				    uint8_t vdev_id,
				    struct qdf_mac_addr peer_macaddr)
{
	tListElem *entry = NULL;
	tSmeCmd *sme_cmd;

	entry = csr_nonscan_pending_ll_peek_head(mac_ctx, LL_ACCESS_NOLOCK);
	while (entry) {
		sme_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if ((sme_cmd->vdev_id == vdev_id) &&
		    csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
			return true;
		entry = csr_nonscan_pending_ll_next(mac_ctx, entry,
						    LL_ACCESS_NOLOCK);
	}

	return false;
}

static bool
csr_is_deauth_disassoc_in_active_q(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr peer_macaddr)
{
	tSmeCmd *sme_cmd;

	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
						WLAN_SER_CMD_FORCE_DEAUTH_STA);

	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
					WLAN_SER_CMD_FORCE_DISASSOC_STA);

	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	/*
	 * WLAN_SER_CMD_WM_STATUS_CHANGE is of two type, the handling
	 * should take care of both the types.
	 */
	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
						WLAN_SER_CMD_WM_STATUS_CHANGE);
	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	return false;
}

/*
 * csr_is_deauth_disassoc_already_active() - Function to check if deauth or
 *  disassoc is already in progress.
 * @mac_ctx: Global MAC context
 * @vdev_id: vdev id
 * @peer_macaddr: Peer MAC address
 *
 * Return: True if deauth/disassoc indication can be dropped
 *  else false
 */
static bool
csr_is_deauth_disassoc_already_active(struct mac_context *mac_ctx,
				      uint8_t vdev_id,
				      struct qdf_mac_addr peer_macaddr)
{
	bool ret = csr_is_deauth_disassoc_in_pending_q(mac_ctx,
						      vdev_id,
						      peer_macaddr);
	if (!ret)
		/**
		 * commands are not found in pending queue, check in active
		 * queue as well
		 */
		ret = csr_is_deauth_disassoc_in_active_q(mac_ctx,
							  vdev_id,
							  peer_macaddr);

	if (ret)
		sme_debug("Deauth/Disassoc already in progress for "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(peer_macaddr.bytes));

	return ret;
}

/**
 * csr_roam_issue_disassociate_sta_cmd() - disassociate a associated station
 * @sessionId:     Session Id for Soft AP
 * @p_del_sta_params: Pointer to parameters of the station to disassoc
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_* on error
 */
QDF_STATUS csr_roam_issue_disassociate_sta_cmd(struct mac_context *mac,
					       uint32_t sessionId,
					       struct csr_del_sta_params
					       *p_del_sta_params)

{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		if (csr_is_deauth_disassoc_already_active(mac, sessionId,
					      p_del_sta_params->peerMacAddr))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDisassocSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
				p_del_sta_params->peerMacAddr.bytes,
				sizeof(pCommand->u.roamCmd.peerMac));
		pCommand->u.roamCmd.reason = p_del_sta_params->reason_code;

		status = csr_queue_sme_command(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

/**
 * csr_roam_issue_deauthSta() - delete a associated station
 * @sessionId:     Session Id for Soft AP
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_** on error
 */
QDF_STATUS csr_roam_issue_deauth_sta_cmd(struct mac_context *mac,
		uint32_t sessionId,
		struct csr_del_sta_params *pDelStaParams)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		if (csr_is_deauth_disassoc_already_active(mac, sessionId,
					      pDelStaParams->peerMacAddr))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDeauthSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
			     pDelStaParams->peerMacAddr.bytes,
			     sizeof(tSirMacAddr));
		pCommand->u.roamCmd.reason = pDelStaParams->reason_code;

		status = csr_queue_sme_command(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

/**
 * csr_get_peer_stats_cb - Peer stats callback
 * @ev: stats event
 * @cookie: Void pointer to mac contaxt
 *
 * Return: None
 */
static void csr_get_peer_stats_cb(struct stats_event *ev, void *cookie)
{
	struct mac_context *mac = (struct mac_context *)cookie;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	if (!mac) {
		sme_err("Invalid mac ctx");
		return;
	}

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj) {
		sme_err("NULL mlme psoc object");
		return;
	}

	qdf_mc_timer_stop(
		&mlme_obj->disconnect_stats_param.disconn_stats_timer);

	if (!ev->peer_stats) {
		sme_debug("no peer stats");
		goto disconnect_stats_complete;
	}

	mac->peer_rssi = ev->peer_stats->peer_rssi;
	mac->peer_txrate = ev->peer_stats->tx_rate;
	mac->peer_rxrate = ev->peer_stats->rx_rate;
	if (!ev->peer_extended_stats) {
		sme_debug("no peer extended stats");
		goto disconnect_stats_complete;
	}
	mac->rx_mc_bc_cnt = ev->peer_extended_stats->rx_mc_bc_cnt;

disconnect_stats_complete:
	sme_debug("peer rssi %d tx_rate %d rx_rate %d rx_mc_bc_cnt %d",
		  mac->peer_rssi, mac->peer_txrate, mac->peer_rxrate,
		  mac->rx_mc_bc_cnt);
	csr_continue_peer_disconnect_after_get_stats(mac);
}

/**
 * csr_get_peer_stats - Get Peer stats
 * @mac: MAC contaxt
 * @session_id: Current session id
 * @peer_mac: Peer mac address
 *
 * Return: None
 */
static void csr_get_peer_stats(struct mac_context *mac, uint32_t session_id,
			       struct qdf_mac_addr peer_mac)
{
	struct wlan_objmgr_vdev *vdev;
	struct request_info info = {0};
	QDF_STATUS status;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj) {
		sme_err("NULL mlme psoc object");
		return;
	}
	/* Reset is_disconn_stats_completed before error handing. */
	qdf_atomic_set(
		&mlme_obj->disconnect_stats_param.is_disconn_stats_completed,
		0);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, session_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		csr_continue_peer_disconnect_after_get_stats(mac);
		return;
	}

	info.cookie = mac;
	info.u.get_peer_rssi_cb = csr_get_peer_stats_cb;
	info.vdev_id = wlan_vdev_get_id(vdev);
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	qdf_mem_copy(info.peer_mac_addr, &peer_mac, QDF_MAC_ADDR_SIZE);
	sme_debug("peer_mac" QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(peer_mac.bytes));
	mlme_obj->disconnect_stats_param.vdev_id = info.vdev_id;
	status = ucfg_mc_cp_stats_send_stats_request(vdev, TYPE_PEER_STATS,
						     &info);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("stats req failed: %d", status);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		csr_continue_peer_disconnect_after_get_stats(mac);
		return;
	}

	qdf_mc_timer_start(
		&mlme_obj->disconnect_stats_param.disconn_stats_timer,
		SME_CMD_GET_DISCONNECT_STATS_TIMEOUT);

	wma_get_rx_retry_cnt(mac, session_id, info.peer_mac_addr);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

QDF_STATUS csr_roam_process_command(struct mac_context *mac, tSmeCmd *pCommand)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t sessionId = pCommand->vdev_id;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	struct qdf_mac_addr peer_mac;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug("Roam Reason: %d sessionId: %d",
		pCommand->u.roamCmd.roamReason, sessionId);

	switch (pCommand->u.roamCmd.roamReason) {
	case eCsrForcedDisassocSta:
	case eCsrForcedDeauthSta:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				      sessionId);
		if (pCommand->u.roamCmd.roamReason == eCsrForcedDeauthSta)
			csr_roam_substate_change(mac,
						 eCSR_ROAM_SUBSTATE_DEAUTH_REQ,
						 sessionId);
		else
			csr_roam_substate_change(
						mac,
						eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
						sessionId);

		qdf_mem_copy(&peer_mac, &pCommand->u.roamCmd.peerMac,
			     QDF_MAC_ADDR_SIZE);
		/*
		 * Get peer stats before peer gets deleted so that these stats
		 * can be given to user space when big data stats are queried.
		 * Once peer stats are retrieved deauth sta will continue
		 */
		csr_get_peer_stats(mac, sessionId, peer_mac);
		break;
	}
	return status;
}

void csr_reinit_roam_cmd(struct mac_context *mac, tSmeCmd *pCommand)
{
	/* Because u.roamCmd is union and share with scanCmd and StatusChange */
	qdf_mem_zero(&pCommand->u.roamCmd, sizeof(struct roam_cmd));
}

void csr_reinit_wm_status_change_cmd(struct mac_context *mac,
			tSmeCmd *pCommand)
{
	qdf_mem_zero(&pCommand->u.wmStatusChangeCmd,
			sizeof(struct wmstatus_changecmd));
}

void csr_roam_complete(struct mac_context *mac_ctx, uint8_t session_id)
{
	tSmeCmd *sme_cmd;
	struct wlan_serialization_command *cmd;

	cmd = wlan_serialization_peek_head_active_cmd_using_psoc(
				mac_ctx->psoc, false);
	if (!cmd) {
		sme_err("Roam completion called but cmd is not active");
		return;
	}
	sme_cmd = cmd->umac_cmd;
	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	if (eSmeCommandRoam == sme_cmd->command) {
		csr_roam_process_results_default(mac_ctx, sme_cmd);
		csr_release_command(mac_ctx, sme_cmd);
	}
}

/* Returns whether the current association is a 11r assoc or not */
bool csr_roam_is11r_assoc(struct mac_context *mac, uint8_t sessionId)
{
	struct cm_roam_values_copy config;

	wlan_cm_roam_cfg_get_value(mac->psoc, sessionId, IS_11R_CONNECTION,
				   &config);

	return config.bool_value;
}

void csr_roam_process_results_default(struct mac_context *mac_ctx,
				      tSmeCmd *cmd)
{
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_session *session;
	struct csr_roam_info *roam_info;
	QDF_STATUS status;
	enum QDF_OPMODE opmode;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	opmode = wlan_get_opmode_vdev_id(mac_ctx->pdev, session_id);
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac_ctx, session_id)) {
		/*
		 * do not free for the other profiles as we need
		 * to send down stop BSS later
		 */
		csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
		csr_set_default_dot11_mode(mac_ctx);
	}

	switch (cmd->u.roamCmd.roamReason) {
	case eCsrForcedDisassocSta:
	case eCsrForcedDeauthSta:
		roam_info->rssi = mac_ctx->peer_rssi;
		roam_info->tx_rate = mac_ctx->peer_txrate;
		roam_info->rx_rate = mac_ctx->peer_rxrate;
		roam_info->rx_mc_bc_cnt = mac_ctx->rx_mc_bc_cnt;
		roam_info->rx_retry_cnt = mac_ctx->rx_retry_cnt;

		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
				      session_id);
		if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
			qdf_mem_copy(roam_info->peerMac.bytes,
				     cmd->u.roamCmd.peerMac,
				     sizeof(tSirMacAddr));
			roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
			/* Update the MAC reason code */
			roam_info->disassoc_reason = cmd->u.roamCmd.reason;
			roam_info->status_code = eSIR_SME_SUCCESS;
			status = csr_roam_call_callback(mac_ctx, session_id,
							roam_info,
							eCSR_ROAM_LOSTLINK,
							eCSR_ROAM_RESULT_FORCED);
		}
		break;
	default:
		csr_roam_state_change(mac_ctx,
			eCSR_ROAMING_STATE_IDLE, session_id);
		break;
	}
	qdf_mem_free(roam_info);
}

/**
 * csr_roam_process_start_bss_success() - Process the result for start bss
 * @mac_ctx:   Global MAC Context
 * @context:   Additional data in context of the cmd
 * @vdev_id:   vdev id
 *
 * Return: None
 */
static void csr_roam_process_start_bss_success(struct mac_context *mac_ctx,
					       struct csr_roam_info *roam_info,
					       void *context, uint8_t vdev_id)
{
	struct csr_roam_session *session;
	struct start_bss_rsp *start_bss_rsp = NULL;
	eRoamCmdStatus roam_status = eCSR_ROAM_INFRA_IND;
	eCsrRoamResult roam_result = eCSR_ROAM_RESULT_INFRA_STARTED;
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	QDF_STATUS status;
	enum QDF_OPMODE opmode;
	uint8_t wmm_mode = 0, value = 0;

	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("Invalid session id %d", vdev_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, vdev_id);

	sme_debug("Start BSS success");
	start_bss_rsp = (struct start_bss_rsp *)context;

	opmode = wlan_get_opmode_vdev_id(mac_ctx->pdev, vdev_id);
	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE)
		session->connectState =
				eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
	else if (opmode == QDF_NDI_MODE)
		session->connectState = eCSR_CONNECT_STATE_TYPE_NDI_STARTED;
	else
		session->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;

	if (opmode == QDF_NDI_MODE) {
		status = ucfg_mlme_get_wmm_mode(mac_ctx->psoc, &wmm_mode);
		if (!QDF_IS_STATUS_SUCCESS(status))
			return;
		if (wmm_mode == WMM_USER_MODE_NO_QOS) {
			session->modifyProfileFields.uapsd_mask = 0;
		} else {
			status =
			   ucfg_mlme_get_wmm_uapsd_mask(mac_ctx->psoc, &value);
			if (!QDF_IS_STATUS_SUCCESS(status))
				return;
			session->modifyProfileFields.uapsd_mask = value;
		}
	}
	csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED, vdev_id);
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	wlan_mlme_get_mac_vdev_id(mac_ctx->pdev, vdev_id, &roam_info->bssid);

	/* We are done with the IEs so free it */
	/*
	 * Only set context for non-WDS_STA. We don't even need it for
	 * WDS_AP. But since the encryption.
	 * is WPA2-PSK so it won't matter.
	 */
	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
		if (wlan_is_open_wep_cipher(mac_ctx->pdev, vdev_id)) {
			/* NO keys. these key parameters don't matter */
			csr_issue_set_context_req_helper(mac_ctx, vdev_id,
							 &bcast_mac, false,
							 false, eSIR_TX_RX,
							 0, 0, NULL);
		}
	}

	/*
	 * Only tell upper layer is we start the BSS because Vista doesn't like
	 * multiple connection indications. If we don't start the BSS ourself,
	 * handler of eSIR_SME_JOINED_NEW_BSS will trigger the connection start
	 * indication in Vista
	 */
	roam_info->staId = (uint8_t)start_bss_rsp->staId;
	if (opmode == QDF_NDI_MODE) {
		csr_roam_update_ndp_return_params(mac_ctx,
						  CSR_SAP_START_BSS_SUCCESS,
						   &roam_status,
						   &roam_result,
						    roam_info);
	}
	/*
	 * Only tell upper layer is we start the BSS because Vista
	 * doesn't like multiple connection indications. If we don't
	 * start the BSS ourself, handler of eSIR_SME_JOINED_NEW_BSS
	 * will trigger the connection start indication in Vista
	 */
	roam_info->status_code = eSIR_SME_SUCCESS;
	csr_roam_call_callback(mac_ctx, vdev_id, roam_info,
			       roam_status, roam_result);
}

static void csr_flush_pending_start_bss_cmd(struct mac_context *mac_ctx,
					     uint8_t vdev_id)
{
	struct wlan_serialization_queued_cmd_info cmd = {0};
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev not found for id %d", vdev_id);
		return;
	}

	/* Flush any pending vdev start command */
	cmd.vdev = vdev;
	cmd.cmd_type = WLAN_SER_CMD_VDEV_START_BSS;
	cmd.req_type = WLAN_SER_CANCEL_VDEV_NON_SCAN_CMD_TYPE;
	cmd.requestor = WLAN_UMAC_COMP_MLME;
	cmd.queue_type = WLAN_SERIALIZATION_PENDING_QUEUE;

	wlan_serialization_cancel_request(&cmd);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

bool cm_csr_is_ss_wait_for_key(uint8_t vdev_id)
{
	struct mac_context *mac;
	bool is_wait_for_key = false;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return is_wait_for_key;
	}

	spin_lock(&mac->roam.roam_state_lock);
	if (CSR_IS_WAIT_FOR_KEY(mac, vdev_id))
		is_wait_for_key = true;
	spin_unlock(&mac->roam.roam_state_lock);

	return is_wait_for_key;
}

void cm_csr_set_ss_wait_for_key(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY,
				 vdev_id);
}

void cm_csr_set_joining(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING, vdev_id);
}

void cm_csr_set_joined(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINED, vdev_id);
}

void cm_csr_set_idle(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_state_change(mac, eCSR_ROAMING_STATE_IDLE, vdev_id);
}

void cm_csr_set_ss_none(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
				 vdev_id);
}

QDF_STATUS csr_roam_ndi_stop(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	bool is_vdev_up;
	bool is_start_bss_in_active_q = false;

	csr_flush_pending_start_bss_cmd(mac_ctx, vdev_id);

	is_vdev_up = wlan_is_vdev_id_up(mac_ctx->pdev, vdev_id);
	if (wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
					      WLAN_SER_CMD_VDEV_START_BSS))
		is_start_bss_in_active_q = true;

	sme_debug("vdev_id: %d is_vdev_up %d is_start_bss_in_active_q %d",
		  vdev_id, is_vdev_up, is_start_bss_in_active_q);

	if (is_vdev_up || is_start_bss_in_active_q)
		status = csr_roam_issue_stop_bss_cmd(mac_ctx, vdev_id,
						     eCSR_BSS_TYPE_NDI);
	return status;
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void csr_fill_single_pmk(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				struct bss_description *bss_desc)
{
	struct cm_roam_values_copy src_cfg = {};

	src_cfg.bool_value = bss_desc->is_single_pmk;
	wlan_cm_roam_cfg_set_value(psoc, vdev_id,
				   IS_SINGLE_PMK, &src_cfg);
}
#else
static inline void
csr_fill_single_pmk(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    struct bss_description *bss_desc)
{}
#endif

void csr_roam_roaming_state_stop_bss_rsp_processor(struct mac_context *mac,
						   void *msg)
{
	struct stop_bss_rsp *stop_bss_rsp = (struct stop_bss_rsp *)msg;
	uint8_t vdev_id = stop_bss_rsp->vdev_id;
	enum csr_sap_response_type result_code = CSR_SAP_STOP_BSS_SUCCESS;

	sme_debug("Received stop bss rsp on vdev: %d", vdev_id);
	mac->roam.roamSession[vdev_id].connectState =
					eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	if (CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac, vdev_id)) {
		if (stop_bss_rsp->status_code != eSIR_SME_SUCCESS)
			result_code = CSR_SAP_STOP_BSS_FAILURE;
		csr_process_sap_response(mac, result_code, NULL, vdev_id);
	}
}

static
void csr_roam_roaming_state_disassoc_rsp_processor(struct mac_context *mac,
						   struct disassoc_rsp *rsp)
{
	struct csr_roam_session *pSession;

	sme_debug("Received disassoc response for vdev : %d status : %d",
		  rsp->sessionId, rsp->status_code);

	pSession = CSR_GET_SESSION(mac, rsp->sessionId);
	if (!pSession) {
		sme_err("session not found");
		return;
	}

	csr_roam_complete(mac, rsp->sessionId);
}

static void csr_roam_roaming_state_deauth_rsp_processor(struct mac_context *mac,
						struct deauth_rsp *rsp)
{
	csr_roam_complete(mac, rsp->sessionId);
}

void
csr_roam_roaming_state_start_bss_rsp_processor(struct mac_context *mac,
					       void *msg)
{
	enum csr_sap_response_type result;
	struct start_bss_rsp *start_bss_rsp = (struct start_bss_rsp *)msg;

	if (!CSR_IS_ROAM_SUBSTATE_START_BSS_REQ(mac, start_bss_rsp->vdev_id)) {
		sme_err(" Start bss received in invalid state");
		return;
	}

	sme_debug("Start Bss response status : %d",
		  start_bss_rsp->status_code);
	if (start_bss_rsp->status_code == eSIR_SME_SUCCESS)
		result = CSR_SAP_START_BSS_SUCCESS;
	else
		result = CSR_SAP_START_BSS_FAILURE;

	csr_process_sap_response(mac, result, start_bss_rsp,
				 start_bss_rsp->vdev_id);
}


/**
 * csr_roam_send_disconnect_done_indication() - Send disconnect ind to HDD.
 *
 * @mac_ctx: mac global context
 * @msg_ptr: incoming message
 *
 * This function gives final disconnect event to HDD after all cleanup in
 * lower layers is done.
 *
 * Return: None
 */
static void
csr_roam_send_disconnect_done_indication(struct mac_context *mac_ctx,
					 tSirSmeRsp *msg_ptr)
{
	struct sir_sme_discon_done_ind *discon_ind =
				(struct sir_sme_discon_done_ind *)(msg_ptr);
	struct csr_roam_info *roam_info;
	uint8_t vdev_id;

	vdev_id = discon_ind->session_id;
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;

	sme_debug("DISCONNECT_DONE_IND RC:%d", discon_ind->reason_code);

	if (CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		roam_info->reasonCode = discon_ind->reason_code;
		roam_info->status_code = eSIR_SME_STA_NOT_ASSOCIATED;
		qdf_mem_copy(roam_info->peerMac.bytes, discon_ind->peer_mac,
			     ETH_ALEN);

		roam_info->rssi = mac_ctx->peer_rssi;
		roam_info->tx_rate = mac_ctx->peer_txrate;
		roam_info->rx_rate = mac_ctx->peer_rxrate;
		roam_info->disassoc_reason = discon_ind->reason_code;
		roam_info->rx_mc_bc_cnt = mac_ctx->rx_mc_bc_cnt;
		roam_info->rx_retry_cnt = mac_ctx->rx_retry_cnt;
		csr_roam_call_callback(mac_ctx, vdev_id,
				       roam_info, eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_DISASSOC_IND);
	} else {
		sme_err("Inactive vdev_id %d", vdev_id);
	}

	/*
	 * Release WM status change command as eWNI_SME_DISCONNECT_DONE_IND
	 * has been sent to HDD and there is nothing else left to do.
	 */
	csr_roam_wm_status_change_complete(mac_ctx, vdev_id);
	qdf_mem_free(roam_info);
}

/**
 * csr_roaming_state_msg_processor() - process roaming messages
 * @mac:       mac global context
 * @msg_buf:    message buffer
 *
 * We need to be careful on whether to cast msg_buf (pSmeRsp) to other type of
 * structures. It depends on how the message is constructed. If the message is
 * sent by lim_send_sme_rsp, the msg_buf is only a generic response and can only
 * be used as pointer to tSirSmeRsp. For the messages where sender allocates
 * memory for specific structures, then it can be cast accordingly.
 *
 * Return: status of operation
 */
void csr_roaming_state_msg_processor(struct mac_context *mac, void *msg_buf)
{
	tSirSmeRsp *pSmeRsp;

	pSmeRsp = (tSirSmeRsp *)msg_buf;

	switch (pSmeRsp->messageType) {
	case eWNI_SME_DISASSOC_RSP:
		/* or the Disassociate response message... */
		if (CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(mac, pSmeRsp->vdev_id)) {
			sme_debug("eWNI_SME_DISASSOC_RSP subState = %s",
				  mac_trace_getcsr_roam_sub_state(
				  mac->roam.curSubState[pSmeRsp->vdev_id]));
			csr_roam_roaming_state_disassoc_rsp_processor(mac,
						(struct disassoc_rsp *) pSmeRsp);
		}
		break;
	case eWNI_SME_DEAUTH_RSP:
		/* or the Deauthentication response message... */
		if (CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ(mac, pSmeRsp->vdev_id))
			csr_roam_roaming_state_deauth_rsp_processor(mac,
						(struct deauth_rsp *) pSmeRsp);
		break;
	/* In case CSR issues STOP_BSS, we need to tell HDD about peer departed
	 * because PE is removing them
	 */
	case eWNI_SME_TRIGGER_SAE:
		sme_debug("Invoke SAE callback");
		csr_sae_callback(mac, pSmeRsp);
		break;

	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_check_for_link_status_change(mac, pSmeRsp);
		break;

	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(mac, pSmeRsp);
		break;

	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
		csr_roam_joined_state_msg_processor(mac, pSmeRsp);
		break;
	default:
		sme_debug("Unexpected message type: %d[0x%X] received in substate %s",
			pSmeRsp->messageType, pSmeRsp->messageType,
			mac_trace_getcsr_roam_sub_state(
				mac->roam.curSubState[pSmeRsp->vdev_id]));
		/* If we are connected, check the link status change */
		if (!csr_is_conn_state_disconnected(mac, pSmeRsp->vdev_id))
			csr_roam_check_for_link_status_change(mac, pSmeRsp);
		break;
	}
}

#ifdef WLAN_FEATURE_11BE_MLO
static void
csr_roam_assoc_cnf_mld_copy(struct csr_roam_info *roam_info,
			    tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf,
			    uint32_t num_bytes)
{
	qdf_mem_copy(roam_info->peer_mld.bytes,
		     pUpperLayerAssocCnf->peer_mld_addr,
		     num_bytes);
}
#else /* WLAN_FEATURE_11BE_MLO */
static inline void
csr_roam_assoc_cnf_mld_copy(struct csr_roam_info *roam_info,
			    tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf,
			    uint32_t num_bytes)
{
}
#endif /* WLAN_FEATURE_11BE_MLO */

void csr_roam_joined_state_msg_processor(struct mac_context *mac, void *msg_buf)
{
	tSirSmeRsp *pSirMsg = (tSirSmeRsp *)msg_buf;

	switch (pSirMsg->messageType) {
	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
	{
		struct csr_roam_session *pSession;
		tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf;
		struct csr_roam_info *roam_info;
		uint32_t sessionId;
		QDF_STATUS status;
		enum QDF_OPMODE opmode;

		sme_debug("ASSOCIATION confirmation can be given to upper layer ");
		pUpperLayerAssocCnf =
			(tSirSmeAssocIndToUpperLayerCnf *)msg_buf;
		status = csr_roam_get_session_id_from_bssid(mac,
							(struct qdf_mac_addr *)
							   pUpperLayerAssocCnf->
							   bssId, &sessionId);
		pSession = CSR_GET_SESSION(mac, sessionId);

		if (!pSession) {
			sme_err("session %d not found", sessionId);
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}

		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info) {
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}
		opmode = wlan_get_opmode_vdev_id(mac->pdev, sessionId);
		/* send the status code as Success */
		roam_info->status_code = eSIR_SME_SUCCESS;
		roam_info->staId = (uint8_t) pUpperLayerAssocCnf->aid;
		roam_info->rsnIELen =
			(uint8_t) pUpperLayerAssocCnf->rsnIE.length;
		roam_info->prsnIE =
			pUpperLayerAssocCnf->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
		roam_info->wapiIELen =
			(uint8_t) pUpperLayerAssocCnf->wapiIE.length;
		roam_info->pwapiIE =
			pUpperLayerAssocCnf->wapiIE.wapiIEdata;
#endif
		roam_info->addIELen =
			(uint8_t) pUpperLayerAssocCnf->addIE.length;
		roam_info->paddIE =
			pUpperLayerAssocCnf->addIE.addIEdata;
		qdf_mem_copy(roam_info->peerMac.bytes,
			     pUpperLayerAssocCnf->peerMacAddr,
			     sizeof(tSirMacAddr));
		csr_roam_assoc_cnf_mld_copy(roam_info,
					    pUpperLayerAssocCnf,
					    sizeof(tSirMacAddr));
		qdf_mem_copy(&roam_info->bssid,
			     pUpperLayerAssocCnf->bssId,
			     sizeof(struct qdf_mac_addr));
		roam_info->wmmEnabledSta =
			pUpperLayerAssocCnf->wmmEnabledSta;
		roam_info->timingMeasCap =
			pUpperLayerAssocCnf->timingMeasCap;
		qdf_mem_copy(&roam_info->chan_info,
			     &pUpperLayerAssocCnf->chan_info,
			     sizeof(struct oem_channel_info));

		roam_info->ampdu = pUpperLayerAssocCnf->ampdu;
		roam_info->sgi_enable = pUpperLayerAssocCnf->sgi_enable;
		roam_info->tx_stbc = pUpperLayerAssocCnf->tx_stbc;
		roam_info->rx_stbc = pUpperLayerAssocCnf->rx_stbc;
		roam_info->ch_width = pUpperLayerAssocCnf->ch_width;
		roam_info->mode = pUpperLayerAssocCnf->mode;
		roam_info->max_supp_idx = pUpperLayerAssocCnf->max_supp_idx;
		roam_info->max_ext_idx = pUpperLayerAssocCnf->max_ext_idx;
		roam_info->max_mcs_idx = pUpperLayerAssocCnf->max_mcs_idx;
		roam_info->max_real_mcs_idx =
					pUpperLayerAssocCnf->max_real_mcs_idx;
		roam_info->rx_mcs_map = pUpperLayerAssocCnf->rx_mcs_map;
		roam_info->tx_mcs_map = pUpperLayerAssocCnf->tx_mcs_map;
		roam_info->ecsa_capable = pUpperLayerAssocCnf->ecsa_capable;
		roam_info->ext_cap = pUpperLayerAssocCnf->ext_cap;
		roam_info->supported_band =
					pUpperLayerAssocCnf->supported_band;
		if (pUpperLayerAssocCnf->ht_caps.present)
			roam_info->ht_caps = pUpperLayerAssocCnf->ht_caps;
		if (pUpperLayerAssocCnf->vht_caps.present)
			roam_info->vht_caps = pUpperLayerAssocCnf->vht_caps;
		roam_info->capability_info =
					pUpperLayerAssocCnf->capability_info;
		roam_info->he_caps_present =
					pUpperLayerAssocCnf->he_caps_present;

		if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
			if (pUpperLayerAssocCnf->ies_len > 0) {
				roam_info->assocReqLength =
						pUpperLayerAssocCnf->ies_len;
				roam_info->assocReqPtr =
						pUpperLayerAssocCnf->ies;
			}

			mac->roam.roamSession[sessionId].connectState =
				eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED;
			roam_info->fReassocReq =
				pUpperLayerAssocCnf->reassocReq;
			status = csr_roam_call_callback(mac, sessionId,
						       roam_info,
						       eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF);
		}
		if (pUpperLayerAssocCnf->ies)
			qdf_mem_free(pUpperLayerAssocCnf->ies);
		qdf_mem_free(roam_info);
	}
	break;
	default:
		csr_roam_check_for_link_status_change(mac, pSirMsg);
		break;
	}
}

/**
 * csr_update_wep_key_peer_macaddr() - Update wep key peer mac addr
 * @vdev: vdev object
 * @crypto_key: crypto key info
 * @unicast: uncast or broadcast
 * @mac_addr: peer mac address
 *
 * Update peer mac address to key context before set wep key to target.
 *
 * Return void
 */
static void
csr_update_wep_key_peer_macaddr(struct wlan_objmgr_vdev *vdev,
				struct wlan_crypto_key *crypto_key,
				bool unicast,
				struct qdf_mac_addr *mac_addr)
{
	if (!crypto_key || !vdev) {
		sme_err("vdev or crytpo_key null");
		return;
	}

	if (unicast) {
		qdf_mem_copy(&crypto_key->macaddr, mac_addr,
			     QDF_MAC_ADDR_SIZE);
	} else {
		if (vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE ||
		    vdev->vdev_mlme.vdev_opmode == QDF_P2P_CLIENT_MODE)
			qdf_mem_copy(&crypto_key->macaddr, mac_addr,
				     QDF_MAC_ADDR_SIZE);
		else
			qdf_mem_copy(&crypto_key->macaddr,
				     vdev->vdev_mlme.macaddr,
				     QDF_MAC_ADDR_SIZE);
	}
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_set_ctx_rsp(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct set_context_rsp *pRsp)
{
	WLAN_HOST_DIAG_EVENT_DEF(setKeyEvent,
				 host_event_wlan_security_payload_type);
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac_ctx->pdev,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	if (cm_is_open_mode(vdev)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	qdf_mem_zero(&setKeyEvent,
		     sizeof(host_event_wlan_security_payload_type));
	if (qdf_is_macaddr_group(&pRsp->peer_macaddr))
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_BCAST_RSP;
	else
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_UNICAST_RSP;
	cm_diag_get_auth_enc_type_vdev_id(mac_ctx->psoc,
					  &setKeyEvent.authMode,
					  &setKeyEvent.encryptionModeUnicast,
					  &setKeyEvent.encryptionModeMulticast,
					  session->vdev_id);
	wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, session->vdev_id,
				    (struct qdf_mac_addr *)&setKeyEvent.bssid);
	if (eSIR_SME_SUCCESS != pRsp->status_code)
		setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
	WLAN_HOST_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
}
#else /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
static void
csr_roam_diag_set_ctx_rsp(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct set_context_rsp *pRsp)
{
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS
csr_roam_send_rso_enable(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_vdev *assoc_vdev = NULL;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		sme_err("vdev object is NULL for vdev %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	    mlo_check_if_all_links_up(vdev)) {
		assoc_vdev = wlan_mlo_get_assoc_link_vdev(vdev);
		if (!assoc_vdev) {
			sme_err("Assoc vdev is null");
			wlan_objmgr_vdev_release_ref(vdev,
						     WLAN_MLME_OBJMGR_ID);
			return QDF_STATUS_E_FAILURE;
		}
		cm_roam_start_init_on_connect(mac_ctx->pdev,
					      wlan_vdev_get_id(assoc_vdev));
	} else if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		cm_roam_start_init_on_connect(mac_ctx->pdev, vdev_id);
	}
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_MLME_OBJMGR_ID);
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS
csr_roam_send_rso_enable(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	cm_roam_start_init_on_connect(mac_ctx->pdev, vdev_id);
	return QDF_STATUS_SUCCESS;
}
#endif

static void
csr_roam_chk_lnk_set_ctx_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	qdf_freq_t chan_freq;
	struct csr_roam_info *roam_info;
	eCsrRoamResult result = eCSR_ROAM_RESULT_NONE;
	struct set_context_rsp *pRsp = (struct set_context_rsp *)msg_ptr;
	struct qdf_mac_addr connected_bssid;

	if (!pRsp) {
		sme_err("set key response is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sessionId = pRsp->sessionId;
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}

	csr_roam_diag_set_ctx_rsp(mac_ctx, session, pRsp);
	chan_freq = wlan_get_operation_chan_freq_vdev_id(mac_ctx->pdev,
							 sessionId);
	wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, sessionId,
				    &connected_bssid);
	sme_debug("vdev %d, Status %d, peer_macaddr "QDF_MAC_ADDR_FMT " obss offload %d freq %d opmode %d",
		  pRsp->sessionId, pRsp->status_code,
		  QDF_MAC_ADDR_REF(pRsp->peer_macaddr.bytes),
		  mac_ctx->obss_scan_offload, chan_freq,
		  wlan_get_opmode_vdev_id(mac_ctx->pdev, sessionId));

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, sessionId)) {
		/* We are done with authentication, whethere succeed or not */
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
		cm_stop_wait_for_key_timer(mac_ctx->psoc, sessionId);
		if (QDF_IS_STATUS_ERROR(csr_roam_send_rso_enable(mac_ctx,
								 sessionId))) {
			qdf_mem_free(roam_info);
			return;
		}
	}
	if (eSIR_SME_SUCCESS == pRsp->status_code) {
		qdf_copy_macaddr(&roam_info->peerMac, &pRsp->peer_macaddr);
		/* Make sure we install the GTK before indicating to HDD as
		 * authenticated. This is to prevent broadcast packets go out
		 * after PTK and before GTK.
		 */
		if (qdf_is_macaddr_broadcast(&pRsp->peer_macaddr)) {
			/*
			 * OBSS SCAN Indication will be sent to Firmware
			 * to start OBSS Scan
			 */
			if (mac_ctx->obss_scan_offload &&
			    wlan_reg_is_24ghz_ch_freq(chan_freq) &&
			    cm_is_vdevid_connected(mac_ctx->pdev, sessionId)) {
				struct sme_obss_ht40_scanind_msg *msg;

				msg = qdf_mem_malloc(sizeof(
					struct sme_obss_ht40_scanind_msg));
				if (!msg) {
					qdf_mem_free(roam_info);
					return;
				}

				msg->msg_type = eWNI_SME_HT40_OBSS_SCAN_IND;
				msg->length =
				      sizeof(struct sme_obss_ht40_scanind_msg);
				qdf_copy_macaddr(&msg->mac_addr,
					&connected_bssid);
				status = umac_send_mb_message_to_mac(msg);
			}
			result = eCSR_ROAM_RESULT_AUTHENTICATED;
		} else {
			result = eCSR_ROAM_RESULT_NONE;
		}
	} else {
		result = eCSR_ROAM_RESULT_FAILURE;
		sme_err(
			"CSR: setkey command failed(err=%d) PeerMac "
			QDF_MAC_ADDR_FMT,
			pRsp->status_code,
			QDF_MAC_ADDR_REF(pRsp->peer_macaddr.bytes));
	}
	csr_roam_call_callback(mac_ctx, sessionId, roam_info,
			       eCSR_ROAM_SET_KEY_COMPLETE, result);
	/* Indicate SME_QOS that the SET_KEY is completed, so that SME_QOS
	 * can go ahead and initiate the TSPEC if any are pending
	 */
	sme_qos_csr_event_ind(mac_ctx, (uint8_t)sessionId,
			      SME_QOS_CSR_SET_KEY_SUCCESS_IND, NULL);
#ifdef FEATURE_WLAN_ESE
	/* Send Adjacent AP repot to new AP. */
	if (result == eCSR_ROAM_RESULT_AUTHENTICATED &&
	    session->isPrevApInfoValid &&
	    wlan_cm_get_ese_assoc(mac_ctx->pdev, sessionId)) {
		csr_send_ese_adjacent_ap_rep_ind(mac_ctx, session);
		session->isPrevApInfoValid = false;
	}
#endif
	qdf_mem_free(roam_info);
}

static QDF_STATUS csr_roam_issue_set_context_req(struct mac_context *mac_ctx,
						 uint32_t session_id,
						 bool add_key, bool unicast,
						 uint8_t key_idx,
						 struct qdf_mac_addr *mac_addr)
{
	enum wlan_crypto_cipher_type cipher;
	struct wlan_crypto_key *crypto_key;
	uint8_t wep_key_idx = 0;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("VDEV object not found for session_id %d", session_id);
		return QDF_STATUS_E_INVAL;
	}
	cipher = wlan_crypto_get_cipher(vdev, unicast, key_idx);
	if (IS_WEP_CIPHER(cipher)) {
		wep_key_idx = wlan_crypto_get_default_key_idx(vdev, !unicast);
		crypto_key = wlan_crypto_get_key(vdev, wep_key_idx);
		csr_update_wep_key_peer_macaddr(vdev, crypto_key, unicast,
						mac_addr);
	} else {
		crypto_key = wlan_crypto_get_key(vdev, key_idx);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	sme_debug("session:%d, cipher:%d, ucast:%d, idx:%d, wep:%d, add:%d",
		  session_id, cipher, unicast, key_idx, wep_key_idx, add_key);
	if (!IS_WEP_CIPHER(cipher) && !add_key)
		return QDF_STATUS_E_INVAL;

	return ucfg_crypto_set_key_req(vdev, crypto_key, (unicast ?
				       WLAN_CRYPTO_KEY_TYPE_UNICAST :
				       WLAN_CRYPTO_KEY_TYPE_GROUP));
}

QDF_STATUS
csr_issue_set_context_req_helper(struct mac_context *mac_ctx,
				 uint32_t session_id, tSirMacAddr *bssid,
				 bool addkey, bool unicast,
				 tAniKeyDirection key_direction, uint8_t key_id,
				 uint16_t key_length, uint8_t *key)
{
	return csr_roam_issue_set_context_req(mac_ctx, session_id, addkey,
					      unicast, key_id,
					      (struct qdf_mac_addr *)bssid);
}

static
bool csr_roam_issue_wm_status_change(struct mac_context *mac, uint32_t sessionId,
				     enum csr_roam_wmstatus_changetypes Type,
				     tSirSmeRsp *pSmeRsp)
{
	bool fCommandQueued = false;
	tSmeCmd *pCommand;

	do {
		/* Validate the type is ok... */
		if ((eCsrDisassociated != Type)
		    && (eCsrDeauthenticated != Type))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err(" fail to get command buffer");
			break;
		}

		pCommand->command = eSmeCommandWmStatusChange;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.wmStatusChangeCmd.Type = Type;
		if (eCsrDisassociated == Type) {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DisassocIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DisassocIndMsg));
		} else {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DeauthIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DeauthIndMsg));
		}
		if (QDF_IS_STATUS_SUCCESS
			    (csr_queue_sme_command(mac, pCommand, false)))
			fCommandQueued = true;
		else
			sme_err("fail to send message");
	} while (0);
	return fCommandQueued;
}

static void csr_update_snr(struct mac_context *mac, void *pMsg)
{
	tAniGetSnrReq *pGetSnrReq = (tAniGetSnrReq *) pMsg;

	if (pGetSnrReq) {
		if (QDF_STATUS_SUCCESS != wma_get_snr(pGetSnrReq)) {
			sme_err("Error in wma_get_snr");
			return;
		}

	} else
		sme_err("pGetSnrReq is NULL");
}

/**
 * csr_translate_akm_type() - Convert ani_akm_type value to equivalent
 * enum csr_akm_type
 * @akm_type: value of type ani_akm_type
 *
 * Return: enum csr_akm_type value
 */
static enum csr_akm_type csr_translate_akm_type(enum ani_akm_type akm_type)
{
	enum csr_akm_type csr_akm_type;

	switch (akm_type)
	{
	case ANI_AKM_TYPE_NONE:
		csr_akm_type = eCSR_AUTH_TYPE_NONE;
		break;
#ifdef WLAN_FEATURE_SAE
	case ANI_AKM_TYPE_SAE:
		csr_akm_type = eCSR_AUTH_TYPE_SAE;
		break;
#endif
	case ANI_AKM_TYPE_WPA:
		csr_akm_type = eCSR_AUTH_TYPE_WPA;
		break;
	case ANI_AKM_TYPE_WPA_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_WPA_PSK;
		break;
	case ANI_AKM_TYPE_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_RSN;
		break;
	case ANI_AKM_TYPE_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK;
		break;
	case ANI_AKM_TYPE_FT_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN;
		break;
	case ANI_AKM_TYPE_FT_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN_PSK;
		break;
#ifdef FEATURE_WLAN_ESE
	case ANI_AKM_TYPE_CCKM:
		csr_akm_type = eCSR_AUTH_TYPE_CCKM_RSN;
		break;
#endif
	case ANI_AKM_TYPE_RSN_PSK_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK_SHA256;
		break;
	case ANI_AKM_TYPE_RSN_8021X_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_8021X_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_DPP_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_DPP_RSN;
		break;
	case ANI_AKM_TYPE_OWE:
		csr_akm_type = eCSR_AUTH_TYPE_OWE;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA256;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA384;
		break;
	case ANI_AKM_TYPE_OSEN:
		csr_akm_type = eCSR_AUTH_TYPE_OSEN;
		break;
	default:
		csr_akm_type = eCSR_AUTH_TYPE_UNKNOWN;
	}

	return csr_akm_type;
}

static bool csr_is_sae_akm_present(tDot11fIERSN * const rsn_ie)
{
	uint16_t i;

	if (rsn_ie->akm_suite_cnt > 6) {
		sme_debug("Invalid akm_suite_cnt in Rx RSN IE");
		return false;
	}

	for (i = 0; i < rsn_ie->akm_suite_cnt; i++) {
		if (LE_READ_4(rsn_ie->akm_suite[i]) == RSN_AUTH_KEY_MGMT_SAE) {
			sme_debug("SAE AKM present");
			return true;
		}
	}
	return false;
}

static bool csr_is_sae_peer_allowed(struct mac_context *mac_ctx,
				    struct assoc_ind *assoc_ind,
				    struct csr_roam_session *session,
				    tDot11fIERSN *rsn_ie,
				    enum wlan_status_code *mac_status_code)
{
	bool is_allowed = false;
	uint8_t *peer_mac_addr;

	/* Allow the peer if it's SAE authenticated */
	if (assoc_ind->is_sae_authenticated)
		return true;

	/* Use peer MLD address to find PMKID
	 * if MLD address is valid
	 */
	peer_mac_addr = assoc_ind->peer_mld_addr;
	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)peer_mac_addr))
		peer_mac_addr = assoc_ind->peerMacAddr;

	/* Allow the peer with valid PMKID */
	if (!rsn_ie->pmkid_count) {
		*mac_status_code = STATUS_NOT_SUPPORTED_AUTH_ALG;
		sme_debug("No PMKID present in RSNIE; Tried to use SAE AKM after non-SAE authentication");
	} else if (csr_is_pmkid_found_for_peer(mac_ctx, session, peer_mac_addr,
					       &rsn_ie->pmkid[0][0],
					       rsn_ie->pmkid_count)) {
		sme_debug("Valid PMKID found for SAE peer");
		is_allowed = true;
	} else {
		*mac_status_code = STATUS_INVALID_PMKID;
		sme_debug("No valid PMKID found for SAE peer");
	}

	return is_allowed;
}

#ifdef WLAN_FEATURE_11BE_MLO
static void
csr_send_assoc_ind_to_upper_layer_mac_copy(tSirSmeAssocIndToUpperLayerCnf *cnf,
					   struct assoc_ind *ind)
{
	qdf_mem_copy(&cnf->peer_mld_addr, &ind->peer_mld_addr,
		     sizeof(cnf->peer_mld_addr));
}
#else /* WLAN_FEATURE_11BE_MLO */
static inline void
csr_send_assoc_ind_to_upper_layer_mac_copy(tSirSmeAssocIndToUpperLayerCnf *cnf,
					   struct assoc_ind *ind)
{
}
#endif /* WLAN_FEATURE_11BE_MLO */

static QDF_STATUS
csr_send_assoc_ind_to_upper_layer_cnf_msg(struct mac_context *mac,
					  struct assoc_ind *ind,
					  QDF_STATUS status,
					  uint8_t vdev_id)
{
	struct scheduler_msg msg = {0};
	tSirSmeAssocIndToUpperLayerCnf *cnf;

	cnf = qdf_mem_malloc(sizeof(*cnf));
	if (!cnf)
		return QDF_STATUS_E_NOMEM;

	cnf->messageType = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
	cnf->length = sizeof(*cnf);
	cnf->sessionId = vdev_id;

	if (QDF_IS_STATUS_SUCCESS(status))
		cnf->status_code = eSIR_SME_SUCCESS;
	else
		cnf->status_code = eSIR_SME_ASSOC_REFUSED;
	qdf_mem_copy(&cnf->bssId, &ind->bssId, sizeof(cnf->bssId));
	qdf_mem_copy(&cnf->peerMacAddr, &ind->peerMacAddr,
		     sizeof(cnf->peerMacAddr));
	csr_send_assoc_ind_to_upper_layer_mac_copy(cnf, ind);
	cnf->aid = ind->staId;
	cnf->wmmEnabledSta = ind->wmmEnabledSta;
	cnf->rsnIE = ind->rsnIE;
#ifdef FEATURE_WLAN_WAPI
	cnf->wapiIE = ind->wapiIE;
#endif
	cnf->addIE = ind->addIE;
	cnf->reassocReq = ind->reassocReq;
	cnf->timingMeasCap = ind->timingMeasCap;
	cnf->chan_info = ind->chan_info;
	cnf->ampdu = ind->ampdu;
	cnf->sgi_enable = ind->sgi_enable;
	cnf->tx_stbc = ind->tx_stbc;
	cnf->ch_width = ind->ch_width;
	cnf->mode = ind->mode;
	cnf->rx_stbc = ind->rx_stbc;
	cnf->max_supp_idx = ind->max_supp_idx;
	cnf->max_ext_idx = ind->max_ext_idx;
	cnf->max_mcs_idx = ind->max_mcs_idx;
	cnf->max_real_mcs_idx = ind->max_real_mcs_idx;
	cnf->rx_mcs_map = ind->rx_mcs_map;
	cnf->tx_mcs_map = ind->tx_mcs_map;
	cnf->ecsa_capable = ind->ecsa_capable;
	cnf->ext_cap = ind->ext_cap;
	cnf->supported_band = ind->supported_band;
	if (ind->HTCaps.present)
		cnf->ht_caps = ind->HTCaps;
	if (ind->VHTCaps.present)
		cnf->vht_caps = ind->VHTCaps;
	cnf->capability_info = ind->capability_info;
	cnf->he_caps_present = ind->he_caps_present;
	if (ind->assocReqPtr) {
		if (ind->assocReqLength < MAX_ASSOC_REQ_IE_LEN) {
			cnf->ies = qdf_mem_malloc(ind->assocReqLength);
			if (!cnf->ies) {
				qdf_mem_free(cnf);
				return QDF_STATUS_E_NOMEM;
			}
			cnf->ies_len = ind->assocReqLength;
			qdf_mem_copy(cnf->ies, ind->assocReqPtr,
				     cnf->ies_len);
		} else {
			sme_err("Assoc Ie length is too long");
		}
	}

	msg.type = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
	msg.bodyptr = cnf;
	sys_process_mmh_msg(mac, &msg);

	return QDF_STATUS_SUCCESS;
}

static void
csr_roam_chk_lnk_assoc_ind_upper_layer(
		struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	struct assoc_ind *assoc_ind;
	QDF_STATUS status;

	assoc_ind = (struct assoc_ind *)msg_ptr;
	status = csr_roam_get_session_id_from_bssid(
			mac_ctx, (struct qdf_mac_addr *)assoc_ind->bssId,
			&session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		goto free_mem;
	}
	csr_send_assoc_ind_to_upper_layer_cnf_msg(
					mac_ctx, assoc_ind, status, session_id);
	/*in the association response tx compete case,
	 *memory for assoc_ind->assocReqPtr will be malloced
	 *in the lim_assoc_rsp_tx_complete -> lim_fill_sme_assoc_ind_params
	 *and then assoc_ind will pass here, so after using it
	 *in the csr_send_assoc_ind_to_upper_layer_cnf_msg and
	 *then free the memory here.
	 */
free_mem:
	if (assoc_ind->assocReqLength != 0 && assoc_ind->assocReqPtr)
		qdf_mem_free(assoc_ind->assocReqPtr);
}

static void
csr_roam_chk_lnk_assoc_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct assoc_ind *pAssocInd;
	enum wlan_status_code mac_status_code = STATUS_SUCCESS;
	enum csr_akm_type csr_akm_type;
	enum QDF_OPMODE opmode;

	sme_debug("Receive WNI_SME_ASSOC_IND from SME");
	pAssocInd = (struct assoc_ind *) msg_ptr;
	sme_debug("Receive WNI_SME_ASSOC_IND from SME vdev id %d",
		  pAssocInd->sessionId);
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				(struct qdf_mac_addr *) pAssocInd->bssId,
				&sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID" QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(pAssocInd->bssId));
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	csr_akm_type = csr_translate_akm_type(pAssocInd->akm_type);

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	opmode = wlan_get_opmode_vdev_id(mac_ctx->pdev, sessionId);
	/* Required for indicating the frames to upper layer */
	roam_info->assocReqLength = pAssocInd->assocReqLength;
	roam_info->assocReqPtr = pAssocInd->assocReqPtr;
	roam_info->status_code = eSIR_SME_SUCCESS;
	roam_info->staId = (uint8_t)pAssocInd->staId;
	roam_info->rsnIELen = (uint8_t)pAssocInd->rsnIE.length;
	roam_info->prsnIE = pAssocInd->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
	roam_info->wapiIELen = (uint8_t)pAssocInd->wapiIE.length;
	roam_info->pwapiIE = pAssocInd->wapiIE.wapiIEdata;
#endif
	roam_info->addIELen = (uint8_t)pAssocInd->addIE.length;
	roam_info->paddIE = pAssocInd->addIE.addIEdata;
	roam_info->fReassocReq = pAssocInd->reassocReq;
	qdf_mem_copy(roam_info->peerMac.bytes,
		     pAssocInd->peerMacAddr,
		     sizeof(tSirMacAddr));
	qdf_mem_copy(roam_info->bssid.bytes,
		     pAssocInd->bssId,
		     sizeof(struct qdf_mac_addr));
	roam_info->wmmEnabledSta = pAssocInd->wmmEnabledSta;
	roam_info->timingMeasCap = pAssocInd->timingMeasCap;
	roam_info->ecsa_capable = pAssocInd->ecsa_capable;
	qdf_mem_copy(&roam_info->chan_info,
		     &pAssocInd->chan_info,
		     sizeof(struct oem_channel_info));

	if (pAssocInd->HTCaps.present)
		qdf_mem_copy(&roam_info->ht_caps,
			     &pAssocInd->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	if (pAssocInd->VHTCaps.present)
		qdf_mem_copy(&roam_info->vht_caps,
			     &pAssocInd->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	roam_info->capability_info = pAssocInd->capability_info;
	roam_info->he_caps_present = pAssocInd->he_caps_present;

	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
		if (wlan_is_open_wep_cipher(mac_ctx->pdev, sessionId)) {
			/* NO keys... these key parameters don't matter. */
			csr_issue_set_context_req_helper(mac_ctx, sessionId,
					&roam_info->peerMac.bytes, false, true,
					eSIR_TX_RX, 0, 0, NULL);
			roam_info->fAuthRequired = false;
		} else {
			roam_info->fAuthRequired = true;
		}
		sme_debug("Receive AUTH_TYPE of %d", csr_akm_type);
		if (csr_akm_type == eCSR_AUTH_TYPE_OWE) {
			roam_info->owe_pending_assoc_ind = qdf_mem_malloc(
							    sizeof(*pAssocInd));
			if (roam_info->owe_pending_assoc_ind)
				qdf_mem_copy(roam_info->owe_pending_assoc_ind,
					     pAssocInd, sizeof(*pAssocInd));
		} else if (csr_akm_type == eCSR_AUTH_TYPE_FT_RSN_PSK) {
			roam_info->ft_pending_assoc_ind = qdf_mem_malloc(
			    sizeof(*pAssocInd));
			if (roam_info->ft_pending_assoc_ind)
				qdf_mem_copy(roam_info->ft_pending_assoc_ind,
					     pAssocInd, sizeof(*pAssocInd));
		}
		status = csr_roam_call_callback(mac_ctx, sessionId,
					roam_info, eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* Refused due to Mac filtering */
			if (roam_info->owe_pending_assoc_ind) {
				qdf_mem_free(roam_info->owe_pending_assoc_ind);
				roam_info->owe_pending_assoc_ind = NULL;
			} else if (roam_info->ft_pending_assoc_ind) {
				qdf_mem_free(roam_info->ft_pending_assoc_ind);
				roam_info->ft_pending_assoc_ind = NULL;
			}
			roam_info->status_code = eSIR_SME_ASSOC_REFUSED;
		} else if (pAssocInd->rsnIE.length && WLAN_ELEMID_RSN ==
			   pAssocInd->rsnIE.rsnIEdata[0]) {
			tDot11fIERSN rsn_ie = {0};

			if (dot11f_unpack_ie_rsn(mac_ctx,
						 pAssocInd->rsnIE.rsnIEdata + 2,
						 pAssocInd->rsnIE.length - 2,
						 &rsn_ie, false)
			    != DOT11F_PARSE_SUCCESS ||
			    (csr_is_sae_akm_present(&rsn_ie) &&
			     !csr_is_sae_peer_allowed(mac_ctx, pAssocInd,
						      session,
						      &rsn_ie,
						      &mac_status_code))) {
				status = QDF_STATUS_E_INVAL;
				roam_info->status_code =
						eSIR_SME_ASSOC_REFUSED;
				sme_debug("SAE peer not allowed: Status: %d",
					  mac_status_code);
			}
		}
	}
	sme_debug("csr_akm_type: %d", csr_akm_type);

	if (csr_akm_type != eCSR_AUTH_TYPE_OWE &&
	    csr_akm_type != eCSR_AUTH_TYPE_FT_RSN_PSK) {
		if ((opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) &&
		    roam_info->status_code != eSIR_SME_ASSOC_REFUSED)
			pAssocInd->need_assoc_rsp_tx_cb = true;
		/* Send Association completion message to PE */
		status = csr_send_assoc_cnf_msg(mac_ctx, pAssocInd, status,
						mac_status_code);
	}

	qdf_mem_free(roam_info);
}

/* csr_if_peer_present() - Check whether peer is present or not
 * @mac_ctx: Pointer to mac context
 * @bssid: Pointer to bssid address
 * @peer_macaddr: Pointer to peer mac address
 *
 * Consider a case
 * 1. SAP received south bound disconnect command
 * 2. At same time, SAP CSA to DFS channel happened and thus peers are deleted.
 * 3. Later same peer got re-added and south bound disconnect command becomes
 *    active for same peer.
 *
 * When SAP receives south bound disconnect command req, driver will post to
 * schedular thread and it will wait in SME message queue. When SAP CSA to DFS
 * channel happens, driver will post to schedular thread and it will wait in PE
 * message queue. Since PE has higher priority than SME message queue, so it
 * will process first. As part of CSA, it will delete all peer including sta
 * hash entry.
 * After CSA, south bound disconnect command got queue to serialization and
 * same peer got re-added again. When south bound disconnect command becomes
 * active, the states will not be proper because for old peer, disassocTrigger
 * is eLIM_PEER_ENTITY_DISASSOC/eLIM_PEER_ENTITY_DEAUTH and when new peer gets
 * re-added, disassocTrigger will be eLIM_HOST_DISASSOC/eLIM_HOST_DEAUTH and
 * thus response to CSR will not be proper. Due to this south bound disconnect
 * command will not remove from active queue which leads to active command
 * timeout.
 * Validate the peer before sending to serialization to avoid queuing command
 * if peer is already deleted.
 *
 * Return: True if peer is present otherwise return false
 */
static bool csr_if_peer_present(struct mac_context *mac_ctx,
				uint8_t *bssid,
				uint8_t *peer_macaddr)
{
	struct wlan_objmgr_peer *peer;
	uint8_t pdev_id;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(mac_ctx->pdev);

	peer = wlan_objmgr_get_peer_by_mac_n_vdev(mac_ctx->psoc, pdev_id,
						  bssid, peer_macaddr,
						  WLAN_LEGACY_SME_ID);

	if (!peer) {
		sme_info("peer not found for mac: " QDF_MAC_ADDR_FMT "and bssid: "
			  QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(peer_macaddr),
			  QDF_MAC_ADDR_REF(bssid));
		return false;
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_SME_ID);
	return true;
}

static void
csr_roam_chk_lnk_disassoc_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	struct disassoc_ind *pDisassocInd;

	/*
	 * Check if AP dis-associated us because of MIC failure. If so,
	 * then we need to take action immediately and not wait till the
	 * the WmStatusChange requests is pushed and processed
	 */
	pDisassocInd = (struct disassoc_ind *)msg_ptr;
	sessionId = pDisassocInd->vdev_id;
	sme_debug("Disassoc Indication from MAC for vdev_id %d bssid " QDF_MAC_ADDR_FMT,
		  pDisassocInd->vdev_id,
		  QDF_MAC_ADDR_REF(pDisassocInd->bssid.bytes));

	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		sme_err("vdev:%d Invalid session. BSSID: " QDF_MAC_ADDR_FMT,
			sessionId, QDF_MAC_ADDR_REF(pDisassocInd->bssid.bytes));

		return;
	}

	if (!csr_if_peer_present(mac_ctx, &pDisassocInd->bssid.bytes[0],
				 &pDisassocInd->peer_macaddr.bytes[0]))
		return;

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDisassocInd->peer_macaddr))
		return;

	sme_nofl_info("disassoc from peer " QDF_MAC_ADDR_FMT
		      "reason: %d status: %d vid %d",
		      QDF_MAC_ADDR_REF(pDisassocInd->peer_macaddr.bytes),
		      pDisassocInd->reasonCode,
		      pDisassocInd->status_code, sessionId);
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session: %d not found", sessionId);
		return;
	}
	/* Update the disconnect stats */
	session->disconnect_stats.disconnection_cnt++;
	session->disconnect_stats.disassoc_by_peer++;

	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDisassociated, msg_ptr);
}

static void
csr_roam_chk_lnk_deauth_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	struct deauth_ind *pDeauthInd;

	pDeauthInd = (struct deauth_ind *)msg_ptr;
	sme_debug("DEAUTH Indication from MAC for vdev_id %d bssid "QDF_MAC_ADDR_FMT,
		  pDeauthInd->vdev_id,
		  QDF_MAC_ADDR_REF(pDeauthInd->bssid.bytes));

	sessionId = pDeauthInd->vdev_id;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		sme_err("vdev %d: Invalid session BSSID: " QDF_MAC_ADDR_FMT,
			pDeauthInd->vdev_id,
			QDF_MAC_ADDR_REF(pDeauthInd->bssid.bytes));
		return;
	}

	if (!csr_if_peer_present(mac_ctx, &pDeauthInd->bssid.bytes[0],
				 &pDeauthInd->peer_macaddr.bytes[0]))
		return;

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDeauthInd->peer_macaddr))
		return;
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	/* Update the disconnect stats */
	switch (pDeauthInd->reasonCode) {
	case REASON_DISASSOC_DUE_TO_INACTIVITY:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.peer_kickout++;
		break;
	case REASON_UNSPEC_FAILURE:
	case REASON_PREV_AUTH_NOT_VALID:
	case REASON_DEAUTH_NETWORK_LEAVING:
	case REASON_CLASS2_FRAME_FROM_NON_AUTH_STA:
	case REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA:
	case REASON_STA_NOT_AUTHENTICATED:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.deauth_by_peer++;
		break;
	case REASON_BEACON_MISSED:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.bmiss++;
		break;
	default:
		/* Unknown reason code */
		break;
	}

	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDeauthenticated,
					msg_ptr);
}

static void
csr_roam_chk_lnk_swt_ch_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct switch_channel_ind *pSwitchChnInd;
	struct csr_roam_info *roam_info;

	/* in case of STA, the SWITCH_CHANNEL originates from its AP */
	sme_debug("eWNI_SME_SWITCH_CHL_IND from SME");
	pSwitchChnInd = (struct switch_channel_ind *)msg_ptr;
	/* Update with the new channel id. The channel id is hidden in the
	 * status_code.
	 */
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pSwitchChnInd->bssid, &sessionId);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (QDF_IS_STATUS_ERROR(pSwitchChnInd->status)) {
		sme_err("Channel switch failed");
		return;
	}
	/* Update the occupied channel list with the new switched channel */
	wlan_cm_init_occupied_ch_freq_list(mac_ctx->pdev, mac_ctx->psoc,
					   sessionId);
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roam_info->chan_info.mhz = pSwitchChnInd->freq;
	roam_info->chan_info.ch_width = pSwitchChnInd->chan_params.ch_width;
	roam_info->chan_info.sec_ch_offset =
				pSwitchChnInd->chan_params.sec_ch_offset;
	roam_info->chan_info.band_center_freq1 =
				pSwitchChnInd->chan_params.mhz_freq_seg0;
	roam_info->chan_info.band_center_freq2 =
				pSwitchChnInd->chan_params.mhz_freq_seg1;

	if (IS_WLAN_PHYMODE_HT(pSwitchChnInd->ch_phymode))
		roam_info->mode = SIR_SME_PHY_MODE_HT;
	else if (IS_WLAN_PHYMODE_VHT(pSwitchChnInd->ch_phymode) ||
		 IS_WLAN_PHYMODE_HE(pSwitchChnInd->ch_phymode))
		roam_info->mode = SIR_SME_PHY_MODE_VHT;
#ifdef WLAN_FEATURE_11BE
	else if (IS_WLAN_PHYMODE_EHT(pSwitchChnInd->ch_phymode))
		roam_info->mode = SIR_SME_PHY_MODE_VHT;
#endif
	else
		roam_info->mode = SIR_SME_PHY_MODE_LEGACY;

	status = csr_roam_call_callback(mac_ctx, sessionId, roam_info,
					eCSR_ROAM_STA_CHANNEL_SWITCH,
					eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_deauth_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct deauth_rsp *pDeauthRsp = (struct deauth_rsp *) msg_ptr;
	enum QDF_OPMODE opmode;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DEAUTH_RSP from SME");
	sessionId = pDeauthRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	opmode = wlan_get_opmode_vdev_id(mac_ctx->pdev, sessionId);
	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->status_code = pDeauthRsp->status_code;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info, eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_disassoc_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	enum QDF_OPMODE opmode;
	/*
	 * session id is invalid here so cant use it to access the array
	 * curSubstate as index
	 */
	struct disassoc_rsp *pDisassocRsp = (struct disassoc_rsp *) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DISASSOC_RSP from SME ");
	sessionId = pDisassocRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	opmode = wlan_get_opmode_vdev_id(mac_ctx->pdev, sessionId);
	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->status_code = pDisassocRsp->status_code;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info,
						eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_mic_fail(struct mac_context *mac_ctx, uint32_t sessionId)
{
	WLAN_HOST_DIAG_EVENT_DEF(secEvent,
				 host_event_wlan_security_payload_type);
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, sessionId);

	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	qdf_mem_zero(&secEvent, sizeof(host_event_wlan_security_payload_type));
	secEvent.eventId = WLAN_SECURITY_EVENT_MIC_ERROR;
	cm_diag_get_auth_enc_type_vdev_id(mac_ctx->psoc,
					  &secEvent.authMode,
					  &secEvent.encryptionModeUnicast,
					  &secEvent.encryptionModeMulticast,
					  sessionId);
	wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, sessionId,
				    (struct qdf_mac_addr *)&secEvent.bssid);
	WLAN_HOST_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_mic_fail_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct mic_failure_ind *mic_ind = (struct mic_failure_ind *)msg_ptr;
	eCsrRoamResult result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				&mic_ind->bssId, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pMICFailureInfo = &mic_ind->info;
		if (mic_ind->info.multicast)
			result = eCSR_ROAM_RESULT_MIC_ERROR_GROUP;
		else
			result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info,
				       eCSR_ROAM_MIC_ERROR_IND, result);
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_roam_diag_mic_fail(mac_ctx, sessionId);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_pbs_probe_req_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tpSirSmeProbeReqInd pProbeReqInd = (tpSirSmeProbeReqInd) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("WPS PBC Probe request Indication from SME");

	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pProbeReqInd->bssid, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pWPSPBCProbeReq = &pProbeReqInd->WPSPBCProbeReq;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info,
				       eCSR_ROAM_WPS_PBC_PROBE_REQ_IND,
				       eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_max_assoc_exceeded(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	tSmeMaxAssocInd *pSmeMaxAssocInd;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	pSmeMaxAssocInd = (tSmeMaxAssocInd *) msg_ptr;
	sme_debug(
		"max assoc have been reached, new peer cannot be accepted");
	sessionId = pSmeMaxAssocInd->sessionId;
	qdf_copy_macaddr(&roam_info->peerMac, &pSmeMaxAssocInd->peer_mac);
	csr_roam_call_callback(mac_ctx, sessionId, roam_info,
			       eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED);
	qdf_mem_free(roam_info);
}

void csr_roam_check_for_link_status_change(struct mac_context *mac,
						tSirSmeRsp *pSirMsg)
{
	if (!pSirMsg) {
		sme_err("pSirMsg is NULL");
		return;
	}
	switch (pSirMsg->messageType) {
	case eWNI_SME_ASSOC_IND:
		csr_roam_chk_lnk_assoc_ind(mac, pSirMsg);
		break;
	case eWNI_SME_ASSOC_IND_UPPER_LAYER:
		csr_roam_chk_lnk_assoc_ind_upper_layer(mac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_IND:
		csr_roam_chk_lnk_disassoc_ind(mac, pSirMsg);
		break;
	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(mac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_IND:
		csr_roam_chk_lnk_deauth_ind(mac, pSirMsg);
		break;
	case eWNI_SME_SWITCH_CHL_IND:
		csr_roam_chk_lnk_swt_ch_ind(mac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_RSP:
		csr_roam_chk_lnk_deauth_rsp(mac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_RSP:
		csr_roam_chk_lnk_disassoc_rsp(mac, pSirMsg);
		break;
	case eWNI_SME_MIC_FAILURE_IND:
		csr_roam_chk_lnk_mic_fail_ind(mac, pSirMsg);
		break;
	case eWNI_SME_WPS_PBC_PROBE_REQ_IND:
		csr_roam_chk_lnk_pbs_probe_req_ind(mac, pSirMsg);
		break;
	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_chk_lnk_set_ctx_rsp(mac, pSirMsg);
		break;
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_RSP:
		sme_debug("TSM Stats rsp from PE");
		csr_tsm_stats_rsp_processor(mac, pSirMsg);
		break;
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_GET_SNR_REQ:
		sme_debug("GetSnrReq from self");
		csr_update_snr(mac, pSirMsg);
		break;
	case eWNI_SME_MAX_ASSOC_EXCEEDED:
		csr_roam_chk_lnk_max_assoc_exceeded(mac, pSirMsg);
		break;
	default:
		break;
	} /* end switch on message type */
}

void csr_roam_wm_status_change_complete(struct mac_context *mac,
					uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if (eSmeCommandWmStatusChange == pCommand->command) {
			/* Nothing to process in a Lost Link completion....  It just kicks off a */
			/* roaming sequence. */
			if (csr_nonscan_active_ll_remove_entry(mac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(mac, pCommand);
			} else {
				sme_err("Failed to release command");
			}
		} else {
			sme_warn("CSR: LOST LINK command is not ACTIVE ...");
		}
	} else {
		sme_warn("CSR: NO commands are ACTIVE ...");
	}
}

void csr_roam_process_wm_status_change_command(
		struct mac_context *mac, tSmeCmd *pCommand)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
						pCommand->vdev_id);
	struct qdf_mac_addr peer_mac;

	if (!pSession) {
		sme_err("session %d not found", pCommand->vdev_id);
		csr_roam_wm_status_change_complete(mac, pCommand->vdev_id);
	}
	sme_debug("session:%d, CmdType : %d",
		pCommand->vdev_id, pCommand->u.wmStatusChangeCmd.Type);

	switch (pCommand->u.wmStatusChangeCmd.Type) {
	case eCsrDisassociated:
		qdf_mem_copy(&peer_mac, &pCommand->u.wmStatusChangeCmd.
					u.DisassocIndMsg.peer_macaddr,
			     QDF_MAC_ADDR_SIZE);
		/*
		 * Get peer stats before peer gets deleted so that these stats
		 * can be given to user space when big data stats are queried.
		 * Once peer stats are retrieved disassoc sta will continue
		 */
		csr_get_peer_stats(mac, pCommand->vdev_id, peer_mac);
		break;
	case eCsrDeauthenticated:
		qdf_mem_copy(&peer_mac, &pCommand->u.wmStatusChangeCmd.
					u.DeauthIndMsg.peer_macaddr,
			     QDF_MAC_ADDR_SIZE);
		/*
		 * Get peer stats before peer gets deleted so that these stats
		 * can be given to user space when big data stats are queried.
		 * Once peer stats are retrieved deauth sta will continue
		 */
		csr_get_peer_stats(mac, pCommand->vdev_id, peer_mac);
		break;
	default:
		sme_warn("gets an unknown command %d",
			pCommand->u.wmStatusChangeCmd.Type);
		csr_roam_wm_status_change_complete(mac, pCommand->vdev_id);
		break;
	}
}

/**
 * csr_compute_mode_and_band() - computes dot11mode
 * @mac: mac global context
 * @dot11_mode: out param, do11 mode calculated
 * @band: out param, band caclculated
 * @opr_ch_freq: operating channel freq in MHz
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes.
 *
 * Return: void
 */
static void
csr_compute_mode_and_band(struct mac_context *mac_ctx,
			  enum csr_cfgdot11mode *dot11_mode,
			  enum reg_wifi_band *band,
			  uint32_t opr_ch_freq)
{
	bool vht_24_ghz = mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band;

	switch (mac_ctx->roam.configParam.uCfgDot11Mode) {
	case eCSR_CFG_DOT11_MODE_11A:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
		*band = REG_BAND_5G;
		break;
	case eCSR_CFG_DOT11_MODE_11B:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
		*band = REG_BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11G:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		*band = REG_BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11N:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AC:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AC_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC_ONLY;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AX:
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = mac_ctx->roam.configParam.uCfgDot11Mode;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
#ifdef WLAN_FEATURE_11BE
	case eCSR_CFG_DOT11_MODE_11BE:
	case eCSR_CFG_DOT11_MODE_11BE_ONLY:
		if (IS_FEATURE_11BE_SUPPORTED_BY_FW) {
			*dot11_mode = mac_ctx->roam.configParam.uCfgDot11Mode;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11AX;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
#endif
	case eCSR_CFG_DOT11_MODE_AUTO:
#ifdef WLAN_FEATURE_11BE
		if (IS_FEATURE_11BE_SUPPORTED_BY_FW) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11BE;
		} else
#endif
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11AX;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band,
			 * check for INI item to disable VHT operation
			 * in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	default:
		/*
		 * Global dot11 Mode setting is 11a/b/g. use the channel number
		 * to determine the Mode setting.
		 */
		if (eCSR_OPERATING_CHANNEL_AUTO == opr_ch_freq) {
			*band = (mac_ctx->mlme_cfg->gen.band == BAND_2G ?
				REG_BAND_2G : REG_BAND_5G);
			if (REG_BAND_2G == *band) {
				*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			} else {
				/* prefer 5GHz */
				*band = REG_BAND_5G;
				*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			}
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			*band = REG_BAND_2G;
		} else {
			/* else, it's a 5.0GHz channel.  Set mode to 11a. */
			*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			*band = REG_BAND_5G;
		}
		break;
	} /* switch */
}

/**
 * csr_roam_get_phy_mode_band_for_bss() - This function returns band and mode
 * information.
 * @mac_ctx:  mac global context
 * dot11_cfg: pointer to dot11 config
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes. The only tricky part is that if phyMode is set to 11abg,
 * this function may return eCSR_CFG_DOT11_MODE_11B instead of
 * eCSR_CFG_DOT11_MODE_11G if everything is set to auto-pick.
 *
 * Return: dot11mode
 */
enum csr_cfgdot11mode
csr_roam_get_phy_mode_band_for_bss(struct mac_context *mac_ctx,
				   struct bss_dot11_config *dot11_cfg)
{
	enum reg_wifi_band band = REG_BAND_2G;
	qdf_freq_t opr_freq = 0;
	bool is_11n_allowed;
	enum csr_cfgdot11mode curr_mode =
		mac_ctx->roam.configParam.uCfgDot11Mode;
	enum csr_cfgdot11mode cfg_dot11_mode;
	enum QDF_OPMODE opmode;
	bool is_ap = false;
	uint8_t privacy;
	uint8_t vdev_id = dot11_cfg->vdev_id;

	if (dot11_cfg->bss_op_ch_freq)
		opr_freq = dot11_cfg->bss_op_ch_freq;

	opmode = wlan_get_opmode_vdev_id(mac_ctx->pdev, vdev_id);
	is_ap = (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE);
	cfg_dot11_mode =
		csr_get_cfg_dot11_mode_from_csr_phy_mode(is_ap,
							 dot11_cfg->phy_mode);
	privacy = dot11_cfg->privacy;

	/*
	 * If the global setting for dot11Mode is set to auto/abg, we overwrite
	 * the setting in the profile.
	 */
	if ((!is_ap && ((eCSR_CFG_DOT11_MODE_AUTO == curr_mode) ||
	     (eCSR_CFG_DOT11_MODE_ABG == curr_mode))) ||
	     (eCSR_CFG_DOT11_MODE_AUTO == cfg_dot11_mode) ||
	     (eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode)) {
		csr_compute_mode_and_band(mac_ctx, &cfg_dot11_mode,
					  &band, opr_freq);
	} /* if( eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode ) */
	else {
		/* dot11 mode is set, lets pick the band */
		if (0 == opr_freq) {
			/* channel is Auto also. */
			if (mac_ctx->mlme_cfg->gen.band == BAND_ALL) {
				/* prefer 5GHz */
				band = REG_BAND_5G;
			}
		} else{
			band = wlan_reg_freq_to_band(opr_freq);
		}
	}

	dot11_cfg->p_band = band;
	if (opr_freq == 2484 && wlan_reg_is_24ghz_ch_freq(opr_freq)) {
		sme_err("Switching to Dot11B mode");
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11B;
	}

	if (wlan_reg_is_24ghz_ch_freq(opr_freq) &&
	    !mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
	    (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode ||
	    eCSR_CFG_DOT11_MODE_11AC_ONLY == cfg_dot11_mode))
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11N;
	/*
	 * Incase of WEP Security encryption type is coming as part of add key.
	 * So while STart BSS dont have information
	 */
	is_11n_allowed = wlan_vdev_id_is_11n_allowed(mac_ctx->pdev, vdev_id);
	if ((!is_11n_allowed || (privacy &&
	    wlan_vdev_id_is_open_cipher(mac_ctx->pdev, vdev_id))) &&
	    ((eCSR_CFG_DOT11_MODE_11N == cfg_dot11_mode) ||
	    (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode) ||
	    (eCSR_CFG_DOT11_MODE_11AX == cfg_dot11_mode) ||
	    CSR_IS_CFG_DOT11_PHY_MODE_11BE(cfg_dot11_mode))) {
		/* We cannot do 11n here */
		if (wlan_reg_is_24ghz_ch_freq(opr_freq))
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		else
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11A;
	}
	sme_debug("dot11mode: %d phyMode %d is_11n_allowed %d privacy %d chan freq %d fw sup AX %d",
		  cfg_dot11_mode, dot11_cfg->phy_mode, is_11n_allowed,
		  privacy, opr_freq,
		  IS_FEATURE_SUPPORTED_BY_FW(DOT11AX));

#ifdef WLAN_FEATURE_11BE
	sme_debug("BE :%d", IS_FEATURE_SUPPORTED_BY_FW(DOT11BE));
#endif
	return cfg_dot11_mode;
}

QDF_STATUS csr_get_cfg_valid_channels(struct mac_context *mac,
				      qdf_freq_t *ch_freq_list,
				      uint32_t *num_ch_freq)
{
	uint8_t num_chan_temp = 0;
	int i;
	uint32_t *valid_ch_freq_list =
				mac->mlme_cfg->reg.valid_channel_freq_list;

	*num_ch_freq = mac->mlme_cfg->reg.valid_channel_list_num;

	for (i = 0; i < *num_ch_freq; i++) {
		if (!wlan_reg_is_dsrc_freq(valid_ch_freq_list[i])) {
			ch_freq_list[num_chan_temp] = valid_ch_freq_list[i];
			num_chan_temp++;
		}
	}

	*num_ch_freq = num_chan_temp;
	return QDF_STATUS_SUCCESS;
}

/**
 * csr_convert_mode_to_nw_type() - convert mode into network type
 * @dot11_mode:    dot11_mode
 * @band:          2.4 or 5 GHz
 *
 * Return: tSirNwType
 */
tSirNwType
csr_convert_mode_to_nw_type(enum csr_cfgdot11mode dot11_mode,
			    enum reg_wifi_band band)
{
	switch (dot11_mode) {
	case eCSR_CFG_DOT11_MODE_11G:
		return eSIR_11G_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11B:
		return eSIR_11B_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11A:
		return eSIR_11A_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11N:
	default:
		/*
		 * Because LIM only verifies it against 11a, 11b or 11g, set
		 * only 11g or 11a here
		 */
		if (REG_BAND_2G == band)
			return eSIR_11G_NW_TYPE;
		else
			return eSIR_11A_NW_TYPE;
	}
	return eSIR_DONOT_USE_NW_TYPE;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef WLAN_FEATURE_11BE_MLO
static bool csr_pmk_match_mlo_address(struct wlan_objmgr_vdev *vdev,
				      struct wlan_crypto_pmksa *pmksa)
{
	struct qdf_mac_addr bss_peer_mld_mac = {0};

	wlan_vdev_get_bss_peer_mld_mac(vdev, &bss_peer_mld_mac);

	return qdf_is_macaddr_equal(&bss_peer_mld_mac, &pmksa->bssid);
}
#else
static inline bool csr_pmk_match_mlo_address(struct wlan_objmgr_vdev *vdev,
					     struct wlan_crypto_pmksa *pmksa)
{
	return false;
}
#endif

void csr_get_pmk_info(struct mac_context *mac_ctx, uint8_t session_id,
		      struct wlan_crypto_pmksa *pmk_cache)
{
	if (!mac_ctx) {
		sme_err("Mac_ctx is NULL");
		return;
	}
	wlan_cm_get_psk_pmk(mac_ctx->pdev, session_id, pmk_cache->pmk,
			    &pmk_cache->pmk_len);
}

QDF_STATUS csr_roam_set_psk_pmk(struct mac_context *mac,
				struct wlan_crypto_pmksa *pmksa,
				uint8_t vdev_id, bool update_to_fw)
{
	struct wlan_objmgr_vdev *vdev;
	struct qdf_mac_addr connected_bssid = {0};
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wlan_mlme_get_bssid_vdev_id(mac->pdev, vdev_id, &connected_bssid);

	/*
	 * If the set_pmksa is received from the userspace in
	 * connected state and if the connected BSSID is not
	 * same as the PMKSA entry bssid, then reject this
	 * global cache updation.
	 *
	 * Also for FILS connection, the set_pmksa will not have
	 * the BSSID. So avoid this check for FILS connection.
	 */
	if (wlan_vdev_mlme_get_state(vdev) == WLAN_VDEV_S_UP &&
	    !pmksa->ssid_len &&
	    !qdf_is_macaddr_equal(&connected_bssid, &pmksa->bssid) &&
	    !csr_pmk_match_mlo_address(vdev, pmksa)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		sme_debug("Set pmksa received for non-connected bss");
		return QDF_STATUS_E_INVAL;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	wlan_cm_set_psk_pmk(mac->pdev, vdev_id, pmksa->pmk, pmksa->pmk_len);
	if (update_to_fw) {
		status = wlan_roam_update_cfg(mac->psoc, vdev_id,
					      REASON_ROAM_PSK_PMK_CHANGED);
		if (status == QDF_STATUS_E_INVAL)
			wlan_mlme_defer_pmk_set_in_roaming(mac->psoc, vdev_id,
							   true);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_set_pmk_cache_ft(struct mac_context *mac, uint8_t vdev_id,
				struct wlan_crypto_pmksa *pmk_cache)
{
	struct wlan_objmgr_vdev *vdev;
	int32_t akm;

	if (!CSR_IS_SESSION_VALID(mac, vdev_id)) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_CCKM)) {
		sme_debug("PMK update is not required for ESE");
		return QDF_STATUS_SUCCESS;
	}

	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X) ||
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256) ||
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FILS_SHA384) ||
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X_SHA384)) {
		sme_debug("Auth type: %x update the MDID in cache", akm);
		cm_update_pmk_cache_ft(mac->psoc, vdev_id, pmk_cache);
	} else {
		struct cm_roam_values_copy src_cfg = {};
		struct scan_filter *scan_filter;
		qdf_list_t *list = NULL;
		struct scan_cache_node *first_node = NULL;
		struct rsn_mdie *mdie = NULL;
		qdf_list_node_t *cur_node = NULL;

		scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
		if (!scan_filter)
			return QDF_STATUS_E_NOMEM;
		scan_filter->num_of_bssid = 1;
		qdf_mem_copy(scan_filter->bssid_list[0].bytes,
			     &pmk_cache->bssid, sizeof(struct qdf_mac_addr));
		list = wlan_scan_get_result(mac->pdev, scan_filter);
		qdf_mem_free(scan_filter);
		if (!list || (list && !qdf_list_size(list))) {
			sme_debug("Scan list is empty");
			goto err;
		}
		qdf_list_peek_front(list, &cur_node);
		first_node = qdf_container_of(cur_node,
					      struct scan_cache_node,
					      node);
		if (first_node && first_node->entry)
			mdie = (struct rsn_mdie *)
					util_scan_entry_mdie(first_node->entry);
		if (mdie) {
			sme_debug("Update MDID in cache from scan_res");
			src_cfg.bool_value = true;
			src_cfg.uint_value =
				(mdie->mobility_domain[0] |
				 (mdie->mobility_domain[1] << 8));
			wlan_cm_roam_cfg_set_value(mac->psoc, vdev_id,
						   MOBILITY_DOMAIN, &src_cfg);
			cm_update_pmk_cache_ft(mac->psoc, vdev_id, pmk_cache);
		}
err:
		if (list)
			wlan_scan_purge_results(list);
	}
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
void csr_clear_sae_single_pmk(struct wlan_objmgr_psoc *psoc,
			      uint8_t vdev_id,
			      struct wlan_crypto_pmksa *pmk_cache)
{
	struct wlan_objmgr_vdev *vdev;
	int32_t keymgmt;
	struct mlme_pmk_info pmk_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0) {
		sme_err("Invalid mgmt cipher");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	if (!(keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE))) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	if (pmk_cache) {
		qdf_mem_copy(&pmk_info.pmk, pmk_cache->pmk, pmk_cache->pmk_len);
		pmk_info.pmk_len = pmk_cache->pmk_len;
		wlan_mlme_clear_sae_single_pmk_info(vdev, &pmk_info);
	} else {
		wlan_mlme_clear_sae_single_pmk_info(vdev, NULL);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#endif

#ifdef FEATURE_WLAN_ESE
void csr_update_prev_ap_info(struct csr_roam_session *session,
			     struct wlan_objmgr_vdev *vdev)
{
	struct wlan_ssid ssid;
	QDF_STATUS status;
	enum QDF_OPMODE opmode;
	struct rso_config *rso_cfg;

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		return;

	if (!rso_cfg->is_ese_assoc || opmode != QDF_STA_MODE)
		return;
	status = wlan_vdev_mlme_get_ssid(vdev, ssid.ssid, &ssid.length);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err(" failed to find SSID for vdev %d", session->vdev_id);
		return;
	}
	session->isPrevApInfoValid = true;
	session->roamTS1 = qdf_mc_timer_get_system_time();
}
#endif

#ifdef WLAN_FEATURE_FILS_SK
static QDF_STATUS csr_cm_update_fils_info(struct wlan_objmgr_vdev *vdev,
					  struct bss_description *bss_desc,
					  struct wlan_cm_vdev_connect_req *req)
{
	uint8_t cache_id[CACHE_ID_LEN] = {0};
	struct scan_cache_entry *entry;
	struct wlan_crypto_pmksa *fils_ssid_pmksa, *bssid_lookup_pmksa;

	if (!req->fils_info || !req->fils_info->is_fils_connection) {
		wlan_cm_update_mlme_fils_info(vdev, NULL);
		return QDF_STATUS_SUCCESS;
	}

	if (bss_desc->fils_info_element.is_cache_id_present) {
		qdf_mem_copy(cache_id, bss_desc->fils_info_element.cache_id,
			     CACHE_ID_LEN);
		sme_debug("FILS_PMKSA: cache_id[0]:%d, cache_id[1]:%d",
			  cache_id[0], cache_id[1]);
	}
	entry = req->bss->entry;
	bssid_lookup_pmksa = wlan_crypto_get_pmksa(vdev, &entry->bssid);
	fils_ssid_pmksa =
			wlan_crypto_get_fils_pmksa(vdev, cache_id,
						   entry->ssid.ssid,
						   entry->ssid.length);

	if ((!req->fils_info->rrk_len ||
	     !req->fils_info->username_len) &&
	     !bss_desc->fils_info_element.is_cache_id_present &&
	     !bssid_lookup_pmksa && !fils_ssid_pmksa)
		return QDF_STATUS_E_FAILURE;

	return wlan_cm_update_mlme_fils_info(vdev, req->fils_info);
}
#else
static inline
QDF_STATUS csr_cm_update_fils_info(struct wlan_objmgr_vdev *vdev,
				   struct bss_description *bss_desc,
				   struct wlan_cm_vdev_connect_req *req)
{
}
#endif

#if defined(WLAN_FEATURE_HOST_ROAM) && defined(FEATURE_WLAN_ESE)
static void csr_update_tspec_info(struct mac_context *mac_ctx,
				  struct wlan_objmgr_vdev *vdev,
				  tDot11fBeaconIEs *ie_struct)
{
	struct mlme_legacy_priv *mlme_priv;
	tESETspecInfo *ese_tspec;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;
	if (!cm_is_ese_connection(vdev, ie_struct->ESEVersion.present))
		return;

	ese_tspec = &mlme_priv->connect_info.ese_tspec_info;
	qdf_mem_zero(ese_tspec, sizeof(tESETspecInfo));
	ese_tspec->numTspecs = sme_qos_ese_retrieve_tspec_info(mac_ctx,
					wlan_vdev_get_id(vdev),
					ese_tspec->tspec);
}
#else
static inline void csr_update_tspec_info(struct mac_context *mac_ctx,
					 struct wlan_objmgr_vdev *vdev,
					 tDot11fBeaconIEs *ie_struct) {}
#endif

void cm_csr_send_set_ie(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		sme_err("Failed to get vdev mlme obj!");
		QDF_BUG(0);
		return;
	}

	csr_send_set_ie(vdev_mlme->mgmt.generic.type,
			vdev_mlme->mgmt.generic.subtype,
			wlan_vdev_get_id(vdev));
}

QDF_STATUS cm_csr_handle_join_req(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_vdev_connect_req *req,
				  struct cm_vdev_join_req *join_req,
				  bool reassoc)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	QDF_STATUS status;
	tDot11fBeaconIEs *ie_struct;
	struct bss_description *bss_desc;
	uint32_t ie_len, bss_len;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	ie_len = util_scan_entry_ie_len(join_req->entry);
	bss_len = (uint16_t)(offsetof(struct bss_description,
			   ieFields[0]) + ie_len);

	bss_desc = qdf_mem_malloc(sizeof(*bss_desc) + bss_len);
	if (!bss_desc)
		return QDF_STATUS_E_NOMEM;

	status = wlan_fill_bss_desc_from_scan_entry(mac_ctx, bss_desc,
						    join_req->entry);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(bss_desc);
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_get_parsed_bss_description_ies(mac_ctx, bss_desc,
						     &ie_struct);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("IE parsing failed vdev id %d", vdev_id);
		qdf_mem_free(bss_desc);
		return QDF_STATUS_E_FAILURE;
	}

	if (reassoc) {
		csr_update_tspec_info(mac_ctx, vdev, ie_struct);
	} else {
		status = csr_cm_update_fils_info(vdev, bss_desc, req);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("failed to update fils info vdev id %d",
				vdev_id);
			qdf_mem_free(ie_struct);
			qdf_mem_free(bss_desc);
			return QDF_STATUS_E_FAILURE;
		}
		sme_qos_csr_event_ind(mac_ctx, vdev_id,
				      SME_QOS_CSR_JOIN_REQ, NULL);
	}

	if ((wlan_reg_11d_enabled_on_host(mac_ctx->psoc)) &&
	     !ie_struct->Country.present)
		csr_apply_channel_power_info_wrapper(mac_ctx);

	qdf_mem_free(ie_struct);
	qdf_mem_free(bss_desc);

	cm_csr_set_joining(vdev_id);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
static uint32_t csr_get_tspec_ie_len(struct cm_vdev_join_rsp *rsp)
{
	return rsp->tspec_ie.len;
}
static inline void csr_copy_tspec_ie_len(struct csr_roam_session *session,
					 uint8_t *frame_ptr,
					 struct cm_vdev_join_rsp *rsp)
{
	session->connectedInfo.nTspecIeLength = rsp->tspec_ie.len;
	if (rsp->tspec_ie.len)
		qdf_mem_copy(frame_ptr, rsp->tspec_ie.ptr,
			     rsp->tspec_ie.len);
}

#else
static inline uint32_t csr_get_tspec_ie_len(struct cm_vdev_join_rsp *rsp)
{
	return 0;
}
static inline void csr_copy_tspec_ie_len(struct csr_roam_session *session,
					 uint8_t *frame_ptr,
					 struct cm_vdev_join_rsp *rsp)
{}
#endif

static void csr_fill_connected_info(struct mac_context *mac_ctx,
				    struct csr_roam_session *session,
				    struct cm_vdev_join_rsp *rsp)
{
	uint32_t len;
	struct wlan_connect_rsp_ies *connect_ies;
	uint8_t *frame_ptr;
	uint32_t beacon_data_len = 0;

	connect_ies = &rsp->connect_rsp.connect_ies;
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	if (connect_ies->bcn_probe_rsp.len > sizeof(struct wlan_frame_hdr))
		beacon_data_len = connect_ies->bcn_probe_rsp.len -
						sizeof(struct wlan_frame_hdr);
	len = beacon_data_len + connect_ies->assoc_req.len +
		connect_ies->assoc_rsp.len + rsp->ric_resp_ie.len +
		csr_get_tspec_ie_len(rsp);
	if (!len)
		return;

	session->connectedInfo.pbFrames = qdf_mem_malloc(len);
	if (!session->connectedInfo.pbFrames)
		return;

	frame_ptr = session->connectedInfo.pbFrames;
	session->connectedInfo.nBeaconLength = beacon_data_len;
	if (beacon_data_len)
		qdf_mem_copy(frame_ptr,
			     connect_ies->bcn_probe_rsp.ptr +
			     sizeof(struct wlan_frame_hdr),
			     beacon_data_len);
	frame_ptr += beacon_data_len;

	session->connectedInfo.nAssocReqLength = connect_ies->assoc_req.len;
	if (connect_ies->assoc_req.len)
		qdf_mem_copy(frame_ptr,
			     connect_ies->assoc_req.ptr,
			     connect_ies->assoc_req.len);
	frame_ptr += connect_ies->assoc_req.len;

	session->connectedInfo.nAssocRspLength = connect_ies->assoc_rsp.len;
	if (connect_ies->assoc_rsp.len)
		qdf_mem_copy(frame_ptr,
			     connect_ies->assoc_rsp.ptr,
			     connect_ies->assoc_rsp.len);
	frame_ptr += connect_ies->assoc_rsp.len;

	session->connectedInfo.nRICRspLength = rsp->ric_resp_ie.len;
	if (rsp->ric_resp_ie.len)
		qdf_mem_copy(frame_ptr, rsp->ric_resp_ie.ptr,
			     rsp->ric_resp_ie.len);
	frame_ptr += rsp->ric_resp_ie.len;

	csr_copy_tspec_ie_len(session, frame_ptr, rsp);
}

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
static inline void csr_qos_send_disconnect_ind(struct mac_context *mac_ctx,
					       uint8_t vdev_id)
{
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_DISCONNECT_IND,
			      NULL);
}

static inline void csr_qos_send_assoc_ind(struct mac_context *mac_ctx,
					  uint8_t vdev_id,
					  sme_QosAssocInfo *assoc_info)
{
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_ASSOC_COMPLETE,
			      assoc_info);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void
csr_qso_disconnect_complete_ind(struct mac_context *mac_ctx,
				struct wlan_cm_connect_resp *connect_rsp)
{
	if (IS_ROAM_REASON_DISCONNECTION(
		connect_rsp->roaming_info->roam_reason))
		sme_qos_csr_event_ind(mac_ctx, connect_rsp->vdev_id,
				      SME_QOS_CSR_DISCONNECT_ROAM_COMPLETE,
				      NULL);
}
#else
static inline void
csr_qso_disconnect_complete_ind(struct mac_context *mac_ctx,
				struct wlan_cm_connect_resp *connect_rsp) {}
#endif

static void
csr_qos_send_reassoc_ind(struct mac_context *mac_ctx,
			 uint8_t vdev_id,
			 sme_QosAssocInfo *assoc_info,
			 struct wlan_cm_connect_resp *connect_rsp)
{
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_HANDOFF_ASSOC_REQ,
			      NULL);
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_REASSOC_REQ,
			      NULL);
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_HANDOFF_COMPLETE,
			      NULL);
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_REASSOC_COMPLETE,
			      assoc_info);

	csr_qso_disconnect_complete_ind(mac_ctx, connect_rsp);
}
#else
static inline void csr_qos_send_disconnect_ind(struct mac_context *mac_ctx,
					       uint8_t vdev_id)
{}

static inline void csr_qos_send_assoc_ind(struct mac_context *mac_ctx,
					  uint8_t vdev_id,
					  sme_QosAssocInfo *assoc_info)
{}
static inline void
csr_qos_send_reassoc_ind(struct mac_context *mac_ctx,
			 uint8_t vdev_id,
			 sme_QosAssocInfo *assoc_info,
			 struct wlan_cm_connect_resp *connect_rsp)
{}
#endif

static void
csr_update_beacon_in_connect_rsp(struct scan_cache_entry *entry,
				 struct wlan_connect_rsp_ies *connect_ies)
{
	if (!entry)
		return;

	/* no need to update if already present */
	if (connect_ies->bcn_probe_rsp.ptr)
		return;

	/*
	 * In case connection to MBSSID: Non Tx BSS OR host reassoc,
	 * vdev/peer manager doesn't send unicast probe req so fill the
	 * beacon in connect resp IEs here.
	 */
	connect_ies->bcn_probe_rsp.len =
				util_scan_entry_frame_len(entry);
	connect_ies->bcn_probe_rsp.ptr =
		qdf_mem_malloc(connect_ies->bcn_probe_rsp.len);
	if (!connect_ies->bcn_probe_rsp.ptr)
		return;

	qdf_mem_copy(connect_ies->bcn_probe_rsp.ptr,
		     util_scan_entry_frame_ptr(entry),
		     connect_ies->bcn_probe_rsp.len);
}

static void csr_fill_connected_profile(struct mac_context *mac_ctx,
				       struct csr_roam_session *session,
				       struct wlan_objmgr_vdev *vdev,
				       struct cm_vdev_join_rsp *rsp)
{
	struct scan_filter *filter;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	QDF_STATUS status;
	qdf_list_t *list = NULL;
	qdf_list_node_t *cur_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	uint32_t bss_len, ie_len;
	struct bss_description *bss_desc = NULL;
	tDot11fBeaconIEs *bcn_ies;
	sme_QosAssocInfo assoc_info;
	struct cm_roam_values_copy src_cfg = {};
	bool is_ese = false;
	uint8_t country_code[REG_ALPHA2_LEN + 1];

	session->modifyProfileFields.uapsd_mask = rsp->uapsd_mask;
	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		return;

	filter->num_of_bssid = 1;
	qdf_copy_macaddr(&filter->bssid_list[0], &rsp->connect_rsp.bssid);
	filter->ignore_auth_enc_type = true;

	status = wlan_vdev_mlme_get_ssid(vdev, filter->ssid_list[0].ssid,
					 &filter->ssid_list[0].length);
	if (QDF_IS_STATUS_SUCCESS(status))
		filter->num_of_ssid = 1;

	list = wlan_scan_get_result(mac_ctx->pdev, filter);
	qdf_mem_free(filter);
	if (!list || (list && !qdf_list_size(list)))
		goto purge_list;


	qdf_list_peek_front(list, &cur_lst);
	if (!cur_lst)
		goto purge_list;

	cur_node = qdf_container_of(cur_lst, struct scan_cache_node, node);
	ie_len = util_scan_entry_ie_len(cur_node->entry);
	bss_len = (uint16_t)(offsetof(struct bss_description,
				      ieFields[0]) + ie_len);
	bss_desc = qdf_mem_malloc(bss_len);
	if (!bss_desc)
		goto purge_list;

	wlan_fill_bss_desc_from_scan_entry(mac_ctx, bss_desc, cur_node->entry);
	pe_debug("Dump scan entry frm:");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   cur_node->entry->raw_frame.ptr,
			   cur_node->entry->raw_frame.len);

	src_cfg.uint_value = bss_desc->mbo_oce_enabled_ap;
	wlan_cm_roam_cfg_set_value(mac_ctx->psoc, vdev_id, MBO_OCE_ENABLED_AP,
				   &src_cfg);
	csr_fill_single_pmk(mac_ctx->psoc, vdev_id, bss_desc);
	status = wlan_get_parsed_bss_description_ies(mac_ctx, bss_desc,
						     &bcn_ies);
	if (QDF_IS_STATUS_ERROR(status))
		goto purge_list;

	if (!bss_desc->beaconInterval)
		sme_err("ERROR: Beacon interval is ZERO");

	csr_update_beacon_in_connect_rsp(cur_node->entry,
					 &rsp->connect_rsp.connect_ies);

	assoc_info.bss_desc = bss_desc;
	if (rsp->connect_rsp.is_reassoc) {
		if (cm_is_ese_connection(vdev, bcn_ies->ESEVersion.present))
			is_ese = true;
		wlan_cm_set_ese_assoc(mac_ctx->pdev, vdev_id, is_ese);
		wlan_cm_roam_cfg_get_value(mac_ctx->psoc, vdev_id, UAPSD_MASK,
					   &src_cfg);
		assoc_info.uapsd_mask = src_cfg.uint_value;
		csr_qos_send_reassoc_ind(mac_ctx, vdev_id, &assoc_info,
					 &rsp->connect_rsp);
		if (src_cfg.uint_value)
			sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), vdev_id);
	} else {
		assoc_info.uapsd_mask = rsp->uapsd_mask;
		csr_qos_send_assoc_ind(mac_ctx, vdev_id, &assoc_info);
	}

	if (bcn_ies->Country.present)
		qdf_mem_copy(country_code, bcn_ies->Country.country,
			     REG_ALPHA2_LEN + 1);
	else
		qdf_mem_zero(country_code, REG_ALPHA2_LEN + 1);
	wlan_cm_set_country_code(mac_ctx->pdev, vdev_id, country_code);

	qdf_mem_free(bcn_ies);

purge_list:
	if (bss_desc)
		qdf_mem_free(bss_desc);
	if (list)
		wlan_scan_purge_results(list);

}

QDF_STATUS cm_csr_connect_rsp(struct wlan_objmgr_vdev *vdev,
			      struct cm_vdev_join_rsp *rsp)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct csr_roam_session *session;
	struct cm_roam_values_copy src_config = {};

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	if (QDF_IS_STATUS_ERROR(rsp->connect_rsp.connect_status))
		return QDF_STATUS_SUCCESS;

	/* handle below only in case of success */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session || !CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("session not found for vdev_id %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!rsp->connect_rsp.is_reassoc) {
		if (rsp->uapsd_mask)
			sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), vdev_id);
		src_config.uint_value = rsp->uapsd_mask;
		wlan_cm_roam_cfg_set_value(mac_ctx->psoc, vdev_id, UAPSD_MASK,
					   &src_config);
	}
	session->nss = rsp->nss;
	csr_fill_connected_info(mac_ctx, session, rsp);
	csr_fill_connected_profile(mac_ctx, session, vdev, rsp);

	return QDF_STATUS_SUCCESS;
}

static void
cm_update_rsn_ocv_cap(int32_t *rsn_cap,
		      struct wlan_cm_connect_resp *rsp)
{
	struct wlan_crypto_params crypto_params;
	uint8_t *ie_ptr;
	uint32_t ie_len;
	QDF_STATUS status;

	/* no need to do anything if OCV is not set */
	if (!(*rsn_cap & WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED))
		return;

	if (!rsp->connect_ies.bcn_probe_rsp.ptr ||
	    !rsp->connect_ies.bcn_probe_rsp.len ||
	    (rsp->connect_ies.bcn_probe_rsp.len <
		(sizeof(struct wlan_frame_hdr) +
		offsetof(struct wlan_bcn_frame, ie)))) {
		sme_err("invalid beacon probe rsp len %d",
			rsp->connect_ies.bcn_probe_rsp.len);
		return;
	}

	ie_len = (rsp->connect_ies.bcn_probe_rsp.len -
			sizeof(struct wlan_frame_hdr) -
			offsetof(struct wlan_bcn_frame, ie));
	ie_ptr = (uint8_t *)(rsp->connect_ies.bcn_probe_rsp.ptr +
			     sizeof(struct wlan_frame_hdr) +
			     offsetof(struct wlan_bcn_frame, ie));

	status = wlan_get_crypto_params_from_rsn_ie(&crypto_params, ie_ptr,
						    ie_len);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	if (!(crypto_params.rsn_caps & WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED))
		*rsn_cap &= ~WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED;
}

QDF_STATUS
cm_csr_connect_done_ind(struct wlan_objmgr_vdev *vdev,
			struct wlan_cm_connect_resp *rsp)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	int32_t count;
	struct set_context_rsp install_key_rsp;
	int32_t rsn_cap, set_value;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct dual_sta_policy *dual_sta_policy;
	bool enable_mcc_adaptive_sch = false;
	struct qdf_mac_addr bc_mac = QDF_MAC_ADDR_BCAST_INIT;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	mlme_obj = mlme_get_psoc_ext_obj(mac_ctx->psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	if (QDF_IS_STATUS_ERROR(rsp->connect_status)) {
		cm_csr_set_idle(vdev_id);
		sme_qos_update_hand_off(vdev_id, false);
		sme_qos_csr_event_ind(mac_ctx, vdev_id,
				      SME_QOS_CSR_DISCONNECT_IND, NULL);
		/* Fill legacy structures from resp for failure */

		return QDF_STATUS_SUCCESS;
	}

	dual_sta_policy = &mlme_obj->cfg.gen.dual_sta_policy;
	count = policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
							  PM_STA_MODE, NULL);
	/*
	 * send duty cycle percentage to FW only if STA + STA
	 * concurrency is in MCC.
	 */
	sme_debug("Current iface vdev_id:%d, Primary vdev_id:%d, Dual sta policy:%d, count:%d",
		  vdev_id, dual_sta_policy->primary_vdev_id,
		  dual_sta_policy->concurrent_sta_policy, count);

	if (dual_sta_policy->primary_vdev_id != WLAN_UMAC_VDEV_ID_MAX &&
	    dual_sta_policy->concurrent_sta_policy ==
	    QCA_WLAN_CONCURRENT_STA_POLICY_PREFER_PRIMARY && count == 2 &&
	    policy_mgr_current_concurrency_is_mcc(mac_ctx->psoc)) {
		policy_mgr_get_mcc_adaptive_sch(mac_ctx->psoc,
						&enable_mcc_adaptive_sch);
		if (enable_mcc_adaptive_sch) {
			sme_debug("Disable mcc_adaptive_scheduler");
			policy_mgr_set_dynamic_mcc_adaptive_sch(
							mac_ctx->psoc, false);
			if (QDF_STATUS_SUCCESS != sme_set_mas(false)) {
				sme_err("Failed to disable mcc_adaptive_sched");
				return -EAGAIN;
			}
		}
		set_value =
			wlan_mlme_get_mcc_duty_cycle_percentage(mac_ctx->pdev);
		sme_cli_set_command(vdev_id, WMA_VDEV_MCC_SET_TIME_QUOTA,
				    set_value, VDEV_CMD);
	  } else if (dual_sta_policy->concurrent_sta_policy ==
		     QCA_WLAN_CONCURRENT_STA_POLICY_UNBIASED && count == 2 &&
		     policy_mgr_current_concurrency_is_mcc(mac_ctx->psoc)) {
		policy_mgr_get_mcc_adaptive_sch(mac_ctx->psoc,
						&enable_mcc_adaptive_sch);
		if (enable_mcc_adaptive_sch) {
			sme_debug("Enable mcc_adaptive_scheduler");
			policy_mgr_set_dynamic_mcc_adaptive_sch(
						  mac_ctx->psoc, true);
			if (QDF_STATUS_SUCCESS != sme_set_mas(true)) {
				sme_err("Failed to enable mcc_adaptive_sched");
				return -EAGAIN;
			}
		}
	} else {
		QDF_STATUS status = QDF_STATUS_SUCCESS;
		uint32_t quota_val;

		quota_val =
			ucfg_mlme_get_user_mcc_quota_percentage(mac_ctx->psoc);

		if (quota_val) {
			if (enable_mcc_adaptive_sch) {
				policy_mgr_set_dynamic_mcc_adaptive_sch(
							mac_ctx->psoc, false);
				status = sme_set_mas(false);
			}
			if (status == QDF_STATUS_SUCCESS)
				sme_cli_set_command(wlan_vdev_get_id(vdev),
						    WMA_VDEV_MCC_SET_TIME_QUOTA,
						    quota_val, VDEV_CMD);
		} else {
			sme_debug("no applicable user mcc/quota");
		}
	}

	/*
	 * For open mode authentication, send dummy install key response to
	 * send OBSS scan and QOS event.
	 */
	if (!rsp->is_wps_connection && cm_is_open_mode(vdev)) {
		install_key_rsp.length = sizeof(install_key_rsp);
		install_key_rsp.status_code = eSIR_SME_SUCCESS;
		install_key_rsp.sessionId = vdev_id;
		/* use BC mac to enable OBSS scan */
		qdf_copy_macaddr(&install_key_rsp.peer_macaddr, &bc_mac);
		csr_roam_chk_lnk_set_ctx_rsp(mac_ctx,
					     (tSirSmeRsp *)&install_key_rsp);
	}

	rsn_cap = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_RSN_CAP);
	if (rsn_cap >= 0) {
		cm_update_rsn_ocv_cap(&rsn_cap, rsp);
		if (wma_cli_set2_command(vdev_id, wmi_vdev_param_rsn_capability,
					 rsn_cap, 0, VDEV_CMD))
			sme_err("Failed to update wmi_vdev_param_rsn_capability for vdev id %d",
				vdev_id);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cm_csr_handle_diconnect_req(struct wlan_objmgr_vdev *vdev,
				       struct wlan_cm_vdev_discon_req *req)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct csr_roam_session *session;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session || !CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("session not found for vdev_id %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	cm_csr_set_joining(vdev_id);

	/* Update the disconnect stats */
	session->disconnect_stats.disconnection_cnt++;
	if (req->req.source == CM_PEER_DISCONNECT) {
		session->disconnect_stats.disassoc_by_peer++;
	} else if (req->req.source == CM_SB_DISCONNECT) {
		switch (req->req.reason_code) {
		case REASON_DISASSOC_DUE_TO_INACTIVITY:
			session->disconnect_stats.peer_kickout++;
			break;
		case REASON_BEACON_MISSED:
			session->disconnect_stats.bmiss++;
			break;
		default:
			/* Unknown reason code */
			break;
		}
	} else {
		session->disconnect_stats.disconnection_by_app++;
	}

	csr_update_prev_ap_info(session, vdev);
	csr_qos_send_disconnect_ind(mac_ctx, vdev_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_csr_diconnect_done_ind(struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_discon_rsp *rsp)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	cm_csr_set_idle(vdev_id);
	if (cm_is_vdev_roaming(vdev))
		sme_qos_update_hand_off(vdev_id, false);
	csr_set_default_dot11_mode(mac_ctx);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_HOST_ROAM
void cm_csr_preauth_done(struct wlan_objmgr_vdev *vdev)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct cm_roam_values_copy config;
	bool is_11r;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return;
	}

	wlan_cm_roam_cfg_get_value(mac_ctx->psoc, vdev_id, IS_11R_CONNECTION,
				   &config);
	is_11r = config.bool_value;
	if (is_11r || wlan_cm_get_ese_assoc(mac_ctx->pdev, vdev_id))
		sme_qos_csr_event_ind(mac_ctx, vdev_id,
				      SME_QOS_CSR_PREAUTH_SUCCESS_IND, NULL);
}
#endif

/* */
QDF_STATUS csr_send_mb_disassoc_req_msg(struct mac_context *mac,
					uint32_t sessionId,
					tSirMacAddr bssId, uint16_t reasonCode)
{
	struct disassoc_req *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!CSR_IS_SESSION_VALID(mac, sessionId))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_DISASSOC_REQ;
	pMsg->length = sizeof(*pMsg);
	pMsg->sessionId = sessionId;

	wlan_mlme_get_mac_vdev_id(mac->pdev, sessionId, &pMsg->bssid);
	qdf_mem_copy(&pMsg->peer_macaddr.bytes, bssId, QDF_MAC_ADDR_SIZE);
	pMsg->reasonCode = reasonCode;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_chng_mcc_beacon_interval(struct mac_context *mac,
						uint32_t sessionId)
{
	struct wlan_change_bi *pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* NO need to update the Beacon Params if update beacon parameter flag
	 * is not set
	 */
	if (!mac->roam.roamSession[sessionId].update_bcn_int)
		return QDF_STATUS_SUCCESS;

	mac->roam.roamSession[sessionId].update_bcn_int =
		false;

	/* Create the message and send to lim */
	len = sizeof(*pMsg);
	pMsg = qdf_mem_malloc(len);
	if (!pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMsg->message_type = eWNI_SME_CHNG_MCC_BEACON_INTERVAL;
		pMsg->length = len;

		wlan_mlme_get_mac_vdev_id(mac->pdev, sessionId,
					  &pMsg->bssid);
		sme_debug("CSR Attempting to change BI for Bssid= "
			  QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(pMsg->bssid.bytes));
		pMsg->session_id = sessionId;
		sme_debug("session %d BeaconInterval %d",
			sessionId,
			mac->roam.roamSession[sessionId].bcn_int);
		pMsg->beacon_interval =
			mac->roam.roamSession[sessionId].bcn_int;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}

#ifdef QCA_HT_2040_COEX
QDF_STATUS csr_set_ht2040_mode(struct mac_context *mac, uint32_t sessionId,
			       ePhyChanBondState cbMode, bool obssEnabled)
{
	struct set_ht2040_mode *pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Create the message and send to lim */
	len = sizeof(struct set_ht2040_mode);
	pMsg = qdf_mem_malloc(len);
	if (!pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_zero(pMsg, sizeof(struct set_ht2040_mode));
		pMsg->messageType = eWNI_SME_SET_HT_2040_MODE;
		pMsg->length = len;

		wlan_mlme_get_mac_vdev_id(mac->pdev, sessionId, &pMsg->bssid);
		sme_debug(
			"CSR Attempting to set HT20/40 mode for Bssid= "
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(pMsg->bssid.bytes));
		pMsg->sessionId = sessionId;
		sme_debug("  session %d HT20/40 mode %d",
			sessionId, cbMode);
		pMsg->cbMode = cbMode;
		pMsg->obssEnabled = obssEnabled;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}
#endif

QDF_STATUS csr_send_mb_deauth_req_msg(struct mac_context *mac,
				      uint32_t vdev_id,
				      tSirMacAddr bssId, uint16_t reasonCode)
{
	struct deauth_req *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!CSR_IS_SESSION_VALID(mac, vdev_id))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_DEAUTH_REQ;
	pMsg->length = sizeof(*pMsg);
	pMsg->vdev_id = vdev_id;

	wlan_mlme_get_mac_vdev_id(mac->pdev, vdev_id, &pMsg->bssid);
	/* Set the peer MAC address before sending the message to LIM */
	qdf_mem_copy(&pMsg->peer_macaddr.bytes, bssId, QDF_MAC_ADDR_SIZE);
	pMsg->reasonCode = reasonCode;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_mb_disassoc_cnf_msg(struct mac_context *mac,
					struct disassoc_ind *pDisassocInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct disassoc_cnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DISASSOC_CNF;
		pMsg->status_code = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(*pMsg);
		pMsg->vdev_id = pDisassocInd->vdev_id;
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDisassocInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		qdf_copy_macaddr(&pMsg->bssid, &pDisassocInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_mb_deauth_cnf_msg(struct mac_context *mac,
				      struct deauth_ind *pDeauthInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct deauth_cnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DEAUTH_CNF;
		pMsg->status_code = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(*pMsg);
		pMsg->vdev_id = pDeauthInd->vdev_id;
		qdf_copy_macaddr(&pMsg->bssid, &pDeauthInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDeauthInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_assoc_cnf_msg(struct mac_context *mac,
				  struct assoc_ind *pAssocInd,
				  QDF_STATUS Halstatus,
				  enum wlan_status_code mac_status_code)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct assoc_cnf *pMsg;
	struct scheduler_msg msg = { 0 };

	sme_debug("HalStatus: %d, mac_status_code %d",
		  Halstatus, mac_status_code);
	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			return QDF_STATUS_E_NOMEM;
		pMsg->messageType = eWNI_SME_ASSOC_CNF;
		pMsg->length = sizeof(*pMsg);
		if (QDF_IS_STATUS_SUCCESS(Halstatus)) {
			pMsg->status_code = eSIR_SME_SUCCESS;
		} else {
			pMsg->status_code = eSIR_SME_ASSOC_REFUSED;
			pMsg->mac_status_code = mac_status_code;
		}
		/* bssId */
		qdf_mem_copy(pMsg->bssid.bytes, pAssocInd->bssId,
			     QDF_MAC_ADDR_SIZE);
		/* peerMacAddr */
		qdf_mem_copy(pMsg->peer_macaddr.bytes, pAssocInd->peerMacAddr,
			     QDF_MAC_ADDR_SIZE);
		/* aid */
		pMsg->aid = pAssocInd->aid;
		/* OWE IE */
		if (pAssocInd->owe_ie_len) {
			pMsg->owe_ie = qdf_mem_malloc(pAssocInd->owe_ie_len);
			if (!pMsg->owe_ie)
				return QDF_STATUS_E_NOMEM;
			qdf_mem_copy(pMsg->owe_ie, pAssocInd->owe_ie,
				     pAssocInd->owe_ie_len);
			pMsg->owe_ie_len = pAssocInd->owe_ie_len;
		}

		if (pAssocInd->ft_ie_len) {
			pMsg->ft_ie = qdf_mem_malloc(pAssocInd->ft_ie_len);
			if (!pMsg->ft_ie)
				return QDF_STATUS_E_NOMEM;
			qdf_mem_copy(pMsg->ft_ie, pAssocInd->ft_ie,
				     pAssocInd->ft_ie_len);
			pMsg->ft_ie_len = pAssocInd->ft_ie_len;
		}
		pMsg->need_assoc_rsp_tx_cb = pAssocInd->need_assoc_rsp_tx_cb;

		msg.type = pMsg->messageType;
		msg.bodyval = 0;
		msg.bodyptr = pMsg;
		/* pMsg is freed by umac_send_mb_message_to_mac in anycase*/
		status = scheduler_post_msg_by_priority(QDF_MODULE_ID_PE, &msg,
							true);
	} while (0);
	return status;
}

/**
 * csr_store_oce_cfg_flags_in_vdev() - fill OCE flags from ini
 * @mac: mac_context.
 * @vdev: Pointer to pdev obj
 * @vdev_id: vdev_id
 *
 * This API will store the oce flags in vdev mlme priv object
 *
 * Return: none
 */
static void csr_store_oce_cfg_flags_in_vdev(struct mac_context *mac,
					    struct wlan_objmgr_pdev *pdev,
					    uint8_t vdev_id)
{
	uint8_t *vdev_dynamic_oce;
	struct wlan_objmgr_vdev *vdev =
	wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_LEGACY_MAC_ID);

	if (!vdev)
		return;

	vdev_dynamic_oce = mlme_get_dynamic_oce_flags(vdev);
	if (vdev_dynamic_oce)
		*vdev_dynamic_oce = mac->mlme_cfg->oce.feature_bitmap;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

void csr_send_set_ie(uint8_t type, uint8_t sub_type,
		     uint8_t vdev_id)
{
	struct send_extcap_ie *msg;
	QDF_STATUS status;

	sme_debug("send SET IE msg to PE");

	if (!(type == WLAN_VDEV_MLME_TYPE_STA ||
	      (type == WLAN_VDEV_MLME_TYPE_AP &&
	      sub_type == WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE))) {
		sme_debug("Failed to send set IE req for vdev_%d", vdev_id);
		return;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return;

	msg->msg_type = eWNI_SME_SET_IE_REQ;
	msg->session_id = vdev_id;
	msg->length = sizeof(*msg);
	status = umac_send_mb_message_to_mac(msg);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_debug("Failed to send set IE req for vdev_%d", vdev_id);
}

void csr_get_vdev_type_nss(enum QDF_OPMODE dev_mode, uint8_t *nss_2g,
			   uint8_t *nss_5g)
{
	struct mac_context *mac_ctx = sme_get_mac_context();

	if (!mac_ctx) {
		sme_err("Invalid MAC context");
		return;
	}

	switch (dev_mode) {
	case QDF_STA_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sta;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sta;
		break;
	case QDF_SAP_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sap;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sap;
		break;
	case QDF_P2P_CLIENT_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_cli;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_cli;
		break;
	case QDF_P2P_GO_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_go;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_go;
		break;
	case QDF_P2P_DEVICE_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_dev;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_dev;
		break;
	case QDF_IBSS_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ibss;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ibss;
		break;
	case QDF_OCB_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ocb;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ocb;
		break;
	case QDF_NAN_DISC_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.nan;
		*nss_5g = mac_ctx->vdev_type_nss_5g.nan;
		break;
	case QDF_NDI_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ndi;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ndi;
		break;
	default:
		*nss_2g = 1;
		*nss_5g = 1;
		sme_err("Unknown device mode");
		break;
	}
	sme_debug("mode - %d: nss_2g - %d, 5g - %d",
		  dev_mode, *nss_2g, *nss_5g);
}

QDF_STATUS csr_setup_vdev_session(struct vdev_mlme_obj *vdev_mlme)
{
	QDF_STATUS status;
	uint32_t existing_session_id;
	struct csr_roam_session *session;
	struct mlme_vht_capabilities_info *vht_cap_info;
	u8 vdev_id;
	struct qdf_mac_addr *mac_addr;
	mac_handle_t mac_handle;
	struct mac_context *mac_ctx;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_vht_config vht_config;
	struct wlan_ht_config ht_cap;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!(mac_ctx->mlme_cfg)) {
		sme_err("invalid mlme cfg");
		return QDF_STATUS_E_FAILURE;
	}
	vht_cap_info = &mac_ctx->mlme_cfg->vht_caps.vht_cap_info;

	vdev = vdev_mlme->vdev;

	vdev_id = wlan_vdev_get_id(vdev);
	mac_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_macaddr(vdev);
	/* check to see if the mac address already belongs to a session */
	status = csr_roam_get_session_id_from_bssid(mac_ctx, mac_addr,
						    &existing_session_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Session %d exists with mac address "QDF_MAC_ADDR_FMT,
			existing_session_id,
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		return QDF_STATUS_E_FAILURE;
	}

	/* attempt to retrieve session for Id */
	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("Session does not exist for interface %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* check to see if the session is already active */
	if (session->sessionActive) {
		sme_err("Cannot re-open active session with Id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	session->sessionActive = true;
	session->vdev_id = vdev_id;

	ht_cap.caps = 0;
	vht_config.caps = 0;
	ht_cap.ht_caps = mac_ctx->mlme_cfg->ht_caps.ht_cap_info;
	vdev_mlme->proto.ht_info.ht_caps = ht_cap.caps;

	vht_config.max_mpdu_len = vht_cap_info->ampdu_len;
	vht_config.supported_channel_widthset =
			vht_cap_info->supp_chan_width;
	vht_config.ldpc_coding = vht_cap_info->ldpc_coding_cap;
	vht_config.shortgi80 = vht_cap_info->short_gi_80mhz;
	vht_config.shortgi160and80plus80 =
			vht_cap_info->short_gi_160mhz;
	vht_config.tx_stbc = vht_cap_info->tx_stbc;
	vht_config.rx_stbc = vht_cap_info->rx_stbc;
	vht_config.su_beam_former = vht_cap_info->su_bformer;
	vht_config.su_beam_formee = vht_cap_info->su_bformee;
	vht_config.csnof_beamformer_antSup =
			vht_cap_info->tx_bfee_ant_supp;
	vht_config.num_soundingdim = vht_cap_info->num_soundingdim;
	vht_config.mu_beam_former = vht_cap_info->mu_bformer;
	vht_config.mu_beam_formee = vht_cap_info->enable_mu_bformee;
	vht_config.vht_txops = vht_cap_info->txop_ps;
	vht_config.htc_vhtcap = vht_cap_info->htc_vhtc;
	vht_config.rx_antpattern = vht_cap_info->rx_antpattern;
	vht_config.tx_antpattern = vht_cap_info->tx_antpattern;

	vht_config.max_ampdu_lenexp =
			vht_cap_info->ampdu_len_exponent;
	vdev_mlme->proto.vht_info.caps = vht_config.caps;
	csr_update_session_he_cap(mac_ctx, session);
	csr_update_session_eht_cap(mac_ctx, session);

	csr_send_set_ie(vdev_mlme->mgmt.generic.type,
			vdev_mlme->mgmt.generic.subtype,
			wlan_vdev_get_id(vdev));

	if (vdev_mlme->mgmt.generic.type == WLAN_VDEV_MLME_TYPE_STA) {
		csr_store_oce_cfg_flags_in_vdev(mac_ctx, mac_ctx->pdev,
						wlan_vdev_get_id(vdev));
		wlan_mlme_update_oce_flags(mac_ctx->pdev);
	}

	return QDF_STATUS_SUCCESS;
}

void csr_cleanup_vdev_session(struct mac_context *mac, uint8_t vdev_id)
{
	if (CSR_IS_SESSION_VALID(mac, vdev_id)) {
		struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
								vdev_id);

		csr_flush_roam_scan_chan_lists(mac, vdev_id);
		csr_roam_free_connected_info(mac, &pSession->connectedInfo);
		csr_init_session(mac, vdev_id);
	}
}

QDF_STATUS csr_prepare_vdev_delete(struct mac_context *mac_ctx,
				   uint8_t vdev_id, bool cleanup)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session)
		return QDF_STATUS_E_INVAL;

	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id))
		return QDF_STATUS_E_INVAL;

	if (cleanup) {
		csr_cleanup_vdev_session(mac_ctx, vdev_id);
		return status;
	}

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, vdev_id)) {
		sme_debug("Stop Wait for key timer and change substate to eCSR_ROAM_SUBSTATE_NONE");
		cm_stop_wait_for_key_timer(mac_ctx->psoc, vdev_id);
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 vdev_id);
	}

	/* Flush all the commands for vdev */
	wlan_serialization_purge_all_cmd_by_vdev_id(mac_ctx->pdev, vdev_id);
	if (!mac_ctx->session_close_cb) {
		sme_err("no close session callback registered");
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

static void csr_init_session(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession)
		return;

	pSession->sessionActive = false;
	pSession->vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	csr_roam_free_connected_info(mac, &pSession->connectedInfo);
}

static void csr_get_vdev_id_from_bssid(struct wlan_objmgr_pdev *pdev,
				       void *object, void *arg)
{
	struct bssid_search_arg *bssid_arg = arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct wlan_objmgr_peer *peer;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
		return;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_LEGACY_SME_ID);
	if (!peer)
		return;

	if (WLAN_ADDR_EQ(bssid_arg->peer_addr.bytes,
			 wlan_peer_get_macaddr(peer)) == QDF_STATUS_SUCCESS)
		bssid_arg->vdev_id = wlan_vdev_get_id(vdev);

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_SME_ID);
}

QDF_STATUS csr_roam_get_session_id_from_bssid(struct mac_context *mac,
					      struct qdf_mac_addr *bssid,
					      uint32_t *pSessionId)
{
	struct bssid_search_arg bssid_arg;

	qdf_copy_macaddr(&bssid_arg.peer_addr, bssid);
	bssid_arg.vdev_id = WLAN_MAX_VDEVS;
	wlan_objmgr_pdev_iterate_obj_list(mac->pdev, WLAN_VDEV_OP,
					  csr_get_vdev_id_from_bssid,
					  &bssid_arg, 0,
					  WLAN_LEGACY_SME_ID);
	if (bssid_arg.vdev_id >= WLAN_MAX_VDEVS) {
		*pSessionId = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*pSessionId = bssid_arg.vdev_id;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_get_snr(struct mac_context *mac,
		       tCsrSnrCallback callback,
		       struct qdf_mac_addr bssId, void *pContext)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	tAniGetSnrReq *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tAniGetSnrReq));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	status = csr_roam_get_session_id_from_bssid(mac, &bssId, &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(pMsg);
		sme_err("Couldn't find session_id for given BSSID");
		return status;
	}

	pMsg->msgType = eWNI_SME_GET_SNR_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetSnrReq);
	pMsg->sessionId = sessionId;
	pMsg->snrCallback = callback;
	pMsg->pDevContext = pContext;
	msg.type = eWNI_SME_GET_SNR_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS csr_invoke_neighbor_report_request(
				uint8_t session_id,
				struct sRrmNeighborReq *neighbor_report_req,
				bool send_resp_to_host)
{
	struct wmi_invoke_neighbor_report_params *invoke_params;
	struct scheduler_msg msg = {0};

	if (!neighbor_report_req) {
		sme_err("Invalid params");
		return QDF_STATUS_E_INVAL;
	}

	invoke_params = qdf_mem_malloc(sizeof(*invoke_params));
	if (!invoke_params)
		return QDF_STATUS_E_NOMEM;

	invoke_params->vdev_id = session_id;
	invoke_params->send_resp_to_host = send_resp_to_host;

	if (!neighbor_report_req->no_ssid) {
		invoke_params->ssid.length = neighbor_report_req->ssid.length;
		qdf_mem_copy(invoke_params->ssid.ssid,
			     neighbor_report_req->ssid.ssId,
			     neighbor_report_req->ssid.length);
	} else {
		invoke_params->ssid.length = 0;
	}

	sme_debug("Sending SIR_HAL_INVOKE_NEIGHBOR_REPORT");

	msg.type = SIR_HAL_INVOKE_NEIGHBOR_REPORT;
	msg.reserved = 0;
	msg.bodyptr = invoke_params;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		sme_err("Not able to post message to WMA");
		qdf_mem_free(invoke_params);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
void wlan_cm_ese_populate_additional_ies(struct wlan_objmgr_pdev *pdev,
			struct wlan_mlme_psoc_ext_obj *mlme_obj,
			uint8_t vdev_id,
			struct wlan_roam_scan_offload_params *rso_mode_cfg)
{
	uint8_t tspec_ie_hdr[SIR_MAC_OUI_WME_HDR_MIN]
			= { 0x00, 0x50, 0xf2, 0x02, 0x02, 0x01 };
	uint8_t tspec_ie_buf[DOT11F_IE_WMMTSPEC_MAX_LEN], j;
	ese_wmm_tspec_ie *tspec_ie;
	tESETspecInfo ese_tspec;
	struct mac_context *mac_ctx;
	struct csr_roam_session *session;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session is null %d", vdev_id);
		return;
	}

	tspec_ie = (ese_wmm_tspec_ie *)(tspec_ie_buf + SIR_MAC_OUI_WME_HDR_MIN);
	if (csr_is_wmm_supported(mac_ctx) &&
	    mlme_obj->cfg.lfr.ese_enabled &&
	    wlan_cm_get_ese_assoc(pdev, session->vdev_id)) {
		ese_tspec.numTspecs = sme_qos_ese_retrieve_tspec_info(
					mac_ctx, session->vdev_id,
					(tTspecInfo *)&ese_tspec.tspec[0]);
		qdf_mem_copy(tspec_ie_buf, tspec_ie_hdr,
			     SIR_MAC_OUI_WME_HDR_MIN);
		for (j = 0; j < ese_tspec.numTspecs; j++) {
			/* Populate the tspec_ie */
			ese_populate_wmm_tspec(&ese_tspec.tspec[j].tspec,
					       tspec_ie);
			wlan_cm_append_assoc_ies(rso_mode_cfg,
						 WLAN_ELEMID_VENDOR,
						  DOT11F_IE_WMMTSPEC_MAX_LEN,
						 tspec_ie_buf);
		}
	}
}
#endif

uint8_t *wlan_cm_get_rrm_cap_ie_data(void)
{
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return NULL;
	}

	return (uint8_t *)&mac_ctx->rrm.rrmPEContext.rrmEnabledCaps;
}
#endif

tSmeCmd *csr_get_command_buffer(struct mac_context *mac)
{
	tSmeCmd *pCmd = sme_get_command_buffer(mac);

	if (pCmd)
		mac->roam.sPendingCommands++;

	return pCmd;
}

static void csr_free_cmd_memory(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (!pCommand) {
		sme_err("pCommand is NULL");
		return;
	}
	switch (pCommand->command) {
	case eSmeCommandRoam:
		csr_release_command_roam(mac, pCommand);
		break;
	case eSmeCommandWmStatusChange:
		csr_release_command_wm_status_change(mac, pCommand);
		break;
	default:
		break;
	}
}

void csr_release_command_buffer(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (mac->roam.sPendingCommands > 0) {
		/*
		 * All command allocated through csr_get_command_buffer
		 * need to decrement the pending count when releasing
		 */
		mac->roam.sPendingCommands--;
		csr_free_cmd_memory(mac, pCommand);
		sme_release_command(mac, pCommand);
	} else {
		sme_err("no pending commands");
		QDF_ASSERT(0);
	}
}

void csr_release_command(struct mac_context *mac_ctx, tSmeCmd *sme_cmd)
{
	struct wlan_serialization_queued_cmd_info cmd_info;
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev;

	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
			sme_cmd->vdev_id, WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}
	qdf_mem_zero(&cmd_info,
			sizeof(struct wlan_serialization_queued_cmd_info));
	cmd_info.cmd_id = sme_cmd->cmd_id;
	cmd_info.req_type = WLAN_SER_CANCEL_NON_SCAN_CMD;
	cmd_info.cmd_type = csr_get_cmd_type(sme_cmd);
	cmd_info.vdev = vdev;
	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	cmd.cmd_id = cmd_info.cmd_id;
	cmd.cmd_type = cmd_info.cmd_type;
	cmd.vdev = cmd_info.vdev;
	if (wlan_serialization_is_cmd_present_in_active_queue(
				mac_ctx->psoc, &cmd)) {
		cmd_info.queue_type = WLAN_SERIALIZATION_ACTIVE_QUEUE;
		wlan_serialization_remove_cmd(&cmd_info);
	} else if (wlan_serialization_is_cmd_present_in_pending_queue(
				mac_ctx->psoc, &cmd)) {
		cmd_info.queue_type = WLAN_SERIALIZATION_PENDING_QUEUE;
		wlan_serialization_cancel_request(&cmd_info);
	} else {
		sme_debug("can't find cmd_id %d cmd_type %d", cmd_info.cmd_id,
			  cmd_info.cmd_type);
	}
	if (cmd_info.vdev)
		wlan_objmgr_vdev_release_ref(cmd_info.vdev, WLAN_LEGACY_SME_ID);
}


static enum wlan_serialization_cmd_type csr_get_roam_cmd_type(
		tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->u.roamCmd.roamReason) {
	case eCsrForcedDisassocSta:
		cmd_type = WLAN_SER_CMD_FORCE_DISASSOC_STA;
		break;
	case eCsrForcedDeauthSta:
		cmd_type = WLAN_SER_CMD_FORCE_DEAUTH_STA;
		break;
	default:
		break;
	}

	return cmd_type;
}

enum wlan_serialization_cmd_type csr_get_cmd_type(tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->command) {
	case eSmeCommandRoam:
		cmd_type = csr_get_roam_cmd_type(sme_cmd);
		break;
	case eSmeCommandWmStatusChange:
		cmd_type = WLAN_SER_CMD_WM_STATUS_CHANGE;
		break;
	case eSmeCommandAddTs:
		cmd_type = WLAN_SER_CMD_ADDTS;
		break;
	case eSmeCommandDelTs:
		cmd_type = WLAN_SER_CMD_DELTS;
		break;
	case e_sme_command_set_hw_mode:
		cmd_type = WLAN_SER_CMD_SET_HW_MODE;
		break;
	case e_sme_command_nss_update:
		cmd_type = WLAN_SER_CMD_NSS_UPDATE;
		break;
	case e_sme_command_set_dual_mac_config:
		cmd_type = WLAN_SER_CMD_SET_DUAL_MAC_CONFIG;
		break;
	case e_sme_command_set_antenna_mode:
		cmd_type = WLAN_SER_CMD_SET_ANTENNA_MODE;
		break;
	default:
		break;
	}

	return cmd_type;
}

static uint32_t csr_get_monotonous_number(struct mac_context *mac_ctx)
{
	uint32_t cmd_id;
	uint32_t mask = 0x00FFFFFF, prefix = 0x0D000000;

	cmd_id = qdf_atomic_inc_return(&mac_ctx->global_cmd_id);
	cmd_id = (cmd_id & mask);
	cmd_id = (cmd_id | prefix);

	return cmd_id;
}

static void csr_fill_cmd_timeout(struct wlan_serialization_command *cmd)
{
	switch (cmd->cmd_type) {
	case WLAN_SER_CMD_WM_STATUS_CHANGE:
		cmd->cmd_timeout_duration = SME_CMD_PEER_DISCONNECT_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_START_BSS:
		cmd->cmd_timeout_duration = SME_CMD_VDEV_START_BSS_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_STOP_BSS:
		cmd->cmd_timeout_duration = SME_CMD_STOP_BSS_CMD_TIMEOUT;
		break;
	case WLAN_SER_CMD_FORCE_DISASSOC_STA:
	case WLAN_SER_CMD_FORCE_DEAUTH_STA:
		cmd->cmd_timeout_duration = SME_CMD_PEER_DISCONNECT_TIMEOUT;
		break;

	case WLAN_SER_CMD_ADDTS:
	case WLAN_SER_CMD_DELTS:
		cmd->cmd_timeout_duration = SME_CMD_ADD_DEL_TS_TIMEOUT;
		break;
	case WLAN_SER_CMD_SET_HW_MODE:
	case WLAN_SER_CMD_NSS_UPDATE:
	case WLAN_SER_CMD_SET_DUAL_MAC_CONFIG:
	case WLAN_SER_CMD_SET_ANTENNA_MODE:
		cmd->cmd_timeout_duration = SME_CMD_POLICY_MGR_CMD_TIMEOUT;
		break;
	default:
		cmd->cmd_timeout_duration = SME_ACTIVE_LIST_CMD_TIMEOUT_VALUE;
		break;
	}
}

QDF_STATUS csr_set_serialization_params_to_cmd(struct mac_context *mac_ctx,
		tSmeCmd *sme_cmd, struct wlan_serialization_command *cmd,
		uint8_t high_priority)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!sme_cmd) {
		sme_err("Invalid sme_cmd");
		return status;
	}
	if (!cmd) {
		sme_err("Invalid serialization_cmd");
		return status;
	}

	/*
	 * no need to fill command id for non-scan as they will be
	 * zero always
	 */
	sme_cmd->cmd_id = csr_get_monotonous_number(mac_ctx);
	cmd->cmd_id = sme_cmd->cmd_id;

	cmd->cmd_type = csr_get_cmd_type(sme_cmd);
	if (cmd->cmd_type == WLAN_SER_CMD_MAX) {
		sme_err("serialization enum not found for sme_cmd type %d",
			sme_cmd->command);
		return status;
	}
	cmd->vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
				sme_cmd->vdev_id, WLAN_LEGACY_SME_ID);
	if (!cmd->vdev) {
		sme_err("vdev is NULL for vdev_id:%d", sme_cmd->vdev_id);
		return status;
	}
	cmd->umac_cmd = sme_cmd;

	csr_fill_cmd_timeout(cmd);

	cmd->source = WLAN_UMAC_COMP_MLME;
	cmd->cmd_cb = sme_ser_cmd_callback;
	cmd->is_high_priority = high_priority;
	cmd->is_blocking = true;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_queue_sme_command(struct mac_context *mac_ctx, tSmeCmd *sme_cmd,
				 bool high_priority)
{
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev = NULL;
	enum wlan_serialization_status ser_cmd_status;
	QDF_STATUS status;

	if (!SME_IS_START(mac_ctx)) {
		sme_err("Sme in stop state");
		QDF_ASSERT(0);
		goto error;
	}

	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	status = csr_set_serialization_params_to_cmd(mac_ctx, sme_cmd,
					&cmd, high_priority);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to set ser params");
		goto error;
	}

	vdev = cmd.vdev;
	ser_cmd_status = wlan_serialization_request(&cmd);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
	case WLAN_SER_CMD_ACTIVE:
		/* Command posted to active/pending list */
		status = QDF_STATUS_SUCCESS;
		break;
	default:
		sme_err("Failed to queue command %d with status:%d",
			  sme_cmd->command, ser_cmd_status);
		status = QDF_STATUS_E_FAILURE;
		goto error;
	}

	return status;

error:
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	csr_release_command_buffer(mac_ctx, sme_cmd);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS csr_roam_update_config(struct mac_context *mac_ctx, uint8_t session_id,
				  uint16_t capab, uint32_t value)
{
	struct update_config *msg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	sme_debug("update HT config requested");
	if (!session) {
		sme_err("Session does not exist for session id %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->messageType = eWNI_SME_UPDATE_CONFIG;
	msg->vdev_id = session_id;
	msg->capab = capab;
	msg->value = value;
	msg->length = sizeof(*msg);
	status = umac_send_mb_message_to_mac(msg);

	return status;
}

QDF_STATUS csr_send_channel_change_req(struct mac_context *mac,
				      struct channel_change_req *req)
{
	struct scheduler_msg msg = {0};
	struct channel_change_req *ch_change_req;
	QDF_STATUS status;

	ch_change_req = qdf_mem_malloc(sizeof(*ch_change_req));
	if (!ch_change_req)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(ch_change_req, req, sizeof(*ch_change_req));
	msg.type = eWNI_SME_CHANNEL_CHANGE_REQ;
	msg.bodyptr = ch_change_req;
	msg.reserved = 0;

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE,
					&msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to send channel change request with status : %d",
			status);
		qdf_mem_free(ch_change_req);
	}

	return status;
}
/*
 * Post Beacon Tx Start request to LIM
 * immediately after SAP CAC WAIT is
 * completed without any RADAR indications.
 */
QDF_STATUS csr_roam_start_beacon_req(struct mac_context *mac,
				     struct qdf_mac_addr bssid,
				     uint8_t dfsCacWaitStatus)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirStartBeaconIndication *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tSirStartBeaconIndication));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_START_BEACON_REQ;
	pMsg->messageLen = sizeof(tSirStartBeaconIndication);
	pMsg->beaconStartStatus = dfsCacWaitStatus;
	qdf_mem_copy(pMsg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);

	status = umac_send_mb_message_to_mac(pMsg);

	return status;
}

/*
 * csr_roam_modify_add_ies -
 * This function sends msg to modify the additional IE buffers in PE
 *
 * @mac: mac global structure
 * @pModifyIE: pointer to tSirModifyIE structure
 * @updateType: Type of buffer
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_modify_add_ies(struct mac_context *mac,
			 tSirModifyIE *pModifyIE, eUpdateIEsType updateType)
{
	tpSirModifyIEsInd pModifyAddIEInd = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	/* following buffer will be freed by consumer (PE) */
	pLocalBuffer = qdf_mem_malloc(pModifyIE->ieBufferlength);
	if (!pLocalBuffer)
		return QDF_STATUS_E_NOMEM;

	pModifyAddIEInd = qdf_mem_malloc(sizeof(tSirModifyIEsInd));
	if (!pModifyAddIEInd) {
		qdf_mem_free(pLocalBuffer);
		return QDF_STATUS_E_NOMEM;
	}

	/*copy the IE buffer */
	qdf_mem_copy(pLocalBuffer, pModifyIE->pIEBuffer,
		     pModifyIE->ieBufferlength);
	qdf_mem_zero(pModifyAddIEInd, sizeof(tSirModifyIEsInd));

	pModifyAddIEInd->msgType = eWNI_SME_MODIFY_ADDITIONAL_IES;
	pModifyAddIEInd->msgLen = sizeof(tSirModifyIEsInd);

	qdf_copy_macaddr(&pModifyAddIEInd->modifyIE.bssid, &pModifyIE->bssid);

	pModifyAddIEInd->modifyIE.vdev_id = pModifyIE->vdev_id;
	pModifyAddIEInd->modifyIE.notify = pModifyIE->notify;
	pModifyAddIEInd->modifyIE.ieID = pModifyIE->ieID;
	pModifyAddIEInd->modifyIE.ieIDLen = pModifyIE->ieIDLen;
	pModifyAddIEInd->modifyIE.pIEBuffer = pLocalBuffer;
	pModifyAddIEInd->modifyIE.ieBufferlength = pModifyIE->ieBufferlength;
	pModifyAddIEInd->modifyIE.oui_length = pModifyIE->oui_length;

	pModifyAddIEInd->updateType = updateType;

	status = umac_send_mb_message_to_mac(pModifyAddIEInd);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/*
 * csr_roam_update_add_ies -
 * This function sends msg to updates the additional IE buffers in PE
 *
 * @mac: mac global structure
 * @sessionId: SME session id
 * @bssid: BSSID
 * @additionIEBuffer: buffer containing addition IE from hostapd
 * @length: length of buffer
 * @updateType: Type of buffer
 * @append: append or replace completely
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_update_add_ies(struct mac_context *mac,
			 tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType)
{
	tpSirUpdateIEsInd pUpdateAddIEs = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	if (pUpdateIE->ieBufferlength != 0) {
		/* Following buffer will be freed by consumer (PE) */
		pLocalBuffer = qdf_mem_malloc(pUpdateIE->ieBufferlength);
		if (!pLocalBuffer) {
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_copy(pLocalBuffer, pUpdateIE->pAdditionIEBuffer,
			     pUpdateIE->ieBufferlength);
	}

	pUpdateAddIEs = qdf_mem_malloc(sizeof(tSirUpdateIEsInd));
	if (!pUpdateAddIEs) {
		qdf_mem_free(pLocalBuffer);
		return QDF_STATUS_E_NOMEM;
	}

	pUpdateAddIEs->msgType = eWNI_SME_UPDATE_ADDITIONAL_IES;
	pUpdateAddIEs->msgLen = sizeof(tSirUpdateIEsInd);

	qdf_copy_macaddr(&pUpdateAddIEs->updateIE.bssid, &pUpdateIE->bssid);

	pUpdateAddIEs->updateIE.vdev_id = pUpdateIE->vdev_id;
	pUpdateAddIEs->updateIE.append = pUpdateIE->append;
	pUpdateAddIEs->updateIE.notify = pUpdateIE->notify;
	pUpdateAddIEs->updateIE.ieBufferlength = pUpdateIE->ieBufferlength;
	pUpdateAddIEs->updateIE.pAdditionIEBuffer = pLocalBuffer;

	pUpdateAddIEs->updateType = updateType;

	status = umac_send_mb_message_to_mac(pUpdateAddIEs);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/**
 * csr_send_ext_change_freq()- function to post send ECSA
 * action frame to lim.
 * @mac_ctx: pointer to global mac structure
 * @ch_freq: new channel freq to switch
 * @session_id: senssion it should be sent on.
 *
 * This function is called to post ECSA frame to lim.
 *
 * Return: success if msg posted to LIM else return failure
 */
QDF_STATUS csr_send_ext_change_freq(struct mac_context *mac_ctx,
				    qdf_freq_t ch_freq, uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct sir_sme_ext_cng_chan_req *msg;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->message_type = eWNI_SME_EXT_CHANGE_CHANNEL;
	msg->length = sizeof(*msg);
	msg->new_ch_freq = ch_freq;
	msg->vdev_id = session_id;
	status = umac_send_mb_message_to_mac(msg);
	return status;
}

QDF_STATUS csr_csa_restart(struct mac_context *mac_ctx, uint8_t session_id)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};

	/* Serialize the req through MC thread */
	message.bodyval = session_id;
	message.type    = eWNI_SME_CSA_RESTART_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)", status);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

QDF_STATUS csr_roam_send_chan_sw_ie_request(struct mac_context *mac_ctx,
					    struct qdf_mac_addr bssid,
					    uint32_t target_chan_freq,
					    uint8_t csa_ie_reqd,
					    struct ch_params *ch_params,
					    uint32_t new_cac_ms)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirDfsCsaIeRequest *msg;

	msg = qdf_mem_malloc(sizeof(tSirDfsCsaIeRequest));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->msgType = eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ;
	msg->msgLen = sizeof(tSirDfsCsaIeRequest);

	msg->target_chan_freq = target_chan_freq;
	msg->csaIeRequired = csa_ie_reqd;
	msg->ch_switch_beacon_cnt =
		 mac_ctx->sap.SapDfsInfo.sap_ch_switch_beacon_cnt;
	if (mac_ctx->sap.one_time_csa_count) {
		msg->ch_switch_beacon_cnt = mac_ctx->sap.one_time_csa_count;
		mac_ctx->sap.one_time_csa_count = 0;
	}
	msg->ch_switch_mode = mac_ctx->sap.SapDfsInfo.sap_ch_switch_mode;
	msg->dfs_ch_switch_disable =
		mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch;
	msg->new_chan_cac_ms = new_cac_ms;
	qdf_mem_copy(msg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&msg->ch_params, ch_params, sizeof(struct ch_params));

	status = umac_send_mb_message_to_mac(msg);

	return status;
}

QDF_STATUS csr_sta_continue_csa(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};

	/* Serialize the req through MC thread */
	message.bodyval = vdev_id;
	message.type    = eWNI_SME_STA_CSA_CONTINUE_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("eWNI_SME_STA_CSA_CONTINUE_REQ failed!(err=%d)",
			status);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * csr_update_op_class_array() - update op class for each band
 * @mac_ctx:          mac global context
 * @op_classes:       out param, operating class array to update
 * @channel_info:     channel info
 * @ch_name:          channel band name to display in debug messages
 * @i:                out param, stores number of operating classes
 *
 * Return: void
 */
static void
csr_update_op_class_array(struct mac_context *mac_ctx,
			  uint8_t *op_classes,
			  struct csr_channel *channel_info,
			  char *ch_name,
			  uint8_t *i)
{
	uint8_t j = 0, idx = 0, class = 0;
	bool found = false;
	uint8_t num_channels = channel_info->numChannels;
	uint8_t ch_num;

	sme_debug("Num %s channels,  %d", ch_name, num_channels);

	for (idx = 0; idx < num_channels &&
		*i < (REG_MAX_SUPP_OPER_CLASSES - 1); idx++) {
		wlan_reg_freq_to_chan_op_class(
			mac_ctx->pdev, channel_info->channel_freq_list[idx],
			true, BIT(BEHAV_NONE), &class, &ch_num);

		found = false;
		for (j = 0; j < REG_MAX_SUPP_OPER_CLASSES - 1; j++) {
			if (op_classes[j] == class) {
				found = true;
				break;
			}
		}

		if (!found) {
			op_classes[*i] = class;
			*i = *i + 1;
		}
	}
}

/**
 * csr_init_operating_classes() - update op class for all bands
 * @mac: pointer to mac context.
 *
 * Return: void
 */
static void csr_init_operating_classes(struct mac_context *mac)
{
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t swap = 0;
	uint8_t numClasses = 0;
	uint8_t opClasses[REG_MAX_SUPP_OPER_CLASSES] = {0,};
	uint8_t reg_cc[REG_ALPHA2_LEN + 1];

	wlan_reg_read_current_country(mac->psoc, reg_cc);
	sme_debug("Current Country = %s", reg_cc);

	csr_update_op_class_array(mac, opClasses,
				  &mac->scan.base_channels, "20MHz", &i);
	numClasses = i;

	/* As per spec the operating classes should be in ascending order.
	 * Bubble sort is fine since we don't have many classes
	 */
	for (i = 0; i < (numClasses - 1); i++) {
		for (j = 0; j < (numClasses - i - 1); j++) {
			/* For decreasing order use < */
			if (opClasses[j] > opClasses[j + 1]) {
				swap = opClasses[j];
				opClasses[j] = opClasses[j + 1];
				opClasses[j + 1] = swap;
			}
		}
	}

	/* Set the ordered list of op classes in regdomain
	 * for use by other modules
	 */
	wlan_reg_dmn_set_curr_opclasses(numClasses, &opClasses[0]);
}

/**
 * csr_process_set_hw_mode() - Set HW mode command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the set HW mode command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_set_hw_mode(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct s_sir_set_hw_mode *cmd = NULL;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_set_hw_mode_resp *param;
	enum policy_mgr_hw_mode_change hw_mode;
	enum policy_mgr_conc_next_action action;
	enum set_hw_mode_status hw_mode_change_status =
						SET_HW_MODE_STATUS_ECANCELED;

	/* Setting HW mode is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	action = command->u.set_hw_mode_cmd.action;

	status = policy_mgr_validate_dbs_switch(mac->psoc, action);

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_debug("Hw mode change not sent to FW status = %d", status);
		if (status == QDF_STATUS_E_ALREADY)
			hw_mode_change_status = SET_HW_MODE_STATUS_ALREADY;
		goto fail;
	}

	hw_mode = policy_mgr_get_hw_mode_change_from_hw_mode_index(
			mac->psoc, command->u.set_hw_mode_cmd.hw_mode_index);

	if (POLICY_MGR_HW_MODE_NOT_IN_PROGRESS == hw_mode) {
		sme_err("hw_mode %d, failing", hw_mode);
		goto fail;
	}

	policy_mgr_set_hw_mode_change_in_progress(mac->psoc, hw_mode);
	policy_mgr_reset_connection_update(mac->psoc);

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
	     command->u.set_hw_mode_cmd.reason) &&
	    (true == mac->sme.get_connection_info_cb(NULL, NULL))) {
		sme_err("Set HW mode refused: conn in progress");
		policy_mgr_restart_opportunistic_timer(mac->psoc, false);
		goto reset_state;
	}

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
	     command->u.set_hw_mode_cmd.reason) &&
	    (!command->u.set_hw_mode_cmd.hw_mode_index &&
	     !policy_mgr_need_opportunistic_upgrade(mac->psoc, NULL))) {
		sme_err("Set HW mode to SMM not needed anymore");
		goto reset_state;
	}

	cmd->messageType = eWNI_SME_SET_HW_MODE_REQ;
	cmd->length = len;
	cmd->set_hw.hw_mode_index = command->u.set_hw_mode_cmd.hw_mode_index;
	cmd->set_hw.reason = command->u.set_hw_mode_cmd.reason;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same s_sir_set_hw_mode format.
	 */
	cmd->set_hw.set_hw_mode_cb = command->u.set_hw_mode_cmd.set_hw_mode_cb;

	sme_debug(
		"Posting set hw mode req to PE session:%d reason:%d",
		command->u.set_hw_mode_cmd.session_id,
		command->u.set_hw_mode_cmd.reason);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Posting to PE failed");
		cmd = NULL;
		goto reset_state;
	}
	return;

reset_state:
	policy_mgr_set_hw_mode_change_in_progress(mac->psoc,
			POLICY_MGR_HW_MODE_NOT_IN_PROGRESS);
fail:
	if (cmd)
		qdf_mem_free(cmd);
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_debug("Sending set HW fail response to SME");
	param->status = hw_mode_change_status;
	param->cfgd_hw_mode_index = 0;
	param->num_vdev_mac_entries = 0;
	msg.type = eWNI_SME_SET_HW_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_dual_mac_config() - Set HW mode command to PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_dual_mac_config(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_dual_mac_cfg *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_dual_mac_config_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	cmd->message_type = eWNI_SME_SET_DUAL_MAC_CFG_REQ;
	cmd->length = len;
	cmd->set_dual_mac.scan_config = command->u.set_dual_mac_cmd.scan_config;
	cmd->set_dual_mac.fw_mode_config =
		command->u.set_dual_mac_cmd.fw_mode_config;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same sir_set_dual_mac_cfg format.
	 */
	cmd->set_dual_mac.set_dual_mac_cb =
		command->u.set_dual_mac_cmd.set_dual_mac_cb;

	sme_debug("Posting eWNI_SME_SET_DUAL_MAC_CFG_REQ to PE: %x %x",
		  cmd->set_dual_mac.scan_config,
		  cmd->set_dual_mac.fw_mode_config);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Posting to PE failed");
		goto fail;
	}
	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_antenna_mode() - Set antenna mode command to
 * PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_antenna_mode(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_antenna_mode *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_antenna_mode_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set antenna mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		goto fail;

	cmd->message_type = eWNI_SME_SET_ANTENNA_MODE_REQ;
	cmd->length = len;
	cmd->set_antenna_mode = command->u.set_antenna_mode_cmd;

	sme_debug(
		"Posting eWNI_SME_SET_ANTENNA_MODE_REQ to PE: %d %d",
		cmd->set_antenna_mode.num_rx_chains,
		cmd->set_antenna_mode.num_tx_chains);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Posting to PE failed");
		/*
		 * umac_send_mb_message_to_mac would've released the mem
		 * allocated by cmd.
		 */
		goto fail;
	}

	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_ANTENNA_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_ANTENNA_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_nss_update_req() - Update nss command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the nss update command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_nss_update_req(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_nss_update_request *msg;
	QDF_STATUS status;
	struct scheduler_msg msg_return = {0};
	struct sir_bcn_update_rsp *param;
	struct csr_roam_session *session;


	if (!CSR_IS_SESSION_VALID(mac, command->vdev_id)) {
		sme_err("Invalid session id %d", command->vdev_id);
		goto fail;
	}
	session = CSR_GET_SESSION(mac, command->vdev_id);

	len = sizeof(*msg);
	msg = qdf_mem_malloc(len);
	if (!msg)
		/* Probably the fail response is also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	msg->msgType = eWNI_SME_NSS_UPDATE_REQ;
	msg->msgLen = sizeof(*msg);

	msg->new_nss = command->u.nss_update_cmd.new_nss;
	msg->ch_width = command->u.nss_update_cmd.ch_width;
	msg->vdev_id = command->u.nss_update_cmd.session_id;

	sme_debug("Posting eWNI_SME_NSS_UPDATE_REQ to PE");

	status = umac_send_mb_message_to_mac(msg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	sme_err("Posting to PE failed");
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending nss update fail response to SME");
	param->status = QDF_STATUS_E_FAILURE;
	param->vdev_id = command->u.nss_update_cmd.session_id;
	param->reason = REASON_NSS_UPDATE;
	msg_return.type = eWNI_SME_NSS_UPDATE_RSP;
	msg_return.bodyptr = param;
	msg_return.bodyval = 0;
	sys_process_mmh_msg(mac, &msg_return);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef WLAN_FEATURE_SAE
/**
 * csr_process_roam_auth_sae_callback() - API to trigger the
 * WPA3 pre-auth event for candidate AP received from firmware.
 * @mac_ctx: Global mac context pointer
 * @vdev_id: vdev id
 * @roam_bssid: Candidate BSSID to roam
 * @akm: Candidate AKM
 *
 * This function calls the hdd_sme_roam_callback with reason
 * eCSR_ROAM_SAE_COMPUTE to trigger SAE auth to supplicant.
 */
static QDF_STATUS
csr_process_roam_auth_sae_callback(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr roam_bssid,
				   uint32_t akm)
{
	struct csr_roam_info *roam_info;
	struct sir_sae_info sae_info;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (!session) {
		sme_err("WPA3 Preauth event with invalid session id:%d",
			vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_FAILURE;

	sae_info.msg_len = sizeof(sae_info);
	sae_info.vdev_id = vdev_id;
	sae_info.akm = akm;
	wlan_cm_get_roam_offload_ssid(mac_ctx->psoc, vdev_id,
				      sae_info.ssid.ssId,
				      &sae_info.ssid.length);
	qdf_mem_copy(sae_info.peer_mac_addr.bytes,
		     roam_bssid.bytes, QDF_MAC_ADDR_SIZE);

	roam_info->sae_info = &sae_info;

	csr_roam_call_callback(mac_ctx, vdev_id, roam_info,
			       eCSR_ROAM_SAE_COMPUTE, eCSR_ROAM_RESULT_NONE);

	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
csr_process_roam_auth_sae_callback(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr roam_bssid,
				   uint32_t akm)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

QDF_STATUS
csr_roam_auth_offload_callback(struct mac_context *mac_ctx, uint8_t vdev_id,
			       struct qdf_mac_addr bssid, uint32_t akm)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = csr_process_roam_auth_sae_callback(mac_ctx, vdev_id,
						    bssid, akm);

	sme_release_global_lock(&mac_ctx->sme);

	return status;

}
#endif

QDF_STATUS csr_update_owe_info(struct mac_context *mac,
			       struct assoc_ind *assoc_ind)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;

	status = csr_roam_get_session_id_from_bssid(mac,
					(struct qdf_mac_addr *)assoc_ind->bssId,
					&session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		return QDF_STATUS_E_FAILURE;
	}

	/* Send Association completion message to PE */
	if (assoc_ind->owe_status)
		status = QDF_STATUS_E_INVAL;
	status = csr_send_assoc_cnf_msg(mac, assoc_ind, status,
					assoc_ind->owe_status);
	/*
	 * send a message to CSR itself just to avoid the EAPOL frames
	 * going OTA before association response
	 */
	if (assoc_ind->owe_status == 0)
		status = csr_send_assoc_ind_to_upper_layer_cnf_msg(mac,
								   assoc_ind,
								   status,
								   session_id);

	return status;
}

QDF_STATUS csr_update_ft_info(struct mac_context *mac,
			      struct assoc_ind *assoc_ind)
{
	QDF_STATUS status;

	/* Send Association completion message to PE */
	status = assoc_ind->ft_status ? QDF_STATUS_E_INVAL : QDF_STATUS_SUCCESS;
	assoc_ind->need_assoc_rsp_tx_cb = true;
	status = csr_send_assoc_cnf_msg(mac, assoc_ind, status,
					assoc_ind->ft_status);
	return status;
}

/**
 * csr_set_sap_ser_params() - API to fill serialization parameters for
 * SAP requests
 * @cmd : Serialization command
 * @cmd_type: Type of serialization command
 *
 * Return: Void
 */
static void csr_set_sap_ser_params(struct wlan_serialization_command *cmd,
				   enum wlan_serialization_cmd_type cmd_type)
{
	cmd->cmd_type = cmd_type;
	cmd->source = WLAN_UMAC_COMP_MLME;
	cmd->cmd_cb = sme_sap_ser_callback;
	cmd->is_high_priority =  false;
	cmd->is_blocking = true;
	return;
}

QDF_STATUS csr_bss_start(struct mac_context *mac, uint32_t vdev_id,
			 struct start_bss_config *bss_config)
{
	struct wlan_serialization_command cmd = {0};
	struct wlan_objmgr_vdev *vdev;
	struct start_bss_config *start_bss_cfg = NULL;
	enum QDF_OPMODE persona;
	enum wlan_serialization_status status;
	struct csr_roam_session *session;
	struct validate_bss_data candidate;

	session = CSR_GET_SESSION(mac, vdev_id);
	if (!session)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac->pdev, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("VDEV not found for vdev id : %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	persona = wlan_vdev_mlme_get_opmode(vdev);
	if (persona != QDF_SAP_MODE && persona != QDF_NDI_MODE &&
	    persona != QDF_P2P_GO_MODE) {
		sme_err("Start BSS request for invalid mode %d", persona);
		goto error;
	}

	start_bss_cfg = qdf_mem_malloc(sizeof(struct start_bss_config));
	if (!start_bss_cfg) {
		sme_err("SAP BSS config allocation failed");
		goto error;
	}

	qdf_mem_copy(start_bss_cfg, bss_config,
		     sizeof(struct start_bss_config));
	start_bss_cfg->cmd_id = csr_get_monotonous_number(mac);

	session->cb_mode = start_bss_cfg->sec_ch_offset;
	session->bcn_int = bss_config->beaconInterval;
	candidate.beacon_interval = session->bcn_int;
	candidate.chan_freq = bss_config->oper_ch_freq;
	if_mgr_is_beacon_interval_valid(mac->pdev, vdev_id,
					&candidate);
	bss_config->beaconInterval = candidate.beacon_interval;
	session->bcn_int = candidate.beacon_interval;

	cmd.cmd_id = start_bss_cfg->cmd_id;
	csr_set_sap_ser_params(&cmd, WLAN_SER_CMD_VDEV_START_BSS);
	cmd.umac_cmd = start_bss_cfg;
	cmd.vdev = vdev;
	csr_fill_cmd_timeout(&cmd);

	status = wlan_vdev_mlme_ser_start_bss(&cmd);
	switch (status) {
	case WLAN_SER_CMD_PENDING:
	case WLAN_SER_CMD_ACTIVE:
		break;
	default:
		sme_err("ser cmd status %d", status);
		goto error;
	}

	return QDF_STATUS_SUCCESS;
error:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
	if (start_bss_cfg)
		qdf_mem_free(start_bss_cfg);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS csr_roam_issue_stop_bss_cmd(struct mac_context *mac,
				       uint8_t vdev_id,
				       eCsrRoamBssType bss_type)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_serialization_command cmd = {0};
	enum wlan_serialization_status status;
	struct stop_bss_req *stop_bss_req;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac->pdev, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("VDEV not found for vdev id : %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* Change the substate in case it is wait-for-key */
	if (CSR_IS_WAIT_FOR_KEY(mac, vdev_id)) {
		cm_stop_wait_for_key_timer(mac->psoc, vdev_id);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					 vdev_id);
	}

	sme_debug("Stop BSS vdev_id: %d bss_type %d", vdev_id, bss_type);
	stop_bss_req = qdf_mem_malloc(sizeof(*stop_bss_req));
	if (!stop_bss_req)
		goto error;

	stop_bss_req->vdev_id = vdev_id;
	stop_bss_req->cmd_id = csr_get_monotonous_number(mac);

	cmd.cmd_id = stop_bss_req->cmd_id;
	csr_set_sap_ser_params(&cmd, WLAN_SER_CMD_VDEV_STOP_BSS);
	cmd.umac_cmd = stop_bss_req;
	cmd.vdev = vdev;
	csr_fill_cmd_timeout(&cmd);

	status = wlan_vdev_mlme_ser_stop_bss(&cmd);
	switch (status) {
	case WLAN_SER_CMD_PENDING:
	case WLAN_SER_CMD_ACTIVE:
		break;
	default:
		sme_err("ser cmd status %d", status);
		goto error;
	}
	return QDF_STATUS_SUCCESS;

error:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
	qdf_mem_free(stop_bss_req);
	return QDF_STATUS_E_FAILURE;
}

static void csr_process_stop_bss_response(struct mac_context *mac_ctx,
					  uint32_t vdev_id)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac_ctx, vdev_id)) {
		csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
		csr_set_default_dot11_mode(mac_ctx);
	}

	csr_roam_call_callback(mac_ctx, vdev_id, NULL, eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_INFRA_STOPPED);
	return;
}

/**
 * csr_process_sap_results() - API to process the LIM response for
 * the messages posted by SAP module
 * @mac_ctx: mac context
 * @req: Serialization command posted by SAP
 * @rsp: Response from LIM
 * @result: Result from LIM
 * @vdev_id : vdev id
 *
 * Return: void
 */
static bool csr_process_sap_results(struct mac_context *mac_ctx,
				    void *rsp,
				    enum csr_sap_response_type result,
				    uint8_t vdev_id)
{
	struct csr_roam_info *roam_info;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);
	eRoamCmdStatus roam_status = eCSR_ROAM_INFRA_IND;
	eCsrRoamResult roam_result = eCSR_ROAM_RESULT_INFRA_START_FAILED;
	enum QDF_OPMODE opmode;

	if (!session) {
		sme_err("session %d not found ", vdev_id);
		return false;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return false;

	opmode = wlan_get_opmode_vdev_id(mac_ctx->pdev, vdev_id);
	sme_debug("SAP result : %d", result);

	switch (result) {
	case CSR_SAP_START_BSS_SUCCESS:
		csr_roam_process_start_bss_success(mac_ctx, roam_info,
						   rsp, vdev_id);
		break;
	case CSR_SAP_START_BSS_FAILURE:
		if (opmode == QDF_NDI_MODE) {
			csr_roam_update_ndp_return_params(mac_ctx,
							  CSR_SAP_START_BSS_FAILURE,
							  &roam_status,
							  &roam_result,
							  roam_info);
		}
		csr_roam_call_callback(mac_ctx, vdev_id, roam_info,
				       roam_status, roam_result);
		csr_set_default_dot11_mode(mac_ctx);
		break;
	case CSR_SAP_STOP_BSS_SUCCESS:
	case CSR_SAP_STOP_BSS_FAILURE:
		if (opmode == QDF_NDI_MODE) {
			qdf_mem_zero(roam_info, sizeof(*roam_info));
			csr_roam_update_ndp_return_params(mac_ctx, result,
							  &roam_status,
							  &roam_result,
							  roam_info);
			csr_roam_call_callback(mac_ctx, vdev_id, roam_info,
					       roam_status, roam_result);
		} else {
			csr_process_stop_bss_response(mac_ctx, vdev_id);
		}
		break;
	default:
		sme_err("Invalid response");
		break;
	}
	qdf_mem_free(roam_info);
	return true;
}

static enum wlan_serialization_cmd_type
get_cmd_type_from_result(enum csr_sap_response_type result)
{
	switch (result) {
	case CSR_SAP_START_BSS_SUCCESS:
	case CSR_SAP_START_BSS_FAILURE:
		return WLAN_SER_CMD_VDEV_START_BSS;
	case CSR_SAP_STOP_BSS_SUCCESS:
	case CSR_SAP_STOP_BSS_FAILURE:
		return WLAN_SER_CMD_VDEV_STOP_BSS;
	default:
		return WLAN_SER_CMD_MAX;
	}
}

static inline
uint32_t get_cmd_id_from_cmd_type(void *cmd,
				  enum wlan_serialization_cmd_type cmd_type)
{
	switch (cmd_type) {
	case WLAN_SER_CMD_VDEV_START_BSS:
		return ((struct start_bss_config *)cmd)->cmd_id;
	case WLAN_SER_CMD_VDEV_STOP_BSS:
		return ((struct stop_bss_req *)cmd)->cmd_id;
	default:
		sme_err("Invalid cmd_type %d to be dequeued", cmd_type);
		return 0;
	}
}

void csr_process_sap_response(struct mac_context *mac_ctx,
			      enum csr_sap_response_type result,
			      void *rsp, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	void *req;
	uint32_t cmd_id;
	enum wlan_serialization_cmd_type cmd_type =
				get_cmd_type_from_result(result);

	if (cmd_type >= WLAN_SER_CMD_MAX) {
		sme_err("Invalid command to be dequeued %d", cmd_type);
		return;
	}

	req = wlan_serialization_get_active_cmd(mac_ctx->psoc,
						vdev_id, cmd_type);
	if (!req) {
		sme_err("No active command for response from LIM for cmd: %d vdev: %d",
			cmd_type, vdev_id);
		csr_process_sap_results(mac_ctx, rsp, result, vdev_id);
		return;
	}

	csr_process_sap_results(mac_ctx, rsp, result, vdev_id);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("vdev not found for vdev id: %d", vdev_id);
		return;
	}

	cmd_id = get_cmd_id_from_cmd_type(req, cmd_type);
	sme_debug("Dequeue cmd id : %d type : %d", cmd_id, cmd_type);

	wlan_vdev_mlme_ser_remove_request(vdev, cmd_id, cmd_type);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}
