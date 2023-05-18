ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
ifeq ($(CONFIG_ARCH_KONA),y)

dtbo-$(CONFIG_OCULUS_HOLLYWOOD) += \
	hollywood-poc2-overlay.dtbo \
	hollywood-p1.1-overlay.dtbo \
	hollywood-evt1-overlay.dtbo \
	hollywood-evt2-overlay.dtbo \
	hollywood-dvt-overlay.dtbo \
	hollywood-pvt-b-overlay.dtbo \
	hollywood-crimson-overlay.dtbo

dtbo-$(CONFIG_OCULUS_SEACLIFF) += \
	seacliff-dev2.5-overlay.dtbo \
	seacliff-p2.5-overlay.dtbo \
	seacliff-dev3-overlay.dtbo \
	seacliff-p3-overlay.dtbo \
	seacliff-p3dd-overlay.dtbo \
	seacliff-evt1-overlay.dtbo \
	seacliff-pre-evt2-overlay.dtbo \
	seacliff-evt2-overlay.dtbo \
	seacliff-evt2-mlbdoe1.1-overlay.dtbo \
	seacliff-evt2-mlbdoe1.2-overlay.dtbo \
	seacliff-dvt-overlay.dtbo \
	seacliff-dvt-mlb2-overlay.dtbo \
	seacliff-pvt-overlay.dtbo \
	seacliff-pvt-mlb2-overlay.dtbo \
	seacliff-pvt-b-overlay.dtbo \
	seacliff-pvt-b-mlb2-overlay.dtbo

dtbo-$(CONFIG_OCULUS_unknown5) += \
	unknown5-p0-overlay.dtbo \
	unknown5-poc1-nff-overlay.dtbo

hollywood-poc2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-p1.1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-evt1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-evt2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-dvt-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-pvt-b-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-comet-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
hollywood-crimson-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-dev2.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p2.5-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-dev3-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p3-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-p3dd-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-evt1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-pre-evt2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-evt2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-evt2-mlbdoe1.1-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-evt2-mlbdoe1.2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-dvt-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-dvt-mlb2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-pvt-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-pvt-mlb2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-pvt-b-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
seacliff-pvt-b-mlb2-overlay.dtbo-base := kona-oculus.dtb kona-v2-oculus.dtb kona-v2.1-oculus.dtb
unknown5-p0-overlay.dtbo-base := kona-unknown5.dtb kona-v2-unknown5.dtb kona-v2.1-unknown5.dtb
unknown5-poc1-nff-overlay.dtbo-base := kona-unknown5.dtb kona-v2-unknown5.dtb kona-v2.1-unknown5.dtb

endif
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
