/*
 * dbmd2-i2s.c  --  DBMD2 I2S interface
 *
 * Copyright (C) 2014 DSP Group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

static int dbmd2_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
		     unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_i2s_set_pll(struct snd_soc_dai *dai, int pll_id,
			     int source, unsigned int freq_in,
			     unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id,
				int div)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_i2s_set_tdm_slot(struct snd_soc_dai *dai,
				  unsigned int tx_mask, unsigned int rx_mask,
				  int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_i2s_set_channel_map(struct snd_soc_dai *dai,
				     unsigned int tx_num, unsigned int *tx_slot,
				     unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_i2s_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d tristate:%d\n", __func__,
		dai->id, tristate);

	/* TODO might check if dai->id is valid */
	return 0;
}

static int dbmd2_i2s_port_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d mute:%d\n", __func__,
		dai->id, mute);

	/* TODO might check if dai->id is valid */
	return 0;
}


static int dbmd2_i2s_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d\n", __func__, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s: playback stream\n", __func__);
	else
		dev_dbg(codec->dev, "%s: capture stream\n", __func__);

	return 0;
}

static void dbmd2_i2s_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d\n", __func__, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s: playback stream\n", __func__);
	else
		dev_dbg(codec->dev, "%s: capture stream\n", __func__);
}

static int dbmd2_i2s_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d\n", __func__, dai->id);
	/* TODO might check if dai->id is valid */

	dev_dbg(codec->dev, "%s: params_channels = %d\n", __func__,
		params_channels(params));

	switch (params_channels(params)) {
	case 2:
		break;
	default:
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: params_rate = %d\n", __func__,
		params_rate(params));
	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 48000:
		break;
	default:
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		break;
	default:
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s: playback stream\n", __func__);
	else
		dev_dbg(codec->dev, "%s: capture stream\n", __func__);

	return 0;
}

static int dbmd2_i2s_hw_free(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d\n", __func__, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s: playback stream\n", __func__);
	else
		dev_dbg(codec->dev, "%s: capture stream\n", __func__);

	return 0;
}

static int dbmd2_i2s_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d\n", __func__, dai->id);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s: playback stream\n", __func__);
	else
		dev_dbg(codec->dev, "%s: capture stream\n", __func__);

	return 0;
}

static int dbmd2_i2s_trigger(struct snd_pcm_substream *substream,
			     int cmd,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: dai:%d cmd=%d\n", __func__,
		dai->id, cmd);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_dbg(codec->dev, "%s: playback stream\n", __func__);
	else
		dev_dbg(codec->dev, "%s: capture stream\n", __func__);

	return 0;
}

struct snd_soc_dai_ops dbmd2_i2s_dai_ops = {
	.set_sysclk		= dbmd2_i2s_set_sysclk,
	.set_pll		= dbmd2_i2s_set_pll,
	.set_clkdiv		= dbmd2_i2s_set_clkdiv,
	.set_fmt		= dbmd2_i2s_set_dai_fmt,
	.set_tdm_slot		= dbmd2_i2s_set_tdm_slot,
	.set_channel_map	= dbmd2_i2s_set_channel_map,
	.set_tristate		= dbmd2_i2s_set_tristate,
	.digital_mute		= dbmd2_i2s_port_mute,
	.startup		= dbmd2_i2s_startup,
	.shutdown		= dbmd2_i2s_shutdown,
	.hw_params		= dbmd2_i2s_hw_params,
	.hw_free		= dbmd2_i2s_hw_free,
	.prepare		= dbmd2_i2s_prepare,
	.trigger		= dbmd2_i2s_trigger,
};
EXPORT_SYMBOL(dbmd2_i2s_dai_ops);
