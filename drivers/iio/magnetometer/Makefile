#
# Makefile for industrial I/O Magnetometer sensor drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_AK09911)	+= ak09911.o
obj-$(CONFIG_AK8975)	+= ak8975.o
obj-$(CONFIG_MAG3110)	+= mag3110.o
obj-$(CONFIG_HID_SENSOR_MAGNETOMETER_3D) += hid-sensor-magn-3d.o

obj-$(CONFIG_IIO_ST_MAGN_3AXIS) += st_magn.o
st_magn-y := st_magn_core.o
st_magn-$(CONFIG_IIO_BUFFER) += st_magn_buffer.o

obj-$(CONFIG_IIO_ST_MAGN_I2C_3AXIS) += st_magn_i2c.o
obj-$(CONFIG_IIO_ST_MAGN_SPI_3AXIS) += st_magn_spi.o

st_mag40-y += st_mag40_buffer.o st_mag40_core.o
obj-$(CONFIG_ST_MAG40_IIO) += st_mag40.o
obj-$(CONFIG_ST_MAG40_I2C_IIO) += st_mag40_i2c.o
obj-$(CONFIG_ST_MAG40_SPI_IIO) += st_mag40_spi.o

st_mag3d-y += st_mag3d_core.o st_mag3d_buffer.o
obj-$(CONFIG_ST_MAG3D_IIO) += st_mag3d.o
obj-$(CONFIG_ST_MAG3D_I2C_IIO) += st_mag3d_i2c.o
obj-$(CONFIG_ST_MAG3D_SPI_IIO) += st_mag3d_spi.o

