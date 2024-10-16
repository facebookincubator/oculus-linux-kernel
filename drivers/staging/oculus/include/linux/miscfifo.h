#ifndef _LINUX_MISCFIFO_H
#define _LINUX_MISCFIFO_H

#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/rwsem.h>

/* Function type that can be used with the config filter_fn to
 * implement per-client packet filtering.
 *
 * This function is expected to be fast/lockless, or it could cause
 * the send() function to block
 */
typedef bool (*miscfifo_filter_fn)(const void *context,
				   const u8 *header, size_t header_len,
				   const u8 *payload, size_t payload_len);

struct miscfifo {
	struct {
		size_t kfifo_size;

		/* This filter function can be used to determine which
		 * packets get sent to a given client.
		 */
		miscfifo_filter_fn filter_fn;
	} config;

	/* state below */
	struct {
		wait_queue_head_t wait;
		struct list_head list;
		struct rw_semaphore rw_lock;
	} clients;

	/* device that registered this miscfifo */
	struct device *dev;
};

struct miscfifo_client {
	struct miscfifo *mf;
	// If you must hold both the consumer and producter locks, the consumer lock
	// must be acquired first and released last!
	struct mutex consumer_lock; // hold while reading from the fifo, in case of multiple consumers
	struct mutex producer_lock; // hold while writing to the fifo, in case of multiple producers
	struct mutex context_lock;  // hold while accessing 'context'
	struct kfifo_rec_ptr_2 fifo;
	bool logged_fifo_full;
	struct list_head node;
	void *context;
	struct file *file;
	char *name;
};

/**
 * devm_miscfifo_register() - resource managed miscfifo registration
 * @dev: device that is registering the miscfifo
 * @mf: pointer to a miscfifo with the config portion filled out
 *
 * Return: 0 on success, -errno otherwise
 */
int devm_miscfifo_register(struct device *dev, struct miscfifo *mf);

/**
 * devm_miscfifo_unregister() - resource managed miscfifo unregistration
 * @dev: device that registered the miscfifo
 * @mf: miscfifo instance to unregisfter
 *
 * Normally this function will not need to be called and the resource
 * managementcode will ensure that the resource is freed.
 */
void devm_miscfifo_unregister(struct device *dev, struct miscfifo *mf);

/**
 * Write a buffer to the FIFO for all clients.
 *
 * Use with config.header_payload = false;
 *
 * @param  mf           miscfifo instance
 * @param  buf          buffer to send
 * @param  len          length of buffer
 * @param  should_wake  set to true if client wakeup is necessary
 * @return              0 on success, > 0 if dropped, -errno otherwise
 */
int miscfifo_write_buf(struct miscfifo *mf, const u8 *buf, size_t len, bool *should_wake);

/**
 * Send a buffer to all clients.
 *
 * Use with config.header_payload = false;
 *
 * @param  mf   miscfifo instance
 * @param  buf  buffer to send
 * @param  len  length of buffer
 * @return      0 on success, > 0 if dropped, -errno otherwise
 */
int miscfifo_send_buf(struct miscfifo *mf, const u8 *buf, size_t len);

/**
 * Wake any clients that may be waiting on this FIFO.
 *
 * @param  mf   miscfifo instance
 */
void miscfifo_wake_waiters(struct miscfifo *mf);

/**
 * Wake any clients that may be waiting on this FIFO,
 * but don't trigger a reschedule.
 *
 * @param  mf   miscfifo instance
 */
void miscfifo_wake_waiters_sync(struct miscfifo *mf);

/**
 * Clear any unread data from the fifo.
 * Use care when calling this. This call will block all readers and writers,
 * and may result in poor performance if called at times when readers or
 * writers are active.
 *
 * @param  mf   miscfifo instance
 */
void miscfifo_clear(struct miscfifo *mf);

/**
 * Call this function from the open() file_operation for the chardev hosting
 * this interface.
 *
 * miscfifo will set the the file's private data with context specific
 * to this client
 *
 * @param file         file pointer being opened
 * @param  mf          miscfifo instance
 * @return             0 on success or -errno
 */
int miscfifo_fop_open(struct file *, struct miscfifo *mf);

/**
 * These miscfifo functions below implement the file_operations interface. They
 * can be passed to vfs verbatim to handle these functions below.
 *
 * static const struct file_operations sample_fops = {
 *     .owner = THIS_MODULE,
 *     .open = sample_fop_open,
 *     .release = miscfifo_fop_release,
 *     .read = miscfifo_fop_read,
 *     .poll = miscfifo_fop_poll,
 * };
 *
 * int sample_fop_open(struct inode *inode, struct file *file)
 * {
 *     struct miscfifo *mf = ...;
 *
 *     return miscfifo_fop_open(file, mf);
 * }
 */
ssize_t miscfifo_fop_read(struct file *file,
	char __user *buf, size_t len, loff_t *off);
unsigned int miscfifo_fop_poll(struct file *file,
	struct poll_table_struct *pt);
int miscfifo_fop_release(struct inode *inode, struct file *file);

/**
 * Allow to extract as many entries from the fifo as available that completely
 * fit into the provided buffer.
 *
 * @param file file handle
 * @param buf User buffer into which data will be copied.
 * @param len Length of user buffer.
 * @param off Unused.
 */
ssize_t miscfifo_fop_read_many(struct file *file,
	char __user *buf, size_t len, loff_t *off);

/**
 * Set context pointer that will be passed to the packet filtering
 * function (if one was set by miscfifo_fop_set_filter_fn) and return the
 * previous value (if there was one).
 *
 * @param  file   file handle
 * @param  context   context
 */
void *miscfifo_fop_xchg_context(struct file *file, void *context);

#endif
