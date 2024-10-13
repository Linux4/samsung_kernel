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
#include "parse_hwinfo_tshark2.h"

#define ISP_BASE_ADDR                      ISP_BASE//SPRD_ISP_BASE

/*isp sub block: Interrupt*/
#define ISP_INT_STATUS                     (ISP_BASE_ADDR+0x0000UL)
#define ISP_INT_EN0                        (ISP_BASE_ADDR+0x0014UL)
#define ISP_INT_CLR0                       (ISP_BASE_ADDR+0x0018UL)
#define ISP_INT_RAW0                       (ISP_BASE_ADDR+0x001CUL)
#define ISP_INT_INT0                       (ISP_BASE_ADDR+0x0020UL)
#define ISP_INT_EN1                        (ISP_BASE_ADDR+0x0024UL)
#define ISP_INT_CLR1                       (ISP_BASE_ADDR+0x0028UL)
#define ISP_INT_RAW1                       (ISP_BASE_ADDR+0x002CUL)
#define ISP_INT_INT1                       (ISP_BASE_ADDR+0x0030UL)
#define ISP_INT_EN2                        (ISP_BASE_ADDR+0x0034UL)
#define ISP_INT_CLR2                       (ISP_BASE_ADDR+0x0038UL)
#define ISP_INT_RAW2                       (ISP_BASE_ADDR+0x003CUL)
#define ISP_INT_INT2                       (ISP_BASE_ADDR+0x0040UL)
#define ISP_INT_EN3                        (ISP_BASE_ADDR+0x0044UL)
#define ISP_INT_CLR3                       (ISP_BASE_ADDR+0x0048UL)
#define ISP_INT_RAW3                       (ISP_BASE_ADDR+0x004CUL)
#define ISP_INT_INT3                       (ISP_BASE_ADDR+0x0050UL)
#define ISP_INT_ALL_DONE_CTRL              (ISP_BASE_ADDR+0x0054UL)

/*isp sub block: Fetch*/
#define ISP_FETCH_MEM_STATUS0              (ISP_BASE_ADDR+0x0100UL)
#define ISP_FETCH_MEM_STATUS1              (ISP_BASE_ADDR+0x0104UL)
#define ISP_FETCH_DCAM_STATUS0             (ISP_BASE_ADDR+0x0108UL)
#define ISP_FETCH_DCAM_STATUS1             (ISP_BASE_ADDR+0x010CUL)
#define ISP_FETCH_PARAM                    (ISP_BASE_ADDR+0x0114UL)
#define ISP_FETCH_START                    (ISP_BASE_ADDR+0x0118UL)
#define ISP_FETCH_SLICE_SIZE               (ISP_BASE_ADDR+0x0120UL)
#define ISP_FETCH_SLICE_Y_ADDR             (ISP_BASE_ADDR+0x0124UL)
#define ISP_FETCH_SLICE_Y_PITCH            (ISP_BASE_ADDR+0x0128UL)
#define ISP_FETCH_SLICE_U_ADDR             (ISP_BASE_ADDR+0x012CUL)
#define ISP_FETCH_SLICE_U_PITCH            (ISP_BASE_ADDR+0x0130UL)
#define ISP_FETCH_SLICE_V_ADDR             (ISP_BASE_ADDR+0x0134UL)
#define ISP_FETCH_SLICE_V_PITCH            (ISP_BASE_ADDR+0x0138UL)
#define ISP_FETCH_MIPI_WORD_INFO           (ISP_BASE_ADDR+0x013CUL)
#define ISP_FETCH_MIPI_BYTE_INFO           (ISP_BASE_ADDR+0x0140UL)
#define ISP_FETCH_BAYER_MODE               (ISP_BASE_ADDR+0x0144UL)

/*isp sub block: Store*/
#define ISP_STORE_STATUS_MEM0              (ISP_BASE_ADDR+0x0200UL)
#define ISP_STORE_STATUS_MEM1              (ISP_BASE_ADDR+0x0204UL)
#define ISP_STORE_STATUS_DCAM0             (ISP_BASE_ADDR+0x0208UL)
#define ISP_STORE_STATUS_DCAM1             (ISP_BASE_ADDR+0x020CUL)
#define ISP_STORE_PARAM                    (ISP_BASE_ADDR+0x0214UL)
#define ISP_STORE_SLICE_SIZE               (ISP_BASE_ADDR+0x0218UL)
#define ISP_STORE_BORDER                   (ISP_BASE_ADDR+0x021CUL)
#define ISP_STORE_Y_ADDR                   (ISP_BASE_ADDR+0x0220UL)
#define ISP_STORE_Y_PITCH                  (ISP_BASE_ADDR+0x0224UL)
#define ISP_STORE_U_ADDR                   (ISP_BASE_ADDR+0x0228UL)
#define ISP_STORE_U_PITCH                  (ISP_BASE_ADDR+0x022CUL)
#define ISP_STORE_V_ADDR                   (ISP_BASE_ADDR+0x0230UL)
#define ISP_STORE_V_PITCH                  (ISP_BASE_ADDR+0x0234UL)
#define ISP_STORE_SHADOW_CLR               (ISP_BASE_ADDR+0x0238UL)

/*isp sub block: dispatch(pike different from tshark2)*/
#define ISP_DISPATCH_STATUS_CH0            (ISP_BASE_ADDR+0x0300UL)
#define ISP_DISPATCH_STATUS_CH1            (ISP_BASE_ADDR+0x0304UL)
//#define ISP_DISPATCH_STATUS_COMM            (ISP_BASE_ADDR+0x0308UL)
//#define ISP_DISPATCH_STATUS1_CH0             (ISP_BASE_ADDR+0x030CUL)
//#define ISP_DISPATCH_STATUS1_CH1             (ISP_BASE_ADDR+0x0310UL)
#define ISP_DISPATCH_CH0_BAYER             (ISP_BASE_ADDR+0x0314UL)
#define ISP_DISPATCH_CH0_SIZE              (ISP_BASE_ADDR+0x0318UL)
#define ISP_DISPATCH_CH1_SIZE              (ISP_BASE_ADDR+0x031CUL)
//#define ISP_DISPATCH_DLY                                (ISP_BASE_ADDR+0x0320UL)
//#define ISP_DISPATCH_CH1_BAYER                    (ISP_BASE_ADDR+0x0324UL)
//#define ISP_DISPATCH_HW_CTRL_CH0               (ISP_BASE_ADDR+0x0328UL)
//#define ISP_DISPATCH_HW_CTRL_CH1               (ISP_BASE_ADDR+0x032CUL)

/*isp sub block: arbiter*/
#define ISP_ARBITER_WR_STATUS              (ISP_BASE_ADDR+0x0400UL)
#define ISP_ARBITER_RD_STATUS              (ISP_BASE_ADDR+0x0404UL)
#define ISP_ARBITER_PARAM                  (ISP_BASE_ADDR+0x0414UL)
#define ISP_ARBITER_ENDIAN_CH0             (ISP_BASE_ADDR+0x0418UL)
#define ISP_ARBITER_ENDIAN_CH1             (ISP_BASE_ADDR+0x041CUL)
//#define ISP_ARBITER_CTRL                                 (ISP_BASE_ADDR+0x0420UL)//pike has no this register

/*isp sub block: axi*/
#define ISP_AXI_WR_MASTER_STATUS           (ISP_BASE_ADDR+0x0500UL)
#define ISP_AXI_RD_MASTER_STATUS           (ISP_BASE_ADDR+0x0504UL)
#define ISP_AXI_ITI2AXIM_CTRL              (ISP_BASE_ADDR+0x0514UL)
//#define ISP_AXI_CONVERT_WR_CTRL            (ISP_BASE_ADDR+0x0518UL)//pike has no this register

/*isp sub block:  raw sizer(pike has no this module)*/
/*#define ISP_RAW_SIZER_STATUS0              (ISP_BASE_ADDR+0x0600UL)
#define ISP_RAW_SIZER_PARAM                (ISP_BASE_ADDR+0x0614UL)
#define ISP_RAW_SIZER_CROP_SRC             (ISP_BASE_ADDR+0x0618UL)
#define ISP_RAW_SIZER_CROP_DST             (ISP_BASE_ADDR+0x061CUL)
#define ISP_RAW_SIZER_CROP_START           (ISP_BASE_ADDR+0x0620UL)
#define ISP_RAW_SIZER_DST                  (ISP_BASE_ADDR+0x0624UL)
#define ISP_RAW_SIZER_BPC                  (ISP_BASE_ADDR+0x0628UL)
#define ISP_RAW_SIZER_HCOEFF0              (ISP_BASE_ADDR+0x062CUL)
#define ISP_RAW_SIZER_VCOEFF0              (ISP_BASE_ADDR+0x0684UL)
#define ISP_RAW_SIZER_INIT_HOR0            (ISP_BASE_ADDR+0x06B0UL)
#define ISP_RAW_SIZER_INIT_HOR1            (ISP_BASE_ADDR+0x06B4UL)
#define ISP_RAW_SIZER_INIT_HOR2            (ISP_BASE_ADDR+0x06B8UL)
#define ISP_RAW_SIZER_INIT_VER0            (ISP_BASE_ADDR+0x06BCUL)
#define ISP_RAW_SIZER_INIT_VER1            (ISP_BASE_ADDR+0x06C0UL)
#define ISP_RAW_SIZER_INIT_VER2            (ISP_BASE_ADDR+0x06C4UL)*/

/*isp sub block: common*/
#define ISP_COMMON_VERSION                 (ISP_BASE_ADDR+0x0700UL)
#define ISP_COMMON_STATUS0                 (ISP_BASE_ADDR+0x0704UL)
#define ISP_COMMON_STATUS1                 (ISP_BASE_ADDR+0x0708UL)
#define ISP_COMMON_CTRL_CH0                (ISP_BASE_ADDR+0x0714UL)
#define ISP_COMMON_SPACE_SEL               (ISP_BASE_ADDR+0x0718UL)
//#define ISP_COMMON_CTRL_CH1                (ISP_BASE_ADDR+0x071CUL)
//#define ISP_COMMON_CTRL_AWBM               (ISP_BASE_ADDR+0x0720UL)
#define ISP_COMMON_PMU_RAW_MASK            (ISP_BASE_ADDR+0x0724UL)
#define ISP_COMMON_PMU_HW_MASK             (ISP_BASE_ADDR+0x0728UL)
#define ISP_COMMON_PMU_HW_ENABLE           (ISP_BASE_ADDR+0x072CUL)
#define ISP_COMMON_PMU_SW_SWITCH           (ISP_BASE_ADDR+0x0730UL)
#define ISP_COMMON_PMU_SW_TOGGLE           (ISP_BASE_ADDR+0x0734UL)
#define ISP_COMMON_LBUF_OFFSET            (ISP_BASE_ADDR+0x0738UL)
#define ISP_SHADOW_CTRL_CH0                (ISP_BASE_ADDR+0x073CUL)
#define ISP_SHADOW_CTRL_CH1                (ISP_BASE_ADDR+0x0740UL)
//#define ISP_COMMON_3A_CTRL0                (ISP_BASE_ADDR+0x0744UL)
//#define ISP_COMMON_LBUF_OFFSET1            (ISP_BASE_ADDR+0x0748UL)
//#define ISP_COMMON_3A_CTRL1                (ISP_BASE_ADDR+0x074CUL)
#define ISP_COMMON_RESERVED0               (ISP_BASE_ADDR+0x0750UL)
#define ISP_COMMON_RESERVED1               (ISP_BASE_ADDR+0x0754UL)

/*isp sub block: Pre Global Gain*/
#define ISP_PGG_STATUS                     (ISP_BASE_ADDR+0x1000UL)
#define ISP_PGG_PARAM                      (ISP_BASE_ADDR+0x1014UL)

/*isp sub block: BLC: back level calibration*/
#define ISP_BLC_STATUS                     (ISP_BASE_ADDR+0x1100UL)
#define ISP_BLC_PARAM                      (ISP_BASE_ADDR+0x1114UL)
#define ISP_BLC_B_PARAM_R_B                (ISP_BASE_ADDR+0x1118UL)
#define ISP_BLC_B_PARAM_G                  (ISP_BASE_ADDR+0x111CUL)

/*isp sub block: RGBG*/
#define ISP_RGBG_STATUS                    (ISP_BASE_ADDR+0x1200UL)
#define ISP_RGBG_PARAM                     (ISP_BASE_ADDR+0x1214UL)
#define ISP_RGBG_RB                        (ISP_BASE_ADDR+0x1218UL)
#define ISP_RGBG_G                         (ISP_BASE_ADDR+0x121CUL)

/*isp sub block: PWD*/
#define ISP_PWD_STATUS0                    (ISP_BASE_ADDR+0x1300UL)
#define ISP_PWD_STATUS1                    (ISP_BASE_ADDR+0x1304UL)
#define ISP_PWD_PARAM                      (ISP_BASE_ADDR+0x1314UL)
#define ISP_PWD_PARAM1                     (ISP_BASE_ADDR+0x1318UL)
#define ISP_PWD_PARAM2                     (ISP_BASE_ADDR+0x131CUL)
#define ISP_PWD_PARAM3                     (ISP_BASE_ADDR+0x1320UL)
#define ISP_PWD_PARAM4                     (ISP_BASE_ADDR+0x1324UL)
#define ISP_PWD_PARAM5                     (ISP_BASE_ADDR+0x1328UL)
#define ISP_PWD_PARAM6                     (ISP_BASE_ADDR+0x132CUL)
#define ISP_PWD_PARAM7                     (ISP_BASE_ADDR+0x1330UL)
#define ISP_PWD_PARAM8                     (ISP_BASE_ADDR+0x1334UL)
#define ISP_PWD_PARAM9                     (ISP_BASE_ADDR+0x1338UL)
#define ISP_PWD_PARAM10                    (ISP_BASE_ADDR+0x133CUL)

/*isp sub block: NLC*/
#define ISP_NLC_STATUS                     (ISP_BASE_ADDR+0x1400UL)
#define ISP_NLC_PARA                       (ISP_BASE_ADDR+0x1414UL)
#define ISP_NLC_PARA_R0                    (ISP_BASE_ADDR+0x1418UL)
#define ISP_NLC_PARA_R9                    (ISP_BASE_ADDR+0x143CUL)
#define ISP_NLC_PARA_G0                    (ISP_BASE_ADDR+0x1440UL)
#define ISP_NLC_PARA_G9                    (ISP_BASE_ADDR+0x1464UL)
#define ISP_NLC_PARA_B0                    (ISP_BASE_ADDR+0x1468UL)
#define ISP_NLC_PARA_B9                    (ISP_BASE_ADDR+0x148CUL)
#define ISP_NLC_PARA_L0                    (ISP_BASE_ADDR+0x1490UL)

/*isp sub block: lens shading calibration*/
#define ISP_LENS_STATUS                    (ISP_BASE_ADDR+0x1500UL)
#define ISP_LENS_PARAM                     (ISP_BASE_ADDR+0x1514UL)
#define ISP_LENS_PARAM_ADDR                (ISP_BASE_ADDR+0x1518UL)
#define ISP_LENS_SLICE_POS                 (ISP_BASE_ADDR+0x151CUL)
#define ISP_LENS_LOAD_EB                   (ISP_BASE_ADDR+0x1520UL)
#define ISP_LENS_GRID_PITCH                (ISP_BASE_ADDR+0x1524UL)
#define ISP_LENS_GRID_SIZE                 (ISP_BASE_ADDR+0x1528UL)
#define ISP_LENS_LOAD_BUF                  (ISP_BASE_ADDR+0x152CUL)
#define ISP_LENS_MISC                      (ISP_BASE_ADDR+0x1530UL)
#define ISP_LENS_SLICE_SIZE                (ISP_BASE_ADDR+0x1534UL)
#define ISP_LENS_INIT_COOR                 (ISP_BASE_ADDR+0x1538UL)
#define ISP_LENS_Q0_VALUE                  (ISP_BASE_ADDR+0x153CUL)

/*isp sub block: binning*/
#define ISP_BINNING_STATUS                 (ISP_BASE_ADDR+0x1600UL)
#define ISP_BINNING_PARAM                  (ISP_BASE_ADDR+0x1614UL)
#define ISP_BINNING_MEM_ADDR               (ISP_BASE_ADDR+0x1618UL)
#define ISP_BINNING_PITCH                  (ISP_BASE_ADDR+0x161CUL)

/*isp sub block: AWBM*/
#define ISP_AWBM_STATUS                    (ISP_BASE_ADDR+0x1700UL)
#define ISP_AWBM_PARAM                     (ISP_BASE_ADDR+0x1714UL)
#define ISP_AWBM_BLOCK_OFFSET              (ISP_BASE_ADDR+0x1718UL)
#define ISP_AWBM_BLOCK_SIZE                (ISP_BASE_ADDR+0x171CUL)
#define ISP_AWBM_WR_0_S                    (ISP_BASE_ADDR+0x1720UL)
#define ISP_AWBM_WR_0_E                    (ISP_BASE_ADDR+0x1724UL)
#define ISP_AWBM_NW_0_XYR                  (ISP_BASE_ADDR+0x1748UL)
#define ISP_AWBM_CLCTOR_0_S                (ISP_BASE_ADDR+0x175CUL)
#define ISP_AWBM_CLCTOR_0_E                (ISP_BASE_ADDR+0x1760UL)
#define ISP_AWBM_CLCTR_0_PIXEL_NUM         (ISP_BASE_ADDR+0x1784UL)
#define ISP_AWBM_MEM_ADDR                  (ISP_BASE_ADDR+0x1798UL)
#define ISP_AWBM_POSITION_SEL              (ISP_BASE_ADDR+0x179CUL)
#define ISP_AWBM_THR_VALUE1                (ISP_BASE_ADDR+0x17A0UL)
#define ISP_AWBM_THR_VALUE2                (ISP_BASE_ADDR+0x17A4UL)

/*isp sub block: AWBC*/
#define ISP_AWBC_STATUS                    (ISP_BASE_ADDR+0x1800UL)
#define ISP_AWBC_PARAM                     (ISP_BASE_ADDR+0x1814UL)
#define ISP_AWBC_GAIN0                     (ISP_BASE_ADDR+0x1818UL)
#define ISP_AWBC_GAIN1                     (ISP_BASE_ADDR+0x181CUL)
#define ISP_AWBC_THRD                      (ISP_BASE_ADDR+0x1820UL)
#define ISP_AWBC_OFFSET0                   (ISP_BASE_ADDR+0x1824UL)
#define ISP_AWBC_OFFSET1                   (ISP_BASE_ADDR+0x1828UL)
#define ISP_AWBC_GAIN0_BUF                 (ISP_BASE_ADDR+0x182CUL)
#define ISP_AWBC_GAIN1_BUF                 (ISP_BASE_ADDR+0x1830UL)
#define ISP_AWBC_OFFSET0_BUF               (ISP_BASE_ADDR+0x1834UL)
#define ISP_AWBC_OFFSET1_BUF               (ISP_BASE_ADDR+0x1838UL)
#define ISP_AWBC_GAIN0_READ                (ISP_BASE_ADDR+0x183CUL)
#define ISP_AWBC_GAIN1_READ                (ISP_BASE_ADDR+0x1840UL)
#define ISP_AWBC_OFFSET0_READ              (ISP_BASE_ADDR+0x1844UL)
#define ISP_AWBC_OFFSET1_READ              (ISP_BASE_ADDR+0x1848UL)

/*isp sub block: AEM*/
#define ISP_AEM_STATUS                     (ISP_BASE_ADDR+0x1900UL)
#define ISP_AEM_PARAM                      (ISP_BASE_ADDR+0x1914UL)
#define ISP_AEM_OFFSET                     (ISP_BASE_ADDR+0x1918UL)
#define ISP_AEM_BLK_SIZE                   (ISP_BASE_ADDR+0x191CUL)
#define ISP_AEM_SLICE_SIZE                 (ISP_BASE_ADDR+0x1920UL)

/*isp sub block: BPC: bad pixel correction*/
#define ISP_BPC_STATUS                     (ISP_BASE_ADDR+0x1A00UL)
#define ISP_BPC_PARAM                      (ISP_BASE_ADDR+0x1A14UL)
#define ISP_BPC_CFG                        (ISP_BASE_ADDR+0x1A18UL)
#define ISP_BPC_FACTOR                     (ISP_BASE_ADDR+0x1A1CUL)
#define ISP_BPC_COEFF                      (ISP_BASE_ADDR+0x1A20UL)
#define ISP_BPC_LUTWORD0                   (ISP_BASE_ADDR+0x1A24UL)
#define ISP_BPC_NEW_OLD_SEL                (ISP_BASE_ADDR+0x1A44UL)
#define ISP_BPC_MAP_ADDR                   (ISP_BASE_ADDR+0x1A48UL)

/*isp sub block: GRGB*/
#define ISP_GRGB_STATUS                    (ISP_BASE_ADDR+0x1B00UL)
#define ISP_GRGB_PARAM                     (ISP_BASE_ADDR+0x1B14UL)
#define ISP_GRGB_GRID                      (ISP_BASE_ADDR+0x1B18UL)

/*isp sub block: BDN*/
#define ISP_BDN_STATUS                     (ISP_BASE_ADDR+0x1C00UL)
#define ISP_BDN_PARAM                      (ISP_BASE_ADDR+0x1C14UL)
#define ISP_BDN_DISWEI0_0                  (ISP_BASE_ADDR+0x1C18UL)
#define ISP_BDN_DISWEI0_1                  (ISP_BASE_ADDR+0x1C1CUL)
#define ISP_BDN_RANWEI0_0                  (ISP_BASE_ADDR+0x1C20UL)
#define ISP_BDN_RANWEI0_7                  (ISP_BASE_ADDR+0x1C3CUL)
#define ISP_BDN_1DLNC_CENTER               (ISP_BASE_ADDR+0x1DA8UL)
#define ISP_BDN_1DLNC_SQUARE_X             (ISP_BASE_ADDR+0x1DACUL)
#define ISP_BDN_1DLNC_SQUARE_Y             (ISP_BASE_ADDR+0x1DB0UL)
#define ISP_BDN_1DLNC_P                    (ISP_BASE_ADDR+0x1DB4UL)
#define ISP_BDN_1DLNC_SIZE                 (ISP_BASE_ADDR+0x1DB8UL)
#define ISP_BDN_1DLNC_POS                  (ISP_BASE_ADDR+0x1DBCUL)
#define	ISP_BDN_1DLNC_OFFSET               (ISP_BASE_ADDR+0x1DC0UL)

/*isp sub block: RGBG2*/
#define ISP_RGBG2_STATUS                   (ISP_BASE_ADDR+0x1E00UL)
#define ISP_RGBG2_PARAM                    (ISP_BASE_ADDR+0x1E14UL)
#define ISP_RGBG2_OFFSET                   (ISP_BASE_ADDR+0x1E18UL)

/*isp sub block: 1d lnc*/
#define ISP_1D_LNC_STATUS                  (ISP_BASE_ADDR+0x1F00UL)
#define ISP_1D_LNC_PARAM                   (ISP_BASE_ADDR+0x1F14UL)
#define ISP_1D_LNC_PARAM1                  (ISP_BASE_ADDR+0x1F18UL)
#define ISP_1D_LNC_PARAM2                  (ISP_BASE_ADDR+0x1F1CUL)
#define ISP_1D_LNC_PARAM3                  (ISP_BASE_ADDR+0x1F20UL)
#define ISP_1D_LNC_PARAM4                  (ISP_BASE_ADDR+0x1F24UL)
#define ISP_1D_LNC_PARAM5                  (ISP_BASE_ADDR+0x1F28UL)
#define ISP_1D_LNC_PARAM6                  (ISP_BASE_ADDR+0x1F2CUL)
#define ISP_1D_LNC_PARAM7                  (ISP_BASE_ADDR+0x1F30UL)
#define ISP_1D_LNC_PARAM8                  (ISP_BASE_ADDR+0x1F34UL)
#define ISP_1D_LNC_PARAM9                  (ISP_BASE_ADDR+0x1F38UL)
#define ISP_1D_LNC_PARAM10                 (ISP_BASE_ADDR+0x1F3CUL)
#define ISP_1D_LNC_PARAM11                 (ISP_BASE_ADDR+0x1F40UL)
#define ISP_1D_LNC_PARAM12                 (ISP_BASE_ADDR+0x1F44UL)
#define ISP_1D_LNC_PARAM13                 (ISP_BASE_ADDR+0x1F48UL)
#define ISP_1D_LNC_PARAM14                 (ISP_BASE_ADDR+0x1F4CUL)
#define ISP_1D_LNC_PARAM15                 (ISP_BASE_ADDR+0x1F50UL)
#define ISP_1D_LNC_PARAM16                 (ISP_BASE_ADDR+0x1F54UL)
#define ISP_1D_LNC_PARAM17                 (ISP_BASE_ADDR+0x1F58UL)
#define ISP_1D_LNC_PARAM18                 (ISP_BASE_ADDR+0x1F5CUL)

/*isp sub block: NLM VST IVST*/
#define ISP_VST_PARA                       (ISP_BASE_ADDR+0x2014UL)
#define ISP_IVST_PARA                      (ISP_BASE_ADDR+0x2214UL)
#define ISP_NLM_LB_STATUS0                 (ISP_BASE_ADDR+0x2100UL)
#define ISP_NLM_LB_STATUS1                 (ISP_BASE_ADDR+0x2104UL)
#define ISP_NLM_STATUS                     (ISP_BASE_ADDR+0x2108UL)
#define ISP_NLM_PARA                       (ISP_BASE_ADDR+0x2114UL)
#define ISP_NLM_FLAT_PARA_0                (ISP_BASE_ADDR+0x2118UL)
#define	ISP_NLM_STERNGTH                   (ISP_BASE_ADDR+0x212CUL)
#define ISP_NLM_IS_FLAT                    (ISP_BASE_ADDR+0x2130UL)
#define ISP_NLM_ADD_BACK                   (ISP_BASE_ADDR+0x2134UL)
//#define ISP_NLM_DIRECTION_0                (ISP_BASE_ADDR+0x2138UL)
//#define ISP_NLM_DIRECTION_1                (ISP_BASE_ADDR+0x213CUL)
#define ISP_NLM_LUT_W_0                    (ISP_BASE_ADDR+0x2140UL)

/*isp sub block: CFA: clolor filter array*/
#define ISP_CFAE_STATUS                    (ISP_BASE_ADDR+0x3000UL)
#define ISP_CFAE_EE_CFG0                   (ISP_BASE_ADDR+0x3014UL)
#define ISP_CFAE_THRD_0                    (ISP_BASE_ADDR+0x3018UL)
#define ISP_CFAE_THRD_1                    (ISP_BASE_ADDR+0x301CUL)
#define ISP_CFAE_THRD_2                    (ISP_BASE_ADDR+0x3020UL)
#define ISP_CFAE_THRD_3                    (ISP_BASE_ADDR+0x3024UL)
#define ISP_CFAE_THRD_4                    (ISP_BASE_ADDR+0x3028UL)
#define ISP_CFAE_EE_CFG1                   (ISP_BASE_ADDR+0x302CUL)

/*isp sub block:CMC: Color matrix correction for 10 bit   */
#define ISP_CMC10_STATUS0                  (ISP_BASE_ADDR+0x3100UL)
#define ISP_CMC10_STATUS1                  (ISP_BASE_ADDR+0x3104UL)
#define ISP_CMC10_PARAM                    (ISP_BASE_ADDR+0x3114UL)
#define ISP_CMC10_MATRIX0                  (ISP_BASE_ADDR+0x3118UL)
#define ISP_CMC10_MATRIX0_BUF              (ISP_BASE_ADDR+0x312CUL)

/*isp sub block: FRGB WDR */
#define ISP_HDR_STATUS                (ISP_BASE_ADDR+0x3200UL)
#define ISP_HDR_PARAM                 (ISP_BASE_ADDR+0x3214UL)

//#define ISP_HDR_STATUS              (ISP_BASE_ADDR+0x1A00UL)
//#define ISP_HDR_PARAM               (ISP_BASE_ADDR+0x1A14UL)
//#define ISP_HDR_INDEX               (ISP_BASE_ADDR+0x1A18UL)

/*isp sub block: GAMMA */
#define ISP_GAMMA_STATUS                   (ISP_BASE_ADDR+0x3300UL)
#define ISP_GAMMA_PARAM                    (ISP_BASE_ADDR+0x3314UL)

/*isp sub block:CMC: Color matrix correction for 8 bit  */
#define ISP_CMC8_STATUS0                   (ISP_BASE_ADDR+0x3500UL)
#define ISP_CMC8_STATUS1                   (ISP_BASE_ADDR+0x3504UL)
#define ISP_CMC8_PARAM                     (ISP_BASE_ADDR+0x3514UL)
#define ISP_CMC8_MATRIX0                   (ISP_BASE_ADDR+0x3518UL)
#define ISP_CMC8_MATRIX0_BUF               (ISP_BASE_ADDR+0x352CUL)

/*CT: Color transformation*/
#define ISP_CT_STATUS0                     (ISP_BASE_ADDR+0x3600UL)
#define ISP_CT_STATUS1                     (ISP_BASE_ADDR+0x3604UL)
#define ISP_CT_PARAM                       (ISP_BASE_ADDR+0x3614UL)

/*isp sub block: CCE: clolor conversion enhancement*/
#define ISP_CCE_STATUS0                    (ISP_BASE_ADDR+0x3700UL)
#define ISP_CCE_STATUS1                    (ISP_BASE_ADDR+0x3704UL)
#define ISP_CCE_PARAM                      (ISP_BASE_ADDR+0x3714UL)
#define ISP_CCE_MATRIX0                    (ISP_BASE_ADDR+0x3718UL)
#define ISP_CCE_MATRIX1                    (ISP_BASE_ADDR+0x371CUL)
#define ISP_CCE_MATRIX2                    (ISP_BASE_ADDR+0x3720UL)
#define ISP_CCE_MATRIX3                    (ISP_BASE_ADDR+0x3724UL)
#define ISP_CCE_MATRIX4                    (ISP_BASE_ADDR+0x3728UL)
#define ISP_CCE_SHIFT                      (ISP_BASE_ADDR+0x372CUL)
#define ISP_CCE_UVD_PARAM0                 (ISP_BASE_ADDR+0x3730UL)
#define ISP_CCE_UVD_PARAM1                 (ISP_BASE_ADDR+0x3734UL)
#define ISP_CCE_UVD_PARAM2                 (ISP_BASE_ADDR+0x3738UL)
#define ISP_CCE_UVD_PARAM3                 (ISP_BASE_ADDR+0x373CUL)
#define ISP_CCE_UVD_PARAM4                 (ISP_BASE_ADDR+0x3740UL)
#define ISP_CCE_UVD_PARAM5                 (ISP_BASE_ADDR+0x3744UL)

/*isp sub block:HSV*/
#define ISP_HSV_STATUS                     (ISP_BASE_ADDR+0x3800UL)
#define ISP_HSV_PARAM                      (ISP_BASE_ADDR+0x3814UL)

/*isp sub block:Radial CSC*/
#define ISP_CSC_STATUS                     (ISP_BASE_ADDR + 0x3900UL)
#define ISP_CSC_CTRL                       (ISP_BASE_ADDR + 0x3914UL)
#define ISP_CSC_RCC                        (ISP_BASE_ADDR + 0x3918UL)
#define ISP_CSC_BCC                        (ISP_BASE_ADDR + 0x391CUL)
#define ISP_CSC_REDX2INIT                  (ISP_BASE_ADDR + 0x3920UL)
#define ISP_CSC_REDY2INIT                  (ISP_BASE_ADDR + 0x3924UL)
#define ISP_CSC_BLUEX2INIT                 (ISP_BASE_ADDR + 0x3928UL)
#define ISP_CSC_BLUEY2INIT                 (ISP_BASE_ADDR + 0x392CUL)
#define ISP_CSC_REDTHR                     (ISP_BASE_ADDR + 0x3930UL)
#define ISP_CSC_BLUETHR                    (ISP_BASE_ADDR + 0x3934UL)
#define ISP_CSC_REDPOLY                    (ISP_BASE_ADDR + 0x3938UL)
#define ISP_CSC_BLUEPOLY                   (ISP_BASE_ADDR + 0x393CUL)
#define ISP_CSC_MAXGAIN                    (ISP_BASE_ADDR + 0x3940UL)
#define ISP_CSC_PICSIZE                    (ISP_BASE_ADDR + 0x3944UL)
#define ISP_CSC_START_ROW_COL              (ISP_BASE_ADDR + 0x3948UL)

/*isp sub block:ISP_PRECNRNEW*/
#define ISP_PRECNRNEW_STATUS0              (ISP_BASE_ADDR+0x3A00UL)
#define ISP_PRECNRNEW_STATUS1              (ISP_BASE_ADDR+0x3A04UL)
#define ISP_PRECNRNEW_CFG                  (ISP_BASE_ADDR+0x3A14UL)
#define ISP_PRECNRNEW_MEDIAN_THR           (ISP_BASE_ADDR+0x3A18UL)
#define ISP_PRECNRNEW_THRU                 (ISP_BASE_ADDR+0x3A1CUL)
#define ISP_PRECNRNEW_THRV                 (ISP_BASE_ADDR+0x3A20UL)

/*isp sub block:ISP_PSTRZ*/
#define ISP_PSTRZ_STATUS                   (ISP_BASE_ADDR+0x3B00UL)
#define ISP_PSTRZ_PARAM                    (ISP_BASE_ADDR+0x3B14UL)
#define ISP_PSTRZ_LEVEL0                   (ISP_BASE_ADDR+0x3B18UL)
#define ISP_PSTRZ_LEVEL1                   (ISP_BASE_ADDR+0x3B1CUL)
#define ISP_PSTRZ_LEVEL2                   (ISP_BASE_ADDR+0x3B20UL)
#define ISP_PSTRZ_LEVEL3                   (ISP_BASE_ADDR+0x3B24UL)
#define ISP_PSTRZ_LEVEL4                   (ISP_BASE_ADDR+0x3B28UL)
#define ISP_PSTRZ_LEVEL5                   (ISP_BASE_ADDR+0x3B2CUL)
#define ISP_PSTRZ_LEVEL6                   (ISP_BASE_ADDR+0x3B30UL)
#define ISP_PSTRZ_LEVEL7                   (ISP_BASE_ADDR+0x3B34UL)

/*isp sub block: AFM : auto focus monitor*/
#define ISP_RGB_AFM_STATUS0                (ISP_BASE_ADDR+0x3C00UL)
#define ISP_RGB_AFM_STATUS1                (ISP_BASE_ADDR+0x3C04UL)
#define ISP_RGB_AFM_PARAM                  (ISP_BASE_ADDR+0x3C14UL)
#define ISP_RGB_AFM_FRAME_RANGE            (ISP_BASE_ADDR+0x3C18UL)
#define ISP_RGB_AFM_SOBEL_THRESHOLD        (ISP_BASE_ADDR+0x3C1CUL)
#define ISP_RGB_AFM_SPSMD_THRESHOLD_MAX    (ISP_BASE_ADDR+0x3C20UL)
#define ISP_RGB_AFM_WIN_RANGE0             (ISP_BASE_ADDR+0x3C24UL)
#define ISP_RGB_AFM_TYPE1_VAL00            (ISP_BASE_ADDR+0x3CECUL)
#define ISP_RGB_AFM_TYPE2_VAL00            (ISP_BASE_ADDR+0x3D50UL)
#define ISP_RGB_AFM_SPSMD_THRESHOLD_MIN    (ISP_BASE_ADDR+0x3DB4UL)

/*isp sub block:RGB2Y*/
#define ISP_Y_RGB2Y_STATUS0                (ISP_BASE_ADDR+0x4000UL)
#define ISP_Y_RGB2Y_STATUS1                (ISP_BASE_ADDR+0x4004UL)
#define ISP_Y_RGB2Y_PARAM                  (ISP_BASE_ADDR+0x4014UL)

/*isp sub block:YIQ AEM*/
#define ISP_YIQ_AEM_STATUS                 (ISP_BASE_ADDR + 0x4100UL)
#define ISP_YIQ_AEM_PARAM                  (ISP_BASE_ADDR + 0x4114UL)
#define ISP_YIQ_YGAMMA_NODE_X0             (ISP_BASE_ADDR + 0x4118UL)
#define ISP_YIQ_YGAMMA_NODE_X1             (ISP_BASE_ADDR + 0x411CUL)
#define ISP_YIQ_YGAMMA_NODE_Y0             (ISP_BASE_ADDR + 0x4120UL)
#define ISP_YIQ_YGAMMA_NODE_Y1             (ISP_BASE_ADDR + 0x4124UL)
#define ISP_YIQ_YGAMMA_NODE_Y2             (ISP_BASE_ADDR + 0x4128UL)
#define ISP_YIQ_YGAMMA_NODE_IDX            (ISP_BASE_ADDR + 0x412CUL)
#define ISP_YIQ_AEM_OFFSET                 (ISP_BASE_ADDR + 0x4130UL)
#define ISP_YIQ_AEM_BLK_SIZE               (ISP_BASE_ADDR + 0x4134UL)
#define ISP_YIQ_AEM_SLICE_SIZE             (ISP_BASE_ADDR + 0x4138UL)

/*isp sub block:ANTI FLICKER*/
#define ISP_ANTI_FLICKER_STATUS0           (ISP_BASE_ADDR + 0x4200UL)
#define ISP_ANTI_FLICKER_STATUS1           (ISP_BASE_ADDR + 0x4204UL)
#define ISP_ANTI_FLICKER_PARAM0            (ISP_BASE_ADDR + 0x4214UL)
#define ISP_ANTI_FLICKER_PARAM1            (ISP_BASE_ADDR + 0x4218UL)
#define ISP_ANTI_FLICKER_COL_POS           (ISP_BASE_ADDR + 0x421CUL)
#define ISP_ANTI_FLICKER_DDR_INIT_ADDR     (ISP_BASE_ADDR + 0x4220UL)

/*isp sub block:YIQ AFM*/
#define ISP_YIQ_AFM_STATUS0                (ISP_BASE_ADDR + 0x4300UL)
#define ISP_YIQ_AFM_STATUS1                (ISP_BASE_ADDR + 0x4304UL)
#define ISP_YIQ_AFM_PARAM                  (ISP_BASE_ADDR + 0x4314UL)
#define ISP_YIQ_AFM_WIN_RANGE0             (ISP_BASE_ADDR + 0x4318UL)
#define ISP_YIQ_AFM_COEF0_1                (ISP_BASE_ADDR + 0x43E0UL)
#define ISP_YIQ_AFM_COEF2_3                (ISP_BASE_ADDR + 0x43E4UL)
#define ISP_YIQ_AFM_COEF4_5                (ISP_BASE_ADDR + 0x43E8UL)
#define ISP_YIQ_AFM_COEF6_7                (ISP_BASE_ADDR + 0x43ECUL)
#define ISP_YIQ_AFM_COEF8_9                (ISP_BASE_ADDR + 0x43F0UL)
#define ISP_YIQ_AFM_COEF10                 (ISP_BASE_ADDR + 0x43F4UL)
#define ISP_YIQ_AFM_LAPLACE0               (ISP_BASE_ADDR + 0x43F8UL)
#define ISP_YIQ_AFM_SLICE_SIZE             (ISP_BASE_ADDR + 0x4588UL)

/*isp sub block: Pre-CDN*/
#define ISP_PRECDN_STATUS0                 (ISP_BASE_ADDR+0x5000UL)
#define ISP_PRECDN_STATUS1                 (ISP_BASE_ADDR+0x5004UL)
#define ISP_PRECDN_PARAM                   (ISP_BASE_ADDR+0x5014UL)
#define ISP_PRECDN_MEDIAN_THRUV01          (ISP_BASE_ADDR+0x5018UL)
#define ISP_PRECDN_THRYUV                  (ISP_BASE_ADDR+0x501CUL)
#define ISP_PRECDN_SEGU_0                  (ISP_BASE_ADDR+0x5020UL)
#define ISP_PRECDN_SEGU_3                  (ISP_BASE_ADDR+0x502CUL)
#define ISP_PRECDN_SEGV_0                  (ISP_BASE_ADDR+0x5030UL)
#define ISP_PRECDN_SEGV_3                  (ISP_BASE_ADDR+0x503CUL)
#define ISP_PRECDN_SEGY_0                  (ISP_BASE_ADDR+0x5040UL)
#define ISP_PRECDN_SEGY_3                  (ISP_BASE_ADDR+0x504CUL)
#define ISP_PRECDN_DISTW0                  (ISP_BASE_ADDR+0x5050UL)
#define ISP_PRECDN_DISTW3                  (ISP_BASE_ADDR+0x505CUL)

/*isp sub block: Pre-Filter*/
#define ISP_PREF_STATUS0                   (ISP_BASE_ADDR+0x5100UL)
#define ISP_PREF_STATUS1                   (ISP_BASE_ADDR+0x5104UL)
#define ISP_PREF_PARAM                     (ISP_BASE_ADDR+0x5114UL)
#define ISP_PREF_THRD                      (ISP_BASE_ADDR+0x5118UL)

/*isp sub block: UV Pre-Filter*/
#define ISP_UV_PREF_STATUS0                (ISP_BASE_ADDR+0x5200UL)
#define ISP_UV_PREF_STATUS1                (ISP_BASE_ADDR+0x5204UL)
#define ISP_UV_PREF_PARAM                  (ISP_BASE_ADDR+0x5214UL)
#define ISP_UV_PREF_THRD                   (ISP_BASE_ADDR+0x5218UL)

/*isp sub block: Brightness*/
#define ISP_BRIGHT_STATUS                  (ISP_BASE_ADDR+0x5300UL)
#define ISP_BRIGHT_PARAM                   (ISP_BASE_ADDR+0x5314UL)

/*isp sub block: Contrast*/
#define ISP_CONTRAST_STATUS                (ISP_BASE_ADDR+0x5400UL)
#define ISP_CONTRAST_PARAM                 (ISP_BASE_ADDR+0x5414UL)

/*isp sub block: HIST : histogram*/
#define ISP_HIST_STATUS                    (ISP_BASE_ADDR+0x5500UL)
#define ISP_HIST_PARAM                     (ISP_BASE_ADDR+0x5514UL)
#define ISP_HIST_RATIO                     (ISP_BASE_ADDR+0x5518UL)
#define ISP_HIST_MAX_MIN                   (ISP_BASE_ADDR+0x551CUL)
#define ISP_HIST_SLICE_SIZE                (ISP_BASE_ADDR+0x5520UL)
#define ISP_HIST_ADJUST                    (ISP_BASE_ADDR+0x5524UL)

/*isp sub block: HIST2*/
#define ISP_HIST2_STATUS                   (ISP_BASE_ADDR+0x5600UL)
#define ISP_HIST2_PARAM                    (ISP_BASE_ADDR+0x5614UL)
#define ISP_HIST2_ROI_S0                   (ISP_BASE_ADDR+0x5618UL)
#define ISP_HIST2_ROI_E0                   (ISP_BASE_ADDR+0x561CUL)

/*isp sub block: AUTOCONT : auto contrast adjustment*/
#define ISP_AUTOCONT_STATUS                (ISP_BASE_ADDR+0x5700UL)
#define ISP_AUTOCONT_PARAM                 (ISP_BASE_ADDR+0x5714UL)
#define ISP_AUTOCONT_MAX_MIN               (ISP_BASE_ADDR+0x5718UL)

/*isp sub block: cdn*/
#define ISP_CDN_STATUS0                    (ISP_BASE_ADDR+0x5800UL)
#define ISP_CDN_STATUS1                    (ISP_BASE_ADDR+0x5804UL)
#define ISP_CDN_PARAM                      (ISP_BASE_ADDR+0x5814UL)
#define ISP_CDN_THRUV                      (ISP_BASE_ADDR+0x5818UL)
#define ISP_CDN_U_RANWEI_0                 (ISP_BASE_ADDR+0x581CUL)
#define ISP_CDN_U_RANWEI_7                 (ISP_BASE_ADDR+0x5838UL)
#define ISP_CDN_V_RANWEI_0                 (ISP_BASE_ADDR+0x583CUL)
#define ISP_CDN_V_RANWEI_7                 (ISP_BASE_ADDR+0x5858UL)

/*isp sub block: edge*/
#define ISP_EE_STATUS                      (ISP_BASE_ADDR+0x5900UL)
#define ISP_EE_PARAM                       (ISP_BASE_ADDR+0x5914UL)
#define ISP_EE_CFG0                        (ISP_BASE_ADDR+0x5918UL)
#define ISP_EE_CFG1                        (ISP_BASE_ADDR+0x591CUL)
#define ISP_EE_CFG2                        (ISP_BASE_ADDR+0x5920UL)
#define ISP_EE_CFG3                        (ISP_BASE_ADDR+0x5924UL)
#define ISP_EE_CFG4                        (ISP_BASE_ADDR+0x5928UL)
#define ISP_EE_CFG5                        (ISP_BASE_ADDR+0x592CUL)
#define ISP_EE_CFG6                        (ISP_BASE_ADDR+0x5930UL)
#define ISP_EE_CFG7                        (ISP_BASE_ADDR+0x5934UL)
//pike has no registers ISP_EE_ADP_CFG0 ~ ISP_EE_ADP_CFG2
//#define ISP_EE_ADP_CFG0                    (ISP_BASE_ADDR+0x5938UL)
//#define ISP_EE_ADP_CFG1                    (ISP_BASE_ADDR+0x593CUL)
//#define ISP_EE_ADP_CFG2                    (ISP_BASE_ADDR+0x5940UL)

/*isp sub block: emboss(pike has no this module)*/
#define ISP_EMBOSS_Y_STATUS                (ISP_BASE_ADDR+0x5A00UL)
#define ISP_EMBOSS_Y_PARAM                 (ISP_BASE_ADDR+0x5A14UL)
#define ISP_EMBOSS_UV_STATUS               (ISP_BASE_ADDR+0x5F00UL)
#define ISP_EMBOSS_UV_PARAM                (ISP_BASE_ADDR+0x5F14UL)

/*isp sub block: CSS*/
#define ISP_CSS_STATUS                     (ISP_BASE_ADDR+0x5B00UL)
#define ISP_CSS_PARAM                      (ISP_BASE_ADDR+0x5B14UL)
#define ISP_CSS_THRD0                      (ISP_BASE_ADDR+0x5B18UL)
#define ISP_CSS_LH_RATIO                   (ISP_BASE_ADDR+0x5B1CUL)
#define ISP_CSS_EX_TH_0                    (ISP_BASE_ADDR+0x5B20UL)
#define ISP_CSS_EX_TH_1                    (ISP_BASE_ADDR+0x5B24UL)
#define ISP_CSS_CHROM_L_TH_0               (ISP_BASE_ADDR+0x5B28UL)
#define ISP_CSS_CHROM_L_TH_1               (ISP_BASE_ADDR+0x5B2CUL)
#define ISP_CSS_CHROM_H_TH_0               (ISP_BASE_ADDR+0x5B30UL)
#define ISP_CSS_CHROM_H_TH_1               (ISP_BASE_ADDR+0x5B34UL)
#define ISP_CSS_RATIO                      (ISP_BASE_ADDR+0x5B38UL)

/*isp sub block: csa*/
#define ISP_CSA_STATUS                     (ISP_BASE_ADDR+0x5C00UL)
#define ISP_CSA_PARAM                      (ISP_BASE_ADDR+0x5C14UL)

/*isp sub block: hua*/
#define ISP_HUA_STATUS                     (ISP_BASE_ADDR+0x5D00UL)
#define ISP_HUA_PARAM                      (ISP_BASE_ADDR+0x5D14UL)

/*isp sub block: post-cdn*/
#define ISP_POSTCDN_STATUS                 (ISP_BASE_ADDR+0x5E00UL)
#define ISP_POSTCDN_COMMON_CTRL            (ISP_BASE_ADDR+0x5E14UL)
#define ISP_POSTCDN_ADPT_THR               (ISP_BASE_ADDR+0x5E18UL)
#define ISP_POSTCDN_UVTHR                  (ISP_BASE_ADDR+0x5E1CUL)
#define ISP_POSTCDN_THRU                   (ISP_BASE_ADDR+0x5E20UL)
#define ISP_POSTCDN_THRV                   (ISP_BASE_ADDR+0x5E24UL)
#define ISP_POSTCDN_RSEGU01                (ISP_BASE_ADDR+0x5E28UL)
#define ISP_POSTCDN_RSEGU23                (ISP_BASE_ADDR+0x5E2CUL)
#define ISP_POSTCDN_RSEGU45                (ISP_BASE_ADDR+0x5E30UL)
#define ISP_POSTCDN_RSEGU6                 (ISP_BASE_ADDR+0x5E34UL)
#define ISP_POSTCDN_RSEGV01                (ISP_BASE_ADDR+0x5E38UL)
#define ISP_POSTCDN_RSEGV23                (ISP_BASE_ADDR+0x5E3CUL)
#define ISP_POSTCDN_RSEGV45                (ISP_BASE_ADDR+0x5E40UL)
#define ISP_POSTCDN_RSEGV6                 (ISP_BASE_ADDR+0x5E44UL)
#define ISP_POSTCDN_R_DISTW0               (ISP_BASE_ADDR+0x5E48UL)
#define ISP_POSTCDN_START_ROW_MOD4         (ISP_BASE_ADDR+0x5E84UL)

/*isp sub block: ygamma*/
#define ISP_YGAMMA_STATUS                  (ISP_BASE_ADDR+0x6000UL)
#define ISP_YGAMMA_PARAM                   (ISP_BASE_ADDR+0x6014UL)

/*isp sub block: ydelay*/
#define ISP_YDELAY_STATUS                  (ISP_BASE_ADDR+0x6100UL)
#define ISP_YDELAY_PARAM                   (ISP_BASE_ADDR+0x6114UL)

/*isp sub block: yuv nlm*/
#define ISP_YNLM_STATUS0                   (ISP_BASE_ADDR+0x6200UL)
#define ISP_YNLM_STATUS1                   (ISP_BASE_ADDR+0x6204UL)
#define ISP_YNLM_PARAM                     (ISP_BASE_ADDR+0x6214UL)
#define ISP_YNLM_ADAPT_PARAM1              (ISP_BASE_ADDR+0x6218UL)
#define ISP_YNLM_ADAPT_PARAM2              (ISP_BASE_ADDR+0x621CUL)
#define ISP_YNLM_ADAPT_PARAM3              (ISP_BASE_ADDR+0x6220UL)
#define ISP_YNLM_ADAPT_PARAM4              (ISP_BASE_ADDR+0x6224UL)
#define ISP_YNLM_ADAPT_PARAM5              (ISP_BASE_ADDR+0x6228UL)
#define ISP_YNLM_ADAPT_PARAM6              (ISP_BASE_ADDR+0x622CUL)
#define ISP_YNLM_ADAPT_PARAM7              (ISP_BASE_ADDR+0x6230UL)
#define ISP_YNLM_ADAPT_PARAM8              (ISP_BASE_ADDR+0x6234UL)
#define ISP_YNLM_DEN_STERNGTH              (ISP_BASE_ADDR+0x6238UL)
#define ISP_YNLM_CENTER                    (ISP_BASE_ADDR+0x623CUL)
#define ISP_YNLM_CENTER_X2                 (ISP_BASE_ADDR+0x6240UL)
#define ISP_YNLM_CENTER_Y2                 (ISP_BASE_ADDR+0x6244UL)
#define ISP_YNLM_RADIUS                    (ISP_BASE_ADDR+0x6248UL)
#define ISP_YNLM_RADIUS_RANGE              (ISP_BASE_ADDR+0x624CUL)
#define ISP_YNLM_GAIN_MAX                  (ISP_BASE_ADDR+0x6250UL)
#define ISP_YNLM_IMG_PARAM0                (ISP_BASE_ADDR+0x6254UL)
#define ISP_YNLM_IMG_PARAM1                (ISP_BASE_ADDR+0x6258UL)
#define ISP_YNLM_ADD_BACK                  (ISP_BASE_ADDR+0x625CUL)
#define ISP_YNLM_LUT_W_0                   (ISP_BASE_ADDR+0x6260UL)
#define ISP_YNLM_LUT_W_1                   (ISP_BASE_ADDR+0x6264UL)
#define ISP_YNLM_LUT_W_2                   (ISP_BASE_ADDR+0x6268UL)
#define ISP_YNLM_LUT_W_3                   (ISP_BASE_ADDR+0x626CUL)
#define ISP_YNLM_LUT_W_4                   (ISP_BASE_ADDR+0x6270UL)
#define ISP_YNLM_LUT_W_5                   (ISP_BASE_ADDR+0x6274UL)
#define ISP_YNLM_LUT_W_6                   (ISP_BASE_ADDR+0x6278UL)
#define ISP_YNLM_LUT_W_7                   (ISP_BASE_ADDR+0x627CUL)
#define ISP_YNLM_LUT_W_8                   (ISP_BASE_ADDR+0x6280UL)
#define ISP_YNLM_LUT_W_9                   (ISP_BASE_ADDR+0x6284UL)
#define ISP_YNLM_LUT_W_10                  (ISP_BASE_ADDR+0x6288UL)
#define ISP_YNLM_LUT_W_11                  (ISP_BASE_ADDR+0x628CUL)
#define ISP_YNLM_LUT_W_12                  (ISP_BASE_ADDR+0x6290UL)
#define ISP_YNLM_LUT_W_13                  (ISP_BASE_ADDR+0x6294UL)
#define ISP_YNLM_LUT_W_14                  (ISP_BASE_ADDR+0x6298UL)
#define ISP_YNLM_LUT_W_15                  (ISP_BASE_ADDR+0x629CUL)
#define ISP_YNLM_LUT_W_16                  (ISP_BASE_ADDR+0x62A0UL)
#define ISP_YNLM_LUT_W_17                  (ISP_BASE_ADDR+0x62A4UL)
#define ISP_YNLM_LUT_W_18                  (ISP_BASE_ADDR+0x62A8UL)
#define ISP_YNLM_LUT_W_19                  (ISP_BASE_ADDR+0x62ACUL)
#define ISP_YNLM_LUT_W_20                  (ISP_BASE_ADDR+0x62B0UL)
#define ISP_YNLM_LUT_W_21                  (ISP_BASE_ADDR+0x62B4UL)
#define ISP_YNLM_LUT_W_22                  (ISP_BASE_ADDR+0x62B8UL)
#define ISP_YNLM_LUT_W_23                  (ISP_BASE_ADDR+0x62BCUL)
#define ISP_YNLM_LUT_VS_0                  (ISP_BASE_ADDR+0x6300UL)
#define ISP_YNLM_LUT_VS_1                  (ISP_BASE_ADDR+0x6304UL)
#define ISP_YNLM_LUT_VS_2                  (ISP_BASE_ADDR+0x6308UL)
#define ISP_YNLM_LUT_VS_3                  (ISP_BASE_ADDR+0x630CUL)
#define ISP_YNLM_LUT_VS_4                  (ISP_BASE_ADDR+0x6310UL)
#define ISP_YNLM_LUT_VS_5                  (ISP_BASE_ADDR+0x6314UL)
#define ISP_YNLM_LUT_VS_6                  (ISP_BASE_ADDR+0x6318UL)
#define ISP_YNLM_LUT_VS_7                  (ISP_BASE_ADDR+0x631CUL)
#define ISP_YNLM_LUT_VS_8                  (ISP_BASE_ADDR+0x6320UL)
#define ISP_YNLM_LUT_VS_9                  (ISP_BASE_ADDR+0x6324UL)
#define ISP_YNLM_LUT_VS_10                 (ISP_BASE_ADDR+0x6328UL)
#define ISP_YNLM_LUT_VS_11                 (ISP_BASE_ADDR+0x632CUL)
#define ISP_YNLM_LUT_VS_12                 (ISP_BASE_ADDR+0x6330UL)
#define ISP_YNLM_LUT_VS_13                 (ISP_BASE_ADDR+0x6334UL)
#define ISP_YNLM_LUT_VS_14                 (ISP_BASE_ADDR+0x6338UL)
#define ISP_YNLM_LUT_VS_15                 (ISP_BASE_ADDR+0x633CUL)
#define ISP_YNLM_LUT_VS_16                 (ISP_BASE_ADDR+0x6340UL)
#define ISP_YNLM_LUT_VS_17                 (ISP_BASE_ADDR+0x6344UL)
#define ISP_YNLM_LUT_VS_18                 (ISP_BASE_ADDR+0x6348UL)
#define ISP_YNLM_LUT_VS_19                 (ISP_BASE_ADDR+0x634CUL)
#define ISP_YNLM_LUT_VS_20                 (ISP_BASE_ADDR+0x6350UL)
#define ISP_YNLM_LUT_VS_21                 (ISP_BASE_ADDR+0x6354UL)
#define ISP_YNLM_LUT_VS_22                 (ISP_BASE_ADDR+0x6358UL)
#define ISP_YNLM_LUT_VS_23                 (ISP_BASE_ADDR+0x635CUL)
#define ISP_YNLM_LUT_VS_24                 (ISP_BASE_ADDR+0x6360UL)
#define ISP_YNLM_LUT_VS_25                 (ISP_BASE_ADDR+0x6364UL)
#define ISP_YNLM_LUT_VS_26                 (ISP_BASE_ADDR+0x6368UL)
#define ISP_YNLM_LUT_VS_27                 (ISP_BASE_ADDR+0x636CUL)
#define ISP_YNLM_LUT_VS_28                 (ISP_BASE_ADDR+0x6370UL)
#define ISP_YNLM_LUT_VS_29                 (ISP_BASE_ADDR+0x6374UL)
#define ISP_YNLM_LUT_VS_30                 (ISP_BASE_ADDR+0x6378UL)
#define ISP_YNLM_LUT_VS_31                 (ISP_BASE_ADDR+0x637CUL)
#define ISP_YNLM_LUT_VS_32                 (ISP_BASE_ADDR+0x6380UL)
#define ISP_YNLM_LUT_VS_33                 (ISP_BASE_ADDR+0x6384UL)
#define ISP_YNLM_LUT_VS_34                 (ISP_BASE_ADDR+0x6388UL)
#define ISP_YNLM_LUT_VS_35                 (ISP_BASE_ADDR+0x638CUL)
#define ISP_YNLM_LUT_VS_36                 (ISP_BASE_ADDR+0x6390UL)
#define ISP_YNLM_LUT_VS_37                 (ISP_BASE_ADDR+0x6394UL)
#define ISP_YNLM_LUT_VS_38                 (ISP_BASE_ADDR+0x6398UL)
#define ISP_YNLM_LUT_VS_39                 (ISP_BASE_ADDR+0x639CUL)
#define ISP_YNLM_LUT_VS_40                 (ISP_BASE_ADDR+0x63A0UL)
#define ISP_YNLM_LUT_VS_41                 (ISP_BASE_ADDR+0x63A4UL)
#define ISP_YNLM_LUT_VS_42                 (ISP_BASE_ADDR+0x63A8UL)
#define ISP_YNLM_LUT_VS_43                 (ISP_BASE_ADDR+0x63ACUL)
#define ISP_YNLM_LUT_VS_44                 (ISP_BASE_ADDR+0x63B0UL)
#define ISP_YNLM_LUT_VS_45                 (ISP_BASE_ADDR+0x63B4UL)
#define ISP_YNLM_LUT_VS_46                 (ISP_BASE_ADDR+0x63B8UL)
#define ISP_YNLM_LUT_VS_47                 (ISP_BASE_ADDR+0x63BCUL)
#define ISP_YNLM_LUT_VS_48                 (ISP_BASE_ADDR+0x63C0UL)
#define ISP_YNLM_LUT_VS_49                 (ISP_BASE_ADDR+0x63C4UL)
#define ISP_YNLM_LUT_VS_50                 (ISP_BASE_ADDR+0x63C8UL)
#define ISP_YNLM_LUT_VS_51                 (ISP_BASE_ADDR+0x63CCUL)
#define ISP_YNLM_LUT_VS_52                 (ISP_BASE_ADDR+0x63D0UL)
#define ISP_YNLM_LUT_VS_53                 (ISP_BASE_ADDR+0x63D4UL)
#define ISP_YNLM_LUT_VS_54                 (ISP_BASE_ADDR+0x63D8UL)
#define ISP_YNLM_LUT_VS_55                 (ISP_BASE_ADDR+0x63DCUL)
#define ISP_YNLM_LUT_VS_56                 (ISP_BASE_ADDR+0x63E0UL)
#define ISP_YNLM_LUT_VS_57                 (ISP_BASE_ADDR+0x63E4UL)
#define ISP_YNLM_LUT_VS_58                 (ISP_BASE_ADDR+0x63E8UL)
#define ISP_YNLM_LUT_VS_59                 (ISP_BASE_ADDR+0x63ECUL)
#define ISP_YNLM_LUT_VS_60                 (ISP_BASE_ADDR+0x63F0UL)
#define ISP_YNLM_LUT_VS_61                 (ISP_BASE_ADDR+0x63F4UL)
#define ISP_YNLM_LUT_VS_62                 (ISP_BASE_ADDR+0x63F8UL)
#define ISP_YNLM_LUT_VS_63                 (ISP_BASE_ADDR+0x63FCUL)



/*isp sub block: iircnr(pike has no this module)*/
/*#define ISP_IIRCNR_STATUS0                 (ISP_BASE_ADDR+0x6400UL)
#define ISP_IIRCNR_STATUS1                 (ISP_BASE_ADDR+0x6404UL)
#define ISP_IIRCNR_PARAM                   (ISP_BASE_ADDR+0x6414UL)
#define ISP_IIRCNR_PARAM1                  (ISP_BASE_ADDR+0x6418UL)
#define ISP_IIRCNR_PARAM2                  (ISP_BASE_ADDR+0x641CUL)
#define ISP_IIRCNR_PARAM3                  (ISP_BASE_ADDR+0x6420UL)
#define ISP_IIRCNR_PARAM4                  (ISP_BASE_ADDR+0x6424UL)
#define ISP_IIRCNR_PARAM5                  (ISP_BASE_ADDR+0x6428UL)
#define ISP_IIRCNR_PARAM6                  (ISP_BASE_ADDR+0x642CUL)
#define ISP_IIRCNR_PARAM7                  (ISP_BASE_ADDR+0x6430UL)
#define ISP_IIRCNR_PARAM8                  (ISP_BASE_ADDR+0x6434UL)
#define ISP_IIRCNR_PARAM9                  (ISP_BASE_ADDR+0x6438UL)
#define ISP_YRANDOM_PARAM0                 (ISP_BASE_ADDR+0x643CUL)
#define ISP_YRANDOM_PARAM1                 (ISP_BASE_ADDR+0x6440UL)
#define ISP_YRANDOM_PARAM2                 (ISP_BASE_ADDR+0x6444UL)
#define ISP_YRANDOM_STATUS0                (ISP_BASE_ADDR+0x6448UL)
#define ISP_YRANDOM_STATUS1                (ISP_BASE_ADDR+0x644CUL)
#define ISP_YRANDOM_STATUS2                (ISP_BASE_ADDR+0x6450UL)*/

/*pingpang buffer*/
#define ISP_3D_LUT_BUF0_CH0                (ISP_BASE_ADDR+0x17000UL)
#define ISP_HSV_BUF0_CH0                   (ISP_BASE_ADDR+0x18000UL)
#define ISP_VST_BUF0_CH0                   (ISP_BASE_ADDR+0x19000UL)
#define ISP_IVST_BUF0_CH0                  (ISP_BASE_ADDR+0x1A000UL)
#define ISP_FGAMMA_R_BUF0_CH0              (ISP_BASE_ADDR+0x1B000UL)
#define ISP_FGAMMA_G_BUF0_CH0              (ISP_BASE_ADDR+0x1C000UL)
#define ISP_FGAMMA_B_BUF0_CH0              (ISP_BASE_ADDR+0x1D000UL)
#define ISP_YGAMMA_BUF0_CH0                (ISP_BASE_ADDR+0x1E000UL)
//#define ISP_NLM_BUF0_CH0                   (ISP_BASE_ADDR+0x1F000UL)
//#define ISP_3D_LUT_BUF1_CH0                (ISP_BASE_ADDR+0x20000UL)
#define ISP_HSV_BUF1_CH0                   (ISP_BASE_ADDR+0x1F000UL)
//#define ISP_VST_BUF1_CH0                   (ISP_BASE_ADDR+0x22000UL)
//#define ISP_IVST_BUF1_CH0                  (ISP_BASE_ADDR+0x23000UL)
//#define ISP_FGAMMA_R_BUF1_CH0              (ISP_BASE_ADDR+0x24000UL)
//#define ISP_FGAMMA_G_BUF1_CH0              (ISP_BASE_ADDR+0x25000UL)
//#define ISP_FGAMMA_B_BUF1_CH0              (ISP_BASE_ADDR+0x26000UL)
//#define ISP_YGAMMA_BUF1_CH0                (ISP_BASE_ADDR+0x27000UL)
//#define ISP_NLM_BUF1_CH0                   (ISP_BASE_ADDR+0x28000UL)
#define ISP_LEN_BUF0_CH0                   (ISP_BASE_ADDR+0x29000UL)
#define ISP_LEN_BUF1_CH0                   (ISP_BASE_ADDR+0x2b000UL)

#define ISP_AHB_BASE                       SPRD_MMAHB_BASE
#define ISP_MODULE_EB                      (ISP_AHB_BASE+0x0000UL)
#define ISP_MODULE_RESET                   (ISP_AHB_BASE+0x0004UL)
#define ISP_CORE_CLK_EB                    (ISP_AHB_BASE+0x0008UL)

/*isp v memory1: awbm*/
#define ISP_RAW_AWBM_OUTPUT                (ISP_BASE_ADDR+0x10000)
/*#define ISP_RAW_AWBM_ITEM                  256*/

/*isp v memory2: aem*/
#define ISP_RAW_AEM_OUTPUT                 (ISP_BASE_ADDR+0x12000)
/*#define ISP_RAW_AEM_ITEM                   1024*/

/*isp v memory11: 3D_LUT0*/
#define ISP_3D_LUT0_OUTPUT                 (ISP_BASE_ADDR+0x17000)
/*#define ISP_3D_LUT0_ITEM                    729*/

/*isp v memory12: HSV BUF0*/
#define ISP_HSV_BUF0_OUTPUT                (ISP_BASE_ADDR+0x18000)
/*#define ISP_HSV_ITEM                     361*/

/*isp v memory20: 3D_LUT1*/
//#define ISP_3D_LUT1_OUTPUT                 (ISP_BASE_ADDR+0x20000)
/*#define ISP_3D_LUT1_ITEM                    729*/

/*isp v memory21: HSV BUF1*/
#define ISP_HSV_BUF1_OUTPUT                (ISP_BASE_ADDR+0x21000)
/*#define ISP_HSV_ITEM                     361*/

/*irq line number in system*/
#define ISP_IRQ                            IRQ_ISP_INT

#define ISP_RST_LOG_BIT                    BIT_2
#define ISP_RST_CFG_BIT                    BIT_3

#define ISP_IRQ_HW_MASK                    (0xFFFFFFFF)
#define ISP_IRQ_NUM                        (32)
#define ISP_REG_BUF_SIZE                   (4 * 1024)
#define ISP_RAW_AE_BUF_SIZE                (1024 * 4 * 3)
#define ISP_FRGB_GAMMA_BUF_SIZE                (257 * 4 + 4)
#define ISP_YUV_YGAMMA_BUF_SIZE            (129 * 4 )
#define ISP_RAW_AWB_BUF_SIZE                (256 * 4 * 9)
#define ISP_BING4AWB_SIZE                  (640 * 480 * 2)
#define ISP_YIQ_ANTIFLICKER_SIZE           (1944 * 4 * 61)
#define ISP_YIQ_AEM_BUF_SIZE               (1024 * 4)
