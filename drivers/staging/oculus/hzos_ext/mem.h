/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#ifndef _HZOS_EXT_MEM_H
#define _HZOS_EXT_MEM_H

#include <linux/types.h>

void hzos_ext_mem_init(void);

/* Numeric feature callbacks for MEM_HAIRCUT */
ssize_t mem_haircut_write(s64 value);
s64 mem_haircut_get(void);

#endif /* _HZOS_EXT_MEM_H */
