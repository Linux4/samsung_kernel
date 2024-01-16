// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Sound Card Driver for Exynos8835
 *
 *  Copyright 2020 Samsung Electronics Co. Ltd.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pm_wakeup.h>

#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/samsung/abox.h>
#include <sound/samsung/vts.h>

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_EXYNOS_TFA9878)
#include "../../codecs/tfa9878/bigdata_tfa_sysfs_cb.h"
#endif

#if IS_ENABLED(CONFIG_SND_SOC_CS35L45)
#include <sound/cirrus/core.h>
#include <sound/cirrus/big_data.h>
#include "../../codecs/bigdata_cirrus_sysfs_cb.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif
#endif

#define ABOX_SOC_VERSION(m, n, r) (((m) << 16) | ((n) << 8) | (r))

#define ABOX_UAIF_DAI_ID(c, i)		(0xaf00 | (c) << 4 | (i))
#define ABOX_BE_DAI_ID(c, i)		(0xbe00 | (c) << 4 | (i))
#define CODEC_MAX			32
#define AUX_MAX				2
#define RDMA_COUNT			12
#if ABOX_SOC_VERSION(4, 0, 0) < CONFIG_SND_SOC_SAMSUNG_ABOX_VERSION
#define WDMA_COUNT			12
#else
#define WDMA_COUNT			8
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
#define VTS_COUNT			3
#else
#define VTS_COUNT			0
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
#define DP_COUNT			1
#else
#define DP_COUNT			0
#endif
#define DDMA_COUNT			6
#define DUAL_COUNT			WDMA_COUNT
#define UAIF_START			(RDMA_COUNT + WDMA_COUNT + VTS_COUNT\
					+ DP_COUNT + DDMA_COUNT + DUAL_COUNT)
#define UAIF_COUNT			7

struct sound_drvdata {
	struct device *dev;
	struct wakeup_source *ws;
};

static struct sound_drvdata exynos_drvdata;

static const struct snd_soc_ops rdma_ops = {
};

static const struct snd_soc_ops wdma_ops = {
};

#if IS_ENABLED(CONFIG_SND_SOC_CS35L45)
void cirrus_i2c_fail_log(const char *suffix)
{
	pr_info("%s(%s)\n", __func__, suffix);
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	sec_abc_send_event("MODULE=audio@INFO=spk_amp");
#else
	sec_abc_send_event("MODULE=audio@WARN=spk_amp");
#endif
#endif
}

void cirrus_amp_fail_event(const char *suffix)
{
	pr_info("%s(%s)\n", __func__, suffix);
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	sec_abc_send_event("MODULE=audio@INFO=spk_amp_short");
#else
	sec_abc_send_event("MODULE=audio@WARN=spk_amp_short");
#endif
#endif
}
#endif

static int uaif1_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct snd_soc_component *component = NULL;
#if IS_ENABLED(CONFIG_SND_SOC_CS35L45)
	struct snd_soc_dai *dai;
	struct snd_soc_dapm_context *dapm;
	int i;
#endif
	if (!codec_dai)
		return 0;

	component = codec_dai->component;
	if (!component)
		return 0;

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_EXYNOS_TFA9878)
	if (strstr(component->name, "tfa98xx"))
		register_tfa98xx_bigdata_cb(component);
#endif
#if IS_ENABLED(CONFIG_SND_SOC_CS35L45)
	if (strstr(component->name, "cs35l45")) {
		register_cirrus_bigdata_cb(component);
		cirrus_amp_register_i2c_error_callback("", cirrus_i2c_fail_log);
		cirrus_amp_register_i2c_error_callback("_r", cirrus_i2c_fail_log);
		cirrus_amp_register_error_callback("", cirrus_amp_fail_event);
		cirrus_amp_register_error_callback("_r", cirrus_amp_fail_event);

		for_each_rtd_codec_dais(rtd, i, dai) {
			dapm = snd_soc_component_get_dapm(dai->component);

			snd_soc_dapm_ignore_suspend(dapm, "SPK");
			snd_soc_dapm_ignore_suspend(dapm, "AP");
			snd_soc_dapm_ignore_suspend(dapm, "AMP Enable");
			snd_soc_dapm_ignore_suspend(dapm, "Entry");
			snd_soc_dapm_ignore_suspend(dapm, "Exit");
			snd_soc_dapm_sync(dapm);
		}
	}
#endif

	return 0;
}

static int uaif_ops_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	unsigned int width, clk, channels, rate;
	int ret = 0;
#if IS_ENABLED(CONFIG_SND_SOC_CS35L45)
	struct snd_soc_dai *codec_dai;
	unsigned int num_codecs = rtd->num_codecs;
	int i;
#endif

	switch (dai_link->id) {
	case ABOX_UAIF_DAI_ID(0, 1):
		width = params_width(params);
		channels = params_channels(params);
		rate = params_rate(params);
		clk = snd_soc_params_to_bclk(params);

		cpu_dai = asoc_rtd_to_cpu(rtd, 0);
		snd_soc_dai_set_sysclk(cpu_dai, 0, clk, SND_SOC_CLOCK_OUT);
		snd_soc_dai_set_tristate(cpu_dai, 0);

		dev_info(card->dev, "%s: %s-%d %dch, %dHz, %dbit, %dbytes\n",
				__func__, dai_link->name, substream->stream,
				channels, rate, width, params_buffer_bytes(params));

#if IS_ENABLED(CONFIG_SND_SOC_CS35L45)
		for (i = 0; i < num_codecs; i++) {
			codec_dai = asoc_rtd_to_codec(rtd, i);
			ret = snd_soc_component_set_sysclk(codec_dai->component,
						0, 0, clk,
						SND_SOC_CLOCK_IN);

			if (ret < 0)
				dev_err(card->dev, "%s: set amp sysclk failed: %d\n",
					codec_dai->name, ret);
			else
				dev_info(card->dev, "%s: set amp sysclk : %d\n",
					codec_dai->name, clk);

			ret = snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, channels, width);
			if ((ret < 0) && (ret != -ENOTSUPP))
				return ret;
		}
#endif
		break;
	default:
		break;
	}

	return ret;
}

static const struct snd_soc_ops uaif_ops = {
	.hw_params = uaif_ops_hw_params,
};

static int dsif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
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

static const struct snd_soc_ops udma_ops = {
};

static int exynos_late_probe(struct snd_soc_card *card)
{
	struct device *dev = card->dev;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *dai;
	struct snd_soc_dapm_context *dapm;
	const char *name;
	int i;

	dapm = &card->dapm;
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "RECEIVER");
	snd_soc_dapm_ignore_suspend(dapm, "SPEAKER");
	snd_soc_dapm_ignore_suspend(dapm, "BLUETOOTH MIC");
	snd_soc_dapm_ignore_suspend(dapm, "BLUETOOTH SPK");
	snd_soc_dapm_ignore_suspend(dapm, "USB MIC");
	snd_soc_dapm_ignore_suspend(dapm, "USB SPK");
	snd_soc_dapm_ignore_suspend(dapm, "FWD MIC");
	snd_soc_dapm_ignore_suspend(dapm, "FWD SPK");
	snd_soc_dapm_ignore_suspend(dapm, "VTS Virtual Output");
	snd_soc_dapm_ignore_suspend(dapm, "VINPUT_FM");
	snd_soc_dapm_sync(dapm);

	for_each_card_rtds(card, rtd) {
		for_each_rtd_cpu_dais(rtd, i, dai) {
			dapm = snd_soc_component_get_dapm(dai->component);
			if (dai->playback_widget) {
				name = dai->driver->playback.stream_name;
				dev_dbg(dev, "ignore suspend: %s\n", name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
			if (dai->capture_widget) {
				name = dai->driver->capture.stream_name;
				dev_dbg(dev, "ignore suspend: %s\n", name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
		}

		for_each_rtd_codec_dais(rtd, i, dai) {
			dapm = snd_soc_component_get_dapm(dai->component);
			if (dai->playback_widget) {
				name = dai->driver->playback.stream_name;
				dev_dbg(dev, "ignore suspend: %s\n", name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
			if (dai->capture_widget) {
				name = dai->driver->capture.stream_name;
				dev_dbg(dev, "ignore suspend: %s\n", name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
		}
	}

	return 0;
}

static const struct snd_soc_dapm_widget exynos_supply_widgets[] = {
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS1", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS2", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS3", 0, 0),
};

static int exynos_probe(struct snd_soc_card *card)
{
	struct device *dev = card->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(exynos_supply_widgets); i++) {
		const struct snd_soc_dapm_widget *w;
		struct regulator *r;

		w = &exynos_supply_widgets[i];
		switch (w->id) {
		case snd_soc_dapm_regulator_supply:
			r = regulator_get(dev, w->name);
			if (!IS_ERR_OR_NULL(r)) {
				regulator_put(r);
				snd_soc_dapm_new_control(&card->dapm, w);
			}
			break;
		default:
			dev_warn(dev, "ignored: %s\n", w->name);
			break;
		}
	}

	return 0;
}

static struct snd_soc_dai_link exynos_dai[100] = {
	{
		.name = "RDMA0",
		.stream_name = "RDMA0",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA1",
		.stream_name = "RDMA1",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA2",
		.stream_name = "RDMA2",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA3",
		.stream_name = "RDMA3",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA4",
		.stream_name = "RDMA4",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA5",
		.stream_name = "RDMA5",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA6",
		.stream_name = "RDMA6",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA7",
		.stream_name = "RDMA7",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA8",
		.stream_name = "RDMA8",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA9",
		.stream_name = "RDMA9",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA10",
		.stream_name = "RDMA10",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA11",
		.stream_name = "RDMA11",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "WDMA0",
		.stream_name = "WDMA0",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA1",
		.stream_name = "WDMA1",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA2",
		.stream_name = "WDMA2",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA3",
		.stream_name = "WDMA3",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA4",
		.stream_name = "WDMA4",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA5",
		.stream_name = "WDMA5",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA6",
		.stream_name = "WDMA6",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA7",
		.stream_name = "WDMA7",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
#if ABOX_SOC_VERSION(4, 0, 0) < CONFIG_SND_SOC_SAMSUNG_ABOX_VERSION
	{
		.name = "WDMA8",
		.stream_name = "WDMA8",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA9",
		.stream_name = "WDMA9",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA10",
		.stream_name = "WDMA10",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA11",
		.stream_name = "WDMA11",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
	{
		.name = "VTS-Trigger",
		.stream_name = "VTS-Trigger",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
	},
	{
		.name = "VTS-Record",
		.stream_name = "VTS-Record",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
	},
	{
		.name = "VTS-Internal",
		.stream_name = "VTS-Internal",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = vts_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
	{
		.name = "DP0 Audio",
		.stream_name = "DP0 Audio",
		.ignore_suspend = 1,
	},
#endif
	{
		.name = "WDMA0 DUAL",
		.stream_name = "WDMA0 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA1 DUAL",
		.stream_name = "WDMA1 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA2 DUAL",
		.stream_name = "WDMA2 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA3 DUAL",
		.stream_name = "WDMA3 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA4 DUAL",
		.stream_name = "WDMA4 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA5 DUAL",
		.stream_name = "WDMA5 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA6 DUAL",
		.stream_name = "WDMA6 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA7 DUAL",
		.stream_name = "WDMA7 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
#if ABOX_SOC_VERSION(4, 0, 0) < CONFIG_SND_SOC_SAMSUNG_ABOX_VERSION
	{
		.name = "WDMA8 DUAL",
		.stream_name = "WDMA8 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA9 DUAL",
		.stream_name = "WDMA9 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA10 DUAL",
		.stream_name = "WDMA10 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA11 DUAL",
		.stream_name = "WDMA11 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
#endif
	{
		.name = "DBG0",
		.stream_name = "DBG0",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DBG1",
		.stream_name = "DBG1",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DBG2",
		.stream_name = "DBG2",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DBG3",
		.stream_name = "DBG3",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DBG4",
		.stream_name = "DBG4",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DBG5",
		.stream_name = "DBG5",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "UAIF0",
		.stream_name = "UAIF0",
		.id = ABOX_UAIF_DAI_ID(0, 0),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF1",
		.stream_name = "UAIF1",
		.id = ABOX_UAIF_DAI_ID(0, 1),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.init = uaif1_init,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF2",
		.stream_name = "UAIF2",
		.id = ABOX_UAIF_DAI_ID(0, 2),
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
		.id = ABOX_UAIF_DAI_ID(0, 3),
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
		.id = ABOX_UAIF_DAI_ID(0, 4),
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
		.id = ABOX_UAIF_DAI_ID(0, 5),
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
		.id = ABOX_UAIF_DAI_ID(0, 6),
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
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &dsif_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "SPDY",
		.stream_name = "SPDY",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "UDMA RD0",
		.stream_name = "UDMA RD0",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &udma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "UDMA WR0",
		.stream_name = "UDMA WR0",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &udma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "UDMA WR0 DUAL",
		.stream_name = "UDMA WR0 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "UDMA DBG0",
		.stream_name = "UDMA DBG0",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "RDMA0 BE",
		.stream_name = "RDMA0 BE",
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
		.id = ABOX_BE_DAI_ID(1, 4),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA5 BE",
		.stream_name = "WDMA5 BE",
		.id = ABOX_BE_DAI_ID(1, 5),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA6 BE",
		.stream_name = "WDMA6 BE",
		.id = ABOX_BE_DAI_ID(1, 6),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA7 BE",
		.stream_name = "WDMA7 BE",
		.id = ABOX_BE_DAI_ID(1, 7),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
#if ABOX_SOC_VERSION(4, 0, 0) < CONFIG_SND_SOC_SAMSUNG_ABOX_VERSION
	{
		.name = "WDMA8 BE",
		.stream_name = "WDMA8 BE",
		.id = ABOX_BE_DAI_ID(1, 8),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA9 BE",
		.stream_name = "WDMA9 BE",
		.id = ABOX_BE_DAI_ID(1, 9),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA10 BE",
		.stream_name = "WDMA10 BE",
		.id = ABOX_BE_DAI_ID(1, 10),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA11 BE",
		.stream_name = "WDMA11 BE",
		.id = ABOX_BE_DAI_ID(1, 11),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
#endif
	{
		.name = "USB",
		.stream_name = "USB",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "FWD",
		.stream_name = "FWD",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
	},
};

static int left_speaker(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		break;
	}

	return 0;
}

static int right_speaker(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
#if IS_ENABLED(CONFIG_SND_SOC_CS35L45)
		cirrus_bd_store_values("_0");
		cirrus_bd_store_values("_1");
#endif
		break;
	}

	return 0;
}

static int get_sound_wakelock(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct sound_drvdata *drvdata = &exynos_drvdata;
	unsigned int val = drvdata->ws->active;

	dev_dbg(drvdata->dev, "%s: %d\n", __func__, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int set_sound_wakelock(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct sound_drvdata *drvdata = &exynos_drvdata;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(drvdata->dev, "%s: %d\n", __func__, val);

	if (val)
		__pm_stay_awake(drvdata->ws);
	else
		__pm_relax(drvdata->ws);

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

static const struct snd_kcontrol_new exynos_controls[] = {
	SOC_DAPM_PIN_SWITCH("DMIC1"),
	SOC_DAPM_PIN_SWITCH("DMIC2"),
	SOC_DAPM_PIN_SWITCH("DMIC3"),
	SOC_DAPM_PIN_SWITCH("SPEAKER"),
	SOC_DAPM_PIN_SWITCH("RECEIVER"),
	SOC_SINGLE_BOOL_EXT("Sound Wakelock",
			0, get_sound_wakelock, set_sound_wakelock),
};

static const struct snd_soc_dapm_widget exynos_widgets[] = {
	SND_SOC_DAPM_MIC("DMIC1", NULL),
	SND_SOC_DAPM_MIC("DMIC2", NULL),
	SND_SOC_DAPM_MIC("DMIC3", NULL),
	SND_SOC_DAPM_SPK("RECEIVER", left_speaker),
	SND_SOC_DAPM_SPK("SPEAKER", right_speaker),
	SND_SOC_DAPM_MIC("BLUETOOTH MIC", NULL),
	SND_SOC_DAPM_SPK("BLUETOOTH SPK", NULL),
	SND_SOC_DAPM_MIC("USB MIC", NULL),
	SND_SOC_DAPM_SPK("USB SPK", NULL),
	SND_SOC_DAPM_MIC("FWD MIC", NULL),
	SND_SOC_DAPM_SPK("FWD SPK", NULL),
	SND_SOC_DAPM_OUTPUT("VTS Virtual Output"),
	SND_SOC_DAPM_MUX("VTS Virtual Output Mux", SND_SOC_NOPM, 0, 0,
			&vts_output_mux[0]),
	SND_SOC_DAPM_INPUT("VINPUT_FM"),
};

static const struct snd_soc_dapm_route exynos_routes[] = {
	{"VTS Virtual Output Mux", "DMIC1", "DMIC1"},
};

static struct snd_soc_codec_conf codec_conf[CODEC_MAX];

static struct snd_soc_aux_dev aux_dev[AUX_MAX];

static struct snd_soc_card exynos_sound = {
	.name = "Exynos-Sound",
	.owner = THIS_MODULE,
	.dai_link = exynos_dai,
	.num_links = ARRAY_SIZE(exynos_dai),

	.probe = exynos_probe,
	.late_probe = exynos_late_probe,

	.controls = exynos_controls,
	.num_controls = ARRAY_SIZE(exynos_controls),
	.dapm_widgets = exynos_widgets,
	.num_dapm_widgets = ARRAY_SIZE(exynos_widgets),
	.dapm_routes = exynos_routes,
	.num_dapm_routes = ARRAY_SIZE(exynos_routes),

	.drvdata = (void *)&exynos_drvdata,

	.codec_conf = codec_conf,
	.num_configs = ARRAY_SIZE(codec_conf),

	.aux_dev = aux_dev,
	.num_aux_devs = ARRAY_SIZE(aux_dev),
};

static int read_platform(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret = 0;
	struct snd_soc_dai_link_component *platform;

	np = of_get_child_by_name(np, "platform");
	if (!np)
		return 0;

	platform = devm_kcalloc(dev, 1, sizeof(*platform), GFP_KERNEL);
	if (!platform) {
		ret = -ENOMEM;
		goto out;
	}

	platform->of_node = of_parse_phandle(np, "sound-dai", 0);
	if (!platform->of_node) {
		ret = -ENODEV;
		goto out;
	}
out:
	if (ret < 0) {
		if (platform)
			devm_kfree(dev, platform);
	} else {
		dai_link->platforms = platform;
		dai_link->num_platforms = 1;
	}
	of_node_put(np);

	return ret;
}

static int read_cpu(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret = 0;
	struct snd_soc_dai_link_component *cpu;

	np = of_get_child_by_name(np, "cpu");
	if (!np)
		return 0;

	cpu = devm_kcalloc(dev, 1, sizeof(*cpu), GFP_KERNEL);
	if (!cpu) {
		ret = -ENOMEM;
		goto out;
	}

	cpu->of_node = of_parse_phandle(np, "sound-dai", 0);
	if (!cpu->of_node) {
		ret = -ENODEV;
		goto out;
	}

	ret = snd_soc_of_get_dai_name(np, &cpu->dai_name);
out:
	if (ret < 0) {
		if (cpu)
			devm_kfree(dev, cpu);
	} else {
		dai_link->cpus = cpu;
		dai_link->num_cpus = 1;
	}
	of_node_put(np);

	return ret;
}

SND_SOC_DAILINK_DEF(dailink_comp_dummy, DAILINK_COMP_ARRAY(COMP_DUMMY()));

static int read_codec(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret;

	np = of_get_child_by_name(np, "codec");
	if (!np) {
		dai_link->codecs = dailink_comp_dummy;
		dai_link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
		return 0;
	}

	ret = snd_soc_of_get_dai_link_codecs(dev, np, dai_link);
	of_node_put(np);

	return ret;
}

static void exynos_register_card_work_func(struct work_struct *work)
{
	struct snd_soc_card *card = &exynos_sound;
	struct device *dev = card->dev;
	int retry = 10, ret;

	do {
		ret = devm_snd_soc_register_card(dev, card);
		if (!ret) {
			dev_info(dev, "sound card registered\n");
			break;
		}
		if (ret < 0 && ret != -EPROBE_DEFER) {
			dev_err(dev, "sound card register fail: %d\n", ret);
			break;
		}
		fsleep(1 * USEC_PER_SEC);
		dev_info(dev, "retry sound card register\n");
	} while (--retry > 0);

	if (ret < 0)
		dev_err(dev, "sound card register failed: %d\n", ret);

}
static DECLARE_WORK(exynos_register_card_work, exynos_register_card_work_func);

static int exynos_sound_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snd_soc_card *card = &exynos_sound;
	struct sound_drvdata *drvdata = card->drvdata;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dai;
	struct snd_soc_dai_link *link;
	int i, ret, nlink = 0;

	drvdata->dev = card->dev = dev;
	drvdata->ws = wakeup_source_register(NULL, "exynos-sound");

	snd_soc_card_set_drvdata(card, drvdata);

	for_each_child_of_node(np, dai) {
		link = &exynos_dai[nlink];

		if (!link->name)
			link->name = dai->name;
		if (!link->stream_name)
			link->stream_name = dai->name;

		if (!link->cpus) {
			ret = read_cpu(dai, dev, link);
			if (ret < 0) {
				dev_err(dev, "Failed to parse cpu DAI for %s: %d\n",
						dai->name, ret);
				return ret;
			}
		}

		if (!link->platforms) {
			ret = read_platform(dai, dev, link);
			if (ret < 0) {
				dev_warn(dev, "Failed to parse platform for %s: %d\n",
						dai->name, ret);
				ret = 0;
			}
		}

		if (!link->codecs) {
			ret = read_codec(dai, dev, link);
			if (ret < 0) {
				dev_warn(dev, "Failed to parse codec DAI for %s: %d\n",
						dai->name, ret);
				ret = 0;
			}
		}

		link->dai_fmt = snd_soc_daifmt_parse_format(dai, NULL);
		link->dai_fmt |= snd_soc_daifmt_parse_clock_provider_as_flag(dai, NULL);

		if (++nlink == card->num_links)
			break;
	}

	if (!nlink) {
		dev_err(dev, "No DAIs specified\n");
		return -EINVAL;
	}

	/* Dummy pcm to adjust ID of PCM added by topology */
	for (; nlink < card->num_links; nlink++) {
		link = &exynos_dai[nlink];

		if (!link->name)
			link->name = devm_kasprintf(dev, GFP_KERNEL,
					"dummy%d", nlink);
		if (!link->stream_name)
			link->stream_name = link->name;

		if (!link->cpus) {
			link->cpus = dailink_comp_dummy;
			link->num_cpus = ARRAY_SIZE(dailink_comp_dummy);
		}

		if (!link->codecs) {
			link->codecs = dailink_comp_dummy;
			link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
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
		codec_conf[i].dlc.of_node = of_parse_phandle(np, "samsung,codec",
				i);
		if (!codec_conf[i].dlc.of_node)
			break;

		ret = of_property_read_string_index(np, "samsung,prefix", i,
				&codec_conf[i].name_prefix);
		if (ret < 0)
			codec_conf[i].name_prefix = "";
	}
	card->num_configs = i;

	for (i = 0; i < ARRAY_SIZE(aux_dev); i++) {
		aux_dev[i].dlc.of_node = of_parse_phandle(np, "samsung,aux", i);
		if (!aux_dev[i].dlc.of_node)
			break;
	}
	card->num_aux_devs = i;

	schedule_work(&exynos_register_card_work);

	return ret;
}

static int exynos_sound_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct sound_drvdata *drvdata;
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_dai_link_component *cpu, *platform;
	int i;

	card = platform_get_drvdata(pdev);
	if (!card)
		return 0;

	drvdata = snd_soc_card_get_drvdata(card);
	if (!drvdata)
		return 0;

	for (dai_link = exynos_dai; dai_link - exynos_dai <
			ARRAY_SIZE(exynos_dai); dai_link++) {
		for_each_link_cpus(dai_link, i, cpu) {
			if (cpu->of_node)
				of_node_put(cpu->of_node);
		}

		for_each_link_platforms(dai_link, i, platform) {
			if (platform->of_node)
				of_node_put(platform->of_node);
		}

		snd_soc_of_put_dai_link_codecs(dai_link);
	}

	wakeup_source_unregister(drvdata->ws);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id exynos_sound_of_match[] = {
	{ .compatible = "samsung,exynos8835-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_sound_of_match);
#endif /* CONFIG_OF */

static struct platform_driver exynos_sound_driver = {
	.driver		= {
		.name	= "exynos8835-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(exynos_sound_of_match),
	},

	.probe		= exynos_sound_probe,
	.remove		= exynos_sound_remove,
};

module_platform_driver(exynos_sound_driver);

MODULE_DESCRIPTION("ALSA SoC Exynos Sound Driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.wolfsonmicro.com>");
MODULE_AUTHOR("Shinhyung Kang <s47.kang@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:exynos8835-sound");
