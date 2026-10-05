/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file arfw_int.h
 *
 * @brief Internal header for private data structures.
 *
 * @details
 *
 *******************************************************************************/

#ifndef ARFW_INT_H
#define ARFW_INT_H

#include <linux/arfw_io_interface.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/wait.h>

#include <ar_binned_sampler.h>
#include <arfw_ops.h>
#include <arfw_config.h>
#include <arfw_mem_util.h>
#include <ar_fw_queue.h>
#include <lifetime_lock.h>

#define MIN_LATENCY_LOG_DELAY 30 // seconds

// thresholds in microseconds
static const int LATENCY_THRESHOLDS_US[] = { 250,    500,    1000,   2500,
					     5000,   10000,  20000,  40000,
					     80000,  160000, 320000, 640000,
					     1280000 };
#define CAPACITY_MAX_BUCKET_COUNT 10

/// Holds debug info of a queue
struct arfw_queue_debug {
	int pid;
	char comm[TASK_COMM_LEN];
	ktime_t creation_ts;
	ar_atomic64_t msg_count;
	ar_atomic64_t inline_count;
	ar_atomic64_t external_count;
	ar_atomic64_t consumed_count;
	ktime_t msg_activity_last_ts;
	ar_atomic64_t drop_count;
	ar_atomic64_t pend_count;
	ar_atomic64_t pend_error_count;
	ar_atomic64_t pend_active_count;
	ar_atomic64_t pend_req_count;
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
};

/// AR firmware client queue. This is the handle used in the driver API.
struct arfw_client_queue {
	struct lifetime_lock lt_lock;
	struct arfw_int_client_queue *queue;
	struct arfw_queue_debug debug;
	struct list_head list;
};

/// AR fd context, corresponds to an open file descriptor.
struct arfw_fd_context {
	/// Driver ops, inherited from arfw_char_device
	const struct arfw_driver_ops *driver_ops;

	/// hw specific base context, inherited from the main arfw_char_device
	void *base_context;

	/// struct device for hw ops
	struct device *dev;

	// cached device information
	struct {
		struct mutex lock;
		struct arfw_device_information_req info;
		bool is_cached;
	} device_info_locked;

	/// the arfw client associated with this fd
	struct arfw_client_queue client;
};

/// Events that can happen to a arfw client queue
struct arfw_client_event {
	struct list_head list_node;
	enum ar_queue_event_type type;
	union {
		uint16_t memory_id;
		uint32_t pend_size;
		void *payload_context;
		enum ar_queue_shutdown_reason shutdown_reason;
	};
};

struct arfw_client_event_batch {
	struct arfw_client_event events[AR_QUEUE_EVENT_BATCH_MAX];
	uint16_t size;
};

#define ARFW_REGION_ID_MIN 1
#define ARFW_REGION_ID_MAX UINT16_MAX

#define ARFW_PEND_ID_MIN 1
#define ARFW_PEND_ID_MAX UINT16_MAX

enum arfw_client_region_location {
	EXTERNAL,
	APERTURE,
};

enum arfw_client_region_type {
	ARFW_CLIENT_REGION_DIRECT,
	ARFW_CLIENT_REGION_DMABUF,
};

/// Describes an ar_mem_segment mapped directly into kernel virtual memory.
struct arfw_region_direct_mapping {
	struct page **pages;
	int num_pages;
	void *base_ptr;
	struct sg_table sgt;
};

/// Describes an attached ar_mem_segment represented by dmabuf.
struct arfw_region_dmabuf_mapping {
	struct dma_buf *buf;
	struct dma_buf_attachment *att;
	struct sg_table *sgt;
};

struct arfw_client_region_mapping {
	enum arfw_client_region_type type;
	union {
		struct arfw_region_direct_mapping direct;
		struct arfw_region_dmabuf_mapping dmabuf;
	};
	size_t region_size;
	dma_addr_t dma_region_addr;
	int dma_region_offset;
	size_t dma_region_size;
	enum dma_data_direction dma_direction;
	struct device *hw_dev;
};

/// A registered client memory region
struct arfw_client_region {
	uint16_t id;
	struct arfw_client_region_mapping mapping;
	enum arfw_client_region_location location;
	/// Marks the region to be deleted as soon as it becomes inactive
	bool released;
	/// Reference count for activity (inflight dma) on this region
	uint32_t references;
};

/// Internal data associated with a created client queue.
struct arfw_int_client_queue {
	// internal queue id, used for debug and tracking
	int id;

	/// The hlos id of the queue for firmware IPC
	ar_endpoint_id_t hlos_endpoint_id;

	/// The fw id of the queue for firmware IPC
	ar_endpoint_id_t fw_endpoint_id;

	// the queue direction
	enum ar_queue_direction direction;

	// if this queue is a mirror queue
	bool mirror;

	/// Where the queue resides.
	struct arfw_client_region_mapping queue_mapping;

	/// Meta data for the queue
	ar_fw_queue_meta_t queue_meta;

	/// device information for the queue
	struct arfw_device_information_req queue_device_info;

	/// The data queue
	ar_fw_queue_t arfw_queue;

	/// Size of the shared metadata.
	size_t metadata_size;

	/// VA where the shared metadata is mapped into this process.
	uintptr_t metadata_va;

	/// Driver ops
	const struct arfw_driver_ops *driver_ops;

	/// Driver context, for this queue
	void *queue_context;

	/// Base driver context, from device register
	void *base_context;

	/// Queue data memory region
	struct arfw_queue_mem_region *queue_data;

	/// parent device object.
	struct device *dev;
	struct device *hw_dev;

	/// Ensures operations on the client_queue are serialized. Take only after acquiring lifetime lock.
	struct mutex serial_work_lock;

	/// events on this client queue
	struct {
		spinlock_t lock;
		// a list of arfw_client_event events
		struct list_head events;
		// if there is a read event in the list of events
		bool has_read_event;
	} locked_events;

	/// wait queue for events
	wait_queue_head_t event_wq;

	// memory regions for this client queue
	struct {
		spinlock_t lock;
		struct idr region_idr;
		unsigned int num_regions;
		size_t total_size;
	} locked_regions;

	/// additional context to explain driver originated shutdowns
	enum ar_queue_shutdown_reason shutdown_reason;
};

/// AR firmware character device object
struct arfw_char_device {
	struct cdev cdev;
	struct device *dev;
	struct device *hw_dev;
	// human readable device id string
	char *id;
	// driver operations
	const struct arfw_driver_ops *driver_ops;
	// hw specific context for base arfw device ops (not on a specific queue)
	void *base_context;
	// used for list of registered ar device drivers
	struct list_head list;
	// used to count open fds for each device and defer release
	uint32_t references;
	// whether the device has been unregistered
	bool released;
	// used to sync the device deletion
	struct kobject kobj;
};

// helper to dump a ar_queue to kernel logs
void dbg_dump_queue_info(struct arfw_client_queue *client, const char *action);

/**
 * A context for an aperture buffer file descriptor.
 */
struct arfw_aperture_fd_context {
	/// client queue associated with this aperture buffer
	struct arfw_client_queue *client;
	/// Driver ops, in case queue is closed
	const struct arfw_driver_ops *driver_ops;
	/// hw specific base context, in case queue is closed
	void *base_context;
	/// region id associated with this aperture buffer
	uint16_t region_id;
};

#endif // !ARFW_INT_H
