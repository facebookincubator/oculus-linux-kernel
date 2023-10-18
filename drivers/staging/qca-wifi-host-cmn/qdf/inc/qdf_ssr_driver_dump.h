/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _QDF_SSR_DRIVER_DUMP_H_
#define _QDF_SSR_DRIVER_DUMP_H_

#include <qdf_types.h>

#ifdef WLAN_FEATURE_SSR_DRIVER_DUMP
#include "i_qdf_ssr_driver_dump.h"

typedef __qdf_ssr_driver_dump_entry qdf_ssr_driver_dump_entry;

/**
 * qdf_ssr_driver_dump_init() - Initialize the dump collection API.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - Initialization was successful
 *	else		   - Error initializing mutex
 */
QDF_STATUS qdf_ssr_driver_dump_init(void);

/*
 * qdf_ssr_driver_dump_deinit() - Deinitialize the dump collection API.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - Deinitialization was successful
 *	else		   - Error destroying mutex
 */
QDF_STATUS qdf_ssr_driver_dump_deinit(void);

/*
 * qdf_ssr_driver_dump_register_region() - Add a region to the entry list
 * @region_name: name of region to be registered
 * @region_buffer: pointer to region
 * @region_size: size of region in bytes
 *
 * Return:
 *	QDF_STATUS_SUCCESS - Region registration successful
 *	QDF_STATUS_E_NULL_VALUE - Null pointers provided.
 *	QDF_STATUS_E_RESOURCES - Error acquiring/releasing mutex
 *	QDF_STATUS_E_INVAL - Duplicate region registration
 *	QDF_STATUS_E_NOMEM - Not enough space for another region
 */
QDF_STATUS
qdf_ssr_driver_dump_register_region(char *region_name, void *region_buffer,
				    size_t region_size);

/*
 * qdf_ssr_driver_dump_unregister_region() - Add a client to the entry list
 * @region_name: name of region to be unregistered
 *
 * Return:
 *	QDF_STATUS_SUCCESS - Region unregistration successful
 *	QDF_STATUS_E_NULL_VALUE - Null pointer provided.
 *	QDF_STATUS_E_RESOURCES - Error acquiring/releasing mutex
 *	QDF_STATUS_E_INVAL - Region not found
 */
QDF_STATUS qdf_ssr_driver_dump_unregister_region(char *region_name);

/*
 * qdf_ssr_driver_dump_retrieve_regions() - Retrieve list of clients
 * @input_array: pointer to an array of cnss_ssr_driver_dump_entry, which
 *		 will be filled with registered clients by this function.
 * @num_entries_retrieved: pointer to a variable which will be filled with
 *			   number of regions added.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - Region retrieval successful
 *	QDF_STATUS_E_NULL_VALUE - Null pointers provided
 *	QDF_STATUS_E_INVAL - Error retrieving regions
 *	QDF_STATUS_E_RESOURCES - Error acquiring/releasing mutex
 */
QDF_STATUS
qdf_ssr_driver_dump_retrieve_regions(qdf_ssr_driver_dump_entry *input_array,
				     size_t *num_entries_retrieved);
#else

static inline QDF_STATUS
qdf_ssr_driver_dump_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
qdf_ssr_driver_dump_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
qdf_ssr_driver_dump_register_region(char *region_name, void *region_buffer,
				    size_t region_size)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
qdf_ssr_driver_dump_unregister_region(char *region_name)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* #ifdef WLAN_FEATURE_SSR_DRIVER_DUMP */
#endif
