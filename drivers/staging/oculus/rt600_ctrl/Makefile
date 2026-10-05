obj-$(CONFIG_RT600_CTRL) += rt600-ctrl.o

# Kconfig symbols reach this build as environment variables rather than through
# .config, so gate on a -D rather than IS_ENABLED(), which reads autoconf.h.
ccflags-$(CONFIG_RT600_CTRL_BOARD_ID) += -DRT600_CTRL_BOARD_ID
