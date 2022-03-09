# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux Wireless network device drivers.
#
#

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

#obj-$(CONFIG_WLAN_VENDOR_ADMTEK) += admtek/
#obj-$(CONFIG_WLAN_VENDOR_ATH) += ath/
#obj-$(CONFIG_WLAN_VENDOR_ATMEL) += atmel/
obj-$(CPTCFG_WLAN_VENDOR_BROADCOM) += broadcom/
#obj-$(CONFIG_WLAN_VENDOR_CISCO) += cisco/
#obj-$(CONFIG_WLAN_VENDOR_INTEL) += intel/
#obj-$(CONFIG_WLAN_VENDOR_INTERSIL) += intersil/
#obj-$(CONFIG_WLAN_VENDOR_MARVELL) += marvell/
#obj-$(CONFIG_WLAN_VENDOR_MEDIATEK) += mediatek/
#obj-$(CONFIG_WLAN_VENDOR_RALINK) += ralink/
#obj-$(CONFIG_WLAN_VENDOR_REALTEK) += realtek/
#obj-$(CONFIG_WLAN_VENDOR_RSI) += rsi/
#obj-$(CONFIG_WLAN_VENDOR_ST) += st/
#obj-$(CONFIG_WLAN_VENDOR_TI) += ti/
#obj-$(CONFIG_WLAN_VENDOR_ZYDAS) += zydas/
#obj-$(CONFIG_WLAN_VENDOR_QUANTENNA) += quantenna/

# 16-bit wireless PCMCIA client drivers
#obj-$(CPTCFG_PCMCIA_RAYCS)	+= ray_cs.o
#obj-$(CPTCFG_PCMCIA_WL3501)	+= wl3501_cs.o
#
#obj-$(CPTCFG_USB_NET_RNDIS_WLAN)	+= rndis_wlan.o
#
#obj-$(CPTCFG_MAC80211_HWSIM)	+= mac80211_hwsim.o
