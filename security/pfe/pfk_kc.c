// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

/*
 * PFK Key Cache
 *
 * Key Cache used internally in PFK.
 * The purpose of the cache is to save access time to QSEE when loading keys.
 * Currently the cache is the same size as the total number of keys that can
 * be loaded to ICE. Since this number is relatively small, the algorithms for
 * cache eviction are simple, linear and based on last usage timestamp, i.e
 * the node that will be evicted is the one with the oldest timestamp.
 * Empty entries always have the oldest timestamp.
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/sched/signal.h>

#include "pfk_kc.h"
#include "pfk_ice.h"


/** the first available index in ice engine */
#define PFK_KC_STARTING_INDEX 2

/** currently the only supported key and salt sizes */
#define PFK_KC_KEY_SIZE 32
#define PFK_KC_SALT_SIZE 32

/** Table size */
#define PFK_KC_TABLE_SIZE ((32) - (PFK_KC_STARTING_INDEX))

/** The maximum key and salt size */
#define PFK_MAX_KEY_SIZE PFK_KC_KEY_SIZE
#define PFK_MAX_SALT_SIZE PFK_KC_SALT_SIZE
#define PFK_UFS "ufs"
#define PFK_UFS_CARD "ufscard"

static DEFINE_SPINLOCK(kc_lock);
static unsigned long flags;
static bool kc_ready;
static char *s_type = "sdcc";

/**
 * enum pfk_kc_entry_state - state of the entry inside kc table
 *
 * @FREE:		   entry is free
 * @ACTIVE_ICE_PRELOAD:    entry is actively used by ICE engine
			   and cannot be used by others. SCM call
			   to load key to ICE is pending to be performed
 * @ACTIVE_ICE_LOADED:     entry is actively used by ICE engine and
			   cannot be used by others. SCM call to load the
			   key to ICE was successfully executed and key is
			   now loaded
 * @INACTIVE_INVALIDATING: entry is being invalidated during file close
			   and cannot be used by others until invalidation
			   is complete
 * @INACTIVE:		   entry's key is already loaded, but is not
			   currently being used. It can be re-used for
			   optimization and to avoid SCM call cost or
			   it can be taken by another key if there are
			   no FREE entries
 * @SCM_ERROR:		   error occurred while scm call was performed to
			   load the key to ICE
 */
enum pfk_kc_entry_state {
	FREE,
	ACTIVE_ICE_PRELOAD,
	ACTIVE_ICE_LOADED,
	INACTIVE_INVALIDATING,
	INACTIVE,
	SCM_ERROR
};

struct kc_entry {
	unsigned char key[PFK_MAX_KEY_SIZE];
	size_t key_size;

	unsigned char salt[PFK_MAX_SALT_SIZE];
	size_t salt_size;

	u64 time_stamp;
	u32 key_index;

	struct task_struct *thread_pending;

	enum pfk_kc_entry_state state;

	/* ref count for the number of requests in the HW queue for this key */
	int loaded_ref_cnt;
	int scm_error;
};

/**
 * kc_is_ready() - driver is initialized and ready.
 *
 * Return: true if the key cache is ready.
 */
static inline bool kc_is_ready(void)
{
	return kc_ready;
}

static inline void kc_spin_lock(void)
{
	spin_lock_irqsave(&kc_lock, flags);
}

static inline void kc_spin_unlock(void)
{
	spin_unlock_irqrestore(&kc_lock, flags);
}

/**
 * pfk_kc_get_storage_type() - return the hardware storage type.
 *
 * Return: storage type queried during bootup.
 */
const char *pfk_kc_get_storage_type(void)
{
	return s_type;
}

/**
 * kc_entry_is_available() - checks whether the entry is available
 *
 * Return true if it is , false otherwise or if invalid
 * Should be invoked under spinlock
 */
static bool kc_entry_is_available(const struct kc_entry *entry)
{
	if (!entry)
		return false;

	return (entry->state == FREE || entry->state == INACTIVE);
}

/**
 * kc_entry_wait_till_available() - waits till entry is available
 *
 * Returns 0 in case of success or -ERESTARTSYS if the wait was interrupted
 * by signal
 *
 * Should be invoked under spinlock
 */
static int kc_entry_wait_till_available(struct kc_entry *entry)
{
	int res = 0;

	while (!kc_entry_is_available(entry)) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			res = -ERESTARTSYS;
			break;
		}
		/* assuming only one thread can try to invalidate
		 * the same entry
		 */
		entry->thread_pending = current;
		kc_spin_unlock();
		schedule();
		kc_spin_lock();
	}
	set_current_state(TASK_RUNNING);

	return res;
}

/**
 * kc_entry_start_invalidating() - moves entry to state
 *			           INACTIVE_INVALIDATING
 *				   If entry is in use, waits till
 *				   it gets available
 * @entry: pointer to entry
 *
 * Return 0 in case of success, otherwise error
 * Should be invoked under spinlock
 */
static int kc_entry_start_invalidating(struct kc_entry *entry)
{
	int res;

	res = kc_entry_wait_till_available(entry);
	if (res)
		return res;

	entry->state = INACTIVE_INVALIDATING;

	return 0;
}

/**
 * kc_entry_finish_invalidating() - moves entry to state FREE
 *				    wakes up all the tasks waiting
 *				    on it
 *
 * @entry: pointer to entry
 *
 * Return 0 in case of success, otherwise error
 * Should be invoked under spinlock
 */
static void kc_entry_finish_invalidating(struct kc_entry *entry)
{
	if (!entry)
		return;

	if (entry->state != INACTIVE_INVALIDATING)
		return;

	entry->state = FREE;
}

/**
 * kc_min_entry() - compare two entries to find one with minimal time
 * @a: ptr to the first entry. If NULL the other entry will be returned
 * @b: pointer to the second entry
 *
 * Return the entry which timestamp is the minimal, or b if a is NULL
 */
static inline struct kc_entry *kc_min_entry(struct kc_entry *a,
		struct kc_entry *b)
{
	if (!a)
		return b;

	if (time_before64(b->time_stamp, a->time_stamp))
		return b;

	return a;
}

/**
 * kc_entry_at_index() - return entry at specific index
 * @index: index of entry to be accessed
 *
 * Return entry
 * Should be invoked under spinlock
 */
static struct kc_entry *kc_entry_at_index(int index,
		struct ice_device *ice_dev)
{
	return (struct kc_entry *)(ice_dev->key_table) + index;
}

/**
 * kc_find_key_at_index() - find kc entry starting at specific index
 * @key: key to look for
 * @key_size: the key size
 * @salt: salt to look for
 * @salt_size: the salt size
 * @sarting_index: index to start search with, if entry found, updated with
 * index of that entry
 *
 * Return entry or NULL in case of error
 * Should be invoked under spinlock
 */
static struct kc_entry *kc_find_key_at_index(const unsigned char *key,
	size_t key_size, const unsigned char *salt, size_t salt_size,
	struct ice_device *ice_dev, int *starting_index)
{
	struct kc_entry *entry = NULL;
	int i = 0;

	for (i = *starting_index; i < PFK_KC_TABLE_SIZE; i++) {
		entry = kc_entry_at_index(i, ice_dev);

		if (salt != NULL) {
			if (entry->salt_size != salt_size)
				continue;

			if (memcmp(entry->salt, salt, salt_size) != 0)
				continue;
		}

		if (entry->key_size != key_size)
			continue;

		if (memcmp(entry->key, key, key_size) == 0) {
			*starting_index = i;
			return entry;
		}
	}

	return NULL;
}

/**
 * kc_find_key() - find kc entry
 * @key: key to look for
 * @key_size: the key size
 * @salt: salt to look for
 * @salt_size: the salt size
 *
 * Return entry or NULL in case of error
 * Should be invoked under spinlock
 */
static struct kc_entry *kc_find_key(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size,
		struct ice_device *ice_dev)
{
	int index = 0;

	return kc_find_key_at_index(key, key_size, salt, salt_size,
				ice_dev, &index);
}

/**
 * kc_find_oldest_entry_non_locked() - finds the entry with minimal timestamp
 * that is not locked
 *
 * Returns entry with minimal timestamp. Empty entries have timestamp
 * of 0, therefore they are returned first.
 * If all the entries are locked, will return NULL
 * Should be invoked under spin lock
 */
static struct kc_entry *kc_find_oldest_entry_non_locked(
		struct ice_device *ice_dev)
{
	struct kc_entry *curr_min_entry = NULL;
	struct kc_entry *entry = NULL;
	int i = 0;

	for (i = 0; i < PFK_KC_TABLE_SIZE; i++) {
		entry = kc_entry_at_index(i, ice_dev);

		if (entry->state == FREE)
			return entry;

		if (entry->state == INACTIVE)
			curr_min_entry = kc_min_entry(curr_min_entry, entry);
	}

	return curr_min_entry;
}

/**
 * kc_update_timestamp() - updates timestamp of entry to current
 *
 * @entry: entry to update
 *
 */
static void kc_update_timestamp(struct kc_entry *entry)
{
	if (!entry)
		return;

	entry->time_stamp = get_jiffies_64();
}

/**
 * kc_clear_entry() - clear the key from entry and mark entry not in use
 *
 * @entry: pointer to entry
 *
 * Should be invoked under spinlock
 */
static void kc_clear_entry(struct kc_entry *entry)
{
	if (!entry)
		return;

	memset(entry->key, 0, entry->key_size);
	memset(entry->salt, 0, entry->salt_size);

	entry->key_size = 0;
	entry->salt_size = 0;

	entry->time_stamp = 0;
	entry->scm_error = 0;

	entry->state = FREE;

	entry->loaded_ref_cnt = 0;
	entry->thread_pending = NULL;
}

/**
 * kc_update_entry() - replaces the key in given entry and
 *			loads the new key to ICE
 *
 * @entry: entry to replace key in
 * @key: key
 * @key_size: key_size
 * @salt: salt
 * @salt_size: salt_size
 * @data_unit: dun size
 *
 * The previous key is securely released and wiped, the new one is loaded
 * to ICE.
 * Should be invoked under spinlock
 * Caller to validate that key/salt_size matches the size in struct kc_entry
 */
static int kc_update_entry(struct kc_entry *entry, const unsigned char *key,
	size_t key_size, const unsigned char *salt, size_t salt_size,
	unsigned int data_unit, struct ice_device *ice_dev)
{
	int ret;

	kc_clear_entry(entry);

	memcpy(entry->key, key, key_size);
	entry->key_size = key_size;

	memcpy(entry->salt, salt, salt_size);
	entry->salt_size = salt_size;

	/* Mark entry as no longer free before releasing the lock */
	entry->state = ACTIVE_ICE_PRELOAD;
	kc_spin_unlock();

	ret = qti_pfk_ice_set_key(entry->key_index, entry->key,
			entry->salt, ice_dev, data_unit);

	kc_spin_lock();
	return ret;
}

/**
 * pfk_kc_init() - init function
 *
 * Return 0 in case of success, error otherwise
 */
static int pfk_kc_init(struct ice_device *ice_dev)
{
	int i = 0;
	struct kc_entry *entry = NULL;

	kc_spin_lock();
	for (i = 0; i < PFK_KC_TABLE_SIZE; i++) {
		entry = kc_entry_at_index(i, ice_dev);
		entry->key_index = PFK_KC_STARTING_INDEX + i;
	}
	kc_ready = true;
	kc_spin_unlock();

	return 0;
}

/**
 * pfk_kc_denit() - deinit function
 *
 * Return 0 in case of success, error otherwise
 */
int pfk_kc_deinit(void)
{
	kc_ready = false;

	return 0;
}

/**
 * pfk_kc_load_key_start() - retrieve the key from cache or add it if
 * it's not there and return the ICE hw key index in @key_index.
 * @key: pointer to the key
 * @key_size: the size of the key
 * @salt: pointer to the salt
 * @salt_size: the size of the salt
 * @key_index: the pointer to key_index where the output will be stored
 * @async: whether scm calls are allowed in the caller context
 *
 * If key is present in cache, than the key_index will be retrieved from cache.
 * If it is not present, the oldest entry from kc table will be evicted,
 * the key will be loaded to ICE via QSEE to the index that is the evicted
 * entry number and stored in cache.
 * Entry that is going to be used is marked as being used, it will mark
 * as not being used when ICE finishes using it and pfk_kc_load_key_end
 * will be invoked.
 * As QSEE calls can only be done from a non-atomic context, when @async flag
 * is set to 'false', it specifies that it is ok to make the calls in the
 * current context. Otherwise, when @async is set, the caller should retry the
 * call again from a different context, and -EAGAIN error will be returned.
 *
 * Return 0 in case of success, error otherwise
 */
int pfk_kc_load_key_start(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size, u32 *key_index,
		bool async, unsigned int data_unit, struct ice_device *ice_dev)
{
	int ret = 0;
	struct kc_entry *entry = NULL;
	bool entry_exists = false;

	if (!kc_is_ready())
		return -ENODEV;

	if (!key || !salt || !key_index) {
		pr_err("%s key/salt/key_index NULL\n", __func__);
		return -EINVAL;
	}

	if (key_size != PFK_KC_KEY_SIZE) {
		pr_err("unsupported key size %zu\n", key_size);
		return -EINVAL;
	}

	if (salt_size != PFK_KC_SALT_SIZE) {
		pr_err("unsupported salt size %zu\n", salt_size);
		return -EINVAL;
	}

	kc_spin_lock();

	entry = kc_find_key(key, key_size, salt, salt_size, ice_dev);
	if (!entry) {
		if (async) {
			pr_debug("%s task will populate entry\n", __func__);
			kc_spin_unlock();
			return -EAGAIN;
		}

		entry = kc_find_oldest_entry_non_locked(ice_dev);
		if (!entry) {
			/* could not find a single non locked entry,
			 * return EBUSY to upper layers so that the
			 * request will be rescheduled
			 */
			kc_spin_unlock();
			return -EBUSY;
		}
	} else {
		entry_exists = true;
	}

	pr_debug("entry with index %d is in state %d\n",
		entry->key_index, entry->state);

	switch (entry->state) {
	case (INACTIVE):
		if (entry_exists) {
			kc_update_timestamp(entry);
			entry->state = ACTIVE_ICE_LOADED;

			if (!strcmp(ice_dev->ice_instance_type,
				(char *)PFK_UFS) ||
					!strcmp(ice_dev->ice_instance_type,
						(char *)PFK_UFS_CARD)) {
				if (async)
					entry->loaded_ref_cnt++;
			} else {
				entry->loaded_ref_cnt++;
			}
			break;
		}
	case (FREE):
		ret = kc_update_entry(entry, key, key_size, salt, salt_size,
					data_unit, ice_dev);
		if (ret) {
			entry->state = SCM_ERROR;
			entry->scm_error = ret;
			pr_err("%s: key load error (%d)\n", __func__, ret);
		} else {
			kc_update_timestamp(entry);
			entry->state = ACTIVE_ICE_LOADED;

			/*
			 * In case of UFS only increase ref cnt for async calls,
			 * sync calls from within work thread do not pass
			 * requests further to HW
			 */
			if (!strcmp(ice_dev->ice_instance_type,
				(char *)PFK_UFS) ||
					!strcmp(ice_dev->ice_instance_type,
						(char *)PFK_UFS_CARD)) {
				if (async)
					entry->loaded_ref_cnt++;
			} else {
				entry->loaded_ref_cnt++;
			}
		}
		break;
	case (ACTIVE_ICE_PRELOAD):
	case (INACTIVE_INVALIDATING):
		ret = -EAGAIN;
		break;
	case (ACTIVE_ICE_LOADED):
		kc_update_timestamp(entry);

		if (!strcmp(ice_dev->ice_instance_type, (char *)PFK_UFS) ||
			!strcmp(ice_dev->ice_instance_type,
				(char *)PFK_UFS_CARD)) {
			if (async)
				entry->loaded_ref_cnt++;
		} else {
			entry->loaded_ref_cnt++;
		}
		break;
	case(SCM_ERROR):
		ret = entry->scm_error;
		kc_clear_entry(entry);
		entry->state = FREE;
		break;
	default:
		pr_err("invalid state %d for entry with key index %d\n",
			entry->state, entry->key_index);
		ret = -EINVAL;
	}

	*key_index = entry->key_index;
	kc_spin_unlock();

	return ret;
}

/**
 * pfk_kc_load_key_end() - finish the process of key loading that was started
 *						   by pfk_kc_load_key_start
 *						   by marking the entry as not
 *						   being in use
 * @key: pointer to the key
 * @key_size: the size of the key
 * @salt: pointer to the salt
 * @salt_size: the size of the salt
 *
 */
void pfk_kc_load_key_end(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size,
		struct ice_device *ice_dev)
{
	struct kc_entry *entry = NULL;
	struct task_struct *tmp_pending = NULL;
	int ref_cnt = 0;

	if (!kc_is_ready())
		return;

	if (!key || !salt)
		return;

	if (key_size != PFK_KC_KEY_SIZE)
		return;

	if (salt_size != PFK_KC_SALT_SIZE)
		return;

	kc_spin_lock();

	entry = kc_find_key(key, key_size, salt, salt_size, ice_dev);
	if (!entry) {
		kc_spin_unlock();
		pr_err("internal error, there should an entry to unlock\n");

		return;
	}
	ref_cnt = --entry->loaded_ref_cnt;

	if (ref_cnt < 0)
		pr_err("internal error, ref count should never be negative\n");

	if (!ref_cnt) {
		entry->state = INACTIVE;
		/*
		 * wake-up invalidation if it's waiting
		 * for the entry to be released
		 */
		if (entry->thread_pending) {
			tmp_pending = entry->thread_pending;
			entry->thread_pending = NULL;

			kc_spin_unlock();
			wake_up_process(tmp_pending);
			return;
		}
	}

	kc_spin_unlock();
}

/**
 * pfk_kc_remove_key_with_salt() - remove the key and salt from cache
 * and from ICE engine.
 * @key: pointer to the key
 * @key_size: the size of the key
 * @salt: pointer to the key
 * @salt_size: the size of the key
 *
 * Return 0 in case of success, error otherwise (also in case of non
 * (existing key)
 */
int pfk_kc_remove_key_with_salt(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size)
{
	struct kc_entry *entry = NULL;
	struct list_head *ice_dev_list = NULL;
	struct ice_device *ice_dev;
	int res = 0;

	if (!kc_is_ready())
		return -ENODEV;

	if (!key)
		return -EINVAL;

	if (!salt)
		return -EINVAL;

	if (key_size != PFK_KC_KEY_SIZE)
		return -EINVAL;

	if (salt_size != PFK_KC_SALT_SIZE)
		return -EINVAL;

	kc_spin_lock();

	ice_dev_list = get_ice_dev_list();
	if (!ice_dev_list) {
		pr_err("%s: Did not find ICE device head\n", __func__);
		return -ENODEV;
	}
	list_for_each_entry(ice_dev, ice_dev_list, list) {
		entry = kc_find_key(key, key_size, salt, salt_size, ice_dev);
		if (entry) {
			pr_debug("%s: Found entry for ice_dev number %d\n",
					__func__, ice_dev->device_no);

			break;
		}
		pr_debug("%s: Can't find  entry for ice_dev number %d\n",
					__func__, ice_dev->device_no);
	}

	if (!entry) {
		pr_debug("%s: Cannot find entry for any ice device\n",
				__func__);
		kc_spin_unlock();
		return -EINVAL;
	}

	res = kc_entry_start_invalidating(entry);
	if (res != 0) {
		kc_spin_unlock();
		return res;
	}
	kc_clear_entry(entry);

	kc_spin_unlock();

	qti_pfk_ice_invalidate_key(entry->key_index, ice_dev);

	kc_spin_lock();
	kc_entry_finish_invalidating(entry);
	kc_spin_unlock();

	return 0;
}

/**
 * pfk_kc_clear() - clear the table and remove all keys from ICE
 *
 * Return 0 on success, error otherwise
 *
 */
int pfk_kc_clear(struct ice_device *ice_dev)
{
	struct kc_entry *entry = NULL;
	int i = 0;
	int res = 0;

	if (!kc_is_ready())
		return -ENODEV;

	kc_spin_lock();
	for (i = 0; i < PFK_KC_TABLE_SIZE; i++) {
		entry = kc_entry_at_index(i, ice_dev);
		res = kc_entry_start_invalidating(entry);
		if (res != 0) {
			kc_spin_unlock();
			goto out;
		}
		kc_clear_entry(entry);
	}
	kc_spin_unlock();

	for (i = 0; i < PFK_KC_TABLE_SIZE; i++)
		qti_pfk_ice_invalidate_key(
			kc_entry_at_index(i, ice_dev)->key_index, ice_dev);

	/* fall through */
	res = 0;
out:
	kc_spin_lock();
	for (i = 0; i < PFK_KC_TABLE_SIZE; i++)
		kc_entry_finish_invalidating(kc_entry_at_index(i, ice_dev));
	kc_spin_unlock();

	return res;
}

/**
 * pfk_kc_clear_on_reset() - clear the table and remove all keys from ICE
 * The assumption is that at this point we don't have any pending transactions
 * Also, there is no need to clear keys from ICE
 *
 * Return 0 on success, error otherwise
 *
 */
void pfk_kc_clear_on_reset(struct ice_device *ice_dev)
{
	struct kc_entry *entry = NULL;
	int i = 0;

	if (!kc_is_ready())
		return;

	kc_spin_lock();
	for (i = 0; i < PFK_KC_TABLE_SIZE; i++) {
		entry = kc_entry_at_index(i, ice_dev);
		kc_clear_entry(entry);
	}
	kc_spin_unlock();
}

static int pfk_kc_find_storage_type(char **device)
{
	char boot[20] = {'\0'};
	char *match = (char *)strnstr(saved_command_line,
				"androidboot.bootdevice=",
				strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.bootdevice=")),
			sizeof(boot) - 1);
		if (strnstr(boot, PFK_UFS, strlen(boot)))
			*device = PFK_UFS;

		return 0;
	}
	return -EINVAL;
}

int pfk_kc_initialize_key_table(struct ice_device *ice_dev)
{
	int res = 0;
	struct kc_entry *kc_table;

	kc_table = kzalloc(PFK_KC_TABLE_SIZE*sizeof(struct kc_entry),
			GFP_KERNEL);
	if (!kc_table) {
		res = -ENOMEM;
		pr_err("%s: Error %d allocating memory for key table\n",
			__func__, res);
	}
	ice_dev->key_table = kc_table;
	pfk_kc_init(ice_dev);

	return res;
}

static int __init pfk_kc_pre_init(void)
{
	return pfk_kc_find_storage_type(&s_type);
}

static void __exit pfk_kc_exit(void)
{
	s_type = NULL;
}

module_init(pfk_kc_pre_init);
module_exit(pfk_kc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Per-File-Key-KC driver");
