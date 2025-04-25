/*
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
 * DOC: wlan_osif_features.h
 *
 * Define feature flags to cleanly describe when features
 * are present in a given version of the kernel
 */

#ifndef _WLAN_OSIF_FEATURES_H_
#define _WLAN_OSIF_FEATURES_H_

#include <linux/version.h>

/*
 * CFG80211_11BE_BASIC
 * Used to indicate the Linux Kernel contains support for basic 802.11be
 * definitions.
 *
 * These definitions were introduced in Linux Kernel 5.18 via:
 * cbc1ca0a9d0a ieee80211: Add EHT (802.11be) definitions
 * 2a2c86f15e17 ieee80211: add EHT 1K aggregation definitions
 * 5cd5a8a3e2fb cfg80211: Add data structures to capture EHT capabilities
 * 3743bec6120a cfg80211: Add support for EHT 320 MHz channel width
 * cfb14110acf8 nl80211: add EHT MCS support
 * c2b3d7699fb0 nl80211: add support for 320MHz channel limitation
 * 31846b657857 cfg80211: add NO-EHT flag to regulatory
 * ea05fd3581d3 cfg80211: Support configuration of station EHT capabilities
 *
 * These definitions were backported to Android Common Kernel 5.15 via:
 * https://android-review.googlesource.com/c/kernel/common/+/1996261
 * https://android-review.googlesource.com/c/kernel/common/+/1996262
 * https://android-review.googlesource.com/c/kernel/common/+/1996263
 * https://android-review.googlesource.com/c/kernel/common/+/1996264
 * https://android-review.googlesource.com/c/kernel/common/+/1996265
 * https://android-review.googlesource.com/c/kernel/common/+/1996266
 * https://android-review.googlesource.com/c/kernel/common/+/1996267
 * https://android-review.googlesource.com/c/kernel/common/+/1996268
 */

#if (defined(__ANDROID_COMMON_KERNEL__) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0))) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0))
#define CFG80211_11BE_BASIC 1
#endif

/*
 * CFG80211_SA_QUERY_OFFLOAD_SUPPORT
 * Used to indicate the Linux Kernel contains support to offload SA Query
 * procedures for AP SME device
 *
 * This feature was introduced in Linux Kernel 5.17 via:
 * 47301a74bbfa ("nl80211: Add support to set AP settings flags with single attribute")
 * 87c1aec15dee ("nl80211: Add support to offload SA Query procedures for AP SME device")
 *
 * This feature was backported to Android Common Kernel 5.15 via:
 * https://android-review.googlesource.com/c/kernel/common/+/1958439
 * https://android-review.googlesource.com/c/kernel/common/+/1958440
 */

#if (defined(__ANDROID_COMMON_KERNEL__) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0))) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0))
#define CFG80211_SA_QUERY_OFFLOAD_SUPPORT 1
#endif

/*
 * CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT
 * Used to indicate the Linux Kernel contains support for single netdevice multi
 * link support.
 *
 * This feature was merged into wireless-next via below commits:
 * 7b0a0e3c3 wifi: cfg80211: do some rework towards MLO link APIs
 * 0f7594489 wifi: cfg80211: mlme: get BSS entry outside cfg80211_mlme_assoc()
 * 9ecff10e8 wifi: nl80211: refactor BSS lookup in nl80211_associate()
 * 0f48b8b88 wifi: ieee80211: add definitions for multi-link element
 * 325839da9 wifi: cfg80211: simplify cfg80211_mlme_auth() prototype
 * d648c2302 wifi: nl80211: support MLO in auth/assoc
 *
 * This feature was backported to Android Common Kernel 5.15 via:
 * https://android-review.googlesource.com/c/kernel/common/+/2123895
 * https://android-review.googlesource.com/c/kernel/common/+/2115618
 * https://android-review.googlesource.com/c/kernel/common/+/2115620
 * https://android-review.googlesource.com/c/kernel/common/+/2121347
 * https://android-review.googlesource.com/c/kernel/common/+/2121348
 * https://android-review.googlesource.com/c/kernel/common/+/2121349
 * https://android-review.googlesource.com/c/kernel/common/+/2121350
 * https://android-review.googlesource.com/c/kernel/common/+/2121351
 * https://android-review.googlesource.com/c/kernel/common/+/2123452
 * https://android-review.googlesource.com/c/kernel/common/+/2123454
 * https://android-review.googlesource.com/c/kernel/common/+/2115621
 *
 */
#if ((defined(__ANDROID_COMMON_KERNEL__) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
	(defined  IEEE80211_MLD_MAX_NUM_LINKS)) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)))
#define CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT 1
#endif

/**
 * CFG80211_SAE_AUTH_TA_ADDR_SUPPORT
 * Used to indicate the Linux Kernel contains support for ML SAE auth with link
 * address as the transmitter address
 *
 * TODO: These changes are currently in internal review once upstreamed and
 * backported to 5.15 need to add the respective commit-ids
 */
#if (defined(__ANDROID_COMMON_KERNEL__) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
	(defined  CFG80211_EXTERNAL_AUTH_TA_SUPPORT))
#define CFG80211_SAE_AUTH_TA_ADDR_SUPPORT 1
#endif

/*
 * CFG80211_MULTI_AKM_CONNECT_SUPPORT
 * used to indicate the Linux kernel contains support for multi AKM connect
 * support
 *
 * This feature was introduced in Linux Kernel 6.0 via:
 * ecad3b0b99bf wifi: cfg80211: Increase akm_suites array size in
 * cfg80211_crypto_settings.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0) || \
	(defined CFG80211_MAX_NUM_AKM_SUITES))
#define CFG80211_MULTI_AKM_CONNECT_SUPPORT 1
#endif

/*
 * WLAN_MLD_AP_STA_CONNECT_SUPPORT
 * Used to indicate Linux Kernel supports ML connection on SAP.
 */
#if (defined(__ANDROID_COMMON_KERNEL__) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
	(defined CFG80211_MLD_AP_STA_CONNECT_SUPPORT))
#define WLAN_MLD_AP_STA_CONNECT_SUPPORT 1
#endif

/*
 * WLAN_MLD_AP_OWE_INFO_SUPPORT
 * Used to indicate Linux Kernel supports ML OWE connection
 * on SAP
 */
#if (defined(__ANDROID_COMMON_KERNEL__) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
	(defined CFG80211_MLD_AP_OWE_INFO_SUPPORT))
#define WLAN_MLD_AP_OWE_INFO_SUPPORT 1
#endif

/*
 * CFG80211_TX_CONTROL_PORT_LINK_SUPPORT
 * Used to indicate Linux kernel contains support for TX control port from
 * specific link ID
 *
 * This feature was introduced in Linux Kernel 6.0 via:
 * 9b6bf4d6120a wifi: nl80211: set BSS to NULL if IS_ERR()
 * 45aaf17c0c34 wifi: nl80211: check MLO support in authenticate
 * d2bc52498b6b wifi: nl80211: Support MLD parameters in nl80211_set_station()
 * 67207bab9341 wifi: cfg80211/mac80211: Support control port TX from specific link
 * 69c3f2d30c35 wifi: nl80211: allow link ID in set_wiphy with frequency
 * fa2ca639c4e6 wifi: nl80211: advertise MLO support
 * e3d331c9b620 wifi: cfg80211: set country_elem to NULL
 * 34d76a14f8f7 wifi: nl80211: reject link specific elements on assoc link
 * df35f3164ec1 wifi: nl80211: reject fragmented and non-inheritance elements
 * ff5c4dc4cd78 wifi: nl80211: fix some attribute policy entries
 * 7464f665158e wifi: cfg80211: add cfg80211_get_iftype_ext_capa()
 * 8876c67e6296 wifi: nl80211: require MLD address on link STA add/modify
 * 9dd1953846c7 wifi: nl80211/mac80211: clarify link ID in control port TX
 * 00b3d8401019 wifi: cfg80211/nl80211: move rx management data into a struct
 * 6074c9e57471 wifi: cfg80211: report link ID in NL80211_CMD_FRAME
 * 95f498bb49f7 wifi: nl80211: add MLO link ID to the NL80211_CMD_FRAME TX API
 * 1e0b3b0b6cb5 wifi: mac80211: Align with Draft P802.11be_D1.5
 * 062e8e02dfd4 wifi: mac80211: Align with Draft P802.11be_D2.0
 * d776763f4808 wifi: cfg80211: debugfs: fix return type in ht40allow_map_read()
 * 64e966d1e84b wifi: cfg80211: fix MCS divisor value
 * 4e9c3af39820 wifi: nl80211: add EML/MLD capabilities to per-iftype capabilities
 * 80b0ed70a271 wifi: nl80211: add RX and TX timestamp attributes
 * ea7d50c925ce wifi: cfg80211: add a function for reporting TX status with hardware timestamps
 * 1ff715ffa0ec wifi: cfg80211: add hardware timestamps to frame RX info
 *
 * This feature was backported to Android Common Kernel 5.15.74 via:
 * https://android-review.googlesource.com/c/kernel/common/+/2253173
 * https://android-review.googlesource.com/c/kernel/common/+/2253174
 * https://android-review.googlesource.com/c/kernel/common/+/2253175
 * https://android-review.googlesource.com/c/kernel/common/+/2253176
 * https://android-review.googlesource.com/c/kernel/common/+/2253177
 * https://android-review.googlesource.com/c/kernel/common/+/2253178
 * https://android-review.googlesource.com/c/kernel/common/+/2253179
 * https://android-review.googlesource.com/c/kernel/common/+/2253180
 * https://android-review.googlesource.com/c/kernel/common/+/2253181
 * https://android-review.googlesource.com/c/kernel/common/+/2253182
 * https://android-review.googlesource.com/c/kernel/common/+/2253183
 * https://android-review.googlesource.com/c/kernel/common/+/2253184
 * https://android-review.googlesource.com/c/kernel/common/+/2253185
 * https://android-review.googlesource.com/c/kernel/common/+/2253186
 * https://android-review.googlesource.com/c/kernel/common/+/2253187
 * https://android-review.googlesource.com/c/kernel/common/+/2253188
 * https://android-review.googlesource.com/c/kernel/common/+/2253189
 * https://android-review.googlesource.com/c/kernel/common/+/2253190
 * https://android-review.googlesource.com/c/kernel/common/+/2253191
 * https://android-review.googlesource.com/c/kernel/common/+/2253192
 * https://android-review.googlesource.com/c/kernel/common/+/2253193
 * https://android-review.googlesource.com/c/kernel/common/+/2253194
 * https://android-review.googlesource.com/c/kernel/common/+/2267469
 * https://android-review.googlesource.com/c/kernel/common/+/2267204
 * https://android-review.googlesource.com/c/kernel/common/+/2267210
 */

#if (defined(__ANDROID_COMMON_KERNEL__) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 74)) || \
	(defined IEEE80211_EHT_OPER_INFO_PRESENT))
#define CFG80211_TX_CONTROL_PORT_LINK_SUPPORT 1
#endif

/*
 * CFG80211_EXTERNAL_AUTH_MLO_SUPPORT
 * Used to indicate Linux kernel contains support for ML external auth
 *
 * TODO: Corresponding Linux kernel changes are still under wirless-next
 * will add the commit-ID when available.
 */
/*
 * TODO: will add this check when available.
 * #if (defined(__ANDROID_COMMON_KERNEL__) && \
 *	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)) && \
 *	(defined CFG80211_EXTERNAL_AUTH_MLO_SUPPORT))
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0) || \
	(defined CFG80211_EXTERNAL_AUTH_MLO_SUPPORT))
#define WLAN_EXTERNAL_AUTH_MLO_SUPPORT
#endif

/*
 * CFG80211_TID_LINK_MAP_SUPPORT
 * Used to indicate Linux kernel contains support to get the TID to link map
 * status and response event.
 *
 * TODO: Corresponding Linux kernel support check changes are still under review
 * will add the commit-ID when available
 */
#if (defined CFG80211_TID_LINK_MAP_SUPPORT)
#define WLAN_TID_LINK_MAP_SUPPORT
#endif

/*
 * CFG80211_EXT_FEATURE_SECURE_NAN
 * Used to indicate Linux kernel contains support to secure NAN feature
 *
 * This feature was introduced in Linux Kernel 6.4 via:
 * 9b89495e479c wifi: nl80211: Allow authentication frames and set keys on NAN
 *                             interface
 */
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)) || \
	(defined CFG80211_EXT_FEATURE_SECURE_NAN))
#define WLAN_EXT_FEATURE_SECURE_NAN
#endif

/*
 * CFG80211_EXT_FEATURE_AUTH_AND_DEAUTH_RANDOM_TA
 * Used to indicate Linux kernel contains support to auth and deauth random TA
 *
 * This feature was introduced in Linux Kernel 6.4 via:
 * 6933486133ec wifi: nl80211: Add support for randomizing TA of auth and deauth
 *                             frames
 */
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)) || \
	(defined CFG80211_EXT_FEATURE_AUTH_AND_DEAUTH_RANDOM_TA))
#define WLAN_EXT_FEATURE_AUTH_AND_DEAUTH_RANDOM_TA
#endif

/*
 * WLAN_MLD_AP_STA_CONNECT_SUPPORT
 * Used to indicate Linux Upstream Kernel supports ML connection on SAP.
 *
 * This feature was introduced in Linux Kernel 6.3 via:
 * a42e59e: wifi: cfg80211: Extend cfg80211_new_sta() for MLD AP
 * 8bb588d: wifi: cfg80211: Extend cfg80211_update_owe_info_event() for MLD AP
 *
 * This feature was backported to Android Common Kernel 5.15.74 via:
 * https://android-review.googlesource.com/c/kernel/common/+/2450265
 * https://android-review.googlesource.com/c/kernel/common/+/2450266
 *
 * This feature was backported to Android Common Kernel 6.1 via:
 * https://android-review.googlesource.com/c/kernel/common/+/2470890
 * https://android-review.googlesource.com/c/kernel/common/+/2470891
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0) || \
	(defined CFG80211_MLD_AP_STA_CONNECT_UPSTREAM_SUPPORT))
#define WLAN_MLD_AP_STA_CONNECT_UPSTREAM_SUPPORT 1
#endif

/*
 * WLAN_LINK_STA_PARAMS_PRESENT
 * WLAN_EHT_CAPABILITY_PRESENT
 *
 * To deterministically identify where EHT fields are present (whether in
 * link_sta_params or station_parameters), enable either
 * WLAN_LINK_STA_PARAMS_PRESENT or WLAN_EHT_CAPABILITY_PRESENT but not both.
 *
 * Incase EHT fields are present in link_sta_parameters only define
 * WLAN_LINK_STA_PARAMS_PRESENT flag.
 *
 * If WLAN_LINK_STA_PARAMS_PRESENT is not defined, then EHT cap can be present
 * in station_parameters structure, in such case WLAN_EHT_CAPABILITY_PRESENT
 * flag will be defined.
 *
 * If both flags are not enabled then kernel is not EHT capable.
 *
 * WLAN_LINK_STA_PARAMS_PRESENT
 * 577e5b8 wifi: cfg80211: add API to add/modify/remove a link station
 * b95eb7f wifi: cfg80211/mac80211: separate link params from station params
 *
 * The changes are backported to ACK from below commits.
 * https://android-review.googlesource.com/c/kernel/common/+/2227406
 * https://android-review.googlesource.com/c/kernel/common/+/2227407
 *
 * WLAN_EHT_CAPABILITY_PRESENT
 * ea05fd3 cfg80211: Support configuration of station EHT capabilities
 *
 * The changes are backported to ACK from below commit.
 * https://android-review.googlesource.com/c/kernel/common/+/1996268
 *
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0) || \
	(defined CFG80211_LINK_STA_PARAMS_PRESENT))
#define WLAN_LINK_STA_PARAMS_PRESENT 1
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0) || \
	(defined NL80211_EHT_MIN_CAPABILITY_LEN))
#define WLAN_EHT_CAPABILITY_PRESENT 1
#endif

#endif
