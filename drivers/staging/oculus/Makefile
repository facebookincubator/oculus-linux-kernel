subdir-ccflags-y += -Werror -Wall

USERINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi

LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi \
	-I$(srctree)/drivers/staging/oculus/include

obj-$(CONFIG_AS5510)			+= as5510.o
obj-$(CONFIG_DEPTH_PROJECTOR)		+= projector.o
obj-$(CONFIG_FRONT_PANEL_SENSOR)	+= front_panel_sensor.o
obj-$(CONFIG_HYPERVISOR_GUEST)		+= hv_guest.o
obj-$(CONFIG_MISCFIFO)			+= miscfifo.o
obj-$(CONFIG_MISCFIFO_SAMPLE)		+= miscfifo-sample.o
obj-$(CONFIG_MOLOKINI)			+= molokini.o
obj-$(CONFIG_SYNCBOSS)			+= swd_driver.o fwupdate_manager.o fw_helpers.o syncboss_swd_ops.o syncboss_spi.o swd.o hubert_swd_ops.o
obj-$(CONFIG_SYNCBOSS_CAMERA_CONTROL)	+= syncboss_camera.o
