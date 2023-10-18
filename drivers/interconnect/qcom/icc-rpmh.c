// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "bcm-voter.h"
#include "icc-debug.h"
#include "icc-rpmh.h"
#include "qnoc-qos.h"

static LIST_HEAD(qnoc_probe_list);
static DEFINE_MUTEX(probe_list_lock);

/**
 * qcom_icc_pre_aggregate - cleans up stale values from prior icc_set
 * @node: icc node to operate on
 */
void qcom_icc_pre_aggregate(struct icc_node *node)
{
	size_t i;
	struct qcom_icc_node *qn;
	struct qcom_icc_provider *qp;

	qn = node->data;
	qp = to_qcom_provider(node->provider);

	for (i = 0; i < QCOM_ICC_NUM_BUCKETS; i++) {
		qn->sum_avg[i] = 0;
		qn->max_peak[i] = 0;
		qn->perf_mode[i] = false;
	}

	for (i = 0; i < qn->num_bcms; i++)
		qcom_icc_bcm_voter_add(qp->voters[qn->bcms[i]->voter_idx],
				       qn->bcms[i]);
}
EXPORT_SYMBOL_GPL(qcom_icc_pre_aggregate);

/**
 * qcom_icc_aggregate - aggregate bw for buckets indicated by tag
 * @node: node to aggregate
 * @tag: tag to indicate which buckets to aggregate
 * @avg_bw: new bw to sum aggregate
 * @peak_bw: new bw to max aggregate
 * @agg_avg: existing aggregate avg bw val
 * @agg_peak: existing aggregate peak bw val
 */
int qcom_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
		       u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	size_t i;
	struct qcom_icc_node *qn;

	qn = node->data;

	if (!tag)
		tag = QCOM_ICC_TAG_ALWAYS;

	for (i = 0; i < QCOM_ICC_NUM_BUCKETS; i++) {
		if (tag & BIT(i)) {
			qn->sum_avg[i] += avg_bw;
			qn->max_peak[i] = max_t(u32, qn->max_peak[i], peak_bw);
			if (tag & QCOM_ICC_TAG_PERF_MODE && (avg_bw || peak_bw))
				qn->perf_mode[i] = true;
		}

		if (node->init_avg || node->init_peak) {
			qn->sum_avg[i] = max_t(u64, qn->sum_avg[i], node->init_avg);
			qn->max_peak[i] = max_t(u64, qn->max_peak[i], node->init_peak);
		}
	}

	*agg_avg += avg_bw;
	*agg_peak = max_t(u32, *agg_peak, peak_bw);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_icc_aggregate);

int qcom_icc_aggregate_stub(struct icc_node *node, u32 tag, u32 avg_bw,
			    u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	return 0;
}
EXPORT_SYMBOL(qcom_icc_aggregate_stub);

/**
 * qcom_icc_set - set the constraints based on path
 * @src: source node for the path to set constraints on
 * @dst: destination node for the path to set constraints on
 *
 * Return: 0 on success, or an error code otherwise
 */
int qcom_icc_set(struct icc_node *src, struct icc_node *dst)
{
	struct qcom_icc_provider *qp;
	struct qcom_icc_node *qn;
	struct icc_node *node;
	int i, ret = 0;

	if (!src)
		node = dst;
	else
		node = src;

	qp = to_qcom_provider(node->provider);
	qn = node->data;

	for (i = 0; i < qp->num_voters; i++)
		qcom_icc_bcm_voter_commit(qp->voters[i]);

	/* Defer setting QoS until the first non-zero bandwidth request. */
	if (qn && qn->qosbox && !qn->qosbox->initialized &&
	    (node->avg_bw || node->peak_bw)) {
		ret = clk_bulk_prepare_enable(qp->num_clks, qp->clks);
		if (ret) {
			pr_err("%s: Clock enable failed for node %s\n",
				__func__, node->name);
			return ret;
		}

		qn->noc_ops->set_qos(qn);
		clk_bulk_disable_unprepare(qp->num_clks, qp->clks);
		qn->qosbox->initialized = true;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_icc_set);

int qcom_icc_set_stub(struct icc_node *src, struct icc_node *dst)
{
	return 0;
}
EXPORT_SYMBOL(qcom_icc_set_stub);

int qcom_icc_get_bw_stub(struct icc_node *node, u32 *avg, u32 *peak)
{
	*avg = 0;
	*peak = 0;

	return 0;
}
EXPORT_SYMBOL(qcom_icc_get_bw_stub);

struct icc_node_data *qcom_icc_xlate_extended(struct of_phandle_args *spec, void *data)
{
	struct icc_node_data *ndata;
	struct icc_node *node;

	node = of_icc_xlate_onecell(spec, data);
	if (IS_ERR(node))
		return ERR_CAST(node);

	ndata = kzalloc(sizeof(*ndata), GFP_KERNEL);
	if (!ndata)
		return ERR_PTR(-ENOMEM);

	ndata->node = node;

	if (spec->args_count == 2)
		ndata->tag = spec->args[1];

	if (spec->args_count > 2)
		pr_warn("%pOF: Too many arguments, path tag is not parsed\n", spec->np);

	return ndata;
}
EXPORT_SYMBOL_GPL(qcom_icc_xlate_extended);

/**
 * qcom_icc_bcm_init - populates bcm aux data and connect qnodes
 * @bcm: bcm to be initialized
 * @dev: associated provider device
 *
 * Return: 0 on success, or an error code otherwise
 */
int qcom_icc_bcm_init(struct qcom_icc_bcm *bcm, struct device *dev)
{
	struct qcom_icc_node *qn;
	const struct bcm_db *data;
	size_t data_count;
	int i;

	/* BCM is already initialised*/
	if (bcm->addr)
		return 0;

	bcm->addr = cmd_db_read_addr(bcm->name);
	if (!bcm->addr) {
		dev_err(dev, "%s could not find RPMh address\n",
			bcm->name);
		return -EINVAL;
	}

	data = cmd_db_read_aux_data(bcm->name, &data_count);
	if (IS_ERR(data)) {
		dev_err(dev, "%s command db read error (%ld)\n",
			bcm->name, PTR_ERR(data));
		return PTR_ERR(data);
	}
	if (!data_count) {
		dev_err(dev, "%s command db missing or partial aux data\n",
			bcm->name);
		return -EINVAL;
	}

	bcm->aux_data.unit = max_t(u32, 1, le32_to_cpu(data->unit));
	bcm->aux_data.width = max_t(u16, 1, le16_to_cpu(data->width));
	bcm->aux_data.vcd = data->vcd;
	bcm->aux_data.reserved = data->reserved;
	INIT_LIST_HEAD(&bcm->list);
	INIT_LIST_HEAD(&bcm->ws_list);

	if (!bcm->vote_scale)
		bcm->vote_scale = 1000;

	/* Link Qnodes to their respective BCMs */
	for (i = 0; i < bcm->num_nodes; i++) {
		qn = bcm->nodes[i];
		qn->bcms[qn->num_bcms] = bcm;
		qn->num_bcms++;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_icc_bcm_init);

static bool bcm_needs_qos_proxy(struct qcom_icc_bcm *bcm)
{
	int i;

	if (bcm->voter_idx == 0)
		for (i = 0; i < bcm->num_nodes; i++)
			if (bcm->nodes[i]->qosbox)
				return true;

	return false;
}

static int enable_qos_deps(struct qcom_icc_provider *qp)
{
	struct qcom_icc_bcm *bcm;
	struct bcm_voter *voter;
	bool keepalive;
	int ret, i;

	for (i = 0; i < qp->num_bcms; i++) {
		bcm = qp->bcms[i];
		if (bcm_needs_qos_proxy(bcm)) {
			keepalive = bcm->keepalive;
			bcm->keepalive = true;

			voter = qp->voters[bcm->voter_idx];
			qcom_icc_bcm_voter_add(voter, bcm);
			ret = qcom_icc_bcm_voter_commit(voter);

			bcm->keepalive = keepalive;

			if (ret) {
				dev_err(qp->dev, "failed to vote BW to %s for QoS\n",
					bcm->name);
				return ret;
			}
		}
	}

	ret = clk_bulk_prepare_enable(qp->num_clks, qp->clks);
	if (ret) {
		dev_err(qp->dev, "failed to enable clocks for QoS\n");
		return ret;
	}

	return 0;
}

static void disable_qos_deps(struct qcom_icc_provider *qp)
{
	struct qcom_icc_bcm *bcm;
	struct bcm_voter *voter;
	int i;

	clk_bulk_disable_unprepare(qp->num_clks, qp->clks);

	for (i = 0; i < qp->num_bcms; i++) {
		bcm = qp->bcms[i];
		if (bcm_needs_qos_proxy(bcm)) {
			voter = qp->voters[bcm->voter_idx];
			qcom_icc_bcm_voter_add(voter, bcm);
			qcom_icc_bcm_voter_commit(voter);
		}
	}
}

static struct regmap *qcom_icc_rpmh_map(struct platform_device *pdev,
					const struct qcom_icc_desc *desc)
{
	void __iomem *base;
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return NULL;

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	return devm_regmap_init_mmio(dev, base, desc->config);
}

int qcom_icc_rpmh_probe(struct platform_device *pdev)
{
	const struct qcom_icc_desc *desc;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct qcom_icc_node **qnodes;
	struct qcom_icc_provider *qp;
	struct icc_node *node;
	size_t num_nodes, i;
	int ret;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	qnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	qp = devm_kzalloc(&pdev->dev, sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return -ENOMEM;

	data = devm_kcalloc(&pdev->dev, num_nodes, sizeof(*node), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider = &qp->provider;
	provider->dev = &pdev->dev;
	provider->set = qcom_icc_set_stub;
	provider->pre_aggregate = qcom_icc_pre_aggregate;
	provider->aggregate = qcom_icc_aggregate_stub;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;
	provider->get_bw = qcom_icc_get_bw_stub;

	qp->dev = &pdev->dev;
	qp->bcms = desc->bcms;
	qp->num_bcms = desc->num_bcms;

	qp->num_voters = desc->num_voters;
	qp->voters = devm_kcalloc(&pdev->dev, qp->num_voters,
				  sizeof(*qp->voters), GFP_KERNEL);

	if (!qp->voters)
		return -ENOMEM;

	for (i = 0; i < qp->num_voters; i++) {
		qp->voters[i] = of_bcm_voter_get(qp->dev, desc->voters[i]);
		if (IS_ERR(qp->voters[i]))
			return PTR_ERR(qp->voters[i]);
	}

	qp->regmap = qcom_icc_rpmh_map(pdev, desc);
	if (IS_ERR(qp->regmap))
		return PTR_ERR(qp->regmap);

	qp->num_clks = devm_clk_bulk_get_all(qp->dev, &qp->clks);
	if (qp->num_clks < 0)
		return qp->num_clks;

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(&pdev->dev, "error adding interconnect provider\n");
		return ret;
	}

	for (i = 0; i < qp->num_bcms; i++)
		qcom_icc_bcm_init(qp->bcms[i], &pdev->dev);

	ret = enable_qos_deps(qp);
	if (ret)
		goto provider_del;

	for (i = 0; i < num_nodes; i++) {
		size_t j;

		if (!qnodes[i])
			continue;

		qnodes[i]->regmap = dev_get_regmap(qp->dev, NULL);

		node = icc_node_create(qnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			dev_err(&pdev->dev, "error creating node %d\n", ret);
			goto err;
		}

		if (qnodes[i]->qosbox) {
			qnodes[i]->noc_ops->set_qos(qnodes[i]);
			qnodes[i]->qosbox->initialized = true;
		}

		node->name = qnodes[i]->name;
		node->data = qnodes[i];
		icc_node_add(node, provider);

		dev_dbg(&pdev->dev, "registered node %pK %s %d\n", node,
			qnodes[i]->name, node->id);

		/* populate links */
		for (j = 0; j < qnodes[i]->num_links; j++)
			icc_link_create(node, qnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	disable_qos_deps(qp);

	platform_set_drvdata(pdev, qp);

	provider->set = qcom_icc_set;
	provider->aggregate = qcom_icc_aggregate;

	qcom_icc_debug_register(provider);

	mutex_lock(&probe_list_lock);
	list_add_tail(&qp->probe_list, &qnoc_probe_list);
	mutex_unlock(&probe_list_lock);

	return ret;
err:
	list_for_each_entry(node, &provider->nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}

	clk_bulk_disable_unprepare(qp->num_clks, qp->clks);
	clk_bulk_put_all(qp->num_clks, qp->clks);
provider_del:
	icc_provider_del(provider);

	return ret;
}
EXPORT_SYMBOL(qcom_icc_rpmh_probe);

int qcom_icc_rpmh_remove(struct platform_device *pdev)
{
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	struct icc_provider *provider = &qp->provider;
	struct icc_node *n;

	qcom_icc_debug_unregister(provider);

	list_for_each_entry(n, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}

	clk_bulk_put_all(qp->num_clks, qp->clks);

	return icc_provider_del(provider);
}
EXPORT_SYMBOL(qcom_icc_rpmh_remove);

void qcom_icc_rpmh_sync_state(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	const struct of_device_id *oft = dev->driver->of_match_table;
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	struct qcom_icc_bcm *bcm;
	struct bcm_voter *voter;
	static int probe_count;
	int num_providers;

	for (num_providers = 0; oft[num_providers].data; num_providers++)
		;

	mutex_lock(&probe_list_lock);
	probe_count++;

	if (probe_count < num_providers) {
		mutex_unlock(&probe_list_lock);
		return;
	}

	list_for_each_entry(qp, &qnoc_probe_list, probe_list) {
		int i;

		for (i = 0; i < qp->num_voters; i++)
			qcom_icc_bcm_voter_clear_init(qp->voters[i]);

		for (i = 0; i < qp->num_bcms; i++) {
			bcm = qp->bcms[i];
			if (bcm->keepalive || bcm->keepalive_early) {
				bcm->keepalive_early = false;

				voter = qp->voters[bcm->voter_idx];
				qcom_icc_bcm_voter_add(voter, bcm);
				qcom_icc_bcm_voter_commit(voter);
			}
		}
	}

	mutex_unlock(&probe_list_lock);
}
EXPORT_SYMBOL(qcom_icc_rpmh_sync_state);

MODULE_LICENSE("GPL v2");
