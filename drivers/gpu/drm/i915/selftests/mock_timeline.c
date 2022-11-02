/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright © 2017-2018 Intel Corporation
 */

#include "../i915_timeline.h"

#include "mock_timeline.h"

void mock_timeline_init(struct i915_timeline *timeline, u64 context)
{
	timeline->fence_context = context;

	spin_lock_init(&timeline->lock);

	init_request_active(&timeline->last_request, NULL);
	INIT_LIST_HEAD(&timeline->requests);

	i915_syncmap_init(&timeline->sync);

	INIT_LIST_HEAD(&timeline->link);
}

void mock_timeline_fini(struct i915_timeline *timeline)
{
	i915_timeline_fini(timeline);
}
