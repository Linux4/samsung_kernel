# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/$(src)
ccflags-y += -I$(srctree)/drivers/interconnect
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/qos
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
obj-$(CONFIG_INTERCONNECT_MTK_EMI) += mtk-emi.o
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON) += mmqos-common.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs
mmqos-common-objs += mmqos-mtk.o mmqos-hrt.o mmqos-vcp.o mmqos-test.o
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6873.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6873) += mmqos-mt6873.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6853.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6853) += mmqos-mt6853.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6886.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6886) += mmqos-mt6886.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6897.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6897) += mmqos-mt6897.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6983.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6983) += mmqos-mt6983.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6985.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6985) += mmqos-mt6985.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6989.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6989) += mmqos-mt6989.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mmqos-mt6878.c),)
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_MT6878) += mmqos-mt6878.o
endif
