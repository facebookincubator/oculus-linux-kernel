#
# Makefile for the fingerprint drivers.
#
# Each configuration option enables a list of files.
ccflags-y += -Idrivers/misc/

obj-$(CONFIG_GW39A1)  += gw39a1.o
ifeq ($(CONFIG_GW39A1),m)
gw39a1-objs += gf_spi.o platform.o gf_common.o
endif
