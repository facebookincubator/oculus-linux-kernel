TARGET_MMRM_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE),true)
	ifeq ($(TARGET_KERNEL_DLKM_MMRM_OVERRIDE),true)
		TARGET_MMRM_ENABLE := true
	endif
else
TARGET_MMRM_ENABLE := true
endif

ifeq ($(TARGET_MMRM_ENABLE),true)
PRODUCT_PACKAGES += msm-mmrm.ko
endif