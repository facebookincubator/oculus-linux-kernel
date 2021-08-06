/*
 * Copyright (c) 2016-2018, 2020 The Linux Foundation. All rights reserved.
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
struct hdd_tgt_cfg;
struct hdd_config;
struct hdd_adapter;
struct wireless_dev;

/* NAN Social channels */
#define NAN_SOCIAL_CHANNEL_2_4GHZ 6
#define NAN_SOCIAL_CHANNEL_5GHZ_LOWER_BAND 44
#define NAN_SOCIAL_CHANNEL_5GHZ_UPPER_BAND 149

#define NDP_BROADCAST_STAID           (0)

#ifdef WLAN_FEATURE_NAN_DATAPATH
#define WLAN_HDD_IS_NDI(adapter) ((adapter)->device_mode == QDF_NDI_MODE)

#define WLAN_HDD_IS_NDI_CONNECTED(adapter) ( \
	eConnectionState_NdiConnected ==\
		(adapter)->session.station.conn_info.connState)
#else
#define WLAN_HDD_IS_NDI(adapter)	(false)
#define WLAN_HDD_IS_NDI_CONNECTED(adapter) (false)
#endif /* WLAN_FEATURE_NAN_DATAPATH */

#define NAN_MSG_ID_DISABLE_INDICATION 26
/**
 * struct nan_msg_hdr - NAN msg header to be sent to userspace
 * @msg_version: NAN msg version
 * @msg_id: NAN message id
 * @reserved: Reserved for now to avoid padding
 *
 * 8-byte control message header used by NAN
 *
 */
struct nan_msg_hdr {
	uint16_t msg_version:4;
	uint16_t msg_id:12;
	uint16_t reserved[3];
};

#define NAN_STATUS_SUCCESS 0
#define NAN_STATUS_UNSUPPORTED_CONCURRENCY_NAN_DISABLED 12

/**
 * struct nan_disable_ind_msg - NAN disable ind params
 * @msg_hdr: NAN msg header
 * @reason: NAN disable reason, below are valid reasons for NAN disable ind
 *          NAN_STATUS_SUCCESS
 *          NAN_STATUS_UNSUPPORTED_CONCURRENCY_NAN_DISABLED
 * @reserved: Reserved for now to avoid padding
 */
struct nan_disable_ind_msg {
	struct nan_msg_hdr msg_hdr;
	uint16_t reason;
	uint16_t reserved;
};
#ifdef WLAN_FEATURE_NAN_DATAPATH
void hdd_ndp_print_ini_config(struct hdd_context *hdd_ctx);
void hdd_nan_datapath_target_config(struct hdd_context *hdd_ctx,
						struct wma_tgt_cfg *cfg);
void hdd_ndp_event_handler(struct hdd_adapter *adapter,
			   struct csr_roam_info *roam_info,
			   uint32_t roam_id, eRoamCmdStatus roam_status,
			   eCsrRoamResult roam_result);
int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);
int hdd_init_nan_data_mode(struct hdd_adapter *adapter);
void hdd_ndp_session_end_handler(struct hdd_adapter *adapter);

/**
 * hdd_cleanup_ndi(): Cleanup NDI state/resources
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
 * hdd_ndi_start(): Start NDI adapter and create NDI vdev
 * @iface_name: NDI interface name
 * @transaction_id: Transaction id given by framework to start the NDI.
 *                  Framework expects this in the immediate response when
 *                  the NDI is created by it.
 *
 * Create NDI mode interface and vdev.
 *
 * Return: 0 upon success
 */
int hdd_ndi_start(char *iface_name, uint16_t transaction_id);
#else
static inline void hdd_ndp_print_ini_config(struct hdd_context *hdd_ctx)
{
}
static inline void hdd_nan_datapath_target_config(struct hdd_context *hdd_ctx,
						struct wma_tgt_cfg *cfg)
{
}
static inline void hdd_ndp_event_handler(struct hdd_adapter *adapter,
					 struct csr_roam_info *roam_info,
					 uint32_t roam_id,
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

static inline int hdd_ndi_start(char *iface_name, uint16_t transaction_id)
{
	return 0;
}
#endif /* WLAN_FEATURE_NAN_DATAPATH */

enum nan_datapath_state;
struct nan_datapath_inf_create_rsp;

int hdd_ndi_open(char *iface_name);
int hdd_ndi_delete(uint8_t vdev_id, char *iface_name, uint16_t transaction_id);
void hdd_ndi_close(uint8_t vdev_id);
void hdd_ndi_drv_ndi_create_rsp_handler(uint8_t vdev_id,
			       struct nan_datapath_inf_create_rsp *ndi_rsp);
void hdd_ndi_drv_ndi_delete_rsp_handler(uint8_t vdev_id);
int hdd_ndp_get_peer_idx(uint8_t vdev_id, struct qdf_mac_addr *addr);
int hdd_ndp_new_peer_handler(uint8_t vdev_id, uint16_t sta_id,
			struct qdf_mac_addr *peer_mac_addr, bool fist_peer);
void hdd_ndp_peer_departed_handler(uint8_t vdev_id, uint16_t sta_id,
			struct qdf_mac_addr *peer_mac_addr, bool last_peer);
#endif /* __WLAN_HDD_NAN_DATAPATH_H */
