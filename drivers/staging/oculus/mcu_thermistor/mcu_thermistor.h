#ifndef __MCU_THERMISTOR_H__
#define __MCU_THERMISTOR_H__

#define DRIVER_NAME        "mcu_thermistor"
#define DEVICE_NAME        DRIVER_NAME
#define CLASS_NAME         DRIVER_NAME
#define MAJOR_NUM          0

#define MCU_THERMISTOR_MAGIC 0x7E57
#define MCU_THERMISTOR_MAX_MESSAGE_SIZE 32
#define MCU_THERMISTOR_MAX_MESSAGE_SIZE_BYTES 4

struct mcu_thermistor_data {
	struct device *dev;
	struct device_node *stp_interface_node;
};

struct mcu_thermistor_tz_data {
	void *data;
	struct thermal_zone_device *tzd;
};

#endif
