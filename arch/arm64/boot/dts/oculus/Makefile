ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
ifeq ($(CONFIG_ARCH_KONA),y)

dtbo-$(CONFIG_OCULUS_EUREKA865) += \
	eureka865-dev0-overlay.dtbo \
	eureka865-dev0.5-overlay.dtbo

dtbo-$(CONFIG_OCULUS_HOLLYWOOD) += \
	hollywood-poc2-overlay.dtbo \
	hollywood-p1.1-overlay.dtbo \
	hollywood-evt1-overlay.dtbo \
	hollywood-evt2-overlay.dtbo \
	hollywood-dvt-overlay.dtbo \
	hollywood-pvt-b-overlay.dtbo \
	hollywood-crimson-overlay.dtbo

dtbo-$(CONFIG_OCULUS_unknown1) += \
	unknown1-dev0-overlay.dtbo \
	unknown1-p0dd-r0-overlay.dtbo \
	unknown1-p0dd-r1.5-overlay.dtbo \
	unknown1-p0-overlay.dtbo \
	unknown1-p1-overlay.dtbo \
	unknown1-dev2-overlay.dtbo \
	unknown1-p2-overlay.dtbo \
	unknown1-p2dd-overlay.dtbo \
	unknown1-dev2.5-overlay.dtbo \
	unknown1-p2.5-overlay.dtbo \
	unknown1-dev3-overlay.dtbo \
	unknown1-p3-overlay.dtbo \
	unknown1-evt1-overlay.dtbo \
	unknown1-evt2-overlay.dtbo \
	unknown1-dvt-overlay.dtbo \
	unknown1-p3dd-overlay.dtbo

dtbo-$(CONFIG_OCULUS_SEABRIGHT) += \
	seabright-rev0-overlay.dtbo \
	seabright-rev1-overlay.dtbo \
	seabright-rev2-overlay.dtbo

eureka865-dev0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
eureka865-dev0.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-poc2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-p1.1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-evt1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-evt2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-dvt-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-pvt-b-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-comet-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seabright-rev0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seabright-rev1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seabright-rev2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-crimson-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-dev0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p0dd-r0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p0dd-r1.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p0-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-dev2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p2dd-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-dev2.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p2.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-dev3-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p3-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-evt1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-evt2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-dvt-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown1-p3dd-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
endif
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
