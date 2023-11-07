/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __PLD_PCIE_H__
#define __PLD_PCIE_H__

#ifdef CONFIG_PLD_PCIE_CNSS
#ifdef CONFIG_CNSS_OUT_OF_TREE
#include "cnss2.h"
#else
#include <net/cnss2.h>
#endif
#endif
#include <linux/pci.h>
#include "pld_internal.h"

#ifdef DYNAMIC_SINGLE_CHIP
#define PREFIX DYNAMIC_SINGLE_CHIP "/"
#else

#ifdef MULTI_IF_NAME
#define PREFIX MULTI_IF_NAME "/"
#else
#define PREFIX ""
#endif

#endif

#if !defined(HIF_PCI) || defined(CONFIG_PLD_PCIE_FW_SIM)
static inline int pld_pcie_register_driver(void)
{
	return 0;
}

static inline void pld_pcie_unregister_driver(void)
{
}

static inline int pld_pcie_get_ce_id(struct device *dev, int irq)
{
	return 0;
}
#else
/**
 * pld_pcie_register_driver() - Register PCIE device callback functions
 *
 * Return: int
 */
int pld_pcie_register_driver(void);

/**
 * pld_pcie_unregister_driver() - Unregister PCIE device callback functions
 *
 * Return: void
 */
void pld_pcie_unregister_driver(void);

/**
 * pld_pcie_get_ce_id() - Get CE number for the provided IRQ
 * @dev: device
 * @irq: IRQ number
 *
 * Return: CE number
 */
int pld_pcie_get_ce_id(struct device *dev, int irq);
#endif

#ifndef CONFIG_PLD_PCIE_CNSS
static inline int pld_pcie_wlan_enable(struct device *dev,
				       struct pld_wlan_enable_cfg *config,
				       enum pld_driver_mode mode,
				       const char *host_version)
{
	return 0;
}

static inline int pld_pcie_wlan_disable(struct device *dev,
					enum pld_driver_mode mode)
{
	return 0;
}

static inline int pld_pcie_wlan_hw_enable(void)
{
	return 0;
}

#else

/**
 * pld_pcie_wlan_enable() - Enable WLAN
 * @dev: device
 * @config: WLAN configuration data
 * @mode: WLAN mode
 * @host_version: host software version
 *
 * This function enables WLAN FW. It passed WLAN configuration data,
 * WLAN mode and host software version to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			 enum pld_driver_mode mode, const char *host_version);

/**
 * pld_pcie_wlan_disable() - Disable WLAN
 * @dev: device
 * @mode: WLAN mode
 *
 * This function disables WLAN FW. It passes WLAN mode to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_wlan_disable(struct device *dev, enum pld_driver_mode mode);

#ifdef FEATURE_CNSS_HW_SECURE_DISABLE
static inline int pld_pcie_wlan_hw_enable(void)
{
	return cnss_wlan_hw_enable();
}
#else
static inline int pld_pcie_wlan_hw_enable(void)
{
	return -EINVAL;
}
#endif
#endif

#if defined(CONFIG_PLD_PCIE_CNSS)
static inline int pld_pcie_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	return cnss_set_fw_log_mode(dev, fw_log_mode);
}

static inline void pld_pcie_intr_notify_q6(struct device *dev)
{
}
#else
static inline int pld_pcie_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	return 0;
}

static inline void pld_pcie_intr_notify_q6(struct device *dev)
{
}
#endif

#if (!defined(CONFIG_PLD_PCIE_CNSS)) || (!defined(CONFIG_CNSS_SECURE_FW))
static inline int pld_pcie_get_sha_hash(struct device *dev, const u8 *data,
					u32 data_len, u8 *hash_idx, u8 *out)
{
	return 0;
}

static inline void *pld_pcie_get_fw_ptr(struct device *dev)
{
	return NULL;
}
#else
static inline int pld_pcie_get_sha_hash(struct device *dev, const u8 *data,
					u32 data_len, u8 *hash_idx, u8 *out)
{
	return cnss_get_sha_hash(data, data_len, hash_idx, out);
}

static inline void *pld_pcie_get_fw_ptr(struct device *dev)
{
	return cnss_get_fw_ptr();
}
#endif

#ifdef CONFIG_PLD_PCIE_CNSS
static inline int pld_pcie_wlan_pm_control(struct device *dev, bool vote)
{
	return cnss_wlan_pm_control(dev, vote);
}
#else
static inline int pld_pcie_wlan_pm_control(struct device *dev, bool vote)
{
	return 0;
}
#endif

#ifndef CONFIG_PLD_PCIE_CNSS
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static inline void *pld_pcie_smmu_get_domain(struct device *dev)
{
	return NULL;
}
#else
static inline void *pld_pcie_smmu_get_mapping(struct device *dev)
{
	return NULL;
}
#endif

static inline int
pld_pcie_smmu_map(struct device *dev,
		  phys_addr_t paddr, uint32_t *iova_addr, size_t size)
{
	return 0;
}

static inline int
pld_pcie_smmu_unmap(struct device *dev, uint32_t iova_addr, size_t size)
{
	return 0;
}

static inline int
pld_pcie_get_fw_files_for_target(struct device *dev,
				 struct pld_fw_files *pfw_files,
				 u32 target_type, u32 target_version)
{
	pld_get_default_fw_files(pfw_files);
	return 0;
}

static inline int pld_pcie_prevent_l1(struct device *dev)
{
	return 0;
}

static inline void pld_pcie_allow_l1(struct device *dev)
{
}

static inline int pld_pcie_set_gen_speed(struct device *dev, u8 pcie_gen_speed)
{
	return 0;
}


static inline void pld_pcie_link_down(struct device *dev)
{
}

static inline int pld_pcie_get_reg_dump(struct device *dev, uint8_t *buf,
					uint32_t len)
{
	return 0;
}

static inline int pld_pcie_is_fw_down(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_athdiag_read(struct device *dev, uint32_t offset,
					uint32_t memtype, uint32_t datalen,
					uint8_t *output)
{
	return 0;
}

static inline int pld_pcie_athdiag_write(struct device *dev, uint32_t offset,
					 uint32_t memtype, uint32_t datalen,
					 uint8_t *input)
{
	return 0;
}

static inline void
pld_pcie_schedule_recovery_work(struct device *dev,
				enum pld_recovery_reason reason)
{
}

static inline void *pld_pcie_get_virt_ramdump_mem(struct device *dev,
						  unsigned long *size)
{
	size_t length = 0;
	int flags = GFP_KERNEL;

	length = TOTAL_DUMP_SIZE;

	if (!size)
		return NULL;

	*size = (unsigned long)length;

	if (in_interrupt() || irqs_disabled() || in_atomic())
		flags = GFP_ATOMIC;

	return kzalloc(length, flags);
}

static inline void pld_pcie_release_virt_ramdump_mem(void *address)
{
	kfree(address);
}

static inline void pld_pcie_device_crashed(struct device *dev)
{
}

static inline void pld_pcie_device_self_recovery(struct device *dev,
					 enum pld_recovery_reason reason)
{
}

static inline void pld_pcie_request_pm_qos(struct device *dev, u32 qos_val)
{
}

static inline void pld_pcie_remove_pm_qos(struct device *dev)
{
}

static inline void pld_pcie_set_tsf_sync_period(struct device *dev, u32 val)
{
}

static inline void pld_pcie_reset_tsf_sync_period(struct device *dev)
{
}

static inline int pld_pcie_request_bus_bandwidth(struct device *dev,
						 int bandwidth)
{
	return 0;
}

static inline int pld_pcie_get_platform_cap(struct device *dev,
					    struct pld_platform_cap *cap)
{
	return 0;
}

static inline int pld_pcie_get_soc_info(struct device *dev,
					struct pld_soc_info *info)
{
	return 0;
}

static inline int pld_pcie_auto_suspend(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_auto_resume(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_force_wake_request(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_force_wake_request_sync(struct device *dev,
						   int timeout_us)
{
	return 0;
}

static inline int pld_pcie_is_device_awake(struct device *dev)
{
	return true;
}

static inline int pld_pcie_force_wake_release(struct device *dev)
{
	return 0;
}

static inline void pld_pcie_lock_reg_window(struct device *dev,
					    unsigned long *flags)
{
}

static inline void pld_pcie_unlock_reg_window(struct device *dev,
					      unsigned long *flags)
{
}

static inline int pld_pcie_get_pci_slot(struct device *dev)
{
	return 0;
}

static inline struct kobject *pld_pcie_get_wifi_kobj(struct device *dev)
{
	return NULL;
}

static inline int pld_pcie_power_on(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_power_off(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_idle_restart(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_idle_shutdown(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_force_assert_target(struct device *dev)
{
	return -EINVAL;
}

static inline int pld_pcie_collect_rddm(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_qmi_send_get(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_qmi_send_put(struct device *dev)
{
	return 0;
}

static inline int
pld_pcie_qmi_send(struct device *dev, int type, void *cmd,
		  int cmd_len, void *cb_ctx,
		  int (*cb)(void *ctx, void *event, int event_len))
{
	return -EINVAL;
}

static inline int pld_pcie_get_user_msi_assignment(struct device *dev,
						   char *user_name,
						   int *num_vectors,
						   uint32_t *user_base_data,
						   uint32_t *base_vector)
{
	return -EINVAL;
}

static inline int pld_pcie_get_msi_irq(struct device *dev, unsigned int vector)
{
	return 0;
}

static inline bool pld_pcie_is_one_msi(struct device *dev)
{
	return false;
}

static inline void pld_pcie_get_msi_address(struct device *dev,
					    uint32_t *msi_addr_low,
					    uint32_t *msi_addr_high)
{
	return;
}

static inline int pld_pcie_is_drv_connected(struct device *dev)
{
	return 0;
}

static inline bool pld_pcie_platform_driver_support(void)
{
	return false;
}

static inline bool pld_pcie_is_direct_link_supported(struct device *dev)
{
	return false;
}

static inline
int pld_pcie_audio_smmu_map(struct device *dev, phys_addr_t paddr,
			    dma_addr_t iova, size_t size)
{
	return 0;
}

static inline
void pld_pcie_audio_smmu_unmap(struct device *dev, dma_addr_t iova, size_t size)
{
}

static inline int pld_pcie_set_wfc_mode(struct device *dev,
					enum pld_wfc_mode wfc_mode)
{
	return 0;
}

static inline int pld_pci_thermal_register(struct device *dev,
					   unsigned long max_state,
					   int mon_id)
{
	return 0;
}

static inline void pld_pci_thermal_unregister(struct device *dev,
					      int mon_id)
{
}

static inline int pld_pci_get_thermal_state(struct device *dev,
					    unsigned long *thermal_state,
					    int mon_id)
{
	return 0;
}

#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int pld_pcie_set_wfc_mode(struct device *dev,
			  enum pld_wfc_mode wfc_mode);
#else
static inline int pld_pcie_set_wfc_mode(struct device *dev,
					enum pld_wfc_mode wfc_mode)
{
	return 0;
}
#endif

/**
 * pld_pcie_get_fw_files_for_target() - Get FW file names
 * @dev: device
 * @pfw_files: buffer for FW file names
 * @target_type: target type
 * @target_version: target version
 *
 * Return target specific FW file names to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_get_fw_files_for_target(struct device *dev,
				     struct pld_fw_files *pfw_files,
				     u32 target_type, u32 target_version);

/**
 * pld_pcie_get_platform_cap() - Get platform capabilities
 * @dev: device
 * @cap: buffer to the capabilities
 *
 * Return capabilities to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_get_platform_cap(struct device *dev, struct pld_platform_cap *cap);

/**
 * pld_pcie_get_soc_info() - Get SOC information
 * @dev: device
 * @info: buffer to SOC information
 *
 * Return SOC info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_get_soc_info(struct device *dev, struct pld_soc_info *info);

/**
 * pld_pcie_schedule_recovery_work() - schedule recovery work
 * @dev: device
 * @reason: recovery reason
 *
 * Return: void
 */
void pld_pcie_schedule_recovery_work(struct device *dev,
				     enum pld_recovery_reason reason);

/**
 * pld_pcie_device_self_recovery() - device self recovery
 * @dev: device
 * @reason: recovery reason
 *
 * Return: void
 */
void pld_pcie_device_self_recovery(struct device *dev,
				   enum pld_recovery_reason reason);
static inline int pld_pcie_collect_rddm(struct device *dev)
{
	return cnss_force_collect_rddm(dev);
}

static inline int pld_pcie_qmi_send_get(struct device *dev)
{
	return cnss_qmi_send_get(dev);
}

static inline int pld_pcie_qmi_send_put(struct device *dev)
{
	return cnss_qmi_send_put(dev);
}

static inline int
pld_pcie_qmi_send(struct device *dev, int type, void *cmd,
		  int cmd_len, void *cb_ctx,
		  int (*cb)(void *ctx, void *event, int event_len))
{
	return cnss_qmi_send(dev, type, cmd, cmd_len, cb_ctx, cb);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static inline void *pld_pcie_smmu_get_domain(struct device *dev)
{
	return cnss_smmu_get_domain(dev);
}
#else
static inline void *pld_pcie_smmu_get_mapping(struct device *dev)
{
	return cnss_smmu_get_mapping(dev);
}
#endif

static inline int
pld_pcie_smmu_map(struct device *dev,
		  phys_addr_t paddr, uint32_t *iova_addr, size_t size)
{
	return cnss_smmu_map(dev, paddr, iova_addr, size);
}

#ifdef CONFIG_SMMU_S1_UNMAP
static inline int
pld_pcie_smmu_unmap(struct device *dev, uint32_t iova_addr, size_t size)
{
	return cnss_smmu_unmap(dev, iova_addr, size);
}
#else /* !CONFIG_SMMU_S1_UNMAP */
static inline int
pld_pcie_smmu_unmap(struct device *dev, uint32_t iova_addr, size_t size)
{
	return 0;
}
#endif /* CONFIG_SMMU_S1_UNMAP */

static inline int pld_pcie_prevent_l1(struct device *dev)
{
	return cnss_pci_prevent_l1(dev);
}

static inline void pld_pcie_allow_l1(struct device *dev)
{
	cnss_pci_allow_l1(dev);
}

#ifdef PCIE_GEN_SWITCH
/**
 * pld_pcie_set_gen_speed() - Wrapper for platform API to set PCIE gen speed
 * @dev: device
 * @pcie_gen_speed: PCIE gen speed required
 *
 * Send required PCIE Gen speed to platform driver
 *
 * Return: 0 for success. Negative error codes.
 */
static inline int pld_pcie_set_gen_speed(struct device *dev, u8 pcie_gen_speed)
{
	return cnss_set_pcie_gen_speed(dev, pcie_gen_speed);
}
#else
static inline int pld_pcie_set_gen_speed(struct device *dev, u8 pcie_gen_speed)
{
	return 0;
}
#endif

static inline void pld_pcie_link_down(struct device *dev)
{
	cnss_pci_link_down(dev);
}

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)) && \
		(LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)))
static inline int pld_pcie_get_reg_dump(struct device *dev, uint8_t *buf,
					uint32_t len)
{
	return cnss_pci_get_reg_dump(dev, buf, len);
}
#else
static inline int pld_pcie_get_reg_dump(struct device *dev, uint8_t *buf,
					uint32_t len)
{
	return 0;
}
#endif

static inline int pld_pcie_is_fw_down(struct device *dev)
{
	return cnss_pci_is_device_down(dev);
}

static inline int pld_pcie_athdiag_read(struct device *dev, uint32_t offset,
					uint32_t memtype, uint32_t datalen,
					uint8_t *output)
{
	return cnss_athdiag_read(dev, offset, memtype, datalen, output);
}

static inline int pld_pcie_athdiag_write(struct device *dev, uint32_t offset,
					 uint32_t memtype, uint32_t datalen,
					 uint8_t *input)
{
	return cnss_athdiag_write(dev, offset, memtype, datalen, input);
}

static inline void *pld_pcie_get_virt_ramdump_mem(struct device *dev,
						  unsigned long *size)
{
	return cnss_get_virt_ramdump_mem(dev, size);
}

static inline void pld_pcie_release_virt_ramdump_mem(void *address)
{
}

static inline void pld_pcie_device_crashed(struct device *dev)
{
	cnss_device_crashed(dev);
}

static inline void pld_pcie_request_pm_qos(struct device *dev, u32 qos_val)
{
	cnss_request_pm_qos(dev, qos_val);
}

static inline void pld_pcie_remove_pm_qos(struct device *dev)
{
	cnss_remove_pm_qos(dev);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static inline void pld_pcie_set_tsf_sync_period(struct device *dev, u32 val)
{
	cnss_update_time_sync_period(dev, val);
}

static inline void pld_pcie_reset_tsf_sync_period(struct device *dev)
{
	cnss_reset_time_sync_period(dev);
}
#else
static inline void pld_pcie_set_tsf_sync_period(struct device *dev, u32 val)
{
}

static inline void pld_pcie_reset_tsf_sync_period(struct device *dev)
{
}
#endif
static inline int pld_pcie_request_bus_bandwidth(struct device *dev,
						 int bandwidth)
{
	return cnss_request_bus_bandwidth(dev, bandwidth);
}

static inline int pld_pcie_auto_suspend(struct device *dev)
{
	return cnss_auto_suspend(dev);
}

static inline int pld_pcie_auto_resume(struct device *dev)
{
	return cnss_auto_resume(dev);
}

static inline int pld_pcie_force_wake_request(struct device *dev)
{
	return cnss_pci_force_wake_request(dev);
}

static inline int pld_pcie_force_wake_request_sync(struct device *dev,
						   int timeout_us)
{
	return cnss_pci_force_wake_request_sync(dev, timeout_us);
}

static inline int pld_pcie_is_device_awake(struct device *dev)
{
	return cnss_pci_is_device_awake(dev);
}

static inline int pld_pcie_force_wake_release(struct device *dev)
{
	return cnss_pci_force_wake_release(dev);
}

static inline void pld_pcie_lock_reg_window(struct device *dev,
					    unsigned long *flags)
{
	cnss_pci_lock_reg_window(dev, flags);
}

static inline void pld_pcie_unlock_reg_window(struct device *dev,
					      unsigned long *flags)
{
	cnss_pci_unlock_reg_window(dev, flags);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static inline int pld_pcie_get_pci_slot(struct device *dev)
{
	return cnss_get_pci_slot(dev);
}
#else
static inline int pld_pcie_get_pci_slot(struct device *dev)
{
	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static inline struct kobject *pld_pcie_get_wifi_kobj(struct device *dev)
{
	return cnss_get_wifi_kobj(dev);
}
#else
static inline struct kobject *pld_pcie_get_wifi_kobj(struct device *dev)
{
	return NULL;
}
#endif

static inline int pld_pcie_power_on(struct device *dev)
{
	return cnss_power_up(dev);
}

static inline int pld_pcie_power_off(struct device *dev)
{
	return cnss_power_down(dev);
}

static inline int pld_pcie_idle_restart(struct device *dev)
{
	return cnss_idle_restart(dev);
}

static inline int pld_pcie_idle_shutdown(struct device *dev)
{
	return cnss_idle_shutdown(dev);
}

static inline int pld_pcie_force_assert_target(struct device *dev)
{
	return cnss_force_fw_assert(dev);
}

static inline int pld_pcie_get_user_msi_assignment(struct device *dev,
						   char *user_name,
						   int *num_vectors,
						   uint32_t *user_base_data,
						   uint32_t *base_vector)
{
	return cnss_get_user_msi_assignment(dev, user_name, num_vectors,
					    user_base_data, base_vector);
}

static inline int pld_pcie_get_msi_irq(struct device *dev, unsigned int vector)
{
	return cnss_get_msi_irq(dev, vector);
}

#ifdef WLAN_ONE_MSI_VECTOR
static inline bool pld_pcie_is_one_msi(struct device *dev)
{
	return cnss_is_one_msi(dev);
}
#else
static inline bool pld_pcie_is_one_msi(struct device *dev)
{
	return false;
}
#endif

static inline void pld_pcie_get_msi_address(struct device *dev,
					    uint32_t *msi_addr_low,
					    uint32_t *msi_addr_high)
{
	cnss_get_msi_address(dev, msi_addr_low, msi_addr_high);
}

static inline int pld_pcie_is_drv_connected(struct device *dev)
{
	return cnss_pci_is_drv_connected(dev);
}

static inline bool pld_pcie_platform_driver_support(void)
{
	return true;
}

static inline int pld_pci_thermal_register(struct device *dev,
					   unsigned long max_state,
					   int mon_id)
{
	return cnss_thermal_cdev_register(dev, max_state, mon_id);
}

static inline void pld_pci_thermal_unregister(struct device *dev,
					      int mon_id)
{
	cnss_thermal_cdev_unregister(dev, mon_id);
}

static inline int pld_pci_get_thermal_state(struct device *dev,
					    unsigned long *thermal_state,
					    int mon_id)
{
	return cnss_get_curr_therm_cdev_state(dev, thermal_state, mon_id);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
static inline bool pld_pcie_is_direct_link_supported(struct device *dev)
{
	return cnss_get_fw_cap(dev, CNSS_FW_CAP_DIRECT_LINK_SUPPORT);
}

static inline
int pld_pcie_audio_smmu_map(struct device *dev, phys_addr_t paddr,
			    dma_addr_t iova, size_t size)
{
	return cnss_audio_smmu_map(dev, paddr, iova, size);
}

static inline
void pld_pcie_audio_smmu_unmap(struct device *dev, dma_addr_t iova, size_t size)
{
	cnss_audio_smmu_unmap(dev, iova, size);
}
#else
static inline bool pld_pcie_is_direct_link_supported(struct device *dev)
{
	return false;
}

static inline
int pld_pcie_audio_smmu_map(struct device *dev, phys_addr_t paddr,
			    dma_addr_t iova, size_t size)
{
	return 0;
}

static inline
void pld_pcie_audio_smmu_unmap(struct device *dev, dma_addr_t iova, size_t size)
{
}
#endif
#endif
#endif
