#
# Makefile for industrial I/O accelerometer drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_BMA180) += bma180.o
obj-$(CONFIG_BMC150_ACCEL) += bmc150-accel.o
obj-$(CONFIG_HID_SENSOR_ACCEL_3D) += hid-sensor-accel-3d.o
obj-$(CONFIG_KXCJK1013) += kxcjk-1013.o
obj-$(CONFIG_KXSD9)	+= kxsd9.o
obj-$(CONFIG_MMA8452)	+= mma8452.o

obj-$(CONFIG_IIO_ST_ACCEL_3AXIS) += st_accel.o
st_accel-y := st_accel_core.o
st_accel-$(CONFIG_IIO_BUFFER) += st_accel_buffer.o

obj-$(CONFIG_IIO_ST_ACCEL_I2C_3AXIS) += st_accel_i2c.o
obj-$(CONFIG_IIO_ST_ACCEL_SPI_3AXIS) += st_accel_spi.o

obj-$(CONFIG_IIO_ST_LIS2DS12) += lis2ds12.o
obj-$(CONFIG_IIO_ST_LIS2DS12_I2C) += st_lis2ds12_i2c.o
obj-$(CONFIG_IIO_ST_LIS2DS12_SPI) += st_lis2ds12_spi.o

lis2ds12-y += st_lis2ds12_core.o st_lis2ds12_buffer.o st_lis2ds12_trigger.o

obj-$(CONFIG_IIO_ST_LIS2HH12) += lis2hh12.o
obj-$(CONFIG_IIO_ST_LIS2HH12_I2C) += st_lis2hh12_i2c.o
obj-$(CONFIG_IIO_ST_LIS2HH12_SPI) += st_lis2hh12_spi.o

lis2hh12-y += st_lis2hh12_core.o st_lis2hh12_buffer.o st_lis2hh12_trigger.o

st_acc33-y := st_acc33_core.o st_acc33_buffer.o
obj-$(CONFIG_IIO_ST_ACC33) += st_acc33.o
obj-$(CONFIG_IIO_ST_ACC33_I2C) += st_acc33_i2c.o
obj-$(CONFIG_IIO_ST_ACC33_SPI) += st_acc33_spi.o

st_lis2dw12-y:= st_lis2dw12_core.o st_lis2dw12_ring.o
obj-$(CONFIG_IIO_ST_LIS2DW12) += st_lis2dw12.o
obj-$(CONFIG_IIO_ST_LIS2DW12_I2C) += st_lis2dw12_i2c.o
obj-$(CONFIG_IIO_ST_LIS2DW12_SPI) += st_lis2dw12_spi.o
