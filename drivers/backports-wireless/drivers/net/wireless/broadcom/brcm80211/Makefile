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

# common flags
subdir-ccflags-$(CPTCFG_BRCMDBG)	+= -DDEBUG

obj-$(CPTCFG_BRCMUTIL)	+= brcmutil/
obj-$(CPTCFG_BRCMFMAC)	+= brcmfmac/
#obj-$(CPTCFG_BRCMSMAC)	+= brcmsmac/
