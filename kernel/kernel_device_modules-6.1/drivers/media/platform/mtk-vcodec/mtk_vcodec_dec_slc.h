/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef _MTK_VCODEC_DEC_SLC_H_
#define _MTK_VCODEC_DEC_SLC_H_

#include "mtk_vcodec_drv.h"

struct slc_param {
	int gid;
	int uid;
	struct slbc_gid_data data;
	bool is_requested;
	unsigned int used_cnt;
	struct mutex param_mutex;
};

void mtk_vdec_init_slc(struct slc_param *param, int uid);
void mtk_vdec_slc_gid_request(struct mtk_vcodec_ctx *ctx, struct slc_param *param);
void mtk_vdec_slc_get_gid_from_dma(struct mtk_vcodec_ctx *ctx, struct dma_buf *dbuf);
void mtk_vdec_slc_read_invalidate(struct mtk_vcodec_ctx *ctx, struct slc_param *param, int enable);
void mtk_vdec_slc_gid_release(struct mtk_vcodec_ctx *ctx, struct slc_param *param);
void mtk_vdec_slc_fill_data(struct mtk_vcodec_ctx *ctx, struct slc_param *param);
void mtk_vdec_slc_update_roi(struct mtk_vcodec_ctx *ctx, struct slc_param *param);


#endif
