// SPDX-License-Identifier: GPL-2.0-only
/*
 * Behavioural tests for the RT throttle dump.
 *
 * The dump feeds crash-report grouping, so the process name it records has to
 * be a real name and the contributor rows have to keep their shape. Both are
 * easy to break from code that looks unrelated, and neither is covered by
 * anything else.
 *
 * The hog here is a kthread, which has no mm, so reading its argv[0] fails --
 * the same condition that makes the dump fall back to comm for a process
 * whose group leader has already exited. Before that fallback existed the
 * dump recorded the literal string "unavailable"; test_process_name_is_real()
 * is what pins it.
 *
 * All privileged work happens in suite_init(): KUnit runs each case in a
 * kthread that cannot open /proc/sys (-EACCES), and driving one throttle for
 * the whole suite costs a CPU for one RT period instead of three. The cases
 * are pure assertions over what suite_init() captured.
 *
 * The suite skips rather than runs if panic_on_throttle is armed, because
 * tripping the throttle would then panic the device instead of failing a test.
 *
 * Not covered: the case where the cmdline read succeeds but argv[0] is itself
 * the empty string. get_task_mm() returns NULL for a kthread, so the hog here
 * always takes the read-failed path; reaching the other one needs a userspace
 * process exec'd with argv[0] == "", which a test module has no business
 * spawning at real-time priority. test_row_name_matches_process_name() covers
 * the adjacent risk -- the two copies of the name waterfall drifting apart.
 */

#include <kunit/test.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kstrtox.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <linux/string.h>

#define SYSCTL_DIR		"/proc/sys/sched_rt/"
#define COMM_PREFIX		"rt_throttle_"
/*
 * The hog carries an embedded newline so every assertion below also proves
 * the dump sanitized it. comm holds 15 characters, so the name is sized to
 * fit the control character rather than have it truncated away.
 */
#define HOG_COMM		"rt_throttle_h\ng"
#define HOG_COMM_CLEAN		"rt_throttle_h?g"
#define PULSE_COMM		"rt_throttle_pulse"
/*
 * The budget is per period, not cumulative, so the hog has to saturate one
 * whole period on its own. At 1500ms a run that straddles a period boundary
 * splits into two ~700ms halves, neither of which trips the 950ms default,
 * and the suite skips. Three periods leaves a full one in the middle however
 * the run lands.
 */
#define HOG_RUN_MS		3000
/* Brief, frequent preemption of the hog so slices complete and get accounted. */
#define PULSE_RUN_MS		2
#define PULSE_IDLE_MS		40
#define THROTTLE_WAIT_MS	5000

static char captured_process_name[64];
static char captured_thread_name[64];
static char captured_pid[16];
static char captured_rows[1280];	/* matches SYSCTL_TOP_CONTRIB_LEN */
static unsigned int captured_period_ms;
static const char *skip_reason;

static int sysctl_read(const char *name, char *buf, size_t sz)
{
	char path[64];
	struct file *f;
	loff_t pos = 0;
	ssize_t n;

	if (name[0] == '/')
		strscpy(path, name, sizeof(path));
	else
		scnprintf(path, sizeof(path), SYSCTL_DIR "%s", name);
	f = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(f))
		return PTR_ERR(f);

	n = kernel_read(f, buf, sz - 1, &pos);
	filp_close(f, NULL);
	if (n < 0)
		return n;

	buf[n] = '\0';
	strim(buf);
	return 0;
}

static int sysctl_write(const char *name, const char *val)
{
	char path[64];
	struct file *f;
	loff_t pos = 0;
	ssize_t n;

	scnprintf(path, sizeof(path), SYSCTL_DIR "%s", name);
	f = filp_open(path, O_WRONLY, 0);
	if (IS_ERR(f))
		return PTR_ERR(f);

	n = kernel_write(f, val, strlen(val), &pos);
	filp_close(f, NULL);
	return n < 0 ? n : 0;
}

static void spin_until(ktime_t deadline)
{
	while (!kthread_should_stop() && ktime_before(ktime_get(), deadline))
		cpu_relax();
}

/* Park until kthread_stop(), so the parent can always join us. */
static void park_until_stopped(void)
{
	while (!kthread_should_stop())
		schedule_timeout_interruptible(1);
}

static int rt_hog_fn(void *arg)
{
	spin_until(ktime_add_ms(ktime_get(), HOG_RUN_MS));
	park_until_stopped();
	return 0;
}

/*
 * The contributor table accounts a tgid only when one of its slices ends, and
 * a slice ends when the task is descheduled. A lone hog runs uninterrupted
 * until the throttle trips, so nothing has been accounted by the time the
 * dump runs and the table comes out empty. This pulse preempts the hog a few
 * times per period -- the same thing a real workload's higher-priority RT
 * threads do -- so there are completed slices to report.
 */
static int rt_pulse_fn(void *arg)
{
	ktime_t deadline = ktime_add_ms(ktime_get(), HOG_RUN_MS);

	while (!kthread_should_stop() && ktime_before(ktime_get(), deadline)) {
		spin_until(ktime_add_ms(ktime_get(), PULSE_RUN_MS));
		schedule_timeout_interruptible(msecs_to_jiffies(PULSE_IDLE_MS));
	}

	park_until_stopped();
	return 0;
}

/* Pin a FIFO hog to one CPU until sched_rt latches a throttle, or give up. */
static int drive_one_throttle(void)
{
	struct task_struct *hog, *pulse;
	char buf[16];
	unsigned long end;
	int cpu;
	int ret;

	ret = sysctl_write("clear_data", "1\n");
	if (ret)
		return ret;

	/*
	 * Writing clear_data runs reset_sysctl_to_defaults(), which re-arms
	 * panic_on_throttle to its build default -- 1 on userdebug. Disarm
	 * after clearing, never before, and confirm it took: getting this
	 * order wrong panics the device on the throttle below instead of
	 * reporting a test failure.
	 */
	ret = sysctl_write("panic_on_throttle", "0\n");
	if (ret)
		return ret;
	ret = sysctl_read("panic_on_throttle", buf, sizeof(buf));
	if (ret)
		return ret;
	if (strcmp(buf, "0"))
		return -EPERM;

	cpu = cpumask_last(cpu_online_mask);

	hog = kthread_create(rt_hog_fn, NULL, HOG_COMM);
	if (IS_ERR(hog))
		return PTR_ERR(hog);

	pulse = kthread_create(rt_pulse_fn, NULL, PULSE_COMM);
	if (IS_ERR(pulse)) {
		kthread_stop(hog);
		return PTR_ERR(pulse);
	}

	kthread_bind(hog, cpu);
	kthread_bind(pulse, cpu);

	/*
	 * Set both policies from here, before either runs. A thread cannot
	 * raise itself to FIFO once the hog is spinning: it would still be
	 * CFS at that point and never get the CPU to make the call.
	 */
	sched_set_fifo_low(hog);
	sched_set_fifo(pulse);

	wake_up_process(hog);
	wake_up_process(pulse);

	ret = -ETIMEDOUT;
	end = jiffies + msecs_to_jiffies(THROTTLE_WAIT_MS);
	while (time_before(jiffies, end)) {
		msleep(100);
		if (sysctl_read("data_latched", buf, sizeof(buf)))
			continue;
		if (!strcmp(buf, "1")) {
			ret = 0;
			break;
		}
	}

	kthread_stop(pulse);
	kthread_stop(hog);
	return ret;
}

static int rt_throttle_suite_init(struct kunit_suite *suite)
{
	char entry_panic[16];
	char period_buf[32];
	int ret;

	/*
	 * Require an explicit opt-in even though drive_one_throttle() disarms
	 * the panic itself. This suite deliberately trips a throttle, so the
	 * operator should have said so before we start.
	 */
	ret = sysctl_read("panic_on_throttle", entry_panic, sizeof(entry_panic));
	if (ret) {
		skip_reason = "cannot read " SYSCTL_DIR "panic_on_throttle";
		return 0;
	}

	if (strcmp(entry_panic, "0")) {
		skip_reason = "panic_on_throttle is armed; run 'echo 0 > "
			      SYSCTL_DIR "panic_on_throttle' first and restore it after";
		return 0;
	}

	ret = drive_one_throttle();
	if (ret) {
		skip_reason = "could not drive an RT throttle on this system";
		goto restore;
	}

	if (!sysctl_read("/proc/sys/kernel/sched_rt_period_us", period_buf,
			 sizeof(period_buf)) &&
	    !kstrtouint(period_buf, 10, &captured_period_ms))
		captured_period_ms = DIV_ROUND_UP(captured_period_ms,
						  USEC_PER_MSEC);

	if (sysctl_read("process_name", captured_process_name,
			sizeof(captured_process_name)) ||
	    sysctl_read("thread_name", captured_thread_name,
			sizeof(captured_thread_name)) ||
	    sysctl_read("pid", captured_pid, sizeof(captured_pid)) ||
	    sysctl_read("top_contributors", captured_rows, sizeof(captured_rows)))
		skip_reason = "could not read back the latched sched_rt data";

restore:
	/* Leave the latch clean, then put panic_on_throttle back as found. */
	sysctl_write("clear_data", "1\n");
	strlcat(entry_panic, "\n", sizeof(entry_panic));
	/*
	 * clear_data re-arms panic_on_throttle to the build default, so a
	 * failure here leaves the device in the opposite state to the one it
	 * started in. Say so rather than restoring silently.
	 */
	if (sysctl_write("panic_on_throttle", entry_panic))
		pr_err("rt_throttle_test: could not restore panic_on_throttle\n");
	return 0;
}

/*
 * The hog has no mm, so its argv[0] cannot be read. The dump has to fall back
 * to comm rather than recording "unavailable", which is the only identifier a
 * crash report keeps for the throttling process.
 */
static void test_process_name_is_real(struct kunit *test)
{
	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);

	KUNIT_EXPECT_STRNEQ(test, captured_process_name, "unavailable");
	KUNIT_EXPECT_STRNEQ(test, captured_process_name, "");
	/* Either of our two RT threads may be current when the throttle trips. */
	KUNIT_EXPECT_TRUE_MSG(test,
			      !strncmp(captured_process_name, COMM_PREFIX,
				       strlen(COMM_PREFIX)),
			      "process_name was '%s'", captured_process_name);
}

/* The thread name is recorded separately and must also survive. */
static void test_thread_name_is_real(struct kunit *test)
{
	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);

	KUNIT_EXPECT_STRNEQ(test, captured_thread_name, "");
	KUNIT_EXPECT_TRUE_MSG(test,
			      !strncmp(captured_thread_name, COMM_PREFIX,
				       strlen(COMM_PREFIX)),
			      "thread_name was '%s'", captured_thread_name);
}

/*
 * Contributor rows are "<name> <tgid> <runtime_ms> <slices> <max_ms>". The
 * report preprocessor needs the counters to follow the tgid immediately, so a
 * row with a blank name or a missing column silently stops the table parse.
 */
static void test_contributor_row_shape(struct kunit *test)
{
	char rows[sizeof(captured_rows)];
	char name[64], top[64];
	unsigned int tgid, ms, slices, max_ms;
	char *cursor = rows;
	char *line;

	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);

	strscpy(rows, captured_rows, sizeof(rows));
	KUNIT_ASSERT_STRNEQ(test, rows, "");

	line = strsep(&cursor, "\n");
	KUNIT_ASSERT_NOT_NULL(test, line);

	KUNIT_EXPECT_EQ_MSG(test,
			    sscanf(line, "%63s %u %u %u %u %63s",
				   name, &tgid, &ms, &slices, &max_ms, top),
			    6,
			    "row did not parse: '%s'", line);
	KUNIT_EXPECT_STRNEQ(test, name, "");
	KUNIT_EXPECT_GT(test, tgid, 0u);
	KUNIT_EXPECT_GT(test, slices, 0u);
}

/*
 * The last column names the thread that ran the longest single slice in the
 * process. Each hog here is its own thread group, so the pick is trivial and
 * has to come out equal to the process name; choosing between several threads
 * of one process needs a multi-threaded process and is covered by the
 * userspace repro, not by this suite.
 */
static void test_top_thread_is_named(struct kunit *test)
{
	char rows[sizeof(captured_rows)];
	char name[64], top[64];
	unsigned int tgid, ms, slices, max_ms;
	char *cursor = rows;
	char *line;

	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);

	strscpy(rows, captured_rows, sizeof(rows));
	line = strsep(&cursor, "\n");
	KUNIT_ASSERT_NOT_NULL(test, line);
	KUNIT_ASSERT_EQ_MSG(test,
			    sscanf(line, "%63s %u %u %u %u %63s",
				   name, &tgid, &ms, &slices, &max_ms, top),
			    6,
			    "row did not parse: '%s'", line);

	KUNIT_EXPECT_STRNEQ(test, top, "");
	KUNIT_EXPECT_STRNEQ(test, top, "unavailable");
	KUNIT_EXPECT_STREQ_MSG(test, top, name,
			       "single-threaded process should name itself");
}

/* Every row must carry a top_thread, not just the first. */
static void test_top_thread_on_every_row(struct kunit *test)
{
	char rows[sizeof(captured_rows)];
	char *cursor = rows;
	char *line;
	int checked = 0;

	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);

	strscpy(rows, captured_rows, sizeof(rows));
	while ((line = strsep(&cursor, "\n")) && *line) {
		char name[64], top[64];
		unsigned int tgid, ms, slices, max_ms;

		KUNIT_ASSERT_EQ_MSG(test,
				    sscanf(line, "%63s %u %u %u %u %63s",
					   name, &tgid, &ms, &slices,
					   &max_ms, top),
				    6,
				    "row did not parse: '%s'", line);
		KUNIT_EXPECT_STRNEQ(test, top, "");
		checked++;
	}
	KUNIT_EXPECT_GT(test, checked, 0);
}

/*
 * The hog here is a kthread, so it has no argv[0]: the hog line falls back to
 * the group leader's comm and the rows carry the snapshotted comm, and the two
 * must agree. They have disagreed before, which is what this catches. A
 * userspace hog resolves the hog line to argv[0] while rows keep comm, so the
 * two legitimately differ there -- that case belongs to the shell test.
 */
static void test_row_name_matches_process_name(struct kunit *test)
{
	char rows[sizeof(captured_rows)];
	char name[64], top[64];
	unsigned int tgid, ms, slices, max_ms, want;
	char *cursor = rows;
	char *line;
	bool found = false;

	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);

	KUNIT_ASSERT_EQ_MSG(test, kstrtouint(captured_pid, 10, &want), 0,
			    "pid sysctl was '%s'", captured_pid);

	strscpy(rows, captured_rows, sizeof(rows));
	while ((line = strsep(&cursor, "\n")) && *line) {
		if (sscanf(line, "%63s %u %u %u %u %63s",
			   name, &tgid, &ms, &slices, &max_ms, top) != 6)
			continue;
		if (tgid != want)
			continue;
		found = true;
		KUNIT_EXPECT_STREQ_MSG(test, name, captured_process_name,
				       "row and hog line disagree for tgid %u",
				       tgid);
	}
	KUNIT_EXPECT_TRUE_MSG(test, found,
			      "no contributor row for throttling tgid %u", want);
}

/*
 * Per-period accounting must stay inside the period. A slice that straddles the
 * period boundary is re-based by the period timer; if that re-base is not atomic
 * with the table reset against the rq-side slice hooks, slice_end can charge the
 * whole pre-boundary slice into the period that just started, and a row then
 * reports more runtime than a period contains.
 */
static void test_row_runtime_within_period(struct kunit *test)
{
	char rows[sizeof(captured_rows)];
	char name[64], top[64];
	unsigned int tgid, ms, slices, max_ms;
	unsigned int period_ms = captured_period_ms;
	char *cursor = rows;
	char *line;

	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);
	if (!period_ms)
		kunit_skip(test, "cannot read sched_rt_period_us");

	strscpy(rows, captured_rows, sizeof(rows));
	while ((line = strsep(&cursor, "\n")) && *line) {
		if (sscanf(line, "%63s %u %u %u %u %63s",
			   name, &tgid, &ms, &slices, &max_ms, top) != 6)
			continue;
		KUNIT_EXPECT_LE_MSG(test, max_ms, period_ms,
				    "max_slice %ums exceeds the %ums period: '%s'",
				    max_ms, period_ms, line);
		KUNIT_EXPECT_LE_MSG(test, ms, period_ms,
				    "runtime %ums exceeds the %ums period: '%s'",
				    ms, period_ms, line);
	}
}

/*
 * comm is user-settable via prctl(PR_SET_NAME) and /proc/self/comm, and the
 * kernel does not reject control characters. A newline in a name splits a
 * contributor row in two, so a line-oriented consumer reads a row that is not
 * a row, and the blame string is the crash-grouping key. The hog is named
 * with an embedded newline so this is exercised end to end.
 */
static void test_control_chars_are_sanitized(struct kunit *test)
{
	const char *c;

	if (skip_reason)
		kunit_skip(test, "%s", skip_reason);

	/*
	 * Assert on the rows, not on thread_name: either RT thread may be
	 * current when the throttle trips, but the hog is always the top
	 * contributor, so its sanitized name is always in the table.
	 */
	KUNIT_EXPECT_TRUE_MSG(test, strstr(captured_rows, HOG_COMM_CLEAN),
			      "no '%s' in rows:\n%s", HOG_COMM_CLEAN,
			      captured_rows);

	for (c = captured_rows; *c; c++)
		KUNIT_ASSERT_FALSE_MSG(test,
				       (unsigned char)*c < 0x20 && *c != '\n',
				       "control char 0x%02x in the rows",
				       (unsigned char)*c);
}

static struct kunit_case rt_throttle_cases[] = {
	KUNIT_CASE(test_process_name_is_real),
	KUNIT_CASE(test_thread_name_is_real),
	KUNIT_CASE(test_contributor_row_shape),
	KUNIT_CASE(test_top_thread_is_named),
	KUNIT_CASE(test_top_thread_on_every_row),
	KUNIT_CASE(test_row_name_matches_process_name),
	KUNIT_CASE(test_row_runtime_within_period),
	KUNIT_CASE(test_control_chars_are_sanitized),
	{}
};

static struct kunit_suite rt_throttle_suite = {
	.name = "rt_throttle",
	.suite_init = rt_throttle_suite_init,
	.test_cases = rt_throttle_cases,
};

kunit_test_suite(rt_throttle_suite);

MODULE_LICENSE("GPL");
