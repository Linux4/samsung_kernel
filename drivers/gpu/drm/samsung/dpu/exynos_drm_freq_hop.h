/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 * Authors:
 *	Hwangjae Lee <hj-yo.lee@samsung.com>
 *
 * Headef file for DPU FREQ HOP.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_FREQ_HOP_H__
#define __EXYNOS_DRM_FREQ_HOP_H__

#include <linux/types.h>

struct mutex;
struct dsim_freq_hop {
	bool enabled;
	u32 target_m;		/* will be applied to DPHY */
	u32 target_k;		/* will be applied to DPHY */
	u32 request_m;		/* user requested m value */
	u32 request_k;		/* user requested m value */

	struct mutex lock;	/* mutex lock to protect request_m,k */
};

struct dsim_device;
struct exynos_drm_crtc;
void dpu_init_freq_hop(struct dsim_device *dsim);
void dpu_freq_hop_debugfs(struct exynos_drm_crtc *exynos_crtc);
struct dsim_freq_hop *dpu_register_freq_hop(struct dsim_device *dsim);

struct dpu_freq_hop_ops {
	/**
	 * @set_freq_hop:
	 *
	 * Set the phy pmsk with target m and k value.
	 * The target m and k should be updated by update_freq_hop in advance.
	 * The driver have to call this function twice to change DPHY PLL frequency.
	 * Before shadow update, @en have to be true.
	 * After shadow update, @en have to be false.
	 */
	void (*set_freq_hop)(struct exynos_drm_crtc *exynos_crtc, bool en);
	/**
	 * @update_freq_hop:
	 *
	 * Update target m and k of freq_hop with the value requested by user.
	 * This function have to be called after dpu_init_freq_hop because
	 * target m and k value will be clear with the initial value at the
	 * dpu_init_freq_hop.
	 */
	void (*update_freq_hop)(struct exynos_drm_crtc *exynos_crtc);
};
#endif /* __EXYNOS_DRM_FREQ_HOP_H__ */
