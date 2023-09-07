###############################################################################
# Bluetooth character device driver

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
# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y

ifeq ($(CONFIG_MTK_CONN_LTE_IDC_SUPPORT),y)
    ccflags-y += -D WMT_IDC_SUPPORT=1
else
    ccflags-y += -D WMT_IDC_SUPPORT=0
endif
###############################################################################
# To add WMT dependent Macro and header file, will be removed later

ccflags-y += -D MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include


###############################################################################
# To include BT driver dependent header file

ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/include
ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/linux/include

###############################################################################

MODULE_NAME := bt_drv
ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
$(warning $(MODULE_NAME) is built-in)
obj-y += $(MODULE_NAME).o
else
$(warning $(MODULE_NAME) is kernel module)
obj-m += $(MODULE_NAME).o
endif
ccflags-y += -D CREATE_NODE_DYNAMIC=1

$(MODULE_NAME)-objs += stp_chrdev_bt.o
