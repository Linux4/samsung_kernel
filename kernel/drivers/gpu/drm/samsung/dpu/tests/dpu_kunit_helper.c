// SPDX-License-Identifier: GPL-2.0-only
/* dpu_kunit_helper.c
 *
 * Copyright (C) 2022 Samsung Electronics Co.Ltd
 * Authors:
 *	Wonyeong Choi <won0.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <kunit/test.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/iosys-map.h>

#include <drm/drm_drv.h>
#include <drm/drm_bridge.h>
#include <drm/drm_rect.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_modeset_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include <exynos_drm_dsim.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_dpp.h>
#include <exynos_drm_gem.h>
#include <exynos_drm_bridge.h>

#include <dpu_kunit_helper.h>
#include "../panel/panel-samsung-drv.h"

struct exynos_drm_gem *mock_gem(struct kunit *test, u32 w, u32 h)
{
	static struct drm_gem_object *gem;
	struct kunit_mock *data = test->priv;
	struct exynos_drm_device *edev = to_exynos_drm(data->drm_dev);
	struct dma_buf *buf;
	size_t size;
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(NULL);
	struct dma_heap *dma_heap;
	int ret;

	size = PAGE_ALIGN(w * h * 4);
	dma_heap = dma_heap_find("system-uncached");
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, dma_heap, "dma_heap_find() failed\n");

	buf = dma_heap_buffer_alloc(dma_heap, (size_t)size, 0, 0);
	dma_heap_put(dma_heap);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, buf,
			"failed to allocate test buffer memory\n");

	ret = dma_buf_vmap(buf, &map);
	KUNIT_ASSERT_GE_MSG(test, ret, 0, "failed to vmap buffer\n");

	memset(map.vaddr, 0x80, size);
	dma_buf_vunmap(buf, &map);

	/* create gem obj and import dma_buf */
	gem = drm_gem_prime_import_dev(data->drm_dev, buf, edev->iommu_client);
	return to_exynos_gem(gem);
}

static const struct drm_framebuffer_funcs exynos_drm_fb_funcs = {
	.destroy	= drm_gem_fb_destroy,
	.create_handle	= drm_gem_fb_create_handle,
};

struct drm_framebuffer* mock_framebuffer(struct kunit *test, bool colormap)
{
	struct kunit_mock *data = test->priv;
	struct drm_framebuffer *fb;
	struct exynos_drm_gem *exynos_gem;
	const struct drm_display_mode *mode = &data->default_mode->mode;
	struct drm_mode_fb_cmd2 fb_cmd;
	int width = mode->hdisplay;
	int height = mode->vdisplay;
	int ret;

	exynos_gem = mock_gem(test, width, height);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, exynos_gem, "failed to alloc gem\n");
	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, fb, "failed to alloc fb\n");
	memset(&fb_cmd, 0, sizeof(struct drm_mode_fb_cmd2));
	fb->obj[0] = &exynos_gem->base;
	fb_cmd.width = width;
	fb_cmd.height = height;
	fb_cmd.pixel_format = DRM_FORMAT_BGRA8888;

	if (colormap) {
		exynos_gem->flags = EXYNOS_DRM_GEM_FLAG_COLORMAP;
		fb_cmd.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_COLORMAP;
	}
	drm_helper_mode_fill_fb_struct(data->drm_dev, fb, &fb_cmd);
	ret = drm_framebuffer_init(data->drm_dev, fb, &exynos_drm_fb_funcs);
	KUNIT_ASSERT_GE_MSG(test, ret, 0, "failed to ini fb\n");

	return fb;
}

#define TEST_COLORMAP_RED		(0x00FF0000)
#define TEST_COLORMAP_GREEN		(0x0000FF00)
#define TEST_COLORMAP_BLUE		(0x000000FF)
void mock_plane_state(struct kunit *test, struct drm_framebuffer *fb, bool colormap)
{
	struct kunit_mock *data = test->priv;
	struct drm_plane_state *plane_state;
	const struct drm_display_mode *mode = &data->default_mode->mode;
	int width, height;
	int ret;

	width = mode->hdisplay;
	height = mode->vdisplay;

	plane_state = drm_atomic_get_plane_state(data->state, data->plane);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, plane_state,
			"failed to get plane_state\n");
	ret = drm_atomic_set_crtc_for_plane(plane_state, data->crtc);
	KUNIT_ASSERT_EQ_MSG(test, ret, 0, "failed to set crtc for plane\n");
	drm_atomic_set_fb_for_plane(plane_state, fb);

	plane_state->src_x = 0;
	plane_state->src_y = 0;
	plane_state->src_w = width << 16;
	plane_state->src_h = height << 16;
	plane_state->crtc_x = 0;
	plane_state->crtc_y = 0;
	plane_state->crtc_w = width;
	plane_state->crtc_h = height;

	plane_state->normalized_zpos = 0;
	plane_state->rotation = DRM_MODE_ROTATE_0;
	if (colormap) {
		struct exynos_drm_plane_state *exynos_plane_state;

		exynos_plane_state = to_exynos_plane_state(plane_state);
		exynos_plane_state->colormap = TEST_COLORMAP_RED;
	}
}

void mock_atomic_state(struct kunit *test)
{
	struct kunit_mock *data = test->priv;
	struct drm_atomic_state	*state;

	state = drm_atomic_state_alloc(data->drm_dev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, state, "failed to alloc atomic_state\n");

	drm_modeset_acquire_init(&data->ctx, DRM_MODESET_ACQUIRE_INTERRUPTIBLE);
	state->acquire_ctx = &data->ctx;

	data->state = state;
}

void mock_crtc_state(struct kunit *test)
{
	struct kunit_mock *data = test->priv;
	struct drm_crtc_state *crtc_state;
	const struct exynos_panel_mode *pmode = data->default_mode;
	int ret;

	crtc_state = drm_atomic_get_crtc_state(data->state, data->crtc);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, crtc_state, "failed to get crtc_state\n");

	ret = drm_atomic_set_mode_for_crtc(crtc_state, &pmode->mode);
	KUNIT_ASSERT_EQ_MSG(test, ret, 0, "failed to set mode for crtc\n");

	crtc_state->enable = true;
	crtc_state->active = true;
}

void mock_connector_state(struct kunit *test)
{
	struct kunit_mock *data = test->priv;
	struct drm_connector_state *conn_state;
	int ret;

	conn_state = drm_atomic_get_connector_state(data->state, data->conn);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, conn_state,
			"failed to get connector_state\n");
	ret = drm_atomic_set_crtc_for_connector(conn_state, data->crtc);
	KUNIT_ASSERT_EQ_MSG(test, ret, 0, "failed to set crtc for conn\n");
}

int dpu_kunit_alloc_data(struct kunit *test)
{
	struct kunit_mock *data;
	struct decon_device *decon;
	struct dsim_device *dsim;
	struct dpp_device *dpp;
	struct exynos_panel *epanel;
	struct drm_bridge *bridge;

	kunit_info(test, "%s\n", __func__);
	data = kunit_kzalloc(test, sizeof(*data), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, data, "failed to alloc kunit data\n");
	test->priv = data;

	decon = get_decon_drvdata(0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, decon, "failed to get decon\n");
	data->crtc = &decon->crtc->base;
	data->drm_dev = data->crtc->dev;

	dpp = decon->dpp[0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, dpp, "failed to get dpp\n");
	data->plane = &dpp->plane->base;

	dsim = get_dsim_drvdata(0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, dsim, "failed to get dsim\n");
	data->encoder = &dsim->encoder->base;
	data->encoder->crtc = data->crtc;

	bridge = drm_bridge_chain_get_first_bridge(data->encoder);
	KUNIT_ASSERT_NOT_ERR_OR_NULL_MSG(test, bridge, "failed to get bridge\n");

	epanel = exynos_bridge_to_exynos_panel(to_exynos_bridge(bridge));
	data->conn = &epanel->exynos_connector.base;
	data->default_mode = epanel->desc->default_mode;

	kunit_info(test, "%s\n", __func__);

	return 0;
}

void dpu_kunit_free_data(struct kunit *test)
{
	struct kunit_mock *data = test->priv;

	kunit_info(test, "%s\n", __func__);
	drm_atomic_state_put(data->state);
	drm_modeset_drop_locks(&data->ctx);
	drm_modeset_acquire_fini(&data->ctx);
	kunit_info(test, "%s\n", __func__);
}
MODULE_IMPORT_NS(DMA_BUF);
