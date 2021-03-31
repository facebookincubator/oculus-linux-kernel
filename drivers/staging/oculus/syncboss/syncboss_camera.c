#include <linux/platform_device.h>
#include <media/msm_cam_sensor.h>
#include <soc/qcom/camera2.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>

#define VS1_GPIO_ALLOC 0x8000000UL

#define is_configured(_gpio) ((_gpio)&VS1_GPIO_ALLOC)
#define to_gpio(_gpio) ((_gpio) & ~VS1_GPIO_ALLOC)

#define gpio_safe_release(_gpio)			\
	do {						\
		if (is_configured((_gpio))) {		\
			gpio_free(to_gpio((_gpio)));	\
			(_gpio) = 0;			\
		}					\
	} while (0)

/* The name of the Vdig supply (we need to treat it specially) */
#define REGULATOR_VDIG_NAME "cam_vdig"

extern int msm_camera_get_clk_info(struct platform_device *pdev,
		struct msm_cam_clk_info **clk_info,
		struct clk ***clk_ptr, size_t *num_clk);
extern int msm_camera_clk_enable(struct device *dev,
		struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable);
extern int camera_init_v4l2(struct device *dev, unsigned int *session);
static bool androidboot_mode_charger;

struct vs1_gpio {
	const char *name;
	int out_value;
	unsigned int *target;
};

struct sensor_ctrl {
	struct list_head list;
	struct platform_device *pdev;

	uint32_t id;
	bool enabled;

	struct camera_vreg_t *vregs;
	int num_vregs;
	void **vreg_handles;

	unsigned int powerdown;

	struct {
		struct msm_cam_clk_info *clk_info;
		struct clk **clk_ptr;
		size_t num_clk;
	} clocks;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_suspend;

	struct device_node *of_node;
};

static LIST_HEAD(sensor_ctrl_list);
static int num_sensors;
static DEFINE_MUTEX(sensor_ctrl_list_mutex);

static int __init get_androidboot_mode(char *str)
{
	if ((str != NULL) && !strncmp("charger", str, sizeof("charger")))
		androidboot_mode_charger = true;
	return 1;
}
__setup("androidboot.mode=", get_androidboot_mode);

static int vs1_configure_gpio(struct device *dev,
		struct vs1_gpio *table)
{
	struct device_node *of_node = dev->of_node;
	struct vs1_gpio *vg;
	int rc = 0;

	for (vg = table; vg->name != NULL; vg++) {
		int gpio_num;

		gpio_num = rc = of_get_named_gpio(of_node, vg->name, 0);
		if (rc < 0) {
			dev_err(dev, "failed to get '%s': %d", vg->name, rc);
			goto fail;
		}

		rc = gpio_request(gpio_num, vg->name);
		if (rc < 0) {
			dev_err(dev, "failed to request GPIO '%s' (%d): %d",
					vg->name, gpio_num, rc);
			goto fail;
		}

		if (vg->out_value >= 0) {
			rc = gpio_direction_output(gpio_num, vg->out_value);
			if (rc < 0) {
				dev_err(dev, "failed to configure GPIO '%s' (%d): %d",
						vg->name, gpio_num, rc);
				gpio_free(gpio_num);
				goto fail;
			}

		} else {
			rc = gpio_direction_input(gpio_num);
			if (rc < 0) {
				dev_err(dev, "failed to configure GPIO '%s' (%d): %d",
						vg->name, gpio_num, rc);
				gpio_free(gpio_num);
				goto fail;
			}
		}

#ifdef DEBUG
		rc = gpio_export(gpio_num, 1);
		if (rc < 0) {
			dev_err(dev, "failed to export GPIO '%s' (%d): %d",
					vg->name, gpio_num, rc);
		}
#endif
		dev_dbg(dev, "configured GPIO '%s' (%d)\n", vg->name, gpio_num);
		*vg->target = (unsigned)gpio_num | VS1_GPIO_ALLOC;
	}

	return 0;

fail:
	for (vg = table; vg->name != NULL; vg++)
		gpio_safe_release(*vg->target);

	return rc;
}

static int32_t get_pin_control(struct device *dev,
		struct pinctrl **ppinctrl,
		struct pinctrl_state **active,
		struct pinctrl_state **inactive,
		const char *active_name,
		const char *inactive_name)
{
	int32_t rc = 0;
	struct pinctrl *pinctrl = NULL;

	if (!dev || !ppinctrl || !active || !inactive || !active_name ||
			!inactive_name)
		return -EINVAL;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		rc = PTR_ERR_OR_ZERO(pinctrl) ?: -EINVAL;
		dev_err(dev, "failed to look up pinctrl");
		goto error;
	}

	*active = pinctrl_lookup_state(pinctrl, active_name);
	if (IS_ERR_OR_NULL(*active)) {
		rc = PTR_ERR_OR_ZERO(*active) ?: -EINVAL;
		dev_err(dev, "failed to look up default pin state");
		goto free_pinctrl;
	}

	*inactive = pinctrl_lookup_state(pinctrl, inactive_name);
	if (IS_ERR_OR_NULL(*inactive)) {
		rc = PTR_ERR_OR_ZERO(*inactive) ?: -EINVAL;
		dev_err(dev, "falied to look up suspend pin state");
		goto free_pinctrl;
	}

	*ppinctrl = pinctrl;

	return rc;

free_pinctrl:
	devm_pinctrl_put(pinctrl);
error:
	return rc;
}

static int32_t syncboss_sensor_driver_create_v4l_subdev(
		struct sensor_ctrl *s_ctrl)
{
	int32_t rc = 0;
	uint32_t session_id = 0;

	rc = camera_init_v4l2(&s_ctrl->pdev->dev, &session_id);
	if (rc < 0) {
		pr_err("failed: camera_init_v4l2: %d\n", rc);
		return rc;
	}
	pr_debug("%s:%d: rc: %d session_id: %d\n",
			__func__, __LINE__, rc, session_id);
	return rc;
}

/*
 * SyncBoss Platform driver
 */
static int syncboss_camera_get_dt_vreg_data(struct device_node *of_node,
		struct camera_vreg_t **cam_vreg,
		int *num_vreg)
{
	int rc = 0, i = 0;
	int32_t count = 0;
	uint32_t *vreg_array = NULL;
	struct camera_vreg_t *vreg = NULL;
	bool custom_vreg_name = false;

	count = of_property_count_strings(of_node, "qcom,cam-vreg-name");

	if (!count || (count == -EINVAL)) {
		pr_err("%s:%d: Number of entries is 0 or not present in dts\n",
				__func__, __LINE__);
		*num_vreg = 0;
		return 0;
	}

	vreg = kcalloc(count, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	*cam_vreg = vreg;
	*num_vreg = count;
	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
				"qcom,cam-vreg-name", i, &vreg[i].reg_name);
		if (rc < 0) {
			pr_err("%s:%d: failed\n", __func__, __LINE__);
			goto ERROR1;
		}
	}

	custom_vreg_name =
		of_property_read_bool(of_node, "qcom,cam-custom-vreg-name");
	if (custom_vreg_name) {
		for (i = 0; i < count; i++) {
			rc = of_property_read_string_index(of_node,
					"qcom,cam-custom-vreg-name", i,
					&vreg[i].custom_vreg_name);
			if (rc < 0) {
				pr_err("%s:%d: failed\n", __func__, __LINE__);
				goto ERROR1;
			}
		}
	}

	vreg_array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!vreg_array) {
		rc = -ENOMEM;
		goto ERROR1;
	}

	for (i = 0; i < count; i++)
		vreg[i].type = VREG_TYPE_DEFAULT;

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-type",
			vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d: failed\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++)
				vreg[i].type = vreg_array[i];
		}
	} else {
		pr_debug("%s:%d: no qcom,cam-vreg-type entries in dts\n",
				__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-min-voltage",
			vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d: failed\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++)
				vreg[i].min_voltage = vreg_array[i];
		}
	} else {
		pr_debug("%s:%d: no qcom,cam-vreg-min-voltage entries in dts\n",
				__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-max-voltage",
			vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d: failed\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++)
				vreg[i].max_voltage = vreg_array[i];
		}
	} else {
		pr_debug("%s:%d: no qcom,cam-vreg-max-voltage entries in dts\n",
				__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-op-mode",
			vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d: failed\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++)
				vreg[i].op_mode = vreg_array[i];
		}
	} else {
		pr_debug("%s:%d: no qcom,cam-vreg-op-mode entries in dts\n",
				__func__, __LINE__);
		rc = 0;
	}

	kfree(vreg_array);
	return rc;
ERROR2:
	kfree(vreg_array);
ERROR1:
	kfree(vreg);
	*num_vreg = 0;
	return rc;
}

static int syncboss_camera_config_single_vreg(struct device *dev,
		struct camera_vreg_t *cam_vreg,
		struct regulator **reg_ptr,
		int config)
{
	int rc = 0;
	const char *vreg_name = NULL;

	if (!dev || !cam_vreg || !reg_ptr) {
		pr_err("%s: get failed NULL parameter\n", __func__);
		goto vreg_get_fail;
	}
	if (cam_vreg->type == VREG_TYPE_CUSTOM) {
		if (cam_vreg->custom_vreg_name == NULL) {
			pr_err("%s: can't find sub reg name\n", __func__);
			goto vreg_get_fail;
		}
		vreg_name = cam_vreg->custom_vreg_name;
	} else {
		if (cam_vreg->reg_name == NULL) {
			pr_err("%s: can't find reg name\n", __func__);
			goto vreg_get_fail;
		}
		vreg_name = cam_vreg->reg_name;
	}

	if (config) {
		*reg_ptr = regulator_get(dev, vreg_name);
		if (IS_ERR(*reg_ptr)) {
			pr_err("%s: %s get failed\n", __func__, vreg_name);
			*reg_ptr = NULL;
			goto vreg_get_fail;
		}
		if (regulator_count_voltages(*reg_ptr) > 0) {
			rc = regulator_set_voltage(*reg_ptr,
					cam_vreg->min_voltage,
					cam_vreg->max_voltage);
			if (rc < 0) {
				pr_err("%s: %s set voltage failed\n",
						__func__, vreg_name);
				goto vreg_set_voltage_fail;
			}
			if (cam_vreg->op_mode >= 0) {
				rc = regulator_set_load(*reg_ptr,
						cam_vreg->op_mode);
				if (rc < 0) {
					pr_err("%s: %s set optimum mode failed\n",
							__func__, vreg_name);
					goto vreg_set_opt_mode_fail;
				}
			}
		}
		rc = regulator_enable(*reg_ptr);
		if (rc < 0) {
			pr_err("%s: %s regulator_enable failed\n",
					__func__, vreg_name);
			goto vreg_unconfig;
		}
	} else {
		if (*reg_ptr) {
			regulator_disable(*reg_ptr);
			if (regulator_count_voltages(*reg_ptr) > 0) {
				if (cam_vreg->op_mode >= 0)
					regulator_set_load(*reg_ptr, 0);
				regulator_set_voltage(*reg_ptr, 0,
						cam_vreg->max_voltage);
			}
			regulator_put(*reg_ptr);
			*reg_ptr = NULL;
		} else {
			pr_err("%s can't disable %s\n", __func__, vreg_name);
		}
	}
	return 0;

vreg_unconfig:
	if (regulator_count_voltages(*reg_ptr) > 0)
		regulator_set_load(*reg_ptr, 0);

vreg_set_opt_mode_fail:
	if (regulator_count_voltages(*reg_ptr) > 0)
		regulator_set_voltage(*reg_ptr, 0, cam_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(*reg_ptr);
	*reg_ptr = NULL;

vreg_get_fail:
	return -ENODEV;
}
static int syncboss_camera_power_enable(struct platform_device *pdev,
					bool enabled)
{
	struct sensor_ctrl *sensor_ctrl = NULL;
	int rc = 0;
	int count;

	sensor_ctrl = platform_get_drvdata(pdev);
	if (!sensor_ctrl) {
		pr_err("%s:%d: Syncboss sensor control data is null!\n",
				__func__, __LINE__);
		return -ENODEV;
	}

	if (sensor_ctrl->vreg_handles) {
		for (count = 0; count < sensor_ctrl->num_vregs; count++) {

			rc = syncboss_camera_config_single_vreg(
					&pdev->dev,
					&(sensor_ctrl->vregs[count]),
					(struct regulator **)
					&sensor_ctrl->vreg_handles[count],
					enabled);
			if (rc) {
				pr_err("%s:%d: config_single_vreg(): %d\n",
						__func__, __LINE__, rc);
			}
		}
	}
	return 0;
}

static int syncboss_sensor_driver_enable_temp_sensor_power(
		struct platform_device *pdev)
{
	struct sensor_ctrl *sensor_ctrl = NULL;
	int rc = 0;
	int count;
	bool found_regulator = false;

	sensor_ctrl = platform_get_drvdata(pdev);
	if (!sensor_ctrl) {
		pr_err("%s:%d: Syncboss sensor control data is null!\n",
				__func__, __LINE__);
		return -ENODEV;
	}

	if (!sensor_ctrl->vreg_handles) {
		pr_err("%s:%d: No regulators found!\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	for (count = 0; count < sensor_ctrl->num_vregs; count++) {
		/* Look for the one specific regulator we care about */
		if ((strcmp(sensor_ctrl->vregs[count].reg_name,
			    REGULATOR_VDIG_NAME) == 0)) {

			found_regulator = true;
				
			rc = syncboss_camera_config_single_vreg(
				&pdev->dev,
				&(sensor_ctrl->vregs[count]),
				(struct regulator **)
				&sensor_ctrl->vreg_handles[count],
				/*enable*/ true);
			if (rc) {
				pr_err("%s:%d: config_single_vreg(): %d\n",
				       __func__, __LINE__, rc);
			}

			/* We've turned on the one regulator we care
			 * about.  No more work to be done here.
			 */
			break;
		}
	}

	if (!found_regulator) {
		pr_err("%s:%d: Was not able to find the vdig regulator\n",
		       __func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static int syncboss_camera_clk_enable(struct platform_device *pdev,
					bool enabled)
{
	struct sensor_ctrl *sensor_ctrl = NULL;
	struct msm_cam_clk_info *clk_info = NULL;
	int rc = 0;
	int count;

	sensor_ctrl = platform_get_drvdata(pdev);
	if (!sensor_ctrl) {
		pr_err("%s:%d: Syncboss sensor control data is null!\n",
				__func__, __LINE__);
		return -ENODEV;
	}
	rc = msm_camera_get_clk_info(pdev, &sensor_ctrl->clocks.clk_info,
			&sensor_ctrl->clocks.clk_ptr,
			&sensor_ctrl->clocks.num_clk);
	if (rc) {
		pr_err("Failed to get clocks information for sensor\n");
		return rc;
	}

	clk_info = sensor_ctrl->clocks.clk_info;

	/* Wait for 1 ms for clocks to stabalize */
	for (count = 0; count < sensor_ctrl->clocks.num_clk; count++)
		clk_info[count].delay = 1;

	rc = msm_camera_clk_enable(&pdev->dev, sensor_ctrl->clocks.clk_info,
			sensor_ctrl->clocks.clk_ptr,
			sensor_ctrl->clocks.num_clk, enabled);
	if (rc) {
		pr_err("%s:%d: Failed to %s camera clocks, rc: %d\n",
				__func__, __LINE__,
				enabled?"enable":"disable", rc);
		return -ENODEV;
	}

	return 0;
}


static int32_t syncboss_sensor_driver_platform_probe(
		struct platform_device *pdev)
{
	int32_t rc = 0;
	struct sensor_ctrl *sensor_ctrl = NULL;
	uint32_t cell_id;

	struct vs1_gpio vs1_syncboss_gpio_table[] = {
		{"pwdn-gpios", -1, NULL}, {},
	};

	if (androidboot_mode_charger) {
		pr_info("%s:%d: Charge mode, skipping proobe camera sensor driver\n",
			__func__, __LINE__);
		return 0;
	}

	sensor_ctrl = kzalloc(sizeof(*sensor_ctrl), GFP_KERNEL);
	if (!sensor_ctrl)
		return -ENOMEM;

	platform_set_drvdata(pdev, sensor_ctrl);

	sensor_ctrl->of_node = pdev->dev.of_node;
	sensor_ctrl->pdev = pdev;

	vs1_syncboss_gpio_table[0].target = &sensor_ctrl->powerdown;

	rc = get_pin_control(&pdev->dev, &sensor_ctrl->pinctrl,
			&sensor_ctrl->pin_default, &sensor_ctrl->pin_suspend,
			"cam_default", "cam_suspend");
	if (rc) {
		pr_err("%s:%d: failed to get pin control: %d\n",
				__func__, __LINE__, rc);
		goto free_sensor_ctrl;
	}

	rc = of_property_read_u32(sensor_ctrl->of_node, "cell-index", &cell_id);
	if (rc < 0) {
		pr_err("%s:%d: failed to get sensor cell-index: %d\n",
				__func__, __LINE__, rc);
		goto pinctrl_cleanup;
	}

	/* Validate cell_id */
	if (cell_id >= MAX_CAMERAS) {
		pr_err("%s:%d: invalid cell_id: %d\n",
				__func__, __LINE__, cell_id);
		goto pinctrl_cleanup;
	}

	sensor_ctrl->id = cell_id;

	rc = pinctrl_select_state(sensor_ctrl->pinctrl,
			sensor_ctrl->pin_default);
	if (rc) {
		pr_err("%s:%d: Unable to set sensor pins to active state: %d\n",
		       __func__, __LINE__, rc);
		goto pinctrl_cleanup;
	}

	rc = syncboss_camera_get_dt_vreg_data(sensor_ctrl->of_node,
			&sensor_ctrl->vregs, &sensor_ctrl->num_vregs);
	if (rc) {
		pr_err("%s:%d: Failed to get regulator information: %d\n",
		       __func__, __LINE__, rc);
		goto pinctrl_cleanup;
	}

	sensor_ctrl->vreg_handles = kcalloc(sensor_ctrl->num_vregs,
			sizeof(*sensor_ctrl->vreg_handles), GFP_KERNEL);
	if (!sensor_ctrl->vreg_handles) {
		rc = -ENOMEM;
		goto free_vreg_data;
	}

	rc = vs1_configure_gpio(&pdev->dev, vs1_syncboss_gpio_table);
	if (rc) {
		goto free_vreg_handles;
	}

	rc = syncboss_sensor_driver_create_v4l_subdev(sensor_ctrl);
	if (rc) {
		goto free_vreg_handles;
	}

	mutex_lock(&sensor_ctrl_list_mutex);
	list_add(&sensor_ctrl->list, &sensor_ctrl_list);
	num_sensors++;
	mutex_unlock(&sensor_ctrl_list_mutex);

	return rc;

free_vreg_handles:
	kfree(sensor_ctrl->vreg_handles);
	sensor_ctrl->vreg_handles = NULL;

free_vreg_data:
	kfree(sensor_ctrl->vregs);
	sensor_ctrl->vregs = NULL;

pinctrl_cleanup:
	devm_pinctrl_put(sensor_ctrl->pinctrl);
free_sensor_ctrl:
	kfree(sensor_ctrl);

	return rc;
}

static int syncboss_sensor_platform_remove(struct platform_device *pdev)
{
	struct sensor_ctrl *sensor_ctrl = NULL;
	int rc = 0;

	sensor_ctrl = platform_get_drvdata(pdev);
	if (!sensor_ctrl) {
		pr_err("%s:%d: Syncboss sensor control data is null!\n",
				__func__, __LINE__);
		return 0;
	}

	mutex_lock(&sensor_ctrl_list_mutex);
	list_del(&sensor_ctrl->list);
	num_sensors--;
	mutex_unlock(&sensor_ctrl_list_mutex);

	syncboss_camera_clk_enable(pdev, false);
	if (rc) {
		pr_err("%s:%d: Failed\n", __func__, __LINE__);
		return rc;
	}
	syncboss_camera_power_enable(pdev, false);
	if (rc) {
		pr_err("%s:%d: Failed\n", __func__, __LINE__);
		return rc;
	}

	kfree(sensor_ctrl->vreg_handles);
	kfree(sensor_ctrl->vregs);

	gpio_safe_release(sensor_ctrl->powerdown);

	kfree(sensor_ctrl);

	return 0;
}

static int syncboss_sensor_driver_suspend(struct platform_device *pdev,
					  pm_message_t state)
{
	int rc = 0;

	rc = syncboss_camera_clk_enable(pdev, false);
	if (rc) {
		pr_err("%s:%d: Failed\n", __func__, __LINE__);
		return rc;
	}
	rc = syncboss_camera_power_enable(pdev, false);
	if (rc) {
		pr_err("%s:%d: Failed\n", __func__, __LINE__);
		return rc;
	}

	return 0;
}

static int syncboss_sensor_driver_resume(struct platform_device *pdev)
{
	int rc = 0;

	syncboss_camera_power_enable(pdev, true);
	if (rc) {
		pr_err("%s:%d: Failed\n", __func__, __LINE__);
		return rc;
	}
	syncboss_camera_clk_enable(pdev, true);
	if (rc) {
		pr_err("%s:%d: Failed\n", __func__, __LINE__);
		return rc;
	}
	return 0;
}

static const struct of_device_id syncboss_sensor_driver_dt_match[] = {
	{
		.compatible = "oculus,camera"
	},
	{}
};

static struct platform_driver syncboss_sensor_platform_driver = {
	.probe = syncboss_sensor_driver_platform_probe,
	.driver = {
		.name = "oculus,camera",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_sensor_driver_dt_match,
	},
	.remove = syncboss_sensor_platform_remove,
};

static int __init syncboss_camera_driver_init(void)
{
	int rc = 0;
	rc = platform_driver_register(&syncboss_sensor_platform_driver);
	if (rc)
		pr_err("Failed to register syncboss platform driver: %d\n", rc);

	return rc;
}

static void __exit syncboss_camera_driver_exit(void)
{
	struct sensor_ctrl *sensor, *temp;

	platform_driver_unregister(&syncboss_sensor_platform_driver);

	mutex_lock(&sensor_ctrl_list_mutex);

	list_for_each_entry_safe(sensor, temp, &sensor_ctrl_list, list)
		list_del(&sensor->list);
	num_sensors = 0;

	mutex_unlock(&sensor_ctrl_list_mutex);
}

void enable_cameras(void)
{
	struct sensor_ctrl *sensor;

	mutex_lock(&sensor_ctrl_list_mutex);
	list_for_each_entry(sensor, &sensor_ctrl_list, list) {
		if (sensor->enabled) {
			pr_info("Skipping enable for camera @ idx %d (already enabled)\n",
				sensor->id);
		} else {
			pr_info("Enabling camera @ idx %d\n", sensor->id);
			syncboss_sensor_driver_resume(sensor->pdev);
			sensor->enabled = true;
		}
	}
	mutex_unlock(&sensor_ctrl_list_mutex);
}

void disable_cameras(void)
{
	struct sensor_ctrl *sensor;

	mutex_lock(&sensor_ctrl_list_mutex);
	list_for_each_entry(sensor, &sensor_ctrl_list, list) {
		if (sensor->enabled) {
			pr_info("Disabling camera @ idx %d\n", sensor->id);
			syncboss_sensor_driver_suspend(sensor->pdev,
						       (pm_message_t){0});
			sensor->enabled = false;
		} else {
			pr_info("Skipping disable for camera @ idx %d (already disabled)\n",
				sensor->id);
		}
	}
	mutex_unlock(&sensor_ctrl_list_mutex);
}

int enable_camera_temp_sensor_power(void)
{
	struct sensor_ctrl *sensor;

	mutex_lock(&sensor_ctrl_list_mutex);
	list_for_each_entry(sensor, &sensor_ctrl_list, list) {
		pr_info("Turning on camera temp sensor regulator @ idx %d\n",
			sensor->id);
		syncboss_sensor_driver_enable_temp_sensor_power(
				sensor->pdev);
	}
	mutex_unlock(&sensor_ctrl_list_mutex);

	return 0;
}

int get_num_cameras(void)
{
	return num_sensors;
}

module_init(syncboss_camera_driver_init);
module_exit(syncboss_camera_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Driver for syncboss camera power");
