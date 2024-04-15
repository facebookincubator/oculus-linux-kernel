// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>

#include "syncboss_powerstate.h"

/*
 * These are SPI message types that the driver explicitly monitors and
 * sends to SyncBoss firmware. They must be kept in sync.
 */
#define SYNCBOSS_SET_DATA_MESSAGE_TYPE 3
#define SYNCBOSS_PROX_ENABLE_MESSAGE_TYPE 203
#define SYNCBOSS_PROX_DISABLE_MESSAGE_TYPE 204
#define SYNCBOSS_SET_PROX_CAL_MESSAGE_TYPE 205
#define SYNCBOSS_PROXSTATE_MESSAGE_TYPE 207
#define SYNCBOSS_SET_PROX_CONFIG_VERSION_MESSAGE_TYPE 212
#define SYNCBOSS_WAKEUP_REASON_MESSAGE_TYPE 244

#define DEFAULT_PROX_CONFIG_VERSION_VALUE 1

#define INVALID_PROX_CAL_VALUE -1
#define INVALID_POWERSTATE_VALUE -1

#define POWERSTATE_DEVICE_NAME "syncboss_powerstate0"
#define POWERSTATE_MISCFIFO_SIZE 1024

static int read_cal_int(struct powerstate_dev_data *devdata, const char *cal_file_name)
{
	int ret = 0;
	u32 temp_parse = 0;
	const struct firmware *fw = NULL;
	char tempstr[16] = {0};

	ret = firmware_request_nowarn(&fw, cal_file_name, devdata->dev);
	if (ret != 0) {
		dev_err(devdata->dev,
			"firmware_request_nowarn() returned %d. Ensure %s is present",
			ret, cal_file_name);
		return ret;
	}

	if (fw->size >= sizeof(tempstr)) {
		dev_err(devdata->dev,
			"unexpected size for %s (size is %zd)",
			cal_file_name, fw->size);
		ret = -EINVAL;
		goto error;
	}

	/* Copy to temp buffer to ensure null-termination */
	memcpy(tempstr, fw->data, fw->size);
	tempstr[fw->size] = '\0';

	ret = kstrtou32(tempstr, /*base */10, &temp_parse);
	if (ret < 0) {
		dev_err(devdata->dev, "failed to parse integer out of %s",
			tempstr);
		goto error;
	}

	ret = temp_parse;

error:
	release_firmware(fw);
	return ret;
}

static int prox_cal_valid(struct powerstate_dev_data *devdata)
{
	/*
	 * Note: There are cases in the factory where they need to
	 * just set prox_canc without setting the other values.  Given
	 * this use-case, we consider a prox cal "valid" if anything
	 * is >= zero.
	 */
	return (devdata->prox_canc >= 0) || (devdata->prox_thdl >= 0)
		|| (devdata->prox_thdh >= 0);
}

static void read_prox_cal(struct powerstate_dev_data *devdata)
{
	/* If prox doesn't require calibration, use dummy values. */
	if (devdata->has_no_prox_cal) {
		devdata->prox_config_version = 0;
		devdata->prox_canc = 0;
		devdata->prox_thdl = 0;
		return;
	}

	devdata->prox_config_version = read_cal_int(devdata, "PROX_PS_CAL_VERSION");
	devdata->prox_canc = read_cal_int(devdata, "PROX_PS_CANC");
	devdata->prox_thdl = read_cal_int(devdata, "PROX_PS_THDL");
	devdata->prox_thdh = read_cal_int(devdata, "PROX_PS_THDH");

	if (!prox_cal_valid(devdata)) {
		dev_err(devdata->dev,
			"failed read prox calibration data (ver: %u, canc: %d, thdl: %d, thdh: %d)",
			devdata->prox_config_version, devdata->prox_canc,
			devdata->prox_thdl, devdata->prox_thdh);
		return;
	}

	dev_info(devdata->dev,
		 "prox cal read: canc: %d, thdl: %d, thdh: %d",
		 devdata->prox_canc, devdata->prox_thdl, devdata->prox_thdh);
}

static void push_prox_cal_and_enable_wake(struct powerstate_dev_data *devdata, bool enable)
{
	u8 message_buf[sizeof(struct syncboss_data) +
		       sizeof(struct prox_config_data)] = {};
	struct syncboss_data *message = (struct syncboss_data *)message_buf;
	struct prox_config_version *prox_cfg_version =
		(struct prox_config_version *)message->data;
	struct prox_config_data *prox_cal =
		(struct prox_config_data *)message->data;
	struct syncboss_consumer_ops *ops = devdata->syncboss_ops;
	size_t data_len = 0;

	if (!devdata->has_prox)
		return;
	else if (!prox_cal_valid(devdata)) {
		dev_warn(devdata->dev, "not pushing prox cal since it's invalid");
		return;
	}

	if (enable)
		dev_info(devdata->dev,
			 "pushing prox cal to device and enabling prox wakeup");
	else
		dev_info(devdata->dev, "disabling prox wakeup");

	if (enable) {
		/* Only set prox config / calibration if we're enabling prox */

		/* Set prox config version */
		message->type = SYNCBOSS_SET_DATA_MESSAGE_TYPE;
		message->sequence_id = 0;
		message->data_len = sizeof(*prox_cfg_version);

		prox_cfg_version->type = SYNCBOSS_SET_PROX_CONFIG_VERSION_MESSAGE_TYPE;
		prox_cfg_version->config_version = devdata->prox_config_version;

		data_len = sizeof(struct syncboss_data) + message->data_len;
		ops->queue_tx_packet(devdata->dev, message, data_len, false);

		/* Set prox calibration */
		message->type = SYNCBOSS_SET_DATA_MESSAGE_TYPE;
		message->sequence_id = 0;
		message->data_len = sizeof(*prox_cal);

		prox_cal->type = SYNCBOSS_SET_PROX_CAL_MESSAGE_TYPE;
		prox_cal->prox_thdh = (u16)devdata->prox_thdh;
		prox_cal->prox_thdl = (u16)devdata->prox_thdl;
		prox_cal->prox_canc = (u16)devdata->prox_canc;

		data_len = sizeof(struct syncboss_data) + message->data_len;
		ops->queue_tx_packet(devdata->dev, message, data_len, false);
	}

	/* Enable or disable prox */
	message->type = enable ? SYNCBOSS_PROX_ENABLE_MESSAGE_TYPE : SYNCBOSS_PROX_DISABLE_MESSAGE_TYPE;
	message->sequence_id = 0;
	message->data_len = 0;

	data_len = sizeof(struct syncboss_data) + message->data_len;
	ops->queue_tx_packet(devdata->dev, message, data_len, false);
}

static void powerstate_set_enable(struct powerstate_dev_data *devdata, bool enable)
{
	int ret;
	struct syncboss_consumer_ops *ops = devdata->syncboss_ops;

	devdata->prox_config_in_progress = true;
	mb(); /* Ensure flag is set before streaming starts */

	ret = ops->enable_stream(devdata->dev);
	if (ret)
		goto error;

	push_prox_cal_and_enable_wake(devdata, enable);

	devdata->powerstate_events_enabled = enable;

	/*
	 * Note: We can stop streaming immediately since the stop
	 * streaming impl sends a sleep command to SyncBoss and waits
	 * for it to sleep.  This means our prox settings are ensured
	 * to get over as well.
	 */
	ops->disable_stream(devdata->dev);

error:
	devdata->prox_config_in_progress = false;
}

static void powerstate_enable(struct powerstate_dev_data *devdata)
{
	powerstate_set_enable(devdata, true);
}

static void powerstate_disable(struct powerstate_dev_data *devdata)
{
	powerstate_set_enable(devdata, false);
}

static int signal_powerstate_event(struct powerstate_dev_data *devdata, int evt)
{
	int ret = 0;
	bool should_update_last_evt = true;

	struct syncboss_driver_data_header_driver_message_t msg = {
		.header = {
			.header_version = SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION,
			.header_length = sizeof(struct syncboss_driver_data_header_driver_message_t),
			.from_driver = true,
		},
		.driver_message_type = SYNCBOSS_DRIVER_MESSAGE_POWERSTATE_MSG,
		.driver_message_data = evt,
	};

	if ((evt == SYNCBOSS_PROX_EVENT_SYSTEM_UP) &&
	    devdata->eat_next_system_up_event) {
		/* This is a manual reset, so no need to notify clients */
		dev_info(devdata->dev, "eating prox system_up event on reset..yum!");
		devdata->eat_next_system_up_event = false;
		/* We don't want anyone who opens a powerstate handle to see this event. */
		should_update_last_evt = false;
	} else if (evt == SYNCBOSS_PROX_EVENT_PROX_ON && devdata->eat_prox_on_events) {
		dev_info(devdata->dev, "sensor still covered. eating prox_on event..yum!");
	} else if (devdata->prox_config_in_progress) {
		dev_info(devdata->dev, "silencing powerstate event %d", evt);
		/*
		 * We don't want anyone who opens a powerstate handle to see this event
		 * since it was produced while waking the MCU just to reconfigure prox.
		 * It was not due to "real" MCU power state change.
		 */
		should_update_last_evt = false;
	} else {
		if (evt == SYNCBOSS_PROX_EVENT_PROX_OFF && devdata->eat_prox_on_events) {
			dev_info(devdata->dev, "prox_off received. Resuming handling of prox_on events.");
			devdata->eat_prox_on_events = false;
		}

		ret = miscfifo_send_buf(&devdata->powerstate_fifo, (u8 *) &msg, sizeof(msg));
		if (ret < 0)
			dev_warn_ratelimited(devdata->dev, "powerstate fifo error (%d)", ret);
	}

	if (should_update_last_evt)
		devdata->powerstate_last_evt = evt;

	return ret;
}

static int syncboss_powerstate_open(struct inode *inode, struct file *f)
{
	int ret;
	struct powerstate_dev_data *devdata =
		container_of(f->private_data, struct powerstate_dev_data,
			     misc_powerstate);

	ret = mutex_lock_interruptible(&devdata->miscdevice_mutex);
	if (ret) {
		dev_warn(devdata->dev, "syncboss open by %s (%d) aborted due to signal. ret=%d",
				current->comm, current->pid, ret);
		return ret;
	}

	ret = miscfifo_fop_open(f, &devdata->powerstate_fifo);
	if (ret)
		goto out;

	++devdata->powerstate_client_count;

	/* Send the last powerstate_event */
	if (devdata->powerstate_last_evt != INVALID_POWERSTATE_VALUE) {
		dev_info(devdata->dev,
			 "signaling powerstate_last_evt (%d)",
			 devdata->powerstate_last_evt);
		signal_powerstate_event(devdata, devdata->powerstate_last_evt);
	}

	if (devdata->powerstate_client_count == 1) {
		devdata->syncboss_ops->enable_mcu(devdata->dev);

		if (devdata->has_prox)
			read_prox_cal(devdata);

		dev_dbg(devdata->dev, "enabling powerstate events");
		powerstate_enable(devdata);
	}

	dev_info(devdata->dev, "powerstate opened by %s (%d)",
		 current->comm, current->pid);

out:
	mutex_unlock(&devdata->miscdevice_mutex);
	return ret;
}

static int syncboss_powerstate_release(struct inode *inode, struct file *file)
{
	struct miscfifo_client *client = file->private_data;
	struct miscfifo *mf = client->mf;
	struct powerstate_dev_data *devdata =
		container_of(mf, struct powerstate_dev_data,
			     powerstate_fifo);

	/*
	 * It is unsafe to use the interruptible variant here, as the driver can get
	 * in an inconsistent state if this lock fails. We need to make sure release
	 * handling occurs, since we can't retry releasing the file if, e.g., the
	 * file is released when a process is killed.
	 */
	mutex_lock(&devdata->miscdevice_mutex);

	--devdata->powerstate_client_count;

	if (devdata->powerstate_client_count == 0) {
		dev_dbg(devdata->dev, "disabling powerstate events");
		powerstate_disable(devdata);
		devdata->syncboss_ops->disable_mcu(devdata->dev);
	}

	mutex_unlock(&devdata->miscdevice_mutex);

	return miscfifo_fop_release(inode, file);
}

static const struct file_operations powerstate_fops = {
	.open = syncboss_powerstate_open,
	.release = syncboss_powerstate_release,
	.read = miscfifo_fop_read,
	.write = NULL,
	.poll = miscfifo_fop_poll
};

static int syncboss_state_handler(struct notifier_block *nb, unsigned long event, void *p)
{
	struct powerstate_dev_data *devdata = container_of(nb, struct powerstate_dev_data, syncboss_state_nb);

	if (!devdata->has_prox)
		return NOTIFY_DONE;

	switch (event) {
	/* Handlers not guaranteed to be calledfrom user thread context: */
	case SYNCBOSS_EVENT_MCU_UP:
		signal_powerstate_event(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_UP);
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_MCU_DOWN:
		signal_powerstate_event(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_DOWN);
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_MCU_PIN_RESET:
		/*
		 * Since we're triggering a reset, no need to notify clients
		 * when syncboss comes back up.
		 */
		devdata->eat_next_system_up_event = true;
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_STREAMING_SUSPEND:
		if (devdata->powerstate_last_evt == SYNCBOSS_PROX_EVENT_PROX_ON) {
			/*
			 * Handle the case where we suspend while the prox is still covered (ex. device
			 * is put into suspend by a button press, or goes into suspend after a long period
			 * of no motion).
			 *
			 * In this scenario, a PROX_ON event will be emitted when we start streaming and
			 * re-enable MCU prox events upon resume from suspend, even if the prox sensor
			 * state hasn't physically changed.
			 *
			 * To prevent such a PROX_ON from waking the whole system and turning on the
			 * display, etc, swallow all PROX_ON events received until at least one
			 * PROX_OFF event has been reported, indicating the persistent obstruction has
			 * been cleared.
			 */
			dev_dbg(devdata->dev, "ignoring prox_on events until the next prox_off");
			devdata->eat_prox_on_events = true;
		}
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_STREAMING_RESUME:
		push_prox_cal_and_enable_wake(devdata, devdata->powerstate_events_enabled);
		return NOTIFY_OK;


	/* Handlers guaranteed to be called from user thread context: */
	case SYNCBOSS_EVENT_STREAMING_STARTING:
		/*
		 * Don't (re)-read prox cal data if streaming is being started
		 * as part of sending prox configuration to the MCU. It was
		 * already read.
		 */
		if (!devdata->prox_config_in_progress) {
			/* read_prox_cal must be called from user context. */
			read_prox_cal(devdata);
		}
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_STREAMING_STOPPED:
		signal_powerstate_event(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_DOWN);
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_STREAMING_STARTED:
		push_prox_cal_and_enable_wake(devdata, devdata->powerstate_events_enabled);
		return NOTIFY_OK;
	case SYNCBOSS_EVENT_STREAMING_STOPPING:
	default:
		return NOTIFY_DONE;
	}
}

static int rx_packet_handler(struct notifier_block *nb, unsigned long type, void *p)
{
	struct powerstate_dev_data *devdata = container_of(nb, struct powerstate_dev_data, rx_packet_nb);
	struct syncboss_data *packet = p;

	switch (type) {
	case SYNCBOSS_PROXSTATE_MESSAGE_TYPE:
		signal_powerstate_event(devdata, packet->data[0] ?
					SYNCBOSS_PROX_EVENT_PROX_ON : SYNCBOSS_PROX_EVENT_PROX_OFF);
		return NOTIFY_STOP;
	case SYNCBOSS_WAKEUP_REASON_MESSAGE_TYPE:
		signal_powerstate_event(devdata, packet->data[0]);
		return NOTIFY_STOP;
	default:
		return NOTIFY_DONE;
	}
}

static int syncboss_powerstate_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct powerstate_dev_data *devdata = dev_get_drvdata(dev);
	struct device_node *parent_node = of_get_parent(node);
	int ret;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct powerstate_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);

	mutex_init(&devdata->miscdevice_mutex);

	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);

	devdata->misc_powerstate.name = POWERSTATE_DEVICE_NAME;
	devdata->misc_powerstate.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_powerstate.fops = &powerstate_fops;
	ret = misc_register(&devdata->misc_powerstate);
	if (ret < 0) {
		dev_err(dev, "failed to register powerstate misc device, error %d", ret);
		goto out;
	}

	devdata->has_prox = of_property_read_bool(node, "meta,syncboss-has-prox");
	devdata->has_no_prox_cal = of_property_read_bool(node, "meta,syncboss-has-no-prox-cal");
	dev_dbg(dev, "has-prox: %s", devdata->has_prox ? "true" : "false");

	devdata->prox_canc = INVALID_PROX_CAL_VALUE;
	devdata->prox_thdl = INVALID_PROX_CAL_VALUE;
	devdata->prox_thdh = INVALID_PROX_CAL_VALUE;
	devdata->prox_config_version = DEFAULT_PROX_CONFIG_VERSION_VALUE;
	devdata->powerstate_last_evt = INVALID_POWERSTATE_VALUE;

	devdata->powerstate_fifo.config.kfifo_size = POWERSTATE_MISCFIFO_SIZE;
	ret = devm_miscfifo_register(dev, &devdata->powerstate_fifo);
	if (ret < 0) {
		dev_err(dev, "failed to register miscfifo device, error %d", ret);
		goto err_after_miscfifo_reg;
	}

	devdata->syncboss_state_nb.notifier_call = syncboss_state_handler;
	ret = devdata->syncboss_ops->state_event_notifier_register(dev, &devdata->syncboss_state_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register state event notifier, error %d", ret);
		goto err_after_miscfifo_reg;
	}

	devdata->rx_packet_nb.notifier_call = rx_packet_handler;
	devdata->rx_packet_nb.priority = 2; /* intercept messages before they go to miscfifo/directchannel */
	ret = devdata->syncboss_ops->rx_packet_notifier_register(dev, &devdata->rx_packet_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register rx packet notifier, error %d", ret);
		goto err_after_state_event_reg;
	}

	return 0;

err_after_state_event_reg:
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);
err_after_miscfifo_reg:
	misc_deregister(&devdata->misc_powerstate);
out:
	return ret;
}

static int syncboss_powerstate_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct powerstate_dev_data *devdata = dev_get_drvdata(dev);

	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->syncboss_state_nb);

	misc_deregister(&devdata->misc_powerstate);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_powerstate_match_table[] = {
	{ .compatible = "meta,syncboss-powerstate", },
	{ },
};
#else
#define syncboss_powerstate_match_table NULL
#endif

struct platform_driver syncboss_powerstate_driver = {
	.driver = {
		.name = "syncboss_powerstate",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_powerstate_match_table
	},
	.probe = syncboss_powerstate_probe,
	.remove = syncboss_powerstate_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_powerstate_driver,
};

static int __init syncboss_powerstate_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_powerstate_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(syncboss_powerstate_init);
module_exit(syncboss_powerstate_exit);
MODULE_DESCRIPTION("Syncboss Powerstate Event Driver");
MODULE_LICENSE("GPL v2");
