/* Copyright (c) 2014 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef BRCMFMAC_COMMON_H
#define BRCMFMAC_COMMON_H

#include <linux/platform_device.h>
#include <linux/platform_data/brcmfmac.h>
#include "fwil_types.h"

extern const u8 ALLFFMAC[ETH_ALEN];

#define BRCMF_FW_ALTPATH_LEN			256

/* Definitions for the module global and device specific settings are defined
 * here. Two structs are used for them. brcmf_mp_global_t and brcmf_mp_device.
 * The mp_global is instantiated once in a global struct and gets initialized
 * by the common_attach function which should be called before any other
 * (module) initiliazation takes place. The device specific settings is part
 * of the drvr struct and should be initialized on every brcmf_attach.
 */

/**
 * struct brcmf_mp_global_t - Global module paramaters.
 *
 * @firmware_path: Alternative firmware path.
 */
struct brcmf_mp_global_t {
	char	firmware_path[BRCMF_FW_ALTPATH_LEN];
};

extern struct brcmf_mp_global_t brcmf_mp_global;
#if defined(CPTCFG_BRCMFMAC_ANDROID)
extern struct regulator *wifi_regulator;
#endif /* defined(CPTCFG_BRCMFMAC_ANDROID) */
/**
 * struct brcmf_mp_device - Device module paramaters.
 *
 * @p2p_enable: Legacy P2P0 enable (old wpa_supplicant).
 * @feature_disable: Feature_disable bitmask.
 * @fcmode: FWS flow control.
 * @roamoff: Firmware roaming off?
 * @eap_restrict: Not allow data tx/rx until 802.1X auth succeeds
 * @sdio_wq_highpri: Tasks submitted to SDIO workqueue will run immediately.
 * @frameburst: Firmware frame burst mode.
 * @default_pm: default power management (PM) mode.
 * @ignore_probe_fail: Ignore probe failure.
 * @country_codes: If available, pointer to struct for translating country codes
 * @bus: Bus specific platform data. Only SDIO at the mmoment.
 */
struct brcmf_mp_device {
	bool		p2p_enable;
	unsigned int	feature_disable;
	int		fcmode;
	bool		roamoff;
	bool		eap_restrict;
	int		sdio_dpc_prio;
	bool		sdio_wq_highpri;
	bool		frameburst;
	int		default_pm;
	bool		ignore_probe_fail;
	struct brcmfmac_pd_cc *country_codes;
	union {
		struct brcmfmac_sdio_pd sdio;
	} bus;
	bool	early_scan_finish;
};

void brcmf_c_set_joinpref_default(struct brcmf_if *ifp);

struct brcmf_mp_device *brcmf_get_module_param(struct device *dev,
					       enum brcmf_bus_type bus_type,
					       u32 chip, u32 chiprev);
void brcmf_release_module_param(struct brcmf_mp_device *module_param);

/* Sets dongle media info (drv_version, mac address). */
int brcmf_c_preinit_dcmds(struct brcmf_if *ifp);

u8 brcmf_map_prio_to_prec(void *cfg, u8 prio);

u8 brcmf_map_prio_to_aci(void *cfg, u8 prio);

#endif /* BRCMFMAC_COMMON_H */
