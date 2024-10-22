# SPDX-License-Identifier: GPL-2.0

ccflags-y += \
	-I$(srctree)/include/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/

obj-$(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE) += cpudvfs.o
cpudvfs-y += cpufreq-dbg-lite.o
#cpudvfs-y += devinfo.o
