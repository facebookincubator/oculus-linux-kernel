M ?= $(shell pwd)
AUDIO_ROOT=$(shell cd $(KERNEL_SRC); readlink -e $(M))
UAPI_OUT=$(shell cd $(KERNEL_SRC); readlink -e $(M))

KBUILD_OPTIONS += AUDIO_ROOT=$(shell cd $(KERNEL_SRC); readlink -e $(M))
KBUILD_OPTIONS += UAPI_OUT=$(shell cd $(KERNEL_SRC); readlink -e $(M))
KBUILD_OPTIONS += MODNAME=audio
KBUILD_OPTIONS += CONFIG_SND_SOC_WAIPIO=m

obj-m := ipc/
obj-m += dsp/
obj-m += soc/
obj-m += asoc/
obj-m += asoc/codecs/
obj-m += asoc/codecs/lpass-cdc/
obj-m += asoc/codecs/wcd938x/

all: modules
modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
