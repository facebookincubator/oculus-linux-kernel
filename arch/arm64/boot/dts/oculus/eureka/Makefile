BOARD_BASE_DTB := eureka.dtb
dtb-y += $(BOARD_BASE_DTB)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-y += \
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

eureka-dev1-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-p1.2-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-evt0-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-evt1-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-evt1.5-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-evt2.0-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-evt2.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-evt2.2-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-dvt1.0-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-dvt1.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-pvt1.0-overlay.dtbo-base := $(BOARD_BASE_DTB)
eureka-pvt1.1-overlay.dtbo-base := $(BOARD_BASE_DTB)
endif # CONFIG_BUILD_ARM64_DT_OVERLAY

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
