// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/iopoll.h>
#include <linux/slab.h>

#include <media/cam_isp.h>
#include <media/cam_defs.h>

#include <dt-bindings/msm-camera.h>

#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_hw.h"
#include "cam_cdm_util.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_ife_csid_soc.h"
#include "cam_ife_csid_common.h"
#include "cam_ife_csid_hw_ver1.h"
#include "cam_ife_csid_hw_ver2.h"
#include "cam_cdm_intf_api.h"

struct csid_ref_time g_ref_time;

const uint8_t *cam_ife_csid_irq_reg_tag[CAM_IFE_CSID_IRQ_REG_MAX] = {
	"TOP",
	"RX",
	"RDI0",
	"RDI1",
	"RDI2",
	"RDI3",
	"RDI4",
	"IPP",
	"PPP",
	"UDI0",
	"UDI1",
	"UDI2",
};

int cam_ife_csid_get_phy_sel(uint32_t in_res)
{
	switch (in_res) {
	case CAM_ISP_IFE_IN_RES_PHY_0:
		return 1;
	case CAM_ISP_IFE_IN_RES_PHY_1:
		return 2;
	case CAM_ISP_IFE_IN_RES_PHY_2:
		return 3;
	case CAM_ISP_IFE_IN_RES_PHY_3:
		return 4;
	case CAM_ISP_IFE_IN_RES_PHY_4:
		return 5;
	case CAM_ISP_IFE_IN_RES_PHY_5:
		return 6;
	case CAM_ISP_IFE_IN_RES_PHY_6:
		return 7;
	default:
		CAM_ERR(CAM_ISP, "in _res 0x%x don't have valid phy", in_res);
		return -EINVAL;
	}
}

static int cam_ife_csid_get_cid(struct cam_ife_csid_cid_data *cid_data,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{
	uint32_t  i;

	/*currently only signal vc/dt is supported with per-port en */
	if (reserve->in_port->per_port_en && reserve->per_port_acquire) {
		if (cid_data->cid_cnt == 0) {
			cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc = reserve->vc;
			cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt = reserve->dt;
			cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].valid = true;
			cid_data->num_vc_dt = 1;
			return 0;
		}
		if ((cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].vc == reserve->vc) &&
			(cid_data->vc_dt[CAM_IFE_CSID_MULTI_VC_DT_GRP_0].dt == reserve->dt)) {
			return 0;
		}
		return -EINVAL;
	}

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

int cam_ife_csid_is_pix_res_format_supported(
	uint32_t in_format)
{
	int rc = -EINVAL;

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_MIPI_RAW_12:
	case CAM_FORMAT_MIPI_RAW_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_MIPI_RAW_20:
	case CAM_FORMAT_DPCM_10_6_10:
	case CAM_FORMAT_DPCM_10_8_10:
	case CAM_FORMAT_DPCM_12_6_12:
	case CAM_FORMAT_DPCM_12_8_12:
	case CAM_FORMAT_DPCM_14_8_14:
	case CAM_FORMAT_DPCM_14_10_14:
	case CAM_FORMAT_DPCM_12_10_12:
	case CAM_FORMAT_YUV422:
	case CAM_FORMAT_YUV422_10:
		rc = 0;
		break;
	default:
		break;
	}
	return rc;
}

static int cam_ife_csid_validate_rdi_format(uint32_t in_format,
	uint32_t out_format)
{
	int rc = 0;

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_6:
		case CAM_FORMAT_PLAIN8:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_8:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_PLAIN128:
		case CAM_FORMAT_PLAIN8:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_PLAIN128:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_16:
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_PLAIN8:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_16:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_14:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_16:
		case CAM_FORMAT_PLAIN16_16:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		switch (out_format) {
		case CAM_FORMAT_MIPI_RAW_20:
		case CAM_FORMAT_PLAIN32_20:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_YUV422:
		switch (out_format) {
		case CAM_FORMAT_YUV422:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_FORMAT_YUV422_10:
		switch (out_format) {
		case CAM_FORMAT_YUV422_10:
			break;
		default:
			rc = -EINVAL;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc)
		CAM_ERR(CAM_ISP, "Unsupported format pair in %d out %d",
			in_format, out_format);
	return rc;
}

int cam_ife_csid_get_format_rdi(
	uint32_t in_format, uint32_t out_format,
	struct cam_ife_csid_path_format *path_format, bool mipi_pack_supported,
	bool mipi_unpacked)
{
	int rc = 0;

	rc = cam_ife_csid_validate_rdi_format(in_format, out_format);
	if (rc)
		goto err;

	memset(path_format, 0, sizeof(*path_format));
	/* if no packing supported and input is same as output dump the raw payload */
	if (!mipi_pack_supported && (in_format == out_format)) {
		path_format->decode_fmt = 0xf;
		goto end;
	}

	/* Configure the incoming stream format types */
	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
		path_format->decode_fmt = 0x0;
		path_format->bits_per_pxl = 6;
		break;
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_YUV422:
		path_format->decode_fmt = 0x1;
		path_format->bits_per_pxl = 8;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_YUV422_10:
		path_format->decode_fmt = 0x2;
		path_format->bits_per_pxl = 10;
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		path_format->decode_fmt = 0x3;
		path_format->bits_per_pxl = 12;
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		path_format->decode_fmt = 0x4;
		path_format->bits_per_pxl = 14;
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		path_format->decode_fmt = 0x5;
		path_format->bits_per_pxl = 16;
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		path_format->decode_fmt = 0x6;
		path_format->bits_per_pxl = 20;
		break;
	default:
		rc = -EINVAL;
		goto err;
	}

	/* Configure the out stream format types */
	switch (out_format) {
	case CAM_FORMAT_MIPI_RAW_8:
		if (mipi_unpacked ||
				in_format == CAM_FORMAT_MIPI_RAW_10 ||
				in_format == CAM_FORMAT_MIPI_RAW_12 ||
				in_format == CAM_FORMAT_MIPI_RAW_14 ||
				in_format == CAM_FORMAT_MIPI_RAW_16 ||
				in_format == CAM_FORMAT_MIPI_RAW_20)
			path_format->plain_fmt = 0x0;
		else
			path_format->packing_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_YUV422:
		if (mipi_unpacked)
			path_format->plain_fmt = 0x0;
		else
			path_format->packing_fmt = 0x1;
		break;
	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_PLAIN8:
		path_format->plain_fmt = 0x0;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_MIPI_RAW_12:
	case CAM_FORMAT_MIPI_RAW_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_YUV422_10:
		if (mipi_unpacked)
			path_format->plain_fmt = 0x1;
		else
			path_format->packing_fmt = 0x1;
		break;
	case CAM_FORMAT_PLAIN16_10:
	case CAM_FORMAT_PLAIN16_12:
	case CAM_FORMAT_PLAIN16_14:
	case CAM_FORMAT_PLAIN16_16:
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		if (mipi_unpacked)
			path_format->plain_fmt = 0x2;
		else
			path_format->packing_fmt = 0x1;
		break;
	case CAM_FORMAT_PLAIN32_20:
		path_format->plain_fmt = 0x2;
		break;
	default:
		rc = -EINVAL;
		goto err;
	}

end:
	CAM_DBG(CAM_ISP,
		"in %u out %u plain_fmt %u packing %u decode %u bpp %u unpack %u pack supported %u",
		in_format, out_format, path_format->plain_fmt, path_format->packing_fmt,
		path_format->decode_fmt, path_format->bits_per_pxl, mipi_unpacked,
		mipi_pack_supported);
	return rc;

err:
	CAM_ERR(CAM_ISP, "Unsupported format pair in %d out %d",
		in_format, out_format);
	return rc;
}

int cam_ife_csid_get_format_ipp_ppp(
	uint32_t in_format,
	struct cam_ife_csid_path_format *path_format)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "input format:%d",
		 in_format);

	switch (in_format) {
	case CAM_FORMAT_MIPI_RAW_6:
		path_format->decode_fmt  = 0;
		path_format->plain_fmt = 0;
		break;
	case CAM_FORMAT_MIPI_RAW_8:
		path_format->decode_fmt  = 0x1;
		path_format->plain_fmt = 0;
		break;
	case CAM_FORMAT_MIPI_RAW_10:
		path_format->decode_fmt  = 0x2;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		path_format->decode_fmt  = 0x3;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		path_format->decode_fmt  = 0x4;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		path_format->decode_fmt  = 0x5;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		path_format->decode_fmt  = 0x6;
		path_format->plain_fmt = 0x2;
		break;
	case CAM_FORMAT_DPCM_10_6_10:
		path_format->decode_fmt  = 0x7;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_10_8_10:
		path_format->decode_fmt  = 0x8;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_12_6_12:
		path_format->decode_fmt  = 0x9;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_12_8_12:
		path_format->decode_fmt  = 0xA;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_14_8_14:
		path_format->decode_fmt  = 0xB;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_14_10_14:
		path_format->decode_fmt  = 0xC;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_DPCM_12_10_12:
		path_format->decode_fmt  = 0xD;
		path_format->plain_fmt = 0x1;
		break;
	case CAM_FORMAT_YUV422:
		path_format->decode_fmt  = 0x1;
		path_format->plain_fmt = 0x0;
		break;
	case CAM_FORMAT_YUV422_10:
		path_format->decode_fmt  = 0x2;
		path_format->plain_fmt = 0x0;
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported format %d",
			in_format);
		rc = -EINVAL;
	}

	CAM_DBG(CAM_ISP, "decode_fmt:%d plain_fmt:%d",
		 path_format->decode_fmt,
		 path_format->plain_fmt);

	return rc;
}

int cam_ife_csid_hw_probe_init(struct cam_hw_intf *hw_intf,
	struct cam_ife_csid_core_info *core_info, bool is_custom)
{

	int rc = -EINVAL;

	if (core_info->sw_version == CAM_IFE_CSID_VER_1_0) {
		rc = cam_ife_csid_hw_ver1_init(hw_intf,
			core_info, is_custom);
	} else if (core_info->sw_version == CAM_IFE_CSID_VER_2_0) {
		rc = cam_ife_csid_hw_ver2_init(hw_intf,
			core_info, is_custom);
	}

	return rc;
}

int cam_ife_csid_hw_deinit(struct cam_hw_intf *hw_intf,
	struct cam_ife_csid_core_info *core_info)
{
	int rc = -EINVAL;

	if (core_info->sw_version == CAM_IFE_CSID_VER_1_0)
		rc = cam_ife_csid_hw_ver1_deinit(hw_intf->hw_priv);
	else if (core_info->sw_version == CAM_IFE_CSID_VER_2_0)
		rc = cam_ife_csid_hw_ver2_deinit(
			hw_intf->hw_priv);

	return rc;
}

int cam_ife_csid_is_vc_full_width(struct cam_ife_csid_cid_data *cid_data)
{
	int i, j;
	int rc = 0;
	struct cam_ife_csid_cid_data *p_cid;

	for (i = 0; i < CAM_IFE_CSID_CID_MAX; i++) {
		p_cid = &cid_data[i];

		if (!p_cid->cid_cnt)
			continue;

		if (p_cid->num_vc_dt > CAM_IFE_CSID_MULTI_VC_DT_GRP_MAX) {
			CAM_ERR(CAM_ISP, "Invalid num_vc_dt:%d cid: %d",
				p_cid->num_vc_dt, i);
			rc = -EINVAL;
			goto end;
		}

		for (j = 0; j < p_cid->num_vc_dt; j++) {
			if (p_cid->vc_dt[j].valid &&
				p_cid->vc_dt[j].vc > 3) {
				rc = 1;
				goto end;
			}
		}
	}

end:
	return rc;
}

int cam_ife_csid_cid_reserve(struct cam_ife_csid_cid_data *cid_data,
	uint32_t *cid_value,
	uint32_t hw_idx,
	struct cam_csid_hw_reserve_resource_args  *reserve)
{
	int i, j, rc = 0;

	for (i = 0; i < CAM_IFE_CSID_CID_MAX; i++) {
		rc = cam_ife_csid_get_cid(&cid_data[i], reserve);
		if (!rc)
			break;
	}

	if (i == CAM_IFE_CSID_CID_MAX) {
		for (j = 0; j < reserve->in_port->num_valid_vc_dt; j++) {
			CAM_ERR(CAM_ISP,
				"CSID[%d] reserve fail vc[%d] dt[%d]",
				hw_idx, reserve->in_port->vc[j],
				reserve->in_port->dt[j]);
			return -EINVAL;
		}
	}

	cid_data[i].cid_cnt++;
	*cid_value = i;

	for (j = 0; j < cid_data->num_vc_dt; j++) {
		CAM_DBG(CAM_ISP,
			"CSID[%d] cid_value:%d cid_cnt:%d num_vc_dt:%d vc:%d dt:%d per_port_acquire:%d",
			hw_idx, *cid_value, cid_data[i].cid_cnt, cid_data->num_vc_dt,
			cid_data->vc_dt[j].vc, cid_data->vc_dt[j].dt,
			reserve->per_port_acquire);
	}

	return 0;
}

int cam_ife_csid_cid_release(
	struct cam_ife_csid_cid_data *cid_data,
	uint32_t hw_idx,
	uint32_t cid)
{
	int i;

	if (!cid_data->cid_cnt) {
		CAM_WARN(CAM_ISP, "CSID[%d] unbalanced cid:%d release",
			hw_idx, cid);
		return 0;
	}

	cid_data->cid_cnt--;

	if (cid_data->cid_cnt == 0) {

		for (i = 0; i < cid_data->num_vc_dt; i++)
			cid_data->vc_dt[i].valid = false;

		cid_data->num_vc_dt = 0;
	}

	return 0;
}

int cam_ife_csid_check_in_port_args(
	struct cam_csid_hw_reserve_resource_args  *reserve,
	uint32_t hw_idx)
{

	if (reserve->in_port->res_type >= CAM_ISP_IFE_IN_RES_MAX) {

		CAM_ERR(CAM_ISP, "CSID:%d  Invalid phy sel %d",
			hw_idx, reserve->in_port->res_type);
		return -EINVAL;
	}

	if (reserve->in_port->lane_type >= CAM_ISP_LANE_TYPE_MAX &&
		reserve->in_port->res_type != CAM_ISP_IFE_IN_RES_TPG) {

		CAM_ERR(CAM_ISP, "CSID:%d  Invalid lane type %d",
			hw_idx, reserve->in_port->lane_type);
		return -EINVAL;
	}

	if ((reserve->in_port->lane_type ==  CAM_ISP_LANE_TYPE_DPHY &&
		reserve->in_port->lane_num > 4) &&
		reserve->in_port->res_type != CAM_ISP_IFE_IN_RES_TPG) {

		CAM_ERR(CAM_ISP, "CSID:%d Invalid lane num %d",
			hw_idx, reserve->in_port->lane_num);
		return -EINVAL;
	}

	if ((reserve->in_port->lane_type == CAM_ISP_LANE_TYPE_CPHY &&
		reserve->in_port->lane_num > 3) &&
		reserve->in_port->res_type != CAM_ISP_IFE_IN_RES_TPG) {

		CAM_ERR(CAM_ISP, " CSID:%d Invalid lane type %d & num %d",
			hw_idx,
			reserve->in_port->lane_type,
			reserve->in_port->lane_num);
		return -EINVAL;
	}

	if ((reserve->res_id ==  CAM_IFE_PIX_PATH_RES_IPP ||
		reserve->res_id == CAM_IFE_PIX_PATH_RES_PPP) &&
		(cam_ife_csid_is_pix_res_format_supported(
			reserve->in_port->format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0]))) {
		CAM_ERR(CAM_ISP, "CSID %d, res_id %d, unsupported format %d",
			hw_idx,
			reserve->res_id,
			reserve->in_port->format[CAM_IFE_CSID_MULTI_VC_DT_GRP_0]);
		return -EINVAL;
	}

	return 0;
}

int cam_ife_csid_get_rt_irq_idx(
	uint32_t irq_reg, uint32_t num_ipp,
	uint32_t num_ppp, uint32_t num_rdi)
{
	int rt_irq_reg_idx = -EINVAL;

	switch (irq_reg) {
	case CAM_IFE_CSID_IRQ_REG_IPP:
		rt_irq_reg_idx = CAM_IFE_CSID_IRQ_REG_RX +
			num_rdi + 1;
		break;
	case CAM_IFE_CSID_IRQ_REG_PPP:
		rt_irq_reg_idx = CAM_IFE_CSID_IRQ_REG_RX +
			num_rdi + num_ipp + 1;
		break;
	case CAM_IFE_CSID_IRQ_REG_RDI_0:
	case CAM_IFE_CSID_IRQ_REG_RDI_1:
	case CAM_IFE_CSID_IRQ_REG_RDI_2:
	case CAM_IFE_CSID_IRQ_REG_RDI_3:
	case CAM_IFE_CSID_IRQ_REG_RDI_4:
		rt_irq_reg_idx = irq_reg;
		break;
	case CAM_IFE_CSID_IRQ_REG_UDI_0:
	case CAM_IFE_CSID_IRQ_REG_UDI_1:
	case CAM_IFE_CSID_IRQ_REG_UDI_2:
		rt_irq_reg_idx = CAM_IFE_CSID_IRQ_REG_RX +
			num_rdi + num_ipp + num_ppp +
			(irq_reg - CAM_IFE_CSID_IRQ_REG_UDI_0) + 1;
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid irq reg %d", irq_reg);
		break;
	}

	return rt_irq_reg_idx;
}

int cam_ife_csid_convert_res_to_irq_reg(uint32_t res_id)
{
	switch (res_id) {

	case CAM_IFE_PIX_PATH_RES_RDI_0:
		return CAM_IFE_CSID_IRQ_REG_RDI_0;
	case CAM_IFE_PIX_PATH_RES_RDI_1:
		return CAM_IFE_CSID_IRQ_REG_RDI_1;
	case CAM_IFE_PIX_PATH_RES_RDI_2:
		return CAM_IFE_CSID_IRQ_REG_RDI_2;
	case CAM_IFE_PIX_PATH_RES_RDI_3:
		return CAM_IFE_CSID_IRQ_REG_RDI_3;
	case CAM_IFE_PIX_PATH_RES_RDI_4:
		return CAM_IFE_CSID_IRQ_REG_RDI_4;
	case CAM_IFE_PIX_PATH_RES_IPP:
		return CAM_IFE_CSID_IRQ_REG_IPP;
	case CAM_IFE_PIX_PATH_RES_PPP:
		return CAM_IFE_CSID_IRQ_REG_PPP;
	case CAM_IFE_PIX_PATH_RES_UDI_0:
		return CAM_IFE_CSID_IRQ_REG_UDI_0;
	case CAM_IFE_PIX_PATH_RES_UDI_1:
		return CAM_IFE_CSID_IRQ_REG_UDI_1;
	case CAM_IFE_PIX_PATH_RES_UDI_2:
		return CAM_IFE_CSID_IRQ_REG_UDI_2;
	default:
		return CAM_IFE_CSID_IRQ_REG_MAX;
	}
}

const char *cam_ife_csid_reset_type_to_string(enum cam_ife_csid_reset_type reset_type)
{
	switch (reset_type) {
	case CAM_IFE_CSID_RESET_GLOBAL: return "global";
	case CAM_IFE_CSID_RESET_PATH: return "path";
	default: return "invalid";
	}
}

int cam_ife_csid_get_base(struct cam_hw_soc_info *soc_info,
	uint32_t base_id, void *cmd_args, size_t arg_size)
{
	struct cam_isp_hw_get_cmd_update *cdm_args  = cmd_args;
	struct cam_cdm_utils_ops         *cdm_util_ops = NULL;
	size_t                           size = 0;
	uint32_t                          mem_base = 0;
	struct cam_csid_soc_private      *soc_private;


	if (arg_size != sizeof(struct cam_isp_hw_get_cmd_update)) {
		CAM_ERR(CAM_ISP, "Error, Invalid cmd size");
		return -EINVAL;
	}

	if (!cdm_args || !cdm_args->res || !soc_info) {
		CAM_ERR(CAM_ISP, "Error, Invalid args");
		return -EINVAL;
	}

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "soc_private is null");
		return -EINVAL;
	}

	cdm_util_ops =
		(struct cam_cdm_utils_ops *)cdm_args->res->cdm_ops;

	if (!cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid CDM ops");
		return -EINVAL;
	}

	size = cdm_util_ops->cdm_required_size_changebase();
	/* since cdm returns dwords, we need to convert it into bytes */
	if ((size * 4) > cdm_args->cmd.size) {
		CAM_ERR(CAM_ISP, "buf size:%d is not sufficient, expected: %d",
			cdm_args->cmd.size, size);
		return -EINVAL;
	}

	mem_base = CAM_SOC_GET_REG_MAP_CAM_BASE(soc_info, base_id);
	if (cdm_args->cdm_id == CAM_CDM_RT) {
		if (!soc_private->rt_wrapper_base) {
			CAM_ERR(CAM_ISP, "rt_wrapper_base_addr is null");
			return -EINVAL;
		}

		mem_base -= soc_private->rt_wrapper_base;
	}

	CAM_DBG(CAM_ISP, "core %d mem_base 0x%x, cdm_id:%u",
		soc_info->index, mem_base, cdm_args->cdm_id);

	cdm_util_ops->cdm_write_changebase(cdm_args->cmd.cmd_buf_addr, mem_base);
	cdm_args->cmd.used_bytes = (size * 4);

	return 0;
}
const uint8_t **cam_ife_csid_get_irq_reg_tag_ptr(void)
{
	return cam_ife_csid_irq_reg_tag;
}


