/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SYNCBOSS_SWD_OPS_NRF52XXX_H
#define SYNCBOSS_SWD_OPS_NRF52XXX_H

#include <linux/device.h>
#include <linux/module.h>

#include "swd.h"

int syncboss_swd_nrf52xxx_provisioning_read(struct device *dev, int addr, u8 *data, size_t len);
int syncboss_swd_nrf52xxx_provisioning_write(struct device *dev, int addr, u8 *data, size_t len);

int syncboss_swd_nrf52xxx_erase_app(struct device *dev);
int syncboss_swd_nrf52xxx_chip_erase(struct device *dev);
int syncboss_swd_nrf52xxx_write_chunk(struct device *dev, int addr, const u8 *data,
			     size_t len);
size_t syncboss_get_write_chunk_size(struct device *dev);
int syncboss_swd_nrf52xxx_read(struct device *dev, int addr, u8 *dest,
			    size_t len);
bool syncboss_swd_nrf52xxx_page_is_erased(struct device *dev, u32 page);
int syncboss_swd_nrf52xxx_finalize(struct device *dev);

int syncboss_swd_nrf52832_prepare(struct device *dev);

#endif
