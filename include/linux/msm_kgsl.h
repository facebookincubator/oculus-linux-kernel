/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_KGSL_H
#define _MSM_KGSL_H

#define KGSL_DEVICE_3D0 0

/* Limits mitigations APIs */
#ifdef CONFIG_QCOM_KGSL
void *kgsl_pwr_limits_add(u32 id);
void kgsl_pwr_limits_del(void *limit);
int kgsl_pwr_limits_set_freq(void *limit, unsigned int freq);
int kgsl_pwr_limits_set_gpu_fmax(void *limit, unsigned int freq);
void kgsl_pwr_limits_set_default(void *limit);
unsigned int kgsl_pwr_limits_get_freq(u32 id);
#else
static inline void *kgsl_pwr_limits_add(u32 id) { return NULL; }
static inline void kgsl_pwr_limits_del(void *limit)  { return; }
static inline int kgsl_pwr_limits_set_freq(void *limit, unsigned int freq) { return 0; }
static inline int kgsl_pwr_limits_set_gpu_fmax(void *limit, unsigned int freq) { return 0; }
static inline void kgsl_pwr_limits_set_default(void *limit)  { return; }
static inline unsigned int kgsl_pwr_limits_get_freq(u32 id) { return 0; }
#endif

#endif /* _MSM_KGSL_H */
