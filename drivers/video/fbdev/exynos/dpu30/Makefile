#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_DPP) += dpp.o
dpp-y := dpp_drv.o
obj-$(CONFIG_EXYNOS_MIPI_DSIM) += dsim.o
dsim-y += dsim_drv.o
obj-$(CONFIG_SOC_EXYNOS9830) += ./cal_9830/dsim_reg.o ./cal_9830/dpp_reg.o ./cal_9830/decon_reg.o
obj-$(CONFIG_SOC_EXYNOS9820) += ./cal_9820/dsim_reg.o ./cal_9820/dpp_reg.o ./cal_9820/decon_reg.o
obj-$(CONFIG_SOC_EXYNOS9610) += ./cal_9610/dsim_reg.o ./cal_9610/dpp_reg.o ./cal_9610/decon_reg.o
ifdef CONFIG_EXYNOS_DISPLAYPORT
obj-$(CONFIG_EXYNOS_DISPLAYPORT) += displayport.o
displayport-y += displayport_drv.o displayport_edid.o displayport_hdcp13.o decon_displayport.o displayport_hdcp22_if.o displayport_aux_client.o displayport_topology.o
displayport-y += secdp_unit_test.o
obj-$(CONFIG_SOC_EXYNOS9830) += ./cal_9830/displayport_reg.o
obj-$(CONFIG_SOC_EXYNOS9820) += ./cal_9820/displayport_reg.o
obj-$(CONFIG_SOC_EXYNOS9610) += ./cal_9610/displayport_reg.o
endif
ifdef CONFIG_EXYNOS_DECON_DQE
obj-$(CONFIG_EXYNOS_DECON_DQE) += dqe.o
dqe-y += dqe_drv.o
obj-$(CONFIG_SOC_EXYNOS9830) += ./cal_9830/dqe_reg.o
endif
obj-$(CONFIG_EXYNOS_DPU30) += decon.o
obj-$(CONFIG_EXYNOS_BTS) += bts.o
decon-y += decon_core.o decon_dsi.o decon_wb.o helper.o win_update.o cursor.o fence.o event_log.o format.o
obj-y += panels/
obj-y += mcd_hdr/
obj-$(CONFIG_SUPPORT_MCD_MOTTO_TUNE) += dsim_motto.o

ccflags-$(CONFIG_SAMSUNG_TUI)    += -Idrivers/misc/tui
