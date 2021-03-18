/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "inv_mpu_iio.h"
#include <linux/regulator/consumer.h>

//#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
//#endif
#define INV_SPI_READ 0x80

static int inv_spi_single_write(struct inv_mpu_state *st, u8 reg, u8 data)
{
	struct spi_message msg;
	int res;
	u8 d[2];
	struct spi_transfer xfers = {
		.tx_buf = d,
		.bits_per_word = 8,
		.len = 2,
	};

	pr_debug("reg_write: reg=0x%x data=0x%x\n", reg, data);
	d[0] = reg;
	d[1] = data;
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}

static int inv_spi_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data)
{
	struct spi_message msg;
	int res;
	u8 d[1];
	struct spi_transfer xfers[] = {
		{
		 .tx_buf = d,
		 .bits_per_word = 8,
		 .len = 1,
		 },
		{
		 .rx_buf = data,
		 .bits_per_word = 8,
		 .len = len,
		 }
	};

	if (!data)
		return -EINVAL;

	d[0] = (reg | INV_SPI_READ);

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	if (len ==1)
		pr_debug("reg_read: reg=0x%x length=%d data=0x%x\n",
							reg, len, data[0]);
	else
		pr_debug("reg_read: reg=0x%x length=%d d0=0x%x d1=0x%x\n",
					reg, len, data[0], data[1]);

	return res;

}

static int inv_spi_mem_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		     u32 len, u8 const *data)
{
	struct spi_message msg;
	u8 buf[258];
	int res;

	struct spi_transfer xfers = {
		.tx_buf = buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	if (!data || !st)
		return -EINVAL;

	if (len > (sizeof(buf) - 1))
		return -ENOMEM;

	inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);

	buf[0] = REG_MEM_R_W;
	memcpy(buf + 1, data, len);
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}

static int inv_spi_mem_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		    u32 len, u8 *data)
{
	int res;

	if (!data || !st)
		return -EINVAL;

	if (len > 256)
		return -EINVAL;

	res = inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	res = inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);
	res = inv_plat_read(st, REG_MEM_R_W, len, data);

	return res;
}

/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;
	int err;
	int irq_gpio;
	u32 irq_gpio_flags;

printk("psw ---%d----\n",__LINE__);
	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result = -ENOMEM;
		goto out_no_free;
	}
printk("psw ---%d----\n",__LINE__);
	st = iio_priv(indio_dev);
	st->write = inv_spi_single_write;
	st->read = inv_spi_read;
	st->mem_write = inv_spi_mem_write;
	st->mem_read = inv_spi_mem_read;
	st->dev = &spi->dev;

	irq_gpio = of_get_named_gpio_flags(spi->dev.of_node, "icm20602_irq-gpio",
				0, &irq_gpio_flags);
	spi->irq=gpio_to_irq(irq_gpio);

	st->irq = spi->irq;

#if 0
	st->i2c_dis = BIT_I2C_IF_DIS;
#endif
	st->bus_type = BUS_SPI;
	spi_set_drvdata(spi, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = id->name;

	st->vdd_io = regulator_get(&spi->dev, "vdd");
	regulator_set_voltage(st->vdd_io,1800000,1800000);
	err = regulator_enable(st->vdd_io);
	msleep(100);

printk("psw ---%d----\n",__LINE__);
	result = invensense_mpu_parse_dt(&spi->dev, &st->plat_data);
	if (result)
		goto out_free;
#if 0
	result = invensense_mpu_parse_dt(&spi->dev, &st->plat_data);
	if (result)
		goto out_free;
printk("psw ---%d----\n",__LINE__);
	/* Power on device */
	if (st->plat_data.power_on) {
		result = st->plat_data.power_on(&st->plat_data);
		if (result < 0) {
			dev_err(&spi->dev, "power_on failed: %d\n", result);
			goto out_free;
		}
		pr_info("%s: power on here.\n", __func__);
	}
	pr_info("%s: power on.\n", __func__);

	msleep(100);
//#else
	st->plat_data =
	    *(struct mpu_platform_data *)dev_get_platdata(&spi->dev);
printk("psw ---%d----\n",__LINE__);
#endif

	/* power is turned on inside check chip type */
	result = inv_check_chip_type(indio_dev, indio_dev->name);
	if (result)
		goto out_free;
printk("psw ---%d----\n",__LINE__);
	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}
printk("psw ---%d----\n",__LINE__);
	result = iio_buffer_register(indio_dev, indio_dev->channels,
				     indio_dev->num_channels);
	if (result) {
		pr_err("ring buffer register fail\n");
		goto out_unreg_ring;
	}
printk("psw ---%d----\n",__LINE__);
	result = iio_device_register(indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_remove_ring;
	}
printk("psw ---%d----\n",__LINE__);
	result = inv_create_dmp_sysfs(indio_dev);
	if (result) {
		pr_err("create dmp sysfs failed\n");
		goto out_unreg_iio;
	}
printk("psw ---%d----\n",__LINE__);
	init_waitqueue_head(&st->wait_queue);
	st->resume_state = true;
	wake_lock_init(&st->wake_lock, WAKE_LOCK_SUSPEND, "inv_mpu");

	dev_info(&spi->dev, "%s ma-kernel-%s is ready to go!\n",
	         indio_dev->name, INVENSENSE_DRIVER_VERSION);

	return 0;
out_unreg_iio:
	iio_device_unregister(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
	iio_device_free(indio_dev);
out_no_free:
	dev_err(&spi->dev, "%s failed %d\n", __func__, result);

	return -EIO;
}

static void inv_mpu_shutdown(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	inv_switch_power_in_lp(st, true);
	dev_dbg(&spi->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		dev_err(&spi->dev, "Failed to reset %s\n", st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = inv_set_power(st, false);
	if (result)
		dev_err(&spi->dev, "Failed to turn off %s\n", st->hw->name);
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);
}

/*
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);

	iio_device_unregister(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
	iio_device_free(indio_dev);
	dev_info(&spi->dev, "inv-mpu-iio module removed.\n");

	return 0;
}

#ifdef CONFIG_PM
/*
 * inv_mpu_suspend(): suspend method for this driver.
 *    This method can be modified according to the request of different
 *    customers. If customer want some events, such as SMD to wake up the CPU,
 *    then data interrupt should be disabled in this interrupt to avoid
 *    unnecessary interrupts. If customer want pedometer running while CPU is
 *    asleep, then pedometer should be turned on while pedometer interrupt
 *    should be turned off.
 */
static int inv_mpu_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	pr_info("%s inv_mpu_suspend\n", st->hw->name);
	mutex_lock(&indio_dev->mlock);

	st->resume_state = false;
	if (st->chip_config.wake_on) {
		enable_irq_wake(st->irq);
	} else {
		inv_stop_interrupt(st);
	}

	mutex_unlock(&indio_dev->mlock);

	return 0;
}

/*
 * inv_mpu_complete(): complete method for this driver.
 *    This method can be modified according to the request of different
 *    customers. It basically undo everything suspend is doing
 *    and recover the chip to what it was before suspend. We use complete to
 *    make sure that alarm clock resume is finished. If we use resume, the
 *    alarm clock may not resume yet and get incorrect clock reading.
 */
static void inv_mpu_complete(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	pr_info("%s inv_mpu_complete\n", st->hw->name);
	if (st->resume_state)
		return;
	mutex_lock(&indio_dev->mlock);

	if (!st->chip_config.wake_on) {
		inv_reenable_interrupt(st);
	} else {
		disable_irq_wake(st->irq);
	}
	/* resume state is used to synchronize read_fifo such that it won't
	    proceed unless resume is finished. */
	st->resume_state = true;
	/* resume flag is indicating that current clock reading is from resume,
	   it has up to 1 second drift and should do proper processing */
	st->ts_algo.resume_flag  = true;
	mutex_unlock(&indio_dev->mlock);
	wake_up_interruptible(&st->wait_queue);

	return;
}

static const struct dev_pm_ops inv_mpu_pmops = {
	.suspend = inv_mpu_suspend,
	.complete = inv_mpu_complete,
};

#define INV_MPU_PMOPS (&inv_mpu_pmops)
#else
#define INV_MPU_PMOPS NULL
#endif /* CONFIG_PM */

static const struct spi_device_id inv_mpu_id[] = {
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	{"icm20645", ICM20645},
	{"icm10340", ICM10340},
	{"icm20648", ICM20648},
#else
	{"icm20608d", ICM20608D},
	{"icm20690", ICM20690},
	{"icm20602", ICM20602},
#endif
	{}
};

static struct of_device_id match_table[] = {
	{
		.compatible = "invn,icm20602",
	},
	{}
};

MODULE_DEVICE_TABLE(spi, inv_mpu_id);

static struct spi_driver inv_mpu_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "inv-mpu-iio-spi",
		.of_match_table = match_table,
		.pm = INV_MPU_PMOPS,
	},
	.probe = inv_mpu_probe,
	.remove = inv_mpu_remove,
	.id_table = inv_mpu_id,
	.shutdown = inv_mpu_shutdown,
};
module_spi_driver(inv_mpu_driver);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense SPI device driver");
MODULE_LICENSE("GPL");
