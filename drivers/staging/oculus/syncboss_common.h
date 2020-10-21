#ifndef _SYNCBOSS_COMMON_H
#define _SYNCBOSS_COMMON_H

#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

typedef void (*syncboss_work_func_t)(void *data);


int syncboss_queue_work(struct workqueue_struct *workqueue,
		void *data,
		syncboss_work_func_t func,
		struct completion *work_complete);

int init_syncboss_regulator(struct device *dev,
		struct regulator **reg, const char *reg_name);

#endif
