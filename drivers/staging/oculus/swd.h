#ifndef _SWD_H
#define _SWD_H

#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/workqueue.h>

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

/* Read 4 bytes of memory from a given address */
u32 swd_memory_read(struct device *dev, u32 address);

/* Read 4 bytes of memory from the next address after the previously-read/written word */
u32 swd_memory_read_next(struct device *dev);

/* Write 4 bytes of memory at a given address */
void swd_memory_write(struct device *dev, u32 address, u32 data);

/* Write 4 bytes of memory to the next address after the previously-read/written word */
void swd_memory_write_next(struct device *dev, u32 data);

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
	FW_UPDATE_STATE_WRITING_TO_HW
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
	 * Check if provisioning should be forced
	 * return: true if so, false otherwise
	 */
	bool (*should_force_provision)(struct device *dev);

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
};

struct swd_ops {
	/* Check if the device is busy */
	bool (*is_busy)(struct device *dev);

	/* Handler to call when a firmware update has completed */
	void (*fw_update_cb)(struct device *dev, int status);
};

struct flash_info {
	u32 block_size;
	u32 page_size;
	u32 num_pages;
	u32 num_retained_pages;
};

struct swd_dev_data {
	/* Mutex that protects the state of this structure */
	struct mutex state_mutex;

	/* workqueue associated with device */
	struct workqueue_struct *workqueue;

	/* GPIO line for swdclk */
	int gpio_swdclk;

	/* GPIO line for swdio */
	int gpio_swdio;

	/* Regulator associated with swd interface */
	struct regulator *swd_core;

	/* Number of firwmare chunks written (if any) */
	atomic_t fw_chunks_written;

	/* Number of firwmare chunks to be written (if any) */
	atomic_t fw_chunks_to_write;

	/* Path to firmware e.g. "syncboss.bin" */
	const char *fw_path;

	/* Firmware to flash during update */
	const struct firmware *fw;

	/* State of firmware update */
	enum fw_update_state fw_update_state;

	/* Called prior to updating firmware to ensure peripherial is idle. */
	bool (*is_busy)(struct device *dev);

	/* Called after updating firmware. */
	void (*on_firmware_update_complete)(struct device *dev, int status);

	/* SWD operations implementation */
	struct swd_ops_params swd_ops;

	/* Flash memory organization */
	struct flash_info flash_info;

	/* Direction of current SWD transfer */
	enum swd_direction direction;

	/*
	 * Whether provisioning data should be programmed via SWD (ex. board_id,
	 * serial, handedness)
	 */
	bool swd_provisioning;

	/* Provisioning data, when swd_provisioning is true. */
	struct provisioning *provisioning;
};

ssize_t fwupdate_show_update_firmware(struct device *dev, char *buf);

ssize_t fwupdate_store_update_firmware(struct device *dev,
				       const char *buf, size_t count);

int fwupdate_init_swd_ops(struct device *dev, const char *swdflavor);

#endif
