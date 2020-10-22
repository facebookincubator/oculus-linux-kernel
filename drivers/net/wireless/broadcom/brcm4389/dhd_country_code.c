/*
 * Country Code file
 *
 * Copyright (C) 2020, Broadcom.
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
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 */
#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>

#include <ethernet.h>
#include <dngl_stats.h>
#include <bcmutils.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <dhd_linux.h>
#include <bcmdevs.h>

#include <linux/fcntl.h>
#include <linux/fs.h>

/* Locale table for sec */
const struct cntry_locales_custom translate_custom_table[] = {
	/* default ccode/regrev */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"GL", "GP", 2},
	{"AL", "AL", 2},
	{"DZ", "GB", 6},
	{"AS", "AS", 12},
	{"AI", "AI", 1},
	{"AF", "AD", 0},
	{"AG", "AG", 2},
	{"AR", "AU", 6},
	{"AW", "AW", 2},
	{"AU", "AU", 6},
	{"AT", "AT", 4},
	{"AZ", "AZ", 2},
	{"BS", "BS", 2},
	{"BH", "BH", 4},
	{"BD", "BD", 1},
	{"BY", "BY", 3},
	{"BE", "BE", 4},
	{"BM", "BM", 12},
	{"BA", "BA", 2},
	{"BR", "BR", 2},
	{"VG", "VG", 2},
	{"BN", "BN", 4},
	{"BG", "BG", 4},
	{"KH", "KH", 2},
	{"KY", "KY", 3},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"HR", "HR", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GF", "GF", 2},
	{"DE", "DE", 7},
	{"GR", "GR", 4},
	{"GD", "GD", 2},
	{"GP", "GP", 2},
	{"GU", "GU", 30},
	{"HK", "HK", 2},
	{"HU", "HU", 4},
	{"IS", "IS", 4},
	{"IN", "IN", 3},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "IL", 7},
	{"IT", "IT", 4},
	{"JP", "JP", 45},
	{"JO", "JO", 3},
	{"KE", "SA", 0},
	{"KW", "KW", 5},
	{"LA", "LA", 2},
	{"LV", "LV", 4},
	{"LB", "LB", 5},
	{"LS", "LS", 2},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"MO", "SG", 0},
	{"MK", "MK", 2},
	{"MW", "MW", 1},
	{"MY", "MY", 3},
	{"MV", "MV", 3},
	{"MT", "MT", 4},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MU", "MU", 2},
	{"YT", "YT", 2},
	{"MX", "MX", 44},
	{"MD", "MD", 2},
	{"MC", "MC", 1},
	{"ME", "ME", 2},
	{"MA", "MA", 2},
	{"NL", "NL", 4},
	{"AN", "GD", 2},
	{"NZ", "NZ", 4},
	{"NO", "NO", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PG", "AU", 6},
	{"PY", "PY", 2},
	{"PE", "PE", 20},
	{"PH", "PH", 5},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PR", "PR", 38},
	{"RE", "RE", 2},
	{"RO", "RO", 4},
	{"SN", "MA", 2},
	{"RS", "RS", 2},
	{"SK", "SK", 4},
	{"SI", "SI", 4},
	{"ES", "ES", 4},
	{"LK", "LK", 1},
	{"SE", "SE", 4},
	{"CH", "CH", 4},
	{"TW", "TW", 1},
	{"TH", "TH", 5},
	{"TT", "TT", 3},
	{"TR", "TR", 7},
	{"AE", "AE", 6},
	{"GB", "GB", 6},
	{"UY", "VE", 3},
	{"VI", "PR", 38},
	{"VA", "VA", 2},
	{"VE", "VE", 3},
	{"VN", "VN", 4},
	{"ZM", "LA", 2},
	{"EC", "EC", 21},
	{"SV", "SV", 25},
#if defined(BCM4358_CHIP) || defined(BCM4359_CHIP)
	{"KR", "KR", 70},
#else
	{"KR", "KR", 48},
#endif // endif
	{"RU", "RU", 13},
	{"UA", "UA", 8},
	{"GT", "GT", 1},
	{"MN", "MN", 1},
	{"NI", "NI", 2},
	{"UZ", "MA", 2},
	{"ZA", "ZA", 6},
	{"EG", "EG", 13},
	{"TN", "TN", 1},
	{"AO", "AD", 0},
	{"BT", "BJ", 0},
	{"BW", "BJ", 0},
	{"LY", "LI", 4},
	{"BO", "NG", 0},
	{"UM", "PR", 38},
#if defined(BCM4359_CHIP)
	/* Support FCC 15.407 (Part 15E) Changes, effective June 2 2014 */
	{"US", "US", 988},
	{"CA", "Q2", 993},
#else
	{"CA", "US", 1},
	{"US", "US", 1},
#endif // endif
};

/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
void get_customized_country_code(void *adapter, char *country_iso_code, wl_country_t *cspec)
{
	int size, i;

	size = ARRAYSIZE(translate_custom_table);

	if (cspec == 0)
		 return;

	if (size == 0)
		 return;

	for (i = 0; i < size; i++) {
		if (strcmp(country_iso_code, translate_custom_table[i].iso_abbrev) == 0) {
			memcpy(cspec->ccode,
				translate_custom_table[i].custom_locale, WLC_CNTRY_BUF_SZ);
			cspec->rev = translate_custom_table[i].custom_locale_rev;
			return;
		}
	}
	return;
}
