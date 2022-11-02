/*
 * Broadcom device-specific manifest constants used by DHD, but deprecated in firmware.
 *
 * Copyright (C) 2021, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Dual:>>
 */

#ifndef	_bcmdevs_legacy_h_
#define	_bcmdevs_legacy_h_

/* DONGLE VID/PIDs */

/* PCI Device IDs */
#define BCM4335_D11AC_ID	0x43ae
#define BCM4335_D11AC2G_ID	0x43af
#define BCM4335_D11AC5G_ID	0x43b0
#define BCM4345_D11AC_ID	0x43ab		/* 4345 802.11ac dualband device */
#define BCM4345_D11AC2G_ID	0x43ac		/* 4345 802.11ac 2.4G device */
#define BCM4345_D11AC5G_ID	0x43ad		/* 4345 802.11ac 5G device */
#define BCM43452_D11AC_ID	0x47ab		/* 43452 802.11ac dualband device */
#define BCM43452_D11AC2G_ID	0x47ac		/* 43452 802.11ac 2.4G device */
#define BCM43452_D11AC5G_ID	0x47ad		/* 43452 802.11ac 5G device */
#define BCM4347_D11AC_ID	0x440a		/* 4347 802.11ac dualband device */
#define BCM4347_D11AC2G_ID	0x440b		/* 4347 802.11ac 2.4G device */
#define BCM4347_D11AC5G_ID	0x440c		/* 4347 802.11ac 5G device */
#define BCM4349_D11AC_ID	0x4349		/* 4349 802.11ac dualband device */
#define BCM4349_D11AC2G_ID	0x43dd		/* 4349 802.11ac 2.4G device */
#define BCM4349_D11AC5G_ID	0x43de		/* 4349 802.11ac 5G device */

#define BCM4350_D11AC_ID	0x43a3
#define BCM4350_D11AC2G_ID	0x43a4
#define BCM4350_D11AC5G_ID	0x43a5
#define BCM4354_D11AC_ID	0x43df		/* 4354 802.11ac dualband device */
#define BCM4354_D11AC2G_ID	0x43e0		/* 4354 802.11ac 2.4G device */
#define BCM4354_D11AC5G_ID	0x43e1		/* 4354 802.11ac 5G device */
#define BCM4355_D11AC_ID	0x43dc		/* 4355 802.11ac dualband device */
#define BCM4355_D11AC2G_ID	0x43fc		/* 4355 802.11ac 2.4G device */
#define BCM4355_D11AC5G_ID	0x43fd		/* 4355 802.11ac 5G device */
#define BCM4356_D11AC_ID	0x43ec		/* 4356 802.11ac dualband device */
#define BCM4356_D11AC2G_ID	0x43ed		/* 4356 802.11ac 2.4G device */
#define BCM4356_D11AC5G_ID	0x43ee		/* 4356 802.11ac 5G device */
#define BCM43569_D11AC_ID	0x43d9
#define BCM43569_D11AC2G_ID	0x43da
#define BCM43569_D11AC5G_ID	0x43db
#define BCM4358_D11AC_ID        0x43e9          /* 4358 802.11ac dualband device */
#define BCM4358_D11AC2G_ID      0x43ea          /* 4358 802.11ac 2.4G device */
#define BCM4358_D11AC5G_ID      0x43eb          /* 4358 802.11ac 5G device */

#define BCM4359_D11AC_ID	0x43ef		/* 4359 802.11ac dualband device */
#define BCM4359_D11AC2G_ID	0x43fe		/* 4359 802.11ac 2.4G device */
#define BCM4359_D11AC5G_ID	0x43ff		/* 4359 802.11ac 5G device */
#define BCM43596_D11AC_ID	0x4415		/* 43596 802.11ac dualband device */
#define BCM43596_D11AC2G_ID	0x4416		/* 43596 802.11ac 2.4G device */
#define BCM43596_D11AC5G_ID	0x4417		/* 43596 802.11ac 5G device */
#define BCM43597_D11AC_ID	0x441c		/* 43597 802.11ac dualband device */
#define BCM43597_D11AC2G_ID	0x441d		/* 43597 802.11ac 2.4G device */
#define BCM43597_D11AC5G_ID	0x441e		/* 43597 802.11ac 5G device */
#define BCM4361_D11AC_ID	0x441f		/* 4361 802.11ac dualband device */
#define BCM4361_D11AC2G_ID	0x4420		/* 4361 802.11ac 2.4G device */
#define BCM4361_D11AC5G_ID	0x4421		/* 4361 802.11ac 5G device */
#define BCM4364_D11AC_ID	0x4464		/* 4364 802.11ac dualband device */
#define BCM4364_D11AC2G_ID	0x446a		/* 4364 802.11ac 2.4G device */
#define BCM4364_D11AC5G_ID	0x446b		/* 4364 802.11ac 5G device */
#define BCM4371_D11AC_ID	0x440d		/* 4371 802.11ac dualband device */
#define BCM4371_D11AC2G_ID	0x440e		/* 4371 802.11ac 2.4G device */
#define BCM4371_D11AC5G_ID	0x440f		/* 4371 802.11ac 5G device */

/* Chip IDs */
#define BCM43018_CHIP_ID	43018		/* 43018 chipcommon chipid */
#define BCM4335_CHIP_ID		0x4335		/* 4335 chipcommon chipid */
#define BCM4339_CHIP_ID		0x4339		/* 4339 chipcommon chipid */
#define BCM43430_CHIP_ID	43430		/* 43430 chipcommon chipid */
#define BCM4345_CHIP_ID		0x4345		/* 4345 chipcommon chipid */
#define BCM43452_CHIP_ID	43452		/* 43454 chipcommon chipid */
#define BCM43454_CHIP_ID	43454		/* 43454 chipcommon chipid */
#define BCM43455_CHIP_ID	43455		/* 43455 chipcommon chipid */
#define BCM43457_CHIP_ID	43457		/* 43457 chipcommon chipid */
#define BCM43458_CHIP_ID	43458		/* 43458 chipcommon chipid */

#define BCM4345_CHIP(chipid)	(CHIPID(chipid) == BCM4345_CHIP_ID || \
				 CHIPID(chipid) == BCM43452_CHIP_ID || \
				 CHIPID(chipid) == BCM43454_CHIP_ID || \
				 CHIPID(chipid) == BCM43455_CHIP_ID || \
				 CHIPID(chipid) == BCM43457_CHIP_ID || \
				 CHIPID(chipid) == BCM43458_CHIP_ID)

#define CASE_BCM4345_CHIP	case BCM4345_CHIP_ID: /* fallthrough */ \
				case BCM43454_CHIP_ID: /* fallthrough */ \
				case BCM43455_CHIP_ID: /* fallthrough */ \
				case BCM43457_CHIP_ID: /* fallthrough */ \
				case BCM43458_CHIP_ID

#define BCM4347_CHIP_ID		0x4347          /* 4347 chipcommon chipid */
#define BCM4347_CHIP(chipid)   ((CHIPID(chipid) == BCM4347_CHIP_ID) || \
				(CHIPID(chipid) == BCM4357_CHIP_ID) || \
				(CHIPID(chipid) == BCM4361_CHIP_ID))
#define BCM4347_CHIP_GRPID	BCM4347_CHIP_ID: \
				case BCM4357_CHIP_ID: \
				case BCM4361_CHIP_ID

#define BCM4350_CHIP_ID		0x4350          /* 4350 chipcommon chipid */
#define BCM4354_CHIP_ID		0x4354          /* 4354 chipcommon chipid */
#define BCM4356_CHIP_ID		0x4356          /* 4356 chipcommon chipid */
#define BCM43567_CHIP_ID	0xAA2F          /* 43567 chipcommon chipid */
#define BCM43569_CHIP_ID	0xAA31          /* 43569 chipcommon chipid */
#define BCM4357_CHIP_ID		0x4357          /* 4357 chipcommon chipid */
#define BCM43570_CHIP_ID	0xAA32          /* 43570 chipcommon chipid */
#define BCM4358_CHIP_ID		0x4358          /* 4358 chipcommon chipid */
#define BCM43596_CHIP_ID	43596		/* 43596 chipcommon chipid */
#define BCM4361_CHIP_ID		0x4361          /* 4361 chipcommon chipid */
#define BCM4364_CHIP_ID		0x4364          /* 4364 chipcommon chipid */
#define BCM4371_CHIP_ID		0x4371          /* 4371 chipcommon chipid */

#define BCM4349_CHIP_ID		0x4349		/* 4349 chipcommon chipid */
#define BCM4355_CHIP_ID		0x4355		/* 4355 chipcommon chipid */
#define BCM4359_CHIP_ID		0x4359		/* 4359 chipcommon chipid */
#define BCM4355_CHIP(chipid)	(CHIPID(chipid) == BCM4355_CHIP_ID)
#define BCM4349_CHIP(chipid)	((CHIPID(chipid) == BCM4349_CHIP_ID) || \
				(CHIPID(chipid) == BCM4355_CHIP_ID) || \
				(CHIPID(chipid) == BCM4359_CHIP_ID))
#define BCM4349_CHIP_GRPID		BCM4349_CHIP_ID: \
					case BCM4355_CHIP_ID: \
					case BCM4359_CHIP_ID
#define BCM4350_CHIP(chipid)	((CHIPID(chipid) == BCM4350_CHIP_ID) || \
				(CHIPID(chipid) == BCM4354_CHIP_ID) || \
				(CHIPID(chipid) == BCM43567_CHIP_ID) || \
				(CHIPID(chipid) == BCM43569_CHIP_ID) || \
				(CHIPID(chipid) == BCM43570_CHIP_ID) || \
				(CHIPID(chipid) == BCM4358_CHIP_ID)) /* 4350 variations */

/* Board Flags */

/* Package IDs */

/* Board IDs */

#endif /* _bcmdevs_legacy_h_ */
