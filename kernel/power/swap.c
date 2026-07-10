// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/kernel/power/swap.c
 *
 * This file provides functions for reading the suspend image from
 * and writing it to a swap partition.
 *
 * Copyright (C) 1998,2001-2005 Pavel Machek <pavel@ucw.cz>
 * Copyright (C) 2006 Rafael J. Wysocki <rjw@sisk.pl>
 * Copyright (C) 2010-2012 Bojan Smojver <bojan@rexursive.com>
 */

#define pr_fmt(fmt) "PM: " fmt

#include <linux/module.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/genhd.h>
#include <linux/device.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/cpumask.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/crc32.h>
#include <linux/ktime.h>
#include <trace/hooks/bl_hib.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#include "power.h"

#define AUTH_SLOT	0
#define HIBERNATE_SIG	"S1SUSPEND"

/*
 * When reading an {un,}compressed image, we may restore pages in place,
 * in which case some architectures need these pages cleaning before they
 * can be executed. We don't know which pages these may be, so clean the lot.
 */
static bool clean_pages_on_read;
static bool clean_pages_on_decompress;

static atomic_t stats_compression_us = ATOMIC_INIT(0);
static unsigned int stats_encryption_us;
static unsigned int stats_async_io_us;
static unsigned int stats_num_cmp_threads = 0;
unsigned int stats_consecutive_hib_entries;
/* Forward declaration for compressed_size atomic variable */
static atomic_t compressed_size;
struct hib_bio_batch hb;

#ifdef CONFIG_DEBUG_FS
static unsigned long toggle_bit = 0;
#endif
/*
 *	The swap map is a data structure used for keeping track of each page
 *	written to a swap partition.  It consists of many swap_map_page
 *	structures that contain each an array of MAP_PAGE_ENTRIES swap entries.
 *	These structures are stored on the swap and linked together with the
 *	help of the .next_swap member.
 *
 *	The swap map is created during suspend.  The swap map pages are
 *	allocated and populated one at a time, so we only need one memory
 *	page to set up the entire structure.
 *
 *	During resume we pick up all swap_map_page structures into a list.
 */

#define MAP_PAGE_ENTRIES	(PAGE_SIZE / sizeof(sector_t) - 1)

/*
 * Number of free pages that are not high.
 */
static inline unsigned long low_free_pages(void)
{
	return nr_free_pages() - nr_free_highpages();
}

/*
 * Number of pages required to be kept free while writing the image. Always
 * half of all available low pages before the writing starts.
 */
static inline unsigned long reqd_free_pages(void)
{
	return low_free_pages() / 2;
}

struct swap_map_page {
	sector_t entries[MAP_PAGE_ENTRIES];
	sector_t next_swap;
};

struct swap_map_page_list {
	struct swap_map_page *map;
	struct swap_map_page_list *next;
};

/**
 *	The swap_map_handle structure is used for handling swap in
 *	a file-alike way
 */

struct swap_map_handle {
	struct swap_map_page *cur;
	struct swap_map_page_list *maps;
	sector_t cur_swap;
	sector_t first_sector;
	unsigned int k;
	unsigned long reqd_free_pages;
	u32 crc32;
};

struct swsusp_header *swsusp_header;
EXPORT_SYMBOL_GPL(swsusp_header);

/**
 *	The following functions are used for tracing the allocated
 *	swap pages, so that they can be freed in case of an error.
 */

struct swsusp_extent {
	struct rb_node node;
	unsigned long start;
	unsigned long end;
};

static struct rb_root swsusp_extents = RB_ROOT;

static int swsusp_extents_insert(unsigned long swap_offset)
{
	struct rb_node **new = &(swsusp_extents.rb_node);
	struct rb_node *parent = NULL;
	struct swsusp_extent *ext;

	/* Figure out where to put the new node */
	while (*new) {
		ext = rb_entry(*new, struct swsusp_extent, node);
		parent = *new;
		if (swap_offset < ext->start) {
			/* Try to merge */
			if (swap_offset == ext->start - 1) {
				ext->start--;
				return 0;
			}
			new = &((*new)->rb_left);
		} else if (swap_offset > ext->end) {
			/* Try to merge */
			if (swap_offset == ext->end + 1) {
				ext->end++;
				return 0;
			}
			new = &((*new)->rb_right);
		} else {
			/* It already is in the tree */
			return -EINVAL;
		}
	}
	/* Add the new node and rebalance the tree. */
	ext = kzalloc(sizeof(struct swsusp_extent), GFP_KERNEL);
	if (!ext)
		return -ENOMEM;

	ext->start = swap_offset;
	ext->end = swap_offset;
	rb_link_node(&ext->node, parent, new);
	rb_insert_color(&ext->node, &swsusp_extents);
	return 0;
}

/**
 *	alloc_swapdev_block - allocate a swap page and register that it has
 *	been allocated, so that it can be freed in case of an error.
 */

sector_t alloc_swapdev_block(int swap)
{
	unsigned long offset;

	offset = swp_offset(get_swap_page_of_type(swap));
	if (offset) {
		if (swsusp_extents_insert(offset))
			swap_free(swp_entry(swap, offset));
		else
			return swapdev_block(swap, offset);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(alloc_swapdev_block);

/**
 *	free_all_swap_pages - free swap pages allocated for saving image data.
 *	It also frees the extents used to register which swap entries had been
 *	allocated.
 */

void free_all_swap_pages(int swap)
{
	struct rb_node *node;

	while ((node = swsusp_extents.rb_node)) {
		struct swsusp_extent *ext;
		unsigned long offset;

		ext = rb_entry(node, struct swsusp_extent, node);
		rb_erase(node, &swsusp_extents);
		for (offset = ext->start; offset <= ext->end; offset++)
			swap_free(swp_entry(swap, offset));

		kfree(ext);
	}
}

int swsusp_swap_in_use(void)
{
	return (swsusp_extents.rb_node != NULL);
}

/*
 * General things
 */

static unsigned short root_swap = 0xffff;
struct block_device *hib_resume_bdev;
EXPORT_SYMBOL_GPL(hib_resume_bdev);

uint32_t swap_auth_slot_offset;
EXPORT_SYMBOL_GPL(swap_auth_slot_offset);

struct hib_bio_batch {
	atomic_t		count;
	wait_queue_head_t	wait;
	blk_status_t		error;
	struct blk_plug		plug;
};

static void hib_init_batch(struct hib_bio_batch *hb)
{
	atomic_set(&hb->count, 0);
	init_waitqueue_head(&hb->wait);
	hb->error = BLK_STS_OK;
	blk_start_plug(&hb->plug);
}

static void hib_finish_batch(struct hib_bio_batch *hb)
{
	blk_finish_plug(&hb->plug);
}

static void hib_end_io(struct bio *bio)
{
	struct hib_bio_batch *hb = bio->bi_private;
	struct page *page = bio_first_page_all(bio);

	if (bio->bi_status) {
		pr_alert("Read-error on swap-device (%u:%u:%Lu)\n",
			 MAJOR(bio_dev(bio)), MINOR(bio_dev(bio)),
			 (unsigned long long)bio->bi_iter.bi_sector);
	}

	if (bio_data_dir(bio) == READ)
		trace_android_vh_decrypt_page(page_to_virt(page),
			bio->bi_iter.bi_sector - (PAGE_SIZE >> SECTOR_SHIFT));

	if (bio_data_dir(bio) == WRITE) {
		put_page(page);
	} else if (clean_pages_on_read)
		flush_icache_range((unsigned long)page_address(page),
				   (unsigned long)page_address(page) + PAGE_SIZE);

	if (bio->bi_status && !hb->error)
		hb->error = bio->bi_status;
	if (atomic_dec_and_test(&hb->count))
		wake_up(&hb->wait);

	bio_put(bio);
}

static int hib_submit_io(int op, int op_flags, pgoff_t page_off, void *addr,
		struct hib_bio_batch *hb)
{
	struct page *page = virt_to_page(addr);
	struct bio *bio;
	int error = 0;

	bio = bio_alloc(GFP_NOIO | __GFP_HIGH, 1);
	bio->bi_iter.bi_sector = page_off * (PAGE_SIZE >> 9);
	bio_set_dev(bio, hib_resume_bdev);
	bio_set_op_attrs(bio, op, op_flags);

	if (bio_add_page(bio, page, PAGE_SIZE, 0) < PAGE_SIZE) {
		pr_err("Adding page to bio failed at %llu\n",
		       (unsigned long long)bio->bi_iter.bi_sector);
		bio_put(bio);
		return -EFAULT;
	}

	if (hb) {
		bio->bi_end_io = hib_end_io;
		bio->bi_private = hb;
		atomic_inc(&hb->count);
		submit_bio(bio);
	} else {
		error = submit_bio_wait(bio);
		bio_put(bio);
	}

	return error;
}

static int hib_wait_io(struct hib_bio_batch *hb)
{
	/*
	 * We are relying on the behavior of blk_plug that a thread with
	 * a plug will flush the plug list before sleeping.
	 */
	wait_event(hb->wait, atomic_read(&hb->count) == 0);
	return blk_status_to_errno(hb->error);
}

/*
 * Saving part
 */

static int mark_swapfiles(struct swap_map_handle *handle, unsigned int flags)
{
	int error;
	uint32_t auth_slot = 0;

	hib_submit_io(REQ_OP_READ, 0, swsusp_resume_block,
		      swsusp_header, NULL);
	if (!memcmp("SWAP-SPACE",swsusp_header->sig, 10) ||
	    !memcmp("SWAPSPACE2",swsusp_header->sig, 10)) {
		/* We save the statistics we've collected to first block in swap */
		populate_hib_entry_stats(&swsusp_header->stats);
		trace_android_vh_store_auth_slot_num(&auth_slot);
		memcpy(swsusp_header->orig_sig,swsusp_header->sig, 10);
		memcpy(swsusp_header->sig, HIBERNATE_SIG, 10);
		swsusp_header->image = handle->first_sector;
		swsusp_header->flags = flags;
		((uint32_t *)(swsusp_header->reserved))[AUTH_SLOT] = auth_slot;
		if (flags & SF_CRC32_MODE)
			swsusp_header->crc32 = handle->crc32;
		error = hib_submit_io(REQ_OP_WRITE, REQ_SYNC,
				      swsusp_resume_block, swsusp_header, NULL);
	} else {
		pr_err("Swap header not found!\n");
		error = -ENODEV;
	}
	return error;
}

/*
* Hold the swsusp_header flag. This is used in software_resume() in
* 'kernel/power/hibernate' to check if the image is compressed and query
* for the compression algorithm support(if so).
*/
unsigned int swsusp_header_flags;

/**
 *	swsusp_swap_check - check if the resume device is a swap device
 *	and get its index (if so)
 *
 *	This is called before saving image
 */
static int swsusp_swap_check(void)
{
	int res;

	if (swsusp_resume_device)
		res = swap_type_of(swsusp_resume_device, swsusp_resume_block);
	else
		res = find_first_swap(&swsusp_resume_device);
	if (res < 0)
		return res;
	root_swap = res;

	hib_resume_bdev = blkdev_get_by_dev(swsusp_resume_device, FMODE_WRITE,
			NULL);
	if (IS_ERR(hib_resume_bdev))
		return PTR_ERR(hib_resume_bdev);

	res = set_blocksize(hib_resume_bdev, PAGE_SIZE);
	if (res < 0)
		blkdev_put(hib_resume_bdev, FMODE_WRITE);

	return res;
}

/**
 *	write_page - Write one page to given swap location.
 *	@buf:		Address we're writing.
 *	@offset:	Offset of the swap page we're writing to.
 *	@hb:		bio completion batch
 */

static int write_page(void *buf, sector_t offset, struct hib_bio_batch *hb)
{
	void *src;
	int ret;

	if (!offset)
		return -ENOSPC;

	if (hb) {
		src = (void *)__get_free_page(GFP_NOIO | __GFP_NOWARN |
		                              __GFP_NORETRY);
		if (src) {
			copy_page(src, buf);
		} else {
			ret = hib_wait_io(hb); /* Free pages */
			if (ret)
				return ret;
			src = (void *)__get_free_page(GFP_NOIO |
			                              __GFP_NOWARN |
			                              __GFP_NORETRY);
			if (src) {
				copy_page(src, buf);
			} else {
				WARN_ON_ONCE(1);
				hb = NULL;	/* Go synchronous */
				src = buf;
			}
		}
	} else {
		src = buf;
	}
	return hib_submit_io(REQ_OP_WRITE, REQ_SYNC, offset, src, hb);
}

static void release_swap_writer(struct swap_map_handle *handle)
{
	if (handle->cur)
		free_page((unsigned long)handle->cur);
	handle->cur = NULL;
}

static int get_swap_writer(struct swap_map_handle *handle)
{
	int ret;

	ret = swsusp_swap_check();
	if (ret) {
		if (ret != -ENOSPC)
			pr_err("Cannot find swap device, try swapon -a\n");
		return ret;
	}
	handle->cur = (struct swap_map_page *)get_zeroed_page(GFP_KERNEL);
	if (!handle->cur) {
		ret = -ENOMEM;
		goto err_close;
	}
	handle->cur_swap = alloc_swapdev_block(root_swap);
	if (!handle->cur_swap) {
		ret = -ENOSPC;
		goto err_rel;
	}
	if (handle->cur_swap != 1) {
		ret = -EPERM;
		pr_err("Swap is already used %d\n", handle->cur_swap);
		goto err_free;
	}

	handle->k = 0;
	handle->reqd_free_pages = reqd_free_pages();
	handle->first_sector = handle->cur_swap;
	swsusp_header->image = handle->first_sector;
	return 0;
err_free:
	free_all_swap_pages(root_swap);
err_rel:
	release_swap_writer(handle);
err_close:
	swsusp_close(FMODE_WRITE);
	return ret;
}

static int swap_write_page(struct swap_map_handle *handle, void *buf,
		struct hib_bio_batch *hb)
{
	int error = 0;
	sector_t offset;
	bool skip = false;
	ktime_t start_enc;

	if (!handle->cur)
		return -EINVAL;
	offset = alloc_swapdev_block(root_swap);
	if (hb) {
		start_enc = ktime_get();
		trace_android_vh_encrypt_page(buf, offset * (PAGE_SIZE >> 9));
		stats_encryption_us += ktime_to_us(ktime_sub(ktime_get(), start_enc));
	}
	error = write_page(buf, offset, hb);
	if (error)
		return error;
	handle->cur->entries[handle->k++] = offset;
	if (handle->k >= MAP_PAGE_ENTRIES) {
		offset = alloc_swapdev_block(root_swap);
		if (!offset)
			return -ENOSPC;
		handle->cur->next_swap = offset;
		trace_android_vh_skip_swap_map_write(&skip);
		if (!skip) {
			error = write_page(handle->cur, handle->cur_swap, hb);
			if (error)
				goto out;
		}
		clear_page(handle->cur);
		handle->cur_swap = offset;
		handle->k = 0;

		if (hb && low_free_pages() <= handle->reqd_free_pages) {
			error = hib_wait_io(hb);
			if (error)
				goto out;
			/*
			 * Recalculate the number of required free pages, to
			 * make sure we never take more than half.
			 */
			handle->reqd_free_pages = reqd_free_pages();
		}
	}
 out:
	return error;
}

static int flush_swap_writer(struct swap_map_handle *handle)
{
	if (handle->cur && handle->cur_swap)
		return write_page(handle->cur, handle->cur_swap, NULL);
	else
		return -EINVAL;
}

#ifdef CONFIG_DEBUG_FS
static int get_swap_reader(struct swap_map_handle *handle,
		unsigned int *flags_p);

static void toggle_bit_if_needed(void)
{
	struct swap_map_handle handle;
	uint64_t byte, block, offset;
	unsigned int flags;
	uint8_t mask, value;
	uint8_t *data;
	char *hit_str = "outside hib image";

	if (!toggle_bit)
		return;

	if (IS_ERR_OR_NULL(hib_resume_bdev))
		return;

	byte = toggle_bit >> 3;
	if (byte >= i_size_read(hib_resume_bdev->bd_inode)) {
		pr_err("Toggle bit:%llu ignored. Outside storage size.\n");
		return;
	}

	block = byte >> PAGE_SHIFT;
	offset = byte - (block << PAGE_SHIFT);
	if (block == 0) {
		hit_str = "swap header";
	} else if (block == swsusp_header->image) {
		hit_str = "map page";
	} else if (block == swsusp_header->image + 1) {
		hit_str = "swap info";
	} else {
		bool hit_data = false;
		bool hit_map = false;

		if (get_swap_reader(&handle, &flags)) {
			pr_err("Toggle bit:%llu ignored. Failed to read map pages.\n");
			return;
		}

		while (handle.maps) {
			struct swap_map_page_list *tmp = handle.maps;
			uint32_t i = 0;
			while (!hit_data && i < MAP_PAGE_ENTRIES) {
				if (tmp->map->entries[i++] == block) {
					hit_data = true;
					break;
				}
			}
			if (!hit_data && tmp->map->next_swap == block) {
				hit_map = true;
			}

			if (tmp->map)
				free_page((unsigned long)tmp->map);
			handle.maps = handle.maps->next;
			kfree(tmp);
		}
		if (hit_data)
			hit_str = "data page";
		else if (hit_map)
			hit_str = "map page";
		else {
			sector_t next_slot = alloc_swapdev_block(root_swap);
			uint32_t auth_slot;
			trace_android_vh_store_auth_slot_num(&auth_slot);
			pr_err("Toggle auth_slot:%llu block=%llu next_slot:%llu\n", auth_slot, block, next_slot);
			if (block == (auth_slot - 1))
				hit_str = "params page";
			else if ((block >= auth_slot) && (block <= (next_slot - 2))) {
				if (((auth_slot - block) % 2) && (offset >= (PAGE_SIZE - 16) && offset < (PAGE_SIZE - 8)))
					hit_str = "near auth tags, but miss";
				else
					hit_str = "auth tags";
			} else if (block == next_slot - 1)
				hit_str = "auth tags (inconclusive)";
		}
	}

	mask = (1 << (toggle_bit & 0x7));
	data = (uint8_t *) __get_free_page(GFP_KERNEL);
	if (!data) {
		pr_err("Toggle bit:%llu ignored. Can't get free page.\n");
		return;
	}

	hib_submit_io(REQ_OP_READ, 0, block, data, NULL);
	if (data[offset] & mask)
		value = data[offset] & ~mask;
	else
		value = data[offset] | mask;
	pr_err("Toggle bit:%lld block:%lld offset:%lld mask:0x%02x value:0x%02x->0x%02x will hit %s\n",
			toggle_bit, block, offset, mask, data[offset], value, hit_str);
	data[offset] = value;
	hib_submit_io(REQ_OP_WRITE, REQ_SYNC, block, data, NULL);
	free_page((unsigned long)data);
#endif
}


static int swap_writer_finish(struct swap_map_handle *handle,
		unsigned int flags, int error)
{
	if (!error) {
		pr_info("S");
		error = mark_swapfiles(handle, flags);
		pr_cont("|\n");
		if(!error)
			print_hib_entry_stats(&swsusp_header->stats, true);
		flush_swap_writer(handle);
#ifdef CONFIG_DEBUG_FS
		if(!error)
			toggle_bit_if_needed();
#endif
	}

	if (error)
		free_all_swap_pages(root_swap);
	release_swap_writer(handle);
	swsusp_close(FMODE_WRITE);

	return error;
}

/*
* Bytes we need for compressed data in worst case. We assume(limitation)
* this is the worst of all the compression algorithms.
*/
#define bytes_worst_compress(x) ((x) + ((x) / 16) + 64 + 3 + 2)

/* We need to remember how much compressed data we need to read. */
#define CMP_HEADER sizeof(size_t)

/* Number of pages/bytes we'll compress at one time. */
#define UNC_PAGES 32
#define UNC_SIZE (UNC_PAGES * PAGE_SIZE)

/* Number of pages we need for compressed data (worst case). */
#define CMP_PAGES DIV_ROUND_UP(bytes_worst_compress(UNC_SIZE) + \
	CMP_HEADER, PAGE_SIZE)
#define CMP_SIZE (CMP_PAGES * PAGE_SIZE)

/* Maximum number of threads for compression/decompression. */
#define CMP_THREADS 3

/* Minimum/maximum number of pages for read buffering. */
#define CMP_MIN_RD_PAGES 1024
#define CMP_MAX_RD_PAGES 8192

/**
 *	save_image - save the suspend image data
 */

static int save_image(struct swap_map_handle *handle,
                      struct snapshot_handle *snapshot,
                      unsigned int nr_to_write)
{
	unsigned int m;
	int ret;
	int nr_pages;
	int err2;
	struct hib_bio_batch hb;
	ktime_t start;
	ktime_t stop;

	hib_init_batch(&hb);

	pr_info("Saving image data pages (%u pages)...\n",
		nr_to_write);
	m = nr_to_write / 10;
	if (!m)
		m = 1;
	nr_pages = 0;
	start = ktime_get();
	while (1) {
		ret = snapshot_read_next(snapshot);
		if (ret <= 0)
			break;
		ret = swap_write_page(handle, data_of(*snapshot), &hb);
		if (ret)
			break;
		if (!(nr_pages % m))
			pr_info("Image saving progress: %3d%%\n",
				nr_pages / m * 10);
		nr_pages++;
	}
	err2 = hib_wait_io(&hb);
	hib_finish_batch(&hb);
	stop = ktime_get();
	if (!ret)
		ret = err2;
	if (!ret)
		pr_info("Image saving done\n");
	swsusp_show_speed(start, stop, nr_to_write, "Wrote");
	return ret;
}

/**
 * Structure used for CRC32.
 */
struct crc_data {
	struct task_struct *thr;                  /* thread */
	atomic_t ready;                           /* ready to start flag */
	atomic_t stop;                            /* ready to stop flag */
	unsigned run_threads;                     /* nr current threads */
	wait_queue_head_t go;                     /* start crc update */
	wait_queue_head_t done;                   /* crc update done */
	u32 *crc32;                               /* points to handle's crc32 */
	size_t *unc_len[CMP_THREADS];       /* uncompressed lengths */
	unsigned char *unc[CMP_THREADS];     /* uncompressed data */
};

/**
 * CRC32 update function that runs in its own thread.
 */
static int crc32_threadfn(void *data)
{
	struct crc_data *d = data;
	unsigned i;

	while (1) {
		wait_event(d->go, atomic_read_acquire(&d->ready) ||
		                  kthread_should_stop());
		if (kthread_should_stop()) {
			d->thr = NULL;
			atomic_set_release(&d->stop, 1);
			wake_up(&d->done);
			break;
		}
		atomic_set(&d->ready, 0);

		for (i = 0; i < d->run_threads; i++)
			*d->crc32 = crc32_le(*d->crc32,
			                     d->unc[i], *d->unc_len[i]);
		atomic_set_release(&d->stop, 1);
		wake_up(&d->done);
	}
	return 0;
}
/**
 * Structure used for data compression.
 */
struct cmp_data {
	struct task_struct *thr;                  /* thread */
	struct crypto_comp *cc;          /* crypto compressor stream */
	atomic_t ready;                           /* ready to start flag */
	atomic_t stop;                            /* ready to stop flag */
	int ret;                                  /* return code */
	wait_queue_head_t go;                     /* start compression */
	wait_queue_head_t done;                   /* compression done */
	size_t unc_len;                           /* uncompressed length */
	size_t cmp_len;                           /* compressed length */
	unsigned char unc[UNC_SIZE];       /* uncompressed buffer */
	unsigned char cmp[CMP_SIZE];       /* compressed buffer */
};

/* Indicates the image size after compression */
static atomic_t compressed_size = ATOMIC_INIT(0);

/**
 * Compression function that runs in its own thread.
 */
static int compress_threadfn(void *data)
{
	struct cmp_data *d = data;
	unsigned int cmp_len = 0;
	ktime_t start_cmp;

	while (1) {
		wait_event(d->go, atomic_read_acquire(&d->ready) ||
		                  kthread_should_stop());
		if (kthread_should_stop()) {
			d->thr = NULL;
			d->ret = -1;
			atomic_set_release(&d->stop, 1);
			wake_up(&d->done);
			break;
		}
		atomic_set(&d->ready, 0);

		cmp_len = CMP_SIZE - CMP_HEADER;
		start_cmp = ktime_get();
		d->ret = crypto_comp_compress(d->cc, d->unc, d->unc_len,
			d->cmp + CMP_HEADER,
			&cmp_len);
		d->cmp_len = cmp_len;
		atomic_add(ktime_to_us(ktime_sub(ktime_get(), start_cmp)), &stats_compression_us);

		atomic_set(&compressed_size, atomic_read(&compressed_size) + d->cmp_len);
		atomic_set_release(&d->stop, 1);
		wake_up(&d->done);
	}
	return 0;
}

/**
 * save_compressed_image - Save the suspend image data after compression.
 * @handle: Swap map handle to use for saving the image.
 * @snapshot: Image to read data from.
 * @nr_to_write: Number of pages to save.
 */
static int save_compressed_image(struct swap_map_handle *handle,
	struct snapshot_handle *snapshot,
	unsigned int nr_to_write)
{
	unsigned int m;
	int ret = 0;
	int nr_pages;
	int err2;
	ktime_t start;
	ktime_t stop;
	ktime_t start_io;
	size_t off;
	unsigned thr, run_threads, nr_threads;
	unsigned char *page = NULL;
	struct cmp_data *data = NULL;
	struct crc_data *crc = NULL;

	hib_init_batch(&hb);

	atomic_set(&compressed_size, 0);
	atomic_set(&stats_compression_us, 0);

	/*
	 * We'll limit the number of threads for compression to limit memory
	 * footprint.
	 */
	nr_threads = num_online_cpus() - 1;
	nr_threads = clamp_val(nr_threads, 1, CMP_THREADS);
	stats_num_cmp_threads = nr_threads;

	page = (void *)__get_free_page(GFP_NOIO | __GFP_HIGH);
	if (!page) {
		pr_err("Failed to allocate %s page\n", hib_comp_algo);
		ret = -ENOMEM;
		goto out_clean;
	}

	data = vmalloc(array_size(nr_threads, sizeof(*data)));
	if (!data) {
		pr_err("Failed to allocate %s data\n", hib_comp_algo);
		ret = -ENOMEM;
		goto out_clean;
	}
	for (thr = 0; thr < nr_threads; thr++)
		memset(&data[thr], 0, offsetof(struct cmp_data, go));

	crc = kmalloc(sizeof(*crc), GFP_KERNEL);
	if (!crc) {
		pr_err("Failed to allocate crc\n");
		ret = -ENOMEM;
		goto out_clean;
	}
	memset(crc, 0, offsetof(struct crc_data, go));

	/*
	 * Start the compression threads.
	 */
	for (thr = 0; thr < nr_threads; thr++) {
		init_waitqueue_head(&data[thr].go);
		init_waitqueue_head(&data[thr].done);

	data[thr].cc = crypto_alloc_comp(hib_comp_algo, 0, 0);
	if (IS_ERR_OR_NULL(data[thr].cc)) {
		pr_err("Could not allocate comp stream %ld\n", PTR_ERR(data[thr].cc));
		ret = -EFAULT;
		goto out_clean;
	}

	data[thr].thr = kthread_run(compress_threadfn,
		                            &data[thr],
		                            "image_compress/%u", thr);
		if (IS_ERR(data[thr].thr)) {
			data[thr].thr = NULL;
			pr_err("Cannot start compression threads\n");
			ret = -ENOMEM;
			goto out_clean;
		}
	}

	/*
	 * Start the CRC32 thread.
	 */
	init_waitqueue_head(&crc->go);
	init_waitqueue_head(&crc->done);

	handle->crc32 = 0;
	crc->crc32 = &handle->crc32;
	for (thr = 0; thr < nr_threads; thr++) {
		crc->unc[thr] = data[thr].unc;
		crc->unc_len[thr] = &data[thr].unc_len;
	}

	crc->thr = kthread_run(crc32_threadfn, crc, "image_crc32");
	if (IS_ERR(crc->thr)) {
		crc->thr = NULL;
		pr_err("Cannot start CRC32 thread\n");
		ret = -ENOMEM;
		goto out_clean;
	}

	/*
	 * Adjust the number of required free pages after all allocations have
	 * been done. We don't want to run out of pages when writing.
	 */
	handle->reqd_free_pages = reqd_free_pages();

	pr_info("Using %u thread(s) for %s compression\n", nr_threads, hib_comp_algo);
	pr_info("Compressing and saving image data (%u pages)...\n",
		nr_to_write);
	m = nr_to_write / 10;
	if (!m)
		m = 1;
	nr_pages = 0;
	start = ktime_get();
	for (;;) {
		for (thr = 0; thr < nr_threads; thr++) {
			for (off = 0; off < UNC_SIZE; off += PAGE_SIZE) {
				ret = snapshot_read_next(snapshot);
				if (ret < 0)
					goto out_finish;

				if (!ret)
					break;

				memcpy(data[thr].unc + off,
				       data_of(*snapshot), PAGE_SIZE);

				if (!(nr_pages % m))
					pr_info("Image saving progress: %3d%%\n",
						nr_pages / m * 10);
				nr_pages++;
			}
			if (!off)
				break;

			data[thr].unc_len = off;

			atomic_set_release(&data[thr].ready, 1);
			wake_up(&data[thr].go);
		}

		if (!thr)
			break;

		crc->run_threads = thr;
		atomic_set_release(&crc->ready, 1);
		wake_up(&crc->go);

		for (run_threads = thr, thr = 0; thr < run_threads; thr++) {
			wait_event(data[thr].done,
				atomic_read_acquire(&data[thr].stop));
			atomic_set(&data[thr].stop, 0);

			ret = data[thr].ret;

			if (ret < 0) {
				pr_err("%s compression failed\n", hib_comp_algo);
				goto out_finish;
			}

			if (unlikely(!data[thr].cmp_len ||
			             data[thr].cmp_len >
					bytes_worst_compress(data[thr].unc_len))) {
				pr_err("Invalid %s compressed length\n", hib_comp_algo);
				ret = -1;
				goto out_finish;
			}

			*(size_t *)data[thr].cmp = data[thr].cmp_len;


			/*
			 * Given we are writing one page at a time to disk, we
			 * copy that much from the buffer, although the last
			 * bit will likely be smaller than full page. This is
			 * OK - we saved the length of the compressed data, so
			 * any garbage at the end will be discarded when we
			 * read it.
			 */
			for (off = 0;
			     off < CMP_HEADER + data[thr].cmp_len;
			     off += PAGE_SIZE) {
				memcpy(page, data[thr].cmp + off, PAGE_SIZE);

				start_io = ktime_get();
				ret = swap_write_page(handle, page, &hb);
                                stats_async_io_us += ktime_to_us(ktime_sub(ktime_get(), start_io));
				if (ret)
					goto out_finish;
			}
		}

		wait_event(crc->done, atomic_read_acquire(&crc->stop));
		atomic_set(&crc->stop, 0);
	}

out_finish:
	if (!ret)
		trace_android_rvh_post_image_save(root_swap);

	err2 = hib_wait_io(&hb);
	stop = ktime_get();
	if (!ret)
		ret = err2;
	if (!ret)
		pr_info("Image saving done\n");
	swsusp_show_speed(start, stop, nr_to_write, "Wrote");
	pr_info("Image size after compression: %d kbytes\n",
		(atomic_read(&compressed_size) / 1024));

out_clean:
	hib_finish_batch(&hb);
	if (crc) {
		if (crc->thr)
			kthread_stop(crc->thr);
		kfree(crc);
	}
	if (data) {
		for (thr = 0; thr < nr_threads; thr++) {
			if (data[thr].thr)
				kthread_stop(data[thr].thr);
			if (data[thr].cc)
				crypto_free_comp(data[thr].cc);
		}
		vfree(data);
	}
	if (page) free_page((unsigned long)page);

	return ret;
}

/**
 *	enough_swap - Make sure we have enough swap to save the image.
 *
 *	Returns TRUE or FALSE after checking the total amount of swap
 *	space avaiable from the resume partition.
 */

static int enough_swap(unsigned int nr_pages)
{
	unsigned int free_swap = count_swap_pages(root_swap, 1);
	unsigned int required;

	pr_debug("Free swap pages: %u\n", free_swap);

	required = PAGES_FOR_IO + nr_pages;
	return free_swap > required;
}

void swsusp_mark_swapfile(void)
{
	int error;
	struct swap_map_handle handle;

	error = get_swap_writer(&handle);
	if (error) {
		pr_err("Cannot get swap writer\n");
		return;
	}

	hib_submit_io(REQ_OP_READ, 0, swsusp_resume_block, swsusp_header, NULL);
	if (!memcmp("SWAP-SPACE", swsusp_header->sig, 10) ||
	    !memcmp("SWAPSPACE2", swsusp_header->sig, 10)) {
		memcpy(swsusp_header->orig_sig, swsusp_header->sig, 10);
		memcpy(swsusp_header->sig, HIBERNATE_SIG, 10);
		error = hib_submit_io(REQ_OP_WRITE, REQ_SYNC,
				      swsusp_resume_block, swsusp_header, NULL);
	} else {
		pr_err("Swap header signature %s\n incorrect",
		       swsusp_header->sig);
		error = -ENODEV;
	}

	release_swap_writer(&handle);

	if (error)
		pr_err("Could not mark swap file (code %d)\n", error);
	else
		pr_debug("Image signature written correctly \n");

	return;
}

/**
 *	swsusp_write - Write entire image and metadata.
 *	@flags: flags to pass to the "boot" kernel in the image header
 *
 *	It is important _NOT_ to umount filesystems at this point. We want
 *	them synced (in case something goes wrong) but we DO not want to mark
 *	filesystem clean: it is not. (And it does not matter, if we resume
 *	correctly, we'll mark system clean, anyway.)
 */

int swsusp_write(unsigned int flags)
{
	struct swap_map_handle handle;
	struct snapshot_handle snapshot;
	struct swsusp_info *header;
	unsigned long pages;
	int error = 0;

	pages = snapshot_get_image_size();

	/*
	 * The memory allocated by this vendor hook is later freed as part of
	 * PM_POST_HIBERNATION notifier call.
	 */

	error = get_swap_writer(&handle);
	if (error) {
		pr_err("Cannot get swap writer\n");
		hibernate_stats.failed_swap++;
		hib_save_failed_step(HIBERNATE_SWAP);
		return error;
	}
	trace_android_vh_init_aes_encrypt(&hb);
	if (flags & SF_NOCOMPRESS_MODE) {
		if (!enough_swap(pages)) {
			pr_err("Not enough free swap\n");
			error = -ENOSPC;
			goto out_finish;
		}
	}
	memset(&snapshot, 0, sizeof(struct snapshot_handle));
	error = snapshot_read_next(&snapshot);
	if (error < (int)PAGE_SIZE) {
		if (error >= 0)
			error = -EFAULT;

		goto out_finish;
	}
	header = (struct swsusp_info *)data_of(snapshot);
	error = swap_write_page(&handle, header, NULL);
	if (!error) {
		error = (flags & SF_NOCOMPRESS_MODE) ?
			save_image(&handle, &snapshot, pages - 1) :
			save_compressed_image(&handle, &snapshot, pages - 1);
	}
out_finish:
	error = swap_writer_finish(&handle, flags, error);
	return error;
}

/**
 *	The following functions allow us to read data using a swap map
 *	in a file-alike way
 */

static void release_swap_reader(struct swap_map_handle *handle)
{
	struct swap_map_page_list *tmp;

	while (handle->maps) {
		if (handle->maps->map)
			free_page((unsigned long)handle->maps->map);
		tmp = handle->maps;
		handle->maps = handle->maps->next;
		kfree(tmp);
	}
	handle->cur = NULL;
}

static int get_swap_reader(struct swap_map_handle *handle,
		unsigned int *flags_p)
{
	int error;
	struct swap_map_page_list *tmp, *last;
	sector_t offset;

	*flags_p = swsusp_header->flags;

	if (!swsusp_header->image) /* how can this happen? */
		return -EINVAL;

	handle->cur = NULL;
	last = handle->maps = NULL;
	offset = swsusp_header->image;
	while (offset) {
		tmp = kzalloc(sizeof(*handle->maps), GFP_KERNEL);
		if (!tmp) {
			release_swap_reader(handle);
			return -ENOMEM;
		}
		if (!handle->maps)
			handle->maps = tmp;
		if (last)
			last->next = tmp;
		last = tmp;

		tmp->map = (struct swap_map_page *)
			   __get_free_page(GFP_NOIO | __GFP_HIGH);
		if (!tmp->map) {
			release_swap_reader(handle);
			return -ENOMEM;
		}

		error = hib_submit_io(REQ_OP_READ, 0, offset, tmp->map, NULL);
		if (error) {
			release_swap_reader(handle);
			return error;
		}
		offset = tmp->map->next_swap;
	}
	handle->k = 0;
	handle->cur = handle->maps->map;
	return 0;
}

static int swap_read_page(struct swap_map_handle *handle, void *buf,
		struct hib_bio_batch *hb)
{
	sector_t offset;
	int error;
	struct swap_map_page_list *tmp;

	if (!handle->cur)
		return -EINVAL;
	offset = handle->cur->entries[handle->k];
	if (!offset)
		return -EFAULT;
	error = hib_submit_io(REQ_OP_READ, 0, offset, buf, hb);
	if (error)
		return error;
	if (++handle->k >= MAP_PAGE_ENTRIES) {
		handle->k = 0;
		free_page((unsigned long)handle->maps->map);
		tmp = handle->maps;
		handle->maps = handle->maps->next;
		kfree(tmp);
		if (!handle->maps)
			release_swap_reader(handle);
		else
			handle->cur = handle->maps->map;
	}
	return error;
}

static int swap_reader_finish(struct swap_map_handle *handle)
{
	release_swap_reader(handle);

	return 0;
}

/**
 *	load_image - load the image using the swap map handle
 *	@handle and the snapshot handle @snapshot
 *	(assume there are @nr_pages pages to load)
 */

static int load_image(struct swap_map_handle *handle,
                      struct snapshot_handle *snapshot,
                      unsigned int nr_to_read)
{
	unsigned int m;
	int ret = 0;
	ktime_t start;
	ktime_t stop;
	struct hib_bio_batch hb;
	int err2;
	unsigned nr_pages;

	hib_init_batch(&hb);

	clean_pages_on_read = true;
	pr_info("Loading image data pages (%u pages)...\n", nr_to_read);
	m = nr_to_read / 10;
	if (!m)
		m = 1;
	nr_pages = 0;
	start = ktime_get();
	for ( ; ; ) {
		ret = snapshot_write_next(snapshot);
		if (ret <= 0)
			break;
		ret = swap_read_page(handle, data_of(*snapshot), &hb);
		if (ret)
			break;
		if (snapshot->sync_read)
			ret = hib_wait_io(&hb);
		if (ret)
			break;
		if (!(nr_pages % m))
			pr_info("Image loading progress: %3d%%\n",
				nr_pages / m * 10);
		nr_pages++;
	}
	if (!ret)
		trace_android_rvh_post_image_save(root_swap);

	err2 = hib_wait_io(&hb);
	hib_finish_batch(&hb);
	stop = ktime_get();
	if (!ret)
		ret = err2;
	if (!ret) {
		pr_info("Image loading done\n");
		snapshot_write_finalize(snapshot);
		if (!snapshot_image_loaded(snapshot))
			ret = -ENODATA;
	}
	swsusp_show_speed(start, stop, nr_to_read, "Read");
	return ret;
}

/**
 * Structure used for data decompression.
 */
struct dec_data {
	struct task_struct *thr;                  /* thread */
	struct crypto_comp *cc;          /* crypto compressor stream */
	atomic_t ready;                           /* ready to start flag */
	atomic_t stop;                            /* ready to stop flag */
	int ret;                                  /* return code */
	wait_queue_head_t go;                     /* start decompression */
	wait_queue_head_t done;                   /* decompression done */
	size_t unc_len;                           /* uncompressed length */
	size_t cmp_len;                           /* compressed length */
	unsigned char unc[UNC_SIZE];       /* uncompressed buffer */
	unsigned char cmp[CMP_SIZE];       /* compressed buffer */
};

/**
 * Deompression function that runs in its own thread.
 */
static int decompress_threadfn(void *data)
{
	struct dec_data *d = data;
	unsigned int unc_len = 0;

	while (1) {
		wait_event(d->go, atomic_read_acquire(&d->ready) ||
		                  kthread_should_stop());
		if (kthread_should_stop()) {
			d->thr = NULL;
			d->ret = -1;
			atomic_set_release(&d->stop, 1);
			wake_up(&d->done);
			break;
		}
		atomic_set(&d->ready, 0);

		unc_len = UNC_SIZE;
		d->ret = crypto_comp_decompress(d->cc, d->cmp + CMP_HEADER, d->cmp_len,
			d->unc, &unc_len);
			d->unc_len = unc_len;

		if (clean_pages_on_decompress)
			flush_icache_range((unsigned long)d->unc,
					   (unsigned long)d->unc + d->unc_len);

		atomic_set_release(&d->stop, 1);
		wake_up(&d->done);
	}
	return 0;
}

/**
 * load_compressed_image - Load compressed image data and decompress it.
 * @handle: Swap map handle to use for loading data.
 * @snapshot: Image to copy uncompressed data into.
 * @nr_to_read: Number of pages to load.
 */
static int load_compressed_image(struct swap_map_handle *handle,
			struct snapshot_handle *snapshot,
			unsigned int nr_to_read)
{
	unsigned int m;
	int ret = 0;
	int eof = 0;
	struct hib_bio_batch hb;
	ktime_t start;
	ktime_t stop;
	unsigned nr_pages;
	size_t off;
	unsigned i, thr, run_threads, nr_threads;
	unsigned ring = 0, pg = 0, ring_size = 0,
	         have = 0, want, need, asked = 0;
	unsigned long read_pages = 0;
	unsigned char **page = NULL;
	struct dec_data *data = NULL;
	struct crc_data *crc = NULL;
	atomic_t first_read;

	atomic_set(&first_read, 1);
	hib_init_batch(&hb);

	/*
	 * We'll limit the number of threads for decompression to limit memory
	 * footprint.
	 */
	nr_threads = num_online_cpus() - 1;
	nr_threads = clamp_val(nr_threads, 1, CMP_THREADS);

	page = vmalloc(array_size(CMP_MAX_RD_PAGES, sizeof(*page)));
	if (!page) {
		pr_err("Failed to allocate %s page\n", hib_comp_algo);
		ret = -ENOMEM;
		goto out_clean;
	}

	data = vmalloc(array_size(nr_threads, sizeof(*data)));
	if (!data) {
		pr_err("Failed to allocate %s data\n", hib_comp_algo);
		ret = -ENOMEM;
		goto out_clean;
	}
	for (thr = 0; thr < nr_threads; thr++)
		memset(&data[thr], 0, offsetof(struct dec_data, go));

	crc = kmalloc(sizeof(*crc), GFP_KERNEL);
	if (!crc) {
		pr_err("Failed to allocate crc\n");
		ret = -ENOMEM;
		goto out_clean;
	}
	memset(crc, 0, offsetof(struct crc_data, go));

	clean_pages_on_decompress = true;

	/*
	 * Start the decompression threads.
	 */
	for (thr = 0; thr < nr_threads; thr++) {
		init_waitqueue_head(&data[thr].go);
		init_waitqueue_head(&data[thr].done);

		data[thr].cc = crypto_alloc_comp(hib_comp_algo, 0, 0);
		if (IS_ERR_OR_NULL(data[thr].cc)) {
			pr_err("Could not allocate comp stream %ld\n", PTR_ERR(data[thr].cc));
			ret = -EFAULT;
			goto out_clean;
		}

		data[thr].thr = kthread_run(decompress_threadfn,
		                            &data[thr],
		                            "image_decompress/%u", thr);
		if (IS_ERR(data[thr].thr)) {
			data[thr].thr = NULL;
			pr_err("Cannot start decompression threads\n");
			ret = -ENOMEM;
			goto out_clean;
		}
	}

	/*
	 * Start the CRC32 thread.
	 */
	init_waitqueue_head(&crc->go);
	init_waitqueue_head(&crc->done);

	handle->crc32 = 0;
	crc->crc32 = &handle->crc32;
	for (thr = 0; thr < nr_threads; thr++) {
		crc->unc[thr] = data[thr].unc;
		crc->unc_len[thr] = &data[thr].unc_len;
	}

	crc->thr = kthread_run(crc32_threadfn, crc, "image_crc32");
	if (IS_ERR(crc->thr)) {
		crc->thr = NULL;
		pr_err("Cannot start CRC32 thread\n");
		ret = -ENOMEM;
		goto out_clean;
	}

	/*
	 * Set the number of pages for read buffering.
	 * This is complete guesswork, because we'll only know the real
	 * picture once prepare_image() is called, which is much later on
	 * during the image load phase. We'll assume the worst case and
	 * say that none of the image pages are from high memory.
	 */
	if (low_free_pages() > snapshot_get_image_size())
		read_pages = (low_free_pages() - snapshot_get_image_size()) / 2;
	read_pages = clamp_val(read_pages, CMP_MIN_RD_PAGES, CMP_MAX_RD_PAGES);

	for (i = 0; i < read_pages; i++) {
		page[i] = (void *)__get_free_page(i < CMP_PAGES ?
						  GFP_NOIO | __GFP_HIGH :
						  GFP_NOIO | __GFP_NOWARN |
						  __GFP_NORETRY);

		if (!page[i]) {
			if (i < CMP_PAGES) {
				ring_size = i;
				pr_err("Failed to allocate %s pages\n", hib_comp_algo);
				ret = -ENOMEM;
				goto out_clean;
			} else {
				break;
			}
		}
	}
	want = ring_size = i;

	pr_info("Using %u thread(s) for %s decompression\n", nr_threads, hib_comp_algo);
	pr_info("Loading and decompressing image data (%u pages)...\n",
		nr_to_read);
	m = nr_to_read / 10;
	if (!m)
		m = 1;
	nr_pages = 0;
	start = ktime_get();

	ret = snapshot_write_next(snapshot);
	if (ret <= 0)
		goto out_finish;

	for(;;) {
		for (i = 0; !eof && i < want; i++) {
			ret = swap_read_page(handle, page[ring], &hb);
			if (ret) {
				/*
				 * On real read error, finish. On end of data,
				 * set EOF flag and just exit the read loop.
				 */
				if (handle->cur &&
				    handle->cur->entries[handle->k]) {
					goto out_finish;
				} else {
					eof = 1;
					break;
				}
			}
			if (atomic_read_acquire(&first_read)) {
				hib_wait_io(&hb);
				atomic_set_release(&first_read, 0);
			}
			if (++ring >= ring_size)
				ring = 0;
		}
		asked += i;
		want -= i;

		/*
		 * We are out of data, wait for some more.
		 */
		if (!have) {
			if (!asked)
				break;

			ret = hib_wait_io(&hb);
			if (ret)
				goto out_finish;
			have += asked;
			asked = 0;
			if (eof)
				eof = 2;
		}

		if (crc->run_threads) {
			wait_event(crc->done, atomic_read_acquire(&crc->stop));
			atomic_set(&crc->stop, 0);
			crc->run_threads = 0;
		}

		for (thr = 0; have && thr < nr_threads; thr++) {
			data[thr].cmp_len = *(size_t *)page[pg];
			if (unlikely(!data[thr].cmp_len ||
			             data[thr].cmp_len >
				     bytes_worst_compress(UNC_SIZE))) {
				pr_err("Invalid %s compressed length\n", hib_comp_algo);
				ret = -1;
				goto out_finish;
			}

			need = DIV_ROUND_UP(data[thr].cmp_len + CMP_HEADER,
			                    PAGE_SIZE);
			if (need > have) {
				if (eof > 1) {
					ret = -1;
					goto out_finish;
				}
				break;
			}

			for (off = 0;
			     off < CMP_HEADER + data[thr].cmp_len;
			     off += PAGE_SIZE) {
				memcpy(data[thr].cmp + off,
				       page[pg], PAGE_SIZE);
				have--;
				want++;
				if (++pg >= ring_size)
					pg = 0;
			}

			atomic_set_release(&data[thr].ready, 1);
			wake_up(&data[thr].go);
		}

		/*
		 * Wait for more data while we are decompressing.
		 */
		if (have < CMP_PAGES && asked) {
			ret = hib_wait_io(&hb);
			if (ret)
				goto out_finish;
			have += asked;
			asked = 0;
			if (eof)
				eof = 2;
		}

		for (run_threads = thr, thr = 0; thr < run_threads; thr++) {
			wait_event(data[thr].done,
				atomic_read_acquire(&data[thr].stop));
			atomic_set(&data[thr].stop, 0);

			ret = data[thr].ret;

			if (ret < 0) {
				pr_err("%s decompression failed\n", hib_comp_algo);
				goto out_finish;
			}

			if (unlikely(!data[thr].unc_len ||
				    data[thr].unc_len > UNC_SIZE ||
				    data[thr].unc_len & (PAGE_SIZE - 1))) {
				pr_err("Invalid %s uncompressed length\n", hib_comp_algo);
				ret = -1;
				goto out_finish;
			}

			for (off = 0;
			     off < data[thr].unc_len; off += PAGE_SIZE) {
				memcpy(data_of(*snapshot),
				       data[thr].unc + off, PAGE_SIZE);

				if (!(nr_pages % m))
					pr_info("Image loading progress: %3d%%\n",
						nr_pages / m * 10);
				nr_pages++;

				ret = snapshot_write_next(snapshot);
				if (ret <= 0) {
					crc->run_threads = thr + 1;
					atomic_set_release(&crc->ready, 1);
					wake_up(&crc->go);
					goto out_finish;
				}
			}
		}

		crc->run_threads = thr;
		atomic_set_release(&crc->ready, 1);
		wake_up(&crc->go);
	}

out_finish:
	if (crc->run_threads) {
		wait_event(crc->done, atomic_read_acquire(&crc->stop));
		atomic_set(&crc->stop, 0);
	}
	stop = ktime_get();
	if (!ret) {
		pr_info("Image loading done\n");
		snapshot_write_finalize(snapshot);
		if (!snapshot_image_loaded(snapshot))
			ret = -ENODATA;
		if (!ret) {
			if (swsusp_header->flags & SF_CRC32_MODE) {
				if(handle->crc32 != swsusp_header->crc32) {
					pr_err("Invalid image CRC32!\n");
					ret = -ENODATA;
				}
			}
		}
	} else {
		panic("Failed loading image %d\n", ret);
	}
	swsusp_show_speed(start, stop, nr_to_read, "Read");
	trace_android_vh_store_auth_slot_num(NULL);
out_clean:
	hib_finish_batch(&hb);
	for (i = 0; i < ring_size; i++)
		free_page((unsigned long)page[i]);
	if (crc) {
		if (crc->thr)
			kthread_stop(crc->thr);
		kfree(crc);
	}
	if (data) {
		for (thr = 0; thr < nr_threads; thr++) {
			if (data[thr].thr)
				kthread_stop(data[thr].thr);
			if (data[thr].cc)
				crypto_free_comp(data[thr].cc);
		}
		vfree(data);
	}
	vfree(page);

	return ret;
}

/**
 *	swsusp_read - read the hibernation image.
 *	@flags_p: flags passed by the "frozen" kernel in the image header should
 *		  be written into this memory location
 */

int swsusp_read(unsigned int *flags_p)
{
	int error;
	struct swap_map_handle handle;
	struct snapshot_handle snapshot;
	struct swsusp_info *header;

	memset(&snapshot, 0, sizeof(struct snapshot_handle));
	error = snapshot_write_next(&snapshot);
	if (error < (int)PAGE_SIZE)
		return error < 0 ? error : -EFAULT;
	header = (struct swsusp_info *)data_of(snapshot);
	error = get_swap_reader(&handle, flags_p);
	if (error)
		goto end;
	if (!error)
		error = swap_read_page(&handle, header, NULL);
	if (!error) {
		error = (*flags_p & SF_NOCOMPRESS_MODE) ?
			load_image(&handle, &snapshot, header->pages - 1) :
			load_compressed_image(&handle, &snapshot, header->pages - 1);
	}
	swap_reader_finish(&handle);
end:
	if (!error)
		pr_debug("Image successfully loaded\n");
	else
		pr_debug("Error %d resuming\n", error);
	return error;
}

/**
 *      swsusp_check - Check for swsusp signature in the resume device
 */

int swsusp_check(void)
{
	int error;
	void *holder;

	hib_resume_bdev = blkdev_get_by_dev(swsusp_resume_device,
					    FMODE_READ | FMODE_EXCL, &holder);
	if (!IS_ERR(hib_resume_bdev)) {
		set_blocksize(hib_resume_bdev, PAGE_SIZE);
		clear_page(swsusp_header);
		swsusp_header = (struct swsusp_header *)__get_free_page(GFP_NOIO | __GFP_NOWARN |
                                                __GFP_NORETRY);
		error = hib_submit_io(REQ_OP_READ, 0,
					swsusp_resume_block,
					swsusp_header, NULL);
		if (error)
			goto put;

		if (!memcmp(HIBERNATE_SIG, swsusp_header->sig, 10)) {
			memcpy(swsusp_header->sig, swsusp_header->orig_sig, 10);
			swsusp_header_flags = swsusp_header->flags;
			swap_auth_slot_offset =  ((uint32_t *)(swsusp_header->reserved))[AUTH_SLOT];
			/* Reset swap signature now */
			error = hib_submit_io(REQ_OP_WRITE, REQ_SYNC,
						swsusp_resume_block,
						swsusp_header, NULL);
		} else {
			error = -EINVAL;
		}

put:
		if (error)
			blkdev_put(hib_resume_bdev, FMODE_READ | FMODE_EXCL);
		else
			pr_debug("Image signature found, resuming\n");
	} else {
		error = PTR_ERR(hib_resume_bdev);
	}

	if (error)
		pr_debug("Image not found (code %d)\n", error);

	return error;
}

/**
 *      swsus_pread_stats - Read back statistics stored during hibernation entry
 */

int swsusp_read_stats(struct hib_entry_stats *stats)
{
	int error;
	void *holder;

	if (!stats)
		return -EINVAL;

	hib_resume_bdev = blkdev_get_by_dev(swsusp_resume_device,
					FMODE_READ, &holder);
	if (IS_ERR(hib_resume_bdev))
		return PTR_ERR(hib_resume_bdev);

	set_blocksize(hib_resume_bdev, PAGE_SIZE);
	clear_page(swsusp_header);
	error = hib_submit_io(REQ_OP_READ, 0, swsusp_resume_block,
					swsusp_header, NULL);
	if (!error)
		*stats = swsusp_header->stats;

	blkdev_put(hib_resume_bdev, FMODE_READ);
	return error;
}

void populate_hib_entry_stats(struct hib_entry_stats *stats) {
	unsigned int userspace_us = 0;
	stats->kernel_total_us = ktime_to_us(ktime_sub(ktime_get(), stats_hib_entry_kernel_start));
	stats->uptime_at_hibentry_s = ktime_get_boottime_seconds();
	stats->hib_image_size_kb = atomic_read(&compressed_size) / 1024;
	stats->preallocation_us = stats_preallocation_us;
	stats->preallocation_pages = stats_preallocation_pages;
 	stats->encryption_us = stats_encryption_us;
	stats->compression_avg_us = atomic_read(&stats_compression_us) / stats_num_cmp_threads;
	stats->async_io_us = stats_async_io_us;
	stats->consecutive_hib_entries = stats_consecutive_hib_entries + 1;
	stats->lifetime_hibdata_written_mb += stats->hib_image_size_kb / 1024;
	if (stats_hib_entry_userspace_start) {
		userspace_us = ktime_to_us(ktime_sub(stats_hib_entry_kernel_start, stats_hib_entry_userspace_start));
	}
	stats->userspace_total_us = userspace_us;
}



/**
 *	swsusp_close - close swap device.
 */

void swsusp_close(fmode_t mode)
{
	if (IS_ERR(hib_resume_bdev)) {
		pr_debug("Image device not initialised\n");
		return;
	}

	blkdev_put(hib_resume_bdev, mode);
}

/**
 *      swsusp_unmark - Unmark swsusp signature in the resume device
 */

#ifdef CONFIG_SUSPEND
int swsusp_unmark(void)
{
	int error;

	hib_submit_io(REQ_OP_READ, 0, swsusp_resume_block,
		      swsusp_header, NULL);
	if (!memcmp(HIBERNATE_SIG,swsusp_header->sig, 10)) {
		memcpy(swsusp_header->sig,swsusp_header->orig_sig, 10);
		error = hib_submit_io(REQ_OP_WRITE, REQ_SYNC,
					swsusp_resume_block,
					swsusp_header, NULL);
	} else {
		pr_err("Cannot find swsusp signature!\n");
		error = -ENODEV;
	}

	/*
	 * We just returned from suspend, we don't need the image any more.
	 */
	free_all_swap_pages(root_swap);

	return error;
}
#endif

#ifdef CONFIG_DEBUG_FS
static int __init swsusp_toggle_bit_init(void)
{
	struct dentry *dswsusp, *dtoggle_bit;
	dswsusp = debugfs_create_dir("swsusp", NULL);
	if (IS_ERR(dswsusp))
		return PTR_ERR(dswsusp);
	dtoggle_bit = debugfs_create_ulong("toggle_bit", 0644, dswsusp, &toggle_bit);
	if (IS_ERR(dswsusp))
		return PTR_ERR(dswsusp);
	return 0;
}
postcore_initcall(swsusp_toggle_bit_init);
#endif

static int __init swsusp_header_init(void)
{
	swsusp_header = (struct swsusp_header*) __get_free_page(GFP_KERNEL);
	if (!swsusp_header)
		panic("Could not allocate memory for swsusp_header\n");
	return 0;
}

core_initcall(swsusp_header_init);
