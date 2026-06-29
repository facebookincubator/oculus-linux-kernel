subdir-ccflags-y += -Werror -Wall -Wno-error=frame-address

USERINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi

LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/linux \
	-I$(srctree)/drivers/staging/oculus/include/uapi \
	-I$(srctree)/drivers/staging/oculus/include

obj-$(CONFIG_CAM_FSYNC)				+= cam-fsync/
obj-$(CONFIG_GENERIC_VIRTUAL_SENSOR)		+= generic_virtual_sensor/
obj-$(CONFIG_GPIO_LOWVOLT_DETECT)		+= gpio_low_volt/
obj-$(CONFIG_INSTRUCTION_SAMPLER)		+= oculus_instruction_sampler/
obj-$(CONFIG_LEDS_AW2026) 			+= aw2026/
obj-$(CONFIG_MAX17332)				+= max17332/
obj-$(CONFIG_META_USB_CONN_GPIO)		+= meta_usb_conn_gpio/
obj-$(CONFIG_MFI_I2C)				+= mfi/
obj-$(CONFIG_MPM_SLEEP_CLOCK) 			+= mpm_sleep_clock/
obj-$(CONFIG_POWER_STATE_VIRTUAL_SENSOR)	+= power_state_virtual_sensor/
obj-$(CONFIG_REGULATOR_MAX77813) 		+= max77813/
obj-$(CONFIG_REGULATOR_PM3003A)			+= pm3003a/
obj-$(CONFIG_REGULATOR_RT6160)			+= rt6160/
obj-$(CONFIG_RT600_CTRL) 			+= rt600_ctrl/
obj-$(CONFIG_ST60_CTRL) 			+= st60_ctrl/
obj-$(CONFIG_SPI_STP)				+= stp/
obj-$(CONFIG_METASOC)				+= metasoc/
obj-$(CONFIG_MCU_THERMISTOR) 		+= mcu_thermistor/
obj-y						+= mcu/
