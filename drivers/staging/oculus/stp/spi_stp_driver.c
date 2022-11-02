// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/spi/spi.h>
#include <linux/kthread.h>
#include <linux/memblock.h>
#include <linux/random.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <stp_master.h>
#include <spi_stp_dev.h>
#include <stp_master_common.h>
#include <stp_debug.h>
#include <stp_router.h>

#define STP_PRIORITY_THREAD 50
#define DEVICE_WAKE_TIME_MS 1000

/* SPI STP data internal data */
struct spi_stp_data_type {
	struct spi_device *spi;

	/* GPIO MCU ready for data */
	int gpio_mcu_ready_for_receive_data;

	/*GPIO MCU ready to send data */
	int gpio_mcu_ready_to_send_data;

	struct task_struct *stp_thread;
	struct task_struct *stp_router_thread;

	unsigned int gpio_slave_has_data;
	unsigned int gpio_slave_can_receive;

	wait_queue_head_t start_q;

	struct completion read_done;
	struct completion write_done;

	bool exit;

	bool suspended;

	struct completion suspend_notification_sent;
};

/* Pointer to a singleton internal data object */
static struct spi_stp_data_type *_spi_stp_data;

static int stp_thread(void *data);
static int stp_router_thread(void *data);

/* Size of STP pipelines */
#define STP_PIPELINE_TX_SIZE (1024*300)
#define STP_PIPELINE_RX_SIZE (1024*1024*1)

/* STP pipelines buffers */
static U8 stp_buffer_tx_pipeline[STP_PIPELINE_TX_SIZE];
static U8 stp_buffer_rx_pipeline[STP_PIPELINE_RX_SIZE];

struct stp_pipeline_config stp_rx_pipeline_info = {
	stp_buffer_rx_pipeline, sizeof(stp_buffer_rx_pipeline)
};

struct stp_pipeline_config stp_tx_pipeline_info = {
	stp_buffer_tx_pipeline, sizeof(stp_buffer_tx_pipeline)
};

struct stp_pipelines_config stp_pipelines_config_info = {
	&stp_rx_pipeline_info,
	&stp_tx_pipeline_info
};

/* SPI specific implementation */

irqreturn_t stp_irq_mcu_has_data(int irq, void *dev_id)
{
#ifdef STP_DEBUG_GPIO
	pr_err("STP: GPIO slave_has_data triggered IRQ\n");
#endif
	pm_wakeup_event(&_spi_stp_data->spi->dev, DEVICE_WAKE_TIME_MS);

	stp_ie_record(STP_IE_IRQ_DATA, 0);

	if (_spi_stp_data->suspended) {
		/*
		 * If in suspended state, let it be handled in the
		 *   driver resume callback
		 */
		dev_dbg(&_spi_stp_data->spi->dev,
			"mcu_has_data IRQ (%d) in suspend state\n", irq);
	} else {
		stp_master_signal_data();
	}

	return IRQ_HANDLED;
}

irqreturn_t stp_irq_mcu_ready(int irq, void *dev_id)
{
#ifdef STP_DEBUG_GPIO
	pr_err("STP: GPIO slave_ready triggered IRQ\n");
#endif

	stp_ie_record(STP_IE_IRQ_READY, 0);

	stp_master_signal_slave_ready();

	return IRQ_HANDLED;
}

int stp_config_irq_mcu_has_data(void)
{
	int irq_number = 0;

	if (devm_gpio_request(&_spi_stp_data->spi->dev,
		_spi_stp_data->gpio_slave_has_data,
		"MCU_has_data")) {
		pr_err("STP: GPIO(%d) request failure\n",
		_spi_stp_data->gpio_slave_has_data);
		return STP_ERROR;
	}

	irq_number = gpio_to_irq(_spi_stp_data->gpio_slave_has_data);

	if (irq_number < 0) {
		pr_err("STP: GPIO(%d) to IRQ mapping failure\n",
		_spi_stp_data->gpio_slave_has_data);
		goto free_gpio;
	}

	if (devm_request_irq(&_spi_stp_data->spi->dev,
		irq_number, (irq_handler_t)stp_irq_mcu_has_data,
		IRQF_TRIGGER_FALLING, "MCU_has_data", NULL)) {
		pr_err("STP: Irq Request failure\n");
		goto free_gpio;
	}

	if (enable_irq_wake(irq_number)) {
		pr_err("STP: Failed to set IRQ wake for %d\n", irq_number);
		goto free_irq;
	}

	return 0;

free_irq:
	devm_free_irq(&_spi_stp_data->spi->dev, irq_number, NULL);
free_gpio:
	devm_gpio_free(&_spi_stp_data->spi->dev,
		_spi_stp_data->gpio_slave_has_data);
	return STP_ERROR;
}

int stp_config_irq_mcu_ready(void)
{
	int irq_number = 0;

	if (devm_gpio_request(&_spi_stp_data->spi->dev,
		_spi_stp_data->gpio_slave_can_receive,
		"MCU_ready")) {
		pr_err("STP: GPIO(%d) request failure\n",
		_spi_stp_data->gpio_slave_can_receive);
		return STP_ERROR;
	}

	irq_number = gpio_to_irq(_spi_stp_data->gpio_slave_can_receive);

	if (irq_number < 0) {
		pr_err("STP: GPIO(%d) to IRQ mapping failure\n",
		_spi_stp_data->gpio_slave_can_receive);
		goto free_gpio;
	}

	if (devm_request_irq(&_spi_stp_data->spi->dev,
		irq_number, (irq_handler_t)stp_irq_mcu_ready,
		IRQF_TRIGGER_RISING, "MCU_ready", NULL)) {
		pr_err("STP: Irq Request failure\n");
		goto free_gpio;
	}

	return 0;

free_gpio:
	devm_gpio_free(&_spi_stp_data->spi->dev,
		_spi_stp_data->gpio_slave_can_receive);
	return STP_ERROR;
}

bool is_mcu_data_available(void)
{
	int value;

	value = gpio_get_value(_spi_stp_data->gpio_slave_has_data);

	/* 0 means MCU has data to send */
	return (value == 0);
}

/* This check the GPIO set by MCU when is ready to receive data */
/* This GPIO not working yet, need to investigate it */
bool is_mcu_ready_to_receive_data(void)
{
	int value;

	value = gpio_get_value(_spi_stp_data->gpio_slave_can_receive);

	/* 1 means MCU can receive data */
	return (value != 0);
}

static struct stp_master_handshake_table handshake_table = {
	.slave_has_data = &is_mcu_data_available,
	.slave_can_receive = &is_mcu_ready_to_receive_data
};

static int write_read_data(u8 *write_buffer,
	u8 *read_buffer, unsigned int len_buffer)
{
	int ret = 0;

	struct spi_transfer t = {
			.tx_buf		= write_buffer,
			.rx_buf		= read_buffer,
			.len			= len_buffer,
	};

	stp_ie_record(STP_IE_ENTER_SPI, 0);
	ret = spi_sync_transfer(_spi_stp_data->spi, &t, 1);
	stp_ie_record(STP_IE_EXIT_SPI, 0);

	return ret;
}

static struct stp_transport_table spi_transport_table = {
	.send_receive_data = &write_read_data
};

void spi_stp_signal_read(void)
{
	complete(&_spi_stp_data->read_done);
}

int spi_stp_wait_for_read(void)
{
	return wait_for_completion_interruptible(
			&_spi_stp_data->read_done);
}

void spi_stp_signal_write(void)
{
	complete(&_spi_stp_data->write_done);
}

int spi_stp_wait_for_write(void)
{
	return wait_for_completion_interruptible(
			&_spi_stp_data->write_done);
}

static struct stp_master_wait_signal_table stp_wait_signal_table = {
	.wait_write = spi_stp_wait_for_write,
	.signal_write = spi_stp_signal_write,
	.wait_read = spi_stp_wait_for_read,
	.signal_read = spi_stp_signal_read
};

/* Initialize the SPI_STP data */
static int spi_stp_populate_data(struct spi_device *spi)
{
	int ret = 0;

	if (!spi) {
		pr_err("STP error: Invalid parameter!\n");
		ret = -1;
		goto error;
	}

	if (_spi_stp_data) {
		dev_err(&_spi_stp_data->spi->dev, "Data already initialized\n");
		ret = -1;
		goto error;
	}

	_spi_stp_data = kzalloc(sizeof(*_spi_stp_data), GFP_KERNEL);
	if (!_spi_stp_data) {
		ret = -ENOMEM;
		goto error;
	}

	_spi_stp_data->spi = spi;

	_spi_stp_data->gpio_slave_can_receive = of_get_named_gpio(
		spi->dev.of_node, "mcu-ready", 0);

	_spi_stp_data->gpio_slave_has_data = of_get_named_gpio(
		spi->dev.of_node, "mcu-has-data", 0);

	init_waitqueue_head(&_spi_stp_data->start_q);
	init_completion(&_spi_stp_data->suspend_notification_sent);

	init_completion(&_spi_stp_data->read_done);
	init_completion(&_spi_stp_data->write_done);

	_spi_stp_data->stp_thread = kthread_run(
		stp_thread, NULL, "STP thread");

	if (IS_ERR(_spi_stp_data->stp_thread)) {
		dev_err(&_spi_stp_data->spi->dev, "STP thread can't start\n");
		ret = STP_ERROR;
		goto error;
	}

	_spi_stp_data->stp_router_thread = kthread_run(
		stp_router_thread, NULL, "STP Router thread");

	if (IS_ERR(_spi_stp_data->stp_router_thread)) {
		dev_err(&_spi_stp_data->spi->dev, "STP Router thread can't start\n");
		ret = STP_ERROR;
		goto error;
	}


error:
	if (ret) {
		kfree(_spi_stp_data);
		_spi_stp_data = NULL;
		}

	return ret;
}

/* Remove SPI STP data */
static int spi_stp_remove_data(void)
{
	int ret = 0;

	if (!_spi_stp_data) {
		dev_err(&_spi_stp_data->spi->dev, "Data already removed\n");
		return STP_ERROR;
	}

	_spi_stp_data->exit = true;
	/* we force STP getting out of waits so we cab exit its thread */
	stp_master_signal_slave_ready();
	stp_master_signal_data();

	kthread_stop(_spi_stp_data->stp_thread);
	kthread_stop(_spi_stp_data->stp_router_thread);

	kfree(_spi_stp_data);

	_spi_stp_data = NULL;

	return ret;
}

int spi_stp_configure_gpio_mcu_has_data(void)
{
	int ret = 0;

	if (gpio_direction_input(_spi_stp_data->gpio_slave_has_data)) {
		pr_err("STP errro: GPIO(%d) setting\n",
			_spi_stp_data->gpio_slave_has_data);
		ret = STP_ERROR;
		goto error;
	}

	if (stp_config_irq_mcu_has_data() != 0) {
		ret = STP_ERROR;
		goto error;
	}

error:
	return ret;
}

int spi_stp_configure_gpio_mcu_ready(void)
{
	int ret = 0;

	if (gpio_direction_input(_spi_stp_data->gpio_slave_can_receive)) {
		pr_err("STP error: GPIO(%d) setting\n",
			_spi_stp_data->gpio_slave_can_receive);
		ret = STP_ERROR;
		goto error;
	}

	if (stp_config_irq_mcu_ready() != 0) {
		ret = STP_ERROR;
		goto error;
	}

error:
	return ret;
}

void spi_stp_signal_start(void)
{
	wake_up_interruptible(&_spi_stp_data->start_q);
}

void spi_stp_wait_for_start(void)
{
	wait_event_interruptible(_spi_stp_data->start_q,
		stp_dev_stp_start());
}

void spi_stp_signal_suspend(void)
{
	complete(&_spi_stp_data->suspend_notification_sent);
}

#ifdef CONFIG_PM_SLEEP
static int spi_stp_suspend(struct device *dev)
{
	dev_dbg(dev, "suspend\n");

	if (!is_mcu_ready_to_receive_data()) {
		dev_info(dev, "mcu not ready to receive data, abort suspend");
		return -EBUSY;
	}

	if (stp_has_data_to_send()) {
		dev_info(dev, "STP has data to sent, abort suspend");
		return -EBUSY;
	}

	if (!stp_get_wait_for_data()) {
		dev_info(dev, "STP not waiting for data, abort suspend");
		return -EBUSY;
	}

	_spi_stp_data->suspended = true;
	stp_set_notification(STP_SOC_SUSPENDED);

	reinit_completion(&_spi_stp_data->suspend_notification_sent);
	wait_for_completion(
		&_spi_stp_data->suspend_notification_sent);

	return 0;
}

static int spi_stp_resume(struct device *dev)
{
	dev_dbg(dev, "resume\n");

	_spi_stp_data->suspended = false;
	stp_set_notification(STP_SOC_RESUMED);

	return 0;
}
#else
#define spi_stp_suspend NULL
#define spi_stp_resume NULL
#endif

/* This is the STP transaction thread */
static int stp_thread(void *data)
{

	int ret = 0;
	struct sched_param param;

	param.sched_priority = STP_PRIORITY_THREAD;
	if (sched_setscheduler(current, SCHED_FIFO, &param) == -1)
		pr_err("STP: Error setting priority!\n");

	spi_stp_wait_for_start();

	if (spi_stp_configure_gpio_mcu_has_data() != 0)
		return ret;

	if (spi_stp_configure_gpio_mcu_ready() != 0)
		return ret;

	while (!_spi_stp_data->exit) {
		ret = stp_master_transaction_thread();
		if (ret != 0)
			break;
	}

	return ret;
}

/* This is the STP Router transaction thread */
static int stp_router_thread(void *data)
{
	int ret = STP_ROUTER_SUCCESS;

	while (!_spi_stp_data->exit) {
		ret = stp_router_transaction_thread();
		/* TODO: maybe add a delay when ret is not SUCCESS */
	}

	return ret;
}

static int spi_stp_probe(struct spi_device *spi)
{
	int ret = 0;
	struct stp_init_t stp_init = {
		.transport = &spi_transport_table,
		.handshake = &handshake_table,
		.pipelines = &stp_pipelines_config_info,
		.wait_signal = &stp_wait_signal_table,
	};

	spi->mode |= SPI_CPHA;

	ret = stp_init_master(&stp_init);
	if (ret != 0)
		goto error;

	ret = spi_stp_populate_data(spi);
	if (ret != 0)
		goto error;

	ret = stp_dev_init(&spi->dev);
	if (ret != 0) {
		dev_err(&spi->dev, "failed to init stp_dev %d\n", ret);
		goto dev_error;
	}

	ret = stp_raw_dev_init(spi);
	if (ret != 0) {
		dev_err(&spi->dev, "failed to init stp_raw %d\n", ret);
		goto raw_dev_error;
	}

	ret = device_init_wakeup(&spi->dev, true);
	if (ret != 0) {
		dev_err(&spi->dev, "%s: failed to init wakesource\n", __func__);
		goto error;
	}

	dev_info(&spi->dev, "%s: spi mode = %d, bpw = %d, max_speed_hz = %d\n",
		__func__, spi->mode, spi->bits_per_word, spi->max_speed_hz);

	return 0;

error:
	stp_raw_dev_remove();
raw_dev_error:
	stp_dev_remove();
dev_error:
	spi_stp_remove_data();

	return ret;
}

int spi_stp_remove(struct spi_device *spi)
{
	int ret = 0;

	stp_raw_dev_remove();

	ret = spi_stp_remove_data();

	if (ret) {
		dev_err(&spi->dev, "SPI_STP: remove  data failed (%d)\n", ret);
		goto error;
	}

	ret = stp_dev_remove();
	if (ret != 0)
		goto error;

	ret = stp_deinit_master();
	if (ret != 0)
		goto error;

	ret = device_init_wakeup(&spi->dev, false);
	if (ret != 0)
		dev_err(&spi->dev, "%s: failed to init wakesource\n", __func__);

	dev_err(&spi->dev, "SPI_STP: removed\n");

error:
	return ret;
}

static const struct dev_pm_ops spi_stp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(spi_stp_suspend, spi_stp_resume)
};

static const struct of_device_id spi_stp_of_match[] = {
	{ .compatible = "oculus,spi-stp"	 },
	{ }
};

static struct spi_driver spi_stp_driver = {
	.driver		= {
		.name	= "spi_stp_driver",
		.owner	= THIS_MODULE,
		.of_match_table = spi_stp_of_match,
		.pm	= &spi_stp_pm_ops,
	},
	.probe		= spi_stp_probe,
	.remove		= spi_stp_remove,
};

static int __init spi_stp_driver_init(void)
{
	int ret;

	ret = spi_register_driver(&spi_stp_driver);

	return ret;
}

static void __exit spi_stp_driver_exit(void)
{
	pr_err("%s\n", __func__);

	spi_unregister_driver(&spi_stp_driver);
}

module_init(spi_stp_driver_init);
module_exit(spi_stp_driver_exit);

MODULE_DESCRIPTION("SPI - Synchronized Transport Protocol");
MODULE_LICENSE("GPL v2");
