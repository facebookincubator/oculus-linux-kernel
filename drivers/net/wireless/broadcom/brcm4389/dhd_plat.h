/*
 * DHD Linux platform header file
 *
 * Copyright (C) 2022, Broadcom.
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
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 */

#ifndef __DHD_PLAT_H__
#define __DHD_PLAT_H__

#if defined(__linux__)

#include <linuxver.h>

#if !defined(CONFIG_WIFI_CONTROL_FUNC) && !defined(CUSTOMER_HW4)
#define WLAN_PLAT_NODFS_FLAG	0x01
#define WLAN_PLAT_AP_FLAG	0x02
struct wifi_platform_data {
	int (*set_power)(int val);
	int (*set_reset)(int val);
	int (*set_carddetect)(int val);
#ifdef DHD_COREDUMP
	int (*set_coredump)(const char *buf, int buf_len, const char *info);
#endif /* DHD_COREDUMP */
	void *(*mem_prealloc)(int section, unsigned long size);
	int (*get_mac_addr)(unsigned char *buf);
#ifdef BCMSDIO
	int (*get_wake_irq)(void);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 58)) || defined(CUSTOM_COUNTRY_CODE)
	void *(*get_country_code)(char *ccode, u32 flags);
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 58)) || defined (CUSTOM_COUNTRY_CODE) */
	void *(*get_country_code)(char *ccode);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 58)) */
};
#endif /* CONFIG_WIFI_CONTROL_FUNC  || CUSTOMER_HW4 */

#include <linux/pci.h>

/*
 * Plat Layer defines the interfaces that the BSP specific file should override
 * The default implementation of the interfaces are present in dhd_linux_platdev.c
 * The data structure/handle to be passed by the DHD to BSP specific file is opaque
 * called plat_info. The data structure is to be maintained purely inside the
 * BSP specific file and hence its kept opaque.
 *
 * There are two types of interface functions
 * 1) Functions that need the interface structure plat_info to be passed down
 *    from DHD.
 * 2) Functions that queries for certain information in BSP specific way and just
 *    returns to DHD - These functions does not take the plat_info as argument.
 *
 * The declarations are grouped accordingly. While adding a new interface function
 * declaration based on the group it belongs to add it in the appropriate section
 */

typedef void (*dhd_pcie_event_cb_t) (struct pci_dev *pdev);
extern int dhd_plat_pcie_register_event(void *plat_info,
		struct pci_dev *pdev, dhd_pcie_event_cb_t pfn);
extern void dhd_plat_pcie_deregister_event(void *plat_info);
extern void dhd_plat_report_bh_sched(void *plat_info, int resched);
extern int dhd_plat_pcie_suspend(void *plat_info);
extern int dhd_plat_pcie_resume(void *plat_info);
extern void dhd_plat_pcie_register_dump(void *plat_info);
extern void dhd_plat_pin_dbg_show(void *plat_info);

extern uint32 dhd_plat_get_info_size(void);
extern void dhd_plat_l1ss_ctrl(bool ctrl);

/* To be called when we intend to exit L1 while performing wreg, rreg operations */
extern void dhd_plat_l1_exit_io(void);

/* To be called when we intend to exit L1 in non-io case */
extern void dhd_plat_l1_exit(void);

extern uint32 dhd_plat_get_rc_vendor_id(void);
extern uint32 dhd_plat_get_rc_device_id(void);

extern uint16 dhd_plat_align_rxbuf_size(uint16 rxbufpost_sz);
#endif /* __linux__ */
#endif /* __DHD_PLAT_H__ */
