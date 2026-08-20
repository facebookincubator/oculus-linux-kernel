subdir-ccflags-y += -Werror -Wall

LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/metasoc/utility \
	-I$(srctree)/drivers/staging/oculus/metasoc/soc \
	-I$(srctree)/drivers/staging/oculus/metasoc/platform_headers/linux

obj-$(CONFIG_METASOC)					+= soc/metasoc.o
obj-$(CONFIG_METASOC)					+= utility/metasoc_utility.o
