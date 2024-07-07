ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
ifeq ($(CONFIG_ARCH_ANORAK),y)

dtbo-$(CONFIG_META_unknown12) += \
	unknown12-dev0-overlay.dtbo \
	unknown12-pocb-overlay.dtbo \
	unknown12-nff-overlay.dtbo

dtbo-$(CONFIG_OCULUS_unknown7) += \
	unknown7-dev0-overlay.dtbo \
	unknown7-dev0.5-overlay.dtbo \
	unknown7-p0-overlay.dtbo \
	unknown7-p1-overlay.dtbo


dtbo-$(CONFIG_OCULUS_unknown9) += \
	unknown9-p0-overlay.dtbo \
	unknown9-p1-overlay.dtbo \
	unknown9-p1.1-overlay.dtbo

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
	eureka-pvt1.1-overlay.dtbo \
	eureka-et1.0-overlay.dtbo

dtbo-$(CONFIG_OCULUS_unknown14) += \
	unknown14-rev1-overlay.dtbo \
	unknown14-rev1-wcn7851-overlay.dtbo \
	unknown14-rev2-overlay.dtbo \
	unknown14-rev2-dtof-overlay.dtbo

dtbo-$(CONFIG_OCULUS_unknown6) += \
	unknown6-dev1-overlay.dtbo \
	unknown6-p1.1-overlay.dtbo \
	unknown6-p1.2-overlay.dtbo \
	unknown6-p1.3-overlay.dtbo \
	unknown6-p1.4-overlay.dtbo \
	unknown6-p1.5-overlay.dtbo \
	unknown6-p1.6-overlay.dtbo \
	unknown6-p1.7-overlay.dtbo \
	unknown6-evt1.1-overlay.dtbo \
	unknown6-evt1.2-overlay.dtbo \
	unknown6-evt1.3-overlay.dtbo \
	unknown6-evt1.4-overlay.dtbo \
	unknown6-evt1.5-overlay.dtbo \
	unknown6-evt1.6-overlay.dtbo \
	unknown6-evt1.7-overlay.dtbo \
	unknown6-evt1.8-overlay.dtbo \
	unknown6-evt2.1-overlay.dtbo \
	unknown6-evt2.2-overlay.dtbo \
	unknown6-evt2.3-overlay.dtbo \
	unknown6-dvt1.1-overlay.dtbo \
	unknown6-dvt1.2-overlay.dtbo \
	unknown6-dvt1.3-overlay.dtbo

dtbo-$(CONFIG_OCULUS_unknown13) += \
	unknown13-dev0-overlay.dtbo \
	unknown13-poc0-overlay.dtbo

unknown12-dev0-overlay.dtbo-base := unknown12.dtb
unknown12-pocb-overlay.dtbo-base := unknown12.dtb
unknown12-nff-overlay.dtbo-base := unknown12.dtb
unknown7-dev0-overlay.dtbo-base := unknown7.dtb
unknown7-dev0.5-overlay.dtbo-base := unknown7.dtb
unknown7-p0-overlay.dtbo-base := unknown7.dtb
unknown7-p1-overlay.dtbo-base := unknown7.dtb
unknown9-p0-overlay.dtbo-base := unknown9.dtb
unknown9-p1-overlay.dtbo-base := unknown9.dtb
unknown9-p1.1-overlay.dtbo-base := unknown9.dtb
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
eureka-et1.0-overlay.dtbo-base := eureka.dtb
unknown14-rev1-overlay.dtbo-base := unknown14.dtb
unknown14-rev1-wcn7851-overlay.dtbo-base := unknown14.dtb
unknown14-rev2-overlay.dtbo-base := unknown14.dtb
unknown14-rev2-dtof-overlay.dtbo-base := unknown14.dtb
unknown6-dev1-overlay.dtbo-base := unknown6.dtb
unknown6-p1.1-overlay.dtbo-base := unknown6.dtb
unknown6-p1.2-overlay.dtbo-base := unknown6.dtb
unknown6-p1.3-overlay.dtbo-base := unknown6.dtb
unknown6-p1.4-overlay.dtbo-base := unknown6.dtb
unknown6-p1.5-overlay.dtbo-base := unknown6.dtb
unknown6-p1.6-overlay.dtbo-base := unknown6.dtb
unknown6-p1.7-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.1-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.2-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.3-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.4-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.5-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.6-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.7-overlay.dtbo-base := unknown6.dtb
unknown6-evt1.8-overlay.dtbo-base := unknown6.dtb
unknown6-evt2.1-overlay.dtbo-base := unknown6.dtb
unknown6-evt2.2-overlay.dtbo-base := unknown6.dtb
unknown6-evt2.3-overlay.dtbo-base := unknown6.dtb
unknown6-dvt1.1-overlay.dtbo-base := unknown6.dtb
unknown6-dvt1.2-overlay.dtbo-base := unknown6.dtb
unknown6-dvt1.3-overlay.dtbo-base := unknown6.dtb
unknown13-dev0-overlay.dtbo-base := unknown13.dtb
unknown13-poc0-overlay.dtbo-base := unknown13.dtb

endif # CONFIG_ARCH_ANORAK
endif # CONFIG_BUILD_ARM64_DT_OVERLAY

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
