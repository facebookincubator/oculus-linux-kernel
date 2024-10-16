/*
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

/*
 * DOC: contains coex public structure definitions
 */

#ifndef _WLAN_COEX_PUBLIC_STRUCTS_H_
#define _WLAN_COEX_PUBLIC_STRUCTS_H_

#ifdef WLAN_FEATURE_DBAM_CONFIG
#define WLAN_SET_DBAM_CONFIG_TIMEOUT 5000

/**
 * enum coex_dbam_config_mode - dbam config mode
 * @COEX_DBAM_DISABLE: Disable DBAM
 * @COEX_DBAM_ENABLE: ENABLE DBAM opportunistically when internal
 *  conditions are met.
 * @COEX_DBAM_FORCE_ENABLE: Enable DBAM forcefully
 */
enum coex_dbam_config_mode {
	COEX_DBAM_DISABLE = 0,
	COEX_DBAM_ENABLE = 1,
	COEX_DBAM_FORCE_ENABLE = 2,
};

/**
 * enum coex_dbam_comp_status - dbam config response
 * @COEX_DBAM_COMP_SUCCESS: FW enabled/disabled DBAM mode succssfully
 * @COEX_DBAM_COMP_NOT_SUPPORT: DBAM mode is not supported
 * @COEX_DBAM_COMP_FAIL: FW failed to enable/disable DBAM mode
 */
enum coex_dbam_comp_status {
	COEX_DBAM_COMP_SUCCESS = 0,
	COEX_DBAM_COMP_NOT_SUPPORT = 1,
	COEX_DBAM_COMP_FAIL = 2,
};

/**
 * struct coex_dbam_config_params - Coex DBAM config command params
 * @vdev_id: Virtual device identifier
 * @dbam_mode: DBAM configuration mode - coex_dbam_config_mode enum
 */
struct coex_dbam_config_params {
	uint32_t vdev_id;
	enum coex_dbam_config_mode dbam_mode;
};

/**
 * struct coex_dbam_config_resp - Coex DBAM config response
 * @dbam_resp: DBAM config request response - coex_dbam_comp_status enum
 */
struct coex_dbam_config_resp {
	enum coex_dbam_comp_status dbam_resp;
};

#endif
#endif
