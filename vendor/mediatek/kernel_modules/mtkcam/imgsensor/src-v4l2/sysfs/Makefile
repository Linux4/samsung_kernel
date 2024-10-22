# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Samsung Electronics Inc.

SAMSUNG_MODEL_NAME  := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))

$(info start build of imgsensor_sysfs)

SYSFS_PATH          := $(IMGSENSOR_DRIVER_PATH)/$(SYSFS)

# Build common sysfs files
subdir-ccflags-y += \
	-I$(IMGSENSOR_DRIVER_PATH)/$(SYSFS) \
	-I$(MTK_DRIVER_PATH)/../../leds

SYSFS_OBJS += $(SYSFS)/imgsensor_sysfs.o
SYSFS_OBJS += $(SYSFS)/imgsensor_vendor.o
SYSFS_OBJS += $(SYSFS)/crc32.o
SYSFS_OBJS += $(SYSFS)/imgsensor_vendor_rom_config.o
SYSFS_OBJS += $(SYSFS)/imgsensor_otp_cal.o

# Build project related sysfs files
ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
ifneq ($(wildcard $(SYSFS_PATH)/$(SAMSUNG_MODEL_NAME)),)
$(info camera project of sysfs: $(SAMSUNG_MODEL_NAME))
SYSFS_PROJECT_OBJ := $(notdir $(patsubst %.c, %.o, $(wildcard $(SYSFS_PATH)/$(SAMSUNG_MODEL_NAME)/*.c)))
SYSFS_OBJS += $(addprefix $(SYSFS)/$(SAMSUNG_MODEL_NAME)/, $(SYSFS_PROJECT_OBJ))
else
SYSFS_OBJS += $(SYSFS)/imgsensor_vendor_rom_config_default.o
endif
else
SYSFS_OBJS += $(SYSFS)/imgsensor_vendor_rom_config_default.o
endif

$(info SYSFS_OBJS : $(SYSFS_OBJS))
$(info build of imgsensor_sysfs completed)
