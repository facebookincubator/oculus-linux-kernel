/*
 * Copyright (c) 2011, 2014-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * This module contains the regulatory domain private structure definitions .
 *
 */

#ifndef __CDS_REGDOMAIN_H
#define __CDS_REGDOMAIN_H

#include <wlan_cmn.h>
#include <reg_services_public_struct.h>
#include "reg_db.h"

#define MIN_TX_PWR_CAP    8
#define MAX_TX_PWR_CAP    24

#define CTRY_DEFAULT          0
#define CTRY_FLAG             0x8000
#define WORLD_ROAMING_FLAG    0x4000
#define WORLD_ROAMING_MASK    0x00F0
#define WORLD_ROAMING_PREFIX  0x0060
/**
 * struct reg_dmn_pair: regulatory domain pair
 * @reg_dmn_pair: reg domain pair
 * @reg_dmn_5ghz: 5G reg domain
 * @reg_dmn_2ghz: 2G reg domain
 * @single_cc: country with this reg domain
 */
struct reg_dmn_pair {
	uint16_t reg_dmn_pair;
	uint16_t reg_dmn_5ghz;
	uint16_t reg_dmn_2ghz;
	uint16_t single_cc;
};

/**
 * struct country_code_to_reg_dmn: country code to reg domain mapping
 * @country_code: country code
 * @reg_dmn_pair: regulatory domain pair
 * @alpha2: country alpha2
 * @name: country name
 */
struct country_code_to_reg_dmn {
	uint16_t country_code;
	uint16_t reg_dmn_pair;
	const char *alpha2;
	const char *name;
};

/**
 * struct reg_dmn: regulatory domain structure
 * @reg_dmn: regulatory domain
 * @conformance_test_limit:  CTL limit
 */
struct reg_dmn {
	uint16_t reg_dmn;
	uint8_t conformance_test_limit;
};

/**
 * struct reg_dmn_tables: reg domain table
 * @reg_dmn_pairs: list of reg domain pairs
 * @all_countries: list of countries
 * @reg_dmns: list of reg domains
 * @reg_dmn_pairs_cnt: count of reg domain pairs
 * @all_countries_cnt: count of countries
 * @reg_dmns_cnt: count of reg domains
 */
struct reg_dmn_tables {
	const struct reg_dmn_pair *reg_dmn_pairs;
	const struct country_code_to_reg_dmn *all_countries;
	const struct reg_dmn *reg_dmns;
	uint16_t reg_dmn_pairs_cnt;
	uint16_t all_countries_cnt;
	uint16_t reg_dmns_cnt;
};

int32_t cds_fill_some_regulatory_info(struct regulatory *reg);
int32_t cds_get_country_from_alpha2(uint8_t *alpha2);
void cds_fill_and_send_ctl_to_fw(struct regulatory *reg);
/**
 * cds_is_etsi_europe_country - check ETSI Europe country or not
 * @country: country string with two Characters
 *
 * Return: true if country in ETSI Europe country list
 */
bool cds_is_etsi_europe_country(uint8_t *country);
#endif /* __CDS_REGDOMAIN_H */
