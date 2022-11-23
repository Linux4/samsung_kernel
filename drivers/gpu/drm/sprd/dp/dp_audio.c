// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <sound/sprd-dp-codec.h>
#include "dw_dptx.h"
#include "sprd_dp.h"

void dptx_audio_params_reset(struct audio_params *params)
{
	params->data_width = 24;
	params->num_channels = 2;
	params->inf_type = AIF_DP_I2S;
	params->ats_ver = 17;
	params->mute = 0;
	params->rate = 48000;
}

void dptx_audio_sdp_en(struct dptx *dptx)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_SDP_VERTICAL_CTRL);
	reg |= DPTX_EN_AUDIO_STREAM_SDP;
	dptx_writel(dptx, DPTX_SDP_VERTICAL_CTRL, reg);

	reg = dptx_readl(dptx, DPTX_SDP_HORIZONTAL_CTRL);
	reg |= DPTX_EN_AUDIO_STREAM_SDP;
	dptx_writel(dptx, DPTX_SDP_HORIZONTAL_CTRL, reg);
}

void dptx_audio_timestamp_sdp_en(struct dptx *dptx)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_SDP_VERTICAL_CTRL);
	reg |= DPTX_EN_AUDIO_TIMESTAMP_SDP;
	dptx_writel(dptx, DPTX_SDP_VERTICAL_CTRL, reg);
}

void dptx_audio_stop(struct dptx *dptx)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_SDP_VERTICAL_CTRL);
	reg &= ~DPTX_EN_AUDIO_STREAM_SDP;
	dptx_writel(dptx, DPTX_SDP_VERTICAL_CTRL, reg);

	reg = dptx_readl(dptx, DPTX_SDP_HORIZONTAL_CTRL);
	reg &= ~DPTX_EN_AUDIO_STREAM_SDP;
	dptx_writel(dptx, DPTX_SDP_HORIZONTAL_CTRL, reg);

	reg = dptx_readl(dptx, DPTX_SDP_VERTICAL_CTRL);
	reg &= ~DPTX_EN_AUDIO_TIMESTAMP_SDP;
	dptx_writel(dptx, DPTX_SDP_VERTICAL_CTRL, reg);
}

void dptx_audio_infoframe_sdp_send(struct dptx *dptx)
{
	u32 reg;
	u32 audio_infoframe_header = AUDIO_INFOFREAME_HEADER;
	u32 audio_infoframe_data[3] = {0x00000710, 0x0, 0x0};

	dptx->sdp_list[0].payload[0] = audio_infoframe_header;
	dptx_writel(dptx, DPTX_SDP_BANK, audio_infoframe_header);
	dptx_writel(dptx, DPTX_SDP_BANK + 4, audio_infoframe_data[0]);
	dptx_writel(dptx, DPTX_SDP_BANK + 8, audio_infoframe_data[1]);
	dptx_writel(dptx, DPTX_SDP_BANK + 12, audio_infoframe_data[2]);
	reg = dptx_readl(dptx, DPTX_SDP_VERTICAL_CTRL);
	reg |= DPTX_EN_AUDIO_INFOFRAME_SDP;
	dptx_writel(dptx, DPTX_SDP_VERTICAL_CTRL, reg);
}

void dptx_en_audio_channel(struct dptx *dptx, int ch_num, int enable)
{
	u32 reg;
	u32 data_en;

	reg = dptx_readl(dptx, DPTX_AUD_CONFIG1);
	reg &= ~DPTX_AUD_CONFIG1_DATA_EN_IN_MASK;

	if (enable) {
		switch (ch_num) {
		case 1:
			data_en = DPTX_EN_AUDIO_CH_1;
			break;
		case 2:
			data_en = DPTX_EN_AUDIO_CH_2;
			break;
		case 3:
			data_en = DPTX_EN_AUDIO_CH_3;
			break;
		case 4:
			data_en = DPTX_EN_AUDIO_CH_4;
			break;
		case 5:
			data_en = DPTX_EN_AUDIO_CH_5;
			break;
		case 6:
			data_en = DPTX_EN_AUDIO_CH_6;
			break;
		case 7:
			data_en = DPTX_EN_AUDIO_CH_7;
			break;
		case 8:
			data_en = DPTX_EN_AUDIO_CH_8;
			break;
		default:
			DRM_ERROR("channel num Invalid: %d\n", ch_num);
			data_en = DPTX_EN_AUDIO_CH_1;
			break;
		}
		reg |= data_en << DPTX_AUD_CONFIG1_DATA_EN_IN_SHIFT;
	} else {
		switch (ch_num) {
		case 1:
			data_en = ~DPTX_EN_AUDIO_CH_1;
			break;
		case 2:
			data_en = ~DPTX_EN_AUDIO_CH_2;
			break;
		case 3:
			data_en = ~DPTX_EN_AUDIO_CH_3;
			break;
		case 4:
			data_en = ~DPTX_EN_AUDIO_CH_4;
			break;
		case 5:
			data_en = ~DPTX_EN_AUDIO_CH_5;
			break;
		case 6:
			data_en = ~DPTX_EN_AUDIO_CH_6;
			break;
		case 7:
			data_en = ~DPTX_EN_AUDIO_CH_7;
			break;
		case 8:
			data_en = ~DPTX_EN_AUDIO_CH_8;
			break;
		default:
			DRM_ERROR("channel num Invalid: %d\n", ch_num);
			data_en = ~DPTX_EN_AUDIO_CH_1;
			break;
		}
		reg &= data_en << DPTX_AUD_CONFIG1_DATA_EN_IN_SHIFT;
	}
	dptx_writel(dptx, DPTX_AUD_CONFIG1, reg);
}

void dptx_audio_mute(struct dptx *dptx, bool enable)
{
	u32 reg;

	reg = dptx_readl(dptx, DPTX_AUD_CONFIG1);

	if (enable)
		reg |= DPTX_AUDIO_MUTE;
	else
		reg &= ~DPTX_AUDIO_MUTE;
	dptx_writel(dptx, DPTX_AUD_CONFIG1, reg);
}

void dptx_audio_config(struct dptx *dptx)
{
	dptx_audio_core_config(dptx);
	dptx_audio_sdp_en(dptx);
	dptx_audio_timestamp_sdp_en(dptx);
	//dptx_audio_infoframe_sdp_send(dptx);
}

void dptx_audio_inf_type_change(struct dptx *dptx)
{
	u32 reg;
	struct audio_params *aparams = &dptx->aparams;

	reg = dptx_readl(dptx, DPTX_AUD_CONFIG1);
	reg &= ~DPTX_AUD_CONFIG1_INF_TYPE_MASK;
	reg |= aparams->inf_type << DPTX_AUD_CONFIG1_INF_TYPE_SHIFT;
	dptx_writel(dptx, DPTX_AUD_CONFIG1, reg);
}

void dptx_audio_core_config(struct dptx *dptx)
{
	struct audio_params *aparams;
	u32 reg;

	aparams = &dptx->aparams;

	dptx_audio_inf_type_change(dptx);
	dptx_audio_num_ch_change(dptx);
	dptx_audio_data_width_change(dptx);

	reg = dptx_readl(dptx, DPTX_AUD_CONFIG1);
	reg &= ~DPTX_AUD_CONFIG1_ATS_VER_MASK;
	reg |= aparams->ats_ver << DPTX_AUD_CONFIG1_ATS_VER_SHFIT;
	dptx_writel(dptx, DPTX_AUD_CONFIG1, reg);

	if (aparams->rate == 192000) {
		reg = dptx_readl(dptx, DPTX_AUD_CONFIG1);
		reg &= ~DPTX_AUD_CONFIG1_HBR_MODE_MASK;
		reg |= aparams->rate << DPTX_AUD_CONFIG1_HBR_MODE_SHIFT;
		dptx_writel(dptx, DPTX_AUD_CONFIG1, reg);
	}

	dptx_en_audio_channel(dptx, aparams->num_channels, 1);
}

void dptx_audio_num_ch_change(struct dptx *dptx)
{
	u32 reg = 0;
	u32 num_ch_map;
	struct audio_params *aparams;

	aparams = &dptx->aparams;

	reg = dptx_readl(dptx, DPTX_AUD_CONFIG1);
	reg &= ~DPTX_AUD_CONFIG1_NCH_MASK;

	if (aparams->num_channels == 1)
		num_ch_map = 0;
	else if (aparams->num_channels == 2)
		num_ch_map = 1;
	else
		num_ch_map = aparams->num_channels - 1;

	reg |= num_ch_map << DPTX_AUD_CONFIG1_NCH_SHIFT;
	dptx_writel(dptx, DPTX_AUD_CONFIG1, reg);
}

void dptx_audio_data_width_change(struct dptx *dptx)
{
	u32 reg = 0;
	struct audio_params *aparams;

	aparams = &dptx->aparams;

	reg = dptx_readl(dptx, DPTX_AUD_CONFIG1);
	reg &= ~DPTX_AUD_CONFIG1_DATA_WIDTH_MASK;
	reg |= aparams->data_width << DPTX_AUD_CONFIG1_DATA_WIDTH_SHIFT;
	dptx_writel(dptx, DPTX_AUD_CONFIG1, reg);
}

static void sprd_dp_audio_shutdown(struct device *dev)
{
	struct sprd_dp_codec_pdata *pdata = dev_get_drvdata(dev);
	struct sprd_dp *dp = pdata->private;

	if (dp->snps_dptx->active)
		dptx_audio_stop(dp->snps_dptx);
}

static int sprd_dp_audio_hw_params(struct device *dev, void *data)
{
	struct sprd_dp_codec_pdata *pdata = dev_get_drvdata(dev);
	struct sprd_dp *dp = pdata->private;
	struct sprd_dp_codec_params *params = data;
	struct audio_params *aparams = &dp->snps_dptx->aparams;

	aparams->data_width = params->data_width;
	aparams->num_channels = params->channel;
	aparams->inf_type = params->aif_type;
	aparams->rate = params->rate;

	if (dp->snps_dptx->active)
		dptx_audio_config(dp->snps_dptx);

	return 0;
}

static int sprd_dp_audio_digital_mute(struct device *dev, bool enable)
{
	struct sprd_dp_codec_pdata *pdata = dev_get_drvdata(dev);
	struct sprd_dp *dp = pdata->private;

	if (dp->snps_dptx->active)
		dptx_audio_mute(dp->snps_dptx, enable);

	return 0;
}

static const struct sprd_dp_codec_ops dp_codec_ops = {
	.audio_shutdown = sprd_dp_audio_shutdown,
	.hw_params = sprd_dp_audio_hw_params,
	.digital_mute = sprd_dp_audio_digital_mute,
};

int sprd_dp_audio_codec_init(struct device *dev)
{
	struct sprd_dp *dp = dev_get_drvdata(dev);
	struct sprd_dp_codec_pdata pdata = {
		.ops = &dp_codec_ops,
		.i2s = 1,
		.spdif = 1,
		.max_i2s_channels = 8,
	};

	if (!dp) {
		DRM_DEV_ERROR(dev, "Faied to get dp module\n");
		return -EINVAL;
	}

	pdata.private = dp;
	dp->audio_pdev = platform_device_register_data(
			 dev, SPRD_DP_CODEC_DRV_NAME, PLATFORM_DEVID_AUTO,
			 &pdata, sizeof(pdata));

	return PTR_ERR_OR_ZERO(dp->audio_pdev);
}
