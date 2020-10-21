# SPDX-License-Identifier: GPL-2.0
hostprogs-y	:= genheaders
HOST_EXTRACFLAGS += \
	-I$(srctree)/tools/include/uapi -I$(srctree)/tools/include \
	-I$(srctree)/security/selinux/include

always		:= $(hostprogs-y)
