#
# Copyright (C) 2019 MediaTek Inc.
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

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps
ccflags-y += -I$(srctree)/drivers/iommu

obj-$(CONFIG_MTK_IOMMU_MISC_DBG)    += iommu_debug.o
obj-$(CONFIG_MTK_IOMMU_MISC_SECURE) += iommu_secure.o
obj-$(CONFIG_MTK_IOMMU_MISC_TEST)   += iommu_test.o
obj-$(CONFIG_MTK_IOMMU_MISC_TEST)   += iommu_engine.o
obj-$(CONFIG_MTK_IOMMU_MISC_SECURE) += smmu_secure.o
