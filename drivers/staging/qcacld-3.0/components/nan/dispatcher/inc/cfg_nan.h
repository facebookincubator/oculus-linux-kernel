/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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

#if !defined(__NAN_CFG_H__)
#define __NAN_CFG_H__

/**
 *
 * DOC: nan_cfg.h
 *
 * NAN feature INI configuration parameter definitions
 */
#include "cfg_define.h"
#include "cfg_converged.h"
#include "qdf_types.h"

/*
 * <ini>
 * gEnableNanSupport - NAN feature support configuration
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * When set to 1 NAN feature will be enabled.
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_ENABLE CFG_INI_BOOL("gEnableNanSupport", \
				    0, \
				    "Enable NAN Support")

/*
 * <ini>
 * nan_separate_iface_support: Separate iface creation for NAN
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Value is 1 when Host HDD supports separate iface creation
 * for NAN.
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_SEPARATE_IFACE_SUPP CFG_INI_BOOL("nan_separate_iface_support", \
						 1, \
						 "Seperate iface for NAN")


/*
 * <ini>
 * genable_nan_datapath - Enable NaN data path feature. NaN data path
 *                        enables NAN supported devices to exchange
 *                        data over TCP/UDP network stack.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * When set to 1 NAN Datapath feature will be enabled.
 *
 * Related: gEnableNanSupport
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_DATAPATH_ENABLE CFG_INI_BOOL("genable_nan_datapath", \
					     0, \
					     "Enable NAN Datapath support")

/*
 * <ini>
 * gEnableNDIMacRandomization - When enabled this will randomize NDI Mac
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * When enabled this will randomize NDI Mac
 *
 * Related: gEnableNanSupport
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_RANDOMIZE_NDI_MAC CFG_INI_BOOL("gEnableNDIMacRandomization", \
					       1, \
					       "Enable NAN MAC Randomization")

/*
 * <ini>
 * ndp_inactivity_timeout - To configure duration of how many seconds
 * without TX/RX data traffic, NDI vdev can kickout the connected
 * peer(i.e. NDP Termination).
 *
 * @Min: 0
 * @Max: 1800
 * @Default: 60
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_NDP_INACTIVITY_TIMEOUT CFG_INI_UINT("ndp_inactivity_timeout", \
						    0, \
						    1800, \
						    60, \
						    CFG_VALUE_OR_DEFAULT, \
						    "NDP Auto Terminate time")

/*
 * <ini>
 * gNdpKeepAlivePeriod - To configure duration of how many seconds
 * to wait to kickout peer if peer is not reachable.
 *
 * @Min: 10
 * @Max: 30
 * @Default: 20
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_NDP_KEEP_ALIVE_PERIOD CFG_INI_UINT( \
			"gNdpKeepAlivePeriod", \
			10, \
			30, \
			20, \
			CFG_VALUE_OR_DEFAULT, \
			"Keep alive timeout of a peer")

/*
 * <ini>
 * gSupportMp0Discovery - To support discovery of NAN cluster with
 * Master Preference (MP) as 0 when a new device is enabling NAN.
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SUPPORT_MP0_DISCOVERY CFG_INI_BOOL( \
			"gSupportMp0Discovery", \
			1, \
			"Enable/Disable discovery of NAN cluster with Master Preference (MP) as 0")

#ifdef WLAN_FEATURE_NAN
#define CFG_NAN_DISC CFG(CFG_NAN_ENABLE) \
			CFG(CFG_NDP_KEEP_ALIVE_PERIOD) \
			CFG(CFG_SUPPORT_MP0_DISCOVERY)
#define CFG_NAN_DP      CFG(CFG_NAN_DATAPATH_ENABLE) \
			CFG(CFG_NAN_RANDOMIZE_NDI_MAC) \
			CFG(CFG_NAN_NDP_INACTIVITY_TIMEOUT) \
			CFG(CFG_NAN_SEPARATE_IFACE_SUPP)
#else
#define CFG_NAN_DISC
#define CFG_NAN_DP
#endif

#define CFG_NAN_ALL     CFG_NAN_DISC \
			CFG_NAN_DP

#endif
