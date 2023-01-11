/*
 * linux/sound/soc/pxa/mmp-map-card.c
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mfd/88pm8xxx-headset.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/mfd/mmp-map.h>
#include "mmp-tdm.h"

#ifdef CONFIG_SND_MMP_MAP_DUMP
int hifi1_playback_enable;
int hifi1_capture_enable;
int hifi2_playback_enable;
int hifi2_capture_enable;
#endif

/* I2S1/I2S4/I2S3 use 44.1k sample rate by default */
#define MAP_SR_HIFI SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000
#define MAP_SR_FM SNDRV_PCM_RATE_48000
/* I2S2/I2S3 use 8K or 16K sample rate */
#define MAP_SR_LOFI (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)

/* SSPA and sysclk pll sources */
#define SSPA_AUDIO_PLL                          0
#define SSPA_I2S_PLL                            1
#define SSPA_VCXO_PLL                           2
#define AUDIO_PLL                               3

#define MAX_DAILINK_NUM				0xF

static int map_startup_FM(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	cpu_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->playback.rates = MAP_SR_FM;
	cpu_dai->driver->capture.rates = MAP_SR_FM;

	return 0;
}

#ifdef CONFIG_SND_MMP_MAP_DUMP
static DEVICE_INT_ATTR(hifi1_playback_dump, 0644, hifi1_playback_enable);
static DEVICE_INT_ATTR(hifi1_capture_dump, 0644, hifi1_capture_enable);
static DEVICE_INT_ATTR(hifi2_playback_dump, 0644, hifi2_playback_enable);
static DEVICE_INT_ATTR(hifi2_capture_dump, 0644, hifi2_capture_enable);
#endif

static int map_startup_hifi(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	cpu_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->playback.rates = MAP_SR_HIFI;
	cpu_dai->driver->capture.rates = MAP_SR_HIFI;

	return 0;
}

static int map_startup_lofi(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	cpu_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->playback.rates = MAP_SR_LOFI;
	cpu_dai->driver->capture.rates = MAP_SR_LOFI;

	return 0;
}

static int map_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;

	codec_dai->stream = substream->stream;
	cpu_dai->stream = substream->stream;

	freq_in = 32768;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 128;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 64;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
		    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);

	/* SSPA clock ctrl register changes, and can't use previous API */
	snd_soc_dai_set_sysclk(cpu_dai, AUDIO_PLL, freq_out, 0);
	/* sysclk */
	snd_soc_dai_set_pll(cpu_dai, SSPA_AUDIO_PLL, 0, freq_out, sysclk);
	/* ssp clk */
	snd_soc_dai_set_pll(cpu_dai, SSPA_I2S_PLL, 0, freq_out, sspa_mclk);

	/* set 88pm860 sysclk */
	snd_soc_dai_set_sysclk(codec_dai, 0, 0, 1);

	return 0;
}

static int map_fe_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	u32 freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
	u32 srate = params_rate(params);

	codec_dai->stream = substream->stream;
	cpu_dai->stream = substream->stream;

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

#if 1
	/* For i2s2(voice call) and i2s3(bt-audio), the dai format is pcm */
	if (codec_dai->id == 2) {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	} else if (codec_dai->id == 5) {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_IF | SND_SOC_DAIFMT_CBM_CFM);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_IF | SND_SOC_DAIFMT_CBM_CFM);
	} else {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_IF | SND_SOC_DAIFMT_CBM_CFM);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_IF | SND_SOC_DAIFMT_CBM_CFM);
	}
#else
	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
#endif

	/* SSPA clock ctrl register changes, and can't use previous API */
	snd_soc_dai_set_sysclk(cpu_dai, AUDIO_PLL, freq_out, 0);
	snd_soc_dai_set_clkdiv(cpu_dai, 0, 0);

	/* set i2s1/2/3/4 sysclk */
	snd_soc_dai_set_sysclk(codec_dai, APLL_32K, srate, 0);

	return 0;
}

static int map_be_dei2s_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
	u32 srate = params_rate(params);
	/*
	 * Now, hard code here, suppose change it according to channel num
	 * the channel allocation..
	 */
	unsigned int if1_tx_channel[2] = {3, 1};
	unsigned int if1_rx_channel[2] = {7, 5};
	unsigned int if2_tx_channel[2] = {2, 0};
	unsigned int if2_rx_channel[2] = {6, 4};

	codec_dai->stream = substream->stream;
	cpu_dai->stream = substream->stream;

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	if (cpu_dai->id == 1) {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	} else {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	}

	snd_soc_dai_set_sysclk(cpu_dai, APLL_32K, srate, 0);

	/*
	 * set slot for both cpu_dai and codec dai,
	 * enable L1/R1 channel
	 */
	if (cpu_dai->id == 2) {
		snd_soc_dai_set_channel_map(cpu_dai,
					2, if1_tx_channel, 2, if1_rx_channel);
		snd_soc_dai_set_channel_map(codec_dai,
					2, if1_rx_channel, 2, if1_tx_channel);
	} else if (cpu_dai->id == 3) {
		snd_soc_dai_set_channel_map(cpu_dai,
					2, if2_tx_channel, 2, if2_rx_channel);
		snd_soc_dai_set_channel_map(codec_dai,
					2, if2_rx_channel, 2, if2_tx_channel);
	}

	/* set codec sysclk */
	snd_soc_dai_set_sysclk(codec_dai, 0, 0, 1);

	return 0;
}

static int map_aux_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct map_private *map_priv;
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	map_priv = dev_get_drvdata(cpu_dai->dev->parent);

	if (map_priv->bt_wb_sel)
		rate->min = rate->max = 16000;
	else
		rate->min = rate->max = 8000;
	return 0;
}

static int map_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 48000;
	return 0;
}

static int map_tdm_spkr_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	int tx[2] = {1, 2};
	int tx_num = 2;
#else
	int channel;
#endif
	codec_dai->stream = substream->stream;
	cpu_dai->stream = substream->stream;

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);

	snd_soc_dai_set_sysclk(cpu_dai, APLL_32K, freq_out, 0);

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	mmp_tdm_static_slot_alloc(substream, tx, tx_num, NULL, 0);
	snd_soc_dai_set_channel_map(cpu_dai, tx_num, tx, 0, NULL);
	snd_soc_dai_set_channel_map(codec_dai, 0, NULL, tx_num, tx);
#else
	/*allocate slot*/
	channel = params_channels(params);
	mmp_tdm_request_slot(substream, channel);
#endif
	return 0;
}

void map_tdm_spkr_shutdown(struct snd_pcm_substream *substream)
{
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx[2] = {0, 0};
	int tx_num = 2;
#endif

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	snd_soc_dai_set_channel_map(codec_dai, 0, NULL, tx_num, tx);
	snd_soc_dai_set_channel_map(cpu_dai, tx_num, tx, 0, NULL);
	mmp_tdm_static_slot_free(substream);
#else
	mmp_tdm_free_slot(substream);
#endif
}

static int map_tdm_hs_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	int tx[2] = {3, 4};
	int tx_num = 2;
#else
	int channel;
#endif
	codec_dai->stream = substream->stream;
	cpu_dai->stream = substream->stream;

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);

	snd_soc_dai_set_sysclk(cpu_dai, APLL_32K, freq_out, 0);

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	mmp_tdm_static_slot_alloc(substream, tx, tx_num, NULL, 0);
	snd_soc_dai_set_channel_map(cpu_dai, tx_num, tx, 0, NULL);
	snd_soc_dai_set_channel_map(codec_dai, 0, NULL, tx_num, tx);
#else
	/* allocate slot */
	channel = params_channels(params);
	mmp_tdm_request_slot(substream, channel);
#endif
	return 0;
}

void map_tdm_hs_shutdown(struct snd_pcm_substream *substream)
{
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx[2] = {0, 0};
	int tx_num = 2;
#endif

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	snd_soc_dai_set_channel_map(codec_dai, 0, NULL, tx_num, tx);
	snd_soc_dai_set_channel_map(cpu_dai, tx_num, tx, 0, NULL);
	mmp_tdm_static_slot_free(substream);
#else
	mmp_tdm_free_slot(substream);
#endif
}

static int map_tdm_mic_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	int tx[2] = {1, 2};
	int tx_num = 2;
#else
	int channel;
#endif
	codec_dai->stream = substream->stream;
	cpu_dai->stream = substream->stream;

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = 11289600;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);

	snd_soc_dai_set_sysclk(cpu_dai, APLL_32K, freq_out, 0);

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	mmp_tdm_static_slot_alloc(substream, NULL, 0, tx, tx_num);
	snd_soc_dai_set_channel_map(cpu_dai, 0, NULL, tx_num, tx);
	snd_soc_dai_set_channel_map(codec_dai, tx_num, tx, 0, NULL);
#else
	/* allocate slot */
	channel = params_channels(params);
	mmp_tdm_request_slot(substream, channel);
#endif
	return 0;
}

void map_tdm_mic_shutdown(struct snd_pcm_substream *substream)
{
#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx[1] = {0};
	int tx_num = 1;
#endif

#ifdef CONFIG_SND_TDM_STATIC_ALLOC
	snd_soc_dai_set_channel_map(codec_dai, tx_num, tx, 0, NULL);
	snd_soc_dai_set_channel_map(cpu_dai, 0, NULL, tx_num, tx);
	mmp_tdm_static_slot_free(substream);
#else
	mmp_tdm_free_slot(substream);
#endif
}

/* machine stream operations */
static struct snd_soc_ops map_ops[] = {
	{
	 .startup = map_startup_hifi,
	 .hw_params = map_hw_params,
	},
	{
	 .startup = map_startup_hifi,
	 .hw_params = map_fe_hw_params,
	},
	{
	 .startup = map_startup_lofi,
	 .hw_params = map_fe_hw_params,
	},
	{
	 .startup = map_startup_lofi,
	 .hw_params = map_be_dei2s_hw_params,
	},
	{
	 .startup = map_startup_hifi,
	 .hw_params = map_tdm_spkr_hw_params,
	 .shutdown = map_tdm_spkr_shutdown,
	},
	{
	 .startup = map_startup_hifi,
	 .hw_params = map_tdm_mic_hw_params,
	 .shutdown = map_tdm_mic_shutdown,
	},
	{
	 .startup = map_startup_hifi,
	 .hw_params = map_tdm_hs_hw_params,
	 .shutdown = map_tdm_hs_shutdown,
	},
	{
	 .startup = map_startup_FM,
	 .hw_params = map_fe_hw_params,
	},
};

static struct of_device_id dailink_matches[] = {
	{
	.compatible = "marvell,map-dailink-0",
	.data = (void *)&map_ops[0],
	},
	{
	.compatible = "marvell,map-dailink-1",
	.data = (void *)&map_ops[1],
	},
	{
	.compatible = "marvell,map-dailink-2",
	.data = (void *)&map_ops[2],
	},
	{
	.compatible = "marvell,map-dailink-3",
	.data = (void *)&map_ops[3],
	},
	{
	.compatible = "marvell,map-dailink-4",
	.data = (void *)&map_ops[4],
	},
	{
	.compatible = "marvell,map-dailink-5",
	.data = (void *)&map_ops[5],
	},
	{
	.compatible = "marvell,map-dailink-6",
	.data = (void *)&map_ops[6],
	},
	{
	.compatible = "marvell,map-dailink-7",
	.data = (void *)&map_ops[7],
	},
};

/* digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link map_dai[MAX_DAILINK_NUM];

static struct snd_soc_jack hs_jack, hk_jack;
int (*headset_detect)(struct snd_soc_jack *);
int (*hook_detect)(struct snd_soc_jack *);

static int map_late_probe(struct snd_soc_card *card)
{
	int ret;
	struct snd_soc_codec *codec = card->rtd[0].codec;

	ret = snd_soc_jack_new(codec, "Headset", SND_JACK_HEADSET, &hs_jack);
	if (ret)
		return ret;

	if (headset_detect) {
		ret = headset_detect(&hs_jack);
		if (ret)
			return ret;
	}

	ret = snd_soc_jack_new(codec, "Hook", SND_JACK_HEADSET, &hk_jack);
	if (ret)
		return ret;

	snd_jack_set_key(hk_jack.jack, SND_JACK_BTN_0, KEY_MEDIA);
	snd_jack_set_key(hk_jack.jack, SND_JACK_BTN_1, KEY_VOLUMEUP);
	snd_jack_set_key(hk_jack.jack, SND_JACK_BTN_2, KEY_VOLUMEDOWN);
	snd_jack_set_key(hk_jack.jack, SND_JACK_BTN_3, KEY_VOICECOMMAND);

	if (hook_detect) {
		ret = hook_detect(&hk_jack);
		if (ret)
			return ret;
	}

	return 0;
}

/* audio machine driver */
static struct snd_soc_card snd_soc_map = {
	.name = "map asoc",
	.dai_link = &map_dai[0],
	.late_probe = map_late_probe,
};

static int map_probe_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dailink_np;
	const struct of_device_id *match;
	struct of_phandle_args out_args;
	u32 fixup_index;
	int i, ret = 0;

	if (!np)
		return 1;	/* no device tree */

	ret = snd_soc_of_parse_audio_routing(&snd_soc_map, "map,dapm-route");
	if (ret) {
		dev_err(&pdev->dev, "invalid DAPM routing\n");
		return ret;
	}

	i = 0;
	for_each_child_of_node(np, dailink_np) {
		match = of_match_node(dailink_matches, dailink_np);
		if (!match)
			continue;

		if (i >= ARRAY_SIZE(map_dai)) {
			dev_err(&pdev->dev, "index 0x%x overflow map_dai\n", i);
			return -EINVAL;
		}

		map_dai[i].ops = match->data;

		ret =
		    of_parse_phandle_with_args(dailink_np, "marvell,cpu-dai",
					       "#dailink-cells", 0, &out_args);
		if (ret) {
			dev_err(&pdev->dev, "can't parse cpu-dai for ops[%d]\n",
				i);
			return -EINVAL;
		}
		map_dai[i].cpu_of_node = out_args.np;
		map_dai[i].cpu_dai_id = out_args.args[0];
		map_dai[i].platform_of_node = out_args.np;

		ret =
		    of_parse_phandle_with_args(dailink_np, "marvell,codec-dai",
					       "#dailink-cells", 0, &out_args);
		if (ret) {
			dev_err(&pdev->dev,
				"can't parse codec-dai for ops[%d]\n", i);
			return -EINVAL;
		}
		map_dai[i].codec_of_node = out_args.np;
		map_dai[i].codec_dai_id = out_args.args[0];

		ret =
		    of_property_read_string(dailink_np, "dai-name",
					    &map_dai[i].name);
		if (ret < 0) {
			dev_err(&pdev->dev, "invalid dailink name in dt!\n");
			return -EINVAL;
		}

		ret =
		    of_property_read_string(dailink_np, "stream-name",
					    &map_dai[i].stream_name);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"invalid dailink stream_name in dt!\n");
			return -EINVAL;
		}

		/*
		 * snd-soc-dummy has no dt, and we need to parse codec_name.
		 * clear codec_of_node if codec doesn't have dt.
		 */
		ret =
		    of_property_read_string(dailink_np, "marvell,codec-name",
					    &map_dai[i].codec_name);
		if (ret == 0) {
			map_dai[i].codec_of_node = NULL;
			ret =
			    of_property_read_string(dailink_np,
						    "marvell,codec-dai-name",
						    &map_dai[i].codec_dai_name);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"codec-name & codec-dai-name mismatch in dt!\n");
				return -EINVAL;
			}
		}

		map_dai[i].dynamic = 1;
		map_dai[i].dpcm_playback = 1;
		map_dai[i].dpcm_capture = 1;

		if (of_property_read_bool(dailink_np, "marvell,playback-only")) {
			map_dai[i].dpcm_capture = 0;
		}

		if (of_property_read_bool(dailink_np, "marvell,capture-only")) {
			map_dai[i].dpcm_playback = 0;
		}

		if (of_property_read_bool(dailink_np, "marvell,dai-no-pcm")) {
			map_dai[i].no_pcm = 1;
			map_dai[i].platform_of_node = NULL;
		}

		ret =
		    of_property_read_u32(dailink_np, "marvell,dai-fixup",
					 &fixup_index);
		if (ret == 0) {
			/* currently there are only two fixup functions */
			if (fixup_index == 0)
				map_dai[i].be_hw_params_fixup =
				    &map_aux_be_hw_params_fixup;
			else if (fixup_index == 1)
				map_dai[i].be_hw_params_fixup =
				    &map_tdm_be_hw_params_fixup;
		}

		i++;
	}

	snd_soc_map.num_links = i;

	return 0;
}

static struct of_device_id mmp_map_dt_ids[] = {
	{.compatible = "marvell,map-card",},
	{}
};
MODULE_DEVICE_TABLE(of, mmp_map_dt_ids);

static int map_audio_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card;
	card = &snd_soc_map;

	card->dev = &pdev->dev;

	ret = map_probe_dt(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "map_probe_dt failed: %d\n", ret);
		return ret;
	}

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n",
			ret);

#ifdef CONFIG_SND_MMP_MAP_DUMP
	/* add hifi1_playback_dump sysfs entries */
	ret = device_create_file(&pdev->dev, &dev_attr_hifi1_playback_dump.attr);
	if (ret < 0)
		dev_err(&pdev->dev,
			"%s: failed to add hifi1_playback_dump sysfs files: %d\n",
			__func__, ret);
	/* add hifi1_capture_dump sysfs entries */
	ret = device_create_file(&pdev->dev, &dev_attr_hifi1_capture_dump.attr);
	if (ret < 0)
		dev_err(&pdev->dev,
			"%s: failed to add hifi1_capture_dump sysfs files: %d\n",
			__func__, ret);

	if (!map_get_lite_attr()) {
		/* add hifi2_playback_dump sysfs entries */
		ret = device_create_file(&pdev->dev, &dev_attr_hifi2_playback_dump.attr);
		if (ret < 0)
			dev_err(&pdev->dev,
				"%s: failed to add hifi2_playback_dump sysfs files: %d\n",
				__func__, ret);
		/* add hifi2_capture_dump sysfs entries */
		ret = device_create_file(&pdev->dev, &dev_attr_hifi2_capture_dump.attr);
		if (ret < 0)
			dev_err(&pdev->dev,
				"%s: failed to add hifi2_capture_dump sysfs files: %d\n",
				__func__, ret);
	}
#endif
	return ret;
}

static int map_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
#ifdef CONFIG_SND_MMP_MAP_DUMP
	device_remove_file(&pdev->dev, &dev_attr_hifi1_playback_dump.attr);
	device_remove_file(&pdev->dev, &dev_attr_hifi1_capture_dump.attr);
	if (!map_get_lite_attr()) {
		device_remove_file(&pdev->dev, &dev_attr_hifi2_playback_dump.attr);
		device_remove_file(&pdev->dev, &dev_attr_hifi2_capture_dump.attr);
	}
#endif
	return 0;
}

static struct platform_driver map_audio_driver = {
	.driver		= {
		.name	= "marvell-map-audio",
		.owner	= THIS_MODULE,
		.of_match_table = mmp_map_dt_ids,
	},
	.probe		= map_audio_probe,
	.remove		= map_audio_remove,
};

module_platform_driver(map_audio_driver);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC Audio MAP");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:audio-map");
