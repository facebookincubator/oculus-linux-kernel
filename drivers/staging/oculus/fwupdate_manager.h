/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __FWUPDATE_MANAGER_H
#define __FWUPDATE_MANAGER_H

struct provisioning {
	uint32_t format_version;
	uint32_t flash_addr;
	uint32_t data_length;
	uint8_t data[0];
} __packed;

#endif /* __FWUPDATE_MANAGER_H */
