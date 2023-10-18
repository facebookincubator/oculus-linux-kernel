/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *  DOC: cfg_mgmt_rx_reo.h
 *  This file contains cfg definitions of mgmt rx reo sub-component
 */

#ifndef __CFG_MGMT_RX_REO_H
#define __CFG_MGMT_RX_REO_H

#ifdef WLAN_MGMT_RX_REO_SUPPORT

/*
 * <ini>
 * mgmt_rx_reo_enable - Enable MGMT Rx REO feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable MGMT Rx REO feature
 *
 * Related: None
 *
 * Supported Feature: MGMT Rx REO
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MGMT_RX_REO_ENABLE \
	CFG_INI_BOOL("mgmt_rx_reo_enable", false, \
			"Enable MGMT Rx REO feature")

/*
 * <ini>
 * mgmt_rx_reo_pkt_ctr_delta_thresh - Packet counter delta threshold
 * @Min: 0
 * @Max: 0xFFFF
 * @Default: 0
 *
 * This ini is used to configure the maximum allowed delta between subsequent
 * management frames of a given link. Configurations are as follows:
 * 0 - No restrictions in the delta value
 * >= 1 - Assert the system if the delta between the packet counter values of
 * subsequent frames of a given link crosses this value.
 *
 * Related: None
 *
 * Supported Feature: MGMT Rx REO
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MGMT_RX_REO_PKT_CTR_DELTA_THRESH CFG_INI_UINT(\
		"mgmt_rx_reo_pkt_ctr_delta_thresh",\
		0, 0xFFFF, 0,\
		CFG_VALUE_OR_DEFAULT, "Packet counter delta threshold")

/*
 * <ini>
 * mgmt_rx_reo_ingress_frame_debug_list_size - Size of the list which logs the
 * incoming management frames
 * @Min: 0
 * @Max: WLAN_MGMT_RX_REO_INGRESS_FRAME_DEBUG_LIST_SIZE_MAX
 * @Default: WLAN_MGMT_RX_REO_INGRESS_FRAME_DEBUG_LIST_SIZE_DEFAULT
 *
 * This ini is used to configure the size of the list which logs the incoming
 * management frames.
 *
 * Related: None
 *
 * Supported Feature: MGMT Rx REO
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MGMT_RX_REO_INGRESS_FRAME_DEBUG_LIST_SIZE CFG_INI_UINT(\
		"mgmt_rx_reo_ingress_frame_debug_list_size",\
		0, WLAN_MGMT_RX_REO_INGRESS_FRAME_DEBUG_LIST_SIZE_MAX,\
		WLAN_MGMT_RX_REO_INGRESS_FRAME_DEBUG_LIST_SIZE_DEFAULT,\
		CFG_VALUE_OR_CLAMP, "Size of ingress frame debug list")

/*
 * <ini>
 * mgmt_rx_reo_egress_frame_debug_list_size - Size of the list which logs the
 * outgoing management frames
 * @Min: 0
 * @Max: WLAN_MGMT_RX_REO_EGRESS_FRAME_DEBUG_LIST_SIZE_MAX
 * @Default: WLAN_MGMT_RX_REO_EGRESS_FRAME_DEBUG_LIST_SIZE_DEFAULT
 *
 * This ini is used to configure the size of the list which logs the outgoing
 * management frames.
 *
 * Related: None
 *
 * Supported Feature: MGMT Rx REO
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MGMT_RX_REO_EGRESS_FRAME_DEBUG_LIST_SIZE CFG_INI_UINT(\
		"mgmt_rx_reo_egress_frame_debug_list_size",\
		0, WLAN_MGMT_RX_REO_EGRESS_FRAME_DEBUG_LIST_SIZE_MAX,\
		WLAN_MGMT_RX_REO_EGRESS_FRAME_DEBUG_LIST_SIZE_DEFAULT,\
		CFG_VALUE_OR_CLAMP, "Size of egress frame debug list")

#define CFG_MGMT_RX_REO_ALL \
	CFG(CFG_MGMT_RX_REO_ENABLE) \
	CFG(CFG_MGMT_RX_REO_PKT_CTR_DELTA_THRESH) \
	CFG(CFG_MGMT_RX_REO_INGRESS_FRAME_DEBUG_LIST_SIZE) \
	CFG(CFG_MGMT_RX_REO_EGRESS_FRAME_DEBUG_LIST_SIZE)
#else

#define CFG_MGMT_RX_REO_ALL

#endif /* WLAN_MGMT_RX_REO_SUPPORT */
#endif /* __CFG_MGMT_RX_REO_H */
