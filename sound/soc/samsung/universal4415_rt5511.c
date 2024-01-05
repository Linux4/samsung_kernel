/* sound/soc/samsung/universal4415_rt5511.c
 * RT5511 on universal4415 machine driver
 * Copyright (c) 2014 Richtek Technology Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/exynos_audio.h>

#include <linux/mfd/rt5511/registers.h>
#include <linux/mfd/rt5511/pdata.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/gpio.h>

#include <plat/adc.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "s3c-i2s-v2.h"
#include "../codecs/rt5511.h"


#define UNIVERSAL4415_DEFAULT_MCLK1	24000000
#define UNIVERSAL4415_DEFAULT_MCLK2	32768
#define UNIVERSAL4415_DEFAULT_SYNC_CLK	12288000

struct rt5511_machine_priv {
	struct snd_soc_codec *codec;
	void (*set_mclk) (bool enable, bool forced);
};

static int aif2_mode;

const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static int get_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aif2_mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (aif2_mode == ucontrol->value.integer.value[0])
		return 0;

	aif2_mode = ucontrol->value.integer.value[0];

	pr_info("%s : %s\n", __func__, aif2_mode_text[aif2_mode]);

	return 0;
}

static int universal4415_rt5511_aif1_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int prate = params_rate(params);
	unsigned int pll_out;
	int ret;

	dev_info(codec_dai->dev, "%s %dHz, %dch\n", __func__, prate,
				params_channels(params));

	/* AIF1CLK should be >=3MHz for optimal performance */
	if (prate == 8000 || prate == 11025)
		pll_out = prate * 512;
	else
		pll_out = prate * 256;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	RT_DBG("pll_out=%d\n", pll_out);

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(codec_dai, RT5511_PLL,
				  RT5511_PLL_SRC_MCLK1,
				  UNIVERSAL4415_DEFAULT_MCLK1,
				  pll_out);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to start PLL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5511_SYSCLK_PLL,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to switch to PLL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
				     0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	return 0;
}

static int universal4415_rt5511_aif2_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int prate = params_rate(params);
	int bclk;
	int ret;

	dev_info(codec_dai->dev, "%s %dHz, %dch\n", __func__, prate,
				params_channels(params));

	switch (prate) {
	case 8000:
	case 16000:
	case 48000:
	       break;
	default:
		dev_warn(codec_dai->dev,
			"Unsupported LRCLK %d, falling back to 8kHz\n", prate);
		prate = 8000;
	}

	/* Set the codec DAI configuration */
	if (aif2_mode == 0)
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	else
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);

	if (ret < 0)
		return ret;

	switch (prate) {
	case 8000:
		bclk = 256000;
		break;
	case 16000:
		bclk = 512000;
		break;
	case 48000:
		bclk = 1536000;
		break;
	default:
		return -EINVAL;
	}

	if (aif2_mode == 0) {
		RT_DBG("AIF2:Slave, Ref:BCLK\n");

		snd_soc_update_bits(rtd->codec, RT5511_AIF_CTRL2,
			AIF2_MS_SEL_MASK, AIF_MS_REF_MCLK << AIF2_MS_SEL_SHIFT);

		ret = snd_soc_dai_set_pll(codec_dai, RT5511_PLL,
					RT5511_PLL_SRC_BCLK2,
					bclk, prate * 256);
	} else {
		RT_DBG("AIF2:Master, Ref:AIF2\n");

		ret = snd_soc_dai_set_pll(codec_dai, RT5511_PLL,
				RT5511_PLL_SRC_MCLK2,
				UNIVERSAL4415_DEFAULT_MCLK2, prate * 256);
	}

	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to configure PLL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5511_SYSCLK_PLL,
				prate * 256, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to switch to PLL: %d\n", ret);
		return ret;
	}

	return 0;
}

static int universal4415_rt5511_aif3_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	dev_info(codec_dai->dev, "%s %dHz, %dch\n", __func__,
				params_rate(params),
				params_channels(params));
	return 0;
}

/*
 * universal4415 RT5511 DAI operations.
 */
static struct snd_soc_ops universal4415_rt5511_aif1_ops = {
	.hw_params = universal4415_rt5511_aif1_hw_params,
};

static struct snd_soc_ops universal4415_rt5511_aif2_ops = {
	.hw_params = universal4415_rt5511_aif2_hw_params,
};

static struct snd_soc_ops universal4415_rt5511_aif3_ops = {
	.hw_params = universal4415_rt5511_aif3_hw_params,
};

static const struct snd_kcontrol_new universal4415_codec_controls[] = {
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),
};

static const struct snd_kcontrol_new universal4415_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget universal4415_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
};

const struct snd_soc_dapm_route universal4415_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },

	{ "RCV", NULL, "HPOUT2N" },
	{ "RCV", NULL, "HPOUT2P" },

	{ "MICBIAS2", NULL, "Main Mic" },
	{ "MICBIAS1", NULL, "Sub Mic" },

	{ "MIC1P", NULL, "MICBIAS2" },
	{ "MIC1N", NULL, "MICBIAS1" },
	{ "MIC3P", NULL, "Headset Mic" },
};

static struct snd_soc_dai_driver universal4415_ext_dai[] = {
	{
		.name = "universal4415.cp",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "universal4415.bt",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
				SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link universal4415_dai[] = {
	{ /* Primary DAI i/f */
		.name = "RT5511 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = RT5511_CODEC_DAI1_NAME,
		.platform_name = "samsung-audio",
		.codec_name = RT5511_CODEC_NAME,
		.ops = &universal4415_rt5511_aif1_ops,
	},
	{
		.name = "RT5511 Voice",
		.stream_name = "Voice Tx/Rx",
		.cpu_dai_name = "universal4415.cp",
		.codec_dai_name = RT5511_CODEC_DAI2_NAME,
		.platform_name = "snd-soc-dummy",
		.codec_name = RT5511_CODEC_NAME,
		.ops = &universal4415_rt5511_aif2_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "RT5511 BT",
		.stream_name = "BT Tx/Rx",
		.cpu_dai_name = "universal4415.bt",
		.codec_dai_name = RT5511_CODEC_DAI3_NAME,
		.platform_name = "snd-soc-dummy",
		.codec_name = RT5511_CODEC_NAME,
		.ops = &universal4415_rt5511_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = RT5511_CODEC_DAI1_NAME,
#ifndef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-audio",
#else
		.platform_name = "samsung-idma",
#endif
		.codec_name = RT5511_CODEC_NAME,
		.ops = &universal4415_rt5511_aif1_ops,
	},
};

static int universal4415_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	int ret;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	snd_soc_dapm_ignore_suspend(&card->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HP");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(&card->dapm, "AIF1ADCDAT");

	snd_soc_dapm_disable_pin(&card->dapm, "RCV");
	snd_soc_dapm_disable_pin(&card->dapm, "SPK");
	snd_soc_dapm_disable_pin(&card->dapm, "HP");
	snd_soc_dapm_disable_pin(&card->dapm, "Headset Mic");
	snd_soc_dapm_disable_pin(&card->dapm, "Main Mic");
	snd_soc_dapm_disable_pin(&card->dapm, "Sub Mic");

	ret = snd_soc_add_codec_controls(codec, universal4415_codec_controls,
		ARRAY_SIZE(universal4415_codec_controls));
	if (ret < 0) {
		dev_err(codec->dev,
			"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	return snd_soc_dapm_sync(&codec->dapm);
}

static int universal4415_card_suspend_pre(struct snd_soc_card *card)
{
	return 0;
}

static int universal4415_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct rt5511_machine_priv *machine =
				snd_soc_card_get_drvdata(codec->card);
	int ret = 0;

	if (!codec->active) {
		ret = snd_soc_dai_set_pll(aif1_dai, RT5511_PLL, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL1\n");

		machine->set_mclk(false, true);
	}

	snd_soc_dai_set_tristate(aif1_dai, 1);

	return ret;
}

static int universal4415_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct rt5511_machine_priv *machine =
				snd_soc_card_get_drvdata(codec->card);

	machine->set_mclk(true, false);
	snd_soc_dai_set_tristate(aif1_dai, 0);

	return 0;
}

static int universal4415_card_resume_post(struct snd_soc_card *card)
{
	return 0;
}

static struct snd_soc_card universal4415 = {
	.name = "Universal4415 RT5511",
	.owner = THIS_MODULE,
	.dai_link = universal4415_dai,

	/* If you want to use sec_fifo device,
	 * changes the num_link = 2 or ARRAY_SIZE(universal222ap_dai). */
	.num_links = ARRAY_SIZE(universal4415_dai),

	.controls = universal4415_controls,
	.num_controls = ARRAY_SIZE(universal4415_controls),
	.dapm_widgets = universal4415_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal4415_dapm_widgets),
	.dapm_routes = universal4415_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal4415_dapm_routes),

	.late_probe = universal4415_late_probe,

	.suspend_pre = universal4415_card_suspend_pre,
	.suspend_post = universal4415_card_suspend_post,
	.resume_pre = universal4415_card_resume_pre,
	.resume_post = universal4415_card_resume_post,
};

static int __devinit snd_universal4415_probe(struct platform_device *pdev)
{
	struct rt5511_machine_priv *rt5511;
	const struct exynos_sound_platform_data *sound_pdata;
	int ret = 0;

	rt5511 = kzalloc(sizeof *rt5511, GFP_KERNEL);
	if (!rt5511) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	sound_pdata = exynos_sound_get_platform_data();
	if (!sound_pdata) {
		pr_err("Failed to get sound pdata\n");
		goto err_get_platform_data;
	}

	/* Start the reference clock for the codec's FLL */
	if (sound_pdata->set_mclk) {
		rt5511->set_mclk = sound_pdata->set_mclk;
		rt5511->set_mclk(true, false);
	} else {
		pr_err("Failed to set mclk\n");
		goto err_set_mclk;
	}

	ret = snd_soc_register_dais(&pdev->dev, universal4415_ext_dai,
					ARRAY_SIZE(universal4415_ext_dai));
	if (ret) {
		pr_err("Failed to register external DAIs: %d\n", ret);
		goto err_register_dais;
	}

	snd_soc_card_set_drvdata(&universal4415, rt5511);

	universal4415.dev = &pdev->dev;

	ret = snd_soc_register_card(&universal4415);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		goto err_register_card;
	}

	return 0;

err_register_dais:
err_register_card:
err_set_mclk:
err_get_platform_data:
	kfree(rt5511);
err_kzalloc:
	return ret;
}

static int __devexit snd_universal4415_remove(struct platform_device *pdev)
{
	struct rt5511_machine_priv *rt5511 =
				snd_soc_card_get_drvdata(&universal4415);

	snd_soc_unregister_card(&universal4415);
	kfree(rt5511);

	return 0;
}

static struct platform_driver snd_universal4415_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "rt5511-card",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_universal4415_probe,
	.remove = __devexit_p(snd_universal4415_remove),
};
module_platform_driver(snd_universal4415_driver);

MODULE_AUTHOR("Tsunghan Tsai <tsunghan_tsai@richtek.com>");
MODULE_DESCRIPTION("ALSA SoC Universal4415 RT5511");
MODULE_LICENSE("GPL");
