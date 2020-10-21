// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include "sched.h"
#include "walt.h"
#include <linux/of.h>
#include <linux/sched/core_ctl.h>
#include <trace/events/sched.h>

/*
 * Scheduler boost is a mechanism to temporarily place tasks on CPUs
 * with higher capacity than those where a task would have normally
 * ended up with their load characteristics. Any entity enabling
 * boost is responsible for disabling it as well.
 */

unsigned int sysctl_sched_boost; /* To/from userspace */
unsigned int sched_boost_type; /* currently activated sched boost */
enum sched_boost_policy boost_policy;

static enum sched_boost_policy boost_policy_dt = SCHED_BOOST_NONE;
static DEFINE_MUTEX(boost_mutex);

/*
 * Scheduler boost type and boost policy might at first seem unrelated,
 * however, there exists a connection between them that will allow us
 * to use them interchangeably during placement decisions. We'll explain
 * the connection here in one possible way so that the implications are
 * clear when looking at placement policies.
 *
 * When policy = SCHED_BOOST_NONE, type is either none or RESTRAINED
 * When policy = SCHED_BOOST_ON_ALL or SCHED_BOOST_ON_BIG, type can
 * neither be none nor RESTRAINED.
 */
static void set_boost_policy(int type)
{
	if (type == NO_BOOST || type == RESTRAINED_BOOST) {
		boost_policy = SCHED_BOOST_NONE;
		return;
	}

	if (boost_policy_dt) {
		boost_policy = boost_policy_dt;
		return;
	}

	if (min_possible_efficiency != max_possible_efficiency) {
		boost_policy = SCHED_BOOST_ON_BIG;
		return;
	}

	boost_policy = SCHED_BOOST_ON_ALL;
}

static bool verify_boost_params(int type)
{
	return type >= RESTRAINED_BOOST_DISABLE && type <= RESTRAINED_BOOST;
}

static void sched_no_boost_nop(void)
{
}

static void sched_full_throttle_boost_enter(void)
{
	core_ctl_set_boost(true);
	walt_enable_frequency_aggregation(true);
}

static void sched_full_throttle_boost_exit(void)
{
	core_ctl_set_boost(false);
	walt_enable_frequency_aggregation(false);
}

static void sched_conservative_boost_enter(void)
{
	update_cgroup_boost_settings();
	sched_task_filter_util = sysctl_sched_min_task_util_for_boost;
}

static void sched_conservative_boost_exit(void)
{
	sched_task_filter_util = sysctl_sched_min_task_util_for_colocation;
	restore_cgroup_boost_settings();
}

static void sched_restrained_boost_enter(void)
{
	walt_enable_frequency_aggregation(true);
}

static void sched_restrained_boost_exit(void)
{
	walt_enable_frequency_aggregation(false);
}

struct sched_boost_data {
	int refcount;
	void (*enter)(void);
	void (*exit)(void);
};

static struct sched_boost_data sched_boosts[] = {
	[NO_BOOST] = {
		.refcount = 0,
		.enter = sched_no_boost_nop,
		.exit = sched_no_boost_nop,
	},
	[FULL_THROTTLE_BOOST] = {
		.refcount = 0,
		.enter = sched_full_throttle_boost_enter,
		.exit = sched_full_throttle_boost_exit,
	},
	[CONSERVATIVE_BOOST] = {
		.refcount = 0,
		.enter = sched_conservative_boost_enter,
		.exit = sched_conservative_boost_exit,
	},
	[RESTRAINED_BOOST] = {
		.refcount = 0,
		.enter = sched_restrained_boost_enter,
		.exit = sched_restrained_boost_exit,
	},
};

#define SCHED_BOOST_START FULL_THROTTLE_BOOST
#define SCHED_BOOST_END (RESTRAINED_BOOST + 1)

static int sched_effective_boost(void)
{
	int i;

	/*
	 * The boosts are sorted in descending order by
	 * priority.
	 */
	for (i = SCHED_BOOST_START; i < SCHED_BOOST_END; i++) {
		if (sched_boosts[i].refcount >= 1)
			return i;
	}

	return NO_BOOST;
}

static void sched_boost_disable(int type)
{
	struct sched_boost_data *sb = &sched_boosts[type];
	int next_boost;

	if (sb->refcount <= 0)
		return;

	sb->refcount--;

	if (sb->refcount)
		return;

	/*
	 * This boost's refcount becomes zero, so it must
	 * be disabled. Disable it first and then apply
	 * the next boost.
	 */
	sb->exit();

	next_boost = sched_effective_boost();
	sched_boosts[next_boost].enter();
}

static void sched_boost_enable(int type)
{
	struct sched_boost_data *sb = &sched_boosts[type];
	int next_boost, prev_boost = sched_boost_type;

	sb->refcount++;

	if (sb->refcount != 1)
		return;

	/*
	 * This boost enable request did not come before.
	 * Take this new request and find the next boost
	 * by aggregating all the enabled boosts. If there
	 * is a change, disable the previous boost and enable
	 * the next boost.
	 */

	next_boost = sched_effective_boost();
	if (next_boost == prev_boost)
		return;

	sched_boosts[prev_boost].exit();
	sched_boosts[next_boost].enter();
}

static void sched_boost_disable_all(void)
{
	int i;

	for (i = SCHED_BOOST_START; i < SCHED_BOOST_END; i++) {
		if (sched_boosts[i].refcount > 0) {
			sched_boosts[i].exit();
			sched_boosts[i].refcount = 0;
		}
	}
}

static void _sched_set_boost(int type)
{
	if (type == 0)
		sched_boost_disable_all();
	else if (type > 0)
		sched_boost_enable(type);
	else
		sched_boost_disable(-type);

	/*
	 * sysctl_sched_boost holds the boost request from
	 * user space which could be different from the
	 * effectively enabled boost. Update the effective
	 * boost here.
	 */

	sched_boost_type = sched_effective_boost();
	sysctl_sched_boost = sched_boost_type;
	set_boost_policy(sysctl_sched_boost);
	trace_sched_set_boost(sysctl_sched_boost);
}

void sched_boost_parse_dt(void)
{
	struct device_node *sn;
	const char *boost_policy;

	sn = of_find_node_by_path("/sched-hmp");
	if (!sn)
		return;

	if (!of_property_read_string(sn, "boost-policy", &boost_policy)) {
		if (!strcmp(boost_policy, "boost-on-big"))
			boost_policy_dt = SCHED_BOOST_ON_BIG;
		else if (!strcmp(boost_policy, "boost-on-all"))
			boost_policy_dt = SCHED_BOOST_ON_ALL;
	}
}

int sched_set_boost(int type)
{
	int ret = 0;

	mutex_lock(&boost_mutex);
	if (verify_boost_params(type))
		_sched_set_boost(type);
	else
		ret = -EINVAL;
	mutex_unlock(&boost_mutex);
	return ret;
}

int sched_boost_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret;
	unsigned int *data = (unsigned int *)table->data;

	mutex_lock(&boost_mutex);

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		goto done;

	if (verify_boost_params(*data))
		_sched_set_boost(*data);
	else
		ret = -EINVAL;

done:
	mutex_unlock(&boost_mutex);
	return ret;
}
