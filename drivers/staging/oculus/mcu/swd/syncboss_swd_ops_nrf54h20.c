// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/meta-hpd.h>
#include <linux/meta-proglogic.h>
#include <linux/i2c.h>

#include <linux/of.h>
#include <linux/of_device.h>

#include "swd.h"
#include "syncboss_swd_common_ops.h"
#include "swd_registers_nrf54h20.h"
#include "syncboss_swd_ops_nrf54h20.h"

/*
 * TODO T245743071: revisit and correct/optimize delays.
 */
#define POWER_STATE_CHANGE_WAIT_MS	(200ll)
#define MCU_BOOT_DELAY_MS		(200ll)

#define MUX_STATE_CHANGE_WAIT_US	(2)
#define POLL_INTERVAL_MS		(10)
#define BOOT_TIMEOUT_MS			(8000ll)
#define ERASE_TIMEOUT_MS		(8000ll)
#define MBOX_TIMEOUT_MS			(1000ll)
#define SWD_INIT_TIMEOUT_MS		(1000ll)

enum boot_stage {
	BOOT_STAGE_UNKNOWN,
	BOOT_STAGE_UNSET,
	BOOT_STAGE_SYSCTRL,
	BOOT_STAGE_SYSROM,
	BOOT_STAGE_SDROM,
	BOOT_STAGE_SUIT,
	BOOT_STAGE_IRONSIDE,
	BOOT_STAGE_RECOVERY,
};

static void syncboss_swd_setstate(struct device *dev, bool enable)
{
	struct device_node *np = dev->of_node;
	int count, i, ret;
	struct device_node *proglogic_dev_node;
	struct device_node *tether_hpd;
	struct platform_device *meta_hpd;
	struct i2c_client *i2c_usb_mux_client;
	bool proglogic_client_probed = false;

	if (!np) {
		dev_err(dev, "%s: no dtsi entry for nrf54h20\n", __func__);
		return;
	}

	tether_hpd = of_parse_phandle(np, "meta,hpd", 0);
	if (!tether_hpd) {
		dev_err(dev, "%s: meta,hpd handle not found\n", __func__);
		return;
	}
	meta_hpd = of_find_device_by_node(tether_hpd);
	if (!meta_hpd) {
		dev_err(dev, "%s: ynable to find meta-hpd platform device\n", __func__);
		of_node_put(tether_hpd);
		return;
	}
	of_node_put(tether_hpd);

	count = of_count_phandle_with_args(np, "meta,proglogic", NULL);
	if (count <= 0) {
		dev_err(dev, "%s: proglogic handle not found: %d\n", __func__, count);
		return;
	}
	dev_dbg(dev, "%s: found %d devices!", __func__, count);

	for (i = 0; i < count; i++) {
		proglogic_dev_node = of_parse_phandle(np, "meta,proglogic", i);
		if (!proglogic_dev_node)
			continue;
		i2c_usb_mux_client = of_find_i2c_device_by_node(proglogic_dev_node);

		of_node_put(proglogic_dev_node);
		if (!i2c_usb_mux_client) {
			dev_dbg(dev, "Skipping %d proglogic_client is NULL", count);
			continue;
		}
		if (i2c_usb_mux_client->dev.driver) {
			dev_dbg(dev, "Probe driver for %s found !", dev_name(&i2c_usb_mux_client->dev));
			proglogic_client_probed = true;
			break;
		} else {
			dev_dbg(dev, "Driver %s did not probe", dev_name(&i2c_usb_mux_client->dev));
		}
	}
	dev_dbg(dev, "%s: setting swd enable=%d\n", __func__, enable);
	if (proglogic_client_probed && i2c_usb_mux_client) {
		if (enable) {
			ret = hpd_enable_detect(&meta_hpd->dev, false);
			if (ret)
				dev_err(dev, "%s: unable to disable HPD detection: %d\n", __func__, ret);
			ret = proglogic_swd_enable(&i2c_usb_mux_client->dev);
			if (ret)
				dev_err(dev, "%s: unable to enable SWD: %d\n", __func__, ret);
			udelay(MUX_STATE_CHANGE_WAIT_US);
		} else {
			ret = proglogic_swd_disable(&i2c_usb_mux_client->dev);
			if (ret)
				dev_err(dev, "%s: unable to disable SWD: %d\n", __func__, ret);
			udelay(MUX_STATE_CHANGE_WAIT_US);
			ret = hpd_enable_detect(&meta_hpd->dev, true);
			if (ret)
				dev_err(dev, "%s: unnable to re-enable HPD detection: %d\n", __func__, ret);
		}
		put_device(&i2c_usb_mux_client->dev);
		put_device(&meta_hpd->dev);
	} else {
		put_device(&meta_hpd->dev);
		dev_err(dev, "%s: failed to find swd mux device\n", __func__);
	}
}

static int syncboss_swd_set_tetherpower(struct device *dev, bool enable)
{
	struct device_node *np = dev->of_node;
	struct device_node *tether_hpd;
	struct platform_device *meta_hpd;
	int ret;

	if (!np) {
		dev_err(dev, "%s: no dtsi entry for nrf54h20\n", __func__);
		return -EINVAL;
	}
	tether_hpd = of_parse_phandle(np, "meta,hpd", 0);
	if (!tether_hpd) {
		dev_err(dev, "%s: meta,hpd handle not found\n", __func__);
		return -EINVAL;
	}
	meta_hpd = of_find_device_by_node(tether_hpd);
	if (!meta_hpd) {
		dev_err(dev, "%s: unable to find meta-hpd platform device!\n", __func__);
		return -EINVAL;
	}
	of_node_put(tether_hpd);
	ret = hpd_set_tether_power(&meta_hpd->dev, enable);
	put_device(&meta_hpd->dev);
	return ret;
}

static void syncboss_deep_powercycle(struct device *dev)
{
	int status = 0;

	dev_dbg(dev, "%s start\n", __func__);

	syncboss_swd_setstate(dev, false);
	status = syncboss_swd_set_tetherpower(dev, false);
	if (status)
		dev_err(dev, "%s: tether power disable failed: %d\n", __func__, status);

	msleep(POWER_STATE_CHANGE_WAIT_MS);

	status = syncboss_swd_set_tetherpower(dev, true);
	if (status)
		dev_err(dev, "%s: tether power enable failed: %d\n", __func__, status);

	syncboss_swd_setstate(dev, true);
}

static int syncboss_swd_nrf54h20_read_ctrl_ap_reg(struct device *dev,
						  u32 reg_addr, u32 *data_ptr)
{
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP, reg_addr);
	swd_ap_read(dev, reg_addr);
	return swd_dp_read_rd_buff(dev, data_ptr);
}

static int syncboss_swd_nrf54h20_wait_swd_init(struct device *dev)
{
	int ret;
	u64 timeout_time_ns =
	    ktime_get_ns() + (SWD_INIT_TIMEOUT_MS * NSEC_PER_MSEC);

	/*
	 * If the mcu is not present or booted up after a reset on the other end of
	 * the swd lines, an swd_init can fail. Retry for the provided timeout
	 * before returning failure.
	 */
	while (ktime_get_ns() < timeout_time_ns) {
		ret = swd_init(dev);
		if (ret == 0)
			return ret;
		msleep(POLL_INTERVAL_MS);
	}
	dev_err(dev, "%s: SWD Init timeout\n", __func__);
	return -ETIMEDOUT;
}


static int syncboss_swd_nrf54h20_issue_reset(struct device *dev)
{
	int ret;

	dev_dbg(dev, "%s: reseting MCU via CtrlAP reset\n", __func__);
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			       SWD_NRF54H20_APREG_RESET);
	ret = swd_ap_write(dev, SWD_NRF54H20_APREG_RESET,
				 SWD_NRF54H20_APREG_RESET_Reset);
	if (ret) {
		dev_err(dev, "%s failed to write reset register\n", __func__);
		return ret;
	}
	swd_flush(dev);
	swd_deinit(dev);

	ret = syncboss_swd_nrf54h20_wait_swd_init(dev);
	if (ret) {
		dev_err(dev, "%s: swd_init timeout after reset\n", __func__);
		return ret;
	}

	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			       SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS);
	ret = syncboss_swd_wait_reg_value_mask(
		    dev, SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS,
		    0x0, SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_cmderr_mask,
		    BOOT_TIMEOUT_MS);
	if (ret) {
		dev_err(dev, "%s: reset command failed\n", __func__);
		return ret;
	}

	return 0;
}

int syncboss_swd_nrf54h20_force_sec_dom_fw_version(struct device *dev,
	const char *str)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int status = 0;

	if (strcmp(str, "suit") == 0) {
		devdata->sdfw_version = SWD_NRF54H20_SDFW_VERSION_SUIT;
	} else if (strcmp(str, "ironside") == 0) {
		devdata->sdfw_version = SWD_NRF54H20_SDFW_VERSION_IRONSIDE;
	} else {
		status = -EINVAL;
	}
	dev_info(dev, "Set SDFW Version to %s aka %d!\n", str, devdata->sdfw_version);

	return status;
}

static enum boot_stage syncboss_swd_nrf54h20_get_boot_stage(struct device *dev)
{
	int ret;
	u32 bootstatus, stage;

	ret = syncboss_swd_nrf54h20_read_ctrl_ap_reg(dev,
		SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS, &bootstatus);
	if (ret) {
		dev_err(dev, "%s: bootstatus read failed\n", __func__);
		return BOOT_STAGE_UNKNOWN;
	}
	dev_dbg(dev, "%s: bootstatus = %x", __func__, bootstatus);

	stage = (bootstatus & SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_stage_mask) >>
		SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_stage_shift;

	switch (stage) {
	case SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_unset_stage_val:
		return BOOT_STAGE_UNSET;
	case SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_sysrom_stage_val:
		return BOOT_STAGE_SYSROM;
	case SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_sdrom_stage_val:
		return BOOT_STAGE_SDROM;
	case SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_suit_stage_val:
		return BOOT_STAGE_SUIT;
	case SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_ironside_stage_val:
		return BOOT_STAGE_IRONSIDE;
	default:
		dev_err(dev, "%s: unexpected boot stage 0x%x\n", __func__, stage);
		return BOOT_STAGE_UNKNOWN;
	};
}

static void syncboss_swd_nrf54h20_detect_sec_dom_fw_version(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	/* bootstatus is read only once! a suit mcu can't change to an ironside mcu */
	if (devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_IRONSIDE ||
	    devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_SUIT) {
		dev_dbg(dev, "%s: sdfw version preset to %x\n", __func__, devdata->sdfw_version);
		return;
	}

	switch (syncboss_swd_nrf54h20_get_boot_stage(dev)) {
	case BOOT_STAGE_SUIT:
		devdata->sdfw_version = SWD_NRF54H20_SDFW_VERSION_SUIT;
		break;
	default:
		dev_err(dev, "%s: nrf54h20 Unknown SDFW version! Default to Ironside\n", __func__);
		fallthrough;
	case BOOT_STAGE_IRONSIDE:
		devdata->sdfw_version = SWD_NRF54H20_SDFW_VERSION_IRONSIDE;
		break;
	}
}

static int syncboss_swd_nrf54h20_wait_for_boot_stage(struct device *dev, enum boot_stage stage)
{
	int ret;
	u32 reg_val;

	switch (stage) {
	case BOOT_STAGE_SUIT:
		reg_val = SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_suit_stage_val;
		break;
	case BOOT_STAGE_IRONSIDE:
		reg_val = SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_ironside_stage_val;
		break;
	default:
		return -EINVAL;
	}

	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			  SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS);
	ret = syncboss_swd_wait_reg_value_mask(
		    dev, SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS,
		    reg_val, SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_stage_mask,
		    BOOT_TIMEOUT_MS);
	if (ret)
		return ret;

	/*
	 * Some accesses fail if we make them too soon after booting.
	 * Value is borrowed from vendor scripts but could likely
	 * be optimized for removed if we knew what we were actually
	 * waiting for.
	 */
	msleep(MCU_BOOT_DELAY_MS);

	return 0;
}

static int syncboss_swd_nrf54h20_chip_reboot_into_bootmode(struct device *dev,
							   u32 bootmode)
{
	int ret;

	/* Connect to CTRL-AP */
	dev_dbg(dev, "%s: setting bootmode to %d\n", __func__, bootmode);
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			  SWD_NRF54H20_APREG_MAILBOX_BOOTMODE);

	/* Select Boot Mode */
	ret = swd_ap_write(dev, SWD_NRF54H20_APREG_MAILBOX_BOOTMODE, bootmode);
	if (ret) {
		dev_err(dev, "%s: set bootmode failure\n", __func__);
		return ret;
	}

	/* Reset device using the CTRL-AP */
	ret = syncboss_swd_nrf54h20_issue_reset(dev);
	if (ret) {
		dev_err(dev, "%s: failed to write reset word\n", __func__);
		return ret;
	}

	/* Wait for Ironside to boot */
	ret = syncboss_swd_nrf54h20_wait_for_boot_stage(dev, BOOT_STAGE_IRONSIDE);
	if (ret) {
		dev_err(dev, "%s: ironside boot timeout after reset\n", __func__);
		return ret;
	}

	return 0;
}

static int syncboss_swd_nrf54h20_chip_erase_ironside(struct device *dev)
{
	int status = 0;
	u64 timeout_time_ns = 0;
	u32 bootstatus = 0x0;
	u8 opcode, cmd_error, boot_stage, fw_version, boot_error;

	/* 1. Set up the Ironside SDFW Eraseall command and issue a reboot */
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_EraseAll);
	if (status != 0) {
		dev_err(dev, "%s: ould not reset into EraseAll mode\n", __func__);
		goto error;
	}

	/* 2. Check the status of the erase command by reading the boot status word */
	timeout_time_ns =
	    ktime_get_ns() + (ERASE_TIMEOUT_MS * NSEC_PER_MSEC);
	while (ktime_get_ns() < timeout_time_ns) {
		syncboss_swd_nrf54h20_read_ctrl_ap_reg(dev,
				SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS, &bootstatus);

		if (bootstatus == 0x0) {
			msleep(POLL_INTERVAL_MS);
			continue;
		}

		boot_stage = (bootstatus & SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_stage_mask)
				>> SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_stage_shift;
		fw_version = (bootstatus & SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_fwver_mask)
				>> SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_fwver_shift;
		opcode     = (bootstatus & SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_opcode_mask)
				>> SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_opcode_shift;
		cmd_error  = (bootstatus & SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_cmderr_mask)
				>> SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_cmderr_shift;
		boot_error = (bootstatus & SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_booterr_mask)
				>> SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_booterr_shift;

		if (cmd_error == 0x0) {
			dev_dbg(dev, "%s: nrf reset success. bootstatus = 0x%x\n", __func__, bootstatus);
			status = 0;
		} else {
			dev_err(dev, "%s: NRF54H20 Boot Failure! stage:0x%x fw:0x%x opcode:0x%x err:0x%x boot_err:0x%x\n",
				__func__, boot_stage, fw_version, opcode, cmd_error, boot_error);
			status = -EIO;
			goto error;
		}
		break;
	}

	/* 3. Yank power, sorcery needed to make the erase all work on ironside */
	syncboss_deep_powercycle(dev);
	status = syncboss_swd_nrf54h20_wait_swd_init(dev);
	if (status) {
		dev_err(dev, "%s: swd init failed after deep powercycle\n", __func__);
		goto error;
	}

	status = syncboss_swd_nrf54h20_wait_for_boot_stage(dev, BOOT_STAGE_IRONSIDE);
	if (status) {
		dev_err(dev, "%s: ironside boot timeout after powercycle!\n", __func__);
		return status;
	}

	/*
	 * Step 4: Clear BOOTMODE to normal
	 */
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_Normal);
	if (status == 0)
		return status;

error:
	dev_err(dev, "%s: ironside erase failure\n", __func__);
	return status;
}

/*
 * Erase everything on the chip!
 */
int syncboss_swd_nrf54h20_chip_erase(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int ret;

	syncboss_swd_nrf54h20_detect_sec_dom_fw_version(dev);
	dev_dbg(dev, "%s: sdfw version == 0x%x\n", __func__, devdata->sdfw_version);

	if (devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_IRONSIDE) {
		dev_dbg(dev, "%s: syncboss erase ironside..\n", __func__);
		ret = syncboss_swd_nrf54h20_chip_erase_ironside(dev);
	} else {
		ret = -EIO;
	}
	return ret;
}

int syncboss_swd_nrf54h20_read(struct device *dev, int addr, u8 * const dest,
			       size_t len)
{
	int words = len / sizeof(u32);
	int bytes = len % sizeof(u32);
	int read_idx = 0;
	int ret;

	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	dev_dbg(dev, "%s: Read Addr 0x%08x Len %lu sdfw version %d\n",
			__func__, addr, len, devdata->sdfw_version);
	if (addr % sizeof(u32) != 0 || len % sizeof(u32) != 0) {
		dev_err(dev, "%s: read isn't word-aligned. Addr 0x%08x Len %lu\n",
			__func__, addr, len);
		return -EINVAL;
	}

	swd_select_ap(dev, SWD_NRF54H20_APSEL_APP_AHBAP);

	while (words > 0) {
		ret = swd_memory_read_robust(dev, addr, &((u32 *) dest)[read_idx]);
		if (ret) {
			dev_err(dev, "%s: SWD memory read word failure %d\n", __func__, ret);
			return ret;
		}
		read_idx++;
		dev_dbg(dev, "%s: Read: addr 0x%x Value 0x%x!", __func__, addr,
			((u32 *)dest)[read_idx - 1]);

		addr += sizeof(u32);
		words--;
	}

	if (bytes) {
		u32 val;

		ret = swd_memory_read_robust(dev, addr, &val);
		if (ret) {
			dev_err(dev, "%s: SWD memory read byte failure %d\n", __func__, ret);
			return ret;
		}

		read_idx = read_idx * sizeof(u32);

		memcpy(&dest[read_idx], &val, bytes);
	}

	return 0;
}

static bool syncboss_swd_nrf54h20_address_range_writable(
		u32 start_addr, u32 end_addr,
		struct flash_info *flash)
{
	if (flash->is_protected_region_valid) {
		if ((start_addr >= flash->protected_region.start_addr &&
		     start_addr <= flash->protected_region.end_addr) ||
		    (end_addr >= flash->protected_region.start_addr &&
		     end_addr <= flash->protected_region.end_addr)) {
			return false;
		}
	}
	return true;
}

static inline int syncboss_swd_nrfh20_write_memory_retry(struct device *dev,
							 int addr, u32 value)
{
	const int n_retries = 10;
	const int retry_interval_ms = 1;
	int i = 0;
	int ret;

	ret = swd_memory_write(dev, addr, value);
	while (unlikely(++i < n_retries && ret)) {
		msleep(retry_interval_ms);

		dev_warn_ratelimited(dev, "%s: write failure, err %d addr = %#x, retrying..", __func__, ret, addr);

		ret = swd_init(dev);
		if (ret) {
			dev_err(dev, "swd init failed, err %d, giving up write!", ret);
			return ret;
		}

		ret = swd_memory_write(dev, addr, value);
	}

	return ret;
}

int syncboss_swd_nrf54h20_write_chunk(struct device *dev, int addr,
					const u8 *data, size_t len)
{
	s32 i = 0;
	u32 value = 0;
	s32 start_addr_aligned;
	s32 end_addr_aligned;
	s32 end_addr;
	const u32 alignment = 16;
	const u32 words_in_section = 4;
	const u32 word_sz = sizeof(value);
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int ret;

	/* 1. Check Address ranges! */
	if (addr % sizeof(value) != 0) {
		dev_err(dev, "%s: write start address 0x%08X is not word-aligned.\n",
			__func__, addr);
		return -EINVAL;
	}

	if (addr + len > addr) {
		end_addr = addr + len;
	} else {
		dev_err(dev, "%s: invalid address! addr 0x%x len 0x%lx\n", __func__, addr, len);
		return -EINVAL;
	}

	/* 2. Check we arent writing to the BICR! Bad BICR can BRICK the mcu! */
	if (syncboss_swd_nrf54h20_address_range_writable(
		    addr, addr + len, &devdata->child_mcu_data[0].flash_info) ==
	    false) {
		dev_err(dev, "%s: can't write to protected addr range! 0x%x-0x%lx\n",
			__func__, addr, addr + len);
		return -EINVAL;
	}

	/* 3. Connect to the AHB */
	swd_select_ap(dev, SWD_NRF54H20_APSEL_APP_AHBAP);

	/* Start and End address need to be aligned to 16 bytes! */
	start_addr_aligned = addr & ~(alignment - 1);
	end_addr_aligned = (addr + len + alignment - 1) & ~(alignment - 1);

	/*
	 * 4. Write 16 byte sections, 4 byte words at a time, starting from the
	 * last section. Writes are not flushed to storage till the last word
	 * in the section is written. Pad any gaps at the end in alignment
	 * with FF.
	 */
	s32 curr_addr = end_addr_aligned - alignment;
	s32 data_idx = len - (end_addr - curr_addr);

	while (curr_addr >= start_addr_aligned) {
		/* write one 16 byte section! */
		for (i = 0; i < words_in_section;
		     i++, curr_addr += word_sz, data_idx += word_sz) {
			if (curr_addr >= end_addr) {
				value = 0xffffffff;
			} else if (curr_addr < addr) {
				/* Dont overwrite unaligned words at the start */
				continue;
			} else {
				value = *((u32 *)&data[data_idx]);
			}
			dev_dbg(dev,
				"%s: Write: curr_addr 0x%x end_addr 0x%x value 0x%x\n",
				__func__, curr_addr, end_addr, value);

			ret = syncboss_swd_nrfh20_write_memory_retry(
					dev, curr_addr, value);
			if (ret) {
				dev_err(dev,
					"%s: write failure! err %d addr = 0x%x\n",
					__func__, ret, curr_addr);
				return ret;
			}
		}

		curr_addr -= 2 * alignment;
		data_idx -= 2 * alignment;
	}
	dev_dbg(dev, "%s: chunk update complete! curr_addr 0x%x end_addr 0x%x value 0x%x\n",
				__func__, curr_addr, end_addr, value);

	return 0;
}

/*
 * Get the write granularity.
 * return: bytes
 */
size_t syncboss_swd_nrf54h20_get_app_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return devdata->child_mcu_data[0].flash_info.block_size;
}

size_t syncboss_swd_nrf54h20_get_net_write_chunk_size(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	return devdata->child_mcu_data[1].flash_info.block_size;
}

/*
 * Clear program flash region (but not provisioning region)
 * return: 0 on success
 */
int syncboss_swd_nrf54h20_target_erase(struct device *dev)
{
	return 0;
}

int syncboss_swd_nrf54h20_prepare(struct device *dev)
{
	/*
	 * As per Nordic, a new debug connection is not reliable or possible
	 * on nrf54h20 when the mcu cores are running. Reset the mcu which
	 * resets the debugging state, giving the debugger time to attach to
	 * the mcu cores. We should do this before and after flashing for
	 * robustness.
	 */
	int status;
	syncboss_deep_powercycle(dev);

	status = syncboss_swd_nrf54h20_wait_swd_init(dev);
	if (status != 0)
		dev_err(dev, "%s: Could not do an swd init!!", __func__);

	/*
	 * Its possible that an earlier attempt to flash failed and the mcu
	 * bootloader is corrupted. A corrupt bootloader on the mcu can make the
	 * SWD connection unstable and prevent any future SWD flashing. To avoid
	 * this, Boot into the DebugWait mode, that stops the CPU from booting
	 * into the bootloader, instead the CPU waits for SWD to attach.
	 */
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRF54H20_APREG_MAILBOX_BOOTMODE_DebugWait);
	if (status != 0)
		dev_err(dev, "%s: Could not reset into DebugWait mode\n", __func__);


	return syncboss_swd_nrf54h20_wait_swd_init(dev);
}

int syncboss_swd_nrf54h20_finalize(struct device *dev)
{
	int status;

	/* Clear BOOTMODE to normal */
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_Normal);
	if (status != 0) {
		dev_err(dev, "%s: Finalize failed! Reset into normal boot mode failed!", __func__);
		return status;
	}

	swd_reset(dev);
	swd_flush(dev);

	/* Disable swd_en & re-enable HPD IRQ at end of fw_init */
	syncboss_swd_setstate(dev, false);
	return 0;
}
