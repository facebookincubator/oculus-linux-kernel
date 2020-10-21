// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/component.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/hdmi.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_crtc_helper.h>
#include <linux/string.h>

#define CFG_HPD_INTERRUPTS BIT(0)
#define CFG_EDID_INTERRUPTS BIT(1)
#define CFG_CEC_INTERRUPTS BIT(2)
#define CFG_VID_CHK_INTERRUPTS BIT(3)

#define EDID_SEG_SIZE 256
#define READ_BUF_MAX_SIZE 64
#define WRITE_BUF_MAX_SIZE 64
#define HPD_UEVENT_BUFFER_SIZE 30
#define VERSION_NUM	0x32

struct lt9611_reg_cfg {
	u8 reg;
	u8 val;
};

enum lt9611_fw_upgrade_status {
	UPDATE_SUCCESS = 0,
	UPDATE_RUNNING = 1,
	UPDATE_FAILED = 2,
};

struct lt9611_vreg {
	struct regulator *vreg; /* vreg handle */
	char vreg_name[32];
	int min_voltage;
	int max_voltage;
	int enable_load;
	int disable_load;
	int pre_on_sleep;
	int post_on_sleep;
	int pre_off_sleep;
	int post_off_sleep;
};

struct lt9611_video_cfg {
	u32 h_active;
	u32 h_front_porch;
	u32 h_pulse_width;
	u32 h_back_porch;
	bool h_polarity;
	u32 v_active;
	u32 v_front_porch;
	u32 v_pulse_width;
	u32 v_back_porch;
	bool v_polarity;
	u32 pclk_khz;
	bool interlaced;
	u32 vic;
	enum hdmi_picture_aspect ar;
	u32 num_of_lanes;
	u32 num_of_intfs;
	u8 scaninfo;
};

struct lt9611 {
	struct device *dev;
	struct drm_bridge bridge;

	struct device_node *host_node;
	struct mipi_dsi_device *dsi;
	struct drm_connector connector;

	u8 i2c_addr;
	int irq;
	bool ac_mode;

	u32 irq_gpio;
	u32 reset_gpio;
	u32 hdmi_ps_gpio;
	u32 hdmi_en_gpio;

	unsigned int num_vreg;
	struct lt9611_vreg *vreg_config;

	struct i2c_client *i2c_client;

	enum drm_connector_status status;
	bool power_on;

	/* get display modes from device tree */
	bool non_pluggable;
	u32 num_of_modes;
	struct list_head mode_list;

	struct drm_display_mode curr_mode;
	struct lt9611_video_cfg video_cfg;

	u8 edid_buf[EDID_SEG_SIZE];
	u8 i2c_wbuf[WRITE_BUF_MAX_SIZE];
	u8 i2c_rbuf[READ_BUF_MAX_SIZE];
	bool hdmi_mode;
	enum lt9611_fw_upgrade_status fw_status;
};

struct lt9611_timing_info {
	u16 xres;
	u16 yres;
	u8 bpp;
	u8 fps;
	u8 lanes;
	u8 intfs;
};

static struct lt9611_timing_info lt9611_supp_timing_cfg[] = {
	{3840, 2160, 24, 30, 4, 2}, /* 3840x2160 24bit 30Hz 4Lane 2ports */
	{1920, 1080, 24, 60, 4, 1}, /* 1080P 24bit 60Hz 4lane 1port */
	{1920, 1080, 24, 30, 3, 1}, /* 1080P 24bit 30Hz 3lane 1port */
	{1920, 1080, 24, 24, 3, 1},
	{720, 480, 24, 60, 2, 1},
	{720, 576, 24, 50, 2, 1},
	{640, 480, 24, 60, 2, 1},
	{0xffff, 0xffff, 0xff, 0xff, 0xff},
};

static struct lt9611 *bridge_to_lt9611(struct drm_bridge *bridge)
{
	return container_of(bridge, struct lt9611, bridge);
}

static struct lt9611 *connector_to_lt9611(struct drm_connector *connector)
{
	return container_of(connector, struct lt9611, connector);
}

/*
 * Write one reg with more values;
 * Reg -> value0, value1, value2.
 */

static int lt9611_write(struct lt9611 *pdata, u8 reg,
		const u8 *buf, int size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = size + 1,
		.buf = pdata->i2c_wbuf,
	};

	pdata->i2c_wbuf[0] = reg;
	if (size > (WRITE_BUF_MAX_SIZE - 1)) {
		pr_err("invalid write buffer size %d\n", size);
		return -EINVAL;
	}

	memcpy(pdata->i2c_wbuf + 1, buf, size);

	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		pr_err("i2c write failed\n");
		return -EIO;
	}

	return 0;
}

/*
 * Write one reg with one value;
 * Reg -> value
 */
static int lt9611_write_byte(struct lt9611 *pdata, const u8 reg, u8 value)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = pdata->i2c_wbuf,
	};

	memset(pdata->i2c_wbuf, 0, WRITE_BUF_MAX_SIZE);
	pdata->i2c_wbuf[0] = reg;
	pdata->i2c_wbuf[1] = value;

	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		pr_err("i2c write failed\n");
		return -EIO;
	}

	return 0;
}

/*
 * Write more regs with more values;
 * Reg1 -> value1
 * Reg2 -> value2
 */
static void lt9611_write_array(struct lt9611 *pdata,
	struct lt9611_reg_cfg *reg_arry, int size)
{
	int i = 0;

	for (i = 0; i < size; i++)
		lt9611_write_byte(pdata, reg_arry[i].reg, reg_arry[i].val);
}

static int lt9611_read(struct lt9611 *pdata, u8 reg, char *buf, u32 size)
{
	struct i2c_client *client = pdata->i2c_client;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = pdata->i2c_wbuf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = pdata->i2c_rbuf,
		}
	};

	memset(pdata->i2c_wbuf, 0x0, WRITE_BUF_MAX_SIZE);
	memset(pdata->i2c_rbuf, 0x0, READ_BUF_MAX_SIZE);
	pdata->i2c_wbuf[0] = reg;

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		pr_err("i2c read failed\n");
		return -EIO;
	}

	memcpy(buf, pdata->i2c_rbuf, size);

	return 0;
}

void lt9611_config(struct lt9611 *pdata)
{
	struct lt9611_reg_cfg reg_cfg[] = {
		{0xFF, 0x80},
		{0xEE, 0x01},
		{0x5E, 0xDF},
		{0x58, 0x00},
		{0x59, 0x50},
		{0x5A, 0x10},
		{0x5A, 0x00},
	};

	lt9611_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

u8 lt9611_get_version(struct lt9611 *pdata)
{
	u8 revison = 0;

	lt9611_write_byte(pdata, 0xFF, 0x80);
	lt9611_write_byte(pdata, 0xEE, 0x01);
	lt9611_write_byte(pdata, 0xFF, 0xB0);

	if (!lt9611_read(pdata, 0x21, &revison, 1))
		pr_info("LT9611 revison: 0x%x\n", revison);
	else
		pr_err("LT9611 get revison failed\n");

	lt9611_write_byte(pdata, 0xFF, 0x80);
	lt9611_write_byte(pdata, 0xEE, 0x00);

	return revison;
}

void lt9611_flash_write_en(struct lt9611 *pdata)
{
	struct lt9611_reg_cfg reg_cfg0[] = {
		{0xFF, 0x81},
		{0x08, 0xBF},
	};

	struct lt9611_reg_cfg reg_cfg1[] = {
		{0xFF, 0x80},
		{0x5A, 0x04},
		{0x5A, 0x00},
	};

	lt9611_write_array(pdata, reg_cfg0, ARRAY_SIZE(reg_cfg0));
	msleep(20);
	lt9611_write_byte(pdata, 0x08, 0xFF);
	msleep(20);
	lt9611_write_array(pdata, reg_cfg1, ARRAY_SIZE(reg_cfg1));
}

void lt9611_block_erase(struct lt9611 *pdata)
{
	struct lt9611_reg_cfg reg_cfg[] = {
		{0xFF, 0x80},
		{0xEE, 0x01},
		{0x5A, 0x04},
		{0x5A, 0x00},
		{0x5B, 0x00},
		{0x5C, 0x00},
		{0x5D, 0x00},
		{0x5A, 0x01},
		{0x5A, 0x00},
	};

	pr_info("LT9611 block erase\n");
	lt9611_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
	msleep(3000);
}

void lt9611_flash_read_addr_set(struct lt9611 *pdata, u32 addr)
{
	struct lt9611_reg_cfg reg_cfg[] = {
		{0x5E, 0x5F},
		{0x5A, 0xA0},
		{0x5A, 0x80},
		{0x5B, (addr & 0xFF0000) >> 16},
		{0x5C, (addr & 0xFF00) >> 8},
		{0x5D, addr & 0xFF},
		{0x5A, 0x90},
		{0x5A, 0x80},
		{0x58, 0x21},
	};

	lt9611_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

void lt9611_fw_read_back(struct lt9611 *pdata, u8 *buff, int size)
{
	u8 page_data[32];
	int page_number = 0, i = 0, addr = 0;

	struct lt9611_reg_cfg reg_cfg[] = {
		{0xFF, 0x80},
		{0xEE, 0x01},
		{0x5A, 0x84},
		{0x5A, 0x80},
	};
	/*
	 * Read 32 bytes once.
	 */
	page_number = size / 32;
	if (size % 32)
		page_number++;

	lt9611_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));

	for (i = 0; i < page_number; i++) {
		memset(page_data, 0x0, 32);
		lt9611_flash_read_addr_set(pdata, addr);
		lt9611_read(pdata, 0x5F, page_data, 32);
		memcpy(buff, page_data, 32);
		buff += 32;
		addr += 32;
	}
}

void lt9611_flash_write_config(struct lt9611 *pdata)
{
	struct lt9611_reg_cfg reg_cfg[] = {
		{0xFF, 0x80},
		{0x5E, 0xDF},
		{0x5A, 0x20},
		{0x5A, 0x00},
		{0x58, 0x21},
	};

	lt9611_flash_write_en(pdata);
	lt9611_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

void lt9611_flash_write_addr_set(struct lt9611 *pdata, u32 addr)
{
	struct lt9611_reg_cfg reg_cfg[] = {
		{0x5B, (addr & 0xFF0000) >> 16},
		{0x5C, (addr & 0xFF00) >> 8},
		{0x5D, addr & 0xFF},
		{0x5A, 0x10},
		{0x5A, 0x00},
	};

	lt9611_write_array(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));
}

void lt9611_firmware_write(struct lt9611 *pdata, const u8 *f_data,
		int size)
{
	u8 last_buf[32];
	int i = 0, page_size = 32;
	int start_addr = 0, total_page = 0, rest_data = 0;

	total_page = size / page_size;
	rest_data = size % page_size;

	for (i = 0; i < total_page; i++) {
		lt9611_flash_write_config(pdata);
		lt9611_write(pdata, 0x59, f_data, page_size);
		lt9611_flash_write_addr_set(pdata, start_addr);
		start_addr += page_size;
		f_data += page_size;
		msleep(20);
	}

	if (rest_data > 0) {
		memset(last_buf, 0xFF, 32);
		memcpy(last_buf, f_data, rest_data);
		lt9611_flash_write_config(pdata);
		lt9611_write(pdata, 0x59, last_buf, page_size);

		lt9611_flash_write_addr_set(pdata, start_addr);
		msleep(20);
	}
	msleep(20);

	pr_info("LT9611 FW write over, total size: %d, page: %d, reset: %d\n",
		size, total_page, rest_data);
}

void lt9611_firmware_upgrade(struct lt9611 *pdata,
			const struct firmware *cfg)
{
	int i = 0;
	u8 *fw_read_data = NULL;
	int data_len = (int)cfg->size;

	pr_info("LT9611 FW total size %d\n", data_len);

	fw_read_data = kzalloc(ALIGN(data_len, 32), GFP_KERNEL);
	if (!fw_read_data)
		return;

	pdata->fw_status = UPDATE_RUNNING;
	lt9611_config(pdata);

	/*
	 * Need erase block 2 timess here.
	 * Sometimes, erase can fail.
	 * This is a workaroud.
	 */
	for (i = 0; i < 2; i++)
		lt9611_block_erase(pdata);

	lt9611_firmware_write(pdata, cfg->data, data_len);
	msleep(20);
	lt9611_fw_read_back(pdata, fw_read_data, data_len);

	if (!memcmp(cfg->data, fw_read_data, data_len)) {
		pdata->fw_status = UPDATE_SUCCESS;
		pr_debug("LT9611 Firmware upgrade success.\n");
	} else {
		pdata->fw_status = UPDATE_FAILED;
		pr_err("LT9611 Firmware upgrade failed\n");
	}

	kfree(fw_read_data);
}

static void lt9611_firmware_cb(const struct firmware *cfg, void *data)
{
	struct lt9611 *pdata = (struct lt9611 *)data;

	if (!cfg) {
		pr_err("LT9611 get firmware failed\n");
		return;
	}

	lt9611_firmware_upgrade(pdata, cfg);
	release_firmware(cfg);
}

static int lt9611_parse_dt_modes(struct device_node *np,
					struct list_head *head,
					u32 *num_of_modes)
{
	int rc = 0;
	struct drm_display_mode *mode;
	u32 mode_count = 0;
	struct device_node *node = NULL;
	struct device_node *root_node = NULL;
	u32 h_front_porch, h_pulse_width, h_back_porch;
	u32 v_front_porch, v_pulse_width, v_back_porch;
	bool h_active_high, v_active_high;
	u32 flags = 0;

	root_node = of_get_child_by_name(np, "lt,customize-modes");
	if (!root_node) {
		root_node = of_parse_phandle(np, "lt,customize-modes", 0);
		if (!root_node) {
			pr_info("No entry present for lt,customize-modes\n");
			goto end;
		}
	}

	for_each_child_of_node(root_node, node) {
		rc = 0;
		mode = kzalloc(sizeof(*mode), GFP_KERNEL);
		if (!mode) {
			pr_err("Out of memory\n");
			rc =  -ENOMEM;
			continue;
		}

		rc = of_property_read_u32(node, "lt,mode-h-active",
						&mode->hdisplay);
		if (rc) {
			pr_err("failed to read h-active, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-h-front-porch",
						&h_front_porch);
		if (rc) {
			pr_err("failed to read h-front-porch, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-h-pulse-width",
						&h_pulse_width);
		if (rc) {
			pr_err("failed to read h-pulse-width, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-h-back-porch",
						&h_back_porch);
		if (rc) {
			pr_err("failed to read h-back-porch, rc=%d\n", rc);
			goto fail;
		}

		h_active_high = of_property_read_bool(node,
						"lt,mode-h-active-high");

		rc = of_property_read_u32(node, "lt,mode-v-active",
						&mode->vdisplay);
		if (rc) {
			pr_err("failed to read v-active, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-v-front-porch",
						&v_front_porch);
		if (rc) {
			pr_err("failed to read v-front-porch, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-v-pulse-width",
						&v_pulse_width);
		if (rc) {
			pr_err("failed to read v-pulse-width, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-v-back-porch",
						&v_back_porch);
		if (rc) {
			pr_err("failed to read v-back-porch, rc=%d\n", rc);
			goto fail;
		}

		v_active_high = of_property_read_bool(node,
						"lt,mode-v-active-high");

		rc = of_property_read_u32(node, "lt,mode-refresh-rate",
						&mode->vrefresh);
		if (rc) {
			pr_err("failed to read refresh-rate, rc=%d\n", rc);
			goto fail;
		}

		rc = of_property_read_u32(node, "lt,mode-clock-in-khz",
						&mode->clock);
		if (rc) {
			pr_err("failed to read clock, rc=%d\n", rc);
			goto fail;
		}

		mode->hsync_start = mode->hdisplay + h_front_porch;
		mode->hsync_end = mode->hsync_start + h_pulse_width;
		mode->htotal = mode->hsync_end + h_back_porch;
		mode->vsync_start = mode->vdisplay + v_front_porch;
		mode->vsync_end = mode->vsync_start + v_pulse_width;
		mode->vtotal = mode->vsync_end + v_back_porch;
		if (h_active_high)
			flags |= DRM_MODE_FLAG_PHSYNC;
		else
			flags |= DRM_MODE_FLAG_NHSYNC;
		if (v_active_high)
			flags |= DRM_MODE_FLAG_PVSYNC;
		else
			flags |= DRM_MODE_FLAG_NVSYNC;
		mode->flags = flags;

		if (!rc) {
			mode_count++;
			list_add_tail(&mode->head, head);
		}

		drm_mode_set_name(mode);

		pr_debug("mode[%s] h[%d,%d,%d,%d] v[%d,%d,%d,%d] %d %x %dkHZ\n",
			mode->name, mode->hdisplay, mode->hsync_start,
			mode->hsync_end, mode->htotal, mode->vdisplay,
			mode->vsync_start, mode->vsync_end, mode->vtotal,
			mode->vrefresh, mode->flags, mode->clock);
fail:
		if (rc) {
			kfree(mode);
			continue;
		}
	}

	if (num_of_modes)
		*num_of_modes = mode_count;

end:
	return rc;
}


static int lt9611_parse_dt(struct device *dev,
	struct lt9611 *pdata)
{
	struct device_node *np = dev->of_node;
	struct device_node *end_node;
	int ret = 0;

	end_node = of_graph_get_endpoint_by_regs(dev->of_node, 0, 0);
	if (!end_node) {
		pr_err("remote endpoint not found\n");
		return -ENODEV;
	}

	pdata->host_node = of_graph_get_remote_port_parent(end_node);
	of_node_put(end_node);
	if (!pdata->host_node) {
		pr_err("remote node not found\n");
		return -ENODEV;
	}
	of_node_put(pdata->host_node);

	pdata->irq_gpio =
		of_get_named_gpio(np, "lt,irq-gpio", 0);
	if (!gpio_is_valid(pdata->irq_gpio)) {
		pr_err("irq gpio not specified\n");
		ret = -EINVAL;
	}
	pr_debug("irq_gpio=%d\n", pdata->irq_gpio);

	pdata->reset_gpio =
		of_get_named_gpio(np, "lt,reset-gpio", 0);
	if (!gpio_is_valid(pdata->reset_gpio)) {
		pr_err("reset gpio not specified\n");
		ret = -EINVAL;
	}
	pr_debug("reset_gpio=%d\n", pdata->reset_gpio);

	pdata->hdmi_ps_gpio =
		of_get_named_gpio(np, "lt,hdmi-ps-gpio", 0);
	if (!gpio_is_valid(pdata->hdmi_ps_gpio))
		pr_debug("hdmi ps gpio not specified\n");
	else
		pr_debug("hdmi_ps_gpio=%d\n", pdata->hdmi_ps_gpio);

	pdata->hdmi_en_gpio =
		of_get_named_gpio(np, "lt,hdmi-en-gpio", 0);
	if (!gpio_is_valid(pdata->hdmi_en_gpio))
		pr_debug("hdmi en gpio not specified\n");
	else
		pr_debug("hdmi_en_gpio=%d\n", pdata->hdmi_en_gpio);

	pdata->ac_mode = of_property_read_bool(np, "lt,ac-mode");
	pr_debug("ac_mode=%d\n", pdata->ac_mode);

	pdata->non_pluggable = of_property_read_bool(np, "lt,non-pluggable");
	pr_debug("non_pluggable = %d\n", pdata->non_pluggable);
	if (pdata->non_pluggable) {
		INIT_LIST_HEAD(&pdata->mode_list);
		ret = lt9611_parse_dt_modes(np,
			&pdata->mode_list, &pdata->num_of_modes);
	}

	return ret;
}

static int lt9611_gpio_configure(struct lt9611 *pdata, bool on)
{
	int ret = 0;

	if (on) {
		ret = gpio_request(pdata->reset_gpio,
			"lt9611-reset-gpio");
		if (ret) {
			pr_err("lt9611 reset gpio request failed\n");
			goto error;
		}

		ret = gpio_direction_output(pdata->reset_gpio, 0);
		if (ret) {
			pr_err("lt9611 reset gpio direction failed\n");
			goto reset_error;
		}

		if (gpio_is_valid(pdata->hdmi_en_gpio)) {
			ret = gpio_request(pdata->hdmi_en_gpio,
					"lt9611-hdmi-en-gpio");
			if (ret) {
				pr_err("lt9611 hdmi en gpio request failed\n");
				goto reset_error;
			}

			ret = gpio_direction_output(pdata->hdmi_en_gpio, 1);
			if (ret) {
				pr_err("lt9611 hdmi en gpio direction failed\n");
				goto hdmi_en_error;
			}
		}

		if (gpio_is_valid(pdata->hdmi_ps_gpio)) {
			ret = gpio_request(pdata->hdmi_ps_gpio,
				"lt9611-hdmi-ps-gpio");
			if (ret) {
				pr_err("lt9611 hdmi ps gpio request failed\n");
				goto hdmi_en_error;
			}

			ret = gpio_direction_input(pdata->hdmi_ps_gpio);
			if (ret) {
				pr_err("lt9611 hdmi ps gpio direction failed\n");
				goto hdmi_ps_error;
			}
		}

		ret = gpio_request(pdata->irq_gpio, "lt9611-irq-gpio");
		if (ret) {
			pr_err("lt9611 irq gpio request failed\n");
			goto hdmi_ps_error;
		}

		ret = gpio_direction_input(pdata->irq_gpio);
		if (ret) {
			pr_err("lt9611 irq gpio direction failed\n");
			goto irq_error;
		}
	} else {
		gpio_free(pdata->irq_gpio);
		if (gpio_is_valid(pdata->hdmi_ps_gpio))
			gpio_free(pdata->hdmi_ps_gpio);
		if (gpio_is_valid(pdata->hdmi_en_gpio))
			gpio_free(pdata->hdmi_en_gpio);
		gpio_free(pdata->reset_gpio);
	}

	return ret;


irq_error:
	gpio_free(pdata->irq_gpio);
hdmi_ps_error:
	if (gpio_is_valid(pdata->hdmi_ps_gpio))
		gpio_free(pdata->hdmi_ps_gpio);
hdmi_en_error:
	if (gpio_is_valid(pdata->hdmi_en_gpio))
		gpio_free(pdata->hdmi_en_gpio);
reset_error:
	gpio_free(pdata->reset_gpio);
error:
	return ret;
}

static int lt9611_read_device_id(struct lt9611 *pdata)
{
	u8 rev0 = 0, rev1 = 0;
	int ret = 0;

	lt9611_write_byte(pdata, 0xFF, 0x80);
	lt9611_write_byte(pdata, 0xEE, 0x01);
	lt9611_write_byte(pdata, 0xFF, 0x81);

	if (!lt9611_read(pdata, 0x00, &rev0, 1) &&
		!lt9611_read(pdata, 0x01, &rev1, 1)) {
		pr_info("LT9611 id: 0x%x\n", (rev0 << 8) | rev1);
	} else {
		pr_err("LT9611 get id failed\n");
		ret = -1;
	}

	lt9611_write_byte(pdata, 0xFF, 0x80);
	lt9611_write_byte(pdata, 0xEE, 0x00);

	return ret;
}

static irqreturn_t lt9611_irq_thread_handler(int irq, void *dev_id)
{
	pr_debug("irq_thread_handler\n");
	return IRQ_HANDLED;
}

static void lt9611_reset(struct lt9611 *pdata, bool on_off)
{
	pr_debug("reset: %d\n", on_off);
	if (on_off) {
		gpio_set_value(pdata->reset_gpio, 1);
		msleep(20);
		gpio_set_value(pdata->reset_gpio, 0);
		msleep(20);
		gpio_set_value(pdata->reset_gpio, 1);
		msleep(20);
	} else {
		gpio_set_value(pdata->reset_gpio, 0);
	}
}

static void lt9611_assert_5v(struct lt9611 *pdata)
{
	if (gpio_is_valid(pdata->hdmi_en_gpio)) {
		gpio_set_value(pdata->hdmi_en_gpio, 1);
		msleep(20);
	}
}

static int lt9611_config_vreg(struct device *dev,
	struct lt9611_vreg *in_vreg, int num_vreg, bool config)
{
	int i = 0, rc = 0;
	struct lt9611_vreg *curr_vreg = NULL;

	if (!in_vreg || !num_vreg)
		return rc;

	if (config) {
		for (i = 0; i < num_vreg; i++) {
			curr_vreg = &in_vreg[i];
			curr_vreg->vreg = regulator_get(dev,
					curr_vreg->vreg_name);
			rc = PTR_RET(curr_vreg->vreg);
			if (rc) {
				pr_err("%s get failed. rc=%d\n",
						curr_vreg->vreg_name, rc);
				curr_vreg->vreg = NULL;
				goto vreg_get_fail;
			}

			rc = regulator_set_voltage(
					curr_vreg->vreg,
					curr_vreg->min_voltage,
					curr_vreg->max_voltage);
			if (rc < 0) {
				pr_err("%s set vltg fail\n",
						curr_vreg->vreg_name);
				goto vreg_set_voltage_fail;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			curr_vreg = &in_vreg[i];
			if (curr_vreg->vreg) {
				regulator_set_voltage(curr_vreg->vreg,
						0, curr_vreg->max_voltage);

				regulator_put(curr_vreg->vreg);
				curr_vreg->vreg = NULL;
			}
		}
	}
	return 0;

vreg_unconfig:
	regulator_set_load(curr_vreg->vreg, 0);

vreg_set_voltage_fail:
	regulator_put(curr_vreg->vreg);
	curr_vreg->vreg = NULL;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		curr_vreg = &in_vreg[i];
		goto vreg_unconfig;
	}
	return rc;
}

static int lt9611_get_dt_supply(struct device *dev,
		struct lt9611 *pdata)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_root_node = NULL;
	struct device_node *supply_node = NULL;

	if (!dev || !pdata) {
		pr_err("invalid input param dev:%pK pdata:%pK\n", dev, pdata);
		return -EINVAL;
	}

	of_node = dev->of_node;

	pdata->num_vreg = 0;
	supply_root_node = of_get_child_by_name(of_node,
			"lt,supply-entries");
	if (!supply_root_node) {
		pr_info("no supply entry present\n");
		return 0;
	}

	pdata->num_vreg = of_get_available_child_count(supply_root_node);
	if (pdata->num_vreg == 0) {
		pr_info("no vreg present\n");
		return 0;
	}

	pr_debug("vreg found. count=%d\n", pdata->num_vreg);
	pdata->vreg_config = devm_kzalloc(dev, sizeof(struct lt9611_vreg) *
			pdata->num_vreg, GFP_KERNEL);
	if (!pdata->vreg_config)
		return -ENOMEM;

	for_each_available_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;

		rc = of_property_read_string(supply_node,
				"lt,supply-name", &st);
		if (rc) {
			pr_err("error reading name. rc=%d\n", rc);
			goto error;
		}

		strlcpy(pdata->vreg_config[i].vreg_name, st,
				sizeof(pdata->vreg_config[i].vreg_name));

		rc = of_property_read_u32(supply_node,
				"lt,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("error reading min volt. rc=%d\n", rc);
			goto error;
		}
		pdata->vreg_config[i].min_voltage = tmp;

		rc = of_property_read_u32(supply_node,
				"lt,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("error reading max volt. rc=%d\n", rc);
			goto error;
		}
		pdata->vreg_config[i].max_voltage = tmp;

		rc = of_property_read_u32(supply_node,
				"lt,supply-enable-load", &tmp);
		if (rc)
			pr_debug("no supply enable load value. rc=%d\n", rc);

		pdata->vreg_config[i].enable_load = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-disable-load", &tmp);
		if (rc)
			pr_debug("no supply disable load value. rc=%d\n", rc);

		pdata->vreg_config[i].disable_load = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-pre-on-sleep", &tmp);
		if (rc)
			pr_debug("no supply pre on sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].pre_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-pre-off-sleep", &tmp);
		if (rc)
			pr_debug("no supply pre off sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].pre_off_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-post-on-sleep", &tmp);
		if (rc)
			pr_debug("no supply post on sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].post_on_sleep = (!rc ? tmp : 0);

		rc = of_property_read_u32(supply_node,
				"lt,supply-post-off-sleep", &tmp);
		if (rc)
			pr_debug("no supply post off sleep value. rc=%d\n", rc);

		pdata->vreg_config[i].post_off_sleep = (!rc ? tmp : 0);

		pr_debug("%s min=%d, max=%d, enable=%d, disable=%d\n",
				pdata->vreg_config[i].vreg_name,
				pdata->vreg_config[i].min_voltage,
				pdata->vreg_config[i].max_voltage,
				pdata->vreg_config[i].enable_load,
				pdata->vreg_config[i].disable_load);
		++i;

		rc = 0;
	}

	rc = lt9611_config_vreg(dev,
			pdata->vreg_config, pdata->num_vreg, true);
	if (rc)
		goto error;

	return rc;

error:
	if (pdata->vreg_config) {
		devm_kfree(dev, pdata->vreg_config);
		pdata->vreg_config = NULL;
		pdata->num_vreg = 0;
	}

	return rc;
}

static void lt9611_put_dt_supply(struct device *dev,
		struct lt9611 *pdata)
{
	if (!dev || !pdata) {
		pr_err("invalid input param dev:%pK pdata:%pK\n", dev, pdata);
		return;
	}

	lt9611_config_vreg(dev,
			pdata->vreg_config, pdata->num_vreg, false);

	if (pdata->vreg_config) {
		devm_kfree(dev, pdata->vreg_config);
		pdata->vreg_config = NULL;
	}
	pdata->num_vreg = 0;
}

static int lt9611_enable_vreg(struct lt9611 *pdata, int enable)
{
	int i = 0, rc = 0;
	bool need_sleep;
	struct lt9611_vreg *in_vreg = pdata->vreg_config;
	int num_vreg = pdata->num_vreg;

	if (enable) {
		for (i = 0; i < num_vreg; i++) {
			rc = PTR_RET(in_vreg[i].vreg);
			if (rc) {
				pr_err("%s regulator error. rc=%d\n",
						in_vreg[i].vreg_name, rc);
				goto vreg_set_opt_mode_fail;
			}

			need_sleep = !regulator_is_enabled(in_vreg[i].vreg);
			if (in_vreg[i].pre_on_sleep && need_sleep)
				usleep_range(in_vreg[i].pre_on_sleep * 1000,
						in_vreg[i].pre_on_sleep * 1000);

			rc = regulator_set_load(in_vreg[i].vreg,
					in_vreg[i].enable_load);
			if (rc < 0) {
				pr_err("%s set opt m fail\n",
						in_vreg[i].vreg_name);
				goto vreg_set_opt_mode_fail;
			}

			rc = regulator_enable(in_vreg[i].vreg);
			if (in_vreg[i].post_on_sleep && need_sleep)
				usleep_range(in_vreg[i].post_on_sleep * 1000,
					in_vreg[i].post_on_sleep * 1000);
			if (rc < 0) {
				pr_err("%s enable failed\n",
						in_vreg[i].vreg_name);
				goto disable_vreg;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			if (in_vreg[i].pre_off_sleep)
				usleep_range(in_vreg[i].pre_off_sleep * 1000,
					in_vreg[i].pre_off_sleep * 1000);

			regulator_set_load(in_vreg[i].vreg,
					in_vreg[i].disable_load);
			regulator_disable(in_vreg[i].vreg);

			if (in_vreg[i].post_off_sleep)
				usleep_range(in_vreg[i].post_off_sleep * 1000,
					in_vreg[i].post_off_sleep * 1000);
		}
	}
	return rc;

disable_vreg:
	regulator_set_load(in_vreg[i].vreg, in_vreg[i].disable_load);

vreg_set_opt_mode_fail:
	for (i--; i >= 0; i--) {
		if (in_vreg[i].pre_off_sleep)
			usleep_range(in_vreg[i].pre_off_sleep * 1000,
					in_vreg[i].pre_off_sleep * 1000);

		regulator_set_load(in_vreg[i].vreg,
				in_vreg[i].disable_load);
		regulator_disable(in_vreg[i].vreg);

		if (in_vreg[i].post_off_sleep)
			usleep_range(in_vreg[i].post_off_sleep * 1000,
					in_vreg[i].post_off_sleep * 1000);
	}

	return rc;
}

static struct lt9611_timing_info *lt9611_get_supported_timing(
		struct drm_display_mode *mode)
{
	int i = 0;

	while (lt9611_supp_timing_cfg[i].xres != 0xffff) {
		if (lt9611_supp_timing_cfg[i].xres == mode->hdisplay &&
			lt9611_supp_timing_cfg[i].yres == mode->vdisplay &&
			lt9611_supp_timing_cfg[i].fps ==
					drm_mode_vrefresh(mode)) {
			return &lt9611_supp_timing_cfg[i];
		}
		i++;
	}

	return NULL;
}

/* TODO: intf/lane number needs info from both DSI host and client */
static int lt9611_get_intf_num(struct lt9611 *pdata,
	struct drm_display_mode *mode)
{
	int num_of_intfs = 0;
	struct lt9611_timing_info *timing =
			lt9611_get_supported_timing(mode);

	if (timing)
		num_of_intfs = timing->intfs;
	else {
		pr_err("interface number not defined by bridge chip\n");
		num_of_intfs = 0;
	}

	return num_of_intfs;
}

static int lt9611_get_lane_num(struct lt9611 *pdata,
	struct drm_display_mode *mode)
{
	int num_of_lanes = 0;
	struct lt9611_timing_info *timing =
				lt9611_get_supported_timing(mode);

	if (timing)
		num_of_lanes = timing->lanes;
	else {
		pr_err("lane number not defined by bridge chip\n");
		num_of_lanes = 0;
	}

	return num_of_lanes;
}

static void lt9611_get_video_cfg(struct lt9611 *pdata,
	struct drm_display_mode *mode,
	struct lt9611_video_cfg *video_cfg)
{
	int rc = 0;
	struct hdmi_avi_infoframe avi_frame;

	memset(&avi_frame, 0, sizeof(avi_frame));

	video_cfg->h_active = mode->hdisplay;
	video_cfg->v_active = mode->vdisplay;
	video_cfg->h_front_porch = mode->hsync_start - mode->hdisplay;
	video_cfg->v_front_porch = mode->vsync_start - mode->vdisplay;
	video_cfg->h_back_porch = mode->htotal - mode->hsync_end;
	video_cfg->v_back_porch = mode->vtotal - mode->vsync_end;
	video_cfg->h_pulse_width = mode->hsync_end - mode->hsync_start;
	video_cfg->v_pulse_width = mode->vsync_end - mode->vsync_start;
	video_cfg->pclk_khz = mode->clock;

	video_cfg->h_polarity = !!(mode->flags & DRM_MODE_FLAG_PHSYNC);
	video_cfg->v_polarity = !!(mode->flags & DRM_MODE_FLAG_PVSYNC);

	video_cfg->num_of_lanes = lt9611_get_lane_num(pdata, mode);
	video_cfg->num_of_intfs = lt9611_get_intf_num(pdata, mode);

	pr_debug("video=h[%d,%d,%d,%d] v[%d,%d,%d,%d] pclk=%d lane=%d intf=%d\n",
		video_cfg->h_active, video_cfg->h_front_porch,
		video_cfg->h_pulse_width, video_cfg->h_back_porch,
		video_cfg->v_active, video_cfg->v_front_porch,
		video_cfg->v_pulse_width, video_cfg->v_back_porch,
		video_cfg->pclk_khz, video_cfg->num_of_lanes,
		video_cfg->num_of_intfs);

	rc = drm_hdmi_avi_infoframe_from_display_mode(&avi_frame, mode, false);
	if (rc) {
		pr_err("get avi frame failed ret=%d\n", rc);
	} else {
		video_cfg->scaninfo = avi_frame.scan_mode;
		video_cfg->ar = avi_frame.picture_aspect;
		video_cfg->vic = avi_frame.video_code;
		pr_debug("scaninfo=%d ar=%d vic=%d\n",
			video_cfg->scaninfo, video_cfg->ar, video_cfg->vic);
	}
}

/* connector funcs */
static enum drm_connector_status
lt9611_connector_detect(struct drm_connector *connector, bool force)
{
	struct lt9611 *pdata = connector_to_lt9611(connector);
		pdata->status = connector_status_connected;

	return pdata->status;
}

static int lt9611_read_edid(struct lt9611 *pdata)
{
	return 0;
}

/* TODO: add support for more extension blocks */
static int lt9611_get_edid_block(void *data, u8 *buf, unsigned int block,
				  size_t len)
{

	pr_info("get edid block: block=%d, len=%d\n", block, (int)len);

	return 0;
}

static void lt9611_set_preferred_mode(struct drm_connector *connector)
{
	struct lt9611 *pdata = connector_to_lt9611(connector);
	struct drm_display_mode *mode;
	const char *string;

	/* use specified mode as preferred */
	if (!of_property_read_string(pdata->dev->of_node,
			"lt,preferred-mode", &string)) {
		list_for_each_entry(mode, &connector->probed_modes, head) {
			if (!strcmp(mode->name, string))
				mode->type |= DRM_MODE_TYPE_PREFERRED;
		}
	}
}

static int lt9611_connector_get_modes(struct drm_connector *connector)
{
	struct lt9611 *pdata = connector_to_lt9611(connector);
	struct drm_display_mode *mode, *m;
	unsigned int count = 0;

	if (pdata->non_pluggable) {
		list_for_each_entry(mode, &pdata->mode_list, head) {
			m = drm_mode_duplicate(connector->dev, mode);
			if (!m) {
				pr_err("failed to add hdmi mode %dx%d\n",
					mode->hdisplay, mode->vdisplay);
				break;
			}
			drm_mode_probed_add(connector, m);
		}
		count = pdata->num_of_modes;
	} else {
		struct edid *edid;

		edid = drm_do_get_edid(connector, lt9611_get_edid_block, pdata);

		drm_connector_update_edid_property(connector, edid);
		count = drm_add_edid_modes(connector, edid);

		pdata->hdmi_mode = drm_detect_hdmi_monitor(edid);
		pr_info("hdmi_mode = %d\n", pdata->hdmi_mode);

		kfree(edid);
	}

	lt9611_set_preferred_mode(connector);

	return count;
}

static enum drm_mode_status lt9611_connector_mode_valid(
	struct drm_connector *connector, struct drm_display_mode *mode)
{

	struct lt9611_timing_info *timing =
			lt9611_get_supported_timing(mode);

	return timing ? MODE_OK : MODE_BAD;
}

/* bridge funcs */
static void lt9611_bridge_enable(struct drm_bridge *bridge)
{
	pr_debug("bridge enable\n");
}

static void lt9611_bridge_disable(struct drm_bridge *bridge)
{
	pr_debug("bridge disable\n");
}

static void lt9611_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adj_mode)
{
	struct lt9611 *pdata = bridge_to_lt9611(bridge);
	struct lt9611_video_cfg *video_cfg = &pdata->video_cfg;
	int ret = 0;

	pr_debug(" hdisplay=%d, vdisplay=%d, vrefresh=%d, clock=%d\n",
		adj_mode->hdisplay, adj_mode->vdisplay,
		adj_mode->vrefresh, adj_mode->clock);

	drm_mode_copy(&pdata->curr_mode, adj_mode);

	memset(video_cfg, 0, sizeof(struct lt9611_video_cfg));
	lt9611_get_video_cfg(pdata, adj_mode, video_cfg);

	/* TODO: update intf number of host */
	if (video_cfg->num_of_lanes != pdata->dsi->lanes) {
		mipi_dsi_detach(pdata->dsi);
		pdata->dsi->lanes = video_cfg->num_of_lanes;
		ret = mipi_dsi_attach(pdata->dsi);
		if (ret)
			pr_err("failed to change host lanes\n");
	}
}

static const struct drm_connector_helper_funcs
		lt9611_connector_helper_funcs = {
	.get_modes = lt9611_connector_get_modes,
	.mode_valid = lt9611_connector_mode_valid,
};


static const struct drm_connector_funcs lt9611_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = lt9611_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};


static int lt9611_bridge_attach(struct drm_bridge *bridge)
{
	struct mipi_dsi_host *host;
	struct mipi_dsi_device *dsi;
	struct lt9611 *pdata = bridge_to_lt9611(bridge);
	int ret;
	const struct mipi_dsi_device_info info = { .type = "lt9611",
						   .channel = 0,
						   .node = NULL,
						 };

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	ret = drm_connector_init(bridge->dev, &pdata->connector,
				 &lt9611_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		DRM_ERROR("Failed to initialize connector: %d\n", ret);
		return ret;
	}

	drm_connector_helper_add(&pdata->connector,
				 &lt9611_connector_helper_funcs);

	ret = drm_connector_register(&pdata->connector);
	if (ret) {
		DRM_ERROR("Failed to register connector: %d\n", ret);
		return ret;
	}

	pdata->connector.polled = DRM_CONNECTOR_POLL_CONNECT;

	ret = drm_connector_attach_encoder(&pdata->connector,
						bridge->encoder);
	if (ret) {
		DRM_ERROR("Failed to link up connector to encoder: %d\n", ret);
		return ret;
	}

	host = of_find_mipi_dsi_host_by_node(pdata->host_node);
	if (!host) {
		pr_err("failed to find dsi host\n");
		return -EPROBE_DEFER;
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) {
		pr_err("failed to create dsi device\n");
		ret = PTR_ERR(dsi);
		goto err_dsi_device;
	}

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO_BLLP |
			  MIPI_DSI_MODE_VIDEO_EOF_BLLP;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		pr_err("failed to attach dsi to host\n");
		goto err_dsi_attach;
	}

	pdata->dsi = dsi;

	return 0;

err_dsi_attach:
	mipi_dsi_device_unregister(dsi);
err_dsi_device:
	return ret;
}

static void lt9611_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct lt9611 *pdata = bridge_to_lt9611(bridge);

	pr_debug("bridge pre_enable\n");
	lt9611_reset(pdata, true);
}

static bool lt9611_bridge_mode_fixup(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void lt9611_bridge_post_disable(struct drm_bridge *bridge)
{
	pr_debug("bridge post_disable\n");

}

static const struct drm_bridge_funcs lt9611_bridge_funcs = {
	.attach = lt9611_bridge_attach,
	.mode_fixup   = lt9611_bridge_mode_fixup,
	.pre_enable   = lt9611_bridge_pre_enable,
	.enable = lt9611_bridge_enable,
	.disable = lt9611_bridge_disable,
	.post_disable = lt9611_bridge_post_disable,
	.mode_set = lt9611_bridge_mode_set,
};

/* sysfs */
static int lt9611_dump_debug_info(struct lt9611 *pdata)
{
	if (!pdata->power_on) {
		pr_err("device is not power on\n");
		return -EINVAL;
	}

	lt9611_read_edid(pdata);

	return 0;
}

static ssize_t dump_info_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	struct lt9611 *pdata = dev_get_drvdata(dev);

	if (!pdata) {
		pr_err("pdata is NULL\n");
		return -EINVAL;
	}

	lt9611_dump_debug_info(pdata);

	return count;
}

static ssize_t firmware_upgrade_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct lt9611 *pdata = dev_get_drvdata(dev);
	int ret = 0;

	if (!pdata) {
		pr_err("pdata is NULL\n");
		return -EINVAL;
	}

	ret = request_firmware_nowait(THIS_MODULE, true,
		"lt9611_fw.bin", &pdata->i2c_client->dev, GFP_KERNEL, pdata,
		lt9611_firmware_cb);
	if (ret)
		dev_err(&pdata->i2c_client->dev,
			"Failed to invoke firmware loader: %d\n", ret);
	else
		pr_info("LT9611 starts upgrade, waiting for about 40s...\n");

	return count;
}

static ssize_t firmware_upgrade_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lt9611 *pdata = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", pdata->fw_status);
}

static DEVICE_ATTR_WO(dump_info);
static DEVICE_ATTR_RW(firmware_upgrade);

static struct attribute *lt9611_sysfs_attrs[] = {
	&dev_attr_dump_info.attr,
	&dev_attr_firmware_upgrade.attr,
	NULL,
};

static struct attribute_group lt9611_sysfs_attr_grp = {
	.attrs = lt9611_sysfs_attrs,
};

static int lt9611_sysfs_init(struct device *dev)
{
	int rc = 0;

	if (!dev) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	rc = sysfs_create_group(&dev->kobj, &lt9611_sysfs_attr_grp);
	if (rc)
		pr_err("%s: sysfs group creation failed %d\n", __func__, rc);

	return rc;
}

static void lt9611_sysfs_remove(struct device *dev)
{
	if (!dev) {
		pr_err("%s: Invalid params\n", __func__);
		return;
	}

	sysfs_remove_group(&dev->kobj, &lt9611_sysfs_attr_grp);
}

static int lt9611_probe(struct i2c_client *client,
	 const struct i2c_device_id *id)
{
	struct lt9611 *pdata;
	int ret = 0;

	if (!client || !client->dev.of_node) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("device doesn't support I2C\n");
		return -ENODEV;
	}

	pdata = devm_kzalloc(&client->dev,
		sizeof(struct lt9611), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = lt9611_parse_dt(&client->dev, pdata);
	if (ret) {
		pr_err("failed to parse device tree\n");
		goto err_dt_parse;
	}

	ret = lt9611_get_dt_supply(&client->dev, pdata);
	if (ret) {
		pr_err("failed to get dt supply\n");
		goto err_dt_parse;
	}

	pdata->dev = &client->dev;
	pdata->i2c_client = client;

	ret = lt9611_gpio_configure(pdata, true);
	if (ret) {
		pr_err("failed to configure GPIOs\n");
		goto err_dt_supply;
	}

	lt9611_assert_5v(pdata);

	ret = lt9611_enable_vreg(pdata, true);
	if (ret) {
		pr_err("failed to enable vreg\n");
		goto err_i2c_prog;
	}

	lt9611_reset(pdata, true);

	pdata->irq = gpio_to_irq(pdata->irq_gpio);
	ret = request_threaded_irq(pdata->irq, NULL, lt9611_irq_thread_handler,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lt9611", pdata);
	if (ret) {
		pr_err("failed to request irq\n");
		goto err_i2c_prog;
	}

	ret = lt9611_read_device_id(pdata);
	if (ret) {
		pr_err("failed to read chip rev\n");
		goto err_sysfs_init;
	}

	msleep(200);

	i2c_set_clientdata(client, pdata);
	dev_set_drvdata(&client->dev, pdata);

	if (lt9611_get_version(pdata) == VERSION_NUM) {
		pr_info("LT9611 works, no need to upgrade FW\n");
	} else {
		ret = request_firmware_nowait(THIS_MODULE, true,
			"lt9611_fw.bin", &client->dev, GFP_KERNEL, pdata,
				lt9611_firmware_cb);
		if (ret) {
			dev_err(&client->dev,
				"Failed to invoke firmware loader: %d\n", ret);
			goto err_sysfs_init;
		} else
			return 0;
	}

	ret = lt9611_sysfs_init(&client->dev);
	if (ret) {
		pr_err("sysfs init failed\n");
		goto err_sysfs_init;
	}

#if IS_ENABLED(CONFIG_OF)
	pdata->bridge.of_node = client->dev.of_node;
#endif

	pdata->bridge.funcs = &lt9611_bridge_funcs;
	drm_bridge_add(&pdata->bridge);

	return 0;

err_sysfs_init:
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);
err_i2c_prog:
	lt9611_gpio_configure(pdata, false);
err_dt_supply:
	lt9611_put_dt_supply(&client->dev, pdata);
err_dt_parse:
	devm_kfree(&client->dev, pdata);
	return ret;
}

static int lt9611_remove(struct i2c_client *client)
{
	int ret = -EINVAL;
	struct lt9611 *pdata = i2c_get_clientdata(client);
	struct drm_display_mode *mode, *n;

	if (!pdata)
		goto end;

	mipi_dsi_detach(pdata->dsi);
	mipi_dsi_device_unregister(pdata->dsi);

	drm_bridge_remove(&pdata->bridge);

	lt9611_sysfs_remove(&client->dev);

	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);

	ret = lt9611_gpio_configure(pdata, false);

	lt9611_put_dt_supply(&client->dev, pdata);

	if (pdata->non_pluggable) {
		list_for_each_entry_safe(mode, n, &pdata->mode_list, head) {
			list_del(&mode->head);
			kfree(mode);
		}
	}

	devm_kfree(&client->dev, pdata);

end:
	return ret;
}


static struct i2c_device_id lt9611_id[] = {
	{ "lt,lt9611uxc", 0},
	{}
};

static const struct of_device_id lt9611_match_table[] = {
	{.compatible = "lt,lt9611uxc"},
	{}
};
MODULE_DEVICE_TABLE(of, lt9611_match_table);

static struct i2c_driver lt9611_driver = {
	.driver = {
		.name = "lt9611",
		.of_match_table = lt9611_match_table,
	},
	.probe = lt9611_probe,
	.remove = lt9611_remove,
	.id_table = lt9611_id,
};

static int __init lt9611_init(void)
{
	return i2c_add_driver(&lt9611_driver);
}

static void __exit lt9611_exit(void)
{
	i2c_del_driver(&lt9611_driver);
}

module_init(lt9611_init);
module_exit(lt9611_exit);

