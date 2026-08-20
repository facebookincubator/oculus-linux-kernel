// SPDX-License-Identifier: GPL-2.0
/*
 * Meta implementation of USB GPIO based connection detection driver
 *
 * Copyright (C) 2024 MediaTek Inc.
 *
 * Some code borrowed from drivers/usb/common/usb-conn-gpio.c
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/suspend.h>
#include <linux/usb/role.h>

#define USB_GPIO_DEB_MS		20	/* ms */
#define USB_GPIO_DEB_US		((USB_GPIO_DEB_MS) * 1000)	/* us */

#define meta_usb_conn_IRQF	\
	(IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT)

#define meta_usb_conn_TIMEOUT_SECS 6
#define ONE_SEC_IN_MS 1000

struct meta_usb_conn_info {
	struct device *dev;
	struct usb_role_switch *role_sw;
	enum usb_role last_role;
	struct delayed_work dw_det;
	unsigned long debounce_jiffies;

	struct gpio_desc *vbus_gpiod;
	struct gpio_desc *vchrg_debug_gpiod;
	int vbus_irq;
	int vchrg_debug_irq;
	u32 role_sw_delay_ms;
	u32 init_done_timeout_secs;
	bool init_done;
	bool connect_enabled;
	bool hotplug_enabled;
	bool vchrg_debug_detect;
	bool vbus_vchrg_override;
	bool pm_notifier_registered;

	struct power_supply_desc usb_desc;
	struct power_supply *usb_charger;
	struct power_supply_desc dc_desc;
	struct power_supply *dc_charger;
	struct device_node *usb_node;
	struct platform_device *usb_pdev;
	struct notifier_block pm_notifier;
};

static bool suspend_disable = false;

module_param_call(suspend_disable, param_set_bool, param_get_bool, &suspend_disable, 0644);
MODULE_PARM_DESC(suspend_disable, "boolean to configure whether suspend is disabled");

static void meta_usb_conn_set_usb_role(struct meta_usb_conn_info *info,
				  enum usb_role role)
{
	int ret;

	dev_info(info->dev, "role %d/%d, \n", info->last_role, role);

	ret = usb_role_switch_set_role(info->role_sw, role);
	if (ret) {
		dev_err(info->dev, "failed to set role: %d\n", ret);
		return;
	}
	info->last_role = role;
}

static void meta_usb_conn_detect_cable(struct work_struct *work)
{
	struct meta_usb_conn_info *info;
	int vbus, vchrg_debug;

	info = container_of(to_delayed_work(work), struct meta_usb_conn_info, dw_det);

	if (info->vchrg_debug_detect) {
		vchrg_debug = info->vchrg_debug_gpiod ? gpiod_get_value_cansleep(info->vchrg_debug_gpiod) : 0;
		dev_warn(info->dev, "vchrg_debug = %d\n", vchrg_debug);

		if (vchrg_debug || suspend_disable) {
			pm_wakeup_event(info->dev, info->role_sw_delay_ms * 2);
			dev_warn(
				info->dev,
				"Vchrg_debug is detected or USB suspend is disabled. Switch role to device\n");
			meta_usb_conn_set_usb_role(info, USB_ROLE_DEVICE);
		} else {
			dev_warn(info->dev,
				"Vchrg_debug is not detected and USB suspend is enabled. Switch role to none\n");
			meta_usb_conn_set_usb_role(info, USB_ROLE_NONE);
		}

	} else {
		/* check VBUS */
		vbus = info->vbus_gpiod ? gpiod_get_value_cansleep(info->vbus_gpiod) : 0;
		dev_warn(info->dev, "vbus = %d\n", vbus);

		if (!info->connect_enabled) {
			dev_warn(info->dev,
				"USB connection disabled, ignoring cable detection\n");
			meta_usb_conn_set_usb_role(info, USB_ROLE_NONE);
			goto exit;
		}

		if (suspend_disable) {
			dev_warn(
				info->dev,
				"USB suspend is disabled, setting role to device by default\n");
			meta_usb_conn_set_usb_role(info, USB_ROLE_DEVICE);
		}

		if (vbus) {
			if (info->hotplug_enabled) {
				/* Block suspend during reconfiguration */
				pm_wakeup_event(info->dev, info->role_sw_delay_ms * 2);
				meta_usb_conn_set_usb_role(info, USB_ROLE_NONE);
				msleep(info->role_sw_delay_ms);
			}
			meta_usb_conn_set_usb_role(info, USB_ROLE_DEVICE);
		}

		if (!vbus && !suspend_disable) {
			dev_info(info->dev,
					"Vbus is not detected. Switch role to none\n");
			meta_usb_conn_set_usb_role(info, USB_ROLE_NONE);
		}
	}

exit:
	power_supply_changed(info->usb_charger);
	power_supply_changed(info->dc_charger);
}

static void meta_usb_conn_queue_dwork(struct meta_usb_conn_info *info,
				 unsigned long delay)
{
	queue_delayed_work(system_power_efficient_wq, &info->dw_det, delay);
}

static irqreturn_t meta_usb_conn_isr(int irq, void *dev_id)
{
	struct meta_usb_conn_info *info = dev_id;

	meta_usb_conn_queue_dwork(info, info->debounce_jiffies);

	return IRQ_HANDLED;
}

static int meta_usb_conn_pm_notifier(struct notifier_block *nb,
				      unsigned long event, void *ptr)
{
	struct meta_usb_conn_info *info = container_of(nb, struct meta_usb_conn_info, pm_notifier);

	switch (event) {
	case PM_HIBERNATION_PREPARE:
		dev_info(info->dev, "PM notifier: Hibernation prepare - setting USB role to NONE\n");

		/* Cancel any pending work to avoid conflicts */
		cancel_delayed_work_sync(&info->dw_det);

		/* Proactively set USB role to NONE before hibernation */
		meta_usb_conn_set_usb_role(info, USB_ROLE_NONE);

		break;

	case PM_POST_HIBERNATION:
		dev_info(info->dev, "PM notifier: Post hibernation\n");
		
		/* Re-detect cable state after hibernation */
		meta_usb_conn_queue_dwork(info, 0);
		break;
	}

	return NOTIFY_DONE;
}

static enum power_supply_property charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int usb_charger_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct meta_usb_conn_info *info = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval =
			info->vbus_gpiod ?
				      gpiod_get_value_cansleep(info->vbus_gpiod) :
				      0;
		if (info->vchrg_debug_detect)
			val->intval =
				info->vchrg_debug_gpiod ?
				      gpiod_get_value_cansleep(info->vchrg_debug_gpiod) :
				      0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int meta_usb_conn_psy_register(struct meta_usb_conn_info *info)
{
	struct device *dev = info->dev;
	struct power_supply_desc *desc = &info->usb_desc;
	struct power_supply_config cfg = {
		.of_node = dev->of_node,
	};

	desc->name = "usb-charger";
	desc->properties = charger_properties;
	desc->num_properties = ARRAY_SIZE(charger_properties);
	desc->get_property = usb_charger_get_property;
	desc->type = POWER_SUPPLY_TYPE_USB;
	cfg.drv_data = info;
	info->usb_charger = devm_power_supply_register(dev, desc, &cfg);
	if (IS_ERR(info->usb_charger))
		dev_err(dev, "Unable to register charger\n");

	return PTR_ERR_OR_ZERO(info->usb_charger);
}

static int dc_charger_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct meta_usb_conn_info *info = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval =
			info->vbus_gpiod ?
				      gpiod_get_value_cansleep(info->vbus_gpiod) :
				      0;
		if (info->vbus_vchrg_override)
			val->intval |=
				info->vchrg_debug_gpiod ?
				      gpiod_get_value_cansleep(info->vchrg_debug_gpiod) :
				      0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int meta_dc_conn_psy_register(struct meta_usb_conn_info *info)
{
	struct device *dev = info->dev;
	struct power_supply_desc *desc = &info->dc_desc;
	struct power_supply_config cfg = {
		.of_node = dev->of_node,
	};

	desc->name = "dc-charger";
	desc->properties = charger_properties;
	desc->num_properties = ARRAY_SIZE(charger_properties);
	desc->get_property = dc_charger_get_property;
	desc->type = POWER_SUPPLY_TYPE_MAINS;
	cfg.drv_data = info;

	info->dc_charger = devm_power_supply_register(dev, desc, &cfg);
	if (IS_ERR(info->dc_charger))
		dev_err(dev, "Unable to register charger\n");

	return PTR_ERR_OR_ZERO(info->dc_charger);
}

static ssize_t suspend_disable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", suspend_disable);
}

static ssize_t suspend_disable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct meta_usb_conn_info *info =
		(struct meta_usb_conn_info *)dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for suspend disable: %s", buf);
		return result;
	}

	suspend_disable = temp;

	meta_usb_conn_queue_dwork(info, 0);

	return count;
}
static DEVICE_ATTR_RW(suspend_disable);

static ssize_t connect_enabled_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct meta_usb_conn_info *info =
		(struct meta_usb_conn_info *)dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", info->connect_enabled);
}

static ssize_t connect_enabled_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct meta_usb_conn_info *info =
		(struct meta_usb_conn_info *)dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for connect supported: %s", buf);
		return result;
	}

	info->connect_enabled = temp;

	/* Force re-evaluation */
	meta_usb_conn_queue_dwork(info, 0);
	return count;
}
static DEVICE_ATTR_RW(connect_enabled);

static ssize_t hotplug_enabled_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct meta_usb_conn_info *info =
		(struct meta_usb_conn_info *)dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", info->hotplug_enabled);
}

static ssize_t hotplug_enabled_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct meta_usb_conn_info *info =
		(struct meta_usb_conn_info *)dev_get_drvdata(dev);
	int result;
	bool temp;

	result = kstrtobool(buf, &temp);
	if (result < 0) {
		dev_err(dev, "Illegal input for connect supported: %s", buf);
		return result;
	}

	info->hotplug_enabled = temp;

	/* Force re-evaluation */
	meta_usb_conn_queue_dwork(info, 0);
	return count;
}
static DEVICE_ATTR_RW(hotplug_enabled);

static ssize_t vbus_active_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct meta_usb_conn_info *info =
		(struct meta_usb_conn_info *)dev_get_drvdata(dev);

	if (!info->vbus_gpiod){
		dev_err(dev, "vbus gpio undefined\n");
		return -ENOTSUPP;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", gpiod_get_value_cansleep(info->vbus_gpiod));
}
static DEVICE_ATTR_RO(vbus_active);

static ssize_t vchrg_debug_active_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct meta_usb_conn_info *info =
		(struct meta_usb_conn_info *)dev_get_drvdata(dev);

	if (!info->vchrg_debug_gpiod){
		dev_err(dev, "vchrg gpio undefined\n");
		return -ENOTSUPP;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", gpiod_get_value_cansleep(info->vchrg_debug_gpiod));
}
static DEVICE_ATTR_RO(vchrg_debug_active);

static struct attribute *meta_usb_conn_gpio_attrs[] = {
	&dev_attr_suspend_disable.attr,
	&dev_attr_connect_enabled.attr,
	&dev_attr_hotplug_enabled.attr,
	&dev_attr_vbus_active.attr,
	&dev_attr_vchrg_debug_active.attr,
	NULL,
};
ATTRIBUTE_GROUPS(meta_usb_conn_gpio);

static void meta_usb_conn_gpio_create_sysfs(struct meta_usb_conn_info *info)
{
	int result;

	result = sysfs_create_groups(&info->dev->kobj, meta_usb_conn_gpio_groups);
	if (result != 0)
		dev_err(info->dev, "Error creating sysfs entries: %d\n",
			result);
}

static int meta_usb_conn_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct meta_usb_conn_info *info;
	int ret = 0;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	info->usb_node =
		of_parse_phandle(info->dev->of_node, "usb-controller", 0);
	if (!info->usb_node) {
		dev_err(info->dev, "unable to get usb node\n");
		return -EINVAL;
	}

	info->usb_pdev = of_find_device_by_node(info->usb_node);
	if (!info->usb_pdev) {
		of_node_put(info->usb_node);
		dev_err(info->dev, "unable to get usb pdev\n");
		return -EINVAL;
	}

	info->vbus_gpiod = devm_gpiod_get_optional(dev, "vbus", GPIOD_IN);
	if (IS_ERR(info->vbus_gpiod))
		return PTR_ERR(info->vbus_gpiod);

	info->vchrg_debug_gpiod = devm_gpiod_get_optional(dev, "vchrg-debug-detect", GPIOD_IN);
	if (IS_ERR(info->vchrg_debug_gpiod))
		return PTR_ERR(info->vchrg_debug_gpiod);

	if (!info->vbus_gpiod && !info->vchrg_debug_gpiod) {
		dev_err(dev, "failed to get gpios\n");
		return -ENODEV;
	}

	if (info->vbus_gpiod)
		ret = gpiod_set_debounce(info->vbus_gpiod, USB_GPIO_DEB_US);
	if (!ret && info->vchrg_debug_gpiod)
		ret = gpiod_set_debounce(info->vchrg_debug_gpiod, USB_GPIO_DEB_US);
	if (ret < 0)
		info->debounce_jiffies = msecs_to_jiffies(USB_GPIO_DEB_MS);

	info->connect_enabled = 1;

	INIT_DELAYED_WORK(&info->dw_det, meta_usb_conn_detect_cable);

	ret = of_property_read_u32(dev->of_node, "role-sw-delay-ms",
				   &info->role_sw_delay_ms);
	if (ret < 0) {
		dev_err(dev, "failed to read role-sw-delay-ms from DT: %d\n",
			ret);
		return -ENODEV;
	}

	if (of_property_read_bool(dev->of_node, "meta,suspend-disable")) {
		suspend_disable = true;
		dev_dbg(dev, "%s: Suspend disable property is set \n",
			__func__);
	}

	if (of_property_read_bool(dev->of_node, "meta,enable-vchrg-debug-detect")) {
		info->vchrg_debug_detect = true;
		dev_dbg(dev, "%s: Enable vchrg debug detect property is set\n",
			__func__);
	}

	if (of_property_read_bool(dev->of_node, "meta,enable-vbus-vchrg-override")) {
		info->vbus_vchrg_override = true;
		dev_dbg(dev, "%s: Enable vbus vchrg override\n",
			__func__);
	}

	info->role_sw = usb_role_switch_get(dev);
	if (IS_ERR(info->role_sw)) {
		if (PTR_ERR(info->role_sw) != -EPROBE_DEFER)
			dev_err(dev, "failed to get role switch\n");

		return PTR_ERR(info->role_sw);
	}
	if (!info->role_sw)
		dev_err(dev, "obtained role switch is none\n");

	ret = meta_usb_conn_psy_register(info);
	if (ret)
		goto put_role_sw;

	ret = meta_dc_conn_psy_register(info);
	if (ret)
		goto put_role_sw;

	if (info->vbus_gpiod) {
		info->vbus_irq = gpiod_to_irq(info->vbus_gpiod);
		if (info->vbus_irq < 0) {
			dev_err(dev, "failed to get VBUS IRQ\n");
			ret = info->vbus_irq;
			goto put_role_sw;
		}

		ret = devm_request_threaded_irq(dev, info->vbus_irq, NULL,
						meta_usb_conn_isr, meta_usb_conn_IRQF,
						pdev->name, info);
		if (ret < 0) {
			dev_err(dev, "failed to request VBUS IRQ\n");
			goto put_role_sw;
		}
	}
	if (info->vchrg_debug_gpiod) {
		info->vchrg_debug_irq = gpiod_to_irq(info->vchrg_debug_gpiod);
		if (info->vchrg_debug_irq < 0) {
			dev_err(dev, "failed to get VCHRG debug IRQ\n");
			ret = info->vchrg_debug_irq;
			goto put_role_sw;
		}

		ret = devm_request_threaded_irq(dev, info->vchrg_debug_irq, NULL,
						meta_usb_conn_isr, meta_usb_conn_IRQF,
						pdev->name, info);
		if (ret < 0) {
			dev_err(dev, "failed to request VCHRG debug IRQ\n");
			goto put_role_sw;
		}
	}
	meta_usb_conn_gpio_create_sysfs(info);
	platform_set_drvdata(pdev, info);
	device_set_wakeup_capable(&pdev->dev, true);
	device_set_wakeup_enable(&pdev->dev, true);

	/* Perform initial detection */
	meta_usb_conn_queue_dwork(info, 0);

	/* Initialize PM notifier */
	info->pm_notifier.notifier_call = meta_usb_conn_pm_notifier;
	info->pm_notifier.priority = 0;
	register_pm_notifier(&info->pm_notifier);
	info->pm_notifier_registered = true;

	return 0;

put_role_sw:
	usb_role_switch_put(info->role_sw);
	return ret;
}

static int meta_usb_conn_remove(struct platform_device *pdev)
{
	struct meta_usb_conn_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&info->dw_det);
	
	/* Unregister PM notifier if it was registered */
	if (info->pm_notifier_registered) {
		unregister_pm_notifier(&info->pm_notifier);
	}

	usb_role_switch_put(info->role_sw);
	of_node_put(info->usb_node);
	platform_device_put(info->usb_pdev);

	return 0;
}

static int __maybe_unused meta_usb_conn_suspend(struct device *dev)
{
	struct meta_usb_conn_info *info = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (info->vbus_gpiod)
			enable_irq_wake(info->vbus_irq);
		if (info->vchrg_debug_gpiod)
			enable_irq_wake(info->vchrg_debug_irq);
		return 0;
	}

	if (info->vbus_gpiod)
		disable_irq(info->vbus_irq);
	if (info->vchrg_debug_gpiod)
		disable_irq(info->vchrg_debug_irq);
	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int __maybe_unused meta_usb_conn_resume(struct device *dev)
{
	struct meta_usb_conn_info *info = dev_get_drvdata(dev);

	if (device_may_wakeup(dev)) {
		if (info->vbus_gpiod)
			disable_irq_wake(info->vbus_irq);
		if (info->vchrg_debug_gpiod)
			disable_irq_wake(info->vchrg_debug_irq);
		return 0;
	}

	pinctrl_pm_select_default_state(dev);

	if (info->vbus_gpiod)
		enable_irq(info->vbus_irq);
	if (info->vchrg_debug_gpiod)
		enable_irq(info->vchrg_debug_irq);
	meta_usb_conn_queue_dwork(info, 0);

	return 0;
}

#ifdef CONFIG_HIBERNATION
static int meta_usb_conn_restore(struct device *dev)
{
	struct meta_usb_conn_info *info = dev_get_drvdata(dev);
	int ret;

	/* TODO:T215799356: IRQ settings are not restored properly after resume */
	if (info->vbus_gpiod) {
		info->vbus_irq = gpiod_to_irq(info->vbus_gpiod);
		if (info->vbus_irq < 0) {
			dev_err(dev, "%s: failed to get VBUS IRQ, %d\n",
					__func__, info->vbus_irq);
			goto exit;
		}
		ret = devm_request_threaded_irq(dev, info->vbus_irq,
				NULL, meta_usb_conn_isr, meta_usb_conn_IRQF,
				"usb_vbus", info);
		if (ret < 0)
			dev_err(dev, "%s: failed to req VCHRG debug IRQ, %d\n",
					__func__, ret);
			
	}
	if (info->vchrg_debug_gpiod) {
		info->vchrg_debug_irq = gpiod_to_irq(info->vchrg_debug_gpiod);
		if (info->vchrg_debug_irq < 0) {
			dev_err(dev, "%s: failed to get VCHRG debug IRQ, %d\n",
					__func__, info->vchrg_debug_irq);
			goto exit;
		}
		ret = devm_request_threaded_irq(dev, info->vchrg_debug_irq,
				NULL, meta_usb_conn_isr, meta_usb_conn_IRQF,
				"usb_conn_vchrg", info);
		if (ret < 0)
			dev_err(dev, "%s: failed to req VCHRG debug IRQ, %d\n",
					__func__, ret);
	}
exit:
	meta_usb_conn_queue_dwork(info, 0);
	return 0;
}

static int meta_usb_conn_freeze(struct device *dev)
{
	struct meta_usb_conn_info *info = dev_get_drvdata(dev);

	/* TODO:T215799356: IRQ settings are not restored properly after resume */
	if (info->vbus_irq)
		free_irq(info->vbus_irq, info);
	if (info->vchrg_debug_irq)
		free_irq(info->vchrg_debug_irq, info);

	return 0;
}
#else
#define meta_usb_conn_restore NULL
#define meta_usb_conn_freeze NULL
#endif

static const struct dev_pm_ops meta_usb_conn_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS (
	meta_usb_conn_suspend, meta_usb_conn_resume)
#ifdef CONFIG_HIBERNATION
	.freeze = meta_usb_conn_freeze,
	.thaw = meta_usb_conn_restore,
	.restore = meta_usb_conn_restore,
#endif
};

static const struct of_device_id meta_usb_conn_dt_match[] = {
	{ .compatible = "meta-gpio-usb-b-connector", },
	{ }
};
MODULE_DEVICE_TABLE(of, meta_usb_conn_dt_match);

static struct platform_driver meta_usb_conn_driver = {
	.probe		= meta_usb_conn_probe,
	.remove		= meta_usb_conn_remove,
	.driver		= {
		.name	= "meta-usb-conn-gpio",
		.pm	= &meta_usb_conn_pm_ops,
		.of_match_table = meta_usb_conn_dt_match,
	},
};

module_platform_driver(meta_usb_conn_driver);

MODULE_DESCRIPTION("USB GPIO based connection detection driver");
MODULE_LICENSE("GPL v2");
