#ifndef SEACLIFF_BLU_SPI_H
#define SEACLIFF_BLU_SPI_H

#define MAGIC_NUM 'B'

/* ioctl used to set the backlight matrix */
#define BLU_SPI_SET_BACKLIGHT_IOCTL \
	_IOW(MAGIC_NUM, 1, struct blu_spi_backlight_matrix)

/* ioctl used to send debug messages over SPI */
#define BLU_SPI_DEBUG_MSG_IOCTL \
	_IOWR(MAGIC_NUM, 2, struct blu_spi_debug_message)

#define BLU_SPI_SET_CALIBRATION _IOW(MAGIC_NUM, 3, int)

/* ioctl used to get the backlight matrix */
#define BLU_SPI_GET_BACKLIGHT_IOCTL \
	_IOWR(MAGIC_NUM, 4, struct blu_spi_backlight_matrix)

/* struct passed to BLU_SPI_SET_BACKLIGHT_IOCTL which is
 * used to send the backlight matrix from the LD engine
 * in the compositor to the BLU driver
 */
struct blu_spi_backlight_matrix {
	const uint8_t *backlight_matrix;
	size_t matrix_size;
};

/* struct passed to BLU_SPI_DEBUG_MSG_IOCTL which is
 * used to send debug messages from userspace over SPI.
 * The max length of the tx buffer is 512 bytes,
 * the max length of the rx buffer is 8 bytes
 */
struct blu_spi_debug_message {
	const uint8_t *tx_buf;
	uint64_t rx_buf;
	uint32_t len;
};


#endif  // SEACLIFF_BLU_SPI_H
