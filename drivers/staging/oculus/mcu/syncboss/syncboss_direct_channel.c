// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-direction.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/of_platform.h>
#include <linux/syncboss/consumer.h>
#include <linux/time64.h>

#include <uapi/linux/syncboss.h>

#include "syncboss_consumer_priorities.h"
#include "syncboss_direct_channel.h"
#include "syncboss_direct_channel_mcu_defs.h"

#define SYNCBOSS_DIRECTCHANNEL_DEVICE_NAME "syncboss_directchannel0"

static struct kmem_cache *syncboss_dma_fence_cache;

#define DEBUGFS_ROOT_DIR_NAME "syncboss_direct_channel"
static atomic_t syncboss_directchannel_active_rd_fences;
static atomic_t syncboss_directchannel_active_wr_fences;
static atomic_t syncboss_directchannel_allocd_fences;


static int syncboss_directchannel_open(struct inode *inode, struct file *f)
{
	struct direct_channel_dev_data *devdata = container_of(
		f->private_data, struct direct_channel_dev_data, misc_directchannel);

	dev_dbg(devdata->dev, "direct channel opened by %s (%d)",
		 current->comm, current->pid);

	return 0;
}

static int syncboss_directchannel_release(struct inode *inode, struct file *f)
{
	int status = 0;
	struct direct_channel_dev_data *devdata = container_of(
		f->private_data, struct direct_channel_dev_data, misc_directchannel);
	struct device *dev = devdata->dev;
	struct channel_client_entry *client_data, *tmp_list_node;
	struct channel_dma_buf_info *c_dma_buf_info;
	struct dma_buf *c_dma_buf;
	int i;

	dev_dbg(dev, "releasing direct channel file : %p)", f);

	down_write(
		&devdata->direct_channel_rw_lock); /* rare event, could add jitter to interrupt thread; if issue add locks per sensor type. */
	for (i = SPI_PACKET_MIN_PACKET_TYPE_SUPPORTED; i <= SPI_PACKET_MAX_PACKET_TYPE_SUPPORTED; i++) {
		struct list_head *client_list = devdata->direct_channel_data[i];

		if (!client_list || list_empty(client_list))
			continue;

		dev_dbg(
			dev,
			"releasing direct channel for sensor type Client - %d called by %s (%d)",
			i, current->comm, current->pid);
		list_for_each_entry_safe(client_data, tmp_list_node, client_list, list_entry) {
			if (client_data->file != f)
				continue;

			if (client_data->active_fence) {
				dma_fence_signal(&client_data->active_fence->fence);
				dma_fence_put(&client_data->active_fence->fence);
				atomic_dec(&syncboss_directchannel_active_rd_fences);
				client_data->active_fence = NULL;
			}
			client_data->channel_data.channel_current_ptr =  NULL;
			c_dma_buf_info = &client_data->dma_buf_info;
			c_dma_buf = c_dma_buf_info->dma_buf;
			dma_resv_lock(c_dma_buf->resv, NULL);
			dma_resv_add_excl_fence(c_dma_buf->resv, NULL);
			dma_buf_vunmap(c_dma_buf, c_dma_buf_info->k_virtptr);
			dma_resv_unlock(c_dma_buf->resv);
			dma_buf_put(c_dma_buf); /* decrement ref count */
			list_del(&client_data->list_entry);
			devm_kfree(dev, client_data);

			if (list_empty(client_list)) {
				devdata->direct_channel_data[i] = NULL;
				devm_kfree(dev, client_list);
			}
			break;
		}
	}
	up_write(&devdata->direct_channel_rw_lock);

	return status;
}

/* syncboss direct channel dma_fence support
 *
 * The syncboss direct channel driver uses dma_fence objects to enable
 * synchronization with userspace.  The userspace code accesses the
 * direct channel shared memory via memory allocated by a
 * device-specific graphics memory driver that is owned by the Linux
 * dma-buf driver.
 *
 * Note that dma_fence objects start out in the unsignalled state, and
 * can make a one-time transition to the signalled state.  There is no
 * transitioning back to unsignalled: the only way to get an
 * unsignalled fence is to allocate a new one.  Also note that for
 * efficiency the dma_buf subsystem makes use of RCU for managing
 * access to dma_fence objects.  To use the efficiently, this driver
 * uses a kmem_cache of dma_fence objects and must free them only at
 * RCU boundaries.
 *
 * This driver uses dma_fence objects a little differently than what
 * they are normally used for.  The normal use case for them is to
 * handle synchronizing between external hardware that is accessing
 * dma_buf memory, such as a GPU.  The typical implementation is that
 * when a GPU operation is initiated on a DMA buffer, a dma_fence is
 * attached to the dma_buf.  This fence is initially in the
 * unsignalled state, and when the GPU completes its operation the
 * fence is signalled.
 *
 * An "exclusive" fence is used when the GPU is writing to the
 * dma_buf, and a "shared" fence is used when one (or more) GPU
 * operations are reading from a dma_buf.
 *
 * In this driver, we attach an exclusive fence to the dma_buf during
 * channel configuration.  When the direct channel driver writes new
 * data into the dma_buf, it signals the exclusive fence and attaches
 * a shared fence to the dma_buf.  When the user space code waits on
 * the shared fence, the direct channel driver will signal it
 * immediately and replace any exclusive fence with a new untriggered
 * exclusive fence.  If the user space code waits on an exclusive
 * fence, after the fence is triggered and wakes up the waiting code
 * it will allocate a fresh exclusive fence and attach it to the
 * dma_buf.
 *
 * All of this leads to two models for applications to synchronize
 * with new data from the syncboss direct channel driver:
 *
 * Method 1: poll-like system call to wait, ioctl() to rearm
 * --------
 *
 * 1. Wait for read ready on dma_buf fd with poll()-like syscall
 * 2. Re-arm the read side with an ioctl() call (see below) before
 *    next poll()
 *
 * User space code can use an of the poll() family of system calls
 * (including poll(), ppoll(), select(), and epoll()) to wait for read
 * data from the dma_buf fd (combined with any other fd's of interest
 * it is waiting on).  The dma_buf fd will be marked ready for read
 * when data is pushed into the direct channel buffer.
 *
 * The fd will stay ready until the userspace code calls:
 *
 *   struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE };
 *   ioctl(dma_buf_fd, DMA_BUF_IOCTL_SYNC, &sync);
 *
 * This ends up calling syncboss_fence_wait() on the shared fence,
 * which will rearm the read side to be not-ready until more data is
 * written into the direct channel buffer.
 *
 * Method 2: single ioctl() call
 * --------
 *
 * If the thread reading from the direct channel buffer is not waiting
 * on data from any other entity, it can synchronize with one system
 * call.  That would be:
 *
 *   struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ };
 *   ioctl(dma_buf_fd, DMA_BUF_IOCTL_SYNC, &sync);
 *
 * This will end up in syncboss_fence_wait() below waiting on the
 * exclusive dma_fence object.  Once that object is triggered by the
 * spi thread distributing data to it, the second half of
 * syncboss_fence_wait() will re-arm the read side by replace the
 * exclusive dma_fence object with a new one.
 *
 * The upside to method 2 is that there is only one syscall per data
 * item from the direct channel buffer.  The downside is that there is
 * no opportunity for multiplexing to handle waiting on multiple fd's
 * as there is with method 1.
 */

static const char *syncboss_fence_get_driver_name(struct dma_fence *fence);
static const char *syncboss_fence_get_timeline_name(struct dma_fence *fence);
static bool syncboss_fence_enable_signaling(struct dma_fence *fence);
static signed long syncboss_fence_wait(struct dma_fence *fence,
						     bool intr, signed long timeout);
static void syncboss_fence_release(struct dma_fence *fence);

static struct dma_fence_ops syncboss_fence_ops = {
	.get_driver_name = syncboss_fence_get_driver_name,
	.get_timeline_name = syncboss_fence_get_timeline_name,
	.enable_signaling = syncboss_fence_enable_signaling,
	.wait = syncboss_fence_wait,
	.release = syncboss_fence_release,
};

static const char *syncboss_fence_get_driver_name(struct dma_fence *fence)
{
	return "syncboss_directchannel";
}

static const char *syncboss_fence_get_timeline_name(struct dma_fence *fence)
{
	return "unbound";
}

static void syncboss_fence_free_rcu(struct rcu_head *head)
{
	struct dma_fence *fence = container_of(head, struct dma_fence, rcu);
	struct syncboss_dma_fence *sb_fence = container_of(fence, struct syncboss_dma_fence, fence);

	kmem_cache_free(syncboss_dma_fence_cache, sb_fence);
	atomic_dec(&syncboss_directchannel_allocd_fences);
}

static void syncboss_directchannel_prepare_fence(struct channel_client_entry *client_data, bool read)
{
	struct device *dev = client_data->devdata->dev;
	struct syncboss_dma_fence *new_fence;
	struct dma_buf *dma_buffer;

	new_fence = kmem_cache_alloc(syncboss_dma_fence_cache, GFP_KERNEL);
	atomic_inc(&syncboss_directchannel_allocd_fences);
	dma_fence_init(&new_fence->fence, &syncboss_fence_ops, &client_data->fence_lock, 0, 0);
	new_fence->client = client_data;
	new_fence->read = read;

	if (read) {
		atomic_inc(&syncboss_directchannel_active_rd_fences);
	} else {
		atomic_inc(&syncboss_directchannel_active_wr_fences);
	}

	dma_buffer = client_data->dma_buf_info.dma_buf;
	dma_resv_lock(dma_buffer->resv, NULL);
	if (read) {
		// Readers wait for DMA writers, which go in the exclusive slot
		dma_resv_add_excl_fence(dma_buffer->resv, &new_fence->fence);
	} else {
		// Writers wait for DMA readers, which go in the shared slot
		int err;

		err = dma_resv_reserve_shared(dma_buffer->resv, 1);
		if (err) {
			// This means we will not be able to rearm the
			// read side, and it will continue to be
			// marked as available to read.
			dev_err(dev,
				"%s failed to reserve dma buf shared reservation fence space",
				__func__);
		} else {
			dma_resv_add_shared_fence(dma_buffer->resv, &new_fence->fence);
		}
	}
	dma_resv_unlock(dma_buffer->resv);

	// Track the new fence as active so we signal it when
	// we get the next packet
	if (read) {
		unsigned long flags;

		spin_lock_irqsave(&client_data->fence_lock, flags);
		if (client_data->active_fence) {
			dma_fence_put(&client_data->active_fence->fence);
		}
		client_data->active_fence = new_fence;
		spin_unlock_irqrestore(&client_data->fence_lock, flags);
	} else {
		dma_fence_put(&new_fence->fence);
	}
}

static bool syncboss_fence_enable_signaling(struct dma_fence *fence)
{
	// NOTE: This is called in atomic context (irqs disabled, fence spinlock held)
	struct syncboss_dma_fence *sb_fence =
		container_of(fence, struct syncboss_dma_fence, fence);

	if (sb_fence->read) {
		// Arm the wakeup of this fence.
		atomic_set(&sb_fence->client->reader_waiting, 1);
	}

	return true;
}

static signed long syncboss_fence_wait(struct dma_fence *fence,
				       bool intr, signed long timeout)
{
	struct syncboss_dma_fence *sb_fence =
		container_of(fence, struct syncboss_dma_fence, fence);
	signed long wait_result;

	/* See the comment above that starts with "syncboss direct
	 * channel dma_fence support" for an explanation of the logic
	 * in this method.
	 */

	if (sb_fence->read) {
		// Arm the wakeup of this fence.
		atomic_set(&sb_fence->client->reader_waiting, 1);
	} else {
		dma_fence_signal(fence);
		atomic_dec(&syncboss_directchannel_active_wr_fences);
	}

	wait_result = dma_fence_default_wait(fence, intr, timeout);

	syncboss_directchannel_prepare_fence(sb_fence->client, true);

	return wait_result;
}

static void syncboss_fence_release(struct dma_fence *fence)
{
	if (!dma_fence_is_signaled(fence)) {
		struct syncboss_dma_fence *sb_fence =
			container_of(fence, struct syncboss_dma_fence, fence);

		if (sb_fence->read) {
			atomic_dec(&syncboss_directchannel_active_rd_fences);
		} else {
			atomic_dec(&syncboss_directchannel_active_wr_fences);
		}
	}
	call_rcu(&fence->rcu, syncboss_fence_free_rcu);
}

/* Update the direct channel buffer with IMU data. */
static int direct_channel_distribute_spi_payload(struct direct_channel_dev_data *devdata,
				   struct channel_client_entry *client_data,
				   const struct rx_packet_info *packet_info)
{
	int status = 0;
	struct device *dev = devdata->dev;
	const struct syncboss_data *packet = packet_info->data;
	const struct syncboss_driver_data_header_t *header = &packet_info->header;
	struct syncboss_sensor_direct_channel_data *current_ptr;
	struct direct_channel_data *channel_data;

	channel_data = &client_data->channel_data;
	current_ptr = (struct syncboss_sensor_direct_channel_data *)
			      channel_data->channel_current_ptr;
	if (current_ptr == NULL) {
		dev_err(dev,
			"%s syncboss dropping data due to null current pointer %d",
			__func__, packet->data_len);
		return -EIO;
	}

	if (header->header_length + packet->data_len > sizeof(union syncboss_direct_channel_payload)) {
		dev_err_ratelimited(dev, "%s syncboss data size to large %d",
				    __func__, packet->data_len);
		return -EIO;
	}

	if (header->nsync_offset_status != SYNCBOSS_TIME_OFFSET_VALID) {
		dev_err_ratelimited(dev, "direct channel syncboss data has invalid time offset - dropping");
		return status;
	}

	/* Note that we do not call dma_buf_begin_cpu_access() /
	 * dma_buf_end_cpu_access() around accessing the buffer.  As
	 * we are just using the buffer as shared memory between code
	 * all running on the CPU it is not needed (it is written by
	 * the CPU here in this driver, and read on the CPU by the
	 * user mode code consuming the sensor data).  This ensures
	 * that we can use DMA fences to let the user space code wait
	 * for data to be available in the direct channel buffer.
	 */

	/* size/report_token and sensor_type are fixed values and added at init time */
	current_ptr->timestamp = ktime_get_ns();
	memcpy(current_ptr->payload.data, header, header->header_length);
	memcpy(current_ptr->payload.data + header->header_length, packet->data, packet->data_len);

	channel_data->counter++;
	if (channel_data->counter == 0)
		channel_data->counter = 1;

	/*
	 *  wmb here to ensure prior writes are complete before we update the counter.
	 *  counter is used by the client code to determine when to read new data.
	 */
	wmb();
	current_ptr->counter = channel_data->counter;

	// Signal any active DMA fence to wake up waiters if epoll
	// support enabled and there are any active waiters.
	if ((client_data->wake_epoll) &&
	    (atomic_read(&client_data->reader_waiting) != 0) &&
	    (client_data->active_fence != NULL)) {
		struct syncboss_dma_fence *active_fence = client_data->active_fence;
		unsigned long flags;

		// Disarm so we don't wake up again until requested
		atomic_set(&client_data->reader_waiting, 0);

		// Remove this fence from active before we wake up waiters
		spin_lock_irqsave(&client_data->fence_lock, flags);
		client_data->active_fence = NULL;
		spin_unlock_irqrestore(&client_data->fence_lock, flags);

		// Signal this read-side fence
		dma_fence_signal(&active_fence->fence);
		dma_fence_put(&active_fence->fence);
		atomic_dec(&syncboss_directchannel_active_rd_fences);

		// Prepare a write-side fence that will be used to re-arm the read side
		syncboss_directchannel_prepare_fence(client_data, false);
	}

	channel_data->channel_current_ptr++;
	if (channel_data->channel_current_ptr >= channel_data->channel_end_ptr)
		channel_data->channel_current_ptr = channel_data->channel_start_ptr;

	return status;
}

static int init_client_entry_instance(struct channel_client_entry *channel_client_entry_list,
			struct dma_buf *dma_buffer,
			int num_channel_entries,
			struct syncboss_driver_directchannel_shared_memory_config *new_config,
			struct file *file,
			struct direct_channel_dev_data *devdata)
{
	int status = 0;
	void *k_virt_addr;

	dma_resv_lock(dma_buffer->resv, NULL);
	k_virt_addr = dma_buf_vmap(dma_buffer);
	if (IS_ERR_OR_NULL(k_virt_addr)) {
		dev_err(
			devdata->dev,
			"%s :dma_buf_vmap failed %p",
			__func__,
			k_virt_addr);
		dma_resv_unlock(dma_buffer->resv);
		status = -EFAULT;
		goto err;
	}
	dma_resv_unlock(dma_buffer->resv);
	/*
	 * Save a copy of the mapped virtual address to avoid mapping/remapping
	 */
	channel_client_entry_list->dma_buf_info.k_virtptr = k_virt_addr;
	channel_client_entry_list->dma_buf_info.buffer_size = dma_buffer->size;
	channel_client_entry_list->dma_buf_info.dma_buf = dma_buffer;
	channel_client_entry_list->channel_data.channel_start_ptr = k_virt_addr;
	channel_client_entry_list->channel_data.channel_current_ptr = k_virt_addr;
	channel_client_entry_list->channel_data.channel_end_ptr =
		k_virt_addr + (num_channel_entries * sizeof(struct syncboss_sensor_direct_channel_data));
	channel_client_entry_list->channel_data.counter = 0;
	channel_client_entry_list->devdata = devdata;
	channel_client_entry_list->file = file;
	channel_client_entry_list->wake_epoll = new_config->wake_epoll;
	spin_lock_init(&channel_client_entry_list->fence_lock);
	channel_client_entry_list->active_fence = NULL;
	atomic_set(&channel_client_entry_list->reader_waiting, 0);

	if (new_config->uapi_pkt_type == SPI_PACKET_TYPE_IMU_DATA) {
		channel_client_entry_list->direct_channel_distribute =
			direct_channel_distribute_spi_payload;
	}

err:
	return status;
}

/*
 * Setup from IOCTL call.
 */
static int syncboss_set_directchannel_sm(
	struct direct_channel_dev_data *devdata, struct file *file,
	const struct syncboss_driver_directchannel_shared_memory_config __user
		*config)
{
	int status = 0;
	struct device *dev = devdata->dev;
	struct syncboss_driver_directchannel_shared_memory_config *new_config = NULL;
	struct dma_buf *dma_buffer;
	uint32_t num_channel_entries;
	struct channel_client_entry *channel_client_entry_list = NULL;
	struct list_head *channel_list = NULL;
	struct syncboss_sensor_direct_channel_data *dc_ptr;

	new_config = kzalloc(sizeof(*new_config), GFP_KERNEL);
	if (!new_config)
		return -ENOMEM;

	status = copy_from_user(new_config, config, sizeof(*new_config));
	if (status != 0) {
		dev_err(dev,
			"Syncboss direct channel ioctl:failed to copy %d bytes from user stream shared memory config",
			status);
		status = -EFAULT;
		goto err;
	}

	dev_dbg(
		dev,
		"Syncboss direct channel ioctl:FileDesc: %d direct channel buffer size:%lu Packet Type:%d Wake epoll:%d",
		new_config->dmabuf_fd,
		new_config->direct_channel_buffer_size,
		new_config->uapi_pkt_type,
		new_config->wake_epoll);

	dma_buffer = dma_buf_get(new_config->dmabuf_fd);
	if (PTR_ERR_OR_ZERO(dma_buffer)) {
		status = PTR_ERR(dma_buffer);
		dev_err(
			dev,
			"Syncboss direct channel ioctl: FD is not a dmabuf error:%d",
			status);
		goto err;
	}

	num_channel_entries =
			new_config->direct_channel_buffer_size /
			sizeof(struct syncboss_sensor_direct_channel_data);

	dev_dbg(
			dev,
			"Syncboss direct channel ioctl:dma_buffer size:%lu direct channel buffer size:%lu num_channel_entries:%d entry size:%lu",
			dma_buffer->size,
			new_config->direct_channel_buffer_size,
			num_channel_entries,
			sizeof(struct syncboss_sensor_direct_channel_data));

	if (new_config->direct_channel_buffer_size > dma_buffer->size)	{
		dev_err(
			dev,
			"Syncboss direct channel ioctl: DMA buffer smaller than direct channel buffer size");
		status = -EINVAL;
		goto err;
	}

	if (num_channel_entries == 0) {
		dev_err(
			dev,
			"Syncboss direct channel ioctl: channel entries = 0");

		status = -EINVAL;
		goto err;
	}

	if (new_config->uapi_pkt_type < SPI_PACKET_MIN_PACKET_TYPE_SUPPORTED ||
		new_config->uapi_pkt_type > SPI_PACKET_MAX_PACKET_TYPE_SUPPORTED) {
		dev_err(dev,
			"Syncboss direct channel ioctl unsupported packet type %d",
			new_config->uapi_pkt_type);
		status = -EINVAL;
		goto err;
	}

	/*
	 * Set up memory for list of clients, if not already allocated.
	 */
	if (!devdata->direct_channel_data[new_config->uapi_pkt_type]) {
		channel_list =
			devm_kzalloc(dev, sizeof(*channel_list), GFP_KERNEL);
		if (!channel_list) {
			status = -ENOMEM;
			goto err;
		}
		devdata->direct_channel_data[new_config->uapi_pkt_type] =
			channel_list;
		INIT_LIST_HEAD(
			devdata->direct_channel_data[new_config->uapi_pkt_type]);
	}

	channel_client_entry_list =
		devm_kzalloc(dev, sizeof(*channel_client_entry_list), GFP_KERNEL);
	if (!channel_client_entry_list) {
		status = -ENOMEM;
		goto err;
	}
	INIT_LIST_HEAD(&(channel_client_entry_list->list_entry));

	dev->dma_mask = &devdata->dma_mask;
	status = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (status != 0) {
		dev_err(dev, "dma_set_mask_and_coherent failed rc=%d",
			status);
		status = -EFAULT;
		goto err;
	}

	if (init_client_entry_instance(channel_client_entry_list,
								dma_buffer,
								num_channel_entries,
								new_config,
								file,
								devdata) != 0)
		goto err;

	/*
	 * Initialize the dma buf channel data structure for elements that dont change per update.
	 */
	memset(channel_client_entry_list->dma_buf_info.k_virtptr, 0, num_channel_entries * sizeof(struct syncboss_sensor_direct_channel_data));
	dc_ptr = (struct syncboss_sensor_direct_channel_data *)channel_client_entry_list->dma_buf_info.k_virtptr;

	do {
		dc_ptr->size = sizeof(struct syncboss_sensor_direct_channel_data);
		dc_ptr->report_token = 0;
		dc_ptr->sensor_type = new_config->uapi_pkt_type;
		dc_ptr += 1;
	} while (dc_ptr < (struct syncboss_sensor_direct_channel_data *)channel_client_entry_list->channel_data.channel_end_ptr);

	down_write(&devdata->direct_channel_rw_lock);
	list_add(&channel_client_entry_list->list_entry,
		 devdata->direct_channel_data[new_config->uapi_pkt_type]);
	up_write(&devdata->direct_channel_rw_lock);

	if (new_config->wake_epoll) {
		// Set up the initial read-side dma fence
		syncboss_directchannel_prepare_fence(channel_client_entry_list, true);
	}

	kfree(new_config);
	return 0;

err:
	kfree(new_config);
	devm_kfree(dev, channel_client_entry_list);
	devm_kfree(dev, channel_list);
	if (devdata->direct_channel_data[new_config->uapi_pkt_type] == channel_list) /* first time through, remove the empty list pointer.*/
		devdata->direct_channel_data[new_config->uapi_pkt_type] = NULL;
	return status;
}

static int syncboss_clear_directchannel_sm(
	struct direct_channel_dev_data *devdata, struct file *file,
	const struct syncboss_driver_directchannel_shared_memory_clear_config __user
		*config)
{
	int status = -EINVAL;
	struct device *dev = devdata->dev;
	struct syncboss_driver_directchannel_shared_memory_clear_config *new_config = NULL;
	struct channel_client_entry *client_data, *tmp_list_node;
	struct list_head *client_list;
	struct channel_dma_buf_info *c_dma_buf_info;
	struct dma_buf *c_dma_buf;
	struct dma_buf *dma_buffer;

	new_config = kzalloc(sizeof(*new_config), GFP_KERNEL);
	if (!new_config)
		return -ENOMEM;

	status = copy_from_user(new_config, config, sizeof(*new_config));
	if (status != 0) {
		dev_err(dev,
			"Syncboss direct channel ioctl:failed to copy %d bytes from user stream shared memory config",
			status);
		status = -EFAULT;
		goto err;
	}

	dev_dbg(
		dev,
		"Syncboss direct channel ioctl:FileDesc: %d Packet Type:%d",
		new_config->dmabuf_fd,
		new_config->uapi_pkt_type);

	if (new_config->uapi_pkt_type < SPI_PACKET_MIN_PACKET_TYPE_SUPPORTED ||
		new_config->uapi_pkt_type > SPI_PACKET_MAX_PACKET_TYPE_SUPPORTED) {
		dev_err(dev,
			"Syncboss direct channel ioctl unsupported packet type %d",
			new_config->uapi_pkt_type);
		status = -EINVAL;
		goto err;
	}

	dma_buffer = dma_buf_get(new_config->dmabuf_fd);
	if (PTR_ERR_OR_ZERO(dma_buffer)) {
		dev_err(
			dev,
			"Syncboss direct channel ioctl: FD is not a dmabuf error:%d",
			status);
		status = PTR_ERR(dma_buffer);
		goto err;
	}

	down_write(
		&devdata->direct_channel_rw_lock); /* rare event, could add jitter to interrupt thread; if issue add locks per sensor type. */

	client_list = devdata->direct_channel_data[new_config->uapi_pkt_type];

	if (!client_list || list_empty(client_list)) {
		dev_err(dev,
			"Syncboss direct channel ioctl no dmabuf entries for packet type %d",
			new_config->uapi_pkt_type);
		status = -EINVAL;
		goto err_up;
	}

	dev_dbg(
		dev,
		"releasing direct channel dmabuf fd %d for sensor type Client - %d called by %s (%d)", new_config->dmabuf_fd,
		new_config->uapi_pkt_type, current->comm, current->pid);

	list_for_each_entry_safe(client_data, tmp_list_node, client_list, list_entry) {
		if (client_data->file != file || client_data->dma_buf_info.dma_buf != dma_buffer)
			continue;

		status = 0;
		if (client_data->active_fence) {
			dma_fence_signal(&client_data->active_fence->fence);
			dma_fence_put(&client_data->active_fence->fence);
			atomic_dec(&syncboss_directchannel_active_rd_fences);
			client_data->active_fence = NULL;
		}
		client_data->channel_data.channel_current_ptr =  NULL;
		c_dma_buf_info = &client_data->dma_buf_info;
		c_dma_buf = c_dma_buf_info->dma_buf;
		dma_resv_lock(c_dma_buf->resv, NULL);
		dma_resv_add_excl_fence(c_dma_buf->resv, NULL);
		dma_buf_vunmap(c_dma_buf, c_dma_buf_info->k_virtptr);
		dma_resv_unlock(c_dma_buf->resv);
		dma_buf_put(c_dma_buf); /* decrement ref count */
		list_del(&client_data->list_entry);
		devm_kfree(dev, client_data);

		if (list_empty(client_list)) {
			devdata->direct_channel_data[new_config->uapi_pkt_type] = NULL;
			devm_kfree(dev, client_list);
		}
		break;
	}

	up_write(&devdata->direct_channel_rw_lock);
	dma_buf_put(dma_buffer); /* decrement ref count */
	kfree(new_config);
	return status;

err_up:
	up_write(&devdata->direct_channel_rw_lock);
err:
	kfree(new_config);
	return status;
}

static long syncboss_directchannel_ioctl(struct file *f, unsigned int cmd,
					 unsigned long arg)
{
	struct direct_channel_dev_data *devdata = container_of(
		f->private_data, struct direct_channel_dev_data, misc_directchannel);

	switch (cmd) {
	case SYNCBOSS_SET_DIRECTCHANNEL_SHARED_MEMORY_IOCTL:
		dev_dbg(devdata->dev,
			 "direct channel ioctl %d from %s (%d)", cmd,
			 current->comm, current->pid);
		return syncboss_set_directchannel_sm(
			devdata, f,
			(struct syncboss_driver_directchannel_shared_memory_config *)arg);
	case SYNCBOSS_CLEAR_DIRECTCHANNEL_SHARED_MEMORY_IOCTL:
		dev_dbg(devdata->dev,
			"direct channel ioctl %d from %s (%d)", cmd,
			current->comm, current->pid);
		return syncboss_clear_directchannel_sm(
			devdata, f,
			(struct syncboss_driver_directchannel_shared_memory_clear_config *)arg);
	default:
		dev_err(devdata->dev,
			"unrecognized direct channel ioctl %d from %s (%d)",
			cmd, current->comm, current->pid);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations directchannel_fops = {
	.open = syncboss_directchannel_open,
	.release = syncboss_directchannel_release,
	.read = NULL,
	.poll = NULL,
	.unlocked_ioctl = syncboss_directchannel_ioctl
};

static int rx_packet_handler(struct notifier_block *nb, unsigned long type, void *pi)
{
	struct direct_channel_dev_data *devdata = container_of(nb, struct direct_channel_dev_data, rx_packet_nb);
	const struct rx_packet_info *packet_info = pi;
	int ret = NOTIFY_DONE;
	struct channel_client_entry *client_data;

	/* Acquire Read Lock for Direct Channel Clients - writer is rare. */
	down_read(&devdata->direct_channel_rw_lock);

	if (devdata->direct_channel_data[type]) {
		list_for_each_entry(client_data, devdata->direct_channel_data[type], list_entry) {
			int status = (*client_data->direct_channel_distribute) (devdata, client_data, packet_info);

			if (status >= 0) {
				/* If we return NOTIFY_STOP the packet will only get delivered to direct channel
				   clients; if we return NOTIFY_DONE it will also get delivered to the sensor service
				   for processing.  We need to let any changes to nsync status go to the sensor service;
				   but otherwise we want to only deliver IMU data to direct clients. */
				const struct syncboss_driver_data_header_t *header = &packet_info->header;
				if ((header->nsync_offset_us != devdata->last_nsync_offset_us) ||
				    (header->nsync_offset_status != devdata->last_nsync_offset_status)) {
					devdata->last_nsync_offset_us = header->nsync_offset_us;
					devdata->last_nsync_offset_status = header->nsync_offset_status;
					ret = NOTIFY_DONE;
				} else {
					ret = NOTIFY_STOP;
				}
			} else {
				dev_dbg(devdata->dev, "direct channel distibute failed error %d", status);
				break;
			}
		}
	}
	up_read(&devdata->direct_channel_rw_lock);

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int syncboss_direct_channel_debugfs_init(struct direct_channel_dev_data *devdata)
{
	struct device *dev = devdata->dev;

	devdata->dentry = debugfs_create_dir(DEBUGFS_ROOT_DIR_NAME, NULL);
	if (IS_ERR_OR_NULL(devdata->dentry)) {
		dev_err(dev, "failed to create debugfs " DEBUGFS_ROOT_DIR_NAME
			" dir: %ld", PTR_ERR(devdata->dentry));
		return PTR_ERR(devdata->dentry);
	}

	debugfs_create_u32("active_rd_fences",
		0444, devdata->dentry,
		&syncboss_directchannel_active_rd_fences.counter);
	debugfs_create_u32("active_wr_fences",
		0444, devdata->dentry,
		&syncboss_directchannel_active_wr_fences.counter);
	debugfs_create_u32("allocd_fences",
		0444, devdata->dentry,
		&syncboss_directchannel_allocd_fences.counter);

	return 0;
}

static void syncboss_direct_channel_debugfs_deinit(struct direct_channel_dev_data *devdata)
{
	if (!IS_ERR_OR_NULL(devdata->dentry)) {
		debugfs_remove_recursive(devdata->dentry);
		devdata->dentry = NULL;
	}
}
#else /* CONFIG_DEBUG_FS */
static int syncboss_direct_channel_debugfs_init(struct direct_channel_dev_data *devdata)
{
	return 0;
}

static void syncboss_direct_channel_debugfs_deinit(struct direct_channel_dev_data *devdata)
{
}
#endif /* CONFIG_DEBUG_FS */

static int syncboss_direct_channel_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct direct_channel_dev_data *devdata = dev_get_drvdata(dev);
	struct device_node *parent_node = of_get_parent(node);
	int ret;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct direct_channel_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);

	devdata->dev = dev;
	devdata->syncboss_ops = dev_get_drvdata(dev->parent);

	devdata->misc_directchannel.name = SYNCBOSS_DIRECTCHANNEL_DEVICE_NAME;
	devdata->misc_directchannel.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_directchannel.fops = &directchannel_fops;

	ret = misc_register(&devdata->misc_directchannel);
	if (ret < 0) {
		dev_err(dev, "failed to register direct channel device, error %d", ret);
		goto out;
	}

	/* Gloal read/write lock to update direct channel clients. */
	init_rwsem(&devdata->direct_channel_rw_lock);

	devdata->rx_packet_nb.notifier_call = rx_packet_handler;
	devdata->rx_packet_nb.priority = SYNCBOSS_PACKET_CONSUMER_PRIORITY_DIRECTCHANNEL;
	ret = devdata->syncboss_ops->rx_packet_notifier_register(dev, &devdata->rx_packet_nb);
	if (ret < 0) {
		dev_err(dev, "failed to register rx packet notifier, error %d", ret);
		goto err_after_misc_reg;
	}

	ret = syncboss_direct_channel_debugfs_init(devdata);
	if (ret < 0) {
		dev_err(dev, "failed to init debugfs, error %d", ret);
		goto err_after_rx_packet_notifier_register;
	}

	dev_dbg(dev, "%s register direct channel misc device", __func__);

	return ret;

err_after_rx_packet_notifier_register:
	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);
err_after_misc_reg:
	misc_deregister(&devdata->misc_directchannel);
out:
	return ret;
}

static int syncboss_direct_channel_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct direct_channel_dev_data *devdata = dev_get_drvdata(dev);

	devdata->syncboss_ops->rx_packet_notifier_unregister(dev, &devdata->rx_packet_nb);

	syncboss_direct_channel_debugfs_deinit(devdata);

	misc_deregister(&devdata->misc_directchannel);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_direct_channel_match_table[] = {
	{ .compatible = "meta,syncboss-direct-channel", },
	{ },
};
#else
#define syncboss_direct_channel_match_table NULL
#endif

struct platform_driver syncboss_direct_channel_driver = {
	.driver = {
		.name = "syncboss_direct_channel",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_direct_channel_match_table
	},
	.probe = syncboss_direct_channel_probe,
	.remove = syncboss_direct_channel_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_direct_channel_driver,
};

static int __init syncboss_direct_channel_init(void)
{
	syncboss_dma_fence_cache = KMEM_CACHE(syncboss_dma_fence, SLAB_PANIC | SLAB_HWCACHE_ALIGN);
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_direct_channel_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
	rcu_barrier_tasks();
	kmem_cache_destroy(syncboss_dma_fence_cache);
	syncboss_dma_fence_cache = NULL;
}

module_init(syncboss_direct_channel_init);
module_exit(syncboss_direct_channel_exit);
MODULE_DESCRIPTION("Syncboss Direct Channel Interface Driver");
MODULE_LICENSE("GPL v2");
