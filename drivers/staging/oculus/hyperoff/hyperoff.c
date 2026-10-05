#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#define HYPEROFF_DRV_LOG_ERR(m, ...) \
	pr_err("[hyperoff] (%s:%d) " m "\n", __func__, __LINE__, ##__VA_ARGS__)

#define EVENT_DEVICE "hoffevent"
#define EVENT_DEVICE_CLASS "hoffevent"
#define MAX_TRANSMIT_SIZE 1024
#define INCOMING_EVENT_BUFFER_SIZE 1 << 20
#define TRANSACTION_TIMEOUT_MS 2000

typedef struct {
	struct spi_device *spi;
	int write_result;
	atomic_t pending_write;

	uint8_t *tx_buffer;
	uint8_t *rx_buffer;

	uint8_t incoming_event_buffer[INCOMING_EVENT_BUFFER_SIZE];
	atomic_t incoming_event_length;

	int event_chr_major;
	struct class *event_chr_class;
	struct device *event_chr_device;

	unsigned int soc_has_data_pin;
	unsigned int mcu_request_transaction_pin;

	struct timer_list event_send_timeout;
} hyperoff_driver_data_t;

static hyperoff_driver_data_t hyperoff_driver_data = { 0 };
static DECLARE_WAIT_QUEUE_HEAD(hyperoff_event_read);
static DECLARE_WAIT_QUEUE_HEAD(hyperoff_event_write);
static DEFINE_MUTEX(spi_transaction_mutex);
static struct work_struct timeout_work;

static void do_spi_transaction()
{
	struct spi_message message;
	uint32_t rx_len;
	struct spi_transfer transfer = {
		.tx_buf = hyperoff_driver_data.tx_buffer,
		.rx_buf = hyperoff_driver_data.rx_buffer,
		.len = MAX_TRANSMIT_SIZE
	};

	mutex_lock_interruptible(&spi_transaction_mutex);

	// we may not have requested this transaction, but if we did, we don't need it anymore
	gpio_set_value(hyperoff_driver_data.soc_has_data_pin, 0);

	// perform the SPI transfer
	spi_message_init(&message);
	spi_message_add_tail(&transfer, &message);
	if (spi_sync(hyperoff_driver_data.spi, &message)) {
		// if we have a pending write, we need to notify the writer that this failed
		if (atomic_read(&hyperoff_driver_data.pending_write) > 0) {
			memset(hyperoff_driver_data.tx_buffer, 0,
			       MAX_TRANSMIT_SIZE);
			hyperoff_driver_data.write_result = -EIO;
			atomic_set(&hyperoff_driver_data.pending_write, 0);
			wake_up_interruptible(&hyperoff_event_write);
		}

		mutex_unlock(&spi_transaction_mutex);
		HYPEROFF_DRV_LOG_ERR("SPI transfer failed");
		return;
	}

	// notify any pending writers that the write succeeded
	if (atomic_read(&hyperoff_driver_data.pending_write) > 0) {
		memset(hyperoff_driver_data.tx_buffer, 0, MAX_TRANSMIT_SIZE);
		hyperoff_driver_data.write_result = 0;
		atomic_set(&hyperoff_driver_data.pending_write, 0);
		wake_up_interruptible(&hyperoff_event_write);
	}

	// if we have any incoming data from the MCU, buffer it if we have space
	// each event is prefixed with a payload length, we need only buffer the valid region of the
	// received event
	rx_len = ((uint32_t *)hyperoff_driver_data.rx_buffer)[0];
	if (rx_len > 0) {
		if (rx_len + sizeof(uint32_t) +
			    atomic_read(&hyperoff_driver_data
						 .incoming_event_length) >=
		    INCOMING_EVENT_BUFFER_SIZE) {
			HYPEROFF_DRV_LOG_ERR(
				"ignored incoming event due to buffer overrun");
		} else {
			memcpy(hyperoff_driver_data.incoming_event_buffer +
				       atomic_read(
					       &hyperoff_driver_data
							.incoming_event_length),
			       hyperoff_driver_data.rx_buffer,
			       rx_len + sizeof(uint32_t));
			atomic_add(rx_len + sizeof(uint32_t),
				   &hyperoff_driver_data.incoming_event_length);
			wake_up_interruptible(&hyperoff_event_read);
		}
	}

	mutex_unlock(&spi_transaction_mutex);
}

static irqreturn_t mcu_prepared_transaction_irq_thread(int irq, void *ptr)
{
	do_spi_transaction();
	return IRQ_HANDLED;
}

static irqreturn_t mcu_prepared_transaction_irq_handler(int irq, void *ptr)
{
	// MCU either acknowledged or requested a transfer; in either case, wake our IRQ thread to initiate
	// a SPI transfer
	return IRQ_WAKE_THREAD;
}

static void mcu_response_timeout_work(struct work_struct *work)
{
	mutex_lock_interruptible(&spi_transaction_mutex);

	gpio_set_value(hyperoff_driver_data.soc_has_data_pin, 0);
	hyperoff_driver_data.write_result = -ETIMEDOUT;
	atomic_set(&hyperoff_driver_data.pending_write, 0);
	memset(hyperoff_driver_data.tx_buffer, 0, MAX_TRANSMIT_SIZE);

	mutex_unlock(&spi_transaction_mutex);

	wake_up_interruptible(&hyperoff_event_write);
}

// We raise soc_has_data, then wait for mcu_request_transaction - this timeout protects against
// an unresponsive MCU, aborting the event write attempt if it doesn't respond. A
static void mcu_response_timeout(struct timer_list *t)
{
	// spi_transaction_mutex can be held for awhile, so we'll handle this timeout expiration in a work queue
	schedule_work(&timeout_work);
}

// We have one output pin: soc-has-data, which is raised when n blocking event write is pending on our
// character device. This tells the MCU to request a transaction.
// We have one input pin: mcu-request-transaction, which will signal us when the MCU either wants to send
// us an event, or is acknowleding our request for a transaction with soc-has-data. In either case, when
// this pin is raised we should complete a SPI transaction.
static int init_pins(struct device *dev)
{
	// configure soc-has-data
	hyperoff_driver_data.soc_has_data_pin =
		of_get_named_gpio(dev->of_node, "soc-has-data", 0);
	if (gpio_direction_output(hyperoff_driver_data.soc_has_data_pin, 0)) {
		HYPEROFF_DRV_LOG_ERR("soc-has-data is misconfigured");
		return -1;
	}

	// configure mcu-request-transaction
	hyperoff_driver_data.mcu_request_transaction_pin =
		of_get_named_gpio(dev->of_node, "mcu-request-transaction", 0);
	if (gpio_direction_input(
		    hyperoff_driver_data.mcu_request_transaction_pin)) {
		HYPEROFF_DRV_LOG_ERR(
			"mcu-request-transaction is misconfigured");
		return -1;
	}

	if (devm_gpio_request(dev,
			      hyperoff_driver_data.mcu_request_transaction_pin,
			      "mcu-request-transaction")) {
		HYPEROFF_DRV_LOG_ERR("gpio request failure");
		return -1;
	}

	return 0;
}

static int enable_mcu_transaction_irq(struct device *dev)
{
	int irq_number;

	irq_number =
		gpio_to_irq(hyperoff_driver_data.mcu_request_transaction_pin);
	if (devm_request_threaded_irq(
		    dev, irq_number, mcu_prepared_transaction_irq_handler,
		    mcu_prepared_transaction_irq_thread, IRQF_TRIGGER_RISING,
		    "mcu-request-transaction", NULL)) {
		HYPEROFF_DRV_LOG_ERR("irq request failure");
		return -1;
	}

	if (enable_irq_wake(irq_number)) {
		HYPEROFF_DRV_LOG_ERR("failed to set IRQ wake for `%d`",
				     irq_number);
		return -1;
	}

	return 0;
}

static int disable_mcu_transaction_irq()
{
	int irq_number;

	irq_number =
		gpio_to_irq(hyperoff_driver_data.mcu_request_transaction_pin);

	if (disable_irq_wake(irq_number)) {
		HYPEROFF_DRV_LOG_ERR("failed to disable IRQ wake for `%d`",
				     irq_number);
	}
	disable_irq(irq_number);

	return 0;
}

// copy as much data as possible from our internal event buffer to the given user buffer
static ssize_t copy_event(char *buf, size_t size)
{
	size_t incoming_event_length = (size_t)atomic_read(
		&hyperoff_driver_data.incoming_event_length);
	size_t to_copy = min(incoming_event_length, size);
	if (copy_to_user(buf, hyperoff_driver_data.incoming_event_buffer,
			 to_copy)) {
		HYPEROFF_DRV_LOG_ERR("failed to copy to user buffer");
		return -EFAULT;
	}

	// compact buffer if we didn't consume the whole thing
	if (incoming_event_length > to_copy) {
		memmove(hyperoff_driver_data.incoming_event_buffer,
			hyperoff_driver_data.incoming_event_buffer + to_copy,
			incoming_event_length - to_copy);
	}
	atomic_sub(to_copy, &hyperoff_driver_data.incoming_event_length);

	return to_copy;
}

static ssize_t event_read(struct file *filep, char *buf, size_t size,
			  loff_t *offset)
{
	ssize_t result;

	// (maybe) block until we have at least some data to return to our reader
	wait_event_interruptible(
		hyperoff_event_read,
		atomic_read(&hyperoff_driver_data.incoming_event_length) > 0);

	// flush as much of our buffered data as we can to the user buffer
	mutex_lock_interruptible(&spi_transaction_mutex);
	result = copy_event(buf, size);
	mutex_unlock(&spi_transaction_mutex);
	return result;
}

static ssize_t event_write(struct file *filep, const char *buf, size_t len,
			   loff_t *offset)
{
	if (len > MAX_TRANSMIT_SIZE - sizeof(uint32_t)) {
		HYPEROFF_DRV_LOG_ERR(
			"user buffer larger than TX buffer, cannot send");
		return -EFAULT;
	}

	mutex_lock_interruptible(&spi_transaction_mutex);

	// reset our state, then prepare our tx buffer
	hyperoff_driver_data.write_result = 0;
	memset(hyperoff_driver_data.tx_buffer, 0, MAX_TRANSMIT_SIZE);
	((uint32_t *)hyperoff_driver_data.tx_buffer)[0] = len;

	if (copy_from_user(hyperoff_driver_data.tx_buffer + sizeof(uint32_t),
			   buf, len)) {
		HYPEROFF_DRV_LOG_ERR("failed to copy user buffer");
		return -EFAULT;
	}

	// signal the MCU that we need a transaction to transfer the event, then wait for the write
	// to finish (either via the SPI transfer, or timeout)
	gpio_set_value(hyperoff_driver_data.soc_has_data_pin, 1);
	mod_timer(&hyperoff_driver_data.event_send_timeout,
		  jiffies + msecs_to_jiffies(TRANSACTION_TIMEOUT_MS));

	atomic_set(&hyperoff_driver_data.pending_write, 1);
	mutex_unlock(&spi_transaction_mutex);

	wait_event_interruptible(
		hyperoff_event_write,
		atomic_read(&hyperoff_driver_data.pending_write) == 0);

	if (hyperoff_driver_data.write_result < 0) {
		return hyperoff_driver_data.write_result;
	}
	return len;
}

static struct file_operations event_device_ops = {
	.read = event_read,
	.write = event_write,
};

// We register /dev/hoffevent, which can be written to to send an event to the MCU, or
// read from to receive an MCU event. Writes must be atomic: a single call to write with
// a buffer containing exactly the full event payload to transfer via a SPI transaction.
// Reads are coalesced into a small buffer, but writes are blocking.
static int register_event_device()
{
	int err;

	hyperoff_driver_data.event_chr_major =
		register_chrdev(0, EVENT_DEVICE, &event_device_ops);
	if (hyperoff_driver_data.event_chr_major < 0) {
		HYPEROFF_DRV_LOG_ERR(
			"failed to register hyperoff_event chrdev");
		return hyperoff_driver_data.event_chr_major;
	}

	hyperoff_driver_data.event_chr_class =
		class_create(THIS_MODULE, EVENT_DEVICE_CLASS);
	if (IS_ERR(hyperoff_driver_data.event_chr_class)) {
		err = PTR_ERR(hyperoff_driver_data.event_chr_class);
		HYPEROFF_DRV_LOG_ERR("failed to register hyperoff_event class");
		goto error_chrdev_registered;
	}

	hyperoff_driver_data.event_chr_device =
		device_create(hyperoff_driver_data.event_chr_class, NULL,
			      MKDEV(hyperoff_driver_data.event_chr_major, 0),
			      NULL, EVENT_DEVICE);
	if (IS_ERR(hyperoff_driver_data.event_chr_device)) {
		err = PTR_ERR(hyperoff_driver_data.event_chr_device);
		HYPEROFF_DRV_LOG_ERR("failed to create hyperoff_event device");
		goto error_class_created;
	}

	HYPEROFF_DRV_LOG_ERR("hyperoff_event device registered successfully");
	return 0;

error_class_created:
	class_destroy(hyperoff_driver_data.event_chr_class);
error_chrdev_registered:
	unregister_chrdev(hyperoff_driver_data.event_chr_major, EVENT_DEVICE);
	return err;
}

static void unregister_event_device()
{
	device_destroy(hyperoff_driver_data.event_chr_class,
		       MKDEV(hyperoff_driver_data.event_chr_major, 0));
	class_unregister(hyperoff_driver_data.event_chr_class);
	class_destroy(hyperoff_driver_data.event_chr_class);
	unregister_chrdev(hyperoff_driver_data.event_chr_major, EVENT_DEVICE);
}

static int hyperoff_probe(struct spi_device *spi)
{
	int ret = 0;

	HYPEROFF_DRV_LOG_ERR("hyperoff driver initializing");

	ret = init_pins(&spi->dev);
	if (ret) {
		return ret;
	}

	ret = register_event_device();
	if (ret) {
		return ret;
	}

	hyperoff_driver_data.tx_buffer = devm_kzalloc(
		&spi->dev, MAX_TRANSMIT_SIZE, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(hyperoff_driver_data.tx_buffer)) {
		HYPEROFF_DRV_LOG_ERR("Failed to allocate controller tx buffer");
		unregister_event_device();
		return -ENOMEM;
	}

	hyperoff_driver_data.rx_buffer = devm_kzalloc(
		&spi->dev, MAX_TRANSMIT_SIZE, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(hyperoff_driver_data.rx_buffer)) {
		HYPEROFF_DRV_LOG_ERR("Failed to allocate controller rx buffer");
		kfree(hyperoff_driver_data.tx_buffer);
		unregister_event_device();
		return -ENOMEM;
	}

	hyperoff_driver_data.spi = spi;

	INIT_WORK(&timeout_work, mcu_response_timeout_work);

	timer_setup(&hyperoff_driver_data.event_send_timeout,
		    mcu_response_timeout, 0);

	ret = enable_mcu_transaction_irq(&spi->dev);
	if (ret) {
		HYPEROFF_DRV_LOG_ERR("Failed to enable transaction IRQ");
		kfree(hyperoff_driver_data.tx_buffer);
		kfree(hyperoff_driver_data.rx_buffer);
		unregister_event_device();
		return ret;
	}

	if (gpio_get_value(hyperoff_driver_data.mcu_request_transaction_pin) !=
	    0) {
		do_spi_transaction();
	}

	return ret;
}

static int hyperoff_remove(struct spi_device *spi)
{
	disable_mcu_transaction_irq();

	del_timer(&hyperoff_driver_data.event_send_timeout);
	flush_work(&timeout_work);
	unregister_event_device();

	kfree(hyperoff_driver_data.tx_buffer);
	kfree(hyperoff_driver_data.rx_buffer);

	memset(&hyperoff_driver_data, 0, sizeof(hyperoff_driver_data));
	return 0;
}

static const struct of_device_id hyperoff_of_match[] = {
	{ .compatible = "meta,hyperoff" },
	{}
};

static const struct dev_pm_ops hyperoff_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	NULL, NULL) };

static struct spi_driver hyperoff_driver = {
	.driver = {
			.name = "hyperoff",
			.owner = THIS_MODULE,
			.of_match_table = hyperoff_of_match,
			.pm = &hyperoff_pm_ops,
		},
	.probe = hyperoff_probe,
	.remove = hyperoff_remove,
};

static int __init hyperoff_driver_init(void)
{
	return spi_register_driver(&hyperoff_driver);
}

static void __exit hyperoff_driver_exit(void)
{
	HYPEROFF_DRV_LOG_ERR("hyperoff_driver_exit");
	spi_unregister_driver(&hyperoff_driver);
}

module_init(hyperoff_driver_init);
module_exit(hyperoff_driver_exit);

MODULE_DESCRIPTION("Hyperoff SPI");
MODULE_LICENSE("GPL v2");
