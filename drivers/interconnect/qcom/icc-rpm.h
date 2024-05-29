/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __DRIVERS_INTERCONNECT_QCOM_ICC_RPM_H__
#define __DRIVERS_INTERCONNECT_QCOM_ICC_RPM_H__

#include <linux/regmap.h>
#include <soc/qcom/rpm-smd.h>

#define MAX_LINKS		64

#define RPM_BUS_MASTER_REQ		0x73616d62
#define RPM_BUS_SLAVE_REQ		0x766c7362

#define RPM_SLEEP_SET		MSM_RPM_CTX_SLEEP_SET
#define RPM_ACTIVE_SET		MSM_RPM_CTX_ACTIVE_SET
#define RPM_CLK_MAX_LEVEL		INT_MAX
#define RPM_CLK_MIN_LEVEL               19200000

#define DEFAULT_UTIL_FACTOR		100

#define to_qcom_provider(_provider) \
	container_of(_provider, struct qcom_icc_provider, provider)

enum qcom_icc_rpm_slave_field_type {
	RPM_SLAVE_FIELD_BW = 0x00007762,
};

enum qcom_icc_rpm_mas_field_type {
	RPM_MASTER_FIELD_BW = 0x00007762,
	RPM_MASTER_FIELD_BW_T0 = 0x30747762,
	RPM_MASTER_FIELD_BW_T1 = 0x31747762,
	RPM_MASTER_FIELD_BW_T2 = 0x32747762,
};

enum qcom_icc_rpm_context {
	RPM_SLEEP_CXT,
	RPM_ACTIVE_CXT,
	RPM_NUM_CXT
};

/**
 * struct qcom_icc_provider - QTI specific interconnect provider
 * @provider: generic interconnect provider
 * @dev: reference to the NoC device
 * @bus_clks: the clk_bulk_data table of bus clocks
 * @num_clks: the total number of clk_bulk_data entries
 * @bus_clk_cur_rate: current frequency of bus clock
 * @keepalive: flag used to indicate whether a keepalive is required
 * @init: flag to determine when init has completed.
 */
struct qcom_icc_provider {
	struct icc_provider provider;
	struct device *dev;
	struct regmap *regmap;
	struct list_head probe_list;
	struct clk_bulk_data *bus_clks;
	int num_clks;
	u32 util_factor;
	u64 bus_clk_cur_rate[RPM_NUM_CXT];
	bool keepalive;
	bool init;
};

/**
 * struct qcom_icc_node - QTI specific interconnect nodes
 * @name: the node name used in debugfs
 * @id: a unique node identifier
 * @links: an array of nodes where we can go next while traversing
 * @num_links: the total number of @links
 * @channels: num of channels at this node
 * @buswidth: width of the interconnect between a node and the bus (bytes)
 * @last_sum_avg: aggregated average bandwidth from previous aggregation
 * @sum_avg: current sum aggregate value of all avg bw requests
 * @max_peak: current max aggregate value of all peak bw requests
 * @mas_rpm_id: RPM id for devices that are bus masters
 * @slv_rpm_id: RPM id for devices that are bus slaves
 * @dirty: flag used to indicate whether the node needs to be committed
 */
struct qcom_icc_node {
	const char *name;
	u16 id;
	u16 links[MAX_LINKS];
	u16 num_links;
	u16 channels;
	u16 buswidth;
	u64 last_sum_avg[RPM_NUM_CXT];
	u64 sum_avg[RPM_NUM_CXT];
	u64 max_peak[RPM_NUM_CXT];
	int mas_rpm_id;
	int slv_rpm_id;
	struct regmap *regmap;
	struct qcom_icc_qosbox *qosbox;
	const struct qcom_icc_noc_ops *noc_ops;
	bool dirty;
};

struct qcom_icc_desc {
	const struct regmap_config *config;
	struct qcom_icc_node **nodes;
	size_t num_nodes;
};

int qcom_icc_rpm_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
			      u32 peak_bw, u32 *agg_avg, u32 *agg_peak);
int qcom_icc_rpm_set(struct icc_node *src, struct icc_node *dst);
void qcom_icc_rpm_pre_aggregate(struct icc_node *node);

#endif
