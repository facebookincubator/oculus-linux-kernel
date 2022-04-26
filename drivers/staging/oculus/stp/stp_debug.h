/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef STP_DEBUG_H
#define STP_DEBUG_H

#define STP_DEBUG

#ifdef STP_DEBUG

// Display the SPI transaction details
//#define STP_DEBUG_TRANSACTIONS

// Display the GPIOs details
//#define STP_DEBUG_GPIO

// Display the slave state details
//#define STP_DEBUG_SLAVE_STATE

// Introduce noise in TX data
//#define STP_DEBUG_TX_DATA_NOISE

// Introduce noise in TX metadata
//#define STP_DEBUG_TX_METADATA_NOISE

// Introduce noise in control metadata
//#define STP_DEBUG_CONTROL_METADATA_NOISE

#endif

#endif
