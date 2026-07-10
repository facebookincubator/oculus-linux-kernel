# SPDX-License-Identifier: GPL-2.0-only
include ../../scripts/Makefile.include
include ../../scripts/Makefile.arch

srctree := $(abspath $(CURDIR)/../../../)
roottree := $(abspath $(CURDIR)/../../../../../../)
PREBUILT_HOST_KERNEL_BUILD_TOOLS_ROOT ?= $(roottree)/prebuilts/kernel-build-tools/linux-x86
HOST_SYSROOT ?= $(roottree)/prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.17-4.8/sysroot
override HOSTCFLAGS += -I$(PREBUILT_HOST_KERNEL_BUILD_TOOLS_ROOT)/include
override HOSTCFLAGS += --sysroot=$(HOST_SYSROOT)

ifeq ($(V),1)
  Q =
  msg =
else
  Q = @
  ifeq ($(silent),1)
    msg =
  else
    msg = @printf '  %-8s %s%s\n' "$(1)" "$(notdir $(2))" "$(if $(3), $(3))";
  endif
  MAKEFLAGS=--no-print-directory
endif

# Overrides for the prepare step libraries.
HOST_OVERRIDES := AR="$(HOSTAR)" CC="$(HOSTCC)" LD="$(HOSTLD)" ARCH="$(HOSTARCH)" \
		  CROSS_COMPILE="" EXTRA_CFLAGS="$(HOSTCFLAGS)"

RM      ?= rm
HOSTCC  ?= gcc
HOSTLD  ?= ld
HOSTAR  ?= ar
CROSS_COMPILE =

OUTPUT ?= $(srctree)/tools/bpf/resolve_btfids/

LIBBPF_SRC := $(srctree)/tools/lib/bpf/
SUBCMD_SRC := $(srctree)/tools/lib/subcmd/

BPFOBJ     := $(OUTPUT)/libbpf/libbpf.a
LIBBPF_OUT := $(abspath $(dir $(BPFOBJ)))/
SUBCMDOBJ  := $(OUTPUT)/libsubcmd/libsubcmd.a
SUBCMD_OUT := $(abspath $(dir $(SUBCMDOBJ)))/

LIBBPF_DESTDIR := $(LIBBPF_OUT)
LIBBPF_INCLUDE := $(LIBBPF_DESTDIR)include

SUBCMD_DESTDIR := $(SUBCMD_OUT)
SUBCMD_INCLUDE := $(SUBCMD_DESTDIR)include

BINARY     := $(OUTPUT)/resolve_btfids
BINARY_IN  := $(BINARY)-in.o

all: $(BINARY)

prepare: $(BPFOBJ) $(SUBCMDOBJ)

$(OUTPUT) $(OUTPUT)/libsubcmd $(LIBBPF_OUT):
	$(call msg,MKDIR,,$@)
	$(Q)mkdir -p $(@)

$(SUBCMDOBJ): fixdep FORCE | $(OUTPUT)/libsubcmd
	$(Q)$(MAKE) -C $(SUBCMD_SRC) OUTPUT=$(SUBCMD_OUT) \
		    DESTDIR=$(SUBCMD_DESTDIR) $(HOST_OVERRIDES) prefix= subdir= \
		    $(abspath $@) install_headers

$(BPFOBJ): $(wildcard $(LIBBPF_SRC)/*.[ch] $(LIBBPF_SRC)/Makefile) | $(LIBBPF_OUT)
	$(Q)$(MAKE) $(submake_extras) -C $(LIBBPF_SRC) OUTPUT=$(LIBBPF_OUT)    \
		    DESTDIR=$(LIBBPF_DESTDIR) $(HOST_OVERRIDES) prefix= subdir= \
		    $(abspath $@) install_headers

LIBELF_FLAGS := $(shell $(HOSTPKG_CONFIG) libelf --cflags 2>/dev/null)
LIBELF_LIBS  := $(shell $(HOSTPKG_CONFIG) libelf --libs 2>/dev/null || echo -lelf)

ifndef LIBELF_FLAGS
  # if the libefl is not avaiable on the host machine, use the prebuilts version instead
  LIBELF_FLAGS := -I$(PREBUILT_HOST_KERNEL_BUILD_TOOLS_ROOT)/include
  override KBUILD_HOSTLDFLAGS += -Wl,-rpath,$(PREBUILT_HOST_KERNEL_BUILD_TOOLS_ROOT)/lib64
  override KBUILD_HOSTLDFLAGS += -L$(PREBUILT_HOST_KERNEL_BUILD_TOOLS_ROOT)/lib64
  # libz is a dependency of libdelf, but in the prebuilts package, it is named libz-host.
  LIBS = $(LIBELF_LIBS) -lz-host
endif

HOSTCFLAGS_resolve_btfids += -g \
          -I$(srctree)/tools/include \
          -I$(srctree)/tools/include/uapi \
          -I$(LIBBPF_INCLUDE) \
          -I$(SUBCMD_INCLUDE) \
          $(LIBELF_FLAGS)

LIBS ?= $(LIBELF_LIBS) -lz

export srctree OUTPUT HOSTCFLAGS_resolve_btfids Q HOSTCC HOSTLD HOSTAR
include $(srctree)/tools/build/Makefile.include

$(BINARY_IN): fixdep FORCE prepare | $(OUTPUT)
	$(Q)$(MAKE) $(build)=resolve_btfids

$(BINARY): $(BPFOBJ) $(SUBCMDOBJ) $(BINARY_IN)
	$(call msg,LINK,$@)
	$(Q)$(HOSTCC) $(BINARY_IN) $(KBUILD_HOSTLDFLAGS) -o $@ $(BPFOBJ) $(SUBCMDOBJ) $(LIBS)

clean_objects := $(wildcard $(OUTPUT)/*.o                \
                            $(OUTPUT)/.*.o.cmd           \
                            $(OUTPUT)/.*.o.d             \
                            $(LIBBPF_OUT)                \
                            $(LIBBPF_DESTDIR)            \
                            $(SUBCMD_OUT)                \
                            $(SUBCMD_DESTDIR)            \
                            $(OUTPUT)/resolve_btfids)

ifneq ($(clean_objects),)
clean: fixdep-clean
	$(call msg,CLEAN,$(BINARY))
	$(Q)$(RM) -rf $(clean_objects)
else
clean:
endif

tags:
	$(call msg,GEN,,tags)
	$(Q)ctags -R . $(LIBBPF_SRC) $(SUBCMD_SRC)

FORCE:

.PHONY: all FORCE clean tags prepare
