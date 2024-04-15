/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __FWUPDATE_MANAGER_H
#define __FWUPDATE_MANAGER_H

struct fwupdate_provisioning {
	u32 format_version;
	u32 flash_addr;
	u32 data_length;
	u8 data[0];
} __packed;

struct fwupdate_header {
	u8 force_bootloader_update;
	struct fwupdate_provisioning provisioning[0];
} __packed;

#endif /* __FWUPDATE_MANAGER_H */
