// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_bridge.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 * Authors:
 *	Beob Ki Jung <beobki.jung@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drm_bridge.h>
#include <drm/drm_atomic_state_helper.h>
#include <exynos_drm_bridge.h>
#include <exynos_drm_crtc.h>
#include <dpu_trace.h>

static int exynos_bridge_attach(struct drm_bridge *bridge,
					enum drm_bridge_attach_flags flags)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);
	const struct exynos_drm_bridge_funcs *funcs = exynos_bridge->funcs;

	if (funcs && funcs->attach)
		return funcs->attach(exynos_bridge, flags);

	return 0;
}

static void exynos_bridge_detach(struct drm_bridge *bridge)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);
	const struct exynos_drm_bridge_funcs *funcs = exynos_bridge->funcs;

	if (funcs && funcs->detach)
		funcs->detach(exynos_bridge);
}

static void exynos_bridge_atomic_pre_enable(struct drm_bridge *bridge,
					    struct drm_bridge_state *old_bridge_state)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);
	const struct exynos_drm_bridge_funcs *funcs = exynos_bridge->funcs;
	struct exynos_drm_bridge_state *exynos_bridge_state =
				bridge_to_exynos_bridge_state(bridge);

	if (exynos_bridge_state->psr_enabled)
		return;

	DPU_ATRACE_BEGIN(__func__);
	if (funcs && funcs->pre_enable)
		funcs->pre_enable(exynos_bridge);
	DPU_ATRACE_END(__func__);
}

static void exynos_bridge_atomic_enable(struct drm_bridge *bridge,
					struct drm_bridge_state *old_bridge_state)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);
	const struct exynos_drm_bridge_funcs *funcs = exynos_bridge->funcs;
	struct exynos_drm_bridge_state *exynos_bridge_state =
				bridge_to_exynos_bridge_state(bridge);
	bool psr_req = exynos_bridge_state->psr_request;
	bool psr_en = exynos_bridge_state->psr_enabled;

	if (unlikely(!funcs))
		return;

	DPU_ATRACE_BEGIN(__func__);
	if (!psr_req && psr_en && funcs->psr_stop) {
		funcs->psr_stop(exynos_bridge);
		exynos_bridge_state->psr_enabled = false;
	} else if (funcs->enable)
		funcs->enable(exynos_bridge);
	DPU_ATRACE_END(__func__);
}

static void exynos_bridge_atomic_disable(struct drm_bridge *bridge,
					 struct drm_bridge_state *old_bridge_state)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);
	const struct exynos_drm_bridge_funcs *funcs = exynos_bridge->funcs;
	struct exynos_drm_bridge_state *exynos_bridge_state =
				bridge_to_exynos_bridge_state(bridge);

	if (unlikely(!funcs))
		return;

	DPU_ATRACE_BEGIN(__func__);
	if (exynos_bridge_state->psr_request && funcs->psr_start) {
		funcs->psr_start(exynos_bridge);
		exynos_bridge_state->psr_enabled = true;
	} else if (funcs->disable) {
		if (funcs->disable(exynos_bridge) == 0)
			exynos_bridge_state->psr_enabled = false;
	}
	DPU_ATRACE_END(__func__);
}

static void exynos_bridge_atomic_post_disable(struct drm_bridge *bridge,
					      struct drm_bridge_state *old_bridge_state)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);
	const struct exynos_drm_bridge_funcs *funcs = exynos_bridge->funcs;
	struct exynos_drm_bridge_state *exynos_bridge_state =
				bridge_to_exynos_bridge_state(bridge);

	if (exynos_bridge_state->psr_enabled)
		return;

	DPU_ATRACE_BEGIN(__func__);
	if (funcs && funcs->post_disable)
		funcs->post_disable(exynos_bridge);
	DPU_ATRACE_END(__func__);
}

static void exynos_bridge_mode_set(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  const struct drm_display_mode *adjusted_mode)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);
	const struct exynos_drm_bridge_funcs *funcs = exynos_bridge->funcs;

	if (funcs && funcs->mode_set)
		funcs->mode_set(exynos_bridge, mode, adjusted_mode);
}

static int exynos_drm_bridge_atomic_check(struct drm_bridge *bridge,
						 struct drm_bridge_state *bridge_state,
						 struct drm_crtc_state *crtc_state,
						 struct drm_connector_state *conn_state)
{
	struct exynos_drm_bridge_state *exynos_bridge_state =
				to_exynos_bridge_state(bridge_state);

	exynos_bridge_state->psr_request =
			crtc_state && crtc_state->self_refresh_active;

	return 0;
}

struct drm_bridge_state *
exynos_drm_bridge_atomic_duplicate_state(struct drm_bridge *bridge)
{
	struct exynos_drm_bridge_state *new;
	struct exynos_drm_bridge_state *old;

	if (WARN_ON(!bridge->base.state))
		return NULL;

	old = bridge_to_exynos_bridge_state(bridge);

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (new) {
		__drm_atomic_helper_bridge_duplicate_state(bridge, &new->base);
		if (old)
			new->psr_enabled = old->psr_enabled;
	}

	return &new->base;
}

void exynos_drm_bridge_atomic_destroy_state(struct drm_bridge *bridge,
					    struct drm_bridge_state *state)
{
	struct exynos_drm_bridge_state *exynos_state =
					to_exynos_bridge_state(state);

	kfree(exynos_state);
}

struct drm_bridge_state *exynos_drm_bridge_atomic_reset(struct drm_bridge *bridge)
{
	struct drm_bridge_state *state;
	struct exynos_drm_bridge_state *exynos_state;

	exynos_state = kzalloc(sizeof(*exynos_state), GFP_KERNEL);
	if (!exynos_state)
		return ERR_PTR(-ENOMEM);

	state = &exynos_state->base;
	exynos_state->psr_request = false;
	exynos_state->psr_enabled = false;

	__drm_atomic_helper_bridge_reset(bridge, state);

	return state;
}

static const struct drm_bridge_funcs exynos_bridge_funcs = {
	.attach				= exynos_bridge_attach,
	.detach				= exynos_bridge_detach,
	.atomic_pre_enable		= exynos_bridge_atomic_pre_enable,
	.atomic_enable 			= exynos_bridge_atomic_enable,
	.atomic_disable 		= exynos_bridge_atomic_disable,
	.atomic_post_disable 		= exynos_bridge_atomic_post_disable,
	.mode_set 			= exynos_bridge_mode_set,
	.atomic_check			= exynos_drm_bridge_atomic_check,
	.atomic_duplicate_state		= exynos_drm_bridge_atomic_duplicate_state,
	.atomic_destroy_state		= exynos_drm_bridge_atomic_destroy_state,
	.atomic_reset			= exynos_drm_bridge_atomic_reset,
};

void exynos_drm_bridge_reset(struct drm_bridge *bridge, enum exynos_bridge_reset_pos pos)
{
	struct exynos_drm_bridge *exynos_bridge = NULL;
	struct exynos_drm_bridge_state *state = NULL;
	const struct exynos_drm_bridge_funcs *funcs = NULL;

	if (!bridge)
		return;

	exynos_bridge = to_exynos_bridge(bridge);
	if (!exynos_bridge)
		return;

	state = bridge_to_exynos_bridge_state(bridge);

	funcs = exynos_bridge->funcs;

	if (state->psr_enabled)
		return;

	if (exynos_bridge->reset_pos != pos)
		return;

	if (funcs && funcs->reset)
		funcs->reset(exynos_bridge);
}
EXPORT_SYMBOL(exynos_drm_bridge_reset);

bool exynos_drm_bridge_lp11_reset(struct drm_bridge *bridge)
{
	struct exynos_drm_bridge *exynos_bridge = to_exynos_bridge(bridge);

	return ((exynos_bridge->reset_pos == BRIDGE_RESET_LP11) ? true : false);
}
EXPORT_SYMBOL(exynos_drm_bridge_lp11_reset);

void exynos_drm_bridge_init(struct device *dev, struct exynos_drm_bridge *exynos_bridge,
		const struct exynos_drm_bridge_funcs *funcs)
{
	struct drm_bridge *bridge = &exynos_bridge->base;

	exynos_bridge->funcs = funcs;

	bridge->funcs = &exynos_bridge_funcs;

#ifdef CONFIG_OF
	bridge->of_node = dev->of_node;
#endif

	drm_bridge_add(bridge);
}
EXPORT_SYMBOL(exynos_drm_bridge_init);
