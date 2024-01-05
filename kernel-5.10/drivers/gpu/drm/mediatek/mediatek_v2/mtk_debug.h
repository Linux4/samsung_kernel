/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTKFB_DEBUG_H
#define __MTKFB_DEBUG_H

#include "mtk_drm_plane.h"
#include "mtk_panel_ext.h"

#define LOGGER_BUFFER_SIZE (16 * 1024)
#define ERROR_BUFFER_COUNT 16
#define FENCE_BUFFER_COUNT 80
#define DEBUG_BUFFER_COUNT 100
#define DUMP_BUFFER_COUNT 40
#define STATUS_BUFFER_COUNT 1
#define _DRM_P_H_
#if defined(CONFIG_MT_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define DEBUG_BUFFER_SIZE                                                      \
	(4096 +                                                                \
	 (ERROR_BUFFER_COUNT + FENCE_BUFFER_COUNT + DEBUG_BUFFER_COUNT +       \
	  DUMP_BUFFER_COUNT + STATUS_BUFFER_COUNT) *                           \
		 LOGGER_BUFFER_SIZE)
#else
#define DEBUG_BUFFER_SIZE 10240
#endif

extern void disp_color_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_ccorr_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_aal_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_dither_set_color_detect(struct drm_crtc *crtc, int enable);
extern void mtk_trans_gain_to_gamma(struct drm_crtc *crtc,
	unsigned int gain[3], unsigned int bl);

extern unsigned int m_new_pq_persist_property[32];
extern unsigned int g_gamma_data_mode;
enum mtk_pq_persist_property {
	DISP_PQ_COLOR_BYPASS,
	DISP_PQ_CCORR_BYPASS,
	DISP_PQ_GAMMA_BYPASS,
	DISP_PQ_DITHER_BYPASS,
	DISP_PQ_AAL_BYPASS,
	DISP_PQ_C3D_BYPASS,
	DISP_PQ_TDSHP_BYPASS,
	DISP_PQ_CCORR_SILKY_BRIGHTNESS,
	DISP_PQ_GAMMA_SILKY_BRIGHTNESS,
	DISP_PQ_DITHER_COLOR_DETECT,
	DISP_PQ_PROPERTY_MAX,
};

enum DISP_FRAME_STATE {
	DISP_FRAME_START = 0,
	DISP_FRAME_DONE,
};

#define CUSTOMER_USE_SIMPLE_API 1
enum mtk_set_lcm_sceanario {
	SET_LCM_NONE = 0,
	SET_LCM_POWER_MODE_SWITCH,

	SET_LCM_CMDQ_AVAILABLE,

	SET_LCM_FPS_CHANGE,

	SET_LCM_CMDQ_FRAME_DONE,

	SET_LCM_POWER_MODE_NEED_CMDQ,
	SET_LCM_HBM_CMD,
	SET_LCM_DISP_ON,
};

#define SET_LCM_BLOCKING_NOWAIT 3
#define SET_LCM_NONBLOCKING 2
#define SET_LCM_BLOCKING 1

struct frame_condition_wq {
	wait_queue_head_t wq;
	atomic_t condition;
};

int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

extern int mtk_disp_hrt_bw_dbg(void);

#ifdef _DRM_P_H_
struct disp_rect {
	u32 x;
	u32 y;
	u32 width;
	u32 height;
};
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *drm_dev);
void disp_dbg_deinit(void);
void mtk_drm_cwb_backup_copy_size(void);
int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);
int mtk_dprec_mmp_dump_wdma_layer(struct drm_crtc *crtc,
	struct drm_framebuffer *wb_fb);
int mtk_dprec_mmp_dump_cwb_buffer(struct drm_crtc *crtc,
	void *buffer, unsigned int buf_idx);
int disp_met_set(void *data, u64 val);
void mtk_drm_idlemgr_kick_ext(const char *source);
unsigned int mtk_dbg_get_lfr_mode_value(void);
unsigned int mtk_dbg_get_lfr_type_value(void);
unsigned int mtk_dbg_get_lfr_enable_value(void);
unsigned int mtk_dbg_get_lfr_update_value(void);
unsigned int mtk_dbg_get_lfr_vse_dis_value(void);
unsigned int mtk_dbg_get_lfr_skip_num_value(void);
unsigned int mtk_dbg_get_lfr_dbg_value(void);
int hrt_lp_switch_get(void);
int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg,
			int blocking, bool need_lock);
int mtk_ddic_dsi_read_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool need_lock);
int set_lcm_wrapper(struct mtk_ddic_dsi_msg *cmd_msg, int blocking);
int read_lcm_wrapper(struct mtk_ddic_dsi_msg *cmd_msg);
int mtk_crtc_lock_control(bool en);
void reset_frame_wq(struct frame_condition_wq *wq);
void wakeup_frame_wq(struct frame_condition_wq *wq);
int wait_frame_condition(enum DISP_FRAME_STATE state, unsigned int timeout);
int mtk_drm_set_frame_skip(bool skip);
#endif
#if IS_ENABLED(CONFIG_DRM_PANEL_MCD_COMMON)
int mtk_drm_wait_one_vblank(void);
void mtk_disp_mipi_ccci_callback(unsigned int en, unsigned int usrdata);
#endif

#endif
