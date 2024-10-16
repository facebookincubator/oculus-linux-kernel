// SPDX-License-Identifier: GPL-2.0-only
/****************************************************************************
 * @file power_state_virtual_sensor.h
 *
 * @brief Header file for peak power state virtual sensor for hammerhead
 *
 * @details
 *
 ****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#ifndef POWER_STATE_VIRTUAL_SENSOR_H
#define POWER_STATE_VIRTUAL_SENSOR_H

/* Each scenario has a bit in the power status
   to enable easy decoding by SNAppManager and
   apps
*/

enum usecase_bitshifts {
	IMAGE_CAPTURE_BITSHIFT,
	VIDEO_CAPTURE_BITSHIFT,
	MEDIA_TRANSFER_BITSHIFT,
	LIVESTREAMING_BITSHIFT,
	AUDIOPLAYBACK_BITSHIFT,
};

enum HammerheadScenarioBlock {
	HH_ImageCapture_Blocked = 0x1 << IMAGE_CAPTURE_BITSHIFT,
	HH_VideoCapture_Blocked = 0x1 << VIDEO_CAPTURE_BITSHIFT,
	HH_MediaTransfer20MHz_Blocked = 0x1 << MEDIA_TRANSFER_BITSHIFT,
	HH_LiveStreaming_Blocked = 0x1 << LIVESTREAMING_BITSHIFT,
	HH_AudioPlayback_Blocked = 0x1 << AUDIOPLAYBACK_BITSHIFT,
};

/*
  These are effectively bit-ors of the
  scenario codes, except for the OK
  state.
 */
typedef enum {
	HH_POWER_OK = 1,
	HH_POWER_NOVC = HH_VideoCapture_Blocked,
	HH_POWER_NOVCMEDIA20 = HH_VideoCapture_Blocked |
                HH_MediaTransfer20MHz_Blocked,
	HH_POWER_NOICVCMEDIA20 = HH_ImageCapture_Blocked |
                HH_VideoCapture_Blocked |
                HH_MediaTransfer20MHz_Blocked,
	HH_POWER_RED = 12,
} HammerheadPeakPowerState;

/* Power tables.
 * Format is {temp, capacity, state, capacity, state ...}
 * IMPORTANT: order of (temp,state) tuples matters!
 */
int PeakPowerPolicy25[] = {25, 8, HH_POWER_NOVC};
int PeakPowerPolicy15[] = {15, 18, HH_POWER_NOVCMEDIA20, 6, HH_POWER_RED};
int PeakPowerPolicy10[] = {10, 26, HH_POWER_NOVCMEDIA20, 10, HH_POWER_NOICVCMEDIA20,
			   6, HH_POWER_RED};
int PeakPowerPolicy5[]  = {5, 52, HH_POWER_NOVC, 46, HH_POWER_NOVCMEDIA20,
			   24, HH_POWER_NOICVCMEDIA20, 18, HH_POWER_RED};
int PeakPowerPolicy0[]  = {0, 68, HH_POWER_NOVC, 64, HH_POWER_NOVCMEDIA20,
			   46, HH_POWER_NOICVCMEDIA20, 40, HH_POWER_RED};


/*
   Used for the policy structure.
*/
typedef struct {
	int *policyArray;
	int numPolicyBuckets;
} TempPowerPolicy;

#define NUM_TEMP_BUCKETS (5)

#endif
