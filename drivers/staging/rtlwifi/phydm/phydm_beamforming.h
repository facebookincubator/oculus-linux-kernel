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
#ifndef __INC_PHYDM_BEAMFORMING_H
#define __INC_PHYDM_BEAMFORMING_H

/*Beamforming Related*/
#include "txbf/halcomtxbf.h"
#include "txbf/haltxbfjaguar.h"
#include "txbf/haltxbf8822b.h"
#include "txbf/haltxbfinterface.h"

#define beamforming_gid_paid(adapter, tcb)
#define phydm_acting_determine(dm, type) false
#define beamforming_enter(dm, sta_idx)
#define beamforming_leave(dm, RA)
#define beamforming_end_fw(dm)
#define beamforming_control_v1(dm, RA, AID, mode, BW, rate) true
#define beamforming_control_v2(dm, idx, mode, BW, period) true
#define phydm_beamforming_end_sw(dm, _status)
#define beamforming_timer_callback(dm)
#define phydm_beamforming_init(dm)
#define phydm_beamforming_control_v2(dm, _idx, _mode, _BW, _period) false
#define beamforming_watchdog(dm)
#define phydm_beamforming_watchdog(dm)

#endif
