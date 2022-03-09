/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mailbox_client.h>
#include <dt-bindings/clock/qcom,aop-qmp.h>

#define MAX_LEN			        96
#define MBOX_TOUT_MS			1000

struct qmp_pkt {
	u32 size;
	void *data;
};

#define DEFINE_CLK_AOP_QMP(_name, _class, _res, _estate, _dstate, _flags) \
	static struct clk_aop_qmp _name = {				\
		.msg.class = #_class,					\
		.msg.res = #_res,					\
		.enable_state = _estate,				\
		.disable_state = _dstate,				\
		.hw.init = &(struct clk_init_data){			\
			.ops = &aop_qmp_clk_ops,			\
			.name = #_name,					\
			.num_parents = 0,				\
			.flags = _flags,				\
		},							\
	}

#define to_aop_qmp_clk(hw) container_of(hw, struct clk_aop_qmp, hw)

/*
 * struct qmp_mbox_msg -  mailbox data to QMP
 * @class:	identifies the class.
 * @res:	identifies the resource in the class
 * @level:	identifies the level for the resource.
 */
struct qmp_mbox_msg {
	char class[MAX_LEN];
	char res[MAX_LEN];
	int level;
};

/*
 * struct clk_aop_qmp -  AOP clock
 * @dev:		The device that corresponds to this clock.
 * @hw:			The clock hardware for this clock.
 * @cl:			The client mailbox for this clock.
 * @mbox:		The mbox controller for this clock.
 * @level:		The clock level for this clock.
 * @enable_state:	The clock state when this clock is prepared.
 * @disable_state:	The clock state when this clock is unprepared.
 * @msg:		QMP data associated with this clock.
 * @enabled:		Status of the clock enable.
 */
struct clk_aop_qmp {
	struct device *dev;
	struct clk_hw hw;
	struct mbox_client cl;
	struct mbox_chan *mbox;
	int level;
	int enable_state;
	int disable_state;
	struct qmp_mbox_msg msg;
	bool enabled;
};

static DEFINE_MUTEX(clk_aop_lock);

static unsigned long clk_aop_qmp_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct clk_aop_qmp *clk = to_aop_qmp_clk(hw);

	return clk->level;
}

static long clk_aop_qmp_round_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long *parent_rate)
{
	return rate;
}

static int clk_aop_qmp_set_rate(struct clk_hw *hw, unsigned long rate,
						unsigned long parent_rate)
{
	char mbox_msg[MAX_LEN];
	struct qmp_pkt pkt;
	struct clk_aop_qmp *clk = to_aop_qmp_clk(hw);
	int ret = 0;

	mutex_lock(&clk_aop_lock);

	snprintf(mbox_msg, MAX_LEN, "{class: %s, res: %s, val: %ld}",
					clk->msg.class, clk->msg.res, rate);
	pkt.size = MAX_LEN;
	pkt.data = mbox_msg;

	ret = mbox_send_message(clk->mbox, &pkt);
	if (ret < 0) {
		pr_err("Failed to send set rate request of %lu for %s, ret %d\n",
					rate, clk_hw_get_name(hw), ret);
		goto err;
	} else
		/* Success: update the return value */
		ret = 0;

	/* update the current clock level once the mailbox message is sent */
	clk->level = rate;
err:
	mutex_unlock(&clk_aop_lock);

	return ret;
}

static int clk_aop_qmp_prepare(struct clk_hw *hw)
{
	char mbox_msg[MAX_LEN];
	unsigned long rate;
	int ret = 0;
	struct qmp_pkt pkt;
	struct clk_aop_qmp *clk = to_aop_qmp_clk(hw);

	mutex_lock(&clk_aop_lock);
	/*
	 * Return early if the clock has been enabled already. This
	 * is to avoid issues with sending duplicate enable requests.
	 */
	if (clk->enabled)
		goto err;

	if (clk->level)
		rate = clk->level;
	else
		rate = clk->enable_state;

	snprintf(mbox_msg, MAX_LEN, "{class: %s, res: %s, val: %ld}",
				clk->msg.class, clk->msg.res, rate);
	pkt.size = MAX_LEN;
	pkt.data = mbox_msg;

	ret = mbox_send_message(clk->mbox, &pkt);
	if (ret < 0) {
		pr_err("Failed to send clk prepare request for %s, ret %d\n",
					clk_hw_get_name(hw), ret);
		goto err;
	} else
		/* Success: update the return value */
		ret = 0;

	/* update the current clock level once the mailbox message is sent */
	clk->level = rate;

	clk->enabled = true;
err:
	mutex_unlock(&clk_aop_lock);

	return ret;
}

static void clk_aop_qmp_unprepare(struct clk_hw *hw)
{
	char mbox_msg[MAX_LEN];
	unsigned long rate;
	int ret = 0;
	struct qmp_pkt pkt;
	struct clk_aop_qmp *clk = to_aop_qmp_clk(hw);

	mutex_lock(&clk_aop_lock);

	if (!clk->enabled)
		goto err;

	rate = clk->disable_state;

	snprintf(mbox_msg, MAX_LEN, "{class: %s, res: %s, val: %ld}",
				clk->msg.class, clk->msg.res, rate);
	pkt.size = MAX_LEN;
	pkt.data = mbox_msg;

	ret = mbox_send_message(clk->mbox, &pkt);
	if (ret < 0) {
		pr_err("Failed to send clk unprepare request for %s, ret %d\n",
					clk_hw_get_name(hw), ret);
		goto err;
	}

	clk->enabled = false;
err:
	mutex_unlock(&clk_aop_lock);
}

static int clk_aop_qmp_is_enabled(struct clk_hw *hw)
{
	struct clk_aop_qmp *clk = to_aop_qmp_clk(hw);

	return clk->enabled;
}

static const struct clk_ops aop_qmp_clk_ops = {
	.prepare	= clk_aop_qmp_prepare,
	.unprepare	= clk_aop_qmp_unprepare,
	.recalc_rate	= clk_aop_qmp_recalc_rate,
	.set_rate	= clk_aop_qmp_set_rate,
	.round_rate	= clk_aop_qmp_round_rate,
	.is_enabled	= clk_aop_qmp_is_enabled,
};

DEFINE_CLK_AOP_QMP(qdss_qmp_clk, clock, qdss, QDSS_CLK_LEVEL_DYNAMIC,
			QDSS_CLK_LEVEL_OFF, CLK_ENABLE_HAND_OFF);
DEFINE_CLK_AOP_QMP(qdss_ao_qmp_clk, clock, qdss_ao, QDSS_CLK_LEVEL_DYNAMIC,
			QDSS_CLK_LEVEL_OFF, 0);

static struct clk_hw *aop_qmp_clk_hws[] = {
	[QDSS_CLK] = &qdss_qmp_clk.hw,
	[QDSS_AO_CLK] = &qdss_ao_qmp_clk.hw,
};

/*
 * Due to HW limitations on v1, the qdss_ao clock was not supported by the clock
 * driver on AOP.
 */
static void aop_qmp_fixup_v1(void)
{
	aop_qmp_clk_hws[QDSS_AO_CLK] = NULL;
}

static int qmp_update_client(struct clk_hw *hw, struct device *dev,
		struct mbox_chan *mbox)
{
	struct clk_aop_qmp *clk_aop = to_aop_qmp_clk(hw);

	/* Use mailbox client with blocking mode */
	clk_aop->cl.dev = dev;
	clk_aop->cl.tx_block = true;
	clk_aop->cl.tx_tout = MBOX_TOUT_MS;
	clk_aop->cl.knows_txdone = false;

	if (mbox) {
		clk_aop->mbox = mbox;
		return 0;
	}

	/* Allocate mailbox channel */
	mbox = clk_aop->mbox = mbox_request_channel(&clk_aop->cl, 0);
	if (IS_ERR(clk_aop->mbox) && PTR_ERR(clk_aop->mbox) != -EPROBE_DEFER) {
		dev_err(dev, "Failed to get mailbox channel %pK %ld\n",
						mbox, PTR_ERR(mbox));
		return PTR_ERR(clk_aop->mbox);
	}

	return 0;
}

static int aop_qmp_clk_probe(struct platform_device *pdev)
{
	struct clk *clk = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct mbox_chan *mbox = NULL;
	int num_clks = ARRAY_SIZE(aop_qmp_clk_hws);
	int ret = 0, i = 0;

	/*
	 * Allocate mbox channel for the first clock client. The same channel
	 * would be used for the rest of the clock clients.
	 */
	ret = qmp_update_client(aop_qmp_clk_hws[i], &pdev->dev, mbox);
	if (ret < 0)
		return ret;

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,aop-qmp-clk-v1"))
		aop_qmp_fixup_v1();

	for (i = 1; i < num_clks; i++) {
		if (!aop_qmp_clk_hws[i])
			continue;
		ret = qmp_update_client(aop_qmp_clk_hws[i], &pdev->dev, mbox);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to update QMP client %d\n",
							ret);
			goto fail;
		}
	}

	/*
	 * Proxy vote on the QDSS clock. This is needed to avoid issues with
	 * excessive requests on the QMP layer during the QDSS driver probe.
	 */
	ret = clk_aop_qmp_prepare(&qdss_qmp_clk.hw);
	if (ret < 0)
		goto fail;

	for (i = 0; i < num_clks; i++) {
		if (!aop_qmp_clk_hws[i])
			continue;
		clk = devm_clk_register(&pdev->dev, aop_qmp_clk_hws[i]);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			goto fail;
		}
	}

	ret = of_clk_add_provider(np, of_clk_src_simple_get, clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register clock provider\n");
		goto fail;
	}

	dev_info(&pdev->dev, "Registered clocks with AOP\n");

	return ret;
fail:
	mbox_free_channel(mbox);

	return ret;
}

static const struct of_device_id aop_qmp_clk_of_match[] = {
	{ .compatible = "qcom,aop-qmp-clk-v1" },
	{ .compatible = "qcom,aop-qmp-clk-v2" },
	{}
};

static struct platform_driver aop_qmp_clk_driver = {
	.driver = {
		.name = "qmp-aop-clk",
		.of_match_table = aop_qmp_clk_of_match,
	},
	.probe = aop_qmp_clk_probe,
};

static int __init aop_qmp_clk_init(void)
{
	return platform_driver_register(&aop_qmp_clk_driver);
}
subsys_initcall(aop_qmp_clk_init);
