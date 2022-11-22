// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "dp_drm.h"
#include "dp_debug.h"
#ifdef CONFIG_SEC_DISPLAYPORT
#include "secdp.h"
#define DP_ENUM_STR(x)	#x
#endif

#define DP_MST_DEBUG(fmt, ...) DP_DEBUG(fmt, ##__VA_ARGS__)

#define to_dp_bridge(x)     container_of((x), struct dp_bridge, base)

void convert_to_drm_mode(const struct dp_display_mode *dp_mode,
				struct drm_display_mode *drm_mode)
{
	u32 flags = 0;

	memset(drm_mode, 0, sizeof(*drm_mode));

	drm_mode->hdisplay = dp_mode->timing.h_active;
	drm_mode->hsync_start = drm_mode->hdisplay +
				dp_mode->timing.h_front_porch;
	drm_mode->hsync_end = drm_mode->hsync_start +
			      dp_mode->timing.h_sync_width;
	drm_mode->htotal = drm_mode->hsync_end + dp_mode->timing.h_back_porch;
	drm_mode->hskew = dp_mode->timing.h_skew;

	drm_mode->vdisplay = dp_mode->timing.v_active;
	drm_mode->vsync_start = drm_mode->vdisplay +
				dp_mode->timing.v_front_porch;
	drm_mode->vsync_end = drm_mode->vsync_start +
			      dp_mode->timing.v_sync_width;
	drm_mode->vtotal = drm_mode->vsync_end + dp_mode->timing.v_back_porch;

	drm_mode->vrefresh = dp_mode->timing.refresh_rate;
	drm_mode->clock = dp_mode->timing.pixel_clk_khz;

	if (dp_mode->timing.h_active_low)
		flags |= DRM_MODE_FLAG_NHSYNC;
	else
		flags |= DRM_MODE_FLAG_PHSYNC;

	if (dp_mode->timing.v_active_low)
		flags |= DRM_MODE_FLAG_NVSYNC;
	else
		flags |= DRM_MODE_FLAG_PVSYNC;

	drm_mode->flags = flags;

	drm_mode->type = 0x48;
	drm_mode_set_name(drm_mode);
}

static int dp_bridge_attach(struct drm_bridge *dp_bridge)
{
	struct dp_bridge *bridge = to_dp_bridge(dp_bridge);

	if (!dp_bridge) {
		DP_ERR("Invalid params\n");
		return -EINVAL;
	}

	DP_DEBUG("[%d] attached\n", bridge->id);

	return 0;
}

static void dp_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	pr_debug("+++\n");

	bridge = to_dp_bridge(drm_bridge);
	dp = bridge->display;

	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		return;
	}

	/* By this point mode should have been validated through mode_fixup */
	rc = dp->set_mode(dp, bridge->dp_panel, &bridge->dp_mode);
	if (rc) {
		DP_ERR("[%d] failed to perform a mode set, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->prepare(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display prepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	/* for SST force stream id, start slot and total slots to 0 */
	dp->set_stream_info(dp, bridge->dp_panel, 0, 0, 0, 0, 0);

	rc = dp->enable(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display enable failed, rc=%d\n",
		       bridge->id, rc);
		dp->unprepare(dp, bridge->dp_panel);
	}
}

static void dp_bridge_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	pr_debug("+++\n");

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	rc = dp->post_enable(dp, bridge->dp_panel);
	if (rc)
		DP_ERR("[%d] DP display post enable failed, rc=%d\n",
		       bridge->id, rc);
}

static void dp_bridge_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	pr_debug("+++\n");

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	if (!dp) {
		DP_ERR("dp is null\n");
		return;
	}

	if (dp)
		sde_connector_helper_bridge_disable(bridge->connector);

	rc = dp->pre_disable(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display pre disable failed, rc=%d\n",
		       bridge->id, rc);
	}
}

static void dp_bridge_post_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	pr_debug("+++\n");

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	rc = dp->disable(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display disable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	rc = dp->unprepare(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display unprepare failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}
}

static void dp_bridge_mode_set(struct drm_bridge *drm_bridge,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge || !mode || !adjusted_mode) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	if (!bridge->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		return;
	}

	dp = bridge->display;

	dp->convert_to_dp_mode(dp, bridge->dp_panel, adjusted_mode,
			&bridge->dp_mode);
}

static bool dp_bridge_mode_fixup(struct drm_bridge *drm_bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	bool ret = true;
	struct dp_display_mode dp_mode;
	struct dp_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge || !mode || !adjusted_mode) {
		DP_ERR("Invalid params\n");
		ret = false;
		goto end;
	}

	bridge = to_dp_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		ret = false;
		goto end;
	}

	if (!bridge->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		ret = false;
		goto end;
	}

	dp = bridge->display;

	dp->convert_to_dp_mode(dp, bridge->dp_panel, mode, &dp_mode);
	convert_to_drm_mode(&dp_mode, adjusted_mode);
end:
	return ret;
}

static const struct drm_bridge_funcs dp_bridge_ops = {
	.attach       = dp_bridge_attach,
	.mode_fixup   = dp_bridge_mode_fixup,
	.pre_enable   = dp_bridge_pre_enable,
	.enable       = dp_bridge_enable,
	.disable      = dp_bridge_disable,
	.post_disable = dp_bridge_post_disable,
	.mode_set     = dp_bridge_mode_set,
};

int dp_connector_config_hdr(struct drm_connector *connector, void *display,
	struct sde_connector_state *c_state)
{
	struct dp_display *dp = display;
	struct sde_connector *sde_conn;

	if (!display || !c_state || !connector) {
		DP_ERR("invalid params\n");
		return -EINVAL;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid dp panel\n");
		return -EINVAL;
	}

	return dp->config_hdr(dp, sde_conn->drv_panel, &c_state->hdr_meta,
			c_state->dyn_hdr_meta.dynamic_hdr_update);
}

int dp_connector_set_colorspace(struct drm_connector *connector,
	void *display)
{
	struct dp_display *dp_display = display;
	struct sde_connector *sde_conn;

	if (!dp_display || !connector)
		return -EINVAL;

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		pr_err("invalid dp panel\n");
		return -EINVAL;
	}

	return dp_display->set_colorspace(dp_display,
		sde_conn->drv_panel, connector->state->colorspace);
}

int dp_connector_post_init(struct drm_connector *connector, void *display)
{
	int rc;
	struct dp_display *dp_display = display;
	struct sde_connector *sde_conn;

	if (!dp_display || !connector)
		return -EINVAL;

	dp_display->base_connector = connector;
	dp_display->bridge->connector = connector;

	if (dp_display->post_init) {
		rc = dp_display->post_init(dp_display);
		if (rc)
			goto end;
	}

	sde_conn = to_sde_connector(connector);
	dp_display->bridge->dp_panel = sde_conn->drv_panel;

	rc = dp_mst_init(dp_display);
end:
	return rc;
}

int dp_connector_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	const u32 single_intf = 1;
	const u32 no_enc = 0;
	struct msm_display_topology *topology;
	struct sde_connector *sde_conn;
	struct dp_panel *dp_panel;
	struct dp_display_mode dp_mode;
	struct dp_display *dp_disp = display;
	struct msm_drm_private *priv;
	int rc = 0;

	if (!drm_mode || !mode_info || !avail_res ||
			!avail_res->max_mixer_width || !connector || !display ||
			!connector->dev || !connector->dev->dev_private) {
		DP_ERR("invalid params\n");
		return -EINVAL;
	}

	memset(mode_info, 0, sizeof(*mode_info));

	sde_conn = to_sde_connector(connector);
	dp_panel = sde_conn->drv_panel;
	priv = connector->dev->dev_private;

	topology = &mode_info->topology;

	rc = msm_get_mixer_count(priv, drm_mode, avail_res,
			&topology->num_lm);
	if (rc) {
		DP_ERR("error getting mixer count. rc:%d\n", rc);
		return rc;
	}

	topology->num_enc = no_enc;
	topology->num_intf = single_intf;

	mode_info->frame_rate = drm_mode->vrefresh;
	mode_info->vtotal = drm_mode->vtotal;

	mode_info->wide_bus_en = dp_panel->widebus_en;

	dp_disp->convert_to_dp_mode(dp_disp, dp_panel, drm_mode, &dp_mode);

	if (dp_mode.timing.comp_info.comp_ratio) {
		memcpy(&mode_info->comp_info,
			&dp_mode.timing.comp_info,
			sizeof(mode_info->comp_info));

		topology->num_enc = topology->num_lm;
	}

	return 0;
}

int dp_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *data)
{
	struct dp_display *display = data;

	if (!info || !display || !display->drm_dev) {
		DP_ERR("invalid params\n");
		return -EINVAL;
	}

	info->intf_type = DRM_MODE_CONNECTOR_DisplayPort;

	info->num_of_h_tiles = 1;
	info->h_tile_instance[0] = 0;
	info->is_connected = display->is_sst_connected;
	info->capabilities = MSM_DISPLAY_CAP_VID_MODE | MSM_DISPLAY_CAP_EDID |
		MSM_DISPLAY_CAP_HOT_PLUG;

	return 0;
}

enum drm_connector_status dp_connector_detect(struct drm_connector *conn,
		bool force,
		void *display)
{
	enum drm_connector_status status = connector_status_unknown;
	struct msm_display_info info;
	int rc;

	if (!conn || !display)
		return status;

	/* get display dp_info */
	memset(&info, 0x0, sizeof(info));
	rc = dp_connector_get_info(conn, &info, display);
	if (rc) {
		DP_ERR("failed to get display info, rc=%d\n", rc);
		return connector_status_disconnected;
	}

	if (info.capabilities & MSM_DISPLAY_CAP_HOT_PLUG)
		status = (info.is_connected ? connector_status_connected :
					      connector_status_disconnected);
	else
		status = connector_status_connected;

	conn->display_info.width_mm = info.width_mm;
	conn->display_info.height_mm = info.height_mm;

	return status;
}

void dp_connector_post_open(struct drm_connector *connector, void *display)
{
	struct dp_display *dp;

	if (!display) {
		DP_ERR("invalid input\n");
		return;
	}

	dp = display;

	if (dp->post_open)
		dp->post_open(dp);
}

int dp_connector_atomic_check(struct drm_connector *connector,
	void *display,
	struct drm_connector_state *c_state)
{
	struct sde_connector *sde_conn;
	struct drm_connector_state *old_state;

	if (!connector || !display)
		return -EINVAL;

	old_state =
		drm_atomic_get_old_connector_state(c_state->state, connector);

	if (!old_state)
		return -EINVAL;

	sde_conn = to_sde_connector(connector);

	/*
	 * Marking the colorspace has been changed
	 * the flag shall be checked in the pre_kickoff
	 * to configure the new colorspace in HW
	 */
	if (c_state->colorspace != old_state->colorspace) {
		DP_DEBUG("colorspace has been updated\n");
		sde_conn->colorspace_updated = true;
	}

	return 0;
}

int dp_connector_get_modes(struct drm_connector *connector,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	int rc = 0;
	struct dp_display *dp;
	struct dp_display_mode *dp_mode = NULL;
	struct drm_display_mode *m, drm_mode;
	struct sde_connector *sde_conn;

	if (!connector || !display)
		return 0;

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid dp panel\n");
		return 0;
	}

	dp = display;

	dp_mode = kzalloc(sizeof(*dp_mode),  GFP_KERNEL);
	if (!dp_mode)
		return 0;

	/* pluggable case assumes EDID is read when HPD */
	if (dp->is_sst_connected) {
		rc = dp->get_modes(dp, sde_conn->drv_panel, dp_mode);
		if (!rc)
			DP_ERR("failed to get DP sink modes, rc=%d\n", rc);

		if (dp_mode->timing.pixel_clk_khz) { /* valid DP mode */
			memset(&drm_mode, 0x0, sizeof(drm_mode));
			convert_to_drm_mode(dp_mode, &drm_mode);
			m = drm_mode_duplicate(connector->dev, &drm_mode);
			if (!m) {
				DP_ERR("failed to add mode %ux%u\n",
				       drm_mode.hdisplay,
				       drm_mode.vdisplay);
				kfree(dp_mode);
				return 0;
			}
			m->width_mm = connector->display_info.width_mm;
			m->height_mm = connector->display_info.height_mm;
			drm_mode_probed_add(connector, m);
		}
	} else {
		DP_ERR("No sink connected\n");
	}
	kfree(dp_mode);

	return rc;
}

int dp_drm_bridge_init(void *data, struct drm_encoder *encoder)
{
	int rc = 0;
	struct dp_bridge *bridge;
	struct drm_device *dev;
	struct dp_display *display = data;
	struct msm_drm_private *priv = NULL;

	bridge = kzalloc(sizeof(*bridge), GFP_KERNEL);
	if (!bridge) {
		rc = -ENOMEM;
		goto error;
	}

	dev = display->drm_dev;
	bridge->display = display;
	bridge->base.funcs = &dp_bridge_ops;
	bridge->base.encoder = encoder;

	priv = dev->dev_private;

	rc = drm_bridge_attach(encoder, &bridge->base, NULL);
	if (rc) {
		DP_ERR("failed to attach bridge, rc=%d\n", rc);
		goto error_free_bridge;
	}

	rc = display->request_irq(display);
	if (rc) {
		DP_ERR("request_irq failed, rc=%d\n", rc);
		goto error_free_bridge;
	}

	encoder->bridge = &bridge->base;
	priv->bridges[priv->num_bridges++] = &bridge->base;
	display->bridge = bridge;

	return 0;
error_free_bridge:
	kfree(bridge);
error:
	return rc;
}

void dp_drm_bridge_deinit(void *data)
{
	struct dp_display *display = data;
	struct dp_bridge *bridge = display->bridge;

	if (bridge && bridge->base.encoder)
		bridge->base.encoder->bridge = NULL;

	kfree(bridge);
}

#ifdef CONFIG_SEC_DISPLAYPORT
/* Index of max resolution which supported by sink */
static int g_max_res_index;

/* Index of max resolution which supported by dex station */
static int g_dex_max_res_index;
static int g_ignore_ratio = 0;

void secdp_dex_res_init(void)
{
	g_max_res_index = g_dex_max_res_index = -1;
	g_ignore_ratio = 0;
}

static struct secdp_display_timing secdp_supported_resolution[] = {
	{ 0,   640,    480,  60, false, DEX_RES_1920X1080},
	{ 1,   720,    480,  60, false, DEX_RES_1920X1080},
	{ 2,   720,    576,  50, false, DEX_RES_1920X1080},

	{ 3,   1280,   720,  50, false, DEX_RES_1920X1080,   MON_RATIO_16_9},
	{ 4,   1280,   720,  60, false, DEX_RES_1920X1080,   MON_RATIO_16_9},

	{ 5,   1280,   768,  60, false, DEX_RES_1920X1080},                    /* CTS 4.4.1.3 */
	{ 6,   1280,   800,  60, false, DEX_RES_1920X1080,   MON_RATIO_16_10}, /* CTS 18bpp */
	{ 7,   1280,  1024,  60, false, DEX_RES_1920X1080},                    /* CTS 18bpp */
	{ 8,   1360,   768,  60, false, DEX_RES_1920X1080,   MON_RATIO_16_9},  /* CTS 4.4.1.3 */

	{ 9,   1366,  768,   60, false, DEX_RES_1920X1080,   MON_RATIO_16_9},
	{10,   1600,  900,   60, false, DEX_RES_1920X1080,   MON_RATIO_16_9},

	{20,   1920,  1080,  24, false, DEX_RES_1920X1080,   MON_RATIO_16_9},
	{21,   1920,  1080,  25, false, DEX_RES_1920X1080,   MON_RATIO_16_9},
	{22,   1920,  1080,  30, false, DEX_RES_1920X1080,   MON_RATIO_16_9},
	{23,   1920,  1080,  50, false, DEX_RES_1920X1080,   MON_RATIO_16_9},
	{24,   1920,  1080,  60, false, DEX_RES_1920X1080,   MON_RATIO_16_9},

	{25,   1920,  1200,  60, false, DEX_RES_1920X1200,   MON_RATIO_16_10},

	{30,   1920,  1440,  60, false, DEX_RES_NOT_SUPPORT},                  /* CTS 400.3.3.1 */
	{40,   2048,  1536,  60, false, DEX_RES_NOT_SUPPORT},                  /* CTS 18bpp */
	{45,   2400,  1200,  90, false, DEX_RES_NOT_SUPPORT},                  /* relumino */

#ifdef SECDP_WIDE_21_9_SUPPORT
	{60,   2560,  1080,  60, false, DEX_RES_2560X1080,   MON_RATIO_21_9},
#endif

	{61,   2560,  1440,  60, false, DEX_RES_2560X1440,   MON_RATIO_16_9},
	{62,   2560,  1600,  60, false, DEX_RES_2560X1600,   MON_RATIO_16_10},

	{70,   1440,  2560,  60, false, DEX_RES_NOT_SUPPORT},                  /* TVR test */
	{71,   1440,  2560,  75, false, DEX_RES_NOT_SUPPORT},                  /* TVR test */

#ifdef SECDP_SUPPORT_ODYSSEY
	{73,   2880,  1600,  60, false, DEX_RES_NOT_SUPPORT},
	{74,   2880,  1600,  90, false, DEX_RES_NOT_SUPPORT},
#endif

#ifdef SECDP_WIDE_21_9_SUPPORT
	{80,   3440,  1440,  50, false, DEX_RES_3440X1440,   MON_RATIO_21_9},
	{81,   3440,  1440,  60, false, DEX_RES_3440X1440,   MON_RATIO_21_9},
#ifdef SECDP_HIGH_REFRESH_SUPPORT
	{82,   3440,  1440,	100, false, DEX_RES_NOT_SUPPORT, MON_RATIO_21_9},
#endif
#endif

#ifdef SECDP_WIDE_32_9_SUPPORT
	{100,  3840, 1080,   60, false, DEX_RES_NOT_SUPPORT, MON_RATIO_32_9},
#ifdef SECDP_HIGH_REFRESH_SUPPORT
	{101,  3840,  1080, 100, false, DEX_RES_NOT_SUPPORT, MON_RATIO_32_9},
	{102,  3840,  1080, 120, false, DEX_RES_NOT_SUPPORT, MON_RATIO_32_9},
	{104,  3840,  1080, 144, false, DEX_RES_NOT_SUPPORT, MON_RATIO_32_9},
#endif
#endif

#ifdef SECDP_WIDE_32_10_SUPPORT
	{110,  3840,  1200,	 60, false, DEX_RES_NOT_SUPPORT, MON_RATIO_32_10},
#ifdef SECDP_HIGH_REFRESH_SUPPORT
	{111,  3840,  1200, 100, false, DEX_RES_NOT_SUPPORT, MON_RATIO_32_10},
	{112,  3840,  1200, 120, false, DEX_RES_NOT_SUPPORT, MON_RATIO_32_10},
#endif
#endif

	{150,  3840,  2160,  24, false, DEX_RES_NOT_SUPPORT, MON_RATIO_16_9},
	{151,  3840,  2160,  25, false, DEX_RES_NOT_SUPPORT, MON_RATIO_16_9},
	{152,  3840,  2160,  30, false, DEX_RES_NOT_SUPPORT, MON_RATIO_16_9},
	{153,  3840,  2160,  50, false, DEX_RES_NOT_SUPPORT, MON_RATIO_16_9},
	{154,  3840,  2160,  60, false, DEX_RES_NOT_SUPPORT, MON_RATIO_16_9},

	{200,  4096,  2160,  24, false, DEX_RES_NOT_SUPPORT},
	{201,  4096,  2160,  25, false, DEX_RES_NOT_SUPPORT},
	{202,  4096,  2160,  30, false, DEX_RES_NOT_SUPPORT},
	{203,  4096,  2160,  50, false, DEX_RES_NOT_SUPPORT},
	{204,  4096,  2160,  60, false, DEX_RES_NOT_SUPPORT},
};

bool secdp_check_dex_reconnect(void)
{
	pr_info("%d, %d\n", g_max_res_index, g_dex_max_res_index);
	if (g_max_res_index == g_dex_max_res_index)
		return false;

	return true;
}

static inline char *secdp_aspect_ratio_to_string(enum mon_aspect_ratio_t ratio)
{
	switch (ratio) {
	case MON_RATIO_16_9:    return DP_ENUM_STR(MON_RATIO_16_9);
	case MON_RATIO_16_10:   return DP_ENUM_STR(MON_RATIO_16_10);
	case MON_RATIO_21_9:    return DP_ENUM_STR(MON_RATIO_21_9);
	case MON_RATIO_32_9:    return DP_ENUM_STR(MON_RATIO_32_9);
	case MON_RATIO_32_10:   return DP_ENUM_STR(MON_RATIO_32_10);
	case MON_RATIO_NA:      return DP_ENUM_STR(MON_RATIO_NA);
	default:                return "unknown";
	}
}

static enum mon_aspect_ratio_t secdp_get_aspect_ratio(struct drm_display_mode *mode)
{
	enum mon_aspect_ratio_t aspect_ratio = MON_RATIO_NA;
	int hdisplay = mode->hdisplay;
	int vdisplay = mode->vdisplay;

	if ((hdisplay == 3840 && vdisplay == 2160) ||
		(hdisplay == 2560 && vdisplay == 1440) ||
		(hdisplay == 1920 && vdisplay == 1080) ||
		(hdisplay == 1600 && vdisplay == 900) ||
		(hdisplay == 1366 && vdisplay == 768)  ||
		(hdisplay == 1280 && vdisplay == 720))
		aspect_ratio = MON_RATIO_16_9;
	else if ((hdisplay == 2560 && vdisplay == 1600) ||
		(hdisplay == 1920 && vdisplay == 1200) ||
		(hdisplay == 1680 && vdisplay == 1050) ||
		(hdisplay == 1440 && vdisplay == 900)  ||
		(hdisplay == 1280 && vdisplay == 800))
		aspect_ratio = MON_RATIO_16_10;
	else if ((hdisplay == 3440 && vdisplay == 1440) ||
		(hdisplay == 2560 && vdisplay == 1080))
		aspect_ratio = MON_RATIO_21_9;
	else if (hdisplay == 3840 && vdisplay == 1080)
		aspect_ratio = MON_RATIO_32_9;
	else if (hdisplay == 3840 && vdisplay == 1200)
		aspect_ratio = MON_RATIO_32_10;

	return aspect_ratio;
}

#ifdef SECDP_USE_MST
/* check if given resolution is bigger than MST max resolution */
static bool secdp_check_mst_max_resolution(struct drm_display_mode *mode)
{
#define MST_MAX_COLS	2560
#define MST_MAX_ROWS	1440
#define MST_MAX_FPS		60

	bool ret = false;

	if (secdp_is_mst_receiver() == SECDP_ADT_SST)
		goto end;

	if (mode->hdisplay > MST_MAX_COLS || mode->vdisplay > MST_MAX_ROWS ||
		mode->vrefresh > MST_MAX_FPS) {
		/* bigger than MST max resolution */
		ret = true;
		goto end;
	}

end:
	return ret;
}
#endif

bool secdp_check_supported_resolution(struct drm_display_mode *mode, bool supported)
{
	int i, fps_diff;
	int res_cnt = ARRAY_SIZE(secdp_supported_resolution);
	bool interlaced = !!(mode->flags & DRM_MODE_FLAG_INTERLACE);
	static enum mon_aspect_ratio_t prefer_ratio;

	if (mode->type & DRM_MODE_TYPE_PREFERRED) {
		prefer_ratio = secdp_get_aspect_ratio(mode);
		pr_info("preferred_timing! %dx%d@%dhz, %s\n",
			mode->hdisplay, mode->vdisplay, mode->vrefresh,
			secdp_aspect_ratio_to_string(prefer_ratio));
		pr_info("max resolution - mirror: %d, dex: %d\n", g_max_res_index, g_dex_max_res_index);  
		if (g_max_res_index >= 0) {
			if (g_dex_max_res_index < 10) /* less than 1600 x 900 */
			 	g_ignore_ratio = 1;
#ifndef SECDP_USE_PREFERRED
			mode->type = mode->type & (unsigned int)(~DRM_MODE_TYPE_PREFERRED);
#endif
		}
	}
	
	if (!supported)
		return false;

	for (i = 0; i < res_cnt; i++) {
		bool ret = false;
		fps_diff = secdp_supported_resolution[i].refresh_rate - drm_mode_vrefresh(mode);
		fps_diff = fps_diff < 0 ? fps_diff * (-1) : fps_diff;

		if (fps_diff > 1)
			continue;

		if (secdp_supported_resolution[i].interlaced != interlaced)
			continue;

		if (secdp_supported_resolution[i].active_h != mode->hdisplay)
			continue;

		if (secdp_supported_resolution[i].active_v != mode->vdisplay)
			continue;

#ifdef SECDP_USE_MST
		if (secdp_check_mst_max_resolution(mode))
			continue;
#endif

		/* find max resolution which supported by sink */
		if (g_max_res_index < secdp_supported_resolution[i].index)
			g_max_res_index = secdp_supported_resolution[i].index;

		if (secdp_supported_resolution[i].dex_res != DEX_RES_NOT_SUPPORT &&
				secdp_supported_resolution[i].dex_res <= secdp_get_dex_res()) {

#if 0/*TEST*/
			if (secdp_supported_resolution[i].dex_res == DEX_RES_3440X1440) {
				pr_debug("[TEST] RETURN FALSE 3440x1440!!\n");
				ret = false;
			} else if (secdp_supported_resolution[i].dex_res == DEX_RES_2560X1080) {
				pr_debug("[TEST] RETURN FALSE 2560x1080!!\n");
				ret = false;
			} else
#endif
			if (g_ignore_ratio)
				ret = true;
			else if (secdp_supported_resolution[i].mon_ratio == prefer_ratio)
				ret = true;
			else
				ret = false;
		} else
			ret = false;

		/* find max resolution which supported by dex station */
		if (ret && g_dex_max_res_index < secdp_supported_resolution[i].index)
			g_dex_max_res_index = secdp_supported_resolution[i].index;

		if (secdp_check_dex_mode())
			return ret;

		return true;
	}

	return false;
}
#endif

enum drm_mode_status dp_connector_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode, void *display,
		const struct msm_resource_caps_info *avail_res)
{
	struct dp_display *dp_disp;
	struct sde_connector *sde_conn;

	if (!mode || !display || !connector) {
		DP_ERR("invalid params\n");
		return MODE_ERROR;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid dp panel\n");
		return MODE_ERROR;
	}

	dp_disp = display;
	mode->vrefresh = drm_mode_vrefresh(mode);

	return dp_disp->validate_mode(dp_disp, sde_conn->drv_panel,
			mode, avail_res);
}

int dp_connector_update_pps(struct drm_connector *connector,
		char *pps_cmd, void *display)
{
	struct dp_display *dp_disp;
	struct sde_connector *sde_conn;

	if (!display || !connector) {
		DP_ERR("invalid params\n");
		return -EINVAL;
	}

	sde_conn = to_sde_connector(connector);
	if (!sde_conn->drv_panel) {
		DP_ERR("invalid dp panel\n");
		return MODE_ERROR;
	}

	dp_disp = display;
	return dp_disp->update_pps(dp_disp, connector, pps_cmd);
}
