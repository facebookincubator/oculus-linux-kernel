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
#include <stp/common/stp_logging.h>
#include <stp/controller/stp_controller.h>
#include <stp/controller/stp_controller_common.h>

#define STP_MCU_READY_TIMEOUT_MS 1000
#define STP_MCU_READY_TOTAL_TIMEOUT_MS 60000

uint32_t stp_mcu_ready_timer_expired_irq_missed_counter;

// singleton object containing all information
static struct stp_type _stp_data_object;
// pointer to singleton object
struct stp_type *_stp_controller_data = &_stp_data_object;

static void stp_controller_prepare_tx_data_transaction(bool *do_transaction,
                                                       struct stp_pending_tx *tx)
{
    STP_ASSERT(do_transaction, "Invalid parameter(s)");

    /* We don't set to true by default of there is a pending transaction */
    *do_transaction = false;

    uint8_t channel = stp_get_channel_with_data(_stp_controller_data->channels,
                                                _stp_controller_data->device_channels_status,
                                                &_stp_controller_data->pending);

    if (channel != STP_TOTAL_NUM_CHANNELS)
    {
        stp_prepare_tx_packet_data(channel,
                                   &_stp_controller_data->channels[channel].tx_pl,
                                   _stp_controller_data->tx_buffer);

        tx->sent    = true;
        tx->channel = channel;

        _stp_controller_data->pending.tx.channel = channel;

        _stp_controller_data->wait_signal->signal_write(channel);

        *do_transaction = true;

        uint32_t data_size;
        stp_pl_get_data_size(&_stp_controller_data->channels[channel].tx_pl, &data_size);
        if (data_size == 0)
        {
            _stp_controller_data->wait_signal->signal_fsync(channel);
        }
    }
    else
    {
        struct stp_data_header_type *header =
            (struct stp_data_header_type *)_stp_controller_data->tx_buffer;

        header->channel_opcode = stp_set_opcode_value(header->channel_opcode, STP_OPCODE_EMPTY);
        header->len_data       = STP_EMPTY_SIZE;

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

    if (!init || !init->transport || !init->handshake || !init->wait_signal)
    {
        STP_LOG_ERROR("STP controller: invalid init parameter");
        ret = STP_ERROR;
        goto error;
    }

    if (!init->transport->send_receive_data)
    {
        STP_LOG_ERROR("STP controller: invalid transport parameter");
        ret = STP_ERROR;
        goto error;
    }

    if (!init->handshake->device_has_data || !init->handshake->device_can_receive)
    {
        STP_LOG_ERROR("STP controller: invalid handshake parameter");
        ret = STP_ERROR;
        goto error;
    }

    if (!init->wait_signal->wait_write || !init->wait_signal->signal_write ||
        !init->wait_signal->wait_read || !init->wait_signal->signal_read ||
        !init->wait_signal->wait_for_device_ready || !init->wait_signal->signal_device_ready ||
        !init->wait_signal->set_ready_for_transaction || !init->wait_signal->wait_for_data ||
        !init->wait_signal->signal_data || !init->wait_signal->wait_fsync ||
        !init->wait_signal->signal_fsync || !init->wait_signal->reset_fsync ||
        !init->wait_signal->wait_open || !init->wait_signal->signal_open)
    {
        STP_LOG_ERROR("STP controller: invalid wait/signal parameter");
        ret = STP_ERROR;
        goto error;
    }

    stp_controller_init_internal(init->transport);
    _stp_controller_data->rx_buffer = init->rx_buffer;
    _stp_controller_data->tx_buffer = init->tx_buffer;
    _stp_controller_data->handshake   = init->handshake;
    _stp_controller_data->wait_signal = init->wait_signal;

    _stp_controller_data->wait_signal->set_ready_for_transaction(true);

    stp_mcu_ready_timer_expired_irq_missed_counter = 0;

    _stp_controller_data->wait_for_data = false;

    _stp_controller_data->last_tx_notification = STP_IN_NONE;

error:
    return ret;
}

int32_t stp_controller_deinit(void)
{
    STP_LOCK_DEINIT(_stp_controller_data->lock_notification);

    return STP_SUCCESS;
}

/* controller data transaction */
static bool stp_controller_data_transaction(void)
{
    uint32_t ret;
    bool do_transaction      = false;
    struct stp_pending_tx tx = {0};

    // Setting to all 0xFF is likely to break tests if certain fields are not set properly.
    // Uncomment and run tests if you suspect there's a bug in how the buffer is prepared.
#ifdef STP_INITIALIZE_BUFFERS
    memset(_stp_controller_data->tx_buffer, 0xFF, STP_TOTAL_DATA_SIZE);
#endif

    stp_controller_prepare_tx_notification(&do_transaction);
    if (!do_transaction)
    {
        stp_controller_prepare_tx_data_transaction(&do_transaction, &tx);
    }

    if (!do_transaction)
    {
        if (_stp_controller_data->handshake->device_has_data())
        {
            do_transaction = true;
        }
    }

    if (!do_transaction)
    {
        uint32_t channels_status = stp_get_channels_status(_stp_controller_data->channels);
        if (channels_status != _stp_controller_data->prev_channels_status)
        {
            do_transaction = true;
        }
    }

    if (do_transaction)
    {
        _stp_controller_data->pending_device_ready_signal = false;

        stp_controller_prepare_common_tx_transaction();

        ret = _stp_controller_data->transport->send_receive_data(
            _stp_controller_data->tx_buffer, _stp_controller_data->rx_buffer, STP_TOTAL_DATA_SIZE);
        STP_ASSERT(!ret, "STP - Transport error");

        stp_controller_process_data_transaction(&tx);
    }

    return do_transaction;
}

bool stp_controller_get_wait_for_data(void)
{
    return _stp_controller_data->wait_for_data;
}

void stp_controller_wait_for_data(void)
{
    _stp_controller_data->wait_for_data = true;

    _stp_controller_data->wait_signal->wait_for_data();

    _stp_controller_data->wait_for_data = false;

    if (_stp_controller_data->pending_device_ready_signal)
    {
        stp_controller_signal_device_ready();
    }

    /*
     * We need this one here because we already reset it to false
     * without actually starting a transaction
     */
    _stp_controller_data->wait_signal->set_ready_for_transaction(true);
}

void stp_controller_signal_data(void)
{
    _stp_controller_data->wait_signal->signal_data();
}

void stp_controller_signal_device_ready(void)
{
    _stp_controller_data->wait_signal->set_ready_for_transaction(true);
    _stp_controller_data->wait_signal->signal_device_ready();
}

int32_t stp_controller_wait_for_device_ready(void)
{
    int32_t ret                         = 0;
    uint32_t crt_wait_time_mcu_ready_ms = 0;

    while (1)
    {
        ret = _stp_controller_data->wait_signal->wait_for_device_ready();

        if (ret > 0)
        {
            // In this case ready_for_transaction = true
            break;
        }

        if (ret < 0)
            return ret;

        // if STP still in INIT state, we keep waiting
        // This is to avoid any corner cases whihc might not require STP/MCU to be up
        if (_stp_controller_data->state == STP_STATE_INIT)
        {
            continue;
        }

        if (_stp_controller_data->handshake->device_can_receive())
        {
            STP_LOG_ERROR("MCU ready timer expired: IRQ missed!");
            stp_mcu_ready_timer_expired_irq_missed_counter++;
            break;
        }

        crt_wait_time_mcu_ready_ms += STP_MCU_READY_TIMEOUT_MS;

        if (crt_wait_time_mcu_ready_ms >= STP_MCU_READY_TOTAL_TIMEOUT_MS)
        {
            /* TODO: Eventually, we can decide if we want
             * to do something else here
             */
            STP_LOG_ERROR("MCU still not ready!");

            crt_wait_time_mcu_ready_ms = 0;
        }
    }

    _stp_controller_data->wait_signal->set_ready_for_transaction(false);
    _stp_controller_data->pending_device_ready_signal = true;

    /* If last notification was SUSPENDED
     * We need to signal suspend callback to resume
     * There is no data to be sent, since we check that in SUSPEND callback
     */
    //	if (_stp_controller_data->last_tx_notification == STP_IN_SOC_SUSPENDED)
    //		spi_stp_signal_suspend();
    return STP_SUCCESS;
}

/* controller main transaction entry. Should be called from a separate thread */
int32_t stp_controller_transaction_thread(void)
{
    int32_t ret = STP_SUCCESS;

    STP_ASSERT(_stp_controller_data && _stp_controller_data->handshake, "Invalid internal data");

    ret = stp_controller_wait_for_device_ready();
    if (ret < 0)
        return ret;

    ret = STP_SUCCESS;

    if (_stp_controller_data->state == STP_STATE_INIT)
    {
        stp_controller_init_transaction();
    }
    else if (_stp_controller_data->state == STP_STATE_DATA)
    {
        if (!stp_controller_data_transaction())
            stp_controller_wait_for_data();
    }
    else
    {
        STP_LOG_ERROR("STP controller unknown state: %zu", (size_t)_stp_controller_data->state);
        ret = STP_ERROR;
    }

    return ret;
}

void stp_controller_disconnect(uint8_t channel)
{
    _stp_controller_data->channels[channel].controller_connected = false;

    stp_pl_reset(&_stp_controller_data->channels[channel].tx_pl);
    stp_pl_reset(&_stp_controller_data->channels[channel].rx_pl);

    /* wake up read/write to return error */
    _stp_controller_data->wait_signal->signal_read(channel);
    _stp_controller_data->wait_signal->signal_write(channel);

    stp_controller_set_notification(channel, STP_IN_DISCONNECTED);
}

void stp_controller_invalidate_channel(uint8_t channel)
{
    _stp_controller_data->channels[channel].valid_session = false;

    if (_stp_controller_data->channels[channel].controller_connected)
    {
        stp_pl_reset(&_stp_controller_data->channels[channel].tx_pl);
        stp_pl_reset(&_stp_controller_data->channels[channel].rx_pl);
    }

    _stp_controller_data->channels[channel].device_connected     = false;
    _stp_controller_data->channels[channel].controller_connected = false;

    /* wake up read/write to return error */
    _stp_controller_data->wait_signal->signal_read(channel);
    _stp_controller_data->wait_signal->signal_write(channel);
}

void stp_controller_invalidate_session(void)
{
    STP_LOG_ERROR("STP stp_invalidate_session!");

    for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++)
    {
        stp_controller_invalidate_channel(i);
    }

    if (_stp_controller_data->callback_client)
        _stp_controller_data->callback_client(STP_CONTROLLER_EVENT_INIT);
}

bool stp_controller_is_channel_valid(uint8_t channel)
{
    return (channel < STP_TOTAL_NUM_CHANNELS);
}

void stp_controller_prepare_common_tx_transaction(void)
{
    uint8_t *buffer                     = _stp_controller_data->tx_buffer;
    struct stp_data_header_type *header = (struct stp_data_header_type *)buffer;

    header->channels_status = stp_get_channels_status(_stp_controller_data->channels);
    _stp_controller_data->prev_channels_status = header->channels_status;

    header->crc = stp_set_bad_crc_value(header->crc, _stp_controller_data->pending_bad_crc);

    uint16_t calculated_crc = stp_calculate_crc_for_transaction_packet(buffer);

    stp_set_crc(&header->crc, calculated_crc);
}

/* Initialize the STP controller/device internal data */
void stp_controller_init_internal(struct stp_controller_transport_table *transport)
{
    STP_ASSERT(transport, "Invalid parameter(s)");

    memset(_stp_controller_data, 0, sizeof(*_stp_controller_data));

    _stp_controller_data->transport = transport;

    _stp_controller_data->state = STP_STATE_INIT;

    _stp_controller_data->pending_bad_crc = false;

    _stp_controller_data->device_channels_status = 0xFFFFFFFF;
    _stp_controller_data->prev_channels_status   = 0xFFFFFFFF;

    STP_LOCK_INIT(_stp_controller_data->lock_notification);
}

void stp_controller_init_transaction(void)
{
    struct stp_data_header_type *ack_rec =
        (struct stp_data_header_type *)_stp_controller_data->rx_buffer;
    struct stp_data_header_type *ack_send =
        (struct stp_data_header_type *)_stp_controller_data->tx_buffer;

    ack_send->channel_opcode = stp_set_opcode_value(ack_send->channel_opcode, STP_OPCODE_INIT);
    ack_send->len_data       = STP_INIT_SIZE;

    _stp_controller_data->transport->send_receive_data(
        _stp_controller_data->tx_buffer, _stp_controller_data->rx_buffer, STP_TOTAL_DATA_SIZE);

    uint8_t opcode = stp_get_opcode_value(ack_rec->channel_opcode);
    if (opcode == STP_OPCODE_INIT)
    {
        _stp_controller_data->state = STP_STATE_DATA;
        STP_LOG_INFO("STP Controller: init: init done!");
        stp_controller_invalidate_session();
    }
}

#define STP_BAD_CRC_BACKOFF_DELAY_MS 10
void stp_controller_process_data_transaction(struct stp_pending_tx *tx)
{
    DATA_HEADER_TYPE *header;

    header = (DATA_HEADER_TYPE *)_stp_controller_data->rx_buffer;

    bool check_crc = stp_check_crc(_stp_controller_data->rx_buffer);
    if (!check_crc)
    {
        _stp_controller_data->bad_crcs_in_a_row++;
        _stp_controller_data->pending_bad_crc = true;
        _stp_controller_data->state           = STP_STATE_INIT;
        STP_LOG_ERROR("STP controller: Bad CRC!");

        if (_stp_controller_data->bad_crcs_in_a_row++ > 3){
            STP_MSLEEP(STP_BAD_CRC_BACKOFF_DELAY_MS);
        }
        return;
    }

    // If we receive a good packet, just reset the bad_crc counter
    _stp_controller_data->bad_crcs_in_a_row = 0;

    uint8_t opcode = stp_get_opcode_value(header->channel_opcode);

    _stp_controller_data->device_channels_status = header->channels_status;

    if (opcode == STP_OPCODE_EMPTY)
    {
        _stp_controller_data->state = STP_STATE_DATA;
    }
    else if (opcode == STP_OPCODE_DATA)
    {
        uint8_t channel = stp_get_channel_value(header->channel_opcode);
        if (channel >= STP_TOTAL_NUM_CHANNELS)
        {
            STP_LOG_ERROR("STP Controller: Invalid channel %zu", (size_t)channel);
            return;
        }
        else if (!_stp_controller_data->channels[channel].controller_connected)
        {
            STP_LOG_ERROR(
                "STP Controller: Received data for an unconnected channel %zu", (size_t)channel);
            return;
        }

        _stp_controller_data->pending_bad_crc = !stp_process_rx_packet(
            &_stp_controller_data->channels[channel], _stp_controller_data->rx_buffer);
        if (!_stp_controller_data->pending_bad_crc)
        {
            _stp_controller_data->wait_signal->signal_read(channel);
        }
        _stp_controller_data->state = STP_STATE_DATA;
    }
    else if (opcode == STP_OPCODE_NOTIFICATION)
    {
        uint32_t notification;
        uint8_t channel;

        _stp_controller_data->pending_bad_crc =
            !stp_process_rx_notification(_stp_controller_data->rx_buffer, &channel, &notification);
        if (!_stp_controller_data->pending_bad_crc)
        {
            stp_controller_rx_notification(channel, notification);
        }

        _stp_controller_data->state = STP_STATE_DATA;
    }
    else
    {
        _stp_controller_data->pending_bad_crc = true;
        _stp_controller_data->state           = STP_STATE_INIT;
    }

    if ((opcode == STP_OPCODE_EMPTY) || (opcode == STP_OPCODE_DATA) ||
        (opcode == STP_OPCODE_NOTIFICATION))
    {
        if (stp_get_bad_crc_value(header->crc))
        {
            tx->sent = false;
            // TBD: invalidate session and inform clients
        }
    }

    _stp_controller_data->pending.tx = *tx;
}

bool stp_controller_has_data_to_send(void)
{
    bool is_empty = true;

    for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++)
    {
        if (_stp_controller_data->channels[i].pending_tx_notification != STP_IN_NONE)
            return true;

        if (_stp_controller_data->channels[i].controller_connected)
        {
            stp_pl_is_empty(&_stp_controller_data->channels[i].tx_pl, &is_empty);

            if (!is_empty)
            {
                break;
            }
        }
    }

    return !is_empty;
}
