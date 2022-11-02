/*
 * Sleepable Read-Copy Update mechanism for mutual exclusion,
 *	tiny version for non-preemptible single-CPU use.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * Copyright (C) IBM Corporation, 2017
 *
 * Author: Paul McKenney <paulmck@us.ibm.com>
 */

#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <linux/rcupdate_wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/srcu.h>

#include <linux/rcu_node_tree.h>
#include "rcu_segcblist.h"
#include "rcu.h"

int rcu_scheduler_active __read_mostly;

static int init_srcu_struct_fields(struct srcu_struct *sp)
{
	sp->srcu_lock_nesting[0] = 0;
	sp->srcu_lock_nesting[1] = 0;
	init_swait_queue_head(&sp->srcu_wq);
	sp->srcu_cb_head = NULL;
	sp->srcu_cb_tail = &sp->srcu_cb_head;
	sp->srcu_gp_running = false;
	sp->srcu_gp_waiting = false;
	sp->srcu_idx = 0;
	INIT_WORK(&sp->srcu_work, srcu_drive_gp);
	return 0;
}

#ifdef CONFIG_DEBUG_LOCK_ALLOC

int __init_srcu_struct(struct srcu_struct *sp, const char *name,
		       struct lock_class_key *key)
{
	/* Don't re-initialize a lock while it is held. */
	debug_check_no_locks_freed((void *)sp, sizeof(*sp));
	lockdep_init_map(&sp->dep_map, name, key, 0);
	return init_srcu_struct_fields(sp);
}
EXPORT_SYMBOL_GPL(__init_srcu_struct);

#else /* #ifdef CONFIG_DEBUG_LOCK_ALLOC */

/*
 * init_srcu_struct - initialize a sleep-RCU structure
 * @sp: structure to initialize.
 *
 * Must invoke this on a given srcu_struct before passing that srcu_struct
 * to any other function.  Each srcu_struct represents a separate domain
 * of SRCU protection.
 */
int init_srcu_struct(struct srcu_struct *sp)
{
	return init_srcu_struct_fields(sp);
}
EXPORT_SYMBOL_GPL(init_srcu_struct);

#endif /* #else #ifdef CONFIG_DEBUG_LOCK_ALLOC */

/*
 * cleanup_srcu_struct - deconstruct a sleep-RCU structure
 * @sp: structure to clean up.
 *
 * Must invoke this after you are finished using a given srcu_struct that
 * was initialized via init_srcu_struct(), else you leak memory.
 */
void _cleanup_srcu_struct(struct srcu_struct *sp, bool quiesced)
{
	WARN_ON(sp->srcu_lock_nesting[0] || sp->srcu_lock_nesting[1]);
	if (quiesced)
		WARN_ON(work_pending(&sp->srcu_work));
	else
		flush_work(&sp->srcu_work);
	WARN_ON(sp->srcu_gp_running);
	WARN_ON(sp->srcu_gp_waiting);
	WARN_ON(sp->srcu_cb_head);
	WARN_ON(&sp->srcu_cb_head != sp->srcu_cb_tail);
}
EXPORT_SYMBOL_GPL(_cleanup_srcu_struct);

/*
 * Removes the count for the old reader from the appropriate element of
 * the srcu_struct.
 */
void __srcu_read_unlock(struct srcu_struct *sp, int idx)
{
	int newval = sp->srcu_lock_nesting[idx] - 1;

	WRITE_ONCE(sp->srcu_lock_nesting[idx], newval);
	if (!newval && READ_ONCE(sp->srcu_gp_waiting))
		swake_up_one(&sp->srcu_wq);
}
EXPORT_SYMBOL_GPL(__srcu_read_unlock);

/*
 * Workqueue handler to drive one grace period and invoke any callbacks
 * that become ready as a result.  Single-CPU and !PREEMPT operation
 * means that we get away with murder on synchronization.  ;-)
 */
void srcu_drive_gp(struct work_struct *wp)
{
	int idx;
	struct rcu_head *lh;
	struct rcu_head *rhp;
	struct srcu_struct *sp;

	sp = container_of(wp, struct srcu_struct, srcu_work);
	if (sp->srcu_gp_running || !READ_ONCE(sp->srcu_cb_head))
		return; /* Already running or nothing to do. */

	/* Remove recently arrived callbacks and wait for readers. */
	WRITE_ONCE(sp->srcu_gp_running, true);
	local_irq_disable();
	lh = sp->srcu_cb_head;
	sp->srcu_cb_head = NULL;
	sp->srcu_cb_tail = &sp->srcu_cb_head;
	local_irq_enable();
	idx = sp->srcu_idx;
	WRITE_ONCE(sp->srcu_idx, !sp->srcu_idx);
	WRITE_ONCE(sp->srcu_gp_waiting, true);  /* srcu_read_unlock() wakes! */
	swait_event_exclusive(sp->srcu_wq, !READ_ONCE(sp->srcu_lock_nesting[idx]));
	WRITE_ONCE(sp->srcu_gp_waiting, false); /* srcu_read_unlock() cheap. */

	/* Invoke the callbacks we removed above. */
	while (lh) {
		rhp = lh;
		lh = lh->next;
		local_bh_disable();
		rhp->func(rhp);
		local_bh_enable();
	}

	/*
	 * Enable rescheduling, and if there are more callbacks,
	 * reschedule ourselves.  This can race with a call_srcu()
	 * at interrupt level, but the ->srcu_gp_running checks will
	 * straighten that out.
	 */
	WRITE_ONCE(sp->srcu_gp_running, false);
	if (READ_ONCE(sp->srcu_cb_head))
		schedule_work(&sp->srcu_work);
}
EXPORT_SYMBOL_GPL(srcu_drive_gp);

/*
 * Enqueue an SRCU callback on the specified srcu_struct structure,
 * initiating grace-period processing if it is not already running.
 */
void call_srcu(struct srcu_struct *sp, struct rcu_head *rhp,
	       rcu_callback_t func)
{
	unsigned long flags;

	rhp->func = func;
	rhp->next = NULL;
	local_irq_save(flags);
	*sp->srcu_cb_tail = rhp;
	sp->srcu_cb_tail = &rhp->next;
	local_irq_restore(flags);
	if (!READ_ONCE(sp->srcu_gp_running))
		schedule_work(&sp->srcu_work);
}
EXPORT_SYMBOL_GPL(call_srcu);

/*
 * synchronize_srcu - wait for prior SRCU read-side critical-section completion
 */
void synchronize_srcu(struct srcu_struct *sp)
{
	struct rcu_synchronize rs;

	init_rcu_head_on_stack(&rs.head);
	init_completion(&rs.completion);
	call_srcu(sp, &rs.head, wakeme_after_rcu);
	wait_for_completion(&rs.completion);
	destroy_rcu_head_on_stack(&rs.head);
}
EXPORT_SYMBOL_GPL(synchronize_srcu);

/* Lockdep diagnostics.  */
void __init rcu_scheduler_starting(void)
{
	rcu_scheduler_active = RCU_SCHEDULER_RUNNING;
}
