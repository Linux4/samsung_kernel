/*
 * Universal3475-COD3025X Audio Machine driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/tlv.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/s2803x.h>
#include <sound/cod3025x.h>

#include <mach/regs-pmu.h>

#ifdef CONFIG_SAMSUNG_JACK
#include <linux/iio/consumer.h>
#include <linux/sec_jack.h>
#endif

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/cod3025x.h"

#define COD3025X_BFS_48KHZ		32
#define COD3025X_RFS_48KHZ		512
#define COD3025X_SAMPLE_RATE_48KHZ	48000

#define COD3025X_BFS_192KHZ		64
#define COD3025X_RFS_192KHZ		128
#define COD3025X_SAMPLE_RATE_192KHZ	192000

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err
#endif

#ifdef CONFIG_SAMSUNG_JACK
#define SEC_JACK_SAMPLE_SIZE 5
static struct iio_channel *jack_adc;

static int sec_jack_get_adc_data(struct iio_channel *jack_adc)
{
	int adc_data = -1;
	int adc_max = 0;
	int adc_min = 0xFFFF;
	int adc_total = 0;
	int adc_retry_cnt = 0;
	int ret = 0;
	int i;

	for (i = 0; i < SEC_JACK_SAMPLE_SIZE; i++) {
		ret = iio_read_channel_raw(&jack_adc[0], &adc_data);
		if (adc_data < 0) {

			adc_retry_cnt++;

			if (adc_retry_cnt > 10)
				return adc_data;
		}

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (SEC_JACK_SAMPLE_SIZE - 2);
}

static int get_adc_value(void){
	return sec_jack_get_adc_data(jack_adc);
}
#endif
static struct snd_soc_card universal3475_cod3025x_card;

struct cod3025x_machine_priv {
	struct snd_soc_codec *codec;
	int aifrate;
	bool use_external_jd;
};

static const struct snd_soc_component_driver universal3475_cmpnt = {
	.name = "Universal3475-audio",
};

static int universal3475_aif1_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct cod3025x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;
	int rfs, bfs;

	dev_info(card->dev, "aif1: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	priv->aifrate = params_rate(params);
	if (priv->aifrate == COD3025X_SAMPLE_RATE_192KHZ) {
		rfs = COD3025X_RFS_192KHZ;
		bfs = COD3025X_BFS_192KHZ;
	} else {
		rfs = COD3025X_RFS_48KHZ;
		bfs = COD3025X_BFS_48KHZ;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set Codec DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set CPU DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
				rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set SAMSUNG_I2S_CDCLK\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
						0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set SAMSUNG_I2S_OPCLK\n");
		return ret;
}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0) {
		dev_err(card->dev,
				"aif1: Failed to set SAMSUNG_I2S_RCLKSRC_1\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set BFS\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_RCLK, rfs);
		if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set RFS\n");
			return ret;
		}

	ret = s2803x_hw_params(substream, params, bfs, 1);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal3475_aif2_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int bfs, ret;

	dev_info(card->dev, "aif2: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		dev_err(card->dev, "aif2: Unsupported PCM_FORMAT\n");
		return -EINVAL;
	}

	ret = s2803x_hw_params(substream, params, bfs, 2);
	if (ret < 0) {
		dev_err(card->dev, "aif2: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal3475_aif3_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int bfs, ret;

	dev_info(card->dev, "aif3: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		dev_err(card->dev, "aif3: Unsupported PCM_FORMAT\n");
		return -EINVAL;
	}

	ret = s2803x_hw_params(substream, params, bfs, 3);
	if (ret < 0) {
		dev_err(card->dev, "aif3: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal3475_aif1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif1: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_startup(S2803X_IF_AP);
	return 0;
}

void universal3475_aif1_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif1: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_shutdown(S2803X_IF_AP);
}

static int universal3475_aif2_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif2: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_startup(S2803X_IF_CP);

	return 0;
}

void universal3475_aif2_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif2: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_shutdown(S2803X_IF_CP);
}

static int universal3475_aif3_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif3: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_startup(S2803X_IF_BT);

	return 0;
}

void universal3475_aif3_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif3: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_shutdown(S2803X_IF_BT);
}

static int universal3475_aif4_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif4: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_startup(S2803X_IF_BT);

	return 0;
}

void universal3475_aif4_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif4: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2803x_shutdown(S2803X_IF_BT);
}

static int universal3475_set_bias_level(struct snd_soc_card *card,
				 struct snd_soc_dapm_context *dapm,
				 enum snd_soc_bias_level level)
{
	return 0;
}

static int universal3475_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct device *dev = card->dev;
	struct cod3025x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;
	dev_dbg(card->dev, "%s called\n", __func__);

	if (of_find_property(dev->of_node, "samsung,use-externel-jd",
					NULL) != NULL) {
		priv->use_external_jd = true;
		cod3025x_set_externel_jd(codec);
	} else {
		priv->use_external_jd = false;
	ret = cod3025x_jack_mic_register(codec);
	if (ret < 0) {
		dev_err(card->dev,
			"Jack mic registration failed with error %d\n", ret);
		return ret;
		}
	}

	return 0;
}

static int s2803x_init(struct snd_soc_dapm_context *dapm)
{
	dev_dbg(dapm->dev, "%s called\n", __func__);

	return 0;
}

static const struct snd_kcontrol_new universal3475_controls[] = {
};

const struct snd_soc_dapm_widget universal3475_dapm_widgets[] = {
};

const struct snd_soc_dapm_route universal3475_dapm_routes[] = {
};

static struct snd_soc_ops universal3475_aif1_ops = {
	.hw_params = universal3475_aif1_hw_params,
	.startup = universal3475_aif1_startup,
	.shutdown = universal3475_aif1_shutdown,
};

static struct snd_soc_ops universal3475_aif2_ops = {
	.hw_params = universal3475_aif2_hw_params,
	.startup = universal3475_aif2_startup,
	.shutdown = universal3475_aif2_shutdown,
};

static struct snd_soc_ops universal3475_aif3_ops = {
	.hw_params = universal3475_aif3_hw_params,
	.startup = universal3475_aif3_startup,
	.shutdown = universal3475_aif3_shutdown,
};

static struct snd_soc_ops universal3475_aif4_ops = {
	.startup = universal3475_aif4_startup,
	.shutdown = universal3475_aif4_shutdown,
};

static struct snd_soc_dai_driver universal3475_ext_dai[] = {
	{
		.name = "universal3475 voice call",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
	},
	{
		.name = "universal3475 BT",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
	},
	{
		.name = "universal3475 FM",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
			 SNDRV_PCM_FMTBIT_S24_LE)
		},
	},
};

static struct snd_soc_dai_link universal3475_cod3025x_dai[] = {
	/* Playback and Recording */
	{
		.name = "universal3475-cod3025x",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "cod3025x-aif",
		.ops = &universal3475_aif1_ops,
	},
	/* Deep buffer playback */
	{
		.name = "universal3475-cod3025x-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.stream_name = "i2s0-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "cod3025x-aif",
		.ops = &universal3475_aif1_ops,
	},
	/* Voice Call */
	{
		.name = "cp",
		.stream_name = "voice call",
		.cpu_dai_name = "universal3475 voice call",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "cod3025x-aif2",
		.ops = &universal3475_aif2_ops,
		.ignore_suspend = 1,
	},
	/* BT */
	{
		.name = "bt",
		.stream_name = "bluetooth audio",
		.cpu_dai_name = "universal3475 BT",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "dummy-aif2",
		.ops = &universal3475_aif3_ops,
		.ignore_suspend = 1,
	},
	/* FM */
	{
		.name = "fm",
		.stream_name = "FM audio",
		.cpu_dai_name = "universal3475 FM",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "cod3025x-aif",
		.ops = &universal3475_aif4_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_aux_dev s2803x_aux_dev[] = {
	{
		.init = s2803x_init,
	},
};

static struct snd_soc_codec_conf s2803x_codec_conf[] = {
	{
		.name_prefix = "AudioMixer",
	},
};

static struct snd_soc_card universal3475_cod3025x_card = {
	.name = "Universal3475-I2S",
	.owner = THIS_MODULE,

	.dai_link = universal3475_cod3025x_dai,
	.num_links = ARRAY_SIZE(universal3475_cod3025x_dai),

	.controls = universal3475_controls,
	.num_controls = ARRAY_SIZE(universal3475_controls),
	.dapm_widgets = universal3475_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal3475_dapm_widgets),
	.dapm_routes = universal3475_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal3475_dapm_routes),

	.late_probe = universal3475_late_probe,
	.set_bias_level = universal3475_set_bias_level,
	.aux_dev = s2803x_aux_dev,
	.num_aux_devs = ARRAY_SIZE(s2803x_aux_dev),
	.codec_conf = s2803x_codec_conf,
	.num_configs = ARRAY_SIZE(s2803x_codec_conf),
};

static int universal3475_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_np, *codec_np, *auxdev_np;
	struct snd_soc_card *card = &universal3475_cod3025x_card;
	struct cod3025x_machine_priv *priv;

	if (!np) {
		dev_err(&pdev->dev, "Failed to get device node\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	card->dev = &pdev->dev;
	card->num_links = 0;

	ret = snd_soc_register_component(card->dev, &universal3475_cmpnt,
			universal3475_ext_dai,
			ARRAY_SIZE(universal3475_ext_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(universal3475_cod3025x_dai); n++) {
		/* Skip parsing DT for fully formed dai links */
		if (universal3475_cod3025x_dai[n].platform_name &&
				universal3475_cod3025x_dai[n].codec_name) {
			dev_dbg(card->dev,
			"Skipping dt for populated dai link %s\n",
			universal3475_cod3025x_dai[n].name);
			card->num_links++;
			continue;
		}

		cpu_np = of_parse_phandle(np, "samsung,audio-cpu", n);
		if (!cpu_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,audio-cpu' missing\n");
			break;
		}

		codec_np = of_parse_phandle(np, "samsung,audio-codec", n);
		if (!codec_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,audio-codec' missing\n");
			break;
		}

		universal3475_cod3025x_dai[n].codec_of_node = codec_np;
		if (!universal3475_cod3025x_dai[n].cpu_dai_name)
			universal3475_cod3025x_dai[n].cpu_of_node = cpu_np;
		if (!universal3475_cod3025x_dai[n].platform_name)
			universal3475_cod3025x_dai[n].platform_of_node = cpu_np;

		card->num_links++;
	}

	for (n = 0; n < ARRAY_SIZE(s2803x_aux_dev); n++) {
		auxdev_np = of_parse_phandle(np, "samsung,auxdev", n);
		if (!auxdev_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,auxdev' missing\n");
			return -EINVAL;
		}

		s2803x_aux_dev[n].of_node = auxdev_np;
		s2803x_codec_conf[n].of_node = auxdev_np;
	}

	snd_soc_card_set_drvdata(card, priv);

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);

#ifdef CONFIG_SAMSUNG_JACK
	jack_adc = iio_channel_get_all(&pdev->dev);
	jack_controls.get_adc = get_adc_value;
	jack_controls.snd_card_registered = 1;
#endif
	return ret;
}

static int universal3475_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

#ifdef CONFIG_SAMSUNG_JACK
	iio_channel_release(jack_adc);
#endif
	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id universal3475_cod3025x_of_match[] = {
	{.compatible = "samsung,universal3475-cod3025x",},
	{},
};
MODULE_DEVICE_TABLE(of, universal3475_cod3025x_of_match);

static struct platform_driver universal3475_audio_driver = {
	.driver = {
		.name = "Universal3475-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(universal3475_cod3025x_of_match),
	},
	.probe = universal3475_audio_probe,
	.remove = universal3475_audio_remove,
};

module_platform_driver(universal3475_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Universal3475 COD3025X");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:universal3475-audio");
