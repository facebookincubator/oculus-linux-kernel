/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STP_DEVICE_H
#define STP_DEVICE_H

#include <linux/kernel.h>
#include <linux/spi/spi.h>

struct stp_channel_data {
	uint32_t channel;
	uint32_t tx_len_bytes;
	uint32_t rx_len_bytes;
	uint32_t priority;
};

int stp_create_device(struct device *dev);

// Removes all associated channels
int stp_remove_device(struct device *dev);

int stp_create_channel(struct stp_channel_data *const data);
int stp_release_channel(uint8_t channel);
int stp_remove_channel(uint8_t channel);

void stp_channel_signal_write(uint8_t channel);
int stp_channel_wait_write(uint8_t channel);
void stp_channel_signal_read(uint8_t channel);
int stp_channel_wait_read(uint8_t channel);
int stp_channel_wait_read_timeout(uint8_t channel, unsigned long timeout);
void stp_channel_signal_fsync(uint8_t channel);
int stp_channel_wait_fsync(uint8_t channel);
void stp_channel_reset_fsync(uint8_t channel);
void stp_channel_signal_open(uint8_t channel);
int stp_channel_wait_open(uint8_t channel);

bool stp_get_spi_busy(void);
void stp_set_spi_busy(bool busy);

int stp_interrupt_all_channels(void);

struct stp_device_channel* stp_channel_open(int channel_num, bool nonblock);
int stp_channel_close(struct stp_device_channel *channel);
int stp_channel_write(struct stp_device_channel *channel,
		      const char *buf, size_t count, bool from_user);
int stp_channel_read(struct stp_device_channel *channel, char *buf,
		      size_t count, bool to_user);
bool stp_check_stale_channel(struct stp_device_channel *channel);
bool stp_get_device_ready(void);
void stp_dump_channel_state(const char *reason);
int stp_channel_connected(uint32_t channel, uint32_t *connected);
int stp_channel_rx_filled(uint32_t channel, uint32_t *rx_data_avail);
int stp_protocol_synced(uint32_t *synced);

#endif
