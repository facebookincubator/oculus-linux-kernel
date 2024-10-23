/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RPROC_QCOM_COMMON_H__
#define __RPROC_QCOM_COMMON_H__

#include <linux/timer.h>
#include <linux/remoteproc.h>
#include "remoteproc_internal.h"
#include <linux/soc/qcom/qmi.h>
#include <linux/remoteproc/qcom_rproc.h>

static const char * const subdevice_state_string[] = {
	[QCOM_SSR_BEFORE_POWERUP]	= "before_powerup",
	[QCOM_SSR_AFTER_POWERUP]	= "after_powerup",
	[QCOM_SSR_BEFORE_SHUTDOWN]	= "before_shutdown",
	[QCOM_SSR_AFTER_SHUTDOWN]	= "after_shutdown",
};

struct reg_info {
	struct regulator *reg;
	int uV;
	int uA;
};

struct qcom_sysmon;

struct qcom_rproc_glink {
	struct rproc_subdev subdev;

	const char *ssr_name;

	struct device *dev;
	struct device_node *node;
	struct qcom_glink *edge;

	struct notifier_block nb;
	void *notifier_handle;
};

struct qcom_rproc_subdev {
	struct rproc_subdev subdev;

	struct device *dev;
	struct device_node *node;
	struct qcom_smd_edge *edge;
};

struct qcom_ssr_subsystem;

struct qcom_rproc_ssr {
	struct rproc_subdev subdev;
	bool is_notified;
	enum qcom_ssr_notify_type notification;
	struct timer_list timer;
	struct qcom_ssr_subsystem *info;
};

extern bool qcom_device_shutdown_in_progress;

typedef void (*rproc_dumpfn_t)(struct rproc *rproc, struct rproc_dump_segment *segment,
			       void *dest, size_t offset, size_t size);

void qcom_minidump(struct rproc *rproc, struct device *md_dev,
			unsigned int minidump_id, rproc_dumpfn_t dumpfn);

void qcom_add_glink_subdev(struct rproc *rproc, struct qcom_rproc_glink *glink,
			   const char *ssr_name);
void qcom_remove_glink_subdev(struct rproc *rproc, struct qcom_rproc_glink *glink);

int qcom_register_dump_segments(struct rproc *rproc, const struct firmware *fw);

void qcom_add_smd_subdev(struct rproc *rproc, struct qcom_rproc_subdev *smd);
void qcom_remove_smd_subdev(struct rproc *rproc, struct qcom_rproc_subdev *smd);

void qcom_add_ssr_subdev(struct rproc *rproc, struct qcom_rproc_ssr *ssr,
			 const char *ssr_name);
void qcom_notify_early_ssr_clients(struct rproc_subdev *subdev);
void qcom_remove_ssr_subdev(struct rproc *rproc, struct qcom_rproc_ssr *ssr);

#if IS_ENABLED(CONFIG_QCOM_SYSMON)
struct qcom_sysmon *qcom_add_sysmon_subdev(struct rproc *rproc,
					   const char *name,
					   int ssctl_instance);
void qcom_remove_sysmon_subdev(struct qcom_sysmon *sysmon);
bool qcom_sysmon_shutdown_acked(struct qcom_sysmon *sysmon);
uint32_t qcom_sysmon_get_txn_id(struct qcom_sysmon *sysmon);
int qcom_sysmon_get_reason(struct qcom_sysmon *sysmon, char *buf, size_t len);
void qcom_sysmon_register_ssr_subdev(struct qcom_sysmon *sysmon,
				struct rproc_subdev *ssr_subdev);
#else
static inline void qcom_sysmon_register_ssr_subdev(struct qcom_sysmon *sysmon,
				struct rproc_subdev *ssr_subdev)
{
}

static inline struct qcom_sysmon *qcom_add_sysmon_subdev(struct rproc *rproc,
							 const char *name,
							 int ssctl_instance)
{
	return NULL;
}

static inline void qcom_remove_sysmon_subdev(struct qcom_sysmon *sysmon)
{
}

static inline bool qcom_sysmon_shutdown_acked(struct qcom_sysmon *sysmon)
{
	return false;
}

static inline uint32_t qcom_sysmon_get_txn_id(struct qcom_sysmon *sysmon)
{
	return 0;
}

int qcom_sysmon_get_reason(struct qcom_sysmon *sysmon, char *buf, size_t len)
{ return -ENODEV; }
#endif

#endif
