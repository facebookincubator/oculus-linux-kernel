/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Userspace interface to the Meta SWD driver's /dev/swd_<flavor>.
 *
 * Beyond the firmware update path (read/write/SWD_IOCTL_CHIP_ERASE), this
 * exposes the raw wire so an external debugger can borrow the link instead of
 * fighting the driver for the GPIOs. OpenOCD's meta_swd adapter is the
 * intended consumer.
 */
#ifndef _UAPI_LINUX_SWD_CHARDEV_H
#define _UAPI_LINUX_SWD_CHARDEV_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SWD_MAGIC_NUM 100

/* Maximum transfers in one SWD_IOCTL_DAP_XFER batch. */
#define SWD_DAP_XFER_MAX 64

/* Maximum bits in one SWD_IOCTL_RAW_SEQ sequence. */
#define SWD_RAW_SEQ_MAX_BITS 256

/*
 * Maximum idle cycles accepted per transfer. Matches the range of OpenOCD's
 * 'dap memaccess', which is where the value comes from, and bounds a loop that
 * bit-bangs in kernel context.
 */
#define SWD_IDLE_CLOCKS_MAX 255

/* ADIv5 acknowledge codes, as sampled off the wire. */
#define SWD_ACK_OK    0x1
#define SWD_ACK_WAIT  0x2
#define SWD_ACK_FAULT 0x4

/**
 * struct swd_dap_xfer - one DP or AP register transfer
 * @apndp: 0 addresses a DP register, 1 an AP register
 * @rnw: 0 writes @data, 1 reads into @data
 * @reg: register offset within the bank; one of 0x0, 0x4, 0x8, 0xc
 * @ack: out: the acknowledge sampled off the wire, reported verbatim
 * @data: value to write, or the value read back
 * @idle_clocks: idle cycles to clock out after the transfer
 */
struct swd_dap_xfer {
	__u8 apndp;
	__u8 rnw;
	__u8 reg;
	__u8 ack;
	__u32 data;
	__u32 idle_clocks;
};

/**
 * struct swd_dap_batch - a run of transfers executed back to back
 * @count: entries in @xfers, at most SWD_DAP_XFER_MAX
 * @completed: out: how many entries were executed, including one that NAKed
 * @xfers: userspace pointer to @count struct swd_dap_xfer
 *
 * The batch stops at the first non-OK acknowledge. Nothing is retried: the
 * debugger runs its own WAIT/FAULT recovery, and a second retry layer in the
 * kernel would make failures impossible to attribute.
 */
struct swd_dap_batch {
	__u32 count;
	__u32 completed;
	__u64 xfers;
};

/**
 * struct swd_raw_seq - an arbitrary bit sequence to clock out on SWDIO
 * @nbits: bits to clock, at most SWD_RAW_SEQ_MAX_BITS
 * @bits: userspace pointer to the bit array; bit n is
 *        (bits[n / 8] >> (n % 8)) & 1
 *
 * Used for the ADIv5 special sequences - line reset, JTAG-to-SWD and friends -
 * which belong to the debugger rather than to this driver.
 */
struct swd_raw_seq {
	__u32 nbits;
	__u64 bits;
};

#define SWD_IOCTL_CHIP_ERASE    _IO(SWD_MAGIC_NUM, 0)

/*
 * Hold the MCU reset line for this many milliseconds. Note the argument is
 * read by value despite the _IOW(..., __u32) encoding conventionally meaning
 * "pointer to __u32": passing &hold_ms hands the driver a pointer as a
 * millisecond count.
 */
#define SWD_IOCTL_RESET         _IOW(SWD_MAGIC_NUM, 1, __u32)

#define SWD_IOCTL_DAP_XFER      _IOWR(SWD_MAGIC_NUM, 20, struct swd_dap_batch)
#define SWD_IOCTL_RAW_SEQ       _IOW(SWD_MAGIC_NUM, 21, struct swd_raw_seq)
#define SWD_IOCTL_DEBUG_CLAIM   _IO(SWD_MAGIC_NUM, 22)
#define SWD_IOCTL_DEBUG_RELEASE _IO(SWD_MAGIC_NUM, 23)

#endif /* _UAPI_LINUX_SWD_CHARDEV_H */
