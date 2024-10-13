// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_dpp.c
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 * Authors:
 *	Seong-gyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <drm/drm_atomic.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_blend.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <soc/samsung/exynos/exynos-hvc.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2_exynos_media.h>

#include <exynos_drm_fb.h>
#include <exynos_drm_dpp.h>
#include <exynos_drm_format.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_debug.h>
#include <exynos_drm_modifier.h>
#include <exynos_drm_sfr_dma.h>
#include <exynos_drm_hdr.h>
#include <exynos_display_common.h>

#include <regs-dpp.h>

struct dpuf *dpuf_blkdata[MAX_DPUF_CNT];
struct dpp_device *dpp_drvdata[MAX_DPP_CNT];

static int dpu_dpp_log_level = 6;
module_param(dpu_dpp_log_level, int, 0600);
MODULE_PARM_DESC(dpu_dpp_log_level, "log level for dpu dpp [default : 6]");

#define dpp_info(dpp, fmt, ...)		\
dpu_pr_info(drv_name((dpp)), (dpp)->id, dpu_dpp_log_level, fmt, ##__VA_ARGS__)

#define dpp_warn(dpp, fmt, ...)		\
dpu_pr_warn(drv_name((dpp)), (dpp)->id, dpu_dpp_log_level, fmt, ##__VA_ARGS__)

#define dpp_err(dpp, fmt, ...)		\
dpu_pr_err(drv_name((dpp)), (dpp)->id, dpu_dpp_log_level, fmt, ##__VA_ARGS__)

#define dpp_debug(dpp, fmt, ...)	\
dpu_pr_debug(drv_name((dpp)), (dpp)->id, dpu_dpp_log_level, fmt, ##__VA_ARGS__)

#define P010_Y_SIZE(w, h)		((w) * (h) * 2)
#define P010_CBCR_SIZE(w, h)		((w) * (h))
#define P010_CBCR_BASE(base, w, h)	((base) + P010_Y_SIZE((w), (h)))

#define IN_RANGE(val, min, max)					\
	(((min) > 0 && (min) < (max) &&				\
	  (val) >= (min) && (val) <= (max)) ? true : false)

static const struct of_device_id dpp_of_match[] = {
	{
		.compatible = "samsung,exynos-dpp",
	}, {
	},
};

void dpp_dump(struct dpp_device *dpps[], int dpp_cnt)
{
	struct dpp_device *dpp;
	int i;

	for (i = 0 ; i < MAX_DPUF_CNT; i++) {
		dpp = dpps[i * DPP_PER_DPUF];
		__dpp_common_dump(i, &dpp->regs);
	}

	for (i = 0; i < dpp_cnt; i++){
		dpp = dpps[i];
		if (dpp->state != DPP_STATE_ON) {
			dpp_info(dpp, "dpp state is off\n");
			continue;
		}

		__dpp_dump(dpp->id, &dpp->regs);
		exynos_hdr_dump(dpp->hdr);
	}
}

__weak void __rcd_dump(u32 id, struct dpp_regs *regs) {}
void rcd_dump(struct dpp_device *dpp)
{
	if (dpp->state != DPP_STATE_ON) {
		dpp_info(dpp, "rcd state is off\n");
		return;
	}
	__rcd_dump(dpp->id, &dpp->regs);
}

void exynos_dpuf_set_ring_clk(struct dpuf *dpuf, bool en)
{
	struct dpuf_votf *votf = dpuf->votf;
	u32 dpuf_id = dpuf->id;

	if (!votf || !votf->enabled)
		return;

	spin_lock(&votf->slock);
	if (en) {
		if (votf->enable_count++ == 0) {
			votf_reg_set_global_init(dpuf_id, votf->base_addr, true);
			pr_debug("[%s] dpuf(%d) ring clk enabled\n", __func__,
					dpuf_id);
		}
	} else {
		if (--votf->enable_count == 0) {
			votf_reg_set_global_init(dpuf_id, votf->base_addr, false);
			pr_debug("[%s] dpuf(%d) ring clk disabled\n", __func__,
					dpuf_id);
		}
	}
	pr_debug("[%s] dpuf(%d) ring clk ref-cnt(%d)\n", __func__, dpuf_id,
			votf->enable_count);
	spin_unlock(&votf->slock);
}

int exynos_dpuf_set_votf(u32 dpuf_idx, bool en)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct dpuf *dpuf;
	struct dpuf_votf *votf;
	int ret = 0;

	if (dpuf_idx >= MAX_DPUF_CNT) {
		pr_err("[%s] invalid dpuf_idx(%d)\n", __func__, dpuf_idx);
		return -EINVAL;
	}

	dpuf = dpuf_blkdata[dpuf_idx];
	votf = dpuf->votf;
	if (!votf || !votf->enabled)
		return ret;

	if (en) {
		ret = pm_runtime_get_sync(decon->dev);

		exynos_dpuf_set_ring_clk(dpuf, true);
		votf_reg_init_trs(dpuf_idx);
	} else {
		exynos_dpuf_set_ring_clk(dpuf, false);
		ret = pm_runtime_put(decon->dev);
	}

	return ret;
}
EXPORT_SYMBOL(exynos_dpuf_set_votf);

void exynos_drm_init_resource_cnt(void)
{
	__dpp_init_dpuf_resource_cnt();
}

#define DPP_MAX_MODIFIERS	(10)
#define add_modifier(new, list, dpp, i) do {			\
	if ((i) >= DPP_MAX_MODIFIERS) {				\
		dpp_err((dpp), "failed to add modifers\n");	\
		return NULL;					\
	}							\
	(list)[(i)++] = (new);					\
} while (0)
static uint64_t *dpp_get_modifiers(struct dpp_device *dpp)
{
	int i = 0;
	static uint64_t modifiers[DPP_MAX_MODIFIERS];

	/* default modifiers*/
	add_modifier(DRM_FORMAT_MOD_SAMSUNG_COLORMAP, modifiers, dpp, i);
	add_modifier(DRM_FORMAT_MOD_PROTECTION, modifiers, dpp, i);

	if (dpp->dpuf->votf && dpp->dpuf->votf->enabled)
		add_modifier(DRM_FORMAT_MOD_SAMSUNG_VOTF(0), modifiers, dpp, i);
	if (test_bit(DPP_ATTR_SAJC, &dpp->attr))
		add_modifier(DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0), modifiers, dpp, i);
	if (test_bit(DPP_ATTR_AFBC, &dpp->attr))
		add_modifier(DRM_FORMAT_MOD_ARM_AFBC(0), modifiers, dpp, i);

	if (test_bit(DPP_ATTR_SBWC, &dpp->attr))
		add_modifier(DRM_FORMAT_MOD_SAMSUNG_SBWC(0, 0, 0), modifiers, dpp, i);

	add_modifier(DRM_FORMAT_MOD_INVALID, modifiers, dpp, i);

	return modifiers;
}

static unsigned long dpp_get_aclk(const struct dpp_device *dpp,
		const struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct drm_crtc *crtc = exynos_plane_state->base.crtc;
	const struct decon_device *decon;
	unsigned long aclk_khz, max_aclk_khz = 0x7FFFFFFF; /* max disp clock in khz */

	if (!crtc || IS_ENABLED(CONFIG_BOARD_EMULATOR))
		return max_aclk_khz;

	decon = to_exynos_crtc(crtc)->ctx;
	aclk_khz = exynos_devfreq_get_domain_freq(decon->bts.df_disp_idx);
	if (!aclk_khz) {
#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ) || IS_ENABLED(CONFIG_ARM_EXYNOS_ESCA_DEVFREQ)
		dpp_warn(dpp, "exynos_devfreq_get_domain_freq(disp) returned 0!!!\n");
#endif
		return max_aclk_khz;
	}

	return aclk_khz;
}

static void dpp_set_comp_config(const struct dpp_device *dpp,
		struct dpp_params_info *config, const struct drm_framebuffer *fb)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(config->format);

	/* set comptype & blk size */
	if (has_all_bits(DRM_FORMAT_MOD_ARM_AFBC(0), fb->modifier)) {
		config->comp_type = COMP_TYPE_AFBC;
	} else if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0), fb->modifier)) {
		config->comp_type = COMP_TYPE_SAJC;
		config->blk_size = SAJC_BLK_SIZE_GET(fb->modifier);
		config->sajc_sw_mode = SAJC_MOD_SW_MODE_GET(fb->modifier);
	} else if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSY, 0, 0),
				fb->modifier)) {
		config->comp_type = COMP_TYPE_SBWCL;
		config->blk_size = SBWC_BLK_96B;
	} else if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0, 0),
				fb->modifier)) {
		bool is_10bpc = IS_10BPC(fmt_info);
		bool is_256b = SBWC_ALIGN_GET(fb->modifier);
		int lossless_2_7[2][2] = {
			{SBWC_BLK_128B, SBWC_BLK_128B},
			{SBWC_BLK_160B, SBWC_BLK_256B},
		};

		config->comp_type = COMP_TYPE_SBWC;
		config->blk_size = lossless_2_7[is_10bpc][is_256b];
	}
}

static void dpp_set_addr_config(const struct dpp_device *dpp,
		struct dpp_params_info *config, const struct drm_framebuffer *fb)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(config->format);
	bool is_10bpc = IS_10BPC(fmt_info);

	/* set dma base address for each comptype */
	switch (config->comp_type) {
	case COMP_TYPE_NONE:
	case COMP_TYPE_AFBC:
		config->addr[0] = exynos_drm_fb_dma_addr(fb, 0);
		config->addr[1] = exynos_drm_fb_dma_addr(fb, 1);
		break;
	case COMP_TYPE_SAJC:
		/* SAJC header */
		config->addr[0] = exynos_drm_fb_dma_addr(fb, 1);
		/* SAJC payload */
		config->addr[1] = exynos_drm_fb_dma_addr(fb, 0);
		break;
	case COMP_TYPE_SBWC:
		/* Luminance header */
		config->addr[0] = exynos_drm_fb_dma_addr(fb, 0) +
			Y_PL_SIZE_SBWC(config->src.f_w,	config->src.f_h, is_10bpc,
					SBWC_ALIGN_GET(fb->modifier));
		if (SBWC_ALIGN_GET(fb->modifier)) {
			config->stride[0] = SBWC_HEADER_STRIDE(config->src.f_w);
			config->stride[1] = is_10bpc ? SBWC_256_10B_STRIDE(config->src.f_w) :
					SBWC_256_8B_STRIDE(config->src.f_w);
			config->stride[2] = SBWC_HEADER_STRIDE(config->src.f_w);
			config->stride[3] = is_10bpc ? SBWC_256_10B_STRIDE(config->src.f_w) :
					SBWC_256_8B_STRIDE(config->src.f_w);
		}
		/* Luminance payload */
		config->addr[1] = exynos_drm_fb_dma_addr(fb, 0);
		/* Chrominance header */
		config->addr[2] = exynos_drm_fb_dma_addr(fb, 1) +
			UV_PL_SIZE_SBWC(config->src.f_w, config->src.f_h, is_10bpc,
					SBWC_ALIGN_GET(fb->modifier));
		/* Chrominance payload */
		config->addr[3] = exynos_drm_fb_dma_addr(fb, 1);
		break;
	case COMP_TYPE_SBWCL:
		/* Luminance header */
		config->addr[0] = exynos_drm_fb_dma_addr(fb, 0) +
			SBWCL_32_Y_SIZE(fb->width, fb->height);
		/* Luminance payload */
		config->addr[1] = exynos_drm_fb_dma_addr(fb, 0);
		/* Chrominance header */
		config->addr[2] = exynos_drm_fb_dma_addr(fb, 1) +
			SBWCL_32_CBCR_SIZE(fb->width, fb->height);
		/* Chrominance payload */
		config->addr[3] = exynos_drm_fb_dma_addr(fb, 1);
		break;
	default:
		break;
	}
}

static void dpp_set_scale_config(struct dpp_params_info *config,
		const struct exynos_drm_plane_state *state)
{
	unsigned int simplified_rot;
	u64 src_w, src_h, dst_w, dst_h, temp;

	simplified_rot = drm_rotation_simplify(state->base.rotation,
			DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
			DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);
	if (simplified_rot & DRM_MODE_ROTATE_90)
		config->rot |= DPP_ROT;
	if (simplified_rot & DRM_MODE_REFLECT_X)
		config->rot |= DPP_X_FLIP;
	if (simplified_rot & DRM_MODE_REFLECT_Y)
		config->rot |= DPP_Y_FLIP;

	if (config->split) {
		src_w = config->original_size.src_w;
		src_h = config->original_size.src_h;
		dst_w = config->original_size.dst_w;
		dst_h = config->original_size.dst_h;
	} else {
		src_w = config->src.w;
		src_h = config->src.h;
		dst_w = config->dst.w;
		dst_h = config->dst.h;
	}

	if (config->rot & DPP_ROT) {
		temp = src_w;
		src_w = src_h;
		src_h = temp;
	}

	config->h_ratio = (src_w << 20) / dst_w;
	config->v_ratio = (src_h << 20) / dst_h;

	if ((config->h_ratio != (1 << 20)) || (config->v_ratio != (1 << 20)))
		config->is_scale = true;
}

static void dpp_convert_plane_state_to_config(const struct dpp_device *dpp,
				struct dpp_params_info *config,
				const struct exynos_drm_plane_state *state,
				const struct drm_display_mode *mode)
{
	struct drm_framebuffer *fb = state->base.fb;

	dpp_debug(dpp, "mode(%dx%d)\n", mode->hdisplay, mode->vdisplay);

	memset(config, 0, sizeof(*config));

	config->src.x = state->base.src.x1 >> 16;
	config->src.y = state->base.src.y1 >> 16;
	config->src.w = drm_rect_width(&state->base.src) >> 16;
	config->src.h = drm_rect_height(&state->base.src) >> 16;
	config->src.f_w = fb->width;
	config->src.f_h = fb->height;

	config->dst.x = state->base.dst.x1;
	config->dst.y = state->base.dst.y1;
	config->dst.w = drm_rect_width(&state->base.dst);
	config->dst.h = drm_rect_height(&state->base.dst);
	config->dst.f_w = mode->hdisplay;
	config->dst.f_h = mode->vdisplay;

	config->format = fb->format->format;
	config->standard = state->standard;
	config->transfer = state->transfer;
	config->range = state->range;

	if (state->split) {
		struct exynos_split *split = (struct exynos_split *)state->split->data;
		struct exynos_original_size *orig_size = &split->orig_size;
		config->split = split->position;
		config->original_size.src_w = orig_size->src_w;
		config->original_size.src_h = orig_size->src_h;
		config->original_size.dst_w = orig_size->dst_w;
		config->original_size.dst_h = orig_size->dst_h;
		config->has_fraction = split->has_fraction;
		config->need_scaler_pos = state->need_scaler_pos;
	} else {
		config->split = DPP_SPLIT_NONE;
		config->original_size.src_w = config->src.w;
		config->original_size.src_h = config->src.h;
		config->original_size.dst_w = config->dst.w;
		config->original_size.dst_h = config->dst.h;
		config->has_fraction = false;
		config->need_scaler_pos = state->need_scaler_pos;
	}

	dpp_set_comp_config(dpp, config, fb);
	dpp_set_addr_config(dpp, config, fb);
	dpp_set_scale_config(config, state);

	config->rcv_num = dpp_get_aclk(dpp, state);

	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_VOTF(0), fb->modifier)) {
		config->votf_en = 1;
		config->votf_buf_idx = VOTF_BUF_IDX_GET(fb->modifier);
	}
	config->hdr_en = state->hdr_en;

	if (test_bit(DPP_ATTR_RCD, &dpp->attr)) {
		bool is_rcd_size_eq_to_panel_size = true;

		if ((dpp->win_config.dst.w < mode->hdisplay) ||
				(dpp->win_config.dst.h < mode->vdisplay))
			is_rcd_size_eq_to_panel_size = false;

		if ((dpp->rcd_block_mode_en) && (is_rcd_size_eq_to_panel_size)) {
			config->block.x = dpp->rcd_blk.x;
			config->block.y = dpp->rcd_blk.y;
			config->block.w = dpp->rcd_blk.w;
			config->block.h = dpp->rcd_blk.h;
			config->is_block = (config->block.w > 0) && (config->block.h > 0);
		} else {
			config->is_block = false;
		}
	} else {
		if (state->block) {
			struct decon_win_rect *block =
				(struct decon_win_rect *)state->block->data;
			config->block.x = block->x;
			config->block.y = block->y;
			config->block.w = block->w;
			config->block.h = block->h;
			config->is_block = config->block.w > 0 && config->block.h > 0;
		} else {
			config->is_block = false;
		}
	}
}

static void __dpp_enable(struct dpp_device *dpp)
{
	if (dpp->state == DPP_STATE_ON)
		return;

	exynos_dpuf_set_ring_clk(dpp->dpuf, true);
	dpp_reg_init(dpp->id, dpp->attr);

	dpp->state = DPP_STATE_ON;
	enable_irq(dpp->dma_irq);
	if ((dpp->dpp_irq >= 0) && test_bit(DPP_ATTR_DPP, &dpp->attr))
		enable_irq(dpp->dpp_irq);

	dpp_debug(dpp, "enabled\n");
}

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
static int set_protection(struct dpp_device *dpp, uint64_t modifier)
{
	bool protection;
	u32 protection_id;
	int ret = 0;
	static const u32 protection_ids[] = { PROT_L0, PROT_L1, PROT_L2,
					PROT_L3, PROT_L4, PROT_L5,
					PROT_L6, PROT_L7, PROT_L8,
					PROT_L9, PROT_L10, PROT_L11,
					PROT_L12, PROT_L13, PROT_L14,
					PROT_L15 };

	protection = DRM_MODE_GET_PROT(modifier);
	if (dpp->protection == protection)
		return ret;

	if (dpp->id >= ARRAY_SIZE(protection_ids)) {
		dpp_err(dpp, "failed to get protection id(%u)\n", dpp->id);
		return -EINVAL;
	}
	protection_id = protection_ids[dpp->id];

	ret = exynos_hvc(HVC_PROTECTION_SET, 0, protection_id,
			(protection ? HVC_PROTECTION_ENABLE :
			HVC_PROTECTION_DISABLE), 0);
	if (ret) {
		dpp_err(dpp, "failed to %s protection(ch:%u, ret:%d)\n",
				protection ? "enable" : "disable",
				dpp->id, ret);
		return ret;
	}
	dpp->protection = protection;

	dpp_debug(dpp, "ch:%u, en:%d\n", dpp->id, protection);

	return ret;
}
#else
static inline int
set_protection(struct dpp_device *dpp, uint64_t modifier) { return 0; }
#endif

static void __dpp_disable(struct dpp_device *dpp)
{
	if (dpp->state == DPP_STATE_OFF)
		return;

	if ((dpp->dpp_irq >= 0) && test_bit(DPP_ATTR_DPP, &dpp->attr))
		disable_irq(dpp->dpp_irq);
	disable_irq(dpp->dma_irq);

	exynos_dpuf_set_ring_clk(dpp->dpuf, false);
	dpp_reg_deinit(dpp->id, false, dpp->attr);

	exynos_hdr_disable(dpp->hdr);

	set_protection(dpp, 0);
	dpp->state = DPP_STATE_OFF;
	dpp->decon_id = -1;

	dpp_debug(dpp, "disabled\n");
}

static int dpp_disable(struct exynos_drm_plane *exynos_plane)
{
	struct dpp_device *dpp = plane_to_dpp(exynos_plane);

	__dpp_disable(dpp);

	return 0;
}

static void dpp_atomic_print_state(struct drm_printer *p,
		const struct exynos_drm_plane *exynos_plane)
{
	const struct dpp_device *dpp = plane_to_dpp(exynos_plane);

	drm_printf(p, "\tDPP #%d", dpp->id);
	if (dpp->state == DPP_STATE_OFF) {
		drm_printf(p, " (off)\n");
		return;
	}

	drm_printf(p, "\n\t\tport=%d\n", dpp->port);
	drm_printf(p, "\t\tdecon_id=%d\n", dpp->decon_id);
	drm_printf(p, "\t\tprotection=%d\n", dpp->protection);
}

static int dpp_check_scale(const struct dpp_device *dpp,
			struct dpp_params_info *config)
{
	const struct dpp_restrict *res;
	const struct decon_frame *src, *dst;
	u32 src_w, src_h;

	res = &dpp->restriction;
	src = &config->src;
	dst = &config->dst;

	if (config->rot & DPP_ROT) {
		src_w = src->h;
		src_h = src->w;
	} else {
		src_w = src->w;
		src_h = src->h;
	}

	/* If scaling is not requested, it doesn't need to check limitation */
	if ((src_w == dst->w) && (src_h == dst->h))
		return 0;

	/* Scaling is requested. need to check limitation */
	if (!test_bit(DPP_ATTR_SCALE, &dpp->attr)) {
		dpp_err(dpp, "not support scaling\n");
		return -ENOTSUPP;
	}

	if ((config->h_ratio > ((1 << 20) * res->scale_down)) ||
			(config->v_ratio > ((1 << 20) * res->scale_down))) {
		dpp_err(dpp, "not support under 1/%dx scale-down\n",
				res->scale_down);
		return -ENOTSUPP;
	}

	if ((config->h_ratio < ((1 << 20) / res->scale_up)) ||
			(config->v_ratio < ((1 << 20) / res->scale_up))) {
		dpp_err(dpp, "not support over %dx scale-up\n", res->scale_up);
		return -ENOTSUPP;
	}

	return 0;
}

static int dpp_check_size(const struct dpp_device *dpp,
			struct dpp_params_info *config)
{
	const struct decon_frame *src, *dst;
	const struct dpu_fmt *fmt_info;
	const struct dpp_restrict *res;
	u32 mul = 1; /* factor to multiply alignment */
	u32 src_w_max, src_h_max;

	fmt_info = dpu_find_fmt_info(config->format);

	if (IS_YUV(fmt_info))
		mul = 2;

	res = &dpp->restriction;
	src = &config->src;
	dst = &config->dst;

	if (config->rot & DPP_ROT) {
		src_w_max = res->src_w_rot_max;
		src_h_max = res->src_h_rot_max;
	} else {
		src_w_max = res->src_w.max;
		src_h_max = res->src_h.max;
	}

	/* check alignment */
	if (!IS_ALIGNED(src->x, res->src_x_align * mul) ||
			!IS_ALIGNED(src->y, res->src_y_align * mul) ||
			!IS_ALIGNED(src->w, res->src_w.align * mul) ||
			!IS_ALIGNED(src->h, res->src_h.align * mul) ||
			!IS_ALIGNED(src->f_w, res->src_f_w.align * mul) ||
			!IS_ALIGNED(src->f_h, res->src_f_h.align * mul)) {
		dpp_err(dpp, "not supported source alignment\n");
		return -ENOTSUPP;
	}

	if (!IS_ALIGNED(dst->x, res->dst_x_align) ||
			!IS_ALIGNED(dst->y, res->dst_y_align) ||
			!IS_ALIGNED(dst->w, res->dst_w.align) ||
			!IS_ALIGNED(dst->h, res->dst_h.align) ||
			!IS_ALIGNED(dst->f_w, res->dst_f_w.align) ||
			!IS_ALIGNED(dst->f_h, res->dst_f_h.align)) {
		dpp_err(dpp, "not supported destination alignment\n");
		return -ENOTSUPP;
	}

	/* check range */
	if (!IN_RANGE(src->w, res->src_w.min * mul, src_w_max) ||
			!IN_RANGE(src->h, res->src_h.min * mul, src_h_max) ||
			!IN_RANGE(src->f_w, res->src_f_w.min * mul,
						res->src_f_w.max) ||
			!IN_RANGE(src->f_h, res->src_f_h.min,
						res->src_f_h.max)) {
		dpp_err(dpp, "not supported source size range\n");
		return -ENOTSUPP;
	}

	if (!IN_RANGE(dst->w, res->dst_w.min, res->dst_w.max) ||
			!IN_RANGE(dst->h, res->dst_h.min, res->dst_h.max) ||
			!IN_RANGE(dst->f_w, res->dst_f_w.min,
						res->dst_f_w.max) ||
			!IN_RANGE(dst->f_h, res->dst_f_h.min,
						res->dst_f_h.max)) {
		dpp_err(dpp, "not supported destination size range\n");
		return -ENOTSUPP;
	}

	return 0;
}

static void set_sfr_mode(u32 dpp_id, struct dpp_params_info *config)
{
	config->sfr_dma_en = exynos_drm_sfr_dma_is_enabled(dpp_id);
}

static int dpp_check(struct exynos_drm_plane *exynos_plane,
		     struct drm_atomic_state *state)
{
	const struct drm_plane_state *new_plane_state =
			drm_atomic_get_new_plane_state(state, &exynos_plane->base);
	struct exynos_drm_plane_state *new_exynos_plane_state =
				to_exynos_plane_state(new_plane_state);
	const struct dpp_device *dpp = plane_to_dpp(exynos_plane);
	struct dpp_params_info config;
	const struct drm_crtc_state *new_crtc_state =
		drm_atomic_get_new_crtc_state(state, new_plane_state->crtc);
	const struct drm_display_mode *mode = &new_crtc_state->adjusted_mode;

	dpp_debug(dpp, "+\n");

	memset(&config, 0, sizeof(struct dpp_params_info));

	dpp_convert_plane_state_to_config(dpp, &config, new_exynos_plane_state, mode);

	if (dpp_check_scale(dpp, &config))
		goto err;

	if (dpp_check_size(dpp, &config))
		goto err;

	if (!test_bit(DPP_ATTR_SBWC, &dpp->attr) &&
			(config.comp_type == COMP_TYPE_SBWC)) {
		dpp_err(dpp, "SBWC is not supported\n");
		goto err;
	}

	if (__dpp_check(dpp->id, &config, dpp->attr))
		goto err;

	exynos_hdr_prepare(dpp->hdr, new_exynos_plane_state);

	dpp_debug(dpp, "-\n");

	return 0;

err:
	dpp_err(dpp, "src[%d %d %d %d %d %d] dst[%d %d %d %d %d %d] fmt[%d]\n",
			config.src.x, config.src.y, config.src.w, config.src.h,
			config.src.f_w, config.src.f_h,
			config.dst.x, config.dst.y, config.dst.w, config.dst.h,
			config.dst.f_w, config.dst.f_h,
			config.format);
	dpp_err(dpp, "rot[0x%x] comp_type[%d]\n", config.rot, config.comp_type);

	return -ENOTSUPP;
}

static int dpp_update(struct exynos_drm_plane *exynos_plane,
		      const struct exynos_drm_plane_state *state)
{
	struct dpp_device *dpp = plane_to_dpp(exynos_plane);
	struct dpp_params_info *config = &dpp->win_config;
	const struct drm_plane_state *plane_state = &state->base;
	const struct drm_crtc_state *crtc_state = plane_state->crtc->state;
	const struct drm_display_mode *mode = &crtc_state->adjusted_mode;
	const struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	dpp_debug(dpp, "+\n");

	__dpp_enable(dpp);

	dpp_convert_plane_state_to_config(dpp, config, state, mode);
	config->in_bpc = exynos_crtc_state->in_bpc >= 10 ? DPP_BPC_10 : DPP_BPC_8;

	set_protection(dpp, plane_state->fb->modifier);

	if (!test_bit(DPP_ATTR_RCD, &dpp->attr))
		set_sfr_mode(dpp->id, config);

	dpp_reg_configure_params(dpp->id, config, dpp->attr);

	exynos_hdr_update(dpp->hdr, state);

	dpp_debug(dpp, "-\n");

	return 0;
}

struct exynos_drm_plane_ops dpp_plane_ops = {
	.atomic_check = dpp_check,
	.atomic_update = dpp_update,
	.atomic_disable = dpp_disable,
	.atomic_print_state = dpp_atomic_print_state,
};

static int dpp_bind(struct device *dev, struct device *master, void *data)
{
	struct dpp_device *dpp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct exynos_drm_plane_config plane_config;
	int id = dpp->id;
	uint64_t *modifiers;

	dpp_debug(dpp, "+\n");

	memset(&plane_config, 0, sizeof(plane_config));

	plane_config.ctx = dpp;
	plane_config.pixel_formats = dpp->pixel_formats;
	plane_config.num_pixel_formats = dpp->num_pixel_formats;
	plane_config.zpos = id;
	plane_config.max_zpos = MAX_WIN_PER_DECON;
	plane_config.type = get_decon_drvdata(id) ? DRM_PLANE_TYPE_PRIMARY :
		DRM_PLANE_TYPE_OVERLAY;
	plane_config.res = &dpp->restriction;

	if (test_bit(DPP_ATTR_ROT, &dpp->attr))
		plane_config.capabilities |= EXYNOS_DRM_PLANE_CAP_ROT;
	if (test_bit(DPP_ATTR_FLIP, &dpp->attr))
		plane_config.capabilities |= EXYNOS_DRM_PLANE_CAP_FLIP;
	if (test_bit(DPP_ATTR_CSC, &dpp->attr))
		plane_config.capabilities |= EXYNOS_DRM_PLANE_CAP_CSC;
	if (test_bit(DPP_ATTR_HDR, &dpp->attr) ||
			test_bit(DPP_ATTR_WCG, &dpp->attr) ||
			test_bit(DPP_ATTR_HDR10_PLUS, &dpp->attr))
		plane_config.capabilities |= EXYNOS_DRM_PLANE_CAP_HDR;
	if (test_bit(DPP_ATTR_RCD, &dpp->attr))
		plane_config.capabilities |= EXYNOS_DRM_PLANE_CAP_RCD;

	modifiers = dpp_get_modifiers(dpp);
	if (!modifiers)
		return -ENOMEM;

	plane_config.modifiers = modifiers;
	dpp->plane = exynos_drm_plane_create(drm_dev, id, &plane_config,
					     &dpp_plane_ops);
	if (IS_ERR(dpp->plane))
		return PTR_ERR(dpp->plane);

	dpp_debug(dpp, "-\n");

	return 0;
}


static void dpp_unbind(struct device *dev, struct device *master, void *data)
{
	struct dpp_device *dpp = dev_get_drvdata(dev);

	if (dpp->state == DPP_STATE_ON)
		__dpp_disable(dpp);
}

static const struct component_ops exynos_dpp_component_ops = {
	.bind	= dpp_bind,
	.unbind	= dpp_unbind,
};

static void dpp_print_restriction(struct device *dev, struct dpp_restrict *res)
{
	dev_info(dev, "src_f_w[%d %d %d] src_f_h[%d %d %d]\n",
			DPP_SIZE_ARG(res->src_f_w), DPP_SIZE_ARG(res->src_f_h));
	dev_info(dev, "src_w[%d %d %d] src_h[%d %d %d] src_x_y_align[%d %d]\n",
			DPP_SIZE_ARG(res->src_w), DPP_SIZE_ARG(res->src_h),
			res->src_x_align, res->src_y_align);

	dev_info(dev, "dst_f_w[%d %d %d] dst_f_h[%d %d %d]\n",
			DPP_SIZE_ARG(res->dst_f_w), DPP_SIZE_ARG(res->dst_f_h));
	dev_info(dev, "dst_w[%d %d %d] dst_h[%d %d %d] dst_x_y_align[%d %d]\n",
			DPP_SIZE_ARG(res->dst_w), DPP_SIZE_ARG(res->dst_h),
			res->dst_x_align, res->dst_y_align);

	dev_info(dev, "blk_w[%d %d %d] blk_h[%d %d %d] blk_x_y_align[%d %d]\n",
			DPP_SIZE_ARG(res->blk_w), DPP_SIZE_ARG(res->blk_h),
			res->blk_x_align, res->blk_y_align);

	dev_info(dev, "src_h_rot_max[%d]\n", res->src_h_rot_max);

	dev_info(dev, "max scale up(%dx), down(1/%dx) ratio\n",
			res->scale_up, res->scale_down);
}

int exynos_parse_formats(struct device *dev, uint32_t **pixel_formats)
{
	int fmt_cnt, ret = 0;
	struct device_node *np = dev->of_node, *fmt_np = NULL;

	fmt_np = of_parse_phandle(np, "dpp,formats", 0);
	if (!fmt_np) {
		dev_err(dev, "failed to find format phandle\n");
		ret = -EINVAL;
		goto fail;
	}

	fmt_cnt = of_property_count_u32_elems(fmt_np, "formats");
	*pixel_formats = devm_kzalloc(dev, fmt_cnt * sizeof(u32), GFP_KERNEL);
	if (!(*pixel_formats)) {
		dev_err(dev, "failed to alloc format list\n");
		ret = -EINVAL;
		goto fail;
	}

	ret = of_property_read_u32_array(fmt_np, "formats", *pixel_formats, fmt_cnt);
	if (ret) {
		dev_err(dev, "failed to parse format list\n");
		ret = -EINVAL;
		goto fail;
	}

fail:
	of_node_put(fmt_np);

	return ret ?: fmt_cnt;
}

#define RANGE_NUM (3)
static inline int get_dpp_restriction(const struct device_node *np,
					     const char *propname,
					     struct dpu_u32_range *range)
{
	u32 values[RANGE_NUM];
	int ret = of_property_read_u32_array(np, propname, values, RANGE_NUM);

	range->min = values[0];
	range->max = values[1];
	range->align = values[2];

	return ret;
}

static int exynos_parse_restrictions(struct device *dev, struct dpp_restrict *res)
{
	int ret = 0;
	struct device_node *np = dev->of_node, *res_np = NULL;

	/* If the structure changes, update the version. */
	ret = of_property_read_u32(np, "dpp,id", &res->id);
	ret |= of_property_read_u32(np, "port", &res->port);
	ret |= of_property_read_u32(np, "attr", (u32 *)&res->attr);
	if (of_property_read_u32(np, "dpuf,id", &res->dpuf_id))
		res->dpuf_id = 0;
	if (ret)
		return ret;

	res_np = of_parse_phandle(np, "dpp,restriction", 0);
	if (!res_np) {
		dev_err(dev, "failed to find restriction phandle\n");
		ret = -EINVAL;
		goto fail;
	}

	ret |= get_dpp_restriction(res_np, "src_f_w", &res->src_f_w);
	ret |= get_dpp_restriction(res_np, "src_f_h", &res->src_f_h);
	ret |= get_dpp_restriction(res_np, "src_w", &res->src_w);
	ret |= get_dpp_restriction(res_np, "src_h", &res->src_h);
	ret |= get_dpp_restriction(res_np, "dst_f_w", &res->dst_f_w);
	ret |= get_dpp_restriction(res_np, "dst_f_h", &res->dst_f_h);
	ret |= get_dpp_restriction(res_np, "dst_w", &res->dst_w);
	ret |= get_dpp_restriction(res_np, "dst_h", &res->dst_h);
	ret |= get_dpp_restriction(res_np, "blk_w", &res->blk_w);
	ret |= get_dpp_restriction(res_np, "blk_h", &res->blk_h);
	ret |= get_dpp_restriction(res_np, "src_rgb_comp_f_w", &res->src_rgb_comp_f_w);
	ret |= get_dpp_restriction(res_np, "src_rgb_comp_f_h", &res->src_rgb_comp_f_h);
	ret |= get_dpp_restriction(res_np, "src_rgb_comp_w", &res->src_rgb_comp_w);
	ret |= get_dpp_restriction(res_np, "src_rgb_comp_h", &res->src_rgb_comp_h);

	ret |= of_property_read_u32(res_np, "src_x_align", (u32 *)&res->src_x_align);
	ret |= of_property_read_u32(res_np, "src_y_align", (u32 *)&res->src_y_align);
	ret |= of_property_read_u32(res_np, "dst_x_align", (u32 *)&res->dst_x_align);
	ret |= of_property_read_u32(res_np, "dst_y_align", (u32 *)&res->dst_y_align);
	ret |= of_property_read_u32(res_np, "blk_x_align", (u32 *)&res->blk_x_align);
	ret |= of_property_read_u32(res_np, "blk_y_align", (u32 *)&res->blk_y_align);
	ret |= of_property_read_u32(res_np, "src_w_rot_max", (u32 *)&res->src_w_rot_max);
	ret |= of_property_read_u32(res_np, "src_h_rot_max", (u32 *)&res->src_h_rot_max);

	ret |= of_property_read_u32(np, "scale_down", (u32 *)&res->scale_down);
	ret |= of_property_read_u32(np, "scale_up", (u32 *)&res->scale_up);
	if (ret)
		goto fail;

	dpp_print_restriction(dev, res);

fail:
	of_node_put(res_np);

	return ret;
}

static struct dpuf*
exynos_drm_parse_dpuf(struct dpp_device *dpp, struct device_node *np)
{
	struct dpp_dpuf_resource *dpuf_rsc;
	struct dpuf *dpuf;
	int dpuf_id;
	int i;

	if (of_property_read_u32(np, "dpuf,id", &dpuf_id))
		dpuf_id = 0;

	/* already dpuf block info parsed */
	if (dpuf_blkdata[dpuf_id])
		return dpuf_blkdata[dpuf_id];

	dpuf = devm_kzalloc(dpp->dev, sizeof(*dpuf), GFP_KERNEL);
	if (!dpuf)
		return ERR_PTR(-ENOMEM);

	dpuf->id = dpuf_id;
	dpuf_blkdata[dpuf_id] = dpuf;

	i = of_property_match_string(np, "reg-names", "votf");
	if (i >= 0 && !dpuf->votf) {
		struct platform_device *pdev = to_platform_device(dpp->dev);
		struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		struct dpuf_votf *votf;

		votf = devm_kzalloc(dpp->dev, sizeof(*votf), GFP_KERNEL);
		if (!votf)
			return ERR_PTR(-ENOMEM);

		votf->base_addr = (u32)res->start;
		votf->enabled = true;
		spin_lock_init(&votf->slock);
		dpuf->votf = votf;
	}

	if (dpp->id != 0)
		return dpuf;

	dpuf_rsc = &dpuf->resource;
	if (of_property_read_bool(np, "dpp,rsc_check"))  {
		dpuf_rsc->check = 1;
		of_property_read_u32(np, "rsc_sajc", (u32 *)&dpuf_rsc->sajc);
		of_property_read_u32(np, "rsc_sbwc", (u32 *)&dpuf_rsc->sbwc);
		of_property_read_u32(np, "rsc_rot", (u32 *)&dpuf_rsc->rot);
		of_property_read_u32(np, "rsc_scl", (u32 *)&dpuf_rsc->scl);
		of_property_read_u32(np, "rsc_itp_csc", (u32 *)&dpuf_rsc->itp_csc);
		of_property_read_u32(np, "rsc_sramc", (u32 *)&dpuf_rsc->sramc);
		of_property_read_u32(np, "rsc_sram_w", (u32 *)&dpuf_rsc->sram_w);
		of_property_read_u32(np, "rsc_sram", (u32 *)&dpuf_rsc->sram);
	} else
		dpuf_rsc->check = 0;

	return dpuf;
}

static int exynos_dpp_parse_dt(struct dpp_device *dpp, struct device_node *np)
{
	int ret = 0;

	ret = of_property_read_u32(np, "dpp,id", &dpp->id);
	ret |= of_property_read_u32(np, "attr", (u32 *)&dpp->attr);
	ret |= of_property_read_u32(np, "port", &dpp->port);
	if (ret < 0)
		return ret;

	dpp->sfr_dma_en = of_property_read_bool(np, "dpp,enable-dma-write-mode");

	dpp->dpuf = exynos_drm_parse_dpuf(dpp, np);
	if (IS_ERR(dpp->dpuf)) {
		ret = PTR_ERR(dpp->dpuf);
		return ret;
	}

	dpp->num_pixel_formats = exynos_parse_formats(dpp->dev, &dpp->pixel_formats);
	if (dpp->num_pixel_formats <= 0) {
		dpp_err(dpp, "failed to parse format list\n");
		return dpp->num_pixel_formats;
	}

	ret = exynos_parse_restrictions(dpp->dev,
			(struct dpp_restrict*)&dpp->restriction);
	if (ret) {
		dpp_err(dpp, "failed to parse restriction\n");
		return ret;
	}

	if (test_bit(DPP_ATTR_RCD, &dpp->attr)) {
		dpp->rcd_block_mode_en = of_property_read_bool(np, "rcd_block_mode_en");

		if (dpp->rcd_block_mode_en) {
			ret = of_property_read_u32(np, "rcd_block_mode_x", &dpp->rcd_blk.x);
			ret |= of_property_read_u32(np, "rcd_block_mode_y", &dpp->rcd_blk.y);
			ret |= of_property_read_u32(np, "rcd_block_mode_w", &dpp->rcd_blk.w);
			ret |= of_property_read_u32(np, "rcd_block_mode_h", &dpp->rcd_blk.h);
			if(ret < 0)
				dpp_err(dpp, "Invalid RCD Block mode configuration !! \n");
		} else {
			dpp_info(dpp, "Enable the RCD Block mode !! \n");
		}

		dpp_info(dpp, "rcd_block_mode_en(%d), rcd_block_mode_x(%d),",
				dpp->rcd_block_mode_en, dpp->rcd_blk.x);
		dpp_info(dpp, "rcd_block_mode_y(%d), rcd_block_mode_w(%d),",
				dpp->rcd_blk.y, dpp->rcd_blk.w);
		dpp_info(dpp, "rcd_block_mode_h(%d) \n", dpp->rcd_blk.h);
	}

	dpp_info(dpp, "attr(0x%lx), port(%d), dma write mode(%d)\n", dpp->attr,
			dpp->port, dpp->sfr_dma_en);

	return ret;
}

static irqreturn_t dpp_irq_handler(int irq, void *priv)
{
	struct dpp_device *dpp = priv;
	struct decon_device *decon = get_decon_drvdata(dpp->decon_id);
	u32 dpp_irq = 0;

	spin_lock(&dpp->slock);
	if (dpp->state == DPP_STATE_OFF)
		goto irq_end;

	dpp_irq = dpp_reg_get_irq_and_clear(dpp->id);

	if (dpp_irq & DPP_CFG_ERROR_IRQ)
		DPU_EVENT_LOG("DPP_CFG_ERR", decon->crtc, EVENT_FLAG_ERROR, NULL);

irq_end:
	spin_unlock(&dpp->slock);
	return IRQ_HANDLED;
}

static irqreturn_t dma_irq_handler(int irq, void *priv)
{
	struct dpp_device *dpp = priv;
	struct exynos_drm_plane *exynos_plane = dpp->plane;
	struct decon_device *decon = get_decon_drvdata(dpp->decon_id);
	u32 irqs;
	const char *str_comp;

	spin_lock(&dpp->dma_slock);
	if (dpp->state == DPP_STATE_OFF)
		goto irq_end;

	/* IDMA case */
	irqs = idma_reg_get_irq_and_clear(dpp->id);

	dpp_debug(dpp, "irq_sts_reg = %x\n", irqs);

	if (irqs & IDMA_RECOVERY_START_IRQ) {
		exynos_plane->recovery_cnt++;
		str_comp = get_comp_src_name(exynos_plane->comp_src);
		DPU_EVENT_LOG("DMA_RECOVERY", decon->crtc, EVENT_FLAG_ERROR,
				"ID:%d SRC:%s COUNT:%d",
				dpp->id, str_comp, ++exynos_plane->recovery_cnt);
		dpp_info(dpp, "recovery start(0x%x) cnt(%d) src(%s)\n", irqs,
				exynos_plane->recovery_cnt, str_comp);
	}

	/*
	 * TODO: Normally, DMA framedone occurs before DPP framedone.
	 * But DMA framedone can occur in case of AFBC crop mode
	 */
	if (irqs & IDMA_STATUS_FRAMEDONE_IRQ) {
		idma_reg_set_votf_disable(dpp->id);
		DPU_EVENT_LOG("DPP_FRAMEDONE", decon->crtc, 0, "ID:%d", dpp->id);
	}
	if ((irqs & IDMA_READ_SLAVE_ERROR) || (irqs & IDMA_STATUS_DEADLOCK_IRQ)) {
		int fid = (dpp->id >= DPP_PER_DPUF) ? 1 : 0;
		struct dpp_device *dpp_f = get_dpp_drvdata(fid);

		DPU_EVENT_LOG("DMA_ERR", decon->crtc, EVENT_FLAG_ERROR,	NULL);
		dpp_err(dpp, "error irq occur(0x%x)\n", irqs);
		__dpp_common_dump(fid, &dpp_f->regs);
		__dpp_dump(dpp->id, &dpp->regs);
		goto irq_end;
	}

	if (dpp->dpp_irq < 0) {
		if (irqs & DPP_CFG_ERROR_IRQ)
			DPU_EVENT_LOG("DPP_CFG_ERR", decon->crtc, EVENT_FLAG_ERROR, NULL);
	}

irq_end:

	spin_unlock(&dpp->dma_slock);
	return IRQ_HANDLED;
}

static int dpp_remap_by_name(struct dpp_device *dpp, void __iomem **base,
		const char *reg_name, enum dpp_regs_type type)
{
	int i, ret = 0;
	struct resource res;
	struct device_node *np = dpp->dev->of_node;

	i = of_property_match_string(np, "reg-names", reg_name);
	if (i < 0) {
		dpp_info(dpp, "failed to find %s SFR region\n", reg_name);
		return ret;
	}

	ret = of_address_to_resource(np, i, &res);
	if (ret)
		return ret;

	*base = devm_ioremap(dpp->dev, res.start, resource_size(&res));
	if (!(*base)) {
		dpp_err(dpp, "failed to remap %s SFR region\n", reg_name);
		return -EINVAL;
	}
	dpp_regs_desc_init(*base, reg_name, type, dpp->id);

	return 0;
}

static int dpp_remap_regs(struct dpp_device *dpp)
{
	struct device *dev = dpp->dev;
	struct platform_device *pdev;
	struct device_node *np = dev->of_node;
	int i;

	pdev = to_platform_device(dev);

	if (dpp_remap_by_name(dpp, &dpp->regs.dma_base_regs, "dma", REGS_DMA))
		goto err;

	if (test_bit(DPP_ATTR_DPP, &dpp->attr) && dpp_remap_by_name(dpp,
				&dpp->regs.dpp_base_regs, "dpp", REGS_DPP))
		goto err;

	if (test_bit(DPP_ATTR_SRAMC, &dpp->attr) && dpp_remap_by_name(dpp,
				&dpp->regs.sramc_base_regs, "sramc", REGS_SRAMC))
		goto err;

	if (dpp_remap_by_name(dpp, &dpp->regs.votf_base_regs, "votf", REGS_VOTF))
		goto err;

	if (dpp_remap_by_name(dpp, &dpp->regs.scl_coef_base_regs, "scl_coef",
				REGS_SCL_COEF))
		goto err;

	if (test_bit(DPP_ATTR_HDR_COMM, &dpp->attr) && dpp_remap_by_name(dpp,
			&dpp->regs.hdr_comm_base_regs, "hdr_comm", REGS_HDR_COMM))
		goto err;

	i = of_property_match_string(np, "reg-names", "dpp_debug");
	if (i >= 0) {
		dpp->regs.dpp_debug_base_regs = of_iomap(np, i);
		if (!dpp->regs.dpp_debug_base_regs) {
			dpp_err(dpp, "failed to remap dpp_debug SFR region\n");
			goto err;
		}
	}

	dpp_remap_by_name(dpp, &dpp->regs.sfr_dma_base_regs, "sfr_dma", REGS_SFR_DMA);

	if (dpp->regs.sfr_dma_base_regs) {
		struct dpp_device *dpp0 = get_dpp_drvdata(0);

		if (exynos_drm_sfr_dma_initialize(dpp->dev, dpp->regs.sfr_dma_base_regs,
					sizeof(u64) * 64, dpp0->sfr_dma_en) < 0)
			dpp_err(dpp, "failed to init dma-buf\n");
	}

	return 0;

err:
	return -EINVAL;
}

static int dpp_register_irqs(struct dpp_device *dpp)
{
	struct device *dev = dpp->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	int ret = 0;

	pdev = to_platform_device(dev);

	dpp->dma_irq = of_irq_get_byname(np, "dma");
	dpp_debug(dpp, "dma irq no = %d\n", dpp->dma_irq);
	ret = devm_request_irq(dev, dpp->dma_irq, dma_irq_handler, 0,
			pdev->name, dpp);
	if (ret) {
		dpp_err(dpp, "failed to install DPU DMA irq\n");
		return ret;
	}
	disable_irq(dpp->dma_irq);

	if (test_bit(DPP_ATTR_DPP, &dpp->attr)) {
		dpp->dpp_irq = of_irq_get_byname(np, "dpp");
		if ((dpp->dpp_irq) >= 0) {
			dpp_debug(dpp, "dpp irq no = %d\n", dpp->dpp_irq);
			ret = devm_request_irq(dev, dpp->dpp_irq, dpp_irq_handler, 0,
					pdev->name, dpp);
			if (ret) {
				dpp_err(dpp, "failed to install DPP irq\n");
				return ret;
			}
			disable_irq(dpp->dpp_irq);
		}
	}

	return ret;
}

static int dpp_init_resources(struct dpp_device *dpp)
{
	int ret = 0;

	ret = dpp_remap_regs(dpp);
	if (ret)
		goto err;

	ret = dpp_register_irqs(dpp);
	if (ret)
		goto err;

	if (dpp->id == 0) {
		pm_runtime_get_sync(dpp->dev);
		dpp_reg_get_version(dpp->id, &dpp->dpp_ver);
		idma_reg_get_version(dpp->id, &dpp->dma_ver);
		pm_runtime_put_sync(dpp->dev);
	} else {
		struct dpp_device *dpp0 = get_dpp_drvdata(0);

		dpp->dpp_ver = dpp0->dpp_ver;
		dpp->dma_ver = dpp0->dma_ver;
	}

err:
	return ret;
}

static int dpp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct dpp_device *dpp;

	dpp = devm_kzalloc(dev, sizeof(struct dpp_device), GFP_KERNEL);
	if (!dpp)
		return -ENOMEM;

	dpp->dev = dev;

	ret = exynos_dpp_parse_dt(dpp, dev->of_node);
	if (ret)
		goto fail;
	dpp_drvdata[dpp->id] = dpp;

	spin_lock_init(&dpp->slock);
	spin_lock_init(&dpp->dma_slock);

	dpp->state = DPP_STATE_OFF;

	pm_runtime_enable(dpp->dev);

	ret = dpp_init_resources(dpp);
	if (ret)
		goto fail;

	dpp->hdr = exynos_hdr_register(dpp);

	/* dpp is not connected decon now */
	dpp->decon_id = -1;

	platform_set_drvdata(pdev, dpp);

	dpp_info(dpp, "dpp_ver(%02d.%02d.00) dma_ver(%02d.%02d.00) successfully probed",
			dpp->dpp_ver.major, dpp->dpp_ver.minor,
			dpp->dma_ver.major, dpp->dma_ver.minor);

	return component_add(dev, &exynos_dpp_component_ops);

fail:
	dpp_err(dpp, "probe failed");

	return ret;
}

bool dpp_condition_check(const struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct dpp_device *dpp;
	int id;

	exynos_crtc = to_exynos_crtc(crtc);
	if (!exynos_crtc)
		return false;

	if (exynos_crtc)
		hibernation_block_exit(exynos_crtc->hibernation);

	for (id = 0; id < 16; ++id) {
		dpp = get_dpp_drvdata((u32)id);
		if (dpp->state == DPP_STATE_OFF)
			continue;
		idma_reg_check_outstanding_count(id);
	}

	if (exynos_crtc)
		hibernation_unblock(exynos_crtc->hibernation);

	return false;
}

static int dpp_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	component_del(&pdev->dev, &exynos_dpp_component_ops);

	return 0;
}

struct platform_driver dpp_driver = {
	.probe = dpp_probe,
	.remove = dpp_remove,
	.driver = {
		   .name = "exynos-drmdpp",
		   .owner = THIS_MODULE,
		   .of_match_table = dpp_of_match,
	},
};
EXPORT_SYMBOL(dpp_driver);

MODULE_AUTHOR("Seong-gyu Park <seongyu.park@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC Display Pre Processor");
MODULE_LICENSE("GPL v2");
