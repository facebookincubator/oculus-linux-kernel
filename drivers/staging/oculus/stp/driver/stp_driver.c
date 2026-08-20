// SPDX-License-Identifier: GPL-2.0
#include <asm-generic/errno-base.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/compiler.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvisibility"
#include <linux/timekeeping.h>
#pragma clang diagnostic pop
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <uapi/linux/sched/types.h>
#endif

#include <common/stp_device_logging.h>
#include <common/stp_error_mapping.h>
#include <device/stp_device.h>
#include <device/stp_raw_device.h>
#include <driver/stp_driver_data.h>
#include <driver/stp_gpio.h>
#include <stp/controller/stp_controller.h>
#include <stp/controller/stp_controller_common.h>

#include <device/stp_channel_events.h>
#include "stp_driver.h"
#include <emulation/stp_emulation.h>
#include <linux/rt600-ctrl.h>

#define DEVICE_WAKE_TIME_MS 1000

static void wdt_work_func(struct work_struct* unused);
static DECLARE_DELAYED_WORK(wdt_work, wdt_work_func);
#define STP_WDT_TIMEOUT_MS 60000
static DEFINE_MUTEX(wdt_mutex);

// Handle that stores the persistent driver information
static struct spi_stp_driver_data *_stp_driver_data;
static void stp_set_soc_has_data(bool value);
static bool stp_mcu_request_transaction(void);

static BLOCKING_NOTIFIER_HEAD(stp_device_state_nb);
// TODO: The time channel should be driver configuration not a constant.
#define TIME_CHANNEL 24

#define STP_PRIORITY_THREAD 50
static int stp_thread(void *data)
{
	int rval = 0;
	struct sched_param param;

	param.sched_priority = STP_PRIORITY_THREAD;
	if (sched_setscheduler(current, SCHED_FIFO, &param) == -1)
		STP_DRV_LOG_ERR("error setting priority");

	while (!stp_controller_get_stop_thread()) {
		rval = STP_ERR_VAL(stp_controller_transaction_thread());
		if (STP_ERR_VAL(rval) && STP_ERR_VAL(rval) != -ERESTARTSYS) {
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

static ssize_t txdata_stuck_counter_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t rval;

	rval = scnprintf(
		buf, PAGE_SIZE,
		"%d\n",
		atomic_read(&_stp_driver_data->txdata_stuck_counter));
	return rval;
}
static DEVICE_ATTR_RO(txdata_stuck_counter);

static ssize_t wdt_bark_counter_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t rval;

	rval = scnprintf(
		buf, PAGE_SIZE,
		"%d\n",
		atomic_read(&_stp_driver_data->wdt_bark_counter));
	return rval;
}
static DEVICE_ATTR_RO(wdt_bark_counter);

static ssize_t T193790187_dump_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	const int diag_ch_n = 10;
	ssize_t rval = 0;
	int ret;
	uint32_t rx_data = 0;
	uint32_t tx_data = 0;

	ret = STP_ERR_VAL(stp_controller_get_channel_attribute(diag_ch_n, STP_RX_FILLED, &rx_data));
	if (STP_IS_ERR(ret)) {
		STP_DRV_LOG_ERR("Error getting attribute STP_RX_FILLED");
		return rval;
	}
	ret = STP_ERR_VAL(stp_controller_get_channel_attribute(diag_ch_n, STP_TX_DATA, &tx_data));
	if (STP_IS_ERR(ret)) {
		STP_DRV_LOG_ERR("Error getting attribute STP_TX_DATA");
		return rval;
	}
	rval = scnprintf(
		buf, PAGE_SIZE,
		"gpio: soc_has_data:%d, mcu_req_transaction:%d\n"
		"ch%d: rx_filled=%u, tx_queued=%u\n",
		gpio_get_value(_stp_driver_data->gpio_data.controller_has_data),
		gpio_get_value(_stp_driver_data->gpio_data.device_request_transaction),
		diag_ch_n, rx_data, tx_data
		);
	return rval;
}
static DEVICE_ATTR_RO(T193790187_dump);

static ssize_t stp_connection_state_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	uint32_t ret = 0;
	ssize_t rval = 0;
	ssize_t char_count = 0;
	uint32_t synced = 0;

	ret = STP_ERR_VAL(stp_controller_get_attribute(STP_ATTRIB_SYNCED, &synced));
	if (STP_IS_ERR(ret)) {
		STP_DRV_LOG_ERR("Error getting attribute");
		return rval;
	}

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

		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(i, STP_ATTRIB_VALID_SESSION, &valid));
		if (STP_IS_ERR(ret)) {
			STP_DRV_LOG_ERR("Error getting attribute STP_ATTRIB_VALID_SESSION");
			return rval;
		}

		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(i, STP_ATTRIB_CONTROLLER_CONNECTED, &controller_connected));
		if (STP_IS_ERR(ret)) {
			STP_DRV_LOG_ERR("Error getting attribute STP_ATTRIB_CONTROLLER_CONNECTED");
			return rval;
		}

		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(i, STP_ATTRIB_DEVICE_CONNECTED, &device_connected));
		if (STP_IS_ERR(ret)) {
			STP_DRV_LOG_ERR("Error getting attribute STP_ATTRIB_DEVICE_CONNECTED");
			return rval;
		}

		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(i, STP_RX_FILLED, &rx_data));
		if (STP_IS_ERR(ret)) {
			STP_DRV_LOG_ERR("Error getting attribute STP_RX_FILLED");
			return rval;
		}

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
	uint32_t ret = 0;

	char_count = scnprintf(
			buf + rval, PAGE_SIZE - rval,
			"Channels with log enabled:\n"
			);
	rval += char_count;

	for (uint8_t i = 0; i < STP_TOTAL_NUM_CHANNELS; i++) {
		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(i, STP_CONTROLLER_ATTRIB_SET_LOG_TX_DATA, &log_tx_value));
		if (STP_IS_ERR(ret)) {
			STP_DRV_LOG_ERR("Error getting attribute STP_CONTROLLER_ATTRIB_SET_LOG_TX_DATA");
			return rval;
		}

		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(i, STP_CONTROLLER_ATTRIB_SET_LOG_RX_DATA, &log_rx_value));
		if (STP_IS_ERR(ret)) {
			STP_DRV_LOG_ERR("Error getting attribute STP_CONTROLLER_ATTRIB_SET_LOG_RX_DATA");
			return rval;
		}

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

static ssize_t stp_spi_owner_switch_store(
				struct device *dev,
				struct device_attribute *attr,
				 const char *buf, size_t len)
{
	int ret;
	bool switch_spi_to_stp = false;
	char spi_owner[8]	= "";
	if (sscanf(buf, "set_spi_owner %7s", spi_owner) != 1) {
		STP_DRV_LOG_ERR("Usage: set_spi_owner {stp | stp_raw}");
		return len;
	}

	if (!strncmp(spi_owner, "stp_raw", 7)) {
		switch_spi_to_stp = false;
	} else if (!strncmp(spi_owner, "stp", 3)) {
		switch_spi_to_stp = true;
	} else {
		STP_DRV_LOG_ERR("Usage: set_spi_owner {stp | stp_raw}");
		return len;
	}

	// switch spi from stp to stp raw
	if (!switch_spi_to_stp) {
		bool service_interrupt = true;
		bool stp_raw_spi_busy = stp_raw_get_spi_busy();
		if (!stp_raw_spi_busy) {
			STP_DRV_LOG_ERR("STP_RAW already owned SPI");
			return len;
		}

		stp_dump_channel_state();

		trace_stp_switch_to_stpraw(0);

		// Prevent PM from issuing suspend which can cause race when parking stp thread
		pm_stay_awake(dev);

		// Set flag to return fops early when spi is taken by stp raw
		stp_set_spi_busy(true);

		// Park stp thread
		stp_controller_signal_suspend();
		ret = kthread_park(_stp_driver_data->stp_thread);
		if (ret) {
			STP_DRV_LOG_ERR("Failed to park STP thread!");
		}

		// Set controller flag to return ongoing controller calls when
		// they hit stp_controller_check_for_rw_errors
		stp_controller_set(STP_ATTRIB_SERVICE_INTERRUPTION, (void *)&service_interrupt);

		// Unblock controller functions that are waiting for read/write
		stp_interrupt_all_channels();

		// Clear buffered data for all channels
		stp_controller_reset_all_channel_buffer();

		// Do not confuse MCU by requesting STP data in raw mode.
		stp_set_soc_has_data(false);
		// There are no longer pending STP data requests, so deactivate the WDT.
		cancel_delayed_work_sync(&wdt_work);

		// Set flag to enable calling stp raw fops
		stp_raw_set_spi_busy(false);

		trace_stp_switch_to_stpraw(1);
		STP_DRV_LOG_ERR("STP RAW now owns SPI!");
	// switch spi from stp raw to stp
	} else {
		bool service_interrupt = false;
		bool stp_spi_busy = stp_get_spi_busy();
		if (!stp_spi_busy) {
			STP_DRV_LOG_ERR("STP already owned SPI");
			return len;
		}

		trace_stp_switch_to_stp(0);

		// Unpark STP thread
		kthread_unpark(_stp_driver_data->stp_thread);

		// Disable accessing all stp raw fops
		stp_raw_set_spi_busy(true);

		// Set the controller flag to resume normal activity
		stp_controller_set(STP_ATTRIB_SERVICE_INTERRUPTION, (void *)&service_interrupt);

		// Prepare controller to do a resync
		stp_controller_request_protocol_resync();

		// Set flag to enable calling stp fops
		stp_set_spi_busy(false);

		// If the mcu request transaction signal is high, we have missed the first gpio interrupt
		if (stp_mcu_request_transaction())
			stp_controller_signal_start_transaction();

		// Allow PM to start the suspend flow
		pm_relax(dev);

		trace_stp_switch_to_stp(1);
		STP_DRV_LOG_ERR("STP now owns SPI!");
	}

	return len;
}
static DEVICE_ATTR_WO(stp_spi_owner_switch);

static ssize_t stp_force_suspend_store(
				struct device *dev,
				struct device_attribute *attr,
				 const char *buf, size_t len)
{
	STP_DRV_LOG_INFO("[WQ] len = %zu", len);
	if (!strncmp(buf, "enable", len - 1)) {
		_stp_driver_data->enable_force_suspend = true;
		disable_irq_wake(_stp_driver_data->gpio_data.device_request_transaction_irq);
		STP_DRV_LOG_INFO("stp_force_suspend: enabled force suspend");
	} else if (!strncmp(buf, "disable", len - 1)) {
		_stp_driver_data->enable_force_suspend = false;
		enable_irq_wake(_stp_driver_data->gpio_data.device_request_transaction_irq);
		STP_DRV_LOG_INFO("stp_force_suspend: disabled force suspend");
	} else {
		STP_DRV_LOG_ERR("stp_force_suspend: Usage: {enable | disable}");
	}

	return len;
}
static DEVICE_ATTR_WO(stp_force_suspend);

static ssize_t stp_channel_wakeups_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t rval = 0;
	int i;

	for (i = 0; i <= STP_TOTAL_NUM_CHANNELS; i++) {
		rval += scnprintf(buf + rval, PAGE_SIZE - rval,
				  "%u,", (unsigned int)_stp_driver_data->wakeups[i]);
	}
	rval += scnprintf(buf + rval - 1, PAGE_SIZE - rval, "\n");
	return rval;
}
static DEVICE_ATTR_RO(stp_channel_wakeups);

static bool stp_mcu_request_transaction(void)
{
	int value;

	if (!_stp_driver_data)
		return false;

	value = gpio_get_value(_stp_driver_data->gpio_data.device_request_transaction);
	trace_stp_mcu_request_transaction(value == 0);

	return value == 0;
}

static void stp_set_soc_has_data(bool value)
{
	bool ret;

	if (!_stp_driver_data)
		return;

	if (_stp_driver_data->device_request_transaction_cache == value) {
		// This is already the value set, nothing to do.
		return;
	}

	mutex_lock(&wdt_mutex);

	// The following is either the expected operation or a no-op which will warm the cache.
	trace_stp_set_soc_has_data(value);
	gpio_set_value(_stp_driver_data->gpio_data.controller_has_data, true);
	if (value) {
		/* Arm the WDT before toggling the SoC request. Do not leave
		 * a race window in which the MCU could trigger
		 * stp_irq_mcu_request_transaction and stop WDT.
		 */
		ret = schedule_delayed_work(&wdt_work, msecs_to_jiffies(STP_WDT_TIMEOUT_MS));
		trace_stp_schedule_wdt_work(ret);

#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
		// Warm the cache for the time collection function. The accuracy of the timestamping
		// method derives in very large part to knowing as exactly as possible when the
		// GPIO being set below changes. Calling these functions beforehand does the work
		// of bringing their respective code and data into L1 cache. For millisecond timing,
		// this would be overkill, but for microsecond timing where the cost for DRAM fetches
		// can takes hundreds of nanoseconds, the stack up of delays matters.
		arch_timer_read_counter();

		// Actually perform the transition and get the time stamp.
		const uint64_t t0 = arch_timer_read_counter();
#else /* x86_64 and other architectures */
		// For x86_64, use ktime functions for high-precision timestamps
		// Warm the cache
		ktime_get_ns();

		// Actually perform the transition and get the time stamp.
		const uint64_t t0 = ktime_get_ns();
#endif
		gpio_set_value(_stp_driver_data->gpio_data.controller_has_data,
			       false);
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
		const uint64_t t1 = arch_timer_read_counter();
#else /* x86_64 and other architectures */
		const uint64_t t1 = ktime_get_ns();
#endif
		// Write the average to ensure the time stamp is well bounded.
		_stp_driver_data->device_request_transaction_timestamp =
			(t0 + t1) / 2;
	}

	_stp_driver_data->device_request_transaction_cache = value;
	mutex_unlock(&wdt_mutex);
}

static bool stp_get_soc_has_data()
{
	if (!_stp_driver_data)
		return false;

	return _stp_driver_data->device_request_transaction_cache;
}

#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
/** Convert the architecture timer from ticks to nanoseconds. */
static uint64_t arch_timer_to_ns(uint64_t arch_timer)
{
	const uint32_t hz = arch_timer_get_rate();
	uint64_t ns = 1000000000ULL * (arch_timer / hz);
	const uint64_t remainder = (arch_timer % hz);
	ns += (remainder * 1000000000ULL) / hz;

	return ns;
}

/** Returns the last time the stp_set_soc_has_data was called to set the GPIO. */
static uint64_t stp_get_last_has_data_ns(void)
{
	return arch_timer_to_ns(
		_stp_driver_data->device_request_transaction_timestamp);
}

static uint64_t stp_get_ns(void)
{
	return arch_timer_to_ns(arch_timer_read_counter());
}
#else /* x86_64 and other architectures */
/** Returns the last time the stp_set_soc_has_data was called to set the GPIO. */
static uint64_t stp_get_last_has_data_ns(void) {
  return _stp_driver_data->device_request_transaction_timestamp;
}

static uint64_t stp_get_ns(void) {
  return ktime_get_ns();
}
#endif

static ssize_t stp_driver_timestamp_ns_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	uint64_t *buf64 = (uint64_t *)buf;
	*buf64 = stp_get_ns();
	return sizeof(uint64_t);
}
static DEVICE_ATTR_RO(stp_driver_timestamp_ns);

static struct stp_controller_handshake_table stp_handshake_table = {
	.device_request_transaction = &stp_mcu_request_transaction,
	.set_controller_has_data = &stp_set_soc_has_data,
	.get_controller_has_data = &stp_get_soc_has_data,
	.get_controller_last_has_data_ns = &stp_get_last_has_data_ns,
	.stp_controller_get_ns = &stp_get_ns,
};

static irqreturn_t stp_irq_mcu_request_transaction(int irq, void *dev_id)
{
	bool ret;

	pm_wakeup_event(&_stp_driver_data->spi->dev, DEVICE_WAKE_TIME_MS);

	/* Not only we cannot use a _sync variant for canceling delayed work
	 * in interrupt context, but it also does not make sense in our WDT
	 * use case. A WDT bite has already happened, and it is irrelevant
	 * to wait for the handler to finish.
	 *
	 * Also, do no leave the WDT pending during suspend. MCU has confirmed
	 * the transaction request, which is the primary purpose of this WDT.
	 * Suspend/resume takes a bit of time, increasing the likelihood
	 * of spurious WDT bites.
	 */
	ret = cancel_delayed_work(&wdt_work);
	trace_stp_cancel_wdt_work(ret);

	/* If stp_thread is parked, this call will not unpark and wake it up.
	 * But this call will raise the "start_transaction" flag,
	 * which in turn would be checked when stp_thread is
	 * eventually unparked. */
	stp_controller_signal_start_transaction();

	atomic_inc(&_stp_driver_data->stats.data_irq_count);

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

static void stp_signal_event(void)
{
	wake_up_interruptible(&_stp_driver_data->thread_event_queue);
}

static int32_t stp_wait_event(void)
{
	return wait_event_interruptible(_stp_driver_data->thread_event_queue,
					stp_controller_pending_event());
}

static int32_t stp_pause_thread(void)
{
	while (1) {
		if (kthread_should_park()) {
			kthread_parkme();
			break;
		}
		STP_DRV_LOG_ERR_RATE_LIMIT("Spining and waiting for kthread_should_park");
		schedule();
	}

	return 0;
}

// No use on linux because we do park and unpark thread together
static int32_t stp_resume_thread(void)
{
	return 0;
}

static struct stp_controller_wait_signal_table stp_wait_signal_table = {
	.wait_write = &stp_wait_for_write,
	.signal_write = &stp_signal_write,
	.wait_read = &stp_wait_for_read,
	.signal_read = &stp_signal_read,
	.wait_fsync = &stp_wait_fsync,
	.signal_fsync = &stp_signal_fsync,
	.reset_fsync = &stp_reset_fsync,
	.wait_open = &stp_wait_open,
	.signal_open = &stp_signal_open,
	.signal_stp_event = &stp_signal_event,
	.wait_stp_event = &stp_wait_event,
	.pause_thread = &stp_pause_thread,
	.resume_thread = &stp_resume_thread,
};

#define STP_TRANSACTION_COUNT 1
#define STP_DEBUG_HEADER_LEN 8
static int stp_send_receive_data(uint8_t *send_buffer, uint8_t *receive_buffer,
				 unsigned int len_buffer)
{
	int rval;

	struct spi_transfer xfer = {
		.tx_buf = send_buffer,
		.rx_buf = receive_buffer,
		.len = len_buffer,
	};

	print_hex_dump_debug("[STP]: send raw header: ",
			     DUMP_PREFIX_NONE,
			     16, 1,
			     send_buffer, STP_DEBUG_HEADER_LEN,
			     false);
	trace_stp_spi_send_data(send_buffer, len_buffer);

	rval = spi_sync_transfer(_stp_driver_data->spi, &xfer,
				 STP_TRANSACTION_COUNT);

	if (atomic_cmpxchg(&_stp_driver_data->first_transact_after_resume, 1, 0) == 1) {
		int idx = 0;
		char buffer[STP_DEBUG_BUFFER_LEN];
		uint8_t channel = stp_controller_get_packet_channel(receive_buffer, len_buffer);

		if (channel > STP_TOTAL_NUM_CHANNELS)
			channel = STP_TOTAL_NUM_CHANNELS;
		_stp_driver_data->wakeups[channel]++;

		memset(buffer, 0, STP_DEBUG_BUFFER_LEN);
		idx = snprintf(buffer, STP_DEBUG_BUFFER_LEN, "%s ", "1st rx pkt after resume: ");
		for (int i = 0; i < STP_DEBUG_HEADER_LEN; i++)
			idx += snprintf(&buffer[idx], STP_DEBUG_BUFFER_LEN - idx - 1,
					"0x%x ", receive_buffer[i]);
		STP_DRV_LOG_INFO("%s", buffer);
	}

	print_hex_dump_debug("[STP]: recv raw header: ",
			     DUMP_PREFIX_NONE,
			     16, 1,
			     receive_buffer, STP_DEBUG_HEADER_LEN,
			     false);
	trace_stp_spi_recv_data(receive_buffer, len_buffer);

	return rval;
}

static struct stp_controller_transport_table stp_transport_table = {
	.send_receive_data = &stp_send_receive_data,
};

#ifndef STP_EMULATION
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
#endif // ifndef STP_EMULATION

int register_spi_stp_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&stp_device_state_nb, nb);
}
EXPORT_SYMBOL(register_spi_stp_notifier);

void unregister_spi_stp_notifier(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&stp_device_state_nb, nb);
}
EXPORT_SYMBOL(unregister_spi_stp_notifier);

static void stp_controller_callback(int event)
{
	if (event == STP_CONTROLLER_EVENT_INIT)
		blocking_notifier_call_chain(&stp_device_state_nb,
					     SPI_STP_CTRL_INIT, NULL);
}

static int create_stp_channels(struct spi_device *spi)
{
	struct stp_channel_data channel_data;
	int rval = 0;
	if(spi)
	{
#ifdef STP_EMULATION
		rval = stp_emulation_create_channels(&channel_data);
#else
		struct device_node *np;
		np = spi->dev.of_node;
		for_each_node_by_name(np, "channel") {
			rval = stp_get_channel_data(np, &channel_data);
			if (rval != 0) {
				STP_DRV_LOG_ERR("no channel data");
				rval = -ENOENT;
				return rval;
			}

			rval = stp_create_channel(&channel_data);
			if (rval != 0) {
				STP_DRV_LOG_ERR("create channel failed");
				return rval;
			}
		}
#endif // #ifdef STP_EMULATION
	}
	else
	{
		STP_DRV_LOG_ERR("spi device is null");
		rval = -ENOENT;
	}
return rval;
}

static int spi_stp_probe(struct spi_device *spi)
{
	int rval;

	// To be removed: Temporary debug logs
	STP_DRV_LOG_ERR("Probe started");

	if (_stp_driver_data) {
		STP_DRV_LOG_ERR("already initialized");
		return -EEXIST;
	}

	// Other drivers may change the mode so explicitly set it here
	spi->mode = SPI_CPHA;

	_stp_driver_data =
		devm_kzalloc(&spi->dev, sizeof(*_stp_driver_data), GFP_KERNEL);
	if (IS_ERR(_stp_driver_data)) {
		STP_DRV_LOG_ERR("Failed to allocate driver data");
		return -ENOMEM;
	}
#ifdef STP_EMULATION
	if (stp_emulation_init_gpio(&spi->dev, &_stp_driver_data->gpio_data) !=
	    0) {
		STP_DRV_LOG_ERR("Failed to initialize gpio");
		rval = -ENODEV;
		goto exit_device_error;
	}
#else // ifdef STP_EMULATION
	if (stp_init_gpio(spi->dev.of_node, &_stp_driver_data->gpio_data) !=
	    0) {
		STP_DRV_LOG_ERR("Failed to initialize gpio");
		rval = -ENODEV;
		goto exit_device_error;
	}
#endif // ifdef STP_EMULATION

	// Set force suspend to false by default
	_stp_driver_data->enable_force_suspend = false;

	// This reflects the semantic meaning, opposite this GPIO since it's active low.
	_stp_driver_data->device_request_transaction_cache = false;

	_stp_driver_data->device_request_transaction_timestamp = 0;

	if (stp_create_device(&spi->dev) != 0) {
		STP_DRV_LOG_ERR("Failed to create device");
		rval = -ENODEV;
		goto exit_device_error;
	}

	if (device_init_wakeup(&spi->dev, true) != 0) {
		STP_DRV_LOG_ERR("Failed to init wakesource\n");
		rval = -ENODEV;
		goto exit_device_error;
	}

	init_completion(&_stp_driver_data->stp_thread_complete);

	atomic_set(&_stp_driver_data->stats.data_irq_count, 0);
	atomic_set(&_stp_driver_data->stats.device_ready_irq_count, 0);
	atomic_set(&_stp_driver_data->first_transact_after_resume, 0);

	rval = create_stp_channels(spi);
	if (rval != 0) {
		goto exit_channel_error;
	}

	stp_set_spi_busy(false);
	stp_raw_set_spi_busy(true);

	rval = stp_raw_dev_init(spi);
	if (rval != 0) {
		STP_DRV_LOG_ERR("failed to init stp raw device %d", rval);
		goto exit_error;
	}

	device_create_file(&spi->dev, &dev_attr_stp_driver_stats);
	device_create_file(&spi->dev, &dev_attr_stp_connection_state);
	device_create_file(&spi->dev, &dev_attr_stp_log_channel_data);
	device_create_file(&spi->dev, &dev_attr_stp_spi_owner_switch);
	device_create_file(&spi->dev, &dev_attr_txdata_stuck_counter);
	device_create_file(&spi->dev, &dev_attr_wdt_bark_counter);
	device_create_file(&spi->dev, &dev_attr_T193790187_dump);
	device_create_file(&spi->dev, &dev_attr_stp_driver_timestamp_ns);
	device_create_file(&spi->dev, &dev_attr_stp_force_suspend);
	device_create_file(&spi->dev, &dev_attr_stp_channel_wakeups);

	_stp_driver_data->txdata_stuck_counter_attr_node = sysfs_get_dirent(spi->dev.kobj.sd, "txdata_stuck_counter");
	if (!_stp_driver_data->txdata_stuck_counter_attr_node) {
		dev_info(&spi->dev, "failed to get txdata_stuck_counter kernel fs node");
	}

	_stp_driver_data->wdt_bark_counter_attr_node = sysfs_get_dirent(spi->dev.kobj.sd, "wdt_bark_counter");
	if (!_stp_driver_data->wdt_bark_counter_attr_node) {
		dev_info(&spi->dev, "failed to get wdt_bark_counter kernel fs node");
	}

	_stp_driver_data->spi = spi;

	_stp_driver_data->controller_rx_buffer = devm_kzalloc(&spi->dev, STP_TOTAL_DATA_SIZE, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(_stp_driver_data->controller_rx_buffer)) {
		STP_DRV_LOG_ERR("Failed to allocate controller rx buffer");
		rval = -ENOMEM;
		goto exit_error;
	}

	_stp_driver_data->controller_tx_buffer = devm_kzalloc(&spi->dev, STP_TOTAL_DATA_SIZE, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(_stp_driver_data->controller_tx_buffer)) {
		STP_DRV_LOG_ERR("Failed to allocate controller tx buffer");
		rval = -ENOMEM;
		goto exit_error;
	}

	struct stp_controller_init_t controller_init = {
		.transport = &stp_transport_table,
		.handshake = &stp_handshake_table,
		.wait_signal = &stp_wait_signal_table,
		.rx_buffer = _stp_driver_data->controller_rx_buffer,
		.tx_buffer = _stp_driver_data->controller_tx_buffer,
		.time_channel = TIME_CHANNEL,
	};

	rval = STP_ERR_VAL(stp_controller_init(&controller_init));
	if (STP_IS_ERR(rval)) {
		STP_DRV_LOG_ERR("error initializing controller");
		goto exit_error;
	}

	rval = stp_controller_set_callback(stp_controller_callback);
	if (STP_IS_ERR(rval)) {
		STP_DRV_LOG_ERR("error setting controller callback");
		goto exit_error;
	}

	init_waitqueue_head (&_stp_driver_data->thread_event_queue);

	_stp_driver_data->stp_thread =
		kthread_run(stp_thread, NULL, "STP thread");

	if (IS_ERR(_stp_driver_data->stp_thread)) {
		STP_DRV_LOG_ERR("thread can't start");
		rval = -ENOENT;
		goto exit_error;
	}

	if(devm_gpio_request(&spi->dev, _stp_driver_data->gpio_data.controller_has_data, "SOC_has_data")) {
		rval = -ENODEV;
		STP_DRV_LOG_ERR("gpio request failure for controller_has_data");
		goto exit_error;
	}

	if(devm_gpio_request(&spi->dev, _stp_driver_data->gpio_data.device_request_transaction, "MCU_request_transaction")) {
		rval = -ENODEV;
		STP_DRV_LOG_ERR("gpio request failure for device_request_transaction");
		goto exit_error;
	}

	// After everything is set up, enable the IRQs. If we do this early,
	// we may start trying to execute transactions before we are initialized.
	if (stp_config_gpio_irq(&spi->dev, &_stp_driver_data->gpio_data,
				&stp_irq_mcu_request_transaction) != 0) {
		rval = -ENODEV;
		STP_DRV_LOG_ERR("gpio irq config failure");
		goto exit_error;
	}

	// If the mcu request transaction signal is high, we have missed the first gpio interrupt so
	// manually signal
	if (stp_mcu_request_transaction())
		stp_controller_signal_start_transaction();

	blocking_notifier_call_chain(&stp_device_state_nb, SPI_STP_DRV_LOADED, NULL);

	return 0;

exit_error:
	stp_raw_dev_remove();

// fallthrough
exit_channel_error:
	stp_remove_device(&spi->dev);

// fallthrough
exit_device_error:
	devm_kfree(&spi->dev, _stp_driver_data);
	_stp_driver_data = NULL;
	return rval;
}

static int spi_stp_remove(struct spi_device *spi)
{
	bool ret;

	// To be removed: Temporary debug logs
	STP_DRV_LOG_ERR("Removing STP");

	blocking_notifier_call_chain(&stp_device_state_nb, SPI_STP_DRV_REMOVING,
				     NULL);

	// Disable the GPIO first to prevent IRQs from coming in during
	// teardown
	stp_disable_gpio_irq(&spi->dev, &_stp_driver_data->gpio_data);

	// Flag the thread stop and unblock the thread
	// to exit the transaction thread
	stp_controller_signal_stop_thread();
	stp_controller_unset_suspend();
	kthread_unpark(_stp_driver_data->stp_thread);

	wait_for_completion(&_stp_driver_data->stp_thread_complete);

	if (STP_IS_ERR(stp_controller_deinit()))
		STP_DRV_LOG_ERR("failed controller deinit, continuing");

	if (device_init_wakeup(&spi->dev, false) != 0)
		STP_DRV_LOG_ERR("Failed to deinit wakesource\n");

	stp_raw_dev_remove();
	stp_remove_device(&spi->dev);

	ret = cancel_delayed_work_sync(&wdt_work);
	trace_stp_cancel_wdt_work(ret);

	/* Driver removal cannot race with suspend. So the following is safe. */
	sysfs_put(_stp_driver_data->txdata_stuck_counter_attr_node);
	_stp_driver_data->txdata_stuck_counter_attr_node = NULL;
	sysfs_put(_stp_driver_data->wdt_bark_counter_attr_node);
	_stp_driver_data->wdt_bark_counter_attr_node = NULL;

	device_remove_file(&spi->dev, &dev_attr_stp_driver_stats);
	device_remove_file(&spi->dev, &dev_attr_stp_connection_state);
	device_remove_file(&spi->dev, &dev_attr_stp_log_channel_data);
	device_remove_file(&spi->dev, &dev_attr_stp_spi_owner_switch);
	device_remove_file(&spi->dev, &dev_attr_txdata_stuck_counter);
	device_remove_file(&spi->dev, &dev_attr_wdt_bark_counter);
	device_remove_file(&spi->dev, &dev_attr_T193790187_dump);
	device_remove_file(&spi->dev, &dev_attr_stp_driver_timestamp_ns);
	device_remove_file(&spi->dev, &dev_attr_stp_force_suspend);
	device_remove_file(&spi->dev, &dev_attr_stp_channel_wakeups);

	devm_kfree(&spi->dev, _stp_driver_data);
	_stp_driver_data = NULL;
	STP_DRV_LOG_ERR("Device removed");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int spi_stp_suspend(struct device *dev)
{
	int ret;

	if (stp_mcu_request_transaction()) {
		STP_DRV_LOG_ERR("mcu is requesting a transaction, abort suspend");
		goto exit_error;
	}
	if (_stp_driver_data->enable_force_suspend) {
		STP_DRV_LOG_ERR("Force suspend enabled. Reset buffer for all channels");
		stp_controller_reset_all_channel_buffer();
	} else if (stp_controller_has_data_to_send()) {
		atomic_inc(&_stp_driver_data->txdata_stuck_counter);
		struct kernfs_node *node = _stp_driver_data->txdata_stuck_counter_attr_node;
		sysfs_notify_dirent(node);

		STP_DRV_LOG_ERR("STP has data to send, abort suspend");
		goto exit_error;
	}

	stp_controller_signal_suspend();

	//Block and wait for stp thread to park
	ret = kthread_park(_stp_driver_data->stp_thread);
	if (ret) {
		STP_DRV_LOG_ERR("Failed to park STP thread, abort suspend. ret %d", ret);
		goto exit_error;
	}

	return 0;

exit_error:
	return -EBUSY;
}

static int spi_stp_resume(struct device *dev)
{
	atomic_set(&_stp_driver_data->first_transact_after_resume, 1);

	kthread_unpark(_stp_driver_data->stp_thread);

	atomic_set(&_stp_driver_data->txdata_stuck_counter, 0);

	return 0;
}
#else
#define spi_stp_suspend NULL
#define spi_stp_resume NULL
#endif

static void wdt_work_func(struct work_struct *unused)
{
	int ret;
	STP_DRV_LOG_ERR("SPI WDT bark (gpio: soc_has_data:%d, mcu_req_transaction:%d)",
		gpio_get_value(_stp_driver_data->gpio_data.controller_has_data),
		gpio_get_value(_stp_driver_data->gpio_data.device_request_transaction));


	if (!_stp_driver_data)
		return;

	mutex_lock(&wdt_mutex);

	trace_stp_wdt_bark(_stp_driver_data->device_request_transaction_cache);

	/*
	 * We expect device_request_transaction_cache is set to true, which
	 * tells the MCU has pending data.
	 */
	WARN_ON_ONCE(!_stp_driver_data->device_request_transaction_cache);

	/* Trigger remote reset after WDT timeout */
	ret = rt600_trigger_reset();
	if (ret == 0) {
		STP_DRV_LOG_ERR("Remote reset triggered successfully");
	} else if (ret == -EBUSY) {
		STP_DRV_LOG_ERR("Remote reset already in progress");
	} else if (ret == -ENODEV) {
		STP_DRV_LOG_ERR("Remote driver not initialized");
	} else {
		STP_DRV_LOG_ERR("Remote reset failed with error %d", ret);
	}

	atomic_inc(&_stp_driver_data->wdt_bark_counter);
	struct kernfs_node *node = _stp_driver_data->wdt_bark_counter_attr_node;
	if (node)
		sysfs_notify_dirent(node);

	mutex_unlock(&wdt_mutex);
}

#ifdef CONFIG_HIBERNATION
static int spi_stp_freeze(struct device *dev)
{
	STP_DRV_LOG_INFO("Freeze start");
	stp_disable_gpio_irq(dev, &_stp_driver_data->gpio_data);
	free_irq(_stp_driver_data->gpio_data.device_request_transaction_irq, NULL);
	gpio_free(_stp_driver_data->gpio_data.device_request_transaction);
	gpio_free(_stp_driver_data->gpio_data.controller_has_data);
	STP_DRV_LOG_INFO("Freeze end");
	return 0;
}

static int spi_stp_restore(struct device *dev)
{
	int rval = 0;
	STP_DRV_LOG_INFO("Restore start");
	stp_controller_request_protocol_resync();

	if(stp_gpio_set_direction(&_stp_driver_data->gpio_data)) {
		STP_DRV_LOG_ERR("Failed to set gpio direction");
		return -EINVAL;
	}

	if(devm_gpio_request(dev, _stp_driver_data->gpio_data.device_request_transaction, "MCU_request_transaction")) {
		STP_DRV_LOG_ERR("GPIO request failure for device_request_transaction");
		return -ENODEV;
	}

	if (stp_config_gpio_irq(dev, &_stp_driver_data->gpio_data,
				&stp_irq_mcu_request_transaction) != 0) {
		STP_DRV_LOG_ERR("Failed to configure gpio irq for device_request_transaction");
		return -EINVAL;
	}

	stp_controller_signal_start_transaction();

	STP_DRV_LOG_INFO("Restore end");
	return rval;
}

static int spi_stp_thaw(struct device *dev)
{
	int rval = 0;
	STP_DRV_LOG_INFO("Thaw start");
	rval = spi_stp_restore(dev);
	STP_DRV_LOG_INFO("Thaw end");
	return rval;
}

#else
#define spi_stp_freeze NULL
#define spi_stp_restore NULL
#endif

static const struct dev_pm_ops spi_stp_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	spi_stp_suspend, spi_stp_resume)
#ifdef CONFIG_HIBERNATION
	.freeze  = spi_stp_freeze,
	.thaw    = spi_stp_thaw,
	.restore = spi_stp_restore,
#endif
	};

static const struct of_device_id spi_stp_of_match[] = {
	{ .compatible = "stella,spi-stp" },
	{ .compatible = "meta,spi-stp" },
	{}
};

#ifdef STP_EMULATION
static const struct spi_device_id spi_id_table[] = {
	{"spi-virtio",0},
	{}
};
#endif // #ifdef STP_EMULATION

static struct spi_driver spi_stp_driver = {
	.driver = {
			.name = "spi_stp_driver",
			.owner = THIS_MODULE,
			.of_match_table = spi_stp_of_match,
			.pm = &spi_stp_pm_ops,
		},
	.probe = spi_stp_probe,
	.remove = spi_stp_remove,
#ifdef STP_EMULATION
	.id_table = spi_id_table,
#endif // #ifdef STP_EMULATION
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
