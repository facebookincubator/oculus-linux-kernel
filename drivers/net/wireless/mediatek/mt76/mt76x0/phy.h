/*
 * (c) Copyright 2002-2010, Ralink Technology, Inc.
 * Copyright (C) 2018 Stanislaw Gruszka <stf_xl@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MT76X0_PHY_H_
#define _MT76X0_PHY_H_

#define RF_G_BAND 	0x0100
#define RF_A_BAND 	0x0200
#define RF_A_BAND_LB	0x0400
#define RF_A_BAND_MB	0x0800
#define RF_A_BAND_HB	0x1000
#define RF_A_BAND_11J	0x2000

#define RF_BW_20        1
#define RF_BW_40        2
#define RF_BW_10        4
#define RF_BW_80        8

#define MT_RF(bank, reg) ((bank) << 16 | (reg))
#define MT_RF_BANK(offset) (offset >> 16)
#define MT_RF_REG(offset) (offset & 0xff)

struct mt76x0_bbp_switch_item {
	u16 bw_band;
	struct mt76_reg_pair reg_pair;
};

struct mt76x0_rf_switch_item {
	u32 rf_bank_reg;
	u16 bw_band;
	u8 value;
};

struct mt76x0_freq_item {
	u8 channel;
	u32 band;
	u8 pllR37;
	u8 pllR36;
	u8 pllR35;
	u8 pllR34;
	u8 pllR33;
	u8 pllR32_b7b5;
	u8 pllR32_b4b0; /* PLL_DEN (Denomina - 8) */
	u8 pllR31_b7b5;
	u8 pllR31_b4b0; /* PLL_K (Nominator *)*/
	u8 pllR30_b7;	/* sdm_reset_n */
	u8 pllR30_b6b2; /* sdmmash_prbs,sin */
	u8 pllR30_b1;	/* sdm_bp */
	u16 pll_n;	/* R30<0>, R29<7:0> (hex) */
	u8 pllR28_b7b6; /* isi,iso */
	u8 pllR28_b5b4;	/* pfd_dly */
	u8 pllR28_b3b2;	/* clksel option */
	u32 pll_sdm_k;	/* R28<1:0>, R27<7:0>, R26<7:0> (hex) SDM_k */
	u8 pllR24_b1b0;	/* xo_div */
};

struct mt76x0_rate_pwr_item {
	s8 mcs_power;
	u8 rf_pa_mode;
};

struct mt76x0_rate_pwr_tab {
	struct mt76x0_rate_pwr_item cck[4];
	struct mt76x0_rate_pwr_item ofdm[8];
	struct mt76x0_rate_pwr_item ht[8];
	struct mt76x0_rate_pwr_item vht[10];
	struct mt76x0_rate_pwr_item stbc[8];
	struct mt76x0_rate_pwr_item mcs32;
};

#endif /* _MT76X0_PHY_H_ */
