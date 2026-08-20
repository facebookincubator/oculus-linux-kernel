LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/metasoc/utility \
	-I$(srctree)/drivers/staging/oculus/metasoc/soc \
	-I$(srctree)/drivers/staging/oculus/metasoc/platform_headers/linux \
	-I$(srctree)/drivers/staging/oculus/gpio_low_volt

obj-$(CONFIG_MAX17332)			+= max17332.o
obj-$(CONFIG_MAX17332)			+= max17332-battery.o
obj-$(CONFIG_MAX17332)			+= max17332-charger.o
obj-$(CONFIG_MAX17332)			+= max17332-cache.o
obj-$(CONFIG_MAX17332_CURRENT_OVERRIDE)	+= max17332-current-override.o
obj-$(CONFIG_MAX17332_VOLTAGE_ADJUSTMENT)	+= max17332-voltage-adjustment.o
obj-$(CONFIG_MAX17332_CDEV)		+= max17332-cdev.o
