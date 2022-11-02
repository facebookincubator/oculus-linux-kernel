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

#ifndef __PHYDMKFREE_H__
#define __PHYDMKFREE_H__

#define KFREE_VERSION "1.0"

enum phydm_kfree_channeltosw {
	PHYDM_2G = 0,
	PHYDM_5GLB1 = 1,
	PHYDM_5GLB2 = 2,
	PHYDM_5GMB1 = 3,
	PHYDM_5GMB2 = 4,
	PHYDM_5GHB = 5,
};

void phydm_config_kfree(void *dm_void, u8 channel_to_sw, u8 *kfree_table);

#endif
