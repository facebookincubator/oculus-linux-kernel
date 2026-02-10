// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/pstore.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#define RAWDUMP_DEV_PATH "/dev/block/by-name/rawdump"
#define QCOM_RAWDUMP_SIGNATURE "Raw_Dmp!"
#define MAX_CONFIGURED_SECTIONS 256

struct qcom_minidump_header {
	char signature[8];
	u32 version;
	u32 valid;
	u64 data;
	char context[8];
	u32 reset_trigger;
	u64 dump_size;
	u64 total_size;
	u32 sections_count;
} __packed;
static_assert(sizeof(struct qcom_minidump_header) == 56, "unexpected minidump header size");

struct qcom_minidump_section_header {
	u32 valid;
	u32 version;
	u32 section_type;
	u64 section_offset;
	u64 section_size;
	u64 paddr;
	u64 info;
	char name[20];
} __packed;
static_assert(sizeof(struct qcom_minidump_section_header) == 64, "unexpected minidump section header size");

struct minidump_data {
	struct kobject kobj;
	bool data_loaded;
	struct qcom_minidump_header md_header;
	struct qcom_minidump_section_header *sections;
	struct qcom_minidump_section_header *kboot_log_section;
	char *kboot_log_data;

	/* Combined PStore is a special set of sections that get concatenated later on */
	char *combined_pstore_data;
	size_t combined_pstore_size;
	struct work_struct minidump_init_work;

	/* DebugFS */
	struct dentry *debugfs_root;
	struct debugfs_section_data **debugfs_section_data;
};

static struct minidump_data *g_minidump_data;

#define to_minidump_data(x) container_of(x, struct minidump_data, kobj)

static bool validate_minidump_header(struct qcom_minidump_header *hdr)
{
	if (strncmp(hdr->signature, QCOM_RAWDUMP_SIGNATURE, strlen(QCOM_RAWDUMP_SIGNATURE)) != 0) {
		pr_debug("minidump: Invalid minidump signature: %.8s\n",
			hdr->signature);
		return false;
	}
	if (hdr->valid != 1) {
		pr_debug("minidump: Minidump valid flag not set: %u\n", hdr->valid);
		return false;
	}
	return true;
}

static int load_bio_sectors(struct minidump_data *md, struct block_device *bdev,
					size_t offset_bytes, size_t total_bytes, char *buffer)
{
	struct bio *bio;
	struct page *page;
	int ret = 0;
	const size_t page_aligned_offset = offset_bytes & ~(PAGE_SIZE - 1);
	const size_t offset_in_page = offset_bytes - page_aligned_offset;
	sector_t sector = page_aligned_offset / SECTOR_SIZE;
	size_t copy_size;
	size_t pages_needed;
	char *page_buffer;

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	pages_needed = DIV_ROUND_UP(offset_in_page + total_bytes, PAGE_SIZE);
	for (size_t i = 0; i < pages_needed; i++) {
		bio = bio_alloc(bdev, 1, REQ_OP_READ, GFP_KERNEL);
		if (!bio) {
			__free_page(page);
			return -ENOMEM;
		}

		bio->bi_iter.bi_sector = sector + (i * PAGE_SIZE / SECTOR_SIZE);
		bio_add_page(bio, page, PAGE_SIZE, 0);
		ret = submit_bio_wait(bio);
		bio_put(bio);
		if (ret) {
			pr_err("minidump: Bio read failed for page %zu: %d\n", i, ret);
			__free_page(page);
			return ret;
		}
		page_buffer = page_address(page);

		if (i == 0) {
			/* Skip to starting offset */
			copy_size = min_t(size_t, PAGE_SIZE - offset_in_page, total_bytes);
			memcpy(buffer, page_buffer + offset_in_page, copy_size);
		} else {
			size_t bytes_copied = PAGE_SIZE - offset_in_page + (i - 1) * PAGE_SIZE;
			size_t remaining = total_bytes - bytes_copied;

			copy_size = min_t(size_t, PAGE_SIZE, remaining);
			memcpy(buffer + bytes_copied, page_buffer, copy_size);
		}
	}
	__free_page(page);

	return 0;
}

static struct qcom_minidump_section_header *find_minidump_section(struct minidump_data *md,
							   const char *section_name)
{
	if (!md->sections || !md->data_loaded)
		return NULL;

	for (int i = 0; i < md->md_header.sections_count; i++) {
		if (strncmp(md->sections[i].name, section_name, sizeof(md->sections[i].name)) == 0) {
			pr_debug("minidump: Found section '%s': offset=0x%llx size=0x%llx\n",
				 section_name,
				 md->sections[i].section_offset,
				 md->sections[i].section_size);
			return &md->sections[i];
		}
	}

	pr_info("minidump: Section '%s' not found\n", section_name);
	return NULL;
}

static int load_kboot_log_section(struct minidump_data *md)
{
	struct block_device *bdev;
	int ret = 0;

	if (!md->kboot_log_section || md->kboot_log_data)
		return 0;

	bdev = blkdev_get_by_path(RAWDUMP_DEV_PATH, FMODE_READ, NULL);
	if (IS_ERR(bdev)) {
		pr_err("minidump: Failed to open %s: %ld\n",
			RAWDUMP_DEV_PATH, PTR_ERR(bdev));
		return PTR_ERR(bdev);
	}

	md->kboot_log_data = kzalloc(md->kboot_log_section->section_size, GFP_KERNEL);
	if (!md->kboot_log_data) {
		ret = -ENOMEM;
		goto out_put_bdev;
	}

	ret = load_bio_sectors(md, bdev,
					md->kboot_log_section->section_offset,
					md->kboot_log_section->section_size,
					md->kboot_log_data);
	if (ret) {
		kfree(md->kboot_log_data);
		md->kboot_log_data = NULL;
	}

out_put_bdev:
	blkdev_put(bdev, FMODE_READ);
	return ret;
}

static int load_header(struct minidump_data *md)
{
	struct block_device *bdev;
	int ret = 0;

	bdev = blkdev_get_by_path(RAWDUMP_DEV_PATH, FMODE_READ, NULL);
	if (IS_ERR(bdev)) {
		pr_err("minidump: Failed to open %s: %ld\n", RAWDUMP_DEV_PATH, PTR_ERR(bdev));
		return PTR_ERR(bdev);
	}

	ret = load_bio_sectors(md, bdev, 0, sizeof(struct qcom_minidump_header), (char *)&(md->md_header));
	if (ret) {
		pr_err("minidump: Failed to read minidump header: %d\n", ret);
		goto out_put_bdev;
	}

	if (!validate_minidump_header(&md->md_header)) {
		ret = -ENODEV;
		goto out_put_bdev;
	}

	if (md->md_header.sections_count > MAX_CONFIGURED_SECTIONS) {
		pr_err("minidump: Section count exceeds limit: %u > %u\n",
				md->md_header.sections_count, MAX_CONFIGURED_SECTIONS);
		ret = -EINVAL;
		goto out_put_bdev;
	}

	const size_t total_size = md->md_header.sections_count * sizeof(struct qcom_minidump_section_header);

	md->sections = kzalloc(total_size, GFP_KERNEL);
	if (!md->sections) {
		ret = -ENOMEM;
		goto out_put_bdev;
	}

	ret = load_bio_sectors(md, bdev, sizeof(struct qcom_minidump_header), total_size, (char *)md->sections);
	if (ret) {
		kfree(md->sections);
		md->sections = NULL;
		goto out_put_bdev;
	}
	md->data_loaded = true;

out_put_bdev:
	blkdev_put(bdev, FMODE_READ);
	return ret;
}

static int invalidate_minidump_header(void)
{
	struct block_device *bdev;
	struct bio *bio;
	struct page *page;
	char *page_buffer;
	int ret = 0;

	bdev = blkdev_get_by_path(RAWDUMP_DEV_PATH, FMODE_WRITE, NULL);
	if (IS_ERR(bdev)) {
		pr_err("minidump: Failed to open %s for write: %ld\n",
			 RAWDUMP_DEV_PATH, PTR_ERR(bdev));
		return PTR_ERR(bdev);
	}
	page = alloc_page(GFP_KERNEL);
	if (!page) {
		ret = -ENOMEM;
		goto out_put_bdev;
	}

	page_buffer = page_address(page);
	memset(page_buffer, 0, sizeof(struct qcom_minidump_header));
	bio = bio_alloc(bdev, 1, REQ_OP_WRITE, GFP_KERNEL);
	if (!bio) {
		ret = -ENOMEM;
		goto out_free_page;
	}

	bio->bi_iter.bi_sector = 0;
	bio_add_page(bio, page, PAGE_SIZE, 0);

	ret = submit_bio_wait(bio);
	if (ret)
		pr_err("minidump: Failed to write to bio: %d\n", ret);
	bio_put(bio);

out_free_page:
	__free_page(page);
out_put_bdev:
	blkdev_put(bdev, FMODE_WRITE);
	return ret;
}

static int section_offset_cmp(const void *left, const void *right)
{
	const struct qcom_minidump_section_header * const *section_a = left;
	const struct qcom_minidump_section_header * const *section_b = right;

	if ((*section_a)->section_offset < (*section_b)->section_offset)
		return -1;
	if ((*section_a)->section_offset > (*section_b)->section_offset)
		return 1;
	return 0;
}

static int load_combined_pstore_data(struct minidump_data *md)
{
	struct block_device *bdev;
	struct qcom_minidump_section_header *sections[4];
	size_t total_size = 0;
	int section_count = 0;
	int ret = 0;
	struct qcom_minidump_section_header *kdmesg_section = find_minidump_section(md, "md_KDMESG.BIN");
	struct qcom_minidump_section_header *kconsole_section = find_minidump_section(md, "md_KCONSOLE.BIN");
	struct qcom_minidump_section_header *kftrace_section = find_minidump_section(md, "md_KFTRACE.BIN");
	struct qcom_minidump_section_header *kpmsg_section = find_minidump_section(md, "md_KPMSG.BIN");

	if (md->combined_pstore_data)
		return 0;

	if (kdmesg_section)
		sections[section_count++] = kdmesg_section;
	if (kconsole_section)
		sections[section_count++] = kconsole_section;
	if (kftrace_section)
		sections[section_count++] = kftrace_section;
	if (kpmsg_section)
		sections[section_count++] = kpmsg_section;

	/*
	 * Sort sections by their actual file offset to ensure correct order, This assumes that
	 * minidump is writing out the sections in the order that ramoops saves them.
	 */
	sort(sections, section_count, sizeof(struct qcom_minidump_section_header *),
	     section_offset_cmp, NULL);

	if (section_count == 0) {
		pr_info("minidump: No pstore sections found for combined buffer\n");
		return -ENODATA;
	}

	for (int i = 0; i < section_count; i++)
		total_size += sections[i]->section_size;

	bdev = blkdev_get_by_path(RAWDUMP_DEV_PATH, FMODE_READ, NULL);
	if (IS_ERR(bdev)) {
		pr_err("minidump: Failed to open %s: %ld\n",
			RAWDUMP_DEV_PATH, PTR_ERR(bdev));
		return PTR_ERR(bdev);
	}

	md->combined_pstore_data = kzalloc(total_size, GFP_KERNEL);
	if (!md->combined_pstore_data) {
		ret = -ENOMEM;
		goto out_put_bdev;
	}

	/* Check if sections are contiguous */
	u64 start_offset = sections[0]->section_offset;
	bool contiguous = true;
	u64 expected_offset = sections[0]->section_offset;

	for (int i = 0; i < section_count; i++) {
		if (sections[i]->section_offset != expected_offset) {
			contiguous = false;
			break;
		}
		expected_offset += sections[i]->section_size;
	}

	if (!contiguous) {
		pr_err("minidump: pstore sections are not contiguous - cannot create combined buffer\n");
		ret = -EINVAL;
		goto out_free_and_put_bdev;
	}
	ret = load_bio_sectors(md, bdev, start_offset, total_size,
			       md->combined_pstore_data);
	if (ret) {
		pr_err("minidump: Failed to load contiguous pstore sections: %d\n", ret);
		goto out_free_and_put_bdev;
	}
	md->combined_pstore_size = total_size;
	blkdev_put(bdev, FMODE_READ);

	return 0;

out_free_and_put_bdev:
	kfree(md->combined_pstore_data);
	md->combined_pstore_data = NULL;
out_put_bdev:
	blkdev_put(bdev, FMODE_READ);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
struct debugfs_section_data {
	struct minidump_data *md;
	struct qcom_minidump_section_header *section;
};

static ssize_t debugfs_section_read(struct file *file, char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct debugfs_section_data *data = file->private_data;
	struct minidump_data *md = data->md;
	struct qcom_minidump_section_header *section = data->section;
	struct block_device *bdev;
	char *buffer;
	int ret;
	loff_t offset = *ppos;
	size_t to_read;

	if (!section || !md->data_loaded)
		return -ENODATA;

	if (offset < 0)
		return -EINVAL;

	if (offset >= section->section_size)
		return 0;

	to_read = min_t(size_t, count, section->section_size - offset);

	buffer = kzalloc(to_read, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	bdev = blkdev_get_by_path(RAWDUMP_DEV_PATH, FMODE_READ, NULL);
	if (IS_ERR(bdev)) {
		pr_err("minidump: Failed to open %s: %ld\n",
			RAWDUMP_DEV_PATH, PTR_ERR(bdev));
		ret = PTR_ERR(bdev);
		goto out_free_buffer;
	}

	ret = load_bio_sectors(md, bdev, section->section_offset + offset,
			       to_read, buffer);
	if (ret) {
		pr_err("minidump: Failed to read section data: %d\n", ret);
		goto out_put_bdev;
	}

	if (copy_to_user(user_buf, buffer, to_read)) {
		ret = -EFAULT;
		goto out_put_bdev;
	}

	*ppos += to_read;
	ret = to_read;

out_put_bdev:
	blkdev_put(bdev, FMODE_READ);
out_free_buffer:
	kfree(buffer);
	return ret;
}

static int debugfs_section_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debugfs_section_fops = {
	.owner = THIS_MODULE,
	.open = debugfs_section_open,
	.read = debugfs_section_read,
	.llseek = default_llseek,
};

static int minidump_create_debugfs(struct minidump_data *md)
{
	int i;
	struct debugfs_section_data *section_data;

	md->debugfs_root = debugfs_create_dir("minidump_sections", NULL);
	md->debugfs_section_data =
		kcalloc(md->md_header.sections_count,
			sizeof(struct debugfs_section_data *),
			GFP_KERNEL);
	if (!md->debugfs_section_data) {
		debugfs_remove_recursive(md->debugfs_root);
		md->debugfs_root = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < md->md_header.sections_count; i++) {
		char sanitized_name[32];
		int j, k;

		section_data = kzalloc(sizeof(*section_data), GFP_KERNEL);
		if (!section_data)
			continue;

		section_data->md = md;
		section_data->section = &md->sections[i];
		md->debugfs_section_data[i] = section_data;

		for (j = 0, k = 0; j < sizeof(md->sections[i].name) &&
				   k < sizeof(sanitized_name) - 1; j++) {
			char c = md->sections[i].name[j];

			if (c == '\0')
				break;
			if (c == '/' || c == '\\')
				c = '_';
			sanitized_name[k++] = c;
		}
		sanitized_name[k] = '\0';

		debugfs_create_file(sanitized_name, 0444, md->debugfs_root,
				    section_data, &debugfs_section_fops);
	}

	return 0;
}

static void minidump_remove_debugfs(struct minidump_data *md)
{
	int i;

	if (md->debugfs_section_data) {
		for (i = 0; i < md->md_header.sections_count; i++)
			kfree(md->debugfs_section_data[i]);
		kfree(md->debugfs_section_data);
		md->debugfs_section_data = NULL;
	}

	debugfs_remove_recursive(md->debugfs_root);
	md->debugfs_root = NULL;
}

#else /* CONFIG_DEBUG_FS */
static int minidump_create_debugfs(struct minidump_data *md)
{
	return 0;
}
static void minidump_remove_debugfs(struct minidump_data *md) {}
#endif /* CONFIG_DEBUG_FS */

static void minidump_work_handler(struct work_struct *work)
{
	int ret;
	struct minidump_data *md = container_of(work, struct minidump_data, minidump_init_work);

	/* non-fatal as there may not have been a crash */
	ret = load_header(md);
	if (ret)
		return;

	md->kboot_log_section = find_minidump_section(md, "md_KBOOT_LOG.BIN");

	ret = minidump_create_debugfs(md);
	if (ret)
		pr_warn("minidump: Failed to create debugfs entries: %d\n", ret);

	ret = load_combined_pstore_data(md);
	if (ret) {
		pr_warn("minidump: Failed to preload pstore data: %d\n", ret);
		return;
	}

	ret = ramoops_populate_from_buffer(virt_to_phys(md->combined_pstore_data), md->combined_pstore_size);
	if (ret)
		pr_err("minidump: Failed to populate pstore data: %d\n", ret);

	ret = invalidate_minidump_header();
	if (ret)
		pr_warn("minidump: Failed to invalidate minidump header: %d\n", ret);
}

static ssize_t header_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct minidump_data *md = to_minidump_data(kobj);
	int len = 0;

	if (!md->data_loaded)
		return scnprintf(buf, PAGE_SIZE, "Minidump data not loaded\n");

	len += scnprintf(buf + len, PAGE_SIZE - len,
			"Minidump signature: %.8s\n"
			"Version: %u\n"
			"Sections: %u\n",
			md->md_header.signature,
			md->md_header.version,
			md->md_header.sections_count);

	return len;
}

static ssize_t sections_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct minidump_data *md = to_minidump_data(kobj);
	int len = 0;

	if (!md->data_loaded)
		return scnprintf(buf, PAGE_SIZE, "Minidump data not loaded\n");

	if (!md->sections)
		return scnprintf(buf, PAGE_SIZE, "No sections available\n");

	/* Approximate size of the print buffer per section */
	const int section_print_size = 50;

	for (int i = 0; i < md->md_header.sections_count && len < PAGE_SIZE - section_print_size; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "Section %d: Name: %.20s\n",
				 i,
				 md->sections[i].name);
	}

	return len;
}

static ssize_t kboot_log_read(struct file *file, struct kobject *kobj,
			     struct bin_attribute *attr, char *buf,
			     loff_t offset, size_t count)
{
	struct minidump_data *md = to_minidump_data(kobj);
	int ret;

	if (!md->data_loaded)
		return -ENODATA;

	if (!md->kboot_log_section)
		return -ENODATA;

	if (offset < 0)
		return -EINVAL;

	if (!md->kboot_log_data) {
		ret = load_kboot_log_section(md);
		if (ret)
			return ret;
	}

	if (offset >= md->kboot_log_section->section_size)
		return 0;

	if (offset + count > md->kboot_log_section->section_size)
		count = md->kboot_log_section->section_size - offset;

	memcpy(buf, md->kboot_log_data + offset, count);
	return count;
}

static ssize_t pstore_read(struct file *file, struct kobject *kobj,
			   struct bin_attribute *attr, char *buf,
			   loff_t offset, size_t count)
{
	struct minidump_data *md = to_minidump_data(kobj);
	int ret;

	if (!md->data_loaded)
		return -ENODATA;

	if (offset < 0)
		return -EINVAL;

	/* Load combined pstore data if not already loaded */
	if (!md->combined_pstore_data) {
		ret = load_combined_pstore_data(md);
		if (ret)
			return ret;
	}

	if (offset >= md->combined_pstore_size)
		return 0;

	if (offset + count > md->combined_pstore_size)
		count = md->combined_pstore_size - offset;

	memcpy(buf, md->combined_pstore_data + offset, count);
	return count;
}

static struct kobj_attribute header_attr = __ATTR_RO(header);
static struct kobj_attribute sections_attr = __ATTR_RO(sections);

static struct bin_attribute kboot_log_attr = {
	.attr = {
		.name = "kboot_log",
		.mode = 0444,
	},
	.size = 0,
	.read = kboot_log_read,
};

static struct bin_attribute pstore_attr = {
	.attr = {
		.name = "pstore",
		.mode = 0444,
	},
	.size = 0,
	.read = pstore_read,
};

static struct attribute *minidump_attrs[] = {
	&header_attr.attr,
	&sections_attr.attr,
	NULL,
};

static struct bin_attribute *minidump_bin_attrs[] = {
	&kboot_log_attr,
	&pstore_attr,
	NULL,
};

static const struct attribute_group minidump_attr_group = {
	.attrs = minidump_attrs,
	.bin_attrs = minidump_bin_attrs,
};

static void minidump_kobj_release(struct kobject *kobj)
{
	struct minidump_data *data = to_minidump_data(kobj);

	kfree(data->sections);
	kfree(data->kboot_log_data);
	kfree(data->combined_pstore_data);
	kfree(data);
}

static struct kobj_type minidump_ktype = {
	.release = minidump_kobj_release,
	.sysfs_ops = &kobj_sysfs_ops,
};

static int __init minidump_parser_init(void)
{
	struct minidump_data *md;
	int ret;

	pr_info("minidump: Initializing Meta Qualcomm minidump parser\n");

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md)
		return -ENOMEM;

	g_minidump_data = md;

	kobject_init(&md->kobj, &minidump_ktype);
	ret = kobject_add(&md->kobj, fs_kobj, "minidump");
	if (ret) {
		pr_err("minidump: Failed to add kobject\n");
		goto out_free_data;
	}

	ret = sysfs_create_group(&md->kobj, &minidump_attr_group);
	if (ret) {
		pr_err("minidump: Failed to create sysfs files: %d\n", ret);
		goto out_free_data;
	}

	INIT_WORK(&md->minidump_init_work, minidump_work_handler);
	schedule_work(&md->minidump_init_work);

	return 0;

out_free_data:
	kobject_put(&md->kobj);
	g_minidump_data = NULL;
	return ret;
}

static void __exit minidump_parser_exit(void)
{
	if (g_minidump_data) {
		cancel_work_sync(&g_minidump_data->minidump_init_work);
		minidump_remove_debugfs(g_minidump_data);
		sysfs_remove_group(&g_minidump_data->kobj, &minidump_attr_group);
		kobject_put(&g_minidump_data->kobj);
		g_minidump_data = NULL;
	}
}

subsys_initcall(minidump_parser_init);
module_exit(minidump_parser_exit);

MODULE_AUTHOR("Meta Platforms, Inc.");
MODULE_DESCRIPTION("Meta Qualcomm Minidump Parser");
MODULE_LICENSE("GPL v2");
