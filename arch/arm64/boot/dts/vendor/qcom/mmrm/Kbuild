ifeq ($(CONFIG_ARCH_WAIPIO), y)
dtbo-y += waipio-mmrm.dtbo
dtbo-y += waipio-mmrm-test.dtbo
dtbo-y += waipio-v2-mmrm.dtbo
dtbo-y += waipio-v2-mmrm-test.dtbo
endif

ifeq ($(CONFIG_ARCH_NEO), y)
dtbo-y += neo-mmrm.dtbo
dtbo-y += neo-mmrm-test.dtbo
endif

ifeq ($(CONFIG_ARCH_CAPE), y)
dtbo-y += cape-mmrm.dtbo
endif

ifeq ($(CONFIG_ARCH_DIWALI), y)
dtbo-y += diwali-mmrm.dtbo
endif

always-y	:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
