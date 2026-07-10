#ifndef __UAPI_MINI_SENSOR__
#define __UAPI_MINI_SENSOR__

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define LSM6DSL_REG_BANK_A_FLAG 0x100

enum minisensor_event_type {
	minisensor_event_data = 0,
	minisensor_event_pm_suspend_fence = 1,
	minisensor_event_pm_resume_fence = 2,
};

struct minisensor_event {
	__u64 timestamp;
	__u32 type; /*1 of enum minisensor_event_type */
	__u32 len;
	__u32 fifo_status;
	__u8 data[];
} __attribute__((packed));

struct minisensor_reg_operation {
	__u16 reg;
	__u32 len;
	__u32 delay_us;
	void __user *buf;
};

enum minisensor_interrupt_mode {
	MINI_SENSOR_INTR_DISABLED,
	MINI_SENSOR_INTR_READ_FIXED_RANGE,
	MINI_SENSOR_INTR_READ_FIFO,
};

struct minisensor_interrupt_config {
	enum minisensor_interrupt_mode mode;
	union {
		struct {
			__u16 reg;
			__u32 len;
		} range;
		struct {
			__u16 status_reg;
			__u8 len_mask;
			__u8 len_right_shift;
			__u8 len_multiplier;
			__u16 data_reg;
		} fifo;
	};
};

struct minisensor_pm_config {
	__u32 num_ops;
	struct minisensor_reg_operation __user *ops;
};

#define MINI_SENSOR_IOC_REG_READ \
	_IOWR('L', 0x01, struct minisensor_reg_operation)
#define MINI_SENSOR_IOC_REG_WRITE \
	_IOW('L', 0x02, struct minisensor_reg_operation)
#define MINI_SENSOR_IOC_INTR_GET_CFG \
	_IOR('L', 0x03, struct minisensor_interrupt_config)
#define MINI_SENSOR_IOC_INTR_SET_CFG \
	_IOW('L', 0x04, struct minisensor_interrupt_config)
#define MINI_SENSOR_IOC_PM_SET_SUSPEND \
	_IOW('L', 0x05, struct minisensor_pm_config)
#define MINI_SENSOR_IOC_PM_SET_RESUME \
	_IOW('L', 0x06, struct minisensor_pm_config)
#endif /* __UAPI_MINI_SENSOR__*/
