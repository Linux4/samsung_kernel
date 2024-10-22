/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_SPR_H__
#define __MTK_DISP_SPR_H__

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"

struct mtk_disp_spr_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};
int mtk_spr_check_postalign_status(struct mtk_drm_crtc *mtk_crtc);

#endif
