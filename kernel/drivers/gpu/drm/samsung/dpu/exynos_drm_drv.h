/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_drv.h
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

#ifndef _EXYNOS_DRM_DRV_H_
#define _EXYNOS_DRM_DRV_H_

#include <drm/drm_atomic.h>
#include <drm/drm_property.h>

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <samsung_drm.h>
#include <exynos_drm_connector.h>
#include <exynos_drm_bts.h>

#define get_plane_num(drm_dev) (drm_dev->mode_config.num_total_plane)

/* this enumerates display type. */
enum exynos_drm_output_type {
	EXYNOS_DISPLAY_TYPE_NONE = 0,
	/* RGB or CPU Interface. */
	EXYNOS_DISPLAY_TYPE_DSI0 = BIT(0),
	EXYNOS_DISPLAY_TYPE_DSI1 = BIT(1),
	EXYNOS_DISPLAY_TYPE_DSI  = EXYNOS_DISPLAY_TYPE_DSI0 |
					EXYNOS_DISPLAY_TYPE_DSI1,
	/* HDMI Interface. */
	EXYNOS_DISPLAY_TYPE_HDMI = BIT(2),
	/* Virtual Display Interface. */
	EXYNOS_DISPLAY_TYPE_VIDI = BIT(3),

	/* DP Interface. */
	EXYNOS_DISPLAY_TYPE_DP0_SST1  = BIT(4),
	EXYNOS_DISPLAY_TYPE_DP0_SST2  = BIT(5),
	EXYNOS_DISPLAY_TYPE_DP0_SST3  = BIT(6),
	EXYNOS_DISPLAY_TYPE_DP0_SST4  = BIT(7),
	EXYNOS_DISPLAY_TYPE_DP1_SST1  = BIT(8),
	EXYNOS_DISPLAY_TYPE_DP1_SST2  = BIT(9),
	EXYNOS_DISPLAY_TYPE_DP1_SST3  = BIT(10),
	EXYNOS_DISPLAY_TYPE_DP1_SST4  = BIT(11),
	EXYNOS_DISPLAY_TYPE_DP0   = EXYNOS_DISPLAY_TYPE_DP0_SST1 |
					EXYNOS_DISPLAY_TYPE_DP0_SST2 |
					EXYNOS_DISPLAY_TYPE_DP0_SST3 |
					EXYNOS_DISPLAY_TYPE_DP0_SST4,
	EXYNOS_DISPLAY_TYPE_DP1   = EXYNOS_DISPLAY_TYPE_DP1_SST1 |
					EXYNOS_DISPLAY_TYPE_DP1_SST2 |
					EXYNOS_DISPLAY_TYPE_DP1_SST3 |
					EXYNOS_DISPLAY_TYPE_DP1_SST4,
};

enum exynos_drm_writeback_type {
	EXYNOS_WB_NONE,
	EXYNOS_WB_CWB,
	EXYNOS_WB_SWB,
};

struct drm_exynos_file_private {
	u32 dummy;
};

struct exynos_drm_priv_state {
	struct drm_private_state base;
	/* If need shared resource control for multi-display pipeline, add it here. */
};

static inline struct exynos_drm_priv_state *
to_exynos_priv_state(const struct drm_private_state *state)
{
	return container_of(state, struct exynos_drm_priv_state, base);
}

struct exynos_drm_properties {
	/* properties for crtc */
	struct drm_property *color_mode;
	struct drm_property *render_intent;
	struct drm_property *adjusted_vblank;
	struct drm_property *operation_mode;
	struct drm_property *dsr_status;
	struct drm_property *modeset_only;
	struct drm_property *partial;
	struct drm_property *dqe_fd;
	struct drm_property *bts_fps;
	struct drm_property *expected_present_time;

	/* properties for plane */
	struct drm_property *standard;
	struct drm_property *transfer;
	struct drm_property *range;
	struct drm_property *colormap;
	struct drm_property *split;
	struct drm_property *hdr_fd;
	struct drm_property *block;

	/* properties for connector */
	struct drm_property *max_luminance;
	struct drm_property *max_avg_luminance;
	struct drm_property *min_luminance;
	struct drm_property *hdr_formats;
	struct drm_property *adjusted_fps;
	struct drm_property *lp_mode;
	struct drm_property *hdr_sink_connected;
	struct drm_property *use_repeater_buffer;
	struct drm_property *idle_supported;
	struct drm_property *dual_blender;
#if IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	struct drm_property *fingerprint_mask;
#endif
	/* common properties */
	struct drm_property *restrictions;
};

/*
 * Exynos drm private structure.
 *
 * @lock: mutext lock to protect available_win_mask.
 * @obj: base drm private object struct.
 * @available_win_mask: Bitmask of the window that all displays are available.
 */
struct exynos_drm_private {
	struct mutex		lock;
	struct drm_private_obj obj;
	unsigned int available_win_mask;
};

/*
 * Exynos drm device structure.
 *
 * @base: base drm device structure.
 * @props: exynos private properties.
 * @iommu_client: main iommu device(usually decon0) to be attached dma_buf of gem.
 * @fault_ctx: dpu fault context to report dpu fault and to print debug information.
 */
struct exynos_drm_device {
	struct drm_device base;
	struct exynos_drm_properties props;
	struct device *iommu_client;
	void *fault_ctx;
};

static inline struct exynos_drm_device *to_exynos_drm(struct drm_device *drm_dev)
{
	return container_of(drm_dev, struct exynos_drm_device, base);
}

void exynos_drm_init_resource_cnt(void);

static inline struct exynos_drm_properties *
dev_get_exynos_props(struct drm_device *dev)
{
	struct exynos_drm_device *exynos_dev = to_exynos_drm(dev);

	if (WARN_ON(!exynos_dev))
		return NULL;

	return &exynos_dev->props;
}

int exynos_drm_replace_property_blob_from_id(struct drm_device *dev,
					     struct drm_property_blob **blob,
					     uint64_t blob_id,
					     ssize_t expected_size);
#endif
