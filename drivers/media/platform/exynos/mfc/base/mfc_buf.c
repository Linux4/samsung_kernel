/*
 * drivers/media/platform/exynos/mfc/mfc_buf.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <soc/samsung/exynos-smc.h>
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/exynos/imgloader.h>
#endif
#include <linux/firmware.h>
#include <linux/iommu.h>

#include "mfc_buf.h"

#include "mfc_mem.h"

#include "mfc_utils.h"

/* Release context buffers for SYS_INIT */
static void __mfc_release_common_context(struct mfc_core *core,
					enum mfc_buf_usage_type buf_type)
{
	struct mfc_special_buf *ctx_buf;

	ctx_buf = &core->common_ctx_buf;
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	if (buf_type == MFCBUF_DRM)
		ctx_buf = &core->drm_common_ctx_buf;
#endif

	mfc_iova_pool_free(core->dev, ctx_buf);

	mfc_mem_special_buf_free(core->dev, ctx_buf);

	ctx_buf->dma_buf = NULL;
	ctx_buf->vaddr = NULL;
	ctx_buf->daddr = 0;
}

/* Release context buffers for SYS_INIT */
void mfc_release_common_context(struct mfc_core *core)
{
	__mfc_release_common_context(core, MFCBUF_NORMAL);
	mfc_core_change_fw_state(core, 0, MFC_CTX_ALLOC, 0);

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	__mfc_release_common_context(core, MFCBUF_DRM);
	mfc_core_change_fw_state(core, 1, MFC_CTX_ALLOC, 0);
#endif
}

static int __mfc_alloc_common_context(struct mfc_core *core,
			enum mfc_buf_usage_type buf_type)
{
	struct mfc_dev *dev = core->dev;
	struct mfc_special_buf *ctx_buf;
	struct mfc_ctx_buf_size *buf_size;

	mfc_core_debug_enter();

	ctx_buf = &core->common_ctx_buf;
	ctx_buf->buftype = MFCBUF_NORMAL;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	if (buf_type == MFCBUF_DRM) {
		ctx_buf = &core->drm_common_ctx_buf;
		ctx_buf->buftype = MFCBUF_DRM;
	}
#endif

	buf_size = dev->variant->buf_size->ctx_buf;
	ctx_buf->size = buf_size->dev_ctx;

	snprintf(ctx_buf->name, MFC_NUM_SPECIAL_BUF_NAME, "MFC%d common context", core->id);
	if (mfc_mem_special_buf_alloc(dev, ctx_buf)) {
		mfc_core_err("Allocating %s context buffer failed\n",
				buf_type == MFCBUF_DRM ? "secure" : "normal");
		return -ENOMEM;
	}

	if (mfc_iova_pool_alloc(dev, ctx_buf)) {
		mfc_core_err("[POOL] failed to get iova\n");
		__mfc_release_common_context(core, buf_type);
		return -ENOMEM;
	}

	mfc_core_debug_leave();

	return 0;
}

/* Wrapper : allocate context buffers for SYS_INIT */
int mfc_alloc_common_context(struct mfc_core *core)
{
	int ret = 0;

	ret = __mfc_alloc_common_context(core, MFCBUF_NORMAL);
	if (ret)
		return ret;
	mfc_core_change_fw_state(core, 0, MFC_CTX_ALLOC, 1);

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	ret = __mfc_alloc_common_context(core, MFCBUF_DRM);
	if (ret) {
		mfc_core_change_fw_state(core, 0, MFC_CTX_ALLOC, 0);
		return ret;
	}

	mfc_core_change_fw_state(core, 1, MFC_CTX_ALLOC, 1);
#endif

	return ret;
}

/* Release instance buffer */
void mfc_release_instance_context(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;

	mfc_debug_enter();

	mfc_iova_pool_free(core->dev, &core_ctx->instance_ctx_buf);

	mfc_mem_special_buf_free(core->dev, &core_ctx->instance_ctx_buf);

	mfc_debug_leave();
}

/* Allocate memory for instance data buffer */
int mfc_alloc_instance_context(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dev *dev = ctx->dev;
	struct mfc_ctx_buf_size *buf_size;

	mfc_debug_enter();

	buf_size = dev->variant->buf_size->ctx_buf;

	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_DEC:
	case MFC_REG_CODEC_H264_MVC_DEC:
	case MFC_REG_CODEC_HEVC_DEC:
	case MFC_REG_CODEC_BPG_DEC:
		core_ctx->instance_ctx_buf.size = buf_size->h264_dec_ctx;
		break;
	case MFC_REG_CODEC_MPEG4_DEC:
	case MFC_REG_CODEC_H263_DEC:
	case MFC_REG_CODEC_VC1_RCV_DEC:
	case MFC_REG_CODEC_VC1_DEC:
	case MFC_REG_CODEC_MPEG2_DEC:
	case MFC_REG_CODEC_VP8_DEC:
	case MFC_REG_CODEC_VP9_DEC:
	case MFC_REG_CODEC_FIMV1_DEC:
	case MFC_REG_CODEC_FIMV2_DEC:
	case MFC_REG_CODEC_FIMV3_DEC:
	case MFC_REG_CODEC_FIMV4_DEC:
		core_ctx->instance_ctx_buf.size = buf_size->other_dec_ctx;
		break;
	case MFC_REG_CODEC_H264_ENC:
	case MFC_REG_CODEC_AV1_DEC:
		core_ctx->instance_ctx_buf.size = buf_size->h264_enc_ctx;
		break;
	case MFC_REG_CODEC_HEVC_ENC:
	case MFC_REG_CODEC_BPG_ENC:
		core_ctx->instance_ctx_buf.size = buf_size->hevc_enc_ctx;
		break;
	case MFC_REG_CODEC_MPEG4_ENC:
	case MFC_REG_CODEC_H263_ENC:
	case MFC_REG_CODEC_VP8_ENC:
	case MFC_REG_CODEC_VP9_ENC:
		core_ctx->instance_ctx_buf.size = buf_size->other_enc_ctx;
		break;
	default:
		core_ctx->instance_ctx_buf.size = 0;
		mfc_err("Codec type(%d) should be checked!\n",
				ctx->codec_mode);
		return -ENOMEM;
	}

	if (ctx->is_drm)
		core_ctx->instance_ctx_buf.buftype = MFCBUF_DRM;
	else
		core_ctx->instance_ctx_buf.buftype = MFCBUF_NORMAL;

	snprintf(core_ctx->instance_ctx_buf.name, MFC_NUM_SPECIAL_BUF_NAME,
			"MFC%d ctx%d instance", core_ctx->core->id, core_ctx->num);
	if (mfc_mem_special_buf_alloc(dev, &core_ctx->instance_ctx_buf)) {
		mfc_err("Allocating context buffer failed\n");
		return -ENOMEM;
	}

	if (mfc_iova_pool_alloc(dev, &core_ctx->instance_ctx_buf)) {
		mfc_err("[POOL] failed to get iova\n");
		mfc_release_instance_context(core_ctx);
		return -ENOMEM;
	}

	mfc_debug_leave();

	return 0;
}

static void __mfc_dec_calc_codec_buffer_size(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dec *dec = ctx->dec_priv;

	/* Codecs have different memory requirements */
	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_DEC:
	case MFC_REG_CODEC_H264_MVC_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		ctx->mv_buf.size = dec->mv_count * ctx->mv_size;
		break;
	case MFC_REG_CODEC_MPEG4_DEC:
	case MFC_REG_CODEC_FIMV1_DEC:
	case MFC_REG_CODEC_FIMV2_DEC:
	case MFC_REG_CODEC_FIMV3_DEC:
	case MFC_REG_CODEC_FIMV4_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		if (dec->loop_filter_mpeg4) {
			ctx->loopfilter_luma_size = ALIGN(ctx->raw_buf.plane_size[0], 256);
			ctx->loopfilter_chroma_size = ALIGN(ctx->raw_buf.plane_size[1] +
							ctx->raw_buf.plane_size[2], 256);
			core_ctx->codec_buf.size = ctx->scratch_buf_size +
				(NUM_MPEG4_LF_BUF * (ctx->loopfilter_luma_size +
						     ctx->loopfilter_chroma_size));
		} else {
			core_ctx->codec_buf.size = ctx->scratch_buf_size;
		}
		break;
	case MFC_REG_CODEC_VC1_RCV_DEC:
	case MFC_REG_CODEC_VC1_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_MPEG2_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_H263_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_VP8_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_VP9_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size +
			DEC_STATIC_BUFFER_SIZE;
		break;
	case MFC_REG_CODEC_AV1_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size +
			DEC_AV1_STATIC_BUFFER_SIZE(ctx->img_width, ctx->img_height);
		ctx->mv_buf.size = dec->mv_count * ctx->mv_size;
		break;
	case MFC_REG_CODEC_HEVC_DEC:
	case MFC_REG_CODEC_BPG_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		ctx->mv_buf.size = dec->mv_count * ctx->mv_size;
		break;
	default:
		core_ctx->codec_buf.size = 0;
		mfc_err("invalid codec type: %d\n", ctx->codec_mode);
		break;
	}

	mfc_debug(2, "[MEMINFO] scratch: %zu, MV: %zu x count %d\n",
			ctx->scratch_buf_size, ctx->mv_size, dec->mv_count);
	if (dec->loop_filter_mpeg4)
		mfc_debug(2, "[MEMINFO] (loopfilter luma: %zu, chroma: %zu) x count %d\n",
				ctx->loopfilter_luma_size, ctx->loopfilter_chroma_size,
				NUM_MPEG4_LF_BUF);
}

static void __mfc_enc_calc_codec_buffer_size(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_enc *enc;
	unsigned int mb_width, mb_height;
	unsigned int lcu_width = 0, lcu_height = 0;

	enc = ctx->enc_priv;
	enc->tmv_buffer_size = 0;

	mb_width = WIDTH_MB(ctx->crop_width);
	mb_height = HEIGHT_MB(ctx->crop_height);

	lcu_width = ENC_LCU_WIDTH(ctx->crop_width);
	lcu_height = ENC_LCU_HEIGHT(ctx->crop_height);

	if (IS_SBWC_DPB(ctx) && ctx->is_10bit) {
		enc->luma_dpb_size = ENC_SBWC_LUMA_10B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
		enc->chroma_dpb_size = ENC_SBWC_CHROMA_10B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
	} else if (IS_SBWC_DPB(ctx) && !ctx->is_10bit) {
		enc->luma_dpb_size = ENC_SBWC_LUMA_8B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
		enc->chroma_dpb_size = ENC_SBWC_CHROMA_8B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
	} else {
		/* default recon buffer size, it can be changed in case of 422, 10bit */
		enc->luma_dpb_size =
			ALIGN(ENC_LUMA_DPB_SIZE(ctx->crop_width, ctx->crop_height), 64);
		enc->chroma_dpb_size =
			ALIGN(ENC_CHROMA_DPB_SIZE(ctx->crop_width, ctx->crop_height), 64);
	}

	/* Codecs have different memory requirements */
	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_ENC:
		enc->me_buffer_size =
			ALIGN(ENC_V100_H264_ME_SIZE(mb_width, mb_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_MPEG4_ENC:
	case MFC_REG_CODEC_H263_ENC:
		enc->me_buffer_size =
			ALIGN(ENC_V100_MPEG4_ME_SIZE(mb_width, mb_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_VP8_ENC:
		enc->me_buffer_size =
			ALIGN(ENC_V100_VP8_ME_SIZE(mb_width, mb_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_VP9_ENC:
		if (!IS_SBWC_DPB(ctx) && (ctx->is_10bit || ctx->is_422)) {
			enc->luma_dpb_size =
				ALIGN(ENC_VP9_LUMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			enc->chroma_dpb_size =
				ALIGN(ENC_VP9_CHROMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			mfc_debug(2, "[10BIT] VP9 10bit or 422 recon luma size: %zu chroma size: %zu\n",
					enc->luma_dpb_size, enc->chroma_dpb_size);
		}
		enc->me_buffer_size =
			ALIGN(ENC_V100_VP9_ME_SIZE(lcu_width, lcu_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
					   enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_HEVC_ENC:
	case MFC_REG_CODEC_BPG_ENC:
		if (!IS_SBWC_DPB(ctx) && (ctx->is_10bit || ctx->is_422)) {
			enc->luma_dpb_size =
				ALIGN(ENC_HEVC_LUMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			enc->chroma_dpb_size =
				ALIGN(ENC_HEVC_CHROMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			mfc_debug(2, "[10BIT] HEVC 10bit or 422 recon luma size: %zu chroma size: %zu\n",
					enc->luma_dpb_size, enc->chroma_dpb_size);
		}
		enc->me_buffer_size =
			ALIGN(ENC_V100_HEVC_ME_SIZE(lcu_width, lcu_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
					   enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	default:
		core_ctx->codec_buf.size = 0;
		mfc_err("invalid codec type: %d\n", ctx->codec_mode);
		break;
	}

	mfc_debug(2, "[MEMINFO] scratch: %zu, TMV: %zu, (recon luma: %zu, chroma: %zu, me: %zu) x count %d\n",
			ctx->scratch_buf_size, enc->tmv_buffer_size,
			enc->luma_dpb_size, enc->chroma_dpb_size, enc->me_buffer_size,
			ctx->dpb_count);
}

/* Allocate codec buffers */
int mfc_alloc_codec_buffers(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx *ctx = core_ctx->ctx;

	mfc_debug_enter();

	if (ctx->type == MFCINST_DECODER) {
		__mfc_dec_calc_codec_buffer_size(core_ctx);
	} else if (ctx->type == MFCINST_ENCODER) {
		__mfc_enc_calc_codec_buffer_size(core_ctx);
	} else {
		mfc_err("invalid type: %d\n", ctx->type);
		return -EINVAL;
	}

	if (ctx->is_drm) {
		core_ctx->codec_buf.buftype = MFCBUF_DRM;
		ctx->mv_buf.buftype = MFCBUF_DRM;
	} else {
		core_ctx->codec_buf.buftype = MFCBUF_NORMAL;
		ctx->mv_buf.buftype = MFCBUF_NORMAL;
	}

	if (core_ctx->codec_buf.size > 0) {
		snprintf(core_ctx->codec_buf.name, MFC_NUM_SPECIAL_BUF_NAME,
				"MFC%d ctx%d codec", core->id, core_ctx->num);
		if (mfc_mem_special_buf_alloc(dev, &core_ctx->codec_buf)) {
			mfc_err("Allocating codec buffer failed\n");
			return -ENOMEM;
		}
		core_ctx->codec_buffer_allocated = 1;
	} else if (ctx->codec_mode == MFC_REG_CODEC_MPEG2_DEC) {
		core_ctx->codec_buffer_allocated = 1;
	}

	if (!ctx->mv_buffer_allocated && ctx->mv_buf.size > 0) {
		snprintf(ctx->mv_buf.name, MFC_NUM_SPECIAL_BUF_NAME,
				"MFC%d ctx%d MV", core->id, ctx->num);
		if (mfc_mem_special_buf_alloc(dev, &ctx->mv_buf)) {
			mfc_err("Allocating MV buffer failed\n");
			return -ENOMEM;
		}
		ctx->mv_buffer_allocated = 1;
	}

	mfc_debug_leave();

	return 0;
}

/* Release buffers allocated for codec */
void mfc_release_codec_buffers(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;

	if (core_ctx->codec_buffer_allocated) {
		mfc_mem_special_buf_free(ctx->dev, &core_ctx->codec_buf);
		core_ctx->codec_buffer_allocated = 0;
	}

	if (ctx->mv_buffer_allocated) {
		mfc_mem_special_buf_free(ctx->dev, &ctx->mv_buf);
		ctx->mv_buffer_allocated = 0;
	}

	mfc_release_scratch_buffer(core_ctx);
}

int mfc_alloc_scratch_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx *ctx = core_ctx->ctx;

	mfc_debug_enter();

	if (core_ctx->scratch_buffer_allocated) {
		mfc_mem_special_buf_free(dev, &core_ctx->scratch_buf);
		core_ctx->scratch_buffer_allocated = 0;
		mfc_debug(2, "[MEMINFO] Release the scratch buffer ctx[%d]\n",
				core_ctx->num);
	}

	if (ctx->is_drm)
		core_ctx->scratch_buf.buftype = MFCBUF_DRM;
	else
		core_ctx->scratch_buf.buftype = MFCBUF_NORMAL;

	core_ctx->scratch_buf.size =  ALIGN(ctx->scratch_buf_size, 256);
	if (core_ctx->scratch_buf.size > 0) {
		snprintf(core_ctx->scratch_buf.name, MFC_NUM_SPECIAL_BUF_NAME,
				"MFC%d ctx%d scratch", core->id, core_ctx->num);
		if (mfc_mem_special_buf_alloc(dev, &core_ctx->scratch_buf)) {
			mfc_err("Allocating scratch_buf buffer failed\n");
			return -ENOMEM;
		}
		core_ctx->scratch_buffer_allocated = 1;
	}

	mfc_debug_leave();

	return 0;
}

void mfc_release_scratch_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;

	mfc_debug_enter();
	if (core_ctx->scratch_buffer_allocated) {
		mfc_mem_special_buf_free(ctx->dev, &core_ctx->scratch_buf);
		core_ctx->scratch_buffer_allocated = 0;
	}
	mfc_debug_leave();
}

int mfc_alloc_internal_dpb(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_raw_info *raw;
	int i;

	raw = &ctx->internal_raw_buf;
	for (i = 0; i < dec->total_dpb_count; i++) {
		if (ctx->is_drm)
			dec->internal_dpb[i].buftype = MFCBUF_DRM;
		else
			dec->internal_dpb[i].buftype = MFCBUF_NORMAL;
		dec->internal_dpb[i].size = raw->total_plane_size;
		snprintf(dec->internal_dpb[i].name, MFC_NUM_SPECIAL_BUF_NAME,
				"ctx%d internal DPB", ctx->num);
		if (mfc_mem_special_buf_alloc(dev, &dec->internal_dpb[i])) {
			mfc_ctx_err("[MEMINFO][PLUGIN] Allocating internal DPB failed\n");
			return -ENOMEM;
		}
	}

	return 0;
}

void mfc_release_internal_dpb(struct mfc_ctx *ctx)
{
	struct mfc_dec *dec = ctx->dec_priv;
	int i;

	for (i = 0; i < dec->total_dpb_count; i++) {
		if (!dec->internal_dpb[i].dma_buf) {
			mfc_ctx_debug(2, "[MEMINFO][PLUGIN] internal DPB is already freed\n");
			continue;
		}

		mfc_mem_special_buf_free(ctx->dev, &dec->internal_dpb[i]);
	}
}

/* Allocation buffer of debug infor memory for FW debugging */
int mfc_alloc_dbg_info_buffer(struct mfc_core *core)
{
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx_buf_size *buf_size = dev->variant->buf_size->ctx_buf;

	mfc_core_debug(2, "Allocate a debug-info buffer\n");

	core->dbg_info_buf.buftype = MFCBUF_NORMAL;
	core->dbg_info_buf.size = buf_size->dbg_info_buf;
	snprintf(core->dbg_info_buf.name, MFC_NUM_SPECIAL_BUF_NAME,
			"MFC%d debug", core->id);
	if (mfc_mem_special_buf_alloc(dev, &core->dbg_info_buf)) {
		mfc_core_err("Allocating debug info buffer failed\n");
		return -ENOMEM;
	}

	if (mfc_iova_pool_alloc(dev, &core->dbg_info_buf)) {
		mfc_core_err("[POOL] failed to get iova\n");
		mfc_release_dbg_info_buffer(core);
		return -ENOMEM;
	}

	return 0;
}

/* Release buffer of debug infor memory for FW debugging */
void mfc_release_dbg_info_buffer(struct mfc_core *core)
{
	if (!core->dbg_info_buf.dma_buf)
		mfc_core_debug(2, "debug info buffer is already freed\n");

	mfc_iova_pool_free(core->dev, &core->dbg_info_buf);

	mfc_mem_special_buf_free(core->dev, &core->dbg_info_buf);
}

/* Allocation buffer of ROI macroblock information */
static int __mfc_alloc_enc_roi_buffer(struct mfc_core_ctx *core_ctx,
			size_t size, struct mfc_special_buf *roi_buf)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_dev *dev = core->dev;

	roi_buf->size = size;
	roi_buf->buftype = MFCBUF_NORMAL;

	if (roi_buf->dma_buf == NULL) {
		snprintf(roi_buf->name, MFC_NUM_SPECIAL_BUF_NAME,
				"ctx%d ROI", core_ctx->num);
		if (mfc_mem_special_buf_alloc(dev, roi_buf)) {
			mfc_err("[ROI] Allocating ROI buffer failed\n");
			return -ENOMEM;
		}
	}
	memset(roi_buf->vaddr, 0, roi_buf->size);

	return 0;
}

/* Wrapper : allocation ROI buffers */
int mfc_alloc_enc_roi_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_enc *enc = ctx->enc_priv;
	unsigned int mb_width, mb_height;
	unsigned int lcu_width = 0, lcu_height = 0;
	size_t size;
	int i;

	mb_width = WIDTH_MB(ctx->crop_width);
	mb_height = HEIGHT_MB(ctx->crop_height);

	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_ENC:
		size = ((((mb_width * (mb_height + 1) / 2) + 15) / 16) * 16) * 2;
		break;
	case MFC_REG_CODEC_MPEG4_ENC:
	case MFC_REG_CODEC_VP8_ENC:
		size = mb_width * mb_height;
		break;
	case MFC_REG_CODEC_VP9_ENC:
		lcu_width = (ctx->crop_width + 63) / 64;
		lcu_height = (ctx->crop_height + 63) / 64;
		size = lcu_width * lcu_height * 4;
		break;
	case MFC_REG_CODEC_HEVC_ENC:
		lcu_width = (ctx->crop_width + 31) / 32;
		lcu_height = (ctx->crop_height + 31) / 32;
		size = lcu_width * lcu_height;
		break;
	default:
		mfc_debug(2, "ROI not supported codec type(%d). Allocate with default size\n",
				ctx->codec_mode);
		size = mb_width * mb_height;
		break;
	}

	for (i = 0; i < MFC_MAX_EXTRA_BUF; i++) {
		if (__mfc_alloc_enc_roi_buffer(core_ctx, size,
					&enc->roi_buf[i]) < 0) {
			mfc_err("[ROI] Allocating remapping buffer[%d] failed\n",
							i);
			return -ENOMEM;
		}
	}

	return 0;
}

/* Release buffer of ROI macroblock information */
void mfc_release_enc_roi_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_enc *enc = ctx->enc_priv;
	int i;

	for (i = 0; i < MFC_MAX_EXTRA_BUF; i++)
		if (enc->roi_buf[i].dma_buf)
			mfc_mem_special_buf_free(ctx->dev, &enc->roi_buf[i]);
}

int mfc_otf_alloc_stream_buf(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;
	struct _otf_handle *handle = ctx->otf_handle;
	struct _otf_debug *debug = &handle->otf_debug;
	struct mfc_special_buf *buf;
	struct mfc_raw_info *raw = &ctx->raw_buf;
	int i;

	mfc_ctx_debug_enter();

	for (i = 0; i < OTF_MAX_BUF; i++) {
		buf = &debug->stream_buf[i];
		buf->buftype = MFCBUF_NORMAL;
		buf->size = raw->total_plane_size;
		snprintf(buf->name, MFC_NUM_SPECIAL_BUF_NAME,
				"ctx%d OTF stream", ctx->num);
		if (mfc_mem_special_buf_alloc(dev, buf)) {
			mfc_ctx_err("[OTF] Allocating stream buffer failed\n");
			return -EINVAL;
		}
		memset(buf->vaddr, 0, raw->total_plane_size);
	}

	mfc_ctx_debug_leave();

	return 0;
}

void mfc_otf_release_stream_buf(struct mfc_ctx *ctx)
{
	struct _otf_handle *handle = ctx->otf_handle;
	struct _otf_debug *debug = &handle->otf_debug;
	struct mfc_special_buf *buf;
	int i;

	mfc_ctx_debug_enter();

	for (i = 0; i < OTF_MAX_BUF; i++) {
		buf = &debug->stream_buf[i];
		if (buf->dma_buf)
			mfc_mem_special_buf_free(ctx->dev, buf);
	}

	mfc_ctx_debug_leave();
}

int mfc_alloc_metadata_buffer(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;

	mfc_ctx_debug_enter();

	if (ctx->metadata_buffer_allocated)
		return 0;

	ctx->metadata_buf.size = HDR10_PLUS_DATA_SIZE * MFC_MAX_DPBS;
	if (ctx->is_drm)
		ctx->metadata_buf.buftype = MFCBUF_DRM;
	else
		ctx->metadata_buf.buftype = MFCBUF_NORMAL;

	snprintf(ctx->metadata_buf.name, MFC_NUM_SPECIAL_BUF_NAME,
			"ctx%d metadata", ctx->num);
	if (mfc_mem_special_buf_alloc(dev, &ctx->metadata_buf)) {
		mfc_ctx_err("Allocating metadata_buf buffer failed\n");
		return -ENOMEM;
	}
	ctx->metadata_buffer_allocated = 1;

	if (mfc_iova_pool_alloc(dev, &ctx->metadata_buf)) {
		mfc_ctx_err("[POOL] failed to get iova\n");
		mfc_release_metadata_buffer(ctx);
		return -ENOMEM;
	}

	mfc_ctx_debug_leave();

	return 0;
}

void mfc_release_metadata_buffer(struct mfc_ctx *ctx)
{
	mfc_ctx_debug_enter();

	mfc_iova_pool_free(ctx->dev, &ctx->metadata_buf);

	if (ctx->metadata_buffer_allocated) {
		mfc_mem_special_buf_free(ctx->dev, &ctx->metadata_buf);
		ctx->metadata_buffer_allocated = 0;
	}

	mfc_ctx_debug_leave();
}

/* Allocate firmware */
int mfc_alloc_firmware(struct mfc_core *core)
{
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx_buf_size *buf_size;
	struct mfc_special_buf *fw_buf;

	mfc_core_debug_enter();

	buf_size = dev->variant->buf_size->ctx_buf;

	if (core->fw_buf.sgt)
		return 0;

	mfc_core_debug(4, "[F/W] Allocating memory for firmware\n");

	core->fw_buf.size = dev->variant->buf_size->firmware_code;
	core->fw_buf.buftype = MFCBUF_NORMAL_FW;
	/* The normal firmware name should be "MFC0/1" for S2MPU */
	strncpy(core->fw_buf.name, core->name, sizeof(core->name));
	if (mfc_mem_special_buf_alloc(dev, &core->fw_buf)) {
		mfc_core_err("[F/W] Allocating normal firmware buffer failed\n");
		return -ENOMEM;
	}

	fw_buf = &core->fw_buf;
	if (mfc_iommu_map_firmware(core, fw_buf))
		goto err_reserve_iova;

	mfc_core_change_fw_state(core, 0, MFC_FW_ALLOC, 1);

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	core->drm_fw_buf.buftype = MFCBUF_DRM_FW;
	core->drm_fw_buf.size = dev->variant->buf_size->firmware_code;
	/* The drm firmware name should be "TZMP2_MFC0/1" for S2MPU */
	snprintf(core->drm_fw_buf.name, MFC_NUM_SPECIAL_BUF_NAME, "TZMP2_MFC%d", core->id);
	if (mfc_mem_special_buf_alloc(dev, &core->drm_fw_buf)) {
		mfc_core_err("[F/W] Allocating DRM firmware buffer failed\n");
		goto err_reserve_iova;
	}

	if (mfc_iommu_map_firmware(core, &core->drm_fw_buf))
		goto err_reserve_iova_secure;

	mfc_core_change_fw_state(core, 1, MFC_FW_ALLOC, 1);
#endif

	mfc_core_debug_leave();

	return 0;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
err_reserve_iova_secure:
	mfc_mem_special_buf_free(dev, &core->drm_fw_buf);
#endif
err_reserve_iova:
	mfc_core_change_fw_state(core, 0, MFC_FW_ALLOC, 0);
	iommu_unmap(core->domain, fw_buf->daddr, fw_buf->map_size);
	mfc_mem_special_buf_free(dev, &core->fw_buf);
	return -ENOMEM;
}

/* Load firmware to MFC */
int mfc_load_firmware(struct mfc_core *core, struct mfc_special_buf *fw_buf,
		const u8 *fw_data, size_t fw_size)
{
	mfc_core_debug(2, "[MEMINFO][F/W] loaded %s F/W Size: %zu\n",
			fw_buf->buftype == MFCBUF_NORMAL_FW ? "normal" : "secure", fw_size);

	if (fw_size > fw_buf->size) {
		mfc_core_err("[MEMINFO][F/W] MFC firmware(%zu) is too big to be loaded in memory(%zu)\n",
				fw_size, fw_buf->size);
		return -ENOMEM;
	}

	core->fw.fw_size = fw_size;

	if (fw_buf->sgt == NULL || fw_buf->daddr == 0) {
		mfc_core_err("[F/W] MFC firmware is not allocated or was not mapped correctly\n");
		return -EINVAL;
	}

	/*  This adds to clear with '0' for firmware memory except code region. */
	mfc_core_debug(4, "[F/W] memset before memcpy for %s fw\n",
			fw_buf->buftype == MFCBUF_NORMAL_FW ? "normal" : "secure");
	memset((fw_buf->vaddr + fw_size), 0, (fw_buf->size - fw_size));
	memcpy(fw_buf->vaddr, fw_data, fw_size);

	/* cache flush for memcpy by CPU */
	dma_sync_sgtable_for_device(core->device, fw_buf->sgt, DMA_TO_DEVICE);
	mfc_core_debug(4, "[F/W] cache flush for %s FW region\n",
			fw_buf->buftype == MFCBUF_NORMAL_FW ? "normal" : "secure");

	if (fw_buf->buftype == MFCBUF_NORMAL_FW)
		mfc_core_change_fw_state(core, 0, MFC_FW_LOADED, 1);
	else
		mfc_core_change_fw_state(core, 1, MFC_FW_LOADED, 1);

	if (core->dev->debugfs.sfr_dump & MFC_DUMP_FIRMWARE) {
		print_hex_dump(KERN_ERR, "FW dump ", DUMP_PREFIX_OFFSET, 32, 1,
				fw_buf->vaddr, 0x200, false);
		mfc_core_info("......\n");
		print_hex_dump(KERN_ERR, "FW dump + 0xffe00 ", DUMP_PREFIX_OFFSET, 32, 1,
				fw_buf->vaddr + 0xffe00, 0x200, false);
	}

	return 0;
}

#if !IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static int __mfc_request_load_firmware(struct mfc_core *core, struct mfc_special_buf *fw_buf)
{
	const struct firmware *fw_blob;
	int ret;

	mfc_core_debug(4, "[F/W] Requesting %s F/W\n",
			fw_buf->buftype == MFCBUF_NORMAL_FW ? "normal" : "secure");

	ret = request_firmware(&fw_blob, MFC_FW_NAME, core->dev->v4l2_dev.dev);
	if (ret != 0) {
		mfc_core_err("[F/W] Couldn't find the F/W invalid path\n");
		return ret;
	}

	ret = mfc_load_firmware(core, fw_buf, fw_blob->data, fw_blob->size);
	if (ret) {
		mfc_core_err("[F/W] Failed to load the MFC F/W\n");
		release_firmware(fw_blob);
		return ret;
	}

	release_firmware(fw_blob);

	return 0;
}
#endif

/* Request and load firmware to MFC */
int mfc_request_load_firmware(struct mfc_core *core, struct mfc_special_buf *fw_buf)
{
	int ret;

	mfc_core_debug_enter();

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	if (fw_buf->buftype == MFCBUF_NORMAL_FW) {
		mfc_core_debug(4, "[F/W] Requesting imgloader boot for F/W\n");

		ret = imgloader_boot(&core->mfc_imgloader_desc);
		if (ret) {
			mfc_core_err("[F/W] imgloader boot failed.\n");
			return -EINVAL;
		}
		/* Imageloader verifies the FW directly after mem_setup() */
		mfc_core_change_fw_state(core, 0, MFC_FW_VERIFIED, 1);
	} else if (fw_buf->buftype == MFCBUF_DRM_FW) {
		ret = imgloader_boot(&core->mfc_imgloader_desc_drm);
		if (ret) {
			mfc_core_err("[F/W] imgloader boot failed.\n");
			return -EINVAL;
		}
		/* Imageloader verifies the FW directly after mem_setup() */
		mfc_core_change_fw_state(core, 1, MFC_FW_VERIFIED, 1);
	} else {
		mfc_core_err("[F/W] not supported FW buftype: %d\n", fw_buf->buftype);
		return -EINVAL;
	}
#else
	ret = __mfc_request_load_firmware(core, fw_buf);
	if (ret)
		return ret;
#endif

	mfc_core_debug_leave();

	return 0;
}

/* Release firmware memory */
int mfc_release_firmware(struct mfc_core *core)
{
	/* Before calling this function one has to make sure
	 * that MFC is no longer processing */
	if (core->fw_buf.daddr)
		iommu_unmap(core->domain, core->fw_buf.daddr, core->fw_buf.map_size);

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	/* free Secure-DVA region */
	if (core->drm_fw_buf.daddr)
		iommu_unmap(core->domain, core->drm_fw_buf.daddr, core->drm_fw_buf.map_size);
	mfc_mem_special_buf_free(core->dev, &core->drm_fw_buf);
#endif

	mfc_mem_special_buf_free(core->dev, &core->fw_buf);

	return 0;
}
