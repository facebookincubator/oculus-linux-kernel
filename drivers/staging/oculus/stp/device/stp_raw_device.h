#ifndef STP_RAW_DEVICE_H
#define STP_RAW_DEVICE_H

#include <linux/kernel.h>

int stp_raw_dev_init(struct spi_device *spi);
int stp_raw_dev_remove(void);
bool stp_raw_get_spi_busy(void);
void stp_raw_set_spi_busy(bool busy);

#endif
