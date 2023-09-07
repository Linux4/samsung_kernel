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

ifeq ($(TARGET_BUILD_VARIANT),$(filter $(TARGET_BUILD_VARIANT),userdebug user))
    ldflags-y += -s
endif

# Force build fail on modpost warning
KBUILD_MODPOST_FAIL_ON_WARNINGS := y

ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
adapter_folder = $(srctree)/$(src)
else
adapter_folder = $(src)
endif

ccflags-y += \
    -I$(KERNEL_DIR)/drivers/misc/mediatek/include/mt-plat \
    -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/include \
    -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/wmt_drv/common_main/linux/include

ifeq ($(CONFIG_MTK_CONN_LTE_IDC_SUPPORT),y)
    ccflags-y += -D WMT_IDC_SUPPORT=1
else
    ccflags-y += -D WMT_IDC_SUPPORT=0
endif

ccflags-y += -D MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT

ccflags-y += -D CREATE_NODE_DYNAMIC=1

ifeq ($(CONFIG_MTK_TC10_FEATURE), y)
    ccflags-y += -DCFG_USE_AOSP_TETHERING_NAME
    ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/connectivity/common
    ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
    ccflags-y += -I$(KERNEL_DIR)/drivers/misc/mediatek/base/power/include
endif

# Wi-Fi character device driver
MODULE_NAME := wmt_chrdev_wifi
ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
$(warning $(MODULE_NAME) build-in boot.img)
obj-y += $(MODULE_NAME).o
else
$(warning $(MODULE_NAME) is kernel module)
obj-m += $(MODULE_NAME).o
endif
