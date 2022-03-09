#ifndef __BACKPORT_LINUX_IRQDOMAIN_H
#define __BACKPORT_LINUX_IRQDOMAIN_H
#include <linux/version.h>

#if LINUX_VERSION_IS_GEQ(3,1,0)
#include_next <linux/irqdomain.h>
#endif

#endif /* __BACKPORT_LINUX_IRQDOMAIN_H */
