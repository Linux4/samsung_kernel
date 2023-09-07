# drivers/barcelona/gps/Makefile
#
# Makefile for the Barcelona GPS driver.
#
# Copyright (C) 2004,2005 TomTom BV <http://www.tomtom.com/>
# Author: Dimitry Andric <dimitry.andric@tomtom.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

###############################################################################
# Necessary Check

ifneq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
ifeq ($(AUTOCONF_H),)
    $(error AUTOCONF_H is not defined)
endif

ccflags-y += -imacros $(AUTOCONF_H)
else
KERNEL_DIR = $(srctree)
$(warning KERNEL_DIR = $(KERNEL_DIR))
endif

ifeq ($(CONFIG_MTK_COMBO_CHIP),)
    $(error CONFIG_MTK_COMBO_CHIP not defined)
endif

ifeq ($(TARGET_BUILD_VARIANT),$(filter $(TARGET_BUILD_VARIANT),userdebug user))
    ldflags-y += -s
endif

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y
###############################################################################

# only WMT align this design flow, but gps use this also.
ccflags-y += -D MTK_WCN_REMOVE_KERNEL_MODULE

ifeq ($(CONFIG_ARM64), y)
    ccflags-y += -D CONFIG_MTK_WCN_ARM64
endif

ifeq ($(CONFIG_MTK_CONN_LTE_IDC_SUPPORT),y)
    ccflags-y += -D WMT_IDC_SUPPORT=1
else
    ccflags-y += -D WMT_IDC_SUPPORT=0
endif
ccflags-y += -D MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT

ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/emi/submodule
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/emi/$(MTK_PLATFORM)
###############################################################################

MODULE_NAME := gps_drv
ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
$(warning $(MODULE_NAME) build-in boot.img)
obj-y += $(MODULE_NAME).o
else
$(warning $(MODULE_NAME) is kernel module)
obj-m += $(MODULE_NAME).o
endif

ifneq ($(filter "CONSYS_%",$(CONFIG_MTK_COMBO_CHIP)),)
        ccflags-y += -DSOC_CO_CLOCK_FLAG=1
        ccflags-y += -DWMT_CREATE_NODE_DYNAMIC=1
        ccflags-y += -DREMOVE_MK_NODE=0

ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/include
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/linux
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/linux/include
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/core/include
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/platform/include

else
        ccflags-y += -DSOC_CO_CLOCK_FLAG=0
        ccflags-y += -DWMT_CREATE_NODE_DYNAMIC=0
        ccflags-y += -DREMOVE_MK_NODE=1
endif

ifneq ($(filter "CONSYS_6771" "CONSYS_6775",$(CONFIG_MTK_COMBO_CHIP)),)
        ccflags-y += -DEMI_MPU_PROTECTION_IS_READY=1
endif

ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/include
ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/linux/include
ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/core/include
ccflags-y += -I$(WMT_SRC_FOLDER)/common_main/platform/include

$(warning hao test $(CONFIG_MTK_CONN_MT3337_CHIP_SUPPORT))

ifeq ($(CONFIG_MTK_CONN_MT3337_CHIP_SUPPORT),y)
        $(MODULE_NAME)-objs += gps_mt3337.o
else
        $(MODULE_NAME)-objs += stp_chrdev_gps.o
endif
ifneq ($(CONFIG_MTK_GPS_EMI),)
$(MODULE_NAME)-objs += gps_emi.o
endif

# EOF
