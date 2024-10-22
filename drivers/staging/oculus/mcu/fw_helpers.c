// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
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
EXPORT_SYMBOL(fw_queue_work);

MODULE_LICENSE("GPL v2");
