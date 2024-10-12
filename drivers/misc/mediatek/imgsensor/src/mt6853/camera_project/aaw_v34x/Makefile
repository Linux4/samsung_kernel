# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Samsung Electronics Inc.

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))
else
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
endif

IMGSENSOR_DRIVER_PATH  := $(srctree)/drivers/misc/mediatek/imgsensor/src

define FILTER_PROJECT_DRV
ifneq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
project_drv += $(1)/
endif
endef



ifneq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw),)
obj-y				+= camera_hw/
endif

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_PROJECT_DRV,$(drv))))

obj-y += $(project_drv)

ifneq ($(project_drv),)
$(info imgsensor drv by project $(project_drv))
endif
