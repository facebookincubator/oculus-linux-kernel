// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "qcom-memlat: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/spinlock.h>
#include <trace/hooks/sched.h>
#include <soc/qcom/dcvs.h>
#include <soc/qcom/pmu_lib.h>
#include <linux/scmi_protocol.h>
#include <linux/scmi_memlat.h>
#include "trace-dcvs.h"

#define MAX_MEMLAT_GRPS	NUM_DCVS_HW_TYPES
#define FP_NAME		"memlat_fp"
#define MAX_SPM_WINDOW_SIZE 20U
#define MAX_SPM_FREQ_MAP 2
#define MISS_DELTA_PCT_THRES 30U
#define SPM_FREQ_THRES 2000U
#define L2MISS_RATIO_THRES 150U
#define SPM_CPU_FREQ_IGN 200U

enum common_ev_idx {
	INST_IDX,
	CYC_IDX,
	FE_STALL_IDX,
	BE_STALL_IDX,
	NUM_COMMON_EVS
};

enum grp_ev_idx {
	MISS_IDX,
	WB_IDX,
	ACC_IDX,
	NUM_GRP_EVS
};

enum mon_type {
	SAMPLING_MON	= BIT(0),
	THREADLAT_MON	= BIT(1),
	CPUCP_MON	= BIT(2),
	NUM_MON_TYPES
};

#define SAMPLING_VOTER	max(num_possible_cpus(), 8U)
#define NUM_FP_VOTERS	(SAMPLING_VOTER + 1)

enum memlat_type {
	MEMLAT_DEV,
	MEMLAT_GRP,
	MEMLAT_MON,
	NUM_MEMLAT_TYPES
};

struct memlat_spec {
	enum memlat_type type;
};

struct cpu_ctrs {
	u64				common_ctrs[NUM_COMMON_EVS];
	u64				grp_ctrs[MAX_MEMLAT_GRPS][NUM_GRP_EVS];
};

struct cpu_stats {
	struct cpu_ctrs			prev;
	struct cpu_ctrs			curr;
	struct cpu_ctrs			delta;
	struct qcom_pmu_data		raw_ctrs;
	bool				idle_sample;
	ktime_t				sample_ts;
	ktime_t				last_sample_ts;
	spinlock_t			ctrs_lock;
	u32				freq_mhz;
	u32				fe_stall_pct;
	u32				be_stall_pct;
	u32				ipm[MAX_MEMLAT_GRPS];
	u32				wb_pct[MAX_MEMLAT_GRPS];
	u32				spm[MAX_MEMLAT_GRPS];
	u32				l2miss_ratio[MAX_MEMLAT_GRPS];
};

struct cpufreq_memfreq_map {
	unsigned int			cpufreq_mhz;
	unsigned int			memfreq_khz;
};

/* cur_freq is maintained by sampling algorithm only */
struct memlat_mon {
	struct device			*dev;
	struct memlat_group		*memlat_grp;
	enum mon_type			type;
	u32				cpus_mpidr;
	cpumask_t			cpus;
	struct cpufreq_memfreq_map	*freq_map;
	u32				freq_map_len;
	u32				ipm_ceil;
	u32				fe_stall_floor;
	u32				be_stall_floor;
	u32				freq_scale_pct;
	u32				freq_scale_limit_mhz;
	u32				wb_pct_thres;
	u32				wb_filter_ipm;
	u32				min_freq;
	u32				max_freq;
	u32				mon_min_freq;
	u32				mon_max_freq;
	u32				cur_freq;
	struct kobject			kobj;
	bool				is_compute;
	u32				index;
	u32			enable_spm_voting;
	u64			prev_max_miss;
	u32			spm_thres;
	u32			spm_drop_pct;
	u32			spm_window_size;
	u32			sampled_max_spm[MAX_SPM_WINDOW_SIZE];
	u32			sampled_max_cpu_freq[MAX_SPM_WINDOW_SIZE];
	u32			sampled_spm_idx;
	u32			spm_vote_inc_steps;
	u32			disable_spm_value;
	struct	cpufreq_memfreq_map	spm_freq_map[MAX_SPM_FREQ_MAP];
};

struct memlat_group {
	struct device			*dev;
	struct kobject			*dcvs_kobj;
	enum dcvs_hw_type		hw_type;
	enum dcvs_path_type		sampling_path_type;
	enum dcvs_path_type		threadlat_path_type;
	bool				cpucp_enabled;
	u32				sampling_cur_freq;
	u32				adaptive_cur_freq;
	u32				adaptive_high_freq;
	u32				adaptive_low_freq;
	bool				fp_voting_enabled;
	u32				fp_freq;
	u32				*fp_votes;
	u32				grp_ev_ids[NUM_GRP_EVS];
	struct memlat_mon		*mons;
	u32				num_mons;
	u32				num_inited_mons;
	struct mutex			mons_lock;
	struct kobject			kobj;
};

struct memlat_dev_data {
	struct device			*dev;
	struct kobject			kobj;
	u32				common_ev_ids[NUM_COMMON_EVS];
	struct work_struct		work;
	struct workqueue_struct		*memlat_wq;
	u32				sample_ms;
	struct hrtimer			timer;
	ktime_t				last_update_ts;
	ktime_t				last_jiffy_ts;
	struct memlat_group		*groups[MAX_MEMLAT_GRPS];
	int				num_grps;
	int				num_inited_grps;
	spinlock_t			fp_agg_lock;
	spinlock_t			fp_commit_lock;
	bool				fp_enabled;
	bool				sampling_enabled;
	bool				inited;
/* CPUCP related struct fields */
	const struct scmi_memlat_vendor_ops *memlat_ops;
	struct scmi_protocol_handle	*ph;
	u32				cpucp_sample_ms;
	u32				cpucp_log_level;
};

static struct memlat_dev_data		*memlat_data;
static DEFINE_PER_CPU(struct cpu_stats *, sampling_stats);
static DEFINE_MUTEX(memlat_lock);

struct qcom_memlat_attr {
	struct attribute		attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};

#define to_memlat_attr(_attr) \
	container_of(_attr, struct qcom_memlat_attr, attr)
#define to_memlat_mon(k) container_of(k, struct memlat_mon, kobj)
#define to_memlat_grp(k) container_of(k, struct memlat_group, kobj)

#define MEMLAT_ATTR_RW(_name)						\
static struct qcom_memlat_attr _name =					\
__ATTR(_name, 0644, show_##_name, store_##_name)			\

#define MEMLAT_ATTR_RO(_name)						\
static struct qcom_memlat_attr _name =					\
__ATTR(_name, 0444, show_##_name, NULL)					\

#define show_attr(name)							\
static ssize_t show_##name(struct kobject *kobj,			\
			struct attribute *attr, char *buf)		\
{									\
	struct memlat_mon *mon = to_memlat_mon(kobj);			\
	return scnprintf(buf, PAGE_SIZE, "%u\n", mon->name);		\
}									\

#define store_attr(name, _min, _max)					\
static ssize_t store_##name(struct kobject *kobj,			\
			struct attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	int ret;							\
	unsigned int val;						\
	struct memlat_mon *mon = to_memlat_mon(kobj);			\
	struct memlat_group *grp = mon->memlat_grp;			\
	const struct scmi_memlat_vendor_ops *ops = memlat_data->memlat_ops;	\
	ret = kstrtouint(buf, 10, &val);				\
	if (ret < 0)							\
		return ret;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	mon->name = val;						\
	if (mon->type == CPUCP_MON && ops) {					\
		ret = ops->name(memlat_data->ph, grp->hw_type,		\
				mon->index, mon->name);			\
		if (ret == 0) {							\
			pr_err("failed to set mon tunable :%d\n", ret);	\
			count = 0;					\
		}							\
	}								\
	return count;							\
}									\

#define show_grp_attr(name)						\
static ssize_t show_##name(struct kobject *kobj,			\
			struct attribute *attr, char *buf)		\
{									\
	struct memlat_group *grp = to_memlat_grp(kobj);			\
	return scnprintf(buf, PAGE_SIZE, "%u\n", grp->name);		\
}									\

#define store_grp_attr(name, _min, _max)				\
static ssize_t store_##name(struct kobject *kobj,			\
			struct attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	int ret;							\
	unsigned int val;						\
	struct memlat_group *grp = to_memlat_grp(kobj);			\
	ret = kstrtouint(buf, 10, &val);				\
	if (ret < 0)							\
		return ret;						\
	val = max(val, _min);						\
	val = min(val, _max);						\
	grp->name = val;						\
	return count;							\
}									\

static ssize_t store_min_freq(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned int freq;
	struct memlat_mon *mon = to_memlat_mon(kobj);
	struct memlat_group *grp = mon->memlat_grp;
	const struct scmi_memlat_vendor_ops *ops = memlat_data->memlat_ops;

	ret = kstrtouint(buf, 10, &freq);
	if (ret < 0)
		return ret;
	freq = max(freq, mon->mon_min_freq);
	freq = min(freq, mon->max_freq);
	mon->min_freq = freq;

	if (mon->type == CPUCP_MON && ops) {
		ret = ops->min_freq(memlat_data->ph, grp->hw_type,
				    mon->index, mon->min_freq);
		if (ret < 0) {
			pr_err("failed to set min_freq :%d\n", ret);
			count = 0;
		}
	}

	return count;
}

static ssize_t store_max_freq(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned int freq;
	struct memlat_mon *mon = to_memlat_mon(kobj);
	struct memlat_group *grp = mon->memlat_grp;
	const struct scmi_memlat_vendor_ops *ops = memlat_data->memlat_ops;

	ret = kstrtouint(buf, 10, &freq);
	if (ret < 0)
		return ret;
	freq = max(freq, mon->min_freq);
	freq = min(freq, mon->mon_max_freq);
	mon->max_freq = freq;

	if (mon->type == CPUCP_MON && ops) {
		ret = ops->max_freq(memlat_data->ph, grp->hw_type,
				    mon->index, mon->max_freq);
		if (ret < 0) {
			pr_err("failed to set max_freq :%d\n", ret);
			count = 0;
		}
	}

	return count;
}

static ssize_t show_freq_map(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct memlat_mon *mon = to_memlat_mon(kobj);
	struct cpufreq_memfreq_map *map = mon->freq_map;
	unsigned int cnt = 0;

	cnt += scnprintf(buf, PAGE_SIZE, "CPU freq (MHz)\tMem freq (kHz)\n");

	while (map->cpufreq_mhz && cnt < PAGE_SIZE) {
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "%14u\t%14u\n",
				map->cpufreq_mhz, map->memfreq_khz);
		map++;
	}
	if (cnt < PAGE_SIZE)
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "\n");

	return cnt;
}

static ssize_t show_spm_freq_map(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	struct memlat_mon *mon = to_memlat_mon(kobj);
	unsigned int cnt = 0, i;

	cnt += scnprintf(buf, PAGE_SIZE, "CPU freq (MHz)\tMem freq (kHz)\n");
	for (i = 0; i < MAX_SPM_FREQ_MAP && mon->spm_freq_map[i].cpufreq_mhz; i++)
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "%14u\t%14u\n",
				mon->spm_freq_map[i].cpufreq_mhz, mon->spm_freq_map[i].memfreq_khz);
	if (cnt < PAGE_SIZE)
		cnt += scnprintf(buf + cnt, PAGE_SIZE - cnt, "\n");

	return cnt;
}

#define MIN_SAMPLE_MS	4U
#define MAX_SAMPLE_MS	1000U
static ssize_t store_sample_ms(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;
	val = max(val, MIN_SAMPLE_MS);
	val = min(val, MAX_SAMPLE_MS);

	memlat_data->sample_ms = val;

	return count;
}

static ssize_t show_sample_ms(struct kobject *kobj,
			struct attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", memlat_data->sample_ms);
}

static ssize_t store_cpucp_sample_ms(struct kobject *kobj,
				     struct attribute *attr, const char *buf,
				     size_t count)
{
	int ret, i;
	unsigned int val;
	const struct scmi_memlat_vendor_ops *ops = memlat_data->memlat_ops;
	struct memlat_group *grp;

	if (!ops)
		return -ENODEV;

	for (i = 0; i < MAX_MEMLAT_GRPS; i++) {
		grp = memlat_data->groups[i];
		if (grp->cpucp_enabled)
			break;
	}
	if (i == MAX_MEMLAT_GRPS)
		return count;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;
	val = max(val, MIN_SAMPLE_MS);
	val = min(val, MAX_SAMPLE_MS);

	ret = ops->sample_ms(memlat_data->ph, val);
	if (ret < 0) {
		pr_err("Failed to set cpucp sample ms :%d\n", ret);
		return 0;
	}

	memlat_data->cpucp_sample_ms = val;
	return count;
}

static ssize_t show_cpucp_sample_ms(struct kobject *kobj,
				    struct attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", memlat_data->cpucp_sample_ms);
}

static ssize_t store_cpucp_log_level(struct kobject *kobj,
				     struct attribute *attr, const char *buf,
				     size_t count)
{
	int ret, i;
	unsigned int val;
	const struct scmi_memlat_vendor_ops *ops = memlat_data->memlat_ops;
	struct memlat_group *grp;

	if (!ops)
		return -ENODEV;

	for (i = 0; i < MAX_MEMLAT_GRPS; i++) {
		grp = memlat_data->groups[i];
		if (grp->cpucp_enabled)
			break;
	}
	if (i == MAX_MEMLAT_GRPS)
		return count;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return ret;

	ret = ops->set_log_level(memlat_data->ph, val);
	if (ret < 0) {
		pr_err("failed to configure log_level, ret = %d\n", ret);
		return 0;
	}

	memlat_data->cpucp_log_level = val;
	return count;
}

#define MIN_SPM_WINDOW_SIZE 1U
static ssize_t store_spm_window_size(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	u32 spm_window_size;
	struct memlat_mon *mon = to_memlat_mon(kobj);

	ret = kstrtouint(buf, 10, &spm_window_size);
	if (ret < 0)
		return ret;
	spm_window_size = max(spm_window_size, MIN_SPM_WINDOW_SIZE);
	spm_window_size = min(spm_window_size, MAX_SPM_WINDOW_SIZE);
	mon->spm_window_size = spm_window_size;
	return count;
}

#define MIN_SPM_DROP_PCT 1U
#define MAX_SPM_DROP_PCT 100U
static ssize_t store_spm_drop_pct(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	u32 spm_drop_pct;
	struct memlat_mon *mon = to_memlat_mon(kobj);

	ret = kstrtouint(buf, 10, &spm_drop_pct);
	if (ret < 0)
		return ret;
	spm_drop_pct = max(spm_drop_pct, MIN_SPM_DROP_PCT);
	spm_drop_pct = min(spm_drop_pct, MAX_SPM_DROP_PCT);
	mon->spm_drop_pct = spm_drop_pct;
	mon->disable_spm_value = mon->spm_thres -
				mult_frac(mon->spm_thres, mon->spm_drop_pct, 100);

	return count;
}

#define MIN_SPM_THRES 1U
#define MAX_SPM_THRES 1000U
static ssize_t store_spm_thres(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	int ret;
	u32 spm_thres;
	struct memlat_mon *mon = to_memlat_mon(kobj);

	ret = kstrtouint(buf, 10, &spm_thres);
	if (ret < 0)
		return ret;
	spm_thres = max(spm_thres, MIN_SPM_THRES);
	spm_thres = min(spm_thres, MAX_SPM_THRES);
	mon->spm_thres = spm_thres;
	if (mon->spm_thres < MAX_SPM_THRES)
		mon->enable_spm_voting = 1;
	else
		mon->enable_spm_voting = 0;
	mon->disable_spm_value = mon->spm_thres -
				mult_frac(mon->spm_thres, mon->spm_drop_pct, 100);
	return count;
}

static ssize_t store_spm_freq_map(struct kobject *kobj,
			struct attribute *attr, const char *buf,
			size_t count)
{
	struct memlat_mon *mon  = to_memlat_mon(kobj);
	int ret;
	char  *sptr, *token, *str;
	u32 val, i;

	str = kstrdup(buf, GFP_KERNEL);
	if (!str)
		return -ENOMEM;
	sptr = str;
	for (i = 0; i < MAX_SPM_FREQ_MAP; i++) {
		token = strsep(&sptr, ":");
		if (!token) {
			ret =  -EINVAL;
			goto out;
		}
		ret = kstrtouint(token, 10, &val);
		if (ret < 0) {
			ret =  -EINVAL;
			goto out;
		}
		val = max(val, 0U);
		val = min(val, U32_MAX);
		mon->spm_freq_map[i].cpufreq_mhz = val;
		token = strsep(&sptr, " ");
		if (!token) {
			ret =  -EINVAL;
			goto out;
		}
		ret = kstrtouint(token, 10, &val);
		if (ret < 0) {
			ret =  -EINVAL;
			goto out;
		}
		val = max(val, mon->min_freq);
		val = min(val, mon->mon_max_freq);
		mon->spm_freq_map[i].memfreq_khz = val;
	}
	ret = count;
out:
	kfree(str);
	return ret;
}

static ssize_t show_cpucp_log_level(struct kobject *kobj,
				    struct attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", memlat_data->cpucp_log_level);
}

show_grp_attr(sampling_cur_freq);
show_grp_attr(adaptive_cur_freq);
show_grp_attr(adaptive_high_freq);
store_grp_attr(adaptive_high_freq, 0U, 8000000U);
show_grp_attr(adaptive_low_freq);
store_grp_attr(adaptive_low_freq, 0U, 8000000U);

show_attr(min_freq);
show_attr(max_freq);
show_attr(cur_freq);
show_attr(ipm_ceil);
store_attr(ipm_ceil, 1U, 50000U);
show_attr(fe_stall_floor);
store_attr(fe_stall_floor, 0U, 100U);
show_attr(be_stall_floor);
store_attr(be_stall_floor, 0U, 100U);
show_attr(freq_scale_pct);
store_attr(freq_scale_pct, 0U, 1000U);
show_attr(wb_pct_thres);
store_attr(wb_pct_thres, 0U, 100U);
show_attr(wb_filter_ipm);
store_attr(wb_filter_ipm, 0U, 50000U);
store_attr(freq_scale_limit_mhz, 0U, 5000U);
show_attr(freq_scale_limit_mhz);
show_attr(spm_drop_pct);
show_attr(spm_thres);
show_attr(spm_window_size);

MEMLAT_ATTR_RW(sample_ms);
MEMLAT_ATTR_RW(cpucp_sample_ms);
MEMLAT_ATTR_RW(cpucp_log_level);

MEMLAT_ATTR_RO(sampling_cur_freq);
MEMLAT_ATTR_RO(adaptive_cur_freq);
MEMLAT_ATTR_RW(adaptive_low_freq);
MEMLAT_ATTR_RW(adaptive_high_freq);

MEMLAT_ATTR_RW(min_freq);
MEMLAT_ATTR_RW(max_freq);
MEMLAT_ATTR_RO(freq_map);
MEMLAT_ATTR_RO(cur_freq);
MEMLAT_ATTR_RW(ipm_ceil);
MEMLAT_ATTR_RW(fe_stall_floor);
MEMLAT_ATTR_RW(be_stall_floor);
MEMLAT_ATTR_RW(freq_scale_pct);
MEMLAT_ATTR_RW(wb_pct_thres);
MEMLAT_ATTR_RW(wb_filter_ipm);
MEMLAT_ATTR_RW(freq_scale_limit_mhz);
MEMLAT_ATTR_RW(spm_thres);
MEMLAT_ATTR_RW(spm_drop_pct);
MEMLAT_ATTR_RW(spm_window_size);
MEMLAT_ATTR_RW(spm_freq_map);

static struct attribute *memlat_settings_attr[] = {
	&sample_ms.attr,
	&cpucp_sample_ms.attr,
	&cpucp_log_level.attr,
	NULL,
};

static struct attribute *memlat_grp_attr[] = {
	&sampling_cur_freq.attr,
	&adaptive_cur_freq.attr,
	&adaptive_high_freq.attr,
	&adaptive_low_freq.attr,
	NULL,
};

static struct attribute *memlat_mon_attr[] = {
	&min_freq.attr,
	&max_freq.attr,
	&freq_map.attr,
	&cur_freq.attr,
	&ipm_ceil.attr,
	&fe_stall_floor.attr,
	&be_stall_floor.attr,
	&freq_scale_pct.attr,
	&wb_pct_thres.attr,
	&wb_filter_ipm.attr,
	&freq_scale_limit_mhz.attr,
	&spm_thres.attr,
	&spm_drop_pct.attr,
	&spm_window_size.attr,
	&spm_freq_map.attr,
	NULL,
};

static struct attribute *compute_mon_attr[] = {
	&min_freq.attr,
	&max_freq.attr,
	&freq_map.attr,
	&cur_freq.attr,
	NULL,
};

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct qcom_memlat_attr *memlat_attr = to_memlat_attr(attr);
	ssize_t ret = -EIO;

	if (memlat_attr->show)
		ret = memlat_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct qcom_memlat_attr *memlat_attr = to_memlat_attr(attr);
	ssize_t ret = -EIO;

	if (memlat_attr->store)
		ret = memlat_attr->store(kobj, attr, buf, count);

	return ret;
}

static const struct sysfs_ops memlat_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static struct kobj_type memlat_settings_ktype = {
	.sysfs_ops	= &memlat_sysfs_ops,
	.default_attrs	= memlat_settings_attr,

};

static struct kobj_type memlat_mon_ktype = {
	.sysfs_ops	= &memlat_sysfs_ops,
	.default_attrs	= memlat_mon_attr,

};

static struct kobj_type compute_mon_ktype = {
	.sysfs_ops	= &memlat_sysfs_ops,
	.default_attrs	= compute_mon_attr,

};

static struct kobj_type memlat_grp_ktype = {
	.sysfs_ops	= &memlat_sysfs_ops,
	.default_attrs	= memlat_grp_attr,

};

static u32 cpufreq_to_memfreq(struct memlat_mon *mon, u32 cpu_mhz)
{
	struct cpufreq_memfreq_map *map = mon->freq_map;
	u32 mem_khz = 0;

	if (!map)
		goto out;

	while (map->cpufreq_mhz && map->cpufreq_mhz < cpu_mhz)
		map++;
	if (!map->cpufreq_mhz)
		map--;
	mem_khz = map->memfreq_khz;

out:
	return mem_khz;
}

static void calculate_sampling_stats(void)
{
	int i, grp, cpu, level = 0;
	unsigned long flags;
	struct cpu_stats *stats;
	struct cpu_ctrs *delta;
	struct memlat_group *memlat_grp;
	ktime_t now = ktime_get();
	s64 delta_us, update_us;

	update_us = ktime_us_delta(now, memlat_data->last_update_ts);
	memlat_data->last_update_ts = now;

	local_irq_save(flags);
	for_each_possible_cpu(cpu) {
		stats = per_cpu(sampling_stats, cpu);
		if (level == 0)
			spin_lock(&stats->ctrs_lock);
		else
			spin_lock_nested(&stats->ctrs_lock, level);
		level++;
	}

	for_each_possible_cpu(cpu) {
		stats = per_cpu(sampling_stats, cpu);
		delta = &stats->delta;
		/* use update_us and now to synchronize idle cpus */
		if (stats->idle_sample) {
			delta_us = update_us;
			stats->last_sample_ts = now;
		} else {
			delta_us = ktime_us_delta(stats->sample_ts,
							stats->last_sample_ts);
			stats->last_sample_ts = stats->sample_ts;
		}
		for (i = 0; i < NUM_COMMON_EVS; i++) {
			if (!memlat_data->common_ev_ids[i])
				continue;
			delta->common_ctrs[i] = stats->curr.common_ctrs[i] -
						stats->prev.common_ctrs[i];
		}

		for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
			memlat_grp = memlat_data->groups[grp];
			if (!memlat_grp)
				continue;
			for (i = 0; i < NUM_GRP_EVS; i++) {
				if (!memlat_grp->grp_ev_ids[i])
					continue;
				delta->grp_ctrs[grp][i] =
					    stats->curr.grp_ctrs[grp][i] -
					    stats->prev.grp_ctrs[grp][i];
			}
		}

		stats->freq_mhz = delta->common_ctrs[CYC_IDX] / delta_us;
		if (!memlat_data->common_ev_ids[FE_STALL_IDX])
			stats->fe_stall_pct = 100;
		else
			stats->fe_stall_pct = mult_frac(100,
					       delta->common_ctrs[FE_STALL_IDX],
					       delta->common_ctrs[CYC_IDX]);
		if (!memlat_data->common_ev_ids[BE_STALL_IDX])
			stats->be_stall_pct = 100;
		else
			stats->be_stall_pct = mult_frac(100,
					       delta->common_ctrs[BE_STALL_IDX],
					       delta->common_ctrs[CYC_IDX]);
		for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
			memlat_grp = memlat_data->groups[grp];
			if (!memlat_grp) {
				stats->ipm[grp] = 0;
				stats->wb_pct[grp] = 0;
				continue;
			}
			stats->ipm[grp] = delta->common_ctrs[INST_IDX];
			if (delta->grp_ctrs[grp][MISS_IDX])
				stats->ipm[grp] /=
					delta->grp_ctrs[grp][MISS_IDX];

			if (delta->grp_ctrs[grp][MISS_IDX] > 0)
				stats->l2miss_ratio[grp] = mult_frac(100,
					delta->grp_ctrs[DCVS_L3][MISS_IDX],
					delta->grp_ctrs[grp][MISS_IDX]);
			else
				stats->l2miss_ratio[grp] =
				(delta->grp_ctrs[DCVS_L3][MISS_IDX] * 100);

			stats->spm[grp] = delta->common_ctrs[BE_STALL_IDX];
			if (delta->grp_ctrs[grp][MISS_IDX])
				stats->spm[grp] /=
					delta->grp_ctrs[grp][MISS_IDX];
			else
				stats->spm[grp] = 0;
			if (!memlat_grp->grp_ev_ids[WB_IDX]
					|| !memlat_grp->grp_ev_ids[ACC_IDX])
				stats->wb_pct[grp] = 0;
			else
				stats->wb_pct[grp] = mult_frac(100,
						delta->grp_ctrs[grp][WB_IDX],
						delta->grp_ctrs[grp][ACC_IDX]);

			/* one meas event per memlat_group with group name */
			trace_memlat_dev_meas(dev_name(memlat_grp->dev), cpu,
					delta->common_ctrs[INST_IDX],
					delta->grp_ctrs[grp][MISS_IDX],
					stats->freq_mhz, stats->be_stall_pct,
					stats->wb_pct[grp], stats->ipm[grp],
					stats->fe_stall_pct);

		}
		memcpy(&stats->prev, &stats->curr, sizeof(stats->curr));
	}

	for_each_possible_cpu(cpu) {
		stats = per_cpu(sampling_stats, cpu);
		spin_unlock(&stats->ctrs_lock);
	}
	local_irq_restore(flags);
}

static inline void set_higher_freq(int *max_cpu, int cpu, u32 *max_cpufreq,
			    u32 cpufreq)
{
	if (cpufreq > *max_cpufreq) {
		*max_cpu = cpu;
		*max_cpufreq = cpufreq;
	}
}

static inline void apply_adaptive_freq(struct memlat_group *memlat_grp,
					u32 *max_freq)
{
	u32 prev_freq = memlat_grp->adaptive_cur_freq;

	if (*max_freq < memlat_grp->adaptive_low_freq) {
		*max_freq = memlat_grp->adaptive_low_freq;
		memlat_grp->adaptive_cur_freq = memlat_grp->adaptive_low_freq;
	} else if (*max_freq < memlat_grp->adaptive_high_freq) {
		*max_freq = memlat_grp->adaptive_high_freq;
		memlat_grp->adaptive_cur_freq = memlat_grp->adaptive_high_freq;
	} else
		memlat_grp->adaptive_cur_freq = memlat_grp->adaptive_high_freq;

	if (prev_freq != memlat_grp->adaptive_cur_freq)
		trace_memlat_dev_update(dev_name(memlat_grp->dev),
				0, 0, 0, 0, memlat_grp->adaptive_cur_freq);
}

static void calculate_mon_sampling_freq(struct memlat_mon *mon)
{
	struct cpu_stats *stats;
	int cpu, max_cpu = cpumask_first(&mon->cpus);
	u32 max_memfreq, max_cpufreq = 0, max_max_spm_cpufreq = 0, max_spm_cpufreq = 0;
	u32 max_cpufreq_scaled = 0, ipm_diff, base_vote = 0;
	u32 hw = mon->memlat_grp->hw_type;
	u32 max_spm = 0, avg_spm = 0, min_l2miss_ratio = U32_MAX;
	u64  max_miss = 0;
	u32 miss_delta, miss_delta_pct;
	u32 i, j, vote_idx, spm_max_vote_khz;
	struct cpufreq_memfreq_map *map;

	if (hw >= NUM_DCVS_HW_TYPES)
		return;

	for_each_cpu(cpu, &mon->cpus) {
		stats = per_cpu(sampling_stats, cpu);
		/* these are max of any CPU (for SPM algo) */
		if (stats->l2miss_ratio[hw])
			min_l2miss_ratio = min(stats->l2miss_ratio[hw], min_l2miss_ratio);
		max_miss = max(stats->delta.grp_ctrs[hw][MISS_IDX], max_miss);
		max_spm_cpufreq = max(max_spm_cpufreq, stats->freq_mhz);
		if (mon->is_compute || (stats->wb_pct[hw] >= mon->wb_pct_thres
		    && stats->ipm[hw] <= mon->wb_filter_ipm))
			set_higher_freq(&max_cpu, cpu, &max_cpufreq,
					stats->freq_mhz);
		else if (stats->ipm[hw] <= mon->ipm_ceil) {
			ipm_diff = mon->ipm_ceil - stats->ipm[hw];
			max_cpufreq_scaled = stats->freq_mhz;

			if (mon->enable_spm_voting && stats->freq_mhz >= SPM_CPU_FREQ_IGN)
				max_spm = max(stats->spm[hw], max_spm);

			if (mon->freq_scale_pct && stats->freq_mhz &&
			    (stats->freq_mhz < mon->freq_scale_limit_mhz) &&
			    (stats->be_stall_pct >= mon->be_stall_floor)) {
				max_cpufreq_scaled += (stats->freq_mhz * ipm_diff *
					mon->freq_scale_pct) / (mon->ipm_ceil * 100);
				max_cpufreq_scaled = min(mon->freq_scale_limit_mhz,
							 max_cpufreq_scaled);
			}
			set_higher_freq(&max_cpu, cpu, &max_cpufreq,
					max_cpufreq_scaled);
		}
	}
	max_memfreq = cpufreq_to_memfreq(mon, max_cpufreq);

	base_vote = max_memfreq;
	if (mon->enable_spm_voting) {
		mon->sampled_max_spm[mon->sampled_spm_idx] = max_spm;
		mon->sampled_max_cpu_freq[mon->sampled_spm_idx] = max_spm_cpufreq;
		mon->sampled_spm_idx = (mon->sampled_spm_idx + 1) % mon->spm_window_size;
		for (i = 0; i < mon->spm_window_size; i++) {
			avg_spm += mon->sampled_max_spm[i];
			max_max_spm_cpufreq = max(max_max_spm_cpufreq,
				mon->sampled_max_cpu_freq[i]);
		}
		avg_spm = avg_spm / mon->spm_window_size;
		if (avg_spm >= mon->spm_thres && ((min_l2miss_ratio
			< L2MISS_RATIO_THRES) || max_spm_cpufreq >= SPM_FREQ_THRES))
			mon->spm_vote_inc_steps++;
		else if (avg_spm < mon->disable_spm_value && mon->spm_vote_inc_steps >= 1)
			mon->spm_vote_inc_steps--;

		if (mon->spm_vote_inc_steps && max_miss < mon->prev_max_miss) {
			miss_delta = mon->prev_max_miss - max_miss;
			miss_delta_pct = mult_frac(100, miss_delta, mon->prev_max_miss);
			if (miss_delta_pct >= MISS_DELTA_PCT_THRES)
				mon->spm_vote_inc_steps = 0;
		}
		mon->prev_max_miss = max_miss;
		if (max_spm_cpufreq < SPM_CPU_FREQ_IGN)
			mon->spm_vote_inc_steps = 0;
		if (mon->spm_vote_inc_steps) {
			if (max_max_spm_cpufreq < mon->spm_freq_map[0].cpufreq_mhz)
				spm_max_vote_khz = mon->spm_freq_map[0].memfreq_khz;
			else if (max_max_spm_cpufreq < mon->spm_freq_map[1].cpufreq_mhz)
				spm_max_vote_khz = mon->spm_freq_map[1].memfreq_khz;
			else
				spm_max_vote_khz = mon->max_freq;
		}
	} else
		mon->spm_vote_inc_steps = 0;

	map = mon->freq_map;
	if (mon->spm_vote_inc_steps && max_memfreq < spm_max_vote_khz) {
		for (i = 0; i < mon->freq_map_len && map[i].cpufreq_mhz; i++) {
			if (map[i].memfreq_khz >= max_memfreq)
				break;
		}
		for (j = i; j < mon->freq_map_len && map[j].cpufreq_mhz; j++) {
			if (map[j].memfreq_khz >= spm_max_vote_khz)
				break;
		}
		if (i != mon->freq_map_len && j != mon->freq_map_len) {
			vote_idx = i + mon->spm_vote_inc_steps;
			vote_idx = min(vote_idx, j);
			if (i + mon->spm_vote_inc_steps > j)
				mon->spm_vote_inc_steps = j - i;
			if (vote_idx < mon->freq_map_len && mon->freq_map[vote_idx].memfreq_khz)
				max_memfreq = min(mon->freq_map[vote_idx].memfreq_khz,
								spm_max_vote_khz);
		}
	}

	max_memfreq = max(max_memfreq, mon->min_freq);
	max_memfreq = min(max_memfreq, mon->max_freq);

	if (max_cpufreq || mon->cur_freq != mon->min_freq) {
		stats = per_cpu(sampling_stats, max_cpu);
		trace_memlat_dev_update(dev_name(mon->dev), max_cpu,
				stats->delta.common_ctrs[INST_IDX],
				stats->delta.grp_ctrs[hw][MISS_IDX],
				max_cpufreq, max_memfreq);

		if (mon->enable_spm_voting)
			trace_memlat_spm_update(dev_name(mon->dev),
				max_spm_cpufreq, max_max_spm_cpufreq, base_vote,
				max_memfreq, mon->spm_vote_inc_steps,
				spm_max_vote_khz, avg_spm, min_l2miss_ratio);
	}

	mon->cur_freq = max_memfreq;
}

/*
 * updates fast path votes for "cpu" (i.e. fp_votes index)
 * fp_freqs array length MAX_MEMLAT_GRPS (i.e. one freq per grp)
 */
static void update_memlat_fp_vote(int cpu, u32 *fp_freqs)
{
	struct dcvs_freq voted_freqs[MAX_MEMLAT_GRPS];
	u32 max_freqs[MAX_MEMLAT_GRPS] = { 0 };
	int grp, i, ret;
	unsigned long flags;
	struct memlat_group *memlat_grp;
	u32 commit_mask = 0;

	if (!memlat_data->fp_enabled || cpu > NUM_FP_VOTERS)
		return;

	local_irq_save(flags);
	spin_lock(&memlat_data->fp_agg_lock);
	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp || !memlat_grp->fp_voting_enabled)
			continue;
		memlat_grp->fp_votes[cpu] = fp_freqs[grp];

		/* aggregate across all "cpus" */
		for (i = 0; i < NUM_FP_VOTERS; i++)
			max_freqs[grp] = max(memlat_grp->fp_votes[i],
							max_freqs[grp]);
		if (max_freqs[grp] != memlat_grp->fp_freq)
			commit_mask |= BIT(grp);
	}

	if (!commit_mask) {
		spin_unlock(&memlat_data->fp_agg_lock);
		local_irq_restore(flags);
		return;
	}

	spin_lock_nested(&memlat_data->fp_commit_lock, SINGLE_DEPTH_NESTING);
	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		if (!(commit_mask & BIT(grp)))
			continue;
		memlat_grp = memlat_data->groups[grp];
		memlat_grp->fp_freq = max_freqs[grp];
	}
	spin_unlock(&memlat_data->fp_agg_lock);

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		if (!(commit_mask & BIT(grp)))
			continue;
		voted_freqs[grp].ib = max_freqs[grp];
		voted_freqs[grp].hw_type = grp;
	}
	ret = qcom_dcvs_update_votes(FP_NAME, voted_freqs, commit_mask,
							DCVS_FAST_PATH);
	if (ret < 0)
		pr_err("error updating qcom dcvs fp: %d\n", ret);
	spin_unlock(&memlat_data->fp_commit_lock);
	local_irq_restore(flags);
}

/* sampling path update work */
static void memlat_update_work(struct work_struct *work)
{
	int i, grp, ret;
	struct memlat_group *memlat_grp;
	struct memlat_mon *mon;
	struct dcvs_freq new_freq;
	u32 max_freqs[MAX_MEMLAT_GRPS] = { 0 };

	/* aggregate mons to calculate max freq per memlat_group */
	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp)
			continue;
		for (i = 0; i < memlat_grp->num_inited_mons; i++) {
			mon = &memlat_grp->mons[i];
			if (!mon || !(mon->type & SAMPLING_MON))
				continue;
			calculate_mon_sampling_freq(mon);
			max_freqs[grp] = max(mon->cur_freq, max_freqs[grp]);
		}
		if (memlat_grp->adaptive_high_freq ||
				memlat_grp->adaptive_low_freq ||
				memlat_grp->adaptive_cur_freq)
			apply_adaptive_freq(memlat_grp, &max_freqs[grp]);
	}

	update_memlat_fp_vote(SAMPLING_VOTER, max_freqs);

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp || memlat_grp->sampling_cur_freq == max_freqs[grp] ||
		    memlat_grp->sampling_path_type == NUM_DCVS_PATHS)
			continue;
		memlat_grp->sampling_cur_freq = max_freqs[grp];
		if (memlat_grp->fp_voting_enabled)
			continue;
		new_freq.ib = max_freqs[grp];
		new_freq.ab = 0;
		new_freq.hw_type = grp;
		ret = qcom_dcvs_update_votes(dev_name(memlat_grp->dev),
				&new_freq, 1, memlat_grp->sampling_path_type);
		if (ret < 0)
			dev_err(memlat_grp->dev, "qcom dcvs err: %d\n", ret);
	}
}

static enum hrtimer_restart memlat_hrtimer_handler(struct hrtimer *timer)
{
	calculate_sampling_stats();
	queue_work(memlat_data->memlat_wq, &memlat_data->work);

	return HRTIMER_NORESTART;
}

static const u64 HALF_TICK_NS = (NSEC_PER_SEC / HZ) >> 1;
#define MEMLAT_UPDATE_DELAY (100 * NSEC_PER_USEC)
static void memlat_jiffies_update_cb(void *unused, void *extra)
{
	ktime_t now = ktime_get();
	s64 delta_ns = now - memlat_data->last_jiffy_ts + HALF_TICK_NS;

	if (unlikely(!memlat_data->inited))
		return;

	if (delta_ns > ms_to_ktime(memlat_data->sample_ms)) {
		hrtimer_start(&memlat_data->timer, MEMLAT_UPDATE_DELAY,
						HRTIMER_MODE_REL_PINNED);
		memlat_data->last_jiffy_ts = now;
	}
}

/*
 * Note: must hold stats->ctrs_lock and populate stats->raw_ctrs
 * before calling this API.
 */
static void process_raw_ctrs(struct cpu_stats *stats)
{
	int i, grp, idx;
	struct cpu_ctrs *curr_ctrs = &stats->curr;
	struct qcom_pmu_data *raw_ctrs = &stats->raw_ctrs;
	struct memlat_group *memlat_grp;
	u32 event_id;
	u64 ev_data;

	for (i = 0; i < raw_ctrs->num_evs; i++) {
		event_id = raw_ctrs->event_ids[i];
		ev_data = raw_ctrs->ev_data[i];
		if (!event_id)
			break;

		for (idx = 0; idx < NUM_COMMON_EVS; idx++) {
			if (event_id != memlat_data->common_ev_ids[idx])
				continue;
			curr_ctrs->common_ctrs[idx] = ev_data;
			break;
		}
		if (idx < NUM_COMMON_EVS)
			continue;

		for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
			memlat_grp = memlat_data->groups[grp];
			if (!memlat_grp)
				continue;
			for (idx = 0; idx < NUM_GRP_EVS; idx++) {
				if (event_id != memlat_grp->grp_ev_ids[idx])
					continue;
				curr_ctrs->grp_ctrs[grp][idx] = ev_data;
				break;
			}
		}
	}
}

static void memlat_pmu_idle_cb(struct qcom_pmu_data *data, int cpu, int state)
{
	struct cpu_stats *stats = per_cpu(sampling_stats, cpu);
	unsigned long flags;

	if (unlikely(!memlat_data->inited))
		return;

	spin_lock_irqsave(&stats->ctrs_lock, flags);
	memcpy(&stats->raw_ctrs, data, sizeof(*data));
	process_raw_ctrs(stats);
	stats->idle_sample = true;
	spin_unlock_irqrestore(&stats->ctrs_lock, flags);
}

static struct qcom_pmu_notif_node memlat_idle_notif = {
	.idle_cb = memlat_pmu_idle_cb,
};

static void memlat_sched_tick_cb(void *unused, struct rq *rq)
{
	int ret, cpu = smp_processor_id();
	struct cpu_stats *stats = per_cpu(sampling_stats, cpu);
	ktime_t now = ktime_get();
	s64 delta_ns;
	unsigned long flags;

	if (unlikely(!memlat_data->inited))
		return;

	spin_lock_irqsave(&stats->ctrs_lock, flags);
	delta_ns = now - stats->last_sample_ts + HALF_TICK_NS;
	if (delta_ns < ms_to_ktime(memlat_data->sample_ms))
		goto out;
	stats->sample_ts = now;
	stats->idle_sample = false;
	stats->raw_ctrs.num_evs = 0;
	ret = qcom_pmu_read_all_local(&stats->raw_ctrs);
	if (ret < 0 || stats->raw_ctrs.num_evs == 0) {
		pr_err("error reading pmu counters on cpu%d: %d\n", cpu, ret);
		goto out;
	}
	process_raw_ctrs(stats);

out:
	spin_unlock_irqrestore(&stats->ctrs_lock, flags);
}

static void get_mpidr_cpu(void *cpu)
{
	u64 mpidr = read_cpuid_mpidr() & MPIDR_HWID_BITMASK;

	*((uint32_t *)cpu) = MPIDR_AFFINITY_LEVEL(mpidr, 1);
}

static int get_mask_and_mpidr_from_pdev(struct platform_device *pdev,
					cpumask_t *mask, u32 *cpus_mpidr)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_phandle;
	struct device *cpu_dev;
	int cpu, i = 0;
	uint32_t physical_cpu;
	int ret = -ENODEV;

	dev_phandle = of_parse_phandle(dev->of_node, "qcom,cpulist", i++);
	while (dev_phandle) {
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpumask_set_cpu(cpu, mask);
				smp_call_function_single(cpu, get_mpidr_cpu,
							 &physical_cpu, true);
				*cpus_mpidr |= BIT(physical_cpu);
				ret = 0;
				break;
			}
		}
		dev_phandle = of_parse_phandle(dev->of_node, "qcom,cpulist", i++);
	}

	return ret;
}

#define COREDEV_TBL_PROP	"qcom,cpufreq-memfreq-tbl"
#define NUM_COLS		2
static struct cpufreq_memfreq_map *init_cpufreq_memfreq_map(struct device *dev,
							    struct device_node *of_node,
							    u32 *cnt)
{
	int len, nf, i, j;
	u32 data;
	struct cpufreq_memfreq_map *tbl;
	int ret;

	if (!of_find_property(of_node, COREDEV_TBL_PROP, &len))
		return NULL;
	len /= sizeof(data);

	if (len % NUM_COLS || len == 0)
		return NULL;
	nf = len / NUM_COLS;

	tbl = devm_kzalloc(dev, (nf + 1) * sizeof(struct cpufreq_memfreq_map),
			GFP_KERNEL);
	if (!tbl)
		return NULL;

	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(of_node, COREDEV_TBL_PROP,
							j, &data);
		if (ret < 0)
			return NULL;
		tbl[i].cpufreq_mhz = data / 1000;

		ret = of_property_read_u32_index(of_node, COREDEV_TBL_PROP,
							j + 1, &data);
		if (ret < 0)
			return NULL;
		tbl[i].memfreq_khz = data;
		pr_debug("Entry%d CPU:%u, Mem:%u\n", i, tbl[i].cpufreq_mhz,
				tbl[i].memfreq_khz);
	}
	*cnt = nf;
	tbl[i].cpufreq_mhz = 0;

	return tbl;
}

static bool memlat_grps_and_mons_inited(void)
{
	struct memlat_group *memlat_grp;
	int grp;

	if (memlat_data->num_inited_grps < memlat_data->num_grps)
		return false;

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (!memlat_grp)
			continue;
		if (memlat_grp->num_inited_mons < memlat_grp->num_mons)
			return false;
	}

	return true;
}

static int memlat_sampling_init(void)
{
	int cpu;
	struct device *dev = memlat_data->dev;
	struct cpu_stats *stats;

	for_each_possible_cpu(cpu) {
		stats = devm_kzalloc(dev, sizeof(*stats), GFP_KERNEL);
		if (!stats)
			return -ENOMEM;
		per_cpu(sampling_stats, cpu) = stats;
		spin_lock_init(&stats->ctrs_lock);
	}

	memlat_data->memlat_wq = create_freezable_workqueue("memlat_wq");
	if (!memlat_data->memlat_wq) {
		dev_err(dev, "Couldn't create memlat workqueue.\n");
		return -ENOMEM;
	}
	INIT_WORK(&memlat_data->work, &memlat_update_work);

	hrtimer_init(&memlat_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	memlat_data->timer.function = memlat_hrtimer_handler;

	register_trace_android_vh_scheduler_tick(memlat_sched_tick_cb, NULL);
	register_trace_android_vh_jiffies_update(memlat_jiffies_update_cb, NULL);
	qcom_pmu_idle_register(&memlat_idle_notif);

	return 0;
}

static inline bool should_enable_memlat_fp(void)
{
	int grp;
	struct memlat_group *memlat_grp;

	/* wait until all groups have inited before enabling */
	if (memlat_data->num_inited_grps < memlat_data->num_grps)
		return false;

	for (grp = 0; grp < MAX_MEMLAT_GRPS; grp++) {
		memlat_grp = memlat_data->groups[grp];
		if (memlat_grp && memlat_grp->fp_voting_enabled)
			return true;
	}

	return false;
}

static int configure_cpucp_grp(struct memlat_group *grp)
{
	const struct scmi_memlat_vendor_ops *ops = memlat_data->memlat_ops;
	int ret = 0, i, j = 0;
	struct device_node *of_node = grp->dev->of_node;
	u8 ev_map[MAX_EV_CNTRS];

	ret = ops->set_mem_grp(memlat_data->ph, *cpumask_bits(cpu_possible_mask),
			       grp->hw_type);
	if (ret < 0) {
		pr_err("Failed to configure mem grp %s\n", of_node->name);
		return ret;
	}

	memset(ev_map, 0xFF, MAX_EV_CNTRS);
	for (i = 0; i < NUM_COMMON_EVS; i++, j++) {
		if (!memlat_data->common_ev_ids[i])
			continue;

		ret = qcom_get_cpucp_id(memlat_data->common_ev_ids[i], 0);
		if (ret >= 0 && ret < MAX_CPUCP_EVT)
			ev_map[j] = ret;
	}

	for (i = 0; i < NUM_GRP_EVS; i++, j++) {
		if (!grp->grp_ev_ids[i])
			continue;

		ret = qcom_get_cpucp_id(grp->grp_ev_ids[i], 0);
		if (ret >= 0 && ret < MAX_CPUCP_EVT)
			ev_map[j] = ret;
	}

	ret = ops->set_ev_map(memlat_data->ph, grp->hw_type, ev_map);
	if (ret < 0)
		pr_err("Failed to configure event map for mem grp %s\n",
							of_node->name);
	return ret;
}

static int configure_cpucp_mon(struct memlat_mon *mon)
{
	struct memlat_group *grp = mon->memlat_grp;
	const struct scmi_memlat_vendor_ops *ops = memlat_data->memlat_ops;
	struct device_node *of_node = mon->dev->of_node;
	int ret;

	ret = ops->set_mon(memlat_data->ph, mon->cpus_mpidr, grp->hw_type,
			   mon->is_compute, mon->index);
	if (ret < 0) {
		pr_err("failed to configure monitor %s\n", of_node->name);
		return ret;
	}

	ret = ops->ipm_ceil(memlat_data->ph, grp->hw_type, mon->index,
			    mon->ipm_ceil);
	if (ret < 0) {
		pr_err("failed to set ipm ceil for %s\n", of_node->name);
		return ret;
	}

	ret = ops->fe_stall_floor(memlat_data->ph, grp->hw_type, mon->index,
				  mon->fe_stall_floor);
	if (ret < 0) {
		pr_err("failed to set fe stall floor for %s\n", of_node->name);
		return ret;
	}

	ret = ops->be_stall_floor(memlat_data->ph, grp->hw_type, mon->index,
				  mon->be_stall_floor);
	if (ret < 0) {
		pr_err("failed to set be stall floor for %s\n", of_node->name);
		return ret;
	}

	ret = ops->sample_ms(memlat_data->ph, memlat_data->cpucp_sample_ms);
	if (ret < 0) {
		pr_err("failed to set cpucp sample_ms for %s\n", of_node->name);
		return ret;
	}

	ret = ops->wb_pct_thres(memlat_data->ph, grp->hw_type, mon->index,
				mon->wb_pct_thres);
	if (ret < 0) {
		pr_err("failed to set wb pct for %s\n", of_node->name);
		return ret;
	}

	ret = ops->wb_filter_ipm(memlat_data->ph, grp->hw_type, mon->index,
				 mon->wb_filter_ipm);
	if (ret < 0) {
		pr_err("failed to set wb filter ipm for %s\n", of_node->name);
		return ret;
	}

	ret = ops->freq_scale_pct(memlat_data->ph, grp->hw_type, mon->index,
				  mon->freq_scale_pct);
	if (ret < 0) {
		pr_err("failed to set freq_scale_pct for %s\n", of_node->name);
		return ret;
	}

	ret = ops->freq_scale_limit_mhz(memlat_data->ph, grp->hw_type, mon->index,
					mon->freq_scale_limit_mhz);
	if (ret < 0) {
		pr_err("failed to set wb filter ipm for %s\n", of_node->name);
		return ret;
	}

	ret = ops->freq_map(memlat_data->ph, grp->hw_type, mon->index,
			    mon->freq_map_len, mon->freq_map);
	if (ret < 0) {
		pr_err("failed to configure freq_map for %s\n", of_node->name);
		return ret;
	}

	ret = ops->min_freq(memlat_data->ph, grp->hw_type, mon->index,
			    mon->min_freq);
	if (ret < 0) {
		pr_err("failed to set min_freq for %s\n", of_node->name);
		return ret;
	}

	ret = ops->max_freq(memlat_data->ph, grp->hw_type, mon->index,
			    mon->max_freq);
	if (ret < 0)
		pr_err("failed to set max_freq for %s\n", of_node->name);

	return ret;
}

int cpucp_memlat_init(struct scmi_device *sdev)
{
	int ret = 0, i, j;
	struct scmi_protocol_handle *ph;
	const struct scmi_memlat_vendor_ops *ops;
	struct memlat_group *grp;
	bool start_cpucp_timer = false;

	if (!memlat_data->inited)
		return -EPROBE_DEFER;

	if (!sdev || !sdev->handle)
		return -EINVAL;

	ops = sdev->handle->devm_get_protocol(sdev, SCMI_PROTOCOL_MEMLAT, &ph);
	if (IS_ERR(ops))
		return PTR_ERR(ops);

	mutex_lock(&memlat_lock);
	memlat_data->ph = ph;
	memlat_data->memlat_ops = ops;

	/* Configure group and common parameters */
	for (i = 0; i < MAX_MEMLAT_GRPS; i++) {
		grp = memlat_data->groups[i];
		if (!grp->cpucp_enabled)
			continue;
		ret = configure_cpucp_grp(grp);
		if (ret < 0) {
			pr_err("Failed to configure mem group: %d\n", ret);
			ops = NULL;
			goto memlat_unlock;
		}

		mutex_lock(&grp->mons_lock);
		for (j = 0; j < grp->num_inited_mons; j++) {
			if (grp->mons[j].type != CPUCP_MON)
				continue;
			/* Configure per monitor parameters */
			ret = configure_cpucp_mon(&grp->mons[j]);
			if (ret < 0) {
				pr_err("failed to configure mon: %d\n", ret);
				ops = NULL;
				goto mons_unlock;
			}
			start_cpucp_timer = true;
		}
		mutex_unlock(&grp->mons_lock);
	}

	/* Start sampling and voting timer */
	if (!start_cpucp_timer)
		goto memlat_unlock;

	ret = ops->start_timer(memlat_data->ph);
	if (ret < 0)
		pr_err("Error in starting the mem group timer %d\n", ret);

	goto memlat_unlock;

mons_unlock:
	mutex_unlock(&grp->mons_lock);
memlat_unlock:
	mutex_unlock(&memlat_lock);
	return ret;
}
EXPORT_SYMBOL(cpucp_memlat_init);

#define INST_EV		0x08
#define CYC_EV		0x11
static int memlat_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct kobject *dcvs_kobj;
	struct memlat_dev_data *dev_data;
	int i, cpu, ret;
	u32 event_id;

	dev_data = devm_kzalloc(dev, sizeof(*dev_data), GFP_KERNEL);
	if (!dev_data)
		return -ENOMEM;

	dev_data->dev = dev;
	dev_data->sample_ms = 8;
	dev_data->cpucp_sample_ms = 8;

	dev_data->num_grps = of_get_available_child_count(dev->of_node);
	if (!dev_data->num_grps) {
		dev_err(dev, "No memlat grps provided!\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,inst-ev", &event_id);
	if (ret < 0) {
		dev_dbg(dev, "Inst event not specified. Using def:0x%x\n",
			INST_EV);
		event_id = INST_EV;
	}
	dev_data->common_ev_ids[INST_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,cyc-ev", &event_id);
	if (ret < 0) {
		dev_dbg(dev, "Cyc event not specified. Using def:0x%x\n",
			CYC_EV);
		event_id = CYC_EV;
	}
	dev_data->common_ev_ids[CYC_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,fe-stall-ev", &event_id);
	if (ret < 0)
		dev_dbg(dev, "FE Stall event not specified. Skipping.\n");
	else
		dev_data->common_ev_ids[FE_STALL_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,be-stall-ev", &event_id);
	if (ret < 0)
		dev_dbg(dev, "BE Stall event not specified. Skipping.\n");
	else
		dev_data->common_ev_ids[BE_STALL_IDX] = event_id;

	for_each_possible_cpu(cpu) {
		for (i = 0; i < NUM_COMMON_EVS; i++) {
			event_id = dev_data->common_ev_ids[i];
			if (!event_id)
				continue;
			ret = qcom_pmu_event_supported(event_id, cpu);
			if (!ret)
				continue;
			if (ret != -EPROBE_DEFER) {
				dev_err(dev, "ev=%lu not found on cpu%d: %d\n",
						event_id, cpu, ret);
				if (event_id == INST_EV || event_id == CYC_EV)
					return ret;
			} else
				return ret;
		}
	}

	dcvs_kobj = qcom_dcvs_kobject_get(NUM_DCVS_HW_TYPES);
	if (IS_ERR(dcvs_kobj)) {
		ret = PTR_ERR(dcvs_kobj);
		dev_err(dev, "error getting kobj from qcom_dcvs: %d\n", ret);
		return ret;
	}
	ret = kobject_init_and_add(&dev_data->kobj, &memlat_settings_ktype,
					dcvs_kobj, "memlat_settings");
	if (ret < 0) {
		dev_err(dev, "failed to init memlat settings kobj: %d\n", ret);
		kobject_put(&dev_data->kobj);
		return ret;
	}

	memlat_data = dev_data;

	return 0;
}

static int memlat_grp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct memlat_group *memlat_grp;
	int i, cpu, ret;
	u32 event_id, num_mons;
	u32 hw_type = NUM_DCVS_PATHS, path_type = NUM_DCVS_PATHS;
	struct device_node *of_node;

	of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!of_node) {
		dev_err(dev, "Unable to find target-dev for grp\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(of_node, "qcom,dcvs-hw-type", &hw_type);
	if (ret < 0 || hw_type >= NUM_DCVS_HW_TYPES) {
		dev_err(dev, "invalid dcvs hw_type=%d, ret=%d\n", hw_type, ret);
		return -EINVAL;
	}

	memlat_grp = devm_kzalloc(dev, sizeof(*memlat_grp), GFP_KERNEL);
	if (!memlat_grp)
		return -ENOMEM;

	memlat_grp->hw_type = hw_type;
	memlat_grp->dev = dev;

	memlat_grp->dcvs_kobj = qcom_dcvs_kobject_get(hw_type);
	if (IS_ERR(memlat_grp->dcvs_kobj)) {
		ret = PTR_ERR(memlat_grp->dcvs_kobj);
		dev_err(dev, "error getting kobj from qcom_dcvs: %d\n", ret);
		return ret;
	}

	of_node = of_parse_phandle(dev->of_node, "qcom,sampling-path", 0);
	if (of_node) {
		ret = of_property_read_u32(of_node, "qcom,dcvs-path-type",
								&path_type);
		if (ret < 0 || path_type >= NUM_DCVS_PATHS) {
			dev_err(dev, "invalid dcvs path: %d, ret=%d\n",
							path_type, ret);
			return -EINVAL;
		}
		if (path_type == DCVS_FAST_PATH)
			ret = qcom_dcvs_register_voter(FP_NAME, hw_type,
								path_type);
		else
			ret = qcom_dcvs_register_voter(dev_name(dev), hw_type,
								path_type);
		if (ret < 0) {
			dev_err(dev, "qcom dcvs registration error: %d\n", ret);
			return ret;
		}
		memlat_grp->sampling_path_type = path_type;
	} else
		memlat_grp->sampling_path_type = NUM_DCVS_PATHS;

	of_node = of_parse_phandle(dev->of_node, "qcom,threadlat-path", 0);
	if (of_node) {
		ret = of_property_read_u32(of_node, "qcom,dcvs-path-type",
								&path_type);
		if (ret < 0 || path_type >= NUM_DCVS_PATHS) {
			dev_err(dev, "invalid dcvs path: %d, ret=%d\n",
							path_type, ret);
			return -EINVAL;
		}
		memlat_grp->threadlat_path_type = path_type;
	} else
		memlat_grp->threadlat_path_type = NUM_DCVS_PATHS;

	if (path_type >= NUM_DCVS_PATHS) {
		dev_err(dev, "error: no dcvs voting paths\n");
		return -ENODEV;
	}
	if (memlat_grp->sampling_path_type == DCVS_FAST_PATH ||
			memlat_grp->threadlat_path_type == DCVS_FAST_PATH) {
		memlat_grp->fp_voting_enabled = true;
		memlat_grp->fp_votes = devm_kzalloc(dev, NUM_FP_VOTERS *
						sizeof(*memlat_grp->fp_votes),
						GFP_KERNEL);
	}

	num_mons = of_get_available_child_count(dev->of_node);
	if (!num_mons) {
		dev_err(dev, "No mons provided!\n");
		return -ENODEV;
	}
	memlat_grp->mons =
		devm_kzalloc(dev, num_mons * sizeof(*memlat_grp->mons),
				GFP_KERNEL);
	if (!memlat_grp->mons)
		return -ENOMEM;
	memlat_grp->num_mons = num_mons;
	memlat_grp->num_inited_mons = 0;
	mutex_init(&memlat_grp->mons_lock);

	ret = of_property_read_u32(dev->of_node, "qcom,miss-ev", &event_id);
	if (ret < 0) {
		dev_err(dev, "Cache miss event missing for grp: %d\n", ret);
		return -EINVAL;
	}
	memlat_grp->grp_ev_ids[MISS_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,access-ev",
				   &event_id);
	if (ret < 0)
		dev_dbg(dev, "Access event not specified. Skipping.\n");
	else
		memlat_grp->grp_ev_ids[ACC_IDX] = event_id;

	ret = of_property_read_u32(dev->of_node, "qcom,wb-ev", &event_id);
	if (ret < 0)
		dev_dbg(dev, "WB event not specified. Skipping.\n");
	else
		memlat_grp->grp_ev_ids[WB_IDX] = event_id;

	for_each_possible_cpu(cpu) {
		for (i = 0; i < NUM_GRP_EVS; i++) {
			event_id = memlat_grp->grp_ev_ids[i];
			if (!event_id)
				continue;
			ret = qcom_pmu_event_supported(event_id, cpu);
			if (!ret)
				continue;
			if (ret != -EPROBE_DEFER)
				dev_err(dev, "ev=%lu not found on cpu%d: %d\n",
						event_id, cpu, ret);
			return ret;
		}
	}

	ret = kobject_init_and_add(&memlat_grp->kobj, &memlat_grp_ktype,
				memlat_grp->dcvs_kobj, "memlat");

	mutex_lock(&memlat_lock);
	memlat_data->num_inited_grps++;
	memlat_data->groups[hw_type] = memlat_grp;
	if (!memlat_data->fp_enabled && should_enable_memlat_fp()) {
		spin_lock_init(&memlat_data->fp_agg_lock);
		spin_lock_init(&memlat_data->fp_commit_lock);
		memlat_data->fp_enabled = true;
	}
	mutex_unlock(&memlat_lock);

	dev_set_drvdata(dev, memlat_grp);

	return 0;
}

static int memlat_mon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct memlat_group *memlat_grp;
	struct memlat_mon *mon;
	struct device_node *of_node = dev->of_node;
	u32 num_cpus;

	memlat_grp = dev_get_drvdata(dev->parent);
	if (!memlat_grp) {
		dev_err(dev, "Mon probe called without memlat_grp inited.\n");
		return -ENODEV;
	}

	mutex_lock(&memlat_grp->mons_lock);
	mon = &memlat_grp->mons[memlat_grp->num_inited_mons];
	mon->memlat_grp = memlat_grp;
	mon->dev = dev;

	if (get_mask_and_mpidr_from_pdev(pdev, &mon->cpus, &mon->cpus_mpidr)) {
		dev_err(dev, "Mon missing cpulist\n");
		ret = -ENODEV;
		memlat_grp->num_mons--;
		goto unlock_out_init;
	}

	num_cpus = cpumask_weight(&mon->cpus);

	if (of_property_read_bool(dev->of_node, "qcom,sampling-enabled")) {
		mutex_lock(&memlat_lock);
		if (!memlat_data->sampling_enabled) {
			ret = memlat_sampling_init();
			memlat_data->sampling_enabled = true;
		}
		mutex_unlock(&memlat_lock);
		mon->type |= SAMPLING_MON;
	}

	if (of_property_read_bool(dev->of_node, "qcom,threadlat-enabled"))
		mon->type |= THREADLAT_MON;

	if (of_property_read_bool(dev->of_node, "qcom,cpucp-enabled")) {
		mon->type |= CPUCP_MON;
		memlat_grp->cpucp_enabled = true;
	}

	if (!mon->type) {
		dev_err(dev, "No types configured for mon!\n");
		ret = -ENODEV;
		goto unlock_out;
	}

	if (of_property_read_bool(dev->of_node, "qcom,compute-mon"))
		mon->is_compute = true;

	mon->ipm_ceil = 400;
	mon->fe_stall_floor = 0;
	mon->be_stall_floor = 0;
	mon->freq_scale_pct = 0;
	mon->wb_pct_thres = 100;
	mon->wb_filter_ipm = 25000;
	mon->freq_scale_limit_mhz = 5000;
	mon->spm_thres = MAX_SPM_THRES;
	mon->spm_drop_pct = 20;
	mon->spm_window_size = 10;
	mon->spm_freq_map[0].cpufreq_mhz = 1200;
	mon->spm_freq_map[1].cpufreq_mhz = 2100;
	mon->spm_freq_map[0].memfreq_khz = 1708000;
	mon->spm_freq_map[1].memfreq_khz = 2092000;

	if (of_parse_phandle(of_node, COREDEV_TBL_PROP, 0))
		of_node = of_parse_phandle(of_node, COREDEV_TBL_PROP, 0);
	if (of_get_child_count(of_node))
		of_node = qcom_dcvs_get_ddr_child_node(of_node);

	mon->freq_map = init_cpufreq_memfreq_map(dev, of_node,
						 &mon->freq_map_len);
	if (!mon->freq_map) {
		dev_err(dev, "error importing cpufreq-memfreq table!\n");
		ret = -EINVAL;
		goto unlock_out;
	}

	mon->mon_min_freq = mon->min_freq = cpufreq_to_memfreq(mon, 0);
	mon->mon_max_freq = mon->max_freq = cpufreq_to_memfreq(mon, U32_MAX);
	mon->cur_freq = mon->min_freq;

	if (mon->is_compute)
		ret = kobject_init_and_add(&mon->kobj, &compute_mon_ktype,
					memlat_grp->dcvs_kobj, dev_name(dev));
	else
		ret = kobject_init_and_add(&mon->kobj, &memlat_mon_ktype,
					memlat_grp->dcvs_kobj, dev_name(dev));
	if (ret < 0) {
		dev_err(dev, "failed to init memlat mon kobj: %d\n", ret);
		kobject_put(&mon->kobj);
		goto unlock_out;
	}

	mon->index = memlat_grp->num_inited_mons++;
unlock_out_init:
	if (memlat_grps_and_mons_inited())
		memlat_data->inited = true;

unlock_out:
	mutex_unlock(&memlat_grp->mons_lock);
	return ret;
}

static int qcom_memlat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	const struct memlat_spec *spec = of_device_get_match_data(dev);
	enum memlat_type type = NUM_MEMLAT_TYPES;

	if (spec)
		type = spec->type;

	switch (type) {
	case MEMLAT_DEV:
		if (memlat_data) {
			dev_err(dev, "only one memlat device allowed\n");
			ret = -ENODEV;
		}
		ret = memlat_dev_probe(pdev);
		if (!ret && of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case MEMLAT_GRP:
		ret = memlat_grp_probe(pdev);
		if (!ret && of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case MEMLAT_MON:
		ret = memlat_mon_probe(pdev);
		break;
	default:
		/*
		 * This should never happen.
		 */
		dev_err(dev, "Invalid memlat mon type specified: %u\n", type);
		return -EINVAL;
	}

	if (ret < 0) {
		dev_err(dev, "Failure to probe memlat device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct memlat_spec spec[] = {
	[0] = { MEMLAT_DEV },
	[1] = { MEMLAT_GRP },
	[2] = { MEMLAT_MON },
};

static const struct of_device_id memlat_match_table[] = {
	{ .compatible = "qcom,memlat", .data = &spec[0] },
	{ .compatible = "qcom,memlat-grp", .data = &spec[1] },
	{ .compatible = "qcom,memlat-mon", .data = &spec[2] },
	{}
};

static struct platform_driver qcom_memlat_driver = {
	.probe = qcom_memlat_probe,
	.driver = {
		.name = "qcom-memlat",
		.of_match_table = memlat_match_table,
		.suppress_bind_attrs = true,
	},
};

static int __init qcom_memlat_init(void)
{
	return platform_driver_register(&qcom_memlat_driver);
}

#if IS_MODULE(CONFIG_QCOM_MEMLAT)
module_init(qcom_memlat_init);
#else
arch_initcall(qcom_memlat_init);
#endif

MODULE_DESCRIPTION("QCOM MEMLAT Driver");
MODULE_LICENSE("GPL v2");
