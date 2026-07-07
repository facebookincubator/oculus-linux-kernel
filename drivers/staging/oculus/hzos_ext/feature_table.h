/*
 * Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

/*
 * HZOS_EXT_FEATURE(name, enabled)
 *
 * Boolean features that can be enabled or disabled at runtime.
 *
 * The meaning of "enabled" depends on the feature. For example, for ALLOW_RT,
 * "enabled" means that we will enforce the check if a task is allowed to use
 * RT.
 *
 * Features can be queried, and dynamically enabled or disabled at runtime, by
 * reading or writing to the /proc/hzos_ext procfs file. To enable or
 * disable the feature, echo <NO_>HZOS_EXT_FEATURE_<NAME> to the file:
 *
 * - "NO_" should be prefixed to DISABLE the feature. Simply echoing the name of
 *   the feature will ENABLE it.
 * - Only a single feature may be enabled or disabled in a single syscall.
 *
 * To query the current status of all features, simply read the procfs file.
 * Each feature will be printed to the buffer passed by user space, with any
 * disabled feature being prefixed with "NO_".
 */
HZOS_EXT_FEATURE(ALLOW_RT, false)
HZOS_EXT_FEATURE(SELECT_RQ_IDLE, false)
HZOS_EXT_FEATURE(BALANCE_ANON_FILE_RECLAIM, true)

/*
 * HZOS_EXT_NUMERIC_FEATURE(name, write_cb, read_cb)
 *
 * Numeric features store a 64-bit signed integer value that can be queried
 * and modified at runtime via callbacks. Each numeric feature specifies:
 *
 * - write_cb: void (*)(s64) - called when the feature value is written
 * - read_cb: s64 (*)(void) - called when the feature value is read
 *
 * To set a numeric feature value, echo NAME=VALUE to /proc/hzos_ext:
 *
 * - "MEM_HAIRCUT=32000000" sets MEM_HAIRCUT to 32000000
 * - Only a single feature may be modified in a single syscall.
 *
 * To query the current status, read /proc/hzos_ext. Each numeric feature
 * will be printed as NAME=VALUE.
 */
HZOS_EXT_NUMERIC_FEATURE(MEM_HAIRCUT, mem_haircut_write, mem_haircut_get)
