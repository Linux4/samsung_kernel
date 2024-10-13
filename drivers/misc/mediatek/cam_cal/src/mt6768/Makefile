# SPDX-License-Identifier: GPL-2.0

MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_PLATFORM_SRC_PATH := $(CAM_CAL_SRC_PATH)/$(MTK_PLATFORM)
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
COMMON_VERSION := v1

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(CONFIG_MTK_PLATFORM)/camera_hw \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_PLATFORM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION)
subdir-ccflags-y += -I$(srctree)/drivers/i2c/busses/

# Common code
obj-y += ../common/$(COMMON_VERSION)/eeprom_driver.o
obj-y += ../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o

# Platform code
obj-y += eeprom_i2c_custom_driver.o

# hs14 code for SR-AL6528-01-161 Universal macro adaptation by renxinglin at 2022/9/27 start
#a06 code for SR-AL7160A-01-499 by pengxutao at 20240415  start
# A06 code for SR-AL7160A-01-502 by xuyunhui at 20240605 start
# A06 code for SR-AL7160A-01-502 by xuyunhui at 20240619 start
# A06 code for SR-AL7160A-01-502 by jiangwenhan at 20240626 start
# Otp code
ifeq ($(CONFIG_HQ_PROJECT_O22), y)
obj-y += eeprom_i2c_a1401widehi556wly_driver.o
obj-y += eeprom_i2c_a1402widesc520cstxd_driver.o
else ifeq ($(CONFIG_HQ_PROJECT_O8), y)
obj-y += eeprom_i2c_a0601ddfrontsc820cs_driver.o
obj-y += eeprom_i2c_a0602cxtfrontgc08a8_driver.o
obj-y += eeprom_i2c_a0603ddfrontmt811_driver.o
obj-y += eeprom_i2c_a0604xlfrontmt811_driver.o
obj-y += eeprom_i2c_a0605ddfrontsc820cs_driver.o
obj-y += eeprom_i2c_a0606ddfrontmt811_driver.o
obj-y += eeprom_i2c_a0607xlfrontmt811_driver.o
endif
# hs14 code for SR-AL6528-01-161 Universal macro adaptation by renxinglin at 2022/9/27 end
#a06 code for SR-AL7160A-01-499 by pengxutao at 20240415  end
# A06 code for SR-AL7160A-01-502 by xuyunhui at 20240605 end 
# A06 code for SR-AL7160A-01-502 by xuyunhui at 20240619 end
# A06 code for SR-AL7160A-01-502 by jiangwenhan at 20240626 start
define INC_CUSTOM_SRC
ifeq ($(wildcard $(CAM_CAL_PLATFORM_SRC_PATH)/camera_project/$(ARCH_MTK_PROJECT)/$(1).c),)
obj-y += $(1).o
else
obj-y += camera_project/$(ARCH_MTK_PROJECT)/$(1).o
endif
endef

# Project or platform code
CUSTOM_SRC_LIST := cam_cal_list eeprom_i2c_dev
$(foreach src,$(CUSTOM_SRC_LIST),$(eval $(call INC_CUSTOM_SRC,$(src))))
