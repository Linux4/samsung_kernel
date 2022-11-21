#
# Copyright (C) 2015 MediaTek Inc.
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

#
# Makefile for misc devices that really don't fit anywhere else.
#

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifdef CONFIG_CAMERA_AAT_V12
ARCH_MTK_PROJECT := aat_v12
else
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
endif

IMGSENSOR_DRIVER_PATH  := $(srctree)/drivers/misc/mediatek/imgsensor/src
COMMON_VERSION := v1

subdir-ccflags-y +=  -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(IMGSENSOR_DRIVER_PATH)/inc \
	-I$(IMGSENSOR_DRIVER_PATH)/common/sysfs
obj-y				+= ../common/$(COMMON_VERSION)/imgsensor.o
obj-y				+= ../common/$(COMMON_VERSION)/imgsensor_hw.o
obj-y				+= ../common/$(COMMON_VERSION)/imgsensor_i2c.o
obj-y				+= ../common/$(COMMON_VERSION)/imgsensor_legacy.o
obj-y				+= ../common/$(COMMON_VERSION)/imgsensor_proc.o
obj-y				+= ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o

ifneq ($(ARCH_MTK_PROJECT), $(subst ",,$(CONFIG_ARCH_MTK_PROJECT)))
ifeq ($(strip $(ARCH_MTK_PROJECT)),)
$(warning ARCH_MTK_PROJECT is not defined)
else
obj-y				+= ../common/sysfs/imgsensor_vendor_rom_config_$(ARCH_MTK_PROJECT).o
obj-y				+= ../common/sysfs/imgsensor_sysfs.o
obj-y				+= ../common/sysfs/imgsensor_vendor.o
obj-y				+= ../common/sysfs/imgsensor_vendor_rom_config.o
obj-y				+= ../common/sysfs/crc32.o
endif
else
$(warning Unable to build sysfs files)
endif

ifdef CONFIG_CAMERA_AAT_V12
obj-y				+= ../common/sysfs/imgsensor_eeprom_rear_gm2_v007.o
obj-y				+= ../common/sysfs/imgsensor_eeprom_rear_2p6_v008.o
obj-y				+= ../common/sysfs/imgsensor_otp_front_sr846d_v008.o
obj-y				+= ../common/sysfs/imgsensor_otp_front_4ha_v008.o
obj-y				+= ../common/sysfs/imgsensor_otp_rear2_gc5035b_v008.o
obj-y				+= ../common/sysfs/imgsensor_otp_rear3_gc02m1b_v008.o
obj-y				+= ../common/sysfs/imgsensor_eeprom_rear4_gc02m1_v008.o
endif

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/$(1)),)
common_drv += ../common/$(COMMON_VERSION)/$(1)/
else
platform_drv += $(1)/
endif
else
project_drv += ./camera_project/$(ARCH_MTK_PROJECT)/$(1)/
endif
endef


$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))

ifneq ($(common_drv),)
$(info imgsensor drv by common $(common_drv))
endif
obj-y				+= $(common_drv)


ifneq ($(platform_drv),)
$(info imgsensor drv by platform $(platform_drv))
endif
obj-y				+= $(platform_drv)
obj-y				+= imgsensor_clk.o

ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw))
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
obj-y				+= camera_hw/
ifneq ($(project_drv),)
obj-y				+= camera_project/$(ARCH_MTK_PROJECT)/
endif
else
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw
obj-y				+= camera_project/$(ARCH_MTK_PROJECT)/
endif

#subdir-ccflags-y +=  -DNO_CLK_METER
