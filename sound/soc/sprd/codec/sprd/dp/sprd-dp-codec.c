/*
 * SPRD SoC DP-Codec -- SpreadTrum SOC DP-Codec.
 *
 * Copyright (C) 2021 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/sprd-dp-codec.h>

struct sprd_dp_codec_priv {
	struct sprd_dp_codec_pdata data;
	struct sprd_dp_codec_params params;
};


static int sprd_dp_get_bit_width(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_dp_codec_priv *priv = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = priv->params.data_width;

	return 0;
}

static int sprd_dp_set_bit_width(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_dp_codec_priv *priv = snd_soc_codec_get_drvdata(codec);

	priv->params.data_width = ucontrol->value.integer.value[0];
	if (priv->params.data_width < 16)
		priv->params.data_width = 16;

	return 0;
}

static const struct snd_kcontrol_new sprd_dp_snd_controls[] = {
	SOC_SINGLE_EXT("DP Data Width", 0, 0, 24, 0,
		sprd_dp_get_bit_width, sprd_dp_set_bit_width),
};

static int sprd_dp_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct sprd_dp_codec_priv *priv = snd_soc_dai_get_drvdata(dai);
	int ret;

	if (priv->data.ops->audio_startup) {
		ret = priv->data.ops->audio_startup(dai->dev);
		if (ret)
			return ret;
	}

	return 0;
}

static void sprd_dp_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct sprd_dp_codec_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (priv->data.ops->audio_shutdown)
		priv->data.ops->audio_shutdown(dai->dev);

}

static int sprd_dp_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct sprd_dp_codec_priv *priv = snd_soc_dai_get_drvdata(dai);

	priv->params.channel = params_channels(params);
	priv->params.rate = params_rate(params);

	pr_info("%s: channel (%u) rate (%u) width (%u) aif_type (%u)\n",
		__func__, priv->params.channel, priv->params.rate,
		priv->params.data_width, priv->params.aif_type);

	if (priv->data.ops->hw_params)
		return priv->data.ops->hw_params(dai->dev, &priv->params);

	return 0;
}

static int sprd_dp_dai_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct sprd_dp_codec_priv *priv = snd_soc_dai_get_drvdata(dai);

	if (priv->data.ops->digital_mute)
		return priv->data.ops->digital_mute(dai->dev, mute);

	return 0;
}

static const struct snd_soc_dai_ops sprd_dp_dai_ops = {
	.startup		= sprd_dp_dai_startup,
	.shutdown		= sprd_dp_dai_shutdown,
	.hw_params		= sprd_dp_dai_hw_params,
	.digital_mute	= sprd_dp_dai_digital_mute,
};

static struct snd_soc_dai_driver sprd_dp_dai[] = {
	{
		.name = "DP I2S",
		.id = AIF_DP_I2S,
		.playback = {
			.stream_name	= "DP I2S Playback",
			.formats		= SNDRV_PCM_FMTBIT_S32_LE,
			.rates			= SNDRV_PCM_RATE_8000_192000,
			.rate_min		= 8000,
			.rate_max		= 192000,
			.channels_min	= 1,
			.channels_max	= 8,
		},
		.ops = &sprd_dp_dai_ops,
	},
};

static const struct snd_soc_codec_driver sprd_dp_codec = {
	.component_driver = {
		.controls		= sprd_dp_snd_controls,
		.num_controls	= ARRAY_SIZE(sprd_dp_snd_controls),
	},
};

static int sprd_dp_codec_probe(struct platform_device *pdev)
{
	struct sprd_dp_codec_pdata *data = pdev->dev.platform_data;
	struct sprd_dp_codec_priv *priv;

	if (!data)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev,
		sizeof(struct sprd_dp_codec_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->data = *data;
	priv->params.channel = 2;
	priv->params.rate = 48000;
	priv->params.data_width = 24;
	priv->params.aif_type = AIF_DP_I2S;

	dev_set_drvdata(&pdev->dev, priv);

	pr_info("%s: success\n", __func__);

	return snd_soc_register_codec(&pdev->dev,
					&sprd_dp_codec,
					sprd_dp_dai,
					ARRAY_SIZE(sprd_dp_dai));
}

static int sprd_dp_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static struct platform_driver sprd_dp_codec_driver = {
	.driver = {
		.name = SPRD_DP_CODEC_DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = sprd_dp_codec_probe,
	.remove = sprd_dp_codec_remove,
};
module_platform_driver(sprd_dp_codec_driver);

MODULE_DESCRIPTION("SPRD ASoC DP Codec Driver");
MODULE_AUTHOR("Bin Pang <bin.pang@unisoc.com>");
MODULE_LICENSE("GPL");
