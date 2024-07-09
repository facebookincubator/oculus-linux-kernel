ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
ifeq ($(CONFIG_ARCH_BENGAL),y)

dtbo-$(CONFIG_OCULUS_STARLET) += \
	starlet-daedalus-overlay.dtbo \
	starlet-dev2-overlay.dtbo \
	starlet-dev2-som-overlay.dtbo \
	starlet-p2-overlay.dtbo \
	starlet-evt1-overlay.dtbo \
	starlet-evt2-overlay.dtbo \
	starlet-evt3-overlay.dtbo \
	starlet-dvt-overlay.dtbo \
	starlet-pvt-overlay.dtbo \
	starlet-raven-overlay.dtbo \

starlet-daedalus-overlay.dtbo-base := starlet.dtb
starlet-dev1-overlay.dtbo-base := starlet.dtb
starlet-dev2-overlay.dtbo-base := starlet.dtb
starlet-dev2-som-overlay.dtbo-base := starlet.dtb
starlet-p2-overlay.dtbo-base := starlet.dtb
starlet-evt1-overlay.dtbo-base := starlet.dtb
starlet-evt2-overlay.dtbo-base := starlet.dtb
starlet-evt3-overlay.dtbo-base := starlet.dtb
starlet-dvt-overlay.dtbo-base := starlet.dtb
starlet-pvt-overlay.dtbo-base := starlet.dtb
starlet-raven-overlay.dtbo-base := starlet.dtb
endif
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
