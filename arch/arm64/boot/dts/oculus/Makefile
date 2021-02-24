ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_KONA) += \
	hollywood-poc2-overlay.dtbo \
	hollywood-p1-1-overlay.dtbo \
	hollywood-evt1-overlay.dtbo \
	hollywood-evt2-overlay.dtbo \
	hollywood-dvt-overlay.dtbo \
	unknown1-dev0-overlay.dtbo \
	unknown1-p0dd-overlay.dtbo \
	unknown1-p0-overlay.dtbo \
	unknown1-p1-overlay.dtbo \
	unknown1-p1-5dd-overlay.dtbo \
	unknown1-dev2-overlay.dtbo \
	unknown1-p2-overlay.dtbo

hollywood-poc2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
hollywood-p1-1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
hollywood-evt1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
hollywood-evt2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
hollywood-dvt-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
unknown1-dev0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
unknown1-p0dd-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
unknown1-p0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
unknown1-p1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
unknown1-p1-5dd-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
unknown1-dev2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
unknown1-p2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
