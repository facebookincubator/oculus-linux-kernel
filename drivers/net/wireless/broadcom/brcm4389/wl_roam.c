/*
 * Linux roam cache
 *
 * Copyright (C) 2020, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <bcmutils.h>
#ifdef WL_CFG80211
#include <wl_cfg80211.h>
#endif // endif
#include <wldev_common.h>
#if defined(__linux__)
#include <bcmstdlib_s.h>
#endif /* defined(__linux__) */

#ifdef ESCAN_CHANNEL_CACHE
#define MAX_ROAM_CACHE		200
#define MAX_SSID_BUFSIZE	36

typedef struct {
	chanspec_t chanspec;
	int ssid_len;
	char ssid[MAX_SSID_BUFSIZE];
} roam_channel_cache;

static int n_roam_cache = 0;
static int roam_band = WLC_BAND_AUTO;
static roam_channel_cache roam_cache[MAX_ROAM_CACHE];
static uint band_bw;

#ifdef ESCAN_CHANNEL_CACHE
static int
set_roamscan_chspec_list(struct net_device *dev,
		unsigned chan_cnt, chanspec_t *chspecs)
{
	s32 error;
	wl_roam_channel_list_t channel_list;
	char iobuf[WLC_IOCTL_SMLEN];
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	cfg->roamscan_mode = ROAMSCAN_MODE_NORMAL;

	if (chan_cnt > MAX_ROAM_CHANNEL) {
		chan_cnt = MAX_ROAM_CHANNEL;
	}

	channel_list.n = chan_cnt;
	if (memcpy_s(channel_list.channels, sizeof(channel_list.channels),
			chspecs, (chan_cnt * sizeof(chanspec_t))) != BCME_OK) {
		WL_ERR(("channel list copy failed\n"));
		return -EINVAL;
	}
	/* need to set ROAMSCAN_MODE_NORMAL to update roamscan_channels,
	 * otherwise, it won't be updated
	 */
	error = wldev_iovar_setint(dev, "roamscan_mode", ROAMSCAN_MODE_NORMAL);
	if (error) {
		WL_ERR(("Failed to set roamscan mode to %d, error = %d\n",
			ROAMSCAN_MODE_NORMAL, error));
		return error;
	}
	error = wldev_iovar_setbuf(dev, "roamscan_channels", &channel_list,
		sizeof(channel_list), iobuf, sizeof(iobuf), NULL);
	if (error) {
		WL_ERR(("Failed to set roamscan channels, error = %d\n", error));
		return error;
	}

	return error;
}
#endif /* ESCAN_CHANNEL_CACHE */

#ifdef ESCAN_CHANNEL_CACHE
void set_roam_band(int band)
{
	roam_band = band;
}

void reset_roam_cache(struct bcm_cfg80211 *cfg)
{
	if (!cfg->rcc_enabled) {
		return;
	}

	n_roam_cache = 0;
}

static void
add_roam_cache_list(uint8 *SSID, uint32 SSID_len, chanspec_t chanspec)
{
	int i;
	uint8 channel;
	char chanbuf[CHANSPEC_STR_LEN];

	if (n_roam_cache >= MAX_ROAM_CACHE) {
		return;
	}

	for (i = 0; i < n_roam_cache; i++) {
		if ((roam_cache[i].ssid_len == SSID_len) &&
			(roam_cache[i].chanspec == chanspec) &&
			(memcmp(roam_cache[i].ssid, SSID, SSID_len) == 0)) {
			/* identical one found, just return */
			return;
		}
	}

	roam_cache[n_roam_cache].ssid_len = SSID_len;
	channel = wf_chspec_ctlchan(chanspec);
	WL_DBG(("CHSPEC  = %s, CTL %d SSID %s\n",
		wf_chspec_ntoa_ex(chanspec, chanbuf), channel, SSID));
	roam_cache[n_roam_cache].chanspec = CHSPEC_BAND(chanspec) | band_bw | channel;
	(void)memcpy_s(roam_cache[n_roam_cache].ssid, SSID_len, SSID, SSID_len);

	n_roam_cache++;
}

void
add_roam_cache(struct bcm_cfg80211 *cfg, wl_bss_info_t *bi)
{
	if (!cfg->rcc_enabled) {
		return;
	}

	add_roam_cache_list(bi->SSID, bi->SSID_len, bi->chanspec);
}

static bool is_duplicated_channel(const chanspec_t *channels, int n_channels, chanspec_t new)
{
	int i;

	for (i = 0; i < n_channels; i++) {
		if (channels[i] == new)
			return TRUE;
	}

	return FALSE;
}

int get_roam_channel_list(struct bcm_cfg80211 *cfg, chanspec_t target_chan,
	chanspec_t *channels, int n_channels, const wlc_ssid_t *ssid, int ioctl_ver)
{
	int i, n = 0;
	char chanbuf[CHANSPEC_STR_LEN];

	/* first index is filled with the given target channel */
	if ((target_chan != INVCHANSPEC) && (target_chan != 0)) {
		channels[0] = target_chan;
		n++;
	}

	WL_DBG((" %s: 0x%04X\n", __FUNCTION__, channels[0]));

	for (i = 0; i < n_roam_cache; i++) {
		chanspec_t ch = roam_cache[i].chanspec;
		bool band_match = ((roam_band == WLC_BAND_AUTO) ||
			((roam_band == WLC_BAND_2G) && (CHSPEC_IS2G(ch))) ||
			((roam_band == WLC_BAND_5G) && (CHSPEC_IS5G(ch))));

		ch = CHSPEC_CHANNEL(ch) | CHSPEC_BAND(ch) | band_bw;
		if ((roam_cache[i].ssid_len == ssid->SSID_len) &&
			band_match && !is_duplicated_channel(channels, n, ch) &&
			(memcmp(roam_cache[i].ssid, ssid->SSID, ssid->SSID_len) == 0)) {
			/* match found, add it */
			WL_DBG(("%s: Chanspec = %s\n", __FUNCTION__,
				wf_chspec_ntoa_ex(ch, chanbuf)));
			channels[n++] = ch;
			if (n >= n_channels) {
				WL_ERR(("Too many roam scan channels\n"));
				return n;
			}
		}
	}

	return n;
}

void
wl_update_rcc_list(struct net_device *dev)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	chanspec_t chanspec_list[MAX_ROAM_CHANNEL];
	wlc_ssid_t *ssid = NULL;
	u16 rcc_cnt = 0;

	if (cfg->rcc_enabled)
	{
		ssid = (wlc_ssid_t*)wl_read_prof(cfg, dev, WL_PROF_SSID);
		if (ssid == NULL) {
			WL_ERR(("Failed to read SSID profile\n"));
			ASSERT(0);
			return;
		}
		rcc_cnt = get_roam_channel_list(cfg, INVCHANSPEC, chanspec_list, MAX_ROAM_CHANNEL,
			ssid, ioctl_version);
		if (rcc_cnt != 0) {
			/* Update rcc list to the firmware for roam use case */
			if (set_roamscan_chspec_list(dev, rcc_cnt, chanspec_list) != BCME_OK) {
				WL_ERR(("Roam channel update failed\n"));
			} else {
				WL_DBG(("Roam channel updated chan_cnt:%d\n", rcc_cnt));
			}
		}
	}
}
#endif /* ESCAN_CHANNEL_CACHE */

#endif /* ESCAN_CHANNEL_CACHE */
