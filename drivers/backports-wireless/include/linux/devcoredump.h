/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_BUILD_WANT_DEV_COREDUMP
#include_next <linux/devcoredump.h>
#include <linux/bp-devcoredump.h>
#else
#undef dev_coredumpv
#define dev_coredumpv LINUX_BACKPORT(dev_coredumpv)
#undef dev_coredumpm
#define dev_coredumpm LINUX_BACKPORT(dev_coredumpm)
#undef dev_coredumpsg
#define dev_coredumpsg LINUX_BACKPORT(dev_coredumpsg)
#include <linux/backport-devcoredump.h>
#endif /* CPTCFG_BPAUTO_BUILD_WANT_DEV_COREDUMP */
