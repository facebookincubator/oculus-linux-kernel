TARGET_MMRM_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE),true)
	ifeq ($(TARGET_KERNEL_DLKM_MMRM_OVERRIDE),true)
		TARGET_MMRM_ENABLE := true
	endif
else
TARGET_MMRM_ENABLE := true
endif

ifeq ($(TARGET_MMRM_ENABLE),true)
MMRM_BLD_DIR := $(shell pwd)/vendor/qcom/opensource/mmrm-driver

# Build msm-mmrm.ko
###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := MMRM_ROOT=$(MMRM_BLD_DIR)
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
###########################################################

DLKM_DIR   := device/qcom/common/dlkm

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES           := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
LOCAL_MODULE              := mmrm-module-symvers
LOCAL_MODULE_STEM         := Module.symvers
LOCAL_MODULE_KBUILD_NAME  := Module.symvers
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
# Include kp_module.ko in the /vendor/lib/modules (vendor.img)
# BOARD_VENDOR_KERNEL_MODULES += $(LOCAL_MODULE_PATH)/$(LOCAL_MODULE)
include $(DLKM_DIR)/Build_external_kernelmodule.mk

include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES           := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
LOCAL_MODULE              := msm-mmrm.ko
LOCAL_MODULE_KBUILD_NAME  := driver/msm-mmrm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
# Include kp_module.ko in the /vendor/lib/modules (vendor.img)
# BOARD_VENDOR_KERNEL_MODULES += $(LOCAL_MODULE_PATH)/$(LOCAL_MODULE)
include $(DLKM_DIR)/Build_external_kernelmodule.mk

include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES           := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
LOCAL_MODULE              := mmrm_test_module.ko
LOCAL_MODULE_KBUILD_NAME  := test/mmrm_test_module.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
# Include kp_module.ko in the /vendor/lib/modules (vendor.img)
# BOARD_VENDOR_KERNEL_MODULES += $(LOCAL_MODULE_PATH)/$(LOCAL_MODULE)
include $(DLKM_DIR)/Build_external_kernelmodule.mk

ifeq ($(CONFIG_MSM_MMRM_VM),y)
	include $(CLEAR_VARS)
	# For incremental compilation
	LOCAL_SRC_FILES           := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := mmrm_vm_be.ko
	LOCAL_MODULE_KBUILD_NAME  := vm/be/mmrm_vm_be.ko
	LOCAL_MODULE_TAGS         := optional
	LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	LOCAL_INIT_RC             := vm/be/src/mmrm_vm_be.rc
	LOCAL_C_INCLUDES          := vm/common/inc/
	# Include kp_module.ko in the /vendor/lib/modules (vendor.img)
	# BOARD_VENDOR_KERNEL_MODULES += $(LOCAL_MODULE_PATH)/$(LOCAL_MODULE)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
endif
endif
