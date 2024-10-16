
ifeq ($(CONFIG_ARCH_WAIPIO), y)
dtbo-y += waipio-vidc.dtbo
endif

ifeq ($(CONFIG_ARCH_KALAMA), y)
dtbo-y += kalama-vidc.dtbo
dtbo-y += kalama-vidc-v2.dtbo
dtbo-y += kalama-vidc-iot.dtbo
endif

ifeq ($(CONFIG_ARCH_CROW), y)
dtbo-y += crow-vidc.dtbo
endif

ifeq ($(CONFIG_ARCH_ANORAK), y)
dtbo-y += anorak-vidc.dtbo
endif

always-y    := $(dtb-y) $(dtbo-y)
subdir-y    := $(dts-dirs)
clean-files    := *.dtb *.dtbo
