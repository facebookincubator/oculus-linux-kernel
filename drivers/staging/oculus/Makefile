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
obj-$(CONFIG_CAM_FSYNC)			+= cam-fsync.o
obj-$(CONFIG_CHARGER_CYPD3177)		+= cypd3177.o
obj-$(CONFIG_CHARGING_THERMAL_CONTROLLER)	+= charging_thermal_controller.o
obj-$(CONFIG_CYPD_POLICY_ENGINE)	+= cypd_policy_engine.o
obj-$(CONFIG_DISPLAY_VIRTUAL_SENSOR)	+= display_virtual_sensor.o
obj-$(CONFIG_FPGA_AGGREGATOR)		+= fpga_aggregator.o
obj-$(CONFIG_FRONT_PANEL_SENSOR)	+= front_panel_sensor.o
obj-$(CONFIG_GENERIC_VIRTUAL_SENSOR)	+= generic_virtual_sensor.o
obj-$(CONFIG_GPIO_CAPACITOR)		+= gpio_capacitor.o
obj-$(CONFIG_IIO_CHANNEL_SENSOR)	+= iio_channel_sensor.o
obj-$(CONFIG_INSTRUCTION_SAMPLER)	+= oculus_instruction_sampler.o
obj-$(CONFIG_MISCFIFO)			+= miscfifo.o
obj-$(CONFIG_MISCFIFO_SAMPLE)		+= miscfifo-sample.o
obj-$(CONFIG_NAUTILUS_RESOURCE_MANAGER)	+= nautilus.o
obj-$(CONFIG_OCULUS_CHARGING_DOCK)	+= charging_dock.o
obj-$(CONFIG_OCULUS_EXTERNAL_BATTERY)	+= external_battery_drv.o
external_battery_drv-y := external_battery.o
external_battery_drv-$(CONFIG_OCULUS_EXTERNAL_BATTERY_USBPD) += external_battery_usbpd.o
external_battery_drv-$(CONFIG_OCULUS_EXTERNAL_BATTERY_GLINK) += external_battery_glink.o
obj-$(CONFIG_OCULUS_VDM_GLINK)		+= vdm_glink.o
obj-$(CONFIG_POWER_STATE_VIRTUAL_SENSOR)	+= power_state_virtual_sensor.o
obj-$(CONFIG_REGULATOR_AW37504)		+= aw37504.o
obj-$(CONFIG_REGULATOR_SGM62110)	+= sgm62110-regulator.o
obj-$(CONFIG_RT600_CTRL) 		+= rt600-ctrl.o
obj-$(CONFIG_SEACLIFF_BLU)		+= seacliff_blu_spi.o
obj-$(CONFIG_SENSORS_PWM_TACH_FAN)	+= pwm-tach-fan.o
obj-$(CONFIG_SPI_STP)			+= stp/
obj-$(CONFIG_TOP_PANEL_SENSOR)		+= top_panel_sensor.o
obj-$(CONFIG_UVLO_TLV4021)   += uvlo_tlv4021.o
obj-$(CONFIG_VIRTUAL_SENSOR_UTILS)	+= virtual_sensor_utils.o
obj-y					+= internal/
obj-y					+= mcu/
