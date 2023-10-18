/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains MLO manager public file containing peer functionality
 */
#ifndef _WLAN_MLO_MGR_PEER_H_
#define _WLAN_MLO_MGR_PEER_H_

#include "wlan_objmgr_peer_obj.h"

#define WLAN_LINK_ID_INVALID    0xff

/**
 * mlo_peer_create - Initiatiate peer create on secondary link(s)
 * by posting a message
 *
 * @vdev: pointer to vdev
 * @peer: pointer to peer context
 * @mlo_ie: MLO information element
 * @aid: association ID
 *
 * Initiate the peer on the second link
 *
 * Return: none
 */
void mlo_peer_create(struct wlan_objmgr_vdev *vdev,
			       struct wlan_objmgr_peer *peer, uint8_t *mlo_ie,
			       uint8_t aid);

/**
 * mlo_get_mlpeer - Get ML peer corresponds to the MLD address
 * @ml_dev: MLO DEV object
 * @ml_addr: MLD MAC address
 *
 * This API will be used to get the ML peer associated with MLD address.
 * It will return Null if the peer does not exist for the given MLD address.
 *
 * Return: Pointer to the ML peer context structure
 */
struct wlan_mlo_peer_context *mlo_get_mlpeer(
				struct wlan_mlo_dev_context *ml_dev,
				const struct qdf_mac_addr *ml_addr);

/**
 * mlo_peer_attach - Attaches the peer by updating the MLO peer context with
 * the new link information
 *
 * @vdev: pointer to vdev
 * @peer: pointer to peer context
 *
 * Return: none
 */
void mlo_peer_attach(struct wlan_objmgr_vdev *vdev,
		     struct wlan_objmgr_peer *peer);

/**
 * mlo_peer_setup_failed_notify - Notify MLO manager that peer setup has failed
 * and to cleanup by deleting the partner peers
 *
 * @vdev: pointer to vdev
 *
 * This API is called in scenarios where peer create or peer assoc fails
 *
 * Return: none
 */
void mlo_peer_setup_failed_notify(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_peer_disconnect_notify - Notify MLO manager that peer has disconnected
 * and to clean up by deleting partner peers
 *
 * @vdev: pointer to vdev
 *
 * Return: none
 */
void mlo_peer_disconnect_notify(struct wlan_objmgr_peer *peer);

/**
 * wlan_peer_delete_complete - Notify MLO manager that peer delete is completed
 * and to clean up by unlinking the peer object
 *
 * @peer: pointer to peer context
 *
 * Return: none
 */
void wlan_peer_delete_complete(struct wlan_objmgr_peer *peer);

/**
 * mlo_peer_delete - Delete the peer object
 *
 * @peer: pointer to peer context
 *
 * Return: none
 */
void mlo_peer_delete(struct wlan_objmgr_peer *peer);

/**
 * is_mlo_all_peer_links_deleted - Check if all the peer links are deleted
 *
 * Return: true if all the peer links are deleted, false otherwise
 */
bool is_mlo_all_peer_links_deleted(void);

/**
 * wlan_mlo_peer_is_disconnect_progress() - MLO peer is in disconnect progress
 * @ml_peer: MLO peer
 *
 * This function checks whether MLO Peer is in disconnect progress
 *
 * Return: SUCCESS if MLO Peer is in disconnect progress
 */
QDF_STATUS wlan_mlo_peer_is_disconnect_progress(
					struct wlan_mlo_peer_context *ml_peer);

/**
 * wlan_mlo_peer_is_assoc_done() - MLO peer is Assoc complete
 * @ml_peer: MLO peer
 *
 * This function checks whether MLO Peer's Assoc is completed
 *
 * Return: SUCCESS if MLO Peer Assoc is completed
 */
QDF_STATUS wlan_mlo_peer_is_assoc_done(struct wlan_mlo_peer_context *ml_peer);

/**
 * wlan_mlo_peer_get_assoc_peer() - get assoc peer
 * @ml_peer: MLO peer
 *
 * This function returns assoc peer of MLO peer
 *
 * Return: assoc peer, if it is found, otherwise NULL
 */
struct wlan_objmgr_peer *wlan_mlo_peer_get_assoc_peer(
					struct wlan_mlo_peer_context *ml_peer);

/**
 * mlo_peer_is_assoc_peer() - check whether the peer is assoc peer
 * @ml_peer: MLO peer
 * @peer: Link peer
 *
 * This function checks whether the peer is assoc peer of MLO peer,
 * This API doesn't have lock protection, caller needs to take the lock
 *
 * Return: true, if it is assoc peer
 */
bool mlo_peer_is_assoc_peer(struct wlan_mlo_peer_context *ml_peer,
			    struct wlan_objmgr_peer *peer);

/**
 * wlan_mlo_peer_is_assoc_peer() - check whether the peer is assoc peer
 * @ml_peer: MLO peer
 * @peer: Link peer
 *
 * This function checks whether the peer is assoc peer of MLO peer
 *
 * Return: true, if it is assoc peer
 */
bool wlan_mlo_peer_is_assoc_peer(struct wlan_mlo_peer_context *ml_peer,
				 struct wlan_objmgr_peer *peer);

/**
 * wlan_mlo_partner_peer_assoc_post() - Notify partner peer assoc
 * @peer: Link peer
 *
 * This function notifies link peers to send peer assoc command to FW
 *
 * Return: void
 */
void wlan_mlo_partner_peer_assoc_post(struct wlan_objmgr_peer *assoc_peer);

/**
 * wlan_mlo_peer_deauth_init() - Initiate Deauth of MLO peer
 * @ml_peer: MLO peer
 * @src_peer: Source peer, if this pointer is valid, send deauth on other link
 * @is_disassoc: to indicate, whether Disassoc to be sent instead of deauth
 *
 * This function initiates deauth on MLO peer and its links peers
 *
 * Return: void
 */
void
wlan_mlo_peer_deauth_init(struct wlan_mlo_peer_context *ml_peer,
			  struct wlan_objmgr_peer *src_peer,
			  uint8_t is_disassoc);

/**
 * wlan_mlo_partner_peer_create_failed_notify() - Notify peer creation fail
 * @ml_peer: MLO peer
 *
 * This function notifies about link peer creation failure
 *
 * Return: void
 */
void wlan_mlo_partner_peer_create_failed_notify(
					struct wlan_mlo_peer_context *ml_peer);

/**
 * wlan_mlo_partner_peer_disconnect_notify() - Notify peer disconnect
 * @peer: Link peer
 *
 * This function notifies about disconnect is being initilated on link peer
 *
 * Return: void
 */
void wlan_mlo_partner_peer_disconnect_notify(struct wlan_objmgr_peer *src_peer);

/**
 * wlan_mlo_peer_create() - MLO peer create
 * @vdev: Link VDEV
 * @link_peer: Link peer
 * @ml_info: ML links info
 * @frm_buf: Assoc req buffer
 * @aid: AID, if already allocated
 *
 * This function creates MLO peer and notifies other partner VDEVs to create
 * link peers
 *
 * Return: SUCCESS, if MLO peer is successfully created
 */
QDF_STATUS wlan_mlo_peer_create(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_peer *link_peer,
				struct mlo_partner_info *ml_info,
				qdf_nbuf_t frm_buf,
				uint16_t aid);

/**
 * mlo_peer_cleanup() - Free MLO peer
 * @ml_peer: MLO peer
 *
 * This function frees MLO peer and resets MLO peer associations
 * Note, this API is ref count protected, it should be always invoked
 * from wlan_mlo_peer_release_ref()
 *
 * Return: void
 */
void mlo_peer_cleanup(struct wlan_mlo_peer_context *ml_peer);

/**
 * wlan_mlo_peer_get_ref() - Get ref of MLO peer
 * @ml_peer: MLO peer
 *
 * This function gets ref of MLO peer
 *
 * Return: void
 */
static inline void wlan_mlo_peer_get_ref(struct wlan_mlo_peer_context *ml_peer)
{
	qdf_atomic_inc(&ml_peer->ref_cnt);
}

/**
 * wlan_mlo_peer_release_ref() - Release ref of MLO peer
 * @ml_peer: MLO peer
 *
 * This function releases ref of MLO peer, if ref is 0, invokes MLO peer free
 *
 * Return: void
 */
static inline void wlan_mlo_peer_release_ref(
					struct wlan_mlo_peer_context *ml_peer)
{
	if (qdf_atomic_dec_and_test(&ml_peer->ref_cnt))
		mlo_peer_cleanup(ml_peer);
}

/**
 * wlan_mlo_link_peer_attach() - MLO link peer attach
 * @ml_peer: MLO peer
 * @peer: Link peer
 * @frm_buf: Assoc resp buffer of non-assoc link
 *
 * This function attaches link peer to MLO peer
 *
 * Return: SUCCESS, if peer is successfully attached to MLO peer
 */
QDF_STATUS wlan_mlo_link_peer_attach(struct wlan_mlo_peer_context *ml_peer,
				     struct wlan_objmgr_peer *peer,
				     qdf_nbuf_t frm_buf);

/**
 * wlan_mlo_link_peer_delete() - MLO link peer delete
 * @peer: Link peer
 *
 * This function detaches link peer from MLO peer, if this peer is last link
 * peer, then MLO peer gets deleted
 *
 * Return: SUCCESS, if peer is detached from MLO peer
 */
QDF_STATUS wlan_mlo_link_peer_delete(struct wlan_objmgr_peer *peer);

/**
 * mlo_peer_get_link_peer_assoc_req_buf() - API to get link assoc req buffer
 * @peer: Object manager peer
 * @link_ix: link id of vdev
 *
 * Return: assoc req buffer
 */
qdf_nbuf_t mlo_peer_get_link_peer_assoc_req_buf(
			struct wlan_mlo_peer_context *ml_peer,
			uint8_t link_ix);

/**
 * mlo_peer_get_link_peer_assoc_resp_buf() - get MLO link peer assoc resp buf
 * @ml_peer: MLO peer
 * @link_ix: Link index of the link peer
 *
 * This function retrieves stored assoc resp buffer of link peer
 *
 * Return: resp_buf, if link_peer is available
 *         NULL, if link_peer is not present
 */
qdf_nbuf_t mlo_peer_get_link_peer_assoc_resp_buf(
		struct wlan_mlo_peer_context *ml_peer,
		uint8_t link_ix);

/**
 * wlan_mlo_peer_free_all_link_assoc_resp_buf() - Free all assoc resp buffers
 * @peer: Link peer
 *
 * This function frees all assoc resp link buffers
 *
 * Return: void
 */
void wlan_mlo_peer_free_all_link_assoc_resp_buf(struct wlan_objmgr_peer *peer);

/**
 * wlan_mlo_peer_get_links_info() - get MLO peer partner links info
 * @peer: Link peer
 * @ml_links: structure to be filled with partner link info
 *
 * This function retrieves partner link info of link peer such as hw link id,
 * vdev id
 *
 * Return: void
 */
void wlan_mlo_peer_get_links_info(struct wlan_objmgr_peer *peer,
				  struct mlo_tgt_partner_info *ml_links);

/**
 * wlan_mlo_peer_get_primary_peer_link_id() - get vdev link ID of primary peer
 * @peer: Link peer
 *
 * This function checks for the peers and returns vdev link id of the primary
 * peer.
 *
 * Return: link id of primary vdev
 */
uint8_t wlan_mlo_peer_get_primary_peer_link_id(struct wlan_objmgr_peer *peer);

/**
 * wlan_mlo_peer_get_partner_links_info() - get MLO peer partner links info
 * @peer: Link peer
 * @ml_links: structure to be filled with partner link info
 *
 * This function retrieves partner link info of link peer such as link id,
 * mac address
 *
 * Return: void
 */
void wlan_mlo_peer_get_partner_links_info(struct wlan_objmgr_peer *peer,
					  struct mlo_partner_info *ml_links);

/**
 ** APIs to operations on ML peer object
 */
typedef QDF_STATUS (*wlan_mlo_op_handler)(struct wlan_mlo_dev_context *ml_dev,
				    void *ml_peer,
				    void *arg);

/**
 * wlan_mlo_iterate_ml_peerlist() - iterate through all ml peer objects
 * @ml_dev: MLO DEV object
 * @handler: the handler will be called for each ml peer
 *            the handler should be implemented to perform required operation
 * @arg:     arguments passed by caller
 *
 * API to be used for performing the operations on all ML PEER objects
 *
 * Return: SUCCESS/FAILURE
 */
QDF_STATUS wlan_mlo_iterate_ml_peerlist(struct wlan_mlo_dev_context *ml_dev,
					wlan_mlo_op_handler handler,
					void *arg);

/**
 * wlan_mlo_get_mlpeer_by_linkmac() - find ML peer by Link MAC address
 * @ml_dev: MLO DEV object
 * @link_mac:  Link peer MAC address
 *
 * API to get ML peer using link MAC address
 *
 * Return: ML peer object, if it is found
 *         otherwise, returns NULL
 */
struct wlan_mlo_peer_context *wlan_mlo_get_mlpeer_by_linkmac(
				struct wlan_mlo_dev_context *ml_dev,
				struct qdf_mac_addr *link_mac);

/**
 * wlan_mlo_get_mlpeer_by_mld_mac() - find ML peer by MLD MAC address
 * @ml_dev: MLO DEV object
 * @mld_mac:  Peer MLD MAC address
 *
 * API to get ML peer using link MAC address
 *
 * Return: ML peer object, if it is found
 *         otherwise, returns NULL
 */
struct wlan_mlo_peer_context *wlan_mlo_get_mlpeer_by_mld_mac(
				struct wlan_mlo_dev_context *ml_dev,
				struct qdf_mac_addr *mld_mac);

/**
 * wlan_mlo_get_mlpeer_by_aid() - find ML peer by AID
 * @ml_dev: MLO DEV object
 * @aid:  AID
 *
 * API to get ML peer using AID
 *
 * Return: ML peer object, if it is found
 *         otherwise, returns NULL
 */
struct wlan_mlo_peer_context *wlan_mlo_get_mlpeer_by_aid(
				struct wlan_mlo_dev_context *ml_dev,
				uint16_t assoc_id);

/**
 * wlan_mlo_get_mlpeer_by_ml_peerid() - find ML peer by ML peer id
 * @ml_dev: MLO DEV object
 * @ml_peerid:  ML Peer ID
 *
 * API to get ML peer using ML peer id
 *
 * Return: ML peer object, if it is found
 *         otherwise, returns NULL
 */
struct wlan_mlo_peer_context *wlan_mlo_get_mlpeer_by_ml_peerid(
				struct wlan_mlo_dev_context *ml_dev,
				uint16_t ml_peerid);

/**
 * wlan_mlo_get_mlpeer() - find ML peer by MLD MAC address
 * @ml_dev: MLO DEV object
 * @ml_addr: MLO MAC address
 *
 * API to get ML peer using MLO MAC address
 *
 * Return: ML peer object, if it is found
 *         otherwise, returns NULL
 */
struct wlan_mlo_peer_context *wlan_mlo_get_mlpeer(
				struct wlan_mlo_dev_context *ml_dev,
				struct qdf_mac_addr *ml_addr);

/**
 * mlo_dev_mlpeer_attach() - Add ML PEER to ML peer list
 * @ml_dev: MLO DEV object
 * @ml_peer: ML peer
 *
 * API to attach ML PEER to MLD PEER table
 *
 * Return: SUCCESS, if it attached successfully
 *         otherwise, returns FAILURE
 */
QDF_STATUS mlo_dev_mlpeer_attach(struct wlan_mlo_dev_context *ml_dev,
				 struct wlan_mlo_peer_context *ml_peer);

/**
 * mlo_dev_mlpeer_detach() - Delete ML PEER from ML peer list
 * @ml_dev: MLO DEV object
 * @ml_peer: ML peer
 *
 * API to detach ML PEER from MLD PEER table
 *
 * Return: SUCCESS, if it detached successfully
 *         otherwise, returns FAILURE
 */
QDF_STATUS mlo_dev_mlpeer_detach(struct wlan_mlo_dev_context *ml_dev,
				 struct wlan_mlo_peer_context *ml_peer);

/**
 * mlo_dev_mlpeer_list_init() - Initialize ML peer list
 * @ml_dev: MLO DEV object
 *
 * API to initialize MLO peer list
 *
 * Return: SUCCESS, if initialized successfully
 */
QDF_STATUS mlo_dev_mlpeer_list_init(struct wlan_mlo_dev_context *ml_dev);

/**
 * mlo_dev_mlpeer_list_deinit() - destroys ML peer list
 * @ml_dev: MLO DEV object
 *
 * API to destroys MLO peer list
 *
 * Return: SUCCESS, if initialized successfully
 */
QDF_STATUS mlo_dev_mlpeer_list_deinit(struct wlan_mlo_dev_context *ml_dev);

/**
 * wlan_peer_is_mlo() - check whether peer is MLO link peer
 * @peer: link peer object
 *
 * API to check link peer is part of MLO peer or not
 *
 * Return: true if it MLO peer
 *         false, if it is not MLO peer
 */
static inline uint8_t wlan_peer_is_mlo(struct wlan_objmgr_peer *peer)
{
	return wlan_peer_mlme_flag_ext_get(peer, WLAN_PEER_FEXT_MLO);
}

/**
 * wlan_peer_set_mlo() - Set peer as MLO link peer
 * @peer: link peer object
 *
 * API to set MLO peer flag in link peer is part of MLO peer
 *
 * Return: void
 */
static inline void wlan_peer_set_mlo(struct wlan_objmgr_peer *peer)
{
	return wlan_peer_mlme_flag_ext_set(peer, WLAN_PEER_FEXT_MLO);
}

/**
 * wlan_peer_clear_mlo() - clear peer as MLO link peer
 * @peer: link peer object
 *
 * API to clear MLO peer flag in link peer
 *
 * Return: void
 */
static inline void wlan_peer_clear_mlo(struct wlan_objmgr_peer *peer)
{
	return wlan_peer_mlme_flag_ext_clear(peer, WLAN_PEER_FEXT_MLO);
}

#if defined(MESH_MODE_SUPPORT) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * wlan_mlo_peer_is_mesh() - Check if ml_peer is configured to operate as MESH
 * @ml_peer: MLO peer
 *
 * Return: TRUE if ml peer is configured as MESH
 */
bool wlan_mlo_peer_is_mesh(struct wlan_mlo_peer_context *ml_peer);
#else
static inline
bool wlan_mlo_peer_is_mesh(struct wlan_mlo_peer_context *ml_peer)
{
	return false;
}
#endif

#ifdef UMAC_SUPPORT_MLNAWDS
/**
 * wlan_mlo_peer_is_nawds() - Check if ml_peer is configured to operate as NAWDS
 * @ml_peer: MLO peer
 *
 * Return TRUE if ml peer is configured as NAWDS
 */
bool wlan_mlo_peer_is_nawds(struct wlan_mlo_peer_context *ml_peer);
#else
static inline
bool wlan_mlo_peer_is_nawds(struct wlan_mlo_peer_context *ml_peer)
{
	return false;
}
#endif
#ifdef UMAC_MLO_AUTH_DEFER
/**
 * mlo_peer_link_auth_defer() - Auth request defer for MLO peer
 * @ml_peer: ML peer
 * @link_mac:  Link peer MAC address
 * @auth_params: Defer Auth param
 *
 * This function saves Auth request params in MLO peer
 *
 * Return: SUCCESS if MAC address matches one of the link peers
 *         FAILURE, if MAC address doesn't match
 */
QDF_STATUS mlo_peer_link_auth_defer(struct wlan_mlo_peer_context *ml_peer,
				    struct qdf_mac_addr *link_mac,
				    struct mlpeer_auth_params *auth_params);

/**
 * mlo_peer_free_auth_param() - Free deferred Auth request params
 * @auth_params: Defer Auth param
 *
 * This function frees Auth request params
 *
 * Return: void
 */
void mlo_peer_free_auth_param(struct mlpeer_auth_params *auth_params);
#else
static inline void
mlo_peer_free_auth_param(struct mlpeer_auth_params *auth_params)
{
}
#endif

/**
 * wlan_mlo_partner_peer_delete_is_allowed() - Checks MLO peer delete is allowed
 * @src_peer: Link peer
 *
 * This function checks whether MLO peer can be deleted along with link peer
 * delete in link removal cases
 *
 * Return: true, if MLO peer can be deleted
 */
bool wlan_mlo_partner_peer_delete_is_allowed(struct wlan_objmgr_peer *src_peer);
#endif
