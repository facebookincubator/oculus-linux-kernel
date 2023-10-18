/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
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

/**
 * DOC: reg_db.h
 * This file contains regulatory component data structures
 */

#ifndef __REG_DB_H
#define __REG_DB_H

/*
 * If COMPILE_REGDB_6G and CONFIG_BAND_6GHZ are defined, then
 * reg_6ghz_super_dmn_id and max_bw_6g are part of the
 * country_code_to_reg_domain table for a country
 * entry. If COMPILE_REGDB_6G and CONFIG_BAND_6GHZ are not defined, then they
 * are absent.
 *
 * COMPILE_REGDB_6G is not defined for the Partial offload platform.
 *
 * CE:- country entry
 */
#if defined(CONFIG_BAND_6GHZ) && defined(COMPILE_REGDB_6G)
#define CE(country_code, reg_dmn_pair_id, reg_6ghz_super_dmn_id,         \
	   alpha2, max_bw_2g, max_bw_5g, max_bw_6g, phymode_bitmap)      \
	{CTRY_ ## country_code, reg_dmn_pair_id, reg_6ghz_super_dmn_id,  \
	 #alpha2, max_bw_2g, max_bw_5g, max_bw_6g, phymode_bitmap}
#else
#define CE(country_code, reg_dmn_pair_id, reg_6ghz_super_dmn_id, alpha2, \
	   max_bw_2g, max_bw_5g, max_bw_6g, phymode_bitmap)              \
	{CTRY_ ## country_code, reg_dmn_pair_id, #alpha2, max_bw_2g,     \
	 max_bw_5g, phymode_bitmap}
#endif

/* Alpha2 code for world reg domain */
#define REG_WORLD_ALPHA2 "00"

enum reg_domain {
	NO_ENUMRD = 0x00,
	NULL1_WORLD = 0x03,
	NULL1_ETSIB = 0x07,
	NULL1_ETSIC = 0x08,

	FCC1_FCCA = 0x10,
	FCC1_WORLD = 0x11,
	FCC2_FCCA = 0x20,
	FCC2_WORLD = 0x21,
	FCC2_ETSIC = 0x22,
	FCC3_FCCA = 0x3A,
	FCC3_WORLD = 0x3B,
	FCC3_ETSIC = 0x3F,
	FCC4_FCCA = 0x12,
	FCC5_FCCA = 0x13,
	FCC6_WORLD = 0x23,
	FCC6_FCCA = 0x14,
	FCC7_FCCA = 0x15,
	FCC8_FCCA = 0x16,
	FCC8_WORLD = 0x09,
	FCC9_FCCA = 0x17,
	FCC10_FCCA = 0x18,
	FCC11_WORLD = 0x19,
	FCC13_WORLD = 0xE4,
	FCC14_FCCB = 0xE6,
	FCC14_WORLD = 0xD1,
	FCC15_FCCA = 0xEA,
	FCC16_FCCA = 0xE8,
	FCC17_FCCA = 0xE9,
	FCC17_WORLD = 0xEB,
	FCC17_ETSIC = 0xEC,
	FCC18_ETSIC = 0xED,

	ETSI1_WORLD = 0x37,
	ETSI2_WORLD = 0x35,
	ETSI3_WORLD = 0x36,
	ETSI3_ETSIA = 0x32,
	ETSI4_WORLD = 0x30,
	ETSI4_ETSIC = 0x38,
	ETSI5_WORLD = 0x39,
	ETSI6_WORLD = 0x34,
	ETSI7_WORLD = 0x3C,
	ETSI8_WORLD = 0x3D,
	ETSI9_WORLD = 0x3E,
	ETSI10_WORLD = 0x24,
	ETSI10_FCCA = 0x25,
	ETSI11_WORLD = 0x26,
	ETSI12_WORLD = 0x28,
	ETSI13_WORLD = 0x27,
	ETSI14_WORLD = 0x29,
	ETSI15_WORLD = 0x31,
	ETSI15_ETSIC = 0x7A,
	ETSI16_WORLD = 0x4A,
	ETSI17_WORLD = 0x4B,
	ETSI18_WORLD = 0x6E,
	ETSI19_WORLD = 0x7B,

	APL1_WORLD = 0x52,
	APL1_ETSIC = 0x55,
	APL2_WORLD = 0x45,
	APL2_ETSIC = 0x56,
	APL2_ETSID = 0x41,
	APL2_FCCA = 0x4D,
	APL4_WORLD = 0x42,
	APL6_WORLD = 0x5B,
	APL7_FCCA = 0x5C,
	APL8_WORLD = 0x5D,
	APL9_WORLD = 0x5E,
	APL9_MKKC  = 0x48,
	APL9_KRRA  = 0x43,
	APL10_WORLD = 0x5F,
	APL11_FCCA = 0x4F,
	APL12_WORLD = 0x51,
	APL13_WORLD = 0x5A,
	APL14_WORLD = 0x57,
	APL14_CHNA = 0x74,
	APL15_WORLD = 0x59,
	APL16_WORLD = 0x70,
	APL16_ETSIC = 0x6D,
	APL17_ETSIC = 0xE7,
	APL17_ETSID = 0xE0,
	APL19_ETSIC = 0x71,
	APL20_WORLD = 0xE5,
	APL23_WORLD = 0xE3,
	APL24_ETSIC = 0xE2,
	APL25_ETSIC = 0x75,
	APL26_ETSIC = 0x72,
	APL27_FCCA = 0x73,
	APL28_ETSIC = 0x76,

	WOR0_WORLD = 0x60,
	WOR1_WORLD = 0x61,
	WOR2_WORLD = 0x62,
	WOR3_WORLD = 0x63,
	WOR4_FCCA = 0x64,
	WOR5_ETSIC = 0x65,
	WOR01_WORLD = 0x66,
	WOR02_WORLD = 0x67,
	EU1_WORLD = 0x68,
	WOR9_WORLD = 0x69,
	WORA_WORLD = 0x6A,
	WORB_WORLD = 0x6B,
	WORC_WORLD = 0x6C,

	MKK3_MKKB = 0x80,
	MKK3_MKKA2 = 0x81,
	MKK3_MKKC = 0x82,
	MKK4_MKKB = 0x83,
	MKK4_MKKA2 = 0x84,
	MKK4_MKKC = 0x85,
	MKK5_MKKA = 0x99,
	MKK5_FCCA = 0x9A,
	MKK5_MKKB = 0x86,
	MKK5_MKKA2 = 0x87,
	MKK5_MKKC = 0x88,
	MKK3_MKKA = 0xF0,
	MKK3_MKKA1 = 0xF1,
	MKK3_FCCA = 0xF2,
	MKK4_MKKA = 0xF3,
	MKK4_MKKA1 = 0xF4,
	MKK4_FCCA = 0xF5,
	MKK9_MKKA = 0xF6,
	MKK9_FCCA = 0xFC,
	MKK9_MKKA1 = 0xFD,
	MKK9_MKKC = 0xFE,
	MKK9_MKKA2 = 0xFF,
	MKK10_MKKA = 0xF7,
	MKK10_FCCA = 0xD0,
	MKK10_MKKA1 = 0xD1,
	MKK10_MKKC = 0xD2,
	MKK10_MKKA2 = 0xD3,
	MKK11_MKKA = 0xD4,
	MKK11_FCCA = 0xD5,
	MKK11_MKKA1 = 0xD6,
	MKK11_MKKC = 0xD7,
	MKK11_MKKA2 = 0xD8,
	MKK16_MKKC = 0xDF,
	MKK17_MKKC = 0xE1,

	WORLD_60 = 0x60,
	WORLD_61 = 0x61,
	WORLD_62 = 0x62,
	WORLD_63 = 0x63,
	WORLD_65 = 0x65,
	WORLD_64 = 0x64,
	WORLD_66 = 0x66,
	WORLD_69 = 0x69,
	WORLD_67 = 0x67,
	WORLD_68 = 0x68,
	WORLD_6A = 0x6A,
	WORLD_6C = 0x6C,
};

enum reg_domains_5g {
	NULL1,

	FCC1,
	FCC2,
	FCC3,
	FCC4,
	FCC5,
	FCC6,
	FCC7,
	FCC8,
	FCC9,
	FCC10,
	FCC11,
	FCC13,
	FCC14,
	FCC15,
	FCC16,
	FCC17,
	FCC18,

	ETSI1,
	ETSI2,
	ETSI3,
	ETSI4,
	ETSI5,
	ETSI6,
	ETSI8,
	ETSI9,
	ETSI10,
	ETSI11,
	ETSI12,
	ETSI13,
	ETSI14,
	ETSI15,
	ETSI16,
	ETSI17,
	ETSI18,
	ETSI19,

	APL1,
	APL2,
	APL3,
	APL4,
	APL5,
	APL6,
	APL7,
	APL8,
	APL9,
	APL10,
	APL11,
	APL12,
	APL13,
	APL14,
	APL15,
	APL16,
	APL17,
	APL19,
	APL23,
	APL20,
	APL24,
	APL25,
	APL26,
	APL27,
	APL28,

	MKK3,
	MKK5,
	MKK11,
	MKK16,
	MKK17,
	MKK4,
	MKK9,
	MKK10,

	WORLD_5G_1,
	WORLD_5G_2,

	REG_DOMAINS_5G_MAX,
};

enum reg_domains_2g {
	FCCA,
	MKKA,
	MKKC,
	KRRA,
	CHNA,
	FCCB,
	ETSIC,
	WORLD,
	ETSID,
	WORLD_2G_1,
	WORLD_2G_2,
	WORLD_2G_3,

	REG_DOMAINS_2G_MAX,
};

enum country_code {
	CTRY_AFGHANISTAN = 4,
	CTRY_ALAND_ISLANDS = 248,
	CTRY_ALBANIA = 8,
	CTRY_ALGERIA = 12,
	CTRY_AMERICAN_SAMOA = 16,
	CTRY_ANDORRA = 20,
	CTRY_ANGUILLA = 660,
	CTRY_ANTIGUA_AND_BARBUDA = 28,
	CTRY_ARGENTINA = 32,
	CTRY_ARMENIA = 51,
	CTRY_MYANMAR = 104,
	CTRY_ARUBA = 533,
	CTRY_AUSTRALIA = 36,
	CTRY_AUSTRIA = 40,
	CTRY_AZERBAIJAN = 31,
	CTRY_BAHAMAS = 44,
	CTRY_BAHRAIN = 48,
	CTRY_BANGLADESH = 50,
	CTRY_BARBADOS = 52,
	CTRY_BELARUS = 112,
	CTRY_BELGIUM = 56,
	CTRY_BELIZE = 84,
	CTRY_BERMUDA = 60,
	CTRY_BHUTAN = 64,
	CTRY_BOLIVIA = 68,
	CTRY_BOSNIA_HERZ = 70,
	CTRY_BOTSWANA = 72,
	CTRY_BRAZIL = 76,
	CTRY_BRUNEI_DARUSSALAM = 96,
	CTRY_BULGARIA = 100,
	CTRY_BURUNDI = 108,
	CTRY_BURKINA_FASO = 854,
	CTRY_CAMBODIA = 116,
	CTRY_CAMEROON = 120,
	CTRY_CANADA = 124,
	CTRY_CAYMAN_ISLANDS = 136,
	CTRY_CENTRAL_AFRICA_REPUBLIC = 140,
	CTRY_CHAD = 148,
	CTRY_CHILE = 152,
	CTRY_CHINA = 156,
	CTRY_CHRISTMAS_ISLAND = 162,
	CTRY_COLOMBIA = 170,
	CTRY_CONGO = 178,
	CTRY_CONGO_DEMOCRATIC_REPUBLIC = 180,
	CTRY_COOK_ISLANDS = 184,
	CTRY_COSTA_RICA = 188,
	CTRY_COTE_DIVOIRE = 384,
	CTRY_CROATIA = 191,
	CTRY_CURACAO = 531,
	CTRY_CYPRUS = 196,
	CTRY_CZECH = 203,
	CTRY_DENMARK = 208,
	CTRY_DOMINICA = 212,
	CTRY_DOMINICAN_REPUBLIC = 214,
	CTRY_ECUADOR = 218,
	CTRY_EGYPT = 818,
	CTRY_EL_SALVADOR = 222,
	CTRY_ESTONIA = 233,
	CTRY_ETHIOPIA = 231,
	CTRY_FALKLAND_ISLANDS = 238,
	CTRY_FAROE_ISLANDS = 234,
	CTRY_FIJI = 242,
	CTRY_FINLAND = 246,
	CTRY_FRANCE = 250,
	CTRY_FRENCH_GUIANA = 254,
	CTRY_FRENCH_POLYNESIA = 258,
	CTRY_FRENCH_SOUTHERN_TERRITORIES = 260,
	CTRY_GABON = 266,
	CTRY_GEORGIA = 268,
	CTRY_GERMANY = 276,
	CTRY_GHANA = 288,
	CTRY_GIBRALTAR = 292,
	CTRY_GREECE = 300,
	CTRY_GREENLAND = 304,
	CTRY_GRENADA = 308,
	CTRY_GUADELOUPE = 312,
	CTRY_GUAM = 316,
	CTRY_GUATEMALA = 320,
	CTRY_GUERNSEY = 831,
	CTRY_GUYANA = 328,
	CTRY_HAITI = 332,
	CTRY_HEARD_ISLAND_AND_MCDONALD_ISLANDS = 334,
	CTRY_HOLY_SEE = 336,
	CTRY_HONDURAS = 340,
	CTRY_HONG_KONG = 344,
	CTRY_HUNGARY = 348,
	CTRY_ICELAND = 352,
	CTRY_INDIA = 356,
	CTRY_INDONESIA = 360,
	CTRY_IRAQ = 368,
	CTRY_IRELAND = 372,
	CTRY_ISLE_OF_MAN = 833,
	CTRY_ISRAEL = 376,
	CTRY_ITALY = 380,
	CTRY_JAMAICA = 388,
	CTRY_JAPAN = 392,
	CTRY_JAPAN15 = 4015,
	CTRY_JERSEY = 832,
	CTRY_JORDAN = 400,
	CTRY_KAZAKHSTAN = 398,
	CTRY_KENYA = 404,
	CTRY_KOREA_ROC = 410,
	CTRY_KUWAIT = 414,
	CTRY_LAO_PEOPLES_DEMOCRATIC_REPUBLIC = 418,
	CTRY_LATVIA = 428,
	CTRY_LEBANON = 422,
	CTRY_LESOTHO = 426,
	CTRY_LIBYA = 434,
	CTRY_LIECHTENSTEIN = 438,
	CTRY_LITHUANIA = 440,
	CTRY_LUXEMBOURG = 442,
	CTRY_MACAU = 446,
	CTRY_MACEDONIA = 807,
	CTRY_MALAWI = 454,
	CTRY_MALAYSIA = 458,
	CTRY_MALDIVES = 462,
	CTRY_MALTA = 470,
	CTRY_MARSHALL_ISLANDS = 584,
	CTRY_MARTINIQUE = 474,
	CTRY_MAURITANIA = 478,
	CTRY_MAURITIUS = 480,
	CTRY_MAYOTTE = 175,
	CTRY_MEXICO = 484,
	CTRY_MICRONESIA = 583,
	CTRY_MOLDOVA = 498,
	CTRY_MONACO = 492,
	CTRY_MONGOLIA = 496,
	CTRY_MONTENEGRO = 499,
	CTRY_MONTSERRAT = 500,
	CTRY_MOROCCO = 504,
	CTRY_NAMIBIA = 516,
	CTRY_NEPAL = 524,
	CTRY_NETHERLANDS = 528,
	CTRY_NETHERLANDS_ANTILLES = 530,
	CTRY_NEW_CALEDONIA = 540,
	CTRY_NEW_ZEALAND = 554,
	CTRY_NIGERIA = 566,
	CTRY_NORTHERN_MARIANA_ISLANDS = 580,
	CTRY_NICARAGUA = 558,
	CTRY_NIUE = 570,
	CTRY_NORFOLK_ISLAND = 574,
	CTRY_NORWAY = 578,
	CTRY_OMAN = 512,
	CTRY_PAKISTAN = 586,
	CTRY_PALAU = 585,
	CTRY_PANAMA = 591,
	CTRY_PAPUA_NEW_GUINEA = 598,
	CTRY_PARAGUAY = 600,
	CTRY_PERU = 604,
	CTRY_PHILIPPINES = 608,
	CTRY_POLAND = 616,
	CTRY_PORTUGAL = 620,
	CTRY_PUERTO_RICO = 630,
	CTRY_QATAR = 634,
	CTRY_REUNION = 638,
	CTRY_ROMANIA = 642,
	CTRY_RUSSIA = 643,
	CTRY_RWANDA = 646,
	CTRY_SAINT_BARTHELEMY = 652,
	CTRY_SAINT_HELENA_ASCENSION_AND_TRISTAN_DA_CUNHA = 654,
	CTRY_SAINT_KITTS_AND_NEVIS = 659,
	CTRY_SAINT_LUCIA = 662,
	CTRY_SAINT_MARTIN = 663,
	CTRY_SAINT_PIERRE_AND_MIQUELON = 666,
	CTRY_SAINT_VINCENT_AND_THE_GRENADIENS = 670,
	CTRY_SAMOA = 882,
	CTRY_SAN_MARINO = 674,
	CTRY_SAO_TOME_AND_PRINCIPE = 678,
	CTRY_SAUDI_ARABIA = 682,
	CTRY_SENEGAL = 686,
	CTRY_SERBIA = 688,
	CTRY_SINGAPORE = 702,
	CTRY_SINT_MAARTEN = 534,
	CTRY_SLOVAKIA = 703,
	CTRY_SLOVENIA = 705,
	CTRY_SOUTH_AFRICA = 710,
	CTRY_SPAIN = 724,
	CTRY_SURINAME = 740,
	CTRY_SRI_LANKA = 144,
	CTRY_SVALBARD_AND_JAN_MAYEN = 744,
	CTRY_SWEDEN = 752,
	CTRY_SWITZERLAND = 756,
	CTRY_TAIWAN = 158,
	CTRY_TANZANIA = 834,
	CTRY_THAILAND = 764,
	CTRY_TOGO = 768,
	CTRY_TRINIDAD_Y_TOBAGO = 780,
	CTRY_TUNISIA = 788,
	CTRY_TURKEY = 792,
	CTRY_TURKS_AND_CAICOS = 796,
	CTRY_UGANDA = 800,
	CTRY_UKRAINE = 804,
	CTRY_UAE = 784,
	CTRY_UNITED_KINGDOM = 826,
	CTRY_UNITED_STATES = 840,
	CTRY_UNITED_STATES_MINOR_OUTLYING_ISLANDS = 581,
	CTRY_URUGUAY = 858,
	CTRY_UZBEKISTAN = 860,
	CTRY_VANUATU = 548,
	CTRY_VENEZUELA = 862,
	CTRY_VIET_NAM = 704,
	CTRY_VIRGIN_ISLANDS = 850,
	CTRY_VIRGIN_ISLANDS_BRITISH = 92,
	CTRY_WALLIS_AND_FUTUNA = 876,
	CTRY_XA = 4100,   /* Used by Linux Client for legacy MKK domain */
	CTRY_YEMEN = 887,
	CTRY_ZIMBABWE = 716,
	CTRY_ZAMBIA = 884,
};

/**
 * struct regulatory_rule
 * @start_freq: start frequency
 * @end_freq: end frequency
 * @max_bw: maximum bandwidth
 * @reg_power: regulatory power
 * @flags: regulatory flags
 */
struct regulatory_rule {
	uint16_t start_freq;
	uint16_t end_freq;
	uint16_t max_bw;
	uint8_t reg_power;
	uint16_t flags;
};

#if defined(CONFIG_BAND_6GHZ) && defined(COMPILE_REGDB_6G)
/**
 * struct regulatory_rule_ext
 * @start_freq: start frequency in MHz
 * @end_freq: end frequency in MHz
 * @max_bw: maximum bandwidth in MHz
 * @eirp_power: EIRP power in dBm
 * @psd_power: Max PSD power in dBm per MHz
 * @flags: regulatory flags
 */
struct regulatory_rule_ext {
	uint16_t start_freq;
	uint16_t end_freq;
	uint16_t max_bw;
	uint8_t eirp_power;
	int8_t psd_power;
	uint16_t flags;
};
#endif

/**
 * struct regdomain
 * @ctl_val: CTL value
 * @dfs_region: dfs region
 * @min_bw: minimum bandwidth
 * @max_bw: maximum bandwidth
 * @num_reg_rules: number of regulatory rules
 * @reg_rules_id: regulatory rule index
 */
struct regdomain   {
	uint8_t ctl_val;
	enum dfs_reg dfs_region;
	uint16_t min_bw;
	uint16_t max_bw;
	uint8_t ant_gain;
	uint8_t num_reg_rules;
	uint8_t reg_rule_id[MAX_REG_RULES];
};

#if defined(CONFIG_BAND_6GHZ) && defined(COMPILE_REGDB_6G)
#define REG_MAX_PSD (0x7F) /* 127=63.5 dBm/MHz */

/**
 * struct sub_6g_regdomain
 * @min_bw: Minimum bandwidth in MHz
 * @max_bw: Maximum bandwidth in MHz
 * @num_reg_rules: number of regulatory rules
 * @reg_rules_id: regulatory rule index
 */
struct sub_6g_regdomain   {
	uint16_t min_bw;
	uint16_t max_bw;
	uint8_t num_reg_rules;
	uint8_t sixg_reg_rule_id[MAX_REG_RULES];
};
#endif

/**
 * struct country_code_to_reg_domain
 * @country_code: country code
 * @reg_dmn_pair_id: reg domainpair id
 * @reg_6ghz_super_dmn_id: 6GHz super domain id
 * @alpha2: iso-3166 alpha2
 * @max_bw_2g: maximum 2g bandwidth in MHz
 * @max_bw_5g: maximum 5g bandwidth in MHz
 * @max_bw_6g: maximum 6g bandwidth in MHz
 * @phymode_bitmap: phymodes not supported
 */
struct country_code_to_reg_domain   {
	uint16_t country_code;
	uint16_t reg_dmn_pair_id;
#if defined(CONFIG_BAND_6GHZ) && defined(COMPILE_REGDB_6G)
	uint16_t reg_6ghz_super_dmn_id;
#endif
	uint8_t alpha2[REG_ALPHA2_LEN + 1];
	uint16_t max_bw_2g;
	uint16_t max_bw_5g;
#if defined(CONFIG_BAND_6GHZ) && defined(COMPILE_REGDB_6G)
	uint16_t max_bw_6g;
#endif
	uint16_t phymode_bitmap;
};

/**
 * struct reg_domain_pair
 * @reg_dmn_pair_id: reg domainpiar value
 * @dmn_id_5g: 5g reg domain value
 * @dmn_id_2g: 2g regdomain value
 */
struct reg_domain_pair {
	uint16_t reg_dmn_pair_id;
	uint8_t dmn_id_5g;
	uint8_t dmn_id_2g;
};

#if defined(CONFIG_BAND_6GHZ)
/**
 * enum reg_super_domain_6g - 6G Super Domain enumeration
 * @FCC1_6G_01: Super domain FCC1_6G_01 for US
 * @ETSI1_6G_02: Super domain ETSI1_6G_02 for EU
 * @ETSI2_6G_03: Super domain ETSI2_6G_03 for UK
 * @APL1_6G_04: Super domain APL1_6G_04 for Korea
 * @FCC1_6G_05: Super domain FCC1_6G_05 for Chile
 * @APL2_6G_06: Super domain APL2_6G_06 for Guatemala
 * @FCC1_6G_07: Super domain FCC1_6G_07 for Brazil
 * @APL3_6G_08: Super domain APL3_6G_08 for UAE
 * @FCC1_6G_09: Super domain FCC1_6G_09 for US AFC Testing
 * @APL6_6G_0A: Super domain APL6_6G_0A for Saudi Arabia LPI STA and AP
 * @MKK1_6G_0B: Super domain MKK1_6G_0B for Japan LPI and VLP
 * @ETSI2_6G_0C: Super domain ETSI2_6G_0C for Australia LPI and VLP
 * @ETSI2_6G_0D: Super domain ETSI2_6G_0D for ISRAEL LPI
 * @ETSI2_6G_0E: Super domain ETSI2_6G_0E for NEW ZEALAND LPI and VLP
 * @FCC2_6G_10: Super domain FCC1_6G_10 for Canada LPI &
		SP(VLP to be added later)
 * @APL4_6G_11: Super domain APL3_6G_11 for Costa Rica LPI and VLP
 * @APL5_6G_12: Super domain APL3_6G_12 for CHILE LPI and VLP
 */
enum reg_super_domain_6g {
	FCC1_6G_01 = 0x01,
	ETSI1_6G_02 = 0x02,
	ETSI2_6G_03 = 0x03,
	APL1_6G_04 = 0x04,
	FCC1_6G_05 = 0x05,
	APL2_6G_06 = 0x06,
	FCC1_6G_07 = 0x07,
	APL3_6G_08 = 0x08,
	FCC1_6G_09 = 0x09,
	APL6_6G_0A = 0x0A,
	MKK1_6G_0B = 0x0B,
	ETSI2_6G_0C = 0x0C,
	ETSI1_6G_0D = 0x0D,
	ETSI2_6G_0E = 0x0E,
	FCC2_6G_10 = 0x10,
	APL4_6G_11 = 0x11,
	APL5_6G_12 = 0x12,
};

#if defined(COMPILE_REGDB_6G)
/**
 * struct sixghz_super_to_subdomains
 * @reg_6ghz_super_dmn_id: 6G super domain id.
 * @reg_domain_6g_id_ap_lpi: 6G domain id for LPI AP.
 * @reg_domain_6g_id_ap_sp: 6G domain id for SP AP.
 * @reg_domain_6g_id_ap_vlp: 6G domain id for VLP AP.
 * @reg_domain_6g_id_client_lpi: 6G domain id for clients of the LPI AP.
 * @reg_domain_6g_id_client_sp: 6G domain id for clients of the SP AP.
 * @reg_domain_6g_id_client_vlp: 6G domain id for clients of the VLP AP.
 */
struct sixghz_super_to_subdomains {
	uint16_t reg_6ghz_super_dmn_id;
	uint8_t reg_domain_6g_id_ap_lpi;
	uint8_t reg_domain_6g_id_ap_sp;
	uint8_t reg_domain_6g_id_ap_vlp;
	uint8_t reg_domain_6g_id_client_lpi[REG_MAX_CLIENT_TYPE];
	uint8_t reg_domain_6g_id_client_sp[REG_MAX_CLIENT_TYPE];
	uint8_t reg_domain_6g_id_client_vlp[REG_MAX_CLIENT_TYPE];
};
#endif
#endif

QDF_STATUS reg_get_num_countries(int *num_countries);

QDF_STATUS reg_get_num_reg_dmn_pairs(int *num_reg_dmn);

/**
 * reg_etsi13_regdmn () - Checks if the reg domain is ETSI13 or not
 * @reg_dmn: reg domain
 *
 * Return: true or false
 */
bool reg_etsi13_regdmn(uint8_t reg_dmn);

/**
 * reg_fcc_regdmn () - Checks if the reg domain is FCC3/FCC8/FCC15/FCC16 or not
 * @reg_dmn: reg domain
 *
 * Return: true or false
 */
bool reg_fcc_regdmn(uint8_t reg_dmn);

#ifdef WLAN_REG_PARTIAL_OFFLOAD
QDF_STATUS reg_get_default_country(uint16_t *default_country);

/**
 * reg_en302_502_regdmn() - Check if the reg domain is en302_502 applicable.
 * @reg_dmn: Regulatory domain pair ID.
 *
 * Return: True if EN302_502 applicable, else false.
 */
bool reg_en302_502_regdmn(uint16_t reg_dmn);
#endif
#endif
