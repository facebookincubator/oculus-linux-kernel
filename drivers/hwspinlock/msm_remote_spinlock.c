/* Copyright (c) 2008-2009, 2011-2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/msm_remote_spinlock.h>
#include <linux/slab.h>

#include <soc/qcom/smem.h>

/**
 * The local processor (APPS) is PID 0, but because 0 is reserved for an empty
 * lock, the value PID + 1 is used as the APSS token when writing to the lock.
 */
#define SPINLOCK_TOKEN_APPS 1

static int is_hw_lock_type;
static DEFINE_MUTEX(ops_init_lock);

struct spinlock_ops {
	void (*lock)(raw_remote_spinlock_t *lock);
	void (*unlock)(raw_remote_spinlock_t *lock);
	int (*trylock)(raw_remote_spinlock_t *lock);
	int (*release)(raw_remote_spinlock_t *lock, uint32_t pid);
	int (*owner)(raw_remote_spinlock_t *lock);
	void (*lock_rlock_id)(raw_remote_spinlock_t *lock, uint32_t tid);
	void (*unlock_rlock)(raw_remote_spinlock_t *lock);
	int (*get_hw_spinlocks_element)(raw_remote_spinlock_t *lock);
};

static struct spinlock_ops current_ops;

static int remote_spinlock_init_address(int id, _remote_spinlock_t *lock);

/* ldrex implementation ----------------------------------------------------- */
static char *ldrex_compatible_string = "qcom,ipc-spinlock-ldrex";

#ifdef CONFIG_ARM
static void __raw_remote_ex_spin_lock(raw_remote_spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"1:     ldrex   %0, [%1]\n"
"       teq     %0, #0\n"
"       strexeq %0, %2, [%1]\n"
"       teqeq   %0, #0\n"
"       bne     1b"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (SPINLOCK_TOKEN_APPS)
	: "cc");

	/*
	 * Ensure the ordering of read/write operations to ensure the
	 * proper ownership of the lock during the lock/unlock operations
	 */
	smp_mb();
}

static int __raw_remote_ex_spin_trylock(raw_remote_spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"       ldrex   %0, [%1]\n"
"       teq     %0, #0\n"
"       strexeq %0, %2, [%1]\n"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (SPINLOCK_TOKEN_APPS)
	: "cc");

	if (tmp == 0) {
		/*
		 * Ensure the ordering of read/write operations to ensure the
		 * proper ownership of the lock during the lock/unlock
		 * operations
		 */
		smp_mb();
		return 1;
	}
	return 0;
}

static void __raw_remote_ex_spin_unlock(raw_remote_spinlock_t *lock)
{
	int lock_owner;

	/*
	 * Ensure the ordering of read/write operations to ensure the
	 * proper ownership of the lock during the lock/unlock operations
	 */
	smp_mb();
	lock_owner = readl_relaxed(&lock->lock);
	if (lock_owner != SPINLOCK_TOKEN_APPS) {
		pr_err("%s: spinlock not owned by Apps (actual owner is %d)\n",
				__func__, lock_owner);
	}

	__asm__ __volatile__(
"       str     %1, [%0]\n"
	:
	: "r" (&lock->lock), "r" (0)
	: "cc");
}
#else
static void __raw_remote_ex_spin_lock(raw_remote_spinlock_t *lock)
{
}

static int __raw_remote_ex_spin_trylock(raw_remote_spinlock_t *lock)
{
	return 0;
}

static void __raw_remote_ex_spin_unlock(raw_remote_spinlock_t *lock)
{
}
#endif /* CONFIG_ARM */
/* end ldrex implementation ------------------------------------------------- */

/* sfpb implementation ------------------------------------------------------ */
static uint32_t lock_count;
static phys_addr_t reg_base;
static uint32_t reg_size;
static uint32_t lock_offset; /* offset into the hardware block before lock 0 */
static uint32_t lock_size;

static void *hw_mutex_reg_base;
static DEFINE_MUTEX(hw_map_init_lock);
static int *hw_spinlocks;

static char *sfpb_compatible_string = "qcom,ipc-spinlock-sfpb";

static int init_hw_mutex(struct device_node *node)
{
	struct resource r;
	int rc;

	rc = of_address_to_resource(node, 0, &r);
	if (rc)
		BUG();

	rc = of_property_read_u32(node, "qcom,num-locks", &lock_count);
	if (rc)
		BUG();

	reg_base = r.start;
	reg_size = (uint32_t)(resource_size(&r));
	lock_offset = 0;
	lock_size = reg_size / lock_count;

	return 0;
}

static void find_and_init_hw_mutex(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, sfpb_compatible_string);
	BUG_ON(node == NULL);
	init_hw_mutex(node);
	hw_mutex_reg_base = ioremap(reg_base, reg_size);
	BUG_ON(hw_mutex_reg_base == NULL);
	hw_spinlocks = kcalloc(lock_count, sizeof(int), GFP_KERNEL);
	BUG_ON(hw_spinlocks == NULL);
}

static int remote_spinlock_init_address_hw(int id, _remote_spinlock_t *lock)
{
	/*
	 * Optimistic locking.  Init only needs to be done once by the first
	 * caller.  After that, serializing inits between different callers
	 * is unnecessary.  The second check after the lock ensures init
	 * wasn't previously completed by someone else before the lock could
	 * be grabbed.
	 */
	if (!hw_mutex_reg_base) {
		mutex_lock(&hw_map_init_lock);
		if (!hw_mutex_reg_base)
			find_and_init_hw_mutex();
		mutex_unlock(&hw_map_init_lock);
	}

	if (id >= lock_count)
		return -EINVAL;

	*lock = hw_mutex_reg_base + lock_offset + id * lock_size;
	return 0;
}

static unsigned int remote_spinlock_get_lock_id(raw_remote_spinlock_t *lock)
{
	unsigned int id;

	BUG_ON((uintptr_t)lock < (uintptr_t)hw_mutex_reg_base);
	BUG_ON(((uintptr_t)lock - (uintptr_t)hw_mutex_reg_base) < lock_offset);

	id = (unsigned int)((uintptr_t)lock - (uintptr_t)hw_mutex_reg_base -
			lock_offset) / lock_size;
	BUG_ON(id >= lock_count);
	return id;
}

static void __raw_remote_sfpb_spin_lock(raw_remote_spinlock_t *lock)
{
	int owner;
	unsigned int id = remote_spinlock_get_lock_id(lock);

	/*
	 * Wait for other local processor task to release spinlock if it
	 * already has the remote spinlock locked.  This can only happen in
	 * test cases since the local spinlock will prevent this when using the
	 * public APIs.
	 */
	while (readl_relaxed(lock) == SPINLOCK_TOKEN_APPS)
		;

	/* acquire remote spinlock */
	do {
		writel_relaxed(SPINLOCK_TOKEN_APPS, lock);
		/*
		 * Ensure the ordering of read/write operations to ensure the
		 * proper ownership of the lock during the lock/unlock
		 * operations
		 */
		smp_mb();
		owner = readl_relaxed(lock);
		hw_spinlocks[id] = owner;
	} while (owner != SPINLOCK_TOKEN_APPS);
}

static int __raw_remote_sfpb_spin_trylock(raw_remote_spinlock_t *lock)
{
	int owner;
	unsigned int id = remote_spinlock_get_lock_id(lock);
	/*
	 * If the local processor owns the spinlock, return failure. This can
	 * only happen in test cases since the local spinlock will prevent this
	 * when using the public APIs.
	 */
	if (readl_relaxed(lock) == SPINLOCK_TOKEN_APPS)
		return 0;

	writel_relaxed(SPINLOCK_TOKEN_APPS, lock);
	/*
	 * Ensure the ordering of read/write operations to ensure the
	 * proper ownership of the lock during the lock/unlock operations
	 */
	smp_mb();
	owner = readl_relaxed(lock);
	hw_spinlocks[id] = owner;
	return owner == SPINLOCK_TOKEN_APPS;
}

static void __raw_remote_sfpb_spin_unlock(raw_remote_spinlock_t *lock)
{
	int lock_owner;

	lock_owner = readl_relaxed(lock);
	if (lock_owner != SPINLOCK_TOKEN_APPS) {
		pr_err("%s: spinlock not owned by Apps (actual owner is %d)\n",
				__func__, lock_owner);
	}

	writel_relaxed(0, lock);
	/*
	 * Ensure the ordering of read/write operations to ensure the
	 * proper ownership of the lock during the lock/unlock operations
	 */
	smp_mb();
}

static void __raw_remote_sfpb_spin_lock_rlock_id(raw_remote_spinlock_t *lock,
						 uint32_t tid)
{
	if (unlikely(!tid)) {
		pr_err("%s: unsupported rlock tid=0\n", __func__);
		BUG();
	}

	do {
		writel_relaxed(tid, lock);
		/*
		 * Ensure the ordering of read/write operations to ensure the
		 * proper ownership of the lock during the lock/unlock
		 * operations
		 */
		smp_mb();
	} while (readl_relaxed(lock) != tid);
}

static void __raw_remote_sfpb_spin_unlock_rlock(raw_remote_spinlock_t *lock)
{
	writel_relaxed(0, lock);
	/*
	 * Ensure the ordering of read/write operations to ensure the
	 * proper ownership of the lock during the lock/unlock operations
	 */
	smp_mb();
}

static int __raw_remote_sfpb_get_hw_spinlocks_element(
		raw_remote_spinlock_t *lock)
{
	return hw_spinlocks[remote_spinlock_get_lock_id(lock)];
}

/* end sfpb implementation -------------------------------------------------- */

/* common spinlock API ------------------------------------------------------ */
/**
 * Release spinlock if it is owned by @pid.
 *
 * This is only to be used for situations where the processor owning
 * the spinlock has crashed and the spinlock must be released.
 *
 * @lock: lock structure
 * @pid: processor ID of processor to release
 */
static int __raw_remote_gen_spin_release(raw_remote_spinlock_t *lock,
		uint32_t pid)
{
	int ret = 1;

	/*
	 * Since 0 is reserved for an empty lock and the PIDs start at 0, the
	 * value PID + 1 is written to the lock.
	 */
	if (readl_relaxed(&lock->lock) == (pid + 1)) {
		writel_relaxed(0, &lock->lock);
		/*
		 * Ensure the ordering of read/write operations to ensure the
		 * proper ownership of the lock during the lock/unlock
		 * operations
		 */
		wmb();
		ret = 0;
	}
	return ret;
}

/**
 * Return owner of the spinlock.
 *
 * @lock: pointer to lock structure
 * @returns: >= 0 owned PID; < 0 for error case
 *
 * Used for testing.  PID's are assumed to be 31 bits or less.
 */
static int __raw_remote_gen_spin_owner(raw_remote_spinlock_t *lock)
{
	int owner;

	/*
	 * Ensure the ordering of read/write operations to ensure the
	 * proper ownership of the lock during the lock/unlock operations
	 */
	rmb();

	owner = readl_relaxed(&lock->lock);
	if (owner)
		return owner - 1;
	else
		return -ENODEV;
}


static int dt_node_is_valid(const struct device_node *node)
{
	const char *status;
	int statlen;

	status = of_get_property(node, "status", &statlen);
	if (status == NULL)
		return 1;

	if (statlen > 0) {
		if (!strcmp(status, "okay") || !strcmp(status, "ok"))
			return 1;
	}

	return 0;
}

static void initialize_ops(void)
{
	struct device_node *node;

	/*
	 * of_find_compatible_node() returns a valid pointer even if
	 * the status property is "disabled", so the validity needs
	 * to be checked
	 */
	node = of_find_compatible_node(NULL, NULL, sfpb_compatible_string);
	if (node && dt_node_is_valid(node)) {
		current_ops.lock = __raw_remote_sfpb_spin_lock;
		current_ops.unlock = __raw_remote_sfpb_spin_unlock;
		current_ops.trylock = __raw_remote_sfpb_spin_trylock;
		current_ops.release = __raw_remote_gen_spin_release;
		current_ops.owner = __raw_remote_gen_spin_owner;
		current_ops.lock_rlock_id =
				__raw_remote_sfpb_spin_lock_rlock_id;
		current_ops.unlock_rlock = __raw_remote_sfpb_spin_unlock_rlock;
		current_ops.get_hw_spinlocks_element =
			__raw_remote_sfpb_get_hw_spinlocks_element;
		is_hw_lock_type = 1;
		return;
	}

	node = of_find_compatible_node(NULL, NULL, ldrex_compatible_string);
	if (node && dt_node_is_valid(node)) {
		current_ops.lock = __raw_remote_ex_spin_lock;
		current_ops.unlock = __raw_remote_ex_spin_unlock;
		current_ops.trylock = __raw_remote_ex_spin_trylock;
		current_ops.release = __raw_remote_gen_spin_release;
		current_ops.owner = __raw_remote_gen_spin_owner;
		is_hw_lock_type = 0;
		return;
	}

	current_ops.lock = __raw_remote_ex_spin_lock;
	current_ops.unlock = __raw_remote_ex_spin_unlock;
	current_ops.trylock = __raw_remote_ex_spin_trylock;
	current_ops.release = __raw_remote_gen_spin_release;
	current_ops.owner = __raw_remote_gen_spin_owner;
	is_hw_lock_type = 0;
	pr_warn("Falling back to LDREX remote spinlock implementation");
}

/**
 * Release all spinlocks owned by @pid.
 *
 * This is only to be used for situations where the processor owning
 * spinlocks has crashed and the spinlocks must be released.
 *
 * @pid - processor ID of processor to release
 */
static void remote_spin_release_all_locks(uint32_t pid, int count)
{
	int n;
	 _remote_spinlock_t lock;

	if (pid >= REMOTE_SPINLOCK_NUM_PID) {
		pr_err("%s: Unsupported PID %d\n", __func__, pid);
		return;
	}

	for (n = 0; n < count; ++n) {
		if (remote_spinlock_init_address(n, &lock) == 0)
			_remote_spin_release(&lock, pid);
	}
}

void _remote_spin_release_all(uint32_t pid)
{
	remote_spin_release_all_locks(pid, lock_count);
}

#define SMEM_SPINLOCK_COUNT 8
#define SMEM_SPINLOCK_ARRAY_SIZE (SMEM_SPINLOCK_COUNT * sizeof(uint32_t))

static int remote_spinlock_init_address_smem(int id, _remote_spinlock_t *lock)
{
	_remote_spinlock_t spinlock_start;

	if (id >= SMEM_SPINLOCK_COUNT)
		return -EINVAL;

	spinlock_start = smem_find(SMEM_SPINLOCK_ARRAY,
				    SMEM_SPINLOCK_ARRAY_SIZE,
				    0,
				    SMEM_ANY_HOST_FLAG);
	if (spinlock_start == NULL)
		return -ENXIO;

	*lock = spinlock_start + id;

	lock_count = SMEM_SPINLOCK_COUNT;

	return 0;
}

static int remote_spinlock_init_address(int id, _remote_spinlock_t *lock)
{
	if (is_hw_lock_type)
		return remote_spinlock_init_address_hw(id, lock);
	else
		return remote_spinlock_init_address_smem(id, lock);
}

int _remote_spin_lock_init(remote_spinlock_id_t id, _remote_spinlock_t *lock)
{
	BUG_ON(id == NULL);

	/*
	 * Optimistic locking.  Init only needs to be done once by the first
	 * caller.  After that, serializing inits between different callers
	 * is unnecessary.  The second check after the lock ensures init
	 * wasn't previously completed by someone else before the lock could
	 * be grabbed.
	 */
	if (!current_ops.lock) {
		mutex_lock(&ops_init_lock);
		if (!current_ops.lock)
			initialize_ops();
		mutex_unlock(&ops_init_lock);
	}

	if (id[0] == 'S' && id[1] == ':') {
		/* Single-digit lock ID follows "S:" */
		BUG_ON(id[3] != '\0');

		return remote_spinlock_init_address((((uint8_t)id[2])-'0'),
			lock);
	} else {
		return -EINVAL;
	}
}

/*
 * lock comes in as a pointer to a pointer to the lock location, so it must
 * be dereferenced and casted to the right type for the actual lock
 * implementation functions
 */
void _remote_spin_lock(_remote_spinlock_t *lock)
{
	if (unlikely(!current_ops.lock))
		BUG();
	current_ops.lock((raw_remote_spinlock_t *)(*lock));
}
EXPORT_SYMBOL(_remote_spin_lock);

void _remote_spin_unlock(_remote_spinlock_t *lock)
{
	if (unlikely(!current_ops.unlock))
		BUG();
	current_ops.unlock((raw_remote_spinlock_t *)(*lock));
}
EXPORT_SYMBOL(_remote_spin_unlock);

int _remote_spin_trylock(_remote_spinlock_t *lock)
{
	if (unlikely(!current_ops.trylock))
		BUG();
	return current_ops.trylock((raw_remote_spinlock_t *)(*lock));
}
EXPORT_SYMBOL(_remote_spin_trylock);

int _remote_spin_release(_remote_spinlock_t *lock, uint32_t pid)
{
	if (unlikely(!current_ops.release))
		BUG();
	return current_ops.release((raw_remote_spinlock_t *)(*lock), pid);
}
EXPORT_SYMBOL(_remote_spin_release);

int _remote_spin_owner(_remote_spinlock_t *lock)
{
	if (unlikely(!current_ops.owner))
		BUG();
	return current_ops.owner((raw_remote_spinlock_t *)(*lock));
}
EXPORT_SYMBOL(_remote_spin_owner);

void _remote_spin_lock_rlock_id(_remote_spinlock_t *lock, uint32_t tid)
{
	if (unlikely(!current_ops.lock_rlock_id))
		BUG();
	current_ops.lock_rlock_id((raw_remote_spinlock_t *)(*lock), tid);
}
EXPORT_SYMBOL(_remote_spin_lock_rlock_id);

void _remote_spin_unlock_rlock(_remote_spinlock_t *lock)
{
	if (unlikely(!current_ops.unlock_rlock))
		BUG();
	current_ops.unlock_rlock((raw_remote_spinlock_t *)(*lock));
}
EXPORT_SYMBOL(_remote_spin_unlock_rlock);

int _remote_spin_get_hw_spinlocks_element(_remote_spinlock_t *lock)
{
	return current_ops.get_hw_spinlocks_element(
			(raw_remote_spinlock_t *)(*lock));
}
EXPORT_SYMBOL(_remote_spin_get_hw_spinlocks_element);

/* end common spinlock API -------------------------------------------------- */
