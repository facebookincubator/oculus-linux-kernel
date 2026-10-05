subdir-ccflags-y += -Werror -Wall -Wno-error=frame-address

USERINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/uapi

LINUXINCLUDE	+= \
	-I$(srctree)/drivers/staging/oculus/include/linux \
	-I$(srctree)/drivers/staging/oculus/include/uapi \
	-I$(srctree)/drivers/staging/oculus/include

obj-$(CONFIG_DSP_PD_KILL)			+= dsp_pd_kill/
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

obj-$(CONFIG_ACROPOLIS_CTRL)			+= acropolis-ctrl/
obj-$(CONFIG_AMEM_KMD)				+= amem/
obj-$(CONFIG_ARFIRMWARE)			+= arfw/
obj-$(CONFIG_BBC_CTRL)				+= bbc-ctrl/
obj-$(CONFIG_DAM_CTRL)				+= dam-ctrl/
obj-$(CONFIG_HYPEROFF)				+= hyperoff/
obj-$(CONFIG_IIO_CHANNEL_TEMP_SENSOR)		+= iio_channel_temp_sensor/
obj-$(CONFIG_JBD4020_I2C)			+= jbd4020/
obj-$(CONFIG_MAX31875_I2C)			+= max31875/
obj-$(CONFIG_MAX77789)				+= max77789/
obj-$(CONFIG_METABATTERY)			+= metabattery/
obj-$(CONFIG_PM8150_XO)				+= pm8150-xo/
obj-$(CONFIG_RAA491901_I2C)			+= raa491901/
obj-$(CONFIG_REGULATOR_DA9172)			+= da9172/
obj-$(CONFIG_REGULATOR_HL7594)			+= hl7594/
obj-$(CONFIG_REGULATOR_KTB8399)			+= ktb8399/
obj-$(CONFIG_REGULATOR_MAX77655)		+= max77655/
obj-$(CONFIG_REGULATOR_MP28167)			+= mp28167/
obj-$(CONFIG_STUSB4500_USB)			+= stusb4500/
obj-$(CONFIG_TMP114_I2C)			+= tmp114/
obj-$(CONFIG_TPS61280)				+= tps61280/
obj-$(CONFIG_META_HAMMERHEAD)			+= netinfo/
obj-$(CONFIG_META_FLORIAN)			+= netinfo/
obj-$(CONFIG_META_GREATWHITE)			+= netinfo/
obj-$(CONFIG_META_BLUEWHITE)			+= netinfo/

obj-y						+= mcu/
