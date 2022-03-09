BACKPORT_CONFIG := stella_cypress
BACKPORT_DIR := drivers/backports-wireless
backport_srctree := $(srctree)/$(BACKPORT_DIR)
NOSTDINC_FLAGS := \
	-I$(backport_srctree)/backport-include/ \
	-I$(backport_srctree)/backport-include/uapi \
	-I$(backport_srctree)/include/ \
	-I$(backport_srctree)/include/uapi \
	-include $(backport_srctree)/backport-include/backport/backport.h \
	-include $(backport_srctree)/defconfigs/$(BACKPORT_CONFIG)_conf.h \
	$(KBUILD_CFLAGS)

include $(backport_srctree)/defconfigs/$(BACKPORT_CONFIG).conf

#obj-$(CONFIG_B43)		+= b43/
#obj-$(CONFIG_B43LEGACY)		+= b43legacy/

obj-$(CPTCFG_BRCMFMAC)	+= brcm80211/
#obj-$(CPTCFG_BRCMSMAC)	+= brcm80211/
