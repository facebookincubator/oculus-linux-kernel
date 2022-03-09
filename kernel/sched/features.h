/*
 * Only give sleepers 50% of their service deficit. This allows
 * them to run sooner, but does not allow tons of sleepers to
 * rip the spread apart.
 */
SCHED_FEAT(GENTLE_FAIR_SLEEPERS, true)

/*
 * Place new tasks ahead so that they do not starve already running
 * tasks
 */
SCHED_FEAT(START_DEBIT, true)

/*
 * Prefer to schedule the task we woke last (assuming it failed
 * wakeup-preemption), since its likely going to consume data we
 * touched, increases cache locality.
 */
SCHED_FEAT(NEXT_BUDDY, false)

/*
 * Prefer to schedule the task that ran last (when we did
 * wake-preempt) as that likely will touch the same data, increases
 * cache locality.
 */
SCHED_FEAT(LAST_BUDDY, true)

/*
 * skip buddy i.e task called yield() is always skipped and the
 * next entity is selected to run irrespective of the vruntime
 */
SCHED_FEAT(STRICT_SKIP_BUDDY, true)

/*
 * Consider buddies to be cache hot, decreases the likelyness of a
 * cache buddy being migrated away, increases cache locality.
 */
SCHED_FEAT(CACHE_HOT_BUDDY, true)

/*
 * Allow wakeup-time preemption of the current task:
 */
SCHED_FEAT(WAKEUP_PREEMPTION, true)

SCHED_FEAT(HRTICK, false)
SCHED_FEAT(DOUBLE_TICK, false)
SCHED_FEAT(LB_BIAS, true)

/*
 * Decrement CPU capacity based on time not spent running tasks
 */
SCHED_FEAT(NONTASK_CAPACITY, true)

/*
 * Queue remote wakeups on the target CPU and process them
 * using the scheduler IPI. Reduces rq->lock contention/bounces.
 */
SCHED_FEAT(TTWU_QUEUE, false)

/*
 * When doing wakeups, attempt to limit superfluous scans of the LLC domain.
 */
SCHED_FEAT(SIS_AVG_CPU, false)

#ifdef HAVE_RT_PUSH_IPI
/*
 * In order to avoid a thundering herd attack of CPUs that are
 * lowering their priorities at the same time, and there being
 * a single CPU that has an RT task that can migrate and is waiting
 * to run, where the other CPUs will try to take that CPUs
 * rq lock and possibly create a large contention, sending an
 * IPI to that CPU and let that CPU push the RT task to where
 * it should go may be a better scenario.
 */
SCHED_FEAT(RT_PUSH_IPI, true)
#endif

SCHED_FEAT(FORCE_SD_OVERLAP, false)
SCHED_FEAT(RT_RUNTIME_SHARE, true)
SCHED_FEAT(LB_MIN, false)
SCHED_FEAT(ATTACH_AGE_LOAD, true)

/*
 * Energy aware scheduling. Use platform energy model to guide scheduling
 * decisions optimizing for energy efficiency.
 */
#ifdef CONFIG_DEFAULT_USE_ENERGY_AWARE
SCHED_FEAT(ENERGY_AWARE, true)
#else
SCHED_FEAT(ENERGY_AWARE, false)
#endif

/*
 * Minimum capacity capping. Keep track of minimum capacity factor when
 * minimum frequency available to a policy is modified.
 * If enabled, this can be used to inform the scheduler about capacity
 * restrictions.
 */
SCHED_FEAT(MIN_CAPACITY_CAPPING, false)

/*
 * Enforce the priority of candidates selected by find_best_target()
 * ON: If the target CPU saves any energy, use that.
 * OFF: Use whichever of target or backup saves most.
 */
SCHED_FEAT(FBT_STRICT_ORDER, false)
/*
 * Enforce schedtune.prefer_idle to take need_idle path.
 * ON: schedtune.prefer_idle is replaced with need_idle
 * OFF: schedtune.prefer_idle is honored as is.
 */
SCHED_FEAT(EAS_USE_NEED_IDLE, true)
