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

/**************************************/
/* DO NOT MODIFY THESE VALUES WITHOUT ALSO CHANGING THE TEST BELOW */
/* service-check-batt-temp-e2e-mtest.js */
/**************************************/

enum usecase_bitshifts {
	IMAGE_CAPTURE_BITSHIFT,
	VIDEO_CAPTURE_BITSHIFT,
	MEDIA_TRANSFER_BITSHIFT,
	LIVESTREAMING_BITSHIFT,
	AUDIOPLAYBACK_BITSHIFT,
	OTA_BITSHIFT,
	MMLLM_BITSHIFT,
	LONG_VIDEO_CAPTURE_BITSHIFT,
	VIDEO_CALLING_WIFI_DIRECT_BITSHIFT,
	AUTOCAPTURE_BITSHIFT,
	BME_BITSHIFT,
	LIVESTREAMING_WIFI_DIRECT_BITSHIFT,
	THROTTLE_SHUTDOWN_BITSHIFT = 31,
};

enum HammerheadScenarioBlock {
	HH_ImageCapture_Blocked = 0x1 << IMAGE_CAPTURE_BITSHIFT,
	HH_VideoCapture_Blocked = 0x1 << VIDEO_CAPTURE_BITSHIFT,
	HH_MediaTransfer20MHz_Blocked = 0x1 << MEDIA_TRANSFER_BITSHIFT,
	HH_LiveStreaming_Blocked = 0x1 << LIVESTREAMING_BITSHIFT,
	HH_AudioPlayback_Blocked = 0x1 << AUDIOPLAYBACK_BITSHIFT,
	HH_OTA_Blocked = 0x1 << OTA_BITSHIFT,
	HH_MMLLM_Blocked = 0x1 << MMLLM_BITSHIFT,
	HH_VideoCapture3Min_Blocked = 0x1 << LONG_VIDEO_CAPTURE_BITSHIFT,
	HH_VideoCallingWifiDirect_Blocked = 0x1 << VIDEO_CALLING_WIFI_DIRECT_BITSHIFT,
	HH_Autocapture_Blocked = 0x1 << AUTOCAPTURE_BITSHIFT,
	HH_Bme_Blocked = 0x1 << BME_BITSHIFT,
	HH_LiveStreamingWifiDirect_Blocked = 0x1 << LIVESTREAMING_WIFI_DIRECT_BITSHIFT,
};

/*
  These are effectively bit-ors of the
  scenario codes, except for the OK
  state.
 */
typedef enum {
	HH_POWER_OK = 0,
	HH_POWER_NOBME = HH_Bme_Blocked,
	HH_POWER_NOOTA = HH_OTA_Blocked,
	HH_POWER_NOMEDIA20 = HH_MediaTransfer20MHz_Blocked,
	HH_POWER_NOMEDIA20BME = HH_MediaTransfer20MHz_Blocked | HH_Bme_Blocked,
	HH_POWER_NOOTABME = HH_OTA_Blocked | HH_Bme_Blocked,
	HH_POWER_NOOTAVC3AUTOCAPTUREBME =
		HH_OTA_Blocked | HH_VideoCapture3Min_Blocked |
		HH_Autocapture_Blocked | HH_Bme_Blocked,
	HH_POWER_NOOTAMEDIA20AUTOCAPTUREBME =
		HH_OTA_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_Autocapture_Blocked | HH_Bme_Blocked,
	HH_POWER_NOOTAMEDIA20VC3AUTOCAPTUREBME =
		HH_OTA_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_VideoCapture3Min_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_NOOTAMEDIA20VC3VCWIFIAUTOCAPTUREBME =
		HH_OTA_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_NOOTAMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI =
		HH_OTA_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked | HH_LiveStreamingWifiDirect_Blocked,
	HH_POWER_NOOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI =
		HH_OTA_Blocked | HH_VideoCapture_Blocked |
		HH_MediaTransfer20MHz_Blocked | HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked | HH_LiveStreamingWifiDirect_Blocked,
	HH_POWER_NOMMLLMMEDIA20AUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_Autocapture_Blocked | HH_Bme_Blocked,
	HH_POWER_NOMMLLMOTAMEDIA20AUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_OTA_Blocked |
		HH_MediaTransfer20MHz_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_NOMMLLMOTAMEDIA20VCWIFIAUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_OTA_Blocked |
		HH_MediaTransfer20MHz_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_NOMMLLMOTAMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI =
		HH_MMLLM_Blocked | HH_OTA_Blocked |
		HH_MediaTransfer20MHz_Blocked | HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked | HH_LiveStreamingWifiDirect_Blocked,
	HH_POWER_NOMMLLMOTAMEDIA20VC3AUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_OTA_Blocked |
		HH_MediaTransfer20MHz_Blocked | HH_VideoCapture3Min_Blocked |
		HH_Autocapture_Blocked | HH_Bme_Blocked,
	HH_POWER_NOMMLLMOTAMEDIA20VC3VCWIFIAUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_OTA_Blocked |
		HH_MediaTransfer20MHz_Blocked | HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_NOMMLLMOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_OTA_Blocked | HH_VideoCapture_Blocked |
		HH_MediaTransfer20MHz_Blocked | HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_NOMMLLMOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI =
		HH_MMLLM_Blocked | HH_OTA_Blocked | HH_VideoCapture_Blocked |
		HH_MediaTransfer20MHz_Blocked | HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked | HH_LiveStreamingWifiDirect_Blocked,
	HH_POWER_NOMMLLMOTALSVCMEDIA20VC3VCWIFIAUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_OTA_Blocked | HH_LiveStreaming_Blocked |
		HH_VideoCapture_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_NOMMLLMOTALSVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI =
		HH_MMLLM_Blocked | HH_OTA_Blocked | HH_LiveStreaming_Blocked |
		HH_VideoCapture_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked | HH_LiveStreamingWifiDirect_Blocked,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI =
		HH_MMLLM_Blocked | HH_OTA_Blocked | HH_ImageCapture_Blocked |
		HH_VideoCapture_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_LiveStreaming_Blocked | HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked | HH_LiveStreamingWifiDirect_Blocked,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBME =
		HH_MMLLM_Blocked | HH_OTA_Blocked | HH_ImageCapture_Blocked |
		HH_VideoCapture_Blocked | HH_MediaTransfer20MHz_Blocked |
		HH_LiveStreaming_Blocked | HH_VideoCapture3Min_Blocked |
		HH_VideoCallingWifiDirect_Blocked | HH_Autocapture_Blocked |
		HH_Bme_Blocked,
	HH_POWER_RED = 0x1 << THROTTLE_SHUTDOWN_BITSHIFT,
} HammerheadPeakPowerState;

/* Power tables.
 * Format is {temp, capacity, state, capacity, state ...}
 * IMPORTANT: order of (capacity,state) tuples matters!
 */
int PeakPowerPolicy25[] = {
	25,
	30,
	HH_POWER_NOOTA,
	20,
	HH_POWER_NOOTAVC3AUTOCAPTUREBME,
	15,
	HH_POWER_NOOTAMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	10,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI
};

int PeakPowerPolicy20[] = {
	20,
	30,
	HH_POWER_NOOTA,
	25,
	HH_POWER_NOOTABME,
	20,
	HH_POWER_NOOTAMEDIA20VC3VCWIFIAUTOCAPTUREBME,
	15,
	HH_POWER_NOOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	10,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI
};

int PeakPowerPolicy15[] = {
	15,
	35,
	HH_POWER_NOBME,
	30,
	HH_POWER_NOOTAMEDIA20AUTOCAPTUREBME,
	25,
	HH_POWER_NOOTAMEDIA20VC3AUTOCAPTUREBME,
	20,
	HH_POWER_NOOTAMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	15,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI
};

int PeakPowerPolicy10[] = {
	10,
	50,
	HH_POWER_NOMEDIA20,
	45,
	HH_POWER_NOMMLLMMEDIA20AUTOCAPTUREBME,
	40,
	HH_POWER_NOMMLLMOTAMEDIA20AUTOCAPTUREBME,
	35,
	HH_POWER_NOMMLLMOTAMEDIA20VCWIFIAUTOCAPTUREBME,
	30,
	HH_POWER_NOMMLLMOTAMEDIA20VC3VCWIFIAUTOCAPTUREBME,
	25,
	HH_POWER_NOMMLLMOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBME,
	20,
	HH_POWER_NOMMLLMOTALSVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	15,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	5,
	HH_POWER_RED
};

int PeakPowerPolicy5[] = {
	5,
	70,
	HH_POWER_NOMEDIA20BME,
	60,
	HH_POWER_NOOTAMEDIA20AUTOCAPTUREBME,
	55,
	HH_POWER_NOMMLLMOTAMEDIA20AUTOCAPTUREBME,
	50,
	HH_POWER_NOMMLLMOTAMEDIA20VC3VCWIFIAUTOCAPTUREBME,
	40,
	HH_POWER_NOMMLLMOTAMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	35,
	HH_POWER_NOMMLLMOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	30,
	HH_POWER_NOMMLLMOTALSVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	20,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	5,
	HH_POWER_RED
};

int PeakPowerPolicy0[] = {
	0,
	85,
	HH_POWER_NOMEDIA20BME,
	80,
	HH_POWER_NOOTAMEDIA20VC3AUTOCAPTUREBME,
	75,
	HH_POWER_NOMMLLMOTAMEDIA20VC3AUTOCAPTUREBME,
	65,
	HH_POWER_NOMMLLMOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBME,
	60,
	HH_POWER_NOMMLLMOTAVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	50,
	HH_POWER_NOMMLLMOTALSVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	48,
	HH_POWER_NOMMLLMOTALSICVCMEDIA20VC3VCWIFIAUTOCAPTUREBMELSWIFI,
	5,
	HH_POWER_RED
};

/*
  Map of peak power states to thermal states
*/

typedef enum {
	THERMAL_STATE_NONE = 0,
	THERMAL_STATE_LIGHT,
	THERMAL_STATE_MODERATE,
	THERMAL_STATE_SEVERE,
	THERMAL_STATE_CRITICAL,
	THERMAL_STATE_EMERGENCY,
	THERMAL_STATE_SHUTDOWN,
	THERMAL_STATE_COUNT
} ThermalHALStates;

/*
   Used for the policy structure.
*/
typedef struct {
	int *policyArray;
	int numPolicyBuckets;
} TempPowerPolicy;

#define NUM_TEMP_BUCKETS (6)

#endif
