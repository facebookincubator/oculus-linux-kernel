#ifndef __BP_OF_DEVICE_H
#define __BP_OF_DEVICE_H
#include_next <linux/of_device.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,1,0)
static inline void of_dma_configure(struct device *dev, struct device_node *np)
{}
#endif /* < 4.1.0 */

#endif /* __BP_OF_DEVICE_H */
