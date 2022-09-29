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

#ifndef STP_COMMON_H
#define STP_COMMON_H

#include <stp/common/stp_os.h>
#include <stp/common/stp_pipeline.h>
#include <stp/common/stp_common_public.h>

#define STP_NOTIFICATION_SIZE sizeof(uint32_t)
#define STP_EMPTY_SIZE 0
#define STP_INIT_SIZE 0

#define STP_OPCODE_EMPTY ((uint8_t)0x00)
#define STP_OPCODE_INIT ((uint8_t)0x05)
#define STP_OPCODE_DATA ((uint8_t)0x06)
#define STP_OPCODE_NOTIFICATION ((uint8_t)0x03)

#define CRC_MASK ((uint16_t)0x7FFF)
// Bit 15 is used for bad_crc value
#define BAD_CRC_MASK (~CRC_MASK)

#define CHANNEL_MASK ((uint8_t)0xF8)
#define CHANNEL_NUM_BITS_SHIFT 3

#define OPCODE_MASK ((uint8_t)0x7)

// values of the possible states
// State machine flow:
// INIT->DATA->DATA->DATA
// for any missmatch between masater and device,
// it goes back in INIT state on both sides
enum
{
    // initial state
    STP_STATE_INIT = 0xAA,
    // data state
    STP_STATE_DATA = 0x33,
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"
#endif // __clang__

// These are the STP internal MCU-SoC notifications
enum
{
    // No notification
    STP_IN_NONE = 0x00000000,
    // SoC Connected
    STP_IN_CONNECTED = 0x00C08EC1,
    // SoC Disconnected
    STP_IN_DISCONNECTED = 0xD1C08EC1,
    // SoC Suspended
    STP_IN_SOC_SUSPENDED = 0xD2C08EC1,
    // SoC Resumed
    STP_IN_SOC_RESUMED = 0xD3C08EC1,
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__

struct stp_pending_tx
{
    bool sent;
    uint8_t channel;
};

struct stp_pending
{
    struct stp_pending_tx tx;
    uint32_t tx_notification;
};

struct stp_channel
{
    // priority
    uint8_t priority;
    // TX pipeline
    PL_TYPE tx_pl;
    // RX pipeline
    PL_TYPE rx_pl;

    // info about the pending transactions
    // set by processing data transaction
    // used for following control transaction
    uint32_t pending_tx_notification;

    // this is set when stp_open() is called and reset on stp_close()
    _Atomic bool device_connected;

    // this is set when a notifier is received from controller side
    _Atomic bool controller_connected;

    // this is used to invalidate the session after INIT
    // usecase: SoC reset without close() or close() notification
    // is not sent to MCU
    _Atomic bool valid_session;

    // notification callback
    _Atomic stp_channel_callback callback;

    // the limit for rx_available_data when STP_NOTIFICATION_RX_DATA_AVAILABLE is sent to the client
    _Atomic uint32_t rx_available_data_limit_notification;

    // the limit for tx_available_space when STP_NOTIFICATION_TX_SPACE_AVAILABLE is sent to the
    // client
    uint32_t tx_available_space_limit_notification;
};

// Packet metadata for the data transaction
// The fields need to be declared in this order for automatic packing by the compiler
struct stp_data_header_type
{
    // CRC of data sent
    // [15] bad_crc [14:0] crc
    uint16_t crc;
    // [7:3] channel [2:0] opcode
    uint8_t channel_opcode;
    // size of data sent
    uint8_t len_data;
    // channels status
    uint32_t channels_status;
};

struct stp_pipeline_config
{
    uint8_t *buffer;
    uint32_t buffer_size;
};

struct stp_pipelines_config
{
    struct stp_pipeline_config *rx_pipeline;
    struct stp_pipeline_config *tx_pipeline;
};

#ifdef __cplusplus
extern "C" {
#endif

uint16_t stp_calculate_checksum(const uint8_t *buffer, uint32_t buffer_size);

uint16_t stp_calculate_crc_for_transaction_packet(const uint8_t *buffer);

void stp_set_crc(uint16_t *full_crc_field, uint16_t new_crc_value);

uint16_t stp_get_crc(uint16_t full_crc_field);

bool stp_get_bad_crc_value(uint16_t full_crc_field);

uint16_t stp_set_bad_crc_value(uint16_t full_crc_field, bool bad_crc);

uint8_t stp_get_channel_value(uint8_t channel_opcode);

uint8_t stp_set_channel_value(uint8_t channel_opcode, uint8_t channel);

uint8_t stp_get_opcode_value(uint8_t channel_opcode);

uint8_t stp_set_opcode_value(uint8_t channel_opcode, uint8_t opcode);

bool stp_check_crc(const uint8_t *buffer);

bool stp_channel_connected_available_has_tx_data(uint8_t channel,
                                                 struct stp_channel *channels,
                                                 uint32_t channels_status);

uint8_t stp_get_highest_priority_channel_with_data(struct stp_channel *channels,
                                                   uint32_t channels_status);

uint8_t stp_get_channel_with_data(struct stp_channel *channels,
                                  uint32_t channels_status,
                                  struct stp_pending *pending);

void stp_prepare_tx_packet_data(uint8_t channel, PL_TYPE *tx_pl, uint8_t *buffer);

bool stp_process_rx_packet(struct stp_channel *channel, const uint8_t *buffer);

void stp_prepare_tx_notification_packet(uint8_t channel, uint8_t *buffer, uint32_t notification);

int32_t stp_get_channel_with_notification(struct stp_channel *channels);

bool stp_process_rx_notification(const uint8_t *buffer, uint8_t *channel, uint32_t *notification);

uint32_t stp_get_channels_status(struct stp_channel *channels);

// define metadata type
#define DATA_HEADER_TYPE struct stp_data_header_type
// size of metadata data packet
#define HEADER_DATA_SIZE (sizeof(struct stp_data_header_type))
// actual data size of a data packet
#define ACTUAL_DATA_SIZE (STP_TOTAL_DATA_SIZE - HEADER_DATA_SIZE)

#ifdef __cplusplus
}
#endif

#endif
