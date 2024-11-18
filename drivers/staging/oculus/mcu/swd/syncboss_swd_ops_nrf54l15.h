/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SYNCBOSS_SWD_NRF54L15_OPS_H
#define SYNCBOSS_SWD_NRF54L15_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int syncboss_swd_nrf54l15_provisioning_read(struct device *dev, int addr, u8 *data, size_t len);
int syncboss_swd_nrf54l15_provisioning_write(struct device *dev, int addr, u8 *data, size_t len);
int syncboss_swd_nrf54l15_erase_app(struct device *dev);
bool syncboss_swd_nrf54l15_page_is_erased(struct device *dev, u32 page);
int syncboss_swd_nrf54l15_chip_erase(struct device *dev);
size_t syncboss_swd_nrf54l15_get_write_chunk_size(struct device *dev);
int syncboss_swd_nrf54l15_write_chunk(struct device *dev, int addr,
				     const u8 *data, size_t len);
int syncboss_swd_nrf54l15_read(struct device *dev, int addr, u8 *dest, size_t len);

#endif // SYNCBOSS_SWD_NRF54L15_OPS_H
