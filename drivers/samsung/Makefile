# drivers/samsung/Makefile
#
# Copyright (c) 2010-2019 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

obj-$(CONFIG_SEC_MISC)          += misc/
obj-$(CONFIG_SEC_PM)            += pm/
obj-$(CONFIG_SEC_PERF)          += perf/

# Samsung Reset Feature
obj-$(CONFIG_SEC_REBOOT)        += sec_reboot.o

# Samsung sec sysfs Feature
obj-$(CONFIG_DRV_SAMSUNG)       += sec_class.o
obj-$(CONFIG_SEC_PARAM)         += sec_param.o
obj-$(CONFIG_SEC_PARAM)         += sec_fmm_lock.o
obj-$(CONFIG_SEC_COMMON)        += sec_common.o

obj-$(CONFIG_SEC_DEBUG)         += sec_crash_key.o
obj-$(CONFIG_SEC_DEBUG)         += sec_crash_key_user.o
obj-$(CONFIG_SEC_DEBUG)         += sec_hard_reset_hook.o

# Samsung Throughput monitoring Feature
obj-$(CONFIG_ARGOS)             += sec_argos.o

# Samsung System Update
obj-$(CONFIG_SEC_SYSUP)         += sec_sysup.o
