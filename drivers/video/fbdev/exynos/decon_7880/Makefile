#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_VPP) += vpp/
obj-$(CONFIG_EXYNOS_MIPI_DSI) += dsim.o
dsim-y += dsim_drv.o dsim_reg_7880.o
obj-$(CONFIG_EXYNOS_DECON_7880) += decon.o
decon-y += decon_core.o decon_f.o decon_dsi.o decon_helper.o decon_board.o decon_notify.o decon_abd.o
obj-$(CONFIG_EXYNOS8890_BTS_OPTIMIZATION) += decon_bts.o
obj-$(CONFIG_SOC_EXYNOS7880) += decon_reg_7880.o
obj-y += panels/
