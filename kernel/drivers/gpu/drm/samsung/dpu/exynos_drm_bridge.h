/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_bridge.h
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

#ifndef _EXYNOS_DRM_BRIDGE_H_
#define _EXYNOS_DRM_BRIDGE_H_

#include <exynos_drm_drv.h>
#include <drm/drm_bridge.h>

enum exynos_bridge_reset_pos {
	BRIDGE_RESET_LP00,	/* datalane lp00, clklane lp */
	BRIDGE_RESET_LP11,	/* datalane lp11, clklane lp */
	BRIDGE_RESET_LP11_HS,	/* datalane lp11, clklane hs : default */
	BRIDGE_RESET_MAX,
};

struct exynos_drm_bridge_state {
	struct drm_bridge_state base;
	bool psr_request;
	bool psr_enabled;
};

#define to_exynos_bridge(e) \
	container_of((e), struct exynos_drm_bridge, base)
#define to_exynos_bridge_state(e) \
	container_of((e), struct exynos_drm_bridge_state, base)
#define bridge_to_exynos_bridge_state(e) \
	to_exynos_bridge_state(drm_priv_to_bridge_state((e)->base.state))

/*
 * Exynos drm encoder ops
 *
 * @mode_valid: specific driver callback for mode validation
 * @atomic_mode_set: specific driver callback for enabling vblank interrupt.
 * @enable: enable the device
 * @disable: disable the device
 * @atomic_check: validate state
 */

struct exynos_drm_bridge;
struct exynos_drm_bridge_funcs {
	int (*attach)(struct exynos_drm_bridge *bridge, enum drm_bridge_attach_flags flags);
	void (*detach)(struct exynos_drm_bridge *bridge);
	void (*pre_enable)(struct exynos_drm_bridge *bridge);
	void (*enable)(struct exynos_drm_bridge *bridge);
	int (*disable)(struct exynos_drm_bridge *bridge);
	void (*post_disable)(struct exynos_drm_bridge *bridge);
	void (*mode_set)(struct exynos_drm_bridge *bridge,
			 const struct drm_display_mode *mode,
			 const struct drm_display_mode *adjusted_mode);
	void (*reset)(struct exynos_drm_bridge *bridge);
	void (*psr_stop)(struct exynos_drm_bridge *bridge);
	void (*psr_start)(struct exynos_drm_bridge *bridge);
	void (*lastclose)(struct exynos_drm_bridge *bridge);
};

/*
 * Exynos specific bridge structure.
 *
 * @base: bridge object.
 * @funcs: callback fuctions.
 * @reset_pos: bridge reset position.
 */
struct exynos_drm_bridge {
	struct drm_bridge			base;
	enum exynos_bridge_reset_pos		reset_pos;
	const struct exynos_drm_bridge_funcs	*funcs;
};

void exynos_drm_bridge_init(struct device *dev, struct exynos_drm_bridge *exynos_bridge,
			const struct exynos_drm_bridge_funcs *funcs);
void exynos_drm_bridge_reset(struct drm_bridge *bridge, enum exynos_bridge_reset_pos pos);
bool exynos_drm_bridge_lp11_reset(struct drm_bridge *bridge);
#endif
