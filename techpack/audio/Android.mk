# Android makefile for audio kernel modules
MY_LOCAL_PATH := $(call my-dir)

UAPI_OUT := $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/include

ifeq ($(call is-board-platform-in-list,msmnile $(MSMSTEPPE) $(TRINKET) kona lito bengal sdmshrike sdm660),true)
$(shell mkdir -p $(UAPI_OUT)/linux;)
$(shell mkdir -p $(UAPI_OUT)/sound;)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/ipc/Module.symvers)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/dsp/Module.symvers)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/dsp/codecs/Module.symvers)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/soc/Module.symvers)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/Module.symvers)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/Module.symvers)

include $(MY_LOCAL_PATH)/include/uapi/Android.mk
include $(MY_LOCAL_PATH)/ipc/Android.mk
include $(MY_LOCAL_PATH)/dsp/Android.mk
include $(MY_LOCAL_PATH)/dsp/codecs/Android.mk
include $(MY_LOCAL_PATH)/soc/Android.mk
include $(MY_LOCAL_PATH)/asoc/Android.mk
include $(MY_LOCAL_PATH)/asoc/codecs/Android.mk
endif

ifeq ($(call is-board-platform-in-list,msmnile $(MSMSTEPPE) $(TRINKET) sdmshrike),true)
ifneq ($(TARGET_BOARD_AUTO),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/wcd934x/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/wcd934x/Android.mk
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/soc/Module.symvers)
include $(MY_LOCAL_PATH)/soc/Android.mk
endif
endif

ifeq ($(call is-board-platform-in-list,sdm660),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/wcd934x/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/wcd934x/Android.mk
endif

ifeq ($(call is-board-platform-in-list,msmnile sdmshrike),true)
ifneq ($(TARGET_BOARD_AUTO),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/aqt1000/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/aqt1000/Android.mk
endif
endif

ifeq ($(call is-board-platform-in-list,$(MSMSTEPPE) $(TRINKET) bengal),true)
ifneq ($(TARGET_BOARD_AUTO),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/bolero/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/bolero/Android.mk
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/wcd937x/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/wcd937x/Android.mk
endif
endif

ifeq ($(call is-board-platform-in-list, bengal),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/rouleur/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/rouleur/Android.mk
endif

ifeq ($(call is-board-platform-in-list, kona lito),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/bolero/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/bolero/Android.mk
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/wcd938x/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/wcd938x/Android.mk
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/wcd937x/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/wcd937x/Android.mk
endif

ifeq ($(call is-board-platform-in-list, lito),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/wsa883x/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/wsa883x/Android.mk
endif

ifeq ($(call is-board-platform-in-list, sdm660),true)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/sdm660_cdc/Module.symvers)
$(shell rm -rf $(PRODUCT_OUT)/obj/vendor/qcom/opensource/audio-kernel/asoc/codecs/msm_sdw/Module.symvers)
include $(MY_LOCAL_PATH)/asoc/codecs/sdm660_cdc/Android.mk
include $(MY_LOCAL_PATH)/asoc/codecs/msm_sdw/Android.mk
endif
