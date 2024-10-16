/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STP_CONTROLLER_COMMON_H
#define STP_CONTROLLER_COMMON_H

#include <stp/common/stp_os.h>
#include <stp/common/stp_pipeline.h>
#include <stp/common/stp_common.h>
#include <stp/controller/stp_controller.h>

#ifdef __cplusplus
extern "C" {
#endif

/* STP internal data */
struct stp_type {
	// transport interface (from upper layer)
	struct stp_controller_transport_table *transport;
	// handshake table
	struct stp_controller_handshake_table *handshake;
	// wait-signal table
	struct stp_controller_wait_signal_table *wait_signal;

	struct stp_channel channels[STP_TOTAL_NUM_CHANNELS];

	// TX buffer used for current packet
	uint8_t *tx_buffer;
	// RX buffer used for current packet
	uint8_t *rx_buffer;

	// current state of STP
	_Atomic uint32_t state;

	void (*callback_client)(int event);

	STP_LOCK_TYPE lock_notification;

	// info about the pending transactions
	// set by processing data transaction
	// used for following control transaction
	struct stp_pending pending;

	// info about lost comms
	size_t bad_crcs_in_a_row;

	_Atomic uint32_t device_channels_status;

	// the last status sent
	// if the current status is different, we need to start a transaction
	_Atomic uint32_t prev_channels_status;

	uint32_t last_tx_notification;

	bool pending_device_ready_signal;

	_Atomic bool device_ready;
	_Atomic bool has_data;
	_Atomic bool stop_thread;
	_Atomic bool suspend;
};

extern struct stp_type *_stp_controller_data;
// extern struct stp_debug_type _stp_debug;

/* Initialize the STP controller/device internal data */
void stp_controller_init_internal(
	struct stp_controller_transport_table *_transport);

void stp_controller_deinit_internal(void);

void stp_controller_init_transaction(void);

void stp_controller_process_control_transaction(bool *valid_packet);

void stp_controller_init_pending(void);

void stp_controller_prepare_tx_packet_data(uint8_t channel, uint8_t *buffer,
					   struct stp_pending_tx *tx);

void stp_controller_process_data_transaction(struct stp_pending_tx *tx);

void stp_controller_signal_has_data(void);

bool stp_controller_get_has_data(void);

void stp_controller_signal_device_ready(void);

bool stp_controller_get_device_ready(void);

void stp_controller_signal_stop_thread(void);

bool stp_controller_get_stop_thread(void);

void stp_controller_signal_suspend(void);

void stp_controller_set_notification(uint8_t channel, uint32_t notification);

void stp_controller_prepare_tx_notification(bool *do_transaction);

void stp_controller_disconnect(uint8_t channel);

void stp_controller_invalidate_channel(uint8_t channel);

void stp_controller_invalidate_session(void);

bool stp_controller_has_data_to_send(void);

extern uint32_t stp_mcu_ready_timer_expired_irq_missed_counter;

bool stp_controller_is_channel_valid(uint8_t channel);

void stp_controller_prepare_common_tx_transaction(void);

void stp_controller_rx_notification(uint8_t channel, uint32_t notification);

#ifdef __cplusplus
}
#endif

#endif
