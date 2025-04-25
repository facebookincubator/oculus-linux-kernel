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
 * DOC:  qdf_threads
 * QCA driver framework (QDF) thread related APIs
 */

#if !defined(__QDF_THREADS_H)
#define __QDF_THREADS_H

#include <qdf_types.h>
#include "i_qdf_threads.h"

typedef __qdf_thread_t qdf_thread_t;
typedef QDF_STATUS (*qdf_thread_func)(void *context);

/* Function declarations and documentation */

void qdf_sleep(uint32_t ms_interval);

void qdf_sleep_us(uint32_t us_interval);

void qdf_busy_wait(uint32_t us_interval);

/**
 * qdf_set_wake_up_idle() - set wakeup idle value
 * @idle: true/false value for wake up idle
 *
 * Return: none
 */
void qdf_set_wake_up_idle(bool idle);

/**
 * qdf_set_user_nice() - set thread's nice value
 * @thread: pointer to thread
 * @nice: nice value
 *
 * Return: void
 */
void qdf_set_user_nice(qdf_thread_t *thread, long nice);

/**
 * qdf_create_thread() - create a kernel thread
 * @thread_handler: pointer to thread handler
 * @data: data
 * @thread_name: thread name
 *
 * Return: pointer to created kernel thread on success else NULL
 */
qdf_thread_t *qdf_create_thread(int (*thread_handler)(void *data), void *data,
				const char thread_name[]);

/**
 * qdf_thread_run() - run the given function in a new thread
 *
 * You must call qdf_thread_join() to avoid a reasource leak!
 * For more flexibility, use qdf_create_thread() instead.
 * @callback: callback function
 * @context: context
 *
 * Return: a new qdf_thread pointer
 */
qdf_thread_t *qdf_thread_run(qdf_thread_func callback, void *context);

/**
 * qdf_thread_join() - signal and wait for a thread to stop
 * @thread: pointer to thread
 *
 * This sets a flag that the given thread can check to see if it should exit.
 * The thread can check to see if this flag has been set by calling
 * qdf_thread_should_stop().
 *
 * Return: QDF_STATUS - the return value from the thread function
 */
QDF_STATUS qdf_thread_join(qdf_thread_t *thread);

/**
 * qdf_thread_should_stop() - true if the current thread was signalled to stop
 *
 * If qdf_thread_join() has been called on the current thread, this API returns
 * true. Otherwise, this returns false.
 *
 * Return: true if the current thread should stop
 */
bool qdf_thread_should_stop(void);

/**
 * qdf_wake_up_process() - wake up given thread
 * @thread: pointer to thread which needs to be woken up
 *
 * Return: none
 */
int qdf_wake_up_process(qdf_thread_t *thread);

/**
 * qdf_print_thread_trace() - prints the stack trace of the given thread
 * @thread: the thread for which the stack trace will be printed
 *
 * Return: None
 */
void qdf_print_thread_trace(qdf_thread_t *thread);

/**
 * qdf_get_current_task() - get current task struct
 *
 * Return: pointer to task struct
 */
qdf_thread_t *qdf_get_current_task(void);

/**
 * qdf_get_current_pid() - get current task's process id
 *
 * Return: current task's process id (int)
 */
int qdf_get_current_pid(void);

/**
 * qdf_get_current_comm() - get current task's command name
 *
 * Return: current task's command name(char *)
 */
const char *qdf_get_current_comm(void);

/**
 * qdf_thread_set_cpus_allowed_mask() - set cpu mask for a particular thread
 * @thread: thread for which new cpu mask is set
 * @new_mask: new cpu mask to be set for the thread
 *
 * Return: None
 */
void
qdf_thread_set_cpus_allowed_mask(qdf_thread_t *thread, qdf_cpu_mask *new_mask);

/**
 * qdf_cpumask_clear() - clear all cpus in a cpumask
 * @dstp: cpumask pointer
 *
 * Return: None
 */
void qdf_cpumask_clear(qdf_cpu_mask *dstp);

/**
 * qdf_cpumask_set_cpu() - set a cpu in a cpumask
 * @cpu: cpu number
 * @dstp: cpumask pointer
 *
 * Return: None
 */
void qdf_cpumask_set_cpu(unsigned int cpu, qdf_cpu_mask *dstp);

/**
 * qdf_cpumask_setall - set all cpus
 * @dstp: cpumask pointer
 *
 * Return: None
 */
void qdf_cpumask_setall(qdf_cpu_mask *dstp);

/**
 * qdf_cpumask_clear_cpu() - clear a cpu in a cpumask
 * @cpu: cpu number
 * @dstp: cpumask pointer
 *
 * Return: None
 */
void qdf_cpumask_clear_cpu(unsigned int cpu, qdf_cpu_mask *dstp);

/**
 * qdf_cpumask_empty - Check if cpu_mask is empty
 * @srcp: cpumask pointer
 *
 * Return: true or false
 *
 */
bool qdf_cpumask_empty(const qdf_cpu_mask *srcp);

/**
 * qdf_cpumask_copy - Copy srcp cpumask to dstp
 * @srcp: source cpumask pointer
 * @dstp: destination cpumask pointer
 *
 * Return: None
 *
 */
void qdf_cpumask_copy(qdf_cpu_mask *dstp,
		      const qdf_cpu_mask *srcp);

/**
 * qdf_cpumask_or - set *dstp = *src1p | *src2p
 * @dstp: the cpumask result
 * @src1p: the first input
 * @src2p: the second input
 *
 * Return: None
 */
void qdf_cpumask_or(qdf_cpu_mask *dstp, qdf_cpu_mask *src1p,
		    qdf_cpu_mask *src2p);

/**
 * qdf_thread_cpumap_print_to_pagebuf  - copies the cpumask into the buffer
 * either as comma-separated list of cpus or hex values of cpumask
 * @list: indicates whether the cpumap is list or not
 * @new_mask: the cpumask to copy
 * @new_mask_str: the buffer to copy into
 *
 * This functions copies the cpu mask set for the thread by
 * qdf_thread_set_cpus_allowed_mask() to new_mask_str
 *
 * Return: None
 */
void
qdf_thread_cpumap_print_to_pagebuf(bool list, char *new_mask_str,
				   qdf_cpu_mask *new_mask);

/**
 * qdf_cpumask_and - *dstp = *src1p & *src2p
 * @dstp: the cpumask result
 * @src1p: the first input
 * @src2p: the second input
 *
 * Return: If *@dstp is empty, returns false, else returns true
 */
bool
qdf_cpumask_and(qdf_cpu_mask *dstp, const qdf_cpu_mask *src1p,
		const qdf_cpu_mask *src2p);

/**
 * qdf_cpumask_andnot - *dstp = *src1p & ~*src2p
 * @dstp: the cpumask result
 * @src1p: the first input
 * @src2p: the second input
 *
 * Return: If *@dstp is empty, returns false, else returns true
 */
bool
qdf_cpumask_andnot(qdf_cpu_mask *dstp, const qdf_cpu_mask *src1p,
		   const qdf_cpu_mask *src2p);

/**
 * qdf_cpumask_equal - *src1p == *src2p
 * @src1p: the first input
 * @src2p: the second input
 *
 * Return: If *@src1p == *@src2p return true, else return false
 */
bool
qdf_cpumask_equal(const qdf_cpu_mask *src1p, const qdf_cpu_mask *src2p);

/**
 * qdf_cpumask_complement - *dstp = ~*srcp
 * @dstp: the cpumask result
 * @srcp: the input to invert
 *
 * Return: None
 */
void
qdf_cpumask_complement(qdf_cpu_mask *dstp, const qdf_cpu_mask *srcp);

#if defined(WALT_GET_CPU_TAKEN_SUPPORT) && IS_ENABLED(CONFIG_SCHED_WALT)
/**
 * qdf_walt_get_cpus_taken - Get taken CPUs
 *
 * Return: Taken CPUs
 */
qdf_cpu_mask qdf_walt_get_cpus_taken(void);

/*
 * qdf_walt_get_cpus_taken_supported: walt_get_cpus_taken supported
 *
 * Return: true if walt_get_cpus_taken API is supported
 */
static inline bool
qdf_walt_get_cpus_taken_supported(void)
{
	return true;
}
#else
static inline
qdf_cpu_mask qdf_walt_get_cpus_taken(void)
{
	qdf_cpu_mask mask;

	qdf_cpumask_clear(&mask);

	return mask;
}

static inline bool
qdf_walt_get_cpus_taken_supported(void)
{
	return false;
}
#endif
#endif /* __QDF_THREADS_H */
