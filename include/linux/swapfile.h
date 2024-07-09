/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SWAPFILE_H
#define _LINUX_SWAPFILE_H

/*
 * these were static in swapfile.c but frontswap.c needs them and we don't
 * want to expose them to the dozens of source files that include swap.h
 */
extern spinlock_t swap_lock;
extern struct plist_head swap_active_head;
extern spinlock_t swap_avail_lock;
extern struct plist_head *swap_avail_heads;
extern struct swap_info_struct *swap_info[];
extern int try_to_unuse(unsigned int, bool, unsigned long);
extern unsigned long generic_max_swapfile_size(void);
extern unsigned long max_swapfile_size(void);
extern int swap_ratio(struct swap_info_struct **si, int node);
extern void setup_swap_ratio(struct swap_info_struct *p, int prio);
extern bool is_swap_ratio_group(int prio);

#endif /* _LINUX_SWAPFILE_H */
