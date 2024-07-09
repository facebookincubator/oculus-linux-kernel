// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/pm_opp.h>
#include <linux/cpu_cooling.h>
#include <linux/atomic.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>

#include <asm/smp_plat.h>
#include <asm/cacheflush.h>

#include <soc/qcom/scm.h>

#include "../thermal_core.h"
#include "lmh_dbg.h"

#define CREATE_TRACE_POINTS
#include <trace/events/lmh.h>

#define LIMITS_DCVSH			0x10
#define LIMITS_PROFILE_CHANGE		0x01
#define LIMITS_NODE_DCVS		0x44435653

#define LIMITS_SUB_FN_THERMAL		0x54484D4C
#define LIMITS_SUB_FN_CRNT		0x43524E54
#define LIMITS_SUB_FN_REL		0x52454C00
#define LIMITS_SUB_FN_BCL		0x42434C00

#define LIMITS_ALGO_MODE_ENABLE		0x454E424C

#define LIMITS_HI_THRESHOLD		0x48494748
#define LIMITS_LOW_THRESHOLD		0x4C4F5700
#define LIMITS_ARM_THRESHOLD		0x41524D00

#define LIMITS_CLUSTER_0		0x6370302D
#define LIMITS_CLUSTER_1		0x6370312D

#define LIMITS_FREQ_CAP			0x46434150

#define LIMITS_TEMP_DEFAULT		75000
#define LIMITS_TEMP_HIGH_THRESH_MAX	120000
#define LIMITS_LOW_THRESHOLD_OFFSET	500
#define LIMITS_POLLING_DELAY_MS		10
#define LIMITS_CLUSTER_REQ_OFFSET	0x704
#define LIMITS_CLUSTER_INT_CLR_OFFSET	0x8
#define LIMITS_CLUSTER_MIN_FREQ_OFFSET	0x3C0
#define dcvsh_get_frequency(_val, _max) do { \
	_max = (_val) & 0x3FF; \
	_max *= 19200; \
} while (0)
#define FREQ_KHZ_TO_HZ(_val) ((_val) * 1000)
#define FREQ_HZ_TO_KHZ(_val) ((_val) / 1000)

enum lmh_hw_trips {
	LIMITS_TRIP_ARM,
	LIMITS_TRIP_HI,
	LIMITS_TRIP_MAX,
};

struct __limits_cdev_data {
	struct thermal_cooling_device *cdev;
	u32 max_freq;
	u32 min_freq;
};

struct limits_dcvs_hw {
	char sensor_name[THERMAL_NAME_LENGTH];
	uint32_t affinity;
	uint32_t temp_limits[LIMITS_TRIP_MAX];
	int irq_num;
	void *osm_hw_reg;
	void *int_clr_reg;
	void *min_freq_reg;
	cpumask_t core_map;
	cpumask_t online_mask;
	struct delayed_work freq_poll_work;
	unsigned long max_freq[NR_CPUS];
	unsigned long min_freq[NR_CPUS];
	unsigned long hw_freq_limit;
	struct device_attribute lmh_freq_attr;
	struct list_head list;
	bool is_irq_enabled;
	struct mutex access_lock;
	struct __limits_cdev_data *cdev_data;
	uint32_t cdev_registered;
	struct regulator *isens_reg[2];
	struct work_struct cdev_register_work;
};

LIST_HEAD(lmh_dcvs_hw_list);
DEFINE_MUTEX(lmh_dcvs_list_access);

static void limits_dcvs_get_freq_limits(struct limits_dcvs_hw *hw)
{
	unsigned long freq_ceil = UINT_MAX, freq_floor = 0;
	struct device *cpu_dev = NULL;
	uint32_t cpu, idx = 0;

	for_each_cpu(cpu, &hw->core_map) {
		freq_ceil = UINT_MAX;
		freq_floor = 0;
		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			pr_err("Error in get CPU%d device\n", cpu);
			idx++;
			continue;
		}

		dev_pm_opp_find_freq_floor(cpu_dev, &freq_ceil);
		dev_pm_opp_find_freq_ceil(cpu_dev, &freq_floor);

		hw->max_freq[idx] = freq_ceil / 1000;
		hw->min_freq[idx] = freq_floor / 1000;
		idx++;
	}
}

static unsigned long limits_mitigation_notify(struct limits_dcvs_hw *hw)
{
	uint32_t val = 0, max_cpu_ct = 0, max_cpu_limit = 0, idx = 0, cpu = 0;
	struct device *cpu_dev = NULL;
	unsigned long freq_val, max_limit = 0;
	struct dev_pm_opp *opp_entry;

	val = readl_relaxed(hw->osm_hw_reg);
	dcvsh_get_frequency(val, max_limit);
	for_each_cpu(cpu, &hw->core_map) {
		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			pr_err("Error in get CPU%d device\n",
				cpumask_first(&hw->core_map));
			goto notify_exit;
		}

		pr_debug("CPU:%d max value read:%lu\n",
			cpumask_first(&hw->core_map),
			max_limit);
		freq_val = FREQ_KHZ_TO_HZ(max_limit);
		opp_entry = dev_pm_opp_find_freq_floor(cpu_dev, &freq_val);
		/*
		 * Hardware mitigation frequency can be lower than the lowest
		 * possible CPU frequency. In that case freq floor call will
		 * fail with -ERANGE and we need to match to the lowest
		 * frequency using freq_ceil.
		 */
		if (IS_ERR(opp_entry) && PTR_ERR(opp_entry) == -ERANGE) {
			opp_entry = dev_pm_opp_find_freq_ceil(cpu_dev,
								&freq_val);
			if (IS_ERR(opp_entry))
				dev_err(cpu_dev,
					"frequency:%lu. opp error:%ld\n",
					freq_val, PTR_ERR(opp_entry));
		}
		if (FREQ_HZ_TO_KHZ(freq_val) == hw->max_freq[idx]) {
			max_cpu_ct++;
			if (max_cpu_limit < hw->max_freq[idx])
				max_cpu_limit = hw->max_freq[idx];
			idx++;
			continue;
		}
		max_limit = FREQ_HZ_TO_KHZ(freq_val);
		break;
	}

	if (max_cpu_ct == cpumask_weight(&hw->core_map))
		max_limit = max_cpu_limit;
	sched_update_cpu_freq_min_max(&hw->core_map, 0, max_limit);
	pr_debug("CPU:%d max limit:%lu\n", cpumask_first(&hw->core_map),
			max_limit);
	trace_lmh_dcvs_freq(cpumask_first(&hw->core_map), max_limit);

notify_exit:
	hw->hw_freq_limit = max_limit;
	return max_limit;
}

static void limits_dcvs_poll(struct work_struct *work)
{
	unsigned long max_limit = 0;
	struct limits_dcvs_hw *hw = container_of(work,
					struct limits_dcvs_hw,
					freq_poll_work.work);
	int cpu_ct = 0, cpu = 0, idx = 0;

	mutex_lock(&hw->access_lock);
	if (hw->max_freq[0] == U32_MAX)
		limits_dcvs_get_freq_limits(hw);
	max_limit = limits_mitigation_notify(hw);
	for_each_cpu(cpu, &hw->core_map) {
		if (max_limit >= hw->max_freq[idx])
			cpu_ct++;
		idx++;
	}
	if (cpu_ct >= cpumask_weight(&hw->core_map)) {
		writel_relaxed(0xFF, hw->int_clr_reg);
		hw->is_irq_enabled = true;
		enable_irq(hw->irq_num);
	} else {
		mod_delayed_work(system_highpri_wq, &hw->freq_poll_work,
			 msecs_to_jiffies(LIMITS_POLLING_DELAY_MS));
	}
	mutex_unlock(&hw->access_lock);
}

static void lmh_dcvs_notify(struct limits_dcvs_hw *hw)
{
	if (hw->is_irq_enabled) {
		hw->is_irq_enabled = false;
		disable_irq_nosync(hw->irq_num);
		limits_mitigation_notify(hw);
		mod_delayed_work(system_highpri_wq, &hw->freq_poll_work,
			 msecs_to_jiffies(LIMITS_POLLING_DELAY_MS));
	}
}

static irqreturn_t lmh_dcvs_handle_isr(int irq, void *data)
{
	struct limits_dcvs_hw *hw = data;

	mutex_lock(&hw->access_lock);
	lmh_dcvs_notify(hw);
	mutex_unlock(&hw->access_lock);

	return IRQ_HANDLED;
}

static int limits_dcvs_write(uint32_t node_id, uint32_t fn,
			      uint32_t setting, uint32_t val, uint32_t val1,
			      bool enable_val1)
{
	int ret;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;
	uint32_t payload_len;

	payload_len = ((enable_val1) ? 6 : 5) * sizeof(uint32_t);
	payload = kzalloc(payload_len, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	payload[0] = fn; /* algorithm */
	payload[1] = 0; /* unused sub-algorithm */
	payload[2] = setting;
	payload[3] = enable_val1 ? 2 : 1; /* number of values */
	payload[4] = val;
	if (enable_val1)
		payload[5] = val1;

	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = payload_len;
	desc_arg.args[2] = LIMITS_NODE_DCVS;
	desc_arg.args[3] = node_id;
	desc_arg.args[4] = 0; /* version */
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL,
					SCM_VAL, SCM_VAL);

	dmac_flush_range(payload, (void *)payload + payload_len);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, LIMITS_DCVSH), &desc_arg);

	kfree(payload);

	return ret;
}

static int lmh_get_temp(void *data, int *val)
{
	/*
	 * LMH DCVSh hardware doesn't support temperature read.
	 * return a default value for the thermal core to aggregate
	 * the thresholds
	 */
	*val = LIMITS_TEMP_DEFAULT;

	return 0;
}

static int lmh_set_trips(void *data, int low, int high)
{
	struct limits_dcvs_hw *hw = (struct limits_dcvs_hw *)data;
	int ret = 0;

	if (high >= LIMITS_TEMP_HIGH_THRESH_MAX || low < 0) {
		pr_err("Value out of range low:%d high:%d\n",
				low, high);
		return -EINVAL;
	}

	/* Sanity check limits before writing to the hardware */
	if (low >= high)
		return -EINVAL;

	hw->temp_limits[LIMITS_TRIP_HI] = (uint32_t)high;
	hw->temp_limits[LIMITS_TRIP_ARM] = (uint32_t)low;

	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_ARM_THRESHOLD, low, 0, 0);
	if (ret)
		return ret;
	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_HI_THRESHOLD, high, 0, 0);
	if (ret)
		return ret;
	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_LOW_THRESHOLD,
				  high - LIMITS_LOW_THRESHOLD_OFFSET,
				  0, 0);
	if (ret)
		return ret;

	return ret;
}

static struct thermal_zone_of_device_ops limits_sensor_ops = {
	.get_temp   = lmh_get_temp,
	.set_trips  = lmh_set_trips,
};

static struct limits_dcvs_hw *get_dcvsh_hw_from_cpu(int cpu)
{
	struct limits_dcvs_hw *hw;

	list_for_each_entry(hw, &lmh_dcvs_hw_list, list) {
		if (cpumask_test_cpu(cpu, &hw->core_map))
			return hw;
	}

	return NULL;
}

static int enable_lmh(void)
{
	int ret = 0;
	struct scm_desc desc_arg;

	desc_arg.args[0] = 1;
	desc_arg.arginfo = SCM_ARGS(1, SCM_VAL);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, LIMITS_PROFILE_CHANGE),
			&desc_arg);
	if (ret) {
		pr_err("Error switching profile:[1]. err:%d\n", ret);
		return ret;
	}

	return ret;
}

static int lmh_set_max_limit(int cpu, u32 freq)
{
	struct limits_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);
	int ret = 0, cpu_idx, idx = 0;
	u32 max_freq = U32_MAX;

	if (!hw)
		return -EINVAL;

	mutex_lock(&hw->access_lock);
	for_each_cpu(cpu_idx, &hw->core_map) {
		if (cpu_idx == cpu)
		/*
		 * If there is no limits restriction for CPU scaling max
		 * frequency, vote for a very high value. This will allow
		 * the CPU to use the boost frequencies.
		 */
			hw->cdev_data[idx].max_freq =
				(freq == hw->max_freq[idx]) ? U32_MAX : freq;
		if (max_freq > hw->cdev_data[idx].max_freq)
			max_freq = hw->cdev_data[idx].max_freq;
		idx++;
	}
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_FREQ_CAP, max_freq,
				  (max_freq == U32_MAX) ? 0 : 1, 1);
	lmh_dcvs_notify(hw);
	mutex_unlock(&hw->access_lock);

	return ret;
}

static int lmh_set_min_limit(int cpu, u32 freq)
{
	struct limits_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);
	int cpu_idx, idx = 0, cpu_ct = 0;

	if (!hw)
		return -EINVAL;

	mutex_lock(&hw->access_lock);
	for_each_cpu(cpu_idx, &hw->core_map) {
		if (cpu_idx == cpu)
			hw->cdev_data[idx].min_freq = freq;
		if (hw->cdev_data[idx].min_freq <= hw->min_freq[idx])
			cpu_ct++;
		idx++;
	}
	if (cpu_ct < cpumask_weight(&hw->core_map))
		writel_relaxed(0x01, hw->min_freq_reg);
	else
		writel_relaxed(0x00, hw->min_freq_reg);
	mutex_unlock(&hw->access_lock);

	return 0;
}
static struct cpu_cooling_ops cd_ops = {
	.ceil_limit = lmh_set_max_limit,
	.floor_limit = lmh_set_min_limit,
};

static void register_cooling_device(struct work_struct *work)
{
	struct limits_dcvs_hw *hw;
	unsigned int cpu = 0, idx = 0;
	struct cpufreq_policy *policy = NULL;

	mutex_lock(&lmh_dcvs_list_access);
	list_for_each_entry(hw, &lmh_dcvs_hw_list, list) {
		if (hw->max_freq[0] == U32_MAX)
			limits_dcvs_get_freq_limits(hw);

		idx = 0;
		for_each_cpu(cpu, &hw->core_map) {
			if (hw->cdev_data[idx].cdev ||
				!cpumask_test_cpu(cpu, &hw->online_mask)) {
				idx++;
				continue;
			}
			policy = cpufreq_cpu_get(cpu);
			if (!policy) {
				pr_err("no policy for cpu%d\n", cpu);
				continue;
			}
			hw->cdev_data[idx].max_freq = U32_MAX;
			hw->cdev_data[idx].min_freq = 0;
			hw->cdev_data[idx].cdev =
					cpufreq_platform_cooling_register(
							policy, &cd_ops);
			if (IS_ERR_OR_NULL(hw->cdev_data[idx].cdev)) {
				pr_err("CPU:%u cdev register error:%ld\n",
					cpu, PTR_ERR(hw->cdev_data[idx].cdev));
				hw->cdev_data[idx].cdev = NULL;
			} else {
				pr_debug("CPU:%u cdev registered\n", cpu);
				hw->cdev_registered++;
			}
			idx++;
		}
	}
	mutex_unlock(&lmh_dcvs_list_access);
}

static int limits_cpu_online(unsigned int online_cpu)
{
	struct limits_dcvs_hw *hw = get_dcvsh_hw_from_cpu(online_cpu);

	if (!hw)
		return 0;
	cpumask_set_cpu(online_cpu, &hw->online_mask);
	if (hw->cdev_registered != cpumask_weight(&hw->core_map))
		queue_work(system_highpri_wq, &hw->cdev_register_work);

	return 0;
}

static void limits_isens_qref_init(struct platform_device *pdev,
					struct limits_dcvs_hw *hw,
					int idx, char *reg_name,
					char *reg_setting)
{
	int ret = 0;
	uint32_t settings[3];

	ret = of_property_read_u32_array(pdev->dev.of_node,
					reg_setting, settings, 3);
	if (ret) {
		if (ret == -EINVAL)
			return;

		pr_err("Regulator:isens_vref settings read error:%d\n",
				ret);
		return;
	}
	hw->isens_reg[idx] = devm_regulator_get(&pdev->dev, reg_name);
	if (IS_ERR_OR_NULL(hw->isens_reg[idx])) {
		pr_err("Regulator:isens_vref init error:%ld\n",
			PTR_ERR(hw->isens_reg[idx]));
		return;
	}
	ret = regulator_set_voltage(hw->isens_reg[idx], settings[0],
					settings[1]);
	if (ret) {
		pr_err("Regulator:isens_vref set voltage error:%d\n", ret);
		devm_regulator_put(hw->isens_reg[idx]);
		return;
	}
	ret = regulator_set_load(hw->isens_reg[idx], settings[2]);
	if (ret) {
		pr_err("Regulator:isens_vref set load error:%d\n", ret);
		devm_regulator_put(hw->isens_reg[idx]);
		return;
	}
	if (regulator_enable(hw->isens_reg[idx])) {
		pr_err("Failed to enable regulator:isens_vref\n");
		devm_regulator_put(hw->isens_reg[idx]);
		return;
	}
}

static void limits_isens_vref_ldo_init(struct platform_device *pdev,
					struct limits_dcvs_hw *hw)
{
	limits_isens_qref_init(pdev, hw, 0, "isens_vref_1p8",
				"isens-vref-1p8-settings");
	limits_isens_qref_init(pdev, hw, 1, "isens_vref_0p8",
				"isens-vref-0p8-settings");
}

static ssize_t
lmh_freq_limit_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct limits_dcvs_hw *hw = container_of(devattr,
						struct limits_dcvs_hw,
						lmh_freq_attr);

	return snprintf(buf, PAGE_SIZE, "%lu\n", hw->hw_freq_limit);
}

static int limits_dcvs_probe(struct platform_device *pdev)
{
	int ret;
	int affinity = -1;
	struct limits_dcvs_hw *hw;
	struct thermal_zone_device *tzdev;
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *cpu_node, *lmh_node;
	uint32_t request_reg, clear_reg, min_reg;
	int cpu, idx = 0;
	cpumask_t mask = { CPU_BITS_NONE };
	const __be32 *addr;
	bool no_cdev_register = false;

	for_each_possible_cpu(cpu) {
		cpu_node = of_cpu_device_node_get(cpu);
		if (!cpu_node)
			continue;
		lmh_node = of_parse_phandle(cpu_node, "qcom,lmh-dcvs", 0);
		if (lmh_node == dn) {
			/*set the cpumask*/
			cpumask_set_cpu(cpu, &(mask));
		}
		of_node_put(cpu_node);
		of_node_put(lmh_node);
	}

	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;
	/*
	 * We just init regulator if none of the CPUs have
	 * reference to our LMH node
	 */
	if (cpumask_empty(&mask)) {
		limits_isens_vref_ldo_init(pdev, hw);
		mutex_lock(&lmh_dcvs_list_access);
		INIT_LIST_HEAD(&hw->list);
		list_add_tail(&hw->list, &lmh_dcvs_hw_list);
		mutex_unlock(&lmh_dcvs_list_access);
		return 0;
	}

	hw->cdev_data = devm_kcalloc(&pdev->dev, cpumask_weight(&mask),
				   sizeof(*hw->cdev_data),
				   GFP_KERNEL);
	if (!hw->cdev_data)
		return -ENOMEM;

	cpumask_copy(&hw->core_map, &mask);
	cpumask_clear(&hw->online_mask);
	hw->cdev_registered = 0;
	for_each_cpu(cpu, &hw->core_map) {
		hw->cdev_data[idx].cdev = NULL;
		hw->cdev_data[idx].max_freq = U32_MAX;
		hw->cdev_data[idx].min_freq = 0;
		hw->max_freq[idx] = U32_MAX;
		hw->min_freq[idx] = 0;
		idx++;
	}
	ret = of_property_read_u32(dn, "qcom,affinity", &affinity);
	if (ret)
		return -ENODEV;
	switch (affinity) {
	case 0:
		hw->affinity = LIMITS_CLUSTER_0;
		break;
	case 1:
		hw->affinity = LIMITS_CLUSTER_1;
		break;
	default:
		return -EINVAL;
	}

	/* Check legcay LMH HW enablement is needed or not */
	if (of_property_read_bool(dn, "qcom,legacy-lmh-enable")) {
		/* Enable the thermal algorithm early */
		ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
			 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
		if (ret) {
			pr_err("Unable to enable THERM algo for cluster%d\n",
				affinity);
			return ret;
		}
		/* Enable the LMH outer loop algorithm */
		ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_CRNT,
			 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
		if (ret) {
			pr_err("Unable to enable CRNT algo for cluster%d\n",
				affinity);
			return ret;
		}
		/* Enable the Reliability algorithm */
		ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_REL,
			 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
		if (ret) {
			pr_err("Unable to enable REL algo for cluster%d\n",
				affinity);
			return ret;
		}
		/* Enable the BCL algorithm */
		ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_BCL,
			 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
		if (ret) {
			pr_err("Unable to enable BCL algo for cluster%d\n",
				affinity);
			return ret;
		}
		ret = enable_lmh();
		if (ret)
			return ret;
	}

	no_cdev_register = of_property_read_bool(dn,
				"qcom,no-cooling-device-register");

	addr = of_get_address(dn, 0, NULL, NULL);
	if (!addr) {
		pr_err("Property llm-base-addr not found\n");
		return -EINVAL;
	}
	clear_reg = be32_to_cpu(addr[0]) + LIMITS_CLUSTER_INT_CLR_OFFSET;
	min_reg = be32_to_cpu(addr[0]) + LIMITS_CLUSTER_MIN_FREQ_OFFSET;
	addr = of_get_address(dn, 1, NULL, NULL);
	if (!addr) {
		pr_err("Property osm-base-addr not found\n");
		return -EINVAL;
	}
	request_reg = be32_to_cpu(addr[0]) + LIMITS_CLUSTER_REQ_OFFSET;

	/*
	 * Setup virtual thermal zones for each LMH-DCVS hardware
	 * The sensor does not do actual thermal temperature readings
	 * but does support setting thresholds for trips.
	 * Let's register with thermal framework, so we have the ability
	 * to set low/high thresholds.
	 */
	hw->temp_limits[LIMITS_TRIP_HI] = INT_MAX;
	hw->temp_limits[LIMITS_TRIP_ARM] = 0;
	hw->hw_freq_limit = U32_MAX;
	snprintf(hw->sensor_name, sizeof(hw->sensor_name), "limits_sensor-%02d",
			affinity);
	tzdev = thermal_zone_of_sensor_register(&pdev->dev, 0, hw,
			&limits_sensor_ops);
	if (IS_ERR_OR_NULL(tzdev)) {
		/*
		 * Ignore error in case if thermal zone devicetree node is not
		 * defined for this lmh hardware.
		 */
		if (!tzdev || PTR_ERR(tzdev) != -ENODEV)
			return PTR_ERR(tzdev);
	}

	if (!no_cdev_register) {
		hw->min_freq_reg = devm_ioremap(&pdev->dev, min_reg, 0x4);
		if (!hw->min_freq_reg) {
			pr_err("min frequency enable register remap failed\n");
			ret = -ENOMEM;
			goto unregister_sensor;
		}
	}

	mutex_init(&hw->access_lock);
	INIT_WORK(&hw->cdev_register_work, register_cooling_device);
	INIT_DEFERRABLE_WORK(&hw->freq_poll_work, limits_dcvs_poll);
	hw->osm_hw_reg = devm_ioremap(&pdev->dev, request_reg, 0x4);
	if (!hw->osm_hw_reg) {
		pr_err("register remap failed\n");
		goto probe_exit;
	}
	hw->int_clr_reg = devm_ioremap(&pdev->dev, clear_reg, 0x4);
	if (!hw->int_clr_reg) {
		pr_err("interrupt clear reg remap failed\n");
		goto probe_exit;
	}

	hw->irq_num = of_irq_get(pdev->dev.of_node, 0);
	if (hw->irq_num < 0) {
		pr_err("Error getting IRQ number. err:%d\n", hw->irq_num);
		goto probe_exit;
	}
	hw->is_irq_enabled = true;
	ret = devm_request_threaded_irq(&pdev->dev, hw->irq_num, NULL,
		lmh_dcvs_handle_isr, IRQF_TRIGGER_HIGH | IRQF_ONESHOT
		| IRQF_NO_SUSPEND | IRQF_SHARED, hw->sensor_name, hw);
	if (ret) {
		pr_err("Error registering for irq. err:%d\n", ret);
		ret = 0;
		goto probe_exit;
	}
	limits_isens_vref_ldo_init(pdev, hw);
	hw->lmh_freq_attr.attr.name = "lmh_freq_limit";
	hw->lmh_freq_attr.show = lmh_freq_limit_show;
	hw->lmh_freq_attr.attr.mode = 0444;
	sysfs_attr_init(&hw->lmh_freq_attr.attr);
	device_create_file(&pdev->dev, &hw->lmh_freq_attr);

probe_exit:
	mutex_lock(&lmh_dcvs_list_access);
	INIT_LIST_HEAD(&hw->list);
	list_add_tail(&hw->list, &lmh_dcvs_hw_list);
	mutex_unlock(&lmh_dcvs_list_access);
	lmh_debug_register(pdev);

	if (!no_cdev_register) {
		ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
					"lmh-dcvs/cdev:online",
					limits_cpu_online, NULL);
		if (ret < 0)
			goto unregister_sensor;
		ret = 0;
	}

	return ret;

unregister_sensor:
	thermal_zone_of_sensor_unregister(&pdev->dev, tzdev);

	return ret;
}

static const struct of_device_id limits_dcvs_match[] = {
	{ .compatible = "qcom,msm-hw-limits", },
	{},
};

static struct platform_driver limits_dcvs_driver = {
	.probe		= limits_dcvs_probe,
	.driver		= {
		.name = KBUILD_MODNAME,
		.of_match_table = limits_dcvs_match,
	},
};
builtin_platform_driver(limits_dcvs_driver);
