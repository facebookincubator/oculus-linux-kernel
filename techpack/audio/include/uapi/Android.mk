# Use this by setting
#   LOCAL_HEADER_LIBRARIES := audio_kernel_headers

LOCAL_PATH := $(call my-dir)
MYLOCAL_PATH := $(LOCAL_PATH)

UAPI_OUT := $(PRODUCT_OUT)/obj/vendor/qcom/kona/opensource/audio-kernel/include

AUDIO_KERNEL_HEADERS := $(call all-named-files-under,*.h,linux) $(call all-named-files-under,*.h,sound)

BUILD_ROOT_RELATIVE := ../../../../../../../../

HEADER_INSTALL_DIR := kernel/qcom/$(TARGET_BOARD_PLATFORM)/scripts

include $(CLEAR_VARS)
LOCAL_MODULE                  := audio_kernel_headers
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_PREBUILT_INT_KERNEL)
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

GEN := $(addprefix $(UAPI_OUT)/,$(AUDIO_KERNEL_HEADERS))
$(GEN): $(KERNEL_USR)
$(GEN): PRIVATE_PATH := $(MYLOCAL_PATH)
$(GEN): PRIVATE_CUSTOM_TOOL = $(shell cd $(PRODUCT_OUT)/obj/kernel/qcom/$(TARGET_BOARD_PLATFORM); $(BUILD_ROOT_RELATIVE)$(HEADER_INSTALL_DIR)/headers_install.sh $(BUILD_ROOT_RELATIVE)$(dir $@) $(BUILD_ROOT_RELATIVE)$(subst $(UAPI_OUT),$(MYLOCAL_PATH),$(dir $@)) $(notdir $@))
$(GEN): $(addprefix $(MYLOCAL_PATH)/,$(AUDIO_KERNEL_HEADERS))
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES := $(GEN)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(UAPI_OUT)

include $(BUILD_HEADER_LIBRARY)
