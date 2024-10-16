/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _CAM_REQ_MGR_WORKER_WRAPPER_H_
#define _CAM_REQ_MGR_WORKER_WRAPPER_H_

#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/init.h>
#include<linux/sched.h>
#include<linux/workqueue.h>
#include<linux/slab.h>
#include<linux/timer.h>
#include<linux/kthread.h>
#include <linux/cpumask.h>
#include <uapi/linux/sched/types.h>

#include "media/cam_req_mgr.h"

/* Threshold for scheduling delay in ms */
#define CAM_WORKER_SCHEDULE_TIME_THRESHOLD   5

/* Threshold for execution delay in ms */
#define CAM_WORKER_EXE_TIME_THRESHOLD        10

/* Flag to create a high priority worker */
#define CAM_WORKER_FLAG_HIGH_PRIORITY             (1 << 0)

/*
 * This flag ensures only one task from a given
 * worker will execute at any given point on any
 * given CPU.
 */
#define CAM_WORKER_FLAG_SERIAL                    (1 << 1)

/* workqueue will be used from irq context or not */
enum crm_worker_context {
	CRM_WORKER_USAGE_NON_IRQ,
	CRM_WORKER_USAGE_IRQ,
	CRM_WORKER_USAGE_INVALID,
};

/* Task priorities, lower the number higher the priority*/
enum crm_task_priority {
	CRM_TASK_PRIORITY_0,
	CRM_TASK_PRIORITY_1,
	CRM_TASK_PRIORITY_MAX,
};

/**
 * struct cam_req_mgr_core_workq_mini_dump
 * @worker_scheduled_ts: scheduled ts
 * task -
 * @pending_cnt : # of tasks left in queue
 * @free_cnt    : # of free/available tasks
 * @num_task    : size of tasks pool
 */
struct cam_req_mgr_core_worker_mini_dump {
	ktime_t                    worker_scheduled_ts;
	/* tasks */
	struct {
		uint32_t               pending_cnt;
		uint32_t               free_cnt;
		uint32_t               num_task;
	} task;
};

/** struct crm_worker_task
 * @priority   : caller can assign priority to task based on type.
 * @payload    : depending of user of task this payload type will change
 * @process_cb : registered callback called by worker when task enqueued is
 *               ready for processing in worker thread context
 * @parent     : worker parent is link which is enqqueing taks to this workq
 * @entry      : list head of this list entry is worker's empty_head
 * @cancel     : if caller has got free task from pool but wants to abort
 *               or put back without using it
 * @priv       : when task is enqueuer caller can attach priv along which
 *               it will get in process callback
 * @ret        : return value in future to use for blocking calls
 */
struct crm_worker_task {
	int32_t                    priority;
	void                      *payload;
	int32_t                  (*process_cb)(void *priv, void *data);
	void                      *parent;
	struct list_head           entry;
	uint8_t                    cancel;
	void                      *priv;
	int32_t                    ret;
};

/** struct cam_req_mgr_core_worker
 * @work        : work token used by workqueue
 * @job         : workqueue internal job struct
 * @lock_bh     : lock for task structs
 * @mutex_lock  : mutex lock for task structs
 * @in_irq      : set true if workque can be used in irq context
 * @is_paused   : flag to indicate if worker is paused or not
 * @worker_scheduled_ts: enqueue time of worker
 * @flush       : used to track if flush has been called on workqueue
 * @worker_name : name of the worker
 * task -
 * @lock        : Current task's lock handle
 * @pending_cnt : # of tasks left in queue
 * @free_cnt    : # of free/available tasks
 * @process_head: list of tasks enqueued to be executed
 * @empty_head  : list  head of available task which can be used
 *                or acquired in order to enqueue a task to worker
 * @pool        : pool of tasks used for handling events in worker context
 * @num_task    : size of tasks pool
 */
struct cam_req_mgr_core_worker {
#ifndef CONFIG_KTHREAD_BASED_WORKER
	struct work_struct         work;
	struct workqueue_struct   *job;
#else
	struct kthread_worker     *job;
	struct kthread_work        work;
#endif
	spinlock_t                 lock_bh;
	struct mutex               mutex_lock;
	uint32_t                   in_irq;
	bool                       is_paused;
	ktime_t                    worker_scheduled_ts;
	atomic_t                   flush;
	char                       worker_name[128];

	/* tasks */
	struct {
		struct mutex           lock;
		atomic_t               pending_cnt;
		atomic_t               free_cnt;

		struct list_head       process_head[CRM_TASK_PRIORITY_MAX];
		struct list_head       empty_head;
		struct crm_worker_task *pool;
		uint32_t               num_task;
	} task;
};

/**
 * struct cam_kthread_data - Single node of information about a kthread worker *
 * @kthread_worker  : Kthread worker
 * @list     : List member used to append this node to a linked list
 */
struct cam_kthread_data {
	struct kthread_worker  *kthread_worker;
	struct list_head        list;
};

/**
 * struct cam_kthread_info - Kthread scheduling information *
 * @policy               : Scheduling policy
 * @priority             : Scheduling priority
 * @nice                 : Nice value
 * @affinity             : Core affinity
 * @is_list_initialized  : bool to show if list is intialized
 * @kthread_list         : List of all created kthreads
 */
struct cam_kthread_info {
	uint32_t              policy;
	int32_t               priority;
	int32_t               nice;
	uint32_t              affinity;
	bool                  is_list_initalized;
	struct list_head      kthread_list;
};

/**
 * cam_req_mgr_workq_get_task()
 * @brief : Returns empty task pointer for use
 * @worker: workque used for processing
 */
struct crm_worker_task *cam_req_mgr_worker_get_task(
	struct cam_req_mgr_core_worker *worker);

/**
 * cam_req_mgr_worker_create()
 * @brief    : create a workqueue
 * @name     : Name of the workque to be allocated, it is combination
 *             of session handle and link handle
 * @num_task : Num_tasks to be allocated for worker
 * @worker   : Double pointer worker
 * @in_irq   : Set to one if worker might be used in irq context
 * @flags    : Bitwise OR of Flags for worker behavior.
 *             e.g. CAM_REQ_MGR_WORKER_HIGH_PRIORITY | CAM_REQ_MGR_WORKER_SERIAL
 * This function will allocate and create workqueue and pass
 * the worker pointer to caller.
 */
int cam_req_mgr_worker_create(char *name, int32_t num_tasks,
	struct cam_req_mgr_core_worker **worker, enum crm_worker_context in_irq,
	int flags);

/**
 * cam_req_mgr_worker_destroy()
 * @brief : destroy workqueue
 * @worker: pointer to worker data struct
 * this function will destroy workqueue and clean up resources
 * associated with worker such as tasks.
 */
void cam_req_mgr_worker_destroy(struct cam_req_mgr_core_worker **worker);

/**
 * cam_req_mgr_worker_enqueue_task()
 * @brief: Enqueue task in worker queue
 * @task : task to be processed by worker
 * @priv : clients private data
 * @prio : task priority
 * process callback func
 */
int cam_req_mgr_worker_enqueue_task(struct crm_worker_task *task,
	void *priv, int32_t prio);

/**
 * cam_req_mgr_worker_flush()
 * @brief : Flushes the work queue. Function will sleep until any active task is complete.
 * @worker: pointer to worker data struct
 */
void cam_req_mgr_worker_flush(struct cam_req_mgr_core_worker *worker);

/**
 * cam_req_mgr_worker_pause()
 * @worker: pointer to worker data struct
 */
void cam_req_mgr_worker_pause(struct cam_req_mgr_core_worker *worker);

/**
 * cam_req_mgr_worker_resume()
 * @worker: pointer to worker data struct
 */
void cam_req_mgr_worker_resume(struct cam_req_mgr_core_worker *worker);

/**
 * cam_req_mgr_set_thread_prop()
 * @worker: pointer to worker data struct
 */
int cam_req_mgr_set_thread_prop(struct cam_req_mgr_thread_prop_control *worker);

extern struct cam_irq_bh_api worker_bh_api;

#endif
