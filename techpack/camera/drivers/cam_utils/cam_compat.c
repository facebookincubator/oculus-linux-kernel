// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#include "cam_compat.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"
#include "camera_main.h"
#include <media/cam_isp.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
int cam_reserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length)
{
	int rc = 0;
	struct device_node *of_node;
	struct device_node *mem_node;
	struct resource     res;

	of_node = (icp_fw->fw_dev)->of_node;
	mem_node = of_parse_phandle(of_node, "memory-region", 0);
	if (!mem_node) {
		rc = -ENOMEM;
		CAM_ERR(CAM_SMMU, "FW memory carveout not found");
		goto end;
	}
	rc = of_address_to_resource(mem_node, 0, &res);
	of_node_put(mem_node);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Unable to get start of FW mem carveout");
		goto end;
	}
	icp_fw->fw_hdl = res.start;
	icp_fw->fw_kva = ioremap_wc(icp_fw->fw_hdl, fw_length);
	if (!icp_fw->fw_kva) {
		CAM_ERR(CAM_SMMU, "Failed to map the FW.");
		rc = -ENOMEM;
		goto end;
	}
	memset_io(icp_fw->fw_kva, 0, fw_length);

end:
	return rc;
}

void cam_unreserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length)
{
	iounmap(icp_fw->fw_kva);
}

int cam_ife_notify_safe_lut_scm(bool safe_trigger)
{
	const uint32_t smmu_se_ife = 0;
	uint32_t camera_hw_version, rc = 0;

	rc = cam_cpas_get_cpas_hw_version(&camera_hw_version);
	if (!rc && qcom_scm_smmu_notify_secure_lut(smmu_se_ife, safe_trigger)) {
		switch (camera_hw_version) {
		case CAM_CPAS_TITAN_170_V100:
		case CAM_CPAS_TITAN_170_V110:
		case CAM_CPAS_TITAN_175_V100:
			CAM_ERR(CAM_ISP, "scm call to enable safe failed");
			rc = -EINVAL;
			break;
		default:
			break;
		}
	}

	return rc;
}

int cam_csiphy_notify_secure_mode(struct csiphy_device *csiphy_dev,
	bool protect, int32_t offset, bool is_shutdown)
{
	int rc = 0;

#ifdef CONFIG_SECURE_CAMERA_V3
	if (!is_shutdown) {
		struct smci_object client_env, sc_object;
		struct tc_driver_sensor_info params = {0};

		if (offset >= CSIPHY_MAX_INSTANCES_PER_PHY) {
			CAM_ERR(CAM_CSIPHY, "Invalid CSIPHY offset");
			return -EINVAL;
		}

		rc = get_client_env_object(&client_env);
		if (rc) {
			CAM_ERR(CAM_CSIPHY, "Failed getting mink env object, rc: %d", rc);
			return rc;
		}

		rc = smci_clientenv_open(client_env, CTRUSTEDCAMERADRIVER_UID, &sc_object);
		if (rc) {
			CAM_ERR(CAM_CSIPHY, "Failed getting mink sc_object, rc: %d", rc);
			return rc;
		}

		params.phy_lane_sel_mask = csiphy_dev->csiphy_info[offset].csiphy_cpas_cp_reg_mask;
		params.protect = protect ? 1 : 0;

		CAM_DBG(CAM_UTIL, "phy_sel_m: %d protect: %d",
					params.phy_lane_sel_mask,
					params.protect);

		rc = trusted_camera_driver_dynamic_protect_sensor(sc_object, &params);
		if (rc) {
			CAM_ERR(CAM_CSIPHY, "Mink secure call failed, rc: %d", rc);
			return rc;
		}

		rc = smci_object_release(sc_object);
		if (rc) {
			CAM_ERR(CAM_CSIPHY, "Failed releasing secure camera object, rc: %d", rc);
			return rc;
		}
		rc = smci_object_release(client_env);
		if (rc) {
			CAM_ERR(CAM_CSIPHY, "Failed releasing mink env object, rc: %d", rc);
			return rc;
		}
	} else {
		if (offset >= csiphy_dev->session_max_device_support) {
			CAM_ERR(CAM_CSIPHY, "Invalid CSIPHY offset");
			rc = -EINVAL;
		} else if (qcom_scm_camera_protect_phy_lanes(protect,
				csiphy_dev->csiphy_info[offset]
					.csiphy_cpas_cp_reg_mask)) {
			CAM_ERR(CAM_CSIPHY, "SCM call to hypervisor failed");
			rc = -EINVAL;
		}
	}
#else
	if (offset >= csiphy_dev->session_max_device_support) {
		CAM_ERR(CAM_CSIPHY, "Invalid CSIPHY offset");
		rc = -EINVAL;
	} else if (qcom_scm_camera_protect_phy_lanes(protect,
			csiphy_dev->csiphy_info[offset]
				.csiphy_cpas_cp_reg_mask)) {
		CAM_ERR(CAM_CSIPHY, "SCM call to hypervisor failed");
		rc = -EINVAL;
	}
#endif

	return rc;
}
#ifdef CONFIG_SECURE_CAMERA_V3
int cam_isp_notify_secure_unsecure_port(struct port_info *sec_unsec_port_info)
{
	int rc = 0;
	struct smci_object client_env, sc_object;

	rc = get_client_env_object(&client_env);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed getting mink env object, rc: %d", rc);
		return rc;
	}

	rc = smci_clientenv_open(client_env, CTRUSTEDCAMERADRIVER_UID, &sc_object);
	if (rc) {
		CAM_ERR(CAM_ISP, "Failed getting mink sc_object, rc: %d", rc);
		goto release_client;
	}

	rc = trusted_camera_driver_dynamic_configure_ports(sc_object, sec_unsec_port_info, 2);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"trusted_camera_driver_dynamic_configure_ports failed, rc: %d", rc);
		goto release_sc_object;
	}

release_sc_object:
	if (smci_object_release(sc_object)) {
		if (!rc)
			rc = -EINVAL;
		CAM_ERR(CAM_ISP, "Failed releasing secure camera object, rc: %d", rc);
	}

release_client:
	if (smci_object_release(client_env)) {
		if (!rc)
			rc = -EINVAL;
		CAM_ERR(CAM_ISP, "Failed releasing mink env object, rc: %d", rc);
	}

	return rc;
}

int32_t cam_convert_hw_id_to_secure_hw_type(uint32_t hw_id)
{
	uint32_t hw_type = -1;

	switch (hw_id) {
	case CAM_ISP_IFE0_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE0;
		break;
	case CAM_ISP_IFE1_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE1;
		break;
	case CAM_ISP_IFE2_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE2;
		break;
	case CAM_ISP_IFE0_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_0;
		break;
	case CAM_ISP_IFE1_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_1;
		break;
	case CAM_ISP_IFE2_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_2;
		break;
	case CAM_ISP_IFE3_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_3;
		break;
	case CAM_ISP_IFE4_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_4;
		break;
	case CAM_ISP_IFE5_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_5;
		break;
	case CAM_ISP_IFE6_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_6;
		break;
	case CAM_ISP_IFE7_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_7;
		break;
	case CAM_ISP_IFE8_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_8;
		break;
	case CAM_ISP_IFE9_LITE_HW:
		hw_type = ITRUSTEDCAMERADRIVER_IFE_LITE_9;
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid hw_id 0x%x", hw_id);
		break;
	}
	return hw_type;
}
#endif

void cam_cpastop_scm_write(struct cam_cpas_hw_errata_wa *errata_wa)
{
	int reg_val;

	qcom_scm_io_readl(errata_wa->data.reg_info.offset, &reg_val);
	reg_val |= errata_wa->data.reg_info.value;
	qcom_scm_io_writel(errata_wa->data.reg_info.offset, reg_val);
}

static int camera_platform_compare_dev(struct device *dev, const void *data)
{
	return platform_bus_type.match(dev, (struct device_driver *) data);
}

static int camera_i2c_compare_dev(struct device *dev, const void *data)
{
	return i2c_bus_type.match(dev, (struct device_driver *) data);
}
#else
int cam_reserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length)
{
	int rc = 0;

	icp_fw->fw_kva = dma_alloc_coherent(icp_fw->fw_dev, fw_length,
		&icp_fw->fw_hdl, GFP_KERNEL);

	if (!icp_fw->fw_kva) {
		CAM_ERR(CAM_SMMU, "FW memory alloc failed");
		rc = -ENOMEM;
	}

	return rc;
}

void cam_unreserve_icp_fw(struct cam_fw_alloc_info *icp_fw, size_t fw_length)
{
	dma_free_coherent(icp_fw->fw_dev, fw_length, icp_fw->fw_kva,
		icp_fw->fw_hdl);
}

int cam_ife_notify_safe_lut_scm(bool safe_trigger)
{
	const uint32_t smmu_se_ife = 0;
	uint32_t camera_hw_version, rc = 0;
	struct scm_desc description = {
		.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL),
		.args[0] = smmu_se_ife,
		.args[1] = safe_trigger,
	};

	rc = cam_cpas_get_cpas_hw_version(&camera_hw_version);
	if (!rc && scm_call2(SCM_SIP_FNID(0x15, 0x3), &description)) {
		switch (camera_hw_version) {
		case CAM_CPAS_TITAN_170_V100:
		case CAM_CPAS_TITAN_170_V110:
		case CAM_CPAS_TITAN_175_V100:
			CAM_ERR(CAM_ISP, "scm call to enable safe failed");
			rc = -EINVAL;
			break;
		default:
			break;
		}
	}

	return rc;
}

int cam_csiphy_notify_secure_mode(struct csiphy_device *csiphy_dev,
	bool protect, int32_t offset)
{
	int rc = 0;
	struct scm_desc description = {
		.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL),
		.args[0] = protect,
		.args[1] = csiphy_dev->csiphy_info[offset]
			.csiphy_cpas_cp_reg_mask,
	};

	if (offset >= csiphy_dev->session_max_device_support) {
		CAM_ERR(CAM_CSIPHY, "Invalid CSIPHY offset");
		rc = -EINVAL;
	} else if (scm_call2(SCM_SIP_FNID(0x18, 0x7), &description)) {
		CAM_ERR(CAM_CSIPHY, "SCM call to hypervisor failed");
		rc = -EINVAL;
	}

	return rc;
}

void cam_cpastop_scm_write(struct cam_cpas_hw_errata_wa *errata_wa)
{
	int reg_val;

	reg_val = scm_io_read(errata_wa->data.reg_info.offset);
	reg_val |= errata_wa->data.reg_info.value;
	scm_io_write(errata_wa->data.reg_info.offset, reg_val);
}

static int camera_platform_compare_dev(struct device *dev, void *data)
{
	return platform_bus_type.match(dev, (struct device_driver *) data);
}

static int camera_i2c_compare_dev(struct device *dev, void *data)
{
	return i2c_bus_type.match(dev, (struct device_driver *) data);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
void cam_free_clear(const void * ptr)
{
	kfree_sensitive(ptr);
}
#else
void cam_free_clear(const void * ptr)
{
	kzfree(ptr);
}
#endif

/* Callback to compare device from match list before adding as component */
static inline int camera_component_compare_dev(struct device *dev, void *data)
{
	return dev == data;
}

/* Add component matches to list for master of aggregate driver */
int camera_component_match_add_drivers(struct device *master_dev,
	struct component_match **match_list)
{
	int i, rc = 0;
	struct platform_device *pdev = NULL;
	struct i2c_client *client = NULL;
	struct device *start_dev = NULL, *match_dev = NULL;

	if (!master_dev || !match_list) {
		CAM_ERR(CAM_UTIL, "Invalid parameters for component match add");
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < ARRAY_SIZE(cam_component_platform_drivers); i++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		struct device_driver const *drv =
			&cam_component_platform_drivers[i]->driver;
		const void *drv_ptr = (const void *)drv;
#else
		struct device_driver *drv = &cam_component_platform_drivers[i]->driver;
		void *drv_ptr = (void *)drv;
#endif
		start_dev = NULL;
		while ((match_dev = bus_find_device(&platform_bus_type,
			start_dev, drv_ptr, &camera_platform_compare_dev))) {
			put_device(start_dev);
			pdev = to_platform_device(match_dev);
			CAM_DBG(CAM_UTIL, "Adding matched component:%s", pdev->name);
			component_match_add(master_dev, match_list,
				camera_component_compare_dev, match_dev);
			start_dev = match_dev;
		}
		put_device(start_dev);
	}

	for (i = 0; i < ARRAY_SIZE(cam_component_i2c_drivers); i++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		struct device_driver const *drv =
			&cam_component_i2c_drivers[i]->driver;
		const void *drv_ptr = (const void *)drv;
#else
		struct device_driver *drv = &cam_component_i2c_drivers[i]->driver;
		void *drv_ptr = (void *)drv;
#endif
		start_dev = NULL;
		while ((match_dev = bus_find_device(&i2c_bus_type,
			start_dev, drv_ptr, &camera_i2c_compare_dev))) {
			put_device(start_dev);
			client = to_i2c_client(match_dev);
			CAM_DBG(CAM_UTIL, "Adding matched component:%s", client->name);
			component_match_add(master_dev, match_list,
				camera_component_compare_dev, match_dev);
			start_dev = match_dev;
		}
		put_device(start_dev);
	}

end:
	return rc;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include <linux/qcom-iommu-util.h>
void cam_check_iommu_faults(struct iommu_domain *domain,
	struct cam_smmu_pf_info *pf_info)
{
	struct qcom_iommu_fault_ids fault_ids = {0, 0, 0};

	if (qcom_iommu_get_fault_ids(domain, &fault_ids))
		CAM_ERR(CAM_SMMU, "Cannot get smmu fault ids");
	else
		CAM_ERR(CAM_SMMU, "smmu fault ids bid:%d pid:%d mid:%d",
			fault_ids.bid, fault_ids.pid, fault_ids.mid);

	pf_info->bid = fault_ids.bid;
	pf_info->pid = fault_ids.pid;
	pf_info->mid = fault_ids.mid;
}
#else
void cam_check_iommu_faults(struct iommu_domain *domain,
	struct cam_smmu_pf_info *pf_info)
{
	struct iommu_fault_ids fault_ids = {0, 0, 0};

	if (iommu_get_fault_ids(domain, &fault_ids))
		CAM_ERR(CAM_SMMU, "Error: Can not get smmu fault ids");

	CAM_ERR(CAM_SMMU, "smmu fault ids bid:%d pid:%d mid:%d",
		fault_ids.bid, fault_ids.pid, fault_ids.mid);

	pf_info->bid = fault_ids.bid;
	pf_info->pid = fault_ids.pid;
	pf_info->mid = fault_ids.mid;
}
#endif

static int inline cam_subdev_list_cmp(struct cam_subdev *entry_1, struct cam_subdev *entry_2)
{
	if (entry_1->close_seq_prior > entry_2->close_seq_prior)
		return 1;
	else if (entry_1->close_seq_prior < entry_2->close_seq_prior)
		return -1;
	else
		return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
void cam_smmu_util_iommu_custom(struct device *dev,
	dma_addr_t discard_start, size_t discard_length)
{
	return;
}

int cam_req_mgr_ordered_list_cmp(void *priv,
	const struct list_head *head_1, const struct list_head *head_2)
{
	return cam_subdev_list_cmp(list_entry(head_1, struct cam_subdev, list),
		list_entry(head_2, struct cam_subdev, list));
}

int cam_compat_util_get_dmabuf_va(struct dma_buf *dmabuf, uintptr_t *vaddr)
{
	struct dma_buf_map mapping;
	int error_code = dma_buf_vmap(dmabuf, &mapping);

	if (error_code)
		*vaddr = 0;
	else
		*vaddr = (mapping.is_iomem) ?
			(uintptr_t)mapping.vaddr_iomem : (uintptr_t)mapping.vaddr;

	return error_code;
}

void cam_compat_util_put_dmabuf_va(struct dma_buf *dmabuf, void *vaddr)
{
	struct dma_buf_map mapping = DMA_BUF_MAP_INIT_VADDR(vaddr);

	dma_buf_vunmap(dmabuf, &mapping);
}

int cam_get_ddr_type(void)
{
	/* We assume all chipsets running kernel version 5.15+
	 * to be using only DDR5 based memory.
	 */
	return DDR_TYPE_LPDDR5;
}
#else
void cam_smmu_util_iommu_custom(struct device *dev,
	dma_addr_t discard_start, size_t discard_length)
{
	iommu_dma_enable_best_fit_algo(dev);

	if (discard_start)
		iommu_dma_reserve_iova(dev, discard_start, discard_length);

	return;
}

int cam_req_mgr_ordered_list_cmp(void *priv,
	struct list_head *head_1, struct list_head *head_2)
{
	return cam_subdev_list_cmp(list_entry(head_1, struct cam_subdev, list),
		list_entry(head_2, struct cam_subdev, list));
}

int cam_compat_util_get_dmabuf_va(struct dma_buf *dmabuf, uintptr_t *vaddr)
{
	int error_code = 0;
	void *addr = dma_buf_vmap(dmabuf);

	if (!addr) {
		*vaddr = 0;
		error_code = -ENOSPC;
	} else {
		*vaddr = (uintptr_t)addr;
	}

	return error_code;
}

void cam_compat_util_put_dmabuf_va(struct dma_buf *dmabuf, void *vaddr)
{
	dma_buf_vunmap(dmabuf, vaddr);
}

int cam_get_ddr_type(void)
{
	return of_fdt_get_ddrtype();
}
#endif
