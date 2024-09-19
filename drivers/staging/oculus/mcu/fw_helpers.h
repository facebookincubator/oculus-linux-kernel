/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _FW_HELPERS_H
#define _FW_HELPERS_H

#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

typedef void (*fw_work_func_t)(void *data);


int fw_queue_work(struct workqueue_struct *workqueue, void *data,
		  fw_work_func_t func, struct completion *work_complete);

#endif
