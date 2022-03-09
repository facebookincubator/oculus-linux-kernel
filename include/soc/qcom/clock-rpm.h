/* Copyright (c) 2010-2015, 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_RPM_H
#define __ARCH_ARM_MACH_MSM_CLOCK_RPM_H

#include <linux/clk/msm-clk-provider.h>
#include <soc/qcom/rpm-smd.h>

#define RPM_SMD_KEY_RATE	0x007A484B
#define RPM_SMD_KEY_ENABLE	0x62616E45
#define RPM_SMD_KEY_STATE	0x54415453

#define RPM_CLK_BUFFER_A_REQ			0x616B6C63
#define RPM_KEY_SOFTWARE_ENABLE			0x6E657773
#define RPM_KEY_PIN_CTRL_CLK_BUFFER_ENABLE_KEY	0x62636370

struct clk_ops;
struct clk_rpmrs_data;
extern const struct clk_ops clk_ops_rpm;
extern const struct clk_ops clk_ops_rpm_branch;

struct rpm_clk {
	int rpm_res_type;
	int rpm_key;
	int rpm_clk_id;
	const int rpm_status_id;
	bool active_only;
	bool enabled;
	bool branch; /* true: RPM only accepts 1 for ON and 0 for OFF */
	struct clk_rpmrs_data *rpmrs_data;
	struct rpm_clk *peer;
	struct clk c;
	uint32_t *last_active_set_vote;
	uint32_t *last_sleep_set_vote;
};

static inline struct rpm_clk *to_rpm_clk(struct clk *clk)
{
	return container_of(clk, struct rpm_clk, c);
}

/*
 * RPM scaling enable function used for target that has an RPM resource for
 * rpm clock scaling enable.
 */
int enable_rpm_scaling(void);

int vote_bimc(struct rpm_clk *r, uint32_t value);

extern struct clk_rpmrs_data clk_rpmrs_data_smd;

/*
 * A note on name##last_{active,sleep}_set_vote below:
 * We track the last active and sleep set votes across both
 * active-only and active+sleep set clocks. We use the same
 * tracking variables for both clocks in order to keep both
 * updated about the last vote irrespective of which clock
 * actually made the request. This is the only way to allow
 * optimizations that prevent duplicate requests from being sent
 * to the RPM. Separate tracking does not work since it is not
 * possible to know if the peer's last request was actually sent
 * to the RPM.
 */

#define __DEFINE_CLK_RPM(name, active, type, r_id, stat_id, dep, key, \
				rpmrsdata) \
	static struct rpm_clk active; \
	static uint32_t name##last_active_set_vote; \
	static uint32_t name##last_sleep_set_vote; \
	static struct rpm_clk name = { \
		.rpm_res_type = (type), \
		.rpm_clk_id = (r_id), \
		.rpm_status_id = (stat_id), \
		.rpm_key = (key), \
		.peer = &active, \
		.rpmrs_data = (rpmrsdata),\
		.last_active_set_vote = &name##last_active_set_vote, \
		.last_sleep_set_vote = &name##last_sleep_set_vote, \
		.c = { \
			.ops = &clk_ops_rpm, \
			.dbg_name = #name, \
			CLK_INIT(name.c), \
			.depends = dep, \
		}, \
	}; \
	static struct rpm_clk active = { \
		.rpm_res_type = (type), \
		.rpm_clk_id = (r_id), \
		.rpm_status_id = (stat_id), \
		.rpm_key = (key), \
		.peer = &name, \
		.active_only = true, \
		.rpmrs_data = (rpmrsdata),\
		.last_active_set_vote = &name##last_active_set_vote, \
		.last_sleep_set_vote = &name##last_sleep_set_vote, \
		.c = { \
			.ops = &clk_ops_rpm, \
			.dbg_name = #active, \
			CLK_INIT(active.c), \
			.depends = dep, \
		}, \
	} \

#define __DEFINE_CLK_RPM_BRANCH(name, active, type, r_id, stat_id, r, \
					key, rpmrsdata) \
	static struct rpm_clk active; \
	static uint32_t name##last_active_set_vote; \
	static uint32_t name##last_sleep_set_vote; \
	static struct rpm_clk name = { \
		.rpm_res_type = (type), \
		.rpm_clk_id = (r_id), \
		.rpm_status_id = (stat_id), \
		.rpm_key = (key), \
		.peer = &active, \
		.branch = true, \
		.rpmrs_data = (rpmrsdata),\
		.last_active_set_vote = &name##last_active_set_vote, \
		.last_sleep_set_vote = &name##last_sleep_set_vote, \
		.c = { \
			.ops = &clk_ops_rpm_branch, \
			.dbg_name = #name, \
			.rate = (r), \
			CLK_INIT(name.c), \
		}, \
	}; \
	static struct rpm_clk active = { \
		.rpm_res_type = (type), \
		.rpm_clk_id = (r_id), \
		.rpm_status_id = (stat_id), \
		.rpm_key = (key), \
		.peer = &name, \
		.active_only = true, \
		.branch = true, \
		.rpmrs_data = (rpmrsdata),\
		.last_active_set_vote = &name##last_active_set_vote, \
		.last_sleep_set_vote = &name##last_sleep_set_vote, \
		.c = { \
			.ops = &clk_ops_rpm_branch, \
			.dbg_name = #active, \
			.rate = (r), \
			CLK_INIT(active.c), \
		}, \
	} \

#define DEFINE_CLK_RPM_SMD(name, active, type, r_id, dep) \
	__DEFINE_CLK_RPM(name, active, type, r_id, 0, dep, \
				RPM_SMD_KEY_RATE, &clk_rpmrs_data_smd)

#define DEFINE_CLK_RPM_SMD_BRANCH(name, active, type, r_id, r) \
	__DEFINE_CLK_RPM_BRANCH(name, active, type, r_id, 0, r, \
				RPM_SMD_KEY_ENABLE, &clk_rpmrs_data_smd)

#define DEFINE_CLK_RPM_SMD_QDSS(name, active, type, r_id) \
	__DEFINE_CLK_RPM(name, active, type, r_id, \
		0, 0, RPM_SMD_KEY_STATE, &clk_rpmrs_data_smd)
/*
 * The RPM XO buffer clock management code aggregates votes for pin-control mode
 * and software mode separately. Software-enable has higher priority over pin-
 * control, and if the software-mode aggregation results in a 'disable', the
 * buffer will be left in pin-control mode if a pin-control vote is in place.
 */
#define DEFINE_CLK_RPM_SMD_XO_BUFFER(name, active, r_id) \
	__DEFINE_CLK_RPM_BRANCH(name, active, RPM_CLK_BUFFER_A_REQ, r_id, 0, \
			1000, RPM_KEY_SOFTWARE_ENABLE, &clk_rpmrs_data_smd)

#define DEFINE_CLK_RPM_SMD_XO_BUFFER_PINCTRL(name, active, r_id) \
	__DEFINE_CLK_RPM_BRANCH(name, active, RPM_CLK_BUFFER_A_REQ, r_id, 0, \
	1000, RPM_KEY_PIN_CTRL_CLK_BUFFER_ENABLE_KEY, &clk_rpmrs_data_smd)
#endif
