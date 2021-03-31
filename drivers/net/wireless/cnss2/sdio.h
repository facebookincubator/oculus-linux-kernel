/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CNSS_SDIO_H
#define _CNSS_SDIO_H

#include "main.h"
#ifdef CONFIG_SDIO_QCN
#include <linux/qcn_sdio_al.h>

struct cnss_sdio_data {
	struct cnss_plat_data *plat_priv;
	struct sdio_al_client_handle *al_client_handle;
	struct cnss_sdio_wlan_driver *ops;
	struct sdio_device_id *device_id;
	void *client_priv;
};

int cnss_sdio_init(struct cnss_plat_data *plat_priv);
int cnss_sdio_deinit(struct cnss_plat_data *plat_priv);
int cnss_sdio_register_driver_hdlr(struct cnss_sdio_data *sdio_info,
				   void *data);
int cnss_sdio_unregister_driver_hdlr(struct cnss_sdio_data *sdio_info);
int cnss_sdio_dev_powerup(struct cnss_sdio_data *cnss_info);
int cnss_sdio_dev_shutdown(struct cnss_sdio_data *cnss_info);
int cnss_sdio_call_driver_probe(struct cnss_sdio_data *sdio_priv);
int cnss_sdio_call_driver_remove(struct cnss_sdio_data *sdio_priv);
void cnss_sdio_fw_boot_timeout_hdlr(void *bus_priv);
#else
inline int cnss_sdio_init(void *plat_priv)
{
	return -EINVAL;
}

inline int cnss_sdio_deinit(void *plat_priv)
{
	return -EINVAL;
}

inline int cnss_sdio_register_driver_hdlr(void *sdio_info,
				   void *data)
{
	return -EINVAL;
}

inline int cnss_sdio_unregister_driver_hdlr(void *sdio_info)
{
	return -EINVAL;
}

inline int cnss_sdio_dev_powerup(void *cnss_info)
{
	return -EINVAL;
}

inline int cnss_sdio_dev_shutdown(void *cnss_info)
{
	return -EINVAL;
}

inline int cnss_sdio_call_driver_probe(void *sdio_priv)
{
	return -EINVAL;
}

inline int cnss_sdio_call_driver_remove(void *sdio_priv)
{
	return -EINVAL;
}

inline void cnss_sdio_fw_boot_timeout_hdlr(void *bus_priv)
{
	/* no op */
}
#endif

#endif
