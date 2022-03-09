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

# SPDX-License-Identifier: GPL-2.0
obj-$(CPTCFG_CFG80211) += cfg80211.o
obj-$(CPTCFG_LIB80211) += lib80211.o
obj-$(CPTCFG_LIB80211_CRYPT_WEP) += lib80211_crypt_wep.o
obj-$(CPTCFG_LIB80211_CRYPT_CCMP) += lib80211_crypt_ccmp.o
obj-$(CPTCFG_LIB80211_CRYPT_TKIP) += lib80211_crypt_tkip.o

#obj-$(CONFIG_WEXT_CORE) += wext-core.o
#obj-$(CONFIG_WEXT_PROC) += wext-proc.o
#obj-$(CONFIG_WEXT_SPY) += wext-spy.o
#obj-$(CONFIG_WEXT_PRIV) += wext-priv.o

cfg80211-y += core.o sysfs.o radiotap.o util.o reg.o scan.o nl80211.o
cfg80211-y += mlme.o ibss.o sme.o chan.o ethtool.o mesh.o ap.o trace.o ocb.o
cfg80211-$(CONFIG_OF) += of.o
cfg80211-$(CPTCFG_CFG80211_DEBUGFS) += debugfs.o
cfg80211-$(CPTCFG_CFG80211_WEXT) += wext-compat.o wext-sme.o
cfg80211-$(CPTCFG_CFG80211_INTERNAL_REGDB) += regdb.o

CFLAGS_trace.o := -I$(src)

$(obj)/regdb.c: $(src)/db.txt $(src)/genregdb.awk
	@$(AWK) -f $(srctree)/$(src)/genregdb.awk < $< > $@

clean-files := regdb.c
