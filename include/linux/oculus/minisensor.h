#ifndef __MINI_SENSOR__
#define __MINI_SENSOR__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscfifo.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/spinlock.h>
#include <linux/regulator/consumer.h>

#include <uapi/linux/oculus/minisensor.h>

#define REG_READ_FLAG			0x80

#define REG_FUNC_CFG_ACCESS		0x01
#define VAL_FUNC_CFG_NORMAL		0x00
#define VAL_FUNC_CFG_EMB_BANK_A		(1 << 7)
#define REG_BANK_SWITCH_DELAY_US	1000

const size_t SPI_BUF_SZ = 4096;
const size_t SPI_RX_LIMIT = 4096;
const size_t SPI_TX_LIMIT = 64;

struct minisensor_device {
	struct device *dev; /* shortcut to misc device */
	struct spi_device *spi;
	struct miscdevice misc;
	struct miscfifo mf;

	struct regulator *reg_vdd_io;
	u32 vdd_io_min_uV; /* vdd_io voltage in uV */
	u32 vdd_io_max_uV; /* vdd_io voltage in uV */

	atomic_long_t irq_timestamp;

	struct {
		struct mutex lock;
		struct minisensor_interrupt_config interrupt_config;
		struct minisensor_pm_config suspend_cfg;
		struct minisensor_pm_config resume_cfg;
	} state;

	struct {
		struct mutex lock;
		/* DMA safe buffers for SPI transfer */
		u8 *rx_buf;
		u8 *tx_buf;
	} io;

	/* HACKS! */
	bool has_lsm6dsl_embedded_pages;

	u16 wai_addr;
	u8 wai_value;
};

#endif /* __MINI_SENSOR__ */
