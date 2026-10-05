// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file arfw_debug.c
 *
 * @brief Contains operations to initialize, destroy and maintain debugfs
 * for arfirmware IPC.
 *
 * @details
 *
 *******************************************************************************/

#include <linux/arfw_types.h>
#include <linux/arfw_debug.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "arfw_debug.h"

#include <ar_binned_sampler.h>
#include <ar_fw_message.h>
#include <arfw_log.h>

// Debugfs definitions
#define ARFW_PATH_LEN 32
#define ARFW_INACTIVE_QUEUES_NUM 32
#define ARFW_DEBUG_ROOT_PATH "arfw_ipc"
#define ARFW_DEBUG_QUEUE_PATH "queues"
#define ARFW_DEBUG_DEVICE_PATH "devices"
#define ARFW_DEBUG_INACTIVE_QUEUE_PATH "inactive_queues"

#define INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name, print_func, field_name,     \
					   inner_func, empty)                \
	static int _arfw_debug_##name(struct seq_file *sfile, void *ignored) \
	{                                                                    \
		struct inactive_queue_debug *inactive;                       \
		inactive = sfile->private;                                   \
		AR_ASSERT(inactive);                                         \
		print_func(sfile, inner_func(inactive->empty##field_name));  \
		return 0;                                                    \
	}                                                                    \
	static int _fops_open_##name(struct inode *inode, struct file *file) \
	{                                                                    \
		return single_open(file, _arfw_debug_##name,                 \
				   inode->i_private);                        \
	}                                                                    \
	static const struct file_operations empty##name = {                  \
		.open = _fops_open_##name,                                   \
		.read = seq_read,                                            \
		.llseek = seq_lseek,                                         \
		.release = single_release,                                   \
	}

#define ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name, print_func, field_name,       \
					 inner_func, empty)                  \
	static int _arfw_debug_##name(struct seq_file *sfile, void *ignored) \
	{                                                                    \
		struct arfw_client_queue *client;                            \
		client = sfile->private;                                     \
		AR_ASSERT(client);                                           \
		print_func(sfile,                                            \
			   inner_func(client->debug.empty##field_name));     \
		return 0;                                                    \
	}                                                                    \
	static int _fops_open_##name(struct inode *inode, struct file *file) \
	{                                                                    \
		return single_open(file, _arfw_debug_##name,                 \
				   inode->i_private);                        \
	}                                                                    \
	static const struct file_operations empty##name = {                  \
		.open = _fops_open_##name,                                   \
		.read = seq_read,                                            \
		.llseek = seq_lseek,                                         \
		.release = single_release,                                   \
	}

#define ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name, print_func, field_name,       \
					 inner_func, empty)                  \
	static int _arfw_debug_##name(struct seq_file *sfile, void *ignored) \
	{                                                                    \
		struct arfw_client_queue *client;                            \
		client = sfile->private;                                     \
		AR_ASSERT(client);                                           \
		print_func(sfile,                                            \
			   inner_func(client->debug.empty##field_name));     \
		return 0;                                                    \
	}                                                                    \
	static int _fops_open_##name(struct inode *inode, struct file *file) \
	{                                                                    \
		return single_open(file, _arfw_debug_##name,                 \
				   inode->i_private);                        \
	}                                                                    \
	static const struct file_operations empty##name = {                  \
		.open = _fops_open_##name,                                   \
		.read = seq_read,                                            \
		.llseek = seq_lseek,                                         \
		.release = single_release,                                   \
	}

/**
 * Macro to create fs operation functions and structs for use in debugfs_create_file for an active
 * queue.
*/
#define ACTIVE_DEBUG_FS_OPS_CREATE(name, print_func, field_name) \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name, print_func, field_name, , )

// same macro for an inactive queue
#define INACTIVE_DEBUG_FS_OPS_CREATE(name, print_func, field_name) \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name, print_func, field_name, , )

// shorthand for boilerplate funcs for active + inactive queues
#define DEBUG_FS_OPS_CREATE(name, print_func, field_name)         \
	ACTIVE_DEBUG_FS_OPS_CREATE(name, print_func, field_name); \
	INACTIVE_DEBUG_FS_OPS_CREATE(name##_inactive, print_func, field_name)

static void print_str(struct seq_file *sfile, const char *str)
{
	seq_printf(sfile, "%s\n", str);
}

static void print_u64(struct seq_file *sfile, uint64_t value)
{
	seq_printf(sfile, "%llu\n", value);
}

static void print_int(struct seq_file *sfile, int value)
{
	seq_printf(sfile, "%d\n", value);
}

// simple macro avoid declaring so many static functions by hand if they do all the same thing
#define get_pX(x)                                              \
	static int get_p##x(const void *bs)                    \
	{                                                      \
		return ar_binned_sampler_get_threshold(bs, x); \
	}

// declare the get_p50, p90, p95, p99 functions for use in further macros
get_pX(50) get_pX(90) get_pX(95) get_pX(99)
/**
 * Macro to create all of the boilerplate functions to print a sampler for an active queue.
 */
#define ACTIVE_DEBUG_SAMPLER_FS_OPS_CREATE(name, sampler_name)                 \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_num_samples, print_u64,        \
					 sampler_name,                         \
					 ar_binned_sampler_get_num_samples, ); \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_min, print_int, sampler_name,  \
					 ar_binned_sampler_get_min, );         \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p50, print_int, sampler_name,  \
					 get_p50, );                           \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p90, print_int, sampler_name,  \
					 get_p90, );                           \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p95, print_int, sampler_name,  \
					 get_p95, );                           \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p99, print_int, sampler_name,  \
					 get_p99, );                           \
	ACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_max, print_int, sampler_name,  \
					 ar_binned_sampler_get_max, )

/**
 * Macro to create all of the boilerplate functions to print a sampler for an inactive queue.
*/
#define INACTIVE_DEBUG_SAMPLER_FS_OPS_CREATE(name, sampler_name)         \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(                              \
		name##_num_samples, print_u64, sampler_name,             \
		ar_binned_sampler_get_num_samples, );                    \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_min, print_int,        \
					   sampler_name,                 \
					   ar_binned_sampler_get_min, ); \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p50, print_int,        \
					   sampler_name, get_p50, );     \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p90, print_int,        \
					   sampler_name, get_p90, );     \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p95, print_int,        \
					   sampler_name, get_p95, );     \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_p99, print_int,        \
					   sampler_name, get_p99, );     \
	INACTIVE_DEBUG_FS_OPS_CREATE_INNER(name##_max, print_int,        \
					   sampler_name,                 \
					   ar_binned_sampler_get_max, )

// shorthand for declaring all the boilerplate for active and inactive functions
#define DEBUG_SAMPLER_FS_OPS_CREATE(name, sampler_name)         \
	ACTIVE_DEBUG_SAMPLER_FS_OPS_CREATE(name, sampler_name); \
	INACTIVE_DEBUG_SAMPLER_FS_OPS_CREATE(name##_inactive, sampler_name)

#define DEBUG_SAMPLER_CREATE_FILES(dir, private, name)                    \
	do {                                                              \
		debugfs_create_file("num_samples", S_IRUSR, dir, private, \
				    &name##_num_samples);                 \
		debugfs_create_file("min", S_IRUSR, dir, private,         \
				    &name##_min);                         \
		debugfs_create_file("p50", S_IRUSR, dir, private,         \
				    &name##_p50);                         \
		debugfs_create_file("p90", S_IRUSR, dir, private,         \
				    &name##_p90);                         \
		debugfs_create_file("p95", S_IRUSR, dir, private,         \
				    &name##_p95);                         \
		debugfs_create_file("p99", S_IRUSR, dir, private,         \
				    &name##_p99);                         \
		debugfs_create_file("max", S_IRUSR, dir, private,         \
				    &name##_max);                         \
	} while (0)

	// Dump the current state of the queue before removing it. This structure will
	// be used for the inactive queues. The only purpose is a debugging purpose.
	struct inactive_queue_debug {
	struct dentry *dir;

	int id;
	ar_endpoint_id_t hlos_endpoint_id;
	ar_endpoint_id_t fw_endpoint_id;
	enum ar_queue_direction direction;
	bool mirror;
	uint16_t depth;

	uint16_t consumer_idx;
	uint16_t producer_idx;
	uint16_t producer_inflight;
	uint16_t consumer_iteration;
	uint16_t producer_iteration;

	int pid;
	char comm[TASK_COMM_LEN];
	ktime_t creation_ts;
	ktime_t destruction_ts;
	int shutdown_reason;
	long msg_count;
	long inline_count;
	long external_count;
	long drop_count;
	long consumed_count;
	ktime_t msg_activity_last_ts;
	long pend_count;
	long pend_error_count;
	long pend_active_count;
	long pend_req_count;
	ktime_t pend_last_ts;
	uint8_t latency_sampler[AR_BINNED_SAMPLER_SIZE(
		ARRAY_SIZE(LATENCY_THRESHOLDS_US))];
	time64_t latency_log_last_ts;
	time64_t latency_threshold_last_ts;
	uint8_t user_latency_sampler[AR_BINNED_SAMPLER_SIZE(
		ARRAY_SIZE(LATENCY_THRESHOLDS_US))];
	time64_t user_latency_threshold_last_ts;
	uint8_t capacity_sampler[AR_BINNED_SAMPLER_SIZE(
		CAPACITY_MAX_BUCKET_COUNT)];

	struct list_head list_node;
};

static struct dentry *arfw_debug_root_dir;
static struct dentry *arfw_debug_queue_dir;
static struct dentry *arfw_debug_device_dir;
static struct dentry *arfw_debug_inactive_queue_dir;

static DEFINE_MUTEX(inactive_queues_lock);
static struct list_head inactive_queues_list;
static int inactive_queues_num;
static struct inactive_queue_debug
	arfw_debug_inactive_queue[ARFW_INACTIVE_QUEUES_NUM];

static void arfw_debug_inactive_queue_add(struct arfw_client_queue *client,
					  char *path);
static void
arfw_debug_inactive_queue_remove(struct inactive_queue_debug *inactive);

static void arfw_debug_queue_path_create(char *path, int size,
					 struct arfw_int_client_queue *queue)
{
	AR_ASSERT(path);
	AR_ASSERT(queue);

	snprintf(path, size, "%04x-%04x-%d-%s", queue->hlos_endpoint_id,
		 queue->fw_endpoint_id, queue->mirror,
		 (queue->direction == AR_QUEUE_HLOS_TO_FW) ? "send" : "recv");
}

static int arfw_debug_help_show(struct seq_file *sfile, void *ignored)
{
	seq_printf(sfile, "AR firmware IPC debug information.\n\n");

	seq_printf(
		sfile,
		"Providing information about ARFW queues. The `queues` directory contains information for the\n");
	seq_printf(
		sfile,
		"active queues, while `inactive_queues` store information about last %d removed queues.\n\n",
		ARFW_INACTIVE_QUEUES_NUM);

	seq_printf(
		sfile,
		"Each queue is represented as a directory entry, with the name like:\n");
	seq_printf(
		sfile,
		"  [HLOS_ENDPOINT]-[FW_ENDPOINT]-[MIRROR]-[DIRECTION], where\n");
	seq_printf(
		sfile,
		"    - HLOS_ENDPOINT: is hex number for the HLOS endpoint\n");
	seq_printf(sfile,
		   "    - FW_ENDPOINT  : is hex number for the FW endpoint\n");
	seq_printf(
		sfile,
		"    - MIRROR       : is either 0 if it is an original queue, and 1 if it is a mirror of the\n");
	seq_printf(sfile, "                     original\n");
	seq_printf(
		sfile,
		"    - DIRECTION    : is either send in case of `HLOS-to-FW` direction and recv otherwise\n\n");

	seq_printf(
		sfile,
		"All the information related to the queue could be dumped by using the `dumpstr` node. The\n");
	seq_printf(sfile, "following information is provided:\n");
	seq_printf(sfile, "  - HLOS endpoint       : hex number\n");
	seq_printf(sfile, "  - FW endpoint         : hex number\n");
	seq_printf(
		sfile,
		"  - queue direction     : either `->` for `HLOS-to-FW`, `<-` otherwise\n");
	seq_printf(
		sfile,
		"  - id                  : internal kernel queue id number\n");
	seq_printf(
		sfile,
		"  - mirror              : 0 for original queue, and 1 for the mirror\n");
	seq_printf(
		sfile,
		"  - status              : ACTIVE for the working queue, INACTIVE otherwise\n");
	seq_printf(
		sfile,
		"  - pid                 : the process which made a request to create a queue\n");
	seq_printf(sfile, "  - comm                : the process name\n");
	seq_printf(
		sfile,
		"  - depth               : the real depth of the queue. The effective queue length is usually\n");
	seq_printf(sfile, "                          `depth - 1`.\n");
	seq_printf(
		sfile,
		"  - message count       : number of messages sent/received\n");
	seq_printf(
		sfile,
		"  - consumer_idx        : current consumer index (<= producer_idx)\n");
	seq_printf(
		sfile,
		"  - producer_idx        : current producer index (>= consumer_idx)\n");
	seq_printf(
		sfile,
		"  - producer_inflight   : the amount of the messages which have been already reserverd for\n");
	seq_printf(
		sfile,
		"                          sending, but not ready for consumption.\n");
	seq_printf(
		sfile,
		"  - consumer_iteration  : number of the consumer index full cycles (<= producer_iteration)\n");
	seq_printf(
		sfile,
		"  - producer_iteration  : number of the producer index full cycles (>= consumer_iteration)\n");
	seq_printf(
		sfile,
		"  - inline              : number of the inline or inline messages\n");
	seq_printf(
		sfile,
		"  - external            : number of the messages which are using an external buffers\n");
	seq_printf(
		sfile,
		"  - drop                : number of drops for the operation\n");
	seq_printf(sfile,
		   "  - msg_last_ts         : timestamp of last message");
	seq_printf(sfile,
		   "  - pend count          : number of pended buffers\n");
	seq_printf(
		sfile,
		"  - pend active         : number of active pend buffers (only for recv queues)\n");
	seq_printf(
		sfile,
		"  - pend request        : number of pend buffer requests\n");
	seq_printf(
		sfile,
		"  - pend error          : number of failed pend buffer requests\n");
	seq_printf(
		sfile,
		"  - pend_last_ts        : last time the process baked the pending buffer\n");
	seq_printf(
		sfile,
		"  - sys_latency_stats   : system latency stats. min, max, and percentile latency thresholds.\n");
	seq_printf(
		sfile,
		"  - sys_latency_last_ts : last time we saw a system msg processing time over threshold\n");
	seq_printf(
		sfile,
		"  - user_latency_stats  : user latency stats. min, max, and percentile latency thresholds.\n");
	seq_printf(
		sfile,
		"  - user_latency_last_ts: last time we saw a user msg processing time over threshold\n");
	seq_printf(
		sfile,
		"  - capacity_stats      : capacity stats. min, max, and percentile capacity thresholds.\n");
	seq_printf(sfile, "  - creation_ts         : creation timestamp");
	seq_printf(
		sfile,
		"  - destruction_ts      : destruction timestamp (only for inactive queues)");

	return 0;
}

static void arfw_debug_sampler_show(struct seq_file *sfile, const char *unit,
				    void *sampler)
{
	int min = -1, max = -1, p50 = -1, p95 = -1, p99 = -1;

	min = ar_binned_sampler_get_min(sampler);
	max = ar_binned_sampler_get_max(sampler);
	p50 = ar_binned_sampler_get_threshold(sampler, 50);
	p95 = ar_binned_sampler_get_threshold(sampler, 95);
	p99 = ar_binned_sampler_get_threshold(sampler, 99);

	seq_printf(
		sfile,
		"min: %d%s, max: %d%s, estimated p50: %d%s, p95: %d%s, p99: %d%s\n",
		min, unit, max, unit, p50, unit, p95, unit, p99, unit);
}

static int arfw_debug_help_open(struct inode *inode, struct file *file)
{
	return single_open(file, arfw_debug_help_show, inode->i_private);
}

static const struct file_operations arfw_debug_help_fops = {
	.open = arfw_debug_help_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * show a "low precision" timestamp. A time64_t only has resolution in seconds, so this
 * function formats and prints the time as date + time with seconds.
*/
static void arfw_debug_ts_show(struct seq_file *sfile, time64_t ts)
{
	struct tm last_tm;

	if (ts) {
		time64_to_tm(ts, 0, &last_tm);
		seq_printf(sfile, "%02d-%02d %02d:%02d:%02d\n",
			   last_tm.tm_mon + 1, last_tm.tm_mday, last_tm.tm_hour,
			   last_tm.tm_min, last_tm.tm_sec);
	} else {
		seq_puts(sfile, "null\n");
	}
}

/**
 * show a "high precision" timestamp. A ktim_t has at least millisecond resolution, so this
 * function formats and prints the time as date + time with seconds and milliseconds.
*/
static void arfw_debug_kt_show(struct seq_file *sfile, ktime_t kt)
{
	struct timespec64 ts;
	struct tm last_tm;

	if (kt) {
		ts = ktime_to_timespec64(kt);

		time64_to_tm(ts.tv_sec, 0, &last_tm);

		seq_printf(sfile, "%02d-%02d %02d:%02d:%02d.%03ld\n",
			   last_tm.tm_mon + 1, last_tm.tm_mday, last_tm.tm_hour,
			   last_tm.tm_min, last_tm.tm_sec,
			   ts.tv_nsec / NSEC_PER_MSEC);
	} else {
		seq_puts(sfile, "null\n");
	}
}

static void arfw_debug_print_latency_sampler(struct seq_file *sfile,
					     const char *tag, void *sampler,
					     time64_t last_ts)
{
	seq_printf(sfile,
		   "\t[%s] sample_count: %llu, last spike over %d us ts: ", tag,
		   ar_binned_sampler_get_num_samples(sampler), THRESHOLD_TIME);
	arfw_debug_ts_show(sfile, last_ts);

	if (ar_binned_sampler_get_num_samples(sampler) > 0) {
		seq_puts(sfile, "\t\t");
		arfw_debug_sampler_show(sfile, " us", sampler);
	}
}

static int arfw_debug_queue_regions_dumpbin_show(struct seq_file *sfile,
						 void *ignored)
{
	int id;
	struct arfw_client_queue *client;
	struct arfw_int_client_queue *queue;
	struct arfw_client_region *region = NULL;
	unsigned long flags;
	struct ar_queue_region_info region_info = { 0 };

	client = sfile->private;
	AR_ASSERT(client);

	queue = client->queue;
	AR_ASSERT(queue);

	spin_lock_irqsave(&queue->locked_regions.lock, flags);
	seq_write(sfile, &queue->locked_regions.num_regions, sizeof(uint32_t));
	idr_for_each_entry(&queue->locked_regions.region_idr, region, id) {
		if (region == NULL)
			continue;

		region_info.id = id;
		region_info.location = region->location;
		region_info.size = region->mapping.region_size;
		seq_write(sfile, &region_info, sizeof(region_info));
	};
	spin_unlock_irqrestore(&queue->locked_regions.lock, flags);

	return 0;
}

static int arfw_debug_queue_regions_dumpbin_open(struct inode *inode,
						 struct file *file)
{
	return single_open(file, arfw_debug_queue_regions_dumpbin_show,
			   inode->i_private);
}

static const struct file_operations arfw_debug_queue_regions_dumpbin_fops = {
	.open = arfw_debug_queue_regions_dumpbin_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int arfw_debug_queue_regions_dumpstr_show(struct seq_file *sfile,
						 void *ignored)
{
	int id;
	struct arfw_client_queue *client;
	struct arfw_int_client_queue *queue;
	struct arfw_client_region *region = NULL;
	unsigned long flags;

	client = sfile->private;
	AR_ASSERT(client);

	queue = client->queue;
	AR_ASSERT(queue);

	spin_lock_irqsave(&queue->locked_regions.lock, flags);
	idr_for_each_entry(&queue->locked_regions.region_idr, region, id) {
		seq_printf(sfile, "id: %d, size: %zu kB, loc: %s\n", id,
			   region->mapping.region_size / 1024,
			   region->location == EXTERNAL ? "external" :
								"aperture");
	};
	spin_unlock_irqrestore(&queue->locked_regions.lock, flags);

	return 0;
}

static int arfw_debug_queue_regions_dumpstr_open(struct inode *inode,
						 struct file *file)
{
	return single_open(file, arfw_debug_queue_regions_dumpstr_show,
			   inode->i_private);
}

static const struct file_operations arfw_debug_queue_regions_dumpstr_fops = {
	.open = arfw_debug_queue_regions_dumpstr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int arfw_debug_queue_dumpstr_show(struct seq_file *sfile, void *ignored)
{
	struct arfw_client_queue *client;
	struct arfw_int_client_queue *queue;

	client = sfile->private;
	AR_ASSERT(client);
	queue = client->queue;
	AR_ASSERT(queue);
	seq_printf(
		sfile,
		"[0x%04x%s0x%04x]: id: %d, mirror: %d, status: ACTIVE, creation_ts: ",
		queue->hlos_endpoint_id,
		(queue->direction == AR_QUEUE_HLOS_TO_FW) ? "->" : "<-",
		queue->fw_endpoint_id, queue->id, queue->mirror);
	arfw_debug_kt_show(sfile, client->debug.creation_ts);
	seq_printf(sfile, "\tpid: %d, comm: %s, depth: %d, regions: %d\n",
		   client->debug.pid, client->debug.comm,
		   queue->queue_meta.depth, queue->locked_regions.num_regions);
	seq_printf(sfile,
		   "\tmessage count: %lu, consumer_idx: %d, producer_idx: %d\n",
		   client->debug.msg_count,
		   ar_queue_get_consumer_idx(&queue->arfw_queue.submit_queue),
		   ar_queue_get_producer_idx(&queue->arfw_queue.submit_queue));
	seq_printf(
		sfile,
		"\tproducer_inflight: %u, consumer_iteration: %d, producer_iteration: %d\n",
		ar_queue_get_producer_inflight(&queue->arfw_queue.submit_queue),
		ar_queue_get_consumer_iteration(
			&queue->arfw_queue.submit_queue),
		ar_queue_get_producer_iteration(
			&queue->arfw_queue.submit_queue));
	seq_printf(
		sfile,
		"\tinline: %lu, external: %lu, consumed: %lu, drop: %lu, last_msg_ts: ",
		client->debug.inline_count, client->debug.external_count,
		client->debug.consumed_count, client->debug.drop_count);
	arfw_debug_kt_show(sfile, client->debug.msg_activity_last_ts);
	if (queue->direction == AR_QUEUE_FW_TO_HLOS) {
		seq_printf(
			sfile,
			"\t[pend buffers] active: %lu, count: %lu, request: %lu, error: %lu, last ts: ",
			client->debug.pend_active_count,
			client->debug.pend_count, client->debug.pend_req_count,
			client->debug.pend_error_count);
		arfw_debug_kt_show(sfile, client->debug.pend_last_ts);
	}

	arfw_debug_print_latency_sampler(
		sfile, "sys latency counters", client->debug.latency_sampler,
		client->debug.latency_threshold_last_ts);
	if (queue->direction == AR_QUEUE_FW_TO_HLOS)
		arfw_debug_print_latency_sampler(
			sfile, "user latency counters",
			client->debug.user_latency_sampler,
			client->debug.user_latency_threshold_last_ts);

	if (ar_binned_sampler_get_num_samples(client->debug.capacity_sampler) >
	    0) {
		seq_printf(sfile, "\t[capacity stats] ");
		arfw_debug_sampler_show(sfile, "",
					client->debug.capacity_sampler);
	}

	return 0;
}

static int arfw_debug_queue_dumpstr_open(struct inode *inode, struct file *file)
{
	return single_open(file, arfw_debug_queue_dumpstr_show,
			   inode->i_private);
}

static const struct file_operations arfw_debug_queue_dumpstr_fops = {
	.open = arfw_debug_queue_dumpstr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int arfw_debug_inactive_queue_dumpstr_show(struct seq_file *sfile,
						  void *ignored)
{
	struct inactive_queue_debug *inactive;

	inactive = sfile->private;
	AR_ASSERT(inactive);
	seq_printf(
		sfile,
		"[0x%04x%s0x%04x]: id: %d, mirror: %d, status: INACTIVE, creation_ts: ",
		inactive->hlos_endpoint_id,
		(inactive->direction == AR_QUEUE_HLOS_TO_FW) ? "->" : "<-",
		inactive->fw_endpoint_id, inactive->id, inactive->mirror);
	arfw_debug_kt_show(sfile, inactive->creation_ts);
	seq_printf(
		sfile,
		"\tpid: %d, comm: %s, depth: %d, shutdown_reason: %d, destruction_ts: ",
		inactive->pid, inactive->comm, inactive->depth,
		inactive->shutdown_reason);
	arfw_debug_kt_show(sfile, inactive->destruction_ts);
	seq_printf(sfile,
		   "\tmessage count: %lu, consumer_idx: %d, producer_idx: %d\n",
		   inactive->msg_count, inactive->consumer_idx,
		   inactive->producer_idx);
	seq_printf(
		sfile,
		"\tproducer_inflight: %u, consumer_iteration: %d, producer_iteration: %d\n",
		inactive->producer_inflight, inactive->consumer_iteration,
		inactive->producer_iteration);
	seq_printf(
		sfile,
		"\tinline: %lu, external: %lu, consumed: %lu, drop: %lu, last_msg_ts: ",
		inactive->inline_count, inactive->external_count,
		inactive->consumed_count, inactive->drop_count);
	arfw_debug_kt_show(sfile, inactive->msg_activity_last_ts);

	if (inactive->direction == AR_QUEUE_FW_TO_HLOS) {
		seq_printf(
			sfile,
			"\t[pend buffers] active: %lu, count: %lu, request: %lu, error: %lu, last ts: ",
			inactive->pend_active_count, inactive->pend_count,
			inactive->pend_req_count, inactive->pend_error_count);
		arfw_debug_kt_show(sfile, inactive->pend_last_ts);
	}

	arfw_debug_print_latency_sampler(sfile, "sys latency counters",
					 inactive->latency_sampler,
					 inactive->latency_threshold_last_ts);
	if (inactive->direction == AR_QUEUE_FW_TO_HLOS)
		arfw_debug_print_latency_sampler(
			sfile, "user latency counters",
			inactive->user_latency_sampler,
			inactive->user_latency_threshold_last_ts);

	if (ar_binned_sampler_get_num_samples(inactive->capacity_sampler) > 0) {
		seq_printf(sfile, "\t[capacity stats] ");
		arfw_debug_sampler_show(sfile, "", inactive->capacity_sampler);
	}

	return 0;
}

static int arfw_debug_inactive_queue_dumpstr_open(struct inode *inode,
						  struct file *file)
{
	return single_open(file, arfw_debug_inactive_queue_dumpstr_show,
			   inode->i_private);
}

static const struct file_operations arfw_debug_inactive_queue_dumpstr_fops = {
	.open = arfw_debug_inactive_queue_dumpstr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void arfw_debug_sampler_get_stats(void *sampler,
					 struct ar_queue_sampler_stats *stats)
{
	stats->num_samples = ar_binned_sampler_get_num_samples(sampler);

	// if num_samples is 0, then the sampler is empty and all values are -1.
	if (stats->num_samples == 0)
		return;

	stats->min = ar_binned_sampler_get_min(sampler);
	stats->max = ar_binned_sampler_get_max(sampler);
	stats->p50 = ar_binned_sampler_get_threshold(sampler, 50);
	stats->p90 = ar_binned_sampler_get_threshold(sampler, 90);
	stats->p95 = ar_binned_sampler_get_threshold(sampler, 95);
	stats->p99 = ar_binned_sampler_get_threshold(sampler, 99);
}

static int arfw_debug_queue_dumpbin_show(struct seq_file *sfile, void *ignored)
{
	struct arfw_client_queue *client;
	struct arfw_int_client_queue *queue;
	struct ar_queue_debug debug_info = {};

	client = sfile->private;
	AR_ASSERT(client);
	queue = client->queue;
	AR_ASSERT(queue);

	debug_info.id = queue->id;
	debug_info.hlos_endpoint_id = queue->hlos_endpoint_id;
	debug_info.fw_endpoint_id = queue->fw_endpoint_id;
	debug_info.direction = queue->direction;
	debug_info.mirror = queue->mirror;
	debug_info.depth = queue->queue_meta.depth;

	debug_info.consumer_idx =
		ar_queue_get_consumer_idx(&queue->arfw_queue.submit_queue);
	debug_info.producer_idx =
		ar_queue_get_producer_idx(&queue->arfw_queue.submit_queue);
	debug_info.producer_inflight =
		ar_queue_get_producer_inflight(&queue->arfw_queue.submit_queue);
	debug_info.consumer_iteration = ar_queue_get_consumer_iteration(
		&queue->arfw_queue.submit_queue);
	debug_info.producer_iteration = ar_queue_get_producer_iteration(
		&queue->arfw_queue.submit_queue);

	debug_info.pid = client->debug.pid;
	memcpy(debug_info.comm, client->debug.comm, TASK_COMM_LEN);
	debug_info.creation_ts = client->debug.creation_ts;
	debug_info.msg_count = client->debug.msg_count;
	debug_info.inline_count = client->debug.inline_count;
	debug_info.external_count = client->debug.external_count;
	debug_info.drop_count = client->debug.drop_count;
	debug_info.consumed_count = client->debug.consumed_count;
	debug_info.msg_activity_last_ts = client->debug.msg_activity_last_ts;
	debug_info.pend_count = client->debug.pend_count;
	debug_info.pend_active_count = client->debug.pend_active_count;
	debug_info.pend_error_count = client->debug.pend_error_count;
	debug_info.pend_req_count = client->debug.pend_req_count;
	debug_info.pend_last_ts = client->debug.pend_last_ts;
	arfw_debug_sampler_get_stats(client->debug.latency_sampler,
				     &debug_info.sys_latency_stats);
	debug_info.latency_threshold_last_ts =
		client->debug.latency_threshold_last_ts;
	arfw_debug_sampler_get_stats(client->debug.user_latency_sampler,
				     &debug_info.user_latency_stats);
	debug_info.user_latency_threshold_last_ts =
		client->debug.user_latency_threshold_last_ts;
	arfw_debug_sampler_get_stats(client->debug.capacity_sampler,
				     &debug_info.capacity_stats);

	return seq_write(sfile, &debug_info, sizeof(debug_info));
}

static int arfw_debug_queue_dumpbin_open(struct inode *inode, struct file *file)
{
	return single_open(file, arfw_debug_queue_dumpbin_show,
			   inode->i_private);
}

static const struct file_operations arfw_debug_dumpbin_fops = {
	.open = arfw_debug_queue_dumpbin_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int arfw_debug_inactive_queue_dumpbin_show(struct seq_file *sfile,
						  void *ignored)
{
	struct inactive_queue_debug *inactive;
	struct ar_queue_debug debug_info = {};

	inactive = sfile->private;
	AR_ASSERT(inactive);

	debug_info.id = inactive->id;
	debug_info.hlos_endpoint_id = inactive->hlos_endpoint_id;
	debug_info.fw_endpoint_id = inactive->fw_endpoint_id;
	debug_info.direction = inactive->direction;
	debug_info.mirror = inactive->mirror;
	debug_info.depth = inactive->depth;

	debug_info.consumer_idx = inactive->consumer_idx;
	debug_info.producer_idx = inactive->producer_idx;
	debug_info.producer_inflight = inactive->producer_inflight;
	debug_info.consumer_iteration = inactive->consumer_iteration;
	debug_info.producer_iteration = inactive->producer_iteration;

	debug_info.pid = inactive->pid;
	memcpy(debug_info.comm, inactive->comm, TASK_COMM_LEN);
	debug_info.creation_ts = inactive->creation_ts;
	debug_info.destruction_ts = inactive->destruction_ts;
	debug_info.shutdown_reason = inactive->shutdown_reason;
	debug_info.msg_count = inactive->msg_count;
	debug_info.inline_count = inactive->inline_count;
	debug_info.external_count = inactive->external_count;
	debug_info.drop_count = inactive->drop_count;
	debug_info.consumed_count = inactive->consumed_count;
	debug_info.msg_activity_last_ts = inactive->msg_activity_last_ts;
	debug_info.pend_count = inactive->pend_count;
	debug_info.pend_error_count = inactive->pend_error_count;
	debug_info.pend_active_count = inactive->pend_active_count;
	debug_info.pend_req_count = inactive->pend_req_count;
	debug_info.pend_last_ts = inactive->pend_last_ts;
	arfw_debug_sampler_get_stats(inactive->latency_sampler,
				     &debug_info.sys_latency_stats);
	debug_info.latency_threshold_last_ts =
		inactive->latency_threshold_last_ts;
	arfw_debug_sampler_get_stats(inactive->user_latency_sampler,
				     &debug_info.user_latency_stats);
	debug_info.user_latency_threshold_last_ts =
		inactive->user_latency_threshold_last_ts;
	arfw_debug_sampler_get_stats(inactive->capacity_sampler,
				     &debug_info.capacity_stats);

	return seq_write(sfile, &debug_info, sizeof(debug_info));
}

static int arfw_debug_inactive_queue_dumpbin_open(struct inode *inode,
						  struct file *file)
{
	return single_open(file, arfw_debug_inactive_queue_dumpbin_show,
			   inode->i_private);
}

static const struct file_operations arfw_debug_dumpbin_inactive_fops = {
	.open = arfw_debug_inactive_queue_dumpbin_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * create all the boilerplate for printing debug info as file nodes
 */
//command line
DEBUG_FS_OPS_CREATE(arfw_debug_comm, print_str, comm);

// operational timestamps, high precision (arfw_debug_kt_show)
DEBUG_FS_OPS_CREATE(arfw_debug_pend_last_ts_fops, arfw_debug_kt_show,
		    pend_last_ts);
DEBUG_FS_OPS_CREATE(arfw_debug_msg_last_ts_fops, arfw_debug_kt_show,
		    msg_activity_last_ts);
DEBUG_FS_OPS_CREATE(arfw_debug_creation_ts_fops, arfw_debug_kt_show,
		    creation_ts);
INACTIVE_DEBUG_FS_OPS_CREATE(arfw_debug_destruction_ts_fops, arfw_debug_kt_show,
			     destruction_ts);

// operational timestamps, low precision (arfw_debug_ts_show)
DEBUG_FS_OPS_CREATE(arfw_debug_sys_latency_last_ts_fops, arfw_debug_ts_show,
		    latency_threshold_last_ts);
DEBUG_FS_OPS_CREATE(arfw_debug_user_latency_last_ts_fops, arfw_debug_ts_show,
		    user_latency_threshold_last_ts);

// statistical samplers
DEBUG_SAMPLER_FS_OPS_CREATE(arfw_debug_sys_latency_stats, latency_sampler);
DEBUG_SAMPLER_FS_OPS_CREATE(arfw_debug_user_latency_stats,
			    user_latency_sampler);
DEBUG_SAMPLER_FS_OPS_CREATE(arfw_debug_capacity_stats, capacity_sampler);

void arfw_debug_queue_add(struct arfw_client_queue *client)
{
	struct dentry *dir, *latency_dir, *user_latency_dir, *capacity_dir;
	struct arfw_int_client_queue *queue;
	char path[ARFW_PATH_LEN];

	if (!arfw_debug_queue_dir)
		return;

	AR_ASSERT(client);
	queue = client->queue;
	arfw_debug_queue_path_create(path, ARFW_PATH_LEN, queue);
	dir = debugfs_create_dir(path, arfw_debug_queue_dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(queue, AR_LOG_DEBUGFS_QUEUE,
				      "Can't create new directory for queue",
				      "id %d, path %s", queue->id, path);
		return;
	}

	debugfs_create_file("dumpstr", S_IRUSR, dir, client,
			    &arfw_debug_queue_dumpstr_fops);
	debugfs_create_file("comm", S_IRUSR, dir, client, &arfw_debug_comm);
	debugfs_create_u32("id", S_IRUSR, dir, &queue->id);
	debugfs_create_x16("hlos_endpoint_id", S_IRUSR, dir,
			   &queue->hlos_endpoint_id);
	debugfs_create_x16("fw_endpoint_id", S_IRUSR, dir,
			   &queue->fw_endpoint_id);
	debugfs_create_u32("direction", S_IRUSR, dir, &queue->direction);
	debugfs_create_bool("mirror", S_IRUSR, dir, &queue->mirror);
	debugfs_create_u32("pid", S_IRUSR, dir, &client->debug.pid);
	debugfs_create_u16("depth", S_IRUSR, dir, &queue->queue_meta.depth);
	debugfs_create_ulong("msg_count", S_IRUSR, dir,
			     &client->debug.msg_count);
	// consumer_idx and producer_idx are available through the "dumpstr" node.
	// It is hard to provide the address of the ar_queue_get_consumer_* or
	// ar_queue_get_producer_* return values. These fields are internal.
	// If it will be required, the nodes with the file operations will be
	// added in the future.
	debugfs_create_ulong("inline_count", S_IRUSR, dir,
			     &client->debug.inline_count);
	debugfs_create_ulong("external_count", S_IRUSR, dir,
			     &client->debug.external_count);
	debugfs_create_ulong("consumed_count", S_IRUSR, dir,
			     &client->debug.consumed_count);
	debugfs_create_ulong("drop_count", S_IRUSR, dir,
			     &client->debug.drop_count);
	debugfs_create_ulong("pend_active_count", S_IRUSR, dir,
			     &client->debug.pend_active_count);
	debugfs_create_ulong("pend_count", S_IRUSR, dir,
			     &client->debug.pend_count);
	debugfs_create_ulong("pend_error_count", S_IRUSR, dir,
			     &client->debug.pend_error_count);
	debugfs_create_ulong("pend_req", S_IRUSR, dir,
			     &client->debug.pend_req_count);
	debugfs_create_file("pend_last_ts", S_IRUSR, dir, client,
			    &arfw_debug_pend_last_ts_fops);
	debugfs_create_u64("pend_last_ts_raw_ktime", S_IRUSR, dir,
			   &client->debug.pend_last_ts);
	debugfs_create_file("msg_last_ts", S_IRUSR, dir, client,
			    &arfw_debug_msg_last_ts_fops);
	debugfs_create_u64("msg_last_ts_raw_ktime", S_IRUSR, dir,
			   &client->debug.msg_activity_last_ts);
	debugfs_create_file("creation_ts", S_IRUSR, dir, client,
			    &arfw_debug_creation_ts_fops);
	debugfs_create_u64("creation_ts_raw_ktime", S_IRUSR, dir,
			   &client->debug.creation_ts);

	debugfs_create_file("dumpbin", S_IRUSR, dir, client,
			    &arfw_debug_dumpbin_fops);

	// create sys latency info directory
	latency_dir = debugfs_create_dir("sys_latency", dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't create new directory for queue sys latency info",
			"id %d", queue->id);
		return;
	}
	debugfs_create_file("last_ts", S_IRUSR, latency_dir, client,
			    &arfw_debug_sys_latency_last_ts_fops);
	debugfs_create_u64("last_ts_raw_time64", S_IRUSR, latency_dir,
			   &client->debug.latency_threshold_last_ts);
	DEBUG_SAMPLER_CREATE_FILES(latency_dir, client,
				   arfw_debug_sys_latency_stats);

	// create user latency info directory
	user_latency_dir = debugfs_create_dir("user_latency", dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't create new directory for queue sys latency info",
			"id %d", queue->id);
		return;
	}
	debugfs_create_file("last_ts", S_IRUSR, user_latency_dir, client,
			    &arfw_debug_user_latency_last_ts_fops);
	debugfs_create_u64("last_ts_raw_time64", S_IRUSR, user_latency_dir,
			   &client->debug.user_latency_threshold_last_ts);
	DEBUG_SAMPLER_CREATE_FILES(user_latency_dir, client,
				   arfw_debug_user_latency_stats);

	capacity_dir = debugfs_create_dir("capacity_stats", dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't create new directory for queue sys latency info",
			"id %d", queue->id);
		return;
	}
	DEBUG_SAMPLER_CREATE_FILES(capacity_dir, client,
				   arfw_debug_capacity_stats);

	// add memory regions nodes
	debugfs_create_file("regions_bin", S_IRUSR, dir, client,
			    &arfw_debug_queue_regions_dumpbin_fops);
	debugfs_create_file("regions", S_IRUSR, dir, client,
			    &arfw_debug_queue_regions_dumpstr_fops);
}

void arfw_debug_queue_remove(struct arfw_client_queue *client)
{
	struct dentry *dir;
	char path[ARFW_PATH_LEN];
	struct arfw_int_client_queue *queue;

	if (!arfw_debug_queue_dir)
		return;

	AR_ASSERT(client);
	queue = client->queue;
	arfw_debug_queue_path_create(path, ARFW_PATH_LEN, queue);
	dir = debugfs_lookup(path, arfw_debug_queue_dir);
	if (!dir)
		return;

	debugfs_remove_recursive(dir);

	// Put the queue to the inactive array, for the debugging purposes.
	arfw_debug_inactive_queue_add(client, path);
}

static void
arfw_debug_inactive_queue_init(struct inactive_queue_debug *inactive,
			       struct arfw_client_queue *client)
{
	AR_ASSERT(inactive);
	AR_ASSERT(client);

	inactive->id = client->queue->id;
	inactive->hlos_endpoint_id = client->queue->hlos_endpoint_id;
	inactive->fw_endpoint_id = client->queue->fw_endpoint_id;
	inactive->direction = client->queue->direction;
	inactive->mirror = client->queue->mirror;
	inactive->depth = client->queue->queue_meta.depth;

	inactive->consumer_idx = ar_queue_get_consumer_idx(
		&client->queue->arfw_queue.submit_queue);
	inactive->producer_idx = ar_queue_get_producer_idx(
		&client->queue->arfw_queue.submit_queue);
	inactive->producer_inflight = ar_queue_get_producer_inflight(
		&client->queue->arfw_queue.submit_queue);
	inactive->consumer_iteration = ar_queue_get_consumer_iteration(
		&client->queue->arfw_queue.submit_queue);
	inactive->producer_iteration = ar_queue_get_producer_iteration(
		&client->queue->arfw_queue.submit_queue);

	inactive->pid = client->debug.pid;
	strncpy(inactive->comm, client->debug.comm, TASK_COMM_LEN);
	inactive->creation_ts = client->debug.creation_ts;
	inactive->destruction_ts = ktime_get_real();
	inactive->shutdown_reason = client->queue->shutdown_reason;
	inactive->msg_count = client->debug.msg_count;
	inactive->inline_count = client->debug.inline_count;
	inactive->external_count = client->debug.external_count;
	inactive->consumed_count = client->debug.consumed_count;
	inactive->msg_activity_last_ts = client->debug.msg_activity_last_ts;
	inactive->drop_count = client->debug.drop_count;
	inactive->pend_count = client->debug.pend_count;
	inactive->pend_error_count = client->debug.pend_error_count;
	inactive->pend_active_count = client->debug.pend_active_count;
	inactive->pend_req_count = client->debug.pend_req_count;
	memcpy(&inactive->pend_last_ts, &client->debug.pend_last_ts,
	       sizeof(inactive->pend_last_ts));
	inactive->latency_threshold_last_ts =
		client->debug.latency_threshold_last_ts;
	inactive->user_latency_threshold_last_ts =
		client->debug.user_latency_threshold_last_ts;

	memcpy(inactive->latency_sampler, client->debug.latency_sampler,
	       sizeof(inactive->latency_sampler));
	memcpy(inactive->user_latency_sampler,
	       client->debug.user_latency_sampler,
	       sizeof(inactive->user_latency_sampler));
	memcpy(inactive->capacity_sampler, client->debug.capacity_sampler,
	       sizeof(inactive->capacity_sampler));
}

static void arfw_debug_inactive_queue_add(struct arfw_client_queue *client,
					  char *path)
{
	struct dentry *dir, *latency_dir, *user_latency_dir, *capacity_dir;
	struct arfw_int_client_queue *queue;
	struct inactive_queue_debug *inactive;
	char new_path[ARFW_PATH_LEN];
	struct inode *inode;

	if (!arfw_debug_inactive_queue_dir)
		return;

	AR_ASSERT(client);
	queue = client->queue;
	if (mutex_lock_interruptible(&inactive_queues_lock)) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't grab a lock to update inactive queues", "");
		return;
	}

	dir = debugfs_lookup(path, arfw_debug_inactive_queue_dir);
	if (dir) {
		// Check if we already have the queue with such name. This will be the
		// case if the queue is created/removed in the loop. In this case reuse
		// the node.
		inactive = d_inode(dir)->i_private;
		arfw_debug_inactive_queue_remove(inactive);
		// Remove it, so it will be added to the tail.
		list_del(&inactive->list_node);
	} else if (inactive_queues_num < ARFW_INACTIVE_QUEUES_NUM) {
		// There are still free slots available.
		inactive = &arfw_debug_inactive_queue[inactive_queues_num];
		inactive_queues_num++;
	} else {
		// No free slot, just remove the oldest one.
		inactive = list_first_entry(&inactive_queues_list,
					    struct inactive_queue_debug,
					    list_node);
		arfw_debug_inactive_queue_remove(inactive);
		list_del(&inactive->list_node);
	}

	// Create directory for the inactive queue.
	dir = debugfs_create_dir(path, arfw_debug_inactive_queue_dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't create new directory for inactive queue",
			"id %d, path %s", queue->id, new_path);
		goto exit_unlock;
	}

	// Store all the required information about the queue into the internal debug structure.
	inactive->dir = dir;
	arfw_debug_inactive_queue_init(inactive, client);
	inode = d_inode(dir);
	inode->i_private = inactive;
	list_add(&inactive->list_node, &inactive_queues_list);

	debugfs_create_file("dumpstr", S_IRUSR, dir, inactive,
			    &arfw_debug_inactive_queue_dumpstr_fops);
	debugfs_create_file("comm", S_IRUSR, dir, inactive,
			    &arfw_debug_comm_inactive);
	debugfs_create_u32("id", S_IRUSR, dir, &inactive->id);
	debugfs_create_x16("hlos_endpoint_id", S_IRUSR, dir,
			   &inactive->hlos_endpoint_id);
	debugfs_create_x16("fw_endpoint_id", S_IRUSR, dir,
			   &inactive->fw_endpoint_id);
	debugfs_create_u32("direction", S_IRUSR, dir, &inactive->direction);
	debugfs_create_bool("mirror", S_IRUSR, dir, &inactive->mirror);
	debugfs_create_u32("pid", S_IRUSR, dir, &inactive->pid);
	debugfs_create_u16("depth", S_IRUSR, dir, &inactive->depth);
	debugfs_create_ulong("msg_count", S_IRUSR, dir, &inactive->msg_count);
	debugfs_create_u16("consumer_idx", S_IRUSR, dir,
			   &inactive->consumer_idx);
	debugfs_create_u16("producer_idx", S_IRUSR, dir,
			   &inactive->producer_idx);
	debugfs_create_u16("producer_inflight", S_IRUSR, dir,
			   &inactive->producer_inflight);
	debugfs_create_u16("consumer_iteration", S_IRUSR, dir,
			   &inactive->consumer_iteration);
	debugfs_create_u16("producer_iteration", S_IRUSR, dir,
			   &inactive->producer_iteration);
	debugfs_create_ulong("inline_count", S_IRUSR, dir,
			     &inactive->inline_count);
	debugfs_create_ulong("external_count", S_IRUSR, dir,
			     &inactive->external_count);
	debugfs_create_ulong("consumed_count", S_IRUSR, dir,
			     &inactive->consumed_count);
	debugfs_create_ulong("drop_count", S_IRUSR, dir, &inactive->drop_count);
	debugfs_create_ulong("pend_active_count", S_IRUSR, dir,
			     &inactive->pend_active_count);
	debugfs_create_ulong("pend_count", S_IRUSR, dir, &inactive->pend_count);
	debugfs_create_ulong("pend_error_count", S_IRUSR, dir,
			     &inactive->pend_error_count);
	debugfs_create_ulong("pend_req", S_IRUSR, dir,
			     &inactive->pend_req_count);
	debugfs_create_file("pend_last_ts", S_IRUSR, dir, inactive,
			    &arfw_debug_pend_last_ts_fops_inactive);
	debugfs_create_u64("pend_last_ts_raw_ktime", S_IRUSR, dir,
			   &inactive->pend_last_ts);
	debugfs_create_file("msg_last_ts", S_IRUSR, dir, inactive,
			    &arfw_debug_msg_last_ts_fops_inactive);
	debugfs_create_u64("msg_last_ts_raw_ktime", S_IRUSR, dir,
			   &inactive->msg_activity_last_ts);
	debugfs_create_file("creation_ts", S_IRUSR, dir, inactive,
			    &arfw_debug_creation_ts_fops_inactive);
	debugfs_create_u64("creation_ts_raw_ktime", S_IRUSR, dir,
			   &inactive->creation_ts);
	debugfs_create_file("destruction_ts", S_IRUSR, dir, inactive,
			    &arfw_debug_destruction_ts_fops);
	debugfs_create_u64("destruction_ts_raw_ktime", S_IRUSR, dir,
			   &inactive->destruction_ts);
	debugfs_create_u32("shutdown_reason", S_IRUSR, dir,
			   &inactive->shutdown_reason);

	debugfs_create_file("dumpbin", S_IRUSR, dir, inactive,
			    &arfw_debug_dumpbin_inactive_fops);

	// create sys latency info directory
	latency_dir = debugfs_create_dir("sys_latency", dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't create new directory for queue sys latency info",
			"id %d", queue->id);
		goto exit_unlock;
	}
	debugfs_create_file("last_ts", S_IRUSR, latency_dir, inactive,
			    &arfw_debug_sys_latency_last_ts_fops_inactive);
	debugfs_create_u64("last_ts_raw_time64", S_IRUSR, latency_dir,
			   &inactive->latency_threshold_last_ts);
	DEBUG_SAMPLER_CREATE_FILES(latency_dir, inactive,
				   arfw_debug_sys_latency_stats_inactive);

	// create user latency info directory
	user_latency_dir = debugfs_create_dir("user_latency", dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't create new directory for queue sys latency info",
			"id %d", queue->id);
		goto exit_unlock;
	}
	debugfs_create_file("last_ts", S_IRUSR, user_latency_dir, inactive,
			    &arfw_debug_user_latency_last_ts_fops_inactive);
	debugfs_create_u64("last_ts_raw_time64", S_IRUSR, user_latency_dir,
			   &inactive->user_latency_threshold_last_ts);
	DEBUG_SAMPLER_CREATE_FILES(user_latency_dir, inactive,
				   arfw_debug_user_latency_stats_inactive);

	capacity_dir = debugfs_create_dir("capacity_stats", dir);
	if (!dir) {
		AR_LOG_ARFW_QUEUE_ERR(
			queue, AR_LOG_DEBUGFS_QUEUE,
			"Can't create new directory for queue sys latency info",
			"id %d", queue->id);
		goto exit_unlock;
	}
	DEBUG_SAMPLER_CREATE_FILES(capacity_dir, inactive,
				   arfw_debug_capacity_stats_inactive);

exit_unlock:
	mutex_unlock(&inactive_queues_lock);
}

static void
arfw_debug_inactive_queue_remove(struct inactive_queue_debug *inactive)
{
	struct dentry *dir = inactive->dir;

	AR_ASSERT(mutex_is_locked(&inactive_queues_lock));

	// remove debufs files before freeing memory
	if (arfw_debug_inactive_queue_dir && dir)
		debugfs_remove_recursive(dir);
}

void arfw_debug_device_add(const char *device)
{
	struct dentry *device_node;

	if (!arfw_debug_device_dir) {
		AR_LOG_ARFW_ERR(AR_LOG_DEBUGFS_DEVICE,
				"Device folder is not initialized");
		return;
	}

	AR_ASSERT(device);
	device_node = debugfs_lookup(device, arfw_debug_queue_dir);
	if (device_node) {
		AR_LOG_ARFW_ERR(AR_LOG_DEBUGFS_DEVICE,
				"Device node already exists: %s", device);
		return;
	}

	debugfs_create_file(device, S_IRUSR, arfw_debug_device_dir, NULL, NULL);
}

void arfw_debug_device_remove(const char *device)
{
	struct dentry *device_node;

	if (!arfw_debug_device_dir) {
		AR_LOG_ARFW_ERR(AR_LOG_DEBUGFS_DEVICE,
				"Device folder is not initialized");
		return;
	}

	AR_ASSERT(device);
	device_node = debugfs_lookup(device, arfw_debug_device_dir);
	if (!device_node) {
		AR_LOG_ARFW_ERR(AR_LOG_DEBUGFS_DEVICE,
				"No device node found to remove: %s", device);
		return;
	}

	debugfs_remove(device_node);
}

static int arfw_debug_queue_init(void)
{
	arfw_debug_queue_dir =
		debugfs_create_dir(ARFW_DEBUG_QUEUE_PATH, arfw_debug_root_dir);
	if (!arfw_debug_queue_dir) {
		AR_LOG_ARFW_ERR(
			AR_LOG_DEBUGFS_QUEUE,
			"Can't initialize debug FS for AR firmware queues: %s.",
			ARFW_DEBUG_QUEUE_PATH);
		return -ENOMEM;
	}
	arfw_debug_inactive_queue_dir = debugfs_create_dir(
		ARFW_DEBUG_INACTIVE_QUEUE_PATH, arfw_debug_root_dir);
	if (!arfw_debug_inactive_queue_dir) {
		AR_LOG_ARFW_ERR(
			AR_LOG_DEBUGFS_QUEUE,
			"Can't initialize debug FS for inactive AR firmware queues: %s.",
			ARFW_DEBUG_INACTIVE_QUEUE_PATH);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&inactive_queues_list);

	return 0;
}

static int arfw_debug_device_init(void)
{
	arfw_debug_device_dir =
		debugfs_create_dir(ARFW_DEBUG_DEVICE_PATH, arfw_debug_root_dir);
	if (!arfw_debug_device_dir) {
		AR_LOG_ARFW_ERR(
			AR_LOG_DEBUGFS_DEVICE,
			"Can't initialize debug FS for AR firmware devices: %s.",
			ARFW_DEBUG_DEVICE_PATH);
		return -ENOMEM;
	}

	return 0;
}

int arfw_debug_init(void)
{
	int err;

	if (!debugfs_initialized())
		return 0;

	arfw_debug_root_dir = debugfs_create_dir(ARFW_DEBUG_ROOT_PATH, NULL);
	if (!arfw_debug_root_dir) {
		AR_LOG_ARFW_ERR(
			AR_LOG_DEBUGFS_QUEUE,
			"Can't initialize debug FS for AR firmware IPC: %s.",
			ARFW_DEBUG_ROOT_PATH);
		err = -ENOMEM;
		goto error_destroy;
	}

	err = arfw_debug_queue_init();
	if (err)
		goto error_destroy;

	err = arfw_debug_device_init();
	if (err)
		goto error_destroy;

	debugfs_create_file("help", S_IRUSR, arfw_debug_root_dir, NULL,
			    &arfw_debug_help_fops);

	return 0;

error_destroy:
	arfw_debug_destroy();

	return err;
}

void arfw_debug_destroy(void)
{
	if (!arfw_debug_root_dir) {
		return;
	}
	debugfs_remove_recursive(arfw_debug_root_dir);
	// No clean up for inactive_queues_list, since the static array is used
	// to store the nodes.

	arfw_debug_queue_dir = NULL;
	arfw_debug_device_dir = NULL;
	arfw_debug_inactive_queue_dir = NULL;
	arfw_debug_root_dir = NULL;
}

void arfw_debug_dump_queue(struct arfw_client_queue *client)
{
	struct arfw_int_client_queue *queue;

	AR_ASSERT(client);
	queue = client->queue;
	AR_ASSERT(queue);

	AR_LOG_ARFW_INFO(
		AR_LOG_DEBUGFS_QUEUE, "[0x%04x%s0x%04x]: id: %d, mirror: %d",
		queue->hlos_endpoint_id,
		(queue->direction == AR_QUEUE_HLOS_TO_FW) ? "->" : "<-",
		queue->fw_endpoint_id, queue->id, queue->mirror);
	AR_LOG_ARFW_INFO(AR_LOG_DEBUGFS_QUEUE, "pid: %d, comm: %s, depth: %d",
			 client->debug.pid, client->debug.comm,
			 queue->queue_meta.depth);
	AR_LOG_ARFW_INFO(
		AR_LOG_DEBUGFS_QUEUE,
		"message count: %lu, consumer_idx: %d, producer_idx: %d",
		client->debug.msg_count,
		ar_queue_get_consumer_idx(&queue->arfw_queue.submit_queue),
		ar_queue_get_producer_idx(&queue->arfw_queue.submit_queue));
	AR_LOG_ARFW_INFO(
		AR_LOG_DEBUGFS_QUEUE,
		"producer_inflight: %u, consumer_iteration: %d, producer_iteration: %d",
		ar_queue_get_producer_inflight(&queue->arfw_queue.submit_queue),
		ar_queue_get_consumer_iteration(
			&queue->arfw_queue.submit_queue),
		ar_queue_get_producer_iteration(
			&queue->arfw_queue.submit_queue));
	AR_LOG_ARFW_INFO(AR_LOG_DEBUGFS_QUEUE,
			 "inline: %lu, external: %lu, consumed: %lu, drop: %lu",
			 client->debug.inline_count,
			 client->debug.external_count,
			 client->debug.consumed_count,
			 client->debug.drop_count);
	if (queue->direction == AR_QUEUE_FW_TO_HLOS) {
		AR_LOG_ARFW_INFO(
			AR_LOG_DEBUGFS_QUEUE,
			"[pend buffers] active: %lu, count: %lu, error: %lu, req: %lu",
			client->debug.pend_active_count,
			client->debug.pend_count,
			client->debug.pend_error_count,
			client->debug.pend_req_count);
	}
}

int arfw_queue_debug_samplers_create(struct device *dev,
				     struct arfw_client_queue *client,
				     struct ar_queue_create_req *req)
{
	int capacity_threshold = 0, capacity_count = 0, capacity_increment = 0;
	int capacity_thresholds[CAPACITY_MAX_BUCKET_COUNT];

	ar_binned_sampler_init(client->debug.latency_sampler,
			       LATENCY_THRESHOLDS_US,
			       ARRAY_SIZE(LATENCY_THRESHOLDS_US));

	ar_binned_sampler_init(client->debug.user_latency_sampler,
			       LATENCY_THRESHOLDS_US,
			       ARRAY_SIZE(LATENCY_THRESHOLDS_US));

	// calculate capacity thresholds. These are at least 1 apart, maximum of CAPACITY_MAX_BUCKET_COUNT buckets
	capacity_increment = req->depth / CAPACITY_MAX_BUCKET_COUNT;
	if (capacity_increment < 1)
		capacity_increment = 1;

	capacity_threshold = capacity_increment;
	while (capacity_threshold < req->depth &&
	       capacity_count < CAPACITY_MAX_BUCKET_COUNT) {
		capacity_thresholds[capacity_count++] = capacity_threshold;
		capacity_threshold += capacity_increment;
	}

	ar_binned_sampler_init(client->debug.capacity_sampler,
			       capacity_thresholds, capacity_count);

	return 0;
}
