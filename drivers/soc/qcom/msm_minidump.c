// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "Minidump: " fmt

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/android_debug_symbols.h>
#include <linux/soc/qcom/smem.h>
#include <soc/qcom/minidump.h>
#include "minidump_private.h"
#include "elf.h"

#define MAX_NUM_ENTRIES         (CONFIG_MINIDUMP_MAX_ENTRIES + 1)
#define MAX_STRTBL_SIZE		(MAX_NUM_ENTRIES * MAX_REGION_NAME_LENGTH)

/**
 * md_table : Local Minidump toc holder
 * @num_regions : Number of regions requested
 * @md_ss_toc  : HLOS toc pointer
 * @md_gbl_toc : Global toc pointer
 * @md_regions : HLOS regions base pointer
 * @entry : array of HLOS regions requested
 */
struct md_table {
	u32			revision;
	u32                     num_regions;
	struct md_ss_toc	*md_ss_toc;
	struct md_global_toc	*md_gbl_toc;
	struct md_ss_region	*md_regions;
	struct md_region	entry[MAX_NUM_ENTRIES];
};

/**
 * md_elfhdr: Minidump table elf header
 * @ehdr: elf main header
 * @shdr: Section header
 * @phdr: Program header
 * @elf_offset: section offset in elf
 * @strtable_idx: string table current index position
 */
struct md_elfhdr {
	struct elfhdr		*ehdr;
	struct elf_shdr		*shdr;
	struct elf_phdr		*phdr;
	u64			elf_offset;
	u64			strtable_idx;
};

/* Protect elfheader and smem table from deferred calls contention */
static DEFINE_SPINLOCK(mdt_lock);
static DEFINE_RWLOCK(mdt_remove_lock);
static struct md_table		minidump_table;
static struct md_elfhdr		minidump_elfheader;
static int first_removed_entry = INT_MAX;
static bool md_init_done;

/* Number of pending entries to be added in ToC regions */
static unsigned int pendings;

static inline char *elf_lookup_string(struct elfhdr *hdr, int offset)
{
	char *strtab = elf_str_table(hdr);

	if ((strtab == NULL) || (minidump_elfheader.strtable_idx < offset))
		return NULL;
	return strtab + offset;
}

static inline unsigned int set_section_name(const char *name)
{
	char *strtab = elf_str_table(minidump_elfheader.ehdr);
	int idx = minidump_elfheader.strtable_idx;
	int ret = 0;

	if ((strtab == NULL) || (name == NULL))
		return 0;

	ret = idx;
	idx += strlcpy((strtab + idx), name, MAX_REGION_NAME_LENGTH);
	minidump_elfheader.strtable_idx = idx + 1;

	return ret;
}

struct md_region *md_get_region(char *name)
{
	struct md_region *mdr;
	int i, regno = minidump_table.num_regions;

	for (i = 0; i < regno; i++) {
		mdr = &minidump_table.entry[i];
		if (!strcmp(mdr->name, name))
			return mdr;
	}
	return NULL;
}

static inline int md_region_num(const char *name, int *seqno)
{
	struct md_ss_region *mde = minidump_table.md_regions;
	int i, regno = minidump_table.md_ss_toc->ss_region_count;
	int ret = -EINVAL;

	for (i = 0; i < regno; i++, mde++) {
		if (!strcmp(mde->name, name)) {
			ret = i;
			if (mde->seq_num > *seqno)
				*seqno = mde->seq_num;
		}
	}
	return ret;
}

static inline int md_entry_num(const struct md_region *entry)
{
	struct md_region *mdr;
	int i, regno = minidump_table.num_regions;

	for (i = 0; i < regno; i++) {
		mdr = &minidump_table.entry[i];
		if (!strcmp(mdr->name, entry->name))
			return i;
	}
	return -ENOENT;
}

/* Update Mini dump table in SMEM */
static void md_update_ss_toc(const struct md_region *entry)
{
	struct md_ss_region *mdr;
	struct elfhdr *hdr = minidump_elfheader.ehdr;
	struct elf_shdr *shdr = elf_section(hdr, hdr->e_shnum++);
	struct elf_phdr *phdr = elf_program(hdr, hdr->e_phnum++);
	int seq = 0, reg_cnt = minidump_table.md_ss_toc->ss_region_count;

	mdr = &minidump_table.md_regions[reg_cnt];

	strlcpy(mdr->name, entry->name, sizeof(mdr->name));
	mdr->region_base_address = entry->phys_addr;
	mdr->region_size = entry->size;
	if (md_region_num(entry->name, &seq) >= 0)
		mdr->seq_num = seq + 1;

	/* Update elf header */
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_name = set_section_name(mdr->name);
	shdr->sh_addr = (elf_addr_t)entry->virt_addr;
	shdr->sh_size = mdr->region_size;
	shdr->sh_flags = SHF_WRITE;
	shdr->sh_offset = minidump_elfheader.elf_offset;
	shdr->sh_entsize = 0;

	phdr->p_type = PT_LOAD;
	phdr->p_offset = minidump_elfheader.elf_offset;
	phdr->p_vaddr = entry->virt_addr;
	phdr->p_paddr = entry->phys_addr;
	phdr->p_filesz = phdr->p_memsz =  mdr->region_size;
	phdr->p_flags = PF_R | PF_W;
	minidump_elfheader.elf_offset += shdr->sh_size;
	mdr->md_valid = MD_REGION_VALID;
	minidump_table.md_ss_toc->ss_region_count++;
}

bool msm_minidump_enabled(void)
{
	bool ret = false;
	unsigned long flags;

	spin_lock_irqsave(&mdt_lock, flags);
	if (minidump_table.md_ss_toc &&
		(minidump_table.md_ss_toc->md_ss_enable_status ==
		 MD_SS_ENABLED))
		ret = true;
	spin_unlock_irqrestore(&mdt_lock, flags);
	return ret;
}
EXPORT_SYMBOL(msm_minidump_enabled);

static inline int validate_region(const struct md_region *entry)
{
	if (!entry)
		return -EINVAL;

	if ((strlen(entry->name) > MAX_NAME_LENGTH) || !entry->virt_addr ||
		(!IS_ALIGNED(entry->size, 4))) {
		pr_err("Invalid entry details\n");
		return -EINVAL;
	}

	return 0;
}

int msm_minidump_update_region(int regno, const struct md_region *entry)
{
	int ret = 0;
	struct md_region *mdr;
	struct md_ss_region *mdssr;
	struct elfhdr *hdr = minidump_elfheader.ehdr;
	struct elf_shdr *shdr;
	struct elf_phdr *phdr;
	unsigned long flags;

	/* Ensure that init completes before we update regions */
	if (!smp_load_acquire(&md_init_done))
		return -EINVAL;

	if (validate_region(entry) || (regno >= MAX_NUM_ENTRIES))
		return -EINVAL;

	read_lock_irqsave(&mdt_remove_lock, flags);

	if (regno >= first_removed_entry) {
		pr_err("Region:[%s] was moved\n", entry->name);
		ret = -EINVAL;
		goto err_unlock;
	}

	ret = md_entry_num(entry);
	if (ret < 0) {
		pr_err("Region:[%s] does not exist to update.\n", entry->name);
		goto err_unlock;
	}

	mdr = &minidump_table.entry[regno];
	mdr->virt_addr = entry->virt_addr;
	mdr->phys_addr = entry->phys_addr;

	mdssr = &minidump_table.md_regions[regno + 1];
	mdssr->region_base_address = entry->phys_addr;

	shdr = elf_section(hdr, regno + 4);
	phdr = elf_program(hdr, regno + 1);

	shdr->sh_addr = (elf_addr_t)entry->virt_addr;
	phdr->p_vaddr = entry->virt_addr;
	phdr->p_paddr = entry->phys_addr;

err_unlock:
	read_unlock_irqrestore(&mdt_remove_lock, flags);

	return ret;
}
EXPORT_SYMBOL(msm_minidump_update_region);

#ifdef CONFIG_MINIDUMP_PSTORE_ONLY
/* File names here must be in sync with register_pstore_info() in drivers/soc/qcom/minidump_log.c */
const char *pstore_region_list[] = {"KCONSOLE", "KDMESG", "KPMSG"};
bool msm_minidump_is_region_in_pstore(const struct md_region *entry)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pstore_region_list); i++) {
		if (strcmp(pstore_region_list[i], entry->name) == 0)
			return true;
	}
	return false;
}
#endif

int msm_minidump_add_region(const struct md_region *entry)
{
	u32 entries;
	u32 toc_init;
	struct md_region *mdr;
	unsigned long flags;

	if (validate_region(entry))
		return -EINVAL;

	spin_lock_irqsave(&mdt_lock, flags);

#ifdef CONFIG_MINIDUMP_PSTORE_ONLY
	if (!msm_minidump_is_region_in_pstore(entry)) {
		spin_unlock_irqrestore(&mdt_lock, flags);
		pr_info("Ignoring minidump region %s.\n", entry->name);
		return minidump_table.num_regions;
	}
#endif

	if (md_entry_num(entry) >= 0) {
		spin_unlock_irqrestore(&mdt_lock, flags);
		pr_info("Entry name already exist.\n");
		return -EEXIST;
	}

	entries = minidump_table.num_regions;
	if (entries >= MAX_NUM_ENTRIES) {
		pr_err("Maximum entries reached.\n");
		spin_unlock_irqrestore(&mdt_lock, flags);
		return -ENOMEM;
	}

	toc_init = 0;
	if (minidump_table.md_ss_toc &&
		(minidump_table.md_ss_toc->md_ss_enable_status ==
		MD_SS_ENABLED)) {
		toc_init = 1;
		if (minidump_table.md_ss_toc->ss_region_count >= MAX_NUM_ENTRIES) {
			spin_unlock_irqrestore(&mdt_lock, flags);
			pr_err("Maximum regions in minidump table reached.\n");
			return -ENOMEM;
		}
	}

	mdr = &minidump_table.entry[entries];
	strlcpy(mdr->name, entry->name, sizeof(mdr->name));
	mdr->virt_addr = entry->virt_addr;
	mdr->phys_addr = entry->phys_addr;
	mdr->size = entry->size;
	mdr->id = entry->id;

	minidump_table.num_regions = entries + 1;

	if (toc_init)
		md_update_ss_toc(entry);
	else
		pendings++;

	spin_unlock_irqrestore(&mdt_lock, flags);

	return entries;
}
EXPORT_SYMBOL(msm_minidump_add_region);

int msm_minidump_clear_headers(const struct md_region *entry)
{
	struct elfhdr *hdr = minidump_elfheader.ehdr;
	struct elf_shdr *shdr = NULL, *tshdr = NULL;
	struct elf_phdr *phdr = NULL, *tphdr = NULL;
	int pidx, shidx, strln, i;
	char *shname;
	u64 esize;

	esize = entry->size;
	for (i = 0; i < hdr->e_phnum; i++) {
		phdr = elf_program(hdr, i);
		if ((phdr->p_paddr == entry->phys_addr) &&
			(phdr->p_memsz == entry->size))
			break;
	}
	if (i == hdr->e_phnum) {
		pr_err("Cannot find entry in elf\n");
		return -EINVAL;
	}
	pidx = i;

	for (i = 0; i < hdr->e_shnum; i++) {
		shdr = elf_section(hdr, i);
		shname = elf_lookup_string(hdr, shdr->sh_name);
		if (shname && !strcmp(shname, entry->name))
			if ((shdr->sh_addr == entry->virt_addr) &&
				(shdr->sh_size == entry->size))
				break;

	}
	if (i == hdr->e_shnum) {
		pr_err("Cannot find entry in elf\n");
		return -EINVAL;
	}
	shidx = i;

	if (shdr->sh_offset != phdr->p_offset) {
		pr_err("Invalid entry details in elf, Minidump broken..\n");
		return -EINVAL;
	}

	/* Clear name in string table */
	strln = strlen(shname) + 1;
	memmove(shname, shname + strln,
		(minidump_elfheader.strtable_idx - shdr->sh_name));
	minidump_elfheader.strtable_idx -= strln;

	/* Clear program header */
	tphdr = elf_program(hdr, pidx);
	for (i = pidx; i < hdr->e_phnum - 1; i++) {
		tphdr = elf_program(hdr, i + 1);
		phdr = elf_program(hdr, i);
		memcpy(phdr, tphdr, sizeof(struct elf_phdr));
		phdr->p_offset = phdr->p_offset - esize;
	}
	memset(tphdr, 0, sizeof(struct elf_phdr));
	hdr->e_phnum--;

	/* Clear section header */
	tshdr = elf_section(hdr, shidx);
	for (i = shidx; i < hdr->e_shnum - 1; i++) {
		tshdr = elf_section(hdr, i + 1);
		shdr = elf_section(hdr, i);
		memcpy(shdr, tshdr, sizeof(struct elf_shdr));
		shdr->sh_offset -= esize;
		shdr->sh_name -= strln;
	}
	memset(tshdr, 0, sizeof(struct elf_shdr));
	hdr->e_shnum--;

	minidump_elfheader.elf_offset -= esize;
	return 0;
}

int msm_minidump_remove_region(const struct md_region *entry)
{
	int rcount, ecount, seq = 0, rgno, entryno, ret;
	unsigned long flags;

	if (!entry || !minidump_table.md_ss_toc ||
		(minidump_table.md_ss_toc->md_ss_enable_status !=
						MD_SS_ENABLED))
		return -EINVAL;

	spin_lock_irqsave(&mdt_lock, flags);
	write_lock(&mdt_remove_lock);

#ifdef CONFIG_MINIDUMP_PSTORE_ONLY
	if (!msm_minidump_is_region_in_pstore(entry)) {
		write_unlock(&mdt_remove_lock);
		spin_unlock_irqrestore(&mdt_lock, flags);
		pr_info("Ignoring minidump region %s.\n", entry->name);
		return 0;
	}
#endif

	ret = md_entry_num(entry);
	if (ret < 0) {
		write_unlock(&mdt_remove_lock);
		spin_unlock_irqrestore(&mdt_lock, flags);
		pr_info("Not able to find the entry %s in table\n", entry->name);
		return ret;
	}
	entryno = ret;
	rgno = md_region_num(entry->name, &seq);
	if (rgno < 0) {
		write_unlock(&mdt_remove_lock);
		spin_unlock_irqrestore(&mdt_lock, flags);
		pr_err("Not able to find the region %s (%d,%d) in table\n",
			entry->name, entryno, rgno);
		return -EINVAL;
	}
	ecount = minidump_table.num_regions;
	rcount = minidump_table.md_ss_toc->ss_region_count;
	if (first_removed_entry > entryno)
		first_removed_entry = entryno;
	minidump_table.md_ss_toc->md_ss_toc_init = 0;

	/* Remove entry from: entry list, ss region list and elf header */
	memmove(&minidump_table.entry[entryno],
		&minidump_table.entry[entryno + 1],
		((ecount - entryno - 1) * sizeof(struct md_region)));
	memset(&minidump_table.entry[ecount - 1], 0, sizeof(struct md_region));


	memmove(&minidump_table.md_regions[rgno],
		&minidump_table.md_regions[rgno + 1],
		((rcount - rgno - 1) * sizeof(struct md_ss_region)));
	memset(&minidump_table.md_regions[rcount - 1], 0,
					sizeof(struct md_ss_region));

	ret = msm_minidump_clear_headers(entry);
	if (ret)
		goto out;

	minidump_table.md_ss_toc->ss_region_count--;
	minidump_table.md_ss_toc->md_ss_toc_init = 1;
	minidump_table.num_regions--;
out:
	write_unlock(&mdt_remove_lock);
	spin_unlock_irqrestore(&mdt_lock, flags);

	if (ret)
		pr_err("Minidump is broken..disable Minidump collection\n");
	return ret;
}
EXPORT_SYMBOL(msm_minidump_remove_region);

static int msm_minidump_add_header(void)
{
#ifndef CONFIG_MINIDUMP_PSTORE_ONLY
	struct md_ss_region *mdreg = &minidump_table.md_regions[0];
#endif
	struct elfhdr *ehdr;
	struct elf_shdr *shdr;
	struct elf_phdr *phdr;
	unsigned int strtbl_off, elfh_size, phdr_off;
	char *banner, *linux_banner;

	linux_banner = android_debug_symbol(ADS_LINUX_BANNER);
	/* Header buffer contains:
	 * elf header, MAX_NUM_ENTRIES+4 of section and program elf headers,
	 * string table section and linux banner.
	 */
	elfh_size = sizeof(*ehdr) + MAX_STRTBL_SIZE +
			(strlen(linux_banner) + 1) +
			((sizeof(*shdr) + sizeof(*phdr))
			 * (MAX_NUM_ENTRIES + 4));

	elfh_size = ALIGN(elfh_size, 4);

	minidump_elfheader.ehdr = kzalloc(elfh_size, GFP_KERNEL);
	if (!minidump_elfheader.ehdr)
		return -ENOMEM;

#ifndef CONFIG_MINIDUMP_PSTORE_ONLY
	strlcpy(mdreg->name, "KELF_HEADER", sizeof(mdreg->name));
	mdreg->region_base_address = virt_to_phys(minidump_elfheader.ehdr);
	mdreg->region_size = elfh_size;
	mdreg->md_valid = MD_REGION_VALID;
	// Set to one because this driver assumes ELF header is always the first region
	minidump_table.md_ss_toc->ss_region_count = 1;
#endif

	ehdr = minidump_elfheader.ehdr;
	/* Assign section/program headers offset */
	minidump_elfheader.shdr = shdr = (struct elf_shdr *)(ehdr + 1);
	minidump_elfheader.phdr = phdr =
				 (struct elf_phdr *)(shdr + MAX_NUM_ENTRIES);
	phdr_off = sizeof(*ehdr) + (sizeof(*shdr) * MAX_NUM_ENTRIES);

	memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
	ehdr->e_ident[EI_CLASS] = ELF_CLASS;
	ehdr->e_ident[EI_DATA] = ELF_DATA;
	ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	ehdr->e_ident[EI_OSABI] = ELF_OSABI;
	ehdr->e_type = ET_CORE;
	ehdr->e_machine  = ELF_ARCH;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_ehsize = sizeof(*ehdr);
	ehdr->e_phoff = phdr_off;
	ehdr->e_phentsize = sizeof(*phdr);
	ehdr->e_shoff = sizeof(*ehdr);
	ehdr->e_shentsize = sizeof(*shdr);
	ehdr->e_shstrndx = 1;

	minidump_elfheader.elf_offset = elfh_size;

	/*
	 * First section header should be NULL,
	 * 2nd section is string table.
	 */
	minidump_elfheader.strtable_idx = 1;
	strtbl_off = sizeof(*ehdr) +
			((sizeof(*phdr) + sizeof(*shdr)) * MAX_NUM_ENTRIES);
	shdr++;
	shdr->sh_type = SHT_STRTAB;
	shdr->sh_offset = (elf_addr_t)strtbl_off;
	shdr->sh_size = MAX_STRTBL_SIZE;
	shdr->sh_entsize = 0;
	shdr->sh_flags = 0;
	shdr->sh_name = set_section_name("STR_TBL");
	shdr++;

	/* 3rd section is for minidump_table VA, used by parsers */
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_entsize = 0;
	shdr->sh_flags = 0;
	shdr->sh_addr = (elf_addr_t)&minidump_table;
	shdr->sh_name = set_section_name("minidump_table");
	shdr++;

	/* 4th section is linux banner */
	banner = (char *)ehdr + strtbl_off + MAX_STRTBL_SIZE;
	strlcpy(banner, linux_banner, MAX_STRTBL_SIZE);

	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_offset = (elf_addr_t)(strtbl_off + MAX_STRTBL_SIZE);
	shdr->sh_size = strlen(linux_banner) + 1;
	shdr->sh_addr = (elf_addr_t)linux_banner;
	shdr->sh_entsize = 0;
	shdr->sh_flags = SHF_WRITE;
	shdr->sh_name = set_section_name("linux_banner");

	phdr->p_type = PT_LOAD;
	phdr->p_offset = (elf_addr_t)(strtbl_off + MAX_STRTBL_SIZE);
	phdr->p_vaddr = (elf_addr_t)linux_banner;
	phdr->p_paddr = virt_to_phys(linux_banner);
	phdr->p_filesz = phdr->p_memsz = strlen(linux_banner) + 1;
	phdr->p_flags = PF_R | PF_W;

	/* Update headers count*/
	ehdr->e_phnum = 1;
	ehdr->e_shnum = 4;

	return 0;
}

static int msm_minidump_driver_remove(struct platform_device *pdev)
{
	/* TO-DO.
	 *Free the required resources and set the global
	 * variables as minidump is not initialized.
	 */
	return 0;
}

static int msm_minidump_driver_probe(struct platform_device *pdev)
{
	unsigned int i;
	size_t size;
	struct md_region *mdr;
	struct md_global_toc *md_global_toc;
	struct md_ss_toc *md_ss_toc;
	unsigned long flags;

	/* Get Minidump table */
	md_global_toc = qcom_smem_get(QCOM_SMEM_HOST_ANY, SBL_MINIDUMP_SMEM_ID,
				      &size);
	if (IS_ERR_OR_NULL(md_global_toc)) {
		pr_err("SMEM is not initialized.\n");
		return PTR_ERR(md_global_toc);
	}

	/*Check global minidump support initialization */
	if (!md_global_toc->md_toc_init) {
		pr_err("System Minidump TOC not initialized\n");
		return -ENODEV;
	}

	minidump_table.md_gbl_toc = md_global_toc;
	minidump_table.revision = md_global_toc->md_revision;
	md_ss_toc = &md_global_toc->md_ss_toc[MD_SS_HLOS_ID];

	md_ss_toc->encryption_status = MD_SS_ENCR_DONE;
	md_ss_toc->encryption_required = MD_SS_ENCR_NOTREQ;

	minidump_table.md_ss_toc = md_ss_toc;
	minidump_table.md_regions = devm_kzalloc(&pdev->dev, (MAX_NUM_ENTRIES *
				sizeof(struct md_ss_region)), GFP_KERNEL);
	if (!minidump_table.md_regions)
		return -ENOMEM;

	md_ss_toc->md_ss_smem_regions_baseptr =
				virt_to_phys(minidump_table.md_regions);

	/* First entry would be ELF header */
	msm_minidump_add_header();

	/* Add pending entries to HLOS TOC */
	spin_lock_irqsave(&mdt_lock, flags);
	md_ss_toc->md_ss_toc_init = 1;
	md_ss_toc->md_ss_enable_status = MD_SS_ENABLED;
	for (i = 0; i < pendings; i++) {
		mdr = &minidump_table.entry[i];
		md_update_ss_toc(mdr);
	}

	pendings = 0;
	spin_unlock_irqrestore(&mdt_lock, flags);

	/* All updates above should be visible, before init completes */
	smp_store_release(&md_init_done, true);
	msm_minidump_log_init();
	pr_info("Enabled with max number of regions %d\n",
		CONFIG_MINIDUMP_MAX_ENTRIES);

	return 0;
}

static const struct of_device_id msm_minidump_of_match[] = {
	{ .compatible = "qcom,minidump" },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_minidump_of_match);

static struct platform_driver msm_minidump_driver = {
	.driver = {
		.name = "qcom-minidump",
		.of_match_table = msm_minidump_of_match,
	},
	.probe = msm_minidump_driver_probe,
	.remove = msm_minidump_driver_remove,
};
module_platform_driver(msm_minidump_driver);

MODULE_DESCRIPTION("MSM Mini Dump Driver");
MODULE_LICENSE("GPL v2");
