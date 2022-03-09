/*
 * Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SEEMP_LOGK_H__
#define __SEEMP_LOGK_H__

#define OBSERVER_VERSION 0x01

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <asm/ioctls.h>

#define seemp_LOGK_NUM_DEVS 1
#define seemp_LOGK_DEV_NAME "seemplog"

/*
 * The logcat driver on Android uses four 256k ring buffers
 * here, we use two ring buffers of the same size.
 * we think this is reasonable
 */
#define FULL_BUF_SIZE (64 * 1024 * 1024)
#define HALF_BUF_SIZE (32 * 1024 * 1024)
#define FULL_BLOCKS (8 * 1024)
#define HALF_BLOCKS (4 * 1024)

#define READER_NOT_READY 0
#define READER_READY 1

#define MAGIC 'z'

#define SEEMP_CMD_RESERVE_RDBLKS     _IOR(MAGIC, 1, int)
#define SEEMP_CMD_RELEASE_RDBLKS     _IO(MAGIC, 2)
#define SEEMP_CMD_GET_RINGSZ     _IOR(MAGIC, 3, int)
#define SEEMP_CMD_GET_BLKSZ     _IOR(MAGIC, 4, int)
#define SEEMP_CMD_SET_MASK          _IO(MAGIC, 5)
#define SEEMP_CMD_SET_MAPPING       _IO(MAGIC, 6)
#define SEEMP_CMD_CHECK_FILTER      _IOR(MAGIC, 7, int)

struct read_range {
	int start_idx;
	int num;
};

struct seemp_logk_dev {
	unsigned int major;
	unsigned int minor;

	struct cdev cdev;
	struct class *cls;
	/*the full buffer*/
	struct seemp_logk_blk *ring;
	/*an array of blks*/
	unsigned int ring_sz;
	unsigned int blk_sz;

	int num_tot_blks;

	int num_write_avail_blks;
	int num_write_in_prog_blks;

	int num_read_avail_blks;
	int num_read_in_prog_blks;

	int num_writers;

	/*
	 * there is always one reader
	 * which is the observer daemon
	 * therefore there is no necessity
	 * for num_readers variable
	 */

	/*
	 * read_idx  and write_idx loop through from zero to ring_sz,
	 * and then back to zero in a circle, as they advance
	 * based on the reader's and writers' accesses
	 */
	int read_idx;

	int write_idx;

	/*
	 * wait queues
	 * readers_wq: implement wait for readers
	 * writers_wq: implement wait for writers
	 *
	 * whether writers are blocked or not is driven by the policy:
	 * case 1: (best_effort_logging == 1)
	 *         writers are not blocked, and
	 *         when there is no mem in the ring to store logs,
	 *         the logs are simply dropped.
	 * case 2: (best_effort_logging == 0)
	 *         when there is no mem in the ring to store logs,
	 *         the process gets blocked until there is space.
	 */
	wait_queue_head_t readers_wq;
	wait_queue_head_t writers_wq;

	/*
	 * protects everything in the device
	 * including ring buffer and all the num_ variables
	 * spinlock_t lock;
	 */
	struct mutex lock;
};

#define BLK_SIZE       256
#define BLK_HDR_SIZE   64
#define TS_SIZE        20
#define BLK_MAX_MSG_SZ (BLK_SIZE - BLK_HDR_SIZE)

struct blk_payload {
	__u32 api_id;  /* event API id */
	char  msg[BLK_MAX_MSG_SZ]; /* event parameters */
} __packed;

struct seemp_logk_blk {
	__u8  status;  /* bits: 0->valid/invalid; 1-7: unused as of now! */
	__u16 len;     /* length of the payload */
	__u8  version; /* version number */
	__s32 pid;     /* generating process's pid */
	__s32 uid;     /* generating process's uid - app specific */
	__s32 tid;     /* generating process's tid */
	__s32 sec;     /* seconds since Epoch */
	__s32 nsec;    /* nanoseconds */
	char        ts[TS_SIZE];  /* Time Stamp */
	char        appname[TASK_COMM_LEN];
	struct blk_payload payload;
} __packed;


extern unsigned int kmalloc_flag;

struct seemp_source_mask {
	__u32       hash;
	bool        isOn;
};

/* report region header */
struct el2_report_header_t {
	__u64 report_version;     /* Version of the EL2 report */
	__u64 mp_catalog_version;
		/* Version of MP catalogue used for kernel protection */
	__u64 num_incidents;      /* Number of Incidents Observed by EL2 */
	__u8 protection_enabled;  /* Kernel Assets protected by EL2 */
	__u8 pad1;
	__u8 pad2;
	__u8 pad3;
	__u32 pad4;
};

/* individual report */
struct el2_report_data_t {
	__u64 sequence_number; /* Sequence number of the report */
	__u64 actor; /* Actor that caused the Incident.  */
	__u8 report_valid;
		/* Flag to indicate whether report instance is valid */
	__u8 report_type;        /* Report Type */
	__u8 asset_category; /* Asset Category */
	__u8 response;       /* Response From EL2 */
};

#endif
