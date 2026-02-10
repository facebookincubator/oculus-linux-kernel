/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#ifndef _HZOS_EXT_FEATURES_H
#define _HZOS_EXT_FEATURES_H

#include <linux/types.h>

#ifdef HZOS_EXT_FEATURE
#undef HZOS_EXT_FEATURE
#endif

#define HZOS_EXT_FEATURE(__feature, __enabled) HZOS_EXT_FEATURE_ ##__feature,
enum hzos_ext_feature {
#include "feature_table.h"
	HZOS_EXT_NUM_FEATURES,
};
#undef HZOS_EXT_FEATURE

#define hzos_ext_branch__true DECLARE_STATIC_KEY_TRUE
#define hzos_ext_branch__false DECLARE_STATIC_KEY_FALSE
#define HZOS_EXT_FEATURE(__feature, __enabled) hzos_ext_branch__ ##__enabled(static_key__ ##__feature);
#include "feature_table.h"
#undef hzos_ext_branch__true
#undef hzos_ext_branch__false
#undef HZOS_EXT_FEATURE

static inline bool hzos_ext_feature_enabled(enum hzos_ext_feature feature)
{
#define hzos_ext_branch__true static_branch_likely
#define hzos_ext_branch__false static_branch_unlikely

#define HZOS_EXT_FEATURE(__feature, __enabled)						\
	case HZOS_EXT_FEATURE_ ##__feature:							\
		return hzos_ext_branch__ ##__enabled(&static_key__ ##__feature);
	switch (feature) {
#include "feature_table.h"
		default: BUG();
	}
#undef hzos_ext_branch__true
#undef hzos_ext_branch__false
#undef HZOS_EXT_FEATURE
}

void hzos_ext_features_init(void);

#endif /* _HZOS_EXT_FEATURES_H */
