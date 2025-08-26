/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#ifndef _ORCHESTRATOR_FEATURES_H
#define _ORCHESTRATOR_FEATURES_H

#include <linux/types.h>

#ifdef ORCHESTRATOR_FEATURE
#undef ORCHESTRATOR_FEATURE
#endif

#define ORCHESTRATOR_FEATURE(__feature, __enabled) ORCHESTRATOR_FEATURE_ ##__feature,
enum orchestrator_feature {
#include "feature_table.h"
	ORCHESTRATOR_NUM_FEATURES,
};
#undef ORCHESTRATOR_FEATURE

#define orchestrator_branch__true DECLARE_STATIC_KEY_TRUE
#define orchestrator_branch__false DECLARE_STATIC_KEY_FALSE
#define ORCHESTRATOR_FEATURE(__feature, __enabled) orchestrator_branch__ ##__enabled(static_key__ ##__feature);
#include "feature_table.h"
#undef orchestrator_branch__true
#undef orchestrator_branch__false
#undef ORCHESTRATOR_FEATURE

static inline bool orchestrator_feature_enabled(enum orchestrator_feature feature)
{
#define orchestrator_branch__true static_branch_likely
#define orchestrator_branch__false static_branch_unlikely

#define ORCHESTRATOR_FEATURE(__feature, __enabled)						\
	case ORCHESTRATOR_FEATURE_ ##__feature:							\
		return orchestrator_branch__ ##__enabled(&static_key__ ##__feature);
	switch (feature) {
#include "feature_table.h"
		default: BUG();
	}
#undef orchestrator_branch__true
#undef orchestrator_branch__false
#undef ORCHESTRATOR_FEATURE
}

void orchestrator_features_init(void);

#endif /* _ORCHESTRATOR_FEATURES_H */
