/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#ifndef _HZOS_EXT_FEATURES_H
#define _HZOS_EXT_FEATURES_H

#include <linux/types.h>

/* Undefine macros if previously defined */
#ifdef HZOS_EXT_FEATURE
#undef HZOS_EXT_FEATURE
#endif
#ifdef HZOS_EXT_NUMERIC_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE
#endif

/* Generate enum for boolean features */
#define HZOS_EXT_FEATURE(__feature, __enabled) HZOS_EXT_FEATURE_ ##__feature,
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) /* skip numeric */
enum hzos_ext_feature {
#include "feature_table.h"
	HZOS_EXT_NUM_FEATURES,
};
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

/* Generate enum for numeric features */
#define HZOS_EXT_FEATURE(__feature, __enabled) /* skip boolean */
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) HZOS_EXT_NUMERIC_ ##__feature,
enum hzos_ext_numeric_feature {
#include "feature_table.h"
	HZOS_EXT_NUM_NUMERIC_FEATURES,
};
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

/* Declare static keys for boolean features */
#define hzos_ext_branch__true DECLARE_STATIC_KEY_TRUE
#define hzos_ext_branch__false DECLARE_STATIC_KEY_FALSE
#define HZOS_EXT_FEATURE(__feature, __enabled) hzos_ext_branch__ ##__enabled(static_key__ ##__feature);
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) /* skip numeric */
#include "feature_table.h"
#undef hzos_ext_branch__true
#undef hzos_ext_branch__false
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE

/**
 * hzos_ext_feature_enabled() - Check if a boolean feature is enabled
 * @feature: The feature to check
 *
 * Returns true if the feature is enabled, false otherwise.
 */
static inline bool hzos_ext_feature_enabled(enum hzos_ext_feature feature)
{
#define hzos_ext_branch__true static_branch_likely
#define hzos_ext_branch__false static_branch_unlikely

#define HZOS_EXT_FEATURE(__feature, __enabled)						\
	case HZOS_EXT_FEATURE_ ##__feature:						\
		return hzos_ext_branch__ ##__enabled(&static_key__ ##__feature);
#define HZOS_EXT_NUMERIC_FEATURE(__feature, __write_cb, __read_cb) /* skip numeric */
	switch (feature) {
#include "feature_table.h"
		default: BUG();
	}
#undef hzos_ext_branch__true
#undef hzos_ext_branch__false
#undef HZOS_EXT_FEATURE
#undef HZOS_EXT_NUMERIC_FEATURE
}

void hzos_ext_features_init(void);

#endif /* _HZOS_EXT_FEATURES_H */
