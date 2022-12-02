#ifndef DS5_H
#define DS5_H

#include <linux/types.h>
#include <linux/ioctl.h>

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
};

#define DS5_IOCTL_MAGIC 'i'

#define DS5_GET_FW_VERSION _IOR(DS5_IOCTL_MAGIC, 0xA0, struct ds5_fw_version)
#define DS5_GET_STATUS     _IOR(DS5_IOCTL_MAGIC, 0xA4, struct ds5_status)

#define DS5_STREAM_CONFIG  _IOW(DS5_IOCTL_MAGIC, 0xA1, struct ds5_stream_config)
#define DS5_STREAM_START   _IO(DS5_IOCTL_MAGIC, 0xA2)
#define DS5_STREAM_STOP    _IO(DS5_IOCTL_MAGIC, 0xA3)

#endif /* DS5_H */
