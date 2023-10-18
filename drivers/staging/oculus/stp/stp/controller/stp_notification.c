#include <stp/common/stp_pipeline.h>
#include <stp/common/stp_common.h>
#include <stp/common/stp_logging.h>
#include <stp/controller/stp_controller.h>
#include <stp/controller/stp_controller_common.h>

void stp_controller_rx_notification_connected(uint8_t channel)
{
	if (_stp_controller_data->channels[channel].controller_connected) {
		stp_pl_reset(&_stp_controller_data->channels[channel].tx_pl);
		stp_pl_reset(&_stp_controller_data->channels[channel].rx_pl);
	}

	_stp_controller_data->channels[channel].device_connected = true;
	_stp_controller_data->wait_signal->signal_open(channel);
}

void stp_controller_rx_notification_disconnected(uint8_t channel)
{
	_stp_controller_data->channels[channel].device_connected = false;

	if (_stp_controller_data->channels[channel].controller_connected) {
		stp_pl_reset(&_stp_controller_data->channels[channel].tx_pl);
		stp_pl_reset(&_stp_controller_data->channels[channel].rx_pl);
	}

	// wake up read/write to return error
	_stp_controller_data->wait_signal->signal_read(channel);
	_stp_controller_data->wait_signal->signal_write(channel);
}

void stp_controller_rx_notification(uint8_t channel, uint32_t notification)
{
	switch (notification) {
	case STP_IN_CONNECTED:
		stp_controller_rx_notification_connected(channel);
		break;
	case STP_IN_DISCONNECTED:
		stp_controller_rx_notification_disconnected(channel);
		break;
	default:
		STP_LOG_ERROR("Unknown notification %zu\n",
			      (size_t)notification);
		break;
	}
}

void stp_controller_set_notification(uint8_t channel, uint32_t notification)
{
	STP_LOCK(_stp_controller_data->lock_notification);

	if (_stp_controller_data->channels[channel].pending_tx_notification !=
	    STP_IN_NONE)
		STP_LOG_ERROR("STP controller overwrite notif 0x%zx with 0x%zx",
			      (size_t)_stp_controller_data->channels[channel]
				      .pending_tx_notification,
			      (size_t)notification);

	_stp_controller_data->channels[channel].pending_tx_notification =
		notification;

	STP_UNLOCK(_stp_controller_data->lock_notification);

	stp_controller_signal_has_data();
}

void stp_controller_prepare_tx_notification(bool *do_transaction)
{
	STP_ASSERT(do_transaction, "Invalid parameter(s)");

	STP_LOCK(_stp_controller_data->lock_notification);

	*do_transaction = false;

	int channel_index = stp_get_channel_with_notification(
		_stp_controller_data->channels);
	if (channel_index != -1) {
		stp_prepare_tx_notification_packet(
			(uint8_t)channel_index, _stp_controller_data->tx_buffer,
			_stp_controller_data->channels[channel_index]
				.pending_tx_notification);

		*do_transaction = true;

		_stp_controller_data->channels[channel_index]
			.pending_tx_notification = STP_IN_NONE;
	}

	STP_UNLOCK(_stp_controller_data->lock_notification);
}
