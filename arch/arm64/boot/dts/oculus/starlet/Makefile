BOARD_BASE_DTB := starlet.dtb
dtb-y += $(BOARD_BASE_DTB)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-y += \
	starlet-daedalus-overlay.dtbo \
	starlet-dev2-overlay.dtbo \
	starlet-dev2-som-overlay.dtbo \
	starlet-p2-overlay.dtbo \
	starlet-evt1-overlay.dtbo \
	starlet-evt2-overlay.dtbo \
	starlet-evt3-overlay.dtbo \
	starlet-dvt-overlay.dtbo \
	starlet-pvt-overlay.dtbo \
	starlet-raven-overlay.dtbo

starlet-daedalus-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-dev1-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-dev2-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-dev2-som-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-p2-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-evt1-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-evt2-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-evt3-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-dvt-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-pvt-overlay.dtbo-base := $(BOARD_BASE_DTB)
starlet-raven-overlay.dtbo-base := $(BOARD_BASE_DTB)
endif # CONFIG_BUILD_ARM64_DT_OVERLAY

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
