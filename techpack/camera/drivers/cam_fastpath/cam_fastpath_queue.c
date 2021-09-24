// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include "linux/err.h"
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/atomic.h>

#include "cam_mem_mgr.h"
#include "cam_debug_util.h"
#include "cam_fastpath_queue.h"

#define to_fpq(f) container_of((f)->private_data, \
				struct cam_fp_queue, \
				misc_device)

/* Helper to call queue ops */
#define cam_fpq_call_op(fpq, op, arg...)             \
	(!((fpq)->ops) ? -ENODEV : (((fpq)->ops->op) ?  \
	((fpq)->ops->op)((fpq->priv), ##arg) : -EINVAL))

/* Kmem cache of chain processing memory */
DEFINE_MUTEX(chain_mem_mutex);
static struct {
	struct kmem_cache *cache;
	unsigned int refcnt;
} chain_mem;

static struct kmem_cache *cam_fp_queue_create_chain_mem(void)
{
	struct kmem_cache *mem_cache;

	mutex_lock(&chain_mem_mutex);

	if (chain_mem.refcnt++ == 0)
		chain_mem.cache = KMEM_CACHE(cam_fp_process_chain, 0);

	mem_cache = chain_mem.cache;

	mutex_unlock(&chain_mem_mutex);

	return mem_cache;
}

static void cam_fp_queue_destroy_chain_mem(struct kmem_cache *kmem_cache)
{

	mutex_lock(&chain_mem_mutex);
	if (WARN_ON(chain_mem.refcnt < 1)) {
		CAM_ERR(CAM_CORE, "Chain memory alredy destroyed!");
		mutex_unlock(&chain_mem_mutex);
		return;
	}

	if (chain_mem.refcnt-- == 1) {
		if (WARN_ON(kmem_cache != chain_mem.cache))
			CAM_ERR(CAM_CORE, "Wrong chain mem!");

		kmem_cache_destroy(chain_mem.cache);
		chain_mem.cache = NULL;
	}

	mutex_unlock(&chain_mem_mutex);
}

static void
cam_fp_queue_do_cache_ops(u64 buf_mask,
			  struct cam_fp_buffer *bufs,
			  u32 cache_op)
{
	unsigned long *addr = (unsigned long *)&buf_mask;
	struct cam_mem_cache_ops_cmd cmd = {0};
	int i;
	int p;

	cmd.mem_cache_ops = cache_op;
	for_each_set_bit(i, addr, CAM_FP_MAX_BUFS) {
		if (bufs[i].cache_maintenance) {
			int plane_cnt = bufs[i].num_planes;

			for (p = 0; p < plane_cnt; p++) {
				cmd.buf_handle = bufs[i].plane[p].handle;
				cam_mem_mgr_cache_ops(&cmd);
			}

		}
	}
}

static void
cam_fp_queue_cache_maintanence(struct cam_fp_buffer_set *buf_set)
{
	/* Flush the cache on input buffers */
	cam_fp_queue_do_cache_ops(buf_set->in_buffer_set_mask,
				  buf_set->in_bufs,
				  CAM_MEM_CLEAN_CACHE);

	/* Invalidate the cache on output buffers */
	cam_fp_queue_do_cache_ops(buf_set->out_buffer_set_mask,
				  buf_set->out_bufs,
				  CAM_MEM_INV_CACHE);
}

static int
cam_fp_queue_enqueue_buffer_set(struct cam_fp_queue *fpq,
				struct cam_fp_buffer_set *buf_set,
				struct cam_fp_process_chain *process_chain,
				bool from_usr)
{
	struct cam_fp_buffer_list *buf_list;
	int ret = 0;

	mutex_lock(&fpq->mutex);

	if (list_empty(&fpq->free_queue)) {
		ret = -EBUSY;
		goto error_unlock;
	}

	buf_list = list_first_entry(&fpq->free_queue,
			struct cam_fp_buffer_list, list);
	if (from_usr) {
		if (copy_from_user(&buf_list->buf_set,
				   (void __user *)buf_set,
				   sizeof(buf_list->buf_set))) {
			CAM_ERR(CAM_CORE, "Copy from user failed %s",
				fpq->name);
			ret = -EFAULT;
			goto error_unlock;
		}
	} else {
		memcpy(&buf_list->buf_set, buf_set,
			sizeof(buf_list->buf_set));
	}

	buf_list->proc_chain = process_chain;

	list_del(&buf_list->list);
	list_add_tail(&buf_list->list, &fpq->pending_queue);

	/* Do cache maintanence on enqueue buffer */
	cam_fp_queue_cache_maintanence(&buf_list->buf_set);

	mutex_unlock(&fpq->mutex);

	/* Notify for new buffer available */
	cam_fpq_call_op(fpq, queue_buf_notify);

	CAM_DBG(CAM_CORE, "CAM_FP_ENQUEUE_BUFSET %s %lld", fpq->name,
		buf_list->buf_set.request_id);

	return 0;

error_unlock:
	mutex_unlock(&fpq->mutex);
	return ret;
}

static int
cam_fp_queue_dequeue_buffer_set(struct cam_fp_queue *fpq,
				struct cam_fp_buffer_set *usr_buf_set)
{
	struct cam_fp_buffer_list *buf_list;
	int ret = 0;

	mutex_lock(&fpq->mutex);

	if (list_empty(&fpq->done_queue)) {
		CAM_ERR(CAM_CORE, "done_queue is EMPTY %s", fpq->name);
		ret = -EAGAIN;
		goto done_unlock;
	}

	buf_list = list_first_entry(&fpq->done_queue,
				struct cam_fp_buffer_list, list);

	if (copy_to_user((void __user *)usr_buf_set, &buf_list->buf_set,
			 sizeof(buf_list->buf_set))) {
		CAM_ERR(CAM_CORE, "Copy to user failed %s", fpq->name);
		ret = -EFAULT;
		goto done_unlock;
	}

	list_del(&buf_list->list);

	CAM_DBG(CAM_CORE, "CAM_FP_DEQUEUE_BUFSET %s %lld", fpq->name,
			buf_list->buf_set.request_id);
	/* Put the buffer to free queue */
	list_add_tail(&buf_list->list, &fpq->free_queue);

done_unlock:
	mutex_unlock(&fpq->mutex);
	return ret;
}

static int
cam_fp_queue_sched_next_chain(struct cam_fp_queue *fpq,
			      struct cam_fp_process_chain *proc_chain,
			      u64 timestamp, __u32 sof_index)
{
	int ret = -EINVAL;
	int idx;

	if (!proc_chain)
		return ret;

	idx = atomic_inc_return(&proc_chain->idx) - 1;
	if (idx < proc_chain->chain.num) {
		/* Pass the timestamp on next process */
		proc_chain->chain.buf_set[idx].timestamp = timestamp;
		proc_chain->chain.buf_set[idx].sof_index = sof_index;
		CAM_DBG(CAM_CORE, "%s schedule next chain %d SOF %d",
						fpq->name, idx, sof_index);
		ret = cam_fp_queue_enqueue_buffer_set(proc_chain->fpq[idx],
			&proc_chain->chain.buf_set[idx],
			proc_chain, false);
		if (ret < 0) {
			CAM_ERR(CAM_CORE, "%s Can not schedule next chain %d",
				fpq->name, idx);
		} else {
			CAM_DBG(CAM_CORE, "%s Next chain is scheduled %d",
				fpq->name, idx);
		}
	}

	CAM_DBG(CAM_CORE, "%s: Process chain %d", fpq->name, idx);

	return ret;
}

static int
cam_fp_queue_enqueue_chain(struct file *file,
			   struct cam_fp_queue *fpq,
			   struct cam_fp_chain *usr_chain)
{
	struct cam_fp_process_chain *proc_chain;
	int ret;
	int i;

	proc_chain = kmem_cache_zalloc(fpq->chain_mem, GFP_KERNEL);
	if (!proc_chain)
		return -ENOMEM;

	atomic_set(&proc_chain->idx, 0);

	if (copy_from_user(&proc_chain->chain,
			   (void __user *)usr_chain,
			   sizeof(proc_chain->chain))) {
		CAM_ERR(CAM_CORE, "Copy from user failed %s", fpq->name);
		ret = -EFAULT;
		goto error_free_proc_chain;
	}

	if (proc_chain->chain.num > ARRAY_SIZE(proc_chain->chain.fd)) {
		CAM_ERR(CAM_CORE, "Invalid num fds %d",
			fpq->name, usr_chain->num);
		ret = -EINVAL;
		goto error_free_proc_chain;
	}

	for (i = 0; i < proc_chain->chain.num; i++) {
		struct fd f = fdget(proc_chain->chain.fd[i]);

		if (!f.file || f.file->f_op != file->f_op) {
			CAM_ERR(CAM_CORE, "Fd not valid %d",
				fpq->name, proc_chain->chain.fd[i]);
			ret = -EINVAL;
			goto error_free_proc_chain;
		}

		proc_chain->fpq[i] = to_fpq(f.file);

		fdput(f);
	}

	ret = cam_fp_queue_sched_next_chain(fpq, proc_chain, 0, 0);
	if (ret < 0) {
		CAM_ERR(CAM_CORE, "Can not enqueue first chain! %s",
			fpq->name);
		goto error_free_proc_chain;
	}

	CAM_DBG(CAM_CORE, "CAM_FP_QUEUE_CHAINED_BUFSETS %s", fpq->name);
	return 0;

error_free_proc_chain:
	kfree(proc_chain);
	return ret;
}

/*
 * These are the file operation function for user access to /dev/cam_fpq_buf
 */
static long cam_fp_queue_misc_ioctl(struct file *file, unsigned int cmd,
				    unsigned long arg)
{
	struct cam_fp_queue *fpq = to_fpq(file);
	int ret;

	switch (cmd) {
	case CAM_FP_ENQUEUE_BUFSET:
		ret = cam_fp_queue_enqueue_buffer_set(fpq,
				(struct cam_fp_buffer_set *) arg,
				NULL, true);
		CAM_DBG(CAM_CORE, "CAM_FP_ENQUEUE_BUFSET %s", fpq->name);
		break;
	case CAM_FP_DEQUEUE_BUFSET:
		ret = cam_fp_queue_dequeue_buffer_set(fpq,
				(struct cam_fp_buffer_set *) arg);
		CAM_DBG(CAM_CORE, "CAM_FP_DEQUEUE_BUFSET %s", fpq->name);
		break;
	case CAM_FP_QUEUE_CHAIN:
		ret = cam_fp_queue_enqueue_chain(file, fpq,
				(struct cam_fp_chain *) arg);
		CAM_DBG(CAM_CORE, "CAM_FP_QUEUE_CHAIN %s", fpq->name);
		break;
	default:
		CAM_DBG(CAM_CORE, "%s Invalid ioctl! %lx", fpq->name, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static __poll_t cam_fp_queue_poll(struct file *file, poll_table *wait)
{
	struct cam_fp_queue *fpq = to_fpq(file);
	__poll_t req_events = poll_requested_events(wait);
	__poll_t res = 0;

	if (!(req_events & (EPOLLIN | EPOLLRDNORM)))
		return res;

	/* Check first if we have someting in done queue */
	if (mutex_lock_interruptible(&fpq->mutex))
		return EPOLLERR;

	poll_wait(file, &fpq->done_wq, wait);

	if (!list_empty(&fpq->done_queue))
		res = EPOLLIN | EPOLLRDNORM;

	mutex_unlock(&fpq->mutex);

	return res;
}

static int cam_fp_queue_misc_open(struct inode *inode, struct file *file)
{
	struct cam_fp_queue *fpq = to_fpq(file);
	int ret = 0;
	int i;

	mutex_lock(&fpq->mutex);
	if (fpq->buf_mem) {
		ret = -EBUSY;
		goto error_unlock;
	}

	fpq->chain_mem = cam_fp_queue_create_chain_mem();
	if (!fpq->chain_mem) {
		ret = -ENOMEM;
		goto error_unlock;
	}

	fpq->buf_mem = vmalloc(sizeof(*fpq->buf_mem) * fpq->max_bufs);
	if (!fpq->buf_mem) {
		ret = -ENOMEM;
		goto error_destroy_chain_mem;
	}

	/* Fill empty buffers queue */
	for (i = 0; i < fpq->max_bufs; i++) {
		INIT_LIST_HEAD(&fpq->buf_mem[i].list);
		list_add_tail(&fpq->buf_mem[i].list, &fpq->free_queue);
	}

	mutex_unlock(&fpq->mutex);
	return 0;

error_destroy_chain_mem:
	cam_fp_queue_destroy_chain_mem(fpq->chain_mem);
	fpq->chain_mem = NULL;
error_unlock:
	mutex_unlock(&fpq->mutex);
	return ret;
}

static int cam_fp_queue_misc_release(struct inode *inode, struct file *file)
{
	struct cam_fp_queue *fpq = to_fpq(file);
	struct cam_fp_buffer_list *buf_list;
	struct list_head *pos;

	cam_fpq_call_op(fpq, flush);

	mutex_lock(&fpq->mutex);

	/* Release chains which are in processing/pending queues */
	list_for_each(pos, &fpq->pending_queue) {
		buf_list = list_entry(pos, struct cam_fp_buffer_list, list);
		if (buf_list->proc_chain) {
			kmem_cache_free(fpq->chain_mem,
					buf_list->proc_chain);
			buf_list->proc_chain = NULL;
			CAM_ERR(CAM_CORE, "%s Free chain in pending queue",
				fpq->name);
		}
	}

	list_for_each(pos, &fpq->processing_queue) {
		buf_list = list_entry(pos, struct cam_fp_buffer_list, list);
		if (buf_list->proc_chain) {
			kmem_cache_free(fpq->chain_mem,
					buf_list->proc_chain);
			buf_list->proc_chain = NULL;
			CAM_ERR(CAM_CORE, "%s Free chain in processing queue",
				fpq->name);
		}
	}

	/* Initialize queues to prevent the usage after free  */
	INIT_LIST_HEAD(&fpq->processing_queue);
	INIT_LIST_HEAD(&fpq->pending_queue);
	INIT_LIST_HEAD(&fpq->done_queue);
	INIT_LIST_HEAD(&fpq->free_queue);

	if (fpq->buf_mem) {
		vfree(fpq->buf_mem);
		fpq->buf_mem = NULL;
	}

	if (fpq->chain_mem) {
		cam_fp_queue_destroy_chain_mem(fpq->chain_mem);
		fpq->chain_mem = NULL;
	}

	mutex_unlock(&fpq->mutex);

	return 0;
}

static const struct file_operations cam_fpq_buf_misc_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= cam_fp_queue_misc_ioctl,
	.compat_ioctl   = cam_fp_queue_misc_ioctl,
	.open		= cam_fp_queue_misc_open,
	.poll		= cam_fp_queue_poll,
	.release	= cam_fp_queue_misc_release,
};

struct cam_fp_buffer_set *cam_fp_queue_get_buffer_set(struct cam_fp_queue *fpq)
{
	struct cam_fp_buffer_list *buf_list;

	mutex_lock(&fpq->mutex);

	if (list_empty(&fpq->pending_queue)) {
		CAM_DBG(CAM_CORE, "pending_queue is EMPTY %s", fpq->name);
		mutex_unlock(&fpq->mutex);
		return NULL;
	}

	buf_list = list_first_entry(&fpq->pending_queue,
				struct cam_fp_buffer_list, list);
	list_del(&buf_list->list);
	/* Put the buffer to processing queue */
	list_add_tail(&buf_list->list, &fpq->processing_queue);

	mutex_unlock(&fpq->mutex);

	CAM_DBG(CAM_CORE, "Processing req %s %lld", fpq->name,
		buf_list->buf_set.request_id);

	return &buf_list->buf_set;
}

int cam_fp_queue_buffer_set_done(struct cam_fp_queue *fpq,
				 u64 request_id, u64 timestamp,
				 enum cam_fp_buffer_status status,
				 __u32 sof_index)
{
	struct cam_fp_buffer_list *buf_list = NULL;
	struct list_head *next = NULL;
	struct list_head *pos = NULL;
	int ret = -EINVAL;

	mutex_lock(&fpq->mutex);

	list_for_each_safe(pos, next, &fpq->processing_queue) {
		buf_list = list_entry(pos, struct cam_fp_buffer_list, list);
		if (buf_list->buf_set.request_id != request_id)
			continue;

		list_del(&buf_list->list);

		/* Put the buffer to done queue */
		buf_list->buf_set.status = status;

		/* Fill the timestamp if provided */
		if (timestamp) {
			buf_list->buf_set.timestamp = timestamp;
			buf_list->buf_set.sof_index = sof_index;
		}

		/* Don't leave userspace with 0 timestamp */
		if (!buf_list->buf_set.timestamp) {
			buf_list->buf_set.timestamp = ktime_get_ns();
			CAM_WARN(CAM_CORE, "%s: Timestamp not provided!",
				fpq->name);
		}

		if (!cam_fp_queue_sched_next_chain(fpq, buf_list->proc_chain,
				buf_list->buf_set.timestamp, buf_list->buf_set.sof_index)) {
			/* In chain process do not notify userspace for
			 * buffer done. The main reason is to reduce ioctl
			 * calls. This is agreement between uapi and kernel.
			 * The agreement is that buffer chain dequeued on
			 * last device.
			 */
			list_add_tail(&buf_list->list, &fpq->free_queue);
		} else {
			/*
			 * There is no next chain to process this is the last
			 * set. Add the buffer set to done queue.
			 */
			if (buf_list->proc_chain) {
				kmem_cache_free(fpq->chain_mem,
						buf_list->proc_chain);
				buf_list->proc_chain = NULL;
			}
			list_add_tail(&buf_list->list, &fpq->done_queue);
			wake_up_all(&fpq->done_wq);
		}
		ret = 0;
		break;
	}

	mutex_unlock(&fpq->mutex);

	CAM_DBG(CAM_CORE, "DONE: %s req %lld %s", fpq->name, request_id,
		ret ? "ERROR":"SUCCESS");
	return ret;
}

int cam_fp_queue_init(struct cam_fp_queue *fpq,
		      const char *name,
		      unsigned int max_bufs,
		      const struct cam_fp_queue_ops *ops,
		      void *priv)
{
	int ret;

	if (!fpq || !name)
		return -EINVAL;

	strlcpy(fpq->name, name, sizeof(fpq->name));

	memset(&fpq->misc_device, 0, sizeof(fpq->misc_device));

	fpq->misc_device.name = fpq->name;
	fpq->misc_device.minor = MISC_DYNAMIC_MINOR;
	fpq->misc_device.fops = &cam_fpq_buf_misc_fops;

	ret = misc_register(&fpq->misc_device);
	if (ret) {
		CAM_ERR(CAM_CORE, "%s: can't misc_register!", name);
		return ret;
	}

	mutex_init(&fpq->mutex);
	init_waitqueue_head(&fpq->done_wq);
	fpq->max_bufs = max_bufs;
	fpq->ops = ops;
	fpq->priv = priv;

	/* Initialize queues */
	INIT_LIST_HEAD(&fpq->processing_queue);
	INIT_LIST_HEAD(&fpq->pending_queue);
	INIT_LIST_HEAD(&fpq->done_queue);
	INIT_LIST_HEAD(&fpq->free_queue);

	return 0;
}

int cam_fp_queue_deinit(struct cam_fp_queue *fpq)
{
	if (!fpq)
		return -EINVAL;

	misc_deregister(&fpq->misc_device);
	mutex_destroy(&fpq->mutex);

	return 0;
}
