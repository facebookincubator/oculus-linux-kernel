// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "common.h"
#include <linux/scmi_bwprof.h>

#define SCMI_VENDOR_MSG_MODULE_START   (16)
#define MAX_MASTERS			3
#define MAX_BUCKETS			3

enum scmi_bwprof_cmd {
	BWPROF_SET_LOG_LEVEL = SCMI_VENDOR_MSG_MODULE_START,
	BWPROF_SET_SAMPLE_MS,
	BWPROF_MASTER_LIST,
	BWPROF_SET_ENABLE,
	BWPROF_SET_HIST_INFO
};

struct master_info {
	uint8_t cnt;
	uint8_t masters[MAX_MASTERS];
} __packed;

struct bucket_info {
	uint32_t buckets[MAX_BUCKETS];
} __packed;

struct sample_ms_info {
	uint8_t	hist;
	uint16_t sample_ms;
} __packed;

static int scmi_set_global_var(const struct scmi_protocol_handle *ph, u8 val,
		u32 msg_id)
{
	struct scmi_xfer *t;
	uint8_t *msg;
	int ret;

	ret = ph->xops->xfer_get_init(ph, msg_id, sizeof(*msg), sizeof(*msg), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	*msg = val;
	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);
	return ret;
}

static int scmi_set_masters_list(const struct scmi_protocol_handle *ph, u8 cnt,
		u8 *master_list)
{
	struct master_info *msg;
	struct scmi_xfer *t;
	int ret, i;

	ret = ph->xops->xfer_get_init(ph, BWPROF_MASTER_LIST, sizeof(*msg),
			sizeof(*msg), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	msg->cnt = cnt;
	for (i = 0; i < cnt; i++)
		msg->masters[i] = master_list[i];
	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);

	return ret;
}

static int scmi_set_sample_ms(const struct scmi_protocol_handle *ph,
		uint8_t hist_enable, uint16_t ms_val)
{
	struct sample_ms_info *msg;
	struct scmi_xfer *t;
	int ret;

	ret = ph->xops->xfer_get_init(ph, BWPROF_SET_SAMPLE_MS, sizeof(*msg),
			sizeof(*msg), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;

	msg->hist = hist_enable;
	msg->sample_ms = ms_val;

	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);

	return ret;
}

static int scmi_set_log_level(const struct scmi_protocol_handle *ph, u8 val)
{
	return scmi_set_global_var(ph, val, BWPROF_SET_LOG_LEVEL);
}

static int scmi_set_sampling_enable(const struct scmi_protocol_handle *ph,
	u8 val)
{
	return scmi_set_global_var(ph, val, BWPROF_SET_ENABLE);
}

static int scmi_set_hist_info(const struct scmi_protocol_handle *ph,
		u32 *buckets_list)
{
	struct bucket_info *msg;
	struct scmi_xfer *t;
	int ret, i;

	ret = ph->xops->xfer_get_init(ph, BWPROF_SET_HIST_INFO, sizeof(*msg),
			sizeof(*msg), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	for (i = 0; i < MAX_BUCKETS; i++)
		msg->buckets[i] = cpu_to_le32(buckets_list[i]);

	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);

	return ret;
}
static struct scmi_bwprof_vendor_ops bwprof_proto_ops = {
	.set_log_level = scmi_set_log_level,
	.set_sample_ms = scmi_set_sample_ms,
	.set_masters_list = scmi_set_masters_list,
	.set_sampling_enable = scmi_set_sampling_enable,
	.set_hist_info = scmi_set_hist_info
};

static int scmi_bwprof_vendor_protocol_init(
	const struct scmi_protocol_handle *ph)
{
	u32 version;

	ph->xops->version_get(ph, &version);
	dev_dbg(ph->dev, "bwprof version %d.%d\n",
			PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	return 0;
}

static const struct scmi_protocol scmi_bwprof_vendor = {
	.id = SCMI_PROTOCOL_BWPROF,
	.owner = THIS_MODULE,
	.init_instance = &scmi_bwprof_vendor_protocol_init,
	.ops = &bwprof_proto_ops,
};
module_scmi_protocol(scmi_bwprof_vendor);

MODULE_DESCRIPTION("SCMI bwprof vendor Protocol");
MODULE_LICENSE("GPL");
