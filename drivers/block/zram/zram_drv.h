/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *               2012, 2013 Minchan Kim
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 */

#ifndef _ZRAM_DRV_H_
#define _ZRAM_DRV_H_

#include <linux/rwsem.h>
#include <linux/zsmalloc.h>
#include <linux/crypto.h>

#include "zcomp.h"

#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)
#define ZRAM_LOGICAL_BLOCK_SHIFT 12
#define ZRAM_LOGICAL_BLOCK_SIZE	(1 << ZRAM_LOGICAL_BLOCK_SHIFT)
#define ZRAM_SECTOR_PER_LOGICAL_BLOCK	\
	(1 << (ZRAM_LOGICAL_BLOCK_SHIFT - SECTOR_SHIFT))


/*
 * The lower ZRAM_FLAG_SHIFT bits of table.flags is for
 * object size (excluding header), the higher bits is for
 * zram_pageflags.
 *
 * zram is mainly used for memory efficiency so we want to keep memory
 * footprint small so we can squeeze size and flags into a field.
 * The lower ZRAM_FLAG_SHIFT bits is for object size (excluding header),
 * the higher bits is for zram_pageflags.
 */
#define ZRAM_FLAG_SHIFT 24

/* Flags for zram pages (table[page_no].flags) */
enum zram_pageflags {
	/* zram slot is locked */
	ZRAM_LOCK = ZRAM_FLAG_SHIFT,
	ZRAM_SAME,	/* Page consists the same element */
	ZRAM_WB,	/* page is stored on backing_device */
	ZRAM_UNDER_WB,	/* page is under writeback */
	ZRAM_HUGE,	/* Incompressible page */
	ZRAM_IDLE,	/* not accessed page since last idle marking */
	ZRAM_SIM_WB,	/* page is stored on simulated backing device */

	__NR_ZRAM_PAGEFLAGS,
};

/*-- Data structures */

/* Allocated for each disk page */
struct zram_table_entry {
	union {
		unsigned long handle;
		unsigned long element;
	};
	unsigned long flags;
#if defined(CONFIG_ZRAM_MEMORY_TRACKING) || \
    defined(CONFIG_ZRAM_IDLE_HISTOGRAM) ||  \
    defined(CONFIG_ZRAM_SIMULATE_WRITEBACK_STATS)
	ktime_t ac_time;
#endif
};

struct zram_stats {
	atomic64_t compr_data_size;	/* compressed size of pages stored */
	atomic64_t num_reads;	/* failed + successful */
	atomic64_t num_writes;	/* --do-- */
	atomic64_t failed_reads;	/* can happen when memory is too low */
	atomic64_t failed_writes;	/* can happen when memory is too low */
	atomic64_t invalid_io;	/* non-page-aligned I/O requests */
	atomic64_t notify_free;	/* no. of swap slot free notifications */
	atomic64_t same_pages;		/* no. of same element filled pages */
	atomic64_t huge_pages;		/* no. of huge pages */
	atomic64_t pages_stored;	/* no. of pages currently stored */
	atomic_long_t max_used_pages;	/* no. of maximum pages stored */
	atomic64_t writestall;		/* no. of write slow paths */
	atomic64_t miss_free;		/* no. of missed free */
#ifdef	CONFIG_ZRAM_WRITEBACK
	atomic64_t bd_count;		/* no. of pages in backing device */
	atomic64_t bd_reads;		/* no. of reads from backing device */
	atomic64_t bd_writes;		/* no. of writes from backing device */
#endif
#ifdef CONFIG_ZRAM_SIMULATE_WRITEBACK_STATS
	atomic64_t simulated_bd_count;	/* pgs in backing dev if configured */
	atomic64_t simulated_bd_reads;	/* pgs read from simulated backing dev */
	atomic64_t simulated_bd_writes;	/* pgs written to simulated backing dev */
	atomic64_t simulated_bd_stats_time; /* usec to calculate simulated bd stats */
#endif
#ifdef CONFIG_ZRAM_IDLE_HISTOGRAM
	/* Prevent concurrent read and write of idle page histograms */
	struct rw_semaphore idle_histo_lock;
	/*
	 * An exponential histogram of page IDLE time. First bucket is
	 * 1min wide. Exponent is 2, giving us buckets that begin at 0, 2, 4,
	 * 8, 16 ... mins. Do not reduce size of histogram, that may break an
	 * assumption in code to speed up calculation.
	 */
	#define IDLE_AGE_HISTOGRAM_SZ		(64)
	int idle_pg_histo[IDLE_AGE_HISTOGRAM_SZ];
	/*
	 * An exponential histogram of number of sequential Idle pages.
	 * First bucket is 4 pgs wide. Exponent is 4, giving us buckets that
	 * begin at 0, 4, 16, 64, 128... pages. Do not reduce size of histogram,
	 * that may break an assumption in code to speed up calculation.
	 */
	#define IDLE_CLUSTER_HISTOGRAM_SZ	(32)
	int idle_cluster_sz_histo[IDLE_CLUSTER_HISTOGRAM_SZ];
#endif
};

struct zram {
	struct zram_table_entry *table;
	struct zs_pool *mem_pool;
	struct zcomp *comp;
	struct gendisk *disk;
	/* Prevent concurrent execution of device init */
	struct rw_semaphore init_lock;
	/*
	 * the number of pages zram can consume for storing compressed data
	 */
	unsigned long limit_pages;

	struct zram_stats stats;
	/*
	 * This is the limit on amount of *uncompressed* worth of data
	 * we can store in a disk.
	 */
	u64 disksize;	/* bytes */
	char compressor[CRYPTO_MAX_ALG_NAME];
	/*
	 * zram is claimed so open request will be failed
	 */
	bool claim; /* Protected by bdev->bd_mutex */
	struct file *backing_dev;
#ifdef CONFIG_ZRAM_WRITEBACK
	spinlock_t wb_limit_lock;
	bool wb_limit_enable;
	u64 bd_wb_limit;
	struct block_device *bdev;
	unsigned int old_block_size;
	unsigned long *bitmap;
	unsigned long nr_pages;
#endif
#if defined(CONFIG_ZRAM_SIMULATE_WRITEBACK_STATS) || defined(CONFIG_ZRAM_IDLE_HISTOGRAM)
	/*
	 * Pages not read/written for longer than idle_age_nsec are considered
	 * IDLE and shall be considered written to WRITE_BACK device. Doing
	 * this will allow us to track the number of WRITES to the flash device
	 * without actually enabling writeback
	 */
	u64 idle_age_nsec;
#endif
#ifdef CONFIG_ZRAM_MEMORY_TRACKING
	struct dentry *debugfs_dir;
#endif
};
#endif
