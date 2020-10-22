# Build FW/driver and uCode revision files.
# Run before FW/driver build.

# Copyright (C) 2020, Broadcom.
#
#      Unless you and Broadcom execute a separate written software license
# agreement governing use of this software, this software is licensed to you
# under the terms of the GNU General Public License version 2 (the "GPL"),
# available at http://www.broadcom.com/licenses/GPLv2.php, with the
# following added to such license:
#
#      As a special exception, the copyright holders of this software give you
# permission to link this software with independent modules, and to copy and
# distribute the resulting executable under terms of your choice, provided that
# you also meet, for each linked independent module, the terms and conditions of
# the license of that module.  An independent module is a module which is not
# derived from this software.  The special exception does not apply to any
# modifications of the software.
#
#
# <<Broadcom-WL-IPTag/Open:>>

EPIVERS_MK := epivers.mk
UC_UREV_MK := ../../components/ucode_dot11_firmware/urev.mk

.PHONY: all
all:

# Prerequisite of this build set in included makefile $(EPIVERS_MK)
EPIVERS :=

include $(EPIVERS_MK)
$(call .epivers_mk_gen,,.)
all: $(EPIVERS)

ifneq ($(and $(if $(filter help,$(MAKECMDGOALS)),,1),$(wildcard \
  $(UC_UREV_MK))),)
  UC_TRACE :=
  include $(UC_UREV_MK)
  all: $(UREV)
endif

# May be unused but preserved from old version for compatibility.
.PHONY: release
release: all

define usage
Usage:
    To build FW/driver and uCode revision files run from
    the top of a deps checkout prior to FW/driver build as

    make -C components/bcmutils/make
endef

# vim: sw=2:noet:
