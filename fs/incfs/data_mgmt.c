// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */
#include <linux/crc32.h>
#include <linux/file.h>
#include <linux/gfp.h>
#include <linux/ktime.h>
#include <linux/lz4.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "data_mgmt.h"
#include "format.h"
#include "integrity.h"

static void log_wake_up_all(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct read_log *rl = container_of(dw, struct read_log, ml_wakeup_work);
	wake_up_all(&rl->ml_notif_wq);
}

struct mount_info *incfs_alloc_mount_info(struct super_block *sb,
					  struct mount_options *options,
					  struct path *backing_dir_path)
{
	struct mount_info *mi = NULL;
	int error = 0;

	mi = kzalloc(sizeof(*mi), GFP_NOFS);
	if (!mi)
		return ERR_PTR(-ENOMEM);

	mi->mi_sb = sb;
	mi->mi_backing_dir_path = *backing_dir_path;
	mi->mi_owner = get_current_cred();
	path_get(&mi->mi_backing_dir_path);
	mutex_init(&mi->mi_dir_struct_mutex);
	mutex_init(&mi->mi_pending_reads_mutex);
	init_waitqueue_head(&mi->mi_pending_reads_notif_wq);
	init_waitqueue_head(&mi->mi_log.ml_notif_wq);
	INIT_DELAYED_WORK(&mi->mi_log.ml_wakeup_work, log_wake_up_all);
	spin_lock_init(&mi->mi_log.rl_lock);
	INIT_LIST_HEAD(&mi->mi_reads_list_head);

	error = incfs_realloc_mount_info(mi, options);
	if (error)
		goto err;

	return mi;

err:
	incfs_free_mount_info(mi);
	return ERR_PTR(error);
}

int incfs_realloc_mount_info(struct mount_info *mi,
			     struct mount_options *options)
{
	void *new_buffer = NULL;
	void *old_buffer;
	size_t new_buffer_size = 0;

	if (options->read_log_pages != mi->mi_options.read_log_pages) {
		struct read_log_state log_state;
		/*
		 * Even though having two buffers allocated at once isn't
		 * usually good, allocating a multipage buffer under a spinlock
		 * is even worse, so let's optimize for the shorter lock
		 * duration. It's not end of the world if we fail to increase
		 * the buffer size anyway.
		 */
		if (options->read_log_pages > 0) {
			new_buffer_size = PAGE_SIZE * options->read_log_pages;
			new_buffer = kzalloc(new_buffer_size, GFP_NOFS);
			if (!new_buffer)
				return -ENOMEM;
		}

		spin_lock(&mi->mi_log.rl_lock);
		old_buffer = mi->mi_log.rl_ring_buf;
		mi->mi_log.rl_ring_buf = new_buffer;
		mi->mi_log.rl_size = new_buffer_size;
		log_state = (struct read_log_state){
			.generation_id = mi->mi_log.rl_head.generation_id + 1,
		};
		mi->mi_log.rl_head = log_state;
		mi->mi_log.rl_tail = log_state;
		spin_unlock(&mi->mi_log.rl_lock);

		kfree(old_buffer);
	}

	mi->mi_options = *options;
	return 0;
}

void incfs_free_mount_info(struct mount_info *mi)
{
	if (!mi)
		return;

	flush_delayed_work(&mi->mi_log.ml_wakeup_work);

	dput(mi->mi_index_dir);
	path_put(&mi->mi_backing_dir_path);
	mutex_destroy(&mi->mi_dir_struct_mutex);
	mutex_destroy(&mi->mi_pending_reads_mutex);
	put_cred(mi->mi_owner);
	kfree(mi->mi_log.rl_ring_buf);
	kfree(mi->log_xattr);
	kfree(mi->pending_read_xattr);
	kfree(mi);
}

static void data_file_segment_init(struct data_file_segment *segment)
{
	init_waitqueue_head(&segment->new_data_arrival_wq);
	mutex_init(&segment->blockmap_mutex);
	INIT_LIST_HEAD(&segment->reads_list_head);
}

static void data_file_segment_destroy(struct data_file_segment *segment)
{
	mutex_destroy(&segment->blockmap_mutex);
}

struct data_file *incfs_open_data_file(struct mount_info *mi, struct file *bf)
{
	struct data_file *df = NULL;
	struct backing_file_context *bfc = NULL;
	int md_records;
	u64 size;
	int error = 0;
	int i;

	if (!bf || !mi)
		return ERR_PTR(-EFAULT);

	if (!S_ISREG(bf->f_inode->i_mode))
		return ERR_PTR(-EBADF);

	bfc = incfs_alloc_bfc(mi, bf);
	if (IS_ERR(bfc))
		return ERR_CAST(bfc);

	df = kzalloc(sizeof(*df), GFP_NOFS);
	if (!df) {
		error = -ENOMEM;
		goto out;
	}

	df->df_backing_file_context = bfc;
	df->df_mount_info = mi;
	for (i = 0; i < ARRAY_SIZE(df->df_segments); i++)
		data_file_segment_init(&df->df_segments[i]);

	error = mutex_lock_interruptible(&bfc->bc_mutex);
	if (error)
		goto out;
	error = incfs_read_file_header(bfc, &df->df_metadata_off, &df->df_id,
				       &size, &df->df_header_flags);
	mutex_unlock(&bfc->bc_mutex);

	if (error)
		goto out;

	df->df_size = size;
	if (size > 0)
		df->df_data_block_count = get_blocks_count_for_size(size);

	md_records = incfs_scan_metadata_chain(df);
	if (md_records < 0)
		error = md_records;

out:
	if (error) {
		incfs_free_bfc(bfc);
		if (df)
			df->df_backing_file_context = NULL;
		incfs_free_data_file(df);
		return ERR_PTR(error);
	}
	return df;
}

void incfs_free_data_file(struct data_file *df)
{
	int i;

	if (!df)
		return;

	incfs_free_mtree(df->df_hash_tree);
	for (i = 0; i < ARRAY_SIZE(df->df_segments); i++)
		data_file_segment_destroy(&df->df_segments[i]);
	incfs_free_bfc(df->df_backing_file_context);
	kfree(df->df_signature);
	kfree(df);
}

int make_inode_ready_for_data_ops(struct mount_info *mi,
				struct inode *inode,
				struct file *backing_file)
{
	struct inode_info *node = get_incfs_node(inode);
	struct data_file *df = NULL;
	int err = 0;

	inode_lock(inode);
	if (S_ISREG(inode->i_mode)) {
		if (!node->n_file) {
			df = incfs_open_data_file(mi, backing_file);

			if (IS_ERR(df))
				err = PTR_ERR(df);
			else
				node->n_file = df;
		}
	} else
		err = -EBADF;
	inode_unlock(inode);
	return err;
}

struct dir_file *incfs_open_dir_file(struct mount_info *mi, struct file *bf)
{
	struct dir_file *dir = NULL;

	if (!S_ISDIR(bf->f_inode->i_mode))
		return ERR_PTR(-EBADF);

	dir = kzalloc(sizeof(*dir), GFP_NOFS);
	if (!dir)
		return ERR_PTR(-ENOMEM);

	dir->backing_dir = get_file(bf);
	dir->mount_info = mi;
	return dir;
}

void incfs_free_dir_file(struct dir_file *dir)
{
	if (!dir)
		return;
	if (dir->backing_dir)
		fput(dir->backing_dir);
	kfree(dir);
}

static ssize_t decompress(struct mem_range src, struct mem_range dst)
{
	int result = LZ4_decompress_safe(src.data, dst.data, src.len, dst.len);

	if (result < 0)
		return -EBADMSG;

	return result;
}

static void log_read_one_record(struct read_log *rl, struct read_log_state *rs)
{
	union log_record *record =
		(union log_record *)((u8 *)rl->rl_ring_buf + rs->next_offset);
	size_t record_size;

	switch (record->full_record.type) {
	case FULL:
		rs->base_record = record->full_record;
		record_size = sizeof(record->full_record);
		break;

	case SAME_FILE:
		rs->base_record.block_index =
			record->same_file_record.block_index;
		rs->base_record.absolute_ts_us +=
			record->same_file_record.relative_ts_us;
		record_size = sizeof(record->same_file_record);
		break;

	case SAME_FILE_NEXT_BLOCK:
		++rs->base_record.block_index;
		rs->base_record.absolute_ts_us +=
			record->same_file_next_block.relative_ts_us;
		record_size = sizeof(record->same_file_next_block);
		break;

	case SAME_FILE_NEXT_BLOCK_SHORT:
		++rs->base_record.block_index;
		rs->base_record.absolute_ts_us +=
			record->same_file_next_block_short.relative_ts_us;
		record_size = sizeof(record->same_file_next_block_short);
		break;
	}

	rs->next_offset += record_size;
	if (rs->next_offset > rl->rl_size - sizeof(*record)) {
		rs->next_offset = 0;
		++rs->current_pass_no;
	}
	++rs->current_record_no;
}

static void log_block_read(struct mount_info *mi, incfs_uuid_t *id,
			   int block_index)
{
	struct read_log *log = &mi->mi_log;
	struct read_log_state *head, *tail;
	s64 now_us;
	s64 relative_us;
	union log_record record;
	size_t record_size;

	/*
	 * This may read the old value, but it's OK to delay the logging start
	 * right after the configuration update.
	 */
	if (READ_ONCE(log->rl_size) == 0)
		return;

	now_us = ktime_to_us(ktime_get());

	spin_lock(&log->rl_lock);
	if (log->rl_size == 0) {
		spin_unlock(&log->rl_lock);
		return;
	}

	head = &log->rl_head;
	tail = &log->rl_tail;
	relative_us = now_us - head->base_record.absolute_ts_us;

	if (memcmp(id, &head->base_record.file_id, sizeof(incfs_uuid_t)) ||
	    relative_us >= 1ll << 32) {
		record.full_record = (struct full_record){
			.type = FULL,
			.block_index = block_index,
			.file_id = *id,
			.absolute_ts_us = now_us,
		};
		head->base_record.file_id = *id;
		record_size = sizeof(struct full_record);
	} else if (block_index != head->base_record.block_index + 1 ||
		   relative_us >= 1 << 30) {
		record.same_file_record = (struct same_file_record){
			.type = SAME_FILE,
			.block_index = block_index,
			.relative_ts_us = relative_us,
		};
		record_size = sizeof(struct same_file_record);
	} else if (relative_us >= 1 << 14) {
		record.same_file_next_block = (struct same_file_next_block){
			.type = SAME_FILE_NEXT_BLOCK,
			.relative_ts_us = relative_us,
		};
		record_size = sizeof(struct same_file_next_block);
	} else {
		record.same_file_next_block_short =
			(struct same_file_next_block_short){
				.type = SAME_FILE_NEXT_BLOCK_SHORT,
				.relative_ts_us = relative_us,
			};
		record_size = sizeof(struct same_file_next_block_short);
	}

	head->base_record.block_index = block_index;
	head->base_record.absolute_ts_us = now_us;

	/* Advance tail beyond area we are going to overwrite */
	while (tail->current_pass_no < head->current_pass_no &&
	       tail->next_offset < head->next_offset + record_size)
		log_read_one_record(log, tail);

	memcpy(((u8 *)log->rl_ring_buf) + head->next_offset, &record,
	       record_size);
	head->next_offset += record_size;
	if (head->next_offset > log->rl_size - sizeof(record)) {
		head->next_offset = 0;
		++head->current_pass_no;
	}
	++head->current_record_no;

	spin_unlock(&log->rl_lock);
	schedule_delayed_work(&log->ml_wakeup_work, msecs_to_jiffies(16));
}

static int validate_hash_tree(struct backing_file_context *bfc, struct file *f,
			      int block_index, struct mem_range data, u8 *buf)
{
	struct data_file *df = get_incfs_data_file(f);
	u8 stored_digest[INCFS_MAX_HASH_SIZE] = {};
	u8 calculated_digest[INCFS_MAX_HASH_SIZE] = {};
	struct mtree *tree = NULL;
	struct incfs_df_signature *sig = NULL;
	int digest_size;
	int hash_block_index = block_index;
	int lvl;
	int res;
	loff_t hash_block_offset[INCFS_MAX_MTREE_LEVELS];
	size_t hash_offset_in_block[INCFS_MAX_MTREE_LEVELS];
	int hash_per_block;
	pgoff_t file_pages;

	tree = df->df_hash_tree;
	sig = df->df_signature;
	if (!tree || !sig)
		return 0;

	digest_size = tree->alg->digest_size;
	hash_per_block = INCFS_DATA_FILE_BLOCK_SIZE / digest_size;
	for (lvl = 0; lvl < tree->depth; lvl++) {
		loff_t lvl_off = tree->hash_level_suboffset[lvl];

		hash_block_offset[lvl] =
			lvl_off + round_down(hash_block_index * digest_size,
					     INCFS_DATA_FILE_BLOCK_SIZE);
		hash_offset_in_block[lvl] = hash_block_index * digest_size %
					    INCFS_DATA_FILE_BLOCK_SIZE;
		hash_block_index /= hash_per_block;
	}

	memcpy(stored_digest, tree->root_hash, digest_size);

	file_pages = DIV_ROUND_UP(df->df_size, INCFS_DATA_FILE_BLOCK_SIZE);
	for (lvl = tree->depth - 1; lvl >= 0; lvl--) {
		pgoff_t hash_page =
			file_pages +
			hash_block_offset[lvl] / INCFS_DATA_FILE_BLOCK_SIZE;
		struct page *page = find_get_page_flags(
			f->f_inode->i_mapping, hash_page, FGP_ACCESSED);

		if (page && PageChecked(page)) {
			u8 *addr = kmap_atomic(page);

			memcpy(stored_digest, addr + hash_offset_in_block[lvl],
			       digest_size);
			kunmap_atomic(addr);
			put_page(page);
			continue;
		}

		if (page)
			put_page(page);

		res = incfs_kread(bfc, buf, INCFS_DATA_FILE_BLOCK_SIZE,
				  hash_block_offset[lvl] + sig->hash_offset);
		if (res < 0)
			return res;
		if (res != INCFS_DATA_FILE_BLOCK_SIZE)
			return -EIO;
		res = incfs_calc_digest(tree->alg,
					range(buf, INCFS_DATA_FILE_BLOCK_SIZE),
					range(calculated_digest, digest_size));
		if (res)
			return res;

		if (memcmp(stored_digest, calculated_digest, digest_size)) {
			int i;
			bool zero = true;

			pr_debug("incfs: Hash mismatch lvl:%d blk:%d\n",
				lvl, block_index);
			for (i = 0; i < digest_size; i++)
				if (stored_digest[i]) {
					zero = false;
					break;
				}

			if (zero)
				pr_debug("incfs: Note saved_digest all zero - did you forget to load the hashes?\n");
			return -EBADMSG;
		}

		memcpy(stored_digest, buf + hash_offset_in_block[lvl],
		       digest_size);

		page = grab_cache_page(f->f_inode->i_mapping, hash_page);
		if (page) {
			u8 *addr = kmap_atomic(page);

			memcpy(addr, buf, INCFS_DATA_FILE_BLOCK_SIZE);
			kunmap_atomic(addr);
			SetPageChecked(page);
			unlock_page(page);
			put_page(page);
		}
	}

	res = incfs_calc_digest(tree->alg, data,
				range(calculated_digest, digest_size));
	if (res)
		return res;

	if (memcmp(stored_digest, calculated_digest, digest_size)) {
		pr_debug("incfs: Leaf hash mismatch blk:%d\n", block_index);
		return -EBADMSG;
	}

	return 0;
}

static struct data_file_segment *get_file_segment(struct data_file *df,
						  int block_index)
{
	int seg_idx = block_index % ARRAY_SIZE(df->df_segments);

	return &df->df_segments[seg_idx];
}

static bool is_data_block_present(struct data_file_block *block)
{
	return (block->db_backing_file_data_offset != 0) &&
	       (block->db_stored_size != 0);
}

static void convert_data_file_block(struct incfs_blockmap_entry *bme,
				    struct data_file_block *res_block)
{
	u16 flags = le16_to_cpu(bme->me_flags);

	res_block->db_backing_file_data_offset =
		le16_to_cpu(bme->me_data_offset_hi);
	res_block->db_backing_file_data_offset <<= 32;
	res_block->db_backing_file_data_offset |=
		le32_to_cpu(bme->me_data_offset_lo);
	res_block->db_stored_size = le16_to_cpu(bme->me_data_size);
	res_block->db_comp_alg = (flags & INCFS_BLOCK_COMPRESSED_LZ4) ?
					 COMPRESSION_LZ4 :
					 COMPRESSION_NONE;
}

static int get_data_file_block(struct data_file *df, int index,
			       struct data_file_block *res_block)
{
	struct incfs_blockmap_entry bme = {};
	struct backing_file_context *bfc = NULL;
	loff_t blockmap_off = 0;
	int error = 0;

	if (!df || !res_block)
		return -EFAULT;

	blockmap_off = df->df_blockmap_off;
	bfc = df->df_backing_file_context;

	if (index < 0 || blockmap_off == 0)
		return -EINVAL;

	error = incfs_read_blockmap_entry(bfc, index, blockmap_off, &bme);
	if (error)
		return error;

	convert_data_file_block(&bme, res_block);
	return 0;
}

static int check_room_for_one_range(u32 size, u32 size_out)
{
	if (size_out + sizeof(struct incfs_filled_range) > size)
		return -ERANGE;
	return 0;
}

static int copy_one_range(struct incfs_filled_range *range, void __user *buffer,
			  u32 size, u32 *size_out)
{
	int error = check_room_for_one_range(size, *size_out);
	if (error)
		return error;

	if (copy_to_user(((char __user *)buffer) + *size_out, range,
				sizeof(*range)))
		return -EFAULT;

	*size_out += sizeof(*range);
	return 0;
}

static int update_file_header_flags(struct data_file *df, u32 bits_to_reset,
				    u32 bits_to_set)
{
	int result;
	u32 new_flags;
	struct backing_file_context *bfc;

	if (!df)
		return -EFAULT;
	bfc = df->df_backing_file_context;
	if (!bfc)
		return -EFAULT;

	result = mutex_lock_interruptible(&bfc->bc_mutex);
	if (result)
		return result;

	new_flags = (df->df_header_flags & ~bits_to_reset) | bits_to_set;
	if (new_flags != df->df_header_flags) {
		df->df_header_flags = new_flags;
		result = incfs_write_file_header_flags(bfc, new_flags);
	}

	mutex_unlock(&bfc->bc_mutex);

	return result;
}

#define READ_BLOCKMAP_ENTRIES 512
int incfs_get_filled_blocks(struct data_file *df,
			    struct incfs_get_filled_blocks_args *arg)
{
	int error = 0;
	bool in_range = false;
	struct incfs_filled_range range;
	void __user *buffer = u64_to_user_ptr(arg->range_buffer);
	u32 size = arg->range_buffer_size;
	u32 end_index =
		arg->end_index ? arg->end_index : df->df_total_block_count;
	u32 *size_out = &arg->range_buffer_size_out;
	int i = READ_BLOCKMAP_ENTRIES - 1;
	int entries_read = 0;
	struct incfs_blockmap_entry *bme;

	*size_out = 0;
	if (end_index > df->df_total_block_count)
		end_index = df->df_total_block_count;
	arg->total_blocks_out = df->df_total_block_count;
	arg->data_blocks_out = df->df_data_block_count;

	if (df->df_header_flags & INCFS_FILE_COMPLETE) {
		pr_debug("File marked full, fast get_filled_blocks");
		if (arg->start_index > end_index) {
			arg->index_out = arg->start_index;
			return 0;
		}
		arg->index_out = arg->start_index;

		error = check_room_for_one_range(size, *size_out);
		if (error)
			return error;

		range = (struct incfs_filled_range){
			.begin = arg->start_index,
			.end = end_index,
		};

		error = copy_one_range(&range, buffer, size, size_out);
		if (error)
			return error;
		arg->index_out = end_index;
		return 0;
	}

	bme = kzalloc(sizeof(*bme) * READ_BLOCKMAP_ENTRIES,
		      GFP_NOFS | __GFP_COMP);
	if (!bme)
		return -ENOMEM;

	for (arg->index_out = arg->start_index; arg->index_out < end_index;
	     ++arg->index_out) {
		struct data_file_block dfb;

		if (++i == READ_BLOCKMAP_ENTRIES) {
			entries_read = incfs_read_blockmap_entries(
				df->df_backing_file_context, bme,
				arg->index_out, READ_BLOCKMAP_ENTRIES,
				df->df_blockmap_off);
			if (entries_read < 0) {
				error = entries_read;
				break;
			}

			i = 0;
		}

		if (i >= entries_read) {
			error = -EIO;
			break;
		}

		convert_data_file_block(bme + i, &dfb);

		if (is_data_block_present(&dfb) == in_range)
			continue;

		if (!in_range) {
			error = check_room_for_one_range(size, *size_out);
			if (error)
				break;
			in_range = true;
			range.begin = arg->index_out;
		} else {
			range.end = arg->index_out;
			error = copy_one_range(&range, buffer, size, size_out);
			if (error) {
				/* there will be another try out of the loop,
				 * it will reset the index_out if it fails too
				 */
				break;
			}
			in_range = false;
		}
	}

	if (in_range) {
		range.end = arg->index_out;
		error = copy_one_range(&range, buffer, size, size_out);
		if (error)
			arg->index_out = range.begin;
	}

	if (!error && in_range && arg->start_index == 0 &&
	    end_index == df->df_total_block_count &&
	    *size_out == sizeof(struct incfs_filled_range)) {
		int result =
			update_file_header_flags(df, 0, INCFS_FILE_COMPLETE);
		/* Log failure only, since it's just a failed optimization */
		pr_debug("Marked file full with result %d", result);
	}

	kfree(bme);
	return error;
}

static bool is_read_done(struct pending_read *read)
{
	return atomic_read_acquire(&read->done) != 0;
}

static void set_read_done(struct pending_read *read)
{
	atomic_set_release(&read->done, 1);
}

/*
 * Notifies a given data file about pending read from a given block.
 * Returns a new pending read entry.
 */
static struct pending_read *add_pending_read(struct data_file *df,
					     int block_index)
{
	struct pending_read *result = NULL;
	struct data_file_segment *segment = NULL;
	struct mount_info *mi = NULL;

	segment = get_file_segment(df, block_index);
	mi = df->df_mount_info;

	result = kzalloc(sizeof(*result), GFP_NOFS);
	if (!result)
		return NULL;

	result->file_id = df->df_id;
	result->block_index = block_index;
	result->timestamp_us = ktime_to_us(ktime_get());

	mutex_lock(&mi->mi_pending_reads_mutex);

	result->serial_number = ++mi->mi_last_pending_read_number;
	mi->mi_pending_reads_count++;

	list_add(&result->mi_reads_list, &mi->mi_reads_list_head);
	list_add(&result->segment_reads_list, &segment->reads_list_head);
	mutex_unlock(&mi->mi_pending_reads_mutex);

	wake_up_all(&mi->mi_pending_reads_notif_wq);
	return result;
}

/* Notifies a given data file that pending read is completed. */
static void remove_pending_read(struct data_file *df, struct pending_read *read)
{
	struct mount_info *mi = NULL;

	if (!df || !read) {
		WARN_ON(!df);
		WARN_ON(!read);
		return;
	}

	mi = df->df_mount_info;

	mutex_lock(&mi->mi_pending_reads_mutex);
	list_del(&read->mi_reads_list);
	list_del(&read->segment_reads_list);

	mi->mi_pending_reads_count--;
	mutex_unlock(&mi->mi_pending_reads_mutex);

	kfree(read);
}

static void notify_pending_reads(struct mount_info *mi,
		struct data_file_segment *segment,
		int index)
{
	struct pending_read *entry = NULL;

	/* Notify pending reads waiting for this block. */
	mutex_lock(&mi->mi_pending_reads_mutex);
	list_for_each_entry(entry, &segment->reads_list_head,
						segment_reads_list) {
		if (entry->block_index == index)
			set_read_done(entry);
	}
	mutex_unlock(&mi->mi_pending_reads_mutex);
	wake_up_all(&segment->new_data_arrival_wq);
}

static int wait_for_data_block(struct data_file *df, int block_index,
			       int timeout_ms,
			       struct data_file_block *res_block)
{
	struct data_file_block block = {};
	struct data_file_segment *segment = NULL;
	struct pending_read *read = NULL;
	struct mount_info *mi = NULL;
	int error = 0;
	int wait_res = 0;

	if (!df || !res_block)
		return -EFAULT;

	if (block_index < 0 || block_index >= df->df_data_block_count)
		return -EINVAL;

	if (df->df_blockmap_off <= 0)
		return -ENODATA;

	segment = get_file_segment(df, block_index);
	error = mutex_lock_interruptible(&segment->blockmap_mutex);
	if (error)
		return error;

	/* Look up the given block */
	error = get_data_file_block(df, block_index, &block);

	/* If it's not found, create a pending read */
	if (!error && !is_data_block_present(&block) && timeout_ms != 0)
		read = add_pending_read(df, block_index);

	mutex_unlock(&segment->blockmap_mutex);
	if (error)
		return error;

	/* If the block was found, just return it. No need to wait. */
	if (is_data_block_present(&block)) {
		*res_block = block;
		return 0;
	}

	mi = df->df_mount_info;

	if (timeout_ms == 0) {
		log_block_read(mi, &df->df_id, block_index);
		return -ETIME;
	}

	if (!read)
		return -ENOMEM;

	/* Wait for notifications about block's arrival */
	wait_res =
		wait_event_interruptible_timeout(segment->new_data_arrival_wq,
						 (is_read_done(read)),
						 msecs_to_jiffies(timeout_ms));

	/* Woke up, the pending read is no longer needed. */
	remove_pending_read(df, read);
	read = NULL;

	if (wait_res == 0) {
		/* Wait has timed out */
		log_block_read(mi, &df->df_id, block_index);
		return -ETIME;
	}
	if (wait_res < 0) {
		/*
		 * Only ERESTARTSYS is really expected here when a signal
		 * comes while we wait.
		 */
		return wait_res;
	}

	error = mutex_lock_interruptible(&segment->blockmap_mutex);
	if (error)
		return error;

	/*
	 * Re-read block's info now, it has just arrived and
	 * should be available.
	 */
	error = get_data_file_block(df, block_index, &block);
	if (!error) {
		if (is_data_block_present(&block))
			*res_block = block;
		else {
			/*
			 * Somehow wait finished successfully bug block still
			 * can't be found. It's not normal.
			 */
			pr_warn("incfs:Wait succeeded, but block not found.\n");
			error = -ENODATA;
		}
	}

	mutex_unlock(&segment->blockmap_mutex);
	return error;
}

ssize_t incfs_read_data_file_block(struct mem_range dst, struct file *f,
				   int index, int timeout_ms,
				   struct mem_range tmp)
{
	loff_t pos;
	ssize_t result;
	size_t bytes_to_read;
	struct mount_info *mi = NULL;
	struct backing_file_context *bfc = NULL;
	struct data_file_block block = {};
	struct data_file *df = get_incfs_data_file(f);

	if (!dst.data || !df)
		return -EFAULT;

	if (tmp.len < 2 * INCFS_DATA_FILE_BLOCK_SIZE)
		return -ERANGE;

	mi = df->df_mount_info;
	bfc = df->df_backing_file_context;

	result = wait_for_data_block(df, index, timeout_ms, &block);
	if (result < 0)
		goto out;

	pos = block.db_backing_file_data_offset;
	if (block.db_comp_alg == COMPRESSION_NONE) {
		bytes_to_read = min(dst.len, block.db_stored_size);
		result = incfs_kread(bfc, dst.data, bytes_to_read, pos);

		/* Some data was read, but not enough */
		if (result >= 0 && result != bytes_to_read)
			result = -EIO;
	} else {
		bytes_to_read = min(tmp.len, block.db_stored_size);
		result = incfs_kread(bfc, tmp.data, bytes_to_read, pos);
		if (result == bytes_to_read) {
			result =
				decompress(range(tmp.data, bytes_to_read), dst);
			if (result < 0) {
				const char *name =
				    bfc->bc_file->f_path.dentry->d_name.name;

				pr_warn_once("incfs: Decompression error. %s",
					     name);
			}
		} else if (result >= 0) {
			/* Some data was read, but not enough */
			result = -EIO;
		}
	}

	if (result > 0) {
		int err = validate_hash_tree(bfc, f, index, dst, tmp.data);

		if (err < 0)
			result = err;
	}

	if (result >= 0)
		log_block_read(mi, &df->df_id, index);

out:
	return result;
}

int incfs_process_new_data_block(struct data_file *df,
				 struct incfs_fill_block *block, u8 *data)
{
	struct mount_info *mi = NULL;
	struct backing_file_context *bfc = NULL;
	struct data_file_segment *segment = NULL;
	struct data_file_block existing_block = {};
	u16 flags = 0;
	int error = 0;

	if (!df || !block)
		return -EFAULT;

	bfc = df->df_backing_file_context;
	mi = df->df_mount_info;

	if (block->block_index >= df->df_data_block_count)
		return -ERANGE;

	segment = get_file_segment(df, block->block_index);
	if (!segment)
		return -EFAULT;
	if (block->compression == COMPRESSION_LZ4)
		flags |= INCFS_BLOCK_COMPRESSED_LZ4;

	error = mutex_lock_interruptible(&segment->blockmap_mutex);
	if (error)
		return error;

	error = get_data_file_block(df, block->block_index, &existing_block);
	if (error)
		goto unlock;
	if (is_data_block_present(&existing_block)) {
		/* Block is already present, nothing to do here */
		goto unlock;
	}

	error = mutex_lock_interruptible(&bfc->bc_mutex);
	if (!error) {
		error = incfs_write_data_block_to_backing_file(
			bfc, range(data, block->data_len), block->block_index,
			df->df_blockmap_off, flags);
		mutex_unlock(&bfc->bc_mutex);
	}
	if (!error)
		notify_pending_reads(mi, segment, block->block_index);

unlock:
	mutex_unlock(&segment->blockmap_mutex);
	if (error)
		pr_debug("%d error: %d\n", block->block_index, error);
	return error;
}

int incfs_read_file_signature(struct data_file *df, struct mem_range dst)
{
	struct backing_file_context *bfc = df->df_backing_file_context;
	struct incfs_df_signature *sig;
	int read_res = 0;

	if (!dst.data)
		return -EFAULT;

	sig = df->df_signature;
	if (!sig)
		return 0;

	if (dst.len < sig->sig_size)
		return -E2BIG;

	read_res = incfs_kread(bfc, dst.data, sig->sig_size, sig->sig_offset);

	if (read_res < 0)
		return read_res;

	if (read_res != sig->sig_size)
		return -EIO;

	return read_res;
}

int incfs_process_new_hash_block(struct data_file *df,
				 struct incfs_fill_block *block, u8 *data)
{
	struct backing_file_context *bfc = NULL;
	struct mount_info *mi = NULL;
	struct mtree *hash_tree = NULL;
	struct incfs_df_signature *sig = NULL;
	loff_t hash_area_base = 0;
	loff_t hash_area_size = 0;
	int error = 0;

	if (!df || !block)
		return -EFAULT;

	if (!(block->flags & INCFS_BLOCK_FLAGS_HASH))
		return -EINVAL;

	bfc = df->df_backing_file_context;
	mi = df->df_mount_info;

	if (!df)
		return -ENOENT;

	hash_tree = df->df_hash_tree;
	sig = df->df_signature;
	if (!hash_tree || !sig || sig->hash_offset == 0)
		return -ENOTSUPP;

	hash_area_base = sig->hash_offset;
	hash_area_size = sig->hash_size;
	if (hash_area_size < block->block_index * INCFS_DATA_FILE_BLOCK_SIZE
				+ block->data_len) {
		/* Hash block goes beyond dedicated hash area of this file. */
		return -ERANGE;
	}

	error = mutex_lock_interruptible(&bfc->bc_mutex);
	if (!error) {
		error = incfs_write_hash_block_to_backing_file(
			bfc, range(data, block->data_len), block->block_index,
			hash_area_base, df->df_blockmap_off, df->df_size);
		mutex_unlock(&bfc->bc_mutex);
	}
	return error;
}

static int process_blockmap_md(struct incfs_blockmap *bm,
			       struct metadata_handler *handler)
{
	struct data_file *df = handler->context;
	int error = 0;
	loff_t base_off = le64_to_cpu(bm->m_base_offset);
	u32 block_count = le32_to_cpu(bm->m_block_count);

	if (!df)
		return -EFAULT;

	if (df->df_data_block_count > block_count)
		return -EBADMSG;

	df->df_total_block_count = block_count;
	df->df_blockmap_off = base_off;
	return error;
}

static int process_file_attr_md(struct incfs_file_attr *fa,
				struct metadata_handler *handler)
{
	struct data_file *df = handler->context;
	u16 attr_size = le16_to_cpu(fa->fa_size);

	if (!df)
		return -EFAULT;

	if (attr_size > INCFS_MAX_FILE_ATTR_SIZE)
		return -E2BIG;

	df->n_attr.fa_value_offset = le64_to_cpu(fa->fa_offset);
	df->n_attr.fa_value_size = attr_size;
	df->n_attr.fa_crc = le32_to_cpu(fa->fa_crc);

	return 0;
}

static int process_file_signature_md(struct incfs_file_signature *sg,
				struct metadata_handler *handler)
{
	struct data_file *df = handler->context;
	struct mtree *hash_tree = NULL;
	int error = 0;
	struct incfs_df_signature *signature =
		kzalloc(sizeof(*signature), GFP_NOFS);
	void *buf = NULL;
	ssize_t read;

	if (!signature)
		return -ENOMEM;

	if (!df || !df->df_backing_file_context ||
	    !df->df_backing_file_context->bc_file) {
		error = -ENOENT;
		goto out;
	}

	signature->hash_offset = le64_to_cpu(sg->sg_hash_tree_offset);
	signature->hash_size = le32_to_cpu(sg->sg_hash_tree_size);
	signature->sig_offset = le64_to_cpu(sg->sg_sig_offset);
	signature->sig_size = le32_to_cpu(sg->sg_sig_size);

	buf = kzalloc(signature->sig_size, GFP_NOFS);
	if (!buf) {
		error = -ENOMEM;
		goto out;
	}

	read = incfs_kread(df->df_backing_file_context, buf,
			   signature->sig_size, signature->sig_offset);
	if (read < 0) {
		error = read;
		goto out;
	}

	if (read != signature->sig_size) {
		error = -EINVAL;
		goto out;
	}

	hash_tree = incfs_alloc_mtree(range(buf, signature->sig_size),
				      df->df_data_block_count);
	if (IS_ERR(hash_tree)) {
		error = PTR_ERR(hash_tree);
		hash_tree = NULL;
		goto out;
	}
	if (hash_tree->hash_tree_area_size != signature->hash_size) {
		error = -EINVAL;
		goto out;
	}
	if (signature->hash_size > 0 &&
	    handler->md_record_offset <= signature->hash_offset) {
		error = -EINVAL;
		goto out;
	}
	if (handler->md_record_offset <= signature->sig_offset) {
		error = -EINVAL;
		goto out;
	}
	df->df_hash_tree = hash_tree;
	hash_tree = NULL;
	df->df_signature = signature;
	signature = NULL;
out:
	incfs_free_mtree(hash_tree);
	kfree(signature);
	kfree(buf);

	return error;
}

int incfs_scan_metadata_chain(struct data_file *df)
{
	struct metadata_handler *handler = NULL;
	int result = 0;
	int records_count = 0;
	int error = 0;
	struct backing_file_context *bfc = NULL;

	if (!df || !df->df_backing_file_context)
		return -EFAULT;

	bfc = df->df_backing_file_context;

	handler = kzalloc(sizeof(*handler), GFP_NOFS);
	if (!handler)
		return -ENOMEM;

	/* No writing to the backing file while it's being scanned. */
	error = mutex_lock_interruptible(&bfc->bc_mutex);
	if (error)
		goto out;

	/* Reading superblock */
	handler->md_record_offset = df->df_metadata_off;
	handler->context = df;
	handler->handle_blockmap = process_blockmap_md;
	handler->handle_file_attr = process_file_attr_md;
	handler->handle_signature = process_file_signature_md;

	pr_debug("incfs: Starting reading incfs-metadata records at offset %lld\n",
		 handler->md_record_offset);
	while (handler->md_record_offset > 0) {
		error = incfs_read_next_metadata_record(bfc, handler);
		if (error) {
			pr_warn("incfs: Error during reading incfs-metadata record. Offset: %lld Record #%d Error code: %d\n",
				handler->md_record_offset, records_count + 1,
				-error);
			break;
		}
		records_count++;
	}
	if (error) {
		pr_debug("incfs: Error %d after reading %d incfs-metadata records.\n",
			 -error, records_count);
		result = error;
	} else {
		pr_debug("incfs: Finished reading %d incfs-metadata records.\n",
			 records_count);
		result = records_count;
	}
	mutex_unlock(&bfc->bc_mutex);

	if (df->df_hash_tree) {
		int hash_block_count = get_blocks_count_for_size(
			df->df_hash_tree->hash_tree_area_size);

		if (df->df_data_block_count + hash_block_count !=
		    df->df_total_block_count)
			result = -EINVAL;
	} else if (df->df_data_block_count != df->df_total_block_count)
		result = -EINVAL;

out:
	kfree(handler);
	return result;
}

/*
 * Quickly checks if there are pending reads with a serial number larger
 * than a given one.
 */
bool incfs_fresh_pending_reads_exist(struct mount_info *mi, int last_number)
{
	bool result = false;

	mutex_lock(&mi->mi_pending_reads_mutex);
	result = (mi->mi_last_pending_read_number > last_number) &&
		 (mi->mi_pending_reads_count > 0);
	mutex_unlock(&mi->mi_pending_reads_mutex);
	return result;
}

int incfs_collect_pending_reads(struct mount_info *mi, int sn_lowerbound,
				struct incfs_pending_read_info *reads,
				int reads_size)
{
	int reported_reads = 0;
	struct pending_read *entry = NULL;

	if (!mi)
		return -EFAULT;

	if (reads_size <= 0)
		return 0;

	mutex_lock(&mi->mi_pending_reads_mutex);

	if (mi->mi_last_pending_read_number <= sn_lowerbound
	    || mi->mi_pending_reads_count == 0)
		goto unlock;

	list_for_each_entry(entry, &mi->mi_reads_list_head, mi_reads_list) {
		if (entry->serial_number <= sn_lowerbound)
			continue;

		reads[reported_reads].file_id = entry->file_id;
		reads[reported_reads].block_index = entry->block_index;
		reads[reported_reads].serial_number = entry->serial_number;
		reads[reported_reads].timestamp_us = entry->timestamp_us;
		/* reads[reported_reads].kind = INCFS_READ_KIND_PENDING; */

		reported_reads++;
		if (reported_reads >= reads_size)
			break;
	}

unlock:
	mutex_unlock(&mi->mi_pending_reads_mutex);

	return reported_reads;
}

struct read_log_state incfs_get_log_state(struct mount_info *mi)
{
	struct read_log *log = &mi->mi_log;
	struct read_log_state result;

	spin_lock(&log->rl_lock);
	result = log->rl_head;
	spin_unlock(&log->rl_lock);
	return result;
}

int incfs_get_uncollected_logs_count(struct mount_info *mi,
				     const struct read_log_state *state)
{
	struct read_log *log = &mi->mi_log;
	u32 generation;
	u64 head_no, tail_no;

	spin_lock(&log->rl_lock);
	tail_no = log->rl_tail.current_record_no;
	head_no = log->rl_head.current_record_no;
	generation = log->rl_head.generation_id;
	spin_unlock(&log->rl_lock);

	if (generation != state->generation_id)
		return head_no - tail_no;
	else
		return head_no - max_t(u64, tail_no, state->current_record_no);
}

int incfs_collect_logged_reads(struct mount_info *mi,
			       struct read_log_state *reader_state,
			       struct incfs_pending_read_info *reads,
			       int reads_size)
{
	int dst_idx;
	struct read_log *log = &mi->mi_log;
	struct read_log_state *head, *tail;

	spin_lock(&log->rl_lock);
	head = &log->rl_head;
	tail = &log->rl_tail;

	if (reader_state->generation_id != head->generation_id) {
		pr_debug("read ptr is wrong generation: %u/%u",
			 reader_state->generation_id, head->generation_id);

		*reader_state = (struct read_log_state){
			.generation_id = head->generation_id,
		};
	}

	if (reader_state->current_record_no < tail->current_record_no) {
		pr_debug("read ptr is behind, moving: %u/%u -> %u/%u\n",
			 (u32)reader_state->next_offset,
			 (u32)reader_state->current_pass_no,
			 (u32)tail->next_offset, (u32)tail->current_pass_no);

		*reader_state = *tail;
	}

	for (dst_idx = 0; dst_idx < reads_size; dst_idx++) {
		if (reader_state->current_record_no == head->current_record_no)
			break;

		log_read_one_record(log, reader_state);

		reads[dst_idx] = (struct incfs_pending_read_info){
			.file_id = reader_state->base_record.file_id,
			.block_index = reader_state->base_record.block_index,
			.serial_number = reader_state->current_record_no,
			.timestamp_us = reader_state->base_record.absolute_ts_us
		};
	}

	spin_unlock(&log->rl_lock);
	return dst_idx;
}

bool incfs_equal_ranges(struct mem_range lhs, struct mem_range rhs)
{
	if (lhs.len != rhs.len)
		return false;
	return memcmp(lhs.data, rhs.data, lhs.len) == 0;
}
