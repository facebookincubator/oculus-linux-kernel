/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

/*
 * ORCHESTRATOR_FEATURE(name, enabled)
 *
 * The meaning of "enabled" depends on the feature. For example, for ALLOW_RT,
 * "enabled" means that we will enforce the check if a task is allowed to use
 * RT.
 *
 * Features can be queried, and dynamically enabled or disabled at runtime, by
 * reading or writing to the /proc/orchestrator procfs file. To enable or
 * disable the feature, echo <NO_>ORCHESTRATOR_FEATURE_<NAME> to the file:
 *
 * - "NO_" should be prefixed to DISABLE the feature. Simply echoing the name of
 *   the feature will ENABLE it.
 * - Only a single feature may be enabled or disabled in a single syscall.
 *
 * To query the current status of all features, simply read the procfs file.
 * Each feature will be printed to the buffer passed by user space, with any
 * disabled feature being prefixed with "NO_".
 */
ORCHESTRATOR_FEATURE(ALLOW_RT, false)
ORCHESTRATOR_FEATURE(SELECT_RQ_IDLE, false)
ORCHESTRATOR_FEATURE(BALANCE_ANON_FILE_RECLAIM, true)
