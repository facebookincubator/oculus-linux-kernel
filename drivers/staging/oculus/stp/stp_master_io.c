/*
 * SPI STP Master IO code
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

#include "stp_master.h"
#include "stp_master_common.h"
#include "stp_pipeline.h"

/*  Public APIs
 *-------------------------------------------------
 *-------------------------------------------------
 */

int stp_check_for_rw_errors(void)
{
	int ret = STP_SUCCESS;
	unsigned int synced;

	stp_get(STP_ATTRIB_SYNCED, &synced);
	if (!synced) {
		ret = STP_ERROR_NOT_SYNCED;
		goto error;
	}

	if (!_stp_data->valid_session) {
		ret = STP_ERROR_INVALID_SESSION;
		goto error;
	}

	if (!_stp_data->master_connected) {
		ret = STP_ERROR_MASTER_NOT_CONNECTED;
		goto error;
	}

	if (!_stp_data->slave_connected) {
		ret = STP_ERROR_SLAVE_NOT_CONNECTED;
		goto error;
	}

error:
	return ret;
}

/* Read data non-blocking mode */
int
stp_read_nb(U8 *buffer, UINT buffer_size, UINT *data_size)
{
	int ret = STP_SUCCESS;
	unsigned int data_av;

	STP_ASSERT(_stp_data, "Internal data not initialized!");

	if (!buffer || !data_size) {
		STP_LOG_ERROR("STP read_nb: invalid params!%c", STP_NL);
		return STP_ERROR;
	}

	STP_LOCK(_stp_data->rx_pl.lock);

	ret = stp_check_for_rw_errors();
	if (ret != STP_SUCCESS)
		goto error;

	*data_size = buffer_size;

	stp_pl_get_data_size(&_stp_data->rx_pl, &data_av);

	if (data_av < buffer_size)
		*data_size = data_av;

	if (*data_size > 0)
		stp_pl_get_data(&_stp_data->rx_pl, buffer, *data_size);

	stp_master_signal_data();

error:
	STP_UNLOCK(_stp_data->rx_pl.lock);

	return ret;
}

/* Read data in blocking mode */
int
stp_read(U8 *buffer, UINT buffer_size, UINT *data_size)
{
	int ret = STP_SUCCESS;
	unsigned int remaining_data = buffer_size;
	unsigned int data_read = 0;
	U8 *p = buffer;

	if (!buffer || !data_size) {
		STP_LOG_ERROR("STP read error: invalid parameters!%c", STP_NL);
		return STP_ERROR;
	}

	while (1) {
		ret = stp_read_nb(p, remaining_data, &data_read);
		if (ret != STP_SUCCESS)
			goto error;

		if (data_read > 0) {
			p += data_read;
			remaining_data -= data_read;
		}

		if (remaining_data == 0)
			break;

		ret = _stp_data->wait_signal->wait_read();
		if (ret != 0)
			STP_LOG_ERROR("stp_read intrerupted!");
	}

	*data_size = buffer_size;

error:
	return ret;
}

/* Write data in blocking mode */
int
stp_write(const U8 *buffer, UINT buffer_size, UINT *data_size)
{
	int ret = STP_SUCCESS;
	unsigned int size_av;
	unsigned int remaining_data = buffer_size;
	const U8 *p = buffer;

	while (1) {
		STP_LOCK(_stp_data->tx_pl.lock);

		ret = stp_check_for_rw_errors();
		if (ret != STP_SUCCESS) {
			STP_UNLOCK(_stp_data->tx_pl.lock);
			goto error;
		}
		stp_pl_get_available_space(&_stp_data->tx_pl, &size_av);

		if (size_av > remaining_data) {
			stp_pl_add_data(&_stp_data->tx_pl, p, remaining_data);
			remaining_data = 0;
		} else if (size_av > 0) {
			stp_pl_add_data(&_stp_data->tx_pl, p, size_av);
			remaining_data -= size_av;
			p += size_av;
		}

		stp_master_signal_data();

		STP_UNLOCK(_stp_data->tx_pl.lock);

		if (remaining_data == 0)
			break;

		ret = _stp_data->wait_signal->wait_write();
		if (ret < 0) {
			ret = stp_check_for_rw_errors();

			if (ret == STP_SUCCESS)
				ret = STP_ERROR_IO_INTRERRUPT;
			goto error;
		}
	}

	*data_size = buffer_size - remaining_data;

error:
	return ret;
}

int stp_write_list(struct stp_write_object *list, unsigned int list_size)
{
	int ret = STP_SUCCESS;
	 unsigned int i;

	if (!list || !list_size) {
		STP_LOG_ERROR("STP write error: invalid params!%c", STP_NL);
		return STP_ERROR_INVALID_PARAMETERS;
	}

	for (i = 0; i < list_size; i++)
		if (!list[i].buffer || !list[i].size || !list[i].data_size)
			return STP_ERROR_INVALID_PARAMETERS;

	for (i = 0; i < list_size; i++) {
		ret = stp_write(list[i].buffer, list[i].size,
			list[i].data_size);
		if (ret != STP_SUCCESS)
			break;
	}

	return ret;
}

/* Write data in non-blocking mode */
int
stp_write_nb(const U8 *buffer, UINT buffer_size, UINT *data_size)
{
	int ret = STP_SUCCESS;
	unsigned int size_av;

	STP_ASSERT(_stp_data, "Internal data not initialized!");

	if (!buffer || !data_size) {
		STP_LOG_ERROR("STP write_nb error: invalid params!%c", STP_NL);
		return STP_ERROR;
	}

	STP_LOCK(_stp_data->tx_pl.lock);

	ret = stp_check_for_rw_errors();
	if (ret != STP_SUCCESS)
		goto error;

	stp_pl_get_available_space(&_stp_data->tx_pl, &size_av);

	*data_size = buffer_size;
	if (size_av < buffer_size)
		*data_size = size_av;

	if (*data_size > 0)
		stp_pl_add_data(&_stp_data->tx_pl, buffer, *data_size);

	stp_master_signal_data();

error:
	STP_UNLOCK(_stp_data->tx_pl.lock);

	return ret;
}

/* Get attributes */
int
stp_get(UINT attribute, void *param)
{
	int ret = STP_SUCCESS;
	unsigned int *p_value;
	struct stp_statistics *p_stats;

	STP_ASSERT(_stp_data, "Internal data not initialized!");

	if (!param) {
		STP_LOG_ERROR("STP get error: invalid params!%c", STP_NL);
		return STP_ERROR;
	}

	switch (attribute) {
	case STP_TX_AVAILABLE:
		p_value = (unsigned int *)param;
		stp_pl_get_available_space(&_stp_data->tx_pl, p_value);
		break;
	case STP_TX_DATA:
		p_value = (unsigned int *)param;
		stp_pl_get_data_size(&_stp_data->tx_pl, p_value);
		break;
	case STP_RX_DATA:
		p_value = (unsigned int *)param;
		stp_pl_get_data_size(&_stp_data->rx_pl, p_value);
		break;
	case STP_STATS:
		p_stats = (struct stp_statistics *)param;
		memcpy(p_stats, &_stp_stats, sizeof(_stp_stats));
		break;
	case STP_WAIT_FOR_SLAVE:
		p_value = (unsigned int *)param;
		*p_value = _stp_debug.wait_for_slave ? 1 : 0;
		break;
	case STP_WAIT_FOR_DATA:
		p_value = (unsigned int *)param;
		*p_value = _stp_debug.wait_for_data ? 1 : 0;
		break;
	case STP_ATTRIB_SYNCED:
		p_value = (unsigned int *)param;
		*p_value = (_stp_data->state == STP_STATE_INIT) ? 0 : 1;
		break;
	case STP_ATTRIB_VALID_SESSION:
		p_value = (unsigned int *)param;
		*p_value = (unsigned int)_stp_data->valid_session;
		break;
	case STP_ATTRIB_SLAVE_CONNECTED:
		p_value = (unsigned int *)param;
		*p_value = (unsigned int)_stp_data->slave_connected;
		break;
	case STP_ATTRIB_MASTER_CONNECTED:
		p_value = (unsigned int *)param;
		*p_value = (unsigned int)_stp_data->master_connected;
		break;

	default:
		STP_LOG_ERROR("STP get error: invalid attribute!%c", STP_NL);
		ret = STP_ERROR;
		break;
	}

	return ret;
}

/* Set attributes (TBD) */
int
stp_set(UINT attribute, void *param)
{
	int ret = STP_SUCCESS;

	STP_ASSERT(_stp_data, "Internal data not initialized!");

	if (!param) {
		STP_LOG_ERROR("STP set error: invalid params!%c",
			STP_NL);
		ret = STP_ERROR;
		goto error;
	}

	switch (attribute) {
	default:
		STP_LOG_ERROR("STP set: invalid attribute!%c", STP_NL);
		ret = STP_ERROR;
		goto error;
	}

error:
	return ret;
}

/* Set callback */
int
stp_set_callback(void (*callback)(int))
{
	int ret = STP_SUCCESS;

	STP_ASSERT(_stp_data, "Internal data not initialized!");

	if (!callback) {
		STP_LOG_ERROR("STP error: invalid params!\n");
		ret = STP_ERROR;
		goto error;
	}

	_stp_data->callback_client = callback;

error:
	return ret;
}

int stp_open(void)
{
	unsigned int synced;
	int ret = STP_SUCCESS;

	if (_stp_data->master_connected)
		return STP_ERROR_MASTER_ALREADY_OPEN;

	stp_get(STP_ATTRIB_SYNCED, &synced);
	if (synced == 0)
		return STP_ERROR_NOT_SYNCED;

	stp_pl_reset(&_stp_data->tx_pl);
	stp_pl_reset(&_stp_data->rx_pl);

	_stp_data->master_connected = true;
	_stp_data->valid_session = true;

	stp_set_notification(STP_IN_CONNECTED);
	stp_ie_record(STP_IE_MASTER_CONNECTED, 0);

	return ret;
}

int stp_close(void)
{
	int ret = STP_SUCCESS;

	if (!_stp_data->master_connected)
		return STP_ERROR_MASTER_ALREADY_CLOSED;

	stp_disconnect();

	return ret;
}

