#
# Copyright (C) 2023 MediaTek Inc.
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
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/v2/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/ged/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpueb/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/

ccflags-y += \
	-Wno-unused-variable \
	-Wno-unused-function

ifneq ($(wildcard $(srctree)/$(src)/gpu_iommu_mt6989.c),)
mtk_gpu_iommu_mt6989-y := gpu_iommu_mt6989.o
obj-$(CONFIG_MTK_GPU_MT6989_SUPPORT) += mtk_gpu_iommu_mt6989.o
endif