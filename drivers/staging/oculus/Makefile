subdir-ccflags-y += -Werror -Wall

USERINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi

LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi \
	-I$(srctree)/drivers/staging/oculus/include

obj-$(CONFIG_AS5510)			+= as5510.o
obj-$(CONFIG_BATTERY_VIRTUAL_SENSOR)	+= battery_virtual_sensor.o
obj-$(CONFIG_BLU_FW)			+= blu_fw_driver.o
obj-$(CONFIG_DEPTH_PROJECTOR)		+= projector.o
obj-$(CONFIG_DISPLAY_VIRTUAL_SENSOR)	+= display_virtual_sensor.o
obj-$(CONFIG_EYE_FACE_LED)		+= eye_face_led.o
obj-$(CONFIG_FRONT_PANEL_SENSOR)	+= front_panel_sensor.o
obj-$(CONFIG_HYPERVISOR_GUEST)		+= hv_guest.o
obj-$(CONFIG_MISCFIFO)			+= miscfifo.o
obj-$(CONFIG_MISCFIFO_SAMPLE)		+= miscfifo-sample.o
obj-$(CONFIG_MOLOKINI)			+= molokini.o
obj-$(CONFIG_NAUTILUS_CLK)		+= nautilus_clk.o
obj-$(CONFIG_SEACLIFF_BLU)		+= seacliff_blu_spi.o
obj-$(CONFIG_SYNCBOSS)			+= syncboss/
obj-$(CONFIG_VIRTUAL_SENSOR_UTILS)	+= virtual_sensor_utils.o
