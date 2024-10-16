// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <soc/qcom/secure_buffer.h>
#include "cam_fastrpc.h"
#include "cam_debug_util.h"

#define FASTRPC_UNREGISTERED 0
#define FASTRPC_REGISTERED 1
struct fastrpc_apps gfa_cv;

static char name[16] = "qcom,fastcv234";

static int cam_fastrpc_probe(struct fastrpc_device *rpc_dev)
{

	CAM_INFO(CAM_MEM, "fastrpc probe handle 0x%x\n",
		rpc_dev->handle);

	if (gfa_cv.handle == rpc_dev->handle) {
		gfa_cv.fastrpc_device = rpc_dev;
		complete(&gfa_cv.fastrpc_probe_completion);
	}

	return 0;
}

static int cam_fastrpc_callback(struct fastrpc_device *rpc_dev,
			enum fastrpc_driver_status fastrpc_proc_num)
{
	CAM_INFO(CAM_MEM, "handle 0x%x, proc %d", __func__,
			rpc_dev->handle, fastrpc_proc_num);
	//TODO: Check if any cleanup needed here.

	return 0;
}

static struct fastrpc_driver _fastrpc_client = {
	.probe = cam_fastrpc_probe,
	.callback = cam_fastrpc_callback,
};

int cam_fastrpc_dev_map_dma(struct cam_mem_buf_queue *buf,
			uint32_t dsp_remote_map,
			uint64_t *v_dsp_addr)
{
	struct fastrpc_dev_map_dma frpc_map_buf = {0};
	int rc = 0;

	if (dsp_remote_map == 1) {
		frpc_map_buf.buf = buf->dma_buf;
		frpc_map_buf.size = buf->len;
		frpc_map_buf.attrs = 0;

		CAM_DBG(CAM_MEM,
			"frpc_map_buf size %d, dma_buf %px, map %pK, 0x%x, buf_handle %x",
			frpc_map_buf.size, frpc_map_buf.buf,
			&frpc_map_buf, (unsigned long)&frpc_map_buf, buf->buf_handle);
		rc = fastrpc_driver_invoke(gfa_cv.fastrpc_device, FASTRPC_DEV_MAP_DMA,
			(unsigned long)(&frpc_map_buf));
		if (rc) {
			CAM_ERR(CAM_MEM, "Failed to map buffer %d", rc);
			return rc;
		}
		*v_dsp_addr = frpc_map_buf.v_dsp_addr;
	} else {
		CAM_ERR(CAM_MEM, "Buffer not mapped to dsp");
		buf->fd = -1;
	}

	return rc;
}

int cam_fastrpc_dev_unmap_dma(struct cam_mem_buf_queue *buf)
{
	struct fastrpc_dev_unmap_dma frpc_unmap_buf = {0};
	int rc = 0;

	/* Only if buffer is mapped to dsp */
	if (buf->fd != -1) {
		frpc_unmap_buf.buf = buf->dma_buf;
		CAM_DBG(CAM_MEM, "frpc_unmap_buf dma_buf %px buf_handle %x",
			frpc_unmap_buf.buf, buf->buf_handle);
		rc = fastrpc_driver_invoke(gfa_cv.fastrpc_device, FASTRPC_DEV_UNMAP_DMA,
				(unsigned long)(&frpc_unmap_buf));
		if (rc) {
			CAM_ERR(CAM_MEM, "Failed to unmap buffer 0x%x", rc);
			return rc;
		}
	} else {
		CAM_ERR(CAM_MEM, "buffer not mapped to dsp");
	}

	return rc;
}

int cam_fastrpc_driver_register(uint32_t handle)
{
	int rc = 0;
	bool skip_deregister = true;

	if (FASTRPC_REGISTERED == gfa_cv.state)
	{
		CAM_INFO(CAM_MEM, "fastrpc already registered");
		return rc;
	}
	CAM_DBG(CAM_MEM, "new fastrpc node pid 0x%x", handle);

	/* Setup fastrpc_node */
	gfa_cv.handle = handle;
	gfa_cv.fastrpc_driver = _fastrpc_client;
	gfa_cv.fastrpc_driver.handle = handle;
	gfa_cv.fastrpc_driver.driver.name = name;
	/* Init completion */
	init_completion(&gfa_cv.fastrpc_probe_completion);

	/* register fastrpc device to this session */
	rc = fastrpc_driver_register(&gfa_cv.fastrpc_driver);
	if (rc) {
		CAM_ERR(CAM_MEM, "fastrpc driver reg fail err %d", rc);
		skip_deregister = true;
		goto fail_fastrpc_driver_register;
	}
	/* signal wait reuse dsp timeout setup for now */
	if (!wait_for_completion_timeout(
			&gfa_cv.fastrpc_probe_completion,
			msecs_to_jiffies(200))) {
		CAM_ERR(CAM_MEM, "fastrpc driver_register timeout");
		skip_deregister = false;
		goto fail_fastrpc_driver_register;
	}
	gfa_cv.state = FASTRPC_REGISTERED;

	return rc;

fail_fastrpc_driver_register:
	return -EINVAL;
}

int cam_fastrpc_driver_unregister(uint32_t handle)
{
	int rc = -1;
	if (FASTRPC_UNREGISTERED == gfa_cv.state)
	{
		CAM_INFO(CAM_MEM, "fastrpc already unregistered");
		return rc;
	}

	CAM_INFO(CAM_MEM, "fastrpc unregister node pid 0x%x", handle);

	fastrpc_driver_unregister(&gfa_cv.fastrpc_driver);

	gfa_cv.state = FASTRPC_UNREGISTERED;

	return 0;
}
