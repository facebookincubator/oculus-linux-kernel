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

#include "qdf_ssr_driver_dump.h"
#include "qdf_lock.h"
#include "qdf_str.h"
#include <qdf_trace.h>
#include <qdf_parse.h>
#include <qdf_module.h>
#include <qdf_util.h>
#include <qdf_mem.h>
#include <qdf_types.h>

static qdf_mutex_t region_list_mutex;

static struct cnss_ssr_driver_dump_entry
dump_entry_list[CNSS_HOST_DUMP_TYPE_MAX];

static size_t num_of_regions_registered;

QDF_STATUS qdf_ssr_driver_dump_init(void)
{
	QDF_STATUS status;

	status = qdf_mutex_create(&region_list_mutex);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	num_of_regions_registered = 0;
	qdf_mem_zero(dump_entry_list, sizeof(dump_entry_list));
	return status;
}

QDF_STATUS qdf_ssr_driver_dump_deinit(void)
{
	QDF_STATUS status;

	status = qdf_mutex_destroy(&region_list_mutex);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	if (num_of_regions_registered > 0)
		qdf_warn("deiniting with regions still registered");
	num_of_regions_registered = 0;
	return status;
}

static struct cnss_ssr_driver_dump_entry *
qdf_ssr_driver_dump_find_next_free_entry(void)
{
	int i;

	for (i = 0; i < CNSS_HOST_DUMP_TYPE_MAX; i++) {
		if (!dump_entry_list[i].buffer_pointer)
			return &dump_entry_list[i];
	}
	return NULL;
}

static struct cnss_ssr_driver_dump_entry *
qdf_ssr_driver_dump_find_entry_by_name(char *region_name)
{
	int i;

	for (i = 0; i < CNSS_HOST_DUMP_TYPE_MAX; i++) {
		if (qdf_str_eq(dump_entry_list[i].region_name, region_name) &&
		    dump_entry_list[i].buffer_pointer) {
			return &dump_entry_list[i];
		}
	}

	return NULL;
}

QDF_STATUS
qdf_ssr_driver_dump_register_region(char *region_name, void *region_buffer,
				    size_t region_size)
{
	QDF_STATUS status;
	struct cnss_ssr_driver_dump_entry *entry;

	if (!region_buffer || !region_name) {
		qdf_err("null region pointer or region_name");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = qdf_mutex_acquire(&region_list_mutex);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_err("couldn't acquire lock");
		return QDF_STATUS_E_RESOURCES;
	}

	entry = qdf_ssr_driver_dump_find_entry_by_name(region_name);
	if (entry) {
		qdf_err("duplicate registration of %s", region_name);
		status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	entry = qdf_ssr_driver_dump_find_next_free_entry();
	if (!entry) {
		qdf_err("too many entries: %d, cannot insert %s",
			num_of_regions_registered, region_name);
		status = QDF_STATUS_E_NOMEM;
		goto ret;
	}

	entry->buffer_pointer = region_buffer;
	entry->buffer_size = region_size;
	qdf_str_lcopy(entry->region_name, region_name,
		      sizeof(entry->region_name));
	num_of_regions_registered++;
	goto ret;

ret:
	if (QDF_IS_STATUS_ERROR(qdf_mutex_release(&region_list_mutex))) {
		qdf_err("error releasing lock");
		return QDF_STATUS_E_RESOURCES;
	}
	return status;
}

QDF_STATUS qdf_ssr_driver_dump_unregister_region(char *region_name)
{
	QDF_STATUS status;
	struct cnss_ssr_driver_dump_entry *entry;

	if (!region_name) {
		qdf_err("null region_name");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = qdf_mutex_acquire(&region_list_mutex);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_err("couldn't acquire lock");
		return QDF_STATUS_E_RESOURCES;
	}

	entry = qdf_ssr_driver_dump_find_entry_by_name(region_name);
	if (!entry) {
		qdf_err("couldn't find entry: %s", region_name);
		status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	entry->buffer_pointer = NULL;
	num_of_regions_registered--;

ret:
	if (QDF_IS_STATUS_ERROR(qdf_mutex_release(&region_list_mutex))) {
		qdf_err("error releasing lock");
		return QDF_STATUS_E_RESOURCES;
	}
	return status;
}

QDF_STATUS
qdf_ssr_driver_dump_retrieve_regions(qdf_ssr_driver_dump_entry
				     *input_array,
				     size_t *num_entries_loaded)
{
	QDF_STATUS status;
	int i;
	size_t input_index = 0;

	if (!input_array || !num_entries_loaded) {
		qdf_err("null input_array or num_entries_loaded");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = qdf_mutex_acquire(&region_list_mutex);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_err("couldn't acquire lock");
		return QDF_STATUS_E_RESOURCES;
	}

	for (i = 0; i < CNSS_HOST_DUMP_TYPE_MAX; i++) {
		if (dump_entry_list[i].buffer_pointer) {
			qdf_mem_copy(&input_array[input_index],
				     &dump_entry_list[i],
				     sizeof(dump_entry_list[i]));
			input_index++;
		}
	}
	if (input_index != num_of_regions_registered) {
		qdf_err("num entries mismatch index:%d num reg registered:%d",
			input_index, num_of_regions_registered);
		status = QDF_STATUS_E_INVAL;
		goto ret;
	}
	*num_entries_loaded = input_index;

ret:
	if (QDF_IS_STATUS_ERROR(qdf_mutex_release(&region_list_mutex))) {
		qdf_err("error releasing lock");
		return QDF_STATUS_E_RESOURCES;
	}
	return status;
}
