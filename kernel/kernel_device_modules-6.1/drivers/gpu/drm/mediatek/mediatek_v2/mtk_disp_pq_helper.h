/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_DISP_PQ_HELPER_H_
#define _MTK_DISP_PQ_HELPER_H_

#include <uapi/drm/mediatek_drm.h>
#include "mtk_disp_vidle.h"

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
	DISP_CLARITY_SUPPORT,
	DISP_PQ_PROPERTY_MAX,
};

enum PQ_REG_TABLE_IDX {
	TUNING_DISP_COLOR = 0,
	TUNING_DISP_CCORR,
	TUNING_DISP_AAL,
	TUNING_DISP_GAMMA,
	TUNING_DISP_DITHER,
	TUNING_DISP_CCORR1,     // 5
	TUNING_DISP_TDSHP,
	TUNING_DISP_C3D,
	TUNING_DISP_MDP_AAL,
	TUNING_DISP_ODDMR_TOP,
	TUNING_DISP_ODDMR_OD,   // 10
	TUNING_REG_MAX
};

struct pq_tuning_pa_base {
	enum mtk_ddp_comp_type type;
	resource_size_t pa_base;
	resource_size_t companion_pa_base;
};

int mtk_drm_ioctl_pq_frame_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_pq_proxy(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_pq_helper_frame_config(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle,
	void *data, bool user_lock);
int mtk_pq_helper_fill_comp_pipe_info(struct mtk_ddp_comp *comp, int *path_order,
	bool *is_right_pipe, struct mtk_ddp_comp **companion);
int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
struct drm_crtc *get_crtc_from_connector(int connector_id, struct drm_device *drm_dev);
int mtk_drm_ioctl_sw_write_impl(struct drm_crtc *crtc, void *data);
int mtk_drm_ioctl_sw_read_impl(struct drm_crtc *crtc, void *data);
int mtk_drm_ioctl_hw_read_impl(struct drm_crtc *crtc, void *data);
int mtk_drm_ioctl_hw_write_impl(struct drm_crtc *crtc, void *data);

#endif /* _MTK_DISP_PQ_HELPER_H_ */
