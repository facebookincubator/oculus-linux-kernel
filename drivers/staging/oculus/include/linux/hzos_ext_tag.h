/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_HZOS_EXT_TAG_H
#define _LINUX_HZOS_EXT_TAG_H

#include <linux/hzos_ext_types.h>
#include <linux/sched.h>
#include <linux/string.h>

/*
 * The task that holds @task's tag of @kind: the thread group leader for a
 * process tag, @task itself for a thread tag. /proc/<tid>/ resolves to the
 * thread, so process tags must be redirected here to stay per process.
 */
static inline struct task_struct *
hzos_ext_tag_owner_of(struct task_struct *task, enum hzos_ext_tag_kind kind)
{
	return kind == HZOS_EXT_TAG_PROCESS ? READ_ONCE(task->group_leader) : task;
}

static inline struct hzos_ext_tag *
hzos_ext_tag_slot(struct task_struct *owner, enum hzos_ext_tag_kind kind)
{
	return kind == HZOS_EXT_TAG_PROCESS ? &owner->hzos_ext.task_profiles :
					      &owner->hzos_ext.thread_profiles;
}

/*
 * Caller holds task_lock() of the slot's owner. Strips @value in place.
 * Returns true if the tag changed (and @gen was bumped).
 */
static inline bool hzos_ext_tag_store(struct hzos_ext_tag *tag, char *value)
{
	const char *v = strstrip(value);

	if (!strcmp(tag->buf, v))
		return false;
	strscpy(tag->buf, v, sizeof(tag->buf));
	WRITE_ONCE(tag->gen, tag->gen + 1);
	return true;
}

#endif /* _LINUX_HZOS_EXT_TAG_H */
