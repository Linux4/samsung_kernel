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

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek


subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/mtk/main/inc
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/mtk/main/common/lc898122af
subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/mtk/main/common/lc898212xdaf/inc
ifeq ($(CONFIG_MTK_LENS_BU63165AF_SUPPORT), y)
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/mtk/main/common/bu63165af/inc
else
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/lens/mtk/main/common/bu63169af/inc
endif
ifeq ($(CONFIG_MACH_MT6779), y)
  ccflags-y += -I$(srctree)/drivers/misc/mediatek/timer/timesync/
endif

obj-$(CONFIG_MTK_LENS)    += mainaf.o

mainaf-y  += main_lens.o
mainaf-y  += common/fp5510e2af/FP5510E2AF.o
mainaf-y  += common/fp5529af/FP5529AF.o
mainaf-y  += common/bu6424af/BU6424AF.o
mainaf-y  += common/bu6429af/BU6429AF.o
mainaf-y  += common/bu64748af/bu64748_function.o
mainaf-y  += common/bu64748af/bu64748af.o
mainaf-y  += common/dw9714af/DW9714AF.o
mainaf-y  += common/dw9800waf/DW9800WAF.o
mainaf-y  += common/dw9814af/DW9814AF.o
mainaf-y  += common/dw9718af/DW9718AF.o
mainaf-y  += common/dw9718saf/DW9718SAF.o
mainaf-y  += common/dw9718taf/DW9718TAF.o
mainaf-y  += common/dw9719taf/DW9719TAF.o
mainaf-y  += common/dw9763af/DW9763AF.o
mainaf-y  += common/dw9839af/DW9839AF.o
mainaf-y  += common/gt9764af/GT9764AF.o
mainaf-y  += common/txd_gt9772af/TXD_GT9772AF.o
mainaf-y  += common/st_gt9772af/ST_GT9772AF.o
mainaf-y  += common/lc898212af/LC898212AF.o
mainaf-y  += common/lc898212xdaf/LC898212XDAF.o
mainaf-y  += common/lc898212xdaf/AfInter.o
mainaf-y  += common/lc898212xdaf/AfInit.o
mainaf-y  += common/lc898212xdaf/AfSTMV.o
mainaf-y  += common/lc898214af/LC898214AF.o
mainaf-y  += common/lc898217af/LC898217AF.o
mainaf-y  += common/lc898217afa/LC898217AFA.o
mainaf-y  += common/lc898217afb/LC898217AFB.o
mainaf-y  += common/lc898217afc/LC898217AFC.o
mainaf-y  += common/lc898229af/LC898229AF.o
mainaf-y  += common/lc898122af/LC898122AF.o
mainaf-y  += common/lc898122af/OisCmd.o
mainaf-y  += common/lc898122af/OisIni.o
mainaf-y  += common/wv511aaf/WV511AAF.o
mainaf-y  += common/ak7371af/AK7371AF.o
mainaf-y  += common/bu63169af/BU63169AF.o
mainaf-y  += common/bu63169af/OIS_func.o
mainaf-y  += common/bu63169af/OIS_main.o
mainaf-y  += common/bu63169af/OIS_user.o
mainaf-y  += common/bu64253gwzaf/BU64253GWZAF.o
mainaf-y  += common/truly_s5kjn1_gt9778af/truly_s5kjn1_gt9778af.o
mainaf-y  += common/st_s5kjn1_gt9778af/st_s5kjn1_gt9778af.o
mainaf-y  += common/txd_hi5022q_gt9778af/txd_hi5022q_gt9778af.o
mainaf-y  += common/truly_hi5021q_gt9778af/truly_hi5021q_gt9778af.o
mainaf-y  += common/st_hi5021q_aw8601waf/st_hi5021q_aw8601waf.o
mainaf-y  += common/txd_s5kjn1_gt9778af/txd_s5kjn1_gt9778af.o
mainaf-y  += common/delta_hi5021q_gt9778af/delta_hi5021q_gt9778af.o
mainaf-y  += common/n28trulyhi5021q_aw8601waf/n28trulyhi5021q_aw8601waf.o
mainaf-y  += common/n28hi5021qdc_dw9800waf/n28hi5021qdc_dw9800waf.o
mainaf-y  += common/n28gc50e0gkw_gt9778af/n28gc50e0gkw_gt9778af.o
mainaf-y  += common/n28s5kjn1truly_gt9778af/n28s5kjn1truly_gt9778af.o
mainaf-y  += common/n28s5kjn1dc_gt9778af/n28s5kjn1dc_gt9778af.o