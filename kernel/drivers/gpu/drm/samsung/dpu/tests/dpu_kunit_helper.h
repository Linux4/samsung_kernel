// SPDX-License-Identifier: GPL-2.0-only
/* dpu_knit_helper.h
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

#ifndef __DPU_KUNIT_HELPER_H__
#define __DPU_KUNIT_HELPER_H__

#include <kunit/test.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_device.h>
#include <drm/drm_plane.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>
#include <drm/drm_framebuffer.h>

#include "../panel/panel-samsung-drv.h"
#define DPU_KUNIT_DUMP	0

/* Exported APIs tested by kunit */
extern void decon_dump(struct exynos_drm_crtc *exynos_crtc);

struct kunit_mock {
	struct drm_device		*drm_dev;
	struct drm_plane		*plane;
	struct drm_crtc			*crtc;
	struct drm_encoder		*encoder;
	struct drm_connector		*conn;
	const struct exynos_panel_mode	*default_mode;

	struct drm_atomic_state		*state;
	struct drm_modeset_acquire_ctx 	ctx;
};

int dpu_kunit_alloc_data(struct kunit *test);
void dpu_kunit_free_data(struct kunit *test);

struct drm_framebuffer* mock_framebuffer(struct kunit *test, bool colormap);
void mock_plane_state(struct kunit *test, struct drm_framebuffer *fb, bool colormap);
void mock_atomic_state(struct kunit *test);
void mock_crtc_state(struct kunit *test);
void mock_connector_state(struct kunit *test);
#endif /* __DPU_KUNIT_HELPER_H__ */
