/* sound/soc/samsung/universal5430_rt5511.c
 * RT5511 on universal5430 machine driver
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
#include <linux/mutex.h>
#include <mach/exynos-audio.h>

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


#define UNIVERSAL5430_DEFAULT_MCLK1	24000000
#define UNIVERSAL5430_DEFAULT_MCLK2	32768
#define UNIVERSAL5430_DEFAULT_SYNC_CLK	12288000

struct rt5511_machine_priv {
	struct snd_soc_codec *codec;
	struct clk *mclk;
	struct mutex sysclk_io;
	
	int sysclk_flag;
	int sysclk_rate;
	int asyncclk_rate;
};

static struct rt5511_machine_priv universal5430_rt5511_priv = {
 	.sysclk_rate = 4915200,
 	.asyncclk_rate = 49152000,
};

static int aif2_mode;

const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static int aif_mclk_ref[2];

const char *aif_mclk_ref_text[] = {
	"MCLK1", "MCLK2"
};

static const struct soc_enum aif_mclk_ref_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif_mclk_ref_text), aif_mclk_ref_text),
};

static const struct snd_soc_component_driver rt5511_cmpnt = {
	.name	= "rt5511-card",
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

static unsigned int get_aif_mclk_ref_index(const char *name)
{
	unsigned int index = 0;
	if (strcmp(name, "AIF1 MCLK Ref") == 0)
		index = 0;
	else
		index = 1;

	return index;
}

static int get_aif_mclk_ref(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int index =
		get_aif_mclk_ref_index(kcontrol->id.name);

	ucontrol->value.integer.value[0] = aif_mclk_ref[index];
	return 0;
}

static int set_aif_mclk_ref(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int new_data;
	unsigned int index;

	new_data = ucontrol->value.integer.value[0];
	index = get_aif_mclk_ref_index(kcontrol->id.name);

	if (aif_mclk_ref[index] == new_data)
		return 0;

	aif_mclk_ref[index] = new_data;

	pr_info("%s (%d): %s\n", __func__, index,
		aif_mclk_ref_text[new_data]);

	return 0;
}

static int universal5430_start_sysclk(struct snd_soc_card *card);

static int universal5430_codec_set_pll( int id,
	struct snd_soc_dai *codec_dai,unsigned int freq_out)
{
	int source;
	unsigned int freq_in;

	if (aif_mclk_ref[id]==0) {
		source = RT5511_PLL_SRC_MCLK1;
		freq_in = UNIVERSAL5430_DEFAULT_MCLK1;
	}
	else {
		source = RT5511_PLL_SRC_MCLK2;
		freq_in = UNIVERSAL5430_DEFAULT_MCLK2;
	}

	return snd_soc_dai_set_pll(
		codec_dai, RT5511_PLL, source, freq_in, freq_out);
}

static int universal5430_rt5511_aif1_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	int prate = params_rate(params);
	unsigned int pll_out;
	int ret;

	dev_info(codec_dai->dev, "%s %dHz, %dch\n", __func__, prate,
				params_channels(params));

	universal5430_start_sysclk(card);
	dev_info(card->dev, "%s: start_sysclk\n", __func__);

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
	ret = universal5430_codec_set_pll(0,codec_dai,pll_out);
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

static int universal5430_rt5511_aif2_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	int prate = params_rate(params);
	int bclk;
	int ret;

	dev_info(codec_dai->dev, "%s %dHz, %dch\n", __func__, prate,
				params_channels(params));

	universal5430_start_sysclk(card);
	dev_info(card->dev, "%s: start_sysclk\n", __func__);

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

		snd_soc_update_bits(rtd->codec, RT5511_AIF_CTRL1,
			AIF1_MS_SEL_MASK, AIF_MS_REF_MCLK << AIF1_MS_SEL_SHIFT);

		ret = snd_soc_dai_set_pll(codec_dai, RT5511_PLL,
					RT5511_PLL_SRC_BCLK1,
					bclk, prate * 256);
	} else {
		RT_DBG("AIF2:Master, Ref:AIF2\n");

		ret = universal5430_codec_set_pll(1,codec_dai,prate * 256);
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

static int universal5430_rt5511_aif3_hw_params(
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
 * universal5430 RT5511 DAI operations.
 */
static struct snd_soc_ops universal5430_rt5511_aif1_ops = {
	.hw_params = universal5430_rt5511_aif1_hw_params,
};

static struct snd_soc_ops universal5430_rt5511_aif2_ops = {
	.hw_params = universal5430_rt5511_aif2_hw_params,
};

static struct snd_soc_ops universal5430_rt5511_aif3_ops = {
	.hw_params = universal5430_rt5511_aif3_hw_params,
};

static const struct snd_kcontrol_new universal5430_codec_controls[] = {
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),

	SOC_ENUM_EXT("AIF1 MCLK Ref", aif_mclk_ref_enum[0],
		get_aif_mclk_ref, set_aif_mclk_ref),
	SOC_ENUM_EXT("AIF2 MCLK Ref", aif_mclk_ref_enum[0],
		get_aif_mclk_ref, set_aif_mclk_ref),
};

static const struct snd_kcontrol_new universal5430_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget universal5430_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
};

const struct snd_soc_dapm_route universal5430_dapm_routes[] = {
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

static struct snd_soc_dai_driver universal5430_ext_dai[] = {
	{
		.name = "universal5430.cp",
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
		.name = "universal5430.bt",
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
		.name = "nxptfa95.0",
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

static struct snd_soc_dai_link universal5430_dai[] = {
	{ /* Primary DAI i/f */
		.name = "RT5511 AIF1",
		.stream_name = "Pri_Dai",
//		.cpu_dai_name = "samsung-i2s-sec",
		.codec_dai_name = RT5511_CODEC_DAI2_NAME,
//		.platform_name = "samsung-i2s-sec",
		//.codec_name = RT5511_CODEC_NAME,
		.ops = &universal5430_rt5511_aif1_ops,
	},
	{
		.name = "RT5511 Voice",
		.stream_name = "Voice Tx/Rx",
		.cpu_dai_name = "universal5430.cp",
		.codec_dai_name = RT5511_CODEC_DAI1_NAME,
		.platform_name = "snd-soc-dummy",
		//.codec_name = RT5511_CODEC_NAME,
		.ops = &universal5430_rt5511_aif2_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "RT5511 BT",
		.stream_name = "BT Tx/Rx",
		.cpu_dai_name = "universal5430.bt",
		.codec_dai_name = RT5511_CODEC_DAI3_NAME,
		.platform_name = "snd-soc-dummy",
		//.codec_name = RT5511_CODEC_NAME,
		.ops = &universal5430_rt5511_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s-sec",
		.codec_dai_name = RT5511_CODEC_DAI2_NAME,
#ifndef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-i2s-sec",
#else
		.platform_name = "samsung-idma",
#endif
		//.codec_name = RT5511_CODEC_NAME,
		.ops = &universal5430_rt5511_aif1_ops,
	},
	{
		.name = "NXP",
		.stream_name = "NXP SPEAK",
		.cpu_dai_name = "nxptfa95.0",
		.codec_dai_name = RT5511_CODEC_DAI3_NAME,
		.platform_name = "snd-soc-dummy",
		//.codec_name = RT5511_CODEC_NAME,
		.ops = &universal5430_rt5511_aif3_ops,
		.ignore_suspend = 1,
	},
};

static int universal5430_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
    struct rt5511_machine_priv *priv = card->drvdata;
	int ret;

	priv->codec = codec;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	/* close codec device immediately when pcm is closed */
	codec->ignore_pmdown_time = true;

	ret = snd_soc_add_codec_controls(codec, universal5430_codec_controls,
		ARRAY_SIZE(universal5430_codec_controls));
	if (ret < 0) {
		dev_err(codec->dev,
			"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	snd_soc_dapm_ignore_suspend(&card->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HP");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");
	snd_soc_dapm_sync(&codec->dapm);

	return 0;
}

static int universal5430_start_sysclk(struct snd_soc_card *card)
{
	struct rt5511_machine_priv *rt5511 = card->drvdata;

	mutex_lock(&rt5511->sysclk_io);
	if (!rt5511->sysclk_flag) {
		if (rt5511->mclk) {
			clk_enable(rt5511->mclk);
			dev_info(card->dev, "mclk enabled\n");
		} else
			exynos5_audio_set_mclk(true, 0);
		rt5511->sysclk_flag = 1;
	}
	mutex_unlock(&rt5511->sysclk_io);

	return 0;
}

static int universal5430_stop_sysclk(struct snd_soc_card *card)
{
	struct rt5511_machine_priv *rt5511 = card->drvdata;

	mutex_lock(&rt5511->sysclk_io);
	if (rt5511->sysclk_flag) {
		if (rt5511->mclk) {
			clk_disable(rt5511->mclk);
			dev_info(card->dev, "mclk disbled\n");
		} else
			exynos5_audio_set_mclk(false, 0);
		rt5511->sysclk_flag = 0;
	}
	mutex_unlock(&rt5511->sysclk_io);

	return 0;
}

static int universal5430_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct rt5511_machine_priv *rt5511 = card->drvdata;

	if (!rt5511->codec || dapm != &rt5511->codec->dapm)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		if (card->dapm.bias_level == SND_SOC_BIAS_OFF)
			universal5430_start_sysclk(card);
		break;
	case SND_SOC_BIAS_OFF:
		universal5430_stop_sysclk(card);
		break;
	default:
		break;
	}

	card->dapm.bias_level = level;
	dev_info(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int universal5430_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int universal5430_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;

	snd_soc_dai_set_tristate(aif1_dai, 1);

	return 0;
}

static int universal5430_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;

	snd_soc_dai_set_tristate(aif1_dai, 0);

	return 0;
}

static struct snd_soc_card universal5430 = {
	.name = "universal5430 RT5511",
	.owner = THIS_MODULE,
	.dai_link = universal5430_dai,

	/* If you want to use sec_fifo device,
	 * changes the num_link = 2 or ARRAY_SIZE(universal222ap_dai). */
	.num_links = ARRAY_SIZE(universal5430_dai),

	.controls = universal5430_controls,
	.num_controls = ARRAY_SIZE(universal5430_controls),
	.dapm_widgets = universal5430_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal5430_dapm_widgets),
	.dapm_routes = universal5430_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal5430_dapm_routes),

	.late_probe = universal5430_late_probe,

	.suspend_post = universal5430_card_suspend_post,
	.resume_pre = universal5430_card_resume_pre,

	.set_bias_level = universal5430_set_bias_level,
	.set_bias_level_post = universal5430_set_bias_level_post,

	.drvdata = &universal5430_rt5511_priv,
};

static int __devinit snd_universal5430_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np, *cpu_np;
	struct snd_soc_card *card = &universal5430;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct rt5511_machine_priv *rt5511;

	printk("snd_universal5430_probe start\n");

	aif_mclk_ref[0] = 0;
	aif_mclk_ref[1] = 1;

	card->dev = &pdev->dev;
	rt5511 = card->drvdata;

	mutex_init(&rt5511->sysclk_io);
	rt5511->sysclk_flag = 0; /* init sysclk_flag */

	rt5511->mclk = devm_clk_get(card->dev, "mclk");
	if (IS_ERR(rt5511->mclk)) {
		dev_dbg(card->dev, "Device tree node not found for mclk");
		rt5511->mclk = NULL;
 	} else
 		clk_prepare(rt5511->mclk);

	ret = snd_soc_register_component(card->dev, &rt5511_cmpnt,
				universal5430_ext_dai, ARRAY_SIZE(universal5430_ext_dai));
	if (ret != 0)
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);

	if (np) {
		for (n = 0; n < card->num_links; n++) {

			/* Skip parsing DT for fully formed dai links */
			if (dai_link[n].platform_name &&
			    dai_link[n].codec_name) {
				dev_dbg(card->dev,
					"Skipping dt for populated dai link %s\n",
					dai_link[n].name);
				continue;
			}

			cpu_np = of_parse_phandle(np,
					"samsung,audio-cpu", n);
			if (!cpu_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-cpu'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			codec_np = of_parse_phandle(np,
					"samsung,audio-codec", n);
			if (!codec_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-codec'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			if (!dai_link[n].cpu_dai_name)
				dai_link[n].cpu_of_node = cpu_np;

			if (!dai_link[n].platform_name)
				dai_link[n].platform_of_node = cpu_np;

			dai_link[n].codec_of_node = codec_np;
		}
	} else
		dev_err(&pdev->dev, "Failed to get device node\n");

	ret = snd_soc_register_card(&universal5430);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
		goto out;
	}
	
	printk("snd_universal5430_probe done\n");
	return ret;

out:
	snd_soc_unregister_component(card->dev);
	return ret;
}

static int __devexit snd_universal5430_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct rt5511_machine_priv *rt5511 = card->drvdata;

	snd_soc_unregister_component(card->dev);
	snd_soc_unregister_card(card);

	mutex_destroy(&rt5511->sysclk_io);	

	if (rt5511->mclk) {
		clk_unprepare(rt5511->mclk);
		devm_clk_put(card->dev, rt5511->mclk);
	}

	return 0;
}

static const struct of_device_id universal5430_rt5511_of_match[] = {
	{ .compatible = "richtek,rt5511", },
	{ },
};
MODULE_DEVICE_TABLE(of, universal5430_rt5511_of_match);

static struct platform_driver snd_universal5430_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "rt5511-card",
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(universal5430_rt5511_of_match),
	},
	.probe = snd_universal5430_probe,
	.remove = __devexit_p(snd_universal5430_remove),
};
module_platform_driver(snd_universal5430_driver);

MODULE_AUTHOR("Tsunghan Tsai <tsunghan_tsai@richtek.com>");
MODULE_DESCRIPTION("ALSA SoC universal5430 RT5511");
MODULE_LICENSE("GPL");
