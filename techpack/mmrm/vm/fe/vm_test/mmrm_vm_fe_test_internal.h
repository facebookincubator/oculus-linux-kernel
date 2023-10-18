/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef TEST_MMRM_TEST_INTERNAL_H_
#define TEST_MMRM_TEST_INTERNAL_H_

#include <linux/platform_device.h>
#include <linux/soc/qcom/msm_mmrm.h>

#define MMRM_SYSFS_ENTRY_MAX_LEN     PAGE_SIZE

struct mmrm_test_desc {
	struct mmrm_test_clk_client  *clk_client;
	u32 clk_rate_id;
};

#define MMRM_SYSFS_ENTRY_MAX_LEN     PAGE_SIZE

enum mmrm_vdd_level {
	MMRM_TEST_VDD_LEVEL_LOW_SVS=0,
	MMRM_TEST_VDD_LEVEL_SVS,
	MMRM_TEST_VDD_LEVEL_SVS_L1,
	MMRM_TEST_VDD_LEVEL_NOM,
	MMRM_TEST_VDD_LEVEL_TURBO,
	MMRM_TEST_VDD_LEVEL_MAX
};

struct clock_rate {
	const char *name;
	u32   domain;
	u32   id;
	u32   clk_rates[MMRM_TEST_VDD_LEVEL_MAX];
};

typedef struct test_case_info_s {
	const char name[MMRM_CLK_CLIENT_NAME_SIZE];
	int  vdd_level;
	u32 flags;
	u32 num_hw_blocks;
	u32 client_domain;
	u32 client_id;
	u32 clk_rate[MMRM_TEST_VDD_LEVEL_MAX];
	struct mmrm_client *client;
} test_case_info_t;

extern test_case_info_t  *kalama_testcases[];
extern int kalama_testcases_count;

extern test_case_info_t *kalama_cornercase_testcases [];
extern int kalama_cornercase_testcases_count;

void mmrm_vm_fe_client_tests(struct platform_device *pdev);
void test_mmrm_single_client_cases(struct platform_device *pdev,
	int index, int count);
void test_mmrm_concurrent_client_cases(struct platform_device *pdev,
	test_case_info_t **testcases, int count);
struct clock_rate *find_clk_by_name(const char *name);
struct clock_rate *get_nth_clock(int nth);
void test_mmrm_switch_volt_corner_client_testcases(struct platform_device *pdev,
	test_case_info_t **testcases, int count);
int get_clock_count(void);
void mmrm_vm_fe_client_register_tests(struct platform_device *pdev);

#endif  // TEST_MMRM_TEST_INTERNAL_H_
