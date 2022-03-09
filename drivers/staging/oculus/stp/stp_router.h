/* (c) Facebook, Inc. and its affiliates. Confidential and proprietary. */
#pragma once

#include "stp_master.h"
#define STP_ROUTER_NUM_HANDLES 2

/**
 * @brief stp_router handle
 *
 * Returned by open() and then used for the rest of the APIs
 */
#define stp_router_handle uint8_t

struct stp_router_open_t {
	uint32_t client_id;
	uint8_t *rx_buffer;
	uint32_t rx_buffer_size;
	struct stp_slave_notification_table *notification_table;
};

/**
 * @brief stp_router errors
 *
 * Errors returns by stp_router APIs
 */
enum {
	STP_ROUTER_ERROR_NONE = 0,
	STP_ROUTER_SUCCESS = 0,

	STP_ROUTER_ERROR_INVALID_METADATA = STP_ERROR_NEXT_ERROR,
};

/* STP Router callbacks. Defined by the client */
struct stp_router_wait_signal_table {
	/* wait until read can resume */
	int (*wait_read)(stp_router_handle);

	/* signal that read can resume */
	int (*signal_read)(stp_router_handle);

	/* check error - if true, that error should be returned to upper layer*/
	bool (*check_error)(int);
};

/* Used for STP initialization */
struct stp_router_init_t {
	struct stp_router_wait_signal_table *wait_signal;
};

/**
 * @brief Init stp_router
 *
 *
 * @return fw_router error code
 */
int32_t stp_router_init(struct stp_router_init_t *init);

/**
 * @brief Open stp_router
 *
 * Any client using fw_router needs to call this function first
 * to get a handle.
 *
 * @param handle Pointer to a handle returned by this function
 * @param obj Pointer to a stp_router_open_t object
 * @return stp_router error code
 */
int32_t stp_router_open(stp_router_handle *handle,
	struct stp_router_open_t *obj);

/**
 * @brief Close stp_router
 *
 * Called by stp_router client
 *
 * @param handle Pointer to the stp_router handle returned by this function
 * @return stp_router error code
 */
int32_t stp_router_close(stp_router_handle handle);

/**
 * @brief Write data to stp_router
 *
 * Write some data to stp_router
 *
 * @param handle Handle for stp_router
 * @param buffer Pointer to the data to write
 * @param size Size of data to write
 * @return stp_router error code
 */
int32_t stp_router_write(stp_router_handle handle, const uint8_t *buffer,
	uint32_t size, uint32_t *data_size);

/**
 * @brief Read data from stp_router in non-blocking mode
 *
 * Read some data from stp_router
 *
 * @param handle Handle for stp_router
 * @param buffer Pointer to the data to read
 * @param size Size of data to read
 * @return stp_router error code
 */
int32_t stp_router_read_nb(stp_router_handle handle, uint8_t *buffer,
	uint32_t buffer_size, uint32_t *data_size);

/**
 * @brief Read data from stp_router in blocking mode
 *
 * Read some data from stp_router
 *
 * @param handle Handle for stp_router
 * @param buffer Pointer to the data to read
 * @param size Size of data to read
 * @return stp_router error code
 */
int32_t stp_router_read(stp_router_handle handle, uint8_t *buffer,
	uint32_t size, uint32_t *data_size);

/**
 * @brief Transaction thread function,
 * should be called from related module thread
 *
 * Transaction function
 *
 * @return stp_router error code
 */
int32_t stp_router_transaction_thread(void);

/**
 * @brief Check for IO errors
 *
 * Check for IO errors
 *
 * @return stp_router error code
 */
int32_t stp_router_check_for_rw_errors(stp_router_handle handle);

/**
 * @brief Get the debug info
 *
 * Get the debug info
 *
 * @return NUmber of chars in buf
 */
int32_t stp_router_debug(char *buf, int size);

