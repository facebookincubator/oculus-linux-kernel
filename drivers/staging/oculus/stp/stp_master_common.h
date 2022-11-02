/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef STP_MASTER_COMMON_H
#define STP_MASTER_COMMON_H

#include "stp_os.h"
#include "stp_pipeline.h"
#include "stp_master.h"

// total size of a data packet
#define TOTAL_DATA_SIZE	256

// align RX/TX SPI buffers to 64 bytes
#define SPI_ALIGN_DATA 64

// magic number used for initial packet
#define STP_MAGIC_NUMBER_INIT		0x5127755F
// magic number used for data packet
#define STP_MAGIC_NUMBER_DATA		0x512ABC78
// magic number for empty data packet
#define STP_MAGIC_NUMBER_EMPTY		0xEE00DD11
// magic number for notification packet
#define STP_MAGIC_NUMBER_NOTIFICATION		0x88001E88

// values of the possible states
// State machine flow:
// INIT->DATA->DATA
// for any missmatch between masater and slave,
// it goes back in INIT state on both sides
enum {
	// initial state
	STP_STATE_INIT = 0xAA,
	// data state
	STP_STATE_DATA = 0x33,
};

// Internal SoC-MCU notifications
enum {
	// no notification
	STP_IN_NONE = 0x00000000,

	// MCU connected
	STP_IN_CONNECTED = 0x00C08EC1,

	// MCU disconnected
	STP_IN_DISCONNECTED = 0xD1C08EC1,

	// SoC in suspend
	STP_SOC_SUSPENDED = 0xD2C08EC1,

	// SoC active
	STP_SOC_RESUMED = 0xD3C08EC1,
};

// values of acked for control transactions
// set based in previous data packet data
enum stp_data_ack_type {
	// no acked information
	STP_ACK_NO_INFO = 0xaa00bb00,
	// data was valid and accepted
	STP_ACK_ACCEPTED,
	// data was valid but no space to store it
	STP_ACK_NO_SPACE,
	// magic number was enmpty (no data)
	STP_ACK_EMPTY_MAGIC,
	// magic number was invalid
	STP_ACK_BAD_MAGIC,
	// bad CRC
	STP_ACK_BAD_CRC,
	// size of data was invalid
	STP_ACK_BAD_SIZE
};

// values used for internal events
// the internal events might result in adding them to log or stats
enum stp_internal_events {
	// RX errors
	STP_IE_RX_BAD_MAGIC_NUMBER = 0,
	STP_IE_RX_BAD_LEN,
	STP_IE_RX_BAD_CRC,
	STP_IE_RX_PIPELINE_FULL,

	// TX errors
	STP_IE_TX_BAD_MAGIC_NUMBER,
	STP_IE_TX_BAD_LEN,
	STP_IE_TX_BAD_CRC,
	STP_IE_TX_PIPELINE_FULL,

	// any invalid magic received when in init state
	STP_IE_MISMATCH_INIT,

	// Receive ok
	STP_IE_RX_OK,
	// Send ok
	STP_IE_TX_OK,

	// start SPI transaction
	STP_IE_ENTER_SPI,

	// exit SPI transaction
	STP_IE_EXIT_SPI,

	// enter wait for slave
	STP_IE_ENTER_WAIT_SLAVE,

	// exit wait for slave
	STP_IE_EXIT_WAIT_SLAVE,

	// enter wait for read
	STP_IE_ENTER_WAIT_READ,

	// exit wait for read
	STP_IE_EXIT_WAIT_READ,

	// enter wait for write
	STP_IE_ENTER_WAIT_WRITE,

	// exit wait for write
	STP_IE_EXIT_WAIT_WRITE,

	// add data RX pipeline
	STP_IE_ADD_DATA_RX_PIPELINE,

	// add data TX pipeline
	STP_IE_ADD_DATA_TX_PIPELINE,

	// enter wait for data
	STP_IE_ENTER_WAIT_DATA,

	// exit wait for slave
	STP_IE_EXIT_WAIT_DATA,

	// IRQ ready
	STP_IE_IRQ_READY,

	// set signal slave ready
	STP_IE_SET_SIGNAL_SLAVE_READY,

	// reset signal slave ready
	STP_IE_RESET_SIGNAL_SLAVE_READY,

	// IRQ data
	STP_IE_IRQ_DATA,

	// set signal data
	STP_IE_SET_SIGNAL_DATA,

	// reset signal data
	STP_IE_RESET_SIGNAL_DATA,

	// get data RX pipeline
	STP_IE_GET_DATA_RX_PIPELINE,

	// get data TX pipeline
	STP_IE_GET_DATA_TX_PIPELINE,

	// slave connected
	STP_IE_SLAVE_CONNECTED,

	// slave disconnected
	STP_IE_SLAVE_DISCONNECTED,

	// slave connected
	STP_IE_MASTER_CONNECTED,

	// slave disconnected
	STP_IE_MASTER_DISCONNECTED,

	// exit init state
	STP_IE_EXIT_INIT_STATE,

	// numbers of internal events
	STP_IE_NUM
};

#define ACK_TYPE enum stp_data_ack_type

struct stp_pending_tx {
	bool sent;
	unsigned int total_data;
	unsigned int pipeline_head;
};

struct stp_debug_type {
	bool wait_for_slave;
	bool wait_for_data;
	bool wait_for_read;
	bool wait_for_write;
	bool signal_slave_ready;
	bool signal_data;
	unsigned long irq_slave_has_data_counter;
	unsigned long irq_slave_ready_counter;
	unsigned long router_drops_full_pipeline;
};

/* STP internal data */
struct stp_type {
	// transaport interface (from upper layer)
	struct stp_transport_table *transport;
	// handshake table
	struct stp_master_handshake_table *handshake;
	// wait-signal table
	struct stp_master_wait_signal_table *wait_signal;

	// TX pipeline
	PL_TYPE tx_pl;
	// RX pipeline
	PL_TYPE rx_pl;
	// TX buffer used for current packet
	U8 tx_buffer[TOTAL_DATA_SIZE] __aligned(SPI_ALIGN_DATA);
	// RX buffer used for current packet
	U8 rx_buffer[TOTAL_DATA_SIZE] __aligned(SPI_ALIGN_DATA);

	// current state (init, data or control)
	unsigned int state;

	// info about the pending transactions
	// set by processing data transaction
	// used for following control transaction
	struct _pending {
		struct _rx {
			ACK_TYPE acked;
		} rx;

		struct stp_pending_tx tx;

		unsigned int tx_notification;
	} pending;

	atomic_t data_av;
	wait_queue_head_t trans_q;

	void (*callback_client)(int event);

	bool slave_ready;

	wait_queue_head_t slave_ready_q;

	// Set/reset when MCU notifications are received
	bool slave_connected;

	// set/reset when open/close are called from STP client
	bool master_connected;

	STP_LOCK_TYPE lock_notification;

	// this is used to invalidate the session after INIT
	// example: MCU restart, SoC need to open again
	bool valid_session;

	bool wait_for_data;

	bool flashing_mode;

	unsigned int last_tx_notification;
};

// Packet metadata for init and control transactions
struct stp_control_header_type {
	// magic number used for validation
	int magic_number;
	// ack value used for control transaction
	unsigned int status;
};

// Packet metadata for the data transaction
struct stp_data_header_type {
	// magic number used for validation
	int magic_number;
	// size of data sent, if any
	unsigned int len_data;
	// CRC of data sent
	int crc;
	// Prev ack
	unsigned int prev_ack;
};

// define metadata type
#define DATA_HEADER_TYPE struct stp_data_header_type

// size of metadata control packet
#define CONTROL_SIZE	(sizeof(struct stp_control_header_type))
// size of metadata data packet
#define HEADER_DATA_SIZE		(sizeof(struct stp_data_header_type))
// actual data size of a data packet
#define ACTUAL_DATA_SIZE \
		(TOTAL_DATA_SIZE - HEADER_DATA_SIZE)

extern struct stp_type *_stp_data;
extern struct stp_statistics _stp_stats;
extern struct stp_debug_type _stp_debug;

/* Calculate checksum  */
int calculate_checksum(U8 *buffer, unsigned int buffer_size);

/* Initialize the STP master/slave internal data */
void stp_init(struct stp_transport_table *_transport,
	struct stp_pipelines_config *pipelines);

void stp_init_transaction(void);

void process_control_transaction(bool *valid_packet);

void stp_init_pending(void);

/* Prepare TX packet */
void prepare_tx_packet_data_rel(U8 *buffer, struct stp_pending_tx *tx,
	UINT rel_head);

void process_data_transaction(struct stp_pending_tx *tx);

void stp_master_signal_data(void);

void stp_master_signal_slave_ready(void);

// record an internal event
int stp_ie_record(enum stp_internal_events event, unsigned int data);
/* Process RX notifications */
void process_rx_notification(ACK_TYPE *acked);

void stp_set_notification(unsigned int notification);

void prepare_tx_notification(bool *do_transaction);

void stp_disconnect(void);

void stp_invalidate_session(void);

int stp_check_for_rw_errors(void);

extern unsigned int stp_mcu_ready_timer_expired_irq_missed_counter;

#endif
