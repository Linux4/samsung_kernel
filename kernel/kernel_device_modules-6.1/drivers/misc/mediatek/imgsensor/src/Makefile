# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/i2c/busses/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ccu/inc

MTK_DRIVER_PATH  := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek

MTK_PLATFORM := isp6s

subdir-ccflags-y += \
             -I$(MTK_DRIVER_PATH)/imgsensor/inc \
             -I$(MTK_DRIVER_PATH)/cam_cal/inc

CUSTOM_KERNEL_FLAGS := $(addprefix -D, $(shell echo \
             $(CONFIG_CUSTOM_KERNEL_IMGSENSOR) | tr a-z A-Z))
subdir-ccflags-y += $(CUSTOM_KERNEL_FLAGS)

obj-$(CONFIG_MTK_IMGSENSOR) += \
                        $(MTK_PLATFORM)/