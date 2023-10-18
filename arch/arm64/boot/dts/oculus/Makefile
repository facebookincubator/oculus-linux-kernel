ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
ifeq ($(CONFIG_ARCH_ANORAK),y)

dtbo-$(CONFIG_OCULUS_unknown7XR2G2) += \
	unknown7xr2g2-dev0-overlay.dtbo \
	unknown7xr2g2-p0-overlay.dtbo \
	unknown7xr2g2-p1-overlay.dtbo \

dtbo-$(CONFIG_OCULUS_EUREKA) += \
	eureka-dev1-overlay.dtbo \
	eureka-p1.2-overlay.dtbo \
	eureka-evt0-overlay.dtbo \
	eureka-evt1-overlay.dtbo \
	eureka-evt1.5-overlay.dtbo \
	eureka-evt2.0-overlay.dtbo \
	eureka-evt2.1-overlay.dtbo \
	eureka-evt2.2-overlay.dtbo \
	eureka-dvt1.0-overlay.dtbo \
	eureka-dvt1.1-overlay.dtbo \
	eureka-pvt1.0-overlay.dtbo \
	eureka-pvt1.1-overlay.dtbo

dtbo-$(CONFIG_OCULUS_unknown6) += \
	unknown6-dev1-overlay.dtbo \
	unknown6-p1.1-overlay.dtbo \
	unknown6-p1.2-overlay.dtbo \
	unknown6-p1.3-overlay.dtbo \
	unknown6-p1.4-overlay.dtbo \
	unknown6-p1.5-overlay.dtbo \
	unknown6-p1.6-overlay.dtbo \
	unknown6-p1.7-overlay.dtbo

unknown7xr2g2-dev0-overlay.dtbo-base := unknown7xr2g2.dtb
unknown7xr2g2-p0-overlay.dtbo-base := unknown7xr2g2.dtb
unknown7xr2g2-p1-overlay.dtbo-base := unknown7xr2g2.dtb
eureka-dev1-overlay.dtbo-base := eureka.dtb
eureka-p1.2-overlay.dtbo-base := eureka.dtb
eureka-evt0-overlay.dtbo-base := eureka.dtb
eureka-evt1-overlay.dtbo-base := eureka.dtb
eureka-evt1.5-overlay.dtbo-base := eureka.dtb
eureka-evt2.0-overlay.dtbo-base := eureka.dtb
eureka-evt2.1-overlay.dtbo-base := eureka.dtb
eureka-evt2.2-overlay.dtbo-base := eureka.dtb
eureka-dvt1.0-overlay.dtbo-base := eureka.dtb
eureka-dvt1.1-overlay.dtbo-base := eureka.dtb
eureka-pvt1.0-overlay.dtbo-base := eureka.dtb
eureka-pvt1.1-overlay.dtbo-base := eureka.dtb
unknown6-dev1-overlay.dtbo-base := unknown6.dtb
unknown6-p1.1-overlay.dtbo-base := unknown6.dtb
unknown6-p1.2-overlay.dtbo-base := unknown6.dtb
unknown6-p1.3-overlay.dtbo-base := unknown6.dtb
unknown6-p1.4-overlay.dtbo-base := unknown6.dtb
unknown6-p1.5-overlay.dtbo-base := unknown6.dtb
unknown6-p1.6-overlay.dtbo-base := unknown6.dtb
unknown6-p1.7-overlay.dtbo-base := unknown6.dtb

endif # CONFIG_ARCH_ANORAK
endif # CONFIG_BUILD_ARM64_DT_OVERLAY

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
