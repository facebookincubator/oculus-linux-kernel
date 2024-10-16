/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "qdf_file.h"
#include "qdf_module.h"
#include "qdf_parse.h"
#include "qdf_status.h"
#include "qdf_str.h"
#include "qdf_trace.h"
#include "qdf_types.h"

#ifdef WLAN_USE_CONFIG_PARAMS
#define QDF_SECTION_FOUND break
#else
#define QDF_SECTION_FOUND continue
#endif

static QDF_STATUS qdf_ini_read_values(char **main_cursor,
				      char **read_key, char **read_value,
				      bool *section_item)
{
	char *cursor = *main_cursor;

	/* foreach line */
	while (*cursor != '\0') {
		char *key = cursor;
		char *value = NULL;
		bool comment = false;
		bool eol = false;

		/*
		 * Look for the end of the line, while noting any
		 * value ('=') or comment ('#') indicators
		 */
		while (!eol) {
			switch (*cursor) {
			case '\r':
			case '\n':
				*cursor = '\0';
				cursor++;
				fallthrough;
			case '\0':
				eol = true;
				break;

			case '=':
				/*
				 * The first '=' is the value indicator.
				 * Subsequent '=' are valid value characters.
				 */
				if (!value && !comment) {
					value = cursor + 1;
					*cursor = '\0';
				}

				cursor++;
				break;

			case '#':
				/*
				 * We don't process comments, so we can null-
				 * terminate unconditionally here (unlike '=').
				 */
				comment = true;
				*cursor = '\0';
				fallthrough;
			default:
				cursor++;
				break;
			}
		}

		key = qdf_str_trim(key);
		/*
		 * Ignoring comments, a valid ini line contains one of:
		 *	1) some 'key=value' config item
		 *	2) section header
		 *	3) a line containing whitespace
		 */
		if (value) {
			*read_key = key;
			*read_value = value;
			*section_item = 0;
			*main_cursor = cursor;
			return QDF_STATUS_SUCCESS;
		} else if (key[0] == '[') {
			qdf_size_t len = qdf_str_len(key);

			if (key[len - 1] != ']') {
				qdf_err("Invalid *.ini syntax '%s'", key);
				return QDF_STATUS_E_INVAL;
			} else {
				key[len - 1] = '\0';
				*read_key = key + 1;
				*section_item = 1;
				*main_cursor = cursor;
				return QDF_STATUS_SUCCESS;
			}
		} else if (key[0] != '\0') {
			qdf_err("Invalid *.ini syntax '%s'", key);
			return QDF_STATUS_E_INVAL;
		}

		/* skip remaining EoL characters */
		while (*cursor == '\n' || *cursor == '\r')
			cursor++;
	}

	return QDF_STATUS_E_INVAL;
}

QDF_STATUS qdf_ini_parse(const char *ini_path, void *context,
			 qdf_ini_item_cb item_cb, qdf_ini_section_cb section_cb)
{
	QDF_STATUS status;
	char *read_key;
	char *read_value;
	bool section_item;
	int ini_read_count = 0;
	char *fbuf;
	char *cursor;

	if (qdf_str_eq(QDF_WIFI_MODULE_PARAMS_FILE, ini_path))
		status = qdf_module_param_file_read(ini_path, &fbuf);
	else
		status = qdf_file_read(ini_path, &fbuf);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_err("Failed to read *.ini file @ %s", ini_path);
		return status;
	}

	/* foreach line */
	cursor = fbuf;

	while (qdf_ini_read_values(&cursor, &read_key, &read_value,
				   &section_item) == QDF_STATUS_SUCCESS) {
		if (!section_item) {
			status = item_cb(context, read_key, read_value);
			if (QDF_IS_STATUS_ERROR(status))
				break;
			else
				ini_read_count++;
		} else  {
			qdf_debug("Section started in global file");
		/* Currently AP Platforms supports and uses Sections,
		 * hence break the loop, sections will be parsed separately,
		 * in case of non AP platforms, sections are used as
		 * logical separators hence continue reading the values.
		 */
			QDF_SECTION_FOUND;
		}
	}

	qdf_info("INI values read: %d", ini_read_count);
	if (ini_read_count != 0) {
		qdf_info("INI file parse successful");
		status = QDF_STATUS_SUCCESS;
	} else {
		qdf_info("INI file parse fail: invalid file format");
		status = QDF_STATUS_E_INVAL;
	}

	if (qdf_str_eq(QDF_WIFI_MODULE_PARAMS_FILE, ini_path))
		qdf_module_param_file_free(fbuf);
	else
		qdf_file_buf_free(fbuf);

	return status;
}

qdf_export_symbol(qdf_ini_parse);

QDF_STATUS qdf_ini_section_parse(const char *ini_path, void *context,
				 qdf_ini_item_cb item_cb,
				 const char *section_name)
{
	QDF_STATUS status;
	char *read_key;
	char *read_value;
	bool section_item;
	bool section_found = 0;
	bool section_complete = 0;
	int ini_read_count = 0;
	char *fbuf;
	char *cursor;

	if (qdf_str_eq(QDF_WIFI_MODULE_PARAMS_FILE, ini_path))
		status = qdf_module_param_file_read(ini_path, &fbuf);
	else
		status = qdf_file_read(ini_path, &fbuf);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_err("Failed to read *.ini file @ %s", ini_path);
		return status;
	}

	/* foreach line */
	cursor = fbuf;

	while (qdf_ini_read_values(&cursor, &read_key, &read_value,
				   &section_item) == QDF_STATUS_SUCCESS) {
		if (section_item) {
			if (qdf_str_cmp(read_key, section_name) == 0) {
				section_found = 1;
				section_complete = 0;
			} else {
				if (section_found == 1)
					section_complete = 1;
				section_found = 0;
			}
		} else if (section_found) {
			status = item_cb(context, read_key, read_value);
			if (QDF_IS_STATUS_ERROR(status))
				break;
			else
				ini_read_count++;
		} else if (section_complete) {
			break;
		}
	}

	qdf_info("INI values parse successful read: %d from section %s",
		 ini_read_count, section_name);

	if (ini_read_count != 0) {
		status = QDF_STATUS_SUCCESS;
	} else {
		qdf_debug("INI file parse fail: Section not found %s",
			  section_name);
		status = QDF_STATUS_SUCCESS;
	}

	if (qdf_str_eq(QDF_WIFI_MODULE_PARAMS_FILE, ini_path))
		qdf_module_param_file_free(fbuf);
	else
		qdf_file_buf_free(fbuf);

	return status;
}

qdf_export_symbol(qdf_ini_section_parse);

static bool is_valid_key(char **main_cursor)
{
	char *cursor = *main_cursor;

	while (*cursor != '\0') {
		unsigned char *val = (unsigned char *)cursor;

		switch (*cursor) {
		case '\r':
		case '\n':
			cursor++;
			break;
		case '\0':
			break;
		case '=':
		case '#':
		case ']':
		case '[':
		case '_':
		case '-':
		case ' ':
		case ':':
			cursor++;
			break;

		default:
			if (!isalnum(*val)) {
				qdf_err("Found invalid character %c", *cursor);
				return false;
			}
			cursor++;
			break;
		}
	}
	return true;
}

bool qdf_valid_ini_check(const char  *ini_path)
{
	QDF_STATUS status;
	char *fbuf;
	char *cursor;
	bool is_valid = false;

	if (qdf_str_eq(QDF_WIFI_MODULE_PARAMS_FILE, ini_path))
		status = qdf_module_param_file_read(ini_path, &fbuf);
	else
		status = qdf_file_read(ini_path, &fbuf);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_err("Failed to read *.ini file @ %s", ini_path);
		return false;
	}

	/* foreach line */
	cursor = fbuf;

	is_valid = is_valid_key(&cursor);

	if (qdf_str_eq(QDF_WIFI_MODULE_PARAMS_FILE, ini_path))
		qdf_module_param_file_free(fbuf);
	else
		qdf_file_buf_free(fbuf);

	return is_valid;
}

qdf_export_symbol(qdf_valid_ini_check);
