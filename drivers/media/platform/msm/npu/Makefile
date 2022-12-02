# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_MSM_NPU)      := msm_npu.o
msm_npu-objs := npu_dbg.o \
                npu_dev.o \
                npu_host_ipc.o \
                npu_hw_access.o \
                npu_mgr.o

ifeq ($(CONFIG_DEBUG_FS),y)
  msm_npu-objs += npu_debugfs.o
endif
