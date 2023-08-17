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
obj-y += eeprom_i2c_hi846_driver.o
obj-y += eeprom_i2c_gc8054_driver.o
obj-y += eeprom_i2c_gc8054_cxt_driver.o
obj-y += eeprom_i2c_ov02b10_jk_driver.o
obj-y += eeprom_i2c_gc02m1_sjc_driver.o
obj-y += eeprom_i2c_gc02m1sub_cxt_driver.o
# TabA7 Lite code for SR-AX3565-01-907 by chenjun at 2022/02/21 start
obj-y += eeprom_i2c_s5k4h7_hlt_driver.o
# TabA7 Lite code for SR-AX3565-01-907 by chenjun at 2022/02/21 end
#A03s code for SR-AL5625-01-332 by xuxianwei at 2021/05/02 start
obj-y += eeprom_i2c_hi556_txd_driver.o
obj-y += eeprom_i2c_gc5035_ly_driver.o
obj-y += eeprom_i2c_hi556_ofilm_driver.o
obj-y += eeprom_i2c_gc5035_dd_driver.o
#A03s code for SR-AL5625-01-332 by xuxianwei at 2021/05/02 end
#A03s code for CAM-AL5625-01-247 by majunfeng at 2021/05/25 start
obj-y += eeprom_i2c_gc5035_xl_driver.o
#A03s code for CAM-AL5625-01-247 by majunfeng at 2021/05/25 end
obj-y += eeprom_i2c_gc02m1_cxt_driver.o
obj-y += eeprom_i2c_ov02b10_ly_driver.o
#/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/06/28 start */
obj-y += eeprom_i2c_gc02m1_jk_driver.o
#/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/06/28 end */
#hs03s_NM code for SL6215DEV-4183 by liluling at 2022/4/15 start
obj-y += eeprom_i2c_sc500cs_dd_driver.o
#hs03s_NM code for SL6215DEV-4183 by liluling at 2022/4/15 start

#hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/17 start
######################## HS04 OTP START ################################
ifeq ($(CONFIG_HQ_PROJECT_HS04), y)
obj-y += eeprom_i2c_o2101_hi556txd_front_driver.o
obj-y += eeprom_i2c_o2103_sc520syx_front_driver.o
obj-y += eeprom_i2c_o2104_hi556wtxd_front_driver.o
endif
######################## HS04 OTP END ##################################
#hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/17 end

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
