/* Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: contains MLO manager public file containing setup/teardown functionality
 */

#ifndef _WLAN_MLO_MGR_SETUP_H_
#define _WLAN_MLO_MGR_SETUP_H_
#ifdef WLAN_MLO_MULTI_CHIP

/**
 * mlo_setup_init() - API to init setup info events
 * @total_grp: Total number of MLO groups
 *
 * Return: None
 */
void mlo_setup_init(uint8_t total_grp);

/**
 * mlo_setup_deinit() - API to deinit setup info events
 *
 * Return: None
 */
void mlo_setup_deinit(void);

/**
 * mlo_is_ml_soc() - API to check if psoc belongs to ML group
 * @psoc: Soc to be checked.
 * @grp_id: ID of the required mlo group
 *
 * Return: true if psoc found in ml soc_list, or else return false
 */
bool mlo_is_ml_soc(struct wlan_objmgr_psoc *psoc, uint8_t grp_id);

/**
 * mlo_get_soc_list() - API to get the list of SOCs participating in MLO
 * @soc_list: list where ML participating SOCs need to be populated
 * @grp_id: ID of the required mlo group
 * @tot_socs: Total number of socs, for which soc list is allocated
 * @curr: Flag to get the current psoc list or actual psoc list
 *
 * Return: None
 */
void mlo_get_soc_list(struct wlan_objmgr_psoc **soc_list, uint8_t grp_id,
		      uint8_t tot_socs, enum MLO_SOC_LIST curr);

/**
 * mlo_setup_update_soc_id_list() - API to update the list of SOCs ids
 *                                  participating in that MLO group
 * @grp_id: ID of the required mlo group
 * @soc_list: soc ids part of that MLO group
 *
 * Return: None
 */
void mlo_setup_update_soc_id_list(uint8_t grp_id, uint8_t *soc_id_list);

/**
 * mlo_psoc_get_grp_id() - API to get the MLO group id of the SoC
 * @psoc: Required psoc pointer
 * @grp_id: MLO Group id will be stored in here
 *
 * Return: bool: if valid group id true, else false
 */
bool mlo_psoc_get_grp_id(struct wlan_objmgr_psoc *psoc, uint8_t *grp_id);

/**
 * mlo_cleanup_asserted_soc_setup_info() - API to cleanup the mlo setup info of
 * asserted soc
 * @psoc: Soc to be cleaned up
 * @grp_id: ID of the required mlo group
 *
 * Return: None
 */
void mlo_cleanup_asserted_soc_setup_info(struct wlan_objmgr_psoc *psoc,
					 uint8_t grp_id);

/**
 * mlo_setup_update_total_socs() - API to update total socs for mlo
 * @grp_id: ID of the required mlo group
 * @tot_socs: Total socs
 *
 * Return: None.
 */
void mlo_setup_update_total_socs(uint8_t grp_id, uint8_t tot_socs);

/**
 * mlo_setup_get_total_socs() - API to get total socs for mlo group
 * @grp_id: ID of the required mlo group
 *
 * Return: uint8_t, Number of total socs
 */
uint8_t mlo_setup_get_total_socs(uint8_t grp_id);

/**
 * mlo_setup_update_num_links() - API to update num links in soc for mlo
 * @soc_id: soc object of SoC corresponding to num_link
 * @grp_id: ID of the required mlo group
 * @num_links: Number of links in that soc
 *
 * Return: None.
 */
void mlo_setup_update_num_links(struct wlan_objmgr_psoc *psoc,
				uint8_t grp_id,
				uint8_t num_links);

/**
 * mlo_setup_update_soc_ready() - API to notify when FW init done
 * @psoc: soc object of SoC ready
 * @grp_id: ID of the required mlo group
 *
 * Return: None.
 */
void mlo_setup_update_soc_ready(struct wlan_objmgr_psoc *psoc, uint8_t grp_id);

/**
 * mlo_setup_link_ready() - API to notify link ready
 * @pdev: Pointer to pdev object
 * @grp_id: ID of the required mlo group
 *
 * Return: None.
 */
void mlo_setup_link_ready(struct wlan_objmgr_pdev *pdev, uint8_t grp_id);

/**
 * mlo_link_setup_complete() - API to notify setup complete
 * @pdev: Pointer to pdev object
 * @grp_id: ID of the required mlo group
 *
 * Return: None.
 */
void mlo_link_setup_complete(struct wlan_objmgr_pdev *pdev, uint8_t grp_id);

/**
 * mlo_link_teardown_complete() - API to notify teardown complete
 * @pdev: Pointer to pdev object
 * @grp_id: ID of the required mlo group
 *
 * Return: None.
 */
void mlo_link_teardown_complete(struct wlan_objmgr_pdev *pdev, uint8_t grp_id);

/**
 * mlo_setup_update_soc_down() - API to check and clear all links and bring
 *                               back to initial state for the particular soc
 *
 * @psoc: Pointer to psoc object
 * @grp_id: ID of the required mlo group
 *
 * Return: None.
 */
void mlo_setup_update_soc_down(struct wlan_objmgr_psoc *psoc, uint8_t grp_id);

/**
 * mlo_link_teardown_link() - API to trigger teardown
 * @psoc: Pointer to psoc object
 * @grp_id: ID of the required mlo group
 * @reason: Reason code for MLO tear down
 *
 * Return: QDF_STATUS - success / failure.
 */
QDF_STATUS mlo_link_teardown_link(struct wlan_objmgr_psoc *psoc,
				  uint8_t grp_id,
				  uint32_t reason);

/**
 * mlo_vdevs_check_single_soc() - API to check all the vaps in vdev list
 *                                belong to single soc or not
 * @wlan_vdev_list: List of all vdevs to check
 * @vdev_count: Number of vdevs in the list
 *
 * Return: bool: True if belongs to single soc else false
 */
bool mlo_vdevs_check_single_soc(struct wlan_objmgr_vdev **wlan_vdev_list,
				uint8_t vdev_count);

/**
 * mlo_check_all_pdev_state() - API to check all the pdev of the soc
 *                              are on the same expected state.
 *
 * @psoc: Pointer to psoc object
 * @grp_id: ID of the required mlo group
 * @state: Expected link state to be verified
 *
 * Return: QDF_STATUS: QDF_STATUS_SUCCESS if all belongs to same state
 */
QDF_STATUS mlo_check_all_pdev_state(struct wlan_objmgr_psoc *psoc,
				    uint8_t grp_id,
				    enum MLO_LINK_STATE state);
#else
static inline void mlo_setup_init(uint8_t total_grp)
{
}

static inline void mlo_setup_deinit(void)
{
}

static inline bool
mlo_vdevs_check_single_soc(struct wlan_objmgr_vdev **wlan_vdev_list,
			   uint8_t vdev_count)
{
	return true;
}

static inline
QDF_STATUS mlo_check_all_pdev_state(struct wlan_objmgr_psoc *psoc,
				    uint32_t state)
{
	return QDF_STATUS_SUCCESS;
}

static inline
bool mlo_psoc_get_grp_id(struct wlan_objmgr_psoc *psoc, uint8_t *grp_id)
{
	return 0;
}
#endif /* WLAN_MLO_MULTI_CHIP */
#endif /* _WLAN_MLO_MGR_SETUP_H_ */
