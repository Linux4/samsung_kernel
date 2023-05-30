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
# TabA7 Lite  code for SR-AX3565-01-320 by liuchengfei at 20201127 start
mainaf-y  += common/gt9772afhlt/GT9772AFHLT.o
# TabA7 Lite  code for SR-AX3565-01-320 by liuchengfei at 20201127 end
#gaozhenyu add for samsung af start
mainaf-y  += common/gt9769af/GT9769AF.o
#gaozhenyu add for samsung af end
# TabA7 Lite  code for SR-AX3565-01-320 by lisizhou at 20201129 start
mainaf-y  += common/va26x802af/VA26X802AF.o
# TabA7 Lite  code for SR-AX3565-01-320 by lisizhou at 20201129 end
#TabA7 Lite code for SR-AX3565-01-875 by gaozhenyu at 2021/11/19 start
mainaf-y  += common/gt9769saf/GT9769sAF.o
#TabA7 Lite code for SR-AX3565-01-875 by gaozhenyu at 2021/11/19 end
#TabA7 Lite code for SR-AX3565-01-906  by chenjun at 2022/02/19 start
mainaf-y  += common/fp5510main6af/FP5510MAIN6AF.o
#TabA7 Lite code for SR-AX3565-01-906  by chenjun at 2022/02/19 end
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
# A03s code for CAM-AL5625-01-247 by lisizhou at 2021/04/22 start
mainaf-y  += common/gt9772af/GT9772AF.o
# A03s code for CAM-AL5625-01-247 by lisizhou at 2021/04/22 end
#hs03s_NM code for SL6215DEV-4183 by liluling at 2022/4/15 start
mainaf-y  += common/gt9773af/GT9773AF.o
mainaf-y  += common/fp5519af/FP5519AF.o
#A03s code for CAM-AL5625-01-247 by xuxianwei at 2021/05/18 start
#hs03s_NM code for SL6215DEV-4183 by liluling at 2022/4/15 start
mainaf-y  += common/cn3927ddaf/CN3927DDAF.o
mainaf-y  += common/cn3927ofilmaf/CN3927OFILMAF.o
mainaf-y  += common/cn3927xlaf/CN3927XLAF.o
#A03s code for CAM-AL5625-01-247 by xuxianwei at 2021/05/18 end

#hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/17 start
##################### M04 AF CODE START #########################
ifeq ($(CONFIG_HQ_PROJECT_HS04), y)
mainaf-y  += common/o2101_gt9772af/O2101_GT9772AF.o
mainaf-y  += common/o2102_gt9769af/O2102_GT9769AF.o
mainaf-y  += common/o2103_pe916af/O2103_PE916AF.o
mainaf-y  += common/o2104_gt9769af/O2104_GT9769AF.o
endif
##################### M04 AF CODE END #########################
#hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/17 end
#hs14 code for AL6528ADEU-2675 by pengxutao at 2022/11/18 start
ifeq ($(CONFIG_HQ_PROJECT_O22), y)
##################### A14 AF CODE START #########################
mainaf-y  += common/a1401aw8601waf/A1401AW8601WAF.o
mainaf-y  += common/a1402dw9767af/A1402DW9767AF.o
mainaf-y  += common/a1403gt9778af/A1403GT9778AF.o
mainaf-y  += common/a1404aw8601waf/A1404AW8601WAF.o
##################### A14 AF CODE START #########################
endif
#hs14 code for AL6528ADEU-2675 by pengxutao at 2022/11/18 end
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

