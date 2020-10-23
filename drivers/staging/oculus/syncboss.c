#include "syncboss_spi.h"
#include "fwupdate_driver.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

static struct platform_driver * const platform_drivers[] = {
	&oculus_fwupdate_driver,
};

static int __init syncboss_init(void)
{
	int rc = spi_register_driver(&oculus_syncboss_driver);

	if (rc < 0)
		return rc;
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
	spi_unregister_driver(&oculus_syncboss_driver);
}

module_init(syncboss_init);
module_exit(syncboss_exit);
MODULE_DESCRIPTION("SYNCBOSS");
MODULE_LICENSE("GPL v2");
