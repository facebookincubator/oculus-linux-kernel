ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_KONA) += \
	hollywood-poc2-overlay.dtbo \
	hollywood-p1-1-overlay.dtbo \
	hollywood-evt1-overlay.dtbo \
	hollywood-evt2-overlay.dtbo \
	hollywood-dvt-overlay.dtbo \
	seacliff-dev0-overlay.dtbo \
	seacliff-p0dd-overlay.dtbo \
	seacliff-p0-overlay.dtbo \
	seacliff-p1-overlay.dtbo \
	seacliff-p1-5dd-overlay.dtbo \
	seacliff-dev2-overlay.dtbo \
	seacliff-p2-overlay.dtbo

hollywood-poc2-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
hollywood-p1-1-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
hollywood-evt1-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
hollywood-evt2-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
hollywood-dvt-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
seacliff-dev0-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
seacliff-p0dd-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
seacliff-p0-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
seacliff-p1-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
seacliff-p1-5dd-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
seacliff-dev2-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
seacliff-p2-overlay.dtbo-base := vendor-base/qcom/kona.dtb vendor-base/qcom/kona-v2.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
