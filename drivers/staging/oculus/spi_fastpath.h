#pragma once

#include <linux/spi/spi.h>

int spi_fastpath_init(struct spi_device* spi);

int spi_fastpath_deinit(struct spi_device* spi);

int spi_fastpath_transfer(struct spi_device* spi, struct spi_message* msg);
