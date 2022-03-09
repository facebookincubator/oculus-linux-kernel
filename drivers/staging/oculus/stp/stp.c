/*
 * SPI STP core code
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
#include "stp_pipeline.h"
#include "stp_master_common.h"
#include "stp_debug.h"

// singleton object containing all information
static struct stp_type _stp_data_object;
// pointer to singleton object
struct stp_type *_stp_data = &_stp_data_object;
// object containing the stats of transactions
struct stp_statistics _stp_stats;

struct stp_debug_type _stp_debug;

/* Calculate checksum  */
int calculate_checksum(U8 *buffer, unsigned int buffer_size)
{
	int checksum = 0;
	unsigned int i;

	STP_ASSERT(buffer && buffer_size,
		"Invalid parameter(s)");

	for (i = 0; i < buffer_size; ++i)
		checksum += buffer[i];

	checksum = 0 - checksum;

	return checksum;
}

/* Initialize the STP master/slave internal data */
void stp_init(struct stp_transport_table *transport,
	struct stp_pipelines_config *pipelines)
{
	STP_ASSERT(transport && pipelines, "Invalid parameter(s)");
	STP_ASSERT(pipelines->tx_pipeline &&
		pipelines->rx_pipeline, "Invalid parameter(s)");

	memset(_stp_data, 0, sizeof(*_stp_data));

	_stp_data->transport = transport;

	_stp_data->state = STP_STATE_INIT;

	stp_pl_init(&_stp_data->tx_pl, pipelines->tx_pipeline->buffer,
		pipelines->tx_pipeline->buffer_size);
	stp_pl_init(&_stp_data->rx_pl, pipelines->rx_pipeline->buffer,
		pipelines->rx_pipeline->buffer_size);

	_stp_data->pending.rx.acked = STP_ACK_NO_INFO;

	STP_LOCK_INIT(_stp_data->lock_notification);

	memset(&_stp_stats, 0, sizeof(_stp_stats));
	memset(&_stp_debug, 0, sizeof(_stp_debug));
}

//	Prepare TX packet
// This is relative to the rel_head not the current pipeline head
// because there might be a pending transaction (waiting to be acked)
void prepare_tx_packet_data_rel(U8 *buffer, struct stp_pending_tx *tx,
	UINT rel_head)
{
	struct stp_data_header_type *header;
	unsigned int total_size;
	U8 *p_data;

	STP_ASSERT(buffer && tx, "Invalid parameter(s)");

	tx->total_data = ACTUAL_DATA_SIZE;

	STP_LOCK(_stp_data->tx_pl.lock);

	stp_pl_get_data_size_rel(&_stp_data->tx_pl, rel_head, &total_size);

	if (total_size < tx->total_data)
		tx->total_data = total_size;

	stp_pl_get_data_no_update_rel(&_stp_data->tx_pl, rel_head,
		buffer + HEADER_DATA_SIZE, tx->total_data,
		&(tx->pipeline_head));

	header = (struct stp_data_header_type *)buffer;
	header->magic_number = STP_MAGIC_NUMBER_DATA;
	header->len_data = tx->total_data;
	header->prev_ack = _stp_data->pending.rx.acked;

	p_data = buffer + HEADER_DATA_SIZE;

	header->crc = calculate_checksum(p_data, tx->total_data);

	tx->sent = true;

	STP_UNLOCK(_stp_data->tx_pl.lock);
}

/* Process RX packet */
static void process_rx_packet(ACK_TYPE *acked)
{
	struct stp_data_header_type *header;
	unsigned int pl_size_av;
	int crc;
	U8 *p_data;
	U8 *buffer = _stp_data->rx_buffer;

	STP_ASSERT(acked, "Invalid parameter(s)");

	*acked = STP_ACK_ACCEPTED;

	STP_LOCK(_stp_data->rx_pl.lock);

	header = (struct stp_data_header_type *)buffer;

	if (header->len_data > ACTUAL_DATA_SIZE) {
		stp_ie_record(STP_IE_RX_BAD_LEN, 0);
		*acked = STP_ACK_BAD_SIZE;
		goto error;
	}

	p_data = buffer + HEADER_DATA_SIZE;

	crc = calculate_checksum(p_data, header->len_data);
	if (crc != header->crc) {
		stp_ie_record(STP_IE_RX_BAD_CRC, 0);
		*acked = STP_ACK_BAD_CRC;

		goto error;
	}

	stp_pl_get_available_space(&_stp_data->rx_pl, &pl_size_av);

	if (pl_size_av < header->len_data) {
		stp_ie_record(STP_IE_RX_PIPELINE_FULL, 0);
		*acked = STP_ACK_NO_SPACE;
		goto error;
	}

	stp_pl_add_data(&_stp_data->rx_pl, p_data,
		header->len_data);
	stp_ie_record(STP_IE_ADD_DATA_RX_PIPELINE, header->len_data);

	_stp_data->wait_signal->signal_read();

error:
	STP_UNLOCK(_stp_data->rx_pl.lock);
}

void stp_init_transaction(void)
{
	unsigned int magic_rec;
	struct stp_control_header_type *ack_rec =
		(struct stp_control_header_type *)_stp_data->rx_buffer;
	struct stp_control_header_type *ack_send =
		(struct stp_control_header_type *)_stp_data->tx_buffer;

	ack_send->magic_number = STP_MAGIC_NUMBER_INIT;
	ack_send->status = STP_ACK_NO_INFO;

	_stp_data->transport->send_receive_data(_stp_data->tx_buffer,
				_stp_data->rx_buffer, TOTAL_DATA_SIZE);

	magic_rec = ack_rec->magic_number;
	if (magic_rec == STP_MAGIC_NUMBER_INIT) {
		_stp_data->state = STP_STATE_DATA;
		STP_LOG_INFO("STP init: init done!\n");
		stp_invalidate_session();
	} else {
		stp_ie_record(STP_IE_MISMATCH_INIT, 0);
	}
}

void process_data_transaction(struct stp_pending_tx *tx)
{
	DATA_HEADER_TYPE *header;
	unsigned int magic_num;
	unsigned int acked = STP_ACK_NO_INFO;

	header = (DATA_HEADER_TYPE *)_stp_data->rx_buffer;
	magic_num = header->magic_number;

	if (magic_num == STP_MAGIC_NUMBER_EMPTY) {
		acked = STP_ACK_EMPTY_MAGIC;
		_stp_data->state = STP_STATE_DATA;
	} else if (magic_num == STP_MAGIC_NUMBER_DATA) {
		// If there is a pending RX error, we need to drop this packet
		if ((_stp_data->pending.rx.acked == STP_ACK_NO_INFO) ||
			(_stp_data->pending.rx.acked == STP_ACK_ACCEPTED) ||
			(_stp_data->pending.rx.acked == STP_ACK_EMPTY_MAGIC))
			process_rx_packet(&acked);
		else {
			acked = STP_ACK_NO_INFO;
		}
		_stp_data->state = STP_STATE_DATA;
	} else if (magic_num == STP_MAGIC_NUMBER_NOTIFICATION) {
		process_rx_notification(&acked);
		_stp_data->state = STP_STATE_DATA;
	} else {
		stp_ie_record(STP_IE_RX_BAD_MAGIC_NUMBER, 0);
		acked = STP_ACK_BAD_MAGIC;
		_stp_data->state = STP_STATE_INIT;
	}

	if ((magic_num == STP_MAGIC_NUMBER_EMPTY) ||
		(magic_num == STP_MAGIC_NUMBER_DATA) ||
		(magic_num == STP_MAGIC_NUMBER_NOTIFICATION)) {

		if (_stp_data->pending.tx.sent) {
			if (header->prev_ack == STP_ACK_ACCEPTED) {
				STP_LOCK(_stp_data->tx_pl.lock);
				stp_pl_update_head(&_stp_data->tx_pl,
					_stp_data->pending.tx.pipeline_head);
				STP_UNLOCK(_stp_data->tx_pl.lock);

				_stp_data->wait_signal->signal_write();
			} else {
				tx->sent = false;
			}
		}
	}

	_stp_data->pending.tx = *tx;
	_stp_data->pending.rx.acked = acked;
}

bool stp_has_data_to_send(void)
{
	bool is_empty = true;

	if (_stp_data->pending.tx_notification != STP_IN_NONE)
		return true;

	if (!_stp_data->pending.tx.sent) {
		/* this is the case where there is no pending transaction */
		stp_pl_is_empty(&_stp_data->tx_pl, &is_empty);
	} else {
		/* this is the case where there is a transaction sent */
		stp_pl_is_empty_rel(&_stp_data->tx_pl,
			_stp_data->pending.tx.pipeline_head, &is_empty);
	}

	return !is_empty;
}
