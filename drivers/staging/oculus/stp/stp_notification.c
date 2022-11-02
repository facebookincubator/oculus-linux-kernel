// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include "stp_master.h"
#include "stp_pipeline.h"
#include "stp_master_common.h"

/* we support only simple/empty notifiers for now */
#define STP_NOTIFICATION_SIZE sizeof(unsigned int)

void stp_rx_notification_connected(void)
{
	stp_pl_reset(&_stp_data->tx_pl);
	stp_pl_reset(&_stp_data->rx_pl);

	_stp_data->slave_connected = true;

	stp_ie_record(STP_IE_SLAVE_CONNECTED, 0);
}

void stp_rx_notification_disconnected(void)
{
	_stp_data->slave_connected = false;

	stp_pl_reset(&_stp_data->tx_pl);
	stp_pl_reset(&_stp_data->rx_pl);

	// wake up read/write to return error
	_stp_data->wait_signal->signal_read();
	_stp_data->wait_signal->signal_write();

	stp_ie_record(STP_IE_SLAVE_DISCONNECTED, 0);
}

/* Process RX notifications */
void process_rx_notification(ACK_TYPE *acked)
{
	DATA_HEADER_TYPE *header;
	unsigned int notification;
	int crc;
	U8 *p_data;
	U8 *buffer = _stp_data->rx_buffer;

	STP_ASSERT(acked,
		"Invalid parameter(s)");

	*acked = STP_ACK_ACCEPTED;

	header = (struct stp_data_header_type *)buffer;

	if (header->len_data != STP_NOTIFICATION_SIZE) {
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

	notification = *(unsigned int *)p_data;

	switch (notification) {
	case STP_IN_CONNECTED:
		stp_rx_notification_connected();
		break;
	case STP_IN_DISCONNECTED:
		stp_rx_notification_disconnected();
		break;
	default:
		STP_LOG_ERROR("Unknown notification 0x%08x\n", notification);
		break;
	}

error:
	return;
}

void stp_set_notification(unsigned int notification)
{
	STP_LOCK(_stp_data->lock_notification);

	if (_stp_data->pending.tx_notification != STP_IN_NONE)
		STP_LOG_ERROR("STP Overwrite notif 0x%08x with 0x%08x\n",
			_stp_data->pending.tx_notification, notification);

	_stp_data->pending.tx_notification = notification;

	STP_UNLOCK(_stp_data->lock_notification);

	stp_master_signal_data();
}

// TODO: T70170619
// Ack for notifications
void prepare_tx_notification_packet(U8 *buffer)
{
	struct stp_data_header_type *header;
	unsigned int *p_notification;

	STP_ASSERT(buffer, "Invalid parameter(s)");

	p_notification = (unsigned int *)(buffer + HEADER_DATA_SIZE);
	*p_notification = _stp_data->pending.tx_notification;

	header = (struct stp_data_header_type *)buffer;
	header->magic_number = STP_MAGIC_NUMBER_NOTIFICATION;
	header->len_data = STP_NOTIFICATION_SIZE;
	header->prev_ack = _stp_data->pending.rx.acked;

	header->crc = calculate_checksum(buffer + HEADER_DATA_SIZE,
		header->len_data);
}

void prepare_tx_notification(bool *do_transaction)
{
	STP_ASSERT(do_transaction, "Invalid parameter(s)");

	STP_LOCK(_stp_data->lock_notification);

	*do_transaction = false;

	_stp_data->last_tx_notification = STP_IN_NONE;

	if (_stp_data->pending.tx_notification != STP_IN_NONE) {
		prepare_tx_notification_packet(_stp_data->tx_buffer);

		*do_transaction = true;

		_stp_data->last_tx_notification =
			_stp_data->pending.tx_notification;

		_stp_data->pending.tx_notification = STP_IN_NONE;
	}

	STP_UNLOCK(_stp_data->lock_notification);
}
