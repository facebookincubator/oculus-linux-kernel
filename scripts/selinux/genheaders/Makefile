# SPDX-License-Identifier: GPL-2.0
hostprogs-y	:= genheaders
HOST_PREFERRED_INCLUDE_CFLAGS += \
	-I$(srctree)/tools/include/uapi -I$(srctree)/tools/include \
	-I$(srctree)/security/selinux/include

always		:= $(hostprogs-y)
