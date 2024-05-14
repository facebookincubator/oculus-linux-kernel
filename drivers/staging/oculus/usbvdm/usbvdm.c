// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "USBVDM: " fmt

#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "engine.h"
#include "subscriber.h"

static DEFINE_MUTEX(engine_lock);
static DEFINE_MUTEX(subscription_lock);

static LIST_HEAD(engine_list);
static LIST_HEAD(subscription_list);
static LIST_HEAD(link_list);

struct usbvdm_engine {
	struct device *dev;
	struct usbvdm_engine_ops ops;
	void *priv;

	u16 conn_svid;
	u16 conn_pid;

	struct list_head entry;
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

	mutex_lock(&subscription_lock);
	mutex_lock(&engine_lock);

	usbvdm_engine_set_drvdata(engine, drvdata);
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

	kfree(engine);
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
	struct usbvdm_subscription *sub;

	if (!engine)
		return;

	pr_debug("%s: Engine '%s' received connection, SVID=0x%02x PID=0x%02x",
			__func__, dev_name(engine->dev), svid, pid);

	mutex_lock(&engine_lock);
	engine->conn_svid = svid;
	engine->conn_pid = pid;
	mutex_unlock(&engine_lock);

	mutex_lock(&subscription_lock);
	sub = usbvdm_find_subscription(svid, pid);

	if (sub && sub->ops.connect) {
		pr_debug("%s: Notifying sub '%s' of connection, SVID=0x%02x PID=0x%02x",
				__func__, dev_name(sub->dev), svid, pid);
		/*
		 * TODO(T171134119): Implement failure handling
		 */
		sub->ops.connect(sub, svid, pid);
	}

	mutex_unlock(&subscription_lock);
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
	struct usbvdm_subscription *sub;
	u16 svid, pid;

	if (!engine)
		return;

	mutex_lock(&engine_lock);
	svid = engine->conn_svid;
	pid = engine->conn_pid;
	engine->conn_svid = 0x0;
	engine->conn_pid = 0x0;
	mutex_unlock(&engine_lock);

	pr_debug("%s: Engine '%s' received disconnection, SVID=0x%02x PID=0x%02x",
			__func__, dev_name(engine->dev), svid, pid);

	mutex_lock(&subscription_lock);
	sub = usbvdm_find_subscription(svid, pid);

	if (sub && sub->ops.disconnect) {
		pr_debug("%s: Notifying sub '%s' of disconnection, SVID=0x%02x PID=0x%02x",
				__func__, dev_name(sub->dev), svid, pid);
		/* TODO(T171134119): Implement failure handling */
		sub->ops.disconnect(sub);
	}

	mutex_unlock(&subscription_lock);
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
	struct usbvdm_subscription *sub;

	pr_debug("%s: Engine '%s' received ExtMsg, MsgType=0x%02x Len=%lu",
			__func__, dev_name(engine->dev), msg_type, data_len);

	mutex_lock(&subscription_lock);
	sub = usbvdm_find_subscription(engine->conn_svid, engine->conn_pid);
	if (sub && sub->ops.ext_msg) {
		pr_debug("%s: Notifying sub '%s' of ExtMsg, MsgType=0x%02x Len=%lu",
				__func__, dev_name(sub->dev), msg_type, data_len);
		sub->ops.ext_msg(sub, msg_type, data, data_len);
	}
	mutex_unlock(&subscription_lock);
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
	struct usbvdm_subscription *sub;

	pr_debug("%s: Engine '%s' received VDM, vdm_hdr=0x%04x num_vdos=%d",
			__func__, dev_name(engine->dev), vdm_hdr, num_vdos);

	mutex_lock(&subscription_lock);
	sub = usbvdm_find_subscription(engine->conn_svid, engine->conn_pid);
	if (sub && sub->ops.vdm) {
		pr_debug("%s: Notifying sub '%s' of VDM, vdm_hdr=0x%04x num_vdos=%d",
				__func__, dev_name(sub->dev), vdm_hdr, num_vdos);
		sub->ops.vdm(sub, vdm_hdr, vdos, num_vdos);
	}
	mutex_unlock(&subscription_lock);
}
EXPORT_SYMBOL(usbvdm_engine_vdm);

/**
 * usbvdm_engine_set_drvdata - Set driver private data
 * @engine: Engine instance
 * @priv: Private data
 */
void usbvdm_engine_set_drvdata(struct usbvdm_engine *engine, void *priv)
{
	if (engine)
		engine->priv = priv;
}
EXPORT_SYMBOL(usbvdm_engine_set_drvdata);

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
 *
 * Returns an opaque handle that the subscriber should track and send to
 * the framework when using the API functions, or NULL if registration fails.
 */
struct usbvdm_subscription *usbvdm_subscribe(struct device *dev,
		u16 svid, u16 pid, struct usbvdm_subscriber_ops ops)
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
	struct usbvdm_engine *engine;
	struct usbvdm_link *link_node, *tmp;

	if (!sub)
		return;

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

	list_del(&sub->entry);

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
		if (engine->conn_svid == sub->svid && engine->conn_pid == sub->pid) {
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
		if (engine->conn_svid == sub->svid && engine->conn_pid == sub->pid) {
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
 * usbvdm_subscriber_set_drvdata - Set driver private data
 * @sub: Subscription instance
 * @priv: Driver private data
 */
void usbvdm_subscriber_set_drvdata(struct usbvdm_subscription *sub, void *priv)
{
	if (sub)
		sub->priv = priv;
}
EXPORT_SYMBOL(usbvdm_subscriber_set_drvdata);

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

MODULE_DESCRIPTION("Meta USB VDM abstraction library");
MODULE_LICENSE("GPL v2");
