// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include "mt6989-afe-common.h"
#include <linux/pm_runtime.h>

#include "../common/mtk-sram-manager.h"

/* don't use this directly if not necessary */
static struct mtk_base_afe *local_afe;

int mt6989_set_local_afe(struct mtk_base_afe *afe)
{
	local_afe = afe;
	return 0;
}

unsigned int mt6989_general_rate_transform(struct device *dev,
		unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_IPM2P0_RATE_8K;
	case 11025:
		return MTK_AFE_IPM2P0_RATE_11K;
	case 12000:
		return MTK_AFE_IPM2P0_RATE_12K;
	case 16000:
		return MTK_AFE_IPM2P0_RATE_16K;
	case 22050:
		return MTK_AFE_IPM2P0_RATE_22K;
	case 24000:
		return MTK_AFE_IPM2P0_RATE_24K;
	case 32000:
		return MTK_AFE_IPM2P0_RATE_32K;
	case 44100:
		return MTK_AFE_IPM2P0_RATE_44K;
	case 48000:
		return MTK_AFE_IPM2P0_RATE_48K;
	case 88200:
		return MTK_AFE_IPM2P0_RATE_88K;
	case 96000:
		return MTK_AFE_IPM2P0_RATE_96K;
	case 176400:
		return MTK_AFE_IPM2P0_RATE_176K;
	case 192000:
		return MTK_AFE_IPM2P0_RATE_192K;
	/* not support 260K */
	case 352800:
		return MTK_AFE_IPM2P0_RATE_352K;
	case 384000:
		return MTK_AFE_IPM2P0_RATE_384K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_IPM2P0_RATE_48K);
		return MTK_AFE_IPM2P0_RATE_48K;
	}
}

static unsigned int pcm_rate_transform(struct device *dev,
				       unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_PCM_RATE_8K;
	case 16000:
		return MTK_AFE_PCM_RATE_16K;
	case 32000:
		return MTK_AFE_PCM_RATE_32K;
	case 48000:
		return MTK_AFE_PCM_RATE_48K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_PCM_RATE_32K);
		return MTK_AFE_PCM_RATE_32K;
	}
}

unsigned int mt6989_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk)
{
	switch (aud_blk) {
	case MT6989_DAI_PCM_0:
	case MT6989_DAI_PCM_1:
		return pcm_rate_transform(dev, rate);
	default:
		return mt6989_general_rate_transform(dev, rate);
	}
}

int mt6989_dai_set_priv(struct mtk_base_afe *afe, int id,
			int priv_size, const void *priv_data)
{
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	void *temp_data;

	temp_data = devm_kzalloc(afe->dev,
				 priv_size,
				 GFP_KERNEL);
	if (!temp_data)
		return -ENOMEM;

	if (priv_data)
		memcpy(temp_data, priv_data, priv_size);

	if (id < 0 || id >= MT6989_DAI_NUM) {
		dev_info(afe->dev, "%s(), invalid DAI id %d\n", __func__, id);
		return -EINVAL;
	}

	afe_priv->dai_priv[id] = temp_data;

	return 0;
}

/* DC compensation */
int mt6989_enable_dc_compensation(bool enable)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);


	pm_runtime_get_sync(local_afe->dev);
	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6989_enable_dc_compensation);

int mt6989_set_lch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);
	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6989_set_lch_dc_compensation);

int mt6989_set_rch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);
	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6989_set_rch_dc_compensation);

int mt6989_adda_dl_gain_control(bool mute)
{
	unsigned int dl_gain_ctl;

	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_info(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);

	if (mute)
		dl_gain_ctl = MTK_AFE_ADDA_DL_GAIN_MUTE;
	else
		dl_gain_ctl = MTK_AFE_ADDA_DL_GAIN_NORMAL;


	dev_info(local_afe->dev, "%s(), adda_dl_gain %x\n",
		 __func__, dl_gain_ctl);

	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6989_adda_dl_gain_control);

struct audio_swpm_data mt6989_aud_get_power_scenario(void)
{
	struct audio_swpm_data test;

	test.adda_mode = 0;
	test.afe_on = 1;
	test.channel_num = 4;
	test.input_device = 0;
	test.output_device = 2;
	test.sample_rate = 2;
	test.user_case = 1;
	return test;
}
EXPORT_SYMBOL_GPL(mt6989_aud_get_power_scenario);
