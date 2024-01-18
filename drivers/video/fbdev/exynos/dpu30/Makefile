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
obj-$(CONFIG_SOC_EXYNOS9630) += ./cal_9630/dsim_reg.o ./cal_9630/dpp_reg.o ./cal_9630/decon_reg.o
ifdef CONFIG_EXYNOS_DISPLAYPORT
obj-$(CONFIG_EXYNOS_DISPLAYPORT) += displayport.o
displayport-y += displayport_drv.o displayport_edid.o displayport_hdcp13.o decon_displayport.o displayport_hdcp22_if.o displayport_aux_client.o displayport_topology.o
obj-$(CONFIG_SOC_EXYNOS9630) += ./cal_9630/displayport_reg.o
endif
obj-$(CONFIG_EXYNOS_DPU30) += decon.o
obj-$(CONFIG_EXYNOS_BTS) += bts.o
ifdef CONFIG_EXYNOS_DECON_DQE
obj-$(CONFIG_EXYNOS_DECON_DQE) += dqe.o
dqe-y += dqe_drv.o
obj-$(CONFIG_SOC_EXYNOS9630) += ./cal_9630/dqe_reg.o
endif
decon-y += decon_core.o decon_dsi.o decon_wb.o helper.o win_update.o cursor.o fence.o event_log.o format.o
obj-y += panels/
