// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_drm_drv.h"
#include "../mtk_drm_lowpower.h"
#include "../mtk_log.h"
#include "../mtk_dump.h"
#include "../mtk_drm_mmp.h"
#include "../mtk_drm_gem.h"
#include "../mtk_drm_fb.h"
#include "mtk_disp_oddmr.h"
#include "mtk_disp_oddmr_parse_data.h"
#include "mtk_disp_oddmr_tuning.h"

/* ODDMR TOP */
#define DISP_ODDMR_TOP_CTR_1 0x0004
#define DISP_ODDMR_TOP_CTR_2 0x0008
#define REG_VS_RE_GEN REG_FLD_MSB_LSB(1, 1)
#define DISP_ODDMR_TOP_CTR_3 0x000C
#define REG_ODDMR_TOP_CLK_FORCE_EN REG_FLD_MSB_LSB(15, 15)
#define REG_FORCE_COMMIT REG_FLD_MSB_LSB(4, 4)
#define REG_BYPASS_SHADOW REG_FLD_MSB_LSB(5, 5)
#define DISP_ODDMR_TOP_CTR_4 0x001C//rst
#define REG_DMR_SMI_BRG_SW_RST REG_FLD_MSB_LSB(2, 2)
#define REG_ODR_SMI_BRG_SW_RST REG_FLD_MSB_LSB(3, 3)
#define REG_ODW_SMI_BRG_SW_RST REG_FLD_MSB_LSB(4, 4)
#define DISP_ODDMR_IRQ_FORCE 0x0020
#define DISP_ODDMR_IRQ_CLEAR 0x0024
#define DISP_ODDMR_IRQ_MASK 0x0028
#define DISP_ODDMR_IRQ_STATUS 0x002C
#define DISP_ODDMR_IRQ_FRAME_DONE BIT(0)
#define DISP_ODDMR_IRQ_ODW_DONE BIT(1)
#define DISP_ODDMR_IRQ_I_FRM_DNE BIT(2)
#define DISP_ODDMR_IRQ_O_FRM_DNE BIT(3)
#define DISP_ODDMR_IRQ_SOF BIT(5)
#define DISP_ODDMR_IRQ_RAW_STATUS 0x0030
#define DISP_ODDMR_DITHER_CTR_1 0x0044//11bit
#define DISP_ODDMR_DITHER_CTR_2 0x0048//10bit
#define DISP_ODDMR_DMR_UDMA_CTR_4 0x00D0
#define REG_DMR_BASE_ADDR_L REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_DMR_UDMA_CTR_5 0x00D4
#define REG_DMR_BASE_ADDR_H REG_FLD_MSB_LSB(15, 0)

#define DISP_ODDMR_DMR_UDMA_CTR_6 0x00D8
#define DISP_ODDMR_DMR_UDMA_CTR_7 0x00DC


#define DISP_ODDMR_OD_UDMA_CTR_0 0x0100
#define REG_OD_CLK_EN REG_FLD_MSB_LSB(11, 11)

#define DISP_ODDMR_FMT_CTR_0 0x0140
#define DISP_ODDMR_FMT_CTR_1 0x0144


#define DISP_ODDMR_OD_HSK_0 0x0180
#define REG_HS2DE_VDE_H_SIZE REG_FLD_MSB_LSB(13, 0)
#define DISP_ODDMR_OD_HSK_1 0x0184
#define REG_HS2DE_VDE_V_SIZE REG_FLD_MSB_LSB(12, 0)
#define DISP_ODDMR_OD_HSK_2 0x0188
#define DISP_ODDMR_OD_HSK_3 0x018C
#define DISP_ODDMR_OD_HSK_4 0x0190
#define DISP_ODDMR_CRP_CTR_0 0x0194
#define REG_CROP_VDE_H_SIZE REG_FLD_MSB_LSB(13, 0)
#define DISP_ODDMR_CRP_CTR_1 0x0198
#define REG_CROP_VDE_V_SIZE REG_FLD_MSB_LSB(12, 0)
#define DISP_ODDMR_CRP_CTR_2 0x019C
#define REG_WINDOW_LR REG_FLD_MSB_LSB(0, 0)
#define REG_GUARD_BAND_PIXEL REG_FLD_MSB_LSB(14, 8)
#define DISP_ODDMR_TOP_RESERVE_1 0x01A4
#define DISP_ODDMR_ODR_H DISP_ODDMR_TOP_RESERVE_1
#define DISP_ODDMR_TOP_RESERVE_2 0x01A8
#define DISP_ODDMR_ODR_V DISP_ODDMR_TOP_RESERVE_2
#define DISP_ODDMR_TOP_RESERVE_3 0x01AC
#define DISP_ODDMR_TOP_OD_BYASS 0x01C0
#define DISP_ODDMR_TOP_DMR_BYASS 0x01C4
#define DISP_ODDMR_TOP_S2R_BYPASS 0x01C8
#define DISP_ODDMR_TOP_HRT0_BYPASS 0x01CC
#define DISP_ODDMR_TOP_HRT1_BYPASS 0x01D0
#define DISP_ODDMR_TOP_DITHER_BYPASS 0x01D4
#define DISP_ODDMR_TOP_CRP_BYPSS 0x01D8
#define DISP_ODDMR_TOP_CLK_GATING 0x01E0
#define DISP_ODDMR_DMR_UDMA_ENABLE 0x01E4
#define DISP_ODDMR_DITHER_12TO11_BYPASS 0x01E8
#define DISP_ODDMR_DITHER_12TO10_BYPASS 0x01EC
#define DISP_ODDMR_DMR_SECURITY 0x01FC

/* DMR */
#define DISP_ODDMR_REG_DMR_EN 0x020c
#define DISP_ODDMR_REG_DMR_CLK_EN 0x0f00

/* OD */
#define DISP_ODDMR_REG_OD_BASE 0x1000
#define DISP_ODDMR_OD_SRAM_CTRL_0 (0x0004 + DISP_ODDMR_REG_OD_BASE)
#define REG_W_OD_SRAM_IO_EN REG_FLD_MSB_LSB(0, 0)
#define REG_B_OD_SRAM_IO_EN REG_FLD_MSB_LSB(1, 1)
#define REG_G_OD_SRAM_IO_EN REG_FLD_MSB_LSB(2, 2)
#define REG_R_OD_SRAM_IO_EN REG_FLD_MSB_LSB(3, 3)
#define REG_WBGR_OD_SRAM_IO_EN REG_FLD_MSB_LSB(3, 0)
#define REG_OD_SRAM_WRITE_SEL REG_FLD_MSB_LSB(4, 4)
#define REG_OD_SRAM_READ_SEL REG_FLD_MSB_LSB(5, 5)
#define REG_AUTO_SRAM_ADR_INC_EN REG_FLD_MSB_LSB(11, 11)
//#define REG_SRAM_TABLE_EN REG_FLD_MSB_LSB(15, 12)

#define DISP_ODDMR_OD_SRAM_CTRL_1 (0x0008 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM1_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM1_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_2 (0x000C + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM1_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_3 (0x0010 + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_4 (0x0014 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM2_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM2_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_5 (0x0018 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM2_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_6 (0x001C + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_7 (0x0020 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM3_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM3_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_8 (0x0024 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM3_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_9 (0x0028 + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_10 (0x002C + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM4_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM4_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_11 (0x0030 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM4_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_12 (0x0034 + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_13 (0x0038 + DISP_ODDMR_REG_OD_BASE)
#define DISP_ODDMR_OD_CTRL_EN (0x0040 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_EN REG_FLD_MSB_LSB(0, 0)
#define REG_OD_MODE REG_FLD_MSB_LSB(3, 1)
#define DISP_ODDMR_OD_PQ_0 (0x0044 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_USER_WEIGHT REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_PQ_1 (0x0048 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_ACT_THRD REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_BASE_ADDR_R_LSB (0x0054 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_R_LSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_R_MSB (0x0058 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_R_MSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_G_LSB (0x005C + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_G_LSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_G_MSB (0x0060 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_G_MSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_B_LSB (0x0064 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_B_LSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_B_MSB (0x0068 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_B_MSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_SW_RESET (0x0094 + DISP_ODDMR_REG_OD_BASE)
#define DISP_ODDMR_OD_UMDA_CTRL_0 (0x00B0 + DISP_ODDMR_REG_OD_BASE)//all 0
#define DISP_ODDMR_OD_UMDA_CTRL_1 (0x00B4 + DISP_ODDMR_REG_OD_BASE)
#define REG_LN_OFFSET REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_UMDA_CTRL_2 (0x00B8 + DISP_ODDMR_REG_OD_BASE)
#define REG_HSIZE REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_UMDA_CTRL_3 (0x00BC + DISP_ODDMR_REG_OD_BASE)
#define REG_VSIZE REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_SCALING_6 (0x01B0 + DISP_ODDMR_REG_OD_BASE)
#define REG_ENABLE_HSCALING REG_FLD_MSB_LSB(0, 0)
#define REG_ENABLE_VSCALING REG_FLD_MSB_LSB(1, 1)
#define REG_DE_ALIGN8_EN REG_FLD_MSB_LSB(6, 6)
#define REG_HSD_2X4X_SEL REG_FLD_MSB_LSB(7, 7)
#define DISP_ODDMR_OD_DUMMY_0 (0x01C4 + DISP_ODDMR_REG_OD_BASE)
#define REG_CUR_LINE_BUFFER_DE_2P_ALIGN REG_FLD_MSB_LSB(0, 0)
#define REG_UDMA_HSIZE_USER_MODE REG_FLD_MSB_LSB(1, 1)
#define REG_UDMA_VSIZE_USER_MODE REG_FLD_MSB_LSB(2, 2)
#define REG_HVSP2UDMA_W_LAST_MASK REG_FLD_MSB_LSB(3, 3)
#define REG_BUF_LV_INVERSE REG_FLD_MSB_LSB(4, 4)
#define REG_BYPASS_MIU2SMI_W_BRIDGE REG_FLD_MSB_LSB(7, 7)
#define DISP_ODDMR_OD_DUMMY_1 (0x01C8 + DISP_ODDMR_REG_OD_BASE)
#define DISP_ODDMR_OD_DUMMY_2 (0x01CC + DISP_ODDMR_REG_OD_BASE)
#define DISP_ODDMR_OD_DUMMY_3 (0x01D0 + DISP_ODDMR_REG_OD_BASE)


/* SPR2RGB */
#define DISP_ODDMR_REG_SPR2RGB_BASE 0x800
#define DISP_ODDMR_REG_SPR_COMP_EN (0x0004 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_0 (0x0008 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_1 (0x000C + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_2 (0x0010 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_3 (0x0014 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_4 (0x0018 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_5 (0x001C + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_2X2 (0x0020 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_PANEL_WIDTH (0x0024 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_X_INIT (0x0028 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_REMAP_EN (0x002C + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_REMAP_GAIN (0x0030 + DISP_ODDMR_REG_SPR2RGB_BASE)



/* SMI SB */
#define DISP_ODDMR_REG_SMI_BASE 0x000
#define DISP_ODDMR_HRT_CTR (0x003C + DISP_ODDMR_REG_SMI_BASE)
#define REG_HR0_RE_ULTRA_MODE REG_FLD_MSB_LSB(3, 0)
#define REG_HR0_POACH_CFG_OFF REG_FLD_MSB_LSB(4, 4)
#define REG_HR0_PREULTRA_RE_ULTRA_MASK REG_FLD_MSB_LSB(6, 6)
#define REG_HR0_ULTRA_RE_MASK REG_FLD_MSB_LSB(7, 7)
#define REG_HR1_RE_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)
#define REG_HR1_POACH_CFG_OFF REG_FLD_MSB_LSB(12, 12)
#define REG_HR1_PREULTRA_RE_ULTRA_MASK REG_FLD_MSB_LSB(14, 14)
#define REG_HR1_ULTRA_RE_MASK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_IN_THR_0 (0x0040 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_IN_THR_1 (0x0044 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_OUT_THR_0 (0x0048 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_OUT_THR_1 (0x004C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_IN_THR_0 (0x0050 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_IN_THR_1 (0x0054 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_OUT_THR_0 (0x0058 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_OUT_THR_1 (0x005C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_IN_THR_0 (0x0060 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_IN_THR_1 (0x0064 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_OUT_THR_0 (0x0068 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_OUT_THR_1 (0x006C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_IN_THR_0 (0x0070 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_IN_THR_1 (0x0074 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_OUT_THR_0 (0x0078 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_OUT_THR_1 (0x007C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_0 (0x0060 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_1 (0x0074 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_2 (0x0068 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_3 (0x006C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_4 (0x0070 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_5 (0x0074 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_6 (0x0078 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_7 (0x007C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_8 (0x0054 + DISP_ODDMR_REG_SMI_BASE)
#define REG_ODR_POACH_CFG_OFF REG_FLD_MSB_LSB(1, 1)
#define REG_ODR_RE_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)
#define DISP_ODDMR_SMI_SB_FLG_ODW_0  (0x0080 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_1  (0x0084 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_2  (0x0088 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_3  (0x008C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_4  (0x0090 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_5  (0x0094 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_6  (0x0098 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_7  (0x009C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_8 (0x0058 + DISP_ODDMR_REG_SMI_BASE)
#define REG_ODW_POACH_CFG_OFF REG_FLD_MSB_LSB(1, 1)
#define REG_ODW_WR_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_0  (0x00A0 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_1  (0x00A4 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_0  (0x00A8 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_1  (0x00AC + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_0  (0x00B0 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_1  (0x00B4 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_0  (0x00B8 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_1  (0x00BC + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_DMR_8 (0x005c + DISP_ODDMR_REG_SMI_BASE)
#define REG_DMR_RE_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)

/* OD UDMA R*/
#define DISP_ODDMR_REG_UDMA_R_BASE 0x1600
#define DISP_ODDMR_UDMA_R_CTRL84_0	(0x0160 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL84_1	(0x0164 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL84_2	(0x0168 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL86_0	(0x01A0 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL86_1	(0x01A4 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL86_2	(0x01A8 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL88	(0x01C0 + DISP_ODDMR_REG_UDMA_R_BASE)//W 1
#define DISP_ODDMR_UDMA_R_CTRL_104	(0x01C8 + DISP_ODDMR_REG_UDMA_R_BASE)//R

/* OD UDMA W*/
#define DISP_ODDMR_REG_UDMA_W_BASE 0x1800
#define DISP_ODDMR_UDMA_W_CTR_16	(0x005C + DISP_ODDMR_REG_UDMA_W_BASE)//0x01
#define DISP_ODDMR_UDMA_W_CTR_23	(0x0084 + DISP_ODDMR_REG_UDMA_W_BASE)//0xFF
#define DISP_ODDMR_UDMA_W_CTR_58	(0x0160 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_59	(0x0164 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_60	(0x0168 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_74	(0x01A0 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_75	(0x01A4 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_76	(0x01A8 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_82	(0x01C0 + DISP_ODDMR_REG_UDMA_W_BASE)//W 1
#define DISP_ODDMR_UDMA_W_CTR_84	(0x01C8 + DISP_ODDMR_REG_UDMA_W_BASE)//R

#define OD_H_ALIGN_BITS 128

#define ODDMR_READ_IN_PRE_ULTRA(size)     (size * 2 / 3 + 3135)
#define ODDMR_READ_IN_ULTRA(size)         (size * 1 / 3 + 3135)
#define ODDMR_READ_OUT_PRE_ULTRA(size)    (size * 3 / 4 + 3135)
#define ODDMR_READ_OUT_ULTRA(size)        (size * 2 / 4 + 3135)
#define ODDMR_WRITE_IN_PRE_ULTRA(size)    (size * 1 / 3)
#define ODDMR_WRITE_IN_ULTRA(size)        (size * 2 / 3)
#define ODDMR_WRITE_OUT_PRE_ULTRA(size)   (size * 1 / 4)
#define ODDMR_WRITE_OUT_ULTRA(size)       (size * 2 / 4)

#define ODDMR_ENABLE_IRQ
#define ODDMR_IRQ_MASK_VAL 0x2F

/* DBI: deburn in mt6989 */
#define DISP_ODDMR_REG_DMR_DBI_EN (0x210)//bit 0
#define DISP_ODDMR_REG_DMR_DBI_OUT_CUP_EN (0x214)//bit 0
#define DISP_ODDMR_REG_DMR_DBI_DITHER_EN (0x234)//bit 0
#define DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_DBI_R (0x24C)//bit 0-15
#define DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_DBI_G (0x268)//bit 0-15
#define DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_DBI_B (0x284)//bit 0-15
#define DISP_ODDMR_REG_DMR_DBI_RANGE_SEL_R (0x2B0)//bit 0-2
#define DISP_ODDMR_REG_DMR_DBI_RANGE_SEL_G (0x2C4)//bit 0-2
#define DISP_ODDMR_REG_DMR_DBI_RANGE_SEL_B (0x2D8)//bit 0-2
/* 0 ~ 20 */
#define DISP_ODDMR_REG_DMR_GLOBAL_DBI_GAIN_R_0 (0xD00)//bit 0-11
/* 0 ~ 20 */
#define DISP_ODDMR_REG_DMR_GLOBAL_DBI_GAIN_G_0 (0xD54)//bit 0-11
/* 0 ~ 20 */
#define DISP_ODDMR_REG_DMR_GLOBAL_DBI_GAIN_B_0 (0xDA8)//bit 0-11
#define DISP_ODDMR_REG_DMR_DBI_LAYER_NUM (0xE40)//bit 0-3
#define DISP_ODDMR_REG_DMR_DBI_PIXEL_SHIFT_EN (0xED8)//bit 0
#define DISP_ODDMR_REG_DMR_DBI_OVERFLOW_R (0xF04)//bit 0
#define DISP_ODDMR_REG_DMR_DBI_OVERFLOW_G (0xF08)//bit 0
#define DISP_ODDMR_REG_DMR_DBI_OVERFLOW_B (0xF0C)//bit 0
#define DISP_ODDMR_REG_DMR_DBI_OVERFLOW_CLEAR (0xF10)//bit 0
#define DISP_ODDMR_REG_DMR_DBI_H_NUM_R (0xF4C)//bit 0-1
#define DISP_ODDMR_REG_DMR_DBI_V_NUM_R (0xF50)//bit 0-1
#define DISP_ODDMR_REG_DMR_DBI_H_NUM_G (0xF54)//bit 0-1
#define DISP_ODDMR_REG_DMR_DBI_V_NUM_G (0xF58)//bit 0-1
#define DISP_ODDMR_REG_DMR_DBI_H_NUM_B (0xF5C)//bit 0-1
#define DISP_ODDMR_REG_DMR_DBI_V_NUM_B (0xF60)//bit 0-1
#define DISP_ODDMR_REG_DMR_AREAL_DBI_INDIV (0x1F20)//bit 0
#define DISP_ODDMR_REG_DMR_AREAL_DBI_COMP_R (0x1FA8)//bit 0-7
#define DISP_ODDMR_REG_DMR_AREAL_DBI_COMP_G (0x1FAC)//bit 0-7
#define DISP_ODDMR_REG_DMR_AREAL_DBI_COMP_B (0x1FB0)//bit 0-7

/* ODDMR Reg for Partial Update */
#define DISP_ODDMR_REG_DMR_Y_INI			(0xE30)//bit 0-12
#define DISP_ODDMR_REG_Y_IDX2_INI			(0x3D0)//bit 0-12
#define DISP_ODDMR_REG_Y_REMAIN2_INI		(0x3D4)//bit 0-12
//#define DISP_ODDMR_TOP_CRP_BYPASS			(0x1DB)//bit 0

static bool debug_flow_log;
#define ODDMRFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		DDPMSG("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_flow_msg;
#define ODDMRFLOW_MSG(fmt, arg...) do { \
	if (debug_flow_msg) \
		DDPMSG("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_low_log;
#define ODDMRLOW_LOG(fmt, arg...) do { \
	if (debug_low_log) \
		DDPMSG("[LOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define ODDMRAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		DDPMSG("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

//TODO split dmr table in sw for dynamic overhead
static bool is_oddmr_od_support;
static bool is_oddmr_dmr_support;
static bool is_oddmr_dbi_support;
static bool g_oddmr_ddren = true;
static bool g_oddmr_hrt_en = true;
static bool g_oddmr_dump_en;
static uint32_t g_od_udma_merge_lines;
#define MAX_LONG_BURST_SIZE 16
#define DDR_RATE_800
#ifdef DDR_RATE_800
static uint32_t g_od_udma_effi[MAX_LONG_BURST_SIZE] = {
	3436, 6946, 6327, 8883,
	7001, 9045, 7357, 9013,
	7780, 8963, 7919, 8682,
	8095, 8803, 7821, 8611
};
#endif
#ifdef DDR_RATE_1600
static uint32_t g_od_udma_effi[MAX_LONG_BURST_SIZE] = {
	3183, 6356, 6365, 8933,
	7525, 9186, 8224, 9491,
	8390, 9578, 8697, 9521,
	8761, 9471, 8825, 9613
};
#endif
static uint32_t g_od_udma_merge_lines_cand[] = {
	1, 2, 4, 6, 8, 10, 12, 14, 16,
};

static struct mtk_disp_oddmr_parital_data_v dbi_part_data;

#define MIN(a,b) (((a)<(b))?(a):(b))


static unsigned char lookup[16] = {
	0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
	0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

/*
 * od_weight_trigger is used to trigger od set pq
 * is used in resume, res switch flow frame 2
 * frame 1: od on weight = 0 (weight_trigger == 1)
 * frame 2: od on weight != 0 (weight_trigger == 0)
 */
static atomic_t g_oddmr_od_weight_trigger = ATOMIC_INIT(0);
static atomic_t g_oddmr_frame_dirty = ATOMIC_INIT(0);
static atomic_t g_oddmr_pq_dirty = ATOMIC_INIT(0);
static atomic_t g_oddmr_sof_irq_available = ATOMIC_INIT(0);
/* 2: need oddmr hrt, 1: oddmr hrt done, 0:nothing todo */
static atomic_t g_oddmr_dmr_hrt_done = ATOMIC_INIT(0);
static atomic_t g_oddmr_dbi_hrt_done = ATOMIC_INIT(0);
static atomic_t g_oddmr_od_hrt_done = ATOMIC_INIT(0);

/* 0: instant trigger, 1: delay trigger 2: no trigger*/
static uint32_t g_od_check_trigger = 1;
/*
 * od err: bit(0-3)
 * dmr err: bit(4-7)
 */
#define ODDMR_OD_UDMA_W_ERR 0x01
#define ODDMR_OD_UDMA_R_ERR 0x02
static unsigned int oddmr_err_trigger;
static unsigned int oddmr_err_trigger_mask = 0xFF;

// It's a work around for no comp assigned in functions.
struct mtk_ddp_comp *default_comp;
struct mtk_ddp_comp *oddmr1_default_comp;
struct mtk_disp_oddmr *g_oddmr_priv;
struct mtk_disp_oddmr *g_oddmr1_priv;
struct mtk_oddmr_timing g_oddmr_current_timing;
static struct mtk_oddmr_panelid g_panelid;

static struct task_struct *oddmr_sof_irq_event_task;
static DECLARE_WAIT_QUEUE_HEAD(g_oddmr_sof_irq_wq);
static DECLARE_WAIT_QUEUE_HEAD(g_oddmr_hrt_wq);
static DEFINE_SPINLOCK(g_oddmr_clock_lock);
static DEFINE_SPINLOCK(g_oddmr_timing_lock);
static DEFINE_MUTEX(g_dbi_data_lock);

static void mtk_oddmr_od_hsk_force_clk(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_od_smi(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_od_set_dram(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_od_set_dram(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_set_od_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle);
static void mtk_oddmr_set_od_enable_dual(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle);
static uint32_t mtk_oddmr_od_get_dram_size(struct mtk_ddp_comp *comp, uint32_t width,
		uint32_t height, uint32_t scaling_mode, uint32_t od_mode, uint32_t channel);
static uint32_t mtk_oddmr_od_get_data_size(uint32_t width, uint32_t height,
		uint32_t scaling_mode, uint32_t od_mode, uint32_t channel);
static int mtk_oddmr_od_get_bpc(uint32_t od_mode, uint32_t channel);
static uint32_t mtk_oddmr_od_get_ln_offset(struct mtk_ddp_comp *comp, uint32_t width,
		uint32_t height, uint32_t scaling_mode, uint32_t od_mode, uint32_t channel);
static void mtk_oddmr_od_dummy(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);

static void mtk_oddmr_dmr_gain_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, unsigned int dbv_node, unsigned int fps_node,
		struct mtk_drm_dmr_cfg_info *cfg_info);

static void mtk_oddmr_set_dmr_enable_dual(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle);
static void mtk_oddmr_dmr_common_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_dmr_smi_dual(struct cmdq_pkt *pkg);
static int mtk_oddmr_dmr_dbv_lookup(unsigned int dbv, struct mtk_drm_dmr_cfg_info *cfg_info,
	unsigned int *dbv_table_idx, unsigned int *dbv_node);
static int mtk_oddmr_dmr_fps_lookup(unsigned int fps, struct mtk_drm_dmr_cfg_info *cfg_info,
	unsigned int *fps_table_idx, unsigned int *fps_node);
static void mtk_oddmr_dmr_static_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, struct mtk_drm_dmr_static_cfg *static_cfg_data);
static void mtk_oddmr_dmr_gain_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, unsigned int dbv_node, unsigned int fps_node,
		struct mtk_drm_dmr_cfg_info *cfg_info);
static void mtk_oddmr_set_dmr_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle);

static int mtk_oddmr_dbi_dbv_lookup(unsigned int dbv, unsigned int *dbv_node_array,
	unsigned int dbv_node_num, unsigned int *dbv_node);
static int mtk_oddmr_dbi_fps_lookup(unsigned int fps, struct mtk_drm_dbi_cfg_info *cfg_info,
		unsigned int *fps_node);
static void mtk_oddmr_set_dbi_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle);

static void mtk_oddmr_dbi_change_remap_gain(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, uint32_t cur_max_time);
static void mtk_oddmr_remap_set_enable(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, bool en);
static int mtk_oddmr_remap_enable(struct drm_device *dev, bool en);
static void mtk_oddmr_dbi_dbv_table_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, unsigned int dbv_node,
		struct mtk_drm_dbi_cfg_info *cfg_info);
static void mtk_oddmr_dbi_gain_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, unsigned int dbv_node, unsigned int fps_node,
		struct mtk_drm_dbi_cfg_info *cfg_info, unsigned int gain_ratio);


static inline unsigned int mtk_oddmr_read(struct mtk_ddp_comp *comp,
		unsigned int offset)
{
		if (offset >= 0x2000 || (offset % 4) != 0) {
			DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return 0;
	}

	return readl(comp->regs + offset);
}

static inline void mtk_oddmr_write_mask_cpu(struct mtk_ddp_comp *comp,
		unsigned int value, unsigned int offset, unsigned int mask)
{
	unsigned int tmp;

	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}

	tmp = readl(comp->regs + offset);
	tmp = (tmp & ~mask) | (value & mask);
	writel(tmp, comp->regs + offset);
}

static inline void mtk_oddmr_write_cpu(struct mtk_ddp_comp *comp,
		unsigned int value, unsigned int offset)
{
	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}
	writel(value, comp->regs + offset);
}

static inline void mtk_oddmr_write(struct mtk_ddp_comp *comp, unsigned int value,
		unsigned int offset, void *handle)
{
	if (comp == NULL) {
		DDPPR_ERR("%s: invalid comp\n", __func__);
		return;
	}
	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}
	if (handle != NULL) {
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
				comp->regs_pa + offset, value, ~0);
	} else {
		writel(value, comp->regs + offset);
	}
}

static inline void mtk_oddmr_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
		unsigned int offset, unsigned int mask, void *handle)
{
	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}
	if (handle != NULL) {
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
				comp->regs_pa + offset, value, mask);
	} else {
		mtk_oddmr_write_mask_cpu(comp, value, offset, mask);
	}
}

static inline struct mtk_disp_oddmr *comp_to_oddmr(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_oddmr, ddp_comp);
}

/* must check after get current table idx*/
static inline bool mtk_oddmr_is_table_valid(int table_idx,
		uint32_t valid_table, const char *name, int line, int log)
{
	if (table_idx < 0) {
		if (log)
			DDPPR_ERR("%s[%d] invalid tableidx %d 0x%x\n",
					name, line, table_idx, valid_table);
		return false;
	}
	if (((1 << table_idx) & valid_table) > 0)
		return true;
	if (log)
		DDPPR_ERR("%s[%d] invalid tableidx %d 0x%x\n",
				name, line, table_idx, valid_table);
	return false;
}
#define IS_TABLE_VALID(idx, valid) mtk_oddmr_is_table_valid(idx, valid, __func__, __LINE__, 1)
#define IS_TABLE_VALID_LOW(idx, valid) mtk_oddmr_is_table_valid(idx, valid, __func__, __LINE__, 0)
static bool mtk_oddmr_is_svp_on_mtee(void)
{
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(NULL, "MTEE");
	if (!dt_node)
		return false;

	return true;
}
/*
 * @addr init data from addr if needed, set to NULL when no need
 * @secu alloc svp iommu if needed
 */
	static inline struct mtk_drm_gem_obj *
mtk_oddmr_load_buffer(struct drm_crtc *crtc,
		size_t size, void *addr, bool secu)
{
	struct mtk_drm_gem_obj *gem;

	ODDMRAPI_LOG("+\n");

	if (!size) {
		DDPMSG("%s invalid size\n",
				__func__);
		return NULL;
	}
	if (secu) {
		//mtk_svp_page-uncached,mtk_mm-uncached
		gem = mtk_drm_gem_create_from_heap(crtc->dev,
				"mtk_svp_page-uncached", size);
	} else {
		gem = mtk_drm_gem_create(
				crtc->dev, size, true);
	}

	if (!gem) {
		DDPMSG("%s gem create fail\n", __func__);
		return NULL;
	}
	DDPMSG("%s gem create %p iommu %llx size %lu\n", __func__,
		gem->kvaddr, gem->dma_addr, size);

	if ((addr != NULL) && (!secu))
		memcpy(gem->kvaddr, addr, size);

	/* if demura and dbi opened at the same time need combine */

	return gem;
}

static int mtk_oddmr_create_gce_pkt(struct drm_crtc *crtc,
		struct cmdq_pkt **pkt)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int index = 0;

	if (!mtk_crtc) {
		DDPMSG("%s:%d, invalid crtc:0x%p\n",
				__func__, __LINE__, crtc);
		return -1;
	}

	index = drm_crtc_index(crtc);
	if (index) {
		DDPMSG("%s:%d, invalid crtc:0x%p, index:%d\n",
				__func__, __LINE__, crtc, index);
		return -1;
	}

	if (*pkt != NULL)
		return 0;

	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_PQ]);
	else
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

	return 0;
}

static int mtk_oddmr_acquire_clock(void)
{
	unsigned long flags;

	ODDMRAPI_LOG("%d+\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));

	spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	if (atomic_read(&g_oddmr_priv->oddmr_clock_ref) == 0) {
		ODDMRFLOW_LOG("top clock is off\n");
		spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
		return -EAGAIN;
	}
	if (default_comp->mtk_crtc->is_dual_pipe) {
		if (atomic_read(&g_oddmr1_priv->oddmr_clock_ref) == 0) {
			ODDMRFLOW_LOG("top clock is off\n");
			spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
			return -EAGAIN;
		}
		atomic_inc(&g_oddmr1_priv->oddmr_clock_ref);
	}
	atomic_inc(&g_oddmr_priv->oddmr_clock_ref);
	spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
	ODDMRAPI_LOG("%d-\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));
	return 0;
}

static int mtk_oddmr_release_clock(void)
{
	unsigned long flags;

	ODDMRAPI_LOG("%d+\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));

	spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	if (atomic_read(&g_oddmr_priv->oddmr_clock_ref) == 0) {
		ODDMRFLOW_LOG("top clock is off\n");
		spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
		return -EAGAIN;
	}
	if (default_comp->mtk_crtc->is_dual_pipe) {
		if (atomic_read(&g_oddmr1_priv->oddmr_clock_ref) == 0) {
			ODDMRFLOW_LOG("top clock is off\n");
			spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
			return -EAGAIN;
		}
		atomic_dec(&g_oddmr1_priv->oddmr_clock_ref);
	}
	atomic_dec(&g_oddmr_priv->oddmr_clock_ref);
	spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
	ODDMRAPI_LOG("%d-\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));
	return 0;
}

static void mtk_oddmr_set_pq(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, struct mtk_oddmr_pq_param *pq)
{
	struct mtk_oddmr_pq_pair *param;
	uint32_t cnt;
	int i;

	param = pq->param;
	cnt = pq->counts;
	if (0 == cnt || NULL == param) {
		ODDMRFLOW_LOG("pq is NULL\n");
	} else {
		for (i = 0; i < cnt; i++) {
			ODDMRLOW_LOG("i %d, 0x%x 0x%x\n", i, param[i].addr, param[i].value);
			mtk_oddmr_write(comp, param[i].value, param[i].addr, pkg);
		}
	}
}

static void mtk_oddmr_set_crop(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	uint32_t tile_overhead, comp_width, height;
	uint32_t reg_value, reg_mask, win_lr = 0;

	reg_value = 0;
	reg_mask = 0;
	tile_overhead = oddmr_priv->cfg.comp_overhead;
	comp_width = oddmr_priv->cfg.comp_in_width;
	height = oddmr_priv->cfg.height;
	if (oddmr_priv->is_right_pipe)
		win_lr = 1;
	ODDMRAPI_LOG("comp_width %u tile_overhead %u win_lr %u+\n",
		comp_width, tile_overhead, win_lr);
	SET_VAL_MASK(reg_value, reg_mask, win_lr, REG_WINDOW_LR);
	SET_VAL_MASK(reg_value, reg_mask, tile_overhead, REG_GUARD_BAND_PIXEL);
	mtk_oddmr_write(comp, reg_value, DISP_ODDMR_CRP_CTR_2, pkg);
	//actual width
	mtk_oddmr_write(comp, comp_width - tile_overhead, DISP_ODDMR_CRP_CTR_0, pkg);
	mtk_oddmr_write(comp, height, DISP_ODDMR_CRP_CTR_1, pkg);
	mtk_oddmr_write(comp, !tile_overhead, DISP_ODDMR_TOP_CRP_BYPSS, pkg);
}

static void mtk_oddmr_set_crop_dual(struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	if (default_comp->mtk_crtc->is_dual_pipe) {
		mtk_oddmr_set_crop(default_comp, pkg);
		mtk_oddmr_set_crop(oddmr1_default_comp, pkg);
	} else
		mtk_oddmr_write(default_comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, pkg);
}

static void mtk_oddmr_od_udma_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t od_mode = g_od_param.od_basic_info.basic_param.od_mode;

	switch (od_mode) {
	case OD_MODE_TYPE_COMPRESS_18:
		break;
	case OD_MODE_TYPE_COMPRESS_12:
		mtk_oddmr_write(comp, 0x00000000, 0x183c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1800, pkg);
		mtk_oddmr_write(comp, 0x00000009, 0x1804, pkg);
		mtk_oddmr_write(comp, 0x00007fff, 0x1808, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x180c, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x1810, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1814, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1818, pkg);
		mtk_oddmr_write(comp, 0x00000008, 0x181c, pkg);
		mtk_oddmr_write(comp, 0x00000010, 0x1820, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1824, pkg);
		mtk_oddmr_write(comp, 0x00000020, 0x1828, pkg);
		mtk_oddmr_write(comp, 0x00000001, 0x183c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1800, pkg);
		mtk_oddmr_write(comp, 0x00000009, 0x1804, pkg);
		mtk_oddmr_write(comp, 0x00007fff, 0x1808, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x180c, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x1810, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1814, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1818, pkg);
		mtk_oddmr_write(comp, 0x00000008, 0x181c, pkg);
		mtk_oddmr_write(comp, 0x00000010, 0x1820, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1824, pkg);
		mtk_oddmr_write(comp, 0x00000020, 0x1828, pkg);
		mtk_oddmr_write(comp, 0x00000002, 0x183c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1800, pkg);
		mtk_oddmr_write(comp, 0x00000009, 0x1804, pkg);
		mtk_oddmr_write(comp, 0x00007fff, 0x1808, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x180c, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x1810, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1814, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1818, pkg);
		mtk_oddmr_write(comp, 0x00000008, 0x181c, pkg);
		mtk_oddmr_write(comp, 0x00000010, 0x1820, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1824, pkg);
		mtk_oddmr_write(comp, 0x00000020, 0x1828, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1600, pkg);
		mtk_oddmr_write(comp, 0x000000ff, 0x1604, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1608, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x160c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1610, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1614, pkg);
		mtk_oddmr_write(comp, 0x000000ff, 0x1618, pkg);
		mtk_oddmr_write(comp, 0x000000ff, 0x161c, pkg);
		mtk_oddmr_write(comp, 0x00000007, 0x1620, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1624, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1628, pkg);
		mtk_oddmr_write(comp, 0x000000f8, 0x162c, pkg);
		mtk_oddmr_write(comp, 0x000000f8, 0x1630, pkg);
		mtk_oddmr_write(comp, 0x000000f8, 0x1634, pkg);
		mtk_oddmr_write(comp, 0x0000ffea, 0x1638, pkg);
		break;
	default:
		break;
	}
	mtk_oddmr_write(comp, 0xFF, DISP_ODDMR_UDMA_W_CTR_23, pkg);
	mtk_oddmr_write(comp, 0x01, DISP_ODDMR_UDMA_W_CTR_16, pkg);
}

/* top,dither,common pq, udma */
static void mtk_oddmr_od_common_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t value = 0, mask = 0;

	ODDMRAPI_LOG("+\n");
	/* top */
	SET_VAL_MASK(value, mask, 0, REG_OD_EN);
	SET_VAL_MASK(value, mask, g_od_param.od_basic_info.basic_param.od_mode, REG_OD_MODE);
	mtk_oddmr_write(comp, value, DISP_ODDMR_OD_CTRL_EN, pkg);
	if (g_od_param.od_basic_info.basic_param.dither_sel == 1) {
		mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.dither_ctl,
				DISP_ODDMR_DITHER_CTR_1, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	} else if (g_od_param.od_basic_info.basic_param.dither_sel == 2) {
		mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.dither_ctl,
				DISP_ODDMR_DITHER_CTR_2, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	} else if (g_od_param.od_basic_info.basic_param.dither_sel == 0) {
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	}
	/* od basic pq */
	mtk_oddmr_set_pq(comp, pkg, &g_od_param.od_basic_info.basic_pq);
}
static void mtk_oddmr_od_hsk(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t comp_width, hsk_0, hsk_1, hsk_2;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	if (oddmr_priv == NULL)
		return;
	//merge_lines = oddmr_priv->od_data.merge_lines;
	hsk_0 = oddmr_priv->cfg.comp_in_width / 2;
	hsk_1 = oddmr_priv->cfg.height;

	comp_width = oddmr_priv->cfg.comp_in_width;
	if(oddmr_priv->data->p_num != 0)
		hsk_2 = comp_width / oddmr_priv->data->p_num;
	else
		hsk_2 = comp_width / 2;

	mtk_oddmr_write(comp, hsk_0, DISP_ODDMR_OD_HSK_0, pkg);
	mtk_oddmr_write(comp, hsk_1, DISP_ODDMR_OD_HSK_1, pkg);
	mtk_oddmr_write(comp, hsk_2, DISP_ODDMR_OD_HSK_2, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_OD_HSK_3, pkg);
	mtk_oddmr_write(comp, 0x8003, DISP_ODDMR_OD_HSK_4, pkg);
}

static void mtk_oddmr_od_hsk_6985(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t hsk_0, hsk_1, hsk_2, merge_lines;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	if (oddmr_priv == NULL)
		return;

	merge_lines = oddmr_priv->od_data.merge_lines;
	hsk_0 = oddmr_priv->cfg.comp_in_width * merge_lines;
	hsk_1 = oddmr_priv->cfg.height / merge_lines;
	hsk_2 = (4368 + hsk_0) / 16;

	mtk_oddmr_write(comp, hsk_0, DISP_ODDMR_OD_HSK_0, pkg);
	mtk_oddmr_write(comp, hsk_1, DISP_ODDMR_OD_HSK_1, pkg);
	mtk_oddmr_write(comp, hsk_2,
			DISP_ODDMR_OD_HSK_2, pkg);
	mtk_oddmr_write(comp, 0xFFF,
			DISP_ODDMR_OD_HSK_3, pkg);
	mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.od_hsk_4,
			DISP_ODDMR_OD_HSK_4, pkg);
}

static void mtk_oddmr_od_set_res_udma(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	uint32_t scaling_mode, hsize_scale, od_mode, dummy_1, hsize_align, hscaling;
	uint32_t comp_width, width, height, line_offset = 0, hsize = 0, vsize = 0, merge_lines;
	uint32_t reg_value = 0, reg_mask = 0, is_hscaling, is_vscaling, is_h_2x4x_sel;
	struct mtk_drm_private *priv = default_comp->mtk_crtc->base.dev->dev_private;

	if (oddmr_priv == NULL)
		return;
	if (priv == NULL)
		return;
	scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;
	od_mode = g_od_param.od_basic_info.basic_param.od_mode;
	is_hscaling = (scaling_mode & BIT(0)) > 0 ? 1 : 0;
	is_vscaling = (scaling_mode & BIT(1)) > 0 ? 1 : 0;
	is_h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 1 : 0;
	comp_width = oddmr_priv->cfg.comp_in_width;
	width = oddmr_priv->cfg.width;
	height = oddmr_priv->cfg.height;
	line_offset = mtk_oddmr_od_get_ln_offset(comp, comp_width, height,
						scaling_mode, od_mode, 0);
	merge_lines = oddmr_priv->od_data.merge_lines;
	if (priv->data->mmsys_id == MMSYS_MT6989) {
		hsize = comp_width;
		vsize = height;
		line_offset = line_offset > hsize ? line_offset:hsize;
		ODDMRAPI_LOG("line_offset %u, hsize %u\n", line_offset, hsize);

		hscaling = is_hscaling ? (is_h_2x4x_sel ? 4 : 2) : 1;
		hsize_align = is_hscaling ?
			(is_h_2x4x_sel ?
			DIV_ROUND_UP(comp_width, 8) * 8 : DIV_ROUND_UP(comp_width, 4) * 4)
			: comp_width;
		hsize_scale = hsize_align / hscaling;
		dummy_1 = hsize_scale * merge_lines;
		ODDMRAPI_LOG("hscaling %u, hsize_align %u, hsize_scale %u\n",
			hscaling, hsize_align, hsize_scale);
		line_offset = line_offset > dummy_1 ? line_offset : dummy_1;
		ODDMRAPI_LOG("line_offset %u, dummy_1 %u\n", line_offset, dummy_1);
	} else {
		hsize = comp_width * merge_lines;
		vsize = height / merge_lines;
	}
	mtk_oddmr_write(comp, 0, DISP_ODDMR_OD_UMDA_CTRL_0, pkg);
	mtk_oddmr_write(comp, line_offset, DISP_ODDMR_OD_UMDA_CTRL_1, pkg);
	mtk_oddmr_write(comp, hsize, DISP_ODDMR_OD_UMDA_CTRL_2, pkg);
	mtk_oddmr_write(comp, vsize, DISP_ODDMR_OD_UMDA_CTRL_3, pkg);
	SET_VAL_MASK(reg_value, reg_mask, is_hscaling, REG_ENABLE_HSCALING);
	SET_VAL_MASK(reg_value, reg_mask, is_vscaling, REG_ENABLE_VSCALING);
	SET_VAL_MASK(reg_value, reg_mask, 1, REG_DE_ALIGN8_EN);
	SET_VAL_MASK(reg_value, reg_mask, is_h_2x4x_sel, REG_HSD_2X4X_SEL);
	mtk_oddmr_write_mask(comp, reg_value, DISP_ODDMR_OD_SCALING_6, reg_mask, pkg);
	ODDMRAPI_LOG("w %u, h %u, comp_w %u ln_h_v %u, %u, %u, scaling 0x%x\n",
		width, height, comp_width, line_offset, hsize, vsize, reg_value);
}

static void mtk_oddmr_od_init_end(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv;

	priv = mtk_crtc->base.dev->dev_private;

	//od reset
	mtk_oddmr_write(comp, 0x200, DISP_ODDMR_OD_SW_RESET, handle);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_OD_SW_RESET, handle);
	mtk_oddmr_od_udma_init(comp, handle);
	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_ODDMR_OD_AEE)) {
		/* better hw aee check method in future */
		mtk_oddmr_write_mask(comp, ODDMR_IRQ_MASK_VAL,
			DISP_ODDMR_IRQ_MASK, ODDMR_IRQ_MASK_VAL, handle);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_UDMA_R_CTRL88, handle);
	}
	//force clk off
	if (priv->data->mmsys_id == MMSYS_MT6985)
		mtk_oddmr_od_hsk_6985(comp, handle);
	else {
		mtk_oddmr_od_hsk(comp, handle);
		mtk_oddmr_od_dummy(comp, handle);
	}

	//bypass off
	mtk_oddmr_write(comp, 0,
			DISP_ODDMR_TOP_OD_BYASS, handle);
	mtk_oddmr_write(comp, 1,
			DISP_ODDMR_TOP_HRT0_BYPASS, handle);
}

static void mtk_oddmr_fill_cfg(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	oddmr_priv->cfg.width = cfg->w;
	oddmr_priv->cfg.height = cfg->h;
	if (!cfg->tile_overhead.is_support) {
		if (mtk_crtc->is_dual_pipe) {
			oddmr_priv->cfg.comp_in_width = cfg->w / 2;
			oddmr_priv->cfg.comp_overhead = 0;
			oddmr_priv->cfg.total_overhead = 0;
		} else {
			oddmr_priv->cfg.comp_in_width = cfg->w;
			oddmr_priv->cfg.comp_overhead = 0;
			oddmr_priv->cfg.total_overhead = 0;
		}
	}
}
static bool mtk_oddmr_drm_mode_to_oddmr_timg(struct drm_display_mode *mode,
		struct mtk_oddmr_timing *oddmr_mode)
{
	bool ret = false;

	if ((mode != NULL) && (oddmr_mode != NULL)) {
		oddmr_mode->hdisplay = mode->hdisplay;
		oddmr_mode->vdisplay = mode->vdisplay;
		oddmr_mode->vrefresh = drm_mode_vrefresh(mode);
		ret = true;
	} else
		ret =  false;
	return ret;
}

static void mtk_oddmr_copy_oddmr_timg(struct mtk_oddmr_timing *dst,
		struct mtk_oddmr_timing *src)
{
	ODDMRAPI_LOG("+\n");
	dst->hdisplay = src->hdisplay;
	dst->vdisplay = src->vdisplay;
	dst->vrefresh = src->vrefresh;
	dst->mode_chg_index = src->mode_chg_index;
	dst->bl_level = src->bl_level;
}

static bool mtk_oddmr_match_panelid(struct mtk_oddmr_panelid *read,
		struct mtk_oddmr_panelid *expected)
{
	int i;
	bool flag = true;

	/* always return true if panelid in bin is zero*/
	if (expected->len == 0)
		return true;
	if (expected->len > 16)
		expected->len = 16;

	for (i = 0; i < expected->len; i++) {
		if (expected->data[i] != 0) {
			flag = false;
			break;
		}
	}
	if (flag == true)
		return flag;
	/* compare */
	flag = true;
	if (read->len > 16)
		read->len = 16;

	for (i = 0; i < read->len; i++) {
		if (expected->data[i] != read->data[i]) {
			flag = false;
			DDPPR_ERR("%s, idx %d, expected %u but %u\n",
					__func__, i, expected->data[i], read->data[i]);
			break;
		}
	}
	return flag;
}

/* return 100 x byte per pixel */
static int mtk_oddmr_dbi_bpp(int mode)
{
	int ret = 0;
	unsigned long  table_size = g_oddmr_priv->dbi_data.table_size;
	unsigned long  layer_size;
	struct mtk_drm_dbi_cfg_info *dbi_cfg_info = &g_oddmr_priv->dbi_cfg_info;

	if(dbi_cfg_info){
		layer_size = dbi_cfg_info->basic_info.panel_width
			* dbi_cfg_info->basic_info.panel_height * 4;
		ret = (400 * table_size)/layer_size;
		ODDMRFLOW_LOG("dbi hrt %d\n", ret);
	}
	return ret;

}

/* return 100 x byte per pixel */
static int mtk_oddmr_dmr_bpp(int mode)
{
	int ret = 0;
	unsigned long  table_size;
	unsigned long  layer_size;
	struct mtk_drm_dmr_cfg_info *dmr_cfg_info = &g_oddmr_priv->dmr_cfg_info;

	if(dmr_cfg_info->table_index.table_byte_num){
		table_size = dmr_cfg_info->table_index.table_byte_num;
		layer_size = dmr_cfg_info->basic_info.panel_width
			* dmr_cfg_info->basic_info.panel_height * 4;
		ret = (400 * table_size)/layer_size;
		ODDMRFLOW_LOG("dmr hrt %d\n", ret);
	}
	return ret;
}

/* return 100 x byte per pixel */
static int mtk_oddmr_od_bpp(int mode)
{
	int bits, ret = 100;
	uint32_t scaling_mode =
		g_od_param.od_basic_info.basic_param.scaling_mode;
	int hscaling, h_2x4x_sel, vscaling;

	hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
	vscaling = (scaling_mode & BIT(1)) > 0 ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
	hscaling = hscaling * h_2x4x_sel;
	switch (mode) {
	case OD_MODE_TYPE_RGB444:
	case OD_MODE_TYPE_COMPRESS_12:
		bits = 12;
		break;
	case OD_MODE_TYPE_RGB565:
		bits = 16;
		break;
	case OD_MODE_TYPE_COMPRESS_18:
		bits = 18;
		break;
	case OD_MODE_TYPE_RGB555:
		bits = 15;
		break;
	case OD_MODE_TYPE_RGB888:
		bits = 24;
		break;
	default:
		bits = 24;
		break;
	}
	/* double for R & W */
	ret = 2 * ret;
	ret = ret * bits / (hscaling * vscaling * 8);
	return ret;
}

static void mtk_oddmr_od_srt_cal(struct mtk_ddp_comp *comp, int en)
{
	struct mtk_disp_oddmr *oddmr_priv;
	uint32_t srt = 0, scaling_mode, od_mode, vrefresh;

	if (comp == NULL)
		return;
	oddmr_priv = comp_to_oddmr(comp);
	if (en) {
		scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;
		od_mode = g_od_param.od_basic_info.basic_param.od_mode;
		srt += mtk_oddmr_od_get_data_size(oddmr_priv->cfg.comp_in_width,
			oddmr_priv->cfg.height, scaling_mode, od_mode, 0);
		srt += mtk_oddmr_od_get_data_size(oddmr_priv->cfg.comp_in_width,
			oddmr_priv->cfg.height, scaling_mode, od_mode, 1);
		srt += mtk_oddmr_od_get_data_size(oddmr_priv->cfg.comp_in_width,
			oddmr_priv->cfg.height, scaling_mode, od_mode, 2);
		vrefresh = g_oddmr_current_timing.vrefresh;
		//blanking ratio
		do_div(srt, 1000);
		srt *= 125;
		do_div(srt, 100);
		srt = srt * vrefresh;
		do_div(srt, 1000);
		oddmr_priv->qos_srt_odr = srt;
		oddmr_priv->qos_srt_odw = srt;
	} else {
		oddmr_priv->qos_srt_odr = 0;
		oddmr_priv->qos_srt_odw = 0;
	}
	ODDMRAPI_LOG("srt %u -\n", srt);
}

static void mtk_oddmr_dbi_srt_cal(struct mtk_ddp_comp *comp, int en)
{
	struct mtk_disp_oddmr *oddmr_priv;
	struct mtk_drm_crtc *mtk_crtc;
	unsigned int table_size = g_oddmr_priv->dbi_data.table_size;
	uint32_t vrefresh;
	uint32_t srt = 0;

	if (comp == NULL)
		return;
	oddmr_priv = comp_to_oddmr(comp);
	mtk_crtc = comp->mtk_crtc;
	if (en) {
		srt = table_size;
		vrefresh = g_oddmr_current_timing.vrefresh;
		//blanking ratio
		do_div(srt, 1000);
		srt *= 125;
		do_div(srt, 100);
		srt = srt * vrefresh;
		do_div(srt, 1000);
		oddmr_priv->qos_srt_dbir = srt;
	} else {
		oddmr_priv->qos_srt_dbir = 0;
	}
	ODDMRAPI_LOG("cal srt %d/%d\n", oddmr_priv->qos_srt_dbir,oddmr_priv->last_qos_srt_dbir);
}


static void mtk_oddmr_dmr_srt_cal(struct mtk_ddp_comp *comp, int en)
{
	struct mtk_disp_oddmr *oddmr_priv;
	struct mtk_drm_crtc *mtk_crtc;
	unsigned int table_size;
	uint32_t vrefresh;
	uint32_t srt = 0;
	struct mtk_drm_dmr_cfg_info *dmr_cfg_data = &g_oddmr_priv->dmr_cfg_info;

	if (comp == NULL)
		return;
	oddmr_priv = comp_to_oddmr(comp);
	mtk_crtc = comp->mtk_crtc;
	if (en) {
		table_size = dmr_cfg_data->table_index.table_byte_num;
		srt = table_size;
		vrefresh = g_oddmr_current_timing.vrefresh;
		//blanking ratio
		do_div(srt, 1000);
		srt *= 125;
		do_div(srt, 100);
		srt = srt * vrefresh;
		do_div(srt, 1000);
		oddmr_priv->qos_srt_dmrr = srt;
	} else {
		oddmr_priv->qos_srt_dmrr = 0;
	}
	ODDMRAPI_LOG("cal srt %d/%d\n", oddmr_priv->qos_srt_dmrr,oddmr_priv->last_qos_srt_dmrr);
}

static void mtk_oddmr_set_spr2rgb(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t mask_2x2 = 0, spr_mask[6] = {0}, x_init = 0, spr_format = 0;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	bool is_right_pipe = false;
	uint32_t overhead, comp_width, width;

	if (oddmr_priv->spr_enable == 0 || oddmr_priv->spr_relay == 1)
		return;
	ODDMRAPI_LOG("+\n");
	is_right_pipe = oddmr_priv->is_right_pipe;
	spr_format = oddmr_priv->spr_format;
	overhead = oddmr_priv->cfg.comp_overhead;
	comp_width = oddmr_priv->cfg.comp_in_width;
	width = oddmr_priv->cfg.width;
	switch (spr_format) {
	case MTK_PANEL_RGBG_BGRG_TYPE:
		mask_2x2 = 1;
		spr_mask[0] = 1;
		spr_mask[1] = 3;
		spr_mask[2] = 3;
		spr_mask[3] = 1;
		spr_mask[4] = 0;
		spr_mask[5] = 0;
		break;
	case MTK_PANEL_BGRG_RGBG_TYPE:
		mask_2x2 = 1;
		spr_mask[0] = 3;
		spr_mask[1] = 1;
		spr_mask[2] = 1;
		spr_mask[3] = 3;
		spr_mask[4] = 0;
		spr_mask[5] = 0;
		break;
	case MTK_PANEL_RGBRGB_BGRBGR_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 1;
		spr_mask[1] = 2;
		spr_mask[2] = 3;
		spr_mask[3] = 3;
		spr_mask[4] = 2;
		spr_mask[5] = 1;
		break;
	case MTK_PANEL_BGRBGR_RGBRGB_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 3;
		spr_mask[1] = 2;
		spr_mask[2] = 1;
		spr_mask[3] = 1;
		spr_mask[4] = 2;
		spr_mask[5] = 3;
		break;
	case MTK_PANEL_RGBRGB_BRGBRG_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 1;
		spr_mask[1] = 2;
		spr_mask[2] = 3;
		spr_mask[3] = 2;
		spr_mask[4] = 3;
		spr_mask[5] = 1;
		break;
	case MTK_PANEL_BRGBRG_RGBRGB_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 2;
		spr_mask[1] = 3;
		spr_mask[2] = 1;
		spr_mask[3] = 1;
		spr_mask[4] = 2;
		spr_mask[5] = 3;
		break;
	default:
		break;
	}
	mtk_oddmr_write(comp, mask_2x2, DISP_ODDMR_REG_SPR_MASK_2X2, pkg);
	mtk_oddmr_write(comp, spr_mask[0], DISP_ODDMR_REG_SPR_MASK_0, pkg);
	mtk_oddmr_write(comp, spr_mask[1], DISP_ODDMR_REG_SPR_MASK_1, pkg);
	mtk_oddmr_write(comp, spr_mask[2], DISP_ODDMR_REG_SPR_MASK_2, pkg);
	mtk_oddmr_write(comp, spr_mask[3], DISP_ODDMR_REG_SPR_MASK_3, pkg);
	mtk_oddmr_write(comp, spr_mask[4], DISP_ODDMR_REG_SPR_MASK_4, pkg);
	mtk_oddmr_write(comp, spr_mask[5], DISP_ODDMR_REG_SPR_MASK_5, pkg);
	if (is_right_pipe) {
		//start idx of right pipe
		x_init = width - comp_width;
		x_init = mask_2x2 ? x_init % 2 : x_init % 3;
	}
	mtk_oddmr_write(comp, x_init, DISP_ODDMR_REG_SPR_X_INIT, pkg);
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_write(comp, comp_width, DISP_ODDMR_REG_SPR_PANEL_WIDTH, pkg);
	else
		mtk_oddmr_write(comp, width, DISP_ODDMR_REG_SPR_PANEL_WIDTH, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_REG_SPR_COMP_EN, pkg);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_TOP_S2R_BYPASS, pkg);
}

static void mtk_oddmr_set_spr2rgb_dual(struct cmdq_pkt *pkg)
{
	if (g_oddmr_priv->spr_enable == 0 || g_oddmr_priv->spr_relay == 1)
		return;
	mtk_oddmr_set_spr2rgb(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_spr2rgb(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	ODDMRLOW_LOG("%s oddmr_start\n", mtk_dump_comp_str(comp));
}

static void mtk_oddmr_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	ODDMRFLOW_LOG("%s oddmr_stop\n", mtk_dump_comp_str(comp));
	oddmr_priv->qos_srt_dmrr = 0;
	oddmr_priv->qos_srt_dbir = 0;
	oddmr_priv->qos_srt_odr = 0;
	oddmr_priv->qos_srt_odw = 0;
}

static void mtk_oddmr_od_clk_en(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;

	value = 0x0880; mask = 0x0880;
	SET_VAL_MASK(value, mask, 1, REG_OD_CLK_EN);
	mtk_oddmr_write_mask(comp, value,
			DISP_ODDMR_OD_UDMA_CTR_0, mask, NULL);
}
static void mtk_oddmr_od_prepare(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	ODDMRFLOW_LOG("%s+\n", mtk_dump_comp_str(comp));
	if (oddmr_priv->data->is_od_need_force_clk)
		mtk_oddmr_od_hsk_force_clk(comp, NULL);
	if (oddmr_priv->data->is_od_need_crop_garbage == true) {
		mtk_oddmr_write_cpu(comp, 0, DISP_ODDMR_CRP_CTR_0);
		mtk_oddmr_write_cpu(comp, 0, DISP_ODDMR_CRP_CTR_1);
		mtk_oddmr_write_cpu(comp, 0, DISP_ODDMR_TOP_CRP_BYPSS);
		udelay(1);
	}
	mtk_oddmr_od_clk_en(comp, NULL);
	//crop off, add for first boot
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_crop(comp, NULL);
	else
		mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, NULL);
}

static void mtk_oddmr_spr2rgb_prepare(struct mtk_ddp_comp *comp)
{
	mtk_oddmr_write_cpu(comp, 1,
			DISP_ODDMR_REG_SPR_COMP_EN);
}

static void mtk_oddmr_top_prepare(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	uint32_t value = 0, mask = 0;

	ODDMRAPI_LOG("+\n");
	SET_VAL_MASK(value, mask, 1, REG_ODDMR_TOP_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 1, REG_FORCE_COMMIT);
	if (oddmr_priv->data->need_bypass_shadow == true)
		SET_VAL_MASK(value, mask, 1, REG_BYPASS_SHADOW);
	else
		SET_VAL_MASK(value, mask, 0, REG_BYPASS_SHADOW);
	mtk_oddmr_write_mask(comp, value,
			DISP_ODDMR_TOP_CTR_3, mask, handle);

}

static void mtk_oddmr_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	unsigned int postalign_en = comp->mtk_crtc->panel_ext->params->spr_params.postalign_en;

	ODDMRAPI_LOG("+\n");
	mtk_ddp_comp_clk_prepare(comp);

	if (is_oddmr_od_support || is_oddmr_dmr_support ||
		((comp->mtk_crtc->panel_ext->params->spr_params.enable == 1) &&
		(comp->mtk_crtc->panel_ext->params->spr_params.relay == 0) &&
		postalign_en == 0))
		mtk_oddmr_top_prepare(comp, NULL);
	if (is_oddmr_od_support)
		mtk_oddmr_od_prepare(comp, NULL);

	if ((comp->mtk_crtc->panel_ext->params->spr_params.enable == 1) &&
			(comp->mtk_crtc->panel_ext->params->spr_params.relay == 0) &&
			(is_oddmr_dbi_support ||
				is_oddmr_od_support || is_oddmr_dmr_support || postalign_en == 0))
		mtk_oddmr_spr2rgb_prepare(comp);
	atomic_set(&oddmr_priv->oddmr_clock_ref, 1);
}

static void mtk_oddmr_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	unsigned long flags;

	ODDMRAPI_LOG("+\n");
	spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	atomic_dec(&oddmr_priv->oddmr_clock_ref);
	while (atomic_read(&oddmr_priv->oddmr_clock_ref) > 0) {
		spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
		DDPINFO("waiting for oddmr_lock, %d\n",
				atomic_read(&oddmr_priv->oddmr_clock_ref));
		usleep_range(50, 100);
		spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	}
	spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_disp_oddmr_config_overhead(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	uint32_t comp_overhead;

	if (cfg->tile_overhead.is_support) {
		//TODO: dmr blocksize
		if (mtk_crtc->is_dual_pipe)
			comp_overhead = oddmr_priv->data->tile_overhead;
		else
			comp_overhead = 0;
		is_oddmr_dmr_support = comp->mtk_crtc->panel_ext->params->is_support_dmr;
		is_oddmr_od_support = comp->mtk_crtc->panel_ext->params->is_support_od;
		if (!is_oddmr_dmr_support && !is_oddmr_od_support)
			comp_overhead = 0;
		if (oddmr_priv->is_right_pipe) {
			cfg->tile_overhead.right_in_width += comp_overhead;
			cfg->tile_overhead.right_overhead += comp_overhead;
			oddmr_priv->cfg.comp_overhead = comp_overhead;
			oddmr_priv->cfg.comp_in_width = cfg->tile_overhead.right_in_width;
			oddmr_priv->cfg.total_overhead = cfg->tile_overhead.right_overhead;
		} else {
			cfg->tile_overhead.left_in_width += comp_overhead;
			cfg->tile_overhead.left_overhead += comp_overhead;
			oddmr_priv->cfg.comp_overhead = comp_overhead;
			oddmr_priv->cfg.comp_in_width = cfg->tile_overhead.left_in_width;
			oddmr_priv->cfg.total_overhead = cfg->tile_overhead.left_overhead;
		}
	}
}

static void mtk_disp_oddmr_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	unsigned int oddmr_overhead_v = 0;
	unsigned int dbi_overhead_v = 0;

	DDPDBG("%s line: %d\n", __func__, __LINE__);

	if ( comp->mtk_crtc->panel_ext->params->is_support_dbi == true)
		oddmr_overhead_v += dbi_overhead_v;

	/*set component overhead*/
	oddmr_priv->tile_overhead_v.comp_overhead_v = oddmr_overhead_v;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
			oddmr_priv->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	oddmr_priv->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void mtk_oddmr_first_cfg(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	struct mtk_ddp_comp *out_comp = NULL;
	unsigned long flags;
	int crtc_idx;
	struct drm_display_mode *mode;
	unsigned int postalign_en;

	DDPMSG("%s+\n", __func__);
	is_oddmr_dmr_support = comp->mtk_crtc->panel_ext->params->is_support_dmr;
	is_oddmr_od_support = comp->mtk_crtc->panel_ext->params->is_support_od;
	postalign_en = comp->mtk_crtc->panel_ext->params->spr_params.postalign_en;
	is_oddmr_dbi_support = comp->mtk_crtc->panel_ext->params->is_support_dbi;
	/* get spr status */
	oddmr_priv->spr_enable = comp->mtk_crtc->panel_ext->params->spr_params.enable;
	oddmr_priv->spr_relay = comp->mtk_crtc->panel_ext->params->spr_params.relay;
	oddmr_priv->spr_format = comp->mtk_crtc->panel_ext->params->spr_params.spr_format_type;
	oddmr_priv->od_user_gain = 64;
	/* read panelid */
	crtc_idx = drm_crtc_index(&mtk_crtc->base);
	if (crtc_idx == 0)
		out_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (out_comp && (is_oddmr_dmr_support || is_oddmr_od_support))
		out_comp->funcs->io_cmd(out_comp, NULL, DSI_READ_PANELID, &g_panelid);
	spin_lock_irqsave(&g_oddmr_timing_lock, flags);
	mode = mtk_crtc_get_display_mode_by_comp(__func__, &mtk_crtc->base, comp, false);
	mtk_oddmr_drm_mode_to_oddmr_timg(mode, &g_oddmr_current_timing);
	spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);
	mtk_oddmr_fill_cfg(comp, cfg);
	if (is_oddmr_dmr_support || is_oddmr_od_support ||
		((comp->mtk_crtc->panel_ext->params->spr_params.enable == 1) &&
		(comp->mtk_crtc->panel_ext->params->spr_params.relay == 0) &&
		postalign_en == 0)) {
		mtk_oddmr_set_spr2rgb(comp, handle);
		if (comp->mtk_crtc->is_dual_pipe)
			mtk_oddmr_set_crop(comp, NULL);
		else
			mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, NULL);
	}
	DDPMSG("%s dmr %d od %d-\n", __func__, is_oddmr_dmr_support, is_oddmr_od_support);
}

static void mtk_oddmr_remap_config(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle)
{
	int remap_enable;

	ODDMRAPI_LOG("+\n");

	remap_enable = atomic_read(&g_oddmr_priv->dbi_data.remap_enable);

	if (remap_enable) {
		mutex_lock(&g_dbi_data_lock);
		mtk_oddmr_dbi_change_remap_gain(default_comp, handle, g_oddmr_priv->dbi_data.cur_max_time);
		mtk_oddmr_remap_set_enable(comp, handle, true);
		mutex_unlock(&g_dbi_data_lock);
	}

}


static void mtk_oddmr_dbi_config(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *handle)
{
	unsigned int dbi_fps_node = 0;
	unsigned int dbi_dbv_node = 0;
	int cur_tb;

	struct mtk_drm_dbi_cfg_info *dbi_cfg_data = &g_oddmr_priv->dbi_cfg_info;
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data_tb1 = &g_oddmr_priv->dbi_cfg_info_tb1;
	dma_addr_t addr = 0;

	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;
	struct mtk_disp_oddmr *oddmr = comp_to_oddmr(comp);
	unsigned int crop_height;
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
	unsigned int idx;

	struct mtk_ddp_comp *output_comp = mtk_ddp_comp_request_output(comp->mtk_crtc);
	struct mtk_dsi *dsi = container_of(output_comp, struct mtk_dsi, ddp_comp);

	unsigned int cur_dbv;
	unsigned int cur_fps;
	unsigned long flags;
	unsigned int gain_ratio;

	if (is_oddmr_dbi_support == true && g_oddmr_priv->dbi_state == ODDMR_INIT_DONE
		&& atomic_read(&g_oddmr_priv->dbi_data.update_table_done)) {

		gain_ratio = atomic_read(&g_oddmr_priv->dbi_data.gain_ratio);

		spin_lock_irqsave(&g_oddmr_timing_lock, flags);
		cur_dbv = g_oddmr_current_timing.bl_level;
		cur_fps = g_oddmr_current_timing.vrefresh;
		spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);

		mtk_oddmr_dmr_common_init(comp, handle);

		if (!(g_oddmr_priv->spr_enable == 0 || g_oddmr_priv->spr_relay == 1))
			mtk_oddmr_set_spr2rgb(comp, handle);

		mtk_oddmr_dmr_smi_dual(handle);

		if(mtk_oddmr_dbi_dbv_lookup(cur_dbv,
			dbi_cfg_data->fps_dbv_node.DBV_node, dbi_cfg_data->fps_dbv_node.DBV_num, &dbi_dbv_node))
			ODDMRFLOW_LOG("dmr dbv lookup fail\n");
		if(mtk_oddmr_dbi_fps_lookup(cur_fps,
			dbi_cfg_data, &dbi_fps_node))
			ODDMRFLOW_LOG("dmr fps lookup fail\n");
		atomic_set(&g_oddmr_priv->dbi_data.cur_dbv_node, dbi_dbv_node);
		atomic_set(&g_oddmr_priv->dbi_data.cur_fps_node, dbi_fps_node);

		if (dsi->output_en) {
			idx = (unsigned int)atomic_read(&g_oddmr_priv->dbi_data.cur_table_idx);
			addr = g_oddmr_priv->dbi_data.dbi_table[idx]->dma_addr;
		} else {
			idx = (unsigned int)atomic_read(&g_oddmr_priv->dbi_data.update_table_idx);
			addr = g_oddmr_priv->dbi_data.dbi_table[idx]->dma_addr;
			atomic_set(&g_oddmr_priv->dbi_data.cur_table_idx, atomic_read(
				&g_oddmr_priv->dbi_data.update_table_idx));
		}
		cur_tb = atomic_read(&g_oddmr_priv->dbi_data.cur_table_idx);
		if (cur_tb) {
			mtk_oddmr_dmr_static_cfg(comp, handle, &dbi_cfg_data_tb1->static_cfg);
			mtk_oddmr_dbi_gain_cfg(comp,
				handle, dbi_dbv_node, dbi_fps_node, dbi_cfg_data_tb1 ,gain_ratio);
		} else {
			mtk_oddmr_dmr_static_cfg(comp, handle, &dbi_cfg_data->static_cfg);
			mtk_oddmr_dbi_gain_cfg(comp,
				handle, dbi_dbv_node, dbi_fps_node, dbi_cfg_data,gain_ratio);
		}
		mtk_oddmr_write(default_comp, addr >> 4, DISP_ODDMR_DMR_UDMA_CTR_4, handle);
		mtk_oddmr_write(default_comp, addr >> 20, DISP_ODDMR_DMR_UDMA_CTR_5, handle);

		if(oddmr->set_partial_update) {
			overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : oddmr->tile_overhead_v.overhead_v;
			comp_overhead_v = (!overhead_v) ? 0 : oddmr->tile_overhead_v.comp_overhead_v;
			crop_height = oddmr->roi_height + (overhead_v - comp_overhead_v) * 2;
			ODDMRAPI_LOG("log: %s %d overhead_v:%d comp_overhead_v:%d crop_hegiht:%d\n",
				__func__, __LINE__, overhead_v, comp_overhead_v, crop_height);

			if (priv->data->mmsys_id == MMSYS_MT6989) {
				/* ODDMR on MT6989 dose not support V crop */
				mtk_oddmr_write(comp, dbi_part_data.dbi_y_ini,
					DISP_ODDMR_REG_DMR_Y_INI, handle);
				mtk_oddmr_write(comp, (oddmr->roi_height + overhead_v * 2),
					DISP_ODDMR_FMT_CTR_1, handle); // oddmr input height

				mtk_oddmr_write(comp, dbi_part_data.dbi_udma_y_ini,
					DISP_ODDMR_DMR_UDMA_CTR_7, handle);

				mtk_oddmr_write(comp, dbi_part_data.y_idx2_ini,
					DISP_ODDMR_REG_Y_IDX2_INI, handle);
				mtk_oddmr_write(comp, dbi_part_data.y_remain2_ini,
					DISP_ODDMR_REG_Y_REMAIN2_INI, handle);
			}
		} else {
			if (priv->data->mmsys_id == MMSYS_MT6989) {
				mtk_oddmr_write(comp, 0,
					DISP_ODDMR_REG_DMR_Y_INI, handle);
				mtk_oddmr_write(comp, dbi_cfg_data->basic_info.panel_height,
					DISP_ODDMR_FMT_CTR_1, handle); // oddmr input height

				mtk_oddmr_write(comp, 0,
					DISP_ODDMR_DMR_UDMA_CTR_7, handle);

				mtk_oddmr_write(comp, 0,
					DISP_ODDMR_REG_Y_IDX2_INI, handle);
				mtk_oddmr_write(comp, 0,
					DISP_ODDMR_REG_Y_REMAIN2_INI, handle);
			}
		}
		if (dbi_cfg_data->dbv_node.DBV_num) {
			if(mtk_oddmr_dbi_dbv_lookup(cur_dbv,
				dbi_cfg_data->dbv_node.DBV_node, dbi_cfg_data->dbv_node.DBV_num, &dbi_dbv_node))
				ODDMRFLOW_LOG("dbi dbv lookup fail\n");
			mtk_oddmr_dbi_dbv_table_cfg(comp,
				handle, dbi_dbv_node, dbi_cfg_data);
		}
		mtk_oddmr_set_dbi_enable(comp, g_oddmr_priv->dbi_enable, handle);
	}
}

static void mtk_oddmr_s2r_bypass(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, bool s2r_bypass)
{
	ODDMRFLOW_LOG("%s+\n", mtk_dump_comp_str(comp));

	if (s2r_bypass)
		mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_SPR_COMP_EN, handle);
	else
		mtk_oddmr_set_spr2rgb(comp, handle);
}

static void mtk_oddmr_config(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg,
		struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct cmdq_pkt *cmdq_handle0 = NULL;
	struct cmdq_pkt *cmdq_handle1 = NULL;
	struct cmdq_client *client = NULL;
	unsigned int dbv_table_idx = 0;
	unsigned int dbv_node = 0;
	unsigned int fps_table_idx = 0;
	unsigned int fps_node = 0;
	unsigned int dbi_fps_node = 0;
	unsigned int dbi_dbv_node = 0;

	dma_addr_t addr = 0;
	struct mtk_drm_dmr_cfg_info *dmr_cfg_data = &g_oddmr_priv->dmr_cfg_info;
	unsigned int postalign_en = 0;

	if (!comp->mtk_crtc || !comp->mtk_crtc->panel_ext || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("comp is invalid-\n");
		return;
	}

	if (comp->mtk_crtc->panel_ext->params)
		postalign_en = comp->mtk_crtc->panel_ext->params->spr_params.postalign_en;

	ODDMRFLOW_LOG("%s+\n", mtk_dump_comp_str(comp));
	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		client = mtk_crtc->gce_obj.client[CLIENT_PQ];
	else
		client = mtk_crtc->gce_obj.client[CLIENT_CFG];

	mtk_oddmr_fill_cfg(comp, cfg);

	if(is_oddmr_od_support || is_oddmr_dmr_support) {
		//crop off
		if (comp->mtk_crtc->is_dual_pipe)
			mtk_oddmr_set_crop(comp, NULL);
		else
			mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, NULL);
	}

	if (is_oddmr_od_support == true &&
		g_oddmr_priv->od_state >= ODDMR_INIT_DONE) {
		//crop off
		if (comp->mtk_crtc->is_dual_pipe)
			mtk_oddmr_set_crop(comp, NULL);
		else
			mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, NULL);
		//write sram
		cmdq_mbox_enable(client->chan);
		cmdq_handle0 =
			oddmr_priv->od_data.od_sram_pkgs[oddmr_priv->od_data.od_dram_sel[0]][0];
		cmdq_handle1 =
			oddmr_priv->od_data.od_sram_pkgs[oddmr_priv->od_data.od_dram_sel[1]][1];
		if (cmdq_handle0) {
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 0);
			cmdq_pkt_refinalize(cmdq_handle0);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 1);
			cmdq_pkt_flush(cmdq_handle0);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 2);
		}
		if (cmdq_handle1) {
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 3);
			cmdq_pkt_refinalize(cmdq_handle1);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 4);
			cmdq_pkt_flush(cmdq_handle1);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 5);
		}
		cmdq_mbox_disable(client->chan);
		if (oddmr_priv->od_data.od_sram_read_sel == 1)
			mtk_oddmr_write(comp, 0x20, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
		else
			mtk_oddmr_write(comp, 0x10, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
		if (!(g_oddmr_priv->spr_enable == 0 || g_oddmr_priv->spr_relay == 1))
			mtk_oddmr_set_spr2rgb(comp, NULL);
		mtk_oddmr_od_smi(comp, NULL);
		mtk_oddmr_od_set_dram(comp, NULL);
		mtk_oddmr_od_common_init(comp, NULL);
		mtk_oddmr_od_set_res_udma(comp, NULL);
		mtk_oddmr_od_init_end(comp, NULL);
		mtk_oddmr_set_od_enable(comp, oddmr_priv->od_enable, handle);
		//sw bypass first frame od pq
		if ((oddmr_priv->data->is_od_support_hw_skip_first_frame == false) &&
				(is_oddmr_od_support == true) &&
				(oddmr_priv->od_enable == 1)) {
			mtk_oddmr_write_mask(comp, 0, DISP_ODDMR_OD_PQ_0,
					REG_FLD_MASK(REG_OD_USER_WEIGHT), handle);
			atomic_set(&g_oddmr_od_weight_trigger, 1);
		}
	}

	if (is_oddmr_dmr_support == true && g_oddmr_priv->dmr_state == ODDMR_INIT_DONE) {
		mtk_oddmr_dmr_common_init(comp, handle);
		if (!(g_oddmr_priv->spr_enable == 0 || g_oddmr_priv->spr_relay == 1))
			mtk_oddmr_set_spr2rgb(comp, handle);
		mtk_oddmr_dmr_smi_dual(handle);

		if(mtk_oddmr_dmr_dbv_lookup(g_oddmr_current_timing.bl_level,
			dmr_cfg_data, &dbv_table_idx, &dbv_node))
			ODDMRFLOW_LOG("dmr dbv lookup fail\n");
		if(mtk_oddmr_dmr_fps_lookup(g_oddmr_current_timing.vrefresh,
			dmr_cfg_data, &fps_table_idx, &fps_node))
			ODDMRFLOW_LOG("dmr fps lookup fail\n");
		atomic_set(&g_oddmr_priv->dmr_data.cur_dbv_node, dbv_node);
		atomic_set(&g_oddmr_priv->dmr_data.cur_dbv_table_idx, dbv_table_idx);
		atomic_set(&g_oddmr_priv->dmr_data.cur_fps_node, fps_node);
		atomic_set(&g_oddmr_priv->dmr_data.cur_fps_table_idx, fps_table_idx);

		mtk_oddmr_dmr_static_cfg(comp, handle, &dmr_cfg_data->static_cfg);
		mtk_oddmr_dmr_gain_cfg(comp,
			handle, dbi_dbv_node, dbi_fps_node, dmr_cfg_data);
		//set dmr table
		addr = g_oddmr_priv->dmr_data.mura_table[dbv_table_idx][fps_table_idx]->dma_addr;
		mtk_oddmr_write(default_comp, addr >> 4, DISP_ODDMR_DMR_UDMA_CTR_4, handle);
		mtk_oddmr_write(default_comp, addr >> 20, DISP_ODDMR_DMR_UDMA_CTR_5, handle);
		mtk_oddmr_set_dmr_enable(comp, g_oddmr_priv->dmr_enable, handle);
	}

	if (g_oddmr_priv->spr_enable == 1 && g_oddmr_priv->spr_relay == 0 &&
		postalign_en == 0 && mtk_crtc->spr_is_on == 1)
		mtk_oddmr_set_spr2rgb(comp, handle);

	mtk_oddmr_dbi_config(comp,handle);
	mtk_oddmr_remap_config(comp, handle);
}

static void mtk_oddmr_dump_od_table(int table_idx)
{
	struct mtk_oddmr_od_table *table = g_od_param.od_tables[table_idx];
	struct mtk_oddmr_od_table_basic_info *info = &table->table_basic_info;
	int i;

	DDPDUMP("OD Table%d\n", table_idx);
	DDPDUMP("%u x %u fps: %u(%d-%d) dbv 0x%x(0x%x-0x%x)\n",
			info->width, info->height,
			info->fps, info->min_fps, info->max_fps,
			info->dbv, info->min_dbv, info->max_dbv);
	DDPDUMP("fps cnt %d\n", table->fps_cnt);
	for (i = 0; i < table->fps_cnt && i < OD_GAIN_MAX; i++) {
		DDPDUMP("(%d, %d) ",
				table->fps_table[i].item, table->fps_table[i].value);
	}
	DDPDUMP("\n");
	DDPDUMP("dbv cnt %d\n", table->bl_cnt);
	for (i = 0; i < table->bl_cnt && i < OD_GAIN_MAX; i++) {
		DDPDUMP("(%d, %d) ",
				table->bl_table[i].item, table->bl_table[i].value);
	}
	DDPDUMP("\n");
}

static void mtk_oddmr_dump_od_param(void)
{
	struct mtk_oddmr_od_basic_param *basic_param =
		&g_od_param.od_basic_info.basic_param;

	DDPDUMP("OD Basic info:\n");
	DDPDUMP("valid_table 0x%x, valid_table_cnt %d\n",
			g_od_param.valid_table, g_od_param.valid_table_cnt);
	DDPDUMP("res_switch_mode %d, %u x %u, table cnts %d, od_mode %d\n",
			basic_param->resolution_switch_mode,
			basic_param->panel_width, basic_param->panel_height,
			basic_param->table_cnt, basic_param->od_mode);
	DDPDUMP("dither_sel %d scaling_mode 0x%x\n",
			basic_param->dither_sel, basic_param->scaling_mode);
	if (IS_TABLE_VALID_LOW(0, g_od_param.valid_table))
		mtk_oddmr_dump_od_table(0);
	if (IS_TABLE_VALID_LOW(1, g_od_param.valid_table))
		mtk_oddmr_dump_od_table(1);
	if (IS_TABLE_VALID_LOW(2, g_od_param.valid_table))
		mtk_oddmr_dump_od_table(2);
	if (IS_TABLE_VALID_LOW(3, g_od_param.valid_table))
		mtk_oddmr_dump_od_table(3);
}

static void mtk_oddmr_dump_dmr_table(int table_idx)
{
	struct mtk_oddmr_dmr_table *table = g_dmr_param.dmr_tables[table_idx];
	struct mtk_oddmr_dmr_table_basic_info *info = &table->table_basic_info;
	int i;

	DDPDUMP("DMR Table%d\n", table_idx);
	DDPDUMP("%u x %u fps: %u(%d-%d) dbv 0x%x(0x%x-0x%x)\n",
			info->width, info->height,
			info->fps, info->min_fps, info->max_fps,
			info->dbv, info->min_dbv, info->max_dbv);
	DDPDUMP("fps cnt %d\n", table->fps_cnt);
	for (i = 0; i < table->fps_cnt && i < DMR_GAIN_MAX; i++) {
		DDPDUMP("(%d, 0x%x 0x%x 0x%x) ",
				table->fps_table[i].fps, table->fps_table[i].beta,
				table->fps_table[i].gain, table->fps_table[i].offset);
	}
	DDPDUMP("\n");
	DDPDUMP("dbv cnt %d\n", table->bl_cnt);
	for (i = 0; i < table->bl_cnt && i < DMR_GAIN_MAX; i++) {
		DDPDUMP("(%d, %d) ",
				table->bl_table[i].item, table->bl_table[i].value);
	}
	DDPDUMP("\n");
}

static void mtk_oddmr_dump_dmr_param(void)
{
	struct mtk_oddmr_dmr_basic_param *basic_param =
		&g_dmr_param.dmr_basic_info.basic_param;

	DDPDUMP("DMR Basic info:\n");
	DDPDUMP("valid_table 0x%x, valid_table_cnt %d\n",
			g_dmr_param.valid_table, g_dmr_param.valid_table_cnt);
	DDPDUMP("res_switch_mode %d, %u x %u, table cnts %d, table mode %d\n",
			basic_param->resolution_switch_mode,
			basic_param->panel_width, basic_param->panel_height,
			basic_param->table_cnt, basic_param->dmr_table_mode);
	DDPDUMP("dither_sel %d is_second_dmr 0x%x\n",
			basic_param->dither_sel, basic_param->is_second_dmr);
	if (IS_TABLE_VALID_LOW(0, g_dmr_param.valid_table))
		mtk_oddmr_dump_dmr_table(0);
	if (IS_TABLE_VALID_LOW(1, g_dmr_param.valid_table))
		mtk_oddmr_dump_dmr_table(1);
}

int mtk_oddmr_analysis(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	DDPDUMP("== %s ANALYSIS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("od_support %d dmr_support %d\n", is_oddmr_od_support, is_oddmr_dmr_support);
	DDPDUMP("od_en %d, dmr_en %d, od_state %d, dmr_state %d\n",
			oddmr_priv->od_enable,
			oddmr_priv->dmr_enable,
			oddmr_priv->od_state,
			oddmr_priv->dmr_state);
	DDPDUMP("oddmr current bl %u, fps %u %u x %u\n",
			g_oddmr_current_timing.bl_level,
			g_oddmr_current_timing.vrefresh,
			g_oddmr_current_timing.hdisplay,
			g_oddmr_current_timing.vdisplay);
	DDPDUMP("OD: r_sel %d, dram_sel %d, dram_sel %d, sram0 %d, sram1 %d\n",
			oddmr_priv->od_data.od_sram_read_sel,
			oddmr_priv->od_data.od_dram_sel[0],
			oddmr_priv->od_data.od_dram_sel[1],
			oddmr_priv->od_data.od_sram_table_idx[0],
			oddmr_priv->od_data.od_sram_table_idx[1]);
	mtk_oddmr_dump_od_param();
	DDPDUMP("DMR: cur_dbv_node %d, cur_fps_node %d\n",
			atomic_read(&oddmr_priv->dmr_data.cur_dbv_node),
			atomic_read(&oddmr_priv->dmr_data.cur_fps_node));
	DDPDUMP("DMR: cur_dbv_table_idx %d, cur_fps_table_idx %d\n",
			atomic_read(&oddmr_priv->dmr_data.cur_dbv_table_idx),
			atomic_read(&oddmr_priv->dmr_data.cur_fps_table_idx));
	mtk_oddmr_dump_dmr_param();
	return 0;
}

void mtk_oddmr_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	void __iomem *mbaddr;
	int i;
	struct mtk_drm_private *priv = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;


	if (g_oddmr_dump_en == false)
		return;
	mtk_crtc = comp->mtk_crtc;
	if(mtk_crtc->base.dev->dev_private)
		priv = mtk_crtc->base.dev->dev_private;

	if(priv && (priv->data->mmsys_id == MMSYS_MT6989)){
		DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
		DDPDUMP("-- Start dump oddmr registers --\n");
		mbaddr = baddr;
		for (i = 0; i < 0x2000; i += 16) {
			DDPDUMP("ODDMR+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
		}
		return;
	}

	DDPDUMP("== %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("-- Start dump oddmr registers --\n");
	mbaddr = baddr;
	DDPDUMP("ODDMR_TOP: 0x%p\n", mbaddr);
	for (i = 0; i < 0x200; i += 16) {
		DDPDUMP("ODDMR_TOP+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_OD_BASE;
	DDPDUMP("ODDMR_OD: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1FC; i += 16) {
		DDPDUMP("ODDMR_OD+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_SMI_BASE;
	DDPDUMP("ODDMR_SMI: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1E0; i += 16) {
		DDPDUMP("ODDMR_SMI+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_SPR2RGB_BASE;
	DDPDUMP("ODDMR_SPR2RGB: 0x%p\n", mbaddr);
	for (i = 0; i < 0x2C; i += 16) {
		DDPDUMP("ODDMR_SPR2RGB+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_UDMA_R_BASE;
	DDPDUMP("ODDMR_UDMA_R: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1C8; i += 16) {
		DDPDUMP("ODDMR_UDMA_R+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_UDMA_W_BASE;
	DDPDUMP("ODDMR_UDMA_W: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1C8; i += 16) {
		DDPDUMP("ODDMR_UDMA_W+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}
}

void mtk_disp_oddmr_debug(const char *opt)
{
	ODDMRFLOW_LOG(":%s\n", opt);
	if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		ODDMRFLOW_LOG("debug_flow_log = %d\n", debug_flow_log);
	} else if (strncmp(opt, "flow_msg:", 9) == 0) {
		debug_flow_msg = strncmp(opt + 9, "1", 1) == 0;
		ODDMRFLOW_LOG("debug_flow_msg = %d\n", debug_flow_msg);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		ODDMRFLOW_LOG("debug_api_log = %d\n", debug_api_log);
	} else if (strncmp(opt, "low_log:", 8) == 0) {
		debug_low_log = strncmp(opt + 8, "1", 1) == 0;
		ODDMRFLOW_LOG("debug_low_log = %d\n", debug_low_log);
	} else if (strncmp(opt, "dump_en:", 8) == 0) {
		g_oddmr_dump_en = strncmp(opt + 8, "1", 1) == 0;
		ODDMRFLOW_LOG("g_oddmr_dump_en = %d\n", g_oddmr_dump_en);
	} else if (strncmp(opt, "ddren:", 6) == 0) {
		g_oddmr_ddren = strncmp(opt + 6, "1", 1) == 0;
		ODDMRFLOW_LOG("g_oddmr_ddren = %d\n", g_oddmr_ddren);
	} else if (strncmp(opt, "hrt:", 4) == 0) {
		g_oddmr_hrt_en = strncmp(opt + 4, "1", 1) == 0;
		ODDMRFLOW_LOG("g_oddmr_hrt_en = %d\n", g_oddmr_hrt_en);
	} else if (strncmp(opt, "od_support:", 11) == 0) {
		is_oddmr_od_support = strncmp(opt + 11, "1", 1) == 0;
		ODDMRFLOW_LOG("od_support = %d\n", is_oddmr_od_support);
	} else if (strncmp(opt, "dmr_support:", 12) == 0) {
		is_oddmr_dmr_support = strncmp(opt + 12, "1", 1) == 0;
		ODDMRFLOW_LOG("dmr_support = %d\n", is_oddmr_dmr_support);
	} else if (strncmp(opt, "sof_stop", 8) == 0) {
		kthread_stop(oddmr_sof_irq_event_task);
		ODDMRFLOW_LOG("sof_stop\n");
	} else if (strncmp(opt, "check_trigger:", 14) == 0) {
		unsigned int on, ret;

		ret = sscanf(opt, "check_trigger:%u\n", &on);
		if (ret != 1) {
			ODDMRFLOW_LOG("error to parse cmd %s\n", opt);
			return;
		}
		ODDMRFLOW_LOG("check_trigger = %u\n", on);
		g_od_check_trigger = on;
	} else if (strncmp(opt, "od_merge_lines:", 15) == 0) {
		unsigned int lines, ret;

		ret = sscanf(opt, "od_merge_lines:%u\n", &lines);
		if (ret != 1) {
			ODDMRFLOW_LOG("error to parse cmd %s\n", opt);
			return;
		}
		ODDMRFLOW_LOG("od_merge_lines = %u\n", lines);
		g_od_udma_merge_lines = lines;
	} else if (strncmp(opt, "oddmr_err_trigger:", 18) == 0) {
		unsigned int val, ret;

		ret = sscanf(opt, "oddmr_err_trigger_mask:%u\n", &val);
		if (ret != 1) {
			ODDMRFLOW_LOG("error to parse cmd %s\n", opt);
			return;
		}
		ODDMRFLOW_LOG("oddmr_err_trigger_mask = %u\n", val);
		oddmr_err_trigger_mask = val;
	} else if (strncmp(opt, "debugdump:", 10) == 0) {
		ODDMRFLOW_LOG("debug_flow_log = %d\n", debug_flow_log);
		ODDMRFLOW_LOG("debug_api_log = %d\n", debug_api_log);
		ODDMRFLOW_LOG("debug_low_log = %d\n", debug_low_log);
	}
}

static inline int mtk_oddmr_gain_interpolation(int left_item,
		int tmp_item, int right_item, int left_value, int right_value)
{
	int result;

	if (right_item == left_item)
		return left_value;

	result = (100 * (tmp_item - left_item) / (right_item - left_item) *
			(right_value - left_value) + 100 * left_value)/100;
	return result;
}

static void mtk_oddmr_od_free_buffer(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);

	ODDMRAPI_LOG("+\n");
	if (priv->od_data.r_channel != NULL) {
		mtk_drm_gem_free_object(&priv->od_data.r_channel->base);
		priv->od_data.r_channel = NULL;
	}
	if (priv->od_data.g_channel != NULL) {
		mtk_drm_gem_free_object(&priv->od_data.g_channel->base);
		priv->od_data.g_channel = NULL;
	}
	if (priv->od_data.b_channel != NULL) {
		mtk_drm_gem_free_object(&priv->od_data.b_channel->base);
		priv->od_data.b_channel = NULL;
	}
}
/* channel: 0:B 1:G 2:R */
static int mtk_oddmr_od_get_bpc(uint32_t od_mode, uint32_t channel)
{
	int bpc = 0;

	ODDMRAPI_LOG("+\n");
	switch (od_mode) {
	case OD_MODE_TYPE_RGB444:
	case OD_MODE_TYPE_COMPRESS_12:
		bpc = 4;
		break;
	case OD_MODE_TYPE_RGB565:
		if (channel == 1)
			bpc = 6;
		else
			bpc = 5;
		break;
	case OD_MODE_TYPE_COMPRESS_18:
		bpc = 6;
		break;
	case OD_MODE_TYPE_RGB555:
		bpc = 5;
		break;
	case OD_MODE_TYPE_RGB888:
		bpc = 8;
		break;
	default:
		bpc = 4;
		break;
	}
	return bpc;
}

/*
 * hscaling : 1,2,4; vscaling: 1,2
 * Width = align_up(Width, 8)
 * channel size = align_up(Width / hscaling x bpc, 128) x Height / vscaling / 8
 */
static uint32_t mtk_oddmr_od_get_data_size(uint32_t width, uint32_t height,
		uint32_t scaling_mode, uint32_t od_mode, uint32_t channel)
{
	uint32_t hscaling, h_2x4x_sel, vscaling, bpc, size, width_up;

	ODDMRAPI_LOG("+\n");
	bpc = mtk_oddmr_od_get_bpc(od_mode, channel);
	hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
	vscaling = (scaling_mode & BIT(1)) > 0 ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
	hscaling = hscaling * h_2x4x_sel;
	width_up = DIV_ROUND_UP(width, 8) * 8;
	size = width / hscaling * bpc;
	size = DIV_ROUND_UP(size, OD_H_ALIGN_BITS);
	size = size * OD_H_ALIGN_BITS * height / vscaling / 8;

	ODDMRLOW_LOG("width %u width_up %u hscaling %u vscaling %u bpc %u\n",
		width, width_up, hscaling, vscaling, bpc);
	ODDMRLOW_LOG("table mode %u channel %u data size %u\n", od_mode, channel, size);
	return size;
}

/*
 * hscaling : 1,2,4; vscaling: 1,2
 * dram_ln_beats = roundup(merge_width*bpc/hscaling/128)
 * long_burst = dram_ln_beats / 16
 * short_burst = dram_ln_beats % 16 > 0 ? 1:0
 * short_size = dram_ln_beats % 16
 * 1. Height / vscaling % merge_lines == 0
 * 2. short_size >= 4 long_burst >= 1x16 + 10
 * 3. select merge_lines with max avg effeciency, min lines
 */
static uint32_t mtk_oddmr_od_get_udma_effi(uint32_t dram_ln_beats)
{
	uint32_t effi_avg, effi_long, effi_short, long_burst, short_burst, short_size;

	effi_long = g_od_udma_effi[MAX_LONG_BURST_SIZE - 1];
	long_burst = dram_ln_beats / MAX_LONG_BURST_SIZE;
	short_burst = dram_ln_beats % MAX_LONG_BURST_SIZE > 0 ? 1 : 0;
	if (short_burst > 0) {
		short_size = dram_ln_beats % 16;
		effi_short = g_od_udma_effi[short_size - 1];
	} else {
		short_size = 0;
		effi_short = 0;
	}
	effi_avg = (effi_long * long_burst + short_burst * effi_short) / (long_burst + short_burst);
	if (short_size < 4 || long_burst < 1 || (long_burst == 1 && short_size < 10)) {
		ODDMRLOW_LOG("beats %u, burst(16x%u+%u) effi %u skip\n",
			dram_ln_beats, long_burst, short_size, effi_avg);
		effi_avg = 0;
	}
	return effi_avg;
}

static uint32_t mtk_oddmr_od_find_merge_lines(struct mtk_ddp_comp *comp, uint32_t width,
	uint32_t height, uint32_t hscaling, uint32_t vscaling, uint32_t bpc)
{
	uint32_t merge_width, dram_ln_beats, merge_lines;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	if (oddmr_priv == NULL)
		return 1;
	if (g_od_udma_merge_lines != 0)
		merge_lines = g_od_udma_merge_lines;
	else if (oddmr_priv->data->is_od_merge_lines == false)
		merge_lines = 1;
	else {
		int i, lines, min_lines = 1, effi = 0, max_effi = 0;

		for (i = 2; i < sizeof(g_od_udma_merge_lines_cand) / sizeof(uint32_t); i++) {
			lines = g_od_udma_merge_lines_cand[i];
			if (height / vscaling % lines != 0)
				continue;
			merge_width = DIV_ROUND_UP(width * lines, 8) * 8;
			dram_ln_beats = merge_width * bpc / hscaling;
			dram_ln_beats = DIV_ROUND_UP(dram_ln_beats, 128);
			effi = mtk_oddmr_od_get_udma_effi(dram_ln_beats);
			// will choose more lines if effi raise more than 400
			if (max_effi + 400 < effi) {
				max_effi = effi;
				min_lines = lines;
			}
			ODDMRLOW_LOG("merge_width %u beats %u, (%u, %u),max (%u, %u)\n",
				merge_width, dram_ln_beats, lines, effi, min_lines, max_effi);
		}
		merge_lines = min_lines;
		ODDMRAPI_LOG("find merge_lines %u with effi %u\n", min_lines, max_effi);
	}
	return merge_lines;
}
/*
 * hscaling : 1,2,4; vscaling: 1,2
 * merge_width = align_up((w+gb)*merge_lines, 8)
 * merge_lines = 8,10,20
 * dram_ln_beats = roundup(merge_width*bpc/hscaling/128)
 * dram_ln_beats_aligned = align_up(dram_ln_beats, 16)
 * ln_offset = dram_ln_beats_aligned * 128 / bpc
 */
static uint32_t mtk_oddmr_od_get_ln_offset(struct mtk_ddp_comp *comp, uint32_t width,
		uint32_t height, uint32_t scaling_mode, uint32_t od_mode, uint32_t channel)
{
	uint32_t hscaling, h_2x4x_sel, vscaling, bpc, merge_width, merge_lines;
	uint32_t dram_ln_beats, dram_ln_beats_aligned, ln_offset;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	if (oddmr_priv == NULL)
		return 0;
	bpc = mtk_oddmr_od_get_bpc(od_mode, channel);
	hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
	vscaling = (scaling_mode & BIT(1)) > 0 ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
	hscaling = hscaling * h_2x4x_sel;
	merge_lines = mtk_oddmr_od_find_merge_lines(comp, width, height, hscaling, vscaling, bpc);

	merge_width = DIV_ROUND_UP(width * merge_lines, 8) * 8;
	dram_ln_beats = merge_width * bpc / hscaling;
	dram_ln_beats = DIV_ROUND_UP(dram_ln_beats, 128);
	dram_ln_beats_aligned = DIV_ROUND_UP(dram_ln_beats, 16) * 16;
	ln_offset = dram_ln_beats_aligned * 128 / bpc;

	oddmr_priv->od_data.ln_offset = ln_offset;
	oddmr_priv->od_data.merge_lines = merge_lines;
	ODDMRAPI_LOG("width %u merge_width %u hscaling %u vscaling %u bpc %u\n",
		width, merge_width, hscaling, vscaling, bpc);
	ODDMRAPI_LOG("table mode %u channel %u\n", od_mode, channel);
	ODDMRAPI_LOG("merge_lines %u dram_ln_beats %u dram_ln_beats_aligned %u ln_offset %u\n",
		merge_lines, dram_ln_beats, dram_ln_beats_aligned, ln_offset);
	return ln_offset;
}

/*
 * hscaling : 1,2,4; vscaling: 1,2
 * merge_width = align_up((w+gb)*merge_lines, 8)
 * merge_lines = 8,10,20
 * dram_ln_beats = roundup(merge_width*bpc/hscaling/128)
 * dram_ln_beats_aligned = align_up(dram_ln_beats, 16)
 * ln_offset = dram_ln_beats_aligned * 128 / bpc
 * channel size = dram_ln_beats_aligned*128*Height/vscaling/merge_line/8
 */
static uint32_t mtk_oddmr_od_get_dram_size(struct mtk_ddp_comp *comp, uint32_t width,
		uint32_t height, uint32_t scaling_mode, uint32_t od_mode, uint32_t channel)
{
	uint32_t hscaling, h_2x4x_sel, vscaling, bpc, size, merge_width, merge_lines;
	uint32_t dram_ln_beats, dram_ln_beats_aligned, ln_offset;

	bpc = mtk_oddmr_od_get_bpc(od_mode, channel);
	hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
	vscaling = (scaling_mode & BIT(1)) > 0 ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
	hscaling = hscaling * h_2x4x_sel;
	merge_lines = mtk_oddmr_od_find_merge_lines(comp, width, height, hscaling, vscaling, bpc);
	merge_width = DIV_ROUND_UP(width * merge_lines, 8) * 8;
	dram_ln_beats = merge_width * bpc / hscaling;
	dram_ln_beats = DIV_ROUND_UP(dram_ln_beats, 128);
	dram_ln_beats_aligned = DIV_ROUND_UP(dram_ln_beats, 16) * 16;
	ln_offset = dram_ln_beats_aligned * 128 / bpc;
	size = dram_ln_beats_aligned * 128 * height / vscaling / merge_lines / 8;

	ODDMRFLOW_LOG("width %u merge_width %u hscaling %u vscaling %u bpc %u\n",
		width, merge_width, hscaling, vscaling, bpc);
	ODDMRFLOW_LOG("table mode %u channel %u dram size %u\n", od_mode, channel, size);
	ODDMRFLOW_LOG("dram_ln_beats %u dram_ln_beats_aligned %u ln_offset %u\n",
		dram_ln_beats, dram_ln_beats_aligned, ln_offset);
	return size;
}
/*
 *od_dummy_0 = bit0~bit4,bit7 always 1
 *hscaling = hsacling_enable ? (h_2x4x_sel ? 4 : 2) : 1
 *hsize_align = hscaling_en ? (h_2x4x_sel ?
 *      DIV_ROUND_UP(width, 8) * 8 : DIV_ROUND_UP(width, 4) * 4) : width;
 *hsize_scale = hsize_align / hscaling
 *od_dummy_1 = hsize_scale * merge_line
 *vsize_scale = height / vscaling;
 *od_dummy_2 = vsize_scale / merge_lines;
 *od_dummy_3 = merge_line - 1
 */
static void mtk_oddmr_od_dummy(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t hscaling, h_2x4x_sel, vscaling, hsize_align, hsize_scale, scaling_mode;
	uint32_t hscaling_en, merge_lines, value, mask, width, height, vsize_scale;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	struct mtk_drm_private *priv = default_comp->mtk_crtc->base.dev->dev_private;

	ODDMRAPI_LOG("+\n");
	if (priv->data->mmsys_id != MMSYS_MT6989)
		return;

	if (oddmr_priv == NULL)
		return;

	width = oddmr_priv->cfg.comp_in_width;
	height = oddmr_priv->cfg.height;
	scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;

	ODDMRFLOW_LOG("scaling_mode %u,width %u\n",scaling_mode,width);
	hscaling_en = (scaling_mode & BIT(0)) ? 1 : 0;
	ODDMRFLOW_LOG("hscaling_en %u\n",hscaling_en);
	vscaling = (scaling_mode & BIT(1)) ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) ? 1 : 0;
	hscaling = hscaling_en ? (h_2x4x_sel ? 4 : 2) : 1;

	hsize_align = hscaling_en ?
		(h_2x4x_sel ?
		DIV_ROUND_UP(width, 8) * 8 : DIV_ROUND_UP(width, 4) * 4)
		: width;
	hsize_scale = hsize_align / hscaling;
	merge_lines = oddmr_priv->od_data.merge_lines;

	ODDMRFLOW_LOG("hscaling %u h_2x4x_sel %u hsize_align %u hsize_scale %u merge_lines %u\n",
		hscaling, h_2x4x_sel, hsize_align, hsize_scale, merge_lines);
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1, REG_CUR_LINE_BUFFER_DE_2P_ALIGN);
	SET_VAL_MASK(value, mask, 1, REG_UDMA_HSIZE_USER_MODE);
	SET_VAL_MASK(value, mask, 1, REG_UDMA_VSIZE_USER_MODE);
	SET_VAL_MASK(value, mask, 1, REG_HVSP2UDMA_W_LAST_MASK);
	SET_VAL_MASK(value, mask, 1, REG_BUF_LV_INVERSE);
	SET_VAL_MASK(value, mask, 1, REG_BYPASS_MIU2SMI_W_BRIDGE);
	mtk_oddmr_write(comp, value, DISP_ODDMR_OD_DUMMY_0, pkg);

	value = hsize_scale * merge_lines;
	mtk_oddmr_write(comp, value, DISP_ODDMR_OD_DUMMY_1, pkg);
	vsize_scale = height / vscaling;
	value = vsize_scale / merge_lines;
	ODDMRFLOW_LOG("vsize_scale %u,vscaling %u\n",vsize_scale,vscaling);
	mtk_oddmr_write(comp, value, DISP_ODDMR_OD_DUMMY_2, pkg);
	value = merge_lines - 1;
	mtk_oddmr_write(comp, value, DISP_ODDMR_OD_DUMMY_3, pkg);
}

static uint32_t mtk_oddmr_od_find_max_dram_size(struct mtk_ddp_comp *comp,
	uint32_t scaling_mode, uint32_t od_mode)
{
	uint32_t width = 0, height = 0, tile_overhead, size_max = 0, size;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	tile_overhead = g_oddmr_priv->cfg.comp_overhead;
	if (mtk_crtc->avail_modes_num > 0) {
		int i;

		for (i = 0; i < mtk_crtc->avail_modes_num; i++) {
			struct drm_display_mode *mode = &mtk_crtc->avail_modes[i];

			if (mtk_crtc->is_dual_pipe)
				width = mode->hdisplay / 2 + tile_overhead;
			else
				width = mode->hdisplay;
			height = mode->vdisplay;
			size = mtk_oddmr_od_get_dram_size(comp, width, height,
							scaling_mode, od_mode, 1);
			if (size_max < size)
				size_max = size;
		}
	} else {
		size_max = 0;
		DDPMSG("invalid display mode\n");
	}
	ODDMRFLOW_LOG("find size_max %u\n", size_max);
	return size_max;
}
static void mtk_oddmr_od_alloc_dram_dual(void)
{

	uint32_t scaling_mode, size_max, od_mode;
	bool secu;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		DDPPR_ERR("%s comp is invalid\n", __func__);
		return;
	}
	scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;
	od_mode = g_od_param.od_basic_info.basic_param.od_mode;
	size_max = mtk_oddmr_od_find_max_dram_size(default_comp, scaling_mode, od_mode);
	if (size_max == 0)
		return;
	secu = mtk_oddmr_is_svp_on_mtee();
	//od do not support secu for short of secu mem
	if (g_oddmr_priv->data != NULL && !g_oddmr_priv->data->is_od_support_sec)
		secu = false;
	//TODO check size, should not be too big
	mtk_oddmr_od_free_buffer(default_comp);
	g_oddmr_priv->od_data.b_channel =
		mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_max, NULL, secu);
	g_oddmr_priv->od_data.g_channel =
		mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_max, NULL, secu);
	g_oddmr_priv->od_data.r_channel =
		mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_max, NULL, secu);
	if (default_comp->mtk_crtc->is_dual_pipe) {
		/* non secure */
		mtk_oddmr_od_free_buffer(oddmr1_default_comp);
		g_oddmr1_priv->od_data.b_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_max, NULL, secu);
		g_oddmr1_priv->od_data.g_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_max, NULL, secu);
		g_oddmr1_priv->od_data.r_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_max, NULL, secu);
	}
}

static void mtk_oddmr_od_set_dram(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	dma_addr_t addr = 0;
	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);

	ODDMRAPI_LOG("+\n");
	if ((priv->od_data.r_channel != NULL) &&
			(priv->od_data.g_channel != NULL) &&
			(priv->od_data.b_channel != NULL)) {
		addr = priv->od_data.r_channel->dma_addr;
		mtk_oddmr_write_mask(comp, addr >> 4, DISP_ODDMR_OD_BASE_ADDR_R_LSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_R_LSB), pkg);
		mtk_oddmr_write_mask(comp, addr >> 20, DISP_ODDMR_OD_BASE_ADDR_R_MSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_R_MSB), pkg);
		addr = priv->od_data.g_channel->dma_addr;
		mtk_oddmr_write_mask(comp, addr >> 4, DISP_ODDMR_OD_BASE_ADDR_G_LSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_G_LSB), pkg);
		mtk_oddmr_write_mask(comp, addr >> 20, DISP_ODDMR_OD_BASE_ADDR_G_MSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_G_MSB), pkg);
		addr = priv->od_data.b_channel->dma_addr;
		mtk_oddmr_write_mask(comp, addr >> 4, DISP_ODDMR_OD_BASE_ADDR_B_LSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_B_LSB), pkg);
		mtk_oddmr_write_mask(comp, addr >> 20, DISP_ODDMR_OD_BASE_ADDR_B_MSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_B_MSB), pkg);
	} else {
		DDPPR_ERR("%s buffer is invalid\n", __func__);
	}
}

static void mtk_oddmr_od_set_dram_dual(struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	mtk_oddmr_od_set_dram(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_set_dram(oddmr1_default_comp, pkg);
}

static int mtk_oddmr_od_init_sram(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, int table_idx, int sram_idx)
{
	struct mtk_oddmr_od_table *table;
	struct mtk_oddmr_pq_pair *param_pq;
	uint8_t *raw_table, tmp_data;
	int channel, srams, cols, rows, raw_idx, i, change_channel;
	uint32_t value, mask, tmp_r_sel, tmp_w_sel, table_size;
	uint32_t sram_write_change;
	struct mtk_drm_private *priv = default_comp->mtk_crtc->base.dev->dev_private;

	ODDMRAPI_LOG("+\n");
	if (!IS_TABLE_VALID(table_idx, g_od_param.valid_table)) {
		DDPPR_ERR("%s table %d is invalid\n", __func__, table_idx);
		return -EFAULT;
	}
	table = g_od_param.od_tables[table_idx];
	param_pq = table->pq_od.param;
	raw_table = table->raw_table.value;
	table_size = table->raw_table.size;
	raw_idx = 0;
	if (table_size < 33 * 33 * 3) {
		DDPPR_ERR("%s table%d size %u is too small\n", __func__, table_idx, table_size);
		return -EFAULT;
	}
	for (i = 0; i < table->pq_od.counts; i++)
		mtk_oddmr_write(comp, param_pq[i].value, param_pq[i].addr, pkg);

	if(priv->data->mmsys_id == MMSYS_MT6985 ||
		priv->data->mmsys_id == MMSYS_MT6989)
		sram_write_change = 1;
	else
		sram_write_change = 0;

	/* B:0-bit1, G:1-bit2, R:2-bit3*/
	for (channel = 0; channel < 3; channel++) {
		/* 1: 17x17 2:16x17 3:17x16 4:16x16*/

		//change begin
		//B:0-bit1, G:1-bit2, R:2-bit3 -->> B:2-bit1, G:1-bit2, R:0-bit3
		if(sram_write_change)
			change_channel = channel - (channel-1)*2;
		else
			change_channel = channel;
		//change end

		for (srams = 1; srams < 5; srams++) {
			value = 0;
			mask = 0;
			tmp_w_sel = sram_idx;
			tmp_r_sel = !sram_idx;
			SET_VAL_MASK(value, mask, 1 << (change_channel  + 1), REG_WBGR_OD_SRAM_IO_EN);
			SET_VAL_MASK(value, mask, 0, REG_AUTO_SRAM_ADR_INC_EN);
			SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
			SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
			mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, pkg);
			rows = (srams < 3) ? 17 : 16;
			cols = (srams % 2 == 1) ? 17 : 16;
			ODDMRFLOW_LOG("channel%d sram%d size %dx%d\n", change_channel , srams, rows, cols);
			for (i = 0; i < rows * cols; i++) {
				tmp_data = raw_table[raw_idx];
				raw_idx++;
				mtk_oddmr_write(comp, tmp_data,
					(DISP_ODDMR_OD_SRAM_CTRL_2 + 12 * (srams - 1)), pkg);
				mtk_oddmr_write(comp, 0x8000 | (i & 0x1FF),
					(DISP_ODDMR_OD_SRAM_CTRL_1 + 12 * (srams - 1)), pkg);
			}
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, 0, REG_WBGR_OD_SRAM_IO_EN);
			SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
			SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
			SET_VAL_MASK(value, mask, 0, REG_AUTO_SRAM_ADR_INC_EN);
			mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, pkg);
		}
		//SET_VAL_MASK(value, mask, 0, REG_FLD(1, channel + 1));
		//mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, pkg);
		//mtk_oddmr_write(comp, 0, DISP_ODDMR_OD_SRAM_CTRL_0, pkg);
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
		SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
		mtk_oddmr_write(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);

	}
	return 0;
}

int mtk_oddmr_od_init_sram_dual(struct cmdq_pkt *pkg, int table_idx, int sram_idx)
{
	int ret;

	ret = mtk_oddmr_od_init_sram(default_comp, pkg, table_idx, sram_idx);
	if (ret >= 0)
		g_oddmr_priv->od_data.od_sram_table_idx[sram_idx] = table_idx;
	if (default_comp->mtk_crtc->is_dual_pipe) {
		ret = mtk_oddmr_od_init_sram(oddmr1_default_comp, pkg, table_idx, sram_idx);
		if (ret >= 0)
			g_oddmr1_priv->od_data.od_sram_table_idx[sram_idx] = table_idx;
	}
	ODDMRAPI_LOG("sram_idx %d, table_idx %d\n",
			sram_idx, g_oddmr_priv->od_data.od_sram_table_idx[sram_idx]);
	return ret;
}

static int _mtk_oddmr_od_table_lookup(struct mtk_disp_oddmr *priv,
		struct mtk_oddmr_timing *cur_timing)
{
	int idx;
	unsigned int fps = cur_timing->vrefresh;
	unsigned int dbv = cur_timing->bl_level;
	uint32_t cnts = g_od_param.od_basic_info.basic_param.table_cnt;

	for (idx = 0; idx < cnts; idx++) {
		if ((IS_TABLE_VALID(idx, g_od_param.valid_table)) &&
			(fps >= g_od_param.od_tables[idx]->table_basic_info.min_fps) &&
			(fps <= g_od_param.od_tables[idx]->table_basic_info.max_fps) &&
			(!priv->data->is_od_4_table || (priv->data->is_od_4_table &&
			(dbv >= g_od_param.od_tables[idx]->table_basic_info.min_dbv) &&
			(dbv <= g_od_param.od_tables[idx]->table_basic_info.max_dbv)))) {
			ODDMRFLOW_LOG("table_idx %d,is_4_tab:%d,min_fps:%d,max_fps:%d,min_dbv:%d,max_dbv:%d\n", idx,
			priv->data->is_od_4_table,
			g_od_param.od_tables[idx]->table_basic_info.min_fps,
			g_od_param.od_tables[idx]->table_basic_info.max_fps,
			g_od_param.od_tables[idx]->table_basic_info.min_dbv,
			g_od_param.od_tables[idx]->table_basic_info.max_dbv);
		break;
		}
	}

	if ((idx == cnts) ||
			!IS_TABLE_VALID(idx, g_od_param.valid_table)) {
		ODDMRFLOW_LOG("not find table!\n");
		idx = -1;
	}
	ODDMRFLOW_LOG("table_idx %d\n", idx);
	return idx;
}
/*
 *table_idx: output table_idx found
 *return 0: state cur sram, 1: flip sram, 2: update sram table
 */
static int mtk_oddmr_od_table_lookup(struct mtk_disp_oddmr *priv,
		struct mtk_oddmr_timing *cur_timing, int *table_idx)
{
	int tmp_table_idx, tmp1_table_idx, tmp_sram_idx, ret;
	int sram_table_idx, sram1_table_idx;

	ODDMRAPI_LOG("fps %u,dbv %d\n", cur_timing->vrefresh, cur_timing->bl_level);
	if (priv->od_data.od_sram_read_sel < 0 ||
			priv->od_data.od_sram_table_idx[0] < 0) {
		DDPPR_ERR("%s od is not init properly\n", __func__);
		return -EFAULT;
	}
	tmp_sram_idx = priv->od_data.od_sram_read_sel;
	ODDMRFLOW_LOG("%d,tmp_sram_idx %d\n", __LINE__,tmp_sram_idx);
	sram_table_idx = priv->od_data.od_sram_table_idx[!!tmp_sram_idx];
	sram1_table_idx = priv->od_data.od_sram_table_idx[!tmp_sram_idx];
	ODDMRFLOW_LOG("%d,sram %d,sram1 %d\n", __LINE__,sram_table_idx,sram1_table_idx);
	tmp_table_idx = priv->od_data.od_dram_sel[sram_table_idx];
	tmp1_table_idx = priv->od_data.od_dram_sel[sram1_table_idx];
	ODDMRFLOW_LOG("%d,dram %d,dram1 %d\n", __LINE__,tmp_table_idx,tmp1_table_idx);
	/* best stay at current table */
	if ((IS_TABLE_VALID(tmp_table_idx, g_od_param.valid_table)) &&
	(cur_timing->vrefresh >= g_od_param.od_tables[tmp_table_idx]->table_basic_info.min_fps) &&
	(cur_timing->vrefresh <= g_od_param.od_tables[tmp_table_idx]->table_basic_info.max_fps) &&
	(!priv->data->is_od_4_table || //if is_od_4_table = 0 always true, if is_od_4_table = 1, need judge bl_level
	(priv->data->is_od_4_table &&
	(cur_timing->bl_level >= g_od_param.od_tables[tmp_table_idx]->table_basic_info.min_dbv) &&
	(cur_timing->bl_level <= g_od_param.od_tables[tmp_table_idx]->table_basic_info.max_dbv)))) {
		*table_idx = tmp_table_idx;
		return 0;
	}
	//if od in updating, used old table before updata table finish
	if (g_oddmr_priv->od_state == ODDMR_TABLE_UPDATING) {
		ODDMRFLOW_LOG("ODDMR_TABLE_UPDATING\n");
		*table_idx = tmp_table_idx;
		return 0;
	}
	ODDMRFLOW_LOG("%d\n", __LINE__);
	/* second best just flip sram */
	if ((IS_TABLE_VALID(tmp1_table_idx, g_od_param.valid_table)) &&
	(cur_timing->vrefresh >= g_od_param.od_tables[tmp1_table_idx]->table_basic_info.min_fps) &&
	(cur_timing->vrefresh <= g_od_param.od_tables[tmp1_table_idx]->table_basic_info.max_fps) &&
	(!priv->data->is_od_4_table || //if is_od_4_table = 0 always true, if is_od_4_table = 1, need judge bl_level
	(priv->data->is_od_4_table &&
	(cur_timing->bl_level >= g_od_param.od_tables[tmp1_table_idx]->table_basic_info.min_dbv) &&
	(cur_timing->bl_level <= g_od_param.od_tables[tmp1_table_idx]->table_basic_info.max_dbv)))) {
		*table_idx = tmp1_table_idx;
		CRTC_MMP_MARK(0, oddmr_ctl, 0xf, tmp1_table_idx);
		return 1;
	}
	ODDMRFLOW_LOG("%d\n", __LINE__);

	/* worst case should update table */
	ret = _mtk_oddmr_od_table_lookup(priv,cur_timing);
	if (ret >= 0 && priv->data->is_od_4_table) {
		*table_idx = tmp_table_idx; //used table not change before updata table finish
		g_od_param.updata_dram_table = ret;
		ODDMRFLOW_LOG("update table fps %u, vrefresh %u, table_idx %d sram(%d %d)\n",
				cur_timing->vrefresh, cur_timing->vrefresh,
				ret, priv->od_data.od_dram_sel[0],
				priv->od_data.od_dram_sel[1]);
		CRTC_MMP_MARK(0, oddmr_ctl, 0xc, tmp_table_idx);
		return 2;
	}
	ODDMRFLOW_LOG("fps %u out of range\n", cur_timing->vrefresh);
	return -EFAULT;
}


/*
 * return int for further calculate
 */
static int mtk_oddmr_common_gain_lookup(int item, void *table, uint32_t cnt)
{
	int i, left_value, right_value, result, left_item, right_item, tmp_item;
	struct mtk_oddmr_table_gain *gain_table;

	ODDMRAPI_LOG("cnt %u\n", cnt);
	gain_table = (struct mtk_oddmr_table_gain *)table;
	tmp_item = item;
	if (tmp_item < gain_table[0].item) {
		if (tmp_item != 0)
			ODDMRFLOW_LOG("item %d outof range (%u, %u)\n", tmp_item,
					gain_table[0].item, gain_table[cnt - 1].item);
		result = 0;
		return result;
	}
	if (tmp_item == gain_table[0].item) {
		result = gain_table[0].value;
		ODDMRFLOW_LOG("L:(%d,%d) V:(%d,%d)\n",
			gain_table[0].item, gain_table[0].value, tmp_item, result);
		return result;
	}
	for (i = 1; i < cnt; i++) {
		if (tmp_item <= gain_table[i].item)
			break;
	}
	if (i >= cnt) {
		result = 0;
		ODDMRFLOW_LOG("item %u outof range (%u, %u)\n", tmp_item,
				gain_table[0].item, gain_table[cnt - 1].item);
	} else {
		//to cover negative value in calculate
		left_value = (int)gain_table[i - 1].value;
		right_value = (int)gain_table[i].value;
		left_item = (int)gain_table[i - 1].item;
		right_item = (int)gain_table[i].item;
		result = mtk_oddmr_gain_interpolation(left_item, tmp_item, right_item,
				left_value, right_value);
		ODDMRFLOW_LOG("idx %d L:(%d,%d),R:(%d,%d) V:(%d,%d)\n", i,
				left_item, left_value, right_item, right_value, tmp_item, result);
	}
	return result;
}

static int mtk_oddmr_od_gain_lookup(struct mtk_ddp_comp *comp,
	uint32_t fps, uint32_t dbv, int table_idx, uint32_t *weight)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	int result_fps, result_dbv, tmp_item;
	uint32_t cnt, result;
	uint32_t user_gain = oddmr_priv->od_user_gain;
	struct mtk_oddmr_table_gain *bl_gain_table;
	struct mtk_oddmr_table_gain *fps_gain_table;

	ODDMRAPI_LOG("fps %u, dbv %u, table%d\n", fps, dbv, table_idx);
	if (!IS_TABLE_VALID(table_idx, g_od_param.valid_table)) {
		*weight = 0;
		DDPPR_ERR("%s table%d is invalid\n", __func__, table_idx);
		return -EFAULT;
	}
	/*fps*/
	cnt = g_od_param.od_tables[table_idx]->fps_cnt;
	fps_gain_table = g_od_param.od_tables[table_idx]->fps_table;
	tmp_item = (int)fps;
	result_fps = mtk_oddmr_common_gain_lookup(tmp_item, fps_gain_table, cnt);
	/* dbv */
	cnt = g_od_param.od_tables[table_idx]->bl_cnt;
	bl_gain_table = g_od_param.od_tables[table_idx]->bl_table;
	tmp_item = (int)dbv;
	result_dbv = mtk_oddmr_common_gain_lookup(tmp_item, bl_gain_table, cnt);
	result = ((uint32_t)result_dbv * (uint32_t)result_fps + 32) / 64;
	result = (result * user_gain + 32) / 64;
	if (result > 255)
		result = 255;
	*weight = result;
	ODDMRAPI_LOG("dbv_gain %d, fps_gain %d, user_gain %d weight %d\n",
		result_dbv, result_fps, user_gain, result);
	return 0;
}

static void mtk_oddmr_od_smi(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t value, mask, buf_size;
	struct mtk_disp_oddmr *oddmr = comp_to_oddmr(comp);
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;
	if (priv->data->mmsys_id != MMSYS_MT6989)
		return;

	ODDMRAPI_LOG();

	value = 0;
	mask = 0;
	/* odr*/
	SET_VAL_MASK(value, mask, 4, REG_ODR_RE_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 0, REG_ODR_POACH_CFG_OFF);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODR_8, mask, pkg);
	buf_size = oddmr->data->odr_buffer_size;
	value = ODDMR_READ_IN_PRE_ULTRA(buf_size);//read in pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODR_0, 0xFFFF, pkg);
	value = ODDMR_READ_IN_ULTRA(buf_size);//read in ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODR_4, 0xFFFF, pkg);
	value = ODDMR_READ_OUT_PRE_ULTRA(buf_size);//read out pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODR_2, 0xFFFF, pkg);
	value = ODDMR_READ_OUT_ULTRA(buf_size);//read out ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODR_6, 0xFFFF, pkg);
	value = 0;
	mask = 0;
	/* odw*/
	SET_VAL_MASK(value, mask, 4, REG_ODW_WR_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 0, REG_ODW_POACH_CFG_OFF);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODW_8, mask, pkg);
	buf_size = oddmr->data->odw_buffer_size;
	value = ODDMR_WRITE_IN_PRE_ULTRA(buf_size);//write in pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODW_0, 0xFFFF, pkg);
	value = ODDMR_WRITE_IN_ULTRA(buf_size);//write in ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODW_4, 0xFFFF, pkg);
	value = ODDMR_WRITE_OUT_PRE_ULTRA(buf_size);//write out pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODW_2, 0xFFFF, pkg);
	value = ODDMR_WRITE_OUT_ULTRA(buf_size);//write out ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODW_6, 0xFFFF, pkg);
}


static void mtk_oddmr_dmr_smi(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t value, mask, buf_size;
	struct mtk_disp_oddmr *oddmr = comp_to_oddmr(comp);

	/* dmr */
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 4, REG_DMR_RE_ULTRA_MODE);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_DMR_8, mask, pkg);
	buf_size = oddmr->data->dmr_buffer_size;
	value = ODDMR_READ_IN_PRE_ULTRA(buf_size);//read in pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_1, 0xFFFF, pkg);
	value = ODDMR_READ_IN_ULTRA(buf_size);//read in ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_1, 0xFFFF, pkg);
	value = ODDMR_READ_OUT_PRE_ULTRA(buf_size);//read out pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_1, 0xFFFF, pkg);
	value = ODDMR_READ_OUT_ULTRA(buf_size);//read out ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_1, 0xFFFF, pkg);
}

static void mtk_oddmr_od_smi_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_smi(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_smi(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_dmr_smi_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_dmr_smi(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_dmr_smi(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_od_set_res_udma_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_set_res_udma(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_set_res_udma(oddmr1_default_comp, pkg);
}

//static void mtk_oddmr_dmr_free_table(struct mtk_ddp_comp *comp)
//{
//	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);
//
//	ODDMRAPI_LOG("+\n");
//	if (priv->dmr_data.mura_table != NULL) {
//		mtk_drm_gem_free_object(&priv->dmr_data.mura_table->base);
//		priv->dmr_data.mura_table = NULL;
//	}
//}

static int mtk_oddmr_dmr_alloc_table(struct mtk_drm_dmr_cfg_info *dmr_cfg_info)
{
	uint32_t size;
	void *addr ;
	int ret = -EFAULT;
	int i,j;

	ODDMRAPI_LOG("+\n");

	if(!dmr_cfg_info) {
		ODDMRFLOW_LOG("%s alloc dmr table fail\n", __func__);
		return ret;
	}

	size = dmr_cfg_info->table_index.table_byte_num;

	for(i=0;i < dmr_cfg_info->table_index.DBV_table_num ;i++) {
		for(j=0;j<dmr_cfg_info->table_index.FPS_table_num;j++) {
			if(dmr_cfg_info->table_index.DC_table_flag)
				addr = dmr_cfg_info->table_content.table_single_DC + i*j*size +j*size;
			else
				addr = dmr_cfg_info->table_content.table_single + i*j*size +j*size;
				ODDMRFLOW_LOG("load_buffer i:%d, j:%d\n", i,j);
			g_oddmr_priv->dmr_data.mura_table[i][j] = mtk_oddmr_load_buffer(
				&default_comp->mtk_crtc->base, size, addr, false);
			if(!(g_oddmr_priv->dmr_data.mura_table[i][j]))
				ODDMRFLOW_LOG("%s alloc dmr table fail dbv:%d,fps:%d\n", __func__, i,j);
		}
	}
	if(dmr_cfg_info->table_index.DC_table_flag) {
		if(dmr_cfg_info->table_content.table_single_DC)
			vfree(dmr_cfg_info->table_content.table_single_DC);
		else
			vfree(dmr_cfg_info->table_content.table_single);
	}
	ret = 0;
	return ret;
}

static int mtk_oddmr_dbi_fps_lookup(unsigned int fps, struct mtk_drm_dbi_cfg_info *cfg_info,
	unsigned int *fps_node)
{
	struct mtk_drm_dmr_fps_dbv_node *fps_dbv_node;
	unsigned int fps_node_tmp;
	int i;

	ODDMRAPI_LOG("+\n");

	if(!cfg_info) {
		ODDMRFLOW_LOG("dmr config info is NULL\n");
		return -1;
	}

	if(!(cfg_info->fps_dbv_node.FPS_node)) {
		ODDMRFLOW_LOG("dmr config info:FPS_node is NULL\n");
		return -1;
	}

	fps_dbv_node = &cfg_info->fps_dbv_node;

	if(fps_dbv_node->FPS_num == 0) {
		ODDMRFLOW_LOG("dmr fps node number is 0\n");
		return -1;
	}

	//dmr fps node lookup
	for(i = 0; i < fps_dbv_node->FPS_num; i++) {
		if(fps < fps_dbv_node->FPS_node[i])
			break;
	}

	if(i >= fps_dbv_node->FPS_num)
		fps_node_tmp = fps_dbv_node->FPS_num -1;
	else
		fps_node_tmp = (i>0)?(i-1):i;

	*fps_node = fps_node_tmp;

	for(i = 0; i < fps_dbv_node->FPS_num; i++)
		ODDMRFLOW_MSG("dbi_node_num:%d, fps_node%d:%d\n",
			fps_dbv_node->FPS_num, i, fps_dbv_node->FPS_node[i]);

	ODDMRFLOW_MSG("dbi_current_fps:%d,fps_node:%d\n",
		fps,*fps_node);

	return 0;
}


static int mtk_oddmr_dmr_fps_lookup(unsigned int fps, struct mtk_drm_dmr_cfg_info *cfg_info,
	unsigned int *fps_table_idx, unsigned int *fps_node)
{
	struct mtk_drm_dmr_fps_dbv_node *fps_dbv_node;
	struct mtk_drm_dmr_table_index *table_index;
	unsigned int fps_table_idx_tmp;
	unsigned int fps_node_tmp;
	int i;

	ODDMRAPI_LOG("+\n");

	if(!cfg_info) {
		ODDMRFLOW_LOG("dmr config info is NULL\n");
		return -1;
	}

	if(!(cfg_info->fps_dbv_node.FPS_node)) {
		ODDMRFLOW_LOG("dmr config info:FPS_node is NULL\n");
		return -1;
	}

	if(!(cfg_info->table_index.FPS_table_idx)) {
		ODDMRFLOW_LOG("dmr config info:FPS_table_idx is NULL\n");
		return -1;
	}

	fps_dbv_node = &cfg_info->fps_dbv_node;
	table_index = &cfg_info->table_index;

	if(fps_dbv_node->FPS_num == 0) {
		ODDMRFLOW_LOG("dmr fps node number is 0\n");
		return -1;
	}

	if(table_index->FPS_table_num == 0) {
		ODDMRFLOW_LOG("dmr fps table number is 0\n");
		return -1;
	}

	//dmr fps node lookup
	for(i = 0; i < fps_dbv_node->FPS_num; i++) {
		if(fps < fps_dbv_node->FPS_node[i])
			break;
	}

	if(i >= fps_dbv_node->FPS_num)
		fps_node_tmp = fps_dbv_node->FPS_num -1;
	else
		fps_node_tmp = (i>0)?(i-1):i;

	//dmr fps table lookup
	for(i = 0; i < table_index->FPS_table_num; i++) {
		if(fps < table_index->FPS_table_idx[i])
			break;
	}

	if(i >= table_index->FPS_table_num)
		fps_table_idx_tmp = table_index->FPS_table_num -1;
	else
		fps_table_idx_tmp = (i>0)?(i-1):i;

	*fps_node = fps_node_tmp;
	*fps_table_idx = fps_table_idx_tmp;

	for(i = 0; i < fps_dbv_node->FPS_num; i++)
		ODDMRFLOW_MSG("node_num:%d, fps_node%d:%d\n",
			fps_dbv_node->FPS_num, i, fps_dbv_node->FPS_node[i]);
	for(i = 0; i < table_index->FPS_table_num; i++)
		ODDMRFLOW_MSG("table_num:%d, table_idx%d:%d\n",
			table_index->FPS_table_num, i, table_index->FPS_table_idx[i]);
	ODDMRFLOW_MSG("current_fps:%d,fps_node:%d,fps_table_idx:%d\n",
		fps,*fps_node ,*fps_table_idx);

	return 0;
}

static int mtk_oddmr_dbi_dbv_lookup(unsigned int dbv, unsigned int *dbv_node_array, unsigned int dbv_node_num,
	unsigned int *dbv_node)
{
	unsigned int dbv_node_tmp;
	int i;

	ODDMRAPI_LOG("+\n");

	if(!dbv_node_array) {
		ODDMRFLOW_LOG("dbv_node_array is NULL\n");
		return -1;
	}

	//dbv mode lookup
	if(dbv_node_num == 0) {
		ODDMRFLOW_LOG("dbi dbv node number is 0\n");
		return -1;
	}

	//dbv mode lookup
	for(i = 0; i < dbv_node_num; i++) {
		if(dbv < dbv_node_array[i])
			break;
	}

	if(i >= dbv_node_num)
		dbv_node_tmp = dbv_node_num -1;
	else
		dbv_node_tmp = (i>0)?(i-1):i;

	*dbv_node = dbv_node_tmp;

	for(i = 0; i < dbv_node_num; i++)
		ODDMRFLOW_MSG("dbi_node_num:%d, dbv_node%d:%d\n",
			dbv_node_num, i, dbv_node_array[i]);

	ODDMRFLOW_MSG("dbi_current_dbv:%d,dbv_node:%d\n",
		dbv, *dbv_node);
	return 0;
}


static int mtk_oddmr_dmr_dbv_lookup(unsigned int dbv, struct mtk_drm_dmr_cfg_info *cfg_info,
	unsigned int *dbv_table_idx, unsigned int *dbv_node)
{
	struct mtk_drm_dmr_fps_dbv_node *fps_dbv_node;
	struct mtk_drm_dmr_table_index *table_index;
	unsigned int dbv_table_idx_tmp;
	unsigned int dbv_node_tmp;
	int i;

	ODDMRAPI_LOG("+\n");

	if(!cfg_info) {
		ODDMRFLOW_LOG("dmr config info is NULL\n");
		return -1;
	}

	if(!(cfg_info->fps_dbv_node.DBV_node)) {
		ODDMRFLOW_LOG("dmr config info:DBV_node is NULL\n");
		return -1;
	}

	if(!(cfg_info->table_index.DBV_table_idx)) {
		ODDMRFLOW_LOG("dmr config info:DBV_table_idx is NULL\n");
		return -1;
	}

	//dbv mode lookup
	fps_dbv_node = &cfg_info->fps_dbv_node;
	table_index = &cfg_info->table_index;
	if(fps_dbv_node->DBV_num == 0) {
		ODDMRFLOW_LOG("dmr dbv node number is 0\n");
		return -1;
	}

	if(table_index->DBV_table_num== 0) {
		ODDMRFLOW_LOG("dmr dbv table number is 0\n");
		return -1;
	}

	//dbv mode lookup
	for(i = 0; i < fps_dbv_node->DBV_num; i++) {
		if(dbv < fps_dbv_node->DBV_node[i])
			break;
	}

	if(i >= fps_dbv_node->DBV_num)
		dbv_node_tmp = fps_dbv_node->DBV_num -1;
	else
		dbv_node_tmp = (i>0)?(i-1):i;

	////dbv table lookup
	for(i = 0; i < table_index->DBV_table_num; i++) {
		if(dbv < table_index->DBV_table_idx[i])
			break;
	}

	if(i >= table_index->DBV_table_num)
		dbv_table_idx_tmp = table_index->DBV_table_num-1;
	else
		dbv_table_idx_tmp = (i>0)?(i-1):i;

	*dbv_node = dbv_node_tmp;
	*dbv_table_idx = dbv_table_idx_tmp;

	for(i = 0; i < fps_dbv_node->DBV_num; i++)
		ODDMRFLOW_MSG("node_num:%d, dbv_node%d:%d\n",
			fps_dbv_node->DBV_num, i, fps_dbv_node->DBV_node[i]);
	for(i = 0; i < table_index->DBV_table_num; i++)
		ODDMRFLOW_MSG("table_num:%d, table_idx%d:%d\n",
			table_index->DBV_table_num, i, table_index->DBV_table_idx[i]);
	ODDMRFLOW_MSG("current_dbv:%d,dbv_node:%d,dbv_table_idx:%d\n",
		dbv, *dbv_node, *dbv_table_idx);
	return 0;
}

static void mtk_oddmr_dbi_timing_chg_dual(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle)
{
	uint32_t dbv_node = 0;
	uint32_t fps_node = 0;
	uint32_t fps;
	int cur_tb;
	unsigned int gain_ratio;
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data = &g_oddmr_priv->dbi_cfg_info;
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data_tb1 = &g_oddmr_priv->dbi_cfg_info_tb1;

	ODDMRAPI_LOG("+\n");
	if (g_oddmr_priv->dbi_state >= ODDMR_INIT_DONE) {
		fps = timing->vrefresh;
		mtk_oddmr_dbi_fps_lookup(fps, &g_oddmr_priv->dbi_cfg_info,&fps_node);
		dbv_node = atomic_read(&g_oddmr_priv->dbi_data.cur_dbv_node);
		gain_ratio = atomic_read(&g_oddmr_priv->dbi_data.gain_ratio);

		cur_tb = atomic_read(&g_oddmr_priv->dbi_data.cur_table_idx);
		if(cur_tb)
			mtk_oddmr_dbi_gain_cfg(default_comp,
				handle, dbv_node, fps_node,
				dbi_cfg_data_tb1, gain_ratio);
		else
			mtk_oddmr_dbi_gain_cfg(default_comp,
				handle, dbv_node, fps_node,
				dbi_cfg_data, gain_ratio);
		atomic_set(&g_oddmr_priv->dbi_data.cur_fps_node, fps_node);

		ODDMRFLOW_LOG("dbi gain config: dbv_node:%d, fps_node:%d\n", dbv_node, fps_node);
	}
}


static void mtk_oddmr_dmr_timing_chg_dual(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle)
{


	uint32_t dbv_node = 0;
	uint32_t fps_node = 0;
	uint32_t dbv_table_idx = 0;
	uint32_t fps_table_idx = 0;
	uint32_t fps;
	dma_addr_t addr = 0;

	ODDMRAPI_LOG("+\n");
	if (g_oddmr_priv->dmr_state >= ODDMR_INIT_DONE) {
		fps = timing->vrefresh;
		mtk_oddmr_dmr_fps_lookup(fps, &g_oddmr_priv->dmr_cfg_info, &fps_table_idx,&fps_node);
		dbv_node = atomic_read(&g_oddmr_priv->dmr_data.cur_dbv_node);
		dbv_table_idx = atomic_read(&g_oddmr_priv->dmr_data.cur_dbv_table_idx);

		if(fps_node != atomic_read(&g_oddmr_priv->dmr_data.cur_fps_node)){
			mtk_oddmr_dmr_gain_cfg(default_comp,
				handle, dbv_node, fps_node,
				(struct mtk_drm_dmr_cfg_info *)&g_oddmr_priv->dmr_cfg_info);
			atomic_set(&g_oddmr_priv->dmr_data.cur_fps_node, fps_node);
		}

		if(fps_table_idx != atomic_read(&g_oddmr_priv->dmr_data.cur_fps_table_idx)) {
			addr = g_oddmr_priv->dmr_data.mura_table[dbv_table_idx][fps_table_idx]->dma_addr;
			mtk_oddmr_write(default_comp, addr >> 4, DISP_ODDMR_DMR_UDMA_CTR_4, NULL);
			mtk_oddmr_write(default_comp, addr >> 20, DISP_ODDMR_DMR_UDMA_CTR_5, NULL);
			atomic_set(&g_oddmr_priv->dmr_data.cur_fps_table_idx, fps_table_idx);
		}
		ODDMRFLOW_LOG("dmr gain config: dbv_node:%d, fps_node:%d\n", dbv_node, fps_node);
		ODDMRFLOW_LOG("dmr table cfg: dbv_table:%d, fps_table:%d\n", dbv_table_idx, fps_table_idx);
	}
}

static void mtk_oddmr_od_flip(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);
	int table_idx, tmp_sram_idx;
	uint32_t value, mask, r_sel, w_sel;

	ODDMRAPI_LOG("+\n");
	//flip
	priv->od_data.od_sram_read_sel = !priv->od_data.od_sram_read_sel;
	tmp_sram_idx = priv->od_data.od_sram_read_sel;
	table_idx = priv->od_data.od_dram_sel[priv->od_data.od_sram_table_idx[!!tmp_sram_idx]];
	r_sel = priv->od_data.od_sram_read_sel;
	w_sel = !r_sel;
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, w_sel, REG_OD_SRAM_WRITE_SEL);
	SET_VAL_MASK(value, mask, r_sel, REG_OD_SRAM_READ_SEL);
	ODDMRFLOW_LOG("value w_sel %d r_sel %d 0x%x\n", w_sel, r_sel, value);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, handle);
	/* od pq */
	if (IS_TABLE_VALID(table_idx, g_od_param.valid_table))
		mtk_oddmr_set_pq(comp, handle, &g_od_param.od_tables[table_idx]->pq_od);
}

static void mtk_oddmr_set_od_weight(struct mtk_ddp_comp *comp, uint32_t weight,
		struct cmdq_pkt *handle)
{
	mtk_oddmr_write(comp, weight, DISP_ODDMR_OD_PQ_0,
			handle);
}

static void mtk_oddmr_set_od_weight_dual(struct mtk_ddp_comp *comp, uint32_t weight,
		struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG("%u+\n", weight);
	mtk_oddmr_set_od_weight(default_comp, weight, handle);
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_od_weight(oddmr1_default_comp, weight, handle);
}

static void mtk_oddmr_od_timing_chg_dual(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle)
{
	uint32_t weight;
	int ret, table_idx;

	ODDMRAPI_LOG("+\n");
	if (g_oddmr_priv->od_state >= ODDMR_INIT_DONE) {
		ret = mtk_oddmr_od_table_lookup(g_oddmr_priv, timing, &table_idx);
		if (ret >= 0) {
			if ((timing->mode_chg_index & MODE_DSI_RES) &&
			(g_od_param.od_basic_info.basic_param.resolution_switch_mode == 1)) {
				/* res switch in ddic */
				atomic_set(&g_oddmr_od_weight_trigger, 1);
				weight = 0;
				mtk_oddmr_od_set_res_udma_dual(handle);
				mtk_oddmr_set_od_weight_dual(default_comp, weight, handle);
			}
			g_oddmr_priv->od_force_off = 0;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_force_off = 0;
		} else {
			g_oddmr_priv->od_force_off = 1;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_force_off = 1;
		}
	}
}


/* call from drm atomic flow */
void mtk_oddmr_timing_chg(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle)
{
	unsigned long flags;
	struct mtk_oddmr_timing timing_working_copy;

	if (is_oddmr_dmr_support || is_oddmr_od_support || is_oddmr_dbi_support) {
		ODDMRAPI_LOG("+\n");
		mtk_oddmr_copy_oddmr_timg(&timing_working_copy, timing);
		spin_lock_irqsave(&g_oddmr_timing_lock, flags);
		timing_working_copy.bl_level = g_oddmr_current_timing.bl_level;
		mtk_oddmr_copy_oddmr_timg(&g_oddmr_current_timing, &timing_working_copy);
		spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);
		ODDMRFLOW_LOG("w %u, h %u, fps %u, bl %u\n",
				timing_working_copy.hdisplay,
				timing_working_copy.vdisplay,
				timing_working_copy.vrefresh,
				timing_working_copy.bl_level);
		if (timing->mode_chg_index & MODE_DSI_RES)
			mtk_oddmr_set_spr2rgb_dual(handle);
	}
	if (is_oddmr_dmr_support && handle != NULL)
		mtk_oddmr_dmr_timing_chg_dual(&timing_working_copy, handle);
	if (is_oddmr_od_support && handle != NULL)
		mtk_oddmr_od_timing_chg_dual(&timing_working_copy, handle);
	if (is_oddmr_dbi_support && handle != NULL)
		mtk_oddmr_dbi_timing_chg_dual(&timing_working_copy, handle);
}

static void mtk_oddmr_dmr_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle)
{
	uint32_t dbv_node = 0;
	uint32_t fps_node = 0;
	uint32_t dbv_table_idx = 0;
	uint32_t fps_table_idx = 0;
	dma_addr_t addr = 0;
	if (g_oddmr_priv->dmr_state >= ODDMR_INIT_DONE) {
		ODDMRAPI_LOG("+\n");
		mtk_oddmr_dmr_dbv_lookup(bl_level, &g_oddmr_priv->dmr_cfg_info, &dbv_table_idx, &dbv_node);
		fps_node = atomic_read(&g_oddmr_priv->dmr_data.cur_fps_node);
		fps_table_idx = atomic_read(&g_oddmr_priv->dmr_data.cur_fps_table_idx);

		if(dbv_node != atomic_read(&g_oddmr_priv->dmr_data.cur_dbv_node)){
			mtk_oddmr_dmr_gain_cfg(default_comp,
					handle, dbv_node, fps_node, &g_oddmr_priv->dmr_cfg_info);
			atomic_set(&g_oddmr_priv->dmr_data.cur_dbv_node, dbv_node);
		}
		if(dbv_table_idx != atomic_read(&g_oddmr_priv->dmr_data.cur_dbv_table_idx)){
			addr = g_oddmr_priv->dmr_data.mura_table[dbv_table_idx][fps_table_idx]->dma_addr;
			mtk_oddmr_write(default_comp, addr >> 4, DISP_ODDMR_DMR_UDMA_CTR_4, NULL);
			mtk_oddmr_write(default_comp, addr >> 20, DISP_ODDMR_DMR_UDMA_CTR_5, NULL);
			atomic_set(&g_oddmr_priv->dmr_data.cur_dbv_table_idx, dbv_table_idx);
		}
		ODDMRFLOW_LOG("dmr gain config: dbv_node:%d, fps_node:%d\n", dbv_node, fps_node);
		ODDMRFLOW_LOG("dmr table cfg: dbv_table:%d, fps_table:%d\n", dbv_table_idx, fps_table_idx);
	}
}

static void mtk_oddmr_dbi_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle)
{
	uint32_t dbv_node = 0;
	uint32_t fps_node = 0;
	int cur_tb;
	uint32_t max_time_set_done = 0;
	unsigned int gain_ratio;

	if (g_oddmr_priv->dbi_state >= ODDMR_INIT_DONE) {
		ODDMRAPI_LOG("+\n");
		mtk_oddmr_dbi_dbv_lookup(bl_level, g_oddmr_priv->dbi_cfg_info.fps_dbv_node.DBV_node,
			g_oddmr_priv->dbi_cfg_info.fps_dbv_node.DBV_num, &dbv_node);
		fps_node = atomic_read(&g_oddmr_priv->dbi_data.cur_fps_node);

		gain_ratio = atomic_read(&g_oddmr_priv->dbi_data.gain_ratio);
		cur_tb = atomic_read(&g_oddmr_priv->dbi_data.cur_table_idx);
		if(cur_tb)
			mtk_oddmr_dbi_gain_cfg(default_comp,
				handle, dbv_node, fps_node,
				&g_oddmr_priv->dbi_cfg_info_tb1, gain_ratio);
		else
			mtk_oddmr_dbi_gain_cfg(default_comp,
				handle, dbv_node, fps_node,
				&g_oddmr_priv->dbi_cfg_info, gain_ratio);
		atomic_set(&g_oddmr_priv->dbi_data.cur_dbv_node, dbv_node);

		if (g_oddmr_priv->dbi_cfg_info.dbv_node.DBV_num) {
			if(mtk_oddmr_dbi_dbv_lookup(bl_level,
				g_oddmr_priv->dbi_cfg_info.dbv_node.DBV_node,
				g_oddmr_priv->dbi_cfg_info.dbv_node.DBV_num, &dbv_node))
				ODDMRFLOW_LOG("dbi dbv lookup fail\n");
			mtk_oddmr_dbi_dbv_table_cfg(default_comp,
				handle, dbv_node, &g_oddmr_priv->dbi_cfg_info);
		}

		max_time_set_done = atomic_read(&g_oddmr_priv->dbi_data.max_time_set_done);
		if (max_time_set_done) {
			mutex_lock(&g_dbi_data_lock);
			mtk_oddmr_dbi_change_remap_gain(default_comp, handle, g_oddmr_priv->dbi_data.cur_max_time);
			mutex_unlock(&g_dbi_data_lock);
		}
		ODDMRFLOW_LOG("dbi gain config: dbv_node:%d, fps_node:%d\n", dbv_node, fps_node);
	}
}

static void mtk_oddmr_od_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle)
{
	int table_idx, ret;

	if (g_oddmr_priv->od_state >= ODDMR_INIT_DONE) {
		ODDMRAPI_LOG("+\n");
		ret = mtk_oddmr_od_table_lookup(g_oddmr_priv, &g_oddmr_current_timing, &table_idx);
		if (ret >= 0) {
			g_oddmr_priv->od_force_off = 0;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_force_off = 0;
		} else {
			g_oddmr_priv->od_force_off = 1;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_force_off = 1;
		}
	}
}

void mtk_oddmr_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle)
{
	unsigned long flags;

	ODDMRAPI_LOG("bl_level %u\n", bl_level);
	/* keep track of chg anytime */
	spin_lock_irqsave(&g_oddmr_timing_lock, flags);
	g_oddmr_current_timing.bl_level = bl_level;
	spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);

	if (is_oddmr_dmr_support && handle != NULL)
		mtk_oddmr_dmr_bl_chg(bl_level, handle);

	if (is_oddmr_dbi_support && handle != NULL)
		mtk_oddmr_dbi_bl_chg(bl_level, handle);

	if (is_oddmr_od_support && handle != NULL)
		mtk_oddmr_od_bl_chg(bl_level, handle);
}

void mtk_oddmr_set_pq_dirty(void)
{
	atomic_set(&g_oddmr_pq_dirty, 1);
}

int mtk_oddmr_hrt_cal_notify(int *oddmr_hrt)
{
	int sum = 0, ret = 0;
	unsigned long long res_ratio = 1000;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	if (!default_comp || !g_oddmr_priv) {
		return 0;
	}
	mtk_crtc = default_comp->mtk_crtc;
	if (!mtk_crtc) {
		return 0;
	}
	priv = mtk_crtc->base.dev->dev_private;
	if (is_oddmr_od_support || is_oddmr_dmr_support || is_oddmr_dbi_support) {
		if (atomic_read(&g_oddmr_od_hrt_done) == 2)
			atomic_set(&g_oddmr_od_hrt_done, 1);
		if (atomic_read(&g_oddmr_dmr_hrt_done) == 2)
			atomic_set(&g_oddmr_dmr_hrt_done, 1);
		if (atomic_read(&g_oddmr_dbi_hrt_done) == 2)
			atomic_set(&g_oddmr_dbi_hrt_done, 1);
		if (g_oddmr_priv->od_enable_req)
			sum += mtk_oddmr_od_bpp(g_od_param.od_basic_info.basic_param.od_mode);
		if (g_oddmr_priv->dmr_enable_req)
			sum += mtk_oddmr_dmr_bpp(
					g_dmr_param.dmr_basic_info.basic_param.dmr_table_mode);
		/* DBI HRT */
		if (g_oddmr_priv->dbi_enable_req)
			sum += mtk_oddmr_dbi_bpp(
					g_dmr_param.dmr_basic_info.basic_param.dmr_table_mode);

		wake_up_all(&g_oddmr_hrt_wq);
		g_oddmr_priv->od_enable = g_oddmr_priv->od_enable_req && !g_oddmr_priv->pq_od_bypass;
		g_oddmr_priv->dmr_enable = g_oddmr_priv->dmr_enable_req;
		g_oddmr_priv->dbi_enable = g_oddmr_priv->dbi_enable_req;
		if (default_comp->mtk_crtc->is_dual_pipe) {
			g_oddmr1_priv->od_enable = g_oddmr1_priv->od_enable_req && !g_oddmr_priv->pq_od_bypass;
			g_oddmr1_priv->dmr_enable = g_oddmr1_priv->dmr_enable_req;
			g_oddmr1_priv->dbi_enable = g_oddmr1_priv->dbi_enable_req;
		}
		if (mtk_crtc->scaling_ctx.scaling_en) {
			res_ratio =
				((unsigned long long)mtk_crtc->scaling_ctx.lcm_width *
				mtk_crtc->scaling_ctx.lcm_height * 1000) /
				((unsigned long long)mtk_crtc->base.state->adjusted_mode.vdisplay *
				mtk_crtc->base.state->adjusted_mode.hdisplay);
		}
		if (g_oddmr_priv->od_enable)
			sum += mtk_oddmr_od_bpp(g_od_param.od_basic_info.basic_param.od_mode);
		ODDMRLOW_LOG("od %d dmr %d sum %d res_ratio %llu\n",
				g_oddmr_priv->od_enable, g_oddmr_priv->dmr_enable, sum, res_ratio);
		sum = sum * res_ratio / 1000;
	} else {
		ret = 0;
		goto out;
	}
	if (g_oddmr_hrt_en == false) {
		ret = 0;
		goto out;
	}

	*oddmr_hrt += sum;
	ret = sum;

out:
	if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
		if (default_comp->mtk_crtc->is_dual_pipe) {
			default_comp->larb_hrt_weight = *oddmr_hrt / 2;
			oddmr1_default_comp->larb_hrt_weight = *oddmr_hrt / 2;
		} else
			default_comp->larb_hrt_weight = *oddmr_hrt;
		DDPDBG_HBL("%s,oddmr comp:%u,w:%u,oddmr_hrt:%u,sum:%u\n",
			__func__, default_comp->id,
			default_comp->larb_hrt_weight, *oddmr_hrt, sum);
	}

	return ret;
}

void mtk_oddmr_update_larb_hrt_state(void)
{
	if (default_comp && g_oddmr_priv) {
		atomic_set(&g_oddmr_priv->hrt_weight, default_comp->larb_hrt_weight);
		if (!default_comp->mtk_crtc->is_dual_pipe)
			return;
	}

	if (oddmr1_default_comp && g_oddmr1_priv)
		atomic_set(&g_oddmr1_priv->hrt_weight,
			oddmr1_default_comp->larb_hrt_weight);
}

void mtk_oddmr_od_sec_bypass(uint32_t sec_on, struct cmdq_pkt *handle)
{
	uint32_t enable;
	static uint32_t prev;

	ODDMRAPI_LOG();
	if (is_oddmr_od_support) {
		enable = g_oddmr_priv->od_enable && (!sec_on);
		mtk_oddmr_set_od_enable_dual(NULL, enable, handle);
		if (enable && sec_on != prev &&
			(g_oddmr_priv->data->is_od_support_hw_skip_first_frame == false)) {
			mtk_oddmr_set_od_weight_dual(default_comp, 0, handle);
			atomic_set(&g_oddmr_od_weight_trigger, 1);
		}
		prev = sec_on;
	}
}
void mtk_oddmr_ddren(struct cmdq_pkt *cmdq_handle,
	struct drm_crtc *crtc, int en)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned long crtc_id = (unsigned long)drm_crtc_index(crtc);
	const u16 reg_jump = CMDQ_THR_SPR_IDX1;
	const u16 var1 = CMDQ_THR_SPR_IDX2;
	struct cmdq_operand lop, rop;
	u32 inst_condi_jump;
	u64 *inst, jump_pa;
	resource_size_t pa;
	unsigned int mmsys_sodi_req_mask, sodi_req_sel_val_sw;

	if (priv->data->mmsys_id != MMSYS_MT6985 || g_oddmr_ddren == false)
		return;
	if (crtc_id != 0)
		return;
	mmsys_sodi_req_mask = 0xF4;
	sodi_req_sel_val_sw = 0x4100;
	pa = 0x14013000;
	if (default_comp)
		pa = default_comp->regs_pa;
	if (en) {
		/* 1. get od status */
		lop.reg = true;
		lop.idx = CMDQ_THR_SPR_IDX2;
		rop.reg = false;
		rop.value = 1;

		cmdq_pkt_read(cmdq_handle, NULL,
					pa + DISP_ODDMR_OD_CTRL_EN, var1);
		cmdq_pkt_logic_command(cmdq_handle, CMDQ_LOGIC_AND, var1, &lop, &rop);
		/* 2. set ddren if od is enabled*/
		lop.reg = true;
		lop.idx = CMDQ_THR_SPR_IDX2;
		rop.reg = false;
		rop.value = 1;

		/*mark condition jump */
		inst_condi_jump = cmdq_handle->cmd_buf_size;
		cmdq_pkt_assign_command(cmdq_handle, reg_jump, 0);

		cmdq_pkt_cond_jump_abs(cmdq_handle, reg_jump, &lop, &rop,
			CMDQ_NOT_EQUAL);

		/* if condition false, will jump here */
		{
			cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
				mtk_crtc->config_regs_pa + mmsys_sodi_req_mask,
				sodi_req_sel_val_sw, sodi_req_sel_val_sw);
		}

		/* if condition true, will jump curreent postzion */
		inst = cmdq_pkt_get_va_by_offset(cmdq_handle,  inst_condi_jump);
		jump_pa = cmdq_pkt_get_pa_by_offset(cmdq_handle,
					cmdq_handle->cmd_buf_size);
		*inst = *inst | CMDQ_REG_SHIFT_ADDR(jump_pa);

	} else
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			mtk_crtc->config_regs_pa + mmsys_sodi_req_mask,
			0x4000, sodi_req_sel_val_sw);
}

static void mtk_cal_oddmr_valid_partial_roi(struct mtk_ddp_comp *comp,
				struct mtk_rect *partial_roi)
{
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data = &g_oddmr_priv->dbi_cfg_info;
	unsigned int scale_factor_v = 4;
	unsigned int dbi_y_diff = 0;

	ODDMRLOW_LOG("%s line: %d before partial_y:%d partial_height:%d\n",
		__func__, __LINE__, partial_roi->y, partial_roi->height);

	if (comp->mtk_crtc->panel_ext->params->is_support_dbi == true &&
		g_oddmr_priv->dbi_state == ODDMR_INIT_DONE) {
		scale_factor_v = dbi_cfg_data->basic_info.partial_update_scale_factor_v;
		/* align to scale factor v*/
		if (partial_roi->y % scale_factor_v != 0) {
			dbi_y_diff =
				partial_roi->y - (partial_roi->y / scale_factor_v) * scale_factor_v;
			partial_roi->y -= dbi_y_diff;
		}
		partial_roi->height += dbi_y_diff;
	}
	ODDMRLOW_LOG("%s line: %d end partial_y:%d partial_height:%d\n",
		__func__, __LINE__, partial_roi->y, partial_roi->height);
}

int mtk_oddmr_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		enum mtk_ddp_io_cmd cmd, void *params)
{
	ODDMRLOW_LOG("cmd %d+\n", cmd);

	switch (cmd) {
	case FRAME_DIRTY:
		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		atomic_set(&g_oddmr_frame_dirty, 1);
		break;
	case ODDMR_TRIG_CTL:
	{
		uint32_t on = *(uint32_t *)params;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		g_od_check_trigger = on;
	}
		break;
	case PQ_DIRTY:
		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_set_pq_dirty();
		break;
	case ODDMR_BL_CHG:
	{
		uint32_t bl_level = *(uint32_t *)params;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_bl_chg(bl_level, handle);
	}
		break;
	case ODDMR_TIMING_CHG:
	{
		struct mtk_oddmr_timing *timing = (struct mtk_oddmr_timing *)params;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_timing_chg(timing, handle);
	}
		break;
	case COMP_ODDMR_CFG:
	{
		bool sec_on = *(bool *)params;

		static int dmr_enable;
		static int dbi_enable;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_od_sec_bypass(sec_on, handle);
		if(dmr_enable != g_oddmr_priv->dmr_enable) {
			mtk_oddmr_set_dmr_enable_dual(NULL, g_oddmr_priv->dmr_enable, handle);
			dmr_enable = g_oddmr_priv->dmr_enable;
		}

		if(dbi_enable != g_oddmr_priv->dbi_enable) {
			mtk_oddmr_dbi_config(default_comp,handle);
			dbi_enable = g_oddmr_priv->dbi_enable;
		}
		break;
	}
	case PMQOS_UPDATE_BW:
	{
		struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
		struct mtk_drm_private *priv =
			comp->mtk_crtc->base.dev->dev_private;
		unsigned int force_update = 0; /* force_update repeat last qos BW */

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;
		ODDMRLOW_LOG("srt odr(%u,%u),odw(%u,%u),dmrr(%u,%u) dbi(%u,%u)\n",
			oddmr_priv->last_qos_srt_odr, oddmr_priv->qos_srt_odr,
			oddmr_priv->last_qos_srt_odw, oddmr_priv->qos_srt_odw,
			oddmr_priv->last_qos_srt_dmrr, oddmr_priv->qos_srt_dmrr,
			oddmr_priv->last_qos_srt_dbir, oddmr_priv->qos_srt_dbir);

		if (params) {
			force_update = *(unsigned int *)params;
			force_update = (force_update == DISP_BW_FORCE_UPDATE) ? 1 : 0;
		}

		/* process normal */
		if (force_update || oddmr_priv->last_qos_srt_odr != oddmr_priv->qos_srt_odr) {
			__mtk_disp_set_module_srt(oddmr_priv->qos_req_odr, comp->id,
				oddmr_priv->qos_srt_odr, 0, DISP_BW_NORMAL_MODE);
			oddmr_priv->last_qos_srt_odr = oddmr_priv->qos_srt_odr;
			if (!force_update)
				comp->mtk_crtc->total_srt += oddmr_priv->qos_srt_odr;
		}
		if (force_update || oddmr_priv->last_qos_srt_odw != oddmr_priv->qos_srt_odw) {
			__mtk_disp_set_module_srt(oddmr_priv->qos_req_odw, comp->id,
				oddmr_priv->qos_srt_odw, 0, DISP_BW_NORMAL_MODE);
			oddmr_priv->last_qos_srt_odw = oddmr_priv->qos_srt_odw;
			if (!force_update)
				comp->mtk_crtc->total_srt += oddmr_priv->qos_srt_odw;
		}
		if (force_update || oddmr_priv->last_qos_srt_dmrr != oddmr_priv->qos_srt_dmrr) {
			__mtk_disp_set_module_srt(oddmr_priv->qos_req_dmrr, comp->id,
				oddmr_priv->qos_srt_dmrr, 0, DISP_BW_NORMAL_MODE);
			oddmr_priv->last_qos_srt_dmrr = oddmr_priv->qos_srt_dmrr;
			if (!force_update)
				comp->mtk_crtc->total_srt += oddmr_priv->qos_srt_dmrr;
		}
		if (force_update || oddmr_priv->last_qos_srt_dbir != oddmr_priv->qos_srt_dbir) {
			__mtk_disp_set_module_srt(oddmr_priv->qos_req_dbir, comp->id,
				oddmr_priv->qos_srt_dbir, 0, DISP_BW_NORMAL_MODE);
			oddmr_priv->last_qos_srt_dbir = oddmr_priv->qos_srt_dbir;
			if (!force_update)
				comp->mtk_crtc->total_srt += oddmr_priv->qos_srt_dbir;
		}
	}
		break;
	case PMQOS_GET_HRT_BW:
	{
		struct mtk_drm_private *priv =
			comp->mtk_crtc->base.dev->dev_private;
		struct mtk_larb_bw *data = (struct mtk_larb_bw *)params;
		unsigned int bw_base = 0;
		int calc = !!data->larb_bw;

		if (IS_ERR_OR_NULL(data)) {
			DDPMSG("%s, invalid larb data\n", __func__);
			break;
		}

		data->larb_id = -1;
		data->larb_bw = 0;
		if (!comp || !mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT) ||
			!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
			break;

		if (comp->larb_num == 1)
			data->larb_id = comp->larb_id;
		else if (comp->larb_num > 1)
			data->larb_id = comp->larb_ids[0];

		if (data->larb_id < 0) {
			DDPMSG("%s, invalid larb id, num:%d\n", __func__,
				comp->larb_num);
			break;
		}

		if (calc) {
			bw_base = mtk_drm_primary_frame_bw(&comp->mtk_crtc->base);
			if (default_comp && g_oddmr_priv &&
				comp->id == default_comp->id)
				data->larb_bw = atomic_read(
					&g_oddmr_priv->hrt_weight) * bw_base / 400;
			else if (oddmr1_default_comp && g_oddmr1_priv &&
				comp->id == oddmr1_default_comp->id)
				data->larb_bw = atomic_read(
					&g_oddmr1_priv->hrt_weight) * bw_base / 400;
		}
		DDPDBG_HBL("%s,oddmr comp:%u,larb:%d,bw:%u,calc:%d,bw_base:%u\n",
			__func__, comp->id, data->larb_id, data->larb_bw,
			calc, bw_base);
	}
		break;
	case PMQOS_SET_HRT_BW:
	{
		int od_enable, dmr_enable, dbi_enable, sec_on;
		u32 bw_val = *(unsigned int *)params;
		struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
		struct mtk_drm_private *priv =
			comp->mtk_crtc->base.dev->dev_private;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;
		sec_on = comp->mtk_crtc->sec_on;
		od_enable = oddmr_priv->od_enable && (!sec_on);
		dmr_enable = oddmr_priv->dmr_enable;
		dbi_enable = oddmr_priv->dbi_enable;
		od_enable = !!bw_val && od_enable;
		dmr_enable = !!bw_val && dmr_enable;
		dbi_enable = !!bw_val && dbi_enable;
		/* set to max if need hrt */
		__mtk_disp_set_module_hrt(oddmr_priv->qos_req_dmrr_hrt, dmr_enable);
		__mtk_disp_set_module_hrt(oddmr_priv->qos_req_dbir_hrt, dbi_enable);
		__mtk_disp_set_module_hrt(oddmr_priv->qos_req_odr_hrt, od_enable);
		__mtk_disp_set_module_hrt(oddmr_priv->qos_req_odw_hrt, od_enable);
		ODDMRLOW_LOG("hrt od %d dmr %d\n", od_enable, dmr_enable);
	}
		break;
	case GET_VALID_PARTIAL_ROI:
	{
		struct mtk_rect *partial_roi = NULL;

		partial_roi = (struct mtk_rect *)params;
		mtk_cal_oddmr_valid_partial_roi(comp, partial_roi);
	}
		break;
	case BYPASS_SPR2RGB:
	{
		bool s2r_bypass = *(bool *)params;

		mtk_oddmr_s2r_bypass(comp, handle, s2r_bypass);
	}
		break;
	default:
		break;
	}
	return 0;
}

static void mtk_oddmr_od_bypass(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	ODDMRLOW_LOG("+\n");
	mtk_oddmr_write(comp, 1,
			DISP_ODDMR_TOP_OD_BYASS, handle);
	mtk_oddmr_write(comp, 0x200, DISP_ODDMR_OD_SW_RESET, handle);
}

static void mtk_oddmr_od_table_chg(struct cmdq_pkt *handle)
{
	uint32_t weight = 0;
	int table_idx,ret;

	if ((g_oddmr_priv->od_state >= ODDMR_INIT_DONE) &&
		(g_oddmr_current_timing.old_vrefresh != g_oddmr_current_timing.vrefresh ||
		g_oddmr_current_timing.old_bl_level != g_oddmr_current_timing.bl_level)) {
		ODDMRAPI_LOG("+\n");
		ret = mtk_oddmr_od_table_lookup(g_oddmr_priv, &g_oddmr_current_timing, &table_idx);
		if (ret >= 0) {
			if (ret == 1) {
				//flip
				mtk_oddmr_od_flip(default_comp, handle);
				if (default_comp->mtk_crtc->is_dual_pipe)
					mtk_oddmr_od_flip(oddmr1_default_comp, handle);

				g_oddmr_current_timing.old_vrefresh = g_oddmr_current_timing.vrefresh;
				g_oddmr_current_timing.old_bl_level = g_oddmr_current_timing.bl_level;
			} else if (g_oddmr_priv->data->is_od_4_table && ret == 2) {
				//TODO:update table work queue(table pq sram) mt6985 no need
				queue_work(g_oddmr_priv->oddmr_wq, &g_oddmr_priv->update_table_work);
			} else if (ret == 0) {
				g_oddmr_current_timing.old_vrefresh = g_oddmr_current_timing.vrefresh;
				g_oddmr_current_timing.old_bl_level = g_oddmr_current_timing.bl_level;
			}

			mtk_oddmr_od_gain_lookup(default_comp, g_oddmr_current_timing.vrefresh,
					g_oddmr_current_timing.bl_level, table_idx, &weight);
			mtk_oddmr_set_od_weight_dual(default_comp, weight, handle);
			g_oddmr_priv->od_force_off = 0;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_force_off = 0;
		} else {
			g_oddmr_priv->od_force_off = 1;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_force_off = 1;
		}
	}
}

static void mtk_oddmr_set_od_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	bool sec_on, en;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	if (g_oddmr_priv->od_state < ODDMR_INIT_DONE)
		return;
	sec_on = comp->mtk_crtc->sec_on;
	en = enable && !sec_on && !oddmr_priv->od_force_off;
	ODDMRLOW_LOG("en %d, sec %d force_off %d\n",
		enable, sec_on, oddmr_priv->od_force_off);
	mtk_oddmr_od_srt_cal(comp, en);
	if (en) {
		if (oddmr_priv->od_enable_last == 0 && en == true) {
			atomic_set(&g_oddmr_od_weight_trigger, 1);
			mtk_oddmr_set_od_weight(comp, 0, handle);
			mtk_oddmr_od_init_end(comp, handle);
		}
		mtk_oddmr_write_mask(comp, 1, DISP_ODDMR_OD_CTRL_EN, 0x01, handle);
	} else {
		mtk_oddmr_write_mask(comp, 0, DISP_ODDMR_OD_CTRL_EN, 0x01, handle);
		mtk_oddmr_od_bypass(comp, handle);
	}
	oddmr_priv->od_enable_last = en;
}

static void mtk_oddmr_bypass(struct mtk_ddp_comp *comp, int bypass,
		struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG();
	if (comp->id == DDP_COMPONENT_ODDMR1)
		return;
	if (is_oddmr_od_support && (g_oddmr_priv->pq_od_bypass != bypass)) {
		g_oddmr_priv->pq_od_bypass = bypass;
		g_oddmr_priv->od_enable =
			g_oddmr_priv->od_enable_req && !g_oddmr_priv->pq_od_bypass;

		if (default_comp->mtk_crtc->is_dual_pipe) {
			g_oddmr1_priv->pq_od_bypass = bypass;
			g_oddmr1_priv->od_enable =
				g_oddmr1_priv->od_enable_req && !g_oddmr1_priv->pq_od_bypass;
		}
		ODDMRLOW_LOG("pq_od_bypass %d,od_enable %d",
			g_oddmr_priv->pq_od_bypass, g_oddmr_priv->od_enable);
		if (bypass == 0) {
			mtk_oddmr_set_od_enable_dual(comp,
				g_oddmr_priv->od_enable_req, handle);
		} else if (bypass == 1) {
			mtk_oddmr_set_od_enable_dual(comp, 0, handle);
		}
	}
}

static void mtk_oddmr_set_od_enable_dual(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	bool sec_on, en;

	sec_on = default_comp->mtk_crtc->sec_on;
	if (default_comp->mtk_crtc->is_dual_pipe)
		sec_on = sec_on && oddmr1_default_comp->mtk_crtc->sec_on;

	en = enable && !sec_on && !g_oddmr_priv->od_force_off;
	if (default_comp->mtk_crtc->is_dual_pipe)
		en = en && !g_oddmr1_priv->od_force_off;

	mtk_oddmr_set_od_enable(default_comp, enable, handle);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_od_enable(oddmr1_default_comp, enable, handle);
	if(en && g_oddmr_priv->od_state >= ODDMR_INIT_DONE)
		mtk_oddmr_od_table_chg(handle);
}
static void mtk_oddmr_set_dmr_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG("+\n");

	mtk_oddmr_dmr_srt_cal(comp, enable);

	if(is_oddmr_dmr_support) {
		if (enable) {
			//mtk_oddmr_top_prepare(comp, handle);
			mtk_oddmr_write(comp, 1, DISP_ODDMR_REG_DMR_EN, handle);
			mtk_oddmr_write(comp, 0,
				DISP_ODDMR_TOP_DMR_BYASS, handle);
			mtk_oddmr_write(comp, 1,
				DISP_ODDMR_DMR_UDMA_ENABLE, handle);
			mtk_oddmr_write(comp, 1,
				DISP_ODDMR_REG_DMR_CLK_EN, handle);
		} else {
			mtk_oddmr_write(comp, 1,
				DISP_ODDMR_TOP_DMR_BYASS, handle);
			mtk_oddmr_write(comp, 0,
				DISP_ODDMR_DMR_UDMA_ENABLE, handle);
			mtk_oddmr_write(comp, 0,
				DISP_ODDMR_REG_DMR_CLK_EN, handle);
			mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_DMR_EN, handle);
		}
	}
}

static void mtk_oddmr_set_dbi_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG("+\n");

	mtk_oddmr_dbi_srt_cal(comp, enable);

	if(is_oddmr_dbi_support) {
		if (enable) {
			//mtk_oddmr_top_prepare(comp, handle);
			mtk_oddmr_write(comp, 1, DISP_ODDMR_REG_DMR_DBI_EN, handle);
			mtk_oddmr_write(comp, 0,
				DISP_ODDMR_TOP_DMR_BYASS, handle);
			mtk_oddmr_write(comp, 1,
				DISP_ODDMR_DMR_UDMA_ENABLE, handle);
			mtk_oddmr_write(comp, 1,
				DISP_ODDMR_REG_DMR_CLK_EN, handle);
		} else {
			mtk_oddmr_write(comp, 1,
				DISP_ODDMR_TOP_DMR_BYASS, handle);
			mtk_oddmr_write(comp, 0,
				DISP_ODDMR_DMR_UDMA_ENABLE, handle);
			mtk_oddmr_write(comp, 0,
				DISP_ODDMR_REG_DMR_CLK_EN, handle);
			mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_DMR_DBI_EN, handle);
		}
	}
}


static void mtk_oddmr_set_dmr_enable_dual(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	ODDMRFLOW_LOG("+\n");
	mtk_oddmr_set_dmr_enable(default_comp, enable, handle);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_dmr_enable(oddmr1_default_comp, enable, handle);
}

static void mtk_oddmr_od_init_end_dual(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	ODDMRFLOW_LOG("+\n");
	mtk_oddmr_od_init_end(default_comp, handle);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_init_end(oddmr1_default_comp, handle);
}

static void mtk_oddmr_od_tuning_write_sram(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct mtk_oddmr_od_tuning_sram *tuning_data)
{
	uint32_t channel, sram, idx, val, ctl;
	uint32_t value = 0, mask = 0, tmp_r_sel = 0, tmp_w_sel = 0;
	uint32_t sram_write_change;
	struct mtk_drm_private *priv = default_comp->mtk_crtc->base.dev->dev_private;

	if(priv->data->mmsys_id == MMSYS_MT6985 ||
		priv->data->mmsys_id == MMSYS_MT6989)
		sram_write_change = 1;
	else
		sram_write_change = 0;

	channel = tuning_data->channel;
	sram = tuning_data->sram;
	idx = tuning_data->idx;
	val = tuning_data->value;

	ctl = mtk_oddmr_read(comp, DISP_ODDMR_OD_SRAM_CTRL_0);

	tmp_r_sel = (ctl & 0x20) >> 5;
	tmp_w_sel = tmp_r_sel;
	value = 0;
	mask = 0;
	//change begin
	//B:0-bit1, G:1-bit2, R:2-bit3 -->> B:2-bit1, G:1-bit2, R:0-bit3
	if(sram_write_change)
		channel = channel - (channel-1)*2;
	//change end
	SET_VAL_MASK(value, mask, 1 << (channel + 1), REG_WBGR_OD_SRAM_IO_EN);
	SET_VAL_MASK(value, mask, 0, REG_AUTO_SRAM_ADR_INC_EN);
	SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
	SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, handle);
	mtk_oddmr_write(comp, val,
		(DISP_ODDMR_OD_SRAM_CTRL_2 + 12 * (sram - 1)), handle);
	mtk_oddmr_write(comp, 0x8000 | (idx & 0x1FF),
		(DISP_ODDMR_OD_SRAM_CTRL_1 + 12 * (sram - 1)), handle);
	mtk_oddmr_write_mask(comp, ctl, DISP_ODDMR_OD_SRAM_CTRL_0, 0xFFFFFFFF, handle);
}

static void mtk_oddmr_od_tuning_write_sram_dual(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct mtk_oddmr_od_tuning_sram *tuning_data)
{
	mtk_oddmr_od_tuning_write_sram(default_comp, handle, tuning_data);
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_tuning_write_sram(oddmr1_default_comp, handle, tuning_data);
}

static void mtk_oddmr_od_tuning_read_sram(struct mtk_ddp_comp *comp,
	struct mtk_oddmr_od_tuning_sram *tuning_data)
{
	uint32_t channel, sram, idx, ctl;
	uint32_t value = 0, mask = 0, tmp_r_sel = 0, tmp_w_sel = 0;
	uint32_t sram_write_change;
	struct mtk_drm_private *priv = default_comp->mtk_crtc->base.dev->dev_private;

	if(priv->data->mmsys_id == MMSYS_MT6985 ||
		priv->data->mmsys_id == MMSYS_MT6989)
		sram_write_change = 1;
	else
		sram_write_change = 0;

	channel = tuning_data->channel;
	sram = tuning_data->sram;
	idx = tuning_data->idx;

	ctl = mtk_oddmr_read(comp, DISP_ODDMR_OD_SRAM_CTRL_0);

	tmp_r_sel = (ctl & 0x20) >> 5;
	tmp_w_sel = tmp_r_sel;
	value = 0;
	mask = 0;
	//change begin
	//B:0-bit1, G:1-bit2, R:2-bit3 -->> B:2-bit1, G:1-bit2, R:0-bit3
	if(sram_write_change)
		channel = channel - (channel-1)*2;
	//change end
	SET_VAL_MASK(value, mask, 1 << (channel + 1), REG_WBGR_OD_SRAM_IO_EN);
	SET_VAL_MASK(value, mask, 0, REG_AUTO_SRAM_ADR_INC_EN);
	SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
	SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
	mtk_oddmr_write_mask_cpu(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask);
	mtk_oddmr_write_cpu(comp, 0x4000 | (idx & 0x1FF),
		(DISP_ODDMR_OD_SRAM_CTRL_1 + 12 * (sram - 1)));
	tuning_data->value = mtk_oddmr_read(comp,
		(DISP_ODDMR_OD_SRAM_CTRL_3 + 12 * (sram - 1)));
	mtk_oddmr_write_cpu(comp, ctl, DISP_ODDMR_OD_SRAM_CTRL_0);
}

/* all oddmr user cmd use handle dualpipe itself because it is not drm atomic */
static int mtk_oddmr_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		uint32_t cmd, void *data)
{
	ODDMRFLOW_LOG("+ cmd: %d\n", cmd);
	/* only handle comp0 as main oddmr comp */
	if (comp->id == DDP_COMPONENT_ODDMR1)
		return 0;

	switch (cmd) {
	case ODDMR_CMD_OD_SET_WEIGHT:
	{
		uint32_t value = *(uint32_t *)data;

		mtk_oddmr_set_od_weight_dual(comp, value, handle);
		break;
	}
	case ODDMR_CMD_OD_ENABLE:
	{
		uint32_t value = *(uint32_t *)data;
		//sw bypass first frame od pq
		//TODO: g_oddmr_od_weight_trigger = 2 ???
		if ((g_oddmr_priv->data->is_od_support_hw_skip_first_frame == false) &&
				(is_oddmr_od_support == true) &&
				(value == 1)) {
			mtk_oddmr_set_od_weight_dual(comp, 0, handle);
			atomic_set(&g_oddmr_od_weight_trigger, 1);
		}
		mtk_oddmr_set_od_enable_dual(comp, value, handle);
		break;
	}
	case ODDMR_CMD_DMR_ENABLE:
	{
		uint32_t value = *(uint32_t *)data;

		mtk_oddmr_set_dmr_enable_dual(comp, value, handle);
		break;
	}
	case ODDMR_CMD_OD_INIT_END:
	{
		mtk_oddmr_od_init_end_dual(comp, handle);
		break;
	}
	case ODDMR_CMD_EOF_CHECK_TRIGGER:
	{
		cmdq_pkt_set_event(handle, comp->mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		break;
	}
	case ODDMR_CMD_OD_TUNING_WRITE_SRAM:
	{
		struct mtk_oddmr_od_tuning_sram *tuning_data = data;

		if (tuning_data == NULL) {
			ODDMRFLOW_LOG("%d tuning data is NULL\n", cmd);
			return -EFAULT;
		}
		mtk_oddmr_od_tuning_write_sram_dual(comp, handle, tuning_data);
		break;
	}
	case ODDMR_CMD_ODDMR_REMAP_EN:
	{
		mutex_lock(&g_dbi_data_lock);
		mtk_oddmr_dbi_change_remap_gain(comp, handle, g_oddmr_priv->dbi_data.cur_max_time);
		mtk_oddmr_remap_set_enable(comp, handle, true);
		mutex_unlock(&g_dbi_data_lock);
		break;
	}
	case ODDMR_CMD_ODDMR_REMAP_OFF:
	{
		mtk_oddmr_remap_set_enable(comp, handle, false);
		break;
	}
	case ODDMR_CMD_ODDMR_REMAP_CHG:
	{
		mutex_lock(&g_dbi_data_lock);
		mtk_oddmr_dbi_change_remap_gain(comp, handle, g_oddmr_priv->dbi_data.cur_max_time);
		mutex_unlock(&g_dbi_data_lock);
		break;
	}
	default:
	ODDMRFLOW_LOG("error cmd: %d\n", cmd);
	return -EINVAL;
	}
	return 0;
}

static void disp_oddmr_on_start_of_frame(struct mtk_ddp_comp *comp)
{
	if (!default_comp)
		return;
	if (is_oddmr_od_support == false ||
			g_oddmr_priv->od_state < ODDMR_INIT_DONE ||
			g_oddmr_priv->od_enable == 0)
		return;
	if (!atomic_read(&g_oddmr_sof_irq_available)) {
		atomic_set(&g_oddmr_sof_irq_available, 1);
		wake_up_interruptible(&g_oddmr_sof_irq_wq);
	}
}

static void disp_oddmr_wait_sof_irq(void)
{
	uint32_t weight;
	int ret = 0, sel = 0;
	bool frame_req_trig, pq_req_trig;

	if (atomic_read(&g_oddmr_sof_irq_available) == 0) {
		ODDMRLOW_LOG("wait_event_interruptible\n");
		ret = wait_event_interruptible(g_oddmr_sof_irq_wq,
				atomic_read(&g_oddmr_sof_irq_available) == 1);
		ODDMRLOW_LOG("sof_irq_available = 1, waken up, ret = %d\n", ret);
		if (ret < 0)
			return;
	} else {
		ODDMRFLOW_LOG("sof_irq_available = 0\n");
		return;
	}
	CRTC_MMP_EVENT_START(0, oddmr_sof_thread, 0, 0);
	CRTC_MMP_MARK(0, oddmr_sof_thread, atomic_read(&g_oddmr_od_weight_trigger), 0);
	if (g_oddmr_priv->od_enable) {
		struct mtk_drm_private *priv = default_comp->mtk_crtc->base.dev->dev_private;
		/* 1. restore user weight */
		if (atomic_read(&g_oddmr_od_weight_trigger) > 0) {
			atomic_dec(&g_oddmr_od_weight_trigger);
			if (atomic_read(&g_oddmr_od_weight_trigger) == 0) {
				sel = g_oddmr_priv->od_data.od_sram_read_sel;
				mtk_oddmr_od_gain_lookup(default_comp,
						g_oddmr_current_timing.vrefresh,
						g_oddmr_current_timing.bl_level,
						g_oddmr_priv->od_data.od_sram_table_idx[sel],
						&weight);
				ODDMRFLOW_LOG("weight restore %u\n", weight);
				mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
					default_comp, ODDMR_CMD_OD_SET_WEIGHT, &weight);
				CRTC_MMP_MARK(0, oddmr_sof_thread, weight, 1);
			}
		}
		/* 2. wait until near next frame te */
		frame_req_trig = (atomic_read(&g_oddmr_frame_dirty) == 1);
		pq_req_trig = (atomic_read(&g_oddmr_pq_dirty) == 1);
		atomic_set(&g_oddmr_frame_dirty, 0);
		atomic_set(&g_oddmr_pq_dirty, 0);
		if (priv->data->mmsys_id != MMSYS_MT6989)
		{
			uint32_t second = 1000000, eof;
			u16 fps = g_oddmr_current_timing.vrefresh;

			if (fps <= 0)
				fps = 10;
			eof = 1 * second / fps;

			ODDMRLOW_LOG("fps: %u eof %u\n", fps, eof);
			if (eof >= 2000)
				usleep_range(eof - 2000, eof - 1500);
		}
		CRTC_MMP_MARK(0, oddmr_sof_thread, 0, 2);
		/* 3. check & trigger */
		ODDMRLOW_LOG("frame %d,pq %d,weight_trigger%d\n",
			frame_req_trig,pq_req_trig,atomic_read(&g_oddmr_od_weight_trigger));
		if (frame_req_trig || pq_req_trig ||
				atomic_read(&g_oddmr_od_weight_trigger)) {
			ODDMRLOW_LOG("%d,%d,%d,%d\n",atomic_read(&g_oddmr_frame_dirty),atomic_read(&g_oddmr_pq_dirty),
				mtk_crtc_is_frame_trigger_mode(&default_comp->mtk_crtc->base),g_od_check_trigger);
			if (atomic_read(&g_oddmr_frame_dirty) == 0 &&
				atomic_read(&g_oddmr_pq_dirty) == 0 &&
				mtk_crtc_is_frame_trigger_mode(&default_comp->mtk_crtc->base) &&
				g_od_check_trigger != 2) {
				ODDMRLOW_LOG("check trigger\n");
				mtk_crtc_check_trigger(default_comp->mtk_crtc,
					!!g_od_check_trigger, true);
				//mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
						//default_comp, ODDMR_CMD_EOF_CHECK_TRIGGER, NULL);
				CRTC_MMP_MARK(0, oddmr_sof_thread, 0, 3);
			}
		}
	}
	CRTC_MMP_EVENT_END(0, oddmr_sof_thread, 0, 0);
}
static int mtk_oddmr_sof_irq_trigger(void *data)
{
	while (1) {
		disp_oddmr_wait_sof_irq();
		atomic_set(&g_oddmr_sof_irq_available, 0);
		if (kthread_should_stop()) {
			DDPPR_ERR("%s stopped\n", __func__);
			break;
		}
	}
	return 0;
}

/* top,dither,common pq, udma */
static void mtk_oddmr_od_common_init_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_common_init(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_common_init(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_od_hsk_force_clk(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	mtk_oddmr_write(comp, 1, DISP_ODDMR_OD_HSK_0, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_OD_HSK_1, pkg);
	mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_OD_HSK_2, pkg);
	mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_OD_HSK_3, pkg);
	mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_OD_HSK_4, pkg);
}

static void mtk_oddmr_od_hsk_force_clk_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_hsk_force_clk(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_hsk_force_clk(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_od_gce_pkt_init(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_ddp_comp *comp,
	struct mtk_disp_oddmr *priv, uint32_t dram_id)
{
	//create table  gce for sram 0
	mtk_oddmr_create_gce_pkt(&mtk_crtc->base,
		&priv->od_data.od_sram_pkgs[dram_id][0]);
	//table write in sram 0
	mtk_oddmr_od_init_sram(comp,
		priv->od_data.od_sram_pkgs[dram_id][0], dram_id, 0);
	//create table  gce for sram 1
	mtk_oddmr_create_gce_pkt(&mtk_crtc->base,
		&priv->od_data.od_sram_pkgs[dram_id][1]);
	//table write in sram 1
	mtk_oddmr_od_init_sram(comp,
		priv->od_data.od_sram_pkgs[dram_id][1], dram_id, 1);
}

static int mtk_oddmr_od_init(void)
{
	struct mtk_drm_crtc *mtk_crtc;
	int ret, table_idx;
	struct cmdq_client *client = NULL;

	DDPMSG("%s+\n",__func__);
	if (default_comp == NULL || g_oddmr_priv == NULL || default_comp->mtk_crtc == NULL) {
		ODDMRFLOW_LOG("oddmr comp is not available\n");
		return -1;
	}
	if (!is_oddmr_od_support) {
		ODDMRFLOW_LOG("od is not support\n");
		return -1;
	}
	if (g_oddmr_priv->od_state != ODDMR_LOAD_PARTS) {
		ODDMRFLOW_LOG("od can not init, state %d\n", g_oddmr_priv->od_state);
		return -1;
	}
	if (g_oddmr_priv->od_enable > 0) {
		ODDMRFLOW_LOG("od can not init when running\n");
		return -1;
	}
	if (g_od_param.od_basic_info.basic_param.table_cnt != g_od_param.valid_table_cnt) {
		g_oddmr_priv->od_state = ODDMR_INVALID;
		ODDMRFLOW_LOG("tables are not fully loaded,cnts %d, valid %d\n",
			g_od_param.od_basic_info.basic_param.table_cnt, g_od_param.valid_table_cnt);
		return -1;
	}
	if (!mtk_oddmr_match_panelid(&g_panelid, &g_od_param.od_basic_info.basic_param.panelid)) {
		ODDMRFLOW_LOG("panelid does not match\n");
		return -1;
	}
	mtk_crtc = default_comp->mtk_crtc;
	mtk_drm_set_idlemgr(&mtk_crtc->base, 0, 1);
	mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0) {
		g_oddmr_priv->od_state = ODDMR_LOAD_DONE;
		mtk_oddmr_od_common_init_dual(NULL);
		mtk_oddmr_set_spr2rgb_dual(NULL);
		mtk_oddmr_od_smi_dual(NULL);
		mtk_oddmr_od_alloc_dram_dual();
		mtk_oddmr_od_set_dram_dual(NULL);
		table_idx = _mtk_oddmr_od_table_lookup(g_oddmr_priv, &g_oddmr_current_timing);
		if (table_idx == -1)
			table_idx = 0;
		ODDMRFLOW_LOG("init table_idx %d\n", table_idx);
		//for 6985 force en clk, need to restore od_hsk later
		if (g_oddmr_priv->data->is_od_need_force_clk)
			mtk_oddmr_od_hsk_force_clk_dual(NULL);
		//init srams
		if (mtk_crtc->gce_obj.client[CLIENT_PQ])
			client = mtk_crtc->gce_obj.client[CLIENT_PQ];
		else
			client = mtk_crtc->gce_obj.client[CLIENT_CFG];
		cmdq_mbox_enable(client->chan);
		//sram [0] --> dram [0]
		//sram [1] --> dram [table_idx]
		if (IS_TABLE_VALID(0, g_od_param.valid_table)) {
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 0);
			mtk_oddmr_od_gce_pkt_init(mtk_crtc, default_comp, g_oddmr_priv, 0);
			g_oddmr_priv->od_data.od_sram_table_idx[0] = 0;
			g_oddmr_priv->od_data.od_dram_sel[0] = 0;
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 1);
			//use gce for sram 0
			cmdq_pkt_flush(g_oddmr_priv->od_data.od_sram_pkgs[0][0]);
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 2);
		} else {
			ODDMRFLOW_LOG("table0 must be valid\n");
			g_oddmr_priv->od_state = ODDMR_LOAD_PARTS;
			mtk_oddmr_release_clock();
			mtk_drm_set_idlemgr(&mtk_crtc->base, 1, 1);
			return -1;
		}
		if (IS_TABLE_VALID(1, g_od_param.valid_table)) {
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 10);
			mtk_oddmr_od_gce_pkt_init(mtk_crtc, default_comp, g_oddmr_priv, 1);
			if(table_idx == 0 || table_idx == 1) {
				g_oddmr_priv->od_data.od_sram_table_idx[1] = 1;
				g_oddmr_priv->od_data.od_dram_sel[1] = 1;
				CRTC_MMP_MARK(0, oddmr_ctl, 0, 11);
				//use gce for sram 1
				cmdq_pkt_flush(g_oddmr_priv->od_data.od_sram_pkgs[1][1]);
			}
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 12);
		}

		if (IS_TABLE_VALID(2, g_od_param.valid_table)) {
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 20);
			mtk_oddmr_od_gce_pkt_init(mtk_crtc, default_comp, g_oddmr_priv, 2);
			if(table_idx == 2) {
				g_oddmr_priv->od_data.od_sram_table_idx[1] = 1;
				g_oddmr_priv->od_data.od_dram_sel[1] = 2;
				CRTC_MMP_MARK(0, oddmr_ctl, 0, 21);
				cmdq_pkt_flush(g_oddmr_priv->od_data.od_sram_pkgs[2][1]);
			}
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 22);
		}

		if (IS_TABLE_VALID(3, g_od_param.valid_table)) {
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 30);
			mtk_oddmr_od_gce_pkt_init(mtk_crtc, default_comp, g_oddmr_priv, 3);
			if(table_idx == 3) {
				g_oddmr_priv->od_data.od_sram_table_idx[1] = 1;
				g_oddmr_priv->od_data.od_dram_sel[1] = 3;
				CRTC_MMP_MARK(0, oddmr_ctl, 0, 31);
				cmdq_pkt_flush(g_oddmr_priv->od_data.od_sram_pkgs[3][1]);
			}
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 32);
		}

		if (table_idx == 0) {
			mtk_oddmr_write(default_comp,
				0x10, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
			g_oddmr_priv->od_data.od_sram_read_sel = 0;
		} else {
			mtk_oddmr_write(default_comp,
				0x20, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
			g_oddmr_priv->od_data.od_sram_read_sel = 1;
		}
		//sram [0] --> dram [0]
		//sram [1] --> dram [table_idx]
		if (default_comp->mtk_crtc->is_dual_pipe) {
			if (IS_TABLE_VALID(0, g_od_param.valid_table)) {
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 0);
				mtk_oddmr_od_gce_pkt_init(mtk_crtc, oddmr1_default_comp, g_oddmr1_priv, 0);
				g_oddmr1_priv->od_data.od_sram_table_idx[0] = 0;
				g_oddmr1_priv->od_data.od_dram_sel[0] = 0;
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 1);
				cmdq_pkt_flush(g_oddmr1_priv->od_data.od_sram_pkgs[0][0]);
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 2);
			}

			if (IS_TABLE_VALID(1, g_od_param.valid_table)) {
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 10);
				mtk_oddmr_od_gce_pkt_init(mtk_crtc, oddmr1_default_comp, g_oddmr1_priv, 1);
				if (table_idx == 0 || table_idx == 1) {
					g_oddmr1_priv->od_data.od_sram_table_idx[1] = 1;
					g_oddmr1_priv->od_data.od_dram_sel[1] = 1;
					CRTC_MMP_MARK(0, oddmr_ctl, 1, 11);
					cmdq_pkt_flush(g_oddmr1_priv->od_data.od_sram_pkgs[1][1]);
				}
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 12);
			}

			if (IS_TABLE_VALID(2, g_od_param.valid_table)) {
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 20);
				mtk_oddmr_od_gce_pkt_init(mtk_crtc, oddmr1_default_comp, g_oddmr1_priv, 2);
				if(table_idx == 2) {
					g_oddmr1_priv->od_data.od_sram_table_idx[1] = 1;
					g_oddmr1_priv->od_data.od_dram_sel[1] = 2;
					CRTC_MMP_MARK(0, oddmr_ctl, 1, 21);
					cmdq_pkt_flush(g_oddmr1_priv->od_data.od_sram_pkgs[1][1]);
				}
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 22);
			}

			if (IS_TABLE_VALID(3, g_od_param.valid_table)) {
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 30);
				mtk_oddmr_od_gce_pkt_init(mtk_crtc, oddmr1_default_comp, g_oddmr1_priv, 3);
				if(table_idx == 3) {
					g_oddmr1_priv->od_data.od_sram_table_idx[1] = 1;
					g_oddmr1_priv->od_data.od_dram_sel[1] = 3;
					CRTC_MMP_MARK(0, oddmr_ctl, 0, 31);
					cmdq_pkt_flush(g_oddmr1_priv->od_data.od_sram_pkgs[1][1]);
				}
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 32);
			}

			if (table_idx == 0) {
				mtk_oddmr_write(oddmr1_default_comp,
					0x10, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
				g_oddmr1_priv->od_data.od_sram_read_sel = 0;
			} else {
				mtk_oddmr_write(oddmr1_default_comp,
					0x20, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
				g_oddmr1_priv->od_data.od_sram_read_sel = 1;
			}
		}

		cmdq_mbox_disable(client->chan);

		mtk_oddmr_od_set_res_udma_dual(NULL);
		mtk_oddmr_set_crop_dual(NULL);
		g_oddmr_priv->od_state = ODDMR_INIT_DONE;
		mtk_oddmr_release_clock();
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
				default_comp, ODDMR_CMD_OD_INIT_END, NULL);
	}
	mtk_drm_set_idlemgr(&mtk_crtc->base, 1, 1);
	return ret;
}


static int mtk_oddmr_od_enable(struct drm_device *dev, int en)
{
	int ret = 0, enable = en;

	DDPMSG("%s:%d+\n",__func__,enable);
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("od comp is NULL\n");
		return -1;
	}
	if (g_oddmr_priv->od_state < ODDMR_INIT_DONE) {
		ODDMRFLOW_LOG("od can not enable, state %d\n", g_oddmr_priv->od_state);
		return -EFAULT;
	}
	mtk_drm_idlemgr_kick(__func__,
			&default_comp->mtk_crtc->base, 1);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0)
		mtk_oddmr_release_clock();
	else {
		ODDMRFLOW_LOG("clock not on %d\n", ret);
		return ret;
	}

	g_oddmr_priv->od_enable_req = enable;
	if (default_comp->mtk_crtc->is_dual_pipe)
		g_oddmr1_priv->od_enable_req = enable;
	atomic_set(&g_oddmr_od_hrt_done, 2);
	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev);
	ret = wait_event_interruptible_timeout(g_oddmr_hrt_wq,
			atomic_read(&g_oddmr_od_hrt_done) == 1, msecs_to_jiffies(200));
	if (ret <= 0) {
		ODDMRFLOW_LOG("enable %d repaint timeout %d\n", enable, ret);
		ret = 0;
	}
	return ret;
}

static int mtk_oddmr_od_user_gain(struct mtk_ddp_comp *comp, struct mtk_drm_oddmr_ctl *ctl_data)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	uint8_t od_user_gain = 0;
	int sel;

	if (ctl_data == NULL ||
		ctl_data->data == NULL ||
		ctl_data->size != 1) {
		ODDMRFLOW_LOG("ctl_data is invalid\n");
		return -EFAULT;
	}

	if (copy_from_user(&od_user_gain, ctl_data->data, 1)) {
		ODDMRFLOW_LOG("copy_from_user fail\n");
		return -EFAULT;
	}

	oddmr_priv->od_user_gain = od_user_gain;
	if (comp->mtk_crtc->is_dual_pipe) {
		oddmr_priv = comp_to_oddmr(oddmr1_default_comp);
		oddmr_priv->od_user_gain = od_user_gain;
	}

	sel = oddmr_priv->od_data.od_sram_read_sel;
	atomic_set(&g_oddmr_od_weight_trigger, 1);
	mtk_drm_idlemgr_kick(__func__,
			&comp->mtk_crtc->base, 1);
	mtk_crtc_check_trigger(comp->mtk_crtc, true, true);
	ODDMRFLOW_LOG("weight set user gain %u\n", od_user_gain);
	return 0;
}


static void mtk_oddmr_dmr_common_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	/* top */
	mtk_oddmr_write(comp, 32, DISP_ODDMR_TOP_CTR_1, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CTR_2, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_DMR_BYASS, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CLK_GATING, pkg);
}

static void mtk_oddmr_dmr_common_init_dual(struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	mtk_oddmr_dmr_common_init(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_dmr_common_init(oddmr1_default_comp, pkg);
}




static void mtk_oddmr_dmr_static_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, struct mtk_drm_dmr_static_cfg *static_cfg_data)
{
	uint32_t cnt;
	int i;

	ODDMRAPI_LOG("+\n");
	if (static_cfg_data && static_cfg_data->reg_num &&
		static_cfg_data->reg_offset &&
		static_cfg_data->reg_value) {
		cnt = static_cfg_data->reg_num;
		for (i = 0; i < cnt; i++)
			mtk_oddmr_write_mask(comp, static_cfg_data->reg_value[i],
				static_cfg_data->reg_offset[i],static_cfg_data->reg_mask[i], pkg);
	} else
		ODDMRFLOW_LOG("dmr static config data error\n");

}
int mtk_oddmr_linear_interpolation(int x, int x1, int y1, int x2, int y2)
{
	int y;

	if(x1 == x2)
		return y1;
	y = (100 * y1 + 100 * (y2 - y1)*(x - x1)/(x2 - x1))/100;
	return y;
}

static void mtk_oddmr_dmr_gain_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, unsigned int dbv_node, unsigned int fps_node,
		struct mtk_drm_dmr_cfg_info *cfg_info)
{
	unsigned int cnt;
	int i;
	unsigned int base_idx;
	unsigned int base_idx_fps_add1;
	unsigned int base_idx_dbv_add1;
	unsigned int base_idx_dbv_fps_add1;

	unsigned int dbv_node_add1;
	unsigned int fps_node_add1;
	unsigned int value_interpolate_by_dbv;
	unsigned int value_interpolate_by_dbv1;
	unsigned int value_interpolate_by_fps;
	unsigned int cur_dbv;
	unsigned int cur_fps;
	unsigned long flags;

	/* keep track of chg anytime */
	spin_lock_irqsave(&g_oddmr_timing_lock, flags);
	cur_dbv = g_oddmr_current_timing.bl_level;
	cur_fps = g_oddmr_current_timing.vrefresh;
	spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);

	ODDMRAPI_LOG("+\n");

	if (cfg_info && cfg_info->fps_dbv_change_cfg.reg_offset && cfg_info->fps_dbv_change_cfg.reg_value) {
		cnt = cfg_info->fps_dbv_change_cfg.reg_num;
		if(dbv_node < (cfg_info->fps_dbv_node.DBV_num - 1))
			dbv_node_add1 = dbv_node + 1;
		else
			dbv_node_add1 = dbv_node;

		if(fps_node < (cfg_info->fps_dbv_node.FPS_num- 1))
			fps_node_add1 = fps_node + 1;
		else
			fps_node_add1 = fps_node;


		base_idx = dbv_node * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node * cnt;
		base_idx_fps_add1 = dbv_node * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node_add1 * cnt;
		base_idx_dbv_add1 = dbv_node_add1 * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node * cnt;
		base_idx_dbv_fps_add1 = dbv_node_add1 * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node_add1 * cnt;

		for(i = 0; i < cnt; i++) {

			value_interpolate_by_dbv = mtk_oddmr_linear_interpolation(cur_dbv,
				cfg_info->fps_dbv_node.DBV_node[dbv_node],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx + i],
				cfg_info->fps_dbv_node.DBV_node[dbv_node_add1],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx_dbv_add1 + i]);
			value_interpolate_by_dbv1 = mtk_oddmr_linear_interpolation(cur_dbv,
				cfg_info->fps_dbv_node.DBV_node[dbv_node],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx_fps_add1 + i],
				cfg_info->fps_dbv_node.DBV_node[dbv_node_add1],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx_dbv_fps_add1 + i]);
			value_interpolate_by_fps = mtk_oddmr_linear_interpolation(cur_fps,
				cfg_info->fps_dbv_node.FPS_node[fps_node],
				value_interpolate_by_dbv,
				cfg_info->fps_dbv_node.FPS_node[fps_node_add1],
				value_interpolate_by_dbv1);

			mtk_oddmr_write_mask(comp,
				value_interpolate_by_fps,
				cfg_info->fps_dbv_change_cfg.reg_offset[i],
				cfg_info->fps_dbv_change_cfg.reg_mask[i], pkg);
		}
	} else
		ODDMRFLOW_LOG("dmr static config data error\n");
}

static void mtk_oddmr_dbi_gain_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, unsigned int dbv_node, unsigned int fps_node,
		struct mtk_drm_dbi_cfg_info *cfg_info, unsigned int gain_ratio)
{
	unsigned int cnt;
	int i;
	unsigned int base_idx;
	unsigned int base_idx_fps_add1;
	unsigned int base_idx_dbv_add1;
	unsigned int base_idx_dbv_fps_add1;

	unsigned int dbv_node_add1;
	unsigned int fps_node_add1;
	unsigned int value_interpolate_by_dbv;
	unsigned int value_interpolate_by_dbv1;
	unsigned int value_interpolate_by_fps;
	unsigned int cur_dbv;
	unsigned int cur_fps;
	unsigned long flags;

	/* keep track of chg anytime */
	spin_lock_irqsave(&g_oddmr_timing_lock, flags);
	cur_dbv = g_oddmr_current_timing.bl_level;
	cur_fps = g_oddmr_current_timing.vrefresh;
	spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);

	ODDMRAPI_LOG("+\n");

	if (cfg_info && cfg_info->fps_dbv_change_cfg.reg_offset && cfg_info->fps_dbv_change_cfg.reg_value) {
		cnt = cfg_info->fps_dbv_change_cfg.reg_num;
		if(dbv_node < (cfg_info->fps_dbv_node.DBV_num - 1))
			dbv_node_add1 = dbv_node + 1;
		else
			dbv_node_add1 = dbv_node;

		if(fps_node < (cfg_info->fps_dbv_node.FPS_num- 1))
			fps_node_add1 = fps_node + 1;
		else
			fps_node_add1 = fps_node;


		base_idx = dbv_node * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node * cnt;
		base_idx_fps_add1 = dbv_node * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node_add1 * cnt;
		base_idx_dbv_add1 = dbv_node_add1 * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node * cnt;
		base_idx_dbv_fps_add1 = dbv_node_add1 * (cfg_info->fps_dbv_node.FPS_num) * cnt + fps_node_add1 * cnt;

		for(i = 0; i < cnt; i++) {
			value_interpolate_by_dbv = mtk_oddmr_linear_interpolation(cur_dbv,
				cfg_info->fps_dbv_node.DBV_node[dbv_node],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx + i],
				cfg_info->fps_dbv_node.DBV_node[dbv_node_add1],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx_dbv_add1 + i]);
			value_interpolate_by_dbv1 = mtk_oddmr_linear_interpolation(cur_dbv,
				cfg_info->fps_dbv_node.DBV_node[dbv_node],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx_fps_add1 + i],
				cfg_info->fps_dbv_node.DBV_node[dbv_node_add1],
				cfg_info->fps_dbv_change_cfg.reg_value[base_idx_dbv_fps_add1 + i]);
			value_interpolate_by_fps = mtk_oddmr_linear_interpolation(cur_fps,
				cfg_info->fps_dbv_node.FPS_node[fps_node],
				value_interpolate_by_dbv,
				cfg_info->fps_dbv_node.FPS_node[fps_node_add1],
				value_interpolate_by_dbv1);

			ODDMRAPI_LOG("gain_ratio : %d\n", gain_ratio);
			value_interpolate_by_fps *= gain_ratio;
			value_interpolate_by_fps /= 100;

			mtk_oddmr_write_mask(comp,
				value_interpolate_by_fps,
				cfg_info->fps_dbv_change_cfg.reg_offset[i],
				cfg_info->fps_dbv_change_cfg.reg_mask[i], pkg);
		}
	} else
		ODDMRFLOW_LOG("dbi gain config data error\n");
}


static void mtk_oddmr_dbi_dbv_table_cfg(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, unsigned int dbv_node,
		struct mtk_drm_dbi_cfg_info *cfg_info)
{
	unsigned int cnt;
	int i,j;
	unsigned int base_idx;
	unsigned int base_idx_dbv_add1;

	unsigned int dbv_node_add1;
	unsigned int value_interpolate_by_dbv;

	unsigned int cur_dbv;
	unsigned int cur_fps;
	unsigned long flags;
	int index[7] = {3, 4, 6, 8, 12, 18, 20};
	int value[7] = {4, 8, 32, 64, 128, 224, 255};

	/* keep track of chg anytime */
	spin_lock_irqsave(&g_oddmr_timing_lock, flags);
	cur_dbv = g_oddmr_current_timing.bl_level;
	cur_fps = g_oddmr_current_timing.vrefresh;
	spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);

	ODDMRAPI_LOG("+\n");

	if (cfg_info && cfg_info->dbv_change_cfg.reg_offset && cfg_info->dbv_change_cfg.reg_value) {
		cnt = cfg_info->dbv_change_cfg.reg_num;
		if(dbv_node < (cfg_info->fps_dbv_node.DBV_num - 1))
			dbv_node_add1 = dbv_node + 1;
		else
			dbv_node_add1 = dbv_node;

		base_idx = dbv_node * cfg_info->dbv_change_cfg.reg_num;
		base_idx_dbv_add1 = dbv_node_add1 * cfg_info->dbv_change_cfg.reg_num;

		ODDMRAPI_LOG("dbv num  %d\n", cfg_info->dbv_node.DBV_num);
		ODDMRAPI_LOG("curdbv : %d\n", cur_dbv);
		for(i = 0; i < cnt; i++) {

			value_interpolate_by_dbv = mtk_oddmr_linear_interpolation(cur_dbv,
				cfg_info->dbv_node.DBV_node[dbv_node],
				cfg_info->dbv_change_cfg.reg_value[base_idx + i],
				cfg_info->dbv_node.DBV_node[dbv_node_add1],
				cfg_info->dbv_change_cfg.reg_value[base_idx_dbv_add1 + i]);

			for(j = 0; j < 7; j++) {
				if(i%21 == index[j]) {
					ODDMRAPI_LOG("dbv_node : %d, value:%d\n",
						cfg_info->dbv_node.DBV_node[dbv_node],
						cfg_info->dbv_change_cfg.reg_value[base_idx + i]/4);
					ODDMRAPI_LOG("dbv_node+1 : %d, value:%d\n",
						cfg_info->dbv_node.DBV_node[dbv_node_add1],
						cfg_info->dbv_change_cfg.reg_value[base_idx_dbv_add1 + i]/4);
					if(i/21 == 0)
						ODDMRAPI_LOG("R gray %d : %d\n", value[j],
							value_interpolate_by_dbv/4);
					if(i/21 == 1)
						ODDMRAPI_LOG("G gray %d : %d\n", value[j],
							value_interpolate_by_dbv/4);
					if(i/21 == 2)
						ODDMRAPI_LOG("B gray %d : %d\n", value[j],
							value_interpolate_by_dbv/4);
				}
			}
			mtk_oddmr_write_mask(comp,
				value_interpolate_by_dbv,
				cfg_info->dbv_change_cfg.reg_offset[i],
				cfg_info->dbv_change_cfg.reg_mask[i], pkg);
		}
	} else
		ODDMRFLOW_LOG("dbi dbv table config data error\n");
}



static int mtk_oddmr_get_dmr_cfg_data(struct mtk_drm_dmr_cfg_info *cfg_info)
{
	void *data[20] = {0};
	int index = 0;
	int size;
	int i;
	int static_log=10;
	int change_log=10;
	int table_log = 10;
	struct mtk_drm_dmr_cfg_info *dmr_cfg_data = &g_oddmr_priv->dmr_cfg_info;

	ODDMRAPI_LOG("+\n");
	memcpy(dmr_cfg_data, cfg_info, sizeof(struct mtk_drm_dmr_cfg_info));

	ODDMRLOW_LOG("basic_info.panel_id_len %d\n", dmr_cfg_data->basic_info.panel_id_len);
	ODDMRLOW_LOG("basic_info.panel_width %d\n", dmr_cfg_data->basic_info.panel_width);
	ODDMRLOW_LOG("basic_info.panel_height %d\n", dmr_cfg_data->basic_info.panel_height);
	ODDMRLOW_LOG("basic_info.catch_bit %d\n", dmr_cfg_data->basic_info.catch_bit);
	ODDMRLOW_LOG("basic_info.blank_bit %d\n", dmr_cfg_data->basic_info.blank_bit);
	ODDMRLOW_LOG("basic_info.zero_bit %d\n", dmr_cfg_data->basic_info.zero_bit);
	ODDMRLOW_LOG("basic_info.h_num %d\n", dmr_cfg_data->basic_info.h_num);
	ODDMRLOW_LOG("basic_info.v_num %d\n", dmr_cfg_data->basic_info.v_num);

	if(dmr_cfg_data->static_cfg.reg_num) {
		size = sizeof(uint32_t) * dmr_cfg_data->static_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
				__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->static_cfg.reg_value, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->static_cfg.reg_value = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->static_cfg.reg_offset, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->static_cfg.reg_offset = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->static_cfg.reg_mask, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->static_cfg.reg_mask = (uint32_t *)data[index];
		index++;
	}

	if(static_log > dmr_cfg_data->static_cfg.reg_num)
		static_log = dmr_cfg_data->static_cfg.reg_num;

	if(dmr_cfg_data->fps_dbv_node.DBV_num){
		ODDMRLOW_LOG("dmr copy dbv node: %d", dmr_cfg_data->fps_dbv_node.DBV_num);
		size = sizeof(uint32_t) * dmr_cfg_data->fps_dbv_node.DBV_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->fps_dbv_node.DBV_node, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->fps_dbv_node.DBV_node = (uint32_t *)data[index];
		index++;
	}

	if(dmr_cfg_data->fps_dbv_node.FPS_num) {
		ODDMRLOW_LOG("dmr copy fps node: %d", dmr_cfg_data->fps_dbv_node.FPS_num);
		size = sizeof(uint32_t) * dmr_cfg_data->fps_dbv_node.FPS_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->fps_dbv_node.FPS_node, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->fps_dbv_node.FPS_node = (uint32_t *)data[index];
		index++;
	}

	if(dmr_cfg_data->fps_dbv_change_cfg.reg_num){
		size = sizeof(uint32_t) * dmr_cfg_data->fps_dbv_change_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->fps_dbv_change_cfg.reg_offset, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->fps_dbv_change_cfg.reg_offset = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->fps_dbv_change_cfg.reg_mask, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->fps_dbv_change_cfg.reg_mask = (uint32_t *)data[index];
		index++;

		size = sizeof(uint32_t) * dmr_cfg_data->fps_dbv_node.FPS_num
			* dmr_cfg_data->fps_dbv_node.DBV_num * dmr_cfg_data->fps_dbv_change_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}

		if (dmr_cfg_data->fps_dbv_node.DC_flag) {
			if (copy_from_user(data[index], dmr_cfg_data->fps_dbv_change_cfg.reg_DC_value, size)) {
				DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
				goto fail;
			}
			dmr_cfg_data->fps_dbv_change_cfg.reg_DC_value= (uint32_t *)data[index];
		}else{
			if (copy_from_user(data[index], dmr_cfg_data->fps_dbv_change_cfg.reg_value, size)) {
				DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
				goto fail;
			}
			dmr_cfg_data->fps_dbv_change_cfg.reg_value= (uint32_t *)data[index];
		}
		index++;
	}

	if(change_log > dmr_cfg_data->fps_dbv_change_cfg.reg_num)
		change_log = dmr_cfg_data->fps_dbv_change_cfg.reg_num;

	if(dmr_cfg_data->table_index.DBV_table_num){
		ODDMRLOW_LOG("dmr copy DBV table index: %d", dmr_cfg_data->table_index.DBV_table_num);
		size = sizeof(uint32_t) * dmr_cfg_data->table_index.DBV_table_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->table_index.DBV_table_idx, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->table_index.DBV_table_idx = (uint32_t *)data[index];
		index++;
	}

	if (dmr_cfg_data->table_index.FPS_table_num) {
		ODDMRLOW_LOG("dmr copy FPS table index: %d", dmr_cfg_data->table_index.FPS_table_num);
		size = sizeof(uint32_t) * dmr_cfg_data->table_index.FPS_table_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dmr_cfg_data->table_index.FPS_table_idx, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dmr_cfg_data->table_index.FPS_table_idx = (uint32_t *)data[index];
		index++;
	}
	if (dmr_cfg_data->table_index.table_byte_num) {
		size = dmr_cfg_data->table_index.table_byte_num
			* dmr_cfg_data->table_index.DBV_table_num * dmr_cfg_data->table_index.FPS_table_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (dmr_cfg_data->table_index.DC_table_flag) {
			if (copy_from_user(data[index], dmr_cfg_data->table_content.table_single_DC, size)) {
				DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
				goto fail;
			}
			dmr_cfg_data->table_content.table_single_DC = (unsigned char *)data[index];
		}else{
			if (copy_from_user(data[index], dmr_cfg_data->table_content.table_single, size)) {
				DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
				goto fail;
			}
			dmr_cfg_data->table_content.table_single= (unsigned char *)data[index];
		}
	}

	if(table_log > dmr_cfg_data->table_index.table_byte_num)
		table_log = dmr_cfg_data->table_index.table_byte_num;
	for(i = 0; i < table_log; i++)
		ODDMRLOW_LOG("table_content %d\n", dmr_cfg_data->table_content.table_single[i]);
	g_oddmr_priv->dmr_state = ODDMR_LOAD_DONE;
	return 0;

fail:
	for (i = 0; i<ARRAY_SIZE(data); i++) {
		if (data[i])
			vfree(data[i]);

	}
	return -EFAULT;

}

void *oddmr_realloc_buffer(void *a, int old_size, int new_size)
{
	void *b;

	b = vmalloc(new_size);
	if (b) {
		memcpy(b,a,old_size);
		vfree(a);
	}
	return b;
}

void insert_buffer(struct bitstream_buffer *a, uint8_t element)
{
	uint8_t *extended_array = NULL;

	if (a->used_entry == a->size) {
		a->size += 10; // extend size for 10 entries each time
		extended_array = (uint8_t *)oddmr_realloc_buffer(a->_buffer, a->size-10, a->size);

		if (extended_array != NULL)
			a->_buffer = extended_array;
		else {
			// Fail to extend array size
			a->_buffer = NULL;
		}
	}
	if(a->_buffer)
		a->_buffer[a->used_entry++] = element;
}

void init_buffer(struct bitstream_buffer *a, uint32_t initialSize)
{
	a->_buffer = vmalloc(initialSize * sizeof(uint8_t));
	a->used_entry = 0;
	a->used_bit = 0;
	a->size = initialSize;
}

void free_buffer(struct bitstream_buffer *a)
{
	vfree(a->_buffer);
	a->_buffer = NULL;
	a->used_entry = a->size = 0;
}


void init_read(struct bitstream_buffer *a)
{
	a->read_bit = 0;
}

int get_buffer(struct bitstream_buffer *a, int bits)
{
	int data = 0;
	unsigned int res_bits = a->read_bit & 0x7;
	int rem_bytes;
	unsigned int read_bits;

	if (res_bits > 0) {
		read_bits = MIN(8 - res_bits, (unsigned int)bits);
		data = (a->_buffer[a->read_bit >> 3] >> (8 - res_bits - read_bits)) & ((1 << read_bits) - 1);

		bits -= read_bits;
		a->read_bit += read_bits;
	}

	rem_bytes = bits >> 3;
	for (int i = 0; i < rem_bytes; ++i) {
		data = (data << 8) | a->_buffer[a->read_bit >> 3];
		bits -= 8;
		a->read_bit += 8;
	}

	if (bits > 0) {
		data = (data << bits) | (a->_buffer[a->read_bit >> 3] >> (8 - bits));
		a->read_bit += bits;
	}

	return data;
}

void put_buffer(struct bitstream_buffer *a, uint32_t data, uint32_t data_bits)
{
	uint32_t bytes = a->used_bit >> 3;
	uint32_t res_bits = a->used_bit & 0x7;
	int rem_bytes;
	uint32_t push_bits;

	a->used_bit += data_bits;
	if (res_bits > 0) {
		push_bits = MIN(8 - res_bits, data_bits);
		a->_buffer[bytes] = a->_buffer[bytes] | (
			(((1 << push_bits) - 1) & (data >> (data_bits - push_bits))
				) << (8 - res_bits - push_bits)
			);
		data_bits -= push_bits;
		data = data & ((1 << data_bits) - 1);
	}
	rem_bytes = data_bits >> 3;
	for (int i = 0; i < rem_bytes; ++i) {
		insert_buffer(a, (data >> (data_bits - 8)));
		data_bits -= 8;
	}

	if (data_bits > 0)
		insert_buffer(a, (data & ((1 << data_bits) - 1)) << (8 - data_bits));
}

unsigned char _reverse(unsigned char n)
{
	unsigned char tmp = n & 0xf;
	unsigned char tmp1 = n >> 4;

	// Reverse the top and bottom nibble then swap them.
	return (lookup[tmp] << 4) | lookup[tmp1];
}

int mtk_oddmr_dbi_copy_static_cfg(struct bitstream_buffer *ret_drm_data,
	struct mtk_drm_dbi_cfg_info *dbi_bin_struct)
{
	int size = sizeof(uint32_t) * dbi_bin_struct->static_cfg.reg_num;

	if (copy_from_user(dbi_bin_struct->static_cfg.reg_value,
		ret_drm_data->static_cfg.reg_value, size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		return -1;
	}
	if (copy_from_user(dbi_bin_struct->static_cfg.reg_offset,
		ret_drm_data->static_cfg.reg_offset, size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		return -1;
	}
	if (copy_from_user(dbi_bin_struct->static_cfg.reg_mask,
		ret_drm_data->static_cfg.reg_mask, size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		return -1;
	}
	return 0;
}

int mtk_oddmr_dbi_copy_chg_cfg(struct bitstream_buffer *ret_drm_data,
	struct mtk_drm_dbi_cfg_info *dbi_bin_struct)
{
	int size = sizeof(uint32_t) * dbi_bin_struct->fps_dbv_change_cfg.reg_num;

	if (copy_from_user(dbi_bin_struct->fps_dbv_change_cfg.reg_value,
		ret_drm_data->fps_dbv_change_cfg.reg_value, size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		return -1;
	}
	if (copy_from_user(dbi_bin_struct->fps_dbv_change_cfg.reg_offset,
		ret_drm_data->fps_dbv_change_cfg.reg_offset, size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		return -1;
	}
	if (copy_from_user(dbi_bin_struct->fps_dbv_change_cfg.reg_mask,
		ret_drm_data->fps_dbv_change_cfg.reg_mask, size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		return -1;
	}

	size = sizeof(uint32_t) * dbi_bin_struct->fps_dbv_node.FPS_num
		* dbi_bin_struct->fps_dbv_node.DBV_num * dbi_bin_struct->fps_dbv_change_cfg.reg_num;

	if (dbi_bin_struct->fps_dbv_node.DC_flag) {
		if (copy_from_user(dbi_bin_struct->fps_dbv_change_cfg.reg_DC_value,
			ret_drm_data->fps_dbv_change_cfg.reg_DC_value, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			return -1;
		}
	} else {
		if (copy_from_user(dbi_bin_struct->fps_dbv_change_cfg.reg_value,
			ret_drm_data->fps_dbv_change_cfg.reg_value, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			return -1;
		}
	}
	return 0;
}


void kernel_dbi_table_update(struct bitstream_buffer *ret_drm_data)
{
	struct mtk_drm_dbi_cfg_info *dbi_bin_struct = &g_oddmr_priv->dbi_cfg_info;
	struct mtk_drm_dbi_cfg_info *dbi_bin_struct_tb1 = &g_oddmr_priv->dbi_cfg_info_tb1;
	int h_bit_cnt = ((dbi_bin_struct->basic_info.catch_bit + dbi_bin_struct->basic_info.blank_bit)
		* dbi_bin_struct->basic_info.h_num);
	int stuff_bit = 128 - (h_bit_cnt % 128);
	int use_byte = (h_bit_cnt + stuff_bit)*dbi_bin_struct->basic_info.v_num / 8;
	struct bitstream_buffer dbi_table_real;
	struct bitstream_buffer *dbi_table = &dbi_table_real;
	static int first_update;
	int line_stuff_bit;
	const int catch_once = 32;
	int catch_bit;

	init_buffer(dbi_table, use_byte);
	init_read(ret_drm_data);

	for (int y = 0; y < dbi_bin_struct->basic_info.v_num; y++) {
		for (int x = 0; x < dbi_bin_struct->basic_info.h_num; x++) {
			catch_bit = dbi_bin_struct->basic_info.catch_bit;
			while(catch_bit > 0) {
				if(catch_bit > catch_once)
					put_buffer(dbi_table, get_buffer(ret_drm_data, catch_once), catch_once);
				else
					put_buffer(dbi_table, get_buffer(ret_drm_data, catch_bit), catch_bit);
				catch_bit -= catch_once;
			}
			put_buffer(dbi_table, 0, dbi_bin_struct->basic_info.blank_bit);
		}
		line_stuff_bit = dbi_table->used_bit % 128;
		if (line_stuff_bit != 0)
			put_buffer(dbi_table, 0, (128-line_stuff_bit));
	}

	for (int i = 0; i < dbi_table->used_entry; i++)
		dbi_table->_buffer[i] = _reverse(dbi_table->_buffer[i]);

	if(!first_update) {
		first_update++;
		g_oddmr_priv->dbi_data.dbi_table[0]=mtk_oddmr_load_buffer(
				&default_comp->mtk_crtc->base, dbi_table->used_entry, dbi_table->_buffer, false);
		g_oddmr_priv->dbi_data.dbi_table[1]=mtk_oddmr_load_buffer(
				&default_comp->mtk_crtc->base, dbi_table->used_entry, dbi_table->_buffer, false);

		if(mtk_oddmr_dbi_copy_static_cfg(ret_drm_data, dbi_bin_struct)) {
			DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
			goto final;
		}
		if(mtk_oddmr_dbi_copy_static_cfg(ret_drm_data, dbi_bin_struct_tb1)) {
			DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
			goto final;
		}
		if(mtk_oddmr_dbi_copy_chg_cfg(ret_drm_data, dbi_bin_struct)) {
			DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
			goto final;
		}
		if(mtk_oddmr_dbi_copy_chg_cfg(ret_drm_data, dbi_bin_struct_tb1)) {
			DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
			goto final;
		}

		atomic_set(&g_oddmr_priv->dbi_data.update_table_done, 1);
		atomic_set(&g_oddmr_priv->dbi_data.cur_table_idx, 0);
		atomic_set(&g_oddmr_priv->dbi_data.update_table_idx, 0);
		g_oddmr_priv->dbi_data.table_size =  dbi_table->used_entry;

	} else {
		if(atomic_read(&g_oddmr_priv->dbi_data.cur_table_idx)){
			if(mtk_oddmr_dbi_copy_static_cfg(ret_drm_data, dbi_bin_struct)) {
				DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
				goto final;
			}
			if(mtk_oddmr_dbi_copy_chg_cfg(ret_drm_data, dbi_bin_struct)) {
				DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
				goto final;
			}

		memcpy(g_oddmr_priv->dbi_data.dbi_table[0]->kvaddr, dbi_table->_buffer, dbi_table->used_entry);
		atomic_set(&g_oddmr_priv->dbi_data.update_table_idx, 0);
		} else {
			if(mtk_oddmr_dbi_copy_static_cfg(ret_drm_data, dbi_bin_struct_tb1)) {
				DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
				goto final;
			}
			if(mtk_oddmr_dbi_copy_chg_cfg(ret_drm_data, dbi_bin_struct_tb1)) {
				DDPMSG("%s:%d,copy_from_user fail\n", __func__, __LINE__);
				goto final;
			}

		memcpy(g_oddmr_priv->dbi_data.dbi_table[1]->kvaddr, dbi_table->_buffer, dbi_table->used_entry);
		atomic_set(&g_oddmr_priv->dbi_data.update_table_idx, 1);
		}
	}
final:
	free_buffer(dbi_table);
	return;
}


static int mtk_oddmr_dbi_init(struct mtk_drm_dbi_cfg_info *cfg_info)
{
	struct mtk_oddmr_panelid expect_panel_id = {0};
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data;
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data_tb1;
	void *data[30] = {0};
	unsigned int size;
	unsigned int index = 0;
	int i;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL || default_comp->mtk_crtc == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}
	if (!is_oddmr_dbi_support) {
		ODDMRFLOW_LOG("dmr is not support\n");
		return -1;
	}
	if (g_oddmr_priv->dbi_state != ODDMR_INVALID) {
		ODDMRFLOW_LOG("dmr can not init, state %d\n", g_oddmr_priv->dmr_state);
		return -1;
	}
	if (g_oddmr_priv->dbi_enable > 0) {
		ODDMRFLOW_LOG("dmr can not init when running\n");
		return -1;
	}

	if(!cfg_info){
		ODDMRFLOW_LOG("dmr config info is NULL\n");
		return -1;
	}

	dbi_cfg_data =	&g_oddmr_priv->dbi_cfg_info;
	dbi_cfg_data_tb1 = &g_oddmr_priv->dbi_cfg_info_tb1;
	memcpy(dbi_cfg_data, cfg_info, sizeof(struct mtk_drm_dbi_cfg_info));

	/*match panel id */
	expect_panel_id.len = dbi_cfg_data->basic_info.panel_id_len;
	if(expect_panel_id.len)
		memcpy(expect_panel_id.data, dbi_cfg_data->basic_info.panel_id, expect_panel_id.len);
	if (!mtk_oddmr_match_panelid(&g_panelid, &expect_panel_id)) {
		ODDMRFLOW_LOG("panelid does not match\n");
		return -1;
	}

	dbi_cfg_data_tb1->static_cfg.reg_num = dbi_cfg_data->static_cfg.reg_num;
	dbi_cfg_data_tb1->fps_dbv_node.FPS_num = dbi_cfg_data->fps_dbv_node.FPS_num;
	dbi_cfg_data_tb1->fps_dbv_node.DBV_num = dbi_cfg_data->fps_dbv_node.DBV_num;
	dbi_cfg_data_tb1->fps_dbv_node.DC_flag = dbi_cfg_data->fps_dbv_node.DC_flag;
	dbi_cfg_data_tb1->fps_dbv_change_cfg.reg_num = dbi_cfg_data->fps_dbv_change_cfg.reg_num;

	if(dbi_cfg_data->static_cfg.reg_num) {
		size = sizeof(uint32_t) * dbi_cfg_data->static_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
				__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->static_cfg.reg_value, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->static_cfg.reg_value = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->static_cfg.reg_offset, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->static_cfg.reg_offset = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->static_cfg.reg_mask, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->static_cfg.reg_mask = (uint32_t *)data[index];
		index++;

		//for other table
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data_tb1->static_cfg.reg_offset= (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data_tb1->static_cfg.reg_value= (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data_tb1->static_cfg.reg_mask= (uint32_t *)data[index];
		index++;

	}



	if(dbi_cfg_data->fps_dbv_node.DBV_num){
		ODDMRLOW_LOG("dbi copy dbv node: %d", dbi_cfg_data->fps_dbv_node.DBV_num);
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.DBV_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_node.DBV_node, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_node.DBV_node = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data_tb1->fps_dbv_node.DBV_node = (uint32_t *)data[index];
		index++;
		memcpy(dbi_cfg_data_tb1->fps_dbv_node.DBV_node,
			dbi_cfg_data->fps_dbv_node.DBV_node, size);
	}



	if(dbi_cfg_data->fps_dbv_node.FPS_num) {
		ODDMRLOW_LOG("dbi copy fps node: %d", dbi_cfg_data->fps_dbv_node.FPS_num);
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.FPS_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_node.FPS_node, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_node.FPS_node = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data_tb1->fps_dbv_node.FPS_node = (uint32_t *)data[index];
		index++;
		memcpy(dbi_cfg_data_tb1->fps_dbv_node.FPS_node,
			dbi_cfg_data->fps_dbv_node.FPS_node, size);
	}

	if(dbi_cfg_data->fps_dbv_node.remap_reduce_offset_num) {
		ODDMRLOW_LOG("dmr copy remap node: %d", dbi_cfg_data->fps_dbv_node.remap_reduce_offset_num);
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.remap_reduce_offset_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_node.remap_reduce_offset_node, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_node.remap_reduce_offset_node = (uint32_t *)data[index];
		index++;
	}

	if(dbi_cfg_data->fps_dbv_node.remap_reduce_offset_num) {
		ODDMRLOW_LOG("dmr copy remap node: %d", dbi_cfg_data->fps_dbv_node.remap_reduce_offset_num);
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.remap_reduce_offset_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_node.remap_reduce_offset_value, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_node.remap_reduce_offset_value = (uint32_t *)data[index];
		index++;
	}

	if(dbi_cfg_data->fps_dbv_node.remap_dbv_gain_num) {
		ODDMRLOW_LOG("dmr copy remap dbv node: %d", dbi_cfg_data->fps_dbv_node.remap_dbv_gain_num);
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.remap_dbv_gain_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_node.remap_dbv_gain_node, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_node.remap_dbv_gain_node = (uint32_t *)data[index];
		index++;
	}

	if(dbi_cfg_data->fps_dbv_node.remap_dbv_gain_num) {
		ODDMRLOW_LOG("dmr copy remap dbv node: %d", dbi_cfg_data->fps_dbv_node.remap_dbv_gain_num);
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.remap_dbv_gain_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_node.remap_dbv_gain_value, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_node.remap_dbv_gain_value = (uint32_t *)data[index];
		index++;
	}



	if(dbi_cfg_data->fps_dbv_change_cfg.reg_num){
		ODDMRLOW_LOG("dmr copy fps_dbv_change_cfg: %d", dbi_cfg_data->fps_dbv_change_cfg.reg_num);
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_change_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_change_cfg.reg_offset, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_change_cfg.reg_offset = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->fps_dbv_change_cfg.reg_mask, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->fps_dbv_change_cfg.reg_mask = (uint32_t *)data[index];
		index++;

		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.FPS_num
			* dbi_cfg_data->fps_dbv_node.DBV_num * dbi_cfg_data->fps_dbv_change_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}

		if (dbi_cfg_data->fps_dbv_node.DC_flag) {
			if (copy_from_user(data[index], dbi_cfg_data->fps_dbv_change_cfg.reg_DC_value, size)) {
				DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
				goto fail;
			}
			dbi_cfg_data->fps_dbv_change_cfg.reg_DC_value= (uint32_t *)data[index];
		}else{
			if (copy_from_user(data[index], dbi_cfg_data->fps_dbv_change_cfg.reg_value, size)) {
				DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
				goto fail;
			}
			dbi_cfg_data->fps_dbv_change_cfg.reg_value= (uint32_t *)data[index];
		}
		index++;

		//for other table
		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_change_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data_tb1->fps_dbv_change_cfg.reg_offset = (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data_tb1->fps_dbv_change_cfg.reg_mask= (uint32_t *)data[index];
		index++;

		size = sizeof(uint32_t) * dbi_cfg_data->fps_dbv_node.FPS_num
			* dbi_cfg_data->fps_dbv_node.DBV_num * dbi_cfg_data->fps_dbv_change_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (dbi_cfg_data->fps_dbv_node.DC_flag)
			dbi_cfg_data_tb1->fps_dbv_change_cfg.reg_DC_value= (uint32_t *)data[index];
		else
			dbi_cfg_data_tb1->fps_dbv_change_cfg.reg_value= (uint32_t *)data[index];
		index++;
	}

	if(dbi_cfg_data->dbv_node.DBV_num) {
		ODDMRLOW_LOG("dbi copy dbv node: %d", dbi_cfg_data->dbv_node.DBV_num);
		size = sizeof(uint32_t) * dbi_cfg_data->dbv_node.DBV_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->dbv_node.DBV_node, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->dbv_node.DBV_node= (uint32_t *)data[index];
		index++;
	}

	if(dbi_cfg_data->dbv_change_cfg.reg_num) {
		ODDMRLOW_LOG("dbi copy dbv chg cfg num: %d", dbi_cfg_data->dbv_change_cfg.reg_num);
		size = sizeof(uint32_t) * dbi_cfg_data->dbv_change_cfg.reg_num;
		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->dbv_change_cfg.reg_offset, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->dbv_change_cfg.reg_offset= (uint32_t *)data[index];
		index++;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->dbv_change_cfg.reg_mask, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->dbv_change_cfg.reg_mask= (uint32_t *)data[index];
		index++;

		size = sizeof(uint32_t) * dbi_cfg_data->dbv_change_cfg.reg_num * dbi_cfg_data->dbv_node.DBV_num;

		data[index] = vmalloc(size);
		if (!data[index]) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
			__func__, __LINE__);
			goto fail;
		}
		if (copy_from_user(data[index],
			dbi_cfg_data->dbv_change_cfg.reg_value, size)) {
			DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			goto fail;
		}
		dbi_cfg_data->dbv_change_cfg.reg_value= (uint32_t *)data[index];
		index++;
	}


	g_oddmr_priv->dbi_state = ODDMR_INIT_DONE;

	return 0;

fail:
	for (i = 0; i<ARRAY_SIZE(data); i++) {
		if (data[i])
			vfree(data[i]);

	}
	return -EFAULT;


}


static int mtk_oddmr_dmr_init(struct mtk_drm_dmr_cfg_info *cfg_info)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_dmr_cfg_info *dmr_cfg_data;
	int ret = 0;
	unsigned int dbv_table_idx = 0;
	unsigned int dbv_node = 0;
	unsigned int fps_table_idx = 0;
	unsigned int fps_node = 0;
	dma_addr_t addr = 0;
	struct mtk_oddmr_panelid expect_panel_id = {0};

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL || default_comp->mtk_crtc == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}
	if (!is_oddmr_dmr_support) {
		ODDMRFLOW_LOG("dmr is not support\n");
		return -1;
	}
	if (g_oddmr_priv->dmr_state != ODDMR_INVALID) {
		ODDMRFLOW_LOG("dmr can not init, state %d\n", g_oddmr_priv->dmr_state);
		return -1;
	}
	if (g_oddmr_priv->dmr_enable > 0) {
		ODDMRFLOW_LOG("dmr can not init when running\n");
		return -1;
	}

	if(!cfg_info){
		ODDMRFLOW_LOG("dmr config info is NULL\n");
		return -1;
	}

	if (mtk_oddmr_get_dmr_cfg_data(cfg_info)) {
		ODDMRFLOW_LOG("get dmr config data fail\n");
		return -1;
	}

	dmr_cfg_data =	&g_oddmr_priv->dmr_cfg_info;

	expect_panel_id.len = dmr_cfg_data->basic_info.panel_id_len;
	if(expect_panel_id.len)
		memcpy(expect_panel_id.data, dmr_cfg_data->basic_info.panel_id, expect_panel_id.len);

	if (!mtk_oddmr_match_panelid(&g_panelid, &expect_panel_id)) {
		ODDMRFLOW_LOG("panelid does not match\n");
		return -1;
	}

	if (mtk_oddmr_dmr_alloc_table(dmr_cfg_data)) {
		ODDMRFLOW_LOG("dmr alloc table fail\n");
		return -1;
	}

	if(mtk_oddmr_dmr_dbv_lookup(g_oddmr_current_timing.bl_level, dmr_cfg_data, &dbv_table_idx, &dbv_node)){
		ODDMRFLOW_LOG("dmr dbv lookup fail\n");
		return -1;
	}

	if(mtk_oddmr_dmr_fps_lookup(g_oddmr_current_timing.vrefresh, dmr_cfg_data, &fps_table_idx, &fps_node)){
		ODDMRFLOW_LOG("dmr fps lookup fail\n");
		return -1;
	}
	ODDMRFLOW_LOG("now_bl:now_vrefresh %d:%d\n", g_oddmr_current_timing.bl_level,g_oddmr_current_timing.vrefresh);
	ODDMRFLOW_LOG("dbv_table_idx:dbv_node %d:%d\n", dbv_table_idx,dbv_node);
	ODDMRFLOW_LOG("fps_table_idx:fps_node %d:%d\n", fps_table_idx,fps_node);

	atomic_set(&g_oddmr_priv->dmr_data.cur_dbv_node, dbv_node);
	atomic_set(&g_oddmr_priv->dmr_data.cur_dbv_table_idx, dbv_table_idx);
	atomic_set(&g_oddmr_priv->dmr_data.cur_fps_node, fps_node);
	atomic_set(&g_oddmr_priv->dmr_data.cur_fps_table_idx, fps_table_idx);

	mtk_drm_idlemgr_kick(__func__,
			&default_comp->mtk_crtc->base, 1);

	ret = mtk_oddmr_acquire_clock();
	if (ret == 0) {
		mtk_crtc = default_comp->mtk_crtc;

		mtk_oddmr_dmr_common_init_dual(NULL);
		mtk_oddmr_set_spr2rgb_dual(NULL);
		mtk_oddmr_dmr_smi_dual(NULL);

		mtk_oddmr_dmr_static_cfg(default_comp, NULL, &dmr_cfg_data->static_cfg);

		mtk_oddmr_dmr_gain_cfg(default_comp,
			NULL, dbv_node, fps_node, dmr_cfg_data);

		//set dmr table
		addr = g_oddmr_priv->dmr_data.mura_table[dbv_table_idx][fps_table_idx]->dma_addr;
		mtk_oddmr_write(default_comp, addr >> 4, DISP_ODDMR_DMR_UDMA_CTR_4, NULL);
		mtk_oddmr_write(default_comp, addr >> 20, DISP_ODDMR_DMR_UDMA_CTR_5, NULL);

		g_oddmr_priv->dmr_state = ODDMR_INIT_DONE;
		mtk_oddmr_release_clock();
	}


	return ret;
}


uint32_t mtk_oddmr_dbi_alpha_blend_int(uint32_t list_num, uint32_t *list_node,
	uint32_t *list_value, uint32_t target, uint32_t frac_bit)
{
	unsigned long num;
	int reduce_ind;
	unsigned long alpha;

	if (list_node == NULL || list_value == NULL || list_num == 0)
		return 0;

	if (target <= list_node[0])
		return (list_value[0] << frac_bit);

	if (target >= list_node[list_num - 1])
		return (list_value[list_num - 1] << frac_bit);

	for (reduce_ind = 1; reduce_ind < list_num; reduce_ind++) {
		if (target == list_node[reduce_ind])
			return (list_value[reduce_ind] << frac_bit);
		if (target < list_node[reduce_ind]) {
			num = list_node[reduce_ind] - list_node[reduce_ind - 1];
			alpha = target - list_node[reduce_ind - 1];
			if (num == 0)
				return (list_value[reduce_ind - 1] << frac_bit);
			else
				return (((unsigned long)(list_value[reduce_ind - 1] * (num - alpha))
					+ (unsigned long)(list_value[reduce_ind] * (alpha))) << frac_bit) / num;
		}
	}

	return 0;
}

void mtk_oddmr_scp_status(bool enable)
{
	if(enable)
		atomic_set(&g_oddmr_priv->dbi_data.enter_scp, 1);
}
EXPORT_SYMBOL(mtk_oddmr_scp_status);

static void mtk_oddmr_dbi_change_remap_gain(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, uint32_t cur_max_time)
{
	uint32_t frac_bit = 0;
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data = &g_oddmr_priv->dbi_cfg_info;
	uint32_t cur_dbv;
	uint32_t cur_offset;
	uint32_t cur_dbv_gain;
	uint32_t remap_gain_target_code;
	unsigned long flags;
	uint32_t remap_gain;

	if (g_oddmr_priv->dbi_state < ODDMR_INIT_DONE) {
		ODDMRLOW_LOG("dbi off remap_gain_target_code %u\n", dbi_cfg_data->fps_dbv_node.remap_gain_target_code);
		remap_gain_target_code = (dbi_cfg_data->fps_dbv_node.remap_gain_target_code<<16);
		remap_gain = MIN(((remap_gain_target_code  / 255) >> 4), 4096);
		mtk_oddmr_write_mask(comp, remap_gain,
			DISP_ODDMR_REG_SPR_REMAP_GAIN, 0xffffffff, pkg);
	} else {
		ODDMRLOW_LOG("cur_max_time %u\n", cur_max_time);
		ODDMRLOW_LOG("remap_gain_target_code %u\n", dbi_cfg_data->fps_dbv_node.remap_gain_target_code);

		spin_lock_irqsave(&g_oddmr_timing_lock, flags);
		cur_dbv = g_oddmr_current_timing.bl_level;
		spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);

		cur_offset = mtk_oddmr_dbi_alpha_blend_int(dbi_cfg_data->fps_dbv_node.remap_reduce_offset_num,
			dbi_cfg_data->fps_dbv_node.remap_reduce_offset_node,
			dbi_cfg_data->fps_dbv_node.remap_reduce_offset_value, cur_max_time, frac_bit);
		cur_dbv_gain = mtk_oddmr_dbi_alpha_blend_int(dbi_cfg_data->fps_dbv_node.remap_dbv_gain_num,
			dbi_cfg_data->fps_dbv_node.remap_dbv_gain_node,
			dbi_cfg_data->fps_dbv_node.remap_dbv_gain_value, cur_dbv, frac_bit);

		remap_gain_target_code = (dbi_cfg_data->fps_dbv_node.remap_gain_target_code<<16);
		remap_gain = MIN((((remap_gain_target_code - (cur_offset * cur_dbv_gain)) / 255) >> 4), 4096);

		ODDMRLOW_LOG("remap gain:0x%x, remap offset:0x%x, remap DBV gain:0x%x\n",
			remap_gain, cur_offset, cur_dbv_gain);

		mtk_oddmr_write_mask(comp, remap_gain,
			DISP_ODDMR_REG_SPR_REMAP_GAIN, 0xffffffff, pkg);
	}
}

static int mtk_oddmr_dbi_enable(struct drm_device *dev, bool en)
{
	int ret = 0, enable = en;

	ODDMRAPI_LOG("%d\n", enable);
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}
	if (g_oddmr_priv->dbi_state < ODDMR_INIT_DONE) {
		ODDMRFLOW_LOG("can not enable, state %d\n", g_oddmr_priv->dbi_state);
		return -EFAULT;
	}
	mtk_drm_idlemgr_kick(__func__,
			&default_comp->mtk_crtc->base, 1);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0)
		mtk_oddmr_release_clock();
	else {
		ODDMRFLOW_LOG("clock not on %d\n", ret);
		return ret;
	}

	g_oddmr_priv->dbi_enable_req = enable;
	if (default_comp->mtk_crtc->is_dual_pipe)
		g_oddmr1_priv->dbi_enable_req = enable;
	atomic_set(&g_oddmr_dbi_hrt_done, 2);
	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, default_comp->mtk_crtc->base.dev);
	ret = wait_event_interruptible_timeout(g_oddmr_hrt_wq,
			atomic_read(&g_oddmr_dbi_hrt_done) == 1, msecs_to_jiffies(200));
	if (ret <= 0) {
		atomic_set(&g_oddmr_dbi_hrt_done, 0);
		ODDMRFLOW_LOG("enable %d repaint timeout %d\n", enable, ret);
		ret = -EAGAIN;
	}
	return ret;
}

static int mtk_oddmr_remap_update(void)
{
	int ret = 0;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}

	ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base, default_comp,
					ODDMR_CMD_ODDMR_REMAP_CHG, NULL);
	return ret;
}


static int mtk_oddmr_dmr_enable(struct drm_device *dev, bool en)
{
	int ret = 0, enable = en;

	ODDMRAPI_LOG("%d\n", enable);
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("od comp is NULL\n");
		return -1;
	}
	if (g_oddmr_priv->dmr_state < ODDMR_INIT_DONE) {
		ODDMRFLOW_LOG("can not enable, state %d\n", g_oddmr_priv->dmr_state);
		return -EFAULT;
	}
	mtk_drm_idlemgr_kick(__func__,
			&default_comp->mtk_crtc->base, 1);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0)
		mtk_oddmr_release_clock();
	else {
		ODDMRFLOW_LOG("clock not on %d\n", ret);
		return ret;
	}

	g_oddmr_priv->dmr_enable_req = enable;
	if (default_comp->mtk_crtc->is_dual_pipe)
		g_oddmr1_priv->dmr_enable_req = enable;
	atomic_set(&g_oddmr_dmr_hrt_done, 2);
	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, default_comp->mtk_crtc->base.dev);
	ret = wait_event_interruptible_timeout(g_oddmr_hrt_wq,
			atomic_read(&g_oddmr_dmr_hrt_done) == 1, msecs_to_jiffies(200));
	if (ret <= 0) {
		atomic_set(&g_oddmr_dmr_hrt_done, 0);
		ODDMRFLOW_LOG("enable %d repaint timeout %d\n", enable, ret);
		ret = -EAGAIN;
	}
	return ret;
}

static int mtk_oddmr_tuning_od_set_sram_data(uint32_t map_id,
	struct mtk_oddmr_od_tuning_sram *tuning_data)
{
	/* B:0-bit1, G:1-bit2, R:2-bit3*/
	if (map_id >= OD_TABLE_B_SRAM1_START && map_id <= OD_TABLE_B_SRAM1_END) {
		tuning_data->channel = 0;
		tuning_data->sram = 1;
		tuning_data->idx = map_id - OD_TABLE_B_SRAM1_START;
	} else if (map_id >= OD_TABLE_B_SRAM2_START && map_id <= OD_TABLE_B_SRAM2_END) {
		tuning_data->channel = 0;
		tuning_data->sram = 2;
		tuning_data->idx = map_id - OD_TABLE_B_SRAM2_START;
	} else if (map_id >= OD_TABLE_B_SRAM3_START && map_id <= OD_TABLE_B_SRAM3_END) {
		tuning_data->channel = 0;
		tuning_data->sram = 3;
		tuning_data->idx = map_id - OD_TABLE_B_SRAM3_START;
	} else if (map_id >= OD_TABLE_B_SRAM4_START && map_id <= OD_TABLE_B_SRAM4_END) {
		tuning_data->channel = 0;
		tuning_data->sram = 4;
		tuning_data->idx = map_id - OD_TABLE_B_SRAM4_START;
	} else if (map_id >= OD_TABLE_G_SRAM1_START && map_id <= OD_TABLE_G_SRAM1_END) {
		tuning_data->channel = 1;
		tuning_data->sram = 1;
		tuning_data->idx = map_id - OD_TABLE_G_SRAM1_START;
	} else if (map_id >= OD_TABLE_G_SRAM2_START && map_id <= OD_TABLE_G_SRAM2_END) {
		tuning_data->channel = 1;
		tuning_data->sram = 2;
		tuning_data->idx = map_id - OD_TABLE_G_SRAM2_START;
	} else if (map_id >= OD_TABLE_G_SRAM3_START && map_id <= OD_TABLE_G_SRAM3_END) {
		tuning_data->channel = 1;
		tuning_data->sram = 3;
		tuning_data->idx = map_id - OD_TABLE_G_SRAM3_START;
	} else if (map_id >= OD_TABLE_G_SRAM4_START && map_id <= OD_TABLE_G_SRAM4_END) {
		tuning_data->channel = 1;
		tuning_data->sram = 4;
		tuning_data->idx = map_id - OD_TABLE_G_SRAM4_START;
	} else if (map_id >= OD_TABLE_R_SRAM1_START && map_id <= OD_TABLE_R_SRAM1_END) {
		tuning_data->channel = 2;
		tuning_data->sram = 1;
		tuning_data->idx = map_id - OD_TABLE_R_SRAM1_START;
	} else if (map_id >= OD_TABLE_R_SRAM2_START && map_id <= OD_TABLE_R_SRAM2_END) {
		tuning_data->channel = 2;
		tuning_data->sram = 2;
		tuning_data->idx = map_id - OD_TABLE_R_SRAM2_START;
	} else if (map_id >= OD_TABLE_R_SRAM3_START && map_id <= OD_TABLE_R_SRAM3_END) {
		tuning_data->channel = 2;
		tuning_data->sram = 3;
		tuning_data->idx = map_id - OD_TABLE_R_SRAM3_START;
	} else if (map_id >= OD_TABLE_R_SRAM4_START && map_id <= OD_TABLE_R_SRAM4_END) {
		tuning_data->channel = 2;
		tuning_data->sram = 4;
		tuning_data->idx = map_id - OD_TABLE_R_SRAM4_START;
	} else {
		ODDMRFLOW_LOG("map_id 0x%x outof bound\n", map_id);
		return -EFAULT;
	}
	return 0;
}

static int mtk_oddmr_od_sram_read(struct mtk_ddp_comp *comp, int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	struct mtk_oddmr_od_table *table;
	uint8_t *raw_table;
	uint32_t table_size, map_id, *val;
	int idx;
	struct mtk_oddmr_od_tuning_sram tuning_data = {0};

	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	table = pparam->od_tables[table_idx];
	raw_table = table->raw_table.value;
	table_size = table->raw_table.size;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = &sw_reg->val;
	idx = map_id - OD_TABLE_B_SRAM1_START;
	if (idx >= table_size) {
		ODDMRFLOW_LOG("table%d idx %d outof size %d\n", table_idx, idx, table_size);
		return -EFAULT;
	}
	mtk_oddmr_tuning_od_set_sram_data(map_id, &tuning_data);
	mtk_oddmr_od_tuning_read_sram(comp, &tuning_data);
	*val = tuning_data.value;
	ODDMRAPI_LOG("map_id 0x%x table_idx %d, idx %d val %u\n", map_id, table_idx, idx, *val);
	return 0;
}

static int mtk_oddmr_od_sram_write(struct mtk_ddp_comp *comp, int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	struct mtk_oddmr_od_table *table;
	uint8_t *raw_table;
	uint32_t table_size, map_id, val;
	int idx, ret = 0;
	struct mtk_oddmr_od_tuning_sram tuning_data = {0};

	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	table = pparam->od_tables[table_idx];
	raw_table = table->raw_table.value;
	table_size = table->raw_table.size;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = sw_reg->val;
	idx = map_id - OD_TABLE_B_SRAM1_START;
	if (idx >= table_size) {
		ODDMRFLOW_LOG("table%d idx %d outof size %d\n", table_idx, idx, table_size);
		return -EFAULT;
	}
	//raw_table[idx] = val;
	mtk_oddmr_tuning_od_set_sram_data(map_id, &tuning_data);
	tuning_data.value = val;

	ret = mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp,
		ODDMR_CMD_OD_TUNING_WRITE_SRAM, &tuning_data);
	if (ret == 1)
		ret = -EFAULT;
	ODDMRAPI_LOG("ret %d map_id:0x%x(%u,%u,%u,%u)\n", ret, map_id,
		tuning_data.channel, tuning_data.sram, tuning_data.idx, tuning_data.value);
	return ret;
}

static int mtk_oddmr_od_status_read(struct mtk_ddp_comp *comp, int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_timing *timing)
{
	uint32_t map_id, *val;
	int ret = 0;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = &sw_reg->val;
	switch (map_id) {
	case OD_CURRENT_TABLE:
		*val = table_idx;
		break;
	case OD_CURRENT_BL:
		*val = timing->bl_level;
		break;
	case OD_CURRENT_FPS:
		*val = timing->vrefresh;
		break;
	case OD_CURRENT_HDISPLAY:
		*val = timing->hdisplay;
		break;
	case OD_CURRENT_VDISPLAY:
		*val = timing->vdisplay;
		break;
	default:
		ret = -EFAULT;
		break;
	}
	ODDMRAPI_LOG("map_id 0x%x, %d\n", map_id, *val);
	return ret;
}

static int mtk_oddmr_od_read_sw_reg(struct mtk_ddp_comp *comp, struct mtk_drm_oddmr_ctl *ctl_data,
			struct mtk_oddmr_od_param *pparam)
{
	int ret, table_idx, tmp_sram_idx;
	struct mtk_oddmr_sw_reg sw_reg;
	uint32_t map_id;
	struct mtk_disp_oddmr *priv;

	if (ctl_data == NULL ||
		ctl_data->data == NULL ||
		ctl_data->size != sizeof(struct mtk_oddmr_sw_reg)) {
		ODDMRFLOW_LOG("ctl_data is invalid\n");
		return -EFAULT;
	}
	if (comp == NULL || pparam == NULL) {
		ODDMRFLOW_LOG("comp,pparam is NULL\n");
		return -EFAULT;
	}
	if (copy_from_user(&sw_reg, ctl_data->data, ctl_data->size)) {
		ODDMRFLOW_LOG("copy_from_user fail\n");
		ret = -EFAULT;
		goto out;
	}
	priv = comp_to_oddmr(comp);
	tmp_sram_idx = priv->od_data.od_sram_read_sel;
	table_idx = priv->od_data.od_dram_sel[priv->od_data.od_sram_table_idx[!!tmp_sram_idx]];
	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	map_id = (sw_reg.reg & 0xFFFF) / 4;
	if (map_id >= OD_TABLE_B_SRAM1_START && map_id <= OD_TABLE_R_SRAM4_END)
		ret = mtk_oddmr_od_sram_read(comp, table_idx, &sw_reg, pparam);
	else if (map_id <= OD_CURRENT_VDISPLAY)
		ret = mtk_oddmr_od_status_read(comp, table_idx, &sw_reg, &g_oddmr_current_timing);
	else
		ret = mtk_oddmr_od_tuning_read(comp, table_idx, &sw_reg, pparam);
	if (copy_to_user(ctl_data->data, &sw_reg, ctl_data->size)) {
		ODDMRFLOW_LOG("copy_to_user fail\n");
		ret = -EFAULT;
		goto out;
	}
	ODDMRFLOW_LOG("reg 0x%x 0x%x 0x%x+\n", sw_reg.reg, sw_reg.val, ctl_data->size);
out:
	return ret;
}

static int mtk_oddmr_od_write_sw_reg(struct mtk_ddp_comp *comp, struct mtk_drm_oddmr_ctl *ctl_data,
			struct mtk_oddmr_od_param *pparam)
{
	int ret, table_idx, tmp_sram_idx;
	struct mtk_oddmr_sw_reg sw_reg;
	uint32_t map_id;
	struct mtk_disp_oddmr *priv;

	if (ctl_data == NULL ||
		ctl_data->data == NULL ||
		ctl_data->size != sizeof(struct mtk_oddmr_sw_reg)) {
		ODDMRFLOW_LOG("ctl_data is invalid\n");
		return -EFAULT;
	}
	if (comp == NULL || pparam == NULL) {
		ODDMRFLOW_LOG("comp,pparam is NULL\n");
		return -EFAULT;
	}
	if (copy_from_user(&sw_reg, ctl_data->data, ctl_data->size)) {
		ODDMRFLOW_LOG("copy_from_user fail\n");
		ret = -EFAULT;
		goto out;
	}
	ODDMRFLOW_LOG("reg 0x%x val 0x%x+\n", sw_reg.reg, sw_reg.val);
	priv = comp_to_oddmr(comp);
	tmp_sram_idx = priv->od_data.od_sram_read_sel;
	table_idx = priv->od_data.od_dram_sel[priv->od_data.od_sram_table_idx[!!tmp_sram_idx]];
	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	map_id = (sw_reg.reg & 0xFFFF) / 4;
	if (map_id >= OD_TABLE_B_SRAM1_START && map_id <= OD_TABLE_R_SRAM4_END)
		ret = mtk_oddmr_od_sram_write(comp, table_idx, &sw_reg, pparam);
	else
		ret = mtk_oddmr_od_tuning_write(comp, table_idx, &sw_reg, pparam);
out:
	return ret;
}

/* all oddmr ioctl use handle dualpipe itself because it is not int drm atomic flow */
int mtk_drm_ioctl_oddmr_ctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret;
	int pm_ret;
	struct mtk_drm_oddmr_ctl *param = data;

	ODDMRAPI_LOG("+\n");
	if (param == NULL) {
		ODDMRFLOW_LOG("param is null\n");
		return -EFAULT;
	}
	pm_ret = mtk_vidle_pq_power_get(__func__);
	ODDMRAPI_LOG("%s: cmd is %u\n", __func__, param->cmd);
	switch (param->cmd) {
	case MTK_DRM_ODDMR_OD_INIT:
		ret = mtk_oddmr_od_init();
		break;
	case MTK_DRM_ODDMR_OD_ENABLE:
		ret = mtk_oddmr_od_enable(dev, 1);
		break;
	case MTK_DRM_ODDMR_OD_DISABLE:
		ret = mtk_oddmr_od_enable(dev, 0);
		break;
	case MTK_DRM_ODDMR_DMR_INIT:
		ret = 0;
		break;
	case MTK_DRM_ODDMR_DMR_ENABLE:
		ret = mtk_oddmr_dmr_enable(dev, 1);
		break;
	case MTK_DRM_ODDMR_DMR_DISABLE:
		ret = mtk_oddmr_dmr_enable(dev, 0);
		break;
	case MTK_DRM_ODDMR_OD_READ_SW_REG:
		ret = mtk_oddmr_od_read_sw_reg(default_comp, param, &g_od_param);
		break;
	case MTK_DRM_ODDMR_OD_WRITE_SW_REG:
		ret = mtk_oddmr_od_write_sw_reg(default_comp, param, &g_od_param);
		break;
	case MTK_DRM_ODDMR_OD_USER_GAIN:
		ret = mtk_oddmr_od_user_gain(default_comp, param);
		break;
	default:
		ODDMRFLOW_LOG("cmd %d is invalid\n", param->cmd);
		ret = -EINVAL;
		break;
	}
	if (!pm_ret)
		mtk_vidle_pq_power_put(__func__);
	ODDMRFLOW_LOG("cmd %d ret %d\n", param->cmd, ret);
	return ret;
}

int mtk_drm_ioctl_oddmr_load_param(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret;
	struct mtk_drm_oddmr_param *param = data;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}
	if (param == NULL) {
		ODDMRFLOW_LOG("param is NULL\n");
		return -EFAULT;
	}
	ret = 0;
	switch (param->head_id >> 24) {
	case ODDMR_DMR_BASIC_INFO:
	case ODDMR_DMR_TABLE:
		if (!is_oddmr_dmr_support) {
			ODDMRFLOW_LOG("dmr not support\n");
			ret = -EFAULT;
		}
		break;
	case ODDMR_OD_BASIC_INFO:
	case ODDMR_OD_TABLE:
		if (!is_oddmr_od_support) {
			ODDMRFLOW_LOG("od not support\n");
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EFAULT;
		break;
	}
	if (ret < 0)
		return ret;
	/*
	 * If any section is loaded, set all to loading,
	 * set loading done in init to support partial loading.
	 */
	ret = mtk_oddmr_load_param(g_oddmr_priv, data);
	return ret;
}

static void mtk_oddmr_odr_get_status(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	const u16 reg_jump = CMDQ_THR_SPR_IDX1;
	const u16 var1 = CMDQ_THR_SPR_IDX2;
	struct cmdq_operand lop, rop;
	u32 inst_condi_jump;
	u64 *inst, jump_pa;

	/* 1. get od status */
	lop.reg = true;
	lop.idx = CMDQ_THR_SPR_IDX2;
	rop.reg = false;
	rop.value = 1;

	cmdq_pkt_read(handle, NULL,
				comp->regs_pa + DISP_ODDMR_OD_CTRL_EN, var1);
	cmdq_pkt_logic_command(handle, CMDQ_LOGIC_AND, var1, &lop, &rop);
	/* 2. read odr cnts if od is enabled */
	lop.reg = true;
	lop.idx = CMDQ_THR_SPR_IDX2;
	rop.reg = false;
	rop.value = 1;

	/*mark condition jump */
	inst_condi_jump = handle->cmd_buf_size;
	cmdq_pkt_assign_command(handle, reg_jump, 0);

	cmdq_pkt_cond_jump_abs(handle, reg_jump, &lop, &rop,
		CMDQ_NOT_EQUAL);

	/* if condition false, here is nop jump, od enable */
	{
		cmdq_pkt_mem_move(handle, NULL, comp->regs_pa + DISP_ODDMR_UDMA_R_CTRL84_0,
			comp->regs_pa + DISP_ODDMR_ODR_H,
			CMDQ_THR_SPR_IDX3);
		cmdq_pkt_mem_move(handle, NULL, comp->regs_pa + DISP_ODDMR_UDMA_R_CTRL86_0,
			comp->regs_pa + DISP_ODDMR_ODR_V,
			CMDQ_THR_SPR_IDX3);
	}

	/* end condition */
	inst = cmdq_pkt_get_va_by_offset(handle, inst_condi_jump);
	jump_pa = cmdq_pkt_get_pa_by_offset(handle,
				handle->cmd_buf_size);
	*inst = *inst | CMDQ_REG_SHIFT_ADDR(jump_pa);
}

static void mtk_oddmr_config_trigger(struct mtk_ddp_comp *comp,
				   struct cmdq_pkt *handle,
				   enum mtk_ddp_comp_trigger_flag flag)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	if (!mtk_crtc) {
		DDPPR_ERR("%s oddmr comp not configure CRTC yet\n", __func__);
		return;
	}
	if (!mtk_crtc->base.dev)
		return;
	priv = mtk_crtc->base.dev->dev_private;
	ODDMRFLOW_LOG("%d\n", flag);
	switch (flag) {
	case MTK_TRIG_FLAG_EOF:
	{
		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_ODDMR_OD_AEE))
			break;
		mtk_oddmr_odr_get_status(comp, handle);
	}
		break;
	default:
		break;
	}
}

static void mtk_oddmr_remap_set_enable(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, bool en)
{
	int enable = en;

	ODDMRAPI_LOG("%d\n", enable);
	if (en) {
		mtk_oddmr_write_mask(comp, 1,
			DISP_ODDMR_REG_SPR_REMAP_EN, 0xffffffff, pkg);
		mtk_oddmr_write_mask(comp, 0,
			DISP_ODDMR_TOP_S2R_BYPASS, 0xffffffff, pkg);
	} else
		mtk_oddmr_write_mask(comp, 0,
			DISP_ODDMR_REG_SPR_REMAP_EN, 0xffffffff, pkg);
}

static int mtk_oddmr_remap_enable(struct drm_device *dev, bool en)
{
	int ret = 0, enable = en;

	ODDMRAPI_LOG("%d\n", enable);
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}

	if (en)
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base, default_comp,
			ODDMR_CMD_ODDMR_REMAP_EN, NULL);
	else
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base, default_comp,
			ODDMR_CMD_ODDMR_REMAP_OFF, NULL);

	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, default_comp->mtk_crtc->base.dev);

	return ret;
}

static int mtk_oddmr_pq_ioctl_transact(struct mtk_ddp_comp *comp,
	unsigned int cmd, void *params, unsigned int size)
{
	int ret = -1;
	struct bitstream_buffer *stream_buffer;
	void *ptr;
	unsigned int *cur_max_time;
	unsigned int *target_code;
	unsigned int *gain_ratio;
	struct mtk_drm_private *priv = NULL;
	unsigned long flags;
	unsigned int temp;

	switch (cmd) {
	case PQ_DMR_INIT:
		ret = mtk_oddmr_dmr_init(params);
		DDPMSG("%s, PQ_DMR_INIT\n", __func__);
		break;
	case PQ_DMR_ENABLE:
		ret = 0;
		mtk_oddmr_dmr_enable(NULL, true);
		DDPMSG("%s, PQ_DMR_ENABLE\n", __func__);
		break;
	case PQ_DMR_DISABLE:
		ret = 0;
		mtk_oddmr_dmr_enable(NULL, false);
		DDPMSG("%s, PQ_DMR_DISABLE\n", __func__);
		break;
	case PQ_DBI_LOAD_PARAM:
		ret = mtk_oddmr_dbi_init(params);
		DDPMSG("%s, PQ_DBI_LOAD_PARAM\n", __func__);
		break;
	case PQ_DBI_LOAD_TB:
	{
		ret = 0;
		if (g_oddmr_priv->dbi_state < ODDMR_INIT_DONE) {
			ODDMRFLOW_LOG("can not load table, state %d\n", g_oddmr_priv->dbi_state);
			return -EFAULT;
		}
		stream_buffer = params;
		size = stream_buffer->size;
		ptr = vmalloc(size);
		if (!ptr) {
			DDPMSG("%s:%d, param buffer alloc fail\n",
				__func__, __LINE__);
			return -1;
		}
		if (copy_from_user(ptr,
			stream_buffer->_buffer, size)) {
			DDPMSG("%s:%d, copy_from_user fail\n", __func__, __LINE__);
			if (ptr)
				vfree(ptr);
			return -1;
		}
		stream_buffer->_buffer = ptr;

		kernel_dbi_table_update(stream_buffer);
		if (ptr)
			vfree(ptr);
		DDPMSG("%s, PQ_DBI_LOAD_TB\n", __func__);
	}
		break;
	case PQ_DBI_ENABLE:
		ret = 0;
		mtk_oddmr_dbi_enable(NULL, true);
		DDPMSG("%s, PQ_DBI_ENABLE\n", __func__);
		break;
	case PQ_DBI_DISABLE:
		ret = 0;
		mtk_oddmr_dbi_enable(NULL, false);
		DDPMSG("%s, PQ_DBI_ENABLE\n", __func__);
		break;
	case PQ_DBI_REMAP_TARGET:
		ret = 0;
		mutex_lock(&g_dbi_data_lock);
		target_code = params;
		g_oddmr_priv->dbi_cfg_info.fps_dbv_node.remap_gain_target_code = *target_code;
		mutex_unlock(&g_dbi_data_lock);
		mtk_oddmr_remap_update();
		DDPMSG("%s, PQ_DBI_REMAP_TARGET remap_gain_target_code:%d\n",
			__func__, g_oddmr_priv->dbi_cfg_info.fps_dbv_node.remap_gain_target_code);
		break;
	case PQ_DBI_REMAP_CHG:
		ret = 0;
		mutex_lock(&g_dbi_data_lock);
		cur_max_time = params;
		g_oddmr_priv->dbi_data.cur_max_time = *cur_max_time;
		atomic_set(&g_oddmr_priv->dbi_data.max_time_set_done, 1);
		mutex_unlock(&g_dbi_data_lock);
		DDPMSG("%s, PQ_DBI_REMAP cur_max_time:%d\n", __func__, g_oddmr_priv->dbi_data.cur_max_time);
		break;
	case PQ_DBI_REMAP_ENABLE:
		ret = 0;
		atomic_set(&g_oddmr_priv->dbi_data.remap_enable, 1);
		mtk_oddmr_remap_enable(NULL, true);
		break;
	case PQ_DBI_REMAP_DISABLE:
		ret = 0;
		atomic_set(&g_oddmr_priv->dbi_data.remap_enable, 0);
		mtk_oddmr_remap_enable(NULL, false);
		break;
	case PQ_DBI_GET_HW_ID:
		ret = 0;
		DDP_MUTEX_LOCK(&default_comp->mtk_crtc->lock, __func__, __LINE__);
		priv = default_comp->mtk_crtc->base.dev->dev_private;
		*(unsigned int *)params = priv->data->mmsys_id;
		DDP_MUTEX_UNLOCK(&default_comp->mtk_crtc->lock, __func__, __LINE__);
		break;
	case PQ_DBI_GET_WIDTH:
		ret = 0;
		DDP_MUTEX_LOCK(&default_comp->mtk_crtc->lock, __func__, __LINE__);
		*(unsigned int *)params = mtk_crtc_get_width_by_comp(__func__,
			&default_comp->mtk_crtc->base, default_comp, false);
		DDP_MUTEX_UNLOCK(&default_comp->mtk_crtc->lock, __func__, __LINE__);
		break;
	case PQ_DBI_GET_HEIGHT:
		ret = 0;
		DDP_MUTEX_LOCK(&default_comp->mtk_crtc->lock, __func__, __LINE__);
		*(unsigned int *)params = mtk_crtc_get_height_by_comp(__func__,
			&default_comp->mtk_crtc->base, default_comp, false);
		DDP_MUTEX_UNLOCK(&default_comp->mtk_crtc->lock, __func__, __LINE__);
		break;
	case PQ_DBI_GET_DBV:
		ret = 0;
		spin_lock_irqsave(&g_oddmr_timing_lock, flags);
		*(unsigned int *)params = g_oddmr_current_timing.bl_level;
		spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);
		break;
	case PQ_DBI_GET_FPS:
		ret = 0;
		spin_lock_irqsave(&g_oddmr_timing_lock, flags);
		*(unsigned int *)params = g_oddmr_current_timing.vrefresh;
		spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);
		break;
	case PQ_DBI_GET_SCP:
		ret = 0;
		temp = atomic_read(&g_oddmr_priv->dbi_data.enter_scp);
		*(unsigned int *)params = temp;
		if(temp)
			atomic_set(&g_oddmr_priv->dbi_data.enter_scp, 0);
		break;
	case PQ_DBI_SET_GAIN_RATIO:
		ret = 0;
		gain_ratio = params;
		atomic_set(&g_oddmr_priv->dbi_data.gain_ratio, *gain_ratio);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_oddmr_set_partial_update(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct mtk_rect partial_roi, bool enable)
{
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;
	struct mtk_disp_oddmr *oddmr = comp_to_oddmr(comp);
	struct mtk_drm_dbi_cfg_info *dbi_cfg_data = &g_oddmr_priv->dbi_cfg_info;
	unsigned int scale_factor_v = dbi_cfg_data->basic_info.partial_update_scale_factor_v;

	//unsigned int is_roi_en = 0;
	//unsigned int roi_v_start = 0;
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
				&comp->mtk_crtc->base, comp, true);
	unsigned int crop_voffset = 0;
	unsigned int crop_height;
	unsigned int overhead_v;
	unsigned int comp_overhead_v;

	unsigned int dbi_y_ini, dbi_udma_y_ini;
	unsigned int block_size;
	unsigned int y_idx2_ini, y_remain2_ini;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
		__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	/* oddmr crop offset set*/
	oddmr->set_partial_update = enable;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
			? 0 : oddmr->tile_overhead_v.overhead_v;
	comp_overhead_v = (!overhead_v) ? 0 : oddmr->tile_overhead_v.comp_overhead_v;
	oddmr->roi_height = partial_roi.height;
	crop_voffset = comp_overhead_v;
	crop_height = oddmr->roi_height + (overhead_v - comp_overhead_v) * 2;

	DDPDBG("%s, %s total overhead_v:%d, spr overhead_v:%d\n",
		__func__, mtk_dump_comp_str(comp), overhead_v, comp_overhead_v);

	/* update y ini */
	if (oddmr->set_partial_update)
		dbi_y_ini = partial_roi.y;
	else
		dbi_y_ini = full_height;

	/* update udma y ini */
	//if (is_roi_en == 1) {
	//	if (dbi_y_ini > roi_v_start)
	//		dbi_udma_y_ini = (dbi_y_ini - roi_v_start) / scale_factor_v;
	//	else
	//		dbi_udma_y_ini = 0;
	//} else {
	dbi_udma_y_ini = dbi_y_ini / scale_factor_v;
	//}

	/* update idx/remain y ini */
	block_size = (full_height + 3) / 4;
	if (block_size) {
		y_idx2_ini = dbi_y_ini / block_size;
		y_remain2_ini = dbi_y_ini - block_size * y_idx2_ini;
	} else {
		DDPMSG("%s block_size = 0", __func__);
		y_idx2_ini = 0;
		y_remain2_ini = 0;
	}

	/* record dbi partial data */
	dbi_part_data.dbi_y_ini = dbi_y_ini;
	dbi_part_data.dbi_udma_y_ini = dbi_udma_y_ini;
	dbi_part_data.y_idx2_ini = y_idx2_ini;
	dbi_part_data.y_remain2_ini = y_remain2_ini;

	/* oddmr reg config */
	if (oddmr->set_partial_update) {
		if (priv->data->mmsys_id == MMSYS_MT6989) {
			/* ODDMR on MT6989 not support V crop */
//			mtk_oddmr_write(comp, oddmr->set_partial_update,
//					DISP_ODDMR_TOP_CRP_BYPASS, handle);
			mtk_oddmr_write(comp, dbi_y_ini,
					DISP_ODDMR_REG_DMR_Y_INI, handle);
			mtk_oddmr_write(comp, (oddmr->roi_height + overhead_v * 2),
					DISP_ODDMR_FMT_CTR_1, handle); // oddmr input height
//			mtk_oddmr_write(comp, crop_height,
//					DISP_ODDMR_CRP_CTR_1, handle); // oddmr output height

			mtk_oddmr_write(comp, dbi_udma_y_ini,
					DISP_ODDMR_DMR_UDMA_CTR_7, handle);

			mtk_oddmr_write(comp, y_idx2_ini,
					DISP_ODDMR_REG_Y_IDX2_INI, handle);
			mtk_oddmr_write(comp, y_remain2_ini,
					DISP_ODDMR_REG_Y_REMAIN2_INI, handle);
		}
	} else {
		if (priv->data->mmsys_id == MMSYS_MT6989) {
//			mtk_oddmr_write(comp, oddmr->set_partial_update,
//					DISP_ODDMR_TOP_CRP_BYPASS, handle);
			mtk_oddmr_write(comp, 0,
					DISP_ODDMR_REG_DMR_Y_INI, handle);
			mtk_oddmr_write(comp, full_height,
					DISP_ODDMR_FMT_CTR_1, handle); // oddmr input height
//			mtk_oddmr_write(comp, full_height,
//					DISP_ODDMR_CRP_CTR_1, handle); // oddmr output height

			mtk_oddmr_write(comp, 0,
					DISP_ODDMR_DMR_UDMA_CTR_7, handle);

			mtk_oddmr_write(comp, 0,
					DISP_ODDMR_REG_Y_IDX2_INI, handle);
			mtk_oddmr_write(comp, 0,
					DISP_ODDMR_REG_Y_REMAIN2_INI, handle);
		}
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_oddmr_funcs = {
	.config = mtk_oddmr_config,
	.start = mtk_oddmr_start,
	.stop = mtk_oddmr_stop,
	.prepare = mtk_oddmr_prepare,
	.unprepare = mtk_oddmr_unprepare,
	.io_cmd = mtk_oddmr_io_cmd,
	.user_cmd = mtk_oddmr_user_cmd,
	.first_cfg = mtk_oddmr_first_cfg,
	.config_trigger = mtk_oddmr_config_trigger,
	.config_overhead = mtk_disp_oddmr_config_overhead,
	.mutex_sof_irq = disp_oddmr_on_start_of_frame,
	.pq_ioctl_transact = mtk_oddmr_pq_ioctl_transact,
	.bypass = mtk_oddmr_bypass,
	.config_overhead_v = mtk_disp_oddmr_config_overhead_v,
	.partial_update = mtk_oddmr_set_partial_update,
};

static int mtk_disp_oddmr_bind(struct device *dev, struct device *master,
		void *data)
{
	struct mtk_disp_oddmr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	struct mtk_disp_oddmr *oddmr_priv;
	int ret;
	char buf[50];

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
				dev->of_node->full_name, ret);
		return ret;
	}
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		oddmr_priv = comp_to_oddmr(&priv->ddp_comp);
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "DMRR");
		oddmr_priv->qos_req_dmrr = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "DBIR");
		oddmr_priv->qos_req_dbir = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODR");
		oddmr_priv->qos_req_odr = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODW");
		oddmr_priv->qos_req_odw = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "DMRR_HRT");
		oddmr_priv->qos_req_dmrr_hrt = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "DBIR_HRT");
		oddmr_priv->qos_req_dbir_hrt = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODR_HRT");
		oddmr_priv->qos_req_odr_hrt = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODW_HRT");
		oddmr_priv->qos_req_odw_hrt = of_mtk_icc_get(dev, buf);

	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void mtk_disp_oddmr_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct mtk_disp_oddmr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_oddmr_component_ops = {
	.bind = mtk_disp_oddmr_bind,
	.unbind = mtk_disp_oddmr_unbind,
};

unsigned int check_oddmr_err_event(void)
{
	return !!(oddmr_err_trigger & oddmr_err_trigger_mask);
}

void clear_oddmr_err_event(void)
{
	DDPMSG("%s, do clear underrun event\n", __func__);
	oddmr_err_trigger = 0;
}

#ifdef ODDMR_ENABLE_IRQ
/*
 * Check frame done status at sof.
 * It is a err detection with delay,
 * should be replaced when better ways come along.
 */
static irqreturn_t mtk_oddmr_check_framedone(int irq, void *dev_id)
{
	struct mtk_disp_oddmr *oddmr_priv = dev_id;
	struct mtk_ddp_comp *comp;
	unsigned int ret = 0;
	uint32_t status_raw, status, od_enable, oddmr_err = 0;
	bool dump_en_tmp;
	struct mtk_drm_private *priv = NULL;
	int od_opt = 0;

	if (IS_ERR_OR_NULL(oddmr_priv))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get("oddmr_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	comp = &oddmr_priv->ddp_comp;
	status_raw = mtk_oddmr_read(comp, DISP_ODDMR_IRQ_RAW_STATUS);
	status = mtk_oddmr_read(comp, DISP_ODDMR_IRQ_STATUS);
	mtk_oddmr_write(comp, status, DISP_ODDMR_IRQ_CLEAR, NULL);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_IRQ_CLEAR, NULL);
	priv = comp->mtk_crtc->base.dev->dev_private;
	od_opt = mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_ODDMR_OD_AEE);
	CRTC_MMP_MARK(0, oddmr_ctl, status_raw, status);
	DDPIRQ("%s %s irq, val:0x%x,0x%x\n", __func__, mtk_dump_comp_str(comp),
		status_raw, status);
	if (status &
		(DISP_ODDMR_IRQ_FRAME_DONE |
		DISP_ODDMR_IRQ_ODW_DONE |
		DISP_ODDMR_IRQ_I_FRM_DNE |
		DISP_ODDMR_IRQ_O_FRM_DNE)) {
		//only care eof follows sof
		if (oddmr_priv->irq_status & DISP_ODDMR_IRQ_SOF)
			oddmr_priv->irq_status |= status;
		if (status & DISP_ODDMR_IRQ_O_FRM_DNE) {
			uint32_t odr_h, odr_v, odr_h_exp, odr_v_exp;
			uint32_t scaling_mode, hscaling, vscaling, h_2x4x_sel;
			uint32_t comp_width, merge_width, merge_lines;

			odr_h = mtk_oddmr_read(comp, DISP_ODDMR_ODR_H);
			odr_v = mtk_oddmr_read(comp, DISP_ODDMR_ODR_V);
			scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;
			vscaling = (scaling_mode & BIT(1)) > 0 ? 2 : 1;
			hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
			h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
			hscaling = hscaling * h_2x4x_sel;
			comp_width = oddmr_priv->cfg.comp_in_width;
			merge_lines = oddmr_priv->od_data.merge_lines;
			merge_width = DIV_ROUND_UP(comp_width * merge_lines, 8) * 8;
			odr_h_exp = merge_width / hscaling;
			odr_v_exp = oddmr_priv->cfg.height / vscaling / merge_lines;
			ODDMRLOW_LOG("%s o_frm_done UDMA_R (0x%x 0x%x) exp (0x%x 0x%x)\n",
				mtk_dump_comp_str(comp), odr_h, odr_v, odr_h_exp, odr_v_exp);
			if ((odr_v != odr_v_exp && odr_v != 0)
				|| (odr_h != odr_h_exp && odr_h != 0)) {
				oddmr_err |= ODDMR_OD_UDMA_R_ERR;
				oddmr_err_trigger |= oddmr_err;
				DDPPR_ERR("%s %s err UDMA_R(0x%x 0x%x) exp(0x%x 0x%x) TS:%08llx\n",
					__func__, mtk_dump_comp_str(comp),
					odr_h, odr_v, odr_h_exp, odr_v_exp,
					arch_timer_read_counter());
			}
		}
		DDPIRQ("%s %s eof status:0x%x,0x%x\n", __func__, mtk_dump_comp_str(comp),
			status, oddmr_priv->irq_status);
	}
	if (status & DISP_ODDMR_IRQ_SOF) {
		status = oddmr_priv->irq_status | DISP_ODDMR_IRQ_SOF;

		/* reset irq status */
		od_enable = mtk_oddmr_read(comp, DISP_ODDMR_OD_CTRL_EN) & 0x01;
		oddmr_priv->irq_status = DISP_ODDMR_IRQ_SOF;
		/* do not care eof status when disable */
		if (!od_enable)
			oddmr_priv->irq_status = 0;
		DDPIRQ("%s %s sof status:0x%x,0x%x\n", __func__, mtk_dump_comp_str(comp),
			status, oddmr_priv->irq_status);
		if ((status & DISP_ODDMR_IRQ_I_FRM_DNE) &&
			(status & DISP_ODDMR_IRQ_O_FRM_DNE) &&
			!(status & DISP_ODDMR_IRQ_FRAME_DONE) &&
			!(status & DISP_ODDMR_IRQ_ODW_DONE)) {
			if (od_enable) {
				oddmr_err |= ODDMR_OD_UDMA_W_ERR;
				oddmr_err_trigger |= oddmr_err;
				DDPPR_ERR("%s %s err UDMA_W status 0x%x, TS: 0x%08llx\n", __func__,
					mtk_dump_comp_str(comp), status, arch_timer_read_counter());
			}
		}
	}
	if (od_opt && oddmr_err) {
		DDPAEE("ODDMR err 0x%x. TS: 0x%08llx\n",
			oddmr_err, arch_timer_read_counter());
		dump_en_tmp = g_oddmr_dump_en;
		g_oddmr_dump_en = true;
		mtk_drm_crtc_analysis(&(comp->mtk_crtc->base));
		mtk_drm_crtc_dump(&(comp->mtk_crtc->base));
		g_oddmr_dump_en = dump_en_tmp;
		mtk_smi_dbg_hang_detect("oddmr err");
	}
	ret = IRQ_HANDLED;
	mtk_drm_top_clk_isr_put("oddmr_irq");

	return ret;
}
#endif

static void mtk_oddmr_update_table_handle(struct work_struct *data)
{
	struct cmdq_pkt *cmdq_handle0 = NULL;
	struct cmdq_pkt *cmdq_handle1 = NULL;
	uint32_t update_sram_idx, updata_dram_idx;

	CRTC_MMP_EVENT_START(0, oddmr_ctl,
		g_oddmr_priv->od_data.od_dram_sel[0], g_oddmr_priv->od_data.od_dram_sel[1]);
	ODDMRFLOW_LOG("ODDMR_TABLE_UPDATING\n");
	g_oddmr_priv->od_state = ODDMR_TABLE_UPDATING;

	update_sram_idx = (uint32_t)!g_oddmr_priv->od_data.od_sram_read_sel;
	updata_dram_idx = (uint32_t)g_od_param.updata_dram_table;
	CRTC_MMP_MARK(0, oddmr_ctl, 10, 0);
	cmdq_handle0 =
		g_oddmr_priv->od_data.od_sram_pkgs[updata_dram_idx][update_sram_idx];
	cmdq_pkt_refinalize(cmdq_handle0);
	CRTC_MMP_MARK(0, oddmr_ctl, (unsigned long)cmdq_handle0, updata_dram_idx);
	cmdq_pkt_flush(cmdq_handle0);
	CRTC_MMP_MARK(0, oddmr_ctl, 10, 2);
	ODDMRFLOW_LOG("now dram0 %d, now_dram1 %d\n",
		g_oddmr_priv->od_data.od_dram_sel[0], g_oddmr_priv->od_data.od_dram_sel[1]);
	g_oddmr_priv->od_data.od_dram_sel[update_sram_idx] = updata_dram_idx;
	if (default_comp->mtk_crtc->is_dual_pipe) {
		cmdq_handle1 =
			g_oddmr1_priv->od_data.od_sram_pkgs[updata_dram_idx][update_sram_idx];
		cmdq_pkt_refinalize(cmdq_handle1);
		CRTC_MMP_MARK(0, oddmr_ctl, 11, 0);
		CRTC_MMP_MARK(0, oddmr_ctl, (unsigned long)cmdq_handle1, updata_dram_idx);
		cmdq_pkt_flush(cmdq_handle1);
		CRTC_MMP_MARK(0, oddmr_ctl, 11, 2);
		g_oddmr1_priv->od_data.od_dram_sel[update_sram_idx] = updata_dram_idx;
	}
	ODDMRFLOW_LOG("now sram %d, update_sram %d, dram %d\n",
		g_oddmr_priv->od_data.od_sram_read_sel,update_sram_idx, updata_dram_idx);
	g_oddmr_priv->od_state = ODDMR_INIT_DONE;
	ODDMRFLOW_LOG("ODDMR_INIT_DONE\n");
	CRTC_MMP_EVENT_END(0, oddmr_ctl, update_sram_idx, updata_dram_idx);
}

static int mtk_oddmr_create_workqueue(struct mtk_disp_oddmr *priv)
{
	priv->oddmr_wq = create_singlethread_workqueue("mtk_oddmr_wq");
	if (!priv->oddmr_wq) {
		DDPMSG("Failed to create oddmr workqueue\n");
		return -ENOMEM;
	}

	INIT_WORK(&priv->update_table_work, mtk_oddmr_update_table_handle);

	return 0;
}


static int mtk_disp_oddmr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_oddmr *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;
	struct sched_param param = {.sched_priority = 85 };
	struct cpumask mask;

	DDPMSG("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_ODDMR);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto err;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
			&mtk_disp_oddmr_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto err;
	}
	ret = of_property_read_u32(dev->of_node, "mediatek,larb-oddmr-dmrr", &priv->larb_dmrr);
	if (ret) {
		dev_err(dev, "Failed to initialize oddmr-dmrr: %d\n", ret);
		priv->larb_dmrr = 0;
	}
	ret = of_property_read_u32(dev->of_node, "mediatek,larb-oddmr-dbir", &priv->larb_dbir);
	if (ret) {
		dev_err(dev, "Failed to initialize oddmr-dbir: %d\n", ret);
		priv->larb_dbir = 0;
	}
	ret = of_property_read_u32(dev->of_node, "mediatek,larb-oddmr-odr", &priv->larb_odr);
	if (ret) {
		dev_err(dev, "Failed to initialize oddmr-odr: %d\n", ret);
		priv->larb_odr = 0;
	}
	ret = of_property_read_u32(dev->of_node, "mediatek,larb-oddmr-odw", &priv->larb_odw);
	if (ret) {
		dev_err(dev, "Failed to initialize oddmr-odw: %d\n", ret);
		priv->larb_odw = 0;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	if (comp_id == DDP_COMPONENT_ODDMR0) {
		priv->is_right_pipe = false;
		g_oddmr_priv = priv;
		if (g_oddmr_priv->data->is_od_4_table)
			mtk_oddmr_create_workqueue(g_oddmr_priv);
	}
	if (comp_id == DDP_COMPONENT_ODDMR1) {
		priv->is_right_pipe = true;
		g_oddmr1_priv = priv;
	}
	if (!default_comp && comp_id == DDP_COMPONENT_ODDMR0)
		default_comp = &priv->ddp_comp;
	if (!oddmr1_default_comp && comp_id == DDP_COMPONENT_ODDMR1)
		oddmr1_default_comp = &priv->ddp_comp;

#ifdef ODDMR_ENABLE_IRQ
	if (comp_id == DDP_COMPONENT_ODDMR0 ||
		comp_id == DDP_COMPONENT_ODDMR1) {
		int irq_num;

		irq_num = platform_get_irq(pdev, 0);
		if (irq_num < 0) {
			dev_err(&pdev->dev, "failed to request oddmr irq resource\n");
			ret = -EPROBE_DEFER;
			goto err;
		}
		irq_set_status_flags(irq_num, IRQ_TYPE_LEVEL_HIGH);
		ret = devm_request_irq(
			&pdev->dev, irq_num, priv->data->irq_handler,
			IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), priv);
		if (ret) {
			DDPAEE("%s:%d, failed to request irq:%d ret:%d\n",
					__func__, __LINE__,
					irq_num, ret);
			ret = -EPROBE_DEFER;
			goto err;
		}
	}
#endif

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_oddmr_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
		goto err;
	}

	/* probe only once */
	if (comp_id == DDP_COMPONENT_ODDMR0) {
		oddmr_sof_irq_event_task =
			kthread_create(mtk_oddmr_sof_irq_trigger,
					NULL, "oddmr_sof");
		cpumask_setall(&mask);
		cpumask_clear_cpu(0, &mask);
		set_cpus_allowed_ptr(oddmr_sof_irq_event_task, &mask);
		if (sched_setscheduler(oddmr_sof_irq_event_task, SCHED_RR, &param))
			DDPMSG("oddmr_sof_irq_event_task setschedule fail");
		wake_up_process(oddmr_sof_irq_event_task);
	}

	priv->od_data.od_sram_read_sel = -1;
	priv->od_data.od_sram_table_idx[0] = -1;
	priv->od_data.od_sram_table_idx[1] = -1;


	atomic_set(&priv->dbi_data.cur_table_idx, 0);
	atomic_set(&priv->dbi_data.update_table_idx, 0);
	atomic_set(&priv->dbi_data.update_table_done, 0);
	priv->dbi_cfg_info.fps_dbv_node.remap_gain_target_code = 255;
	atomic_set(&priv->dbi_data.enter_scp, 0);
	atomic_set(&priv->dbi_data.max_time_set_done, 0);
	atomic_set(&priv->dbi_data.gain_ratio, 100);
	atomic_set(&priv->dbi_data.remap_enable, 0);

	DDPMSG("%s id %d, type %d\n", __func__, comp_id, mtk_ddp_comp_get_type(comp_id));
	return ret;
err:
	return ret;
}

static int mtk_disp_oddmr_remove(struct platform_device *pdev)
{
	struct mtk_disp_oddmr *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_oddmr_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_oddmr_data mt6985_oddmr_driver_data = {
	.need_bypass_shadow = true,
	.is_od_support_table_update = false,
	.is_support_rtff = false,
	.is_od_support_hw_skip_first_frame = false,
	.is_od_need_crop_garbage = true,
	.is_od_need_force_clk = true,
	.is_od_support_sec = false,
	.is_od_merge_lines = true,
	.is_od_4_table = false,
	.tile_overhead = 8,
	.dmr_buffer_size = 458,
	.odr_buffer_size = 264,
	.odw_buffer_size = 264,
	.irq_handler = mtk_oddmr_check_framedone,
};

static const struct mtk_disp_oddmr_data mt6897_oddmr_driver_data = {
	.need_bypass_shadow = true,
	.is_od_support_table_update = false,
	.is_support_rtff = false,
	.is_od_support_hw_skip_first_frame = false,
	.is_od_need_crop_garbage = false,
	.is_od_need_force_clk = false,
	.is_od_support_sec = false,
	.is_od_merge_lines = true,
	.is_od_4_table = false,
	.tile_overhead = 0,
	.dmr_buffer_size = 458,
	.odr_buffer_size = 264,
	.odw_buffer_size = 264,
	.irq_handler = mtk_oddmr_check_framedone,
};

static const struct mtk_disp_oddmr_data mt6989_oddmr_driver_data = {
	.need_bypass_shadow = true,
	.is_od_support_table_update = false,
	.is_support_rtff = false,
	.is_od_support_hw_skip_first_frame = false,
	.is_od_need_crop_garbage = false,
	.is_od_need_force_clk = false,
	.is_od_support_sec = false,
	.is_od_merge_lines = true,
	.is_od_4_table = true,
	.p_num = 2,
	.tile_overhead = 8,
	.dmr_buffer_size = 458,
	.odr_buffer_size = 960,
	.odw_buffer_size = 960,
	.irq_handler = mtk_oddmr_check_framedone,
};

static const struct of_device_id mtk_disp_oddmr_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6985-disp-oddmr",
	  .data = &mt6985_oddmr_driver_data},
	{ .compatible = "mediatek,mt6897-disp-oddmr",
	  .data = &mt6897_oddmr_driver_data},
	{ .compatible = "mediatek,mt6989-disp-oddmr",
	  .data = &mt6989_oddmr_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_oddmr_driver_dt_match);

struct platform_driver mtk_disp_oddmr_driver = {
	.probe = mtk_disp_oddmr_probe,
	.remove = mtk_disp_oddmr_remove,
	.driver = {
		.name = "mediatek-disp-oddmr",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_oddmr_driver_dt_match,
	},
};
