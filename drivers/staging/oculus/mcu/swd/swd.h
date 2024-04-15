/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SWD_H
#define _SWD_H

#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "../fw_helpers.h"
#include "fwupdate_manager.h"

/* Initialize the SWD GPIO state */
void swd_init(struct device *dev);

/* Deinit the SWD GPIO state */
void swd_deinit(struct device *dev);

/* Stop the target */
void swd_halt(struct device *dev);

/*
 * Flush the SW-DP. Should be called after the last SP transaction to be executed
 * to guarantee it actually takes effect.
 */
void swd_flush(struct device *dev);

/* Select the Access Port */
void swd_select_ap(struct device *dev, u8 apsel);

/* Write a word to the selected SWD Access Port */
void swd_ap_write(struct device *dev, u8 reg, u32 data);

/* Read a word from the selected SWD Access Port */
u32 swd_ap_read(struct device *dev, u8 reg);

/* Read 4 bytes of memory from a given address */
u32 swd_memory_read(struct device *dev, u32 address);

/* Read 4 bytes of memory from the next address after the previously-read/written word */
u32 swd_memory_read_next(struct device *dev);

/* Write 4 bytes of memory at a given address */
void swd_memory_write(struct device *dev, u32 address, u32 data);

/* Write 4 bytes of memory to the next address after the previously-read/written word */
void swd_memory_write_next(struct device *dev, u32 data);

/*
 * Write the next 4 bytes of memory, but don't bitbang the last bit until some minimum
 * time has passed to allow for the previous write to finish.
 */
void swd_memory_write_next_delayed(struct device *dev, u32 data, u32 delay_us);

/* Reset the target */
void swd_reset(struct device *dev);

/* SWD transfer directions (relative to master) */
enum swd_direction {
	SWD_DIRECTION_IN = 0,
	SWD_DIRECTION_OUT
};

/* Firmware state */
enum fw_update_state {
	FW_UPDATE_STATE_IDLE,
	FW_UPDATE_STATE_WRITING_TO_HW,
	FW_UPDATE_STATE_ERROR
};

struct swd_ops_params {
	/*
	 * Read provisioning data from the target flash
	 * return: 0 success
	 */
	int (*provisioning_read)(struct device *dev,
				 int addr,
				 u8 *data,
				 size_t len);

	/*
	 * Write the provisioning data to target flash
	 * return: 0 success
	 */
	int (*provisioning_write)(struct device *dev,
				  int addr,
				  u8 *data,
				  size_t len);

	/*
	 * Perform any target-specific configuration needed priort to a FW update
	 * return: 0 on success
	 */
	int (*target_prepare)(struct device *dev);

	/*
	 * Clear program flash region (but not provisioning region)
	 * return: 0 on success
	 */
	int (*target_erase)(struct device *dev);

	/*
	 * Perform a chip erase, which erases everything including provisioning region.
	 * return: 0 on success
	 */
	int (*target_chip_erase)(struct device *dev);

	/*
	 * Write a single chunk to target flash
	 * return: 0 success
	 */
	int (*target_program_write_chunk)(struct device *dev,
					  int start_addr,
					  const u8 *data,
					  size_t len);

	/*
	 * Get the write granularity.
	 * return: bytes
	 */
	size_t (*target_get_write_chunk_size)(struct device *dev);

	/*
	 * Read from flash
	 * return: 0 success
	 */
	int (*target_program_read)(struct device *dev,
				   int start_addr,
				   u8 *dest,
				   size_t len);

	/*
	 * Check whether a flash page is already erased
	 * return: true if erased, false otherwise or on error.
	 */
	bool (*target_page_is_erased)(struct device *dev, u32 page);

	/*
	 * Final steps after SWD update is completed. E.g., verify success.
	 */
	int (*target_finalize)(struct device *dev);
};

struct flash_info {
	u32 block_size;
	u32 page_size;
	u32 num_pages;
	/* Retained pages at end of flash */
	u32 num_retained_pages;
	/* Retained pages at beginning of flash */
	u32 num_protected_bootloader_pages;
	/* For devices that have multple banks of flash, i.e. stm32l */
	u32 bank_count;
};

struct swd_mcu_data {
	/* Path to firmware e.g. "syncboss.bin" */
	const char *fw_path;

	/* Target flavor e.g. "nrf5340" */
	const char *target_flavor;

	/* Firmware to flash during update */
	const struct firmware *fw;

	/* Address of MEM-AP. Defaults to using the first AP (address 0). */
	u32 mem_ap;

	/* Number of firwmare chunks written (if any) */
	atomic_t fw_chunks_written;

	/* Number of firwmare chunks to be written (if any) */
	atomic_t fw_chunks_to_write;

	/* SWD operations implementation */
	struct swd_ops_params swd_ops;

	/* Flash memory organization */
	struct flash_info flash_info;
};

struct swd_dev_data {
	/* Mutex that protects the state of this structure */
	struct mutex state_mutex;

	/* workqueue associated with device */
	struct workqueue_struct *workqueue;

	/* GPIO line for MCU reset */
	int gpio_reset;

	/* GPIO line for swdclk */
	int gpio_swdclk;

	/* GPIO line for swdio */
	int gpio_swdio;

	/* Regulator associated with swd interface */
	struct regulator *swd_core;

	/* State of firmware update */
	enum fw_update_state fw_update_state;

	/* For ensuring the MCU is idle (ie. not streaming) during a firmware update. */
	void (*mcu_state_lock)(struct device *dev);
	void (*mcu_state_unlock)(struct device *dev);
	bool (*get_syncboss_is_streaming)(struct device *dev);
	bool mcu_state_locked;

	/* Direction of current SWD transfer */
	enum swd_direction direction;

	/* Header of data passed from user space */
	struct fwupdate_header *data_hdr;

	/*
	 * Whether provisioning data should be programmed via SWD (ex. board_id,
	 * serial, handedness)
	 */
	bool swd_provisioning;

	/* Provisioning data, when swd_provisioning is true. */
	struct fwupdate_provisioning *provisioning;

	struct swd_mcu_data mcu_data;

	/* Perform a full chip erase before writing firmware */
	bool erase_all;

	/* swd_dev_data for any system that has multiple MCUs or cores sharing an SWD */
	struct swd_mcu_data *child_mcu_data;
	int num_children;

	struct dentry *debug_entry;
};

ssize_t fwupdate_update_firmware_show(struct device *dev, char *buf);

ssize_t fwupdate_update_firmware_store(struct device *dev,
				       const char *buf, size_t count);

int fwupdate_init_swd_ops(struct device *dev, struct swd_mcu_data *mcudata);

#endif
