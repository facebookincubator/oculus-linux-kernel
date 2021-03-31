dtb-$(CONFIG_ARCH_MSM8998) += \
	vs1-proto-v1.0.dtb \
	vs1-proto-v2.0.dtb \
	vs1-proto-v3.0.dtb \
	vs1-evt1.dtb \
	vs1-evt2-auo.dtb \
	vs1-evt2-sdc.dtb \
	vs1-evt3-auo.dtb \
	vs1-evt3-sdc.dtb \
	vs1-dvt-auo.dtb \
	vs1-dvt-sdc.dtb \
	vs1-pvt-auo.dtb \
	vs1-pvt-sdc.dtb \
	vs1-pvt-b-auo.dtb \
	vs1-pvt-b-sdc.dtb

ifeq ($(CONFIG_ARM64),y)
always          := $(dtb-y)
subdir-y        := $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(dtb-y))
endif
clean-files := *.dtb
