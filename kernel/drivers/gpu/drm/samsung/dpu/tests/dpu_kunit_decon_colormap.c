// SPDX-License-Identifier: GPL-2.0-only
/* dpu_kunit_decon_colormap.c
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

#include <exynos_drm_crtc.h>

#include <dpu_kunit_helper.h>

static void dpu_kunit_decon_colormap(struct kunit *test)
{
	struct kunit_mock *data = test->priv;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(data->crtc);
	int ret;

	kunit_info(test, "%s\n", __func__);

	ret = drm_atomic_commit(data->state);
	KUNIT_ASSERT_EQ_MSG(test, ret, 0, "failed to commit\n");

	if (DPU_KUNIT_DUMP)
		decon_dump(exynos_crtc);

	kunit_info(test, "%s\n", __func__);
}

static int dpu_kunit_init(struct kunit *test)
{
	struct drm_framebuffer *fb;

	kunit_info(test, "%s\n", __func__);
	dpu_kunit_alloc_data(test);
	mock_atomic_state(test);
	mock_crtc_state(test);
	mock_connector_state(test);
	fb = mock_framebuffer(test, true);
	mock_plane_state(test, fb, true);
	kunit_info(test, "%s\n", __func__);

	return 0;
}

static void dpu_kunit_exit(struct kunit *test)
{
	kunit_info(test, "%s\n", __func__);
	dpu_kunit_free_data(test);
	kunit_info(test, "%s\n", __func__);
}

static struct kunit_case dpu_kunit_cases[] = {
	KUNIT_CASE(dpu_kunit_decon_colormap),
	{}
};

#define DPU_KUNIT_NAME "dpu_kunit_decon_colormap"
static struct kunit_suite dpu_kunit_suite = {
	.name = DPU_KUNIT_NAME,
	.init = dpu_kunit_init,
	.exit = dpu_kunit_exit,
	.test_cases = dpu_kunit_cases,
};
kunit_test_suite(dpu_kunit_suite);

MODULE_AUTHOR("Wonyeong Choi <won0.choi@samsung.com>");
MODULE_DESCRIPTION(DPU_KUNIT_NAME);
MODULE_LICENSE("GPL v2");
