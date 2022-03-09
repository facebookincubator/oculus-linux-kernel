stella_base_dtb_path := stella.dtb
stella-dev0_base_dtb_path := stella-dev0.dtb
ts_base_dtb_path := qcom-base/sdm429-spyro-dvt.dtb

dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-dev0-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-dev0r-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-dev1-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-dvt-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-evt1-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-evt2-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-evt3-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-mp-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-p1-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += stella-pvt-overlay.dtbo
dtbo-$(CONFIG_TARGET_IMAGE_STELLA) += ts-dev0-overlay.dtbo
dtb-$(CONFIG_TARGET_IMAGE_STELLA) += $(stella_base_dtb_path) $(stella-dev0_base_dtb_path) $(ts_base_dtb_path)

stella-p1-overlay.dtbo-base := $(stella_base_dtb_path)
stella-dev0r-overlay.dtbo-base := $(stella_base_dtb_path)
stella-dev0-overlay.dtbo-base := $(stella-dev0_base_dtb_path)
ts-dev0-overlay.dtbo-base := $(ts_base_dtb_path)

dtbo-$(CONFIG_TARGET_IMAGE_MOS) += mos-dev0-overlay.dtbo
dtb-$(CONFIG_TARGET_IMAGE_MOS) +=  mos-dev0.dtb
mos-dev0-overlay.dtbo-base := mos-dev0.dtb

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbs: $(addprefix $(obj)/,$(dtb-y)) $(addprefix $(obj)/,$(dtbo-y))
else
dtbs: $(addprefix $(obj)/,$(dtb-y))
endif
clean-files := *.dtb
