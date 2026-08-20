// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/rt600-ctrl.h>
#include <linux/workqueue.h>
#include <linux/pinctrl/consumer.h>

struct rt600_ctrl_ctx {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_boot_normal;
	struct pinctrl_state *pin_boot_flashing;
	struct work_struct boot_work;
	struct work_struct reset_work;
	struct work_struct reset_work_spl;
	struct work_struct trigger_crash_work;
	struct work_struct trigger_assert_work;
	struct gpio_desc *crash_notify_gpiod;
	struct gpio_desc *rstn_gpiod;
	struct gpio_desc *nirq_gpiod;
	bool is_rt600;
	// port << 8 | pin
	uint32_t nirq_pinmap;
	enum rt600_boot_state boot_state;
	char *state_show;
	struct kernfs_node *crash_attr_node;
	atomic_t reset_complete;
	struct kernfs_node *reset_complete_attr_node;
	struct gpio_desc *assert_gpiod;
	bool has_assert_pin;

	/* Only for Oatmeal */
	bool is_oatmeal;
	unsigned int clk_en_gpio;
	unsigned int mcu_en_gpio;
	unsigned int mcu_alive_gpio;
	unsigned int pca_on_gpio;
	struct gpio_desc *soc_suspended_gpio;
	struct clk *clk_gen_xin;
	struct task_struct *boot_check_thread;
	struct completion boot_complete;
	struct completion shutdown_complete;
};

#define RT600_RESET_DELAY		100
#define RT600_CRASH_DELAY		100
#define RT600_WAKE_TIME_MS 		1000
#define RT600_BOOT_TIMEOUT_MS		1500
#define RT600_SHUTDOWN_TIMEOUT_MS	100

#define RT600_BOOT_STATE_NORMAL      "normal"
#define RT600_BOOT_STATE_FLASHING    "flashing"

bool rt600_ctrl_is_rt600 = true;
static BLOCKING_NOTIFIER_HEAD(state_subscribers);
static struct rt600_ctrl_ctx *rt600_ctx = NULL;

static void oatmeal_shutdown_sequence(struct rt600_ctrl_ctx *ctx);
static int oatmeal_init_sequence(struct rt600_ctrl_ctx *ctx);

static ssize_t boot_state_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", ctx->state_show);
}

static ssize_t boot_state_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	if (!strncmp(buf, RT600_BOOT_STATE_FLASHING,
		     strlen(RT600_BOOT_STATE_FLASHING))) {
		if (ctx->boot_state == flashing) {
			dev_info(ctx->dev, "Already in flashing mode...");
		} else {
			ctx->boot_state = flashing;
			ctx->state_show = RT600_BOOT_STATE_FLASHING;
			schedule_work(&ctx->boot_work);
		}
	} else if (!strncmp(buf, RT600_BOOT_STATE_NORMAL,
			    strlen(RT600_BOOT_STATE_NORMAL))) {
		if (ctx->boot_state == normal) {
			dev_info(ctx->dev, "Already in normal mode...");
		} else {
			ctx->boot_state = normal;
			ctx->state_show = RT600_BOOT_STATE_NORMAL;
			schedule_work(&ctx->boot_work);
		}
	}

	return count;
}
static DEVICE_ATTR_RW(boot_state);

static void toggle_reset(struct rt600_ctrl_ctx *ctx)
{
	if (!ctx->rstn_gpiod) {
		dev_warn(ctx->dev, "no rstn gpio for reset");
		return;
	}
	gpiod_set_value(ctx->rstn_gpiod, 1);
	msleep(RT600_RESET_DELAY);
	gpiod_set_value(ctx->rstn_gpiod, 0);
}

static void toggle_reset_spl(struct rt600_ctrl_ctx *ctx)
{
	if (!ctx->nirq_gpiod) {
		dev_warn(ctx->dev, "no nirq gpio for reset");
		return;
	}
	gpiod_direction_output(ctx->nirq_gpiod, 0);
	gpiod_set_value(ctx->nirq_gpiod, 0);
	toggle_reset(ctx);
	msleep(RT600_RESET_DELAY);
	gpiod_direction_input(ctx->nirq_gpiod);
}

static void toggle_nirq(struct rt600_ctrl_ctx *ctx)
{
	if (!ctx->nirq_gpiod) {
		dev_warn(ctx->dev, "no nirq gpio for reset");
		return;
	}
	gpiod_direction_output(ctx->nirq_gpiod, 0);
	gpiod_set_value(ctx->nirq_gpiod, 0);
	msleep(RT600_CRASH_DELAY);
	gpiod_set_value(ctx->nirq_gpiod, 1);
	gpiod_direction_input(ctx->nirq_gpiod);
}

static void toggle_assert_pin(struct rt600_ctrl_ctx *ctx)
{
	if (!ctx->assert_gpiod) {
		dev_warn(ctx->dev, "no assert gpio");
		return;
	}
	gpiod_set_value(ctx->assert_gpiod, 1);
	msleep(RT600_CRASH_DELAY);
	gpiod_set_value(ctx->assert_gpiod, 0);
}

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	long res;

	if (!kstrtol(buf, 0, &res) && res && atomic_xchg(&ctx->reset_complete, 0))
		schedule_work(&ctx->reset_work);

	return count;
}

static ssize_t reset_spl_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	long res;

	if (!kstrtol(buf, 0, &res) && res && atomic_xchg(&ctx->reset_complete, 0))
		schedule_work(&ctx->reset_work_spl);

	return count;
}

static ssize_t trigger_crash_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	long res;

	if (!kstrtol(buf, 0, &res) && res && atomic_xchg(&ctx->reset_complete, 0))
		schedule_work(&ctx->trigger_crash_work);

	return count;
}

static ssize_t trigger_assert_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	long res;

	/* On products without the assert pin, return success (no-op) */
	if (!ctx->has_assert_pin)
		return count;

	if (!kstrtol(buf, 0, &res) && res && atomic_xchg(&ctx->reset_complete, 0))
		schedule_work(&ctx->trigger_assert_work);

	return count;
}

static DEVICE_ATTR_WO(reset);
static DEVICE_ATTR_WO(reset_spl);
static DEVICE_ATTR_WO(trigger_crash);
static DEVICE_ATTR_WO(trigger_assert);


static ssize_t nirq_pinmap_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", ctx->nirq_pinmap);
}
static DEVICE_ATTR_RO(nirq_pinmap);

static ssize_t nirq_value_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", gpiod_get_value(ctx->nirq_gpiod));
}
static DEVICE_ATTR_RO(nirq_value);

static ssize_t crash_notify_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", gpiod_get_value(ctx->crash_notify_gpiod));
}
static DEVICE_ATTR_RO(crash_notify);

static ssize_t is_rt600_show(struct device *dev,
							 struct device_attribute *attr,
							 char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	if (ctx->is_rt600)
		return scnprintf(buf, PAGE_SIZE, "1");
	else
		return scnprintf(buf, PAGE_SIZE, "0");
}
static DEVICE_ATTR_RO(is_rt600);

static irqreturn_t crash_notify_irq_handler(int irq, void *data)
{
	struct rt600_ctrl_ctx *ctx = data;

	pm_wakeup_event(ctx->dev, RT600_WAKE_TIME_MS);

	if (ctx && ctx->crash_attr_node) {
		sysfs_notify_dirent(ctx->crash_attr_node);
	}

	return IRQ_HANDLED;
}

static ssize_t exit_shipmode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);
	struct pinctrl_state *pstate_default;
	int ret, irq_number;

	pstate_default = pinctrl_lookup_state(ctx->pinctrl,
			ctx->is_rt600 ? "default_rt600" : "default_rt700");
	if (IS_ERR_OR_NULL(pstate_default)) {
		dev_err(dev, "Failed to look up default pin state");
		return PTR_ERR_OR_ZERO(pstate_default) ?: -EINVAL;
	}
	ret = pinctrl_select_state(ctx->pinctrl, pstate_default);
	if (ret) {
		dev_err(dev, "Failed to select pinctrl state default");
		return ret;
	}

	if (ctx->crash_notify_gpiod) {
		irq_number = gpiod_to_irq(ctx->crash_notify_gpiod);
		enable_irq_wake(irq_number);
	}

	ret = oatmeal_init_sequence(ctx);
	if (ret) {
		dev_err(dev, "Failed to initialize the MCU");
		return ret;
	}

	return count;
}
static DEVICE_ATTR_WO(exit_shipmode);

static ssize_t enter_shipmode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	oatmeal_shutdown_sequence(ctx);

	return count;
}
static DEVICE_ATTR_WO(enter_shipmode);

int rt600_event_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&state_subscribers, nb);
}
EXPORT_SYMBOL(rt600_event_register);

int rt600_event_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&state_subscribers, nb);
}
EXPORT_SYMBOL(rt600_event_unregister);

static void boot_work(struct work_struct *work)
{
	int rc;
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, boot_work);

	switch (ctx->boot_state) {
	case normal:
		dev_info(ctx->dev, "Setting normal mode...");
		rc = pinctrl_select_state(ctx->pinctrl,
			ctx->pin_boot_normal);
		if (rc) {
			dev_err(ctx->dev, "Failed to select the normal boot state");
			return;
		}
		break;
	case flashing:
		dev_info(ctx->dev, "Setting flashing mode...");
		rc = pinctrl_select_state(ctx->pinctrl,
			ctx->pin_boot_flashing);
		if (rc) {
			dev_err(ctx->dev, "Failed to select the flashing boot state");
			return;
		}
		break;
	}

	toggle_reset(ctx);
	blocking_notifier_call_chain(&state_subscribers, ctx->boot_state, NULL);
}

static void reset_complete_notify(struct rt600_ctrl_ctx *ctx)
{
	atomic_set(&ctx->reset_complete, 1);
	if (ctx && ctx->reset_complete_attr_node) {
		sysfs_notify_dirent(ctx->reset_complete_attr_node);
	} else {
		dev_err(ctx->dev, "failed to notify reset_complete");
	}
}

static void reset_work(struct work_struct *work)
{
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, reset_work);

	dev_info(ctx->dev, "Resetting ...");
	toggle_reset(ctx);
	reset_complete_notify(ctx);
}

static void reset_work_spl(struct work_struct *work)
{
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, reset_work_spl);

	dev_info(ctx->dev, "Resetting to SPL...");
	toggle_reset_spl(ctx);
	reset_complete_notify(ctx);
}

static void trigger_crash_work(struct work_struct *work)
{
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, trigger_crash_work);

	dev_info(ctx->dev, "Triggering MCU crash...");
	toggle_nirq(ctx);
	reset_complete_notify(ctx);
}

static void trigger_assert_work_fn(struct work_struct *work)
{
	struct rt600_ctrl_ctx *ctx =
		container_of(work, struct rt600_ctrl_ctx, trigger_assert_work);

	dev_info(ctx->dev, "Triggering MCU assert...");
	toggle_assert_pin(ctx);
	reset_complete_notify(ctx);
}

int rt600_trigger_reset(void)
{
	if (!rt600_ctx) {
		pr_err("rt600_ctrl: driver not initialized\n");
		return -ENODEV;
	}

	if (!atomic_xchg(&rt600_ctx->reset_complete, 0)) {
		dev_warn(rt600_ctx->dev, "Reset already in progress\n");
		return -EBUSY;
	}

	schedule_work(&rt600_ctx->reset_work);
	return 0;
}
EXPORT_SYMBOL(rt600_trigger_reset);

int rt600_trigger_crash(void)
{
	if (!rt600_ctx) {
		pr_err("rt600_ctrl: driver not initialized\n");
		return -ENODEV;
	}

	if (!atomic_xchg(&rt600_ctx->reset_complete, 0)) {
		dev_warn(rt600_ctx->dev, "Reset already in progress\n");
		return -EBUSY;
	}

	schedule_work(&rt600_ctx->trigger_crash_work);
	return 0;
}
EXPORT_SYMBOL(rt600_trigger_crash);

static ssize_t reset_complete_show(struct device *dev,
			                       struct device_attribute *attr,
			                       char *buf)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&ctx->reset_complete));
}
static DEVICE_ATTR_RO(reset_complete);

static irqreturn_t mcu_alive_irq_handler(int irq, void *data)
{
	struct rt600_ctrl_ctx *ctx = data;
	int val;

	if (!ctx)
		return IRQ_HANDLED;

	val = gpio_get_value(ctx->mcu_alive_gpio);
	dev_dbg(ctx->dev, "MCU alive interrupt val=%d", val);

	if (val)
		complete(&ctx->boot_complete);
	else
		complete(&ctx->shutdown_complete);

	return IRQ_HANDLED;
}

/**
 * We need to power down the MCU gracefully to avoid a crash when the clock
 * generator gets turned off.
 */
static void oatmeal_shutdown_sequence(struct rt600_ctrl_ctx *ctx)
{
	struct pinctrl_state *pstate_shipmode;
	int ret, irq_number;

	reinit_completion(&ctx->shutdown_complete);

	/**
	 * Shutdown sequence:
	 * 1) De-assert the MCU_EN signal
	 * 2) Wait for the MCU to de-assert the MCU_ALIVE signal
	 * 3) Turn off the clock generator output
	 * 4) Turn off the clock generator input
	 * 5) Disable the crash notify IRQ
	 * 6) Set the GPIOs to input pull down
	 */

	dev_info(ctx->dev, "Shutting down the MCU");
	gpio_set_value(ctx->mcu_en_gpio, 0);
	/* Wait for the MCU to de-assert the alive signal */
	if (!wait_for_completion_timeout(&ctx->shutdown_complete,
					 msecs_to_jiffies(RT600_SHUTDOWN_TIMEOUT_MS)))
		dev_warn(ctx->dev, "MCU failed to shutdown after %d ms",
			 RT600_SHUTDOWN_TIMEOUT_MS);

	gpio_set_value(ctx->clk_en_gpio, 0);
	clk_disable_unprepare(ctx->clk_gen_xin);

	if (ctx->crash_notify_gpiod) {
		irq_number = gpiod_to_irq(ctx->crash_notify_gpiod);
		disable_irq_wake(irq_number);
	}

	pstate_shipmode = pinctrl_lookup_state(ctx->pinctrl, "shipmode");
	if (IS_ERR_OR_NULL(pstate_shipmode)) {
		dev_err(ctx->dev, "Failed to look up shipmode pin state");
		return;
	}
	ret = pinctrl_select_state(ctx->pinctrl, pstate_shipmode);
	if (ret) {
		dev_err(ctx->dev, "Failed to select pinctrl state shipmode");
		return;
	}
}

static int oatmeal_boot_check_fn(void *data)
{
	struct rt600_ctrl_ctx *ctx = data;

	/* Wait for the MCU to assert the alive signal */
	if (!wait_for_completion_timeout(&ctx->boot_complete,
					 msecs_to_jiffies(RT600_BOOT_TIMEOUT_MS))) {
		dev_warn(ctx->dev, "MCU failed to boot up after %d ms, resetting it",
			 RT600_BOOT_TIMEOUT_MS);
		toggle_reset(ctx);
	}

	dev_info(ctx->dev, "RT600 init sequence for Oatmeal complete");
	return 0;
}

static int oatmeal_init_sequence(struct rt600_ctrl_ctx *ctx)
{
	struct device *dev = ctx->dev;
	int ret;

	/**
	 * Init sequence:
	 * 1) Enable the input clock to the clock generator
	 * 2) Enable the clock generator output port
	 * 3) Assert the MCU_EN signal
	 * 4) Toggle the PCA_ON signal to power up the MCU
	 * 5) Wait for the MCU to assert the MCU_ALIVE signal
	 * 6) If that operation times out then reset the MCU
	 */

	ret = clk_prepare_enable(ctx->clk_gen_xin);
	if (ret) {
		dev_err(dev, "Failed to enable the clk_gen_xin clock");
		return ret;
	}

	gpio_direction_output(ctx->clk_en_gpio, 1);
	/* 5L2503 settling time is 2ms */
	udelay(2000);

	gpio_direction_output(ctx->mcu_en_gpio, 1);
	udelay(10);

	gpio_direction_output(ctx->pca_on_gpio, 1);
	/* PCA9420 ON deglitch time is 200us */
	udelay(200);
	gpio_set_value(ctx->pca_on_gpio, 0);

	/**
	 * The MCU can take a long time to boot so perform the boot detection
	 * async so we don't delay the boot up process.
	 */
	ctx->boot_check_thread = kthread_run(oatmeal_boot_check_fn, ctx,
					     "rt600_boot_check");
	if (IS_ERR(ctx->boot_check_thread)) {
		dev_err(dev, "Failed to start the boot check thread");
		return PTR_ERR(ctx->boot_check_thread);
	}

	return 0;
}

static int oatmeal_setup_chip(struct rt600_ctrl_ctx *ctx)
{
	struct device* dev = ctx->dev;
	int irq_number;
	int ret;

	init_completion(&ctx->boot_complete);
	init_completion(&ctx->shutdown_complete);

	ctx->clk_gen_xin = devm_clk_get(dev, "clk_generator_xin");
	if (IS_ERR_OR_NULL(ctx->clk_gen_xin)) {
		dev_err(dev, "failed to get clk_generator_xin clock");
		return PTR_ERR_OR_ZERO(ctx->clk_gen_xin) ?: -EINVAL;
	}

	ctx->clk_en_gpio = of_get_named_gpio(dev->of_node, "clk-en-gpio", 0);
	if (!gpio_is_valid(ctx->clk_en_gpio)) {
		dev_err(dev, "failed to look up the clk_en gpio");
		return -EINVAL;
	}
	if (devm_gpio_request(dev, ctx->clk_en_gpio, "clk_en_gpio")) {
		dev_err(dev, "failed to request clk_en gpio");
		return -EIO;
	}

	ctx->pca_on_gpio = of_get_named_gpio(dev->of_node, "pca-on-gpio", 0);
	if (!gpio_is_valid(ctx->pca_on_gpio)) {
		dev_err(dev, "failed to look up the pca_on gpio");
		return -EINVAL;
	}
	if (devm_gpio_request(dev, ctx->pca_on_gpio, "pca_on_gpio")) {
		dev_err(dev, "failed to request pca_on gpio");
		return -EIO;
	}

	ctx->mcu_en_gpio = of_get_named_gpio(dev->of_node, "mcu-en-gpio", 0);
	if (!gpio_is_valid(ctx->mcu_en_gpio)) {
		dev_err(dev, "failed to look up the mcu_en gpio");
		return -EINVAL;
	}
	if (devm_gpio_request(dev, ctx->mcu_en_gpio, "mcu_en_gpio")) {
		dev_err(dev, "failed to request mcu_en gpio");
		return -EIO;
	}

	ctx->mcu_alive_gpio = of_get_named_gpio(dev->of_node, "mcu-alive-gpio", 0);
	if (!gpio_is_valid(ctx->mcu_alive_gpio)) {
		dev_err(dev, "failed to look up the mcu_alive gpio");
		return -EINVAL;
	}
	if (devm_gpio_request(dev, ctx->mcu_alive_gpio, "mcu_alive_gpio")) {
		dev_err(dev, "failed to request mcu_alive gpio");
		return -EIO;
	}
	gpio_direction_input(ctx->mcu_alive_gpio);
	irq_number = gpio_to_irq(ctx->mcu_alive_gpio);
	ret = devm_request_irq(dev, irq_number, &mcu_alive_irq_handler,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "mcu_alive_gpio", ctx);
	if (ret) {
		dev_err(dev, "mcu_alive irq request failure");
		return ret;
	}

	ctx->soc_suspended_gpio = devm_gpiod_get(dev, "soc-suspended", 0);
	if (IS_ERR_OR_NULL(ctx->soc_suspended_gpio)) {
		ret = PTR_ERR(ctx->soc_suspended_gpio);
		dev_err(dev, "Failed to acquire soc-suspended gpio");
		return ret;
	}

	ret = gpiod_direction_output(ctx->soc_suspended_gpio, 1);
	if (ret) {
		dev_err(dev, "Failed to set direction to output for soc-suspended gpio");
		return ret;
	}

	ret = oatmeal_init_sequence(ctx);
	if (ret) {
		dev_err(ctx->dev, "Failed to initialize the Oatmeal MCU");
		return ret;
	}

	return 0;
}

#define NIRQ_PINMAP_COUNT 2
static int rt600_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rt600_ctrl_ctx *ctx =
		devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	int irq_number = 0;
	int rc = 0;
	uint32_t nirq_pinmap[NIRQ_PINMAP_COUNT];
	struct gpio_desc *is_rt600_gpiod;

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(ctx->pinctrl)) {
		dev_err(dev, "failed to get pinctrl");
		return PTR_ERR_OR_ZERO(ctx->pinctrl) ?: -EINVAL;
	}

	ctx->rstn_gpiod = devm_gpiod_get(dev, "rstn", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->rstn_gpiod))
		return dev_err_probe(dev, PTR_ERR(ctx->rstn_gpiod),
				     "failed to request rstn gpio\n");

	ctx->nirq_gpiod = devm_gpiod_get(dev, "nirq", GPIOD_IN);
	if (IS_ERR(ctx->nirq_gpiod))
		return dev_err_probe(dev, PTR_ERR(ctx->nirq_gpiod),
				     "failed to request nirq gpio\n");

	ctx->crash_notify_gpiod = devm_gpiod_get(dev, "crash-notify", GPIOD_IN);
	if (IS_ERR(ctx->crash_notify_gpiod))
		return dev_err_probe(dev, PTR_ERR(ctx->crash_notify_gpiod),
				     "failed to request crash_notify gpio\n");
	irq_number = gpiod_to_irq(ctx->crash_notify_gpiod);
	rc = devm_request_irq(dev, irq_number, &crash_notify_irq_handler, IRQF_TRIGGER_FALLING, "crash_notify_gpio", ctx);
	if (rc) {
		dev_err(dev, "crash_notify irq request failure");
		return rc;
	}

	rc = enable_irq_wake(irq_number);
	if (rc) {
		dev_err(dev, "Failed to set IRQ wake for `%d`", irq_number);
		return rc;
	}

	/* Assert/panic pin — only populated on greatwhite */
	ctx->assert_gpiod = devm_gpiod_get_optional(dev, "assert", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->assert_gpiod))
		return PTR_ERR(ctx->assert_gpiod);

	ctx->has_assert_pin = ctx->assert_gpiod != NULL;

	ctx->is_rt600 = true;

	if (device_property_read_bool(dev, "meta,rt700-override")) {
		rt600_ctrl_is_rt600 = false;
		ctx->is_rt600 = false;
	}

	if (ctx->is_rt600) {
		is_rt600_gpiod = devm_gpiod_get_optional(dev, "is-rt600", GPIOD_IN);
		if (IS_ERR(is_rt600_gpiod))
			return PTR_ERR(is_rt600_gpiod);
		if (is_rt600_gpiod) {
			ctx->is_rt600 = gpiod_get_value(is_rt600_gpiod);
			rt600_ctrl_is_rt600 = ctx->is_rt600;
			dev_info(dev, "is_rt600 is %d", ctx->is_rt600);
			devm_gpiod_put(dev, is_rt600_gpiod);
		}
	}

	ctx->pin_boot_normal = pinctrl_lookup_state(ctx->pinctrl, ctx->is_rt600 ? "default_rt600" : "default_rt700");
	if (IS_ERR(ctx->pin_boot_normal)) {
		rc = PTR_ERR(ctx->pin_boot_normal);
		/*
		 * On ACPI/emulator targets the pinctrl provider (pinctrl-virtio)
		 * may register its mappings after we first acquired our pinctrl
		 * handle, so create_pinctrl() found no matching maps and built an
		 * empty state list (-ENODEV here). Defer: devm releases the stale
		 * handle and the lookup is retried once the maps exist. On DT the
		 * state is present at first probe, so this path never defers.
		 */
		if (rc == -ENODEV)
			return dev_err_probe(dev, -EPROBE_DEFER, "default pin state not ready\n");
		return dev_err_probe(dev, rc, "failed to look up default pin state\n");
	}

	ctx->pin_boot_flashing = pinctrl_lookup_state(ctx->pinctrl, ctx->is_rt600 ? "flashing_rt600" : "flashing_rt700");
	if (IS_ERR(ctx->pin_boot_flashing)) {
		rc = PTR_ERR(ctx->pin_boot_flashing);
		if (rc == -ENODEV)
			return dev_err_probe(dev, -EPROBE_DEFER, "flashing pin state not ready\n");
		return dev_err_probe(dev, rc, "failed to look up flashing pin state\n");
	}

	// Some Dev boards can be either RT600 or RT700, so set defaults explicitly.
	dev_info(ctx->dev, "Setting normal mode...");
	rc = pinctrl_select_state(ctx->pinctrl, ctx->pin_boot_normal);

	if (ctx->is_rt600) {
		if (device_property_read_u32_array(dev, "nirq-mcu-map", nirq_pinmap, NIRQ_PINMAP_COUNT)) {
			dev_err(dev, "nirq pinmap not valid");
			return -EINVAL;
		}
	} else {
		if (device_property_read_u32_array(dev, "alt-nirq-mcu-map", nirq_pinmap, NIRQ_PINMAP_COUNT)) {
			dev_err(dev, "alternate nirq pinmap not valid");
			return -EINVAL;
		}
	}
	ctx->nirq_pinmap = nirq_pinmap[0] << 8 | nirq_pinmap[1];

	/* Init sequence for Oatmeal which uses a clock generator and an enable pin */
	if (device_property_read_bool(dev, "meta,oatmeal-init-sequence")) {
		ctx->is_oatmeal = true;
		rc = oatmeal_setup_chip(ctx);
		if (rc) {
			dev_err(dev, "failed to perform the RT600 init sequence for Oatmeal");
			return rc;
		}
	}

	ctx->boot_state = normal;
	ctx->state_show = RT600_BOOT_STATE_NORMAL;

	INIT_WORK(&ctx->boot_work, boot_work);
	INIT_WORK(&ctx->reset_work, reset_work);
	INIT_WORK(&ctx->reset_work_spl, reset_work_spl);
	INIT_WORK(&ctx->trigger_crash_work, trigger_crash_work);
	INIT_WORK(&ctx->trigger_assert_work, trigger_assert_work_fn);

	device_create_file(dev, &dev_attr_boot_state);
	device_create_file(dev, &dev_attr_crash_notify);
	device_create_file(dev, &dev_attr_nirq_value);
	device_create_file(dev, &dev_attr_nirq_pinmap);
	device_create_file(dev, &dev_attr_is_rt600);
	device_create_file(dev, &dev_attr_reset);
	device_create_file(dev, &dev_attr_reset_spl);
	device_create_file(dev, &dev_attr_reset_complete);
	device_create_file(dev, &dev_attr_trigger_crash);
	device_create_file(dev, &dev_attr_trigger_assert);

	if (ctx->is_oatmeal) {
		device_create_file(dev, &dev_attr_enter_shipmode);
		device_create_file(dev, &dev_attr_exit_shipmode);
	}

	ctx->crash_attr_node = sysfs_get_dirent(ctx->dev->kobj.sd, "crash_notify");
	if(!ctx->crash_attr_node) {
		dev_info(dev, "failed to get crash_notify kernel fs node");
	}

	atomic_set(&ctx->reset_complete, 1);
	ctx->reset_complete_attr_node = sysfs_get_dirent(ctx->dev->kobj.sd, "reset_complete");
	if(!ctx->reset_complete_attr_node) {
		dev_info(dev, "failed to get reset_complete kernel fs node");
	}

	platform_set_drvdata(pdev, ctx);
	rt600_ctx = ctx;

	rc = device_init_wakeup(ctx->dev, true);
	if (rc) {
		dev_err(dev, "Failed to init wakesource");
		return rc;
	}

	dev_info(dev, "rt600-ctrl probe success.\n");

	return rc;
}

static int rt600_ctrl_remove(struct platform_device *pdev)
{
	struct rt600_ctrl_ctx *ctx = platform_get_drvdata(pdev);

	rt600_ctx = NULL;

	if (device_init_wakeup(ctx->dev, false)) {
		dev_err(ctx->dev, "Failed to deinit wakesource");
	}

	device_remove_file(ctx->dev, &dev_attr_boot_state);
	device_remove_file(ctx->dev, &dev_attr_crash_notify);
	device_remove_file(ctx->dev, &dev_attr_nirq_value);
	device_remove_file(ctx->dev, &dev_attr_nirq_pinmap);
	device_remove_file(ctx->dev, &dev_attr_is_rt600);
	device_remove_file(ctx->dev, &dev_attr_reset);
	device_remove_file(ctx->dev, &dev_attr_reset_spl);
	device_remove_file(ctx->dev, &dev_attr_reset_complete);
	device_remove_file(ctx->dev, &dev_attr_trigger_crash);
	device_remove_file(ctx->dev, &dev_attr_trigger_assert);

	if (ctx->is_oatmeal) {
		device_remove_file(ctx->dev, &dev_attr_enter_shipmode);
		device_remove_file(ctx->dev, &dev_attr_exit_shipmode);
		oatmeal_shutdown_sequence(ctx);
	}

	devm_kfree(ctx->dev, ctx);

	return 0;
}

static void rt600_ctrl_shutdown(struct platform_device *pdev)
{
	struct rt600_ctrl_ctx *ctx = platform_get_drvdata(pdev);

	if (ctx->is_oatmeal)
		oatmeal_shutdown_sequence(ctx);
}

static int rt600_ctrl_suspend(struct device *dev)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	if (ctx->is_oatmeal)
		gpiod_set_value(ctx->soc_suspended_gpio, 0);
	return 0;
}

static int rt600_ctrl_resume(struct device *dev)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	if (ctx->is_oatmeal)
		gpiod_set_value(ctx->soc_suspended_gpio, 1);
	return 0;
}

static int rt600_ctrl_restore(struct device *dev)
{
	struct rt600_ctrl_ctx *ctx = dev_get_drvdata(dev);

	if (ctx->rstn_gpiod)
		gpiod_direction_output(ctx->rstn_gpiod, 0);

	if (ctx->nirq_gpiod)
		gpiod_direction_input(ctx->nirq_gpiod);

	if (ctx->crash_notify_gpiod)
		gpiod_direction_input(ctx->crash_notify_gpiod);

	if (ctx->has_assert_pin && ctx->assert_gpiod)
		gpiod_direction_output(ctx->assert_gpiod, 0);

	if (ctx->is_oatmeal)
		gpiod_direction_output(ctx->soc_suspended_gpio, 1);

	return 0;
}

static const struct dev_pm_ops rt600_ctrl_pm_ops = {
	.suspend = rt600_ctrl_suspend,
	.resume = rt600_ctrl_resume,
	.freeze = rt600_ctrl_suspend,
	.thaw = rt600_ctrl_resume,
	.poweroff = rt600_ctrl_suspend,
	.restore = rt600_ctrl_restore,
};

static const struct of_device_id rt600_ctrl_of_match[] = {
	{
		.compatible = "meta,rt600_ctrl",
	},
	{},
};

static struct platform_driver rt600_ctrl_driver = {
	.driver = {
		.name = "meta,rt600_ctrl",
		.of_match_table = rt600_ctrl_of_match,
		.pm = &rt600_ctrl_pm_ops,
	},
	.probe = rt600_ctrl_probe,
	.remove = rt600_ctrl_remove,
	.shutdown = rt600_ctrl_shutdown,
};

static int __init rt600_ctrl_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&rt600_ctrl_driver);
	if (rc) {
		pr_err("Unable to register RT600 ctrl driver:%d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit rt600_ctrl_driver_exit(void)
{
	platform_driver_unregister(&rt600_ctrl_driver);
}

module_init(rt600_ctrl_driver_init);
module_exit(rt600_ctrl_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for RT600 control");
