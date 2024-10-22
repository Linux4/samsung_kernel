ifneq ($(CONFIG_MTK_TINYSYS_SSPM_V2),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2
else ifneq ($(CONFIG_MTK_TINYSYS_SCMI),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi/
endif

obj-$(CONFIG_MTK_PMSR) += pmsr.o
obj-$(CONFIG_MTK_PMSR) += twam/
obj-$(CONFIG_MTK_PMSR) += v2/
