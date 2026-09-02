/*
 * SPI STP Controller IO code
 *
 * Copyright (C) 2020 Eugen Pirvu
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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

#include <stp/common/stp_pipeline.h>
#include <stp/common/stp_common.h>
#include <stp/common/stp_logging.h>
#include <stp/controller/stp_controller.h>
#include <stp/controller/stp_controller_common.h>

/*  Public APIs
 *-------------------------------------------------
 *-------------------------------------------------
 */

static int32_t stp_controller_check_for_rw_errors(uint8_t channel)
{
	int32_t ret = STP_SUCCESS;
	uint32_t synced;
	uint32_t service_interruption = false;

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	ret = stp_controller_get_attribute(STP_ATTRIB_SYNCED, &synced);
	if (ret != STP_SUCCESS)
		goto error;

	if (!synced) {
		ret = STP_ERROR_NOT_SYNCED;
		if (_stp_controller_data->handshake
			    ->device_request_transaction())
			stp_controller_signal_start_transaction();
		goto error;
	}

	stp_controller_get_attribute(STP_ATTRIB_SERVICE_INTERRUPTION,
				     &service_interruption);
	if (service_interruption) {
		ret = STP_ERROR_SERVICE_INTERRUPTION;
		goto error;
	}

	if (!_stp_controller_data->channels[channel].valid_session) {
		ret = STP_ERROR_INVALID_SESSION;
		goto error;
	}

	if (!_stp_controller_data->channels[channel].controller_connected) {
		ret = STP_ERROR_CONTROLLER_NOT_CONNECTED;
		goto error;
	}

	if (!_stp_controller_data->channels[channel].device_connected) {
		ret = STP_ERROR_DEVICE_NOT_CONNECTED;
		goto error;
	}

error:
	return ret;
}

/* Read data non-blocking mode */
int32_t stp_controller_read_nb(uint8_t channel, uint8_t *buffer,
			       uint32_t buffer_size, uint32_t *data_size)
{
	int32_t ret = STP_SUCCESS;
	uint32_t data_av;

	if (!_stp_controller_data) {
		STP_LOG_ERROR("Internal data not initialized!");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	if (!buffer || !data_size) {
		STP_LOG_ERROR("STP read_nb: invalid params!");
		return STP_ERROR;
	}

	STP_LOCK(_stp_controller_data->channels[channel].rx_pl.lock);

	ret = stp_controller_check_for_rw_errors(channel);
	if (ret != STP_SUCCESS)
		goto error;

	*data_size = buffer_size;

	stp_pl_get_data_size(&_stp_controller_data->channels[channel].rx_pl,
			     &data_av);

	if (data_av < buffer_size)
		*data_size = data_av;

	if (*data_size > 0)
		stp_pl_get_data(&_stp_controller_data->channels[channel].rx_pl,
				buffer, *data_size);

	if (stp_channel_status_change(
		    channel, _stp_controller_data->channels,
		    _stp_controller_data->prev_channels_status)) {
		STP_LOCK(_stp_controller_data->lock_set_has_data);
		stp_controller_set_controller_has_data(true);
		STP_UNLOCK(_stp_controller_data->lock_set_has_data);
	}

error:
	STP_UNLOCK(_stp_controller_data->channels[channel].rx_pl.lock);

	return ret;
}

/* Read data, blocking until buffer_size bytes become available. */
int32_t stp_controller_read(uint8_t channel, uint8_t *buffer,
			    uint32_t buffer_size, uint32_t *data_size)
{
	return stp_controller_read_minimum(channel, buffer, buffer_size,
					   data_size, buffer_size);
}

/* Read a minimum number of bytes, up to the specified buffer size. Blocks until the minimum becomes
 * available.
 */
int32_t stp_controller_read_minimum(uint8_t channel, uint8_t *buffer,
				    uint32_t buffer_size, uint32_t *data_size,
				    uint32_t minimum)
{
	int32_t ret = STP_SUCCESS;
	uint32_t data_read = 0;
	uint32_t remaining_data = buffer_size;
	uint32_t data_read_total = 0;
	uint8_t *p = buffer;

	if (!buffer || !data_size || (minimum > buffer_size)) {
		STP_LOG_ERROR("STP read error: invalid parameters!");
		return STP_ERROR;
	}

	ret = stp_controller_check_for_rw_errors(channel);
	if (ret != STP_SUCCESS)
		return ret;

	while (1) {
		ret = stp_controller_read_nb(channel, p, remaining_data,
					     &data_read);
		if (ret != STP_SUCCESS)
			goto error;

		if (data_read > 0) {
			p += data_read;
			remaining_data -= data_read;
			data_read_total += data_read;

			STP_LOG_DEBUG(
				"Controller read_minimum: minimum: %zu, read:%zu, total:%zu, remaining:%zu",
				(size_t)minimum, (size_t)data_read,
				(size_t)data_read_total,
				(size_t)remaining_data);
		}

		if (data_read_total >= minimum)
			break;

		ret = _stp_controller_data->wait_signal->wait_read(channel);
		if (ret < 0) {
			ret = STP_ERROR_IO_INTERRUPT;
			break;
		}
	}

	*data_size = data_read_total;

error:
	return ret;
}

/* Write data in blocking mode */
int32_t stp_controller_write(uint8_t channel, const uint8_t *buffer,
			     uint32_t buffer_size, uint32_t *data_size)
{
	int32_t ret = STP_SUCCESS;
	uint32_t size_av;
	uint32_t remaining_data = buffer_size;
	const uint8_t *p = buffer;

	if (!buffer || !data_size) {
		STP_LOG_ERROR("STP write error: invalid parameters!");
		return STP_ERROR;
	}

	ret = stp_controller_check_for_rw_errors(channel);
	if (ret != STP_SUCCESS)
		return ret;

	_stp_controller_data->wait_signal->reset_fsync(channel);

	while (1) {
		STP_LOCK(_stp_controller_data->channels[channel].tx_pl.lock);

		ret = stp_controller_check_for_rw_errors(channel);
		if (ret != STP_SUCCESS) {
			STP_UNLOCK(_stp_controller_data->channels[channel]
					   .tx_pl.lock);
			goto error;
		}
		stp_pl_get_available_space(
			&_stp_controller_data->channels[channel].tx_pl,
			&size_av);

		if (size_av > remaining_data) {
			stp_pl_add_data(
				&_stp_controller_data->channels[channel].tx_pl,
				p, remaining_data);
			remaining_data = 0;
		} else if (size_av > 0) {
			stp_pl_add_data(
				&_stp_controller_data->channels[channel].tx_pl,
				p, size_av);
			remaining_data -= size_av;
			p += size_av;
		}

		STP_UNLOCK(_stp_controller_data->channels[channel].tx_pl.lock);

		STP_LOCK(_stp_controller_data->lock_set_has_data);
		stp_controller_set_controller_has_data(true);
		STP_UNLOCK(_stp_controller_data->lock_set_has_data);

		if (remaining_data == 0)
			break;

		ret = _stp_controller_data->wait_signal->wait_write(channel);
		if (ret < 0) {
			ret = stp_controller_check_for_rw_errors(channel);

			if (ret == STP_SUCCESS)
				ret = STP_ERROR_IO_INTERRUPT;
			goto error;
		}
	}

	*data_size = buffer_size - remaining_data;

	return ret;

error:
	/* If there is an error, we need to signal fsync to unblock the clients */
	_stp_controller_data->wait_signal->signal_fsync(channel);
	return ret;
}

/* Write data in non-blocking mode */
int32_t stp_controller_write_nb(uint8_t channel, const uint8_t *buffer,
				uint32_t buffer_size, uint32_t *data_size)
{
	int32_t ret = STP_SUCCESS;
	uint32_t size_av;

	if (!_stp_controller_data) {
		STP_LOG_ERROR("Internal data not initialized!");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	if (!buffer || !data_size) {
		STP_LOG_ERROR("STP write_nb error: invalid params!");
		return STP_ERROR;
	}

	STP_LOCK(_stp_controller_data->channels[channel].tx_pl.lock);

	ret = stp_controller_check_for_rw_errors(channel);
	if (ret != STP_SUCCESS)
		goto error;

	stp_pl_get_available_space(
		&_stp_controller_data->channels[channel].tx_pl, &size_av);

	*data_size = buffer_size;
	if (size_av < buffer_size)
		*data_size = size_av;

	if (*data_size > 0) {
		_stp_controller_data->wait_signal->reset_fsync(channel);
		stp_pl_add_data(&_stp_controller_data->channels[channel].tx_pl,
				buffer, *data_size);
		STP_LOCK(_stp_controller_data->lock_set_has_data);
		stp_controller_set_controller_has_data(true);
		STP_UNLOCK(_stp_controller_data->lock_set_has_data);
	}

	STP_UNLOCK(_stp_controller_data->channels[channel].tx_pl.lock);

	return ret;

error:
	STP_UNLOCK(_stp_controller_data->channels[channel].tx_pl.lock);
	/* If there is an error, we need to signal fsync to unblock the clients */
	_stp_controller_data->wait_signal->signal_fsync(channel);
	return ret;
}

/* Get attributes */
int32_t stp_controller_get_attribute(uint32_t attribute, void *param)
{
	int32_t ret = STP_SUCCESS;
	uint32_t *p_value;

	if (!_stp_controller_data) {
		STP_LOG_ERROR("Internal data not initialized!");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	if (!param) {
		STP_LOG_ERROR("STP get error: invalid params!");
		return STP_ERROR;
	}

	switch (attribute) {
	case STP_ATTRIB_SYNCED:
		p_value = (uint32_t *)param;
		*p_value =
			(_stp_controller_data->state == STP_STATE_INIT) ? 0 : 1;
		break;
	case STP_ATTRIB_CHANNELS_STATUS:
		p_value = (uint32_t *)param;
		*p_value =
			(uint32_t)_stp_controller_data->device_channels_status;
		break;
	case STP_ATTRIB_SERVICE_INTERRUPTION:
		p_value = (uint32_t *)param;
		*p_value = (_stp_controller_data->service_interruption) ? 1 : 0;
		break;
	default:
		STP_LOG_ERROR("STP get error: invalid attribute!");
		ret = STP_ERROR;
		break;
	}

	return ret;
}

static const char *stp_state_name(uint32_t state)
{
	return state == STP_STATE_INIT ? "INIT" :
	       (state == STP_STATE_DATA ? "DATA" : "UNKNOWN");
}

void stp_dump_controller_state(const char *reason)
{
	struct stp_type *c = _stp_controller_data;
	uint32_t state;

	if (!c) {
		STP_LOG_ERROR("[STPDump] controller (%s): not initialized",
			      reason ? reason : "");
		return;
	}

	state = c->state;
	STP_LOG_ERROR(
		"[STPDump] controller (%s): state=0x%x(%s) start_txn=%d stop_thread=%d suspend=%d svc_intr=%d pending_dev_ready=%d",
		reason ? reason : "", state,
		state == STP_STATE_INIT ? "INIT" :
			(state == STP_STATE_DATA ? "DATA" : "UNKNOWN"),
		c->start_transaction, c->stop_thread, c->suspend,
		c->service_interruption, c->pending_device_ready_signal);

	STP_LOG_ERROR(
		"[STPDump] controller (%s): dev_ch_status=0x%x prev_dev_ch_status=0x%x prev_ch_status=0x%x last_tx_noti=0x%x pending{tx.sent=%d tx.ch=%u tx_noti=0x%x} bad_crcs=%zu",
		reason ? reason : "",
		(uint32_t)c->device_channels_status,
		(uint32_t)c->prev_device_channels_status,
		(uint32_t)c->prev_channels_status, c->last_tx_notification,
		c->pending.tx.sent, c->pending.tx.channel,
		c->pending.tx_notification, c->bad_crcs_in_a_row);

	STP_LOG_ERROR(
		"[STPDump] controller (%s): txn_count=%u last_rx_opcode=%u last_rx_crc_ok=%d",
		reason ? reason : "", c->transaction_count, c->last_rx_opcode,
		c->last_rx_crc_ok);

	/* at_ns is when the link flipped into that state, so current tells you
	 * how long it has been stuck and previous what it flipped out of.
	 */
	STP_LOG_ERROR(
		"[STPDump] controller (%s): sync{current=%s at_ns=%llu previous=%s at_ns=%llu}",
		reason ? reason : "",
		c->sync_current_state ?
			stp_state_name(c->sync_current_state) : "NONE",
		(unsigned long long)c->sync_current_ns,
		c->sync_previous_state ?
			stp_state_name(c->sync_previous_state) : "NONE",
		(unsigned long long)c->sync_previous_ns);
}
EXPORT_SYMBOL(stp_dump_controller_state);

/* Get channel attributes */
int32_t stp_controller_get_channel_attribute(uint8_t channel,
					     uint32_t attribute, void *param)
{
	int32_t ret = STP_SUCCESS;
	uint32_t *p_value;

	if (!_stp_controller_data) {
		STP_LOG_ERROR("Internal data not initialized!");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	if (!param) {
		STP_LOG_ERROR("STP get error: invalid params!");
		return STP_ERROR;
	}

	switch (attribute) {
	case STP_TX_AVAILABLE:
		p_value = (uint32_t *)param;
		stp_pl_get_available_space(
			&_stp_controller_data->channels[channel].tx_pl,
			p_value);
		break;
	case STP_TX_DATA:
		p_value = (uint32_t *)param;
		stp_pl_get_data_size(
			&_stp_controller_data->channels[channel].tx_pl,
			p_value);
		break;
	case STP_RX_FILLED:
		p_value = (uint32_t *)param;
		stp_pl_get_data_size(
			&_stp_controller_data->channels[channel].rx_pl,
			p_value);
		break;
	case STP_ATTRIB_VALID_SESSION:
		p_value = (uint32_t *)param;
		*p_value = (uint32_t)_stp_controller_data->channels[channel]
				   .valid_session;
		break;
	case STP_ATTRIB_DEVICE_CONNECTED:
		p_value = (uint32_t *)param;
		*p_value = (uint32_t)_stp_controller_data->channels[channel]
				   .device_connected;
		break;
	case STP_ATTRIB_CONTROLLER_CONNECTED:
		p_value = (uint32_t *)param;
		*p_value = (uint32_t)_stp_controller_data->channels[channel]
				   .controller_connected;
		break;
	case STP_CONTROLLER_ATTRIB_SET_LOG_TX_DATA:
		p_value = (uint32_t *)param;
		*p_value = (uint32_t)_stp_controller_data->channels[channel]
				   .log_tx_data;
		break;
	case STP_CONTROLLER_ATTRIB_SET_LOG_RX_DATA:
		p_value = (uint32_t *)param;
		*p_value = (uint32_t)_stp_controller_data->channels[channel]
				   .log_rx_data;
		break;
	default:
		STP_LOG_ERROR("STP get error: invalid attribute!");
		ret = STP_ERROR;
		break;
	}

	return ret;
}

/* Set attributes */
int32_t stp_controller_set_channel_attribute32(uint8_t channel,
					       uint32_t attribute,
					       uint32_t value)
{
	int32_t ret = STP_SUCCESS;

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	switch (attribute) {
	case STP_CONTROLLER_ATTRIB_SET_LOG_TX_DATA:
		_stp_controller_data->channels[channel].log_tx_data = value;
		break;
	case STP_CONTROLLER_ATTRIB_SET_LOG_RX_DATA:
		_stp_controller_data->channels[channel].log_rx_data = value;
		break;
	default:
		STP_LOG_ERROR("STP set error: invalid attribute!");
		ret = STP_ERROR;
		break;
	}

	return ret;
}

int32_t stp_controller_set(uint32_t attribute, void *param)
{
	int32_t ret = STP_SUCCESS;
	bool *p_value_bool;

	if (!_stp_controller_data) {
		STP_LOG_ERROR("Internal data not initialized!");
		ret = STP_ERROR_INVALID_PARAMETERS;
		goto error;
	}

	if (!param) {
		STP_LOG_ERROR("STP set error: invalid params!");
		ret = STP_ERROR;
		goto error;
	}

	switch (attribute) {
	case STP_ATTRIB_SERVICE_INTERRUPTION:
		p_value_bool = (bool *)param;
		_stp_controller_data->service_interruption = *p_value_bool;
		break;
	default:
		STP_LOG_ERROR("STP set: invalid attribute!");
		ret = STP_ERROR;
		goto error;
	}

error:
	return ret;
}

/* Set callback */
int32_t stp_controller_set_callback(void (*callback)(int))
{
	int32_t ret = STP_SUCCESS;

	if (!_stp_controller_data) {
		STP_LOG_ERROR("Internal data not initialized!");
		ret = STP_ERROR_INVALID_PARAMETERS;
		goto error;
	}

	if (!callback) {
		STP_LOG_ERROR("STP error: invalid params!\n");
		ret = STP_ERROR;
		goto error;
	}

	_stp_controller_data->callback_client = callback;

error:
	return ret;
}

int32_t stp_controller_open(uint8_t channel, uint8_t priority,
			    uint8_t *rx_buffer, size_t rx_buffer_size,
			    uint8_t *tx_buffer, size_t tx_buffer_size)
{
	uint32_t synced;
	int32_t ret = STP_SUCCESS;

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	if (!rx_buffer || !rx_buffer_size || !tx_buffer || !tx_buffer_size) {
		STP_LOG_ERROR("invalid params!");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	if (_stp_controller_data->channels[channel].controller_connected)
		return STP_ERROR_ALREADY_OPEN;

	ret = stp_controller_get_attribute(STP_ATTRIB_SYNCED, &synced);
	if (ret != STP_SUCCESS)
		return ret;
	if (synced == 0)
		return STP_ERROR_NOT_SYNCED;

	stp_pl_init(&_stp_controller_data->channels[channel].rx_pl, rx_buffer,
		    rx_buffer_size);
	stp_pl_init(&_stp_controller_data->channels[channel].tx_pl, tx_buffer,
		    tx_buffer_size);

	_stp_controller_data->channels[channel].controller_connected = true;
	_stp_controller_data->channels[channel].valid_session = true;

	_stp_controller_data->channels[channel].priority = priority;

	stp_controller_set_notification(channel, STP_IN_CONNECTED);

	return ret;
}

int32_t stp_controller_open_blocking(uint8_t channel, uint8_t priority,
				     uint8_t *rx_buffer, size_t rx_buffer_size,
				     uint8_t *tx_buffer, size_t tx_buffer_size)
{
	uint32_t synced;
	int32_t ret = STP_SUCCESS;

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	if (!rx_buffer || !rx_buffer_size || !tx_buffer || !tx_buffer_size) {
		STP_LOG_ERROR("invalid params!");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	if (_stp_controller_data->channels[channel].controller_connected)
		return STP_ERROR_ALREADY_OPEN;

	ret = stp_controller_get_attribute(STP_ATTRIB_SYNCED, &synced);
	if (ret != STP_SUCCESS)
		return ret;
	if (synced == 0)
		return STP_ERROR_NOT_SYNCED;

	stp_pl_init(&_stp_controller_data->channels[channel].rx_pl, rx_buffer,
		    rx_buffer_size);
	stp_pl_init(&_stp_controller_data->channels[channel].tx_pl, tx_buffer,
		    tx_buffer_size);

	_stp_controller_data->channels[channel].controller_connected = true;
	_stp_controller_data->channels[channel].valid_session = true;

	_stp_controller_data->channels[channel].priority = priority;

	stp_controller_set_notification(channel, STP_IN_CONNECTED);

	while (!_stp_controller_data->channels[channel].device_connected) {
		ret = _stp_controller_data->wait_signal->wait_open(channel);
		if (ret < 0)
			return STP_ERROR_IO_INTERRUPT;
	}

	return STP_SUCCESS;
}

int32_t stp_controller_close(uint8_t channel)
{
	int32_t ret = STP_SUCCESS;

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	if (!_stp_controller_data->channels[channel].controller_connected)
		return STP_ERROR_ALREADY_CLOSED;

	stp_controller_disconnect(channel);

	stp_pl_deinit(&_stp_controller_data->channels[channel].rx_pl);
	stp_pl_deinit(&_stp_controller_data->channels[channel].tx_pl);

	return ret;
}

int32_t stp_controller_fsync(uint8_t channel)
{
	int32_t ret = STP_SUCCESS;

	if (!stp_controller_is_channel_valid(channel))
		return STP_ERROR_INVALID_PARAMETERS;

	ret = stp_controller_check_for_rw_errors(channel);
	if (ret != STP_SUCCESS)
		return ret;

	ret = _stp_controller_data->wait_signal->wait_fsync(channel);
	if (ret < 0)
		ret = STP_ERROR_IO_INTERRUPT;

	return ret;
}
