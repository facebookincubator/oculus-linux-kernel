// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "USBVDM: " fmt

#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/usb/pd_vdo.h>
#include <linux/workqueue.h>

#include "engine.h"
#include "subscriber.h"

#define USBVDM_MAX_EXT_MSG_LEN	260	/* PD 3.0 MaxExtendedMsgLen */

static DEFINE_MUTEX(engine_lock);
static DEFINE_MUTEX(subscription_lock);
/* Protects conn_svid/conn_pid of every engine */
static DEFINE_SPINLOCK(conn_lock);

static LIST_HEAD(engine_list);
static LIST_HEAD(subscription_list);
static LIST_HEAD(link_list);

/*
 * Engine notifications are queued here and delivered to subscribers in
 * order, with no usbvdm lock held, so engines may report them from any
 * process context, including from their own locked callbacks.
 */
static struct workqueue_struct *usbvdm_wq;

struct usbvdm_engine {
	struct device *dev;
	struct usbvdm_engine_ops ops;
	void *priv;

	u16 conn_svid;
	u16 conn_pid;

	struct list_head entry;
	struct work_struct release_work;
};

struct usbvdm_subscription {
	struct device *dev;
	struct usbvdm_subscriber_ops ops;
	void *priv;

	u16 svid;
	u16 pid;

	struct list_head entry;
};

struct usbvdm_link {
	struct device_link *link;
	struct list_head entry;
};

static struct usbvdm_subscription *usbvdm_find_subscription(u16 svid, u16 pid)
{
	struct usbvdm_subscription *pos, *subscription = NULL;

	list_for_each_entry(pos, &subscription_list, entry) {
		if (pos->svid == svid && pos->pid == pid)
			subscription = pos;
	}

	return subscription;
}

static int usbvdm_link_add(struct usbvdm_subscription *sub,
		struct usbvdm_engine *eng)
{
	struct usbvdm_link *link_node;

	link_node = kzalloc(sizeof(*link_node), GFP_KERNEL);
	if (!link_node)
		return -ENOMEM;

	link_node->link = device_link_add(sub->dev, eng->dev, DL_FLAG_STATELESS);
	if (!link_node->link)
		return -EINVAL;

	list_add(&link_node->entry, &link_list);
	return 0;
}

static bool usbvdm_engine_connected(struct usbvdm_engine *engine,
		struct usbvdm_subscription *sub)
{
	bool connected;

	spin_lock(&conn_lock);
	connected = engine->conn_svid == sub->svid &&
			engine->conn_pid == sub->pid;
	spin_unlock(&conn_lock);

	return connected;
}

static void usbvdm_engine_release(struct work_struct *work)
{
	kfree(container_of(work, struct usbvdm_engine, release_work));
}

/* -------------------------------------------------------------------------- */
/* Event delivery */

enum usbvdm_event_type {
	USBVDM_EVENT_CONNECT,
	USBVDM_EVENT_DISCONNECT,
	USBVDM_EVENT_EXT_MSG,
	USBVDM_EVENT_VDM,
	USBVDM_EVENT_TRANSFER_FIRMWARE,
};

struct usbvdm_event {
	struct work_struct work;
	struct usbvdm_engine *engine;
	enum usbvdm_event_type type;

	u16 svid;
	u16 pid;
	u8 msg_type;
	u32 vdm_hdr;
	int progress;

	size_t size;
	u8 data[];
};

static void usbvdm_deliver(struct usbvdm_subscription *sub,
		struct usbvdm_event *event, u16 svid, u16 pid)
{
	switch (event->type) {
	case USBVDM_EVENT_CONNECT:
		if (!sub->ops.connect)
			break;
		pr_debug("%s: Notifying sub '%s' of connection, SVID=0x%02x PID=0x%02x",
				__func__, dev_name(sub->dev), svid, pid);
		/* TODO(T171134119): Implement failure handling */
		sub->ops.connect(sub, svid, pid);
		break;
	case USBVDM_EVENT_DISCONNECT:
		if (!sub->ops.disconnect)
			break;
		pr_debug("%s: Notifying sub '%s' of disconnection, SVID=0x%02x PID=0x%02x",
				__func__, dev_name(sub->dev), svid, pid);
		/* TODO(T171134119): Implement failure handling */
		sub->ops.disconnect(sub);
		break;
	case USBVDM_EVENT_EXT_MSG:
		if (!sub->ops.ext_msg)
			break;
		pr_debug("%s: Notifying sub '%s' of ExtMsg, MsgType=0x%02x Len=%zu",
				__func__, dev_name(sub->dev), event->msg_type,
				event->size);
		sub->ops.ext_msg(sub, event->msg_type, event->data, event->size);
		break;
	case USBVDM_EVENT_VDM:
		if (!sub->ops.vdm)
			break;
		pr_debug("%s: Notifying sub '%s' of VDM, vdm_hdr=0x%04x num_vdos=%zu",
				__func__, dev_name(sub->dev), event->vdm_hdr,
				event->size / sizeof(u32));
		sub->ops.vdm(sub, event->vdm_hdr, (const u32 *)event->data,
				event->size / sizeof(u32));
		break;
	case USBVDM_EVENT_TRANSFER_FIRMWARE:
		if (!sub->ops.transfer_firmware)
			break;
		pr_debug("%s: Notifying sub '%s' of firmware transfer progress update, progress=%d%%",
				__func__, dev_name(sub->dev), event->progress);
		sub->ops.transfer_firmware(sub, event->progress);
		break;
	}
}

static void usbvdm_event_work(struct work_struct *work)
{
	struct usbvdm_event *event =
		container_of(work, struct usbvdm_event, work);
	struct usbvdm_engine *engine = event->engine;
	struct usbvdm_subscription *sub;
	u16 svid, pid;

	/* Only this worker changes a connection, so it follows event order */
	spin_lock(&conn_lock);
	if (event->type == USBVDM_EVENT_CONNECT) {
		engine->conn_svid = event->svid;
		engine->conn_pid = event->pid;
	}
	svid = engine->conn_svid;
	pid = engine->conn_pid;
	if (event->type == USBVDM_EVENT_DISCONNECT) {
		engine->conn_svid = 0x0;
		engine->conn_pid = 0x0;
	}
	spin_unlock(&conn_lock);

	mutex_lock(&subscription_lock);
	sub = usbvdm_find_subscription(svid, pid);
	mutex_unlock(&subscription_lock);

	/* usbvdm_unsubscribe() flushes usbvdm_wq before freeing sub */
	if (sub)
		usbvdm_deliver(sub, event, svid, pid);

	kfree(event);
}

static struct usbvdm_event *usbvdm_event_alloc(struct usbvdm_engine *engine,
		enum usbvdm_event_type type, const void *data, size_t size)
{
	struct usbvdm_event *event;

	event = kzalloc(struct_size(event, data, size), GFP_KERNEL);
	if (!event)
		return NULL;

	INIT_WORK(&event->work, usbvdm_event_work);
	event->engine = engine;
	event->type = type;
	event->size = size;
	if (size)
		memcpy(event->data, data, size);

	return event;
}

/* -------------------------------------------------------------------------- */
/* API for the engine drivers */

/**
 * usbvdm_register - Register an engine device with the framework
 * @dev: Device to register
 * @ops: Callbacks the framework will use to interact with engine driver
 * @drvdata: Engine driver private data
 *
 * Returns an opaque handle that the engine driver should track and send to
 * the framework when using the API functions, or NULL if registration fails.
 */
struct usbvdm_engine *usbvdm_register(struct device *dev,
		struct usbvdm_engine_ops ops, void *drvdata)
{
	struct usbvdm_engine *engine;
	struct usbvdm_subscription *sub;
	int rc;

	engine = kzalloc(sizeof(*engine), GFP_KERNEL);
	if (!engine)
		return ERR_PTR(-ENOMEM);

	INIT_WORK(&engine->release_work, usbvdm_engine_release);

	mutex_lock(&subscription_lock);
	mutex_lock(&engine_lock);

	engine->priv = drvdata;
	engine->ops = ops;
	engine->dev = dev;
	list_add(&engine->entry, &engine_list);

	list_for_each_entry(sub, &subscription_list, entry) {
		rc = usbvdm_link_add(sub, engine);
		if (rc) {
			list_del(&engine->entry);
			mutex_unlock(&engine_lock);
			mutex_unlock(&subscription_lock);
			return ERR_PTR(rc);
		}

		if (engine->ops.subscribe)
			engine->ops.subscribe(engine, sub->svid, sub->pid);
	}

	mutex_unlock(&engine_lock);
	mutex_unlock(&subscription_lock);

	pr_debug("Registered engine '%s'", dev_name(dev));

	return engine;
}
EXPORT_SYMBOL(usbvdm_register);

/**
 * usbvdm_unregister - Unregister a engine driver with framework
 * @engine: Engine instance provided by usbvdm_register()
 */
void usbvdm_unregister(struct usbvdm_engine *engine)
{
	struct usbvdm_link *link_node, *tmp;

	if (!engine)
		return;

	mutex_lock(&engine_lock);
	list_del(&engine->entry);
	list_for_each_entry_safe(link_node, tmp, &link_list, entry) {
		if (link_node->link->supplier == engine->dev) {
			device_link_del(link_node->link);
			list_del(&link_node->entry);
			kfree(link_node);
		}
	}
	mutex_unlock(&engine_lock);

	pr_debug("Unregistered engine '%s'", dev_name(engine->dev));

	queue_work(usbvdm_wq, &engine->release_work);
}
EXPORT_SYMBOL(usbvdm_unregister);

/**
 * usbvdm_connect - Notify the framework of a connection
 * @engine: Engine instance
 * @svid: Standard or Vendor ID that was connected
 * @pid: Product ID that was connected
 *
 * Engine drivers should call this when a device is connected
 * to their interface so that the framework can notify subscribers.
 */
void usbvdm_connect(struct usbvdm_engine *engine,
		u16 svid, u16 pid)
{
	struct usbvdm_event *event;

	if (!engine)
		return;

	pr_debug("%s: Engine '%s' received connection, SVID=0x%02x PID=0x%02x",
			__func__, dev_name(engine->dev), svid, pid);

	event = usbvdm_event_alloc(engine, USBVDM_EVENT_CONNECT, NULL, 0);
	if (!event)
		return;

	event->svid = svid;
	event->pid = pid;
	queue_work(usbvdm_wq, &event->work);
}
EXPORT_SYMBOL(usbvdm_connect);

/**
 * usbvdm_disconnect - Notify the framework of a disconnection
 * @engine: Engine instance
 *
 * Engine drivers should call this when a device is disconnected
 * from their interface so that the framework can notify subscribers.
 */
void usbvdm_disconnect(struct usbvdm_engine *engine)
{
	struct usbvdm_event *event;

	if (!engine)
		return;

	pr_debug("%s: Engine '%s' received disconnection",
			__func__, dev_name(engine->dev));

	event = usbvdm_event_alloc(engine, USBVDM_EVENT_DISCONNECT, NULL, 0);
	if (event)
		queue_work(usbvdm_wq, &event->work);
}
EXPORT_SYMBOL(usbvdm_disconnect);

/**
 * usbvdm_engine_ext_msg - Notify the framework of a received Extended Message
 * @engine: Engine instance
 * @msg_type: Message Type as found in PD Message Header
 * @data: Array of bytes containing raw payload
 * @data_len: Length of @data
 *
 * Engine drivers should call this when an Extended Message is received from
 * the device connected to their interface so that the framework
 * can notify subscribers.
 */
void usbvdm_engine_ext_msg(struct usbvdm_engine *engine,
		u8 msg_type, const u8 *data, size_t data_len)
{
	struct usbvdm_event *event;

	if (!engine)
		return;

	pr_debug("%s: Engine '%s' received ExtMsg, MsgType=0x%02x Len=%lu",
			__func__, dev_name(engine->dev), msg_type, data_len);

	if (data_len > USBVDM_MAX_EXT_MSG_LEN) {
		pr_err("%s: Engine '%s' sent oversized ExtMsg, Len=%zu",
				__func__, dev_name(engine->dev), data_len);
		return;
	}

	event = usbvdm_event_alloc(engine, USBVDM_EVENT_EXT_MSG, data,
			data_len);
	if (!event)
		return;

	event->msg_type = msg_type;
	queue_work(usbvdm_wq, &event->work);
}
EXPORT_SYMBOL(usbvdm_engine_ext_msg);

/**
 * usbvdm_engine_vdm - Notify the framework of a received VDM
 * @engine: Engine instance
 * @vdm_hdr: VDM Header
 * @vdos: Array of VDOs (not including the VDM Header)
 * @num_vdos: Length of @vdos
 *
 * Engine drivers should call this when a VDM is received from
 * the device connected to their interface so that the framework
 * can notify subscribers.
 */
void usbvdm_engine_vdm(struct usbvdm_engine *engine,
		u32 vdm_hdr, const u32 *vdos, u32 num_vdos)
{
	struct usbvdm_event *event;

	if (!engine)
		return;

	pr_debug("%s: Engine '%s' received VDM, vdm_hdr=0x%04x num_vdos=%d",
			__func__, dev_name(engine->dev), vdm_hdr, num_vdos);

	if (num_vdos > VDO_MAX_OBJECTS) {
		pr_err("%s: Engine '%s' sent VDM with too many VDOs, num_vdos=%u",
				__func__, dev_name(engine->dev), num_vdos);
		return;
	}

	event = usbvdm_event_alloc(engine, USBVDM_EVENT_VDM, vdos,
			num_vdos * sizeof(*vdos));
	if (!event)
		return;

	event->vdm_hdr = vdm_hdr;
	queue_work(usbvdm_wq, &event->work);
}
EXPORT_SYMBOL(usbvdm_engine_vdm);

/**
 * usbvdm_engine_transfer_firmware - Notify subscribers of firmware transfer progress updates
 * @engine: Engine instance
 * @progress: Progress of the firmware transfer, interpreted as a percentage
 *
 * Engine drivers should call this when progress updates of the ongoing firmware transfer are
 * received so that they may be shared with subscribers.
 */
void usbvdm_engine_transfer_firmware(struct usbvdm_engine *engine,
		int progress)
{
	struct usbvdm_event *event;

	if (!engine)
		return;

	pr_debug("%s: Engine '%s' received firmware transfer progress update, progress=%d%%",
			__func__, dev_name(engine->dev), progress);

	event = usbvdm_event_alloc(engine, USBVDM_EVENT_TRANSFER_FIRMWARE,
			NULL, 0);
	if (!event)
		return;

	event->progress = progress;
	queue_work(usbvdm_wq, &event->work);
}
EXPORT_SYMBOL(usbvdm_engine_transfer_firmware);

/**
 * usbvdm_engine_get_drvdata - Get driver private data
 * @engine: Engine instance
 */
void *usbvdm_engine_get_drvdata(struct usbvdm_engine *engine)
{
	if (!engine)
		return NULL;

	return engine->priv;
}
EXPORT_SYMBOL(usbvdm_engine_get_drvdata);

/* -------------------------------------------------------------------------- */
/* API for the client subscribers */

/**
 * usbvdm_subscribe - Subscribe for a certain SVID/PID
 * @svid: Standard or Vendor ID to subscribe to
 * @pid: Product ID to subscribe to
 * @ops: Callbacks the framework will use to interact with subscriber
 * @priv: Subscriber private data, returned by usbvdm_subscriber_get_drvdata()
 *
 * Returns an opaque handle that the subscriber should track and send to
 * the framework when using the API functions, or NULL if registration fails.
 */
struct usbvdm_subscription *usbvdm_subscribe(struct device *dev,
		u16 svid, u16 pid, struct usbvdm_subscriber_ops ops, void *priv)
{
	struct usbvdm_engine *engine;
	struct usbvdm_subscription *sub;
	int rc;

	sub = kzalloc(sizeof(*sub), GFP_KERNEL);
	if (!sub)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&subscription_lock);
	mutex_lock(&engine_lock);

	sub->svid = svid;
	sub->pid = pid;
	sub->ops = ops;
	sub->dev = dev;
	sub->priv = priv;
	list_add(&sub->entry, &subscription_list);

	list_for_each_entry(engine, &engine_list, entry) {
		rc = usbvdm_link_add(sub, engine);
		if (rc) {
			list_del(&sub->entry);
			mutex_unlock(&engine_lock);
			mutex_unlock(&subscription_lock);
			return ERR_PTR(rc);
		}

		if (engine->ops.subscribe)
			engine->ops.subscribe(engine, svid, pid);
	}

	mutex_unlock(&engine_lock);
	mutex_unlock(&subscription_lock);

	pr_debug("New subscriber for SVID/PID: 0x%04x/0x%04x", svid, pid);

	return sub;
}
EXPORT_SYMBOL(usbvdm_subscribe);

/**
 * usbvdm_unsubscribe - Unsubscribe to a certain SVID/PID
 * @sub: Subscription instance
 */
void usbvdm_unsubscribe(struct usbvdm_subscription *sub)
{
	struct work_struct *work = current_work();
	struct usbvdm_engine *engine;
	struct usbvdm_link *link_node, *tmp;

	if (!sub)
		return;

	mutex_lock(&subscription_lock);
	list_del(&sub->entry);
	mutex_unlock(&subscription_lock);

	/*
	 * A callback that unsubscribes would wait for itself here. usbvdm_wq
	 * is ordered, so no other callback is running and the flush can be
	 * skipped.
	 */
	if (!WARN_ON_ONCE(work && work->func == usbvdm_event_work))
		flush_workqueue(usbvdm_wq);

	mutex_lock(&subscription_lock);
	mutex_lock(&engine_lock);

	list_for_each_entry(engine, &engine_list, entry) {
		if (engine->ops.unsubscribe)
			engine->ops.unsubscribe(engine, sub->svid, sub->pid);
	}

	list_for_each_entry_safe(link_node, tmp, &link_list, entry) {
		if (link_node->link->consumer == sub->dev) {
			device_link_del(link_node->link);
			list_del(&link_node->entry);
			kfree(link_node);
		}
	}

	mutex_unlock(&engine_lock);
	mutex_unlock(&subscription_lock);

	pr_debug("Subscriber unsubscribed for SVID/PID: 0x%04x/0x%04x",
			sub->svid, sub->pid);

	kfree(sub);
}
EXPORT_SYMBOL(usbvdm_unsubscribe);

/**
 * usbvdm_subscriber_ext_msg - Send an Extended Message
 * @sub: Subscription instance
 * @msg_type: Message Type as found in PD Message Header
 * @data: Array of bytes containing raw payload
 * @data_len: Length of @data
 *
 * Subscribers use this to send an Extended Message. The framework determines
 * which interface it should be routed to.
 */
int usbvdm_subscriber_ext_msg(struct usbvdm_subscription *sub,
		u8 msg_type, const u8 *data, size_t data_len)
{
	struct usbvdm_engine *engine;
	int rc = -ENODEV;

	if (!sub)
		return -EINVAL;

	pr_debug("%s: Sub '%s' sending ExtMsg, MsgType=0x%02x Len=%lu",
			__func__, dev_name(sub->dev), msg_type, data_len);

	mutex_lock(&engine_lock);
	list_for_each_entry(engine, &engine_list, entry) {
		if (usbvdm_engine_connected(engine, sub)) {
			if (engine->ops.ext_msg) {
				pr_debug("%s: Notifying engine '%s' to send ExtMsg, MsgType=0x%02x Len=%lu",
						__func__, dev_name(engine->dev), msg_type, data_len);
				rc = engine->ops.ext_msg(engine, msg_type, data, data_len);
			}
			break;
		}
	}
	mutex_unlock(&engine_lock);

	return rc;
}
EXPORT_SYMBOL(usbvdm_subscriber_ext_msg);

/**
 * usbvdm_subscriber_vdm - Send a VDM
 * @sub: Subscription instance
 * @vdm_hdr: VDM Header
 * @vdos: Array of VDOs (not including the VDM Header)
 * @num_vdos: Length of @vdos
 *
 * Subscribers use this to send a VDM. The framework determines
 * which interface it should be routed to.
 */
int usbvdm_subscriber_vdm(struct usbvdm_subscription *sub,
		u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	struct usbvdm_engine *engine;
	int rc = -ENODEV;

	if (!sub)
		return -EINVAL;

	pr_debug("%s: Sub '%s' sending VDM, vdm_hdr=0x%04x, num_vdos=%d",
			__func__, dev_name(sub->dev), vdm_hdr, num_vdos);

	mutex_lock(&engine_lock);
	list_for_each_entry(engine, &engine_list, entry) {
		if (usbvdm_engine_connected(engine, sub)) {
			if (engine->ops.vdm) {
				pr_debug("%s: Notifying engine '%s' to send VDM, vdm_hdr=0x%04x, num_vdos=%d",
						__func__, dev_name(engine->dev), vdm_hdr, num_vdos);
				rc = engine->ops.vdm(engine, vdm_hdr, vdos, num_vdos);
			}
			break;
		}
	}
	mutex_unlock(&engine_lock);

	return rc;
}
EXPORT_SYMBOL(usbvdm_subscriber_vdm);

/**
 * usbvdm_subscriber_transfer_firmware - Initiate a firmware transfer
 * @sub: Subscription instance
 * @data: Firmware data to be transferred
 * @data_len: Size of @data
 *
 * Subscribers may call this to begin engine-driven firmware transfers. The engine will report
 * progress updates back to the subscriber via the subscriber's `transfer_firmware` handler.
 */
int usbvdm_subscriber_transfer_firmware(struct usbvdm_subscription *sub,
		const u8 *data, size_t data_len)
{
	struct usbvdm_engine *engine;
	int rc = -ENODEV;

	if (!sub)
		return -EINVAL;

	pr_debug("%s: Sub '%s' transferring firmware of size %lu",
			__func__, dev_name(sub->dev), data_len);

	mutex_lock(&engine_lock);
	list_for_each_entry(engine, &engine_list, entry) {
		if (usbvdm_engine_connected(engine, sub)) {
			if (engine->ops.transfer_firmware) {
				pr_debug("%s: Transferring firmware of size %lu to engine '%s'",
						__func__, data_len, dev_name(engine->dev));
				rc = engine->ops.transfer_firmware(engine, data, data_len);
			}
			break;
		}
	}
	mutex_unlock(&engine_lock);

	return rc;
}
EXPORT_SYMBOL(usbvdm_subscriber_transfer_firmware);

/**
 * usbvdm_subscriber_get_drvdata - Get driver private data
 * @sub: Subscription instance
 */
void *usbvdm_subscriber_get_drvdata(struct usbvdm_subscription *sub)
{
	if (!sub)
		return NULL;

	return sub->priv;
}
EXPORT_SYMBOL(usbvdm_subscriber_get_drvdata);

static int __init usbvdm_init(void)
{
	usbvdm_wq = alloc_ordered_workqueue("usbvdm", 0);
	if (!usbvdm_wq)
		return -ENOMEM;

	return 0;
}
subsys_initcall(usbvdm_init);

static void __exit usbvdm_exit(void)
{
	destroy_workqueue(usbvdm_wq);
}
module_exit(usbvdm_exit);

MODULE_DESCRIPTION("Meta USB VDM abstraction library");
MODULE_LICENSE("GPL v2");
