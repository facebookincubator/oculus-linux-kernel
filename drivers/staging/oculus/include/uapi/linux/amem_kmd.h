/* SPDX-License-Identifier: LGPL-2.0+ WITH Linux-syscall-note */
/*
 * Driver interface for Adaptive Memory Kernel mode driver.
 *
 * Copyright Meta Platforms, Inc. and its affiliates.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * !!!!!! WARNING !!!!!!
 *
 * A cleaned copy of this file is at:
 *   device/meta/meg2/kernel-headers/linux/amem_kmd.h
 *
 * Any changes made here need to be copied to that other copy using the
 * following script:
 *
 *  "
 *   bionic/libc/kernel/tools/clean_header.py
 *     -k kernel/drivers/staging/oculus/include/uapi/linux
 *     -d device/meta/meg2/kernel-headers/linux amem_kmd.h
 *  "
 */

#define AMEM_IOCTL_CREATE_AMEM_MANAGER	_IOW(0x84, 1, struct amem_create_manager_args)
#define AMEM_IOCTL_CREATE_AMEM		_IOW(0x84, 2, struct amem_create_args)

#define AMEM_IOCTL_LOAD			_IO(0x84, 1)
#define AMEM_IOCTL_EVICT		_IO(0x84, 2)

#define AMEM_IOCTL_GET_ID		_IOR(0x84, 1, uint16_t)
#define AMEM_IOCTL_GET_SIZE		_IOR(0x84, 2, size_t)

/* The memory is zero initialized */
#define ZERO_INIT		BIT(0)

/* The memory is not retained while unpinned */
#define NOT_RETAINED		BIT(1)

/* The memory is incompressible (encrypted or already compressed) */
#define INCOMPRESSIBLE		BIT(2)

/* The memory needs to be physically contiguous for DMA usage.
 * This also means the physical memory can't be moved while pinned.
 */
#define DMA_BUF			BIT(3)

#define AMEM_VALID_FLAGS (ZERO_INIT | NOT_RETAINED | INCOMPRESSIBLE | DMA_BUF)

#define AMEM_VIRT_ADDR_INIT ((void *)-1)

/*
 * !!!!!! WARNING !!!!!!
 *
 * Everything below this is considered a private contract between the
 * /dev/amem driver and the amem.c API implementation
 */

struct amem_create_manager_args {
	uint64_t client_tag;
};

struct amem_create_args {
	int amem_manager_fd;		/* file descriptor of the initialized amem_manager */
	uint16_t amem_id;		/* The index of amem_data within the manager */
	size_t size;			/* Size of memory to allocate */
	size_t max_latency_ns;		/* maximum latency of memory in nanoseconds */
	uint64_t flags;
};

// Currently, exactly one page of memory is allocated per manager to hold
// shared data with the client.
// The first object is a 16-byte struct amem_manager_data.
// The remaining 255 objects are 16-byte struct amem_data
// 16 bytes * 256 = 4096 Bytes -> 1 page
#define AMEM_SHARED_DATA_SIZE 16
#define AMEM_PER_MANAGER (PAGE_SIZE / AMEM_SHARED_DATA_SIZE)

// Structure must be exactly 16 bytes with no members larger than 8 bytes
struct amem_data {

	// This is a union because virt_addr actually has a lot of unused bits
	union {
		// The start of the virtual memory address used to access the memory.
		// User space virtual addresses are limited to the lower 48 bits unless
		// they opt-in to 52 bits with a mmap hint (which we don't to use).
		// Additionally, the lower PAGE_SHIFT (at least 12) bits are also unused.
		// After the shift, in total, the bottom 28 bits can be used for other
		// fields in this union.
		void *virt_addr;

		struct {
			// client_flags are written by the client and can be read by either
			// BIT(0): pinned
			// BIT(1): dirty (set by inc_write() call, cleared by pin() call)
			// BIT(2):
			// BIT(3):
			// BIT(4): pinAck (copies driver_flags1->pinAck on pin operation)
			// BIT(5): unpinAck (copies driver_flags1->unpinAck on unpin operation)
			// BIT(6): writeAck (copies driver_flags1->writeAck on inc_write operation)
			// BIT(7): readAck (copies driver_flags1->readAck on inc_read operation)
			atomic_uint_fast8_t client_flags;

			// BIT(0):
			// BIT(1):
			// BIT(2):
			// BIT(3):
			// BIT(4): <Used by virt_addr on 4KB PAGE_SIZE systems>
			// BIT(5): <Used by virt_addr on 4KB PAGE_SIZE systems>
			// BIT(6): <Used by virt_addr on 4KB and 16KB PAGE_SIZE systems>
			// BIT(7): <Used by virt_addr on 4KB and 16KB PAGE_SIZE systems>
			uint8_t client_flags2;

			uint16_t reserved_for_virt_addr1;
			uint16_t reserved_for_virt_addr2;

			// The file descriptor. zero if this structure is unallocated.
			// It occupies the top 16 bits of the union.
			uint16_t fd;
		};
	};

	// The millisecond timestamp at which the previous pin operation occured
	// There's enough bits here to last just over a minute without overflow.
	// amem buffers should not, in general, be kept pinned longer than 30 seconds.
	// Every 30 seconds, (when not in standby) the driver will scan amem
	// buffers and flag any that have been pinned for longer than 30 seconds.
	// Long-pinned buffers may be evicted to RAM.
	uint16_t last_timestamp;

	// Expected next access time in seconds expressed as an exponent:
	// (2 ^ access_exponent) seconds.
	// Example : expected next access
	//    0  : 1 second from now
	//   -1  : 0.5 seconds from now
	//   -10 : ~1ms from now
	//    10 : 1024 seconds = ~17 min from now
	//
	// This is used as a (min) priority value where the lowest values are highest
	// priority to be placed in a close memory.
	//
	// When pin() is called, higher writeCountExpected and readCountExpected
	// drive down access_exponent and higher milliseconds drive up access_exponent.
	// When unpin() is called, higher milliseconds drive up access_exponent.
	char access_exponent;

	// This is the bias applied to access_exponent due to mispredictions.
	// Same format as access_exponent.
	char access_exponent_bias;

	// Client increments whenever over 50% of the bytes of the buffer are
	// read or written.
	// Must use saturating addition to stop at 255.
	// This is reset to zero on pin() calls
	uint8_t access_count;

	// driver_flags are written by the driver and can be read by the client.
	// The client has access to write to these, even though it shouldn't, so
	// the driver can NOT trust this data.
	// BIT(0): request AMEM_IOCTL_INTERCEPT_PIN
	// BIT(1): request AMEM_IOCTL_INTERCEPT_UNPIN
	// BIT(2):
	// BIT(3): LONG_PIN
	// BIT(4): pinAck (copied to driver_flags1->pinAck on pin operation)
	// BIT(5): unpinAck (copied to driver_flags1->unpinAck on unpin operation)
	// BIT(6): writeAck (copied to driver_flags1->writeAck on inc_write operation)
	// BIT(7): readAck (copied to driver_flags1->readAck on inc_read operation)
	uint8_t driver_flags;

	// ???  Should this hold the status of what is backing the memory?
	uint16_t driver_flags2;
};

// It can also be retrieved by finding the start of the page
// Must not exceed 16 bytes long
struct amem_manager_data {

	uint16_t reserved0;

	// The file descriptor.
	uint16_t fd;

	uint8_t amem_count;
	uint8_t reserved2;
	uint8_t reserved3;

	// Same format as amem_data::access_exponent.
	// This is the current expected cutoff value above which the client should
	// not expect to be loaded in TCM.
	char access_exponent_tcm_cutoff;

	uint8_t driver_reserved1;
	uint8_t driver_reserved2;
	uint8_t driver_reserved3;

	uint32_t driver_reserved4;
};
