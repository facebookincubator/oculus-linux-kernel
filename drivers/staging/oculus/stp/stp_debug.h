/*
 * SPI STP Master debug header
 *
 * Copyright (C) 2020 Eugen Pirvu
 * Copyright (C) 2020 Facebook, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
