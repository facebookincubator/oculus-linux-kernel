/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SYNCBOSS_SWD_NRF54H20_OPS_H
#define SYNCBOSS_SWD_NRF54H20_OPS_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int syncboss_swd_nrf54h20_chip_erase(struct device *dev);
int syncboss_swd_nrf54h20_finalize(struct device *dev);

size_t syncboss_swd_nrf54h20_get_app_write_chunk_size(struct device *dev);

size_t syncboss_swd_nrf54h20_get_net_write_chunk_size(struct device *dev);

int syncboss_swd_nrf54h20_target_erase(struct device *dev);

int syncboss_swd_nrf54h20_write_chunk(struct device *dev, int addr,
				      const u8 *data, size_t len);

int syncboss_swd_nrf54h20_read_part_number(struct device *dev, u32 *partnum);

int syncboss_swd_nrf54h20_read(struct device *dev, int addr, u8 *const dest,
			       size_t len);

int syncboss_swd_nrf54h20_force_sec_dom_fw_version(struct device *dev,
						 const char *str);
#endif // SYNCBOSS_SWD_NRF54H20_OPS_H
