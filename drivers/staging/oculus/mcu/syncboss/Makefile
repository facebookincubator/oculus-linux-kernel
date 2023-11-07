obj-$(CONFIG_SYNCBOSS)		+= syncboss.o
syncboss-objs += spi_fastpath.o syncboss_sequence_number.o syncboss_spi.o syncboss_sysfs.o

ifeq ($(CONFIG_SYNCBOSS_DEBUG),y)
syncboss-objs += syncboss_debugfs.o
endif
