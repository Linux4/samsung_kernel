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
#include <drm/drm_print.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <exynos_drm_encoder.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_writeback.h>
#include <dpu_trace.h>

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
	struct drm_connector *conn;
	struct drm_crtc *crtc = encoder->crtc;
	bool exit_ulps;

	if (unlikely(!funcs))
		return;

	conn = exynos_encoder_get_conn(exynos_encoder, state);
	if (!conn) {
		pr_err("can't find binded connector\n");
		return;
	}

	DPU_ATRACE_BEGIN(__func__);
	exit_ulps = is_crtc_psr_disabled(crtc, state);
	if (exit_ulps && funcs->atomic_exit_ulps)
		funcs->atomic_exit_ulps(exynos_encoder);
	else if (!exit_ulps && funcs->atomic_enable)
		funcs->atomic_enable(exynos_encoder, state);

	DPU_ATRACE_END(__func__);
}

static void exynos_encoder_atomic_disable(struct drm_encoder *encoder,
					  struct drm_atomic_state *state)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;
	struct drm_crtc *crtc = encoder->crtc;
	bool enter_ulps, exit_ulps;

	if (unlikely(!funcs))
		return;

	enter_ulps = is_crtc_psr_enabled(crtc, state);
	if (enter_ulps && funcs->atomic_enter_ulps)
		funcs->atomic_enter_ulps(exynos_encoder);
	else if (!enter_ulps && funcs->atomic_disable) {
		exit_ulps = is_crtc_psr_disabled(crtc, state);
		if (exit_ulps && funcs->atomic_exit_ulps)
			funcs->atomic_exit_ulps(exynos_encoder);
		funcs->atomic_disable(exynos_encoder, state);
	}
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

void exynos_drm_encoder_activate(struct drm_encoder *encoder,
				 struct drm_atomic_state *state)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;
	struct drm_crtc *crtc = encoder->crtc;

	if (unlikely(!funcs))
		return;

	if (is_crtc_psr_disabled(crtc, state))
		return;

	if (funcs->atomic_activate)
		funcs->atomic_activate(exynos_encoder);
}
EXPORT_SYMBOL(exynos_drm_encoder_activate);

struct exynos_drm_encoder *
exynos_drm_encoder_create(struct drm_device *drm_dev,
			  const struct exynos_drm_encoder_funcs *funcs,
			  int encoder_type, enum exynos_drm_output_type output_type,
			  void *ctx)
{
	struct exynos_drm_encoder *exynos_encoder;
	struct drm_encoder *encoder;

	exynos_encoder = drmm_encoder_alloc(drm_dev, struct exynos_drm_encoder, base,
			&exynos_encoder_funcs, encoder_type, "exynos-encoder");
	if (IS_ERR(exynos_encoder)) {
		DRM_ERROR("failed to alloc exynos_encoder(%d)\n", encoder_type);
		return exynos_encoder;
	}

	exynos_encoder->funcs = funcs;
	exynos_encoder->ctx = ctx;
	encoder = &exynos_encoder->base;
	drm_encoder_helper_add(encoder, &exynos_encoder_helper_funcs);

	encoder->possible_crtcs = exynos_drm_get_possible_crtcs(encoder, output_type);
	if (!encoder->possible_crtcs) {
		DRM_ERROR("failed to get possible crtc\n");
		return ERR_PTR(-ENOTSUPP);
	}

	return exynos_encoder;
}
EXPORT_SYMBOL(exynos_drm_encoder_create);

void exynos_drm_encoder_set_possible_mask(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	u32 wb_mask = 0;
	u32 encoder_mask = 0;

	drm_for_each_encoder(encoder, dev) {
		if (encoder->encoder_type == DRM_MODE_ENCODER_VIRTUAL)
			wb_mask |= drm_encoder_mask(encoder);

		encoder_mask |= drm_encoder_mask(encoder);
	}

	drm_for_each_encoder(encoder, dev) {
		if (encoder->encoder_type == DRM_MODE_ENCODER_VIRTUAL) {
			struct writeback_device *wb = enc_to_wb_dev(encoder);

			encoder->possible_clones = encoder_mask;
			encoder->possible_crtcs =
				exynos_drm_get_possible_crtcs(encoder, wb->output_type);
		} else {
			encoder->possible_clones = drm_encoder_mask(encoder) |
				wb_mask;
		}

		pr_info("encoder[%d] type(0x%x) possible_clones(0x%x)\n",
				encoder->index, encoder->encoder_type,
				encoder->possible_clones);
	}
}
