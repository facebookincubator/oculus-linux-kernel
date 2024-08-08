// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_irq_controller.h"
#include "cam_req_mgr_worker_wrapper.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"

struct cam_irq_bh_api worker_bh_api = {
	.bottom_half_enqueue_func = cam_req_mgr_worker_enqueue_task,
	.get_bh_payload_func = cam_req_mgr_worker_get_task
};

#define WORKER_ACQUIRE_LOCK(worker, flags) {\
	if ((worker)->in_irq) \
		spin_lock_irqsave(&(worker)->lock_bh, (flags)); \
	else \
		mutex_lock(&(worker)->mutex_lock); \
}

#define WORKER_RELEASE_LOCK(worker, flags) {\
	if ((worker)->in_irq) \
		spin_unlock_irqrestore(&(worker)->lock_bh, (flags)); \
	else	\
		mutex_unlock(&(worker)->mutex_lock); \
}

#ifdef CONFIG_KTHREAD_BASED_WORKER
#define WORK struct kthread_work
static struct cam_kthread_info g_cam_kthread_info;
#else
#define WORK struct work_struct
#endif

static void cam_req_mgr_worker_put_task_unlocked(struct crm_worker_task *task)
{
	struct cam_req_mgr_core_worker *worker =
		(struct cam_req_mgr_core_worker *)task->parent;

	task->cancel = 0;
	task->process_cb = NULL;
	task->priv = NULL;

	list_add_tail(&task->entry,
		&worker->task.empty_head);
	atomic_add(1, &worker->task.free_cnt);
}


static void cam_req_mgr_worker_put_task(struct crm_worker_task *task)
{
	struct cam_req_mgr_core_worker *worker =
		(struct cam_req_mgr_core_worker *)task->parent;
	unsigned long flags = 0;

	WORKER_ACQUIRE_LOCK(worker, flags);
	if (worker->is_paused)
		goto end;
	cam_req_mgr_worker_put_task_unlocked(task);

end:
	WORKER_RELEASE_LOCK(worker, flags);
}

/**
 * cam_req_mgr_process_worker_task() - Process the enqueued task
 * @task: pointer to task worker thread shall process
 */
static int cam_req_mgr_process_worker_task(struct crm_worker_task *task)
{

	if (!task)
		return -EINVAL;

	if (task->process_cb)
		task->process_cb(task->priv, task->payload);
	else
		CAM_ERR(CAM_CRM, "FATAL:no task handler registered for worker");
	cam_req_mgr_worker_put_task(task);

	return 0;
}

void cam_req_mgr_process_worker(WORK *w)

{
	struct cam_req_mgr_core_worker *worker = NULL;
	struct crm_worker_task         *task;
	int32_t                        i = CRM_TASK_PRIORITY_0;
	unsigned long                  flags = 0;
	ktime_t                        sched_start_time;

	if (!w) {
		CAM_ERR(CAM_CRM, "NULL task pointer can not schedule");
		return;
	}

	worker = (struct cam_req_mgr_core_worker *)
		container_of(w, struct cam_req_mgr_core_worker, work);

	cam_common_util_thread_switch_delay_detect(
		"CRM worker schedule",
		worker->worker_scheduled_ts,
		CAM_WORKER_SCHEDULE_TIME_THRESHOLD);
	sched_start_time = ktime_get();
	while (i < CRM_TASK_PRIORITY_MAX) {
		WORKER_ACQUIRE_LOCK(worker, flags);
		while (!list_empty(&worker->task.process_head[i])) {
			task = list_first_entry(&worker->task.process_head[i],
				struct crm_worker_task, entry);
			atomic_sub(1, &worker->task.pending_cnt);
			list_del_init(&task->entry);
			WORKER_RELEASE_LOCK(worker, flags);
			if (!unlikely(atomic_read(&worker->flush)))
				cam_req_mgr_process_worker_task(task);
			CAM_DBG(CAM_CRM, "processed task %pK free_cnt %d",
				task, atomic_read(&worker->task.free_cnt));
			WORKER_ACQUIRE_LOCK(worker, flags);
		}
		WORKER_RELEASE_LOCK(worker, flags);
		i++;
	}
	cam_common_util_thread_switch_delay_detect(
		"CRM worker execution",
		sched_start_time,
		CAM_WORKER_EXE_TIME_THRESHOLD);
}


#ifdef CONFIG_KTHREAD_BASED_WORKER
int cam_req_mgr_kthread_set_thread_prop(struct cam_kthread_data *kthread_data) {
	struct sched_param thread_priority = {0};
	struct cpumask cpu_affinity;
	int i = 0, temp, rc = 0;

	if (g_cam_kthread_info.priority) {
		if (g_cam_kthread_info.priority != -1) {
			thread_priority.sched_priority = g_cam_kthread_info.priority;
		}

		CAM_DBG(CAM_REQ, "priority %d policy %d", thread_priority.sched_priority, g_cam_kthread_info.policy);
		rc = sched_setscheduler_nocheck(kthread_data->kthread_worker->task, g_cam_kthread_info.policy, &thread_priority);
		if (rc) {
			CAM_ERR(CAM_REQ, "Failed to set priority %d policy %d return %d",
				thread_priority.sched_priority, g_cam_kthread_info.policy, rc);
			goto end;
		}

	}
	if (g_cam_kthread_info.affinity) {
		temp = g_cam_kthread_info.affinity;
		while (temp) {
			if (temp & 0x1)
				cpumask_set_cpu(i, &cpu_affinity);
			temp >>= 1;
			i++;
		}
		kthread_bind_mask(kthread_data->kthread_worker->task, &cpu_affinity);
	}
end:
	return rc;
}

inline int cam_req_mgr_kthread_create(struct cam_req_mgr_core_worker *crm_worker, char *name) {
	char buf[128] = "crm_kthread-";

	strlcat(buf, name, sizeof(buf));
	CAM_DBG(CAM_CRM, "create kthread crm_kthread-%s", name);
	crm_worker->job = kthread_create_worker(0, buf);
	if (IS_ERR(crm_worker->job)) {
		return PTR_ERR(crm_worker->job);
	}

	/* kthread attributes initialization */
	strlcpy(crm_worker->worker_name, buf, sizeof(crm_worker->worker_name));
	kthread_init_work(&crm_worker->work, cam_req_mgr_process_worker);
	return 0;
}

inline void cam_req_mgr_kthread_destroy(struct cam_req_mgr_core_worker *worker) {
	struct kthread_worker   *kthread_worker;
	unsigned long flags = 0;
	struct cam_kthread_data *kthread_data;
	bool need_free = false;

	if (worker->job) {
		kthread_worker = worker->job;
		worker->job = NULL;
		WORKER_RELEASE_LOCK(worker, flags);
		kthread_destroy_worker(kthread_worker);
		WORKER_ACQUIRE_LOCK(worker, flags);
	}
	list_for_each_entry(kthread_data, &g_cam_kthread_info.kthread_list, list) {
		if (kthread_data->kthread_worker == kthread_worker) {
			list_del_init(&kthread_data->list);
			need_free = true;
			break;
		}
	}

	if (need_free) {
		WORKER_RELEASE_LOCK(worker, flags);
		vfree(kthread_data);
		WORKER_ACQUIRE_LOCK(worker, flags);
	}
}

#define CREATE_WORKER(crm_worker, name, num_tasks, flags) cam_req_mgr_kthread_create(crm_worker, name)
#define DESTROY_WORKER cam_req_mgr_kthread_destroy
#define QUEUE_WORK kthread_queue_work
#define FLUSH_WORKER(worker) kthread_flush_worker(worker->job)
#else
inline int cam_req_mgr_workq_create(struct cam_req_mgr_core_worker *crm_worker,
		char *name, int32_t num_tasks, int flags) {
	char buf[128] = "crm_workq-";
	int wq_flags = 0, max_active_tasks = 0;

	wq_flags |= WQ_UNBOUND;
	if (flags & CAM_WORKER_FLAG_HIGH_PRIORITY)
		wq_flags |= WQ_HIGHPRI;

	if (flags & CAM_WORKER_FLAG_SERIAL)
		max_active_tasks = 1;

	strlcat(buf, name, sizeof(buf));
	CAM_DBG(CAM_CRM, "create workque crm_workq-%s", name);
	crm_worker->job = alloc_workqueue(buf,
		wq_flags, max_active_tasks, NULL);
	if (!crm_worker->job) {
		return -ENOMEM;
	}

	/* Workq attributes initialization */
	strlcpy(crm_worker->worker_name, buf, sizeof(crm_worker->worker_name));
	INIT_WORK(&crm_worker->work, cam_req_mgr_process_worker);
	return 0;
}

inline void cam_req_mgr_workq_destroy(struct cam_req_mgr_core_worker *worker) {
	struct workqueue_struct   *job;
	unsigned long flags = 0;

	if (worker->job) {
		job = worker->job;
		worker->job = NULL;
		WORKER_RELEASE_LOCK(worker, flags);
		destroy_workqueue(job);
		WORKER_ACQUIRE_LOCK(worker, flags);
	}
}
#define CREATE_WORKER(crm_worker, name, num_tasks, flags) cam_req_mgr_workq_create(crm_worker, name, num_tasks, flags)
#define DESTROY_WORKER cam_req_mgr_workq_destroy
#define QUEUE_WORK queue_work
#define FLUSH_WORKER(worker) cancel_work_sync(&worker->work)
#endif

inline struct crm_worker_task *cam_req_mgr_worker_get_task(
	struct cam_req_mgr_core_worker *worker) {
	struct crm_worker_task *task = NULL;
	unsigned long flags = 0;

	if (!worker)
		return NULL;

	WORKER_ACQUIRE_LOCK(worker, flags);
	if (worker->is_paused) {
		task = ERR_PTR(-EIO);
		goto end;
	}

	if (list_empty(&worker->task.empty_head))
		goto end;

	task = list_first_entry(&worker->task.empty_head,
		struct crm_worker_task, entry);
	if (task) {
		atomic_sub(1, &worker->task.free_cnt);
		list_del_init(&task->entry);
	}

end:
	WORKER_RELEASE_LOCK(worker, flags);

	CAM_DBG(CAM_CRM, "get_task worker %s free_cnt %d pause %d",
			worker->worker_name, atomic_read(&worker->task.free_cnt),
			worker->is_paused);

	return task;

}

inline int cam_req_mgr_worker_create(char *name, int32_t num_tasks,
	struct cam_req_mgr_core_worker **worker, enum crm_worker_context in_irq,
	int flags) {
	int32_t i, rc;
	struct crm_worker_task  *task;
	struct cam_req_mgr_core_worker *crm_worker = NULL;
#ifdef CONFIG_KTHREAD_BASED_WORKER
	struct cam_kthread_data *kthread_data;
#endif

	if (!*worker) {
		crm_worker = vzalloc(sizeof(struct cam_req_mgr_core_worker));
		if (crm_worker == NULL)
			return -ENOMEM;
		rc = CREATE_WORKER(crm_worker, name, num_tasks, flags);
		if (rc) {
			vfree(crm_worker);
			return rc;
		}
		spin_lock_init(&crm_worker->lock_bh);
		mutex_init(&crm_worker->mutex_lock);
		CAM_DBG(CAM_CRM, "LOCK_DBG worker %s lock %pK",
			name, &crm_worker->lock_bh);

		/* Task attributes initialization */
		atomic_set(&crm_worker->task.pending_cnt, 0);
		atomic_set(&crm_worker->task.free_cnt, 0);
		for (i = CRM_TASK_PRIORITY_0; i < CRM_TASK_PRIORITY_MAX; i++)
			INIT_LIST_HEAD(&crm_worker->task.process_head[i]);
		INIT_LIST_HEAD(&crm_worker->task.empty_head);
		atomic_set(&crm_worker->flush, 0);
		crm_worker->is_paused = false;
		crm_worker->in_irq = in_irq;
		crm_worker->task.num_task = num_tasks;
		crm_worker->task.pool = vzalloc(crm_worker->task.num_task *
				sizeof(struct crm_worker_task));
		if (!crm_worker->task.pool) {
			CAM_WARN(CAM_CRM, "Insufficient memory %zu",
				sizeof(struct crm_worker_task) *
				crm_worker->task.num_task);
			vfree(crm_worker);
			return -ENOMEM;
		}

		for (i = 0; i < crm_worker->task.num_task; i++) {
			task = &crm_worker->task.pool[i];
			task->parent = (void *)crm_worker;
			/* Put all tasks in free pool */
			INIT_LIST_HEAD(&task->entry);
			cam_req_mgr_worker_put_task(task);
		}
		*worker = crm_worker;
#ifdef CONFIG_KTHREAD_BASED_WORKER
		kthread_data = vzalloc(sizeof(struct cam_kthread_data));
		kthread_data->kthread_worker = crm_worker->job;
		if (!g_cam_kthread_info.is_list_initalized) {
			INIT_LIST_HEAD(&g_cam_kthread_info.kthread_list);
			g_cam_kthread_info.is_list_initalized = true;
		}
		INIT_LIST_HEAD(&kthread_data->list);
		list_add_tail(&kthread_data->list,
			&g_cam_kthread_info.kthread_list);
		cam_req_mgr_kthread_set_thread_prop(kthread_data);
#endif
		CAM_DBG(CAM_CRM, "free tasks %d",
			atomic_read(&crm_worker->task.free_cnt));
	}

	return 0;

}

inline void cam_req_mgr_worker_destroy(struct cam_req_mgr_core_worker **crm_worker) {
	unsigned long flags = 0;
	struct cam_req_mgr_core_worker *worker;
	int i;

	if (crm_worker && *crm_worker) {
		worker = *crm_worker;
		CAM_DBG(CAM_CRM, "destroy kthread %s", worker->worker_name);
		WORKER_ACQUIRE_LOCK(worker, flags);
		/* prevent any processing of callbacks */
		atomic_set(&worker->flush, 1);
		DESTROY_WORKER(worker);

		/* Destroy worker payload data */
		WORKER_RELEASE_LOCK(worker, flags);
		kfree(worker->task.pool[0].payload);
		vfree(worker->task.pool);
		WORKER_ACQUIRE_LOCK(worker, flags);

		/* Leave lists in stable state after freeing pool */
		INIT_LIST_HEAD(&worker->task.empty_head);
		for (i = 0; i < CRM_TASK_PRIORITY_MAX; i++)
			INIT_LIST_HEAD(&worker->task.process_head[i]);
		*crm_worker = NULL;
		WORKER_RELEASE_LOCK(worker, flags);
		mutex_destroy(&worker->mutex_lock);
		vfree(worker);
	}

}

inline int cam_req_mgr_worker_enqueue_task(struct crm_worker_task *task,
	void *priv, int32_t prio) {
	int rc = 0;
	struct cam_req_mgr_core_worker *worker = NULL;
	unsigned long flags = 0;

	if (!task) {
		CAM_WARN(CAM_CRM, "NULL task pointer can not schedule");
		return -EINVAL;
	}
	worker = (struct cam_req_mgr_core_worker *)task->parent;
	if (!worker) {
		CAM_DBG(CAM_CRM, "NULL worker pointer suspect mem corruption");
		return -EINVAL;
	}

	if (task->cancel == 1 || atomic_read(&worker->flush)) {
		rc = 0;
		goto abort;
	}
	task->priv = priv;
	task->priority =
		(prio < CRM_TASK_PRIORITY_MAX && prio >= CRM_TASK_PRIORITY_0)
		? prio : CRM_TASK_PRIORITY_0;

	WORKER_ACQUIRE_LOCK(worker, flags);
	if (!worker->job || worker->is_paused) {
		rc = -EINVAL;
		WORKER_RELEASE_LOCK(worker, flags);
		goto abort;
	}

	list_add_tail(&task->entry,
		&worker->task.process_head[task->priority]);

	atomic_add(1, &worker->task.pending_cnt);
	CAM_DBG(CAM_CRM, "enq task %pK pending_cnt %d",
		task, atomic_read(&worker->task.pending_cnt));

	worker->worker_scheduled_ts = ktime_get();
	QUEUE_WORK(worker->job, &worker->work);

	WORKER_RELEASE_LOCK(worker, flags);

	return rc;
abort:
	cam_req_mgr_worker_put_task(task);
	CAM_INFO(CAM_CRM, "task aborted and queued back to pool");
	return rc;
}

inline void cam_req_mgr_worker_flush(struct cam_req_mgr_core_worker *worker) {
	int i;
	unsigned long flags = 0;
	struct crm_worker_task  *task;

	if (!worker) {
		CAM_ERR(CAM_CRM, "worker is null");
		return;
	}

	atomic_set(&worker->flush, 1);
	FLUSH_WORKER(worker);
	/* Task attributes initialization */
	atomic_set(&worker->task.pending_cnt, 0);
	atomic_set(&worker->task.free_cnt, 0);

	WORKER_ACQUIRE_LOCK(worker, flags);
	INIT_LIST_HEAD(&worker->task.empty_head);

	for (i = 0; i < worker->task.num_task; i++) {
		task = &worker->task.pool[i];
		task->parent = (void *)worker;
		/* Put all tasks in free pool */
		INIT_LIST_HEAD(&task->entry);
		cam_req_mgr_worker_put_task_unlocked(task);
	}
	WORKER_RELEASE_LOCK(worker, flags);
	atomic_set(&worker->flush, 0);
}

inline void cam_req_mgr_worker_pause(struct cam_req_mgr_core_worker *worker) {
	unsigned long flags = 0;

	WORKER_ACQUIRE_LOCK(worker, flags);
	worker->is_paused = true;
	WORKER_RELEASE_LOCK(worker, flags);
	CAM_DBG(CAM_CRM, "pause worker %s, free_cnt %d",
			worker->worker_name, atomic_read(&worker->task.free_cnt));

}

inline void cam_req_mgr_worker_resume(struct cam_req_mgr_core_worker *worker) {
	unsigned long flags = 0;

	WORKER_ACQUIRE_LOCK(worker, flags);
	worker->is_paused = false;
	WORKER_RELEASE_LOCK(worker, flags);
	CAM_DBG(CAM_CRM, "resume worker %s, free_cnt %d",
			worker->worker_name, atomic_read(&worker->task.free_cnt));

}

inline int cam_req_mgr_set_thread_prop(struct cam_req_mgr_thread_prop_control *thread_prop) {
#ifdef CONFIG_KTHREAD_BASED_WORKER
	struct cam_kthread_data *kthread_data;
	int rc = 0;

	g_cam_kthread_info.affinity = thread_prop->affinity;
	g_cam_kthread_info.policy   = thread_prop->policy;
	g_cam_kthread_info.priority = thread_prop->priority;
	g_cam_kthread_info.nice     = thread_prop->nice;
	list_for_each_entry(kthread_data, &g_cam_kthread_info.kthread_list, list) {
		cam_req_mgr_kthread_set_thread_prop(kthread_data);
		if(rc)
			return rc;
	}
	return rc;
#else
	CAM_INFO(CAM_REQ, "Kthread not enabled");
	return -EOPNOTSUPP;
#endif
}
