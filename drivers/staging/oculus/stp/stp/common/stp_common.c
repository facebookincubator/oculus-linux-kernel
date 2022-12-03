/*
 * Copyright Meta Platforms, Inc. and its affiliates.
 *
 * NOTICE OF CONFIDENTIAL AND PROPRIETARY INFORMATION & TECHNOLOGY:
 * The information and technology contained herein (including the accompanying binary code)
 * is the confidential information of Meta Platforms, Inc. and its affiliates (collectively,
 * "Meta"). It is protected by applicable copyright and trade secret law, and may be claimed
 * in one or more U.S. or foreign patents or pending patent applications. Meta retains all right,
 * title and interest (including all intellectual property rights) in such information and
 * technology, and no licenses are hereby granted by Meta. Unauthorized use, reproduction, or
 * dissemination is a violation of Meta's rights and is strictly prohibited.
 */

#include <stp/common/stp_os.h>
#include <stp/common/stp_common.h>
#include <stp/common/stp_logging.h>

#define LOG_NUM_BYTES_PER_LINE 16
#define BUFFER_LOG_LEN 128

static const char *packet_error_to_string[STP_PACKET_ERROR_NUM_ERRORS] = {
	[STP_PACKET_ERROR_BAD_CRC] = "BAD_CRC",
	[STP_PACKET_ERROR_INVALID_CHANNEL] = "INVALID_CHANNEL",
	[STP_PACKET_ERROR_PROCESS_RX_DATA] = "PROCESS_RX_DATA",
	[STP_PACKET_ERROR_PROCESS_RX_NOTIFICATION] = "PROCESS_RX_NOTIFICATION",
	[STP_PACKET_ERROR_UNKNOWN_OPCODE] = "UNKNOWN_OPCODE",
};

uint8_t stp_get_channel_value(uint8_t channel_opcode)
{
	uint8_t channel = channel_opcode & STP_CHANNEL_MASK;

	channel >>= STP_CHANNEL_NUM_BITS_SHIFT;

	return channel;
}

uint8_t stp_set_channel_value(uint8_t channel_opcode, uint8_t channel)
{
	channel <<= STP_CHANNEL_NUM_BITS_SHIFT;

	channel_opcode &= ~STP_CHANNEL_MASK;
	channel_opcode |= channel;

	return channel_opcode;
}

uint8_t stp_get_opcode_value(uint8_t channel_opcode)
{
	uint8_t opcode = channel_opcode & STP_OPCODE_MASK;

	return opcode;
}

uint8_t stp_set_opcode_value(uint8_t channel_opcode, uint8_t opcode)
{
	channel_opcode &= ~STP_OPCODE_MASK;
	opcode &= STP_OPCODE_MASK;
	channel_opcode |= opcode;

	return channel_opcode;
}

// Assumes a fully formed packet
uint16_t
stp_calculate_crc_for_transaction_packet(const uint8_t *const buffer,
					 enum calculate_pkt_crc_result *result)
{
	STP_ASSERT(buffer && result, "Invalid parameter(s)");
	struct stp_data_header_type *header =
		(struct stp_data_header_type *)buffer;

	uint32_t crc_size = sizeof(header->crc);
	uint32_t data_len = header->len_data;

	// The CRC field is assumed to be the first field in the buffer so move past this field
	// for the start of the CRC calculation
	const uint8_t *p_data = buffer + crc_size;

	// Check data length
	if (data_len > STP_ACTUAL_DATA_SIZE) {
		*result = STP_CALCULATE_CRC_BAD_DATA_LENGTH;
		return 0;
	}

	// Size of data over which to calculate CRC
	uint32_t data_size_for_crc = STP_HEADER_DATA_SIZE - crc_size + data_len;

	// Do it
	uint16_t calculated_crc = STP_CRC_COMPUTE(p_data, data_size_for_crc);

	STP_LOG_DEBUG(
		"calc_crc_pkt: b:%p p:%p hdr_sz:%zu crc_sz:%zu datalen:%zu szforcrc:%zu crc:0x%x",
		buffer, p_data, (size_t)STP_HEADER_DATA_SIZE, (size_t)crc_size,
		(size_t)data_len, (size_t)data_size_for_crc, calculated_crc);

	*result = STP_CALCULATE_CRC_SUCCESS;
	return calculated_crc;
}

bool stp_check_crc(const uint8_t *buffer)
{
	enum calculate_pkt_crc_result calculate_result =
		STP_CALCULATE_CRC_UNKNOWN;
	struct stp_data_header_type *header =
		(struct stp_data_header_type *)buffer;

	uint16_t crc_calculated = stp_calculate_crc_for_transaction_packet(
		buffer, &calculate_result);

	switch (calculate_result) {
	case STP_CALCULATE_CRC_UNKNOWN:
		STP_LOG_ERROR(
			"Failed to calculate CRC: STP_CALCULATE_CRC_UNKNOWN");
		return false;

	case STP_CALCULATE_CRC_BAD_DATA_LENGTH:
		STP_LOG_ERROR(
			"Failed to calculate CRC: STP_CALCULATE_CRC_BAD_DATA_LENGTH");
		return false;

	case STP_CALCULATE_CRC_SUCCESS:
		if (crc_calculated != header->crc) {
			STP_LOG_ERROR_RATE_LIMIT(
				"Calculated CRC (0x%x) doesn't match CRC from header (0x%x)",
				crc_calculated, header->crc);
			return false;
		}
		break;
	default:
		STP_LOG_ERROR("Unknown calculate CRC result");
		return false;
	}

	return true;
}

bool stp_channel_connected_available_has_tx_data(uint8_t channel,
						 struct stp_channel *channels,
						 uint32_t channels_status)
{
	// check if the channel is open on both sides
	if (!channels[channel].device_connected ||
	    !channels[channel].controller_connected) {
		return false;
	}

	// check if the master can receive data on this channel
	uint32_t crt_index_availability = 1u << channel;

	if (!(crt_index_availability & channels_status))
		return false;

	bool is_empty = true;

	stp_pl_is_empty(&channels[channel].tx_pl, &is_empty);

	return !is_empty;
}

uint8_t stp_get_highest_priority_channel_with_data(struct stp_channel *channels,
						   uint32_t channels_status)
{
	uint8_t selected_channel_index = STP_TOTAL_NUM_CHANNELS;
	uint8_t selected_channel_priority;

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		bool has_data = stp_channel_connected_available_has_tx_data(
			i, channels, channels_status);

		if (has_data) {
			if ((selected_channel_index ==
			     STP_TOTAL_NUM_CHANNELS) ||
			    (channels[i].priority <
			     selected_channel_priority)) {
				selected_channel_index = i;
				selected_channel_priority =
					channels[i].priority;
			}
		}
	}

	return selected_channel_index;
}

uint8_t stp_get_channel_with_data(struct stp_channel *channels,
				  uint32_t channels_status,
				  struct stp_pending *pending)
{
	uint8_t selected_channel_index =
		stp_get_highest_priority_channel_with_data(channels,
							   channels_status);

	if (selected_channel_index == STP_TOTAL_NUM_CHANNELS)
		return selected_channel_index;

	if (pending->tx.channel != STP_TOTAL_NUM_CHANNELS) {
		uint8_t previous_priority =
			channels[pending->tx.channel].priority;
		if (channels[selected_channel_index].priority !=
		    previous_priority) {
			return selected_channel_index;
		}
	}

	uint8_t crt_channel = selected_channel_index + 1;

	if (crt_channel == STP_TOTAL_NUM_CHANNELS)
		crt_channel = 0;

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS - 1; i++) {
		bool has_data = stp_channel_connected_available_has_tx_data(
			crt_channel, channels, channels_status);
		if (has_data) {
			if (channels[crt_channel].priority ==
			    channels[selected_channel_index].priority) {
				return crt_channel;
			}
		}

		if (++crt_channel == STP_TOTAL_NUM_CHANNELS)
			crt_channel = 0;
	}

	return selected_channel_index;
}

void stp_prepare_tx_packet_data(uint8_t channel, PL_TYPE *tx_pl,
				uint8_t *buffer, bool log)
{
	struct stp_data_header_type *header;
	uint32_t total_size;

	STP_ASSERT(buffer && tx_pl, "Invalid parameter(s)");

	uint32_t total_data = STP_ACTUAL_DATA_SIZE;

	STP_LOCK(tx_pl->lock);

	stp_pl_get_data_size(tx_pl, &total_size);

	if (total_size < total_data)
		total_data = total_size;

	stp_pl_get_data(tx_pl, buffer + STP_HEADER_DATA_SIZE, total_data);

	if (log) {
		uint32_t counter = total_data;
		uint32_t index = 0;
		uint8_t *p_data = buffer + STP_HEADER_DATA_SIZE;

		while (counter > 0) {
#define BUFFER_LOG_LEN 128
			char buffer_log[BUFFER_LOG_LEN] = "";
			uint32_t max = (counter >= LOG_NUM_BYTES_PER_LINE) ?
						     LOG_NUM_BYTES_PER_LINE :
						     counter;

			for (uint32_t i = 0; i < max; i++) {
				uint32_t crt_len = strlen(buffer_log);

				snprintf(buffer_log + crt_len,
					 BUFFER_LOG_LEN - crt_len, "%x ",
					 p_data[index++]);
			}
			STP_LOG_TX_DATA(buffer_log);
			counter -= max;
			STP_MSLEEP(1);
		}
	}

	header = (struct stp_data_header_type *)buffer;
	header->channel_opcode =
		stp_set_opcode_value(header->channel_opcode, STP_OPCODE_DATA);
	header->len_data = total_data;
	header->channel_opcode =
		stp_set_channel_value(header->channel_opcode, channel);

	STP_UNLOCK(tx_pl->lock);
}

bool stp_process_rx_packet(struct stp_channel *channel, const uint8_t *buffer,
			   bool log)
{
	struct stp_data_header_type *header;
	uint32_t pl_size_av;
	const uint8_t *p_data;

	header = (struct stp_data_header_type *)buffer;

	p_data = buffer + sizeof(struct stp_data_header_type);

	if (header->len_data > STP_ACTUAL_DATA_SIZE)
		return false;

	STP_LOCK(channel->rx_pl.lock);

	stp_pl_get_available_space(&channel->rx_pl, &pl_size_av);

	bool ret = true;

	if (pl_size_av < header->len_data) {
		ret = false;
		goto error;
	}

	if (log) {
		uint32_t counter = header->len_data;
		uint32_t index = 0;

		while (counter > 0) {
			char buffer_log[BUFFER_LOG_LEN] = "";
			uint32_t max = (counter >= LOG_NUM_BYTES_PER_LINE) ?
						     LOG_NUM_BYTES_PER_LINE :
						     counter;

			for (uint32_t i = 0; i < max; i++) {
				uint32_t crt_len = strlen(buffer_log);

				snprintf(buffer_log + crt_len,
					 BUFFER_LOG_LEN - crt_len, "%x ",
					 p_data[index++]);
			}
			STP_LOG_RX_DATA(buffer_log);

			counter -= max;
			STP_MSLEEP(1);
		}
	}

	stp_pl_add_data(&channel->rx_pl, p_data, header->len_data);

error:
	STP_UNLOCK(channel->rx_pl.lock);

	return ret;
}

void stp_prepare_tx_notification_packet(uint8_t channel, uint8_t *buffer,
					uint32_t notification)
{
	struct stp_data_header_type *header;
	uint32_t *p_notification;

	STP_ASSERT(buffer, "Invalid parameter(s)");

	p_notification = (uint32_t *)(buffer + STP_HEADER_DATA_SIZE);
	*p_notification = notification;

	header = (struct stp_data_header_type *)buffer;
	header->channel_opcode = stp_set_opcode_value(header->channel_opcode,
						      STP_OPCODE_NOTIFICATION);
	header->len_data = STP_NOTIFICATION_SIZE;
	header->channel_opcode =
		stp_set_channel_value(header->channel_opcode, channel);
}

int32_t stp_get_channel_with_notification(struct stp_channel *channels)
{
	int32_t selected_channel_index = -1;
	uint8_t selected_channel_priority;

	for (int i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		if (channels[i].pending_tx_notification != STP_IN_NONE) {
			if (selected_channel_index == -1) {
				selected_channel_index = i;
				selected_channel_priority =
					channels[i].priority;
			} else if (channels[i].priority <
				   selected_channel_priority) {
				selected_channel_index = i;
				selected_channel_priority =
					channels[i].priority;
			}
		}
	}

	return selected_channel_index;
}

bool stp_process_rx_notification(const uint8_t *buffer, uint8_t *channel,
				 uint32_t *notification)
{
	STP_DATA_HEADER_TYPE *header;

	header = (struct stp_data_header_type *)buffer;

	if (!stp_check_crc(buffer)) {
		STP_LOG_ERROR("%s: bad CRC", __func__);
		return false;
	}
	if (header->len_data != STP_NOTIFICATION_SIZE) {
		STP_LOG_ERROR("%s: bad data length: %d", __func__,
			      header->len_data);
		return false;
	}

	*channel = stp_get_channel_value(header->channel_opcode);
	if (*channel >= STP_TOTAL_NUM_CHANNELS)
		return false;

	*notification = *(uint32_t *)(buffer + STP_HEADER_DATA_SIZE);

	return true;
}

uint32_t stp_get_channels_status(struct stp_channel *channels)
{
	uint32_t channels_status = 0;

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		if (channels[i].device_connected) {
			uint32_t pl_size_av;

			stp_pl_get_available_space(&channels[i].rx_pl,
						   &pl_size_av);
			if (pl_size_av >= 2 * STP_TOTAL_DATA_SIZE) {
				uint32_t channel = 1u << i;

				channels_status |= channel;
			}
		}
	}

	return channels_status;
}

static const char *get_packet_error_str(enum packet_error_t error)
{
	if (error >= STP_PACKET_ERROR_NUM_ERRORS)
		return "UNKNOWN_ERROR";

	return packet_error_to_string[error];
}

static char *get_state_str(uint32_t state)
{
	switch (state) {
	case STP_STATE_INIT:
		return "INIT";
	case STP_STATE_DATA:
		return "DATA";
	default:
		return "UNKNOWN";
	}
}

void packet_error(const char *ctx_str, enum packet_error_t error,
		  _Atomic uint32_t *state)
{
	STP_LOG_ERROR_RATE_LIMIT(
		"Packet error (%s): %s: transitioning state: %s -> %s", ctx_str,
		get_packet_error_str(error), get_state_str(*state),
		get_state_str(STP_STATE_INIT));
	*state = STP_STATE_INIT;
}
