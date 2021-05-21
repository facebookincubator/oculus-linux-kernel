#ifndef SEACLIFF_BLU_SPI_H
#define SEACLIFF_BLU_SPI_H

#include <linux/major.h>

/* ioctl used to get the backlight matrix */
#define BLU_SPI_SET_BACKLIGHT_IOCTL \
	_IOW(MISC_MAJOR, 1, struct blu_spi_backlight_matrix)

/* struct passed to BLU_SPI_SET_BACKLIGHT_IOCTL which is
 * used to send the backlight matrix from the LD engine
 * in the compositor to the BLU driver
 */
struct blu_spi_backlight_matrix {
	const uint8_t *backlight_matrix;
	size_t matrix_size;
};


#endif  // SEACLIFF_BLU_SPI_H
