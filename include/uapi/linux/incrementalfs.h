/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Userspace interface for Incremental FS.
 *
 * Incremental FS is special-purpose Linux virtual file system that allows
 * execution of a program while its binary and resource files are still being
 * lazily downloaded over the network, USB etc.
 *
 * Copyright 2019 Google LLC
 */
#ifndef _UAPI_LINUX_INCREMENTALFS_H
#define _UAPI_LINUX_INCREMENTALFS_H

#include <linux/limits.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/xattr.h>

/* ===== constants ===== */
#define INCFS_NAME "incremental-fs"
#define INCFS_MAGIC_NUMBER (0x5346434e49ul)
#define INCFS_DATA_FILE_BLOCK_SIZE 4096
#define INCFS_HEADER_VER 1

/* TODO: This value is assumed in incfs_copy_signature_info_from_user to be the
 * actual signature length. Set back to 64 when fixed.
 */
#define INCFS_MAX_HASH_SIZE 32
#define INCFS_MAX_FILE_ATTR_SIZE 512

#define INCFS_PENDING_READS_FILENAME ".pending_reads"
#define INCFS_LOG_FILENAME ".log"
#define INCFS_XATTR_ID_NAME (XATTR_USER_PREFIX "incfs.id")
#define INCFS_XATTR_SIZE_NAME (XATTR_USER_PREFIX "incfs.size")
#define INCFS_XATTR_METADATA_NAME (XATTR_USER_PREFIX "incfs.metadata")

#define INCFS_MAX_SIGNATURE_SIZE 8096
#define INCFS_SIGNATURE_VERSION 2
#define INCFS_SIGNATURE_SECTIONS 2

#define INCFS_IOCTL_BASE_CODE 'g'

/* ===== ioctl requests on the command dir ===== */

/* Create a new file */
#define INCFS_IOC_CREATE_FILE \
	_IOWR(INCFS_IOCTL_BASE_CODE, 30, struct incfs_new_file_args)

/* Read file signature */
#define INCFS_IOC_READ_FILE_SIGNATURE                                          \
	_IOR(INCFS_IOCTL_BASE_CODE, 31, struct incfs_get_file_sig_args)

/*
 * Fill in one or more data block. This may only be called on a handle
 * passed as a parameter to INCFS_IOC_PERMIT_FILLING
 *
 * Returns number of blocks filled in, or error if none were
 */
#define INCFS_IOC_FILL_BLOCKS                                                  \
	_IOR(INCFS_IOCTL_BASE_CODE, 32, struct incfs_fill_blocks)

/*
 * Permit INCFS_IOC_FILL_BLOCKS on the given file descriptor
 * May only be called on .pending_reads file
 *
 * Returns 0 on success or error
 */
#define INCFS_IOC_PERMIT_FILL                                                  \
	_IOW(INCFS_IOCTL_BASE_CODE, 33, struct incfs_permit_fill)

/*
 * Fills buffer with ranges of populated blocks
 *
 * Returns 0 if all ranges written
 *	   error otherwise
 *
 *	   Either way, range_buffer_size_out is set to the number
 *	   of bytes written. Should be set to 0 by caller. The ranges
 *	   filled are valid, but if an error was returned there might
 *	   be more ranges to come.
 *
 *	   Ranges are ranges of filled blocks:
 *
 *	   1 2 7 9
 *
 *	   means blocks 1, 2, 7, 8, 9 are filled, 0, 3, 4, 5, 6 and 10 on
 *	   are not
 *
 *	   If hashing is enabled for the file, the hash blocks are simply
 *	   treated as though they immediately followed the data blocks.
 */
#define INCFS_IOC_GET_FILLED_BLOCKS                                            \
	_IOR(INCFS_IOCTL_BASE_CODE, 34, struct incfs_get_filled_blocks_args)

enum incfs_compression_alg {
	COMPRESSION_NONE = 0,
	COMPRESSION_LZ4 = 1
};

enum incfs_block_flags {
	INCFS_BLOCK_FLAGS_NONE = 0,
	INCFS_BLOCK_FLAGS_HASH = 1,
};

typedef struct {
	__u8 bytes[16];
} incfs_uuid_t __attribute__((aligned (8)));

/*
 * Description of a pending read. A pending read - a read call by
 * a userspace program for which the filesystem currently doesn't have data.
 */
struct incfs_pending_read_info {
	/* Id of a file that is being read from. */
	incfs_uuid_t file_id;

	/* A number of microseconds since system boot to the read. */
	__aligned_u64 timestamp_us;

	/* Index of a file block that is being read. */
	__u32 block_index;

	/* A serial number of this pending read. */
	__u32 serial_number;
};

/*
 * Description of a data or hash block to add to a data file.
 */
struct incfs_fill_block {
	/* Index of a data block. */
	__u32 block_index;

	/* Length of data */
	__u32 data_len;

	/*
	 * A pointer to an actual data for the block.
	 *
	 * Equivalent to: __u8 *data;
	 */
	__aligned_u64 data;

	/*
	 * Compression algorithm used to compress the data block.
	 * Values from enum incfs_compression_alg.
	 */
	__u8 compression;

	/* Values from enum incfs_block_flags */
	__u8 flags;

	__u16 reserved1;

	__u32 reserved2;

	__aligned_u64 reserved3;
};

/*
 * Description of a number of blocks to add to a data file
 *
 * Argument for INCFS_IOC_FILL_BLOCKS
 */
struct incfs_fill_blocks {
	/* Number of blocks */
	__u64 count;

	/* A pointer to an array of incfs_fill_block structs */
	__aligned_u64 fill_blocks;
};

/*
 * Permit INCFS_IOC_FILL_BLOCKS on the given file descriptor
 * May only be called on .pending_reads file
 *
 * Argument for INCFS_IOC_PERMIT_FILL
 */
struct incfs_permit_fill {
	/* File to permit fills on */
	__u32 file_descriptor;
};

enum incfs_hash_tree_algorithm {
	INCFS_HASH_TREE_NONE = 0,
	INCFS_HASH_TREE_SHA256 = 1
};

/*
 * Create a new file or directory.
 */
struct incfs_new_file_args {
	/* Id of a file to create. */
	incfs_uuid_t file_id;

	/*
	 * Total size of the new file. Ignored if S_ISDIR(mode).
	 */
	__aligned_u64 size;

	/*
	 * File mode. Permissions and dir flag.
	 */
	__u16 mode;

	__u16 reserved1;

	__u32 reserved2;

	/*
	 * A pointer to a null-terminated relative path to the file's parent
	 * dir.
	 * Max length: PATH_MAX
	 *
	 * Equivalent to: char *directory_path;
	 */
	__aligned_u64 directory_path;

	/*
	 * A pointer to a null-terminated file's name.
	 * Max length: PATH_MAX
	 *
	 * Equivalent to: char *file_name;
	 */
	__aligned_u64 file_name;

	/*
	 * A pointer to a file attribute to be set on creation.
	 *
	 * Equivalent to: u8 *file_attr;
	 */
	__aligned_u64 file_attr;

	/*
	 * Length of the data buffer specfied by file_attr.
	 * Max value: INCFS_MAX_FILE_ATTR_SIZE
	 */
	__u32 file_attr_len;

	__u32 reserved4;

	/*
	 * Points to an APK V4 Signature data blob
	 * Signature must have two sections
	 * Format is:
	 *	u32 version
	 *	u32 size_of_hash_info_section
	 *	u8 hash_info_section[]
	 *	u32 size_of_signing_info_section
	 *	u8 signing_info_section[]
	 *
	 * Note that incfs does not care about what is in signing_info_section
	 *
	 * hash_info_section has following format:
	 *	u32 hash_algorithm; // Must be SHA256 == 1
	 *	u8 log2_blocksize;  // Must be 12 for 4096 byte blocks
	 *	u32 salt_size;
	 *	u8 salt[];
	 *	u32 hash_size;
	 *	u8 root_hash[];
	 */
	__aligned_u64 signature_info;

	/* Size of signature_info */
	__aligned_u64 signature_size;

	__aligned_u64 reserved6;
};

/*
 * Request a digital signature blob for a given file.
 * Argument for INCFS_IOC_READ_FILE_SIGNATURE ioctl
 */
struct incfs_get_file_sig_args {
	/*
	 * A pointer to the data buffer to save an signature blob to.
	 *
	 * Equivalent to: u8 *file_signature;
	 */
	__aligned_u64 file_signature;

	/* Size of the buffer at file_signature. */
	__u32 file_signature_buf_size;

	/*
	 * Number of bytes save file_signature buffer.
	 * It is set after ioctl done.
	 */
	__u32 file_signature_len_out;
};

struct incfs_filled_range {
	__u32 begin;
	__u32 end;
};

/*
 * Request ranges of filled blocks
 * Argument for INCFS_IOC_GET_FILLED_BLOCKS
 */
struct incfs_get_filled_blocks_args {
	/*
	 * A buffer to populate with ranges of filled blocks
	 *
	 * Equivalent to struct incfs_filled_ranges *range_buffer
	 */
	__aligned_u64 range_buffer;

	/* Size of range_buffer */
	__u32 range_buffer_size;

	/* Start index to read from */
	__u32 start_index;

	/*
	 * End index to read to. 0 means read to end. This is a range,
	 * so incfs will read from start_index to end_index - 1
	 */
	__u32 end_index;

	/* Actual number of blocks in file */
	__u32 total_blocks_out;

	/* The  number of data blocks in file */
	__u32 data_blocks_out;

	/* Number of bytes written to range buffer */
	__u32 range_buffer_size_out;

	/* Sector scanned up to, if the call was interrupted */
	__u32 index_out;
};

#endif /* _UAPI_LINUX_INCREMENTALFS_H */
