# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Samsung Electronics Inc.

SAMSUNG_MODEL_NAME  := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))

SYSFS_PATH          := $(srctree)/drivers/misc/mediatek/imgsensor/src/common/sysfs

# Build common sysfs files
obj-y				+= imgsensor_sysfs.o
obj-y				+= imgsensor_vendor.o
obj-y				+= crc32.o
obj-y				+= imgsensor_vendor_rom_config.o
obj-y				+= imgsensor_otp_cal.o

# Build project related sysfs files
ifdef CONFIG_CAMERA_SAMSUNG_MODEL_NAME
ifneq ($(wildcard $(SYSFS_PATH)/$(SAMSUNG_MODEL_NAME)),)
$(info camera project of sysfs: $(SAMSUNG_MODEL_NAME))
SYSFS_PROJECT_OBJ := $(notdir $(patsubst %.c, %.o, $(wildcard $(SYSFS_PATH)/$(SAMSUNG_MODEL_NAME)/*.c)))
obj-y             += $(addprefix $(SAMSUNG_MODEL_NAME)/, $(SYSFS_PROJECT_OBJ))
else
obj-y             += imgsensor_vendor_rom_config_default.o
endif
else
obj-y             += imgsensor_vendor_rom_config_default.o
endif
