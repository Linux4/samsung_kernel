// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "dw_dptx.h"

int dptx_timing_cfg(struct dptx *dptx, const struct drm_display_mode *mode,
		    const struct drm_display_info *info)
{
	struct dtd *mdtd = &dptx->vparams.mdtd;
	int vic;

	mdtd->pixel_clock = mode->clock;
	if (mdtd->pixel_clock < 0x01) {
		DRM_ERROR("%s: pixel_clock error\n", __func__);
		return -EINVAL;
	}

	mdtd->h_active = mode->hdisplay;
	mdtd->h_blanking = mode->htotal - mode->hdisplay;
	mdtd->h_sync_offset = mode->hsync_start - mode->hdisplay;
	mdtd->h_sync_pulse_width = mode->hsync_end - mode->hsync_start;

	mdtd->v_active = mode->vdisplay;
	mdtd->v_blanking = mode->vtotal - mode->vdisplay;
	mdtd->v_sync_offset = mode->vsync_start - mode->vdisplay;
	mdtd->v_sync_pulse_width = mode->vsync_end - mode->vsync_start;

	mdtd->interlaced = !!(mode->flags & DRM_MODE_FLAG_INTERLACE);
	mdtd->v_sync_polarity = !!(mode->flags & DRM_MODE_FLAG_NVSYNC);
	mdtd->h_sync_polarity = !!(mode->flags & DRM_MODE_FLAG_NHSYNC);
	if (mdtd->interlaced == 1)
		mdtd->v_active /= 2;

	DRM_DEBUG("DTD pixel_clock: %d interlaced: %d\n",
		 mdtd->pixel_clock, mdtd->interlaced);
	DRM_DEBUG("h_active: %d h_blanking: %d h_sync_offset: %d\n",
		 mdtd->h_active, mdtd->h_blanking, mdtd->h_sync_offset);
	DRM_DEBUG("h_sync_pulse_width: %d h_image_size: %d h_sync_polarity: %d\n",
		 mdtd->h_sync_pulse_width, mdtd->h_image_size,
		 mdtd->h_sync_polarity);
	DRM_DEBUG("v_active: %d v_blanking: %d v_sync_offset: %d\n",
		 mdtd->v_active, mdtd->v_blanking, mdtd->v_sync_offset);
	DRM_DEBUG("v_sync_pulse_width: %d v_image_size: %d v_sync_polarity: %d\n",
		 mdtd->v_sync_pulse_width, mdtd->v_image_size,
		 mdtd->v_sync_polarity);
	DRM_DEBUG("info->bpc%d\n", info->bpc);

	dptx->vparams.bpc = info->bpc;

	vic = drm_match_cea_mode(mode);
	if ((vic == 6) || (vic == 7) || (vic == 21) || (vic == 22) ||
	    (vic == 2) || (vic == 3) || (vic == 17) || (vic == 18)) {
		dptx->vparams.dynamic_range = CEA;
		dptx->vparams.colorimetry = ITU601;
	} else if (vic) {
		dptx->vparams.dynamic_range = CEA;
		dptx->vparams.colorimetry = ITU709;
	} else {
		dptx->vparams.dynamic_range = VESA;
		dptx->vparams.colorimetry = ITU709;
	}

	if (info->color_formats & DRM_COLOR_FORMAT_YCRCB444)
		dptx->vparams.pix_enc = YCBCR444;
	else if (info->color_formats & DRM_COLOR_FORMAT_YCRCB422)
		dptx->vparams.pix_enc = YCBCR422;
	else if (info->color_formats & DRM_COLOR_FORMAT_YCRCB420)
		dptx->vparams.pix_enc = YCBCR420;
	else
		dptx->vparams.pix_enc = RGB;

	/*TODO*/
	dptx->vparams.pix_enc = RGB;

	return 0;
}

void dptx_disable_sdp(struct dptx *dptx, u32 *payload)
{
	int i;

	for (i = 0; i < DPTX_SDP_NUM; i++)
		if (!memcmp(dptx->sdp_list[i].payload, payload, 9))
			memset(dptx->sdp_list[i].payload, 0, 9);
}

void dptx_enable_sdp(struct dptx *dptx, struct sdp_full_data *data)
{
	int i, reg_num, sdp_offset;
	u32 reg, header;

	reg_num = 0;
	header = cpu_to_be32(data->payload[0]);
	for (i = 0; i < DPTX_SDP_NUM; i++)
		if (dptx->sdp_list[i].payload[0] == 0) {
			dptx->sdp_list[i].payload[0] = header;
			sdp_offset = i * DPTX_SDP_SIZE;
			reg_num = 0;
			while (reg_num < DPTX_SDP_LEN) {
				dptx_writel(dptx, DPTX_SDP_BANK + sdp_offset
					    + reg_num * 4,
					    cpu_to_be32(data->payload[reg_num])
				    );
				reg_num++;
			}
			switch (data->blanking) {
			case 0:
				reg = dptx_readl(dptx, DPTX_SDP_VERTICAL_CTRL);
				reg |= (1 << (2 + i));
				dptx_writel(dptx, DPTX_SDP_VERTICAL_CTRL, reg);
				break;
			case 1:
				reg = dptx_readl(dptx,
						 DPTX_SDP_HORIZONTAL_CTRL);
				reg |= (1 << (2 + i));
				dptx_writel(dptx, DPTX_SDP_HORIZONTAL_CTRL,
					    reg);
				break;
			case 2:
				reg = dptx_readl(dptx, DPTX_SDP_VERTICAL_CTRL);
				reg |= (1 << (2 + i));
				dptx_writel(dptx, DPTX_SDP_VERTICAL_CTRL, reg);
				reg = dptx_readl(dptx,
						 DPTX_SDP_HORIZONTAL_CTRL);
				reg |= (1 << (2 + i));
				dptx_writel(dptx, DPTX_SDP_HORIZONTAL_CTRL,
					    reg);
				break;
			}
			break;
		}
}

void dptx_fill_sdp(struct dptx *dptx, struct sdp_full_data *data)
{
	if (data->en)
		dptx_enable_sdp(dptx, data);
	else
		dptx_disable_sdp(dptx, data->payload);
}

void dptx_vsc_ycbcr420_send(struct dptx *dptx, u8 enable)
{
	struct sdp_full_data vsc_data;
	int i;

	vsc_data.en = enable;
	for (i = 0; i < 9; i++) {
		if (i == 0)
			vsc_data.payload[i] = 0x00070513;
		else if (i == 5)
			switch (dptx->vparams.bpc) {
			case COLOR_DEPTH_8:
				vsc_data.payload[i] = 0x30010000;
				break;
			case COLOR_DEPTH_10:
				vsc_data.payload[i] = 0x30020000;
				break;
			case COLOR_DEPTH_12:
				vsc_data.payload[i] = 0x30030000;
				break;
			case COLOR_DEPTH_16:
				vsc_data.payload[i] = 0x30040000;
				break;
			default:
				DRM_ERROR("Invalid bpc: %d\n", dptx->vparams.bpc);
				vsc_data.payload[i] = 0x0;
				break;
		} else
			vsc_data.payload[i] = 0x0;
	}
	vsc_data.blanking = 0;
	vsc_data.cont = 1;

	dptx_fill_sdp(dptx, &vsc_data);
}

void dptx_video_reset(struct dptx *dptx, int enable, int stream)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_SRST_CTRL);

	if (enable)
		reg |= DPTX_SRST_VIDEO_RESET_N(stream);
	else
		reg &= ~DPTX_SRST_VIDEO_RESET_N(stream);

	dptx_writel(dptx, DPTX_SRST_CTRL, reg);
}

void dptx_video_timing_change(struct dptx *dptx, int stream)
{
	dptx_disable_default_video_stream(dptx, stream);
	dptx_video_core_config(dptx, stream);
	dptx_enable_default_video_stream(dptx, stream);
}

static int dptx_calculate_hblank_interval(struct dptx *dptx)
{
	struct video_params *vparams;
	int pixel_clk;
	u16 h_blank;
	u32 link_clk;
	u8 rate;
	int hblank_interval;

	vparams = &dptx->vparams;
	pixel_clk = vparams->mdtd.pixel_clock;
	h_blank = vparams->mdtd.h_blanking;
	rate = dptx->link.rate;

	switch (rate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		link_clk = 40500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		link_clk = 67500;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		link_clk = 135000;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		link_clk = 202500;
		break;
	default:
		DRM_ERROR("Invalid rate 0x%x\n", rate);
		return -EINVAL;
	}

	hblank_interval = h_blank * link_clk / pixel_clk;

	return hblank_interval;
}

void dptx_video_core_config(struct dptx *dptx, int stream)
{
	u32 reg = 0;

	struct dtd *mdtd = &dptx->vparams.mdtd;

	dptx_video_set_core_bpc(dptx, stream);

	/* Single, dual, or quad pixel */
	reg = dptx_readl(dptx, DPTX_VSAMPLE_CTRL_N(stream));
	reg &= ~DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK;
	reg |= dptx->multipixel << DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT;
	dptx_writel(dptx, DPTX_VSAMPLE_CTRL_N(stream), reg);

	/* Configure DPTX_VSAMPLE_POLARITY_CTRL register */
	reg = 0;

	if (mdtd->h_sync_polarity == 1)
		reg |= DPTX_POL_CTRL_H_SYNC_POL_EN;
	if (mdtd->v_sync_polarity == 1)
		reg |= DPTX_POL_CTRL_V_SYNC_POL_EN;

	dptx_writel(dptx, DPTX_VSAMPLE_POLARITY_CTRL_N(stream), reg);

	reg = 0;
	if (mdtd->interlaced == 1)
		reg |= DPTX_VIDEO_CONFIG1_O_IP_EN;

	reg |= mdtd->h_active << DPTX_VIDEO_H_ACTIVE_SHIFT;
	reg |= mdtd->h_blanking << DPTX_VIDEO_H_BLANK_SHIFT;
	dptx_writel(dptx, DPTX_VIDEO_CONFIG1_N(stream), reg);

	/* Configure video_config2 register */
	reg = 0;
	reg |= mdtd->v_active << DPTX_VIDEO_V_ACTIVE_SHIFT;
	reg |= mdtd->v_blanking << DPTX_VIDEO_V_BLANK_SHIFT;
	dptx_writel(dptx, DPTX_VIDEO_CONFIG2_N(stream), reg);

	/* Configure video_config3 register */
	reg = 0;
	reg |= mdtd->h_sync_offset << DPTX_VIDEO_H_FRONT_PORCH;
	reg |= mdtd->h_sync_pulse_width << DPTX_VIDEO_H_SYNC_WIDTH;

	dptx_writel(dptx, DPTX_VIDEO_CONFIG3_N(stream), reg);

	/* Configure video_config4 register */
	reg = 0;
	reg |= mdtd->v_sync_offset << DPTX_VIDEO_V_FRONT_PORCH;
	reg |= mdtd->v_sync_pulse_width << DPTX_VIDEO_V_SYNC_WIDTH;
	dptx_writel(dptx, DPTX_VIDEO_CONFIG4_N(stream), reg);

	/* Configure video_config5 register */
	dptx_video_ts_change(dptx, stream);

	/* Configure video_msa1 register */
	reg = 0;
	reg |= (mdtd->h_blanking - mdtd->h_sync_offset)
	    << DPTX_VIDEO_MSA1_H_START_SHIFT;
	reg |= (mdtd->v_blanking - mdtd->v_sync_offset)
	    << DPTX_VIDEO_MSA1_V_START_SHIFT;
	dptx_writel(dptx, DPTX_VIDEO_MSA1_N(stream), reg);

	dptx_video_set_sink_bpc(dptx, stream);

	reg = dptx_calculate_hblank_interval(dptx);
	reg |=
	    (DPTX_VIDEO_HBLANK_INTERVAL_ENABLE <<
	     DPTX_VIDEO_HBLANK_INTERVAL_SHIFT);
	dptx_writel(dptx, DPTX_VIDEO_HBLANK_INTERVAL, reg);
}

int dptx_video_ts_calculate(struct dptx *dptx, int lane_num, int rate,
			    int bpc, int encoding, int pixel_clock)
{
	struct video_params *vparams;
	struct dtd *mdtd;
	int link_rate;
	int retval = 0;
	int ts;
	int tu;
	int tu_frac;
	int color_dep;

	vparams = &dptx->vparams;
	mdtd = &vparams->mdtd;

	switch (rate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		link_rate = 162;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		link_rate = 270;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		link_rate = 540;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		link_rate = 810;
		break;
	default:
		link_rate = 162;
	}

	switch (bpc) {
	case COLOR_DEPTH_6:
		color_dep = 18;
		break;
	case COLOR_DEPTH_8:
		if (encoding == YCBCR420)
			color_dep = 12;
		else if (encoding == YCBCR422)
			color_dep = 16;
		else if (encoding == YONLY)
			color_dep = 8;
		else
			color_dep = 24;
		break;
	case COLOR_DEPTH_10:
		if (encoding == YCBCR420)
			color_dep = 15;
		else if (encoding == YCBCR422)
			color_dep = 20;
		else if (encoding == YONLY)
			color_dep = 10;
		else
			color_dep = 30;
		break;
	default:
		color_dep = 18;
		DRM_ERROR("%s: bpc error %d", __func__, bpc);
		break;
	}
	ts = (8 * color_dep * pixel_clock) / (lane_num * link_rate);
	tu = ts / 1000;
	if (tu >= 65) {
		DRM_INFO("%s: tu(%d) > 65", __func__, tu);
		return -EINVAL;
	}

	tu_frac = ts / 100 - tu * 10;
	if (tu < 6) {
		vparams->init_threshold = 32;
	} else if ((encoding == RGB || encoding == YCBCR444) &&
		   mdtd->h_blanking <= 80) {
		if (dptx->multipixel == DPTX_MP_QUAD_PIXEL)
			vparams->init_threshold = 4;
		else
			vparams->init_threshold = 12;
	} else {
		vparams->init_threshold = 16;
	}

	DRM_INFO("tu = %d, tu_frac = %d\n", tu, tu_frac);

	vparams->aver_bytes_per_tu = tu;
	vparams->aver_bytes_per_tu_frac = tu_frac;

	return retval;
}

void dptx_video_ts_change(struct dptx *dptx, int stream)
{
	u32 reg;
	struct video_params *vparams;

	vparams = &dptx->vparams;

	reg = dptx_readl(dptx, DPTX_VIDEO_CONFIG5_N(stream));
	reg = reg & (~DPTX_VIDEO_CONFIG5_TU_MASK);
	reg = reg | (vparams->aver_bytes_per_tu << DPTX_VIDEO_CONFIG5_TU_SHIFT);
	reg = reg & (~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_SST);
	reg = reg | (vparams->aver_bytes_per_tu_frac <<
		     DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_SST);
	reg = reg & (~DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_MASK);
	reg = reg | (vparams->init_threshold <<
		     DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT);
	dptx_writel(dptx, DPTX_VIDEO_CONFIG5_N(stream), reg);
}

void dptx_video_bpc_change(struct dptx *dptx, int stream)
{
	dptx_video_set_core_bpc(dptx, stream);
	dptx_video_set_sink_bpc(dptx, stream);
}

void dptx_video_set_core_bpc(struct dptx *dptx, int stream)
{
	u32 reg;
	u8 bpc_mapping = 0, bpc = 0;
	enum pixel_enc_type pix_enc;

	bpc = dptx->vparams.bpc;
	pix_enc = dptx->vparams.pix_enc;

	reg = dptx_readl(dptx, DPTX_VSAMPLE_CTRL_N(stream));
	reg &= ~DPTX_VSAMPLE_CTRL_VMAP_BPC_MASK;

	switch (pix_enc) {
	case RGB:
		if (bpc == COLOR_DEPTH_6)
			bpc_mapping = 0;
		else if (bpc == COLOR_DEPTH_8)
			bpc_mapping = 1;
		else if (bpc == COLOR_DEPTH_10)
			bpc_mapping = 2;
		else if (bpc == COLOR_DEPTH_12)
			bpc_mapping = 3;
		if (bpc == COLOR_DEPTH_16)
			bpc_mapping = 4;
		break;
	case YCBCR444:
		if (bpc == COLOR_DEPTH_8)
			bpc_mapping = 5;
		else if (bpc == COLOR_DEPTH_10)
			bpc_mapping = 6;
		else if (bpc == COLOR_DEPTH_12)
			bpc_mapping = 7;
		if (bpc == COLOR_DEPTH_16)
			bpc_mapping = 8;
		break;
	case YCBCR422:
		if (bpc == COLOR_DEPTH_8)
			bpc_mapping = 9;
		else if (bpc == COLOR_DEPTH_10)
			bpc_mapping = 10;
		else if (bpc == COLOR_DEPTH_12)
			bpc_mapping = 11;
		if (bpc == COLOR_DEPTH_16)
			bpc_mapping = 12;
		break;
	case YCBCR420:
		if (bpc == COLOR_DEPTH_8)
			bpc_mapping = 13;
		else if (bpc == COLOR_DEPTH_10)
			bpc_mapping = 14;
		else if (bpc == COLOR_DEPTH_12)
			bpc_mapping = 15;
		if (bpc == COLOR_DEPTH_16)
			bpc_mapping = 16;
		break;
	default:
		bpc_mapping = 1;
		DRM_ERROR("%s: pix_enc error %d", __func__, pix_enc);
		break;
	}

	reg |= (bpc_mapping << DPTX_VSAMPLE_CTRL_VMAP_BPC_SHIFT);
	dptx_writel(dptx, DPTX_VSAMPLE_CTRL_N(stream), reg);
}

void dptx_video_set_sink_col(struct dptx *dptx, int stream)
{
	u32 reg_msa2;
	u8 col_mapping;
	u8 colorimetry;
	u8 dynamic_range;
	struct video_params *vparams;
	enum pixel_enc_type pix_enc;

	vparams = &dptx->vparams;
	pix_enc = vparams->pix_enc;
	colorimetry = vparams->colorimetry;
	dynamic_range = vparams->dynamic_range;

	reg_msa2 = dptx_readl(dptx, DPTX_VIDEO_MSA2_N(stream));
	reg_msa2 &= ~DPTX_VIDEO_VMSA2_COL_MASK;

	col_mapping = 0;

	/* According to Table 2-94 of DisplayPort spec 1.3 */
	switch (pix_enc) {
	case RGB:
		if (dynamic_range == CEA)
			col_mapping = 4;
		else if (dynamic_range == VESA)
			col_mapping = 0;
		break;
	case YCBCR422:
		if (colorimetry == ITU601)
			col_mapping = 5;
		else if (colorimetry == ITU709)
			col_mapping = 13;
		break;
	case YCBCR444:
		if (colorimetry == ITU601)
			col_mapping = 6;
		else if (colorimetry == ITU709)
			col_mapping = 14;
		break;
	case YCBCR420:
		break;
	default:
		col_mapping = 4;
		DRM_ERROR("%s: pix_enc error %d", __func__, pix_enc);
		break;
	}

	reg_msa2 |= col_mapping << DPTX_VIDEO_VMSA2_COL_SHIFT;
	dptx_writel(dptx, DPTX_VIDEO_MSA2_N(stream), reg_msa2);
}

void dptx_video_set_sink_bpc(struct dptx *dptx, int stream)
{
	u32 reg_msa2, reg_msa3;
	u8 bpc_mapping = 0, bpc = 0;
	struct video_params *vparams;
	enum pixel_enc_type pix_enc;

	vparams = &dptx->vparams;
	pix_enc = vparams->pix_enc;
	bpc = vparams->bpc;

	reg_msa2 = dptx_readl(dptx, DPTX_VIDEO_MSA2_N(stream));
	reg_msa3 = dptx_readl(dptx, DPTX_VIDEO_MSA3_N(stream));

	reg_msa2 &= ~DPTX_VIDEO_VMSA2_BPC_MASK;
	reg_msa3 &= ~DPTX_VIDEO_VMSA3_PIX_ENC_MASK;

	switch (pix_enc) {
	case RGB:
		if (bpc == COLOR_DEPTH_6)
			bpc_mapping = 0;
		else if (bpc == COLOR_DEPTH_8)
			bpc_mapping = 1;
		else if (bpc == COLOR_DEPTH_10)
			bpc_mapping = 2;
		else if (bpc == COLOR_DEPTH_12)
			bpc_mapping = 3;
		if (bpc == COLOR_DEPTH_16)
			bpc_mapping = 4;
		break;
	case YCBCR444:
		if (bpc == COLOR_DEPTH_8)
			bpc_mapping = 1;
		else if (bpc == COLOR_DEPTH_10)
			bpc_mapping = 2;
		else if (bpc == COLOR_DEPTH_12)
			bpc_mapping = 3;
		if (bpc == COLOR_DEPTH_16)
			bpc_mapping = 4;
		break;
	case YCBCR422:
		if (bpc == COLOR_DEPTH_8)
			bpc_mapping = 1;
		else if (bpc == COLOR_DEPTH_10)
			bpc_mapping = 2;
		else if (bpc == COLOR_DEPTH_12)
			bpc_mapping = 3;
		if (bpc == COLOR_DEPTH_16)
			bpc_mapping = 4;
		break;
	case YCBCR420:
		reg_msa3 |= 1 << DPTX_VIDEO_VMSA3_PIX_ENC_YCBCR420_SHIFT;
		break;
	default:
		bpc_mapping = 1;
		DRM_ERROR("%s: pix_enc error %d", __func__, pix_enc);
		break;
	}
	reg_msa2 |= bpc_mapping << DPTX_VIDEO_VMSA2_BPC_SHIFT;
	dptx_writel(dptx, DPTX_VIDEO_MSA2_N(stream), reg_msa2);
	dptx_writel(dptx, DPTX_VIDEO_MSA3_N(stream), reg_msa3);
	dptx_video_set_sink_col(dptx, stream);
}

void dptx_disable_default_video_stream(struct dptx *dptx, int stream)
{
	u32 vsamplectrl;

	vsamplectrl = dptx_readl(dptx, DPTX_VSAMPLE_CTRL_N(stream));
	vsamplectrl &= ~DPTX_VSAMPLE_CTRL_STREAM_EN;
	dptx_writel(dptx, DPTX_VSAMPLE_CTRL_N(stream), vsamplectrl);
}

void dptx_enable_default_video_stream(struct dptx *dptx, int stream)
{
	u32 vsamplectrl;

	vsamplectrl = dptx_readl(dptx, DPTX_VSAMPLE_CTRL_N(stream));
	vsamplectrl |= DPTX_VSAMPLE_CTRL_STREAM_EN;
	dptx_writel(dptx, DPTX_VSAMPLE_CTRL_N(stream), vsamplectrl);
}

void dptx_video_params_reset(struct dptx *dptx)
{
	struct video_params *params = &dptx->vparams;

	params->pix_enc = RGB;
	params->mode = 1;
	params->bpc = COLOR_DEPTH_8;
	params->colorimetry = ITU601;
	params->dynamic_range = CEA;
	params->aver_bytes_per_tu = 30;
	params->aver_bytes_per_tu_frac = 0;
	params->init_threshold = 15;
	params->pattern_mode = RAMP;
	params->refresh_rate = 60000;
	params->video_format = VCEA;
}

void dptx_misc_reset(struct dptx *dptx)
{
	dptx->mst = false;
	dptx->ssc_en = true;
	dptx->streams = 1;
	dptx->multipixel = DPTX_MP_SINGLE_PIXEL;
	dptx->max_rate = DPTX_DEFAULT_LINK_RATE;
	dptx->max_lanes = DPTX_DEFAULT_LINK_LANES;
}
