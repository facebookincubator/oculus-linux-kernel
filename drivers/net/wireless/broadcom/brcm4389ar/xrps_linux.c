// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * @file xrps_linux.c
 * @brief Implementation of OS layer interfaces for XRPS
 *
 *****************************************************************************/

#include "xrps_linux.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/spinlock.h>

#include "xrps.h"

#ifdef CONFIG_XRPS

static struct xrps_linux xrps_linux;

static void wq_sysint_fn(struct work_struct *work)
{
	xrps_sysint_handler();
}

static void wq_eot_fn(struct work_struct *work)
{
	xrps_send_eot();
}

int submit_eot_work(void)
{
	schedule_work(&xrps_linux.eot_work);
	return 0;
}

static enum hrtimer_restart sysint_timer_cb(struct hrtimer *timer)
{
	uint64_t next;
	int ret;
	schedule_work(&xrps_linux.sysint_work);
	ret = xrps_get_next_sysint(&next);
	if (ret != 0) {
		pr_err("Failed to get next sysint, timer not restarted\n");
		return HRTIMER_NORESTART;
	}

	hrtimer_set_expires(&xrps_linux.sysint_timer, ns_to_ktime(next));
	return HRTIMER_RESTART;
}

int start_sysint_timer(uint64_t time)
{
	hrtimer_start(&xrps_linux.sysint_timer, ns_to_ktime(time), HRTIMER_MODE_ABS);
	pr_info("XR Start timer at ns %llu\n", time);
	return 0;
}

int stop_sysint_timer(void)
{
	pr_info("XR Stop sysint timer\n");
	hrtimer_cancel(&xrps_linux.sysint_timer);
	return 0;
}

static ssize_t queue_pause_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE - 1, "%d\n",
			 xrps_linux.xrps->queue_pause);
}

static ssize_t queue_pause_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf,
				 size_t count)
{
	int ret;
	int queue_pause;

	ret = kstrtoint(buf, 10, &queue_pause);
	if (ret < 0)
		return ret;
	xrps_set_queue_pause(queue_pause);
	return count;
}

static ssize_t send_eot_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int ret = xrps_send_eot();

	if (ret < 0)
		return ret;
	return count;
}

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return scnprintf(buf, PAGE_SIZE - 1,
			 "eot_tx : %llu\n"
			 "eot_rx : %llu\n"
			 "data_txcmplt : %llu\n"
			 "sys_ints : %llu\n"
			 "max_ring_latency_us : %llu\n"
			 "avg_ring_latency_us : %llu\n"
			 "send_eot_fail : %llu\n"
			 "max_num_queued : %llu\n"
			 "avg_num_queued : %llu\n"
			 "unpause_queue : %llu\n"
			 "pause_queue : %llu\n",
			 xrps_linux.xrps->stats.eot_tx,
			 xrps_linux.xrps->stats.eot_rx,
			 xrps_linux.xrps->stats.data_txcmplt,
			 xrps_linux.xrps->stats.sys_ints,
			 xrps_linux.xrps->stats.max_ring_latency_us,
			 xrps_linux.xrps->stats.avg_ring_latency_us,
			 xrps_linux.xrps->stats.send_eot_fail,
			 xrps_linux.xrps->stats.max_num_queued,
			 xrps_linux.xrps->stats.avg_num_queued,
			 xrps_linux.xrps->stats.unpause_queue,
			 xrps_linux.xrps->stats.pause_queue);
}

static ssize_t stats_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	int val;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;
	if (val != 0)
		return -EINVAL;
	xrps_clear_stats();

	return count;
}

static ssize_t eot_disabled_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE - 1, "%d\n",
			 xrps_get_eot_disabled());
}

static ssize_t eot_disabled_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf,
				 size_t count)
{
	int ret;
	int disabled;

	ret = kstrtoint(buf, 10, &disabled);
	if (ret < 0)
		return ret;
	xrps_set_eot_disabled(disabled);
	return count;
}

static struct kobj_attribute queue_pause_attr = __ATTR_RW(queue_pause);
static struct kobj_attribute send_eot_attr = __ATTR_WO(send_eot);
static struct kobj_attribute stats_attr = __ATTR_RW(stats);
static struct kobj_attribute eot_disabled_attr = __ATTR_RW(eot_disabled);

// Create attribute group
static struct attribute *xrps_attrs[] = {
	&queue_pause_attr.attr, &send_eot_attr.attr, &stats_attr.attr, &eot_disabled_attr.attr, NULL,
};

static struct attribute_group xrps_attr_group = {
	.attrs = xrps_attrs,
};

static int64_t xrps_get_ktime(void)
{
	return ktime_get();
}

static uint64_t xrps_ktime_to_us(int64_t ktime)
{
	return ktime_to_us(ktime);
}

static void xrps_osl_spin_lock_init(xrps_osl_spinlock_t *lock)
{
	if (lock != NULL)
		spin_lock_init(lock);
}

static xrps_osl_spinlock_flag_t xrps_osl_spin_lock(xrps_osl_spinlock_t *lock)
{
	xrps_osl_spinlock_flag_t flags = 0;

	if (lock != NULL)
		spin_lock_irqsave(lock, flags);

	return flags;
}

static void xrps_osl_spin_unlock(xrps_osl_spinlock_t *lock,
				 xrps_osl_spinlock_flag_t flags)
{
	if (lock != NULL)
		spin_unlock_irqrestore(lock, flags);
}

int xrps_init_linux(struct xrps *xrps)
{
	int ret;

	pr_info("%s\n", __func__);
	xrps_linux.xrps = xrps;

	hrtimer_init(&xrps_linux.sysint_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_ABS);
	xrps_linux.sysint_timer.function = sysint_timer_cb;

	INIT_WORK(&xrps_linux.sysint_work, wq_sysint_fn);
	INIT_WORK(&xrps_linux.eot_work, wq_eot_fn);

	xrps_linux.kobj = kobject_create_and_add("xrps", NULL);
	if (!xrps_linux.kobj) {
		pr_err("%s NOMEM\n", __func__);
		return -ENOMEM;
	}

	ret = sysfs_create_group(xrps_linux.kobj, &xrps_attr_group);
	if (ret) {
		pr_err("%s: sysfs_create_group err=%d\n", __func__, ret);
		kobject_put(xrps_linux.kobj);
		return ret;
	}

	kobject_uevent(xrps_linux.kobj, KOBJ_ADD);
	return 0;
}

void xrps_cleanup_linux(struct xrps *xrps)
{
	pr_info("%s\n", __func__);
	stop_sysint_timer();
	/* Release the kobject */
	kobject_put(xrps_linux.kobj);
}

static struct xrps_osl_intf xrps_linux_intf = {
	.init_osl = xrps_init_linux,
	.get_ktime = xrps_get_ktime,
	.ktime_to_us = xrps_ktime_to_us,
	.cleanup_osl = xrps_cleanup_linux,
	.start_sysint_timer = start_sysint_timer,
	.stop_sysint_timer = stop_sysint_timer,
	.submit_eot_work = submit_eot_work,
	.spin_lock_init = xrps_osl_spin_lock_init,
	.spin_lock = xrps_osl_spin_lock,
	.spin_unlock = xrps_osl_spin_unlock,
};

int xrps_init_osl_intf(struct xrps_osl_intf **osl_intf)
{
	*osl_intf = &xrps_linux_intf;
	return 0;
}

#endif /* CONFIG_XRPS */
