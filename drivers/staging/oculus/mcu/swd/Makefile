obj-$(CONFIG_OCULUS_SWD)		+= oculus-swd.o
oculus-swd-objs += fwupdate_manager.o swd.o swd_driver.o

ifeq ($(CONFIG_OCULUS_SWD_DEBUG),y)
oculus-swd-objs += fwupdate_debug.o
endif

ifeq ($(CONFIG_OCULUS_SWD_HUBERT),y)
oculus-swd-objs += hubert_swd_ops.o
endif

ifeq ($(CONFIG_OCULUS_SWD_QM35),y)
oculus-swd-objs += qm35xxx_swd_ops.o
endif

ifeq ($(CONFIG_OCULUS_SWD_STM32G0),y)
oculus-swd-objs += stm32g0_swd_ops.o
endif

ifeq ($(CONFIG_OCULUS_SWD_STM32L4),y)
oculus-swd-objs += stm32l47xxx_swd_ops.o
endif

ifeq ($(CONFIG_OCULUS_SWD_SYNCBOSS_NRF52XXX),y)
oculus-swd-objs += syncboss_swd_ops_nrf52xxx.o
endif

ifeq ($(CONFIG_OCULUS_SWD_SYNCBOSS_NRF5340),y)
oculus-swd-objs += syncboss_swd_ops_nrf5340.o
endif
