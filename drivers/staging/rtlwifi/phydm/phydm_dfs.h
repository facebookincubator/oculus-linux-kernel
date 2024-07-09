/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * Copyright(c) 2007 - 2016  Realtek Corporation.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#ifndef __PHYDM_DFS_H__
#define __PHYDM_DFS_H__

#define DFS_VERSION "0.0"

/* ============================================================
 *  Definition
 * ============================================================
 */

/* ============================================================
 * 1  structure
 * ============================================================
 */

/* ============================================================
 *  enumeration
 * ============================================================
 */

enum phydm_dfs_region_domain {
	PHYDM_DFS_DOMAIN_UNKNOWN = 0,
	PHYDM_DFS_DOMAIN_FCC = 1,
	PHYDM_DFS_DOMAIN_MKK = 2,
	PHYDM_DFS_DOMAIN_ETSI = 3,
};

/* ============================================================
 *  function prototype
 * ============================================================
 */
#define phydm_dfs_master_enabled(dm) false

#endif /*#ifndef __PHYDM_DFS_H__ */
