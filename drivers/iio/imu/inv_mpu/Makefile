#
# Makefile for Invensense inv-mpu-iio device.
#

obj-y += inv-mpu-iio.o

inv-mpu-iio-objs += inv_mpu_common.o
inv-mpu-iio-objs += inv_mpu_ring.o
inv-mpu-iio-objs += inv_mpu_timestamp.o

# compile Invensense MPU IIO driver as DTS
#ifeq ($(CONFIG_DTS_INV_MPU_IIO), y)
inv-mpu-iio-objs += inv_mpu_dts.o
#endif

# chip support

inv-mpu-iio-objs += icm20602/inv_mpu_init_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_core_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_parsing_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_setup_20602.o
inv-mpu-iio-objs += icm20602/inv_mpu_selftest_20602.o

# Bus support
ifeq ($(CONFIG_INV_MPU_IIO_I2C), y)
inv-mpu-iio-objs += inv_mpu_i2c.o
endif

inv-mpu-iio-objs += inv_mpu_spi.o


obj-y += inv_test/
