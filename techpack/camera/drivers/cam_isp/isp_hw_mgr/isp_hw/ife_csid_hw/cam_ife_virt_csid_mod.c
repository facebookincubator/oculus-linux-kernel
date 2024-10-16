// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/component.h>

#include "cam_debug_util.h"
#include "cam_hw_intf.h"
#include "cam_isp_hw.h"
#include "cam_ife_virt_csid_mod.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_rpmsg.h"

#include "cam_ife_csid_hw_ver2.h"
#include "cam_ife_csid_lite650.h"

#define CAM_VIRT_CSID_DRV_NAME "virtual-csid"

#define RDI_ENABLE_SHIFT       31
#define DT_ID_SHIFT            27
#define CFG0_VC_SHIFT          22
#define CFG0_DT_SHIFT          16
#define DECODE_FORMAT_SHIFT    12
#define PIX_STORE_EN_SHIFT     3
#define RDI3_CFG0_OFF          0x800

#define START_CMD_SHIFT        0
#define RDI3_CTRL_OFF          0x804

#define PACKING_FORMAT_SHIFT    15
#define PLAIN_FORMAT_SHIFT      12
#define PLAIN_ALIGNMENT_SHIFT   11
#define CROP_V_EN_SHIFT         8
#define CROP_H_EN_SHIFT         7
#define DROP_V_EN_SHIFT         6
#define DROP_H_EN_SHIFT         5
#define TIMESTAMP_EN_SHIFT      4
#define TIMESTAMP_STB_SEL_SHIFT 0
#define RDI3_CFG1_OFF           0x810

#define MULTI_VCDT_DT_SHIFT    22
#define MULTI_VCDT_VC_SHIFT    17
#define MULTI_VCDT_EN_SHIFT    16
#define VCDT_CFGn_DT_SHIFT     6
#define VCDT_CFGn_VC_SHIFT     1
#define VCDT_CFGn_EN_SHIFT     0
#define RDI3_VCDT_CFG2_OFF     0x8F4
#define RDI3_VCDT_CFG3_OFF     0x8F8
#define RDI3_VCDT_CFG4_OFF     0x8FC
#define RDI3_VCDT_CFG5_OFF     0x868

#define DT1_SHIFT                7
#define VC1_SHIFT                2
#define CFG0_MULTI_VCDT_EN_SHIFT 0
#define RDI3_MULTI_VCDT_CFG0     0x80c

#define DECODE_FORMAT_MASK     0xF
#define PHY_TYPE_SEL_SHIFT     24
#define PHY_NUM_SEL_SHIFT      20
#define INPUT_SEL_SHIFT        4
#define NUM_ACTIVE_LANES_SHIFT 0
#define CSI2_RX_CFG0_OFF       0x200

#define EPD_MODE_SHIFT         8
#define VC_MODE_SHIFT          2
#define ECC_CORR_SHIFT         0
#define CSI2_RX_CFG1_OFF       0x204

int cam_ife_virt_csid_get_hw_caps(void *hw_priv, void *get_hw_cap_args, uint32_t arg_size)
{
	struct cam_ife_csid_hw_caps           *hw_caps;

	hw_caps = (struct cam_ife_csid_hw_caps *) get_hw_cap_args;
	hw_caps->num_rdis = 0;
	hw_caps->num_pix = 0;
	hw_caps->num_ppp = 0;
	hw_caps->global_reset_en = 1;
	hw_caps->rup_en = 1;
	hw_caps->is_lite = false;
	hw_caps->only_master_rup = 1;
	hw_caps->is_virt = true;
	return 0;
}

int cam_ife_virt_csid_init_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	WARN_ON_ONCE(1);
	CAM_ERR(CAM_ISP,  "Unimplemented");
	return 0;
}

int cam_ife_virt_csid_deinit_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	WARN_ON_ONCE(1);
	CAM_ERR(CAM_ISP,  "Unimplemented");
	return 0;
}

int cam_ife_virt_csid_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size)
{
	WARN_ON_ONCE(1);
	CAM_ERR(CAM_ISP,  "Unimplemented");
	return 0;
}

static int cam_ife_virt_csid_get_cid(struct cam_ife_csid_cid_data *cid_data,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{
	uint32_t  i;

	if (cid_data->cid_cnt == 0) {

		for (i = 0; i < reserve->in_port->num_valid_vc_dt; i++) {
			cid_data->vc_dt[i].vc = reserve->in_port->vc[i];
			cid_data->vc_dt[i].dt = reserve->in_port->dt[i];
			cid_data->vc_dt[i].valid = true;
		}

		cid_data->num_vc_dt = reserve->in_port->num_valid_vc_dt;
		return 0;
	}

	for (i = 0; i < reserve->in_port->num_valid_vc_dt; i++) {

		if (cid_data->vc_dt[i].vc == reserve->in_port->vc[i] &&
			cid_data->vc_dt[i].dt == reserve->in_port->dt[i])
			return 0;
	}

	return -EINVAL;
}

int cam_ife_virt_csid_cid_reserve(struct cam_ife_csid_cid_data *cid_data,
	uint32_t *cid_value,
	uint32_t hw_idx,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{
	int i, j, rc = 0;

	for (i = 0; i < CAM_IFE_CSID_CID_MAX; i++) {
		rc = cam_ife_virt_csid_get_cid(&cid_data[i], reserve);
		if (!rc)
			break;
	}

	if (i == CAM_IFE_CSID_CID_MAX) {
		for (j = 0; j < reserve->in_port->num_valid_vc_dt; j++) {
			CAM_ERR(CAM_ISP,
				"VCSID[%d] reserve fail vc[%d] dt[%d]",
				hw_idx, reserve->in_port->vc[j],
				reserve->in_port->dt[j]);
			return -EINVAL;
		}
	}

	cid_data[i].cid_cnt++;
	*cid_value = i;

	return 0;
}

static int cam_ife_virt_csid_config_rx(
	struct cam_ife_virt_csid        *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{
	csid_hw->rx_cfg.lane_cfg =
		reserve->in_port->lane_cfg;
	csid_hw->rx_cfg.lane_type =
		reserve->in_port->lane_type;
	csid_hw->rx_cfg.lane_num =
		reserve->in_port->lane_num;
	csid_hw->rx_cfg.dynamic_sensor_switch_en =
		reserve->in_port->dynamic_sensor_switch_en;
	if (reserve->in_port->epd_supported)
		csid_hw->rx_cfg.epd_supported = 1;
	csid_hw->rx_cfg.tpg_mux_sel = 0;
	csid_hw->rx_cfg.phy_sel =
		cam_ife_csid_get_phy_sel(reserve->in_port->res_type);

	if (csid_hw->rx_cfg.phy_sel < 0) {
		CAM_ERR(CAM_ISP, "Invalid phy sel for res %d",
			reserve->in_port->res_type);
		return -EINVAL;
	}
	CAM_DBG(CAM_ISP,
		"VCSID:%d Rx lane param: cfg:%u type:%u num:%u res:%u",
		csid_hw->hw_intf->hw_idx,
		reserve->in_port->lane_cfg, reserve->in_port->lane_type,
		reserve->in_port->lane_num, reserve->in_port->res_type);

	return 0;
}

static bool cam_ife_virt_csid_hw_need_unpack_mipi(
	struct cam_ife_virt_csid                     *csid_hw,
	struct cam_csid_hw_reserve_resource_args     *reserve,
	const struct cam_ife_csid_ver2_path_reg_info *path_reg,
	uint32_t                                      format)
{
	bool  need_unpack = false;

	switch (format) {
	case CAM_FORMAT_MIPI_RAW_8:
		need_unpack = (bool)(path_reg->capabilities & CAM_IFE_CSID_CAP_MIPI8_UNPACK);
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		need_unpack = (bool)(path_reg->capabilities & CAM_IFE_CSID_CAP_MIPI10_UNPACK);
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		need_unpack = (bool)(path_reg->capabilities & CAM_IFE_CSID_CAP_MIPI12_UNPACK);
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		need_unpack = (bool)(path_reg->capabilities & CAM_IFE_CSID_CAP_MIPI14_UNPACK);
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		need_unpack = (bool)(path_reg->capabilities & CAM_IFE_CSID_CAP_MIPI16_UNPACK);
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		need_unpack = (bool)(path_reg->capabilities & CAM_IFE_CSID_CAP_MIPI20_UNPACK);
		break;
	default:
		need_unpack = false;
		break;
	}

	CAM_DBG(CAM_ISP, "VCSID[%u], RDI_%u format %u need_unpack %u sfe_shdr %u",
		csid_hw->hw_intf->hw_idx, reserve->res_id, format, need_unpack,
		reserve->sfe_inline_shdr);

	return need_unpack;
}

static int cam_virt_ife_csid_decode_format1_validate(
	struct cam_ife_virt_csid *csid_hw,
	struct cam_isp_resource_node *res)
{
	int rc = 0;
	const struct cam_ife_csid_ver2_reg_info *csid_reg =
		(struct cam_ife_csid_ver2_reg_info *)csid_hw->core_info->csid_reg;
	struct cam_ife_csid_ver2_path_cfg *path_cfg =
		(struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	struct cam_ife_csid_cid_data *cid_data = &csid_hw->cid_data[path_cfg->cid];

	/* Validation is only required  for multi vc dt use case */
	if (!cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)
		return rc;

	if ((path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt ==
		csid_reg->cmn_reg->decode_format_payload_only) ||
		(path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt  ==
		 csid_reg->cmn_reg->decode_format_payload_only)) {
		if (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt !=
			path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt) {
			CAM_ERR(CAM_ISP,
				"CSID:%d decode_fmt %d decode_fmt1 %d mismatch",
				csid_hw->hw_intf->hw_idx,
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt,
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt);
			rc = -EINVAL;
			goto err;
		}
	}

	if ((cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc ==
		cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc) &&
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt ==
		cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt)) {
		if (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt !=
			path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt) {
			CAM_ERR(CAM_ISP,
				"CSID:%d Wrong multi VC-DT configuration",
					csid_hw->hw_intf->hw_idx);
			CAM_ERR(CAM_ISP,
				"fmt %d fmt1 %d vc %d vc1 %d dt %d dt1 %d",
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt,
				path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].decode_fmt,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].vc,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt,
				cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].dt);
			rc = -EINVAL;
			goto err;
		}
	}

	return rc;
err:
	CAM_ERR(CAM_ISP, "Invalid decode fmt1 cfg csid[%d] res [id %d name %s] rc %d",
		csid_hw->hw_intf->hw_idx, res->res_id, res->res_name, rc);
	return rc;
}


static int cam_ife_virt_csid_hw_config_path_data(
	struct cam_ife_virt_csid *csid_hw,
	struct cam_ife_csid_ver2_path_cfg *path_cfg,
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t cid)
{
	int rc = 0, i = 0;
	const struct cam_ife_csid_ver2_reg_info *csid_reg =
		(struct cam_ife_csid_ver2_reg_info *)csid_hw->core_info->csid_reg;
	struct cam_ife_csid_cid_data *cid_data = &csid_hw->cid_data[cid];
	struct cam_isp_resource_node *res = &csid_hw->path_res[reserve->res_id];
	const struct cam_ife_csid_ver2_path_reg_info  *path_reg = NULL;

	for (i = 0; i < reserve->in_port->num_valid_vc_dt; i++)
		path_cfg->in_format[i] = reserve->in_port->format[i];

	path_cfg->cid = cid;
	path_cfg->out_format = reserve->out_port->format;
	path_cfg->sync_mode = reserve->sync_mode;
	path_cfg->height  = reserve->in_port->height;
	path_cfg->start_line = reserve->in_port->line_start;
	path_cfg->end_line = reserve->in_port->line_stop;
	path_cfg->crop_enable = reserve->crop_enable;
	path_cfg->drop_enable = reserve->drop_enable;
	path_cfg->horizontal_bin = reserve->in_port->horizontal_bin;
	path_cfg->vertical_bin = reserve->in_port->vertical_bin;
	path_cfg->qcfa_bin = reserve->in_port->qcfa_bin;
	path_cfg->num_bytes_out = reserve->in_port->num_bytes_out;
	path_cfg->sec_evt_config.en_secondary_evt = reserve->sec_evt_config.en_secondary_evt;
	path_cfg->sec_evt_config.evt_type = reserve->sec_evt_config.evt_type;
	path_reg = csid_reg->path_reg[res->res_id];

	if (reserve->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
		path_cfg->start_pixel = reserve->in_port->left_start;
		path_cfg->end_pixel = reserve->in_port->left_stop;
		path_cfg->width  = reserve->in_port->left_width;

		if (reserve->res_id >= CAM_IFE_PIX_PATH_RES_RDI_0 &&
			reserve->res_id <= (CAM_IFE_PIX_PATH_RES_RDI_0 +
			CAM_IFE_CSID_RDI_MAX - 1)) {
			path_cfg->end_pixel = reserve->in_port->right_stop;
			path_cfg->width = path_cfg->end_pixel -
				path_cfg->start_pixel + 1;
		}
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d master:startpixel 0x%x endpixel:0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_pixel, path_cfg->end_pixel);
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d master:line start:0x%x line end:0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_line, path_cfg->end_line);
	} else if (reserve->sync_mode == CAM_ISP_HW_SYNC_SLAVE) {
		path_cfg->start_pixel = reserve->in_port->right_start;
		path_cfg->end_pixel = reserve->in_port->right_stop;
		path_cfg->width  = reserve->in_port->right_width;
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d slave:start:0x%x end:0x%x width 0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_pixel, path_cfg->end_pixel,
			path_cfg->width);
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d slave:line start:0x%x line end:0x%x",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			path_cfg->start_line, path_cfg->end_line);
	} else {
		path_cfg->width  = reserve->in_port->left_width;
		path_cfg->start_pixel = reserve->in_port->left_start;
		path_cfg->end_pixel = reserve->in_port->left_stop;
		CAM_DBG(CAM_ISP,
			"CSID:%d res:%d left width %d start: %d stop:%d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			reserve->in_port->left_width,
			reserve->in_port->left_start,
			reserve->in_port->left_stop);
	}

	switch (reserve->res_id) {
	case CAM_IFE_PIX_PATH_RES_RDI_0:
	case CAM_IFE_PIX_PATH_RES_RDI_1:
	case CAM_IFE_PIX_PATH_RES_RDI_2:
	case CAM_IFE_PIX_PATH_RES_RDI_3:
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		/*
		 * if csid gives unpacked out, packing needs to be done at
		 * WM side if needed, based on the format the decision is
		 * taken at WM side
		 */
		reserve->use_wm_pack = cam_ife_virt_csid_hw_need_unpack_mipi(csid_hw,
			reserve, path_reg, path_cfg->out_format);
		rc = cam_ife_csid_get_format_rdi(
			path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0],
			path_cfg->out_format,
			&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0],
			path_reg->mipi_pack_supported, reserve->use_wm_pack);
		if (rc)
			goto end;

		if (csid_reg->cmn_reg->decode_format1_supported &&
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)) {

			rc = cam_ife_csid_get_format_rdi(
				path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1],
				path_cfg->out_format,
				&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1],
				path_reg->mipi_pack_supported, reserve->use_wm_pack);
			if (rc)
				goto end;
		}
		break;
	case CAM_IFE_PIX_PATH_RES_IPP:
	case CAM_IFE_PIX_PATH_RES_PPP:
		rc = cam_ife_csid_get_format_ipp_ppp(
			path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0],
			&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0]);
		if (rc)
			goto end;

		if (csid_reg->cmn_reg->decode_format1_supported &&
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)) {

			rc = cam_ife_csid_get_format_ipp_ppp(
				path_cfg->in_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1],
				&path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_1]);
			if (rc)
				goto end;
		}
		break;
	default:
		rc = -EINVAL;
		CAM_ERR(CAM_ISP, "Invalid Res id %u", reserve->res_id);
		break;
	}

	if (csid_reg->cmn_reg->decode_format1_supported &&
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_1].valid)) {
		rc = cam_virt_ife_csid_decode_format1_validate(csid_hw, res);
		if (rc) {
			CAM_ERR(CAM_ISP, "VCSID[%d] res %d decode fmt1 validation failed",
				csid_hw->hw_intf->hw_idx, res);
			goto end;
		}
	}

end:
	return rc;
}

int cam_ife_virt_csid_hw_cfg(
	struct cam_ife_virt_csid *csid_hw,
	struct cam_ife_csid_ver2_path_cfg *path_cfg,
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t cid)
{
	int rc = 0;

	rc = cam_ife_virt_csid_config_rx(csid_hw, reserve);

	if (rc) {
		CAM_ERR(CAM_ISP, "VCSID[%d] rx config failed",
			csid_hw->hw_intf->hw_idx);
		return rc;
	}

	rc = cam_ife_virt_csid_hw_config_path_data(csid_hw, path_cfg,
		reserve, cid);
	if (rc) {
		CAM_ERR(CAM_ISP, "VCSID[%d] config path data failed",
				csid_hw->hw_intf->hw_idx);
		return rc;
	}

	return rc;
}


int cam_ife_virt_csid_reserve(void *hw_priv, void *reserve_args, uint32_t arg_size)
{
	struct cam_ife_virt_csid        *csid_hw;
	struct cam_hw_info              *hw_info;
	struct cam_isp_resource_node    *res = NULL;
	struct cam_csid_hw_reserve_resource_args  *reserve;
	struct cam_ife_csid_ver2_path_cfg    *path_cfg;
	uint32_t cid;
	int rc = 0;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_virt_csid *)hw_info->core_info;
	reserve = (struct cam_csid_hw_reserve_resource_args  *)reserve_args;

	res = &csid_hw->path_res[reserve->res_id];
	if (!res) {
		CAM_ERR(CAM_ISP, "resourse is null for id %d", reserve->res_id);
		return -EINVAL;
	}
	if (res->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		/**
		 * intentionally set as DBG log to since this log gets printed when hw manager
		 * checks if resource is available
		 */
		CAM_DBG(CAM_ISP, "VCSID %d Res_id %d state %d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			res->res_state);
		return -EBUSY;
	}

	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	if (!path_cfg) {
		CAM_ERR(CAM_ISP,
			"VCSID %d Unallocated Res_id %d state %d",
			csid_hw->hw_intf->hw_idx, reserve->res_id,
			res->res_state);
		return -EINVAL;
	}

	rc = cam_ife_virt_csid_cid_reserve(csid_hw->cid_data, &cid,
		csid_hw->hw_intf->hw_idx, reserve);

	if (rc) {
		CAM_ERR(CAM_ISP, "VCSID %d Res_id %d invalid cid %d",
			csid_hw->hw_intf->hw_idx, reserve->res_id, cid);
		return rc;
	}

	rc = cam_ife_virt_csid_hw_cfg(csid_hw, path_cfg,
		reserve, cid);
	if (rc) {
		CAM_ERR(CAM_ISP, "VCSID[%d] res %d hw_cfg fail",
			csid_hw->hw_intf->hw_idx, reserve->res_id);
		goto release;
	}

	reserve->node_res = res;
	csid_hw->flags.sfe_en = reserve->sfe_en;
	csid_hw->flags.offline_mode = reserve->is_offline;

	CAM_DBG(CAM_ISP, "VCSID[%u] Resource[id: %d cid %d",
		csid_hw->hw_intf->hw_idx, reserve->res_id, cid);
	return 0;
release:
	cam_ife_csid_cid_release(&csid_hw->cid_data[cid],
		csid_hw->hw_intf->hw_idx,
		path_cfg->cid);
	return rc;
}

int cam_ife_virt_csid_release(void *hw_priv, void *release_args, uint32_t arg_size)
{
	struct cam_ife_virt_csid        *csid_hw;
	struct cam_hw_info              *hw_info;
	struct cam_isp_resource_node    *res = NULL;
	struct cam_ife_csid_ver2_path_cfg    *path_cfg;
	int rc = 0;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_virt_csid *)hw_info->core_info;
	res = (struct cam_isp_resource_node *)release_args;

	if (res->res_type != CAM_ISP_RESOURCE_PIX_PATH) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		return -EINVAL;
	}

	mutex_lock(&csid_hw->hw_info->hw_mutex);

	if ((res->res_type == CAM_ISP_RESOURCE_PIX_PATH &&
		res->res_id >= CAM_IFE_PIX_PATH_RES_MAX)) {
		CAM_ERR(CAM_ISP, "CSID:%d Invalid res type:%d res id%d",
			csid_hw->hw_intf->hw_idx, res->res_type,
			res->res_id);
		rc = -EINVAL;
		goto end;
	}

	if ((res->res_state <= CAM_ISP_RESOURCE_STATE_AVAILABLE) ||
		(res->res_state >= CAM_ISP_RESOURCE_STATE_STREAMING)) {
		CAM_WARN(CAM_ISP,
			"CSID:%d res type:%d Res %d in state %d",
			csid_hw->hw_intf->hw_idx,
			res->res_type, res->res_id,
			res->res_state);
		goto end;
	}

	CAM_DBG(CAM_ISP, "CSID:%d res type :%d Resource [id:%d name:%s]",
		csid_hw->hw_intf->hw_idx, res->res_type,
		res->res_id, res->res_name);

	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;

	cam_ife_csid_cid_release(&csid_hw->cid_data[path_cfg->cid],
		csid_hw->hw_intf->hw_idx,
		path_cfg->cid);

	memset(path_cfg, 0, sizeof(*path_cfg));

	memset(&csid_hw->rx_cfg, 0,
		sizeof(struct cam_ife_csid_rx_cfg));

	res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
end:
	mutex_unlock(&csid_hw->hw_info->hw_mutex);
	return rc;
}

static int cam_ife_virt_csid_program_rdi_path (
	struct cam_ife_virt_csid    *csid_hw,
	struct cam_isp_resource_node   *res,
	uint32_t                       *rup_aup_mask,
	uint32_t                       *cdm_payload,
	uint32_t                       *idx)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver2_common_reg_info *cmn_reg = NULL;
	struct cam_ife_csid_cid_data *cid_data;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	uint32_t cfg0, cfg1 = 0, val = 0;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	cmn_reg = csid_reg->cmn_reg;
	path_reg = csid_reg->path_reg[res->res_id];
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	cid_data = &csid_hw->cid_data[path_cfg->cid];

	cfg0 = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt <<
		 	cmn_reg->decode_format_shift_val);

	if (csid_reg->cmn_reg->vfr_supported)
		cfg0 |= path_cfg->vfr_en << cmn_reg->vfr_en_shift_val;

	if (csid_reg->cmn_reg->frame_id_dec_supported)
		cfg0 |= path_cfg->frame_id_dec_en <<
			cmn_reg->frame_id_decode_en_shift_val;

	if (cmn_reg->timestamp_enabled_in_cfg0)
		cfg0 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	/* Enable the RDI path */
	cfg0 |= (1 << cmn_reg->path_en_shift_val);

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->cfg0_addr, cfg0);

	cfg1 = (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if (cmn_reg->drop_supported)
		cfg1 |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	if (path_reg->mipi_pack_supported)
		cfg1 |= path_cfg->
			path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].packing_fmt <<
			path_reg->packing_fmt_shift_val;

	cfg1 |= (path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].plain_fmt <<
			path_reg->plain_fmt_shift_val);

	if (!cmn_reg->timestamp_enabled_in_cfg0)
		cfg1 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	/* We use line smoothting only on RDI_0 in all usecases */
	if ((path_reg->capabilities &
		CAM_IFE_CSID_CAP_LINE_SMOOTHING_IN_RDI) &&
		(res->res_id == CAM_IFE_PIX_PATH_RES_RDI_0))
		cfg1 |= 1 << path_reg->pix_store_en_shift_val;

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->cfg1_addr, cfg1);

	/* set frame drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->frm_drop_period_addr, 1);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->frm_drop_pattern_addr, 0);

	/* set pxl drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->pix_drop_pattern_addr, 0);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->pix_drop_period_addr, 1);

	/* set line drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->line_drop_pattern_addr, 0);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->line_drop_period_addr, 1);

	if (path_reg->overflow_ctrl_en) {
		val = path_reg->overflow_ctrl_en |
			path_reg->overflow_ctrl_mode_val;
		CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->err_recovery_cfg0_addr, val);
	}

	*rup_aup_mask |= path_reg->rup_aup_mask;

	return 0;
}

static int cam_ife_virt_csid_program_ppp_path (
	struct cam_ife_virt_csid    *csid_hw,
	struct cam_isp_resource_node   *res,
	uint32_t                       *rup_aup_mask,
	uint32_t                       *cdm_payload,
	uint32_t                       *idx)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver2_common_reg_info *cmn_reg = NULL;
	struct cam_ife_csid_cid_data *cid_data;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	uint32_t cfg0, cfg1 = 0, val = 0;

	//pixel path
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	cmn_reg = csid_reg->cmn_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	path_reg = csid_reg->path_reg[res->res_id];
	cid_data = &csid_hw->cid_data[path_cfg->cid];

	cfg0 = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt <<
		 	cmn_reg->decode_format_shift_val);

	if (csid_reg->cmn_reg->vfr_supported)
		cfg0 |= path_cfg->vfr_en << cmn_reg->vfr_en_shift_val;

	if (csid_reg->cmn_reg->frame_id_dec_supported)
		cfg0 |= path_cfg->frame_id_dec_en <<
			cmn_reg->frame_id_decode_en_shift_val;

	if (cmn_reg->timestamp_enabled_in_cfg0)
		cfg0 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	cfg0 |= (1 << cmn_reg->path_en_shift_val);

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->cfg0_addr, cfg0);

	if (csid_hw->flags.binning_enabled) {

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_HORIZONTAL)
			cfg1 |= path_cfg->horizontal_bin <<
				path_reg->bin_h_en_shift_val;

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_VERTICAL)
			cfg1 |= path_cfg->vertical_bin <<
				path_reg->bin_v_en_shift_val;

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_QCFA)
			cfg1 |= path_cfg->qcfa_bin <<
				path_reg->bin_qcfa_en_shift_val;

		if (path_cfg->qcfa_bin || path_cfg->vertical_bin ||
				path_cfg->horizontal_bin)
			cfg1 |= 1  << path_reg->bin_en_shift_val;
	}

	cfg1 |= (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if (cmn_reg->drop_supported)
		cfg1 |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	cfg1 |= 1 << path_reg->pix_store_en_shift_val;

	if (!cmn_reg->timestamp_enabled_in_cfg0)
		cfg1 |= (1 << path_reg->timestamp_en_shift_val) |
			(cmn_reg->timestamp_strobe_val <<
				cmn_reg->timestamp_stb_sel_shift_val);

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->cfg1_addr, cfg1);

	/* set frame drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->frm_drop_period_addr, 1);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->frm_drop_pattern_addr, 0);

	/* set pxl drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->pix_drop_pattern_addr, 0);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->pix_drop_period_addr, 1);

	/* set line drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->line_drop_pattern_addr, 0);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->line_drop_period_addr, 1);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->cfg0_addr, cfg0);

	if (path_reg->overflow_ctrl_en) {
		val = path_reg->overflow_ctrl_en |
			path_reg->overflow_ctrl_mode_val;
		CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->err_recovery_cfg0_addr, val);
	}

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->ctrl_addr, 0);

	// No dual IFE
	*rup_aup_mask |= path_reg->rup_aup_mask;

	return 0;
}

static int cam_ife_virt_csid_program_ipp_path (
	struct cam_ife_virt_csid    *csid_hw,
	struct cam_isp_resource_node   *res,
	uint32_t                       *rup_aup_mask,
	uint32_t                       *cdm_payload,
	uint32_t                       *idx)
{
	const struct cam_ife_csid_ver2_reg_info *csid_reg;
	const struct cam_ife_csid_ver2_path_reg_info *path_reg = NULL;
	const struct cam_ife_csid_ver2_common_reg_info *cmn_reg = NULL;
	struct cam_ife_csid_cid_data *cid_data;
	struct cam_ife_csid_ver2_path_cfg *path_cfg;
	uint32_t cfg0, cfg1 = 0, val = 0;

	//pixel path
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	cmn_reg = csid_reg->cmn_reg;
	path_cfg = (struct cam_ife_csid_ver2_path_cfg *)res->res_priv;
	path_reg = csid_reg->path_reg[res->res_id];
	cid_data = &csid_hw->cid_data[path_cfg->cid];

	cfg0 = (cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc <<
			cmn_reg->vc_shift_val) |
		(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt <<
			cmn_reg->dt_shift_val) |
		(path_cfg->cid << cmn_reg->dt_id_shift_val) |
		(path_cfg->path_format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].decode_fmt <<
		 	cmn_reg->decode_format_shift_val);

	cfg0 |= (1 << cmn_reg->path_en_shift_val);

	// append offset and val
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->cfg0_addr, cfg0);

	// binning enable
	if (csid_hw->flags.binning_enabled) {

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_HORIZONTAL)
			cfg1 |= path_cfg->horizontal_bin <<
				path_reg->bin_h_en_shift_val;

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_VERTICAL)
			cfg1 |= path_cfg->vertical_bin <<
				path_reg->bin_v_en_shift_val;

		if (path_reg->binning_supported & CAM_IFE_CSID_BIN_QCFA)
			cfg1 |= path_cfg->qcfa_bin <<
				path_reg->bin_qcfa_en_shift_val;

		if (path_cfg->qcfa_bin || path_cfg->vertical_bin ||
				path_cfg->horizontal_bin)
			cfg1 |= 1  << path_reg->bin_en_shift_val;
	}

	cfg1 |= (path_cfg->crop_enable << path_reg->crop_h_en_shift_val) |
		(path_cfg->crop_enable <<
		 path_reg->crop_v_en_shift_val);

	if (cmn_reg->drop_supported)
		cfg1 |= (path_cfg->drop_enable <<
				path_reg->drop_v_en_shift_val) |
			(path_cfg->drop_enable <<
				path_reg->drop_h_en_shift_val);

	cfg1 |= 1 << path_reg->pix_store_en_shift_val;

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->cfg1_addr, cfg1);

	/* set frame drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->frm_drop_period_addr, 1);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->frm_drop_pattern_addr, 0);

	/* set pxl drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->pix_drop_pattern_addr, 0);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->pix_drop_period_addr, 1);

	/* set line drop pattern to 0 and period to 1 */
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->line_drop_pattern_addr, 0);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->line_drop_period_addr, 1);

	if (path_reg->overflow_ctrl_en) {
		val = path_reg->overflow_ctrl_en |
			path_reg->overflow_ctrl_mode_val;
		CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->err_recovery_cfg0_addr, val);
	}

	val = path_reg->start_master_sel_val <<
		path_reg->start_master_sel_shift;

	if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_MASTER) {
		/* Set start mode as master */
		val |= path_reg->start_mode_master  <<
			path_reg->start_mode_shift;
	} else if (path_cfg->sync_mode == CAM_ISP_HW_SYNC_SLAVE) {
		/* Set start mode as slave */
		val |= path_reg->start_mode_slave <<
			path_reg->start_mode_shift;
	} else {
		/* Default is internal halt mode */
		val = 0;
	}

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, path_reg->ctrl_addr, val);

	// No dual IFE
	*rup_aup_mask |= path_reg->rup_aup_mask;

	return 0;
}

static int cam_ife_virt_csid_rx_capture_config(
	struct cam_ife_virt_csid       *csid_hw,
	uint32_t                       *cdm_payload,
	uint32_t                       *idx)
{
	const struct cam_ife_csid_ver2_reg_info   *csid_reg;
	// struct cam_hw_soc_info                    *soc_info;
	struct cam_ife_csid_rx_cfg                *rx_cfg;
	uint32_t vc, dt, i;
	uint32_t val = 0;

	for (i = 0; i < CAM_IFE_CSID_CID_MAX; i++)
		if (csid_hw->cid_data[i].cid_cnt)
			break;

	if (i == CAM_IFE_CSID_CID_MAX) {
		CAM_WARN(CAM_ISP, "VCSID[%d] no valid cid",
			csid_hw->hw_intf->hw_idx);
		return 0;
	}

	rx_cfg = &csid_hw->rx_cfg;

	vc  = csid_hw->cid_data[i].vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc;
	dt  = csid_hw->cid_data[i].vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *) csid_hw->core_info->csid_reg;

	/* CAM_IFE_CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE */
	val |= ((1 << csid_reg->csi2_reg->capture_long_pkt_en_shift) |
		(dt << csid_reg->csi2_reg->capture_long_pkt_dt_shift) |
		(vc << csid_reg->csi2_reg->capture_long_pkt_vc_shift));

	/* CAM_IFE_CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE */
	if (rx_cfg->lane_type == CAM_ISP_LANE_TYPE_CPHY) {
		val |= ((1 << csid_reg->csi2_reg->capture_cphy_pkt_en_shift) |
			(dt << csid_reg->csi2_reg->capture_cphy_pkt_dt_shift) |
			(vc << csid_reg->csi2_reg->capture_cphy_pkt_vc_shift));
	}

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, csid_reg->csi2_reg->capture_ctrl_addr, val);


	return 0;
}

void cam_ife_virt_csid_print_reg(uint32_t *regs, uint32_t len, char *tag)
{
	int i;
	CAM_INFO(CAM_ISP, "%s", tag);
	for (i = 0; i < len; i+=2)
		CAM_INFO(CAM_ISP, "%x -> %x", regs[i], regs[i + 1]);
}

static __maybe_unused void cam_ife_hw_mgr_dump_sensor_data(
		struct cam_ife_hybrid_sensor_data *sdata)
{
	int i;

	if (!sdata) {
		CAM_ERR(CAM_ISP, "sensor info is NULL");
		return;
	}

	CAM_DBG(CAM_ISP, "is_cphy %d, phy_num %d, num_lanes %d",
			sdata->is_cphy, sdata->phy_num, sdata->num_lanes);
	CAM_DBG(CAM_ISP, "lane_cfg %d, epd_node %d",
			sdata->lane_cfg, sdata->epd_mode);
	CAM_DBG(CAM_ISP, "decode_fmt %d, num_valid_vcdt %d",
			sdata->decode_format, sdata->num_vcdt);

	for(i = 0; i < CAM_VIRT_ISP_VC_DT_CFG; i++) {
		CAM_DBG(CAM_ISP, "vc[%02d]: %d, dt[%02d]: %d",
				i, sdata->vcdt[i].vc, i, sdata->vcdt[i].dt);
	}
}

int __translate_to_decode_fmt(int fmt)
{
	switch(fmt) {
		case CAM_FORMAT_MIPI_RAW_8:
			return 1;
		case CAM_FORMAT_MIPI_RAW_10:
			return 2;
		case CAM_FORMAT_MIPI_RAW_12:
			return 3;
		case CAM_FORMAT_MIPI_RAW_14:
			return 4;
		case CAM_FORMAT_MIPI_RAW_16:
			return 5;
		case CAM_FORMAT_MIPI_RAW_20:
			return 6;
		default:
			CAM_ERR(CAM_ISP, "Unknown fmt %d", fmt);
	}
	return -1;
}

int cam_ife_virt_csid_populate_hybrid_regs(void *hw_priv, void *args,
		int arg_size)
{
	struct cam_virt_process_cmd           *pcmd;
	struct cam_ife_hybrid_sensor_data     *sdata;
	uint32_t                              *cdm_payload, *idx;
	uint32_t                              *size_ptr, old_idx;
	uint32_t cfg = 0, csi_cfg0 = 0, csi_cfg1 = 0, val, ctrl = 0;
	uint32_t vcdt2 = 0, vcdt3 = 0, vcdt4 = 0, vcdt5 = 0;
	int rc = 0;

	pcmd = (struct cam_virt_process_cmd *)args;
	sdata = (struct cam_ife_hybrid_sensor_data *)pcmd->args;

	if (!sdata) {
		CAM_ERR(CAM_ISP, "Sensor data is null");
		return -EINVAL;
	}
	cam_ife_hw_mgr_dump_sensor_data(sdata);

	cdm_payload = pcmd->pkt;
	idx = pcmd->off;

	cdm_payload[(*idx)++] = CAM_SLAVE_HW_CSID;
	size_ptr = &cdm_payload[(*idx)++];
	old_idx = (*idx)++; // 4 byted CDM header

	switch (sdata->num_vcdt) {
		case 10:
			vcdt5 |= sdata->vcdt[9].vc << MULTI_VCDT_VC_SHIFT |
				sdata->vcdt[9].dt << MULTI_VCDT_DT_SHIFT |
				1 << MULTI_VCDT_EN_SHIFT;
		case 9:
			vcdt5 |= sdata->vcdt[8].vc << VCDT_CFGn_VC_SHIFT |
				sdata->vcdt[8].dt << VCDT_CFGn_DT_SHIFT |
				1 << VCDT_CFGn_EN_SHIFT;
			CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx,
					RDI3_VCDT_CFG5_OFF, vcdt5);
		case 8:
			vcdt4 |= sdata->vcdt[7].vc << MULTI_VCDT_VC_SHIFT |
				sdata->vcdt[7].dt << MULTI_VCDT_DT_SHIFT |
				1 << MULTI_VCDT_EN_SHIFT;
		case 7:
			vcdt4 |= sdata->vcdt[6].vc << VCDT_CFGn_VC_SHIFT |
				sdata->vcdt[6].dt << VCDT_CFGn_DT_SHIFT |
				1 << VCDT_CFGn_EN_SHIFT;
			CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx,
					RDI3_VCDT_CFG4_OFF, vcdt4);
		case 6:
			vcdt3 |= sdata->vcdt[5].vc << MULTI_VCDT_VC_SHIFT |
				sdata->vcdt[5].dt << MULTI_VCDT_DT_SHIFT |
				1 << MULTI_VCDT_EN_SHIFT;
		case 5:
			vcdt3 |= sdata->vcdt[4].vc << VCDT_CFGn_VC_SHIFT |
				sdata->vcdt[4].dt << VCDT_CFGn_DT_SHIFT |
				1 << VCDT_CFGn_EN_SHIFT;
			CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx,
					RDI3_VCDT_CFG3_OFF, vcdt3);
		case 4:
			vcdt2 |= sdata->vcdt[3].vc << MULTI_VCDT_VC_SHIFT |
				sdata->vcdt[3].dt << MULTI_VCDT_DT_SHIFT |
				1 << MULTI_VCDT_EN_SHIFT;
		case 3:
			vcdt2 |= sdata->vcdt[2].vc << VCDT_CFGn_VC_SHIFT |
				sdata->vcdt[2].dt << VCDT_CFGn_DT_SHIFT |
				1 << VCDT_CFGn_EN_SHIFT;
			CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx,
					RDI3_VCDT_CFG2_OFF, vcdt2);
		case 2:
			/* programmed in RDI3_MULTI_VCDT_CFG0 */
		case 1:
			/* programmed in RDI3_CFG0 */
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid num_modes %d",
					sdata->num_vcdt);
	}

	cfg = 0;
	/* data is always mipi packed */
	cfg |= (1 << PACKING_FORMAT_SHIFT);
	cfg &= ~(1 << CROP_V_EN_SHIFT | 1 << CROP_H_EN_SHIFT);
	cfg &= ~(1 << DROP_V_EN_SHIFT | 1 << DROP_H_EN_SHIFT);
	cfg |= (1 << TIMESTAMP_EN_SHIFT);
	/* time-stamp strobe is post IRQ subsample strobe */
	cfg |= (2 << TIMESTAMP_STB_SEL_SHIFT);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, RDI3_CFG1_OFF, cfg);

	cfg = 0;
	/* Decode format in 12-15 bits */
	cfg |= (__translate_to_decode_fmt(sdata->decode_format) &
			DECODE_FORMAT_MASK) << DECODE_FORMAT_SHIFT;
	/* set 31st bit */
	cfg |= 1 << RDI_ENABLE_SHIFT;

	/* set DT_ID to 0 */
	cfg &= (~(1 << DT_ID_SHIFT));

	/* enable PIX_STORE */
	cfg |= (1 << PIX_STORE_EN_SHIFT);

	cfg |= (sdata->vcdt[0].vc << CFG0_VC_SHIFT);
	cfg |= (sdata->vcdt[0].dt << CFG0_DT_SHIFT);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, RDI3_CFG0_OFF, cfg);

	csi_cfg0 = (sdata->is_cphy << PHY_TYPE_SEL_SHIFT) |
			((sdata->num_lanes - 1) << NUM_ACTIVE_LANES_SHIFT) |
			(sdata->lane_cfg << INPUT_SEL_SHIFT) |
			((sdata->phy_num & 0xFF) << PHY_NUM_SEL_SHIFT);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, CSI2_RX_CFG0_OFF, csi_cfg0);

	/* CSID_LITE_0_RDI3_CTRL */
	ctrl = 0;

	/* Resume at frame boundary */
	ctrl |= (1 << START_CMD_SHIFT);

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, RDI3_CTRL_OFF, ctrl);

	val = 0;
	val |= sdata->vcdt[1].vc << VC1_SHIFT;
	val |= sdata->vcdt[1].dt << DT1_SHIFT;
	val |= sdata->num_vcdt >= 2 ? 1 : 0;
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, RDI3_MULTI_VCDT_CFG0, val);

	csi_cfg1 = 1 << ECC_CORR_SHIFT;

	/* set VC_MODE to extended */
	csi_cfg1 |= (1 << VC_MODE_SHIFT);

	/* set EPD_MODE */
	csi_cfg1 |= (sdata->epd_mode << EPD_MODE_SHIFT);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, CSI2_RX_CFG1_OFF, csi_cfg1);

	*size_ptr = (*idx - old_idx) * 4; // size in bytes
	/* add cdm header in payload */
	CAM_CDM_SET_TYPE(size_ptr + 1, CAM_CDM_TYPE_REG_RAND);
	CAM_CDM_SET_SIZE(size_ptr + 1, ((*idx - old_idx) - 1) / 2); // RegRand

	return rc;
}

int cam_ife_virt_csid_populate_regs(void *hw_priv, void *args, int arg_size)
{
	struct cam_virt_process_cmd           *pcmd;
	struct cam_hw_info                    *hw_info;
	struct cam_ife_virt_csid              *csid_hw;
	struct cam_csid_hw_start_args         *start_args;
	struct cam_isp_resource_node          *res;
	struct cam_ife_csid_ver2_reg_info     *csid_reg;
	const struct cam_ife_csid_csi2_rx_reg_info  *csi2_reg;
	struct cam_ife_csid_rx_cfg            *rx_cfg;
	uint32_t                               rup_aup_mask = 0;
	int                                    i, rc = 0;
	uint32_t                              *cdm_payload;
	uint32_t                               *idx = NULL, old_idx, val = 0;
	uint32_t                              *size_ptr;
	int                                    vc_full_width;

	hw_info = (struct cam_hw_info *)hw_priv;
	csid_hw = (struct cam_ife_virt_csid *)hw_info->core_info;
	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	csi2_reg  = csid_reg->csi2_reg;
	pcmd = (struct cam_virt_process_cmd *)args;
	start_args = (struct cam_csid_hw_start_args *)pcmd->args;
	rx_cfg  = &csid_hw->rx_cfg;
	cdm_payload = pcmd->pkt;
	idx = pcmd->off;

	if (!start_args) {
		CAM_ERR(CAM_ISP, "start_args is NULL");
		return -EINVAL;
	}

	cdm_payload[(*idx)++] = CAM_SLAVE_HW_CSID;
	size_ptr = &cdm_payload[(*idx)++];
	old_idx = (*idx)++; // 4 byted CDM header

	for (i = 0; i < start_args->num_res; i++) {
		res = start_args->node_res[i];
		CAM_DBG(CAM_ISP, "res %d type %x", i, res->res_id);
		switch (res->res_id) {
			case  CAM_IFE_PIX_PATH_RES_IPP:
			rc = cam_ife_virt_csid_program_ipp_path(csid_hw, res, &rup_aup_mask,
					cdm_payload, idx);
			if (rc)
				goto end;
			break;

			case  CAM_IFE_PIX_PATH_RES_PPP:
			rc = cam_ife_virt_csid_program_ppp_path(csid_hw, res, &rup_aup_mask,
					cdm_payload, idx);
			if (rc)
				goto end;
			break;

			case CAM_IFE_PIX_PATH_RES_RDI_0:
			case CAM_IFE_PIX_PATH_RES_RDI_1:
			case CAM_IFE_PIX_PATH_RES_RDI_2:
			case CAM_IFE_PIX_PATH_RES_RDI_3:
			case CAM_IFE_PIX_PATH_RES_RDI_4:
			rc = cam_ife_virt_csid_program_rdi_path(csid_hw, res, &rup_aup_mask,
					cdm_payload, idx);
			if (rc)
				goto end;
			break;

			default:
				CAM_ERR(CAM_ISP, "CSID:%d Invalid res type %d",
						csid_hw->hw_intf->hw_idx, res->res_type);
				break;
		}

	}
	rup_aup_mask |= (csid_hw->rx_cfg.mup << csid_reg->cmn_reg->mup_shift_val);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx,
			csid_reg->cmn_reg->rup_aup_cmd_addr, rup_aup_mask);

	/*Configure Rx cfg0 */
	val |= ((rx_cfg->lane_cfg << csi2_reg->lane_cfg_shift) |
		((rx_cfg->lane_num - 1) << csi2_reg->lane_num_shift) |
		(rx_cfg->lane_type << csi2_reg->phy_type_shift));

	if (rx_cfg->tpg_mux_sel) {
		val |= ((rx_cfg->tpg_num_sel << csi2_reg->tpg_num_sel_shift) |
			(rx_cfg->tpg_mux_sel << csi2_reg->tpg_mux_en_shift));
	} else {
		val |= rx_cfg->phy_sel << csi2_reg->phy_num_shift;
	}

	CAM_DBG(CAM_ISP, "lane_cfg %x lane_num %x, lane_type %x phy_sel %x tpg_sel %d",
			rx_cfg->lane_cfg, rx_cfg->lane_num, rx_cfg->lane_type, rx_cfg->phy_sel,
			rx_cfg->tpg_mux_sel);
	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, csi2_reg->cfg0_addr, val);

	val = 0;
	/*Configure Rx cfg1*/
	val = 1 << csi2_reg->misr_enable_shift_val;
	val |= 1 << csi2_reg->ecc_correction_shift_en;
	val |= (rx_cfg->epd_supported << csi2_reg->epd_mode_shift_en);
	if (rx_cfg->dynamic_sensor_switch_en)
		val |= 1 << csi2_reg->dyn_sensor_switch_shift_en;

	vc_full_width = cam_ife_csid_is_vc_full_width(csid_hw->cid_data);

	if (vc_full_width == 1) {
		val |= 1 <<  csi2_reg->vc_mode_shift_val;
	} else if (vc_full_width < 0) {
		CAM_ERR(CAM_ISP, "Error VC DT");
		return -EINVAL;
	}

	CAM_ADD_REG_VAL_PAIR(cdm_payload, *idx, csi2_reg->cfg1_addr, val);

	cam_ife_virt_csid_rx_capture_config(csid_hw, cdm_payload, idx);

	*size_ptr = (*idx - old_idx) * 4; // size in bytes
	/* add cdm header in payload */
	CAM_CDM_SET_TYPE(size_ptr + 1, CAM_CDM_TYPE_REG_RAND);
	CAM_CDM_SET_SIZE(size_ptr + 1, ((*idx - old_idx) - 1) / 2);

end:
	return rc;
}

int cam_ife_virt_csid_start(void *hw_priv, void *args, uint32_t arg_size)
{
	WARN_ON_ONCE(1);
	CAM_ERR(CAM_ISP,  "partly Unimplemented");
	return 0;
}

int cam_ife_virt_csid_stop(void *hw_priv, void *stop_args, uint32_t arg_size)
{
	WARN_ON_ONCE(1);
	CAM_ERR(CAM_ISP,  "Unimplemented");
	return 0;
}

int cam_ife_virt_csid_read(void *hw_priv, void *read_args, uint32_t arg_size)
{
	WARN_ON_ONCE(1);
	CAM_ERR(CAM_ISP,  "Unimplemented");
	return 0;
}

int cam_ife_virt_csid_write(void *hw_priv, void *write_args, uint32_t arg_size)
{
	WARN_ON_ONCE(1);
	CAM_ERR(CAM_ISP,  "Unimplemented");
	return 0;
}

int cam_ife_virt_csid_process_cmd(void *hw_priv, uint32_t cmd_type, void *cmd_args,
	uint32_t arg_size)
{
	int rc = 0;
	struct cam_virt_process_cmd          *pcmd;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "CSID: Invalid arguments");
		return -EINVAL;
	}

	pcmd = (struct cam_virt_process_cmd *)cmd_args;

	switch (cmd_type) {
	case CAM_ISP_VIRT_POPULATE_REGS:
		if (pcmd->acquire_type == CAM_ISP_ACQUIRE_TYPE_VIRTUAL)
			rc = cam_ife_virt_csid_populate_regs(hw_priv, cmd_args, arg_size);
		else {
			rc = cam_ife_virt_csid_populate_hybrid_regs(hw_priv, cmd_args, arg_size);
		}
		break;
	case CAM_IFE_CSID_SET_CSID_DEBUG:
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported command %d", cmd_type);
		rc = -EINVAL;
	}
	return rc;
}

static int cam_ife_virt_csid_hw_alloc_res(
	struct cam_isp_resource_node *res,
	uint32_t res_type,
	struct cam_hw_intf   *hw_intf,
	uint32_t res_id)

{
	struct cam_ife_csid_ver2_path_cfg *path_cfg = NULL;

	path_cfg = kzalloc(sizeof(*path_cfg), GFP_KERNEL);

	if (!path_cfg)
		return -ENOMEM;

	res->res_id = res_id;
	res->res_type = res_type;
	res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	res->hw_intf = hw_intf;
	res->res_priv = path_cfg;

	return 0;
}

int cam_ife_virt_csid_hw_init_path_res(struct cam_ife_virt_csid *csid_hw)
{
	int rc = 0, i;
	struct cam_ife_csid_ver2_reg_info *csid_reg;
	struct cam_isp_resource_node *res;

	csid_reg = (struct cam_ife_csid_ver2_reg_info *)
			csid_hw->core_info->csid_reg;
	/* Initialize the IPP resources */
	if (csid_reg->cmn_reg->num_pix) {
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP];
		rc = cam_ife_virt_csid_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_IPP);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d IPP res init fail",
				csid_hw->hw_intf->hw_idx);
			goto free_res;
		}
		scnprintf(csid_hw->path_res[CAM_IFE_PIX_PATH_RES_IPP].res_name,
			CAM_ISP_RES_NAME_LEN, "V_IPP");
	}

	/* Initialize PPP resource */
	if (csid_reg->cmn_reg->num_ppp) {
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP];
		rc = cam_ife_virt_csid_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_PPP);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d PPP res init fail",
				csid_hw->hw_intf->hw_idx);
			goto free_res;
		}
		scnprintf(csid_hw->path_res[CAM_IFE_PIX_PATH_RES_PPP].res_name,
			CAM_ISP_RES_NAME_LEN, "PPP");
	}

	/* Initialize the RDI resource */
	for (i = 0; i < csid_reg->cmn_reg->num_rdis; i++) {
		/* res type is from RDI 0 to RDI3 */
		res = &csid_hw->path_res[CAM_IFE_PIX_PATH_RES_RDI_0 + i];
		rc = cam_ife_virt_csid_hw_alloc_res(
			res,
			CAM_ISP_RESOURCE_PIX_PATH,
			csid_hw->hw_intf,
			CAM_IFE_PIX_PATH_RES_RDI_0 + i);
		if (rc) {
			CAM_ERR(CAM_ISP, "CSID: %d RDI[%d] res init fail",
				csid_hw->hw_intf->hw_idx, i);
			goto free_res;
		}
		scnprintf(res->res_name, CAM_ISP_RES_NAME_LEN, "V_RDI_%d", i);
	}

	CAM_INFO(CAM_ISP, "alloc res pix %d, ppp %d, rdi %d", csid_reg->cmn_reg->num_pix,
			csid_reg->cmn_reg->num_ppp, csid_reg->cmn_reg->num_rdis);
free_res:
	//TODO: free resources
	return 0;
}
int cam_ife_virt_csid_hw_probe_init(struct cam_hw_intf *hw_intf,
	struct cam_ife_csid_core_info *core_info)
{

	int rc = -EINVAL;
	struct cam_hw_info                   *hw_info;
	struct cam_ife_virt_csid             *csid_hw = NULL;

	if (!hw_intf || !core_info) {
		CAM_ERR(CAM_ISP, "Invalid parameters intf: %pK hw_info: %pK",
			hw_intf, core_info);
		return rc;
	}

	hw_info = (struct cam_hw_info  *)hw_intf->hw_priv;

	csid_hw = kzalloc(sizeof(struct cam_ife_virt_csid), GFP_KERNEL);

	if (!csid_hw) {
		CAM_ERR(CAM_ISP, "virt csid core %d hw allocation fails",
			hw_intf->hw_idx);
		return -ENOMEM;
	}

	hw_info->core_info = csid_hw;
	csid_hw->hw_intf = hw_intf;
	csid_hw->hw_info = hw_info;
	csid_hw->core_info = core_info;
	CAM_DBG(CAM_ISP, "type %d index %d",
		hw_intf->hw_type,
		hw_intf->hw_idx);

	csid_hw->hw_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&csid_hw->hw_info->hw_mutex);
	spin_lock_init(&csid_hw->hw_info->hw_lock);

	csid_hw->hw_intf->hw_ops.get_hw_caps = cam_ife_virt_csid_get_hw_caps;
	csid_hw->hw_intf->hw_ops.init        = cam_ife_virt_csid_init_hw;
	csid_hw->hw_intf->hw_ops.deinit      = cam_ife_virt_csid_deinit_hw;
	csid_hw->hw_intf->hw_ops.reset       = cam_ife_virt_csid_reset;
	csid_hw->hw_intf->hw_ops.reserve     = cam_ife_virt_csid_reserve;
	csid_hw->hw_intf->hw_ops.release     = cam_ife_virt_csid_release;
	csid_hw->hw_intf->hw_ops.start       = cam_ife_virt_csid_start;
	csid_hw->hw_intf->hw_ops.stop        = cam_ife_virt_csid_stop;
	csid_hw->hw_intf->hw_ops.read        = cam_ife_virt_csid_read;
	csid_hw->hw_intf->hw_ops.write       = cam_ife_virt_csid_write;
	csid_hw->hw_intf->hw_ops.process_cmd = cam_ife_virt_csid_process_cmd;

	rc = cam_ife_virt_csid_hw_init_path_res(csid_hw);

	if (rc) {
		CAM_ERR(CAM_ISP, "VCSID[%d] Probe Init failed",
			hw_intf->hw_idx);
		goto free_hw_info;
	}
	if (hw_intf->hw_idx < CAM_IFE_CSID_HW_NUM_MAX)
		cam_ife_csid_hw_list[hw_intf->hw_idx] = hw_intf;
	else {
		CAM_DBG(CAM_ISP, "Failed to register virt csid with hw_mgr idx %d",
			hw_intf->hw_idx);
		goto free_hw_info;
	}
	CAM_INFO(CAM_ISP, "VCSID:%d component bound successfully",
		hw_intf->hw_idx);

	return 0;

free_hw_info:
	kfree(csid_hw);
	return -EBUSY;
}

// static
int cam_ife_virt_csid_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_intf             *hw_intf;
	struct cam_hw_info             *hw_info;
	const struct of_device_id      *match_dev = NULL;
	struct cam_ife_csid_core_info  *vcsid_core_info = NULL;
	uint32_t                        vcsid_dev_idx;
	int                             rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	CAM_INFO(CAM_ISP, "Binding IFE Virt CSID component");

	hw_intf = kzalloc(sizeof(*hw_intf), GFP_KERNEL);
	if (!hw_intf) {
		rc = -ENOMEM;
		goto err;
	}

	hw_info = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!hw_info) {
		rc = -ENOMEM;
		goto free_hw_intf;
	}

	/* get ife csid hw index */
	of_property_read_u32(pdev->dev.of_node, "cell-index", &vcsid_dev_idx);
	/* get ife csid hw information */
	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "No matching table for the IFE CSID HW!");
		rc = -EINVAL;
		goto free_hw_info;
	}

	hw_intf->hw_idx = vcsid_dev_idx;
	hw_intf->hw_type = CAM_ISP_HW_TYPE_VCSID;
	hw_intf->hw_priv = hw_info;

	hw_info->soc_info.pdev = pdev;
	hw_info->soc_info.dev = &pdev->dev;
	hw_info->soc_info.dev_name = pdev->name;
	hw_info->soc_info.index = vcsid_dev_idx;
	hw_info->is_virtual = true;

	vcsid_core_info = (struct cam_ife_csid_core_info  *)match_dev->data;
	rc = cam_ife_virt_csid_hw_probe_init(hw_intf, vcsid_core_info);
	platform_set_drvdata(pdev, hw_intf);

	return 0;

free_hw_info:
	kfree(hw_info);
free_hw_intf:
	kfree(hw_intf);
err:
	return rc;
}

static void cam_ife_virt_csid_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	CAM_DBG(CAM_ISP, "Virt CSID component unbind");
}

const static struct component_ops cam_ife_virt_csid_component_ops = {
	.bind = cam_ife_virt_csid_component_bind,
	.unbind = cam_ife_virt_csid_component_unbind,
};

int cam_ife_virt_csid_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_INFO(CAM_ISP, "Adding VIRT IFE CSID component");
	rc = component_add(&pdev->dev, &cam_ife_virt_csid_component_ops);
	if (rc)
		CAM_ERR(CAM_ISP, "failed to add component rc: %d", rc);

	return rc;
}

int cam_ife_virt_csid_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_ife_virt_csid_component_ops);
	return 0;
}

static struct cam_ife_csid_core_info cam_ife_csid_lite_650_hw_info = {
	.csid_reg = &cam_ife_csid_lite_650_reg_info,
	.sw_version  = CAM_IFE_CSID_VER_2_0,
};

static const struct of_device_id cam_ife_virt_csid_dt_match[] = {

	{
		.compatible = "qcom,virt_csid",
		.data = &cam_ife_csid_lite_650_hw_info,
	},
	{},
};

MODULE_DEVICE_TABLE(of, cam_ife_virt_csid_dt_match);

struct platform_driver cam_ife_virt_csid_driver = {
	.probe = cam_ife_virt_csid_probe,
	.remove = cam_ife_virt_csid_remove,
	.driver = {
		.name = CAM_VIRT_CSID_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_ife_virt_csid_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_ife_virt_csid_init_module(void)
{
	CAM_INFO(CAM_ISP, "register VIRT IFE CSID component");
	return platform_driver_register(&cam_ife_virt_csid_driver);
}

void cam_ife_virt_csid_exit_module(void)
{
	platform_driver_unregister(&cam_ife_virt_csid_driver);
}

MODULE_DESCRIPTION("CAM IFE_VIRTUAL_CSID driver");
MODULE_LICENSE("GPL v2");
