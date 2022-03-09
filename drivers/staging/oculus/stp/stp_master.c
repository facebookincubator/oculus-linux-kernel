/*
 * SPI STP Master code
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
#include "linux/rt600-ctrl.h"

#define STP_MCU_READY_TIMEOUT_MS 1000
#define STP_MCU_READY_TOTAL_TIMEOUT_MS 60000

unsigned int stp_mcu_ready_timer_expired_irq_missed_counter;

int stp_rt600_notify(struct notifier_block *nb,
				unsigned long event, void *v)
{
	switch (event) {
	case normal:
		_stp_data->flashing_mode = false;
		break;

	case flashing:
		_stp_data->flashing_mode = true;
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block stp_rt600_notifier_nb = {
	.notifier_call = stp_rt600_notify,
	.priority = 0,
};

static void prepare_tx_data_transaction(bool *do_transaction,
	struct stp_pending_tx *tx)
{
	bool is_empty;

	STP_ASSERT(do_transaction, "Invalid parameter(s)");

	/* We don't set to true by default of there is a pending transaction */
	*do_transaction = false;

	if (!_stp_data->pending.tx.sent) {
		/* this is the case where there is no pending transaction */
		stp_pl_is_empty(&_stp_data->tx_pl, &is_empty);
	} else {
		/* this is the case where there is a transaction sent */
		stp_pl_is_empty_rel(&_stp_data->tx_pl,
			_stp_data->pending.tx.pipeline_head, &is_empty);
	}

	if (!is_empty) {
		if (!_stp_data->pending.tx.sent) {
		/* this is the case where there is no pending transaction */
			prepare_tx_packet_data_rel(
				_stp_data->tx_buffer, tx,
				_stp_data->tx_pl.head);
		} else {
			/*
			 * this is the case where there is a transactin sent
			 * in this case, we take data from pipeline
			 * based on pending head
			 */
			prepare_tx_packet_data_rel(
				_stp_data->tx_buffer, tx,
				_stp_data->pending.tx.pipeline_head);
		}

		*do_transaction = true;
	} else {
		struct stp_data_header_type *header =
			(struct stp_data_header_type *)_stp_data->tx_buffer;

		header->magic_number = STP_MAGIC_NUMBER_EMPTY;
		header->prev_ack = _stp_data->pending.rx.acked;

		/*
		 * if we don't have data, we still should start a transaction
		 * if there is a pending one (to get the ack for it)
		 */
		*do_transaction = _stp_data->pending.tx.sent;
	}
}

/* Initialize the STP master internal data */
int
stp_init_master(struct stp_init_t *init)
{
	int ret = STP_SUCCESS;

	if (!init || !init->transport ||
		!init->handshake || !init->pipelines ||
		!init->wait_signal) {
		STP_LOG_ERROR("STP master: invalid params\n");
		ret = STP_ERROR;
		goto error;
	}

	stp_init(init->transport, init->pipelines);
	_stp_data->handshake = init->handshake;
	_stp_data->wait_signal = init->wait_signal;

	_stp_data->slave_ready = true;

	atomic_set(&_stp_data->data_av, 0);

	init_waitqueue_head(&_stp_data->trans_q);

	init_waitqueue_head(&_stp_data->slave_ready_q);

	stp_mcu_ready_timer_expired_irq_missed_counter = 0;

	rt600_event_register(&stp_rt600_notifier_nb);

	_stp_data->wait_for_data = false;

	_stp_data->flashing_mode = false;

	_stp_data->last_tx_notification = STP_IN_NONE;

error:
	return ret;
}

int
stp_deinit_master(void)
{
	int ret = STP_SUCCESS;

	rt600_event_unregister(&stp_rt600_notifier_nb);

	return ret;
}

/* Master init transaction */
static void stp_master_init_transaction(void)
{
	stp_init_transaction();
}

/* Master data transaction */
static bool stp_master_data_transaction(void)
{
	unsigned int ret;
	bool do_transaction = false;
	struct stp_pending_tx tx = {0};

	prepare_tx_notification(&do_transaction);
	if (!do_transaction)
		prepare_tx_data_transaction(&do_transaction, &tx);

	if (!do_transaction) {
		if (_stp_data->handshake->slave_has_data()) {
			unsigned int av_space;

			STP_LOCK(_stp_data->rx_pl.lock);
			stp_pl_get_available_space(
				&_stp_data->rx_pl, &av_space);
			STP_UNLOCK(_stp_data->rx_pl.lock);

			if (av_space >= ACTUAL_DATA_SIZE)
				do_transaction = true;
			else {
				static bool log;

				if (!log) {
					STP_LOG_ERROR("STP Full pipeline!\n");
					log = true;
				}
			}
		}
	}

	if (do_transaction) {
		ret = _stp_data->transport->send_receive_data(
					_stp_data->tx_buffer,
					_stp_data->rx_buffer,
					TOTAL_DATA_SIZE);
		STP_ASSERT(!ret, "STP - Transport error");

		process_data_transaction(&tx);
	}

	return do_transaction;
}

bool stp_get_wait_for_data(void)
{
	return _stp_data->wait_for_data;
}

void stp_master_wait_for_data(void)
{
	stp_ie_record(STP_IE_ENTER_WAIT_DATA, 0);

	_stp_data->wait_for_data = true;

	wait_event_interruptible(_stp_data->trans_q,
		(atomic_read(&_stp_data->data_av) != 0));

	atomic_set(&_stp_data->data_av, 0);
	stp_ie_record(STP_IE_RESET_SIGNAL_DATA, 0);

	_stp_data->wait_for_data = false;

	/*
	 * We need this one here because we already reset it to false
	 * without actually starting a transaction
	 */
	_stp_data->slave_ready = true;

	stp_ie_record(STP_IE_EXIT_WAIT_DATA, 0);
}

void stp_master_signal_data(void)
{
	atomic_set(&_stp_data->data_av, 1);
	stp_ie_record(STP_IE_SET_SIGNAL_DATA, 0);

	wake_up_interruptible(&_stp_data->trans_q);
}

void stp_master_signal_slave_ready(void)
{
	_stp_data->slave_ready = true;
	stp_ie_record(STP_IE_SET_SIGNAL_SLAVE_READY, 0);

	wake_up_interruptible(&_stp_data->slave_ready_q);
}

void stp_master_wait_for_slave_ready(void)
{
	int ret = 0;
	unsigned int crt_wait_time_mcu_ready_ms = 0;

	stp_ie_record(STP_IE_ENTER_WAIT_SLAVE, 0);

	while (1) {
		ret = wait_event_interruptible_timeout(_stp_data->slave_ready_q,
			_stp_data->slave_ready,
			msecs_to_jiffies(STP_MCU_READY_TIMEOUT_MS));

		if (ret > 0) {
			// In this case slave_ready = true
			break;
		}

		// if STP still in INIT state, we keep waiting
		// This is to avoid any corner cases whihc might not require STP/MCU to be up
		if (_stp_data->state == STP_STATE_INIT)
		{
			continue;
		}

		if (is_mcu_ready_to_receive_data()) {
			STP_LOG_ERROR("MCU ready timer expired: IRQ missed!");
			stp_mcu_ready_timer_expired_irq_missed_counter++;
			break;
		}

		crt_wait_time_mcu_ready_ms += STP_MCU_READY_TIMEOUT_MS;

		if (crt_wait_time_mcu_ready_ms >=
				STP_MCU_READY_TOTAL_TIMEOUT_MS) {
			if (!_stp_data->flashing_mode) {
				/* TODO: Eventually, we can decide if we want
				 * to do something else here
				 */
				STP_LOG_ERROR("MCU still not ready!");
			}
			crt_wait_time_mcu_ready_ms = 0;
		}
	}

	_stp_data->slave_ready = false;
	stp_ie_record(STP_IE_RESET_SIGNAL_SLAVE_READY, 0);

	stp_ie_record(STP_IE_EXIT_WAIT_SLAVE, 0);

	/* If last notification was SUSPENDED
	 * We need to signal suspend callback to resume
	 * There is no data to be sent, since we check that in SUSPEND callback
	 */
	if (_stp_data->last_tx_notification == STP_SOC_SUSPENDED)
		spi_stp_signal_suspend();
}

/* Master main transaction entry. Should be called from a separate thread */
int stp_master_transaction_thread(void)
{
	int ret = STP_SUCCESS;

	STP_ASSERT(_stp_data && _stp_data->handshake,
		"Invalid internal data");

	stp_master_wait_for_slave_ready();

	switch (_stp_data->state) {
	case STP_STATE_INIT:
		stp_master_init_transaction();
		stp_ie_record(STP_IE_EXIT_INIT_STATE, 0);
		break;
	case STP_STATE_DATA:
		if (!stp_master_data_transaction())
			stp_master_wait_for_data();
		break;
	default:
		STP_LOG_ERROR("STP master error: unknown state: %d",
			_stp_data->state);
		ret = STP_ERROR;
		break;
	}

	return ret;
}

void stp_disconnect(void)
{
	_stp_data->master_connected = false;

	stp_pl_reset(&_stp_data->tx_pl);
	stp_pl_reset(&_stp_data->rx_pl);

	/* wake up read/write to return error */
	_stp_data->wait_signal->signal_read();
	_stp_data->wait_signal->signal_write();

	stp_set_notification(STP_IN_DISCONNECTED);
	stp_ie_record(STP_IE_MASTER_DISCONNECTED, 0);
}

void stp_invalidate_session(void)
{
	STP_LOG_ERROR("STP stp_invalidate_session!");

	_stp_data->valid_session = false;
	_stp_data->slave_connected = false;
	_stp_data->master_connected = false;

	stp_pl_reset(&_stp_data->tx_pl);
	stp_pl_reset(&_stp_data->rx_pl);

	/* wake up read/write to return error */
	_stp_data->wait_signal->signal_read();
	_stp_data->wait_signal->signal_write();

	if (_stp_data->callback_client)
		_stp_data->callback_client(STP_EVENT_INIT);
}
