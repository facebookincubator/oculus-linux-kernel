#include "syncboss.h"
#include "syncboss_camera.h"

/* Valid sequence numbers are from [1, 254] */
#define SYNCBOSS_MIN_SEQ_NUM 1
#define SYNCBOSS_MAX_SEQ_NUM 254

static ssize_t store_reset(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	bool should_reset = false;
	int status = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	if ((count >= 1) && (strtobool(buf, &should_reset) == 0) &&
	    should_reset) {

		status = mutex_lock_interruptible(&devdata->state_mutex);
		if (status != 0) {
			dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
				status);
			return status;
		}
		syncboss_pin_reset(devdata);
		mutex_unlock(&devdata->state_mutex);

		status = count;
	} else {
		dev_err(dev, "Invalid argument: \"%s\".	 Must be 1", buf);
		status = -EINVAL;
	}
	return status;
}

static ssize_t show_transaction_length(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", devdata->transaction_length);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_transaction_length(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int status = 0;
	u32 temp_transaction_length = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10, &temp_transaction_length);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	} else if (temp_transaction_length > SYNCBOSS_MAX_TRANSACTION_LENGTH) {
		dev_err(dev, "Transaction length must be <= %d",
			SYNCBOSS_MAX_TRANSACTION_LENGTH);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->transaction_length = (u16)temp_transaction_length;

	if (devdata->is_streaming) {
		dev_info(dev,
			"Transaction length changed while streaming.");
		dev_info(dev,
			"This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return count;
}

static int next_seq_num(int current_seq)
{
	int next_seq = current_seq + 1;
	BUG_ON((current_seq < SYNCBOSS_MIN_SEQ_NUM) ||
	       (current_seq > SYNCBOSS_MAX_SEQ_NUM));

	if (next_seq > SYNCBOSS_MAX_SEQ_NUM)
		next_seq = SYNCBOSS_MIN_SEQ_NUM;
	return next_seq;
}

static ssize_t show_next_avail_seq_num(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n",
			   devdata->next_avail_seq_num);

	devdata->next_avail_seq_num = next_seq_num(devdata->next_avail_seq_num);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t show_cpu_affinity(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%*pb\n",
		cpumask_pr_args(&devdata->cpu_affinity));

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_cpu_affinity(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int status = 0;
	struct cpumask temp_cpu_affinity;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = cpumask_parse(buf, &temp_cpu_affinity);
	if (status < 0) {
		dev_err(dev, "Failed to parse cpumask out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->cpu_affinity = temp_cpu_affinity;

	if (devdata->is_streaming) {
		dev_info(dev,
			"CPU affinity changed while streaming.");
		dev_info(dev,
			"This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return count;
}

static ssize_t show_transaction_period_us(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n",
			   devdata->transaction_period_ns / 1000);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_transaction_period_us(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	int status = 0;
	u32 temp_transaction_period_us = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10, &temp_transaction_period_us);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->transaction_period_ns = temp_transaction_period_us * 1000;
	status = count;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "Transaction period changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return status;
}

static ssize_t show_minimum_time_between_transactions_us(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(
	    buf, PAGE_SIZE, "%ld\n",
	    (devdata->min_time_between_transactions_ns / NSEC_PER_USEC));

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_minimum_time_between_transactions_us(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	int status = 0;
	u32 temp_minimum_time_between_transactions_us = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10,
			   &temp_minimum_time_between_transactions_us);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->min_time_between_transactions_ns =
	    temp_minimum_time_between_transactions_us * NSEC_PER_USEC;
	status = count;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "Minimum time between transactions changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return status;
}

static ssize_t show_power(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%u\n", devdata->power_state);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}


static ssize_t show_stats(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE,
			   "num_bad_magic_numbers\t"
			   "num_bad_checksums\t"
			   "num_rejected_transactions\n"
			   "%u\t%u\t%u\n",
			   devdata->stats.num_bad_magic_numbers,
			   devdata->stats.num_bad_checksums,
			   devdata->stats.num_rejected_transactions);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t show_spi_max_clk_rate(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", devdata->spi_max_clk_rate);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_spi_max_clk_rate(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int status = 0;
	u32 temp_spi_max_clk_rate = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10, &temp_spi_max_clk_rate);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	if (temp_spi_max_clk_rate > devdata->spi->max_speed_hz) {
		dev_err(dev, "Invalid value of spi_max_clk_rate: %u (max value: %u)",
			temp_spi_max_clk_rate, devdata->spi->max_speed_hz);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	/* 0 is shorthand for the max rate */
	if (temp_spi_max_clk_rate == 0)
		devdata->spi_max_clk_rate = devdata->spi->max_speed_hz;
	else
		devdata->spi_max_clk_rate = temp_spi_max_clk_rate;

	status = count;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "SPI max clock rate changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return status;
}

static ssize_t show_poll_priority(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", devdata->poll_prio);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_poll_priority(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int status = 0;
	u16 temp_priority = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou16(buf, /*base */10, &temp_priority);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	} else if (temp_priority < 1 ||
		   temp_priority > (MAX_USER_RT_PRIO -1)) {
		dev_err(dev, "Invalid real time priority");
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->poll_prio = temp_priority;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "Poll thread priority changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return count;
}

static ssize_t show_te_timestamp(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct syncboss_dev_data *devdata =
	(struct syncboss_dev_data *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
		atomic64_read(&devdata->last_te_timestamp_ns));
}

static ssize_t store_power(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	dev_err(dev, "Power file writing deprecated.  This is a no-op");
	return 0;
}

static ssize_t show_num_cameras(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int num_cams = 0;

#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
	num_cams = get_num_cameras();
#endif

	return scnprintf(buf, PAGE_SIZE, "%d\n", num_cams);
}

static ssize_t show_enable_fastpath(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", !!devdata->use_fastpath);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_enable_fastpath(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status = 0;
	u32 temp_fastpath = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base*/10, &temp_fastpath);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->use_fastpath = !!temp_fastpath;
	status = count;

	mutex_unlock(&devdata->state_mutex);
	return status;
}



/* Sysfs Attributes
 * ================
 * Note: If you add a new attribute, make sure you also set the
 *       correct ownership and permissions in the corresponding init
 *       script (such as
 *       device/oculus/monterey/rootdir/etc/init.monterey.rc)
 *
 * For most of these settings, the stream will have to be stopped/started again
 * for the settings to take effect.  So either set these before creating a
 * syncboss handle, or close/re-open the syncboss handle after changing these
 * settings.
 *
 * transaction_length - set the fixed size of the periodic SPI transaction
 * transaction_period_us - set the period of the SPI requests
 * minimum_time_between_transactions_us - set the minimum amount of time we
 *     should wait between SPI transactions
 * transaction_history_size - set the size of the transaction history
 * spi_max_clk_rate - set the maximum clock rate to use for SPI transactions
 *     (actual clock rate may be lower)

 *     have been rejected by SyncBoss (deprecated, use the stats file instead)
 * reset - set to 1 to reset the syncboss (toggle reset pin)
 * stats - show various driver stats
 * update_firmware - write 1 to do a firmware update (firmware must be under
 *      /vendor/firmware/syncboss.bin)
 * next_avail_seq_num - the next available sequence number for control calls
 * te_timestamp - timestamp of the last TE event
 */

static ssize_t store_reset(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count);

static ssize_t show_transaction_length(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);
static ssize_t store_transaction_length(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t show_transaction_period_us(struct device *dev,
					  struct device_attribute *attr,
					  char *buf);
static ssize_t store_transaction_period_us(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count);

static ssize_t show_minimum_time_between_transactions_us(struct device *dev,
						struct device_attribute *attr,
						char *buf);
static ssize_t store_minimum_time_between_transactions_us(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count);

static ssize_t show_spi_max_clk_rate(struct device *dev,
				     struct device_attribute *attr, char *buf);
static ssize_t store_spi_max_clk_rate(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count);

static ssize_t show_cpu_affinity(struct device *dev,
				 struct device_attribute *attr, char *buf);
static ssize_t store_cpu_affinity(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

static ssize_t show_stats(struct device *dev,
			  struct device_attribute *attr,
			  char *buf);

static ssize_t show_poll_priority(struct device *dev,
				  struct device_attribute *attr, char *buf);
static ssize_t store_poll_priority(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t show_next_avail_seq_num(struct device *dev,
				       struct device_attribute *attr, char *buf);

static ssize_t show_power(struct device *dev,
			  struct device_attribute *attr,
			  char *buf);

static ssize_t store_power(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t show_te_timestamp(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t show_num_cameras(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t show_enable_fastpath(struct device *dev,
				    struct device_attribute *attr,
				    char *buf);

static ssize_t store_enable_fastpath(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);


static DEVICE_ATTR(reset, S_IWUSR, NULL, store_reset);
static DEVICE_ATTR(spi_max_clk_rate, S_IWUSR | S_IRUGO, show_spi_max_clk_rate,
		   store_spi_max_clk_rate);
static DEVICE_ATTR(transaction_period_us, S_IWUSR | S_IRUGO,
		   show_transaction_period_us, store_transaction_period_us);
static DEVICE_ATTR(minimum_time_between_transactions_us, S_IWUSR | S_IRUGO,
		   show_minimum_time_between_transactions_us,
		   store_minimum_time_between_transactions_us);
static DEVICE_ATTR(transaction_length, S_IWUSR | S_IRUGO,
		   show_transaction_length, store_transaction_length);
static DEVICE_ATTR(cpu_affinity, S_IWUSR | S_IRUGO,
		   show_cpu_affinity, store_cpu_affinity);
static DEVICE_ATTR(stats, S_IRUGO,
		   show_stats, NULL);
static DEVICE_ATTR(poll_prio, S_IWUSR | S_IRUGO,
		   show_poll_priority, store_poll_priority);
static DEVICE_ATTR(next_avail_seq_num, S_IRUGO,
		   show_next_avail_seq_num, NULL);
static DEVICE_ATTR(power, S_IWUSR | S_IRUGO, show_power, store_power);
static DEVICE_ATTR(te_timestamp, S_IRUGO, show_te_timestamp, NULL);
static DEVICE_ATTR(num_cameras, S_IRUGO, show_num_cameras, NULL);
static DEVICE_ATTR(enable_fastpath, S_IWUSR | S_IRUGO, show_enable_fastpath, store_enable_fastpath);

static struct attribute *syncboss_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_spi_max_clk_rate.attr,
	&dev_attr_transaction_period_us.attr,
	&dev_attr_minimum_time_between_transactions_us.attr,
	&dev_attr_transaction_length.attr,
	&dev_attr_cpu_affinity.attr,
	&dev_attr_stats.attr,
	&dev_attr_poll_prio.attr,
	&dev_attr_next_avail_seq_num.attr,
	&dev_attr_power.attr,
	&dev_attr_te_timestamp.attr,
	&dev_attr_num_cameras.attr,
	&dev_attr_enable_fastpath.attr,
	NULL
};

static struct attribute_group syncboss_attr_grp = {
	.name = "control",
	.attrs = syncboss_attrs
};

static int create_swd_sysfs_symlink(struct device *dev, void *kobj)
{
	struct device_node *node = dev_of_node(dev);

	if (of_device_is_compatible(node, "oculus,swd")) {
		return sysfs_create_link((struct kobject *)kobj, &dev->kobj,
					 node->name);
	}

	return 0;
}

static int remove_swd_sysfs_symlink(struct device *dev, void *kobj)
{
	struct device_node *node = dev_of_node(dev);

	if (of_device_is_compatible(node, "oculus,swd"))
		sysfs_remove_link((struct kobject *)kobj, node->name);

	return 0;
}

int syncboss_init_sysfs_attrs(struct syncboss_dev_data *devdata)
{
	struct device *spi_dev = &devdata->spi->dev;
	int ret;

	ret = sysfs_create_group(&spi_dev->kobj, &syncboss_attr_grp);
	if (ret) {
		dev_err(spi_dev, "sysfs_create_group failed: error %d\n", ret);
		return ret;
	}

	ret = sysfs_create_link(&devdata->misc.this_device->kobj,
				&spi_dev->kobj, "spi");
	if (ret) {
		dev_err(spi_dev,
			"sysfs_create_link(spi) failed: error %d\n", ret);
		return ret;
	}

	ret = device_for_each_child(spi_dev, &devdata->misc.this_device->kobj,
				    create_swd_sysfs_symlink);
	if (ret) {
		dev_err(spi_dev,
			"sysfs_create_link(swd) failed: error %d\n", ret);
		return ret;
	}

	return ret;
}

void syncboss_deinit_sysfs_attrs(struct syncboss_dev_data *devdata)
{
	struct device *spi_dev = &devdata->spi->dev;

	device_for_each_child(spi_dev, &devdata->misc.this_device->kobj,
			      remove_swd_sysfs_symlink);
	sysfs_remove_link(&devdata->misc.this_device->kobj, "spi");
	sysfs_remove_group(&spi_dev->kobj, &syncboss_attr_grp);
}
