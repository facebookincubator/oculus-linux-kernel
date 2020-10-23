subdir-ccflags-y += -Werror -Wall

USERINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi

LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi \
	-I$(srctree)/drivers/staging/oculus/include

obj-$(CONFIG_AS5510)			+= as5510.o
obj-$(CONFIG_DEPTH_PROJECTOR)		+= projector.o
obj-$(CONFIG_HYPERVISOR_GUEST)		+= hv_guest.o
obj-$(CONFIG_MISCFIFO)			+= miscfifo.o
obj-$(CONFIG_MISCFIFO_SAMPLE)		+= miscfifo-sample.o
obj-$(CONFIG_SYNCBOSS)			+= fwupdate_driver.o fwupdate_manager.o syncboss_swd_ops.o syncboss.o syncboss_common.o syncboss_spi.o syncboss_swd.o hubert_swd_ops.o
obj-$(CONFIG_SYNCBOSS_CAMERA_CONTROL)	+= syncboss_camera.o
