// SPDX-License-Identifier: GPL-2.0-only
/**
 * This driver is responsible for starting up Acropolis and setting up its
 * control signals on the Oatmeal platform.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>

#define ACRO_ENABLE_DELAY_MS 34
#define ACRO_DISABLE_DELAY_MS 10
#define ACRO_WAKE_TIME_MS 500

struct acropolis_ctrl {
	struct device *dev;
	struct clk *clk_gen_xin;
	struct gpio_desc *clk_en_gpio;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *mute_gpio;
	struct gpio_desc *pwron_gpio;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *time_trigger_gpio;
	bool uart0_mux_enabled;
	struct gpio_desc *uart0_mux_gpio;
	struct work_struct recovery_work;
	struct mutex recovery_lock;
	atomic64_t time_trigger_ns;
	struct kernfs_node *alive_attr_node;
	atomic64_t last_alive_time_ns;
	struct kernfs_node *enable_attr_node;
	atomic_t acro_enabled;
};

static irqreturn_t time_trigger_irq_handler(int irq, void *data)
{
	struct acropolis_ctrl *pdata = (struct acropolis_ctrl *)data;
	struct device *dev = pdata->dev;

	dev_dbg(dev, "Received time trigger IRQ\n");
	atomic64_set(&pdata->time_trigger_ns, ktime_get_boottime_ns());

	return IRQ_HANDLED;
}

/**
  * Handles Acro alive signal being de-asserted which signals that
  * the link is down and we need to notify userspace to handle it.
  */
static irqreturn_t acro_alive_irq_handler(int irq, void *data)
{
	struct acropolis_ctrl *pdata = (struct acropolis_ctrl *)data;
	struct device *dev = pdata->dev;

	/* Make sure userspace has enough time to handle this */
	pm_wakeup_event(dev, ACRO_WAKE_TIME_MS);

	dev_dbg(dev, "Acropolis alive signal de-asserted\n");
	atomic64_set(&pdata->last_alive_time_ns, ktime_get_boottime_ns());
	sysfs_notify_dirent(pdata->alive_attr_node);

	return IRQ_HANDLED;
}

static int gpio_store_helper(struct gpio_desc *gpio, const char *buf)
{
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	else if (val < 0 || val > 1)
		return -EINVAL;

	gpiod_set_value_cansleep(gpio, val);

	return 0;
}

/**
  * Init sequence:
  * 1) Enable Acropolis
  * 2) Enable the input clock to the clock generator
  * 3) Wait for a period of time
  * 4) Enable the clock generator output ports
  */
static int acropolis_ctrl_enable(struct device *dev)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int ret;

	if (atomic_cmpxchg(&pdata->acro_enabled, 0, 1) != 0) {
		dev_warn(dev, "Acropolis already enabled\n");
		return -EINVAL;
	}

	gpiod_set_value_cansleep(pdata->enable_gpio, 1);
	ret = clk_prepare_enable(pdata->clk_gen_xin);
	if (ret) {
		dev_err(dev, "Failed to enable the clk_gen_xin clock ret=%d\n",
			ret);
		return ret;
	}
	msleep(ACRO_ENABLE_DELAY_MS);
	gpiod_set_value_cansleep(pdata->clk_en_gpio, 1);
	sysfs_notify_dirent(pdata->enable_attr_node);

	dev_info(dev, "Acropolis enabled\n");

	return 0;
}

static int acropolis_ctrl_disable(struct device *dev)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);

	if (atomic_cmpxchg(&pdata->acro_enabled, 1, 0) != 1) {
		dev_warn(dev, "Acropolis already disabled\n");
		return -EINVAL;
	}

	gpiod_set_value_cansleep(pdata->enable_gpio, 0);
	gpiod_set_value_cansleep(pdata->clk_en_gpio, 0);
	clk_disable_unprepare(pdata->clk_gen_xin);
	sysfs_notify_dirent(pdata->enable_attr_node);

	dev_info(dev, "Acropolis disabled\n");

	return 0;
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pdata->enable_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	else if (val < 0 || val > 1)
		return -EINVAL;

	ret = val ? acropolis_ctrl_enable(dev) : acropolis_ctrl_disable(dev);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(enable);

static ssize_t mute_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pdata->mute_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t mute_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int ret;

	ret = gpio_store_helper(pdata->mute_gpio, buf);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(mute);

static ssize_t pwron_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pdata->pwron_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pwron_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int ret;

	ret = gpio_store_helper(pdata->pwron_gpio, buf);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(pwron);

static ssize_t recovery_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	bool busy = mutex_is_locked(&pdata->recovery_lock);

	return scnprintf(buf, PAGE_SIZE, "%s\n", busy ? "busy" : "idle");
}

static ssize_t recovery_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	if (val != 1)
		return -EINVAL;

	schedule_work(&pdata->recovery_work);

	return count;
}
static DEVICE_ATTR_RW(recovery);

static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	if (val != 1)
		return -EINVAL;

	gpiod_direction_output(pdata->reset_gpio, 0);

	/**
	  * Value here maps to active state and not the actual value of the pin
	  */
	gpiod_set_value(pdata->reset_gpio, 1);
	usleep_range(10000, 10500);
	gpiod_set_value(pdata->reset_gpio, 0);
	usleep_range(10000, 10500);

	/**
	  * The reset pin is controlled by Minerva. Its default state in SoC
	  * should be input no-pull. Soc only uses it for doing reset. Set it
	  * back to input after finishing the reset.
	  */
	gpiod_direction_input(pdata->reset_gpio);

	return count;
}
static DEVICE_ATTR_WO(reset);

static ssize_t uart0_mux_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int val = gpiod_get_value_cansleep(pdata->uart0_mux_gpio);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t uart0_mux_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	int ret;

	ret = gpio_store_helper(pdata->uart0_mux_gpio, buf);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(uart0_mux);

static ssize_t time_trigger_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);
	// Goal: at return, early will contain a time before the edge
	//       and late will contain a time after the edge.
	long long early, late;

	// Wait for the line to be low
	early = ktime_get_boottime_ns();
	late = early;
	while (gpiod_get_value(pdata->time_trigger_gpio)) {
	}

	// Wait for the line to be high
	do {
		long long prev_late = late;
		bool state = gpiod_get_value(pdata->time_trigger_gpio);
		late = ktime_get_boottime_ns();
		if (state) {
			break;
		}
		// The line is still low, so it was low at the time
		// read on the previous iteration.
		early = prev_late;
	} while (true);

	return scnprintf(buf, PAGE_SIZE, "%lld %lld %lld",
			 atomic64_read(&pdata->time_trigger_ns), early, late);
}
static DEVICE_ATTR_RO(time_trigger);

static ssize_t alive_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			 atomic64_read(&pdata->last_alive_time_ns));
}
static DEVICE_ATTR_RO(alive);

/* See the Oatmeal ERS for more details about the sequence */
static void recovery_entry_work_fn(struct work_struct *work)
{
	struct acropolis_ctrl *pdata =
		container_of(work, struct acropolis_ctrl, recovery_work);

	dev_info(pdata->dev, "Starting recovery entry sequence\n");

	mutex_lock(&pdata->recovery_lock);

	/**
	  * Value here maps to active state and not the actual value of the pin
	  */
	gpiod_set_value(pdata->pwron_gpio, 0);
	gpiod_set_value(pdata->enable_gpio, 0);
	gpiod_set_value(pdata->mute_gpio, 0);

	/* Wait for tAC0 */
	msleep(1000);

	gpiod_set_value(pdata->enable_gpio, 1);
	gpiod_set_value(pdata->pwron_gpio, 1);

	/* Wait for tAC1 + tAC2 */
	msleep(600);

	/* Toggle the mute pin twice, wait for tAC3 each time */
	gpiod_set_value(pdata->mute_gpio, 1);
	msleep(100);
	gpiod_set_value(pdata->mute_gpio, 0);
	msleep(100);
	gpiod_set_value(pdata->mute_gpio, 1);
	msleep(100);
	gpiod_set_value(pdata->mute_gpio, 0);

	/* Wait for the rest of tAC4 */
	msleep(15200);

	gpiod_set_value(pdata->pwron_gpio, 0);

	mutex_unlock(&pdata->recovery_lock);

	dev_info(pdata->dev, "Recovery entry sequence complete\n");
}

static int gpio_init_helper(struct device *dev, struct gpio_desc **desc,
			    const char *name, enum gpiod_flags flags)
{
	*desc = devm_gpiod_get(dev, name, flags);
	if (IS_ERR_OR_NULL(*desc)) {
		if (PTR_ERR(*desc) == -ENOENT)
			dev_warn(dev, "Could not find definition for %s gpio\n",
				 name);
		else
			dev_err(dev, "Failed to acquire %s gpio\n", name);
		return PTR_ERR(*desc);
	}
	return 0;
}

static int acropolis_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct acropolis_ctrl *pdata;
	int time_trigger_irq;
	int alive_irq;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = dev;

	pdata->clk_gen_xin = devm_clk_get(dev, "clk_gen_xin");
	if (IS_ERR_OR_NULL(pdata->clk_gen_xin)) {
		dev_err(dev, "Failed to get clk_gen_xin ret=%ld\n",
			PTR_ERR(pdata->clk_gen_xin));
		return PTR_ERR(pdata->clk_gen_xin);
	}

	/**
	  * The output value here maps to active state and not the actual value
	  * of the pin
	  */
	ret = gpio_init_helper(dev, &pdata->clk_en_gpio, "clk-en",
			       GPIOD_OUT_LOW);
	if (ret)
		return ret;
	ret = gpio_init_helper(dev, &pdata->enable_gpio, "enable",
			       GPIOD_OUT_LOW);
	if (ret)
		return ret;
	ret = gpio_init_helper(dev, &pdata->mute_gpio, "mute", GPIOD_OUT_LOW);
	if (ret)
		return ret;
	ret = gpio_init_helper(dev, &pdata->pwron_gpio, "pwron", GPIOD_OUT_LOW);
	if (ret)
		return ret;
	/**
	  * The reset pin is connected to Minerva and it should be controlled
	  * by Minerva. SoC should set it as Hi-Z (input no-pull).
	  */
	ret = gpio_init_helper(dev, &pdata->reset_gpio, "reset", GPIOD_IN);
	if (ret)
		return ret;

	/* Optional */
	ret = gpio_init_helper(dev, &pdata->uart0_mux_gpio, "uart0-mux",
			       GPIOD_OUT_LOW);
	if (ret == 0)
		pdata->uart0_mux_enabled = true;
	else if (ret == -ENOENT)
		pdata->uart0_mux_enabled = false;
	else
		return ret;

	/* Set up the time trigger IRQ */
	ret = gpio_init_helper(dev, &pdata->time_trigger_gpio, "time-trigger",
			       GPIOD_IN);
	if (ret)
		return ret;
	time_trigger_irq = gpiod_to_irq(pdata->time_trigger_gpio);
	ret = devm_request_irq(dev, time_trigger_irq, &time_trigger_irq_handler,
			       IRQF_TRIGGER_RISING, "acropolis_time_trigger",
			       pdata);
	if (ret) {
		dev_err(dev, "Failed to request time trigger IRQ ret=%d\n",
			ret);
		return ret;
	}

	/* Set up the Acro alive IRQ */
	alive_irq = of_irq_get_byname(dev->of_node, "acro-alive");
	if (alive_irq < 0) {
		dev_err(dev, "Failed to get the alive IRQ ret=%d\n", alive_irq);
		return alive_irq;
	}
	ret = devm_request_irq(dev, alive_irq, &acro_alive_irq_handler,
			       IRQF_TRIGGER_FALLING | IRQF_SHARED,
			       "acro_ctrl_alive_irq", pdata);
	if (ret) {
		dev_err(dev, "Failed to request the alive IRQ ret=%d\n", ret);
		return ret;
	}
	ret = enable_irq_wake(alive_irq);
	if (ret) {
		dev_err(dev, "Failed to enable wake for the alive IRQ");
		return ret;
	}

	mutex_init(&pdata->recovery_lock);
	INIT_WORK(&pdata->recovery_work, recovery_entry_work_fn);

	device_create_file(dev, &dev_attr_enable);
	pdata->enable_attr_node = sysfs_get_dirent(dev->kobj.sd, "enable");
	device_create_file(dev, &dev_attr_mute);
	device_create_file(dev, &dev_attr_pwron);
	device_create_file(dev, &dev_attr_recovery);
	device_create_file(dev, &dev_attr_reset);
	if (pdata->uart0_mux_enabled)
		device_create_file(dev, &dev_attr_uart0_mux);
	device_create_file(dev, &dev_attr_time_trigger);

	device_create_file(dev, &dev_attr_alive);
	pdata->alive_attr_node = sysfs_get_dirent(dev->kobj.sd, "alive");

	dev_set_drvdata(dev, pdata);

	ret = acropolis_ctrl_enable(dev);
	if (ret) {
		dev_err(dev, "Failed to initialize Acropolis ret=%d\n", ret);
		return ret;
	}

	dev_info(dev, "Acropolis setup complete\n");

	return 0;
}

static int acropolis_ctrl_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct acropolis_ctrl *pdata = dev_get_drvdata(dev);

	device_remove_file(dev, &dev_attr_enable);
	device_remove_file(dev, &dev_attr_mute);
	device_remove_file(dev, &dev_attr_pwron);
	device_remove_file(dev, &dev_attr_recovery);
	device_remove_file(dev, &dev_attr_reset);
	if (pdata->uart0_mux_enabled)
		device_remove_file(dev, &dev_attr_uart0_mux);
	device_remove_file(dev, &dev_attr_time_trigger);
	device_remove_file(dev, &dev_attr_alive);

	return 0;
}

static const struct of_device_id acropolis_ctrl_of_match[] = {
	{ .compatible = "meta,acropolis-ctrl" },
	{},
};

MODULE_DEVICE_TABLE(of, acropolis_ctrl_of_match);

static struct platform_driver acropolis_ctrl_driver = {
	 .driver = {
		 .name = "acropolis_ctrl",
		 .of_match_table = of_match_ptr(acropolis_ctrl_of_match),
	 },
	 .probe = acropolis_ctrl_probe,
	 .remove = acropolis_ctrl_remove,
 };

module_platform_driver(acropolis_ctrl_driver);

MODULE_DESCRIPTION("Acropolis control driver");
MODULE_LICENSE("GPL");
