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

/*Image2HeaderVersion: 3.2*/
#ifndef __INC_MP_BB_HW_IMG_8822B_H
#define __INC_MP_BB_HW_IMG_8822B_H

/******************************************************************************
 *                           agc_tab.TXT
 ******************************************************************************/

void odm_read_and_config_mp_8822b_agc_tab(/* tc: Test Chip, mp: mp Chip*/
					  struct phy_dm_struct *dm);
u32 odm_get_version_mp_8822b_agc_tab(void);

/******************************************************************************
 *                           phy_reg.TXT
 ******************************************************************************/

void odm_read_and_config_mp_8822b_phy_reg(/* tc: Test Chip, mp: mp Chip*/
					  struct phy_dm_struct *dm);
u32 odm_get_version_mp_8822b_phy_reg(void);

/******************************************************************************
 *                           phy_reg_pg.TXT
 ******************************************************************************/

void odm_read_and_config_mp_8822b_phy_reg_pg(/* tc: Test Chip, mp: mp Chip*/
					     struct phy_dm_struct *dm);
u32 odm_get_version_mp_8822b_phy_reg_pg(void);

#endif
