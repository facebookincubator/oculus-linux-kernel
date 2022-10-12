#include <asm-generic/errno-base.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <uapi/linux/sched/types.h>
#endif

#include <common/stp_device_logging.h>
#include <common/stp_error_mapping.h>
#include <device/stp_device.h>
#include <driver/stp_driver_data.h>
#include <driver/stp_gpio.h>
#include <stp/controller/stp_controller.h>
#include <stp/controller/stp_controller_common.h>

// Handle that stores the persistent driver information
static struct spi_stp_driver_data *_stp_driver_data;

#define STP_PRIORITY_THREAD 50
static int stp_thread(void *data)
{
	int rval = 0;
	struct sched_param param;

	param.sched_priority = STP_PRIORITY_THREAD;
	if (sched_setscheduler(current, SCHED_FIFO, &param) == -1)
		STP_DRV_LOG_ERR("error setting priority");

	while (atomic_read(&_stp_driver_data->stop_thread) == 0) {
		rval = STP_ERR_VAL(stp_controller_transaction_thread());
		if (STP_ERR_VAL(rval)) {
			STP_DRV_LOG_ERR("transaction error `%d`, exiting",
					rval);
			break;
		}
	}

	complete_and_exit(&_stp_driver_data->stp_thread_complete, 0);

	return rval;
}

static ssize_t stp_driver_stats_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t rval;

	rval = scnprintf(
		buf, PAGE_SIZE,
		"device ready irq count: %d\n"
		"data irq count: %d\n",
		atomic_read(&_stp_driver_data->stats.device_ready_irq_count),
		atomic_read(&_stp_driver_data->stats.data_irq_count));
	return rval;
}
static DEVICE_ATTR_RO(stp_driver_stats);

static ssize_t stp_connection_state_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t rval = 0;
	ssize_t char_count = 0;
	uint32_t synced = 0;

	stp_controller_get_attribute(STP_ATTRIB_SYNCED, &synced);

	char_count = scnprintf(
		buf + rval, PAGE_SIZE - rval,
		"SYNCED: state:%u\n",
		synced);
	rval += char_count;

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		uint32_t valid = 0;
		uint32_t controller_connected = 0;
		uint32_t device_connected = 0;
		uint32_t rx_data = 0;

		stp_controller_get_channel_attribute(i, STP_ATTRIB_VALID_SESSION, &valid);
		stp_controller_get_channel_attribute(i, STP_ATTRIB_CONTROLLER_CONNECTED, &controller_connected);
		stp_controller_get_channel_attribute(i, STP_ATTRIB_DEVICE_CONNECTED, &device_connected);
		stp_controller_get_channel_attribute(i, STP_RX_FILLED, &rx_data);

		char_count = scnprintf(
			buf + rval, PAGE_SIZE - rval,
			"CHANNEL:%u\n"
			"valid_session:%u\n"
			"controller:%u device:%u\n"
			"rx_pipe_filled:%u\n",
			i,
			valid,
			controller_connected,
			device_connected,
			rx_data);
		rval += char_count;
	}
	STP_DRV_LOG_INFO("stp_connection_state char count: %zu", rval);
	return rval;
}
static DEVICE_ATTR_RO(stp_connection_state);

static ssize_t stp_log_channel_data_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t rval = 0;

	ssize_t char_count = 0;
	uint32_t log_tx_value = 0;
	uint32_t log_rx_value = 0;

	char_count = scnprintf(
			buf + rval, PAGE_SIZE - rval,
			"Channels with log enabled:\n"
			);
	rval += char_count;

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		stp_controller_get_channel_attribute(i, STP_CONTROLLER_ATTRIB_SET_LOG_TX_DATA, &log_tx_value);
		stp_controller_get_channel_attribute(i, STP_CONTROLLER_ATTRIB_SET_LOG_RX_DATA, &log_rx_value);

		if (log_tx_value || log_rx_value) {
			char_count = scnprintf(
				buf + rval, PAGE_SIZE - rval,
				"CHANNEL:%u\n"
				"  LOG_TX VALUE:%u\n"
				"  LOG_RX VALUE:%u\n",
				i,
				log_tx_value,
				log_rx_value
				);
		}
		rval += char_count;
	}

	return rval;
}

static ssize_t stp_log_channel_data_store(
				struct device *dev,
				struct device_attribute *attr,
				 const char *buf, size_t len)
{
	uint32_t channel	= 0;
	uint32_t attribute	= 0;
	uint32_t value		= 0;
	char direction[3]	= "";

	if (sscanf(buf, "stp setchannel %2s %u %u",
				direction, &channel, &value) != 3) {
		STP_DRV_LOG_ERR("stp_log_channel_data: Usage: stp setchannel {tx | rx} <channel ID> {0 | 1}");
		return len;
	}

	if (channel < 0 || channel >= STP_TOTAL_NUM_CHANNELS) {
		STP_DRV_LOG_ERR("stp_log_channel_data: Incorrect channel ID");
		return len;
	}

	if (!strncmp(direction, "tx", 2)) {
		attribute = STP_CONTROLLER_ATTRIB_SET_LOG_TX_DATA;
	} else if (!strncmp(direction, "rx", 2)) {
		attribute = STP_CONTROLLER_ATTRIB_SET_LOG_RX_DATA;
	} else {
		STP_DRV_LOG_ERR("stp_log_channel_data: Usage: stp setchannel {tx | rx} <channelId> {0 | 1}");
		return len;
	}

	stp_controller_set_channel_attribute32((uint8_t) channel, attribute, value);

	return len;
}
static DEVICE_ATTR_RW(stp_log_channel_data);

static bool stp_is_mcu_data_available(void)
{
	int value;

	if (!_stp_driver_data)
		return false;

	value = gpio_get_value(_stp_driver_data->gpio_data.device_has_data);

	// 0 means MCU has data to send
	return value == 0;
}

static bool stp_is_mcu_ready_to_receive(void)
{
	int value;

	if (!_stp_driver_data)
		return false;

	value = gpio_get_value(_stp_driver_data->gpio_data.device_can_receive);

	// 1 means MCU can receive data
	return value != 0;
}

static struct stp_controller_handshake_table stp_handshake_table = {
	.device_has_data = &stp_is_mcu_data_available,
	.device_can_receive = &stp_is_mcu_ready_to_receive,
};

static irqreturn_t stp_irq_mcu_has_data(int irq, void *dev_id)
{
	stp_controller_signal_data();
	atomic_inc(&_stp_driver_data->stats.data_irq_count);

	return IRQ_HANDLED;
}

static irqreturn_t stp_irq_mcu_ready(int irq, void *dev_id)
{
	stp_controller_signal_device_ready();
	atomic_inc(&_stp_driver_data->stats.device_ready_irq_count);

	return IRQ_HANDLED;
}

static int32_t stp_wait_for_write(uint8_t channel)
{
	return stp_channel_wait_write(channel);
}

static void stp_signal_write(uint8_t channel)
{
	stp_channel_signal_write(channel);
}

static int32_t stp_wait_for_read(uint8_t channel)
{
	return stp_channel_wait_read(channel);
}

static void stp_signal_read(uint8_t channel)
{
	stp_channel_signal_read(channel);
}

static int32_t stp_wait_fsync(uint8_t channel)
{
	return stp_channel_wait_fsync(channel);
}

static void stp_signal_fsync(uint8_t channel)
{
	stp_channel_signal_fsync(channel);
}

static void stp_reset_fsync(uint8_t channel)
{
	stp_channel_reset_fsync(channel);
}

static int32_t stp_wait_open(uint8_t channel)
{
	return stp_channel_wait_open(channel);
}

static void stp_signal_open(uint8_t channel)
{
	stp_channel_signal_open(channel);
}

static void stp_set_ready_for_transaction(bool busy)
{
	if (busy)
		atomic_set(&_stp_driver_data->device_ready_flag, 1);
	else
		atomic_set(&_stp_driver_data->device_ready_flag, 0);
}

#define STP_MCU_READY_TIMEOUT_MS 1000
static int32_t stp_wait_for_device_ready(void)
{
	if (!!atomic_read(&_stp_driver_data->stop_thread))
		return -ERESTARTSYS;

	return wait_event_interruptible_timeout(
		_stp_driver_data->device_ready_q,
		!!atomic_read(&_stp_driver_data->device_ready_flag),
		msecs_to_jiffies(STP_MCU_READY_TIMEOUT_MS));
}

static void stp_signal_device_ready(void)
{
	wake_up_interruptible(&_stp_driver_data->device_ready_q);
}

static void stp_wait_for_data(void)
{
	if (!!atomic_read(&_stp_driver_data->stop_thread))
		return;
	wait_for_completion_interruptible(&_stp_driver_data->data_ready);
}

static void stp_signal_data(void)
{
	complete(&_stp_driver_data->data_ready);
}

static struct stp_controller_wait_signal_table stp_wait_signal_table = {
	.wait_write = &stp_wait_for_write,
	.signal_write = &stp_signal_write,
	.wait_read = &stp_wait_for_read,
	.signal_read = &stp_signal_read,
	.wait_for_device_ready = &stp_wait_for_device_ready,
	.signal_device_ready = &stp_signal_device_ready,
	.set_ready_for_transaction = &stp_set_ready_for_transaction,
	.wait_for_data = &stp_wait_for_data,
	.signal_data = &stp_signal_data,
	.wait_fsync = &stp_wait_fsync,
	.signal_fsync = &stp_signal_fsync,
	.reset_fsync = &stp_reset_fsync,
	.wait_open = &stp_wait_open,
	.signal_open = &stp_signal_open,
};

#define STP_TRANSACTION_COUNT 1
#define STP_DEBUG_HEADER_LEN 8
static int stp_send_receive_data(uint8_t *send_buffer, uint8_t *receive_buffer,
				 unsigned int len_buffer)
{
	int rval;
#if defined(STP_DEBUG_MSG_HEADERS) && (STP_DEBUG_MSG_HEADERS == 1)
	char debug_buffer[STP_DEBUG_BUFFER_LEN];
	int pos = 0;
#endif

	struct spi_transfer xfer = {
		.tx_buf = send_buffer,
		.rx_buf = receive_buffer,
		.len = len_buffer,
	};

#if defined(STP_DEBUG_MSG_HEADERS) && (STP_DEBUG_MSG_HEADERS == 1)
	memset(debug_buffer, 0, STP_DEBUG_BUFFER_LEN);
	pos = snprintf(debug_buffer, STP_DEBUG_BUFFER_LEN, "%s ", "send");

	for (int i = 0; i < STP_DEBUG_HEADER_LEN; i++)
		pos += snprintf(&debug_buffer[pos], STP_DEBUG_BUFFER_LEN - pos,
				"%d ", send_buffer[i]);
	STP_DRV_LOG_INFO("%s", debug_buffer);
#endif

	rval = spi_sync_transfer(_stp_driver_data->spi, &xfer,
				 STP_TRANSACTION_COUNT);

#if defined(STP_DEBUG_MSG_HEADERS) && (STP_DEBUG_MSG_HEADERS == 1)
	pos = snprintf(debug_buffer, STP_DEBUG_BUFFER_LEN, "%s ", "recv");
	for (int i = 0; i < STP_DEBUG_HEADER_LEN; i++)
		pos += snprintf(&debug_buffer[pos], STP_DEBUG_BUFFER_LEN - pos,
				"%d ", receive_buffer[i]);
	STP_DRV_LOG_INFO("%s", debug_buffer);
#endif

	return rval;
}

static struct stp_controller_transport_table stp_transport_table = {
	.send_receive_data = &stp_send_receive_data,
};

static int stp_get_channel_data(struct device_node *const np,
				struct stp_channel_data *const data)
{
	if (of_property_read_u32(np, "channel", &data->channel) < 0) {
		STP_DRV_LOG_ERR("no channel");
		return -ENOENT;
	}

	if (of_property_read_u32(np, "tx_buffer_size", &data->tx_len_bytes) < 0) {
		STP_DRV_LOG_ERR("no tx_buffer_size");
		return -ENOENT;
	}

	if (of_property_read_u32(np, "rx_buffer_size", &data->rx_len_bytes) < 0) {
		STP_DRV_LOG_ERR("no rx_buffer_size");
		return -ENOENT;
	}

	if (of_property_read_u32(np, "priority", &data->priority) < 0) {
		STP_DRV_LOG_ERR("no priority");
		return -ENOENT;
	}

	return 0;
}

static int spi_stp_probe(struct spi_device *spi)
{
	struct stp_channel_data channel_data;
	struct device_node *np;
	int rval;

	if (_stp_driver_data) {
		STP_DRV_LOG_ERR("already initialized");
		return -EEXIST;
	}

	// Other drivers may change the mode so explicitly set it here
	spi->mode = SPI_CPHA;

	_stp_driver_data =
		devm_kzalloc(&spi->dev, sizeof(*_stp_driver_data), GFP_KERNEL);
	if (IS_ERR(_stp_driver_data))
		return -ENOMEM;

	if (stp_init_gpio(spi->dev.of_node, &_stp_driver_data->gpio_data) !=
	    0) {
		rval = -ENODEV;
		goto exit_device_error;
	}

	if (stp_create_device(&spi->dev) != 0) {
		rval = -ENODEV;
		goto exit_device_error;
	}
	np = spi->dev.of_node;
	for_each_node_by_name(np, "channel") {
		rval = stp_get_channel_data(np, &channel_data);
		if (rval != 0) {
			STP_DRV_LOG_ERR("no channel data");
			rval = -ENOENT;
			goto exit_error;
		}

		rval = stp_create_channel(&channel_data);
		if (rval != 0) {
			STP_DRV_LOG_ERR("create channel failed");
			stp_remove_device(&spi->dev);
			goto exit_error;
		}
	}

	device_create_file(&spi->dev, &dev_attr_stp_driver_stats);
	device_create_file(&spi->dev, &dev_attr_stp_connection_state);
	device_create_file(&spi->dev, &dev_attr_stp_log_channel_data);

	_stp_driver_data->spi = spi;
	init_waitqueue_head(&_stp_driver_data->device_ready_q);
	init_completion(&_stp_driver_data->data_ready);
	init_completion(&_stp_driver_data->stp_thread_complete);

	atomic_set(&_stp_driver_data->stats.data_irq_count, 0);
	atomic_set(&_stp_driver_data->stats.device_ready_irq_count, 0);

	_stp_driver_data->controller_rx_buffer = devm_kzalloc(&spi->dev, STP_TOTAL_DATA_SIZE, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(_stp_driver_data->controller_rx_buffer)) {
		rval = -ENOMEM;
		goto exit_error;
	}

	_stp_driver_data->controller_tx_buffer = devm_kzalloc(&spi->dev, STP_TOTAL_DATA_SIZE, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(_stp_driver_data->controller_tx_buffer)) {
		rval = -ENOMEM;
		goto exit_error;
	}

	struct stp_controller_init_t controller_init = {
		.transport = &stp_transport_table,
		.handshake = &stp_handshake_table,
		.wait_signal = &stp_wait_signal_table,
		.rx_buffer = _stp_driver_data->controller_rx_buffer,
		.tx_buffer = _stp_driver_data->controller_tx_buffer,
	};

	rval = STP_ERR_VAL(stp_controller_init(&controller_init));
	if (STP_IS_ERR(rval)) {
		STP_DRV_LOG_ERR("error initializing controller");
		goto exit_error;
	}

	atomic_set(&_stp_driver_data->stop_thread, 0);
	_stp_driver_data->stp_thread =
		kthread_run(stp_thread, NULL, "STP thread");

	if (IS_ERR(_stp_driver_data->stp_thread)) {
		STP_DRV_LOG_ERR("thread can't start");
		rval = -ENOENT;
		goto exit_error;
	}

	// After everything is set up, enable the IRQs. If we do this early,
	// we may start trying to execute transactions before we are initialized.
	if (stp_config_gpio_irq(&spi->dev, &_stp_driver_data->gpio_data,
				&stp_irq_mcu_has_data,
				&stp_irq_mcu_ready) != 0) {
		rval = -ENODEV;
		STP_DRV_LOG_ERR("gpio irq config failure");
		goto exit_error;
	}

	return 0;

exit_error:
	stp_remove_device(&spi->dev);

// fallthrough
exit_device_error:
	devm_kfree(&spi->dev, _stp_driver_data->controller_rx_buffer);
	devm_kfree(&spi->dev, _stp_driver_data->controller_tx_buffer);
	devm_kfree(&spi->dev, _stp_driver_data);
	_stp_driver_data = NULL;
	return rval;
}

static int spi_stp_remove(struct spi_device *spi)
{
	// Release the GPIO first to prevent IRQs from coming in during
	// teardown
	stp_release_gpio_irq(&spi->dev, &_stp_driver_data->gpio_data);

	// Flag the thread stop and signal the device
	// wait notification to exit the transaction thread
	atomic_set(&_stp_driver_data->stop_thread, 1);
	stp_signal_device_ready();
	stp_controller_signal_data();

	wait_for_completion(&_stp_driver_data->stp_thread_complete);

	if (STP_IS_ERR(stp_controller_deinit()))
		STP_DRV_LOG_ERR("failed controller deinit, continuing");

	stp_remove_device(&spi->dev);
	device_remove_file(&spi->dev, &dev_attr_stp_driver_stats);
	device_remove_file(&spi->dev, &dev_attr_stp_connection_state);
	device_remove_file(&spi->dev, &dev_attr_stp_log_channel_data);

	devm_kfree(&spi->dev, _stp_driver_data->controller_rx_buffer);
	devm_kfree(&spi->dev, _stp_driver_data->controller_tx_buffer);
	devm_kfree(&spi->dev, _stp_driver_data);
	_stp_driver_data = NULL;
	STP_DRV_LOG_INFO("Device removed");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int spi_stp_suspend(struct device *dev)
{
	return 0;
}

static int spi_stp_resume(struct device *dev)
{
	return 0;
}
#else
#define spi_stp_suspend NULL
#define spi_stp_resume NULL
#endif

static const struct dev_pm_ops spi_stp_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	spi_stp_suspend, spi_stp_resume) };

static const struct of_device_id spi_stp_of_match[] = {
	{ .compatible = "stella,spi-stp" },
	{ .compatible = "meta,spi-stp" },
	{}
};

static struct spi_driver spi_stp_driver = {
	.driver = {
			.name = "spi_stp_driver",
			.owner = THIS_MODULE,
			.of_match_table = spi_stp_of_match,
			.pm = &spi_stp_pm_ops,
		},
	.probe = spi_stp_probe,
	.remove = spi_stp_remove,
};

static int __init spi_stp_driver_init(void)
{
	int ret;

	ret = spi_register_driver(&spi_stp_driver);

	return ret;
}

static void __exit spi_stp_driver_exit(void)
{
	STP_DRV_LOG_ERR("spi_stp_driver_exit");

	spi_unregister_driver(&spi_stp_driver);
}

module_init(spi_stp_driver_init);
module_exit(spi_stp_driver_exit);

MODULE_DESCRIPTION("SPI - Synchronized Transport Protocol");
MODULE_LICENSE("GPL v2");
