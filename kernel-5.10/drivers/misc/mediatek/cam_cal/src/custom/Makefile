#
# Copyright (C) 2018 MediaTek Inc.
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
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_CUSTOM_SRC_PATH := $(CAM_CAL_SRC_PATH)/custom
CAM_CAL_CUSTOM_SRC_LAYOUT_PATH := $(CAM_CAL_CUSTOM_SRC_PATH)/layout
COMMON_VERSION := v2

SAMSUNG_MODEL_NAME  := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/common/sysfs \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_CUSTOM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION)

# For camera ois mcu fw updates
subdir-ccflags-$(CONFIG_CAMERA_OIS) += -I$(MTK_CUSTOM_PATH)/lens/cam_ois/inc

# Common code
obj-$(CONFIG_MTK_CAM_CAL) += camera_eeprom.o
camera_eeprom-objs := ../common/$(COMMON_VERSION)/eeprom_driver.o
camera_eeprom-objs += ../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o
camera_eeprom-objs += ../common/$(COMMON_VERSION)/eeprom_utils.o

# Platform code
camera_eeprom-objs += eeprom_i2c_custom_driver.o

# Select cam_cal_list.c by project
ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
ifneq ($(wildcard $(CAM_CAL_CUSTOM_SRC_PATH)/$(SAMSUNG_MODEL_NAME)),)
MTK_SENSOR_PROJECT := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))
camera_eeprom-objs += $(MTK_SENSOR_PROJECT)/cam_cal_list.o
else
camera_eeprom-objs += cam_cal_list.o
endif
else
camera_eeprom-objs += cam_cal_list.o
endif


comma := ,
empty :=
space := $(empty) $(empty)

KERNEL_CAM_CAL_SRC := $(notdir $(wildcard $(CAM_CAL_CUSTOM_SRC_LAYOUT_PATH)/*.c))
KERNEL_CAM_CAL_OBJ := $(addprefix layout/,$(patsubst %.c, %.o, $(KERNEL_CAM_CAL_SRC)))
KERNEL_CAM_CAL := $(basename $(KERNEL_CAM_CAL_SRC))
CAM_CAL_CONFIG_LIST := $(subst $(space),$(comma),$(foreach e,$(subst ",,$(KERNEL_CAM_CAL)),E($(e))))
subdir-ccflags-y += -D'CAM_CAL_CONFIG_LIST=$(CAM_CAL_CONFIG_LIST)'

camera_eeprom-objs += cam_cal_config.o
camera_eeprom-objs += $(KERNEL_CAM_CAL_OBJ)

$(info camera_eeprom-objs : $(camera_eeprom-objs))
$(info camera_eeprom subdir-ccflags-y : $(subdir-ccflags-y))
