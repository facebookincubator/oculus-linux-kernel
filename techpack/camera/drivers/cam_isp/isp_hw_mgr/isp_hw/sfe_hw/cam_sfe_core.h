/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_SFE_CORE_H_
#define _CAM_SFE_CORE_H_

#include <linux/spinlock_types.h>
#include "cam_hw_intf.h"
#include "cam_sfe_hw_intf.h"
#include "cam_sfe_bus.h"
#include "cam_sfe_top.h"

#define CAM_SFE_EVT_MAX                        256

struct cam_sfe_hw_info {
	struct cam_irq_controller_reg_info *irq_reg_info;

	uint32_t                            bus_wr_version;
	void                               *bus_wr_hw_info;

	uint32_t                            bus_rd_version;
	void                               *bus_rd_hw_info;

	uint32_t                            top_version;
	void                               *top_hw_info;
};

struct cam_sfe_hw_core_info {
	struct cam_sfe_hw_info             *sfe_hw_info;
	struct cam_sfe_top                 *sfe_top;
	struct cam_sfe_bus                 *sfe_bus_wr;
	struct cam_sfe_bus                 *sfe_bus_rd;
	void                               *sfe_irq_controller;
	void                               *worker_info;
	spinlock_t                          spin_lock;
};

/*
 * Debug config to enable interrupts and debug features
 */
#define SFE_DEBUG_ENABLE_SOF_EOF_IRQ              BIT(0)
#define SFE_DEBUG_ENABLE_SENSOR_DIAG_INFO         BIT(1)
#define SFE_DEBUG_ENABLE_FRAME_COUNTER            BIT(2)
#define SFE_DEBUG_ENABLE_RD_DONE_IRQ              BIT(3)
#define SFE_DEBUG_DISABLE_MMU_PREFETCH            BIT(4)
#define SFE_DEBUG_ENABLE_TESTBUS1                 BIT(8)
#define SFE_DEBUG_ENABLE_TESTBUS2                 BIT(9)

/* Reserve 4 bits for future test-busses in debug config */
#define SFE_DEBUG_ENABLE_TESTBUS_RESERVED1        BIT(10)
#define SFE_DEBUG_ENABLE_TESTBUS_RESERVED2        BIT(11)
#define SFE_DEBUG_ENABLE_TESTBUS_RESERVED3        BIT(12)
#define SFE_DEBUG_ENABLE_TESTBUS_RESERVED4        BIT(13)

int cam_sfe_get_hw_caps(void *device_priv,
	void *get_hw_cap_args, uint32_t arg_size);
int cam_sfe_reset(void *device_priv,
	void *reset_core_args, uint32_t arg_size);
int cam_sfe_read(void *device_priv,
	void *read_args, uint32_t arg_size);
int cam_sfe_write(void *device_priv,
	void *write_args, uint32_t arg_size);
int cam_sfe_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size);
int cam_sfe_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size);
int cam_sfe_reserve(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_sfe_release(void *device_priv,
	void *reserve_args, uint32_t arg_size);
int cam_sfe_start(void *device_priv,
	void *start_args, uint32_t arg_size);
int cam_sfe_stop(void *device_priv,
	void *stop_args, uint32_t arg_size);
int cam_sfe_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size);

irqreturn_t cam_sfe_irq(int irq_num, void *data);

int cam_sfe_core_init(struct cam_sfe_hw_core_info *core_info,
	struct cam_hw_soc_info             *soc_info,
	struct cam_hw_intf                 *hw_intf,
	struct cam_sfe_hw_info             *sfe_hw_info);

int cam_sfe_core_deinit(struct cam_sfe_hw_core_info *core_info,
	struct cam_sfe_hw_info             *sfe_hw_info);

#endif /* _CAM_SFE_CORE_H_ */
