/*
 * Linux cfg80211 driver - Android related functions
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

#include <linux/module.h>
#include <linux/netdevice.h>
#include <wldev_common.h>

/* If any feature uses the Generic Netlink Interface, put it here to enable WL_GENL
 * automatically
 */
#if defined(BT_WIFI_HANDOVER)
#define WL_GENL
#endif // endif

typedef struct _android_wifi_priv_cmd {
    char *buf;
    int used_len;
    int total_len;
} android_wifi_priv_cmd;

#ifdef CONFIG_COMPAT
typedef struct _compat_android_wifi_priv_cmd {
    compat_caddr_t buf;
    int used_len;
    int total_len;
} compat_android_wifi_priv_cmd;
#endif /* CONFIG_COMPAT */

/**
 * Android platform dependent functions, feel free to add Android specific functions here
 * (save the macros in dhd). Please do NOT declare functions that are NOT exposed to dhd
 * or cfg, define them as static in wl_android.c
 */

/**
 * wl_android_init will be called from module init function (dhd_module_init now), similarly
 * wl_android_exit will be called from module exit function (dhd_module_cleanup now)
 */
int wl_android_init(void);
int wl_android_exit(void);
void wl_android_post_init(void);
int wl_android_wifi_on(struct net_device *dev);
int wl_android_wifi_off(struct net_device *dev, bool on_failure);
int wl_android_priv_cmd(struct net_device *net, struct ifreq *ifr);
int wl_handle_private_cmd(struct net_device *net, char *command, u32 cmd_len);

s32 wl_netlink_send_msg(int pid, int type, int seq, const void *data, size_t size);

/* hostap mac mode */
#define MACLIST_MODE_DISABLED   0
#define MACLIST_MODE_DENY       1
#define MACLIST_MODE_ALLOW      2

/* max number of assoc list */
#define MAX_NUM_OF_ASSOCLIST    64

/* Bandwidth */
#define WL_CH_BANDWIDTH_20MHZ 20
#define WL_CH_BANDWIDTH_40MHZ 40
#define WL_CH_BANDWIDTH_80MHZ 80
#define WL_CH_BANDWIDTH_160MHZ 160

/* max number of mac filter list
 * restrict max number to 10 as maximum cmd string size is 255
 */
#define MAX_NUM_MAC_FILT        10
#define	WL_GET_BAND(ch)	(((uint)(ch) <= CH_MAX_2G_CHANNEL) ?	\
	WLC_BAND_2G : WLC_BAND_5G)

int wl_android_set_ap_mac_list(struct net_device *dev, int macmode, struct maclist *maclist);
#ifdef WL_BCNRECV
extern int wl_android_bcnrecv_config(struct net_device *ndev, char *data,
		int total_len);
extern int wl_android_bcnrecv_stop(struct net_device *ndev, uint reason);
extern int wl_android_bcnrecv_resume(struct net_device *ndev);
extern int wl_android_bcnrecv_suspend(struct net_device *ndev);
extern int wl_android_bcnrecv_event(struct net_device *ndev,
		uint attr_type, uint status, uint reason, uint8 *data, uint data_len);
#endif /* WL_BCNRECV */
#ifdef WL_CAC_TS
#define TSPEC_UPLINK_DIRECTION (0 << 5)	/* uplink direction traffic stream */
#define TSPEC_DOWNLINK_DIRECTION (1 << 5)	/* downlink direction traffic stream */
#define TSPEC_BI_DIRECTION (3 << 5)	/* bi direction traffic stream */
#define TSPEC_EDCA_ACCESS (1 << 7)	/* EDCA access policy */
#define TSPEC_UAPSD_PSB (1 << 2)		/* U-APSD power saving behavior */
#define TSPEC_TSINFO_TID_SHIFT 1		/* TID Shift */
#define TSPEC_TSINFO_PRIO_SHIFT 3		/* PRIO Shift */
#define TSPEC_MAX_ACCESS_CATEGORY 3
#define TSPEC_MAX_USER_PRIO	7
#define TSPEC_MAX_DIALOG_TOKEN	255
#define TSPEC_MAX_SURPLUS_BW 12410
#define TSPEC_MIN_SURPLUS_BW 11210
#define TSPEC_MAX_MSDU_SIZE 1520
#define TSPEC_DEF_MEAN_DATA_RATE 120000
#define TSPEC_DEF_MIN_PHY_RATE 6000000
#define TSPEC_DEF_DIALOG_TOKEN 7
#endif /* WL_CAC_TS */

#define WL_PRIV_CMD_LEN 64
