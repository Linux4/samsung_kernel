# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y                            :=
obj-m                            :=
obj-n                            :=
obj-                             :=

# Samsung Battery
obj-$(CONFIG_BATTERY_SAMSUNG)    += sec_batt.o

# Samsung TN Features 
obj-$(CONFIG_SEC_EXT)            += sec_log.o
# Samsung TN Sysfs Feature
obj-$(CONFIG_SEC_SYSFS)          += sec_sysfs.o
# Samsung TN Reset Feature
obj-$(CONFIG_SEC_REBOOT)         += sec_reboot.o

# Samsung TN Debugging Features
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug.o sec_getlog.o sec_debug_test.o
obj-$(CONFIG_SEC_DEBUG)			+= sec_debug_hard_reset_hook.o
obj-$(CONFIG_SEC_DEBUG_RESET_REASON)	+= sec_debug_reset_reason.o
obj-$(CONFIG_SEC_DEBUG_EXTRA_INFO)	+= sec_debug_extra_info.o
obj-$(CONFIG_SEC_DEBUG_HW_PARAM)	+= sec_debug_hw_param.o
obj-$(CONFIG_SEC_DEBUG_AUTO_SUMMARY)	+= sec_debug_auto_summary.o
obj-$(CONFIG_SEC_DEBUG_LAST_KMSG)	+= sec_debug_last_kmsg.o
obj-$(CONFIG_SEC_UPLOAD)		+= sec_upload.o

# Samsung TN BSP Features
obj-$(CONFIG_SEC_PARAM)          += sec_param.o
obj-$(CONFIG_SEC_BOOTSTAT)       += sec_bootstat.o
obj-$(CONFIG_SEC_INITCALL_DEBUG) += sec_initcall_debug.o
obj-$(CONFIG_ARGOS)              += sec_argos.o
obj-$(CONFIG_SEC_CP830_ID)       += sec_cp830_id.o

# Samsung sec NAD Feature
obj-$(CONFIG_SEC_NAD)		+= sec_nad.o

# Samsung Misc Feature
obj-$(CONFIG_SEC_MISC)           += sec_misc.o
obj-$(CONFIG_SEC_SLOW_MODE)      += sec_slow.o
