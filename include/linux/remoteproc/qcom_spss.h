/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_RPROC_SPSS_H_
#define _QCOM_RPROC_SPSS_H_

/*
 * Include this file only after including linux/remoteproc.h
 */

#if IS_ENABLED(CONFIG_QCOM_SPSS)

extern int qcom_spss_set_fw_name(struct rproc *rproc, const char *fw_name);

#else

static inline int qcom_spss_set_fw_name(struct rproc *rproc, const char *fw_name)
{
	return -ENODEV;
}

#endif

#endif
