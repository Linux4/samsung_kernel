#
# Copyright (C) 2015 MediaTek Inc.
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

#
# Makefile for misc devices that really don't fit anywhere else.
#

obj-y += lcd_gc7202_ls_hsd_mipi_hdp_video.o
obj-y += lcd_jd9365t_txd_ctc_mipi_hdp_video.o
# HS04 code added for SR-AL6398A-01-634 by duanyaoming at 20220713 start
obj-y += lcd_nl9911c_hlt_hkc_mipi_hdp_video.o
# HS04 code added for SR-AL6398A-01-634 by duanyaoming at 20220713 end
# HS04 code for SR-AL6398A-01-627 by suyurui at 20220719 start
obj-y += lcd_jd9365t_txd_boe_mipi_hdp_video.o
# HS04 code for SR-AL6398A-01-627 by suyurui at 20220719 end
# HS04 code for SR-AL6398A-01-641 by zhawei at 20220722 start
obj-y += lcd_nl9911c_hr_hr_mipi_hdp_video.o
# HS04 code for SR-AL6398A-01-641 by zhawei at 20220722 end
# HS04 code for DEAL6398A-1725 by zhawei at 20220915 start
obj-y += lcd_jd9365t_txd_ctc_jbpd_mipi_hdp_video.o
# HS04 code for DEAL6398A-1725 by zhawei at 20220915 end
obj-y += hs04_hdplus1600_dsi_default_vdo.o
ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc

