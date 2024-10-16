// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/component.h>

#include "cam_debug_util.h"
#include "cam_hw_intf.h"
#include "cam_isp_hw.h"
#include "cam_vfe_top.h"
#include "cam_vfe_core.h"
#include "cam_vfe_top_ver4.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_vfe_lite65x.h"
#include "cam_rpmsg.h"

#define CAM_VIRT_CSID_DRV_NAME "virtual-ife"
#define CAM_VIRT_VFE_WM_CFG    0x10001

extern struct cam_isp_hw_intf_data cam_vfe_hw_list[CAM_VFE_HW_NUM_MAX];

struct cam_virt_vfe_top {
	int first_line;
	int last_line;
	int horizontal_bin;
	int qcfa_bin;
	int vbi_value;
};

struct cam_virt_vfe_hw_core_info {
	struct cam_vfe_hw_info             *vfe_hw_info;
	atomic_t                            start_count;
	struct cam_vfe_top                 *vfe_top;
	struct cam_vfe_bus                 *vfe_bus;
};

struct cam_virt_vfe_top_ver4_priv {
	struct cam_vfe_top_ver4_common_data common_data;
	struct cam_vfe_top_priv_common      top_common;
};

int __translate_res_id(int res_id)
{
	switch(res_id) {
		case CAM_VFE_BUS_VER3_VFE_OUT_RDI0:
			return CAM_ISP_IFE_OUT_RES_RDI_0;
		case CAM_VFE_BUS_VER3_VFE_OUT_RDI1:
			return CAM_ISP_IFE_OUT_RES_RDI_1;
		case CAM_VFE_BUS_VER3_VFE_OUT_RDI2:
			return CAM_ISP_IFE_OUT_RES_RDI_2;
		case CAM_VFE_BUS_VER3_VFE_OUT_RDI3:
			return CAM_ISP_IFE_OUT_RES_RDI_3;
		case CAM_VFE_BUS_VER3_VFE_OUT_STATS_BG:
			return CAM_ISP_IFE_LITE_OUT_RES_STATS_BG;
		case CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW:
			return CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW;
		case CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW1:
			return CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW1;
		case CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW2:
			return CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW2;
		case CAM_VFE_BUS_VER3_VFE_OUT_STATS_LITE_BHIST:
			return CAM_ISP_IFE_LITE_OUT_RES_STATS_BHIST;
		default:
			CAM_ERR(CAM_ISP, "Unknown type %d", res_id);
	}
	return -1;
}

int cam_virt_vfe_populate_out_ports(void *hw_priv, void *cmd, uint32_t arg_size)
{
	int rc = 0, i, *num_out;
	struct cam_virt_process_cmd          *process_cmd;
	struct cam_virt_populate_out_args    *out_args;
	struct cam_isp_resource_node         *isp_res;
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data;
	struct cam_isp_resource_node         *wm_res;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_rsrc_data;
	struct cam_rpmsg_out_port *out;
	uint32_t *buf, *idx;

	if (!hw_priv || !cmd ||
		(arg_size != sizeof(struct cam_virt_process_cmd))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	process_cmd = (struct cam_virt_process_cmd *)cmd;
	out_args = (struct cam_virt_populate_out_args *)process_cmd->args;
	CAM_DBG(CAM_ISP, "num_res %d", out_args->num_res);
	buf = process_cmd->pkt;
	idx = process_cmd->off;
	num_out = &buf[(*idx)++];
	out = (struct cam_rpmsg_out_port *)&buf[*idx];
	for (i = 0; i < out_args->num_res; i++, (*num_out)++) {
		isp_res = out_args->res[i];
		rsrc_data = isp_res->res_priv;
		wm_res = &rsrc_data->wm_res[0];
		wm_rsrc_data = wm_res->res_priv;
		CAM_DBG(CAM_ISP, "res_id %d width %d height %d format %d",
				__translate_res_id(wm_res->res_id),
				wm_rsrc_data->acquired_width, wm_rsrc_data->acquired_height,
				wm_rsrc_data->format);
		out[i].type = __translate_res_id(wm_res->res_id);
		out[i].width = wm_rsrc_data->acquired_width;
		out[i].height = wm_rsrc_data->acquired_height;
		out[i].format = wm_rsrc_data->format;
	}
	*idx += ((*num_out * sizeof(struct cam_rpmsg_out_port)) / sizeof(uint32_t));

	return rc;
}

int cam_virt_vfe_populate_wm(
	struct cam_isp_resource_node      *wm_res,
	uint32_t                          *reg_payload,
	uint32_t                          *idx)
{
	int rc = 0;
	const uint32_t enable_debug_status_1 = 11 << 8;
	struct cam_vfe_bus_ver3_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_vfe_bus_ver3_common_data        *common_data =
		rsrc_data->common_data;
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;

	ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)
		rsrc_data->hw_regs->ubwc_regs;

	CAM_ADD_REG_VAL_PAIR(reg_payload, *idx, rsrc_data->hw_regs->burst_limit, 0xf);
	CAM_ADD_REG_VAL_PAIR(reg_payload, *idx, rsrc_data->hw_regs->image_cfg_0,
		(rsrc_data->height << 16) | rsrc_data->width);
	CAM_ADD_REG_VAL_PAIR(reg_payload, *idx, rsrc_data->hw_regs->packer_cfg,
		rsrc_data->pack_fmt);

	/* enable ubwc if needed*/
	if (rsrc_data->en_ubwc) {
		if (!ubwc_regs) {
			CAM_ERR(CAM_ISP,
				"ubwc_regs is NULL, WM:%d en_ubwc:%d",
				rsrc_data->index, rsrc_data->en_ubwc);
			return -EINVAL;
		}
		CAM_ADD_REG_VAL_PAIR(reg_payload, *idx, ubwc_regs->mode_cfg, 0x1);
	}

	/* Validate for debugfs and mmu reg info for targets that don't list it */
	if (!(common_data->disable_mmu_prefetch) &&
		(rsrc_data->hw_regs->mmu_prefetch_cfg)) {
		CAM_ADD_REG_VAL_PAIR(reg_payload, *idx, rsrc_data->hw_regs->mmu_prefetch_cfg,
			0x1);
		CAM_ADD_REG_VAL_PAIR(reg_payload, *idx,
			rsrc_data->hw_regs->mmu_prefetch_max_offset, 0xFFFFFFFF);
	}
	/*
	 * Enable WM and configure WM mode to frame based, since slave
	 * works in frame based mode only.
	 */
	CAM_ADD_REG_VAL_PAIR(reg_payload, *idx, rsrc_data->hw_regs->cfg, CAM_VIRT_VFE_WM_CFG);

	/* Enable constraint error detection */
	CAM_ADD_REG_VAL_PAIR(reg_payload, *idx,
		rsrc_data->hw_regs->debug_status_cfg, enable_debug_status_1);

	return rc;
}

int cam_virt_vfe_populate_out(
	struct cam_isp_resource_node      *vfe_out,
	uint32_t                          *reg_payload,
	uint32_t                          *idx)
{
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;
	int rc = 0, i;

	rsrc_data = vfe_out->res_priv;

	for (i = 0; i < rsrc_data->num_wm; i++) {
		rc = cam_virt_vfe_populate_wm(&rsrc_data->wm_res[i], reg_payload, idx);
	}

	return rc;
}

int cam_virt_vfe_populate_top(
	struct cam_virt_vfe_top_ver4_priv      *top_priv,
	struct cam_isp_resource_node      *vfe_res,
	uint32_t                          *reg_payload,
	uint32_t                          *idx)
{
	struct cam_vfe_mux_ver4_data            *rsrc_data;
	int rc = 0;

	if (!top_priv || !vfe_res) {
		CAM_ERR(CAM_ISP, "Error, Invalid input arguments");
		return -EINVAL;
	}

	rsrc_data = (struct cam_vfe_mux_ver4_data *)vfe_res->res_priv;

	CAM_ADD_REG_VAL_PAIR(reg_payload, *idx, rsrc_data->common_reg->top_debug_cfg,
			rsrc_data->reg_data->top_debug_cfg_en);

	return rc;

}

int cam_virt_vfe_populate_regs(void *hw_priv, void *cmd, uint32_t arg_size)
{

	struct cam_virt_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_virt_process_cmd       *hw_regs;
	struct cam_isp_resource_node      *isp_res;
	int                                rc = 0;
	uint32_t                          *reg_payload = NULL, *size_ptr = NULL;
	uint32_t                          *idx = 0, old_idx;

	if (!hw_priv || !cmd ||
		(arg_size != sizeof(struct cam_virt_process_cmd))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_virt_vfe_hw_core_info *)vfe_hw->core_info;
	hw_regs = (struct cam_virt_process_cmd *)cmd;
	isp_res = (struct cam_isp_resource_node  *)hw_regs->args;
	reg_payload = hw_regs->pkt;
	idx = hw_regs->off;

	if (!reg_payload) {
		CAM_ERR(CAM_ISP, "Alloc failed");
		return -ENOMEM;
	}

	reg_payload[(*idx)++] = CAM_SLAVE_HW_VFE;
	size_ptr = &reg_payload[(*idx)++];
	/* leave 4 bytes for CDM Header */
	old_idx = (*idx)++;

	mutex_lock(&vfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_IN) {
		rc = cam_virt_vfe_populate_top(core_info->vfe_top->top_priv, isp_res,
			reg_payload, idx);

		if (rc)
			CAM_ERR(CAM_ISP, "Failed to start VFE IN");
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_OUT) {
		rc = cam_virt_vfe_populate_out(isp_res, reg_payload, idx);

		if (rc)
			CAM_ERR(CAM_ISP, "Failed to start VFE OUT");
	} else {
		CAM_ERR(CAM_ISP, "Invalid res type:%x", isp_res->res_type);
		rc = -EFAULT;
	}

	mutex_unlock(&vfe_hw->hw_mutex);

	*size_ptr = (*idx - old_idx) * 4; // size in bytes
	/* add cdm header in payload */
	CAM_CDM_SET_TYPE(size_ptr + 1, CAM_CDM_TYPE_REG_RAND);
	CAM_CDM_SET_SIZE(size_ptr + 1, ((*idx - old_idx) - 1) / 2);

	return rc;
}

int cam_virt_vfe_get_hw_caps(void *hw_priv, void *get_hw_cap_args, uint32_t arg_size)
{
	struct cam_vfe_hw_get_hw_cap   *vfe_cap_info = NULL;
	struct cam_hw_info                  *vfe_dev = hw_priv;
	struct cam_vfe_hw_core_info       *core_info = NULL;
	int rc = 0;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	core_info = (struct cam_vfe_hw_core_info *)vfe_dev->core_info;
	vfe_cap_info = get_hw_cap_args;

	if (core_info->vfe_top->hw_ops.get_hw_caps)
		core_info->vfe_top->hw_ops.get_hw_caps(
			core_info->vfe_top->top_priv,
			get_hw_cap_args, arg_size);

	vfe_cap_info->is_virtual = true;
	CAM_DBG(CAM_ISP, "Exit");

	return rc;
}

int cam_virt_vfe_init_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "Unimplemented");
	return -EINVAL;
}

int cam_virt_vfe_deinit_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "Unimplemented");
	return -EINVAL;
}

int cam_virt_vfe_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "Unimplemented");
	return -EINVAL;
}

int cam_virt_vfe_reserve(void *hw_priv, void *reserve_args, uint32_t arg_size)
{
	struct cam_virt_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info            *vfe_hw = hw_priv;
	struct cam_vfe_acquire_args            *acquire;
	int rc = 0;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_vfe_acquire_args))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_virt_vfe_hw_core_info *)vfe_hw->core_info;
	acquire = (struct cam_vfe_acquire_args   *)reserve_args;

	if (acquire->rsrc_type == CAM_ISP_RESOURCE_VFE_IN) {
		rc = core_info->vfe_top->hw_ops.reserve(
			core_info->vfe_top->top_priv,
			acquire, sizeof(*acquire));
	} else if (acquire->rsrc_type == CAM_ISP_RESOURCE_VFE_OUT) {
		rc = core_info->vfe_bus->hw_ops.reserve(
			core_info->vfe_bus->bus_priv, acquire,
			sizeof(*acquire));
		if (rc) {
			CAM_ERR(CAM_ISP, "failed to acq bus rc %d", rc);
		}
	} else {
		CAM_ERR(CAM_ISP, "reserve not implemented for type %d", acquire->rsrc_type);
		rc = -EINVAL;
	}

	CAM_DBG(CAM_ISP, "acq res type: %d", acquire->rsrc_type);
	return rc;
}

int cam_virt_vfe_release(void *hw_priv, void *release_args, uint32_t arg_size)
{
	struct cam_virt_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info            *vfe_hw = hw_priv;
	struct cam_isp_resource_node      *isp_res;
	int rc = -ENODEV;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_virt_vfe_hw_core_info *)vfe_hw->core_info;
	isp_res = (struct cam_isp_resource_node      *) release_args;

	mutex_lock(&vfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_IN)
		rc = core_info->vfe_top->hw_ops.release(
			core_info->vfe_top->top_priv, isp_res,
			sizeof(*isp_res));
	else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_OUT)
		rc = core_info->vfe_bus->hw_ops.release(
			core_info->vfe_bus->bus_priv, isp_res,
			sizeof(*isp_res));
	else
		CAM_ERR(CAM_ISP, "Invalid res type:%d", isp_res->res_type);

	mutex_unlock(&vfe_hw->hw_mutex);

	CAM_DBG(CAM_ISP, "virt_vfe release rc %d res_type %d", rc, isp_res->res_type);

	return rc;
}

int cam_virt_vfe_start(void *hw_priv, void *start_args, uint32_t arg_size)
{
	CAM_DBG(CAM_ISP, "return success");
	return 0;
}

int cam_virt_vfe_stop(void *hw_priv, void *stop_args, uint32_t arg_size)
{
	CAM_DBG(CAM_ISP, "return success");
	return 0;
}

int cam_virt_vfe_read(void *hw_priv, void *read_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "Unimplemented");
	return -EINVAL;
}

int cam_virt_vfe_write(void *hw_priv, void *write_args, uint32_t arg_size)
{
	CAM_ERR(CAM_ISP, "Unimplemented");
	return -EINVAL;
}

int cam_virt_vfe_process_cmd(void *hw_priv, uint32_t cmd_type, void *cmd_args,
	uint32_t arg_size)
{
	int rc = -EINVAL;

	CAM_DBG(CAM_ISP, "cmd_type %d", cmd_type);
	switch (cmd_type) {
	case CAM_ISP_VIRT_POPULATE_REGS:
		rc = cam_virt_vfe_populate_regs(hw_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_VIRT_POPULATE_OUT_PORTS:
		rc = cam_virt_vfe_populate_out_ports(hw_priv, cmd_args, arg_size);
		break;
	}

	return rc;
}

static int cam_virt_vfe_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info                *vfe_hw = NULL;
	struct cam_hw_intf                *vfe_hw_intf = NULL;
	const struct of_device_id         *match_dev = NULL;
	int                                rc = 0i, vfe_dev_idx;
	struct cam_virt_vfe_hw_core_info       *core_info = NULL;
	struct cam_vfe_hw_info            *hw_info = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	vfe_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!vfe_hw_intf) {
		return -ENOMEM;
	}

	vfe_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!vfe_hw) {
		rc = -ENOMEM;
		goto free_vfe_hw_intf;
	}

	/* get vfe hw index */
	of_property_read_u32(pdev->dev.of_node, "cell-index", &vfe_dev_idx);

	/* get vfe hw information */
	vfe_hw->soc_info.pdev = pdev;
	vfe_hw->soc_info.dev = &pdev->dev;
	vfe_hw->soc_info.dev_name = pdev->name;
	vfe_hw_intf->hw_priv = vfe_hw;
	vfe_hw_intf->hw_ops.get_hw_caps = cam_virt_vfe_get_hw_caps;
	vfe_hw_intf->hw_ops.init = cam_virt_vfe_init_hw;
	vfe_hw_intf->hw_ops.deinit = cam_virt_vfe_deinit_hw;
	vfe_hw_intf->hw_ops.reset = cam_virt_vfe_reset;
	vfe_hw_intf->hw_ops.reserve = cam_virt_vfe_reserve;
	vfe_hw_intf->hw_ops.release = cam_virt_vfe_release;
	vfe_hw_intf->hw_ops.start = cam_virt_vfe_start;
	vfe_hw_intf->hw_ops.stop = cam_virt_vfe_stop;
	vfe_hw_intf->hw_ops.read = cam_virt_vfe_read;
	vfe_hw_intf->hw_ops.write = cam_virt_vfe_write;
	vfe_hw_intf->hw_ops.process_cmd = cam_virt_vfe_process_cmd;
	vfe_hw_intf->hw_type = CAM_ISP_HW_TYPE_VIFE;
	vfe_hw_intf->hw_idx = vfe_dev_idx;

	CAM_DBG(CAM_ISP, "VFE component bind, type %d index %d",
		vfe_hw_intf->hw_type, vfe_hw_intf->hw_idx);
	platform_set_drvdata(pdev, vfe_hw_intf);

	vfe_hw->core_info = kzalloc(sizeof(struct cam_virt_vfe_hw_core_info),
		GFP_KERNEL);
	if (!vfe_hw->core_info) {
		CAM_DBG(CAM_ISP, "Failed to alloc for core");
		rc = -ENOMEM;
		goto free_vfe_hw;
	}
	core_info = (struct cam_virt_vfe_hw_core_info *)vfe_hw->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "Of_match Failed");
		rc = -EINVAL;
		goto free_core_info;
	}
	hw_info = (struct cam_vfe_hw_info *)match_dev->data;
	core_info->vfe_hw_info = hw_info;

	rc = cam_vfe_top_init(hw_info->top_version, NULL, vfe_hw_intf,
			hw_info->top_hw_info, NULL, &core_info->vfe_top);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error, cam_vfe_top_init failed rc = %d", rc);
		goto free_core_info;
	}

	rc = cam_vfe_bus_init(hw_info->bus_version, BUS_TYPE_WR,
		NULL, vfe_hw_intf, hw_info->bus_hw_info,
		NULL, &core_info->vfe_bus);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error, cam_vfe_bus_init failed rc = %d", rc);
		goto deinit_top;
	}

	vfe_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&vfe_hw->hw_mutex);
	spin_lock_init(&vfe_hw->hw_lock);
	init_completion(&vfe_hw->hw_complete);

	if (vfe_hw_intf->hw_idx < CAM_VFE_HW_NUM_MAX)
		cam_vfe_hw_list[vfe_hw_intf->hw_idx].hw_intf = vfe_hw_intf;
	else {
		CAM_ERR(CAM_ISP, "VVFE index is more than max supported index hw_idx =%d max index",
			vfe_hw_intf->hw_idx, CAM_VFE_HW_NUM_MAX);
		rc = -EINVAL;
		goto deinit_bus;
	}

	CAM_DBG(CAM_ISP, "VVFE:%d component bound successfully",
		vfe_hw_intf->hw_idx);
	return rc;
deinit_bus:
	cam_vfe_bus_deinit(hw_info->bus_version, &core_info->vfe_bus);
deinit_top:
	cam_vfe_top_deinit(hw_info->top_version, &core_info->vfe_top);
free_core_info:
	kfree(vfe_hw->core_info);
free_vfe_hw:
	kfree(vfe_hw);
free_vfe_hw_intf:
	kfree(vfe_hw_intf);
	return rc;
}

static void cam_virt_vfe_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info                        *vfe_hw = NULL;
	struct cam_hw_intf                        *vfe_hw_intf = NULL;
	struct cam_virt_vfe_hw_core_info	  *core_info = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	vfe_hw_intf = platform_get_drvdata(pdev);
	if (!vfe_hw_intf) {
		CAM_ERR(CAM_ISP, "Error! No data in pdev");
		return;
	}

	CAM_DBG(CAM_ISP, "VFE component unbind, type %d index %d",
		vfe_hw_intf->hw_type, vfe_hw_intf->hw_idx);


	vfe_hw = vfe_hw_intf->hw_priv;
	if (!vfe_hw) {
		CAM_ERR(CAM_ISP, "Error! HW data is NULL");
		goto free_vfe_hw_intf;
	}

	core_info = (struct cam_virt_vfe_hw_core_info *)vfe_hw->core_info;
	if (!core_info) {
		CAM_ERR(CAM_ISP, "Error! core data NULL");
	}

	kfree(vfe_hw->core_info);

	mutex_destroy(&vfe_hw->hw_mutex);
	kfree(vfe_hw);

	CAM_DBG(CAM_ISP, "VFE%d component unbound", vfe_hw_intf->hw_idx);

free_vfe_hw_intf:
	kfree(vfe_hw_intf);
}

const static struct component_ops cam_virt_vfe_component_ops = {
	.bind = cam_virt_vfe_component_bind,
	.unbind = cam_virt_vfe_component_unbind,
};

int cam_virt_vfe_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "Adding VFE component");
	rc = component_add(&pdev->dev, &cam_virt_vfe_component_ops);
	if (rc)
		CAM_ERR(CAM_ISP, "failed to add component rc: %d", rc);

	return rc;
}

int cam_virt_vfe_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_virt_vfe_component_ops);
	return 0;
}

static const struct of_device_id cam_virt_vfe_dt_match[] = {
	{
		.compatible = "qcom,virt_vfe",
		.data = &cam_vfe_lite65x_hw_info,
	},
	{},
};

MODULE_DEVICE_TABLE(of, cam_virt_vfe_dt_match);

struct platform_driver cam_virt_vfe_driver = {
	.probe = cam_virt_vfe_probe,
	.remove = cam_virt_vfe_remove,
	.driver = {
		.name = "cam_virt_vfe",
		.owner = THIS_MODULE,
		.of_match_table = cam_virt_vfe_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_virt_vfe_init_module(void)
{
	return platform_driver_register(&cam_virt_vfe_driver);
}


void cam_virt_vfe_exit_module(void)
{
	platform_driver_unregister(&cam_virt_vfe_driver);
}

MODULE_DESCRIPTION("CAM VIRT VFE driver");
MODULE_LICENSE("GPL v2");
