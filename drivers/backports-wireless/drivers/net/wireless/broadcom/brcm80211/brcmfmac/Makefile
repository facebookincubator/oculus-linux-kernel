#
# Makefile fragment for Broadcom 802.11n Networking Device Driver
#
# Copyright (c) 2010 Broadcom Corporation
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

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

ccflags-y += \
	-I$(backport_srctree)/drivers/net/wireless/broadcom/brcm80211/brcmfmac	\
	-I$(backport_srctree)/drivers/net/wireless/broadcom/brcm80211/include

obj-$(CPTCFG_BRCMFMAC) += brcmfmac.o
brcmfmac-objs += \
		cfg80211.o \
		chip.o \
		fwil.o \
		fweh.o \
		p2p.o \
		proto.o \
		common.o \
		core.o \
		firmware.o \
		feature.o \
		btcoex.o \
		vendor.o \
		pno.o
brcmfmac-$(CPTCFG_BRCMFMAC_PROTO_BCDC) += \
		bcdc.o \
		fwsignal.o
brcmfmac-$(CPTCFG_BRCMFMAC_PROTO_MSGBUF) += \
		commonring.o \
		flowring.o \
		msgbuf.o
brcmfmac-$(CPTCFG_BRCMFMAC_SDIO) += \
		sdio.o \
		bcmsdh.o
brcmfmac-$(CPTCFG_BRCMFMAC_USB) += \
		usb.o
brcmfmac-$(CPTCFG_BRCMFMAC_PCIE) += \
		pcie.o
brcmfmac-$(CPTCFG_BRCMDBG) += \
		debug.o
brcmfmac-$(CPTCFG_BRCM_TRACING) += \
		tracepoint.o
brcmfmac-$(CONFIG_OF) += \
		of.o
brcmfmac-$(CPTCFG_BRCMFMAC_ANDROID) += \
		android.o
