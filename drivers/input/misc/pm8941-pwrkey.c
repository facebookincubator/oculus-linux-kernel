// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2011, 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2014, Sony Mobile Communications Inc.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>

#define PON_REV2			0x01

#define PON_SUBTYPE			0x05

#define PON_SUBTYPE_PRIMARY		0x01
#define PON_SUBTYPE_SECONDARY		0x02
#define PON_SUBTYPE_1REG		0x03
#define PON_SUBTYPE_GEN2_PRIMARY	0x04
#define PON_SUBTYPE_GEN2_SECONDARY	0x05
#define PON_SUBTYPE_GEN3_PBS		0x08
#define PON_SUBTYPE_GEN3_HLOS		0x09

#define PON_RT_STS			0x10
#define  PON_KPDPWR_N_SET		BIT(0)
#define  PON_RESIN_N_SET		BIT(1)
#define  PON_GEN3_RESIN_N_SET		BIT(6)
#define  PON_GEN3_KPDPWR_N_SET		BIT(7)

#define PON_PS_HOLD_RST_CTL		0x5a
#define PON_PS_HOLD_RST_CTL2		0x5b
#define  PON_PS_HOLD_ENABLE		BIT(7)
#define  PON_PS_HOLD_TYPE_MASK		0x0f
#define  PON_PS_HOLD_TYPE_SHUTDOWN	4
#define  PON_PS_HOLD_TYPE_HARD_RESET	7

#define PON_PULL_CTL			0x70
#define  PON_KPDPWR_PULL_UP		BIT(1)
#define  PON_RESIN_PULL_UP		BIT(0)

#define PON_DBC_CTL			0x71
#define  PON_DBC_DELAY_MASK		0x7

struct pm8941_data {
	unsigned int	pull_up_bit;
	unsigned int	status_bit;
	bool		supports_ps_hold_poff_config;
	bool		supports_debounce_config;
	bool		needs_sw_debounce;
	bool		has_pon_pbs;
	const char	*name;
	const char	*phys;
};

struct pm8941_pwrkey {
	struct device *dev;
	int irq;
	u32 baseaddr;
	u32 pon_pbs_baseaddr;
	struct regmap *regmap;
	struct input_dev *input;

	unsigned int revision;
	unsigned int subtype;
	struct notifier_block reboot_notifier;

	u32 code;
	u32 sw_debounce_time_us;
	ktime_t last_release_time;
	bool last_status;
	bool log_kpd_event;
	const struct pm8941_data *data;
};

static int pm8941_reboot_notify(struct notifier_block *nb,
				unsigned long code, void *unused)
{
	struct pm8941_pwrkey *pwrkey = container_of(nb, struct pm8941_pwrkey,
						    reboot_notifier);
	unsigned int enable_reg;
	unsigned int reset_type;
	int error;

	/* PMICs with revision 0 have the enable bit in same register as ctrl */
	if (pwrkey->revision == 0)
		enable_reg = PON_PS_HOLD_RST_CTL;
	else
		enable_reg = PON_PS_HOLD_RST_CTL2;

	error = regmap_update_bits(pwrkey->regmap,
				   pwrkey->baseaddr + enable_reg,
				   PON_PS_HOLD_ENABLE,
				   0);
	if (error)
		dev_err(pwrkey->dev,
			"unable to clear ps hold reset enable: %d\n",
			error);

	/*
	 * Updates of PON_PS_HOLD_ENABLE requires 3 sleep cycles between
	 * writes.
	 */
	usleep_range(100, 1000);

	switch (code) {
	case SYS_HALT:
	case SYS_POWER_OFF:
		reset_type = PON_PS_HOLD_TYPE_SHUTDOWN;
		break;
	case SYS_RESTART:
	default:
		reset_type = PON_PS_HOLD_TYPE_HARD_RESET;
		break;
	}

	error = regmap_update_bits(pwrkey->regmap,
				   pwrkey->baseaddr + PON_PS_HOLD_RST_CTL,
				   PON_PS_HOLD_TYPE_MASK,
				   reset_type);
	if (error)
		dev_err(pwrkey->dev, "unable to set ps hold reset type: %d\n",
			error);

	error = regmap_update_bits(pwrkey->regmap,
				   pwrkey->baseaddr + enable_reg,
				   PON_PS_HOLD_ENABLE,
				   PON_PS_HOLD_ENABLE);
	if (error)
		dev_err(pwrkey->dev, "unable to re-set enable: %d\n", error);

	return NOTIFY_DONE;
}

static irqreturn_t pm8941_pwrkey_irq(int irq, void *_data)
{
	struct pm8941_pwrkey *pwrkey = _data;
	unsigned int sts;
	int error;
	u64 elapsed_us;

	if (pwrkey->sw_debounce_time_us) {
		elapsed_us = ktime_us_delta(ktime_get(),
					    pwrkey->last_release_time);
		if (elapsed_us < pwrkey->sw_debounce_time_us) {
			dev_dbg(pwrkey->dev, "ignoring key event received after %llu us, debounce time=%u us\n",
				elapsed_us, pwrkey->sw_debounce_time_us);
			return IRQ_HANDLED;
		}
	}

	error = regmap_read(pwrkey->regmap,
			    pwrkey->baseaddr + PON_RT_STS, &sts);
	if (error)
		return IRQ_HANDLED;

	sts &= pwrkey->data->status_bit;

	if (pwrkey->sw_debounce_time_us && !sts)
		pwrkey->last_release_time = ktime_get();

	if (pwrkey->log_kpd_event)
		pr_info_ratelimited("PMIC input: KPDPWR status=0x%02x, KPDPWR_ON=%d\n",
			sts, (sts & PON_KPDPWR_N_SET));

	/*
	 * Simulate a press event in case a release event occurred without a
	 * corresponding press event.
	 */
	if (!pwrkey->last_status && !sts) {
		input_report_key(pwrkey->input, pwrkey->code, 1);
		input_sync(pwrkey->input);
	}
	pwrkey->last_status = sts;

	input_report_key(pwrkey->input, pwrkey->code, sts);
	input_sync(pwrkey->input);

	return IRQ_HANDLED;
}

static int pm8941_pwrkey_sw_debounce_init(struct pm8941_pwrkey *pwrkey)
{
	unsigned int val, addr;
	int error;

	if (pwrkey->data->has_pon_pbs && !pwrkey->pon_pbs_baseaddr) {
		dev_err(pwrkey->dev, "PON_PBS address missing, can't read HW debounce time\n");
		return 0;
	}

	if (pwrkey->pon_pbs_baseaddr)
		addr = pwrkey->pon_pbs_baseaddr + PON_DBC_CTL;
	else
		addr = pwrkey->baseaddr + PON_DBC_CTL;
	error = regmap_read(pwrkey->regmap, addr, &val);
	if (error)
		return error;

	if (pwrkey->subtype >= PON_SUBTYPE_GEN2_PRIMARY)
		pwrkey->sw_debounce_time_us = 2 * USEC_PER_SEC /
						(1 << (0xf - (val & 0xf)));
	else
		pwrkey->sw_debounce_time_us = 2 * USEC_PER_SEC /
						(1 << (0x7 - (val & 0x7)));

	dev_dbg(pwrkey->dev, "SW debounce time = %u us\n",
		pwrkey->sw_debounce_time_us);

	return 0;
}

static int __maybe_unused pm8941_pwrkey_suspend(struct device *dev)
{
	struct pm8941_pwrkey *pwrkey = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(pwrkey->irq);

	return 0;
}

static int __maybe_unused pm8941_pwrkey_resume(struct device *dev)
{
	struct pm8941_pwrkey *pwrkey = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(pwrkey->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(pm8941_pwr_key_pm_ops,
			 pm8941_pwrkey_suspend, pm8941_pwrkey_resume);

static int pm8941_pwrkey_probe(struct platform_device *pdev)
{
	struct pm8941_pwrkey *pwrkey;
	bool pull_up;
	struct device *parent;
	struct device_node *regmap_node;
	const __be32 *addr;
	u32 req_delay;
	unsigned int sts;
	int error;

	if (of_property_read_u32(pdev->dev.of_node, "debounce", &req_delay))
		req_delay = 15625;

	if (req_delay > 2000000 || req_delay == 0) {
		dev_err(&pdev->dev, "invalid debounce time: %u\n", req_delay);
		return -EINVAL;
	}

	pull_up = of_property_read_bool(pdev->dev.of_node, "bias-pull-up");

	pwrkey = devm_kzalloc(&pdev->dev, sizeof(*pwrkey), GFP_KERNEL);
	if (!pwrkey)
		return -ENOMEM;

	pwrkey->dev = &pdev->dev;
	pwrkey->data = of_device_get_match_data(&pdev->dev);
	if (!pwrkey->data) {
		dev_err(&pdev->dev, "match data not found\n");
		return -ENODEV;
	}

	parent = pdev->dev.parent;
	regmap_node = pdev->dev.of_node;
	pwrkey->regmap = dev_get_regmap(parent, NULL);
	if (!pwrkey->regmap) {
		regmap_node = parent->of_node;
		/*
		 * We failed to get regmap for parent. Let's see if we are
		 * a child of pon node and read regmap and reg from its
		 * parent.
		 */
		pwrkey->regmap = dev_get_regmap(parent->parent, NULL);
		if (!pwrkey->regmap) {
			dev_err(&pdev->dev, "failed to locate regmap\n");
			return -ENODEV;
		}
	}

	addr = of_get_address(regmap_node, 0, NULL, NULL);
	if (!addr) {
		dev_err(&pdev->dev, "reg property missing\n");
		return -EINVAL;
	}
	pwrkey->baseaddr = be32_to_cpu(*addr);

	if (pwrkey->data->has_pon_pbs) {
		/* PON_PBS base address is optional */
		addr = of_get_address(regmap_node, 1, NULL, NULL);
		if (addr)
			pwrkey->pon_pbs_baseaddr = be32_to_cpu(*addr);
	}

	pwrkey->irq = platform_get_irq(pdev, 0);
	if (pwrkey->irq < 0)
		return pwrkey->irq;

	error = regmap_read(pwrkey->regmap, pwrkey->baseaddr + PON_REV2,
			    &pwrkey->revision);
	if (error) {
		dev_err(&pdev->dev, "failed to read revision: %d\n", error);
		return error;
	}

	error = regmap_read(pwrkey->regmap, pwrkey->baseaddr + PON_SUBTYPE,
			    &pwrkey->subtype);
	if (error) {
		dev_err(&pdev->dev, "failed to read subtype: %d\n", error);
		return error;
	}

	error = of_property_read_u32(pdev->dev.of_node, "linux,code",
				     &pwrkey->code);
	if (error) {
		dev_dbg(&pdev->dev,
			"no linux,code assuming power (%d)\n", error);
		pwrkey->code = KEY_POWER;
	}

	pwrkey->input = devm_input_allocate_device(&pdev->dev);
	if (!pwrkey->input) {
		dev_dbg(&pdev->dev, "unable to allocate input device\n");
		return -ENOMEM;
	}

	input_set_capability(pwrkey->input, EV_KEY, pwrkey->code);

	pwrkey->input->name = pwrkey->data->name;
	pwrkey->input->phys = pwrkey->data->phys;

	if (pwrkey->data->supports_debounce_config) {
		req_delay = (req_delay << 6) / USEC_PER_SEC;
		req_delay = ilog2(req_delay);

		error = regmap_update_bits(pwrkey->regmap,
					   pwrkey->baseaddr + PON_DBC_CTL,
					   PON_DBC_DELAY_MASK,
					   req_delay);
		if (error) {
			dev_err(&pdev->dev, "failed to set debounce: %d\n",
				error);
			return error;
		}
	}

	if (pwrkey->data->needs_sw_debounce) {
		error = pm8941_pwrkey_sw_debounce_init(pwrkey);
		if (error)
			return error;
	}

	if (pwrkey->data->pull_up_bit) {
		error = regmap_update_bits(pwrkey->regmap,
					   pwrkey->baseaddr + PON_PULL_CTL,
					   pwrkey->data->pull_up_bit,
					   pull_up ? pwrkey->data->pull_up_bit :
						     0);
		if (error) {
			dev_err(&pdev->dev, "failed to set pull: %d\n", error);
			return error;
		}
	}

	pwrkey->log_kpd_event = of_property_read_bool(pdev->dev.of_node, "qcom,log-kpd-event");

	if (pwrkey->log_kpd_event) {
		error = regmap_read(pwrkey->regmap,
				    pwrkey->baseaddr + PON_RT_STS, &sts);
		if (error)
			dev_err(&pdev->dev, "failed to read PON_RT_STS rc=%d\n", error);
		else
			pr_info("KPDPWR status at init=0x%02x, KPDPWR_ON=%d\n",
				sts, (sts & PON_KPDPWR_N_SET));
	}

	error = devm_request_threaded_irq(&pdev->dev, pwrkey->irq,
					  NULL, pm8941_pwrkey_irq,
					  IRQF_ONESHOT,
					  pwrkey->data->name, pwrkey);
	if (error) {
		dev_err(&pdev->dev, "failed requesting IRQ: %d\n", error);
		return error;
	}

	error = input_register_device(pwrkey->input);
	if (error) {
		dev_err(&pdev->dev, "failed to register input device: %d\n",
			error);
		return error;
	}

	if (pwrkey->data->supports_ps_hold_poff_config) {
		pwrkey->reboot_notifier.notifier_call = pm8941_reboot_notify,
		error = register_reboot_notifier(&pwrkey->reboot_notifier);
		if (error) {
			dev_err(&pdev->dev, "failed to register reboot notifier: %d\n",
				error);
			return error;
		}
	}

	platform_set_drvdata(pdev, pwrkey);
	device_init_wakeup(&pdev->dev, 1);

	return 0;
}

static int pm8941_pwrkey_remove(struct platform_device *pdev)
{
	struct pm8941_pwrkey *pwrkey = platform_get_drvdata(pdev);

	if (pwrkey->data->supports_ps_hold_poff_config)
		unregister_reboot_notifier(&pwrkey->reboot_notifier);

	return 0;
}

static const struct pm8941_data pwrkey_data = {
	.pull_up_bit = PON_KPDPWR_PULL_UP,
	.status_bit = PON_KPDPWR_N_SET,
	.name = "pm8941_pwrkey",
	.phys = "pm8941_pwrkey/input0",
	.supports_ps_hold_poff_config = true,
	.supports_debounce_config = true,
	.needs_sw_debounce = true,
	.has_pon_pbs = false,
};

static const struct pm8941_data resin_data = {
	.pull_up_bit = PON_RESIN_PULL_UP,
	.status_bit = PON_RESIN_N_SET,
	.name = "pm8941_resin",
	.phys = "pm8941_resin/input0",
	.supports_ps_hold_poff_config = true,
	.supports_debounce_config = true,
	.needs_sw_debounce = true,
	.has_pon_pbs = false,
};

static const struct pm8941_data pon_gen3_pwrkey_data = {
	.status_bit = PON_GEN3_KPDPWR_N_SET,
	.name = "pmic_pwrkey",
	.phys = "pmic_pwrkey/input0",
	.supports_ps_hold_poff_config = false,
	.supports_debounce_config = false,
	.needs_sw_debounce = true,
	.has_pon_pbs = true,
};

static const struct pm8941_data pon_gen3_resin_data = {
	.status_bit = PON_GEN3_RESIN_N_SET,
	.name = "pmic_resin",
	.phys = "pmic_resin/input0",
	.supports_ps_hold_poff_config = false,
	.supports_debounce_config = false,
	.needs_sw_debounce = true,
	.has_pon_pbs = true,
};

static const struct of_device_id pm8941_pwr_key_id_table[] = {
	{ .compatible = "qcom,pm8941-pwrkey", .data = &pwrkey_data },
	{ .compatible = "qcom,pm8941-resin", .data = &resin_data },
	{ .compatible = "qcom,pmk8350-pwrkey", .data = &pon_gen3_pwrkey_data },
	{ .compatible = "qcom,pmk8350-resin", .data = &pon_gen3_resin_data },
	{ }
};
MODULE_DEVICE_TABLE(of, pm8941_pwr_key_id_table);

static struct platform_driver pm8941_pwrkey_driver = {
	.probe = pm8941_pwrkey_probe,
	.remove = pm8941_pwrkey_remove,
	.driver = {
		.name = "pm8941-pwrkey",
		.pm = &pm8941_pwr_key_pm_ops,
		.of_match_table = of_match_ptr(pm8941_pwr_key_id_table),
	},
};
module_platform_driver(pm8941_pwrkey_driver);

MODULE_DESCRIPTION("PM8941 Power Key driver");
MODULE_LICENSE("GPL v2");
