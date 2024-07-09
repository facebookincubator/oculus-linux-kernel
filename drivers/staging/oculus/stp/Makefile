#
# Makefile for kernel SPI STP LAYER driver.
#

ccflags-y += -Idrivers/staging/oculus/stp
ccflags-y += -std=gnu99
ccflags-y += -Wno-declaration-after-statement

obj-$(CONFIG_SPI_STP) += stp/common/stp_common.o
obj-$(CONFIG_SPI_STP) += stp/common/stp_pipeline.o
obj-$(CONFIG_SPI_STP) += stp/controller/stp_controller_io.o
obj-$(CONFIG_SPI_STP) += stp/controller/stp_controller.o
obj-$(CONFIG_SPI_STP) += stp/controller/stp_notification.o
obj-$(CONFIG_SPI_STP) += common/stp_error_mapping.o
obj-$(CONFIG_SPI_STP) += device/stp_device.o
obj-$(CONFIG_SPI_STP) += driver/stp_driver.o
obj-$(CONFIG_SPI_STP) += driver/stp_gpio.o
