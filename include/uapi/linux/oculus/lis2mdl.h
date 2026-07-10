#ifndef __UAPI_LIS2MDL__
#define __UAPI_LIS2MDL__

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

enum lis2mdl_event_type {
	lis2mdl_event_data = 0,
	lis2mdl_event_pm_suspend_fence = 1,
	lis2mdl_event_pm_resume_fence = 2,
};

struct lis2mdl_event {
	__u64 timestamp;
	__u32 type; /*1 of enum lis2mdl_event_type */
	__u32 len;
	__u8 data[];
} __attribute__((packed));

enum lis2mdl_reg_op_flags {
	lis2mdl_reg_op_flag_none,
	lis2mdl_reg_op_flag_reg_bank_a,
};

struct lis2mdl_reg_operation {
	__u8 reg;
	__u32 len;
	int flags;
	void __user *buf;
};

enum lis2mdl_interrupt_mode {
	LIS2MDL_INTR_DISABLED,
	LIS2MDL_INTR_READ_RANGE,
};

struct lis2mdl_interrupt_config {
	enum lis2mdl_interrupt_mode mode;
	union {
		struct {
			__u8 reg;
			__u32 len;
		} range;
	};
};

struct lis2mdl_pm_config {
	__u32 num_ops;
	struct lis2mdl_reg_operation __user *ops;
};

#define LIS2MDL_IOC_REG_READ \
	_IOWR('L', 0x01, struct lis2mdl_reg_operation)
#define LIS2MDL_IOC_REG_WRITE \
	_IOW('L', 0x02, struct lis2mdl_reg_operation)
#define LIS2MDL_IOC_INTR_GET_CFG \
	_IOR('L', 0x03, struct lis2mdl_interrupt_config)
#define LIS2MDL_IOC_INTR_SET_CFG \
	_IOW('L', 0x04, struct lis2mdl_interrupt_config)
#define LIS2MDL_IOC_PM_SET_SUSPEND \
	_IOW('L', 0x05, struct lis2mdl_pm_config)
#define LIS2MDL_IOC_PM_SET_RESUME \
	_IOW('L', 0x06, struct lis2mdl_pm_config)

#endif

