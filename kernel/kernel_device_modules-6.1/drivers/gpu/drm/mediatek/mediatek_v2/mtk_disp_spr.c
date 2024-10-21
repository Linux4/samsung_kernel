// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_spr.h"


#define DISP_REG_SPR_STA			0x0000

#define DISP_REG_SPR_INTEN			0x0004
	#define IF_END_INT_EN	BIT(0)
	#define OF_END_INT_EN	BIT(1)
#define DISP_REG_SPR_INTSTA			0x0008
	#define IF_END_INT	BIT(0)
	#define OF_END_INT	BIT(1)

#define DISP_REG_SPR_EN			0x000C
	#define SPR_EN BIT(0)
	#define SPR_LUT_EN BIT(1)
	#define SPR_WRAP_MODE BIT(2)
	#define SPR_RELAY_MODE BIT(4)
	#define DISP_SPR_RELAY_MODE BIT(5)
	#define IGNORE_ABNORMAL_SOF		BIT(16)
	#define READ_WRK_REG BIT(20)
	#define SPR_FORCE_COMMIT		BIT(21)
	#define SPR_BYPASS_SHADOW		BIT(22)
	#define CON_FLD_SPR_EN		REG_FLD_MSB_LSB(0, 0)
	#define CON_FLD_DISP_SPR_RELAY_MODE		REG_FLD_MSB_LSB(4, 4)

#define DISP_REG_SPR_CFG			0x0010
	#define SPR_STALL_CG_ON BIT(0)
	#define INDATA_RES_SEL BIT(1)
	#define OUTDATA_RES_SEL REG_FLD_MSB_LSB(3, 2)
	#define SPR_RGB_SWAP BIT(4)
	#define RG_BYPASS_DITHER BIT(5)
	#define POSTALIGN_EN		BIT(16)
	#define POSTALIGN_SEL REG_FLD_MSB_LSB(25, 20)
	#define PADDING_REPEAT_EN		BIT(28)
	#define POSTALIGN_6TYPE_MODE		BIT(31)

#define DISP_REG_SPR_RST			0x0014
	#define SPR_RESET BIT(0)
	#define SPR_TOP_RST BIT(4)
	#define DISP_SPR_DBG_SEL REG_FLD_MSB_LSB(19, 16)

#define DISP_REG_SPR_STATUS			0x0018
	#define IF_UNFINISH BIT(0)
	#define OF_UNFINISH BIT(1)
	#define FRAME_DONE BIT(4)
	#define WRAP_STATE REG_FLD_MSB_LSB(11, 8)
	#define HANDSHAKE REG_FLD_MSB_LSB(31, 16)

#define DISP_REG_SPR_CHK_SUM0			0x001C
	#define CHECKSUM_EN BIT(0)
	#define CHECKSUM_SEL REG_FLD_MSB_LSB(2, 1)

#define DISP_REG_SPR_CHK_SUM1		0x0020
	#define CHECKSUM_LSB	REG_FLD_MSB_LSB(31, 0)

#define DISP_REG_SPR_CHK_SUM2		0x0024
	#define CHECKSUM_MSB	REG_FLD_MSB_LSB(31, 0)
#define DISP_REG_SPR_ROI_SIZE		0x002C
	#define REG_RESO_MAX_X REG_FLD_MSB_LSB(12, 0)
	#define REG_RESO_MAX_Y REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_SPR_FRAME_DONE_DEL		0x0030
	#define DISP_FRAME_DONE_DEL REG_FLD_MSB_LSB(7, 0)

#define DISP_REG_SPR_SW_SCRATCH			0x0038
	#define DISP_SPR_SW_SCRATCH REG_FLD_MSB_LSB(31, 0)
#define DISP_REG_SPR_RDY_SEL			0x003C
	#define DISP_SPR_RDY_SEL BIT(0)
#define DISP_REG_SPR_RDY_SEL_EN			0x0040
	#define DISP_SPR_RDY_SEL_EN BIT(0)

#define DISP_REG_SPR_OPTION		0x004C
	#define DISP_SPR_AUTO_CLOCK BIT(4)

#define DISP_REG_SPR_CK_ON		0x0044
	#define DISP_SPR_CK_ON BIT(0)
	#define CROP_HOFFSET            REG_FLD_MSB_LSB(23, 16)
	#define CROP_VOFFSET            REG_FLD_MSB_LSB(31, 24)

#define DISP_REG_SPR_DBG0			0x0050
	#define INP_PIX_CNT REG_FLD_MSB_LSB(12, 0)
	#define INP_LINE_CNT REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_SPR_DBG1			0x0054
	#define OUTP_PIX_CNT REG_FLD_MSB_LSB(12, 0)
	#define OUTP_LINE_CNT REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_SPR_DBG2			0x0058
	#define HCNT_IN REG_FLD_MSB_LSB(13, 0)
	#define VCNT_IN REG_FLD_MSB_LSB(29, 16)

#define DISP_REG_SPR_DBG3			0x005C
	#define HCNT_OUT REG_FLD_MSB_LSB(13, 0)
	#define VCNT_OUT REG_FLD_MSB_LSB(29, 16)

#define DISP_REG_SPR_DBG4			0x0060
	#define PIX_CNT_POST REG_FLD_MSB_LSB(12, 0)
	#define LINE_CNT_POST REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_SPR_DBG5			0x0064
	#define ROUT REG_FLD_MSB_LSB(11, 0)

#define DISP_REG_SPR_DBG6			0x0068
	#define GOUT REG_FLD_MSB_LSB(11, 0)

#define DISP_REG_SPR_DBG7			0x006C
	#define BOUT REG_FLD_MSB_LSB(11, 0)

#define DISP_REG_SPR_MANUAL_RST			0x0070
	#define RG_SPR_VSYNC_LOW BIT(0)
	#define RG_SPR_VSYNC_PERIOD REG_FLD_MSB_LSB(9, 4)

#define DISP_REG_SPR_DUMMY			0x0080
	#define RG_SPR_DUMMY REG_FLD_MSB_LSB(31, 0)

#define DISP_REG_SPR_CTRL			0x0100
	#define REG_SYNC_SS BIT(0)
	#define REG_SYNC_GS BIT(4)
	#define SPR_POWERSAVING BIT(16)
	#define SPR_SPECIALCASEEN BIT(28)

#define DISP_REG_SPR_ARRANGE0			0x0104
	#define SPR_TH_H REG_FLD_MSB_LSB(5, 0)
	#define SPR_TH_L REG_FLD_MSB_LSB(21, 16)

#define DISP_REG_SPR_ARRANGE1			0x0108
	#define SPR_PIXELGROUP REG_FLD_MSB_LSB(1, 0)
	#define SPR_ARRANGE_UL_P0 REG_FLD_MSB_LSB(6, 4)
	#define SPR_ARRANGE_UL_P1 REG_FLD_MSB_LSB(10, 8)
	#define SPR_ARRANGE_UL_P2 REG_FLD_MSB_LSB(14, 12)
	#define SPR_ARRANGE_DL_P0 REG_FLD_MSB_LSB(18, 16)
	#define SPR_ARRANGE_DL_P1 REG_FLD_MSB_LSB(22, 20)
	#define SPR_ARRANGE_DL_P2 REG_FLD_MSB_LSB(26, 24)

#define DISP_REG_SPR_ARRANGE2			0x010C
	#define SPR_VALIDDOTS_UL_P0 REG_FLD_MSB_LSB(1, 0)
	#define SPR_VALIDDOTS_UL_P1 REG_FLD_MSB_LSB(5, 4)
	#define SPR_VALIDDOTS_UL_P2 REG_FLD_MSB_LSB(9, 8)
	#define SPR_VALIDDOTS_DL_P0 REG_FLD_MSB_LSB(13, 12)
	#define SPR_VALIDDOTS_DL_P1 REG_FLD_MSB_LSB(17, 16)
	#define SPR_VALIDDOTS_DL_P2 REG_FLD_MSB_LSB(21, 20)
	#define SPR_VALIDDOTS_INC_UL REG_FLD_MSB_LSB(25, 24)
	#define SPR_VALIDDOTS_INC_DL REG_FLD_MSB_LSB(29, 28)
#define DISP_REG_SPR_WEIGHT0			0x0110
	#define SPR_PARA_UL_P0_D0_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_UL_P0_D0_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_UL_P0_D1_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_UL_P0_D1_Y REG_FLD_MSB_LSB(31, 24)

#define DISP_REG_SPR_WEIGHT1			0x0114
	#define SPR_PARA_UL_P0_D2_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_UL_P0_D2_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_UL_P1_D0_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_UL_P1_D0_Y REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_SPR_WEIGHT2			0x0118
	#define SPR_PARA_UL_P1_D1_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_UL_P1_D1_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_UL_P1_D2_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_UL_P1_D2_Y REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_SPR_WEIGHT3			0x011C
	#define SPR_PARA_UL_P2_D0_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_UL_P2_D0_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_UL_P2_D1_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_UL_P2_D1_Y REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_SPR_WEIGHT4			0x0120
	#define SPR_PARA_UL_P2_D2_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_UL_P2_D2_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_DL_P0_D0_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_DL_P0_D0_Y REG_FLD_MSB_LSB(31, 24)

#define DISP_REG_SPR_WEIGHT5			0x0124
	#define SPR_PARA_DL_P0_D1_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_DL_P0_D1_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_DL_P0_D2_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_DL_P0_D2_Y REG_FLD_MSB_LSB(31, 24)

#define DISP_REG_SPR_WEIGHT6			0x0128
	#define SPR_PARA_DL_P1_D0_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_DL_P1_D0_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_DL_P1_D1_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_DL_P1_D1_Y REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_SPR_WEIGHT7			0x012C
	#define SPR_PARA_DL_P1_D2_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_DL_P1_D2_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_DL_P2_D0_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_DL_P2_D0_Y REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_SPR_WEIGHT8			0x0130
	#define SPR_PARA_DL_P2_D1_X REG_FLD_MSB_LSB(7, 0)
	#define SPR_PARA_DL_P2_D1_Y REG_FLD_MSB_LSB(15, 8)
	#define SPR_PARA_DL_P2_D2_X REG_FLD_MSB_LSB(23, 16)
	#define SPR_PARA_DL_P2_D2_Y REG_FLD_MSB_LSB(31, 24)

#define DISP_REG_SPR_BORDER0			0x0134
	#define SPR_BORDER_UP_UL_P0_D0 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_UP_UL_P0_D1 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_UP_UL_P0_D2 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_UP_UL_P1_D0 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_UP_UL_P1_D1 REG_FLD_MSB_LSB(17, 16)
	#define SPR_BORDER_UP_UL_P1_D2 REG_FLD_MSB_LSB(21, 20)
	#define SPR_BORDER_UP_UL_P2_D0 REG_FLD_MSB_LSB(25, 24)
	#define SPR_BORDER_UP_UL_P2_D1 REG_FLD_MSB_LSB(29, 28)

#define DISP_REG_SPR_BORDER1			0x0138
	#define SPR_BORDER_UP_UL_P2_D2 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_UP_DL_P0_D0 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_UP_DL_P0_D1 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_UP_DL_P0_D2 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_UP_DL_P1_D0 REG_FLD_MSB_LSB(17, 16)
	#define SPR_BORDER_UP_DL_P1_D1 REG_FLD_MSB_LSB(21, 20)
	#define SPR_BORDER_UP_DL_P1_D2 REG_FLD_MSB_LSB(25, 24)
	#define SPR_BORDER_UP_DL_P2_D0 REG_FLD_MSB_LSB(29, 28)

#define DISP_REG_SPR_BORDER2			0x013C
	#define SPR_BORDER_UP_DL_P2_D1 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_UP_DL_P2_D2 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_DN_UL_P0_D0 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_DN_UL_P0_D1 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_DN_UL_P0_D2 REG_FLD_MSB_LSB(17, 16)
	#define SPR_BORDER_DN_UL_P1_D0 REG_FLD_MSB_LSB(21, 20)
	#define SPR_BORDER_DN_UL_P1_D1 REG_FLD_MSB_LSB(25, 24)
	#define SPR_BORDER_DN_UL_P1_D2 REG_FLD_MSB_LSB(29, 28)

#define DISP_REG_SPR_BORDER3			0x0140
	#define SPR_BORDER_DN_UL_P2_D0 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_DN_UL_P2_D1 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_DN_UL_P2_D2 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_DN_DL_P0_D0 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_DN_DL_P0_D1 REG_FLD_MSB_LSB(17, 16)
	#define SPR_BORDER_DN_DL_P0_D2 REG_FLD_MSB_LSB(21, 20)
	#define SPR_BORDER_DN_DL_P1_D0 REG_FLD_MSB_LSB(25, 24)
	#define SPR_BORDER_DN_DL_P1_D1 REG_FLD_MSB_LSB(29, 28)

#define DISP_REG_SPR_BORDER4			0x0144
	#define SPR_BORDER_DN_DL_P1_D2 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_DN_DL_P2_D0 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_DN_DL_P2_D1 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_DN_DL_P2_D2 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_LT_UL_P0_D0 REG_FLD_MSB_LSB(17, 16)
	#define SPR_BORDER_LT_UL_P0_D1 REG_FLD_MSB_LSB(21, 20)
	#define SPR_BORDER_LT_UL_P0_D2 REG_FLD_MSB_LSB(25, 24)
	#define SPR_BORDER_LT_UL_P1_D0 REG_FLD_MSB_LSB(29, 28)
#define DISP_REG_SPR_BORDER5			0x0148
	#define SPR_BORDER_LT_UL_P1_D1 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_LT_UL_P1_D2 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_LT_UL_P2_D0 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_LT_UL_P2_D1 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_LT_UL_P2_D2 REG_FLD_MSB_LSB(17, 16)
	#define SPR_BORDER_LT_DL_P0_D0 REG_FLD_MSB_LSB(21, 20)
	#define SPR_BORDER_LT_DL_P0_D1 REG_FLD_MSB_LSB(25, 24)
	#define SPR_BORDER_LT_DL_P0_D2 REG_FLD_MSB_LSB(29, 28)
#define DISP_REG_SPR_BORDER6			0x014C
	#define SPR_BORDER_LT_DL_P1_D0 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_LT_DL_P1_D1 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_LT_DL_P1_D2 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_LT_DL_P2_D0 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_LT_DL_P2_D1 REG_FLD_MSB_LSB(17, 16)
	#define SPR_BORDER_LT_DL_P2_D2 REG_FLD_MSB_LSB(21, 20)
	#define SPR_BORDER_RT_UL_P0_D0 REG_FLD_MSB_LSB(25, 24)
	#define SPR_BORDER_RT_UL_P0_D1 REG_FLD_MSB_LSB(29, 28)
#define DISP_REG_SPR_BORDER7			0x0150
	#define SPR_BORDER_RT_UL_P0_D2 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_RT_UL_P1_D0 REG_FLD_MSB_LSB(3, 2)
	#define SPR_BORDER_RT_UL_P1_D1 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_RT_UL_P1_D2 REG_FLD_MSB_LSB(7, 6)
	#define SPR_BORDER_RT_UL_P2_D0 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_RT_UL_P2_D1 REG_FLD_MSB_LSB(11, 10)
	#define SPR_BORDER_RT_UL_P2_D2 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_RT_DL_P0_D0 REG_FLD_MSB_LSB(15, 14)
#define DISP_REG_SPR_BORDER8			0x0154
	#define SPR_BORDER_RT_DL_P0_D1 REG_FLD_MSB_LSB(1, 0)
	#define SPR_BORDER_RT_DL_P0_D2 REG_FLD_MSB_LSB(3, 2)
	#define SPR_BORDER_RT_DL_P1_D0 REG_FLD_MSB_LSB(5, 4)
	#define SPR_BORDER_RT_DL_P1_D1 REG_FLD_MSB_LSB(7, 6)
	#define SPR_BORDER_RT_DL_P1_D2 REG_FLD_MSB_LSB(9, 8)
	#define SPR_BORDER_RT_DL_P2_D0 REG_FLD_MSB_LSB(11, 10)
	#define SPR_BORDER_RT_DL_P2_D1 REG_FLD_MSB_LSB(13, 12)
	#define SPR_BORDER_RT_DL_P2_D2 REG_FLD_MSB_LSB(15, 14)
#define DISP_REG_SPR_BORDER9			0x0158
	#define SPR_BORDER_UP_PARA0 REG_FLD_MSB_LSB(7, 0)
	#define SPR_BORDER_UP_PARA1 REG_FLD_MSB_LSB(15, 8)
	#define SPR_BORDER_DN_PARA0 REG_FLD_MSB_LSB(23, 16)
	#define SPR_BORDER_DN_PARA1 REG_FLD_MSB_LSB(31, 24)
#define DISP_REG_SPR_BORDER10			0x015C
	#define SPR_BORDER_LT_PARA0 REG_FLD_MSB_LSB(7, 0)
	#define SPR_BORDER_LT_PARA1 REG_FLD_MSB_LSB(15, 8)
	#define SPR_BORDER_RT_PARA0 REG_FLD_MSB_LSB(23, 16)
	#define SPR_BORDER_RT_PARA1 REG_FLD_MSB_LSB(31, 24)

#define DISP_REG_SPR_SPE0			0x0160
	#define SPR_SET4_CASE0_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET4_CASE0_EN BIT(12)
	#define SPR_SET4_CASE1_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET4_CASE1_EN BIT(28)
#define DISP_REG_SPR_SPE1			0x0164
	#define SPR_SET4_CASE2_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET4_CASE2_EN BIT(12)
	#define SPR_SET4_CASE3_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET4_CASE3_EN BIT(28)
#define DISP_REG_SPR_SPE2			0x0168
	#define SPR_SET4_CASE4_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET4_CASE4_EN BIT(12)
	#define SPR_SET4_CASE5_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET4_CASE5_EN BIT(28)
#define DISP_REG_SPR_SPE3			0x016C
	#define SPR_SET3_CASE0_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET3_CASE0_EN BIT(12)
	#define SPR_SET3_CASE1_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET3_CASE1_EN BIT(28)
#define DISP_REG_SPR_SPE4			0x0170
	#define SPR_SET3_CASE2_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET3_CASE2_EN BIT(12)
	#define SPR_SET3_CASE3_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET3_CASE3_EN BIT(28)
#define DISP_REG_SPR_SPE5			0x0174
	#define SPR_SET2_CASE0_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET2_CASE0_EN BIT(12)
	#define SPR_SET2_CASE1_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET2_CASE1_EN BIT(28)
#define DISP_REG_SPR_SPE6			0x0178
	#define SPR_SET2_CASE2_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET2_CASE2_EN BIT(12)
	#define SPR_SET2_CASE3_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET2_CASE3_EN BIT(28)
#define DISP_REG_SPR_SPE7			0x017C
	#define SPR_SET2_CASE4_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET2_CASE4_EN BIT(12)
	#define SPR_SET2_CASE5_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET2_CASE5_EN BIT(28)
#define DISP_REG_SPR_SPE8			0x0180
	#define SPR_SET2_CASE6_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET2_CASE6_EN BIT(12)
	#define SPR_SET2_CASE7_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET2_CASE7_EN BIT(28)
#define DISP_REG_SPR_SPE9			0x0184
	#define SPR_SET2_CASE8_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET2_CASE8_EN BIT(12)
	#define SPR_SET2_CASE9_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET2_CASE9_EN BIT(28)
#define DISP_REG_SPR_SPE10			0x0188
	#define SPR_SET2_CASE10_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET2_CASE10_EN BIT(12)
	#define SPR_SET2_CASE11_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET2_CASE11_EN BIT(28)
#define DISP_REG_SPR_SPE11			0x018C
	#define SPR_SET1_CASE0_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET1_CASE0_EN BIT(12)
	#define SPR_SET1_CASE1_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET1_CASE1_EN BIT(28)
#define DISP_REG_SPR_SPE12			0x0190
	#define SPR_SET1_CASE2_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET1_CASE2_EN BIT(12)
	#define SPR_SET1_CASE3_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET1_CASE3_EN BIT(28)
#define DISP_REG_SPR_SPE13			0x0194
	#define SPR_SET1_CASE4_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET1_CASE4_EN BIT(12)
	#define SPR_SET1_CASE5_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET1_CASE5_EN BIT(28)
#define DISP_REG_SPR_SPE14			0x0198
	#define SPR_SET1_CASE6_SET REG_FLD_MSB_LSB(8, 0)
	#define SPR_SET1_CASE6_EN BIT(12)
	#define SPR_SET1_CASE7_SET REG_FLD_MSB_LSB(24, 16)
	#define SPR_SET1_CASE7_EN BIT(28)

#define DISP_REG_SPR_GAMMA0			0x019C
	#define GV0 REG_FLD_MSB_LSB(12, 0)
	#define GV1 REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_SPR_GAMMA16			0x01DC
	#define GV32 REG_FLD_MSB_LSB(12, 0)
	#define TF_IN_EN BIT(16)

#define DISP_REG_SPR_DEGAMMA0			0x01E0
	#define DGV0 REG_FLD_MSB_LSB(11, 0)
	#define DGV1 REG_FLD_MSB_LSB(27, 16)
#define DISP_REG_SPR_DEGAMMA16			0x0224
	#define DGV32 REG_FLD_MSB_LSB(11, 0)
	#define TF_OUT_EN BIT(16)
	#define DEGAMMA_CLAMP_OPT BIT(17)
#define DISP_REG_SPR_DITHER_0			0x0300
	#define START BIT(0)
	#define OUT_SEL BIT(4)
	#define FRAME_DONE_DEL REG_FLD_MSB_LSB(15, 8)
	#define CRC_CEN BIT(16)
	#define CRC_START BIT(20)
	#define CRC_CLR BIT(24)
#define DISP_REG_SPR_DITHER_1			0x0304
	#define DITHER_SOFT_RESET BIT(0)
#define DISP_REG_SPR_DITHER_2			0x0308
	#define AUX0_I BIT(0)
	#define AUX1_I BIT(1)
#define DISP_REG_SPR_DITHER_5			0x0314
	#define W_DEMO REG_FLD_MSB_LSB(15, 0)
#define DISP_REG_SPR_DITHER_6			0x0318
	#define EDITHER_EN BIT(0)
	#define LFSR_EN BIT(1)
	#define RDITHER_EN BIT(2)
	#define ROUND_EN BIT(3)
	#define FPHASE REG_FLD_MSB_LSB(9, 4)
	#define FPHASE_EN BIT(12)
	#define FPHASE_R BIT(13)
	#define LEFT_EN REG_FLD_MSB_LSB(15, 14)
	#define WRAP_MODE BIT(16)
#define DISP_REG_SPR_DITHER_7			0x031C
	#define DRMODE_R REG_FLD_MSB_LSB(1, 0)
	#define DRMODE_G REG_FLD_MSB_LSB(5, 4)
	#define DRMODE_B REG_FLD_MSB_LSB(9, 8)
#define DISP_REG_SPR_DITHER_8			0x0320
	#define INK BIT(0)
	#define INK_DATA_R REG_FLD_MSB_LSB(27, 16)
#define DISP_REG_SPR_DITHER_9			0x0324
	#define INK_DATA_G REG_FLD_MSB_LSB(11, 0)
	#define INK_DATA_B REG_FLD_MSB_LSB(27, 16)
#define DISP_REG_SPR_DITHER_10			0x0328
	#define FPHASE_CTRL REG_FLD_MSB_LSB(1, 0)
	#define FPHASE_SEL REG_FLD_MSB_LSB(5, 4)
	#define FPHASE_BIT REG_FLD_MSB_LSB(10, 8)
#define DISP_REG_SPR_DITHER_11		0x032C
	#define SUBPIX_EN BIT(0)
	#define SUB_R REG_FLD_MSB_LSB(5, 4)
	#define SUB_G REG_FLD_MSB_LSB(9, 8)
	#define SUB_B REG_FLD_MSB_LSB(13, 12)
#define DISP_REG_SPR_DITHER_12		0x0330
	#define LSB_OFF BIT(0)
	#define TABLE_EN REG_FLD_MSB_LSB(5, 4)
	#define H_ACTIVE REG_FLD_MSB_LSB(31, 16)
#define DISP_REG_SPR_DITHER_13		0x0334
	#define RSHIFT_R REG_FLD_MSB_LSB(2, 0)
	#define RSHIFT_G REG_FLD_MSB_LSB(6, 4)
	#define RSHIFT_B REG_FLD_MSB_LSB(10, 8)
#define DISP_REG_SPR_DITHER_14		0x0338
	#define TESTPIN_EN BIT(0)
	#define DIFF_SHIFT REG_FLD_MSB_LSB(6, 4)
	#define DEBUG_MODE REG_FLD_MSB_LSB(9, 8)
#define DISP_REG_SPR_DITHER_15		0x033C
	#define NEW_BIT_MODE BIT(0)
	#define INPUT_RSHIFT_R REG_FLD_MSB_LSB(18, 16)
	#define ADD_LSHIFT_R REG_FLD_MSB_LSB(22, 20)
	#define OVFLW_BIT_R REG_FLD_MSB_LSB(26, 24)
	#define LSB_ERR_SHIFT_R REG_FLD_MSB_LSB(30, 28)
#define DISP_REG_SPR_DITHER_16		0x0340
	#define INPUT_RSHIFT_G REG_FLD_MSB_LSB(18, 16)
	#define ADD_LSHIFT_G REG_FLD_MSB_LSB(22, 20)
	#define OVFLW_BIT_G REG_FLD_MSB_LSB(26, 24)
	#define LSB_ERR_SHIFT_G REG_FLD_MSB_LSB(30, 28)
	#define INPUT_RSHIFT_B REG_FLD_MSB_LSB(18, 16)
	#define ADD_LSHIFT_B REG_FLD_MSB_LSB(22, 20)
	#define OVFLW_BIT_B REG_FLD_MSB_LSB(26, 24)
	#define LSB_ERR_SHIFT_B REG_FLD_MSB_LSB(30, 28)
#define DISP_REG_SPR_DITHER_17		0x0344
	#define CRC_OUT REG_FLD_MSB_LSB(15, 0)
	#define CRC_RDY BIT(16)
#define DISP_REG_SPR_DITHER_18		0x0348
	#define SPR_FUNC_DCM_DIS BIT(0)

/* MTK SPRv2 Registers define start */
/* TODO: The following SPRv2 registers need to be redefined when used */
//MT6985
#define MT6985_DISP_REG_SPR_V_BLANK        0x0028
	#define VBP                            REG_FLD_MSB_LSB(4, 0)
	#define VFP                            REG_FLD_MSB_LSB(20, 16)

#define MT6985_DISP_REG_SPR_EN_BF_HS       0x002C
	#define EN_BF_HS                       REG_FLD_MSB_LSB(7, 0)

#define DISP_REG_V2_SPR_ROI_SIZE       0x0030

#define MT6985_DISP_REG_SPR_FRAME_DONE_DEL 0x0034

#define MT6985_DISP_REG_SPR_SW_SCRATCH     0x0038

#define MT6985_DISP_REG_SPR_RDY_SEL        0x003C
	#define CROP_OUT_HSIZE                 REG_FLD_MSB_LSB(28, 16)

#define MT6985_DISP_REG_SPR_RDY_SEL_EN     0x0040
	#define CROP_OUT_VSIZE                 REG_FLD_MSB_LSB(28, 16)

#define MT6985_DISP_REG_SPR_CK_ON          0x0044
	#define CROP_HOFFSET                   REG_FLD_MSB_LSB(23, 16)
	#define CROP_VOFFSET                   REG_FLD_MSB_LSB(31, 24)

#define MT6985_DISP_REG_SPR_H_BLANK        0x0048
	#define HBP                            REG_FLD_MSB_LSB(7, 0)
	#define HFP                            REG_FLD_MSB_LSB(15, 8)

#define MT6985_DISP_REG_SPR_OPTION         0x004C
	#define PADDING_ZERO                   REG_FLD_MSB_LSB(0, 0)
	#define AUTO_CROP                      REG_FLD_MSB_LSB(4, 4)
	#define ABNORMAL_RECOVER_EN            REG_FLD_MSB_LSB(8, 8)
	#define H_PROCH_MANUAL                 REG_FLD_MSB_LSB(12, 12)
	#define PIPE_LATENCY                   REG_FLD_MSB_LSB(20, 16)
	#define CUP_DATA_DOUBLE_BUF            REG_FLD_MSB_LSB(28, 28)

#define MT6985_DISP_REG_SPR_DUMMY          0x0074

#define MT6985_DISP_REG_SPR_ARRANGE1       0x007C

#define DISP_V2_SPR_IP_PARAMS_NUM           (832)

#define DISP_V2_SPR_IP_SHRINK_PARAMS_NUM    (123)

#define DISP_REG_V2_SPR_IP_CFG_0       0x0080

#define DISP_REG_V2_SPR_IP_CFG_18      0x00C8

#define MT6985_DISP_REG_SPR_IP_CFG_831     0x0D7C

#define MT6985_DISP_REG_SPR_DITHER_0       0x0D80

#define MT6985_DISP_REG_SPR_DITHER_1       0x0D84

#define MT6985_DISP_REG_SPR_DITHER_2       0x0D88

#define MT6985_DISP_REG_SPR_DITHER_5       0x0D8C

#define MT6985_DISP_REG_SPR_DITHER_6       0x0D90

#define MT6985_DISP_REG_SPR_DITHER_7       0x0D94

#define MT6985_DISP_REG_SPR_DITHER_8       0x0D98

#define MT6985_DISP_REG_SPR_DITHER_9       0x0D9C

#define MT6985_DISP_REG_SPR_DITHER_10      0x0DA0

#define MT6985_DISP_REG_SPR_DITHER_11      0x0DA4

#define MT6985_DISP_REG_SPR_DITHER_12      0x0DA8

#define MT6985_DISP_REG_SPR_DITHER_13      0x0DAC

#define MT6985_DISP_REG_SPR_DITHER_14      0x0DB0

#define MT6985_DISP_REG_SPR_DITHER_15      0x0DB4

#define MT6985_DISP_REG_SPR_DITHER_16      0x0DB8

#define MT6985_DISP_REG_SPR_DITHER_17      0x0DBC

#define MT6985_DISP_REG_SPR_DITHER_18      0x0DC0
	#define PARA_DUMMY                     REG_FLD_MSB_LSB(31, 16)

#define MT6985_DISP_REG_SPR_IP_DBG0        0x0DC4
	#define FX_RND_X_DATA_OUT              REG_FLD_MSB_LSB(23, 0)

#define MT6985_DISP_REG_SPR_IP_DBG1        0x0DC8
	#define FX_SMO_OUT                     REG_FLD_MSB_LSB(23, 0)

#define MT6985_DISP_REG_SPR_IP_DBG2        0x0DCC
	#define SPR_VERSION                    REG_FLD_MSB_LSB(7, 0)

#define MT6985_DISP_REG_SPR_CUP_0          0x0DD0
	#define SPR_CUP_ADDR                   REG_FLD_MSB_LSB(0, 0)
	#define SPR_CUP_SEL                    REG_FLD_MSB_LSB(9, 8)
	#define SPR_CUP_INC                    REG_FLD_MSB_LSB(24, 16)

#define MT6985_DISP_REG_SPR_CUP_1          0x0DD4
	#define SPR_CUP_DATA_LSB               REG_FLD_MSB_LSB(31, 0)

#define MT6985_DISP_REG_SPR_CUP_2          0x0DD8
	#define SPR_CUP_DATA_MSB               REG_FLD_MSB_LSB(15, 0)

#define MT6985_DISP_REG_SPR_CUP_SRAM_R_IF  0x0DE0
	#define SPR_CUP_SRAM_RDATA             REG_FLD_MSB_LSB(31, 0)

#define MT6985_DISP_REG_SPR_CUP_SRAM_R_IF_MSB   0x0DE4
	#define SPR_CUP_SRAM_RDATA_MSB              REG_FLD_MSB_LSB(15, 0)
/* MTK SPRv2 Registers define end */

//MT6989 SPR
#define MT6989_DISP_REG_SPR_CROP_SIZE           0x0040
	#define MT6989_CROP_OUT_HSIZE               REG_FLD_MSB_LSB(12, 0)
	#define MT6989_CROP_OUT_VSIZE               REG_FLD_MSB_LSB(28, 16)

//MT6989 Postalign
#define MT6989_DISP_REG_POSTALIGN0_EN           0x000
#define MT6989_DISP_REG_POSTALIGN0_RESET        0x004
#define MT6989_DISP_REG_POSTALIGN0_INTEN        0x008
	#define MT6989_IF_END_INT_EN                BIT(0)
	#define MT6989_OF_END_INT_EN                BIT(1)
#define MT6989_DISP_REG_POSTALIGN0_SHADOW_CTRL  0x014
	#define MT6989_FORCE_COMMIT                 BIT(0)
	#define MT6989_BYPASS_SHADOW                BIT(1)
	#define MT6989_READ_WRK_REG                 BIT(2)
#define MT6989_DISP_REG_POSTALIGN0_CFG          0x018
	#define MT6989_POSTALIGN_SEL                REG_FLD_MSB_LSB(5, 0)
	#define MT6989_POSTALIGN_6TYPE_MODE         BIT(8)
	#define MT6989_DSC_PADDING_REPEAT_EN        BIT(12)
	#define MT6989_RELAY_MODE                   BIT(16)
	#define MT6989_POSTALIGN_LUT_EN             BIT(17)
	#define MT6989_STALL_CG_ON                  BIT(18)
#define MT6989_DISP_REG_POSTALIGN0_SIZE         0x01C
	#define MT6989_VSIZE                        REG_FLD_MSB_LSB(12, 0)
	#define MT6989_HSIZE                        REG_FLD_MSB_LSB(28, 16)
#define MT6989_DISP_REG_POSTALIGN0_ARRANGE      0x020
	#define MT6989_SPR_PIXELGROUP               REG_FLD_MSB_LSB(1, 0)
	#define MT6989_SPR_ARRANGE_UL_P0            REG_FLD_MSB_LSB(6, 4)
	#define MT6989_SPR_ARRANGE_UL_P1            REG_FLD_MSB_LSB(10, 8)
	#define MT6989_SPR_ARRANGE_UL_P2            REG_FLD_MSB_LSB(14, 12)
	#define MT6989_SPR_ARRANGE_DL_P0            REG_FLD_MSB_LSB(18, 16)
	#define MT6989_SPR_ARRANGE_DL_P1            REG_FLD_MSB_LSB(22, 20)
	#define MT6989_SPR_ARRANGE_DL_P2            REG_FLD_MSB_LSB(26, 24)

//dispsys
#define DISP_REG_POSTALIGN0_CON0                0x050
	#define DISP_POSTALIGN0_ENG_EN              REG_FLD_MSB_LSB(0, 0)
	#define DISP_POSTALIGN0_ENG_RESET           REG_FLD_MSB_LSB(1, 1)
	#define DISP_POSTALIGN0_EN                  REG_FLD_MSB_LSB(16, 16)
	#define DISP_POSTALIGN0_SEL                 REG_FLD_MSB_LSB(25, 20)
	#define DISP_POSTALIGN0_PADDING_REPEAT_EN   REG_FLD_MSB_LSB(28, 28)
	#define DISP_POSTALIGN0_6TYPE_MODE          REG_FLD_MSB_LSB(31, 31)

#define DISP_REG_POSTALIGN0_CON1                0x054
	#define DISP_POSTALIGN0_HSIZE               REG_FLD_MSB_LSB(12, 0)
	#define DISP_POSTALIGN0_VSIZE               REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_POSTALIGN0_CON2                0x058
	#define DISP_POSTALIGN0_SPR_PIXELGROUP      REG_FLD_MSB_LSB(1, 0)
	#define DISP_POSTALIGN0_SPR_ARRANGE_UL_P0   REG_FLD_MSB_LSB(6, 4)
	#define DISP_POSTALIGN0_SPR_ARRANGE_UL_P1   REG_FLD_MSB_LSB(10, 8)
	#define DISP_POSTALIGN0_SPR_ARRANGE_UL_P2   REG_FLD_MSB_LSB(14, 12)
	#define DISP_POSTALIGN0_SPR_ARRANGE_DL_P0   REG_FLD_MSB_LSB(18, 16)
	#define DISP_POSTALIGN0_SPR_ARRANGE_DL_P1   REG_FLD_MSB_LSB(22, 20)
	#define DISP_POSTALIGN0_SPR_ARRANGE_DL_P2   REG_FLD_MSB_LSB(26, 24)

#define DISP_REG_POSTALIGN0_MON0                0x05C
	#define DISP_POSTALIGN0_PIX_CNT_OUT         REG_FLD_MSB_LSB(12, 0)
	#define DISP_POSTALIGN0_LINT_CNT_OUT        REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_POSTALIGN0_MON1                0x060
	#define DISP_POSTALIGN0_ROUT                REG_FLD_MSB_LSB(9, 0)
	#define DISP_POSTALIGN0_GOUT                REG_FLD_MSB_LSB(19, 10)
	#define DISP_POSTALIGN0_BOUT                REG_FLD_MSB_LSB(29, 20)

enum mtk_spr_version {
	MTK_SPR_V1,
	MTK_SPR_V2,
};

struct mtk_disp_spr_data {
	bool support_shadow;
	bool need_bypass_shadow;
	enum mtk_spr_version version;
	bool shrink_cfg;
};

/**
 * struct mtk_disp_spr - DISP_SPR driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_spr {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_spr_data *data;
	int enable;
	struct mtk_disp_spr_tile_overhead_v tile_overhead_v;
	bool set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_disp_spr *comp_to_spr(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_spr, ddp_comp);
}

static void mtk_spr_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;

	mtk_ddp_write_mask(comp, SPR_FORCE_COMMIT,
		DISP_REG_SPR_EN, SPR_FORCE_COMMIT, handle);

	mtk_ddp_write_mask(comp, SPR_EN, DISP_REG_SPR_EN,
		SPR_EN, handle);
	mtk_ddp_write_mask(comp, SPR_LUT_EN, DISP_REG_SPR_EN,
		SPR_LUT_EN, handle);

	DDPINFO("%s, spr_start:0x%x\n",
		mtk_dump_comp_str(comp), readl(baddr + DISP_REG_SPR_EN));
}

static void mtk_spr_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;

	mtk_ddp_write_mask(comp, 0x0, DISP_REG_SPR_EN, SPR_EN, handle);
	mtk_ddp_write_mask(comp, 0x0, DISP_REG_SPR_EN, SPR_LUT_EN, handle);

	DDPINFO("%s, spr_stop:0x%x\n",
		mtk_dump_comp_str(comp), readl(baddr + DISP_REG_SPR_EN));
}

static void mtk_spr_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_spr *spr = NULL;
	struct mtk_ddp_comp *out_comp = NULL;
	unsigned int panel_spr_enable;

	DDPINFO("%s+\n", __func__);

	if (!comp)
		return;
	spr = comp_to_spr(comp);

	mtk_ddp_comp_clk_prepare(comp);

	if (spr->data) {
		if (spr->data->need_bypass_shadow)
			mtk_ddp_write_mask_cpu(comp, SPR_BYPASS_SHADOW,
					DISP_REG_SPR_EN, SPR_BYPASS_SHADOW);
		else
			mtk_ddp_write_mask_cpu(comp, 0,
					DISP_REG_SPR_EN, SPR_BYPASS_SHADOW);
	}

	if (comp && comp->mtk_crtc && comp->mtk_crtc->panel_ext
		&& comp->mtk_crtc->panel_ext->params
		&& comp->mtk_crtc->panel_ext->params->spr_params.enable == 1
		&& comp->mtk_crtc->panel_ext->params->spr_params.relay == 0) {
		if (comp->mtk_crtc->spr_is_on)
			panel_spr_enable = 0xfefe;
		else
			panel_spr_enable = 0xeeee;
	}

	if(comp && comp->mtk_crtc)
		out_comp = mtk_ddp_comp_request_output(comp->mtk_crtc);

	if (out_comp && out_comp->funcs && out_comp->funcs->io_cmd)
		out_comp->funcs->io_cmd(out_comp, NULL, DSI_SET_PANEL_SPR, &panel_spr_enable);

	if (spr->data && spr->data->version == MTK_SPR_V2) {
		if (comp->mtk_crtc && comp->mtk_crtc->panel_ext &&
			comp->mtk_crtc->panel_ext->params) {
			struct mtk_panel_spr_params *spr_params;
			unsigned int i;

			spr_params = &comp->mtk_crtc->panel_ext->params->spr_params;
			if ((spr_params->enable == 1) && (spr_params->relay == 0)
			 && (spr->data->shrink_cfg == false)) {
				DDPINFO("%s: spr ip config\n", __func__);
				for (i = 1; i < DISP_V2_SPR_IP_PARAMS_NUM; i++)
					mtk_ddp_write_relaxed(comp,
						*(spr_params->spr_ip_params + i),
						(DISP_REG_V2_SPR_IP_CFG_0 + 0x4 * i), NULL);
			}
			if ((spr_params->enable == 1) && (spr_params->relay == 0)
			 && (spr->data->shrink_cfg == true)) {
				DDPINFO("%s: spr ip shrink config\n", __func__);
				for (i = 1; i < DISP_V2_SPR_IP_SHRINK_PARAMS_NUM; i++)
					mtk_ddp_write_relaxed(comp,
						*(spr_params->spr_ip_shrink_params + i),
						(DISP_REG_V2_SPR_IP_CFG_0 + 0x4 * i), NULL);
			}
		}
	}
}

static void mtk_spr_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

extern unsigned int disp_spr_bypass;

static void mtk_spr_color_tune_config(struct mtk_ddp_comp *comp,
				 struct cmdq_pkt *handle)
{
	struct mtk_panel_spr_params *spr_params;
	struct mtk_panel_spr_params *spr_params_tune;
	struct spr_color_params *spr_color_params;
	struct spr_color_params *spr_color_params_tune;
	u32 reg_val;
	int i = 0, j = 0;

	if (!comp->mtk_crtc || !comp->mtk_crtc->panel_ext)
		return;
	DDPINFO("%s\n", __func__);

	spr_params_tune = comp->mtk_crtc->panel_spr_params;
	if (!spr_params_tune)
		return;
	DDPINFO("%s+\n", __func__);
	spr_params = &comp->mtk_crtc->panel_ext->params->spr_params;

	for (j = 0; j < SPR_COLOR_PARAMS_TYPE_NUM; j++) {
		spr_color_params = &spr_params->spr_color_params[j];
		spr_color_params_tune = &spr_params_tune->spr_color_params[j];
		DDPINFO("%s, spr_color:0x%x\n", mtk_dump_comp_str(comp),
			spr_color_params->spr_color_params_type);
		switch (spr_color_params->spr_color_params_type) {
		case SPR_WEIGHT_SET:
			for (i = 0; i < spr_color_params->count; i += 4) {
				reg_val = (spr_color_params_tune->tune_list[i] == 1 ?
					spr_color_params_tune->para_list[i] :
					spr_color_params->para_list[i]) |
					((spr_color_params_tune->tune_list[i+1] == 1 ?
					spr_color_params_tune->para_list[i+1] :
					spr_color_params->para_list[i+1]) << 8) |
					((spr_color_params_tune->tune_list[i+2] == 1 ?
					spr_color_params_tune->para_list[i+2] :
					spr_color_params->para_list[i+2]) << 16) |
					((spr_color_params_tune->tune_list[i+3] == 1 ?
					spr_color_params_tune->para_list[i+3] :
					spr_color_params->para_list[i+3]) << 24);
				DDPINFO("%s, spr_weight:0x%x count:%d\n", mtk_dump_comp_str(comp),
					reg_val, spr_color_params->count);
				mtk_ddp_write_relaxed(comp, reg_val,
					DISP_REG_SPR_WEIGHT0 + 0x4 * i/4, handle);
			}
			break;
		case SPR_BORDER_SET:
			for (i = 0; i < 72; i += 8) {
				reg_val = (spr_color_params_tune->tune_list[i] == 1 ?
					spr_color_params_tune->para_list[i] :
					spr_color_params->para_list[i]) |
					((spr_color_params_tune->tune_list[i+1] == 1 ?
					spr_color_params_tune->para_list[i+1] :
					spr_color_params->para_list[i+1]) << 4) |
					((spr_color_params_tune->tune_list[i+2] == 1 ?
					spr_color_params_tune->para_list[i+2] :
					spr_color_params->para_list[i+2]) << 8) |
					((spr_color_params_tune->tune_list[i+3] == 1 ?
					spr_color_params_tune->para_list[i+3] :
					spr_color_params->para_list[i+3]) << 12) |
					((spr_color_params_tune->tune_list[i+4] == 1 ?
					spr_color_params_tune->para_list[i+4] :
					spr_color_params->para_list[i+4]) << 16) |
					((spr_color_params_tune->tune_list[i+5] == 1 ?
					spr_color_params_tune->para_list[i+5] :
					spr_color_params->para_list[i+5]) << 20) |
					((spr_color_params_tune->tune_list[i+6] == 1 ?
					spr_color_params_tune->para_list[i+6] :
					spr_color_params->para_list[i+6]) << 24) |
					((spr_color_params_tune->tune_list[i+7] == 1 ?
					spr_color_params_tune->para_list[i+7] :
					spr_color_params->para_list[i+7]) << 28);
				mtk_ddp_write(comp, reg_val,
					DISP_REG_SPR_BORDER0 + 0x4 * i/8, handle);
			}
			for (i = 72; i < spr_color_params->count; i += 4) {
				reg_val = (spr_color_params_tune->tune_list[i] == 1 ?
					spr_color_params_tune->para_list[i] :
					spr_color_params->para_list[i]) |
					((spr_color_params_tune->tune_list[i+1] == 1 ?
					spr_color_params_tune->para_list[i+1] :
					spr_color_params->para_list[i+1]) << 8) |
					((spr_color_params_tune->tune_list[i+2] == 1 ?
					spr_color_params_tune->para_list[i+2] :
					spr_color_params->para_list[i+2]) << 16) |
					((spr_color_params_tune->tune_list[i+3] == 1 ?
					spr_color_params_tune->para_list[i+3] :
					spr_color_params->para_list[i+3]) << 24);
				mtk_ddp_write(comp, reg_val,
					DISP_REG_SPR_BORDER9 + 0x4 * (i - 72)/4, handle);
			}
			break;
		case SPR_SPE_SET:
			for (i = 0; i < spr_color_params->count; i += 4) {
				reg_val = (spr_color_params_tune->tune_list[i] == 1 ?
					spr_color_params_tune->para_list[i] :
					spr_color_params->para_list[i]) |
					((spr_color_params_tune->tune_list[i+1] == 1 ?
					spr_color_params_tune->para_list[i+1] :
					spr_color_params->para_list[i+1]) << 8) |
					((spr_color_params_tune->tune_list[i+2] == 1 ?
					spr_color_params_tune->para_list[i+2] :
					spr_color_params->para_list[i+2]) << 16) |
					((spr_color_params_tune->tune_list[i+3] == 1 ?
					spr_color_params_tune->para_list[i+3] :
					spr_color_params->para_list[i+3]) << 24);
				DDPINFO("%s, spr_weight:0x%x count:%d\n", mtk_dump_comp_str(comp),
					reg_val, spr_color_params->count);
				mtk_ddp_write_relaxed(comp, reg_val,
					DISP_REG_SPR_SPE0 + 0x4 * i/4, handle);
			}
			break;
		default:
			break;
		}
		}
}

static void mtk_spr_color_config(struct mtk_ddp_comp *comp,
				 struct cmdq_pkt *handle)
{
	struct mtk_panel_spr_params *spr_params;
	struct spr_color_params *spr_color_params;
	u32 reg_val;
	int i = 0, j = 0;

	if (!comp->mtk_crtc || !comp->mtk_crtc->panel_ext)
		return;
	DDPINFO("%s\n", __func__);

	spr_params = &comp->mtk_crtc->panel_ext->params->spr_params;

	for (j = 0; j < SPR_COLOR_PARAMS_TYPE_NUM; j++) {
		spr_color_params = &spr_params->spr_color_params[j];
		DDPINFO("%s, spr_color:0x%x\n", mtk_dump_comp_str(comp),
			spr_color_params->spr_color_params_type);
		switch (spr_color_params->spr_color_params_type) {
		case SPR_WEIGHT_SET:
			for (i = 0; i < spr_color_params->count; i += 4) {
				reg_val = (spr_color_params->para_list[i]) |
					(spr_color_params->para_list[i+1] << 8) |
					(spr_color_params->para_list[i+2] << 16) |
					(spr_color_params->para_list[i+3] << 24);
				DDPINFO("%s, spr_weight:0x%x count:%d\n", mtk_dump_comp_str(comp),
					reg_val, spr_color_params->count);
				mtk_ddp_write_relaxed(comp, reg_val,
					DISP_REG_SPR_WEIGHT0 + 0x4 * i/4, handle);
			}
			break;
		case SPR_BORDER_SET:
			for (i = 0; i < 72; i += 8) {
				reg_val = (spr_color_params->para_list[i]) |
					(spr_color_params->para_list[i+1] << 4) |
					(spr_color_params->para_list[i+2] << 8) |
					(spr_color_params->para_list[i+3] << 12) |
					(spr_color_params->para_list[i+4] << 16) |
					(spr_color_params->para_list[i+5] << 20) |
					(spr_color_params->para_list[i+6] << 24) |
					(spr_color_params->para_list[i+7] << 28);
				mtk_ddp_write(comp, reg_val,
					DISP_REG_SPR_BORDER0 + 0x4 * i/8, handle);
			}
			for (i = 72; i < spr_color_params->count; i += 4) {
				reg_val = (spr_color_params->para_list[i]) |
					(spr_color_params->para_list[i+1] << 8) |
					(spr_color_params->para_list[i+2] << 16) |
					(spr_color_params->para_list[i+3] << 24);
				mtk_ddp_write(comp, reg_val,
					DISP_REG_SPR_BORDER9 + 0x4 * (i - 72)/4, handle);
			}
			break;
		case SPR_SPE_SET:
			for (i = 0; i < spr_color_params->count; i += 4) {
				reg_val = (spr_color_params->para_list[i]) |
					(spr_color_params->para_list[i+1] << 8) |
					(spr_color_params->para_list[i+2] << 16) |
					(spr_color_params->para_list[i+3] << 24);
				DDPINFO("%s, spr_weight:0x%x count:%d\n", mtk_dump_comp_str(comp),
					reg_val, spr_color_params->count);
				mtk_ddp_write_relaxed(comp, reg_val,
					DISP_REG_SPR_SPE0 + 0x4 * i/4, handle);
			}
			break;
		default:
			break;
		}
		}
}

static void mtk_spr_config_V1(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	struct mtk_panel_spr_params *spr_params;
	struct mtk_panel_spr_params *spr_params_tune;
	u32 reg_val;
	unsigned int width;

	if (comp && comp->mtk_crtc && comp->mtk_crtc->is_dual_pipe)
		width = cfg->w / 2;
	else
		width = cfg->w;

	if (!comp || !comp->mtk_crtc || !comp->mtk_crtc->panel_ext)
		return;

	spr_params = &comp->mtk_crtc->panel_ext->params->spr_params;
	spr_params_tune = comp->mtk_crtc->panel_spr_params;
	mtk_ddp_write_relaxed(comp,
			cfg->h << 16 | width,
			DISP_REG_SPR_ROI_SIZE, handle);
	mtk_ddp_write_mask(comp, SPR_EN, DISP_REG_SPR_EN,
				SPR_EN, handle);
	mtk_ddp_write_mask(comp, SPR_LUT_EN, DISP_REG_SPR_EN,
				SPR_LUT_EN, handle);
	mtk_ddp_write_mask(comp, SPR_STALL_CG_ON, DISP_REG_SPR_CFG,
				SPR_STALL_CG_ON, handle);
	mtk_ddp_write_mask(comp, RG_BYPASS_DITHER, DISP_REG_SPR_CFG,
				RG_BYPASS_DITHER, handle);
	mtk_ddp_write_mask(comp, width << 16, DISP_REG_SPR_RDY_SEL,
				width << 16 | 0xffff, handle);
	mtk_ddp_write_mask(comp, cfg->h << 16, DISP_REG_SPR_RDY_SEL_EN,
				cfg->h << 16 | 0xffff, handle);
	if (disp_spr_bypass) {
		/*enable spr relay mode*/
		mtk_ddp_write_mask(comp, SPR_RELAY_MODE, DISP_REG_SPR_EN,
				SPR_RELAY_MODE, handle);
		return;
	}
	if (spr_params->enable == 1) {
		mtk_ddp_write_mask(comp, 0, DISP_REG_SPR_EN,
				SPR_RELAY_MODE, handle);
		if (spr_params->bypass_dither == 0) {
			mtk_ddp_write_mask(comp, LFSR_EN, DISP_REG_SPR_DITHER_6,
							LFSR_EN, handle);
			mtk_ddp_write_mask(comp, RDITHER_EN, DISP_REG_SPR_DITHER_6,
							RDITHER_EN, handle);
		}
		//mtk_ddp_write_relaxed(comp, 0x21, DISP_REG_SPR_CFG, handle);
		mtk_ddp_write_mask(comp, spr_params->specialcaseen << 28,
					DISP_REG_SPR_CTRL, SPR_SPECIALCASEEN, handle);
		/*0:5line buffers 1:3line buffers 2:0line buffers*/
		mtk_ddp_write_mask(comp, SPR_POWERSAVING,
					DISP_REG_SPR_CTRL, SPR_POWERSAVING, handle);

		reg_val = (!!spr_params->postalign_6type_mode_en << 31) |
			(!!spr_params->padding_repeat_en << 28) |
			(!!spr_params->postalign_en << (20 + spr_params->spr_format_type)) |
			(!!spr_params->postalign_en << 16) |
			(!!spr_params->bypass_dither << 5) |
			(!!spr_params->rgb_swap << 4) |
			(!!spr_params->outdata_res_sel << 2) |
			(!!spr_params->indata_res_sel << 1) | 1;
		mtk_ddp_write_relaxed(comp, reg_val,
				DISP_REG_SPR_CFG, handle);
		switch (spr_params->spr_format_type) {
		case MTK_PANEL_RGBG_BGRG_TYPE:
			reg_val = 0x00050502;
			break;
		case MTK_PANEL_BGRG_RGBG_TYPE:
			reg_val = 0x00500052;
			break;
		case MTK_PANEL_RGBRGB_BGRBGR_TYPE:
			reg_val = 0x03154203;
			break;
		case MTK_PANEL_BGRBGR_RGBRGB_TYPE:
			reg_val = 0x04203153;
			break;
		case MTK_PANEL_RGBRGB_BRGBRG_TYPE:
			reg_val = 0x04200423;
			break;
		case MTK_PANEL_BRGBRG_RGBRGB_TYPE:
			reg_val = 0x00424203;
			break;
		default:
			reg_val = 0x03154203;
			break;
		}

		mtk_ddp_write_relaxed(comp, reg_val, DISP_REG_SPR_ARRANGE1, handle);
		if (spr_params_tune && spr_params_tune->enable)
			mtk_spr_color_tune_config(comp, handle);
		else
			mtk_spr_color_config(comp, handle);
	} else {
		/*enable spr relay mode*/
		mtk_ddp_write_mask(comp, SPR_RELAY_MODE, DISP_REG_SPR_EN,
				SPR_RELAY_MODE, handle);
	}
}

static unsigned int disp_spr_get_tile_overhead(unsigned int type)
{
	switch (type) {
	case MTK_PANEL_RGBG_BGRG_TYPE:
		return 4;
	case MTK_PANEL_BGRG_RGBG_TYPE:
		return 4;
	case MTK_PANEL_RGBRGB_BGRBGR_TYPE:
		return 6;
	case MTK_PANEL_BGRBGR_RGBRGB_TYPE:
		return 6;
	case MTK_PANEL_RGBRGB_BRGBRG_TYPE:
		return 6;
	case MTK_PANEL_BRGBRG_RGBRGB_TYPE:
		return 6;
	default:
		return 0;
	}
}
struct mtk_disp_spr_tile_overhead {
	unsigned int left_in_width;
	unsigned int left_overhead;
	unsigned int left_comp_overhead;
	unsigned int right_in_width;
	unsigned int right_overhead;
	unsigned int right_comp_overhead;
};

struct mtk_disp_spr_tile_overhead spr_tile_overhead;

static void mtk_disp_spr_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	unsigned int tile_overhead;
	struct mtk_panel_spr_params *spr_params;
	struct mtk_disp_spr *spr = comp_to_spr(comp);

	spr_params = &comp->mtk_crtc->panel_ext->params->spr_params;

	if (cfg->tile_overhead.is_support && spr->data && spr->data->version == MTK_SPR_V2) {
		if (comp->id == DDP_COMPONENT_SPR0) {
			if (spr_params->enable == 1 && spr_params->relay == 0) {
				tile_overhead =
					disp_spr_get_tile_overhead(spr_params->spr_format_type);
				spr_tile_overhead.left_comp_overhead =
					(cfg->tile_overhead.left_in_width + tile_overhead + 3)
					/ 4 * 4 - cfg->tile_overhead.left_in_width;
			} else
				spr_tile_overhead.left_comp_overhead = 0;
			cfg->tile_overhead.left_overhead += spr_tile_overhead.left_comp_overhead;
			cfg->tile_overhead.left_in_width += spr_tile_overhead.left_comp_overhead;
			spr_tile_overhead.left_in_width = cfg->tile_overhead.left_in_width;
			spr_tile_overhead.left_overhead = cfg->tile_overhead.left_overhead;
		}

		if (comp->id == DDP_COMPONENT_SPR1) {
			if (spr_params->enable == 1 && spr_params->relay == 0) {
				tile_overhead =
					disp_spr_get_tile_overhead(spr_params->spr_format_type);
				spr_tile_overhead.right_comp_overhead =
					(cfg->tile_overhead.right_in_width + tile_overhead + 3)
					/ 4 * 4 - cfg->tile_overhead.right_in_width;
			} else
				spr_tile_overhead.right_comp_overhead = 0;
			cfg->tile_overhead.right_overhead += spr_tile_overhead.right_comp_overhead;
			cfg->tile_overhead.right_in_width += spr_tile_overhead.right_comp_overhead;
			spr_tile_overhead.right_in_width = cfg->tile_overhead.right_in_width;
			spr_tile_overhead.right_overhead = cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_disp_spr_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_spr *spr = comp_to_spr(comp);

	DDPDBG("%s line: %d\n", __func__, __LINE__);

	/*set component overhead*/
	spr->tile_overhead_v.comp_overhead_v = 4;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
			spr->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	spr->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

int mtk_spr_check_postalign_status(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_drm_private *priv;
	struct mtk_ddp_comp *postalign_comp;
	unsigned int postalign_cfg;
	void __iomem *baddr;

	if(!mtk_crtc)
		return -1;

	priv = mtk_crtc->base.dev->dev_private;
	if(priv == NULL)
		return -1;
	if(priv->data->mmsys_id == MMSYS_MT6989) {
		postalign_comp = priv->ddp_comp[DDP_COMPONENT_POSTALIGN0];
		baddr = postalign_comp->regs;
		postalign_cfg = readl(baddr + MT6989_DISP_REG_POSTALIGN0_CFG);
		if(postalign_cfg&MT6989_RELAY_MODE)
			return 1;
		else
			return 0;
	}
	return -1;
}

static void mtk_spr_config_V2(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	struct mtk_panel_spr_params *spr_params;
	unsigned int width, height;
	unsigned int out_height = cfg->h;
	unsigned int postalign_width;
	u32 reg_val;
	void __iomem *config_regs;
	resource_size_t config_regs_pa;
	unsigned int crop_hoffset = 0;
	unsigned int crop_voffset = 0;
	unsigned int crop_out_hsize = 0;
	struct mtk_drm_private *priv;
	struct mtk_ddp_comp *postalign_comp;
	struct mtk_disp_spr *spr;
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	if (!comp || !comp->mtk_crtc || !comp->mtk_crtc->panel_ext)
		return;

	priv = comp->mtk_crtc->base.dev->dev_private;
	if (priv == NULL)
		return;
	postalign_comp = priv->ddp_comp[DDP_COMPONENT_POSTALIGN0];

	if (comp->id == DDP_COMPONENT_SPR0) {
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			postalign_comp = priv->ddp_comp[DDP_COMPONENT_POSTALIGN0];
			config_regs = postalign_comp->regs;
			config_regs_pa = postalign_comp->regs_pa;
		} else {
			config_regs = comp->mtk_crtc->config_regs;
			config_regs_pa = comp->mtk_crtc->config_regs_pa;
		}
	} else {
		config_regs = comp->mtk_crtc->side_config_regs;
		config_regs_pa = comp->mtk_crtc->side_config_regs_pa;
	}

	spr_params = &comp->mtk_crtc->panel_ext->params->spr_params;
	spr = comp_to_spr(comp);

	if (comp->mtk_crtc->is_dual_pipe == true) {
		postalign_width = cfg->w / 2;
		width = cfg->w / 2;
		if (cfg->tile_overhead.is_support) {
			if (comp->id == DDP_COMPONENT_SPR0) {
				width = spr_tile_overhead.left_in_width;
				crop_hoffset = 0;
				crop_out_hsize = width - spr_tile_overhead.left_comp_overhead;
			}
			if (comp->id == DDP_COMPONENT_SPR1) {
				width = spr_tile_overhead.right_in_width;
				crop_hoffset = spr_tile_overhead.right_comp_overhead;
				crop_out_hsize = width - spr_tile_overhead.right_comp_overhead;
			}
		} else {
			width = cfg->w / 2;
			crop_out_hsize = width;
		}
		height = cfg->h;

		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_mask(comp, (crop_out_hsize) << 0,
				MT6989_DISP_REG_SPR_CROP_SIZE, REG_FLD_MASK(MT6989_CROP_OUT_HSIZE), handle);
			mtk_ddp_write_mask(comp, height << 16,
				MT6989_DISP_REG_SPR_CROP_SIZE, REG_FLD_MASK(MT6989_CROP_OUT_VSIZE), handle);
		} else {
			mtk_ddp_write_mask(comp, (crop_out_hsize) << 16,
				DISP_REG_SPR_RDY_SEL, REG_FLD_MASK(CROP_OUT_HSIZE), handle);
			mtk_ddp_write_mask(comp, height << 16,
				DISP_REG_SPR_RDY_SEL_EN, REG_FLD_MASK(CROP_OUT_VSIZE), handle);
		}
		mtk_ddp_write_mask(comp, crop_hoffset << 16,
				DISP_REG_SPR_CK_ON, REG_FLD_MASK(CROP_HOFFSET), handle);
		mtk_ddp_write_mask(comp, 0, DISP_REG_SPR_OPTION,
			DISP_SPR_AUTO_CLOCK, handle);
	} else {
		postalign_width = cfg->w;
		width = cfg->w;
		if (!spr->set_partial_update) {
			height = cfg->h;
			crop_voffset = 0;
			out_height = cfg->h;
		} else {
			overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : spr->tile_overhead_v.overhead_v;
			comp_overhead_v = (!overhead_v) ? 0 : spr->tile_overhead_v.comp_overhead_v;
			height = spr->roi_height + overhead_v * 2;
			out_height = spr->roi_height + (overhead_v - comp_overhead_v) * 2;
			crop_voffset = comp_overhead_v;
		}
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_mask(comp, width << 0,
				MT6989_DISP_REG_SPR_CROP_SIZE, REG_FLD_MASK(MT6989_CROP_OUT_HSIZE), handle);
			mtk_ddp_write_mask(comp, out_height << 16,
				MT6989_DISP_REG_SPR_CROP_SIZE, REG_FLD_MASK(MT6989_CROP_OUT_VSIZE), handle);
		} else {
			mtk_ddp_write_mask(comp, width << 16,
				DISP_REG_SPR_RDY_SEL, REG_FLD_MASK(CROP_OUT_HSIZE), handle);
			mtk_ddp_write_mask(comp, out_height << 16,
				DISP_REG_SPR_RDY_SEL_EN, REG_FLD_MASK(CROP_OUT_VSIZE), handle);
		}
		mtk_ddp_write_mask(comp, 0 << 16,
			DISP_REG_SPR_CK_ON, REG_FLD_MASK(CROP_HOFFSET), handle);
		mtk_ddp_write_mask(comp, crop_voffset << 24,
			DISP_REG_SPR_CK_ON, REG_FLD_MASK(CROP_VOFFSET), handle);
	}

	//roi size config
	mtk_ddp_write_relaxed(comp, height << 16 | width,
		DISP_REG_V2_SPR_ROI_SIZE, handle);

	//relay mode: roi size, crop_out_size, spr_en, spr_lut_en
	if (disp_spr_bypass) {
		mtk_ddp_write_mask(comp, SPR_EN, DISP_REG_SPR_EN,
			SPR_EN, handle);
		mtk_ddp_write_mask(comp, SPR_LUT_EN, DISP_REG_SPR_EN,
			SPR_LUT_EN, handle);
		mtk_ddp_write_mask(comp, SPR_RELAY_MODE, DISP_REG_SPR_EN,
			SPR_RELAY_MODE, handle);


		//disable postalign
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_relaxed(postalign_comp, 1, MT6989_DISP_REG_POSTALIGN0_EN,
				handle);
			if (spr->data) {
				if (spr->data->need_bypass_shadow)
					mtk_ddp_write_mask(postalign_comp, MT6989_BYPASS_SHADOW,
							MT6989_DISP_REG_POSTALIGN0_SHADOW_CTRL,
							MT6989_BYPASS_SHADOW, handle);
				else
					mtk_ddp_write_mask(postalign_comp, 0,
							MT6989_DISP_REG_POSTALIGN0_SHADOW_CTRL,
							MT6989_BYPASS_SHADOW, handle);
			}
			mtk_ddp_write_mask(postalign_comp, MT6989_RELAY_MODE,
				MT6989_DISP_REG_POSTALIGN0_CFG, MT6989_RELAY_MODE, handle);
			mtk_ddp_write_mask(postalign_comp, MT6989_POSTALIGN_LUT_EN,
				MT6989_DISP_REG_POSTALIGN0_CFG, MT6989_POSTALIGN_LUT_EN, handle);
			mtk_ddp_write_mask(postalign_comp, height << 0,
				MT6989_DISP_REG_POSTALIGN0_SIZE,
				REG_FLD_MASK(MT6989_VSIZE), handle);
			mtk_ddp_write_mask(postalign_comp, postalign_width << 16,
				MT6989_DISP_REG_POSTALIGN0_SIZE,
				REG_FLD_MASK(MT6989_HSIZE), handle);
		} else {
			if (handle)
				cmdq_pkt_write(handle, comp->cmdq_base,
					config_regs_pa + DISP_REG_POSTALIGN0_CON0, 0, ~0);
			else
				writel_relaxed(0, config_regs + DISP_REG_POSTALIGN0_CON0);
		}

		return;
	}

	if (spr_params->enable == 1 && spr_params->relay == 0 && comp->mtk_crtc->spr_is_on == 1) {
		//postalign config
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_relaxed(postalign_comp, 1, MT6989_DISP_REG_POSTALIGN0_EN,
				handle);
			if (spr->data) {
				if (spr->data->need_bypass_shadow)
					mtk_ddp_write_mask(postalign_comp, MT6989_BYPASS_SHADOW,
							MT6989_DISP_REG_POSTALIGN0_SHADOW_CTRL,
							MT6989_BYPASS_SHADOW, handle);
				else
					mtk_ddp_write_mask(postalign_comp, 0,
							MT6989_DISP_REG_POSTALIGN0_SHADOW_CTRL,
							MT6989_BYPASS_SHADOW, handle);
			}
			if (spr_params->postalign_en == 1) {
				mtk_ddp_write_mask(postalign_comp,
					spr_params->postalign_en << spr_params->spr_format_type,
					MT6989_DISP_REG_POSTALIGN0_CFG,
					REG_FLD_MASK(MT6989_POSTALIGN_SEL), handle);
				mtk_ddp_write_mask(postalign_comp,
					spr_params->postalign_6type_mode_en << 8,
					MT6989_DISP_REG_POSTALIGN0_CFG,
					MT6989_POSTALIGN_6TYPE_MODE, handle);
				mtk_ddp_write_mask(postalign_comp, spr_params->padding_repeat_en << 12,
					MT6989_DISP_REG_POSTALIGN0_CFG,
					MT6989_DSC_PADDING_REPEAT_EN, handle);
				mtk_ddp_write_mask(postalign_comp, 0,
					MT6989_DISP_REG_POSTALIGN0_CFG, MT6989_RELAY_MODE, handle);
			} else {
				mtk_ddp_write_mask(postalign_comp, MT6989_RELAY_MODE,
					MT6989_DISP_REG_POSTALIGN0_CFG, MT6989_RELAY_MODE, handle);
			}
			mtk_ddp_write_mask(postalign_comp, MT6989_POSTALIGN_LUT_EN,
				MT6989_DISP_REG_POSTALIGN0_CFG, MT6989_POSTALIGN_LUT_EN, handle);

			if (!spr->set_partial_update)
				mtk_ddp_write_mask(postalign_comp, out_height << 0,
					MT6989_DISP_REG_POSTALIGN0_SIZE,
					REG_FLD_MASK(MT6989_VSIZE), handle);
			else
				mtk_ddp_write_mask(postalign_comp, out_height << 0,
					MT6989_DISP_REG_POSTALIGN0_SIZE,
					REG_FLD_MASK(MT6989_VSIZE), handle);
			mtk_ddp_write_mask(postalign_comp, postalign_width << 16,
				MT6989_DISP_REG_POSTALIGN0_SIZE,
				REG_FLD_MASK(MT6989_HSIZE), handle);
		} else {
			reg_val = (!!spr_params->postalign_6type_mode_en << 31) |
				(!!spr_params->padding_repeat_en << 28) |
				(!!spr_params->postalign_en << (20 + spr_params->spr_format_type)) |
				(!!spr_params->postalign_en << 16) | 1;

			if (handle) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					config_regs_pa + DISP_REG_POSTALIGN0_CON0,
					reg_val, ~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					config_regs_pa + DISP_REG_POSTALIGN0_CON1,
					(height << 16 | postalign_width), ~0);
			} else {
				writel_relaxed(reg_val,
					config_regs + DISP_REG_POSTALIGN0_CON0);
				writel_relaxed((height << 16 | postalign_width),
					config_regs + DISP_REG_POSTALIGN0_CON1);
			}
		}

		switch (spr_params->spr_format_type) {
		case MTK_PANEL_RGBG_BGRG_TYPE:
			reg_val = 0x00050502;
			break;
		case MTK_PANEL_BGRG_RGBG_TYPE:
			reg_val = 0x00500052;
			break;
		case MTK_PANEL_RGBRGB_BGRBGR_TYPE:
			reg_val = 0x03154203;
			break;
		case MTK_PANEL_BGRBGR_RGBRGB_TYPE:
			reg_val = 0x04203153;
			break;
		case MTK_PANEL_RGBRGB_BRGBRG_TYPE:
			reg_val = 0x04200423;
			break;
		case MTK_PANEL_BRGBRG_RGBRGB_TYPE:
			reg_val = 0x00424203;
			break;
		default:
			reg_val = 0x03154203;
			break;
		}

		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_relaxed(postalign_comp, reg_val,
				MT6989_DISP_REG_POSTALIGN0_ARRANGE, handle);
		} else {
			if (handle)
				cmdq_pkt_write(handle, comp->cmdq_base,
					config_regs_pa + DISP_REG_POSTALIGN0_CON2,
					reg_val, ~0);
			else
				writel_relaxed(reg_val,
					config_regs + DISP_REG_POSTALIGN0_CON2);
		}

		mtk_ddp_write_relaxed(comp, height << 12 | width,
		DISP_REG_V2_SPR_IP_CFG_0, handle);

		reg_val = (!!spr_params->bypass_dither << 5) |
			(!!spr_params->rgb_swap << 4) |
			(!!spr_params->outdata_res_sel << 2) |
			(!!spr_params->indata_res_sel << 1) | 1;
		mtk_ddp_write_relaxed(comp, reg_val,
			DISP_REG_SPR_CFG, handle);

		if (spr_params->bypass_dither == 0) {
			mtk_ddp_write_mask(comp, LFSR_EN, DISP_REG_SPR_DITHER_6,
				LFSR_EN, handle);
			mtk_ddp_write_mask(comp, RDITHER_EN, DISP_REG_SPR_DITHER_6,
				RDITHER_EN, handle);
		}

		mtk_ddp_write_mask(comp, SPR_EN, DISP_REG_SPR_EN,
			SPR_EN, handle);
		mtk_ddp_write_mask(comp, SPR_LUT_EN, DISP_REG_SPR_EN,
			SPR_LUT_EN, handle);
		mtk_ddp_write_mask(comp, 0, DISP_REG_SPR_EN,
			SPR_RELAY_MODE, handle);
		mtk_ddp_write_mask(comp, SPR_FORCE_COMMIT, DISP_REG_SPR_EN,
			SPR_FORCE_COMMIT, handle);
		mtk_ddp_write_mask(comp, SPR_BYPASS_SHADOW, DISP_REG_SPR_EN,
			SPR_BYPASS_SHADOW, handle);
	} else {
		mtk_ddp_write_mask(comp, SPR_EN, DISP_REG_SPR_EN,
			SPR_EN, handle);
		mtk_ddp_write_mask(comp, SPR_LUT_EN, DISP_REG_SPR_EN,
			SPR_LUT_EN, handle);
		mtk_ddp_write_mask(comp, SPR_RELAY_MODE, DISP_REG_SPR_EN,
			SPR_RELAY_MODE, handle);

		//disable postalign
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_relaxed(postalign_comp, 1, MT6989_DISP_REG_POSTALIGN0_EN,
				handle);
			if (spr->data) {
				if (spr->data->need_bypass_shadow)
					mtk_ddp_write_mask(postalign_comp, MT6989_BYPASS_SHADOW,
							MT6989_DISP_REG_POSTALIGN0_SHADOW_CTRL,
							MT6989_BYPASS_SHADOW, handle);
				else
					mtk_ddp_write_mask(postalign_comp, 0,
							MT6989_DISP_REG_POSTALIGN0_SHADOW_CTRL,
							MT6989_BYPASS_SHADOW, handle);
			}
			mtk_ddp_write_mask(postalign_comp, MT6989_RELAY_MODE,
				MT6989_DISP_REG_POSTALIGN0_CFG, MT6989_RELAY_MODE, handle);
			mtk_ddp_write_mask(postalign_comp, MT6989_POSTALIGN_LUT_EN,
				MT6989_DISP_REG_POSTALIGN0_CFG, MT6989_POSTALIGN_LUT_EN, handle);
			mtk_ddp_write_mask(postalign_comp, height << 0,
				MT6989_DISP_REG_POSTALIGN0_SIZE,
				REG_FLD_MASK(MT6989_VSIZE), handle);
			mtk_ddp_write_mask(postalign_comp, postalign_width << 16,
				MT6989_DISP_REG_POSTALIGN0_SIZE,
				REG_FLD_MASK(MT6989_HSIZE), handle);
		} else {
			if (handle)
				cmdq_pkt_write(handle, comp->cmdq_base,
					config_regs_pa + DISP_REG_POSTALIGN0_CON0,
					0, ~0);
			else
				writel_relaxed(0,
					config_regs + DISP_REG_POSTALIGN0_CON0);
		}
	}
}

static void mtk_spr_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_spr *spr = comp_to_spr(comp);

	if (spr->data && spr->data->version == MTK_SPR_V2)
		mtk_spr_config_V2(comp, cfg, handle);
	else
		mtk_spr_config_V1(comp, cfg, handle);
}

static void mtk_spr_first_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	struct mtk_disp_spr *spr = comp_to_spr(comp);

	if (spr->data && spr->data->version == MTK_SPR_V2)
		mtk_spr_config_V2(comp, cfg, handle);
}


void mtk_spr_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned int i, num;
	struct mtk_disp_spr *spr = comp_to_spr(comp);

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	if (spr->data && spr->data->version == MTK_SPR_V2)
		num = 0x50;
	else
		num = 0x350;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	for (i = 0; i < num; i += 16) {
		DDPDUMP("0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
			readl(baddr + i), readl(baddr + i + 0x4),
			readl(baddr + i + 0x8), readl(baddr + i + 0xc));
	}

	if (spr->data && spr->data->version == MTK_SPR_V2) {
		for (i = 0xa0; i < 0xd0; i += 16) {
			DDPDUMP("0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n", i,
				readl(baddr + i), readl(baddr + i + 0x4),
				readl(baddr + i + 0x8), readl(baddr + i + 0xc));
		}
		DDPDUMP("0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n", 0xd70,
			readl(baddr + 0xd70), readl(baddr + 0xd70 + 0x4),
			readl(baddr + 0xd70 + 0x8), readl(baddr + 0xd70 + 0xc));
	}
}

int mtk_spr_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== %s ANALYSIS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("en=%d, spr_bypass=%d\n",
		DISP_REG_GET_FIELD(CON_FLD_SPR_EN,
			baddr + DISP_REG_SPR_EN),
		DISP_REG_GET_FIELD(CON_FLD_DISP_SPR_RELAY_MODE,
			baddr + DISP_REG_SPR_EN));

	return 0;
}

void mtk_cal_spr_valid_partial_roi(struct mtk_drm_crtc *crtc,
				struct mtk_rect *partial_roi)
{
	struct mtk_drm_private *priv = crtc->base.dev->dev_private;
	struct mtk_ddp_comp *spr0_comp = priv->ddp_comp[DDP_COMPONENT_SPR0];
	struct mtk_panel_dsc_params *dsc_params;
	struct mtk_panel_spr_params *spr_params;
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&spr0_comp->mtk_crtc->base, spr0_comp, true);
	unsigned int slice_height = 8;
	int slice_diff, line_diff;

	dsc_params = &spr0_comp->mtk_crtc->panel_ext->params->dsc_params;
	spr_params = &spr0_comp->mtk_crtc->panel_ext->params->spr_params;
	if (spr_params->enable == 1 && spr_params->relay == 0 && spr0_comp->mtk_crtc->spr_is_on == 1)
		dsc_params = &spr0_comp->mtk_crtc->panel_ext->params->dsc_params_spr_in;
	if (dsc_params->enable == 1)
		slice_height = dsc_params->slice_height;

	DDPDBG("%s, calculate spr valid size\n", __func__);
	slice_diff = 0;

	/* align to slice_height*/
	if (partial_roi->y % slice_height != 0) {
		slice_diff =
			partial_roi->y - (partial_roi->y / slice_height) * slice_height;
		partial_roi->y -= slice_diff;
	}

	partial_roi->height += slice_diff;
	if (partial_roi->height % slice_height != 0) {
		partial_roi->height =
			((partial_roi->height / slice_height) + 1) * slice_height;

		if (partial_roi->height > full_height)
			partial_roi->height = full_height;
	}

	/* spr roi size must be greater than 120 lines*/
	if (partial_roi->height < 120) { //extension upwards first
		line_diff = (partial_roi->y + partial_roi->height - full_height / 2) -
			(full_height / 2 - partial_roi->y);
		if (partial_roi->y > full_height / 2) { //fill in the diff & average the remain line to 120
			partial_roi->y -= (120 - partial_roi->height);
		} else if (line_diff > 0) {
			partial_roi->y -= (line_diff + (partial_roi->height - line_diff)/2);
		}
		partial_roi->height = 120;
	}
}

static int mtk_spr_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_rect *partial_roi = NULL;

	switch (cmd) {
	case GET_VALID_PARTIAL_ROI:
	{
		partial_roi = (struct mtk_rect *)params;
		mtk_cal_spr_valid_partial_roi(mtk_crtc, partial_roi);
	}
		break;
	default:
		break;
	}
	return 0;
}


static int mtk_spr_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, bool enable)
{
	resource_size_t config_regs_pa;
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;
	struct mtk_ddp_comp *postalign_comp = priv->ddp_comp[DDP_COMPONENT_POSTALIGN0];
	struct mtk_disp_spr *spr = comp_to_spr(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
				&comp->mtk_crtc->base, comp, true);
	unsigned int crop_voffset = 0;
	unsigned int crop_height;
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
		__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	/* spr crop offset set*/
	spr->set_partial_update = enable;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
			? 0 : spr->tile_overhead_v.overhead_v;
	comp_overhead_v = (!overhead_v) ? 0 : spr->tile_overhead_v.comp_overhead_v;
	spr->roi_height = partial_roi.height;
	crop_voffset = comp_overhead_v;
	crop_height = spr->roi_height + (overhead_v - comp_overhead_v) * 2;

	DDPDBG("%s, %s total overhead_v:%d, spr overhead_v:%d\n",
		__func__, mtk_dump_comp_str(comp), overhead_v, comp_overhead_v);

	// SPR & Postallign reg config
	if (comp->id == DDP_COMPONENT_SPR0) {
		if (priv->data->mmsys_id == MMSYS_MT6989)
			postalign_comp = priv->ddp_comp[DDP_COMPONENT_POSTALIGN0];
		else
			config_regs_pa = comp->mtk_crtc->config_regs_pa;
	} else
		config_regs_pa = comp->mtk_crtc->side_config_regs_pa;

	if (spr->set_partial_update) {
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_mask(comp, crop_height << 16,
				MT6989_DISP_REG_SPR_CROP_SIZE,
				REG_FLD_MASK(MT6989_CROP_OUT_VSIZE), handle);

			mtk_ddp_write_mask(postalign_comp, crop_height << 0,
				MT6989_DISP_REG_POSTALIGN0_SIZE,
				REG_FLD_MASK(MT6989_VSIZE), handle);
		} else {
			mtk_ddp_write_mask(comp, crop_height << 16,
				DISP_REG_SPR_RDY_SEL_EN,
				REG_FLD_MASK(CROP_OUT_VSIZE), handle);

			cmdq_pkt_write(handle, comp->cmdq_base,
				config_regs_pa + DISP_REG_POSTALIGN0_CON1,
				crop_height << 16, 0xffff0000);
		}

		//roi size config
		mtk_ddp_write_mask(comp,
			(spr->roi_height + overhead_v * 2) << 16,
			DISP_REG_V2_SPR_ROI_SIZE, REG_FLD_MASK(CROP_OUT_VSIZE), handle);

		mtk_ddp_write_mask(comp,
			(spr->roi_height + overhead_v * 2) << 12,
			DISP_REG_V2_SPR_IP_CFG_0, 0xfffff000, handle);

		mtk_ddp_write_mask(comp, 1 << 28,
			DISP_REG_V2_SPR_IP_CFG_18, 0xf0000000, handle); //enable SPR IP border

		mtk_ddp_write_mask(comp, crop_voffset << 24,
			DISP_REG_SPR_CK_ON, 0xff000000, handle);
	} else {
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			mtk_ddp_write_mask(comp, full_height << 16,
				MT6989_DISP_REG_SPR_CROP_SIZE,
				REG_FLD_MASK(MT6989_CROP_OUT_VSIZE), handle);

			mtk_ddp_write_mask(postalign_comp, full_height << 0,
				MT6989_DISP_REG_POSTALIGN0_SIZE,
				REG_FLD_MASK(MT6989_VSIZE), handle);
		} else {
			mtk_ddp_write_mask(comp, full_height << 16,
				DISP_REG_SPR_RDY_SEL_EN,
				REG_FLD_MASK(CROP_OUT_VSIZE), handle);

			cmdq_pkt_write(handle, comp->cmdq_base,
				config_regs_pa + DISP_REG_POSTALIGN0_CON1,
				full_height << 16, 0xffff0000);
		}

		//roi size config
		mtk_ddp_write_mask(comp, full_height << 16,
			DISP_REG_V2_SPR_ROI_SIZE, REG_FLD_MASK(CROP_OUT_VSIZE), handle);

		mtk_ddp_write_mask(comp, full_height << 12,
		DISP_REG_V2_SPR_IP_CFG_0, 0xfffff000, handle);

		mtk_ddp_write_mask(comp, 0 << 24,
			DISP_REG_SPR_CK_ON, 0xff000000, handle);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_spr_funcs = {
	.first_cfg = mtk_spr_first_config,
	.config = mtk_spr_config,
	.start = mtk_spr_start,
	.stop = mtk_spr_stop,
	.prepare = mtk_spr_prepare,
	.unprepare = mtk_spr_unprepare,
	.config_overhead = mtk_disp_spr_config_overhead,
	.config_overhead_v = mtk_disp_spr_config_overhead_v,
	.partial_update = mtk_spr_set_partial_update,
	.io_cmd = mtk_spr_io_cmd,
};

static int mtk_disp_spr_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_spr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_spr_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_spr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_spr_component_ops = {
	.bind = mtk_disp_spr_bind,
	.unbind = mtk_disp_spr_unbind,
};

static int mtk_disp_spr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_spr *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		DDPMSG("%s+ irq error\n", __func__);
		return irq;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_SPR);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_spr_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_spr_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);
	return ret;
}

static int mtk_disp_spr_remove(struct platform_device *pdev)
{
	struct mtk_disp_spr *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_spr_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_spr_data mt6853_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V1,
	.shrink_cfg = false,
};

static const struct mtk_disp_spr_data mt6983_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V1,
	.shrink_cfg = false,
};

static const struct mtk_disp_spr_data mt6985_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V2,
	.shrink_cfg = false,
};

static const struct mtk_disp_spr_data mt6989_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V2,
	.shrink_cfg = false,
};

static const struct mtk_disp_spr_data mt6897_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V2,
	.shrink_cfg = true,
};

static const struct mtk_disp_spr_data mt6895_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V1,
	.shrink_cfg = false,
};

static const struct mtk_disp_spr_data mt6886_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V1,
	.shrink_cfg = false,
};

static const struct mtk_disp_spr_data mt6879_spr_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.version = MTK_SPR_V1,
	.shrink_cfg = false,
};

static const struct of_device_id mtk_disp_spr_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6853-disp-spr",
	  .data = &mt6853_spr_driver_data},
	{ .compatible = "mediatek,mt6983-disp-spr",
	  .data = &mt6983_spr_driver_data},
	{ .compatible = "mediatek,mt6985-disp-spr",
	  .data = &mt6985_spr_driver_data},
	{ .compatible = "mediatek,mt6989-disp-spr",
	  .data = &mt6989_spr_driver_data},
	{ .compatible = "mediatek,mt6897-disp-spr",
	  .data = &mt6897_spr_driver_data},
	{ .compatible = "mediatek,mt6895-disp-spr",
	  .data = &mt6895_spr_driver_data},
	{ .compatible = "mediatek,mt6886-disp-spr",
	  .data = &mt6886_spr_driver_data},
	{ .compatible = "mediatek,mt6879-disp-spr",
	  .data = &mt6879_spr_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_spr_driver_dt_match);

struct platform_driver mtk_disp_spr_driver = {
	.probe = mtk_disp_spr_probe,
	.remove = mtk_disp_spr_remove,
	.driver = {
		.name = "mediatek-disp-spr",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_spr_driver_dt_match,
	},
};
