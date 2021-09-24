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
 */
typedef bool (*miscfifo_filter_fn)(const void *context,
				   const u8 *header, size_t header_len,
				   const u8 *payload, size_t payload_len);

struct miscfifo {
	struct {
		/**
		 * If the data being reported has distinct header and payload
		 * sections it's sometimes useful to supply these in different
		 * buffers. Set this property to true and use
		 * #miscfifo_send_header_payload(). The two buffers will be
		 * combined when the event is read
		 *
		 * Otherwise use #miscfifo_send_buf() to send events in
		 * a single buffer
		 **/
		bool header_payload;

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
};

struct miscfifo_client {
	struct miscfifo *mf;
	struct mutex lock;
	struct kfifo_rec_ptr_1 fifo;
	struct list_head node;
	void *context;
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
 * Send buffer to all clients. This function assumes only one writer and
 * caller must serialize if there are multiple writers.
 * Use with config.header_payload = false;
 *
 * @param  mf   miscfifo instance
 * @param  buf  buffer to send
 * @param  len  length of buffer
 * @return      0 on success, > 0 if dropped, -errno otherwise
 */
int miscfifo_send_buf(struct miscfifo *mf, const u8 *buf, size_t len);

/**
 * Send header + payload buffer to all clients. This function assumes only
 * one writer and the caller must serialize if there are multiple writers.
 * Use with config.header_payload = true;
 *
 * @param  mf   miscfifo instance
 * TODO
 * @return      0 on success, > 0 if dropped, -errno otherwise
 */
int miscfifo_send_header_payload(struct miscfifo *mf,
				 const u8 *header, size_t header_len,
				 const u8 *payload, size_t payload_len);

/* call from file_operations->open() */
int miscfifo_fop_open(struct file *, struct miscfifo *mf);

/* assign functions below to file_operations */
ssize_t miscfifo_fop_read(struct file *file,
	char __user *buf, size_t len, loff_t *off);
unsigned int miscfifo_fop_poll(struct file *file,
	struct poll_table_struct *pt);
int miscfifo_fop_release(struct inode *inode, struct file *file);

/**
 * Set context pointer that will be passed to the packet filtering
 * function (if one was set by miscfifo_fop_set_filter_fn)
 *
 * @param  file   file handle
 * @param  context   context
 */
void miscfifo_fop_set_context(struct file *file, void *context);

/**
 * Get context pointer that was previously set with
 * miscfifo_fop_setcontext
 *
 * @param  file   file handle
 * @return   context pointer
 */
void *miscfifo_fop_get_context(struct file *file);

#endif
