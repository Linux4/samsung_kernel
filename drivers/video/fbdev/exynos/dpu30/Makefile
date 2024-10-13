#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_DPU30) += dpu.o
dpu-y += decon_core.o decon_dsi.o decon_wb.o helper.o win_update.o cursor.o fence.o event_log.o format.o
dpu-y += dpp_drv.o dsim_drv.o
dpu-$(CONFIG_SOC_S5E3830) += ./cal_3830/dsim_reg.o ./cal_3830/dpp_reg.o ./cal_3830/decon_reg.o

dpu-$(CONFIG_EXYNOS_BTS) += bts.o
dpu-$(CONFIG_EXYNOS_COMMON_PANEL) += ./panels/common_panel_drv.o\
	./panels/common_mipi_lcd.o
