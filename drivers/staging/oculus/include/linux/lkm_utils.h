/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LKM_UTILS__
#define __LKM_UTILS__

#ifdef CONFIG_KGDB
#include <linux/kgdb.h>
#define breakpoint() kgdb_breakpoint()
#else
#define breakpoint()
#endif

#define IS_OK(rc) (rc == 0)
#define IS_NOK(rc) (rc < 0)

#endif
