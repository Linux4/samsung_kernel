/*
 *  picasso_prince.c -- Machine Driver for Prince AMPs on Picasso
 *
 *  Copyright 2013 Wolfson Microelectronics
 *  Copyright 2016 Cirrus Logic
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/wakelock.h>

#include <soc/samsung/exynos-pmu.h>
#include <sound/samsung/abox.h>

#include <linux/mfd/cs35l41/big_data.h>
#include "bigdata_cirrus_sysfs_cb.h"

#include <soc/samsung/exynos-cpupm.h>

#define MADERA_BASECLK_48K	49152000
#define MADERA_BASECLK_44K1	45158400

#define PRINCE_AMP_RATE	48000
#define PRINCE_AMP_BCLK	(PRINCE_AMP_RATE * 16 * 4)

#define CLK_SRC_SCLK 0
#define CLK_SRC_LRCLK 1
#define CLK_SRC_PDM 2
#define CLK_SRC_SELF 3
#define CLK_SRC_MCLK 4
#define CLK_SRC_SWIRE 5

#define CLK_SRC_DAI 0
#define CLK_SRC_CODEC 1

#define MADERA_DAI_ID			0x4793
#define CS35L41_DAI_ID			0x3541
#define ABOX_BE_DAI_ID(c, i)		(0xbe00 | (c) << 4 | (i))
#define ABOX_CODEC_MAX		32
#define ABOX_AUX_MAX			2
#define RDMA_COUNT			12
#define WDMA_COUNT			5
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
#define VTS_COUNT			2
#else
#define VTS_COUNT			0
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
#define DP_COUNT			2
#else
#define DP_COUNT			0
#endif
#define DDMA_COUNT			6
#define DUAL_COUNT			WDMA_COUNT
#define UAIF_START			(RDMA_COUNT + WDMA_COUNT + VTS_COUNT\
					+ DP_COUNT + DDMA_COUNT + DUAL_COUNT)
#define UAIF_COUNT			7

static unsigned int baserate = MADERA_BASECLK_48K;

enum FLL_ID { FLL1, FLL2, FLL3, FLLAO };
enum CLK_ID { SYSCLK, ASYNCCLK, DSPCLK, OPCLK, OUTCLK };

/* Debugfs value overrides, default to 0 */
static unsigned int forced_mclk1;
static unsigned int forced_sysclk;
static unsigned int forced_dspclk;

struct clk_conf {
	int id;
	const char *name;
	int source;
	int rate;
	int fout;

	bool valid;
};

#define MADERA_MAX_CLOCKS 10

struct picasso_drvdata {
	struct device *dev;

	struct clk_conf fll1_refclk;
	struct clk_conf fll2_refclk;
	struct clk_conf fllao_refclk;
	struct clk_conf sysclk;
	struct clk_conf asyncclk;
	struct clk_conf dspclk;
	struct clk_conf opclk;
	struct clk_conf outclk;

	struct notifier_block nb;

	int left_amp_dai;
	int right_amp_dai;
	struct clk *clk[MADERA_MAX_CLOCKS];

	struct wake_lock wake_lock;
	int wake_lock_switch;

	int cpd_disable_state;
};

static struct picasso_drvdata picasso_prince_drvdata;

static int map_fllid_with_name(const char *name)
{
	if (!strcmp(name, "fll1-refclk"))
		return FLL1;
	else if (!strcmp(name, "fll2-refclk"))
		return FLL2;
	else if (!strcmp(name, "fll3-refclk"))
		return FLL3;
	else if (!strcmp(name, "fllao-refclk"))
		return FLLAO;
	else
		return -1;
}

static int map_clkid_with_name(const char *name)
{
	if (!strcmp(name, "sysclk"))
		return SYSCLK;
	else if (!strcmp(name, "asyncclk"))
		return ASYNCCLK;
	else if (!strcmp(name, "dspclk"))
		return DSPCLK;
	else if (!strcmp(name, "opclk"))
		return OPCLK;
	else if (!strcmp(name, "outclk"))
		return OUTCLK;
	else
		return -1;
}

static struct snd_soc_pcm_runtime *get_rtd(struct snd_soc_card *card, int id)
{
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_pcm_runtime *rtd = NULL;

	for (dai_link = card->dai_link;
			dai_link - card->dai_link < card->num_links;
			dai_link++) {
		if (id == dai_link->id) {
			rtd = snd_soc_get_pcm_runtime(card, dai_link->name);
			break;
		}
	}

	return rtd;
}

static int prince_start_fll(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *codec_dai;
	struct snd_soc_component *codec;
	unsigned int fsrc = 0, fin = 0, fout = 0, pll_id;
	int ret;

	if (!config->valid)
		return 0;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = codec_dai->component;

	pll_id = map_fllid_with_name(config->name);
	switch (pll_id) {
	case FLL1:
		if (forced_mclk1) {
			/* use 32kHz input to avoid overclocking the FLL when
			 * forcing a specific MCLK frequency into the codec
			 * FLL calculations
			 */

			fin = forced_mclk1;
		} else {
			fsrc = config->source;
			fin = config->rate;
		}

		if (forced_sysclk)
			fout = forced_sysclk;
		else
			fout = config->fout;
		break;
	case FLL2:
	case FLLAO:
		fsrc = config->source;
		fin = config->rate;
		fout = config->fout;
		break;
	default:
		dev_err(card->dev, "Unknown FLLID for %s\n", config->name);
	}

	dev_dbg(card->dev, "Setting %s fsrc=%d fin=%uHz fout=%uHz\n",
		config->name, fsrc, fin, fout);

	ret = snd_soc_component_set_pll(codec, config->id, fsrc, fin, fout);
	if (ret)
		dev_err(card->dev, "Failed to start %s\n", config->name);

	return ret;
}

static int prince_stop_fll(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *codec_dai;
	struct snd_soc_component *codec;
	int ret;

	if (!config->valid)
		return 0;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = codec_dai->component;

	ret = snd_soc_component_set_pll(codec, config->id, 0, 0, 0);
	if (ret)
		dev_err(card->dev, "Failed to stop %s\n", config->name);

	return ret;
}

static int prince_set_clock(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *aif_dai;
	struct snd_soc_component *codec;
	unsigned int freq = 0, clk_id;
	int ret;
	int dir = SND_SOC_CLOCK_IN;

	if (!config->valid)
		return 0;

	aif_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = aif_dai->component;

	clk_id = map_clkid_with_name(config->name);
	switch (clk_id) {
	case  SYSCLK:
		if (forced_sysclk)
			freq = forced_sysclk;
		else
			if (config->rate)
				freq = config->rate;
			else
				freq = baserate * 2;
		break;
	case ASYNCCLK:
		freq = config->rate;
		break;
	case DSPCLK:
		if (forced_dspclk)
			freq = forced_dspclk;
		else
			if (config->rate)
				freq = config->rate;
			else
				freq = baserate * 3;
		break;
	case OPCLK:
		freq = config->rate;
		dir = SND_SOC_CLOCK_OUT;
		break;
	case OUTCLK:
		freq = config->rate;
		break;
	default:
		dev_err(card->dev, "Unknown Clock ID for %s\n", config->name);
	}

	dev_dbg(card->dev, "Setting %s freq to %u Hz\n", config->name, freq);

	ret = snd_soc_component_set_sysclk(codec, config->id,
				       config->source, freq, dir);
	if (ret)
		dev_err(card->dev, "Failed to set %s to %u Hz\n",
			config->name, freq);

	return ret;
}

static int prince_stop_clock(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *aif_dai;
	struct snd_soc_component *codec;
	int ret;

	if (!config->valid)
		return 0;

	aif_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = aif_dai->component;

	ret = snd_soc_component_set_sysclk(codec, config->id, 0, 0, 0);
	if (ret)
		dev_err(card->dev, "Failed to stop %s\n", config->name);

	return ret;
}

static int prince_set_clocking(struct snd_soc_card *card,
				  struct picasso_drvdata *drvdata)
{
	int ret;

	ret = prince_start_fll(card, &drvdata->fll1_refclk);
	if (ret)
		return ret;

	if (!drvdata->sysclk.rate) {
		ret = prince_set_clock(card, &drvdata->sysclk);
		if (ret)
			return ret;
	}

	if (!drvdata->dspclk.rate) {
		ret = prince_set_clock(card, &drvdata->dspclk);
		if (ret)
			return ret;
	}

	ret = prince_set_clock(card, &drvdata->opclk);
	if (ret)
		return ret;

	return ret;
}

static const struct snd_soc_ops rdma_ops = {
};

static const struct snd_soc_ops wdma_ops = {
};

static int prince_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct picasso_drvdata *drvdata = card->drvdata;
	unsigned int rate = params_rate(params);
	int ret;

	/* Treat sysclk rate zero as automatic mode */
	if (!drvdata->sysclk.rate) {
		if (rate % 4000)
			baserate = MADERA_BASECLK_44K1;
		else
			baserate = MADERA_BASECLK_48K;
	}

	dev_dbg(card->dev, "Requesting Rate: %dHz, FLL: %dHz\n", rate,
		drvdata->sysclk.rate ? drvdata->sysclk.rate : baserate * 2);

	/* Ensure we can't race against set_bias_level */
	mutex_lock_nested(&card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);
	ret = prince_set_clocking(card, drvdata);
	mutex_unlock(&card->dapm_mutex);

	return ret;
}

static const struct snd_soc_ops uaif0_ops = {
	.hw_params = prince_hw_params,
};

static int cs35l41_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai **codec_dais = rtd->codec_dais;
	unsigned int clk;
	unsigned int num_codecs = rtd->num_codecs;
	int ret = 0, i;

	/* using bclk for sysclk */
	clk = snd_soc_params_to_bclk(params);
	for (i = 0; i < num_codecs; i++) {
		ret = snd_soc_component_set_sysclk(codec_dais[i]->component,
					CLK_SRC_SCLK, 0, clk,
					SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev, "%s: set codec sysclk failed: %d\n",
					codec_dais[i]->name, ret);
	}

	return ret;
}

static const struct snd_soc_ops cs35l41_ops = {
	.hw_params = cs35l41_hw_params,
};

static const struct snd_soc_ops uaif_ops = {
};

static int dsif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx_slot[] = {0, 1};

	/* bclk ratio 64 for DSD64, 128 for DSD128 */
	snd_soc_dai_set_bclk_ratio(cpu_dai, 64);

	/* channel map 0 1 if left is first, 1 0 if right is first */
	snd_soc_dai_set_channel_map(cpu_dai, 2, tx_slot, 0, NULL);
	return 0;
}

static const struct snd_soc_ops dsif_ops = {
	.hw_params = dsif_hw_params,
};

static int picasso_set_bias_level(struct snd_soc_card *card,
				  struct snd_soc_dapm_context *dapm,
				  enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai;
	struct picasso_drvdata *drvdata = card->drvdata;
	int ret;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;

	if (!codec_dai)
		return 0;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level != SND_SOC_BIAS_OFF)
			break;

		ret = prince_set_clocking(card, drvdata);
		if (ret)
			return ret;

		ret = prince_start_fll(card, &drvdata->fll2_refclk);
		if (ret)
			return ret;

		ret = prince_start_fll(card, &drvdata->fllao_refclk);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}

static int picasso_set_bias_level_post(struct snd_soc_card *card,
					 struct snd_soc_dapm_context *dapm,
					 enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai;
	struct picasso_drvdata *drvdata = card->drvdata;
	int ret;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_OFF:
		ret = prince_stop_fll(card, &drvdata->fll1_refclk);
		if (ret)
			return ret;

		ret = prince_stop_fll(card, &drvdata->fll2_refclk);
		if (ret)
			return ret;

		ret = prince_stop_fll(card, &drvdata->fllao_refclk);
		if (ret)
			return ret;

		if (!drvdata->sysclk.rate) {
			ret = prince_stop_clock(card, &drvdata->sysclk);
			if (ret)
				return ret;
		}

		if (!drvdata->dspclk.rate) {
			ret = prince_stop_clock(card, &drvdata->dspclk);
			if (ret)
				return ret;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int prince_amp_late_probe(struct snd_soc_card *card, int dai)
{
	struct picasso_drvdata *drvdata = card->drvdata;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *amp_dai;
	struct snd_soc_component *amp;
	int ret;

	if (!dai || !card->dai_link[dai].name)
		return 0;

	if (!drvdata->opclk.valid) {
		dev_err(card->dev, "OPCLK required to use speaker amp\n");
		return -ENOENT;
	}

	rtd = snd_soc_get_pcm_runtime(card, card->dai_link[dai].name);

	amp_dai = rtd->codec_dai;
	amp = amp_dai->component;

	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0x3, 0x3, 4, 16);
	if (ret)
		dev_err(card->dev, "Failed to set TDM: %d\n", ret);

	ret = snd_soc_component_set_sysclk(amp, 0, 0, drvdata->opclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(card->dev, "Failed to set amp SYSCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(amp_dai, 0, PRINCE_AMP_BCLK,
				     SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(card->dev, "Failed to set amp DAI clock: %d\n", ret);
		return ret;
	}

	return 0;
}

static int picasso_late_probe(struct snd_soc_card *card)
{
	struct picasso_drvdata *drvdata = card->drvdata;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *aif_dai;
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dai_link *link;
	const char *name;
	int ret, i;

	aif_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;

	if (drvdata->sysclk.valid) {
		ret = snd_soc_dai_set_sysclk(aif_dai, drvdata->sysclk.id, 0, 0);
		if (ret != 0) {
			dev_err(drvdata->dev, "Failed to set AIF1 clock: %d\n",
				ret);
			return ret;
		}
	}

	if (drvdata->sysclk.rate) {
		ret = prince_set_clock(card, &drvdata->sysclk);
		if (ret)
			return ret;
	}

	if (drvdata->dspclk.rate) {
		ret = prince_set_clock(card, &drvdata->dspclk);
		if (ret)
			return ret;
	}

	ret = prince_set_clock(card, &drvdata->asyncclk);
	if (ret)
		return ret;

	ret = prince_set_clock(card, &drvdata->outclk);
	if (ret)
		return ret;

	ret = prince_amp_late_probe(card, drvdata->left_amp_dai);
	if (ret)
		return ret;

	ret = prince_amp_late_probe(card, drvdata->right_amp_dai);
	if (ret)
		return ret;

	dapm = &card->dapm;
	snd_soc_dapm_ignore_suspend(dapm, "VOUTPUT");
	snd_soc_dapm_ignore_suspend(dapm, "VINPUT1");
	snd_soc_dapm_ignore_suspend(dapm, "VINPUT2");
	snd_soc_dapm_ignore_suspend(dapm, "VOUTPUTCALL");
	snd_soc_dapm_ignore_suspend(dapm, "VINPUTCALL");
	snd_soc_dapm_ignore_suspend(dapm, "HEADSETMIC");
	snd_soc_dapm_ignore_suspend(dapm, "RECEIVER");
	snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "SPEAKER");
	snd_soc_dapm_ignore_suspend(dapm, "BLUETOOTH MIC");
	snd_soc_dapm_ignore_suspend(dapm, "BLUETOOTH SPK");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "VTS Virtual Output");
	snd_soc_dapm_sync(dapm);

	if (IS_ENABLED(CONFIG_SND_SOC_MADERA)) {
		struct snd_soc_component *codec;

		codec = aif_dai->component;

		dapm = snd_soc_component_get_dapm(codec);
		snd_soc_dapm_ignore_suspend(dapm, "AIF1 Playback");
		snd_soc_dapm_ignore_suspend(dapm, "AIF1 Capture");
		snd_soc_dapm_ignore_suspend(dapm, "AUXPDM1");
			snd_soc_dapm_sync(dapm);

	}

	list_for_each_entry(link, &card->dai_link_list, list) {
		rtd = snd_soc_get_pcm_runtime(card, link->name);
		if (!rtd)
			continue;

		for (i = 0; i < rtd->num_codecs; i++) {
			aif_dai = rtd->cpu_dai;
			dapm = snd_soc_component_get_dapm(aif_dai->component);
			if (aif_dai->playback_widget) {
				name = aif_dai->playback_widget->name;
				dev_info(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
			if (aif_dai->capture_widget) {
				name = aif_dai->capture_widget->name;
				dev_info(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}

			aif_dai = rtd->codec_dais[i];
			dapm = snd_soc_component_get_dapm(aif_dai->component);
			if (aif_dai->playback_widget) {
				name = aif_dai->playback_widget->name;
				dev_info(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
			if (aif_dai->capture_widget) {
				name = aif_dai->capture_widget->name;
				dev_info(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
		}
	}

	aif_dai = get_rtd(card, CS35L41_DAI_ID)->codec_dai;
	register_cirrus_bigdata_cb(aif_dai->component);

	return 0;
}

static const struct snd_soc_dapm_widget picasso_supply_widgets[] = {
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS1", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS2", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS3", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS4", 0, 0),
};

static int picasso_probe(struct snd_soc_card *card)
{
	int i;
	struct picasso_drvdata *drvdata = card->drvdata;

	for (i = 0; i < ARRAY_SIZE(picasso_supply_widgets); i++) {
		const struct snd_soc_dapm_widget *w;
		struct regulator *r;

		w = &picasso_supply_widgets[i];
		switch (w->id) {
		case snd_soc_dapm_regulator_supply:
			r = regulator_get(card->dev, w->name);
			if (!IS_ERR_OR_NULL(r)) {
				regulator_put(r);
				snd_soc_dapm_new_control(&card->dapm, w);
			}
			break;
		default:
			dev_warn(card->dev, "ignored: %s\n", w->name);
			break;
		}
	}

	wake_lock_init(&drvdata->wake_lock, WAKE_LOCK_SUSPEND,
				"picasso-sound");
	drvdata->wake_lock_switch = 0;

	drvdata->cpd_disable_state = 0;

	return 0;
}

static int picasso_remove(struct snd_soc_card *card)
{
	struct picasso_drvdata *drvdata = card->drvdata;

	wake_lock_destroy(&drvdata->wake_lock);

	return 0;
}


static struct snd_soc_pcm_stream prince_amp_params[] = {
	{
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = PRINCE_AMP_RATE,
		.rate_max = PRINCE_AMP_RATE,
		.channels_min = 1,
		.channels_max = 1,
	},
};

static struct snd_soc_dai_link picasso_dai[100] = {
	{
		.name = "RDMA0",
		.stream_name = "RDMA0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA1",
		.stream_name = "RDMA1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA2",
		.stream_name = "RDMA2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA3",
		.stream_name = "RDMA3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA4",
		.stream_name = "RDMA4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA5",
		.stream_name = "RDMA5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA6",
		.stream_name = "RDMA6",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA7",
		.stream_name = "RDMA7",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA8",
		.stream_name = "RDMA8",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA9",
		.stream_name = "RDMA9",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA10",
		.stream_name = "RDMA10",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA11",
		.stream_name = "RDMA11",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "WDMA0",
		.stream_name = "WDMA0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA1",
		.stream_name = "WDMA1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA2",
		.stream_name = "WDMA2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA3",
		.stream_name = "WDMA3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA4",
		.stream_name = "WDMA4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE,
			SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
	{
		.name = "VTS-Trigger",
		.stream_name = "VTS-Trigger",
		.cpu_dai_name = "vts-tri",
		.platform_name = "15510000.vts:vts_dma@0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
	},
	{
		.name = "VTS-Record",
		.stream_name = "VTS-Record",
		.cpu_dai_name = "vts-rec",
		.platform_name = "15510000.vts:vts_dma@1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
	{
		.name = "DP0 Audio",
		.stream_name = "DP0 Audio",
		.platform_name = "dp_dma:dp_dma@0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
	},
	{
		.name = "DP1 Audio",
		.stream_name = "DP1 Audio",
		.platform_name = "dp_dma:dp_dma@1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
	},
#endif
	{
		.name = "WDMA0 DUAL",
		.stream_name = "WDMA0 DUAL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA1 DUAL",
		.stream_name = "WDMA1 DUAL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA2 DUAL",
		.stream_name = "WDMA2 DUAL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA3 DUAL",
		.stream_name = "WDMA3 DUAL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA4 DUAL",
		.stream_name = "WDMA4 DUAL",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},	{
		.name = "DEBUG0",
		.stream_name = "DEBUG0",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG1",
		.stream_name = "DEBUG1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG2",
		.stream_name = "DEBUG2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG3",
		.stream_name = "DEBUG3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG4",
		.stream_name = "DEBUG4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG5",
		.stream_name = "DEBUG5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "UAIF0",
		.stream_name = "UAIF0",
		.platform_name = "snd-soc-dummy",
		.id = MADERA_DAI_ID,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif0_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF1",
		.stream_name = "UAIF1",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF2",
		.stream_name = "UAIF2",
		.platform_name = "snd-soc-dummy",
		.id = CS35L41_DAI_ID,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF3",
		.stream_name = "UAIF3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF4",
		.stream_name = "UAIF4",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF5",
		.stream_name = "UAIF5",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF6",
		.stream_name = "UAIF6",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "DSIF",
		.stream_name = "DSIF",
		.cpu_dai_name = "DSIF",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &dsif_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "SIFS0",
		.stream_name = "SIFS0",
		.cpu_dai_name = "SIFS0",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS1",
		.stream_name = "SIFS1",
		.cpu_dai_name = "SIFS1",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS2",
		.stream_name = "SIFS2",
		.cpu_dai_name = "SIFS2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS3",
		.stream_name = "SIFS3",
		.cpu_dai_name = "SIFS3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS4",
		.stream_name = "SIFS4",
		.cpu_dai_name = "SIFS4",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS5",
		.stream_name = "SIFS5",
		.cpu_dai_name = "SIFS5",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC0",
		.stream_name = "NSRC0",
		.cpu_dai_name = "NSRC0",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC1",
		.stream_name = "NSRC1",
		.cpu_dai_name = "NSRC1",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC2",
		.stream_name = "NSRC2",
		.cpu_dai_name = "NSRC2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC3",
		.stream_name = "NSRC3",
		.cpu_dai_name = "NSRC3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC4",
		.stream_name = "NSRC4",
		.cpu_dai_name = "NSRC4",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA0 BE",
		.stream_name = "RDMA0 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 0),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA1 BE",
		.stream_name = "RDMA1 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 1),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA2 BE",
		.stream_name = "RDMA2 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 2),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA3 BE",
		.stream_name = "RDMA3 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 3),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA4 BE",
		.stream_name = "RDMA4 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 4),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA5 BE",
		.stream_name = "RDMA5 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 5),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA6 BE",
		.stream_name = "RDMA6 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 6),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA7 BE",
		.stream_name = "RDMA7 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 7),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA8 BE",
		.stream_name = "RDMA8 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 8),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA9 BE",
		.stream_name = "RDMA9 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 9),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA10 BE",
		.stream_name = "RDMA10 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 10),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA11 BE",
		.stream_name = "RDMA11 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(0, 11),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA0 BE",
		.stream_name = "WDMA0 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(1, 0),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA1 BE",
		.stream_name = "WDMA1 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(1, 1),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA2 BE",
		.stream_name = "WDMA2 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(1, 2),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA3 BE",
		.stream_name = "WDMA3 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(1, 3),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA4 BE",
		.stream_name = "WDMA4 BE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.id = ABOX_BE_DAI_ID(1, 4),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "USB",
		.stream_name = "USB",
		.cpu_dai_name = "USB",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "FWD",
		.stream_name = "FWD",
		.cpu_dai_name = "FWD",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
	},
};

static int picasso_dmic1(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int picasso_dmic2(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int picasso_dmic3(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int picasso_l_speaker(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		cirrus_bd_store_values("_0");
		break;
	}

	return 0;
}

static int picasso_r_speaker(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		cirrus_bd_store_values("_1");
		break;
	}

	return 0;
}

static int picasso_btsco_mic(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int picasso_btsco_out(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int picasso_usb_mic(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int picasso_usb_out(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int get_sound_wakelock(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct picasso_drvdata *drvdata = &picasso_prince_drvdata;

	ucontrol->value.integer.value[0] = drvdata->wake_lock_switch;

	return 0;
}

static int set_sound_wakelock(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct picasso_drvdata *drvdata = &picasso_prince_drvdata;

	drvdata->wake_lock_switch = ucontrol->value.integer.value[0];

	dev_info(drvdata->dev, "%s: %d\n", __func__, drvdata->wake_lock_switch);

	if (drvdata->wake_lock_switch) {
		wake_lock(&drvdata->wake_lock);
	} else {
		wake_unlock(&drvdata->wake_lock);
	}

	return 0;
}

static int cpd_disable_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct picasso_drvdata *drvdata = &picasso_prince_drvdata;

	ucontrol->value.integer.value[0] = drvdata->cpd_disable_state;

	return 0;
}

static int cpd_disable_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct picasso_drvdata *drvdata = &picasso_prince_drvdata;

	drvdata->cpd_disable_state = ucontrol->value.integer.value[0];

	dev_info(drvdata->dev, "%s: %d\n", __func__, drvdata->cpd_disable_state);

	if (drvdata->cpd_disable_state) {
		disable_power_mode(6, POWERMODE_TYPE_CLUSTER);
	} else {
		enable_power_mode(6, POWERMODE_TYPE_CLUSTER);
	}

	return 0;
}

static const char * const vts_output_texts[] = {
	"None",
	"DMIC1",
};

static const struct soc_enum vts_output_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(vts_output_texts),
			vts_output_texts);


static const struct snd_kcontrol_new vts_output_mux[] = {
	SOC_DAPM_ENUM("VTS Virtual Output Mux", vts_output_enum),
};

static const struct snd_kcontrol_new picasso_controls[] = {
	SOC_DAPM_PIN_SWITCH("DMIC1"),
	SOC_DAPM_PIN_SWITCH("DMIC2"),
	SOC_DAPM_PIN_SWITCH("DMIC3"),
	SOC_DAPM_PIN_SWITCH("DMIC4"),
	SOC_SINGLE_BOOL_EXT("Sound Wakelock",
			0, get_sound_wakelock, set_sound_wakelock),
	/* R8s WA: HW(cheongno.yun) requested
	 * disable CPD during hadnset call */
	SOC_SINGLE_EXT("CPD Disable", SND_SOC_NOPM, 0, 1, 0,
			cpd_disable_get, cpd_disable_put),
};

static const struct snd_soc_dapm_widget picasso_widgets[] = {
	SND_SOC_DAPM_OUTPUT("VOUTPUT"),
	SND_SOC_DAPM_INPUT("VINPUT1"),
	SND_SOC_DAPM_INPUT("VINPUT2"),
	SND_SOC_DAPM_OUTPUT("VOUTPUTCALL"),
	SND_SOC_DAPM_INPUT("VINPUTCALL"),
	SND_SOC_DAPM_MIC("DMIC1", picasso_dmic1),
	SND_SOC_DAPM_MIC("DMIC2", picasso_dmic2),
	SND_SOC_DAPM_MIC("DMIC3", picasso_dmic3),
	SND_SOC_DAPM_MIC("DMIC4", NULL),
	SND_SOC_DAPM_MIC("HEADSETMIC", NULL),
	SND_SOC_DAPM_HP("HEADPHONE", NULL),
	SND_SOC_DAPM_SPK("RECEIVER", picasso_l_speaker),
	SND_SOC_DAPM_SPK("SPEAKER", picasso_r_speaker),
	SND_SOC_DAPM_MIC("SPEAKER FB", NULL),
	SND_SOC_DAPM_MIC("BLUETOOTH MIC", picasso_btsco_mic),
	SND_SOC_DAPM_SPK("BLUETOOTH SPK", picasso_btsco_out),
	SND_SOC_DAPM_MIC("USB MIC", picasso_usb_mic),
	SND_SOC_DAPM_SPK("USB SPK", picasso_usb_out),
	SND_SOC_DAPM_MIC("FWD MIC", NULL),
	SND_SOC_DAPM_SPK("FWD SPK", NULL),
	SND_SOC_DAPM_OUTPUT("VTS Virtual Output"),
	SND_SOC_DAPM_MUX("VTS Virtual Output Mux", SND_SOC_NOPM, 0, 0,
			&vts_output_mux[0]),
};

static const struct snd_soc_dapm_route picasso_routes[] = {
	{"VTS Virtual Output Mux", "DMIC1", "DMIC1"},
};

static struct snd_soc_codec_conf codec_conf[ABOX_CODEC_MAX];

static struct snd_soc_aux_dev aux_dev[ABOX_AUX_MAX];

static struct snd_soc_card picasso_prince = {
	.name = "Picasso-Prince",
	.owner = THIS_MODULE,
	.dai_link = picasso_dai,
	.num_links = ARRAY_SIZE(picasso_dai),

	.probe = picasso_probe,
	.late_probe = picasso_late_probe,
	.remove = picasso_remove,

	.controls = picasso_controls,
	.num_controls = ARRAY_SIZE(picasso_controls),
	.dapm_widgets = picasso_widgets,
	.num_dapm_widgets = ARRAY_SIZE(picasso_widgets),
	.dapm_routes = picasso_routes,
	.num_dapm_routes = ARRAY_SIZE(picasso_routes),

	.set_bias_level = picasso_set_bias_level,
	.set_bias_level_post = picasso_set_bias_level_post,

	.drvdata = (void *)&picasso_prince_drvdata,

	.codec_conf = codec_conf,
	.num_configs = ARRAY_SIZE(codec_conf),

	.aux_dev = aux_dev,
	.num_aux_devs = ARRAY_SIZE(aux_dev),
};

static int read_clk_conf(struct device_node *np,
				   const char * const prop,
				   struct clk_conf *conf,
				   bool is_fll)
{
	u32 tmp;
	int ret;

	/*Truncate "cirrus," from prop_name to fetch clk_name*/
	conf->name = &prop[7];

	ret = of_property_read_u32_index(np, prop, 0, &tmp);
	if (ret)
		return ret;

	conf->id = tmp;

	ret = of_property_read_u32_index(np, prop, 1, &tmp);
	if (ret)
		return ret;

	if (tmp < 0xffff)
		conf->source = tmp;
	else
		conf->source = -1;

	ret = of_property_read_u32_index(np, prop, 2, &tmp);
	if (ret)
		return ret;

	conf->rate = tmp;

	if (is_fll) {
		ret = of_property_read_u32_index(np, prop, 3, &tmp);
		if (ret)
			return ret;
		conf->fout = tmp;
	}

	conf->valid = true;

	return 0;
}

static int read_platform(struct device_node *np, const char * const prop,
			      struct device_node **dai)
{
	int ret = 0;

	np = of_get_child_by_name(np, prop);
	if (!np)
		return -ENOENT;

	*dai = of_parse_phandle(np, "sound-dai", 0);
	if (!*dai) {
		ret = -ENODEV;
		goto out;
	}
out:
	of_node_put(np);

	return ret;
}

static int read_cpu(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret = 0;

	np = of_get_child_by_name(np, "cpu");
	if (!np)
		return -ENOENT;

	dai_link->cpu_of_node = of_parse_phandle(np, "sound-dai", 0);
	if (!dai_link->cpu_of_node) {
		ret = -ENODEV;
		goto out;
	}

	if (dai_link->cpu_dai_name == NULL) {
		/* Ignoring the return
		 * as we don't register DAIs to the platform
		 */
		ret = snd_soc_of_get_dai_name(np, &dai_link->cpu_dai_name);
		if (ret)
			goto out;
	}
out:
	of_node_put(np);

	return ret;
}

static int read_codec(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	np = of_get_child_by_name(np, "codec");
	if (!np)
		return -ENOENT;

	return snd_soc_of_get_dai_link_codecs(dev, np, dai_link);
}

static void picasso_register_card_work_func(struct work_struct *work)
{
	struct snd_soc_card *card = &picasso_prince;
	int ret;

	ret = devm_snd_soc_register_card(card->dev, card);
	if (ret)
		dev_err(card->dev, "sound card register failed: %d\n", ret);
}
DECLARE_WORK(picasso_register_card_work, picasso_register_card_work_func);

static int picasso_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &picasso_prince;
	struct picasso_drvdata *drvdata = card->drvdata;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dai;
	struct snd_soc_dai_link *link;
	int nlink = 0;
	int i, ret;
	const char *cur = NULL;
	struct property *p;

	card->dev = &pdev->dev;
	drvdata->dev = card->dev;

	snd_soc_card_set_drvdata(card, drvdata);

	i = 0;
	p = of_find_property(np, "clock-names", NULL);
	if (p) {
		while ((cur = of_prop_next_string(p, cur)) != NULL) {
			drvdata->clk[i] = devm_clk_get(drvdata->dev, cur);
			if (IS_ERR(drvdata->clk[i])) {
				dev_info(drvdata->dev, "Failed to get %s: %ld\n",
					 cur, PTR_ERR(drvdata->clk[i]));
				drvdata->clk[i] = NULL;
				break;
			}

			clk_prepare_enable(drvdata->clk[i]);

			if (++i == MADERA_MAX_CLOCKS)
				break;
		}
	}

	ret = read_clk_conf(np, "cirrus,sysclk",
				      &drvdata->sysclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse sysclk: %d\n", ret);
	ret = read_clk_conf(np, "cirrus,asyncclk",
				      &drvdata->asyncclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse asyncclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,dspclk",
				      &drvdata->dspclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse dspclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,opclk",
				      &drvdata->opclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse opclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,fll1-refclk",
				      &drvdata->fll1_refclk, true);
	if (ret)
		dev_dbg(card->dev, "Failed to parse fll1-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,fll2-refclk",
				      &drvdata->fll2_refclk, true);
	if (ret)
		dev_dbg(card->dev, "Failed to parse fll2-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,fllao-refclk",
				      &drvdata->fllao_refclk, true);
	if (ret)
		dev_dbg(card->dev, "Failed to parse fllao-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,outclk",
				      &drvdata->outclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse outclk: %d\n", ret);

	for_each_child_of_node(np, dai) {
		link = &picasso_dai[nlink];

		if (!link->name)
			link->name = dai->name;
		if (!link->stream_name)
			link->stream_name = dai->name;

		if (!link->cpu_name) {
			ret = read_cpu(dai, card->dev, link);
			if (ret) {
				dev_err(card->dev, "Failed to parse cpu DAI for %s: %d\n",
						dai->name, ret);
				return ret;
			}
		}

		if (!link->platform_name) {
			ret = read_platform(dai, "platform",
				&link->platform_of_node);
			if (ret) {
				link->platform_of_node = link->cpu_of_node;
				dev_info(card->dev, "Cpu node is used as platform for %s: %d\n",
						dai->name, ret);
			}
		}

		if (!link->codec_name) {
			ret = read_codec(dai, card->dev, link);
			if (ret) {
				dev_warn(card->dev, "Failed to parse codec DAI for %s: %d\n",
						dai->name, ret);

				link->codec_name = "snd-soc-dummy";
				link->codec_dai_name = "snd-soc-dummy-dai";
				ret = 0;
			}

			if (link->codecs && strstr(link->codecs[0].dai_name,
					"cs35l41"))
				link->ops = &cs35l41_ops;
		}

		if (strstr(dai->name, "left-amp")) {
			link->params = prince_amp_params;
			drvdata->left_amp_dai = nlink;
		} else if (strstr(dai->name, "right-amp")) {
			link->params = prince_amp_params;
			drvdata->right_amp_dai = nlink;
		}

		link->dai_fmt = snd_soc_of_parse_daifmt(dai, NULL, NULL, NULL);

		if (++nlink == card->num_links)
			break;
	}

	if (!nlink) {
		dev_err(card->dev, "No DAIs specified\n");
		return -EINVAL;
	}

	/* Dummy pcm to adjust ID of PCM added by topology */
	for (; nlink < card->num_links; nlink++) {
		link = &picasso_dai[nlink];

		if (!link->name)
			link->name = devm_kasprintf(card->dev, GFP_KERNEL,
					"dummy%d", nlink);
		if (!link->stream_name)
			link->stream_name = link->name;
		if (!link->cpu_name) {
			link->cpu_name = "snd-soc-dummy";
			link->cpu_dai_name = "snd-soc-dummy-dai";
		}
		if (!link->codec_name) {
			link->codec_name = "snd-soc-dummy";
			link->codec_dai_name = "snd-soc-dummy-dai";
		}
		link->no_pcm = 1;
		link->ignore_suspend = 1;
	}

	if (of_property_read_bool(np, "samsung,routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "samsung,routing");
		if (ret)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(codec_conf); i++) {
		codec_conf[i].of_node = of_parse_phandle(np,
				"samsung,codec", i);
		if (!codec_conf[i].of_node)
			break;
		ret = of_property_read_string_index(np, "samsung,prefix", i,
				&codec_conf[i].name_prefix);
		if (ret < 0)
			codec_conf[i].name_prefix = "";
	}
	card->num_configs = i;

	for (i = 0; i < ARRAY_SIZE(aux_dev); i++) {
		aux_dev[i].codec_of_node = of_parse_phandle(np,
				"samsung,aux", i);
		if (!aux_dev[i].codec_of_node)
			break;
	}
	card->num_aux_devs = i;

	schedule_work(&picasso_register_card_work);

	return ret;
}

static int picasso_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct picasso_drvdata *drvdata;
	int i;

	card = platform_get_drvdata(pdev);
	if (!card)
		return 0;

	drvdata = snd_soc_card_get_drvdata(card);

	for (i = 0; i < MADERA_MAX_CLOCKS; ++i)
		clk_disable_unprepare(drvdata->clk[i]);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id picasso_audio_of_match[] = {
	{ .compatible = "samsung,picasso-prince", },
	{},
};
MODULE_DEVICE_TABLE(of, picasso_audio_of_match);
#endif /* CONFIG_OF */

static struct platform_driver picasso_audio_driver = {
	.driver		= {
		.name	= "picasso-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(picasso_audio_of_match),
	},

	.probe		= picasso_audio_probe,
	.remove		= picasso_audio_remove,
};

module_platform_driver(picasso_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Picasso Audio Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:picasso-prince");
