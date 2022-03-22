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
	// Do not hold more than one of these locks at a time:
	// hold this while reading from the fifo, in case of multiple consumers
	struct mutex consumer_lock;
	// hold this while writing from the fifo, in case of multiple producers
	struct mutex producer_lock;
	// hold this while accessing context
	struct mutex context_lock;
	struct kfifo_rec_ptr_1 fifo;
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
 * Set context pointer that will be passed to the packet filtering
 * function (if one was set by miscfifo_fop_set_filter_fn) and return the
 * previous value (if there was one).
 *
 * @param  file   file handle
 * @param  context   context
 */
void *miscfifo_fop_xchg_context(struct file *file, void *context);

#endif
