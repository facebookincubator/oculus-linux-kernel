/*
 * Copyright (c) 2026 Meta Platforms, Inc. and affiliates.
 *
 * Exposes the camera on-die temperatures reported by the syncboss MCU as
 * thermal zones.
 */

#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/thermal.h>
#include <linux/units.h>

#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>

#include "syncboss_consumer_priorities.h"

/* MCU camera shutter packet type. */
#define CAMTEMP_SHUTTER_PACKET_TYPE 254

#define CAMTEMP_MAX_CAMERAS 12

/* 0 means "not measuring" (camera idle); never treat it as a valid reading. */
#define CAMTEMP_NO_READING 0

/* A reading older than this is treated as stale and cleared. */
#define CAMTEMP_STALE_MS 3000

struct syncboss_camtemp_devdata;

struct syncboss_camtemp_sensor {
	struct syncboss_camtemp_devdata *devdata;
	bool registered;
	int temp_mc;
	unsigned long updated;
};

struct syncboss_camtemp_devdata {
	struct device *dev;
	struct syncboss_consumer_ops *syncboss_ops;
	struct notifier_block rx_packet_nb;
	struct notifier_block state_nb;

	spinlock_t lock;
	struct syncboss_camtemp_sensor sensors[CAMTEMP_MAX_CAMERAS];
};

/* Matches the MCU shutter packet layout. */
#define CAMTEMP_MAX_PERIPHERALS_PER_SLOT 2

/* Opaque; only its size matters, to reach the per-camera values that follow. */
struct camtemp_peripheral_metadata {
	u32 peripheral_type;
	u8 payload[13];
} __packed;

/*
 * Trailing per-camera values (one per camera after the first).
 * Must match the MCU shutter packet layout.
 */
struct camtemp_per_camera_vals {
	u16 exposure_us;
	u16 gain;
	s8 temp_c;
} __packed;

/*
 * Fixed head of the MCU shutter packet, up to num_per_camera_values; the
 * variable-length per-camera values follow. Must match the MCU packet layout.
 */
struct camtemp_shutter_prefix {
	u8 cam_type;
	u32 cam_id;
	u8 frame_type;
	u64 timestamp;
	u32 count;
	u16 exposure_us;
	u16 gain;
	u8 sequence_number;
	u8 is_sync_pulse;
	u64 start_of_frame_timestamp_us;
	u32 exposure_offset_from_disp_us;
	u16 readout_duration_us;
	u64 synced_illumination_timestamp;
	u32 enabled_leds;
	u16 lens_pos_dac;
	s8 cam_temp_c;
	struct camtemp_peripheral_metadata peripheral_metadata[CAMTEMP_MAX_PERIPHERALS_PER_SLOT];
	u8 num_peripherals;
	u8 num_per_camera_values;
} __packed;

static void camtemp_clear_all(struct syncboss_camtemp_devdata *devdata)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&devdata->lock, flags);
	for (i = 0; i < CAMTEMP_MAX_CAMERAS; i++) {
		devdata->sensors[i].temp_mc = CAMTEMP_NO_READING;
		devdata->sensors[i].updated = 0;
	}
	spin_unlock_irqrestore(&devdata->lock, flags);
}

static int camtemp_rx_packet_handler(struct notifier_block *nb, unsigned long type, void *pi)
{
	struct syncboss_camtemp_devdata *devdata =
		container_of(nb, struct syncboss_camtemp_devdata, rx_packet_nb);
	struct rx_packet_info *packet_info = pi;
	const struct syncboss_data *packet = packet_info->data;
	const struct camtemp_shutter_prefix *shutter;
	const struct camtemp_per_camera_vals *per_camera_vals;
	unsigned long flags;
	int num_extra;
	int slot;
	u32 cam_id;
	u8 bit;

	if (type != CAMTEMP_SHUTTER_PACKET_TYPE)
		return NOTIFY_DONE;

	if (packet->data_len < sizeof(*shutter)) {
		dev_warn_ratelimited(devdata->dev, "shutter packet too short: %u\n",
				     packet->data_len);
		return NOTIFY_DONE;
	}

	shutter = (const struct camtemp_shutter_prefix *)packet->data;
	per_camera_vals = (const struct camtemp_per_camera_vals *)(shutter + 1);

	/* Use the trailer only if the packet is long enough; else fall back to cam_temp_c. */
	num_extra = shutter->num_per_camera_values;
	if (packet->data_len < sizeof(*shutter) + num_extra * sizeof(*per_camera_vals))
		num_extra = 0;

	spin_lock_irqsave(&devdata->lock, flags);
	cam_id = shutter->cam_id;
	slot = -1;
	for (bit = 0; bit < CAMTEMP_MAX_CAMERAS; bit++) {
		struct syncboss_camtemp_sensor *sensor = &devdata->sensors[bit];
		s8 temp_c;

		if (!(cam_id & BIT(bit)))
			continue;

		/*
		 * slot is this camera's position within cam_id, which is how the
		 * MCU indexes per_camera_vals[]. It must advance for every camera
		 * in the mask, including ones we do not publish, or every later
		 * camera's reading is attributed to the wrong camera.
		 */
		slot++;

		if (!sensor->registered)
			continue;

		if (slot == 0)
			temp_c = shutter->cam_temp_c;
		else if (slot <= num_extra)
			temp_c = per_camera_vals[slot - 1].temp_c;
		else
			continue;

		if (temp_c == CAMTEMP_NO_READING)
			continue;

		sensor->temp_mc = temp_c * MILLIDEGREE_PER_DEGREE;
		sensor->updated = jiffies;
	}
	spin_unlock_irqrestore(&devdata->lock, flags);

	return NOTIFY_OK;
}

static int camtemp_state_handler(struct notifier_block *nb, unsigned long event, void *p)
{
	struct syncboss_camtemp_devdata *devdata =
		container_of(nb, struct syncboss_camtemp_devdata, state_nb);

	switch (event) {
	case SYNCBOSS_EVENT_STREAMING_STOPPED:
	case SYNCBOSS_EVENT_STREAMING_SUSPENDED:
	case SYNCBOSS_EVENT_MCU_DOWN:
		camtemp_clear_all(devdata);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int camtemp_get_temp(struct thermal_zone_device *tz, int *temperature)
{
	struct syncboss_camtemp_sensor *sensor = tz->devdata;
	struct syncboss_camtemp_devdata *devdata = sensor->devdata;
	unsigned long flags;

	if (!temperature)
		return -EINVAL;

	spin_lock_irqsave(&devdata->lock, flags);
	if (sensor->updated &&
	    time_is_before_jiffies(sensor->updated + msecs_to_jiffies(CAMTEMP_STALE_MS))) {
		sensor->temp_mc = CAMTEMP_NO_READING;
		sensor->updated = 0;
	}
	*temperature = sensor->temp_mc;
	spin_unlock_irqrestore(&devdata->lock, flags);

	return 0;
}

static const struct thermal_zone_device_ops camtemp_thermal_ops = {
	.get_temp = camtemp_get_temp,
};

static int camtemp_register_zones(struct syncboss_camtemp_devdata *devdata)
{
	struct thermal_zone_device *tzd;
	int registered = 0;
	int i;

	for (i = 0; i < CAMTEMP_MAX_CAMERAS; i++) {
		struct syncboss_camtemp_sensor *sensor = &devdata->sensors[i];

		sensor->devdata = devdata;
		sensor->temp_mc = CAMTEMP_NO_READING;

		tzd = devm_thermal_of_zone_register(devdata->dev, i, sensor,
						    &camtemp_thermal_ops);
		if (IS_ERR(tzd)) {
			/* This camera has no zone pointing at us in the device tree. */
			if (PTR_ERR(tzd) == -ENODEV)
				continue;
			return PTR_ERR(tzd);
		}

		sensor->registered = true;
		registered++;
	}

	if (!registered) {
		dev_err(devdata->dev, "no camera thermal zones in device tree\n");
		return -ENODEV;
	}

	dev_dbg(devdata->dev, "registered %d camera thermal zones\n", registered);

	return 0;
}

static int syncboss_camtemp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *parent_node = of_get_parent(dev->of_node);
	struct syncboss_camtemp_devdata *devdata;
	int ret;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device\n");
		of_node_put(parent_node);
		return -ENODEV;
	}
	of_node_put(parent_node);

	devdata = devm_kzalloc(dev, sizeof(*devdata), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);
	spin_lock_init(&devdata->lock);
	dev_set_drvdata(dev, devdata);

	ret = camtemp_register_zones(devdata);
	if (ret)
		return ret;

	devdata->rx_packet_nb.notifier_call = camtemp_rx_packet_handler;
	devdata->rx_packet_nb.priority = SYNCBOSS_PACKET_CONSUMER_PRIORITY_CAMTEMP;
	ret = devdata->syncboss_ops->rx_packet_notifier_register(dev, &devdata->rx_packet_nb);
	if (ret) {
		dev_err(dev, "failed to register rx packet notifier: %d\n", ret);
		return ret;
	}

	devdata->state_nb.notifier_call = camtemp_state_handler;
	devdata->state_nb.priority = SYNCBOSS_STATE_CONSUMER_PRIORITY_CAMTEMP;
	ret = devdata->syncboss_ops->state_event_notifier_register(dev, &devdata->state_nb);
	if (ret) {
		dev_err(dev, "failed to register state notifier: %d\n", ret);
		devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
		return ret;
	}

	return 0;
}

static int syncboss_camtemp_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct syncboss_camtemp_devdata *devdata = dev_get_drvdata(dev);

	devdata->syncboss_ops->state_event_notifier_unregister(dev, &devdata->state_nb);
	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);

	return 0;
}

static const struct of_device_id syncboss_camtemp_match_table[] = {
	{ .compatible = "meta,syncboss-camtemp" },
	{},
};
MODULE_DEVICE_TABLE(of, syncboss_camtemp_match_table);

static struct platform_driver syncboss_camtemp_driver = {
	.probe = syncboss_camtemp_probe,
	.remove = syncboss_camtemp_remove,
	.driver = {
		.name = "syncboss_camtemp",
		.of_match_table = syncboss_camtemp_match_table,
	},
};

module_platform_driver(syncboss_camtemp_driver);

MODULE_DESCRIPTION("Syncboss camera die temperature thermal zones");
MODULE_LICENSE("GPL v2");
