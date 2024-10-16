obj-$(CONFIG_META_SWD)		+= meta_swd.o
meta_swd-objs += fwupdate_manager.o swd.o swd_driver.o

ifeq ($(CONFIG_META_SWD_DEBUG),y)
meta_swd-objs += fwupdate_debug.o
endif

ifeq ($(CONFIG_META_SWD_HUBERT),y)
meta_swd-objs += hubert_swd_ops.o
endif

ifeq ($(CONFIG_META_SWD_QM35),y)
meta_swd-objs += qm35xxx_swd_ops.o
endif

ifeq ($(CONFIG_META_SWD_STM32G0),y)
meta_swd-objs += stm32g0_swd_ops.o
endif

ifeq ($(CONFIG_META_SWD_STM32L4),y)
meta_swd-objs += stm32l47xxx_swd_ops.o
endif

ifeq ($(CONFIG_META_SWD_SYNCBOSS_NRF52XXX),y)
meta_swd-objs += syncboss_swd_ops_nrf52xxx.o
endif

ifeq ($(CONFIG_META_SWD_SYNCBOSS_NRF5340),y)
meta_swd-objs += syncboss_swd_ops_nrf5340.o
endif
