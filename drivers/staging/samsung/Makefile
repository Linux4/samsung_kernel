# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2016 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

# Samsung Battery
obj-$(CONFIG_BATTERY_SAMSUNG)		+= sec_batt.o
# Samsung Logging Feature
obj-$(CONFIG_SEC_EXT)			+= sec_log.o
# Samsung sec sysfs Feature
obj-$(CONFIG_SEC_SYSFS)			+= sec_sysfs.o
# Samsung Reset Feature
obj-$(CONFIG_SEC_REBOOT)		+= sec_reboot.o
# Samsung Debugging Feature
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug.o sec_getlog.o sec_gaf.o
# Samsung Misc Feature
obj-$(CONFIG_SEC_MISC)			+= sec_misc.o
obj-$(CONFIG_SEC_SLOW_MODE)		+= sec_slow.o
# Samsung Argos
obj-$(CONFIG_ARGOS)			+= sec_argos.o
# Samsung sec bsp Feature
obj-$(CONFIG_SEC_BSP)			+= sec_bsp.o
obj-$(CONFIG_SEC_INITCALL_DEBUG)	+= sec_initcall_debug.o
obj-$(CONFIG_SEC_HOTPLUG)		+= sec_hotplug.o
# Samsung sec NAD Feature
obj-$(CONFIG_SEC_NAD)			+= sec_nad.o
