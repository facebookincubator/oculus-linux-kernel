/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __STP_DRIVER_H__
#define __STP_DRIVER_H__

#define SPI_STP_DRV_LOADED		1
#define SPI_STP_DRV_REMOVING		2
#define SPI_STP_CTRL_INIT		3

int register_spi_stp_notifier(struct notifier_block *nb);
void unregister_spi_stp_notifier(struct notifier_block *nb);

#endif
