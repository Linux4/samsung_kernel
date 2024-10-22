/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_CCORR_H__
#define __MTK_DISP_CCORR_H__

#include <uapi/drm/mediatek_drm.h>

struct mtk_disp_ccorr_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct ccorr_backup {
	unsigned int REG_CCORR_CFG;
	unsigned int REG_CCORR_INTEN;
};

struct mtk_disp_ccorr_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_ccorr_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct mtk_disp_ccorr_primary {
	unsigned int ccorr_8bit_switch;
	unsigned int ccorr_relay_value;
	struct drm_mtk_ccorr_caps disp_ccorr_caps;
	int ccorr_offset_base;
	int ccorr_max_negative;
	int ccorr_max_positive;
	int ccorr_fullbit_mask;
	int ccorr_offset_mask;
	unsigned int disp_ccorr_number;
	unsigned int disp_ccorr_linear;
	bool disp_aosp_ccorr;
	bool prim_ccorr_force_linear;
	bool prim_ccorr_pq_nonlinear;
	bool sbd_on;
	atomic_t ccorr_irq_en;
	struct DRM_DISP_CCORR_COEF_T *disp_ccorr_coef;
	int ccorr_color_matrix[3][3];
	int ccorr_prev_matrix[3][3];
	int rgb_matrix[3][3];
	struct DRM_DISP_CCORR_COEF_T multiply_matrix_coef;
	int disp_ccorr_without_gamma;
	int disp_ccorr_temp_linear;
	wait_queue_head_t ccorr_get_irq_wq;
	spinlock_t ccorr_clock_lock;
	atomic_t ccorr_get_irq;
	spinlock_t pq_bl_change_lock;
	int old_pq_backlight;
	int pq_backlight;
	int pq_backlight_db;
	atomic_t ccorr_is_init_valid;
	struct mutex ccorr_global_lock;
	struct ccorr_backup backup;
};

struct mtk_disp_ccorr {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_ccorr_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	enum drm_disp_ccorr_linear_t is_linear;// each comp property
	struct mtk_disp_ccorr_primary *primary_data;
	atomic_t is_clock_on;
	struct mtk_disp_ccorr_tile_overhead tile_overhead;
	struct mtk_disp_ccorr_tile_overhead_v tile_overhead_v;
	bool bypass_color;
	struct mtk_ddp_comp *color_comp;
	bool set_partial_update;
	unsigned int roi_height;
};

inline struct mtk_disp_ccorr *comp_to_ccorr(struct mtk_ddp_comp *comp);
void ccorr_test(const char *cmd, char *debug_output);
int ccorr_interface_for_color(unsigned int ccorr_idx,
	unsigned int ccorr_coef[3][3], void *handle);
void disp_pq_notify_backlight_changed(struct mtk_ddp_comp *comp, int bl_1024);
int disp_ccorr_set_color_matrix(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	int32_t matrix[16], int32_t hint, bool fte_flag, bool linear);
int disp_ccorr_set_RGB_Gain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int r, int g, int b);
int mtk_drm_ioctl_set_ccorr(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_ccorr_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_ccorr_get_irq(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_support_color_matrix(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aibld_cv_mode(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_get_pq_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_set_pq_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_get_ccorr_caps(struct mtk_ddp_comp *comp, struct drm_mtk_ccorr_caps *ccorr_caps);
void disp_ccorr_set_bypass(struct drm_crtc *crtc, int bypass);
void mtk_ccorr_regdump(struct mtk_ddp_comp *comp);
int mtk_drm_ioctl_ccorr_get_irq_impl(struct mtk_ddp_comp *comp, void *data);
// for displayPQ update to swpm tppa
unsigned int disp_ccorr_bypass_info(struct mtk_drm_crtc *mtk_crtc);

#endif

