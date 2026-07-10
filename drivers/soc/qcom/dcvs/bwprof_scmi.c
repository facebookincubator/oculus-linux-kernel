// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "qcom-bwprof: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/mailbox_client.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/scmi_bwprof.h>
#include <linux/scmi_protocol.h>
#include <linux/configfs.h>
#include <soc/qcom/smci_object.h>
#include <linux/smcinvoke.h>
#include <soc/qcom/smci_clientenv.h>
#include "smci_bwprof.h"
#include "trace-dcvs.h"
#include "bwprof_scmi.h"

#include <linux/delay.h>
#include <linux/slab.h>

#ifndef UINT32_C
#define UINT32_C(x) ((uint32_t)(x))
#endif

#define BWMON_FEATURE_10MS  2102
#define BWMON_FEATURE_1MS   2103
#define BWMON_FEATURE_HIST  2104

static struct bwprof_dev_data *bwprof_data;
void __iomem *base_src;
struct bwprof_monitor_data *monitor_data;
struct bwprof_hist_data *hist_data;

static void reset_moniter_data(void)
{
	int i;
	void __iomem *src;

	if (!base_src || !monitor_data || !hist_data)
		return;

	mutex_lock(&bwprof_data->mons_lock);
	for (i = 0; i < MAX_NUM_SAMPLES; i++) {
		src = base_src + i * sizeof(struct bwprof_monitor_data);
		memcpy_fromio(&monitor_data[i], src,
				sizeof(struct bwprof_monitor_data));
	}

	for (i = 0; i < MAX_HIST_SAMPLES; i++) {
		src = base_src + i * sizeof(struct bwprof_hist_data);
		memcpy_fromio(&hist_data[i], src, sizeof(struct bwprof_hist_data));
	}
	mutex_unlock(&bwprof_data->mons_lock);
}

static bool check_master_info(u8 master, u8 list[], u8 cnt)
{
	int i;

	for (i = 0; i < cnt; i++)
		if (master == list[i])
			return true;

	return false;
}

static bool is_master_enable(u8 master, u32 *master_idx)
{
	u32 hw_type = bwprof_data->hw_type;
	struct bwprof_hw_group  *hw_node = bwprof_data->hw_node[hw_type];
	struct sampling_mode_info *samp_mode = hw_node->mode[hw_node->cfg_type];
	u32 i;

	for (i = 0; i < samp_mode->num_masters; i++) {
		if (master ==  samp_mode->masters_list[i]) {
			*master_idx = i;
			if ((*master_idx > samp_mode->num_masters) ||
					(*master_idx > MAX_MASTERS))
				return false;
			return true;
		}
	}
	return false;
}

static int license_check_init(void)
{
	struct smci_object bwprof_env = {NULL, NULL};
	struct smci_object bwprof_profiler = {NULL, NULL};
	int ret = 0;

	ret = get_client_env_object(&bwprof_env);
	if (ret) {
		bwprof_env.invoke = NULL;
		bwprof_env.context = NULL;
		pr_err("bwprof_env: get client env object failed\n");
		return -EIO;
	}

	ret = smci_clientenv_open(bwprof_env, SMCI_BWPROF_SERVICE_UID,
			&bwprof_profiler);
	if (ret) {
		bwprof_profiler.invoke = NULL;
		bwprof_profiler.context = NULL;
		pr_err("bwprof_profiler: smci client env open failed\n");
		return -EIO;
	}

	bwprof_data->bwprof_profiler = bwprof_profiler;

	return ret;
}

static bool is_sampling_ms_valid(u32 ms_val)
{
	if ((ms_val == SAMPLING_1MS) || (ms_val == SAMPLING_10MS) ||
			(ms_val == SAMPLING_100MS))
		return true;

	if ((ms_val > SAMPLING_100MS) &&
		((ms_val % SAMPLING_MS_GRANULARITY) == 0))
		return true;

	return false;
}

static int map_sampling_ms(u32 ms_mode)
{
	int index = 0;

	switch (ms_mode) {
	case SAMPLING_1MS:
		index = 0;
		break;
	case SAMPLING_10MS:
		index = 1;
		break;
	case BWPROF_HIST:
		index = 3;
		break;
	default:
		index = 2;
		break;
	}

	if (bwprof_data->is_hist_enable)
		index = 3;

	return index;
}

static ssize_t
bwprof_set_config_show(struct config_item *item, char *page)
{
	if (!bwprof_data->is_set_config) {
		page[0] = '\0';
		return 0;
	}

	return scnprintf(page, PAGE_SIZE, "%s", bwprof_data->set_config_str);
}

static ssize_t bwprof_set_config_store(struct config_item *item,
		const char *page, size_t count)
{
	struct bwprof_hw_group *hw_node;
	struct sampling_mode_info *sample_mode, *default_mode_val;
	const struct scmi_bwprof_vendor_ops *ops = bwprof_data->bwprof_ops;
	char *input, *token, *param_name, *ms_name;
	u32 i = 0, cfg_type = 0;
	u16 sampling_ms;
	u8 master_cnt = 0;
	u8 masters_list[MAX_MASTERS];
	u32 bucket_list[MAX_USER_BUCKETS];
	u8 bucket_cnt = 0;
	u8 hist_enable = 0;
	int ret;

	if (bwprof_data->is_sampling_enable)
		return -EINVAL;

	input = kstrdup(page, GFP_KERNEL);
	if (!input)
		return -EINVAL;

	while ((token = strsep(&input, ":")) != NULL) {
		param_name = strsep(&token, "=");
		if (!param_name || !token) {
			kfree(input);
			return -EINVAL;
		}

		if (!strcmp(param_name, "sampling_ms")) {
			if (kstrtou16(token, 0, &sampling_ms) < 0) {
				kfree(input);
				return -EINVAL;
			}
			continue;
		} else if (!strcmp(param_name, "masters")) {
			while ((ms_name = strsep(&token, ",")) != NULL) {
				if (kstrtou8(ms_name, 0, &masters_list[master_cnt]) < 0) {
					kfree(input);
					return -EINVAL;
				}
				master_cnt++;
			}
			continue;
		} else if (!strcmp(param_name, "hist")) {
			if (kstrtou8(token, 0, &hist_enable) < 0) {
				kfree(input);
				return -EINVAL;
			}
		}

		if (hist_enable) {
			if (!strcmp(param_name, "bucket"))
				while ((ms_name = strsep(&token, ",")) != NULL) {
					if (kstrtouint(ms_name, 0,
						&bucket_list[bucket_cnt]) < 0) {
						kfree(input);
						return -EINVAL;
					}
					bucket_cnt++;
				}
		}
	}

	bwprof_data->is_hist_enable = (hist_enable) ? true:false;
	if ((!is_sampling_ms_valid(sampling_ms)) || (master_cnt > MAX_MASTERS) ||
			(master_cnt == 0))
		return -EINVAL;

	ret = license_check_init();
	if (ret) {
		pr_err("bwprof_scmi: license_check_init failed\n");
		return -EIO;
	}

	if (sampling_ms == SAMPLING_1MS) {
		if (bwprof_data->is_hist_enable) {
			ret = smci_bwprof_license_check(bwprof_data->bwprof_profiler,
					BWMON_FEATURE_HIST, NULL, 0);
		} else {
			ret = smci_bwprof_license_check(bwprof_data->bwprof_profiler,
					BWMON_FEATURE_1MS, NULL, 0);
		}
		if (ret) {
			pr_err("smci_bwprof_license_check failed : %d\n", ret);
			return -EIO;
		}
	}

	cfg_type = map_sampling_ms(sampling_ms);
	bwprof_data->hw_type = (masters_list[0] >= DDR_CPU) ? BWPROF_DDR :
		BWPROF_LLCC;

	hw_node = bwprof_data->hw_node[bwprof_data->hw_type];
	sample_mode = hw_node->mode[cfg_type];
	default_mode_val = hw_node->default_mode_val[cfg_type];

	if (!hw_node || !sample_mode || !default_mode_val || !ops)
		return -EINVAL;

	for (i = 0; i < master_cnt; i++) {
		if (!check_master_info(masters_list[i], default_mode_val->masters_list,
					default_mode_val->num_masters))
			return -EINVAL;
	}

	if (hist_enable) {
		if ((sampling_ms != SAMPLING_1MS) ||
			(bucket_cnt > MAX_USER_BUCKETS) ||
			(bucket_cnt == 0) || (bucket_cnt < MAX_USER_BUCKETS)) {
			bwprof_data->is_hist_enable = false;
			return -EINVAL;
		}
		for (i = 0; i < bucket_cnt; i++)
			sample_mode->buckets[i] = bucket_list[i];
	}

	ret = ops->set_sample_ms(bwprof_data->ph, hist_enable, sampling_ms);
	if (ret < 0) {
		pr_err("SCMI command set_sample_ms Failed: %d\n", ret);
		return ret;
	}

	if (hist_enable) {
		ret = ops->set_hist_info(bwprof_data->ph, bucket_list);
		if (ret < 0) {
			pr_err("SCMI command set_hist_info Failed: %d\n", ret);
			return ret;
		}
	}

	ret = ops->set_masters_list(bwprof_data->ph, master_cnt, masters_list);
	if (ret < 0) {
		pr_err("SCMI command set_masters_list Failed: %d\n", ret);
		bwprof_data->is_hist_enable = false;
		return ret;
	}

	sample_mode->num_masters = master_cnt;
	hw_node->cfg_type = cfg_type;
	sample_mode->sampling_ms = sampling_ms;
	bwprof_data->sample_ms = sampling_ms;
	bwprof_data->is_set_config = true;

	strscpy(bwprof_data->set_config_str, page,
			sizeof(bwprof_data->set_config_str));

	for (i = 0; i < master_cnt; i++)
		sample_mode->masters_list[i] = masters_list[i];

	kfree(input);
	return count;
}

CONFIGFS_ATTR(bwprof_, set_config);

static ssize_t bwprof_available_config_show(struct config_item *item,
		char *page)
{
	struct bwprof_hw_group	*hw_node;
	struct sampling_mode_info *mode;
	u32 cnt = 0, j, i;
	u8 k;
	u32 samp_cnt;
	const char *hw_name;

	for (i = 0; i < bwprof_data->hw_cnt; i++) {
		hw_node = bwprof_data->hw_node[i];
		samp_cnt = hw_node->sampling_cnt;
		if (hw_node->hw_type == BWPROF_DDR)
			hw_name = "DDR";
		else if (hw_node->hw_type == BWPROF_LLCC)
			hw_name = "LLCC";
		else
			hw_name = "UNKNOWN";

		cnt += scnprintf(page+cnt, PAGE_SIZE - cnt, "\nhw_type: %s",
				hw_name);
		for (j = 0; j < samp_cnt; j++) {
			mode = hw_node->default_mode_val[j];
			if (j == BWPROF_HIST)
				cnt += scnprintf(page + cnt, PAGE_SIZE - cnt,
					"\nsampling_ms: %dms hist masters :",
					mode->sampling_ms);
			else
				cnt += scnprintf(page + cnt, PAGE_SIZE - cnt,
					"\nsampling_ms: %dms masters :",
					mode->sampling_ms);
			for (k = 0; k < mode->num_masters; k++)
				cnt += scnprintf(page + cnt, PAGE_SIZE - cnt,
					"%u ", mode->masters_list[k]);
		}
		cnt += scnprintf(page+cnt, PAGE_SIZE - cnt, "\n");
	}
	return cnt;
}

CONFIGFS_ATTR_RO(bwprof_, available_config);

static ssize_t
bwprof_enable_config_show(struct config_item *item, char *page)
{
	u32 hw_type = bwprof_data->hw_type;
	struct bwprof_hw_group  *hw_node = bwprof_data->hw_node[hw_type];
	struct sampling_mode_info *samp_mode;

	samp_mode = hw_node->mode[hw_node->cfg_type];
	return scnprintf(page, PAGE_SIZE, "%u\n", samp_mode->enable);
}

static ssize_t bwprof_enable_config_store(struct config_item *item,
		const char *page, size_t count)
{
	u32 hw_type = bwprof_data->hw_type;
	struct bwprof_hw_group  *hw_node = bwprof_data->hw_node[hw_type];
	struct sampling_mode_info *samp_mode = hw_node->mode[hw_node->cfg_type];
	const struct scmi_bwprof_vendor_ops *ops = bwprof_data->bwprof_ops;
	int ret;
	u8 enable;

	ret = kstrtou8(page, 10, &enable);
	if (ret < 0)
		return ret;

	if ((samp_mode->enable == enable) || (!bwprof_data->is_set_config) ||
			(!ops))
		return -EINVAL;

	ret = ops->set_sampling_enable(bwprof_data->ph, enable);
	if (ret < 0) {
		pr_err("SCMI command set_sampling_enable Failed : %d\n", ret);
		return ret;
	}

	samp_mode->enable = enable;
	bwprof_data->is_sampling_enable = (enable) ? true:false;

	if (!enable)
		reset_moniter_data();

	return count;
}

CONFIGFS_ATTR(bwprof_, enable_config);

static ssize_t moniter_data_show(char *page, int master_idx)
{
	u32 hw_type = bwprof_data->hw_type;
	struct bwprof_hw_group  *hw_node = bwprof_data->hw_node[hw_type];
	u32 bus_width = hw_node->bus_width;
	int cnt = 0;
	int i;
	int num_samples_to_read = (MAX_NUM_SAMPLES / bwprof_data->sample_ms);

	if (bwprof_data->sample_ms >= SAMPLING_100MS)
		num_samples_to_read = 1;

	mutex_lock(&bwprof_data->mons_lock);
	for (i = 0; i < num_samples_to_read; i++) {
		cnt += scnprintf(page+cnt, PAGE_SIZE - cnt, "%llu\t%u\t%u\t%u\n",
				monitor_data[i].ts, monitor_data[i].meas_mbps[master_idx],
				(monitor_data[i].mem_freq * bus_width),
				monitor_data[i].mem_freq);
	}
	mutex_unlock(&bwprof_data->mons_lock);
	return cnt;
}

static ssize_t hist_moniter_data(char *page, int master_idx)
{
	int cnt = 0;
	u32 i, j, t;

	mutex_lock(&bwprof_data->mons_lock);
	for (i = 0; i < MAX_HIST_SAMPLES; i++) {
		cnt += scnprintf(page+cnt, PAGE_SIZE - cnt, "%llu",
				hist_data[i].ts);
		for (j = 0; j < MAX_BUCKETS; j++) {
			t = (master_idx * MAX_BUCKETS) + j;
			cnt += scnprintf(page+cnt, PAGE_SIZE - cnt, "\t%u",
					hist_data[i].sample[t]);
		}
		cnt += scnprintf(page+cnt, PAGE_SIZE - cnt, "\n");
	}
	mutex_unlock(&bwprof_data->mons_lock);
	return cnt;
}

static ssize_t bwprof_ls_show_common(char *page, int master)
{
	int cnt = 0;
	u32 master_idx;

	if (is_master_enable(master, &master_idx)) {
		if (!bwprof_data->is_hist_enable)
			cnt = moniter_data_show(page, master_idx);
		else
			cnt = hist_moniter_data(page, master_idx);
	}
	return cnt;
}

static ssize_t bwprof_cpu_ls_show(struct config_item *item, char *page)
{
	u8 master;
	struct bwprof_hw_group *grp = container_of(to_config_group(item),
			struct bwprof_hw_group, ls_group);

	if (!grp)
		return -EINVAL;

	if (grp->hw_type == BWPROF_DDR)
		master = DDR_CPU;
	else if (grp->hw_type == BWPROF_LLCC)
		master = LLCC_CPU;
	else
		return -EINVAL;

	return bwprof_ls_show_common(page, master);
}

CONFIGFS_ATTR_RO(bwprof_, cpu_ls);

static ssize_t bwprof_gpu_ls_show(struct config_item *item, char *page)
{
	u8 master;
	struct bwprof_hw_group *grp = container_of(to_config_group(item),
			struct bwprof_hw_group, ls_group);

	if (!grp)
		return -EINVAL;

	if (grp->hw_type == BWPROF_DDR)
		master = DDR_GPU;
	else if (grp->hw_type == BWPROF_LLCC)
		master = LLCC_GPU;
	else
		return -EINVAL;

	return bwprof_ls_show_common(page, master);
}

CONFIGFS_ATTR_RO(bwprof_, gpu_ls);

static ssize_t bwprof_total_ls_show(struct config_item *item, char *page)
{
	u8 master;
	struct bwprof_hw_group *grp = container_of(to_config_group(item),
			struct bwprof_hw_group, ls_group);

	if (!grp)
		return -EINVAL;

	if (grp->hw_type == BWPROF_DDR)
		master = DDR_TOTAL;
	else if (grp->hw_type == BWPROF_LLCC)
		master = LLCC_TOTAL;
	else
		return -EINVAL;

	return bwprof_ls_show_common(page, master);
}

CONFIGFS_ATTR_RO(bwprof_, total_ls);

static struct configfs_attribute *bwprof_attrs[] = {
	&bwprof_attr_available_config,
	&bwprof_attr_set_config,
	&bwprof_attr_enable_config,
	NULL,
};

static struct configfs_attribute *bwprof_ls_attrs[] = {
	&bwprof_attr_cpu_ls,
	&bwprof_attr_gpu_ls,
	&bwprof_attr_total_ls,
	NULL,
};

static const struct config_item_type ls_item_type = {
	.ct_attrs   = bwprof_ls_attrs,
};

static void trace_event(void)
{
	u32 hw_type = bwprof_data->hw_type;
	struct bwprof_hw_group  *hw_node = bwprof_data->hw_node[hw_type];
	u32 bus_width = hw_node->bus_width;

	if (!bwprof_data->is_hist_enable) {
		trace_bwprof_last_sample_meas(dev_name(bwprof_data->dev),
				monitor_data[0].ts,
				monitor_data[0].meas_mbps[0],
				monitor_data[0].meas_mbps[1],
				monitor_data[0].meas_mbps[2],
				(monitor_data[0].mem_freq * bus_width),
				monitor_data[0].mem_freq);
	}
}

static void bwprof_mon_rx(struct mbox_client *client, void *msg)
{
	int i;
	int num_samples_to_read  = (MAX_NUM_SAMPLES / bwprof_data->sample_ms);
	void __iomem *src;

	if (bwprof_data->sample_ms >= SAMPLING_100MS)
		num_samples_to_read = 1;

	mutex_lock(&bwprof_data->mons_lock);
	if (!bwprof_data->is_hist_enable) {
		for (i = 0; i < num_samples_to_read; i++) {
			src = base_src + i * sizeof(struct bwprof_monitor_data);
			memcpy_fromio(&monitor_data[i], src,
					sizeof(struct bwprof_monitor_data));
		}
	} else {
		for (i = 0; i < MAX_HIST_SAMPLES; i++) {
			src = base_src + i * sizeof(struct bwprof_hist_data);
			memcpy_fromio(&hist_data[i], src,
				sizeof(struct bwprof_hist_data));
		}
	}
	trace_event();
	mutex_unlock(&bwprof_data->mons_lock);
}

static const struct config_item_type bwprof_subsys_type = {
	.ct_attrs = bwprof_attrs,
	.ct_owner   = THIS_MODULE,
};

static struct configfs_subsystem bwprof_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "bwprof",
			.ci_type = &bwprof_subsys_type,
		},
	},
};

static int bwprof_configfs_init(void)
{
	struct bwprof_hw_group  *hw_node;
	u32 i;
	int ret;
	const char *name;

	config_group_init(&bwprof_subsys.su_group);
	mutex_init(&bwprof_subsys.su_mutex);

	for (i = 0; i < bwprof_data->hw_cnt; i++) {
		hw_node = bwprof_data->hw_node[i];

		switch (hw_node->hw_type) {
		case BWPROF_DDR:
			name = "ddr";
			break;
		case BWPROF_LLCC:
			name = "llcc";
			break;
		default:
			pr_err("bwprof_scmi: Unknown hw_type: %d\n",
				hw_node->hw_type);
			continue;
		}

		config_group_init_type_name(&hw_node->ls_group, name,
			&ls_item_type);
		configfs_add_default_group(&hw_node->ls_group,
			&bwprof_subsys.su_group);

	}

	ret = configfs_register_subsystem(&bwprof_subsys);
	if (ret) {
		mutex_destroy(&bwprof_subsys.su_mutex);
		pr_err("Failed configfs_register_subsystem %d\n", ret);
		return ret;
	}

	bwprof_data->dev_group = &bwprof_subsys.su_group;

	return 0;
}

int cpucp_bwprof_init(struct scmi_device *sdev)
{
	u32 data_size;
	const struct scmi_bwprof_vendor_ops *ops;
	struct scmi_protocol_handle *ph;
	int ret;

	if (!bwprof_data || !bwprof_data->inited)
		return -EPROBE_DEFER;

	if (!sdev || !sdev->handle)
		return -ENXIO;

	ops = sdev->handle->devm_get_protocol(sdev, SCMI_PROTOCOL_BWPROF, &ph);
	if (IS_ERR(ops)) {
		ret = PTR_ERR(ops);
		pr_err("SCMI devm_get_protocol Failed\n");
		ops = NULL;
		return ret;
	}

	data_size = (MAX_NUM_SAMPLES * sizeof(struct bwprof_monitor_data));
	monitor_data = kzalloc(data_size, GFP_KERNEL);
	if (!monitor_data)
		return -ENOMEM;

	data_size = (MAX_HIST_SAMPLES * sizeof(struct bwprof_hist_data));
	hist_data = kzalloc(data_size, GFP_KERNEL);
	if (!hist_data) {
		kfree(monitor_data);
		monitor_data = NULL;
		return -ENOMEM;
	}

	if (bwprof_configfs_init()) {
		pr_err("bwprof_configfs_init failed\n");
		return -EINVAL;
	}

	bwprof_data->ph = ph;
	bwprof_data->bwprof_ops = ops;
	bwprof_data->is_sampling_enable = false;
	bwprof_data->is_hist_enable = false;
	bwprof_data->is_set_config = false;
	reset_moniter_data();

	return 0;
}
EXPORT_SYMBOL_GPL(cpucp_bwprof_init);

static int bwprof_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	struct mbox_client *cl;

	bwprof_data = devm_kzalloc(dev, sizeof(*bwprof_data), GFP_KERNEL);
	if (!bwprof_data)
		return -ENOMEM;

	bwprof_data->dev = dev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mem-base");
	if (!res) {
		dev_err(dev, "Failed to get mem-base resource\n");
		return -ENODEV;
	}
	base_src = devm_ioremap_resource(&pdev->dev, res);
	if (!base_src) {
		dev_err(dev, "Failed ioremap for bwprof!\n");
		return -ENOMEM;
	}

	bwprof_data->hw_cnt = of_get_available_child_count(dev->of_node);
	if ((!bwprof_data->hw_cnt) || (bwprof_data->hw_cnt > BWPROF_TOTAL_HW)) {
		dev_err(dev, "No bwprof hw nodes provided!\n");
		return -ENODEV;
	}

	cl = &bwprof_data->cl;
	cl->dev = dev;
	cl->tx_block = false;
	cl->knows_txdone = true;
	cl->rx_callback = bwprof_mon_rx;

	bwprof_data->ch = mbox_request_channel(cl, 0);
	if (IS_ERR(bwprof_data->ch)) {
		ret = PTR_ERR(bwprof_data->ch);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed mbox_request_channel: %d\n", ret);
		return ret;
	}

	bwprof_data->inited = true;
	mutex_init(&bwprof_data->mons_lock);

	return 0;
}

static int bwprof_hw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bwprof_hw_group *bwprof_hw = NULL;
	int ret;
	u32 hw_type;
	u32 bus_width;
	u32 sampling_cnt;

	bwprof_hw = devm_kzalloc(dev, sizeof(*bwprof_hw), GFP_KERNEL);
	if (!bwprof_hw)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "qcom,hw-type", &hw_type);
	if (ret < 0 || hw_type >= BWPROF_TOTAL_HW) {
		dev_err(dev, "Error hw_type=%d, ret=%d\n", hw_type, ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,bus_width", &bus_width);
	if (ret < 0) {
		dev_err(dev, "Error bus_width=%d, ret=%d\n", bus_width, ret);
		return -EINVAL;
	}

	sampling_cnt = of_get_available_child_count(dev->of_node);
	if ((!sampling_cnt) || (sampling_cnt > TOTAL_SAMPLING_MODE_TYPES)) {
		dev_err(dev, "Incorrect sampling nodes configuration!\n");
		return -ENODEV;
	}

	bwprof_hw->dev = dev;
	bwprof_hw->hw_type = hw_type;
	bwprof_hw->bus_width = bus_width;
	bwprof_hw->sampling_cnt = sampling_cnt;
	bwprof_data->hw_node[hw_type] = bwprof_hw;
	dev_set_drvdata(dev, bwprof_hw);

	return 0;
}

static int bwprof_sampling_mode_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bwprof_hw_group *bwprof_hw;
	struct sampling_mode_info *default_mode_val;
	struct sampling_mode_info *mode;
	int ret;
	u32 sampling_ms;
	u8 num_masters;

	default_mode_val = devm_kzalloc(dev, sizeof(*default_mode_val), GFP_KERNEL);
	if (!default_mode_val)
		return -ENOMEM;

	mode = devm_kzalloc(dev, sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return -ENOMEM;

	bwprof_hw = dev_get_drvdata(dev->parent);
	if (!bwprof_hw) {
		dev_err(dev, "Failed dev_get_drvdata\n");
		return -ENODEV;
	}

	num_masters = of_property_count_elems_of_size(dev->of_node,
			"qcom,master-list", sizeof(u8));
	if (num_masters <= 0) {
		dev_info(dev, "Failed to get master list\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,sample-period",
			&sampling_ms);
	if (ret < 0) {
		dev_err(dev, "Failed to get sample period =%d\n", sampling_ms);
		return -EINVAL;
	}

	default_mode_val->sampling_ms = sampling_ms;

	if (of_property_read_bool(dev->of_node, "qcom,hist-enable"))
		sampling_ms = BWPROF_HIST;

	sampling_ms = map_sampling_ms(sampling_ms);

	mode->masters_list = devm_kzalloc(dev, sizeof(u8)*num_masters, GFP_KERNEL);
	if (!mode->masters_list)
		return -ENOMEM;

	default_mode_val->masters_list = devm_kzalloc(dev,
			sizeof(u8)*num_masters, GFP_KERNEL);
	if (!default_mode_val->masters_list)
		return -ENOMEM;

	ret = of_property_read_u8_array(dev->of_node, "qcom,master-list",
			default_mode_val->masters_list, num_masters);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get master-list\n");
		return ret;
	}

	default_mode_val->num_masters = num_masters;
	default_mode_val->enable = 0;
	mode->enable = 0;
	bwprof_hw->default_mode_val[sampling_ms] = default_mode_val;
	bwprof_hw->mode[sampling_ms] = mode;

	return 0;
}

static int qcom_bwprof_scmi_probe(struct platform_device *pdev)
{
	enum bwprof_type type = NUM_BWPROF_TYPES;
	struct device *dev = &pdev->dev;
	const struct bwprof_spec *spec;
	int ret = 0;

	spec = of_device_get_match_data(dev);
	if (spec)
		type = spec->type;

	switch (type) {
	case BWPROF_DEV:
		if (bwprof_data) {
			dev_err(dev, "only one bwprof device allowed\n");
			ret = -ENODEV;
		}
		ret = bwprof_dev_probe(pdev);
		if (!ret && of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case BWPROF_HW:
		ret = bwprof_hw_probe(pdev);
		if (!ret && of_get_available_child_count(dev->of_node))
			of_platform_populate(dev->of_node, NULL, NULL, dev);
		break;
	case BWPROF_SAMPLING:
		ret = bwprof_sampling_mode_probe(pdev);
		break;
	default:
		/*
		 * This should never happen.
		 */
		dev_err(dev, "Invalid bwprof type specified: %u\n", type);
		return -EINVAL;
	}

	if (ret < 0) {
		dev_err(dev, "Failure to probe bwprof device: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct bwprof_spec spec[] = {
	[0] = { BWPROF_DEV },
	[1] = { BWPROF_HW },
	[2] = { BWPROF_SAMPLING },
};

static const struct of_device_id qcom_bwprof_scmi_match_table[] = {
	{ .compatible = "qcom,bwprof-scmi", .data = &spec[0] },
	{ .compatible = "qcom,bwprof-hw", .data = &spec[1] },
	{ .compatible = "qcom,bwprof-sampling-mode", .data = &spec[2] },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_bwprof_scmi_match_table);

static struct platform_driver qcom_bwprof_scmi_driver = {
	.probe = qcom_bwprof_scmi_probe,
	.driver = {
		.name = "qcom-bwprof-scmi",
		.of_match_table = qcom_bwprof_scmi_match_table,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(qcom_bwprof_scmi_driver);

MODULE_DESCRIPTION("QCOM BWPROF SCMI driver");
MODULE_LICENSE("GPL");
