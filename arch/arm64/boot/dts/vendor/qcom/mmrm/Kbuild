
ifeq ($(CONFIG_ARCH_KALAMA), y)
	ifneq ($(CONFIG_ARCH_QTI_VM), y)
		dtbo-y += kalama-mmrm.dtbo
		dtbo-y += kalama-mmrm-test.dtbo
		dtbo-y += kalama-mmrm-v2.dtbo
		dtbo-y += kalama-mmrm-test-v2.dtbo
		ifeq ($(CONFIG_MSM_MMRM_VM),y)
		    dtbo-y += kalama-mmrm-vm-be.dtbo
		endif
	else
		ifeq ($(CONFIG_MSM_MMRM_VM),y)
		    dtbo-y += kalama-mmrm-vm-fe.dtbo
		    dtbo-y += kalama-mmrm-vm-fe-test.dtbo
		endif
	endif
endif

ifeq ($(CONFIG_ARCH_WAIPIO), y)
dtbo-y += waipio-mmrm.dtbo
dtbo-y += waipio-mmrm-test.dtbo
dtbo-y += waipio-v2-mmrm.dtbo
dtbo-y += waipio-v2-mmrm-test.dtbo
endif

always-y	:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
