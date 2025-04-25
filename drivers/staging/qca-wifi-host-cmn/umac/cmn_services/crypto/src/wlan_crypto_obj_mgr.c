/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Public API initialization of crypto service with object manager
 */
#include <qdf_types.h>
#include <wlan_cmn.h>
#include <wlan_objmgr_cmn.h>

#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#include "wlan_crypto_def_i.h"
#include "wlan_crypto_main_i.h"
#include "wlan_crypto_obj_mgr_i.h"
#ifdef WLAN_CRYPTO_SUPPORT_FILS
#include "wlan_crypto_fils_api.h"
#endif

#define CRYPTO_MAX_HASH_IDX 16
#define CRYPTO_MAX_HASH_ENTRY 1024

static qdf_mutex_t crypto_lock;

extern const struct wlan_crypto_cipher
				*wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_MAX];

static inline int crypto_log2_ceil(unsigned int value)
{
	unsigned int tmp = value;
	int log2 = -1;

	crypto_info("crypto log value %d ", value);
	while (tmp) {
		log2++;
		tmp >>= 1;
	}
	if (1 << log2 != value)
		log2++;
	return log2;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static QDF_STATUS wlan_crypto_hash_init(struct crypto_psoc_priv_obj *psoc)
{
	int log2, hash_elems, i;

	log2 = crypto_log2_ceil(CRYPTO_MAX_HASH_IDX);
	hash_elems = 1 << log2;

	psoc->crypto_key_holder.mask = hash_elems - 1;
	psoc->crypto_key_holder.idx_bits = log2;

	/* allocate an array of TAILQ mec object lists */
	psoc->crypto_key_holder.bins = qdf_mem_malloc(
					hash_elems *
					sizeof(TAILQ_HEAD(anonymous_tail_q,
							  crypto_hash_entry)));

	if (!psoc->crypto_key_holder.bins)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < hash_elems; i++)
		TAILQ_INIT(&psoc->crypto_key_holder.bins[i]);

	qdf_mutex_create(&psoc->crypto_key_lock);

	return QDF_STATUS_SUCCESS;
}
#endif

static inline uint32_t crypto_hash_index(struct crypto_psoc_priv_obj *psoc,
					 union crypto_align_mac_addr *mac_addr,
					 uint8_t link_id)
{
	uint32_t index;

	index =
		mac_addr->align2.bytes_ab ^
		mac_addr->align2.bytes_cd ^
		mac_addr->align2.bytes_ef;
	index ^= link_id;
	index ^= index >> psoc->crypto_key_holder.idx_bits;
	index &= psoc->crypto_key_holder.mask;
	return index;
}

static inline int crypto_is_mac_addr_same(
	union crypto_align_mac_addr *mac_addr1,
	union crypto_align_mac_addr *mac_addr2)
{
	/*
	 * Intentionally use & rather than &&.
	 * because the operands are binary rather than generic boolean,
	 * the functionality is equivalent.
	 * Using && has the advantage of short-circuited evaluation,
	 * but using & has the advantage of no conditional branching,
	 * which is a more significant benefit.
	 */
	return ((mac_addr1->align4.bytes_abcd == mac_addr2->align4.bytes_abcd)
		& (mac_addr1->align4.bytes_ef == mac_addr2->align4.bytes_ef));
}

struct
wlan_crypto_key_entry *crypto_hash_find_by_linkid_and_macaddr(
			struct crypto_psoc_priv_obj *psoc,
			uint8_t link_id,
			uint8_t *mac_addr)
{
	union crypto_align_mac_addr local_mac_addr_aligned, *local_mac_addr;
	uint32_t index;
	struct wlan_crypto_key_entry *hash_entry;

	qdf_mem_copy(&local_mac_addr_aligned.raw[0],
		     mac_addr, QDF_MAC_ADDR_SIZE);
	local_mac_addr = &local_mac_addr_aligned;

	index = crypto_hash_index(psoc, local_mac_addr, link_id);
	TAILQ_FOREACH(hash_entry, &psoc->crypto_key_holder.bins[index],
		      hash_list_elem) {
		if (link_id == hash_entry->link_id &&
		    crypto_is_mac_addr_same(
					      local_mac_addr,
					      &hash_entry->mac_addr)) {
			crypto_debug("crypto found entry link id %d mac addr"
				     QDF_MAC_ADDR_FMT,
				     hash_entry->link_id,
				     QDF_MAC_ADDR_REF(mac_addr));
			return hash_entry;
		}
	}
	return NULL;
}

static inline void crypto_hash_add(struct crypto_psoc_priv_obj *psoc,
				   struct wlan_crypto_key_entry *hash_entry,
				   uint8_t link_id)
{
	uint32_t index;

	index = crypto_hash_index(psoc, &hash_entry->mac_addr, link_id);
	crypto_debug("crypto hash add index %d ", index);
	qdf_mutex_acquire(&psoc->crypto_key_lock);
	TAILQ_INSERT_TAIL(&psoc->crypto_key_holder.bins[index], hash_entry,
			  hash_list_elem);
	qdf_mutex_release(&psoc->crypto_key_lock);
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
QDF_STATUS wlan_crypto_add_key_entry(struct wlan_objmgr_psoc *psoc,
				     struct wlan_crypto_key_entry *new_entry)
{
	struct crypto_psoc_priv_obj *crypto_psoc_obj;
	uint8_t link_id = new_entry->link_id;
	struct wlan_crypto_key_entry *crypto_entry = NULL;

	crypto_psoc_obj =
		wlan_objmgr_psoc_get_comp_private_obj(psoc,
						      WLAN_UMAC_COMP_CRYPTO);
	if (!crypto_psoc_obj) {
		crypto_err("crypto_psoc_obj NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (qdf_unlikely(qdf_atomic_read(&crypto_psoc_obj->crypto_key_cnt) >=
					 CRYPTO_MAX_HASH_ENTRY)) {
		crypto_err("max crypto hash entry limit reached mac_addr: "
			   QDF_MAC_ADDR_FMT,
			   QDF_MAC_ADDR_REF(new_entry->mac_addr.raw));
		return QDF_STATUS_E_NOMEM;
	}

	crypto_debug("crypto add entry link id %d mac_addr: " QDF_MAC_ADDR_FMT,
		     link_id, QDF_MAC_ADDR_REF(new_entry->mac_addr.raw));

	qdf_mutex_acquire(&crypto_psoc_obj->crypto_key_lock);
	crypto_entry =
		crypto_hash_find_by_linkid_and_macaddr(crypto_psoc_obj, link_id,
						       new_entry->mac_addr.raw);
	qdf_mutex_release(&crypto_psoc_obj->crypto_key_lock);

	if (qdf_unlikely(crypto_entry))
		wlan_crypto_free_key_by_link_id(psoc,
						(struct qdf_mac_addr *)new_entry->mac_addr.raw,
						link_id);

	crypto_entry = qdf_mem_malloc(sizeof(*crypto_entry));
	if (!crypto_entry)
		return QDF_STATUS_E_NOMEM;

	*crypto_entry = *new_entry;

	crypto_hash_add(crypto_psoc_obj, crypto_entry, link_id);
	qdf_atomic_inc(&crypto_psoc_obj->crypto_key_cnt);
	crypto_entry->is_active = 1;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS crypto_add_entry(struct crypto_psoc_priv_obj *psoc,
			    uint8_t link_id,
			    uint8_t *mac_addr,
			    struct wlan_crypto_key *crypto_key,
			    uint8_t key_index)
{
	struct wlan_crypto_key_entry *crypto_entry = NULL;

	crypto_debug("crypto add entry link id %d mac_addr: " QDF_MAC_ADDR_FMT,
		     link_id, QDF_MAC_ADDR_REF(mac_addr));

	if (qdf_unlikely(qdf_atomic_read(&psoc->crypto_key_cnt) >=
					 CRYPTO_MAX_HASH_ENTRY)) {
		crypto_err("max crypto hash entry limit reached mac_addr: "
			   QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(mac_addr));
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mutex_acquire(&psoc->crypto_key_lock);
	crypto_entry = crypto_hash_find_by_linkid_and_macaddr(psoc, link_id,
							      mac_addr);
	qdf_mutex_release(&psoc->crypto_key_lock);

	if (qdf_likely(!crypto_entry)) {
		crypto_entry = (struct wlan_crypto_key_entry *)
			qdf_mem_malloc(sizeof(struct wlan_crypto_key_entry));

		if (qdf_unlikely(!crypto_entry))
			return QDF_STATUS_E_NOMEM;

		qdf_copy_macaddr((struct qdf_mac_addr *)&crypto_entry->mac_addr.raw[0],
				 (struct qdf_mac_addr *)mac_addr);
		crypto_entry->link_id = link_id;
		crypto_hash_add(psoc, crypto_entry, link_id);
		qdf_atomic_inc(&psoc->crypto_key_cnt);
		crypto_entry->is_active = 1;
	}

	if (key_index < WLAN_CRYPTO_MAXKEYIDX) {
		crypto_entry->keys.key[key_index] = crypto_key;
	} else if (is_igtk(key_index)) {
		crypto_entry->keys.igtk_key[key_index - WLAN_CRYPTO_MAXKEYIDX] =
		crypto_key;
		crypto_entry->keys.def_igtk_tx_keyid =
				key_index - WLAN_CRYPTO_MAXKEYIDX;
		crypto_entry->keys.igtk_key_type = crypto_key->cipher_type;
	} else {
		crypto_entry->keys.bigtk_key[key_index - WLAN_CRYPTO_MAXKEYIDX
				- WLAN_CRYPTO_MAXIGTKKEYIDX] = crypto_key;
		crypto_entry->keys.def_bigtk_tx_keyid =
				key_index - WLAN_CRYPTO_MAXKEYIDX
				- WLAN_CRYPTO_MAXIGTKKEYIDX;
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static void crypto_remove_entry(struct crypto_psoc_priv_obj *psoc,
				struct wlan_crypto_key_entry *crypto_entry,
				void *ptr)
{
	int i = 0;

	uint32_t index = crypto_hash_index(psoc, &crypto_entry->mac_addr,
					   crypto_entry->link_id);
	TAILQ_HEAD(, wlan_crypto_key_entry) * free_list = ptr;

	crypto_info("crypto remove entry key index %d link id %d",
		    index, crypto_entry->link_id);

	for (i = 0; i < WLAN_CRYPTO_MAX_VLANKEYIX; i++) {
		if (crypto_entry->keys.key[i]) {
			qdf_mem_free(crypto_entry->keys.key[i]);
			crypto_entry->keys.key[i] = NULL;
		}
	}

	for (i = 0; i < WLAN_CRYPTO_MAXIGTKKEYIDX; i++) {
		if (crypto_entry->keys.igtk_key[i]) {
			qdf_mem_free(crypto_entry->keys.igtk_key[i]);
			crypto_entry->keys.igtk_key[i] = NULL;
		}
	}

	for (i = 0; i < WLAN_CRYPTO_MAXBIGTKKEYIDX; i++) {
		if (crypto_entry->keys.bigtk_key[i]) {
			qdf_mem_free(crypto_entry->keys.bigtk_key[i]);
			crypto_entry->keys.bigtk_key[i] = NULL;
		}
	}
	/* Reset All key index as well */
	crypto_entry->keys.def_tx_keyid = 0;
	crypto_entry->keys.def_igtk_tx_keyid = 0;
	crypto_entry->keys.def_bigtk_tx_keyid = 0;

	TAILQ_REMOVE(&psoc->crypto_key_holder.bins[index], crypto_entry,
		     hash_list_elem);
	TAILQ_INSERT_TAIL(free_list, crypto_entry, hash_list_elem);
}

static void crypto_free_list(struct crypto_psoc_priv_obj *psoc, void *ptr)
{
	struct wlan_crypto_key_entry *crypto_entry, *hash_entry_next;

	TAILQ_HEAD(, wlan_crypto_key_entry) * free_list = ptr;

	TAILQ_FOREACH_SAFE(crypto_entry, free_list, hash_list_elem,
			   hash_entry_next) {
		crypto_debug("crypto delete for link_id %d mac_addr "
			     QDF_MAC_ADDR_FMT, crypto_entry->link_id,
			     QDF_MAC_ADDR_REF(crypto_entry->mac_addr.raw));
		qdf_mem_free(crypto_entry);
		if (!qdf_atomic_read(&psoc->crypto_key_cnt))
			crypto_debug("Invalid crypto_key_cnt %d",
				     psoc->crypto_key_cnt);
		else
			qdf_atomic_dec(&psoc->crypto_key_cnt);
	}
}

void crypto_flush_entries(struct wlan_objmgr_psoc *psoc)
{
	unsigned int index;
	struct wlan_crypto_key_entry *hash_entry, *hash_entry_next;
	struct crypto_psoc_priv_obj *crypto_priv;

	TAILQ_HEAD(, wlan_crypto_key_entry) free_list;
	TAILQ_INIT(&free_list);

	crypto_priv = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_CRYPTO);

	if (!crypto_priv)
		return;

	if (!crypto_priv->crypto_key_holder.mask)
		return;

	if (!crypto_priv->crypto_key_holder.bins)
		return;

	if (!qdf_atomic_read(&crypto_priv->crypto_key_cnt))
		return;

	qdf_mutex_acquire(&crypto_priv->crypto_key_lock);
	for (index = 0; index <= crypto_priv->crypto_key_holder.mask; index++) {
		if (!TAILQ_EMPTY(&crypto_priv->crypto_key_holder.bins[index])) {
			TAILQ_FOREACH_SAFE(
				hash_entry,
				&crypto_priv->crypto_key_holder.bins[index],
				hash_list_elem, hash_entry_next) {
				crypto_remove_entry(crypto_priv, hash_entry,
						    &free_list);
			}
		}
	}
	crypto_free_list(crypto_priv, &free_list);
	qdf_mutex_release(&crypto_priv->crypto_key_lock);
}

/**
 * wlan_crypto_hash_deinit() - This API deinit hash mechanism
 * @psoc: pointer to PSOC object
 *
 * Return: void
 */
static void wlan_crypto_hash_deinit(struct wlan_objmgr_psoc *psoc)
{
	struct crypto_psoc_priv_obj *crypto_priv;

	crypto_priv = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_CRYPTO);
	if (!crypto_priv) {
		crypto_err("failed to get crypto obj in psoc");
		return;
	}

	crypto_flush_entries(psoc);
	qdf_mem_free(crypto_priv->crypto_key_holder.bins);
	crypto_priv->crypto_key_holder.bins = NULL;
	qdf_mutex_destroy(&crypto_priv->crypto_key_lock);
}

static QDF_STATUS wlan_crypto_psoc_obj_create_handler(
				struct wlan_objmgr_psoc *psoc,
				void *arg)
{
	QDF_STATUS status;
	struct crypto_psoc_priv_obj *crypto_psoc_obj;

	crypto_psoc_obj = qdf_mem_malloc(sizeof(*crypto_psoc_obj));
	if (!crypto_psoc_obj)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						       WLAN_UMAC_COMP_CRYPTO,
						       (void *)crypto_psoc_obj,
						       QDF_STATUS_SUCCESS);

	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(crypto_psoc_obj);
		crypto_err("failed to attach crypto psoc priv object");
		return status;
	}

	status = wlan_crypto_hash_init(crypto_psoc_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_psoc_component_obj_detach(psoc,
						      WLAN_UMAC_COMP_CRYPTO,
						      crypto_psoc_obj);
		qdf_mem_free(crypto_psoc_obj);
		crypto_err("failed to hash init");
	}

	return status;
}

static QDF_STATUS wlan_crypto_psoc_obj_destroy_handler(
				struct wlan_objmgr_psoc *psoc,
				void *arg)
{
	QDF_STATUS status;
	struct crypto_psoc_priv_obj *crypto_psoc_obj;

	wlan_crypto_hash_deinit(psoc);

	crypto_psoc_obj = wlan_objmgr_psoc_get_comp_private_obj(
						psoc,
						WLAN_UMAC_COMP_CRYPTO);
	if (!crypto_psoc_obj) {
		crypto_err("failed to get crypto obj in psoc");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_CRYPTO,
						       crypto_psoc_obj);
	if (QDF_IS_STATUS_ERROR(status))
		crypto_err("failed to detach crypto psoc priv object");

	qdf_mem_free(crypto_psoc_obj);
	return status;
}
#else
void crypto_flush_entries(struct wlan_objmgr_psoc *psoc)
{
}
#endif

static QDF_STATUS wlan_crypto_register_all_ciphers(
					struct wlan_crypto_params *crypto_param)
{

	if (HAS_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_WEP)) {
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_WEP]
							= wep_register();
	}
	if (HAS_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_TKIP_MIC)) {
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_TKIP]
							= tkip_register();
	}
	if (HAS_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_AES)) {
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_AES_CCM]
							= ccmp_register();
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_AES_CCM_256]
							= ccmp256_register();
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_AES_GCM]
							= gcmp_register();
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_AES_GCM_256]
							= gcmp256_register();
	}
	if (HAS_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_WAPI_SMS4)) {
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_WAPI_SMS4]
							= wapi_register();
	}
	if (HAS_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_FILS_AEAD)) {
		wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_FILS_AEAD]
							= fils_register();
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wlan_crypto_vdev_obj_create_handler(
						struct wlan_objmgr_vdev *vdev,
						void *arg)
{
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_crypto_params *crypto_param;
	QDF_STATUS status;

	if (!vdev)
		return QDF_STATUS_E_INVAL;

	crypto_priv = qdf_mem_malloc(sizeof(struct wlan_crypto_comp_priv));
	if (!crypto_priv)
		return QDF_STATUS_E_NOMEM;

	crypto_param = &(crypto_priv->crypto_params);

	RESET_AUTHMODE(crypto_param);
	RESET_UCAST_CIPHERS(crypto_param);
	RESET_MCAST_CIPHERS(crypto_param);
	RESET_MGMT_CIPHERS(crypto_param);
	RESET_KEY_MGMT(crypto_param);
	RESET_CIPHER_CAP(crypto_param);

	pdev = wlan_vdev_get_pdev(vdev);
	wlan_pdev_obj_lock(pdev);
	if (wlan_pdev_nif_fw_cap_get(pdev, WLAN_SOC_C_WEP))
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_WEP);
	if (wlan_pdev_nif_fw_cap_get(pdev, WLAN_SOC_C_TKIP))
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_TKIP_MIC);
	if (wlan_pdev_nif_fw_cap_get(pdev, WLAN_SOC_C_AES)) {
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_AES);
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_CCM256);
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_GCM);
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_GCM_256);
	}
	if (wlan_pdev_nif_fw_cap_get(pdev, WLAN_SOC_C_CKIP))
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_CKIP);
	if (wlan_pdev_nif_fw_cap_get(pdev, WLAN_SOC_C_WAPI))
		SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_WAPI_SMS4);
	SET_CIPHER_CAP(crypto_param, WLAN_CRYPTO_CAP_FILS_AEAD);
	wlan_pdev_obj_unlock(pdev);
	/* update the crypto cipher table based on the fw caps*/
	/* update the fw_caps into ciphercaps then attach to objmgr*/
	wlan_crypto_register_all_ciphers(crypto_param);

	status = wlan_objmgr_vdev_component_obj_attach(vdev,
							WLAN_UMAC_COMP_CRYPTO,
							(void *)crypto_priv,
							QDF_STATUS_SUCCESS);
	if (status != QDF_STATUS_SUCCESS)
		qdf_mem_free(crypto_priv);

	return status;
}

static QDF_STATUS wlan_crypto_peer_obj_create_handler(
						struct wlan_objmgr_peer *peer,
						void *arg)
{
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_param;
	QDF_STATUS status;

	if (!peer)
		return QDF_STATUS_E_INVAL;

	crypto_priv = qdf_mem_malloc(sizeof(struct wlan_crypto_comp_priv));
	if (!crypto_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_peer_component_obj_attach(peer,
				WLAN_UMAC_COMP_CRYPTO, (void *)crypto_priv,
				QDF_STATUS_SUCCESS);

	if (status == QDF_STATUS_SUCCESS) {
		crypto_param = &crypto_priv->crypto_params;
		RESET_AUTHMODE(crypto_param);
		RESET_UCAST_CIPHERS(crypto_param);
		RESET_MCAST_CIPHERS(crypto_param);
		RESET_MGMT_CIPHERS(crypto_param);
		RESET_KEY_MGMT(crypto_param);
		RESET_CIPHER_CAP(crypto_param);
		if (wlan_vdev_get_selfpeer(peer->peer_objmgr.vdev) != peer) {
			wlan_crypto_set_peer_wep_keys(
					wlan_peer_get_vdev(peer), peer);
		}
	} else {
		crypto_err("peer obj failed status %d", status);
		qdf_mem_free(crypto_priv);
	}

	return status;
}

void wlan_crypto_free_key(struct wlan_crypto_keys *crypto_key)
{
	uint8_t i;

	if (!crypto_key) {
		crypto_err("given key ptr is NULL");
		return;
	}

	for (i = 0; i < WLAN_CRYPTO_MAX_VLANKEYIX; i++) {
		if (crypto_key->key[i]) {
			qdf_mem_free(crypto_key->key[i]);
			crypto_key->key[i] = NULL;
		}
	}

	for (i = 0; i < WLAN_CRYPTO_MAXIGTKKEYIDX; i++) {
		if (crypto_key->igtk_key[i]) {
			qdf_mem_free(crypto_key->igtk_key[i]);
			crypto_key->igtk_key[i] = NULL;
		}
	}

	for (i = 0; i < WLAN_CRYPTO_MAXBIGTKKEYIDX; i++) {
		if (crypto_key->bigtk_key[i]) {
			qdf_mem_free(crypto_key->bigtk_key[i]);
			crypto_key->bigtk_key[i] = NULL;
		}
	}

	/* Reset All key index as well */
	crypto_key->def_tx_keyid = 0;
	crypto_key->def_igtk_tx_keyid = 0;
	crypto_key->def_bigtk_tx_keyid = 0;
}

#ifdef CRYPTO_SET_KEY_CONVERGED
void wlan_crypto_free_vdev_key(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_crypto_comp_priv *crypto_priv;

	crypto_priv = wlan_get_vdev_crypto_obj(vdev);
	if (!crypto_priv) {
		crypto_err("crypto_priv NULL");
		return;
	}

	wlan_crypto_free_key(&crypto_priv->crypto_key);
}
#endif

void wlan_crypto_aquire_lock(void)
{
	qdf_mutex_acquire(&crypto_lock);
}

void wlan_crypto_release_lock(void)
{
	qdf_mutex_release(&crypto_lock);
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
void wlan_crypto_free_key_by_link_id(struct wlan_objmgr_psoc *psoc,
				     struct qdf_mac_addr *link_addr,
				     uint8_t link_id)
{
	struct wlan_crypto_key_entry *hash_entry;
	struct crypto_psoc_priv_obj *crypto_psoc_obj;

	TAILQ_HEAD(, wlan_crypto_key_entry) free_list;
	TAILQ_INIT(&free_list);

	crypto_psoc_obj = wlan_objmgr_psoc_get_comp_private_obj(
				psoc,
				WLAN_UMAC_COMP_CRYPTO);
	if (!crypto_psoc_obj) {
		crypto_err("crypto_psoc_obj NULL");
		return;
	}

	if (!crypto_psoc_obj->crypto_key_holder.mask)
		return;

	if (!crypto_psoc_obj->crypto_key_holder.bins)
		return;

	if (!qdf_atomic_read(&crypto_psoc_obj->crypto_key_cnt))
		return;

	qdf_mutex_acquire(&crypto_psoc_obj->crypto_key_lock);
	hash_entry = crypto_hash_find_by_linkid_and_macaddr(
					crypto_psoc_obj, link_id,
					(uint8_t *)link_addr);
	if (hash_entry) {
		crypto_remove_entry(crypto_psoc_obj, hash_entry, &free_list);
		crypto_free_list(crypto_psoc_obj, &free_list);
	}

	qdf_mutex_release(&crypto_psoc_obj->crypto_key_lock);
}

#endif
static QDF_STATUS wlan_crypto_vdev_obj_destroy_handler(
						struct wlan_objmgr_vdev *vdev,
						void *arg)
{
	struct wlan_crypto_comp_priv *crypto_priv;

	if (!vdev) {
		crypto_err("Vdev NULL");
		return QDF_STATUS_E_INVAL;
	}

	crypto_priv = (struct wlan_crypto_comp_priv *)
				wlan_get_vdev_crypto_obj(vdev);

	if (!crypto_priv) {
		crypto_err("crypto_priv NULL");
		return QDF_STATUS_E_INVAL;
	}

	wlan_objmgr_vdev_component_obj_detach(vdev,
						WLAN_UMAC_COMP_CRYPTO,
						(void *)crypto_priv);

	wlan_crypto_pmksa_flush(&crypto_priv->crypto_params);
	wlan_crypto_free_key(&crypto_priv->crypto_key);
	qdf_mem_free(crypto_priv);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wlan_crypto_peer_obj_destroy_handler(
						struct wlan_objmgr_peer *peer,
						void *arg)
{
	struct wlan_crypto_comp_priv *crypto_priv;

	if (!peer) {
		crypto_err("Peer NULL");
		return QDF_STATUS_E_INVAL;
	}
	crypto_priv = (struct wlan_crypto_comp_priv *)
				wlan_get_peer_crypto_obj(peer);
	if (!crypto_priv) {
		crypto_err("crypto_priv NULL");
		return QDF_STATUS_E_INVAL;
	}

	wlan_objmgr_peer_component_obj_detach(peer,
						WLAN_UMAC_COMP_CRYPTO,
						(void *)crypto_priv);
	wlan_crypto_free_key(&crypto_priv->crypto_key);
	qdf_mem_free(crypto_priv);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static int register_psoc_create_handler(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_psoc_obj_create_handler,
			NULL);
	return status;
}

static int register_psoc_destroy_handler(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_psoc_obj_destroy_handler,
			NULL);
	return status;
}

static int unregister_psoc_create_handler(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_psoc_obj_create_handler,
			NULL);
	return status;
}

static int unregister_psoc_destroy_handler(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_psoc_obj_destroy_handler,
			NULL);
	return status;
}

#else
static int register_psoc_create_handler(void)
{
	return QDF_STATUS_SUCCESS;
}

static int register_psoc_destroy_handler(void)
{
	return QDF_STATUS_SUCCESS;
}

static int unregister_psoc_create_handler(void)
{
	return QDF_STATUS_SUCCESS;
}

static int unregister_psoc_destroy_handler(void)
{
	return QDF_STATUS_SUCCESS;
}

#endif

QDF_STATUS __wlan_crypto_init(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Initialize crypto global lock*/
	qdf_mutex_create(&crypto_lock);

	status = register_psoc_create_handler();
	if (QDF_IS_STATUS_ERROR(status)) {
		crypto_err("psoc creation failure");
		return status;
	}

	status = wlan_objmgr_register_vdev_create_handler(
				WLAN_UMAC_COMP_CRYPTO,
				wlan_crypto_vdev_obj_create_handler, NULL);
	if (status != QDF_STATUS_SUCCESS)
		goto err_vdev_create;

	status = wlan_objmgr_register_peer_create_handler(
				WLAN_UMAC_COMP_CRYPTO,
				wlan_crypto_peer_obj_create_handler, NULL);
	if (status != QDF_STATUS_SUCCESS)
		goto err_peer_create;

	status = register_psoc_destroy_handler();
	if (QDF_IS_STATUS_ERROR(status)) {
		crypto_err("psoc destroy failure");
		goto err_psoc_delete;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
				WLAN_UMAC_COMP_CRYPTO,
				wlan_crypto_vdev_obj_destroy_handler, NULL);
	if (status != QDF_STATUS_SUCCESS)
		goto err_vdev_delete;

	status = wlan_objmgr_register_peer_destroy_handler(
				WLAN_UMAC_COMP_CRYPTO,
				wlan_crypto_peer_obj_destroy_handler, NULL);
	if (status != QDF_STATUS_SUCCESS)
		goto err_peer_delete;

	goto register_success;
err_peer_delete:
	wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_vdev_obj_destroy_handler, NULL);
err_vdev_delete:
	unregister_psoc_destroy_handler();
err_psoc_delete:
	wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_peer_obj_create_handler, NULL);
err_peer_create:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_vdev_obj_create_handler, NULL);
err_vdev_create:
	unregister_psoc_create_handler();
register_success:
	return status;
}

QDF_STATUS __wlan_crypto_deinit(void)
{
	if (unregister_psoc_create_handler()
			!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_vdev_obj_create_handler, NULL)
			!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_peer_obj_create_handler, NULL)
			!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_vdev_obj_destroy_handler, NULL)
			!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_peer_destroy_handler(WLAN_UMAC_COMP_CRYPTO,
			wlan_crypto_peer_obj_destroy_handler, NULL)
			!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	if (unregister_psoc_destroy_handler()
			!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	/* Destroy crypto global lock */
	qdf_mutex_destroy(&crypto_lock);

	return QDF_STATUS_SUCCESS;
}
