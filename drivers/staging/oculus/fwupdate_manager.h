#ifndef _FWUPDATE_MANAGER_H
#define _FWUPDATE_MANAGER_H

#include <linux/firmware.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <syncboss_swd.h>

/* Firmware state */
enum syncboss_fw_update_state {
	SYNCBOSS_FW_UPDATE_STATE_IDLE,
	SYNCBOSS_FW_UPDATE_STATE_WRITING_TO_HW
};

/* status: Result of attempting to update firmware. */
/* dev: Device of the fwupdate object */
typedef void (*fwupdate_cb_t)(int status, struct device *dev);

struct swd_ops_params_t {
	/* Flash memory block size */
	int block_size;

	/* Max allowed fw size, in bytes */
	ssize_t max_fw_size;

	/* Called before programming starts
	 * return: 0 on success
	 */
	int (*target_erase)(struct device *dev, struct swdhandle_t *handle);

	/* Write a single block to target flash
	 * return: 0 success
	 */
	int (*target_program_write_block)(struct device *dev,
					  struct swdhandle_t *handle,
					  int start_addr,
					  const u8 *data,
					  int len);

	/* Called after programming done, perform cleanup steps
	 * return: 0 on success
	 */
	int (*target_program_cleanup)(struct device *dev,
				      struct swdhandle_t *handle);
};

/* Device state */
struct fwupdate_data {
	/* Device of the underlying driver.*/
	struct device *dev;

	/* Mutex that protects the state of this structure */
	struct mutex state_mutex;

	/* GPIO line for swdclk */
	int gpio_swdclk;
	/* GPIO line for swdio */
	int gpio_swdio;

	/* Number of firwmare blocks written (if any) */
	atomic_t fw_blocks_written;

	/* Number of firwmare blocks to be written (if any) */
	atomic_t fw_blocks_to_write;

	/* Path to firmware e.g. "syncboss.bin" */
	const char *fw_path;

	/* Firmware to flash during update */
	const struct firmware *fw;

	/* State of firmware update */
	enum syncboss_fw_update_state fw_update_state;

	/* Called after updating firmware. */
	fwupdate_cb_t on_firmware_update_complete;

	/* SWD operations implementation */
	struct swd_ops_params_t swd_ops;
};

ssize_t fwupdate_show_update_firmware(struct device *dev,
				     struct fwupdate_data *fwudata, char *buf);

ssize_t fwupdate_store_update_firmware(struct workqueue_struct *workqueue,
						 struct device *dev,
				     struct fwupdate_data *fwudata,
				     const char *buf, size_t count);

int fwupdate_init_swd_ops(struct device *dev, struct fwupdate_data *fwudata,
				const char *swdflavor);

#endif
