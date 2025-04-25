/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: qdf_time
 * This file abstracts time related functionality.
 */

#ifndef _QDF_OS_TIME_H
#define _QDF_OS_TIME_H

#include <i_qdf_time.h>

typedef __qdf_time_t qdf_time_t;
typedef __qdf_ktime_t qdf_ktime_t;
typedef __qdf_timespec_t qdf_timespec_t;
typedef __qdf_work_struct_t qdf_work_struct_t;

#define qdf_time_uint_to_ms(tu) (((tu) * 1024) / 1000)

#ifdef ENHANCED_OS_ABSTRACTION
/**
 * qdf_ns_to_ktime() - Converts nanoseconds to a qdf_ktime_t object
 * @ns: time in nanoseconds
 *
 * Return: nanoseconds as qdf_ktime_t object
 */
qdf_ktime_t qdf_ns_to_ktime(uint64_t ns);

/**
 * qdf_ktime_add() - Adds two qdf_ktime_t objects and returns
 * a qdf_ktime_t object
 * @ktime1: time as qdf_ktime_t object
 * @ktime2: time as qdf_ktime_t object
 *
 * Return: sum of both qdf_ktime_t as qdf_ktime_t object
 */
qdf_ktime_t qdf_ktime_add(qdf_ktime_t ktime1, qdf_ktime_t ktime2);

/**
 * qdf_ktime_get() - Gets the current time as qdf_ktime_t object
 *
 * Return: current time as qdf_ktime_t object
 */
qdf_ktime_t qdf_ktime_get(void);

/**
 * qdf_ktime_real_get() - Gets the current wall clock as qdf_ktime_t object
 *
 * Return: current wall clock as qdf_ktime_t object
 */
qdf_ktime_t qdf_ktime_real_get(void);

/**
 * qdf_ktime_add_ns() - Adds qdf_ktime_t object and nanoseconds value and
 * returns the qdf_ktime_t object
 * @ktime: time as qdf_ktime_t object
 * @ns: time in nanoseconds
 *
 * Return: qdf_ktime_t object
 */
qdf_ktime_t qdf_ktime_add_ns(qdf_ktime_t ktime, int64_t ns);

/**
 * qdf_ktime_to_ms() - Convert the qdf_ktime_t object into milliseconds
 * @ktime: time as qdf_ktime_t object
 *
 * Return: qdf_ktime_t in milliseconds
 */
int64_t qdf_ktime_to_ms(qdf_ktime_t ktime);

/**
 * qdf_ktime_to_us() - Convert the qdf_ktime_t object into microseconds
 * @ktime: time as qdf_ktime_t object
 *
 * Return: qdf_ktime_t in microseconds
 */
int64_t qdf_ktime_to_us(qdf_ktime_t ktime);

/**
 * qdf_ktime_to_ns() - Convert the qdf_ktime_t object into nanoseconds
 * @ktime: time as qdf_ktime_t object
 *
 * Return: qdf_ktime_t in nanoseconds
 */
int64_t qdf_ktime_to_ns(qdf_ktime_t ktime);

/**
 * qdf_time_ktime_set() - Set a ktime_t variable from a seconds/nanoseconds
 * value
 * @secs: seconds to set
 * @nsecs: nanoseconds to set
 *
 * Return: The qdf_ktime_t representation of the value.
 */
qdf_ktime_t qdf_time_ktime_set(const s64 secs, const unsigned long nsecs);

/**
 * qdf_ktime_get_real_ns() - Gets the current time in ns using UTC
 *
 * Return: qdf_ktime_t in nano sec
 */
qdf_ktime_t qdf_ktime_get_real_ns(void);

/**
 * qdf_ktime_get_ns() - Gets the current time nano seconds
 *
 * Return: qdf_ktime_t in nano sec
 */
qdf_ktime_t qdf_ktime_get_ns(void);

/**
 * qdf_system_ticks - Count the number of ticks elapsed from the time when
 * the system booted
 *
 * Return: ticks
 */
qdf_time_t qdf_system_ticks(void);

#define qdf_system_ticks_per_sec __qdf_system_ticks_per_sec

/**
 * qdf_system_ticks_to_msecs() - convert ticks to milliseconds
 * @clock_ticks: Number of ticks
 *
 * Return: unsigned int Time in milliseconds
 */
uint32_t qdf_system_ticks_to_msecs(unsigned long clock_ticks);

/**
 * qdf_system_ticks_to_nsecs() - convert ticks to nanoseconds
 * @clock_ticks: Number of ticks
 *
 * Return: unsigned int Time in nanoseconds
 */
uint32_t qdf_system_ticks_to_nsecs(unsigned long clock_ticks);

/**
 * qdf_system_msecs_to_ticks() - convert milliseconds to ticks
 * @msecs: Time in milliseconds
 *
 * Return: unsigned long number of ticks
 */
qdf_time_t qdf_system_msecs_to_ticks(uint32_t msecs);

/**
 * qdf_get_system_uptime() - Return a monotonically increasing time
 * This increments once per HZ ticks
 *
 * Return: qdf_time_t system up time in ticks
 */
qdf_time_t qdf_get_system_uptime(void);

/**
 * qdf_get_bootbased_boottime_ns() - Get the bootbased time in nanoseconds
 *
 * qdf_get_bootbased_boottime_ns() function returns the number of nanoseconds
 * that have elapsed since the system was booted. It also includes the time when
 * system was suspended.
 *
 * Return:
 * The time since system booted in nanoseconds
 */
uint64_t qdf_get_bootbased_boottime_ns(void);

/**
 * qdf_get_system_timestamp() - Return current timestamp
 *
 * Return: unsigned long timestamp in ms.
 */
unsigned long qdf_get_system_timestamp(void);

/**
 * qdf_udelay() - delay in microseconds
 * @usecs: Number of microseconds to delay
 *
 * Return: none
 */
void qdf_udelay(int usecs);

/**
 * qdf_mdelay() - Delay in milliseconds.
 * @msecs: Number of milliseconds to delay
 *
 * Return: none
 */
void qdf_mdelay(int msecs);

/**
 * qdf_system_time_after() - Check if a is later than b
 * @a: Time stamp value a
 * @b: Time stamp value b
 *
 * Return: true if a < b else false
 */
bool qdf_system_time_after(qdf_time_t a, qdf_time_t b);

/**
 * qdf_system_time_before() - Check if a is before b
 * @a: Time stamp value a
 * @b: Time stamp value b
 *
 * Return: true if a is before b else false
 */
bool qdf_system_time_before(qdf_time_t a, qdf_time_t b);

/**
 * qdf_system_time_after_eq() - Check if a atleast as recent as b, if not
 * later
 * @a: Time stamp value a
 * @b: Time stamp value b
 *
 * Return: true if a >= b else false
 */
bool qdf_system_time_after_eq(qdf_time_t a, qdf_time_t b);

/**
 * enum qdf_timestamp_unit - what unit the qdf timestamp is in
 * @KERNEL_LOG: boottime time in uS (micro seconds)
 * @QTIMER: QTIME in (1/19200)S
 *
 * This enum is used to distinguish which timer source is used.
 */
enum qdf_timestamp_unit {
	KERNEL_LOG,
	QTIMER,
};

#ifdef MSM_PLATFORM
#define QDF_LOG_TIMESTAMP_UNIT QTIMER
#define QDF_LOG_TIMESTAMP_CYCLES_PER_10_US 192
#else
#define QDF_LOG_TIMESTAMP_UNIT KERNEL_LOG
#define QDF_LOG_TIMESTAMP_CYCLES_PER_10_US 10
#endif /* end of MSM_PLATFORM */

uint64_t qdf_log_timestamp_to_usecs(uint64_t time);

/**
 * qdf_log_timestamp_to_secs() - get time stamp for logging in seconds
 * @time: logging timestamp
 * @secs: pointer to write seconds
 * @usecs: pointer to write microseconds
 *
 * Return: void. The normalized time is returned in @secs and @usecs
 */
void qdf_log_timestamp_to_secs(uint64_t time, uint64_t *secs,
			       uint64_t *usecs);

uint64_t qdf_usecs_to_log_timestamp(uint64_t usecs);

/**
 * qdf_get_log_timestamp() - get time stamp for logging
 * For adrastea this API returns QTIMER tick which is needed to synchronize
 * host and fw log timestamps
 * For ROME and other discrete solution this API returns system boot time stamp
 *
 * Return:
 * QTIMER ticks(19.2MHz) for adrastea
 * System tick for rome and other future discrete solutions
 */
uint64_t qdf_get_log_timestamp(void);

/**
 * qdf_get_log_timestamp_usecs() - get time stamp for logging in microseconds
 *
 * Return: The current logging timestamp normalized to microsecond precision
 */
uint64_t qdf_get_log_timestamp_usecs(void);

/**
 * qdf_get_log_timestamp_lightweight() - get time stamp for logging
 */
#define qdf_get_log_timestamp_lightweight() qdf_get_log_timestamp()

/**
 * qdf_get_monotonic_boottime() - get monotonic kernel boot time
 * This API is similar to qdf_get_system_boottime but it includes
 * time spent in suspend.
 *
 * Return: Time in microseconds
 */
uint64_t qdf_get_monotonic_boottime(void);

/**
 * qdf_time_ktime_get_real_time() - Get the time of day in qdf_timespec_t
 * @ts: pointer to the qdf_timespec_t
 *
 * Return: None
 */
void qdf_time_ktime_get_real_time(qdf_timespec_t *ts);

/**
 * qdf_time_sched_clock() - scheduler clock
 *
 * Return: current time in nanosec units.
 */
unsigned long long qdf_time_sched_clock(void);

/**
 * qdf_usleep_range - introduce sleep with min and max time
 * @min: Minimum time in usecs to sleep
 * @max: Maximum time in usecs to sleep
 *
 * Return: none
 */
void qdf_usleep_range(unsigned long min, unsigned long max);

/**
 *  qdf_ktime_compare - compare two qdf_ktime_t objects
 *  @ktime1: time as qdf_ktime_t object
 *  @ktime2: time as qdf_ktime_t object
 *
 *  Return:
 * * ktime1  < ktime2 - return <0
 * * ktime1 == ktime2 - return 0
 * * ktime1  > ktime2 - return >0
 */
int qdf_ktime_compare(qdf_ktime_t ktime1, qdf_ktime_t ktime2);

#else
static inline qdf_ktime_t qdf_ns_to_ktime(uint64_t ns)
{
	return __qdf_ns_to_ktime(ns);
}

static inline qdf_ktime_t qdf_ktime_add(qdf_ktime_t ktime1, qdf_ktime_t ktime2)
{
	return __qdf_ktime_add(ktime1, ktime2);
}

static inline qdf_ktime_t qdf_ktime_get(void)
{
	return __qdf_ktime_get();
}

static inline qdf_ktime_t qdf_ktime_real_get(void)
{
	return __qdf_ktime_real_get();
}

static inline qdf_ktime_t qdf_ktime_get_real_ns(void)
{
	return __qdf_ktime_get_real_ns();
}

static inline uint64_t qdf_ktime_get_ns(void)
{
	return __qdf_ktime_get_ns();
}

static inline qdf_ktime_t qdf_ktime_compare(qdf_ktime_t ktime1,
					    qdf_ktime_t ktime2)
{
	return __qdf_ktime_compare(ktime1, ktime2);
}

static inline qdf_ktime_t qdf_ktime_add_ns(qdf_ktime_t ktime, int64_t ns)
{
	return __qdf_ktime_add_ns(ktime, ns);
}

static inline int64_t qdf_ktime_to_ms(qdf_ktime_t ktime)
{
	return __qdf_ktime_to_ms(ktime);
}

static inline int64_t qdf_ktime_to_us(qdf_ktime_t ktime)
{
	return __qdf_time_ktime_to_us(ktime);
}

static inline int64_t qdf_ktime_to_ns(qdf_ktime_t ktime)
{
	return __qdf_ktime_to_ns(ktime);
}

static inline qdf_time_t qdf_system_ticks(void)
{
	return __qdf_system_ticks();
}

#define qdf_system_ticks_per_sec __qdf_system_ticks_per_sec
static inline uint32_t qdf_system_ticks_to_msecs(unsigned long clock_ticks)
{
	return __qdf_system_ticks_to_msecs(clock_ticks);
}

static inline qdf_time_t qdf_system_msecs_to_ticks(uint32_t msecs)
{
	return __qdf_system_msecs_to_ticks(msecs);
}

static inline qdf_time_t qdf_get_system_uptime(void)
{
	return __qdf_get_system_uptime();
}

static inline uint64_t qdf_get_bootbased_boottime_ns(void)
{
	return __qdf_get_bootbased_boottime_ns();
}

static inline unsigned long qdf_get_system_timestamp(void)
{
	return __qdf_get_system_timestamp();
}

static inline void qdf_udelay(int usecs)
{
	__qdf_udelay(usecs);
}

static inline void qdf_mdelay(int msecs)
{
	__qdf_mdelay(msecs);
}

static inline bool qdf_system_time_after(qdf_time_t a, qdf_time_t b)
{
	return __qdf_system_time_after(a, b);
}

static inline bool qdf_system_time_before(qdf_time_t a, qdf_time_t b)
{
	return __qdf_system_time_before(a, b);
}

static inline bool qdf_system_time_after_eq(qdf_time_t a, qdf_time_t b)
{
	return __qdf_system_time_after_eq(a, b);
}

/**
 * qdf_sched_clock() - use light weight timer to get timestamp for logging
 *
 * Return: timestamp in ns
 */
static inline uint64_t qdf_sched_clock(void)
{
	return __qdf_sched_clock();
}

/**
 * enum qdf_timestamp_unit - what unit the qdf timestamp is in
 * @KERNEL_LOG: boottime time in uS (micro seconds)
 * @QTIMER: QTIME in (1/19200)S
 *
 * This enum is used to distinguish which timer source is used.
 */
enum qdf_timestamp_unit {
	KERNEL_LOG,
	QTIMER,
};

#ifdef MSM_PLATFORM
#define QDF_LOG_TIMESTAMP_UNIT QTIMER
#define QDF_LOG_TIMESTAMP_CYCLES_PER_10_US 192

static inline uint64_t qdf_log_timestamp_to_usecs(uint64_t time)
{
	/*
	 * Try to preserve precision by multiplying by 10 first.
	 * If that would cause a wrap around, divide first instead.
	 */
	if (time * 10 < time) {
		do_div(time, QDF_LOG_TIMESTAMP_CYCLES_PER_10_US);
		return time * 10;
	}

	time = time * 10;
	do_div(time, QDF_LOG_TIMESTAMP_CYCLES_PER_10_US);

	return time;
}

/**
 * qdf_get_log_timestamp_lightweight() - get time stamp for logging
 * For adrastea this API returns QTIMER tick which is needed to synchronize
 * host and fw log timestamps
 * For ROME and other discrete solution this API returns system boot time stamp
 *
 * Return:
 * QTIMER ticks(19.2MHz) for adrastea
 * System tick for rome and other 3rd party platform solutions
 */
static inline uint64_t qdf_get_log_timestamp_lightweight(void)
{
	return __qdf_get_log_timestamp();
}
#else
#define QDF_LOG_TIMESTAMP_UNIT KERNEL_LOG
#define QDF_LOG_TIMESTAMP_CYCLES_PER_10_US 10

static inline uint64_t qdf_log_timestamp_to_usecs(uint64_t time)
{
	/* timestamps are already in micro seconds */
	return time;
}

static inline uint64_t qdf_get_log_timestamp_lightweight(void)
{
	uint64_t timestamp_us;

	/* explicitly change to uint64_t, otherwise it will assign
	 * uint32_t to timestamp_us, which lose high 32bits.
	 * on 64bit platform, it will only use low 32bits jiffies in
	 * jiffies_to_msecs.
	 * eg: HZ=250, it will overflow every (0xffff ffff<<2==0x3fff ffff)
	 * ticks. it is 1193 hours.
	 */
	timestamp_us =
	(uint64_t)__qdf_system_ticks_to_msecs(qdf_system_ticks()) * 1000;
	return timestamp_us;
}
#endif /* end of MSM_PLATFORM */

static inline void qdf_log_timestamp_to_secs(uint64_t time, uint64_t *secs,
					     uint64_t *usecs)
{
	*secs = qdf_log_timestamp_to_usecs(time);
	*usecs = do_div(*secs, 1000000ul);
}

static inline uint64_t qdf_usecs_to_log_timestamp(uint64_t usecs)
{
	return (usecs * QDF_LOG_TIMESTAMP_CYCLES_PER_10_US) / 10;
}

static inline uint64_t qdf_get_log_timestamp(void)
{
	return __qdf_get_log_timestamp();
}

static inline uint64_t qdf_get_log_timestamp_usecs(void)
{
	return qdf_log_timestamp_to_usecs(qdf_get_log_timestamp());
}

static inline uint64_t qdf_get_monotonic_boottime(void)
{
	return __qdf_get_monotonic_boottime();
}

static inline void qdf_time_ktime_get_real_time(qdf_timespec_t *ts)
{
	return __qdf_time_ktime_get_real_time(ts);
}

static inline unsigned long long qdf_time_sched_clock(void)
{
	return __qdf_time_sched_clock();
}

static inline void qdf_usleep_range(unsigned long min, unsigned long max)
{
	__qdf_usleep_range(min, max);
}
#endif
#endif
