// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Meta Platforms, Inc. and affiliates.
 */
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <device/stp_device.h>
#include <stp/controller/stp_controller.h>
#include "stp-interface.h"
#include "stp_driver.h"

#if 0
#define STP_INTERFACE_PRINT_HEX_DUMP(...) print_hex_dump(__VA_ARGS__)
#else
#define STP_INTERFACE_PRINT_HEX_DUMP(...) \
	do {                              \
	} while (0)
#endif

static uint inject_bad;
module_param(inject_bad, uint, 0644);
static uint inject_bad_size = 4;
module_param(inject_bad_size, uint, 0644);

struct stp_interface {
	struct device *dev;

	u8 stp_channel_number;
	struct stp_device_channel *channel;

	struct mutex channel_mtx;

	struct work_struct stp_channel_open_work;
	bool stp_channel_open_work_stop;

	bool stp_failed_on_probe;
	uint16_t bind_fail_count;
	uint16_t bind_success_count;

	struct list_head node;
	u8 seqnum;
};

static LIST_HEAD(stp_notifier_queue);
static DEFINE_MUTEX(stp_notifier_queue_mtx);
static unsigned long spi_stp_notify_last;
static struct platform_device *dummy_pdev;
static int stp_probe_fails;

static void stp_interface_dummy_register(struct work_struct *dummy);
static DECLARE_DELAYED_WORK(stp_interface_dummy_register_work,
			    stp_interface_dummy_register);

/*
 * Dump SoC-side STP state (controller state machine, per-channel connection,
 * doorbell GPIOs) when the interface cannot reach the MCU, so the log explains
 * SoC/MCU sync disagreements. Rate limited since the failing paths retry often.
 */
static void stp_dump_interface_soc_state(const char *reason)
{
	static DEFINE_RATELIMIT_STATE(dump_rs, 5 * HZ, 1);

	if (!__ratelimit(&dump_rs))
		return;

	stp_dump_driver_state(reason);
	stp_dump_controller_state(reason);
	stp_dump_channel_state(reason);
}

static int stp_interface_wait_for_data(struct stp_interface *stpi,
				       uint32_t needs)
{
	int ret;
	uint32_t avail;
	bool waited = false;
	unsigned long timeout;
	uint32_t connected;

	timeout = jiffies +
		  msecs_to_jiffies(STP_INTERFACE_WAIT_FOR_CHANNEL_TIMEOUT_MSEC);

	while (1) {
		avail = 0;
		ret = stp_channel_rx_filled(stpi->stp_channel_number, &avail);
		if (ret) {
			dev_err(stpi->dev, "%s: stp_channel_rx_filled re=%d\n",
				__func__, ret);
			break;
		}

		if (avail >= needs)
			break;

		if (waited) {
			if (time_after(jiffies, timeout)) {
				dev_warn(stpi->dev, "%s: timedout\n", __func__);
				stp_dump_interface_soc_state(
					"wait_for_data timeout");
				ret = -ETIMEDOUT;
				break;
			}
			/*
			 * A guardrail to avoid busy loop when avail < needs for
			 * long time. It can happen if MCU produces data very
			 * slowly or never produces enough size of data.
			 */
			usleep_range(10 * 1000, 11 * 1000);
		}

		/*
		 * If the channel was invalidated, the controller marks the
		 * struct complete and wakes up blocking readers, which makes
		 * readers' wait_read functions to return without an error.
		 * Check and bail out if the wait_read returned due to the
		 * channel invalidation to avoid possible hang.
		 */
		ret = stp_channel_connected(stpi->stp_channel_number,
					    &connected);
		if (ret || !connected) {
			dev_warn(
				stpi->dev,
				"%s: stp_channel_connected ret=%d connected=%d",
				__func__, ret, connected);
			if (ret == 0)
				ret = -ERESTARTSYS;
			break;
		}

		ret = stp_channel_wait_read_timeout(
			stpi->stp_channel_number,
			msecs_to_jiffies(STP_CHANNEL_READ_WAIT_TIMEOUT_MSEC));
		if (ret <= 0) {
			if (!ret)
				ret = -ETIMEDOUT;
			dev_err(stpi->dev,
				"%s: stp_channel_wait_read_timeout ret=%d\n",
				__func__, ret);
			break;
		} else {
			/*
			 * wait_for_completion_interruptible_timeout() returns
			 * positive number when the input complete was set.
			 */
			ret = 0;
		}

		waited = true;
	}

	return ret;
}

int stp_interface_xfer_read(struct stp_interface *stpi, uint8_t *buf,
			    uint8_t seqnum, bool *stp_failed, int max_data_size,
			    int magic)
{
	int ret;
	struct stp_interface_msg_header header;
	struct stp_interface_msg_payload *payload;
	u16 payload_len;
	int retries_left = STP_MAX_CONSUME;

retry:
	*stp_failed = true;

	/* Called with stpi->channel_mtx locked */
	WARN_ON_ONCE(!mutex_is_locked(&stpi->channel_mtx));

	ret = stp_interface_wait_for_data(stpi, sizeof(header));
	if (ret) {
		dev_err(stpi->dev, "%s: stp_interface_wait_for_data failed with ret=%d\n", __func__, ret);
		return ret;
	}

	ret = stp_channel_read(stpi->channel, (char *)&header, sizeof(header),
			       false);
	if (ret < 0) {
		dev_err(stpi->dev, "%s: stp_channel_read failed with ret=%d\n", __func__, ret);
		return ret;
	}

	STP_INTERFACE_PRINT_HEX_DUMP(KERN_ERR,
				     "READ header: ", DUMP_PREFIX_OFFSET, 16, 1,
				     (__force const void *)&header,
				     sizeof(header), false);

	if (ret != sizeof(header)) {
		dev_err(stpi->dev,
			"%s: unexpected header read size. expected=%ld ret=%d\n",
			__func__, sizeof(header), ret);
		return -EINVAL;
	}

	STP_INTERFACE_PRINT_HEX_DUMP(KERN_ERR,
				     "READ header: ", DUMP_PREFIX_OFFSET, 16, 1,
				     (__force const void *)&header,
				     sizeof(header), false);

	payload_len = __le16_to_cpu(header.payload_len);
	/* The payload's containing data should not exeeced max data size */
	if (WARN_ON_ONCE(header.payload_len - sizeof(*payload) > max_data_size))
		return -EFAULT;

	payload = kmalloc(payload_len, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	ret = stp_interface_wait_for_data(stpi, payload_len);
	if (ret) {
		dev_err(stpi->dev, "%s: stp_interface_wait_for_data failed with ret=%d\n", __func__, ret);
		goto err_exit;
	}

	ret = stp_channel_read(stpi->channel, (char *)payload, payload_len,
			       false);
	if (ret < 0) {
		dev_err(stpi->dev, "%s: stp_channel_read failed with ret=%d\n", __func__, ret);
		goto err_exit;
	}

	if (ret != payload_len) {
		dev_err(stpi->dev,
			"%s: unexpected payload read size. expected=%hu ret=%d\n",
			__func__, payload_len, ret);
		goto err_exit;
	}

	STP_INTERFACE_PRINT_HEX_DUMP(KERN_ERR,
				     "READ payload:", DUMP_PREFIX_OFFSET, 16, 1,
				     (__force const void *)payload, payload_len,
				     false);

	*stp_failed = false;

	dev_dbg(stpi->dev,
		"%s: read header size=%ld payload size=%hu seqnum=%d datalen=%d data[0]=0x%x data[1]=0x%x ret=%d\n",
		__func__, sizeof(header), payload_len, header.seqnum,
		payload->u.datalen,
		payload->u.datalen > 0 ? payload->data[0] : 0xFF,
		payload->u.datalen > 1 ? payload->data[1] : 0xFF, ret);

	if (header.seqnum != seqnum || header.magic != le16_to_cpu(magic)) {
		if (--retries_left >= 0) {
			/*
			 * We received a packet, which is not for the current
			 * request or corrupted.  This happens when the
			 * previous transactions were interrupted or timedout
			 * so the SoC bailed out the master_xfer() but the
			 * MCU actually sent out data over STP. Discard it.
			 */
			dev_warn(
				stpi->dev,
				"%s: malformed packet magic exp=0x%x recv=0x%x seqnum exp=%d recv=%d, retries_left=%d\n",
				__func__, magic, le16_to_cpu(header.magic),
				seqnum, header.seqnum, retries_left);
			kfree(payload);
			goto retry;
		}

		ret = -EINVAL;
	} else {
		if (payload->flags & PAYLOAD_TYPE_ERRVAL) {
			/* IPCs were healthy but MCU read failed */
			ret = payload->u.errval;
			dev_err(stpi->dev, "%s: payload flag contains err =%d\n",__func__, ret);
			goto err_exit;
		}
		if (payload->u.datalen != (payload_len - sizeof(*payload))) {
			/* MCU sent seemingly valid packet, but invalid payload metadata. */
			dev_err(stpi->dev, "%s: expected payload datalen %u, received %u\n",
					__func__,
					(unsigned)(payload_len - sizeof(*payload)),
					payload->u.datalen);
			ret = -EINVAL;
			goto err_exit;
		}

		ret = 0;
	}

	/*
	 * The data in the payload, which has a variable length, is located at
	 * payload[1]. Only copy data on success to avoid using unvalidated
	 * data from malformed or mismatched packets.
	 */
	if (!ret)
		memcpy(buf, (char *)&payload[1], payload->u.datalen);

err_exit:
	kfree(payload);

	return ret;
}

static int stp_interface_wait_for_channel(struct stp_interface *stpi)
{
	int ret = 0;
	unsigned long timeout;

	/* Called with stpi->channel_mtx locked */
	WARN_ON_ONCE(!mutex_is_locked(&stpi->channel_mtx));

	dev_dbg(stpi->dev, "%s: enter\n", __func__);

	/* Wait for only certain amount of time after the last init */
	timeout = spi_stp_notify_last +
		  msecs_to_jiffies(STP_INTERFACE_WAIT_FOR_CHANNEL_TIMEOUT_MSEC);

	while (1) {
		uint32_t connected = 0;

		if (!stp_get_device_ready()) {
			dev_warn(stpi->dev, "%s: stp device is not ready\n",
				 __func__);
			ret = -EIO;
			break;
		}

		ret = stp_channel_connected(stpi->stp_channel_number,
					    &connected);
		if (ret) {
			dev_warn(stpi->dev,
				 "%s: stp_channel_connected ret=%d\n", __func__,
				 ret);
			break;
		}

		if (connected)
			break;

		if (time_after(jiffies, timeout)) {
			ret = -ETIMEDOUT;
			break;
		}

		mutex_unlock(&stpi->channel_mtx);
		dev_dbg(stpi->dev, "%s: waiting for channel to be ready\n",
			__func__);
		msleep(STP_CHANNEL_RETRY_DELAY_MSEC);
		mutex_lock(&stpi->channel_mtx);
	}

	dev_dbg(stpi->dev, "%s: exit ret=%d\n", __func__, ret);

	return ret;
}

static int stp_interface_inject_bad(struct stp_interface *stpi)
{
	char *buf;
	int i, ret = 0;

	if (likely(!inject_bad))
		return 0;

	inject_bad--;
	dev_warn(stpi->dev, "%s: injecting a bad packet, size=%d left=%d\n",
		 __func__, inject_bad_size, inject_bad);

	buf = kmalloc(inject_bad_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < inject_bad_size / 4; i++)
		((uint32_t *)buf)[i] = STP_INTERFACE_BAD_PACKET_PATTERN;

	ret = stp_channel_write(stpi->channel, buf, inject_bad_size, false);
	kfree(buf);

	if (ret != inject_bad_size)
		dev_err(stpi->dev, "%s: inject bad failed=%d\n", __func__, ret);
	else
		dev_warn(stpi->dev, "%s: injected a bad packet\n", __func__);

	return 0;
}

int stp_interface_xfer(struct device_node *stp_interface_node, uint8_t *buf,
		       struct stp_interface_msg_header *header,
		       struct stp_interface_msg_payload *payload,
		       uint8_t stp_msg_len, uint8_t max_message_size,
		       uint16_t magic)
{
	int ret;
	bool stp_failed = false;
	struct stp_interface *stpi = NULL;
	struct platform_device *stp_interface_pdev = NULL;

  if (!stp_interface_node) {
    pr_err("%s: stp_interface_node is NULL\n", __func__);
    ret = -EINVAL;
    goto err_exit_no_mutex;
  }

	stp_interface_pdev = of_find_device_by_node(stp_interface_node);
	if (!stp_interface_pdev) {
		pr_err("%s: unable to get stp-interface pdev\n", __func__);
		ret = -EINVAL;
		goto err_exit_no_mutex;
	}

	stpi = platform_get_drvdata(stp_interface_pdev);
	if (!stpi) {
		/* Details (synced/device_ready/spi_busy) follow in the SoC dump. */
		pr_err_ratelimited("%s: unable to get stpi struct (probe_fails=%d)\n",
				   __func__, stp_probe_fails);
		stp_dump_interface_soc_state("xfer: no stpi struct");
		ret = -EINVAL;
		goto err_exit_no_mutex;
	}

	mutex_lock(&stpi->channel_mtx);

	if (!stpi->channel) {
		dev_warn_ratelimited(stpi->dev, "%s: spi_stp is closed\n",
				     __func__);
		ret = -EFAULT;
		stp_failed = true;
		goto err_exit;
	}

	ret = stp_interface_wait_for_channel(stpi);
	if (ret) {
		dev_err(stpi->dev, "%s: spi_stp is unavailable, ret=%d\n",
			__func__, ret);
		stp_failed = true;
		goto err_exit;
	}

	/* TODO: optimize checking stale channel */
	if (stp_check_stale_channel(stpi->channel)) {
		WARN_ONCE(1, "stale stp channel=0x%p\n", stpi->channel);
		ret = -EFAULT;
		stp_failed = true;
		goto err_exit;
	}

	(void)stp_interface_inject_bad(stpi);

	if (stpi->seqnum == 255)
		stpi->seqnum = 0;
	else
		stpi->seqnum++;
	header->seqnum = stpi->seqnum;
	header->magic = cpu_to_le16(magic);

	ret = stp_channel_write(stpi->channel, (const char *)header,
				stp_msg_len, false);
	dev_dbg(stpi->dev,
		"%s: dev_addr=0x%x reg_addr=0x%x seqnum=%d stp_channel_write len=%d ret=%d\n",
		__func__, payload->dev_addr, payload->reg_addr, header->seqnum,
		stp_msg_len, ret);

	STP_INTERFACE_PRINT_HEX_DUMP(KERN_ERR, "WRITE: ", DUMP_PREFIX_OFFSET,
				     16, 1, (__force const void *)header,
				     stp_msg_len, false);

	kfree(header);

	if (ret != stp_msg_len) {
		dev_err(stpi->dev, "failed to write %d bytes, ret=%d (bytes)\n",
			stp_msg_len, ret);
		ret = -EFAULT;
		stp_failed = true;
		goto err_exit;
	}

	ret = stp_interface_xfer_read(stpi, buf, stpi->seqnum, &stp_failed,
				      max_message_size, magic);
	if (ret) {
		dev_err(stpi->dev,
			"%s: stp_interface_xfer_read failed with ret=%d\n",
			__func__, ret);
		goto err_exit;
	}
err_exit:
	mutex_unlock(&stpi->channel_mtx);
err_exit_no_mutex:
	if (stp_failed && stpi)
		stpi->stp_failed_on_probe = true;

  if (stpi && stpi->dev)
	dev_dbg(stpi->dev, "%s: exit ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(stp_interface_xfer);

static void stp_channel_open_work(struct work_struct *work)
{
	struct stp_interface *stpi;
	int retries_left = STP_CHANNEL_OPEN_WORK_RETRIES;

	stpi = container_of(work, struct stp_interface, stp_channel_open_work);

	dev_dbg(stpi->dev, "%s: enter\n", __func__);

	mutex_lock(&stpi->channel_mtx);
	while (--retries_left >= 0 && !stpi->stp_channel_open_work_stop) {
		dev_info_ratelimited(stpi->dev, "%s: spi_stp reinit\n", __func__);
		if (stpi->channel) {
			dev_warn(stpi->dev, "%s: channel was not closed\n",
				 __func__);
			break;
		}
		stpi->channel =
			stp_channel_open(stpi->stp_channel_number, true);
		if (!IS_ERR_OR_NULL(stpi->channel)) {
			dev_info(stpi->dev, "%s: channel opened\n", __func__);
			break;
		}

		dev_err_ratelimited(stpi->dev,
			"%s: stp_channel_open failed, ret=%ld, retries_left=%d\n",
			__func__, PTR_ERR(stpi->channel), retries_left);
		stpi->channel = NULL;

		mutex_unlock(&stpi->channel_mtx);
		msleep(STP_CHANNEL_RETRY_DELAY_MSEC);
		mutex_lock(&stpi->channel_mtx);
	}
	mutex_unlock(&stpi->channel_mtx);

	dev_dbg(stpi->dev, "%s: exit\n", __func__);
}

static void register_stp_interface_pdev(struct stp_interface *stpi)
{
	/*
	 * We have missied the very first notification, initialize as
	 * connected with the latest jiffies.
	 */
	spi_stp_notify_last = jiffies;

	mutex_lock(&stp_notifier_queue_mtx);
	list_add(&stpi->node, &stp_notifier_queue);
	mutex_unlock(&stp_notifier_queue_mtx);
}

static void unregister_stp_interface_pdev(struct stp_interface *stpi)
{
	mutex_lock(&stp_notifier_queue_mtx);
	list_del(&stpi->node);
	mutex_unlock(&stp_notifier_queue_mtx);
}

static int stp_interface_probe(struct platform_device *pdev)
{
	int ret;
	struct stp_interface *stpi;
	struct device_node *spi_stp_node;
	uint32_t connected, synced;

	dev_info(&pdev->dev, "%s: stp_interface_probe enter\n", __func__);

	stpi = devm_kzalloc(&pdev->dev, sizeof(*stpi), GFP_KERNEL);
	if (!stpi) {
		ret = -ENOMEM;
		goto err_exit_nomem;
	}

	mutex_init(&stpi->channel_mtx);

	ret = of_property_read_u8(pdev->dev.of_node, "stp-channel-number",
				  &stpi->stp_channel_number);
	if (ret) {
		goto err_exit_no_noti;
	}

	dev_dbg(&pdev->dev, "using stp-channel-number=%d\n",
		stpi->stp_channel_number);

	spi_stp_node = of_parse_phandle(pdev->dev.of_node, "spi-stp", 0);
	if (!spi_stp_node) {
		dev_err(&pdev->dev, "unable to get spi-stp device_node\n");
		ret = -EINVAL;
		goto err_exit_no_noti;
	}

	stpi->dev = &pdev->dev;

	mutex_lock(&stpi->channel_mtx);
	if (!stp_get_device_ready()) {
		dev_info(&pdev->dev, "spi-stp is not ready\n");
		ret = -EPROBE_DEFER;
		mutex_unlock(&stpi->channel_mtx);
		goto err_exit_no_noti;
	}

	ret = stp_protocol_synced(&synced);
	if (ret) {
		dev_info(&pdev->dev, "stp_protocol_synced ret=%d\n", ret);
		mutex_unlock(&stpi->channel_mtx);
		goto err_exit_no_noti;
	}

	if (!synced) {
		dev_info(&pdev->dev, "stp protocol not synced\n");
		ret = -EPROBE_DEFER;
		mutex_unlock(&stpi->channel_mtx);
		goto err_exit_no_noti;
	} else {
		dev_err(&pdev->dev, "stp protocol synced\n");
	}

	INIT_WORK(&stpi->stp_channel_open_work, stp_channel_open_work);

	stpi->channel = stp_channel_open(stpi->stp_channel_number, true);
	if (IS_ERR_OR_NULL(stpi->channel)) {
		mutex_unlock(&stpi->channel_mtx);
		ret = PTR_ERR(stpi->channel);
		dev_err(stpi->dev,
			"fail to open stp channel num=%d ret=%d/%d\n",
			stpi->stp_channel_number, ret, -EPROBE_DEFER);
		stpi->channel = NULL;
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	ret = stp_channel_connected(stpi->stp_channel_number, &connected);
	if (ret || connected == 0) {
		dev_err(stpi->dev,
			"probe: stp_channel_connected ret=%d connected=%d closing\n",
			ret, connected);
		stp_channel_close(stpi->channel);
		stpi->channel = NULL;
		mutex_unlock(&stpi->channel_mtx);

		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	dev_info(stpi->dev, "%s: channel=%d open and connected\n", __func__,
		 stpi->stp_channel_number);

	stpi->stp_failed_on_probe = false;

	dev_info(stpi->dev,
		 "%s: bind_success=%d bind_fail=%d\n",
		 __func__, stpi->bind_success_count,
		 stpi->bind_fail_count);

	platform_set_drvdata(pdev, stpi);

	mutex_unlock(&stpi->channel_mtx);

	/* Avoid taking both stp_notifier_queue_mtx and stpi->channel_mtx,
	 * so do this as the last step.  */
	register_stp_interface_pdev(stpi);

	stp_probe_fails = 0;
	dev_info(&pdev->dev, "%s: stp_interface_probe exit\n", __func__);

	return 0;

err_exit:
	stpi->stp_channel_open_work_stop = true;
	cancel_work_sync(&stpi->stp_channel_open_work);
	if (stpi->channel) {
		/*
		 * We were too late to unregister the notifier or to cancel
		 * the channel_open_work. Close it now. No mutext is needed
		 * as both of them are long gone.
		 */
		stp_channel_close(stpi->channel);
		stpi->channel = NULL;
	}
err_exit_no_noti:
	mutex_destroy(&stpi->channel_mtx);
err_exit_nomem:

	stp_probe_fails++;
	dev_err(&pdev->dev, "%s: stp_interface_probe exit with error ret=%d probe_fails=%d\n",
		__func__, ret, stp_probe_fails);
	stp_dump_interface_soc_state("probe deferred/failed");

	return ret;
}

static int stp_interface_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct stp_interface *stpi;

	dev_info(&pdev->dev, "%s: enter stp_interface_remove\n", __func__);

	stpi = platform_get_drvdata(pdev);

	unregister_stp_interface_pdev(stpi);
	stpi->stp_channel_open_work_stop = true;
	cancel_work_sync(&stpi->stp_channel_open_work);

	mutex_lock(&stpi->channel_mtx);
	if (stpi->channel) {
		ret = stp_channel_close(stpi->channel);
		if (ret)
			dev_err(&pdev->dev,
				"failed to close stp_channel ret=%d\n", ret);
		stpi->channel = NULL;
	}
	mutex_unlock(&stpi->channel_mtx);

	mutex_destroy(&stpi->channel_mtx);

	dev_info(&pdev->dev, "%s: done stp_interface_remove ret=%d\n", __func__,
		 ret);

	return ret;
}

static int stp_interface_dummy_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s: enter\n", __func__);
	return 0;
}

static struct platform_driver
	stp_interface_dummy_driver = { .probe = stp_interface_dummy_probe,
				       .driver = {
					       .name = "stp_interface_dummy",
				       } };

static void stp_interface_dummy_register(struct work_struct *dummy)
{
	int ret;

	ret = platform_driver_register(&stp_interface_dummy_driver);
	if (ret) {
		pr_err("stp_interface: %s: failed to rgister stp_interface_dummy_driver ret=%d\n",
		       __func__, ret);
		return;
	}
	platform_driver_unregister(&stp_interface_dummy_driver);
}

static int spi_stp_notifier(struct notifier_block *nb, unsigned long action,
			    void *unused)
{
	struct stp_interface *stpi;

	spi_stp_notify_last = jiffies;

	mutex_lock(&stp_notifier_queue_mtx);
	list_for_each_entry(stpi, &stp_notifier_queue, node) {
		mutex_lock(&stpi->channel_mtx);
		switch (action) {
		case SPI_STP_DRV_LOADED:
			dev_info(stpi->dev, "%s: spi_stp ready\n", __func__);
			/* do nothing here. open will be done by ctrl init */
			break;

		case SPI_STP_DRV_REMOVING:
			dev_info(stpi->dev, "%s: spi_stp removing\n", __func__);
			stpi->stp_channel_open_work_stop = true;
			cancel_work_sync(&stpi->stp_channel_open_work);

			if (stpi->channel) {
				stp_channel_close(stpi->channel);
				stpi->channel = NULL;
			} else {
				dev_warn(stpi->dev,
					 "%s: stp channel was not open\n",
					 __func__);
			}
			break;

		case SPI_STP_CTRL_INIT:
			dev_info_ratelimited(stpi->dev, "%s: spi_stp reinit\n", __func__);
			if (stpi->channel) {
				stp_channel_close(stpi->channel);
				stpi->channel = NULL;
			}

			stpi->stp_channel_open_work_stop = false;
			schedule_work(&stpi->stp_channel_open_work);
			break;
		}
		mutex_unlock(&stpi->channel_mtx);
	}
	mutex_unlock(&stp_notifier_queue_mtx);

	/*
	 *
	 * There is time delay between SYNC notification and INIT completion,
	 * it is typically less than 100ms. This code path is to deal
	 * with an extream corner case, so give a long enough (5sec) time delay.
	 */
	if (action == SPI_STP_CTRL_INIT)
		schedule_delayed_work(&stp_interface_dummy_register_work,
				      msecs_to_jiffies(5000));

	return NOTIFY_OK;
}

static struct notifier_block spi_stp_nb = {
	.notifier_call = spi_stp_notifier,
};

static const struct of_device_id stp_interface_of_match[] = {
	{ .compatible = "meta,stp-interface", .data = NULL },
	{}
};

static struct platform_driver
	stp_interface_driver = { .probe = stp_interface_probe,
				 .remove = stp_interface_remove,
				 .driver = {
					 .name = STP_INTERFACE_DRV_NAME,
					 .of_match_table = of_match_ptr(
						 stp_interface_of_match),
				 } };

static int __init stp_interface_init(void)
{
	int ret;

	dummy_pdev = platform_device_alloc("stp_interface_dummy", -1);

	ret = platform_device_add(dummy_pdev);
	if (ret) {
		pr_err("stp_interface: %s: platform_device_add failed, ret=%d\n",
		       __func__, ret);
		return ret;
	}

	ret = register_spi_stp_notifier(&spi_stp_nb);
	if (ret)
		pr_err("%s: register_spi_stp_notifier failed, ret=%d\n",
		       __func__, ret);
	else
		ret = platform_driver_register(&stp_interface_driver);

	if (ret) {
		platform_device_unregister(dummy_pdev);
		dummy_pdev = NULL;
	}

	return ret;
}
module_init(stp_interface_init);

static void __exit stp_interface_exit(void)
{
	unregister_spi_stp_notifier(&spi_stp_nb);
	platform_driver_unregister(&stp_interface_driver);

	if (dummy_pdev) {
		platform_device_unregister(dummy_pdev);
		dummy_pdev = NULL;
	}

	/* Wait for all in-flight RCU callbacks to complete before module unload.
	 * Placed after unregistration since those paths may schedule new RCU
	 * callbacks (e.g. kfree_rcu) that must also finish before module memory
	 * is freed.
	 */
	rcu_barrier();
}
module_exit(stp_interface_exit);

MODULE_DEVICE_TABLE(of, stp_interface_of_match);
MODULE_LICENSE("GPL v2");
