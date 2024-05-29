// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

/*
 * Secure-Processor-Communication (SPCOM).
 *
 * This driver provides communication to Secure Processor (SP)
 * over RPMSG framework.
 *
 * It provides interface to userspace spcomlib.
 *
 * Userspace application shall use spcomlib for communication with SP.
 * Userspace application can be either client or server. spcomlib shall
 * use write() file operation to send data, and read() file operation
 * to read data.
 *
 * This driver uses RPMSG with glink-spss as a transport layer.
 * This driver exposes "/dev/<sp-channel-name>" file node for each rpmsg
 * logical channel.
 * This driver exposes "/dev/spcom" file node for some debug/control command.
 * The predefined channel "/dev/sp_kernel" is used for loading SP application
 * from HLOS.
 * This driver exposes "/dev/sp_ssr" file node to allow user space poll for SSR.
 * After the remote SP App is loaded, this driver exposes a new file node
 * "/dev/<ch-name>" for the matching HLOS App to use.
 * The access to predefined file nodes and dynamically allocated file nodes is
 * restricted by using unix group and SELinux.
 *
 * No message routing is used, but using the rpmsg/G-Link "multiplexing" feature
 * to use a dedicated logical channel for HLOS and SP Application-Pair.
 *
 * Each HLOS/SP Application can be either Client or Server or both,
 * Messaging is allways point-to-point between 2 HLOS<=>SP applications.
 * Each channel exclusevly used by single Client or Server.
 *
 * User Space Request & Response are synchronous.
 * read() & write() operations are blocking until completed or terminated.
 */
#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/kernel.h>	/* min()             */
#include <linux/module.h>	/* MODULE_LICENSE    */
#include <linux/device.h>	/* class_create()    */
#include <linux/slab.h>	        /* kzalloc()         */
#include <linux/fs.h>		/* file_operations   */
#include <linux/cdev.h>	        /* cdev_add()        */
#include <linux/errno.h>	/* EINVAL, ETIMEDOUT */
#include <linux/printk.h>	/* pr_err()          */
#include <linux/bitops.h>	/* BIT(x)            */
#include <linux/completion.h>	/* wait_for_completion_timeout() */
#include <linux/poll.h>	/* POLLOUT */
#include <linux/platform_device.h>
#include <linux/of.h>		/* of_property_count_strings() */
#include <linux/workqueue.h>
#include <linux/delay.h>	/* msleep() */
#include <linux/dma-buf.h>
#include <linux/limits.h>
#include <linux/rpmsg.h>
#include <linux/atomic.h>
#include <linux/list.h>
#include <uapi/linux/spcom.h>
#include <linux/remoteproc.h>
#include <linux/ioctl.h>
#include <linux/ipc_logging.h>
#include <linux/pm.h>

#define SPCOM_LOG_PAGE_CNT 10

#define spcom_ipc_log_string(_x...) do {				\
	if (spcom_ipc_log_context)					\
		ipc_log_string(spcom_ipc_log_context, _x);		\
	} while (0)

#define spcom_pr_err(_fmt, ...) do {					\
	pr_err(_fmt, ##__VA_ARGS__);					\
	spcom_ipc_log_string("%s" pr_fmt(_fmt), "", ##__VA_ARGS__);	\
	} while (0)

#define spcom_pr_warn(_fmt, ...) do {					\
	pr_warn(_fmt, ##__VA_ARGS__);					\
	spcom_ipc_log_string("%s" pr_fmt(_fmt), "", ##__VA_ARGS__);	\
	} while (0)

#define spcom_pr_info(_fmt, ...) do {					\
	pr_info(_fmt, ##__VA_ARGS__);					\
	spcom_ipc_log_string("%s" pr_fmt(_fmt), "", ##__VA_ARGS__);	\
	} while (0)

#if defined(DEBUG)
#define spcom_pr_dbg(_fmt, ...) do {					\
	pr_debug(_fmt, ##__VA_ARGS__);					\
	spcom_ipc_log_string("%s" pr_fmt(_fmt), "", ##__VA_ARGS__);	\
	} while (0)
#else
#define spcom_pr_dbg(_fmt, ...) do {					\
	no_printk("%s" pr_fmt(_fmt), KERN_DEBUG, ##__VA_ARGS__);	\
	spcom_ipc_log_string("%s" pr_fmt(_fmt), "", ##__VA_ARGS__);	\
	} while (0)
#endif

/**
 * Request buffer size.
 * Any large data (multiply of 4KB) is provided by temp buffer in DDR.
 * Request shall provide the temp buffer physical address (align to 4KB).
 * Maximum request/response size of 268 is used to accommodate APDU size.
 * From kernel spcom driver perspective a PAGE_SIZE of 4K
 * is the actual maximum size for a single read/write file operation.
 */
#define SPCOM_MAX_RESPONSE_SIZE		268

/* SPCOM driver name */
#define DEVICE_NAME	"spcom"

/* maximum clients that can register over a single channel */
#define SPCOM_MAX_CHANNEL_CLIENTS 2

/* maximum shared DMA_buf buffers should be >= SPCOM_MAX_CHANNELS  */
#define SPCOM_MAX_DMA_BUF_PER_CH (SPCOM_MAX_CHANNELS + 4)

/* maximum ION buffer per send request/response command */
#define SPCOM_MAX_ION_BUF_PER_CMD SPCOM_MAX_ION_BUF

/* Maximum command size */
#define SPCOM_MAX_COMMAND_SIZE	(PAGE_SIZE)

/* Maximum input size */
#define SPCOM_MAX_READ_SIZE	(PAGE_SIZE)

/* Current Process ID */
#define current_pid() ((u32)(current->tgid))

/*
 * After both sides get CONNECTED,
 * there is a race between one side queueing rx buffer and the other side
 * trying to call glink_tx() , this race is only on the 1st tx.
 * Do tx retry with some delay to allow the other side to queue rx buffer.
 */
#define TX_RETRY_DELAY_MSEC	100

/* SPCOM_MAX_REQUEST_SIZE-or-SPCOM_MAX_RESPONSE_SIZE + header */
#define SPCOM_RX_BUF_SIZE	300

/*
 * Initial transaction id, use non-zero nonce for debug.
 * Incremented by client on request, and copied back by server on response.
 */
#define INITIAL_TXN_ID	0x12345678

/*
 * Maximum number of control channels between spcom driver and
 * user-mode processes
 */
#define SPCOM_MAX_CONTROL_CHANNELS  SPCOM_MAX_CHANNELS

/*
 * To be used for ioctl copy arg from user if the IOCTL direction is _IOC_WRITE
 * Update the union when new ioctl struct is added
 */
union spcom_ioctl_arg {
	struct spcom_poll_param poll;
	struct spcom_ioctl_poll_event poll_event;
	struct spcom_ioctl_ch channel;
	struct spcom_ioctl_message message;
	struct spcom_ioctl_modified_message modified_message;
	struct spcom_ioctl_next_request_size next_req_size;
	struct spcom_ioctl_dmabuf_lock dmabuf_lock;
} __packed;

/*
 * Max time to keep PM from suspend.
 * From receive RPMSG packet till wakeup source will be deactivated.
 */
#define SPCOM_PM_PACKET_HANDLE_TIMEOUT  (2 * MSEC_PER_SEC)

/**
 * struct spcom_msg_hdr - Request/Response message header between HLOS and SP.
 *
 * This header is proceeding any request specific parameters.
 * The transaction id is used to match request with response.
 * Note: rpmsg API provides the rx/tx data size, so user payload size is
 * calculated by reducing the header size.
 */
struct spcom_msg_hdr {
	uint32_t reserved;	/* for future use */
	uint32_t txn_id;	/* transaction id */
	char buf[0];		/* Variable buffer size, must be last field */
} __packed;

/**
 * struct spcom_client - Client handle
 */
struct spcom_client {
	struct spcom_channel *ch;
};

/**
 * struct spcom_server - Server handle
 */
struct spcom_server {
	struct spcom_channel *ch;
};

/**
 * struct dma_buf_info - DMA BUF support information
 */
struct dma_buf_info {
	int fd;
	struct dma_buf *handle;
	struct dma_buf_attachment *attach;
	struct sg_table *sg;
	u32 owner_pid;
};

/**
 * struct spcom_channel - channel context
 */
struct spcom_channel {
	char name[SPCOM_CHANNEL_NAME_SIZE];
	struct mutex lock;
	uint32_t txn_id;           /* incrementing nonce per client request */
	bool is_server;            /* for txn_id and response_timeout_msec  */
	bool comm_role_undefined;  /* is true on channel creation before first tx/rx on channel */
	uint32_t response_timeout_msec; /* for client only */

	/* char dev */
	struct cdev *cdev;
	struct device *dev;
	struct device_attribute attr;
	dev_t devt;

	/* rpmsg */
	struct rpmsg_driver *rpdrv;
	struct rpmsg_device *rpdev;

	/* Events notification */
	struct completion rx_done;
	struct completion connect;

	/**
	 * Only one client or server per non-sharable channel
	 * SPCOM_MAX_CHANNEL_CLIENTS clients for sharable channel
	 * Only one tx-rx transaction at a time (request + response)
	 */
	bool is_busy;
	bool is_sharable;              /* channel's sharable property         */
	u32 max_clients;               /* number of max clients               */
	u32 active_pid;                /* current tx-rx transaction pid       */
	uint8_t num_clients;           /* current number of clients           */
	struct mutex shared_sync_lock;

	u32 pid[SPCOM_MAX_CHANNEL_CLIENTS];

	/* abort flags */
	bool rpmsg_abort;

	/* rx data info */
	size_t actual_rx_size;	/* actual data size received */
	void *rpmsg_rx_buf;

	/**
	 * to track if rx_buf is read in the same session
	 * in which it is updated
	 */
	uint32_t rx_buf_txn_id;

	/* shared buffer lock/unlock support */
	struct dma_buf_info dmabuf_array[SPCOM_MAX_DMA_BUF_PER_CH];
};

/**
 * struct rx_buff_list - holds rx rpmsg data, before it will be consumed
 * by spcom_signal_rx_done worker, item per rx packet
 */
struct rx_buff_list {
	struct list_head list;

	void *rpmsg_rx_buf;
	int   rx_buf_size;
	struct spcom_channel *ch;
};

/**
 * struct spcom_channel - control channel information
 */
struct spcom_control_channel_info {
	u32 pid;
	u32 ref_cnt;
} spcom_control_channel_info;

/**
 * struct spcom_device - device state structure.
 */
struct spcom_device {
	char predefined_ch_name[SPCOM_MAX_CHANNELS][SPCOM_CHANNEL_NAME_SIZE];

	/* char device info */
	struct cdev cdev;
	dev_t device_no;
	struct class *driver_class;
	struct device *class_dev;
	struct platform_device *pdev;
	struct wakeup_source *ws;

	/* rpmsg channels */
	struct spcom_channel channels[SPCOM_MAX_CHANNELS];
	unsigned int chdev_count;
	struct mutex chdev_count_lock;
	struct mutex ch_list_lock;

	struct completion rpmsg_state_change;
	atomic_t rpmsg_dev_count;
	atomic_t remove_in_progress;

	/* rx data path */
	struct list_head    rx_list_head;
	spinlock_t          rx_lock;

	int32_t nvm_ion_fd;
	uint32_t     rmb_error;    /* PBL error value storet here */
	atomic_t subsys_req;
	struct rproc *spss_rproc;
	struct property *rproc_prop;

	/* Control channels */
	struct spcom_control_channel_info control_channels[SPCOM_MAX_CONTROL_CHANNELS];
};

/* Device Driver State */
static struct spcom_device *spcom_dev;
static void *spcom_ipc_log_context;

/* static functions declaration */
static int spcom_create_channel_chardev(const char *name, bool is_sharable);
static int spcom_destroy_channel_chardev(const char *name);
static struct spcom_channel *spcom_find_channel_by_name(const char *name);
static int spcom_register_rpmsg_drv(struct spcom_channel *ch);
static int spcom_unregister_rpmsg_drv(struct spcom_channel *ch);
static void spcom_release_all_channels_of_process(u32 pid);
static int spom_control_channel_add_client(u32 pid);
static int spom_control_channel_remove_client(u32 pid);

/**
 * spcom_is_channel_open() - channel is open on this side.
 *
 * Channel is fully connected, when rpmsg driver is registered and
 * rpmsg device probed
 */
static inline bool spcom_is_channel_open(struct spcom_channel *ch)
{
	return ch->rpdrv != NULL;
}

/**
 * spcom_is_channel_connected() - channel is fully connected by both sides.
 */
static inline bool spcom_is_channel_connected(struct spcom_channel *ch)
{
	/* Channel must be open before it gets connected */
	if (!spcom_is_channel_open(ch))
		return false;

	return ch->rpdev != NULL;
}

/**
 * spcom_create_predefined_channels_chardev() - expose predefined channels to
 * user space.
 *
 * Predefined channels list is provided by device tree.  Typically, it is for
 * known servers on remote side that are not loaded by the HLOS
 */
static int spcom_create_predefined_channels_chardev(void)
{
	int i, j;
	int ret, rc;
	static bool is_predefined_created;
	const char *name;

	if (is_predefined_created)
		return 0;

	for (i = 0; i < SPCOM_MAX_CHANNELS; i++) {
		name = spcom_dev->predefined_ch_name[i];

		if (name[0] == 0)
			break;

		mutex_lock(&spcom_dev->chdev_count_lock);
		ret = spcom_create_channel_chardev(name, false);
		mutex_unlock(&spcom_dev->chdev_count_lock);
		if (ret) {
			spcom_pr_err("fail to create chardev [%s], ret [%d]\n",
			       name, ret);
			goto destroy_channels;
		}
	}

	is_predefined_created = true;

	return 0;

destroy_channels:
	/* destroy previously created channels */
	for (j = 0; j < i; j++) {
		name = spcom_dev->predefined_ch_name[j];

		if (name[0] == 0)
			break;

		rc = spcom_destroy_channel_chardev(name);
		if (rc) {
			spcom_pr_err("fail to destroy chardev [%s], ret [%d]\n",
			       name, rc);
		}
	}

	return ret;
}

/*======================================================================*/
/*		UTILITIES						*/
/*======================================================================*/

/**
 * spcom_init_channel() - initialize channel state.
 *
 * @ch: channel state struct pointer
 * @is_sharable: whether channel is sharable
 * @name: channel name
 */
static int spcom_init_channel(struct spcom_channel *ch,
			      bool is_sharable,
			      const char *name)
{
	if (!ch || !name || !name[0]) {
		spcom_pr_err("invalid parameters\n");
		return -EINVAL;
	}

	strlcpy(ch->name, name, SPCOM_CHANNEL_NAME_SIZE);

	init_completion(&ch->rx_done);
	init_completion(&ch->connect);

	mutex_init(&ch->lock);
	ch->rpdrv = NULL;
	ch->rpdev = NULL;
	ch->actual_rx_size = 0;
	ch->is_busy = false;
	ch->txn_id = INITIAL_TXN_ID; /* use non-zero nonce for debug */
	ch->rx_buf_txn_id = ch->txn_id;
	memset(ch->pid, 0, sizeof(ch->pid));
	ch->rpmsg_abort = false;
	ch->rpmsg_rx_buf = NULL;
	ch->comm_role_undefined = true;
	ch->is_sharable = is_sharable;
	ch->max_clients = is_sharable ? SPCOM_MAX_CHANNEL_CLIENTS : 1;
	ch->active_pid = 0;
	ch->num_clients = 0;
	mutex_init(&ch->shared_sync_lock);
	return 0;
}

/**
 * spcom_find_channel_by_name() - find a channel by name.
 *
 * @name: channel name
 *
 * Return: a channel state struct.
 */
static struct spcom_channel *spcom_find_channel_by_name(const char *name)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(spcom_dev->channels); i++) {
		struct spcom_channel *ch = &spcom_dev->channels[i];

		if (strcmp(ch->name, name) == 0)
			return ch;
	}

	return NULL;
}

/**
 * spcom_rx() - wait for received data until timeout, unless pending rx data is
 *              already ready
 *
 * @ch: channel state struct pointer
 * @buf: buffer pointer
 * @size: buffer size
 *
 * Return: size in bytes on success, negative value on failure.
 */
static int spcom_rx(struct spcom_channel *ch,
		     void *buf,
		     uint32_t size,
		     uint32_t timeout_msec)
{
	unsigned long jiffies = msecs_to_jiffies(timeout_msec);
	long timeleft = 1;
	int ret = 0;

	mutex_lock(&ch->lock);

	if (ch->rx_buf_txn_id != ch->txn_id) {
		spcom_pr_dbg("ch[%s]:ch->rx_buf_txn_id=%d is updated in a different session\n",
			     ch->name, ch->rx_buf_txn_id);
		if (ch->rpmsg_rx_buf) {
			memset(ch->rpmsg_rx_buf, 0, ch->actual_rx_size);
			kfree((void *)ch->rpmsg_rx_buf);
			ch->rpmsg_rx_buf = NULL;
			ch->actual_rx_size = 0;
		}
	}

	/* check for already pending data */
	if (!ch->actual_rx_size) {
		reinit_completion(&ch->rx_done);

		mutex_unlock(&ch->lock); /* unlock while waiting */
		/* wait for rx response */
		if (timeout_msec)
			timeleft = wait_for_completion_interruptible_timeout(
						     &ch->rx_done, jiffies);
		else
			ret = wait_for_completion_interruptible(&ch->rx_done);
		mutex_lock(&ch->lock);

		if (timeout_msec && timeleft == 0) {
			spcom_pr_err("ch[%s]: timeout expired %d ms, set txn_id=%d\n",
			       ch->name, timeout_msec, ch->txn_id);
			ch->txn_id++; /* to drop expired rx packet later */
			ret = -ETIMEDOUT;
			goto exit_err;
		} else if (ch->rpmsg_abort) {
			spcom_pr_warn("rpmsg channel is closing\n");
			ret = -ERESTART;
			goto exit_err;
		} else if (ret < 0 || timeleft < 0) {
			spcom_pr_err("rx wait was interrupted!");
			ret = -EINTR; /* abort, not restartable */
			goto exit_err;
		} else if (ch->actual_rx_size) {
			spcom_pr_dbg("ch[%s]:actual_rx_size is [%zu], txn_id %d\n",
				 ch->name, ch->actual_rx_size, ch->txn_id);
		} else {
			spcom_pr_err("ch[%s]:actual_rx_size==0\n", ch->name);
			ret = -EFAULT;
			goto exit_err;
		}
	} else {
		spcom_pr_dbg("ch[%s]:rx data size [%zu], txn_id:%d\n",
				ch->name, ch->actual_rx_size, ch->txn_id);
	}
	if (!ch->rpmsg_rx_buf) {
		spcom_pr_err("ch[%s]:invalid rpmsg_rx_buf\n", ch->name);
		ret = -ENOMEM;
		goto exit_err;
	}

	size = min_t(size_t, ch->actual_rx_size, size);
	memcpy(buf, ch->rpmsg_rx_buf, size);

	memset(ch->rpmsg_rx_buf, 0, ch->actual_rx_size);
	kfree((void *)ch->rpmsg_rx_buf);
	ch->rpmsg_rx_buf = NULL;
	ch->actual_rx_size = 0;

	mutex_unlock(&ch->lock);

	return size;
exit_err:
	mutex_unlock(&ch->lock);

	return ret;
}

/**
 * spcom_get_next_request_size() - get request size.
 * already ready
 *
 * @ch: channel state struct pointer
 *
 * Server needs the size of the next request to allocate a request buffer.
 * Initially used intent-request, however this complicated the remote side,
 * so both sides are not using glink_tx() with INTENT_REQ anymore.
 *
 * Return: size in bytes on success, negative value on failure.
 */
static int spcom_get_next_request_size(struct spcom_channel *ch)
{
	int size = -1;
	int ret = 0;

	/* NOTE: Remote clients might not be connected yet.*/
	mutex_lock(&ch->lock);

	/* Update communication role of channel if not set yet */
	if (ch->comm_role_undefined) {
		spcom_pr_dbg("server [%s] reading it's first request\n", ch->name);
		ch->comm_role_undefined = false;
		ch->is_server = true;
	}

	reinit_completion(&ch->rx_done);

	/* check if already got it via callback */
	if (ch->actual_rx_size) {
		spcom_pr_dbg("next-req-size already ready ch [%s] size [%zu]\n",
			 ch->name, ch->actual_rx_size);
		goto exit_ready;
	}
	mutex_unlock(&ch->lock); /* unlock while waiting */

	ret = wait_for_completion_interruptible(&ch->rx_done);
	if (ret < 0) {
		spcom_pr_dbg("ch [%s]:interrupted wait ret=%d\n",
			 ch->name, ret);
		goto exit_error;
	}

	mutex_lock(&ch->lock); /* re-lock after waiting */

	if (ch->actual_rx_size == 0) {
		spcom_pr_err("invalid rx size [%zu] ch [%s]\n",
		       ch->actual_rx_size, ch->name);
		mutex_unlock(&ch->lock);
		ret = -EFAULT;
		goto exit_error;
	}

exit_ready:
	/* actual_rx_size not exeeds SPCOM_RX_BUF_SIZE*/
	size = (int)ch->actual_rx_size;
	if (size > sizeof(struct spcom_msg_hdr)) {
		size -= sizeof(struct spcom_msg_hdr);
	} else {
		spcom_pr_err("rx size [%d] too small\n", size);
		ret = -EFAULT;
		mutex_unlock(&ch->lock);
		goto exit_error;
	}

	mutex_unlock(&ch->lock);
	return size;

exit_error:
	return ret;
}

/*======================================================================*/
/*	USER SPACE commands handling					*/
/*======================================================================*/

/**
 * spcom_handle_create_channel_command() - Handle Create Channel command from
 * user space.
 *
 * @cmd_buf:	command buffer.
 * @cmd_size:	command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_create_channel_command(void *cmd_buf, int cmd_size)
{
	int ret = 0;
	struct spcom_user_create_channel_command *cmd = cmd_buf;

	if (cmd_size != sizeof(*cmd)) {
		spcom_pr_err("cmd_size [%d] , expected [%d]\n",
		       (int) cmd_size,  (int) sizeof(*cmd));
		return -EINVAL;
	}

	mutex_lock(&spcom_dev->chdev_count_lock);
	ret = spcom_create_channel_chardev(cmd->ch_name, cmd->is_sharable);
	mutex_unlock(&spcom_dev->chdev_count_lock);
	if (ret) {
		if (-EINVAL == ret)
			spcom_pr_err("failed to create channel, ret [%d]\n", ret);
		else
			spcom_pr_err("failed to create ch[%s], ret [%d]\n", cmd->ch_name, ret);
	}

	return ret;
}

/**
 * spcom_handle_restart_sp_command() - Handle Restart SP command from
 * user space.
 *
 * @cmd_buf:    command buffer.
 * @cmd_size:   command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_restart_sp_command(void *cmd_buf, int cmd_size)
{
	struct spcom_user_restart_sp_command *cmd = cmd_buf;
	int ret;

	if (!cmd) {
		spcom_pr_err("NULL cmd_buf\n");
		return -EINVAL;
	}

	if (cmd_size != sizeof(*cmd)) {
		spcom_pr_err("cmd_size [%d] , expected [%d]\n",
				(int) cmd_size,  (int) sizeof(*cmd));
		return -EINVAL;
	}

	spcom_dev->spss_rproc = rproc_get_by_phandle(be32_to_cpup(spcom_dev->rproc_prop->value));
	if (!spcom_dev->spss_rproc) {
		pr_err("rproc device not found\n");
		return -ENODEV;  /* no spss peripheral exist */
	}

	ret = rproc_boot(spcom_dev->spss_rproc);
	if (ret == -ETIMEDOUT) {
		/* userspace handles retry if needed */
		spcom_pr_err("FW loading process timeout\n");
	} else if (ret) {
		/*
		 *  SPU shutdown. Return value comes from SPU PBL message.
		 *  The error is not recoverable and userspace handles it
		 *  by request and analyse rmb_error value
		 */
		spcom_dev->rmb_error = (uint32_t)ret;

		spcom_pr_err("spss crashed during device bootup rmb_error[0x%x]\n",
			     spcom_dev->rmb_error);
		ret = -ENODEV;
	} else {
		spcom_pr_info("FW loading process is complete\n");
	}

	return ret;
}

/**
 * spcom_handle_send_command() - Handle send request/response from user space.
 *
 * @buf:	command buffer.
 * @buf_size:	command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_send_command(struct spcom_channel *ch,
					     void *cmd_buf, int size)
{
	int ret = 0;
	struct spcom_send_command *cmd = cmd_buf;
	uint32_t buf_size;
	void *buf;
	struct spcom_msg_hdr *hdr;
	void *tx_buf;
	int tx_buf_size;
	uint32_t timeout_msec;
	int time_msec = 0;

	spcom_pr_dbg("send req/resp ch [%s] size [%d]\n", ch->name, size);

	/*
	 * check that cmd buf size is at least struct size,
	 * to allow access to struct fields.
	 */
	if (size < sizeof(*cmd)) {
		spcom_pr_err("ch [%s] invalid cmd buf\n",
			ch->name);
		return -EINVAL;
	}

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		spcom_pr_err("ch [%s] remote side not connect\n", ch->name);
		return -ENOTCONN;
	}

	/* parse command buffer */
	buf = &cmd->buf;
	buf_size = cmd->buf_size;
	timeout_msec = cmd->timeout_msec;

	/* Check param validity */
	if (buf_size > SPCOM_MAX_RESPONSE_SIZE) {
		spcom_pr_err("ch [%s] invalid buf size [%d]\n",
			ch->name, buf_size);
		return -EINVAL;
	}
	if (size != sizeof(*cmd) + buf_size) {
		spcom_pr_err("ch [%s] invalid cmd size [%d]\n",
			ch->name, size);
		return -EINVAL;
	}

	/* Allocate Buffers*/
	tx_buf_size = sizeof(*hdr) + buf_size;
	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf)
		return -ENOMEM;

	/* Prepare Tx Buf */
	hdr = tx_buf;

	mutex_lock(&ch->lock);
	if (ch->comm_role_undefined) {
		spcom_pr_dbg("ch [%s] send first -> it is client\n", ch->name);
		ch->comm_role_undefined = false;
		ch->is_server = false;
	}

	if (!ch->is_server) {
		ch->txn_id++;   /* client sets the request txn_id */
		ch->response_timeout_msec = timeout_msec;
	}
	hdr->txn_id = ch->txn_id;

	/* user buf */
	memcpy(hdr->buf, buf, buf_size);

	time_msec = 0;
	do {
		if (ch->rpmsg_abort) {
			spcom_pr_err("ch [%s] aborted\n", ch->name);
			ret = -ECANCELED;
			break;
		}
		/* may fail when RX intent not queued by SP */
		ret = rpmsg_trysend(ch->rpdev->ept, tx_buf, tx_buf_size);
		if (ret == 0) {
			spcom_pr_dbg("ch[%s]: successfully sent txn_id=%d\n",
				     ch->name, ch->txn_id);
			break;
		}
		time_msec += TX_RETRY_DELAY_MSEC;
		mutex_unlock(&ch->lock);
		msleep(TX_RETRY_DELAY_MSEC);
		mutex_lock(&ch->lock);
	} while ((ret == -EBUSY || ret == -EAGAIN) && time_msec < timeout_msec);
	if (ret)
		spcom_pr_err("ch [%s] rpmsg_trysend() error (%d), timeout_msec=%d\n",
		       ch->name, ret, timeout_msec);

	if (ch->is_server) {
		__pm_relax(spcom_dev->ws);
		spcom_pr_dbg("ch[%s]:pm_relax() called for server, after tx\n",
			     ch->name);
	}
	mutex_unlock(&ch->lock);

	kfree(tx_buf);
	return ret;
}

/**
 * modify_dma_buf_addr() - replace the ION buffer virtual address with physical
 * address in a request or response buffer.
 *
 * @buf: buffer to modify
 * @buf_size: buffer size
 * @info: DMA buffer info such as FD and offset in buffer.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int modify_dma_buf_addr(struct spcom_channel *ch, void *buf,
			    uint32_t buf_size,
			    struct spcom_dma_buf_info *info)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sg = NULL;
	char *ptr = (char *)buf;
	dma_addr_t phy_addr = 0;
	uint32_t buf_offset = 0;
	int fd, ret = 0;
	int i = 0;
	bool found_handle = false;

	fd = info->fd;
	buf_offset = info->offset;
	ptr += buf_offset;

	if (fd < 0) {
		spcom_pr_err("invalid fd [%d]\n", fd);
		return -ENODEV;
	}

	if (buf_size < sizeof(uint64_t)) {
		spcom_pr_err("buf size too small [%d]\n", buf_size);
		return -ENODEV;
	}

	if (buf_offset % sizeof(uint64_t))
		spcom_pr_dbg("offset [%d] is NOT 64-bit aligned\n", buf_offset);
	else
		spcom_pr_dbg("offset [%d] is 64-bit aligned\n", buf_offset);

	if (buf_offset > buf_size - sizeof(uint64_t)) {
		spcom_pr_err("invalid buf_offset [%d]\n", buf_offset);
		return -ENODEV;
	}

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		spcom_pr_err("fail to get dma buf handle\n");
		return -EINVAL;
	}
	attach = dma_buf_attach(dma_buf, &spcom_dev->pdev->dev);
	if (IS_ERR_OR_NULL(attach)) {
		ret = PTR_ERR(attach);
		spcom_pr_err("fail to attach dma buf %d\n", ret);
		dma_buf_put(dma_buf);
		goto mem_map_table_failed;
	}

	sg = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sg)) {
		ret = PTR_ERR(sg);
		spcom_pr_err("fail to get sg table of dma buf %d\n", ret);
		goto mem_map_table_failed;
	}
	if (sg->sgl) {
		phy_addr = sg->sgl->dma_address;
	} else {
		spcom_pr_err("sgl is NULL\n");
		ret = -ENOMEM;
		goto mem_map_sg_failed;
	}

	for (i = 0 ; i < ARRAY_SIZE(ch->dmabuf_array) ; i++) {
		if (ch->dmabuf_array[i].handle == dma_buf) {
			ch->dmabuf_array[i].attach = attach;
			ch->dmabuf_array[i].sg = sg;
			found_handle = true;
			break;
		}
	}

	if (!found_handle) {
		spcom_pr_err("ch [%s]: trying to send modified command on unlocked buffer\n",
						ch->name);
		ret = -EPERM;
		goto mem_map_sg_failed;
	}

	/* Set the physical address at the buffer offset */
	spcom_pr_dbg("dma phys addr = [0x%lx]\n", (long) phy_addr);
	memcpy(ptr, &phy_addr, sizeof(phy_addr));

	/* Don't unmap the buffer to allow dmabuf sync start/end. */
	dma_buf_put(dma_buf);
	return 0;

mem_map_sg_failed:
	dma_buf_unmap_attachment(attach, sg, DMA_BIDIRECTIONAL);
mem_map_table_failed:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);

	return ret;
}

/**
 * spcom_handle_send_modified_command() - send a request/response with ION
 * buffer address. Modify the request/response by replacing the ION buffer
 * virtual address with the physical address.
 *
 * @ch: channel pointer
 * @cmd_buf: User space command buffer
 * @size: size of user command buffer
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_send_modified_command(struct spcom_channel *ch,
					       void *cmd_buf, int size)
{
	int ret = 0;
	struct spcom_user_send_modified_command *cmd = cmd_buf;
	uint32_t buf_size;
	void *buf;
	struct spcom_msg_hdr *hdr;
	void *tx_buf;
	int tx_buf_size;
	struct spcom_ion_info ion_info[SPCOM_MAX_ION_BUF_PER_CMD];
	int i;
	uint32_t timeout_msec;
	int time_msec = 0;
	struct spcom_dma_buf_info curr_info = {0};

	spcom_pr_dbg("send req/resp ch [%s] size [%d]\n", ch->name, size);

	/*
	 * check that cmd buf size is at least struct size,
	 * to allow access to struct fields.
	 */
	if (size < sizeof(*cmd)) {
		spcom_pr_err("ch [%s] invalid cmd buf\n",
			ch->name);
		return -EINVAL;
	}

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		spcom_pr_err("ch [%s] remote side not connect\n", ch->name);
		return -ENOTCONN;
	}

	/* parse command buffer */
	buf = &cmd->buf;
	buf_size = cmd->buf_size;
	timeout_msec = cmd->timeout_msec;
	memcpy(ion_info, cmd->ion_info, sizeof(ion_info));

	/* Check param validity */
	if (buf_size > SPCOM_MAX_RESPONSE_SIZE) {
		spcom_pr_err("ch [%s] invalid buf size [%d]\n",
			ch->name, buf_size);
		return -EINVAL;
	}
	if (size != sizeof(*cmd) + buf_size) {
		spcom_pr_err("ch [%s] invalid cmd size [%d]\n",
			ch->name, size);
		return -EINVAL;
	}

	/* Allocate Buffers*/
	tx_buf_size = sizeof(*hdr) + buf_size;
	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf)
		return -ENOMEM;

	/* Prepare Tx Buf */
	hdr = tx_buf;

	mutex_lock(&ch->lock);
	if (ch->comm_role_undefined) {
		spcom_pr_dbg("ch [%s] send first -> it is client\n", ch->name);
		ch->comm_role_undefined = false;
		ch->is_server = false;
	}
	if (!ch->is_server) {
		ch->txn_id++;   /* client sets the request txn_id */
		ch->response_timeout_msec = timeout_msec;
	}
	hdr->txn_id = ch->txn_id;

	/* user buf */
	memcpy(hdr->buf, buf, buf_size);

	for (i = 0 ; i < ARRAY_SIZE(ion_info) ; i++) {
		if (ion_info[i].fd >= 0) {
			curr_info.fd = ion_info[i].fd;
			curr_info.offset = ion_info[i].buf_offset;
			ret = modify_dma_buf_addr(ch, hdr->buf, buf_size, &curr_info);
			if (ret < 0) {
				mutex_unlock(&ch->lock);
				memset(tx_buf, 0, tx_buf_size);
				kfree(tx_buf);
				return -EFAULT;
			}
		}
	}

	time_msec = 0;
	do {
		if (ch->rpmsg_abort) {
			spcom_pr_err("ch[%s]: aborted, txn_id=%d\n",
				     ch->name, ch->txn_id);
			ret = -ECANCELED;
			break;
		}
		/* may fail when RX intent not queued by SP */
		ret = rpmsg_trysend(ch->rpdev->ept, tx_buf, tx_buf_size);
		if (ret == 0)
			break;
		time_msec += TX_RETRY_DELAY_MSEC;
		mutex_unlock(&ch->lock);
		msleep(TX_RETRY_DELAY_MSEC);
		mutex_lock(&ch->lock);
	} while ((ret == -EBUSY || ret == -EAGAIN) && time_msec < timeout_msec);
	if (ret)
		spcom_pr_err("ch [%s] rpmsg_trysend() error (%d), timeout_msec=%d\n",
		       ch->name, ret, timeout_msec);

	if (ch->is_server) {
		__pm_relax(spcom_dev->ws);
		spcom_pr_dbg("ch[%s]:pm_relax() called for server, after tx\n",
			     ch->name);
	}
	mutex_unlock(&ch->lock);
	memset(tx_buf, 0, tx_buf_size);
	kfree(tx_buf);
	return ret;
}


/**
 * spcom_handle_lock_ion_buf_command() - Lock an shared buffer.
 *
 * Lock an shared buffer, prevent it from being free if the userspace App crash,
 * while it is used by the remote subsystem.
 */
static int spcom_handle_lock_ion_buf_command(struct spcom_channel *ch,
					      void *cmd_buf, int size)
{
	struct spcom_user_command *cmd = cmd_buf;
	int fd;
	int i;
	struct dma_buf *dma_buf;

	if (size != sizeof(*cmd)) {
		spcom_pr_err("cmd size [%d] , expected [%d]\n",
		       (int) size,  (int) sizeof(*cmd));
		return -EINVAL;
	}

	if (cmd->arg > (unsigned int)INT_MAX) {
		spcom_pr_err("int overflow [%u]\n", cmd->arg);
		return -EINVAL;
	}
	fd = cmd->arg;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		spcom_pr_err("fail to get dma buf handle\n");
		return -EINVAL;
	}

	/* shared buf lock doesn't involve any rx/tx data to SP. */
	mutex_lock(&ch->lock);

	/* Check if this shared buffer is already locked */
	for (i = 0 ; i < ARRAY_SIZE(ch->dmabuf_array) ; i++) {
		if (ch->dmabuf_array[i].handle == dma_buf) {
			spcom_pr_dbg("fd [%d] shared buf is already locked\n",
				     fd);
			/* decrement back the ref count */
			mutex_unlock(&ch->lock);
			dma_buf_put(dma_buf);
			return -EINVAL;
		}
	}

	/* Store the dma_buf handle */
	for (i = 0 ; i < ARRAY_SIZE(ch->dmabuf_array) ; i++) {
		if (ch->dmabuf_array[i].handle == NULL) {
			ch->dmabuf_array[i].handle = dma_buf;
			ch->dmabuf_array[i].fd = fd;
			spcom_pr_dbg("ch [%s] locked ion buf #%d fd [%d] dma_buf=0x%pK\n",
				ch->name, i,
				ch->dmabuf_array[i].fd,
				ch->dmabuf_array[i].handle);
			mutex_unlock(&ch->lock);
			return 0;
		}
	}

	mutex_unlock(&ch->lock);
	/* decrement back the ref count */
	dma_buf_put(dma_buf);
	spcom_pr_err("no free entry to store ion handle of fd [%d]\n", fd);

	return -EFAULT;
}

/**
 * spcom_dmabuf_unlock() - unattach and free dmabuf
 *
 * unattach the dmabuf from spcom driver.
 * decrememt dmabuf ref count.
 */
static int spcom_dmabuf_unlock(struct dma_buf_info *info, bool verify_buf_owner)
{
	u32 pid = current_pid();

	if (info == NULL) {
		spcom_pr_err("Invalid dmabuf info pointer\n");
		return -EINVAL;
	}

	if (info->handle == NULL) {
		spcom_pr_err("DMA buffer handle is NULL\n");
		return -EINVAL;
	}

	if (verify_buf_owner) {
		if (pid == 0) {
			spcom_pr_err("Unknown PID\n");
			return -EINVAL;
		}

		if (info->owner_pid != pid) {
			spcom_pr_err("PID [%u] is not the owner of this DMA buffer\n", pid);
			return -EPERM;
		}
	}

	spcom_pr_dbg("unlock dmbuf fd [%d], PID [%u]\n", info->fd, pid);

	if (info->attach) {
		dma_buf_unmap_attachment(info->attach, info->sg, DMA_BIDIRECTIONAL);
		dma_buf_detach(info->handle, info->attach);
		info->attach = NULL;
		info->sg = NULL;
	}

	dma_buf_put(info->handle);
	info->handle = NULL;
	info->fd = -1;
	info->owner_pid = 0;

	return 0;
}

/**
 * spcom_handle_unlock_ion_buf_command() - Unlock an ION buffer.
 *
 * Unlock an ION buffer, let it be free, when it is no longer being used by
 * the remote subsystem.
 */
static int spcom_handle_unlock_ion_buf_command(struct spcom_channel *ch,
					      void *cmd_buf, int size)
{
	int i;
	struct spcom_user_command *cmd = cmd_buf;
	int fd;
	bool found = false;
	struct dma_buf *dma_buf;

	if (size != sizeof(*cmd)) {
		spcom_pr_err("cmd size [%d], expected [%d]\n",
		       (int)size, (int)sizeof(*cmd));
		return -EINVAL;
	}
	if (cmd->arg > (unsigned int)INT_MAX) {
		spcom_pr_err("int overflow [%u]\n", cmd->arg);
		return -EINVAL;
	}
	fd = cmd->arg;

	spcom_pr_dbg("Unlock ion buf ch [%s] fd [%d]\n", ch->name, fd);

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		spcom_pr_err("fail to get dma buf handle\n");
		return -EINVAL;
	}
	dma_buf_put(dma_buf);

	/* shared buf unlock doesn't involve any rx/tx data to SP. */
	mutex_lock(&ch->lock);
	if (fd == (int) SPCOM_ION_FD_UNLOCK_ALL) {
		spcom_pr_dbg("unlocked ALL ion buf ch [%s]\n", ch->name);
		found = true;
		/* unlock all buf */
		for (i = 0; i < ARRAY_SIZE(ch->dmabuf_array); i++)
			spcom_dmabuf_unlock(&ch->dmabuf_array[i], true);
	} else {
		/* unlock specific buf */
		for (i = 0 ; i < ARRAY_SIZE(ch->dmabuf_array) ; i++) {
			if (!ch->dmabuf_array[i].handle)
				continue;
			if (ch->dmabuf_array[i].handle == dma_buf) {
				spcom_dmabuf_unlock(&ch->dmabuf_array[i], true);
				found = true;
				break;
			}
		}
	}
	mutex_unlock(&ch->lock);

	if (!found) {
		spcom_pr_err("ch [%s] fd [%d] was not found\n", ch->name, fd);
		return -ENODEV;
	}

	return 0;
}

/**
 * spcom_handle_enable_ssr_command() - Handle user space request to enable ssr
 *
 * After FOTA SSR is disabled until IAR update occurs.
 * Then - enable SSR again
 *
 * Return: size in bytes on success, negative value on failure.
 */
static int spcom_handle_enable_ssr_command(void)
{
	spcom_pr_info("TBD: SSR is enabled after FOTA\n");
	return 0;
}

/**
 * spcom_handle_write() - Handle user space write commands.
 *
 * @buf:	command buffer.
 * @buf_size:	command buffer size.
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_handle_write(struct spcom_channel *ch,
			       void *buf,
			       int buf_size)
{
	int ret = 0;
	struct spcom_user_command *cmd = NULL;
	int cmd_id = 0;

	/* Minimal command should have command-id and argument */
	if (buf_size < sizeof(struct spcom_user_command)) {
		spcom_pr_err("Command buffer size [%d] too small\n", buf_size);
		return -EINVAL;
	}

	cmd = (struct spcom_user_command *)buf;
	cmd_id = (int) cmd->cmd_id;

	spcom_pr_dbg("cmd_id [0x%x]\n", cmd_id);

	if (!ch && cmd_id != SPCOM_CMD_CREATE_CHANNEL
			&& cmd_id != SPCOM_CMD_RESTART_SP
			&& cmd_id != SPCOM_CMD_ENABLE_SSR) {
		spcom_pr_err("channel context is null\n");
		return -EINVAL;
	}

	if (cmd_id == SPCOM_CMD_SEND || cmd_id == SPCOM_CMD_SEND_MODIFIED) {
		if (!spcom_is_channel_connected(ch)) {
			pr_err("ch [%s] remote side not connected\n", ch->name);
			return -ENOTCONN;
		}
	}

	switch (cmd_id) {
	case SPCOM_CMD_SEND:
		if (ch->is_sharable) {
			/* Channel shared, mutex protect TxRx */
			mutex_lock(&ch->shared_sync_lock);
			/* pid indicates the current active ch */
			ch->active_pid = current_pid();
		}
		ret = spcom_handle_send_command(ch, buf, buf_size);
		break;
	case SPCOM_CMD_SEND_MODIFIED:
		if (ch->is_sharable) {
			/* Channel shared, mutex protect TxRx */
			mutex_lock(&ch->shared_sync_lock);
			/* pid indicates the current active ch */
			ch->active_pid = current_pid();
		}
		ret = spcom_handle_send_modified_command(ch, buf, buf_size);
		break;
	case SPCOM_CMD_LOCK_ION_BUF:
		ret = spcom_handle_lock_ion_buf_command(ch, buf, buf_size);
		break;
	case SPCOM_CMD_UNLOCK_ION_BUF:
		ret = spcom_handle_unlock_ion_buf_command(ch, buf, buf_size);
		break;
	case SPCOM_CMD_CREATE_CHANNEL:
		ret = spcom_handle_create_channel_command(buf, buf_size);
		break;
	case SPCOM_CMD_RESTART_SP:
		ret = spcom_handle_restart_sp_command(buf, buf_size);
		break;
	case SPCOM_CMD_ENABLE_SSR:
		ret = spcom_handle_enable_ssr_command();
		break;
	default:
		spcom_pr_err("Invalid Command Id [0x%x]\n", (int) cmd->cmd_id);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * spcom_handle_get_req_size() - Handle user space get request size command
 *
 * @ch:	channel handle
 * @buf:	command buffer.
 * @size:	command buffer size.
 *
 * Return: size in bytes on success, negative value on failure.
 */
static int spcom_handle_get_req_size(struct spcom_channel *ch,
				      void *buf,
				      uint32_t size)
{
	int ret = -1;
	uint32_t next_req_size = 0;

	if (size < sizeof(next_req_size)) {
		spcom_pr_err("buf size [%d] too small\n", (int) size);
		return -EINVAL;
	}

	ret = spcom_get_next_request_size(ch);
	if (ret < 0)
		return ret;
	next_req_size = (uint32_t) ret;

	memcpy(buf, &next_req_size, sizeof(next_req_size));
	spcom_pr_dbg("next_req_size [%d]\n", next_req_size);

	return sizeof(next_req_size); /* can't exceed user buffer size */
}

/**
 * spcom_handle_read_req_resp() - Handle user space get request/response command
 *
 * @ch:	channel handle
 * @buf:	command buffer.
 * @size:	command buffer size.
 *
 * Return: size in bytes on success, negative value on failure.
 */
static int spcom_handle_read_req_resp(struct spcom_channel *ch,
				       void *buf,
				       uint32_t size)
{
	int ret;
	struct spcom_msg_hdr *hdr;
	void *rx_buf;
	int rx_buf_size;
	uint32_t timeout_msec = 0; /* client only */

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		spcom_pr_err("ch [%s] remote side not connect\n", ch->name);
		return -ENOTCONN;
	}

	/* Check param validity */
	if (size > SPCOM_MAX_RESPONSE_SIZE) {
		spcom_pr_err("ch [%s] invalid size [%d]\n",
			ch->name, size);
		return -EINVAL;
	}

	/* Allocate Buffers*/
	rx_buf_size = sizeof(*hdr) + size;
	rx_buf = kzalloc(rx_buf_size, GFP_KERNEL);
	if (!rx_buf)
		return -ENOMEM;

	/*
	 * client response timeout depends on the request
	 * handling time on the remote side .
	 */
	if (!ch->is_server) {
		timeout_msec = ch->response_timeout_msec;
		spcom_pr_dbg("response_timeout_msec:%d\n", (int) timeout_msec);
	}

	ret = spcom_rx(ch, rx_buf, rx_buf_size, timeout_msec);
	if (ret < 0) {
		spcom_pr_err("rx error %d\n", ret);
		goto exit_err;
	} else {
		size = ret; /* actual_rx_size */
	}

	hdr = rx_buf;

	if (ch->is_server) {
		ch->txn_id = hdr->txn_id;
		spcom_pr_dbg("ch[%s]:request txn_id [0x%x]\n",
			     ch->name, ch->txn_id);
	}

	/* copy data to user without the header */
	if (size > sizeof(*hdr)) {
		size -= sizeof(*hdr);
		memcpy(buf, hdr->buf, size);
	} else {
		spcom_pr_err("rx size [%d] too small\n", size);
		ret = -EFAULT;
		goto exit_err;
	}

	kfree(rx_buf);
	return size;
exit_err:
	kfree(rx_buf);
	return ret;
}

/**
 * spcom_handle_read() - Handle user space read request/response or
 * request-size command
 *
 * @ch:	channel handle
 * @buf:	command buffer.
 * @size:	command buffer size.
 *
 * A special size SPCOM_GET_NEXT_REQUEST_SIZE, which is bigger than the max
 * response/request tells the kernel that user space only need the size.
 *
 * Return: size in bytes on success, negative value on failure.
 */
static int spcom_handle_read(struct spcom_channel *ch,
			      void *buf,
			      uint32_t size)
{
	int ret = -1;

	if (size == SPCOM_GET_NEXT_REQUEST_SIZE) {
		ch->is_server = true;
		ret = spcom_handle_get_req_size(ch, buf, size);
	} else {
		ret = spcom_handle_read_req_resp(ch, buf, size);
	}

	mutex_lock(&ch->lock);
	if (!ch->is_server) {
		__pm_relax(spcom_dev->ws);
		spcom_pr_dbg("ch[%s]:pm_relax() called for client\n",
			     ch->name);
	}
	mutex_unlock(&ch->lock);

	return ret;
}

/*======================================================================*/
/*		CHAR DEVICE USER SPACE INTERFACE			*/
/*======================================================================*/

/**
 * file_to_filename() - get the filename from file pointer.
 *
 * @filp: file pointer
 *
 * it is used for debug prints.
 *
 * Return: filename string or "unknown".
 */
static char *file_to_filename(struct file *filp)
{
	struct dentry *dentry = NULL;
	char *filename = NULL;

	if (!filp || !filp->f_path.dentry)
		return "unknown";

	dentry = filp->f_path.dentry;
	filename = dentry->d_iname;

	return filename;
}

bool is_proc_channel_owner(struct spcom_channel *ch, u32 pid)
{
	int i = 0;

	for (i = 0; i < ch->max_clients; ++i) {
		if (ch->pid[i] == pid)
			return true;
	}

	return false;
}

/**
 * spcom_device_open() - handle channel file open() from user space.
 *
 * @filp: file pointer
 *
 * The file name (without path) is the channel name.
 * Register rpmsg driver matching with channel name.
 * Store the channel context in the file private date pointer for future
 * read/write/close operations.
 */
static int spcom_device_open(struct inode *inode, struct file *filp)
{
	struct spcom_channel *ch;
	int ret;
	const char *name = file_to_filename(filp);
	u32 pid = current_pid();
	int i = 0;

	if (atomic_read(&spcom_dev->remove_in_progress)) {
		spcom_pr_err("module remove in progress\n");
		return -ENODEV;
	}

	if (strcmp(name, "unknown") == 0) {
		spcom_pr_err("name is unknown\n");
		return -EINVAL;
	}

	if (strcmp(name, "sp_ssr") == 0) {
		spcom_pr_dbg("sp_ssr dev node skipped\n");
		return 0;
	}

	if (pid == 0) {
		spcom_pr_err("unknown PID\n");
		return -EINVAL;
	}

	if (strcmp(name, DEVICE_NAME) == 0) {
		spcom_pr_dbg("control channel is opened by pid %u\n", pid);
		return  spom_control_channel_add_client(pid);
	}

	ch = spcom_find_channel_by_name(name);
	if (!ch) {
		spcom_pr_err("ch[%s] doesn't exist, load app first\n", name);
		return -ENODEV;
	}

	mutex_lock(&ch->lock);
	if (!spcom_is_channel_open(ch)) {
		reinit_completion(&ch->connect);
		/* channel was closed need to register drv again */
		ret = spcom_register_rpmsg_drv(ch);
		if (ret < 0) {
			spcom_pr_err("register rpmsg driver failed %d\n", ret);
			mutex_unlock(&ch->lock);
			return ret;
		}
	}
	/* max number of channel clients reached */
	if (ch->is_busy) {
		spcom_pr_err("channel [%s] is BUSY and has %d of clients, already in use\n",
			name, ch->num_clients);
		mutex_unlock(&ch->lock);
		return -EBUSY;
	}

	/*
	 * if same client trying to register again, this will fail
	 */
	for (i = 0; i < SPCOM_MAX_CHANNEL_CLIENTS; i++) {
		if (ch->pid[i] == pid) {
			spcom_pr_err("client with pid [%d] is already registered with channel[%s]\n",
				pid, name);
			mutex_unlock(&ch->lock);
			return -EINVAL;
		}
	}

	if (ch->is_sharable) {
		ch->num_clients++;
		if (ch->num_clients >= SPCOM_MAX_CHANNEL_CLIENTS)
			ch->is_busy = true;
		else
			ch->is_busy = false;
		/* pid array has pid of all the registered client.
		 * If we reach here, the is_busy flag check above guarantees
		 * that we have atleast one non-zero pid index
		 */
		for (i = 0; i < SPCOM_MAX_CHANNEL_CLIENTS; i++) {
			if (ch->pid[i] == 0) {
				ch->pid[i] = pid;
				break;
			}
		}
	} else {
		ch->num_clients = 1;
		ch->is_busy = true;
		/* Only first index of pid is relevant in case of
		 * non-shareable
		 */
		ch->pid[0] = pid;
	}

	mutex_unlock(&ch->lock);

	filp->private_data = ch;
	return 0;
}

/**
 * spcom_device_release() - handle channel file close() from user space.
 *
 * @filp: file pointer
 *
 * The file name (without path) is the channel name.
 * Open the relevant glink channel.
 * Store the channel context in the file private
 * date pointer for future read/write/close
 * operations.
 */
static int spcom_device_release(struct inode *inode, struct file *filp)
{
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);
	int ret = 0;
	int i = 0;
	u32 pid = current_pid();

	if (strcmp(name, "unknown") == 0) {
		spcom_pr_err("name is unknown\n");
		return -EINVAL;
	}

	if (strcmp(name, "sp_ssr") == 0) {
		spcom_pr_dbg("sp_ssr dev node skipped\n");
		return 0;
	}

	if (pid == 0) {
		spcom_pr_err("unknown PID\n");
		return -EINVAL;
	}

	if (strcmp(name, DEVICE_NAME) == 0) {
		spcom_pr_dbg("PID [%d] release control channel\n", pid);
		return spom_control_channel_remove_client(pid);
	}

	ch = filp->private_data;
	if (!ch) {
		spcom_pr_dbg("ch is NULL, file name %s\n",
			     file_to_filename(filp));
		return -ENODEV;
	}

	mutex_lock(&ch->lock);
	/* channel might be already closed or disconnected */
	if (!spcom_is_channel_open(ch)) {
		spcom_pr_dbg("ch [%s] already closed\n", name);
		mutex_unlock(&ch->lock);
		return 0;
	}

	for (i = 0; i < SPCOM_MAX_CHANNEL_CLIENTS; i++) {
		if (ch->pid[i] == pid) {
			spcom_pr_dbg("PID [%x] is releasing ch [%s]\n", pid, name);
			ch->pid[i] = 0;
			break;
		}
	}

	if (ch->num_clients > 1) {
		/*
		 * Shared client is trying to close channel,
		 * release the sync_lock if applicable
		 */
		if (ch->active_pid == pid) {
			spcom_pr_dbg("active_pid [%x] is releasing ch [%s] sync lock\n",
				 ch->active_pid, name);
			/* No longer the current active user of the channel */
			ch->active_pid = 0;
			mutex_unlock(&ch->shared_sync_lock);
		}
		ch->num_clients--;
		ch->is_busy = false;
		mutex_unlock(&ch->lock);
		return 0;
	}

	ch->is_busy = false;
	ch->num_clients = 0;
	ch->active_pid = 0;

	if (ch->rpmsg_rx_buf) {
		spcom_pr_dbg("ch [%s] discarding unconsumed rx packet actual_rx_size=%zd\n",
		       name, ch->actual_rx_size);
		kfree(ch->rpmsg_rx_buf);
		ch->rpmsg_rx_buf = NULL;
	}
	ch->actual_rx_size = 0;
	mutex_unlock(&ch->lock);
	filp->private_data = NULL;
	return ret;
}

/**
 * spcom_device_write() - handle channel file write() from user space.
 *
 * @filp: file pointer
 *
 * Return: On Success - same size as number of bytes to write.
 * On Failure - negative value.
 */
static ssize_t spcom_device_write(struct file *filp,
				   const char __user *user_buff,
				   size_t size, loff_t *f_pos)
{
	int ret;
	char *buf;
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);
	int buf_size = 0;

	if (!user_buff || !f_pos || !filp) {
		spcom_pr_err("invalid null parameters\n");
		return -EINVAL;
	}

	if (atomic_read(&spcom_dev->remove_in_progress)) {
		spcom_pr_err("module remove in progress\n");
		return -ENODEV;
	}

	if (*f_pos != 0) {
		spcom_pr_err("offset should be zero, no sparse buffer\n");
		return -EINVAL;
	}

	if (!name) {
		spcom_pr_err("name is NULL\n");
		return -EINVAL;
	}

	if (strcmp(name, "unknown") == 0) {
		spcom_pr_err("name is unknown\n");
		return -EINVAL;
	}

	if (size > SPCOM_MAX_COMMAND_SIZE) {
		spcom_pr_err("size [%d] > max size [%d]\n",
			   (int) size, (int) SPCOM_MAX_COMMAND_SIZE);
		return -EINVAL;
	}

	ch = filp->private_data;
	if (!ch) {
		if (strcmp(name, DEVICE_NAME) != 0) {
			spcom_pr_err("NULL ch, command not allowed\n");
			return -EINVAL;
		}
	}
	buf_size = size; /* explicit casting size_t to int */
	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	ret = copy_from_user(buf, user_buff, size);
	if (ret) {
		spcom_pr_err("Unable to copy from user (err %d)\n", ret);
		kfree(buf);
		return -EFAULT;
	}

	ret = spcom_handle_write(ch, buf, buf_size);
	if (ret) {
		spcom_pr_err("handle command error [%d]\n", ret);
		kfree(buf);
		if (ch && ch->active_pid == current_pid()) {
			ch->active_pid = 0;
			mutex_unlock(&ch->shared_sync_lock);
		}
		return ret;
	}

	kfree(buf);

	return size;
}

/**
 * spcom_device_read() - handle channel file read() from user space.
 *
 * @filp: file pointer
 *
 * Return: number of bytes to read on success, negative value on
 * failure.
 */
static ssize_t spcom_device_read(struct file *filp, char __user *user_buff,
				 size_t size, loff_t *f_pos)
{
	int ret = 0;
	int actual_size = 0;
	char *buf;
	struct spcom_channel *ch;
	const char *name = file_to_filename(filp);
	uint32_t buf_size = 0;
	u32 cur_pid = current_pid();

	spcom_pr_dbg("read file [%s], size = %d bytes\n", name, (int) size);

	if (atomic_read(&spcom_dev->remove_in_progress)) {
		spcom_pr_err("module remove in progress\n");
		return -ENODEV;
	}

	if (strcmp(name, "unknown") == 0) {
		spcom_pr_err("name is unknown\n");
		return -EINVAL;
	}

	if (!user_buff || !f_pos ||
	    (size == 0) || (size > SPCOM_MAX_READ_SIZE)) {
		spcom_pr_err("invalid parameters\n");
		return -EINVAL;
	}
	buf_size = size; /* explicit casting size_t to uint32_t */

	ch = filp->private_data;

	if (ch == NULL) {
		spcom_pr_err("invalid ch pointer, file [%s]\n", name);
		return -EINVAL;
	}

	if (!spcom_is_channel_open(ch)) {
		spcom_pr_err("ch is not open, file [%s]\n", name);
		return -EINVAL;
	}

	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL) {
		ret =  -ENOMEM;
		goto exit_err;
	}

	ret = spcom_handle_read(ch, buf, buf_size);
	if (ret < 0) {
		if (ret != -ERESTARTSYS)
			spcom_pr_err("read error [%d]\n", ret);
		goto exit_err;
	}
	actual_size = ret;
	if ((actual_size == 0) || (actual_size > size)) {
		spcom_pr_err("invalid actual_size [%d]\n", actual_size);
		ret = -EFAULT;
		goto exit_err;
	}

	ret = copy_to_user(user_buff, buf, actual_size);
	if (ret) {
		spcom_pr_err("Unable to copy to user, err = %d\n", ret);
		ret = -EFAULT;
		goto exit_err;
	}
	kfree(buf);

	if (ch->active_pid == cur_pid) {
		ch->active_pid = 0;
		mutex_unlock(&ch->shared_sync_lock);
	}
	return actual_size;

exit_err:
	kfree(buf);
	if (ch->active_pid == cur_pid) {
		ch->active_pid = 0;
		mutex_unlock(&ch->shared_sync_lock);
	}
	return ret;
}

static inline int handle_poll(struct file *file,
		       struct spcom_poll_param *op, int *user_retval)
{
	struct spcom_channel *ch = NULL;
	const char *name = file_to_filename(file);
	int ready = 0;
	int ret = 0;

	switch (op->cmd_id) {
	case SPCOM_LINK_STATE_REQ:
		if (op->wait) {
			reinit_completion(&spcom_dev->rpmsg_state_change);
			ready = wait_for_completion_interruptible(
					  &spcom_dev->rpmsg_state_change);
			spcom_pr_dbg("ch [%s] link state change signaled\n",
				     name);
		}
		op->retval = atomic_read(&spcom_dev->rpmsg_dev_count) > 0;
		break;
	case SPCOM_CH_CONN_STATE_REQ:
		if (strcmp(name, DEVICE_NAME) == 0) {
			spcom_pr_err("invalid control device: %s\n", name);
			return -EINVAL;
		}
		/*
		 * ch is not expected to be NULL since user must call open()
		 * to get FD before it can call poll().
		 * open() will fail if no ch related to the char-device.
		 */
		ch = file->private_data;
		if (!ch) {
			spcom_pr_err("invalid ch pointer, file [%s]\n", name);
			ret = -EINVAL;
			break;
		}
		if (op->wait) {
			reinit_completion(&ch->connect);
			ready = wait_for_completion_interruptible(&ch->connect);
			spcom_pr_dbg("ch [%s] connect signaled\n", name);
		}
		mutex_lock(&ch->lock);
		op->retval = (ch->rpdev != NULL);
		mutex_unlock(&ch->lock);
		break;
	default:
		spcom_pr_err("ch [%s] unsupported ioctl:%u\n",
			name, op->cmd_id);
		ret = -EINVAL;
	}
	if (ready < 0) { /* wait was interrupted */
		spcom_pr_info("interrupted wait retval=%d\n", op->retval);
		ret = -EINTR;
	}

	if (!ret) {
		ret = put_user(op->retval, user_retval);
		if (ret) {
			spcom_pr_err("Unable to copy link state to user [%d]\n", ret);
			ret = -EFAULT;
		}
	}

	return ret;
}

/*======================================================================*/
/*		IOCTL USER SPACE COMMANDS HANDLING		*/
/*======================================================================*/

/**
 * spcom_register_channel
 *
 * @brief      Helper function to register SPCOM channel
 *
 * @param[in]  ch    SPCOM channel
 *
 * @return     zero on success, negative value otherwise.
 */
static int spcom_register_channel(struct spcom_channel *ch)
{
	const char *ch_name = NULL;
	u32 pid = current_pid();
	u32 i = 0;

	ch_name = ch->name;

	mutex_lock(&ch->lock);
	spcom_pr_dbg("the pid name [%s] of pid [%d] try to open [%s] channel\n",
		     current->comm, pid, ch_name);

	if (!spcom_is_channel_open(ch))
		spcom_pr_err("channel [%s] is not open\n", ch_name);

	/* max number of channel clients reached */
	if (ch->is_busy) {
		spcom_pr_err("channel [%s] is occupied by max num of clients [%d]\n",
			ch_name, ch->num_clients);
		mutex_unlock(&ch->lock);
		return -EBUSY;
	}

	 /* check if same client trying to register again */
	for (i = 0; i < ch->max_clients; ++i) {
		if (ch->pid[i] == pid) {
			spcom_pr_err("client with pid[%d] is already registered with channel[%s]\n",
				pid, ch_name);
			mutex_unlock(&ch->lock);
			return -EINVAL;
		}
	}

	if (ch->is_sharable) {
		/* set or add channel owner PID */
		for (i = 0; i < ch->max_clients; ++i) {
			if (ch->pid[i] == 0)
				break;
		}
	} else {
		i = 0;
	}

	/* update channel client, whether the channel is shared or not */
	ch->pid[i] = pid;
	ch->num_clients++;
	ch->is_busy = (ch->num_clients == ch->max_clients) ? true : false;
	mutex_unlock(&ch->lock);

	return 0;
}

/**
 * is_valid_ch_name
 *
 * @brief      Helper function to verify channel name pointer
 *
 * @param[in]  ch_name    channel name
 *
 * @return     true if valid channel name pointer, false otherwise.
 */
static inline bool is_valid_ch_name(const char *ch_name)
{
	static const uint32_t maxlen = SPCOM_CHANNEL_NAME_SIZE;

	return (ch_name && ch_name[0] && (strnlen(ch_name, maxlen) < maxlen));
}

/**
 * is_control_channel_name
 *
 * @brief      Helper function to check if channel name is the control channel name
 *
 * @param[in]  ch_name    channel name
 *
 * @return     true if control channel name, false otherwise.
 */
static inline bool is_control_channel_name(const char *ch_name)
{
	return (is_valid_ch_name(ch_name) && (!strcmp(ch_name, DEVICE_NAME)));
}

/**
 * spcom_channel_deinit_locked
 *
 * @brief      Helper function to handle deinit of SPCOM channel while holding the channel's lock
 *
 * @param[in]  ch    SPCOM channel
 *
 * @return     zero on successful operation, negative value otherwise.
 */
static int spcom_channel_deinit_locked(struct spcom_channel *ch, u32 pid)
{
	const char *ch_name = ch->name;
	bool found = false;
	u32 i = 0;

	/* channel might be already closed or disconnected */
	if (!spcom_is_channel_open(ch)) {
		spcom_pr_dbg("ch [%s] already closed\n", ch_name);
		return 0;
	}

	/* check that current process is a client of this channel */
	for (i = 0; i < ch->max_clients; ++i) {
		if (ch->pid[i] == pid) {
			found = true;
			spcom_pr_dbg("pid [%x] is releasing ch [%s]\n", pid, ch_name);
			ch->pid[i] = 0;
			break;
		}
	}

	/* if the current process is not a valid client of this channel, return an error */
	if (!found) {
		spcom_pr_dbg("pid [%d] is not a client of ch [%s]\n", pid, ch_name);
		return -EFAULT;
	}

	/* If shared client owner is trying to close channel, release the sync_lock if
	 * applicable
	 */
	if (ch->active_pid == pid) {
		spcom_pr_dbg("active_pid [%d] is releasing ch [%s] sync lock\n",
			ch->active_pid, ch_name);
		ch->active_pid = 0;
		mutex_unlock(&ch->shared_sync_lock);
	}

	ch->num_clients--;
	ch->is_busy = false;

	if (ch->rpmsg_rx_buf) {
		spcom_pr_dbg("ch [%s] discarding unconsumed rx packet actual_rx_size=%zd\n",
			ch_name, ch->actual_rx_size);
		kfree(ch->rpmsg_rx_buf);
		ch->rpmsg_rx_buf = NULL;
	}
	ch->actual_rx_size = 0;

	return 0;
}

/**
 * spcom_channel_deinit
 *
 * @brief      Helper function to handle deinit of SPCOM channel
 *
 * @param[in]  ch    SPCOM channel
 *
 * @return     zero on successful operation, negative value otherwise.
 */
static int spcom_channel_deinit(struct spcom_channel *ch)
{
	uint32_t pid = current_pid();
	int ret;

	if (!pid) {
		spcom_pr_err("unknown PID\n");
		return -EINVAL;
	}

	mutex_lock(&ch->lock);
	ret = spcom_channel_deinit_locked(ch, pid);
	mutex_unlock(&ch->lock);

	return ret;
}

/**
 * spcom_send_message
 *
 * @brief Helper function to send request/response IOCTL command from user space
 *
 * @param[in]  arg            IOCTL command arguments
 * @param[in]  buffer         user message buffer
 * @param[in]  is_modified    flag to indicate if this is a modified message or regular message
 *
 * Return: 0 on successful operation, negative value otherwise.
 */
static int spcom_send_message(void *arg, void *buffer, bool is_modified)
{
	struct spcom_channel *ch = NULL;
	struct spcom_msg_hdr *hdr = NULL;
	struct spcom_ioctl_message *usr_msg = NULL;
	struct spcom_ioctl_modified_message *usr_mod_msg = NULL;
	const char *ch_name = NULL;
	void *msg_buf = NULL;
	void *tx_buf = NULL;
	int tx_buf_size = 0;
	uint32_t msg_buf_sz = 0;
	uint32_t dma_info_array_sz = 0;
	int ret = 0;
	int time_msec = 0;
	int timeout_msec = 0;
	int i = 0;

	/* Parse message command arguments */
	if (is_modified) {
		/* Send regular message */
		usr_mod_msg = arg;
		msg_buf = buffer;
		msg_buf_sz = usr_mod_msg->buffer_size;
		timeout_msec = usr_mod_msg->timeout_msec;
		ch_name = usr_mod_msg->ch_name;
	} else {
		/* Send modified message */
		usr_msg = arg;
		msg_buf = buffer;
		msg_buf_sz = usr_msg->buffer_size;
		timeout_msec = usr_msg->timeout_msec;
		ch_name = usr_msg->ch_name;
	}

	/* Verify channel name */
	if (!is_valid_ch_name(ch_name)) {
		spcom_pr_err("invalid channel name\n");
		return -EINVAL;
	}

	/* Verify message buffer size */
	if (msg_buf_sz > SPCOM_MAX_RESPONSE_SIZE) {
		spcom_pr_err("ch [%s] message size is too big [%d]\n", ch_name, msg_buf_sz);
		return -EINVAL;
	}

	/* DEVICE_NAME is reserved for control channel */
	if (is_control_channel_name(ch_name)) {
		spcom_pr_err("cannot send message on control channel\n");
		return -EFAULT;
	}

	/* Find spcom channel in spcom channel list by name */
	ch = spcom_find_channel_by_name(ch_name);
	if (!ch)
		return -ENODEV;

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		spcom_pr_err("ch [%s] remote side not connected\n", ch_name);
		return -ENOTCONN;
	}

	spcom_pr_dbg("sending message with size [%d], ch [%s]\n", msg_buf_sz, ch_name);

	/* Allocate and prepare Tx buffer */
	tx_buf_size = sizeof(*hdr) + msg_buf_sz;
	tx_buf = kzalloc(tx_buf_size, GFP_KERNEL);
	if (!tx_buf)
		return -ENOMEM;
	hdr = tx_buf;

	mutex_lock(&ch->lock);

	/* For SPCOM server, get next request size must be called before sending a response
	 * if we got here and the role is not set it means the channel is SPCOM client
	 */
	if (ch->comm_role_undefined) {
		spcom_pr_dbg("client ch [%s] sending it's first message\n", ch_name);
		ch->comm_role_undefined = false;
		ch->is_server = false;
	}

	/* Protect shared channel Tx by lock and set the current process as the owner */
	if (ch->is_sharable) {

		if (ch->is_server) {
			spcom_pr_err("server spcom channel cannot be shared\n");
			goto send_message_err;
		}

		mutex_lock(&ch->shared_sync_lock);
		ch->active_pid = current_pid();
	}

	/* SPCom client sets the request txn_id */
	if (!ch->is_server) {
		ch->txn_id++;
		ch->response_timeout_msec = timeout_msec;
	}
	hdr->txn_id = ch->txn_id;

	/* Copy user buffer to tx */
	memcpy(hdr->buf, msg_buf, msg_buf_sz);

	/* For modified message write the DMA buffer addresses to the user defined offset in the
	 * message buffer
	 */
	if (is_modified) {
		dma_info_array_sz = ARRAY_SIZE(usr_mod_msg->info);

		if (dma_info_array_sz != SPCOM_MAX_DMA_BUF) {
			spcom_pr_err("invalid info array size [%d], ch[%s]\n", dma_info_array_sz,
				ch_name);
			ret = -EINVAL;
			goto send_message_err;
		}

		for (i = 0; i < dma_info_array_sz; ++i) {
			if (usr_mod_msg->info[i].fd >= 0) {
				ret = modify_dma_buf_addr(ch, hdr->buf, msg_buf_sz,
					&usr_mod_msg->info[i]);
				if (ret) {
					ret = -EFAULT;
					goto send_message_err;
				}
			}
		}
	}

	/* Send Tx to remote edge */
	do {
		if (ch->rpmsg_abort) {
			spcom_pr_err("ch [%s] aborted\n", ch_name);
			ret = -ECANCELED;
			break;
		}

		/* may fail when Rx intent not queued by remote edge */
		ret = rpmsg_trysend(ch->rpdev->ept, tx_buf, tx_buf_size);
		if (ret == 0) {
			spcom_pr_dbg("ch[%s]: successfully sent txn_id=%d\n", ch_name, ch->txn_id);
			break;
		}

		time_msec += TX_RETRY_DELAY_MSEC;

		/* release channel lock before sleep */
		mutex_unlock(&ch->lock);
		msleep(TX_RETRY_DELAY_MSEC);
		mutex_lock(&ch->lock);

	} while ((ret == -EBUSY || ret == -EAGAIN) && time_msec < timeout_msec);

	if (ret)
		spcom_pr_err("Tx failed: ch [%s], err [%d], timeout [%d]ms\n",
			ch_name, ret, timeout_msec);

	ret = msg_buf_sz;

send_message_err:

	if (ret < 0 && ch->is_sharable && ch->active_pid == current_pid()) {
		ch->active_pid = 0;
		mutex_unlock(&ch->shared_sync_lock);
	}

	/* close pm awake window after spcom server response */
	if (ch->is_server) {
		__pm_relax(spcom_dev->ws);
		spcom_pr_dbg("ch[%s]:pm_relax() called for server, after tx\n",
			     ch->name);
	}
	mutex_unlock(&ch->lock);
	memset(tx_buf, 0, tx_buf_size);
	kfree(tx_buf);
	return ret;
}

/**
 * is_control_channel
 *
 * @brief      Helper function to check if device file if of a control channel
 *
 * @param[in]  file    device file
 *
 * @return     true if file is control device file, false otherwise.
 */
static inline bool is_control_channel(struct file *file)
{
	return (!strcmp(file_to_filename(file), DEVICE_NAME)) ? true : false;
}

/**
 * spcom_ioctl_handle_restart_spu_command
 *
 * @brief  Handle SPU restart IOCTL command from user space
 *
 * @return zero on success, negative value otherwise.
 */
static int spcom_ioctl_handle_restart_spu_command(void)
{
	int ret = 0;

	spcom_pr_dbg("SPSS restart command\n");

	spcom_dev->spss_rproc = rproc_get_by_phandle(be32_to_cpup(spcom_dev->rproc_prop->value));
	if (!spcom_dev->spss_rproc) {
		pr_err("rproc device not found\n");
		return -ENODEV;  /* no spss peripheral exist */
	}

	ret = rproc_boot(spcom_dev->spss_rproc);
	if (ret == -ETIMEDOUT) {
		/* userspace should handle retry if needed */
		spcom_pr_err("FW loading process timeout\n");
	} else if (ret) {
		/*
		 *  SPU shutdown. Return value comes from SPU PBL message.
		 *  The error is not recoverable and userspace handles it
		 *  by request and analyse rmb_error value
		 */
		spcom_dev->rmb_error = (uint32_t)ret;
		spcom_pr_err("spss crashed during device bootup rmb_error[0x%x]\n",
			     spcom_dev->rmb_error);
		ret = -ENODEV;
	} else {
		spcom_pr_info("FW loading process is complete\n");
	}

	return ret;
}

/**
 * spcom_create_channel
 *
 * @brief      Helper function to create spcom channel
 *
 * @param[in]  ch_name        spcom channel name
 * @param[in]  is_sharable    true if sharable channel, false otherwise
 *
 * @return     zero on success, negative value otherwise.
 */
static int spcom_create_channel(const char *ch_name, bool is_sharable)
{
	struct spcom_channel *ch = NULL;
	struct spcom_channel *free_ch = NULL;
	int ret = 0;
	int i = 0;

	/* check if spcom remove was called */
	if (atomic_read(&spcom_dev->remove_in_progress)) {
		spcom_pr_err("module remove in progress\n");
		ret = -ENODEV;
	}

	if (!is_valid_ch_name(ch_name)) {
		spcom_pr_err("invalid channel name\n");
		return -EINVAL;
	}

	if (is_control_channel_name(ch_name)) {
		spcom_pr_err("cannot create control channel\n");
		return -EINVAL;
	}

	spcom_pr_dbg("create spcom channel, name[%s], is sharable[%d]\n", ch_name, is_sharable);

	for (i = 0; i < SPCOM_MAX_CHANNELS; ++i) {

		/* Check if channel already exist */
		ch = &spcom_dev->channels[i];
		if (!strcmp(ch->name, ch_name))
			break;

		/* Keep address of first free channel */
		if (!free_ch && ch->name[0] == 0)
			free_ch = ch;
	}

	/* Channel doesn't exist */
	if (i == SPCOM_MAX_CHANNELS) {

		/* No free slot to create a new channel */
		if (!free_ch) {
			spcom_pr_err("no free channel\n");
			return -ENODEV;
		}

		/* Create a new channel */
		ret = spcom_init_channel(free_ch, is_sharable, ch_name);
		if (ret)
			ret = -ENODEV;

	} else if (is_sharable) {

		/* Channel is already created as sharable */
		if (spcom_dev->channels[i].is_sharable) {
			spcom_pr_err("already created channel as sharable\n");
			return -EALREADY;
		}

		/* Cannot create sharable channel if channel already created */
		spcom_pr_err("channel already exist, cannot create sharable channel\n");
		ret = -EINVAL;
	}

	if (ret)
		spcom_pr_err("create channel [%s] failed, ret[%d]\n", ch_name, ret);
	else
		spcom_pr_dbg("create channel [%s] is done\n", ch_name);

	return ret;
}

/**
 * spcom_ioctl_handle_create_shared_ch_command
 *
 * @brief      Handle SPCOM create shared channel IOCTL command from user space
 *
 * @param[in]  arg    SPCOM create shared channel IOCTL command arguments
 *
 * @return     zero on success, negative value otherwise.
 */
static inline int spcom_ioctl_handle_create_shared_ch_command(struct spcom_ioctl_ch *arg)
{
	int ret = 0;

	/* Lock before modifying spcom device global channel list */
	mutex_lock(&spcom_dev->ch_list_lock);

	ret = spcom_create_channel(arg->ch_name, true /*sharable*/);

	/* Unlock spcom device global channel list */
	mutex_unlock(&spcom_dev->ch_list_lock);

	return ret;
}

/**
 * spcom_handle_channel_register_command
 *
 * @brief      Handle register to SPCOM channel IOCTL command from user space
 *
 *             Handle both create SPCOM channel (if needed) and register SPCOM channel to avoid
 *             race condition between two processes trying to register to the same channel. The
 *             channel list is protected by a single lock during the create and register flow.
 *
 * @param[in]  arg    IOCTL command arguments
 *
 * @return     zero on success, negative value otherwise.
 */
static int spcom_ioctl_handle_channel_register_command(struct spcom_ioctl_ch *arg)
{
	struct spcom_channel *ch = NULL;
	const char *ch_name =  arg->ch_name;
	int ret = 0;

	if (!current_pid()) {
		spcom_pr_err("unknown PID\n");
		return -EINVAL;
	}

	/* Lock before modifying spcom device global channel list */
	mutex_lock(&spcom_dev->ch_list_lock);

	ret = spcom_create_channel(ch_name, false /*non-sharable*/);
	if (ret) {
		mutex_unlock(&spcom_dev->ch_list_lock);
		return ret;
	}

	ch = spcom_find_channel_by_name(ch_name);
	if (!ch) {
		mutex_unlock(&spcom_dev->ch_list_lock);
		return -ENODEV;
	}

	/* If channel open is called for the first time need to register rpmsg_drv
	 * Please note: spcom_register_rpmsg_drv acquire the channel lock
	 */
	if (!spcom_is_channel_open(ch)) {
		reinit_completion(&ch->connect);
		ret = spcom_register_rpmsg_drv(ch);
		if (ret < 0) {
			mutex_unlock(&spcom_dev->ch_list_lock);
			spcom_pr_err("register rpmsg driver failed %d\n", ret);
			return ret;
		}
	}

	ret = spcom_register_channel(ch);

	/* Unlock spcom device global channel list */
	mutex_unlock(&spcom_dev->ch_list_lock);

	return ret;
}

/**
 * spcom_ioctl_handle_channel_unregister_commnad
 *
 * @brief      Handle SPCOM channel unregister IOCTL command from user space
 *
 * @param[in]  arg    IOCTL command arguments
 *
 * @return     zero on successful operation, negative value otherwise.
 */
static int spcom_ioctl_handle_channel_unregister_command(struct spcom_ioctl_ch *arg)
{
	struct spcom_channel *ch = NULL;
	const char *ch_name = NULL;
	int ret = 0;

	if (!current_pid()) {
		spcom_pr_err("unknown PID\n");
		return -EINVAL;
	}

	spcom_pr_dbg("unregister channel cmd arg: ch_name[%s]\n", arg->ch_name);

	ch_name = arg->ch_name;
	if (!is_valid_ch_name(ch_name)) {
		spcom_pr_err("invalid channel name\n");
		return -EINVAL;
	}

	if (is_control_channel_name(ch_name)) {
		spcom_pr_err("cannot unregister control channel\n");
		return -EINVAL;
	}

	/* Lock before modifying spcom device global channel list */
	mutex_lock(&spcom_dev->ch_list_lock);

	ch = spcom_find_channel_by_name(ch_name);
	if (!ch) {
		spcom_pr_err("could not find channel[%s]\n", ch_name);
		mutex_unlock(&spcom_dev->ch_list_lock);
		return -ENODEV;
	}

	/* Reset channel context */
	ret = spcom_channel_deinit(ch);

	/* Unlock spcom device global channel list */
	mutex_unlock(&spcom_dev->ch_list_lock);

	spcom_pr_dbg("spcom unregister ch[%s] is done, ret[%d]\n", ch_name, ret);

	return ret;
}

/**
 * spcom_ioctl_handle_is_channel_connected
 *
 * @brief     Handle check if SPCOM channel is connected IOCTL command from user space
 *
 * @arg[in]   IOCTL command arguments
 *
 * @return    zero if not connected, positive value if connected, negative value otherwise.
 */
static int spcom_ioctl_handle_is_channel_connected(struct spcom_ioctl_ch *arg)
{
	const char *ch_name = arg->ch_name;
	struct spcom_channel *ch = NULL;
	int ret = 0;

	spcom_pr_dbg("Is channel connected cmd arg: ch_name[%s]\n", arg->ch_name);

	if (!is_valid_ch_name(ch_name)) {
		spcom_pr_err("invalid channel name\n");
		return -EINVAL;
	}

	if (is_control_channel_name(ch_name)) {
		spcom_pr_err("invalid control device: %s\n", ch_name);
		return -EINVAL;
	}

	ch = spcom_find_channel_by_name(ch_name);
	if (!ch) {
		spcom_pr_err("could not find channel[%s]\n", ch_name);
		return -EINVAL;
	}

	mutex_lock(&ch->lock);
	/* rpdev is set during spcom_rpdev_probe when remote app is loaded */
	ret = (ch->rpdev != NULL) ? 1 : 0;
	mutex_unlock(&ch->lock);

	return ret;
}

/**
 * spcom_ioctl_handle_lock_dmabuf_commnad
 *
 * @brief      Handle DMA buffer lock IOCTL command from user space
 *
 * @param[in]  arg    IOCTL command arguments
 *
 * @return     zero on successful operation, negative value otherwise.
 */
static int spcom_ioctl_handle_lock_dmabuf_command(struct spcom_ioctl_dmabuf_lock *arg)
{
	struct spcom_channel *ch = NULL;
	struct dma_buf *dma_buf = NULL;
	const char *ch_name = NULL;
	uint32_t pid = current_pid();
	int fd = 0;
	int i = 0;

	spcom_pr_dbg("Lock dmabuf cmd arg: ch_name[%s], fd[%d], padding[%u], PID[%ld]\n",
		arg->ch_name, arg->fd, arg->padding, current_pid());

	ch_name = arg->ch_name;
	if (!is_valid_ch_name(ch_name)) {
		spcom_pr_err("invalid channel name\n");
		return -EINVAL;
	}

	fd = arg->fd;

	if (!pid) {
		spcom_pr_err("unknown PID\n");
		return -EINVAL;
	}

	if (fd > (unsigned int)INT_MAX) {
		spcom_pr_err("int overflow [%u]\n", fd);
		return -EINVAL;
	}

	ch = spcom_find_channel_by_name(ch_name);
	if (!ch) {
		spcom_pr_err("could not find channel[%s]\n", ch_name);
		return -ENODEV;
	}

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		spcom_pr_err("fail to get dma buf handle\n");
		return -EINVAL;
	}

	/* DMA buffer lock doesn't involve any Rx/Tx data to remote edge */
	mutex_lock(&ch->lock);

	/* Check if channel is open */
	if (!spcom_is_channel_open(ch)) {
		spcom_pr_err("Channel [%s] is closed\n", ch_name);
		mutex_unlock(&ch->lock);
		dma_buf_put(dma_buf);
		return -EINVAL;
	}

	/* Check if this shared buffer is already locked */
	for (i = 0 ; i < ARRAY_SIZE(ch->dmabuf_array); i++) {
		if (ch->dmabuf_array[i].handle == dma_buf) {
			spcom_pr_dbg("fd [%d] shared buf is already locked\n", fd);
			mutex_unlock(&ch->lock);
			dma_buf_put(dma_buf); /* decrement back the ref count */
			return -EINVAL;
		}
	}

	/* Store the dma_buf handle */
	for (i = 0 ; i < ARRAY_SIZE(ch->dmabuf_array); i++) {

		struct dma_buf_info *curr_buf = &ch->dmabuf_array[i];

		if (curr_buf->handle == NULL) {
			curr_buf->handle = dma_buf;
			curr_buf->fd = fd;
			curr_buf->owner_pid = pid;

			spcom_pr_dbg("ch [%s] locked dma buf #%d fd [%d] dma_buf=0x%pK pid #%d\n",
				ch_name, i, curr_buf->fd, curr_buf->handle, curr_buf->owner_pid);

			mutex_unlock(&ch->lock);
			return 0;
		}
	}

	mutex_unlock(&ch->lock);

	/* decrement back the ref count */
	dma_buf_put(dma_buf);

	spcom_pr_err("No free entry to store dmabuf handle of fd [%d] on ch [%s]\n", fd, ch_name);

	return -EFAULT;
}

/**
 * spcom_ioctl_handle_unlock_dmabuf_commnad
 *
 * @brief      Handle DMA buffer unlock IOCTL command from user space
 *
 * @param[in]  arg    IOCTL command arguments
 *
 * @return     zero on success, negative value otherwise.
 */
static int spcom_ioctl_handle_unlock_dmabuf_command(struct spcom_ioctl_dmabuf_lock *arg)
{
	struct spcom_channel *ch = NULL;
	struct dma_buf *dma_buf = NULL;
	const char *ch_name = NULL;
	struct dma_buf *curr_handle = NULL;
	bool found = false;
	int fd = 0;
	int i = 0;
	int ret = 0;
	bool unlock_all = false;

	spcom_pr_dbg("Unlock dmabuf cmd arg: ch_name[%s], fd[%d], padding[%u], PID[%ld]\n",
		arg->ch_name, arg->fd, arg->padding, current_pid());

	ch_name = arg->ch_name;
	if (!is_valid_ch_name(ch_name))
		return -EINVAL;

	fd = arg->fd;

	if (fd > (unsigned int)INT_MAX) {
		spcom_pr_err("int overflow [%u]\n", fd);
		return -EINVAL;
	}

	if (fd == (int) SPCOM_DMABUF_FD_UNLOCK_ALL) {
		spcom_pr_dbg("unlock all FDs of PID [%d]\n", current_pid());
		unlock_all = true;
	}

	ch = spcom_find_channel_by_name(ch_name);
	if (!ch)
		return -ENODEV;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		spcom_pr_err("Failed to get dma buf handle, fd [%d]\n", fd);
		return -EINVAL;
	}
	dma_buf_put(dma_buf);

	mutex_lock(&ch->lock);

	if (unlock_all) { /* Unlock all buffers of current PID on channel */
		for (i = 0; i < ARRAY_SIZE(ch->dmabuf_array); i++) {
			if (spcom_dmabuf_unlock(&ch->dmabuf_array[i], true) == 0)
				found = true;
		}
	} else { /* Unlock specific buffer if owned by current PID */
		for (i = 0; i < ARRAY_SIZE(ch->dmabuf_array); i++) {
			curr_handle = ch->dmabuf_array[i].handle;
			if (curr_handle && curr_handle == dma_buf) {
				ret = spcom_dmabuf_unlock(&ch->dmabuf_array[i], true);
				found = true;
				break;
			}
		}
	}

	mutex_unlock(&ch->lock);

	if (!found) {
		spcom_pr_err("Buffer fd [%d] was not found for PID [%u] on channel [%s]\n",
			fd, current_pid(), ch_name);
		return -ENODEV;
	}

	return ret;
}

/**
 * spcom_ioctl_handle_get_message
 *
 * @brief      Handle get message (request or response) IOCTL command from user space
 *
 * @param[in]   arg                 IOCTL command arguments
 * @param[out]  user_buffer         user space buffer to copy message to
 *
 * @return     size in bytes on success, negative value on failure.
 */
static int spcom_ioctl_handle_get_message(struct spcom_ioctl_message *arg, void *user_buffer)
{
	struct spcom_channel *ch = NULL;
	struct spcom_msg_hdr *hdr = NULL;
	const char *ch_name = NULL;
	void *rx_buf = NULL;
	int rx_buf_size = 0;
	uint32_t msg_sz = arg->buffer_size;
	uint32_t timeout_msec = 0; /* client only */
	int ret = 0;

	spcom_pr_dbg("Get message cmd arg: ch_name[%s], timeout_msec [%u], buffer size[%u]\n",
		arg->ch_name, arg->timeout_msec, arg->buffer_size);

	ch_name = arg->ch_name;
	if (!is_valid_ch_name(ch_name)) {
		spcom_pr_err("invalid channel name\n");
		return -EINVAL;
	}

	/* DEVICE_NAME name is reserved for control channel */
	if (is_control_channel_name(ch_name)) {
		spcom_pr_err("cannot send message on management channel\n", ch_name);
		return -EFAULT;
	}

	ch = spcom_find_channel_by_name(ch_name);
	if (!ch)
		return -ENODEV;

	/* Check if remote side connect */
	if (!spcom_is_channel_connected(ch)) {
		spcom_pr_err("ch [%s] remote side not connect\n", ch_name);
		ret = -ENOTCONN;
		goto get_message_out;
	}

	/* Check param validity */
	if (msg_sz > SPCOM_MAX_RESPONSE_SIZE) {
		spcom_pr_err("ch [%s] invalid size [%d]\n", ch_name, msg_sz);
		ret = -EINVAL;
		goto get_message_out;
	}

	spcom_pr_dbg("waiting for incoming message, ch[%s], size[%u]\n", ch_name, msg_sz);

	/* Allocate Buffers*/
	rx_buf_size = sizeof(*hdr) + msg_sz;
	rx_buf = kzalloc(rx_buf_size, GFP_KERNEL);
	if (!rx_buf) {
		ret = -ENOMEM;
		goto get_message_out;
	}

	/* Client response timeout depends on the request handling time on the remote side
	 * Server send response to remote edge and return immediately, timeout isn't needed
	 */
	if (!ch->is_server) {
		timeout_msec = ch->response_timeout_msec;
		spcom_pr_dbg("response timeout_msec [%d]\n", (int) timeout_msec);
	}

	ret = spcom_rx(ch, rx_buf, rx_buf_size, timeout_msec);
	if (ret < 0) {
		spcom_pr_err("rx error %d\n", ret);
		goto get_message_out;
	}

	msg_sz = ret; /* actual_rx_size */
	hdr = rx_buf;

	if (ch->is_server) {
		ch->txn_id = hdr->txn_id; /* SPCOM server sets the request tnx_id */
		spcom_pr_dbg("ch[%s]: request txn_id [0x%x]\n", ch_name, ch->txn_id);
	}

	/* Verify incoming message size */
	if (msg_sz <= sizeof(*hdr)) {
		spcom_pr_err("rx size [%d] too small\n", msg_sz);
		ret = -EFAULT;
		goto get_message_out;
	}

	/* Copy message to user */
	msg_sz -= sizeof(*hdr);

	spcom_pr_dbg("copying message to user space, size: [%d]\n", msg_sz);
	ret = copy_to_user(user_buffer, hdr->buf, msg_sz);
	if (ret) {
		spcom_pr_err("failed to copy to user, ret [%d]\n");
		ret = -EFAULT;
		goto get_message_out;
	}

	ret = msg_sz;
	spcom_pr_dbg("get message done, msg size[%d]\n", msg_sz);

get_message_out:

	if (ch->active_pid == current_pid()) {
		ch->active_pid = 0;
		mutex_unlock(&ch->shared_sync_lock);
	}

	kfree(rx_buf);

	/* close pm awake window for spcom client get response */
	mutex_lock(&ch->lock);
	if (!ch->is_server) {
		__pm_relax(spcom_dev->ws);
		spcom_pr_dbg("ch[%s]:pm_relax() called for server, after tx\n",
			     ch->name);
	}
	mutex_unlock(&ch->lock);
	return ret;
}

/**
 * spcom_ioctl_handle_poll_event
 *
 * @brief       Handle SPCOM event poll ioctl command from user space
 *
 * @param[in]   arg            IOCTL command arguments
 * @param[out]  user_retval    user space address of poll return value
 *
 * @return  Zero on success, negative value on failure.
 */
static int spcom_ioctl_handle_poll_event(struct spcom_ioctl_poll_event *arg, int32_t *user_retval)
{
	int ret = 0;
	uint32_t link_state = 0;

	spcom_pr_dbg("Handle poll event cmd args: event_id[%d], wait[%u], retval[%d], padding[%d]\n",
		arg->event_id, arg->wait, arg->retval, arg->padding);

	switch (arg->event_id) {
	case SPCOM_EVENT_LINK_STATE:
	{
		if (arg->wait) {
			reinit_completion(&spcom_dev->rpmsg_state_change);
			ret = wait_for_completion_interruptible(
				&spcom_dev->rpmsg_state_change);

			if (ret) {/* wait was interrupted */
				spcom_pr_info("Wait for link state change interrupted, ret[%d]\n",
						ret);
				return -EINTR;
			}
		}

		if (atomic_read(&spcom_dev->rpmsg_dev_count) > 0)
			link_state = 1;

		spcom_pr_dbg("SPCOM link state change: Signaled [%d], PID [%d]\n",
			link_state, current_pid());

		ret = put_user(link_state, user_retval);
		if (ret) {
			spcom_pr_err("unable to copy link state to user [%d]\n", ret);
			return -EFAULT;
		}

		return 0;
	}
	default:
		spcom_pr_err("SPCOM handle poll unsupported event id [%u]\n", arg->event_id);
		return -EINVAL;
	}

	return -EBADRQC;
}

/**
 * spcom_ioctl_handle_get_next_req_msg_size
 *
 * @brief  Handle user space get next request message size IOCTL command from user space
 *
 * @param  arg[in]               IOCTL command arguments
 * @param  user_req_size[out]    user space next request size pointer
 *
 * @Return size in bytes on success, negative value on failure.
 */
static int spcom_ioctl_handle_get_next_req_msg_size(struct spcom_ioctl_next_request_size *arg,
		uint32_t *user_size)
{
	struct spcom_channel *ch = NULL;
	const char *ch_name = NULL;
	int ret = 0;

	spcom_pr_dbg("Get next request msg size cmd arg: ch_name[%s], size[%u], padding[%d]\n",
		arg->ch_name, arg->size, arg->padding);

	ch_name = arg->ch_name;
	if (!is_valid_ch_name(ch_name))
		return -EINVAL;

	ch = spcom_find_channel_by_name(ch_name);
	if (!ch)
		return -ENODEV;

	ret = spcom_get_next_request_size(ch);
	if (ret < 0)
		return ret;

	spcom_pr_dbg("Channel[%s], next request size[%d]\n", ch_name, ret);

	/* Copy next request size to user space */
	ret = put_user(ret, user_size);
	if (ret) {
		spcom_pr_err("unable to copy to user [%d]\n", ret);
		return -EFAULT;
	}

	return 0;
}

/**
 * spcom_ioctl_handle_copy_and_send_message
 *
 * @brief      Handle SPCOM send message (request or response) IOCTL command from user space
 *
 * @param[in]  arg              IOCTL command arguments
 * @param[in]  user_msg_buffer  user message buffer
 * @param[in]  is_modified      flag to indicate if this is a modified message or regular message
 *
 * @return:    zero on success, negative value otherwise.
 */
static int spcom_ioctl_handle_copy_and_send_message(void *arg, void *user_msg_buffer,
	bool is_modified)
{
	struct spcom_ioctl_modified_message *mod_msg = NULL;
	struct spcom_ioctl_message *msg = NULL;
	void *msg_buffer_copy = NULL;
	uint32_t buffer_size = 0;
	int ret = 0;

	if (is_modified) {
		mod_msg = (struct spcom_ioctl_modified_message *)arg;
		buffer_size = mod_msg->buffer_size;
	} else {
		msg = (struct spcom_ioctl_message *)arg;
		buffer_size = msg->buffer_size;
	}

	msg_buffer_copy = kzalloc(buffer_size, GFP_KERNEL);
	if (!msg_buffer_copy)
		return -ENOMEM;

	spcom_pr_dbg("copying message buffer from user space, size[%u]\n", buffer_size);
	ret = copy_from_user(msg_buffer_copy, user_msg_buffer, buffer_size);
	if (ret) {
		spcom_pr_err("failed to copy from user, ret [%d]\n", ret);
		kfree(msg_buffer_copy);
		return -EFAULT;
	}

	/* Send SPCOM message to remote edge */
	ret = spcom_send_message(arg, msg_buffer_copy, is_modified);
	kfree(msg_buffer_copy);
	return ret;
}

/**
 * spcom_ioctl_copy_user_arg
 *
 * Helper function to copy user arguments of IOCTL commands
 *
 * @user_arg:  user IOCTL command arguments pointer
 * @arg_copy:  internal copy of user arguments
 * @size:      size of user arguments struct
 *
 * @return:    zero on success, negative value otherwise.
 */
static inline int spcom_ioctl_copy_user_arg(void *user_arg, void *arg_copy, uint32_t size)
{
	int ret = 0;

	if (!user_arg) {
		spcom_pr_err("user arg is NULL\n");
		return -EINVAL;
	}

	ret = copy_from_user(arg_copy, user_arg, size);
	if (ret) {
		spcom_pr_err("copy from user failed, size [%u], ret[%d]\n", size, ret);
		return -EFAULT;
	}

	return 0;
}

bool is_arg_size_expected(unsigned int cmd, uint32_t arg_size)
{
	uint32_t expected_size = 0;

	switch (cmd) {
	case SPCOM_POLL_STATE:
		expected_size = sizeof(struct spcom_poll_param);
		break;

	case SPCOM_IOCTL_STATE_POLL:
		expected_size = sizeof(struct spcom_ioctl_poll_event);
		break;
	case SPCOM_IOCTL_SEND_MSG:
	case SPCOM_IOCTL_GET_MSG:
		expected_size = sizeof(struct spcom_ioctl_message);
		break;
	case SPCOM_IOCTL_SEND_MOD_MSG:
		expected_size = sizeof(struct spcom_ioctl_modified_message);
		break;
	case SPCOM_IOCTL_GET_NEXT_REQ_SZ:
		expected_size = sizeof(struct spcom_ioctl_next_request_size);
		break;
	case SPCOM_IOCTL_SHARED_CH_CREATE:
	case SPCOM_IOCTL_CH_REGISTER:
	case SPCOM_IOCTL_CH_UNREGISTER:
	case SPCOM_IOCTL_CH_IS_CONNECTED:
		expected_size = sizeof(struct spcom_ioctl_ch);
		break;
	case SPCOM_IOCTL_DMABUF_LOCK:
	case SPCOM_IOCTL_DMABUF_UNLOCK:
		expected_size = sizeof(struct spcom_ioctl_dmabuf_lock);
		break;
	default:
		spcom_pr_err("No userspace data for ioctl cmd[%ld]\n", cmd);
		return false;
	}

	if (arg_size != expected_size) {
		spcom_pr_err("Invalid cmd size: cmd[%ld], arg size[%u], expected[%u]\n",
				cmd, arg_size, expected_size);
		return false;
	}

	return true;
}

static long spcom_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *user_arg = (void __user *)arg;
	union spcom_ioctl_arg arg_copy = {0};
	uint32_t arg_size = 0;
	int ret = 0;

	spcom_pr_dbg("ioctl cmd [%u], PID [%d]\n", _IOC_NR(cmd), current_pid());

	if (atomic_read(&spcom_dev->remove_in_progress)) {
		spcom_pr_err("module remove in progress\n");
		return -ENODEV;
	}

	if (!is_control_channel(file) && cmd != SPCOM_POLL_STATE) {
		spcom_pr_err("ioctl is supported only for control channel\n");
		return -EINVAL;
	}

	if ((_IOC_DIR(cmd) & _IOC_WRITE)) {
		arg_size = _IOC_SIZE(cmd);
		if (!is_arg_size_expected(cmd, arg_size))
			return -EFAULT;

		ret = spcom_ioctl_copy_user_arg(user_arg, &arg_copy, arg_size);
		if (ret)
			return ret;
	}

	switch (cmd) {
	case SPCOM_POLL_STATE:
		return handle_poll(file, &(arg_copy.poll),
			&((struct spcom_poll_param *)user_arg)->retval);

	case SPCOM_GET_RMB_ERROR:
		return put_user(spcom_dev->rmb_error, (uint32_t *)arg);

	case SPCOM_IOCTL_STATE_POLL:
		return spcom_ioctl_handle_poll_event(
			&(arg_copy.poll_event),
			&((struct spcom_ioctl_poll_event *)user_arg)->retval);

	case SPCOM_IOCTL_SEND_MSG:
		return spcom_ioctl_handle_copy_and_send_message(&(arg_copy.message),
			((struct spcom_ioctl_message *)user_arg)->buffer, false);

	case SPCOM_IOCTL_SEND_MOD_MSG:
		return spcom_ioctl_handle_copy_and_send_message(&(arg_copy.message),
			((struct spcom_ioctl_modified_message *)user_arg)->buffer, true);

	case SPCOM_IOCTL_GET_NEXT_REQ_SZ:
		return spcom_ioctl_handle_get_next_req_msg_size(&(arg_copy.next_req_size),
			&((struct spcom_ioctl_next_request_size *)user_arg)->size);

	case SPCOM_IOCTL_GET_MSG:
		return spcom_ioctl_handle_get_message(&(arg_copy.message),
			((struct spcom_ioctl_message *)user_arg)->buffer);

	case SPCOM_IOCTL_SHARED_CH_CREATE:
		return spcom_ioctl_handle_create_shared_ch_command(&(arg_copy.channel));

	case SPCOM_IOCTL_CH_REGISTER:
		return spcom_ioctl_handle_channel_register_command(&(arg_copy.channel));

	case SPCOM_IOCTL_CH_UNREGISTER:
		return spcom_ioctl_handle_channel_unregister_command(&(arg_copy.channel));

	case SPCOM_IOCTL_CH_IS_CONNECTED:
		return spcom_ioctl_handle_is_channel_connected(&(arg_copy.channel));

	case SPCOM_IOCTL_DMABUF_LOCK:
		return spcom_ioctl_handle_lock_dmabuf_command(&(arg_copy.dmabuf_lock));

	case SPCOM_IOCTL_DMABUF_UNLOCK:
		return spcom_ioctl_handle_unlock_dmabuf_command(&(arg_copy.dmabuf_lock));

	case SPCOM_IOCTL_RESTART_SPU:
		return spcom_ioctl_handle_restart_spu_command();

	case SPCOM_IOCTL_ENABLE_SSR:
		return spcom_handle_enable_ssr_command();

	default:
		spcom_pr_err("ioctl cmd[%d] is not supported\n", cmd);
	}

	return -ENOIOCTLCMD;
}

/* file operation supported from user space */
static const struct file_operations fops = {
	.read = spcom_device_read,
	.write = spcom_device_write,
	.open = spcom_device_open,
	.release = spcom_device_release,
	.unlocked_ioctl = spcom_device_ioctl,
};

/**
 * spcom_create_channel_chardev() - Create a channel char-dev node file
 * for user space interface
 */
static int spcom_create_channel_chardev(const char *name, bool is_sharable)
{
	int ret;
	struct device *dev;
	struct spcom_channel *ch;
	dev_t devt;
	struct class *cls = spcom_dev->driver_class;
	struct device *parent = spcom_dev->class_dev;
	void *priv;
	struct cdev *cdev;

	if (!name || strnlen(name, SPCOM_CHANNEL_NAME_SIZE) ==
			SPCOM_CHANNEL_NAME_SIZE) {
		spcom_pr_err("invalid channel name\n");
		return -EINVAL;
	}

	spcom_pr_dbg("creating channel [%s]\n", name);

	ch = spcom_find_channel_by_name(name);
	if (ch) {
		spcom_pr_err("channel [%s] already exist\n", name);
		return -EBUSY;
	}

	ch = spcom_find_channel_by_name(""); /* find reserved channel */
	if (!ch) {
		spcom_pr_err("no free channel\n");
		return -ENODEV;
	}

	ret = spcom_init_channel(ch, is_sharable, name);
	if (ret < 0) {
		spcom_pr_err("can't init channel %d\n", ret);
		return ret;
	}

	ret = spcom_register_rpmsg_drv(ch);
	if (ret < 0) {
		spcom_pr_err("register rpmsg driver failed %d\n", ret);
		goto exit_destroy_channel;
	}

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev) {
		ret = -ENOMEM;
		goto exit_unregister_drv;
	}

	devt = spcom_dev->device_no + spcom_dev->chdev_count;
	priv = ch;

	/*
	 * Pass channel name as formatted string to avoid abuse by using a
	 * formatted string as channel name
	 */
	dev = device_create(cls, parent, devt, priv, "%s", name);
	if (IS_ERR(dev)) {
		spcom_pr_err("device_create failed\n");
		ret = -ENODEV;
		goto exit_free_cdev;
	}

	cdev_init(cdev, &fops);
	cdev->owner = THIS_MODULE;

	ret = cdev_add(cdev, devt, 1);
	if (ret < 0) {
		spcom_pr_err("cdev_add failed %d\n", ret);
		ret = -ENODEV;
		goto exit_destroy_device;
	}
	spcom_dev->chdev_count++;

	mutex_lock(&ch->lock);
	ch->cdev = cdev;
	ch->dev = dev;
	ch->devt = devt;
	mutex_unlock(&ch->lock);

	return 0;

exit_destroy_device:
	device_destroy(spcom_dev->driver_class, devt);
exit_free_cdev:
	kfree(cdev);
exit_unregister_drv:
	ret = spcom_unregister_rpmsg_drv(ch);
	if (ret != 0)
		spcom_pr_err("can't unregister rpmsg drv %d\n", ret);
exit_destroy_channel:
	/* empty channel leaves free slot for next time*/
	mutex_lock(&ch->lock);
	memset(ch->name, 0, SPCOM_CHANNEL_NAME_SIZE);
	mutex_unlock(&ch->lock);
	return ret;
}

// TODO: error handling
static int spcom_destroy_channel_chardev(const char *name)
{
	int ret;
	struct spcom_channel *ch;

	spcom_pr_err("destroy channel [%s]\n", name);

	ch = spcom_find_channel_by_name(name);
	if (!ch) {
		spcom_pr_err("channel [%s] not exist\n", name);
		return -EINVAL;
	}

	ret = spcom_unregister_rpmsg_drv(ch);
	if (ret < 0)
		spcom_pr_err("unregister rpmsg driver failed %d\n", ret);

	mutex_lock(&ch->lock);
	device_destroy(spcom_dev->driver_class, ch->devt);
	kfree(ch->cdev);
	mutex_unlock(&ch->lock);

	mutex_lock(&spcom_dev->chdev_count_lock);
	spcom_dev->chdev_count--;
	mutex_unlock(&spcom_dev->chdev_count_lock);


	return 0;
}

static int spcom_register_chardev(void)
{
	int ret;
	unsigned int baseminor = 0;
	unsigned int count = 1;

	ret = alloc_chrdev_region(&spcom_dev->device_no, baseminor, count,
				 DEVICE_NAME);
	if (ret < 0) {
		spcom_pr_err("alloc_chrdev_region failed %d\n", ret);
		return ret;
	}

	spcom_dev->driver_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(spcom_dev->driver_class)) {
		ret = -ENOMEM;
		spcom_pr_err("class_create failed %d\n", ret);
		goto exit_unreg_chrdev_region;
	}

	spcom_dev->class_dev = device_create(spcom_dev->driver_class, NULL,
				  spcom_dev->device_no, spcom_dev, DEVICE_NAME);

	if (IS_ERR(spcom_dev->class_dev)) {
		spcom_pr_err("class_device_create failed %d\n", ret);
		ret = -ENOMEM;
		goto exit_destroy_class;
	}

	cdev_init(&spcom_dev->cdev, &fops);
	spcom_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&spcom_dev->cdev,
		       MKDEV(MAJOR(spcom_dev->device_no), 0),
		       SPCOM_MAX_CHANNELS);
	if (ret < 0) {
		spcom_pr_err("cdev_add failed %d\n", ret);
		goto exit_destroy_device;
	}

	spcom_pr_dbg("char device created\n");

	return 0;

exit_destroy_device:
	device_destroy(spcom_dev->driver_class, spcom_dev->device_no);
exit_destroy_class:
	class_destroy(spcom_dev->driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(spcom_dev->device_no, 1);
	return ret;
}

static void spcom_unregister_chrdev(void)
{
	cdev_del(&spcom_dev->cdev);
	device_destroy(spcom_dev->driver_class, spcom_dev->device_no);
	class_destroy(spcom_dev->driver_class);

	mutex_lock(&spcom_dev->chdev_count_lock);
	unregister_chrdev_region(spcom_dev->device_no, spcom_dev->chdev_count);
	mutex_unlock(&spcom_dev->chdev_count_lock);

	spcom_pr_dbg("control spcom device removed\n");
}

static int spcom_parse_dt(struct device_node *np)
{
	int ret;
	const char *propname = "qcom,spcom-ch-names";
	int num_ch;
	int i;
	const char *name;

	/* Get predefined channels info */
	num_ch = of_property_count_strings(np, propname);
	if (num_ch < 0) {
		spcom_pr_err("wrong format of predefined channels definition [%d]\n",
		       num_ch);
		return num_ch;
	}
	if (num_ch > ARRAY_SIZE(spcom_dev->predefined_ch_name)) {
		spcom_pr_err("too many predefined channels [%d]\n", num_ch);
		return -EINVAL;
	}

	spcom_pr_dbg("num of predefined channels [%d]\n", num_ch);
	for (i = 0; i < num_ch; i++) {
		ret = of_property_read_string_index(np, propname, i, &name);
		if (ret) {
			spcom_pr_err("failed to read DT ch#%d name\n", i);
			return -EFAULT;
		}
		strlcpy(spcom_dev->predefined_ch_name[i],
			name,
			sizeof(spcom_dev->predefined_ch_name[i]));

		spcom_pr_dbg("found ch [%s]\n", name);
	}

	return num_ch;
}

/*
 * the function is running on system workqueue context,
 * processes delayed (by rpmsg rx callback) packets:
 * each packet belong to its destination spcom channel ch
 */
static void spcom_signal_rx_done(struct work_struct *ignored)
{
	struct spcom_channel *ch;
	struct rx_buff_list *rx_item;
	struct spcom_msg_hdr *hdr;
	unsigned long flags;

	spin_lock_irqsave(&spcom_dev->rx_lock, flags);
	while (!list_empty(&spcom_dev->rx_list_head)) {
		/* detach last entry */
		rx_item = list_last_entry(&spcom_dev->rx_list_head,
					  struct rx_buff_list, list);
		list_del(&rx_item->list);
		spin_unlock_irqrestore(&spcom_dev->rx_lock, flags);

		if (!rx_item) {
			spcom_pr_err("empty entry in pending rx list\n");
			spin_lock_irqsave(&spcom_dev->rx_lock, flags);
			continue;
		}
		ch = rx_item->ch;
		hdr = (struct spcom_msg_hdr *)rx_item->rpmsg_rx_buf;
		mutex_lock(&ch->lock);

		if (ch->comm_role_undefined) {
			ch->comm_role_undefined = false;
			ch->is_server = true;
			ch->txn_id = hdr->txn_id;
			spcom_pr_dbg("ch [%s] first packet txn_id=%d, it is server\n",
				 ch->name, ch->txn_id);
		}

		if (ch->rpmsg_abort) {
			if (ch->rpmsg_rx_buf) {
				spcom_pr_dbg("ch [%s] rx aborted free %zd bytes\n",
					ch->name, ch->actual_rx_size);
				kfree(ch->rpmsg_rx_buf);
				ch->actual_rx_size = 0;
			}
			goto rx_aborted;
		}
		if (ch->rpmsg_rx_buf) {
			spcom_pr_err("ch [%s] previous buffer not consumed %zd bytes\n",
			       ch->name, ch->actual_rx_size);
			kfree(ch->rpmsg_rx_buf);
			ch->rpmsg_rx_buf = NULL;
			ch->actual_rx_size = 0;
		}
		if (!ch->is_server && (hdr->txn_id != ch->txn_id)) {
			spcom_pr_err("ch [%s] client: rx dropped txn_id %d, ch->txn_id %d\n",
				ch->name, hdr->txn_id, ch->txn_id);
			goto rx_aborted;
		}
		spcom_pr_dbg("ch[%s] rx txn_id %d, ch->txn_id %d, size=%d\n",
			     ch->name, hdr->txn_id, ch->txn_id,
			     rx_item->rx_buf_size);
		ch->rpmsg_rx_buf = rx_item->rpmsg_rx_buf;
		ch->actual_rx_size = rx_item->rx_buf_size;
		ch->rx_buf_txn_id = ch->txn_id;
		complete_all(&ch->rx_done);
		mutex_unlock(&ch->lock);

		kfree(rx_item);

		/* lock for the next list entry */
		spin_lock_irqsave(&spcom_dev->rx_lock, flags);
	}
	spin_unlock_irqrestore(&spcom_dev->rx_lock, flags);
	return;
rx_aborted:
	mutex_unlock(&ch->lock);
	kfree(rx_item->rpmsg_rx_buf);
	kfree(rx_item);
}

static int spcom_rpdev_cb(struct rpmsg_device *rpdev,
			  void *data, int len, void *priv, u32 src)
{
	struct spcom_channel *ch;
	static DECLARE_WORK(rpmsg_rx_consumer, spcom_signal_rx_done);
	struct rx_buff_list *rx_item;
	unsigned long flags;

	if (!rpdev || !data) {
		spcom_pr_err("rpdev or data is NULL\n");
		return -EINVAL;
	}
	ch = dev_get_drvdata(&rpdev->dev);
	if (!ch) {
		spcom_pr_err("%s: invalid ch\n");
		return -EINVAL;
	}
	if (len > SPCOM_RX_BUF_SIZE || len <= 0) {
		spcom_pr_err("got msg size %d, max allowed %d\n",
		       len, SPCOM_RX_BUF_SIZE);
		return -EINVAL;
	}

	rx_item = kzalloc(sizeof(*rx_item), GFP_ATOMIC);
	if (!rx_item)
		return -ENOMEM;

	rx_item->rpmsg_rx_buf = kmemdup(data, len, GFP_ATOMIC);
	if (!rx_item->rpmsg_rx_buf)
		return -ENOMEM;

	rx_item->rx_buf_size = len;
	rx_item->ch = ch;

	pm_wakeup_ws_event(spcom_dev->ws, SPCOM_PM_PACKET_HANDLE_TIMEOUT, true);
	spcom_pr_dbg("%s:got new packet, wakeup requested\n", ch->name);


	spin_lock_irqsave(&spcom_dev->rx_lock, flags);
	list_add(&rx_item->list, &spcom_dev->rx_list_head);
	spin_unlock_irqrestore(&spcom_dev->rx_lock, flags);

	schedule_work(&rpmsg_rx_consumer);
	return 0;
}

static int spcom_rpdev_probe(struct rpmsg_device *rpdev)
{
	const char *name;
	struct spcom_channel *ch;

	if (!rpdev) {
		spcom_pr_err("rpdev is NULL\n");
		return -EINVAL;
	}

	name = rpdev->id.name;

	/* module exiting */
	if (atomic_read(&spcom_dev->remove_in_progress)) {
		spcom_pr_warn("remove in progress, ignore rpmsg probe for ch %s\n",
			name);
		return 0;
	}
	spcom_pr_dbg("new channel %s rpmsg_device arrived\n", name);
	ch = spcom_find_channel_by_name(name);
	if (!ch) {
		spcom_pr_err("channel %s not found\n", name);
		return -ENODEV;
	}
	mutex_lock(&ch->lock);
	ch->rpdev = rpdev;
	ch->rpmsg_abort = false;
	ch->txn_id = INITIAL_TXN_ID;
	complete_all(&ch->connect);
	mutex_unlock(&ch->lock);

	dev_set_drvdata(&rpdev->dev, ch);

	/* used to evaluate underlying transport link up/down */
	atomic_inc(&spcom_dev->rpmsg_dev_count);
	if (atomic_read(&spcom_dev->rpmsg_dev_count) == 1) {
		spcom_pr_info("Signal link up\n");
		complete_all(&spcom_dev->rpmsg_state_change);
	}

	return 0;
}

static void spcom_rpdev_remove(struct rpmsg_device *rpdev)
{
	struct spcom_channel *ch;
	int i;

	if (!rpdev) {
		spcom_pr_err("rpdev is NULL\n");
		return;
	}

	dev_info(&rpdev->dev, "rpmsg device %s removed\n", rpdev->id.name);
	ch = dev_get_drvdata(&rpdev->dev);
	if (!ch) {
		spcom_pr_err("channel %s not found\n", rpdev->id.name);
		return;
	}

	mutex_lock(&ch->lock);
	/* unlock all ion buffers of sp_kernel channel*/
	if (strcmp(ch->name, "sp_kernel") == 0) {
		for (i = 0; i < ARRAY_SIZE(ch->dmabuf_array); i++)
			if (ch->dmabuf_array[i].handle)
				spcom_dmabuf_unlock(&ch->dmabuf_array[i], false);
	}

	ch->rpdev = NULL;
	ch->rpmsg_abort = true;
	ch->txn_id = 0;
	complete_all(&ch->rx_done);
	mutex_unlock(&ch->lock);

	/* used to evaluate underlying transport link up/down */
	if (atomic_dec_and_test(&spcom_dev->rpmsg_dev_count)) {
		spcom_pr_err("Signal link down\n");
		complete_all(&spcom_dev->rpmsg_state_change);
	}
}

/* register rpmsg driver to match with channel ch_name */
static int spcom_register_rpmsg_drv(struct spcom_channel *ch)
{
	struct rpmsg_driver *rpdrv;
	struct rpmsg_device_id *match;
	char *drv_name;
	int ret;

	if (ch->rpdrv) {
		spcom_pr_err("ch:%s, rpmsg driver %s already registered\n",
			     ch->name, ch->rpdrv->id_table->name);
		return -ENODEV;
	}

	rpdrv = kzalloc(sizeof(*rpdrv), GFP_KERNEL);
	if (!rpdrv)
		return -ENOMEM;

	/* zalloc array of two to NULL terminate the match list */
	match = kzalloc(2 * sizeof(*match), GFP_KERNEL);
	if (!match) {
		kfree(rpdrv);
		return -ENOMEM;
	}
	snprintf(match->name, RPMSG_NAME_SIZE, "%s", ch->name);

	drv_name = kasprintf(GFP_KERNEL, "%s_%s", "spcom_rpmsg_drv", ch->name);
	if (!drv_name) {
		spcom_pr_err("can't allocate drv_name for %s\n", ch->name);
		kfree(rpdrv);
		kfree(match);
		return -ENOMEM;
	}

	rpdrv->probe = spcom_rpdev_probe;
	rpdrv->remove = spcom_rpdev_remove;
	rpdrv->callback = spcom_rpdev_cb;
	rpdrv->id_table = match;
	rpdrv->drv.name = drv_name;
	ret = register_rpmsg_driver(rpdrv);
	if (ret) {
		spcom_pr_err("can't register rpmsg_driver for %s\n", ch->name);
		kfree(rpdrv);
		kfree(match);
		kfree(drv_name);
		return ret;
	}
	mutex_lock(&ch->lock);
	ch->rpdrv = rpdrv;
	ch->rpmsg_abort = false;
	mutex_unlock(&ch->lock);

	return 0;
}

static int spcom_unregister_rpmsg_drv(struct spcom_channel *ch)
{
	if (!ch->rpdrv) {
		spcom_pr_err("rpdev is NULL, can't unregister rpmsg drv\n");
		return -ENODEV;
	}
	unregister_rpmsg_driver(ch->rpdrv);

	mutex_lock(&ch->lock);
	kfree(ch->rpdrv->drv.name);
	kfree((void *)ch->rpdrv->id_table);
	kfree(ch->rpdrv);
	ch->rpdrv = NULL;
	ch->rpmsg_abort = true; /* will unblock spcom_rx() */
	mutex_unlock(&ch->lock);
	return 0;
}

static int spcom_probe(struct platform_device *pdev)
{
	int ret;
	struct spcom_device *dev = NULL;
	struct device_node *np;
	struct property *prop;

	if (!pdev) {
		pr_err("invalid pdev\n");
		return -ENODEV;
	}

	np = pdev->dev.of_node;
	if (!np) {
		pr_err("invalid DT node\n");
		return -EINVAL;
	}

	prop = of_find_property(np, "qcom,rproc-handle", NULL);
	if (!prop) {
		spcom_pr_err("can't find qcom,rproc-hable property");
		return -EINVAL;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL)
		return -ENOMEM;

	spcom_dev = dev;
	spcom_dev->pdev = pdev;
	spcom_dev->rproc_prop = prop;

	/* start counting exposed channel char devices from 1 */
	spcom_dev->chdev_count = 1;
	mutex_init(&spcom_dev->chdev_count_lock);
	init_completion(&spcom_dev->rpmsg_state_change);
	atomic_set(&spcom_dev->rpmsg_dev_count, 0);
	atomic_set(&spcom_dev->remove_in_progress, 0);

	INIT_LIST_HEAD(&spcom_dev->rx_list_head);
	spin_lock_init(&spcom_dev->rx_lock);
	spcom_dev->nvm_ion_fd = -1;
	spcom_dev->rmb_error = 0;
	mutex_init(&spcom_dev->ch_list_lock);

	// register wakeup source
	spcom_dev->ws =
		wakeup_source_register(&spcom_dev->pdev->dev, "spcom_wakeup");
	if (!spcom_dev->ws)  {
		pr_err("failed to register wakeup source\n");
		ret = -ENOMEM;
		goto fail_while_chardev_reg;
	}

	ret = spcom_register_chardev();
	if (ret) {
		pr_err("create character device failed\n");
		goto fail_while_chardev_reg;
	}

	ret = spcom_parse_dt(np);
	if (ret < 0)
		goto fail_reg_chardev;

	if (of_property_read_bool(np, "qcom,boot-enabled"))
		atomic_set(&dev->subsys_req, 1);

	ret = spcom_create_predefined_channels_chardev();
	if (ret < 0) {
		pr_err("create character device failed (%d)\n", ret);
		goto fail_reg_chardev;
	}

	spcom_ipc_log_context = ipc_log_context_create(SPCOM_LOG_PAGE_CNT,
						       "spcom", 0);
	if (!spcom_ipc_log_context)
		pr_err("Unable to create IPC log context\n");

	spcom_pr_info("Driver Initialization completed ok\n");

	return 0;

fail_reg_chardev:
	pr_err("failed to init driver\n");
	spcom_unregister_chrdev();
fail_while_chardev_reg:
	kfree(dev);
	spcom_dev = NULL;

	return -ENODEV;
}

static int spcom_remove(struct platform_device *pdev)
{
	int ret;
	struct rx_buff_list *rx_item;
	unsigned long flags;
	int i;

	atomic_inc(&spcom_dev->remove_in_progress);

	if (spcom_dev->spss_rproc) {
		spcom_pr_info("shutdown spss\n");
		rproc_shutdown(spcom_dev->spss_rproc);
		spcom_dev->spss_rproc = NULL;
	}
	/* destroy existing channel char devices */
	for (i = 0; i < SPCOM_MAX_CHANNELS; i++) {
		const char *name = spcom_dev->channels[i].name;

		if (name[0] == 0)
			break;
		ret = spcom_destroy_channel_chardev(name);
		if (ret) {
			spcom_pr_err("failed to destroy chardev [%s], ret [%d]\n",
			       name, ret);
			return -EFAULT;
		}
		spcom_pr_dbg("destroyed channel %s", name);
	}

	/* destroy control char device */
	spcom_unregister_chrdev();

	/* release uncompleted rx */
	spin_lock_irqsave(&spcom_dev->rx_lock, flags);
	while (!list_empty(&spcom_dev->rx_list_head)) {
		/* detach last entry */
		rx_item = list_last_entry(&spcom_dev->rx_list_head,
					  struct rx_buff_list, list);
		list_del(&rx_item->list);
		if (!rx_item) {
			spcom_pr_err("empty entry in pending rx list\n");
			spin_lock_irqsave(&spcom_dev->rx_lock, flags);
			continue;
		}
		kfree(rx_item);
	}
	spin_unlock_irqrestore(&spcom_dev->rx_lock, flags);
	wakeup_source_unregister(spcom_dev->ws);

	if (spcom_ipc_log_context)
		ipc_log_context_destroy(spcom_ipc_log_context);

	/* free global device struct */
	kfree(spcom_dev);
	spcom_dev = NULL;
	pr_info("successfully released all module resources\n");

	return 0;
}

static void spcom_release_all_channels_of_process(u32 pid)
{
	u32 i;

	/* Iterate over all channels and release all channels that belong to
	 * the given process
	 */
	for (i = 0; i < SPCOM_MAX_CHANNELS; i++) {
		struct spcom_channel *ch = &spcom_dev->channels[i];

		if (ch->name[0] != '\0') {
			u32 j;

			/* Check if the given process is a client of the current channel, and
			 * if so release the channel
			 */
			for (j = 0; j < SPCOM_MAX_CHANNEL_CLIENTS; j++) {
				if (ch->pid[j] == pid) {
					mutex_lock(&ch->lock);
					spcom_channel_deinit_locked(ch, pid);
					mutex_unlock(&ch->lock);
					break;
				}
			}
		}
	}
}

static int spom_control_channel_add_client(u32 pid)
{
	u32 i;
	int  free_index;
	struct spcom_control_channel_info *ch_info;

	mutex_lock(&spcom_dev->ch_list_lock);

	for (i = 0, free_index = -1; i < SPCOM_MAX_CONTROL_CHANNELS; i++) {
		ch_info = &spcom_dev->control_channels[i];

		/* A process may open only a single control channel */
		if (ch_info->pid == pid) {
			ch_info->ref_cnt++;
			spcom_pr_dbg("Control channel for pid %u already exists, ref_cnt=%u\n",
					pid, ch_info->ref_cnt);
			mutex_unlock(&spcom_dev->ch_list_lock);
			return 0;
		}

		/* Remember the first free entry */
		if (free_index < 0 && ch_info->pid == 0)
			free_index = i;
	}

	/* If no free entry was found then the control channel can't be opened */
	if (free_index < 0) {
		mutex_unlock(&spcom_dev->ch_list_lock);
		spcom_pr_err("Too many open control channels\n");
		return -EMFILE;
	}

	/* Add the process opening the control channel in the free entry */
	ch_info = &spcom_dev->control_channels[free_index];
	ch_info->pid = pid;
	ch_info->ref_cnt = 1;

	spcom_pr_dbg("Add pid %u at index %u\n", pid, free_index);

	mutex_unlock(&spcom_dev->ch_list_lock);

	return 0;
}

static int spom_control_channel_remove_client(u32 pid)
{
	u32 i;
	int ret = -ESRCH;

	mutex_lock(&spcom_dev->ch_list_lock);

	for (i = 0; i < SPCOM_MAX_CONTROL_CHANNELS; i++) {
		struct spcom_control_channel_info *ch_info = &spcom_dev->control_channels[i];

		/* When a process closes the control channel we release all its channels
		 * to allow re-registration if another instance of the process will be created
		 */
		if (ch_info->pid == pid) {
			ch_info->ref_cnt--;
			spcom_pr_dbg("Remove pid %u from index %u, ref_cnt=%u\n",
					pid, i, ch_info->ref_cnt);
			if (ch_info->ref_cnt == 0) {
				ch_info->pid = 0;
				spcom_release_all_channels_of_process(pid);
			}
			ret = 0;
			break;
		}
	}

	mutex_unlock(&spcom_dev->ch_list_lock);

	return ret;
}

static const struct of_device_id spcom_match_table[] = {
	{ .compatible = "qcom,spcom", },
	{ },
};
MODULE_DEVICE_TABLE(of, spcom_match_table);

static struct platform_driver spcom_driver = {
	.probe = spcom_probe,
	.remove = spcom_remove,
	.driver = {
			.name = DEVICE_NAME,
			.of_match_table = of_match_ptr(spcom_match_table),
	},
};

module_platform_driver(spcom_driver);
MODULE_SOFTDEP("pre: spss_utils");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Secure Processor Communication");
