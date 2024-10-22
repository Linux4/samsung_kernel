#
# Copyright (C) 2022 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# Connectivity combo driver
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.

BUILD_UT := n

ifneq ($(KERNELRELEASE),)
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/
    subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/pinctrl/mediatek/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/uarthub/common
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2

    # Do Nothing, move to standalone repo
    MODULE_NAME := uarthub_drv
    obj-$(CONFIG_MTK_UARTHUB) += $(MODULE_NAME).o

    $(MODULE_NAME)-objs += common/uarthub_drv_core.o
    $(MODULE_NAME)-objs += common/uarthub_drv_debug.o
    $(MODULE_NAME)-objs += common/uarthub_drv_ut_test.o
    $(MODULE_NAME)-objs += common/uarthub_drv_export.o

PATH_TO_UARTHUB_DRV := $(srctree)/$(src)
ifneq ($(wildcard $(PATH_TO_UARTHUB_DRV)/common/platform/6985/mt6985.c),)
    $(MODULE_NAME)-objs += common/platform/6985/mt6985.o
endif

ifneq ($(wildcard $(PATH_TO_UARTHUB_DRV)/common/platform/6989/mt6989.c),)
    $(MODULE_NAME)-objs += common/platform/6989/mt6989.o
    $(MODULE_NAME)-objs += common/platform/6989/mt6989_debug.o
ifneq ($(BUILD_UT), n)
    $(MODULE_NAME)-objs += common/platform/6989/ut/mt6989_ut_test.o
    $(MODULE_NAME)-objs += common/platform/6989/ut/mt6989_ut_test_for_ip.o
    $(MODULE_NAME)-objs += common/platform/6989/ut/mt6989_ut_dat.o
    $(MODULE_NAME)-objs += common/platform/6989/ut/ut_util.o
endif
endif

# Otherwise we were called directly from the command line;
# invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
