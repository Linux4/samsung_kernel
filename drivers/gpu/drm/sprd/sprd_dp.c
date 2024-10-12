// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_graph.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>
#include <linux/pm_runtime.h>
#include <linux/usb/typec_dp.h>

#include "disp_lib.h"
#include "sprd_drm.h"
#include "sprd_dpu1.h"
#include "sprd_dp.h"
#include "sysfs/sysfs_display.h"
#include "dp/dw_dptx.h"

#define encoder_to_dp(encoder) \
	container_of(encoder, struct sprd_dp, encoder)
#define connector_to_dp(connector) \
	container_of(connector, struct sprd_dp, connector)

#define drm_connector_update_edid_property(connector, edid) \
	drm_mode_connector_update_edid_property(connector, edid)

LIST_HEAD(dp_glb_head);

BLOCKING_NOTIFIER_HEAD(sprd_dp_notifier_list);

int sprd_dp_notifier_call_chain(void *data)
{
	return blocking_notifier_call_chain(&sprd_dp_notifier_list, 0, data);
}

static int sprd_dp_resume(struct sprd_dp *dp)
{
	if (dp->glb && dp->glb->enable)
		dp->glb->enable(&dp->ctx);

	DRM_INFO("dp resume OK\n");
	return 0;
}

static int sprd_dp_suspend(struct sprd_dp *dp)
{
	if (dp->glb && dp->glb->disable)
		dp->glb->disable(&dp->ctx);

	DRM_INFO("dp suspend OK\n");
	return 0;
}

static int sprd_dp_detect(struct sprd_dp *dp, int enable)
{
	if (dp->glb && dp->glb->detect)
		dp->glb->detect(&dp->ctx, enable);

	DRM_INFO("dp detect\n");
	return 0;
}

static int sprd_dp_notify_callback(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct sprd_dp *dp = container_of(nb, struct sprd_dp, dp_nb);
	enum dp_hpd_status *hpd_status = data;

	if (*hpd_status ==  DP_HOT_PLUG &&
		dp->hpd_status == false) {
		pm_runtime_get_sync(dp->dev.parent);
		sprd_dp_resume(dp);
		sprd_dp_detect(dp, 1);
		dp->hpd_status = true;
	} else if (dp->hpd_status == true &&
		(*hpd_status ==  DP_HOT_UNPLUG ||
		*hpd_status == DP_TYPE_DISCONNECT)) {
		sprd_dp_detect(dp, 0);
		sprd_dp_suspend(dp);
		pm_runtime_put(dp->dev.parent);
		dp->hpd_status = false;
	}

	DRM_INFO("%s() hpd_status:%d\n", __func__, *hpd_status);
	return NOTIFY_OK;
}

static void sprd_dp_timing_set(struct sprd_dp *dp)
{
	dptx_video_ts_calculate(dp->snps_dptx,
				dp->snps_dptx->link.lanes,
				dp->snps_dptx->link.rate,
				dp->snps_dptx->vparams.bpc,
				dp->snps_dptx->vparams.pix_enc,
				dp->snps_dptx->vparams.mdtd.pixel_clock);

	dptx_video_reset(dp->snps_dptx, 1, 0);
	dptx_video_reset(dp->snps_dptx, 0, 0);
	dptx_video_timing_change(dp->snps_dptx, 0);

	/* enable VSC if YCBCR420 is enabled */
	if (dp->snps_dptx->vparams.pix_enc == YCBCR420)
		dptx_vsc_ycbcr420_send(dp->snps_dptx, 1);

	DRM_INFO("%s() set mode %dx%d\n", __func__,
		dp->ctx.vm.hactive, dp->ctx.vm.vactive);
}

static void sprd_dp_encoder_enable(struct drm_encoder *encoder)
{

	struct sprd_dp *dp = encoder_to_dp(encoder);
	struct sprd_dpu *dpu = crtc_to_dpu(encoder->crtc);

	DRM_INFO("%s()\n", __func__);

	if (dp->ctx.enabled) {
		DRM_WARN("dp has already been enabled\n");
		return;
	}

	pm_runtime_get_sync(dp->dev.parent);

	sprd_dp_resume(dp);

	sprd_dp_timing_set(dp);

	sprd_dpu1_run(dpu);

	dp->ctx.enabled = true;
}

static void sprd_dp_encoder_disable(struct drm_encoder *encoder)
{
	struct sprd_dp *dp = encoder_to_dp(encoder);
	struct sprd_dpu *dpu = crtc_to_dpu(encoder->crtc);

	DRM_INFO("%s()\n", __func__);

	if (!dp->ctx.enabled) {
		DRM_WARN("dp has already been disabled\n");
		return;
	}

	sprd_dpu1_stop(dpu);

	sprd_dp_suspend(dp);

	pm_runtime_put(dp->dev.parent);

	dp->ctx.enabled = false;
}

static void sprd_dp_encoder_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	struct sprd_dp *dp = encoder_to_dp(encoder);
	struct sprd_dpu *dpu = crtc_to_dpu(encoder->crtc);
	struct drm_display_info *info = &dp->connector.display_info;
	struct video_params *vparams = &dp->snps_dptx->vparams;

	dptx_timing_cfg(dp->snps_dptx, mode, info);

	if (dpu->ctx.bypass_mode) {
		switch (dpu->crtc.primary->state->fb->format->format) {
		case DRM_FORMAT_NV12:
			vparams->bpc = COLOR_DEPTH_8;
			vparams->pix_enc = YCBCR420;
			break;
		case DRM_FORMAT_P010:
			vparams->bpc = COLOR_DEPTH_10;
			vparams->pix_enc = YCBCR420;
			break;
		default:
			vparams->bpc = COLOR_DEPTH_8;
			vparams->pix_enc = RGB;
			break;
		}
	} else {
		/* TODO not support cts test */
		vparams->bpc = COLOR_DEPTH_8;
		vparams->pix_enc = RGB;
	}

	drm_display_mode_to_videomode(mode, &dp->ctx.vm);

	DRM_INFO("%s() set mode: %s\n", __func__, mode->name);
}

static int sprd_dp_encoder_atomic_check(struct drm_encoder *encoder,
					struct drm_crtc_state *crtc_state,
					struct drm_connector_state *conn_state)
{
	DRM_INFO("%s()\n", __func__);

	return 0;
}

static const struct drm_encoder_helper_funcs sprd_encoder_helper_funcs = {
	.atomic_check = sprd_dp_encoder_atomic_check,
	.mode_set = sprd_dp_encoder_mode_set,
	.enable = sprd_dp_encoder_enable,
	.disable = sprd_dp_encoder_disable,
};

static const struct drm_encoder_funcs sprd_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int sprd_dp_encoder_init(struct drm_device *drm,
			       struct sprd_dp *dp)
{
	struct drm_encoder *encoder = &dp->encoder;
	int ret;

	ret = drm_encoder_init(drm, encoder, &sprd_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, NULL);

	if (ret) {
		DRM_ERROR("failed to init dp encoder\n");
		return ret;
	}

	/*TODO SPRD_DISPLAY_TYPE_DP */
	encoder->possible_crtcs = 2;

	drm_encoder_helper_add(encoder, &sprd_encoder_helper_funcs);

	return 0;
}

static int sprd_dp_connector_get_modes(struct drm_connector *connector)
{
	struct sprd_dp *dp = connector_to_dp(connector);
	struct edid *edid;
	int num_modes = 0;

	DRM_INFO("%s()\n", __func__);

	edid = drm_get_edid(connector, &dp->snps_dptx->aux_dev.ddc);
	if (edid) {
		dp->sink_has_audio = drm_detect_monitor_audio(edid);
		drm_connector_update_edid_property(&dp->connector, edid);
		num_modes += drm_add_edid_modes(&dp->connector, edid);
		drm_edid_to_eld(connector, edid);
		memcpy(&dp->edid_info, edid, sizeof(*edid));
		kfree(edid);
	} else
		DRM_ERROR("%s() no edid\n", __func__);

	return num_modes;
}

static enum drm_mode_status
sprd_dp_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	struct sprd_dp *dp = connector_to_dp(connector);
	struct drm_display_info *info = &dp->connector.display_info;
	int vic;

	vic = drm_match_cea_mode(mode);

	if (mode->type & DRM_MODE_TYPE_PREFERRED)
		mode->type &= ~DRM_MODE_TYPE_PREFERRED;

	/* 1920x1080@60Hz is used by default */
	if (vic == 16 && mode->clock == 148500) {
		DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__,
			 DRM_MODE_ARG(mode));
		mode->type |= DRM_MODE_TYPE_PREFERRED;
	}

	/* 3840x2160@60Hz yuv420 bypass */
	if (vic == 97 && mode->clock == 594000 &&
		(info->color_formats & DRM_COLOR_FORMAT_YCRCB420)) {
		DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__,
			 DRM_MODE_ARG(mode));
		mode->type |= DRM_MODE_TYPE_USERDEF;
	}

	/* 640x480@60Hz is used for cts test */
	if (vic == 1 && mode->clock == 25175) {
		DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__,
			 DRM_MODE_ARG(mode));
		mode->type |= DRM_MODE_TYPE_USERDEF;
	}

	return MODE_OK;
}

static struct drm_encoder *
sprd_dp_connector_best_encoder(struct drm_connector *connector)
{
	struct sprd_dp *dp = connector_to_dp(connector);

	DRM_INFO("%s()\n", __func__);
	return &dp->encoder;
}

static int fill_hdr_info_packet(const struct drm_connector_state *state,
				void *out)
{
	struct hdmi_drm_infoframe frame;
	unsigned char buf[30]; /* 26 + 4 */
	ssize_t len;
	int ret;
	u8 *ptr = out;

	memset(out, 0, sizeof(*out));

	ret = drm_hdmi_infoframe_set_hdr_metadata(&frame, state);
	if (ret)
		return ret;

	len = hdmi_drm_infoframe_pack_only(&frame, buf, sizeof(buf));
	if (len < 0)
		return (int)len;

	/* Static metadata is a fixed 26 bytes + 4 byte header. */
	if (len != 30)
		return -EINVAL;

	switch (state->connector->connector_type) {
	case DRM_MODE_CONNECTOR_DisplayPort:
	case DRM_MODE_CONNECTOR_eDP:
		ptr[0] = 0x00; /* sdp id, zero */
		ptr[1] = 0x87; /* type */
		ptr[2] = 0x1D; /* payload len - 1 */
		ptr[3] = (0x13 << 2); /* sdp version */
		ptr[4] = 0x01; /* version */
		ptr[5] = 0x1A; /* length */
		break;
	default:
		return -EINVAL;
	}

	memcpy(&ptr[6], &buf[4], 26);

	return 0;
}

static bool
is_hdr_metadata_different(const struct drm_connector_state *old_state,
			  const struct drm_connector_state *new_state)
{
	struct drm_property_blob *old_blob = old_state->hdr_output_metadata;
	struct drm_property_blob *new_blob = new_state->hdr_output_metadata;

	if (old_blob != new_blob) {
		if (old_blob && new_blob &&
		    old_blob->length == new_blob->length)
			return memcmp(old_blob->data, new_blob->data,
				      old_blob->length);
		return true;
	}

	return false;
}

static int sprd_dp_connector_atomic_check(struct drm_connector *connector,
				 struct drm_atomic_state *state)
{
	struct drm_connector_state *new_con_state =
		drm_atomic_get_new_connector_state(state, connector);
	struct drm_connector_state *old_con_state =
		drm_atomic_get_old_connector_state(state, connector);
	struct sprd_dp *dp = connector_to_dp(connector);
	struct sprd_dpu *dpu = crtc_to_dpu(dp->encoder.crtc);
	int ret;

	if (is_hdr_metadata_different(old_con_state, new_con_state)) {
		ret = fill_hdr_info_packet(new_con_state,
				dpu->ctx.hdr_static_metadata);
		if (ret)
			return ret;
		dpu->ctx.static_metadata_changed = true;
	}

	return 0;
}

static const struct drm_connector_helper_funcs sprd_dp_connector_helper_funcs = {
	.get_modes = sprd_dp_connector_get_modes,
	.mode_valid = sprd_dp_connector_mode_valid,
	.best_encoder = sprd_dp_connector_best_encoder,
	.atomic_check = sprd_dp_connector_atomic_check,
};

static enum drm_connector_status
sprd_dp_connector_detect(struct drm_connector *connector, bool force)
{
	struct sprd_dp *dp = connector_to_dp(connector);

	DRM_INFO("%s()\n", __func__);

	if (dp->snps_dptx->link.trained)
		return connector_status_connected;
	else
		return connector_status_disconnected;
}

static void sprd_dp_connector_destroy(struct drm_connector *connector)
{
	DRM_INFO("%s()\n", __func__);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static int sprd_dp_atomic_get_property(struct drm_connector *connector,
					const struct drm_connector_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct sprd_dp *dp = connector_to_dp(connector);

	DRM_DEBUG("%s()\n", __func__);

	if (property == dp->edid_prop) {
		memcpy(dp->edid_blob->data, &dp->edid_info, sizeof(struct edid));
		*val = dp->edid_blob->base.id;
		DRM_INFO("%s() val = %d\n", __func__, dp->edid_blob->base.id);
	} else {
		DRM_ERROR("property %s is invalid\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static const struct drm_connector_funcs sprd_dp_atomic_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = sprd_dp_connector_detect,
	.destroy = sprd_dp_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.atomic_get_property = sprd_dp_atomic_get_property,
};

static int sprd_dp_connector_init(struct drm_device *drm, struct sprd_dp *dp)
{
	struct drm_encoder *encoder = &dp->encoder;
	struct drm_connector *connector = &dp->connector;
	struct drm_property *prop;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(drm, connector,
				 &sprd_dp_atomic_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		DRM_ERROR("drm_connector_init() failed\n");
		return ret;
	}

	drm_object_attach_property(
			&connector->base,
			drm->mode_config.hdr_output_metadata_property, 0);

	dp->edid_blob = drm_property_create_blob(drm, (sizeof(struct edid) + 1), &dp->edid_info);
	if (IS_ERR(dp->edid_blob)) {
		DRM_ERROR("drm_property_create_blob edid blob failed\n");
		return PTR_ERR(dp->edid_blob);
	}

	prop = drm_property_create(drm, DRM_MODE_PROP_BLOB, "EDID INFO", 0);
	if (!prop) {
		DRM_ERROR("drm_property_create dpu version failed\n");
		return -ENOMEM;
	}

	drm_object_attach_property(&connector->base, prop, dp->edid_blob->base.id);
	dp->edid_prop = prop;

	DRM_INFO("dp->edid_blob->base.id:%d\n", dp->edid_blob->base.id);

	drm_connector_helper_add(connector,
				 &sprd_dp_connector_helper_funcs);

	drm_mode_connector_attach_encoder(connector, encoder);

	return 0;
}

static int sprd_dp_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_dp *dp = dev_get_drvdata(dev);
	struct video_params *vparams = NULL;
	int ret;

	ret = sprd_dp_encoder_init(drm, dp);
	if (ret)
		return ret;

	ret = sprd_dp_connector_init(drm, dp);
	if (ret)
		goto cleanup_encoder;

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	sprd_dp_resume(dp);

	dp->snps_dptx = dptx_init(dev, drm);
	if (IS_ERR_OR_NULL(dp->snps_dptx)) {
		ret = PTR_ERR(dp->snps_dptx);
		goto cleanup_connector;
	}

	sprd_dp_suspend(dp);
	pm_runtime_put(dev);

	vparams = &dp->snps_dptx->vparams;

	dp->dp_nb.notifier_call = sprd_dp_notify_callback;
	blocking_notifier_chain_register(&sprd_dp_notifier_list,
						&dp->dp_nb);

	return 0;

cleanup_connector:
	drm_connector_cleanup(&dp->connector);
cleanup_encoder:
	drm_encoder_cleanup(&dp->encoder);
	return ret;
}

static void sprd_dp_unbind(struct device *dev,
			struct device *master, void *data)
{
	struct sprd_dp *dp = dev_get_drvdata(dev);
	struct dptx *dptx = dp->snps_dptx;

	dptx_core_deinit(dptx);
}

static const struct component_ops dp_component_ops = {
	.bind = sprd_dp_bind,
	.unbind = sprd_dp_unbind,
};

static int sprd_dp_device_create(struct sprd_dp *dp,
				struct device *parent)
{
	int ret;

	dp->dev.class = display_class;
	dp->dev.parent = parent;
	dp->dev.of_node = parent->of_node;
	dev_set_name(&dp->dev, "dp");
	dev_set_drvdata(&dp->dev, dp);

	ret = device_register(&dp->dev);
	if (ret) {
		DRM_ERROR("dp device register failed\n");
		return ret;
	}

	return 0;
}

static unsigned char kEdid1[128] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x1c, 0xec, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x1b, 0x10, 0x01, 0x03, 0x80, 0x50, 0x2d, 0x78,
	0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27, 0x12, 0x48, 0x4c, 0x00,
	0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38,
	0x2d, 0x40, 0x58, 0x2c, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xfc, 0x00, 0x45, 0x4d, 0x55, 0x5f, 0x64, 0x69, 0x73,
	0x70, 0x6c, 0x61, 0x79, 0x5f, 0x31, 0x00, 0x3b
};

static void sprd_edid_set_default_prop(struct edid *edid_info)
{
	memcpy(edid_info, kEdid1, sizeof(struct edid));
}
static int sprd_dp_context_init(struct sprd_dp *dp, struct device_node *np)
{
	struct dp_context *ctx = &dp->ctx;
	struct resource r;

	if (dp->glb && dp->glb->parse_dt)
		dp->glb->parse_dt(&dp->ctx, np);

	if (of_address_to_resource(np, 0, &r)) {
		DRM_ERROR("parse dp ctrl reg base failed\n");
		return -ENODEV;
	}
	ctx->base = ioremap_nocache(r.start, resource_size(&r));
	if (ctx->base == NULL) {
		DRM_ERROR("dp ctrl reg base ioremap failed\n");
		return -ENODEV;
	}

	sprd_edid_set_default_prop(&dp->edid_info);
	return 0;
}

static const struct of_device_id dp_match_table[] = {
	{.compatible = "sprd,dptx"},
	{ }
};

static int sprd_dp_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sprd_dp *dp;
	const char *str;
	int ret;

	dp = devm_kzalloc(&pdev->dev, sizeof(*dp), GFP_KERNEL);
	if (!dp) {
		DRM_ERROR("failed to allocate dp data.\n");
		return -ENOMEM;
	}

	if (!of_property_read_string(np, "sprd,soc", &str))
		dp->glb = dp_glb_ops_attach(str);
	else
		DRM_WARN("error: 'sprd,soc' was not found\n");

	ret = sprd_dp_context_init(dp, np);
	if (ret)
		return ret;

	sprd_dp_device_create(dp, &pdev->dev);
	platform_set_drvdata(pdev, dp);

	sprd_dp_audio_codec_init(&pdev->dev);

	return component_add(&pdev->dev, &dp_component_ops);
}

static int sprd_dp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dp_component_ops);

	return 0;
}

static struct platform_driver sprd_dp_driver = {
	.probe = sprd_dp_probe,
	.remove = sprd_dp_remove,
	.driver = {
		   .name = "sprd-dp-drv",
		   .of_match_table = dp_match_table,
	},
};

module_platform_driver(sprd_dp_driver);

MODULE_AUTHOR("Chen He <chen.he@unisoc.com>");
MODULE_DESCRIPTION("Unisoc DPTX Controller Driver");
MODULE_LICENSE("GPL v2");
