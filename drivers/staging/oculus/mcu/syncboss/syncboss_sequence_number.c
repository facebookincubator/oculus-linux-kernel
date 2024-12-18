// SPDX-License-Identifier: GPL-2.0
#include <linux/bitops.h>
#include <linux/list.h>

#include "syncboss_sequence_number.h"

int syncboss_sequence_number_client_create_locked(struct syncboss_seq *seq,
	struct syncboss_seq_client **client_seq, struct task_struct *task)
{
	struct syncboss_seq_client *data;

	data = devm_kzalloc(seq->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->task = task;

	*client_seq = data;

	return 0;
}

void syncboss_sequence_number_client_destroy_locked(
	struct syncboss_seq *seq, struct syncboss_seq_client *client_seq)
{
	bitmap_andnot(seq->allocated_seq_num, seq->allocated_seq_num,
		client_seq->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);

	devm_kfree(seq->dev, client_seq);
}

void syncboss_sequence_number_init(struct syncboss_seq *seq, struct device *dev)
{
	seq->dev = dev;
	seq->last_seq_num = SYNCBOSS_SEQ_NUM_MAX;
}

void syncboss_sequence_number_reset_locked(struct syncboss_seq *seq,
	struct list_head *client_data_list)
{
	if (!list_empty(client_data_list))
		dev_err(seq->dev, "resetting sequence numbers while clients exist");

	seq->last_seq_num = SYNCBOSS_SEQ_NUM_MAX;
	if (!bitmap_empty(seq->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS)) {
		dev_err(seq->dev, "resetting sequence numbers with non-zero bitmap");
		bitmap_zero(seq->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);
	}
	seq->seq_num_allocation_count = 0;
}

int syncboss_sequence_number_allocate_locked(struct syncboss_seq *seq,
	struct syncboss_seq_client *client_seq, uint8_t *seq_num)
{
	int next_seq = 0;
	int next_seq_avail = 0;

	/*
	 * Start searching at the sequence number following the last assigned rather
	 * than SYNCBOSS_SEQ_NUM_MIN so that we cycle through all of the possible
	 * sequence numbers rather than reusing the same few at the bottom end of
	 * the range. It is easier to debug transactions this way.
	 */
	next_seq = seq->last_seq_num + 1;
	if (next_seq > SYNCBOSS_SEQ_NUM_MAX)
		next_seq = SYNCBOSS_SEQ_NUM_MIN;

	next_seq_avail = find_next_zero_bit(seq->allocated_seq_num,
						SYNCBOSS_SEQ_NUM_BITS, /* size */
						next_seq /* offset */);
	if (next_seq_avail >= SYNCBOSS_SEQ_NUM_BITS) {
		/* Search only through bits we didn't already look through. */
		next_seq_avail = find_next_zero_bit(seq->allocated_seq_num,
							next_seq, /* size (end) */
							SYNCBOSS_SEQ_NUM_MIN /* offset (start) */);
		if (next_seq_avail >= next_seq) {
			dev_warn(seq->dev,
				"no sequence numbers available for %s (%d)",
				client_seq->task->comm, client_seq->task->pid);
			return -EAGAIN;
		}
	}
	next_seq = next_seq_avail;

	set_bit(next_seq, seq->allocated_seq_num);
	set_bit(next_seq, client_seq->allocated_seq_num);

	seq->seq_num_allocation_count++;
	client_seq->seq_num_allocation_count++;

	seq->last_seq_num = next_seq;

	*seq_num = next_seq;

	return 0;
}

int syncboss_sequence_number_release_locked(struct syncboss_seq *seq,
	struct syncboss_seq_client *client_seq, uint8_t seq_num)
{
	bool client_had_seq_num;

	if (seq_num < SYNCBOSS_SEQ_NUM_MIN || seq_num > SYNCBOSS_SEQ_NUM_MAX) {
		dev_err(seq->dev,
				"sequence number %d is out of range [%d, %d]", seq_num,
				SYNCBOSS_SEQ_NUM_MIN, SYNCBOSS_SEQ_NUM_MAX);
		return -EINVAL;
	}

	client_had_seq_num = test_and_clear_bit(seq_num, client_seq->allocated_seq_num);
	if (!client_had_seq_num) {
		dev_warn(seq->dev,
			"%s (%d) attempted to release a sequence number that was not allocated to them: %d",
			client_seq->task->comm, client_seq->task->pid, seq_num);
		return -EACCES;
	}

	clear_bit(seq_num, seq->allocated_seq_num);

	return 0;
}

void syncboss_sequence_number_release_client_locked(struct syncboss_seq *seq,
	struct syncboss_seq_client *client_seq)
{
	bitmap_andnot(seq->allocated_seq_num, seq->allocated_seq_num,
		client_seq->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);
}
