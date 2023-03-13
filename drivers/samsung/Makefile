# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2016 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_SEC_DEBUG)		+= debug/
obj-$(CONFIG_SEC_MISC)		+= misc/

# Samsung Reset Feature
obj-$(CONFIG_SEC_REBOOT)		+= sec_reboot.o
# Samsung sec sysfs Feature
obj-$(CONFIG_DRV_SAMSUNG)		+= sec_class.o
obj-$(CONFIG_SEC_PARAM)			+= sec_param.o
obj-$(CONFIG_SEC_PARAM)			+= sec_fmm_lock.o

# Samsung Debugging Feature
obj-$(CONFIG_SEC_KEY_NOTIFIER)	+= sec_key_notifier.o
obj-$(CONFIG_SEC_DEBUG)		+= sec_crash_key.o sec_crash_key_user.o sec_hard_reset_hook.o

# Samsung Throughput monitoring Feature
obj-$(CONFIG_ARGOS)			+= sec_argos.o

