/*
 * Copyright (c) 2016-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WMI_UNIFIED_DBR_PARAM_H_
#define _WMI_UNIFIED_DBR_PARAM_H_

#define WMI_HOST_DBR_RING_ADDR_LO_S 0
#define WMI_HOST_DBR_RING_ADDR_LO_M 0xffffffff
#define WMI_HOST_DBR_RING_ADDR_LO \
	(WMI_HOST_DBR_RING_ADDR_LO_M << WMI_HOST_DBR_RING_ADDR_LO_S)

#define WMI_HOST_DBR_RING_ADDR_LO_GET(dword) \
			WMI_HOST_F_MS(dword, WMI_HOST_DBR_RING_ADDR_LO)
#define WMI_HOST_DBR_RING_ADDR_LO_SET(dword, val) \
			WMI_HOST_F_RMW(dword, val, WMI_HOST_DBR_RING_ADDR_LO)

#define WMI_HOST_DBR_RING_ADDR_HI_S 0
#define WMI_HOST_DBR_RING_ADDR_HI_M 0xf
#define WMI_HOST_DBR_RING_ADDR_HI \
	(WMI_HOST_DBR_RING_ADDR_HI_M << WMI_HOST_DBR_RING_ADDR_HI_S)

#define WMI_HOST_DBR_RING_ADDR_HI_GET(dword) \
			WMI_HOST_F_MS(dword, WMI_HOST_DBR_RING_ADDR_HI)
#define WMI_HOST_DBR_RING_ADDR_HI_SET(dword, val) \
			WMI_HOST_F_RMW(dword, val, WMI_HOST_DBR_RING_ADDR_HI)

#define WMI_HOST_DBR_DATA_ADDR_LO_S 0
#define WMI_HOST_DBR_DATA_ADDR_LO_M 0xffffffff
#define WMI_HOST_DBR_DATA_ADDR_LO \
	(WMI_HOST_DBR_DATA_ADDR_LO_M << WMI_HOST_DBR_DATA_ADDR_LO_S)

#define WMI_HOST_DBR_DATA_ADDR_LO_GET(dword) \
			WMI_HOST_F_MS(dword, WMI_HOST_DBR_DATA_ADDR_LO)
#define WMI_HOST_DBR_DATA_ADDR_LO_SET(dword, val) \
			WMI_HOST_F_RMW(dword, val, WMI_HOST_DBR_DATA_ADDR_LO)

#define WMI_HOST_DBR_DATA_ADDR_HI_S 0
#define WMI_HOST_DBR_DATA_ADDR_HI_M 0xf
#define WMI_HOST_DBR_DATA_ADDR_HI \
	(WMI_HOST_DBR_DATA_ADDR_HI_M << WMI_HOST_DBR_DATA_ADDR_HI_S)

#define WMI_HOST_DBR_DATA_ADDR_HI_GET(dword) \
			WMI_HOST_F_MS(dword, WMI_HOST_DBR_DATA_ADDR_HI)
#define WMI_HOST_DBR_DATA_ADDR_HI_SET(dword, val) \
			WMI_HOST_F_RMW(dword, val, WMI_HOST_DBR_DATA_ADDR_HI)

#define WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA_S 12
#define WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA_M 0x7ffff
#define WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA \
	(WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA_M << \
	 WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA_S)

#define WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA_GET(dword) \
		WMI_HOST_F_MS(dword, WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA)
#define WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA_SET(dword, val) \
		WMI_HOST_F_RMW(dword, val, WMI_HOST_DBR_DATA_ADDR_HI_HOST_DATA)

#define WMI_HOST_MAX_NUM_CHAINS 8

/**
 * struct direct_buf_rx_rsp: direct buffer rx response structure
 *
 * @pdev_id: Index of the pdev for which response is received
 * @mod_id: Index of the module for which respone is received
 * @num_buf_release_entry: Number of buffers released through event
 * @num_meta_data_entry: Number of meta data released
 * @num_cv_meta_data_entry: Number of cv meta data released
 * @num_cqi_meta_data_entry: Number of cqi meta data released
 * @dbr_entries: Pointer to direct buffer rx entry struct
 */
struct direct_buf_rx_rsp {
	uint32_t pdev_id;
	uint32_t mod_id;
	uint32_t num_buf_release_entry;
	uint32_t num_meta_data_entry;
	uint32_t num_cv_meta_data_entry;
	uint32_t num_cqi_meta_data_entry;
	struct direct_buf_rx_entry *dbr_entries;
};

/**
 * struct direct_buf_rx_cfg_req: direct buffer rx config request structure
 *
 * @pdev_id: Index of the pdev for which response is received
 * @mod_id: Index of the module for which respone is received
 * @base_paddr_lo: Lower 32bits of ring base address
 * @base_paddr_hi: Higher 32bits of ring base address
 * @head_idx_paddr_lo: Lower 32bits of head idx register address
 * @head_idx_paddr_hi: Higher 32bits of head idx register address
 * @tail_idx_paddr_lo: Lower 32bits of tail idx register address
 * @tail_idx_paddr_hi: Higher 32bits of tail idx register address
 * @buf_size: Size of the buffer for each pointer in the ring
 * @num_elems: Number of pointers allocated and part of the source ring
 * @event_timeout_ms:
 * @num_resp_per_event:
 */
struct direct_buf_rx_cfg_req {
	uint32_t pdev_id;
	uint32_t mod_id;
	uint32_t base_paddr_lo;
	uint32_t base_paddr_hi;
	uint32_t head_idx_paddr_lo;
	uint32_t head_idx_paddr_hi;
	uint32_t tail_idx_paddr_hi;
	uint32_t tail_idx_paddr_lo;
	uint32_t buf_size;
	uint32_t num_elems;
	uint32_t event_timeout_ms;
	uint32_t num_resp_per_event;
};

/**
 * struct direct_buf_rx_metadata: direct buffer metadata
 *
 * @noisefloor: noisefloor
 * @reset_delay: reset delay
 * @cfreq1: center frequency 1
 * @cfreq2: center frequency 2
 * @ch_width: channel width
 */
struct direct_buf_rx_metadata {
	int32_t noisefloor[WMI_HOST_MAX_NUM_CHAINS];
	uint32_t reset_delay;
	uint32_t cfreq1;
	uint32_t cfreq2;
	uint32_t ch_width;
};

/**
 * struct direct_buf_rx_cv_metadata: direct buffer metadata for TxBF CV upload
 *
 * @is_valid: Set cv metadata is valid,
 *            false if sw_peer_id is invalid or FCS error
 * @fb_type: Feedback type, 0 for SU 1 for MU
 * @asnr_len: Average SNR length
 * @asnr_offset: Average SNR offset
 * @dsnr_len: Delta SNR length
 * @dsnr_offset: Delta SNR offset
 * @peer_mac: Peer macaddr
 * @fb_params: Feedback params, [1:0] Nc [3:2] nss_num
 */
struct direct_buf_rx_cv_metadata {
	uint32_t is_valid;
	uint32_t fb_type;
	uint16_t asnr_len;
	uint16_t asnr_offset;
	uint16_t dsnr_len;
	uint16_t dsnr_offset;
	struct qdf_mac_addr peer_mac;
	uint32_t fb_params;
};

/*
 * In CQI data buffer, each user CQI data will be stored
 * in a fixed offset of 64 locations from each other,
 * and each location corresponds to 64-bit length.
 */
#define CQI_USER_DATA_LENGTH      (64 * 8)
#define CQI_USER_DATA_OFFSET(idx) ((idx) * CQI_USER_DATA_LENGTH)
#define MAX_NUM_CQI_USERS         3
/*
 * struct direct_buf_rx_cqi_per_user_info: Per user CQI data
 *
 * @asnr_len: Average SNR length
 * @asnr_offset: Average SNR offset
 * @fb_params: Feedback params, [1:0] Nc
 * @peer_mac: Peer macaddr
 */
struct direct_buf_rx_cqi_per_user_info {
	uint16_t asnr_len;
	uint16_t asnr_offset;
	uint32_t fb_params;
	struct qdf_mac_addr peer_mac;
};

/**
 * struct direct_buf_rx_cqi_metadata: direct buffer metadata for CQI upload
 *
 * @num_users: Number of user info in a metadta buffer
 * @is_valid: Set cqi metadata is valid,
 *            false if sw_peer_id is invalid or FCS error
 * @fb_type: Feedback type, 0 for SU 1 for MU 2 for CQI
 * @fb_params: Feedback params
 *	[0] is_valid0
 *	[1] is_valid1
 *	[2] is_valid2
 *	[4:3] Nc0
 *	[5:4] Nc1
 *	[6:5] Nc2
 * @user_info: Per user CQI info
 */
struct direct_buf_rx_cqi_metadata {
	uint8_t num_users;
	uint32_t is_valid;
	uint32_t fb_type;
	uint32_t fb_params;
	struct direct_buf_rx_cqi_per_user_info user_info[MAX_NUM_CQI_USERS];
};

/**
 * struct direct_buf_rx_entry: direct buffer rx release entry structure
 *
 * @paddr_lo: LSB 32-bits of the buffer
 * @paddr_hi: MSB 32-bits of the buffer
 * @len: Length of the buffer
 */
struct direct_buf_rx_entry {
	uint32_t paddr_lo;
	uint32_t paddr_hi;
	uint32_t len;
};

#endif /* _WMI_UNIFIED_DBR_PARAM_H_ */
