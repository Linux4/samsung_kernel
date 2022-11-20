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
 #ifndef _ISP_DRV_KERNEL_H_
 #define _ISP_DRV_KERNEL_H_

#define ISP_BYPASS_EB                      1
#define ISP_BYPASS_DIS                      0
#define ISP_AWBM_ITEM                      1024
#define ISP_HIST_ITEM                      256
#define ISP_HDR_COMP_ITEM                  64
#define ISP_HDR_P2E_ITEM                   32
#define ISP_HDR_E2P_ITEM                   32
#define ISP_AEM_ITEM                       1024


#define ISP_RAW_AWBM_ITEM                  256
#define ISP_RAW_AEM_ITEM                   1024
#define ISP_3D_LUT_ITEM                    729
#define ISP_RAW_AFM_ITEM                   25
#define ISP_YIQ_AFM_ITEM                   100
#define ISP_YIQ_AEM_ITEM                   1024
#define ISP_HSV_ITEM                       361

#define ISP_RAW_AFM_ITEM_R6P9              105

#define ISP_CMC_MATRIX_TAB_MAX             9
#define ISP_GAMMA_NODE_MAX                 66
#define ISP_CCE_MATRIX_TAB_MAX             9
#define ISP_CCE_UVD_NUM                    7
#define ISP_CCE_UVC0_NUM                   2
#define ISP_CCE_UVC1_NUM                   3
#define ISP_AFM_WIN_NUM                    9
#define ISP_CSS_LOWER_NUM                  7
#define ISP_CSS_LOWER_SUM_NUM              7
#define ISP_CSS_RATIO_NUM                  8
#define ISP_NLC_R_NODE_NUM                 29
#define ISP_NLC_G_NODE_NUM                 29
#define ISP_NLC_B_NODE_NUM                 29
#define ISP_NLC_L_NODE_NUM                 27
#define ISP_YIQ_YGAMMA_XNODE_NUM           8
#define ISP_YIQ_YGAMMA_YNODE_NUM           10
#define ISP_YIQ_YGAMMA_NODE_INDEX_NUM      9
#define ISP_PINGPANG_YUV_YGAMC_NUM         129


enum isp_chip_id {
	ISP_CHIP_ID_INVALID = 0x00,
	ISP_CHIP_ID_SC8820 = 0xA55A8820,
	ISP_CHIP_ID_SHARKL = 0xA55A9630,
	ISP_CHIP_ID_PIKE = 0xA55A7720,
	ISP_CHIP_ID_TSHARK2 = 0xA55A9838,
	ISP_CHIP_ID_TSHARK3
};

enum isp_interrupt_mode {
	ISP_INT_VIDEO_MODE = 0x00,
	ISP_INT_CAPTURE_MODE,
	ISP_INT_CLEAR_MODE
};

 enum isp_clk_sel {
 	ISP_CLK_480M = 0,
	ISP_CLK_384M,
	ISP_CLK_312M,
	ISP_CLK_256M,
	ISP_CLK_128M,
	ISP_CLK_76M8,
	ISP_CLK_48M,
	ISP_CLK_NONE
};

enum {
	ISP_LNC_STATUS_OK = (1<<0),
};

enum {
	ISP_INT_HIST_STORE = (1<<0),
	ISP_INT_STORE = (1<<1),
	ISP_INT_LSC_LOAD = (1<<2),
	ISP_INT_HIST_CAL = (1<<3),
	ISP_INT_HIST_RST = (1<<4),
	ISP_INT_FETCH_BUF_FULL = (1<<5),
	ISP_INT_STORE_BUF_FULL = (1<<6),
	ISP_INT_STORE_ERR = (1<<7),
	ISP_INT_SHADOW = (1<<8),
	ISP_INT_PREVIEW_STOP = (1<<9),
	ISP_INT_AWB_DONE = (1<<10),
	ISP_INT_AF_DONE = (1<<11),
	ISP_INT_SLICE_CNT = (1<<12),
	ISP_INT_AE_DONE = (1<<13),
	ISP_INT_ANTI_FLICKER = (1<<14),
	ISP_INT_AWB_START = (1<<15),
	ISP_INT_AF_START = (1<<16),
	ISP_INT_AE_START = (1<<17),
	ISP_INT_DCAM_SOF = (1<<18),
	ISP_INT_DCAM_EOF = (1<<19),
	ISP_INT_AFM_WIN8 = (1<<20),
	ISP_INT_AFM_WIN7 = (1<<21),
	ISP_INT_AFM_WIN6 = (1<<22),
	ISP_INT_AFM_WIN5 = (1<<23),
	ISP_INT_AFM_WIN4 = (1<<24),
	ISP_INT_AFM_WIN3 = (1<<25),
	ISP_INT_AFM_WIN2 = (1<<26),
	ISP_INT_AFM_WIN1 = (1<<27),
	ISP_INT_AFM_WIN0 = (1<<28),
};

/*********pike**********/
enum {
	ISP_INT_P_ISP_ALL_DONE = (1<<0),
	ISP_INT_P_STORE_DONE = (1<<1),
	ISP_INT_P_LENS_LOAD = (1<<2),
	ISP_INT_P_ISP_START = (1<<3),
	ISP_INT_P_ISP_BUF_FULL = (1<<4),
	ISP_INT_P_DCAM_BUF_FULL = (1<<5),
	ISP_INT_P_STORE_BUF_FULL = (1<<6),
	ISP_INT_P_SHADOW_DONE = (1<<7),
	ISP_INT_P_AWBM_DONE = (1<<8),
	ISP_INT_P_AFM_DONE = (1<<9),
	ISP_INT_P_AEM_DONE = (1<<10),
	ISP_INT_P_AEM2_DONE = (1<<11),
	ISP_INT_P_ANTI_FILCKER_DONE = (1<<12),
	ISP_INT_P_BINNING_DONE = (1<<13),
	ISP_INT_P_DCAM_SOF = (1<<14),
	ISP_INT_P_DCAM_EOF = (1<<15),
	ISP_INT_P_HIST_CAL = (1<<16),
	ISP_INT_P_HIST_RST = (1<<17),
	ISP_INT_P_HIST2 = (1<<18),
	ISP_INT_P_LENS_ERR = (1<<19),
	ISP_INT_P_AWBM_ERR = (1<<20),
	ISP_INT_P_BINNING_ERR = (1<<21),
	ISP_INT_P_BINNING_ERR1 = (1<<22),
	ISP_INT_P_BPC_ERR2 = (1<<23),
	ISP_INT_P_BPC_ERR1 = (1<<24),
	ISP_INT_P_BPC_ERR0 = (1<<25),
	ISP_INT_P_AFM_START = (1<<26),
	ISP_INT_P_AEM_START = (1<<27),
	ISP_INT_P_AEM2_START = (1<<28),
};

enum {
	ISP_INT_P_AFM_Y_WIN0 = (1<<0),
	ISP_INT_P_AFM_Y_WIN1 = (1<<1),
	ISP_INT_P_AFM_Y_WIN2 = (1<<2),
	ISP_INT_P_AFM_Y_WIN3 = (1<<3),
	ISP_INT_P_AFM_Y_WIN4 = (1<<4),
	ISP_INT_P_AFM_Y_WIN5 = (1<<5),
	ISP_INT_P_AFM_Y_WIN6 = (1<<6),
	ISP_INT_P_AFM_Y_WIN7 = (1<<7),
	ISP_INT_P_AFM_Y_WIN8 = (1<<8),
	ISP_INT_P_AFM_Y_WIN9 = (1<<9),
	ISP_INT_P_AFM_Y_WIN10 = (1<<10),
	ISP_INT_P_AFM_Y_WIN11 = (1<<11),
	ISP_INT_P_AFM_Y_WIN12 = (1<<12),
	ISP_INT_P_AFM_Y_WIN13 = (1<<13),
	ISP_INT_P_AFM_Y_WIN14 = (1<<14),
	ISP_INT_P_AFM_Y_WIN15 = (1<<15),
	ISP_INT_P_AFM_Y_WIN16 = (1<<16),
	ISP_INT_P_AFM_Y_WIN17 = (1<<17),
	ISP_INT_P_AFM_Y_WIN18 = (1<<18),
	ISP_INT_P_AFM_Y_WIN19 = (1<<19),
	ISP_INT_P_AFM_Y_WIN20 = (1<<20),
	ISP_INT_P_AFM_Y_WIN21 = (1<<21),
	ISP_INT_P_AFM_Y_WIN22 = (1<<22),
	ISP_INT_P_AFM_Y_WIN23 = (1<<23),
	ISP_INT_P_AFM_Y_WIN24 = (1<<24),
};

enum {
	ISP_INT_P_AFM_RGB_WIN0 = (1<<0),
	ISP_INT_P_AFM_RGB_WIN1 = (1<<1),
	ISP_INT_P_AFM_RGB_WIN2 = (1<<2),
	ISP_INT_P_AFM_RGB_WIN3 = (1<<3),
	ISP_INT_P_AFM_RGB_WIN4 = (1<<4),
	ISP_INT_P_AFM_RGB_WIN5 = (1<<5),
	ISP_INT_P_AFM_RGB_WIN6 = (1<<6),
	ISP_INT_P_AFM_RGB_WIN7 = (1<<7),
	ISP_INT_P_AFM_RGB_WIN8 = (1<<8),
	ISP_INT_P_AFM_RGB_WIN9 = (1<<9),
	ISP_INT_P_AFM_RGB_WIN10 = (1<<10),
	ISP_INT_P_AFM_RGB_WIN11 = (1<<11),
	ISP_INT_P_AFM_RGB_WIN12 = (1<<12),
	ISP_INT_P_AFM_RGB_WIN13 = (1<<13),
	ISP_INT_P_AFM_RGB_WIN14 = (1<<14),
	ISP_INT_P_AFM_RGB_WIN15 = (1<<15),
	ISP_INT_P_AFM_RGB_WIN16 = (1<<16),
	ISP_INT_P_AFM_RGB_WIN17 = (1<<17),
	ISP_INT_P_AFM_RGB_WIN18 = (1<<18),
	ISP_INT_P_AFM_RGB_WIN19 = (1<<19),
	ISP_INT_P_AFM_RGB_WIN20 = (1<<20),
	ISP_INT_P_AFM_RGB_WIN21 = (1<<21),
	ISP_INT_P_AFM_RGB_WIN22 = (1<<22),
	ISP_INT_P_AFM_RGB_WIN23 = (1<<23),
	ISP_INT_P_AFM_RGB_WIN24 = (1<<24),
};

enum {
	ISP_INT_P_YUV_DONE = (1<<0),
	ISP_INT_P_YUV_POSTCDN = (1<<1),
	ISP_INT_P_YUV_CDN_DONE = (1<<2),
	ISP_INT_P_YUV_EE_DONE = (1<<3),
	ISP_INT_P_YUV_NLM_DONE = (1<<4),
	ISP_INT_P_YUV_PRF_UV_DONE = (1<<5),
	ISP_INT_P_YUV_PRF_Y_DONE = (1<<6),
	ISP_INT_P_YUV_START = (1<<7),
	ISP_INT_P_FRGB_CCE_DONE = (1<<8),
	ISP_INT_P_FRGB_CCE_START = (1<<9),
	ISP_INT_P_FRGB_DONE = (1<<10),
	ISP_INT_P_FRGB_START = (1<<11),
	ISP_INT_P_FRGB_CF_AE_DONE = (1<<12),
	ISP_INT_P_FRGB_CF_AE_START = (1<<13),
	ISP_INT_P_RRGB_DONE = (1<<14),
	ISP_INT_P_RRGB_BDN_DONE = (1<<15),
	ISP_INT_P_RRGB_PWD_DONE = (1<<16),
	ISP_INT_P_RRGB_BPC_DONE = (1<<17),
	ISP_INT_P_RRGB_LENS_DONE = (1<<18),
	ISP_INT_P_RRGB_NLM_DONE = (1<<19),
	ISP_INT_P_RRGB_START = (1<<20),
};

/*********tshark2***********/
enum {
	ISP_INT_ISP_ALL_DONE = (1<<0),
	ISP_INT_SHADOW_DONE = (1<<1),
	ISP_INT_STORE_DONE = (1<<2),
	ISP_INT_ISP_STRAT = (1<<3),
	ISP_INT_FETCH_BUFFER_FULL = (1<<4),
	ISP_INT_STORE_BUFFER_FULL = (1<<5),
	ISP_INT_ISP2DCAM_AFIFO_FULL = (1<<6),
	ISP_INT_LSC_LOAD_DONE = (1<<7),
	ISP_INT_AEM_START = (1<<8),
	ISP_INT_AEM_DONE = (1<<9),
	ISP_INT_AEM2_START = (1<<10),
	ISP_INT_AEM2_DONE = (1<<11),
	ISP_INT_AFM_Y_START = (1<<12),
	ISP_INT_AFM_Y_DONE = (1<<13),
	ISP_INT_AFM_RGB_START = (1<<14),
	ISP_INT_AFM_RGB_DONE = (1<<15),
	ISP_INT_AWBM_START = (1<<16),
	ISP_INT_AWBM_DONE = (1<<17),
	ISP_INT_BINNING_DONE = (1<<18),
	ISP_INT_BINNING_START = (1<<19),
	ISP_INT_AFL_START = (1<<20),
	ISP_INT_AFL_DONE = (1<<21),
	ISP_INT_DCAMERA_SOF = (1<<22),
	ISP_INT_DCAMERA_EOF = (1<<23),
	ISP_INT_HIST_START = (1<<24),
	ISP_INT_HIST_DONE = (1<<25),
	ISP_INT_HIST2_START = (1<<26),
	ISP_INT_HIST2_DONE = (1<<27),
	ISP_INT_HIST2_WIN0_DONE = (1<<28),
	ISP_INT_HIST2_WIN1_DONE = (1<<29),
	ISP_INT_HIST2_WIN2_DONE = (1<<30),
	ISP_INT_HIST2_WIN3_DONE = (1<<31),
};

enum {
	ISP_INT_AFM_Y_WIN0 = (1<<0),
	ISP_INT_AFM_Y_WIN1 = (1<<1),
	ISP_INT_AFM_Y_WIN2 = (1<<2),
	ISP_INT_AFM_Y_WIN3 = (1<<3),
	ISP_INT_AFM_Y_WIN4 = (1<<4),
	ISP_INT_AFM_Y_WIN5 = (1<<5),
	ISP_INT_AFM_Y_WIN6 = (1<<6),
	ISP_INT_AFM_Y_WIN7 = (1<<7),
	ISP_INT_AFM_Y_WIN8 = (1<<8),
	ISP_INT_AFM_Y_WIN9 = (1<<9),
	ISP_INT_AFM_Y_WIN10 = (1<<10),
	ISP_INT_AFM_Y_WIN11 = (1<<11),
	ISP_INT_AFM_Y_WIN12 = (1<<12),
	ISP_INT_AFM_Y_WIN13 = (1<<13),
	ISP_INT_AFM_Y_WIN14 = (1<<14),
	ISP_INT_AFM_Y_WIN15 = (1<<15),
	ISP_INT_AFM_Y_WIN16 = (1<<16),
	ISP_INT_AFM_Y_WIN17 = (1<<17),
	ISP_INT_AFM_Y_WIN18 = (1<<18),
	ISP_INT_AFM_Y_WIN19 = (1<<19),
	ISP_INT_AFM_Y_WIN20 = (1<<20),
	ISP_INT_AFM_Y_WIN21 = (1<<21),
	ISP_INT_AFM_Y_WIN22 = (1<<22),
	ISP_INT_AFM_Y_WIN23 = (1<<23),
	ISP_INT_AFM_Y_WIN24 = (1<<24),
	ISP_INT_DISPATCH_BUF_FULL = (1<<25),
	ISP_INT_AWBM_ERR = (1<<26),
	ISP_INT_BINNING_ERR1 = (1<<27),
	ISP_INT_BINNING_ERR0 = (1<<28),
	ISP_INT_BPC_ERR2 = (1<<29),
	ISP_INT_BPC_ERR1 = (1<<30),
	ISP_INT_BPC_ERR0 = (1<<31),
};

enum {
	ISP_INT_AFM_RGB_WIN0 = (1<<0),
	ISP_INT_AFM_RGB_WIN1 = (1<<1),
	ISP_INT_AFM_RGB_WIN2 = (1<<2),
	ISP_INT_AFM_RGB_WIN3 = (1<<3),
	ISP_INT_AFM_RGB_WIN4 = (1<<4),
	ISP_INT_AFM_RGB_WIN5 = (1<<5),
	ISP_INT_AFM_RGB_WIN6 = (1<<6),
	ISP_INT_AFM_RGB_WIN7 = (1<<7),
	ISP_INT_AFM_RGB_WIN8 = (1<<8),
	ISP_INT_AFM_RGB_WIN9 = (1<<9),
	ISP_INT_AFM_RGB_WIN10 = (1<<10),
	ISP_INT_AFM_RGB_WIN11 = (1<<11),
	ISP_INT_AFM_RGB_WIN12 = (1<<12),
	ISP_INT_AFM_RGB_WIN13 = (1<<13),
	ISP_INT_AFM_RGB_WIN14 = (1<<14),
	ISP_INT_AFM_RGB_WIN15 = (1<<15),
	ISP_INT_AFM_RGB_WIN16 = (1<<16),
	ISP_INT_AFM_RGB_WIN17 = (1<<17),
	ISP_INT_AFM_RGB_WIN18 = (1<<18),
	ISP_INT_AFM_RGB_WIN19 = (1<<19),
	ISP_INT_AFM_RGB_WIN20 = (1<<20),
	ISP_INT_AFM_RGB_WIN21 = (1<<21),
	ISP_INT_AFM_RGB_WIN22 = (1<<22),
	ISP_INT_AFM_RGB_WIN23 = (1<<23),
	ISP_INT_AFM_RGB_WIN24 = (1<<24),
	ISP_INT_AFL_NEW_DDR_START = (1<<25),
	ISP_INT_AFL_NEW_DDR_DONE = (1<<26),
	ISP_INT_AFL_NEW_SRAM_START = (1<<27),
	ISP_INT_AFL_NEW_SRAM_DONE = (1<<28),
};

enum {
	ISP_INT_EVT_STOP = (1<<31),
};

enum {
	ISP_INT_EVT_HIST_STORE = (1<<0),
	/*ISP_INT_EVT_STORE = (1<<1),*/
	/*ISP_INT_EVT_LSC_LOAD = (1<<2),*/
	ISP_INT_EVT_HIST_CAL = (1<<3),
	ISP_INT_EVT_HIST_RST = (1<<4),
	/*ISP_INT_EVT_FETCH_BUF_FULL = (1<<5),*/
	/*ISP_INT_EVT_STORE_BUF_FULL = (1<<6),*/
	ISP_INT_EVT_STORE_ERR = (1<<7),
	/*ISP_INT_EVT_SHADOW = (1<<8),*/
	ISP_INT_EVT_PREVIEW_STOP = (1<<9),
	/*ISP_INT_EVT_AWB = (1<<10),*/
	/*ISP_INT_EVT_AF = (1<<11),*/
	ISP_INT_EVT_SLICE_CNT = (1<<12),
	/*ISP_INT_EVT_AE = (1<<13),*/
	/*ISP_INT_EVT_ANTI_FLICKER = (1<<14),*/
	/*ISP_INT_EVT_AWBM_START = (1<<15),*/
	/*ISP_INT_EVT_AFM_START = (1<<16),*/
	/*ISP_INT_EVT_AE_START = (1<<17),*/
	/*ISP_INT_EVT_DCAM_SOF = (1<<18),*/
	/*ISP_INT_EVT_DCAM_EOF = (1<<19),*/
	/*ISP_INT_EVT_AFM_WIN8 = (1<<20),*/
	/*ISP_INT_EVT_AFM_WIN7 = (1<<21),*/
	/*ISP_INT_EVT_AFM_WIN6 = (1<<22),*/
	/*ISP_INT_EVT_AFM_WIN5 = (1<<23),*/
	/*ISP_INT_EVT_AFM_WIN4 = (1<<24),*/
	/*ISP_INT_EVT_AFM_WIN3 = (1<<25),*/
	/*ISP_INT_EVT_AFM_WIN2 = (1<<26),*/
	/*ISP_INT_EVT_AFM_WIN1 = (1<<27),*/
	/*ISP_INT_EVT_AFM_WIN0 = (1<<28),*/
};

/*******tshark2*******/
enum {
	ISP_INT_EVT_ISP_ALL_DONE = (1<<0),
	ISP_INT_EVT_SHADOW_DONE = (1<<1),
	ISP_INT_EVT_STORE_DONE = (1<<2),
	ISP_INT_EVT_ISP_STRAT = (1<<3),
	ISP_INT_EVT_FETCH_BUF_FULL = (1<<4),
	ISP_INT_EVT_STORE_BUF_FULL = (1<<5),
	ISP_INT_EVT_ISP2DCAM_AFIFO_FULL = (1<<6),
	ISP_INT_EVT_LSC_LOAD = (1<<7),
	ISP_INT_EVT_AEM_START = (1<<8),
	ISP_INT_EVT_AEM_DONE = (1<<9),
	ISP_INT_EVT_AEM2_START = (1<<10),
	ISP_INT_EVT_AEM2_DONE = (1<<11),
	ISP_INT_EVT_AFM_Y_START = (1<<12),
	ISP_INT_EVT_AFM_Y_DONE = (1<<13),
	ISP_INT_EVT_AFM_RGB_START = (1<<14),
	ISP_INT_EVT_AFM_RGB_DONE = (1<<15),
	ISP_INT_EVT_AWBM_START = (1<<16),
	ISP_INT_EVT_AWBM_DONE = (1<<17),
	ISP_INT_EVT_BINNING_DONE = (1<<18),
	ISP_INT_EVT_BINNING_START = (1<<19),
	ISP_INT_EVT_AFL_START = (1<<20),
	ISP_INT_EVT_AFL_DONE = (1<<21),
	ISP_INT_EVT_DCAM_SOF = (1<<22),
	ISP_INT_EVT_DCAM_EOF = (1<<23),
	ISP_INT_EVT_HIST_START = (1<<24),
	ISP_INT_EVT_HIST_DONE = (1<<25),
	ISP_INT_EVT_HIST2_START = (1<<26),
	ISP_INT_EVT_HIST2_DONE = (1<<27),
	ISP_INT_EVT_HIST2_WIN0_DONE = (1<<28),
	ISP_INT_EVT_HIST2_WIN1_DONE = (1<<29),
	ISP_INT_EVT_HIST2_WIN2_DONE = (1<<30),
	ISP_INT_EVT_HIST2_WIN3_DONE = (1<<31),
};

enum {
	ISP_INT_EVT_AFM_Y_WIN0 = (1<<0),
	ISP_INT_EVT_AFM_Y_WIN1 = (1<<1),
	ISP_INT_EVT_AFM_Y_WIN2 = (1<<2),
	ISP_INT_EVT_AFM_Y_WIN3 = (1<<3),
	ISP_INT_EVT_AFM_Y_WIN4 = (1<<4),
	ISP_INT_EVT_AFM_Y_WIN5 = (1<<5),
	ISP_INT_EVT_AFM_Y_WIN6 = (1<<6),
	ISP_INT_EVT_AFM_Y_WIN7 = (1<<7),
	ISP_INT_EVT_AFM_Y_WIN8 = (1<<8),
	ISP_INT_EVT_AFM_Y_WIN9 = (1<<9),
	ISP_INT_EVT_AFM_Y_WIN10 = (1<<10),
	ISP_INT_EVT_AFM_Y_WIN11 = (1<<11),
	ISP_INT_EVT_AFM_Y_WIN12 = (1<<12),
	ISP_INT_EVT_AFM_Y_WIN13 = (1<<13),
	ISP_INT_EVT_AFM_Y_WIN14 = (1<<14),
	ISP_INT_EVT_AFM_Y_WIN15 = (1<<15),
	ISP_INT_EVT_AFM_Y_WIN16 = (1<<16),
	ISP_INT_EVT_AFM_Y_WIN17 = (1<<17),
	ISP_INT_EVT_AFM_Y_WIN18 = (1<<18),
	ISP_INT_EVT_AFM_Y_WIN19 = (1<<19),
	ISP_INT_EVT_AFM_Y_WIN20 = (1<<20),
	ISP_INT_EVT_AFM_Y_WIN21 = (1<<21),
	ISP_INT_EVT_AFM_Y_WIN22 = (1<<22),
	ISP_INT_EVT_AFM_Y_WIN23 = (1<<23),
	ISP_INT_EVT_AFM_Y_WIN24 = (1<<24),
	ISP_INT_EVT_DISPATCH_BUF_FULL = (1<<25),
	ISP_INT_EVT_AWBM_ERR = (1<<26),
	ISP_INT_EVT_BINNING_ERR1 = (1<<27),
	ISP_INT_EVT_BINNING_ERR0 = (1<<28),
	ISP_INT_EVT_BPC_ERR2 = (1<<29),
	ISP_INT_EVT_BPC_ERR1 = (1<<30),
	ISP_INT_EVT_BPC_ERR0 = (1<<31),
};

enum {
	ISP_INT_EVT_AFM_RGB_WIN0 = (1<<0),
	ISP_INT_EVT_AFM_RGB_WIN1 = (1<<1),
	ISP_INT_EVT_AFM_RGB_WIN2 = (1<<2),
	ISP_INT_EVT_AFM_RGB_WIN3 = (1<<3),
	ISP_INT_EVT_AFM_RGB_WIN4 = (1<<4),
	ISP_INT_EVT_AFM_RGB_WIN5 = (1<<5),
	ISP_INT_EVT_AFM_RGB_WIN6 = (1<<6),
	ISP_INT_EVT_AFM_RGB_WIN7 = (1<<7),
	ISP_INT_EVT_AFM_RGB_WIN8 = (1<<8),
	ISP_INT_EVT_AFM_RGB_WIN9 = (1<<9),
	ISP_INT_EVT_AFM_RGB_WIN10 = (1<<10),
	ISP_INT_EVT_AFM_RGB_WIN11 = (1<<11),
	ISP_INT_EVT_AFM_RGB_WIN12 = (1<<12),
	ISP_INT_EVT_AFM_RGB_WIN13 = (1<<13),
	ISP_INT_EVT_AFM_RGB_WIN14 = (1<<14),
	ISP_INT_EVT_AFM_RGB_WIN15 = (1<<15),
	ISP_INT_EVT_AFM_RGB_WIN16 = (1<<16),
	ISP_INT_EVT_AFM_RGB_WIN17 = (1<<17),
	ISP_INT_EVT_AFM_RGB_WIN18 = (1<<18),
	ISP_INT_EVT_AFM_RGB_WIN19 = (1<<19),
	ISP_INT_EVT_AFM_RGB_WIN20 = (1<<20),
	ISP_INT_EVT_AFM_RGB_WIN21 = (1<<21),
	ISP_INT_EVT_AFM_RGB_WIN22 = (1<<22),
	ISP_INT_EVT_AFM_RGB_WIN23 = (1<<23),
	ISP_INT_EVT_AFM_RGB_WIN24 = (1<<24),
	ISP_INT_EVT_AFL_NEW_DDR_START = (1<<25),
	ISP_INT_EVT_AFL_NEW_DDR_DONE = (1<<26),
	ISP_INT_EVT_AFL_NEW_SRAM_START = (1<<27),
	ISP_INT_EVT_AFL_NEW_SRAM_DONE = (1<<28),
};

enum isp_block {
	ISP_BLOCK_FETCH,
	ISP_BLOCK_BLC,
	ISP_BLOCK_2D_LSC,
	ISP_BLOCK_AWB,
	ISP_BLOCK_BPC,
	ISP_BLOCK_BDN,
	ISP_BLOCK_GRGB,
	ISP_BLOCK_CFA,
	ISP_BLOCK_CMC,
	ISP_BLOCK_GAMMA,
	ISP_BLOCK_CCE,
	ISP_BLOCK_PRE_FILTER,
	ISP_BLOCK_BRIGHTNESS,
	ISP_BLOCK_CONTRAST,
	ISP_BLOCK_HIST,
	ISP_BLOCK_ACA,
	ISP_BLOCK_AFM,
	ISP_BLOCK_EDGE,
	ISP_BLOCK_EMBOSS,
	ISP_BLOCK_FCS,
	ISP_BLOCK_CSS,
	ISP_BLOCK_CSA,
	ISP_BLOCK_STORE,
	ISP_BLOCK_FEEDER,
	ISP_BLOCK_ARBITER,
	ISP_BLOCK_HDR,
	ISP_BLOCK_NLC,
	ISP_BLOCK_NAWBM,
	ISP_BLOCK_PRE_WAVELET,
	ISP_BLOCK_BINNING4AWB,
	ISP_BLOCK_PRE_GLB_GAIN,
	ISP_BLOCK_COMMON,
	ISP_BLOCK_GLB_GAIN,
	ISP_BLOCK_RGB_GAIN,
	ISP_BLOCK_YIQ,
	ISP_BLOCK_HUE,
	ISP_BLOCK_NBPC,
/***************Tshark2**************************************/
	ISP_BLOCK_1D_LSC,
	ISP_BLOCK_RAW_AEM,

	ISP_BLOCK_RGB_GAIN2,
//	ISP_BLOCK_VST,
	ISP_BLOCK_NLM,
//	ISP_BLOCK_IVST,

	ISP_BLOCK_CMC8,
	ISP_BLOCK_CT,
	ISP_BLOCK_HSV,
	ISP_BLOCK_CSC,
	ISP_BLOCK_PRE_CDN_RGB,
	ISP_BLOCK_POSTERIZE,
	ISP_BLOCK_AFM_V1,
	ISP_BLOCK_YIQ_AEM,
	ISP_BLOCK_ANTI_FLICKER,
	ISP_BLOCK_YIQ_AFM,
	ISP_BLOCK_YUV_PRECDN,
	ISP_BLOCK_PRE_FILTER_V1,
	ISP_BLOCK_HIST2,
	ISP_BLOCK_YUV_CDN,
	ISP_BLOCK_HUA,
	ISP_BLOCK_POST_CDN,
	ISP_BLOCK_YGAMMA,
	ISP_BLOCK_YDELAY,
	ISP_BLOCK_IIRCNR,
	ISP_BLOCK_DISPATCH,
	ISP_BLOCK_ARBITER_V1,
	ISP_BLOCK_COMMON_V1,
	ISP_BLOCK_RAW_SIZER,
/***************pike**************************************/
//	ISP_BLOCK_WDR,
	ISP_BLOCK_RGB2Y,
	ISP_BLOCK_UV_PREFILTER,
	ISP_BLOCK_YUV_NLM

};

enum isp_fetch_property {
	ISP_PRO_FETCH_BLOCK,
	ISP_PRO_FETCH_SLICE_SIZE,
	ISP_PRO_FETCH_START_ISP,
};

enum isp_blc_property {
	ISP_PRO_BLC_BLOCK,
	ISP_PRO_BLC_SLICE_SIZE,
	ISP_PRO_BLC_SLICE_INFO,
};

enum isp_2d_lsc_property {
	ISP_PRO_2D_LSC_BLOCK,
	ISP_PRO_2D_LSC_BYPASS,
	ISP_PRO_2D_LSC_PARAM_UPDATE,
	ISP_PRO_2D_LSC_POS,
	ISP_PRO_2D_LSC_GRID_SIZE,
	ISP_PRO_2D_LSC_LOAD_BUF_SEL,
	ISP_PRO_2D_LSC_SLICE_SIZE,
	ISP_PRO_2D_LSC_TRANSADDR,

};

enum isp_1d_lsc_property {
	ISP_PRO_1D_LSC_BLOCK,
	ISP_PRO_1D_LSC_SLICE_SIZE,
	ISP_PRO_1D_LSC_POS,
};

enum isp_awb_property {
	ISP_PRO_AWB_BLOCK,
	ISP_PRO_AWBM_STATISTICS,
	ISP_PRO_AWBM_BYPASS,
	ISP_PRO_AWBM_MODE,
	ISP_PRO_AWBM_SKIP_NUM,
	ISP_PRO_AWBM_SKIP_NUM_CLR,
	ISP_PRO_AWBM_BLOCK_OFFSET,
	ISP_PRO_AWBM_BLOCK_SIZE,
	ISP_PRO_AWBM_SHIFT,
	ISP_PRO_AWBM_THR_BYPASS,
	ISP_PRO_AWBM_WR_POS,
	ISP_PRO_AWBM_NW_POS,
	ISP_PRO_AWBM_CLCTOR_POS,
	ISP_PRO_AWBM_CLCTOR_PIXEL_NUM,
	ISP_PRO_AWBM_THR_VALUE,
	ISP_PRO_AWBM_MEM_ADDR,
	ISP_PRO_AWBC_BYPASS,
	ISP_PRO_AWBC_GAIN,
	ISP_PRO_AWBC_THRD,
	ISP_PRO_AWBC_GAIN_OFFSET,
};

enum isp_bpc_property {
	ISP_PRO_BPC_BLOCK,
	ISP_PRO_BPC_BYPASS,
	ISP_PRO_BPC_MODE,
	ISP_PRO_BPC_PARAM_COMMON,
	ISP_PRO_BPC_THRD,
	ISP_PRO_BPC_MAP_ADDR,
	ISP_PRO_BPC_PIXEL_NUM,
	ISP_PRO_BPC_DIFF_THRD,
};

enum isp_wavelet_denoise_property {
	ISP_PRO_BDN_BLOCK,
	ISP_PRO_BDN_BYPASS,
	ISP_PRO_BDN_SLICE_SIZE,
	ISP_PRO_BDN_DISWEI,
	ISP_PRO_BDN_RANWEI,
};

enum isp_grgb_property {
	ISP_PRO_GRGB_BLOCK,
	ISP_PRO_GRGB_BYPASS,
	ISP_PRO_GRGB_THRD,
};

enum isp_cfa_property {
	ISP_PRO_CFA_BLOCK,
	ISP_PRO_CFA_THRD,
	ISP_PRO_CFA_SLICE_SIZE,
	ISP_PRO_CFA_SLICE_INFO,
};

enum isp_cmc_property {
	ISP_PRO_CMC_BLOCK,
	ISP_PRO_CMC_BYPASS,
	ISP_PRO_CMC_MATRIX,
};

enum isp_gamma_property {
	ISP_PRO_GAMMA_BLOCK,
	ISP_PRO_GAMMA_BYPASS,
	ISP_PRO_GAMMA_NODE,
};

enum isp_cce_property {
	ISP_PRO_CCE_BLOCK_MATRIX,
	ISP_PRO_CCE_BLOCK_UV,
	ISP_PRO_CCE_UVDIVISION_BYPASS,
	ISP_PRO_CCE_MODE,
	ISP_PRO_CCE_MATRIX,
	ISP_PRO_CCE_SHIFT,
	ISP_PRO_CCE_UVD_THRD,
	ISP_PRO_CCE_UVC_PARAM,
};

enum isp_prefilter_property {
	ISP_PRO_PREF_BLOCK,
	ISP_PRO_PREF_BYPASS,
	ISP_PRO_PREF_WRITEBACK,
	ISP_PRO_PREF_THRD,
	ISP_PRO_PREF_SLICE_SIZE,
	ISP_PRO_PREF_SLICE_INFO,
};

enum isp_brightness_property {
	ISP_PRO_BRIGHT_BLOCK,
	ISP_PRO_BRIGHT_SLICE_SIZE,
	ISP_PRO_BRIGHT_SLICE_INFO,
};

enum isp_contrast_property {
	ISP_PRO_CONTRAST_BLOCK,
};

enum isp_hist_property {
	ISP_PRO_HIST_BLOCK,
	ISP_PRO_HIST_BYPASS,
	ISP_PRO_HIST_AUTO_RST_DISABLE,
	ISP_PRO_HIST_MODE,
	ISP_PRO_HIST_RATIO,
	ISP_PRO_HIST_MAXMIN,
	ISP_PRO_HIST_CLEAR_EB,
	ISP_PRO_HIST_STATISTIC,
	ISP_PRO_HIST_STATISTIC_NUM,
	ISP_PRO_HIST_SLICE_SIZE,
};

enum isp_autocont_property {
	ISP_PRO_ACA_BLOCK,
	ISP_PRO_ACA_BYPASS,
	ISP_PRO_ACA_MODE,
	ISP_PRO_ACA_MAXMIN,
	ISP_PRO_ACA_ADJUST,
};

enum isp_afm_property {
	ISP_PRO_AFM_BLOCK,
	ISP_PRO_AFM_BYPASS,
	ISP_PRO_AFM_SHIFT,
	ISP_PRO_AFM_MODE,
	ISP_PRO_AFM_SKIP_NUM,
	ISP_PRO_AFM_SKIP_NUM_CLR,
	ISP_PRO_AFM_WIN,
	ISP_PRO_AFM_STATISTIC,
	ISP_PRO_AFM_WIN_NUM
};

enum isp_edge_property {
	ISP_PRO_EDGE_BLOCK,
	ISP_PRO_EDGE_BYPASS,
	ISP_PRO_EDGE_PARAM,
};

enum isp_emboss_property {
	ISP_PRO_EMBOSS_BLOCK,
	ISP_PRO_EMBOSS_BYPASS,
	ISP_PRO_EMBOSS_PARAM,
};

enum isp_fcs_property {
	ISP_PRO_FCS_BLOCK,
	ISP_PRO_FCS_BYPASS,
	ISP_PRO_FCS_MODE,
};

enum isp_css_property {
	ISP_PRO_CSS_BLOCK,
	ISP_PRO_CSS_BYPASS,
	ISP_PRO_CSS_THRD,
	ISP_PRO_CSS_SLICE_SIZE,
	ISP_PRO_CSS_RATIO,
};

enum isp_csa_property {
	ISP_PRO_CSA_BLOCK,
	ISP_PRO_CSA_BYPASS,
	ISP_PRO_CSA_FACTOR,
};

enum isp_store_property {
	ISP_PRO_STORE_BLOCK,
	ISP_PRO_STORE_SLICE_SIZE,
};

enum isp_feeder_property {
	ISP_PRO_FEEDER_BLOCK,
	ISP_PRO_FEEDER_DATA_TYPE,
	ISP_PRO_FEEDER_SLICE_SIZE,
};

enum isp_hdr_property {
	ISP_PRO_HDR_BLOCK,
	ISP_PRO_HDR_BYPASS,
	ISP_PRO_HDR_LEVEL,
	ISP_PRO_HDR_INDEX,
	ISP_PRO_HDR_TAB,
};

enum isp_nlc_property {
	ISP_PRO_NLC_BLOCK,
	ISP_PRO_NLC_BYPASS,
	ISP_PRO_NLC_R_NODE,
	ISP_PRO_NLC_G_NODE,
	ISP_PRO_NLC_B_NODE,
	ISP_PRO_NLC_L_NODE,
};

enum isp_nawbm_property {
	ISP_PRO_NAWBM_BLOCK,
	ISP_PRO_NAWBM_BYPASS,
};

enum isp_pre_wavelet_property {
	ISP_PRO_PRE_WAVELET_BLOCK,
	ISP_PRO_PRE_WAVELET_BYPASS,
};

enum isp_binging4awb_property {
	ISP_PRO_BINNING4AWB_BLOCK,
	ISP_PRO_BINNING4AWB_BYPASS,
	ISP_PRO_BINNING4AWB_SCALING_RATIO,
	ISP_PRO_BINNING4AWB_GET_SCALING_RATIO,
	ISP_PRO_BINNING4AWB_MEM_ADDR,
	ISP_PRO_BINNING4AWB_STATISTICS_BUF,
	ISP_PRO_BINNING4AWB_TRANSADDR,
	ISP_PRO_BINNING4AWB_ENDIAN,
};

enum isp_pre_glb_gain_property {
	ISP_PRO_PRE_GLB_GAIN_BLOCK,
};

enum isp_common_property {
	ISP_PRO_COMMON_START,
	ISP_PRO_COMMON_IN_MODE,
	ISP_PRO_COMMON_OUT_MODE,
	ISP_PRO_COMMON_FETCH_ENDIAN,
	ISP_PRO_COMMON_BPC_ENDIAN,
	ISP_PRO_COMMON_STORE_ENDIAN,
	ISP_PRO_COMMON_FETCH_DATA_FORMAT,
	ISP_PRO_COMMON_STORE_FORMAT,
	ISP_PRO_COMMON_BURST_SIZE,
	ISP_PRO_COMMON_MEM_SWITCH,
	ISP_PRO_COMMON_SHADOW,
	ISP_PRO_COMMON_SHADOW_ALL,
	ISP_PRO_COMMON_BAYER_MODE,
	ISP_PRO_COMMON_INT_REGISTER,
	ISP_PRO_COMMON_INT_CLEAR,
	ISP_PRO_COMMON_GET_INT_RAW,
	ISP_PRO_COMMON_PMU_RAW_MASK,
	ISP_PRO_COMMON_HW_MASK,
	ISP_PRO_COMMON_HW_ENABLE,
	ISP_PRO_COMMON_PMU_SEL,
	ISP_PRO_COMMON_SW_ENABLE,
	ISP_PRO_COMMON_PREVIEW_STOP,
	ISP_PRO_COMMON_SET_SHADOW_CONTROL,
	ISP_PRO_COMMON_SHADOW_CONTROL_CLEAR,
	ISP_PRO_COMMON_AXI_STOP,
	ISP_PRO_COMMON_SLICE_CNT_ENABLE,
	ISP_PRO_COMMON_PREFORM_CNT_ENABLE,
	ISP_PRO_COMMON_SET_SLICE_NUM,
	ISP_PRO_COMMON_GET_SLICE_NUM,
	ISP_PRO_COMMON_PERFORM_CNT_RSTATUS,
	ISP_PRO_COMMON_PERFORM_CNT_STATUS,
};

enum isp_glb_gain_property {
	ISP_PRO_GLB_GAIN_BLOCK,
	ISP_PRO_GLB_GAIN_BYPASS,
	ISP_PRO_GLB_GAIN_SET,
	ISP_PRO_GLB_GAIN_SLICE_SIZE,
};

enum isp_rgb_gain_property {
	ISP_PRO_RGB_GAIN_BLOCK,
};

enum isp_yiq_property {
	ISP_RPO_YIQ_BLOCK_YGAMMA,
	ISP_RPO_YIQ_BLOCK_AE,
	ISP_RPO_YIQ_BLOCK_FLICKER,
	ISP_PRO_YIQ_YGAMMA_BYPASS,
	ISP_PRO_YIQ_YGAMMA_XNODE,
	ISP_PRO_YIQ_YGAMMA_YNODE,
	ISP_PRO_YIQ_YGAMMA_INDEX,
	ISP_PRO_YIQ_AE_BYPASS,
	ISP_PRO_YIQ_AE_SOURCE_SEL,
	ISP_PRO_YIQ_AE_MODE,
	ISP_PRO_YIQ_AE_SKIP_NUM,
	ISP_PRO_YIQ_FLICKER_BYPASS,
	ISP_PRO_YIQ_FLICKER_MODE,
	ISP_PRO_YIQ_FLICKER_VHEIGHT,
	ISP_PRO_YIQ_FLICKER_LINE_CONTER,
	ISP_PRO_YIQ_FLICKER_LINE_STEP,
	ISP_PRO_YIQ_FLICKER_LINE_START,
};

enum isp_hue_property {
	ISP_PRO_HUE_BLOCK,
	ISP_PRO_HUE_BYPASS,
	ISP_PRO_HUE_FACTOR,
};

enum isp_nbpc_property {
	ISP_PRO_NBPC_BLOCK,
	ISP_PRO_NBPC_BYPASS,
};

/****************Tshark2****************************/
enum isp_raw_aem_property {
	ISP_PRO_RAW_AEM_BLOCK,
	ISP_PRO_RAW_AEM_BYPASS,
	ISP_PRO_RAW_AEM_MODE,
	ISP_PRO_RAW_AEM_STATISTICS,
	ISP_PRO_RAW_AEM_SKIP_NUM,
	ISP_PRO_RAW_AEM_SHIFT,
	ISP_PRO_RAW_AEM_OFFSET,
	ISP_PRO_RAW_AEM_BLK_SIZE,
	ISP_PRO_RAW_AEM_SLICE_SIZE,
};

enum isp_ct_property {
	ISP_PRO_CT_BLOCK,
};

enum isp_csc_property {
	ISP_PRO_CSC_BLOCK,
	ISP_PRO_CSC_PIC_SIZE,
};

enum isp_posterize_property {
	ISP_PRO_POSTERIZE_BLOCK,
};

enum isp_yuv_precdn_property {
	ISP_PRO_YUV_PRECDN_BLOCK,
};

enum isp_cdn_property {
	ISP_PRO_YUV_CDN_BLOCK,
#if 0
	ISP_PRO_CDN_STATUS0,
	ISP_PRO_CDN_STATUS1,
	ISP_PRO_CDN_BYPASS,
	ISP_PRO_CDN_FILTER_BYPASS,
	ISP_PRO_CDN_MEDIAN_WRITEBACK_EN,
	ISP_PRO_CDN_MEDIAN_MODE,
	ISP_PRO_CDN_GAUSSIAN_MODE,
	ISP_PRO_CDN_MEDIAN_THR,
	ISP_PRO_CDN_MEDIAN_THRUV,
	ISP_PRO_CDN_RANWEI,
#endif
};

enum isp_rgbg2_property {
	ISP_PRO_RGB_GAIN2_BLOCK,
};

struct isp_addr {
	uint32_t chn0;
	uint32_t chn1;
	uint32_t chn2;
};

struct isp_pitch{
	uint32_t chn0;
	uint32_t chn1;
	uint32_t chn2;
};

struct isp_blc_rb {
	uint32_t r;
	uint32_t b;
	uint32_t gr;
	uint32_t gb;
};

struct isp_aem_statistics {
	uint32_t val[ISP_AEM_ITEM];
};

struct isp_raw_aem_statistics {
	uint32_t r[ISP_RAW_AEM_ITEM];
	uint32_t g[ISP_RAW_AEM_ITEM];
	uint32_t b[ISP_RAW_AEM_ITEM];
};

struct isp_awbm_statistics {
	uint32_t r[ISP_AWBM_ITEM];
	uint32_t g[ISP_AWBM_ITEM];
	uint32_t b[ISP_AWBM_ITEM];
};

struct isp_raw_awbm_statistics {
	uint32_t num0;
	uint32_t num1;
	uint32_t num2;
	uint32_t num3;
	uint32_t num4;
	uint32_t num_t;
	uint32_t block_r;
	uint32_t block_g;
	uint32_t block_b;
};

struct isp_awbc_rgb {
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct isp_bpc_common {
	uint32_t pattern_type;
	uint32_t detect_thrd;
	uint32_t super_bad_thrd;
};

struct isp_bpc_thrd {
	uint32_t flat;
	uint32_t std;
	uint32_t texture;
};

struct isp_grgb_thrd {
	uint32_t edge;
	uint32_t diff;
};

struct isp_cfa_thrd {
	uint32_t edge;
	uint32_t ctrl;
};

struct isp_cmc_matrix_tab {
	uint16_t val[ISP_CMC_MATRIX_TAB_MAX];
	uint16_t reserved;
};

struct isp_gamma_node {
	uint32_t val[ISP_GAMMA_NODE_MAX];
};

struct isp_cce_matrix_tab {
	uint16_t matrix[ISP_CCE_MATRIX_TAB_MAX];
	uint16_t reserved;
};

struct isp_cce_shift {
	uint32_t y_shift;
	uint32_t u_shift;
	uint32_t v_shift;
};

struct isp_cce_uvd {
	uint8_t uvd[ISP_CCE_UVD_NUM];
	uint8_t reserved;
};

struct isp_cce_uvc {
	uint8_t uvc0[ISP_CCE_UVC0_NUM];
	uint8_t reserved0;
	uint8_t reserved1;
	uint8_t uvc1[ISP_CCE_UVC1_NUM];
	uint8_t reserved2;
};

struct isp_prefilter_thrd {
	uint32_t y_thrd;
	uint32_t u_thrd;
	uint32_t v_thrd;
};

struct isp_hist_ratio {
	uint32_t low_ratio;
	uint32_t high_ratio;
};

struct isp_hist_maxmin {
	uint32_t in_min;
	uint32_t in_max;
	uint32_t out_min;
	uint32_t out_max;
};

struct isp_aca_maxmin {
	uint32_t in_min;
	uint32_t in_max;
	uint32_t out_min;
	uint32_t out_max;
};

struct isp_aca_adjust {
	uint32_t diff;
	uint32_t small;
	uint32_t big;
};

struct isp_img_coord {
	uint32_t start_x;
	uint32_t start_y;
	uint32_t end_x;
	uint32_t end_y;
};

struct isp_afm_statistic {
	uint32_t val;
};

struct isp_edge_thrd {
	uint32_t detail;
	uint32_t smooth;
	uint32_t strength;
};

struct isp_css_thrd {
	uint8_t lower_thrd[ISP_CSS_LOWER_NUM];
	uint8_t luma_thrd;
	uint8_t lower_sum_thrd[ISP_CSS_LOWER_SUM_NUM];
	uint8_t chroma_thrd;
};

struct isp_css_ratio {
	uint8_t ratio[ISP_CSS_RATIO_NUM];
};

struct isp_coord {
	uint32_t start_x;
	uint32_t start_y;
	uint32_t end_x;
	uint32_t end_y;
};

struct isp_scaling_ratio {
	uint8_t vertical;
	uint8_t horizontal;
};

struct isp_b4awb_phys {
	uint32_t phys0;
	uint32_t phys1;
};

struct isp_hdr_rgb_index {
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct isp_hdr_tab {
	uint8_t com[ISP_HDR_COMP_ITEM * 4];
	uint8_t p2e[ISP_HDR_P2E_ITEM * 4];
	uint8_t e2p[ISP_HDR_E2P_ITEM * 4];
};

struct isp_nlc_r_node {
	uint16_t r_node[ISP_NLC_R_NODE_NUM];
	uint16_t reserved;
};

struct isp_nlc_g_node {
	uint16_t g_node[ISP_NLC_G_NODE_NUM];
	uint16_t reserved;
};

struct isp_nlc_b_node {
	uint16_t b_node[ISP_NLC_B_NODE_NUM];
	uint16_t reserved;
};

struct isp_nlc_l_node {
	uint16_t l_node[ISP_NLC_L_NODE_NUM];
	uint16_t reserved;
};

struct isp_bayer_mode {
	uint32_t nlc_bayer;
	uint32_t awbc_bayer;
	uint32_t wave_bayer;
	uint32_t cfa_bayer;
	uint32_t gain_bayer;
};

struct isp_fetch_endian {
	uint32_t endian;
	uint32_t bit_recorder;
};

struct isp_rgb_gain {
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
};

struct isp_rgb_gain_offset {
	uint32_t r_offset;
	uint32_t g_offset;
	uint32_t b_offset;
};

struct isp_ygamma_xnode {
	uint8_t x_node[ISP_YIQ_YGAMMA_XNODE_NUM];
};

struct isp_ygamma_ynode {
	uint8_t y_node[ISP_YIQ_YGAMMA_YNODE_NUM];
	uint8_t reserved0;
	uint8_t reserved1;
};

struct isp_ygamma_node_index {
	uint8_t node_index[ISP_YIQ_YGAMMA_NODE_INDEX_NUM];
	uint8_t reserved0;
	uint8_t reserved1;
	uint8_t reserved2;
};

struct isp_dev_fetch_info {
	uint32_t bypass;
	uint32_t substract;
	struct isp_addr addr;
	struct isp_pitch pitch;
	uint32_t mipi_word_num;
	uint32_t mipi_byte_rel_pos;
};

struct isp_dev_blc_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t r;
	uint32_t b;
	uint32_t gr;
	uint32_t gb;
};

struct isp_dev_lsc_info {
	uint32_t bypass;
	uint32_t grid_pitch;
	uint32_t grid_mode;
	uint32_t endian;
	uint32_t buf_addr[2]; //compatible with 64bit cpu
	uint32_t buf_len;
};

struct isp_dev_awb_info {
	/*AWBM*/
	uint32_t awbm_bypass;
	uint32_t mode;
	uint32_t skip_num;
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t win_w;
	uint32_t win_h;
	uint32_t shift;
	/*AWBC*/
	uint32_t awbc_bypass;
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
	uint32_t r_thrd;
	uint32_t g_thrd;
	uint32_t b_thrd;
	uint32_t r_offset;
	uint32_t g_offset;
	uint32_t b_offset;
};

struct isp_dev_bpc_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t flat_thrd;
	uint32_t std_thrd;
	uint32_t texture_thrd;
	uint32_t map_addr;
};

struct isp_dev_wavelet_denoise_info {
	uint32_t bypass;
	uint32_t diswei_level;
	uint32_t ranwei_level;
};

struct isp_dev_grgb_info {
	uint32_t bypass;
	uint32_t edge_thrd;
	uint32_t diff_thrd;
};

struct isp_dev_cfa_info {
	uint32_t edge_thrd;
	uint32_t ctrl_thrd;
};

struct isp_dev_cmc_info {
	uint32_t bypass;
	uint16_t matrix[ISP_CMC_MATRIX_TAB_MAX];
	uint16_t reserved;
};

struct isp_dev_gamma_info {
	uint32_t bypass;
	uint32_t node[ISP_GAMMA_NODE_MAX];
};

struct isp_dev_cce_matrix_info {
	uint32_t mode;
	uint16_t matrix[ISP_CCE_MATRIX_TAB_MAX];
	uint16_t reserved;
	uint32_t y_shift;
	uint32_t u_shift;
	uint32_t v_shift;
};

struct isp_dev_cce_uv_info {
	uint32_t bypass;
	uint32_t mode;
	uint8_t uvd[ISP_CCE_UVD_NUM];
	uint8_t reserved0;
	uint8_t uvc0[ISP_CCE_UVC0_NUM];
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t uvc1[ISP_CCE_UVC1_NUM];
	uint8_t reserved3;
};

struct isp_dev_prefilter_info {
	uint32_t bypass;
	uint32_t writeback;
	uint32_t y_thrd;
	uint32_t u_thrd;
	uint32_t v_thrd;
};

struct isp_dev_hist_info {
	uint32_t bypass;
	uint32_t auto_rst_disable;
	uint32_t mode;
	uint32_t low_sum_ratio;
	uint32_t high_sum_ratio;
	uint32_t in_min;
	uint32_t in_max;
	uint32_t out_min;
	uint32_t out_max;
	uint32_t clr_eb;
};

struct isp_dev_aca_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t in_min;
	uint32_t in_max;
	uint32_t out_min;
	uint32_t out_max;
	uint32_t diff_thrd;
	uint32_t small_thrd;
	uint32_t big_thrd;
};

struct isp_dev_afm_info {
	uint32_t bypass;
	uint32_t shift;
	uint32_t mode;
	uint32_t skip_num;
	uint32_t skip_num_clr;
	struct isp_img_coord win[ISP_AFM_WIN_NUM];
};

struct isp_dev_edge_info {
	uint32_t bypass;
	uint32_t detail_thrd;
	uint32_t smooth_thrd;
	uint32_t strength;
};

struct isp_dev_emboss_info {
	uint32_t bypass;
	uint32_t step;
};

struct isp_dev_fcs_info {
	uint32_t bypass;
	uint32_t mode;
};

struct isp_dev_css_info {
	uint32_t bypass;
	uint8_t lower_thrd[ISP_CSS_LOWER_NUM];
	uint8_t luma_thrd;
	uint8_t lower_sum_thrd[ISP_CSS_LOWER_SUM_NUM];
	uint8_t chroma_thrd;
	uint8_t ratio[ISP_CSS_RATIO_NUM];
};

struct isp_dev_csa_info {
	uint32_t bypass;
	uint32_t factor;
};

struct isp_dev_store_info {
	uint32_t bypass;
	uint32_t substract;
	struct isp_addr addr;
	struct isp_pitch pitch;
};

struct isp_dev_feeder_info {
	uint32_t data_type;
};

struct isp_dev_hdr_info {
	uint32_t bypass;
	uint32_t level;
	uint32_t r_index;
	uint32_t g_index;
	uint32_t b_index;
	uint8_t com[ISP_HDR_COMP_ITEM * 4];
	uint8_t p2e[ISP_HDR_P2E_ITEM * 4];
	uint8_t e2p[ISP_HDR_E2P_ITEM * 4];
};

struct isp_dev_nlc_info {
	uint32_t bypass;
	uint16_t r_node[ISP_NLC_R_NODE_NUM];
	uint16_t reserved0;
	uint16_t g_node[ISP_NLC_G_NODE_NUM];
	uint16_t reserved1;
	uint16_t b_node[ISP_NLC_B_NODE_NUM];
	uint16_t reserved2;
	uint16_t l_node[ISP_NLC_L_NODE_NUM];
	uint16_t reserved3;
};

struct isp_dev_nawbm_info {
	uint32_t bypass;
};

struct isp_dev_pre_wavelet_info {
	uint32_t bypass;
};

struct isp_dev_binning4awb_info {
	uint32_t bypass;
};

struct isp_dev_glb_gain_info {
	uint32_t bypass;
	uint32_t gain;
};

struct isp_dev_rgb_gain_info {
	uint32_t bypass;
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
	uint32_t r_offset;
	uint32_t g_offset;
	uint32_t b_offset;
};

struct isp_dev_yiq_ygamma_info {
	uint32_t bypass;
	uint8_t x_node[ISP_YIQ_YGAMMA_XNODE_NUM];
	uint8_t y_node[ISP_YIQ_YGAMMA_YNODE_NUM];
	uint8_t reserved0;
	uint8_t reserved1;
	uint8_t node_index[ISP_YIQ_YGAMMA_NODE_INDEX_NUM];
	uint8_t reserved2;
	uint8_t reserved3;
	uint8_t reserved4;
};

struct isp_dev_yiq_ae_info {
	uint32_t bypass;
	uint32_t src_sel;
	uint32_t mode;
	uint32_t skip_num;
};

struct isp_dev_yiq_flicker_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t v_height;
	uint32_t line_counter;
	uint32_t line_step;
	uint32_t line_start;
};

struct isp_dev_hue_info {
	uint32_t bypass;
	uint32_t theta;
};

struct isp_dev_nbpc_info {
	uint32_t bypass;
};


/*********************Tshark2**********************/
#define ISP_PINGPANG_CTM_NUM 729
#define ISP_CTM_PARAM_NUM   (729 * 4)
#define ISP_PINGPANG_HSV_NUM 361
#define ISP_HSV_PARAM_NUM (361 * 2)
#define ISP_PINGPANG_FRGB_GAMC_NODE 129
#define ISP_PINGPANG_FRGB_GAMC_NUM 257
#define ISP_PINGPANG_YUV_YGAMMA_NUM 129
#define ISP_VST_IVST_NUM 1024
#define ISP_AFM_WIN_NUM_V1 25
#define ISP_AFM_WIN_NUM_R6P9 10

enum isp_dev_capability {
	ISP_CAPABILITY_CHIP_ID,
	ISP_CAPABILITY_SINGLE_SIZE,
	ISP_CAPABILITY_CONTINE_SIZE,
	ISP_CAPABILITY_AWB_WIN,
	ISP_CAPABILITY_AWB_DEFAULT_GAIN,
	ISP_CAPABILITY_AF_MAX_WIN_NUM,
	ISP_CAPABILITY_TIME,
};

enum isp_int_property {
	ISP_PRO_INT_STATUS,
	ISP_PRO_INT_EN0,
	ISP_PRO_INT_CLR0,
	ISP_PRO_INT_RAW0,
	ISP_PRO_INT_INT0,
	ISP_PRO_INT_EN1,
	ISP_PRO_INT_CLR1,
	ISP_PRO_INT_RAW1,
	ISP_PRO_INT_INT1,
	ISP_PRO_INT_EN2,
	ISP_PRO_INT_CLR2,
	ISP_PRO_INT_RAW2,
	ISP_PRO_INT_INT2,
	ISP_PRO_INT_EN3,
	ISP_PRO_INT_CLR3,
	ISP_PRO_INT_RAW3,
	ISP_PRO_INT_INT3,
	ISP_PRO_INT_ALL_DONE_CTRL,
};

enum isp_dispatch_property {
	ISP_PRO_DISPATCH_BLOCK,
	ISP_PRO_DISPATCH_CH0_BAYER,
	ISP_PRO_DISPATCH_CH1_BAYER,
	ISP_PRO_DISPATCH_CH0_SIZE,
	ISP_PRO_DISPATCH_CH1_SIZE,
};

enum isp_arbiter_property {
	ISP_PRO_ARBITER_BLOCK,
	ISP_PRO_ARBITER_WR_STATUS,
	ISP_PRO_ARBITER_RD_STATUS,
	ISP_PRO_ARBITER_PARAM,
	ISP_PRO_ARBITER_ENDIAN_CH0,
	ISP_PRO_ARBITER_ENDIAN_CH1,
	ISP_PRO_ARBITER_CTRL,
};

enum isp_axi_property {
	ISP_PRO_AXI_BLOCK,
	ISP_PRO_AXI_WR_MASTER_STATUS,
	ISP_PRO_AXI_RD_MASTER_STATUS,
	ISP_PRO_AXI_ITI2AXIM_CTRL,
	ISP_PRO_AXI_CONVERT_WR_CTRL,
};

enum isp_raw_sizer_property {
	ISP_PRO_RAW_SIZER_BLOCK,
	ISP_PRO_RAW_SIZER_BYPASS,
	ISP_PRO_RAW_SIZER_BPC_BYPASS,
	ISP_PRO_RAW_SIZER_CROP_BYPASS,
	ISP_PRO_RAW_SIZER_CROP_SRC,
	ISP_PRO_RAW_SIZER_CROP_DST,
	ISP_PRO_RAW_SIZER_CROP_START,
	ISP_PRO_RAW_SIZER_DST,
	ISP_PRO_RAW_SIZER_BPC_SHIFT,
	ISP_PRO_RAW_SIZER_BPC_MULTI,
	ISP_PRO_RAW_SIZER_BPC_MIN_DIFF,
	ISP_PRO_RAW_SIZER_HCOEFF,
	ISP_PRO_RAW_SIZER_VCOEFF,
	ISP_PRO_RAW_SIZER_H_INIT_PARA,
	ISP_PRO_RAW_SIZER_V_INIT_PARA,
};

enum isp_common_property_v1 {
	ISP_PRO_COMMON_BLOCK,
	ISP_PRO_COMMON_VERSION,
	ISP_PRO_COMMON_STATUS0,
	ISP_PRO_COMMON_STATUS1,
	ISP_PRO_COMMON_CH0_FETCH_SEL,
	ISP_PRO_COMMON_CH0_SIZER_SEL,
	ISP_PRO_COMMON_CH0_STORE_SEL,
	ISP_PRO_COMMON_CH1_FETCH_SEL,
	ISP_PRO_COMMON_CH1_SIZER_SEL,
	ISP_PRO_COMMON_CH1_STORE_SEL,
	ISP_PRO_COMMON_FETCH_COLOR_SPACE_SEL,
	ISP_PRO_COMMON_STORE_COLOR_SPACE_SEL,
	ISP_PRO_COMMON_AWBM_POS_SEL,
	ISP_PRO_COMMON_CH0_AEM2_POS,
	ISP_PRO_COMMON_CH0_Y_AFM_POS,
	ISP_PRO_COMMON_CH1_AEM2_POS,
	ISP_PRO_COMMON_CH1_Y_AFM_POS,
	ISP_PRO_COMMON_LBUF_OFFSET,
	ISP_PRO_COMMON_SHADOW_ALL_CTRL,
	ISP_PRO_COMMON_AWBM_SHADOW,
	ISP_PRO_COMMON_AE_SHADOW,
	ISP_PRO_COMMON_AF_SHADOW,
	ISP_PRO_COMMON_AFL_SHADOW,
	ISP_PRO_COMMON_COMM_SHADOW,
	ISP_PRO_COMMON_3A_SINGLE_FRAME_CTRL,
};

enum isp_pwd_property {
	ISP_PRO_PWD_BLOCK,
	ISP_PRO_PWD_SLICE_SIZE,
};

enum isp_awbm_property {
	ISP_PRO_AWBM_BLOCK,
};

enum isp_awbc_property {
	ISP_PRO_AWBC_BLOCK,
};

enum isp_grgb_property_v1 {
	ISP_PRO_GRGB_BLOCK_v1,
};

enum isp_nlm_property {
	ISP_PRO_VST_BLOCK,
	ISP_PRO_NLM_BLOCK,
	ISP_PRO_IVST_BLOCK,
};

enum isp_wdr_property {
	ISP_PRO_WDR_BLOCK,
};

enum isp_cmc8_property {
	ISP_PRO_CMC8_BLOCK,
};

enum isp_hsv_property {
	ISP_PRO_HSV_BLOCK,
};

enum isp_pre_cdn_rgb_property {
	ISP_PRO_PRE_CDN_RGB_BLOCK,
};

enum isp_rgb_afm_property {
	ISP_PRO_RGB_AFM_BLOCK,
	ISP_PRO_RGB_AFM_FRAME_SIZE,
	ISP_PRO_RGB_AFM_TYPE1_STATISTIC,
	ISP_PRO_RGB_AFM_TYPE2_STATISTIC,
	ISP_PRO_RGB_AFM_BYPASS,
	ISP_PRO_RGB_AFM_MODE,
	ISP_PRO_RGB_AFM_SKIP_NUM,
	ISP_PRO_RGB_AFM_SKIP_NUM_CLR,
	ISP_PRO_RGB_AFM_SPSMD_RTGBOT_ENABLE,
	ISP_PRO_RGB_AFM_SPSMD_DIAGONAL_ENABLE,
	ISP_PRO_RGB_AFM_SPSMD_CAL_MOD,
	ISP_PRO_RGB_AFM_SEL_FILTER1,
	ISP_PRO_RGB_AFM_SEL_FILTER2,
	ISP_PRO_RGB_AFM_SOBEL_TYPE,
	ISP_PRO_RGB_AFM_SPSMD_TYPE,
	ISP_PRO_RGB_AFM_SOBEL_THRESHOLD,
	ISP_PRO_RGB_AFM_SPSMD_THRESHOLD,
	ISP_PRO_RGB_AFM_FRAME_RANGE,
	ISP_PRO_RGB_AFM_WIN,
	ISP_PRO_RGB_AFM_WIN_NUM,

/*isp ver:r6p9 only, such as tshark3*/
	ISP_PRO_RGB_AFM_STATISTIC,
	ISP_PRO_RGB_AFM_SPSMD_SQUARE_ENABLE,
	ISP_PRO_RGB_AFM_OVERFLOW_PROTECT,
	ISP_PRO_RGB_AFM_SUBFILTER,
	ISP_PRO_RGB_AFM_SPSMD_TOUCH_MODE,
	ISP_PRO_RGB_AFM_SHFIT,
	ISP_PRO_RGB_AFM_THRESHOLD_RGB,
};

enum isp_rgb2y_property {
	ISP_PRO_RGB2Y_BLOCK,
};

enum isp_yiq_aem_property {
	ISP_PRO_YIQ_AEM_BLOCK,
	ISP_PRO_YIQ_AEM_YGAMMA_BYPASS,
	ISP_PRO_YIQ_AEM_BYPASS,
	ISP_PRO_YIQ_AEM_STATISTICS,
	ISP_PRO_YIQ_AEM_SKIP_NUM,
	ISP_PRO_YIQ_AEM_OFFSET,
	ISP_PRO_YIQ_AEM_BLK_SIZE,
	ISP_PRO_YIQ_AEM_SLICE_SIZE,
};

enum isp_anti_flicker_property {
	ISP_PRO_ANTI_FLICKER_BLOCK,
	ISP_PRO_ANTI_FLICKER_STATISTIC,
	ISP_PRO_ANTI_FLICKER_BYPASS,
};

enum isp_yiq_afm_property {
	ISP_PRO_YIQ_AFM_BLOCK,
	ISP_PRO_YIQ_AFM_SLICE_SIZE,
	ISP_PRO_YIQ_AFM_WIN,
	ISP_PRO_YIQ_AFM_WIN_NUM,
	ISP_PRO_YIQ_AFM_STATISTIC,
};

enum isp_prefilter_property_v1 {
	ISP_PRO_PREF_BLOCK_V1,
};

enum isp_uv_prefilter_property {
	ISP_PRO_UV_PREFILTER_BLOCK,
};

enum isp_hist2_property {
	ISP_PRO_HIST2_BLOCK,
};

enum isp_iircnr_property {
	ISP_PRO_IIRCNR_BLOCK,
	ISP_PRO_YRANDOM_BLOCK,
};

enum isp_post_cdn_property {
	ISP_PRO_POST_CDN_BLOCK,
};

enum isp_ygamma_property {
	ISP_PRO_YGAMMA_BLOCK,
};

enum isp_ydelay_property {
	ISP_PRO_YDELAY_BLOCK,
};

enum isp_yuv_nlm_property {
	ISP_PRO_YUV_NLM_BLOCK,
	ISP_PRO_YUV_NLM_SLICE_SIZE,
};

enum isp_pingpang_property {
	ISP_PRO_PINGPANG_CTM_BLOCK,
	ISP_PRO_PINGPANG_HSV_BLOCK,
	ISP_PRO_PINGPANG_FRGB_GAMC_BLOCK,
};

struct isp_memory {
	unsigned long addr;
	uint32_t len;
};

struct isp_reg_bits {
	unsigned long reg_addr;
	unsigned long reg_value;
};

struct isp_reg_param {
	unsigned long reg_param;
	uint32_t counts;
};

struct isp_time {
	uint32_t sec;
	uint32_t usec;
};

struct isp_irq {
	uint32_t irq_val0;
	uint32_t irq_val1;
	uint32_t irq_val2;
	uint32_t irq_val3;
	uint32_t reserved;
	int32_t ret_val;
	struct isp_time time;
};

struct isp_interrupt {
	uint32_t isp_id;
	uint32_t int_mode;
};

struct isp_capability {
	uint32_t isp_id;
	uint32_t index;
	void * property_param;
};

struct isp_io_param {
	uint32_t isp_id;
	uint32_t sub_block;
	uint32_t property;
	void * property_param;
};

struct isp_img_offset {
	uint32_t x;
	uint32_t y;
};

struct isp_img_size {
	uint32_t width;
	uint32_t height;
};

struct img_offset {
	uint32_t x;
	uint32_t y;
};

struct isp_lsc_addr {
	uint32_t phys_addr;
	uint32_t virt_addr;
	uint32_t buf_len;
};

//*nlc*//
struct nlc_node {
	uint16_t r_node[29];
	uint16_t reserved0;
	uint16_t g_node[29];
	uint16_t reserved1;
	uint16_t b_node[29];
	uint16_t reserved2;
	uint16_t l_node[27];
	uint16_t reserved3;
};

//*awbm*//
struct awbm_rect_pos {
	uint32_t start_x[5];
	uint32_t start_y[5];
	uint32_t end_x[5];
	uint32_t end_y[5];
};

struct awbm_circle_pos {
	uint32_t x[5];
	uint32_t y[5];
	uint32_t r[5];
};

struct awbm_pixel_num {
	uint32_t pixel_num[5];
};

struct awbm_thr {
	uint32_t r_high;
	uint32_t r_low;
	uint32_t g_high;
	uint32_t g_low;
	uint32_t b_high;
	uint32_t b_low;
};

//**awbc//
struct awbc_param {
	uint32_t r;
	uint32_t b;
	uint32_t gr;
	uint32_t gb;
};

struct awbc_rgb {
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct sobel_thrd {
    uint32_t min;
    uint32_t max;
};

struct spsmd_thrd {
    uint32_t min;
    uint32_t max;
};

struct afm_subfilter {
	uint32_t average;
	uint32_t median;
};

struct afm_shift {
	uint32_t shift_spsmd;
	uint32_t shift_sobel5;
	uint32_t shift_sobel9;
};

struct afm_thrd_rgb {
	uint32_t sobel5_thr_min_red;
	uint32_t sobel5_thr_max_red;
	uint32_t sobel5_thr_min_green;
	uint32_t sobel5_thr_max_green;
	uint32_t sobel5_thr_min_blue;
	uint32_t sobel5_thr_max_blue;
	uint32_t sobel9_thr_min_red;
	uint32_t sobel9_thr_max_red;
	uint32_t sobel9_thr_min_green;
	uint32_t sobel9_thr_max_green;
	uint32_t sobel9_thr_min_blue;
	uint32_t sobel9_thr_max_blue;
	uint32_t spsmd_thr_min_red;
	uint32_t spsmd_thr_max_red;
	uint32_t spsmd_thr_min_green;
	uint32_t spsmd_thr_max_green;
	uint32_t spsmd_thr_min_blue;
	uint32_t spsmd_thr_max_blue;
};

struct cce_shift {
	uint32_t y_shift;
	uint32_t u_shift;
	uint32_t v_shift;
};

struct gamma_node {
	uint32_t val[66];
};

//*cmc8 and cmc10*//
struct cmc_matrix {
	uint16_t val[9];
	uint16_t reserved;
};

struct coordinate_xy {
	 int16_t node_x;
	 int16_t node_y;
};

struct csa_factor_v1 {
	uint32_t factor_u;
	uint32_t factor_v;
};

struct isp_raw_afm_statistic {
	uint32_t val[ISP_RAW_AFM_ITEM];
};

struct isp_raw_afm_statistic_r6p9 {
	uint32_t val[ISP_RAW_AFM_ITEM_R6P9];
};

struct isp_yiq_afm_statistic {
	uint32_t val[ISP_YIQ_AFM_ITEM];
};

struct arbiter_endian_v1 {
	uint32_t bpc_endian;
	uint32_t lens_endian;
	uint32_t store_endian;
	uint32_t fetch_bit_reorder;
	uint32_t fetch_endian;
};

struct arbiter_param_v1 {
	uint32_t pause_cycle;
	uint32_t reset;
};


struct isp_dev_pre_glb_gain_info {
	uint32_t bypass;
	uint32_t gain;
};

struct isp_dev_rgb_gain_info_v1 {
	uint32_t bypass;
	uint32_t global_gain;
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
};

struct isp_dev_pre_wavelet_info_v1 {
	uint32_t bypass;
	uint32_t radial_bypass;
	uint32_t gain_thrs0;
	uint32_t gain_thrs1;
	uint32_t bitshift0;
	uint32_t bitshift1;
	uint32_t offset;
	uint32_t nsr_slope;
	uint32_t lum_ratio;
	uint32_t center_pos_x;
	uint32_t center_pos_y;
	uint32_t delta_x2;
	uint32_t delta_y2;
	uint32_t r2_thr;
	uint32_t p_param1;
	uint32_t p_param2;
	uint32_t addback;
	uint32_t gain_max_thr;
	uint32_t pos_x;
	uint32_t pos_y;
	uint32_t lum_shink_level;
};

struct isp_dev_nlc_info_v1 {
	uint32_t bypass;
	struct nlc_node node;
};

struct isp_dev_2d_lsc_info {
	uint32_t bypass;
	uint32_t buf_sel;
	uint32_t grid_address;
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t loader_enable;
	uint32_t grid_pitch;
	uint32_t grid_width;
	uint32_t grid_x_num;
	uint32_t grid_y_num;
	uint32_t grid_num_t;
	uint32_t load_buf_sel;
	uint32_t load_chn_sel;
	uint32_t endian;
	struct isp_img_size slice_size;
	uint32_t relative_x;
	uint32_t relative_y;
	uint32_t q_value[2][5];
	uint32_t buf_addr[2]; //compatible with 64bit cpu
	uint32_t buf_len;
};

struct isp_dev_1d_lsc_info {
	uint32_t bypass;
	uint32_t gain_max_thr;
	uint32_t center_r0c0_row_y;
	uint32_t center_r0c0_col_x;
	uint32_t center_r0c1_row_y;
	uint32_t center_r0c1_col_x;
	uint32_t center_r1c0_row_y;
	uint32_t center_r1c0_col_x;
	uint32_t center_r1c1_row_y;
	uint32_t center_r1c1_col_x;
	uint32_t delta_square_r0c0_x;
	uint32_t delta_square_r0c0_y;
	uint32_t delta_square_r0c1_x;
	uint32_t delta_square_r0c1_y;
	uint32_t delta_square_r1c0_x;
	uint32_t delta_square_r1c0_y;
	uint32_t delta_square_r1c1_x;
	uint32_t delta_square_r1c1_y;
	uint32_t coef_r0c0_p1;
	uint32_t coef_r0c0_p2;
	uint32_t coef_r0c1_p1;
	uint32_t coef_r0c1_p2;
	uint32_t coef_r1c0_p1;
	uint32_t coef_r1c0_p2;
	uint32_t coef_r1c1_p1;
	uint32_t coef_r1c1_p2;
	struct img_offset start_pos;
};

struct isp_dev_binning4awb_info_v1 {
	uint32_t bypass;
	uint32_t endian;
	uint32_t burst_mode;
	uint32_t vx;
	uint32_t mem_fifo_clr;
	uint32_t hx;
	uint32_t mem_addr;
	uint32_t pitch;
};

struct isp_dev_awbm_info_v1 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_num;
	struct img_offset block_offset;
	struct isp_img_size block_size;
	uint32_t shift;
	uint32_t thr_bypass;
	struct awbm_rect_pos rect_pos;
	struct awbm_circle_pos circle_pos;
	struct awbm_rect_pos clctor_pos;
	struct awbm_pixel_num pix_num;
	struct awbm_thr thr;
	struct isp_img_size slice_size;
	uint32_t skip_num_clear;
	uint32_t mem_addr;
};

struct isp_dev_awbc_info_v1 {
	uint32_t bypass;
	uint32_t alpha_bypass;
	uint32_t alpha_value;
	uint32_t buf_sel;
	struct awbc_param gain;
	struct awbc_rgb thrd;
	struct awbc_param gain_offset;
	struct awbc_param gain_buff;
	struct awbc_param gain_offset_buff;
};

struct isp_dev_awb_info_v1 {
	/*AWBM*/
	uint32_t awbm_bypass;
	uint32_t mode;
	uint32_t skip_num;
	struct img_offset block_offset;
	struct isp_img_size block_size;
	uint32_t shift;
	uint32_t thr_bypass;
	struct awbm_rect_pos rect_pos;
	struct awbm_circle_pos circle_pos;
	struct awbm_rect_pos clctor_pos;
	struct awbm_pixel_num pix_num;
	struct awbm_thr thr;
	struct isp_img_size slice_size;
	uint32_t skip_num_clear;
	uint32_t mem_addr;
	/*AWBC*/
	uint32_t awbc_bypass;
	uint32_t alpha_bypass;
	uint32_t alpha_value;
	uint32_t buf_sel;
	struct awbc_param gain;
	struct awbc_rgb thrd;
	struct awbc_param gain_offset;
	struct awbc_param gain_buff;
	struct awbc_param gain_offset_buff;
};

struct isp_dev_awb_info_v2 {
	/*AWBM*/
	uint32_t awbm_bypass;
	uint32_t mode;
	uint32_t skip_num;
	struct img_offset block_offset;
	struct isp_img_size block_size;
	uint32_t shift;
	uint32_t thr_bypass;
	struct awbm_rect_pos rect_pos;
	struct awbm_circle_pos circle_pos;
	struct awbm_rect_pos clctor_pos;
	struct awbm_pixel_num pix_num;
	struct awbm_thr thr;
	struct isp_img_size slice_size;
	uint32_t skip_num_clear;
	uint32_t position_sel;
	uint32_t mem_addr;
	/*AWBC*/
	uint32_t awbc_bypass;
	uint32_t alpha_bypass;
	uint32_t alpha_value;
	uint32_t buf_sel;
	struct awbc_param gain;
	struct awbc_rgb thrd;
	struct awbc_param gain_offset;
	struct awbc_param gain_buff;
	struct awbc_param gain_offset_buff;
};


struct isp_dev_raw_aem_info {
	uint32_t bypass;
	uint32_t shift;
	uint32_t mode;
};

struct isp_dev_bpc_info_v1 {
	uint32_t bypass;
	uint32_t pvd_bypass;
	uint32_t bpc_mode;
	uint32_t mask_mode;
	uint32_t kmin;
	uint32_t kmax;
	uint32_t cntr_threshold;
	uint32_t bad_map_hw_fifo_clr_en;
	uint32_t ktimes;
	uint32_t bpc_map_fifo_clr;
	uint32_t delta;
	uint32_t bad_pixel_num;
	uint32_t flat_factor;
	uint32_t safe_factor;
	uint32_t spike_coeff;
	uint32_t dead_coeff;
	uint16_t intercept_b[8];
	uint16_t slope_k[8];
	uint16_t lut_level[8];
	uint32_t new_old_sel;
	uint32_t map_done_sel;
	uint32_t bpc_map_addr_new;
	uint32_t level;
};

struct isp_dev_bdn_info_v1 {
	uint32_t bypass;
	uint32_t radial_bypass;
	uint32_t addback;
	uint32_t dis[10][2];
	uint32_t ran[10][8];
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t squ_x2;
	uint32_t squ_y2;
	uint32_t coef;
	uint32_t coef2;
	uint32_t start_pos_x;
	uint32_t start_pos_y;
	uint32_t offset;
	uint32_t dis_level;
	uint32_t ran_level;
};

struct isp_dev_grgb_info_v1 {
	uint32_t bypass;
	uint32_t edge;
	uint32_t diff;
	uint32_t grid;
};

struct isp_dev_rgb_gain2_info {
	uint32_t bypass;
	uint32_t r_gain;
	uint32_t g_gain;
	uint32_t b_gain;
	uint32_t r_offset;
	uint32_t g_offset;
	uint32_t b_offset;
};

struct isp_dev_vst_info_v1 {
	uint32_t bypass;
	uint32_t buf_sel;
};

struct isp_dev_nlm_info_v1 {
	uint32_t bypass;
	uint32_t imp_opt_bypass;
	uint32_t flat_opt_bypass;
	uint32_t flat_thr_bypass;
	uint32_t direction_mode_bypass;
	uint32_t buf_sel;
	uint8_t strength[5];
	uint8_t cnt[5];
	uint16_t thresh[5];
	uint32_t streng_th;
	uint32_t texture_dec;
	//uint32_t is_flat;
	uint32_t addback;
	uint32_t opt_mode;
	uint32_t dist_mode;
	uint8_t w_shift[3];
	uint8_t reserved;
	uint32_t cnt_th;
	uint32_t tdist_min_th;
	uint32_t diff_th;
	uint16_t lut_w[72];
	//void *vst_addr;
	uint32_t vst_addr[2]; //compatible with 64bit cpu
	uint32_t vst_len;
	//void *ivst_addr;
	uint32_t ivst_addr[2]; //compatible with 64bit cpu
	uint32_t ivst_len;
	//void *nlm_addr;
	uint32_t nlm_addr[2]; //compatible with 64bit cpu
	uint32_t nlm_len;
	uint32_t strength_level;
};

struct isp_dev_nlm_info_v2 {
	uint32_t bypass;
	uint32_t imp_opt_bypass;
	uint32_t flat_opt_bypass;
//	uint32_t flat_thr_bypass;
//	uint32_t direction_mode_bypass;
	uint32_t buf_sel;
	uint8_t strength[5];
	uint8_t cnt[5];
	uint16_t thresh[5];
	uint32_t streng_th;
	uint32_t texture_dec;
	uint32_t is_flat;
	uint32_t addback;
//	uint32_t opt_mode;
//	uint32_t dist_mode;
//	uint32_t w_shift[3];
//	uint32_t cnt_th;
//	uint32_t tdist_min_th;
//	uint32_t diff_th;
	uint16_t lut_w[72];
	uint32_t vst_addr[2]; //compatible with 64bit cpu
	uint32_t vst_len;
	uint32_t ivst_addr[2]; //compatible with 64bit cpu
	uint32_t ivst_len;
	uint32_t nlm_addr[2]; //compatible with 64bit cpu
	uint32_t nlm_len;
	uint32_t strength_level;

};


struct isp_dev_ivst_info_v1 {
	uint32_t bypass;
	uint32_t buf_sel;
};

struct isp_dev_cfa_info_v1 {
	uint32_t  bypass;
	uint32_t  ee_bypass;
	uint32_t  doee_base;
	uint32_t  gbuf_addr_max;
	uint32_t  avg_mode;
	uint32_t  grid_gain;
	uint32_t  cfa_uni_dir_intplt_tr ;
	uint32_t  cfai_ee_uni_dir_tr;
	uint32_t  cfai_ee_edge_tr;
	uint32_t  cfai_ee_diagonal_tr;
	uint32_t  cfai_ee_grid_tr;
	uint32_t  cfai_doee_clip_tr;
	uint32_t  cfai_ee_saturation_level;
	uint32_t  plt_diff_tr;
	uint32_t  grid_min_tr ;
	uint32_t  strength_tr_neg;
	uint32_t  strength_tr_pos;
	uint32_t  ee_strength_neg;
	uint32_t  ee_strength_pos;
	uint32_t  inter_chl_gain;
};

struct isp_dev_cmc10_info {
	uint32_t bypass;
	uint32_t buf_sel;
	uint32_t alpha;
	uint32_t alpha_bypass;
	struct cmc_matrix matrix;
	struct cmc_matrix matrix_buf;
};

struct isp_dev_gamma_info_v1 {
	uint32_t bypass;
	struct coordinate_xy nodes[ISP_PINGPANG_FRGB_GAMC_NUM];
};

struct isp_dev_cmc8_info_v1 {
	uint32_t bypass;
	uint32_t buf_sel;
	uint32_t alpha;
	uint32_t alpha_bypass;
	struct cmc_matrix matrix;
	struct cmc_matrix matrix_buf;
};

struct isp_dev_ct_info {
	uint32_t bypass;
	//void *data_ptr;
	uint32_t data_ptr[2]; //compatible with 64bit cpu
	uint32_t size;
};

struct isp_dev_cce_info_v1 {
	uint32_t mode;
	uint16_t matrix[9];
	uint16_t reserved0;
	uint16_t y_offset;
	uint16_t u_offset;
	uint16_t v_offset;
	uint16_t reserved1;
};

struct isp_dev_cce_uvd_info {
	uint32_t uvdiv_bypass;
	uint32_t lum_th_h_len;
	uint32_t lum_th_h;
	uint32_t lum_th_l_len;
	uint32_t lum_th_l;
	uint32_t chroma_min_h;
	uint32_t chroma_min_l;
	uint32_t chroma_max_h;
	uint32_t chroma_max_l;
	uint32_t u_th1_h;
	uint32_t u_th1_l;
	uint32_t u_th0_h;
	uint32_t u_th0_l;
	uint32_t v_th1_h;
	uint32_t v_th1_l;
	uint32_t v_th0_h;
	uint32_t v_th0_l;
	uint8_t ratio[9];
	uint8_t reserved[3];
	uint32_t base;
	uint32_t level;
};

struct isp_dev_hsv_info_v1 {
	uint32_t bypass;
	uint32_t buf_sel;
	//void *data_ptr;
	uint32_t data_ptr[2]; //compatible with 64bit cpu
	uint32_t size;
};

struct isp_dev_csc_info {
	uint32_t bypass;
	uint32_t red_centre_x;
	uint32_t red_centre_y;
	uint32_t blue_centre_x;
	uint32_t blue_centre_y;
	uint32_t red_x2_init;
	uint32_t red_y2_init;
	uint32_t blue_x2_init;
	uint32_t blue_y2_init;
	uint32_t red_threshold;
	uint32_t blue_threshold;
	uint32_t red_p1_param;
	uint32_t red_p2_param;
	uint32_t blue_p1_param;
	uint32_t blue_p2_param;
	uint32_t max_gain_thr;
	struct isp_img_size img_size;
	struct img_offset start_pos;
};

struct isp_dev_pre_cdn_rgb_info {
	uint32_t bypass;
	uint32_t median_mode;
	uint32_t median_thr;
	uint32_t thru0;
	uint32_t thru1;
	uint32_t thrv0;
	uint32_t thrv1;
	uint32_t level;
};

struct isp_dev_posterize_info {
	uint32_t bypass;
	uint8_t  posterize_level_bottom[8];
	uint8_t  posterize_level_top[8];
	uint8_t  posterize_level_out[8];
};

struct isp_dev_rgb_afm_info_v1 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_num;
	uint32_t skip_num_clear;
	uint32_t spsmd_rtgbot_enable;
	uint32_t spsmd_diagonal_enable;
	uint32_t spsmd_cal_mode;
	uint32_t af_sel_filter1;
	uint32_t af_sel_filter2;
	uint32_t sobel_type;
	uint32_t spsmd_type;
	uint32_t sobel_threshold_min;
	uint32_t sobel_threshold_max;
	uint32_t spsmd_threshold_min;
	uint32_t spsmd_threshold_max;
	uint32_t frame_width;
	uint32_t frame_height;
	struct isp_coord coord[ISP_AFM_WIN_NUM_V1];

/*isp ver:r6p9 only, such as tshark3*/
	uint32_t spsmd_square_en;
	uint32_t overflow_protect_en;
	struct afm_subfilter subfilter;
	uint32_t spsmd_touch_mode;
	struct afm_shift shift;
	struct afm_thrd_rgb thrd;
};

struct isp_dev_rgb2y_info {
	uint32_t signal_sel;
};

struct isp_dev_yiq_aem_info_v1 {
	uint32_t ygamma_bypass;
	int16_t  gamma_xnode[10];
	int16_t  gamma_ynode[10];
	uint32_t gamma_node_idx[10];
	uint32_t aem_bypass;
	uint32_t aem_mode;
	uint32_t aem_skip_num;
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t width;
	uint32_t height;
};

struct isp_dev_yiq_aem_info_v2 {
	uint32_t ygamma_bypass;
	uint32_t gamma_xnode[10];
	uint32_t gamma_ynode[10];
	uint32_t gamma_node_idx[10];
	uint32_t aem_bypass;
	uint32_t aem_mode;
	uint32_t aem_skip_num;
	uint32_t offset_x;
	uint32_t offset_y;
	uint32_t avgshift;
	uint32_t width;
	uint32_t height;
};


struct isp_dev_anti_flicker_info_v1 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_frame_num;
	uint32_t line_step;
	uint32_t frame_num;
	uint32_t vheight;
	uint32_t start_col;
	uint32_t end_col;
	uint32_t addr;
};

struct isp_dev_yiq_afm_info_v1 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t source_pos;
	uint32_t shift;
	uint32_t skip_num;
	uint32_t skip_num_clear;
	uint32_t format;
	uint32_t iir_bypass;
	struct isp_coord coord[25];
	int16_t IIR_c[11];
	uint16_t reserved;
};

struct isp_dev_yiq_afm_info_v2 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t shift;
	uint32_t skip_num;
	uint32_t skip_num_clear;
	uint32_t af_position;
	uint32_t format;
	uint32_t iir_bypass;
	struct isp_coord coord[25];
	int IIR_c[11];
};


struct isp_dev_yuv_precdn_info {
	uint32_t bypass;
	uint32_t mode;
	uint32_t median_writeback_en;
	uint32_t median_mode;
	uint32_t den_stren;
	uint32_t uv_joint;
	uint8_t median_thr_u[2];
	uint8_t median_thr_v[2];
	uint32_t median_thr;
	uint32_t uv_thr;
	uint32_t y_thr;
	uint8_t r_segu[2][7];
	uint8_t r_segv[2][7];
	uint8_t r_segy[2][7];
	uint8_t r_distw[25];
	uint8_t reserved;
	uint32_t level;
};

struct isp_dev_prefilter_info_v1 {
	uint32_t bypass;
	uint32_t writeback;
	uint32_t thrd;
};

struct isp_dev_uv_prefilter_info {
	uint32_t bypass;
	uint32_t writeback;
	uint32_t nr_thr_u;
	uint32_t nr_thr_v;
};

struct isp_dev_brightness_info {
	uint32_t bypass;
	uint32_t factor;
};

struct isp_dev_contrast_info {
	uint32_t bypass;
	uint32_t factor;
};

struct isp_dev_hist_info_v1 {
	uint32_t bypass;
	uint32_t off;
	uint32_t buf_rst_en;
	uint32_t pof_rst_en;//pike have no this bit
	uint32_t skip_num;
	uint32_t skip_num_clr;
	uint32_t mode;
	uint32_t low_ratio;
	uint32_t high_ratio;
	uint32_t big_adj;
	uint32_t small_adj;
	uint32_t dif_adj;
};

struct isp_dev_hist2_info_v1 {
	uint32_t bypass;
	uint32_t en;//pike has no this bit
	uint32_t skip_num;
	uint32_t skip_num_clr;
	uint32_t mode;
	int32_t  hist_roi_x_s[4];
	int32_t  hist_roi_y_s[4];
	uint32_t hist_roi_x_e[4];
	uint32_t hist_roi_y_e[4];
};

struct isp_dev_autocont_info_v1 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t in_min;
	uint32_t in_max;
	uint32_t out_min;
	uint32_t out_max;
};

struct isp_dev_yuv_cdn_info {
	uint32_t bypass;
	uint32_t filter_bypass;
	uint32_t median_writeback_en;
	uint32_t median_mode;
	uint32_t gaussian_mode;
	uint32_t median_thr;
	uint32_t median_thru0;
	uint32_t median_thru1;
	uint32_t median_thrv0;
	uint32_t median_thrv1;
	uint32_t rangewu[31];
	uint32_t rangewv[31];
	uint32_t level;
};

struct isp_dev_edge_info_v1 {
	uint32_t bypass;
	/*CFG0*/
	uint32_t ee_str_m_n;
	uint32_t ee_str_m_p;
	uint32_t ee_str_d_n;
	uint32_t ee_str_d_p;
	uint32_t mode;
	/*CFG1*/
	uint32_t ee_incr_d_n;
	uint32_t ee_incr_d_p;
	uint32_t ee_thr_d_n;
	uint32_t ee_thr_d_p;
	/*CFG2*/
	uint32_t ee_flat_thr_1;
	uint32_t ee_flat_thr_2;
	uint32_t ee_incr_m_n;
	uint32_t ee_incr_m_p;
	/*CFG3*/
	uint32_t ee_txt_thr_1;
	uint32_t ee_txt_thr_2;
	uint32_t ee_txt_thr_3;
	/*CFG4*/
	uint32_t ee_corner_sm_n;
	uint32_t ee_corner_sm_p;
	uint32_t ee_corner_gain_n;
	uint32_t ee_corner_gain_p;
	uint32_t ee_corner_th_n;
	uint32_t ee_corner_th_p;
	uint32_t ee_corner_cor;
	/*CFG5*/
	uint32_t ee_smooth_strength;
	uint32_t ee_smooth_thr;
	uint32_t sigma;
	uint32_t ee_flat_smooth_mode;
	uint32_t ee_edge_smooth_mode;
	/*CFG6*/
	uint32_t ee_incr_b_n;
	uint32_t ee_incr_b_p;
	uint32_t ee_str_b_n;
	uint32_t ee_str_b_p;
	/*CFG7*/
	uint8_t ratio[2];
	uint16_t reserved;
	uint32_t ipd_flat_thr;
	uint32_t ipd_bypass;
	uint32_t ee_clip_after_smooth_en;

	//pike has no ADP_CFG0 ADP_CFG1 ADP_CFG2 registers
	/*ADP_CFG0*/
	uint32_t ee_t1_cfg;
	uint32_t ee_t2_cfg;
	uint32_t ee_t3_cfg;
	/*ADP_CFG1*/
	uint32_t ee_t4_cfg;
	uint32_t ee_cv_clip_n;
	uint32_t ee_cv_clip_p;
	/*ADP_CFG2*/
	uint32_t ee_r1_cfg;
	uint32_t ee_r2_cfg;
	uint32_t ee_r3_cfg;
	/*LEVEL*/
	uint32_t ee_level;
};

struct isp_dev_css_info_v1 {
	uint32_t bypass;
	uint32_t lh_chrom_th;
	uint8_t  chrom_lower_th[7];
	uint8_t  reserved0;
	uint8_t  chrom_high_th[7];
	uint8_t  reserved1;
	uint32_t lum_low_shift;
	uint32_t lum_hig_shift;
	uint8_t  lh_ratio[8];
	uint8_t  ratio[8];
	uint32_t lum_low_th;
	uint32_t lum_ll_th;
	uint32_t lum_hig_th;
	uint32_t lum_hh_th;
	uint32_t u_th_0_l;
	uint32_t u_th_0_h;
	uint32_t v_th_0_l;
	uint32_t v_th_0_h;
	uint32_t u_th_1_l;
	uint32_t u_th_1_h;
	uint32_t v_th_1_l;
	uint32_t v_th_1_h;
	uint32_t cutoff_th;
};

struct isp_dev_csa_info_v1 {
	uint32_t bypass;
	uint32_t factor_u;
	uint32_t factor_v;
	struct csa_factor_v1 factor;
};

struct isp_dev_hue_info_v1 {
	uint32_t bypass;
	uint32_t theta;
};

struct isp_dev_post_cdn_info {
	uint32_t bypass;
	uint32_t downsample_bypass;
	uint32_t mode;
	uint32_t writeback_en;
	uint32_t uvjoint;
	uint32_t median_mode;
	uint32_t adapt_med_thr;
	uint32_t uvthr0;
	uint32_t uvthr1;
	uint32_t thru0;
	uint32_t thru1;
	uint32_t thrv0;
	uint32_t thrv1;
	uint8_t r_segu[2][7];
	uint8_t r_segv[2][7];
	uint8_t r_distw[15][5];
	uint8_t reserved;
	uint32_t start_row_mod4;
	uint32_t level;
};

struct isp_dev_emboss_info_v1 {
	uint32_t y_bypass;
	uint32_t uv_bypass;
	uint32_t y_step;
	uint32_t uv_step;
};

struct isp_dev_ygamma_info {
	uint32_t bypass;
	uint32_t buf_sel;
	struct coordinate_xy nodes[ISP_PINGPANG_YUV_YGAMC_NUM];
};

struct isp_dev_ydelay_info {
	uint32_t bypass;
};

struct isp_dev_yuv_nlm_info {
	uint32_t nlm_bypass;
	uint32_t nlm_radial_bypass;
	uint32_t nlm_adaptive_bypass;
	uint32_t nlm_vst_bypass;
	uint32_t edge_str_req[7];
	uint32_t edge_str_cmp[7];
	uint32_t edge_range_l;
	uint32_t edge_range_h;
	uint32_t edge_time_str;
	uint32_t avg_mode;
	uint32_t den_strength;
	uint32_t center_x;
	uint32_t center_y;
	uint32_t center_x2;
	uint32_t center_y2;
	uint32_t radius;
	uint32_t radius_p1;
	uint32_t radius_p2;
	uint32_t gain_max;
	uint32_t start_col;
	uint32_t start_raw;
	uint32_t add_back;
	uint32_t lut_w[24];
	uint32_t lut_vs[64];
};

struct isp_dev_iircnr_info_v1 {
	uint32_t bypass;
	uint32_t mode;
	uint32_t y_th;
	uint32_t uv_th;
	uint32_t uv_pg_th;
	uint32_t uv_dist;
	uint32_t uv_low_thr2;
	uint32_t uv_low_thr1;
	uint32_t uv_s_th;
	uint32_t alpha_low_u;
	uint32_t alpha_low_v;
	uint32_t uv_high_thr2;
	uint32_t ymd_u;
	uint32_t ymd_v;
	uint32_t slope;
	uint32_t factor;
	uint32_t iirnr_level;
};

struct isp_dev_yrandom_info_v1 {
	uint32_t bypass;
	uint32_t seed;
	uint32_t mode;
	uint32_t init;
	uint32_t offset;
	uint32_t shift;
	uint32_t takeBit[8];
};

struct isp_dev_fetch_info_v1 {
	uint32_t bypass;
	uint32_t substract;
	uint32_t color_format;
//	struct isp_img_size slice_size;
	struct isp_addr addr;
	struct isp_pitch pitch;
	uint32_t mipi_word_num;
	uint32_t mipi_byte_rel_pos;
};

struct isp_dev_store_info_v1 {
	uint32_t bypass;
	uint32_t substract;
	uint32_t color_format;
//	struct isp_img_size size;
	struct isp_addr addr;
	struct isp_pitch pitch;
};

struct isp_dev_dispatch_info_v1 {
	uint32_t bayer_ch0;
	uint32_t bayer_ch1;
};

struct isp_dev_arbiter_info_v1 {
	struct arbiter_endian_v1 endian_ch0;
	struct arbiter_endian_v1 endian_ch1;
	struct arbiter_param_v1 para;
};

struct isp_dev_axi_info_v1 {
	uint32_t iti2axim_ctrl;
	uint32_t convert_wr_ctrl;
};

struct isp_raw_sizer_init_para {
	uint32_t half_dst_wd_remain;
	uint32_t residual_half_wd;
	uint32_t adv_b_init_residual;
	uint32_t adv_a_init_residual;
	uint32_t offset_incr_init_b;
	uint32_t offset_incr_init_a;
	uint32_t offset_base_incr;
};

struct isp_common_lbuf_param_v1 {
	uint32_t cfae_lbuf_offset;
	uint32_t comm_lbuf_offset;
	uint32_t ydly_lbuf_offset;
	uint32_t awbm_lbuf_offset;
};

struct isp_dev_common_info_v1 {
	uint32_t fetch_sel_0;
	uint32_t sizer_sel_0;
	uint32_t store_sel_0;
	uint32_t fetch_sel_1;
	uint32_t sizer_sel_1;
	uint32_t store_sel_1;
	uint32_t fetch_color_format;
	uint32_t store_color_format;
	uint32_t awbm_pos;
	uint32_t y_afm_pos_0;
	uint32_t y_aem_pos_0;
	uint32_t y_afm_pos_1;
	uint32_t y_aem_pos_1;
	struct isp_common_lbuf_param_v1 lbuf_off;
};

struct isp_dev_common_info_v2 {
	uint32_t fetch_sel_0;
	uint32_t sizer_sel_0;
	uint32_t store_sel_0;
	uint32_t fetch_sel_1;
	uint32_t sizer_sel_1;
	uint32_t store_sel_1;
	uint32_t fetch_color_format;
	uint32_t store_color_format;
	uint32_t awbm_pos;
	uint32_t y_afm_pos_0;
	uint32_t y_aem_pos_0;
	uint32_t y_afm_pos_1;
	uint32_t y_aem_pos_1;
	uint32_t lbuf_off;
};


struct isp_dev_pingpang_ctm_info_v1 {
	uint8_t val;
};

struct isp_dev_pingpang_hsv_info_v1 {
	uint16_t val;
};


struct isp_dev_pingpang_frgb_gamc_info_v1 {
	uint32_t r_node;
	uint32_t g_node;
	uint32_t b_node;
};

#define ISP_IO_MAGIC         'R'
#define ISP_IO_CAPABILITY    _IOR(ISP_IO_MAGIC, 0, struct isp_capability)
#define ISP_IO_IRQ           _IOR(ISP_IO_MAGIC, 1, struct isp_irq)
#define ISP_IO_READ          _IOR(ISP_IO_MAGIC, 2, struct isp_reg_param)
#define ISP_IO_WRITE         _IOW(ISP_IO_MAGIC, 3, struct isp_reg_param)
#define ISP_IO_RST           _IOW(ISP_IO_MAGIC, 4, uint32_t)
#define ISP_IO_STOP          _IOW(ISP_IO_MAGIC, 5, uint32_t)
#define ISP_IO_INT           _IOW(ISP_IO_MAGIC, 6, uint32_t)
#define ISP_IO_CFG_PARAM     _IOWR(ISP_IO_MAGIC, 7, struct isp_io_param)
#define ISP_REG_READ	_IOR(ISP_IO_MAGIC, 8, struct isp_reg_bits)

#endif
