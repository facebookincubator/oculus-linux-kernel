/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef STP_MASTER_H
#define STP_MASTER_H

#include "stp_os.h"

/* TBD: define proper error codes */
enum stp_error_type {
	/* success */
	STP_SUCCESS	= 0,
	STP_ERROR_NONE	= 0,

	/* generic error */
	STP_ERROR = STP_SUCCESS - 1,

	/* slave not connected */
	STP_ERROR_SLAVE_NOT_CONNECTED = STP_SUCCESS - 2,

	/* master not connected */
	STP_ERROR_MASTER_NOT_CONNECTED = STP_SUCCESS - 3,

	/* master/slave not synced */
	STP_ERROR_NOT_SYNCED = STP_SUCCESS - 4,

	/* master already open */
	STP_ERROR_MASTER_ALREADY_OPEN = STP_SUCCESS - 5,

	/* master already closed */
	STP_ERROR_MASTER_ALREADY_CLOSED = STP_SUCCESS - 6,

	/* invalid session */
	STP_ERROR_INVALID_SESSION = STP_SUCCESS - 7,

	/* invalid parameters */
	STP_ERROR_INVALID_PARAMETERS = STP_SUCCESS - 8,

	/* user intrerupt */
	STP_ERROR_IO_INTRERRUPT = STP_SUCCESS - 9,

	STP_ERROR_NEXT_ERROR = STP_SUCCESS - 10
};

/* Transactions failures stats */
struct stp_stats_failed_transactions {
	/* total fails */
	unsigned long total;
	/* bad magic number fails */
	unsigned long bad_magic_number;
	/* bad CRC fails */
	unsigned long bad_crc;
	/* invalid size fails */
	unsigned long invalid_size;
	/* no space fails */
	unsigned long not_enough_space;
};

/* Transactions stats */
struct stp_statistics {
	/* missmatch transactions */
	struct _mismatch_stats {
		unsigned long total;
	} mismatch_stats;
	struct _tx_stats {
		/* total transactions */
		unsigned long total_transactions;
		/* toptal valid transactions */
		unsigned long total_transactions_acked;
		/* total data */
		unsigned long total_data;
		/* total data pipeline */
		unsigned long total_data_pipeline;
		/* total valid data */
		unsigned long total_data_acked;
		/* TX failures */
		struct stp_stats_failed_transactions failures;
	} tx_stats;
	struct _rx_stats {
		/* total transactions */
		unsigned long total_transactions;
		/* total valid transactions */
		unsigned long total_transactions_acked;
		/* total data */
		unsigned long total_data;
		/* total data pipeline */
		unsigned long total_data_pipeline;
		/* RX failures */
		struct stp_stats_failed_transactions failures;
	} rx_stats;
};

/* get attributes */
enum stp_get_attribute_type {
	/* Get available TX pipeline */
	STP_TX_AVAILABLE = 0,
	/* Get filled TX pipeline */
	STP_TX_DATA,
	/* Get filled RX pipeline */
	STP_RX_DATA,
	/* Get transaction stats */
	STP_STATS,
	/* Wait for slave attribute */
	STP_WAIT_FOR_SLAVE,
	/* Wait for data attribute */
	STP_WAIT_FOR_DATA,
	/* Get sync status */
	STP_ATTRIB_SYNCED,
	/* Get valid session */
	STP_ATTRIB_VALID_SESSION,
	/* Get slave connected status */
	STP_ATTRIB_SLAVE_CONNECTED,
	/* Get master connected status */
	STP_ATTRIB_MASTER_CONNECTED,

	STP_NUM_GET_ATTRIBUTES
};

/* set attributes */
enum stp_set_attribute_type {
	STP_NUM_SET_ATTRIBUTES
};

/* get attributes */
enum stp_callback_event_type {
	/* Init event */
	STP_EVENT_INIT = 0,

	STP_NUM_EVENTS
};

/* Transport callbacks. Defined by the master/slave clients*/
struct stp_transport_table {
	/* callback used for send data */
	int (*send_data)(U8 *buffer, unsigned int data);
	/* callback used for receiving data */
	int (*receive_data)(U8 *buffer, unsigned int data);
	/* callback for send/receive data */
	int (*send_receive_data)(U8 *inbuf, U8 *outbuf, unsigned int data);
};

/* Master callbacks. Defined by the master client */
struct stp_master_handshake_table {
	/* check if slave has data */
	bool (*slave_has_data)(void);
	/* check if slave can receive data */
	bool (*slave_can_receive)(void);
};

/* Used for STP write_list */
struct stp_write_object {
	const U8 *buffer;
	unsigned int size;
	unsigned int *data_size;
};

struct stp_pipeline_config {
	uint8_t *buffer;
	uint32_t buffer_size;
};

struct stp_pipelines_config {
	struct stp_pipeline_config *rx_pipeline;
	struct stp_pipeline_config *tx_pipeline;
};

struct stp_master_wait_signal_table {
	int (*wait_write)(void);
	void (*signal_write)(void);
	int (*wait_read)(void);
	void (*signal_read)(void);
};

struct stp_init_t {
	struct stp_transport_table *transport;
	struct stp_master_handshake_table *handshake;
	struct stp_master_wait_signal_table *wait_signal;
	struct stp_pipelines_config *pipelines;
};

/* Master/Slave write data in blocking mode*/
int stp_write(const U8 *buffer, UINT buffer_size, UINT *data_size);

/* Master/Slave write data in non-blocking mode*/
int stp_write_nb(const U8 *buffer, unsigned int size, unsigned int *data_size);

/* Master/Slave write list of data in non-blocking mode*/
int stp_write_list(struct stp_write_object *list, unsigned int list_size);

/* Master/Slave read data in non-blocking mode data */
int stp_read_nb(U8 *buffer, unsigned int size, unsigned int *data_size);

/* Master/Slave read data in blocking mode data */
int stp_read(U8 *buffer, UINT buffer_size, UINT *data_size);

/* Master transaction function */
int stp_master_transaction_thread(void);

/* Master initialization */
int stp_init_master(struct stp_init_t *init);

/* Master de-init */
int stp_deinit_master(void);

/* Master/Slave close */
int stp_close(void);

/* Get attributes */
int stp_get(unsigned int attribute, void *p);

/* Set attributes */
int stp_set(unsigned int attribute, void *p);

int stp_set_callback(void (*callback)(int));

void spi_stp_signal_start(void);

int stp_open(void);

int stp_close(void);

bool is_mcu_ready_to_receive_data(void);

bool is_mcu_data_available(void);

int stp_check_for_rw_errors(void);

int calculate_checksum(U8 *buffer, unsigned int buffer_size);

extern struct stp_debug_type _stp_debug;

bool stp_has_data_to_send(void);

bool stp_get_wait_for_data(void);

void spi_stp_signal_suspend(void);

#endif
