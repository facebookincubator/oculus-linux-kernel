// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2016 AmLogic, Inc.
 * Author: Michael Turquette <mturquette@baylibre.com>
 */

/*
 * MultiPhase Locked Loops are outputs from a PLL with additional frequency
 * scaling capabilities. MPLL rates are calculated as:
 *
 * f(N2_integer, SDM_IN ) = 2.0G/(N2_integer + SDM_IN/16384)
 */

#include <linux/clk-provider.h>
#include "clkc.h"

#define SDM_DEN 16384
#define N2_MIN	4
#define N2_MAX	511

static inline struct meson_clk_mpll_data *
meson_clk_mpll_data(struct clk_regmap *clk)
{
	return (struct meson_clk_mpll_data *)clk->data;
}

static long rate_from_params(unsigned long parent_rate,
			     unsigned int sdm,
			     unsigned int n2)
{
	unsigned long divisor = (SDM_DEN * n2) + sdm;

	if (n2 < N2_MIN)
		return -EINVAL;

	return DIV_ROUND_UP_ULL((u64)parent_rate * SDM_DEN, divisor);
}

static void params_from_rate(unsigned long requested_rate,
			     unsigned long parent_rate,
			     unsigned int *sdm,
			     unsigned int *n2,
			     u8 flags)
{
	uint64_t div = parent_rate;
	uint64_t frac = do_div(div, requested_rate);

	frac *= SDM_DEN;

	if (flags & CLK_MESON_MPLL_ROUND_CLOSEST)
		*sdm = DIV_ROUND_CLOSEST_ULL(frac, requested_rate);
	else
		*sdm = DIV_ROUND_UP_ULL(frac, requested_rate);

	if (*sdm == SDM_DEN) {
		*sdm = 0;
		div += 1;
	}

	if (div < N2_MIN) {
		*n2 = N2_MIN;
		*sdm = 0;
	} else if (div > N2_MAX) {
		*n2 = N2_MAX;
		*sdm = SDM_DEN - 1;
	} else {
		*n2 = div;
	}
}

static unsigned long mpll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_mpll_data *mpll = meson_clk_mpll_data(clk);
	unsigned int sdm, n2;
	long rate;

	sdm = meson_parm_read(clk->map, &mpll->sdm);
	n2 = meson_parm_read(clk->map, &mpll->n2);

	rate = rate_from_params(parent_rate, sdm, n2);
	return rate < 0 ? 0 : rate;
}

static long mpll_round_rate(struct clk_hw *hw,
			    unsigned long rate,
			    unsigned long *parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_mpll_data *mpll = meson_clk_mpll_data(clk);
	unsigned int sdm, n2;

	params_from_rate(rate, *parent_rate, &sdm, &n2, mpll->flags);
	return rate_from_params(*parent_rate, sdm, n2);
}

static int mpll_set_rate(struct clk_hw *hw,
			 unsigned long rate,
			 unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_mpll_data *mpll = meson_clk_mpll_data(clk);
	unsigned int sdm, n2;
	unsigned long flags = 0;

	params_from_rate(rate, parent_rate, &sdm, &n2, mpll->flags);

	if (mpll->lock)
		spin_lock_irqsave(mpll->lock, flags);
	else
		__acquire(mpll->lock);

	/* Enable and set the fractional part */
	meson_parm_write(clk->map, &mpll->sdm, sdm);
	meson_parm_write(clk->map, &mpll->sdm_en, 1);

	/* Set additional fractional part enable if required */
	if (MESON_PARM_APPLICABLE(&mpll->ssen))
		meson_parm_write(clk->map, &mpll->ssen, 1);

	/* Set the integer divider part */
	meson_parm_write(clk->map, &mpll->n2, n2);

	/* Set the magic misc bit if required */
	if (MESON_PARM_APPLICABLE(&mpll->misc))
		meson_parm_write(clk->map, &mpll->misc, 1);

	if (mpll->lock)
		spin_unlock_irqrestore(mpll->lock, flags);
	else
		__release(mpll->lock);

	return 0;
}

const struct clk_ops meson_clk_mpll_ro_ops = {
	.recalc_rate	= mpll_recalc_rate,
	.round_rate	= mpll_round_rate,
};

const struct clk_ops meson_clk_mpll_ops = {
	.recalc_rate	= mpll_recalc_rate,
	.round_rate	= mpll_round_rate,
	.set_rate	= mpll_set_rate,
};
