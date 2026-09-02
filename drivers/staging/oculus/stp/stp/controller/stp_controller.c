/*
 * SPI STP Controller code
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
#include <stp/common/stp_common_public.h>
#include <stp/common/stp_logging.h>
#include <stp/controller/stp_controller.h>
#include <stp/controller/stp_controller_common.h>

#define STP_MCU_READY_TIMEOUT_MS 1000
#define STP_MCU_READY_TOTAL_TIMEOUT_MS 60000

/** If it's been more than this long since the controller_has_data was active, pulse it. */
#define STP_CLOCK_PULSE_GPIO_THRESHOLD_NS 10000000ULL

// singleton object containing all information
static struct stp_type _stp_data_object;
// pointer to singleton object
struct stp_type *_stp_controller_data = &_stp_data_object;

void stp_controller_set_controller_has_data(bool value);
static void stp_controller_timestamp_tx_transaction(void);
bool stp_controller_get_controller_has_data(void);

void stp_controller_prepare_tx_data_transaction(bool *do_transaction,
						struct stp_pending_tx *tx,
						bool *stamp_clock)
{
	STP_ASSERT(do_transaction, "Invalid parameter(s)");

	/* We don't set to true by default of there is a pending transaction */
	*do_transaction = false;

	uint8_t channel = stp_get_channel_with_data(
		_stp_controller_data->channels,
		_stp_controller_data->device_channels_status,
		&_stp_controller_data->pending);

	if (channel != STP_TOTAL_NUM_CHANNELS) {
		stp_prepare_tx_packet_data(
			channel, &_stp_controller_data->channels[channel].tx_pl,
			_stp_controller_data->tx_buffer,
			_stp_controller_data->channels[channel].log_tx_data);

		if (stamp_clock != NULL) {
			*stamp_clock =
				(channel == _stp_controller_data->time_channel);
		}

		tx->sent = true;
		tx->channel = channel;

		_stp_controller_data->pending.tx.channel = channel;

		_stp_controller_data->wait_signal->signal_write(channel);

		*do_transaction = true;

		uint32_t data_size;

		stp_pl_get_data_size(
			&_stp_controller_data->channels[channel].tx_pl,
			&data_size);
		if (data_size == 0) {
			_stp_controller_data->wait_signal->signal_fsync(
				channel);
		}
	} else {
		struct stp_data_header_type *header =
			(struct stp_data_header_type *)
				_stp_controller_data->tx_buffer;

		header->channel_opcode = stp_set_opcode_value(
			header->channel_opcode, STP_OPCODE_EMPTY);
		header->len_data = STP_EMPTY_SIZE;

		/*
		 * if we don't have data, we still should start a transaction
		 * if there is a pending one (to get the ack for it)
		 */
		*do_transaction = _stp_controller_data->pending.tx.sent;
	}
}

/* Initialize the STP controller internal data */
int32_t stp_controller_init(struct stp_controller_init_t *init)
{
	int32_t ret = STP_SUCCESS;

	if (!init || !init->transport || !init->handshake ||
	    !init->wait_signal) {
		STP_LOG_ERROR("STP controller: invalid init parameter");
		ret = STP_ERROR;
		goto error;
	}

	if (!init->transport->send_receive_data) {
		STP_LOG_ERROR("STP controller: invalid transport parameter");
		ret = STP_ERROR;
		goto error;
	}

	if (!init->handshake->device_request_transaction ||
	    !init->handshake->set_controller_has_data) {
		STP_LOG_ERROR("STP controller: invalid handshake parameter");
		ret = STP_ERROR;
		goto error;
	}

	if (!init->wait_signal->wait_write ||
	    !init->wait_signal->signal_write || !init->wait_signal->wait_read ||
	    !init->wait_signal->signal_read || !init->wait_signal->wait_fsync ||
	    !init->wait_signal->signal_fsync ||
	    !init->wait_signal->reset_fsync || !init->wait_signal->wait_open ||
	    !init->wait_signal->signal_open ||
	    !init->wait_signal->signal_stp_event ||
	    !init->wait_signal->wait_stp_event ||
	    !init->wait_signal->pause_thread ||
	    !init->wait_signal->resume_thread) {
		STP_LOG_ERROR("STP controller: invalid wait/signal parameter");
		ret = STP_ERROR;
		goto error;
	}

	stp_controller_init_internal(init->transport);
	_stp_controller_data->rx_buffer = init->rx_buffer;
	_stp_controller_data->tx_buffer = init->tx_buffer;
	_stp_controller_data->handshake = init->handshake;
	_stp_controller_data->wait_signal = init->wait_signal;

	_stp_controller_data->last_tx_notification = STP_IN_NONE;

	_stp_controller_data->time_channel = init->time_channel;

error:
	return ret;
}

int32_t stp_controller_deinit(void)
{
	STP_LOCK_DEINIT(_stp_controller_data->lock_notification);
	STP_LOCK_DEINIT(_stp_controller_data->lock_event_processing);
	STP_LOCK_DEINIT(_stp_controller_data->lock_set_has_data);

	stp_controller_deinit_internal();

	return STP_SUCCESS;
}

// Do all the time comsuming checks here before making a transaction to avoid empty
// transactions caused by a late signal udpate
static void stp_controller_update_has_data_pre_process(void)
{
	struct stp_data_header_type *header =
		(struct stp_data_header_type *)_stp_controller_data->tx_buffer;
	header->channels_status =
		stp_get_channels_status(_stp_controller_data->channels);
	_stp_controller_data->prev_channels_status = header->channels_status;

	bool has_buffered_data = false;

	STP_LOCK(_stp_controller_data->lock_set_has_data);

	if (stp_get_channel_with_notification(_stp_controller_data->channels) !=
	    -1)
		has_buffered_data = true;

	else if (stp_get_channel_with_data(
			 _stp_controller_data->channels,
			 _stp_controller_data->device_channels_status,
			 &_stp_controller_data->pending) !=
		 STP_TOTAL_NUM_CHANNELS)
		has_buffered_data = true;

	stp_controller_set_controller_has_data(has_buffered_data);

	STP_UNLOCK(_stp_controller_data->lock_set_has_data);
}

// After processing the data, check if we need to transfer based on the
// information received from the packet. The chance of setting controller
// has data to true is relatively small compared to pre_process, as this
// is dealing with protocol out of sync, device channel status change, and
// controller channels status change.
static void stp_controller_update_has_data_post_process(void)
{
	bool has_buffered_data = false;

	STP_LOCK(_stp_controller_data->lock_set_has_data);

	if (_stp_controller_data->state == STP_STATE_INIT)
		has_buffered_data = true;

	else if (_stp_controller_data->prev_device_channels_status !=
		 _stp_controller_data->device_channels_status) {
		if (stp_get_channel_with_data(
			    _stp_controller_data->channels,
			    _stp_controller_data->device_channels_status,
			    &_stp_controller_data->pending) !=
		    STP_TOTAL_NUM_CHANNELS)
			has_buffered_data = true;

	} else {
		uint32_t channels_status =
			stp_get_channels_status(_stp_controller_data->channels);
		if (channels_status !=
		    _stp_controller_data->prev_channels_status) {
			has_buffered_data = true;
		}
	}

	if (has_buffered_data) {
		stp_controller_set_controller_has_data(true);
	}

	STP_UNLOCK(_stp_controller_data->lock_set_has_data);
}

/* controller data transaction */
static bool stp_controller_data_transaction(void)
{
	uint32_t ret;
	bool do_transaction = false;
	struct stp_pending_tx tx = { 0 };

	// Setting to all 0xFF is likely to break tests if certain fields are not set properly.
	// Uncomment and run tests if you suspect there's a bug in how the buffer is prepared.
#ifdef STP_INITIALIZE_BUFFERS
	memset(_stp_controller_data->tx_buffer, 0xFF, STP_TOTAL_DATA_SIZE);
#endif

	bool stamp_clock = false;
	stp_controller_prepare_tx_notification(&do_transaction);
	if (!do_transaction) {
		stp_controller_prepare_tx_data_transaction(&do_transaction, &tx,
							   &stamp_clock);
	}

	if (!do_transaction) {
		if (_stp_controller_data->handshake
			    ->device_request_transaction()) {
			do_transaction = true;
		}
	}

	if (!do_transaction) {
		uint32_t channels_status =
			stp_get_channels_status(_stp_controller_data->channels);
		if (channels_status !=
		    _stp_controller_data->prev_channels_status)
			do_transaction = true;
	}

	if (do_transaction) {
		stp_controller_update_has_data_pre_process();

		if (stamp_clock) {
			stp_controller_timestamp_tx_transaction();
		}

		stp_controller_prepare_common_tx_transaction();

		ret = _stp_controller_data->transport->send_receive_data(
			_stp_controller_data->tx_buffer,
			_stp_controller_data->rx_buffer, STP_TOTAL_DATA_SIZE);
		STP_ASSERT(!ret, "STP - Transport error");
		_stp_controller_data->transaction_count++;

		stp_controller_process_data_transaction(&tx);

		stp_controller_update_has_data_post_process();
	}

	return do_transaction;
}

void stp_controller_set_controller_has_data(bool value)
{
	_stp_controller_data->handshake->set_controller_has_data(value);
}

/**  Return whether the GPIO is active. */
bool stp_controller_get_controller_has_data(void)
{
	return _stp_controller_data->handshake->get_controller_has_data();
}

/**  Return when the GPIO was last made active. */
uint64_t stp_controller_get_controller_last_has_data_ns(void)
{
	return _stp_controller_data->handshake
		->get_controller_last_has_data_ns();
}

/**  Returns a nanosecond counter using the same time stamp clock as stp_controller_get_controller_last_has_data_timestamp. */
uint64_t stp_controller_get_ns(void)
{
	return _stp_controller_data->handshake->stp_controller_get_ns();
}

/*
 * Records an INIT<->DATA flip. The caller owns the state write, so this also
 * serves the packet_error() path, which performs the transition itself. The
 * data path drives DATA on every round that carries a packet, so the early
 * return is what keeps the clock read off the hot path.
 */
static void stp_controller_note_state_flip(uint32_t old_state,
					   uint32_t new_state)
{
	if (old_state == new_state)
		return;

	_stp_controller_data->sync_previous_state = old_state;
	_stp_controller_data->sync_previous_ns =
		_stp_controller_data->sync_current_ns;
	_stp_controller_data->sync_current_state = new_state;
	_stp_controller_data->sync_current_ns = stp_controller_get_ns();
}

static void stp_controller_set_state(uint32_t new_state)
{
	stp_controller_note_state_flip(_stp_controller_data->state, new_state);
	_stp_controller_data->state = new_state;
}

static void stp_controller_packet_error(const char *ctx_str,
					enum packet_error_t error)
{
	uint32_t old_state = _stp_controller_data->state;

	packet_error(ctx_str, error, &_stp_controller_data->state);
	stp_controller_note_state_flip(old_state, STP_STATE_INIT);
}

void stp_controller_signal_start_transaction(void)
{
	_stp_controller_data->start_transaction = true;
	_stp_controller_data->wait_signal->signal_stp_event();
}

bool stp_controller_get_start_transaction(void)
{
	return _stp_controller_data->start_transaction;
}

void stp_controller_unset_start_transaction(void)
{
	_stp_controller_data->start_transaction = false;
}

void stp_controller_signal_stop_thread(void)
{
	_stp_controller_data->stop_thread = true;
	_stp_controller_data->wait_signal->signal_stp_event();
}

bool stp_controller_get_stop_thread(void)
{
	return _stp_controller_data->stop_thread;
}

void stp_controller_unset_stop_thread(void)
{
	_stp_controller_data->stop_thread = false;
}

void stp_controller_signal_suspend(void)
{
	_stp_controller_data->suspend = true;
	_stp_controller_data->wait_signal->signal_stp_event();
}

bool stp_controller_get_suspend(void)
{
	return _stp_controller_data->suspend;
}

void stp_controller_unset_suspend(void)
{
	_stp_controller_data->suspend = false;
}

bool stp_controller_pending_event(void)
{
	bool pending = false;

	pending = pending || stp_controller_get_start_transaction();
	pending = pending || stp_controller_get_stop_thread();
	pending = pending || stp_controller_get_suspend();

	return pending;
}

int32_t stp_controller_process_event(void)
{
	int32_t ret = STP_SUCCESS;

	if (stp_controller_get_stop_thread()) {
		ret = STP_SUCCESS;
	} else if (stp_controller_get_start_transaction()) {
		stp_controller_unset_start_transaction();
		if (_stp_controller_data->state == STP_STATE_INIT) {
			// If mcu sets device can receive to true but later set it to false due to
			// a reset, SoC's internal signaling will still proceed with a transaction
			// which is illegal. We need to check the device ready signal again
			// before making the transaction.
			if (_stp_controller_data->handshake
				    ->device_request_transaction())
				stp_controller_init_transaction();
		} else if (_stp_controller_data->state == STP_STATE_DATA) {
			if (_stp_controller_data->handshake
				    ->device_request_transaction())
				stp_controller_data_transaction();

		} else {
			STP_LOG_ERROR("STP controller unknown state: %zu",
				      (size_t)_stp_controller_data->state);
			ret = STP_ERROR;
		}
	} else {
		// Controller has data but device is not yet ready. We need to unset
		// data here to prevent a tight loop. Rely on device_ready flag to initiate
		// the next transaction
		STP_LOG_DEBUG("no event available!");
		ret = STP_SUCCESS;
	}

	return ret;
}

/* controller main transaction entry. Should be called from a separate thread */
int32_t stp_controller_transaction_thread(void)
{
	int32_t ret = STP_SUCCESS;

	if (!_stp_controller_data) {
		STP_LOG_ERROR("Invalid internal data");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	if (!_stp_controller_data->handshake) {
		STP_LOG_ERROR("Invalid handshake function");
		return STP_ERROR_INVALID_PARAMETERS;
	}

	ret = _stp_controller_data->wait_signal->wait_stp_event();
	if (ret)
		return STP_ERROR_IO_INTERRUPT;

	/* Handle suspend before acquiring lock_event_processing.
	 * pause_thread() parks the kthread and blocks until it is unparked.
	 * If we held the lock across that, any thread calling
	 * stp_controller_disconnect() would block on lock_event_processing
	 * for the entire duration the kthread stays parked (e.g. while
	 * SPI raw mode is active).
	 */
	if (stp_controller_get_suspend()) {
		stp_controller_unset_suspend();
		_stp_controller_data->wait_signal->pause_thread();
		return STP_SUCCESS;
	}

	STP_LOCK(_stp_controller_data->lock_event_processing);
	ret = stp_controller_process_event();
	STP_UNLOCK(_stp_controller_data->lock_event_processing);

	return ret;
}

void stp_controller_disconnect(uint8_t channel)
{
	/* Wait for the STP kthread to be idle before marking a channel
	 * as disconnected. The STP kthread is not ready for disconnects
	 * while processing events. When STP kthread selects a channel
	 * for data transmission, it will use the pipeline and the buffers
	 * while processing events. Userspace should not interfere by
	 * resetting the pipeline and kfree-ing the buffers.
	 *
	 * It is safe to mark as disconnected when STP is not processing
	 * events. When it eventually starts processing events, it will
	 * see that controller is disconnected, and will not use the pipeline.
	 *
	 * The following critical section has a single trivial assignment,
	 * so there is no risk of deadlocks.
	 */
	STP_LOCK(_stp_controller_data->lock_event_processing);
	_stp_controller_data->channels[channel].controller_connected = false;
	STP_UNLOCK(_stp_controller_data->lock_event_processing);

	/* From now on, even if the STP kthread is running and processing
	   events, it will be aware that controller is not connected.
	   So it will not touch the TX/RX pipelines. Hence we can safely
	   reset and kfree them.
	 */
	stp_pl_reset(&_stp_controller_data->channels[channel].tx_pl);
	stp_pl_reset(&_stp_controller_data->channels[channel].rx_pl);

	/* wake up read/write to return error */
	_stp_controller_data->wait_signal->signal_read(channel);
	_stp_controller_data->wait_signal->signal_write(channel);

	if (!_stp_controller_data->service_interruption)
		stp_controller_set_notification(channel, STP_IN_DISCONNECTED);
}

void stp_controller_invalidate_channel(uint8_t channel)
{
	_stp_controller_data->channels[channel].valid_session = false;

	if (_stp_controller_data->channels[channel].controller_connected) {
		stp_pl_reset(&_stp_controller_data->channels[channel].tx_pl);
		stp_pl_reset(&_stp_controller_data->channels[channel].rx_pl);
	}

	_stp_controller_data->channels[channel].device_connected = false;
	_stp_controller_data->channels[channel].controller_connected = false;

	/* wake up read/write to return error */
	_stp_controller_data->wait_signal->signal_read(channel);
	_stp_controller_data->wait_signal->signal_write(channel);
}

void stp_controller_invalidate_session(void)
{
	STP_LOG_ERROR_RATE_LIMIT(
		"STP protocol synced. Invalidate all channels");

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++)
		stp_controller_invalidate_channel(i);

	if (_stp_controller_data->callback_client)
		_stp_controller_data->callback_client(
			STP_CONTROLLER_EVENT_INIT);
}

bool stp_controller_is_channel_valid(uint8_t channel)
{
	return (channel < STP_TOTAL_NUM_CHANNELS);
}

/**
 * Write the time stamp of when the controller last set the SoC "has data" line.
 */
static void stp_controller_timestamp_tx_transaction()
{
	struct stp_data_header_type *header =
		(struct stp_data_header_type *)_stp_controller_data->tx_buffer;

	// The data comes immediately after the header.
	stp_clock_t *clock_buffer = (stp_clock_t *)(header + 1);

	// It's possible for data to queue up. If the clock channel is congested zero it out.
	if (header->len_data != sizeof(stp_clock_t)) {
		memset(clock_buffer, 0, sizeof(stp_clock_t));
		return;
	}

	// Serialize the has-data read/compare/pulse under lock_set_has_data, the
	// same lock every other stp_controller_set_controller_has_data() caller
	// holds. Without it this timestamp path can toggle the SoC "has data" GPIO
	// concurrently with a locked has_data update, double-arming the driver WDT.
	// See T244827821.
	STP_LOCK(_stp_controller_data->lock_set_has_data);

	uint64_t last_set_ns = stp_controller_get_controller_last_has_data_ns();
	const uint64_t delta_ns = stp_controller_get_ns() - last_set_ns;
	if (delta_ns > STP_CLOCK_PULSE_GPIO_THRESHOLD_NS) {
		// Since it's been so long since the last time the GPIO was set active,
		// pulse the GPIO. At this point the GPIO is set, and the other side
		// has already started its SPI Rx. Pulsing the GPIO can only release
		// an already released semaphore in stp_device_wait_for_transaction on
		// the the other side, which only resets that semaphore in
		// stp_device_wait_for_transaction when the transaction is complete, which
		// happens only after this function is called). The pulse lets the other
		// side get a local timestamp matching last_set_ns.
		stp_controller_set_controller_has_data(false);
		stp_controller_set_controller_has_data(true);
		last_set_ns = stp_controller_get_controller_last_has_data_ns();
	}

	STP_UNLOCK(_stp_controller_data->lock_set_has_data);

	// The target is expected to know the local endianness.
	memcpy(&clock_buffer->sender_signal_ns, &last_set_ns,
	       sizeof(clock_buffer->sender_signal_ns));
}

void stp_controller_prepare_common_tx_transaction()
{
	uint8_t *buffer = _stp_controller_data->tx_buffer;
	struct stp_data_header_type *header =
		(struct stp_data_header_type *)buffer;

	enum calculate_pkt_crc_result calculate_result =
		STP_CALCULATE_CRC_UNKNOWN;

	header->crc = stp_calculate_crc_for_transaction_packet(
		buffer, &calculate_result);

	if (calculate_result != STP_CALCULATE_CRC_SUCCESS)
		STP_LOG_ERROR("STP Controller: Failed to calculate CRC: %d",
			      calculate_result);
}

/* Initialize the STP controller/device internal data */
void stp_controller_init_internal(
	struct stp_controller_transport_table *transport)
{
	STP_LOG_INFO("stp_controller_init_internal");

	STP_ASSERT(transport, "Invalid parameter(s)");

	memset(_stp_controller_data, 0, sizeof(*_stp_controller_data));

	_stp_controller_data->transport = transport;

	_stp_controller_data->state = STP_STATE_INIT;

	_stp_controller_data->device_channels_status = 0xFFFFFFFF;
	_stp_controller_data->prev_device_channels_status = 0xFFFFFFFF;
	_stp_controller_data->prev_channels_status = 0xFFFFFFFF;

	_stp_controller_data->service_interruption = false;

	_stp_controller_data->start_transaction = false;
	_stp_controller_data->stop_thread = false;
	_stp_controller_data->suspend = false;

	STP_LOCK_INIT(_stp_controller_data->lock_notification);
	STP_LOCK_INIT(_stp_controller_data->lock_event_processing);
	STP_LOCK_INIT(_stp_controller_data->lock_set_has_data);

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		stp_pl_init_lock(&_stp_controller_data->channels[i].rx_pl);
		stp_pl_init_lock(&_stp_controller_data->channels[i].tx_pl);

		STP_LOCK_INIT(_stp_controller_data->channels[i].read_lock);
		STP_LOCK_INIT(_stp_controller_data->channels[i].write_lock);
	}
}

void stp_controller_deinit_internal(void)
{
	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		stp_pl_deinit_lock(&_stp_controller_data->channels[i].rx_pl);
		stp_pl_deinit_lock(&_stp_controller_data->channels[i].tx_pl);

		STP_LOCK_DEINIT(_stp_controller_data->channels[i].read_lock);
		STP_LOCK_DEINIT(_stp_controller_data->channels[i].write_lock);
	}
}

void stp_controller_init_transaction(void)
{
	struct stp_data_header_type *ack_rec =
		(struct stp_data_header_type *)_stp_controller_data->rx_buffer;
	struct stp_data_header_type *ack_send =
		(struct stp_data_header_type *)_stp_controller_data->tx_buffer;

	ack_send->channel_opcode =
		stp_set_opcode_value(ack_send->channel_opcode, STP_OPCODE_INIT);
	ack_send->len_data = STP_INIT_SIZE;

	_stp_controller_data->transport->send_receive_data(
		_stp_controller_data->tx_buffer,
		_stp_controller_data->rx_buffer, STP_TOTAL_DATA_SIZE);
	_stp_controller_data->transaction_count++;

	uint8_t opcode = stp_get_opcode_value(ack_rec->channel_opcode);
	bool crc_ok = stp_check_crc(_stp_controller_data->rx_buffer);

	_stp_controller_data->last_rx_opcode = opcode;
	_stp_controller_data->last_rx_crc_ok = crc_ok;

	if (opcode == STP_OPCODE_INIT) {
		stp_controller_invalidate_session();
		stp_controller_set_state(STP_STATE_DATA);
		STP_LOG_INFO_RATE_LIMIT("STP Controller: init: init done!");
	} else {
		/*
		 * MCU did not echo INIT this round. A single occurrence is a
		 * normal part of STP operation; a persistent one means the SoC
		 * stays in INIT while the MCU may consider the link synced, so
		 * log the echo to keep that disagreement diagnosable (T278968653).
		 */
		STP_LOG_INFO_RATE_LIMIT(
			"STP Controller: sent INIT, got rx_opcode=%u crc_ok=%d rx_ch_status=0x%x doorbell=%d",
			opcode, crc_ok, ack_rec->channels_status,
			_stp_controller_data->handshake
				->device_request_transaction());
	}
}

#define STP_BAD_CRC_BACKOFF_DELAY_MS 10
#define STP_MAX_BAD_CRCS_IN_A_ROW_BEFORE_BACKOFF 3
void stp_controller_process_data_transaction(struct stp_pending_tx *tx)
{
	STP_DATA_HEADER_TYPE *header;

	header = (STP_DATA_HEADER_TYPE *)_stp_controller_data->rx_buffer;

	bool check_crc = stp_check_crc(_stp_controller_data->rx_buffer);

	_stp_controller_data->last_rx_crc_ok = check_crc;
	_stp_controller_data->last_rx_opcode =
		stp_get_opcode_value(header->channel_opcode);

	if (!check_crc) {
		stp_controller_packet_error("Controller",
					    STP_PACKET_ERROR_BAD_CRC);

		if (++_stp_controller_data->bad_crcs_in_a_row >
		    STP_MAX_BAD_CRCS_IN_A_ROW_BEFORE_BACKOFF) {
			STP_MSLEEP(STP_BAD_CRC_BACKOFF_DELAY_MS);
		}
		return;
	}

	// If we receive a good packet, just reset the bad_crc counter
	_stp_controller_data->bad_crcs_in_a_row = 0;

	uint8_t opcode = stp_get_opcode_value(header->channel_opcode);

	_stp_controller_data->prev_device_channels_status =
		_stp_controller_data->device_channels_status;
	_stp_controller_data->device_channels_status = header->channels_status;

	if (opcode == STP_OPCODE_EMPTY) {
		// Do nothing, stay in DATA state
	} else if (opcode == STP_OPCODE_DATA) {
		uint8_t channel = stp_get_channel_value(header->channel_opcode);

		if (channel >= STP_TOTAL_NUM_CHANNELS) {
			stp_controller_packet_error(
				"Controller",
				STP_PACKET_ERROR_INVALID_CHANNEL);
			return;
		} else if (!_stp_controller_data->channels[channel]
				    .controller_connected) {
			STP_LOG_ERROR(
				"STP Controller: Received data for an unconnected channel %zu",
				(size_t)channel);
			return;
		}

		if (!stp_process_rx_packet(
			    &_stp_controller_data->channels[channel],
			    _stp_controller_data->rx_buffer,
			    _stp_controller_data->channels[channel]
				    .log_rx_data)) {
			stp_controller_packet_error(
				"Controller",
				STP_PACKET_ERROR_PROCESS_RX_DATA);
			return;
		}

		_stp_controller_data->wait_signal->signal_read(channel);
		stp_controller_set_state(STP_STATE_DATA);
	} else if (opcode == STP_OPCODE_NOTIFICATION) {
		uint32_t notification;
		uint8_t channel;

		if (!stp_process_rx_notification(_stp_controller_data->rx_buffer,
						 &channel, &notification)) {
			stp_controller_packet_error(
				"Controller",
				STP_PACKET_ERROR_PROCESS_RX_NOTIFICATION);
			return;
		}

		stp_controller_rx_notification(channel, notification);
	} else {
		stp_controller_packet_error("Controller",
					    STP_PACKET_ERROR_UNKNOWN_OPCODE);
	}

	_stp_controller_data->pending.tx = *tx;
}

bool stp_controller_has_data_to_send(void)
{
	bool is_empty = true;

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		if (_stp_controller_data->channels[i].pending_tx_notification !=
		    STP_IN_NONE)
			return true;

		if (_stp_controller_data->channels[i].controller_connected) {
			stp_pl_is_empty(
				&_stp_controller_data->channels[i].tx_pl,
				&is_empty);

			if (!is_empty)
				break;
		}
	}

	return !is_empty;
}

void stp_controller_reset_all_channel_buffer(void)
{
	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		stp_pl_reset(&_stp_controller_data->channels[i].tx_pl);
		stp_pl_reset(&_stp_controller_data->channels[i].rx_pl);
	}
}

void stp_controller_request_protocol_resync(void)
{
	stp_controller_set_state(STP_STATE_INIT);
}

uint8_t stp_controller_get_packet_channel(uint8_t *buffer, unsigned int len)
{
	STP_DATA_HEADER_TYPE *header = (STP_DATA_HEADER_TYPE *)buffer;
	uint8_t channel = STP_TOTAL_NUM_CHANNELS;

	if ((len >= sizeof(*header)) && stp_check_crc(buffer)) {
		switch (stp_get_opcode_value(header->channel_opcode)) {
		case STP_OPCODE_EMPTY:
			// Do nothing, stay in DATA state that no error logging
			break;
		case STP_OPCODE_DATA:
		case STP_OPCODE_NOTIFICATION:
			channel = stp_get_channel_value(header->channel_opcode);
			break;
		default:
			STP_LOG_ERROR("Invalid packet: len=%u/%u, code=0x%X(opcode=%d channel=%d)\n",
					len, header->len_data, header->channel_opcode,
					(int)stp_get_opcode_value(header->channel_opcode),
					(int)stp_get_channel_value(header->channel_opcode)
			);
			break;
		}
	} else
		STP_LOG_ERROR("Invalid packet: len=%u, crc=%d\n", len, stp_check_crc(buffer));

	return channel;
}
