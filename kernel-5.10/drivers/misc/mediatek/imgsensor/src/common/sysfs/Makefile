# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Samsung Electronics Inc.

SAMSUNG_MODEL_NAME  := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))

$(info start build of imgsensor_sysfs)

SYSFS_PATH          := $(IMGSENSOR_COMMON_PATH)/sysfs
SYSFS_FULL_PATH     := $(srctree)/drivers/misc/mediatek/imgsensor/src/common/sysfs

# Build common sysfs files
SYSFS_OBJS += $(SYSFS_PATH)/imgsensor_sysfs.o
SYSFS_OBJS += $(SYSFS_PATH)/imgsensor_vendor.o
SYSFS_OBJS += $(SYSFS_PATH)/crc32.o
SYSFS_OBJS += $(SYSFS_PATH)/imgsensor_vendor_rom_config.o
SYSFS_OBJS += $(SYSFS_PATH)/imgsensor_otp_cal.o

# Build project related sysfs files
ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
ifneq ($(wildcard $(SYSFS_FULL_PATH)/$(SAMSUNG_MODEL_NAME)),)
$(info camera project of sysfs: $(SAMSUNG_MODEL_NAME))
SYSFS_PROJECT_OBJ := $(notdir $(patsubst %.c, %.o, $(wildcard $(SYSFS_FULL_PATH)/$(SAMSUNG_MODEL_NAME)/*.c)))
SYSFS_OBJS += $(addprefix $(SYSFS_PATH)/$(SAMSUNG_MODEL_NAME)/, $(SYSFS_PROJECT_OBJ))
else
SYSFS_OBJS += $(SYSFS_PATH)/imgsensor_vendor_rom_config_default.o
endif
else
SYSFS_OBJS += $(SYSFS_PATH)/imgsensor_vendor_rom_config_default.o
endif

$(info SYSFS_OBJS : $(SYSFS_OBJS))
$(info build of imgsensor_sysfs completed)
