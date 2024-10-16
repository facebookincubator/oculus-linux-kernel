// SPDX-License-Identifier: GPL-2.0
/**
 * The MiscFIFO Benchmark Driver simulates the core syncboss driver loop,
 * providing a near minimum-bound compute time for sending periodic
 * data to userland. This is intended to synthetically produce a workload
 * similar to periodic SPI data from the MCU for head tracking IMU data.
 *
 * By being able to define the minimum work required to send data from the
 * driver to userland we can estimate the total overhead of each IMU data
 * message by comparing:
 *  1. miscfifo_bench -> sensor_bench --type driver
 *  2. miscfifo_bench -> sensor_bench --type fmq
 *
 * By measuring the difference between these two scenarios you split the
 * overhead of simply returning to userland with a payload and waking
 * a thread from the HAL/FMQ stack architecture we designed. This should
 * allow accurately measure the gain "skipping" the sensor stack should
 * produce.
 *
 * Benchmark Usage:
 * To utilize this device driver from userland:
 *   1. open("/dev/miscfifo_bench") which contains the benchmarking stream.
 *   2. echo 1600 > /sys/devices/virtual/misc/miscfifo_bench/sample_hz
 *   3. echo 1 > /sys/devices/virtual/misc/miscfifo_bench/start_stream
 *   4. read() loop each message will be a u64 with the time since boot.
 *	Which represents the timestamp for when this message was sent
 *	from the driver.
 *   5. echo 1 > /sys/devices/virtual/misc/miscfifo_bench/stop_stream
 *   6. /sys/devices/virtual/misc/miscfifo_bench/stats contains the
 *	miscfifo_bench_stats structure in the same ordering.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/miscfifo.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/time.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
	#define ktime_get_boottime_ns ktime_get_boot_ns
#endif

#define MISCFIFO_BENCH_DEVICE_NAME "miscfifo_bench"
#define MISCFIFO_BENCH_KFIFO_QUEUE_SZ 1024
#define MISCFIFO_BENCH_DEFAULT_SAMPLE_HZ 1600
#define MISCFIFO_BENCH_TRANSFER_THREAD_RT_PRIO 51
#define MISCFIFO_BENCH_SCHEDULING_SLOP_NS 10000
#define ONE_SECOND_IN_NANOSECONDS 1000000000

struct miscfifo_bench_stats {
	u64 min_sleep_ns;
	u64 max_sleep_ns;
	u64 min_send_ns;
	u64 max_send_ns;
	u64 send_count;
};

struct miscfifo_bench_data {
	/* Locks access to this structure */
	struct mutex lock;
	struct miscfifo stream;
	struct task_struct *worker;
	struct hrtimer timer;
	struct miscfifo_bench_stats stats;
	u64 sample_hz;
};

static struct miscfifo_bench_data bench_data;
static struct miscdevice bench_dev;

static int miscfifo_bench_transfer_thread(void *data)
{
	int status = 0;
	u64 before_sleep_ns = 0;
	u64 after_sleep_ns = 0;
	u64 sleep_diff_ns = 0;
	u64 after_send_ns = 0;
	u64 send_diff_ns = 0;
	u64 runtime_diff_ns = 0;
	u64 max_sleep_ns = 0;
	ktime_t sleep_until_ns = 0;
	union {
		u64 timestamp;
		u8 buffer[sizeof(u64)];
	} imu_payload;
	struct sched_param param = {
		.sched_priority = MISCFIFO_BENCH_TRANSFER_THREAD_RT_PRIO
	};

	dev_info(bench_dev.this_device, "starting transfer thread.\n");
	status = sched_setscheduler(current, SCHED_FIFO, &param);
	if (status) {
		dev_err(bench_dev.this_device, "failed to set SCHED_FIFO.\n");
		return status;
	}
	mutex_lock(&bench_data.lock);
	max_sleep_ns = ONE_SECOND_IN_NANOSECONDS / bench_data.sample_hz;
	sleep_until_ns = ktime_set(0, max_sleep_ns);
	mutex_unlock(&bench_data.lock);

	while (!kthread_should_stop()) {
		before_sleep_ns = ktime_get_boottime_ns();
		runtime_diff_ns = before_sleep_ns - after_sleep_ns;
		sleep_until_ns = ktime_set(0,
					   min(max_sleep_ns - runtime_diff_ns, max_sleep_ns));
		hrtimer_start_range_ns(&bench_data.timer, sleep_until_ns,
				       MISCFIFO_BENCH_SCHEDULING_SLOP_NS, HRTIMER_MODE_REL);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		after_sleep_ns = ktime_get_boottime_ns();

		imu_payload.timestamp = after_sleep_ns;
		status = miscfifo_send_buf(&bench_data.stream,
					   imu_payload.buffer, sizeof(u64));
		if (status < 0)
			dev_err(bench_dev.this_device, "rate limited (%d).\n", status);
		after_send_ns = ktime_get_boottime_ns();

		mutex_lock(&bench_data.lock);
		bench_data.stats.send_count++;
		sleep_diff_ns = after_sleep_ns - before_sleep_ns;
		if (sleep_diff_ns > bench_data.stats.max_sleep_ns)
			bench_data.stats.max_sleep_ns = sleep_diff_ns;
		if (sleep_diff_ns < bench_data.stats.min_sleep_ns)
			bench_data.stats.min_sleep_ns = sleep_diff_ns;

		send_diff_ns = after_send_ns - after_sleep_ns;
		if (send_diff_ns > bench_data.stats.max_send_ns)
			bench_data.stats.max_send_ns = send_diff_ns;
		if (send_diff_ns < bench_data.stats.min_send_ns)
			bench_data.stats.min_send_ns = send_diff_ns;
		mutex_unlock(&bench_data.lock);
	}
	return status;
}

static int miscfifo_bench_open(struct inode *inode, struct file *file)
{
	dev_info(bench_dev.this_device, "opened by comm:%s pid:%d\n",
		 current->comm, current->pid);
	return miscfifo_fop_open(file, &bench_data.stream);
}

static ssize_t start_stream_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	unsigned long start_stream = 0;
	int ret = 0;

	ret = kstrtoul(buf, 0, &start_stream);
	if (ret)
		return ret;
	if (start_stream != 1)
		return -EINVAL;

	ret = mutex_lock_interruptible(&bench_data.lock);
	if (ret)
		return ret;
	if (bench_data.worker) {
		ret = -EINVAL;
		goto cleanup;
	}
	bench_data.worker = kthread_run(miscfifo_bench_transfer_thread,
					NULL, "miscfifo_bench:transfer");
	if (IS_ERR(bench_data.worker)) {
		ret = PTR_ERR(bench_data.worker);
		bench_data.worker = NULL;
	} else {
		ret = len;
	}
cleanup:
	mutex_unlock(&bench_data.lock);
	return ret;
}

static ssize_t start_stream_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	int ret = 0;
	int is_stream_started = 0;

	ret = mutex_lock_interruptible(&bench_data.lock);
	if (ret)
		return ret;
	is_stream_started = bench_data.worker ? 1 : 0;
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", is_stream_started);
	mutex_unlock(&bench_data.lock);
	return ret;
}

static ssize_t stats_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int ret = 0;

	ret = mutex_lock_interruptible(&bench_data.lock);
	if (ret)
		return ret;

	if (bench_data.worker) {
		ret = scnprintf(buf, PAGE_SIZE, "%llu %llu %llu %llu %llu\n",
				bench_data.stats.min_sleep_ns,
			 bench_data.stats.max_sleep_ns,
			 bench_data.stats.min_send_ns,
			 bench_data.stats.max_send_ns,
			 bench_data.stats.send_count);
	} else {
		ret = scnprintf(buf, PAGE_SIZE, "0 0 0 0 0\n");
	}

	mutex_unlock(&bench_data.lock);
	return ret;
}

static ssize_t sample_hz_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t len)
{
	u64 sample_hz = 0;
	int ret = 0;

	ret = kstrtoull(buf, 0, &sample_hz);
	if (ret)
		return ret;
	if (sample_hz == 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&bench_data.lock);
	if (ret)
		return ret;

	bench_data.sample_hz = sample_hz;

	mutex_unlock(&bench_data.lock);
	return len;
}

static ssize_t sample_hz_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	int ret = 0;

	ret = mutex_lock_interruptible(&bench_data.lock);
	if (ret)
		return ret;
	ret = scnprintf(buf, PAGE_SIZE, "%llu\n", bench_data.sample_hz);
	mutex_unlock(&bench_data.lock);
	return ret;
}

static DEVICE_ATTR_RW(start_stream);
static DEVICE_ATTR_RO(stats);
static DEVICE_ATTR_RW(sample_hz);

static struct attribute *attrs[] = {
	&dev_attr_start_stream.attr,
	&dev_attr_stats.attr,
	&dev_attr_sample_hz.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = attrs
};

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = miscfifo_bench_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read_many,
	.poll = miscfifo_fop_poll,
};

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	if (bench_data.worker)
		wake_up_process(bench_data.worker);

	return HRTIMER_NORESTART;
}

static int __init miscfifo_bench_init(void)
{
	int status = 0;

	bench_dev.minor = MISC_DYNAMIC_MINOR;
	bench_dev.name = MISCFIFO_BENCH_DEVICE_NAME;
	bench_dev.fops = &fops;

	status = misc_register(&bench_dev);
	if (status) {
		dev_err(bench_dev.this_device, "couldn't register miscdev.\n");
		return status;
	}

	bench_data.sample_hz = MISCFIFO_BENCH_DEFAULT_SAMPLE_HZ;
	bench_data.stats.min_send_ns = UINT_MAX;
	bench_data.stats.min_sleep_ns = UINT_MAX;
	bench_data.stream.config.kfifo_size = MISCFIFO_BENCH_KFIFO_QUEUE_SZ;

	status = devm_miscfifo_register(bench_dev.this_device,
					&bench_data.stream);
	if (status) {
		dev_err(bench_dev.this_device, "couldn't register miscfifo.\n");
		goto cleanup;
	}
	hrtimer_init(&bench_data.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bench_data.timer.function = timer_callback;

	status = sysfs_create_group(&bench_dev.this_device->kobj,
				    &attr_group);
	mutex_init(&bench_data.lock);
	return status;
cleanup:
	misc_deregister(&bench_dev);
	return status;
}

static void __exit miscfifo_bench_exit(void)
{
	hrtimer_cancel(&bench_data.timer);
	if (bench_data.worker)
		kthread_stop(bench_data.worker);
	sysfs_remove_group(&bench_dev.this_device->kobj, &attr_group);
	mutex_destroy(&bench_data.lock);
	devm_miscfifo_unregister(bench_dev.this_device,
				 &bench_data.stream);
	misc_deregister(&bench_dev);
}

module_init(miscfifo_bench_init);
module_exit(miscfifo_bench_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Miscfifo Bench Driver");
