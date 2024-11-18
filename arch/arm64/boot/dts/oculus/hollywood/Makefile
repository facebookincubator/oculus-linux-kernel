BOARD_BASE_DTBS := ../kona-oculus.dtb ../kona-v2-oculus.dtb ../kona-v2.1-oculus.dtb
dtb-y += $(BOARD_BASE_DTBS)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-y += \
	hollywood-poc2-overlay.dtbo \
	hollywood-p1.1-overlay.dtbo \
	hollywood-evt1-overlay.dtbo \
	hollywood-evt2-overlay.dtbo \
	hollywood-dvt-overlay.dtbo \
	hollywood-pvt-b-overlay.dtbo

hollywood-poc2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
hollywood-p1.1-overlay.dtbo-base := $(BOARD_BASE_DTBS)
hollywood-evt1-overlay.dtbo-base := $(BOARD_BASE_DTBS)
hollywood-evt2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
hollywood-dvt-overlay.dtbo-base := $(BOARD_BASE_DTBS)
hollywood-pvt-b-overlay.dtbo-base := $(BOARD_BASE_DTBS)
endif # CONFIG_BUILD_ARM64_DT_OVERLAY

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
