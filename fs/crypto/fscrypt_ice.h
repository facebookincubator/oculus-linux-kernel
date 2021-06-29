/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _FSCRYPT_ICE_H
#define _FSCRYPT_ICE_H

#include <linux/blkdev.h>
#include "fscrypt_private.h"

#if defined(CONFIG_FS_ENCRYPTION) && defined(CONFIG_PFK)
static inline bool fscrypt_should_be_processed_by_ice(const struct inode *inode)
{
	if (!inode->i_sb->s_cop)
		return false;
	if (!inode->i_sb->s_cop->is_encrypted((struct inode *)inode))
		return false;

	return fscrypt_using_hardware_encryption(inode);
}

static inline int fscrypt_is_ice_capable(const struct super_block *sb)
{
	return blk_queue_inlinecrypt(bdev_get_queue(sb->s_bdev));
}

int fscrypt_is_aes_xts_cipher(const struct inode *inode);

char *fscrypt_get_ice_encryption_key(const struct inode *inode);
char *fscrypt_get_ice_encryption_salt(const struct inode *inode);

bool fscrypt_is_ice_encryption_info_equal(const struct inode *inode1,
					const struct inode *inode2);

static inline size_t fscrypt_get_ice_encryption_key_size(
					const struct inode *inode)
{
	return FS_AES_256_XTS_KEY_SIZE / 2;
}

static inline size_t fscrypt_get_ice_encryption_salt_size(
					const struct inode *inode)
{
	return FS_AES_256_XTS_KEY_SIZE / 2;
}

/*
 * Encrypted data can only be merged if the key is the same and the DUNs are
 * consecutive.
 *
 * This kernel's inline crypto implementation only keeps track of the bio's key
 * indirectly via the inode, not in the bio itself.  So, it's non-obvious to
 * check that the keys are the same.  However, this kernel always puts the inode
 * number in the high 32 bits of the DUN.
 *
 * So, to ensure that each bio uses only one key, we ensure that all its DUNs
 * have the high same 32 bits, and thus the I/O is for only one inode.  Note
 * that with the IV_INO_LBLK_64 strategy ('dun = (ino << 32) | lblk_num'), this
 * is implied by the DUN contiguity check since f2fs never uses lblk_num
 * 0xffffffff.  But with IV_INO_LBLK_32 ('dun = (ino << 32) | (hashed_ino +
 * lblk_num)'), we can have e.g. the bio's last DUN is 0x1ffffffff and next_dun
 * is 0x200000000.  We cannot merge in that case, since inodes 1 and 2 could
 * have different keys, and also because wraparound in the low 32 DUN bits may
 * not be handled in the expected way.
 */
static inline bool fscrypt_enc_bio_mergeable(const struct bio *bio,
					     unsigned int sectors, u64 next_dun)
{
	if (upper_32_bits(bio_dun(bio)) != upper_32_bits(next_dun))
		return false;
	return bio_end_dun(bio) == next_dun;
}

bool fscrypt_force_iv_ino_lblk_32(void);

#else
static inline bool fscrypt_should_be_processed_by_ice(const struct inode *inode)
{
	return false;
}

static inline int fscrypt_is_ice_capable(const struct super_block *sb)
{
	return false;
}

static inline char *fscrypt_get_ice_encryption_key(const struct inode *inode)
{
	return NULL;
}

static inline char *fscrypt_get_ice_encryption_salt(const struct inode *inode)
{
	return NULL;
}

static inline size_t fscrypt_get_ice_encryption_key_size(
					const struct inode *inode)
{
	return 0;
}

static inline size_t fscrypt_get_ice_encryption_salt_size(
					const struct inode *inode)
{
	return 0;
}

static inline int fscrypt_is_xts_cipher(const struct inode *inode)
{
	return 0;
}

static inline bool fscrypt_is_ice_encryption_info_equal(
					const struct inode *inode1,
					const struct inode *inode2)
{
	return false;
}

static inline int fscrypt_is_aes_xts_cipher(const struct inode *inode)
{
	return 0;
}

static inline bool fscrypt_force_iv_ino_lblk_32(void)
{
	return false;
}

#endif

#endif	/* _FSCRYPT_ICE_H */
