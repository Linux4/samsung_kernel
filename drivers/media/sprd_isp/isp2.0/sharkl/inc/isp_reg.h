/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

//#include <mach/hardware.h>
#ifndef CONFIG_64BIT
#include <soc/sprd/hardware.h>
#endif

#define ISP_BASE_ADDR               ISP_BASE//SPRD_ISP_BASE

/*isp sub block1: Fetch*/
#define ISP_FETCH_STATUS            (ISP_BASE_ADDR+0x0000UL)
#define ISP_FETCH_STATUS_PREVIEW    (ISP_BASE_ADDR+0x0004UL)
#define ISP_FETCH_PARAM             (ISP_BASE_ADDR+0x0014UL)
#define ISP_FETCH_SLICE_SIZE        (ISP_BASE_ADDR+0x0018UL)
#define ISP_FETCH_SLICE_Y_ADDR      (ISP_BASE_ADDR+0x001CUL)
#define ISP_FETCH_SLICE_Y_PITCH     (ISP_BASE_ADDR+0x0020UL)
#define ISP_FETCH_SLICE_U_ADDR      (ISP_BASE_ADDR+0x0024UL)
#define ISP_FETCH_SLICE_U_PITCH     (ISP_BASE_ADDR+0x0028UL)
#define ISP_FETCH_MIPI_WORD_INFO    (ISP_BASE_ADDR+0x002CUL)
#define ISP_FETCH_MIPI_BYTE_INFO    (ISP_BASE_ADDR+0x0030UL)
#define ISP_FETCH_SLICE_V_ADDR      (ISP_BASE_ADDR+0x0034UL)
#define ISP_FETCH_SLICE_V_PITCH     (ISP_BASE_ADDR+0x0038UL)
#define ISP_FETCH_PREVIEW_CNT       (ISP_BASE_ADDR+0x003CUL)
#define ISP_FETCH_BUF_ADDR          (ISP_BASE_ADDR+0x0040UL)

/*isp sub block2: BLC: back level calibration*/
#define ISP_BLC_STATUS              (ISP_BASE_ADDR+0x0100UL)
#define ISP_BLC_PARAM               (ISP_BASE_ADDR+0x0114UL)
#define ISP_BLC_B_PARAM_R_B        (ISP_BASE_ADDR+0x0118UL)
#define ISP_BLC_B_PARAM_G           (ISP_BASE_ADDR+0x011CUL)
#define ISP_BLC_SLICE_SIZE          (ISP_BASE_ADDR+0x0120UL)
#define ISP_BLC_SLICE_INFO          (ISP_BASE_ADDR+0x0124UL)

/*isp sub block3: lens shading calibration*/
#define ISP_LSC_STATUS             (ISP_BASE_ADDR+0x0200UL)
#define ISP_LSC_PARAM              (ISP_BASE_ADDR+0x0214UL)
#define ISP_LSC_PARAM_ADDR         (ISP_BASE_ADDR+0x0218UL)
#define ISP_LSC_SLICE_POS          (ISP_BASE_ADDR+0x021CUL)
#define ISP_LSC_LOAD_EB            (ISP_BASE_ADDR+0x0220UL)
#define ISP_LSC_GRID_PITCH         (ISP_BASE_ADDR+0x0224UL)
#define ISP_LSC_GRID_SIZE          (ISP_BASE_ADDR+0x0228UL)
#define ISP_LSC_LOAD_BUF           (ISP_BASE_ADDR+0x022CUL)
#define ISP_LSC_MISC               (ISP_BASE_ADDR+0x0230UL)
#define ISP_LSC_SLICE_SIZE         (ISP_BASE_ADDR+0x0234UL)
#define ISP_LSC_GRID_DONE_SEL      (ISP_BASE_ADDR+0x0238UL)

/*isp sub block4: AWBM*/
#define ISP_AWBM_STATUS             (ISP_BASE_ADDR+0x0300UL)
#define ISP_AWBM_PARAM              (ISP_BASE_ADDR+0x0314UL)
#define ISP_AWBM_BLOCK_OFFSET       (ISP_BASE_ADDR+0x0318UL)
#define ISP_AWBM_BLOCK_SIZE         (ISP_BASE_ADDR+0x031CUL)

/*isp sub block5: AWBC*/
#define ISP_AWBC_STATUS             (ISP_BASE_ADDR+0x0400UL)
#define ISP_AWBC_PARAM              (ISP_BASE_ADDR+0x0414UL)
#define ISP_AWBC_GAIN0              (ISP_BASE_ADDR+0x0418UL)
#define ISP_AWBC_GAIN1              (ISP_BASE_ADDR+0x041CUL)
#define ISP_AWBC_THRD               (ISP_BASE_ADDR+0x0420UL)
#define ISP_AWBC_OFFSET0            (ISP_BASE_ADDR+0x0424UL)
#define ISP_AWBC_OFFSET1            (ISP_BASE_ADDR+0x0428UL)

/*isp sub block6: BPC: bad pixel correction*/
#define ISP_BPC_STATUS              (ISP_BASE_ADDR+0x0500UL)
#define ISP_BPC_PARAM               (ISP_BASE_ADDR+0x0514UL)
#define ISP_BPC_THRD                (ISP_BASE_ADDR+0x0518UL)
#define ISP_BPC_MAP_ADDR            (ISP_BASE_ADDR+0x051CUL)
#define ISP_BPC_PIXEL_NUM           (ISP_BASE_ADDR+0x0520UL)
#define ISP_BPC_DIFF_THRD           (ISP_BASE_ADDR+0x0524UL)

/*isp sub block7: wavelet denoise*/
#define ISP_WAVE_STATUS             (ISP_BASE_ADDR+0x0600UL)
#define ISP_WAVE_PARAM              (ISP_BASE_ADDR+0x0614UL)
#define ISP_WAVE_SLICE_SIZE         (ISP_BASE_ADDR+0x061CUL)
#define ISP_WAVE_DISWEI_0           (ISP_BASE_ADDR+0x0624UL)
#define ISP_WAVE_DISWEI_1           (ISP_BASE_ADDR+0x0628UL)
#define ISP_WAVE_DISWEI_2           (ISP_BASE_ADDR+0x062CUL)
#define ISP_WAVE_DISWEI_3           (ISP_BASE_ADDR+0x0630UL)
#define ISP_WAVE_DISWEI_4           (ISP_BASE_ADDR+0x0634UL)
#define ISP_WAVE_RANWEI_0           (ISP_BASE_ADDR+0x0638UL)
#define ISP_WAVE_RANWEI_1           (ISP_BASE_ADDR+0x063CUL)
#define ISP_WAVE_RANWEI_2           (ISP_BASE_ADDR+0x0640UL)
#define ISP_WAVE_RANWEI_3           (ISP_BASE_ADDR+0x0644UL)
#define ISP_WAVE_RANWEI_4           (ISP_BASE_ADDR+0x0648UL)
#define ISP_WAVE_RANWEI_5           (ISP_BASE_ADDR+0x064CUL)
#define ISP_WAVE_RANWEI_6           (ISP_BASE_ADDR+0x0650UL)
#define ISP_WAVE_RANWEI_7           (ISP_BASE_ADDR+0x0654UL)

/*isp sub block8: GRGB*/
#define ISP_GRGB_STATUS             (ISP_BASE_ADDR+0x0700UL)
#define ISP_GRGB_PARAM              (ISP_BASE_ADDR+0x0714UL)

/*isp sub block9: CAF: clolor filter array*/
#define ISP_CFA_STATUS              (ISP_BASE_ADDR+0x0800UL)
#define ISP_CFA_PARAM               (ISP_BASE_ADDR+0x0814UL)
#define ISP_CFA_SLICE_SIZE          (ISP_BASE_ADDR+0x0818UL)
#define ISP_CFA_SLICE_INFO          (ISP_BASE_ADDR+0x081CUL)

/*isp sub block10: CMC: clolor matrix correction*/
#define ISP_CMC_STATUS              (ISP_BASE_ADDR+0x0900UL)
#define ISP_CMC_PARAM               (ISP_BASE_ADDR+0x0914UL)
#define ISP_CMC_MATRIX0             (ISP_BASE_ADDR+0x0918UL)
#define ISP_CMC_MATRIX1             (ISP_BASE_ADDR+0x091cUL)
#define ISP_CMC_MATRIX2             (ISP_BASE_ADDR+0x0920UL)
#define ISP_CMC_MATRIX3             (ISP_BASE_ADDR+0x0924UL)
#define ISP_CMC_MATRIX4             (ISP_BASE_ADDR+0x0928UL)

/*isp sub block11: GAMMA1 */
#define ISP_GAMMA_STATUS            (ISP_BASE_ADDR+0x0A00UL)
#define ISP_GAMMA_PARAM             (ISP_BASE_ADDR+0x0A14UL)
#define ISP_GAMMA_NODE_R0           (ISP_BASE_ADDR+0x0A18UL)
#define ISP_GAMMA_NODE_R1           (ISP_BASE_ADDR+0x0A1CUL)
#define ISP_GAMMA_NODE_R2           (ISP_BASE_ADDR+0x0A20UL)
#define ISP_GAMMA_NODE_R3           (ISP_BASE_ADDR+0x0A24UL)
#define ISP_GAMMA_NODE_R4           (ISP_BASE_ADDR+0x0A28UL)
#define ISP_GAMMA_NODE_R5           (ISP_BASE_ADDR+0x0A2CUL)
#define ISP_GAMMA_NODE_R6           (ISP_BASE_ADDR+0x0A30UL)
#define ISP_GAMMA_NODE_R7           (ISP_BASE_ADDR+0x0A34UL)
#define ISP_GAMMA_NODE_R8           (ISP_BASE_ADDR+0x0A38UL)
#define ISP_GAMMA_NODE_R9           (ISP_BASE_ADDR+0x0A3CUL)
#define ISP_GAMMA_NODE_R10          (ISP_BASE_ADDR+0x0A40UL)
#define ISP_GAMMA_NODE_R11          (ISP_BASE_ADDR+0x0A44UL)
#define ISP_GAMMA_NODE_R12          (ISP_BASE_ADDR+0x0A48UL)
#define ISP_GAMMA_NODE_R13          (ISP_BASE_ADDR+0x0A4CUL)
#define ISP_GAMMA_NODE_R14          (ISP_BASE_ADDR+0x0A50UL)
#define ISP_GAMMA_NODE_R15          (ISP_BASE_ADDR+0x0A54UL)
#define ISP_GAMMA_NODE_G0           (ISP_BASE_ADDR+0x0A58UL)
#define ISP_GAMMA_NODE_G1           (ISP_BASE_ADDR+0x0A5CUL)
#define ISP_GAMMA_NODE_G2           (ISP_BASE_ADDR+0x0A60UL)
#define ISP_GAMMA_NODE_G3           (ISP_BASE_ADDR+0x0A64UL)
#define ISP_GAMMA_NODE_G4           (ISP_BASE_ADDR+0x0A68UL)
#define ISP_GAMMA_NODE_G5           (ISP_BASE_ADDR+0x0A6CUL)
#define ISP_GAMMA_NODE_G6           (ISP_BASE_ADDR+0x0A70UL)
#define ISP_GAMMA_NODE_G7           (ISP_BASE_ADDR+0x0A74UL)
#define ISP_GAMMA_NODE_G8           (ISP_BASE_ADDR+0x0A78UL)
#define ISP_GAMMA_NODE_G9           (ISP_BASE_ADDR+0x0A7CUL)
#define ISP_GAMMA_NODE_G10          (ISP_BASE_ADDR+0x0A70UL)
#define ISP_GAMMA_NODE_G11          (ISP_BASE_ADDR+0x0A74UL)
#define ISP_GAMMA_NODE_G12          (ISP_BASE_ADDR+0x0A88UL)
#define ISP_GAMMA_NODE_G13          (ISP_BASE_ADDR+0x0A8CUL)
#define ISP_GAMMA_NODE_G14          (ISP_BASE_ADDR+0x0A80UL)
#define ISP_GAMMA_NODE_G15          (ISP_BASE_ADDR+0x0A94UL)
#define ISP_GAMMA_NODE_B0           (ISP_BASE_ADDR+0x0A98UL)
#define ISP_GAMMA_NODE_B1           (ISP_BASE_ADDR+0x0A9CUL)
#define ISP_GAMMA_NODE_B2           (ISP_BASE_ADDR+0x0AA0UL)
#define ISP_GAMMA_NODE_B3           (ISP_BASE_ADDR+0x0AA4UL)
#define ISP_GAMMA_NODE_B4           (ISP_BASE_ADDR+0x0AA8UL)
#define ISP_GAMMA_NODE_B5           (ISP_BASE_ADDR+0x0AACUL)
#define ISP_GAMMA_NODE_B6           (ISP_BASE_ADDR+0x0AB0UL)
#define ISP_GAMMA_NODE_B7           (ISP_BASE_ADDR+0x0AB4UL)
#define ISP_GAMMA_NODE_B8           (ISP_BASE_ADDR+0x0AB8UL)
#define ISP_GAMMA_NODE_B9           (ISP_BASE_ADDR+0x0ABCUL)
#define ISP_GAMMA_NODE_B10          (ISP_BASE_ADDR+0x0AB0UL)
#define ISP_GAMMA_NODE_B11          (ISP_BASE_ADDR+0x0AC4UL)
#define ISP_GAMMA_NODE_B12          (ISP_BASE_ADDR+0x0AC8UL)
#define ISP_GAMMA_NODE_B13          (ISP_BASE_ADDR+0x0ACCUL)
#define ISP_GAMMA_NODE_B14          (ISP_BASE_ADDR+0x0AC0UL)
#define ISP_GAMMA_NODE_B15          (ISP_BASE_ADDR+0x0AD4UL)

/*isp sub block12: CCE: clolor conversion enhancement*/
#define ISP_CCE_STATUS              (ISP_BASE_ADDR+0x0B00UL)
#define ISP_CCE_PARAM               (ISP_BASE_ADDR+0x0B14UL)
#define ISP_CCE_MATRIX0             (ISP_BASE_ADDR+0x0B18UL)
#define ISP_CCE_MATRIX1             (ISP_BASE_ADDR+0x0B1CUL)
#define ISP_CCE_MATRIX2             (ISP_BASE_ADDR+0x0B20UL)
#define ISP_CCE_MATRIX3             (ISP_BASE_ADDR+0x0B24UL)
#define ISP_CCE_MATRIX4             (ISP_BASE_ADDR+0x0B28UL)
#define ISP_CCE_SHIFT               (ISP_BASE_ADDR+0x0B2CUL)
#define ISP_CCE_UVD_THRD0           (ISP_BASE_ADDR+0x0B30UL)
#define ISP_CCE_UVD_THRD1           (ISP_BASE_ADDR+0x0B34UL)
#define ISP_CCE_UVC_PARAM0          (ISP_BASE_ADDR+0x0B38UL)
#define ISP_CCE_UVC_PARAM1          (ISP_BASE_ADDR+0x0B3CUL)

/*isp sub block13: Pre-Filter*/
#define ISP_PREF_STATUS             (ISP_BASE_ADDR+0x0C00UL)
#define ISP_PREF_PARAM              (ISP_BASE_ADDR+0x0C14UL)
#define ISP_PREF_THRD               (ISP_BASE_ADDR+0x0C18UL)
#define ISP_PREF_SLICE_SIZE         (ISP_BASE_ADDR+0x0C1CUL)
#define ISP_PREF_SLICE_INFO         (ISP_BASE_ADDR+0x0C20UL)

/*isp sub block14: Brightness*/
#define ISP_BRIGHT_STATUS           (ISP_BASE_ADDR+0x0D00UL)
#define ISP_BRIGHT_PARAM            (ISP_BASE_ADDR+0x0D14UL)
#define ISP_BRIGHT_SLICE_SIZE       (ISP_BASE_ADDR+0x0D18UL)
#define ISP_BRIGHT_SLICE_INFO       (ISP_BASE_ADDR+0x0D1CUL)

/*isp sub block15: Contrast*/
#define ISP_CONTRAST_STATUS         (ISP_BASE_ADDR+0x0E00UL)
#define ISP_CONTRAST_PARAM          (ISP_BASE_ADDR+0x0E14UL)

/*isp sub block16: HIST : histogram*/
#define ISP_HIST_STATUS             (ISP_BASE_ADDR+0X0F00UL)
#define ISP_HIST_PARAM              (ISP_BASE_ADDR+0X0F14UL)
#define ISP_HIST_RATIO              (ISP_BASE_ADDR+0X0F18UL)
#define ISP_HIST_MAX_MIN            (ISP_BASE_ADDR+0X0F1CUL)
#define ISP_HIST_CLEAR_EB           (ISP_BASE_ADDR+0X0F20UL)

/*isp sub block17: AUTOCONT : auto contrast adjustment*/
#define ISP_AUTOCONT_STATUS         (ISP_BASE_ADDR+0x1000UL)
#define ISP_AUTOCONT_MAXMIN_STATUS  (ISP_BASE_ADDR+0x1004UL)
#define ISP_AUTOCONT_PARAM          (ISP_BASE_ADDR+0x1014UL)
#define ISP_AUTOCONT_MAX_MIN        (ISP_BASE_ADDR+0x1018UL)
#define ISP_AUTOCONT_ADJUST         (ISP_BASE_ADDR+0X101CUL)

/*isp sub block18: AFM : auto focus monitor*/
#define ISP_AFM_STATUS              (ISP_BASE_ADDR+0x1100UL)
#define ISP_AFM_PARAM               (ISP_BASE_ADDR+0x1114UL)
#define ISP_AFM_WIN_RANGE0          (ISP_BASE_ADDR+0x1118UL)
#define ISP_AFM_WIN_RANGE1          (ISP_BASE_ADDR+0X111CUL)
#define ISP_AFM_WIN_RANGE2          (ISP_BASE_ADDR+0x1120UL)
#define ISP_AFM_WIN_RANGE3          (ISP_BASE_ADDR+0x1124UL)
#define ISP_AFM_WIN_RANGE4          (ISP_BASE_ADDR+0x1128UL)
#define ISP_AFM_WIN_RANGE5          (ISP_BASE_ADDR+0X112CUL)
#define ISP_AFM_WIN_RANGE6          (ISP_BASE_ADDR+0x1130UL)
#define ISP_AFM_WIN_RANGE7          (ISP_BASE_ADDR+0x1134UL)
#define ISP_AFM_WIN_RANGE8          (ISP_BASE_ADDR+0x1138UL)
#define ISP_AFM_WIN_RANGE9          (ISP_BASE_ADDR+0x113CUL)
#define ISP_AFM_WIN_RANGE10         (ISP_BASE_ADDR+0x1140UL)
#define ISP_AFM_WIN_RANGE11         (ISP_BASE_ADDR+0x1144UL)
#define ISP_AFM_WIN_RANGE12         (ISP_BASE_ADDR+0x1148UL)
#define ISP_AFM_WIN_RANGE13         (ISP_BASE_ADDR+0X114CUL)
#define ISP_AFM_WIN_RANGE14         (ISP_BASE_ADDR+0x1150UL)
#define ISP_AFM_WIN_RANGE15         (ISP_BASE_ADDR+0x1154UL)
#define ISP_AFM_WIN_RANGE16         (ISP_BASE_ADDR+0x1158UL)
#define ISP_AFM_WIN_RANGE17         (ISP_BASE_ADDR+0x115CUL)
#define ISP_AFM_STATISTIC0_L        (ISP_BASE_ADDR+0x1160UL)
#define ISP_AFM_STATISTIC0_H        (ISP_BASE_ADDR+0x1164UL)
#define ISP_AFM_STATISTIC1_L        (ISP_BASE_ADDR+0x1168UL)
#define ISP_AFM_STATISTIC1_H        (ISP_BASE_ADDR+0x116CUL)
#define ISP_AFM_STATISTIC2_L        (ISP_BASE_ADDR+0x1170UL)
#define ISP_AFM_STATISTIC2_H        (ISP_BASE_ADDR+0x1174UL)
#define ISP_AFM_STATISTIC3_L        (ISP_BASE_ADDR+0x1178UL)
#define ISP_AFM_STATISTIC3_H        (ISP_BASE_ADDR+0x117CUL)
#define ISP_AFM_STATISTIC4_L        (ISP_BASE_ADDR+0x1180UL)
#define ISP_AFM_STATISTIC4_H        (ISP_BASE_ADDR+0x1184UL)
#define ISP_AFM_STATISTIC5_L        (ISP_BASE_ADDR+0x1188UL)
#define ISP_AFM_STATISTIC5_H        (ISP_BASE_ADDR+0x118CUL)
#define ISP_AFM_STATISTIC6_L        (ISP_BASE_ADDR+0x1190UL)
#define ISP_AFM_STATISTIC6_H        (ISP_BASE_ADDR+0x1194UL)
#define ISP_AFM_STATISTIC7_L        (ISP_BASE_ADDR+0x1198UL)
#define ISP_AFM_STATISTIC7_H        (ISP_BASE_ADDR+0x119CUL)
#define ISP_AFM_STATISTIC8_L        (ISP_BASE_ADDR+0x11A0UL)
#define ISP_AFM_STATISTIC8_H        (ISP_BASE_ADDR+0x11A4UL)

/*isp sub block19: EDGE : edge enhancement*/
#define ISP_EDGE_STATUS             (ISP_BASE_ADDR+0x1200UL)
#define ISP_EDGE_PARAM              (ISP_BASE_ADDR+0x1214UL)

/*isp sub block20: EMBOSS*/
#define ISP_EMBOSS_STATUS           (ISP_BASE_ADDR+0x1300UL)
#define ISP_EMBOSS_PARAM            (ISP_BASE_ADDR+0x1314UL)

/*isp sub block21: FCS : false color suppression*/
#define ISP_FCS_STATUS              (ISP_BASE_ADDR+0x1400UL)
#define ISP_FCS_PARAM               (ISP_BASE_ADDR+0x1414UL)

/*isp sub block22: CSS : color saturation suppression*/
#define ISP_CSS_STATUS              (ISP_BASE_ADDR+0x1500UL)
#define ISP_CSS_PARAM               (ISP_BASE_ADDR+0x1514UL)
#define ISP_CSS_THRD0               (ISP_BASE_ADDR+0x1518UL)
#define ISP_CSS_THRD1               (ISP_BASE_ADDR+0x151CUL)
#define ISP_CSS_THRD2               (ISP_BASE_ADDR+0x1520UL)
#define ISP_CSS_THRD3               (ISP_BASE_ADDR+0x1524UL)
#define ISP_CSS_SLICE_INFO          (ISP_BASE_ADDR+0x1528UL)
#define ISP_CSS_RATIO               (ISP_BASE_ADDR+0x152CUL)

/*isp sub block23: CSA : color saturation adjustment*/
#define ISP_CSA_STATUS              (ISP_BASE_ADDR+0x1600UL)
#define ISP_CSA_PARAM               (ISP_BASE_ADDR+0x1614UL)

/*isp sub block24: Store*/
#define ISP_STORE_STATUS            (ISP_BASE_ADDR+0x1700UL)
#define ISP_STORE_STATUS_PREVIEW    (ISP_BASE_ADDR+0x1704UL)
#define ISP_STORE_PARAM             (ISP_BASE_ADDR+0x1714UL)
#define ISP_STORE_SLICE_SIZE        (ISP_BASE_ADDR+0x1718UL)
#define ISP_STORE_Y_ADDR            (ISP_BASE_ADDR+0x171CUL)
#define ISP_STORE_Y_PITCH           (ISP_BASE_ADDR+0x1720UL)
#define ISP_STORE_U_ADDR            (ISP_BASE_ADDR+0x1724UL)
#define ISP_STORE_U_PITCH           (ISP_BASE_ADDR+0x1728UL)
#define ISP_STORE_V_ADDR            (ISP_BASE_ADDR+0x172CUL)
#define ISP_STORE_V_PITCH           (ISP_BASE_ADDR+0x1730UL)
#define ISP_STORE_INT_CTRL          (ISP_BASE_ADDR+0x1734UL)
#define ISP_STORE_BORDER            (ISP_BASE_ADDR+0x1738UL)

/*isp sub block25: Feeder*/
#define ISP_FEEDER_STATUS           (ISP_BASE_ADDR+0x1800UL)
#define ISP_FEEDER_PARAM            (ISP_BASE_ADDR+0x1814UL)
#define ISP_FEEDER_SLICE_SIZE       (ISP_BASE_ADDR+0x1818UL)

/*isp sub block26: Arbiter*/
#define ISP_ARBITER_STATUS          (ISP_BASE_ADDR+0x1900UL)
#define ISP_ARBITER_WR_STATUS       (ISP_BASE_ADDR+0x1904UL)
#define ISP_ARBITER_RD_STATUS       (ISP_BASE_ADDR+0x1908UL)
#define ISP_ARBITER_PRERST          (ISP_BASE_ADDR+0x1914UL)
#define ISP_ARBITER_PAUSE_CYCLE     (ISP_BASE_ADDR+0x1818UL)

/*isp sub block27: HDR: high dynamic range*/
#define ISP_HDR_STATUS              (ISP_BASE_ADDR+0x1A00UL)
#define ISP_HDR_PARAM               (ISP_BASE_ADDR+0x1A14UL)
#define ISP_HDR_INDEX               (ISP_BASE_ADDR+0x1A18UL)

/*isp sub block28: NLC*/
#define ISP_NLC_STATUS              (ISP_BASE_ADDR+0x1B00UL)
#define ISP_NLC_PARAM               (ISP_BASE_ADDR+0x1B14UL)
#define ISP_NLC_N_PARAM_R0          (ISP_BASE_ADDR+0x1B18UL)
#define ISP_NLC_N_PARAM_R1          (ISP_BASE_ADDR+0x1B1CUL)
#define ISP_NLC_N_PARAM_R2          (ISP_BASE_ADDR+0x1B20UL)
#define ISP_NLC_N_PARAM_R3          (ISP_BASE_ADDR+0x1B24UL)
#define ISP_NLC_N_PARAM_R4          (ISP_BASE_ADDR+0x1B28UL)
#define ISP_NLC_N_PARAM_R5          (ISP_BASE_ADDR+0x1B2CUL)
#define ISP_NLC_N_PARAM_R6          (ISP_BASE_ADDR+0x1B30UL)
#define ISP_NLC_N_PARAM_R7          (ISP_BASE_ADDR+0x1B34UL)
#define ISP_NLC_N_PARAM_R8          (ISP_BASE_ADDR+0x1B38UL)
#define ISP_NLC_N_PARAM_R9          (ISP_BASE_ADDR+0x1B3CUL)
#define ISP_NLC_N_PARAM_G0          (ISP_BASE_ADDR+0x1B40UL)
#define ISP_NLC_N_PARAM_G1          (ISP_BASE_ADDR+0x1B44UL)
#define ISP_NLC_N_PARAM_G2          (ISP_BASE_ADDR+0x1B48UL)
#define ISP_NLC_N_PARAM_G3          (ISP_BASE_ADDR+0x1B4CUL)
#define ISP_NLC_N_PARAM_G4          (ISP_BASE_ADDR+0x1B50UL)
#define ISP_NLC_N_PARAM_G5          (ISP_BASE_ADDR+0x1B54UL)
#define ISP_NLC_N_PARAM_G6          (ISP_BASE_ADDR+0x1B58UL)
#define ISP_NLC_N_PARAM_G7          (ISP_BASE_ADDR+0x1B5CUL)
#define ISP_NLC_N_PARAM_G8          (ISP_BASE_ADDR+0x1B60UL)
#define ISP_NLC_N_PARAM_G9          (ISP_BASE_ADDR+0x1B64UL)
#define ISP_NLC_N_PARAM_B0          (ISP_BASE_ADDR+0x1B68UL)
#define ISP_NLC_N_PARAM_B1          (ISP_BASE_ADDR+0x1B6CUL)
#define ISP_NLC_N_PARAM_B2          (ISP_BASE_ADDR+0x1B70UL)
#define ISP_NLC_N_PARAM_B3          (ISP_BASE_ADDR+0x1B74UL)
#define ISP_NLC_N_PARAM_B4          (ISP_BASE_ADDR+0x1B78UL)
#define ISP_NLC_N_PARAM_B5          (ISP_BASE_ADDR+0x1B7CUL)
#define ISP_NLC_N_PARAM_B6          (ISP_BASE_ADDR+0x1B80UL)
#define ISP_NLC_N_PARAM_B7          (ISP_BASE_ADDR+0x1B84UL)
#define ISP_NLC_N_PARAM_B8          (ISP_BASE_ADDR+0x1B88UL)
#define ISP_NLC_N_PARAM_B9          (ISP_BASE_ADDR+0x1B8CUL)
#define ISP_NLC_N_PARAM_L0          (ISP_BASE_ADDR+0x1B90UL)
#define ISP_NLC_N_PARAM_L1          (ISP_BASE_ADDR+0x1B94UL)
#define ISP_NLC_N_PARAM_L2          (ISP_BASE_ADDR+0x1B98UL)
#define ISP_NLC_N_PARAM_L3          (ISP_BASE_ADDR+0x1B9CUL)
#define ISP_NLC_N_PARAM_L4          (ISP_BASE_ADDR+0x1BA0UL)
#define ISP_NLC_N_PARAM_L5          (ISP_BASE_ADDR+0x1BA4UL)
#define ISP_NLC_N_PARAM_L6          (ISP_BASE_ADDR+0x1BA8UL)
#define ISP_NLC_N_PARAM_L7          (ISP_BASE_ADDR+0x1BACUL)
#define ISP_NLC_N_PARAM_L8          (ISP_BASE_ADDR+0x1BB0UL)

/*isp sub block29: NAWBM*/
#define ISP_NAWBM_STATUS            (ISP_BASE_ADDR+0x1C00UL)
#define ISP_NAWBM_PARAM             (ISP_BASE_ADDR+0x1C14UL)
#define ISP_NAWBM_OFFSET            (ISP_BASE_ADDR+0x1C18UL)
#define ISP_NAWBM_BLK_SIZE          (ISP_BASE_ADDR+0x1C1CUL)
#define ISP_NAWBM_WR_0_S            (ISP_BASE_ADDR+0x1C20UL)
#define ISP_NAWBM_WR_0_E            (ISP_BASE_ADDR+0x1C24UL)
#define ISP_NAWBM_WR_1_S            (ISP_BASE_ADDR+0x1C28UL)
#define ISP_NAWBM_WR_1_E            (ISP_BASE_ADDR+0x1C2CUL)
#define ISP_NAWBM_WR_2_S            (ISP_BASE_ADDR+0x1C30UL)
#define ISP_NAWBM_WR_2_E            (ISP_BASE_ADDR+0x1C34UL)
#define ISP_NAWBM_WR_3_S            (ISP_BASE_ADDR+0x1C38UL)
#define ISP_NAWBM_WR_3_E            (ISP_BASE_ADDR+0x1C40UL)
#define ISP_NAWBM_WR_4_S            (ISP_BASE_ADDR+0x1C44UL)
#define ISP_NAWBM_WR_4_E            (ISP_BASE_ADDR+0x1C48UL)
#define ISP_NAWBM_ADDR              (ISP_BASE_ADDR+0x1C98UL)

/*isp sub block30: PRE WAVELET*/
#define ISP_PRE_WAVELET_STATUS      (ISP_BASE_ADDR+0x1D00UL)
#define ISP_PRE_WAVELET_PARAM       (ISP_BASE_ADDR+0x1D14UL)

/*isp sub block31: BING4AWB*/
#define ISP_BINNING_STATUS          (ISP_BASE_ADDR+0x1E00UL)
#define ISP_BINNING_PARAM           (ISP_BASE_ADDR+0x1E14UL)
#define ISP_BINNING_ADDR            (ISP_BASE_ADDR+0x1E18UL)
#define ISP_BINNING_PITCH           (ISP_BASE_ADDR+0x1E1CUL)

/*isp sub block32: PRE GLOBAL GAIN*/
#define ISP_PRE_GLOBAL_GAIN_STATUS  (ISP_BASE_ADDR+0X1F00UL)
#define ISP_PRE_GLOBAL_GAIN         (ISP_BASE_ADDR+0X1F14UL)

/*isp sub block33: COMMON : common register*/
#define ISP_COMMON_STATUS           (ISP_BASE_ADDR+0x2000UL)
#define ISP_COMMON_START            (ISP_BASE_ADDR+0x2014UL)
#define ISP_COMMON_PARAM            (ISP_BASE_ADDR+0x2018UL)
#define ISP_COMMON_BURST_SIZE       (ISP_BASE_ADDR+0x201CUL)
#define ISP_COMMON_MEM_SWITCH       (ISP_BASE_ADDR+0x2020UL)// sharkl no
#define ISP_COMMON_SHADOW           (ISP_BASE_ADDR+0x2024UL)
#define ISP_COMMON_BAYER_MODE       (ISP_BASE_ADDR+0x2028UL)
#define ISP_COMMON_SHADOW_ALL       (ISP_BASE_ADDR+0x202CUL)
#define ISP_COMMON_PMU_RAM_MASK     (ISP_BASE_ADDR+0x2038UL)
#define ISP_COMMON_HW_MASK          (ISP_BASE_ADDR+0x203CUL)
#define ISP_COMMON_HW_ENABLE        (ISP_BASE_ADDR+0x2040UL)
#define ISP_COMMON_SW_SWITCH        (ISP_BASE_ADDR+0x2044UL)
#define ISP_COMMON_SW_TOGGLE        (ISP_BASE_ADDR+0x2048UL)
#define ISP_COMMON_PREVIEW_STOP     (ISP_BASE_ADDR+0x204CUL)
#define ISP_COMMON_SHADOW_CNT       (ISP_BASE_ADDR+0x2050UL)
#define ISP_COMMON_AXI_STOP         (ISP_BASE_ADDR+0x2054UL)
#define ISP_COMMON_AXI_CTRL         (ISP_BASE_ADDR+0x2054UL)
#define ISP_COMMON_SLICE_CNT        (ISP_BASE_ADDR+0x2058UL)
#define ISP_COMMON_PERFORM_CNT_R    (ISP_BASE_ADDR+0x205CUL)
#define ISP_COMMON_PERFORM_CNT      (ISP_BASE_ADDR+0x2060UL)
#define ISP_COMMON_ENA_INTERRUPT    (ISP_BASE_ADDR+0x2078UL)
#define ISP_COMMON_CLR_INTERRUPT    (ISP_BASE_ADDR+0x207CUL)
#define ISP_COMMON_RAW_INTERRUPT    (ISP_BASE_ADDR+0x2080UL)
#define ISP_COMMON_INTERRUPT        (ISP_BASE_ADDR+0x2084UL)
#define ISP_COMMON_ENA_INTERRUPT1   (ISP_BASE_ADDR+0x2088UL)
#define ISP_COMMON_CLR_INTERRUPT1   (ISP_BASE_ADDR+0x208CUL)
#define ISP_COMMON_RAW_INTERRUPT1   (ISP_BASE_ADDR+0x2090UL)
#define ISP_COMMON_INTERRUPT1       (ISP_BASE_ADDR+0x2094UL)

/*isp sub block34: GLOBAL GAIN*/
#define ISP_GAIN_GLB_STATUS         (ISP_BASE_ADDR+0x2100UL)
#define ISP_GAIN_GLB_PARAM          (ISP_BASE_ADDR+0x2114UL)
#define ISP_GAIN_GLB_SLICE_SIZE     (ISP_BASE_ADDR+0x2118UL)

/*isp sub block35: RGB GAIN*/
#define ISP_GAIN_RGB_STATUS         (ISP_BASE_ADDR+0x2200UL)
#define ISP_GAIN_RGB_PARAM          (ISP_BASE_ADDR+0x2214UL)
#define ISP_GAIN_RGB_OFFSET         (ISP_BASE_ADDR+0x2218UL)

/*isp sub block36: YIQ : yiq register*/
#define ISP_YIQ_STATUS              (ISP_BASE_ADDR+0x2300UL)
#define ISP_YIQ_PARAM               (ISP_BASE_ADDR+0x2314UL)
#define ISP_YGAMMA_X0               (ISP_BASE_ADDR+0x2318UL)
#define ISP_YGAMMA_X1               (ISP_BASE_ADDR+0x231CUL)
#define ISP_YGAMMA_Y0               (ISP_BASE_ADDR+0x2320UL)
#define ISP_YGAMMA_Y1               (ISP_BASE_ADDR+0x2324UL)
#define ISP_YGAMMA_Y2               (ISP_BASE_ADDR+0x2328UL)
#define ISP_AF_V_HEIGHT             (ISP_BASE_ADDR+0x232CUL)
#define ISP_AF_LINE_COUNTER         (ISP_BASE_ADDR+0x2330UL)
#define ISP_AF_LINE_STEP            (ISP_BASE_ADDR+0x2334UL)
#define ISP_YGAMMA_NODE_IDX         (ISP_BASE_ADDR+0x2338UL)
#define ISP_AF_LINE_START           (ISP_BASE_ADDR+0x233CUL)

/*isp sub block37: HUE*/
#define ISP_HUE_STATUS              (ISP_BASE_ADDR+0x2400UL)
#define ISP_HUE_PARAM               (ISP_BASE_ADDR+0x2414UL)

/*isp sub block38: NBPC : new bad pixel correction*/
#define ISP_NBPC_STATUS             (ISP_BASE_ADDR+0x2500UL)
#define ISP_NBPC_PARAM              (ISP_BASE_ADDR+0x2514UL)
#define ISP_NBPC_CFG                (ISP_BASE_ADDR+0x2518UL)
#define ISP_NBPC_FACTOR             (ISP_BASE_ADDR+0x251CUL)
#define ISP_NBPC_COEFF              (ISP_BASE_ADDR+0x2520UL)
#define ISP_NBPC_LUT0               (ISP_BASE_ADDR+0x2524UL)
#define ISP_NBPC_LUT1               (ISP_BASE_ADDR+0x2528UL)
#define ISP_NBPC_LUT2               (ISP_BASE_ADDR+0x252CUL)
#define ISP_NBPC_LUT3               (ISP_BASE_ADDR+0x2530UL)
#define ISP_NBPC_LUT4               (ISP_BASE_ADDR+0x2534UL)
#define ISP_NBPC_LUT5               (ISP_BASE_ADDR+0x2538UL)
#define ISP_NBPC_LUT6               (ISP_BASE_ADDR+0x253CUL)
#define ISP_NBPC_LUT7               (ISP_BASE_ADDR+0x2540UL)
#define ISP_NBPC_SEL                (ISP_BASE_ADDR+0x2544UL)
#define ISP_NBPC_MAP_ADDR           (ISP_BASE_ADDR+0x2548UL)

/*isp sub block39: GAMMA2 */
#define ISP_GAMMA_NODE_RX0          (ISP_BASE_ADDR+0x2A14UL)
#define ISP_GAMMA_NODE_RX1          (ISP_BASE_ADDR+0x2A18UL)
#define ISP_GAMMA_NODE_RX2          (ISP_BASE_ADDR+0x2A1CUL)
#define ISP_GAMMA_NODE_RX3          (ISP_BASE_ADDR+0x2A20UL)
#define ISP_GAMMA_NODE_RX4          (ISP_BASE_ADDR+0x2A24UL)
#define ISP_GAMMA_NODE_RX5          (ISP_BASE_ADDR+0x2A28UL)
#define ISP_GAMMA_NODE_GX0          (ISP_BASE_ADDR+0x2A2CUL)
#define ISP_GAMMA_NODE_GX1          (ISP_BASE_ADDR+0x2A30UL)
#define ISP_GAMMA_NODE_GX2          (ISP_BASE_ADDR+0x2A34UL)
#define ISP_GAMMA_NODE_GX3          (ISP_BASE_ADDR+0x2A38UL)
#define ISP_GAMMA_NODE_GX4          (ISP_BASE_ADDR+0x2A3CUL)
#define ISP_GAMMA_NODE_GX5          (ISP_BASE_ADDR+0x2A40UL)
#define ISP_GAMMA_NODE_BX0          (ISP_BASE_ADDR+0x2A44UL)
#define ISP_GAMMA_NODE_BX1          (ISP_BASE_ADDR+0x2A48UL)
#define ISP_GAMMA_NODE_BX2          (ISP_BASE_ADDR+0x2A4CUL)
#define ISP_GAMMA_NODE_BX3          (ISP_BASE_ADDR+0x2A50UL)
#define ISP_GAMMA_NODE_BX4          (ISP_BASE_ADDR+0x2A54UL)
#define ISP_GAMMA_NODE_BX5          (ISP_BASE_ADDR+0x2A58UL)

/*isp v memory1: awbm*/
#define ISP_AWBM_OUTPUT             (ISP_BASE_ADDR+0x5000)/*end: 0x7000*/
/*#define ISP_AWBM_ITEM             1024*/

/*isp v memory2: histogram*/
#define ISP_HIST_OUTPUT             (ISP_BASE_ADDR+0x7010)/*end: 0x7410*/
/*#define ISP_HIST_ITEM             256*/

/*isp v memory3: hdr comp*/
#define ISP_HDR_COMP_OUTPUT         (ISP_BASE_ADDR+0x7500)
/*#define ISP_HDR_COMP_ITEM         64*/

/*isp v memory4: hdr p2e*/
#define ISP_HDR_P2E_OUTPUT          (ISP_BASE_ADDR+0x7700)
/*#define ISP_HDR_P2E_ITEM          32*/

/*isp v memory5: hdr e2p*/
#define ISP_HDR_E2P_OUTPUT          (ISP_BASE_ADDR+0x7800)
/*#define ISP_HDR_E2P_ITEM          32*/

#define ISP_AXI_MASTER              (ISP_BASE_ADDR+0x2000UL)
#define ISP_AXI_MASTER_STOP         (ISP_BASE_ADDR+0X2054UL)
#define ISP_INT_EN                  (ISP_BASE_ADDR+0x2078UL)
#define ISP_INT_CLEAR               (ISP_BASE_ADDR+0x207CUL)
#define ISP_INT_RAW                 (ISP_BASE_ADDR+0x2080UL)
#define ISP_INT_STATUS              (ISP_BASE_ADDR+0x2084UL)
#define ISP_REG_MAX_SIZE            SPRD_ISP_SIZE

#define ISP_AHB_BASE                SPRD_MMAHB_BASE
#define ISP_MODULE_EB               (ISP_AHB_BASE+0x0000UL)
#define ISP_MODULE_RESET            (ISP_AHB_BASE+0x0004UL)
#define ISP_CORE_CLK_EB             (ISP_AHB_BASE+0x0008UL)

/*irq line number in system*/
#define ISP_IRQ                     IRQ_ISP_INT

#define ISP_RST_LOG_BIT             BIT_2
#define ISP_RST_CFG_BIT             BIT_3

#define ISP_IRQ_HW_MASK            (0xFFFFFFFF)
#define ISP_IRQ_NUM                (32)
#define ISP_LSC_BUF_SIZE           (32 * 1024)
#define ISP_REG_BUF_SIZE           (4 * 1024)
#define ISP_RAW_AE_BUF_SIZE        (1024 * 4 * 3)
#define ISP_FRGB_GAMMA_BUF_SIZE                (0)
#define ISP_YUV_YGAMMA_BUF_SIZE            (0)
#define ISP_RAW_AWB_BUF_SIZE                (0)