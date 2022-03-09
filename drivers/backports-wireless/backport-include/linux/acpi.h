#ifndef __BACKPORT_LINUX_ACPI_H
#define __BACKPORT_LINUX_ACPI_H
#include_next <linux/acpi.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(3,8,0)
/*
 * Backports
 *
 * commit 95f8a082b9b1ead0c2859f2a7b1ac91ff63d8765
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 * Date:   Wed Nov 21 00:21:50 2012 +0100
 *
 *     ACPI / driver core: Introduce struct acpi_dev_node and related macros
 *
 *     To avoid adding an ACPI handle pointer to struct device on
 *     architectures that don't use ACPI, or generally when CONFIG_ACPI is
 *     not set, in which cases that pointer is useless, define struct
 *     acpi_dev_node that will contain the handle pointer if CONFIG_ACPI is
 *     set and will be empty otherwise and use it to represent the ACPI
 *     device node field in struct device.
 *
 *     In addition to that define macros for reading and setting the ACPI
 *     handle of a device that don't generate code when CONFIG_ACPI is
 *     unset.  Modify the ACPI subsystem to use those macros instead of
 *     referring to the given device's ACPI handle directly.
 *
 *     Signed-off-by: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 *     Reviewed-by: Mika Westerberg <mika.westerberg@linux.intel.com>
 *     Acked-by: Greg Kroah-Hartman <gregkh@linuxfoundation.org>
 */
#ifdef CONFIG_ACPI
#define ACPI_HANDLE(dev) DEVICE_ACPI_HANDLE(dev)
#else
#define ACPI_HANDLE(dev) (NULL)
#endif /* CONFIG_ACPI */
#endif /* LINUX_VERSION_IS_LESS(3,8,0) */

#ifndef ACPI_COMPANION
#ifdef CONFIG_ACPI
static inline struct acpi_device *_acpi_get_companion(struct device *dev)
{
	struct acpi_device *adev;
	int ret;

	ret = acpi_bus_get_device(ACPI_HANDLE(dev), &adev);
	if (ret < 0)
		adev = NULL;

	return adev;
}
#define ACPI_COMPANION(dev)	_acpi_get_companion(dev)
#else
#define ACPI_COMPANION(dev)	(NULL)
#endif /* CONFIG_ACPI */
#endif /* ACPI_COMPANION */

#if LINUX_VERSION_IS_LESS(3,19,0)
#define acpi_dev_remove_driver_gpios LINUX_BACKPORT(acpi_dev_remove_driver_gpios)
static inline void acpi_dev_remove_driver_gpios(struct acpi_device *adev) {}
#endif /* LINUX_VERSION_IS_LESS(3, 19, 0) */

#if LINUX_VERSION_IN_RANGE(3,19,0, 4,13,0)
#define devm_acpi_dev_add_driver_gpios LINUX_BACKPORT(devm_acpi_dev_add_driver_gpios)
static inline int devm_acpi_dev_add_driver_gpios(struct device *dev,
			      const struct acpi_gpio_mapping *gpios)
{
	return -ENXIO;
}
#endif /* LINUX_VERSION_IN_RANGE(3,19,0, 4,13,0) */

#endif /* __BACKPORT_LINUX_ACPI_H */
