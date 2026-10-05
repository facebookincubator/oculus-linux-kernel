/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfw_sysfs.h
 *
 * @brief Internal header for sysfs helpers.
 *
 * @details APIs for initializing and managing sysfs files for the arfw generic layer. 
 *
 *******************************************************************************/

#ifndef ARFW_SYSFS_H
#define ARFW_SYSFS_H

#include "arfw_int.h"

int add_arfw_device_sysfs_nodes(struct arfw_char_device *device);
void remove_arfw_device_sysfs_nodes(struct arfw_char_device *device);

#endif // ARFW_SYSFS_H
