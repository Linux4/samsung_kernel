// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "vdec_drv_if.h"
#include "mtk_vcodec_dec_slc.h"
#include <mtk_heap.h>


int slc_roi_8k[2][2] = {
	{648413184, 32051}, // 8k hyfbc frame size 81051648 * 8, total bw
	{14700000, 912} // ube size, total bw
};

void mtk_vdec_init_slc(struct slc_param *param, int uid)
{
	param->gid = -1;
	param->uid = uid;
	memset(&param->data, 0, sizeof(struct slbc_gid_data));
	param->is_requested = false;
	param->used_cnt = 0;
	mutex_init(&param->param_mutex);
}

void mtk_vdec_slc_gid_request(struct mtk_vcodec_ctx *ctx, struct slc_param *param)
{
	mutex_lock(&param->param_mutex);
	param->used_cnt++;
	mtk_vdec_slc_fill_data(ctx, param); // update roi info (TBD)

	if (param->uid == ID_VDEC_FRAME) {
		if (ctx->dev->queued_frame && param->gid >= 0 && !param->is_requested) {
			slbc_gid_request(param->uid, &param->gid, &param->data);
			param->is_requested = true;
			mtk_v4l2_debug(1, "[%d][SLC] uid %d gid %d request successfully",
				ctx->id, param->uid, param->gid);
			slbc_validate(param->uid, param->gid);
			mtk_v4l2_debug(4, "[%d][SLC] uid %d gid %d validated",
				ctx->id, param->uid, param->gid);
		} else if (param->gid >= 0 && param->is_requested) {
			mtk_v4l2_debug(4, "[%d][SLC] uid %d gid %d used_cnt increased to %d, reserved for roi update",
				ctx->id, param->uid, param->gid, param->used_cnt);
		} else if (param->gid == -1)
			mtk_v4l2_debug(1, "[%d][SLC] uid %d wait buf queue", ctx->id, param->uid);
	} else { // hw working buffer
		if (param->used_cnt == 1) {
			if (slbc_gid_request(param->uid, &param->gid, &param->data) < 0)
				mtk_v4l2_err("[%d][SLC] uid %d request gid failed",
					ctx->id, param->uid);
			else {
				param->is_requested = true;
				mtk_v4l2_debug(1, "[%d][SLC] uid %d requested new gid %d",
					ctx->id, param->uid, param->gid);
				slbc_validate(param->uid, param->gid);
				mtk_v4l2_debug(4, "[%d][SLC] uid %d gid %d validated",
					ctx->id, param->uid, param->gid);
			}
		} else if (param->is_requested) {
			mtk_v4l2_debug(4, "[%d][SLC] uid %d gid %d, used_cnt increased to %d, reserved for roi update",
				ctx->id, param->uid, param->gid, param->used_cnt);
		} else {
			mtk_v4l2_err("[%d][SLC] uid %d gid %d used_cnt %d not requested",
				ctx->id, param->uid, param->gid, param->used_cnt);
		}
	}
	mutex_unlock(&param->param_mutex);
}

void mtk_vdec_slc_get_gid_from_dma(struct mtk_vcodec_ctx *ctx, struct dma_buf *dbuf)
{
	struct slc_param *param = &ctx->dev->dec_slc_frame;

	mutex_lock(&param->param_mutex);
	if (!ctx->dev->queued_frame) {
		if (param->gid == -1) {
			param->gid = dma_buf_get_gid(dbuf);
			if (param->gid == -1)
				mtk_v4l2_err("[%d][SLC] uid %d get gid from dma failed",
					ctx->id, param->uid);
			else
				mtk_v4l2_debug(1, "[%d][SLC] got gid %d from dma_buf %p",
					ctx->id, param->gid, dbuf);
		}
		ctx->dev->queued_frame = true;
		if (param->used_cnt > 0 && param->gid >= 0 && !param->is_requested) {
			mtk_vdec_slc_fill_data(ctx, param); // update roi info (TBD)
			slbc_gid_request(param->uid, &param->gid, &param->data);
			param->is_requested = true;
			mtk_v4l2_debug(1, "[%d][SLC] uid %d gid %d request successfully",
				ctx->id, param->uid, param->gid);
			slbc_validate(param->uid, param->gid);
			mtk_v4l2_debug(4, "[%d][SLC] uid %d gid %d validated",
				ctx->id, param->uid, param->gid);
		}
	}
	mutex_unlock(&param->param_mutex);
}

void mtk_vdec_slc_read_invalidate(struct mtk_vcodec_ctx *ctx,
	struct slc_param *param, int enable)
{
	mutex_lock(&param->param_mutex);
	if (param->is_requested) {
		slbc_read_invalidate(param->uid, param->gid, enable);
		mtk_v4l2_debug(1, "[%d][SLC] uid %d gid %d set read invalidate %d",
			ctx->id, param->uid, param->gid, enable);
	} else {
		mtk_v4l2_err("[%d][SLC] uid %d gid %d not requested, cannot set read invalidate",
			ctx->id, param->uid, param->gid);
	}
	mutex_unlock(&param->param_mutex);
}

void mtk_vdec_slc_gid_release(struct mtk_vcodec_ctx *ctx, struct slc_param *param)
{
	mutex_lock(&param->param_mutex);
	param->used_cnt--;

	if (param->used_cnt == 0) {
		if (param->is_requested) {
			slbc_invalidate(param->uid, param->gid);
			mtk_v4l2_debug(4, "[%d][SLC] uid %d gid %d invalidated",
				ctx->id, param->uid, param->gid);
			slbc_gid_release(param->uid, param->gid);
			mtk_v4l2_debug(1, "[%d][SLC] uid %d gid %d released",
				ctx->id, param->uid, param->gid);
		}
		param->gid = -1;
		param->is_requested = false;

		if (param->uid == ID_VDEC_FRAME)
			ctx->dev->queued_frame = false;
	} else if (param->is_requested) {
		mtk_v4l2_debug(4, "[%d][SLC] uid %d gid %d, used_cnt decreased to %d, reserved for roi update",
			ctx->id, param->uid, param->gid, param->used_cnt);
	} else {
		mtk_v4l2_err("[%d][SLC] uid %d gid %d used_cnt %d not requested",
			ctx->id, param->uid, param->gid, param->used_cnt);
	}
	mutex_unlock(&param->param_mutex);
}

void mtk_vdec_slc_fill_data(struct mtk_vcodec_ctx *ctx, struct slc_param *param)
{
	int idx;

	if (param->uid == ID_VDEC_FRAME)
		idx = 0;
	else if (param->uid == ID_VDEC_UBE)
		idx = 1;
	else {
		mtk_v4l2_err("[SLC] invalid uid");
		return;
	}
	// TODO: choose bw table?

	param->data.sign = 0x51ca11ca;
	param->data.dma_size = slc_roi_8k[idx][0];
	param->data.bw = slc_roi_8k[idx][1];

	mtk_v4l2_debug(4, "[%d][SLC] uid %d set slc data, dma_size: %d, bw: %d",
		ctx->id, param->uid, param->data.dma_size, param->data.bw);
}

void mtk_vdec_slc_update_roi(struct mtk_vcodec_ctx *ctx, struct slc_param *param)
{
	mtk_vdec_slc_fill_data(ctx, param);
	slbc_roi_update(param->uid, param->gid, &param->data);
}

