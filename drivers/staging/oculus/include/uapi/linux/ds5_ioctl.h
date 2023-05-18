/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef DS5_H
#define DS5_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct ds5_fw_version {
	__u16 major;
	__u16 minor;
	__u16 patch;
};

struct ds5_status {
	__u32 major;
};

struct ds5_stream_config {
	__u16 width;
	__u16 height;
	__u16 fps;
	__u16 stream;
};

struct ds5_preset {
	__u16 preset;
	__u16 rsvd1;
};

#define DS5_IOCTL_MAGIC 'i'

#define DS5_GET_FW_VERSION _IOR(DS5_IOCTL_MAGIC, 0xA0, struct ds5_fw_version)
#define DS5_GET_STATUS _IOR(DS5_IOCTL_MAGIC, 0xA4, struct ds5_status)

#define DS5_STREAM_CONFIG _IOW(DS5_IOCTL_MAGIC, 0xA1, struct ds5_stream_config)
#define DS5_STREAM_START _IO(DS5_IOCTL_MAGIC, 0xA2)
#define DS5_STREAM_STOP _IO(DS5_IOCTL_MAGIC, 0xA3)
#define DS5_SET_PRESET _IOW(DS5_IOCTL_MAGIC, 0xA4, struct ds5_preset)

/* Stream configuration for ds5_stream_config. Depth stream is default */
enum ds5_stream_src {
	DS5_STREAM_SRC_DEPTH = 0,
	DS5_STREAM_SRC_IR = 1,
};

/* Presets: 1 - default, 2 - high-accuracy, 3 - high-coverage */
enum ds5_preset_setting {
	DS5_PRESET_DEFAULT = 1,
	DS5_PRESET_ACCURACY = 2,
	DS5_PRESET_COVERAGE = 3,
};

#endif /* DS5_H */
