// SPDX-License-Identifier: GPL-2.0
#include <linux/bitops.h>
#include <linux/list.h>

#include "syncboss_spi_sequence_number.h"

void syncboss_sequence_number_reset_locked(struct syncboss_dev_data *devdata)
{
	struct device *dev = &devdata->spi->dev;

	if (!list_empty(&devdata->client_data_list))
		dev_err(dev, "resetting sequence numbers while clients exist");

	devdata->last_seq_num = SYNCBOSS_SEQ_NUM_MAX;
	if (!bitmap_empty(devdata->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS)) {
		dev_err(dev, "resetting sequence numbers with non-zero bitmap");
		bitmap_zero(devdata->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);
	}
	devdata->seq_num_allocation_count = 0;
}

int syncboss_sequence_number_allocate_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data, uint8_t *seq)
{
	struct device *dev = &devdata->spi->dev;
	int next_seq = 0;
	int next_seq_avail = 0;

	/*
	 * Start searching at the sequence number following the last assigned rather
	 * than SYNCBOSS_SEQ_NUM_MIN so that we cycle through all of the possible
	 * sequence numbers rather than reusing the same few at the bottom end of
	 * the range. It is easier to debug transactions this way.
	 */
	next_seq = devdata->last_seq_num + 1;
	if (next_seq > SYNCBOSS_SEQ_NUM_MAX)
		next_seq = SYNCBOSS_SEQ_NUM_MIN;

	next_seq_avail = find_next_zero_bit(devdata->allocated_seq_num,
						SYNCBOSS_SEQ_NUM_BITS, /* size */
						next_seq /* offset */);
	if (next_seq_avail >= SYNCBOSS_SEQ_NUM_BITS) {
		/* Search only through bits we didn't already look through. */
		next_seq_avail = find_next_zero_bit(devdata->allocated_seq_num,
							next_seq, /* size (end) */
							SYNCBOSS_SEQ_NUM_MIN /* offset (start) */);
		if (next_seq_avail >= next_seq) {
			dev_warn(dev,
				"no sequence numbers available for %s (%d)",
				client_data->task->comm, client_data->task->pid);
			return -EAGAIN;
		}
	}
	next_seq = next_seq_avail;

	set_bit(next_seq, devdata->allocated_seq_num);
	set_bit(next_seq, client_data->allocated_seq_num);

	devdata->seq_num_allocation_count++;
	client_data->seq_num_allocation_count++;

	devdata->last_seq_num = next_seq;

	*seq = next_seq;

	return 0;

}

int syncboss_sequence_number_release_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data, uint8_t seq)
{
	struct device *dev = &devdata->spi->dev;
	bool client_had_seq_num;

	if (seq < SYNCBOSS_SEQ_NUM_MIN || seq > SYNCBOSS_SEQ_NUM_MAX) {
		dev_err(&devdata->spi->dev,
				"sequence number %d is out of range [%d, %d]", seq,
				SYNCBOSS_SEQ_NUM_MIN, SYNCBOSS_SEQ_NUM_MAX);
		return -EINVAL;
	}

	client_had_seq_num = test_and_clear_bit(seq, client_data->allocated_seq_num);
	if (!client_had_seq_num) {
		dev_warn(dev,
			"%s (%d) attempted to release a sequence number that was not allocated to them: %d",
			client_data->task->comm, client_data->task->pid, seq);
		return -EACCES;
	}

	clear_bit(seq, devdata->allocated_seq_num);

	return 0;
}

void syncboss_sequence_number_release_client_locked(
	struct syncboss_dev_data *devdata, struct syncboss_client_data *client_data)
{
	bitmap_andnot(devdata->allocated_seq_num, devdata->allocated_seq_num,
		client_data->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);
}
