/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * TEE driver for goodix fingerprint sensor
 * Copyright (C) 2016 Goodix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __GF_SPI_H
#define __GF_SPI_H

#include <linux/types.h>
#include <linux/notifier.h>

/**********************************************************/

enum gf_error_type {
	GF_NO_ERROR = 0,
	GF_PERM_ERROR = 1,
};

#define GF_DEVICE_AVAILABLE 1
#define GF_DEVICE_NOT_AVAILABLE 0
#define GF_SPI_CLK_ENABLED 1
#define GF_SPI_CLK_DISABLED 0
#define GF_NETLINK_ENABLE   1

#define GF_NET_EVENT_IRQ 1
#define GF_NET_EVENT_FB_BLACK 2
#define GF_NET_EVENT_FB_UNBLACK 3
#define GF_DEFAULT_SPEED 4800000
#define NETLINK_TEST 25

/****************Chip Specific***********************/
#define GF_W	0xF0
#define GF_R	0xF1
#define GF_WDATA_OFFSET	(0x3)
#define GF_RDATA_OFFSET	(0x4)

struct gf_configs {
	unsigned short addr;
	unsigned short value;
};

struct gf_mode_config {
	struct gf_configs *p_cfg;
	unsigned int cfg_len;
};

enum gf_spi_transfer_speed {
	GF_SPI_LOW_SPEED = 0,
	GF_SPI_HIGH_SPEED,
	GF_SPI_KEEP_SPEED,
};


struct gf_ioc_chip_info {
	unsigned char vendor_id;
	unsigned char mode;
	unsigned char operation;
	unsigned char reserved[5];
};

#define GF_IOC_MAGIC    'g'     //define magic number
#define GF_IOC_INIT             _IOR(GF_IOC_MAGIC, 0, uint8_t)
#define GF_IOC_EXIT             _IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET            _IO(GF_IOC_MAGIC, 2)
#define GF_IOC_ENABLE_IRQ       _IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ      _IO(GF_IOC_MAGIC, 4)
#define GF_IOC_CLK_READY   _IO(GF_IOC_MAGIC, 5)
#define GF_IOC_CLK_UNREADY  _IO(GF_IOC_MAGIC, 6)
#define GF_IOC_POWER_ON     _IO(GF_IOC_MAGIC, 7)
#define GF_IOC_POWER_OFF    _IO(GF_IOC_MAGIC, 8)
#define GF_IOC_SENDKEY  _IOW(GF_IOC_MAGIC, 9, struct gf_key)
#define GF_IOC_ENTER_SLEEP_MODE _IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO      _IOR(GF_IOC_MAGIC, 11, uint8_t)
#define GF_IOC_REMOVE           _IO(GF_IOC_MAGIC, 12)
#define GF_IOC_CHIP_INFO        _IOR(GF_IOC_MAGIC, 13, struct gf_ioc_chip_info)

#define  GF_IOC_MAXNR    14  /* THIS MACRO IS NOT USED NOW... */

#define TZBSP_APSS_ID                   3
#define TZBSP_TZ_ID                     1

#define DEVICE_SPI_TEST 1

#define gf_dbg(fmt, args...) pr_warn("[goodix]:" fmt, ##args)

#define gf_dbg_len(buffer, len) do { \
	char log_buf[256] = {0}; \
	int _i, pos = 0; \
	for (_i = 0; _i < (len) && pos < sizeof(log_buf); _i++) { \
		int ret = snprintf(log_buf + pos, sizeof(log_buf) - pos, "%02X ", (buffer)[_i]); \
		if (ret > 0) { \
			pos += ret; \
		} else { \
			pr_warn("[goodix]: snprintf failed or buffer is full\n"); \
			break; \
		} \
	} \
	pr_warn("[goodix]: %s\n", log_buf); \
} while (0)

#define FUNC_ENTRY()  pr_warn("[goodix]:%s, [entry]\n", __func__)
#define FUNC_EXIT()  pr_warn("[goodix]:%s, [exit]\n", __func__)

struct gf_ioc_transfer {
	unsigned char cmd;
	unsigned char reserve;
	unsigned short addr;
	unsigned int len;
	unsigned char *buf;
};

struct gf_key {
	unsigned int key;
	int value;
};

struct gf_key_map {
	char *name;
	unsigned short val;
};

struct gf_dev {
	dev_t devt;
	spinlock_t	spi_lock;
	struct list_head device_entry;
	struct spi_device *spi;

	struct clk *core_clk;
	struct clk *iface_clk;

	struct input_dev *input;
	/* buffer is NULL unless this device is open (users > 0) */
	struct input_dev *lge_input;
	struct regulator *vreg;
	u32 qup_id;
	u32 vddio_uV;
	bool pipe_owner;
	bool power_on;
	int users;
	int irq_gpio;
	int reset_gpio;
	int cs_gpio;
	int pwr_gpio;
	int irq;
	int irq_enabled;
	int clk_enabled;
	char device_available;
	char fb_black;
	unsigned char *gBuffer;
	struct mutex buf_lock;
	struct mutex frame_lock;
};

int  gf_parse_dts(struct gf_dev *gf_dev);
void gf_cleanup(struct gf_dev *gf_dev);

int gf_power_on(struct gf_dev *gf_dev);
int gf_power_off(struct gf_dev *gf_dev);

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms);
int gf_irq_num(struct gf_dev *gf_dev);

void sendnlmsg(char *message);
int netlink_init(void);
void netlink_exit(void);

#endif /*__GF_SPI_H*/
