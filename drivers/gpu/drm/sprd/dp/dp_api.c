// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "dp_api.h"

#define DEFAULT_STREAM 0

/**
 * dptx_get_video_format() - Get current video format
 * @dptx: The dptx struct
 *
 * Returns video format.
 * 0 - CEA
 * 1 - CVT
 * 2 - DMT
 */
u8 dptx_get_video_format(struct dptx *dptx)
{
	return dptx->vparams.video_format;
}

/**
 * dptx_set_video_format() - Set video format
 * @dptx: The dptx struct
 * @video_format: video format
 * Possible options: 0 - CEA, 1 - CVT, 2 - DMT
 *
 * Returns 0 on success otherwise negative errno.
 */
int dptx_set_video_format(struct dptx *dptx, u8 format)
{
	if (format > DMT) {
		DRM_DEBUG("%s: Invalid video format value %d\n",
			 __func__, format);
		return -EINVAL;
	}

	dptx->vparams.video_format = format;
	DRM_DEBUG("%s: Change video format to %d\n",
		 __func__, format);
	return 0;
}

/**
 * dptx_get_link_lane_count() - Get link lane count
 * Possible options: 1, 2 or 4
 * @dptx: The dptx struct
 *
 * Returns the number of lanes on success otherwise negative errno.
 */
u8 dptx_get_link_lane_count(struct dptx *dptx)
{
	u32 hpdsts;

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);
	if (!(hpdsts & DPTX_HPDSTS_STATUS)) {
		DRM_DEBUG("%s: Not connected\n", __func__);
		return -1;
	}
	return dptx->link.lanes;
}

/**
 * dptx_get_link_rate() - Get link rate
 * Possible options: 0 - RBR, 1 - HBR, 2 - HBR2, 3 - HBR3
 * @dptx: The dptx struct
 *
 * Returns link rate on success otherwise negative errno.
 */
u8 dptx_get_link_rate(struct dptx *dptx)
{
	u32 hpdsts;

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);
	if (!(hpdsts & DPTX_HPDSTS_STATUS)) {
		DRM_DEBUG("%s: Not connected\n", __func__);
		return -1;
	}
	return dptx->link.rate;
}

/**
 * dptx_get_video_dynamic_range() - Get video dynamic range
 * @dptx: The dptx struct
 *
 * Returns video dynamic range
 * 1 - CEA, 2 - VESA
 */
u8 dptx_get_video_dynamic_range(struct dptx *dptx)
{
	return dptx->vparams.dynamic_range;
}

/**
 * dptx_set_video_dynamic_range() - Set video dynamic range
 * @dptx: The dptx struct
 * @dynamic_range: video dynamic range
 * Possible options: 1 - CEA, 2 - VESA
 *
 * Returns 0 on success otherwise negative errno.
 */
int dptx_set_video_dynamic_range(struct dptx *dptx, u8 dynamic_range)
{
	if (dynamic_range > VESA) {
		DRM_DEBUG("%s: Invalid dynamic range value %d\n",
			 __func__, dynamic_range);
		return -EINVAL;
	}

	dptx->vparams.dynamic_range = dynamic_range;

	dptx_disable_default_video_stream(dptx, DEFAULT_STREAM);
	dptx_video_set_sink_col(dptx, DEFAULT_STREAM);
	dptx_enable_default_video_stream(dptx, DEFAULT_STREAM);
	DRM_DEBUG("%s: Change video dynamic range to %d\n",
		 __func__, dynamic_range);

	return 0;
}

/**
 * dptx_get_video_colorimetry() - Get video colorimetry
 * @dptx: The dptx struct
 *
 * Returns video colorimetry
 * 1 - ITU-R BT.601, 2 - ITU-R BT.709
 */
u8 dptx_get_video_colorimetry(struct dptx *dptx)
{
	return dptx->vparams.colorimetry;
}

/**
 * dptx_set_video_colorimetry() - Set video colorimetry
 * @dptx: The dptx struct
 * @video_col: Video colorimetry
 * Possible options: 1 - ITU-R BT.601, 2 - ITU-R BT.709
 *
 * Returns 0 on success otherwise negative errno.
 */
int dptx_set_video_colorimetry(struct dptx *dptx, u8 video_col)
{
	if (video_col > ITU709) {
		DRM_DEBUG("%s: Invalid video colorimetry value %d\n",
			 __func__, video_col);
		return -EINVAL;
	}

	dptx->vparams.colorimetry = video_col;

	dptx_disable_default_video_stream(dptx, DEFAULT_STREAM);
	dptx_video_set_sink_col(dptx, DEFAULT_STREAM);
	dptx_enable_default_video_stream(dptx, DEFAULT_STREAM);
	DRM_DEBUG("%s: Change video colorimetry to %d\n",
		 __func__, video_col);

	return 0;
}

/**
 * dptx_get_pixel_enc() - Get pixel encoding
 * @dptx: The dptx struct
 *
 * Returns pixel encoding
 * RGB - 0, YCbCR420 - 1, YCbCR422 - 2, YCbCR444 - 3, YOnly - 4, RAW - 5
 */
u8 dptx_get_pixel_enc(struct dptx *dptx)
{
	return dptx->vparams.pix_enc;
}

/**
 * dptx_set_pixel_enc() - Set pixel encoding
 * @dptx: The dptx struct
 * @pix_enc: Video pixel encoding
 * Possible options: RGB - 0, YCbCR420 - 1, YCbCR422 - 2
 *		     YCbCR444 - 3, YOnly - 4, RAW -5
 *
 * Returns 0 on success otherwise negative errno.
 */
int dptx_set_pixel_enc(struct dptx *dptx, u8 pix_enc)
{
	u32 hpdsts;
	int retval;
	struct video_params *vparams;
	struct dtd *mdtd;

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);

	if (!(hpdsts & DPTX_HPDSTS_STATUS)) {
		DRM_DEBUG("%s: Not connected\n", __func__);
		return -ENODEV;
	}
	if (pix_enc > RAW) {
		DRM_DEBUG("%s: Invalid pixel encoding value %d\n",
			 __func__, pix_enc);
		return -EINVAL;
	}
	vparams = &dptx->vparams;
	mdtd = &vparams->mdtd;
	retval = dptx_video_ts_calculate(dptx, dptx->link.lanes,
					 dptx->link.rate, vparams->bpc,
					 pix_enc, mdtd->pixel_clock);
	if (retval)
		return retval;

	vparams->pix_enc = pix_enc;

	dptx_disable_default_video_stream(dptx, DEFAULT_STREAM);
	dptx_video_bpc_change(dptx, DEFAULT_STREAM);
	dptx_video_ts_change(dptx, DEFAULT_STREAM);
	dptx_enable_default_video_stream(dptx, DEFAULT_STREAM);

	if (pix_enc == YCBCR420) {
		dptx_vsc_ycbcr420_send(dptx, 1);
		dptx->ycbcr420 = 1;
	} else {
		dptx_vsc_ycbcr420_send(dptx, 0);
		dptx->ycbcr420 = 0;
	}


	DRM_DEBUG("%s: Change pixel encoding to %d\n",
		 __func__, pix_enc);

	return 0;
}

/**
 * dptx_get_bpc() - Get bits per component
 * @dptx: The dptx struct
 *
 * Returns the bit per componenet value
 */
u8 dptx_get_bpc(struct dptx *dptx)
{
	return dptx->vparams.bpc;
}

/**
 * dptx_set_bpc() - Set bits per component
 * @dptx: The dptx struct
 * @bpc: Bits per component value
 * Possible options: 6, 8, 10, 12, 16
 *
 * Returns 0 on success otherwise negative errno.
 */
int dptx_set_bpc(struct dptx *dptx, u8 bpc)
{
	u32 hpdsts;
	int retval;
	struct video_params *vparams;
	struct dtd *mdtd;

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);
	if (!(hpdsts & DPTX_HPDSTS_STATUS)) {
		DRM_DEBUG("%s: Not connected\n", __func__);
		return -ENODEV;
	}

	if (bpc != COLOR_DEPTH_6 && bpc != COLOR_DEPTH_8 &&
	    bpc != COLOR_DEPTH_10 && bpc != COLOR_DEPTH_12 &&
	    bpc != COLOR_DEPTH_16) {
		DRM_DEBUG("%s: Invalid bits per component value %d\n",
			 __func__, bpc);
		return -EINVAL;
	}

	vparams = &dptx->vparams;
	mdtd = &vparams->mdtd;
	retval = dptx_video_ts_calculate(dptx, dptx->link.lanes,
					 dptx->link.rate, bpc,
					 vparams->pix_enc, mdtd->pixel_clock);
	if (retval)
		return retval;

	vparams->bpc = bpc;
	dptx_disable_default_video_stream(dptx, DEFAULT_STREAM);
	dptx_video_bpc_change(dptx, DEFAULT_STREAM);
	dptx_video_ts_change(dptx, DEFAULT_STREAM);
	dptx_enable_default_video_stream(dptx, DEFAULT_STREAM);

	DRM_DEBUG("%s: Change bits per component to %d\n",
		 __func__, bpc);

	return 0;
}

/**
 * dptx_link_retrain() - Retrain link
 * @dptx: The dptx struct
 * @rate: Link rate - Possible options: 0 - RBR, 1 - HBR, 2 - HBR2, 3 - HBR3
 * @lanes: Link lanes count - Possible options 1,2 or 4
 *
 * Returns 0 on success otherwise negative errno.
 */
int dptx_link_retrain(struct dptx *dptx, u8 rate, u8 lanes)
{
	u32 hpdsts;
	struct video_params *vparams;
	struct dtd *mdtd;
	int retval = 0;

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);

	if (!(hpdsts & DPTX_HPDSTS_STATUS)) {
		DRM_DEBUG("%s: Not connected\n", __func__);
		return -ENODEV;
	}

	if (lanes != 1 && lanes != 2 && lanes != 4) {
		DRM_DEBUG("%s: Invalid number of lanes %d\n",
			 __func__, lanes);
		return -EINVAL;
	}

	if (rate > DPTX_PHYIF_CTRL_RATE_HBR3) {
		DRM_DEBUG("%s: Invalid number of lane rate %d\n",
			 __func__, rate);
		return -EINVAL;
	}
	vparams = &dptx->vparams;
	mdtd = &vparams->mdtd;
	retval = dptx_video_ts_calculate(dptx, lanes, rate, vparams->bpc,
					 vparams->pix_enc, mdtd->pixel_clock);
	if (retval)
		return retval;

	retval = dptx_link_training(dptx, rate, lanes);

	if (retval)
		return retval;

	dptx_video_ts_change(dptx, DEFAULT_STREAM);
	DRM_DEBUG("%s: Retraining link rate=%d lanes=%d\n",
		 __func__, rate, lanes);

	return 0;
}
