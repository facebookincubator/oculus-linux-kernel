// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/scatterlist.h>
#include <crypto/aead.h>
#include <soc/qcom/qcom_hibernation.h>
#include <../../../kernel/power/power.h>
#include <trace/hooks/bl_hib.h>
#include <linux/reboot.h>
#include <soc/qcom/smci_clientenv.h>
#include <soc/qcom/smci_low_power_key_mgr.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>

#define ILOWPOWERKEYMANAGER_ERROR_INVALID_OPERATION_CHECK 11
#define AUTH_SIZE 16

static struct kobject *gethibkey_kobj;
static struct smci_object client_env = {0};
static struct smci_object key_mgr_object = {0};
static struct hib_bio_batch *hb;
static struct qcom_crypto_params *params;
static struct crypto_aead *tfm;
static struct aead_request *req;
static uint8_t *auth_mem_start;
static size_t auth_mem_size;
static uint8_t *compressed_blk_array;
static int blk_array_pos;
static int auth_slot_offset;
static atomic_t nr_handled_pages;
static sector_t base_sector;
static ktime_t crypt_ns;

static int setup(void)
{
	int ret = 0;

	ret =  get_client_env_object(&client_env);
	if (ret) {
		pr_err("Failed to get client env object, ret = %d\n", ret);
		return ret;
	}

	ret = smci_clientenv_open(client_env, CLOWPOWERKEYMANAGER_UID,
				  &key_mgr_object);
	if (ret)
		pr_err("Failed to get Key Manager object, ret = %d\n", ret);

	return ret;
}

static void cleanup(void)
{
	SMCI_OBJECT_ASSIGN_NULL(key_mgr_object);
	SMCI_OBJECT_ASSIGN_NULL(client_env);
}

static int key_mgr_get_key(uint32_t event, void *key, size_t key_len,
		    size_t *key_len_out)
{
	int ret = setup();

	if (!ret)
		ret = ILowPowerKeyManager_getKey(key_mgr_object, event, key,
					 key_len, key_len_out);
	cleanup();
	return ret;
}

static int key_mgr_prepare(uint32_t event, const ILOWPOWERKEYMANAGER_key_info *key_info)
{
	int ret = setup();

	if (!ret)
		ret = ILowPowerKeyManager_prepare(key_mgr_object, event, key_info);

	cleanup();
	return ret;
}

#ifdef CONFIG_QCOM_KERNEL_SEC_KEY
int get_key_for_hib_exp(void)
{
	ILOWPOWERKEYMANAGER_key_info key_info;
	u8 key[AES256_KEY_SIZE];
	size_t key_len_out;
	int ret;

	key_info.key_size = AES256_KEY_SIZE;
	ret = key_mgr_prepare(ILOWPOWERKEYMANAGER_HIBERNATE_WITH_ENCRYPTION, &key_info);
	if (!ret)
		return 0;

	if (ret != ILOWPOWERKEYMANAGER_ERROR_INVALID_OPERATION_CHECK) {
		pr_err("%s: Failed to init QTEE: key_mgr_prepare: %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s: Thrashing the old key.. %d %d", __func__, ret,
					ILOWPOWERKEYMANAGER_ERROR_INVALID_OPERATION);
	ret = key_mgr_get_key(ILOWPOWERKEYMANAGER_HIBERNATE_WITH_ENCRYPTION, key,
					AES256_KEY_SIZE, &key_len_out);
	memset(key, 0, AES256_KEY_SIZE);
	if (ret) {
		pr_err("%s: Failed to init QTEE: key_mgr_get_key: %d\n", __func__, ret);
		return ret;
	}

	ret = key_mgr_prepare(ILOWPOWERKEYMANAGER_HIBERNATE_WITH_ENCRYPTION, &key_info);
	if (ret) {
		pr_err("%s: Failed to init QTEE: key_mgr_prepare: %d\n", __func__, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(get_key_for_hib_exp);
#endif

static inline uint64_t sector_to_offset(uint64_t sector) {
	return ((sector >> (PAGE_SHIFT - SECTOR_SHIFT)) * AUTH_SIZE);
}

static void save_authtag(uint8_t *tag, uint64_t sector)
{
	if ((sector_to_offset(sector) + AUTH_SIZE) > auth_mem_size)
		panic("%s invalid sector %lld\n", __func__, sector);

	memcpy(auth_mem_start + sector_to_offset(sector), tag, AUTH_SIZE);
}

static uint8_t* get_authtag(uint64_t sector)
{
	if ((sector_to_offset(sector) + AUTH_SIZE) > auth_mem_size)
		panic("%s invalid sector %lld\n", __func__, sector);

	return auth_mem_start + sector_to_offset(sector);
}

static void store_auth_slot_num(void *data, uint32_t *auth_slot_num)
{
	if (auth_slot_num)
		*auth_slot_num = (uint32_t) auth_slot_offset;
	else
		pr_info("PM: Decryption took %d ms (%d us/page)\n",
				ktime_to_ms(crypt_ns),
				(ktime_to_us(crypt_ns) / atomic_read(&nr_handled_pages)));
}

static void skip_swap_map_write(void *data, bool *skip)
{
	*skip = false;
}

static void encrypt_page(void *data, void *buf, sector_t sector)
{
	struct scatterlist sg_in[1], sg_out[1];
	uint32_t iv[IV_WORDS];
	int ret;
	void *work_buf = (void *)__get_free_pages(GFP_KERNEL, 1);
	ktime_t start_ns = ktime_get();

	if (!work_buf)
		panic("%s failed to allocate mem\n", __func__);

	sector -= base_sector;
	iv[0] = params->iv[0];
	iv[1] = params->iv[1] ^ (uint32_t)(sector >> 32);
	iv[2] = params->iv[2] ^ (uint32_t)sector;
	sg_init_one(sg_in, buf, PAGE_SIZE);
	sg_init_one(sg_out, work_buf, PAGE_SIZE + AUTH_SIZE);
	aead_request_set_crypt(req, sg_in, sg_out, PAGE_SIZE, (uint8_t *)iv);

	ret = crypto_aead_encrypt(req);
	if (ret)
		panic("%s failed %d\n", __func__, ret);

	copy_page(buf, work_buf);
	save_authtag(work_buf + PAGE_SIZE, sector);
	free_pages((unsigned long)work_buf, 1);
	atomic_inc(&nr_handled_pages);
	crypt_ns += ktime_sub(ktime_get(), start_ns);
}

static void decrypt_page(void *data, void *buf, sector_t sector)
{
	struct scatterlist sg_in[1], sg_out[1];
	uint32_t iv[IV_WORDS];
	int ret;
	void *work_buf = (void *)__get_free_pages(GFP_KERNEL, 1);
	ktime_t start_ns = ktime_get();

	if (!work_buf)
		panic("%s failed to allocate mem\n", __func__);

	sector -= base_sector;
	iv[0] = params->iv[0];
	iv[1] = params->iv[1] ^ (uint32_t)(sector >> 32);
	iv[2] = params->iv[2] ^ (uint32_t)sector;

	copy_page(work_buf, buf);
	memcpy(work_buf + PAGE_SIZE, get_authtag(sector), AUTH_SIZE);
	sg_init_one(sg_in, work_buf, PAGE_SIZE + AUTH_SIZE);
	sg_init_one(sg_out, buf, PAGE_SIZE);
	aead_request_set_crypt(req, sg_in, sg_out, PAGE_SIZE + AUTH_SIZE, (uint8_t *)iv);

	ret = crypto_aead_decrypt(req);
	if (ret)
		panic("%s failed %d at block %d\n", __func__, ret,
				((sector >> (PAGE_SHIFT - SECTOR_SHIFT)) + swsusp_header->image + 2));

	free_pages((unsigned long)work_buf, 1);
	atomic_inc(&nr_handled_pages);
	crypt_ns += ktime_sub(ktime_get(), start_ns);
}

static int read_swap_page(sector_t sector, void *buffer)
{
	struct bio *bio;
	struct page *page = virt_to_page(buffer);
	int ret = 0;

	bio = bio_alloc(GFP_NOIO | __GFP_HIGH, 1);
	if (!bio)
		return -ENOMEM;

	bio_set_dev(bio, hib_resume_bdev);
	bio->bi_iter.bi_sector = sector;
	bio_set_op_attrs(bio, REQ_OP_READ, 0);

	if (bio_add_page(bio, page, PAGE_SIZE, 0) < PAGE_SIZE) {
		bio_put(bio);
		return -EIO;
	}

	submit_bio_wait(bio);
	if (bio->bi_status) {
		pr_err("BIO read failed at sector %llu\n", (unsigned long long)sector);
		ret = -EIO;
	}

	bio_put(bio);
	return ret;
}

static int read_auth_params(sector_t sector, int nr_pages, void *buffer)
{
	int i, ret = 0;
	int pages_remaining = nr_pages;
	int page_offset = 0;

	while (pages_remaining > 0) {
		int pages_to_read = min(pages_remaining, BIO_MAX_PAGES);
		struct bio *bio = bio_alloc(GFP_NOIO | __GFP_HIGH, pages_to_read);
		if (!bio)
			return -ENOMEM;

		bio_set_dev(bio, hib_resume_bdev);
		bio->bi_iter.bi_sector = sector;
		bio_set_op_attrs(bio, REQ_OP_READ, 0);

		for (i = 0; i < pages_to_read; i++) {
			void *page_ptr = buffer + (page_offset + i) * PAGE_SIZE;
			struct page *page = vmalloc_to_page(page_ptr);

			if (!page) {
				pr_err("vmalloc_to_page failed for offset %d\n", page_offset + i);
				bio_put(bio);
				return -EFAULT;
			}

			if (bio_add_page(bio, page, PAGE_SIZE, offset_in_page(page_ptr)) < PAGE_SIZE) {
				pr_err("bio_add_page failed at offset %d\n", page_offset + i);
				bio_put(bio);
				return -EIO;
			}
		}

		submit_bio_wait(bio);
		if (bio->bi_status) {
			pr_err("BIO read failed at sector %llu\n", (unsigned long long)sector);
			ret = -EIO;
		}

		bio_put(bio);

		sector += (pages_to_read * (PAGE_SIZE >> SECTOR_SHIFT));
		page_offset += pages_to_read;
		pages_remaining -= pages_to_read;
	}

	return ret;
}

static uint32_t get_auth_mem_nr_pages(uint32_t nr_pages)
{
	// We increase the number of tags by 3 to account for
	// the swap header, the first map page and the swap info page
	// then for all other map pages i.e one for every 511 data pages
	nr_pages += 3 + DIV_ROUND_UP((nr_pages - 510), 511);
	return DIV_ROUND_UP((nr_pages * AUTH_SIZE), PAGE_SIZE);
}

static int get_param_authtags(void)
{
	int params_slot, ret;
	sector_t params_slot_sector, authtag_slot_sector;

	auth_slot_offset = (int) swap_auth_slot_offset;
	if (!auth_slot_offset) {
		return -EINVAL;
	}

	params_slot = auth_slot_offset - 1;
	pr_info("%s params_slot: %d\n", __func__, params_slot);
	pr_info("%s auth_slot_offset: %d\n", __func__, auth_slot_offset);

	params_slot_sector =  params_slot * (PAGE_SIZE >> SECTOR_SHIFT);
	ret = read_swap_page(params_slot_sector, params);
	if (ret) {
		pr_err("Failed to read crypto params from swap\n");
		return ret;
	}

	pr_info("%s auth_slot_last: %d\n", __func__, auth_slot_offset + params->authslot_count);
	pr_info("%s nr_pages_tags: %d\n", __func__, params->authslot_count);
	auth_mem_size = params->authslot_count * PAGE_SIZE;
	auth_mem_start = vmalloc(auth_mem_size);
	if (!auth_mem_start) {
		pr_err("Failed to alloc_auth_memory %d\n", ret);
		return -ENOMEM;
	}

	authtag_slot_sector = auth_slot_offset * (PAGE_SIZE >> SECTOR_SHIFT);
	ret = read_auth_params(authtag_slot_sector, params->authslot_count, auth_mem_start);
	if (ret) {
		pr_err("Failed to read_auth_params:%d\n", ret);
		return ret;
	}
	pr_info("%s nr_pages_tags: %d\n", __func__, params->authslot_count);
	pr_info("%s iv: %*phN\n", __func__, IV_SIZE, params->iv);
	return 0;
}

static void hib_end_io(struct bio *bio)
{
	struct hib_bio_batch *hb = bio->bi_private;
	struct page *page = bio_first_page_all(bio);

	if (bio->bi_status) {
		pr_alert("Read-error on swap-device (%u:%u:%lu)\n",
			MAJOR(bio_dev(bio)), MINOR(bio_dev(bio)),
			(unsigned long long)bio->bi_iter.bi_sector);
	}

	if (bio_data_dir(bio) == WRITE)
		put_page(page);

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
	bio->bi_iter.bi_sector = page_off * (PAGE_SIZE >> SECTOR_SHIFT);
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

static int write_page(void *buf, sector_t offset, struct hib_bio_batch *hb)
{
	void *src;
	if (!offset)
		return -ENOSPC;

	if (hb) {
		src = (void *)__get_free_page(GFP_NOIO | __GFP_NOWARN |
						__GFP_NORETRY);
		if (src) {
			copy_page(src, buf);
		} else {
			WARN_ON_ONCE(1);
			hb = NULL;/* Go synchronous */
			src = buf;
		}
	} else {
		src = buf;
	}
	return hib_submit_io(REQ_OP_WRITE, REQ_SYNC, offset, src, hb);
}

/*
 * Number of pages compressed at one time. This is inline with UNC_PAGES
 * in kernel/power/swap.c.
 */
#define UNCMP_PAGES   32

static uint32_t get_size_of_compression_block_array(void)
{
	/*
	 * Get the max index based on total no. of pages. Current compression
	 * algorithm compresses each UNC_PAGES pages to x pages. Use this logic to
	 * get the max index.
	 */
	uint32_t max_index = DIV_ROUND_UP(snapshot_get_image_size(), UNCMP_PAGES);

	uint32_t size = ALIGN((max_index * sizeof(*compressed_blk_array)), PAGE_SIZE);

	return size;
}

static void save_param_authtags(void *data, unsigned short root_swap)
{
	uint32_t nr_tags = atomic_read(&nr_handled_pages);
	uint32_t nr_pages_tags = get_auth_mem_nr_pages(nr_tags);
	void *authpage = auth_mem_start;
	int params_slot;
	int cur_slot;
	int i = 0;

	/*
	 * Allocate a page to save the encryption params
	 */
	params_slot = alloc_swapdev_block(root_swap);
	auth_slot_offset = params_slot + 1;

	while (i++ < nr_pages_tags) {
		cur_slot = alloc_swapdev_block(root_swap);
		write_page(authpage, cur_slot, hb);
		authpage = authpage + PAGE_SIZE;
	}

	pr_info("%s params_slot: %d\n", __func__, params_slot);
	pr_info("%s auth_slot_offset: %d\n", __func__, auth_slot_offset);
	pr_info("%s auth_slot_last: %d\n", __func__, cur_slot);
	pr_info("%s nr_handled_pages: %d\n", __func__, nr_tags);
	pr_info("%s nr_pages_tags: %d\n", __func__, nr_pages_tags);
	pr_info("%s iv: %*phN\n", __func__, IV_SIZE, params->iv);

	params->authslot_count = nr_pages_tags;
	write_page(params, params_slot, hb);

	// Write the array holding the compressed block count to disk
	if (compressed_blk_array) {
		uint32_t size = get_size_of_compression_block_array();
		for (i = 0; i < size / PAGE_SIZE; i++) {
			cur_slot = alloc_swapdev_block(root_swap);
			write_page(compressed_blk_array + (i * PAGE_SIZE), cur_slot, hb);
		}
	}
	pr_info("PM: Encryption took %d ms (%d us/page)\n",
			ktime_to_ms(crypt_ns),
			(ktime_to_us(crypt_ns) / nr_tags));
}

static int init_aead(void)
{
	u8 key[AES256_KEY_SIZE];
	size_t key_len_out;
	int ret;

	if (params || tfm || req)
		return -EPERM;

	if (IS_ERR_OR_NULL(hib_resume_bdev))
		return -EPERM;

	params = (struct qcom_crypto_params *)__get_free_pages(GFP_KERNEL, 0);
	if (!params)
		return -ENOMEM;

	tfm = crypto_alloc_aead("gcm(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		goto tfm_err;
	}

	req = aead_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto req_error;
	}

	aead_request_set_callback(req, 0, NULL, NULL);
	aead_request_set_ad(req, 0);
	ret = crypto_aead_setauthsize(tfm, AUTH_SIZE);
	if (ret)
		goto set_req_error;

	ret = key_mgr_get_key(ILOWPOWERKEYMANAGER_HIBERNATE_WITH_ENCRYPTION, key,
			AES256_KEY_SIZE, &key_len_out);
	if (ret) {
		pr_err("%s failed: key_mgr_get_key: %d\n", __func__, ret);
		goto set_req_error;
	}

	if ((key_len_out != AES256_KEY_SIZE) || *(uint64_t *)key == 0) {
		ret = -EPERM;
		pr_err("%s failed to get key. Got size:%d key:%*phN\n", __func__,
					key_len_out, AES256_KEY_SIZE, key);
		goto set_req_error;
	}

	ret = crypto_aead_setkey(tfm, key, AES256_KEY_SIZE);
	memset(key, 0, AES256_KEY_SIZE);
	if (ret)
		goto set_req_error;

	crypto_aead_clear_flags(tfm, ~0);
	get_random_bytes(params->iv, IV_SIZE);
	crypt_ns = 0;
	blk_array_pos = 0;
	atomic_set(&nr_handled_pages, 0);
	// If this is restore, we use start sector from
	// resume block device as base sector
	base_sector = hib_resume_bdev->bd_part ?
			get_start_sect(hib_resume_bdev) : 0;
	// We adjust in case when image does not start from the first block
	base_sector += (swsusp_header->image - 1) << (PAGE_SHIFT - SECTOR_SHIFT);
	return 0;

set_req_error:
	aead_request_free(req);
	req = NULL;
req_error:
	crypto_free_aead(tfm);
	tfm = NULL;
tfm_err:
	free_pages((unsigned long)params, 0);
	params = NULL;
	return ret;
}

void deinit_aead(void)
{
	if (req) {
		aead_request_free(req);
		req = NULL;
	}

	if (tfm) {
		crypto_free_aead(tfm);
		tfm = NULL;
	}

	if (params) {
		free_pages((unsigned long)params, 0);
		params = NULL;
	}

	if (auth_mem_start) {
		vfree(auth_mem_start);
		auth_mem_start = NULL;
	}

	if (compressed_blk_array) {
		kvfree((void *)compressed_blk_array);
		compressed_blk_array = NULL;
		blk_array_pos = 0;
	}
}

static int hibernate_pm_notifier(struct notifier_block *nb,
				unsigned long event, void *unused)
{
	int ret = 0;
        switch (event) {
	case (PM_HIBERNATION_PREPARE):
		ret = init_aead();
		break;

	case (PM_POST_HIBERNATION):
		deinit_aead();
		break;

	case (PM_RESTORE_PREPARE):
		ret = init_aead();
		if (ret)
			break;
		ret = get_param_authtags();
		if (ret)
			deinit_aead();
		break;
	case (PM_POST_RESTORE):
		deinit_aead();
		break;
	default:
		// We ignore all other events
		break;
	}

	return ret ? NOTIFY_BAD : NOTIFY_DONE;
}

static struct notifier_block pm_nb = {
	.notifier_call = hibernate_pm_notifier,
};

static void init_aes_encrypt(void *data, void *bio_batch)
{
	uint32_t nr_pages_tags = get_auth_mem_nr_pages(snapshot_get_image_size());
	auth_mem_size = nr_pages_tags * PAGE_SIZE;
	auth_mem_start = vmalloc(auth_mem_size);
	if (!auth_mem_start)
		panic("%s: Failed alloc memory for auth tags\n", __func__);
	base_sector = (swsusp_header->image - 1) << (PAGE_SHIFT - SECTOR_SHIFT);
	hb = (struct hib_bio_batch *)bio_batch;
	return;
}

/*
 * Bit(part of swsusp_header_flags) to indicate if the image is uncompressed
 * or not. This is inline with SF_NOCOMPRESS_MODE defined in
 * kernel/power/power.h.
 */
#define SF_NOCOMPRESS_MODE      2

static void hibernated_do_mem_alloc(void *data, unsigned long pages,
	unsigned int swsusp_header_flags, int *ret)
{
	uint32_t size;

	if (swsusp_header_flags & SF_NOCOMPRESS_MODE)
		return;

	size = get_size_of_compression_block_array();

	compressed_blk_array = kvzalloc(size, GFP_KERNEL);
	if (!compressed_blk_array)
		*ret = -ENOMEM;
}

static void hibernate_save_cmp_len(void *data, size_t cmp_len)
{
	uint8_t pages;

	pages = DIV_ROUND_UP(cmp_len, PAGE_SIZE);
	compressed_blk_array[blk_array_pos++] = pages;
}


static ssize_t gethibkey_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	bool new_value;
	int ret = kstrtobool(buf, &new_value);
	if (ret < 0)
		return ret;

	if (new_value) {
		pr_err("Calling get_key_for_hib_exp..");
		get_key_for_hib_exp();
	}

	return count;
}

static struct kobj_attribute gethibkey_attr = __ATTR(gethibkey, 0664, NULL, gethibkey_store);

static int __init qcom_secure_hibernattion_init(void)
{
	int ret;

#ifndef CONFIG_HIBERNATION
	return 0;
#endif
	register_trace_android_vh_encrypt_page(encrypt_page, NULL);
	register_trace_android_vh_init_aes_encrypt(init_aes_encrypt, NULL);
	register_trace_android_vh_skip_swap_map_write(skip_swap_map_write, NULL);
	register_trace_android_vh_store_auth_slot_num(store_auth_slot_num, NULL);
	register_trace_android_vh_post_image_save(save_param_authtags, NULL);
	register_trace_android_vh_hibernate_save_cmp_len(hibernate_save_cmp_len, NULL);
	register_trace_android_vh_hibernated_do_mem_alloc(hibernated_do_mem_alloc, NULL);
	register_trace_android_vh_decrypt_page(decrypt_page, NULL);

	gethibkey_kobj = kobject_create_and_add("gethibkey_kobject", kernel_kobj);
	if (!gethibkey_kobj)
		return -ENOMEM;

	ret = sysfs_create_file(gethibkey_kobj, &gethibkey_attr.attr);
	if (ret)
		kobject_put(gethibkey_kobj);

	ret = register_pm_notifier(&pm_nb);
	if (ret) {
		pr_err("%s: Failed to register nb: %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

module_init(qcom_secure_hibernattion_init);

MODULE_DESCRIPTION("Framework to encrypt a page using a trusted application");
MODULE_LICENSE("GPL");
