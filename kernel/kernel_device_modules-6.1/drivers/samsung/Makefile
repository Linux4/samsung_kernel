# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2016 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_SEC_DEBUG)		+= debug/
obj-$(CONFIG_SEC_MISC)		+= misc/

# Samsung Extention Feature
obj-$(CONFIG_SEC_EXT)		+= sec_ext.o
sec_ext-y += \
	sec_extention.o \
	sec_class.o \
	sec_chipid.o

# Samsung Forced Reset Feature
obj-$(CONFIG_SEC_DEBUG)		+= sec_reset.o
sec_reset-y += \
	sec_key_notifier.o \
	sec_crash_key.o \
	sec_crash_key_user.o \
	sec_hard_reset_hook.o

# Samsung Boot Stat logging
obj-$(CONFIG_SEC_BOOTSTAT)	+= sec_bootstat.o

# Samsung Reboot & Power-off Feature
obj-$(CONFIG_SEC_REBOOT)	+= sec_reboot.o

# Samsung Throughput monitoring Feature
obj-$(CONFIG_ARGOS)		+= sec_argos.o

obj-$(CONFIG_SEC_PM)		+= pm/