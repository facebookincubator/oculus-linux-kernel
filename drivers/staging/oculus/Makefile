subdir-ccflags-y += -Werror -Wall

USERINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi

LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/linux \
	-I$(srctree)/drivers/staging/oculus/include/uapi \
	-I$(srctree)/drivers/staging/oculus/include

obj-$(CONFIG_AS5510)			+= as5510.o
obj-$(CONFIG_BATTERY_VIRTUAL_SENSOR)	+= battery_virtual_sensor.o
obj-$(CONFIG_BLU_FW)			+= blu_fw_driver.o
obj-$(CONFIG_CHARGING_THERMAL_CONTROLLER)	+= charging_thermal_controller.o
obj-$(CONFIG_DISPLAY_VIRTUAL_SENSOR)	+= display_virtual_sensor.o
obj-$(CONFIG_FRONT_PANEL_SENSOR)	+= front_panel_sensor.o
obj-$(CONFIG_IIO_CHANNEL_SENSOR)	+= iio_channel_sensor.o
obj-$(CONFIG_INSTRUCTION_SAMPLER)	+= oculus_instruction_sampler.o
obj-$(CONFIG_MISCFIFO)			+= miscfifo.o
obj-$(CONFIG_MISCFIFO_SAMPLE)		+= miscfifo-sample.o
obj-$(CONFIG_MOLOKINI)			+= molokini.o
obj-$(CONFIG_SENSORS_PWM_TACH_FAN)	+= pwm-tach-fan.o
obj-$(CONFIG_RT600_CTRL) 		+= rt600-ctrl.o
obj-$(CONFIG_TOP_PANEL_SENSOR)		+= top_panel_sensor.o
obj-$(CONFIG_SPI_STP)			+= stp/
obj-$(CONFIG_VIRTUAL_SENSOR_UTILS)	+= virtual_sensor_utils.o
obj-y					+= internal/
obj-y					+= mcu/
