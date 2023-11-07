// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */


#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <media/cam_isp.h>

#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_hw_intf.h"
#include "cam_ife_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_irq_controller.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_vfe_core.h"
#include "cam_vfe_soc.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "cam_trace.h"
#include "cam_smmu_api.h"
#include "cam_common_util.h"
#include "cam_compat.h"
#include "cam_rpmsg.h"
#include "cam_req_mgr_workq.h"

static const char drv_name[] = "vfe_bus";

#define CAM_VFE_BUS_VER3_IRQ_REG0                0
#define CAM_VFE_BUS_VER3_IRQ_REG1                1
#define CAM_VFE_BUS_VER3_IRQ_MAX                 2

#define CAM_VFE_RDI_BUS_DEFAULT_WIDTH               0xFFFF
#define CAM_VFE_RDI_BUS_DEFAULT_STRIDE              0xFFFF

struct cam_vfe_bus_ver3_comp_grp_acquire_args {
	enum cam_vfe_bus_ver3_comp_grp_type  comp_grp_id;
	uint64_t                             composite_mask;
};

struct cam_vfe_bus_ver3_comp_grp_data {
	enum cam_vfe_bus_ver3_comp_grp_type          comp_grp_type;
	struct cam_vfe_bus_ver3_common_data         *common_data;

	uint64_t                                     composite_mask;
	uint32_t                                     is_master;
	uint32_t                                     is_dual;
	uint32_t                                     dual_slave_core;
	uint32_t                                     intra_client_mask;
	uint32_t                                     addr_sync_mode;

	uint32_t                                     acquire_dev_cnt;
	uint32_t                                     irq_trigger_cnt;
	uint32_t                                     ubwc_static_ctrl;
};

struct cam_vfe_bus_ver3_priv {
	struct cam_vfe_bus_ver3_common_data common_data;
	uint32_t                            num_client;
	uint32_t                            num_out;
	uint32_t                            num_comp_grp;
	uint32_t                            bus_error_irq_mask[CAM_VFE_BUS_VER3_ERR_IRQ_REG_MAX];
	uint32_t                            top_irq_shift;

	struct cam_isp_resource_node       *bus_client;
	struct cam_isp_resource_node       *comp_grp;
	struct cam_isp_resource_node       *vfe_out;
	uint32_t                            vfe_out_map_outtype[CAM_VFE_BUS_VER3_VFE_OUT_MAX];

	struct list_head                    free_comp_grp;
	struct list_head                    used_comp_grp;

	int                                 bus_irq_handle;
	int                                 rup_irq_handle;
	int                                 error_irq_handle;
	void                               *workq_info;
	uint32_t                            max_out_res;
	uint32_t                            num_cons_err;
	struct cam_vfe_constraint_error_info      *constraint_error_list;
};

static void cam_vfe_bus_ver3_unsubscribe_init_irq(
	struct cam_vfe_bus_ver3_priv          *bus_priv);

static int cam_vfe_bus_ver3_subscribe_init_irq(
	struct cam_vfe_bus_ver3_priv          *bus_priv);

static int cam_vfe_bus_ver3_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size);

static int cam_vfe_bus_ver3_get_evt_payload(
	struct cam_vfe_bus_ver3_common_data  *common_data,
	struct cam_vfe_bus_irq_evt_payload  **evt_payload)
{
	int rc;

	spin_lock(&common_data->spin_lock);

	if (!common_data->hw_init) {
		*evt_payload = NULL;
		CAM_ERR_RATE_LIMIT(CAM_ISP, "VFE:%d Bus uninitialized",
			common_data->core_index);
		rc = -EPERM;
		goto done;
	}

	if (list_empty(&common_data->free_payload_list)) {
		*evt_payload = NULL;
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free BUS event payload");
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(&common_data->free_payload_list,
		struct cam_vfe_bus_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	rc = 0;
done:
	spin_unlock(&common_data->spin_lock);
	return rc;
}

static int cam_vfe_bus_ver3_put_evt_payload(
	struct cam_vfe_bus_ver3_common_data     *common_data,
	struct cam_vfe_bus_irq_evt_payload     **evt_payload)
{
	unsigned long flags;

	if (!common_data) {
		CAM_ERR(CAM_ISP, "Invalid param common_data NULL");
		return -EINVAL;
	}

	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&common_data->spin_lock, flags);
	if (common_data->hw_init)
		list_add_tail(&(*evt_payload)->list,
			&common_data->free_payload_list);
	spin_unlock_irqrestore(&common_data->spin_lock, flags);

	*evt_payload = NULL;

	CAM_DBG(CAM_ISP, "Done");
	return 0;
}

static enum cam_vfe_bus_ver3_vfe_out_type
	cam_vfe_bus_ver3_get_out_res_id_and_index(
	struct cam_vfe_bus_ver3_priv  *bus_priv,
	uint32_t res_type, uint32_t  *index)
{
	uint32_t  vfe_out_type;

	switch (res_type) {
	case CAM_ISP_IFE_OUT_RES_FULL:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_FULL;
		break;
	case CAM_ISP_IFE_OUT_RES_DS4:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_DS4;
		break;
	case CAM_ISP_IFE_OUT_RES_DS16:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_DS16;
		break;
	case CAM_ISP_IFE_OUT_RES_FD:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_FD;
		break;
	case CAM_ISP_IFE_OUT_RES_RAW_DUMP:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP;
		break;
	case CAM_ISP_IFE_OUT_RES_2PD:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_2PD;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_0:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_RDI0;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_1:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_RDI1;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_2:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_RDI2;
		break;
	case CAM_ISP_IFE_OUT_RES_RDI_3:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_RDI3;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BE:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BHIST:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BHIST;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_TL_BG:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_BF:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_AWB_BG:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_BHIST:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_RS:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_RS;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_CS:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_CS;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_IHIST:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST;
		break;
	case CAM_ISP_IFE_OUT_RES_FULL_DISP:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP;
		break;
	case CAM_ISP_IFE_OUT_RES_DS4_DISP:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP;
		break;
	case CAM_ISP_IFE_OUT_RES_DS16_DISP:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP;
		break;
	case CAM_ISP_IFE_OUT_RES_LCR:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_LCR;
		break;
	case CAM_ISP_IFE_OUT_RES_AWB_BFW:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_AWB_BFW;
		break;
	case CAM_ISP_IFE_OUT_RES_SPARSE_PD:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_SPARSE_PD;
		break;
	case CAM_ISP_IFE_OUT_RES_PREPROCESS_2PD:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_2PD;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_AEC_BE:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AEC_BE;
		break;
	case CAM_ISP_IFE_OUT_RES_LTM_STATS:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_LTM_STATS;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_GTM_BHIST:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_GTM_BHIST;
		break;
	case CAM_ISP_IFE_LITE_OUT_RES_STATS_BG:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BG;
		break;
	case CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_CAF:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_CAF;
		break;
	case CAM_ISP_IFE_OUT_RES_STATS_BAYER_RS:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BAYER_RS;
		break;
	case CAM_ISP_IFE_OUT_RES_PDAF_PARSED_DATA:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_PDAF_PARSED;
		break;
	case CAM_ISP_IFE_LITE_OUT_RES_STATS_BHIST:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_STATS_LITE_BHIST;
		break;
	case CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW1:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW1;
		break;
	case CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW2:
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW2;
		break;
	default:
		CAM_WARN(CAM_ISP, "Invalid isp res id: %d , assigning max",
			res_type);
		vfe_out_type = CAM_VFE_BUS_VER3_VFE_OUT_MAX;
		*index = CAM_VFE_BUS_VER3_VFE_OUT_MAX;
		return CAM_VFE_BUS_VER3_VFE_OUT_MAX;
	}
	*index = bus_priv->vfe_out_map_outtype[vfe_out_type];

	return vfe_out_type;
}

static int cam_vfe_bus_ver3_get_comp_vfe_out_res_id_list(
	uint64_t comp_mask, uint32_t *out_list, int *num_out)
{
	int count = 0;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_RDI0)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_RDI_0;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_RDI1)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_RDI_1;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_RDI2)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_RDI_2;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_RDI3)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_RDI_3;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_FULL)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_FULL;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_DS4)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_DS4;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_DS16)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_DS16;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_RAW_DUMP;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_FD)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_FD;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_PDAF)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_PDAF;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_HDR_BE;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BHIST)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_HDR_BHIST;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_TL_BG;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_BF;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_AWB_BG;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_BHIST;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_RS)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_RS;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_CS)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_CS;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_IHIST;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_FULL_DISP;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_DS4_DISP;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_DS16_DISP;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_2PD)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_2PD;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_LCR)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_LCR;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_AWB_BFW)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_AWB_BFW;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_2PD)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_PREPROCESS_2PD;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_AEC_BE)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_AEC_BE;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_LTM_STATS)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_LTM_STATS;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_GTM_BHIST)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_GTM_BHIST;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_BG)))
		out_list[count++] = CAM_ISP_IFE_LITE_OUT_RES_STATS_BG;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW)))
		out_list[count++] = CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_SPARSE_PD)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_SPARSE_PD;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_CAF)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_CAF;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_BAYER_RS)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_STATS_BAYER_RS;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_PDAF_PARSED)))
		out_list[count++] = CAM_ISP_IFE_OUT_RES_PDAF_PARSED_DATA;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_STATS_LITE_BHIST)))
		out_list[count++] = CAM_ISP_IFE_LITE_OUT_RES_STATS_BHIST;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW1)))
		out_list[count++] = CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW1;

	if (comp_mask & (BIT_ULL(CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW2)))
		out_list[count++] = CAM_ISP_IFE_LITE_OUT_RES_PREPROCESS_RAW2;

	*num_out = count;
	return 0;
}

static enum cam_vfe_bus_ver3_packer_format
	cam_vfe_bus_ver3_get_packer_fmt(uint32_t out_fmt, int wm_index)
{
	switch (out_fmt) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_PLAIN16_8:
	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_PD8:
		return PACKER_FMT_VER3_PLAIN_128;
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_PLAIN8:
		return PACKER_FMT_VER3_PLAIN_8;
	case CAM_FORMAT_MIPI_RAW_10:
		return PACKER_FMT_VER3_MIPI10;
	case CAM_FORMAT_MIPI_RAW_12:
		return PACKER_FMT_VER3_MIPI12;
	case CAM_FORMAT_MIPI_RAW_14:
		return PACKER_FMT_VER3_MIPI14;
	case CAM_FORMAT_MIPI_RAW_20:
		return PACKER_FMT_VER3_MIPI20;
	case CAM_FORMAT_NV21:
		if ((wm_index == 1) || (wm_index == 3) || (wm_index == 5))
			return PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10_ODD_EVEN;
	case CAM_FORMAT_NV12:
	case CAM_FORMAT_UBWC_NV12:
	case CAM_FORMAT_UBWC_NV12_4R:
	case CAM_FORMAT_Y_ONLY:
		return PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10;
	case CAM_FORMAT_PLAIN16_10:
		return PACKER_FMT_VER3_PLAIN_16_10BPP;
	case CAM_FORMAT_PLAIN16_12:
		return PACKER_FMT_VER3_PLAIN_16_12BPP;
	case CAM_FORMAT_PLAIN16_14:
		return PACKER_FMT_VER3_PLAIN_16_14BPP;
	case CAM_FORMAT_PLAIN16_16:
		return PACKER_FMT_VER3_PLAIN_16_16BPP;
	case CAM_FORMAT_PLAIN32:
	case CAM_FORMAT_ARGB:
		return PACKER_FMT_VER3_PLAIN_32;
	case CAM_FORMAT_PLAIN32_20:
		return PACKER_FMT_VER3_PLAIN32_20BPP;
	case CAM_FORMAT_PLAIN64:
	case CAM_FORMAT_ARGB_16:
	case CAM_FORMAT_PD10:
		return PACKER_FMT_VER3_PLAIN_64;
	case CAM_FORMAT_UBWC_TP10:
	case CAM_FORMAT_TP10:
		return PACKER_FMT_VER3_TP_10;
	default:
		return PACKER_FMT_VER3_MAX;
	}
}

static int cam_vfe_bus_ver3_handle_rup_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                     rc;
	int                                         i;
	struct cam_isp_resource_node               *vfe_out = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data       *rsrc_data = NULL;
	struct cam_vfe_bus_irq_evt_payload         *evt_payload;
	uint32_t irq_status;

	vfe_out = th_payload->handler_priv;
	if (!vfe_out) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No resource");
		return -ENODEV;
	}

	rsrc_data = vfe_out->res_priv;

	CAM_DBG(CAM_ISP, "VFE:%d Bus IRQ status_0: 0x%X",
		rsrc_data->common_data->core_index,
		th_payload->evt_status_arr[0]);

	rc  = cam_vfe_bus_ver3_get_evt_payload(rsrc_data->common_data,
		&evt_payload);

	if (rc) {
		CAM_INFO_RATE_LIMIT(CAM_ISP,
			"VFE:%d Bus IRQ status_0: 0x%X",
			rsrc_data->common_data->core_index,
			th_payload->evt_status_arr[0]);
		return rc;
	}

	evt_payload->core_index = rsrc_data->common_data->core_index;
	evt_payload->evt_id  = evt_id;
	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	irq_status =
		th_payload->evt_status_arr[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];

	trace_cam_log_event("RUP", "RUP_IRQ", irq_status, 0);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static void cam_vfe_bus_ver3_print_constraint_errors(
	struct cam_vfe_bus_ver3_priv *bus_priv,
	uint8_t *wm_name,
	uint32_t constraint_errors)
{
	uint32_t i;

	CAM_INFO_RATE_LIMIT(CAM_ISP, "Constraint violation bitflags: 0x%X",
		constraint_errors);

	for (i = 0; i < bus_priv->num_cons_err; i++) {
		if (bus_priv->constraint_error_list[i].bitmask &
			constraint_errors) {
			CAM_INFO(CAM_ISP, "WM:%s %s",
				wm_name, bus_priv->constraint_error_list[i]
				.error_description);
		}
	}
}

static void cam_vfe_bus_ver3_get_constraint_errors(
	struct cam_vfe_bus_ver3_priv *bus_priv)
{
	uint32_t i, j, constraint_errors;
	uint8_t *wm_name = NULL;
	struct cam_isp_resource_node              *out_rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data      *out_rsrc_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data  *wm_data   = NULL;

	for (i = 0; i < bus_priv->num_out; i++) {
		out_rsrc_node = &bus_priv->vfe_out[i];
		if (!out_rsrc_node || !out_rsrc_node->res_priv) {
			CAM_DBG(CAM_ISP,
				"Vfe out:%d out rsrc node or data is NULL", i);
			continue;
		}
		out_rsrc_data = out_rsrc_node->res_priv;
		for (j = 0; j < out_rsrc_data->num_wm; j++) {
			wm_data = out_rsrc_data->wm_res[j].res_priv;
			wm_name = out_rsrc_data->wm_res[j].res_name;
			if (wm_data) {
				constraint_errors = cam_io_r_mb(
					bus_priv->common_data.mem_base +
					wm_data->hw_regs->debug_status_1);
				if (!constraint_errors)
					continue;

				cam_vfe_bus_ver3_print_constraint_errors(
					bus_priv, wm_name, constraint_errors);
			}
		}
	}
}

static int cam_vfe_bus_ver3_handle_rup_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int                                   ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_vfe_bus_irq_evt_payload   *payload;
	struct cam_isp_resource_node         *vfe_out = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data = NULL;
	struct cam_isp_hw_event_info          evt_info;
	uint32_t                              irq_status;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	payload = evt_payload_priv;
	vfe_out = handler_priv;
	rsrc_data = vfe_out->res_priv;

	if (!rsrc_data->common_data->event_cb) {
		CAM_ERR(CAM_ISP, "Callback to HW MGR not found");
		return ret;
	}

	irq_status = payload->irq_reg_val[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];

	evt_info.hw_type  = CAM_ISP_HW_TYPE_VFE;
	evt_info.hw_idx = rsrc_data->common_data->core_index;
	evt_info.res_type = CAM_ISP_RESOURCE_VFE_IN;

	if (!rsrc_data->common_data->is_lite) {
		if (irq_status & 0x1) {
			CAM_DBG(CAM_ISP, "VFE:%d Received CAMIF RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_CAMIF;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}

		if (irq_status & 0x2) {
			CAM_DBG(CAM_ISP, "VFE:%d Received PDLIB RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_PDLIB;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}

		if (irq_status & 0x4)
			CAM_DBG(CAM_ISP, "VFE:%d Received LCR RUP",
				evt_info.hw_idx);

		if (irq_status & 0x8) {
			CAM_DBG(CAM_ISP, "VFE:%d Received RDI0 RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_RDI0;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}

		if (irq_status & 0x10) {
			CAM_DBG(CAM_ISP, "VFE:%d Received RDI1 RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_RDI1;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}

		if (irq_status & 0x20) {
			CAM_DBG(CAM_ISP, "VFE:%d Received RDI2 RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_RDI2;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}
	} else {
		if (irq_status & 0x1) {
			CAM_DBG(CAM_ISP, "VFE:%d Received RDI0 RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_RDI0;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}

		if (irq_status & 0x2) {
			CAM_DBG(CAM_ISP, "VFE:%d Received RDI1 RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_RDI1;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}

		if (irq_status & 0x4) {
			CAM_DBG(CAM_ISP, "VFE:%d Received RDI2 RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_RDI2;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}

		if (irq_status & 0x8) {
			CAM_DBG(CAM_ISP, "VFE:%d Received RDI3 RUP",
				evt_info.hw_idx);
			evt_info.res_id = CAM_ISP_HW_VFE_IN_RDI3;
			rsrc_data->common_data->event_cb(
				rsrc_data->priv, CAM_ISP_HW_EVENT_REG_UPDATE,
				(void *)&evt_info);
		}
	}

	ret = CAM_VFE_IRQ_STATUS_SUCCESS;

	CAM_DBG(CAM_ISP,
		"VFE:%d Bus RUP IRQ status_0:0x%X rc:%d",
		evt_info.hw_idx, CAM_ISP_HW_EVENT_REG_UPDATE, irq_status, ret);

	cam_vfe_bus_ver3_put_evt_payload(rsrc_data->common_data, &payload);

	return ret;
}

static int cam_vfe_bus_ver3_config_rdi_wm(
	struct cam_vfe_bus_ver3_wm_resource_data  *rsrc_data)
{

	rsrc_data->pack_fmt = PACKER_FMT_VER3_PLAIN_128;
	switch (rsrc_data->format) {
	case CAM_FORMAT_MIPI_RAW_10:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 5) / 4, 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}

		if (rsrc_data->use_wm_pack) {
			rsrc_data->pack_fmt = PACKER_FMT_VER3_MIPI10;
			if (rsrc_data->default_line_based)
				rsrc_data->width = ALIGNUP((rsrc_data->acquired_width), 16);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_6:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 3) / 4, 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_YUV422:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP(rsrc_data->width, 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_12:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 3) / 2, 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}

		if (rsrc_data->use_wm_pack) {
			rsrc_data->pack_fmt = PACKER_FMT_VER3_MIPI12;
			if (rsrc_data->default_line_based)
				rsrc_data->width = ALIGNUP((rsrc_data->acquired_width), 16);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_14:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 7) / 2, 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}

		if (rsrc_data->use_wm_pack) {
			rsrc_data->pack_fmt = PACKER_FMT_VER3_MIPI14;
			if (rsrc_data->default_line_based)
				rsrc_data->width = ALIGNUP((rsrc_data->acquired_width), 16);
		}
		break;
	case CAM_FORMAT_MIPI_RAW_16:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 2), 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_MIPI_RAW_20:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 5) / 2, 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}

		if (rsrc_data->use_wm_pack)
			rsrc_data->pack_fmt = PACKER_FMT_VER3_MIPI20;
		break;
	case CAM_FORMAT_PLAIN128:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 16), 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_PLAIN32_20:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 4), 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	case CAM_FORMAT_PLAIN8:
		rsrc_data->en_cfg = 0x1;
		rsrc_data->stride = rsrc_data->width * 2;
		break;
	case CAM_FORMAT_PLAIN16_10:
	case CAM_FORMAT_PLAIN16_12:
	case CAM_FORMAT_PLAIN16_14:
	case CAM_FORMAT_PLAIN16_16:
		rsrc_data->width =
			ALIGNUP(rsrc_data->width * 2, 16) / 16;
		rsrc_data->en_cfg = 0x1;
		break;
	case CAM_FORMAT_PLAIN64:
		rsrc_data->width =
			ALIGNUP(rsrc_data->width * 8, 16) / 16;
		rsrc_data->en_cfg = 0x1;
		break;
	case CAM_FORMAT_YUV422_10:
		if (rsrc_data->default_line_based) {
			rsrc_data->en_cfg = 0x1;
			rsrc_data->width =
				ALIGNUP((rsrc_data->width * 5) / 4, 16) / 16;
		} else {
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		}
		break;
	default:
		CAM_ERR(CAM_ISP, "Unsupported RDI format %d",
			rsrc_data->format);
		return -EINVAL;
	}

	return 0;
}

static int cam_vfe_bus_ver3_res_update_config_wm(
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv,
	enum cam_vfe_bus_ver3_vfe_out_type      vfe_out_res_id,
	enum cam_vfe_bus_plane_type             plane,
	struct cam_isp_resource_node           *wm_res,
	enum cam_vfe_bus_ver3_comp_grp_type    *comp_grp_id,
	char                                   *wm_mode,
	int32_t                                 wm_mode_size)
{
	int32_t rc = 0;
	struct cam_vfe_bus_ver3_wm_resource_data  *rsrc_data = NULL;

	rsrc_data = wm_res->res_priv;

	if ((vfe_out_res_id >= CAM_VFE_BUS_VER3_VFE_OUT_RDI0) &&
		(vfe_out_res_id <= CAM_VFE_BUS_VER3_VFE_OUT_RDI3)) {
		rc = cam_vfe_bus_ver3_config_rdi_wm(rsrc_data);
		if (rc)
			return rc;
	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP) {

		rsrc_data->stride = rsrc_data->width;
		rsrc_data->en_cfg = 0x1;
		/* LSB aligned */
		rsrc_data->pack_fmt |= (1 <<
				ver3_bus_priv->common_data.pack_align_shift);

	} else if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_FULL) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_FD) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP)) {

		switch (rsrc_data->format) {
		case CAM_FORMAT_UBWC_NV12_4R:
			rsrc_data->en_ubwc = 1;
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_UBWC_NV12:
			rsrc_data->en_ubwc = 1;
			/* Fall through for NV12 */
		case CAM_FORMAT_NV21:
		case CAM_FORMAT_NV12:
		case CAM_FORMAT_Y_ONLY:
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_UBWC_TP10:
			rsrc_data->en_ubwc = 1;
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_TP10:
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_PLAIN16_10:
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->height /= 2;
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}
		rsrc_data->en_cfg = 0x1;

	} else if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_STATS_CAF)) {

		rsrc_data->en_cfg = (0x1 << 16) | 0x1;

	} else if (((vfe_out_res_id >= CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE) &&
		(vfe_out_res_id <= CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST)) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_STATS_BAYER_RS) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_STATS_LITE_BHIST)) {

		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_2PD) {

		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_LCR) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN16_16:
			rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
			rsrc_data->en_cfg = 0x1;
			/* LSB aligned */
			rsrc_data->pack_fmt |= (1 <<
				ver3_bus_priv->common_data.pack_align_shift);

			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
				rsrc_data->en_cfg = 0x1;
				/* LSB aligned */
				rsrc_data->pack_fmt |= (1 <<
					ver3_bus_priv->common_data.pack_align_shift);
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_SPARSE_PD) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN8:
			rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
			rsrc_data->en_cfg = 0x1;
			break;
		case CAM_FORMAT_PLAIN16_8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
			rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
			rsrc_data->en_cfg = 0x1;
			/* LSB aligned */
			rsrc_data->pack_fmt |= (1 <<
				ver3_bus_priv->common_data.pack_align_shift);
			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
				rsrc_data->en_cfg = 0x1;
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_DS4) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_DS16) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP)) {

		rsrc_data->height = rsrc_data->height / 2;
		rsrc_data->width  = rsrc_data->width / 2;
		rsrc_data->en_cfg = 0x1;

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_AWB_BFW) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN64:
			rsrc_data->width = 0;
			rsrc_data->height = 0;
			rsrc_data->stride = 1;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->width = 0;
				rsrc_data->height = 0;
				rsrc_data->stride = 1;
				rsrc_data->en_cfg = (0x1 << 16) | 0x1;
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_2PD) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN16_16:
			rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
			rsrc_data->en_cfg = 0x1;
			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
				rsrc_data->en_cfg = 0x1;
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_LTM_STATS) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN32:
			rsrc_data->stride = ALIGNUP(rsrc_data->width * 4, 16);
			rsrc_data->en_cfg = 0x1;
			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->stride = ALIGNUP(rsrc_data->width * 4, 16);
				rsrc_data->en_cfg = 0x1;
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_STATS_AEC_BE) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_STATS_GTM_BHIST)) {
		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;
	} else if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW1) ||
		(vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW2)) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
			rsrc_data->width = 0;
			rsrc_data->height = 0;
			rsrc_data->stride = 1;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->width = 0;
				rsrc_data->height = 0;
				rsrc_data->stride = 1;
				rsrc_data->en_cfg = (0x1 << 16) | 0x1;
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_STATS_BG) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN64:
			rsrc_data->width = 0;
			rsrc_data->height = 0;
			rsrc_data->stride = 1;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->width = 0;
				rsrc_data->height = 0;
				rsrc_data->stride = 1;
				rsrc_data->en_cfg = (0x1 << 16) | 0x1;
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_PDAF_PARSED) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN16_16:
			rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
			rsrc_data->en_cfg = 0x1;
			/* LSB aligned */
			rsrc_data->pack_fmt |= (1 <<
				ver3_bus_priv->common_data.pack_align_shift);

			break;
		default:
			if (wm_res->is_per_port_acquire) {
				rsrc_data->stride = ALIGNUP(rsrc_data->width * 2, 8);
				rsrc_data->en_cfg = 0x1;
				/* LSB aligned */
				rsrc_data->pack_fmt |= (1 <<
					ver3_bus_priv->common_data.pack_align_shift);
				break;
			}
			CAM_ERR(CAM_ISP, "Invalid format %d out_type:%d",
				rsrc_data->format, vfe_out_res_id);
			return -EINVAL;
		}

	} else {
		CAM_ERR(CAM_ISP, "Invalid out_type:%d requested",
			vfe_out_res_id);
		return -EINVAL;
	}

	*comp_grp_id = rsrc_data->hw_regs->comp_group;

	switch (rsrc_data->en_cfg) {
	case 0x1:
		strlcpy(wm_mode, "line-based", wm_mode_size);
		break;
	case ((0x1 << 16) | 0x1):
		strlcpy(wm_mode, "frame-based", wm_mode_size);
		break;
	case ((0x2 << 16) | 0x1):
		strlcpy(wm_mode, "index-based", wm_mode_size);
		break;
	}

	return rc;
}

static int cam_vfe_bus_ver3_acquire_wm(
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv,
	struct cam_vfe_hw_vfe_out_acquire_args *out_acq_args,
	void                                   *workq,
	enum cam_vfe_bus_ver3_vfe_out_type      vfe_out_res_id,
	enum cam_vfe_bus_plane_type             plane,
	struct cam_isp_resource_node           *wm_res,
	enum cam_vfe_bus_ver3_comp_grp_type   *comp_grp_id,
	bool is_per_port_acquire)
{
	int32_t wm_idx = 0, rc;
	struct cam_vfe_bus_ver3_wm_resource_data  *rsrc_data = NULL;
	char wm_mode[50];

	memset(wm_mode, '\0', sizeof(wm_mode));

	if (wm_res->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "WM:%d not available state:%d",
			wm_idx, wm_res->res_state);
		return -EALREADY;
	}

	rsrc_data = wm_res->res_priv;
	wm_idx = rsrc_data->index;
	rsrc_data->format = out_acq_args->out_port_info->format;
	rsrc_data->use_wm_pack = out_acq_args->use_wm_pack;
	rsrc_data->pack_fmt = cam_vfe_bus_ver3_get_packer_fmt(rsrc_data->format,
		wm_idx);

	rsrc_data->width = out_acq_args->out_port_info->width;
	rsrc_data->height = out_acq_args->out_port_info->height;
	rsrc_data->acquired_width = out_acq_args->out_port_info->width;
	rsrc_data->acquired_height = out_acq_args->out_port_info->height;
	rsrc_data->is_dual = out_acq_args->is_dual;
	if (ver3_bus_priv->common_data.support_tunneling)
		rsrc_data->tunnel_en = out_acq_args->out_port_info->tunnel_en;

	/* Set WM offset value to default */
	rsrc_data->offset  = 0;
	CAM_DBG(CAM_ISP, "WM:%d width %d height %d per_port_en %d",
		rsrc_data->index, rsrc_data->width,
		rsrc_data->height, is_per_port_acquire);

	if (is_per_port_acquire)
		wm_res->is_per_port_acquire = true;

	rc = cam_vfe_bus_ver3_res_update_config_wm(ver3_bus_priv, vfe_out_res_id,
		plane, wm_res, comp_grp_id, wm_mode, sizeof(wm_mode));
	if (rc)
		return rc;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	wm_res->workq_info = workq;

	CAM_DBG(CAM_ISP,
		"VFE:%d WM:%d %s processed width:%d height:%d stride:%d format:0x%X en_ubwc:%d %s",
		rsrc_data->common_data->core_index, rsrc_data->index,
		wm_res->res_name, rsrc_data->width, rsrc_data->height,
		rsrc_data->stride, rsrc_data->format, rsrc_data->en_ubwc,
		wm_mode);
	return 0;
}

static int cam_vfe_bus_ver3_release_wm(void   *bus_priv,
	struct cam_isp_resource_node     *wm_res)
{
	struct cam_vfe_bus_ver3_wm_resource_data   *rsrc_data =
		wm_res->res_priv;

	rsrc_data->offset = 0;
	rsrc_data->width = 0;
	rsrc_data->height = 0;
	rsrc_data->stride = 0;
	rsrc_data->format = 0;
	rsrc_data->pack_fmt = 0;
	rsrc_data->burst_len = 0;
	rsrc_data->irq_subsample_period = 0;
	rsrc_data->irq_subsample_pattern = 0;
	rsrc_data->framedrop_period = 0;
	rsrc_data->framedrop_pattern = 0;
	rsrc_data->packer_cfg = 0;
	rsrc_data->en_ubwc = 0;
	rsrc_data->h_init = 0;
	rsrc_data->ubwc_meta_addr = 0;
	rsrc_data->ubwc_meta_cfg = 0;
	rsrc_data->ubwc_mode_cfg = 0;
	rsrc_data->ubwc_stats_ctrl = 0;
	rsrc_data->ubwc_ctrl_2 = 0;
	rsrc_data->init_cfg_done = false;
	rsrc_data->hfr_cfg_done = false;
	rsrc_data->ubwc_updated = false;
	rsrc_data->en_cfg = 0;
	rsrc_data->is_dual = 0;

	rsrc_data->ubwc_lossy_threshold_0 = 0;
	rsrc_data->ubwc_lossy_threshold_1 = 0;
	rsrc_data->ubwc_offset_lossy_variance = 0;
	rsrc_data->ubwc_bandwidth_limit = 0;
	wm_res->workq_info = NULL;
	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	CAM_DBG(CAM_ISP, "VFE:%d Release WM:%d %s",
		rsrc_data->common_data->core_index, rsrc_data->index,
		wm_res->res_name);

	return 0;
}

static int cam_vfe_bus_ver3_start_wm(struct cam_isp_resource_node *wm_res)
{
	const uint32_t enable_debug_status_1 = 11 << 8;
	int val = 0;
	struct cam_vfe_bus_ver3_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_vfe_bus_ver3_common_data        *common_data =
		rsrc_data->common_data;
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;
	bool disable_ubwc_comp = rsrc_data->common_data->disable_ubwc_comp;

	ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)
		rsrc_data->hw_regs->ubwc_regs;

	cam_io_w(0xf, common_data->mem_base + rsrc_data->hw_regs->burst_limit);

	cam_io_w((rsrc_data->height << 16) | rsrc_data->width,
		common_data->mem_base + rsrc_data->hw_regs->image_cfg_0);
	cam_io_w(rsrc_data->pack_fmt,
		common_data->mem_base + rsrc_data->hw_regs->packer_cfg);

	/* enable ubwc if needed*/
	if (rsrc_data->en_ubwc) {
		if (!ubwc_regs) {
			CAM_ERR(CAM_ISP,
				"ubwc_regs is NULL, VFE:%d WM:%d en_ubwc:%d",
				rsrc_data->common_data->core_index,
				rsrc_data->index, rsrc_data->en_ubwc);
			return -EINVAL;
		}
		val = cam_io_r_mb(common_data->mem_base + ubwc_regs->mode_cfg);
		val |= 0x1;
		if (disable_ubwc_comp) {
			val &= ~ubwc_regs->ubwc_comp_en_bit;
			CAM_DBG(CAM_ISP,
				"Force disable UBWC compression, VFE:%d WM:%d ubwc_mode_cfg: 0x%x",
				rsrc_data->common_data->core_index,
				rsrc_data->index, val);
		}
		cam_io_w_mb(val, common_data->mem_base + ubwc_regs->mode_cfg);
	}

	/* Validate for debugfs and mmu reg info for targets that don't list it */
	if (!(common_data->disable_mmu_prefetch) &&
		(rsrc_data->hw_regs->mmu_prefetch_cfg)) {
		cam_io_w_mb(1, common_data->mem_base +
			rsrc_data->hw_regs->mmu_prefetch_cfg);
		cam_io_w_mb(0xFFFFFFFF, common_data->mem_base +
			rsrc_data->hw_regs->mmu_prefetch_max_offset);
		CAM_DBG(CAM_ISP, "VFE: %d WM: %d MMU prefetch enabled",
			rsrc_data->common_data->core_index,
			rsrc_data->index);
	}

	/* Enable WM */
	cam_io_w_mb(rsrc_data->en_cfg, common_data->mem_base +
		rsrc_data->hw_regs->cfg);

	/* Enable constraint error detection */
	cam_io_w_mb(enable_debug_status_1,
		common_data->mem_base +
		rsrc_data->hw_regs->debug_status_cfg);

	/* enable/disable tunneling feature */
	if ((common_data->support_tunneling) && (rsrc_data->tunnel_en)) {
		val = cam_io_r_mb(common_data->mem_base +
			rsrc_data->common_data->common_reg->tunneling_cfg);
		val |= rsrc_data->tunnel_en << rsrc_data->hw_regs->tunnel_cfg_idx;
		CAM_DBG(CAM_ISP, "WM %d tunn_cfg 0x%x", rsrc_data->index, val);
		cam_io_w_mb(val,
			common_data->mem_base +
			rsrc_data->common_data->common_reg->tunneling_cfg);
	}

	CAM_DBG(CAM_ISP,
		"Start VFE:%d WM:%d %s offset:0x%X en_cfg:0x%X width:%d height:%d tunneling:%d",
		rsrc_data->common_data->core_index, rsrc_data->index, wm_res->res_name,
		(uint32_t) rsrc_data->hw_regs->cfg, rsrc_data->en_cfg, rsrc_data->width,
		rsrc_data->height, rsrc_data->tunnel_en);
	CAM_DBG(CAM_ISP, "WM:%d pk_fmt:%d stride:%d burst len:%d",
		rsrc_data->index, rsrc_data->pack_fmt,
		rsrc_data->stride, 0xF);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}

static int cam_vfe_bus_ver3_stop_wm(struct cam_isp_resource_node *wm_res)
{
	struct cam_vfe_bus_ver3_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_vfe_bus_ver3_common_data        *common_data =
		rsrc_data->common_data;

	/* Disable WM */
	cam_io_w_mb(0x0, common_data->mem_base + rsrc_data->hw_regs->cfg);
	CAM_DBG(CAM_ISP, "Stop VFE:%d WM:%d %s",
		rsrc_data->common_data->core_index, rsrc_data->index,
		wm_res->res_name);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rsrc_data->init_cfg_done = false;
	rsrc_data->hfr_cfg_done = false;
	rsrc_data->ubwc_updated = false;

	return 0;
}

static int cam_vfe_bus_ver3_handle_wm_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_bus_ver3_handle_wm_done_bottom_half(void *wm_node,
	void *evt_payload_priv)
{
	return -EPERM;
}

static int cam_vfe_bus_ver3_init_wm_resource(uint32_t index,
	struct cam_vfe_bus_ver3_priv    *ver3_bus_priv,
	struct cam_vfe_bus_ver3_hw_info *ver3_hw_info,
	struct cam_isp_resource_node    *wm_res,
	uint8_t                         *wm_name,
	uint32_t                         line_based_config)
{
	struct cam_vfe_bus_ver3_wm_resource_data *rsrc_data;

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver3_wm_resource_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CAM_DBG(CAM_ISP, "Failed to alloc for WM res priv");
		return -ENOMEM;
	}
	wm_res->res_priv = rsrc_data;

	rsrc_data->index = index;
	rsrc_data->default_line_based = line_based_config;
	rsrc_data->hw_regs = &ver3_hw_info->bus_client_reg[index];
	rsrc_data->common_data = &ver3_bus_priv->common_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = cam_vfe_bus_ver3_start_wm;
	wm_res->stop = cam_vfe_bus_ver3_stop_wm;
	wm_res->top_half_handler = cam_vfe_bus_ver3_handle_wm_done_top_half;
	wm_res->bottom_half_handler =
		cam_vfe_bus_ver3_handle_wm_done_bottom_half;
	wm_res->hw_intf = ver3_bus_priv->common_data.hw_intf;

	if (wm_name)
		scnprintf(wm_res->res_name, CAM_ISP_RES_NAME_LEN,
			"%s", wm_name);
	return 0;
}

static int cam_vfe_bus_ver3_deinit_wm_resource(
	struct cam_isp_resource_node    *wm_res)
{
	struct cam_vfe_bus_ver3_wm_resource_data *rsrc_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = NULL;
	wm_res->stop = NULL;
	wm_res->top_half_handler = NULL;
	wm_res->bottom_half_handler = NULL;
	wm_res->hw_intf = NULL;

	rsrc_data = wm_res->res_priv;
	wm_res->res_priv = NULL;
	if (!rsrc_data)
		return -ENOMEM;
	kfree(rsrc_data);

	return 0;
}

static bool cam_vfe_bus_ver3_match_comp_grp(
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv,
	struct cam_isp_resource_node          **comp_grp,
	uint32_t                                comp_grp_id)
{
	struct cam_vfe_bus_ver3_comp_grp_data  *rsrc_data = NULL;
	struct cam_isp_resource_node           *comp_grp_local = NULL;

	list_for_each_entry(comp_grp_local,
		&ver3_bus_priv->used_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_type == comp_grp_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			return true;
		}
	}

	list_for_each_entry(comp_grp_local,
		&ver3_bus_priv->free_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_type == comp_grp_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			list_del(&comp_grp_local->list);
			list_add_tail(&comp_grp_local->list,
			&ver3_bus_priv->used_comp_grp);
			return false;
		}
	}

	*comp_grp = NULL;
	return false;
}

static int cam_vfe_bus_ver3_acquire_comp_grp(
	struct cam_vfe_bus_ver3_priv         *ver3_bus_priv,
	void                                *workq,
	uint32_t                             is_dual,
	uint32_t                             is_master,
	struct cam_isp_resource_node       **comp_grp,
	struct cam_vfe_bus_ver3_comp_grp_acquire_args *comp_acq_args,
	bool                                 is_per_port_acquire)
{
	int rc = 0;
	struct cam_isp_resource_node           *comp_grp_local = NULL;
	struct cam_vfe_bus_ver3_comp_grp_data  *rsrc_data = NULL;
	bool previously_acquired = false;

	/* Check if matching comp_grp has already been acquired */
	previously_acquired = cam_vfe_bus_ver3_match_comp_grp(
		ver3_bus_priv, &comp_grp_local, comp_acq_args->comp_grp_id);

	if (!comp_grp_local) {
		CAM_ERR(CAM_ISP, "Invalid comp_grp:%d",
			comp_acq_args->comp_grp_id);
		return -ENODEV;
	}

	rsrc_data = comp_grp_local->res_priv;

	if (!previously_acquired) {
		rsrc_data->intra_client_mask = 0x1;
		comp_grp_local->workq_info = workq;
		comp_grp_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

		rsrc_data->is_master = is_master;
		rsrc_data->is_dual = is_dual;

		if (is_master)
			rsrc_data->addr_sync_mode = 0;
		else
			rsrc_data->addr_sync_mode = 1;

	} else {
		rsrc_data = comp_grp_local->res_priv;
		/* Do not support runtime change in composite mask */
		if (comp_grp_local->res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING) {
			CAM_ERR(CAM_ISP, "Invalid State %d comp_grp:%u",
				comp_grp_local->res_state,
				rsrc_data->comp_grp_type);
			return -EBUSY;
		}
	}

	if (is_per_port_acquire)
		comp_grp_local->is_per_port_acquire = true;

	CAM_DBG(CAM_ISP, "Acquire VFE:%d comp_grp:%u is_per_port_acquire :%d",
		rsrc_data->common_data->core_index,
		rsrc_data->comp_grp_type, comp_grp_local->is_per_port_acquire);

	rsrc_data->acquire_dev_cnt++;
	rsrc_data->composite_mask |= comp_acq_args->composite_mask;
	*comp_grp = comp_grp_local;

	return rc;
}

static int cam_vfe_bus_ver3_release_comp_grp(
	struct cam_vfe_bus_ver3_priv         *ver3_bus_priv,
	struct cam_isp_resource_node         *in_comp_grp)
{
	struct cam_isp_resource_node           *comp_grp = NULL;
	struct cam_vfe_bus_ver3_comp_grp_data  *in_rsrc_data = NULL;
	int match_found = 0;

	if (!in_comp_grp) {
		CAM_ERR(CAM_ISP, "Invalid Params comp_grp %pK", in_comp_grp);
		return -EINVAL;
	}

	if (in_comp_grp->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "Already released comp_grp");
		return 0;
	}

	if (in_comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP, "Invalid State %d",
			in_comp_grp->res_state);
		return -EBUSY;
	}

	in_rsrc_data = in_comp_grp->res_priv;
	CAM_DBG(CAM_ISP, "Release VFE:%d comp_grp:%u",
		ver3_bus_priv->common_data.core_index,
		in_rsrc_data->comp_grp_type);

	list_for_each_entry(comp_grp, &ver3_bus_priv->used_comp_grp, list) {
		if (comp_grp == in_comp_grp) {
			match_found = 1;
			break;
		}
	}

	if (!match_found) {
		CAM_ERR(CAM_ISP, "Could not find comp_grp:%u",
			in_rsrc_data->comp_grp_type);
		return -ENODEV;
	}

	in_rsrc_data->acquire_dev_cnt--;
	if (in_rsrc_data->acquire_dev_cnt == 0) {
		list_del(&comp_grp->list);

		in_rsrc_data->dual_slave_core = CAM_VFE_BUS_VER3_VFE_CORE_MAX;
		in_rsrc_data->addr_sync_mode = 0;
		in_rsrc_data->composite_mask = 0;

		comp_grp->workq_info = NULL;
		comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

		list_add_tail(&comp_grp->list, &ver3_bus_priv->free_comp_grp);
	}

	return 0;
}

static int cam_vfe_bus_ver3_start_comp_grp(
	struct cam_vfe_bus_ver3_vfe_out_data *vfe_out_data, uint32_t *bus_irq_reg_mask)
{
	int rc = 0;
	uint32_t val;
	struct cam_isp_resource_node *comp_grp = vfe_out_data->comp_grp;
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data =
		vfe_out_data->comp_grp->res_priv;
	struct cam_vfe_bus_ver3_common_data *common_data = rsrc_data->common_data;

	CAM_DBG(CAM_ISP,
		"Start VFE:%d comp_grp:%d streaming state:%d comp_mask:0x%X",
		rsrc_data->common_data->core_index,
		rsrc_data->comp_grp_type, comp_grp->res_state,
		rsrc_data->composite_mask);

	if (comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		return 0;

	if (!common_data->comp_config_needed)
		goto skip_comp_cfg;

	if (rsrc_data->is_dual) {
		if (rsrc_data->is_master) {
			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val |= (0x1 << (rsrc_data->comp_grp_type + 14));

			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_1);

			val |= (0x1 << rsrc_data->comp_grp_type);

			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
		} else {
			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val |= (0x1 << rsrc_data->comp_grp_type);
			val |= (0x1 << (rsrc_data->comp_grp_type + 14));

			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_1);

			val |= (0x1 << rsrc_data->comp_grp_type);

			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
		}
	}

skip_comp_cfg:

	if (rsrc_data->ubwc_static_ctrl) {
		val = cam_io_r_mb(common_data->mem_base +
			common_data->common_reg->ubwc_static_ctrl);
		val |= rsrc_data->ubwc_static_ctrl;
		cam_io_w_mb(val, common_data->mem_base +
			common_data->common_reg->ubwc_static_ctrl);
	}

	bus_irq_reg_mask[CAM_VFE_BUS_VER3_IRQ_REG0] =
		(0x1 << (rsrc_data->comp_grp_type +
		vfe_out_data->buf_done_mask_shift +
		rsrc_data->common_data->comp_done_shift));

	CAM_DBG(CAM_ISP, "Start Done VFE:%d comp_grp:%d bus_irq_mask_0: 0x%X",
		rsrc_data->common_data->core_index,
		rsrc_data->comp_grp_type,
		bus_irq_reg_mask[CAM_VFE_BUS_VER3_IRQ_REG0]);

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return rc;
}

static int cam_vfe_bus_ver3_stop_comp_grp(
	struct cam_isp_resource_node          *comp_grp)
{
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return 0;
}

static int cam_vfe_bus_ver3_handle_comp_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_bus_ver3_handle_comp_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv,
	uint64_t            *comp_mask)
{
	int rc = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_vfe_bus_ver3_vfe_out_data  *vfe_out  = handler_priv;
	struct cam_isp_resource_node          *comp_grp = vfe_out->comp_grp;
	struct cam_vfe_bus_irq_evt_payload    *evt_payload = evt_payload_priv;
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data = comp_grp->res_priv;
	uint32_t                              *cam_ife_irq_regs;
	uint32_t                               status_0;

	if (!evt_payload)
		return rc;

	if (rsrc_data->is_dual && (!rsrc_data->is_master)) {
		CAM_ERR(CAM_ISP, "Invalid comp_grp:%u is_master:%u",
			rsrc_data->comp_grp_type, rsrc_data->is_master);
		return rc;
	}

	cam_ife_irq_regs = evt_payload->irq_reg_val;
	status_0 = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];

	if (status_0 & BIT(rsrc_data->comp_grp_type +
		vfe_out->buf_done_mask_shift +
		rsrc_data->common_data->comp_done_shift)) {
		evt_payload->evt_id = CAM_ISP_HW_EVENT_DONE;
		rc = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	CAM_DBG(CAM_ISP, "VFE:%d comp_grp:%d Bus IRQ status_0: 0x%X rc:%d",
		rsrc_data->common_data->core_index, rsrc_data->comp_grp_type,
		status_0, rc);

	*comp_mask = rsrc_data->composite_mask;

	return rc;
}

static int cam_vfe_bus_ver3_init_comp_grp(uint32_t index,
	struct cam_hw_soc_info          *soc_info,
	struct cam_vfe_bus_ver3_priv    *ver3_bus_priv,
	struct cam_vfe_bus_ver3_hw_info *ver3_hw_info,
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data = NULL;
	struct cam_vfe_soc_private *vfe_soc_private = NULL;
	uint32_t ubwc_static_ctrl[2] = {0};

	int ddr_type = 0;

	if (soc_info) {
		vfe_soc_private = soc_info->soc_private;
		ubwc_static_ctrl[0] = vfe_soc_private->ubwc_static_ctrl[0];
		ubwc_static_ctrl[1] = vfe_soc_private->ubwc_static_ctrl[1];
	}

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver3_comp_grp_data),
		GFP_KERNEL);
	if (!rsrc_data)
		return -ENOMEM;

	comp_grp->res_priv = rsrc_data;

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&comp_grp->list);

	rsrc_data->comp_grp_type   = index;
	rsrc_data->common_data     = &ver3_bus_priv->common_data;
	rsrc_data->dual_slave_core = CAM_VFE_BUS_VER3_VFE_CORE_MAX;

	if (rsrc_data->comp_grp_type != CAM_VFE_BUS_VER3_COMP_GRP_0 &&
		rsrc_data->comp_grp_type != CAM_VFE_BUS_VER3_COMP_GRP_1)
		rsrc_data->ubwc_static_ctrl = 0;
	else {
		ddr_type = cam_get_ddr_type();
		if ((ddr_type == DDR_TYPE_LPDDR5) ||
			(ddr_type == DDR_TYPE_LPDDR5X))
			rsrc_data->ubwc_static_ctrl =
				ubwc_static_ctrl[1];
		else
			rsrc_data->ubwc_static_ctrl =
				ubwc_static_ctrl[0];
	}

	list_add_tail(&comp_grp->list, &ver3_bus_priv->free_comp_grp);

	comp_grp->top_half_handler = cam_vfe_bus_ver3_handle_comp_done_top_half;
	comp_grp->hw_intf = ver3_bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_vfe_bus_ver3_deinit_comp_grp(
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data =
		comp_grp->res_priv;

	comp_grp->start = NULL;
	comp_grp->stop = NULL;
	comp_grp->top_half_handler = NULL;
	comp_grp->bottom_half_handler = NULL;
	comp_grp->hw_intf = NULL;

	list_del_init(&comp_grp->list);
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;

	comp_grp->res_priv = NULL;

	if (!rsrc_data) {
		CAM_ERR(CAM_ISP, "comp_grp_priv is NULL");
		return -ENODEV;
	}
	kfree(rsrc_data);

	return 0;
}

static int cam_vfe_bus_ver3_get_secure_mode(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_isp_hw_get_cmd_update      *secure_mode = cmd_args;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data;
	uint32_t                              *mode;

	rsrc_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		secure_mode->res->res_priv;
	mode = (uint32_t *)secure_mode->data;
	*mode = (rsrc_data->secure_mode == CAM_SECURE_MODE_SECURE) ?
		true : false;

	return 0;
}

static int cam_vfe_bus_ver3_acquire_vfe_out(void *bus_priv, void *acquire_args,
	uint32_t args_size)
{
	int                                     rc = -ENODEV;
	int                                     i;
	enum cam_vfe_bus_ver3_vfe_out_type      vfe_out_res_id;
	uint32_t                                format;
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv = bus_priv;
	struct cam_vfe_acquire_args            *acq_args = acquire_args;
	struct cam_vfe_hw_vfe_out_acquire_args *out_acquire_args;
	struct cam_isp_resource_node           *rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data   *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_comp_grp_acquire_args comp_acq_args = {0};
	uint32_t       outmap_index = CAM_VFE_BUS_VER3_VFE_OUT_MAX;
	uint32_t       out_port_res_type;
	bool is_per_port_acquire = false;

	if (!bus_priv || !acquire_args) {
		CAM_ERR(CAM_ISP, "Invalid Param");
		return -EINVAL;
	}

	out_acquire_args = &acq_args->vfe_out;
	format = out_acquire_args->out_port_info->format;

	if (acq_args->per_port_acquire) {
		out_port_res_type = out_acquire_args->vfe_res_out_id;
		is_per_port_acquire = true;
	} else {
		out_port_res_type = out_acquire_args->out_port_info->acquired_res_type;
	}

	CAM_DBG(CAM_ISP, "VFE:%d Acquire out_type:0x%x  %d",
		ver3_bus_priv->common_data.core_index,
		out_port_res_type,
		out_acquire_args->vfe_res_out_id);

	vfe_out_res_id = cam_vfe_bus_ver3_get_out_res_id_and_index(
				ver3_bus_priv, out_port_res_type,
				&outmap_index);

	if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_MAX) ||
		(outmap_index >= ver3_bus_priv->num_out)) {
		CAM_WARN(CAM_ISP,
			"target does not support req res id :0x%x outtype:%d index:%d num: %d",
			out_acquire_args->out_port_info->acquired_res_type,
			vfe_out_res_id, outmap_index, ver3_bus_priv->num_out);
		return -ENODEV;
	}

	rsrc_node = &ver3_bus_priv->vfe_out[outmap_index];
	if (rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP,
			"VFE:%d out_type:0x%X resource not available state:%d",
			ver3_bus_priv->common_data.core_index,
			vfe_out_res_id, rsrc_node->res_state);
		return -EBUSY;
	}

	rsrc_data = rsrc_node->res_priv;
	rsrc_data->common_data->event_cb = acq_args->event_cb;
	rsrc_data->common_data->priv = acq_args->priv;
	rsrc_data->common_data->disable_ubwc_comp =
		out_acquire_args->disable_ubwc_comp;
	rsrc_data->priv = acq_args->priv;
	rsrc_data->bus_priv = ver3_bus_priv;
	rsrc_data->limiter_enabled = false;
	comp_acq_args.composite_mask = (1ULL << vfe_out_res_id);

	/* for some hw versions, buf done is not received from vfe but
	 * from IP external to VFE. In such case, we get the controller
	 * from hw manager and assign it here
	 */
	if (!(ver3_bus_priv->common_data.supported_irq &
			CAM_VFE_HW_IRQ_CAP_BUF_DONE))
		rsrc_data->common_data->buf_done_controller =
			acq_args->buf_done_controller;

	ver3_bus_priv->workq_info = acq_args->workq;
	rsrc_node->rdi_only_ctx = 0;
	rsrc_node->res_id = out_acquire_args->out_port_info->acquired_res_type;
	rsrc_node->workq_info = acq_args->workq;
	rsrc_node->cdm_ops = out_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = out_acquire_args->cdm_ops;
	rsrc_data->format = out_acquire_args->out_port_info->format;

	if ((rsrc_data->out_type == CAM_VFE_BUS_VER3_VFE_OUT_FD) &&
		(rsrc_data->format == CAM_FORMAT_Y_ONLY))
		rsrc_data->num_wm = 1;

	/* Acquire WM and retrieve COMP GRP ID */
	for (i = 0; i < rsrc_data->num_wm; i++) {
		rc = cam_vfe_bus_ver3_acquire_wm(ver3_bus_priv,
			out_acquire_args,
			acq_args->workq,
			vfe_out_res_id,
			i,
			&rsrc_data->wm_res[i],
			&comp_acq_args.comp_grp_id,
			is_per_port_acquire);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed to acquire WM VFE:%d out_type:%d rc:%d",
				rsrc_data->common_data->core_index,
				vfe_out_res_id, rc);
			goto release_wm;
		}
	}


	/* Acquire composite group using COMP GRP ID */
	rc = cam_vfe_bus_ver3_acquire_comp_grp(ver3_bus_priv,
		acq_args->workq,
		out_acquire_args->is_dual,
		out_acquire_args->is_master,
		&rsrc_data->comp_grp,
		&comp_acq_args,
		is_per_port_acquire);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Failed to acquire comp_grp VFE:%d out_typp:%d rc:%d",
			rsrc_data->common_data->core_index,
			vfe_out_res_id, rc);
		goto release_wm;
	}

	rsrc_data->is_dual = out_acquire_args->is_dual;
	rsrc_data->is_master = out_acquire_args->is_master;
	out_acquire_args->secure_mask = rsrc_data->secure_mask;
	if (rsrc_data->secure_mask)
		rsrc_data->secure_mode = out_acquire_args->out_port_info->secure_mode;

	rsrc_node->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rsrc_node->is_per_port_acquire = is_per_port_acquire;
	out_acquire_args->rsrc_node = rsrc_node;

	CAM_DBG(CAM_ISP, "Acquire successful");
	return rc;

release_wm:
	for (i--; i >= 0; i--)
		cam_vfe_bus_ver3_release_wm(ver3_bus_priv,
			&rsrc_data->wm_res[i]);

	return rc;
}

static int cam_vfe_bus_ver3_release_vfe_out(void *bus_priv, void *release_args,
	uint32_t args_size)
{
	uint32_t i;
	struct cam_isp_resource_node          *vfe_out = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;

	if (!bus_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Invalid input bus_priv %pK release_args %pK",
			bus_priv, release_args);
		return -EINVAL;
	}

	vfe_out = release_args;
	rsrc_data = vfe_out->res_priv;

	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP,
			"Invalid resource state:%d VFE:%d out_type:0x%X",
			vfe_out->res_state, rsrc_data->common_data->core_index,
			vfe_out->res_id);
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		cam_vfe_bus_ver3_release_wm(bus_priv, &rsrc_data->wm_res[i]);

	if ((rsrc_data->out_type == CAM_VFE_BUS_VER3_VFE_OUT_FD) &&
		(rsrc_data->format == CAM_FORMAT_Y_ONLY))
		rsrc_data->num_wm = 2;

	if (rsrc_data->comp_grp)
		cam_vfe_bus_ver3_release_comp_grp(bus_priv,
			rsrc_data->comp_grp);
	rsrc_data->comp_grp = NULL;

	vfe_out->workq_info = NULL;
	vfe_out->cdm_ops = NULL;
	rsrc_data->cdm_util_ops = NULL;

	rsrc_data->secure_mode = CAM_SECURE_MODE_NON_SECURE;

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		vfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_ver3_start_vfe_out(
	struct cam_isp_resource_node          *vfe_out)
{
	int rc = 0, i;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_common_data   *common_data = NULL;
	uint32_t source_group = 0;

	if (!vfe_out) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_out->res_priv;
	common_data = rsrc_data->common_data;
	source_group = rsrc_data->source_group;

	CAM_DBG(CAM_ISP, "Start VFE:%d out_type:0x%X",
		rsrc_data->common_data->core_index, rsrc_data->out_type);

	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP,
			"Invalid resource state:%d VFE:%d out_type:0x%X",
			vfe_out->res_state, rsrc_data->common_data->core_index,
			rsrc_data->out_type);
		return -EACCES;
	}

	/* subscribe when first out rsrc is streamed on */
	if (!rsrc_data->common_data->init_irq_subscribed) {
		rc = cam_vfe_bus_ver3_subscribe_init_irq(rsrc_data->bus_priv);
		if (rc)
			return rc;
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_vfe_bus_ver3_start_wm(&rsrc_data->wm_res[i]);

	rc = cam_vfe_bus_ver3_start_comp_grp(rsrc_data,
		rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_BUF_DONE_MASK]);

	if (rsrc_data->is_dual && !rsrc_data->is_master)
		goto end;

	if (vfe_out->is_per_port_start) {
		CAM_DBG(CAM_ISP, "Skipping irq subscribe for resources that are not updated");
		goto end;
	}

	vfe_out->irq_handle = cam_irq_controller_subscribe_irq(
		common_data->buf_done_controller,
		CAM_IRQ_PRIORITY_1,
		rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_BUF_DONE_MASK],
		vfe_out,
		vfe_out->top_half_handler,
		vfe_out->bottom_half_handler,
		vfe_out->workq_info,
		&workq_bh_api,
		CAM_IRQ_EVT_GROUP_0);

	if (vfe_out->irq_handle < 1) {
		CAM_ERR(CAM_ISP, "Subscribe IRQ failed for VFE out_res %d",
			vfe_out->res_id);
		vfe_out->irq_handle = 0;
		return -EFAULT;
	}

	if ((common_data->is_lite || source_group > CAM_VFE_BUS_VER3_SRC_GRP_0)
		&& !vfe_out->rdi_only_ctx)
		goto end;

	if ((common_data->supported_irq & CAM_VFE_HW_IRQ_CAP_RUP) &&
		(!common_data->rup_irq_handle[source_group])) {
		rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK][CAM_VFE_BUS_VER3_IRQ_REG0] |=
			0x1 << source_group;

		CAM_DBG(CAM_ISP,
			"VFE:%d out_type:0x%X bus_irq_mask_0:0x%X for RUP",
			rsrc_data->common_data->core_index, rsrc_data->out_type,
			rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK]
				[CAM_VFE_BUS_VER3_IRQ_REG0]);

		common_data->rup_irq_handle[source_group] =
			cam_irq_controller_subscribe_irq(
				common_data->bus_irq_controller,
				CAM_IRQ_PRIORITY_0,
				rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK],
				vfe_out,
				cam_vfe_bus_ver3_handle_rup_top_half,
				cam_vfe_bus_ver3_handle_rup_bottom_half,
				vfe_out->workq_info,
				&workq_bh_api,
				CAM_IRQ_EVT_GROUP_1);

		if (common_data->rup_irq_handle[source_group] < 1) {
			CAM_ERR(CAM_ISP, "Failed to subscribe RUP IRQ");
			common_data->rup_irq_handle[source_group] = 0;
			return -EFAULT;
		}
	}

end:
	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_vfe_bus_ver3_stop_vfe_out(
	struct cam_isp_resource_node          *vfe_out)
{
	int rc = 0, i;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_common_data   *common_data = NULL;

	if (!vfe_out) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_out->res_priv;
	common_data = rsrc_data->common_data;

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE ||
		vfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "Stop VFE:%d out_type:0x%X state:%d",
			rsrc_data->common_data->core_index, rsrc_data->out_type,
			vfe_out->res_state);
		return rc;
	}

	rc = cam_vfe_bus_ver3_stop_comp_grp(rsrc_data->comp_grp);

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_vfe_bus_ver3_stop_wm(&rsrc_data->wm_res[i]);

	if (common_data->rup_irq_handle[rsrc_data->source_group]) {
		rc = cam_irq_controller_unsubscribe_irq(
			common_data->bus_irq_controller,
			common_data->rup_irq_handle[rsrc_data->source_group]);
		common_data->rup_irq_handle[rsrc_data->source_group] = 0;
	}

	if (vfe_out->irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			common_data->buf_done_controller,
			vfe_out->irq_handle);
		vfe_out->irq_handle = 0;
	}

	if (rsrc_data->common_data->init_irq_subscribed)
		cam_vfe_bus_ver3_unsubscribe_init_irq(rsrc_data->bus_priv);

	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_vfe_bus_ver3_handle_vfe_out_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                     rc;
	int                                         i;
	struct cam_isp_resource_node               *vfe_out = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data       *rsrc_data = NULL;
	struct cam_vfe_bus_irq_evt_payload         *evt_payload;
	struct cam_vfe_bus_ver3_comp_grp_data      *resource_data;
	uint32_t                                    status_0;

	vfe_out = th_payload->handler_priv;
	if (!vfe_out) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No resource");
		return -ENODEV;
	}

	rsrc_data = vfe_out->res_priv;
	resource_data = rsrc_data->comp_grp->res_priv;

	rc  = cam_vfe_bus_ver3_get_evt_payload(rsrc_data->common_data,
		&evt_payload);

	if (rc) {
		for (i = 0; i < th_payload->num_registers; i++)
			CAM_INFO_RATE_LIMIT(CAM_ISP,
				"VFE:%d Bus IRQ status_%d: 0x%X",
				rsrc_data->common_data->core_index, i,
				th_payload->evt_status_arr[i]);
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	evt_payload->core_index = rsrc_data->common_data->core_index;
	evt_payload->evt_id = evt_id;

	for (i = 0; i < th_payload->num_registers; i++) {
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];
		CAM_DBG(CAM_ISP, "VFE:%d Bus IRQ status_%d: 0x%X",
			rsrc_data->common_data->core_index, i,
			th_payload->evt_status_arr[i]);
	}

	th_payload->evt_payload_priv = evt_payload;

	status_0 = th_payload->evt_status_arr[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];

	if (status_0 & BIT(resource_data->comp_grp_type +
		rsrc_data->common_data->comp_done_shift)) {
		trace_cam_log_event("bufdone", "bufdone_IRQ",
			status_0, resource_data->comp_grp_type);
	}

	if (status_0 & 0x1)
		trace_cam_log_event("UnexpectedRUP", "RUP_IRQ", status_0, 40);

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

static uint32_t cam_vfe_bus_ver3_get_last_consumed_addr(
	struct cam_vfe_bus_ver3_priv *bus_priv,
	uint32_t res_type)
{
	uint32_t                                  val = 0;
	struct cam_isp_resource_node             *rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data     *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_rsrc_data = NULL;
	enum cam_vfe_bus_ver3_vfe_out_type        res_id;
	uint32_t           outmap_index = CAM_VFE_BUS_VER3_VFE_OUT_MAX;

	res_id = cam_vfe_bus_ver3_get_out_res_id_and_index(bus_priv,
		res_type, &outmap_index);

	if ((res_id >= CAM_VFE_BUS_VER3_VFE_OUT_MAX) ||
		(outmap_index >= bus_priv->num_out)) {
		CAM_WARN(CAM_ISP,
			"target does not support req res id :0x%x outtype:%d index:%d",
			res_type, res_id, outmap_index);
		return 0;
	}

	rsrc_node = &bus_priv->vfe_out[outmap_index];
	rsrc_data = rsrc_node->res_priv;
	wm_rsrc_data = rsrc_data->wm_res[PLANE_Y].res_priv;

	val = cam_io_r_mb(
		wm_rsrc_data->common_data->mem_base +
		wm_rsrc_data->hw_regs->addr_status_0);

	return val;
}

static int cam_vfe_bus_ver3_handle_vfe_out_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv)
{
	int                                    rc = -EINVAL, num_out = 0, i = 0;
	struct cam_isp_resource_node          *vfe_out = handler_priv;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = vfe_out->res_priv;
	struct cam_vfe_bus_irq_evt_payload    *evt_payload = evt_payload_priv;
	struct cam_isp_hw_event_info           evt_info;
	struct cam_isp_hw_compdone_event_info  compdone_evt_info = {0};
	void                                  *ctx = NULL;
	uint32_t                               evt_id = 0;
	uint64_t                               comp_mask = 0;
	uint32_t                               out_list[CAM_VFE_BUS_VER3_VFE_OUT_MAX];

	rc = cam_vfe_bus_ver3_handle_comp_done_bottom_half(
		rsrc_data, evt_payload_priv, &comp_mask);
	CAM_DBG(CAM_ISP, "VFE:%d out_type:0x%X comp_mask: 0x%lx rc:%d",
		rsrc_data->common_data->core_index, rsrc_data->out_type, comp_mask, rc);

	ctx = rsrc_data->priv;
	memset(out_list, 0, sizeof(out_list));

	switch (rc) {
	case CAM_VFE_IRQ_STATUS_SUCCESS:
		evt_id = evt_payload->evt_id;

		evt_info.res_type = vfe_out->res_type;
		evt_info.hw_idx   = vfe_out->hw_intf->hw_idx;
		evt_info.hw_type  = CAM_ISP_HW_TYPE_VFE;

		cam_vfe_bus_ver3_get_comp_vfe_out_res_id_list(
			comp_mask, out_list, &num_out);
		if (num_out > CAM_NUM_OUT_PER_COMP_IRQ_MAX) {
			CAM_ERR(CAM_ISP,
				"num_out: %d  exceeds max_port_per_comp_grp: %d for comp_mask: %u",
				num_out, CAM_NUM_OUT_PER_COMP_IRQ_MAX, comp_mask);
			for (i = 0; i < num_out; i++)
				CAM_ERR(CAM_ISP,
					"Skipping buf done notify for outport: %u",
					out_list[i]);
			rc = -EINVAL;
			goto end;
		}

		compdone_evt_info.num_res = num_out;
		for (i = 0; i < num_out; i++) {
			compdone_evt_info.res_id[i] = out_list[i];
			compdone_evt_info.last_consumed_addr[i] =
				cam_vfe_bus_ver3_get_last_consumed_addr(
				rsrc_data->bus_priv,
				compdone_evt_info.res_id[i]);
		}
		evt_info.event_data = (void *)&compdone_evt_info;
		if (rsrc_data->common_data->event_cb)
			rsrc_data->common_data->event_cb(ctx, evt_id,
				(void *)&evt_info);
		break;
	default:
		break;
	}

end:
	cam_vfe_bus_ver3_put_evt_payload(rsrc_data->common_data, &evt_payload);

	return rc;
}

static int cam_vfe_bus_ver3_init_vfe_out_resource(uint32_t  index,
	struct cam_vfe_bus_ver3_priv                  *ver3_bus_priv,
	struct cam_vfe_bus_ver3_hw_info               *ver3_hw_info)
{
	struct cam_isp_resource_node         *vfe_out = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data = NULL;
	int rc = 0, i = 0;
	int32_t vfe_out_type =
		ver3_hw_info->vfe_out_hw_info[index].vfe_out_type;

	if (vfe_out_type < 0 ||
		vfe_out_type >= CAM_VFE_BUS_VER3_VFE_OUT_MAX) {
		CAM_ERR(CAM_ISP, "Init VFE Out failed, Invalid type=%d",
			vfe_out_type);
		return -EINVAL;
	}

	ver3_bus_priv->vfe_out_map_outtype[vfe_out_type] = index;
	vfe_out = &ver3_bus_priv->vfe_out[index];
	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_UNAVAILABLE ||
		vfe_out->res_priv) {
		CAM_ERR(CAM_ISP, "vfe_out_type %d has already been initialized",
			vfe_out_type);
		return -EFAULT;
	}

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver3_vfe_out_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		rc = -ENOMEM;
		return rc;
	}

	vfe_out->res_priv = rsrc_data;

	vfe_out->res_type = CAM_ISP_RESOURCE_VFE_OUT;
	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&vfe_out->list);

	rsrc_data->source_group =
		ver3_hw_info->vfe_out_hw_info[index].source_group;
	rsrc_data->buf_done_mask_shift =
		ver3_hw_info->vfe_out_hw_info[index].bufdone_shift;
	rsrc_data->out_type     =
		ver3_hw_info->vfe_out_hw_info[index].vfe_out_type;
	rsrc_data->common_data  = &ver3_bus_priv->common_data;
	rsrc_data->max_width    =
		ver3_hw_info->vfe_out_hw_info[index].max_width;
	rsrc_data->max_height   =
		ver3_hw_info->vfe_out_hw_info[index].max_height;
	rsrc_data->secure_mode  = CAM_SECURE_MODE_NON_SECURE;
	rsrc_data->num_wm       = ver3_hw_info->vfe_out_hw_info[index].num_wm;

	rsrc_data->wm_res = kzalloc((sizeof(struct cam_isp_resource_node) *
		rsrc_data->num_wm), GFP_KERNEL);
	if (!rsrc_data->wm_res) {
		CAM_ERR(CAM_ISP, "Failed to alloc for wm_res");
		return -ENOMEM;
	}

	for (i = 0; i < rsrc_data->num_wm; i++) {
		rc = cam_vfe_bus_ver3_init_wm_resource(
			ver3_hw_info->vfe_out_hw_info[index].wm_idx[i],
			ver3_bus_priv, ver3_hw_info,
			&rsrc_data->wm_res[i],
			ver3_hw_info->vfe_out_hw_info[index].name[i],
			ver3_hw_info->vfe_out_hw_info[index].line_based);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "VFE:%d init WM:%d failed rc:%d",
				ver3_bus_priv->common_data.core_index, i, rc);
			return rc;
		}
	}

	vfe_out->start = cam_vfe_bus_ver3_start_vfe_out;
	vfe_out->stop = cam_vfe_bus_ver3_stop_vfe_out;
	vfe_out->top_half_handler =
		cam_vfe_bus_ver3_handle_vfe_out_done_top_half;
	vfe_out->bottom_half_handler =
		cam_vfe_bus_ver3_handle_vfe_out_done_bottom_half;
	vfe_out->process_cmd = cam_vfe_bus_ver3_process_cmd;
	vfe_out->hw_intf = ver3_bus_priv->common_data.hw_intf;
	vfe_out->irq_handle = 0;

	for (i = 0; i < CAM_VFE_BUS_VER3_MAX_MID_PER_PORT; i++)
		rsrc_data->mid[i] = ver3_hw_info->vfe_out_hw_info[index].mid[i];

	rsrc_data->secure_mask = ver3_hw_info->vfe_out_hw_info[index].secure_mask;

	return 0;
}

static int cam_vfe_bus_ver3_deinit_vfe_out_resource(
	struct cam_isp_resource_node    *vfe_out)
{
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data = vfe_out->res_priv;
	int rc = 0, i = 0;

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_UNAVAILABLE) {
		/*
		 * This is not error. It can happen if the resource is
		 * never supported in the HW.
		 */
		return 0;
	}

	vfe_out->start = NULL;
	vfe_out->stop = NULL;
	vfe_out->top_half_handler = NULL;
	vfe_out->bottom_half_handler = NULL;
	vfe_out->hw_intf = NULL;
	vfe_out->irq_handle = 0;

	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&vfe_out->list);
	vfe_out->res_priv = NULL;

	if (!rsrc_data)
		return -ENOMEM;

	for (i = 0; i < rsrc_data->num_wm; i++) {
		rc = cam_vfe_bus_ver3_deinit_wm_resource(&rsrc_data->wm_res[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"VFE:%d deinit WM:%d failed rc:%d",
				rsrc_data->common_data->core_index, i, rc);
	}

	rsrc_data->wm_res = NULL;
	kfree(rsrc_data);

	return 0;
}

static void cam_vfe_bus_ver3_print_wm_info(
	struct cam_vfe_bus_ver3_wm_resource_data  *wm_data,
	struct cam_vfe_bus_ver3_common_data  *common_data,
	uint8_t *wm_name)
{
	uint32_t addr_status0, addr_status1, addr_status2, addr_status3;

	addr_status0 = cam_io_r_mb(common_data->mem_base +
		wm_data->hw_regs->addr_status_0);
	addr_status1 = cam_io_r_mb(common_data->mem_base +
		wm_data->hw_regs->addr_status_1);
	addr_status2 = cam_io_r_mb(common_data->mem_base +
		wm_data->hw_regs->addr_status_2);
	addr_status3 = cam_io_r_mb(common_data->mem_base +
		wm_data->hw_regs->addr_status_3);

	CAM_INFO(CAM_ISP,
		"VFE:%d WM:%d wm_name:%s width:%u height:%u stride:%u x_init:%u en_cfg:%u acquired width:%u height:%u",
		wm_data->common_data->core_index, wm_data->index, wm_name,
		wm_data->width,
		wm_data->height,
		wm_data->stride, wm_data->h_init,
		wm_data->en_cfg,
		wm_data->acquired_width,
		wm_data->acquired_height);
	CAM_INFO(CAM_ISP,
		"hw:%d WM:%d last consumed address:0x%x last frame addr:0x%x fifo cnt:0x%x current client address:0x%x",
		common_data->hw_intf->hw_idx,
		wm_data->index,
		addr_status0,
		addr_status1,
		addr_status2,
		addr_status3);
}

static int cam_vfe_bus_ver3_mini_dump(
	struct cam_vfe_bus_ver3_priv *bus_priv,
	void *cmd_args)
{
	struct cam_isp_resource_node              *rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data      *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data  *wm        = NULL;
	struct cam_vfe_bus_ver3_mini_dump_data    *md;
	struct cam_vfe_bus_ver3_wm_mini_dump      *md_wm;
	struct cam_hw_mini_dump_args              *md_args;
	struct cam_hw_info                        *hw_info = NULL;
	uint32_t                                   bytes_written = 0;
	uint32_t                                   i, j, k = 0;

	if (!bus_priv || !cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid bus private data");
		return -EINVAL;
	}

	hw_info = (struct cam_hw_info *)bus_priv->common_data.hw_intf->hw_priv;
	md_args = (struct cam_hw_mini_dump_args *)cmd_args;

	if (sizeof(*md) > md_args->len) {
		md_args->bytes_written = 0;
		return 0;
	}

	md = (struct cam_vfe_bus_ver3_mini_dump_data *)md_args->start_addr;
	md->clk_rate = hw_info->soc_info.applied_src_clk_rate;
	md->hw_idx = bus_priv->common_data.hw_intf->hw_idx;
	md->hw_state = hw_info->hw_state;
	bytes_written += sizeof(*md);
	md->wm = (struct cam_vfe_bus_ver3_wm_mini_dump *)
			((uint8_t *)md_args->start_addr + bytes_written);

	for (i = 0; i < bus_priv->num_out; i++) {
		rsrc_node = &bus_priv->vfe_out[i];
		rsrc_data = rsrc_node->res_priv;
		if (!rsrc_data)
			continue;

		for (j = 0; j < rsrc_data->num_wm; j++) {
			if (bytes_written + sizeof(*md_wm) > md_args->len)
				goto end;

			md_wm = &md->wm[k];
			wm = rsrc_data->wm_res[j].res_priv;
			md_wm->width  = wm->width;
			md_wm->index  = wm->index;
			md_wm->height = wm->height;
			md_wm->stride = wm->stride;
			md_wm->en_cfg = wm->en_cfg;
			md_wm->h_init = wm->h_init;
			md_wm->format = wm->format;
			md_wm->acquired_width = wm->acquired_width;
			md_wm->acquired_height = wm->acquired_height;
			md_wm->state = rsrc_node->res_state;
			scnprintf(md_wm->name, CAM_ISP_RES_NAME_LEN,
				"%s", rsrc_data->wm_res[j].res_name);
			k++;
			bytes_written += sizeof(*md_wm);
		}
	}
end:
	md->num_client = k;
	md_args->bytes_written = bytes_written;
	return 0;
}

static int cam_vfe_bus_ver3_print_dimensions(
	uint32_t                                   res_id,
	struct cam_vfe_bus_ver3_priv              *bus_priv)
{
	struct cam_isp_resource_node              *rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data      *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data  *wm_data   = NULL;
	struct cam_vfe_bus_ver3_common_data  *common_data = NULL;
	int                                        i;
	uint8_t                                *wm_name = NULL;
	enum cam_vfe_bus_ver3_vfe_out_type  vfe_out_res_id =
		CAM_VFE_BUS_VER3_VFE_OUT_MAX;
	uint32_t  outmap_index = CAM_VFE_BUS_VER3_VFE_OUT_MAX;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Invalid bus private data, res_id: %d",
			res_id);
		return -EINVAL;
	}

	vfe_out_res_id = cam_vfe_bus_ver3_get_out_res_id_and_index(bus_priv,
				res_id, &outmap_index);

	if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_MAX) ||
		(outmap_index >= bus_priv->num_out)) {
		CAM_WARN_RATE_LIMIT(CAM_ISP,
			"target does not support req res id :0x%x outtype:%d index:%d",
			res_id,
			vfe_out_res_id, outmap_index);
		return -EINVAL;
	}

	rsrc_node = &bus_priv->vfe_out[outmap_index];
	rsrc_data = rsrc_node->res_priv;
	if (!rsrc_data) {
		CAM_ERR(CAM_ISP, "VFE out data is null, res_id: %d",
			vfe_out_res_id);
		return -EINVAL;
	}

	for (i = 0; i < rsrc_data->num_wm; i++) {
		wm_data = rsrc_data->wm_res[i].res_priv;
		wm_name = rsrc_data->wm_res[i].res_name;
		common_data = rsrc_data->common_data;
		cam_vfe_bus_ver3_print_wm_info(wm_data, common_data, wm_name);
	}
	return 0;
}

static int cam_vfe_bus_ver3_handle_bus_irq(uint32_t    evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_vfe_bus_ver3_priv          *bus_priv;
	int rc = 0;

	bus_priv = th_payload->handler_priv;
	CAM_DBG(CAM_ISP, "Enter");
	rc = cam_irq_controller_handle_irq(evt_id,
		bus_priv->common_data.bus_irq_controller, CAM_IRQ_EVT_GROUP_0);
	return (rc == IRQ_HANDLED) ? 0 : -EINVAL;
}

static int cam_vfe_bus_ver3_handle_rup_irq(uint32_t     evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_vfe_bus_ver3_priv          *bus_priv;
	int rc = 0;

	bus_priv = th_payload->handler_priv;
	CAM_DBG(CAM_ISP, "Enter");
	rc = cam_irq_controller_handle_irq(evt_id,
		bus_priv->common_data.bus_irq_controller, CAM_IRQ_EVT_GROUP_1);
	return (rc == IRQ_HANDLED) ? 0 : -EINVAL;
}

static int cam_vfe_bus_ver3_err_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int i = 0, rc = 0;
	struct cam_vfe_bus_ver3_priv *bus_priv =
		th_payload->handler_priv;
	struct cam_vfe_bus_irq_evt_payload *evt_payload;

	CAM_ERR_RATE_LIMIT(CAM_ISP, "VFE:%d BUS Err IRQ",
		bus_priv->common_data.core_index);
	for (i = 0; i < th_payload->num_registers; i++) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "VFE:%d BUS IRQ status_%d: 0x%X",
		bus_priv->common_data.core_index, i,
			th_payload->evt_status_arr[i]);
	}
	cam_irq_controller_disable_irq(bus_priv->common_data.bus_irq_controller,
		bus_priv->error_irq_handle);

	rc  = cam_vfe_bus_ver3_get_evt_payload(&bus_priv->common_data,
		&evt_payload);
	if (rc)
		return rc;

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->core_index = bus_priv->common_data.core_index;

	evt_payload->ccif_violation_status = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->ccif_violation_status);

	evt_payload->image_size_violation_status = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->image_size_violation_status);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static void cam_vfe_print_violations(
	char *error_type,
	uint32_t status,
	struct cam_vfe_bus_ver3_priv *bus_priv)
{
	int i, j;
	struct cam_isp_resource_node       *rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data      *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data  *wm_data   = NULL;
	struct cam_vfe_bus_ver3_common_data  *common_data = NULL;
	uint8_t                                *wm_name = NULL;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Invalid bus private data");
		return;
	}

	for (i = 0; i < bus_priv->num_out; i++) {
		rsrc_node = &bus_priv->vfe_out[i];
		rsrc_data = rsrc_node->res_priv;
		if (!rsrc_data) {
			CAM_ERR(CAM_ISP, "VFE out data is null, res_id: %d", i);
			return;
		}

		for (j = 0; j < rsrc_data->num_wm; j++) {
			wm_data = rsrc_data->wm_res[j].res_priv;
			common_data = rsrc_data->common_data;
			wm_name = rsrc_data->wm_res[j].res_name;

			if (status & (1 << wm_data->index)) {
				CAM_INFO(CAM_ISP, "%s Violation", error_type);
				cam_vfe_bus_ver3_print_wm_info(wm_data,
					common_data, wm_name);
			}
		}
	}
}

static uint32_t cam_vfe_bus_ver3_check_tunneling_overflow(
	uint32_t status, struct cam_vfe_bus_ver3_common_data *common_data)
{
	uint32_t no_tunnelId, tunnel_overflow;

	no_tunnelId     = (status >> common_data->no_tunnelingId_shift) & 0x1;
	tunnel_overflow = (status >> common_data->tunneling_overflow_shift) & 0x1;

	if (no_tunnelId || tunnel_overflow)
		CAM_ERR(CAM_ISP, "tunnel id not present %d tunnel overflow %d",
		no_tunnelId, tunnel_overflow);

	return tunnel_overflow;
}

static int cam_vfe_bus_ver3_err_irq_bottom_half(
	void *handler_priv, void *evt_payload_priv)
{
	struct cam_vfe_bus_irq_evt_payload *evt_payload = evt_payload_priv;
	struct cam_vfe_bus_ver3_priv *bus_priv = handler_priv;
	struct cam_vfe_bus_ver3_common_data *common_data;
	struct cam_isp_hw_event_info evt_info;
	struct cam_isp_hw_error_event_info err_evt_info;
	uint32_t status = 0, image_size_violation = 0, ccif_violation = 0, constraint_violation = 0;
	uint32_t tunnel_overflow = 0;

	if (!handler_priv || !evt_payload_priv)
		return -EINVAL;

	common_data = &bus_priv->common_data;

	status = evt_payload->irq_reg_val[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];
	image_size_violation = (status >> 31) & 0x1;
	ccif_violation = (status >> 30) & 0x1;
	constraint_violation = (status >> 28) & 0x1;

	CAM_ERR(CAM_ISP,
		"VFE:%d BUS error image size violation %d CCIF violation %d constraint violation %d",
		bus_priv->common_data.core_index, image_size_violation,
		ccif_violation, constraint_violation);
	CAM_INFO(CAM_ISP,
		"Image Size violation status 0x%X CCIF violation status 0x%X",
		evt_payload->image_size_violation_status,
		evt_payload->ccif_violation_status);

	if (image_size_violation || constraint_violation) {
		status = evt_payload->image_size_violation_status;
		if (!status)
			cam_vfe_bus_ver3_get_constraint_errors(bus_priv);
		else {
			cam_vfe_print_violations("Image Size", status,
				bus_priv);
		}
	}

	if (ccif_violation) {
		status = evt_payload->ccif_violation_status;
		cam_vfe_print_violations("CCIF", status, NULL);
	}

	if (common_data->support_tunneling)
		tunnel_overflow = cam_vfe_bus_ver3_check_tunneling_overflow(status, common_data);

	cam_vfe_bus_ver3_put_evt_payload(common_data, &evt_payload);

	evt_info.hw_idx = common_data->core_index;
	evt_info.res_type = CAM_ISP_RESOURCE_VFE_OUT;
	evt_info.res_id = CAM_VFE_BUS_VER3_VFE_OUT_MAX;
	evt_info.hw_type = CAM_ISP_HW_TYPE_VFE;
	err_evt_info.err_type = CAM_VFE_IRQ_STATUS_VIOLATION;
	evt_info.event_data = (void *)&err_evt_info;

	if (tunnel_overflow)
		err_evt_info.err_type = CAM_VFE_IRQ_STATUS_TUNNEL_OVERFLOW;

	if (common_data->event_cb)
		common_data->event_cb(common_data->priv, CAM_ISP_HW_EVENT_ERROR,
			(void *)&evt_info);
	return 0;
}

static void cam_vfe_bus_ver3_unsubscribe_init_irq(
	struct cam_vfe_bus_ver3_priv          *bus_priv)
{
	int rc = 0;

	if (bus_priv->error_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			bus_priv->error_irq_handle);
		if (rc)
			CAM_WARN(CAM_ISP, "failed to unsubscribe error irqs");

		bus_priv->error_irq_handle = 0;
	}

	if (bus_priv->bus_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.vfe_irq_controller,
			bus_priv->bus_irq_handle);
		if (rc)
			CAM_WARN(CAM_ISP, "failed to unsubscribe top irq");

		bus_priv->bus_irq_handle = 0;
		cam_irq_controller_unregister_dependent(bus_priv->common_data.vfe_irq_controller,
		bus_priv->common_data.bus_irq_controller);
	}

	if (bus_priv->rup_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.vfe_irq_controller,
			bus_priv->rup_irq_handle);
		if (rc)
			CAM_WARN(CAM_ISP, "failed to unsubscribe rup done irq");

		bus_priv->rup_irq_handle = 0;
	}

	bus_priv->common_data.init_irq_subscribed = false;
	CAM_DBG(CAM_ISP, "BUS init irq unsubscribed");
}

static int cam_vfe_bus_ver3_subscribe_init_irq(
	struct cam_vfe_bus_ver3_priv          *bus_priv)
{
	uint32_t top_irq_reg_mask[3] = {0};

	/* Subscribe top IRQ */
	top_irq_reg_mask[0] = (1 << bus_priv->top_irq_shift);

	cam_irq_controller_register_dependent(
		bus_priv->common_data.vfe_irq_controller,
		bus_priv->common_data.bus_irq_controller,
		top_irq_reg_mask);

	bus_priv->bus_irq_handle = cam_irq_controller_subscribe_irq(
		bus_priv->common_data.vfe_irq_controller,
		CAM_IRQ_PRIORITY_4,
		top_irq_reg_mask,
		bus_priv,
		cam_vfe_bus_ver3_handle_bus_irq,
		NULL,
		NULL,
		NULL,
		CAM_IRQ_EVT_GROUP_0);

	if (bus_priv->bus_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "Failed to subscribe BUS (buf_done) IRQ");
		bus_priv->bus_irq_handle = 0;
		return -EFAULT;
	}


	if (bus_priv->workq_info != NULL) {
		bus_priv->error_irq_handle = cam_irq_controller_subscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			CAM_IRQ_PRIORITY_0,
			bus_priv->bus_error_irq_mask,
			bus_priv,
			cam_vfe_bus_ver3_err_irq_top_half,
			cam_vfe_bus_ver3_err_irq_bottom_half,
			bus_priv->workq_info,
			&workq_bh_api,
			CAM_IRQ_EVT_GROUP_0);

		if (bus_priv->error_irq_handle < 1) {
			CAM_ERR(CAM_ISP, "Failed to subscribe BUS Error IRQ");
			bus_priv->error_irq_handle = 0;
			return -EFAULT;
		}
	}

	if (bus_priv->common_data.vfe_irq_controller &&
			bus_priv->common_data.supported_irq & CAM_VFE_HW_IRQ_CAP_RUP) {
		bus_priv->rup_irq_handle = cam_irq_controller_subscribe_irq(
			bus_priv->common_data.vfe_irq_controller,
			CAM_IRQ_PRIORITY_2,
			top_irq_reg_mask,
			bus_priv,
			cam_vfe_bus_ver3_handle_rup_irq,
			NULL,
			NULL,
			NULL,
			CAM_IRQ_EVT_GROUP_0);

		if (bus_priv->rup_irq_handle < 1) {
			CAM_ERR(CAM_ISP, "Failed to subscribe BUS (rup) IRQ");
			bus_priv->rup_irq_handle = 0;
			return -EFAULT;
		}
	}

	bus_priv->common_data.init_irq_subscribed = true;
	CAM_DBG(CAM_ISP, "BUS irq subscribed");
	return 0;
}

static void cam_vfe_bus_ver3_update_ubwc_meta_addr(
	uint32_t *reg_val_pair,
	uint32_t  *j,
	void     *regs,
	dma_addr_t  image_buf)
{
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;
	uint32_t temp = cam_smmu_is_expanded_memory() ?
		CAM_36BIT_INTF_GET_IOVA_BASE(image_buf) : image_buf;

	if (cam_smmu_is_expanded_memory() &&
		CAM_36BIT_INTF_GET_IOVA_OFFSET(image_buf)) {
		CAM_ERR(CAM_ISP, "Error, address not aligned! offset:0x%x",
			CAM_36BIT_INTF_GET_IOVA_OFFSET(image_buf));
	}

	if (!regs || !reg_val_pair || !j) {
		CAM_ERR(CAM_ISP, "Invalid args");
		goto end;
	}

	ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *) regs;
	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j, ubwc_regs->meta_addr, temp);

end:
	return;
}

static int cam_vfe_bus_ver3_update_ubwc_regs(
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data,
	uint32_t *reg_val_pair,	uint32_t i, uint32_t *j)
{
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;
	int rc = 0;

	if (!wm_data || !reg_val_pair || !j) {
		CAM_ERR(CAM_ISP, "Invalid args");
		rc = -EINVAL;
		goto end;
	}

	ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)
		wm_data->hw_regs->ubwc_regs;

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		wm_data->hw_regs->packer_cfg, wm_data->packer_cfg);
	CAM_DBG(CAM_ISP, "WM:%d packer cfg 0x%X",
		wm_data->index, reg_val_pair[*j-1]);

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->meta_cfg, wm_data->ubwc_meta_cfg);
	CAM_DBG(CAM_ISP, "WM:%d meta stride 0x%X",
		wm_data->index, reg_val_pair[*j-1]);

	if (wm_data->common_data->disable_ubwc_comp) {
		wm_data->ubwc_mode_cfg &= ~ubwc_regs->ubwc_comp_en_bit;
		CAM_DBG(CAM_ISP,
			"Force disable UBWC compression on VFE:%d WM:%d",
			wm_data->common_data->core_index, wm_data->index);
	}

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->mode_cfg, wm_data->ubwc_mode_cfg);
	CAM_DBG(CAM_ISP, "WM:%d ubwc_mode_cfg 0x%X",
		wm_data->index, reg_val_pair[*j-1]);

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->ctrl_2, wm_data->ubwc_ctrl_2);
	CAM_DBG(CAM_ISP, "WM:%d ubwc_ctrl_2 0x%X",
		wm_data->index, reg_val_pair[*j-1]);

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->lossy_thresh0, wm_data->ubwc_lossy_threshold_0);
	CAM_DBG(CAM_ISP, "WM:%d lossy_thresh0 0x%X",
		wm_data->index, reg_val_pair[*j-1]);

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->lossy_thresh1, wm_data->ubwc_lossy_threshold_1);
	CAM_DBG(CAM_ISP, "WM:%d lossy_thresh1 0x%X",
		wm_data->index, reg_val_pair[*j-1]);

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->off_lossy_var, wm_data->ubwc_offset_lossy_variance);
	CAM_DBG(CAM_ISP, "WM:%d off_lossy_var 0x%X",
		wm_data->index, reg_val_pair[*j-1]);

	/*
	 * If limit value >= 0xFFFF, limit configured by
	 * generic limiter blob
	 */
	if (wm_data->ubwc_bandwidth_limit < 0xFFFF) {
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
			ubwc_regs->bw_limit, wm_data->ubwc_bandwidth_limit);
		CAM_DBG(CAM_ISP, "WM:%d ubwc bw limit 0x%X",
			wm_data->index,   wm_data->ubwc_bandwidth_limit);
	}

end:
	return rc;
}

static int cam_vfe_bus_ver3_config_ubwc_regs(
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data)
{
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs =
		(struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)
		wm_data->hw_regs->ubwc_regs;

	cam_io_w_mb(wm_data->packer_cfg, wm_data->common_data->mem_base +
		wm_data->hw_regs->packer_cfg);
	CAM_DBG(CAM_ISP, "WM:%d packer cfg:0x%x",
		wm_data->index, wm_data->packer_cfg);

	cam_io_w_mb(wm_data->ubwc_meta_cfg,
		wm_data->common_data->mem_base + ubwc_regs->meta_cfg);
	CAM_DBG(CAM_ISP, "WM:%d meta stride:0x%x",
		wm_data->index, wm_data->ubwc_meta_cfg);

	if (wm_data->common_data->disable_ubwc_comp) {
		wm_data->ubwc_mode_cfg &= ~ubwc_regs->ubwc_comp_en_bit;
		CAM_DBG(CAM_ISP,
			"Force disable UBWC compression on VFE:%d WM:%d",
			wm_data->common_data->core_index, wm_data->index);
	}

	cam_io_w_mb(wm_data->ubwc_mode_cfg,
		wm_data->common_data->mem_base + ubwc_regs->mode_cfg);
	CAM_DBG(CAM_ISP, "WM:%d ubwc_mode_cfg:0x%x",
		wm_data->index, wm_data->ubwc_mode_cfg);

	cam_io_w_mb(wm_data->ubwc_ctrl_2,
		wm_data->common_data->mem_base + ubwc_regs->ctrl_2);
	CAM_DBG(CAM_ISP, "WM:%d ubwc_ctrl_2:0x%x",
		wm_data->index, wm_data->ubwc_ctrl_2);

	cam_io_w_mb(wm_data->ubwc_lossy_threshold_0,
		wm_data->common_data->mem_base + ubwc_regs->lossy_thresh0);
	CAM_DBG(CAM_ISP, "WM:%d lossy_thresh0: 0x%x",
		wm_data->index, wm_data->ubwc_lossy_threshold_0);

	cam_io_w_mb(wm_data->ubwc_lossy_threshold_1,
		wm_data->common_data->mem_base + ubwc_regs->lossy_thresh1);
	CAM_DBG(CAM_ISP, "WM:%d lossy_thresh0:0x%x",
		wm_data->index, wm_data->ubwc_lossy_threshold_1);

	cam_io_w_mb(wm_data->ubwc_offset_lossy_variance,
		wm_data->common_data->mem_base + ubwc_regs->off_lossy_var);
	CAM_DBG(CAM_ISP, "WM:%d off_lossy_var:0x%x",
	wm_data->index, wm_data->ubwc_offset_lossy_variance);

	/*
	 * If limit value >= 0xFFFF, limit configured by
	 * generic limiter blob
	 */
	if (wm_data->ubwc_bandwidth_limit < 0xFFFF) {
		cam_io_w_mb(wm_data->ubwc_bandwidth_limit,
			wm_data->common_data->mem_base + ubwc_regs->bw_limit);
		CAM_DBG(CAM_ISP, "WM:%d ubwc bw limit:0x%x",
			wm_data->index, wm_data->ubwc_bandwidth_limit);
	}

	return 0;
}

static int cam_vfe_bus_ver3_config_wm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv *bus_priv;
	struct cam_isp_hw_get_cmd_update *update_buf;
	struct cam_vfe_bus_ver3_vfe_out_data *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data = NULL;
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;
	uint32_t i, val, iova_addr, iova_offset, stride;
	dma_addr_t iova;

	bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
	update_buf = (struct cam_isp_hw_get_cmd_update *) cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		update_buf->res->res_priv;
	if (!vfe_out_data) {
		CAM_ERR(CAM_ISP, "Invalid data");
		return -EINVAL;
	}

	if (!vfe_out_data->limiter_enabled)
		CAM_WARN(CAM_ISP,
			"Configuring scratch for VFE out_type: %u, with no BW limiter enabled",
			vfe_out_data->out_type);

	for (i = 0; i < vfe_out_data->num_wm; i++) {
		wm_data = vfe_out_data->wm_res[i].res_priv;
		ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)
			wm_data->hw_regs->ubwc_regs;

		stride =  update_buf->wm_update->stride;
		val = stride;
		val = ALIGNUP(val, 16);
		if (val != stride &&
			val != wm_data->stride)
			CAM_WARN(CAM_SFE, "Warning stride %u expected %u",
				stride, val);

		if (wm_data->stride != val || !wm_data->init_cfg_done) {
			cam_io_w_mb(stride, wm_data->common_data->mem_base +
				wm_data->hw_regs->image_cfg_2);
			wm_data->stride = val;
			CAM_DBG(CAM_ISP, "WM:%d image stride 0x%x",
				wm_data->index, stride);
		}

		/* WM Image address */
		iova = update_buf->wm_update->image_buf[i];
		if (cam_smmu_is_expanded_memory()) {
			iova_addr = CAM_36BIT_INTF_GET_IOVA_BASE(iova);
			iova_offset = CAM_36BIT_INTF_GET_IOVA_OFFSET(iova);

			cam_io_w_mb(iova_addr, wm_data->common_data->mem_base +
				wm_data->hw_regs->image_addr);
			cam_io_w_mb(iova_offset, wm_data->common_data->mem_base +
				wm_data->hw_regs->addr_cfg);

			CAM_DBG(CAM_ISP, "WM:%d image address 0x%x 0x%x",
				wm_data->index, iova_addr, iova_offset);
		} else {
			iova_addr = iova;
			cam_io_w_mb(iova_addr, wm_data->common_data->mem_base +
				wm_data->hw_regs->image_addr);
			CAM_DBG(CAM_ISP, "WM:%d image address 0x%X",
				wm_data->index, iova_addr);
		}

		if (wm_data->en_ubwc) {
			if (!wm_data->hw_regs->ubwc_regs) {
				CAM_ERR(CAM_ISP,
					"No UBWC register to configure for WM: %u",
					wm_data->index);
				return -EINVAL;
			}

			if (wm_data->ubwc_updated) {
				wm_data->ubwc_updated = false;
				cam_vfe_bus_ver3_config_ubwc_regs(wm_data);
			}

			cam_io_w_mb(iova_addr, wm_data->common_data->mem_base +
				ubwc_regs->meta_addr);
			CAM_DBG(CAM_ISP, "WM:%d meta address 0x%x",
				wm_data->index, iova_addr);
		}

		/* enable the WM */
		cam_io_w_mb(wm_data->en_cfg, wm_data->common_data->mem_base +
			wm_data->hw_regs->cfg);
		CAM_DBG(CAM_ISP, "WM:%d en_cfg 0x%x", wm_data->index, wm_data->en_cfg);
	}

	return 0;
}

static int cam_vfe_bus_ver3_update_wm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update         *update_buf;
	struct cam_buf_io_cfg                    *io_cfg;
	struct cam_vfe_bus_ver3_vfe_out_data     *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data = NULL;
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_client = NULL;
	struct cam_cdm_utils_ops                       *cdm_util_ops;
	uint32_t *reg_val_pair;
	uint32_t num_regval_pairs = 0;
	uint32_t i, j, size = 0;
	uint32_t frame_inc = 0, val;
	uint32_t iova_addr, iova_offset, image_buf_offset = 0, stride, slice_h;
	dma_addr_t iova;

	bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
	update_buf = (struct cam_isp_hw_get_cmd_update *) cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		update_buf->res->res_priv;
	if (!vfe_out_data || !vfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid data");
		return -EINVAL;
	}

	cdm_util_ops = vfe_out_data->cdm_util_ops;
	if ((update_buf->wm_update->num_buf != vfe_out_data->num_wm) &&
		(!(update_buf->use_scratch_cfg))) {
		CAM_ERR(CAM_ISP,
			"Failed! Invalid number buffers:%d required:%d",
			update_buf->wm_update->num_buf, vfe_out_data->num_wm);
		return -EINVAL;
	}

	reg_val_pair = &vfe_out_data->common_data->io_buf_update[0];
	if (update_buf->use_scratch_cfg) {
		CAM_DBG(CAM_ISP, "Using scratch for IFE out_type: %u",
			vfe_out_data->out_type);

		if (!vfe_out_data->limiter_enabled)
			CAM_WARN(CAM_ISP,
				"Configuring scratch for VFE out_type: %u, with no BW limiter enabled",
				vfe_out_data->out_type);
	} else {
		io_cfg = update_buf->wm_update->io_cfg;
	}

	for (i = 0, j = 0; i < vfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = vfe_out_data->wm_res[i].res_priv;
		ubwc_client = wm_data->hw_regs->ubwc_regs;

		/* Disable frame header in case it was previously enabled */
		if ((wm_data->en_cfg) & (1 << 2))
			wm_data->en_cfg &= ~(1 << 2);

		if (update_buf->wm_update->frame_header &&
			!update_buf->wm_update->fh_enabled &&
			wm_data->hw_regs->frame_header_addr) {

			wm_data->en_cfg |= 1 << 2;
			update_buf->wm_update->fh_enabled = true;
			if (cam_smmu_is_expanded_memory()) {
				iova_addr = CAM_36BIT_INTF_GET_IOVA_BASE(
					update_buf->wm_update->frame_header);
				iova_offset = CAM_36BIT_INTF_GET_IOVA_OFFSET(
					update_buf->wm_update->frame_header);
			} else {
				iova_addr = update_buf->wm_update->frame_header;
				iova_offset = 0;
			}

			if (iova_offset)
				CAM_ERR(CAM_ISP, "Error, address not aligned! offset:0x%x",
					iova_offset);
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->frame_header_addr, iova_addr);
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->frame_header_cfg,
				update_buf->wm_update->local_id);
			CAM_DBG(CAM_ISP,
				"WM: %d en_cfg 0x%x frame_header %pK local_id %u",
				wm_data->index, wm_data->en_cfg,
				update_buf->wm_update->frame_header,
				update_buf->wm_update->local_id);
		}

		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->cfg, wm_data->en_cfg);
		CAM_DBG(CAM_ISP, "WM:%d %s en_cfg 0x%X",
			wm_data->index, vfe_out_data->wm_res[i].res_name,
			reg_val_pair[j-1]);

		val = (wm_data->height << 16) | wm_data->width;
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->image_cfg_0, val);
		CAM_DBG(CAM_ISP, "WM:%d image height and width 0x%X",
			wm_data->index, reg_val_pair[j-1]);

		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->packer_cfg, wm_data->pack_fmt);
		CAM_DBG(CAM_ISP, "WM:%d pack_fmt 0x%X",
			wm_data->index, reg_val_pair[j-1]);

		/* For initial configuration program all bus registers */
		if (update_buf->use_scratch_cfg) {
			stride = update_buf->wm_update->stride;
			slice_h = update_buf->wm_update->slice_height;
		} else {
			stride = io_cfg->planes[i].plane_stride;
			slice_h = io_cfg->planes[i].slice_height;
		}

		val = stride;
		CAM_DBG(CAM_ISP, "before stride %d", val);
		val = ALIGNUP(val, 16);
		if (val != stride)
			CAM_DBG(CAM_ISP, "Warning stride %u expected %u",
				stride, val);

		if (wm_data->stride != val || !wm_data->init_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_cfg_2,
				stride);
			wm_data->stride = val;
			CAM_DBG(CAM_ISP, "WM:%d image stride 0x%X",
				wm_data->index, reg_val_pair[j-1]);
		}

		if (wm_data->en_ubwc) {
			if (!wm_data->hw_regs->ubwc_regs) {
				CAM_ERR(CAM_ISP,
					"No UBWC register to configure.");
				return -EINVAL;
			}
			if (wm_data->ubwc_updated) {
				wm_data->ubwc_updated = false;
				cam_vfe_bus_ver3_update_ubwc_regs(
					wm_data, reg_val_pair, i, &j);
			}

			/* UBWC meta address */
			cam_vfe_bus_ver3_update_ubwc_meta_addr(
				reg_val_pair, &j,
				wm_data->hw_regs->ubwc_regs,
				update_buf->wm_update->image_buf[i]);
			CAM_DBG(CAM_ISP, "WM:%d ubwc meta addr 0x%llx",
				wm_data->index,
				update_buf->wm_update->image_buf[i]);
		}

		frame_inc = stride * slice_h;
		if (wm_data->en_ubwc) {
			frame_inc = ALIGNUP(stride *
				slice_h, 4096);

			if (!update_buf->use_scratch_cfg) {
				frame_inc += io_cfg->planes[i].meta_size;
				CAM_DBG(CAM_ISP,
					"WM:%d frm %d: ht: %d stride %d meta: %d",
					wm_data->index, frame_inc,
					io_cfg->planes[i].slice_height,
					io_cfg->planes[i].plane_stride,
					io_cfg->planes[i].meta_size);
			}
		}

		if (!(wm_data->en_cfg & (0x3 << 16))) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_cfg_1, wm_data->h_init);
			CAM_DBG(CAM_ISP, "WM:%d h_init 0x%X",
				wm_data->index, reg_val_pair[j-1]);
		}

		if ((wm_data->en_ubwc) && (!update_buf->use_scratch_cfg))
			image_buf_offset = io_cfg->planes[i].meta_size;
		else if (wm_data->en_cfg & (0x3 << 16))
			image_buf_offset = wm_data->offset;
		else
			image_buf_offset = 0;

		/* WM Image address */
		iova = update_buf->wm_update->image_buf[i] +
			image_buf_offset;

		if (cam_smmu_is_expanded_memory()) {
			iova_addr = CAM_36BIT_INTF_GET_IOVA_BASE(iova);
			iova_offset = CAM_36BIT_INTF_GET_IOVA_OFFSET(iova);

			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_addr, iova_addr);

			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->addr_cfg, iova_offset);

			CAM_DBG(CAM_ISP, "WM:%d image address 0x%X 0x%X",
				wm_data->index, reg_val_pair[j-2], reg_val_pair[j-1]);
		} else {
			iova_addr = iova;

			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_addr, iova_addr);

			CAM_DBG(CAM_ISP, "WM:%d image address 0x%X",
				wm_data->index, reg_val_pair[j-1]);
		}

		update_buf->wm_update->image_buf_offset[i] = image_buf_offset;

		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->frame_incr, frame_inc);
		CAM_DBG(CAM_ISP, "WM:%d frame_inc %d",
			wm_data->index, reg_val_pair[j-1]);


		/* enable the WM */
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->cfg,
			wm_data->en_cfg);

		/* set initial configuration done */
		if (!wm_data->init_cfg_done)
			wm_data->init_cfg_done = true;

		if (wm_data->tunnel_en) {
			if (wm_data->tunnel_id) {
				/* program tunneling id */
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->common_data->common_reg->if_frameheader_cfg[wm_data->hw_regs->tunnel_cfg_idx],
					wm_data->tunnel_id);
				CAM_DBG(CAM_ISP, "programmed tunneling Id :%d wm: %d",
					wm_data->tunnel_id, wm_data->index);
				trace_cam_tunnel_id(wm_data->tunnel_id, update_buf->req_id);
				wm_data->tunnel_id = 0;
			}
			else {
				CAM_ERR(CAM_ISP,
					"Tunnel enabled with zero Tunnel_id, for wm:%d res_id %d", wm_data->index);
				return -EINVAL;
			}
		}
	}

	num_regval_pairs = j / 2;
	if (num_regval_pairs) {
		size = cdm_util_ops->cdm_required_size_reg_random(
			num_regval_pairs);

		/* cdm util returns dwords, need to convert to bytes */
		if ((size * 4) > update_buf->cmd.size) {
			CAM_ERR(CAM_ISP,
				"Failed! Buf size:%d insufficient, expected size:%d",
				update_buf->cmd.size, size);
			return -ENOMEM;
		}

		cdm_util_ops->cdm_write_regrandom(
			update_buf->cmd.cmd_buf_addr,
			num_regval_pairs, reg_val_pair);

		/* cdm util returns dwords, need to convert to bytes */
		update_buf->cmd.used_bytes = size * 4;
	} else {
		CAM_DBG(CAM_ISP,
			"No reg val pairs. num_wms: %u",
			vfe_out_data->num_wm);
		update_buf->cmd.used_bytes = 0;
	}

	return 0;
}

static int cam_vfe_bus_ver3_update_hfr(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update         *update_hfr;
	struct cam_vfe_bus_ver3_vfe_out_data     *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data = NULL;
	struct cam_isp_port_hfr_config           *hfr_cfg = NULL;
	struct cam_cdm_utils_ops                 *cdm_util_ops;
	uint32_t *reg_val_pair;
	uint32_t num_regval_pairs = 0;
	uint32_t  i, j, size = 0;

	bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
	update_hfr =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		update_hfr->res->res_priv;

	if (!vfe_out_data || !vfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid data");
		return -EINVAL;
	}

	cdm_util_ops = vfe_out_data->cdm_util_ops;
	reg_val_pair = &vfe_out_data->common_data->io_buf_update[0];
	hfr_cfg = (struct cam_isp_port_hfr_config *)update_hfr->data;

	for (i = 0, j = 0; i < vfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = vfe_out_data->wm_res[i].res_priv;

		if (((!bus_priv->common_data.is_lite && wm_data->index > 22) ||
			bus_priv->common_data.is_lite) &&
			hfr_cfg->subsample_period > 3) {
			CAM_ERR(CAM_ISP,
				"RDI doesn't support irq subsample period %d",
				hfr_cfg->subsample_period);
			return -EINVAL;
		}

		if ((wm_data->framedrop_pattern !=
			hfr_cfg->framedrop_pattern) ||
			!wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_pattern,
				hfr_cfg->framedrop_pattern);
			wm_data->framedrop_pattern = hfr_cfg->framedrop_pattern;
			CAM_DBG(CAM_ISP, "WM:%d framedrop pattern 0x%X",
				wm_data->index, wm_data->framedrop_pattern);
		}

		if (wm_data->framedrop_period != hfr_cfg->framedrop_period ||
			!wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_period,
				hfr_cfg->framedrop_period);
			wm_data->framedrop_period = hfr_cfg->framedrop_period;
			CAM_DBG(CAM_ISP, "WM:%d framedrop period 0x%X",
				wm_data->index, wm_data->framedrop_period);
		}

		if (wm_data->irq_subsample_period != hfr_cfg->subsample_period
			|| !wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_period,
				hfr_cfg->subsample_period);
			wm_data->irq_subsample_period =
				hfr_cfg->subsample_period;
			CAM_DBG(CAM_ISP, "WM:%d irq subsample period 0x%X",
				wm_data->index, wm_data->irq_subsample_period);
		}

		if (wm_data->irq_subsample_pattern != hfr_cfg->subsample_pattern
			|| !wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_pattern,
				hfr_cfg->subsample_pattern);
			wm_data->irq_subsample_pattern =
				hfr_cfg->subsample_pattern;
			CAM_DBG(CAM_ISP, "WM:%d irq subsample pattern 0x%X",
				wm_data->index, wm_data->irq_subsample_pattern);
		}

		/* set initial configuration done */
		if (!wm_data->hfr_cfg_done)
			wm_data->hfr_cfg_done = true;
	}

	num_regval_pairs = j / 2;

	if (num_regval_pairs) {
		size = cdm_util_ops->cdm_required_size_reg_random(
			num_regval_pairs);

		/* cdm util returns dwords, need to convert to bytes */
		if ((size * 4) > update_hfr->cmd.size) {
			CAM_ERR(CAM_ISP,
				"Failed! Buf size:%d insufficient, expected size:%d",
				update_hfr->cmd.size, size);
			return -ENOMEM;
		}

		cdm_util_ops->cdm_write_regrandom(
			update_hfr->cmd.cmd_buf_addr,
			num_regval_pairs, reg_val_pair);

		/* cdm util returns dwords, need to convert to bytes */
		update_hfr->cmd.used_bytes = size * 4;
	} else {
		update_hfr->cmd.used_bytes = 0;
		CAM_DBG(CAM_ISP,
			"No reg val pairs. num_wms: %u",
			vfe_out_data->num_wm);
	}

	return 0;
}

static int cam_vfe_bus_ver3_update_ubwc_config_v2(void *cmd_args)
{
	struct cam_isp_hw_get_cmd_update         *update_ubwc;
	struct cam_vfe_bus_ver3_vfe_out_data     *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data = NULL;
	struct cam_vfe_generic_ubwc_config       *ubwc_generic_cfg = NULL;
	struct cam_vfe_generic_ubwc_plane_config *ubwc_generic_plane_cfg = NULL;
	uint32_t                                  i;
	int                                       rc = 0;

	if (!cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		rc = -EINVAL;
		goto end;
	}

	update_ubwc =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		update_ubwc->res->res_priv;

	if (!vfe_out_data || !vfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid data");
		rc = -EINVAL;
		goto end;
	}

	ubwc_generic_cfg = (struct cam_vfe_generic_ubwc_config *)
		update_ubwc->data;

	for (i = 0; i < vfe_out_data->num_wm; i++) {

		wm_data = vfe_out_data->wm_res[i].res_priv;
		ubwc_generic_plane_cfg = &ubwc_generic_cfg->ubwc_plane_cfg[i];

		if (!wm_data->hw_regs->ubwc_regs) {
			CAM_ERR(CAM_ISP,
				"No UBWC register to configure.");
			rc = -EINVAL;
			goto end;
		}

		if (!wm_data->en_ubwc) {
			CAM_ERR(CAM_ISP, "UBWC Disabled");
			rc = -EINVAL;
			goto end;
		}

		if (wm_data->packer_cfg !=
			ubwc_generic_plane_cfg->packer_config ||
			!wm_data->init_cfg_done) {
			wm_data->packer_cfg =
				ubwc_generic_plane_cfg->packer_config;
			wm_data->ubwc_updated = true;
		}

		if ((!wm_data->is_dual) && ((wm_data->h_init !=
			ubwc_generic_plane_cfg->h_init) ||
			!wm_data->init_cfg_done)) {
			wm_data->h_init = ubwc_generic_plane_cfg->h_init;
			wm_data->ubwc_updated = true;
		}

		if (wm_data->ubwc_meta_cfg !=
			ubwc_generic_plane_cfg->meta_stride ||
			!wm_data->init_cfg_done) {
			wm_data->ubwc_meta_cfg =
				ubwc_generic_plane_cfg->meta_stride;
			wm_data->ubwc_updated = true;
		}

		if (wm_data->ubwc_mode_cfg !=
			ubwc_generic_plane_cfg->mode_config_0 ||
			!wm_data->init_cfg_done) {
			wm_data->ubwc_mode_cfg =
				ubwc_generic_plane_cfg->mode_config_0;
			wm_data->ubwc_updated = true;
		}

		if (wm_data->ubwc_ctrl_2 !=
			ubwc_generic_plane_cfg->ctrl_2 ||
			!wm_data->init_cfg_done) {
			wm_data->ubwc_ctrl_2 =
				ubwc_generic_plane_cfg->ctrl_2;
			wm_data->ubwc_updated = true;
		}

		if (wm_data->ubwc_lossy_threshold_0 !=
			ubwc_generic_plane_cfg->lossy_threshold_0 ||
			!wm_data->init_cfg_done) {
			wm_data->ubwc_lossy_threshold_0 =
				ubwc_generic_plane_cfg->lossy_threshold_0;
			wm_data->ubwc_updated = true;
		}

		if (wm_data->ubwc_lossy_threshold_1 !=
			ubwc_generic_plane_cfg->lossy_threshold_1 ||
			!wm_data->init_cfg_done) {
			wm_data->ubwc_lossy_threshold_1 =
				ubwc_generic_plane_cfg->lossy_threshold_1;
			wm_data->ubwc_updated = true;
		}

		if (wm_data->ubwc_offset_lossy_variance !=
			ubwc_generic_plane_cfg->lossy_var_offset ||
			!wm_data->init_cfg_done) {
			wm_data->ubwc_offset_lossy_variance =
				ubwc_generic_plane_cfg->lossy_var_offset;
			wm_data->ubwc_updated = true;
		}

		if (wm_data->ubwc_bandwidth_limit !=
			ubwc_generic_plane_cfg->bandwidth_limit ||
			!wm_data->init_cfg_done) {
			wm_data->ubwc_bandwidth_limit =
				ubwc_generic_plane_cfg->bandwidth_limit;
			wm_data->ubwc_updated = true;
		}
	}

end:
	return rc;
}

static int cam_vfe_bus_ver3_update_stripe_cfg(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv                *bus_priv;
	struct cam_isp_hw_dual_isp_update_args      *stripe_args;
	struct cam_vfe_bus_ver3_vfe_out_data        *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data    *wm_data = NULL;
	struct cam_isp_dual_stripe_config           *stripe_config;
	uint32_t outport_id, ports_plane_idx, i;

	bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
	stripe_args = (struct cam_isp_hw_dual_isp_update_args *)cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		stripe_args->res->res_priv;

	if (!vfe_out_data) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	outport_id = stripe_args->res->res_id & 0xFF;
	if (stripe_args->res->res_id < CAM_ISP_IFE_OUT_RES_BASE ||
		stripe_args->res->res_id >= bus_priv->max_out_res)
		return 0;

	ports_plane_idx = (stripe_args->split_id *
	(stripe_args->dual_cfg->num_ports * CAM_PACKET_MAX_PLANES)) +
	(outport_id * CAM_PACKET_MAX_PLANES);
	for (i = 0; i < vfe_out_data->num_wm; i++) {
		wm_data = vfe_out_data->wm_res[i].res_priv;
		stripe_config = (struct cam_isp_dual_stripe_config  *)
			&stripe_args->dual_cfg->stripes[ports_plane_idx + i];
		wm_data->width = stripe_config->width;

		/*
		 * UMD sends buffer offset address as offset for clients
		 * programmed to operate in frame/index based mode and h_init
		 * value as offset for clients programmed to operate in line
		 * based mode.
		 */

		if (wm_data->en_cfg & (0x3 << 16))
			wm_data->offset = stripe_config->offset;
		else
			wm_data->h_init = stripe_config->offset;

		CAM_DBG(CAM_ISP,
			"out_type:0x%X WM:%d width:%d offset:0x%X h_init:%d",
			stripe_args->res->res_id, wm_data->index,
			wm_data->width, wm_data->offset, wm_data->h_init);
	}

	return 0;
}

static int cam_vfe_bus_ver3_update_wm_config(
	void                                        *cmd_args)
{
	int                                          i;
	struct cam_isp_hw_get_cmd_update            *wm_config_update;
	struct cam_vfe_bus_ver3_vfe_out_data        *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data    *wm_data = NULL;
	struct cam_isp_vfe_wm_config                *wm_config = NULL;

	if (!cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	wm_config_update = cmd_args;
	vfe_out_data = wm_config_update->res->res_priv;
	wm_config = (struct cam_isp_vfe_wm_config  *)
		wm_config_update->data;

	if (!vfe_out_data || !vfe_out_data->cdm_util_ops || !wm_config) {
		CAM_ERR(CAM_ISP, "Invalid data");
		return -EINVAL;
	}

	for (i = 0; i < vfe_out_data->num_wm; i++) {
		wm_data = vfe_out_data->wm_res[i].res_priv;

		if (wm_config->wm_mode > 0x2) {
			CAM_ERR(CAM_ISP, "Invalid wm_mode: 0x%X WM:%d",
				wm_config->wm_mode, wm_data->index);
			return -EINVAL;
		}

		wm_data->en_cfg = ((wm_config->wm_mode << 16) |
			(wm_config->virtual_frame_en << 1) | 0x1);
		wm_data->width  = wm_config->width;

		if (i == PLANE_C)
			wm_data->height = wm_config->height / 2;
		else
			wm_data->height = wm_config->height;

		CAM_DBG(CAM_ISP,
			"WM:%d en_cfg:0x%X height:%d width:%d",
			wm_data->index, wm_data->en_cfg, wm_data->height,
			wm_data->width);
	}

	return 0;
}

static int cam_vfe_bus_ver3_update_tunnel_id(
	void                         *cmd_args)
{
	int                                          i;
	struct cam_isp_hw_get_cmd_update            *tunnel_id_update;
	struct cam_vfe_bus_ver3_vfe_out_data        *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data    *wm_data = NULL;
	struct cam_isp_tunnel_id_config             *tunnel_config = NULL;

	if (!cmd_args) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	tunnel_id_update = cmd_args;
	vfe_out_data = tunnel_id_update->res->res_priv;
	tunnel_config = (struct cam_isp_tunnel_id_config  *)
		tunnel_id_update->data;

	if (!vfe_out_data || !vfe_out_data->cdm_util_ops || !tunnel_config) {
		CAM_ERR(CAM_ISP, "Invalid args vfe_out_data[%pK] tunnel_config[%pK]",
			vfe_out_data, tunnel_config);
		return -EINVAL;
	}

	for (i = 0; i < vfe_out_data->num_wm; i++) {
		wm_data = vfe_out_data->wm_res[i].res_priv;

		if (wm_data->tunnel_en)
			wm_data->tunnel_id = tunnel_config->tunnel_id;
		else {
			CAM_ERR(CAM_ISP,
				"Received tunnel_id[%u] for WM:%d port:%d with Tunnel Disable",
				tunnel_config->tunnel_id, wm_data->index, vfe_out_data->out_type);
			return -EINVAL;
		}

		CAM_DBG(CAM_ISP, "WM:%d tunnel_id %d", wm_data->index, wm_data->tunnel_id);
	}

	return 0;
}

static int cam_vfe_bus_update_bw_limiter(
	void *priv, void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update         *wm_config_update;
	struct cam_vfe_bus_ver3_vfe_out_data     *vfe_out_data = NULL;
	struct cam_cdm_utils_ops                 *cdm_util_ops;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data = NULL;
	struct cam_isp_wm_bw_limiter_config      *wm_bw_limit_cfg = NULL;
	uint32_t                                  counter_limit = 0, reg_val = 0;
	uint32_t                                 *reg_val_pair, num_regval_pairs = 0;
	uint32_t                                  i, j, size = 0;
	bool                                      limiter_enabled = false;

	bus_priv         = (struct cam_vfe_bus_ver3_priv  *) priv;
	wm_config_update = (struct cam_isp_hw_get_cmd_update *) cmd_args;
	wm_bw_limit_cfg  = (struct cam_isp_wm_bw_limiter_config  *)
			wm_config_update->data;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		wm_config_update->res->res_priv;
	if (!vfe_out_data || !vfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Invalid data");
		return -EINVAL;
	}

	cdm_util_ops = vfe_out_data->cdm_util_ops;
	reg_val_pair = &vfe_out_data->common_data->io_buf_update[0];
	for (i = 0, j = 0; i < vfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - (MAX_BUF_UPDATE_REG_NUM * 2))) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %zu for WM idx %d",
				j, MAX_REG_VAL_PAIR_SIZE, i);
			return -ENOMEM;
		}

		/* Num WMs needs to match max planes */
		if (i >= CAM_PACKET_MAX_PLANES) {
			CAM_WARN(CAM_ISP,
				"Num of WMs: %d exceeded max planes", i);
			goto add_reg_pair;
		}

		wm_data = (struct cam_vfe_bus_ver3_wm_resource_data *)
			vfe_out_data->wm_res[i].res_priv;
		if (!wm_data->hw_regs->bw_limiter_addr) {
			CAM_ERR(CAM_ISP,
				"WM: %d %s has no support for bw limiter",
				wm_data->index, vfe_out_data->wm_res[i].res_name);
			return -EINVAL;
		}

		counter_limit = wm_bw_limit_cfg->counter_limit[i];

		/* Validate max counter limit */
		if (counter_limit >
			wm_data->common_data->max_bw_counter_limit) {
			CAM_WARN(CAM_ISP,
				"Invalid counter limit: 0x%x capping to max: 0x%x",
				wm_bw_limit_cfg->counter_limit[i],
				wm_data->common_data->max_bw_counter_limit);
			counter_limit =
				wm_data->common_data->max_bw_counter_limit;
		}

		if (wm_bw_limit_cfg->enable_limiter && counter_limit) {
			reg_val = 1;
			reg_val |= (counter_limit << 1);
			limiter_enabled = true;
		} else {
			reg_val = 0;
		}

		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->bw_limiter_addr, reg_val);
		CAM_DBG(CAM_ISP, "WM: %d for %s bw_limter: 0x%x",
			wm_data->index, vfe_out_data->wm_res[i].res_name,
			reg_val_pair[j-1]);
	}

add_reg_pair:

	num_regval_pairs = j / 2;

	if (num_regval_pairs) {
		size = cdm_util_ops->cdm_required_size_reg_random(
			num_regval_pairs);

		/* cdm util returns dwords, need to convert to bytes */
		if ((size * 4) > wm_config_update->cmd.size) {
			CAM_ERR(CAM_ISP,
				"Failed! Buf size:%d insufficient, expected size:%d",
				wm_config_update->cmd.size, size);
			return -ENOMEM;
		}

		cdm_util_ops->cdm_write_regrandom(
			wm_config_update->cmd.cmd_buf_addr, num_regval_pairs,
			reg_val_pair);

		/* cdm util returns dwords, need to convert to bytes */
		wm_config_update->cmd.used_bytes = size * 4;
	} else {
		CAM_DBG(CAM_ISP,
			"No reg val pairs. num_wms: %u",
			vfe_out_data->num_wm);
		wm_config_update->cmd.used_bytes = 0;
	}

	vfe_out_data->limiter_enabled = limiter_enabled;
	return 0;
}

static int cam_vfe_bus_ver3_update_res_wm(
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv,
	struct cam_vfe_hw_vfe_out_acquire_args *out_acq_args,
	void                                   *workq,
	enum cam_vfe_bus_ver3_vfe_out_type      vfe_out_res_id,
	enum cam_vfe_bus_plane_type             plane,
	struct cam_isp_resource_node           *wm_res,
	enum cam_vfe_bus_ver3_comp_grp_type   *comp_grp_id)
{
	int32_t wm_idx = 0, rc;
	struct cam_vfe_bus_ver3_wm_resource_data  *rsrc_data = NULL;
	char wm_mode[50];

	memset(wm_mode, '\0', sizeof(wm_mode));

	wm_res->is_per_port_acquire = false;
	rsrc_data = wm_res->res_priv;
	wm_idx = rsrc_data->index;

	if ((vfe_out_res_id >= CAM_VFE_BUS_VER3_VFE_OUT_RDI0) &&
		(vfe_out_res_id <= CAM_VFE_BUS_VER3_VFE_OUT_RDI3)) {
		if (out_acq_args->disable_line_based_mode)
			rsrc_data->default_line_based =
				!(out_acq_args->disable_line_based_mode);
	}

	rsrc_data->format = out_acq_args->out_port_info->format;
	rsrc_data->use_wm_pack = out_acq_args->use_wm_pack;
	rsrc_data->pack_fmt = cam_vfe_bus_ver3_get_packer_fmt(rsrc_data->format,
		wm_idx);

	rsrc_data->width = out_acq_args->out_port_info->width;
	rsrc_data->height = out_acq_args->out_port_info->height;
	rsrc_data->acquired_width = out_acq_args->out_port_info->width;
	rsrc_data->acquired_height = out_acq_args->out_port_info->height;
	rsrc_data->is_dual = out_acq_args->is_dual;
	if (ver3_bus_priv->common_data.support_tunneling)
		rsrc_data->tunnel_en = out_acq_args->out_port_info->tunnel_en;

	/* Set WM offset value to default */
	rsrc_data->offset  = 0;
	CAM_DBG(CAM_ISP, "WM:%d width %d height %d is_per_port:%d default_line_based:%d",
		rsrc_data->index, rsrc_data->width,
		rsrc_data->height, wm_res->is_per_port_acquire,
		rsrc_data->default_line_based);

	rc = cam_vfe_bus_ver3_res_update_config_wm(ver3_bus_priv, vfe_out_res_id,
		plane, wm_res, comp_grp_id, wm_mode, sizeof(wm_mode));
	if (rc)
		return rc;

	wm_res->workq_info = workq;
	CAM_DBG(CAM_ISP,
		"VFE:%d WM:%d %s processed width:%d height:%d stride:%d format:0x%X en_ubwc:%d %s",
		rsrc_data->common_data->core_index, rsrc_data->index,
		wm_res->res_name, rsrc_data->width, rsrc_data->height,
		rsrc_data->stride, rsrc_data->format, rsrc_data->en_ubwc,
		wm_mode);
	return 0;
}

static int cam_vfe_bus_ver3_update_res_comp_grp(
	struct cam_vfe_bus_ver3_priv         *ver3_bus_priv,
	void                                *workq,
	uint32_t                             is_dual,
	uint32_t                             is_master,
	struct cam_isp_resource_node       **comp_grp,
	struct cam_vfe_bus_ver3_comp_grp_acquire_args *comp_acq_args)
{
	struct cam_isp_resource_node           *comp_grp_local = NULL;
	struct cam_vfe_bus_ver3_comp_grp_data  *rsrc_data = NULL;
	bool previously_acquired = false;

	/* Check if matching comp_grp has already been acquired */
	previously_acquired = cam_vfe_bus_ver3_match_comp_grp(
		ver3_bus_priv, &comp_grp_local, comp_acq_args->comp_grp_id);

	if (!comp_grp_local || !previously_acquired) {
		CAM_ERR(CAM_ISP, "Invalid comp_grp:%d",
			comp_acq_args->comp_grp_id);
		return -ENODEV;
	}

	rsrc_data = comp_grp_local->res_priv;
	/* Do not support runtime change in composite mask */
	CAM_DBG(CAM_ISP, "Update res VFE:%d comp_grp:%u",
		rsrc_data->common_data->core_index, rsrc_data->comp_grp_type);

	rsrc_data->acquire_dev_cnt++;
	rsrc_data->composite_mask |= comp_acq_args->composite_mask;
	*comp_grp = comp_grp_local;
	comp_grp_local->is_per_port_acquire = false;

	return 0;
}

static int cam_vfe_bus_ver3_update_res_vfe_out(void *bus_priv, void *acquire_args,
	uint32_t args_size)
{
	int                                     rc = -ENODEV;
	int                                     i;
	enum cam_vfe_bus_ver3_vfe_out_type      vfe_out_res_id;
	uint32_t                                format;
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv = bus_priv;
	struct cam_vfe_acquire_args            *acq_args;
	struct cam_vfe_resource_update		   *res_update_args;
	struct cam_vfe_hw_vfe_out_acquire_args *out_acquire_args;
	struct cam_isp_resource_node           *rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data   *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_comp_grp_acquire_args comp_acq_args = {0};
	uint32_t       outmap_index = CAM_VFE_BUS_VER3_VFE_OUT_MAX;

	if (!bus_priv || !acquire_args) {
		CAM_ERR(CAM_ISP, "Invalid Param");
		return -EINVAL;
	}

	res_update_args = (struct cam_vfe_resource_update *)acquire_args;
	acq_args = res_update_args->vfe_acquire;

	out_acquire_args = &acq_args->vfe_out;
	out_acquire_args->disable_line_based_mode = res_update_args->disable_line_based_mode;
	format = out_acquire_args->out_port_info->format;

	CAM_DBG(CAM_ISP, "VFE:%d Acquire out_type:0x%X",
		ver3_bus_priv->common_data.core_index,
		out_acquire_args->out_port_info->acquired_res_type);

	vfe_out_res_id = cam_vfe_bus_ver3_get_out_res_id_and_index(
				ver3_bus_priv,
				out_acquire_args->out_port_info->acquired_res_type,
				&outmap_index);
	if ((vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_MAX) ||
		(outmap_index >= ver3_bus_priv->num_out)) {
		CAM_WARN(CAM_ISP,
			"target does not support req res id :0x%x outtype:%d index:%d num: %d",
			out_acquire_args->out_port_info->acquired_res_type,
			vfe_out_res_id, outmap_index, ver3_bus_priv->num_out);
		return -ENODEV;
	}

	rsrc_node = &ver3_bus_priv->vfe_out[outmap_index];

	rsrc_data = rsrc_node->res_priv;
	rsrc_data->common_data->event_cb = acq_args->event_cb;
	rsrc_data->common_data->priv = acq_args->priv;
	rsrc_data->common_data->disable_ubwc_comp =
		out_acquire_args->disable_ubwc_comp;
	rsrc_data->priv = acq_args->priv;
	rsrc_data->bus_priv = ver3_bus_priv;
	rsrc_data->limiter_enabled = false;

	comp_acq_args.composite_mask = (1ULL << vfe_out_res_id);

	/* for some hw versions, buf done is not received from vfe but
	 * from IP external to VFE. In such case, we get the controller
	 * from hw manager and assign it here
	 */
	if (!(ver3_bus_priv->common_data.supported_irq &
			CAM_VFE_HW_IRQ_CAP_BUF_DONE))
		rsrc_data->common_data->buf_done_controller =
			acq_args->buf_done_controller;

	ver3_bus_priv->workq_info = acq_args->workq;
	rsrc_node->rdi_only_ctx = 0;
	rsrc_node->res_id = out_acquire_args->out_port_info->acquired_res_type;
	rsrc_node->workq_info = acq_args->workq;
	rsrc_node->cdm_ops = out_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = out_acquire_args->cdm_ops;
	rsrc_data->common_data->event_cb = acq_args->event_cb;

	rsrc_node->workq_info = acq_args->workq;
	rsrc_data->format = out_acquire_args->out_port_info->format;

	if ((rsrc_data->out_type == CAM_VFE_BUS_VER3_VFE_OUT_FD) &&
		(rsrc_data->format == CAM_FORMAT_Y_ONLY))
		rsrc_data->num_wm = 1;

	/* Update WM params and retrieve COMP GRP ID */
	for (i = 0; i < rsrc_data->num_wm; i++) {
		rc = cam_vfe_bus_ver3_update_res_wm(ver3_bus_priv,
			out_acquire_args,
			acq_args->workq,
			vfe_out_res_id,
			i,
			&rsrc_data->wm_res[i],
			&comp_acq_args.comp_grp_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"Failed to update resource WM VFE:%d out_type:%d rc:%d",
				rsrc_data->common_data->core_index,
				vfe_out_res_id, rc);
			return -EINVAL;
		}
	}


	/* Update composite group data using COMP GRP ID */
	rc = cam_vfe_bus_ver3_update_res_comp_grp(ver3_bus_priv,
		acq_args->workq,
		out_acquire_args->is_dual,
		out_acquire_args->is_master,
		&rsrc_data->comp_grp,
		&comp_acq_args);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Failed to update resource comp_grp VFE:%d out_typp:%d rc:%d",
			rsrc_data->common_data->core_index,
			vfe_out_res_id, rc);
		return -EINVAL;
	}

	rsrc_node->is_per_port_acquire = false;
	rsrc_data->is_dual = out_acquire_args->is_dual;
	rsrc_data->is_master = out_acquire_args->is_master;
	if (rsrc_data->secure_mask)
		rsrc_data->secure_mode = out_acquire_args->out_port_info->secure_mode;

	out_acquire_args->rsrc_node = rsrc_node;

	CAM_DBG(CAM_ISP, "Update res successful");
	return rc;
}

static int cam_vfe_bus_ver3_enable_irq_vfe_out(void *bus_priv, void *res_irq_mask)
{
	int rc = 0;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_common_data   *common_data = NULL;
	uint32_t source_group = 0;
	struct cam_isp_resource_node          *vfe_out;
	struct cam_csid_res_irq_info          *irq_args;

	if (!res_irq_mask) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	irq_args = (struct cam_csid_res_irq_info *)res_irq_mask;
	vfe_out = *irq_args->node_res;
	rsrc_data = vfe_out->res_priv;
	common_data = rsrc_data->common_data;
	source_group = rsrc_data->source_group;

	CAM_DBG(CAM_ISP, "Start VFE:%d out_type:0x%X",
		rsrc_data->common_data->core_index, rsrc_data->out_type);

	if (rsrc_data->is_dual && !rsrc_data->is_master)
		goto end;

	if (!vfe_out->irq_handle && !vfe_out->is_per_port_start) {
		vfe_out->irq_handle = cam_irq_controller_subscribe_irq(
			common_data->buf_done_controller,
			CAM_IRQ_PRIORITY_1,
			rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_BUF_DONE_MASK],
			vfe_out,
			vfe_out->top_half_handler,
			vfe_out->bottom_half_handler,
			vfe_out->workq_info,
			&workq_bh_api,
			CAM_IRQ_EVT_GROUP_0);
		if (vfe_out->irq_handle < 1) {
			CAM_ERR(CAM_ISP, "Subscribe IRQ failed for VFE out_res %d",
				vfe_out->res_id);
			vfe_out->irq_handle = 0;
			return -EFAULT;
		}

		if ((common_data->is_lite || source_group > CAM_VFE_BUS_VER3_SRC_GRP_0)
			&& !vfe_out->rdi_only_ctx) {
			goto end;
		}

		if ((common_data->supported_irq & CAM_VFE_HW_IRQ_CAP_RUP) &&
			(!common_data->rup_irq_handle[source_group])) {
			rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK]
			[CAM_VFE_BUS_VER3_IRQ_REG0] |= 0x1 << source_group;

			CAM_DBG(CAM_ISP,
				"VFE:%d out_type:0x%X bus_irq_mask_0:0x%X for RUP",
				rsrc_data->common_data->core_index, rsrc_data->out_type,
				rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK]
				[CAM_VFE_BUS_VER3_IRQ_REG0]);

			common_data->rup_irq_handle[source_group] =
				cam_irq_controller_subscribe_irq(
					common_data->bus_irq_controller,
					CAM_IRQ_PRIORITY_0,
					rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK],
					vfe_out,
					cam_vfe_bus_ver3_handle_rup_top_half,
					cam_vfe_bus_ver3_handle_rup_bottom_half,
					vfe_out->workq_info,
					&workq_bh_api,
					CAM_IRQ_EVT_GROUP_1);

			if (common_data->rup_irq_handle[source_group] < 1) {
				CAM_ERR(CAM_ISP, "Failed to subscribe RUP IRQ");
				common_data->rup_irq_handle[source_group] = 0;
				return -EFAULT;
			}
		}

	} else if (vfe_out->irq_handle) {
		rc = cam_irq_controller_update_irq(
			common_data->buf_done_controller,
			vfe_out->irq_handle,
			irq_args->enable_irq,
			rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_BUF_DONE_MASK]);

		if (rc) {
			CAM_ERR(CAM_ISP, "Update IRQ failed for VFE out_res %d",
				vfe_out->res_id);
			return -EFAULT;
		}

		if ((common_data->is_lite || source_group > CAM_VFE_BUS_VER3_SRC_GRP_0)
			&& !vfe_out->rdi_only_ctx) {
			goto end;
		}

		if ((common_data->supported_irq & CAM_VFE_HW_IRQ_CAP_RUP)) {
			rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK]
				[CAM_VFE_BUS_VER3_IRQ_REG0] |= 0x1 << source_group;

			rc = cam_irq_controller_update_irq(
					common_data->bus_irq_controller,
					common_data->rup_irq_handle[source_group],
					irq_args->enable_irq,
					rsrc_data->stored_irq_masks[CAM_VFE_BUS_VER3_RUP_MASK]);
			if (rc) {
				CAM_ERR(CAM_ISP, "Update IRQ failed for VFE out_res %d",
					vfe_out->res_id);
				return -EFAULT;
			}
		}
	} else {
		CAM_ERR(CAM_ISP, "VFE out_res irq handle not found");
		return -EINVAL;
	}

	if ((common_data->is_lite || source_group > CAM_VFE_BUS_VER3_SRC_GRP_0)
		&& !vfe_out->rdi_only_ctx)
		goto end;

end:
	return rc;
}

static int cam_vfe_bus_ver3_start_hw(void *hw_priv,
	void *start_hw_args, uint32_t arg_size)
{
	return cam_vfe_bus_ver3_start_vfe_out(hw_priv);
}

static int cam_vfe_bus_ver3_stop_hw(void *hw_priv,
	void *stop_hw_args, uint32_t arg_size)
{
	return cam_vfe_bus_ver3_stop_vfe_out(hw_priv);
}

static int cam_vfe_bus_ver3_init_hw(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv    *bus_priv = hw_priv;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	if (bus_priv->common_data.hw_init)
		return 0;

	/* We take the controller only if the buf done is supported on vfe side
	 * for some hw, it is taken from IP extenal to VFE like CSID
	 */
	if ((bus_priv->common_data.supported_irq & CAM_VFE_HW_IRQ_CAP_BUF_DONE))
		bus_priv->common_data.buf_done_controller =
			bus_priv->common_data.bus_irq_controller;

	bus_priv->common_data.hw_init = true;

	return 0;
}

static int cam_vfe_bus_ver3_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv    *bus_priv = hw_priv;
	int                              rc = 0, i;
	unsigned long                    flags;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Error: Invalid args");
		return -EINVAL;
	}

	if (!bus_priv->common_data.hw_init)
		return 0;

	spin_lock_irqsave(&bus_priv->common_data.spin_lock, flags);
	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_VFE_BUS_VER3_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);
		list_add_tail(&bus_priv->common_data.evt_payload[i].list,
			&bus_priv->common_data.free_payload_list);
	}
	bus_priv->common_data.hw_init = false;
	spin_unlock_irqrestore(&bus_priv->common_data.spin_lock, flags);

	return rc;
}

static int cam_vfe_bus_get_res_for_mid(
	struct cam_vfe_bus_ver3_priv *bus_priv,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_vfe_out_data   *out_data = NULL;
	struct cam_isp_hw_get_cmd_update       *cmd_update = cmd_args;
	struct cam_isp_hw_get_res_for_mid       *get_res = NULL;
	int i, j;

	get_res = (struct cam_isp_hw_get_res_for_mid *)cmd_update->data;
	if (!get_res) {
		CAM_ERR(CAM_ISP,
			"invalid get resource for mid paramas");
		return -EINVAL;
	}

	for (i = 0; i < bus_priv->num_out; i++) {
		out_data = (struct cam_vfe_bus_ver3_vfe_out_data   *)
			bus_priv->vfe_out[i].res_priv;

		if (!out_data)
			continue;

		for (j = 0; j < CAM_VFE_BUS_VER3_MAX_MID_PER_PORT; j++) {
			if (out_data->mid[j] == get_res->mid)
				goto end;
		}
	}

	if (i == bus_priv->num_out) {
		CAM_ERR(CAM_ISP,
			"mid:%d does not match with any out resource",
			get_res->mid);
		get_res->out_res_id = 0;
		return -EINVAL;
	}

end:
	CAM_INFO(CAM_ISP, "match mid :%d  out resource:0x%x found",
		get_res->mid, bus_priv->vfe_out[i].res_id);
	get_res->out_res_id = bus_priv->vfe_out[i].res_id;
	return 0;
}

static int __cam_vfe_bus_ver3_process_cmd(void *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	return cam_vfe_bus_ver3_process_cmd(priv, cmd_type, cmd_args, arg_size);
}

static int cam_vfe_bus_ver3_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;
	struct cam_vfe_bus_ver3_priv		 *bus_priv;
	uint32_t top_mask_0 = 0;
	struct cam_isp_hw_bus_cap *vfe_bus_cap;


	if (!priv || !cmd_args) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE:
		rc = cam_vfe_bus_ver3_update_wm(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE:
		rc = cam_vfe_bus_ver3_update_hfr(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_WM_SECURE_MODE:
		rc = cam_vfe_bus_ver3_get_secure_mode(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STRIPE_UPDATE:
		rc = cam_vfe_bus_ver3_update_stripe_cfg(priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ:
		bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
		if (bus_priv->error_irq_handle) {
			CAM_DBG(CAM_ISP, "Mask off bus error irq handler");
			rc = cam_irq_controller_unsubscribe_irq(
				bus_priv->common_data.bus_irq_controller,
				bus_priv->error_irq_handle);
			bus_priv->error_irq_handle = 0;
		}
		break;
	case CAM_ISP_HW_CMD_DUMP_BUS_INFO: {
		struct cam_isp_hw_event_info  *event_info;

		event_info =
			(struct cam_isp_hw_event_info *)cmd_args;
		bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;

		rc = cam_vfe_bus_ver3_print_dimensions(
			event_info->res_id, bus_priv);
		break;
		}
	case CAM_ISP_HW_BUS_MINI_DUMP: {
		bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;

		rc = cam_vfe_bus_ver3_mini_dump(bus_priv, cmd_args);
		break;
		}

	case CAM_ISP_HW_CMD_UBWC_UPDATE_V2:
		rc = cam_vfe_bus_ver3_update_ubwc_config_v2(cmd_args);
		break;
	case CAM_ISP_HW_CMD_WM_CONFIG_UPDATE:
		rc = cam_vfe_bus_ver3_update_wm_config(cmd_args);
		break;
	case CAM_ISP_HW_CMD_TUNNEL_ID_UPDATE:
		bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;

		if (bus_priv->common_data.support_tunneling)
			rc = cam_vfe_bus_ver3_update_tunnel_id(cmd_args);
		else
			CAM_ERR(CAM_ISP, "Invalid Tunneling_ID blob");
		break;
	case CAM_ISP_HW_CMD_UNMASK_BUS_WR_IRQ:
		bus_priv = (struct cam_vfe_bus_ver3_priv *) priv;
		top_mask_0 = cam_io_r_mb(bus_priv->common_data.mem_base +
			bus_priv->common_data.common_reg->top_irq_mask_0);
		top_mask_0 |= (1 << bus_priv->top_irq_shift);
		cam_io_w_mb(top_mask_0, bus_priv->common_data.mem_base +
			bus_priv->common_data.common_reg->top_irq_mask_0);
		break;
	case CAM_ISP_HW_CMD_GET_RES_FOR_MID:
		bus_priv = (struct cam_vfe_bus_ver3_priv *) priv;
		rc = cam_vfe_bus_get_res_for_mid(bus_priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_QUERY_BUS_CAP:
		bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
		vfe_bus_cap = (struct cam_isp_hw_bus_cap *) cmd_args;
		vfe_bus_cap->max_out_res_type = bus_priv->max_out_res;
		vfe_bus_cap->support_consumed_addr =
			bus_priv->common_data.support_consumed_addr;
		vfe_bus_cap->out_fifo_depth =
			bus_priv->common_data.out_fifo_depth;
		break;
	case CAM_ISP_HW_CMD_IFE_BUS_DEBUG_CFG:
		bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
		bus_priv->common_data.disable_mmu_prefetch =
			(*((bool *)cmd_args));
		CAM_DBG(CAM_ISP, "IFE: %u bus WR prefetch %s",
			bus_priv->common_data.core_index,
			bus_priv->common_data.disable_mmu_prefetch ?
			"disabled" : "enabled");
		rc = 0;
		break;
	case CAM_ISP_HW_CMD_BUF_UPDATE:
		rc = cam_vfe_bus_ver3_config_wm(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_WM_BW_LIMIT_CONFIG:
		rc = cam_vfe_bus_update_bw_limiter(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_UPDATE_VFE_OUT_RES_DATA:
		rc = cam_vfe_bus_ver3_update_res_vfe_out(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_UPDATE_VFE_OUT_RES_IRQ_MASK:
		rc = cam_vfe_bus_ver3_enable_irq_vfe_out(priv, cmd_args);
		break;
	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid camif process command:%d",
			cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_bus_ver3_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus)
{
	int i, rc = 0;
	struct cam_vfe_bus_ver3_priv    *bus_priv = NULL;
	struct cam_vfe_bus              *vfe_bus_local;
	struct cam_vfe_bus_ver3_hw_info *ver3_hw_info = bus_hw_info;
	struct cam_vfe_soc_private      *soc_private = NULL;
	int                              is_lite = 0, index = -1;

	CAM_DBG(CAM_ISP, "Enter");

	if (!hw_intf || !bus_hw_info) {
		CAM_ERR(CAM_ISP,
			"Inval_prms soc_info:%pK hw_intf:%pK hw_info%pK",
			soc_info, hw_intf, bus_hw_info);
		rc = -EINVAL;
		goto end;
	}

	if (soc_info) {
		soc_private = soc_info->soc_private;
		is_lite = soc_private->is_ife_lite;
		index = soc_info->index;
	}

	vfe_bus_local = kzalloc(sizeof(struct cam_vfe_bus), GFP_KERNEL);
	if (!vfe_bus_local) {
		CAM_DBG(CAM_ISP, "Failed to alloc for vfe_bus");
		rc = -ENOMEM;
		goto end;
	}

	bus_priv = kzalloc(sizeof(struct cam_vfe_bus_ver3_priv),
		GFP_KERNEL);
	if (!bus_priv) {
		CAM_DBG(CAM_ISP, "Failed to alloc for vfe_bus_priv");
		rc = -ENOMEM;
		goto free_bus_local;
	}
	vfe_bus_local->bus_priv = bus_priv;

	bus_priv->num_client                     = ver3_hw_info->num_client;
	bus_priv->num_out                        = ver3_hw_info->num_out;
	bus_priv->num_comp_grp                   = ver3_hw_info->num_comp_grp;
	bus_priv->top_irq_shift                  = ver3_hw_info->top_irq_shift;
	bus_priv->max_out_res                    = ver3_hw_info->max_out_res;
	bus_priv->common_data.num_sec_out        = 0;
	bus_priv->common_data.secure_mode        = CAM_SECURE_MODE_NON_SECURE;
	bus_priv->common_data.core_index         = hw_intf->hw_idx;
	if (soc_info)
		bus_priv->common_data.mem_base   =
			CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX);
	else
		bus_priv->common_data.mem_base   = 0;
	bus_priv->common_data.hw_intf            = hw_intf;
	bus_priv->common_data.vfe_irq_controller = vfe_irq_controller;
	bus_priv->common_data.common_reg         = &ver3_hw_info->common_reg;
	bus_priv->common_data.comp_done_shift    =
		ver3_hw_info->comp_done_shift;
	bus_priv->common_data.hw_init            = false;

	bus_priv->common_data.is_lite = is_lite;
	bus_priv->common_data.support_consumed_addr =
		ver3_hw_info->support_consumed_addr;
	bus_priv->common_data.out_fifo_depth =
		ver3_hw_info->fifo_depth;
	bus_priv->common_data.support_tunneling =
		ver3_hw_info->support_tunneling;
	bus_priv->common_data.no_tunnelingId_shift =
		ver3_hw_info->no_tunnelingId_shift;
	bus_priv->common_data.tunneling_overflow_shift =
		ver3_hw_info->tunneling_overflow_shift;
	bus_priv->common_data.disable_ubwc_comp = false;
	bus_priv->common_data.supported_irq      = ver3_hw_info->supported_irq;
	bus_priv->common_data.comp_config_needed =
		ver3_hw_info->comp_cfg_needed;
	bus_priv->common_data.init_irq_subscribed = false;
	bus_priv->common_data.disable_mmu_prefetch = false;
	bus_priv->common_data.pack_align_shift =
		ver3_hw_info->pack_align_shift;
	bus_priv->common_data.max_bw_counter_limit =
		ver3_hw_info->max_bw_counter_limit;
	bus_priv->num_cons_err = ver3_hw_info->num_cons_err;
	bus_priv->constraint_error_list = ver3_hw_info->constraint_error_list;

	if (bus_priv->num_out >= CAM_VFE_BUS_VER3_VFE_OUT_MAX) {
		CAM_ERR(CAM_ISP, "number of vfe out:%d more than max value:%d ",
			bus_priv->num_out, CAM_VFE_BUS_VER3_VFE_OUT_MAX);
		rc = -EINVAL;
		goto free_bus_priv;
	}

	bus_priv->comp_grp = kzalloc((sizeof(struct cam_isp_resource_node) *
		bus_priv->num_comp_grp), GFP_KERNEL);
	if (!bus_priv->comp_grp) {
		CAM_ERR(CAM_ISP, "Failed to alloc for bus comp groups");
		rc = -ENOMEM;
		goto free_bus_priv;
	}

	bus_priv->vfe_out = kzalloc((sizeof(struct cam_isp_resource_node) *
		bus_priv->num_out), GFP_KERNEL);
	if (!bus_priv->vfe_out) {
		CAM_ERR(CAM_ISP, "Failed to alloc for bus out res");
		rc = -ENOMEM;
		goto free_comp_grp;
	}

	for (i = 0; i < CAM_VFE_BUS_VER3_ERR_IRQ_REG_MAX; i++)
		bus_priv->bus_error_irq_mask[i] = ver3_hw_info->bus_error_irq_mask[i];

	for (i = 0; i < CAM_VFE_BUS_VER3_SRC_GRP_MAX; i++)
		bus_priv->common_data.rup_irq_handle[i] = 0;

	mutex_init(&bus_priv->common_data.bus_mutex);

	if (vfe_irq_controller) {
		rc = cam_irq_controller_init(drv_name, bus_priv->common_data.mem_base,
			&ver3_hw_info->common_reg.irq_reg_info,
			&bus_priv->common_data.bus_irq_controller);
		if (rc) {
			CAM_ERR(CAM_ISP, "Init bus_irq_controller failed");
			goto free_vfe_out;
		}
	}

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	for (i = 0; i < bus_priv->num_comp_grp; i++) {
		rc = cam_vfe_bus_ver3_init_comp_grp(i, soc_info,
			bus_priv, bus_hw_info,
			&bus_priv->comp_grp[i]);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "VFE:%d init comp_grp:%d failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
			goto deinit_comp_grp;
		}
	}

	for (i = 0; i < CAM_VFE_BUS_VER3_VFE_OUT_MAX; i++)
		bus_priv->vfe_out_map_outtype[i] =
			CAM_VFE_BUS_VER3_VFE_OUT_MAX;

	for (i = 0; i < bus_priv->num_out; i++) {
		rc = cam_vfe_bus_ver3_init_vfe_out_resource(i, bus_priv,
			bus_hw_info);
		if (rc < 0) {
			CAM_ERR(CAM_ISP,
				"VFE:%d init out_type:0x%X failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
			goto deinit_vfe_out;
		}
	}

	spin_lock_init(&bus_priv->common_data.spin_lock);
	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_VFE_BUS_VER3_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);
		list_add_tail(&bus_priv->common_data.evt_payload[i].list,
			&bus_priv->common_data.free_payload_list);
	}

	vfe_bus_local->hw_ops.reserve      = cam_vfe_bus_ver3_acquire_vfe_out;
	vfe_bus_local->hw_ops.release      = cam_vfe_bus_ver3_release_vfe_out;
	vfe_bus_local->hw_ops.start        = cam_vfe_bus_ver3_start_hw;
	vfe_bus_local->hw_ops.stop         = cam_vfe_bus_ver3_stop_hw;
	vfe_bus_local->hw_ops.init         = cam_vfe_bus_ver3_init_hw;
	vfe_bus_local->hw_ops.deinit       = cam_vfe_bus_ver3_deinit_hw;
	vfe_bus_local->top_half_handler    = NULL;
	vfe_bus_local->bottom_half_handler = NULL;
	vfe_bus_local->hw_ops.process_cmd  = __cam_vfe_bus_ver3_process_cmd;

	*vfe_bus = vfe_bus_local;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;

deinit_vfe_out:
	if (i < 0)
		i = bus_priv->num_out;
	for (--i; i >= 0; i--)
		cam_vfe_bus_ver3_deinit_vfe_out_resource(&bus_priv->vfe_out[i]);

deinit_comp_grp:
	if (i < 0)
		i = bus_priv->num_comp_grp;
	for (--i; i >= 0; i--)
		cam_vfe_bus_ver3_deinit_comp_grp(&bus_priv->comp_grp[i]);

free_vfe_out:
	kfree(bus_priv->vfe_out);

free_comp_grp:
	kfree(bus_priv->comp_grp);

free_bus_priv:
	kfree(vfe_bus_local->bus_priv);

free_bus_local:
	kfree(vfe_bus_local);

end:
	return rc;
}

int cam_vfe_bus_ver3_deinit(
	struct cam_vfe_bus                  **vfe_bus)
{
	int i, rc = 0;
	struct cam_vfe_bus_ver3_priv    *bus_priv = NULL;
	struct cam_vfe_bus              *vfe_bus_local;
	unsigned long                    flags;

	if (!vfe_bus || !*vfe_bus) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}
	vfe_bus_local = *vfe_bus;

	bus_priv = vfe_bus_local->bus_priv;
	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "bus_priv is NULL");
		rc = -ENODEV;
		goto free_bus_local;
	}

	spin_lock_irqsave(&bus_priv->common_data.spin_lock, flags);
	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_VFE_BUS_VER3_PAYLOAD_MAX; i++)
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);
	bus_priv->common_data.hw_init = false;
	bus_priv->common_data.init_irq_subscribed = false;
	spin_unlock_irqrestore(&bus_priv->common_data.spin_lock, flags);

	for (i = 0; i < bus_priv->num_comp_grp; i++) {
		rc = cam_vfe_bus_ver3_deinit_comp_grp(&bus_priv->comp_grp[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"VFE:%d deinit comp_grp:%d failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
	}

	for (i = 0; i < bus_priv->num_out; i++) {
		rc = cam_vfe_bus_ver3_deinit_vfe_out_resource(
			&bus_priv->vfe_out[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"VFE:%d deinit out_type:0x%X failed rc:%d",
				bus_priv->common_data.core_index, i, rc);
	}

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	rc = cam_irq_controller_deinit(
		&bus_priv->common_data.bus_irq_controller);
	if (rc)
		CAM_ERR(CAM_ISP,
			"Deinit BUS IRQ Controller failed rc=%d", rc);

	kfree(bus_priv->comp_grp);
	kfree(bus_priv->vfe_out);

	mutex_destroy(&bus_priv->common_data.bus_mutex);
	kfree(vfe_bus_local->bus_priv);

free_bus_local:
	kfree(vfe_bus_local);

	*vfe_bus = NULL;

	return rc;
}
