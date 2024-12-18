/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_SEQUENCE_NUMBER_H
#define _SYNCBOSS_SEQUENCE_NUMBER_H

#include <linux/bitmap.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/types.h>

/* Sequence number settings */
#define SYNCBOSS_SEQ_NUM_MIN 1
#define SYNCBOSS_SEQ_NUM_MAX 254
#define SYNCBOSS_SEQ_NUM_BITS (SYNCBOSS_SEQ_NUM_MAX + 1)

struct syncboss_seq_client {
	struct task_struct *task;
	u64 seq_num_allocation_count;
	/* Bitmap of allocated sequence numbers (ioctl) */
	DECLARE_BITMAP(allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);
};

struct syncboss_seq {
	struct device *dev;

	/* The last sequence number used for a control call (ioctl) */
	int last_seq_num;

	/* Bitmap of allocated sequence numbers (ioctl) */
	DECLARE_BITMAP(allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);

	u64 seq_num_allocation_count;
};

int syncboss_sequence_number_client_create_locked(struct syncboss_seq *seq,
	struct syncboss_seq_client **client_seq, struct task_struct *task);
void syncboss_sequence_number_client_destroy_locked(
	struct syncboss_seq *seq, struct syncboss_seq_client *client_seq);

void syncboss_sequence_number_init(struct syncboss_seq *seq, struct device *dev);
void syncboss_sequence_number_reset_locked(struct syncboss_seq *seq,
	struct list_head *client_data_list);
int syncboss_sequence_number_allocate_locked(struct syncboss_seq *seq,
	struct syncboss_seq_client *client_seq, uint8_t *seq_num);
int syncboss_sequence_number_release_locked(struct syncboss_seq *seq,
	struct syncboss_seq_client *client_seq, uint8_t seq_num);

#endif /* _SYNCBOSS_SEQUENCE_NUMBER_H */
