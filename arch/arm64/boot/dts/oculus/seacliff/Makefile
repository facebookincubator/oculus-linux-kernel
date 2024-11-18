BOARD_BASE_DTBS := ../kona-oculus.dtb ../kona-v2-oculus.dtb ../kona-v2.1-oculus.dtb
dtb-y += $(BOARD_BASE_DTBS)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-y += \
	seacliff-dev2.5-overlay.dtbo \
	seacliff-p2.5-overlay.dtbo \
	seacliff-dev3-overlay.dtbo \
	seacliff-p3-overlay.dtbo \
	seacliff-p3dd-overlay.dtbo \
	seacliff-evt1-overlay.dtbo \
	seacliff-pre-evt2-overlay.dtbo \
	seacliff-evt2-overlay.dtbo \
	seacliff-evt2-mlbdoe1.1-overlay.dtbo \
	seacliff-evt2-mlbdoe1.2-overlay.dtbo \
	seacliff-dvt-overlay.dtbo \
	seacliff-dvt-mlb2-overlay.dtbo \
	seacliff-pvt-overlay.dtbo \
	seacliff-pvt-mlb2-overlay.dtbo \
	seacliff-pvt-b-overlay.dtbo \
	seacliff-pvt-b-mlb2-overlay.dtbo

seacliff-dev2.5-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-p2.5-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-dev3-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-p3-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-p3dd-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-evt1-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-pre-evt2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-evt2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-evt2-mlbdoe1.1-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-evt2-mlbdoe1.2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-dvt-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-dvt-mlb2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-pvt-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-pvt-mlb2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-pvt-b-overlay.dtbo-base := $(BOARD_BASE_DTBS)
seacliff-pvt-b-mlb2-overlay.dtbo-base := $(BOARD_BASE_DTBS)
endif # CONFIG_BUILD_ARM64_DT_OVERLAY

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
