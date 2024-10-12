/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_DP_H__
#define __SPRD_DP_H__

#include <linux/of.h>
#include <linux/device.h>
#include <video/videomode.h>

#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>

#include "disp_lib.h"

struct dp_context {
	void __iomem *base;
	void __iomem *dpu1_dpi_reg;
	void __iomem *tca_base;
	struct regmap *ipa_usb31_dp;
	struct regmap *aon_apb;
	struct regmap *ipa_dispc1_glb_apb;
	struct regmap *ipa_apb;
	struct regmap *force_shutdown;
	struct videomode vm;
	bool enabled;
	u8 lanes;
	u32 format;
	int gpio_en1;
	int gpio_en2;
	int irq;
};

struct dp_glb_ops {
	int (*parse_dt)(struct dp_context *ctx,
			struct device_node *np);
	void (*enable)(struct dp_context *ctx);
	void (*disable)(struct dp_context *ctx);
	void (*reset)(struct dp_context *ctx);
	void (*power)(struct dp_context *ctx, int enable);
	void (*detect)(struct dp_context *ctx, int enable);
};

struct sprd_dp_ops {
	const struct dp_glb_ops *glb;
};

struct sprd_dp {
	struct device dev;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_bridge *bridge;
	struct drm_panel *panel;
	const struct dp_glb_ops *glb;
	struct dp_context ctx;
	struct dptx *snps_dptx;
	struct platform_device *audio_pdev;
	bool sink_has_audio;
	bool hpd_status;
	struct notifier_block dp_nb;
	/* edid releated information for reporting display device HW info to framework */
	struct edid edid_info;
	struct drm_property *edid_prop;
	struct drm_property_blob *edid_blob;
};

extern struct list_head dp_glb_head;

#define dp_glb_ops_register(entry) \
	disp_ops_register(entry, &dp_glb_head)

#define dp_glb_ops_attach(str) \
	disp_ops_attach(str, &dp_glb_head)

#endif /* __SPRD_DP_H__ */
