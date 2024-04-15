obj-$(CONFIG_SYNCBOSS_SPI) += syncboss_spi.o
syncboss_spi-objs += syncboss_spi_core.o syncboss_spi_sequence_number.o syncboss_spi_sysfs.o syncboss_spi_fastpath.o

ifeq ($(CONFIG_SYNCBOSS_DEBUG),y)
syncboss_spi-objs += syncboss_spi_debugfs.o
endif

obj-$(CONFIG_SYNCBOSS_POWERSTATE) += syncboss_powerstate.o
obj-$(CONFIG_SYNCBOSS_NSYNC) += syncboss_nsync.o
obj-$(CONFIG_SYNCBOSS_MISCFIFO) += syncboss_miscfifo.o
obj-$(CONFIG_SYNCBOSS_DIRECTCHANNEL) += syncboss_direct_channel.o
obj-$(CONFIG_SYNCBOSS_TIMESYNC) += syncboss_timesync.o
