// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_fb.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_blend.h>
#include <linux/dma-buf.h>

#include <dpu_trace.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_fb.h>
#include <exynos_drm_format.h>
#include <exynos_drm_gem.h>
#include <exynos_drm_modifier.h>

#define MAX_FB_BUFFER (DRM_FORMAT_MAX_PLANES)

static const struct drm_framebuffer_funcs exynos_drm_fb_funcs = {
	.destroy	= drm_gem_fb_destroy,
	.create_handle	= drm_gem_fb_create_handle,
};

static void exynos_drm_fb_set_offsets(struct drm_framebuffer *fb)
{
	const struct dpu_fmt *fmt_info;
	const struct drm_format_info *info = fb->format;
	bool is_10bpc;

	if (!info->is_yuv || (fb->obj[0] != fb->obj[1]))
		return;

	if (fb->offsets[0] || fb->offsets[1])
		return;

	fmt_info = dpu_find_fmt_info(fb->format->format);
	is_10bpc = IS_10BPC(fmt_info);

	/* If single FD case with yuv format, the offsets will be calculated in here.
	 * The NV12, NV21 and P010 are only supported YUV formats.
	 */
	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0, 0),
				fb->modifier) ||
			has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_NONE, 0, 0),
				fb->modifier)) {
		fb->offsets[1] = Y_SIZE_SBWC(fb->width, fb->height,
				is_10bpc, SBWC_ALIGN_GET(fb->modifier));
	} else {
		fb->offsets[1] = Y_SIZE(fb->width, fb->height, is_10bpc);
	}
}

static struct drm_framebuffer *
exynos_drm_framebuffer_init(struct drm_device *dev,
			    const struct drm_mode_fb_cmd2 *mode_cmd,
			    struct drm_gem_object **obj,
			    int count)
{
	struct drm_framebuffer *fb;
	int i;
	int ret;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++)
		fb->obj[i] = obj[i];

	drm_helper_mode_fill_fb_struct(dev, fb, mode_cmd);

	exynos_drm_fb_set_offsets(fb);

	ret = drm_framebuffer_init(dev, fb, &exynos_drm_fb_funcs);
	if (ret < 0) {
		DRM_ERROR("failed to initialize framebuffer\n");
		kfree(fb);
		return ERR_PTR(ret);
	}

	return fb;
}

static size_t get_plane_size(const struct drm_mode_fb_cmd2 *mode_cmd, u32 idx,
		const struct drm_format_info *info)
{
	u32 height;
	size_t size = 0;
	bool is_10bpc;

	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSY, 0, 0),
				mode_cmd->modifier[idx])) {
		int mul;
		int blk_32x;

		is_10bpc = IS_10BPC(dpu_find_fmt_info(mode_cmd->pixel_format));
		mul = is_10bpc ? 20 : 25;
		blk_32x = SBWCL_MOD_RATIO_GET(mode_cmd->modifier[idx]);

		/*
		 * mapping size[0] : luminance PL
		 * mapping size[1] : chrominance PL
		 */
		if (idx == 0)
			size = Y_SIZE_SBWCL(mode_cmd->width, mode_cmd->height,
					mul * blk_32x, is_10bpc);
		else if (idx == 1)
			size = UV_SIZE_SBWCL(mode_cmd->width, mode_cmd->height,
					mul * blk_32x, is_10bpc);
	} else if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0, 0),
				mode_cmd->modifier[idx])) {
		is_10bpc = IS_10BPC(dpu_find_fmt_info(mode_cmd->pixel_format));

		/*
		 * mapping size[0] : luminance PL/HD
		 * mapping size[1] : chrominance PL/HD
		 */
		if (idx == 0)
			size = Y_SIZE_SBWC(mode_cmd->width, mode_cmd->height,
				is_10bpc, SBWC_ALIGN_GET(mode_cmd->modifier[idx]));
		else if (idx == 1)
			size = UV_SIZE_SBWC(mode_cmd->width, mode_cmd->height,
				is_10bpc, SBWC_ALIGN_GET(mode_cmd->modifier[idx]));
	} else if (mode_cmd->modifier[idx] & SAJC_IDENTIFIER) {
		if (idx == 0)
			size = mode_cmd->height * mode_cmd->pitches[idx];
		else if (idx == 1)
			/* TODO: use macro */
			size = mode_cmd->pitches[idx] * 2 ;
	} else {
		height = (idx == 0) ? mode_cmd->height :
			DIV_ROUND_UP(mode_cmd->height, info->vsub);
		size = height * mode_cmd->pitches[idx];
	}

	return size;
}

struct drm_framebuffer *exynos_user_fb_create(struct drm_device *dev,
					      struct drm_file *file_priv,
					      const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct drm_format_info *info = drm_get_format_info(dev, mode_cmd);
	struct drm_gem_object *obj[MAX_FB_BUFFER] = { 0 };
	struct drm_framebuffer *fb = ERR_PTR(-ENOMEM);
	size_t size;
	int i;
	int ret = 0;

	DRM_DEBUG("%s +\n", __func__);

	DPU_ATRACE_BEGIN(__func__);
	if (unlikely(info->num_planes > MAX_FB_BUFFER)) {
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < info->num_planes; i++) {
		if (mode_cmd->modifier[i] ==
				DRM_FORMAT_MOD_SAMSUNG_COLORMAP) {
			struct exynos_drm_gem *exynos_gem;

			exynos_gem = exynos_drm_gem_alloc(dev, 0,
						EXYNOS_DRM_GEM_FLAG_COLORMAP);
			if (IS_ERR(exynos_gem)) {
				DRM_ERROR("failed to create colormap gem\n");
				ret = PTR_ERR(exynos_gem);
				goto err_gem;
			}

			obj[i] = &exynos_gem->base;
			continue;
		}

		obj[i] = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj[i]) {
			DRM_ERROR("failed to lookup gem object\n");
			ret = -ENOENT;
			goto err_gem;
		}

		size = get_plane_size(mode_cmd, i, info);
		if (!size || mode_cmd->offsets[i] + size > obj[i]->size) {
			DRM_ERROR("offsets[%d](%d) + size(%zu) > obj_size[%d](%zu)\n",
				i, mode_cmd->offsets[i], size, i, obj[i]->size);
			DRM_ERROR("mod(%llx)\n", mode_cmd->modifier[i]);
			i++;
			ret = -EINVAL;
			goto err_gem;
		}
		DRM_DEBUG("[plane:%d] handle(0x%x), offset(%d) pitches(%d),\
				size(%lu)\n", i, mode_cmd->handles[i],
				mode_cmd->offsets[i], mode_cmd->pitches[i], size);
	}
	DRM_DEBUG("width(%d), height(%d), mod(0x%llx) format(%p4cc) \n",
			mode_cmd->width, mode_cmd->height, mode_cmd->modifier[0],
			&mode_cmd->pixel_format);

	fb = exynos_drm_framebuffer_init(dev, mode_cmd, obj, i);
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		goto err_gem;
	}

err_gem:
	if (ret) {
		while (i--)
			drm_gem_object_put(obj[i]);
	}
err:
	DPU_ATRACE_END(__func__);

	return ret ? ERR_PTR(ret) : fb;
}

const struct drm_format_info *
exynos_get_format_info(const struct drm_mode_fb_cmd2 *cmd)
{
	const struct drm_format_info *info = NULL;

	if (cmd->modifier[0] == DRM_FORMAT_MOD_SAMSUNG_COLORMAP) {
		info = drm_format_info(cmd->pixel_format);
		if (info->format == DRM_FORMAT_BGRA8888)
			return info;

		DRM_WARN("%p4cc is not proper fmt for colormap\n", &cmd->pixel_format);
	} else if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0), cmd->modifier[0])) {
		int i;
		/* SAJC RGB format need 2-planes for header, payload */
		static const struct drm_format_info exynos_infos[] = {
			{ .format = DRM_FORMAT_ARGB8888,	.depth = 32, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_ABGR8888,	.depth = 32, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_XRGB8888,	.depth = 24, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1 },
			{ .format = DRM_FORMAT_XBGR8888,	.depth = 24, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1 },
			{ .format = DRM_FORMAT_RGB565,		.depth = 16, .num_planes = 2, .cpp = { 2, 0, 0 }, .hsub = 1, .vsub = 1 },
			{ .format = DRM_FORMAT_ARGB2101010,	.depth = 30, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_ABGR2101010,	.depth = 30, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_ABGR16161616F,	.depth = 0,  .num_planes = 2, .cpp = { 8, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
		};
		for (i = 0; i < ARRAY_SIZE(exynos_infos); ++i) {
			if (exynos_infos[i].format == cmd->pixel_format)
				return &exynos_infos[i];
		}

		DRM_WARN("%p4cc is not proper fmt for SAJC\n", &cmd->pixel_format);
	}

	return NULL;
}

dma_addr_t exynos_drm_fb_dma_addr(const struct drm_framebuffer *fb, int index)
{
	const struct exynos_drm_gem *exynos_gem;

	if (WARN_ON_ONCE(index >= MAX_FB_BUFFER) || !fb->obj[index])
		return 0;

	exynos_gem = to_exynos_gem(fb->obj[index]);

	DRM_DEBUG("%s: fb(%d) dma_addr[%d] = %#llx (+%#x)\n", __func__,
		fb->base.id, index, exynos_gem->dma_addr, fb->offsets[index]);

	return exynos_gem->dma_addr + fb->offsets[index];
}
