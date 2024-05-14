// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include "adreno.h"
#include "adreno_perfcounter.h"
#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_threadstats.h"

static void kgsl_destroy_thread_private(struct kref *kref)
{
	struct kgsl_thread_private *private =
		container_of(kref, struct kgsl_thread_private, refcount);

	kvfree(private);
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

static struct kgsl_thread_private *
kgsl_thread_private_new(struct kgsl_device *device)
{
	const struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_perfcounters *counters;
	const struct adreno_perfcount_group *group;
	struct kgsl_thread_private *private;
	pid_t tid = task_pid_nr(current);
	int i;

	list_for_each_entry(private, &kgsl_driver.thread_list, list) {
		if (private->tid == tid) {
			if (!kgsl_thread_private_get(private))
				private = ERR_PTR(-EINVAL);
			return private;
		}
	}

	private = kvzalloc(sizeof(struct kgsl_thread_private), GFP_KERNEL);
	if (private == NULL)
		return ERR_PTR(-ENOMEM);

	kref_init(&private->refcount);

	private->tid = tid;
	get_task_comm(private->comm, current);

	/* Get a pointer to the ALWAYSON counter. */
	counters = ADRENO_PERFCOUNTERS(adreno_dev);
	if  (counters != NULL && KGSL_PERFCOUNTER_GROUP_ALWAYSON <
			counters->group_count) {
		group = &counters->groups[KGSL_PERFCOUNTER_GROUP_ALWAYSON];
		private->alwayson_reg = (group && group->reg_count > 0) ?
				&group->regs[0] : NULL;
	}

	spin_lock_init(&private->history_lock);
	INIT_LIST_HEAD(&private->history_list);
	for (i = 0; i < KGSL_THREADSTATS_HISTORY_LENGTH; i++)
		INIT_LIST_HEAD(&private->history_entries[i].node);

	return private;
}

struct kgsl_threadstat_attribute {
	struct attribute attr;
	int type;
	ssize_t (*show)(struct kgsl_thread_private *priv, int type, char *buf);
};

static ssize_t threadstat_attr_show(struct kgsl_thread_private *priv, int type,
				    char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n", priv->stats[type]);
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
	THREADSTAT_MULTIATTR(KGSL_THREADSTATS_RETIRED, retired),
	THREADSTAT_MULTIATTR(KGSL_THREADSTATS_QUEUED, queued),
	THREADSTAT_ATTR(KGSL_THREADSTATS_ACTIVE_TIME, active_time),
	THREADSTAT_ATTR(KGSL_THREADSTATS_SYNC_DELTA, sync_delta),
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

static ssize_t history_bin_show(struct file *filep, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off,
	size_t count)
{
	struct kgsl_thread_private *private;
	struct kgsl_threadstats_history_node *node;
	struct kgsl_threadstats_entry *output = (struct kgsl_threadstats_entry *)buf;
	unsigned long flags;

	private = kobj ? container_of(kobj, struct kgsl_thread_private, kobj) :
		      NULL;
	if (!private)
		return -EIO;

	memset(buf, 0, count);

	/* Dump the history entries in list order in binary format. */
	spin_lock_irqsave(&private->history_lock, flags);
	list_for_each_entry(node, &private->history_list, node)
		memcpy(output++, &node->entry, sizeof(struct kgsl_threadstats_entry));
	spin_unlock_irqrestore(&private->history_lock, flags);

	return count;
}

static struct bin_attribute threadstat_history_attr = {
	.attr.name = "history_bin",
	.attr.mode = 0444,
	.size = sizeof(struct kgsl_threadstats_entry) * KGSL_THREADSTATS_HISTORY_LENGTH,
	.read = history_bin_show
};

void kgsl_thread_uninit_sysfs(struct kgsl_thread_private *private)
{
	int i;

	sysfs_remove_bin_file(&private->kobj, &threadstat_history_attr);

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

	if (sysfs_create_bin_file(&private->kobj, &threadstat_history_attr))
		WARN(1, "Couldn't create threadstat history file\n");
}

void kgsl_thread_private_close(struct kgsl_thread_private *private)
{
	if (private == NULL)
		return;

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

struct kgsl_thread_private *kgsl_thread_private_open(struct kgsl_device *device)
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

static void _notify_event(struct kernfs_node *kn)
{
	/* Notify any waiters polling on this event. */
	if (atomic_read(&kn->active) > 0 || kernfs_has_sleeper(kn))
		sysfs_notify_dirent(kn);
}

void kgsl_thread_queue_cmdobj(struct kgsl_thread_private *thread,
		uint32_t timestamp)
{
	struct kgsl_threadstats_history_node *node;
	struct kgsl_threadstats_entry *entry;
	uint64_t count, ktime;
	unsigned long flags;

	if (IS_ERR_OR_NULL(thread))
		return;

	/* Get the kernel monotonic clock */
	ktime = ktime_get_ns();

	spin_lock_irqsave(&thread->history_lock, flags);

	thread->stats[KGSL_THREADSTATS_QUEUED] = ktime;
	thread->stats[KGSL_THREADSTATS_QUEUED_ID] = timestamp;
	count = thread->stats[KGSL_THREADSTATS_QUEUED_COUNT]++;

	node = &thread->history_entries[count % KGSL_THREADSTATS_HISTORY_LENGTH];
	list_del_init(&node->node);
	list_add(&node->node, &thread->history_list);

	/* Initialize the history entry. */
	entry = &node->entry;
	memset(entry, 0, sizeof(struct kgsl_threadstats_entry));
	entry->timestamp = (uint64_t)timestamp;
	entry->queued = ktime;

	spin_unlock_irqrestore(&thread->history_lock, flags);

	_notify_event(thread->event_sd[KGSL_THREADSTATS_QUEUED_EVENT]);
}

void kgsl_thread_submit_cmdobj(struct kgsl_thread_private *thread,
		uint32_t timestamp, u64 ktime, u64 ticks)
{
	struct kgsl_threadstats_history_node *node;
	uint64_t alwayson_value;
	unsigned long flags;

	if (IS_ERR_OR_NULL(thread))
		return;

	thread->sync_ktime = ktime;
	thread->sync_ticks = ticks;
	thread->stats[KGSL_THREADSTATS_SUBMITTED] = ktime;
	thread->stats[KGSL_THREADSTATS_SUBMITTED_ID] = timestamp;
	thread->stats[KGSL_THREADSTATS_SUBMITTED_COUNT]++;

	/*
	 * Update the thread-local time sync. This can be used to adjust
	 * the ALWAYSON performance counter output back to the kernel's
	 * monotonic clock. Add any value stored in the perfcounter register
	 * structure to the sync ticks to match the query behavior.
	 */
	alwayson_value = (thread->alwayson_reg) ?
			READ_ONCE(thread->alwayson_reg->value) : 0;
	thread->stats[KGSL_THREADSTATS_SYNC_DELTA] = thread->sync_ktime -
			(thread->sync_ticks + alwayson_value) * 10000 / 192;

	/*
	 * Look this entry up in the history and update its submitted time if
	 * found.
	 */
	spin_lock_irqsave(&thread->history_lock, flags);
	list_for_each_entry(node, &thread->history_list, node) {
		struct kgsl_threadstats_entry *entry = &node->entry;

		if (entry->timestamp == timestamp) {
			entry->submitted = ktime;
			break;
		}
	}
	spin_unlock_irqrestore(&thread->history_lock, flags);

	_notify_event(thread->event_sd[KGSL_THREADSTATS_SUBMITTED_EVENT]);
}

void kgsl_thread_retire_cmdobj(struct kgsl_thread_private *thread,
		uint32_t timestamp, u64 start, u64 end, u64 active)
{
	if (IS_ERR_OR_NULL(thread))
		return;

	if ((active != 0 || start != 0) && end != 0) {
		struct kgsl_threadstats_history_node *node;
		uint64_t consumed_ktime = 0;
		uint64_t retired_ktime;
		uint64_t active_ktime;
		unsigned long flags;

		/*
		 * Calculate the time delta from the sync in nsecs by converting
		 * from GPU ticks (which operates on a 19.2 MHz timer) and
		 * add that to the sync ktime.
		 */
		retired_ktime = thread->sync_ktime +
				(end - thread->sync_ticks) * 10000 / 192;
		active_ktime = (active ? active : (end - start)) * 10000 / 192;
		thread->stats[KGSL_THREADSTATS_RETIRED] = retired_ktime;
		thread->stats[KGSL_THREADSTATS_ACTIVE_TIME] += active_ktime;

		if (start != 0)
			consumed_ktime = thread->sync_ktime +
					(start - thread->sync_ticks) * 10000 / 192;

		/*
		 * Look this entry up in the history and update its consumed,
		 * retired, and active times if found.
		 */
		spin_lock_irqsave(&thread->history_lock, flags);
		list_for_each_entry(node, &thread->history_list, node) {
			struct kgsl_threadstats_entry *entry = &node->entry;

			if (entry->timestamp == timestamp) {
				entry->consumed = consumed_ktime;
				entry->retired = retired_ktime;
				entry->active = active_ktime;
				break;
			}
		}
		spin_unlock_irqrestore(&thread->history_lock, flags);
	}

	thread->stats[KGSL_THREADSTATS_RETIRED_ID] = timestamp;
	thread->stats[KGSL_THREADSTATS_RETIRED_COUNT]++;

	_notify_event(thread->event_sd[KGSL_THREADSTATS_RETIRED_EVENT]);
	_notify_event(thread->event_sd[KGSL_THREADSTATS_ACTIVE_TIME_EVENT]);
}
