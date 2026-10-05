/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*****************************************************************************
 * @file pci_tunnel.h
 *
 * @brief Interface for syscalls for PCIe tunneling.
 *
 * @details This driver enables generic PCIe tunneling functionality.
 *          The driver is expected to be used for some narrow special cases
 *          and not to be installed for long. Use ARFW for PCIe communications.
 *          IMPORTANT: this driver only works via arfw_shim!
 *****************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 ****************************************************************************/

#ifndef PCI_TUNNEL_H
#define PCI_TUNNEL_H

#ifndef __KERNEL__
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif // !__packed
#else // __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#endif // !__KERNEL__

// The user is expected to call mmap right after the create request to target
// the same buffer for memory mapping. We only allow one fd for all tunnel ops.
// All buffers are cleaned up on fd release. No need to delete buffers manually.
// Buffers will be allocated from the coherent memory pool.
struct __packed pci_tunnel_ioctl_create_buffer_req {
	size_t in_size;
	uint32_t out_id;
	uint64_t out_addr;
};

#define PCI_TUNNEL_IOCTL_MAGIC 0xCC
#define PCI_TUNNEL_IOCTL_CREATE_BUFFER   \
	_IOWR(PCI_TUNNEL_IOCTL_MAGIC, 0, \
	      struct pci_tunnel_ioctl_create_buffer_req *)

#endif // !PCI_TUNNEL_H
