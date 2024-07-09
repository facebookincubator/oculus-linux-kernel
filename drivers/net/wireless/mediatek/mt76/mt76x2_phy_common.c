/*
 * Copyright (C) 2016 Felix Fietkau <nbd@nbd.name>
 * Copyright (C) 2018 Lorenzo Bianconi <lorenzo.bianconi83@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mt76x2.h"
#include "mt76x2_eeprom.h"

static void
mt76x2_adjust_high_lna_gain(struct mt76x2_dev *dev, int reg, s8 offset)
{
	s8 gain;

	gain = FIELD_GET(MT_BBP_AGC_LNA_HIGH_GAIN, mt76_rr(dev, MT_BBP(AGC, reg)));
	gain -= offset / 2;
	mt76_rmw_field(dev, MT_BBP(AGC, reg), MT_BBP_AGC_LNA_HIGH_GAIN, gain);
}

static void
mt76x2_adjust_agc_gain(struct mt76x2_dev *dev, int reg, s8 offset)
{
	s8 gain;

	gain = FIELD_GET(MT_BBP_AGC_GAIN, mt76_rr(dev, MT_BBP(AGC, reg)));
	gain += offset;
	mt76_rmw_field(dev, MT_BBP(AGC, reg), MT_BBP_AGC_GAIN, gain);
}

void mt76x2_apply_gain_adj(struct mt76x2_dev *dev)
{
	s8 *gain_adj = dev->cal.rx.high_gain;

	mt76x2_adjust_high_lna_gain(dev, 4, gain_adj[0]);
	mt76x2_adjust_high_lna_gain(dev, 5, gain_adj[1]);

	mt76x2_adjust_agc_gain(dev, 8, gain_adj[0]);
	mt76x2_adjust_agc_gain(dev, 9, gain_adj[1]);
}
EXPORT_SYMBOL_GPL(mt76x2_apply_gain_adj);

void mt76x2_phy_set_txpower_regs(struct mt76x2_dev *dev,
				 enum nl80211_band band)
{
	u32 pa_mode[2];
	u32 pa_mode_adj;

	if (band == NL80211_BAND_2GHZ) {
		pa_mode[0] = 0x010055ff;
		pa_mode[1] = 0x00550055;

		mt76_wr(dev, MT_TX_ALC_CFG_2, 0x35160a00);
		mt76_wr(dev, MT_TX_ALC_CFG_3, 0x35160a06);

		if (mt76x2_ext_pa_enabled(dev, band)) {
			mt76_wr(dev, MT_RF_PA_MODE_ADJ0, 0x0000ec00);
			mt76_wr(dev, MT_RF_PA_MODE_ADJ1, 0x0000ec00);
		} else {
			mt76_wr(dev, MT_RF_PA_MODE_ADJ0, 0xf4000200);
			mt76_wr(dev, MT_RF_PA_MODE_ADJ1, 0xfa000200);
		}
	} else {
		pa_mode[0] = 0x0000ffff;
		pa_mode[1] = 0x00ff00ff;

		if (mt76x2_ext_pa_enabled(dev, band)) {
			mt76_wr(dev, MT_TX_ALC_CFG_2, 0x2f0f0400);
			mt76_wr(dev, MT_TX_ALC_CFG_3, 0x2f0f0476);
		} else {
			mt76_wr(dev, MT_TX_ALC_CFG_2, 0x1b0f0400);
			mt76_wr(dev, MT_TX_ALC_CFG_3, 0x1b0f0476);
		}

		if (mt76x2_ext_pa_enabled(dev, band))
			pa_mode_adj = 0x04000000;
		else
			pa_mode_adj = 0;

		mt76_wr(dev, MT_RF_PA_MODE_ADJ0, pa_mode_adj);
		mt76_wr(dev, MT_RF_PA_MODE_ADJ1, pa_mode_adj);
	}

	mt76_wr(dev, MT_BB_PA_MODE_CFG0, pa_mode[0]);
	mt76_wr(dev, MT_BB_PA_MODE_CFG1, pa_mode[1]);
	mt76_wr(dev, MT_RF_PA_MODE_CFG0, pa_mode[0]);
	mt76_wr(dev, MT_RF_PA_MODE_CFG1, pa_mode[1]);

	if (mt76x2_ext_pa_enabled(dev, band)) {
		u32 val;

		if (band == NL80211_BAND_2GHZ)
			val = 0x3c3c023c;
		else
			val = 0x363c023c;

		mt76_wr(dev, MT_TX0_RF_GAIN_CORR, val);
		mt76_wr(dev, MT_TX1_RF_GAIN_CORR, val);
		mt76_wr(dev, MT_TX_ALC_CFG_4, 0x00001818);
	} else {
		if (band == NL80211_BAND_2GHZ) {
			u32 val = 0x0f3c3c3c;

			mt76_wr(dev, MT_TX0_RF_GAIN_CORR, val);
			mt76_wr(dev, MT_TX1_RF_GAIN_CORR, val);
			mt76_wr(dev, MT_TX_ALC_CFG_4, 0x00000606);
		} else {
			mt76_wr(dev, MT_TX0_RF_GAIN_CORR, 0x383c023c);
			mt76_wr(dev, MT_TX1_RF_GAIN_CORR, 0x24282e28);
			mt76_wr(dev, MT_TX_ALC_CFG_4, 0);
		}
	}
}
EXPORT_SYMBOL_GPL(mt76x2_phy_set_txpower_regs);

static void
mt76x2_limit_rate_power(struct mt76_rate_power *r, int limit)
{
	int i;

	for (i = 0; i < sizeof(r->all); i++)
		if (r->all[i] > limit)
			r->all[i] = limit;
}

static u32
mt76x2_tx_power_mask(u8 v1, u8 v2, u8 v3, u8 v4)
{
	u32 val = 0;

	val |= (v1 & (BIT(6) - 1)) << 0;
	val |= (v2 & (BIT(6) - 1)) << 8;
	val |= (v3 & (BIT(6) - 1)) << 16;
	val |= (v4 & (BIT(6) - 1)) << 24;
	return val;
}

static void
mt76x2_add_rate_power_offset(struct mt76_rate_power *r, int offset)
{
	int i;

	for (i = 0; i < sizeof(r->all); i++)
		r->all[i] += offset;
}

static int
mt76x2_get_min_rate_power(struct mt76_rate_power *r)
{
	int i;
	s8 ret = 0;

	for (i = 0; i < sizeof(r->all); i++) {
		if (!r->all[i])
			continue;

		if (ret)
			ret = min(ret, r->all[i]);
		else
			ret = r->all[i];
	}

	return ret;
}

void mt76x2_phy_set_txpower(struct mt76x2_dev *dev)
{
	enum nl80211_chan_width width = dev->mt76.chandef.width;
	struct ieee80211_channel *chan = dev->mt76.chandef.chan;
	struct mt76x2_tx_power_info txp;
	int txp_0, txp_1, delta = 0;
	struct mt76_rate_power t = {};
	int base_power, gain;

	mt76x2_get_power_info(dev, &txp, chan);

	if (width == NL80211_CHAN_WIDTH_40)
		delta = txp.delta_bw40;
	else if (width == NL80211_CHAN_WIDTH_80)
		delta = txp.delta_bw80;

	mt76x2_get_rate_power(dev, &t, chan);
	mt76x2_add_rate_power_offset(&t, txp.chain[0].target_power);
	mt76x2_limit_rate_power(&t, dev->txpower_conf);
	dev->txpower_cur = mt76x2_get_max_rate_power(&t);

	base_power = mt76x2_get_min_rate_power(&t);
	delta += base_power - txp.chain[0].target_power;
	txp_0 = txp.chain[0].target_power + txp.chain[0].delta + delta;
	txp_1 = txp.chain[1].target_power + txp.chain[1].delta + delta;

	gain = min(txp_0, txp_1);
	if (gain < 0) {
		base_power -= gain;
		txp_0 -= gain;
		txp_1 -= gain;
	} else if (gain > 0x2f) {
		base_power -= gain - 0x2f;
		txp_0 = 0x2f;
		txp_1 = 0x2f;
	}

	mt76x2_add_rate_power_offset(&t, -base_power);
	dev->target_power = txp.chain[0].target_power;
	dev->target_power_delta[0] = txp_0 - txp.chain[0].target_power;
	dev->target_power_delta[1] = txp_1 - txp.chain[0].target_power;
	dev->rate_power = t;

	mt76_rmw_field(dev, MT_TX_ALC_CFG_0, MT_TX_ALC_CFG_0_CH_INIT_0, txp_0);
	mt76_rmw_field(dev, MT_TX_ALC_CFG_0, MT_TX_ALC_CFG_0_CH_INIT_1, txp_1);

	mt76_wr(dev, MT_TX_PWR_CFG_0,
		mt76x2_tx_power_mask(t.cck[0], t.cck[2], t.ofdm[0], t.ofdm[2]));
	mt76_wr(dev, MT_TX_PWR_CFG_1,
		mt76x2_tx_power_mask(t.ofdm[4], t.ofdm[6], t.ht[0], t.ht[2]));
	mt76_wr(dev, MT_TX_PWR_CFG_2,
		mt76x2_tx_power_mask(t.ht[4], t.ht[6], t.ht[8], t.ht[10]));
	mt76_wr(dev, MT_TX_PWR_CFG_3,
		mt76x2_tx_power_mask(t.ht[12], t.ht[14], t.ht[0], t.ht[2]));
	mt76_wr(dev, MT_TX_PWR_CFG_4,
		mt76x2_tx_power_mask(t.ht[4], t.ht[6], 0, 0));
	mt76_wr(dev, MT_TX_PWR_CFG_7,
		mt76x2_tx_power_mask(t.ofdm[6], t.vht[8], t.ht[6], t.vht[8]));
	mt76_wr(dev, MT_TX_PWR_CFG_8,
		mt76x2_tx_power_mask(t.ht[14], 0, t.vht[8], t.vht[8]));
	mt76_wr(dev, MT_TX_PWR_CFG_9,
		mt76x2_tx_power_mask(t.ht[6], 0, t.vht[8], t.vht[8]));
}
EXPORT_SYMBOL_GPL(mt76x2_phy_set_txpower);

void mt76x2_configure_tx_delay(struct mt76x2_dev *dev,
			       enum nl80211_band band, u8 bw)
{
	u32 cfg0, cfg1;

	if (mt76x2_ext_pa_enabled(dev, band)) {
		cfg0 = bw ? 0x000b0c01 : 0x00101101;
		cfg1 = 0x00011414;
	} else {
		cfg0 = bw ? 0x000b0b01 : 0x00101001;
		cfg1 = 0x00021414;
	}
	mt76_wr(dev, MT_TX_SW_CFG0, cfg0);
	mt76_wr(dev, MT_TX_SW_CFG1, cfg1);

	mt76_rmw_field(dev, MT_XIFS_TIME_CFG, MT_XIFS_TIME_CFG_OFDM_SIFS, 15);
}
EXPORT_SYMBOL_GPL(mt76x2_configure_tx_delay);

void mt76x2_phy_set_bw(struct mt76x2_dev *dev, int width, u8 ctrl)
{
	int core_val, agc_val;

	switch (width) {
	case NL80211_CHAN_WIDTH_80:
		core_val = 3;
		agc_val = 7;
		break;
	case NL80211_CHAN_WIDTH_40:
		core_val = 2;
		agc_val = 3;
		break;
	default:
		core_val = 0;
		agc_val = 1;
		break;
	}

	mt76_rmw_field(dev, MT_BBP(CORE, 1), MT_BBP_CORE_R1_BW, core_val);
	mt76_rmw_field(dev, MT_BBP(AGC, 0), MT_BBP_AGC_R0_BW, agc_val);
	mt76_rmw_field(dev, MT_BBP(AGC, 0), MT_BBP_AGC_R0_CTRL_CHAN, ctrl);
	mt76_rmw_field(dev, MT_BBP(TXBE, 0), MT_BBP_TXBE_R0_CTRL_CHAN, ctrl);
}
EXPORT_SYMBOL_GPL(mt76x2_phy_set_bw);

void mt76x2_phy_set_band(struct mt76x2_dev *dev, int band, bool primary_upper)
{
	switch (band) {
	case NL80211_BAND_2GHZ:
		mt76_set(dev, MT_TX_BAND_CFG, MT_TX_BAND_CFG_2G);
		mt76_clear(dev, MT_TX_BAND_CFG, MT_TX_BAND_CFG_5G);
		break;
	case NL80211_BAND_5GHZ:
		mt76_clear(dev, MT_TX_BAND_CFG, MT_TX_BAND_CFG_2G);
		mt76_set(dev, MT_TX_BAND_CFG, MT_TX_BAND_CFG_5G);
		break;
	}

	mt76_rmw_field(dev, MT_TX_BAND_CFG, MT_TX_BAND_CFG_UPPER_40M,
		       primary_upper);
}
EXPORT_SYMBOL_GPL(mt76x2_phy_set_band);

int mt76x2_phy_get_min_avg_rssi(struct mt76x2_dev *dev)
{
	struct mt76x2_sta *sta;
	struct mt76_wcid *wcid;
	int i, j, min_rssi = 0;
	s8 cur_rssi;

	local_bh_disable();
	rcu_read_lock();

	for (i = 0; i < ARRAY_SIZE(dev->wcid_mask); i++) {
		unsigned long mask = dev->wcid_mask[i];

		if (!mask)
			continue;

		for (j = i * BITS_PER_LONG; mask; j++, mask >>= 1) {
			if (!(mask & 1))
				continue;

			wcid = rcu_dereference(dev->wcid[j]);
			if (!wcid)
				continue;

			sta = container_of(wcid, struct mt76x2_sta, wcid);
			spin_lock(&dev->mt76.rx_lock);
			if (sta->inactive_count++ < 5)
				cur_rssi = ewma_signal_read(&sta->rssi);
			else
				cur_rssi = 0;
			spin_unlock(&dev->mt76.rx_lock);

			if (cur_rssi < min_rssi)
				min_rssi = cur_rssi;
		}
	}

	rcu_read_unlock();
	local_bh_enable();

	if (!min_rssi)
		return -75;

	return min_rssi;
}
EXPORT_SYMBOL_GPL(mt76x2_phy_get_min_avg_rssi);
