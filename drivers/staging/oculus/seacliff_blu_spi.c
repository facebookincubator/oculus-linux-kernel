// SPDX-License-Identifier: GPL-2.0-only

#include <linux/backlight.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/property.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include "linux/seacliff_blu_spi.h"
#include "seacliff_bro_profiles.h"

/* Compatible table name of the blu */
#define BLU_SPI_NODE_TABLE "oculus,seacliff-blu-spi"

/* Parameters of SPI devices */
#define BLU_SPI_MODE SPI_MODE_0
#define BLU_SPI_BITS_PER_WORD 8

/* Frames required to stablilize the BLU after power on */
#define STABLE_FRAME_COUNTS 10

#define BL_NODE_NAME_SIZE 32

/* Delay required between SPI messages for 8 MHz clock */
#define MIN_SPI_DELAY_US 1000
#define MAX_SPI_DELAY_US 1500

/* Parameters for SPI debug messages */
#define MAX_TRANSACTION_LENGTH 512
#define IRQ_DISABLE_WAIT 3

/* convert cross-eye brightness scaler back into a float*/
#define BRIGHTNESS_SCALE_CONVERT 10000

/* start index for backlight matrix after the SPI values */
#define MATRIX_START_INDEX 5

/* keep track of the number of driver instances */
static int number_of_devices;

struct blu_device {
	const char *name;

	/* SPI device */
	struct spi_device *spi;

	/* misc device for receiving the backlight matrix*/
	struct miscdevice misc;

	/* Backlight device */
	struct backlight_device *bl_device;

	struct workqueue_struct *work_queue;
	struct work_struct work_blm;
	struct work_struct work_msg;

	/* irq to trigger spi command transfer */
	int irq;
	/* frame counts since the blu powers on */
	u64 frame_counts;
	/* Backlight level, if 0 then disable irq */
	int backlight_level;
	int max_brightness;
	int brightness_scaler;

	/* The arrray contains brightness info of 8 muxes */
	u8 *backlight_matrix;
	u32 matrix_size;

	/* backlight rolloff compensation values */
	u8 *bro_profile_default;
	u32 rolloff_size;

	/* indices of the BLU that do not contain LEDs */
	u32 *corner_zone_indices;
	u32 zone_size;

	/* backlight matrix intermediate buffer */
	u8 *back_buffer;
	u8 *brightness_buffer;
	u8 *bro_profile_buffer;

	/* flag to control read/write to the temp buffer */
	atomic_t buffer_dirty;

	/* flag to control debug mode */
	bool debug_blu;
	/* frames dropped when SPI write fails */
	u16 dropped_frames;
	/* flag to toggle backlight rolloff */
	bool rolloff_en;
	/* flag to control cross-eye brightness adjustment */
	bool ceb_en;

	/* spi debug mesage */
	struct spi_message msg;
	struct completion rx_ready;
};

/* Blu irq handler, transfer the SPI commands according to blu state */
static irqreturn_t blu_isr(int isr, void *spi_dev);

/*
 * Init the backlight matrix back buffer, brightness buffer, and BRO buffer
 * with messages to be transferred in SPI to BLU devices.
 */
static void init_buffers(struct blu_device *blu);

/* Setup the sysnode for LED framework to control the backlight level */
static int blu_spi_panel_backlight_node_setup(struct blu_device *blu);
/* Update backlight level of the blu */
static void blu_spi_update_backlight(struct blu_device *blu, u32 bl_lvl);
/* Ops for backlight node */
static int blu_spi_backlight_device_update_status(struct backlight_device *bd);
static int blu_spi_backlight_device_get_brightness(struct backlight_device *bd);
static const struct backlight_ops blu_spi_backlight_device_ops = {
	.update_status = blu_spi_backlight_device_update_status,
	.get_brightness = blu_spi_backlight_device_get_brightness,
};

static int blu_spi_backlight_device_get_brightness(struct backlight_device *bd)
{
	return 0;
}

static int blu_spi_backlight_device_update_status(struct backlight_device *bd)
{
	int brightness;
	struct blu_device *blu = (struct blu_device *)dev_get_drvdata(&bd->dev);

	brightness = bd->props.brightness;

	if ((bd->props.power != FB_BLANK_UNBLANK) ||
			(bd->props.state & BL_CORE_FBBLANK) ||
			(bd->props.state & BL_CORE_SUSPENDED))
		brightness = 0;

	blu_spi_update_backlight(blu, brightness);

	return 0;
}

static int blu_spi_panel_backlight_node_setup(struct blu_device *blu)
{
	char bl_node_name[BL_NODE_NAME_SIZE];
	struct backlight_properties props;

	snprintf(bl_node_name, BL_NODE_NAME_SIZE,
			"panel-%s", blu->name);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = blu->max_brightness;
	props.power = FB_BLANK_UNBLANK;

	blu->bl_device = devm_backlight_device_register(
			&blu->spi->dev, bl_node_name, &blu->spi->dev, blu,
			&blu_spi_backlight_device_ops, &props);

	if (IS_ERR_OR_NULL(blu->bl_device)) {
		pr_err("%s: Failed to register backlight: %ld\n",
				    __func__, PTR_ERR(blu->bl_device));
		return -ENODEV;
	}

	return 0;
}

static void blu_spi_update_backlight(struct blu_device *blu, u32 bl_lvl)
{
	if (blu->backlight_level != bl_lvl) {
		if (blu->backlight_level == 0 && bl_lvl > 0)
			enable_irq(blu->irq);
		else if (bl_lvl == 0) {
			disable_irq(blu->irq);
			blu->frame_counts = 0;
		}

		blu->backlight_level = bl_lvl;
	}
}

static void init_buffers(struct blu_device *blu)
{
	blu->back_buffer = devm_kzalloc(&blu->spi->dev, blu->matrix_size, GFP_KERNEL | GFP_DMA);
	blu->brightness_buffer = devm_kzalloc(&blu->spi->dev, blu->matrix_size, GFP_KERNEL | GFP_DMA);
	blu->bro_profile_buffer = devm_kzalloc(&blu->spi->dev, blu->rolloff_size, GFP_KERNEL | GFP_DMA);

	/* ensure the correct SPI values are also in the intermediate buffers */
	memcpy(blu->back_buffer, blu->backlight_matrix, blu->matrix_size);
	memcpy(blu->brightness_buffer, blu->backlight_matrix, blu->matrix_size);
	memcpy(blu->bro_profile_buffer, blu->bro_profile_default, blu->rolloff_size);
}

static void swap_buffers(u8 **buffer1, u8 **buffer2)
{
	u8 *temp;

	temp = *buffer1;
	*buffer1 = *buffer2;
	*buffer2 = temp;
}

static void apply_edge_masking(struct blu_device *blu)
{
	int i, j;

	for (i = 0; i < blu->zone_size; i++) {
		j = blu->corner_zone_indices[i];

		blu->backlight_matrix[MATRIX_START_INDEX + 2 * j] = 0;
		blu->backlight_matrix[MATRIX_START_INDEX + 2 * j + 1] = 0;
	}
}

static void apply_blu_brightness(struct blu_device *blu)
{
	int i;
	u32 scaled_brightness = 0;
	u32 brightness = 0;
	u8 checksum = 0;
	int backlight_level = blu->ceb_en ?
			(blu->backlight_level * blu->brightness_scaler / BRIGHTNESS_SCALE_CONVERT) :
			blu->backlight_level;

	// Scale the payload with the appropriate brightness value
	for (i = MATRIX_START_INDEX; i < (blu->matrix_size - 1); i = i + 2)	{
		brightness = (blu->backlight_matrix[i] << 8) | blu->backlight_matrix[i+1];
		scaled_brightness = (brightness * backlight_level / 255);
		blu->brightness_buffer[i] = (scaled_brightness >> 8) & 0xFF;
		blu->brightness_buffer[i+1] = scaled_brightness & 0xFF;
		if (i == MATRIX_START_INDEX)
			checksum = blu->brightness_buffer[i] ^ blu->brightness_buffer[i+1];
		else
			checksum ^= blu->brightness_buffer[i] ^ blu->brightness_buffer[i+1];
	}
	blu->brightness_buffer[blu->matrix_size-1] = checksum;
}

static irqreturn_t blu_isr(int isr, void *blu_dev)
{
	/* SPI commands to enter/exit soft reset mode*/
	static const u8 blu_enter_reset_cmd[] = { 0x01, 0x00, 0x01, 0x00, 0x00, 0x00 };
	static const u8 blu_exit_reset_cmd[] = { 0x01, 0x00, 0x01, 0x00, 0x00, 0x01 };
	static const u8 blu_led_timing[] = { 0x01, 0x00, 0x05, 0x00, 0x02, 0x23, 0x50, 0x7F, 0xFE, 0x11 };
	static const u8 blu_led_current[] = { 0x01, 0x00, 0x02, 0x00, 0x08, 0x06, 0xA0 };

	/* SPI check error flag is bit 0 of register 0x5BE */
	static const u8 tx_buf[6] = { 0x01, 0x00, 0x01, 0x85, 0xBE, 0x00 };
	static u8 rx_buf[] = { 0xFF };
	int err_flag = 0;

	/* Global LED current setting command */
	struct blu_device *blu = (struct blu_device *)blu_dev;
	struct spi_device *spi = blu->spi;
	int ret = 0;
	int old;

	/*
	 * BLU init requirement of hardware:
	 * frame [0]: enter soft reset mode, set up global LED current and
	 * set backlight rolloff compentation values.
	 * frame [1, STABLE_FRAME_COUNTS1): wait of PLL signal to be stable.
	 * frame [STABLE_FRAME_COUNTS]: exit soft reset mode.
	 * frame (STABLE_FRAME_COUNTS, ): input backlight matrix.
	 **/
	if (blu->frame_counts == 0) {
		ret = spi_write(spi, blu_enter_reset_cmd, sizeof(blu_enter_reset_cmd));
		if (ret)
			dev_err(&spi->dev, "failed to enter soft reset mode, error %d\n", ret);
		usleep_range(MIN_SPI_DELAY_US, MAX_SPI_DELAY_US);
		ret = spi_write(spi, blu_led_timing, sizeof(blu_led_timing));
		if (ret)
			dev_err(&spi->dev, "failed to set LED timings, error %d\n", ret);

		usleep_range(MIN_SPI_DELAY_US, MAX_SPI_DELAY_US);
		ret = spi_write(spi, blu_led_current, sizeof(blu_led_current));
		if (ret)
			dev_err(&spi->dev, "failed to set LED current, error %d\n", ret);

		usleep_range(MIN_SPI_DELAY_US, MAX_SPI_DELAY_US);
		if (blu->rolloff_size != 0 && blu->rolloff_en) {
			memcpy(blu->bro_profile_buffer, bro_profiles[SELECTED_BRO_PROFILE], blu->rolloff_size);
			ret = spi_write(spi, blu->bro_profile_buffer, blu->rolloff_size);
		} else {
			/* send in default no-op rolloff values */
			ret = spi_write(spi, blu->bro_profile_default, blu->rolloff_size);
		}
		if (ret)
			dev_err(&spi->dev, "failed to set rolloff values, error %d\n", ret);

		usleep_range(MIN_SPI_DELAY_US, MAX_SPI_DELAY_US);
		ret = spi_write(spi, blu_exit_reset_cmd, sizeof(blu_exit_reset_cmd));
		if (ret)
			dev_err(&spi->dev, "failed to exit soft reset mode, error %d\n", ret);

	} else if (blu->frame_counts == STABLE_FRAME_COUNTS) {
		ret = spi_write(spi, blu_exit_reset_cmd, sizeof(blu_exit_reset_cmd));
		if (ret)
			dev_err(&spi->dev, "failed to exit soft reset mode, error %d\n", ret);
	} else {
		old = atomic_read(&blu->buffer_dirty);

		/* if the back buffer is dirty, get the new matrix */
		if (old) {
			swap_buffers(&(blu->backlight_matrix), &(blu->back_buffer));
			apply_edge_masking(blu);
		}

		/* Apply brightness correction assuming all local dimming matrix values are at max brightness (255) */
		apply_blu_brightness(blu);

		/* send the backlight matrix */
		ret = spi_write(spi, &(blu->brightness_buffer[0]), blu->matrix_size);
		if (ret) {
			dev_err(&spi->dev, "failed to send backlight matrix, error %d\n", ret);
			/* add dropped frame for failed spi write */
			blu->dropped_frames++;
		}

		if (blu->debug_blu) {
			usleep_range(MIN_SPI_DELAY_US, MAX_SPI_DELAY_US);

			/* verify the SPI transfer */
			ret = spi_write_then_read(spi, tx_buf, sizeof(tx_buf), rx_buf, sizeof(rx_buf));
			if (ret)
				dev_err(&spi->dev, "failed to read register 0x5BE, error %d\n", ret);

			/* check error flag at bit 0 */
			err_flag = (rx_buf[0] & 1);
			if (err_flag) {
				dev_dbg(&spi->dev, "SPI transfer of the backlight matrix was not successful\n");
				/* add dropped frame for incorrect spi write */
				blu->dropped_frames++;
			}
		}

		atomic_set(&blu->buffer_dirty, 0);
	}

	++blu->frame_counts;

	return IRQ_HANDLED;
}

static int blu_spi_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	return 0;
}

static int blu_spi_release(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);

	return 0;
}

static void transfer_backlight_matrix(struct blu_device *blu)
{
	int old = atomic_read(&blu->buffer_dirty);

	/* if the intermediate buffer is still dirty, don't update it */
	if (old) {
		dev_dbg(&blu->spi->dev, "Cannot update the backlight matrix, temp buffer values are still dirty\n");
		return;
	}

	/* set the buffer values to dirty */
	atomic_set(&blu->buffer_dirty, 1);
}

static void do_transfer_backlight_matrix(struct work_struct *work)
{
	struct blu_device *blu = container_of(work, struct blu_device, work_blm);

	if (!blu)
		return;

	transfer_backlight_matrix(blu);
}

static int queue_transfer_backlight_matrix(struct blu_device *blu,
		struct blu_spi_backlight_matrix __user *blm)
{
	int ret;
	struct blu_spi_backlight_matrix blu_matrix;

	/* first get the struct from user space */
	ret = copy_from_user(&blu_matrix, blm, sizeof(blu_matrix));
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy %d bytes from backlight matrix struct\n", ret);
		return -EFAULT;
	}

	if (blu_matrix.matrix_size != (blu->matrix_size - MATRIX_START_INDEX)) {
		dev_err(&blu->spi->dev, "Invalid backlight matrix size\n");
		return -EINVAL;
	}

	/* get the matrix from the struct and place in intermediate buffer */
	ret = copy_from_user(&blu->back_buffer[MATRIX_START_INDEX],
			blu_matrix.backlight_matrix, blu_matrix.matrix_size);
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy %d bytes from backlight matrix\n", ret);
		return -EFAULT;
	}

	queue_work(blu->work_queue, &blu->work_blm);

	return ret;
}

static void send_spi_message(struct blu_device *blu)
{
	int ret;

	/* disable to stop sending backlight matrices through SPI */
	disable_irq(blu->irq);
	/* wait to make sure the IRQ is fully disbled */
	mdelay(IRQ_DISABLE_WAIT);

	ret = spi_sync(blu->spi, &blu->msg);
	if (ret)
		dev_err(&blu->spi->dev, "Error sending spi debug message %d\n", ret);

	complete(&blu->rx_ready);

	enable_irq(blu->irq);
}

static void do_send_spi_message(struct work_struct *work)
{
	struct blu_device *blu = container_of(work, struct blu_device, work_msg);

	if (!blu)
		return;

	send_spi_message(blu);
}

static int queue_send_spi_message(struct blu_device *blu,
		struct blu_spi_debug_message __user *msg)
{
	int ret;
	uint8_t tx[MAX_TRANSACTION_LENGTH] = { 0 };
	uint8_t rx[MAX_TRANSACTION_LENGTH] = { 0 };
	struct blu_spi_debug_message dbg_msg;
	struct spi_transfer *xfer;

	if (!blu->rx_ready.done) {
		dev_dbg(&blu->spi->dev, "Previous SPI message has not finished, returning\n");
		return -ENOMEM;
	}

	xfer = kzalloc(sizeof(*xfer), GFP_KERNEL | GFP_DMA);

	reinit_completion(&blu->rx_ready);

	/* copy the struct from user space */
	ret = copy_from_user(&dbg_msg, msg, sizeof(dbg_msg));
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy %d bytes from spi debug message stuct\n", ret);
		kfree(xfer);
		return -EFAULT;
	}

	if (dbg_msg.len > MAX_TRANSACTION_LENGTH) {
		dev_err(&blu->spi->dev, "Invalid spi transaction size\n");
		kfree(xfer);
		return -EINVAL;
	}
	xfer->len = dbg_msg.len;

	/* get the tx buffer from user space */
	ret = copy_from_user(&tx, dbg_msg.tx_buf, xfer->len);
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy %d bytes from tx buffer\n", ret);
		kfree(xfer);
		return -EFAULT;
	}

	xfer->tx_buf = tx;
	xfer->rx_buf = rx;
	xfer->bits_per_word = BLU_SPI_BITS_PER_WORD;

	spi_message_init(&blu->msg);
	spi_message_add_tail(xfer, &blu->msg);

	/* queue sending the SPI message and wait to make sure it is sent */
	queue_work(blu->work_queue, &blu->work_msg);
	wait_for_completion(&blu->rx_ready);

	/* convert the rx buffer into a uint64_t to match the struct */
	memcpy(&dbg_msg.rx_buf, xfer->rx_buf, sizeof(dbg_msg.rx_buf));
	kfree(xfer);

	/* copy the rx buffer back to user space */
	ret = copy_to_user(msg, &dbg_msg, sizeof(dbg_msg));
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy %d bytes to rx_buf\n", ret);
		return -EFAULT;
	}

	return ret;
}

static int set_brightness_calibration(struct blu_device *blu,
		int __user *scaler)
{
	int ret, scale_val;

	ret = copy_from_user(&scale_val, scaler, sizeof(scaler));
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy brightness calibration value\n");
		return -EFAULT;
	}

	blu->brightness_scaler = scale_val;

	return ret;
}

static int get_backlight_matrix(struct blu_device *blu,
		struct blu_spi_backlight_matrix __user *blm_dump)
{
	int ret;
	uint8_t *matrix_addr;

	/* copy the backlight matrix address we want to write to from user space */
	ret = copy_from_user(&matrix_addr, blm_dump, sizeof(blu->backlight_matrix));
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy %d bytes from user space\n", ret);
		return -EFAULT;
	}

	/* copy the current backlight matrix back to user space */
	ret = copy_to_user(matrix_addr, &(blu->backlight_matrix[0]), blu->matrix_size);
	if (ret) {
		dev_err(&blu->spi->dev, "Failed to copy %d bytes to user space\n", ret);
		return -EFAULT;
	}

	return ret;
}

static long blu_spi_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct blu_device *blu = container_of(file->private_data,
		struct blu_device, misc);

	switch (cmd) {
	case BLU_SPI_SET_BACKLIGHT_IOCTL:
		return queue_transfer_backlight_matrix(blu,
			(struct blu_spi_backlight_matrix *)arg);
	case BLU_SPI_DEBUG_MSG_IOCTL:
		return queue_send_spi_message(blu,
			(struct blu_spi_debug_message *)arg);
	case BLU_SPI_SET_CALIBRATION:
		return set_brightness_calibration(blu, (int *)arg);
	case BLU_SPI_GET_BACKLIGHT_IOCTL:
		return get_backlight_matrix(blu,
			(struct blu_spi_backlight_matrix *)arg);
	default:
		dev_err(&blu->spi->dev, "Unrecognized IOCTL %ul\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static ssize_t cross_eye_brightness_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct blu_device *blu_dev =
		container_of(miscdev, struct blu_device, misc);

	return snprintf(buf, PAGE_SIZE, "%d\n", blu_dev->ceb_en);
}

static ssize_t cross_eye_brightness_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct blu_device *blu_dev =
		container_of(miscdev, struct blu_device, misc);
	int ret;
	bool temp;

	ret = kstrtobool(buf, &temp);
	if (ret < 0) {
		dev_err(dev, "Illegal input for cross_eye_brightness_en: %s", buf);
		return ret;
	}

	blu_dev->ceb_en = temp;
	return count;
}
static DEVICE_ATTR_RW(cross_eye_brightness_en);

static ssize_t debug_blu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct blu_device *blu_dev =
		container_of(miscdev, struct blu_device, misc);

	return snprintf(buf, PAGE_SIZE, "%d\n", blu_dev->debug_blu);
}

static ssize_t debug_blu_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct blu_device *blu_dev =
		container_of(miscdev, struct blu_device, misc);
	int ret;
	bool temp;

	ret = kstrtobool(buf, &temp);
	if (ret < 0) {
		dev_err(dev, "Illegal input for debug_blu: %s", buf);
		return ret;
	}

	blu_dev->debug_blu = temp;
	return count;
}
static DEVICE_ATTR_RW(debug_blu);

static ssize_t dropped_frames_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct blu_device *blu_dev =
		container_of(miscdev, struct blu_device, misc);

	return snprintf(buf, PAGE_SIZE, "%d\n", blu_dev->dropped_frames);
}
static DEVICE_ATTR_RO(dropped_frames);

static ssize_t rolloff_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct blu_device *blu_dev =
		container_of(miscdev, struct blu_device, misc);

	return snprintf(buf, PAGE_SIZE, "%d\n", blu_dev->rolloff_en);
}

static ssize_t rolloff_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct blu_device *blu_dev =
		container_of(miscdev, struct blu_device, misc);
	int ret;
	bool temp;

	ret = kstrtobool(buf, &temp);
	if (ret < 0) {
		dev_err(dev, "Illegal input for rolloff_en: %s", buf);
		return ret;
	}

	blu_dev->rolloff_en = temp;
	return count;
}
static DEVICE_ATTR_RW(rolloff_en);

static struct attribute *blu_spi_attrs[] = {
	&dev_attr_cross_eye_brightness_en.attr,
	&dev_attr_debug_blu.attr,
	&dev_attr_dropped_frames.attr,
	&dev_attr_rolloff_en.attr,
	NULL,
};

static const struct attribute_group blu_spi_group = {
	.attrs = blu_spi_attrs,
};
__ATTRIBUTE_GROUPS(blu_spi);

static const struct file_operations blu_spi_fops = {
	.owner = THIS_MODULE,
	.read = NULL,
	.write = NULL,
	.open = blu_spi_open,
	.release = blu_spi_release,
	.unlocked_ioctl = blu_spi_ioctl
};

static int blu_spi_probe(struct spi_device *spi)
{
	struct blu_device *blu;
	int vsync_gpio;
	int ret = 0;
	char device_name[8];

	spi->mode = BLU_SPI_MODE;
	spi->bits_per_word = BLU_SPI_BITS_PER_WORD;

	blu = devm_kzalloc(&spi->dev, sizeof(struct blu_device), GFP_KERNEL);
	if (!blu)
		return -ENOMEM;

	blu->spi = spi;

	atomic_set(&blu->buffer_dirty, 0);

	ret = of_property_read_string(spi->dev.of_node, "oculus,blu-name", &blu->name);
	if (ret) {
		dev_err(&spi->dev, "%s: could not find oculus,blu-name property, ret=%d\n", __func__, ret);
		return ret;
	}
	dev_dbg(&spi->dev, "%s: initing blu %s\n", __func__, blu->name);

	if (of_property_read_u32(spi->dev.of_node, "oculus,blu-max-brightness", &blu->max_brightness))
		blu->max_brightness = 255;

	ret = blu_spi_panel_backlight_node_setup(blu);
	if (ret) {
		dev_err(&spi->dev, "%s: failed to setup backlight node, ret=%d\n", __func__, ret);
		return ret;
	}

	vsync_gpio = of_get_named_gpio(spi->dev.of_node, "oculus,blu-irq-gpio", 0);
	if (!gpio_is_valid(vsync_gpio)) {
		dev_err(&spi->dev, "%s: blu-irq-gpio %d is invalid\n", __func__, vsync_gpio);
		return -EINVAL;
	}

	ret = devm_gpio_request(&spi->dev, vsync_gpio, blu->name);
	if (ret) {
		dev_err(&spi->dev, "%s: failed to requeset gpio %d\n", __func__, vsync_gpio);
		return ret;
	}

	blu->irq = gpio_to_irq(vsync_gpio);
	if (blu->irq < 0) {
		dev_err(&spi->dev, "%s: failed to transfer gpio %d to irq\n",
				__func__, vsync_gpio);
		return ret;
	}

	dev_set_drvdata(&spi->dev, blu);

	blu->matrix_size = of_property_count_u8_elems(spi->dev.of_node, "oculus,blu-init-matrix");
	if (blu->matrix_size < 0) {
		ret = blu->matrix_size;
		dev_err(&spi->dev, "%s: could not get oculus,blu-init-matrix size, ret=%d\n",
			__func__, ret);
		return ret;
	}

	blu->backlight_matrix = devm_kzalloc(&blu->spi->dev, blu->matrix_size, GFP_KERNEL | GFP_DMA);

	ret = of_property_read_u8_array(spi->dev.of_node, "oculus,blu-init-matrix",
		blu->backlight_matrix, blu->matrix_size);
	if (ret) {
		dev_err(&spi->dev, "%s: could not find oculus,blu-init-matrix property, ret=%d\n",
			__func__, ret);
		return ret;
	}

	blu->rolloff_size = of_property_count_u8_elems(spi->dev.of_node, "oculus,blu-rolloff-comp");
	if (blu->rolloff_size < 0) {
		ret = blu->rolloff_size;
		dev_err(&spi->dev, "%s: could not get oculus,blu-rolloff-comp size, ret=%d\n",
			__func__, ret);
		/* default to 0 if the entry could not be found */
		blu->rolloff_size = 0;
	}

	blu->bro_profile_default = devm_kzalloc(&blu->spi->dev, blu->rolloff_size,
		GFP_KERNEL | GFP_DMA);

	ret = of_property_read_u8_array(spi->dev.of_node, "oculus,blu-rolloff-comp",
		blu->bro_profile_default, blu->rolloff_size);
	if (ret) {
		dev_err(&spi->dev, "%s: could not find oculus,blu-rolloff-comp property, ret=%d\n",
			__func__, ret);
		return ret;
	}

	blu->zone_size = of_property_count_u32_elems(spi->dev.of_node, "oculus,blu-corner-zones");
	if (blu->zone_size < 0) {
		ret = blu->zone_size;
		dev_err(&spi->dev, "%s: could not get oculus,blu-corner-zones size, ret=%d\n",
			__func__, ret);
		return ret;
	}

	blu->corner_zone_indices = devm_kzalloc(&blu->spi->dev, (blu->zone_size * sizeof(u32)),
		GFP_KERNEL | GFP_DMA);

	ret = of_property_read_u32_array(spi->dev.of_node, "oculus,blu-corner-zones",
		blu->corner_zone_indices, blu->zone_size);
	if (ret) {
		dev_err(&spi->dev, "%s: could not find oculus,blu-corner-zones property, ret=%d\n",
			__func__, ret);
		return ret;
	}

	if (of_property_read_bool(spi->dev.of_node, "oculus,continuous-splash"))
		blu->frame_counts = STABLE_FRAME_COUNTS + 1;
	else
		blu->frame_counts = 0;

	blu->backlight_level = blu->max_brightness;
	blu->brightness_scaler = 1 * BRIGHTNESS_SCALE_CONVERT;

	/* enable cross eye brightness by default */
	blu->ceb_en = 1;

	init_buffers(blu);

	ret = devm_request_threaded_irq(&spi->dev,
			blu->irq, NULL, blu_isr,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			blu->name, blu);

	if (ret) {
		dev_err(&blu->spi->dev, "%s: failed to request wake IRQ of %s, ret %d\n",
				__func__, blu->name, ret);
		return ret;
	}

	snprintf(device_name, sizeof(device_name), "blu%d", number_of_devices++);

	/* misc device info */
	blu->misc.name = device_name;
	blu->misc.minor = MISC_DYNAMIC_MINOR;
	blu->misc.groups = blu_spi_groups;
	blu->misc.fops = &blu_spi_fops;

	/* register the misc device */
	ret = misc_register(&blu->misc);
	if (ret < 0) {
		dev_err(&spi->dev, "%s fails to register misc device, error %d",
			__func__, ret);
		return ret;
	}

	blu->work_queue = create_singlethread_workqueue("blu_work_queue");
	if (!blu->work_queue) {
		dev_err(&spi->dev, "%s: could not create work queue\n", __func__);
		return -ENOMEM;
	}

	init_completion(&blu->rx_ready);
	INIT_WORK(&blu->work_blm, do_transfer_backlight_matrix);
	INIT_WORK(&blu->work_msg, do_send_spi_message);

	return ret;
}

static int blu_spi_remove(struct spi_device *spi)
{
	struct blu_device *blu = (struct blu_device *)dev_get_drvdata(&spi->dev);

	if (blu->irq > 0)
		disable_irq(blu->irq);

	flush_workqueue(blu->work_queue);
	destroy_workqueue(blu->work_queue);

	/* unregister the misc device */
	misc_deregister(&blu->misc);

	return 0;
}

static const struct of_device_id blu_spi_of_match_table[] = {
	{.compatible = BLU_SPI_NODE_TABLE,},
	{},
};
MODULE_DEVICE_TABLE(of, blu_spi_of_match_table);

static struct spi_driver blu_spi_driver = {
	.driver = {
		.name = "seacliff-blu-spi",
		.of_match_table = blu_spi_of_match_table,
	},
	.probe = blu_spi_probe,
	.remove = blu_spi_remove,
};
module_spi_driver(blu_spi_driver);

MODULE_DESCRIPTION("Seacliff panels BLU SPI driver");
MODULE_LICENSE("GPL v2");
