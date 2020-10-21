// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/staging/erofs/dir.c
 *
 * Copyright (C) 2017-2018 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of the Linux
 * distribution for more details.
 */
#include "internal.h"

static const unsigned char erofs_filetype_table[EROFS_FT_MAX] = {
	[EROFS_FT_UNKNOWN]	= DT_UNKNOWN,
	[EROFS_FT_REG_FILE]	= DT_REG,
	[EROFS_FT_DIR]		= DT_DIR,
	[EROFS_FT_CHRDEV]	= DT_CHR,
	[EROFS_FT_BLKDEV]	= DT_BLK,
	[EROFS_FT_FIFO]		= DT_FIFO,
	[EROFS_FT_SOCK]		= DT_SOCK,
	[EROFS_FT_SYMLINK]	= DT_LNK,
};

static void debug_one_dentry(unsigned char d_type, const char *de_name,
			     unsigned int de_namelen)
{
#ifdef CONFIG_EROFS_FS_DEBUG
	/* since the on-disk name could not have the trailing '\0' */
	unsigned char dbg_namebuf[EROFS_NAME_LEN + 1];

	memcpy(dbg_namebuf, de_name, de_namelen);
	dbg_namebuf[de_namelen] = '\0';

	debugln("found dirent %s de_len %u d_type %d", dbg_namebuf,
		de_namelen, d_type);
#endif
}

static int erofs_fill_dentries(struct dir_context *ctx,
	void *dentry_blk, unsigned *ofs,
	unsigned nameoff, unsigned maxsize)
{
	struct erofs_dirent *de = dentry_blk;
	const struct erofs_dirent *end = dentry_blk + nameoff;

	de = dentry_blk + *ofs;
	while (de < end) {
		const char *de_name;
		unsigned int de_namelen;
		unsigned char d_type;

		if (de->file_type < EROFS_FT_MAX)
			d_type = erofs_filetype_table[de->file_type];
		else
			d_type = DT_UNKNOWN;

		nameoff = le16_to_cpu(de->nameoff);
		de_name = (char *)dentry_blk + nameoff;

		/* the last dirent in the block? */
		if (de + 1 >= end)
			de_namelen = strnlen(de_name, maxsize - nameoff);
		else
			de_namelen = le16_to_cpu(de[1].nameoff) - nameoff;

		/* a corrupted entry is found */
		if (unlikely(nameoff + de_namelen > maxsize ||
			     de_namelen > EROFS_NAME_LEN)) {
			DBG_BUGON(1);
			return -EIO;
		}

		debug_one_dentry(d_type, de_name, de_namelen);
		if (!dir_emit(ctx, de_name, de_namelen,
					le64_to_cpu(de->nid), d_type))
			/* stoped by some reason */
			return 1;
		++de;
		*ofs += sizeof(struct erofs_dirent);
	}
	*ofs = maxsize;
	return 0;
}

static int erofs_readdir(struct file *f, struct dir_context *ctx)
{
	struct inode *dir = file_inode(f);
	struct address_space *mapping = dir->i_mapping;
	const size_t dirsize = i_size_read(dir);
	unsigned i = ctx->pos / EROFS_BLKSIZ;
	unsigned ofs = ctx->pos % EROFS_BLKSIZ;
	int err = 0;
	bool initial = true;

	while (ctx->pos < dirsize) {
		struct page *dentry_page;
		struct erofs_dirent *de;
		unsigned nameoff, maxsize;

		dentry_page = read_mapping_page(mapping, i, NULL);
		if (dentry_page == ERR_PTR(-ENOMEM)) {
			err = -ENOMEM;
			break;
		} else if (IS_ERR(dentry_page)) {
			errln("fail to readdir of logical block %u of nid %llu",
			      i, EROFS_V(dir)->nid);
			err = PTR_ERR(dentry_page);
			break;
		}

		lock_page(dentry_page);
		de = (struct erofs_dirent *)kmap(dentry_page);

		nameoff = le16_to_cpu(de->nameoff);

		if (unlikely(nameoff < sizeof(struct erofs_dirent) ||
			nameoff >= PAGE_SIZE)) {
			errln("%s, invalid de[0].nameoff %u",
				__func__, nameoff);

			err = -EIO;
			goto skip_this;
		}

		maxsize = min_t(unsigned, dirsize - ctx->pos + ofs, PAGE_SIZE);

		/* search dirents at the arbitrary position */
		if (unlikely(initial)) {
			initial = false;

			ofs = roundup(ofs, sizeof(struct erofs_dirent));
			if (unlikely(ofs >= nameoff))
				goto skip_this;
		}

		err = erofs_fill_dentries(ctx, de, &ofs, nameoff, maxsize);
skip_this:
		kunmap(dentry_page);

		unlock_page(dentry_page);
		put_page(dentry_page);

		ctx->pos = blknr_to_addr(i) + ofs;

		if (unlikely(err))
			break;
		++i;
		ofs = 0;
	}
	return err < 0 ? err : 0;
}

const struct file_operations erofs_dir_fops = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.iterate	= erofs_readdir,
};

