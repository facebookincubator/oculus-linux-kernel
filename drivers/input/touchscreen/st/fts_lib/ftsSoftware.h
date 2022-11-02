/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 *
 *************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                          FW related data                               *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 *
 */

#ifndef __FTS_SOFTWARE_H
#define __FTS_SOFTWARE_H

#include <linux/types.h>

#include "ftsHardware.h"

#define ECHO_ENABLED                    0x00000001

//FTS FW COMMAND
#define FTS_CMD_MS_MT_SENSE_OFF         0x92
#define FTS_CMD_MS_MT_SENSE_ON          0x93
#define FTS_CMD_SS_HOVER_OFF            0x94
#define FTS_CMD_SS_HOVER_ON             0x95
#define FTS_CMD_LP_TIMER_CALIB          0x97
#define FTS_CMD_MS_KEY_OFF              0x9A
#define FTS_CMD_MS_KEY_ON               0x9B
#define FTS_CMD_MS_COMP_TUNING          0xA3
#define FTS_CMD_SS_COMP_TUNING          0xA4
#define FTS_CMD_FULL_INITIALIZATION     0xA5
#define FTS_CMD_ITO_CHECK               0xA7
#define FTS_CMD_RELEASE_INFO            0xAA
#define        FTS_CMD_GESTURE_MODE     0xAD
#define FTS_CMD_REQU_FW_CONF            0xB2
#define FTS_CMD_REQU_FRAME_DATA         0xB7
#define FTS_CMD_REQU_COMP_DATA          0xB8
#define FTS_CMD_WRITE_MP_FLAG           0xC0
#define FTS_CMD_FEATURE_ENABLE          0xC1
#define FTS_CMD_FEATURE_DISABLE         0xC2
#define FTS_CMD_GESTURE_CMD             0xC3
#define FTS_CMD_LOCKDOWN_CMD            0xC4
#define FTS_CMD_NOISE_WRITE             0xC7
#define FTS_CMD_NOISE_READ              0xC8
#define FTS_CMD_LOCKDOWN_FILL           0xCA
#define FTS_CMD_LOCKDOWN_WRITE          0xCB
#define FTS_CMD_LOCKDOWN_READ           0xCC
#define FTS_CMD_SAVE_CX_TUNING          0xFC

//Event ID
#define EVENTID_NO_EVENT                0x00
#define EVENTID_ERROR_EVENT             0x0F
#define EVENTID_CONTROL_READY           0x10
#define EVENTID_FW_CONFIGURATION        0x12
#define EVENTID_COMP_DATA_READ          0x13
#define EVENTID_STATUS_UPDATE           0x16
#define EVENTID_RELEASE_INFO            0x1C
#define EVENTID_LOCKDOWN_INFO           0x1E
#define EVENTID_ENTER_POINTER           0x03
#define EVENTID_LEAVE_POINTER           0x04
#define EVENTID_MOTION_POINTER          0x05
#define EVENTID_HOVER_ENTER_POINTER     0x07
#define EVENTID_HOVER_LEAVE_POINTER     0x08
#define EVENTID_HOVER_MOTION_POINTER    0x09
#define EVENTID_PROXIMITY_ENTER         0x0B
#define EVENTID_PROXIMITY_LEAVE         0x0C
#define EVENTID_KEY_STATUS              0x0E
#define EVENTID_LOCKDOWN_INFO_READ      0x1E
#define EVENTID_NOISE_READ              0x17
#define EVENTID_NOISE_WRITE             0x18
#define EVENTID_GESTURE                 0x22
#define EVENTID_FRAME_DATA_READ         0x25
#define EVENTID_ECHO                    0xEC
#define EVENTID_LAST                    (EVENTID_FRAME_DATA_READ+1)

//EVENT TYPE
#define EVENT_TYPE_MS_TUNING_CMPL       0x01
#define EVENT_TYPE_SS_TUNING_CMPL       0x02
#define EVENT_TYPE_COMP_DATA_SAVED      0x04
#define EVENT_TYPE_ITO                  0x05
#define EVENT_TYPE_FULL_INITIALIZATION  0x07
#define EVENT_TYPE_LPTIMER_TUNING_CMPL  0x20
#define EVENT_TYPE_ESD_ERROR            0x0A
#define EVENT_TYPE_WATCHDOG_ERROR       0x01
#define EVENT_TYPE_CHECKSUM_ERROR       0x03
#define EVENT_TYPE_LOCKDOWN             0x0A
#define EVENT_TYPE_LOCKDOWN_WRITE       0x08
#define EVENT_TYPE_LOCKDOWN_ERROR       0x0B

//CRC type
#define CRC_CONFIG_SIGNATURE            0x01
#define CRC_CONFIG                      0x02
#define CRC_CX_MEMORY                   0x03

// CONFIG ID INFO
#define CONFIG_ID_ADDR                  0x0001
#define CONFIG_ID_BYTE                  2

//ADDRESS OFFSET IN SYSINFO
#define ADDR_RAW_TOUCH                  0x0000
#define ADDR_FILTER_TOUCH               0x0002
#define ADDR_NORM_TOUCH                 0x0004
#define ADDR_CALIB_TOUCH                0x0006
#define ADDR_RAW_HOVER_FORCE            0x000A
#define ADDR_RAW_HOVER_SENSE            0x000C
#define ADDR_FILTER_HOVER_FORCE         0x000E
#define ADDR_FILTER_HOVER_SENSE         0x0010
#define ADDR_NORM_HOVER_FORCE           0x0012
#define ADDR_NORM_HOVER_SENSE           0x0014
#define ADDR_CALIB_HOVER_FORCE          0x0016
#define ADDR_CALIB_HOVER_SENSE          0x0018
#define ADDR_RAW_PRX_FORCE              0x001A
#define ADDR_RAW_PRX_SENSE              0x001C
#define ADDR_FILTER_PRX_FORCE           0x001E
#define ADDR_FILTER_PRX_SENSE           0x0020
#define ADDR_NORM_PRX_FORCE             0x0022
#define ADDR_NORM_PRX_SENSE             0x0024
#define ADDR_CALIB_PRX_FORCE            0x0026
#define ADDR_CALIB_PRX_SENSE            0x0028
#define ADDR_RAW_MS_KEY                 0x0032
#define ADDR_NORM_MS_KEY                0x0036
#define ADDR_COMP_DATA                  0x0050
#define ADDR_FRAMEBUFFER_DATA           0x8000

//ADDRESS FW REGISTER
#define ADDR_SENSE_LEN                  0x0014
#define ADDR_FORCE_LEN                  0x0015
#define ADDR_MS_TUNING_VER              0x0729
#define ADDR_SS_TUNING_VER              0x074E

//B2 INFO
#define B2_DATA_BYTES                   4
//number of bytes
#define B2_CHUNK                       ((FIFO_DEPTH/2)*B2_DATA_BYTES)

//FEATURES
#define FEAT_GLOVE                      0x00000001
#define FEAT_STYLUS                     0x00000002
#define FEAT_COVER                      0x00000004
#define FEAT_CHARGER                    0x00000008
#define FEAT_VR                         0x00000010
#define FEAT_EDGE_REJECTION             0x00000020
#define FEAT_CORNER_REJECTION           0x00000040
#define FEAT_EDGE_PALM_REJECTION        0x00000100

//TOUCH SIZE
#define STYLUS_SIZE                     0x01
#define FINGER_SIZE                     0x02
#define LARGE_SIZE                      0x03

//C7/C8 parameters
#define NOISE_PARAMETERS                0x01

//MP_FLAG_VALUE
#define INIT_MP                        0xA5A5A501
#define INIT_FIELD                     0xA5A5A502

//NOISE PARAMS
#define NOISE_PARAMETERS_SIZE          4 //bytes

//ERROR INFO
#define ERROR_INFO_SIZE                (20*4) // bytes
#define ERROR_SIGNATURE                0xFA5005AF
#define ERROR_SIGN_HEAD                0xA5

#endif
