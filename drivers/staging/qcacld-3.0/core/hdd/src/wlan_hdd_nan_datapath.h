/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_nan_datapath.h
 *
 * WLAN Host Device Driver nan datapath API specification
 */
#ifndef __WLAN_HDD_NAN_DATAPATH_H
#define __WLAN_HDD_NAN_DATAPATH_H

struct hdd_context;
struct hdd_config;
struct hdd_adapter;
struct wireless_dev;

/* NAN Social frequencies */
#define NAN_SOCIAL_FREQ_2_4GHZ 2437
#define NAN_SOCIAL_FREQ_5GHZ_LOWER_BAND 5220
#define NAN_SOCIAL_FREQ_5GHZ_UPPER_BAND 5745

#define NDP_BROADCAST_STAID           (0)

#ifdef WLAN_FEATURE_NAN

#define WLAN_HDD_IS_NDI(adapter) ((adapter)->device_mode == QDF_NDI_MODE)

#define WLAN_HDD_IS_NDI_CONNECTED(adapter) ( \
	eConnectionState_NdiConnected ==\
		(adapter)->session.station.conn_info.conn_state)

void hdd_nan_datapath_target_config(struct hdd_context *hdd_ctx,
						struct wma_tgt_cfg *cfg);
void hdd_ndp_event_handler(struct hdd_adapter *adapter,
			   struct csr_roam_info *roam_info,
			   eRoamCmdStatus roam_status,
			   eCsrRoamResult roam_result);
int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);
int hdd_init_nan_data_mode(struct hdd_adapter *adapter);
void hdd_ndp_session_end_handler(struct hdd_adapter *adapter);

/**
 * hdd_cleanup_ndi() - Cleanup NDI state/resources
 * @hdd_ctx: HDD context
 * @adapter: Pointer to the NDI adapter
 *
 * Cleanup NDI state/resources allocated when NDPs are created on that NDI.
 *
 * Return: None
 */

void hdd_cleanup_ndi(struct hdd_context *hdd_ctx,
		     struct hdd_adapter *adapter);

/**
 * hdd_ndi_start() - Start NDI adapter and create NDI vdev
 * @iface_name: NDI interface name
 * @transaction_id: Transaction id given by framework to start the NDI.
 *                  Framework expects this in the immediate response when
 *                  the NDI is created by it.
 *
 * Create NDI move interface and vdev.
 *
 * Return: 0 upon success
 */
int hdd_ndi_start(const char *iface_name, uint16_t transaction_id);

enum nan_datapath_state;
struct nan_datapath_inf_create_rsp;

/**
 * hdd_ndi_open() - Open NDI interface
 * @iface_name: NDI interface name
 * @is_add_virtual_iface: is this interface getting created through add virtual
 * interface
 *
 * Return: 0 on success, error code on failure
 */
int hdd_ndi_open(const char *iface_name, bool is_add_virtual_iface);

/**
 * hdd_ndi_delete() - Delete NDI interface
 * @vdev_id: vdev id of the NDI interface
 * @iface_name: NDI interface name
 * @transaction_id: Transaction id
 *
 * Return: 0 on success, error code on failure
 */
int hdd_ndi_delete(uint8_t vdev_id, const char *iface_name,
		   uint16_t transaction_id);

/**
 * hdd_ndi_close() - Close NDI interface
 * @vdev_id: vdev id of the NDI interface
 *
 * Return: None
 */
void hdd_ndi_close(uint8_t vdev_id);

/**
 * hdd_ndi_drv_ndi_create_rsp_handler() - ndi create response handler
 * @vdev_id: vdev id of the NDI interface
 * @ndi_rsp: NDI create response
 *
 * Return: None
 */
void hdd_ndi_drv_ndi_create_rsp_handler(uint8_t vdev_id,
					struct nan_datapath_inf_create_rsp *ndi_rsp);

/**
 * hdd_ndi_drv_ndi_delete_rsp_handler() - ndi delete response handler
 * @vdev_id: vdev id of the NDI interface
 *
 * Return: None
 */
void hdd_ndi_drv_ndi_delete_rsp_handler(uint8_t vdev_id);

/**
 * hdd_ndp_new_peer_handler() - NDP new peer indication handler
 * @vdev_id: vdev id
 * @sta_id: STA ID
 * @peer_mac_addr: MAC address of the peer
 * @first_peer: Indicates if it is first peer
 *
 * Return: 0 on success, error code on failure
 */
int hdd_ndp_new_peer_handler(uint8_t vdev_id, uint16_t sta_id,
			     struct qdf_mac_addr *peer_mac_addr,
			     bool first_peer);

/**
 * hdd_ndp_peer_departed_handler() - Handle NDP peer departed indication
 * @vdev_id: vdev id
 * @sta_id: STA ID
 * @peer_mac_addr: MAC address of the peer
 * @last_peer: Indicates if it is last peer
 *
 * Return: None
 */
void hdd_ndp_peer_departed_handler(uint8_t vdev_id, uint16_t sta_id,
				   struct qdf_mac_addr *peer_mac_addr,
				   bool last_peer);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
/**
 * hdd_ndi_set_mode() - set the adapter mode to NDI
 * @iface_name: NDI interface name
 *
 * The adapter mode is STA while creating virtual interface.
 * mode is set to NDI while creating NDI.
 *
 * Return: 0 upon success
 */
int hdd_ndi_set_mode(const char *iface_name);
#else
static inline int hdd_ndi_set_mode(const char *iface_name)
{
	return 0;
}
#endif /* LINUX_VERSION_CODE  */

#else
#define WLAN_HDD_IS_NDI(adapter)	(false)
#define WLAN_HDD_IS_NDI_CONNECTED(adapter) (false)

static inline void hdd_nan_datapath_target_config(struct hdd_context *hdd_ctx,
						struct wma_tgt_cfg *cfg)
{
}
static inline void hdd_ndp_event_handler(struct hdd_adapter *adapter,
					 struct csr_roam_info *roam_info,
					 eRoamCmdStatus roam_status,
					 eCsrRoamResult roam_result)
{
}
static inline int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	return 0;
}
static inline int hdd_init_nan_data_mode(struct hdd_adapter *adapter)
{
	return 0;
}
static inline void hdd_ndp_session_end_handler(struct hdd_adapter *adapter)
{
}

static inline void hdd_cleanup_ndi(struct hdd_context *hdd_ctx,
				   struct hdd_adapter *adapter)
{
}

static inline int hdd_ndi_start(const char *iface_name, uint16_t transaction_id)
{
	return 0;
}

static inline int hdd_ndi_set_mode(const char *iface_name)
{
	return 0;
}

enum nan_datapath_state;
struct nan_datapath_inf_create_rsp;

static inline int
hdd_ndi_open(const char *iface_name, bool is_add_virtual_iface)
{
	return 0;
}

static inline int
hdd_ndi_delete(uint8_t vdev_id, const char *iface_name, uint16_t transaction_id)
{
	return 0;
}

static inline void hdd_ndi_close(uint8_t vdev_id)
{
}

static inline void
hdd_ndi_drv_ndi_create_rsp_handler(uint8_t vdev_id,
				   struct nan_datapath_inf_create_rsp *ndi_rsp)
{
}

static inline void hdd_ndi_drv_ndi_delete_rsp_handler(uint8_t vdev_id)
{
}

static inline int hdd_ndp_new_peer_handler(uint8_t vdev_id, uint16_t sta_id,
					   struct qdf_mac_addr *peer_mac_addr,
					   bool first_peer)
{
	return 0;
}

static inline void hdd_ndp_peer_departed_handler(uint8_t vdev_id,
						 uint16_t sta_id,
						 struct qdf_mac_addr *peer_mac_addr,
						 bool last_peer)
{
}
#endif /* WLAN_FEATURE_NAN */

#endif /* __WLAN_HDD_NAN_DATAPATH_H */
