#ifndef __BACKPORT_H
#define __BACKPORT_H
#include <generated/autoconf.h>
#include <linux/kconfig.h>

#ifndef __ASSEMBLY__
#define LINUX_BACKPORT(__sym) backport_ ##__sym
#ifndef CONFIG_BACKPORT_INTEGRATE
#include <backport/checks.h>
#endif
#endif

#endif /* __BACKPORT_H */
