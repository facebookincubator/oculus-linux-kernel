/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved. */

#ifndef _CNSS_BUS_H
#define _CNSS_BUS_H

#include "main.h"

#define QCA6174_VENDOR_ID		0x168C
#define QCA6174_DEVICE_ID		0x003E
#define QCA6174_REV_ID_OFFSET		0x08
#define QCA6174_REV3_VERSION		0x5020000
#define QCA6174_REV3_2_VERSION		0x5030000
#define QCA6290_VENDOR_ID		0x17CB
#define QCA6290_DEVICE_ID		0x1100
#define QCA6390_VENDOR_ID		0x17CB
#define QCA6390_DEVICE_ID		0x1101
#define QCA6490_VENDOR_ID		0x17CB
#define QCA6490_DEVICE_ID		0x1103

enum cnss_dev_bus_type cnss_get_dev_bus_type(struct device *dev);
enum cnss_dev_bus_type cnss_get_bus_type(unsigned long device_id);
void *cnss_bus_dev_to_bus_priv(struct device *dev);
struct cnss_plat_data *cnss_bus_dev_to_plat_priv(struct device *dev);
int cnss_bus_init(struct cnss_plat_data *plat_priv);
void cnss_bus_deinit(struct cnss_plat_data *plat_priv);
void cnss_bus_add_fw_prefix_name(struct cnss_plat_data *plat_priv,
				 char *prefix_name, char *name);
int cnss_bus_load_m3(struct cnss_plat_data *plat_priv);
int cnss_bus_alloc_fw_mem(struct cnss_plat_data *plat_priv);
int cnss_bus_alloc_qdss_mem(struct cnss_plat_data *plat_priv);
void cnss_bus_free_qdss_mem(struct cnss_plat_data *plat_priv);
u32 cnss_bus_get_wake_irq(struct cnss_plat_data *plat_priv);
int cnss_bus_force_fw_assert_hdlr(struct cnss_plat_data *plat_priv);
int cnss_bus_qmi_send_get(struct cnss_plat_data *plat_priv);
int cnss_bus_qmi_send_put(struct cnss_plat_data *plat_priv);
void cnss_bus_fw_boot_timeout_hdlr(struct timer_list *t);
void cnss_bus_collect_dump_info(struct cnss_plat_data *plat_priv,
				bool in_panic);
void cnss_bus_device_crashed(struct cnss_plat_data *plat_priv);
int cnss_bus_call_driver_probe(struct cnss_plat_data *plat_priv);
int cnss_bus_call_driver_remove(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_powerup(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_shutdown(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_crash_shutdown(struct cnss_plat_data *plat_priv);
int cnss_bus_dev_ramdump(struct cnss_plat_data *plat_priv);
int cnss_bus_register_driver_hdlr(struct cnss_plat_data *plat_priv, void *data);
int cnss_bus_unregister_driver_hdlr(struct cnss_plat_data *plat_priv);
int cnss_bus_call_driver_modem_status(struct cnss_plat_data *plat_priv,
				      int modem_current_status);
int cnss_bus_update_status(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_status status);
int cnss_bus_update_uevent(struct cnss_plat_data *plat_priv,
			   enum cnss_driver_status status, void *data);
int cnss_bus_is_device_down(struct cnss_plat_data *plat_priv);
int cnss_bus_check_link_status(struct cnss_plat_data *plat_priv);
int cnss_bus_recover_link_down(struct cnss_plat_data *plat_priv);
int cnss_bus_debug_reg_read(struct cnss_plat_data *plat_priv, u32 offset,
			    u32 *val);
int cnss_bus_debug_reg_write(struct cnss_plat_data *plat_priv, u32 offset,
			     u32 val);
int cnss_bus_get_iova(struct cnss_plat_data *plat_priv, u64 *addr, u64 *size);
int cnss_bus_get_iova_ipa(struct cnss_plat_data *plat_priv, u64 *addr,
			  u64 *size);
#endif /* _CNSS_BUS_H */
