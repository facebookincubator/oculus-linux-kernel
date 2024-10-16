/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mmrm_test: " fmt

#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/timekeeping.h>

#include "mmrm_vm_debug.h"
#include "mmrm_vm_fe_test_internal.h"

#define MMRM_TEST_MAX_CLK_CLIENTS 30
#define MMRM_TEST_NUM_CASES 3

enum mmrm_test_result {
	TEST_MMRM_SUCCESS = 0,
	TEST_MMRM_FAIL_REGISTER,
	TEST_MMRM_FAIL_SETVALUE,
	TEST_MMRM_FAIL_CLKGET,
};

struct mmrm_test_clk_client {
	struct mmrm_clk_client_desc clk_client_desc;
	unsigned long clk_rate[MMRM_TEST_VDD_LEVEL_MAX];
	struct mmrm_client *client;
};

static int test_mmrm_client_callback(struct mmrm_client_notifier_data *notifier_data)
{
	// TODO: Test callback here
	return 0;
}

static struct mmrm_client *test_mmrm_vm_fe_client_register(struct mmrm_client_desc *desc)
{
	struct mmrm_client *client;
	u64 kt1, kt2;

	if (!desc) {
		d_mpr_e("%s: Invalid input\n", __func__);
		return NULL;
	}

	d_mpr_h("%s: domain(%d) cid(%d) name(%s) type(%d) pri(%d)\n",
		__func__,
		desc->client_info.desc.client_domain,
		desc->client_info.desc.client_id,
		desc->client_info.desc.name,
		desc->client_type,
		desc->priority);

	d_mpr_w("%s: Registering mmrm client %s\n", __func__, desc->client_info.desc.name);

	kt1 = ktime_get_ns();
	client = mmrm_client_register(desc);
	kt2 = ktime_get_ns();
	d_mpr_h("%s: time interval: %lu\n", __func__, kt2 - kt1);

	if (client == NULL) {
		d_mpr_e("%s: Failed to register mmrm client %s\n",
			__func__,
			desc->client_info.desc.name);
		return NULL;
	}
	return client;
}

static int test_mmrm_vm_fe_client_deregister(struct mmrm_client *client)
{
	int rc;

	if (!client) {
		d_mpr_e("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	d_mpr_h("%s: cuid(%d) Deregistering mmrm client\n", __func__, client->client_uid);
	rc = mmrm_client_deregister(client);
	if (rc != 0) {
		d_mpr_e("%s: cuid(%d) Failed to deregister mmrm client with %d\n",
			__func__,
			client->client_uid,
			rc);
		return rc;
	}
	return rc;
}

static int test_mmrm_vm_fe_client_set_value(
	struct mmrm_client *client, struct mmrm_client_data *client_data, unsigned long val)
{
	int rc;
	u64 kt1, kt2;

	if (!client || !client_data) {
		d_mpr_e("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	d_mpr_h("%s: Setting value(%d) for mmrm client\n",
		__func__,
		val);
	kt1 = ktime_get_ns();
	rc = mmrm_client_set_value(client, client_data, val);
	kt2 = ktime_get_ns();

	if (rc != 0) {
		d_mpr_e("%s: Failed to set value(%d) for mmrm client with %d\n",
			__func__,
			val,
			rc);
		return rc;
	}
	return rc;
}

static int test_mmrm_vm_fe_client_get_value(struct mmrm_client *client, struct mmrm_client_res_value *val)
{
	int rc;

	if (!client || !val) {
		d_mpr_e("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	rc = mmrm_client_get_value(client, val);
	if (rc != 0) {
		d_mpr_e("%s: Failed to get value for mmrm client with %d\n",
			__func__,
			rc);
		return rc;
	}
	d_mpr_h("%s: min(%d) cur(%d) max(%d)\n",
		__func__,
		val->min,
		val->cur,
		val->max);
	return rc;
}

void mmrm_vm_fe_client_tests(struct platform_device *pdev)
{
	struct mmrm_client *client; // mmrm client
	struct mmrm_client_data client_data; // mmrm client data
	int level;
	unsigned long val;
	struct clock_rate *p_clk_res;
	struct mmrm_clk_client_desc  *clk_desc;
	struct mmrm_client_desc desc;
	struct mmrm_client_res_value res_val;

	int i, pass_count, count;
	int rc = 0;

	count = get_clock_count();

	d_mpr_w("%s: Running individual client tests : %d\n", __func__, count);

	// Run nominal test for each individual clock source
	for (i = 0, pass_count = 0; i < count; i++) {
		// Create callback used to pass resource data to client
		struct mmrm_client_notifier_data notifier_data = {
			MMRM_CLIENT_RESOURCE_VALUE_CHANGE, // cb_type
			{{0, 0}}, // cb_data (old_val, new_val)
			NULL}; // pvt_data

		// Create client descriptor
		p_clk_res = get_nth_clock(i);

		desc.client_type = MMRM_CLIENT_CLOCK;
		desc.priority = MMRM_CLIENT_PRIOR_HIGH;
		desc.pvt_data = notifier_data.pvt_data;
		desc.notifier_callback_fn = test_mmrm_client_callback;

		clk_desc = &desc.client_info.desc;
		clk_desc->client_domain = p_clk_res->domain;
		clk_desc->client_id = p_clk_res->id;
		strlcpy((char *)clk_desc->name, p_clk_res->name, sizeof(clk_desc->name));

		// Register client
		client = test_mmrm_vm_fe_client_register(&desc);
		if (client == NULL) {
			rc = -EINVAL;
			d_mpr_e("%s: client register failed\n", __func__);
			goto err_register;
		}
		// Set values (Use reserve only)
		client_data = (struct mmrm_client_data){
			1,
			MMRM_CLIENT_DATA_FLAG_RESERVE_ONLY
		};

		for (level = 0; level < MMRM_TEST_VDD_LEVEL_MAX; level++) {
			val = p_clk_res->clk_rates[level];
			rc = test_mmrm_vm_fe_client_set_value(client, &client_data, val);
			if (rc != 0) {
				d_mpr_e("%s: client set value failed\n", __func__);
				goto err_setval;
			}
		}

		// Get value
		rc = test_mmrm_vm_fe_client_get_value(client, &res_val);
		if (rc != 0) {
			d_mpr_e("%s: client get value failed\n", __func__);
			goto err_getval;
		}

		d_mpr_h("%s: min:%d max:%d cur:%d\n", __func__, res_val.min, res_val.max, res_val.cur);

	err_setval:
	err_getval:

		// Reset clk rate
		test_mmrm_vm_fe_client_set_value(client, &client_data, 0);

	err_register:

		if (rc == 0)
			pass_count++;
	}

	d_mpr_w("%s: Finish individual client tests (pass / total): (%d / %d)\n",
		__func__, pass_count, count);
}

void mmrm_vm_fe_client_register_tests(struct platform_device *pdev)
{
	struct mmrm_client *client; // mmrm client
	struct clock_rate *p_clk_res;
	struct mmrm_clk_client_desc  *clk_desc;
	struct mmrm_client_desc desc;

	d_mpr_h("%s:  client register test\n", __func__);

	// Run nominal test for each individual clock source
	{
		// Create callback used to pass resource data to client
		struct mmrm_client_notifier_data notifier_data = {
			MMRM_CLIENT_RESOURCE_VALUE_CHANGE, // cb_type
			{{0, 0}}, // cb_data (old_val, new_val)
			NULL}; // pvt_data

		// Create client descriptor
		p_clk_res = get_nth_clock(0);

		desc.client_type = MMRM_CLIENT_CLOCK;
		desc.priority = MMRM_CLIENT_PRIOR_HIGH;
		desc.pvt_data = notifier_data.pvt_data;
		desc.notifier_callback_fn = test_mmrm_client_callback;

		clk_desc = &desc.client_info.desc;
		clk_desc->client_domain = p_clk_res->domain;
		clk_desc->client_id = p_clk_res->id;
		strlcpy((char *)clk_desc->name, p_clk_res->name, sizeof(clk_desc->name));

		// Register client
		client = test_mmrm_vm_fe_client_register(&desc);
		if (client == NULL) {
			d_mpr_e("%s: client register fails\n", __func__);
		}
		else
			d_mpr_w("%s: client register successful\n", __func__);
	}
}

// for camera ife/ipe/bps at nom
// all camera +cvp at nom
// all camera +cvp + mdss_mdp at nom
// all camera + cvp +mdss_mdp +video at nom
// all camera at nom + mdp/cvp/video svsl1
// mdp at svsl1 + video at nom : voltage corner scaling

// mdp at svsl1 + video at svsl1 + cvp at svsl1 + camera at nom


// for camera ife/ipe/bps at nom
//
static test_case_info_t test_case_1[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// all camera +cvp at nom
//
static test_case_info_t test_case_4[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_jpeg_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_icp_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cphy_rx_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi2phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi4phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi5phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_cci_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cci_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_slow_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_fast_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};


// all camera +cvp + mdss_mdp at nom
//
static test_case_info_t test_case_5[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_jpeg_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_icp_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cphy_rx_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi2phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi4phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi5phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_cci_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cci_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_slow_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_fast_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// all camera + cvp +mdss_mdp +video at nom
//
static test_case_info_t test_case_6[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_jpeg_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_icp_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cphy_rx_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi2phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi4phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi5phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_cci_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cci_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_slow_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_fast_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// all camera at nom
//
static test_case_info_t test_case_7[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_jpeg_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_icp_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cphy_rx_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi2phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi4phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi5phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_cci_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cci_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_slow_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_fast_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

//	ife0, ife1 (lowsvs) + ipe (svs) + bps (nom)
//	ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
//	ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
//	ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
//	ife0, ife1, ife2 (svs) + ipe (nom) + bps (nom) + sbi (svs)
//	ife0, ife1 (svs) , ife2 (lowsvs) +
//	sfe0 (svs) + sfe1(svs) + ipe (nom) + bps (nom) + sbi (svs)
//	ife0, ife1 (svs) + ipe (nom) + bps (nom) + sbi (svs)
//	ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
//	ife0, ife1 (lowsvs) + ipe (svs) + bps (nom)

static test_case_info_t test_case_11[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

//		ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
static test_case_info_t test_case_12[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

//		ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
static test_case_info_t test_case_13[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};


//		ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
static test_case_info_t test_case_14[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

//		ife0, ife1, ife2 (svs) + ipe (nom) + bps (nom) + sbi (svs)
static test_case_info_t test_case_15[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

//		ife0, ife1 (svs) , ife2 (lowsvs) + sfe0 (svs) + sfe1(svs) + ipe (nom) +
//		bps (nom) + sbi (svs)
static test_case_info_t test_case_16[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_sfe_0_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_sfe_1_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

//		ife0, ife1 (svs) + ipe (nom) + bps (nom) + sbi (svs)
static test_case_info_t test_case_17[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

//		ife0, ife1 (lowsvs) + ipe (nom) + bps (nom) + sbi (lowsvs)
static test_case_info_t test_case_18[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// throttle video
// bps(nom) + ipe(nom) +sfe0(nom) + sfe1(nom) +camnoc(nom) + ife0(nom) + csid0(nom)+ ife1(nom) + csid1(nom) + ife2(svs)
//
//
static test_case_info_t test_case_20[] = {
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_sfe_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_SVS},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// throttle ipe
// bps(nom) + ipe(nom) +sfe0(nom) + sfe1(nom) +camnoc(nom) + ife0(nom) + csid0(nom)+ ife1(nom) + csid1(nom) + ife2(svs)
//
//

static test_case_info_t test_case_21[] = {
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_sfe_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_SVS_L1},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_SVS_L1},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// Reinstate throttled client. Moved below clients to LOW SVS to make sufficient available power
// for throttled client to reinstate
static test_case_info_t test_case_22[] = {
	{"video_cc_mvs1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"disp_cc_mdss_mdp_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_sfe_0_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_sfe_1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},

	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// all camera +cam_cc_csid at nom
//

static test_case_info_t test_case_9[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 0, 3},

	{"cam_cc_jpeg_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_icp_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cphy_rx_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi2phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi4phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi5phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_cci_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cci_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_slow_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_fast_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

// all camera at nom + cam_cc_csid
//
static test_case_info_t test_case_10[] = {
	{"cam_cc_ife_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_2_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_sfe_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ife_lite_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_ipe_nps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_bps_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 0, 3},

	{"cam_cc_jpeg_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_camnoc_axi_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_icp_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cphy_rx_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_csi0phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi1phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi2phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi3phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi4phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_csi5phytimer_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"cam_cc_cci_0_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_cci_1_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_slow_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},
	{"cam_cc_fast_ahb_clk_src", MMRM_TEST_VDD_LEVEL_NOM},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

test_case_info_t  *kalama_testcases[] = {
	test_case_1,
	test_case_4,
	test_case_5,
	test_case_6,
	test_case_7,
	test_case_9,
	test_case_10,
	test_case_11,
	test_case_12,
	test_case_13,
	test_case_14,
	test_case_15,
	test_case_16,
	test_case_17,
	test_case_18,
	test_case_20,
	test_case_21,
	test_case_22,
};

int kalama_testcases_count = sizeof(kalama_testcases)/sizeof(kalama_testcases[0]);

static test_case_info_t cornercases_1 [] = {
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 1, 3},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 1, 2},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 1, 1},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 1, 2},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 1, 3},

	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

static test_case_info_t cornercases_2 [] = {
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS, 1, 3},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_LOW_SVS, 1, 2},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_SVS_L1, 1, 1},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 1, 2},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_NOM, 1, 3},
	{"cam_cc_csid_clk_src", MMRM_TEST_VDD_LEVEL_SVS_L1, 1, 1},
	{"", MMRM_TEST_VDD_LEVEL_MAX}
};

test_case_info_t *kalama_cornercase_testcases [] = {
	cornercases_1,
	cornercases_2,
};

int kalama_cornercase_testcases_count = sizeof(kalama_cornercase_testcases)/sizeof(kalama_cornercase_testcases[0]);

int test_mmrm_testcase_client_register(struct platform_device *pdev,
	test_case_info_t *pcase)
{
	int rc = TEST_MMRM_SUCCESS;
	// Create client descriptor
	struct mmrm_client_desc desc = {
		MMRM_CLIENT_CLOCK,          // client type
		{},                         // clock client descriptor
		MMRM_CLIENT_PRIOR_HIGH,     // client priority
		NULL,                       // pvt_data
		test_mmrm_client_callback   // callback fn
	};

	desc.client_info.desc.client_domain = pcase->client_domain;
	desc.client_info.desc.client_id = pcase->client_id;
	strlcpy((char *)(desc.client_info.desc.name), pcase->name,
		MMRM_CLK_CLIENT_NAME_SIZE);

	// Register client
	pcase->client = test_mmrm_vm_fe_client_register(&desc);

	return rc;
}

int test_mmrm_run_one_case(struct platform_device *pdev,
	test_case_info_t *pcase)
{
	struct mmrm_client_data    client_data;
	unsigned long val;
	test_case_info_t *p = pcase;
	int rc = TEST_MMRM_SUCCESS;

	client_data = (struct mmrm_client_data){0, MMRM_CLIENT_DATA_FLAG_RESERVE_ONLY};

	while (p->vdd_level != MMRM_TEST_VDD_LEVEL_MAX) {
		val = p->clk_rate[p->vdd_level];
		rc = test_mmrm_testcase_client_register(pdev, p);
		if ((rc != TEST_MMRM_SUCCESS) || (IS_ERR_OR_NULL(p->client))) {
			d_mpr_e("%s: client(%s) fail register\n", __func__,
				p->name);
			rc = -TEST_MMRM_FAIL_REGISTER;
			break;
		}

		if (p->num_hw_blocks == 0) {
			client_data.num_hw_blocks = 1;
		} else {
			client_data.num_hw_blocks = p->num_hw_blocks;
		}

		d_mpr_h("%s: domain:%d  csid:%d num_hw_block:%d\n",
			__func__,
			p->client_domain,
			p->client_id,
			client_data.num_hw_blocks);

		if (test_mmrm_vm_fe_client_set_value(p->client, &client_data, val) != 0) {
			rc = -TEST_MMRM_FAIL_SETVALUE;
			break;
		}

		p++;
	}

	p = pcase;
	while (p->vdd_level != MMRM_TEST_VDD_LEVEL_MAX) {
		if (!IS_ERR_OR_NULL(p->client)) {
			test_mmrm_vm_fe_client_set_value(p->client, &client_data, 0);
			test_mmrm_vm_fe_client_deregister(p->client);
		}
		p++;
	}

	return rc;
}

int test_mmrm_populate_testcase(struct platform_device *pdev,
	test_case_info_t **pcase, int count)
{
	int i;
	test_case_info_t **p = pcase, *ptr;
	struct clock_rate *p_clk_rate;

	if (pcase[0]->client_id != 0)
		return 0;

	for (i=0; i < count; i++, p++) 	{
		ptr = *p;
		while (ptr->vdd_level != MMRM_TEST_VDD_LEVEL_MAX) {
			p_clk_rate = find_clk_by_name(ptr->name);
			if (p_clk_rate != NULL) {
				ptr->client_domain = p_clk_rate->domain;
				ptr->client_id = p_clk_rate->id;
				memcpy(ptr->clk_rate, p_clk_rate->clk_rates, sizeof(ptr->clk_rate));

				if (ptr->num_hw_blocks == 0)
					ptr->num_hw_blocks = 1;
			}
			ptr++;
		}
	}
	return i;
}

void test_mmrm_concurrent_client_cases(struct platform_device *pdev,
	test_case_info_t **testcases, int count)
{
	test_case_info_t **p = testcases;
	int i;
	int size, rc, pass = 0;
	int *result_ptr;

	d_mpr_h("%s: Started\n", __func__);

	size = sizeof(int) * count;

	test_mmrm_populate_testcase(pdev, testcases, count);

	result_ptr = kzalloc(size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(result_ptr)) {
		d_mpr_e("%s: failed to allocate memory for concurrent client test\n",
			__func__);
		goto err_fail_alloc_result_ptr;
	}

	p = testcases;
	for (i = 0; i < count; i++, p++) {
		d_mpr_e("%s: testcase: %d -----\n", __func__, i);
		rc = test_mmrm_run_one_case(pdev, *p);
		result_ptr[i] = rc;
		if (rc == TEST_MMRM_SUCCESS)
			pass++;
	}

	d_mpr_w("%s: Finish concurrent client tests (pass / total): (%d / %d)\n",
			__func__, pass, count);

	for (i = 0; i < count; i++) {
		if (result_ptr[i] != TEST_MMRM_SUCCESS)
			d_mpr_w("%s: Failed client test# %d reason %d\n",
				__func__, i, result_ptr[i]);
	}
	kfree(result_ptr);

err_fail_alloc_result_ptr:
	;

}

void test_mmrm_switch_volt_corner_client_testcases(struct platform_device *pdev,
	test_case_info_t **testcases, int count)
{
	test_case_info_t **p = testcases;
	int i;
	int size, rc, pass = 0;
	int *result_ptr;

	d_mpr_h("%s: Started\n", __func__);

	size = sizeof(int) * count;

	test_mmrm_populate_testcase(pdev, testcases, count);

	result_ptr = kzalloc(size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(result_ptr)) {
		d_mpr_h("%s: failed to allocate memory for concurrent client test\n",
			__func__);
		goto err_fail_alloc_result_ptr;
	}

	p = testcases;
	for (i = 0; i < count; i++, p++) {
		d_mpr_h("%s: switch volt corner testcase: %d -----\n", __func__, i);
		rc = test_mmrm_run_one_case(pdev, *p);
		result_ptr[i] = rc;
		if (rc == TEST_MMRM_SUCCESS)
			pass++;
	}

	d_mpr_w("%s: Finish switch volt corner client tests (pass / total): (%d / %d)\n",
			__func__, pass, count);

	for (i = 0; i < count; i++) {
		if (result_ptr[i] != TEST_MMRM_SUCCESS)
			d_mpr_w("%s: Failed client test# %d reason %d\n",
				__func__, i, result_ptr[i]);
	}
	kfree(result_ptr);

err_fail_alloc_result_ptr:
	;
}

