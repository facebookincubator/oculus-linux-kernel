/* Copyright (c) 2018, Facebook Technologies, LLC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_threadstats.h"

static void kgsl_destroy_thread_private(struct kref *kref)
{
	struct kgsl_thread_private *private = container_of(kref,
			struct kgsl_thread_private, refcount);

	kfree(private);
}

int kgsl_thread_private_get(struct kgsl_thread_private *thread)
{
	int ret = 0;

	if (thread != NULL)
		ret = kref_get_unless_zero(&thread->refcount);
	return ret;
}

void kgsl_thread_private_put(struct kgsl_thread_private *thread)
{
	if (thread)
		kref_put(&thread->refcount, kgsl_destroy_thread_private);
}

struct kgsl_thread_private *kgsl_thread_private_find(pid_t tid)
{
	struct kgsl_thread_private *t, *private = NULL;

	mutex_lock(&kgsl_driver.process_mutex);
	list_for_each_entry(t, &kgsl_driver.thread_list, list) {
		if (t->tid == tid) {
			if (kgsl_thread_private_get(t))
				private = t;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.process_mutex);
	return private;
}

static struct kgsl_thread_private *kgsl_thread_private_new(
	struct kgsl_device *device)
{
	struct kgsl_thread_private *private;
	pid_t tid = task_pid_nr(current);

	list_for_each_entry(private, &kgsl_driver.thread_list, list) {
		if (private->tid == tid) {
			if (!kgsl_thread_private_get(private))
				private = ERR_PTR(-EINVAL);
			return private;
		}
	}

	private = kzalloc(sizeof(struct kgsl_thread_private), GFP_KERNEL);
	if (private == NULL)
		return ERR_PTR(-ENOMEM);

	kref_init(&private->refcount);

	private->tid = tid;
	get_task_comm(private->comm, current);

	return private;
}

struct kgsl_threadstat_attribute {
	struct attribute attr;
	int type;
	ssize_t (*show)(struct kgsl_thread_private *priv, int type, char *buf);
};

static ssize_t
threadstat_attr_show(struct kgsl_thread_private *priv, int type, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%llu\n", priv->stats[type]);
}

#define THREADSTAT_ATTR(_type, _name) \
[_type ## _EVENT] = { \
	.attr = { .name = __stringify(_name), .mode = 0444 }, \
	.type = _type, \
	.show = threadstat_attr_show, \
}

static ssize_t
threadstat_multiattr_show(
	struct kgsl_thread_private *priv, int type, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%llu,%llu,%llu\n",
		priv->stats[type],
		priv->stats[type + 1],
		priv->stats[type + 2]);
}

#define THREADSTAT_MULTIATTR(_type, _name) \
[_type ## _EVENT] = { \
	.attr = { .name = __stringify(_name), .mode = 0444 }, \
	.type = _type, \
	.show = threadstat_multiattr_show, \
}

struct kgsl_threadstat_attribute threadstat_attrs[] = {
	THREADSTAT_MULTIATTR(KGSL_THREADSTATS_SUBMITTED, submitted),
	THREADSTAT_MULTIATTR(KGSL_THREADSTATS_CONSUMED, consumed),
	THREADSTAT_MULTIATTR(KGSL_THREADSTATS_RETIRED, retired),
	THREADSTAT_MULTIATTR(KGSL_THREADSTATS_QUEUED, queued),
	THREADSTAT_ATTR(KGSL_THREADSTATS_ACTIVE_TIME, active_time),
};

#define to_threadstat_attr(a) \
container_of(a, struct kgsl_threadstat_attribute, attr)

static ssize_t threadstat_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_threadstat_attribute *pattr = to_threadstat_attr(attr);
	struct kgsl_thread_private *priv;
	ssize_t ret;

	priv = kobj ? container_of(kobj, struct kgsl_thread_private, kobj) :
			NULL;

	if (priv && pattr->show)
		ret = pattr->show(priv, pattr->type, buf);
	else
		ret = -EIO;

	return ret;
}

static const struct sysfs_ops threadstat_sysfs_ops = {
	.show = threadstat_sysfs_show,
};

static struct kobj_type ktype_threadstat = {
	.sysfs_ops = &threadstat_sysfs_ops,
};

void kgsl_thread_uninit_sysfs(struct kgsl_thread_private *private)
{
	int i;

	for (i = 0; i < KGSL_THREADSTATS_EVENT_MAX; i++) {
		sysfs_put(private->event_sd[i]);
		sysfs_remove_file(&private->kobj, &threadstat_attrs[i].attr);
	}

	kobject_put(&private->kobj);

	/* Put the refcount we got in kgsl_thread_init_sysfs */
	kgsl_thread_private_put(private);
}

void kgsl_thread_init_sysfs(struct kgsl_device *device,
		struct kgsl_thread_private *private)
{
	int i;
	unsigned char name[16];

	/* Keep private valid until the sysfs enries are removed. */
	kgsl_thread_private_get(private);

	snprintf(name, sizeof(name), "%d", private->tid);

	if (kobject_init_and_add(&private->kobj, &ktype_threadstat,
		kgsl_driver.threadkobj, name)) {
		WARN(1, "Unable to add sysfs dir '%s'\n", name);
		return;
	}

	for (i = 0; i < KGSL_THREADSTATS_EVENT_MAX; i++) {
		if (sysfs_create_file(&private->kobj,
			&threadstat_attrs[i].attr))
			WARN(1, "Couldn't create sysfs file '%s'\n",
				threadstat_attrs[i].attr.name);
		private->event_sd[i] = sysfs_get_dirent(
			private->kobj.sd, threadstat_attrs[i].attr.name);
	}
}

void kgsl_thread_private_close(struct kgsl_thread_private *private)
{
	mutex_lock(&kgsl_driver.process_mutex);

	if (--private->fd_count > 0) {
		mutex_unlock(&kgsl_driver.process_mutex);
		kgsl_thread_private_put(private);
		return;
	}

	pr_debug("thread: %s [%d]\n", private->comm, private->tid);
	kgsl_thread_uninit_sysfs(private);
	list_del(&private->list);

	mutex_unlock(&kgsl_driver.process_mutex);
	kgsl_thread_private_put(private);
}

struct kgsl_thread_private *kgsl_thread_private_open(
	struct kgsl_device *device)
{
	struct kgsl_thread_private *private;

	mutex_lock(&kgsl_driver.process_mutex);
	private = kgsl_thread_private_new(device);

	if (IS_ERR(private))
		goto done;

	if (private->fd_count++ == 0) {
		pr_debug("thread: %s [%d]\n", private->comm, private->tid);
		kgsl_thread_init_sysfs(device, private);
		list_add(&private->list, &kgsl_driver.thread_list);
	}

done:
	mutex_unlock(&kgsl_driver.process_mutex);
	return private;
}
