#ifndef __BACKPORT_SPINLOCK_H
#define __BACKPORT_SPINLOCK_H
#include_next <linux/spinlock.h>

#if LINUX_VERSION_IS_LESS(4,16,0)
int alloc_bucket_spinlocks(spinlock_t **locks, unsigned int *lock_mask,
			   size_t max_size, unsigned int cpu_mult,
			   gfp_t gfp);

void free_bucket_spinlocks(spinlock_t *locks);
#endif /* LINUX_VERSION_IS_LESS(4,16,0) */

#endif /* __BACKPORT_SPINLOCK_H */
