ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
ifeq ($(CONFIG_ARCH_NEO),y)

dtbo-$(CONFIG_META_GREATWHITE) += \
    greatwhite-dev0-overlay.dtbo \
    greatwhite-dev0.1-overlay.dtbo \
    greatwhite-prep1-overlay.dtbo \
    greatwhite-prep1p-overlay.dtbo \
    greatwhite-dev0-2023-overlay.dtbo

dtbo-$(CONFIG_META_HAMMERHEAD) += \
    hammerhead-dev0-overlay.dtbo \
    hammerhead-dev0.1-overlay.dtbo \
    hammerhead-dev1-overlay.dtbo \
    hammerhead-p1-overlay.dtbo \
    hammerhead-evt1-overlay.dtbo \
    hammerhead-evt1.1-overlay.dtbo \
    hammerhead-evt2-overlay.dtbo \
    hammerhead-dvt-overlay.dtbo \
    hammerhead-pvt-overlay.dtbo

dtbo-$(CONFIG_META_OATMEAL) += \
    oatmeal-nff0-overlay.dtbo \
    oatmeal-nff1-overlay.dtbo \
    oatmeal-p0-overlay.dtbo

greatwhite-dev0-overlay.dtbo-base := greatwhite-base.dtb
greatwhite-dev0.1-overlay.dtbo-base := greatwhite-base.dtb
greatwhite-prep1-overlay.dtbo-base := greatwhite-base.dtb
greatwhite-prep1p-overlay.dtbo-base := greatwhite-base.dtb
greatwhite-dev0-2023-overlay.dtbo-base := greatwhite-base.dtb
hammerhead-dev0-overlay.dtbo-base := hammerhead-base.dtb
hammerhead-dev0.1-overlay.dtbo-base := hammerhead-base.dtb
hammerhead-dev1-overlay.dtbo-base := hammerhead-base.dtb
hammerhead-p1-overlay.dtbo-base := hammerhead-base.dtb
hammerhead-evt1-overlay.dtbo-base :=  hammerhead-base.dtb
hammerhead-evt1.1-overlay.dtbo-base :=  hammerhead-base.dtb
hammerhead-evt2-overlay.dtbo-base :=  hammerhead-base.dtb
hammerhead-dvt-overlay.dtbo-base :=  hammerhead-base.dtb
hammerhead-pvt-overlay.dtbo-base :=  hammerhead-base.dtb
oatmeal-nff0-overlay.dtbo-base :=  oatmeal-base.dtb
oatmeal-nff1-overlay.dtbo-base :=  oatmeal-base.dtb
oatmeal-p0-overlay.dtbo-base :=  oatmeal-base.dtb

endif
endif

always    := $(dtb-y)
subdir-y  := $(dts-dirs)
clean-files := *.dtb *.dtbo
