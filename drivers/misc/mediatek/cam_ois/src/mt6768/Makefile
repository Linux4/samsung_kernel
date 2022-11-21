#
# Copyright (C) 2021 Samsung Electronics Co., Ltd.
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

MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
CAM_OIS_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_ois/src
CAM_OIS_PLATFORM_SRC_PATH := $(CAM_OIS_SRC_PATH)/$(MTK_PLATFORM)
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
COMMON_VERSION := v1

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/common/sysfs \
                    -I$(MTK_CUSTOM_PATH)/cam_ois/inc \
                    -I$(CAM_OIS_PLATFORM_SRC_PATH) \
                    -I$(CAM_OIS_SRC_PATH)/common/$(COMMON_VERSION)/inc
subdir-ccflags-y += -I$(srctree)/drivers/i2c/busses/

# Common code
obj-y += ../common/$(COMMON_VERSION)/cam_ois.o
obj-y += ../common/$(COMMON_VERSION)/cam_ois_driver.o

# Platform code

# Project specific cam OIS
ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))
obj-y += camera_project/$(ARCH_MTK_PROJECT)/
subdir-ccflags-y += -I$(CAM_OIS_SRC_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)
else
obj-y += cam_ois_info_default.o
endif

define FILTER_DRV
ifeq ($(wildcard $(CAM_OIS_SRC_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
ifeq ($(wildcard $(CAM_OIS_SRC_PATH)/$(MTK_PLATFORM)/$(1)),)
common_drv += ../common/$(COMMON_VERSION)/$(1)/
else
platform_drv += $(1)/
endif
else
project_drv += ./camera_project/$(ARCH_MTK_PROJECT)/$(1)/
endif
endef

$(foreach drv,$(subst $\",,$(CONFIG_CAMERA_OIS_LIST)),$(eval $(call FILTER_DRV,$(drv))))

ifneq ($(common_drv),)
$(info ois drv by common $(common_drv))
obj-y				+= $(common_drv)
endif

ifneq ($(platform_drv),)
$(info ois drv by platform $(platform_drv))
obj-y				+= $(platform_drv)
endif

ifneq ($(project_drv),)
$(info ois drv by platform $(project_drv))
obj-y				+= $(project_drv)
endif
