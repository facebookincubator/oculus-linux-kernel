# Build FW/driver revision file.

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

_epivers_mk := $(lastword $(MAKEFILE_LIST))
_bcmutils_dir := $(dir $(_epivers_mk))../
_bcmutils_dir := $(patsubst %/make/../,%/,$(_bcmutils_dir))
_static_dir := $(_bcmutils_dir)include/
_tool_dir := $(_bcmutils_dir)bin/

# Revision file generator.
_epivers_gen := $(_tool_dir)epivers.py

# $(call .epivers_mk_gen[,uniqifier[,workspace,[force,[epivers_path]]]])
# Macro which generates make logic to run epivers.py and create or update
# epivers.h when SCM state changes.
# Parameters (all optional except $2):
# $1 is a string to uniqify the target when invoked multiple times.
# $2 is a required path passed to the generator's --key-comp-dir option.
# $3 is a URL analogue of $2 passed to the --key-comp-url option.
# $4 is the path to epivers.h if WLAN_HEADER is not provided.
# $5 is a set of targets which get an order-only dependency on $(EPIVERS).
define .epivers_mk_gen
$(eval \

  # Revision file path. Can be overridden externally or provided as $4.
  WLAN_HEADER$1 ?= $$(or $4,$$(_static_dir)epivers.h)

  # Evaluate _static_dir again in case WLAN_HEADER$1 is set externally.
  _static_dir := $$(dir $$(WLAN_HEADER$1))

  _key_comp_opt := $$(strip \
    --key-comp-dir=$$(strip $$(or $2,$$(WLAN_TreeBaseR)/src)) \
    $(if $3,--key-comp-url=$$(strip $3)))

  # The EPIVERS variable below allows to invoke revision file build from an
  # enclosing makefile.
  #
  # The idea here is to trick make into always running the generator script
  # without assuming its output file is updated, as a normal target/recipe
  # does, because in fact it updates the output only if SCM state changes.
  #
  # EPIVERS$1 is defined as a phony target which is always "built".
  # The revision file WLAN_HEADER$1 may or may not be updated here;
  # the generator in the recipe builds the file if absent and does not
  # update it unless its contents change. If the file is not updated,
  # its dependents in the enclosing makefiles would not update either.
  # HOWEVER: all this is contingent on epivers.h not being present in
  # the old traditional place $$(_static_dir). In customer source
  # packages an epivers.h must be present and can no longer be derived
  # from our SCM so at packaging time the generated file is placed in
  # $$(_static_dir) and treated as just another static header.
  ifeq ($$(wildcard $(_static_dir)epivers.h),)
    EPIVERS$1 := epivers$1
    .PHONY: $$(EPIVERS$1)
    $$(EPIVERS$1): $$(_epivers_gen) | $$(_static_dir); \
      $Q$$(strip $$< $$(if $$V,-v$$V) \
	$$(_key_comp_opt) $$(WLAN_HEADER$1))
  endif # static epivers.h present

  # Verbosity mechanism, if any, is defined externally.
  $$(call .vbt,$$(EPIVERS$1),4)

  # Some targets may need to wait for epivers.h to be made.
  $5: | $$(EPIVERS$1)

  # Add epivers.h to the cleanup set.
  .PHONY: clean
  clean: epivers_clean$1

  .PHONY: epivers_clean$1
  epivers_clean$1: cleanups := $$(wildcard $$(WLAN_HEADER$1))
  epivers_clean$1:
	$$(if $$(cleanups),$$(RM) $$(WLAN_HEADER$1))

)
endef # .epivers_mk_gen

# vim: filetype=make shiftwidth=2
