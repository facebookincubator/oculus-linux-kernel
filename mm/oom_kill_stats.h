/*
 *  linux/mm/oom_kill_stats.c
 *
 *  Util functions to keep tracking of oom kill victims.
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

/**
 * Add a victim information to track. Max capacity is defined as MAX_VICTIMS
 * If max capacity has reached before adding a new victim, the earliest victim will be evicted.
 */
void add_victim(struct task_struct *victim);

/**
 * Dump all victims chronologically to the given file handle. Each victim prints out a line of
 * (process_name time total_vm anon_rss file_rSS shmem_rSS oom_score_adj).
 * Then a boolean flag is printed to indicate if the buffer is overflown or not.
 * This flag can be used to tune the MAX_VICTIMS.
 */
void dump_victims(struct seq_file *file, bool deleteAfterDump);
