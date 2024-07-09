/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */
#ifndef FASTCVPD_H
#define FASTCVPD_H

#include <linux/types.h>

#define FASTCVPD_GLINK_GUID "fastcvpd-glink-apps-dsp"
#define FASTCVPD_SMD_GUID "fastcvpd-smd-apps-dsp"
#define FASTCVPD_DEVICE_NAME "fastcvpd-smd"

#ifdef CONFIG_MSM_FASTCVPD
/*
 * API for Video driver to send physical address to FastCVP driver
 * @param phys_addr
 * Physical address of command message queue
 * that needs to be mapped to CDSP.
 * It should be allocated from CMA adsp_mem region.
 *
 * @param size_in_bytes
 * Size in bytes of command message queue
 */
int fastcvpd_video_send_cmd_hfi_queue(phys_addr_t *phys_addr,
	uint32_t size_in_bytes);

/*
 * API for Video driver to suspend CVP session during
 * power collapse
 *
 * @param session_flag
 * Flag to share details of session.
 */
int fastcvpd_video_suspend(uint32_t session_flag);

/*
 * API for Video driver to resume CVP session during
 * power collapse
 *
 * @param session_flag
 * Flag to share details of session.
 */
int fastcvpd_video_resume(uint32_t session_flag);

/*
 * API for Video driver to shutdown CVP session during
 * video subsystem error.
 *
 * @param session_flag
 * Flag to share details of session.
 */
int fastcvpd_video_shutdown(uint32_t session_flag);

#else

static inline int fastcvpd_video_send_cmd_hfi_queue(
	phys_addr_t *phys_addr,
	uint32_t size_in_bytes)
{
	return -ENODEV;
}

static inline int fastcvpd_video_shutdown(uint32_t session_flag)
{
	return -ENODEV;
}

static inline int fastcvpd_video_suspend(uint32_t session_flag)
{
	return -ENODEV;
}

static inline int fastcvpd_video_resume(uint32_t session_flag)
{
	return -ENODEV;
}

#endif // CONFIG_FASTCVPD
#endif // FASTCVPD_H
