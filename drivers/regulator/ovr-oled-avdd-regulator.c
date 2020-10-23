#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>

struct oled_avdd_config {
	struct regulator_desc desc;
	struct regulator_dev *dev;
	struct regulator_init_data *init_data;

	struct delayed_work avdd_en_work;
	struct delayed_work ocp_work;

	bool is_enabled;
	bool is_first_enable;
	bool ocp_triggered;

	int avdd_en_gpio;
	int pmi_en_gpio;

	int en_a_gpio;
	int en_b_gpio;

	int ocp_gpio;
};

static void avdd_en_work_func(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct oled_avdd_config *cfg = container_of(dw,
			struct oled_avdd_config, avdd_en_work);
	struct regulator_dev *rdev = cfg->dev;

	int en_a_val = gpio_get_value(cfg->en_a_gpio);
	int en_b_val = gpio_get_value(cfg->en_b_gpio);

	if (en_a_val == 1 && en_b_val == 1) {
		dev_dbg(&rdev->dev, "AVDD on\n");
		/* AVDD on */
		gpio_set_value(cfg->pmi_en_gpio, 0);
		gpio_set_value(cfg->avdd_en_gpio, 1);
		cfg->is_enabled = true;
	} else if (en_a_val == 0 && en_b_val == 0) {
		/* AVDD off */
		dev_dbg(&rdev->dev, "AVDD off\n");
		gpio_set_value(cfg->pmi_en_gpio, 1);
		gpio_set_value(cfg->avdd_en_gpio, 0);
	}
}

static void ocp_work_func(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct oled_avdd_config *cfg = container_of(dw,
			struct oled_avdd_config, ocp_work);

	/* AVDD off */
	gpio_set_value(cfg->pmi_en_gpio, 1);
	gpio_set_value(cfg->avdd_en_gpio, 0);
	cfg->is_enabled = false;
	cfg->ocp_triggered = true;
}

irqreturn_t avdd_en_int_handler(int irq, void *data)
{
	struct oled_avdd_config *cfg = (struct oled_avdd_config *)data;

	mod_delayed_work(system_highpri_wq, &cfg->avdd_en_work, 0);

	return IRQ_HANDLED;
}

irqreturn_t ocp_int_handler(int irq, void *data)
{
	struct oled_avdd_config *cfg = (struct oled_avdd_config *)data;

	mod_delayed_work(system_highpri_wq, &cfg->ocp_work, 0);

	return IRQ_HANDLED;
}

static int oled_avdd_enable(struct regulator_dev *rdev)
{
	struct oled_avdd_config *cfg = rdev_get_drvdata(rdev);
	bool enabled_on_boot = cfg->init_data->constraints.boot_on;
	int rc = 0;
	int avdd_en_val = 0, pmi_en_val = 1;

	dev_dbg(&rdev->dev, "enabling\n");

	if (cfg->ocp_triggered) {
		dev_err(&rdev->dev, "OCP triggered, enable not allowed\n");
		return -EINVAL;
	}

	if (enabled_on_boot && cfg->is_first_enable) {
		/*
		 * Set the state (see interrupt handler) if the regulator
		 * is enabled by the bootloader to begin with.
		 */
		avdd_en_val = 1;
		pmi_en_val = 0;
	}

	dev_dbg(&rdev->dev, "AVDD_EN: %d, PMI_EN: %d\n",
			avdd_en_val, pmi_en_val);

	rc = gpio_request(cfg->avdd_en_gpio, "avdd_enable");
	if (rc)
		dev_err(&rdev->dev, "Failed to request avdd_enable GPIO: %d\n",
				rc);

	rc = gpio_direction_output(cfg->avdd_en_gpio, avdd_en_val);
	if (rc)
		dev_err(&rdev->dev, "Failed to set avdd_enable GPIO: %d\n", rc);

	rc = gpio_request(cfg->pmi_en_gpio, "pmi_enable");
	if (rc)
		dev_err(&rdev->dev, "Failed to request pmi_enable GPIO: %d\n",
				rc);

	rc = gpio_direction_output(cfg->pmi_en_gpio, pmi_en_val);
	if (rc)
		dev_err(&rdev->dev, "Failed to set pmi_enable GPIO: %d\n", rc);

	enable_irq(gpio_to_irq(cfg->en_a_gpio));
	enable_irq(gpio_to_irq(cfg->en_b_gpio));
	if (gpio_is_valid(cfg->ocp_gpio))
		enable_irq(gpio_to_irq(cfg->ocp_gpio));

	if (cfg->is_first_enable)
		cfg->is_first_enable = false;

	dev_dbg(&rdev->dev, "enabled\n");

	return 0;
}

static int oled_avdd_disable(struct regulator_dev *rdev)
{
	struct oled_avdd_config *cfg = rdev_get_drvdata(rdev);

	dev_dbg(&rdev->dev, "disabling\n");

	disable_irq(gpio_to_irq(cfg->en_a_gpio));
	disable_irq(gpio_to_irq(cfg->en_b_gpio));
	if (gpio_is_valid(cfg->ocp_gpio))
		disable_irq(gpio_to_irq(cfg->ocp_gpio));

	gpio_free(cfg->avdd_en_gpio);
	gpio_free(cfg->pmi_en_gpio);

	cfg->is_enabled = false;

	dev_dbg(&rdev->dev, "disabled\n");

	return 0;
}

static int oled_avdd_is_enabled(struct regulator_dev *rdev)
{
	struct oled_avdd_config *cfg = rdev_get_drvdata(rdev);

	return cfg->is_enabled;
}

static struct regulator_ops oled_avdd_ops = {
	.enable = oled_avdd_enable,
	.disable = oled_avdd_disable,
	.is_enabled = oled_avdd_is_enabled,
};

static int oled_avdd_parse_dt(struct device *dev,
		struct oled_avdd_config *cfg)
{
	struct regulator_init_data *init_data;

	if (!dev->of_node) {
		dev_err(dev, "No device tree found\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(dev, dev->of_node, &cfg->desc);
	if (!init_data)
		return -ENOMEM;

	if (init_data->constraints.min_uV != init_data->constraints.max_uV) {
		dev_err(dev,
			 "Fixed regulator specified with variable voltages\n");
		return -EINVAL;
	}

	cfg->init_data = init_data;

	cfg->pmi_en_gpio = of_get_named_gpio(dev->of_node, "pmi-en-gpio", 0);
	if (!gpio_is_valid(cfg->pmi_en_gpio)) {
		dev_err(dev, "pmi_en GPIO is missing or invalid\n");
		return -EINVAL;
	}

	cfg->avdd_en_gpio = of_get_named_gpio(dev->of_node, "avdd-en-gpio", 0);
	if (!gpio_is_valid(cfg->avdd_en_gpio)) {
		dev_err(dev, "avdd_en GPIO is missing or invalid\n");
		return -EINVAL;
	}

	cfg->en_a_gpio = of_get_named_gpio(dev->of_node, "en-a-gpio", 0);
	if (!gpio_is_valid(cfg->en_a_gpio)) {
		dev_err(dev, "en_a GPIO is missing or invalid\n");
		return -EINVAL;
	}

	cfg->en_b_gpio = of_get_named_gpio(dev->of_node, "en-b-gpio", 0);
	if (!gpio_is_valid(cfg->en_b_gpio)) {
		dev_err(dev, "en_b GPIO is missing or invalid\n");
		return -EINVAL;
	}

	cfg->ocp_gpio = of_get_named_gpio(dev->of_node, "ocp-gpio", 0);
	if (!gpio_is_valid(cfg->ocp_gpio))
		dev_dbg(dev, "ocp-gpio not present, skipping\n");

	dev_dbg(dev, "Successfully parsed device tree\n");

	return 0;
}

static int oled_avdd_probe(struct platform_device *pdev)
{
	struct oled_avdd_config *drvdata;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	int rc;

	dev_dbg(&pdev->dev, "probing\n");

	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct oled_avdd_config),
			GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	rc = oled_avdd_parse_dt(&pdev->dev, drvdata);
	if (rc) {
		dev_err(&pdev->dev, "Error parsing DT entries\n");
		return -EINVAL;
	}

	init_data = drvdata->init_data;
	init_data->constraints.apply_uV = 0;

	drvdata->ocp_triggered = false;
	drvdata->is_enabled = init_data->constraints.boot_on;
	drvdata->is_first_enable = true;

	drvdata->desc.name = devm_kstrdup(&pdev->dev,
			init_data->constraints.name, GFP_KERNEL);
	if (drvdata->desc.name == NULL)
		return -ENOMEM;

	drvdata->desc.type = REGULATOR_VOLTAGE;
	drvdata->desc.owner = THIS_MODULE;
	drvdata->desc.ops = &oled_avdd_ops;
	drvdata->desc.enable_time = 0;
	drvdata->desc.supply_name = "vin";
	drvdata->desc.n_voltages = 1;
	drvdata->desc.fixed_uV = init_data->constraints.min_uV;

	cfg.dev = &pdev->dev;
	cfg.init_data = init_data;
	cfg.driver_data = drvdata;
	cfg.of_node = pdev->dev.of_node;

	INIT_DELAYED_WORK(&drvdata->avdd_en_work, avdd_en_work_func);

	rc = devm_request_irq(&pdev->dev,
			gpio_to_irq(drvdata->en_a_gpio),
			avdd_en_int_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_NO_THREAD | IRQF_NOBALANCING | IRQF_PERCPU,
			"INT_A_GPIO", drvdata);
	if (rc) {
		dev_err(&pdev->dev, "Failed to request IRQ INT_A_GPIO: %d\n",
				rc);
		return rc;
	}
	disable_irq(gpio_to_irq(drvdata->en_a_gpio));

	rc = devm_request_irq(&pdev->dev,
			gpio_to_irq(drvdata->en_b_gpio),
			avdd_en_int_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_NO_THREAD | IRQF_NOBALANCING | IRQF_PERCPU,
			"INT_B_GPIO", drvdata);
	if (rc) {
		dev_err(&pdev->dev, "Failed to request IRQ INT_B_GPIO: %d\n",
				rc);
		return rc;
	}
	disable_irq(gpio_to_irq(drvdata->en_b_gpio));

	if (gpio_is_valid(drvdata->ocp_gpio)) {
		INIT_DELAYED_WORK(&drvdata->ocp_work, ocp_work_func);

		rc = devm_request_irq(&pdev->dev,
				gpio_to_irq(drvdata->ocp_gpio),
				ocp_int_handler,
				IRQF_TRIGGER_FALLING | IRQF_NO_THREAD |
				IRQF_NOBALANCING | IRQF_PERCPU,
				"INT_OCP_GPIO", drvdata);
		if (rc) {
			dev_err(&pdev->dev,
					"Failed to request IRQ INT_OCP_GPIO: %d\n",
					rc);
			return rc;
		}
		disable_irq(gpio_to_irq(drvdata->ocp_gpio));
	}

	drvdata->dev = devm_regulator_register(&pdev->dev, &drvdata->desc,
			&cfg);
	if (IS_ERR(drvdata->dev)) {
		rc = PTR_ERR(drvdata->dev);
		dev_err(&pdev->dev, "Failed to register regulator: %d\n", rc);
		return rc;
	}

	platform_set_drvdata(pdev, drvdata);

	dev_dbg(&pdev->dev, "%s supplying %duV\n", drvdata->desc.name,
		drvdata->desc.fixed_uV);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id oled_avdd_of_match[] = {
	{ .compatible = "ovr-oled-avdd", },
	{},
};
MODULE_DEVICE_TABLE(of, oled_avdd_of_match);
#endif

static struct platform_driver regulator_oled_avdd_driver = {
	.probe		= oled_avdd_probe,
	.driver		= {
		.name		= "oled-avdd-reg",
		.of_match_table = of_match_ptr(oled_avdd_of_match),
	},
};

static int __init regulator_oled_avdd_init(void)
{
	return platform_driver_register(&regulator_oled_avdd_driver);
}
subsys_initcall(regulator_oled_avdd_init);

static void __exit regulator_oled_avdd_exit(void)
{
	platform_driver_unregister(&regulator_oled_avdd_driver);
}
module_exit(regulator_oled_avdd_exit);

MODULE_AUTHOR("Facebook Technologies");
MODULE_DESCRIPTION("Oculus OLED AVDD voltage regulator driver");
MODULE_LICENSE("GPL");
