// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/meta-proglogic.h>
#include <linux/i2c.h>

#include <linux/of.h>
#include <linux/of_device.h>

#include "swd.h"
#include "syncboss_swd_common_ops.h"
#include "swd_registers_nrf54h20.h"
#include "syncboss_swd_ops_nrf54h20.h"

#define POWER_STATE_CHANGE_WAIT_MS	(200ll)

#define MBOX_TIMEOUT_MS			(1000ll)
#define POLL_INTERVAL_MS		(100ll)

#define SUIT_SWD_READY_TIMEOUT_MS	(500ll)
#define SUIT_BOOT_TIMEOUT_MS		(400ll)

#define IRONSIDE_ERASE_TIMEOUT_MS	(8000ll)
#define IRONSIDE_BOOT_TIMEOUT_MS	(8000ll)

static void syncboss_swd_mux_state(struct device *dev, bool swd_mux_state)
{
	struct device_node *np = dev->of_node;
	int count, i, ret;
	struct device_node *proglogic_dev_node;
	struct i2c_client *i2c_usb_mux_client;

	if (!np) {
		dev_err(dev, "no dtsi entry for nrf54h20!");
		return;
	}

	count = of_count_phandle_with_args(np, "meta,proglogic", NULL);
	if (count <= 0) {
		dev_err(dev, "nrf54h20 proglogic device not found in dts! %d", count);
		return;
	}
	dev_dbg(dev, "nrf54h20 found %d devices!", count);

	for (i = 0; i < count; i++) {
		proglogic_dev_node = of_parse_phandle(np, "meta,proglogic", i);
		if (!proglogic_dev_node)
			continue;
		i2c_usb_mux_client = of_find_i2c_device_by_node(proglogic_dev_node);

		of_node_put(proglogic_dev_node);
		if (i2c_usb_mux_client)
			break; /* Use the first device found */
	}

	if (i2c_usb_mux_client) {
		ret = swd_mux_state ?
			proglogic_swd_enable(&i2c_usb_mux_client->dev) :
			proglogic_swd_disable(&i2c_usb_mux_client->dev);
		put_device(&i2c_usb_mux_client->dev);
		if (ret)
			dev_err(dev, "nrf54h20 swd mux switch state fail %d!", ret);
	} else {
		dev_err(dev, "nrf54h20 failed to find swd mux device!");
	}
}

static void syncboss_deep_powercycle(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int status = 0;

	dev_dbg(dev, "nrf54h20 deep power cycle!");
	if (devdata->swd_core == NULL) {
		dev_err(dev, "nrf54h20 no regulator!!");
		return;
	}

	syncboss_swd_mux_state(dev, false);
	status = regulator_disable(devdata->swd_core);
	dev_dbg(dev, "nrf54h20 regulator disable status %d!", status);

	msleep(POWER_STATE_CHANGE_WAIT_MS);

	status = regulator_enable(devdata->swd_core);
	dev_dbg(dev, "nrf54h20 regulator enable status %d!", status);

	syncboss_swd_mux_state(dev, true);
}

static int syncboss_swd_nrf54h20_read_ctrl_ap_reg(struct device *dev,
						  u32 reg_addr, u32 *data_ptr)
{
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP, reg_addr);
	swd_ap_read(dev, reg_addr);
	return swd_dp_read_rd_buff(dev, data_ptr);
}

static int syncboss_swd_nrf54h20_write_ctrl_ap_reg(struct device *dev,
						    u32 reg_addr, u32 data)
{
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP, reg_addr);
	return swd_ap_write(dev, reg_addr, data);
}

static int syncboss_swd_nrf54h20_write_reset_word(struct device *dev)
{
	dev_dbg(dev, "Reset mcu using Ctrl AP reset!");
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			  SWD_NRF54H20_APREG_RESET);
	return swd_ap_write(dev, SWD_NRF54H20_APREG_RESET,
						SWD_NRF54H20_APREG_RESET_Reset);
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
	dev_info(dev, "Set SDFW Version to %s aka %d!", str, devdata->sdfw_version);

	return status;
}

static void syncboss_swd_nrf54h20_detect_sec_dom_fw_version(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	u32 bootstatus;

	/* bootstatus is read only once! a suit mcu can't change to an ironside mcu */
	if (devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_IRONSIDE ||
	    devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_SUIT) {
		dev_info(dev, "sdfw version preset to %x", devdata->sdfw_version);
		return;
	}

	if (syncboss_swd_nrf54h20_read_ctrl_ap_reg(
	    dev, SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS, &bootstatus) != 0) {
		dev_err(dev, "Ctrl AP Reg failed");
		return;
	}

	dev_info(dev, "bootstatus = %x", bootstatus);

	bootstatus = bootstatus & 0xff000000;
	if ((bootstatus == SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_suit_mask) ||
	    (bootstatus == 0x0)) {
		dev_err(dev, "Unsupported SDFW version SUIT! Default to Ironside");
	} else if (bootstatus !=
		   SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS_ironside_mask) {
		dev_err(dev, "nrf54h20 Unknown SDFW version! Default to Ironside");
	}
	devdata->sdfw_version = SWD_NRF54H20_SDFW_VERSION_IRONSIDE;
}

static bool syncboss_swd_nrf54h20_is_adac_needed(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	if ((devdata->sdfw_version != SWD_NRF54H20_SDFW_VERSION_SUIT) &&
		(devdata->sdfw_version != SWD_NRF54H20_SDFW_VERSION_IRONSIDE)) {
		syncboss_swd_nrf54h20_detect_sec_dom_fw_version(dev);
	}

	return (devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_SUIT);
}

/*
 * Wait for the Mbox TxStatus to turn 'not pending' and Write a u32 word to the
 * Mbox TxData register of the Ctrl AP. Assumes already connected to the CTRL AP
 */
static int syncboss_swd_nrf54h20_ctrl_ap_mbox_write_tx_data(struct device *dev,
							    u32 data)
{
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			  SWD_NRF54H20_APREG_MAILBOX_TXSTATUS);
	if (syncboss_swd_wait_reg_value(
		    dev, SWD_NRF54H20_APREG_MAILBOX_TXSTATUS,
		    SWD_NRF54H20_APREG_MAILBOX_TXSTATUS_NotPending,
		    MBOX_TIMEOUT_MS) == 0) {
		return syncboss_swd_nrf54h20_write_ctrl_ap_reg(
					dev, SWD_NRF54H20_APREG_MAILBOX_TXDATA, data);
	}
	return -ETIMEDOUT;
}

/*
 * Wait for the Mbox RxStatus to turn 'pending' and Read a u32 word from the
 * Mbox RxData register of the Ctrl AP. Assumes already connected to the CTRL AP
 */
static int syncboss_swd_nrf54h20_ctrl_ap_mbox_read_rx_data(struct device *dev,
							    u32 *data)
{
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			  SWD_NRF54H20_APREG_MAILBOX_RXSTATUS);
	if (syncboss_swd_wait_reg_value(
		    dev, SWD_NRF54H20_APREG_MAILBOX_RXSTATUS,
		    SWD_NRF54H20_APREG_MAILBOX_RXSTATUS_Pending,
		    MBOX_TIMEOUT_MS) == 0) {
		return syncboss_swd_nrf54h20_read_ctrl_ap_reg(
				dev, SWD_NRF54H20_APREG_MAILBOX_RXDATA, data);
	}
	return -ETIMEDOUT;
}

static int syncboss_swd_nrf54h20_wait_swd_init(struct device *dev, u64 timeout_ms)
{
	int ret;
	u64 timeout_time_ns =
	    ktime_get_ns() + (timeout_ms * NSEC_PER_MSEC);

	/*
	 * If the mcu is not present or booted up after a reset on the other end of
	 * the swd lines, an swd_init can fail. Retry for the provided timeout
	 * before returning failure.
	 */
	while (ktime_get_ns() < timeout_time_ns) {
		ret = swd_init(dev);
		if (ret == 0)
			return ret;
		dev_dbg(dev, "nrf54h20 sleep 100ms");
		msleep(POLL_INTERVAL_MS);
	}
	dev_err(dev, "nrf54h20 SWD Init timeout!");
	return -ETIMEDOUT;
}

/*
 * Reset the mcu running ironside sdfw, assumes that the reboot mode is already
 * set. Reinit SWD after reset and return status.
 */
int syncboss_swd_nrf54h20_reset_device_ironside(struct device *dev)
{
	int ret;

	/* Reset device using the CTRL-AP */
	ret = syncboss_swd_nrf54h20_write_reset_word(dev);
	if (ret) {
		dev_err(dev, "nrf54h20 failed to write reset word!");
		return ret;
	}
	swd_deinit(dev);
	msleep(POLL_INTERVAL_MS);

	/* Try to reinit SWD with retries if we fail */
	ret = syncboss_swd_nrf54h20_wait_swd_init(dev, IRONSIDE_BOOT_TIMEOUT_MS);
	if (ret) {
		dev_err(dev, "nrf54h20 ironside swd_init timedout after reset!");
		return ret;
	}

	return 0;
}

/*
 * Reset the nrf, assumes that we are already connected to CTRL-AP
 * and the reboot mode is already set
 */
static int syncboss_swd_nrf54h20_reset_device_suit(struct device *dev)
{
	u64 timeout_time_ns = 0;
	const u32 canary = 0x7FFF0000;
	int ret;
	u32 data;

	/* Connect to CTRL-AP */
	dev_dbg(dev, "Ctrl AP select!");
	ret = syncboss_swd_nrf54h20_write_ctrl_ap_reg(
			dev, SWD_NRF54H20_APREG_MAILBOX_TXDATA, canary);
	if (ret) {
		dev_err(dev, "nrf54h20 failed to write ctrl ap reg!");
		return ret;
	}

	/* Reset device using the CTRL-AP */
	ret = syncboss_swd_nrf54h20_write_reset_word(dev);
	if (ret) {
		dev_err(dev, "nrf54h20 failed to write reset word!");
		return ret;
	}

	swd_deinit(dev);
	msleep(SUIT_BOOT_TIMEOUT_MS);

	/* Reinit SWD */
	ret = syncboss_swd_nrf54h20_wait_swd_init(dev, SUIT_BOOT_TIMEOUT_MS);
	if (ret) {
		dev_err(dev, "nrf54h20 suit swd_init timedout after reset!");
		return ret;
	}

	/* Wait for Canary in TxData to be cleared */
	timeout_time_ns =
		ktime_get_ns() + (SUIT_SWD_READY_TIMEOUT_MS	* NSEC_PER_MSEC);
	while (ktime_get_ns() < timeout_time_ns) {
		dev_dbg(dev, "Read Ctrl AP TxData Canary!");
		if ((syncboss_swd_nrf54h20_read_ctrl_ap_reg(
			    dev, SWD_NRF54H20_APREG_MAILBOX_TXDATA, &data) == 0x0) && data == 0) {
			dev_dbg(dev, "Canary cleared! Reset successful");
			break;
		}
		msleep(100);
	}

	/* Wait for CtrlAP Ready to be 0 */
	timeout_time_ns =
		ktime_get_ns() + (SUIT_SWD_READY_TIMEOUT_MS	* NSEC_PER_MSEC);
	while (ktime_get_ns() < timeout_time_ns) {
		dev_dbg(dev, "Read Ctrl AP Ready!");
		if ((syncboss_swd_nrf54h20_read_ctrl_ap_reg(
			    dev, SWD_NRF54H20_APREG_READY, &data) == 0x0) &&
		     data == SWD_NRF54H20_APREG_READY_Ready) {
			dev_dbg(dev, "Ctrl AP Ready set!");
			return 0;
		}
		msleep(100);
	}
	dev_err(dev, "nrf54H20 Reset Failed! Ctrl AP Ready not set!");

	return -ETIMEDOUT;
}

static int syncboss_swd_nrf54h20_chip_reboot_into_bootmode(struct device *dev,
							   u32 bootmode)
{
	int ret;
	struct swd_dev_data *devdata;

	/* Connect to CTRL-AP */
	dev_dbg(dev, "%s Set bootmode %d!", __func__, bootmode);
	swd_select_ap_reg(dev, SWD_NRF54H20_APSEL_DEVICE_CTRLAP,
			  SWD_NRF54H20_APREG_MAILBOX_BOOTMODE);

	ret = swd_ap_write(dev, SWD_NRF54H20_APREG_MAILBOX_BOOTMODE, bootmode);
	if (ret) {
		dev_err(dev, "%s Set bootmode failure!", __func__);
		return ret;
	}

	devdata = dev_get_drvdata(dev);
	if (devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_IRONSIDE)
		ret = syncboss_swd_nrf54h20_reset_device_ironside(dev);
	else
		ret = syncboss_swd_nrf54h20_reset_device_suit(dev);

	return ret;
}

static int syncboss_swd_nrf54h20_tx_sdfw_cmd(struct device *dev,
					     u32 sdfw_req_word,
					     u32 data_count_bytes,
					     const u32 *data)
{
	u32 lcs_change_response = 0x0;
	u32 status = 0x0;
	u32 data_count = 0;
	u32 data_idx = 0;
	u32 num_words = 0;

	/*
	 * Write the command request to mbox
	 * Command: write cmd, data len 4, 32-bit domain ID
	 */
	dev_dbg(dev, "%s write sdfw_req_word! 0x%x", __func__, sdfw_req_word);
	syncboss_swd_nrf54h20_ctrl_ap_mbox_write_tx_data(dev, sdfw_req_word);
	dev_dbg(dev, "%s write data_count=%d bytes", __func__,
		data_count_bytes);
	syncboss_swd_nrf54h20_ctrl_ap_mbox_write_tx_data(dev, data_count_bytes);
	num_words = (data_count_bytes + sizeof(*data) - 1) / sizeof(*data);
	while (data_idx < num_words && data != NULL) {
		dev_dbg(dev, "%s write data 0x%x", __func__, data[data_idx]);
		syncboss_swd_nrf54h20_ctrl_ap_mbox_write_tx_data(
			dev, data[data_idx]);
		data_idx++;
	}

	/* Read LCS Change response */
	dev_dbg(dev, "%s read lcs change response", __func__);
	status = syncboss_swd_nrf54h20_ctrl_ap_mbox_read_rx_data(dev,
			&lcs_change_response);
	if (status) {
		dev_err(dev, "Read LCS Change response failed");
		return status;
	}

	status = syncboss_swd_nrf54h20_ctrl_ap_mbox_read_rx_data(dev, &data_count);
	if (status) {
		dev_err(dev, "Read LCS Change response data cnt failed");
		return status;
	}

	status = (lcs_change_response >> 16) & 0xFFFF;

	if (status != 0) {
		dev_err(dev, "SDFW_ADAC response 0x%x", lcs_change_response);
		dev_err(dev, "SDFW_ADAC response status 0x%x data_count 0x%x",
			status, data_count);
	} else {
		dev_dbg(dev, "SDFW_ADAC response 0x%x", lcs_change_response);
		dev_dbg(dev, "SDFW_ADAC response status 0x%x data_count 0x%x",
			status, data_count);
	}

	num_words = (data_count + sizeof(data_count) - 1) / sizeof(data_count);
	while (num_words > 0) {
		u32 data_resp;

		syncboss_swd_nrf54h20_ctrl_ap_mbox_read_rx_data(dev, &data_resp);

		if (status != 0)
			dev_err(dev, "%s read data =0x%x", __func__, data_resp);

		num_words--;
	}

	return status;
}

/*
 * Send a domain purge request to SUIT secure domain fw for one domain (radio/app)
 */
static int syncboss_swd_nrf54h20_adac_domain_purge_suit(struct device *dev,
							u32 domain_id)
{
	const u32 sdfw_adac_domain_purge_cmd = 0xA308;
	const u32 sdfw_req_word = (sdfw_adac_domain_purge_cmd << 16);
	u32 data_count = 4;

	dev_dbg(dev, "SDFW_ADAC cmd Purge Suit for domain 0x%x", domain_id);
	return syncboss_swd_nrf54h20_tx_sdfw_cmd(dev, sdfw_req_word, data_count,
						 &domain_id);
}

int syncboss_swd_nrf54h20_lcs_discovery(struct device *dev)
{
	const u32 sdfw_adac_discovery_cmd = 0x0001;
	const u32 sdfw_req_word = (sdfw_adac_discovery_cmd << 16);
	u32 data_count = 0x0;

	u32 status = 0x0;

	/* Reset MCU into ROM Mode! */
	dev_dbg(dev, "Reboot device into ROM mode!");
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_Rom);
	if (status != 0)
		goto error;

	dev_dbg(dev, "Send SDFW_ADAC cmd ADAC discovery command!");
	syncboss_swd_nrf54h20_tx_sdfw_cmd(dev, sdfw_req_word, data_count, NULL);

	dev_dbg(dev, "Reboot device into normal mode!");
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_Normal);

error:
	return status;
}

static int syncboss_swd_nrf54h20_chip_erase_suit(struct device *dev)
{
	int status = 0;

	/* Reboot device into recovery app */
	dev_dbg(dev, "Reboot device into recovery app!");
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_SafeOp);
	if (status != 0)
		goto error;

	dev_dbg(dev, "Purge Application Core!");
	status = syncboss_swd_nrf54h20_adac_domain_purge_suit(
		dev, SWD_NRF54H20_SDFW_APPLICATION_OWNER_ID);
	if (status != 0)
		goto error;

	dev_dbg(dev, "Purge Radio Core!");
	status = syncboss_swd_nrf54h20_adac_domain_purge_suit(
		dev, SWD_NRF54H20_SDFW_RADIO_OWNER_ID);
	if (status != 0)
		goto error;

	dev_dbg(dev, "Reboot device into normal mode!");
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_Normal);
	if (status != 0)
		goto error;
error:
	return status;
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
		dev_err(dev, "nrf54h20 Could not reset into EraseAll mode!");
		goto error;
	}

	/* 2. Check the status of the erase command by reading the boot status word */
	timeout_time_ns =
	    ktime_get_ns() + (IRONSIDE_ERASE_TIMEOUT_MS * NSEC_PER_MSEC);
	while (ktime_get_ns() < timeout_time_ns) {
		syncboss_swd_nrf54h20_read_ctrl_ap_reg(dev,
				SWD_NRF54H20_APREG_MAILBOX_BOOTSTATUS, &bootstatus);

		if (bootstatus == 0x0) {
			msleep(POLL_INTERVAL_MS);
			continue;
		}

		boot_stage = (bootstatus >> 24) & 0xf;  /* bits 27:24 */
		fw_version = (bootstatus >> 15) & 0x7f; /* bits 21:15 */
		opcode     = (bootstatus >> 12) & 0x7;  /* bits 14:12 */
		cmd_error  = (bootstatus >>  9) & 0x7;  /* bits 11: 9 */
		boot_error = (bootstatus & 0xff);       /* bits  7: 0 */

		if ((opcode == 0x1) && (cmd_error == 0x0)) {
			dev_dbg(dev, "nrf reset success. bootstatus = 0x%x!", bootstatus);
			status = 0;
		} else {
			dev_err(dev, "!!!NRF54H20 BootFailure. Status Word!!!");
			dev_err(dev, "status word stage:0x%x fw:0x%x opcode:0x%x err:0x%x boot_err:0x%x",
				boot_stage, fw_version, opcode, cmd_error, boot_error);
			status = -EIO;
			goto error;
		}
		break;
	}

	/* 3. Yank power, sorcery needed to make the erase all work on ironside */
	syncboss_deep_powercycle(dev);
	status = syncboss_swd_nrf54h20_wait_swd_init(dev, IRONSIDE_BOOT_TIMEOUT_MS);
	if (status) {
		dev_err(dev, "swd init failed after deep powercycle!");
		goto error;
	}

	/*
	 * Step 4: Clear BOOTMODE to normal
	 */
	status = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_Normal);
	if (status == 0)
		return status;

error:
	dev_err(dev, "nrf54h20 Ironside erase failure!");
	return status;
}

/*
 * Erase everything on the chip!
 */
int syncboss_swd_nrf54h20_chip_erase(struct device *dev)
{
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	int ret;

	syncboss_deep_powercycle(dev);
	syncboss_swd_nrf54h20_wait_swd_init(dev, IRONSIDE_BOOT_TIMEOUT_MS);

	syncboss_swd_nrf54h20_detect_sec_dom_fw_version(dev);
	dev_dbg(dev, "sdfw version == 0x%x!", devdata->sdfw_version);

	if (devdata->sdfw_version == SWD_NRF54H20_SDFW_VERSION_IRONSIDE) {
		dev_dbg(dev, "syncboss erase ironside..");
		ret = syncboss_swd_nrf54h20_chip_erase_ironside(dev);
	} else {
		dev_dbg(dev, "syncboss erase suit..");
		ret = syncboss_swd_nrf54h20_chip_erase_suit(dev);
	}
	return ret;
}

static int syncboss_swd_nrf54h20_adac_start(struct device *dev)
{
	const u32 sdfw_adac_start_cmd = 0xA309;
	const u32 sdfw_req_word = (sdfw_adac_start_cmd << 16);
	const u32 data =
		0x01020000; /* Reserved, domain ID 2 (app core), flags 1 (HALT) */
	const u32 data_count = sizeof(data);

	dev_dbg(dev, "Send SDFW_ADAC cmd ADAC Start!");
	return syncboss_swd_nrf54h20_tx_sdfw_cmd(dev, sdfw_req_word, data_count,
						 &data);
}

static int syncboss_swd_nrf54h20_adac_mem_cfg(struct device *dev, u32 owner_id,
					      u32 address, u32 length)
{
	const u32 sdfw_adac_mem_cfg = 0xA301;
	const u32 sdfw_req_word = (sdfw_adac_mem_cfg << 16);
	u32 data[3];
	const u32 data_count = sizeof(data);

	data[0] = owner_id;
	data[1] = address;
	data[2] = length;
	dev_dbg(dev, "Send SDFW_ADAC cmd ADAC Mem Config %d %d %d %d!",
		data_count, data[0], data[1], data[2]);
	return syncboss_swd_nrf54h20_tx_sdfw_cmd(dev, sdfw_req_word, data_count,
						 data);
}

static int syncboss_swd_nrf54h20_adac_config(struct device *dev, u32 start_addr,
					     u32 len)
{
	const u32 pg_alignment = 4096;
	int ret = syncboss_swd_nrf54h20_adac_start(dev);

	if (ret < 0) {
		dev_err(dev, "ADAC Start Failure! %d", ret);
		return ret;
	}

	dev_dbg(dev, "ADAC config start_addr 0x%x len 0x%x", start_addr, len);

	/* Request must be 4k aligned, and a multiple of 4k length */
	ret = syncboss_swd_nrf54h20_adac_mem_cfg(
		dev, SWD_NRF54H20_SDFW_APPLICATION_OWNER_ID,
		start_addr & ~(pg_alignment - 1),
		(len + pg_alignment - 1) & ~(pg_alignment - 1));
	if (ret != 0) {
		dev_err(dev, "ADAC MemCfg Failure! %d addr 0x%x len %u", ret,
			start_addr, len);
		return ret;
	}
	return 0;
}

int syncboss_swd_nrf54h20_read(struct device *dev, int addr, u8 * const dest,
			       size_t len)
{
	int words = len / sizeof(u32);
	int bytes = len % sizeof(u32);
	int read_idx = 0;
	int ret;

	const u32 chunk_sz = syncboss_swd_nrf54h20_get_app_write_chunk_size(dev);
	struct swd_dev_data *devdata = dev_get_drvdata(dev);
	bool need_adac = syncboss_swd_nrf54h20_is_adac_needed(dev);

	dev_dbg(dev, "Read Addr 0x%08x Len %lu sdfw version %d", addr, len, devdata->sdfw_version);
	if (addr % sizeof(u32) != 0 || len % sizeof(u32) != 0) {
		dev_err(dev, "Read isn't word-aligned. Addr 0x%08x Len %lu",
			addr, len);
		return -EINVAL;
	}

	/* 4. Connect to the AHB */
	ret = syncboss_swd_nrf54h20_chip_reboot_into_bootmode(
		dev, SWD_NRD45H20_APREG_MAILBOX_BOOTMODE_Normal);
	if (ret)
		return ret;

	msleep(500);
	ret = swd_init(dev);
	if (ret) {
		dev_err(dev, "nrf54h20 SWD Init failure!");
		return ret;
	}
	msleep(500);

	/* config the adac from the first chunk boundary before or at addr */
	if (need_adac &&
	    syncboss_swd_nrf54h20_adac_config(dev, addr, len) != 0) {
		dev_err(dev, "ADAC cfg failure! addr 0x%x len %u", addr,
			chunk_sz);
		return -EINVAL;
	}

	swd_select_ap(dev, SWD_NRF54H20_APSEL_APP_AHBAP);

	while (words > 0) {
		ret = swd_memory_read_robust(dev, addr, &((u32 *) dest)[read_idx]);
		if (ret) {
			dev_err(dev, "nrf54h20 SWD memory read failure! %d", ret);
			return ret;
		}
		read_idx++;
		dev_dbg(dev, "Read: addr 0x%x Value 0x%x!", addr,
			((u32 *)dest)[read_idx - 1]);

		addr += sizeof(u32);
		words--;
	}

	if (bytes) {
		u32 val;

		ret = swd_memory_read_robust(dev, addr, &val);
		if (ret) {
			dev_err(dev, "nrf54h20 SWD memory read failure! %d", ret);
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
	bool need_adac = syncboss_swd_nrf54h20_is_adac_needed(dev);

	/* 1. Check Address ranges! */
	if (addr % sizeof(value) != 0) {
		dev_err(dev, "Write start address 0x%08X is not word-aligned.",
			addr);
		return -EINVAL;
	}

	if (addr + len > addr) {
		end_addr = addr + len;
	} else {
		dev_err(dev, "Invalid address! addr 0x%x len 0x%lx", addr, len);
		return -EINVAL;
	}

	/* 2. Check we arent writing to the BICR! Bad BICR can BRICK the mcu! */
	if (syncboss_swd_nrf54h20_address_range_writable(
		    addr, addr + len, &devdata->child_mcu_data[0].flash_info) ==
	    false) {
		dev_err(dev, "Cant write to protected addr range! 0x%x-0x%lx",
			addr, addr + len);
		return -EINVAL;
	}

	/* 3. Configure the ADAC, if needed! */
	if (need_adac && syncboss_swd_nrf54h20_adac_config(dev, addr, len) != 0) {
		dev_err(dev, "ADAC Config failure!");
		return -EINVAL;
	}

	/* 4. Connect to the AHB */
	ret = swd_init(dev);
	if (ret == 0) {
		swd_select_ap(dev, SWD_NRF54H20_APSEL_APP_AHBAP);
	} else {
		dev_err(dev, "SWD Init failure!");
		return ret;
	}

	/* Start and End address need to be aligned to 16 bytes! */
	start_addr_aligned = addr & ~(alignment - 1);
	end_addr_aligned = (addr + len + alignment - 1) & ~(alignment - 1);

	/*
	 * 5. Write 16 byte sections, 4 byte words at a time, starting from the
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
				"Write: curr_addr 0x%x end_addr 0x%x value 0x%x",
				curr_addr, end_addr, value);

			ret = syncboss_swd_nrfh20_write_memory_retry(
					dev, curr_addr, value);
			if (ret) {
				dev_err(dev,
					"nrf54h20 write failure! err %d addr = 0x%x",
					ret, curr_addr);
				return ret;
			}
		}

		curr_addr -= 2 * alignment;
		data_idx -= 2 * alignment;
	}
	dev_dbg(dev, "chunk update complete! curr_addr 0x%x end_addr 0x%x value 0x%x",
				curr_addr, end_addr, value);

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

int syncboss_swd_nrf54h20_read_part_number(struct device *dev, u32 *partnum)
{
	int ret = swd_init(dev);

	if (ret) {
		dev_err(dev, "nrf54h20 SWD Init failure!");
		return ret;
	}
	swd_select_ap(dev, SWD_NRF54H20_APSEL_APP_AHBAP);
	ret = swd_memory_read_robust(dev, SWD_NRF54H20_FICR_APP_BASE |
				    SWD_NRF54H20_FICR_PART_OFFSET, partnum);
	if (ret) {
		dev_err(dev, "nrf54h20 SWD Read failure!");
		return ret;
	}
	return 0;
}
