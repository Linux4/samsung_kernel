# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(src)			# needed for trace events

obj-$(CONFIG_ASHMEM)			+= ashmem.o
obj-$(CONFIG_DEBUG_KINFO)	+= debug_kinfo.o
obj-$(CONFIG_SAMSUNG_LMKD_DEBUG)		+= lmkd_debug.o

ifeq ($(CONFIG_EXYNOS_VARIANT1),y)
include $(srctree)/drivers/staging/android/Makefile.variant1
endif
