/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTKFB_DEBUG_H
#define __MTKFB_DEBUG_H

#include "mtk_panel_ext.h"

#define LOGGER_BUFFER_SIZE (16 * 1024)
#define ERROR_BUFFER_COUNT 16
#define FENCE_BUFFER_COUNT 80
#define DEBUG_BUFFER_COUNT 100
#define DUMP_BUFFER_COUNT 40
#define STATUS_BUFFER_COUNT 1
#if defined(CONFIG_MT_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define LOGGER_BUFFER_SIZE (16 * 1024)
#else
#define LOGGER_BUFFER_SIZE (256)
#endif
#define DEBUG_BUFFER_SIZE                                                      \
	(4096 +                                                                \
	 (ERROR_BUFFER_COUNT + FENCE_BUFFER_COUNT + DEBUG_BUFFER_COUNT +       \
	  DUMP_BUFFER_COUNT + STATUS_BUFFER_COUNT) *                           \
		 LOGGER_BUFFER_SIZE)

enum MTK_DRM_DEBUG_LOG_SWITCH_OPS {
	MTK_DRM_OTHER = 0,
	MTK_DRM_MOBILE_LOG,
	MTK_DRM_DETAIL_LOG,
	MTK_DRM_FENCE_LOG,
	MTK_DRM_IRQ_LOG,
};
extern void disp_color_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_ccorr_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_aal_set_bypass(struct drm_crtc *crtc, int bypass);

extern unsigned int m_new_pq_persist_property[32];
enum mtk_pq_persist_property {
	DISP_PQ_COLOR_BYPASS,
	DISP_PQ_CCORR_BYPASS,
	DISP_PQ_GAMMA_BYPASS,
	DISP_PQ_DITHER_BYPASS,
	DISP_PQ_AAL_BYPASS,
	DISP_PQ_CCORR_SILKY_BRIGHTNESS,
	DISP_PQ_GAMMA_SILKY_BRIGHTNESS,
	DISP_PQ_PROPERTY_MAX,
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
int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg,
			bool blocking, bool need_lock);
int mtk_ddic_dsi_read_cmd(struct mtk_ddic_dsi_msg *cmd_msg, bool need_lock);
int set_lcm_wrapper(struct mtk_ddic_dsi_msg *cmd_msg, int blocking);
int read_lcm_wrapper(struct mtk_ddic_dsi_msg *cmd_msg);
int mtk_drm_set_frame_skip(bool skip);
#endif
int mtk_disp_ioctl_debug_log_switch(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif
