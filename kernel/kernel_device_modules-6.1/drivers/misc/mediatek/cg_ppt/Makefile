# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cg_ppt/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/mt-plat/

obj-$(CONFIG_MTK_CG_PEAK_POWER_THROTTLING) += mtk_cg_peak_power_throttling.o

mtk_cg_peak_power_throttling-objs += mtk_cg_peak_power_throttling_core.o
mtk_cg_peak_power_throttling-objs += mtk_cg_peak_power_throttling_table.o
mtk_cg_peak_power_throttling-objs += mtk_cg_peak_power_throttling_def.o
