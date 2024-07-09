# SPDX-License-Identifier: GPL-2.0
hostprogs-y	:= genheaders
HOST_PREFERRED_INCLUDE_CFLAGS += \
	-I$(srctree)/include/uapi -I$(srctree)/include \
	-I$(srctree)/security/selinux/include

always		:= $(hostprogs-y)
