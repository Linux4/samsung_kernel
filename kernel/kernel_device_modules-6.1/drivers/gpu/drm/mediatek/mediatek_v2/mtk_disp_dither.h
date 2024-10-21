/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_DITHER_H__
#define __MTK_DISP_DITHER_H__

struct mtk_disp_dither_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass);
void disp_dither_set_color_detect(struct drm_crtc *crtc, int enable);
void mtk_dither_regdump(struct mtk_ddp_comp *comp);
// for displayPQ update to swpm tppa
unsigned int disp_dither_bypass_info(struct mtk_drm_crtc *mtk_crtc);
#endif
