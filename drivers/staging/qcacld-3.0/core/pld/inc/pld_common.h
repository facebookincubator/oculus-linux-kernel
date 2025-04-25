/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __PLD_COMMON_H__
#define __PLD_COMMON_H__

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <osapi_linux.h>

#ifdef CONFIG_CNSS_OUT_OF_TREE
#include "cnss2.h"
#else
#include <net/cnss2.h>
#endif

#ifdef CNSS_UTILS
#ifdef CONFIG_CNSS_OUT_OF_TREE
#include "cnss_utils.h"
#else
#include <net/cnss_utils.h>
#endif
#endif

#define PLD_IMAGE_FILE               "athwlan.bin"
#define PLD_UTF_FIRMWARE_FILE        "utf.bin"
#define PLD_BOARD_DATA_FILE          "fakeboar.bin"
#define PLD_OTP_FILE                 "otp.bin"
#define PLD_SETUP_FILE               "athsetup.bin"
#define PLD_EPPING_FILE              "epping.bin"
#define PLD_EVICTED_FILE             ""
#define PLD_MHI_STATE_L0	1

#define TOTAL_DUMP_SIZE         0x00200000

#ifdef CNSS_MEM_PRE_ALLOC
#ifdef CONFIG_CNSS_OUT_OF_TREE
#include "cnss_prealloc.h"
#else
#include <net/cnss_prealloc.h>
#endif
#endif

#define PLD_LIMIT_LOG_FOR_SEC 6
/**
 * __PLD_TRACE_RATE_LIMITED() - rate limited version of PLD_TRACE
 * @params: parameters to pass through to PLD_TRACE
 *
 * This API prevents logging a message more than once in PLD_LIMIT_LOG_FOR_SEC
 * seconds. This means any subsequent calls to this API from the same location
 * within PLD_LIMIT_LOG_FOR_SEC seconds will be dropped.
 *
 * Return: None
 */
#define __PLD_TRACE_RATE_LIMITED(params...)\
	do {\
		static ulong __last_ticks;\
		ulong __ticks = jiffies;\
		if (time_after(__ticks,\
			       __last_ticks + (HZ * PLD_LIMIT_LOG_FOR_SEC))) {\
			pr_err(params);\
			__last_ticks = __ticks;\
		} \
	} while (0)

#define pld_err_rl(params...) __PLD_TRACE_RATE_LIMITED(params)

/**
 * enum pld_bus_type - bus type
 * @PLD_BUS_TYPE_NONE: invalid bus type, only return in error cases
 * @PLD_BUS_TYPE_PCIE: PCIE bus
 * @PLD_BUS_TYPE_SNOC: SNOC bus
 * @PLD_BUS_TYPE_SDIO: SDIO bus
 * @PLD_BUS_TYPE_USB : USB bus
 * @PLD_BUS_TYPE_SNOC_FW_SIM : SNOC FW SIM bus
 * @PLD_BUS_TYPE_PCIE_FW_SIM : PCIE FW SIM bus
 * @PLD_BUS_TYPE_IPCI : IPCI bus
 * @PLD_BUS_TYPE_IPCI_FW_SIM : IPCI FW SIM bus
 */
enum pld_bus_type {
	PLD_BUS_TYPE_NONE = -1,
	PLD_BUS_TYPE_PCIE = 0,
	PLD_BUS_TYPE_SNOC,
	PLD_BUS_TYPE_SDIO,
	PLD_BUS_TYPE_USB,
	PLD_BUS_TYPE_SNOC_FW_SIM,
	PLD_BUS_TYPE_PCIE_FW_SIM,
	PLD_BUS_TYPE_IPCI,
	PLD_BUS_TYPE_IPCI_FW_SIM,
};

#define PLD_MAX_FIRMWARE_SIZE (1 * 1024 * 1024)

/**
 * enum pld_bus_width_type - bus bandwidth
 * @PLD_BUS_WIDTH_NONE: don't vote for bus bandwidth
 * @PLD_BUS_WIDTH_IDLE: vote for idle bandwidth
 * @PLD_BUS_WIDTH_LOW: vote for low bus bandwidth
 * @PLD_BUS_WIDTH_MEDIUM: vote for medium bus bandwidth
 * @PLD_BUS_WIDTH_HIGH: vote for high bus bandwidth
 * @PLD_BUS_WIDTH_MID_HIGH: vote for mid high bus bandwidth
 * @PLD_BUS_WIDTH_VERY_HIGH: vote for very high bus bandwidth
 * @PLD_BUS_WIDTH_ULTRA_HIGH: vote for ultra high bus bandwidth
 * @PLD_BUS_WIDTH_LOW_LATENCY: vote for low latency bus bandwidth
 * @PLD_BUS_WIDTH_MAX:
 */
enum pld_bus_width_type {
	PLD_BUS_WIDTH_NONE,
	PLD_BUS_WIDTH_IDLE,
	PLD_BUS_WIDTH_LOW,
	PLD_BUS_WIDTH_MEDIUM,
	PLD_BUS_WIDTH_HIGH,
	PLD_BUS_WIDTH_VERY_HIGH,
	PLD_BUS_WIDTH_ULTRA_HIGH,
	PLD_BUS_WIDTH_MAX,
	PLD_BUS_WIDTH_LOW_LATENCY,
	PLD_BUS_WIDTH_MID_HIGH,
};

#define PLD_MAX_FILE_NAME NAME_MAX

/**
 * struct pld_fw_files - WLAN FW file names
 * @image_file: WLAN FW image file
 * @board_data: WLAN FW board data file
 * @otp_data: WLAN FW OTP file
 * @utf_file: WLAN FW UTF file
 * @utf_board_data: WLAN FW UTF board data file
 * @epping_file: WLAN FW EPPING mode file
 * @evicted_data: WLAN FW evicted file
 * @setup_file: WLAN FW setup file
 * @ibss_image_file: WLAN FW IBSS mode file
 *
 * pld_fw_files is used to store WLAN FW file names
 */
struct pld_fw_files {
	char image_file[PLD_MAX_FILE_NAME];
	char board_data[PLD_MAX_FILE_NAME];
	char otp_data[PLD_MAX_FILE_NAME];
	char utf_file[PLD_MAX_FILE_NAME];
	char utf_board_data[PLD_MAX_FILE_NAME];
	char epping_file[PLD_MAX_FILE_NAME];
	char evicted_data[PLD_MAX_FILE_NAME];
	char setup_file[PLD_MAX_FILE_NAME];
	char ibss_image_file[PLD_MAX_FILE_NAME];
};

/**
 * enum pld_platform_cap_flag - platform capability flag
 * @PLD_HAS_EXTERNAL_SWREG: has external regulator
 * @PLD_HAS_UART_ACCESS: has UART access
 * @PLD_HAS_DRV_SUPPORT: has PCIe DRV support
 */
enum pld_platform_cap_flag {
	PLD_HAS_EXTERNAL_SWREG = 0x01,
	PLD_HAS_UART_ACCESS = 0x02,
	PLD_HAS_DRV_SUPPORT = 0x04,
};

/**
 * enum pld_wfc_mode - WFC Mode
 * @PLD_WFC_MODE_OFF: WFC Inactive
 * @PLD_WFC_MODE_ON: WFC Active
 */
enum pld_wfc_mode {
	PLD_WFC_MODE_OFF,
	PLD_WFC_MODE_ON,
};

/**
 * struct pld_platform_cap - platform capabilities
 * @cap_flag: capabilities flag
 *
 * pld_platform_cap provides platform capabilities which are
 * extracted from DTS.
 */
struct pld_platform_cap {
	u32 cap_flag;
};

/**
 * enum pld_uevent - PLD uevent event types
 * @PLD_FW_DOWN: firmware is down
 * @PLD_FW_CRASHED: firmware has crashed
 * @PLD_FW_RECOVERY_START: firmware is starting recovery
 * @PLD_FW_HANG_EVENT: firmware update hang event
 * @PLD_BUS_EVENT: update bus/link event
 * @PLD_SMMU_FAULT: SMMU fault
 * @PLD_SYS_REBOOT: system is rebooting
 */
enum pld_uevent {
	PLD_FW_DOWN,
	PLD_FW_CRASHED,
	PLD_FW_RECOVERY_START,
	PLD_FW_HANG_EVENT,
	PLD_BUS_EVENT,
	PLD_SMMU_FAULT,
	PLD_SYS_REBOOT,
};

/**
 * enum pld_bus_event - PLD bus event types
 * @PLD_BUS_EVENT_PCIE_LINK_DOWN: PCIe link is down
 * @PLD_BUS_EVENT_INVALID: invalid event type
 */

enum pld_bus_event {
	PLD_BUS_EVENT_PCIE_LINK_DOWN = 0,

	PLD_BUS_EVENT_INVALID = 0xFFFF,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
/**
 * enum pld_device_config - Get PLD device config
 * @PLD_IPA_DISABLED: IPA is disabled
 */
enum pld_device_config {
	PLD_IPA_DISABLED,
};
#endif

/**
 * struct pld_uevent_data - uevent status received from platform driver
 * @uevent: uevent type
 * @fw_down: FW down info
 * @hang_data: FW hang data
 * @bus_data: bus related data
 */
struct pld_uevent_data {
	enum pld_uevent uevent;
	union {
		struct {
			bool crashed;
		} fw_down;
		struct {
			void *hang_event_data;
			u16 hang_event_data_len;
		} hang_data;
		struct {
			enum pld_bus_event etype;
			void *event_data;
		} bus_data;
	};
};

/**
 * struct pld_ce_tgt_pipe_cfg - copy engine target pipe configuration
 * @pipe_num: pipe number
 * @pipe_dir: pipe direction
 * @nentries: number of entries
 * @nbytes_max: max number of bytes
 * @flags: flags
 * @reserved: reserved
 *
 * pld_ce_tgt_pipe_cfg is used to store copy engine target pipe
 * configuration.
 */
struct pld_ce_tgt_pipe_cfg {
	u32 pipe_num;
	u32 pipe_dir;
	u32 nentries;
	u32 nbytes_max;
	u32 flags;
	u32 reserved;
};

/**
 * struct pld_ce_svc_pipe_cfg - copy engine service pipe configuration
 * @service_id: service ID
 * @pipe_dir: pipe direction
 * @pipe_num: pipe number
 *
 * pld_ce_svc_pipe_cfg is used to store copy engine service pipe
 * configuration.
 */
struct pld_ce_svc_pipe_cfg {
	u32 service_id;
	u32 pipe_dir;
	u32 pipe_num;
};

/**
 * struct pld_shadow_reg_cfg - shadow register configuration
 * @ce_id: copy engine ID
 * @reg_offset: register offset
 *
 * pld_shadow_reg_cfg is used to store shadow register configuration.
 */
struct pld_shadow_reg_cfg {
	u16 ce_id;
	u16 reg_offset;
};

/**
 * struct pld_shadow_reg_v2_cfg - shadow register version 2 configuration
 * @addr: shadow register physical address
 *
 * pld_shadow_reg_v2_cfg is used to store shadow register version 2
 * configuration.
 */
struct pld_shadow_reg_v2_cfg {
	u32 addr;
};

#ifdef CONFIG_SHADOW_V3
struct pld_shadow_reg_v3_cfg {
	u32 addr;
};
#endif

/**
 * struct pld_rri_over_ddr_cfg - rri_over_ddr configuration
 * @base_addr_low: lower 32bit
 * @base_addr_high: higher 32bit
 *
 * pld_rri_over_ddr_cfg_s is used in Genoa to pass rri_over_ddr configuration
 * to firmware to update ring/write index in host DDR.
 */
struct pld_rri_over_ddr_cfg {
	u32 base_addr_low;
	u32 base_addr_high;
};

/**
 * struct pld_wlan_enable_cfg - WLAN FW configuration
 * @num_ce_tgt_cfg: number of CE target configuration
 * @ce_tgt_cfg: CE target configuration
 * @num_ce_svc_pipe_cfg: number of CE service configuration
 * @ce_svc_cfg: CE service configuration
 * @num_shadow_reg_cfg: number of shadow register configuration
 * @shadow_reg_cfg: shadow register configuration
 * @num_shadow_reg_v2_cfg: number of shadow register version 2 configuration
 * @shadow_reg_v2_cfg: shadow register version 2 configuration
 * @rri_over_ddr_cfg_valid: valid flag for rri_over_ddr config
 * @rri_over_ddr_cfg: rri over ddr config
 * @num_shadow_reg_v3_cfg: number of shadow register version 3 configuration
 * @shadow_reg_v3_cfg: shadow register version 3 configuration
 *
 * pld_wlan_enable_cfg stores WLAN FW configurations. It will be
 * passed to WLAN FW when WLAN host driver calls wlan_enable.
 */
struct pld_wlan_enable_cfg {
	u32 num_ce_tgt_cfg;
	struct pld_ce_tgt_pipe_cfg *ce_tgt_cfg;
	u32 num_ce_svc_pipe_cfg;
	struct pld_ce_svc_pipe_cfg *ce_svc_cfg;
	u32 num_shadow_reg_cfg;
	struct pld_shadow_reg_cfg *shadow_reg_cfg;
	u32 num_shadow_reg_v2_cfg;
	struct pld_shadow_reg_v2_cfg *shadow_reg_v2_cfg;
	bool rri_over_ddr_cfg_valid;
	struct pld_rri_over_ddr_cfg rri_over_ddr_cfg;
#ifdef CONFIG_SHADOW_V3
	u32 num_shadow_reg_v3_cfg;
	struct pld_shadow_reg_v3_cfg *shadow_reg_v3_cfg;
#endif
};

/**
 * enum pld_driver_mode - WLAN host driver mode
 * @PLD_MISSION: mission mode
 * @PLD_FTM: FTM mode
 * @PLD_EPPING: EPPING mode
 * @PLD_WALTEST: WAL test mode, FW standalone test mode
 * @PLD_OFF: OFF mode
 * @PLD_COLDBOOT_CALIBRATION: Cold Boot Calibration Mode
 * @PLD_FTM_COLDBOOT_CALIBRATION: Cold Boot Calibration for FTM Mode
 */
enum pld_driver_mode {
	PLD_MISSION,
	PLD_FTM,
	PLD_EPPING,
	PLD_WALTEST,
	PLD_OFF,
	PLD_COLDBOOT_CALIBRATION = 7,
	PLD_FTM_COLDBOOT_CALIBRATION = 10
};

/**
 * struct pld_device_version - WLAN device version info
 * @family_number: family number of WLAN SOC HW
 * @device_number: device number of WLAN SOC HW
 * @major_version: major version of WLAN SOC HW
 * @minor_version: minor version of WLAN SOC HW
 *
 * pld_device_version is used to store WLAN device version info
 */

struct pld_device_version {
	u32 family_number;
	u32 device_number;
	u32 major_version;
	u32 minor_version;
};

/**
 * struct pld_dev_mem_info - WLAN device memory info
 * @start: start address of the memory block
 * @size: size of the memory block
 *
 * pld_dev_mem_info is used to store WLAN device memory info
 */
struct pld_dev_mem_info {
	u64 start;
	u64 size;
};

/**
 * enum pld_wlan_hw_nss_info - WLAN HW nss info
 * @PLD_WLAN_HW_CAP_NSS_UNSPECIFIED: nss info not specified
 * @PLD_WLAN_HW_CAP_NSS_1x1: supported nss link 1x1
 * @PLD_WLAN_HW_CAP_NSS_2x2: supported nss link 2x2
 */
enum pld_wlan_hw_nss_info {
	PLD_WLAN_HW_CAP_NSS_UNSPECIFIED,
	PLD_WLAN_HW_CAP_NSS_1x1,
	PLD_WLAN_HW_CAP_NSS_2x2
};

/**
 * enum pld_wlan_hw_channel_bw_info - WLAN HW channel bw info
 * @PLD_WLAN_HW_CHANNEL_BW_UNSPECIFIED: bw info not specified
 * @PLD_WLAN_HW_CHANNEL_BW_80MHZ: supported bw 80MHZ
 * @PLD_WLAN_HW_CHANNEL_BW_160MHZ: supported bw 160MHZ
 */
enum pld_wlan_hw_channel_bw_info  {
	PLD_WLAN_HW_CHANNEL_BW_UNSPECIFIED,
	PLD_WLAN_HW_CHANNEL_BW_80MHZ,
	PLD_WLAN_HW_CHANNEL_BW_160MHZ
};

/**
 * enum pld_wlan_hw_qam_info - WLAN HW qam info
 * @PLD_WLAN_HW_QAM_UNSPECIFIED: QAM info not specified
 * @PLD_WLAN_HW_QAM_1K: 1K QAM supported
 * @PLD_WLAN_HW_QAM_4K: 4K QAM supported
 */
enum pld_wlan_hw_qam_info  {
	PLD_WLAN_HW_QAM_UNSPECIFIED,
	PLD_WLAN_HW_QAM_1K,
	PLD_WLAN_HW_QAM_4K
};

/**
 * struct pld_wlan_hw_cap_info - WLAN HW cap info
 * @nss: nss info
 * @bw: bw info
 * @qam: qam info
 */
struct pld_wlan_hw_cap_info {
	enum pld_wlan_hw_nss_info nss;
	enum pld_wlan_hw_channel_bw_info bw;
	enum pld_wlan_hw_qam_info qam;
};

#define PLD_MAX_TIMESTAMP_LEN 32
#define PLD_WLFW_MAX_BUILD_ID_LEN 128
#define PLD_MAX_DEV_MEM_NUM 4

/**
 * struct pld_soc_info - SOC information
 * @v_addr: virtual address of preallocated memory
 * @p_addr: physical address of preallcoated memory
 * @chip_id: chip ID
 * @chip_family: chip family
 * @board_id: board ID
 * @soc_id: SOC ID
 * @fw_version: FW version
 * @fw_build_timestamp: FW build timestamp
 * @device_version: WLAN device version info
 * @dev_mem_info: WLAN device memory info
 * @fw_build_id: Firmware build identifier
 * @hw_cap_info: WLAN HW capabilities info
 *
 * pld_soc_info is used to store WLAN SOC information.
 */
struct pld_soc_info {
	void __iomem *v_addr;
	phys_addr_t p_addr;
	u32 chip_id;
	u32 chip_family;
	u32 board_id;
	u32 soc_id;
	u32 fw_version;
	char fw_build_timestamp[PLD_MAX_TIMESTAMP_LEN + 1];
	struct pld_device_version device_version;
	struct pld_dev_mem_info dev_mem_info[PLD_MAX_DEV_MEM_NUM];
	char fw_build_id[PLD_WLFW_MAX_BUILD_ID_LEN + 1];
	struct pld_wlan_hw_cap_info hw_cap_info;
};

/**
 * enum pld_recovery_reason - WLAN host driver recovery reason
 * @PLD_REASON_DEFAULT: default
 * @PLD_REASON_LINK_DOWN: PCIe link down
 */
enum pld_recovery_reason {
	PLD_REASON_DEFAULT,
	PLD_REASON_LINK_DOWN
};

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
/**
 * enum pld_wlan_time_sync_trigger_type - WLAN time sync trigger type
 * @PLD_TRIGGER_POSITIVE_EDGE: Positive edge trigger
 * @PLD_TRIGGER_NEGATIVE_EDGE: Negative edge trigger
 */
enum pld_wlan_time_sync_trigger_type {
	PLD_TRIGGER_POSITIVE_EDGE,
	PLD_TRIGGER_NEGATIVE_EDGE
};
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */

/* MAX channel avoid ranges supported in PLD */
#define PLD_CH_AVOID_MAX_RANGE   4

/**
 * struct pld_ch_avoid_freq_type
 * @start_freq: start freq (MHz)
 * @end_freq: end freq (Mhz)
 */
struct pld_ch_avoid_freq_type {
	uint32_t start_freq;
	uint32_t end_freq;
};

/**
 * struct pld_ch_avoid_ind_type
 * @ch_avoid_range_cnt: count
 * @avoid_freq_range: avoid freq range array
 */
struct pld_ch_avoid_ind_type {
	uint32_t ch_avoid_range_cnt;
	struct pld_ch_avoid_freq_type
		avoid_freq_range[PLD_CH_AVOID_MAX_RANGE];
};

/**
 * struct pld_driver_ops - driver callback functions
 * @probe: required operation, will be called when device is detected
 * @remove: required operation, will be called when device is removed
 * @idle_shutdown: required operation, will be called when device is doing
 *                 idle shutdown after interface inactivity timer has fired
 * @idle_restart: required operation, will be called when device is doing
 *                idle restart after idle shutdown
 * @shutdown: optional operation, will be called during SSR
 * @reinit: optional operation, will be called during SSR
 * @crash_shutdown: optional operation, will be called when a crash is
 *                  detected
 * @suspend: required operation, will be called for power management
 *           is enabled
 * @resume: required operation, will be called for power management
 *          is enabled
 * @reset_resume: required operation, will be called for power management
 *                is enabled
 * @modem_status: optional operation, will be called when platform driver
 *                sending modem power status to WLAN FW
 * @uevent: optional operation, will be called when platform driver
 *                 updating driver status
 * @collect_driver_dump: optional operation, will be called during SSR to
 *                       collect driver memory dump
 * @runtime_suspend: optional operation, prepare the device for a condition
 *                   in which it won't be able to communicate with the CPU(s)
 *                   and RAM due to power management.
 * @runtime_resume: optional operation, put the device into the fully
 *                  active state in response to a wakeup event generated by
 *                  hardware or at the request of software.
 * @suspend_noirq: optional operation, complete the actions started by suspend()
 * @resume_noirq: optional operation, prepare for the execution of resume()
 * @set_curr_therm_cdev_state: optional operation, will be called when there is
 *                        change in the thermal level triggered by the thermal
 *                        subsystem thus requiring mitigation actions. This will
 *                        be called every time there is a change in the state
 *                        and after driver load.
 */
struct pld_driver_ops {
	int (*probe)(struct device *dev,
		     enum pld_bus_type bus_type,
		     void *bdev, void *id);
	void (*remove)(struct device *dev,
		       enum pld_bus_type bus_type);
	int (*idle_shutdown)(struct device *dev,
			      enum pld_bus_type bus_type);
	int (*idle_restart)(struct device *dev,
			     enum pld_bus_type bus_type);
	void (*shutdown)(struct device *dev,
			 enum pld_bus_type bus_type);
	int (*reinit)(struct device *dev,
		      enum pld_bus_type bus_type,
		      void *bdev, void *id);
	void (*crash_shutdown)(struct device *dev,
			       enum pld_bus_type bus_type);
	int (*suspend)(struct device *dev,
		       enum pld_bus_type bus_type,
		       pm_message_t state);
	int (*resume)(struct device *dev,
		      enum pld_bus_type bus_type);
	int (*reset_resume)(struct device *dev,
		      enum pld_bus_type bus_type);
	void (*modem_status)(struct device *dev,
			     enum pld_bus_type bus_type,
			     int state);
	void (*uevent)(struct device *dev, struct pld_uevent_data *uevent);
#ifdef WLAN_FEATURE_SSR_DRIVER_DUMP
	int (*collect_driver_dump)(struct device *dev,
				   enum pld_bus_type bus_type,
				   struct cnss_ssr_driver_dump_entry
				   *input_array,
				   size_t *num_entries_loaded);
#endif
	int (*runtime_suspend)(struct device *dev,
			       enum pld_bus_type bus_type);
	int (*runtime_resume)(struct device *dev,
			      enum pld_bus_type bus_type);
	int (*suspend_noirq)(struct device *dev,
			     enum pld_bus_type bus_type);
	int (*resume_noirq)(struct device *dev,
			    enum pld_bus_type bus_type);
	int (*set_curr_therm_cdev_state)(struct device *dev,
					 unsigned long state,
					 int mon_id);
};

/**
 * pld_init() - Initialize PLD module
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_init(void);

/**
 * pld_deinit() - Uninitialize PLD module
 *
 * Return: void
 */
void pld_deinit(void);

/**
 * pld_set_mode() - set driver mode in PLD module
 * @mode: driver mode
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_set_mode(u8 mode);

/**
 * pld_register_driver() - Register driver to kernel
 * @ops: Callback functions that will be registered to kernel
 *
 * This function should be called when other modules want to
 * register platform driver callback functions to kernel. The
 * probe() is expected to be called after registration if the
 * device is online.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_register_driver(struct pld_driver_ops *ops);

/**
 * pld_unregister_driver() - Unregister driver to kernel
 *
 * This function should be called when other modules want to
 * unregister callback functions from kernel. The remove() is
 * expected to be called after registration.
 *
 * Return: void
 */
void pld_unregister_driver(void);

/**
 * pld_wlan_enable() - Enable WLAN
 * @dev: device
 * @config: WLAN configuration data
 * @mode: WLAN mode
 *
 * This function enables WLAN FW. It passed WLAN configuration data,
 * WLAN mode and host software version to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
		    enum pld_driver_mode mode);

/**
 * pld_wlan_disable() - Disable WLAN
 * @dev: device
 * @mode: WLAN mode
 *
 * This function disables WLAN FW. It passes WLAN mode to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_wlan_disable(struct device *dev, enum pld_driver_mode mode);

/**
 * pld_set_fw_log_mode() - Set FW debug log mode
 * @dev: device
 * @fw_log_mode: 0 for No log, 1 for WMI, 2 for DIAG
 *
 * Switch Fw debug log mode between DIAG logging and WMI logging.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_set_fw_log_mode(struct device *dev, u8 fw_log_mode);

/**
 * pld_get_default_fw_files() - Get default FW file names
 * @pfw_files: buffer for FW file names
 *
 * Return default FW file names to the buffer.
 *
 * Return: void
 */
void pld_get_default_fw_files(struct pld_fw_files *pfw_files);

/**
 * pld_get_fw_files_for_target() - Get FW file names
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
int pld_get_fw_files_for_target(struct device *dev,
				struct pld_fw_files *pfw_files,
				u32 target_type, u32 target_version);

/**
 * pld_prevent_l1() - Prevent PCIe enter L1 state
 * @dev: device
 *
 * Prevent PCIe enter L1 and L1ss states
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_prevent_l1(struct device *dev);

/**
 * pld_allow_l1() - Allow PCIe enter L1 state
 * @dev: device
 *
 * Allow PCIe enter L1 and L1ss states
 *
 * Return: void
 */
void pld_allow_l1(struct device *dev);

/**
 * pld_set_pcie_gen_speed() - Set PCIE gen speed
 * @dev: device
 * @pcie_gen_speed: Required PCIE gen speed
 *
 * Send required PCIE Gen speed to platform driver
 *
 * Return: 0 for success. Negative error codes.
 */
int pld_set_pcie_gen_speed(struct device *dev, u8 pcie_gen_speed);

/**
 * pld_is_pci_link_down() - Notification for pci link down event
 * @dev: device
 *
 * Notify platform that pci link is down.
 *
 * Return: void
 */
void pld_is_pci_link_down(struct device *dev);

/**
 * pld_get_bus_reg_dump() - Get bus reg dump
 * @dev: device
 * @buf: buffer for hang data
 * @len: len of hang data
 *
 * Get pci reg dump for hang data.
 *
 * Return: void
 */
void pld_get_bus_reg_dump(struct device *dev, uint8_t *buf, uint32_t len);

int pld_shadow_control(struct device *dev, bool enable);

/**
 * pld_schedule_recovery_work() - Schedule recovery work
 * @dev: device
 * @reason: recovery reason
 *
 * Schedule a system self recovery work.
 *
 * Return: void
 */
void pld_schedule_recovery_work(struct device *dev,
				enum pld_recovery_reason reason);

/**
 * pld_wlan_hw_enable() - Enable WLAN HW
 *
 * This function enables WLAN HW. If WLAN is secured disabled at boot all wlan
 * boot time activities are deferred. This is used to run deferred activities
 * after wlan is enabled.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_wlan_hw_enable(void);

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
/**
 * pld_get_audio_wlan_timestamp() - Get audio timestamp
 * @dev: device pointer
 * @type: trigger type
 * @ts: audio timestamp
 *
 * This API can be used to get audio timestamp.
 *
 * Return: 0 if trigger to get audio timestamp is successful
 *         Non zero failure code for errors
 */
int pld_get_audio_wlan_timestamp(struct device *dev,
				 enum pld_wlan_time_sync_trigger_type type,
				 uint64_t *ts);
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */

#ifdef CNSS_UTILS
#ifdef CNSS_UTILS_VENDOR_UNSAFE_CHAN_API_SUPPORT
/**
 * pld_get_wlan_unsafe_channel_sap() - Get vendor unsafe ch freq ranges
 * @dev: device
 * @ch_avoid_ranges: unsafe freq channel ranges
 *
 * Get vendor specific unsafe channel frequency ranges
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_get_wlan_unsafe_channel_sap(
	struct device *dev, struct pld_ch_avoid_ind_type *ch_avoid_ranges);
#else
static inline
int pld_get_wlan_unsafe_channel_sap(
	struct device *dev, struct pld_ch_avoid_ind_type *ch_avoid_ranges)
{
	return 0;
}
#endif

/**
 * pld_set_wlan_unsafe_channel() - Set unsafe channel
 * @dev: device
 * @unsafe_ch_list: unsafe channel list
 * @ch_count: number of channel
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_set_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 ch_count)
{
	return cnss_utils_set_wlan_unsafe_channel(dev, unsafe_ch_list,
						  ch_count);
}
/**
 * pld_get_wlan_unsafe_channel() - Get unsafe channel
 * @dev: device
 * @unsafe_ch_list: buffer to unsafe channel list
 * @ch_count: number of channel
 * @buf_len: buffer length
 *
 * Return WLAN unsafe channel to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_get_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 *ch_count, u16 buf_len)
{
	return cnss_utils_get_wlan_unsafe_channel(dev, unsafe_ch_list,
						  ch_count, buf_len);
}
/**
 * pld_wlan_set_dfs_nol() - Set DFS info
 * @dev: device
 * @info: DFS info
 * @info_len: info length
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_wlan_set_dfs_nol(struct device *dev, void *info,
				       u16 info_len)
{
	return cnss_utils_wlan_set_dfs_nol(dev, info, info_len);
}
/**
 * pld_wlan_get_dfs_nol() - Get DFS info
 * @dev: device
 * @info: buffer to DFS info
 * @info_len: info length
 *
 * Return DFS info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int pld_wlan_get_dfs_nol(struct device *dev,
				       void *info, u16 info_len)
{
	return cnss_utils_wlan_get_dfs_nol(dev, info, info_len);
}
/**
 * pld_get_wlan_mac_address() - API to query MAC address from Platform
 * Driver
 * @dev: Device Structure
 * @num: Pointer to number of MAC address supported
 *
 * Platform Driver can have MAC address stored. This API needs to be used
 * to get those MAC address
 *
 * Return: Pointer to the list of MAC address
 */
static inline uint8_t *pld_get_wlan_mac_address(struct device *dev,
						uint32_t *num)
{
	return cnss_utils_get_wlan_mac_address(dev, num);
}

/**
 * pld_get_wlan_derived_mac_address() - API to query derived MAC address
 * from platform Driver
 * @dev: Device Structure
 * @num: Pointer to number of MAC address supported
 *
 * Platform Driver can have MAC address stored. This API needs to be used
 * to get those MAC address
 *
 * Return: Pointer to the list of MAC address
 */
static inline uint8_t *pld_get_wlan_derived_mac_address(struct device *dev,
							uint32_t *num)
{
	return cnss_utils_get_wlan_derived_mac_address(dev, num);
}

/**
 * pld_increment_driver_load_cnt() - Maintain driver load count
 * @dev: device
 *
 * This function maintain a count which get increase whenever wiphy
 * is registered
 *
 * Return: void
 */
static inline void pld_increment_driver_load_cnt(struct device *dev)
{
	cnss_utils_increment_driver_load_cnt(dev);
}
/**
 * pld_get_driver_load_cnt() - get driver load count
 * @dev: device
 *
 * This function provide total wiphy registration count from starting
 *
 * Return: driver load count
 */
static inline int pld_get_driver_load_cnt(struct device *dev)
{
	return cnss_utils_get_driver_load_cnt(dev);
}
#else
static inline int pld_get_wlan_unsafe_channel_sap(
	struct device *dev, struct pld_ch_avoid_ind_type *ch_avoid_ranges)
{
	return 0;
}

static inline int pld_set_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 ch_count)
{
	return 0;
}
static inline int pld_get_wlan_unsafe_channel(struct device *dev,
					      u16 *unsafe_ch_list,
					      u16 *ch_count, u16 buf_len)
{
	*ch_count = 0;

	return 0;
}
static inline int pld_wlan_set_dfs_nol(struct device *dev,
				       void *info, u16 info_len)
{
	return -EINVAL;
}
static inline int pld_wlan_get_dfs_nol(struct device *dev,
				       void *info, u16 info_len)
{
	return -EINVAL;
}
static inline uint8_t *pld_get_wlan_mac_address(struct device *dev,
						uint32_t *num)
{
	*num = 0;
	return NULL;
}

static inline uint8_t *pld_get_wlan_derived_mac_address(struct device *dev,
							uint32_t *num)
{
	*num = 0;
	return NULL;
}

static inline void pld_increment_driver_load_cnt(struct device *dev) {}
static inline int pld_get_driver_load_cnt(struct device *dev)
{
	return -EINVAL;
}
#endif

/**
 * pld_wlan_pm_control() - WLAN PM control on PCIE
 * @dev: device
 * @vote: 0 for enable PCIE PC, 1 for disable PCIE PC
 *
 * This is for PCIE power collapse control during suspend/resume.
 * When PCIE power collapse is disabled, WLAN FW can access memory
 * through PCIE when system is suspended.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_wlan_pm_control(struct device *dev, bool vote);

/**
 * pld_get_virt_ramdump_mem() - Get virtual ramdump memory
 * @dev: device
 * @size: buffer to virtual memory size
 *
 * Return: virtual ramdump memory address
 */
void *pld_get_virt_ramdump_mem(struct device *dev, unsigned long *size);

/**
 * pld_release_virt_ramdump_mem() - Release virtual ramdump memory
 * @dev: device
 * @address: buffer to virtual memory address
 *
 * Return: void
 */
void pld_release_virt_ramdump_mem(struct device *dev, void *address);

/**
 * pld_device_crashed() - Notification for device crash event
 * @dev: device
 *
 * Notify subsystem a device crashed event. A subsystem restart
 * is expected to happen after calling this function.
 *
 * Return: void
 */
void pld_device_crashed(struct device *dev);

/**
 * pld_device_self_recovery() - Device self recovery
 * @dev: device
 * @reason: recovery reason
 *
 * Return: void
 */
void pld_device_self_recovery(struct device *dev,
			      enum pld_recovery_reason reason);

/**
 * pld_intr_notify_q6() - Notify Q6 FW interrupts
 * @dev: device
 *
 * Notify Q6 that a FW interrupt is triggered.
 *
 * Return: void
 */
void pld_intr_notify_q6(struct device *dev);

/**
 * pld_request_pm_qos() - Request system PM
 * @dev: device
 * @qos_val: request value
 *
 * It votes for the value of aggregate QoS expectations.
 *
 * Return: void
 */
void pld_request_pm_qos(struct device *dev, u32 qos_val);

/**
 * pld_remove_pm_qos() - Remove system PM
 * @dev: device
 *
 * Remove the vote request for Qos expectations.
 *
 * Return: void
 */
void pld_remove_pm_qos(struct device *dev);

/**
 * pld_request_bus_bandwidth() - Request bus bandwidth
 * @dev: device
 * @bandwidth: bus bandwidth
 *
 * Votes for HIGH/MEDIUM/LOW bus bandwidth.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_request_bus_bandwidth(struct device *dev, int bandwidth);

/**
 * pld_get_platform_cap() - Get platform capabilities
 * @dev: device
 * @cap: buffer to the capabilities
 *
 * Return capabilities to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_get_platform_cap(struct device *dev, struct pld_platform_cap *cap);

/**
 * pld_get_sha_hash() - Get sha hash number
 * @dev: device
 * @data: input data
 * @data_len: data length
 * @hash_idx: hash index
 * @out:  output buffer
 *
 * Return computed hash to the out buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_get_sha_hash(struct device *dev, const u8 *data,
		     u32 data_len, u8 *hash_idx, u8 *out);

/**
 * pld_get_fw_ptr() - Get secure FW memory address
 * @dev: device
 *
 * Return: secure memory address
 */
void *pld_get_fw_ptr(struct device *dev);

/**
 * pld_auto_suspend() - Auto suspend
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_auto_suspend(struct device *dev);

/**
 * pld_auto_resume() - Auto resume
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_auto_resume(struct device *dev);

/**
 * pld_force_wake_request() - Request vote to assert WAKE register
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_force_wake_request(struct device *dev);

/**
 * pld_is_direct_link_supported() - Get whether direct_link is supported
 *                                  by FW or not
 * @dev: device
 *
 * Return: true if supported
 *         false on failure or if not supported
 */
bool pld_is_direct_link_supported(struct device *dev);

/**
 * pld_force_wake_request_sync() - Request to awake MHI synchronously
 * @dev: device
 * @timeout_us: timeout in micro-sec request to wake
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_force_wake_request_sync(struct device *dev, int timeout_us);

/**
 * pld_exit_power_save() - Send EXIT_POWER_SAVE QMI to FW
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_exit_power_save(struct device *dev);

/**
 * pld_is_device_awake() - Check if it's ready to access MMIO registers
 * @dev: device
 *
 * Return: True for device awake
 *         False for device not awake
 *         Negative failure code for errors
 */
int pld_is_device_awake(struct device *dev);

/**
 * pld_force_wake_release() - Release vote to assert WAKE register
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_force_wake_release(struct device *dev);

/**
 * pld_ce_request_irq() - Register IRQ for CE
 * @dev: device
 * @ce_id: CE number
 * @handler: IRQ callback function
 * @flags: IRQ flags
 * @name: IRQ name
 * @ctx: IRQ context
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ce_request_irq(struct device *dev, unsigned int ce_id,
		       irqreturn_t (*handler)(int, void *),
		       unsigned long flags, const char *name, void *ctx);

/**
 * pld_ce_free_irq() - Free IRQ for CE
 * @dev: device
 * @ce_id: CE number
 * @ctx: IRQ context
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ce_free_irq(struct device *dev, unsigned int ce_id, void *ctx);

/**
 * pld_enable_irq() - Enable IRQ for CE
 * @dev: device
 * @ce_id: CE number
 *
 * Return: void
 */
void pld_enable_irq(struct device *dev, unsigned int ce_id);

/**
 * pld_disable_irq() - Disable IRQ for CE
 * @dev: device
 * @ce_id: CE number
 *
 * Return: void
 */
void pld_disable_irq(struct device *dev, unsigned int ce_id);

/**
 * pld_get_soc_info() - Get SOC information
 * @dev: device
 * @info: buffer to SOC information
 *
 * Return SOC info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_get_soc_info(struct device *dev, struct pld_soc_info *info);

/**
 * pld_get_mhi_state() - Get MHI state Info
 * @dev: device
 *
 * MHI state can be determined by reading this address.
 *
 * Return: MHI state
 */
int pld_get_mhi_state(struct device *dev);

/**
 * pld_is_pci_ep_awake() - Check if PCI EP is L0 state
 * @dev: device
 *
 * Return: True for PCI EP awake
 *         False for PCI EP not awake
 *         Negative failure code for errors
 */
int pld_is_pci_ep_awake(struct device *dev);

/**
 * pld_get_ce_id() - Get CE number for the provided IRQ
 * @dev: device
 * @irq: IRQ number
 *
 * Return: CE number
 */
int pld_get_ce_id(struct device *dev, int irq);

/**
 * pld_get_irq() - Get IRQ number for given CE ID
 * @dev: device
 * @ce_id: CE ID
 *
 * Return: IRQ number
 */
int pld_get_irq(struct device *dev, int ce_id);

/**
 * pld_lock_reg_window() - Lock register window spinlock
 * @dev: device pointer
 * @flags: variable pointer to save CPU states
 *
 * It uses spinlock_bh so avoid calling in top half context.
 *
 * Return: void
 */
void pld_lock_reg_window(struct device *dev, unsigned long *flags);

/**
 * pld_unlock_reg_window() - Unlock register window spinlock
 * @dev: device pointer
 * @flags: variable pointer to save CPU states
 *
 * It uses spinlock_bh so avoid calling in top half context.
 *
 * Return: void
 */
void pld_unlock_reg_window(struct device *dev, unsigned long *flags);

/**
 * pld_get_pci_slot() - Get PCI slot of attached device
 * @dev: device
 *
 * Return: pci slot
 */
int pld_get_pci_slot(struct device *dev);

/**
 * pld_power_on() - Power on WLAN hardware
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_power_on(struct device *dev);

/**
 * pld_power_off() - Power off WLAN hardware
 * @dev: device
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_power_off(struct device *dev);

/**
 * pld_athdiag_read() - Read data from WLAN FW
 * @dev: device
 * @offset: address offset
 * @memtype: memory type
 * @datalen: data length
 * @output: output buffer
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_athdiag_read(struct device *dev, uint32_t offset, uint32_t memtype,
		     uint32_t datalen, uint8_t *output);

/**
 * pld_athdiag_write() - Write data to WLAN FW
 * @dev: device
 * @offset: address offset
 * @memtype: memory type
 * @datalen: data length
 * @input: input buffer
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_athdiag_write(struct device *dev, uint32_t offset, uint32_t memtype,
		      uint32_t datalen, uint8_t *input);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
/**
 * pld_smmu_get_domain() - Get SMMU domain
 * @dev: device
 *
 * Return: Pointer to the domain
 */
void *pld_smmu_get_domain(struct device *dev);
#else
/**
 * pld_smmu_get_mapping() - Get SMMU mapping context
 * @dev: device
 *
 * Return: Pointer to the mapping context
 */
void *pld_smmu_get_mapping(struct device *dev);
#endif

/**
 * pld_smmu_map() - Map SMMU
 * @dev: device
 * @paddr: physical address that needs to map to
 * @iova_addr: IOVA address
 * @size: size to be mapped
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_smmu_map(struct device *dev, phys_addr_t paddr,
		 uint32_t *iova_addr, size_t size);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
struct kobject *pld_get_wifi_kobj(struct device *dev);
#else
static inline struct kobject *pld_get_wifi_kobj(struct device *dev)
{
	return NULL;
}
#endif

/**
 * pld_smmu_unmap() - Unmap SMMU
 * @dev: device
 * @iova_addr: IOVA address to be unmapped
 * @size: size to be unmapped
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
#ifdef CONFIG_SMMU_S1_UNMAP
int pld_smmu_unmap(struct device *dev,
		   uint32_t iova_addr, size_t size);
#else
static inline int pld_smmu_unmap(struct device *dev,
				 uint32_t iova_addr, size_t size)
{
	return 0;
}
#endif

/**
 * pld_get_user_msi_assignment() - Get MSI assignment information
 * @dev: device structure
 * @user_name: name of the user who requests the MSI assignment
 * @num_vectors: number of the MSI vectors assigned for the user
 * @user_base_data: MSI base data assigned for the user, this equals to
 *                  endpoint base data from config space plus base vector
 * @base_vector: base MSI vector (offset) number assigned for the user
 *
 * Return: 0 for success
 *         Negative failure code for errors
 */
int pld_get_user_msi_assignment(struct device *dev, char *user_name,
				int *num_vectors, uint32_t *user_base_data,
				uint32_t *base_vector);

/**
 * pld_get_msi_irq() - Get MSI IRQ number used for request_irq()
 * @dev: device structure
 * @vector: MSI vector (offset) number
 *
 * Return: Positive IRQ number for success
 *         Negative failure code for errors
 */
int pld_get_msi_irq(struct device *dev, unsigned int vector);

/**
 * pld_get_msi_address() - Get the MSI address
 * @dev: device structure
 * @msi_addr_low: lower 32-bit of the address
 * @msi_addr_high: higher 32-bit of the address
 *
 * Return: Void
 */
void pld_get_msi_address(struct device *dev, uint32_t *msi_addr_low,
			 uint32_t *msi_addr_high);

/**
 * pld_is_drv_connected() - Check if DRV subsystem is connected
 * @dev: device structure
 *
 *  Return: 1 DRV is connected
 *          0 DRV is not connected
 *          Non zero failure code for errors
 */
int pld_is_drv_connected(struct device *dev);

/**
 * pld_socinfo_get_serial_number() - Get SOC serial number
 * @dev: device
 *
 * Return: SOC serial number
 */
unsigned int pld_socinfo_get_serial_number(struct device *dev);

/**
 * pld_is_qmi_disable() - Check QMI support is present or not
 * @dev: device
 *
 *  Return: 1 QMI is not supported
 *          0 QMI is supported
 *          Non zero failure code for errors
 */
int pld_is_qmi_disable(struct device *dev);

/**
 * pld_is_fw_down() - Check WLAN fw is down or not
 *
 * @dev: device
 *
 * This API will be called to check if WLAN FW is down or not.
 *
 *  Return: 0 FW is not down
 *          Otherwise FW is down
 *          Always return 0 for unsupported bus type
 */
int pld_is_fw_down(struct device *dev);

/**
 * pld_force_assert_target() - Send a force assert request to FW.
 * @dev: device pointer
 *
 * This can use various sideband requests available at platform driver to
 * initiate a FW assert.
 *
 * Context: Any context
 * Return:
 * 0 - force assert of FW is triggered successfully.
 * -EOPNOTSUPP - force assert is not supported.
 * Other non-zero codes - other failures or errors
 */
int pld_force_assert_target(struct device *dev);

/**
 * pld_force_collect_target_dump() - Collect FW dump after asserting FW.
 * @dev: device pointer
 *
 * This API will send force assert request to FW and wait till FW dump has
 * been collected.
 *
 * Context: Process context only since this is a blocking call.
 * Return:
 * 0 - FW dump is collected successfully.
 * -EOPNOTSUPP - forcing assert and collecting FW dump is not supported.
 * -ETIMEDOUT - FW dump collection is timed out for any reason.
 * Other non-zero codes - other failures or errors
 */
int pld_force_collect_target_dump(struct device *dev);

/**
 * pld_qmi_send_get() - Indicate certain data to be sent over QMI
 * @dev: device pointer
 *
 * This API can be used to indicate certain data to be sent over QMI.
 * pld_qmi_send() is expected to be called later.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_qmi_send_get(struct device *dev);

/**
 * pld_qmi_send_put() - Indicate response sent over QMI has been processed
 * @dev: device pointer
 *
 * This API can be used to indicate response of the data sent over QMI has
 * been processed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_qmi_send_put(struct device *dev);

/**
 * pld_qmi_send() - Send data request over QMI
 * @dev: device pointer
 * @type: type of the send data operation
 * @cmd: buffer pointer of send data request command
 * @cmd_len: size of the command buffer
 * @cb_ctx: context pointer if any to pass back in callback
 * @cb: callback pointer to pass response back
 *
 * This API can be used to send data request over QMI.
 *
 * Return: 0 if data request sends successfully
 *         Non zero failure code for errors
 */
int pld_qmi_send(struct device *dev, int type, void *cmd,
		 int cmd_len, void *cb_ctx,
		 int (*cb)(void *ctx, void *event, int event_len));

/**
 * pld_qmi_indication() - Send data request over QMI
 * @dev: device pointer
 * @cb_ctx: context pointer if any to pass back in callback
 * @cb: callback pointer to pass response back
 *
 * This API can be used to register for QMI events.
 *
 * Return: 0 if registration is successful
 *         Non zero failure code for errors
 */
int pld_qmi_indication(struct device *dev, void *cb_ctx,
		       int (*cb)(void *ctx, uint16_t type,
				 void *event, int event_len));

/**
 * pld_is_fw_dump_skipped() - get fw dump skipped status.
 * @dev: device
 *
 * The subsys ssr status help the driver to decide whether to skip
 * the FW memory dump when FW assert.
 * For SDIO case, the memory dump progress takes 1 minutes to
 * complete, which is not acceptable in SSR enabled.
 *
 * Return: true if need to skip FW dump.
 */
bool pld_is_fw_dump_skipped(struct device *dev);

/**
 * pld_is_low_power_mode() - Check WLAN fw is in low power
 * @dev: device
 *
 * This API will be called to check if WLAN FW is in low power or not.
 * Low power means either Deep Sleep or Hibernate state.
 *
 * Return: 0 FW is not in low power mode
 *         Otherwise FW is low power mode
 *         Always return 0 for unsupported bus type
 */
#ifdef CONFIG_ENABLE_LOW_POWER_MODE
int pld_is_low_power_mode(struct device *dev);
#else
static inline int pld_is_low_power_mode(struct device *dev)
{
	return 0;
}
#endif

/**
 * pld_is_pdr() - Check WLAN PD is Restarted
 * @dev: device
 *
 * Help the driver decide whether FW down is due to
 * WLAN PD Restart.
 *
 * Return: 1 WLAN PD is Restarted
 *         0 WLAN PD is not Restarted
 */
int pld_is_pdr(struct device *dev);

/**
 * pld_is_fw_rejuvenate() - Check WLAN fw is rejuvenating
 * @dev: device
 *
 * Help the driver decide whether FW down is due to
 * SSR or FW rejuvenate.
 *
 * Return: 1 FW is rejuvenating
 *         0 FW is not rejuvenating
 */
int pld_is_fw_rejuvenate(struct device *dev);

/**
 * pld_have_platform_driver_support() - check if platform driver support
 * @dev: device
 *
 * Return: true if platform driver support.
 */
bool pld_have_platform_driver_support(struct device *dev);

/**
 * pld_idle_shutdown - request idle shutdown callback from platform driver
 * @dev: pointer to struct dev
 * @shutdown_cb: pointer to hdd psoc idle shutdown callback handler
 *
 * Return: 0 for success and non-zero negative error code for failure
 */
int pld_idle_shutdown(struct device *dev,
		      int (*shutdown_cb)(struct device *dev));

/**
 * pld_idle_restart - request idle restart callback from platform driver
 * @dev: pointer to struct dev
 * @restart_cb: pointer to hdd psoc idle restart callback handler
 *
 * Return: 0 for success and non-zero negative error code for failure
 */
int pld_idle_restart(struct device *dev,
		     int (*restart_cb)(struct device *dev));

/**
 * pld_srng_devm_request_irq() - Register IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 * @handler: IRQ callback function
 * @irqflags: IRQ flags
 * @name: IRQ name
 * @ctx: IRQ context
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_srng_devm_request_irq(struct device *dev, int irq,
			      irq_handler_t handler,
			      unsigned long irqflags,
			      const char *name,
			      void *ctx);

/**
 * pld_srng_request_irq() - Register IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 * @handler: IRQ callback function
 * @irqflags: IRQ flags
 * @name: IRQ name
 * @ctx: IRQ context
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_srng_request_irq(struct device *dev, int irq, irq_handler_t handler,
			 unsigned long irqflags,
			 const char *name,
			 void *ctx);

/**
 * pld_srng_free_irq() - Free IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 * @ctx: IRQ context
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_srng_free_irq(struct device *dev, int irq, void *ctx);

/**
 * pld_srng_enable_irq() - Enable IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 *
 * Return: void
 */
void pld_srng_enable_irq(struct device *dev, int irq);

/**
 * pld_srng_disable_irq() - Disable IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 *
 * Return: void
 */
void pld_srng_disable_irq(struct device *dev, int irq);

/**
 * pld_srng_disable_irq_sync() - Synchronouus disable IRQ for SRNG
 * @dev: device
 * @irq: IRQ number
 *
 * Return: void
 */
void pld_srng_disable_irq_sync(struct device *dev, int irq);

/**
 * pld_pci_read_config_word() - Read PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_read_config_word(struct pci_dev *pdev, int offset, uint16_t *val);

/**
 * pld_pci_write_config_word() - Write PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_write_config_word(struct pci_dev *pdev, int offset, uint16_t val);

/**
 * pld_pci_read_config_dword() - Read PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_read_config_dword(struct pci_dev *pdev, int offset, uint32_t *val);

/**
 * pld_pci_write_config_dword() - Write PCI config
 * @pdev: pci device
 * @offset: Config space offset
 * @val : Value
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pci_write_config_dword(struct pci_dev *pdev, int offset, uint32_t val);

/**
 * pld_thermal_register() - Register the thermal device with the thermal system
 * @dev: The device structure
 * @state: The max state to be configured on registration
 * @mon_id: Thermal cooling device ID
 *
 * Return: Error code on error
 */
int pld_thermal_register(struct device *dev, unsigned long state, int mon_id);

/**
 * pld_thermal_unregister() - Unregister the device with the thermal system
 * @dev: The device structure
 * @mon_id: Thermal cooling device ID
 *
 * Return: None
 */
void pld_thermal_unregister(struct device *dev, int mon_id);

/**
 * pld_set_wfc_mode() - Sent WFC mode to FW via platform driver
 * @dev: The device structure
 * @wfc_mode: WFC Modes (0 => Inactive, 1 => Active)
 *
 * Return: Error code on error
 */
int pld_set_wfc_mode(struct device *dev, enum pld_wfc_mode wfc_mode);

/**
 * pld_bus_width_type_to_str() - Helper function to convert PLD bandwidth level
 *				 to string
 * @level: PLD bus width level
 *
 * Return: String corresponding to input "level"
 */
const char *pld_bus_width_type_to_str(enum pld_bus_width_type level);

/**
 * pld_get_thermal_state() - Get the current thermal state from the PLD
 * @dev: The device structure
 * @thermal_state: param to store the current thermal state
 * @mon_id: Thermal cooling device ID
 *
 * Return: Non-zero code for error; zero for success
 */
int pld_get_thermal_state(struct device *dev, unsigned long *thermal_state,
			  int mon_id);

/**
 * pld_set_tsf_sync_period() - Set TSF sync period
 * @dev: device
 * @val: TSF sync time value
 *
 * Return: void
 */
void pld_set_tsf_sync_period(struct device *dev, u32 val);

/**
 * pld_reset_tsf_sync_period() - Reset TSF sync period
 * @dev: device
 *
 * Return: void
 */
void pld_reset_tsf_sync_period(struct device *dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
/**
 * pld_is_ipa_offload_disabled() - Check if IPA offload is enabled or not
 * @dev: The device structure
 *
 * Return: Non-zero code for IPA offload disable; zero for IPA offload enable
 */
int pld_is_ipa_offload_disabled(struct device *dev);
#else
static inline
int pld_is_ipa_offload_disabled(struct device *dev)
{
	return 0;
}
#endif

#if defined(CNSS_MEM_PRE_ALLOC) && defined(FEATURE_SKB_PRE_ALLOC)

/**
 * pld_nbuf_pre_alloc() - get allocated nbuf from platform driver.
 * @size: Netbuf requested size
 *
 * Return: nbuf or NULL if no memory
 */
static inline struct sk_buff *pld_nbuf_pre_alloc(size_t size)
{
	struct sk_buff *skb = NULL;

	if (size >= WCNSS_PRE_SKB_ALLOC_GET_THRESHOLD)
		skb = wcnss_skb_prealloc_get(size);

	return skb;
}

/**
 * pld_nbuf_pre_alloc_free() - free the nbuf allocated in platform driver.
 * @skb: Pointer to network buffer
 *
 * Return: TRUE if the nbuf is freed
 */
static inline int pld_nbuf_pre_alloc_free(struct sk_buff *skb)
{
	return wcnss_skb_prealloc_put(skb);
}
#else
static inline struct sk_buff *pld_nbuf_pre_alloc(size_t size)
{
	return NULL;
}
static inline int pld_nbuf_pre_alloc_free(struct sk_buff *skb)
{
	return 0;
}
#endif

#ifdef CONFIG_AFC_SUPPORT
/**
 * pld_send_buffer_to_afcmem() - Send afc data to afc memory
 * @dev: The device structure
 * @afcdb: Pointer to afc data buffer
 * @len: Length of afc data
 * @slotid: Slot id of afc memory
 *
 * Return: Non-zero code for error; zero for success
 */
int pld_send_buffer_to_afcmem(struct device *dev, const uint8_t *afcdb,
			      uint32_t len, uint8_t slotid);

/**
 * pld_reset_afcmem() - Reset afc data in afc memory
 * @dev: The device structure
 * @slotid: Slot id of afc memory
 *
 * Return: Non-zero code for error; zero for success
 */
int pld_reset_afcmem(struct device *dev, uint8_t slotid);
#else
static inline
int pld_send_buffer_to_afcmem(struct device *dev, const uint8_t *afcdb,
			      uint32_t len, uint8_t slotid)
{
	return -EINVAL;
}

static inline
int pld_reset_afcmem(struct device *dev, uint8_t slotid)
{
	return -EINVAL;
}
#endif

/**
 * pld_get_bus_type() - Bus type of the device
 * @dev: device
 *
 * Return: PLD bus type
 */
enum pld_bus_type pld_get_bus_type(struct device *dev);

static inline int pfrm_devm_request_irq(struct device *dev, unsigned int ce_id,
					irqreturn_t (*handler)(int, void *),
					unsigned long flags, const char *name,
					void *ctx)
{
	return pld_srng_devm_request_irq(dev, ce_id, handler, flags, name, ctx);
}

static inline int pfrm_request_irq(struct device *dev, unsigned int ce_id,
				   irqreturn_t (*handler)(int, void *),
				   unsigned long flags, const char *name,
				   void *ctx)
{
	return pld_srng_request_irq(dev, ce_id, handler, flags, name, ctx);
}

static inline int pfrm_free_irq(struct device *dev, int irq, void *ctx)
{
	return pld_srng_free_irq(dev, irq, ctx);
}

static inline void pfrm_enable_irq(struct device *dev, int irq)
{
	pld_srng_enable_irq(dev, irq);
}

static inline void pfrm_disable_irq_nosync(struct device *dev, int irq)
{
	pld_srng_disable_irq(dev, irq);
}

static inline void pfrm_disable_irq(struct device *dev, int irq)
{
	pld_srng_disable_irq_sync(dev, irq);
}

static inline int pfrm_read_config_word(struct pci_dev *pdev, int offset,
					uint16_t *val)
{
	return pld_pci_read_config_word(pdev, offset, val);
}

static inline int pfrm_write_config_word(struct pci_dev *pdev, int offset,
					 uint16_t val)
{
	return pld_pci_write_config_word(pdev, offset, val);
}

static inline int pfrm_read_config_dword(struct pci_dev *pdev, int offset,
					 uint32_t *val)
{
	return pld_pci_read_config_dword(pdev, offset, val);
}

static inline int pfrm_write_config_dword(struct pci_dev *pdev, int offset,
					  uint32_t val)
{
	return pld_pci_write_config_dword(pdev, offset, val);
}

static inline bool pld_get_enable_intx(struct device *dev)
{
	return false;
}

/**
 * pld_is_one_msi()- whether one MSI is used or not
 * @dev: device structure
 *
 * Return: true if it is one MSI
 */
bool pld_is_one_msi(struct device *dev);

#ifdef FEATURE_DIRECT_LINK
/**
 * pld_audio_smmu_map()- Map memory region into Audio SMMU CB
 * @dev: pointer to device structure
 * @paddr: physical address
 * @iova: DMA address
 * @size: memory region size
 *
 * Return: 0 on success else failure code
 */
int pld_audio_smmu_map(struct device *dev, phys_addr_t paddr, dma_addr_t iova,
		       size_t size);

/**
 * pld_audio_smmu_unmap()- Remove memory region mapping from Audio SMMU CB
 * @dev: pointer to device structure
 * @iova: DMA address
 * @size: memory region size
 *
 * Return: None
 */
void pld_audio_smmu_unmap(struct device *dev, dma_addr_t iova, size_t size);
#else
static inline
int pld_audio_smmu_map(struct device *dev, phys_addr_t paddr, dma_addr_t iova,
		       size_t size)
{
	return 0;
}

static inline
void pld_audio_smmu_unmap(struct device *dev, dma_addr_t iova, size_t size)
{
}
#endif
#endif
