/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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
 * ===========================================================================
 *                     sapModule.C
 *  OVERVIEW:
 *  This software unit holds the implementation of the WLAN SAP modules
 *  functions providing EXTERNAL APIs. It is also where the global SAP module
 *  context gets initialised
 *  DEPENDENCIES:
 *  Are listed for each API below.
 * ===========================================================================
 */

/* $Header$ */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "qdf_trace.h"
#include "qdf_util.h"
#include "qdf_atomic.h"
/* Pick up the sme callback registration API */
#include "sme_api.h"

/* SAP API header file */

#include "sap_internal.h"
#include "sme_inside.h"
#include "cds_ieee80211_common_i.h"
#include "cds_regdomain.h"
#include "wlan_policy_mgr_api.h"
#include <wlan_scan_ucfg_api.h>
#include "wlan_reg_services_api.h"
#include <wlan_dfs_utils_api.h>
#include <wlan_reg_ucfg_api.h>
#include "sap_ch_select.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define SAP_DEBUG

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  External declarations for global context
 * -------------------------------------------------------------------------*/
/*  No!  Get this from CDS. */
/*  The main per-Physical Link (per WLAN association) context. */
static struct sap_context *gp_sap_ctx[SAP_MAX_NUM_SESSION];
static qdf_atomic_t sap_ctx_ref_count[SAP_MAX_NUM_SESSION];

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/
static qdf_mutex_t sap_context_lock;

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/**
 * wlansap_global_init() - Initialize SAP globals
 *
 * Initializes the SAP global data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_global_init(void)
{
	uint32_t i;

	if (QDF_IS_STATUS_ERROR(qdf_mutex_create(&sap_context_lock))) {
		sap_err("failed to init sap_context_lock");
		return QDF_STATUS_E_FAULT;
	}

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		gp_sap_ctx[i] = NULL;
		qdf_atomic_init(&sap_ctx_ref_count[i]);
	}

	sap_debug("sap global context initialized");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_global_deinit() - De-initialize SAP globals
 *
 * De-initializes the SAP global data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_global_deinit(void)
{
	uint32_t i;

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (gp_sap_ctx[i]) {
			sap_err("we could be leaking context:%d", i);
		}
		gp_sap_ctx[i] = NULL;
		qdf_atomic_init(&sap_ctx_ref_count[i]);
	}

	if (QDF_IS_STATUS_ERROR(qdf_mutex_destroy(&sap_context_lock))) {
		sap_err("failed to destroy sap_context_lock");
		return QDF_STATUS_E_FAULT;
	}

	sap_debug("sap global context deinitialized");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_save_context() - Save the context in global SAP context
 * @ctx: SAP context to be stored
 *
 * Stores the given SAP context in the global SAP context array
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlansap_save_context(struct sap_context *ctx)
{
	uint32_t i;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (gp_sap_ctx[i] == NULL) {
			gp_sap_ctx[i] = ctx;
			qdf_atomic_inc(&sap_ctx_ref_count[i]);
			qdf_mutex_release(&sap_context_lock);
			sap_debug("sap context saved at index: %d", i);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&sap_context_lock);

	sap_err("failed to save sap context");

	return QDF_STATUS_E_FAILURE;
}

/**
 * wlansap_context_get() - Verify SAP context and increment ref count
 * @ctx: Context to be checked
 *
 * Verifies the SAP context and increments the reference count maintained for
 * the corresponding SAP context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_context_get(struct sap_context *ctx)
{
	uint32_t i;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (ctx && (gp_sap_ctx[i] == ctx)) {
			qdf_atomic_inc(&sap_ctx_ref_count[i]);
			qdf_mutex_release(&sap_context_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&sap_context_lock);

	sap_debug("sap session is not valid");
	return QDF_STATUS_E_FAILURE;
}

/**
 * wlansap_context_put() - Check the reference count and free SAP context
 * @ctx: SAP context to be checked and freed
 *
 * Checks the reference count and frees the SAP context
 *
 * Return: None
 */
void wlansap_context_put(struct sap_context *ctx)
{
	uint32_t i;

	if (!ctx)
		return;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (gp_sap_ctx[i] == ctx) {
			if (qdf_atomic_dec_and_test(&sap_ctx_ref_count[i])) {
				if (ctx->channelList) {
					qdf_mem_free(ctx->channelList);
					ctx->channelList = NULL;
					ctx->num_of_channel = 0;
				}
				qdf_mem_free(ctx);
				gp_sap_ctx[i] = NULL;
				sap_debug("sap session freed: %d", i);
			}
			qdf_mutex_release(&sap_context_lock);
			return;
		}
	}
	qdf_mutex_release(&sap_context_lock);
}

struct sap_context *sap_create_ctx(void)
{
	struct sap_context *sap_ctx;
	QDF_STATUS status;

	/* dynamically allocate the sapContext */
	sap_ctx = qdf_mem_malloc(sizeof(*sap_ctx));

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer from p_cds_gctx");
		return NULL;
	}

	/* Clean up SAP control block, initialize all values */

	/* Save the SAP context pointer */
	status = wlansap_save_context(sap_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to save SAP context");
		qdf_mem_free(sap_ctx);
		return NULL;
	}
	sap_debug("Exit");

	return sap_ctx;
} /* sap_create_ctx */

static QDF_STATUS wlansap_owe_init(struct sap_context *sap_ctx)
{
	qdf_list_create(&sap_ctx->owe_pending_assoc_ind_list, 0);

	return QDF_STATUS_SUCCESS;
}

static void wlansap_owe_cleanup(struct sap_context *sap_ctx)
{
	tHalHandle hal;
	tpAniSirGlobal mac;
	struct owe_assoc_ind *owe_assoc_ind;
	tSirSmeAssocInd *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status;

	if (!sap_ctx) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "Invalid SAP context");
		return;
	}

	hal = CDS_GET_HAL_CB();
	mac = (tpAniSirGlobal)hal;
	if (!mac) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "Invalid MAC context");
		return;
	}

	if (QDF_STATUS_SUCCESS !=
	    qdf_list_peek_front(&sap_ctx->owe_pending_assoc_ind_list,
				&node)) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
				"Failed to find assoc ind list");
		return;
	}

	while (node) {
		qdf_list_peek_next(&sap_ctx->owe_pending_assoc_ind_list,
				   node, &next_node);
		owe_assoc_ind = qdf_container_of(node, struct owe_assoc_ind,
						 node);
		status = qdf_list_remove_node(
					   &sap_ctx->owe_pending_assoc_ind_list,
					   node);
		if (status == QDF_STATUS_SUCCESS) {
			assoc_ind = owe_assoc_ind->assoc_ind;
			qdf_mem_free(owe_assoc_ind);
			assoc_ind->owe_ie = NULL;
			assoc_ind->owe_ie_len = 0;
			assoc_ind->owe_status = eSIR_MAC_UNSPEC_FAILURE_STATUS;
			status = sme_update_owe_info(mac, assoc_ind);
			qdf_mem_free(assoc_ind);
		} else {
			QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
					"Failed to remove assoc ind");
		}
		node = next_node;
		next_node = NULL;
	}
}

static void wlansap_owe_deinit(struct sap_context *sap_ctx)
{
	qdf_list_destroy(&sap_ctx->owe_pending_assoc_ind_list);
}

QDF_STATUS sap_init_ctx(struct sap_context *sap_ctx,
			 enum QDF_OPMODE mode,
			 uint8_t *addr, uint32_t session_id, bool reinit)
{
	QDF_STATUS qdf_ret_status;
	tHalHandle hal;
	tpAniSirGlobal pmac;

	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
		  "wlansap_start invoked successfully");

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	/*------------------------------------------------------------------------
	    For now, presume security is not enabled.
	   -----------------------------------------------------------------------*/
	sap_ctx->ucSecEnabled = WLANSAP_SECURITY_ENABLED_STATE;

	/*------------------------------------------------------------------------
	    Now configure the roaming profile links. To SSID and bssid.
	   ------------------------------------------------------------------------*/
	/* We have room for two SSIDs. */
	sap_ctx->csr_roamProfile.SSIDs.numOfSSIDs = 1;   /* This is true for now. */
	sap_ctx->csr_roamProfile.SSIDs.SSIDList = sap_ctx->SSIDList;     /* Array of two */
	sap_ctx->csr_roamProfile.SSIDs.SSIDList[0].SSID.length = 0;
	sap_ctx->csr_roamProfile.SSIDs.SSIDList[0].handoffPermitted = false;
	sap_ctx->csr_roamProfile.SSIDs.SSIDList[0].ssidHidden =
		sap_ctx->SSIDList[0].ssidHidden;

	sap_ctx->csr_roamProfile.BSSIDs.numOfBSSIDs = 1; /* This is true for now. */
	sap_ctx->csa_reason = CSA_REASON_UNKNOWN;
	sap_ctx->csr_roamProfile.BSSIDs.bssid = &sap_ctx->bssid;
	sap_ctx->csr_roamProfile.csrPersona = mode;
	qdf_mem_copy(sap_ctx->self_mac_addr, addr, QDF_MAC_ADDR_SIZE);

	/* Now configure the auth type in the roaming profile. To open. */
	sap_ctx->csr_roamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM;        /* open is the default */

	hal = (tHalHandle) CDS_GET_HAL_CB();
	if (!hal) {
		sap_err("Invalid HAL pointer");
		return QDF_STATUS_E_INVAL;
	}
	pmac = PMAC_STRUCT(hal);
	qdf_ret_status = sap_set_session_param(hal, sap_ctx, session_id);
	if (QDF_STATUS_SUCCESS != qdf_ret_status) {
		sap_err("Calling sap_set_session_param status = %d",
			qdf_ret_status);
		return QDF_STATUS_E_FAILURE;
	}
	if (sap_ctx->acs_ch_list_protect) {
		qdf_mutex_destroy(sap_ctx->acs_ch_list_protect);
		qdf_mem_free(sap_ctx->acs_ch_list_protect);
		sap_ctx->acs_ch_list_protect = NULL;
	}
	sap_ctx->acs_ch_list_protect =
			qdf_mem_malloc(sizeof(*sap_ctx->acs_ch_list_protect));
	if (sap_ctx->acs_ch_list_protect) {
		qdf_ret_status = qdf_mutex_create(sap_ctx->acs_ch_list_protect);
		if (QDF_IS_STATUS_ERROR(qdf_ret_status)) {
			qdf_mem_free(sap_ctx->acs_ch_list_protect);
			sap_ctx->acs_ch_list_protect = NULL;
		}
	}
	/* Register with scan component only during init */
	if (!reinit)
		sap_ctx->req_id =
			ucfg_scan_register_requester(pmac->psoc, "SAP",
					sap_scan_event_callback, sap_ctx);

	qdf_ret_status = wlansap_owe_init(sap_ctx);
	if (QDF_STATUS_SUCCESS != qdf_ret_status) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
				"OWE init failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_deinit_ctx(struct sap_context *sap_ctx)
{
	tHalHandle hal;
	tpAniSirGlobal pmac;

	/* Sanity check - Extract SAP control block */
	sap_debug("wlansap_stop invoked successfully ");

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	wlansap_owe_cleanup(sap_ctx);
	wlansap_owe_deinit(sap_ctx);

	hal = CDS_GET_HAL_CB();
	pmac = (tpAniSirGlobal) hal;
	if (NULL == pmac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	ucfg_scan_unregister_requester(pmac->psoc, sap_ctx->req_id);

	if (sap_ctx->channelList) {
		qdf_mem_free(sap_ctx->channelList);
		sap_ctx->channelList = NULL;
		sap_ctx->num_of_channel = 0;
	}
	qdf_mem_free(sap_ctx->acs_ch_list_protect);
	sap_ctx->acs_ch_list_protect = NULL;
	sap_free_roam_profile(&sap_ctx->csr_roamProfile);
	if (sap_ctx->sessionId != CSR_SESSION_ID_INVALID) {
		/* empty queues/lists/pkts if any */
		sap_clear_session_param(hal, sap_ctx, sap_ctx->sessionId);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_destroy_ctx(struct sap_context *sap_ctx)
{
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
		  "sap_destroy_ctx invoked");

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	/* Cleanup SAP control block */
	/*
	 * wlansap_context_put will release actual sap_ctx memory
	 * allocated during sap_create_ctx
	 */
	wlansap_context_put(sap_ctx);

	return QDF_STATUS_SUCCESS;
} /* sap_destroy_ctx */

bool wlansap_is_channel_in_nol_list(struct sap_context *sap_ctx,
				    uint8_t channelNumber,
				    ePhyChanBondState chanBondState)
{
	if (!sap_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: Invalid SAP pointer from pCtx", __func__);
		return QDF_STATUS_E_FAULT;
	}

	return sap_dfs_is_channel_in_nol_list(sap_ctx, channelNumber,
					      chanBondState);
}

static QDF_STATUS wlansap_mark_leaking_channel(struct wlan_objmgr_pdev *pdev,
		uint8_t *leakage_adjusted_lst,
		uint8_t chan_bw)
{

	return utils_dfs_mark_leaking_ch(pdev, chan_bw, 1,
			leakage_adjusted_lst);
}

bool wlansap_is_channel_leaking_in_nol(struct sap_context *sap_ctx,
				       uint8_t channel,
				       uint8_t chan_bw)
{
	tpAniSirGlobal mac_ctx;
	uint8_t leakage_adjusted_lst[1];
	void *handle = NULL;

	leakage_adjusted_lst[0] = channel;
	handle = CDS_GET_HAL_CB();
	mac_ctx = PMAC_STRUCT(handle);
	if (!mac_ctx) {
		sap_err("Invalid mac pointer");
		return QDF_STATUS_E_FAULT;
	}
	if (QDF_IS_STATUS_ERROR(wlansap_mark_leaking_channel(mac_ctx->pdev,
			leakage_adjusted_lst, chan_bw)))
		return true;

	if (!leakage_adjusted_lst[0])
		return true;

	return false;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
uint16_t wlansap_check_cc_intf(struct sap_context *sap_ctx)
{
	tHalHandle hHal;
	uint16_t intf_ch;

	hHal = (tHalHandle) CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("Invalid MAC context from p_cds_gctx");
		return 0;
	}
	intf_ch = sme_check_concurrent_channel_overlap(hHal, sap_ctx->channel,
					     sap_ctx->csr_roamProfile.phyMode,
						       sap_ctx->cc_switch_mode);
	return intf_ch;
}
#endif

 /**
  * wlansap_set_scan_acs_channel_params() - Config scan and channel parameters.
  * pconfig:                                Pointer to the SAP config
  * psap_ctx:                               Pointer to the SAP Context.
  * pusr_context:                           Parameter that will be passed
  *                                         back in all the SAP callback events.
  *
  * This api function is used to copy Scan and Channel parameters from sap
  * config to sap context.
  *
  * Return:                                 The result code associated with
  *                                         performing the operation
  */
static QDF_STATUS
wlansap_set_scan_acs_channel_params(tsap_config_t *pconfig,
				struct sap_context *psap_ctx,
				void *pusr_context)
{
	tHalHandle h_hal = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (NULL == pconfig) {
		sap_err("Invalid pconfig passed ");
		return QDF_STATUS_E_FAULT;
	}

	if (NULL == psap_ctx) {
		sap_err("Invalid pconfig passed ");
		return QDF_STATUS_E_FAULT;
	}

	/* Channel selection is auto or configured */
	psap_ctx->channel = pconfig->channel;
	psap_ctx->dfs_mode = pconfig->acs_dfs_mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	psap_ctx->cc_switch_mode = pconfig->cc_switch_mode;
#endif
	psap_ctx->auto_channel_select_weight =
		 pconfig->auto_channel_select_weight;
	psap_ctx->pUsrContext = pusr_context;
	psap_ctx->enableOverLapCh = pconfig->enOverLapCh;
	psap_ctx->acs_cfg = &pconfig->acs_cfg;
	psap_ctx->ch_width_orig = pconfig->acs_cfg.ch_width;
	psap_ctx->secondary_ch = pconfig->sec_ch;

	/*
	 * Set the BSSID to your "self MAC Addr" read
	 * the mac address from Configuation ITEM received
	 * from HDD
	 */
	psap_ctx->csr_roamProfile.BSSIDs.numOfBSSIDs = 1;

	/* Save a copy to SAP context */
	qdf_mem_copy(psap_ctx->csr_roamProfile.BSSIDs.bssid,
		pconfig->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(psap_ctx->self_mac_addr,
		pconfig->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);

	h_hal = (tHalHandle)CDS_GET_HAL_CB();
	if (NULL == h_hal) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			"%s: Invalid MAC context from pvosGCtx", __func__);
		return QDF_STATUS_E_FAULT;
	}

	return status;
}

/**
 * wlan_sap_get_roam_profile() - Returns sap roam profile.
 * @sap_ctx:	Pointer to Sap Context.
 *
 * This function provides the SAP roam profile.
 *
 * Return: SAP RoamProfile
 */
struct csr_roam_profile *wlan_sap_get_roam_profile(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from ctx");
		return NULL;
	}
	return &sap_ctx->csr_roamProfile;
}

eCsrPhyMode wlan_sap_get_phymode(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from ctx");
		return 0;
	}
	return sap_ctx->csr_roamProfile.phyMode;
}

uint32_t wlan_sap_get_vht_ch_width(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return 0;
	}

	return sap_ctx->ch_params.ch_width;
}

void wlan_sap_set_vht_ch_width(struct sap_context *sap_ctx,
			       uint32_t vht_channel_width)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return;
	}

	sap_ctx->ch_params.ch_width = vht_channel_width;
}

/**
 * wlan_sap_validate_channel_switch() - validate target channel switch w.r.t
 *      concurreny rules set to avoid channel interference.
 * @hal - Hal context
 * @sap_ch - channel to switch
 * @sap_context - sap session context
 *
 * Return: true if there is no channel interference else return false
 */
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
static bool wlan_sap_validate_channel_switch(tHalHandle hal, uint16_t sap_ch,
		struct sap_context *sap_context)
{
	return sme_validate_sap_channel_switch(
			hal,
			sap_ch,
			sap_context->csr_roamProfile.phyMode,
			sap_context->cc_switch_mode,
			sap_context->sessionId);
}
#else
static inline bool wlan_sap_validate_channel_switch(tHalHandle hal,
		uint16_t sap_ch, struct sap_context *sap_context)
{
	return true;
}
#endif

void wlan_sap_set_sap_ctx_acs_cfg(struct sap_context *sap_ctx,
				  tsap_config_t *sap_config)
{
	if (!sap_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: Invalid SAP pointer",
			  __func__);
		return;
	}

	sap_ctx->acs_cfg = &sap_config->acs_cfg;
}

QDF_STATUS wlansap_start_bss(struct sap_context *sap_ctx,
			     tpWLAN_SAPEventCB pSapEventCallback,
			     tsap_config_t *pConfig, void *pUsrContext)
{
	tWLAN_SAPEvent sapEvent;        /* State machine event */
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	tHalHandle hHal;
	tpAniSirGlobal pmac = NULL;

	if (!sap_ctx) {
		sap_info("Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->fsm_state = SAP_INIT;

	/* Channel selection is auto or configured */
	sap_ctx->channel = pConfig->channel;
	sap_ctx->dfs_mode = pConfig->acs_dfs_mode;
	sap_ctx->ch_params.ch_width = pConfig->ch_params.ch_width;
	sap_ctx->ch_params.center_freq_seg0 =
		pConfig->ch_params.center_freq_seg0;
	sap_ctx->ch_params.center_freq_seg1 =
		pConfig->ch_params.center_freq_seg1;
	sap_ctx->ch_params.sec_ch_offset =
		pConfig->ch_params.sec_ch_offset;
	sap_ctx->ch_width_orig = pConfig->ch_width_orig;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	sap_ctx->cc_switch_mode = pConfig->cc_switch_mode;
#endif
	sap_ctx->auto_channel_select_weight =
		 pConfig->auto_channel_select_weight;
	sap_ctx->pUsrContext = pUsrContext;
	sap_ctx->enableOverLapCh = pConfig->enOverLapCh;
	sap_ctx->acs_cfg = &pConfig->acs_cfg;
	sap_ctx->secondary_ch = pConfig->sec_ch;
	sap_ctx->dfs_cac_offload = pConfig->dfs_cac_offload;
	sap_ctx->isCacEndNotified = false;
	sap_ctx->is_chan_change_inprogress = false;
	sap_ctx->stop_bss_in_progress = false;
	/* Set the BSSID to your "self MAC Addr" read the mac address
		from Configuation ITEM received from HDD */
	sap_ctx->csr_roamProfile.BSSIDs.numOfBSSIDs = 1;
	qdf_mem_copy(sap_ctx->csr_roamProfile.BSSIDs.bssid,
		     sap_ctx->self_mac_addr, sizeof(struct qdf_mac_addr));

	/* Save a copy to SAP context */
	qdf_mem_copy(sap_ctx->csr_roamProfile.BSSIDs.bssid,
		     pConfig->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(sap_ctx->self_mac_addr,
		     pConfig->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);

	/* copy the configuration items to csrProfile */
	sapconvert_to_csr_profile(pConfig, eCSR_BSS_TYPE_INFRA_AP,
			       &sap_ctx->csr_roamProfile);
	hHal = (tHalHandle) CDS_GET_HAL_CB();
	if (NULL == hHal) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: Invalid MAC context from p_cds_gctx",
			  __func__);
		qdf_status = QDF_STATUS_E_FAULT;
		goto fail;
	}
	pmac = PMAC_STRUCT(hHal);
	if (NULL == pmac) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: Invalid MAC context from p_cds_gctx",
			  __func__);
		qdf_status = QDF_STATUS_E_FAULT;
		goto fail;
	}

	/*
	 * Copy the DFS Test Mode setting to pmac for
	 * access in lower layers
	 */
	pmac->sap.SapDfsInfo.disable_dfs_ch_switch =
				pConfig->disableDFSChSwitch;
	pmac->sap.SapDfsInfo.sap_ch_switch_beacon_cnt =
				pConfig->sap_chanswitch_beacon_cnt;
	pmac->sap.SapDfsInfo.sap_ch_switch_mode =
			pConfig->sap_chanswitch_mode;

	pmac->sap.sapCtxList[sap_ctx->sessionId].sap_context = sap_ctx;
	pmac->sap.sapCtxList[sap_ctx->sessionId].sapPersona =
		sap_ctx->csr_roamProfile.csrPersona;
	pmac->sap.sapCtxList[sap_ctx->sessionId].sessionID =
		sap_ctx->sessionId;
	pmac->sap.SapDfsInfo.dfs_beacon_tx_enhanced =
		pConfig->dfs_beacon_tx_enhanced;
	pmac->sap.SapDfsInfo.reduced_beacon_interval =
				pConfig->reduced_beacon_interval;
	sap_debug("SAP: auth ch select weight:%d chswitch bcn cnt:%d chswitch mode:%d reduced bcn intv:%d",
		  sap_ctx->auto_channel_select_weight,
		  pConfig->sap_chanswitch_beacon_cnt,
		  pmac->sap.SapDfsInfo.sap_ch_switch_mode,
		  pmac->sap.SapDfsInfo.reduced_beacon_interval);

	/* Copy MAC filtering settings to sap context */
	sap_ctx->eSapMacAddrAclMode = pConfig->SapMacaddr_acl;
	qdf_mem_copy(sap_ctx->acceptMacList, pConfig->accept_mac,
		     sizeof(pConfig->accept_mac));
	sap_ctx->nAcceptMac = pConfig->num_accept_mac;
	sap_sort_mac_list(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	qdf_mem_copy(sap_ctx->denyMacList, pConfig->deny_mac,
		     sizeof(pConfig->deny_mac));
	sap_ctx->nDenyMac = pConfig->num_deny_mac;
	sap_sort_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	sap_ctx->beacon_tx_rate = pConfig->beacon_tx_rate;

	/* Fill in the event structure for FSM */
	sapEvent.event = eSAP_HDD_START_INFRA_BSS;
	sapEvent.params = 0;    /* pSapPhysLinkCreate */

	/* Store the HDD callback in SAP context */
	sap_ctx->pfnSapEventCallback = pSapEventCallback;

	/* Handle event */
	qdf_status = sap_fsm(sap_ctx, &sapEvent);
fail:
	if (QDF_IS_STATUS_ERROR(qdf_status))
		sap_free_roam_profile(&sap_ctx->csr_roamProfile);

	return qdf_status;
} /* wlansap_start_bss */

QDF_STATUS wlansap_set_mac_acl(struct sap_context *sap_ctx,
			       tsap_config_t *pConfig)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
		  "wlansap_set_mac_acl");

	if (NULL == sap_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: Invalid SAP pointer", __func__);
		return QDF_STATUS_E_FAULT;
	}
	/* Copy MAC filtering settings to sap context */
	sap_ctx->eSapMacAddrAclMode = pConfig->SapMacaddr_acl;

	if (eSAP_DENY_UNLESS_ACCEPTED == sap_ctx->eSapMacAddrAclMode) {
		qdf_mem_copy(sap_ctx->acceptMacList,
			     pConfig->accept_mac,
			     sizeof(pConfig->accept_mac));
		sap_ctx->nAcceptMac = pConfig->num_accept_mac;
		sap_sort_mac_list(sap_ctx->acceptMacList,
			       sap_ctx->nAcceptMac);
	} else if (eSAP_ACCEPT_UNLESS_DENIED == sap_ctx->eSapMacAddrAclMode) {
		qdf_mem_copy(sap_ctx->denyMacList, pConfig->deny_mac,
			     sizeof(pConfig->deny_mac));
		sap_ctx->nDenyMac = pConfig->num_deny_mac;
		sap_sort_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	}

	return qdf_status;
} /* wlansap_set_mac_acl */

void wlansap_set_stop_bss_inprogress(struct sap_context *sap_ctx,
					bool in_progress)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from ctx");
		return;
	}

	sap_debug("Set stop_bss_in_progress to %d", in_progress);
	sap_ctx->stop_bss_in_progress = in_progress;
}

QDF_STATUS wlansap_stop_bss(struct sap_context *sap_ctx)
{
	tWLAN_SAPEvent sapEvent;        /* State machine event */
	QDF_STATUS qdf_status;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	/* Fill in the event structure for FSM */
	sapEvent.event = eSAP_HDD_STOP_INFRA_BSS;
	sapEvent.params = 0;

	/* Handle event */
	qdf_status = sap_fsm(sap_ctx, &sapEvent);

	return qdf_status;
}

/* This routine will set the mode of operation for ACL dynamically*/
QDF_STATUS wlansap_set_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL mode)
{
	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->eSapMacAddrAclMode = mode;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL *mode)
{
	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	*mode = sap_ctx->eSapMacAddrAclMode;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_accept_list(struct sap_context *sap_ctx,
				       struct qdf_mac_addr *pAcceptList,
				       uint8_t *nAcceptList)
{
	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	memcpy(pAcceptList, sap_ctx->acceptMacList,
	       (sap_ctx->nAcceptMac * QDF_MAC_ADDR_SIZE));
	*nAcceptList = sap_ctx->nAcceptMac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_deny_list(struct sap_context *sap_ctx,
				     struct qdf_mac_addr *pDenyList,
				     uint8_t *nDenyList)
{
	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer from p_cds_gctx");
		return QDF_STATUS_E_FAULT;
	}

	memcpy(pDenyList, sap_ctx->denyMacList,
	       (sap_ctx->nDenyMac * QDF_MAC_ADDR_SIZE));
	*nDenyList = sap_ctx->nDenyMac;
	return QDF_STATUS_SUCCESS;
}

void sap_undo_acs(struct sap_context *sap_ctx, struct sap_config *sap_cfg)
{
	struct sap_acs_cfg *acs_cfg;

	if (!sap_ctx)
		return;

	acs_cfg = &sap_cfg->acs_cfg;
	if (!acs_cfg)
		return;

	if (sap_ctx->acs_ch_list_protect)
		qdf_mutex_acquire(sap_ctx->acs_ch_list_protect);

	if (acs_cfg->ch_list) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "Clearing ACS cfg ch list");
		qdf_mem_free(acs_cfg->ch_list);
		acs_cfg->ch_list = NULL;
	}
	if (acs_cfg->master_ch_list) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "Clearing ACS cfg master ch list");
		qdf_mem_free(acs_cfg->master_ch_list);
		acs_cfg->master_ch_list = NULL;
	}
	if (sap_ctx->channelList) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			  "Clearing sap ctx acs ch list");
		qdf_mem_free(sap_ctx->channelList);
		sap_ctx->channelList = NULL;
	}
	acs_cfg->ch_list_count = 0;
	acs_cfg->master_ch_list_count = 0;
	acs_cfg->acs_mode = false;
	sap_ctx->num_of_channel = 0;

	if (sap_ctx->acs_ch_list_protect)
		qdf_mutex_release(sap_ctx->acs_ch_list_protect);
}

QDF_STATUS wlansap_clear_acl(struct sap_context *sap_ctx)
{
	uint8_t i;

	if (NULL == sap_ctx) {
		return QDF_STATUS_E_RESOURCES;
	}

	for (i = 0; i < (sap_ctx->nDenyMac - 1); i++) {
		qdf_mem_zero((sap_ctx->denyMacList + i)->bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	sap_ctx->nDenyMac = 0;

	for (i = 0; i < (sap_ctx->nAcceptMac - 1); i++) {
		qdf_mem_zero((sap_ctx->acceptMacList + i)->bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	sap_ctx->nAcceptMac = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_modify_acl(struct sap_context *sap_ctx,
			      uint8_t *peer_sta_mac,
			      eSapACLType list_type, eSapACLCmdType cmd)
{
	bool sta_white_list = false, sta_black_list = false;
	uint8_t staWLIndex, staBLIndex;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP Context");
		return QDF_STATUS_E_FAULT;
	}
	if (qdf_mem_cmp(sap_ctx->bssid.bytes, peer_sta_mac,
			QDF_MAC_ADDR_SIZE) == 0) {
			sap_err("requested peer mac is" MAC_ADDRESS_STR
				"our own SAP BSSID. Do not blacklist or whitelist this BSSID",
				MAC_ADDR_ARRAY(peer_sta_mac));
		return QDF_STATUS_E_FAULT;
	}
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_LOW,
		  "Modify ACL entered\n" "Before modification of ACL\n"
		  "size of accept and deny lists %d %d", sap_ctx->nAcceptMac,
		  sap_ctx->nDenyMac);
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
		  "*** WHITE LIST ***");
	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
		  "*** BLACK LIST ***");
	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);

	/* the expectation is a mac addr will not be in both the lists
	 * at the same time. It is the responsiblity of userspace to
	 * ensure this
	 */
	sta_white_list =
		sap_search_mac_list(sap_ctx->acceptMacList, sap_ctx->nAcceptMac,
				 peer_sta_mac, &staWLIndex);
	sta_black_list =
		sap_search_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac,
				 peer_sta_mac, &staBLIndex);

	if (sta_white_list && sta_black_list) {
		sap_err("Peer mac " MAC_ADDRESS_STR
			" found in white and black lists."
			"Initial lists passed incorrect. Cannot execute this command.",
			MAC_ADDR_ARRAY(peer_sta_mac));
		return QDF_STATUS_E_FAILURE;

	}
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_LOW,
		  "cmd %d", cmd);

	switch (list_type) {
	case eSAP_WHITE_LIST:
		if (cmd == ADD_STA_TO_ACL) {
			/* error check */
			/* if list is already at max, return failure */
			if (sap_ctx->nAcceptMac == MAX_ACL_MAC_ADDRESS) {
				sap_err("White list is already maxed out. Cannot accept "
					MAC_ADDRESS_STR,
					MAC_ADDR_ARRAY(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
			if (sta_white_list) {
				/* Do nothing if already present in white list. Just print a warning */
				sap_warn("MAC address already present in white list "
					 MAC_ADDRESS_STR,
					 MAC_ADDR_ARRAY(peer_sta_mac));
				return QDF_STATUS_SUCCESS;
			}
			if (sta_black_list) {
				/* remove it from black list before adding to the white list */
				sap_warn("STA present in black list so first remove from it");
				sap_remove_mac_from_acl(sap_ctx->denyMacList,
						    &sap_ctx->nDenyMac,
						    staBLIndex);
			}
			sap_info("... Now add to the white list");
			sap_add_mac_to_acl(sap_ctx->acceptMacList,
					       &sap_ctx->nAcceptMac,
			       peer_sta_mac);
				QDF_TRACE(QDF_MODULE_ID_SAP,
				  QDF_TRACE_LEVEL_INFO_LOW,
				  "size of accept and deny lists %d %d",
				  sap_ctx->nAcceptMac,
				  sap_ctx->nDenyMac);
		} else if (cmd == DELETE_STA_FROM_ACL) {
			if (sta_white_list) {

				struct csr_del_sta_params delStaParams;

				sap_info("Delete from white list");
				sap_remove_mac_from_acl(sap_ctx->acceptMacList,
						    &sap_ctx->nAcceptMac,
						    staWLIndex);
				/* If a client is deleted from white list and it is connected, send deauth */
				wlansap_populate_del_sta_params(peer_sta_mac,
					eCsrForcedDeauthSta,
					(SIR_MAC_MGMT_DEAUTH >> 4),
					&delStaParams);
				wlansap_deauth_sta(sap_ctx, &delStaParams);
				QDF_TRACE(QDF_MODULE_ID_SAP,
					  QDF_TRACE_LEVEL_INFO_LOW,
					  "size of accept and deny lists %d %d",
					  sap_ctx->nAcceptMac,
					  sap_ctx->nDenyMac);
			} else {
				sap_warn("MAC address to be deleted is not present in the white list "
					 MAC_ADDRESS_STR,
					 MAC_ADDR_ARRAY(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			sap_err("Invalid cmd type passed");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	case eSAP_BLACK_LIST:

		if (cmd == ADD_STA_TO_ACL) {
			struct csr_del_sta_params delStaParams;
			/* error check */
			/* if list is already at max, return failure */
			if (sap_ctx->nDenyMac == MAX_ACL_MAC_ADDRESS) {
				sap_err("Black list is already maxed out. Cannot accept "
					MAC_ADDRESS_STR,
					MAC_ADDR_ARRAY(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
			if (sta_black_list) {
				/* Do nothing if already present in white list */
				sap_warn("MAC address already present in black list "
					 MAC_ADDRESS_STR,
					 MAC_ADDR_ARRAY(peer_sta_mac));
				return QDF_STATUS_SUCCESS;
			}
			if (sta_white_list) {
				/* remove it from white list before adding to the black list */
				sap_warn("Present in white list so first remove from it");
				sap_remove_mac_from_acl(sap_ctx->acceptMacList,
						    &sap_ctx->nAcceptMac,
						    staWLIndex);
			}
			/* If we are adding a client to the black list; if its connected, send deauth */
			wlansap_populate_del_sta_params(peer_sta_mac,
				eCsrForcedDeauthSta,
				(SIR_MAC_MGMT_DEAUTH >> 4),
				&delStaParams);
			wlansap_deauth_sta(sap_ctx, &delStaParams);
			sap_info("... Now add to black list");
			sap_add_mac_to_acl(sap_ctx->denyMacList,
				       &sap_ctx->nDenyMac, peer_sta_mac);
			QDF_TRACE(QDF_MODULE_ID_SAP,
				  QDF_TRACE_LEVEL_INFO_LOW,
				  "size of accept and deny lists %d %d",
				  sap_ctx->nAcceptMac,
				  sap_ctx->nDenyMac);
		} else if (cmd == DELETE_STA_FROM_ACL) {
			if (sta_black_list) {
				sap_info("Delete from black list");
				sap_remove_mac_from_acl(sap_ctx->denyMacList,
						    &sap_ctx->nDenyMac,
						    staBLIndex);
				QDF_TRACE(QDF_MODULE_ID_SAP,
					  QDF_TRACE_LEVEL_INFO_LOW,
					  "no accept and deny mac %d %d",
					  sap_ctx->nAcceptMac,
					  sap_ctx->nDenyMac);
			} else {
				sap_warn("MAC address to be deleted is not present in the black list "
					 MAC_ADDRESS_STR,
					 MAC_ADDR_ARRAY(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			sap_err("Invalid cmd type passed");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	default:
	{
		sap_err("Invalid list type passed %d", list_type);
		return QDF_STATUS_E_FAILURE;
	}
	}
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_LOW,
		  "After modification of ACL");
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
		  "*** WHITE LIST ***");
	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
		  "*** BLACK LIST ***");
	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_disassoc_sta(struct sap_context *sap_ctx,
				struct csr_del_sta_params *p_del_sta_params)
{
	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sme_roam_disconnect_sta(CDS_GET_HAL_CB(),
				sap_ctx->sessionId, p_del_sta_params);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_deauth_sta(struct sap_context *sap_ctx,
			      struct csr_del_sta_params *pDelStaParams)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAULT;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return qdf_status;
	}

	qdf_ret_status =
		sme_roam_deauth_sta(CDS_GET_HAL_CB(),
				    sap_ctx->sessionId, pDelStaParams);

	if (qdf_ret_status == QDF_STATUS_SUCCESS) {
		qdf_status = QDF_STATUS_SUCCESS;
	}
	return qdf_status;
}

/**
 * wlansap_update_csa_channel_params() - function to populate channel width and
 *                                        bonding modes.
 * @sap_context: sap adapter context
 * @channel: target channel
 *
 * Return: The QDF_STATUS code associated with performing the operation
 */
static QDF_STATUS
wlansap_update_csa_channel_params(struct sap_context *sap_context,
				  uint32_t channel)
{
	void *hal;
	tpAniSirGlobal mac_ctx;
	uint8_t bw;

	hal = CDS_GET_HAL_CB();
	if (!hal) {
		sap_err("Invalid hal pointer from p_cds_gctx");
		return QDF_STATUS_E_FAULT;
	}

	mac_ctx = PMAC_STRUCT(hal);

	if (channel <= CHAN_ENUM_14) {
		/*
		 * currently OBSS scan is done in hostapd, so to avoid
		 * SAP coming up in HT40 on channel switch we are
		 * disabling channel bonding in 2.4Ghz.
		 */
		mac_ctx->sap.SapDfsInfo.new_chanWidth = 0;

	} else {
		if (sap_context->csr_roamProfile.phyMode ==
		    eCSR_DOT11_MODE_11ac ||
		    sap_context->csr_roamProfile.phyMode ==
		    eCSR_DOT11_MODE_11ac_ONLY)
			bw = BW80;
		else
			bw = BW40_HIGH_PRIMARY;

		for (; bw >= BW20; bw--) {
			uint16_t op_class;

			op_class = wlan_reg_dmn_get_opclass_from_channel(
					mac_ctx->scan.countryCodeCurrent,
					channel, bw);
			/*
			 * Do not continue if bw is 20. This mean channel is not
			 * found and thus set BW20 for the channel.
			 */
			if (!op_class && bw > BW20)
				continue;

			if (bw == BW80) {
				mac_ctx->sap.SapDfsInfo.new_chanWidth =
					CH_WIDTH_80MHZ;
			} else if (bw == BW40_HIGH_PRIMARY) {
				mac_ctx->sap.SapDfsInfo.new_chanWidth =
					CH_WIDTH_40MHZ;
			} else if (bw == BW40_LOW_PRIMARY) {
				mac_ctx->sap.SapDfsInfo.new_chanWidth =
				   CH_WIDTH_40MHZ;
			} else {
				mac_ctx->sap.SapDfsInfo.new_chanWidth =
				   CH_WIDTH_20MHZ;
			}
			break;
		}

	}

	return QDF_STATUS_SUCCESS;
}

/**
 * sap_get_csa_reason_str() - Get csa reason in string
 * @reason: sap reason enum value
 *
 * Return: string reason
 */
#ifdef WLAN_DEBUG
static char *sap_get_csa_reason_str(enum sap_csa_reason_code reason)
{
	switch (reason) {
	case CSA_REASON_UNKNOWN:
		return "UNKNOWN";
	case CSA_REASON_STA_CONNECT_DFS_TO_NON_DFS:
		return "STA_CONNECT_DFS_TO_NON_DFS";
	case CSA_REASON_USER_INITIATED:
		return "USER_INITIATED";
	case CSA_REASON_PEER_ACTION_FRAME:
		return "PEER_ACTION_FRAME";
	case CSA_REASON_PRE_CAC_SUCCESS:
		return "PRE_CAC_SUCCESS";
	case CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL:
		return "CONCURRENT_STA_CHANGED_CHANNEL";
	case CSA_REASON_UNSAFE_CHANNEL:
		return "UNSAFE_CHANNEL";
	case CSA_REASON_LTE_COEX:
		return "LTE_COEX";
	case CSA_REASON_CONCURRENT_NAN_EVENT:
		return "CONCURRENT_NAN_EVENT";
	case CSA_REASON_BAND_RESTRICTED:
		return "BAND_RESTRICTED";
	default:
		return "UNKNOWN";
	}
}
#endif

/**
 * wlansap_set_channel_change_with_csa() - Set channel change with CSA
 * @sapContext: Pointer to SAP context
 * @targetChannel: Target channel
 * @target_bw: Target bandwidth
 * @strict: if true switch to the requested channel always,
 *        SCC/MCC check will be ignored,
 *        fail otherwise
 *
 * This api function does a channel change to the target channel specified.
 * CSA IE is included in the beacons before doing a channel change.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_set_channel_change_with_csa(struct sap_context *sapContext,
					       uint32_t targetChannel,
					       enum phy_ch_width target_bw,
					       bool strict)
{

	tWLAN_SAPEvent sapEvent;
	tpAniSirGlobal pMac = NULL;
	void *hHal = NULL;
	bool valid;
	QDF_STATUS status;
	bool sta_sap_scc_on_dfs_chan;

	if (NULL == sapContext) {
		sap_err("Invalid SAP pointer");

		return QDF_STATUS_E_FAULT;
	}

	hHal = CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("Invalid HAL pointer from p_cds_gctx");
		return QDF_STATUS_E_FAULT;
	}
	pMac = PMAC_STRUCT(hHal);

	if (strict && !policy_mgr_is_safe_channel(pMac->psoc, targetChannel)) {
		sap_err("%u is unsafe channel", targetChannel);
		return QDF_STATUS_E_FAULT;
	}
	sap_nofl_debug("SAP CSA: %d ---> %d conn on 5GHz:%d, csa_reason:%s(%d) strict %d vdev %d",
		       sapContext->channel, targetChannel,
		       policy_mgr_is_any_mode_active_on_band_along_with_session(
		       pMac->psoc, sapContext->sessionId, POLICY_MGR_BAND_5),
		       sap_get_csa_reason_str(sapContext->csa_reason),
		       sapContext->csa_reason, strict, sapContext->sessionId);

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(pMac->psoc);
	/*
	 * Now, validate if the passed channel is valid in the
	 * current regulatory domain.
	 */
	if (sapContext->channel != targetChannel &&
		((wlan_reg_get_channel_state(pMac->pdev, targetChannel) ==
			CHANNEL_STATE_ENABLE) ||
		(wlan_reg_get_channel_state(pMac->pdev, targetChannel) ==
			CHANNEL_STATE_DFS &&
		(!policy_mgr_is_any_mode_active_on_band_along_with_session(
			pMac->psoc, sapContext->sessionId,
			POLICY_MGR_BAND_5) ||
			sta_sap_scc_on_dfs_chan)))) {
		/*
		 * validate target channel switch w.r.t various concurrency
		 * rules set.
		 */
		if (!strict) {
			valid = wlan_sap_validate_channel_switch(hHal,
				targetChannel, sapContext);
			if (!valid) {
				sap_err("Channel switch to %u is not allowed due to concurrent channel interference",
					targetChannel);
				return QDF_STATUS_E_FAULT;
			}
		}
		/*
		 * Post a CSA IE request to SAP state machine with
		 * target channel information and also CSA IE required
		 * flag set in sapContext only, if SAP is in SAP_STARTED
		 * state.
		 */
		if (sapContext->fsm_state == SAP_STARTED) {
			status = wlansap_update_csa_channel_params(sapContext,
					targetChannel);
			if (status != QDF_STATUS_SUCCESS)
				return status;

			/*
			 * Copy the requested target channel
			 * to sap context.
			 */
			pMac->sap.SapDfsInfo.target_channel = targetChannel;
			pMac->sap.SapDfsInfo.new_ch_params.ch_width =
				pMac->sap.SapDfsInfo.new_chanWidth;

			/* By this time, the best bandwidth is calculated for
			 * the given target channel. Now, if there was a
			 * request from user to move to a selected bandwidth,
			 * we can see if it can be honored.
			 *
			 * Ex1: BW80 was selected for the target channel and
			 * user wants BW40, it can be allowed
			 * Ex2: BW40 was selected for the target channel and
			 * user wants BW80, it cannot be allowed for the given
			 * target channel.
			 *
			 * So, the MIN of the selected channel bandwidth and
			 * user input is used for the bandwidth
			 */
			if (target_bw != CH_WIDTH_MAX) {
				sap_nofl_debug("SAP CSA: target bw:%d new width:%d",
					 target_bw,
					 pMac->sap.SapDfsInfo.
					 new_ch_params.ch_width);
				pMac->sap.SapDfsInfo.new_ch_params.ch_width =
					pMac->sap.SapDfsInfo.new_chanWidth =
					QDF_MIN(pMac->sap.SapDfsInfo.
							new_ch_params.ch_width,
							target_bw);
			}
			wlan_reg_set_channel_params(pMac->pdev, targetChannel,
				0, &pMac->sap.SapDfsInfo.new_ch_params);
			/*
			 * Set the CSA IE required flag.
			 */
			pMac->sap.SapDfsInfo.csaIERequired = true;

			/*
			 * Set the radar found status to allow the channel
			 * change to happen same as in the case of a radar
			 * detection. Since, this will allow SAP to be in
			 * correct state and also resume the netif queues
			 * that were suspended in HDD before the channel
			 * request was issued.
			 */
			pMac->sap.SapDfsInfo.sap_radar_found_status = true;
			pMac->sap.SapDfsInfo.cac_state =
					eSAP_DFS_DO_NOT_SKIP_CAC;
			sap_cac_reset_notify(hHal);

			/*
			 * Post the eSAP_CHANNEL_SWITCH_ANNOUNCEMENT_START
			 * to SAP state machine to process the channel
			 * request with CSA IE set in the beacons.
			 */
			sapEvent.event =
				eSAP_CHANNEL_SWITCH_ANNOUNCEMENT_START;
			sapEvent.params = 0;
			sapEvent.u1 = 0;
			sapEvent.u2 = 0;

			sap_fsm(sapContext, &sapEvent);

		} else {
			sap_err("Failed to request Channel Change, since SAP is not in SAP_STARTED state");
			return QDF_STATUS_E_FAULT;
		}

	} else {
		sap_err("Channel = %d is not valid in the current"
			"regulatory domain", targetChannel);

		return QDF_STATUS_E_FAULT;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_set_key_sta(struct sap_context *sap_ctx,
			       tCsrRoamSetKey *pSetKeyInfo)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	void *hHal = NULL;
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	uint32_t roamId = INVALID_ROAM_ID;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	hHal = CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("Invalid HAL pointer");
		return QDF_STATUS_E_FAULT;
	}
	qdf_ret_status =
		sme_roam_set_key(hHal, sap_ctx->sessionId, pSetKeyInfo,
				 &roamId);

	if (qdf_ret_status == QDF_STATUS_SUCCESS)
		qdf_status = QDF_STATUS_SUCCESS;
	else
		qdf_status = QDF_STATUS_E_FAULT;

	return qdf_status;
}

QDF_STATUS wlan_sap_getstation_ie_information(struct sap_context *sap_ctx,
					      uint32_t *len, uint8_t *buf)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	uint32_t ie_len = 0;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	if (len) {
		ie_len = *len;
		*len = sap_ctx->nStaWPARSnReqIeLength;
			sap_info("WPAIE len : %x", *len);
		if ((buf) && (ie_len >= sap_ctx->nStaWPARSnReqIeLength)) {
			qdf_mem_copy(buf,
				sap_ctx->pStaWpaRsnReqIE,
				sap_ctx->nStaWPARSnReqIeLength);
			sap_info("WPAIE: %02x:%02x:%02x:%02x:%02x:%02x",
				 buf[0], buf[1], buf[2], buf[3], buf[4],
				 buf[5]);
			qdf_status = QDF_STATUS_SUCCESS;
		}
	}
	return qdf_status;
}

QDF_STATUS wlan_sap_update_next_channel(struct sap_context *sap_ctx,
					uint8_t channel,
					enum phy_ch_width chan_bw)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->dfs_vendor_channel = channel;
	sap_ctx->dfs_vendor_chan_bw = chan_bw;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sap_set_pre_cac_status(struct sap_context *sap_ctx,
				       bool status, tHalHandle handle)
{
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(handle);

	if (!mac_ctx) {
		sap_err("Invalid mac pointer");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->is_pre_cac_on = status;
	sap_debug("is_pre_cac_on:%d", sap_ctx->is_pre_cac_on);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sap_set_chan_before_pre_cac(struct sap_context *sap_ctx,
					    uint8_t chan_before_pre_cac)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->chan_before_pre_cac = chan_before_pre_cac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sap_set_pre_cac_complete_status(struct sap_context *sap_ctx,
						bool status)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->pre_cac_complete = status;

	sap_debug("pre cac complete status:%d session:%d", status,
		  sap_ctx->sessionId);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_sap_is_pre_cac_active() - Checks if pre cac in in progress
 * @handle: Global MAC handle
 *
 * Checks if pre cac is in progress in any of the SAP contexts
 *
 * Return: True is pre cac is active, false otherwise
 */
bool wlan_sap_is_pre_cac_active(tHalHandle handle)
{
	tpAniSirGlobal mac = NULL;
	int i;

	mac = PMAC_STRUCT(handle);
	if (!mac) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			"%s: Invalid mac context", __func__);
		return false;
	}

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		struct sap_context *context =
			mac->sap.sapCtxList[i].sap_context;
		if (context && context->is_pre_cac_on)
			return true;
	}
	return false;
}

/**
 * wlan_sap_get_pre_cac_vdev_id() - Get vdev id of the pre cac interface
 * @handle: Global handle
 * @vdev_id: vdev id
 *
 * Fetches the vdev id of the pre cac interface
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_sap_get_pre_cac_vdev_id(tHalHandle handle, uint8_t *vdev_id)
{
	tpAniSirGlobal mac = NULL;
	uint8_t i;

	mac = PMAC_STRUCT(handle);
	if (!mac) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
			"%s: Invalid mac context", __func__);
		return QDF_STATUS_E_FAULT;
	}

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		struct sap_context *context =
			mac->sap.sapCtxList[i].sap_context;
		if (context && context->is_pre_cac_on) {
			*vdev_id = i;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wlansap_register_mgmt_frame(struct sap_context *sap_ctx,
				       uint16_t frameType,
				       uint8_t *matchData,
				       uint16_t matchLen)
{
	void *hHal = NULL;
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer from pCtx");
		return QDF_STATUS_E_FAULT;
	}
	hHal = CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("hal pointer null");
		return QDF_STATUS_E_FAULT;
	}

	qdf_ret_status = sme_register_mgmt_frame(hHal, sap_ctx->sessionId,
						 frameType, matchData,
						 matchLen);

	if (QDF_STATUS_SUCCESS == qdf_ret_status) {
		return QDF_STATUS_SUCCESS;
	}

	sap_err("Failed to Register MGMT frame");

	return QDF_STATUS_E_FAULT;
}

QDF_STATUS wlansap_de_register_mgmt_frame(struct sap_context *sap_ctx,
					  uint16_t frameType,
					  uint8_t *matchData,
					  uint16_t matchLen)
{
	void *hHal = NULL;
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer from pCtx");
		return QDF_STATUS_E_FAULT;
	}
	hHal = CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("hal pointer null");
		return QDF_STATUS_E_FAULT;
	}

	qdf_ret_status =
		sme_deregister_mgmt_frame(hHal, sap_ctx->sessionId, frameType,
					  matchData, matchLen);

	if (QDF_STATUS_SUCCESS == qdf_ret_status) {
		return QDF_STATUS_SUCCESS;
	}

	sap_err("Failed to Deregister MGMT frame");

	return QDF_STATUS_E_FAULT;
}

void wlansap_get_sec_channel(uint8_t sec_ch_offset,
			     uint8_t op_channel,
			     uint8_t *sec_channel)
{
	switch (sec_ch_offset) {
	case LOW_PRIMARY_CH:
		*sec_channel = op_channel + 4;
		break;
	case HIGH_PRIMARY_CH:
		*sec_channel = op_channel - 4;
		break;
	default:
		*sec_channel = 0;
	}
	sap_debug("sec channel offset %d, sec channel %d",
		  sec_ch_offset, *sec_channel);
}

QDF_STATUS wlansap_channel_change_request(struct sap_context *sapContext,
					  uint8_t target_channel)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	void *hHal = NULL;
	tpAniSirGlobal mac_ctx = NULL;
	eCsrPhyMode phy_mode;
	struct ch_params *ch_params;

	if (!target_channel) {
		sap_err("channel 0 requested");
		return QDF_STATUS_E_FAULT;
	}

	if (NULL == sapContext) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	hHal = CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("Invalid HAL pointer from p_cds_gctx");
		return QDF_STATUS_E_FAULT;
	}
	mac_ctx = PMAC_STRUCT(hHal);
	phy_mode = sapContext->csr_roamProfile.phyMode;

	/* Update phy_mode if the target channel is in the other band */
	if (WLAN_CHAN_IS_5GHZ(target_channel) &&
	    ((phy_mode == eCSR_DOT11_MODE_11g) ||
	    (phy_mode == eCSR_DOT11_MODE_11g_ONLY)))
		phy_mode = eCSR_DOT11_MODE_11a;
	else if (WLAN_CHAN_IS_2GHZ(target_channel) &&
		 (phy_mode == eCSR_DOT11_MODE_11a))
		phy_mode = eCSR_DOT11_MODE_11g;

	sapContext->csr_roamProfile.phyMode = phy_mode;

	if (sapContext->csr_roamProfile.ChannelInfo.numOfChannels == 0 ||
	    sapContext->csr_roamProfile.ChannelInfo.ChannelList == NULL) {
		sap_err("Invalid channel list");
		return QDF_STATUS_E_FAULT;
	}
	sapContext->csr_roamProfile.ChannelInfo.ChannelList[0] = target_channel;
	/*
	 * We are getting channel bonding mode from sapDfsInfor structure
	 * because we've implemented channel width fallback mechanism for DFS
	 * which will result in channel width changing dynamically.
	 */
	ch_params = &mac_ctx->sap.SapDfsInfo.new_ch_params;
	wlan_reg_set_channel_params(mac_ctx->pdev, target_channel,
			0, ch_params);
	sapContext->ch_params = *ch_params;
	/* Update the channel as this will be used to
	 * send event to supplicant
	 */
	sapContext->channel = target_channel;
	wlansap_get_sec_channel(ch_params->sec_ch_offset, target_channel,
				(uint8_t *)(&sapContext->secondary_ch));
	sapContext->csr_roamProfile.ch_params.ch_width = ch_params->ch_width;
	sapContext->csr_roamProfile.ch_params.sec_ch_offset =
						ch_params->sec_ch_offset;
	sapContext->csr_roamProfile.ch_params.center_freq_seg0 =
						ch_params->center_freq_seg0;
	sapContext->csr_roamProfile.ch_params.center_freq_seg1 =
						ch_params->center_freq_seg1;
	sap_dfs_set_current_channel(sapContext);

	qdf_ret_status = sme_roam_channel_change_req(hHal, sapContext->bssid,
				ch_params, &sapContext->csr_roamProfile);

	sap_info("chan:%d phy_mode %d width:%d offset:%d seg0:%d seg1:%d",
		 sapContext->channel, phy_mode, ch_params->ch_width,
		 ch_params->sec_ch_offset, ch_params->center_freq_seg0,
		 ch_params->center_freq_seg1);

	if (qdf_ret_status == QDF_STATUS_SUCCESS) {
		sap_signal_hdd_event(sapContext, NULL,
			eSAP_CHANNEL_CHANGE_EVENT,
			(void *) eSAP_STATUS_SUCCESS);

		return QDF_STATUS_SUCCESS;
	}
	return QDF_STATUS_E_FAULT;
}

QDF_STATUS wlansap_start_beacon_req(struct sap_context *sap_ctx)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	void *hHal = NULL;
	uint8_t dfsCacWaitStatus = 0;
	tpAniSirGlobal pMac = NULL;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	hHal = CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("Invalid HAL pointer");
		return QDF_STATUS_E_FAULT;
	}
	pMac = PMAC_STRUCT(hHal);

	/* No Radar was found during CAC WAIT, So start Beaconing */
	if (pMac->sap.SapDfsInfo.sap_radar_found_status == false) {
		/* CAC Wait done without any Radar Detection */
		dfsCacWaitStatus = true;
		sap_ctx->pre_cac_complete = false;
		qdf_ret_status = sme_roam_start_beacon_req(hHal,
							   sap_ctx->bssid,
							   dfsCacWaitStatus);
		if (qdf_ret_status == QDF_STATUS_SUCCESS) {
			return QDF_STATUS_SUCCESS;
		}
		return QDF_STATUS_E_FAULT;
	}

	return QDF_STATUS_E_FAULT;
}

QDF_STATUS wlansap_dfs_send_csa_ie_request(struct sap_context *sap_ctx)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	void *hHal = NULL;
	tpAniSirGlobal pMac = NULL;

	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	hHal = CDS_GET_HAL_CB();
	if (NULL == hHal) {
		sap_err("Invalid HAL pointer");
		return QDF_STATUS_E_FAULT;
	}
	pMac = PMAC_STRUCT(hHal);

	pMac->sap.SapDfsInfo.new_ch_params.ch_width =
				pMac->sap.SapDfsInfo.new_chanWidth;
	wlan_reg_set_channel_params(pMac->pdev,
			pMac->sap.SapDfsInfo.target_channel,
			0, &pMac->sap.SapDfsInfo.new_ch_params);

	sap_info("chan:%d req:%d width:%d off:%d",
		 pMac->sap.SapDfsInfo.target_channel,
		 pMac->sap.SapDfsInfo.csaIERequired,
		 pMac->sap.SapDfsInfo.new_ch_params.ch_width,
		 pMac->sap.SapDfsInfo.new_ch_params.sec_ch_offset);

	qdf_ret_status = sme_roam_csa_ie_request(hHal,
				sap_ctx->bssid,
				pMac->sap.SapDfsInfo.target_channel,
				pMac->sap.SapDfsInfo.csaIERequired,
				&pMac->sap.SapDfsInfo.new_ch_params);

	if (qdf_ret_status == QDF_STATUS_SUCCESS) {
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAULT;
}

/*==========================================================================
   FUNCTION    wlansap_get_dfs_ignore_cac

   DESCRIPTION
   This API is used to get the value of ignore_cac value

   DEPENDENCIES
   NA.

   PARAMETERS
   IN
   hHal : HAL pointer
   pIgnore_cac : pointer to ignore_cac variable

   RETURN VALUE
   The QDF_STATUS code associated with performing the operation

   QDF_STATUS_SUCCESS:  Success

   SIDE EFFECTS
   ============================================================================*/
QDF_STATUS wlansap_get_dfs_ignore_cac(tHalHandle hHal, uint8_t *pIgnore_cac)
{
	tpAniSirGlobal pMac = NULL;

	if (NULL != hHal) {
		pMac = PMAC_STRUCT(hHal);
	} else {
		sap_err("Invalid hHal pointer");
		return QDF_STATUS_E_FAULT;
	}

	*pIgnore_cac = pMac->sap.SapDfsInfo.ignore_cac;
	return QDF_STATUS_SUCCESS;
}

/*==========================================================================
   FUNCTION    wlansap_set_dfs_ignore_cac

   DESCRIPTION
   This API is used to Set the value of ignore_cac value

   DEPENDENCIES
   NA.

   PARAMETERS
   IN
   hHal : HAL pointer
   ignore_cac : value to set for ignore_cac variable in DFS global structure.

   RETURN VALUE
   The QDF_STATUS code associated with performing the operation

   QDF_STATUS_SUCCESS:  Success

   SIDE EFFECTS
   ============================================================================*/
QDF_STATUS wlansap_set_dfs_ignore_cac(tHalHandle hHal, uint8_t ignore_cac)
{
	tpAniSirGlobal pMac = NULL;

	if (NULL != hHal) {
		pMac = PMAC_STRUCT(hHal);
	} else {
		sap_err("Invalid hHal pointer");
		return QDF_STATUS_E_FAULT;
	}

	pMac->sap.SapDfsInfo.ignore_cac = (ignore_cac >= true) ?
					  true : false;
	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_set_dfs_restrict_japan_w53() - enable/disable dfS for japan
 * @hHal : HAL pointer
 * @disable_Dfs_JapanW3 :Indicates if Japan W53 is disabled when set to 1
 *                       Indicates if Japan W53 is enabled when set to 0
 *
 * This API is used to enable or disable Japan W53 Band
 * Return: The QDF_STATUS code associated with performing the operation
 *         QDF_STATUS_SUCCESS:  Success
 */
QDF_STATUS
wlansap_set_dfs_restrict_japan_w53(tHalHandle hHal, uint8_t disable_Dfs_W53)
{
	tpAniSirGlobal pMac = NULL;
	QDF_STATUS status;
	enum dfs_reg dfs_region;

	if (NULL != hHal) {
		pMac = PMAC_STRUCT(hHal);
	} else {
		sap_err("Invalid hHal pointer");
		return QDF_STATUS_E_FAULT;
	}

	wlan_reg_get_dfs_region(pMac->pdev, &dfs_region);

	/*
	 * Set the JAPAN W53 restriction only if the current
	 * regulatory domain is JAPAN.
	 */
	if (DFS_MKK_REG == dfs_region) {
		pMac->sap.SapDfsInfo.is_dfs_w53_disabled = disable_Dfs_W53;
		QDF_TRACE(QDF_MODULE_ID_SAP,
			  QDF_TRACE_LEVEL_INFO_LOW,
			  FL("sapdfs: SET DFS JAPAN W53 DISABLED = %d"),
			  pMac->sap.SapDfsInfo.is_dfs_w53_disabled);

		status = QDF_STATUS_SUCCESS;
	} else {
		sap_err("Regdomain not japan, set disable JP W53 not valid");

		status = QDF_STATUS_E_FAULT;
	}

	return status;
}

bool sap_is_auto_channel_select(struct sap_context *sapcontext)
{
	if (NULL == sapcontext) {
		sap_err("Invalid SAP pointer");
		return false;
	}
	return sapcontext->channel == AUTO_CHANNEL_SELECT;
}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * wlan_sap_set_channel_avoidance() - sets sap mcc channel avoidance ini param
 * @hal:                        hal handle
 * @sap_channel_avoidance:      ini parameter value
 *
 * sets sap mcc channel avoidance ini param, to be called in sap_start
 *
 * Return: success of failure of operation
 */
QDF_STATUS
wlan_sap_set_channel_avoidance(tHalHandle hal, bool sap_channel_avoidance)
{
	tpAniSirGlobal mac_ctx = NULL;

	if (NULL != hal) {
		mac_ctx = PMAC_STRUCT(hal);
	} else {
		sap_err("hal or mac_ctx pointer NULL");
		return QDF_STATUS_E_FAULT;
	}
	mac_ctx->sap.sap_channel_avoidance = sap_channel_avoidance;
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/**
 * wlansap_set_dfs_preferred_channel_location() - set dfs preferred channel
 * @hHal : HAL pointer
 * @dfs_Preferred_Channels_location :
 *       0 - Indicates No preferred channel location restrictions
 *       1 - Indicates SAP Indoor Channels operation only.
 *       2 - Indicates SAP Outdoor Channels operation only.
 *
 * This API is used to set sap preferred channels location
 * to resetrict the DFS random channel selection algorithm
 * either Indoor/Outdoor channels only.
 *
 * Return: The QDF_STATUS code associated with performing the operation
 *         QDF_STATUS_SUCCESS:  Success and error code otherwise.
 */
QDF_STATUS
wlansap_set_dfs_preferred_channel_location(tHalHandle hHal,
					   uint8_t
					   dfs_Preferred_Channels_location)
{
	tpAniSirGlobal pMac = NULL;
	QDF_STATUS status;
	enum dfs_reg dfs_region;

	if (NULL != hHal) {
		pMac = PMAC_STRUCT(hHal);
	} else {
		sap_err("Invalid hHal pointer");
		return QDF_STATUS_E_FAULT;
	}

	wlan_reg_get_dfs_region(pMac->pdev, &dfs_region);

	/*
	 * The Indoor/Outdoor only random channel selection
	 * restriction is currently enforeced only for
	 * JAPAN regulatory domain.
	 */
	if (DFS_MKK_REG == dfs_region) {
		pMac->sap.SapDfsInfo.sap_operating_chan_preferred_location =
			dfs_Preferred_Channels_location;
		QDF_TRACE(QDF_MODULE_ID_SAP,
			  QDF_TRACE_LEVEL_INFO_LOW,
			  FL
				  ("sapdfs:Set Preferred Operating Channel location=%d"),
			  pMac->sap.SapDfsInfo.
			  sap_operating_chan_preferred_location);

		status = QDF_STATUS_SUCCESS;
	} else {
		sap_err("sapdfs:NOT JAPAN REG, Invalid Set preferred chans location");

		status = QDF_STATUS_E_FAULT;
	}

	return status;
}

/*==========================================================================
   FUNCTION    wlansap_set_dfs_target_chnl

   DESCRIPTION
   This API is used to set next target chnl as provided channel.
   you can provide any valid channel to this API.

   DEPENDENCIES
   NA.

   PARAMETERS
   IN
   hHal : HAL pointer
   target_channel : target channel to be set

   RETURN VALUE
   The QDF_STATUS code associated with performing the operation

   QDF_STATUS_SUCCESS:  Success

   SIDE EFFECTS
   ============================================================================*/
QDF_STATUS wlansap_set_dfs_target_chnl(tHalHandle hHal, uint8_t target_channel)
{
	tpAniSirGlobal pMac = NULL;

	if (NULL != hHal) {
		pMac = PMAC_STRUCT(hHal);
	} else {
		sap_err("Invalid hHal pointer");
		return QDF_STATUS_E_FAULT;
	}
	if (target_channel > 0) {
		pMac->sap.SapDfsInfo.user_provided_target_channel =
			target_channel;
	} else {
		pMac->sap.SapDfsInfo.user_provided_target_channel = 0;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlansap_update_sap_config_add_ie(tsap_config_t *pConfig,
				 const uint8_t *pAdditionIEBuffer,
				 uint16_t additionIELength,
				 eUpdateIEsType updateType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t bufferValid = false;
	uint16_t bufferLength = 0;
	uint8_t *pBuffer = NULL;

	if (NULL == pConfig) {
		return QDF_STATUS_E_FAULT;
	}

	if ((pAdditionIEBuffer != NULL) && (additionIELength != 0)) {
		/* initialize the buffer pointer so that pe can copy */
		if (additionIELength > 0) {
			bufferLength = additionIELength;
			pBuffer = qdf_mem_malloc(bufferLength);
			if (NULL == pBuffer) {
				sap_err("Could not allocate the buffer ");
				return QDF_STATUS_E_NOMEM;
			}
			qdf_mem_copy(pBuffer, pAdditionIEBuffer, bufferLength);
			bufferValid = true;
			sap_info("update_type: %d", updateType);
			qdf_trace_hex_dump(QDF_MODULE_ID_SAP,
				QDF_TRACE_LEVEL_INFO, pBuffer, bufferLength);
		}
	}

	switch (updateType) {
	case eUPDATE_IE_PROBE_BCN:
		if (pConfig->pProbeRespBcnIEsBuffer)
			qdf_mem_free(pConfig->pProbeRespBcnIEsBuffer);
		if (bufferValid) {
			pConfig->probeRespBcnIEsLen = bufferLength;
			pConfig->pProbeRespBcnIEsBuffer = pBuffer;
		} else {
			pConfig->probeRespBcnIEsLen = 0;
			pConfig->pProbeRespBcnIEsBuffer = NULL;
			sap_info("No Probe Resp beacone IE received in set beacon");
		}
		break;
	case eUPDATE_IE_PROBE_RESP:
		if (pConfig->pProbeRespIEsBuffer)
			qdf_mem_free(pConfig->pProbeRespIEsBuffer);
		if (bufferValid) {
			pConfig->probeRespIEsBufferLen = bufferLength;
			pConfig->pProbeRespIEsBuffer = pBuffer;
		} else {
			pConfig->probeRespIEsBufferLen = 0;
			pConfig->pProbeRespIEsBuffer = NULL;
			sap_info("No Probe Response IE received in set beacon");
		}
		break;
	case eUPDATE_IE_ASSOC_RESP:
		if (pConfig->pAssocRespIEsBuffer)
			qdf_mem_free(pConfig->pAssocRespIEsBuffer);
		if (bufferValid) {
			pConfig->assocRespIEsLen = bufferLength;
			pConfig->pAssocRespIEsBuffer = pBuffer;
		} else {
			pConfig->assocRespIEsLen = 0;
			pConfig->pAssocRespIEsBuffer = NULL;
			sap_info("No Assoc Response IE received in set beacon");
		}
		break;
	default:
		sap_info("No matching buffer type %d", updateType);
		if (pBuffer != NULL)
			qdf_mem_free(pBuffer);
		break;
	}

	return status;
}

QDF_STATUS
wlansap_reset_sap_config_add_ie(tsap_config_t *pConfig, eUpdateIEsType updateType)
{
	if (NULL == pConfig) {
		sap_err("Invalid Config pointer");
		return QDF_STATUS_E_FAULT;
	}

	switch (updateType) {
	case eUPDATE_IE_ALL:    /*only used to reset */
	case eUPDATE_IE_PROBE_RESP:
		if (pConfig->pProbeRespIEsBuffer) {
			qdf_mem_free(pConfig->pProbeRespIEsBuffer);
			pConfig->probeRespIEsBufferLen = 0;
			pConfig->pProbeRespIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;

	case eUPDATE_IE_ASSOC_RESP:
		if (pConfig->pAssocRespIEsBuffer) {
			qdf_mem_free(pConfig->pAssocRespIEsBuffer);
			pConfig->assocRespIEsLen = 0;
			pConfig->pAssocRespIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;

	case eUPDATE_IE_PROBE_BCN:
		if (pConfig->pProbeRespBcnIEsBuffer) {
			qdf_mem_free(pConfig->pProbeRespBcnIEsBuffer);
			pConfig->probeRespBcnIEsLen = 0;
			pConfig->pProbeRespBcnIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;

	default:
		if (eUPDATE_IE_ALL != updateType)
			sap_err("Invalid buffer type %d", updateType);
		break;
	}
	return QDF_STATUS_SUCCESS;
}

/*==========================================================================
   FUNCTION  wlansap_extend_to_acs_range

   DESCRIPTION Function extends give channel range to consider ACS chan bonding

   DEPENDENCIES PARAMETERS

   IN /OUT
   *startChannelNum : ACS extend start ch
   *endChannelNum   : ACS extended End ch
   *bandStartChannel: Band start ch
   *bandEndChannel  : Band end ch

   RETURN VALUE NONE

   SIDE EFFECTS
   ============================================================================*/
void wlansap_extend_to_acs_range(tHalHandle hal, uint8_t *startChannelNum,
		uint8_t *endChannelNum, uint8_t *bandStartChannel,
		uint8_t *bandEndChannel)
{
#define ACS_WLAN_20M_CH_INC 4
#define ACS_2G_EXTEND ACS_WLAN_20M_CH_INC
#define ACS_5G_EXTEND (ACS_WLAN_20M_CH_INC * 3)

	uint8_t tmp_startChannelNum = 0, tmp_endChannelNum = 0;
	tpAniSirGlobal mac_ctx;

	mac_ctx = PMAC_STRUCT(hal);
	if (!mac_ctx) {
		sap_err("Invalid mac_ctx");
		return;
	}
	if (*startChannelNum <= 14 && *endChannelNum <= 14) {
		*bandStartChannel = CHAN_ENUM_1;
		*bandEndChannel = CHAN_ENUM_14;
		tmp_startChannelNum = *startChannelNum > 5 ?
				   (*startChannelNum - ACS_2G_EXTEND) : 1;
		tmp_endChannelNum = (*endChannelNum + ACS_2G_EXTEND) <= 14 ?
				 (*endChannelNum + ACS_2G_EXTEND) : 14;
	} else if (*startChannelNum >= 36 && *endChannelNum >= 36) {
		*bandStartChannel = CHAN_ENUM_36;
		*bandEndChannel = CHAN_ENUM_173;
		tmp_startChannelNum = (*startChannelNum - ACS_5G_EXTEND) > 36 ?
				   (*startChannelNum - ACS_5G_EXTEND) : 36;
		tmp_endChannelNum = (*endChannelNum + ACS_5G_EXTEND) <=
				     WNI_CFG_CURRENT_CHANNEL_STAMAX ?
				     (*endChannelNum + ACS_5G_EXTEND) :
				     WNI_CFG_CURRENT_CHANNEL_STAMAX;
	} else {
		*bandStartChannel = CHAN_ENUM_1;
		*bandEndChannel = CHAN_ENUM_173;
		tmp_startChannelNum = *startChannelNum > 5 ?
			(*startChannelNum - ACS_2G_EXTEND) : 1;
		tmp_endChannelNum = (*endChannelNum + ACS_5G_EXTEND) <=
				     WNI_CFG_CURRENT_CHANNEL_STAMAX ?
				     (*endChannelNum + ACS_5G_EXTEND) :
				     WNI_CFG_CURRENT_CHANNEL_STAMAX;
	}

	/* Note if the ACS range include only DFS channels, do not cross range
	* Active scanning in adjacent non DFS channels results in transmission
	* spikes in DFS specturm channels which is due to emission spill.
	* Remove the active channels from extend ACS range for DFS only range
	*/
	if (wlan_reg_is_dfs_ch(mac_ctx->pdev, *startChannelNum)) {
		while (!wlan_reg_is_dfs_ch(mac_ctx->pdev,
					tmp_startChannelNum) &&
			tmp_startChannelNum < *startChannelNum)
			tmp_startChannelNum += ACS_WLAN_20M_CH_INC;

		*startChannelNum = tmp_startChannelNum;
	}
	if (wlan_reg_is_dfs_ch(mac_ctx->pdev, *endChannelNum)) {
		while (!wlan_reg_is_dfs_ch(mac_ctx->pdev,
					tmp_endChannelNum) &&
				 tmp_endChannelNum > *endChannelNum)
			tmp_endChannelNum -= ACS_WLAN_20M_CH_INC;

		*endChannelNum = tmp_endChannelNum;
	}
}

QDF_STATUS wlan_sap_set_vendor_acs(struct sap_context *sap_context,
				   bool is_vendor_acs)
{
	if (!sap_context) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	sap_context->vendor_acs_dfs_lte_enabled = is_vendor_acs;

	return QDF_STATUS_SUCCESS;
}

#ifdef DFS_COMPONENT_ENABLE
QDF_STATUS wlansap_set_dfs_nol(struct sap_context *sap_ctx,
			       eSapDfsNolType conf)
{
	void *hal = NULL;
	tpAniSirGlobal mac = NULL;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	hal = CDS_GET_HAL_CB();
	if (!hal) {
		sap_err("Invalid HAL pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = PMAC_STRUCT(hal);

	if (conf == eSAP_DFS_NOL_CLEAR) {
		struct wlan_objmgr_pdev *pdev;

		sap_err("clear the DFS NOL");

		pdev = mac->pdev;
		if (!pdev) {
			sap_err("null pdev");
			return QDF_STATUS_E_FAULT;
		}
		utils_dfs_clear_nol_channels(pdev);
	} else if (conf == eSAP_DFS_NOL_RANDOMIZE) {
		sap_err("Randomize the DFS NOL");

	} else {
		sap_err("unsupport type %d", conf);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlansap_populate_del_sta_params() - populate delete station parameter
 * @mac:           Pointer to peer mac address.
 * @reason_code:   Reason code for the disassoc/deauth.
 * @subtype:       Subtype points to either disassoc/deauth frame.
 * @pDelStaParams: Address where parameters to be populated.
 *
 * This API is used to populate delete station parameter structure
 *
 * Return: none
 */

void wlansap_populate_del_sta_params(const uint8_t *mac,
				     uint16_t reason_code,
				     uint8_t subtype,
				     struct csr_del_sta_params *pDelStaParams)
{
	if (NULL == mac)
		qdf_set_macaddr_broadcast(&pDelStaParams->peerMacAddr);
	else
		qdf_mem_copy(pDelStaParams->peerMacAddr.bytes, mac,
			     QDF_MAC_ADDR_SIZE);

	if (reason_code == 0)
		pDelStaParams->reason_code = eSIR_MAC_DEAUTH_LEAVING_BSS_REASON;
	else
		pDelStaParams->reason_code = reason_code;

	if (subtype == (SIR_MAC_MGMT_DEAUTH >> 4) ||
	    subtype == (SIR_MAC_MGMT_DISASSOC >> 4))
		pDelStaParams->subtype = subtype;
	else
		pDelStaParams->subtype = (SIR_MAC_MGMT_DEAUTH >> 4);

	sap_debug("Delete STA with RC:%hu subtype:%hhu MAC::" MAC_ADDRESS_STR,
		  pDelStaParams->reason_code, pDelStaParams->subtype,
		  MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr.bytes));
}

QDF_STATUS wlansap_acs_chselect(struct sap_context *sap_context,
				tpWLAN_SAPEventCB pacs_event_callback,
				tsap_config_t *pconfig,
				void *pusr_context)
{
	tHalHandle h_hal = NULL;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	tpAniSirGlobal pmac = NULL;

	if (NULL == sap_context) {
		sap_err("Invalid SAP pointer");

		return QDF_STATUS_E_FAULT;
	}

	h_hal = (tHalHandle)CDS_GET_HAL_CB();
	if (NULL == h_hal) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	pmac = PMAC_STRUCT(h_hal);
	sap_context->acs_cfg = &pconfig->acs_cfg;
	sap_context->ch_width_orig = pconfig->acs_cfg.ch_width;
	sap_context->csr_roamProfile.phyMode = pconfig->acs_cfg.hw_mode;

	/*
	 * Now, configure the scan and ACS channel params
	 * to issue a scan request.
	 */
	wlansap_set_scan_acs_channel_params(pconfig, sap_context,
						pusr_context);

	/*
	 * Copy the HDD callback function to report the
	 * ACS result after scan in SAP context callback function.
	 */
	sap_context->pfnSapEventCallback = pacs_event_callback;
	/*
	 * init dfs channel nol
	 */
	sap_init_dfs_channel_nol_list(sap_context);

	/*
	 * Issue the scan request. This scan request is
	 * issued before the start BSS is done so
	 *
	 * 1. No need to pass the second parameter
	 * as the SAP state machine is not started yet
	 * and there is no need for any event posting.
	 *
	 * 2. Set third parameter to TRUE to indicate the
	 * channel selection function to register a
	 * different scan callback function to process
	 * the results pre start BSS.
	 */
	qdf_status = sap_channel_sel(sap_context);

	if (QDF_STATUS_E_ABORTED == qdf_status) {
		sap_err("DFS not supported in the current operating mode");
		return QDF_STATUS_E_FAILURE;
	} else if (QDF_STATUS_E_CANCELED == qdf_status) {
		/*
		* ERROR is returned when either the SME scan request
		* failed or ACS is overridden due to other constrainst
		* So send selected channel to HDD
		*/
		sap_err("Scan Req Failed/ACS Overridden");
		sap_err("Selected channel = %d", sap_context->channel);

		return sap_signal_hdd_event(sap_context, NULL,
				eSAP_ACS_CHANNEL_SELECTED,
				(void *) eSAP_STATUS_SUCCESS);
	}

	return qdf_status;
}

/**
 * wlan_sap_enable_phy_error_logs() - Enable DFS phy error logs
 * @hal:        global hal handle
 * @enable_log: value to set
 *
 * Since the frequency of DFS phy error is very high, enabling logs for them
 * all the times can cause crash and will also create lot of useless logs
 * causing difficulties in debugging other issue. This function will be called
 * from iwpriv cmd to eanble such logs temporarily.
 *
 * Return: void
 */
void wlan_sap_enable_phy_error_logs(tHalHandle hal, uint32_t enable_log)
{
	int error;

	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal);

	mac_ctx->sap.enable_dfs_phy_error_logs = !!enable_log;
	tgt_dfs_control(mac_ctx->pdev, DFS_SET_DEBUG_LEVEL, &enable_log,
			sizeof(uint32_t), NULL, NULL, &error);
}

/**
 * wlan_sap_set_dfs_pri_multiplier() - Set dfs_pri_multiplier
 * @hal:        global hal handle
 * @val:        value to set
 *
 * Return: none
 */
#ifdef DFS_PRI_MULTIPLIER
void wlan_sap_set_dfs_pri_multiplier(tHalHandle hal, uint32_t val)
{
	int error;

	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal);

	tgt_dfs_control(mac_ctx->pdev, DFS_SET_PRI_MULTIPILER, &val,
			sizeof(uint32_t), NULL, NULL, &error);
}
#endif

uint32_t wlansap_get_chan_width(struct sap_context *sap_ctx)
{
	return wlan_sap_get_vht_ch_width(sap_ctx);
}

QDF_STATUS wlansap_set_tx_leakage_threshold(tHalHandle hal,
			uint16_t tx_leakage_threshold)
{
	tpAniSirGlobal mac;

	if (NULL == hal) {
		sap_err("Invalid hal pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = PMAC_STRUCT(hal);
	tgt_dfs_set_tx_leakage_threshold(mac->pdev, tx_leakage_threshold);
	sap_debug(" leakage_threshold %d", tx_leakage_threshold);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_set_invalid_session(struct sap_context *sap_ctx)
{
	if (NULL == sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAILURE;
	}

	sap_ctx->sessionId = CSR_SESSION_ID_INVALID;

	return QDF_STATUS_SUCCESS;
}

void wlansap_cleanup_cac_timer(struct sap_context *sap_ctx)
{
	tHalHandle hal;
	tpAniSirGlobal pmac;

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return;
	}

	hal = CDS_GET_HAL_CB();
	if (!hal) {
		sap_err("Invalid hal pointer");
		return;
	}

	pmac = PMAC_STRUCT(hal);
	if (pmac->sap.SapDfsInfo.is_dfs_cac_timer_running) {
		qdf_mc_timer_stop(&pmac->sap.SapDfsInfo.
				  sap_dfs_cac_timer);
		pmac->sap.SapDfsInfo.is_dfs_cac_timer_running = 0;
		qdf_mc_timer_destroy(
			&pmac->sap.SapDfsInfo.sap_dfs_cac_timer);
		sap_err("sapdfs, force cleanup running dfs cac timer");
	}
}

static bool
wlansap_is_channel_present_in_acs_list(uint8_t ch,
				       uint8_t *ch_list,
				       uint8_t ch_count)
{
	uint8_t i;

	for (i = 0; i < ch_count; i++)
		if (ch_list[i] == ch)
			return true;

	return false;
}

QDF_STATUS wlansap_filter_ch_based_acs(struct sap_context *sap_ctx,
				       uint8_t *ch_list,
				       uint32_t *ch_cnt)
{
	size_t ch_index;
	size_t target_ch_cnt = 0;

	if (!sap_ctx || !ch_list || !ch_cnt ||
	    !sap_ctx->acs_cfg->master_ch_list) {
		sap_err("NULL parameters");
		return QDF_STATUS_E_FAULT;
	}

	for (ch_index = 0; ch_index < *ch_cnt; ch_index++) {
		if (wlansap_is_channel_present_in_acs_list(ch_list[ch_index],
					sap_ctx->acs_cfg->master_ch_list,
					sap_ctx->acs_cfg->master_ch_list_count))
			ch_list[target_ch_cnt++] = ch_list[ch_index];
	}

	*ch_cnt = target_ch_cnt;

	return QDF_STATUS_SUCCESS;
}

#if defined(FEATURE_WLAN_CH_AVOID)
/**
 * wlansap_get_safe_channel() - Get safe channel from current regulatory
 * @sap_ctx: Pointer to SAP context
 *
 * This function is used to get safe channel from current regulatory valid
 * channels to restart SAP if failed to get safe channel from PCL.
 *
 * Return: Channel number to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
static uint8_t
wlansap_get_safe_channel(struct sap_context *sap_ctx)
{
	tHalHandle hal;
	tpAniSirGlobal mac;
	struct sir_pcl_list pcl = {0};
	QDF_STATUS status;

	if (!sap_ctx) {
		sap_err("NULL parameters");
		return INVALID_CHANNEL_ID;
	}

	hal = CDS_GET_HAL_CB();
	if (!hal) {
		sap_err("Invalid HAL pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = PMAC_STRUCT(hal);

	/* get the channel list for current domain */
	status = policy_mgr_get_valid_chans(mac->psoc,
					    pcl.pcl_list,
					    &pcl.pcl_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Error in getting valid channels");
		return INVALID_CHANNEL_ID;
	}

	status = wlansap_filter_ch_based_acs(sap_ctx,
					     pcl.pcl_list,
					     &pcl.pcl_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to filter ch from acs %d", status);
		return INVALID_CHANNEL_ID;
	}

	if (pcl.pcl_len) {
		status = policy_mgr_get_valid_chans_from_range(mac->psoc,
							       pcl.pcl_list,
							       &pcl.pcl_len,
							       PM_SAP_MODE);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("get valid channel: %d failed", status);
			return INVALID_CHANNEL_ID;
		}

		if (pcl.pcl_len) {
			sap_debug("select %d from valid channel list",
				  pcl.pcl_list[0]);
			return pcl.pcl_list[0];
		}
	}

	return INVALID_CHANNEL_ID;
}
#else
/**
 * wlansap_get_safe_channel() - Get safe channel from current regulatory
 * @sap_ctx: Pointer to SAP context
 *
 * This function is used to get safe channel from current regulatory valid
 * channels to restart SAP if failed to get safe channel from PCL.
 *
 * Return: Channel number to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
static uint8_t
wlansap_get_safe_channel(struct sap_context *sap_ctx)
{
	return 0;
}
#endif

uint8_t
wlansap_get_safe_channel_from_pcl_and_acs_range(struct sap_context *sap_ctx)
{
	tHalHandle hal;
	tpAniSirGlobal mac;
	struct sir_pcl_list pcl = {0};
	QDF_STATUS status;

	if (!sap_ctx) {
		sap_err("NULL parameter");
		return INVALID_CHANNEL_ID;
	}

	hal = CDS_GET_HAL_CB();
	if (!hal) {
		sap_err("Invalid HAL pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = PMAC_STRUCT(hal);

	status = policy_mgr_get_pcl_for_existing_conn(
			mac->psoc, PM_SAP_MODE, pcl.pcl_list, &pcl.pcl_len,
			pcl.weight_list, QDF_ARRAY_SIZE(pcl.weight_list),
			false);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Get PCL failed");
		return INVALID_CHANNEL_ID;
	}

	if (pcl.pcl_len) {
		status = wlansap_filter_ch_based_acs(sap_ctx,
						     pcl.pcl_list,
						     &pcl.pcl_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("failed filter ch from acs %d", status);
			return INVALID_CHANNEL_ID;
		}

		if (pcl.pcl_len) {
			sap_debug("select %d from valid channel list",
				  pcl.pcl_list[0]);
			return pcl.pcl_list[0];
		}
		sap_debug("no safe channel from PCL found in ACS range");
	} else {
		sap_debug("pcl length is zero!");
	}

	/*
	 * In some scenarios, like hw dbs disabled, sap+sap case, if operating
	 * channel is unsafe channel, the pcl may be empty, instead of return,
	 * try to choose a safe channel from acs range.
	 */
	return wlansap_get_safe_channel(sap_ctx);
}

static uint8_t wlansap_get_2g_first_safe_chan(struct sap_context *sap_ctx)
{
	uint32_t i;
	uint8_t chan;
	enum channel_state state;
	struct regulatory_channel *cur_chan_list;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	uint8_t *acs_chan_list;
	uint8_t acs_list_count;
	tHalHandle hal;
	tpAniSirGlobal mac;

	hal = CDS_GET_HAL_CB();
	if (!hal) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid HAL pointer"));
		return CHANNEL_6;
	}

	mac = PMAC_STRUCT(hal);

	pdev = mac->pdev;
	psoc = mac->psoc;

	cur_chan_list = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(struct regulatory_channel));
	if (!cur_chan_list)
		return CHANNEL_6;

	if (wlan_reg_get_current_chan_list(pdev, cur_chan_list) !=
					   QDF_STATUS_SUCCESS) {
		chan = CHANNEL_6;
		goto err;
	}

	acs_chan_list = sap_ctx->acs_cfg->master_ch_list;
	acs_list_count = sap_ctx->acs_cfg->master_ch_list_count;
	for (i = 0; i < NUM_CHANNELS; i++) {
		chan = cur_chan_list[i].center_freq;
		state = wlan_reg_get_channel_state(pdev, chan);
		if (state != CHANNEL_STATE_DISABLE &&
		    state != CHANNEL_STATE_INVALID &&
		    WLAN_REG_IS_24GHZ_CH(chan) &&
		    policy_mgr_is_safe_channel(psoc, chan) &&
		    wlansap_is_channel_present_in_acs_list(chan,
							   acs_chan_list,
							   acs_list_count)) {
			QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
				  FL("find a 2g channel: %d"),
				  chan);
			goto err;
		}
	}

	chan = CHANNEL_6;
err:
	qdf_mem_free(cur_chan_list);
	return chan;
}

uint8_t wlansap_get_chan_band_restrict(struct sap_context *sap_ctx)
{
	uint8_t restart_chan;
	enum phy_ch_width restart_ch_width;
	uint8_t intf_ch;
	uint32_t phy_mode;
	uint8_t cc_mode;
	enum band_info sap_band;
	tHalHandle hal;
	tpAniSirGlobal mac;
	enum band_info band;

	if (!sap_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
			  FL("sap_ctx NULL parameter"));
		return 0;
	}
	if (cds_is_driver_recovering())
		return 0;

	hal = CDS_GET_HAL_CB();
	if (!hal) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid HAL pointer"));
		return 0;
	}
	mac = PMAC_STRUCT(hal);
	if (!mac || !mac->pdev)
		return 0;
	if (!sap_ctx->channel)
		return 0;

	if (ucfg_reg_get_curr_band(mac->pdev, &band) != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
			  FL("Failed to get current band config"));
		return 0;
	}

	sap_band = sap_ctx->channel <= 14 ? BAND_2G : BAND_5G;
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
		  FL("SAP/Go current band: %d, pdev band capability: %d"),
		  sap_band, band);

	if (sap_band == BAND_5G && band == BAND_2G) {
		sap_ctx->chan_id_before_switch_band = sap_ctx->channel;
		sap_ctx->chan_width_before_switch_band =
			sap_ctx->ch_params.ch_width;
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Save chan info before switch: %d, width: %d"),
			  sap_ctx->channel, sap_ctx->ch_params.ch_width);
		restart_chan = wlansap_get_2g_first_safe_chan(sap_ctx);
		if (restart_chan == 0) {
			QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
				  FL("use default chan 6"));
			restart_chan = CHANNEL_6;
		}
		restart_ch_width = sap_ctx->ch_params.ch_width;
		if (restart_ch_width > CH_WIDTH_40MHZ) {
			QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
				  FL("set 40M when switch SAP to 2G"));
			restart_ch_width = CH_WIDTH_40MHZ;
		}
	} else if (sap_band == BAND_2G &&
		   (band == BAND_ALL || band == BAND_5G)) {
		if (sap_ctx->chan_id_before_switch_band == 0)
			return 0;
		restart_chan = sap_ctx->chan_id_before_switch_band;
		restart_ch_width = sap_ctx->chan_width_before_switch_band;
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Restore chan: %d, width: %d"),
			  restart_chan, restart_ch_width);
		sap_ctx->chan_id_before_switch_band = 0;
		sap_ctx->chan_width_before_switch_band = CH_WIDTH_INVALID;

	} else {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
			  FL("No need switch SAP/Go channel"));
		return 0;
	}

	cc_mode = sap_ctx->cc_switch_mode;
	phy_mode = sap_ctx->csr_roamProfile.phyMode;
	intf_ch = sme_check_concurrent_channel_overlap(hal,
						       restart_chan,
						       phy_mode,
						       cc_mode);
	if (intf_ch)
		restart_chan = intf_ch;
	QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_DEBUG,
		  FL("CSA target ch: %d"), restart_chan);
	sap_ctx->csa_reason = CSA_REASON_BAND_RESTRICTED;

	return restart_chan;
}

#define DH_OUI_TYPE	(0x20)
/**
 * wlansap_validate_owe_ie() - validate OWE IE
 * @ie: IE buffer
 * @remaining_ie_len: remaining IE length
 *
 * Return: validated IE length, -1 for failure
 */
static int wlansap_validate_owe_ie(const uint8_t *ie, uint32_t remaining_ie_len)
{
	uint8_t ie_id, ie_len, ie_ext_id = 0;

	if (remaining_ie_len < 2) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "IE too short");
		return -EINVAL;
	}

	ie_id = ie[0];
	ie_len = ie[1];

	/* IEs that we are expecting in OWE IEs
	 * - RSN IE
	 * - DH IE
	 */
	switch (ie_id) {
	case DOT11F_EID_RSN:
		if (ie_len < DOT11F_IE_RSN_MIN_LEN ||
		    ie_len > DOT11F_IE_RSN_MAX_LEN) {
			QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
					"Invalid RSN IE len %d", ie_len);
			return -EINVAL;
		}
		ie_len += 2;
		break;
	case DOT11F_EID_DH_PARAMETER_ELEMENT:
		ie_ext_id = ie[2];
		if (ie_ext_id != DH_OUI_TYPE) {
			QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
					"Invalid DH IE ID %d", ie_ext_id);
			return -EINVAL;
		}
		if (ie_len < DOT11F_IE_DH_PARAMETER_ELEMENT_MIN_LEN ||
		    ie_len > DOT11F_IE_DH_PARAMETER_ELEMENT_MAX_LEN) {
			QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
					"Invalid DH IE len %d", ie_len);
			return -EINVAL;
		}
		ie_len += 2;
		break;
	default:
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "Invalid IE %d", ie_id);
		return -EINVAL;
	}

	if (ie_len > remaining_ie_len) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "Invalid IE len");
		return -EINVAL;
	}

	return ie_len;
}

/**
 * wlansap_validate_owe_ies() - validate OWE IEs
 * @ie: IE buffer
 * @ie_len: IE length
 *
 * Return: true if validated
 */
static bool wlansap_validate_owe_ies(const uint8_t *ie, uint32_t ie_len)
{
	const uint8_t *remaining_ie = ie;
	uint32_t remaining_ie_len = ie_len;
	int validated_len;
	bool validated = true;

	while (remaining_ie_len) {
		validated_len = wlansap_validate_owe_ie(remaining_ie,
							remaining_ie_len);
		if (validated_len < 0) {
			validated = false;
			break;
		}
		remaining_ie += validated_len;
		remaining_ie_len -= validated_len;
	}

	return validated;
}

QDF_STATUS wlansap_update_owe_info(struct sap_context *sap_ctx,
				   uint8_t *peer, const uint8_t *ie,
				   uint32_t ie_len, uint16_t owe_status)
{
	tHalHandle hal;
	tpAniSirGlobal mac;
	struct owe_assoc_ind *owe_assoc_ind;
	tSirSmeAssocInd *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!wlansap_validate_owe_ies(ie, ie_len)) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "Invalid OWE IE");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_ctx) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	hal = CDS_GET_HAL_CB();
	mac = (tpAniSirGlobal)hal;
	if (!mac) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP, "Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	if (QDF_STATUS_SUCCESS !=
		qdf_list_peek_front(&sap_ctx->owe_pending_assoc_ind_list,
				    &next_node)) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
				"Failed to find assoc ind list");
		return QDF_STATUS_E_FAILURE;
	}

	do {
		node = next_node;
		owe_assoc_ind = qdf_container_of(node, struct owe_assoc_ind,
						 node);
		if (qdf_mem_cmp(peer,
				owe_assoc_ind->assoc_ind->peerMacAddr,
				QDF_MAC_ADDR_SIZE) == 0) {
			status = qdf_list_remove_node(
					   &sap_ctx->owe_pending_assoc_ind_list,
					   node);
			if (status != QDF_STATUS_SUCCESS) {
				QDF_TRACE_ERROR(QDF_MODULE_ID_SAP,
						"Failed to remove assoc ind");
				return status;
			}
			assoc_ind = owe_assoc_ind->assoc_ind;
			qdf_mem_free(owe_assoc_ind);
			break;
		}
	} while (QDF_STATUS_SUCCESS ==
		 qdf_list_peek_next(&sap_ctx->owe_pending_assoc_ind_list,
				    node, &next_node));

	if (assoc_ind) {
		assoc_ind->owe_ie = ie;
		assoc_ind->owe_ie_len = ie_len;
		assoc_ind->owe_status = owe_status;
		status = sme_update_owe_info(mac, assoc_ind);
		qdf_mem_free(assoc_ind);
	}

	return status;
}
