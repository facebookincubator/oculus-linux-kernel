#ifndef _SPI_FASTPATH_H
#define _SPI_FASTPATH_H

#include <linux/spi/spi.h>

int spi_fastpath_transfer(struct spi_device* spi, struct spi_message* msg);

#endif
