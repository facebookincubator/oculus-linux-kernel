/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SYNCBOSS_SWD_NRF5340_OPS_H
#define SYNCBOSS_SWD_NRF5340_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int syncboss_swd_nrf5340_preswd(struct device *dev);
int syncboss_swd_nrf5340_chip_erase(struct device *dev);
int syncboss_swd_nrf5340_erase_app(struct device *dev);
int syncboss_swd_nrf5340_erase_net(struct device *dev);
bool syncboss_swd_nrf5340_page_is_erased_app(struct device *dev, u32 page);
bool syncboss_swd_nrf5340_page_is_erased_net(struct device *dev, u32 page);
int syncboss_swd_nrf5340_read(struct device *dev, int addr, u8 *dest,
		      size_t len);
int syncboss_swd_nrf5340_app_write_chunk(struct device *dev, int addr, const u8 *data,
			     size_t len);
int syncboss_swd_nrf5340_net_write_chunk(struct device *dev, int addr, const u8 *data,
			     size_t len);
size_t syncboss_nrf5340_get_app_write_chunk_size(struct device *dev);
size_t syncboss_nrf5340_get_net_write_chunk_size(struct device *dev);
int syncboss_swd_nrf5340_finalize(struct device *dev);

#endif
