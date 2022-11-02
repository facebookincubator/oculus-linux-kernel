// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "fscrypt_ice.h"
#include "../../security/pfe/pfk_kc.h"

int fscrypt_using_hardware_encryption(const struct inode *inode)
{
	struct fscrypt_info *ci = inode->i_crypt_info;

	return S_ISREG(inode->i_mode) && ci &&
		ci->ci_data_mode == FS_ENCRYPTION_MODE_PRIVATE;
}
EXPORT_SYMBOL(fscrypt_using_hardware_encryption);

/*
 * Retrieves encryption key from the inode
 */
char *fscrypt_get_ice_encryption_key(const struct inode *inode)
{
	struct fscrypt_info *ci = NULL;

	if (!inode)
		return NULL;

	ci = inode->i_crypt_info;
	if (!ci)
		return NULL;

	return &(ci->ci_raw_key[0]);
}

/*
 * Retrieves encryption salt from the inode
 */
char *fscrypt_get_ice_encryption_salt(const struct inode *inode)
{
	struct fscrypt_info *ci = NULL;

	if (!inode)
		return NULL;

	ci = inode->i_crypt_info;
	if (!ci)
		return NULL;

	return &(ci->ci_raw_key[fscrypt_get_ice_encryption_key_size(inode)]);
}

/*
 * returns true if the cipher mode in inode is AES XTS
 */
int fscrypt_is_aes_xts_cipher(const struct inode *inode)
{
	struct fscrypt_info *ci = inode->i_crypt_info;

	if (!ci)
		return 0;

	return (ci->ci_data_mode == FS_ENCRYPTION_MODE_PRIVATE);
}

/*
 * returns true if encryption info in both inodes is equal
 */
bool fscrypt_is_ice_encryption_info_equal(const struct inode *inode1,
					const struct inode *inode2)
{
	char *key1 = NULL;
	char *key2 = NULL;
	char *salt1 = NULL;
	char *salt2 = NULL;

	if (!inode1 || !inode2)
		return false;

	if (inode1 == inode2)
		return true;

	/*
	 * both do not belong to ice, so we don't care, they are equal
	 * for us
	 */
	if (!fscrypt_should_be_processed_by_ice(inode1) &&
			!fscrypt_should_be_processed_by_ice(inode2))
		return true;

	/* one belongs to ice, the other does not -> not equal */
	if (fscrypt_should_be_processed_by_ice(inode1) ^
			fscrypt_should_be_processed_by_ice(inode2))
		return false;

	key1 = fscrypt_get_ice_encryption_key(inode1);
	key2 = fscrypt_get_ice_encryption_key(inode2);
	salt1 = fscrypt_get_ice_encryption_salt(inode1);
	salt2 = fscrypt_get_ice_encryption_salt(inode2);

	/* key and salt should not be null by this point */
	if (!key1 || !key2 || !salt1 || !salt2 ||
		(fscrypt_get_ice_encryption_key_size(inode1) !=
		 fscrypt_get_ice_encryption_key_size(inode2)) ||
		(fscrypt_get_ice_encryption_salt_size(inode1) !=
		 fscrypt_get_ice_encryption_salt_size(inode2)))
		return false;

	if ((memcmp(key1, key2,
			fscrypt_get_ice_encryption_key_size(inode1)) == 0) &&
		(memcmp(salt1, salt2,
			fscrypt_get_ice_encryption_salt_size(inode1)) == 0))
		return true;

	return false;
}

bool fscrypt_force_iv_ino_lblk_32(void)
{
	return strcmp("sdcc", pfk_kc_get_storage_type()) == 0;
}

static u64 fscrypt_generate_dun(const struct inode *inode, u64 lblk_num)
{
	BUG_ON(inode->i_ino > U32_MAX);
	BUG_ON(inode->i_ino == 0);
	BUG_ON(lblk_num > U32_MAX);

	/*
	 * Standard DUN generation (like upstream IV_INO_LBLK_64): high 32 bits
	 * are inode number, low 32 bits are file logical block number.
	 *
	 * IV_INO_LBLK_32 DUN generation: low 32 bits are hashed inode number +
	 * file logical block number.  We still leave the inode number in the
	 * high 32 bits since it doesn't hurt anything, and it maintains
	 * compatibility with the quirks of this kernel's inline crypto
	 * implementation such as DUN 0 being treated specially.
	 */
	if (inode->i_crypt_info->ci_flags & FS_POLICY_FLAG_IV_INO_LBLK_32)
		lblk_num = (u32)(inode->i_crypt_info->ci_hashed_ino + lblk_num);

	return ((u64)inode->i_ino << 32) | lblk_num;
}

/**
 * fscrypt_set_bio_crypt_ctx - prepare a file contents bio for inline encryption
 * @bio: a bio which will eventually be submitted to the file
 * @inode: the file's inode
 * @first_lblk: the first file logical block number in the I/O
 * @gfp_mask: memory allocation flags - these must be a waiting mask so that
 *					bio_crypt_set_ctx can't fail.
 *
 * If the contents of the file should be encrypted (or decrypted) with inline
 * encryption, then assign the appropriate encryption context to the bio.
 *
 * Normally the bio should be newly allocated (i.e. no pages added yet), as
 * otherwise fscrypt_mergeable_bio() won't work as intended.
 *
 * The encryption context will be freed automatically when the bio is freed.
 *
 * This function also handles setting bi_crypt_skip when needed.
 */
void fscrypt_set_bio_crypt_ctx(struct bio *bio, const struct inode *inode,
			       u64 first_lblk, gfp_t gfp_mask)
{
	if (fscrypt_inode_should_skip_dm_default_key(inode))
		bio_set_skip_dm_default_key(bio);
	if (fscrypt_using_hardware_encryption(inode))
		bio->bi_iter.bi_dun = fscrypt_generate_dun(inode, first_lblk);
}
EXPORT_SYMBOL_GPL(fscrypt_set_bio_crypt_ctx);

/**
 * fscrypt_mergeable_bio - test whether data can be added to a bio
 * @bio: the bio being built up
 * @inode: the inode for the next part of the I/O
 * @next_lblk: the next file logical block number in the I/O
 *
 * When building a bio which may contain data which should undergo inline
 * encryption (or decryption) via fscrypt, filesystems should call this function
 * to ensure that the resulting bio contains only logically contiguous data.
 * This will return false if the next part of the I/O cannot be merged with the
 * bio because either the encryption key would be different or the encryption
 * data unit numbers would be discontiguous.
 *
 * fscrypt_set_bio_crypt_ctx() must have already been called on the bio.
 *
 * This function also returns false if the next part of the I/O would need to
 * have a different value for the bi_crypt_skip flag.
 *
 * Return: true iff the I/O is mergeable
 */
bool fscrypt_mergeable_bio(struct bio *bio, const struct inode *inode,
			   u64 next_lblk)
{
	const bool enc1 = (bio_dun(bio) != 0);
	const bool enc2 = fscrypt_using_hardware_encryption(inode);
	u64 next_dun;

	if (enc1 != enc2)
		return false;
	if (bio_should_skip_dm_default_key(bio) !=
	    fscrypt_inode_should_skip_dm_default_key(inode))
		return false;
	if (!enc1)
		return true;

	next_dun = fscrypt_generate_dun(inode, next_lblk);
	return fscrypt_enc_bio_mergeable(bio, bio_sectors(bio), next_dun);
}
EXPORT_SYMBOL_GPL(fscrypt_mergeable_bio);

