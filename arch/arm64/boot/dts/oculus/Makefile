ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_KONA) += \
	hollywood-poc2-overlay.dtbo \
	hollywood-p1.1-overlay.dtbo \
	hollywood-evt1-overlay.dtbo \
	hollywood-evt2-overlay.dtbo \
	hollywood-dvt-overlay.dtbo \
	hollywood-comet-overlay.dtbo \
	seacliff-dev0-overlay.dtbo \
	seacliff-p0dd-r0-overlay.dtbo \
	seacliff-p0dd-r1.5-overlay.dtbo \
	seacliff-p0-overlay.dtbo \
	seacliff-p1-overlay.dtbo \
	seacliff-dev2-overlay.dtbo \
	seacliff-p2-overlay.dtbo \
	seacliff-dev3.5-overlay.dtbo \
	seacliff-p2.5-overlay.dtbo

hollywood-poc2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-p1.1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-evt1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-evt2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-dvt-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-comet-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-dev0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p0dd-r0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p0dd-r1.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-dev2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-dev3.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p2.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
