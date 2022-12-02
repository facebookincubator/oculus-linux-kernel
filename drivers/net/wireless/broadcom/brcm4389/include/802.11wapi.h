/*
 * WAPI specific types and constants relating to 802.11
 *
 * Copyright (C) 2022, Broadcom.
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

#ifndef _802_11wapi_h_
#define _802_11wapi_h_

#ifdef BCMWAPI_WAI
#define WAPI_IE_MIN_LEN		20	/* WAPI IE min length */
#define WAPI_VERSION		1	/* WAPI version */
#define WAPI_VERSION_LEN	2	/* WAPI version length */
#define WAPI_OUI		"\x00\x14\x72"	/* WAPI OUI */
#define WAPI_OUI_LEN		DOT11_OUI_LEN	/* WAPI OUI length */
#endif /* BCMWAPI_WAI */

#ifdef BCMWAPI_WPI
#define SMS4_KEY_LEN		16
#define SMS4_WPI_CBC_MAC_LEN	16
#endif

#endif /* _802_11wapi_h_ */
