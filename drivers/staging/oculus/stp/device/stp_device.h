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
int stp_remove_channel(uint8_t channel);

void stp_channel_signal_write(uint8_t channel);
int stp_channel_wait_write(uint8_t channel);
void stp_channel_signal_read(uint8_t channel);
int stp_channel_wait_read(uint8_t channel);
void stp_channel_signal_fsync(uint8_t channel);
int stp_channel_wait_fsync(uint8_t channel);
void stp_channel_reset_fsync(uint8_t channel);
void stp_channel_signal_open(uint8_t channel);
int stp_channel_wait_open(uint8_t channel);

#endif
