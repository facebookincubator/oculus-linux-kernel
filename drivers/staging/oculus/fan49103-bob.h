#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/device.h>

#ifndef __FAN49103_BOB_H__
#define __FAN49103_BOB_H__

struct fan49103_ctx {
	struct device *dev;
	struct regmap *registers;
	u8 manufacturer_id;
	u8 device_id;
};

enum fan49103_reg_t {
	FAN49103_REG_MIN_REG = 0x02,
	FAN49103_REG_CONTROL = 0x02,
	FAN49103_REG_MANUFACTURER = 0x40,
	FAN49103_REG_DEVICE = 0x41,
	FAN49103_REG_MAX_REG = 0x41,
};

#define _FAN_IDX_FOR_REG(reg) ((reg) - FAN49103_REG_MIN_REG)

#define FAN_REG_GET_NAME(reg) fan49104_reg_addr_to_name[_FAN_IDX_FOR_REG(reg)]

#define FAN_REG_RANGE_SIZE (FAN49103_REG_MAX_REG - FAN49103_REG_MIN_REG + 1)

#define FAN_REG_PRINT_FMT "%s(0x%02X)"
#define FAN_REG_FMT_ARGS(var) FAN_REG_GET_NAME((var)), (var)

#define _FAN_REG_NAME(reg, name) [_FAN_IDX_FOR_REG(reg)] = name

#endif //__FAN49103_BOB_H__
