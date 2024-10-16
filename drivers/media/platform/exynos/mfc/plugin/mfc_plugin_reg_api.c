/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_reg_api.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_plugin_reg_api.h"

void mfc_fg_set_geometry(struct mfc_core *core, struct mfc_ctx *ctx)
{
	int compressed;
	unsigned int reg = 0;

	/* resolution */
	MFC_CORE_WRITEL(ctx->img_width, REG_FG_PIC_SIZE_X);
	MFC_CORE_WRITEL(ctx->img_height, REG_FG_PIC_SIZE_Y);

	/* bit depth */
	MFC_CORE_WRITEL(ctx->bit_depth_luma, REG_FG_BIT_DEPTH);

	/* compressor ctrl [8]: rdma, [9]: wdma */
	compressed = IS_SBWC_FMT(ctx->internal_fmt);
	mfc_clear_set_bits(reg, 0x1, 8, compressed);
	mfc_ctx_debug(2, "[PLUGIN] rdma compressed %d (reg: %#x)\n", compressed, reg);

	compressed = IS_SBWC_FMT(ctx->dst_fmt);
	mfc_clear_set_bits(reg, 0x1, 9, compressed);
	mfc_ctx_debug(2, "[PLUGIN] wdma compressed %d (reg: %#x)\n", compressed, reg);

	/* IP on */
	mfc_clear_set_bits(reg, 0x1, 0, 1);

	MFC_CORE_WRITEL(reg, REG_FG_CTRL_REG);
}

void mfc_fg_set_sei_info(struct mfc_core *core, struct mfc_ctx *ctx, int index)
{
	struct mfc_dec *dec = ctx->dec_priv;
	unsigned int reg = 0;
	int i;

	mfc_ctx_debug(2, "[FILMGR set] index %d\n", index);

	reg = dec->FilmGrain[index][0];
	if (reg & 0x1) {
		for(i = 0; i < 44; i++)
			MFC_CORE_WRITEL(dec->FilmGrain[index][i],
					REG_FG_AV1_FILM_GRAIN_0 + (i * 4));
		if (core->dev->debugfs.debug_level >= 5)
			print_hex_dump(KERN_ERR, "[FILMGR set] ", DUMP_PREFIX_OFFSET, 32, 4,
					core->regs_base + REG_FG_AV1_FILM_GRAIN_0, 0xB0, false);
	} else {
		/* Bypass without film grain */
		reg = MFC_CORE_READL(REG_FG_CTRL_REG);
		mfc_clear_set_bits(reg, 0x1, 4, 1);
		MFC_CORE_WRITEL(reg, REG_FG_CTRL_REG);
		mfc_ctx_debug(5, "[FILMGR set] there is no apply_grain: %#x\n", reg);
	}
}

static unsigned int __mfc_plugin_set_pixel_format(struct mfc_ctx *ctx, struct mfc_fmt *format)
{
	unsigned int pix_val = 0;
	unsigned int reg = 0;

	switch (format->fourcc) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12N:
	case V4L2_PIX_FMT_NV12M_SBWC_8B:
	case V4L2_PIX_FMT_NV12N_SBWC_8B:
	case V4L2_PIX_FMT_NV12N_SBWC_256_8B:
		pix_val = 7;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21M_SBWC_8B:
		pix_val = 8;
		break;
	case V4L2_PIX_FMT_NV12M_P010:
	case V4L2_PIX_FMT_NV12N_P010:
	case V4L2_PIX_FMT_NV12M_SBWC_10B:
	case V4L2_PIX_FMT_NV12N_SBWC_10B:
	case V4L2_PIX_FMT_NV12N_SBWC_256_10B:
		pix_val = 32;
		break;
	case V4L2_PIX_FMT_NV21M_P010:
	case V4L2_PIX_FMT_NV21M_SBWC_10B:
		pix_val = 33;
		break;
	default:
		pix_val = 7;
		break;
	}

	mfc_set_bits(reg, 0x3F, 8, pix_val);
	mfc_ctx_debug(2, "[PLUGIN] pixel format: %s, pix_val: %d, reg: %#x\n",
			format->name, pix_val, reg);

	return reg;
}

static int __mfc_fg_get_align_width(struct mfc_ctx *ctx, int compressed)
{
	if (compressed)
		return ALIGN(ctx->img_width, 32);
	else
		return ALIGN(ctx->img_width, 2);
}

static int __mfc_fg_get_align_height(struct mfc_ctx *ctx, int compressed)
{
	if (compressed)
		return ALIGN(ctx->img_height, 4);
	else
		return ALIGN(ctx->img_height, 2);
}

void mfc_fg_set_rdma(struct mfc_core *core, struct mfc_ctx *ctx, struct mfc_buf *mfc_buf)
{
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_raw_info *raw;
	dma_addr_t addr;
	unsigned int reg = 0;
	int i, compressed, align;

	mfc_ctx_debug(2, "[PLUGIN] RDMA setting\n");
	MFC_CORE_WRITEL(1, REG_FG_RDMA_ENABLE);

	compressed = IS_SBWC_FMT(ctx->internal_fmt);
	align = IS_SBWC_256ALIGN(ctx->internal_fmt);
	mfc_set_bits(reg, 0x1, 6, align);
	mfc_set_bits(reg, 0x3, 0, compressed);
	MFC_CORE_WRITEL(reg, REG_FG_RDMA_COMP_CONTROL);

	reg = __mfc_plugin_set_pixel_format(ctx, ctx->internal_fmt);
	MFC_CORE_WRITEL(reg, REG_FG_RDMA_DATA_FORMAT);

	MFC_CORE_WRITEL(__mfc_fg_get_align_width(ctx, compressed), REG_FG_RDMA_WIDTH);
	MFC_CORE_WRITEL(__mfc_fg_get_align_height(ctx, compressed), REG_FG_RDMA_HEIGHT);

	/* stride and address */
	raw = &ctx->internal_raw_buf;
	for (i = 0; i < raw->num_planes; i++) {
		addr = dec->dpb[mfc_buf->dpb_index].addr[i];
		MFC_CORE_WRITEL(raw->stride[i], REG_FG_RDMA_IMG_STRIDE_1P + (i * 4));
		MFC_FG_DMA_WRITEL(addr, REG_FG_RDMA_IMG_BASE_ADDR_1P_FRO0 + (i * 0x40));
		if (compressed) {
			MFC_CORE_WRITEL(raw->stride_2bits[i],
					REG_FG_RDMA_HEADER_STRIDE_1P + (i * 4));
			MFC_FG_DMA_WRITEL((addr + raw->plane_size[i]),
					REG_FG_RDMA_HEADER_BASE_ADDR_1P_FRO0 + (i * 0x40));
		}
		mfc_ctx_debug(2, "[PLUGIN] plane[%d] src index %d, addr %#llx\n",
				i, mfc_buf->dpb_index, addr);
	}
}

void mfc_fg_set_wdma(struct mfc_core *core, struct mfc_ctx *ctx, struct mfc_buf *mfc_buf)
{
	struct mfc_raw_info *raw;
	dma_addr_t addr;
	unsigned int reg = 0;
	int i, compressed, align;

	mfc_ctx_debug(2, "[PLUGIN] WDMA setting\n");
        MFC_CORE_WRITEL(1, REG_FG_WDMA_LUM_ENABLE);
        MFC_CORE_WRITEL(1, REG_FG_WDMA_CHR_ENABLE);

	compressed = IS_SBWC_FMT(ctx->dst_fmt);
	align = IS_SBWC_256ALIGN(ctx->dst_fmt);
	mfc_set_bits(reg, 0x1, 6, align);
	mfc_set_bits(reg, 0x3, 0, compressed);
	MFC_CORE_WRITEL(reg, REG_FG_WDMA_LUM_COMP_CONTROL);
	MFC_CORE_WRITEL(reg, REG_FG_WDMA_CHR_COMP_CONTROL);

	reg = __mfc_plugin_set_pixel_format(ctx, ctx->dst_fmt);
	MFC_CORE_WRITEL(reg, REG_FG_WDMA_LUM_DATA_FORMAT);
	MFC_CORE_WRITEL(reg, REG_FG_WDMA_CHR_DATA_FORMAT);

	MFC_CORE_WRITEL(__mfc_fg_get_align_width(ctx, compressed), REG_FG_WDMA_LUM_WIDTH);
	MFC_CORE_WRITEL(__mfc_fg_get_align_width(ctx, compressed), REG_FG_WDMA_CHR_WIDTH);
	MFC_CORE_WRITEL(__mfc_fg_get_align_height(ctx, compressed), REG_FG_WDMA_LUM_HEIGHT);
	MFC_CORE_WRITEL(__mfc_fg_get_align_height(ctx, compressed), REG_FG_WDMA_CHR_HEIGHT);

	/* stride and address */
	raw = &ctx->raw_buf;
	for (i = 0; i < raw->num_planes; i++) {
		addr = mfc_buf->addr[0][i];
		MFC_CORE_WRITEL(raw->stride[i], REG_FG_WDMA_LUM_IMG_STRIDE_1P + (i * 0x1000));
		MFC_FG_DMA_WRITEL(addr,	REG_FG_WDMA_LUM_IMG_BASE_ADDR_1P_FRO0 + (i * 0x1000));
		if (compressed) {
			MFC_CORE_WRITEL(raw->stride_2bits[i],
					REG_FG_WDMA_LUM_HEADER_STRIDE_1P + (i * 0x1000));
			MFC_FG_DMA_WRITEL((addr + raw->plane_size[i]),
					REG_FG_WDMA_LUM_HEADER_BASE_ADDR_1P_FRO0 + (i * 0x1000));
		}
		mfc_ctx_debug(2, "[PLUGIN] plane[%d] dst index %d, addr %#llx\n",
				i, mfc_buf->vb.vb2_buf.index, addr);
	}
}
