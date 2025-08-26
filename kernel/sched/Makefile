# SPDX-License-Identifier: GPL-2.0
ifdef CONFIG_FUNCTION_TRACER
CFLAGS_REMOVE_clock.o = $(CC_FLAGS_FTRACE)
endif

# These files are disabled because they produce non-interesting flaky coverage
# that is not a function of syscall inputs. E.g. involuntary context switches.
KCOV_INSTRUMENT := n

# There are numerous data races here, however, most of them are due to plain accesses.
# This would make it even harder for syzbot to find reproducers, because these
# bugs trigger without specific input. Disable by default, but should re-enable
# eventually.
KCSAN_SANITIZE := n

ifneq ($(CONFIG_SCHED_OMIT_FRAME_POINTER),y)
# According to Alan Modra <alan@linuxcare.com.au>, the -fno-omit-frame-pointer is
# needed for x86 only.  Why this used to be enabled for all architectures is beyond
# me.  I suspect most platforms don't need this, but until we know that for sure
# I turn this off for IA-64 only.  Andreas Schwab says it's also needed on m68k
# to get a correct value for the wait-channel (WCHAN in ps). --davidm
CFLAGS_core.o := $(PROFILING) -fno-omit-frame-pointer
endif

obj-$(CONFIG_SCHED_WALT) += walt/

#
# Build efficiency:
#
# These compilation units have roughly the same size and complexity - so their
# build parallelizes well and finishes roughly at once:
#
obj-y += core.o
obj-y += fair.o
obj-y += build_policy.o
obj-y += build_utility.o
obj-$(CONFIG_ORCHESTRATOR_AGENT) += orchestrator.o
