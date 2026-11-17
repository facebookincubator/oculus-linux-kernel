# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_HZOS_EXT)	+= main.o flags.o features.o sched.o mem.o bpf.o task_profiles.o \
			   soft_rt.o

CFLAGS_main.o := -I$(src)
# soft_rt.o reaches into struct rq through <kernel/sched/sched.h>.
CFLAGS_soft_rt.o := -I$(srctree)
CFLAGS_task_profiles.o := -I$(src)
