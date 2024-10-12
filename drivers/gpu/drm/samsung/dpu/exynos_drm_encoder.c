// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_encoder.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 * Authors:
 *	Won Yeong Choi <won0.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drm_encoder.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <exynos_drm_encoder.h>
#include <exynos_drm_crtc.h>

static enum drm_mode_status exynos_encoder_mode_valid(struct drm_encoder *encoder,
		const struct drm_display_mode *mode)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->mode_valid)
		return funcs->mode_valid(exynos_encoder, mode);

	return MODE_NOMODE;
}

static void exynos_encoder_atomic_mode_set(struct drm_encoder *encoder,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->atomic_mode_set)
		funcs->atomic_mode_set(exynos_encoder, crtc_state, conn_state);
}

static void exynos_encoder_enable(struct drm_encoder *encoder)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->enable)
		funcs->enable(exynos_encoder);
}

static void exynos_encoder_disable(struct drm_encoder *encoder)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->disable)
		funcs->disable(exynos_encoder);
}

static void exynos_encoder_atomic_enable(struct drm_encoder *encoder,
					 struct drm_atomic_state *state)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->atomic_enable)
		funcs->atomic_enable(exynos_encoder, state);
}

static void exynos_encoder_atomic_disable(struct drm_encoder *encoder,
					  struct drm_atomic_state *state)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->atomic_disable)
		funcs->atomic_disable(exynos_encoder, state);
}

static int exynos_encoder_atomic_check(struct drm_encoder *encoder,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->atomic_check)
		return funcs->atomic_check(exynos_encoder, crtc_state, conn_state);

	return 0;
}

static const struct drm_encoder_helper_funcs exynos_encoder_helper_funcs = {
	.mode_valid = exynos_encoder_mode_valid,
	.atomic_mode_set = exynos_encoder_atomic_mode_set,
	.enable = exynos_encoder_enable,
	.disable = exynos_encoder_disable,
	.atomic_enable = exynos_encoder_atomic_enable,
	.atomic_disable = exynos_encoder_atomic_disable,
	.atomic_check = exynos_encoder_atomic_check,
};

static const struct drm_encoder_funcs exynos_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

struct drm_connector *exynos_encoder_get_conn(struct exynos_drm_encoder *exynos_encoder,
					      struct drm_atomic_state *state)
{
	struct drm_connector *conn;
	const struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(state, conn, new_conn_state, i) {
		if (new_conn_state->best_encoder == &exynos_encoder->base)
			return conn;
	}

	return NULL;
}

int exynos_drm_encoder_init(struct drm_device *drm_dev,
		struct exynos_drm_encoder *exynos_encoder,
		const struct exynos_drm_encoder_funcs *funcs,
		int encoder_type, enum exynos_drm_output_type output_type)
{
	int ret;
	struct drm_encoder *encoder = &exynos_encoder->base;

	exynos_encoder->funcs = funcs;
	ret = drm_encoder_init(drm_dev, encoder, &exynos_encoder_funcs,
			encoder_type, NULL);
	drm_encoder_helper_add(encoder, &exynos_encoder_helper_funcs);

	encoder->possible_crtcs = exynos_drm_get_possible_crtcs(encoder, output_type);
	if (!encoder->possible_crtcs) {
		pr_err("failed to get possible crtc, ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
		return -ENOTSUPP;
	}

	return ret;
}
EXPORT_SYMBOL(exynos_drm_encoder_init);
