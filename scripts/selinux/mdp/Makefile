# SPDX-License-Identifier: GPL-2.0
hostprogs-y	:= mdp
HOST_PREFERRED_INCLUDE_CFLAGS += \
	-I$(srctree)/tools/include/uapi -I$(srctree)/tools/include \
	-I$(srctree)/security/selinux/include

always		:= $(hostprogs-y)
clean-files	:= policy.* file_contexts
