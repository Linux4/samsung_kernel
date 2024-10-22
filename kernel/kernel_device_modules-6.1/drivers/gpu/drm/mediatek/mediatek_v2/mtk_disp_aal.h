/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_AAL_H__
#define __MTK_DISP_AAL_H__

#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>

#define AAL_SERVICE_FORCE_UPDATE 0x1
/*******************************/
/* field definition */
/* ------------------------------------------------------------- */
/* AAL */
#define DISP_AAL_EN                             (0x000)
#define DISP_AAL_RESET                          (0x004)
#define DISP_AAL_INTEN                          (0x008)
#define DISP_AAL_INTSTA                         (0x00c)
#define DISP_AAL_STATUS                         (0x010)
#define DISP_AAL_CFG                            (0x020)
#define DISP_AAL_IN_CNT                         (0x024)
#define DISP_AAL_OUT_CNT                        (0x028)
#define DISP_AAL_CHKSUM                         (0x02c)
#define DISP_AAL_SIZE                           (0x030)
#define DISP_AAL_SHADOW_CTL                     (0x0B0)
#define DISP_AAL_DUMMY_REG                      (0x0c0)
#define DISP_AAL_SHADOW_CTRL                    (0x0f0)
#define AAL_BYPASS_SHADOW	BIT(0)
#define AAL_READ_WRK_REG	BIT(2)
#define DISP_AAL_MAX_HIST_CONFIG_00             (0x204)
#define DISP_AAL_CABC_00                        (0x20c)
#define DISP_AAL_CABC_02                        (0x214)
#define DISP_AAL_CABC_04                        (0x21c)
#define DISP_AAL_STATUS_00                      (0x224)
/* 00 ~ 32: max histogram */
#define DISP_AAL_STATUS_32                      (0x2a4)
/* bit 8: dre_gain_force_en */
#define DISP_AAL_DRE_GAIN_FILTER_00             (0x354)
#define DISP_AAL_DRE_FLT_FORCE(idx) \
	(0x358 + (idx) * 4)
#define DISP_AAL_DRE_CRV_CAL_00                 (0x344)
#define DISP_AAL_DRE_MAPPING_00                 (0x3b4)
#define DISP_AAL_CABC_GAINLMT_TBL(idx) \
	(0x410 + (idx) * 4)

#define DISP_AAL_DBG_CFG_MAIN                   (0x45c)
#define DISP_AAL_DUAL_PIPE_INFO_00              (0x4d0)
#define DISP_AAL_DUAL_PIPE_INFO_01              (0x4d4)
#define DISP_AAL_OUTPUT_SIZE                    (0x4d8)
#define DISP_AAL_OUTPUT_OFFSET                  (0x4dc)
#define DISP_Y_HISTOGRAM_00                     (0x504)

/* DRE 3.0 */
#define DISP_AAL_CFG_MAIN                       (0x200)
#define DISP_AAL_SRAM_CFG                       (0x0c4)
#define REG_HIST_SRAM_PP_HALT     REG_FLD_MSB_LSB(1, 1)
#define REG_FORCE_HIST_SRAM_EN    REG_FLD_MSB_LSB(4, 4)
#define REG_FORCE_HIST_SRAM_APB   REG_FLD_MSB_LSB(5, 5)
#define REG_FORCE_HIST_SRAM_INT   REG_FLD_MSB_LSB(6, 6)
#define REG_CURVE_SRAM_PP_HALT    REG_FLD_MSB_LSB(7, 7)
#define REG_FORCE_CURVE_SRAM_EN   REG_FLD_MSB_LSB(8, 8)
#define REG_FORCE_CURVE_SRAM_APB  REG_FLD_MSB_LSB(9, 9)
#define REG_FORCE_CURVE_SRAM_INT  REG_FLD_MSB_LSB(10, 10)
#define REG_CURVE_SRAM_RREQ_EN    REG_FLD_MSB_LSB(11, 14)
#define REG_SRAM_RREQ_EN          REG_FLD_MSB_LSB(16, 19)
#define REG_SRAM_WREQ_EN          REG_FLD_MSB_LSB(20, 21)
#define REG_SRAM_8X1_RREQ_EN      REG_FLD_MSB_LSB(24, 24)
#define REG_SRAM_8X1_WREQ_EN      REG_FLD_MSB_LSB(25, 25)
#define REG_SRAM_RW_SEL           REG_FLD_MSB_LSB(26, 26)
#define REG_CURVE_SRAM_WREQ_EN    REG_FLD_MSB_LSB(27, 28)
#define REG_SRAM_SOF_RST_SEL      REG_FLD_MSB_LSB(29, 29)
#define DISP_AAL_SRAM_STATUS                    (0x0c8)
#define DISP_AAL_SRAM_RW_IF_0                   (0x0cc)
#define DISP_AAL_SRAM_RW_IF_1                   (0x0d0)
#define DISP_AAL_SRAM_RW_IF_2                   (0x0d4)
#define DISP_AAL_SRAM_RW_IF_3                   (0x0d8)
#define MDP_AAL_CURVE_SRAM_RW_IF_0              (0x690)
#define MDP_AAL_CURVE_SRAM_RW_IF_1              (0x694)
#define MDP_AAL_CURVE_SRAM_RW_IF_2              (0x698)
#define MDP_AAL_CURVE_SRAM_RW_IF_3              (0x69c)
#define MDP_AAL_CURVE_SRAM_WADDR      MDP_AAL_CURVE_SRAM_RW_IF_0
#define MDP_AAL_CURVE_SRAM_WDATA      MDP_AAL_CURVE_SRAM_RW_IF_1
#define MDP_AAL_CURVE_SRAM_RADDR      MDP_AAL_CURVE_SRAM_RW_IF_2
#define MDP_AAL_CURVE_SRAM_RDATA      MDP_AAL_CURVE_SRAM_RW_IF_3
#define DISP_AAL_WIN_X_MAIN                     (0x460)
#define DISP_AAL_WIN_Y_MAIN                     (0x464)
#define DISP_AAL_DRE_BLOCK_INFO_00              (0x468)
#define DISP_AAL_DRE_BLOCK_INFO_01              (0x46c)
#define DISP_AAL_DRE_BLOCK_INFO_02              (0x470)
#define DISP_AAL_DRE_BLOCK_INFO_03              (0x474)
#define DISP_AAL_DRE_BLOCK_INFO_04              (0x478)
#define DISP_AAL_DRE_CHROMA_HIST_00             (0x480)
#define DISP_AAL_DRE_CHROMA_HIST_01             (0x484)
#define DISP_AAL_DRE_ALPHA_BLEND_00             (0x488)
#define DISP_AAL_DRE_BITPLUS_00                 (0x48c)
#define DISP_AAL_DRE_BITPLUS_01                 (0x490)
#define DISP_AAL_DRE_BITPLUS_02                 (0x494)
#define DISP_AAL_DRE_BITPLUS_03                 (0x498)
#define DISP_AAL_DRE_BITPLUS_04                 (0x49c)
#define DISP_AAL_DRE_BLOCK_INFO_05              (0x4b4)
#define DISP_AAL_DRE_BLOCK_INFO_06              (0x4b8)

#define DISP_AAL_DRE_BLOCK_INFO_07              (0x0f8)
#define MDP_AAL_TILE_00				(0x4EC)
#define MDP_AAL_TILE_01				(0x4F0)
#define MDP_AAL_TILE_02				(0x0F4)

#define MDP_AAL_DUAL_PIPE00				(0x500)
#define MDP_AAL_DUAL_PIPE08				(0x544)
#define MDP_AAL_DRE_ROI_00						(0x520)
#define MDP_AAL_DRE_ROI_01						(0x524)
#define MDP_AAL_STATUS_00                      (0x224)
#define MDP_Y_HISTOGRAM_00                     (0x604)


/* AAL Calarty */
#define MDP_AAL_DRE_BILATEAL                    (0x53C)
#define MDP_AAL_DRE_BILATERAL_Blending_00       (0x564)
#define MDP_AAL_DRE_BILATERAL_CUST_FLT1_00      (0x568)
#define MDP_AAL_DRE_BILATERAL_CUST_FLT1_01      (0x56C)
#define MDP_AAL_DRE_BILATERAL_CUST_FLT1_02      (0x570)
#define MDP_AAL_DRE_BILATERAL_CUST_FLT2_00      (0x574)
#define MDP_AAL_DRE_BILATERAL_CUST_FLT2_01      (0x578)
#define MDP_AAL_DRE_BILATERAL_CUST_FLT2_02      (0x57C)
#define MDP_AAL_DRE_BILATERAL_FLT_CONFIG        (0x580)
#define MDP_AAL_DRE_BILATERAL_FREQ_BLENDING     (0x584)
#define MDP_AAL_DRE_BILATERAL_STATUS_00         (0x588)
#define MDP_AAL_DRE_BILATERAL_REGION_PROTECTION (0x5A8)
#define MDP_AAL_DRE_BILATERAL_STATUS_ROI_X      (0x5AC)
#define MDP_AAL_DRE_BILATERAL_STATUS_ROI_Y      (0x5B0)
#define MDP_AAL_DRE_BILATERAL_Blending_01       (0x5B4)
#define MDP_AAL_DRE_BILATERAL_STATUS_CTRL       (0x5B8)

/* TDSHP Clarity */
#define MDP_TDSHP_00                            (0x000)
#define MDP_TDSHP_CFG                           (0x110)
#define MDP_HIST_CFG_00                         (0x064)
#define MDP_HIST_CFG_01                         (0x068)
#define MDP_LUMA_HIST_00                        (0x06C)
#define MDP_LUMA_SUM                            (0x0B4)
#define MDP_TDSHP_SRAM_1XN_OUTPUT_CNT           (0x0B8)
#define MDP_Y_FTN_1_0_MAIN                      (0x0BC)
#define MDP_TDSHP_STATUS_00                     (0x644)
#define MIDBAND_COEF_V_CUST_FLT1_00             (0x584)
#define MIDBAND_COEF_V_CUST_FLT1_01             (0x588)
#define MIDBAND_COEF_V_CUST_FLT1_02             (0x58C)
#define MIDBAND_COEF_V_CUST_FLT1_03             (0x590)
#define MIDBAND_COEF_H_CUST_FLT1_00             (0x594)
#define MIDBAND_COEF_H_CUST_FLT1_01             (0x598)
#define MIDBAND_COEF_H_CUST_FLT1_02             (0x59C)
#define MIDBAND_COEF_H_CUST_FLT1_03             (0x600)

#define HIGHBAND_COEF_V_CUST_FLT1_00            (0x604)
#define HIGHBAND_COEF_V_CUST_FLT1_01            (0x608)
#define HIGHBAND_COEF_V_CUST_FLT1_02            (0x60C)
#define HIGHBAND_COEF_V_CUST_FLT1_03            (0x610)
#define HIGHBAND_COEF_H_CUST_FLT1_00            (0x614)
#define HIGHBAND_COEF_H_CUST_FLT1_01            (0x618)
#define HIGHBAND_COEF_H_CUST_FLT1_02            (0x61C)
#define HIGHBAND_COEF_H_CUST_FLT1_03            (0x620)
#define HIGHBAND_COEF_RD_CUST_FLT1_00           (0x624)
#define HIGHBAND_COEF_RD_CUST_FLT1_01           (0x628)
#define HIGHBAND_COEF_RD_CUST_FLT1_02           (0x62C)
#define HIGHBAND_COEF_RD_CUST_FLT1_03           (0x630)
#define HIGHBAND_COEF_LD_CUST_FLT1_00           (0x634)
#define HIGHBAND_COEF_LD_CUST_FLT1_01           (0x638)
#define HIGHBAND_COEF_LD_CUST_FLT1_02           (0x63C)
#define HIGHBAND_COEF_LD_CUST_FLT1_03           (0x640)
#define MDP_TDSHP_SIZE_PARA                     (0x674)
#define MDP_TDSHP_FREQUENCY_WEIGHTING	        (0x678)
#define MDP_TDSHP_FREQUENCY_WEIGHTING_FINAL	(0x67C)
#define SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH	(0x680)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_HUGE	        (0x684)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_BIG	        (0x688)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_MEDIUM	(0x68C)
#define FINAL_SIZE_ADAPTIVE_WEIGHT_SMALL	(0x690)
#define ACTIVE_PARA_FREQ_M	                (0x694)
#define ACTIVE_PARA_FREQ_H	                (0x698)
#define ACTIVE_PARA_FREQ_D	                (0x69C)
#define ACTIVE_PARA_FREQ_L	                (0x700)
#define ACTIVE_PARA	                        (0x704)
#define CLASS_0_2_GAIN	                        (0x708)
#define CLASS_3_5_GAIN	                        (0x70C)
#define CLASS_6_8_GAIN	                        (0x710)
#define LUMA_CHROMA_PARAMETER	                (0x714)
#define MDP_TDSHP_STATUS_ROI_X	                (0x718)
#define MDP_TDSHP_STATUS_ROI_Y	                (0x71C)
#define FRAME_WIDTH_HIGHT	                (0x720)
#define MDP_TDSHP_SHADOW_CTRL	                (0x724)

#define AAL_DRE30_GAIN_REGISTER_NUM		(544)
#define AAL_DRE30_HIST_REGISTER_NUM		(768)

#define AAL_U32_PTR(x) ((unsigned int *)(unsigned long)x)
#define aal_u32_handle_t unsigned long long

enum AAL_ESS_UD_MODE {
	CONFIG_BY_CUSTOM_LIB = 0,
	CONFIG_TO_LCD = 1,
	CONFIG_TO_AMOLED = 2
};

enum AAL_DRE_MODE {
	DRE_EN_BY_CUSTOM_LIB = 0xFFFF,
	DRE_OFF = 0,
	DRE_ON = 1
};

enum AAL_ESS_MODE {
	ESS_EN_BY_CUSTOM_LIB = 0xFFFF,
	ESS_OFF = 0,
	ESS_ON = 1
};

enum AAL_ESS_LEVEL {
	ESS_LEVEL_BY_CUSTOM_LIB = 0xFFFF
};

enum DISP_AAL_REFRESH_LATENCY {
	AAL_REFRESH_17MS = 17,
	AAL_REFRESH_33MS = 33
};

enum MTK_LED_TYPE {
	TYPE_FILE = 0,
	TYPE_ATOMIC = 1,
	TYPE_FACTORY = 2,
	TYPE_MAX = 3,
};

struct DISP_DRE30_HIST {
	unsigned int aal0_dre_hist[AAL_DRE30_HIST_REGISTER_NUM];
	unsigned int aal1_dre_hist[AAL_DRE30_HIST_REGISTER_NUM];
	int dre_blk_x_num;
	int dre_blk_y_num;
};

struct DISP_DRE30_PARAM {
	unsigned int dre30_gain[AAL_DRE30_GAIN_REGISTER_NUM];
};

struct mtk_disp_aal_data {
	bool support_shadow;
	bool need_bypass_shadow;
	int aal_dre_hist_start;
	int aal_dre_hist_end;
	int aal_dre_gain_start;
	int aal_dre_gain_end;
	bool aal_dre3_curve_sram;
	bool aal_dre3_auto_inc;
	bool mdp_aal_ghist_support;
	int bitShift;
};

struct dre3_node {
	struct device *dev;
	void __iomem *va;
	phys_addr_t pa;
	struct clk *clk;
};

struct _mtk_disp_aal_tile_overhead {
	unsigned int in_width;
	unsigned int comp_overhead;
	unsigned int total_overhead;
};

struct _mtk_disp_aal_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_aal_feature_option {
	unsigned int mtk_aal_support;
	unsigned int mtk_dre30_support;
};

#define DRE_FLT_NUM	(12)
#define CABC_GAINLMT_NUM (11)
struct aal_backup { /* structure for backup AAL register value */
	unsigned int DRE_MAPPING;
	unsigned int DRE_FLT_FORCE[DRE_FLT_NUM];
	unsigned int CABC_00;
	unsigned int CABC_02;
	unsigned int CABC_GAINLMT[CABC_GAINLMT_NUM];
#if defined(DRE3_IN_DISP_AAL)
	unsigned int DRE_BLOCK_INFO_00;
	unsigned int DRE_BLOCK_INFO_01;
	unsigned int DRE_BLOCK_INFO_02;
	unsigned int DRE_BLOCK_INFO_04;
	unsigned int DRE_BLOCK_INFO_05;
	unsigned int DRE_BLOCK_INFO_06;
	unsigned int DRE_BLOCK_INFO_07;
	unsigned int DRE_CHROMA_HIST_00;
	unsigned int DRE_CHROMA_HIST_01;
	unsigned int DRE_ALPHA_BLEND_00;
	unsigned int SRAM_CFG;
	unsigned int DUAL_PIPE_INFO_00;
	unsigned int DUAL_PIPE_INFO_01;
#endif
	unsigned int AAL_CFG;
	unsigned int AAL_INTEN;
};

struct work_struct_aal_data {
	void *data;
	struct work_struct task;
};

struct mtk_disp_aal_primary {
	struct task_struct *sof_irq_event_task;
	struct timespec64 start;
	struct timespec64 end;
	int dbg_en;
	struct wait_queue_head hist_wq;
	struct wait_queue_head sof_irq_wq;
	spinlock_t clock_lock;
	spinlock_t hist_lock;
	struct DISP_AAL_HIST hist;
	struct DISP_AAL_HIST hist_db;
	atomic_t hist_wait_dualpipe;
	atomic_t sof_irq_available;
	atomic_t is_init_regs_valid;
	atomic_t backlight_notified;
	atomic_t initialed;
	atomic_t allowPartial;
	atomic_t force_enable_irq;
	atomic_t force_relay;
	atomic_t should_stop;
	atomic_t dre30_write;
	atomic_t irq_en;
	atomic_t eof_irq_en;
	atomic_t force_delay_check_trig;
	struct workqueue_struct *flip_wq;
	struct workqueue_struct *refresh_wq;
	int backlight_set;
	int elvsspn_set;
	spinlock_t dre3_gain_lock;
	atomic_t dre_halt;
	struct DISP_DRE30_INIT init_dre30;
	struct DISP_DRE30_PARAM dre30_gain;
	struct DISP_DRE30_PARAM dre30_gain_db;
	struct DISP_DRE30_HIST dre30_hist;
	struct DISP_DRE30_HIST dre30_hist_db;
	atomic_t change_to_dre30;
	u32 sram_method;
	struct wait_queue_head size_wq;
	bool get_size_available;
	struct DISP_AAL_DISPLAY_SIZE size;
	struct DISP_AAL_DISPLAY_SIZE dual_size;
	atomic_t panel_type;
	int ess_level;
	int dre_en;
	int ess_en;
	int ess_level_cmd_id;
	int dre_en_cmd_id;
	int ess_en_cmd_id;
	bool isDualPQ;
	struct mutex sram_lock;
	bool dre30_enabled;
	bool prv_dre30_enabled;
	unsigned int dre30_en;
	struct DISP_CLARITY_REG *disp_clarity_regs;
	struct mutex clarity_lock;
	struct mtk_aal_feature_option *aal_fo;
	struct DISP_AAL_PARAM aal_param;
	struct DISP_AAL_ESS20_SPECT_PARAM ess20_spect_param;
	int aal_clarity_support;
	int tdshp_clarity_support;
	int disp_clarity_support;
	struct aal_backup backup;
	struct DISP_AAL_INITREG init_regs;
	struct work_struct_aal_data refresh_task;
	enum MTK_LED_TYPE led_type;
	unsigned int fps;
};

struct mtk_disp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	struct dre3_node dre3_hw;
	atomic_t is_clock_on;
	const struct mtk_disp_aal_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_aal_primary *primary_data;
	struct _mtk_disp_aal_tile_overhead overhead;
	struct _mtk_disp_aal_tile_overhead_v tile_overhead_v;
	atomic_t hist_available;
	atomic_t dre20_hist_is_ready;
	atomic_t eof_irq;
	atomic_t first_frame;
	atomic_t force_curve_sram_apb;
	atomic_t force_hist_apb;
	atomic_t dre_hw_init;
	atomic_t dre_hw_prepare;
	atomic_t dre_config;
	struct mtk_ddp_comp *comp_tdshp;
	struct mtk_ddp_comp *comp_gamma;
	struct mtk_ddp_comp *comp_color;
	struct mtk_ddp_comp *comp_dmdp_aal;
	bool set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_disp_aal *comp_to_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_aal, ddp_comp);
}

void disp_aal_debug(struct drm_crtc *crtc, const char *opt);

/* Provide for LED */
void disp_aal_notify_backlight_changed(struct mtk_ddp_comp *comp, int trans_backlight,
	int panel_nits, int max_backlight, int need_lock);

/* AAL Control API in Kernel */
void disp_aal_set_lcm_type(unsigned int panel_type);
void disp_aal_set_ess_level(struct mtk_ddp_comp *comp, int level);
void disp_aal_set_ess_en(struct mtk_ddp_comp *comp, int enable);
void disp_aal_set_dre_en(struct mtk_ddp_comp *comp, int enable);
void disp_aal_set_bypass(struct drm_crtc *crtc, int bypass);
void mtk_aal_regdump(struct mtk_ddp_comp *comp);

int mtk_drm_ioctl_aal_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_get_hist(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_init_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_init_dre30(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_get_size(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_set_trigger_state(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_set_ess20_spect_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_clarity_set_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
// for displayPQ update to swpm tppa
unsigned int disp_aal_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif

