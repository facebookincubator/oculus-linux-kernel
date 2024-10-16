# SPDX-License-Identifier: GPL-2.0-only
TARGET_VIDC_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
	ifeq ($(TARGET_KERNEL_DLKM_VIDEO_OVERRIDE), true)
		TARGET_VIDC_ENABLE := true
	endif
else
TARGET_VIDC_ENABLE := true
endif

ifeq ($(TARGET_VIDC_ENABLE),true)
PRODUCT_PACKAGES += msm_video.ko
endif
