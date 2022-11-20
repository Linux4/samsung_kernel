/*
 * Universal7870-RT5665 Audio Machine driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#define DEBUG

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
#include <sound/exynos-audmixer.h>

#include "i2s.h"
#include "i2s-regs.h"

#include "../codecs/rt5665.h"
#include <sound/jack.h>
#include <uapi/linux/input.h>
#include <linux/mfd/syscon.h>
#include <linux/iio/consumer.h>

#include "jack_rt5665_sysfs_cb.h"

#define CODEC_BFS_48KHZ		32
#define CODEC_RFS_48KHZ		512

#define CODEC_BFS_192KHZ		64
#define CODEC_RFS_192KHZ		128

#define CODEC_SAMPLE_RATE_48KHZ		48000
#define CODEC_PLL_48KHZ				24576000
#define CODEC_SAMPLE_RATE_192KHZ	192000
#define CODEC_PLL_192KHZ			49152000

#define RT5665_MCLK_FREQ	26000000
#define RT5665_DAI_ID		16
#define RT5665_DAI_OFFSET	13
#define RT5665_CODEC_MAX	22
#define RT5665_AUX_MAX		2

#define JOSHUA_MCLK_FREQ 26000000

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#define RX_SRAM_SIZE            (0x2000)        /* 8 KB */
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err
#endif

#define DEBUG

#define EXYNOS_PMU_PMU_DEBUG_OFFSET		0x0A00
struct regmap *pmuregmap;

static struct snd_soc_card universal7870_rt5665_card;

static struct clk *xclkout;

struct rt5665_drvdata {
	struct device *dev;
	struct snd_soc_codec *codec;
	int aifrate;
	bool use_external_jd;
	struct snd_soc_jack rt5665_headset;

	int abox_vss_state;
	unsigned int pcm_state;
	struct wake_lock wake_lock;
	unsigned int wake_lock_switch;
};

static struct rt5665_drvdata universal7870_drvdata;

enum {
	RT5665_MICBIAS1,
	RT5665_MICBIAS2,
};

static const struct snd_soc_component_driver universal7870_rt5665_cmpnt = {
	.name = "Universal7870-rt5665-audio",
};

static int universal7870_aif1_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *amixer_dai = rtd->codec_dais[0];
	struct snd_soc_dai *codec_dai = rtd->codec_dais[1];
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;
	int rfs, bfs;

	dev_info(card->dev, "%s-%d %dch, %dHz, %dbytes\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params),
			params_buffer_bytes(params));

	if (params_rate(params) == CODEC_SAMPLE_RATE_192KHZ) {
		rfs = CODEC_RFS_192KHZ;
		bfs = CODEC_BFS_192KHZ;
	} else {
		rfs = CODEC_RFS_48KHZ;
		bfs = CODEC_BFS_48KHZ;
	}

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 codec fmt: %d\n", ret);
		return ret;
	}

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 cpu fmt: %d\n", ret);
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

	ret = snd_soc_dai_set_bclk_ratio(amixer_dai, bfs);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to configure mixer\n");
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5665_PLL1_S_BCLK1,
		params_rate(params) * 32 * 2, params_rate(params) * 512);

	if (ret < 0) {
		dev_err(card->dev, "codec_dai pll not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5665_SCLK_S_PLL1,
		params_rate(params) * 512, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return ret;
	}

	return ret;

}

static int universal7870_aif2_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *amixer_dai = rtd->codec_dais[0];
	struct snd_soc_dai *codec_dai = rtd->codec_dais[1];
	struct snd_interval *rate =
			hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	int bfs, ret;

	dev_info(card->dev, "aif2: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	dev_info(card->dev, "Fix up the rate to 48KHz\n");
	rate->min = rate->max = 48000;

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

	ret = snd_soc_dai_set_bclk_ratio(amixer_dai, bfs);
	if (ret < 0) {
		dev_err(card->dev, "aif2: Failed to configure mixer\n");
		return ret;
	}

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif2 codec fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5665_PLL1_S_BCLK1,
		params_rate(params) * 32 * 2, params_rate(params) * 512);
	if (ret < 0) {
		dev_err(card->dev, "codec_dai pll not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5665_SCLK_S_PLL1,
		params_rate(params) * 512, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return ret;
	}

	return 0;
}

static int universal7870_aif3_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *amixer_dai = rtd->codec_dais[0];
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

	ret = snd_soc_dai_set_bclk_ratio(amixer_dai, bfs);
	if (ret < 0) {
		dev_err(card->dev, "aif3: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7870_aif4_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *amixer_dai = rtd->codec_dais[0];
	struct snd_soc_dai *codec_dai = rtd->codec_dais[1];
	struct snd_soc_dai *amp_l_dai = rtd->codec_dais[2];
	struct snd_soc_dai *amp_r_dai = rtd->codec_dais[3];
	int ret;
	int rfs, bfs;

	dev_info(card->dev, "aif4: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	if (params_rate(params) == CODEC_SAMPLE_RATE_192KHZ) {
		rfs = CODEC_RFS_192KHZ;
		bfs = CODEC_BFS_192KHZ;
	} else {
		rfs = CODEC_RFS_48KHZ;
		bfs = CODEC_BFS_48KHZ;
	}

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif4 codec fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(amp_l_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set left amp dai fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(amp_r_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set right amp dai fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif4: Failed to set CPU DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
				rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(card->dev, "aif4: Failed to set SAMSUNG_I2S_CDCLK\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
						0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "aif4: Failed to set SAMSUNG_I2S_OPCLK\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0) {
		dev_err(card->dev,
				"aif4: Failed to set SAMSUNG_I2S_RCLKSRC_1\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0) {
		dev_err(card->dev, "aif4: Failed to set BFS\n");
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai,
			SAMSUNG_I2S_DIV_RCLK, rfs);
	if (ret < 0) {
		dev_err(card->dev, "aif4: Failed to set RFS\n");
		return ret;
	}

	ret = snd_soc_dai_set_bclk_ratio(amixer_dai, bfs);
	if (ret < 0) {
		dev_err(card->dev, "aif4: Failed to configure mixer\n");
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5665_PLL1_S_BCLK1,
		params_rate(params) * 32 * 2, params_rate(params) * 512);

	if (ret < 0) {
		dev_err(card->dev, "codec_dai pll not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5665_SCLK_S_PLL1,
		params_rate(params) * 512, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_pll(amp_l_dai, 0, 0,
		params_rate(params) * 32 * 2, params_rate(params) * 512);

	if (ret < 0) {
		dev_err(card->dev, "amp_l_dai pll not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(amp_l_dai, 0,
		params_rate(params) * 512, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(card->dev, "amp_l_dai clock not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_pll(amp_r_dai, 0, 0,
		params_rate(params) * 32 * 2, params_rate(params) * 512);

	if (ret < 0) {
		dev_err(card->dev, "amp_r_dai pll not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(amp_r_dai, 0,
		params_rate(params) * 512, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(card->dev, "amp_r_dai clock not set\n");
		return ret;
	}

	return 0;
}

static int universal7870_aif1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif1: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dev_info(card->dev, "%s MICBIAS2 enable\n", __func__);
		snd_soc_dapm_force_enable_pin(&card->dapm, "MICBIAS2");
		snd_soc_dapm_sync(&card->dapm);
	}

	return 0;
}

void universal7870_rt5665_aif1_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif1: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dev_info(card->dev, "%s MICBIAS2 disable\n", __func__);
		snd_soc_dapm_disable_pin(&card->dapm, "MICBIAS2");
		snd_soc_dapm_sync(&card->dapm);
	}
}

static int universal7870_aif2_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif2: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dev_info(card->dev, "%s MICBIAS2 enable\n", __func__);
		snd_soc_dapm_force_enable_pin(&card->dapm, "MICBIAS2");
		snd_soc_dapm_sync(&card->dapm);
	}

	return 0;
}

void universal7870_rt5665_aif2_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif2: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dev_info(card->dev, "%s MICBIAS2 disable\n", __func__);
		snd_soc_dapm_disable_pin(&card->dapm, "MICBIAS2");
		snd_soc_dapm_sync(&card->dapm);
	}
}

static int universal7870_aif3_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif3: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

void universal7870_rt5665_aif3_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif3: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

}

static int universal7870_aif4_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif4: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

void universal7870_aif4_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif4: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);
}

static int universal7870_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec_dais[1]->codec;
	struct rt5665_drvdata *drvdata = snd_soc_card_get_drvdata(card);
	struct snd_soc_jack *jack;
	int ret;

	dev_info(card->dev, "%s\n", __func__);

	drvdata->codec = codec;

	/* close codec device immediately when pcm is closed */
	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUT");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT1");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT2");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADSETMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "MAINMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SUBMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "RECEIVER");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPEAKER");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1_1 Capture");
	snd_soc_dapm_sync(&codec->dapm);

	ret = snd_soc_jack_new(codec, "Headset Jack",
								SND_JACK_HEADSET, &drvdata->rt5665_headset);

	if (ret) {
		dev_err(card->dev, "Headset Jack creation failed %d\n", ret);
		return ret;
	}

	jack = &drvdata->rt5665_headset;
	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_MEDIA);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);
	rt5665_set_jack_detect(codec, &drvdata->rt5665_headset);

	register_rt5665_jack_cb(codec);

	return 0;
}

static int audmixer_rt5665_init(struct snd_soc_component *cmp)
{
	dev_dbg(cmp->dev, "%s called\n", __func__);

	return 0;
}


static int universal7870_headsetmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int universal7870_mainmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

/*
 * this point dapm enable/disable are cannot set micbias
 *
	if (event == SND_SOC_DAPM_PRE_PMU) {
		dev_info(card->dev, "%s MICBIAS2 enable\n", __func__);
		snd_soc_dapm_force_enable_pin(&card->dapm, "MICBIAS2");
		snd_soc_dapm_sync(&card->dapm);
	} else if (event == SND_SOC_DAPM_POST_PMD) {
		dev_info(card->dev, "%s MICBIAS2 disable\n", __func__);
		snd_soc_dapm_disable_pin(&card->dapm, "MICBIAS2");
		snd_soc_dapm_sync(&card->dapm);
	}
*/

	return 0;
}

static int universal7870_submic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int universal7870_receiver(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int universal7870_headphone(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int universal7870_speaker(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static const struct snd_kcontrol_new universal7870_rt5665_controls[] = {
	SOC_DAPM_PIN_SWITCH("HEADPHONE"),
	SOC_DAPM_PIN_SWITCH("SPEAKER"),
	SOC_DAPM_PIN_SWITCH("RECEIVER"),
	SOC_DAPM_PIN_SWITCH("MAINMIC"),
	SOC_DAPM_PIN_SWITCH("SUBMIC"),
	SOC_DAPM_PIN_SWITCH("HEADSETMIC"),
};

const struct snd_soc_dapm_widget universal7870_rt5665_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("VOUTPUT"),
	SND_SOC_DAPM_INPUT("VINPUT1"),
	SND_SOC_DAPM_INPUT("VINPUT2"),
	SND_SOC_DAPM_OUTPUT("VOUTPUTCALL"),
	SND_SOC_DAPM_INPUT("VINPUTCALL"),
	SND_SOC_DAPM_MIC("HEADSETMIC", universal7870_headsetmic),
	SND_SOC_DAPM_MIC("MAINMIC", universal7870_mainmic),
	SND_SOC_DAPM_MIC("SUBMIC", universal7870_submic),
	SND_SOC_DAPM_SPK("RECEIVER", universal7870_receiver),
	SND_SOC_DAPM_HP("HEADPHONE", universal7870_headphone),
	SND_SOC_DAPM_SPK("SPEAKER", universal7870_speaker),

};


const struct snd_soc_dapm_route universal7870_rt5665_dapm_routes[] = {
	{ "HEADPHONE", NULL, "HPOL" },
	{ "HEADPHONE", NULL, "HPOR" },
	{ "RECEIVER", NULL, "MONOOUT" },
	{ "SPEAKER", NULL, "SPKL Playback" },
	{ "SPEAKER", NULL, "SPKR Playback" },
	{ "IN1P", NULL, "HEADSETMIC" },
	{ "IN1N", NULL, "HEADSETMIC" },
	{ "IN3P", NULL, "MICBIAS2" },
	{ "IN3P", NULL, "Main Mic" },
	{ "IN3N", NULL, "Main Mic" },
};

static struct snd_soc_ops universal7870_aif1_ops = {
	.hw_params = universal7870_aif1_hw_params,
	.startup = universal7870_aif1_startup,
	.shutdown = universal7870_rt5665_aif1_shutdown,
};

static struct snd_soc_ops universal7870_aif2_ops = {
	.hw_params = universal7870_aif2_hw_params,
	.startup = universal7870_aif2_startup,
	.shutdown = universal7870_rt5665_aif2_shutdown,
};

static struct snd_soc_ops universal7870_aif3_ops = {
	.hw_params = universal7870_aif3_hw_params,
	.startup = universal7870_aif3_startup,
	.shutdown = universal7870_rt5665_aif3_shutdown,
};

static struct snd_soc_ops universal7870_aif4_ops = {
	.hw_params = universal7870_aif4_hw_params,
	.startup = universal7870_aif4_startup,
	.shutdown = universal7870_aif4_shutdown,
};

static struct snd_soc_dai_driver universal7870_ext_dai[] = {
	{
		.name = "universal7870 voice call",
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
		.name = "universal7870 BT",
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
};

static struct snd_soc_dai_link_component codecs_ap0[] = {
	{
		.name = "14880000.s1402x",
		.dai_name = "AP0",
	}, {
		.dai_name = "rt5665-aif1_1",
	},
};

static struct snd_soc_dai_link_component codecs_cp0[] = {
	{
		.name = "14880000.s1402x",
		.dai_name = "CP0",
	}, {
		.name = "rt5665.6-001b",
		.dai_name = "rt5665-aif1_1",
	}, {
		.name = "sma1303.4-001e",
		.dai_name = "sma1303-amplifier",
	}, {
		.name = "sma1303.4-007e",
		.dai_name = "sma1303-amplifier",
	},
};

static struct snd_soc_dai_link_component codecs_bt[] = {
	{
		.name = "14880000.s1402x",
		.dai_name = "BT",
	}, {
		.dai_name = "dummy-aif2",
	},
};

static struct snd_soc_dai_link_component codecs_ap1[] = {
	{
		.name = "14880000.s1402x",
		.dai_name = "AP0",
	}, {
		.name = "rt5665.6-001b",
		.dai_name = "rt5665-aif1_1",
	}, {
		.name = "sma1303.4-001e",
		.dai_name = "sma1303-amplifier",
	}, {
		.name = "sma1303.4-007e",
		.dai_name = "sma1303-amplifier",
	},
};

static struct snd_soc_dai_link universal7870_rt5665_dai[] = {
	/* Playback and Recording */
	{
		.name = "universal7870-rt5665-pri",
		.stream_name = "i2s0-pri",
		.codecs = codecs_ap0,
		.num_codecs = ARRAY_SIZE(codecs_ap0),
		.ops = &universal7870_aif1_ops,
		.ignore_pmdown_time = true,
	},

	/* Deep buffer playback */
	{
		.name = "universal7870-rt5665-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.stream_name = "i2s0-sec",
		.platform_name = "samsung-i2s-sec",
		.codecs = codecs_ap1,
		.num_codecs = ARRAY_SIZE(codecs_ap1),
		.ops = &universal7870_aif4_ops,
		.ignore_pmdown_time = true,
	},

	/* Voice Call */
	{
		.name = "cp",
		.stream_name = "voice call",
		.cpu_dai_name = "universal7870 voice call",
		.platform_name = "snd-soc-dummy",
		.codecs = codecs_cp0,
		.num_codecs = ARRAY_SIZE(codecs_cp0),
		.ops = &universal7870_aif2_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
	},

	/* BT */
	{
		.name = "bt",
		.stream_name = "bluetooth audio",
		.cpu_dai_name = "universal7870 BT",
		.platform_name = "snd-soc-dummy",
		.codecs = codecs_bt,
		.num_codecs = ARRAY_SIZE(codecs_bt),
		.ops = &universal7870_aif3_ops,
		.ignore_suspend = 1,
	},

	/* AMP AP Interface */
	{
		.name = "universal7870-rt5665-amp",
		.stream_name = "i2s0-pri",
		.codecs = codecs_ap1,
		.num_codecs = ARRAY_SIZE(codecs_ap1),
		.ops = &universal7870_aif4_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
	},

	/* SW MIXER1 Interface */
	{
		.name = "playback-eax0",
		.stream_name = "eax0",
		.cpu_dai_name = "samsung-eax.0",
		.platform_name = "samsung-eax.0",
		.codecs = codecs_ap1,
		.num_codecs = ARRAY_SIZE(codecs_ap1),
		.ops = &universal7870_aif4_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
	},

	/* SW MIXER2 Interface */
	{
		.name = "playback-eax1",
		.stream_name = "eax1",
		.cpu_dai_name = "samsung-eax.1",
		.platform_name = "samsung-eax.1",
		.codecs = codecs_ap1,
		.num_codecs = ARRAY_SIZE(codecs_ap1),
		.ops = &universal7870_aif4_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
	},

	/* SW MIXER3 Interface */
	{
		.name = "playback-eax2",
		.stream_name = "eax2",
		.cpu_dai_name = "samsung-eax.2",
		.platform_name = "samsung-eax.2",
		.codecs = codecs_ap1,
		.num_codecs = ARRAY_SIZE(codecs_ap1),
		.ops = &universal7870_aif4_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
	},

	/* SW MIXER4 Interface */
	{
		.name = "playback-eax3",
		.stream_name = "eax3",
		.cpu_dai_name = "samsung-eax.3",
		.platform_name = "samsung-eax.3",
		.codecs = codecs_ap1,
		.num_codecs = ARRAY_SIZE(codecs_ap1),
		.ops = &universal7870_aif4_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = true,
	},
};

static struct snd_soc_aux_dev audmixer_aux_dev[] = {
	{
		.init = audmixer_rt5665_init,
	},
};

static struct snd_soc_codec_conf codec_conf[] = {
	{
		.name_prefix = "AudioMixer",
	},
	{
		.name_prefix = NULL,
	},
	{
		.name_prefix = "SPKL",
	},
	{
		.name_prefix = "SPKR",
	},
};

static void control_xclkout(bool on)
{
	if (on) {
		clk_prepare_enable(xclkout);
	} else
		clk_disable_unprepare(xclkout);
}

static struct snd_soc_card universal7870_rt5665_card = {
	.name = "Universal7870-RT5665",
	.owner = THIS_MODULE,

	.dai_link = universal7870_rt5665_dai,
	.num_links = ARRAY_SIZE(universal7870_rt5665_dai),

	.controls = universal7870_rt5665_controls,
	.num_controls = ARRAY_SIZE(universal7870_rt5665_controls),
	.dapm_widgets = universal7870_rt5665_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal7870_rt5665_dapm_widgets),
	.dapm_routes = universal7870_rt5665_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal7870_rt5665_dapm_routes),

	.drvdata = (void *)&universal7870_drvdata,

	.late_probe = universal7870_late_probe,
	.aux_dev = audmixer_aux_dev,
	.num_aux_devs = ARRAY_SIZE(audmixer_aux_dev),

	.codec_conf = codec_conf,
	.num_configs = ARRAY_SIZE(codec_conf),

};

static int universal7870_rt5665_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_np, *codec_np, *auxdev_np;
	struct snd_soc_card *card = &universal7870_rt5665_card;
	int of_route;

	if (!np) {
		dev_err(&pdev->dev, "Failed to get device node\n");
		return -EINVAL;
	}

	card->dev = &pdev->dev;
	card->num_links = 0;

	xclkout = devm_clk_get(&pdev->dev, "xclkout");
	if (IS_ERR(xclkout)) {
		dev_err(&pdev->dev, "xclkout get failed\n");
		xclkout = NULL;
		goto err_clk;
	} else {
		dev_info(&pdev->dev, "xclkout is enabled\n");
		control_xclkout(true);
	}

	ret = snd_soc_register_component(card->dev, &universal7870_rt5665_cmpnt,
			universal7870_ext_dai,
			ARRAY_SIZE(universal7870_ext_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
		return ret;
	}

	of_route = of_property_read_bool(np, "samsung,audio-routing");
	if (!card->dapm_routes || !card->num_dapm_routes || of_route) {
		ret = snd_soc_of_parse_audio_routing(card,
				"samsung,audio-routing");
		if (ret != 0) {
			dev_err(&pdev->dev,
				"Failed to parse audio routing: %d\n",
				ret);
			ret = -EINVAL;
			goto err_audio_route;
		}
	}

	for (n = 0; n < ARRAY_SIZE(universal7870_rt5665_dai); n++) {
		/* Skip parsing DT for fully formed dai links */
		if (universal7870_rt5665_dai[n].platform_name &&
				universal7870_rt5665_dai[n].codec_name) {
			dev_dbg(card->dev,
			"Skipping dt for populated dai link %s\n",
			universal7870_rt5665_dai[n].name);
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

		if ((strcmp("universal7870-rt5665-sec",
				universal7870_rt5665_dai[n].name) != 0)
			&& (strcmp("universal7870-rt5665-amp",
				universal7870_rt5665_dai[n].name) != 0)
			&& (strcmp("playback-eax0",
				universal7870_rt5665_dai[n].name) != 0)
			&& (strcmp("playback-eax1",
				universal7870_rt5665_dai[n].name) != 0)
			&& (strcmp("playback-eax2",
				universal7870_rt5665_dai[n].name) != 0)
			&& (strcmp("playback-eax3",
				universal7870_rt5665_dai[n].name) != 0)
			&& (strcmp("cp", universal7870_rt5665_dai[n].name) != 0)) {
			universal7870_rt5665_dai[n].codecs[1].of_node = codec_np;
		}

		if (!universal7870_rt5665_dai[n].cpu_dai_name)
			universal7870_rt5665_dai[n].cpu_of_node = cpu_np;
		if (!universal7870_rt5665_dai[n].platform_name)
			universal7870_rt5665_dai[n].platform_of_node = cpu_np;

		card->num_links++;
	}

	for (n = 0; n < ARRAY_SIZE(codec_conf); n++) {
		codec_conf[n].of_node = of_parse_phandle(np, "samsung,codec", n);

		if (!codec_conf[n].of_node) {
			dev_err(&pdev->dev,
				"Property 'samsung,codec' missing\n");
			goto err_audio_route;
		}
	}

	for (n = 0; n < ARRAY_SIZE(audmixer_aux_dev); n++) {
		auxdev_np = of_parse_phandle(np, "samsung,auxdev", n);
		if (!auxdev_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,auxdev' missing\n");
			ret = -EINVAL;
			goto err_audio_route;
		}

		audmixer_aux_dev[n].codec_of_node = auxdev_np;
	}

	pmuregmap = syscon_regmap_lookup_by_phandle(np,
			"samsung,syscon-phandle");
	if (IS_ERR(pmuregmap)) {
		dev_err(&pdev->dev, "syscon regmap lookup failed.\n");
		pmuregmap = NULL;
	}

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);

	return ret;

err_audio_route:
	snd_soc_unregister_component(card->dev);
err_clk:
	return ret;
}

static int universal7870_rt5665_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_component(card->dev);
	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id universal7870_rt5665_of_match[] = {
	{.compatible = "samsung,universal7870-rt5665",},
	{},
};
MODULE_DEVICE_TABLE(of, universal7870_rt5665_of_match);

static struct platform_driver universal7870_rt5665_audio_driver = {
	.driver = {
		.name = "Universal7870-rt5665-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(universal7870_rt5665_of_match),
	},
	.probe = universal7870_rt5665_audio_probe,
	.remove = universal7870_rt5665_audio_remove,
};

module_platform_driver(universal7870_rt5665_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Universal7870 RT5665");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:universal7870-audio-rt5665");