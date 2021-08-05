// SPDX-License-Identifier: GPL-2.0
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "fw_helpers.h"

struct fw_work_item {
	struct work_struct work;
	void *data;
	fw_work_func_t work_fn;
	struct completion *work_complete;
};

static void fw_do_work(struct work_struct *work)
{
	struct fw_work_item *devwork =
		container_of(work, struct fw_work_item, work);
	BUG_ON(devwork->work_fn == NULL);
	BUG_ON(devwork->data == NULL);

	devwork->work_fn(devwork->data);

	/* Signal work complete if someone is waiting on it */
	if (devwork->work_complete)
		complete(devwork->work_complete);

	kfree(devwork);
}

int fw_queue_work(struct workqueue_struct *workqueue, void *data,
		  fw_work_func_t func, struct completion *work_complete)
{
	struct fw_work_item *work = kzalloc(sizeof(*work), GFP_KERNEL);

	if (!work)
		return -ENOMEM;

	work->work_fn = func;
	work->data = data;
	INIT_WORK(&work->work, fw_do_work);

	if (work_complete) {
		/* Completion desired, init it here */
		init_completion(work_complete);
		work->work_complete = work_complete;
	}

	queue_work(workqueue, &work->work);
	return 0;
}


#define MAX_PROP_SIZE 40
int fw_init_regulator(struct device *dev, struct regulator **reg,
		      const char *reg_name)
{
	int rc = 0;
	u32 voltage[2] = {0};
	struct device_node *of = dev->of_node;
	char prop_name[MAX_PROP_SIZE] = {0};

	snprintf(prop_name, MAX_PROP_SIZE, "oculus,%s", reg_name);
	*reg = regulator_get(dev, prop_name);
	if (IS_ERR(*reg)) {
		rc = PTR_ERR(*reg);
		dev_warn(dev, "%s: Failed to get regulator: %d\n",
			reg_name, rc);
		return -EINVAL;
	}

	snprintf(prop_name, MAX_PROP_SIZE, "oculus,%s-voltage-level", reg_name);
	rc = of_property_read_u32_array(of, prop_name, voltage, 2);
	if (rc) {
		dev_err(dev, "%s: Failed to get voltage range: %d\n",
			reg_name, rc);
		goto err_reg_put;
	}

	rc = regulator_set_voltage(*reg, voltage[0], voltage[1]);
	if (rc) {
		dev_err(dev, "%s: Failed to set voltage: %d\n", reg_name, rc);
		goto err_reg_put;
	}

	return 0;

err_reg_put:
	regulator_put(*reg);
	*reg = NULL;
	return rc;
}
