// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.*/

#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk/qcom.h>
#include <linux/compiler.h>
#include <linux/crc8.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ipc_logging.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/msm_pcie.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/reset.h>
#include <linux/regulator/consumer.h>
#include <linux/rpmsg.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <soc/qcom/subsystem_notif.h>

#include "../pci.h"

#define PCIE_VENDOR_ID_QCOM (0x17cb)

#define PCIE20_PARF_DBI_BASE_ADDR (0x350)
#define PCIE20_PARF_SLV_ADDR_SPACE_SIZE (0x358)

#define PCIE_GEN3_PRESET_DEFAULT (0x55555555)
#define PCIE_GEN3_SPCIE_CAP (0x0154)
#define PCIE_GEN3_GEN2_CTRL (0x080c)
#define PCIE_GEN3_RELATED (0x0890)
#define PCIE_GEN3_EQ_CONTROL (0x08a8)
#define PCIE_GEN3_EQ_FB_MODE_DIR_CHANGE (0x08ac)
#define PCIE_GEN3_MISC_CONTROL (0x08bc)

#define PCIE20_PARF_SYS_CTRL (0x00)
#define PCIE20_PARF_PM_CTRL (0x20)
#define PCIE20_PARF_PM_STTS (0x24)
#define PCIE20_PARF_PHY_CTRL (0x40)
#define PCIE20_PARF_TEST_BUS (0xe4)
#define PCIE20_PARF_MHI_CLOCK_RESET_CTRL (0x174)
#define PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT (0x1a8)
#define PCIE20_PARF_LTSSM (0x1b0)
#define PCIE20_PARF_INT_ALL_STATUS (0x224)
#define PCIE20_PARF_INT_ALL_CLEAR (0x228)
#define PCIE20_PARF_INT_ALL_MASK (0x22c)
#define PCIE20_PARF_DEVICE_TYPE (0x1000)
#define PCIE20_PARF_BDF_TO_SID_TABLE_N (0x2000)
#define PCIE20_PARF_L1SUB_AHB_CLK_MAX_TIMER (0x180)
#define PCIE20_PARF_DEBUG_INT_EN (0x190)

#define PCIE20_PARF_CLKREQ_OVERRIDE (0x2b0)
#define PCIE20_PARF_CLKREQ_IN_VALUE (BIT(3))
#define PCIE20_PARF_CLKREQ_IN_ENABLE (BIT(1))

#define PCIE20_ELBI_SYS_CTRL (0x04)
#define PCIE20_ELBI_SYS_STTS (0x08)

#define PCIE20_CAP (0x70)
#define PCIE20_CAP_DEVCTRLSTATUS (PCIE20_CAP + 0x08)
#define PCIE20_CAP_LINKCTRLSTATUS (PCIE20_CAP + 0x10)

#define PCIE20_COMMAND_STATUS (0x04)
#define PCIE20_HEADER_TYPE (0x0c)
#define PCIE20_BRIDGE_CTRL (0x3c)
#define PCIE20_DEVICE_CONTROL_STATUS (0x78)
#define PCIE20_DEVICE_CONTROL2_STATUS2 (0x98)

#define PCIE20_AUX_CLK_FREQ_REG (0xb40)
#define PCIE20_ACK_F_ASPM_CTRL_REG (0x70c)
#define PCIE20_ACK_N_FTS (0xff00)

#define PCIE20_PLR_IATU_VIEWPORT (0x900)
#define PCIE20_PLR_IATU_CTRL1 (0x904)
#define PCIE20_PLR_IATU_CTRL2 (0x908)
#define PCIE20_PLR_IATU_LBAR (0x90c)
#define PCIE20_PLR_IATU_UBAR (0x910)
#define PCIE20_PLR_IATU_LAR (0x914)
#define PCIE20_PLR_IATU_LTAR (0x918)
#define PCIE20_PLR_IATU_UTAR (0x91c)

#define PCIE_IATU_BASE(n) (n * 0x200)
#define PCIE_IATU_CTRL1(n) (PCIE_IATU_BASE(n) + 0x00)
#define PCIE_IATU_CTRL2(n) (PCIE_IATU_BASE(n) + 0x04)
#define PCIE_IATU_LBAR(n) (PCIE_IATU_BASE(n) + 0x08)
#define PCIE_IATU_UBAR(n) (PCIE_IATU_BASE(n) + 0x0c)
#define PCIE_IATU_LAR(n) (PCIE_IATU_BASE(n) + 0x10)
#define PCIE_IATU_LTAR(n) (PCIE_IATU_BASE(n) + 0x14)
#define PCIE_IATU_UTAR(n) (PCIE_IATU_BASE(n) + 0x18)

#define PCIE20_PORT_LINK_CTRL_REG (0x710)
#define PCIE20_PIPE_LOOPBACK_CONTROL (0x8b8)
#define LOOPBACK_BASE_ADDR_OFFSET (0x8000)

#define PCIE20_CTRL1_TYPE_CFG0 (0x04)
#define PCIE20_CTRL1_TYPE_CFG1 (0x05)

#define PCIE20_CAP_ID (0x10)
#define L1SUB_CAP_ID (0x1e)

#define PCIE_CAP_PTR_OFFSET (0x34)
#define PCIE_EXT_CAP_OFFSET (0x100)

#define PCIE20_AER_UNCORR_ERR_STATUS_REG (0x104)
#define PCIE20_AER_CORR_ERR_STATUS_REG (0x110)
#define PCIE20_AER_ROOT_ERR_STATUS_REG (0x130)
#define PCIE20_AER_ERR_SRC_ID_REG (0x134)

#define RD (0)
#define WR (1)
#define MSM_PCIE_ERROR (-1)

#define PERST_PROPAGATION_DELAY_US_MIN (1000)
#define PERST_PROPAGATION_DELAY_US_MAX (1005)
#define SWITCH_DELAY_MAX (20)
#define REFCLK_STABILIZATION_DELAY_US_MIN (1000)
#define REFCLK_STABILIZATION_DELAY_US_MAX (1005)
#define LINK_UP_TIMEOUT_US_MIN (5000)
#define LINK_UP_TIMEOUT_US_MAX (5100)
#define LINK_UP_CHECK_MAX_COUNT (20)
#define EP_UP_TIMEOUT_US_MIN (1000)
#define EP_UP_TIMEOUT_US_MAX (1005)
#define EP_UP_TIMEOUT_US (1000000)
#define PHY_STABILIZATION_DELAY_US_MIN (995)
#define PHY_STABILIZATION_DELAY_US_MAX (1005)

#define MSM_PCIE_CRC8_POLYNOMIAL (BIT(2) | BIT(1) | BIT(0))

#define GEN1_SPEED (0x1)
#define GEN2_SPEED (0x2)
#define GEN3_SPEED (0x3)

#define LINK_WIDTH_X1 (0x1)
#define LINK_WIDTH_X2 (0x3)
#define LINK_WIDTH_MASK (0x3f)
#define LINK_WIDTH_SHIFT (16)

#define RATE_CHANGE_19P2MHZ (19200000)
#define RATE_CHANGE_100MHZ (100000000)

#define MSM_PCIE_LTSSM_MASK (0x3f)

#define MSM_PCIE_DRV_MAJOR_VERSION (1)
#define MSM_PCIE_DRV_MINOR_VERSION (0)
#define MSM_PCIE_DRV_SEQ_RESV (0xffff)

#define IPC_TIMEOUT_MS (250)

#define PHY_READY_TIMEOUT_COUNT (10)
#define XMLH_LINK_UP (0x400)
#define MAX_PROP_SIZE (32)
#define MAX_RC_NAME_LEN (15)
#define MSM_PCIE_MAX_VREG (4)
#define MSM_PCIE_MAX_CLK (14)
#define MSM_PCIE_MAX_PIPE_CLK (1)
#define MAX_RC_NUM (3)
#define MAX_DEVICE_NUM (20)
#define PCIE_TLP_RD_SIZE (0x5)
#define PCIE_LOG_PAGES (50)
#define PCIE_CONF_SPACE_DW (1024)
#define PCIE_CLEAR (0xdeadbeef)
#define PCIE_LINK_DOWN (0xffffffff)

#define MSM_PCIE_MAX_RESET (5)
#define MSM_PCIE_MAX_PIPE_RESET (1)

/* Each tick is 19.2 MHz */
#define L1SS_TIMEOUT_US_TO_TICKS(x) (x * 192 / 10)
#define L1SS_TIMEOUT_US (100000)

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define PCIE_UPPER_ADDR(addr) ((u32)((addr) >> 32))
#else
#define PCIE_UPPER_ADDR(addr) (0x0)
#endif
#define PCIE_LOWER_ADDR(addr) ((u32)((addr) & 0xffffffff))

#define PCIE_BUS_PRIV_DATA(bus) \
	(struct msm_pcie_dev_t *)(bus->sysdata)

/* Config Space Offsets */
#define BDF_OFFSET(bus, devfn) \
	((bus << 24) | (devfn << 16))

#define PCIE_GEN_DBG(x...) do { \
	if (msm_pcie_debug_mask) \
		pr_alert(x); \
	} while (0)

#define PCIE_DBG(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log_long)   \
		ipc_log_string((dev)->ipc_log_long, \
			"DBG1:%s: " fmt, __func__, ##arg); \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, ##arg); \
	if (msm_pcie_debug_mask)   \
		pr_alert("%s: " fmt, __func__, ##arg);		  \
	} while (0)

#define PCIE_DBG2(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "DBG2:%s: " fmt, \
				__func__, ##arg);\
	if (msm_pcie_debug_mask)   \
		pr_alert("%s: " fmt, __func__, ##arg);              \
	} while (0)

#define PCIE_DBG3(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "DBG3:%s: " fmt, \
				__func__, ##arg);\
	if (msm_pcie_debug_mask)   \
		pr_alert("%s: " fmt, __func__, ##arg);              \
	} while (0)

#define PCIE_DUMP(dev, fmt, arg...) do {			\
	if ((dev) && (dev)->ipc_log_dump) \
		ipc_log_string((dev)->ipc_log_dump, \
			"DUMP:%s: " fmt, __func__, ##arg); \
	} while (0)

#define PCIE_DBG_FS(dev, fmt, arg...) do {			\
	if ((dev) && (dev)->ipc_log_dump) \
		ipc_log_string((dev)->ipc_log_dump, \
			"DBG_FS:%s: " fmt, __func__, ##arg); \
	pr_alert("%s: " fmt, __func__, ##arg); \
	} while (0)

#define PCIE_INFO(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log_long)   \
		ipc_log_string((dev)->ipc_log_long, \
			"INFO:%s: " fmt, __func__, ##arg); \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, ##arg); \
	pr_info("%s: " fmt, __func__, ##arg);  \
	} while (0)

#define PCIE_ERR(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log_long)   \
		ipc_log_string((dev)->ipc_log_long, \
			"ERR:%s: " fmt, __func__, ##arg); \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, ##arg); \
	pr_err("%s: " fmt, __func__, arg);  \
	} while (0)


enum msm_pcie_res {
	MSM_PCIE_RES_PARF,
	MSM_PCIE_RES_PHY,
	MSM_PCIE_RES_DM_CORE,
	MSM_PCIE_RES_ELBI,
	MSM_PCIE_RES_IATU,
	MSM_PCIE_RES_CONF,
	MSM_PCIE_RES_TCSR,
	MSM_PCIE_RES_RUMI,
	MSM_PCIE_MAX_RES,
};

enum msm_pcie_irq {
	MSM_PCIE_INT_A,
	MSM_PCIE_INT_B,
	MSM_PCIE_INT_C,
	MSM_PCIE_INT_D,
	MSM_PCIE_INT_GLOBAL_INT,
	MSM_PCIE_MAX_IRQ,
};

enum msm_pcie_irq_event {
	MSM_PCIE_INT_EVT_LINK_DOWN = 1,
	MSM_PCIE_INT_EVT_BME,
	MSM_PCIE_INT_EVT_PM_TURNOFF,
	MSM_PCIE_INT_EVT_DEBUG,
	MSM_PCIE_INT_EVT_LTR,
	MSM_PCIE_INT_EVT_MHI_Q6,
	MSM_PCIE_INT_EVT_MHI_A7,
	MSM_PCIE_INT_EVT_DSTATE_CHANGE,
	MSM_PCIE_INT_EVT_L1SUB_TIMEOUT,
	MSM_PCIE_INT_EVT_MMIO_WRITE,
	MSM_PCIE_INT_EVT_CFG_WRITE,
	MSM_PCIE_INT_EVT_BRIDGE_FLUSH_N,
	MSM_PCIE_INT_EVT_LINK_UP,
	MSM_PCIE_INT_EVT_AER_LEGACY,
	MSM_PCIE_INT_EVT_AER_ERR,
	MSM_PCIE_INT_EVT_PME_LEGACY,
	MSM_PCIE_INT_EVT_PLS_PME,
	MSM_PCIE_INT_EVT_INTD,
	MSM_PCIE_INT_EVT_INTC,
	MSM_PCIE_INT_EVT_INTB,
	MSM_PCIE_INT_EVT_INTA,
	MSM_PCIE_INT_EVT_EDMA,
	MSM_PCIE_INT_EVT_MSI_0,
	MSM_PCIE_INT_EVT_MSI_1,
	MSM_PCIE_INT_EVT_MSI_2,
	MSM_PCIE_INT_EVT_MSI_3,
	MSM_PCIE_INT_EVT_MSI_4,
	MSM_PCIE_INT_EVT_MSI_5,
	MSM_PCIE_INT_EVT_MSI_6,
	MSM_PCIE_INT_EVT_MSI_7,
	MSM_PCIE_INT_EVT_MAX = 30,
};

enum msm_pcie_gpio {
	MSM_PCIE_GPIO_PERST,
	MSM_PCIE_GPIO_WAKE,
	MSM_PCIE_GPIO_EP,
	MSM_PCIE_MAX_GPIO
};

enum msm_pcie_link_status {
	MSM_PCIE_LINK_DEINIT,
	MSM_PCIE_LINK_ENABLED,
	MSM_PCIE_LINK_DISABLED,
	MSM_PCIE_LINK_DRV,
};

enum msm_pcie_boot_option {
	MSM_PCIE_NO_PROBE_ENUMERATION = BIT(0),
	MSM_PCIE_NO_WAKE_ENUMERATION = BIT(1)
};

enum msm_pcie_ltssm {
	MSM_PCIE_LTSSM_DETECT_QUIET = 0x00,
	MSM_PCIE_LTSSM_DETECT_ACT = 0x01,
	MSM_PCIE_LTSSM_POLL_ACTIVE = 0x02,
	MSM_PCIE_LTSSM_POLL_COMPLIANCE = 0x03,
	MSM_PCIE_LTSSM_POLL_CONFIG = 0x04,
	MSM_PCIE_LTSSM_PRE_DETECT_QUIET = 0x05,
	MSM_PCIE_LTSSM_DETECT_WAIT = 0x06,
	MSM_PCIE_LTSSM_CFG_LINKWD_START = 0x07,
	MSM_PCIE_LTSSM_CFG_LINKWD_ACEPT = 0x08,
	MSM_PCIE_LTSSM_CFG_LANENUM_WAIT = 0x09,
	MSM_PCIE_LTSSM_CFG_LANENUM_ACEPT = 0x0a,
	MSM_PCIE_LTSSM_CFG_COMPLETE = 0x0b,
	MSM_PCIE_LTSSM_CFG_IDLE = 0x0c,
	MSM_PCIE_LTSSM_RCVRY_LOCK = 0x0d,
	MSM_PCIE_LTSSM_RCVRY_SPEED = 0x0e,
	MSM_PCIE_LTSSM_RCVRY_RCVRCFG = 0x0f,
	MSM_PCIE_LTSSM_RCVRY_IDLE = 0x10,
	MSM_PCIE_LTSSM_RCVRY_EQ0 = 0x20,
	MSM_PCIE_LTSSM_RCVRY_EQ1 = 0x21,
	MSM_PCIE_LTSSM_RCVRY_EQ2 = 0x22,
	MSM_PCIE_LTSSM_RCVRY_EQ3 = 0x23,
	MSM_PCIE_LTSSM_L0 = 0x11,
	MSM_PCIE_LTSSM_L0S = 0x12,
	MSM_PCIE_LTSSM_L123_SEND_EIDLE = 0x13,
	MSM_PCIE_LTSSM_L1_IDLE = 0x14,
	MSM_PCIE_LTSSM_L2_IDLE = 0x15,
	MSM_PCIE_LTSSM_L2_WAKE = 0x16,
	MSM_PCIE_LTSSM_DISABLED_ENTRY = 0x17,
	MSM_PCIE_LTSSM_DISABLED_IDLE = 0x18,
	MSM_PCIE_LTSSM_DISABLED = 0x19,
	MSM_PCIE_LTSSM_LPBK_ENTRY = 0x1a,
	MSM_PCIE_LTSSM_LPBK_ACTIVE = 0x1b,
	MSM_PCIE_LTSSM_LPBK_EXIT = 0x1c,
	MSM_PCIE_LTSSM_LPBK_EXIT_TIMEOUT = 0x1d,
	MSM_PCIE_LTSSM_HOT_RESET_ENTRY = 0x1e,
	MSM_PCIE_LTSSM_HOT_RESET = 0x1f,
};

static const char * const msm_pcie_ltssm_str[] = {
	[MSM_PCIE_LTSSM_DETECT_QUIET] = "LTSSM_DETECT_QUIET",
	[MSM_PCIE_LTSSM_DETECT_ACT] = "LTSSM_DETECT_ACT",
	[MSM_PCIE_LTSSM_POLL_ACTIVE] = "LTSSM_POLL_ACTIVE",
	[MSM_PCIE_LTSSM_POLL_COMPLIANCE] = "LTSSM_POLL_COMPLIANCE",
	[MSM_PCIE_LTSSM_POLL_CONFIG] = "LTSSM_POLL_CONFIG",
	[MSM_PCIE_LTSSM_PRE_DETECT_QUIET] = "LTSSM_PRE_DETECT_QUIET",
	[MSM_PCIE_LTSSM_DETECT_WAIT] = "LTSSM_DETECT_WAIT",
	[MSM_PCIE_LTSSM_CFG_LINKWD_START] = "LTSSM_CFG_LINKWD_START",
	[MSM_PCIE_LTSSM_CFG_LINKWD_ACEPT] = "LTSSM_CFG_LINKWD_ACEPT",
	[MSM_PCIE_LTSSM_CFG_LANENUM_WAIT] = "LTSSM_CFG_LANENUM_WAIT",
	[MSM_PCIE_LTSSM_CFG_LANENUM_ACEPT] = "LTSSM_CFG_LANENUM_ACEPT",
	[MSM_PCIE_LTSSM_CFG_COMPLETE] = "LTSSM_CFG_COMPLETE",
	[MSM_PCIE_LTSSM_CFG_IDLE] = "LTSSM_CFG_IDLE",
	[MSM_PCIE_LTSSM_RCVRY_LOCK] = "LTSSM_RCVRY_LOCK",
	[MSM_PCIE_LTSSM_RCVRY_SPEED] = "LTSSM_RCVRY_SPEED",
	[MSM_PCIE_LTSSM_RCVRY_RCVRCFG] = "LTSSM_RCVRY_RCVRCFG",
	[MSM_PCIE_LTSSM_RCVRY_IDLE] = "LTSSM_RCVRY_IDLE",
	[MSM_PCIE_LTSSM_RCVRY_EQ0] = "LTSSM_RCVRY_EQ0",
	[MSM_PCIE_LTSSM_RCVRY_EQ1] = "LTSSM_RCVRY_EQ1",
	[MSM_PCIE_LTSSM_RCVRY_EQ2] = "LTSSM_RCVRY_EQ2",
	[MSM_PCIE_LTSSM_RCVRY_EQ3] = "LTSSM_RCVRY_EQ3",
	[MSM_PCIE_LTSSM_L0] = "LTSSM_L0",
	[MSM_PCIE_LTSSM_L0S] = "LTSSM_L0S",
	[MSM_PCIE_LTSSM_L123_SEND_EIDLE] = "LTSSM_L123_SEND_EIDLE",
	[MSM_PCIE_LTSSM_L1_IDLE] = "LTSSM_L1_IDLE",
	[MSM_PCIE_LTSSM_L2_IDLE] = "LTSSM_L2_IDLE",
	[MSM_PCIE_LTSSM_L2_WAKE] = "LTSSM_L2_WAKE",
	[MSM_PCIE_LTSSM_DISABLED_ENTRY] = "LTSSM_DISABLED_ENTRY",
	[MSM_PCIE_LTSSM_DISABLED_IDLE] = "LTSSM_DISABLED_IDLE",
	[MSM_PCIE_LTSSM_DISABLED] = "LTSSM_DISABLED",
	[MSM_PCIE_LTSSM_LPBK_ENTRY] = "LTSSM_LPBK_ENTRY",
	[MSM_PCIE_LTSSM_LPBK_ACTIVE] = "LTSSM_LPBK_ACTIVE",
	[MSM_PCIE_LTSSM_LPBK_EXIT] = "LTSSM_LPBK_EXIT",
	[MSM_PCIE_LTSSM_LPBK_EXIT_TIMEOUT] = "LTSSM_LPBK_EXIT_TIMEOUT",
	[MSM_PCIE_LTSSM_HOT_RESET_ENTRY] = "LTSSM_HOT_RESET_ENTRY",
	[MSM_PCIE_LTSSM_HOT_RESET] = "LTSSM_HOT_RESET",
};

#define TO_LTSSM_STR(state) ((state) >= ARRAY_SIZE(msm_pcie_ltssm_str) ? \
				"LTSSM_INVALID" : msm_pcie_ltssm_str[state])

enum msm_pcie_debugfs_option {
	MSM_PCIE_OUTPUT_PCIE_INFO,
	MSM_PCIE_DISABLE_LINK,
	MSM_PCIE_ENABLE_LINK,
	MSM_PCIE_DISABLE_ENABLE_LINK,
	MSM_PCIE_DUMP_SHADOW_REGISTER,
	MSM_PCIE_DISABLE_L0S,
	MSM_PCIE_ENABLE_L0S,
	MSM_PCIE_DISABLE_L1,
	MSM_PCIE_ENABLE_L1,
	MSM_PCIE_DISABLE_L1SS,
	MSM_PCIE_ENABLE_L1SS,
	MSM_PCIE_ENUMERATION,
	MSM_PCIE_READ_PCIE_REGISTER,
	MSM_PCIE_WRITE_PCIE_REGISTER,
	MSM_PCIE_DUMP_PCIE_REGISTER_SPACE,
	MSM_PCIE_ALLOCATE_DDR_MAP_LBAR,
	MSM_PCIE_FREE_DDR_UNMAP_LBAR,
	MSM_PCIE_OUTPUT_DDR_LBAR_ADDRESS,
	MSM_PCIE_CONFIGURE_LOOPBACK,
	MSM_PCIE_SETUP_LOOPBACK_IATU,
	MSM_PCIE_READ_DDR,
	MSM_PCIE_READ_LBAR,
	MSM_PCIE_WRITE_DDR,
	MSM_PCIE_WRITE_LBAR,
	MSM_PCIE_DISABLE_AER,
	MSM_PCIE_ENABLE_AER,
	MSM_PCIE_GPIO_STATUS,
	MSM_PCIE_ASSERT_PERST,
	MSM_PCIE_DEASSERT_PERST,
	MSM_PCIE_KEEP_RESOURCES_ON,
	MSM_PCIE_FORCE_GEN1,
	MSM_PCIE_FORCE_GEN2,
	MSM_PCIE_FORCE_GEN3,
	MSM_PCIE_MAX_DEBUGFS_OPTION
};

static const char * const
	msm_pcie_debugfs_option_desc[MSM_PCIE_MAX_DEBUGFS_OPTION] = {
	"OUTPUT PCIE INFO",
	"DISABLE LINK",
	"ENABLE LINK",
	"DISABLE AND ENABLE LINK",
	"DUMP PCIE SHADOW REGISTER",
	"DISABLE L0S",
	"ENABLE L0S",
	"DISABLE L1",
	"ENABLE L1",
	"DISABLE L1SS",
	"ENABLE L1SS",
	"ENUMERATE",
	"READ A PCIE REGISTER",
	"WRITE TO PCIE REGISTER",
	"DUMP PCIE REGISTER SPACE",
	"ALLOCATE DDR AND MAP LBAR",
	"FREE DDR AND UNMAP LBAR",
	"OUTPUT DDR AND LBAR VIR ADDRESS",
	"CONFIGURE PCIE LOOPBACK",
	"SETUP LOOPBACK IATU",
	"READ DDR",
	"READ LBAR",
	"WRITE DDR",
	"WRITE LBAR",
	"SET AER ENABLE FLAG",
	"CLEAR AER ENABLE FLAG",
	"OUTPUT PERST AND WAKE GPIO STATUS",
	"ASSERT PERST",
	"DE-ASSERT PERST",
	"SET KEEP_RESOURCES_ON FLAG",
	"SET MAXIMUM LINK SPEED TO GEN 1",
	"SET MAXIMUM LINK SPEED TO GEN 2",
	"SET MAXIMUM LINK SPEED TO GEN 3",
};

/* gpio info structure */
struct msm_pcie_gpio_info_t {
	char *name;
	uint32_t num;
	bool out;
	uint32_t on;
	uint32_t init;
	bool required;
};

/* voltage regulator info structrue */
struct msm_pcie_vreg_info_t {
	struct regulator *hdl;
	char *name;
	uint32_t max_v;
	uint32_t min_v;
	uint32_t opt_mode;
	bool required;
};

/* reset info structure */
struct msm_pcie_reset_info_t {
	struct reset_control *hdl;
	char *name;
	bool required;
};

/* clock info structure */
struct msm_pcie_clk_info_t {
	struct clk *hdl;
	char *name;
	u32 freq;
	bool config_mem;
	bool required;
	/*
	 * Suppressible clocks are not turned off during drv suspend.
	 * These clocks will be automatically gated during XO shutdown.
	 */
	bool suppressible;
};

/* resource info structure */
struct msm_pcie_res_info_t {
	char *name;
	struct resource *resource;
	void __iomem *base;
};

/* irq info structrue */
struct msm_pcie_irq_info_t {
	char *name;
	uint32_t num;
};

/* bandwidth info structure */
struct msm_pcie_bw_scale_info_t {
	u32 cx_vreg_min;
	u32 rate_change_freq;
};

/* phy info structure */
struct msm_pcie_phy_info_t {
	u32 offset;
	u32 val;
	u32 delay;
};

/* sid info structure */
struct msm_pcie_sid_info_t {
	u16 bdf;
	u8 pcie_sid;
	u8 hash;
	u8 next_hash;
	u32 smmu_sid;
	u32 value;
};

/* PCIe device info structure */
struct msm_pcie_device_info {
	u32 bdf;
	struct pci_dev *dev;
	short short_bdf;
	u32 sid;
	int domain;
	void __iomem *conf_base;
	unsigned long phy_address;
	u32 dev_ctrlstts_offset;
	struct msm_pcie_register_event *event_reg;
	bool registered;
};

/* DRV IPC command type */
enum msm_pcie_drv_cmds {
	MSM_PCIE_DRV_CMD_ENABLE = 0xc0000000,
	MSM_PCIE_DRV_CMD_DISABLE = 0xc0000001,
};

/* DRV IPC message type */
enum msm_pcie_drv_msg_id {
	MSM_PCIE_DRV_MSG_ID_ACK = 0xa,
	MSM_PCIE_DRV_MSG_ID_CMD = 0xc,
	MSM_PCIE_DRV_MSG_ID_EVT = 0xe,
};

/* DRV IPC header */
struct __packed msm_pcie_drv_header {
	u16 major_ver;
	u16 minor_ver;
	u16 msg_id;
	u16 seq;
	u16 reply_seq;
	u16 payload_size;
	u32 dev_id;
	u8 reserved[8];
};

/* DRV IPC transfer ring element */
struct __packed msm_pcie_drv_tre {
	u32 dword[4];
};

struct __packed msm_pcie_drv_msg {
	struct msm_pcie_drv_header hdr;
	struct msm_pcie_drv_tre pkt;
};

struct msm_pcie_drv_info {
	bool ep_connected; /* drv supports only one endpoint (no switch) */
	struct msm_pcie_drv_msg drv_enable; /* hand off payload */
	struct msm_pcie_drv_msg drv_disable; /* payload to request back */
	int dev_id;
	u16 seq;
	u16 reply_seq;
	u32 timeout_ms; /* IPC command timeout */
	struct completion completion;
};

/* msm pcie device structure */
struct msm_pcie_dev_t {
	struct platform_device *pdev;
	struct pci_dev *dev;
	struct regulator *gdsc;
	struct msm_pcie_vreg_info_t vreg[MSM_PCIE_MAX_VREG];
	struct msm_pcie_gpio_info_t gpio[MSM_PCIE_MAX_GPIO];
	struct msm_pcie_clk_info_t clk[MSM_PCIE_MAX_CLK];
	struct msm_pcie_clk_info_t pipeclk[MSM_PCIE_MAX_PIPE_CLK];
	struct msm_pcie_res_info_t res[MSM_PCIE_MAX_RES];
	struct msm_pcie_irq_info_t irq[MSM_PCIE_MAX_IRQ];
	struct msm_pcie_reset_info_t reset[MSM_PCIE_MAX_RESET];
	struct msm_pcie_reset_info_t pipe_reset[MSM_PCIE_MAX_PIPE_RESET];

	void __iomem *parf;
	void __iomem *phy;
	void __iomem *elbi;
	void __iomem *iatu;
	void __iomem *dm_core;
	void __iomem *conf;
	void __iomem *tcsr;
	void __iomem *rumi;

	uint32_t axi_bar_start;
	uint32_t axi_bar_end;

	uint32_t wake_n;
	uint32_t vreg_n;
	uint32_t gpio_n;
	uint32_t parf_deemph;
	uint32_t parf_swing;

	struct msm_pcie_vreg_info_t *cx_vreg;
	struct msm_pcie_clk_info_t *rate_change_clk;
	struct msm_pcie_bw_scale_info_t *bw_scale;
	u32 bw_gen_max;

	bool cfg_access;
	spinlock_t cfg_lock;
	unsigned long irqsave_flags;
	struct mutex enumerate_lock;
	struct mutex setup_lock;
	struct mutex clk_lock;

	struct irq_domain *irq_domain;

	enum msm_pcie_link_status link_status;
	bool user_suspend;
	bool disable_pc;
	struct pci_saved_state *saved_state;

	struct wakeup_source ws;
	struct msm_bus_scale_pdata *bus_scale_table;
	uint32_t bus_client;

	bool l0s_supported;
	bool l1_supported;
	bool l1ss_supported;
	bool l1_1_pcipm_supported;
	bool l1_2_pcipm_supported;
	bool l1_1_aspm_supported;
	bool l1_2_aspm_supported;
	uint32_t l1_2_th_scale;
	uint32_t l1_2_th_value;
	bool common_clk_en;
	bool clk_power_manage_en;
	bool aux_clk_sync;
	bool aer_enable;
	uint32_t smmu_sid_base;
	uint32_t link_check_max_count;
	uint32_t target_link_speed;
	uint32_t n_fts;
	uint32_t ep_latency;
	uint32_t switch_latency;
	uint32_t wr_halt_size;
	uint32_t slv_addr_space_size;
	uint32_t phy_status_offset;
	uint32_t phy_status_bit;
	uint32_t phy_power_down_offset;
	uint32_t core_preset;
	uint32_t cpl_timeout;
	uint32_t current_bdf;
	uint32_t perst_delay_us_min;
	uint32_t perst_delay_us_max;
	uint32_t tlp_rd_size;
	bool linkdown_panic;
	uint32_t boot_option;

	uint32_t rc_idx;
	uint32_t phy_ver;
	bool drv_ready;
	bool enumerated;
	struct work_struct handle_wake_work;
	struct mutex recovery_lock;
	spinlock_t wakeup_lock;
	spinlock_t irq_lock;
	struct mutex aspm_lock;
	int prevent_l1;
	ulong linkdown_counter;
	ulong link_turned_on_counter;
	ulong link_turned_off_counter;
	ulong rc_corr_counter;
	ulong rc_non_fatal_counter;
	ulong rc_fatal_counter;
	ulong ep_corr_counter;
	ulong ep_non_fatal_counter;
	ulong ep_fatal_counter;
	bool suspending;
	ulong wake_counter;
	u32 num_active_ep;
	u32 num_ep;
	bool pending_ep_reg;
	u32 phy_len;
	struct msm_pcie_phy_info_t *phy_sequence;
	u32 sid_info_len;
	struct msm_pcie_sid_info_t *sid_info;
	u32 ep_shadow[MAX_DEVICE_NUM][PCIE_CONF_SPACE_DW];
	u32 rc_shadow[PCIE_CONF_SPACE_DW];
	bool shadow_en;
	bool bridge_found;
	struct msm_pcie_register_event *event_reg;
	bool power_on;
	void *ipc_log;
	void *ipc_log_long;
	void *ipc_log_dump;
	bool use_19p2mhz_aux_clk;
	bool use_pinctrl;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_sleep;
	struct msm_pcie_device_info pcidev_table[MAX_DEVICE_NUM];

	struct msm_pcie_drv_info *drv_info;

	void (*rumi_init)(struct msm_pcie_dev_t *pcie_dev);
};

struct msm_root_dev_t {
	struct msm_pcie_dev_t *pcie_dev;
	struct pci_dev *pci_dev;
};

/* debug mask sys interface */
static int msm_pcie_debug_mask;
module_param_named(debug_mask, msm_pcie_debug_mask,
			    int, 0644);

/*
 * For each bit set, invert the default capability
 * option for the corresponding root complex
 * and its devices.
 */
static int msm_pcie_invert_l0s_support;
module_param_named(invert_l0s_support, msm_pcie_invert_l0s_support,
			    int, 0644);
static int msm_pcie_invert_l1_support;
module_param_named(invert_l1_support, msm_pcie_invert_l1_support,
			    int, 0644);
static int msm_pcie_invert_l1ss_support;
module_param_named(invert_l1ss_support, msm_pcie_invert_l1ss_support,
			    int, 0644);
static int msm_pcie_invert_aer_support;
module_param_named(invert_aer_support, msm_pcie_invert_aer_support,
			    int, 0644);

/*
 * For each bit set, keep the resources on when link training fails
 * or linkdown occurs for the corresponding root complex
 */
static int msm_pcie_keep_resources_on;
module_param_named(keep_resources_on, msm_pcie_keep_resources_on,
			    int, 0644);

/*
 * For each bit set, force the corresponding root complex
 * to do link training at gen1 speed.
 */
static int msm_pcie_force_gen1;
module_param_named(force_gen1, msm_pcie_force_gen1,
			    int, 0644);


/*
 * For each bit set in BIT[3:0] determines which corresponding
 * root complex will use the value in BIT[31:4] to override the
 * default (LINK_UP_CHECK_MAX_COUNT) max check count for link training.
 * Each iteration is LINK_UP_TIMEOUT_US_MIN long.
 */
static int msm_pcie_link_check_max_count;
module_param_named(link_check_max_count, msm_pcie_link_check_max_count,
			    int, 0644);

/* debugfs values */
static u32 rc_sel = BIT(0);
static u32 base_sel;
static u32 wr_offset;
static u32 wr_mask;
static u32 wr_value;
static u32 corr_counter_limit = 5;

/* CRC8 table for BDF to SID translation */
static u8 msm_pcie_crc8_table[CRC8_TABLE_SIZE];

/* Table to track info of PCIe devices */
static struct msm_pcie_device_info
	msm_pcie_dev_tbl[MAX_RC_NUM * MAX_DEVICE_NUM];

/* PCIe driver state */
static struct pcie_drv_sta {
	u32 rc_num;
	struct msm_pcie_dev_t *msm_pcie_dev;
	struct rpmsg_device *rpdev;
	struct work_struct drv_connect; /* connect worker */
	u32 rate_change_vote; /* each bit corresponds to RC vote for 100MHz */
	struct mutex drv_lock;
} pcie_drv;

/* msm pcie device data */
static struct msm_pcie_dev_t msm_pcie_dev[MAX_RC_NUM];

/* regulators */
static struct msm_pcie_vreg_info_t msm_pcie_vreg_info[MSM_PCIE_MAX_VREG] = {
	{NULL, "vreg-3p3", 0, 0, 0, false},
	{NULL, "vreg-1p8", 1800000, 1800000, 14000, true},
	{NULL, "vreg-0p9", 1000000, 1000000, 40000, true},
	{NULL, "vreg-cx", 0, 0, 0, false}
};

/* GPIOs */
static struct msm_pcie_gpio_info_t msm_pcie_gpio_info[MSM_PCIE_MAX_GPIO] = {
	{"perst-gpio", 0, 1, 0, 0, 1},
	{"wake-gpio", 0, 0, 0, 0, 0},
	{"qcom,ep-gpio", 0, 1, 1, 0, 0}
};

/* resets */
static struct msm_pcie_reset_info_t
msm_pcie_reset_info[MAX_RC_NUM][MSM_PCIE_MAX_RESET] = {
	{
		{NULL, "pcie_0_core_reset", false},
		{NULL, "pcie_phy_reset", false},
		{NULL, "pcie_phy_com_reset", false},
		{NULL, "pcie_phy_nocsr_com_phy_reset", false},
		{NULL, "pcie_0_phy_reset", false}
	},
	{
		{NULL, "pcie_1_core_reset", false},
		{NULL, "pcie_phy_reset", false},
		{NULL, "pcie_phy_com_reset", false},
		{NULL, "pcie_phy_nocsr_com_phy_reset", false},
		{NULL, "pcie_1_phy_reset", false}
	},
	{
		{NULL, "pcie_2_core_reset", false},
		{NULL, "pcie_phy_reset", false},
		{NULL, "pcie_phy_com_reset", false},
		{NULL, "pcie_phy_nocsr_com_phy_reset", false},
		{NULL, "pcie_2_phy_reset", false}
	}
};

/* pipe reset  */
static struct msm_pcie_reset_info_t
msm_pcie_pipe_reset_info[MAX_RC_NUM][MSM_PCIE_MAX_PIPE_RESET] = {
	{
		{NULL, "pcie_0_phy_pipe_reset", false}
	},
	{
		{NULL, "pcie_1_phy_pipe_reset", false}
	},
	{
		{NULL, "pcie_2_phy_pipe_reset", false}
	}
};

/* clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_CLK] = {
	{
	{NULL, "pcie_0_ref_clk_src", 0, false, false, false},
	{NULL, "pcie_0_aux_clk", 1010000, false, true, false},
	{NULL, "pcie_0_cfg_ahb_clk", 0, false, true, false},
	{NULL, "pcie_0_mstr_axi_clk", 0, true, true, false},
	{NULL, "pcie_0_slv_axi_clk", 0, true, true, false},
	{NULL, "pcie_0_ldo", 0, false, true, true},
	{NULL, "pcie_0_smmu_clk", 0, false, false, false},
	{NULL, "pcie_0_slv_q2a_axi_clk", 0, false, false, false},
	{NULL, "pcie_0_sleep_clk", 0, false, false, false},
	{NULL, "pcie_phy_refgen_clk", 0, false, false, true},
	{NULL, "pcie_tbu_clk", 0, false, false, true},
	{NULL, "pcie_ddrss_sf_tbu_clk", 0, false, false, true},
	{NULL, "pcie_phy_cfg_ahb_clk", 0, false, false, false},
	{NULL, "pcie_phy_aux_clk", 0, false, false, false}
	},
	{
	{NULL, "pcie_1_ref_clk_src", 0, false, false, false},
	{NULL, "pcie_1_aux_clk", 1010000, false, true, false},
	{NULL, "pcie_1_cfg_ahb_clk", 0, false, true, false},
	{NULL, "pcie_1_mstr_axi_clk", 0, true, true, false},
	{NULL, "pcie_1_slv_axi_clk", 0, true, true, false},
	{NULL, "pcie_1_ldo", 0, false, true, true},
	{NULL, "pcie_1_smmu_clk", 0, false, false, false},
	{NULL, "pcie_1_slv_q2a_axi_clk", 0, false, false, false},
	{NULL, "pcie_1_sleep_clk", 0, false, false, false},
	{NULL, "pcie_phy_refgen_clk", 0, false, false, true},
	{NULL, "pcie_tbu_clk", 0, false, false, true},
	{NULL, "pcie_ddrss_sf_tbu_clk", 0, false, false, true},
	{NULL, "pcie_phy_cfg_ahb_clk", 0, false, false, false},
	{NULL, "pcie_phy_aux_clk", 0, false, false, false}
	},
	{
	{NULL, "pcie_2_ref_clk_src", 0, false, false, false},
	{NULL, "pcie_2_aux_clk", 1010000, false, true, false},
	{NULL, "pcie_2_cfg_ahb_clk", 0, false, true, false},
	{NULL, "pcie_2_mstr_axi_clk", 0, true, true, false},
	{NULL, "pcie_2_slv_axi_clk", 0, true, true, false},
	{NULL, "pcie_2_ldo", 0, false, true, true},
	{NULL, "pcie_2_smmu_clk", 0, false, false, false},
	{NULL, "pcie_2_slv_q2a_axi_clk", 0, false, false, false},
	{NULL, "pcie_2_sleep_clk", 0, false, false, false},
	{NULL, "pcie_phy_refgen_clk", 0, false, false, true},
	{NULL, "pcie_tbu_clk", 0, false, false, true},
	{NULL, "pcie_ddrss_sf_tbu_clk", 0, false, false, true},
	{NULL, "pcie_phy_cfg_ahb_clk", 0, false, false, false},
	{NULL, "pcie_phy_aux_clk", 0, false, false, false}
	}
};

/* Pipe Clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_pipe_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_PIPE_CLK] = {
	{
	{NULL, "pcie_0_pipe_clk", 125000000, true, true, false},
	},
	{
	{NULL, "pcie_1_pipe_clk", 125000000, true, true, false},
	},
	{
	{NULL, "pcie_2_pipe_clk", 125000000, true, true, false},
	}
};

/* resources */
static const struct msm_pcie_res_info_t msm_pcie_res_info[MSM_PCIE_MAX_RES] = {
	{"parf", NULL, NULL},
	{"phy", NULL, NULL},
	{"dm_core", NULL, NULL},
	{"elbi", NULL, NULL},
	{"iatu", NULL, NULL},
	{"conf", NULL, NULL},
	{"tcsr", NULL, NULL},
	{"rumi", NULL, NULL}
};

/* irqs */
static const struct msm_pcie_irq_info_t msm_pcie_irq_info[MSM_PCIE_MAX_IRQ] = {
	{"int_a", 0},
	{"int_b", 0},
	{"int_c", 0},
	{"int_d", 0},
	{"int_global_int", 0}
};

static void msm_pcie_config_sid(struct msm_pcie_dev_t *dev);
static void msm_pcie_config_l0s_disable_all(struct msm_pcie_dev_t *dev,
				struct pci_bus *bus);
static void msm_pcie_config_l1_disable_all(struct msm_pcie_dev_t *dev,
				struct pci_bus *bus);
static void msm_pcie_config_l1ss_disable_all(struct msm_pcie_dev_t *dev,
				struct pci_bus *bus);
static void msm_pcie_config_l0s_enable_all(struct msm_pcie_dev_t *dev);
static void msm_pcie_config_l1_enable_all(struct msm_pcie_dev_t *dev);
static void msm_pcie_config_l1ss_enable_all(struct msm_pcie_dev_t *dev);

static void msm_pcie_check_l1ss_support_all(struct msm_pcie_dev_t *dev);

static void msm_pcie_config_link_pm(struct msm_pcie_dev_t *dev, bool enable);

#ifdef CONFIG_ARM
static inline void msm_pcie_fixup_irqs(struct msm_pcie_dev_t *dev)
{
	pci_fixup_irqs(pci_common_swizzle, of_irq_parse_and_map_pci);
}
#else
static inline void msm_pcie_fixup_irqs(struct msm_pcie_dev_t *dev)
{
}
#endif

static inline void msm_pcie_write_reg(void __iomem *base, u32 offset, u32 value)
{
	writel_relaxed(value, base + offset);
	/* ensure that changes propagated to the hardware */
	wmb();
}

static inline void msm_pcie_write_reg_field(void __iomem *base, u32 offset,
	const u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 tmp = readl_relaxed(base + offset);

	tmp &= ~mask; /* clear written bits */
	val = tmp | (val << shift);
	writel_relaxed(val, base + offset);
	/* ensure that changes propagated to the hardware */
	wmb();
}

static inline void msm_pcie_config_clear_set_dword(struct pci_dev *pdev,
	int pos, u32 clear, u32 set)
{
	u32 val;

	pci_read_config_dword(pdev, pos, &val);
	val &= ~clear;
	val |= set;
	pci_write_config_dword(pdev, pos, val);
}

static inline void msm_pcie_config_clock_mem(struct msm_pcie_dev_t *dev,
	struct msm_pcie_clk_info_t *info)
{
	int ret;

	ret = clk_set_flags(info->hdl, CLKFLAG_NORETAIN_MEM);
	if (ret)
		PCIE_ERR(dev,
			"PCIe: RC%d can't configure core memory for clk %s: %d.\n",
			dev->rc_idx, info->name, ret);
	else
		PCIE_DBG2(dev,
			"PCIe: RC%d configured core memory for clk %s.\n",
			dev->rc_idx, info->name);

	ret = clk_set_flags(info->hdl, CLKFLAG_NORETAIN_PERIPH);
	if (ret)
		PCIE_ERR(dev,
			"PCIe: RC%d can't configure peripheral memory for clk %s: %d.\n",
			dev->rc_idx, info->name, ret);
	else
		PCIE_DBG2(dev,
			"PCIe: RC%d configured peripheral memory for clk %s.\n",
			dev->rc_idx, info->name);
}

static void msm_pcie_rumi_init(struct msm_pcie_dev_t *pcie_dev)
{
	u32 val;
	u32 reset_offs = 0x04;
	u32 phy_ctrl_offs = 0x40;

	PCIE_DBG(pcie_dev, "PCIe: RC%d: enter.\n", pcie_dev->rc_idx);

	val = readl_relaxed(pcie_dev->rumi + phy_ctrl_offs) | 0x1000;
	writel_relaxed(val, pcie_dev->rumi + phy_ctrl_offs);
	usleep_range(10000, 10001);

	writel_relaxed(0x800, pcie_dev->rumi + reset_offs);
	usleep_range(50000, 50001);
	writel_relaxed(0xFFFFFFFF, pcie_dev->rumi + reset_offs);
	usleep_range(50000, 50001);
	writel_relaxed(0x800, pcie_dev->rumi + reset_offs);
	usleep_range(50000, 50001);
	writel_relaxed(0, pcie_dev->rumi + reset_offs);
	usleep_range(50000, 50001);

	val = readl_relaxed(pcie_dev->rumi + phy_ctrl_offs) & 0xFFFFEFFF;
	writel_relaxed(val, pcie_dev->rumi + phy_ctrl_offs);
	usleep_range(10000, 10001);

	val = readl_relaxed(pcie_dev->rumi + phy_ctrl_offs) & 0xFFFFFFFE;
	writel_relaxed(val, pcie_dev->rumi + phy_ctrl_offs);
}

static void pcie_phy_dump(struct msm_pcie_dev_t *dev)
{
	int i, size;

	size = resource_size(dev->res[MSM_PCIE_RES_PHY].resource);
	for (i = 0; i < size; i += 32) {
		PCIE_DUMP(dev,
			"PCIe PHY of RC%d: 0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
			dev->rc_idx, i,
			readl_relaxed(dev->phy + i),
			readl_relaxed(dev->phy + (i + 4)),
			readl_relaxed(dev->phy + (i + 8)),
			readl_relaxed(dev->phy + (i + 12)),
			readl_relaxed(dev->phy + (i + 16)),
			readl_relaxed(dev->phy + (i + 20)),
			readl_relaxed(dev->phy + (i + 24)),
			readl_relaxed(dev->phy + (i + 28)));
	}
}

static inline int msm_pcie_check_align(struct msm_pcie_dev_t *dev,
						u32 offset)
{
	if (offset % 4) {
		PCIE_ERR(dev,
			"PCIe: RC%d: offset 0x%x is not correctly aligned\n",
			dev->rc_idx, offset);
		return MSM_PCIE_ERROR;
	}

	return 0;
}

static bool msm_pcie_confirm_linkup(struct msm_pcie_dev_t *dev,
						bool check_sw_stts,
						bool check_ep,
						void __iomem *ep_conf)
{
	u32 val;

	if (check_sw_stts && (dev->link_status != MSM_PCIE_LINK_ENABLED)) {
		PCIE_DBG(dev, "PCIe: The link of RC %d is not enabled.\n",
			dev->rc_idx);
		return false;
	}

	if (!(readl_relaxed(dev->dm_core + 0x80) & BIT(29))) {
		PCIE_DBG(dev, "PCIe: The link of RC %d is not up.\n",
			dev->rc_idx);
		return false;
	}

	val = readl_relaxed(dev->dm_core);
	PCIE_DBG(dev, "PCIe: device ID and vender ID of RC %d are 0x%x.\n",
		dev->rc_idx, val);
	if (val == PCIE_LINK_DOWN) {
		PCIE_ERR(dev,
			"PCIe: The link of RC %d is not really up; device ID and vender ID of RC %d are 0x%x.\n",
			dev->rc_idx, dev->rc_idx, val);
		return false;
	}

	if (check_ep) {
		val = readl_relaxed(ep_conf);
		PCIE_DBG(dev,
			"PCIe: device ID and vender ID of EP of RC %d are 0x%x.\n",
			dev->rc_idx, val);
		if (val == PCIE_LINK_DOWN) {
			PCIE_ERR(dev,
				"PCIe: The link of RC %d is not really up; device ID and vender ID of EP of RC %d are 0x%x.\n",
				dev->rc_idx, dev->rc_idx, val);
			return false;
		}
	}

	return true;
}

static void msm_pcie_cfg_recover(struct msm_pcie_dev_t *dev, bool rc)
{
	int i, j;
	u32 val = 0;
	u32 *shadow;
	void __iomem *cfg = dev->conf;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (!rc && !dev->pcidev_table[i].bdf)
			break;
		if (rc) {
			cfg = dev->dm_core;
			shadow = dev->rc_shadow;
		} else {
			if (!msm_pcie_confirm_linkup(dev, false, true,
				dev->pcidev_table[i].conf_base))
				continue;

			shadow = dev->ep_shadow[i];
			PCIE_DBG(dev,
				"PCIe Device: %02x:%02x.%01x\n",
				dev->pcidev_table[i].bdf >> 24,
				dev->pcidev_table[i].bdf >> 19 & 0x1f,
				dev->pcidev_table[i].bdf >> 16 & 0x07);
		}
		for (j = PCIE_CONF_SPACE_DW - 1; j >= 0; j--) {
			val = shadow[j];
			if (val != PCIE_CLEAR) {
				PCIE_DBG3(dev,
					"PCIe: before recovery:cfg 0x%x:0x%x\n",
					j * 4, readl_relaxed(cfg + j * 4));
				PCIE_DBG3(dev,
					"PCIe: shadow_dw[%d]:cfg 0x%x:0x%x\n",
					j, j * 4, val);
				writel_relaxed(val, cfg + j * 4);
				/* ensure changes propagated to the hardware */
				wmb();
				PCIE_DBG3(dev,
					"PCIe: after recovery:cfg 0x%x:0x%x\n\n",
					j * 4, readl_relaxed(cfg + j * 4));
			}
		}
		if (rc)
			break;

		pci_save_state(dev->pcidev_table[i].dev);
		cfg += SZ_4K;
	}
}

static void msm_pcie_write_mask(void __iomem *addr,
				uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t val;

	val = (readl_relaxed(addr) & ~clear_mask) | set_mask;
	writel_relaxed(val, addr);
	wmb();  /* ensure data is written to hardware register */
}

static void pcie_parf_dump(struct msm_pcie_dev_t *dev)
{
	int i, size;
	u32 original;

	PCIE_DUMP(dev, "PCIe: RC%d PARF testbus\n", dev->rc_idx);

	original = readl_relaxed(dev->parf + PCIE20_PARF_SYS_CTRL);
	for (i = 1; i <= 0x1A; i++) {
		msm_pcie_write_mask(dev->parf + PCIE20_PARF_SYS_CTRL,
				0xFF0000, i << 16);
		PCIE_DUMP(dev,
			"RC%d: PARF_SYS_CTRL: 0%08x PARF_TEST_BUS: 0%08x\n",
			dev->rc_idx,
			readl_relaxed(dev->parf + PCIE20_PARF_SYS_CTRL),
			readl_relaxed(dev->parf + PCIE20_PARF_TEST_BUS));
	}
	writel_relaxed(original, dev->parf + PCIE20_PARF_SYS_CTRL);

	PCIE_DUMP(dev, "PCIe: RC%d PARF register dump\n", dev->rc_idx);

	size = resource_size(dev->res[MSM_PCIE_RES_PARF].resource);
	for (i = 0; i < size; i += 32) {
		PCIE_DUMP(dev,
			"RC%d: 0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
			dev->rc_idx, i,
			readl_relaxed(dev->parf + i),
			readl_relaxed(dev->parf + (i + 4)),
			readl_relaxed(dev->parf + (i + 8)),
			readl_relaxed(dev->parf + (i + 12)),
			readl_relaxed(dev->parf + (i + 16)),
			readl_relaxed(dev->parf + (i + 20)),
			readl_relaxed(dev->parf + (i + 24)),
			readl_relaxed(dev->parf + (i + 28)));
	}
}

static void msm_pcie_show_status(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG_FS(dev, "PCIe: RC%d is %s enumerated\n",
		dev->rc_idx, dev->enumerated ? "" : "not");
	PCIE_DBG_FS(dev, "PCIe: link is %s\n",
		(dev->link_status == MSM_PCIE_LINK_ENABLED)
		? "enabled" : "disabled");
	PCIE_DBG_FS(dev, "cfg_access is %s allowed\n",
		dev->cfg_access ? "" : "not");
	PCIE_DBG_FS(dev, "use_pinctrl is %d\n",
		dev->use_pinctrl);
	PCIE_DBG_FS(dev, "use_19p2mhz_aux_clk is %d\n",
		dev->use_19p2mhz_aux_clk);
	PCIE_DBG_FS(dev, "user_suspend is %d\n",
		dev->user_suspend);
	PCIE_DBG_FS(dev, "num_ep: %d\n",
		dev->num_ep);
	PCIE_DBG_FS(dev, "num_active_ep: %d\n",
		dev->num_active_ep);
	PCIE_DBG_FS(dev, "pending_ep_reg: %s\n",
		dev->pending_ep_reg ? "true" : "false");
	PCIE_DBG_FS(dev, "phy_len is %d",
		dev->phy_len);
	PCIE_DBG_FS(dev, "disable_pc is %d",
		dev->disable_pc);
	PCIE_DBG_FS(dev, "l0s_supported is %s supported\n",
		dev->l0s_supported ? "" : "not");
	PCIE_DBG_FS(dev, "l1_supported is %s supported\n",
		dev->l1_supported ? "" : "not");
	PCIE_DBG_FS(dev, "l1ss_supported is %s supported\n",
		dev->l1ss_supported ? "" : "not");
	PCIE_DBG_FS(dev, "l1_1_pcipm_supported is %s supported\n",
		dev->l1_1_pcipm_supported ? "" : "not");
	PCIE_DBG_FS(dev, "l1_2_pcipm_supported is %s supported\n",
		dev->l1_2_pcipm_supported ? "" : "not");
	PCIE_DBG_FS(dev, "l1_1_aspm_supported is %s supported\n",
		dev->l1_1_aspm_supported ? "" : "not");
	PCIE_DBG_FS(dev, "l1_2_aspm_supported is %s supported\n",
		dev->l1_2_aspm_supported ? "" : "not");
	PCIE_DBG_FS(dev, "l1_2_th_scale is %d\n",
		dev->l1_2_th_scale);
	PCIE_DBG_FS(dev, "l1_2_th_value is %d\n",
		dev->l1_2_th_value);
	PCIE_DBG_FS(dev, "common_clk_en is %d\n",
		dev->common_clk_en);
	PCIE_DBG_FS(dev, "clk_power_manage_en is %d\n",
		dev->clk_power_manage_en);
	PCIE_DBG_FS(dev, "aux_clk_sync is %d\n",
		dev->aux_clk_sync);
	PCIE_DBG_FS(dev, "AER is %s enable\n",
		dev->aer_enable ? "" : "not");
	PCIE_DBG_FS(dev, "boot_option is 0x%x\n",
		dev->boot_option);
	PCIE_DBG_FS(dev, "phy_ver is %d\n",
		dev->phy_ver);
	PCIE_DBG_FS(dev, "drv_ready is %d\n",
		dev->drv_ready);
	PCIE_DBG_FS(dev, "linkdown_panic is %d\n",
		dev->linkdown_panic);
	PCIE_DBG_FS(dev, "the link is %s suspending\n",
		dev->suspending ? "" : "not");
	PCIE_DBG_FS(dev, "shadow is %s enabled\n",
		dev->shadow_en ? "" : "not");
	PCIE_DBG_FS(dev, "the power of RC is %s on\n",
		dev->power_on ? "" : "not");
	PCIE_DBG_FS(dev, "bus_client: %d\n",
		dev->bus_client);
	PCIE_DBG_FS(dev, "smmu_sid_base: 0x%x\n",
		dev->smmu_sid_base);
	PCIE_DBG_FS(dev, "n_fts: %d\n",
		dev->n_fts);
	PCIE_DBG_FS(dev, "ep_latency: %dms\n",
		dev->ep_latency);
	PCIE_DBG_FS(dev, "switch_latency: %dms\n",
		dev->switch_latency);
	PCIE_DBG_FS(dev, "wr_halt_size: 0x%x\n",
		dev->wr_halt_size);
	PCIE_DBG_FS(dev, "slv_addr_space_size: 0x%x\n",
		dev->slv_addr_space_size);
	PCIE_DBG_FS(dev, "phy_status_offset: 0x%x\n",
		dev->phy_status_offset);
	PCIE_DBG_FS(dev, "phy_status_bit: %u\n",
		dev->phy_status_bit);
	PCIE_DBG_FS(dev, "phy_power_down_offset: 0x%x\n",
		dev->phy_power_down_offset);
	PCIE_DBG_FS(dev, "core_preset: 0x%x\n",
		dev->core_preset);
	PCIE_DBG_FS(dev, "cpl_timeout: 0x%x\n",
		dev->cpl_timeout);
	PCIE_DBG_FS(dev, "current_bdf: 0x%x\n",
		dev->current_bdf);
	PCIE_DBG_FS(dev, "perst_delay_us_min: %dus\n",
		dev->perst_delay_us_min);
	PCIE_DBG_FS(dev, "perst_delay_us_max: %dus\n",
		dev->perst_delay_us_max);
	PCIE_DBG_FS(dev, "tlp_rd_size: 0x%x\n",
		dev->tlp_rd_size);
	PCIE_DBG_FS(dev, "rc_corr_counter: %lu\n",
		dev->rc_corr_counter);
	PCIE_DBG_FS(dev, "rc_non_fatal_counter: %lu\n",
		dev->rc_non_fatal_counter);
	PCIE_DBG_FS(dev, "rc_fatal_counter: %lu\n",
		dev->rc_fatal_counter);
	PCIE_DBG_FS(dev, "ep_corr_counter: %lu\n",
		dev->ep_corr_counter);
	PCIE_DBG_FS(dev, "ep_non_fatal_counter: %lu\n",
		dev->ep_non_fatal_counter);
	PCIE_DBG_FS(dev, "ep_fatal_counter: %lu\n",
		dev->ep_fatal_counter);
	PCIE_DBG_FS(dev, "linkdown_counter: %lu\n",
		dev->linkdown_counter);
	PCIE_DBG_FS(dev, "wake_counter: %lu\n",
		dev->wake_counter);
	PCIE_DBG_FS(dev, "link_check_max_count: %u\n",
		dev->link_check_max_count);
	PCIE_DBG_FS(dev, "prevent_l1: %d\n",
		dev->prevent_l1);
	PCIE_DBG_FS(dev, "target_link_speed: 0x%x\n",
		dev->target_link_speed);
	PCIE_DBG_FS(dev, "link_turned_on_counter: %lu\n",
		dev->link_turned_on_counter);
	PCIE_DBG_FS(dev, "link_turned_off_counter: %lu\n",
		dev->link_turned_off_counter);
}

static void msm_pcie_shadow_dump(struct msm_pcie_dev_t *dev, bool rc)
{
	int i, j;
	u32 val = 0;
	u32 *shadow;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (!rc && !dev->pcidev_table[i].bdf)
			break;
		if (rc) {
			shadow = dev->rc_shadow;
		} else {
			shadow = dev->ep_shadow[i];
			PCIE_DBG_FS(dev, "PCIe Device: %02x:%02x.%01x\n",
				dev->pcidev_table[i].bdf >> 24,
				dev->pcidev_table[i].bdf >> 19 & 0x1f,
				dev->pcidev_table[i].bdf >> 16 & 0x07);
		}
		for (j = 0; j < PCIE_CONF_SPACE_DW; j++) {
			val = shadow[j];
			if (val != PCIE_CLEAR) {
				PCIE_DBG_FS(dev,
					"PCIe: shadow_dw[%d]:cfg 0x%x:0x%x\n",
					j, j * 4, val);
			}
		}
		if (rc)
			break;
	}
}

static void msm_pcie_sel_debug_testcase(struct msm_pcie_dev_t *dev,
					u32 testcase)
{
	u32 dbi_base_addr = dev->res[MSM_PCIE_RES_DM_CORE].resource->start;
	phys_addr_t loopback_lbar_phy =
		dev->res[MSM_PCIE_RES_DM_CORE].resource->start +
		LOOPBACK_BASE_ADDR_OFFSET;
	static uint32_t loopback_val = 0x1;
	static dma_addr_t loopback_ddr_phy;
	static uint32_t *loopback_ddr_vir;
	static void __iomem *loopback_lbar_vir;
	int ret, i;
	u32 base_sel_size = 0;
	u32 wr_ofst = 0;

	switch (testcase) {
	case MSM_PCIE_OUTPUT_PCIE_INFO:
		PCIE_DBG_FS(dev, "\n\nPCIe: Status for RC%d:\n",
			dev->rc_idx);
		msm_pcie_show_status(dev);
		break;
	case MSM_PCIE_DISABLE_LINK:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: disable link\n\n", dev->rc_idx);
		ret = msm_pcie_pm_control(MSM_PCIE_SUSPEND, 0,
			dev->dev, NULL,
			MSM_PCIE_CONFIG_NO_CFG_RESTORE);
		if (ret)
			PCIE_DBG_FS(dev, "PCIe:%s:failed to disable link\n",
				__func__);
		else
			PCIE_DBG_FS(dev, "PCIe:%s:disabled link\n",
				__func__);
		break;
	case MSM_PCIE_ENABLE_LINK:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: enable link and recover config space\n\n",
			dev->rc_idx);
		ret = msm_pcie_pm_control(MSM_PCIE_RESUME, 0,
			dev->dev, NULL,
			MSM_PCIE_CONFIG_NO_CFG_RESTORE);
		if (ret)
			PCIE_DBG_FS(dev, "PCIe:%s:failed to enable link\n",
				__func__);
		else {
			PCIE_DBG_FS(dev, "PCIe:%s:enabled link\n", __func__);
			msm_pcie_recover_config(dev->dev);
		}
		break;
	case MSM_PCIE_DISABLE_ENABLE_LINK:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: disable and enable link then recover config space\n\n",
			dev->rc_idx);
		ret = msm_pcie_pm_control(MSM_PCIE_SUSPEND, 0,
			dev->dev, NULL,
			MSM_PCIE_CONFIG_NO_CFG_RESTORE);
		if (ret)
			PCIE_DBG_FS(dev, "PCIe:%s:failed to disable link\n",
				__func__);
		else
			PCIE_DBG_FS(dev, "PCIe:%s:disabled link\n", __func__);
		ret = msm_pcie_pm_control(MSM_PCIE_RESUME, 0,
			dev->dev, NULL,
			MSM_PCIE_CONFIG_NO_CFG_RESTORE);
		if (ret)
			PCIE_DBG_FS(dev, "PCIe:%s:failed to enable link\n",
				__func__);
		else {
			PCIE_DBG_FS(dev, "PCIe:%s:enabled link\n", __func__);
			msm_pcie_recover_config(dev->dev);
		}
		break;
	case MSM_PCIE_DUMP_SHADOW_REGISTER:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: dumping RC shadow registers\n",
			dev->rc_idx);
		msm_pcie_shadow_dump(dev, true);

		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: dumping EP shadow registers\n",
			dev->rc_idx);
		msm_pcie_shadow_dump(dev, false);
		break;
	case MSM_PCIE_DISABLE_L0S:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: disable L0s\n\n",
			dev->rc_idx);
		if (dev->link_status == MSM_PCIE_LINK_ENABLED)
			msm_pcie_config_l0s_disable_all(dev, dev->dev->bus);
		dev->l0s_supported = false;
		break;
	case MSM_PCIE_ENABLE_L0S:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: enable L0s\n\n",
			dev->rc_idx);
		dev->l0s_supported = true;
		if (dev->link_status == MSM_PCIE_LINK_ENABLED)
			msm_pcie_config_l0s_enable_all(dev);
		break;
	case MSM_PCIE_DISABLE_L1:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: disable L1\n\n",
			dev->rc_idx);
		if (dev->link_status == MSM_PCIE_LINK_ENABLED)
			msm_pcie_config_l1_disable_all(dev, dev->dev->bus);
		dev->l1_supported = false;
		break;
	case MSM_PCIE_ENABLE_L1:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: enable L1\n\n",
			dev->rc_idx);
		dev->l1_supported = true;
		if (dev->link_status == MSM_PCIE_LINK_ENABLED) {
			/* enable l1 mode, clear bit 5 (REQ_NOT_ENTR_L1) */
			msm_pcie_write_mask(dev->parf +
				PCIE20_PARF_PM_CTRL, BIT(5), 0);

			msm_pcie_config_l1_enable_all(dev);
		}
		break;
	case MSM_PCIE_DISABLE_L1SS:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: disable L1ss\n\n",
			dev->rc_idx);
		if (dev->link_status == MSM_PCIE_LINK_ENABLED)
			msm_pcie_config_l1ss_disable_all(dev, dev->dev->bus);
		dev->l1ss_supported = false;
		dev->l1_1_pcipm_supported = false;
		dev->l1_2_pcipm_supported = false;
		dev->l1_1_aspm_supported = false;
		dev->l1_2_aspm_supported = false;
		break;
	case MSM_PCIE_ENABLE_L1SS:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: enable L1ss\n\n",
			dev->rc_idx);
		dev->l1ss_supported = true;
		dev->l1_1_pcipm_supported = true;
		dev->l1_2_pcipm_supported = true;
		dev->l1_1_aspm_supported = true;
		dev->l1_2_aspm_supported = true;
		if (dev->link_status == MSM_PCIE_LINK_ENABLED) {
			msm_pcie_check_l1ss_support_all(dev);
			msm_pcie_config_l1ss_enable_all(dev);
		}
		break;
	case MSM_PCIE_ENUMERATION:
		PCIE_DBG_FS(dev, "\n\nPCIe: attempting to enumerate RC%d\n\n",
			dev->rc_idx);
		if (dev->enumerated)
			PCIE_DBG_FS(dev, "PCIe: RC%d is already enumerated\n",
				dev->rc_idx);
		else {
			if (!msm_pcie_enumerate(dev->rc_idx))
				PCIE_DBG_FS(dev,
					"PCIe: RC%d is successfully enumerated\n",
					dev->rc_idx);
			else
				PCIE_DBG_FS(dev,
					"PCIe: RC%d enumeration failed\n",
					dev->rc_idx);
		}
		break;
	case MSM_PCIE_READ_PCIE_REGISTER:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: read a PCIe register\n\n",
			dev->rc_idx);
		if (!base_sel) {
			PCIE_DBG_FS(dev, "Invalid base_sel: 0x%x\n", base_sel);
			break;
		}

		PCIE_DBG_FS(dev, "base: %s: 0x%pK\nwr_offset: 0x%x\n",
			dev->res[base_sel - 1].name,
			dev->res[base_sel - 1].base,
			wr_offset);

		base_sel_size = resource_size(dev->res[base_sel - 1].resource);

		if (wr_offset >  base_sel_size - 4 ||
			msm_pcie_check_align(dev, wr_offset)) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: Invalid wr_offset: 0x%x. wr_offset should be no more than 0x%x\n",
				dev->rc_idx, wr_offset, base_sel_size - 4);
		} else {
			phys_addr_t wr_register =
				dev->res[MSM_PCIE_RES_DM_CORE].resource->start;

			wr_register += wr_offset;
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: register: 0x%pa value: 0x%x\n",
				dev->rc_idx, &wr_register,
				readl_relaxed(dev->res[base_sel - 1].base +
					wr_offset));
		}

		break;
	case MSM_PCIE_WRITE_PCIE_REGISTER:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: writing a value to a register\n\n",
			dev->rc_idx);

		if (!base_sel) {
			PCIE_DBG_FS(dev, "Invalid base_sel: 0x%x\n", base_sel);
			break;
		}

		if (((base_sel - 1) >= MSM_PCIE_MAX_RES) ||
					(!dev->res[base_sel - 1].resource)) {
			PCIE_DBG_FS(dev, "PCIe: RC%d Resource does not exist\n",
								dev->rc_idx);
			break;
		}

		wr_ofst = wr_offset;

		PCIE_DBG_FS(dev,
			"base: %s: 0x%pK\nwr_offset: 0x%x\nwr_mask: 0x%x\nwr_value: 0x%x\n",
			dev->res[base_sel - 1].name,
			dev->res[base_sel - 1].base,
			wr_ofst, wr_mask, wr_value);

		base_sel_size = resource_size(dev->res[base_sel - 1].resource);

		if (wr_ofst >  base_sel_size - 4 ||
			msm_pcie_check_align(dev, wr_ofst))
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: Invalid wr_offset: 0x%x. wr_offset should be no more than 0x%x\n",
				dev->rc_idx, wr_ofst, base_sel_size - 4);
		else
			msm_pcie_write_reg_field(dev->res[base_sel - 1].base,
				wr_ofst, wr_mask, wr_value);

		break;
	case MSM_PCIE_DUMP_PCIE_REGISTER_SPACE:
		if (((base_sel - 1) >= MSM_PCIE_MAX_RES) ||
					(!dev->res[base_sel - 1].resource)) {
			PCIE_DBG_FS(dev, "PCIe: RC%d Resource does not exist\n",
								dev->rc_idx);
			break;
		}

		if (!base_sel) {
			PCIE_DBG_FS(dev, "Invalid base_sel: 0x%x\n", base_sel);
			break;
		} else if (base_sel - 1 == MSM_PCIE_RES_PARF) {
			pcie_parf_dump(dev);
			break;
		} else if (base_sel - 1 == MSM_PCIE_RES_PHY) {
			pcie_phy_dump(dev);
			break;
		} else if (base_sel - 1 == MSM_PCIE_RES_CONF) {
			base_sel_size = 0x1000;
		} else {
			base_sel_size = resource_size(
				dev->res[base_sel - 1].resource);
		}

		PCIE_DBG_FS(dev, "\n\nPCIe: Dumping %s Registers for RC%d\n\n",
			dev->res[base_sel - 1].name, dev->rc_idx);

		for (i = 0; i < base_sel_size; i += 32) {
			PCIE_DBG_FS(dev,
			"0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
			i, readl_relaxed(dev->res[base_sel - 1].base + i),
			readl_relaxed(dev->res[base_sel - 1].base + (i + 4)),
			readl_relaxed(dev->res[base_sel - 1].base + (i + 8)),
			readl_relaxed(dev->res[base_sel - 1].base + (i + 12)),
			readl_relaxed(dev->res[base_sel - 1].base + (i + 16)),
			readl_relaxed(dev->res[base_sel - 1].base + (i + 20)),
			readl_relaxed(dev->res[base_sel - 1].base + (i + 24)),
			readl_relaxed(dev->res[base_sel - 1].base + (i + 28)));
		}
		break;
	case MSM_PCIE_ALLOCATE_DDR_MAP_LBAR:
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: Allocate 4K DDR memory and map LBAR.\n",
			dev->rc_idx);
		loopback_ddr_vir = dma_alloc_coherent(&dev->pdev->dev,
			(SZ_1K * sizeof(*loopback_ddr_vir)),
			&loopback_ddr_phy, GFP_KERNEL);
		if (!loopback_ddr_vir) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: failed to dma_alloc_coherent.\n",
				dev->rc_idx);
		} else {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: VIR DDR memory address: 0x%pK\n",
				dev->rc_idx, loopback_ddr_vir);
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: PHY DDR memory address: %pad\n",
				dev->rc_idx, &loopback_ddr_phy);
		}

		PCIE_DBG_FS(dev, "PCIe: RC%d: map LBAR: %pa\n",
			dev->rc_idx, &loopback_lbar_phy);
		loopback_lbar_vir = devm_ioremap(&dev->pdev->dev,
			loopback_lbar_phy, SZ_4K);
		if (!loopback_lbar_vir) {
			PCIE_DBG_FS(dev, "PCIe: RC%d: failed to map %pa\n",
				dev->rc_idx, &loopback_lbar_phy);
		} else {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: successfully mapped %pa to 0x%pK\n",
				dev->rc_idx, &loopback_lbar_phy,
				loopback_lbar_vir);
		}
		break;
	case MSM_PCIE_FREE_DDR_UNMAP_LBAR:
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: Release 4K DDR memory and unmap LBAR.\n",
			dev->rc_idx);

		if (loopback_ddr_vir) {
			dma_free_coherent(&dev->pdev->dev, SZ_4K,
				loopback_ddr_vir, loopback_ddr_phy);
			loopback_ddr_vir = NULL;
		}

		if (loopback_lbar_vir) {
			devm_iounmap(&dev->pdev->dev,
				loopback_lbar_vir);
			loopback_lbar_vir = NULL;
		}
		break;
	case MSM_PCIE_OUTPUT_DDR_LBAR_ADDRESS:
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: Print DDR and LBAR addresses.\n",
			dev->rc_idx);

		if (!loopback_ddr_vir || !loopback_lbar_vir) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: DDR or LBAR address is not mapped\n",
				dev->rc_idx);
			break;
		}

		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PHY DDR address: %pad\n",
			dev->rc_idx, &loopback_ddr_phy);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: VIR DDR address: 0x%pK\n",
			dev->rc_idx, loopback_ddr_vir);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PHY LBAR address: %pa\n",
			dev->rc_idx, &loopback_lbar_phy);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: VIR LBAR address: 0x%pK\n",
			dev->rc_idx, loopback_lbar_vir);
		break;
	case MSM_PCIE_CONFIGURE_LOOPBACK:
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: Configure Loopback.\n",
			dev->rc_idx);

		writel_relaxed(0x10000,
			dev->dm_core + PCIE_GEN3_RELATED);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: 0x%x: 0x%x\n",
			dev->rc_idx,
			dbi_base_addr + PCIE_GEN3_RELATED,
			readl_relaxed(dev->dm_core +
				PCIE_GEN3_RELATED));

		writel_relaxed(0x80000001,
			dev->dm_core + PCIE20_PIPE_LOOPBACK_CONTROL);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: 0x%x: 0x%x\n",
			dev->rc_idx,
			dbi_base_addr + PCIE20_PIPE_LOOPBACK_CONTROL,
			readl_relaxed(dev->dm_core +
				PCIE20_PIPE_LOOPBACK_CONTROL));

		writel_relaxed(0x00010124,
			dev->dm_core + PCIE20_PORT_LINK_CTRL_REG);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: 0x%x: 0x%x\n",
			dev->rc_idx,
			dbi_base_addr + PCIE20_PORT_LINK_CTRL_REG,
			readl_relaxed(dev->dm_core +
				PCIE20_PORT_LINK_CTRL_REG));
		break;
	case MSM_PCIE_SETUP_LOOPBACK_IATU:
	{
		void __iomem *iatu_base_vir;
		u32 iatu_base_phy;
		u32 iatu_viewport_offset;
		u32 iatu_ctrl1_offset;
		u32 iatu_ctrl2_offset;
		u32 iatu_lbar_offset;
		u32 iatu_ubar_offset;
		u32 iatu_lar_offset;
		u32 iatu_ltar_offset;
		u32 iatu_utar_offset;
		u32 iatu_n = 1;

		if (dev->iatu) {
			iatu_base_vir = dev->iatu;
			iatu_base_phy =
				dev->res[MSM_PCIE_RES_IATU].resource->start;

			iatu_viewport_offset = 0;
			iatu_ctrl1_offset = PCIE_IATU_CTRL1(iatu_n);
			iatu_ctrl2_offset = PCIE_IATU_CTRL2(iatu_n);
			iatu_lbar_offset = PCIE_IATU_LBAR(iatu_n);
			iatu_ubar_offset = PCIE_IATU_UBAR(iatu_n);
			iatu_lar_offset = PCIE_IATU_LAR(iatu_n);
			iatu_ltar_offset = PCIE_IATU_LTAR(iatu_n);
			iatu_utar_offset = PCIE_IATU_UTAR(iatu_n);
		} else {
			iatu_base_vir = dev->dm_core;
			iatu_base_phy = dbi_base_addr;

			iatu_viewport_offset = PCIE20_PLR_IATU_VIEWPORT;
			iatu_ctrl1_offset = PCIE20_PLR_IATU_CTRL1;
			iatu_ctrl2_offset = PCIE20_PLR_IATU_CTRL2;
			iatu_lbar_offset = PCIE20_PLR_IATU_LBAR;
			iatu_ubar_offset = PCIE20_PLR_IATU_UBAR;
			iatu_lar_offset = PCIE20_PLR_IATU_LAR;
			iatu_ltar_offset = PCIE20_PLR_IATU_LTAR;
			iatu_utar_offset = PCIE20_PLR_IATU_UTAR;
		}

		PCIE_DBG_FS(dev, "PCIe: RC%d: Setup iATU.\n", dev->rc_idx);

		if (!loopback_ddr_vir) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: DDR address is not mapped.\n",
				dev->rc_idx);
			break;
		}

		if (iatu_viewport_offset) {
			writel_relaxed(0x0, iatu_base_vir +
				iatu_viewport_offset);
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: PCIE20_PLR_IATU_VIEWPORT:\t0x%x: 0x%x\n",
				dev->rc_idx,
				iatu_base_phy + iatu_viewport_offset,
				readl_relaxed(iatu_base_vir +
					iatu_viewport_offset));
		}

		writel_relaxed(0x0, iatu_base_vir + iatu_ctrl1_offset);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PCIE20_PLR_IATU_CTRL1:\t0x%x: 0x%x\n",
			dev->rc_idx, iatu_base_phy + iatu_ctrl1_offset,
			readl_relaxed(iatu_base_vir + iatu_ctrl1_offset));

		writel_relaxed(loopback_lbar_phy,
			iatu_base_vir + iatu_lbar_offset);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PCIE20_PLR_IATU_LBAR:\t0x%x: 0x%x\n",
			dev->rc_idx, iatu_base_phy + iatu_lbar_offset,
			readl_relaxed(iatu_base_vir + iatu_lbar_offset));

		writel_relaxed(0x0, iatu_base_vir + iatu_ubar_offset);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PCIE20_PLR_IATU_UBAR:\t0x%x: 0x%x\n",
			dev->rc_idx, iatu_base_phy + iatu_ubar_offset,
			readl_relaxed(iatu_base_vir + iatu_ubar_offset));

		writel_relaxed(loopback_lbar_phy + 0xfff,
			iatu_base_vir + iatu_lar_offset);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PCIE20_PLR_IATU_LAR:\t0x%x: 0x%x\n",
			dev->rc_idx, iatu_base_phy + iatu_lar_offset,
			readl_relaxed(iatu_base_vir + iatu_lar_offset));

		writel_relaxed(loopback_ddr_phy,
			iatu_base_vir + iatu_ltar_offset);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PCIE20_PLR_IATU_LTAR:\t0x%x: 0x%x\n",
			dev->rc_idx, iatu_base_phy + iatu_ltar_offset,
			readl_relaxed(iatu_base_vir + iatu_ltar_offset));

		writel_relaxed(0, iatu_base_vir + iatu_utar_offset);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PCIE20_PLR_IATU_UTAR:\t0x%x: 0x%x\n",
			dev->rc_idx, iatu_base_phy + iatu_utar_offset,
			readl_relaxed(iatu_base_vir + iatu_utar_offset));

		writel_relaxed(0x80000000,
			iatu_base_vir + iatu_ctrl2_offset);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PCIE20_PLR_IATU_CTRL2:\t0x%x: 0x%x\n",
			dev->rc_idx, iatu_base_phy + iatu_ctrl2_offset,
			readl_relaxed(iatu_base_vir + iatu_ctrl2_offset));
		break;
	}
	case MSM_PCIE_READ_DDR:
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: Read DDR values.\n",
			dev->rc_idx);

		if (!loopback_ddr_vir) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: DDR is not mapped\n",
				dev->rc_idx);
			break;
		}

		for (i = 0; i < SZ_1K; i += 8) {
			PCIE_DBG_FS(dev,
				"0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
				i,
				loopback_ddr_vir[i],
				loopback_ddr_vir[i + 1],
				loopback_ddr_vir[i + 2],
				loopback_ddr_vir[i + 3],
				loopback_ddr_vir[i + 4],
				loopback_ddr_vir[i + 5],
				loopback_ddr_vir[i + 6],
				loopback_ddr_vir[i + 7]);
		}
		break;
	case MSM_PCIE_READ_LBAR:
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: Read LBAR values.\n",
			dev->rc_idx);

		if (!loopback_lbar_vir) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: LBAR address is not mapped\n",
				dev->rc_idx);
			break;
		}

		for (i = 0; i < SZ_4K; i += 32) {
			PCIE_DBG_FS(dev,
				"0x%04x %08x %08x %08x %08x %08x %08x %08x %08x\n",
				i,
				readl_relaxed(loopback_lbar_vir + i),
				readl_relaxed(loopback_lbar_vir + (i + 4)),
				readl_relaxed(loopback_lbar_vir + (i + 8)),
				readl_relaxed(loopback_lbar_vir + (i + 12)),
				readl_relaxed(loopback_lbar_vir + (i + 16)),
				readl_relaxed(loopback_lbar_vir + (i + 20)),
				readl_relaxed(loopback_lbar_vir + (i + 24)),
				readl_relaxed(loopback_lbar_vir + (i + 28)));
		}
		break;
	case MSM_PCIE_WRITE_DDR:
		PCIE_DBG_FS(dev, "PCIe: RC%d: Write 0x%x to DDR.\n",
			dev->rc_idx, loopback_val);

		if (!loopback_ddr_vir) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: DDR address is not mapped\n",
				dev->rc_idx);
			break;
		}

		memset(loopback_ddr_vir, loopback_val,
			(SZ_1K * sizeof(*loopback_ddr_vir)));

		if (unlikely(loopback_val == UINT_MAX))
			loopback_val = 1;
		else
			loopback_val++;
		break;
	case MSM_PCIE_WRITE_LBAR:
		PCIE_DBG_FS(dev, "PCIe: RC%d: Write 0x%x to LBAR.\n",
			dev->rc_idx, loopback_val);

		if (!loopback_lbar_vir) {
			PCIE_DBG_FS(dev,
				"PCIe: RC%d: LBAR address is not mapped\n",
				dev->rc_idx);
			break;
		}

		for (i = 0; i < SZ_4K; i += 32) {
			writel_relaxed(loopback_val,
				loopback_lbar_vir + i),
			writel_relaxed(loopback_val,
				loopback_lbar_vir + (i + 4)),
			writel_relaxed(loopback_val,
				loopback_lbar_vir + (i + 8)),
			writel_relaxed(loopback_val,
				loopback_lbar_vir + (i + 12)),
			writel_relaxed(loopback_val,
				loopback_lbar_vir + (i + 16)),
			writel_relaxed(loopback_val,
				loopback_lbar_vir + (i + 20)),
			writel_relaxed(loopback_val,
				loopback_lbar_vir + (i + 24)),
			writel_relaxed(loopback_val,
				loopback_lbar_vir + (i + 28));
		}

		if (unlikely(loopback_val == UINT_MAX))
			loopback_val = 1;
		else
			loopback_val++;
		break;
	case MSM_PCIE_DISABLE_AER:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: clear AER enable flag\n\n",
			dev->rc_idx);
		dev->aer_enable = false;
		break;
	case MSM_PCIE_ENABLE_AER:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: set AER enable flag\n\n",
			dev->rc_idx);
		dev->aer_enable = true;
		break;
	case MSM_PCIE_GPIO_STATUS:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: PERST and WAKE status\n\n",
			dev->rc_idx);
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: PERST: gpio%u value: %d\n",
			dev->rc_idx, dev->gpio[MSM_PCIE_GPIO_PERST].num,
			gpio_get_value(dev->gpio[MSM_PCIE_GPIO_PERST].num));
		PCIE_DBG_FS(dev,
			"PCIe: RC%d: WAKE: gpio%u value: %d\n",
			dev->rc_idx, dev->gpio[MSM_PCIE_GPIO_WAKE].num,
			gpio_get_value(dev->gpio[MSM_PCIE_GPIO_WAKE].num));
		break;
	case MSM_PCIE_ASSERT_PERST:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: assert PERST\n\n",
			dev->rc_idx);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
					dev->gpio[MSM_PCIE_GPIO_PERST].on);
		usleep_range(dev->perst_delay_us_min, dev->perst_delay_us_max);
		break;
	case MSM_PCIE_DEASSERT_PERST:
		PCIE_DBG_FS(dev, "\n\nPCIe: RC%d: de-assert PERST\n\n",
			dev->rc_idx);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
					1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
		usleep_range(dev->perst_delay_us_min, dev->perst_delay_us_max);
		break;
	case MSM_PCIE_KEEP_RESOURCES_ON:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: set keep resources on flag\n\n",
			dev->rc_idx);
		msm_pcie_keep_resources_on |= BIT(dev->rc_idx);
		break;
	case MSM_PCIE_FORCE_GEN1:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: set target speed to Gen 1\n\n",
			dev->rc_idx);
		dev->target_link_speed = GEN1_SPEED;
		break;
	case MSM_PCIE_FORCE_GEN2:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: set target speed to Gen 2\n\n",
			dev->rc_idx);
		dev->target_link_speed = GEN2_SPEED;
		break;
	case MSM_PCIE_FORCE_GEN3:
		PCIE_DBG_FS(dev,
			"\n\nPCIe: RC%d: set target speed to Gen 3\n\n",
			dev->rc_idx);
		dev->target_link_speed = GEN3_SPEED;
		break;
	default:
		PCIE_DBG_FS(dev, "Invalid testcase: %d.\n", testcase);
		break;
	}
}

int msm_pcie_debug_info(struct pci_dev *dev, u32 option, u32 base,
			u32 offset, u32 mask, u32 value)
{
	int ret = 0;
	struct msm_pcie_dev_t *pdev = NULL;

	if (!dev) {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	if (option == MSM_PCIE_READ_PCIE_REGISTER ||
		option == MSM_PCIE_WRITE_PCIE_REGISTER ||
		option == MSM_PCIE_DUMP_PCIE_REGISTER_SPACE) {
		if (!base || base >= MSM_PCIE_MAX_RES) {
			PCIE_DBG_FS(pdev, "Invalid base_sel: 0x%x\n", base);
			PCIE_DBG_FS(pdev,
				"PCIe: base_sel is still 0x%x\n", base_sel);
			return -EINVAL;
		}

		base_sel = base;
		PCIE_DBG_FS(pdev, "PCIe: base_sel is now 0x%x\n", base_sel);

		if (option == MSM_PCIE_READ_PCIE_REGISTER ||
			option == MSM_PCIE_WRITE_PCIE_REGISTER) {
			wr_offset = offset;
			wr_mask = mask;
			wr_value = value;

			PCIE_DBG_FS(pdev,
				"PCIe: wr_offset is now 0x%x\n", wr_offset);
			PCIE_DBG_FS(pdev,
				"PCIe: wr_mask is now 0x%x\n", wr_mask);
			PCIE_DBG_FS(pdev,
				"PCIe: wr_value is now 0x%x\n", wr_value);
		}
	}

	pdev = PCIE_BUS_PRIV_DATA(dev->bus);
	rc_sel = BIT(pdev->rc_idx);

	msm_pcie_sel_debug_testcase(pdev, option);

	return ret;
}
EXPORT_SYMBOL(msm_pcie_debug_info);

#ifdef CONFIG_SYSFS
static ssize_t enumerate_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)
						dev_get_drvdata(dev);

	if (pcie_dev)
		msm_pcie_enumerate(pcie_dev->rc_idx);

	return count;
}
static DEVICE_ATTR_WO(enumerate);

static void msm_pcie_sysfs_init(struct msm_pcie_dev_t *dev)
{
	int ret;

	ret = device_create_file(&dev->pdev->dev, &dev_attr_enumerate);
	if (ret)
		PCIE_DBG_FS(dev,
			"RC%d: failed to create sysfs enumerate node\n",
			dev->rc_idx);
}

static void msm_pcie_sysfs_exit(struct msm_pcie_dev_t *dev)
{
	if (dev->pdev)
		device_remove_file(&dev->pdev->dev, &dev_attr_enumerate);
}
#else
static void msm_pcie_sysfs_init(struct msm_pcie_dev_t *dev)
{
}

static void msm_pcie_sysfs_exit(struct msm_pcie_dev_t *dev)
{
}
#endif

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent_msm_pcie;
static struct dentry *dfile_rc_sel;
static struct dentry *dfile_case;
static struct dentry *dfile_base_sel;
static struct dentry *dfile_linkdown_panic;
static struct dentry *dfile_wr_offset;
static struct dentry *dfile_wr_mask;
static struct dentry *dfile_wr_value;
static struct dentry *dfile_boot_option;
static struct dentry *dfile_aer_enable;
static struct dentry *dfile_corr_counter_limit;

static u32 rc_sel_max;

static int msm_pcie_debugfs_parse_input(const char __user *buf,
					size_t count, unsigned int *data)
{
	unsigned long ret;
	char *str, *str_temp;

	str = kmalloc(count + 1, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	ret = copy_from_user(str, buf, count);
	if (ret) {
		kfree(str);
		return -EFAULT;
	}

	str[count] = 0;
	str_temp = str;

	ret = get_option(&str_temp, data);
	kfree(str);
	if (ret != 1)
		return -EINVAL;

	return 0;
}

static int msm_pcie_debugfs_case_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < MSM_PCIE_MAX_DEBUGFS_OPTION; i++)
		seq_printf(m, "\t%d:\t %s\n", i,
			msm_pcie_debugfs_option_desc[i]);

	return 0;
}

static int msm_pcie_debugfs_case_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_pcie_debugfs_case_show, NULL);
}

static ssize_t msm_pcie_debugfs_case_select(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int i, ret;
	unsigned int testcase = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &testcase);
	if (ret)
		return ret;

	pr_alert("PCIe: TEST: %d\n", testcase);

	for (i = 0; i < MAX_RC_NUM; i++) {
		if (rc_sel & BIT(i))
			msm_pcie_sel_debug_testcase(&msm_pcie_dev[i], testcase);
	}

	return count;
}

static const struct file_operations msm_pcie_debugfs_case_ops = {
	.open = msm_pcie_debugfs_case_open,
	.release = single_release,
	.read = seq_read,
	.write = msm_pcie_debugfs_case_select,
};

static ssize_t msm_pcie_debugfs_rc_select(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int i, ret;
	u32 new_rc_sel = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &new_rc_sel);
	if (ret)
		return ret;

	if ((!new_rc_sel) || (new_rc_sel > rc_sel_max)) {
		pr_alert("PCIe: invalid value for rc_sel: 0x%x\n", new_rc_sel);
		pr_alert("PCIe: rc_sel is still 0x%x\n", rc_sel ? rc_sel : 0x1);
	} else {
		rc_sel = new_rc_sel;
		pr_alert("PCIe: rc_sel is now: 0x%x\n", rc_sel);
	}

	pr_alert("PCIe: the following RC(s) will be tested:\n");
	for (i = 0; i < MAX_RC_NUM; i++)
		if (rc_sel & BIT(i))
			pr_alert("RC %d\n", i);

	return count;
}

static const struct file_operations msm_pcie_debugfs_rc_select_ops = {
	.write = msm_pcie_debugfs_rc_select,
};

static ssize_t msm_pcie_debugfs_base_select(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret;
	u32 new_base_sel = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &new_base_sel);
	if (ret)
		return ret;

	if (!new_base_sel || new_base_sel > MSM_PCIE_MAX_RES) {
		pr_alert("PCIe: invalid value for base_sel: 0x%x\n",
			new_base_sel);
		pr_alert("PCIe: base_sel is still 0x%x\n", base_sel);
	} else {
		base_sel = new_base_sel;
		pr_alert("PCIe: base_sel is now 0x%x\n", base_sel);
		pr_alert("%s\n", msm_pcie_res_info[base_sel - 1].name);
	}

	return count;
}

static const struct file_operations msm_pcie_debugfs_base_select_ops = {
	.write = msm_pcie_debugfs_base_select,
};

static ssize_t msm_pcie_debugfs_linkdown_panic(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int i, ret;
	u32 new_linkdown_panic = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &new_linkdown_panic);
	if (ret)
		return ret;

	new_linkdown_panic = !!new_linkdown_panic;

	for (i = 0; i < MAX_RC_NUM; i++) {
		if (rc_sel & BIT(i)) {
			msm_pcie_dev[i].linkdown_panic =
				new_linkdown_panic;
			PCIE_DBG_FS(&msm_pcie_dev[i],
				"PCIe: RC%d: linkdown_panic is now %d\n",
				i, msm_pcie_dev[i].linkdown_panic);
		}
	}

	return count;
}

static const struct file_operations msm_pcie_debugfs_linkdown_panic_ops = {
	.write = msm_pcie_debugfs_linkdown_panic,
};

static ssize_t msm_pcie_debugfs_wr_offset(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret;

	wr_offset = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &wr_offset);
	if (ret)
		return ret;

	pr_alert("PCIe: wr_offset is now 0x%x\n", wr_offset);

	return count;
}

static const struct file_operations msm_pcie_debugfs_wr_offset_ops = {
	.write = msm_pcie_debugfs_wr_offset,
};

static ssize_t msm_pcie_debugfs_wr_mask(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret;

	wr_mask = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &wr_mask);
	if (ret)
		return ret;

	pr_alert("PCIe: wr_mask is now 0x%x\n", wr_mask);

	return count;
}

static const struct file_operations msm_pcie_debugfs_wr_mask_ops = {
	.write = msm_pcie_debugfs_wr_mask,
};
static ssize_t msm_pcie_debugfs_wr_value(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret;

	wr_value = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &wr_value);
	if (ret)
		return ret;

	pr_alert("PCIe: wr_value is now 0x%x\n", wr_value);

	return count;
}

static const struct file_operations msm_pcie_debugfs_wr_value_ops = {
	.write = msm_pcie_debugfs_wr_value,
};

static ssize_t msm_pcie_debugfs_boot_option(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int i, ret;
	u32 new_boot_option = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &new_boot_option);
	if (ret)
		return ret;

	if (new_boot_option <= (BIT(0) | BIT(1))) {
		for (i = 0; i < MAX_RC_NUM; i++) {
			if (rc_sel & BIT(i)) {
				msm_pcie_dev[i].boot_option = new_boot_option;
				PCIE_DBG_FS(&msm_pcie_dev[i],
					"PCIe: RC%d: boot_option is now 0x%x\n",
					i, msm_pcie_dev[i].boot_option);
			}
		}
	} else {
		pr_err("PCIe: Invalid input for boot_option: 0x%x.\n",
			new_boot_option);
	}

	return count;
}

static const struct file_operations msm_pcie_debugfs_boot_option_ops = {
	.write = msm_pcie_debugfs_boot_option,
};

static ssize_t msm_pcie_debugfs_aer_enable(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int i, ret;
	u32 new_aer_enable = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &new_aer_enable);
	if (ret)
		return ret;

	new_aer_enable = !!new_aer_enable;

	for (i = 0; i < MAX_RC_NUM; i++) {
		if (rc_sel & BIT(i)) {
			msm_pcie_dev[i].aer_enable = new_aer_enable;
			PCIE_DBG_FS(&msm_pcie_dev[i],
				"PCIe: RC%d: aer_enable is now %d\n",
				i, msm_pcie_dev[i].aer_enable);

			msm_pcie_write_mask(msm_pcie_dev[i].dm_core +
					PCIE20_BRIDGE_CTRL,
					new_aer_enable ? 0 : BIT(16),
					new_aer_enable ? BIT(16) : 0);

			PCIE_DBG_FS(&msm_pcie_dev[i],
				"RC%d: PCIE20_BRIDGE_CTRL: 0x%x\n", i,
				readl_relaxed(msm_pcie_dev[i].dm_core +
					PCIE20_BRIDGE_CTRL));
		}
	}

	return count;
}

static const struct file_operations msm_pcie_debugfs_aer_enable_ops = {
	.write = msm_pcie_debugfs_aer_enable,
};

static ssize_t msm_pcie_debugfs_corr_counter_limit(struct file *file,
				const char __user *buf,
				size_t count, loff_t *ppos)
{
	int ret;

	corr_counter_limit = 0;

	ret = msm_pcie_debugfs_parse_input(buf, count, &corr_counter_limit);
	if (ret)
		return ret;

	pr_info("PCIe: corr_counter_limit is now %u\n", corr_counter_limit);

	return count;
}

static const struct file_operations msm_pcie_debugfs_corr_counter_limit_ops = {
	.write = msm_pcie_debugfs_corr_counter_limit,
};

static void msm_pcie_debugfs_init(void)
{
	rc_sel_max = (0x1 << MAX_RC_NUM) - 1;
	wr_mask = 0xffffffff;

	dent_msm_pcie = debugfs_create_dir("pci-msm", NULL);
	if (IS_ERR(dent_msm_pcie)) {
		pr_err("PCIe: fail to create the folder for debug_fs.\n");
		return;
	}

	dfile_rc_sel = debugfs_create_file("rc_sel", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_rc_select_ops);
	if (!dfile_rc_sel || IS_ERR(dfile_rc_sel)) {
		pr_err("PCIe: fail to create the file for debug_fs rc_sel.\n");
		goto rc_sel_error;
	}

	dfile_case = debugfs_create_file("case", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_case_ops);
	if (!dfile_case || IS_ERR(dfile_case)) {
		pr_err("PCIe: fail to create the file for debug_fs case.\n");
		goto case_error;
	}

	dfile_base_sel = debugfs_create_file("base_sel", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_base_select_ops);
	if (!dfile_base_sel || IS_ERR(dfile_base_sel)) {
		pr_err("PCIe: fail to create the file for debug_fs base_sel.\n");
		goto base_sel_error;
	}

	dfile_linkdown_panic = debugfs_create_file("linkdown_panic", 0644,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_linkdown_panic_ops);
	if (!dfile_linkdown_panic || IS_ERR(dfile_linkdown_panic)) {
		pr_err("PCIe: fail to create the file for debug_fs linkdown_panic.\n");
		goto linkdown_panic_error;
	}

	dfile_wr_offset = debugfs_create_file("wr_offset", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_wr_offset_ops);
	if (!dfile_wr_offset || IS_ERR(dfile_wr_offset)) {
		pr_err("PCIe: fail to create the file for debug_fs wr_offset.\n");
		goto wr_offset_error;
	}

	dfile_wr_mask = debugfs_create_file("wr_mask", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_wr_mask_ops);
	if (!dfile_wr_mask || IS_ERR(dfile_wr_mask)) {
		pr_err("PCIe: fail to create the file for debug_fs wr_mask.\n");
		goto wr_mask_error;
	}

	dfile_wr_value = debugfs_create_file("wr_value", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_wr_value_ops);
	if (!dfile_wr_value || IS_ERR(dfile_wr_value)) {
		pr_err("PCIe: fail to create the file for debug_fs wr_value.\n");
		goto wr_value_error;
	}

	dfile_boot_option = debugfs_create_file("boot_option", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_boot_option_ops);
	if (!dfile_boot_option || IS_ERR(dfile_boot_option)) {
		pr_err("PCIe: fail to create the file for debug_fs boot_option.\n");
		goto boot_option_error;
	}

	dfile_aer_enable = debugfs_create_file("aer_enable", 0664,
					dent_msm_pcie, NULL,
					&msm_pcie_debugfs_aer_enable_ops);
	if (!dfile_aer_enable || IS_ERR(dfile_aer_enable)) {
		pr_err("PCIe: fail to create the file for debug_fs aer_enable.\n");
		goto aer_enable_error;
	}

	dfile_corr_counter_limit = debugfs_create_file("corr_counter_limit",
				0664, dent_msm_pcie, NULL,
				&msm_pcie_debugfs_corr_counter_limit_ops);
	if (!dfile_corr_counter_limit || IS_ERR(dfile_corr_counter_limit)) {
		pr_err("PCIe: fail to create the file for debug_fs corr_counter_limit.\n");
		goto corr_counter_limit_error;
	}
	return;

corr_counter_limit_error:
	debugfs_remove(dfile_aer_enable);
aer_enable_error:
	debugfs_remove(dfile_boot_option);
boot_option_error:
	debugfs_remove(dfile_wr_value);
wr_value_error:
	debugfs_remove(dfile_wr_mask);
wr_mask_error:
	debugfs_remove(dfile_wr_offset);
wr_offset_error:
	debugfs_remove(dfile_linkdown_panic);
linkdown_panic_error:
	debugfs_remove(dfile_base_sel);
base_sel_error:
	debugfs_remove(dfile_case);
case_error:
	debugfs_remove(dfile_rc_sel);
rc_sel_error:
	debugfs_remove(dent_msm_pcie);
}

static void msm_pcie_debugfs_exit(void)
{
	debugfs_remove(dfile_rc_sel);
	debugfs_remove(dfile_case);
	debugfs_remove(dfile_base_sel);
	debugfs_remove(dfile_linkdown_panic);
	debugfs_remove(dfile_wr_offset);
	debugfs_remove(dfile_wr_mask);
	debugfs_remove(dfile_wr_value);
	debugfs_remove(dfile_boot_option);
	debugfs_remove(dfile_aer_enable);
	debugfs_remove(dfile_corr_counter_limit);
}
#else
static void msm_pcie_debugfs_init(void)
{
}

static void msm_pcie_debugfs_exit(void)
{
}
#endif

static inline int msm_pcie_is_link_up(struct msm_pcie_dev_t *dev)
{
	return readl_relaxed(dev->dm_core +
			PCIE20_CAP_LINKCTRLSTATUS) & BIT(29);
}

/**
 * msm_pcie_iatu_config - configure outbound address translation region
 * @dev: root commpex
 * @nr: region number
 * @type: target transaction type, see PCIE20_CTRL1_TYPE_xxx
 * @host_addr: - region start address on host
 * @host_end: - region end address (low 32 bit) on host,
 *	upper 32 bits are same as for @host_addr
 * @target_addr: - region start address on target
 */
static void msm_pcie_iatu_config(struct msm_pcie_dev_t *dev, int nr, u8 type,
				unsigned long host_addr, u32 host_end,
				unsigned long target_addr)
{
	void __iomem *iatu_base = dev->iatu ? dev->iatu : dev->dm_core;

	u32 iatu_viewport_offset;
	u32 iatu_ctrl1_offset;
	u32 iatu_ctrl2_offset;
	u32 iatu_lbar_offset;
	u32 iatu_ubar_offset;
	u32 iatu_lar_offset;
	u32 iatu_ltar_offset;
	u32 iatu_utar_offset;

	if (dev->iatu) {
		iatu_viewport_offset = 0;
		iatu_ctrl1_offset = PCIE_IATU_CTRL1(nr);
		iatu_ctrl2_offset = PCIE_IATU_CTRL2(nr);
		iatu_lbar_offset = PCIE_IATU_LBAR(nr);
		iatu_ubar_offset = PCIE_IATU_UBAR(nr);
		iatu_lar_offset = PCIE_IATU_LAR(nr);
		iatu_ltar_offset = PCIE_IATU_LTAR(nr);
		iatu_utar_offset = PCIE_IATU_UTAR(nr);
	} else {
		iatu_viewport_offset = PCIE20_PLR_IATU_VIEWPORT;
		iatu_ctrl1_offset = PCIE20_PLR_IATU_CTRL1;
		iatu_ctrl2_offset = PCIE20_PLR_IATU_CTRL2;
		iatu_lbar_offset = PCIE20_PLR_IATU_LBAR;
		iatu_ubar_offset = PCIE20_PLR_IATU_UBAR;
		iatu_lar_offset = PCIE20_PLR_IATU_LAR;
		iatu_ltar_offset = PCIE20_PLR_IATU_LTAR;
		iatu_utar_offset = PCIE20_PLR_IATU_UTAR;
	}

	if (dev->shadow_en && iatu_viewport_offset) {
		dev->rc_shadow[PCIE20_PLR_IATU_VIEWPORT / 4] =
			nr;
		dev->rc_shadow[PCIE20_PLR_IATU_CTRL1 / 4] =
			type;
		dev->rc_shadow[PCIE20_PLR_IATU_LBAR / 4] =
			lower_32_bits(host_addr);
		dev->rc_shadow[PCIE20_PLR_IATU_UBAR / 4] =
			upper_32_bits(host_addr);
		dev->rc_shadow[PCIE20_PLR_IATU_LAR / 4] =
			host_end;
		dev->rc_shadow[PCIE20_PLR_IATU_LTAR / 4] =
			lower_32_bits(target_addr);
		dev->rc_shadow[PCIE20_PLR_IATU_UTAR / 4] =
			upper_32_bits(target_addr);
		dev->rc_shadow[PCIE20_PLR_IATU_CTRL2 / 4] =
			BIT(31);
	}

	/* select region */
	if (iatu_viewport_offset) {
		writel_relaxed(nr, iatu_base + iatu_viewport_offset);
		/* ensure that hardware locks it */
		wmb();
	}

	/* switch off region before changing it */
	writel_relaxed(0, iatu_base + iatu_ctrl2_offset);
	/* and wait till it propagates to the hardware */
	wmb();

	writel_relaxed(type, iatu_base + iatu_ctrl1_offset);
	writel_relaxed(lower_32_bits(host_addr),
		       iatu_base + iatu_lbar_offset);
	writel_relaxed(upper_32_bits(host_addr),
		       iatu_base + iatu_ubar_offset);
	writel_relaxed(host_end, iatu_base + iatu_lar_offset);
	writel_relaxed(lower_32_bits(target_addr),
		       iatu_base + iatu_ltar_offset);
	writel_relaxed(upper_32_bits(target_addr),
		       iatu_base + iatu_utar_offset);
	/* ensure that changes propagated to the hardware */
	wmb();
	writel_relaxed(BIT(31), iatu_base + iatu_ctrl2_offset);

	/* ensure that changes propagated to the hardware */
	wmb();

	if (dev->enumerated) {
		PCIE_DBG2(dev, "IATU for Endpoint %02x:%02x.%01x\n",
			dev->pcidev_table[nr].bdf >> 24,
			dev->pcidev_table[nr].bdf >> 19 & 0x1f,
			dev->pcidev_table[nr].bdf >> 16 & 0x07);
		if (iatu_viewport_offset)
			PCIE_DBG2(dev, "IATU_VIEWPORT:0x%x\n",
				readl_relaxed(dev->dm_core +
					PCIE20_PLR_IATU_VIEWPORT));
		PCIE_DBG2(dev, "IATU_CTRL1:0x%x\n",
			readl_relaxed(iatu_base + iatu_ctrl1_offset));
		PCIE_DBG2(dev, "IATU_LBAR:0x%x\n",
			readl_relaxed(iatu_base + iatu_lbar_offset));
		PCIE_DBG2(dev, "IATU_UBAR:0x%x\n",
			readl_relaxed(iatu_base + iatu_ubar_offset));
		PCIE_DBG2(dev, "IATU_LAR:0x%x\n",
			readl_relaxed(iatu_base + iatu_lar_offset));
		PCIE_DBG2(dev, "IATU_LTAR:0x%x\n",
			readl_relaxed(iatu_base + iatu_ltar_offset));
		PCIE_DBG2(dev, "IATU_UTAR:0x%x\n",
			readl_relaxed(iatu_base + iatu_utar_offset));
		PCIE_DBG2(dev, "IATU_CTRL2:0x%x\n\n",
			readl_relaxed(iatu_base + iatu_ctrl2_offset));
	}
}

/**
 * msm_pcie_cfg_bdf - configure for config access
 * @dev: root commpex
 * @bus: PCI bus number
 * @devfn: PCI dev and function number
 *
 * Remap if required region 0 for config access of proper type
 * (CFG0 for bus 1, CFG1 for other buses)
 * Cache current device bdf for speed-up
 */
static void msm_pcie_cfg_bdf(struct msm_pcie_dev_t *dev, u8 bus, u8 devfn)
{
	struct resource *axi_conf = dev->res[MSM_PCIE_RES_CONF].resource;
	u32 bdf  = BDF_OFFSET(bus, devfn);
	u8 type = bus == 1 ? PCIE20_CTRL1_TYPE_CFG0 : PCIE20_CTRL1_TYPE_CFG1;

	if (dev->current_bdf == bdf)
		return;

	msm_pcie_iatu_config(dev, 0, type,
			axi_conf->start,
			axi_conf->start + SZ_4K - 1,
			bdf);

	dev->current_bdf = bdf;
}

static inline void msm_pcie_save_shadow(struct msm_pcie_dev_t *dev,
					u32 word_offset, u32 wr_val,
					u32 bdf, bool rc)
{
	int i, j;
	u32 max_dev = MAX_RC_NUM * MAX_DEVICE_NUM;

	if (rc) {
		dev->rc_shadow[word_offset / 4] = wr_val;
	} else {
		for (i = 0; i < MAX_DEVICE_NUM; i++) {
			if (!dev->pcidev_table[i].bdf) {
				for (j = 0; j < max_dev; j++)
					if (!msm_pcie_dev_tbl[j].bdf) {
						msm_pcie_dev_tbl[j].bdf = bdf;
						break;
					}
				dev->pcidev_table[i].bdf = bdf;
				if ((!dev->bridge_found) && (i > 0))
					dev->bridge_found = true;
			}
			if (dev->pcidev_table[i].bdf == bdf) {
				dev->ep_shadow[i][word_offset / 4] = wr_val;
				break;
			}
		}
	}
}

static inline int msm_pcie_oper_conf(struct pci_bus *bus, u32 devfn, int oper,
				     int where, int size, u32 *val)
{
	uint32_t word_offset, byte_offset, mask;
	uint32_t rd_val, wr_val;
	struct msm_pcie_dev_t *dev;
	void __iomem *config_base;
	bool rc = false;
	u32 rc_idx;
	int rv = 0;
	u32 bdf = BDF_OFFSET(bus->number, devfn);
	int i;

	dev = PCIE_BUS_PRIV_DATA(bus);

	if (!dev) {
		pr_err("PCIe: No device found for this bus.\n");
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	rc_idx = dev->rc_idx;
	rc = (bus->number == 0);

	spin_lock_irqsave(&dev->cfg_lock, dev->irqsave_flags);

	if (!dev->cfg_access) {
		PCIE_DBG3(dev,
			"Access denied for RC%d %d:0x%02x + 0x%04x[%d]\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	if (rc && (devfn != 0)) {
		PCIE_DBG3(dev, "RC%d invalid %s - bus %d devfn %d\n", rc_idx,
			 (oper == RD) ? "rd" : "wr", bus->number, devfn);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	if (dev->link_status != MSM_PCIE_LINK_ENABLED) {
		PCIE_DBG3(dev,
			"Access to RC%d %d:0x%02x + 0x%04x[%d] is denied because link is down\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	/* check if the link is up for endpoint */
	if (!rc && !msm_pcie_is_link_up(dev)) {
		PCIE_ERR(dev,
			"PCIe: RC%d %s fail, link down - bus %d devfn %d\n",
				rc_idx, (oper == RD) ? "rd" : "wr",
				bus->number, devfn);
			*val = ~0;
			rv = PCIBIOS_DEVICE_NOT_FOUND;
			goto unlock;
	}

	if (!rc && !dev->enumerated)
		msm_pcie_cfg_bdf(dev, bus->number, devfn);

	word_offset = where & ~0x3;
	byte_offset = where & 0x3;
	mask = ((u32)~0 >> (8 * (4 - size))) << (8 * byte_offset);

	if (rc || !dev->enumerated) {
		config_base = rc ? dev->dm_core : dev->conf;
	} else {
		for (i = 0; i < MAX_DEVICE_NUM; i++) {
			if (dev->pcidev_table[i].bdf == bdf) {
				config_base = dev->pcidev_table[i].conf_base;
				break;
			}
		}
		if (i == MAX_DEVICE_NUM) {
			*val = ~0;
			rv = PCIBIOS_DEVICE_NOT_FOUND;
			goto unlock;
		}
	}

	rd_val = readl_relaxed(config_base + word_offset);

	if (oper == RD) {
		*val = ((rd_val & mask) >> (8 * byte_offset));
		PCIE_DBG3(dev,
			"RC%d %d:0x%02x + 0x%04x[%d] -> 0x%08x; rd 0x%08x\n",
			rc_idx, bus->number, devfn, where, size, *val, rd_val);
	} else {
		wr_val = (rd_val & ~mask) |
				((*val << (8 * byte_offset)) & mask);

		if ((bus->number == 0) && (where == 0x3c))
			wr_val = wr_val | (3 << 16);

		writel_relaxed(wr_val, config_base + word_offset);
		wmb(); /* ensure config data is written to hardware register */

		if (dev->shadow_en) {
			if (rd_val == PCIE_LINK_DOWN &&
				(readl_relaxed(config_base) == PCIE_LINK_DOWN))
				PCIE_ERR(dev,
					"Read of RC%d %d:0x%02x + 0x%04x[%d] is all FFs\n",
					rc_idx, bus->number, devfn,
					where, size);
			else
				msm_pcie_save_shadow(dev, word_offset, wr_val,
					bdf, rc);
		}

		PCIE_DBG3(dev,
			"RC%d %d:0x%02x + 0x%04x[%d] <- 0x%08x; rd 0x%08x val 0x%08x\n",
			rc_idx, bus->number, devfn, where, size,
			wr_val, rd_val, *val);
	}

unlock:
	spin_unlock_irqrestore(&dev->cfg_lock, dev->irqsave_flags);
out:
	return rv;
}

static int msm_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			    int size, u32 *val)
{
	int ret = msm_pcie_oper_conf(bus, devfn, RD, where, size, val);

	if ((bus->number == 0) && (where == PCI_CLASS_REVISION)) {
		*val = (*val & 0xff) | (PCI_CLASS_BRIDGE_PCI << 16);
		PCIE_GEN_DBG("change class for RC:0x%x\n", *val);
	}

	return ret;
}

static int msm_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			    int where, int size, u32 val)
{
	return msm_pcie_oper_conf(bus, devfn, WR, where, size, &val);
}

static struct pci_ops msm_pcie_ops = {
	.read = msm_pcie_rd_conf,
	.write = msm_pcie_wr_conf,
};

static int msm_pcie_gpio_init(struct msm_pcie_dev_t *dev)
{
	int rc = 0, i;
	struct msm_pcie_gpio_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < dev->gpio_n; i++) {
		info = &dev->gpio[i];

		if (!info->num)
			continue;

		rc = gpio_request(info->num, info->name);
		if (rc) {
			PCIE_ERR(dev, "PCIe: RC%d can't get gpio %s; %d\n",
				dev->rc_idx, info->name, rc);
			break;
		}

		if (info->out)
			rc = gpio_direction_output(info->num, info->init);
		else
			rc = gpio_direction_input(info->num);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d can't set direction for GPIO %s:%d\n",
				dev->rc_idx, info->name, rc);
			gpio_free(info->num);
			break;
		}
	}

	if (rc)
		while (i--)
			gpio_free(dev->gpio[i].num);

	return rc;
}

static void msm_pcie_gpio_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < dev->gpio_n; i++)
		gpio_free(dev->gpio[i].num);
}

static int msm_pcie_vreg_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct regulator *vreg;
	struct msm_pcie_vreg_info_t *info;

	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		info = &dev->vreg[i];
		vreg = info->hdl;

		if (!vreg)
			continue;

		PCIE_DBG2(dev, "RC%d Vreg %s is being enabled\n",
			dev->rc_idx, info->name);
		if (info->max_v) {
			rc = regulator_set_voltage(vreg,
						   info->min_v, info->max_v);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set voltage for %s: %d\n",
					dev->rc_idx, info->name, rc);
				break;
			}
		}

		if (info->opt_mode) {
			rc = regulator_set_load(vreg, info->opt_mode);
			if (rc < 0) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set mode for %s: %d\n",
					dev->rc_idx, info->name, rc);
				break;
			}
		}

		rc = regulator_enable(vreg);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d can't enable regulator %s: %d\n",
				dev->rc_idx, info->name, rc);
			break;
		}
	}

	if (rc)
		while (i--) {
			struct regulator *hdl = dev->vreg[i].hdl;

			if (hdl) {
				regulator_disable(hdl);
				if (!strcmp(dev->vreg[i].name, "vreg-cx")) {
					PCIE_DBG(dev,
						"RC%d: Removing %s vote.\n",
						dev->rc_idx,
						dev->vreg[i].name);
					regulator_set_voltage(hdl,
						RPMH_REGULATOR_LEVEL_RETENTION,
						RPMH_REGULATOR_LEVEL_MAX);
				}

				if (dev->vreg[i].opt_mode) {
					rc = regulator_set_load(hdl, 0);
					if (rc < 0)
						PCIE_ERR(dev,
							"PCIe: RC%d can't set mode for %s: %d\n",
							dev->rc_idx,
							dev->vreg[i].name, rc);
				}
			}

		}

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);

	return rc;
}

static void msm_pcie_vreg_deinit(struct msm_pcie_dev_t *dev)
{
	int i, ret;

	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	for (i = MSM_PCIE_MAX_VREG - 1; i >= 0; i--) {
		if (dev->vreg[i].hdl) {
			PCIE_DBG(dev, "Vreg %s is being disabled\n",
				dev->vreg[i].name);
			regulator_disable(dev->vreg[i].hdl);

			if (!strcmp(dev->vreg[i].name, "vreg-cx")) {
				PCIE_DBG(dev,
					"RC%d: Removing %s vote.\n",
					dev->rc_idx,
					dev->vreg[i].name);
				regulator_set_voltage(dev->vreg[i].hdl,
					RPMH_REGULATOR_LEVEL_RETENTION,
					RPMH_REGULATOR_LEVEL_MAX);
			}

			if (dev->vreg[i].opt_mode) {
				ret = regulator_set_load(dev->vreg[i].hdl, 0);
				if (ret < 0)
					PCIE_ERR(dev,
						"PCIe: RC%d can't set mode for %s: %d\n",
						dev->rc_idx, dev->vreg[i].name,
						ret);
			}
		}
	}

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);
}

static int msm_pcie_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;
	struct msm_pcie_reset_info_t *reset_info;

	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	rc = regulator_enable(dev->gdsc);

	if (rc) {
		PCIE_ERR(dev, "PCIe: fail to enable GDSC for RC%d (%s)\n",
			dev->rc_idx, dev->pdev->name);
		return rc;
	}

	PCIE_DBG(dev, "PCIe: requesting bus vote for RC%d\n", dev->rc_idx);
	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 1);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: fail to set bus bandwidth for RC%d:%d.\n",
				dev->rc_idx, rc);
			return rc;
		}

		PCIE_DBG2(dev,
			"PCIe: set bus bandwidth for RC%d.\n",
			dev->rc_idx);
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		info = &dev->clk[i];

		if (!info->hdl)
			continue;

		if (info->config_mem)
			msm_pcie_config_clock_mem(dev, info);

		if (info->freq) {
			if (!strcmp(info->name, "pcie_phy_refgen_clk")) {
				mutex_lock(&dev->clk_lock);
				pcie_drv.rate_change_vote |= BIT(dev->rc_idx);
				mutex_unlock(&dev->clk_lock);
			}

			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
				break;
			}

			PCIE_DBG2(dev,
				"PCIe: RC%d set rate for clk %s.\n",
				dev->rc_idx, info->name);
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			PCIE_ERR(dev, "PCIe: RC%d failed to enable clk %s\n",
				dev->rc_idx, info->name);
		else
			PCIE_DBG2(dev, "enable clk %s for RC%d.\n",
				info->name, dev->rc_idx);
	}

	if (rc) {
		PCIE_DBG(dev, "RC%d disable clocks for error handling.\n",
			dev->rc_idx);
		while (i--) {
			struct clk *hdl = dev->clk[i].hdl;

			if (hdl)
				clk_disable_unprepare(hdl);
		}

		regulator_disable(dev->gdsc);
	}

	for (i = 0; i < MSM_PCIE_MAX_RESET; i++) {
		reset_info = &dev->reset[i];
		if (reset_info->hdl) {
			rc = reset_control_assert(reset_info->hdl);
			if (rc)
				PCIE_ERR(dev,
					"PCIe: RC%d failed to assert reset for %s.\n",
					dev->rc_idx, reset_info->name);
			else
				PCIE_DBG2(dev,
					"PCIe: RC%d successfully asserted reset for %s.\n",
					dev->rc_idx, reset_info->name);

			/* add a 1ms delay to ensure the reset is asserted */
			usleep_range(1000, 1005);

			rc = reset_control_deassert(reset_info->hdl);
			if (rc)
				PCIE_ERR(dev,
					"PCIe: RC%d failed to deassert reset for %s.\n",
					dev->rc_idx, reset_info->name);
			else
				PCIE_DBG2(dev,
					"PCIe: RC%d successfully deasserted reset for %s.\n",
					dev->rc_idx, reset_info->name);
		}
	}

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);

	return rc;
}

static void msm_pcie_clk_deinit(struct msm_pcie_dev_t *dev)
{
	int i;
	int rc;

	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++)
		if (dev->clk[i].hdl)
			clk_disable_unprepare(dev->clk[i].hdl);

	if (dev->rate_change_clk) {
		mutex_lock(&dev->clk_lock);

		pcie_drv.rate_change_vote &= ~BIT(dev->rc_idx);
		if (!pcie_drv.rate_change_vote)
			clk_set_rate(dev->rate_change_clk->hdl,
					RATE_CHANGE_19P2MHZ);

		mutex_unlock(&dev->clk_lock);
	}

	if (dev->bus_client) {
		PCIE_DBG(dev, "PCIe: removing bus vote for RC%d\n",
			dev->rc_idx);

		rc = msm_bus_scale_client_update_request(dev->bus_client, 0);
		if (rc)
			PCIE_ERR(dev,
				"PCIe: fail to relinquish bus bandwidth for RC%d:%d.\n",
				dev->rc_idx, rc);
		else
			PCIE_DBG(dev,
				"PCIe: relinquish bus bandwidth for RC%d.\n",
				dev->rc_idx);
	}

	regulator_disable(dev->gdsc);

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);
}

static int msm_pcie_pipe_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;
	struct msm_pcie_reset_info_t *pipe_reset_info;

	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		info = &dev->pipeclk[i];

		if (!info->hdl)
			continue;

		if (info->config_mem)
			msm_pcie_config_clock_mem(dev, info);

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
				break;
			}

			PCIE_DBG2(dev,
				"PCIe: RC%d set rate for clk %s: %d.\n",
				dev->rc_idx, info->name, rc);
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			PCIE_ERR(dev, "PCIe: RC%d failed to enable clk %s.\n",
				dev->rc_idx, info->name);
		else
			PCIE_DBG2(dev, "RC%d enabled pipe clk %s.\n",
				dev->rc_idx, info->name);
	}

	if (rc) {
		PCIE_DBG(dev, "RC%d disable pipe clocks for error handling.\n",
			dev->rc_idx);
		while (i--)
			if (dev->pipeclk[i].hdl)
				clk_disable_unprepare(dev->pipeclk[i].hdl);
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_RESET; i++) {
		pipe_reset_info = &dev->pipe_reset[i];
		if (pipe_reset_info->hdl) {
			rc = reset_control_assert(pipe_reset_info->hdl);
			if (rc)
				PCIE_ERR(dev,
					"PCIe: RC%d failed to assert pipe reset for %s.\n",
					dev->rc_idx, pipe_reset_info->name);
			else
				PCIE_DBG2(dev,
					"PCIe: RC%d successfully asserted pipe reset for %s.\n",
					dev->rc_idx, pipe_reset_info->name);

			/* add a 1ms delay to ensure the reset is asserted */
			usleep_range(1000, 1005);

			rc = reset_control_deassert(
					pipe_reset_info->hdl);
			if (rc)
				PCIE_ERR(dev,
					"PCIe: RC%d failed to deassert pipe reset for %s.\n",
					dev->rc_idx, pipe_reset_info->name);
			else
				PCIE_DBG2(dev,
					"PCIe: RC%d successfully deasserted pipe reset for %s.\n",
					dev->rc_idx, pipe_reset_info->name);
		}
	}

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);

	return rc;
}

static void msm_pcie_pipe_clk_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++)
		if (dev->pipeclk[i].hdl)
			clk_disable_unprepare(
				dev->pipeclk[i].hdl);

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);
}

static bool pcie_phy_is_ready(struct msm_pcie_dev_t *dev)
{
	/* There is no PHY status check in RUMI */
	if (dev->rumi)
		return true;

	if (readl_relaxed(dev->phy + dev->phy_status_offset) &
		BIT(dev->phy_status_bit))
		return false;
	else
		return true;
}

static int pcie_phy_init(struct msm_pcie_dev_t *dev)
{
	int i, ret;
	long retries = 0;
	struct msm_pcie_phy_info_t *phy_seq;

	PCIE_DBG(dev, "PCIe: RC%d: Initializing PHY\n", dev->rc_idx);

	if (dev->phy_sequence) {
		i =  dev->phy_len;
		phy_seq = dev->phy_sequence;
		while (i--) {
			msm_pcie_write_reg(dev->phy,
				phy_seq->offset,
				phy_seq->val);
			if (phy_seq->delay)
				usleep_range(phy_seq->delay,
					phy_seq->delay + 1);
			phy_seq++;
		}
	}

	usleep_range(PHY_STABILIZATION_DELAY_US_MIN,
		PHY_STABILIZATION_DELAY_US_MAX);

	/* Enable the pipe clock */
	ret = msm_pcie_pipe_clk_init(dev);

	/* ensure that changes propagated to the hardware */
	wmb();

	PCIE_DBG(dev, "PCIe RC%d: waiting for phy ready...\n", dev->rc_idx);
	do {
		if (pcie_phy_is_ready(dev))
			break;
		retries++;
		usleep_range(REFCLK_STABILIZATION_DELAY_US_MIN,
					 REFCLK_STABILIZATION_DELAY_US_MAX);
	} while (retries < PHY_READY_TIMEOUT_COUNT);

	PCIE_DBG(dev, "PCIe: RC%d: number of PHY retries: %ld.\n", dev->rc_idx,
		retries);

	if (!pcie_phy_is_ready(dev)) {
		PCIE_ERR(dev, "PCIe PHY RC%d failed to come up!\n",
			dev->rc_idx);
		pcie_phy_dump(dev);
		return -ENODEV;
	}

	PCIE_INFO(dev, "PCIe RC%d PHY is ready!\n", dev->rc_idx);

	return 0;
}

static void msm_pcie_iatu_config_all_ep(struct msm_pcie_dev_t *dev)
{
	int i;
	u8 type;
	struct msm_pcie_device_info *dev_table = dev->pcidev_table;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (!dev_table[i].bdf)
			break;

		type = dev_table[i].bdf >> 24 == 0x1 ?
			PCIE20_CTRL1_TYPE_CFG0 : PCIE20_CTRL1_TYPE_CFG1;

		msm_pcie_iatu_config(dev, i, type, dev_table[i].phy_address,
			dev_table[i].phy_address + SZ_4K - 1,
			dev_table[i].bdf);
	}
}

static void msm_pcie_config_controller(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	/*
	 * program and enable address translation region 0 (device config
	 * address space); region type config;
	 * axi config address range to device config address range
	 */
	if (dev->enumerated) {
		msm_pcie_iatu_config_all_ep(dev);
	} else {
		dev->current_bdf = 0; /* to force IATU re-config */
		msm_pcie_cfg_bdf(dev, 1, 0);
	}

	/* configure N_FTS */
	PCIE_DBG2(dev, "Original PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG));
	if (!dev->n_fts)
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					0, BIT(15));
	else
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					PCIE20_ACK_N_FTS,
					dev->n_fts << 8);

	if (dev->shadow_en)
		dev->rc_shadow[PCIE20_ACK_F_ASPM_CTRL_REG / 4] =
			readl_relaxed(dev->dm_core +
			PCIE20_ACK_F_ASPM_CTRL_REG);

	PCIE_DBG2(dev, "Updated PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG));

	/* configure AUX clock frequency register for PCIe core */
	if (dev->use_19p2mhz_aux_clk)
		msm_pcie_write_reg(dev->dm_core, PCIE20_AUX_CLK_FREQ_REG, 0x14);
	else
		msm_pcie_write_reg(dev->dm_core, PCIE20_AUX_CLK_FREQ_REG, 0x01);

	/* configure the completion timeout value for PCIe core */
	if (dev->cpl_timeout && dev->bridge_found)
		msm_pcie_write_reg_field(dev->dm_core,
					PCIE20_DEVICE_CONTROL2_STATUS2,
					0xf, dev->cpl_timeout);

	/* Enable AER on RC */
	if (dev->aer_enable) {
		msm_pcie_write_mask(dev->dm_core + PCIE20_BRIDGE_CTRL, 0,
						BIT(16)|BIT(17));
		msm_pcie_write_mask(dev->dm_core +  PCIE20_CAP_DEVCTRLSTATUS, 0,
						BIT(3)|BIT(2)|BIT(1)|BIT(0));

		PCIE_DBG(dev, "RC's PCIE20_CAP_DEVCTRLSTATUS:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_CAP_DEVCTRLSTATUS));
	}
}

static int msm_pcie_get_clk(struct msm_pcie_dev_t *pcie_dev)
{
	int i, cnt, ret;
	struct msm_pcie_clk_info_t *clk_info;
	u32 *clkfreq = NULL;
	struct platform_device *pdev = pcie_dev->pdev;

	cnt = of_property_count_elems_of_size((&pdev->dev)->of_node,
			"max-clock-frequency-hz", sizeof(u32));
	if (cnt <= 0)
		return -EINVAL;

	clkfreq = devm_kcalloc(&pdev->dev, MSM_PCIE_MAX_CLK +
			MSM_PCIE_MAX_PIPE_CLK, sizeof(*clkfreq), GFP_KERNEL);
	if (!clkfreq)
		return -ENOMEM;

	ret = of_property_read_u32_array(pdev->dev.of_node,
		"max-clock-frequency-hz", clkfreq, cnt);
	if (ret) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d: invalid max-clock-frequency-hz property %d\n",
			pcie_dev->rc_idx, ret);
		return ret;
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		clk_info = &pcie_dev->clk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG(pcie_dev,
					"Clock %s isn't available:%ld\n",
					clk_info->name, PTR_ERR(clk_info->hdl));
				return PTR_ERR(clk_info->hdl);
			}

			PCIE_DBG(pcie_dev, "Ignoring Clock %s\n",
				clk_info->name);
			clk_info->hdl = NULL;
		} else {
			clk_info->freq = clkfreq[i + MSM_PCIE_MAX_PIPE_CLK];
			PCIE_DBG(pcie_dev, "Freq of Clock %s is:%d\n",
				clk_info->name, clk_info->freq);

			if (!strcmp(clk_info->name, "pcie_phy_refgen_clk"))
				pcie_dev->rate_change_clk = clk_info;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		clk_info = &pcie_dev->pipeclk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG(pcie_dev,
					"Clock %s isn't available:%ld\n",
					clk_info->name, PTR_ERR(clk_info->hdl));
				return PTR_ERR(clk_info->hdl);
			}

			PCIE_DBG(pcie_dev, "Ignoring Clock %s\n",
				clk_info->name);
			clk_info->hdl = NULL;
		} else {
			clk_info->freq = clkfreq[i];
			PCIE_DBG(pcie_dev, "Freq of Clock %s is:%d\n",
				clk_info->name, clk_info->freq);
		}
	}

	return 0;
}

static int msm_pcie_get_vreg(struct msm_pcie_dev_t *pcie_dev)
{
	int i, len;
	struct platform_device *pdev = pcie_dev->pdev;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		struct msm_pcie_vreg_info_t *vreg_info = &pcie_dev->vreg[i];

		vreg_info->hdl = devm_regulator_get(&pdev->dev,
						vreg_info->name);

		if (PTR_ERR(vreg_info->hdl) == -EPROBE_DEFER) {
			PCIE_DBG(pcie_dev, "EPROBE_DEFER for VReg:%s\n",
				vreg_info->name);
			return PTR_ERR(vreg_info->hdl);
		}

		if (IS_ERR(vreg_info->hdl)) {
			if (vreg_info->required) {
				PCIE_DBG(pcie_dev, "Vreg %s doesn't exist\n",
					vreg_info->name);
				return PTR_ERR(vreg_info->hdl);
			}

			PCIE_DBG(pcie_dev, "Optional Vreg %s doesn't exist\n",
				vreg_info->name);
			vreg_info->hdl = NULL;
		} else {
			pcie_dev->vreg_n++;
			snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-voltage-level", vreg_info->name);
			prop = of_get_property(pdev->dev.of_node,
						prop_name, &len);
			if (!prop || (len != (3 * sizeof(__be32)))) {
				PCIE_DBG(pcie_dev, "%s %s property\n",
					prop ? "invalid format" :
					"no", prop_name);
			} else {
				vreg_info->max_v = be32_to_cpup(&prop[0]);
				vreg_info->min_v = be32_to_cpup(&prop[1]);
				vreg_info->opt_mode =
					be32_to_cpup(&prop[2]);
			}

			if (!strcmp(vreg_info->name, "vreg-cx"))
				pcie_dev->cx_vreg = vreg_info;
		}
	}

	pcie_dev->gdsc = devm_regulator_get(&pdev->dev, "gdsc-vdd");

	if (IS_ERR(pcie_dev->gdsc)) {
		PCIE_ERR(pcie_dev, "PCIe: RC%d: Failed to get %s GDSC:%ld\n",
			pcie_dev->rc_idx, pdev->name, PTR_ERR(pcie_dev->gdsc));
		if (PTR_ERR(pcie_dev->gdsc) == -EPROBE_DEFER)
			PCIE_DBG(pcie_dev, "PCIe: EPROBE_DEFER for %s GDSC\n",
				pdev->name);
		return PTR_ERR(pcie_dev->gdsc);
	}

	return 0;
}

static int msm_pcie_get_reset(struct msm_pcie_dev_t *pcie_dev)
{
	int i;
	struct msm_pcie_reset_info_t *reset_info;

	for (i = 0; i < MSM_PCIE_MAX_RESET; i++) {
		reset_info = &pcie_dev->reset[i];
		reset_info->hdl = devm_reset_control_get(&pcie_dev->pdev->dev,
							reset_info->name);
		if (IS_ERR(reset_info->hdl)) {
			if (reset_info->required) {
				PCIE_DBG(pcie_dev,
					"Reset %s isn't available:%ld\n",
					reset_info->name,
					PTR_ERR(reset_info->hdl));

				return PTR_ERR(reset_info->hdl);
			}

			PCIE_DBG(pcie_dev, "Ignoring Reset %s\n",
				reset_info->name);
			reset_info->hdl = NULL;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_RESET; i++) {
		reset_info = &pcie_dev->pipe_reset[i];
		reset_info->hdl = devm_reset_control_get(&pcie_dev->pdev->dev,
							reset_info->name);
		if (IS_ERR(reset_info->hdl)) {
			if (reset_info->required) {
				PCIE_DBG(pcie_dev,
					"Pipe Reset %s isn't available:%ld\n",
					reset_info->name,
					PTR_ERR(reset_info->hdl));
				return PTR_ERR(reset_info->hdl);
			}

			PCIE_DBG(pcie_dev, "Ignoring Pipe Reset %s\n",
				reset_info->name);
			reset_info->hdl = NULL;
		}
	}

	return 0;
}

static int msm_pcie_get_bw_scale(struct msm_pcie_dev_t *pcie_dev)
{
	int size = 0;
	struct platform_device *pdev = pcie_dev->pdev;

	of_get_property(pdev->dev.of_node, "qcom,bw-scale", &size);
	if (size) {
		pcie_dev->bw_scale = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
		if (!pcie_dev->bw_scale)
			return -ENOMEM;

		of_property_read_u32_array(pdev->dev.of_node, "qcom,bw-scale",
				(u32 *)pcie_dev->bw_scale, size / sizeof(u32));

		pcie_dev->bw_gen_max = size / sizeof(u32);
	} else {
		PCIE_DBG(pcie_dev, "RC%d: bandwidth scaling is not supported\n",
			pcie_dev->rc_idx);
	}

	return 0;
}

static int msm_pcie_get_phy(struct msm_pcie_dev_t *pcie_dev)
{
	int ret, size = 0;
	struct platform_device *pdev = pcie_dev->pdev;

	of_get_property(pdev->dev.of_node, "qcom,phy-sequence", &size);
	if (!size) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: phy sequence is not present in DT\n",
			pcie_dev->rc_idx);
		return 0;
	}

	pcie_dev->phy_sequence = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!pcie_dev->phy_sequence)
		return -ENOMEM;

	pcie_dev->phy_len = size / sizeof(*pcie_dev->phy_sequence);

	ret = of_property_read_u32_array(pdev->dev.of_node,
				"qcom,phy-sequence",
				(unsigned int *)pcie_dev->phy_sequence,
				size / sizeof(pcie_dev->phy_sequence->offset));
	if (ret) {
		devm_kfree(&pdev->dev, pcie_dev->phy_sequence);
		return -EINVAL;
	}

	return 0;
}

static int msm_pcie_get_iommu_map(struct msm_pcie_dev_t *pcie_dev)
{
	/* iommu map structure */
	struct {
		u32 bdf;
		u32 phandle;
		u32 smmu_sid;
		u32 smmu_sid_len;
	} *map;
	struct platform_device *pdev = pcie_dev->pdev;
	int i, size = 0;

	of_get_property(pdev->dev.of_node, "iommu-map", &size);
	if (!size) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: iommu-map is not present in DT.\n",
			pcie_dev->rc_idx);
		return 0;
	}

	map = kzalloc(size, GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	of_property_read_u32_array(pdev->dev.of_node,
		"iommu-map", (u32 *)map, size / sizeof(u32));

	pcie_dev->sid_info_len = size / (sizeof(*map));
	pcie_dev->sid_info = devm_kcalloc(&pdev->dev, pcie_dev->sid_info_len,
				sizeof(*pcie_dev->sid_info), GFP_KERNEL);
	if (!pcie_dev->sid_info) {
		kfree(map);
		return -ENOMEM;
	}

	for (i = 0; i < pcie_dev->sid_info_len; i++) {
		pcie_dev->sid_info[i].bdf = map[i].bdf;
		pcie_dev->sid_info[i].smmu_sid = map[i].smmu_sid;
		pcie_dev->sid_info[i].pcie_sid =
				pcie_dev->sid_info[i].smmu_sid -
				pcie_dev->smmu_sid_base;
	}

	kfree(map);

	return 0;
}

static int msm_pcie_get_gpio(struct msm_pcie_dev_t *pcie_dev)
{
	int i, ret;

	pcie_dev->gpio_n = 0;
	for (i = 0; i < MSM_PCIE_MAX_GPIO; i++) {
		struct msm_pcie_gpio_info_t *gpio_info = &pcie_dev->gpio[i];

		ret = of_get_named_gpio(pcie_dev->pdev->dev.of_node,
					gpio_info->name, 0);
		if (ret >= 0) {
			gpio_info->num = ret;
			pcie_dev->gpio_n++;
			PCIE_DBG(pcie_dev, "GPIO num for %s is %d\n",
				gpio_info->name, gpio_info->num);
		} else {
			if (gpio_info->required) {
				PCIE_ERR(pcie_dev,
					"Could not get required GPIO %s\n",
					gpio_info->name);
				return ret;
			}

			PCIE_DBG(pcie_dev, "Could not get optional GPIO %s\n",
				gpio_info->name);
		}
	}

	pcie_dev->wake_n = 0;
	if (pcie_dev->gpio[MSM_PCIE_GPIO_WAKE].num)
		pcie_dev->wake_n =
			gpio_to_irq(pcie_dev->gpio[MSM_PCIE_GPIO_WAKE].num);

	return 0;
}

static int msm_pcie_get_reg(struct msm_pcie_dev_t *pcie_dev)
{
	struct resource *res;
	struct msm_pcie_res_info_t *res_info;
	int i;

	for (i = 0; i < MSM_PCIE_MAX_RES; i++) {
		res_info = &pcie_dev->res[i];

		res = platform_get_resource_byname(pcie_dev->pdev,
						IORESOURCE_MEM, res_info->name);
		if (!res) {
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d: no %s resource found.\n",
				pcie_dev->rc_idx, res_info->name);
		} else {
			PCIE_DBG(pcie_dev, "start addr for %s is %pa.\n",
				res_info->name,	&res->start);

			res_info->base = devm_ioremap(&pcie_dev->pdev->dev,
						res->start, resource_size(res));
			if (!res_info->base) {
				PCIE_ERR(pcie_dev,
					"PCIe: RC%d: can't remap %s.\n",
					pcie_dev->rc_idx, res_info->name);
				return -ENOMEM;
			}

			res_info->resource = res;
		}
	}

	pcie_dev->parf = pcie_dev->res[MSM_PCIE_RES_PARF].base;
	pcie_dev->phy = pcie_dev->res[MSM_PCIE_RES_PHY].base;
	pcie_dev->elbi = pcie_dev->res[MSM_PCIE_RES_ELBI].base;
	pcie_dev->iatu = pcie_dev->res[MSM_PCIE_RES_IATU].base;
	pcie_dev->dm_core = pcie_dev->res[MSM_PCIE_RES_DM_CORE].base;
	pcie_dev->conf = pcie_dev->res[MSM_PCIE_RES_CONF].base;
	pcie_dev->tcsr = pcie_dev->res[MSM_PCIE_RES_TCSR].base;
	pcie_dev->rumi = pcie_dev->res[MSM_PCIE_RES_RUMI].base;

	return 0;
}

static int msm_pcie_get_resources(struct msm_pcie_dev_t *dev,
					struct platform_device *pdev)
{
	int i, ret = 0;
	struct resource *res;
	struct msm_pcie_irq_info_t *irq_info;

	PCIE_DBG(dev, "PCIe: RC%d: entry\n", dev->rc_idx);

	dev->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!dev->bus_scale_table) {
		PCIE_DBG(dev, "PCIe: RC%d: No bus scale table for %s\n",
			dev->rc_idx, dev->pdev->name);
		dev->bus_client = 0;
	} else {
		dev->bus_client =
			msm_bus_scale_register_client(dev->bus_scale_table);
		if (!dev->bus_client) {
			PCIE_ERR(dev,
				"PCIe: RC%d:Failed to register bus client %s\n",
				dev->rc_idx, dev->pdev->name);
			msm_bus_cl_clear_pdata(dev->bus_scale_table);
			return -ENODEV;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_IRQ; i++) {
		irq_info = &dev->irq[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							   irq_info->name);
		if (!res) {
			PCIE_DBG(dev, "PCIe: RC%d: can't find IRQ # for %s.\n",
				dev->rc_idx, irq_info->name);
		} else {
			irq_info->num = res->start;
			PCIE_DBG(dev, "IRQ # for %s is %d.\n", irq_info->name,
					irq_info->num);
		}
	}

	ret = msm_pcie_get_clk(dev);
	if (ret)
		return ret;

	ret = msm_pcie_get_vreg(dev);
	if (ret)
		return ret;

	ret = msm_pcie_get_reset(dev);
	if (ret)
		return ret;

	ret = msm_pcie_get_bw_scale(dev);
	if (ret)
		return ret;

	ret = msm_pcie_get_phy(dev);
	if (ret)
		return ret;

	ret = msm_pcie_get_iommu_map(dev);
	if (ret)
		return ret;

	ret = msm_pcie_get_gpio(dev);
	if (ret)
		return ret;

	ret = msm_pcie_get_reg(dev);
	if (ret)
		return ret;

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);

	return 0;
}

static void msm_pcie_release_resources(struct msm_pcie_dev_t *dev)
{
	dev->parf = NULL;
	dev->elbi = NULL;
	dev->iatu = NULL;
	dev->dm_core = NULL;
	dev->conf = NULL;
	dev->tcsr = NULL;
	dev->rumi = NULL;
}

static void msm_pcie_scale_link_bandwidth(struct msm_pcie_dev_t *pcie_dev,
					u16 target_link_speed)
{
	struct msm_pcie_bw_scale_info_t *bw_scale;
	u32 index = target_link_speed - PCI_EXP_LNKCTL2_TLS_2_5GT;

	if (!pcie_dev->bw_scale)
		return;

	if (index >= pcie_dev->bw_gen_max) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d: invalid target link speed: %d\n",
			pcie_dev->rc_idx, target_link_speed);
		return;
	}

	bw_scale = &pcie_dev->bw_scale[index];

	if (pcie_dev->cx_vreg)
		regulator_set_voltage(pcie_dev->cx_vreg->hdl,
					bw_scale->cx_vreg_min,
					pcie_dev->cx_vreg->max_v);

	if (pcie_dev->rate_change_clk) {
		mutex_lock(&pcie_dev->clk_lock);

		/* it is okay to always scale up */
		clk_set_rate(pcie_dev->rate_change_clk->hdl,
				RATE_CHANGE_100MHZ);

		if (bw_scale->rate_change_freq == RATE_CHANGE_100MHZ)
			pcie_drv.rate_change_vote |= BIT(pcie_dev->rc_idx);
		else
			pcie_drv.rate_change_vote &= ~BIT(pcie_dev->rc_idx);

		/* scale down to 19.2MHz if no one needs 100MHz */
		if (!pcie_drv.rate_change_vote)
			clk_set_rate(pcie_dev->rate_change_clk->hdl,
					RATE_CHANGE_19P2MHZ);

		mutex_unlock(&pcie_dev->clk_lock);
	}
}

static int msm_pcie_link_train(struct msm_pcie_dev_t *dev)
{
	int link_check_count = 0;
	uint32_t val;

	msm_pcie_write_reg_field(dev->dm_core,
		PCIE_GEN3_GEN2_CTRL, 0x1f00, 1);

	msm_pcie_write_mask(dev->dm_core,
		PCIE_GEN3_EQ_CONTROL, 0x20);

	msm_pcie_write_mask(dev->dm_core +
		PCIE_GEN3_RELATED, BIT(0), 0);

	/* configure PCIe preset */
	msm_pcie_write_reg_field(dev->dm_core,
		PCIE_GEN3_MISC_CONTROL, BIT(0), 1);
	msm_pcie_write_reg(dev->dm_core,
		PCIE_GEN3_SPCIE_CAP, dev->core_preset);
	msm_pcie_write_reg_field(dev->dm_core,
		PCIE_GEN3_MISC_CONTROL, BIT(0), 0);

	if (msm_pcie_force_gen1 & BIT(dev->rc_idx))
		dev->target_link_speed = GEN1_SPEED;

	if (dev->target_link_speed)
		msm_pcie_write_reg_field(dev->dm_core,
			PCIE20_CAP + PCI_EXP_LNKCTL2,
			PCI_EXP_LNKCAP_SLS, dev->target_link_speed);

	/* set max tlp read size */
	msm_pcie_write_reg_field(dev->dm_core, PCIE20_DEVICE_CONTROL_STATUS,
				0x7000, dev->tlp_rd_size);

	/* enable link training */
	msm_pcie_write_mask(dev->parf + PCIE20_PARF_LTSSM, 0, BIT(8));

	PCIE_DBG(dev, "%s", "check if link is up\n");

	if (msm_pcie_link_check_max_count & BIT(dev->rc_idx))
		dev->link_check_max_count = msm_pcie_link_check_max_count >> 4;

	/* Wait for up to 100ms for the link to come up */
	do {
		usleep_range(LINK_UP_TIMEOUT_US_MIN, LINK_UP_TIMEOUT_US_MAX);
		val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
		PCIE_DBG(dev, "PCIe RC%d: LTSSM_STATE: %s\n",
			dev->rc_idx, TO_LTSSM_STR((val >> 12) & 0x3f));
	} while ((!(val & XMLH_LINK_UP) ||
		!msm_pcie_confirm_linkup(dev, false, false, NULL))
		&& (link_check_count++ < dev->link_check_max_count));

	if ((val & XMLH_LINK_UP) &&
		msm_pcie_confirm_linkup(dev, false, false, NULL)) {
		PCIE_DBG(dev, "Link is up after %d checkings\n",
			link_check_count);
		PCIE_INFO(dev, "PCIe RC%d link initialized\n", dev->rc_idx);
	} else {
		PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
			dev->rc_idx);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
			dev->gpio[MSM_PCIE_GPIO_PERST].on);
		PCIE_ERR(dev, "PCIe RC%d link initialization failed\n",
			dev->rc_idx);
		return MSM_PCIE_ERROR;
	}

	if (dev->bw_scale) {
		u32 index;
		u32 current_link_speed;
		struct msm_pcie_bw_scale_info_t *bw_scale;

		/*
		 * check if the link up GEN speed is less than the max/default
		 * supported. If it is, scale down CX corner and rate change
		 * clock accordingly.
		 */
		current_link_speed = readl_relaxed(dev->dm_core +
						PCIE20_CAP_LINKCTRLSTATUS);
		current_link_speed = ((current_link_speed >> 16) &
					PCI_EXP_LNKSTA_CLS);

		index = current_link_speed - PCI_EXP_LNKCTL2_TLS_2_5GT;
		if (index >= dev->bw_gen_max) {
			PCIE_ERR(dev,
				"PCIe: RC%d: unsupported gen speed: %d\n",
				dev->rc_idx, current_link_speed);
			return 0;
		}

		bw_scale = &dev->bw_scale[index];

		if (bw_scale->cx_vreg_min < dev->cx_vreg->min_v) {
			msm_pcie_write_reg_field(dev->dm_core,
				PCIE20_CAP + PCI_EXP_LNKCTL2,
				PCI_EXP_LNKCAP_SLS, current_link_speed);
			msm_pcie_scale_link_bandwidth(dev, current_link_speed);
		}
	}

	return 0;
}

static int msm_pcie_enable(struct msm_pcie_dev_t *dev)
{
	int ret = 0;
	uint32_t val;
	unsigned long ep_up_timeout = 0;

	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	mutex_lock(&dev->setup_lock);

	if (dev->link_status == MSM_PCIE_LINK_ENABLED) {
		PCIE_ERR(dev, "PCIe: the link of RC%d is already enabled\n",
			dev->rc_idx);
		goto out;
	}

	/* assert PCIe reset link to keep EP in reset */

	PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
		dev->rc_idx);
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable power */
	ret = msm_pcie_vreg_init(dev);
	if (ret)
		goto out;

	/* enable clocks */
	ret = msm_pcie_clk_init(dev);
	/* ensure that changes propagated to the hardware */
	wmb();
	if (ret)
		goto clk_fail;

	/* RUMI PCIe reset sequence */
	if (dev->rumi_init)
		dev->rumi_init(dev);

	/* configure PCIe to RC mode */
	msm_pcie_write_reg(dev->parf, PCIE20_PARF_DEVICE_TYPE, 0x4);

	/* enable l1 mode, clear bit 5 (REQ_NOT_ENTR_L1) */
	if (dev->l1_supported)
		msm_pcie_write_mask(dev->parf + PCIE20_PARF_PM_CTRL, BIT(5), 0);

	/* enable PCIe clocks and resets */
	msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, BIT(0), 0);

	/* change DBI base address */
	writel_relaxed(0, dev->parf + PCIE20_PARF_DBI_BASE_ADDR);

	writel_relaxed(0x365E, dev->parf + PCIE20_PARF_SYS_CTRL);

	msm_pcie_write_mask(dev->parf + PCIE20_PARF_MHI_CLOCK_RESET_CTRL,
				0, BIT(4));

	/* enable selected IRQ */
	msm_pcie_write_reg(dev->parf, PCIE20_PARF_INT_ALL_MASK, 0);

	msm_pcie_write_mask(dev->parf + PCIE20_PARF_INT_ALL_MASK, 0,
				BIT(MSM_PCIE_INT_EVT_LINK_DOWN) |
				BIT(MSM_PCIE_INT_EVT_L1SUB_TIMEOUT) |
				BIT(MSM_PCIE_INT_EVT_AER_LEGACY) |
				BIT(MSM_PCIE_INT_EVT_AER_ERR) |
				BIT(MSM_PCIE_INT_EVT_MSI_0) |
				BIT(MSM_PCIE_INT_EVT_MSI_1) |
				BIT(MSM_PCIE_INT_EVT_MSI_2) |
				BIT(MSM_PCIE_INT_EVT_MSI_3) |
				BIT(MSM_PCIE_INT_EVT_MSI_4) |
				BIT(MSM_PCIE_INT_EVT_MSI_5) |
				BIT(MSM_PCIE_INT_EVT_MSI_6) |
				BIT(MSM_PCIE_INT_EVT_MSI_7));

	PCIE_INFO(dev, "PCIe: RC%d: PCIE20_PARF_INT_ALL_MASK: 0x%x\n",
		dev->rc_idx,
		readl_relaxed(dev->parf + PCIE20_PARF_INT_ALL_MASK));

	writel_relaxed(dev->slv_addr_space_size, dev->parf +
		PCIE20_PARF_SLV_ADDR_SPACE_SIZE);

	val = dev->wr_halt_size ? dev->wr_halt_size :
		readl_relaxed(dev->parf + PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT);
	msm_pcie_write_reg(dev->parf, PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT,
				BIT(31) | val);

	/* init PCIe PHY */
	ret = pcie_phy_init(dev);
	if (ret)
		goto link_fail;

	usleep_range(dev->ep_latency * 1000, dev->ep_latency * 1000);

	if (dev->gpio[MSM_PCIE_GPIO_EP].num)
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_EP].num,
				dev->gpio[MSM_PCIE_GPIO_EP].on);

	/* de-assert PCIe reset link to bring EP out of reset */

	PCIE_INFO(dev, "PCIe: Release the reset of endpoint of RC%d.\n",
		dev->rc_idx);
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(dev->perst_delay_us_min, dev->perst_delay_us_max);

	ep_up_timeout = jiffies + usecs_to_jiffies(EP_UP_TIMEOUT_US);

	ret = msm_pcie_link_train(dev);
	if (ret)
		goto link_fail;

	dev->link_status = MSM_PCIE_LINK_ENABLED;
	dev->power_on = true;
	dev->suspending = false;
	dev->link_turned_on_counter++;

	if (dev->switch_latency) {
		PCIE_DBG(dev, "switch_latency: %dms\n",
			dev->switch_latency);
		if (dev->switch_latency <= SWITCH_DELAY_MAX)
			usleep_range(dev->switch_latency * 1000,
				dev->switch_latency * 1000);
		else
			msleep(dev->switch_latency);
	}

	msm_pcie_config_sid(dev);
	msm_pcie_config_controller(dev);

	/* check endpoint configuration space is accessible */
	while (time_before(jiffies, ep_up_timeout)) {
		if (readl_relaxed(dev->conf) != PCIE_LINK_DOWN)
			break;
		usleep_range(EP_UP_TIMEOUT_US_MIN, EP_UP_TIMEOUT_US_MAX);
	}

	if (readl_relaxed(dev->conf) != PCIE_LINK_DOWN) {
		PCIE_DBG(dev,
			"PCIe: RC%d: endpoint config space is accessible\n",
			dev->rc_idx);
	} else {
		PCIE_ERR(dev,
			"PCIe: RC%d: endpoint config space is not accessible\n",
			dev->rc_idx);
		dev->link_status = MSM_PCIE_LINK_DISABLED;
		dev->power_on = false;
		dev->link_turned_off_counter++;
		ret = -ENODEV;
		goto link_fail;
	}

	if (dev->enumerated)
		msm_pcie_config_link_pm(dev, true);

	goto out;

link_fail:
	if (msm_pcie_keep_resources_on & BIT(dev->rc_idx))
		goto out;

	if (dev->gpio[MSM_PCIE_GPIO_EP].num)
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_EP].num,
				1 - dev->gpio[MSM_PCIE_GPIO_EP].on);

	if (dev->phy_power_down_offset)
		msm_pcie_write_reg(dev->phy, dev->phy_power_down_offset, 0);

	msm_pcie_pipe_clk_deinit(dev);
	msm_pcie_clk_deinit(dev);
clk_fail:
	msm_pcie_vreg_deinit(dev);
out:
	mutex_unlock(&dev->setup_lock);

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);

	return ret;
}

static void msm_pcie_disable(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG(dev, "RC%d: entry\n", dev->rc_idx);

	mutex_lock(&dev->setup_lock);

	if (!dev->power_on) {
		PCIE_DBG(dev,
			"PCIe: the link of RC%d is already power down.\n",
			dev->rc_idx);
		mutex_unlock(&dev->setup_lock);
		return;
	}

	dev->link_status = MSM_PCIE_LINK_DISABLED;
	dev->power_on = false;
	dev->link_turned_off_counter++;

	PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
		dev->rc_idx);

	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);

	if (dev->phy_power_down_offset)
		msm_pcie_write_reg(dev->phy, dev->phy_power_down_offset, 0);

	msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, 0,
				BIT(0));
	msm_pcie_clk_deinit(dev);
	msm_pcie_vreg_deinit(dev);
	msm_pcie_pipe_clk_deinit(dev);

	if (dev->gpio[MSM_PCIE_GPIO_EP].num)
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_EP].num,
				1 - dev->gpio[MSM_PCIE_GPIO_EP].on);

	mutex_unlock(&dev->setup_lock);

	PCIE_DBG(dev, "RC%d: exit\n", dev->rc_idx);
}

static void msm_pcie_config_ep_aer(struct msm_pcie_dev_t *dev,
				struct msm_pcie_device_info *ep_dev_info)
{
	u32 val;
	void __iomem *ep_base = ep_dev_info->conf_base;
	u32 current_offset = readl_relaxed(ep_base + PCIE_CAP_PTR_OFFSET) &
						0xff;

	while (current_offset) {
		if (msm_pcie_check_align(dev, current_offset))
			return;

		val = readl_relaxed(ep_base + current_offset);
		if ((val & 0xff) == PCIE20_CAP_ID) {
			ep_dev_info->dev_ctrlstts_offset =
				current_offset + 0x8;
			break;
		}
		current_offset = (val >> 8) & 0xff;
	}

	if (!ep_dev_info->dev_ctrlstts_offset) {
		PCIE_DBG(dev,
			"RC%d endpoint does not support PCIe cap registers\n",
			dev->rc_idx);
		return;
	}

	PCIE_DBG2(dev, "RC%d: EP dev_ctrlstts_offset: 0x%x\n",
		dev->rc_idx, ep_dev_info->dev_ctrlstts_offset);

	/* Enable AER on EP */
	msm_pcie_write_mask(ep_base + ep_dev_info->dev_ctrlstts_offset, 0,
				BIT(3)|BIT(2)|BIT(1)|BIT(0));

	PCIE_DBG(dev, "EP's PCIE20_CAP_DEVCTRLSTATUS:0x%x\n",
		readl_relaxed(ep_base + ep_dev_info->dev_ctrlstts_offset));
}

static int msm_pcie_config_device_table(struct device *dev, void *pdev)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *) pdev;
	struct msm_pcie_device_info *dev_table_t = pcie_dev->pcidev_table;
	struct resource *axi_conf = pcie_dev->res[MSM_PCIE_RES_CONF].resource;
	int ret = 0;
	u32 rc_idx = pcie_dev->rc_idx;
	u32 i, index;
	u32 bdf = 0;
	u8 type;
	u32 h_type;
	u32 bme;

	if (!pcidev) {
		PCIE_ERR(pcie_dev,
			"PCIe: Did not find PCI device in list for RC%d.\n",
			pcie_dev->rc_idx);
		return -ENODEV;
	}

	PCIE_DBG(pcie_dev,
		"PCI device found: vendor-id:0x%x device-id:0x%x\n",
		pcidev->vendor, pcidev->device);

	if (!pcidev->bus->number)
		return ret;

	bdf = BDF_OFFSET(pcidev->bus->number, pcidev->devfn);
	type = pcidev->bus->number == 1 ?
		PCIE20_CTRL1_TYPE_CFG0 : PCIE20_CTRL1_TYPE_CFG1;

	for (i = 0; i < (MAX_RC_NUM * MAX_DEVICE_NUM); i++) {
		if (msm_pcie_dev_tbl[i].bdf == bdf &&
			!msm_pcie_dev_tbl[i].dev) {
			for (index = 0; index < MAX_DEVICE_NUM; index++) {
				if (dev_table_t[index].bdf == bdf) {
					msm_pcie_dev_tbl[i].dev = pcidev;
					msm_pcie_dev_tbl[i].domain = rc_idx;
					msm_pcie_dev_tbl[i].conf_base =
						pcie_dev->conf + index * SZ_4K;
					msm_pcie_dev_tbl[i].phy_address =
						axi_conf->start + index * SZ_4K;

					dev_table_t[index].dev = pcidev;
					dev_table_t[index].domain = rc_idx;
					dev_table_t[index].conf_base =
						pcie_dev->conf + index * SZ_4K;
					dev_table_t[index].phy_address =
						axi_conf->start + index * SZ_4K;

					msm_pcie_iatu_config(pcie_dev, index,
						type,
						dev_table_t[index].phy_address,
						dev_table_t[index].phy_address
						+ SZ_4K - 1,
						bdf);

					h_type = readl_relaxed(
						dev_table_t[index].conf_base +
						PCIE20_HEADER_TYPE);

					bme = readl_relaxed(
						dev_table_t[index].conf_base +
						PCIE20_COMMAND_STATUS);

					if (h_type & (1 << 16)) {
						pci_write_config_dword(pcidev,
							PCIE20_COMMAND_STATUS,
							bme | 0x06);
					} else {
						pcie_dev->num_ep++;
						dev_table_t[index].registered =
							false;
					}

					if (pcie_dev->num_ep > 1)
						pcie_dev->pending_ep_reg = true;

					if (pcie_dev->aer_enable)
						msm_pcie_config_ep_aer(pcie_dev,
							&dev_table_t[index]);

					break;
				}
			}
			if (index == MAX_DEVICE_NUM) {
				PCIE_ERR(pcie_dev,
					"RC%d PCI device table is full.\n",
					rc_idx);
				ret = index;
			} else {
				break;
			}
		} else if (msm_pcie_dev_tbl[i].bdf == bdf &&
			pcidev == msm_pcie_dev_tbl[i].dev) {
			break;
		}
	}
	if (i == MAX_RC_NUM * MAX_DEVICE_NUM) {
		PCIE_ERR(pcie_dev,
			"Global PCI device table is full: %d elements.\n",
			i);
		PCIE_ERR(pcie_dev,
			"Bus number is 0x%x\nDevice number is 0x%x\n",
			pcidev->bus->number, pcidev->devfn);
		ret = i;
	}
	return ret;
}

static void msm_pcie_config_sid(struct msm_pcie_dev_t *dev)
{
	void __iomem *bdf_to_sid_base = dev->parf +
		PCIE20_PARF_BDF_TO_SID_TABLE_N;
	int i;

	if (!dev->sid_info)
		return;

	/* Registers need to be zero out first */
	memset_io(bdf_to_sid_base, 0, CRC8_TABLE_SIZE * sizeof(u32));

	if (dev->enumerated) {
		for (i = 0; i < dev->sid_info_len; i++)
			writel_relaxed(dev->sid_info[i].value,
				bdf_to_sid_base + dev->sid_info[i].hash *
				sizeof(u32));
		return;
	}

	/* initial setup for boot */
	for (i = 0; i < dev->sid_info_len; i++) {
		struct msm_pcie_sid_info_t *sid_info = &dev->sid_info[i];
		u32 val;
		u8 hash;
		u16 bdf_be = cpu_to_be16(sid_info->bdf);

		hash = crc8(msm_pcie_crc8_table, (u8 *)&bdf_be, sizeof(bdf_be),
			0);

		val = readl_relaxed(bdf_to_sid_base + hash * sizeof(u32));

		/* if there is a collision, look for next available entry */
		while (val) {
			u8 current_hash = hash++;
			u8 next_mask = 0xff;

			/* if NEXT is NULL then update current entry */
			if (!(val & next_mask)) {
				int j;

				val |= (u32)hash;
				writel_relaxed(val, bdf_to_sid_base +
					current_hash * sizeof(u32));

				/* sid_info of current hash and update it */
				for (j = 0; j < dev->sid_info_len; j++) {
					if (dev->sid_info[j].hash !=
						current_hash)
						continue;

					dev->sid_info[j].next_hash = hash;
					dev->sid_info[j].value = val;
					break;
				}
			}

			val = readl_relaxed(bdf_to_sid_base +
				hash * sizeof(u32));
		}

		/* BDF [31:16] | SID [15:8] | NEXT [7:0] */
		val = sid_info->bdf << 16 | sid_info->pcie_sid << 8 | 0;
		writel_relaxed(val, bdf_to_sid_base + hash * sizeof(u32));

		sid_info->hash = hash;
		sid_info->value = val;
	}
}

int msm_pcie_enumerate(u32 rc_idx)
{
	int ret = 0, bus_ret = 0;
	struct msm_pcie_dev_t *dev = &msm_pcie_dev[rc_idx];
	struct pci_dev *pcidev = NULL;
	struct pci_host_bridge *bridge;
	bool found = false;
	struct pci_bus *bus;
	resource_size_t iobase = 0;
	u32 ids, vendor_id, device_id;
	LIST_HEAD(res);

	mutex_lock(&dev->enumerate_lock);

	PCIE_DBG(dev, "Enumerate RC%d\n", rc_idx);

	if (!dev->drv_ready) {
		PCIE_DBG(dev,
			"PCIe: RC%d: has not been successfully probed yet\n",
			rc_idx);
		ret = -EPROBE_DEFER;
		goto out;
	}

	if (dev->enumerated) {
		PCIE_ERR(dev, "PCIe: RC%d: has already been enumerated.\n",
			dev->rc_idx);
		goto out;
	}

	ret = msm_pcie_enable(dev);
	if (ret) {
		PCIE_ERR(dev, "PCIe: RC%d: failed to enable\n", dev->rc_idx);
		goto out;
	}

	/* kick start ARM PCI configuration framework */
	ids = readl_relaxed(dev->dm_core);
	vendor_id = ids & 0xffff;
	device_id = (ids & 0xffff0000) >> 16;

	PCIE_DBG(dev, "PCIe: RC%d: vendor-id:0x%x device_id:0x%x\n",
		dev->rc_idx, vendor_id, device_id);

	bridge = devm_pci_alloc_host_bridge(&dev->pdev->dev, sizeof(*dev));
	if (!bridge) {
		ret = -ENOMEM;
		goto out;
	}

	ret = devm_of_pci_get_host_bridge_resources(&dev->pdev->dev, 0, 0xff,
						&res, &iobase);
	if (ret) {
		PCIE_ERR(dev,
			"PCIe: RC%d: failed to get host bridge resources. ret: %d\n",
			dev->rc_idx, ret);
		goto out;
	}

	ret = devm_request_pci_bus_resources(&dev->pdev->dev, &res);
	if (ret) {
		PCIE_ERR(dev,
			"PCIe: RC%d: failed to request pci bus resources %d\n",
			dev->rc_idx, ret);
		goto out;
	}

	if (IS_ENABLED(CONFIG_PCI_MSM_MSI)) {
		ret = msm_msi_init(&dev->pdev->dev);
		if (ret)
			goto out;
	}

	list_splice_init(&res, &bridge->windows);
	bridge->dev.parent = &dev->pdev->dev;
	bridge->sysdata = dev;
	bridge->busnr = 0;
	bridge->ops = &msm_pcie_ops;
	bridge->map_irq = of_irq_parse_and_map_pci;
	bridge->swizzle_irq = pci_common_swizzle;

	ret = pci_scan_root_bus_bridge(bridge);
	if (ret) {
		PCIE_ERR(dev, "PCIe: RC%d: failed to scan root bus %d\n",
			dev->rc_idx, ret);
		goto out;
	}

	bus = bridge->bus;

	msm_pcie_fixup_irqs(dev);
	pci_assign_unassigned_bus_resources(bus);
	pci_bus_add_devices(bus);

	dev->enumerated = true;
	schedule_work(&pcie_drv.drv_connect);

	msm_pcie_write_mask(dev->dm_core +
		PCIE20_COMMAND_STATUS, 0, BIT(2)|BIT(1));

	if (dev->cpl_timeout && dev->bridge_found)
		msm_pcie_write_reg_field(dev->dm_core,
			PCIE20_DEVICE_CONTROL2_STATUS2, 0xf, dev->cpl_timeout);

	if (dev->shadow_en) {
		u32 val = readl_relaxed(dev->dm_core + PCIE20_COMMAND_STATUS);

		PCIE_DBG(dev, "PCIE20_COMMAND_STATUS:0x%x\n", val);
		dev->rc_shadow[PCIE20_COMMAND_STATUS / 4] = val;
	}

	do {
		pcidev = pci_get_device(vendor_id, device_id, pcidev);
		if (pcidev && (dev == (struct msm_pcie_dev_t *)
			PCIE_BUS_PRIV_DATA(pcidev->bus))) {
			dev->dev = pcidev;
			found = true;
		}
	} while (!found && pcidev);

	if (!pcidev) {
		PCIE_ERR(dev, "PCIe: RC%d: Did not find PCI device.\n",
			dev->rc_idx);
		ret = -ENODEV;
		goto out;
	}

	bus_ret = bus_for_each_dev(&pci_bus_type, NULL, dev,
			&msm_pcie_config_device_table);
	if (bus_ret) {
		PCIE_ERR(dev, "PCIe: RC%d: Failed to set up device table\n",
			dev->rc_idx);
		ret = -ENODEV;
		goto out;
	}

	msm_pcie_check_l1ss_support_all(dev);
	msm_pcie_config_link_pm(dev, true);
out:
	mutex_unlock(&dev->enumerate_lock);

	return ret;
}
EXPORT_SYMBOL(msm_pcie_enumerate);

static void msm_pcie_notify_client(struct msm_pcie_dev_t *dev,
					enum msm_pcie_event event)
{
	if (dev->event_reg && dev->event_reg->callback &&
		(dev->event_reg->events & event)) {
		struct msm_pcie_notify *notify = &dev->event_reg->notify;

		notify->event = event;
		notify->user = dev->event_reg->user;
		PCIE_DBG(dev, "PCIe: callback RC%d for event %d\n",
			dev->rc_idx, event);
		dev->event_reg->callback(notify);

		if ((dev->event_reg->options & MSM_PCIE_CONFIG_NO_RECOVERY) &&
				(event == MSM_PCIE_EVENT_LINKDOWN)) {
			dev->user_suspend = true;
			PCIE_DBG(dev,
				"PCIe: Client of RC%d will recover the link later.\n",
				dev->rc_idx);
			return;
		}
	} else {
		PCIE_DBG2(dev,
			"PCIe: Client of RC%d does not have registration for event %d\n",
			dev->rc_idx, event);
	}
}

static void handle_wake_func(struct work_struct *work)
{
	int i, ret;
	struct msm_pcie_dev_t *dev = container_of(work, struct msm_pcie_dev_t,
					handle_wake_work);

	PCIE_DBG(dev, "PCIe: Wake work for RC%d\n", dev->rc_idx);

	mutex_lock(&dev->recovery_lock);

	if (!dev->enumerated) {
		PCIE_DBG(dev,
			"PCIe: Start enumeration for RC%d upon the wake from endpoint.\n",
			dev->rc_idx);

		ret = msm_pcie_enumerate(dev->rc_idx);
		if (ret) {
			PCIE_ERR(dev,
				"PCIe: failed to enable RC%d upon wake request from the device.\n",
				dev->rc_idx);
			goto out;
		}

		if (dev->num_ep > 1) {
			for (i = 0; i < MAX_DEVICE_NUM; i++) {
				dev->event_reg = dev->pcidev_table[i].event_reg;

				if ((dev->link_status == MSM_PCIE_LINK_ENABLED)
					&& dev->event_reg &&
					dev->event_reg->callback &&
					(dev->event_reg->events &
					MSM_PCIE_EVENT_LINKUP)) {
					struct msm_pcie_notify *notify =
						&dev->event_reg->notify;
					notify->event = MSM_PCIE_EVENT_LINKUP;
					notify->user = dev->event_reg->user;
					PCIE_DBG(dev,
						"PCIe: Linkup callback for RC%d after enumeration is successful in wake IRQ handling\n",
						dev->rc_idx);
					dev->event_reg->callback(notify);
				}
			}
		} else {
			if ((dev->link_status == MSM_PCIE_LINK_ENABLED) &&
				dev->event_reg && dev->event_reg->callback &&
				(dev->event_reg->events &
				MSM_PCIE_EVENT_LINKUP)) {
				struct msm_pcie_notify *notify =
						&dev->event_reg->notify;
				notify->event = MSM_PCIE_EVENT_LINKUP;
				notify->user = dev->event_reg->user;
				PCIE_DBG(dev,
					"PCIe: Linkup callback for RC%d after enumeration is successful in wake IRQ handling\n",
					dev->rc_idx);
				dev->event_reg->callback(notify);
			} else {
				PCIE_DBG(dev,
					"PCIe: Client of RC%d does not have registration for linkup event.\n",
					dev->rc_idx);
			}
		}
		goto out;
	} else {
		PCIE_ERR(dev,
			"PCIe: The enumeration for RC%d has already been done.\n",
			dev->rc_idx);
		goto out;
	}

out:
	mutex_unlock(&dev->recovery_lock);
}

static irqreturn_t handle_aer_irq(int irq, void *data)
{
	struct msm_pcie_dev_t *dev = data;

	int corr_val = 0, uncorr_val = 0, rc_err_status = 0;
	int ep_corr_val = 0, ep_uncorr_val = 0;
	int rc_dev_ctrlstts = 0, ep_dev_ctrlstts = 0;
	u32 ep_dev_ctrlstts_offset = 0;
	int i, j, ep_src_bdf = 0;
	void __iomem *ep_base = NULL;

	PCIE_DBG2(dev,
		"AER Interrupt handler fired for RC%d irq %d\nrc_corr_counter: %lu\nrc_non_fatal_counter: %lu\nrc_fatal_counter: %lu\nep_corr_counter: %lu\nep_non_fatal_counter: %lu\nep_fatal_counter: %lu\n",
		dev->rc_idx, irq, dev->rc_corr_counter,
		dev->rc_non_fatal_counter, dev->rc_fatal_counter,
		dev->ep_corr_counter, dev->ep_non_fatal_counter,
		dev->ep_fatal_counter);

	uncorr_val = readl_relaxed(dev->dm_core +
				PCIE20_AER_UNCORR_ERR_STATUS_REG);
	corr_val = readl_relaxed(dev->dm_core +
				PCIE20_AER_CORR_ERR_STATUS_REG);
	rc_err_status = readl_relaxed(dev->dm_core +
				PCIE20_AER_ROOT_ERR_STATUS_REG);
	rc_dev_ctrlstts = readl_relaxed(dev->dm_core +
				PCIE20_CAP_DEVCTRLSTATUS);

	if (uncorr_val)
		PCIE_DBG(dev, "RC's PCIE20_AER_UNCORR_ERR_STATUS_REG:0x%x\n",
				uncorr_val);
	if (corr_val && (dev->rc_corr_counter < corr_counter_limit))
		PCIE_DBG(dev, "RC's PCIE20_AER_CORR_ERR_STATUS_REG:0x%x\n",
				corr_val);

	if ((rc_dev_ctrlstts >> 18) & 0x1)
		dev->rc_fatal_counter++;
	if ((rc_dev_ctrlstts >> 17) & 0x1)
		dev->rc_non_fatal_counter++;
	if ((rc_dev_ctrlstts >> 16) & 0x1)
		dev->rc_corr_counter++;

	msm_pcie_write_mask(dev->dm_core + PCIE20_CAP_DEVCTRLSTATUS, 0,
				BIT(18)|BIT(17)|BIT(16));

	if (dev->link_status == MSM_PCIE_LINK_DISABLED) {
		PCIE_DBG2(dev, "RC%d link is down\n", dev->rc_idx);
		goto out;
	}

	for (i = 0; i < 2; i++) {
		if (i)
			ep_src_bdf = readl_relaxed(dev->dm_core +
				PCIE20_AER_ERR_SRC_ID_REG) & ~0xffff;
		else
			ep_src_bdf = (readl_relaxed(dev->dm_core +
				PCIE20_AER_ERR_SRC_ID_REG) & 0xffff) << 16;

		if (!ep_src_bdf)
			continue;

		for (j = 0; j < MAX_DEVICE_NUM; j++) {
			if (ep_src_bdf == dev->pcidev_table[j].bdf) {
				PCIE_DBG2(dev,
					"PCIe: %s Error from Endpoint: %02x:%02x.%01x\n",
					i ? "Uncorrectable" : "Correctable",
					dev->pcidev_table[j].bdf >> 24,
					dev->pcidev_table[j].bdf >> 19 & 0x1f,
					dev->pcidev_table[j].bdf >> 16 & 0x07);
				ep_base = dev->pcidev_table[j].conf_base;
				ep_dev_ctrlstts_offset =
				dev->pcidev_table[j].dev_ctrlstts_offset;
				break;
			}
		}

		if (!ep_base) {
			PCIE_ERR(dev,
				"PCIe: RC%d no endpoint found for reported error\n",
				dev->rc_idx);
			goto out;
		}

		ep_uncorr_val = readl_relaxed(ep_base +
					PCIE20_AER_UNCORR_ERR_STATUS_REG);
		ep_corr_val = readl_relaxed(ep_base +
					PCIE20_AER_CORR_ERR_STATUS_REG);
		ep_dev_ctrlstts = readl_relaxed(ep_base +
					ep_dev_ctrlstts_offset);

		if (ep_uncorr_val)
			PCIE_DBG(dev,
				"EP's PCIE20_AER_UNCORR_ERR_STATUS_REG:0x%x\n",
				ep_uncorr_val);
		if (ep_corr_val && (dev->ep_corr_counter < corr_counter_limit))
			PCIE_DBG(dev,
				"EP's PCIE20_AER_CORR_ERR_STATUS_REG:0x%x\n",
				ep_corr_val);

		if ((ep_dev_ctrlstts >> 18) & 0x1)
			dev->ep_fatal_counter++;
		if ((ep_dev_ctrlstts >> 17) & 0x1)
			dev->ep_non_fatal_counter++;
		if ((ep_dev_ctrlstts >> 16) & 0x1)
			dev->ep_corr_counter++;

		msm_pcie_write_mask(ep_base + ep_dev_ctrlstts_offset, 0,
					BIT(18)|BIT(17)|BIT(16));

		msm_pcie_write_reg_field(ep_base,
				PCIE20_AER_UNCORR_ERR_STATUS_REG,
				0x3fff031, 0x3fff031);
		msm_pcie_write_reg_field(ep_base,
				PCIE20_AER_CORR_ERR_STATUS_REG,
				0xf1c1, 0xf1c1);
	}
out:
	if (((dev->rc_corr_counter < corr_counter_limit) &&
		(dev->ep_corr_counter < corr_counter_limit)) ||
		uncorr_val || ep_uncorr_val)
		PCIE_DBG(dev, "RC's PCIE20_AER_ROOT_ERR_STATUS_REG:0x%x\n",
				rc_err_status);
	msm_pcie_write_reg_field(dev->dm_core,
			PCIE20_AER_UNCORR_ERR_STATUS_REG,
			0x3fff031, 0x3fff031);
	msm_pcie_write_reg_field(dev->dm_core,
			PCIE20_AER_CORR_ERR_STATUS_REG,
			0xf1c1, 0xf1c1);
	msm_pcie_write_reg_field(dev->dm_core,
			PCIE20_AER_ROOT_ERR_STATUS_REG,
			0x7f, 0x7f);

	return IRQ_HANDLED;
}

static irqreturn_t handle_wake_irq(int irq, void *data)
{
	struct msm_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;
	int i;

	spin_lock_irqsave(&dev->wakeup_lock, irqsave_flags);

	dev->wake_counter++;
	PCIE_DBG(dev, "PCIe: No. %ld wake IRQ for RC%d\n",
			dev->wake_counter, dev->rc_idx);

	PCIE_DBG2(dev, "PCIe WAKE is asserted by Endpoint of RC%d\n",
		dev->rc_idx);

	if (!dev->enumerated && !(dev->boot_option &
		MSM_PCIE_NO_WAKE_ENUMERATION)) {
		PCIE_DBG(dev, "Start enumerating RC%d\n", dev->rc_idx);
		schedule_work(&dev->handle_wake_work);
	} else {
		PCIE_DBG2(dev, "Wake up RC%d\n", dev->rc_idx);
		__pm_stay_awake(&dev->ws);
		__pm_relax(&dev->ws);

		if (dev->num_ep > 1) {
			for (i = 0; i < MAX_DEVICE_NUM; i++) {
				dev->event_reg =
					dev->pcidev_table[i].event_reg;
				msm_pcie_notify_client(dev,
					MSM_PCIE_EVENT_WAKEUP);
			}
		} else {
			msm_pcie_notify_client(dev, MSM_PCIE_EVENT_WAKEUP);
		}
	}

	spin_unlock_irqrestore(&dev->wakeup_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static irqreturn_t handle_linkdown_irq(int irq, void *data)
{
	struct msm_pcie_dev_t *dev = data;
	int i;

	dev->linkdown_counter++;

	PCIE_DBG(dev,
		"PCIe: No. %ld linkdown IRQ for RC%d.\n",
		dev->linkdown_counter, dev->rc_idx);

	if (!dev->enumerated || dev->link_status != MSM_PCIE_LINK_ENABLED) {
		PCIE_DBG(dev,
			"PCIe:Linkdown IRQ for RC%d when the link is not enabled\n",
			dev->rc_idx);
	} else if (dev->suspending) {
		PCIE_DBG(dev,
			"PCIe:the link of RC%d is suspending.\n",
			dev->rc_idx);
	} else {
		dev->link_status = MSM_PCIE_LINK_DISABLED;
		dev->shadow_en = false;

		if (dev->linkdown_panic)
			panic("User has chosen to panic on linkdown\n");

		/* assert PERST */
		if (!(msm_pcie_keep_resources_on & BIT(dev->rc_idx)))
			gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
					dev->gpio[MSM_PCIE_GPIO_PERST].on);

		PCIE_ERR(dev, "PCIe link is down for RC%d\n", dev->rc_idx);

		if (dev->num_ep > 1) {
			for (i = 0; i < MAX_DEVICE_NUM; i++) {
				dev->event_reg =
					dev->pcidev_table[i].event_reg;
				msm_pcie_notify_client(dev,
					MSM_PCIE_EVENT_LINKDOWN);
			}
		} else {
			msm_pcie_notify_client(dev, MSM_PCIE_EVENT_LINKDOWN);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t handle_global_irq(int irq, void *data)
{
	int i;
	struct msm_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;
	u32 status = 0;

	spin_lock_irqsave(&dev->irq_lock, irqsave_flags);

	if (dev->suspending) {
		PCIE_DBG2(dev,
			"PCIe: RC%d is currently suspending.\n",
			dev->rc_idx);
		spin_unlock_irqrestore(&dev->irq_lock, irqsave_flags);
		return IRQ_HANDLED;
	}

	status = readl_relaxed(dev->parf + PCIE20_PARF_INT_ALL_STATUS) &
			readl_relaxed(dev->parf + PCIE20_PARF_INT_ALL_MASK);

	msm_pcie_write_mask(dev->parf + PCIE20_PARF_INT_ALL_CLEAR, 0, status);

	PCIE_DUMP(dev, "RC%d: Global IRQ %d received: 0x%x\n",
		dev->rc_idx, irq, status);

	for (i = 0; i <= MSM_PCIE_INT_EVT_MAX; i++) {
		if (status & BIT(i)) {
			switch (i) {
			case MSM_PCIE_INT_EVT_LINK_DOWN:
				PCIE_DBG(dev,
					"PCIe: RC%d: handle linkdown event.\n",
					dev->rc_idx);
				handle_linkdown_irq(irq, data);
				break;
			case MSM_PCIE_INT_EVT_L1SUB_TIMEOUT:
				msm_pcie_notify_client(dev,
					MSM_PCIE_EVENT_L1SS_TIMEOUT);
				break;
			case MSM_PCIE_INT_EVT_AER_LEGACY:
				PCIE_DBG(dev,
					"PCIe: RC%d: AER legacy event.\n",
					dev->rc_idx);
				handle_aer_irq(irq, data);
				break;
			case MSM_PCIE_INT_EVT_AER_ERR:
				PCIE_DBG(dev,
					"PCIe: RC%d: AER event.\n",
					dev->rc_idx);
				handle_aer_irq(irq, data);
				break;
			default:
				PCIE_DUMP(dev,
					"PCIe: RC%d: Unexpected event %d is caught!\n",
					dev->rc_idx, i);
			}
		}
	}

	spin_unlock_irqrestore(&dev->irq_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static int32_t msm_pcie_irq_init(struct msm_pcie_dev_t *dev)
{
	int rc;
	struct device *pdev = &dev->pdev->dev;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	if (dev->rc_idx)
		wakeup_source_init(&dev->ws, "RC1 pcie_wakeup_source");
	else
		wakeup_source_init(&dev->ws, "RC0 pcie_wakeup_source");

	if (dev->irq[MSM_PCIE_INT_GLOBAL_INT].num) {
		rc = devm_request_irq(pdev,
				dev->irq[MSM_PCIE_INT_GLOBAL_INT].num,
				handle_global_irq,
				IRQF_TRIGGER_RISING,
				dev->irq[MSM_PCIE_INT_GLOBAL_INT].name,
				dev);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d: Unable to request global_int interrupt: %d\n",
				dev->rc_idx,
				dev->irq[MSM_PCIE_INT_GLOBAL_INT].num);
			return rc;
		}
	}

	/* register handler for PCIE_WAKE_N interrupt line */
	if (dev->wake_n) {
		rc = devm_request_irq(pdev,
				dev->wake_n, handle_wake_irq,
				IRQF_TRIGGER_FALLING, "msm_pcie_wake", dev);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d: Unable to request wake interrupt\n",
				dev->rc_idx);
			return rc;
		}

		INIT_WORK(&dev->handle_wake_work, handle_wake_func);

		rc = enable_irq_wake(dev->wake_n);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d: Unable to enable wake interrupt\n",
				dev->rc_idx);
			return rc;
		}
	}

	return 0;
}

static void msm_pcie_irq_deinit(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	wakeup_source_trash(&dev->ws);

	if (dev->wake_n)
		disable_irq(dev->wake_n);
}

static bool msm_pcie_check_l0s_support(struct pci_dev *pdev,
					struct msm_pcie_dev_t *pcie_dev)
{
	struct pci_dev *parent = pdev->bus->self;
	u32 val;

	/* check parent supports L0s */
	if (parent) {
		u32 val2;

		pci_read_config_dword(parent, parent->pcie_cap + PCI_EXP_LNKCAP,
					&val);
		pci_read_config_dword(parent, parent->pcie_cap + PCI_EXP_LNKCTL,
					&val2);
		val = (val & BIT(10)) && (val2 & PCI_EXP_LNKCTL_ASPM_L0S);
		if (!val) {
			PCIE_DBG(pcie_dev,
				"PCIe: RC%d: Parent PCI device %02x:%02x.%01x does not support L0s\n",
				pcie_dev->rc_idx, parent->bus->number,
				PCI_SLOT(parent->devfn),
				PCI_FUNC(parent->devfn));
			return false;
		}
	}

	pci_read_config_dword(pdev, pdev->pcie_cap + PCI_EXP_LNKCAP, &val);
	if (!(val & BIT(10))) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: PCI device %02x:%02x.%01x does not support L0s\n",
			pcie_dev->rc_idx, pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
		return false;
	}

	return true;
}

static bool msm_pcie_check_l1_support(struct pci_dev *pdev,
					struct msm_pcie_dev_t *pcie_dev)
{
	struct pci_dev *parent = pdev->bus->self;
	u32 val;

	/* check parent supports L1 */
	if (parent) {
		u32 val2;

		pci_read_config_dword(parent, parent->pcie_cap + PCI_EXP_LNKCAP,
					&val);
		pci_read_config_dword(parent, parent->pcie_cap + PCI_EXP_LNKCTL,
					&val2);
		val = (val & BIT(11)) && (val2 & PCI_EXP_LNKCTL_ASPM_L1);
		if (!val) {
			PCIE_DBG(pcie_dev,
				"PCIe: RC%d: Parent PCI device %02x:%02x.%01x does not support L1\n",
				pcie_dev->rc_idx, parent->bus->number,
				PCI_SLOT(parent->devfn),
				PCI_FUNC(parent->devfn));
			return false;
		}
	}

	pci_read_config_dword(pdev, pdev->pcie_cap + PCI_EXP_LNKCAP, &val);
	if (!(val & BIT(11))) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: PCI device %02x:%02x.%01x does not support L1\n",
			pcie_dev->rc_idx, pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
		return false;
	}

	return true;
}

static int msm_pcie_check_l1ss_support(struct pci_dev *pdev, void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;
	u32 val;
	u32 l1ss_cap_id_offset, l1ss_cap_offset, l1ss_ctl1_offset;

	if (!pcie_dev->l1ss_supported)
		return -ENXIO;

	l1ss_cap_id_offset = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_id_offset) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: PCI device %02x:%02x.%01x could not find L1ss capability register\n",
			pcie_dev->rc_idx, pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
		pcie_dev->l1ss_supported = false;
		return -ENXIO;
	}

	l1ss_cap_offset = l1ss_cap_id_offset + PCI_L1SS_CAP;
	l1ss_ctl1_offset = l1ss_cap_id_offset + PCI_L1SS_CTL1;

	pci_read_config_dword(pdev, l1ss_cap_offset, &val);
	pcie_dev->l1_1_pcipm_supported &= !!(val & (PCI_L1SS_CAP_PCIPM_L1_1));
	pcie_dev->l1_2_pcipm_supported &= !!(val & (PCI_L1SS_CAP_PCIPM_L1_2));
	pcie_dev->l1_1_aspm_supported &= !!(val & (PCI_L1SS_CAP_ASPM_L1_1));
	pcie_dev->l1_2_aspm_supported &= !!(val & (PCI_L1SS_CAP_ASPM_L1_2));
	if (!pcie_dev->l1_1_pcipm_supported &&
		!pcie_dev->l1_2_pcipm_supported &&
		!pcie_dev->l1_1_aspm_supported &&
		!pcie_dev->l1_2_aspm_supported) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: PCI device %02x:%02x.%01x does not support any L1ss\n",
			pcie_dev->rc_idx, pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
		pcie_dev->l1ss_supported = false;
		return -ENXIO;
	}

	return 0;
}

static int msm_pcie_config_common_clock_enable(struct pci_dev *pdev,
							void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;

	PCIE_DBG(pcie_dev, "PCIe: RC%d: PCI device %02x:%02x.%01x\n",
		pcie_dev->rc_idx, pdev->bus->number, PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn));

	msm_pcie_config_clear_set_dword(pdev, pdev->pcie_cap + PCI_EXP_LNKCTL,
					0, PCI_EXP_LNKCTL_CCC);

	return 0;
}

static void msm_pcie_config_common_clock_enable_all(struct msm_pcie_dev_t *dev)
{
	if (dev->common_clk_en)
		pci_walk_bus(dev->dev->bus,
			msm_pcie_config_common_clock_enable, dev);
}

static int msm_pcie_config_clock_power_management_enable(struct pci_dev *pdev,
							void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;
	u32 val;

	/* enable only for upstream ports */
	if (pci_is_root_bus(pdev->bus))
		return 0;

	PCIE_DBG(pcie_dev, "PCIe: RC%d: PCI device %02x:%02x.%01x\n",
		pcie_dev->rc_idx, pdev->bus->number, PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn));

	pci_read_config_dword(pdev, pdev->pcie_cap + PCI_EXP_LNKCAP, &val);
	if (val & PCI_EXP_LNKCAP_CLKPM)
		msm_pcie_config_clear_set_dword(pdev,
			pdev->pcie_cap + PCI_EXP_LNKCTL, 0,
			PCI_EXP_LNKCTL_CLKREQ_EN);
	else
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: PCI device %02x:%02x.%01x does not support clock power management\n",
			pcie_dev->rc_idx, pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	return 0;
}

static void msm_pcie_config_clock_power_management_enable_all(
						struct msm_pcie_dev_t *dev)
{
	if (dev->clk_power_manage_en)
		pci_walk_bus(dev->dev->bus,
			msm_pcie_config_clock_power_management_enable, dev);
}

static void msm_pcie_config_l0s(struct msm_pcie_dev_t *dev,
				struct pci_dev *pdev, bool enable)
{
	u32 lnkctl_offset = pdev->pcie_cap + PCI_EXP_LNKCTL;
	int ret;

	PCIE_DBG(dev, "PCIe: RC%d: PCI device %02x:%02x.%01x %s\n",
		dev->rc_idx, pdev->bus->number, PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn), enable ? "enable" : "disable");

	if (enable) {
		ret = msm_pcie_check_l0s_support(pdev, dev);
		if (!ret)
			return;

		msm_pcie_config_clear_set_dword(pdev, lnkctl_offset, 0,
			PCI_EXP_LNKCTL_ASPM_L0S);
	} else {
		msm_pcie_config_clear_set_dword(pdev, lnkctl_offset,
			PCI_EXP_LNKCTL_ASPM_L0S, 0);
	}
}

static void msm_pcie_config_l0s_disable_all(struct msm_pcie_dev_t *dev,
				struct pci_bus *bus)
{
	struct pci_dev *pdev;

	if (!dev->l0s_supported)
		return;

	list_for_each_entry(pdev, &bus->devices, bus_list) {
		struct pci_bus *child;

		child  = pdev->subordinate;
		if (child)
			msm_pcie_config_l0s_disable_all(dev, child);
		msm_pcie_config_l0s(dev, pdev, false);
	}
}

static int msm_pcie_config_l0s_enable(struct pci_dev *pdev, void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;

	msm_pcie_config_l0s(pcie_dev, pdev, true);
	return 0;
}

static void msm_pcie_config_l0s_enable_all(struct msm_pcie_dev_t *dev)
{
	if (dev->l0s_supported)
		pci_walk_bus(dev->dev->bus, msm_pcie_config_l0s_enable, dev);
}

static void msm_pcie_config_l1(struct msm_pcie_dev_t *dev,
				struct pci_dev *pdev, bool enable)
{
	u32 lnkctl_offset = pdev->pcie_cap + PCI_EXP_LNKCTL;
	int ret;

	PCIE_DBG(dev, "PCIe: RC%d: PCI device %02x:%02x.%01x %s\n",
		dev->rc_idx, pdev->bus->number, PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn), enable ? "enable" : "disable");

	if (enable) {
		ret = msm_pcie_check_l1_support(pdev, dev);
		if (!ret)
			return;

		msm_pcie_config_clear_set_dword(pdev, lnkctl_offset, 0,
			PCI_EXP_LNKCTL_ASPM_L1);
	} else {
		msm_pcie_config_clear_set_dword(pdev, lnkctl_offset,
			PCI_EXP_LNKCTL_ASPM_L1, 0);
	}
}

static void msm_pcie_config_l1_disable_all(struct msm_pcie_dev_t *dev,
				struct pci_bus *bus)
{
	struct pci_dev *pdev;

	if (!dev->l1_supported)
		return;

	list_for_each_entry(pdev, &bus->devices, bus_list) {
		struct pci_bus *child;

		child  = pdev->subordinate;
		if (child)
			msm_pcie_config_l1_disable_all(dev, child);
		msm_pcie_config_l1(dev, pdev, false);
	}
}

static int msm_pcie_config_l1_enable(struct pci_dev *pdev, void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;

	msm_pcie_config_l1(pcie_dev, pdev, true);
	return 0;
}

static void msm_pcie_config_l1_enable_all(struct msm_pcie_dev_t *dev)
{
	if (dev->l1_supported)
		pci_walk_bus(dev->dev->bus, msm_pcie_config_l1_enable, dev);
}

static void msm_pcie_config_l1ss(struct msm_pcie_dev_t *dev,
				struct pci_dev *pdev, bool enable)
{
	u32 val, val2;
	u32 l1ss_cap_id_offset, l1ss_ctl1_offset;
	u32 devctl2_offset = pdev->pcie_cap + PCI_EXP_DEVCTL2;

	PCIE_DBG(dev, "PCIe: RC%d: PCI device %02x:%02x.%01x %s\n",
		dev->rc_idx, pdev->bus->number, PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn), enable ? "enable" : "disable");

	l1ss_cap_id_offset = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_id_offset) {
		PCIE_DBG(dev,
			"PCIe: RC%d: PCI device %02x:%02x.%01x could not find L1ss capability register\n",
			dev->rc_idx, pdev->bus->number, PCI_SLOT(pdev->devfn),
			PCI_FUNC(pdev->devfn));
		return;
	}

	l1ss_ctl1_offset = l1ss_cap_id_offset + PCI_L1SS_CTL1;

	/* Enable the AUX Clock and the Core Clk to be synchronous for L1ss */
	if (pci_is_root_bus(pdev->bus) && !dev->aux_clk_sync) {
		if (enable)
			msm_pcie_write_mask(dev->parf +
				PCIE20_PARF_SYS_CTRL, BIT(3), 0);
		else
			msm_pcie_write_mask(dev->parf +
				PCIE20_PARF_SYS_CTRL, 0, BIT(3));
	}

	if (enable) {
		msm_pcie_config_clear_set_dword(pdev, devctl2_offset, 0,
			PCI_EXP_DEVCTL2_LTR_EN);

		msm_pcie_config_clear_set_dword(pdev, l1ss_ctl1_offset, 0,
			(dev->l1_1_pcipm_supported ?
				PCI_L1SS_CTL1_PCIPM_L1_1 : 0) |
			(dev->l1_2_pcipm_supported ?
				PCI_L1SS_CTL1_PCIPM_L1_2 : 0) |
			(dev->l1_1_aspm_supported ?
				PCI_L1SS_CTL1_ASPM_L1_1 : 0) |
			(dev->l1_2_aspm_supported ?
				PCI_L1SS_CTL1_ASPM_L1_2 : 0));
	} else {
		msm_pcie_config_clear_set_dword(pdev, devctl2_offset,
			PCI_EXP_DEVCTL2_LTR_EN, 0);

		msm_pcie_config_clear_set_dword(pdev, l1ss_ctl1_offset,
			PCI_L1SS_CTL1_PCIPM_L1_1 | PCI_L1SS_CTL1_PCIPM_L1_2 |
			PCI_L1SS_CTL1_ASPM_L1_1 | PCI_L1SS_CTL1_ASPM_L1_2, 0);
	}

	pci_read_config_dword(pdev, l1ss_ctl1_offset, &val);
	PCIE_DBG2(dev, "PCIe: RC%d: L1SUB_CONTROL1:0x%x\n", dev->rc_idx, val);

	pci_read_config_dword(pdev, devctl2_offset, &val2);
	PCIE_DBG2(dev, "PCIe: RC%d: DEVICE_CONTROL2_STATUS2::0x%x\n",
		dev->rc_idx, val2);
}

static int msm_pcie_config_l1ss_disable(struct pci_dev *pdev, void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;

	msm_pcie_config_l1ss(pcie_dev, pdev, false);
	return 0;
}

static void msm_pcie_config_l1ss_disable_all(struct msm_pcie_dev_t *dev,
				struct pci_bus *bus)
{
	struct pci_dev *pdev;

	if (!dev->l1ss_supported)
		return;

	list_for_each_entry(pdev, &bus->devices, bus_list) {
		struct pci_bus *child;

		child  = pdev->subordinate;
		if (child)
			msm_pcie_config_l1ss_disable_all(dev, child);
		msm_pcie_config_l1ss_disable(pdev, dev);
	}
}

static int msm_pcie_config_l1_2_threshold(struct pci_dev *pdev, void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;
	u32 l1ss_cap_id_offset, l1ss_ctl1_offset;
	u32 l1_2_th_scale_shift = 29;
	u32 l1_2_th_value_shift = 16;

	/* LTR is not supported */
	if (!pcie_dev->l1_2_th_value)
		return 0;

	PCIE_DBG(pcie_dev, "PCIe: RC%d: PCI device %02x:%02x.%01x\n",
		pcie_dev->rc_idx, pdev->bus->number, PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn));

	l1ss_cap_id_offset = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS);
	if (!l1ss_cap_id_offset) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: PCI device %02x:%02x.%01x could not find L1ss capability register\n",
			pcie_dev->rc_idx, pdev->bus->number,
			PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
		return 0;
	}

	l1ss_ctl1_offset = l1ss_cap_id_offset + PCI_L1SS_CTL1;

	msm_pcie_config_clear_set_dword(pdev, l1ss_ctl1_offset, 0,
		(PCI_L1SS_CTL1_LTR_L12_TH_SCALE &
		(pcie_dev->l1_2_th_scale << l1_2_th_scale_shift)) |
		(PCI_L1SS_CTL1_LTR_L12_TH_VALUE &
		(pcie_dev->l1_2_th_value << l1_2_th_value_shift)));

	return 0;
}

static int msm_pcie_config_l1ss_enable(struct pci_dev *pdev, void *dev)
{
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *)dev;

	msm_pcie_config_l1ss(pcie_dev, pdev, true);
	return 0;
}

static void msm_pcie_config_l1ss_enable_all(struct msm_pcie_dev_t *dev)
{
	if (dev->l1ss_supported) {
		pci_walk_bus(dev->dev->bus, msm_pcie_config_l1_2_threshold,
				dev);
		pci_walk_bus(dev->dev->bus, msm_pcie_config_l1ss_enable, dev);
	}
}

static void msm_pcie_config_link_pm(struct msm_pcie_dev_t *dev, bool enable)
{
	struct pci_bus *bus = dev->dev->bus;

	if (enable) {
		msm_pcie_config_common_clock_enable_all(dev);
		msm_pcie_config_clock_power_management_enable_all(dev);
		msm_pcie_config_l1ss_enable_all(dev);
		msm_pcie_config_l1_enable_all(dev);
		msm_pcie_config_l0s_enable_all(dev);
	} else {
		msm_pcie_config_l0s_disable_all(dev, bus);
		msm_pcie_config_l1_disable_all(dev, bus);
		msm_pcie_config_l1ss_disable_all(dev, bus);
	}
}

static void msm_pcie_check_l1ss_support_all(struct msm_pcie_dev_t *dev)
{
	pci_walk_bus(dev->dev->bus, msm_pcie_check_l1ss_support, dev);
}

static int msm_pcie_setup_drv(struct msm_pcie_dev_t *pcie_dev,
			 struct device_node *of_node)
{
	struct msm_pcie_drv_info *drv_info;
	struct msm_pcie_drv_msg *msg;
	struct msm_pcie_drv_tre *pkt;
	struct msm_pcie_drv_header *hdr;
	u32 drv_l1ss_timeout_us = 0;
	int ret;

	drv_info = devm_kzalloc(&pcie_dev->pdev->dev, sizeof(*drv_info),
				GFP_KERNEL);
	if (!drv_info)
		return -ENOMEM;

	ret = of_property_read_u32(of_node, "qcom,drv-l1ss-timeout-us",
					&drv_l1ss_timeout_us);
	if (ret)
		drv_l1ss_timeout_us = L1SS_TIMEOUT_US;

	PCIE_DBG(pcie_dev, "PCIe: RC%d: DRV L1ss timeout: %dus\n",
		pcie_dev->rc_idx, drv_l1ss_timeout_us);

	drv_info->dev_id = pcie_dev->rc_idx;

	/* cache frequent command for communication */
	msg = &drv_info->drv_enable;
	pkt = &msg->pkt;
	hdr = &msg->hdr;
	hdr->major_ver = MSM_PCIE_DRV_MAJOR_VERSION;
	hdr->minor_ver = MSM_PCIE_DRV_MINOR_VERSION;
	hdr->msg_id = MSM_PCIE_DRV_MSG_ID_CMD;
	hdr->payload_size = sizeof(*pkt);
	hdr->dev_id = drv_info->dev_id;

	pkt->dword[0] = MSM_PCIE_DRV_CMD_ENABLE;
	pkt->dword[1] = hdr->dev_id;
	pkt->dword[2] = drv_l1ss_timeout_us / 1000;

	msg = &drv_info->drv_disable;
	pkt = &msg->pkt;
	hdr = &msg->hdr;
	hdr->major_ver = MSM_PCIE_DRV_MAJOR_VERSION;
	hdr->minor_ver = MSM_PCIE_DRV_MINOR_VERSION;
	hdr->msg_id = MSM_PCIE_DRV_MSG_ID_CMD;
	hdr->payload_size = sizeof(*pkt);
	hdr->dev_id = drv_info->dev_id;

	pkt->dword[0] = MSM_PCIE_DRV_CMD_DISABLE;
	pkt->dword[1] = hdr->dev_id;

	init_completion(&drv_info->completion);
	drv_info->timeout_ms = IPC_TIMEOUT_MS;
	pcie_dev->drv_info = drv_info;

	return 0;
}

static int msm_pcie_probe(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx = -1;
	int i, j;
	struct msm_pcie_dev_t *pcie_dev;
	struct device_node *of_node;
	bool drv_supported;

	PCIE_GEN_DBG("%s\n", __func__);

	mutex_lock(&pcie_drv.drv_lock);

	of_node = pdev->dev.of_node;

	ret = of_property_read_u32(of_node, "cell-index", &rc_idx);
	if (ret) {
		PCIE_GEN_DBG("PCIe: Did not find RC index.\n");
		goto out;
	}

	if (rc_idx >= MAX_RC_NUM)
		goto out;

	pcie_drv.rc_num++;
	pcie_dev = &msm_pcie_dev[rc_idx];
	pcie_dev->rc_idx = rc_idx;
	pcie_dev->pdev = pdev;
	pcie_dev->link_status = MSM_PCIE_LINK_DEINIT;

	PCIE_DBG(pcie_dev, "PCIe: RC index is %d.\n", pcie_dev->rc_idx);

	pcie_dev->l0s_supported = !of_property_read_bool(of_node,
				"qcom,no-l0s-supported");
	if (msm_pcie_invert_l0s_support & BIT(pcie_dev->rc_idx))
		pcie_dev->l0s_supported = !pcie_dev->l0s_supported;
	PCIE_DBG(pcie_dev, "L0s is %s supported.\n", pcie_dev->l0s_supported ?
		"" : "not");

	pcie_dev->l1_supported = !of_property_read_bool(of_node,
				"qcom,no-l1-supported");
	if (msm_pcie_invert_l1_support & BIT(pcie_dev->rc_idx))
		pcie_dev->l1_supported = !pcie_dev->l1_supported;
	PCIE_DBG(pcie_dev, "L1 is %s supported.\n", pcie_dev->l1_supported ?
		"" : "not");

	pcie_dev->l1ss_supported = !of_property_read_bool(of_node,
				"qcom,no-l1ss-supported");
	if (msm_pcie_invert_l1ss_support & BIT(pcie_dev->rc_idx))
		pcie_dev->l1ss_supported = !pcie_dev->l1ss_supported;
	PCIE_DBG(pcie_dev, "L1ss is %s supported.\n", pcie_dev->l1ss_supported ?
		"" : "not");

	pcie_dev->l1_1_aspm_supported = pcie_dev->l1ss_supported;
	pcie_dev->l1_2_aspm_supported = pcie_dev->l1ss_supported;
	pcie_dev->l1_1_pcipm_supported = pcie_dev->l1ss_supported;
	pcie_dev->l1_2_pcipm_supported = pcie_dev->l1ss_supported;

	of_property_read_u32(of_node, "qcom,l1-2-th-scale",
				&pcie_dev->l1_2_th_scale);
	of_property_read_u32(of_node, "qcom,l1-2-th-value",
				&pcie_dev->l1_2_th_value);
	PCIE_DBG(pcie_dev, "PCIe: RC%d: L1.2 threshold scale: %d value: %d.\n",
		pcie_dev->rc_idx, pcie_dev->l1_2_th_scale,
		pcie_dev->l1_2_th_value);

	pcie_dev->common_clk_en = of_property_read_bool(of_node,
				"qcom,common-clk-en");
	PCIE_DBG(pcie_dev, "Common clock is %s enabled.\n",
		pcie_dev->common_clk_en ? "" : "not");

	pcie_dev->clk_power_manage_en = of_property_read_bool(of_node,
				"qcom,clk-power-manage-en");
	PCIE_DBG(pcie_dev, "Clock power management is %s enabled.\n",
		pcie_dev->clk_power_manage_en ? "" : "not");

	pcie_dev->aux_clk_sync = !of_property_read_bool(of_node,
				"qcom,no-aux-clk-sync");
	PCIE_DBG(pcie_dev, "AUX clock is %s synchronous to Core clock.\n",
		pcie_dev->aux_clk_sync ? "" : "not");

	pcie_dev->use_19p2mhz_aux_clk = of_property_read_bool(of_node,
				"qcom,use-19p2mhz-aux-clk");
	PCIE_DBG(pcie_dev, "AUX clock frequency is %s 19.2MHz.\n",
		pcie_dev->use_19p2mhz_aux_clk ? "" : "not");

	of_property_read_u32(of_node, "qcom,smmu-sid-base",
				&pcie_dev->smmu_sid_base);
	PCIE_DBG(pcie_dev, "RC%d: qcom,smmu-sid-base: 0x%x.\n",
		pcie_dev->rc_idx, pcie_dev->smmu_sid_base);

	of_property_read_u32(of_node, "qcom,boot-option",
				&pcie_dev->boot_option);
	PCIE_DBG(pcie_dev, "PCIe: RC%d boot option is 0x%x.\n",
		pcie_dev->rc_idx, pcie_dev->boot_option);

	of_property_read_u32(of_node, "qcom,pcie-phy-ver",
				&pcie_dev->phy_ver);
	PCIE_DBG(pcie_dev, "RC%d: pcie-phy-ver: %d.\n", pcie_dev->rc_idx,
		pcie_dev->phy_ver);

	pcie_dev->link_check_max_count = LINK_UP_CHECK_MAX_COUNT;
	of_property_read_u32(pdev->dev.of_node,
				"qcom,link-check-max-count",
				&pcie_dev->link_check_max_count);
	PCIE_DBG(pcie_dev, "PCIe: RC%d: link-check-max-count: %u.\n",
		pcie_dev->rc_idx, pcie_dev->link_check_max_count);

	of_property_read_u32(of_node, "qcom,target-link-speed",
				&pcie_dev->target_link_speed);
	PCIE_DBG(pcie_dev, "PCIe: RC%d: target-link-speed: 0x%x.\n",
		pcie_dev->rc_idx, pcie_dev->target_link_speed);

	of_property_read_u32(of_node, "qcom,n-fts", &pcie_dev->n_fts);
	PCIE_DBG(pcie_dev, "n-fts: 0x%x.\n", pcie_dev->n_fts);

	of_property_read_u32(of_node, "qcom,ep-latency",
				&pcie_dev->ep_latency);
	PCIE_DBG(pcie_dev, "RC%d: ep-latency: %ums.\n", pcie_dev->rc_idx,
		pcie_dev->ep_latency);

	of_property_read_u32(of_node, "qcom,switch-latency",
				&pcie_dev->switch_latency);
	PCIE_DBG(pcie_dev, "RC%d: switch-latency: %ums.\n", pcie_dev->rc_idx,
		pcie_dev->switch_latency);

	ret = of_property_read_u32(of_node, "qcom,wr-halt-size",
				&pcie_dev->wr_halt_size);
	if (ret)
		PCIE_DBG(pcie_dev,
			"RC%d: wr-halt-size not specified in dt. Use default value.\n",
			pcie_dev->rc_idx);
	else
		PCIE_DBG(pcie_dev, "RC%d: wr-halt-size: 0x%x.\n",
			pcie_dev->rc_idx, pcie_dev->wr_halt_size);

	pcie_dev->slv_addr_space_size = SZ_16M;
	of_property_read_u32(of_node, "qcom,slv-addr-space-size",
				&pcie_dev->slv_addr_space_size);
	PCIE_DBG(pcie_dev, "RC%d: slv-addr-space-size: 0x%x.\n",
		pcie_dev->rc_idx, pcie_dev->slv_addr_space_size);

	of_property_read_u32(of_node, "qcom,phy-status-offset",
				&pcie_dev->phy_status_offset);
	PCIE_DBG(pcie_dev, "RC%d: phy-status-offset: 0x%x.\n", pcie_dev->rc_idx,
		pcie_dev->phy_status_offset);

	of_property_read_u32(pdev->dev.of_node, "qcom,phy-status-bit",
				&pcie_dev->phy_status_bit);
	PCIE_DBG(pcie_dev, "RC%d: phy-status-bit: %u.\n", pcie_dev->rc_idx,
		pcie_dev->phy_status_bit);

	of_property_read_u32(of_node, "qcom,phy-power-down-offset",
				&pcie_dev->phy_power_down_offset);
	PCIE_DBG(pcie_dev, "RC%d: phy-power-down-offset: 0x%x.\n",
		pcie_dev->rc_idx, pcie_dev->phy_power_down_offset);

	pcie_dev->core_preset = PCIE_GEN3_PRESET_DEFAULT;
	of_property_read_u32(pdev->dev.of_node,
				"qcom,core-preset",
				&pcie_dev->core_preset);
	PCIE_DBG(pcie_dev, "RC%d: core-preset: 0x%x.\n",
		pcie_dev->rc_idx, pcie_dev->core_preset);

	of_property_read_u32(of_node, "qcom,cpl-timeout",
				&pcie_dev->cpl_timeout);
	PCIE_DBG(pcie_dev, "RC%d: cpl-timeout: 0x%x.\n",
		pcie_dev->rc_idx, pcie_dev->cpl_timeout);

	pcie_dev->perst_delay_us_min = PERST_PROPAGATION_DELAY_US_MIN;
	pcie_dev->perst_delay_us_max = PERST_PROPAGATION_DELAY_US_MAX;
	of_property_read_u32(of_node, "qcom,perst-delay-us-min",
				&pcie_dev->perst_delay_us_min);
	of_property_read_u32(of_node, "qcom,perst-delay-us-max",
				&pcie_dev->perst_delay_us_max);
	PCIE_DBG(pcie_dev,
		"RC%d: perst-delay-us-min: %dus. perst-delay-us-max: %dus.\n",
		pcie_dev->rc_idx, pcie_dev->perst_delay_us_min,
		pcie_dev->perst_delay_us_max);

	pcie_dev->tlp_rd_size = PCIE_TLP_RD_SIZE;
	of_property_read_u32(of_node, "qcom,tlp-rd-size",
				&pcie_dev->tlp_rd_size);
	PCIE_DBG(pcie_dev, "RC%d: tlp-rd-size: 0x%x.\n", pcie_dev->rc_idx,
		pcie_dev->tlp_rd_size);

	pcie_dev->shadow_en = true;
	pcie_dev->aer_enable = true;
	if (msm_pcie_invert_aer_support)
		pcie_dev->aer_enable = !pcie_dev->aer_enable;

	memcpy(pcie_dev->vreg, msm_pcie_vreg_info, sizeof(msm_pcie_vreg_info));
	memcpy(pcie_dev->gpio, msm_pcie_gpio_info, sizeof(msm_pcie_gpio_info));
	memcpy(pcie_dev->clk, msm_pcie_clk_info[rc_idx],
		sizeof(msm_pcie_clk_info[rc_idx]));
	memcpy(pcie_dev->pipeclk, msm_pcie_pipe_clk_info[rc_idx],
		sizeof(msm_pcie_pipe_clk_info[rc_idx]));
	memcpy(pcie_dev->res, msm_pcie_res_info, sizeof(msm_pcie_res_info));
	memcpy(pcie_dev->irq, msm_pcie_irq_info, sizeof(msm_pcie_irq_info));
	memcpy(pcie_dev->reset, msm_pcie_reset_info[rc_idx],
		sizeof(msm_pcie_reset_info[rc_idx]));
	memcpy(pcie_dev->pipe_reset, msm_pcie_pipe_reset_info[rc_idx],
		sizeof(msm_pcie_pipe_reset_info[rc_idx]));

	for (i = 0; i < PCIE_CONF_SPACE_DW; i++)
		pcie_dev->rc_shadow[i] = PCIE_CLEAR;
	for (i = 0; i < MAX_DEVICE_NUM; i++)
		for (j = 0; j < PCIE_CONF_SPACE_DW; j++)
			pcie_dev->ep_shadow[i][j] = PCIE_CLEAR;
	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		pcie_dev->pcidev_table[i].bdf = 0;
		pcie_dev->pcidev_table[i].dev = NULL;
		pcie_dev->pcidev_table[i].short_bdf = 0;
		pcie_dev->pcidev_table[i].sid = 0;
		pcie_dev->pcidev_table[i].domain = rc_idx;
		pcie_dev->pcidev_table[i].conf_base = NULL;
		pcie_dev->pcidev_table[i].phy_address = 0;
		pcie_dev->pcidev_table[i].dev_ctrlstts_offset = 0;
		pcie_dev->pcidev_table[i].event_reg = NULL;
		pcie_dev->pcidev_table[i].registered = true;
	}

	dev_set_drvdata(&pdev->dev, pcie_dev);

	ret = msm_pcie_get_resources(pcie_dev, pcie_dev->pdev);
	if (ret)
		goto decrease_rc_num;

	if (pcie_dev->rumi)
		pcie_dev->rumi_init = msm_pcie_rumi_init;

	pcie_dev->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pcie_dev->pinctrl))
		PCIE_ERR(pcie_dev, "PCIe: RC%d failed to get pinctrl\n",
		pcie_dev->rc_idx);
	else
		pcie_dev->use_pinctrl = true;

	if (pcie_dev->use_pinctrl) {
		pcie_dev->pins_default = pinctrl_lookup_state(pcie_dev->pinctrl,
								"default");
		if (IS_ERR(pcie_dev->pins_default)) {
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d could not get pinctrl default state\n",
				pcie_dev->rc_idx);
			pcie_dev->pins_default = NULL;
		}

		pcie_dev->pins_sleep = pinctrl_lookup_state(pcie_dev->pinctrl,
								"sleep");
		if (IS_ERR(pcie_dev->pins_sleep)) {
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d could not get pinctrl sleep state\n",
				pcie_dev->rc_idx);
			pcie_dev->pins_sleep = NULL;
		}
	}

	ret = msm_pcie_gpio_init(pcie_dev);
	if (ret) {
		msm_pcie_release_resources(pcie_dev);
		goto decrease_rc_num;
	}

	ret = msm_pcie_irq_init(pcie_dev);
	if (ret) {
		msm_pcie_release_resources(pcie_dev);
		msm_pcie_gpio_deinit(pcie_dev);
		goto decrease_rc_num;
	}

	drv_supported = of_property_read_bool(of_node, "qcom,drv-supported");
	if (drv_supported) {
		ret = msm_pcie_setup_drv(pcie_dev, of_node);
		if (ret)
			PCIE_ERR(pcie_dev,
				 "PCIe: RC%d: DRV: failed to setup DRV: ret: %d\n",
				pcie_dev->rc_idx, ret);
	}

	msm_pcie_sysfs_init(pcie_dev);

	pcie_dev->drv_ready = true;

	if (pcie_dev->boot_option & MSM_PCIE_NO_PROBE_ENUMERATION) {
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d will be enumerated by client or endpoint.\n",
			pcie_dev->rc_idx);
		mutex_unlock(&pcie_drv.drv_lock);
		return 0;
	}

	ret = msm_pcie_enumerate(rc_idx);
	if (ret)
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d is not enabled during bootup; it will be enumerated upon client request.\n",
			pcie_dev->rc_idx);
	else
		PCIE_ERR(pcie_dev, "RC%d is enabled in bootup\n",
			pcie_dev->rc_idx);

	PCIE_DBG(pcie_dev, "PCIe probed %s\n", dev_name(&pdev->dev));

	mutex_unlock(&pcie_drv.drv_lock);
	return 0;

decrease_rc_num:
	pcie_drv.rc_num--;
	PCIE_ERR(pcie_dev, "PCIe: RC%d: Driver probe failed. ret: %d\n",
		pcie_dev->rc_idx, ret);
out:
	if (rc_idx < 0 || rc_idx >= MAX_RC_NUM)
		pr_err("PCIe: Invalid RC index %d. Driver probe failed\n",
			rc_idx);

	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static int msm_pcie_remove(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx;

	PCIE_GEN_DBG("PCIe:%s.\n", __func__);

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		pr_err("%s: Did not find RC index.\n", __func__);
		goto out;
	} else {
		pcie_drv.rc_num--;
		PCIE_GEN_DBG("%s: RC index is 0x%x.", __func__, rc_idx);
	}

	msm_pcie_irq_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_vreg_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_clk_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_gpio_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_release_resources(&msm_pcie_dev[rc_idx]);

out:
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static int msm_pcie_link_retrain(struct msm_pcie_dev_t *pcie_dev,
				struct pci_dev *pci_dev)
{
	u32 cnt;
	u32 cnt_max = 1000; /* 100ms timeout */
	u32 link_status_lbms_mask = PCI_EXP_LNKSTA_LBMS << PCI_EXP_LNKCTL;

	/* link retrain */
	msm_pcie_config_clear_set_dword(pci_dev,
					pci_dev->pcie_cap + PCI_EXP_LNKCTL,
					0, PCI_EXP_LNKCTL_RL);

	cnt = 0;
	/* poll until link train is done */
	while (!(readl_relaxed(pcie_dev->dm_core + pci_dev->pcie_cap +
		PCI_EXP_LNKCTL) & link_status_lbms_mask)) {
		if (unlikely(cnt++ >= cnt_max)) {
			PCIE_ERR(pcie_dev, "PCIe: RC%d: failed to retrain\n",
				pcie_dev->rc_idx);
			return -EIO;
		}

		usleep_range(100, 105);
	}

	return 0;
}

static int msm_pcie_set_link_width(struct msm_pcie_dev_t *pcie_dev,
					u16 target_link_width)
{
	u16 link_width;

	switch (target_link_width) {
	case PCI_EXP_LNKSTA_NLW_X1:
		link_width = LINK_WIDTH_X1;
		break;
	case PCI_EXP_LNKSTA_NLW_X2:
		link_width = LINK_WIDTH_X2;
		break;
	default:
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d: unsupported link width request: %d\n",
			pcie_dev->rc_idx, target_link_width);
		return -EINVAL;
	}

	msm_pcie_write_reg_field(pcie_dev->dm_core,
				PCIE20_PORT_LINK_CTRL_REG,
				LINK_WIDTH_MASK << LINK_WIDTH_SHIFT,
				link_width);

	return 0;
}

void msm_pcie_allow_l1(struct pci_dev *pci_dev)
{
	struct pci_dev *root_pci_dev;
	struct msm_pcie_dev_t *pcie_dev;

	root_pci_dev = pci_find_pcie_root_port(pci_dev);
	if (!root_pci_dev)
		return;

	pcie_dev = PCIE_BUS_PRIV_DATA(root_pci_dev->bus);

	mutex_lock(&pcie_dev->aspm_lock);
	if (unlikely(--pcie_dev->prevent_l1 < 0))
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d: %02x:%02x.%01x: unbalanced prevent_l1: %d < 0\n",
			pcie_dev->rc_idx, pci_dev->bus->number,
			PCI_SLOT(pci_dev->devfn), PCI_FUNC(pci_dev->devfn),
			pcie_dev->prevent_l1);

	if (pcie_dev->prevent_l1) {
		mutex_unlock(&pcie_dev->aspm_lock);
		return;
	}

	msm_pcie_write_mask(pcie_dev->parf + PCIE20_PARF_PM_CTRL, BIT(5), 0);
	/* enable L1 */
	msm_pcie_write_mask(pcie_dev->dm_core +
				(root_pci_dev->pcie_cap + PCI_EXP_LNKCTL),
				0, PCI_EXP_LNKCTL_ASPM_L1);

	PCIE_DBG2(pcie_dev, "PCIe: RC%d: %02x:%02x.%01x: exit\n",
		pcie_dev->rc_idx, pci_dev->bus->number,
		PCI_SLOT(pci_dev->devfn), PCI_FUNC(pci_dev->devfn));
	mutex_unlock(&pcie_dev->aspm_lock);
}
EXPORT_SYMBOL(msm_pcie_allow_l1);

int msm_pcie_prevent_l1(struct pci_dev *pci_dev)
{
	struct pci_dev *root_pci_dev;
	struct msm_pcie_dev_t *pcie_dev;
	u32 cnt = 0;
	u32 cnt_max = 1000; /* 100ms timeout */
	int ret = 0;

	root_pci_dev = pci_find_pcie_root_port(pci_dev);
	if (!root_pci_dev)
		return -ENODEV;

	pcie_dev = PCIE_BUS_PRIV_DATA(root_pci_dev->bus);

	/* disable L1 */
	mutex_lock(&pcie_dev->aspm_lock);
	if (pcie_dev->prevent_l1++) {
		mutex_unlock(&pcie_dev->aspm_lock);
		return 0;
	}

	msm_pcie_write_mask(pcie_dev->dm_core +
				(root_pci_dev->pcie_cap + PCI_EXP_LNKCTL),
				PCI_EXP_LNKCTL_ASPM_L1, 0);
	msm_pcie_write_mask(pcie_dev->parf + PCIE20_PARF_PM_CTRL, 0, BIT(5));

	/* confirm link is in L0 */
	while (((readl_relaxed(pcie_dev->parf + PCIE20_PARF_LTSSM) &
		MSM_PCIE_LTSSM_MASK)) != MSM_PCIE_LTSSM_L0) {
		if (unlikely(cnt++ >= cnt_max)) {
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d: %02x:%02x.%01x: failed to transition to L0\n",
				pcie_dev->rc_idx, pci_dev->bus->number,
				PCI_SLOT(pci_dev->devfn),
				PCI_FUNC(pci_dev->devfn));
			ret = -EIO;
			goto err;
		}

		usleep_range(100, 105);
	}

	PCIE_DBG2(pcie_dev, "PCIe: RC%d: %02x:%02x.%01x: exit\n",
		pcie_dev->rc_idx, pci_dev->bus->number,
		PCI_SLOT(pci_dev->devfn), PCI_FUNC(pci_dev->devfn));
	mutex_unlock(&pcie_dev->aspm_lock);

	return 0;
err:
	mutex_unlock(&pcie_dev->aspm_lock);
	msm_pcie_allow_l1(pci_dev);

	return ret;
}
EXPORT_SYMBOL(msm_pcie_prevent_l1);

int msm_pcie_set_link_bandwidth(struct pci_dev *pci_dev, u16 target_link_speed,
				u16 target_link_width)
{
	struct pci_dev *root_pci_dev;
	struct msm_pcie_dev_t *pcie_dev;
	u16 link_status;
	u16 current_link_speed;
	u16 current_link_width;
	bool set_link_speed = true;
	bool set_link_width = true;
	int ret;

	if (!pci_dev)
		return -EINVAL;

	root_pci_dev = pci_find_pcie_root_port(pci_dev);
	if (!root_pci_dev)
		return -ENODEV;

	pcie_dev = PCIE_BUS_PRIV_DATA(root_pci_dev->bus);

	pcie_capability_read_word(root_pci_dev, PCI_EXP_LNKSTA, &link_status);

	current_link_speed = link_status & PCI_EXP_LNKSTA_CLS;
	current_link_width = link_status & PCI_EXP_LNKSTA_NLW;
	target_link_width <<= PCI_EXP_LNKSTA_NLW_SHIFT;

	if (target_link_speed == current_link_speed)
		set_link_speed = false;

	if (target_link_width == current_link_width)
		set_link_width = false;

	if (!set_link_speed && !set_link_width)
		return 0;

	if (set_link_width) {
		ret = msm_pcie_set_link_width(pcie_dev, target_link_width);
		if (ret)
			return ret;
	}

	if (set_link_speed)
		msm_pcie_config_clear_set_dword(root_pci_dev,
						root_pci_dev->pcie_cap +
						PCI_EXP_LNKCTL2,
						PCI_EXP_LNKSTA_CLS,
						target_link_speed);

	/* need to be in L0 for gen switch */
	ret = msm_pcie_prevent_l1(root_pci_dev);
	if (ret)
		return ret;

	if (target_link_speed > current_link_speed)
		msm_pcie_scale_link_bandwidth(pcie_dev, target_link_speed);

	ret = msm_pcie_link_retrain(pcie_dev, root_pci_dev);
	if (ret)
		goto out;

	pcie_capability_read_word(root_pci_dev, PCI_EXP_LNKSTA, &link_status);
	if ((link_status & PCI_EXP_LNKSTA_CLS) != target_link_speed ||
		(link_status & PCI_EXP_LNKSTA_NLW) != target_link_width) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d: failed to switch bandwidth: target speed: %d width: %d\n",
			pcie_dev->rc_idx, target_link_speed,
			target_link_width >> PCI_EXP_LNKSTA_NLW_SHIFT);
		ret = -EIO;
		goto out;
	}

	if (target_link_speed < current_link_speed)
		msm_pcie_scale_link_bandwidth(pcie_dev, target_link_speed);
out:
	msm_pcie_allow_l1(root_pci_dev);

	return ret;
}
EXPORT_SYMBOL(msm_pcie_set_link_bandwidth);

int msm_pci_probe(struct pci_dev *pci_dev,
		  const struct pci_device_id *device_id)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(pci_dev->bus);
	struct msm_root_dev_t *root_dev;

	PCIE_DBG(pcie_dev, "PCIe: RC%d: PCI Probe\n", pcie_dev->rc_idx);

	if (!pci_dev->dev.of_node)
		return -ENODEV;

	root_dev = devm_kzalloc(&pci_dev->dev, sizeof(*root_dev), GFP_KERNEL);
	if (!root_dev)
		return -ENOMEM;

	root_dev->pcie_dev = pcie_dev;
	root_dev->pci_dev = pci_dev;
	dev_set_drvdata(&pci_dev->dev, root_dev);

	ret = dma_set_mask_and_coherent(&pci_dev->dev, DMA_BIT_MASK(64));
	if (ret) {
		PCIE_ERR(pcie_dev, "DMA set mask failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static struct pci_device_id msm_pci_device_id[] = {
	{PCI_DEVICE(0x17cb, 0x0108)},
	{PCI_DEVICE(0x17cb, 0x010b)},
	{0},
};

static struct pci_driver msm_pci_driver = {
	.name = "pci-msm-rc",
	.id_table = msm_pci_device_id,
	.probe = msm_pci_probe,
};

static const struct of_device_id msm_pcie_match[] = {
	{	.compatible = "qcom,pci-msm",
	},
	{}
};

static struct platform_driver msm_pcie_driver = {
	.probe	= msm_pcie_probe,
	.remove	= msm_pcie_remove,
	.driver	= {
		.name		= "pci-msm",
		.of_match_table	= msm_pcie_match,
	},
};

static int msm_pcie_drv_rpmsg_probe(struct rpmsg_device *rpdev)
{
	pcie_drv.rpdev = rpdev;
	dev_set_drvdata(&rpdev->dev, &pcie_drv);

	/* start drv connection */
	schedule_work(&pcie_drv.drv_connect);

	return 0;
}

static void msm_pcie_drv_notify_client(struct pcie_drv_sta *pcie_drv,
					enum msm_pcie_event event)
{
	struct msm_pcie_dev_t *pcie_dev = pcie_drv->msm_pcie_dev;
	int i;

	for (i = 0; i < MAX_RC_NUM; i++, pcie_dev++) {
		struct msm_pcie_drv_info *drv_info = pcie_dev->drv_info;
		struct msm_pcie_register_event *event_reg =
			pcie_dev->event_reg;

		PCIE_DBG(pcie_dev, "PCIe: RC%d: event %d received\n",
			pcie_dev->rc_idx, event);

		/* does not support DRV or has not been probed yet */
		if (!drv_info)
			continue;

		if (!event_reg || !(event_reg->events & event))
			continue;

		if (drv_info->ep_connected) {
			msm_pcie_notify_client(pcie_dev, event);
			if (event & MSM_PCIE_EVENT_DRV_DISCONNECT)
				drv_info->ep_connected = false;
		}
	}
}

static void msm_pcie_drv_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct pcie_drv_sta *pcie_drv = dev_get_drvdata(&rpdev->dev);

	pcie_drv->rpdev = NULL;
	flush_work(&pcie_drv->drv_connect);

	msm_pcie_drv_notify_client(pcie_drv, MSM_PCIE_EVENT_DRV_DISCONNECT);
}

static int msm_pcie_drv_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
				int len, void *priv, u32 src)
{
	struct pcie_drv_sta *pcie_drv = dev_get_drvdata(&rpdev->dev);
	struct msm_pcie_dev_t *pcie_dev;
	struct msm_pcie_drv_header *drv_header;
	struct msm_pcie_drv_info *drv_info;

	while (len) {
		if (len < sizeof(*drv_header)) {
			pr_err("PCIe: DRV: invalid header length: %d\n",
				len);
			return -EINVAL;
		}

		drv_header = data;
		data += sizeof(*drv_header);
		len -= sizeof(*drv_header);

		if (drv_header->dev_id >= MAX_RC_NUM) {
			pr_err("PCIe: DRV: invalid device id: %d\n",
				drv_header->dev_id);
			return -EINVAL;
		}

		pcie_dev = pcie_drv->msm_pcie_dev + drv_header->dev_id;
		drv_info = pcie_dev->drv_info;
		if (!drv_info) {
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d: DRV: no device info found\n",
				pcie_dev->rc_idx);
			return -ENODEV;
		}

		switch (drv_header->msg_id) {
		case MSM_PCIE_DRV_MSG_ID_ACK:
		{
			u32 *status;
			size_t status_size = sizeof(*status);

			if (drv_header->payload_size != status_size) {
				PCIE_ERR(pcie_dev,
					"PCIe: RC%d: DRV: invalid payload size: %d\n",
					pcie_dev->rc_idx,
					drv_header->payload_size);
				return -EINVAL;
			}

			if (len < status_size) {
				PCIE_ERR(pcie_dev,
					"PCIe: RC%d: DRV: invalid status length: %d\n",
					pcie_dev->rc_idx, len);
				return -EINVAL;
			}

			status = data;
			data += status_size;
			len -= status_size;

			if (drv_header->reply_seq != drv_info->reply_seq) {
				PCIE_ERR(pcie_dev,
					"PCIe: RC%d: DRV: incorrect reply seq: %d: expected seq: %d\n",
					pcie_dev->rc_idx,
					drv_header->reply_seq,
					drv_info->reply_seq);
				return -EINVAL;
			}

			if (*status) {
				PCIE_ERR(pcie_dev,
					"PCIe: RC%d: DRV: invalid status\n",
					pcie_dev->rc_idx);
				return -EINVAL;
			}

			complete(&drv_info->completion);
			break;
		}
		default:
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d: DRV: unsupported command: 0x%x\n",
				pcie_dev->rc_idx, drv_header->msg_id);
			return -EINVAL;
		}
	}

	return 0;
}

static struct rpmsg_device_id msm_pcie_drv_rpmsg_match_table[] = {
	{
		.name = "pcie_drv"
	},
	{},
};

static struct rpmsg_driver msm_pcie_drv_rpmsg_driver = {
	.id_table = msm_pcie_drv_rpmsg_match_table,
	.probe = msm_pcie_drv_rpmsg_probe,
	.remove = msm_pcie_drv_rpmsg_remove,
	.callback = msm_pcie_drv_rpmsg_cb,
	.drv = {
		.name = "pci-msm-drv",
	},
};

static void msm_pcie_early_notifier(void *data)
{
	struct pcie_drv_sta *pcie_drv = data;

	pcie_drv->rpdev = NULL;

	msm_pcie_drv_notify_client(pcie_drv, MSM_PCIE_EVENT_WAKEUP);
};

static void msm_pcie_drv_connect_worker(struct work_struct *work)
{
	struct pcie_drv_sta *pcie_drv = container_of(work, struct pcie_drv_sta,
						     drv_connect);
	struct msm_pcie_dev_t *pcie_dev = pcie_drv->msm_pcie_dev;
	int i;

	/* rpmsg probe hasn't happened yet */
	if (!pcie_drv->rpdev)
		return;

	for (i = 0; i < MAX_RC_NUM; i++, pcie_dev++) {
		struct msm_pcie_drv_info *drv_info = pcie_dev->drv_info;
		struct msm_pcie_register_event *event_reg =
			pcie_dev->event_reg;

		/* does not support DRV or has not been probed yet */
		if (!drv_info || drv_info->ep_connected)
			continue;

		/* no DRV support over a switch */
		if (pcie_dev->num_ep != 1)
			continue;

		if (!event_reg ||
		    !(event_reg->events & MSM_PCIE_EVENT_DRV_CONNECT))
			continue;

		msm_pcie_notify_client(pcie_dev,
				       MSM_PCIE_EVENT_DRV_CONNECT);
		drv_info->ep_connected = true;
	}

	subsys_register_early_notifier("adsp", PCIE_DRV_LAYER_NOTIF,
				msm_pcie_early_notifier, pcie_drv);
}

static int __init pcie_init(void)
{
	int ret = 0, i;
	char rc_name[MAX_RC_NAME_LEN];

	pr_alert("pcie:%s.\n", __func__);

	pcie_drv.rc_num = 0;
	mutex_init(&pcie_drv.drv_lock);

	for (i = 0; i < MAX_RC_NUM; i++) {
		snprintf(rc_name, MAX_RC_NAME_LEN, "pcie%d-short", i);
		msm_pcie_dev[i].ipc_log =
			ipc_log_context_create(PCIE_LOG_PAGES, rc_name, 0);
		if (msm_pcie_dev[i].ipc_log == NULL)
			pr_err("%s: unable to create IPC log context for %s\n",
				__func__, rc_name);
		else
			PCIE_DBG(&msm_pcie_dev[i],
				"PCIe IPC logging is enable for RC%d\n",
				i);
		snprintf(rc_name, MAX_RC_NAME_LEN, "pcie%d-long", i);
		msm_pcie_dev[i].ipc_log_long =
			ipc_log_context_create(PCIE_LOG_PAGES, rc_name, 0);
		if (msm_pcie_dev[i].ipc_log_long == NULL)
			pr_err("%s: unable to create IPC log context for %s\n",
				__func__, rc_name);
		else
			PCIE_DBG(&msm_pcie_dev[i],
				"PCIe IPC logging %s is enable for RC%d\n",
				rc_name, i);
		snprintf(rc_name, MAX_RC_NAME_LEN, "pcie%d-dump", i);
		msm_pcie_dev[i].ipc_log_dump =
			ipc_log_context_create(PCIE_LOG_PAGES, rc_name, 0);
		if (msm_pcie_dev[i].ipc_log_dump == NULL)
			pr_err("%s: unable to create IPC log context for %s\n",
				__func__, rc_name);
		else
			PCIE_DBG(&msm_pcie_dev[i],
				"PCIe IPC logging %s is enable for RC%d\n",
				rc_name, i);
		spin_lock_init(&msm_pcie_dev[i].cfg_lock);
		msm_pcie_dev[i].cfg_access = true;
		mutex_init(&msm_pcie_dev[i].enumerate_lock);
		mutex_init(&msm_pcie_dev[i].setup_lock);
		mutex_init(&msm_pcie_dev[i].clk_lock);
		mutex_init(&msm_pcie_dev[i].recovery_lock);
		mutex_init(&msm_pcie_dev[i].aspm_lock);
		spin_lock_init(&msm_pcie_dev[i].wakeup_lock);
		spin_lock_init(&msm_pcie_dev[i].irq_lock);
		msm_pcie_dev[i].drv_ready = false;
	}
	for (i = 0; i < MAX_RC_NUM * MAX_DEVICE_NUM; i++) {
		msm_pcie_dev_tbl[i].bdf = 0;
		msm_pcie_dev_tbl[i].dev = NULL;
		msm_pcie_dev_tbl[i].short_bdf = 0;
		msm_pcie_dev_tbl[i].sid = 0;
		msm_pcie_dev_tbl[i].domain = -1;
		msm_pcie_dev_tbl[i].conf_base = NULL;
		msm_pcie_dev_tbl[i].phy_address = 0;
		msm_pcie_dev_tbl[i].dev_ctrlstts_offset = 0;
		msm_pcie_dev_tbl[i].event_reg = NULL;
		msm_pcie_dev_tbl[i].registered = true;
	}

	crc8_populate_msb(msm_pcie_crc8_table, MSM_PCIE_CRC8_POLYNOMIAL);

	msm_pcie_debugfs_init();

	ret = pci_register_driver(&msm_pci_driver);
	if (ret)
		return ret;

	INIT_WORK(&pcie_drv.drv_connect, msm_pcie_drv_connect_worker);
	pcie_drv.msm_pcie_dev = msm_pcie_dev;

	ret = register_rpmsg_driver(&msm_pcie_drv_rpmsg_driver);
	if (ret)
		pr_warn("PCIe: DRV: failed to register with rpmsg: ret: %d\n",
			ret);

	ret = platform_driver_register(&msm_pcie_driver);

	return ret;
}

static void __exit pcie_exit(void)
{
	int i;

	PCIE_GEN_DBG("pcie:%s.\n", __func__);

	platform_driver_unregister(&msm_pcie_driver);

	msm_pcie_debugfs_exit();

	for (i = 0; i < MAX_RC_NUM; i++)
		msm_pcie_sysfs_exit(&msm_pcie_dev[i]);
}

subsys_initcall_sync(pcie_init);
module_exit(pcie_exit);


/* RC do not represent the right class; set it to PCI_CLASS_BRIDGE_PCI */
static void msm_pcie_fixup_early(struct pci_dev *dev)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);

	PCIE_DBG(pcie_dev, "hdr_type %d\n", dev->hdr_type);
	if (pci_is_root_bus(dev->bus))
		dev->class = (dev->class & 0xff) | (PCI_CLASS_BRIDGE_PCI << 8);
}
DECLARE_PCI_FIXUP_EARLY(PCIE_VENDOR_ID_QCOM, PCI_ANY_ID,
			msm_pcie_fixup_early);

static void __msm_pcie_l1ss_timeout_disable(struct msm_pcie_dev_t *pcie_dev)
{
	msm_pcie_write_mask(pcie_dev->parf + PCIE20_PARF_DEBUG_INT_EN, BIT(0),
				0);
	writel_relaxed(0, pcie_dev->parf + PCIE20_PARF_L1SUB_AHB_CLK_MAX_TIMER);
}

static void __msm_pcie_l1ss_timeout_enable(struct msm_pcie_dev_t *pcie_dev)
{
	u32 val = BIT(31);

	writel_relaxed(val, pcie_dev->parf +
			PCIE20_PARF_L1SUB_AHB_CLK_MAX_TIMER);

	/* 3 AUX clock cycles so that RESET will sync with timer logic */
	usleep_range(3, 4);

	val |= L1SS_TIMEOUT_US_TO_TICKS(L1SS_TIMEOUT_US);
	writel_relaxed(val, pcie_dev->parf +
			PCIE20_PARF_L1SUB_AHB_CLK_MAX_TIMER);

	/* 1 AUX clock cycle so that CNT_MAX will sync with timer logic */
	usleep_range(1, 2);

	val &= ~BIT(31);
	writel_relaxed(val, pcie_dev->parf +
			PCIE20_PARF_L1SUB_AHB_CLK_MAX_TIMER);

	msm_pcie_write_mask(pcie_dev->parf +
			PCIE20_PARF_DEBUG_INT_EN, 0, BIT(0));
}

/* Suspend the PCIe link */
static int msm_pcie_pm_suspend(struct pci_dev *dev,
			void *user, void *data, u32 options)
{
	int ret = 0;
	u32 val = 0;
	int ret_l23;
	unsigned long irqsave_flags;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);

	PCIE_DBG(pcie_dev, "RC%d: entry\n", pcie_dev->rc_idx);

	spin_lock_irqsave(&pcie_dev->irq_lock, irqsave_flags);
	pcie_dev->suspending = true;
	spin_unlock_irqrestore(&pcie_dev->irq_lock, irqsave_flags);

	if (!pcie_dev->power_on) {
		PCIE_DBG(pcie_dev,
			"PCIe: power of RC%d has been turned off.\n",
			pcie_dev->rc_idx);
		return ret;
	}

	if (dev && !(options & MSM_PCIE_CONFIG_NO_CFG_RESTORE)
		&& msm_pcie_confirm_linkup(pcie_dev, true, true,
			pcie_dev->conf)) {
		ret = pci_save_state(dev);
		pcie_dev->saved_state =	pci_store_saved_state(dev);
	}
	if (ret) {
		PCIE_ERR(pcie_dev, "PCIe: fail to save state of RC%d:%d.\n",
			pcie_dev->rc_idx, ret);
		pcie_dev->suspending = false;
		return ret;
	}

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = false;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	msm_pcie_write_mask(pcie_dev->elbi + PCIE20_ELBI_SYS_CTRL, 0,
				BIT(4));

	PCIE_DBG(pcie_dev, "RC%d: PME_TURNOFF_MSG is sent out\n",
		pcie_dev->rc_idx);

	ret_l23 = readl_poll_timeout((pcie_dev->parf
		+ PCIE20_PARF_PM_STTS), val, (val & BIT(5)), 10000, 100000);

	/* check L23_Ready */
	PCIE_DBG(pcie_dev, "RC%d: PCIE20_PARF_PM_STTS is 0x%x.\n",
		pcie_dev->rc_idx,
		readl_relaxed(pcie_dev->parf + PCIE20_PARF_PM_STTS));
	if (!ret_l23)
		PCIE_DBG(pcie_dev, "RC%d: PM_Enter_L23 is received\n",
			pcie_dev->rc_idx);
	else
		PCIE_DBG(pcie_dev, "RC%d: PM_Enter_L23 is NOT received\n",
			pcie_dev->rc_idx);

	if (pcie_dev->use_pinctrl && pcie_dev->pins_sleep)
		pinctrl_select_state(pcie_dev->pinctrl,
					pcie_dev->pins_sleep);

	msm_pcie_disable(pcie_dev);

	PCIE_DBG(pcie_dev, "RC%d: exit\n", pcie_dev->rc_idx);

	return ret;
}

static void msm_pcie_fixup_suspend(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if (pcie_dev->link_status != MSM_PCIE_LINK_ENABLED ||
		!pci_is_root_bus(dev->bus))
		return;

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	if (pcie_dev->disable_pc) {
		PCIE_DBG(pcie_dev,
			"RC%d: Skip suspend because of user request\n",
			pcie_dev->rc_idx);
		spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
		return;
	}
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	mutex_lock(&pcie_dev->recovery_lock);

	ret = msm_pcie_pm_suspend(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev, "PCIe: RC%d got failure in suspend:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_SUSPEND(PCIE_VENDOR_ID_QCOM, PCI_ANY_ID,
			  msm_pcie_fixup_suspend);

/* Resume the PCIe link */
static int msm_pcie_pm_resume(struct pci_dev *dev,
			void *user, void *data, u32 options)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);

	PCIE_DBG(pcie_dev, "RC%d: entry\n", pcie_dev->rc_idx);

	if (pcie_dev->use_pinctrl && pcie_dev->pins_default)
		pinctrl_select_state(pcie_dev->pinctrl,
					pcie_dev->pins_default);

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = true;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	ret = msm_pcie_enable(pcie_dev);
	if (ret) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d fail to enable PCIe link in resume.\n",
			pcie_dev->rc_idx);
		return ret;
	}

	pcie_dev->suspending = false;
	PCIE_DBG(pcie_dev,
		"dev->bus->number = %d dev->bus->primary = %d\n",
		 dev->bus->number, dev->bus->primary);

	if (!(options & MSM_PCIE_CONFIG_NO_CFG_RESTORE)) {
		if (pcie_dev->saved_state) {
			PCIE_DBG(pcie_dev,
				 "RC%d: entry of PCI framework restore state\n",
				 pcie_dev->rc_idx);

			pci_load_and_free_saved_state(dev,
						      &pcie_dev->saved_state);
			pci_restore_state(dev);

			PCIE_DBG(pcie_dev,
				 "RC%d: exit of PCI framework restore state\n",
				 pcie_dev->rc_idx);
		} else {
			PCIE_DBG(pcie_dev,
				 "RC%d: restore rc config space using shadow recovery\n",
				 pcie_dev->rc_idx);
			msm_pcie_cfg_recover(pcie_dev, true);
		}
	}

	if (pcie_dev->bridge_found) {
		PCIE_DBG(pcie_dev,
			"RC%d: entry of PCIe recover config\n",
			pcie_dev->rc_idx);

		msm_pcie_recover_config(dev);

		PCIE_DBG(pcie_dev,
			"RC%d: exit of PCIe recover config\n",
			pcie_dev->rc_idx);
	}

	PCIE_DBG(pcie_dev, "RC%d: exit\n", pcie_dev->rc_idx);

	return ret;
}

static void msm_pcie_fixup_resume(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend || !pci_is_root_bus(dev->bus))
		return;

	mutex_lock(&pcie_dev->recovery_lock);
	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d got failure in fixup resume:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_RESUME(PCIE_VENDOR_ID_QCOM, PCI_ANY_ID,
				 msm_pcie_fixup_resume);

static void msm_pcie_fixup_resume_early(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend || !pci_is_root_bus(dev->bus))
		return;

	mutex_lock(&pcie_dev->recovery_lock);
	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev, "PCIe: RC%d got failure in resume:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_RESUME_EARLY(PCIE_VENDOR_ID_QCOM, PCI_ANY_ID,
				 msm_pcie_fixup_resume_early);

static int msm_pcie_drv_resume(struct msm_pcie_dev_t *pcie_dev)
{
	struct rpmsg_device *rpdev = pcie_drv.rpdev;
	struct msm_pcie_drv_info *drv_info = pcie_dev->drv_info;
	struct msm_pcie_drv_msg *drv_disable = &drv_info->drv_disable;
	struct msm_pcie_clk_info_t *clk_info;
	u32 current_link_speed;
	int ret, i;

	mutex_lock(&pcie_dev->recovery_lock);
	mutex_lock(&pcie_dev->setup_lock);

	msm_pcie_vreg_init(pcie_dev);

	if (pcie_dev->bus_client) {
		ret = msm_bus_scale_client_update_request(pcie_dev->bus_client,
							1);
		if (ret)
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d: failed to set bus bw vote: %d\n",
				pcie_dev->rc_idx, ret);
	}

	/* turn on all unsuppressible clocks */
	clk_info = pcie_dev->clk;
	for (i = 0; i < MSM_PCIE_MAX_CLK; i++, clk_info++)
		if (clk_info->hdl && !clk_info->suppressible)
			clk_prepare_enable(clk_info->hdl);

	clk_info = pcie_dev->pipeclk;
	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++, clk_info++)
		if (clk_info->hdl && !clk_info->suppressible)
			clk_prepare_enable(clk_info->hdl);

	/* if rpdev is NULL then DRV subsystem is powered down */
	if (rpdev) {
		reinit_completion(&drv_info->completion);

		drv_info->reply_seq = drv_info->seq++;
		drv_disable->hdr.seq = drv_info->reply_seq;

		if (unlikely(drv_info->seq == MSM_PCIE_DRV_SEQ_RESV))
			drv_info->seq = 0;

		ret = rpmsg_trysend(rpdev->ept, drv_disable,
					sizeof(*drv_disable));
		if (!ret) {
			ret = wait_for_completion_timeout(&drv_info->completion,
					msecs_to_jiffies(drv_info->timeout_ms));
			if (!ret)
				PCIE_ERR(pcie_dev,
					"PCIe: RC%d: DRV: completion timeout for rpmsg\n",
					pcie_dev->rc_idx);
		} else {
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d: DRV: failed to send rpmsg\n",
				pcie_dev->rc_idx);
		}
	}

	/* scale CX and rate change based on current GEN speed */
	current_link_speed = readl_relaxed(pcie_dev->dm_core +
					PCIE20_CAP_LINKCTRLSTATUS);
	current_link_speed = ((current_link_speed >> 16) &
				PCI_EXP_LNKSTA_CLS);

	msm_pcie_scale_link_bandwidth(pcie_dev, current_link_speed);

	/* always ungate clkreq */
	msm_pcie_write_reg_field(pcie_dev->parf,
				PCIE20_PARF_CLKREQ_OVERRIDE,
				PCIE20_PARF_CLKREQ_IN_ENABLE, 0);
	msm_pcie_write_reg_field(pcie_dev->parf,
				PCIE20_PARF_CLKREQ_OVERRIDE,
				PCIE20_PARF_CLKREQ_IN_VALUE, 0);

	pcie_dev->user_suspend = false;
	spin_lock_irq(&pcie_dev->cfg_lock);
	pcie_dev->cfg_access = true;
	spin_unlock_irq(&pcie_dev->cfg_lock);
	pcie_dev->link_status = MSM_PCIE_LINK_ENABLED;

	enable_irq(pcie_dev->irq[MSM_PCIE_INT_GLOBAL_INT].num);

	mutex_unlock(&pcie_dev->setup_lock);
	mutex_unlock(&pcie_dev->recovery_lock);

	return 0;
}

static int msm_pcie_drv_suspend(struct msm_pcie_dev_t *pcie_dev)
{
	struct rpmsg_device *rpdev = pcie_drv.rpdev;
	struct msm_pcie_drv_info *drv_info = pcie_dev->drv_info;
	struct msm_pcie_drv_msg *drv_enable = &drv_info->drv_enable;
	struct msm_pcie_clk_info_t *clk_info;
	int ret, i;

	if (!rpdev) {
		PCIE_ERR(pcie_dev, "PCIe: RC%d: DRV: no rpmsg device\n",
			pcie_dev->rc_idx);
		return -EBUSY;
	}

	if (!drv_info->ep_connected) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d: DRV: client requests to DRV suspend while not connected\n",
			pcie_dev->rc_idx);
		return -EINVAL;
	}

	mutex_lock(&pcie_dev->recovery_lock);
	reinit_completion(&drv_info->completion);

	/* disable global irq - no more linkdown/aer detection */
	disable_irq(pcie_dev->irq[MSM_PCIE_INT_GLOBAL_INT].num);

	drv_info->reply_seq = drv_info->seq++;
	drv_enable->hdr.seq = drv_info->reply_seq;

	if (unlikely(drv_info->seq == MSM_PCIE_DRV_SEQ_RESV))
		drv_info->seq = 0;

	ret = rpmsg_trysend(rpdev->ept, drv_enable, sizeof(*drv_enable));
	if (ret) {
		PCIE_ERR(pcie_dev, "PCIe: RC%d: DRV: failed to send rpmsg\n",
			pcie_dev->rc_idx);
		ret = -EBUSY;
		goto out;
	}

	ret = wait_for_completion_timeout(&drv_info->completion,
					msecs_to_jiffies(drv_info->timeout_ms));
	if (!ret) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d: DRV: completion timeout for rpmsg\n",
			pcie_dev->rc_idx);
		ret = -EBUSY;
		goto out;
	}

	pcie_dev->user_suspend = true;
	spin_lock_irq(&pcie_dev->cfg_lock);
	pcie_dev->cfg_access = false;
	spin_unlock_irq(&pcie_dev->cfg_lock);
	mutex_lock(&pcie_dev->setup_lock);
	pcie_dev->link_status = MSM_PCIE_LINK_DRV;

	/* turn off all unsuppressible clocks */
	clk_info = pcie_dev->pipeclk;
	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++, clk_info++)
		if (clk_info->hdl && !clk_info->suppressible)
			clk_disable_unprepare(clk_info->hdl);

	clk_info = pcie_dev->clk;
	for (i = 0; i < MSM_PCIE_MAX_CLK; i++, clk_info++)
		if (clk_info->hdl && !clk_info->suppressible)
			clk_disable_unprepare(clk_info->hdl);

	if (pcie_dev->bus_client) {
		ret = msm_bus_scale_client_update_request(pcie_dev->bus_client,
							0);
		if (ret)
			PCIE_ERR(pcie_dev,
				"PCIe: RC%d: DRV: failed to remove bus bw vote: %d\n",
				pcie_dev->rc_idx, ret);
	}

	msm_pcie_vreg_deinit(pcie_dev);

	mutex_unlock(&pcie_dev->setup_lock);
	mutex_unlock(&pcie_dev->recovery_lock);

	return 0;
out:
	enable_irq(pcie_dev->irq[MSM_PCIE_INT_GLOBAL_INT].num);
	mutex_unlock(&pcie_dev->recovery_lock);
	return ret;
}

int msm_pcie_pm_control(enum msm_pcie_pm_opt pm_opt, u32 busnr, void *user,
			void *data, u32 options)
{
	int ret = 0;
	struct pci_dev *dev;
	u32 rc_idx = 0;
	struct msm_pcie_dev_t *pcie_dev;

	PCIE_GEN_DBG("PCIe: pm_opt:%d;busnr:%d;options:%d\n",
		pm_opt, busnr, options);


	if (!user) {
		pr_err("PCIe: endpoint device is NULL\n");
		ret = -ENODEV;
		goto out;
	}

	pcie_dev = PCIE_BUS_PRIV_DATA(((struct pci_dev *)user)->bus);

	if (pcie_dev) {
		rc_idx = pcie_dev->rc_idx;
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: pm_opt:%d;busnr:%d;options:%d\n",
			rc_idx, pm_opt, busnr, options);
	} else {
		pr_err(
			"PCIe: did not find RC for pci endpoint device.\n"
			);
		ret = -ENODEV;
		goto out;
	}

	dev = msm_pcie_dev[rc_idx].dev;

	if (!msm_pcie_dev[rc_idx].drv_ready) {
		PCIE_ERR(&msm_pcie_dev[rc_idx],
			"RC%d has not been successfully probed yet\n",
			rc_idx);
		return -EPROBE_DEFER;
	}

	switch (pm_opt) {
	case MSM_PCIE_DRV_SUSPEND:
		PCIE_DBG(pcie_dev,
			"PCIe: RC%d: DRV: user requests for DRV suspend\n",
			rc_idx);
		ret = msm_pcie_drv_suspend(pcie_dev);
		break;
	case MSM_PCIE_SUSPEND:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d requests to suspend the link\n", rc_idx);
		if (msm_pcie_dev[rc_idx].link_status != MSM_PCIE_LINK_ENABLED)
			PCIE_DBG(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to suspend when link is not enabled:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);

		if (!msm_pcie_dev[rc_idx].power_on) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to suspend when link is powered down:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);
			break;
		}

		if (msm_pcie_dev[rc_idx].pending_ep_reg) {
			PCIE_DBG(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: request to suspend the link is rejected\n",
				rc_idx);
			break;
		}

		if (pcie_dev->num_active_ep) {
			PCIE_DBG(pcie_dev,
				"RC%d: an EP requested to suspend the link, but other EPs are still active: %d\n",
				pcie_dev->rc_idx, pcie_dev->num_active_ep);
			return ret;
		}

		msm_pcie_dev[rc_idx].user_suspend = true;

		mutex_lock(&msm_pcie_dev[rc_idx].recovery_lock);

		ret = msm_pcie_pm_suspend(dev, user, data, options);
		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user failed to suspend the link.\n",
				rc_idx);
			msm_pcie_dev[rc_idx].user_suspend = false;
		}

		mutex_unlock(&msm_pcie_dev[rc_idx].recovery_lock);
		break;
	case MSM_PCIE_RESUME:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d requests to resume the link\n", rc_idx);

		/* DRV resume */
		if (pcie_dev->link_status == MSM_PCIE_LINK_DRV) {
			ret = msm_pcie_drv_resume(pcie_dev);
			break;
		}

		if (msm_pcie_dev[rc_idx].link_status !=
					MSM_PCIE_LINK_DISABLED) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to resume when link is not disabled:%d. Number of active EP(s): %d\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status,
				msm_pcie_dev[rc_idx].num_active_ep);
			break;
		}

		mutex_lock(&msm_pcie_dev[rc_idx].recovery_lock);
		ret = msm_pcie_pm_resume(dev, user, data, options);
		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user failed to resume the link.\n",
				rc_idx);
		} else {
			PCIE_DBG(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user succeeded to resume the link.\n",
				rc_idx);

			msm_pcie_dev[rc_idx].user_suspend = false;
		}

		mutex_unlock(&msm_pcie_dev[rc_idx].recovery_lock);

		break;
	case MSM_PCIE_DISABLE_PC:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d requests to keep the link always alive.\n",
			rc_idx);
		spin_lock_irqsave(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		if (msm_pcie_dev[rc_idx].suspending) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d Link has been suspended before request\n",
				rc_idx);
			ret = MSM_PCIE_ERROR;
		} else {
			msm_pcie_dev[rc_idx].disable_pc = true;
		}
		spin_unlock_irqrestore(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		break;
	case MSM_PCIE_ENABLE_PC:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d cancels the request of alive link.\n",
			rc_idx);
		spin_lock_irqsave(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		msm_pcie_dev[rc_idx].disable_pc = false;
		spin_unlock_irqrestore(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		break;
	default:
		PCIE_ERR(&msm_pcie_dev[rc_idx],
			"PCIe: RC%d: unsupported pm operation:%d.\n",
			rc_idx, pm_opt);
		ret = -ENODEV;
		goto out;
	}

out:
	return ret;
}
EXPORT_SYMBOL(msm_pcie_pm_control);

void msm_pcie_l1ss_timeout_disable(struct pci_dev *pci_dev)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(pci_dev->bus);

	__msm_pcie_l1ss_timeout_disable(pcie_dev);
}
EXPORT_SYMBOL(msm_pcie_l1ss_timeout_disable);

void msm_pcie_l1ss_timeout_enable(struct pci_dev *pci_dev)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(pci_dev->bus);

	__msm_pcie_l1ss_timeout_enable(pcie_dev);
}
EXPORT_SYMBOL(msm_pcie_l1ss_timeout_enable);

int msm_pcie_register_event(struct msm_pcie_register_event *reg)
{
	int i, ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}

	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}

	pcie_dev = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user)->bus);

	if (!pcie_dev) {
		PCIE_ERR(pcie_dev, "%s",
			"PCIe: did not find RC for pci endpoint device.\n");
		return -ENODEV;
	}

	if (pcie_dev->num_ep > 1) {
		for (i = 0; i < MAX_DEVICE_NUM; i++) {
			if (reg->user ==
				pcie_dev->pcidev_table[i].dev) {
				pcie_dev->event_reg =
					pcie_dev->pcidev_table[i].event_reg;

				if (!pcie_dev->event_reg) {
					pcie_dev->pcidev_table[i].registered =
						true;

					pcie_dev->num_active_ep++;
					PCIE_DBG(pcie_dev,
						"PCIe: RC%d: number of active EP(s): %d.\n",
						pcie_dev->rc_idx,
						pcie_dev->num_active_ep);
				}

				pcie_dev->event_reg = reg;
				pcie_dev->pcidev_table[i].event_reg = reg;
				PCIE_DBG(pcie_dev,
					"Event 0x%x is registered for RC %d\n",
					reg->events,
					pcie_dev->rc_idx);

				break;
			}
		}

		if (pcie_dev->pending_ep_reg) {
			for (i = 0; i < MAX_DEVICE_NUM; i++)
				if (!pcie_dev->pcidev_table[i].registered)
					break;

			if (i == MAX_DEVICE_NUM)
				pcie_dev->pending_ep_reg = false;
		}
	} else {
		pcie_dev->event_reg = reg;
		PCIE_DBG(pcie_dev,
			"Event 0x%x is registered for RC %d\n", reg->events,
			pcie_dev->rc_idx);

		schedule_work(&pcie_drv.drv_connect);
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_register_event);

int msm_pcie_deregister_event(struct msm_pcie_register_event *reg)
{
	int i, ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}

	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}

	pcie_dev = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user)->bus);

	if (!pcie_dev) {
		PCIE_ERR(pcie_dev, "%s",
			"PCIe: did not find RC for pci endpoint device.\n");
		return -ENODEV;
	}

	if (pcie_dev->num_ep > 1) {
		for (i = 0; i < MAX_DEVICE_NUM; i++) {
			if (reg->user == pcie_dev->pcidev_table[i].dev) {
				if (pcie_dev->pcidev_table[i].event_reg) {
					pcie_dev->num_active_ep--;
					PCIE_DBG(pcie_dev,
						"PCIe: RC%d: number of active EP(s) left: %d.\n",
						pcie_dev->rc_idx,
						pcie_dev->num_active_ep);
				}

				pcie_dev->event_reg = NULL;
				pcie_dev->pcidev_table[i].event_reg = NULL;
				PCIE_DBG(pcie_dev,
					"Event is deregistered for RC %d\n",
					pcie_dev->rc_idx);

				break;
			}
		}
	} else {
		pcie_dev->event_reg = NULL;
		PCIE_DBG(pcie_dev, "Event is deregistered for RC %d\n",
				pcie_dev->rc_idx);
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_deregister_event);

int msm_pcie_recover_config(struct pci_dev *dev)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (dev) {
		pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);
		PCIE_DBG(pcie_dev,
			"Recovery for the link of RC%d\n", pcie_dev->rc_idx);
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	if (msm_pcie_confirm_linkup(pcie_dev, true, true, pcie_dev->conf)) {
		PCIE_DBG(pcie_dev,
			"Recover config space of RC%d and its EP\n",
			pcie_dev->rc_idx);
		pcie_dev->shadow_en = false;
		PCIE_DBG(pcie_dev, "Recover RC%d\n", pcie_dev->rc_idx);
		msm_pcie_cfg_recover(pcie_dev, true);
		PCIE_DBG(pcie_dev, "Recover EP of RC%d\n", pcie_dev->rc_idx);
		msm_pcie_cfg_recover(pcie_dev, false);
		PCIE_DBG(pcie_dev,
			"Refreshing the saved config space in PCI framework for RC%d and its EP\n",
			pcie_dev->rc_idx);
		pci_save_state(pcie_dev->dev);
		pci_save_state(dev);
		pcie_dev->shadow_en = true;
		PCIE_DBG(pcie_dev, "Turn on shadow for RC%d\n",
			pcie_dev->rc_idx);
	} else {
		PCIE_ERR(pcie_dev,
			"PCIe: the link of RC%d is not up yet; can't recover config space.\n",
			pcie_dev->rc_idx);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_recover_config);

int msm_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (dev) {
		pcie_dev = PCIE_BUS_PRIV_DATA(dev->bus);
		PCIE_DBG(pcie_dev,
			"User requests to %s shadow\n",
			enable ? "enable" : "disable");
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	PCIE_DBG(pcie_dev,
		"The shadowing of RC%d is %s enabled currently.\n",
		pcie_dev->rc_idx, pcie_dev->shadow_en ? "" : "not");

	pcie_dev->shadow_en = enable;

	PCIE_DBG(pcie_dev,
		"Shadowing of RC%d is turned %s upon user's request.\n",
		pcie_dev->rc_idx, enable ? "on" : "off");

	return ret;
}
EXPORT_SYMBOL(msm_pcie_shadow_control);
