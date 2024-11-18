BOARD_BASE_DTB := panther.dtb
dtb-y += $(BOARD_BASE_DTB)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-y += \
	panther-dev1-overlay.dtbo \
	panther-p1.1-overlay.dtbo \
	panther-p1.2-overlay.dtbo \
	panther-p1.3-overlay.dtbo \
	panther-p1.4-overlay.dtbo \
	panther-p1.5-overlay.dtbo \
	panther-p1.6-overlay.dtbo \
	panther-p1.7-overlay.dtbo \
	panther-evt1.1-overlay.dtbo \
	panther-evt1.2-overlay.dtbo \
	panther-evt1.3-overlay.dtbo \
	panther-evt1.4-overlay.dtbo \
	panther-evt1.5-overlay.dtbo \
	panther-evt1.6-overlay.dtbo \
	panther-evt1.7-overlay.dtbo \
	panther-evt1.8-overlay.dtbo \
	panther-evt2.1-overlay.dtbo \
	panther-evt2.2-overlay.dtbo \
	panther-evt2.3-overlay.dtbo \
	panther-dvt1.1-overlay.dtbo \
	panther-dvt1.2-overlay.dtbo \
	panther-dvt1.3-overlay.dtbo \
	panther-pvt1.1-overlay.dtbo \
	panther-pvt1.2-overlay.dtbo \
	panther-pvt1.3-overlay.dtbo

panther-dev1-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-p1.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-p1.2-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-p1.3-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-p1.4-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-p1.5-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-p1.6-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-p1.7-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.2-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.3-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.4-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.5-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.6-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.7-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt1.8-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt2.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt2.2-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-evt2.3-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-dvt1.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-dvt1.2-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-dvt1.3-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-pvt1.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-pvt1.2-overlay.dtbo-base := $(BOARD_BASE_DTB)
panther-pvt1.3-overlay.dtbo-base := $(BOARD_BASE_DTB)
endif # CONFIG_BUILD_ARM64_DT_OVERLAY

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo

