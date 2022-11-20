/*
 *  universal4415_wm1811_slsi.c
 *
 *  Copyright (c) 2013 Samsung Electronics Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>

#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/gpio.h>

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>

#include <linux/exynos_audio.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "s3c-i2s-v2.h"
#include "../codecs/wm8994.h"


#define UNIVERSAL4415_DEFAULT_MCLK1	24000000
#define UNIVERSAL4415_DEFAULT_MCLK2	32768
#define UNIVERSAL4415_DEFAULT_SYNC_CLK	11289600

#define WM1811_JACKDET_MODE_NONE  0x0000
#define WM1811_JACKDET_MODE_JACK  0x0100
#define WM1811_JACKDET_MODE_MIC   0x0080
#define WM1811_JACKDET_MODE_AUDIO 0x0180

#define WM1811_JACKDET_BTN0	0x04
#define WM1811_JACKDET_BTN1	0x10
#define WM1811_JACKDET_BTN2	0x08

#define WM1811_MIC_IRQ_NUM	(IRQ_BOARD_AUDIO_START + WM8994_IRQ_MIC1_DET)
#define WM1811_JACKDET_IRQ_NUM	(IRQ_BOARD_AUDIO_START + WM8994_IRQ_GPIO(1))

#define MIC_DISABLE     0
#define MIC_ENABLE      1
#define MIC_FORCE_DISABLE       2
#define MIC_FORCE_ENABLE        3

static struct wm8958_micd_rate universal4415_det_rates[] = {
	{ UNIVERSAL4415_DEFAULT_MCLK2,     true,  0,  0 },
	{ UNIVERSAL4415_DEFAULT_MCLK2,    false,  0,  0 },
	{ UNIVERSAL4415_DEFAULT_SYNC_CLK,  true,  7,  7 },
	{ UNIVERSAL4415_DEFAULT_SYNC_CLK, false,  7,  7 },
};

static struct wm8958_micd_rate universal4415_jackdet_rates[] = {
	{ UNIVERSAL4415_DEFAULT_MCLK2,     true,  0,  0 },
	{ UNIVERSAL4415_DEFAULT_MCLK2,    false,  0,  0 },
	{ UNIVERSAL4415_DEFAULT_SYNC_CLK,  true, 12, 12 },
	{ UNIVERSAL4415_DEFAULT_SYNC_CLK, false,  7,  8 },
};

struct wm1811_machine_priv {
	struct clk *pll;
	struct clk *clk;
	unsigned int pll_out;
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct delayed_work mic_work;
	struct wake_lock jackdet_wake_lock;
	void (*set_sub_mic_f) (int on);
	int (*get_g_det_value_f) (void);
	int (*get_g_det_irq_num_f) (void);
};

static int aif2_mode;
const char *aif2_mode_text[] = {
	"Slave", "Master"
};

const char *mic_bias_mode_text[] = {
	"Disable", "Force Disable", "Enable", "Force Enable"
};

static int sub_mic_bias_mode;

#ifndef CONFIG_SEC_DEV_JACK
/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;
#endif

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static const struct soc_enum mic_bias_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mic_bias_mode_text), mic_bias_mode_text),
};

static const struct soc_enum sub_bias_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mic_bias_mode_text), mic_bias_mode_text),
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

	pr_info("set aif2 mode : %s\n", aif2_mode_text[aif2_mode]);

	return 0;
}

static void universal4415_micd_set_rate(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int best, i, sysclk, val;
	bool idle;
	const struct wm8958_micd_rate *rates = NULL;
	int num_rates = 0;

	idle = !wm8994->jack_mic;

	sysclk = snd_soc_read(codec, WM8994_CLOCKING_1);
	if (sysclk & WM8994_SYSCLK_SRC)
		sysclk = wm8994->aifclk[1];
	else
		sysclk = wm8994->aifclk[0];

	if (wm8994->jackdet) {
		rates = universal4415_jackdet_rates;
		num_rates = ARRAY_SIZE(universal4415_jackdet_rates);
		wm8994->pdata->micd_rates = universal4415_jackdet_rates;
		wm8994->pdata->num_micd_rates = num_rates;
	} else {
		rates = universal4415_det_rates;
		num_rates = ARRAY_SIZE(universal4415_det_rates);
		wm8994->pdata->micd_rates = universal4415_det_rates;
		wm8994->pdata->num_micd_rates = num_rates;
	}

	best = 0;
	for (i = 0; i < num_rates; i++) {
		if (rates[i].idle != idle)
			continue;
		if (abs(rates[i].sysclk - sysclk) <
		    abs(rates[best].sysclk - sysclk))
			best = i;
		else if (rates[best].idle != idle)
			best = i;
	}

	val = rates[best].start << WM8958_MICD_BIAS_STARTTIME_SHIFT
		| rates[best].rate << WM8958_MICD_RATE_SHIFT;

	snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
			    WM8958_MICD_BIAS_STARTTIME_MASK |
			    WM8958_MICD_RATE_MASK, val);
}

static int universal4415_wm1811_aif1_hw_params(
		struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret;

	/* AIF1CLK should be >=3MHz for optimal performance */
	if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

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

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1,
				  WM8994_FLL_SRC_MCLK1,
				  UNIVERSAL4415_DEFAULT_MCLK1,
				  pll_out);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to start FLL1: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to switch to FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
				     0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	return 0;
}

static int universal4415_wm1811_aif2_hw_params(
		struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;
	int prate;
	int bclk;

	dev_info(codec_dai->dev, "%s ++\n", __func__);
	prate = params_rate(params);
	switch (params_rate(params)) {
	case 8000:
	case 16000:
	       break;
	default:
		dev_warn(codec_dai->dev, "Unsupported LRCLK %d, falling back to 8000Hz\n",
				(int)params_rate(params));
		prate = 8000;
	}

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A
			| SND_SOC_DAIFMT_IB_NF
			| SND_SOC_DAIFMT_CBS_CFS);

	if (ret < 0)
		return ret;

	bclk = 2048000;

	if (aif2_mode == 0) {
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
					WM8994_FLL_SRC_BCLK,
					bclk, prate * 256);
	} else {
		ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
					  WM8994_FLL_SRC_MCLK1,
					  UNIVERSAL4415_DEFAULT_MCLK1,
					  prate * 256);
	}

	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to configure FLL2: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2,
				     prate * 256, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to switch to FLL2: %d\n", ret);

	dev_info(codec_dai->dev, "%s --\n", __func__);
	return 0;
}

static int universal4415_wm1811_aif3_hw_params(
		struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	pr_err("%s: enter\n", __func__);
	return 0;
}

static int get_sub_mic_bias_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = sub_mic_bias_mode;
	return 0;
}

static int set_sub_mic_bias_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);
	int status = 0;

	status = ucontrol->value.integer.value[0];

	switch (status) {
	case MIC_FORCE_ENABLE:
		sub_mic_bias_mode = status;
		snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				WM8994_MICB1_ENA, WM8994_MICB1_ENA);

		if (wm1811->set_sub_mic_f)
			wm1811->set_sub_mic_f(1);
		break;
	case MIC_ENABLE:
		snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				WM8994_MICB1_ENA, WM8994_MICB1_ENA);
		if (wm1811->set_sub_mic_f)
			wm1811->set_sub_mic_f(1);
		if (sub_mic_bias_mode != MIC_FORCE_ENABLE)
			msleep(100);
		break;
	case MIC_FORCE_DISABLE:
		sub_mic_bias_mode = status;
		snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				WM8994_MICB1_ENA, 0);

		if (wm1811->set_sub_mic_f)
			wm1811->set_sub_mic_f(0);
		break;
	case MIC_DISABLE:
		if (sub_mic_bias_mode != MIC_FORCE_ENABLE) {
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
					WM8994_MICB1_ENA, 0);
			if (wm1811->set_sub_mic_f)
				wm1811->set_sub_mic_f(0);
		} else
			dev_info(codec->dev,
				"SKIP submic disable=%d\n", status);
		break;
	default:
		break;
	}

	dev_info(codec->dev, "sub_mic_bias_mod=%d: status=%d\n",
				sub_mic_bias_mode, status);

	return 0;

}

static int universal4415_ext_submicbias(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct wm1811_machine_priv *wm1811
			= snd_soc_card_get_drvdata(card);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (wm1811->set_sub_mic_f)
			wm1811->set_sub_mic_f(1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (wm1811->set_sub_mic_f)
			wm1811->set_sub_mic_f(0);
		break;
	}
	return 0;
}

/*
 * universal4415 WM1811 DAI operations.
 */
static struct snd_soc_ops universal4415_wm1811_aif1_ops = {
	.hw_params = universal4415_wm1811_aif1_hw_params,
};

static struct snd_soc_ops universal4415_wm1811_aif2_ops = {
	.hw_params = universal4415_wm1811_aif2_hw_params,
};

static struct snd_soc_ops universal4415_wm1811_aif3_ops = {
	.hw_params = universal4415_wm1811_aif3_hw_params,
};

static const struct snd_kcontrol_new universal4415_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("FM In"),
	SOC_DAPM_PIN_SWITCH("LINE"),
	SOC_DAPM_PIN_SWITCH("HDMI"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),

	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
	get_aif2_mode, set_aif2_mode),

	SOC_ENUM_EXT("SubMicBias Mode", mic_bias_mode_enum[0],
	get_sub_mic_bias_mode, set_sub_mic_bias_mode),
};

const struct snd_soc_dapm_widget universal4415_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("LINE", NULL),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", universal4415_ext_submicbias),
	SND_SOC_DAPM_LINE("FM In", NULL),
};

const struct snd_soc_dapm_route universal4415_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "RCV", NULL, "HPOUT2N" },
	{ "RCV", NULL, "HPOUT2P" },

	{ "LINE", NULL, "LINEOUT2N" },
	{ "LINE", NULL, "LINEOUT2P" },

	{ "IN2LP:VXRN", NULL, "Main Mic" },
	{ "IN2LN", NULL, "Main Mic" },

	{ "IN1LP", NULL, "Headset Mic" },
	{ "IN1LN", NULL, "Headset Mic" },

	{ "IN1RP", NULL, "Sub Mic" },
	{ "IN1RN", NULL, "Sub Mic" },

	{ "IN2LP:VXRN", NULL, "MICBIAS1" },
	{ "MICBIAS1", NULL, "Main Mic" },

	{ "IN1LP", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },
};

static struct snd_soc_dai_driver universal4415_ext_dai[] = {
	{
		.name = "universal4415.cp",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "universal4415.bt",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link universal4415_dai[] = {
	{ /* Primary DAI i/f */
		.name = "WM8994 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.ops = &universal4415_wm1811_aif1_ops,
	},
	{ /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "wm8994-aif1",
#ifndef CONFIG_SND_SAMSUNG_USE_IDMA
		.platform_name = "samsung-audio",
#else
		.platform_name = "samsung-idma",
#endif
		.codec_name = "wm8994-codec",
		.ops = &universal4415_wm1811_aif1_ops,
	},
	{
		.name = "WM8994 Voice",
		.stream_name = "Voice Tx/Rx",
		.cpu_dai_name = "universal4415.cp",
		.codec_dai_name = "wm8994-aif2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm8994-codec",
		.ops = &universal4415_wm1811_aif2_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "WM8994 BT",
		.stream_name = "BT Tx/Rx",
		.cpu_dai_name = "universal4415.bt",
		.codec_dai_name = "wm8994-aif3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm8994-codec",
		.ops = &universal4415_wm1811_aif3_ops,
		.ignore_suspend = 1,
	},
};

#ifndef CONFIG_SEC_DEV_JACK
static ssize_t earjack_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	int report = 0;

	if ((wm8994->micdet[0].jack->status & SND_JACK_HEADPHONE) ||
		(wm8994->micdet[0].jack->status & SND_JACK_HEADSET)) {
		report = 1;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", report);
}

static ssize_t earjack_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_key_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	int report = 0;

	if (wm8994->micdet[0].jack->status & SND_JACK_BTN_0)
		report = 1;

	return snprintf(buf, PAGE_SIZE, "%d\n", report);
}

static ssize_t earjack_key_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	wm8994->mic_detecting = false;
	wm8994->jack_mic = true;

	universal4415_micd_set_rate(codec);

	if ((!size) || (buf[0] != '1')) {
		snd_soc_jack_report(wm8994->micdet[0].jack,
				    0, SND_JACK_HEADSET);
		dev_info(codec->dev, "Forced remove microphone\n");
	} else {

		snd_soc_jack_report(wm8994->micdet[0].jack,
				    SND_JACK_HEADSET, SND_JACK_HEADSET);
		dev_info(codec->dev, "Forced detect microphone\n");
	}

	return size;
}

static ssize_t reselect_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);
	return 0;
}

static ssize_t reselect_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);
	return size;
}

static DEVICE_ATTR(reselect_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		reselect_jack_show, reselect_jack_store);

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);
#endif

static int universal4415_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct wm1811_machine_priv *wm1811 =
				snd_soc_card_get_drvdata(codec->card);
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct wm8994 *control = codec->control_data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	const struct exynos_sound_platform_data *sound_pdata;
	int ret;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK1,
				     UNIVERSAL4415_DEFAULT_MCLK1,
				     SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to boot clocking\n");

	/* Force AIF1CLK on as it will be master for jack detection */
	if (wm8994->revision > 1) {
		ret = snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");
		if (ret < 0)
			dev_err(codec->dev,
				"Failed to enable AIF1CLK: %d\n", ret);
	}

	snd_soc_dapm_ignore_suspend(&card->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HP");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(&card->dapm, "AIF1ADCDAT");
	snd_soc_dapm_ignore_suspend(&card->dapm, "FM In");
	snd_soc_dapm_ignore_suspend(&card->dapm, "LINE");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HDMI");

	wm1811->codec = codec;

	/* Configure Hidden registers of WM1811 to conform of
	 * the Samsung's standard Revision 1.1 for earphones */
	snd_soc_write(codec, 0x102, 0x3);
	snd_soc_write(codec, 0xcb, 0x5151);
	snd_soc_write(codec, 0xd3, 0x3f3f);
	snd_soc_write(codec, 0xd4, 0x3f3f);
	snd_soc_write(codec, 0xd5, 0x3f3f);
	snd_soc_write(codec, 0xd6, 0x3228);
	snd_soc_write(codec, 0x102, 0x0);

	/* Samsung-specific customization of MICBIAS levels */
	snd_soc_write(codec, 0xd1, 0x87);
	snd_soc_write(codec, 0x3b, 0x9);
	snd_soc_write(codec, 0x3c, 0x2);

	universal4415_micd_set_rate(codec);

#ifdef CONFIG_SEC_DEV_JACK
	/* By default use idle_bias_off, will override for WM8994 */
	codec->dapm.idle_bias_off = 0;
#else /* CONFIG_SEC_DEV_JACK */
	wm1811->jack.status = 0;

	ret = snd_soc_jack_new(codec, "universal4415 Jack",
				SND_JACK_HEADSET | SND_JACK_BTN_0 |
				SND_JACK_BTN_1 | SND_JACK_BTN_2,
				&wm1811->jack);

	if (ret < 0)
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_MEDIA: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_1,
							KEY_VOLUMEDOWN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEUP: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_2,
							KEY_VOLUMEUP);

	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEDOWN: %d\n", ret);
#if 0
	if (wm8994->revision > 1) {
		dev_info(codec->dev, "wm1811: Rev %c support mic detection\n",
			'A' + wm8994->revision);
		ret = wm8958_mic_detect(codec, &wm1811->jack,
				universal4415_micdet, wm1811);

		if (ret < 0)
			dev_err(codec->dev, "Failed start detection: %d\n",
				ret);
	} else {
		dev_info(codec->dev, "wm1811: Rev %c doesn't support mic detection\n",
			'A' + wm8994->revision);
		codec->dapm.idle_bias_off = 0;
	}
#else
	codec->dapm.idle_bias_off = 0;
#endif
	/* To wakeup for earjack event in suspend mode */
	enable_irq_wake(control->irq);

	wake_lock_init(&wm1811->jackdet_wake_lock,
					WAKE_LOCK_SUSPEND, 
					"universal4415_jackdet");

	/* To support PBA function test */
	jack_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(jack_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(jack_class, NULL, 0, codec, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	if (device_create_file(jack_dev, &dev_attr_key_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_key_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_reselect_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_reselect_jack.attr.name);

#endif /* CONFIG_SEC_DEV_JACK */
	sound_pdata = exynos_sound_get_platform_data();

	if (sound_pdata) {
		wm8994->hubs.dcs_codes_l = sound_pdata->dcs_offset_l;
		wm8994->hubs.dcs_codes_r = sound_pdata->dcs_offset_r;
	}

	return snd_soc_dapm_sync(&codec->dapm);
}

static int universal4415_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *aif2_dai = card->rtd[1].codec_dai;
	struct wm1811_machine_priv *machine =
				snd_soc_card_get_drvdata(codec->card);

	if (!codec->active) {
		int ret;
		ret = snd_soc_dai_set_sysclk(aif2_dai,
					     WM8994_SYSCLK_MCLK2,
					     UNIVERSAL4415_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);

		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2: %d\n",
				ret);

		ret = snd_soc_dai_set_pll(aif2_dai, WM8994_FLL2, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL2\n");

		ret = snd_soc_dai_set_sysclk(aif1_dai,
					     WM8994_SYSCLK_MCLK2,
					     UNIVERSAL4415_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2\n");

		ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL1\n");

		clk_disable(machine->clk);
		exynos_xxti_sys_powerdown(0);
	}

	return 0;
}

static int universal4415_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct wm1811_machine_priv *machine =
				snd_soc_card_get_drvdata(codec->card);
	int ret;
	clk_enable(machine->clk);
	exynos_xxti_sys_powerdown(1);

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1,
				  WM8994_FLL_SRC_MCLK1,
				  UNIVERSAL4415_DEFAULT_MCLK1,
				  machine->pll_out);

	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to start FLL1: %d\n", ret);

	/* Then switch AIF1CLK to it */
	ret = snd_soc_dai_set_sysclk(aif1_dai,
				     WM8994_SYSCLK_FLL1,
				     machine->pll_out,
				     SND_SOC_CLOCK_IN);

	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to switch to FLL1: %d\n", ret);

	return 0;
}

static struct snd_soc_card universal4415 = {
	.name = "Universal4415 WM1811",
	.owner = THIS_MODULE,
	.dai_link = universal4415_dai,

	/* If you want to use sec_fifo device,
	 * changes the num_link = 2 or ARRAY_SIZE(universal4415_dai). */
	.num_links = ARRAY_SIZE(universal4415_dai),

	.controls = universal4415_controls,
	.num_controls = ARRAY_SIZE(universal4415_controls),
	.dapm_widgets = universal4415_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal4415_dapm_widgets),
	.dapm_routes = universal4415_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal4415_dapm_routes),

	.late_probe = universal4415_late_probe,

	.suspend_post = universal4415_card_suspend_post,
	.resume_pre = universal4415_card_resume_pre,
};

static int __devinit snd_universal4415_probe(struct platform_device *pdev)
{
	struct wm1811_machine_priv *wm1811;
	const struct exynos_sound_platform_data *sound_pdata;
	int ret = 0;

	wm1811 = kzalloc(sizeof *wm1811, GFP_KERNEL);
	if (!wm1811) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	wm1811->clk = clk_get(&pdev->dev, "clkout");
	if (IS_ERR(wm1811->clk)) {
		pr_err("failed to get system_clk\n");
		ret = PTR_ERR(wm1811->clk);
		goto err_clk_get;
	}

	/* Start the reference clock for the codec's FLL */
	clk_enable(wm1811->clk);

	wm1811->pll_out = 44100 * 512; /* default sample rate */

	ret = snd_soc_register_dais(&pdev->dev, universal4415_ext_dai,
					ARRAY_SIZE(universal4415_ext_dai));
	if (ret != 0)
		pr_err("Failed to register external DAIs: %d\n", ret);

	snd_soc_card_set_drvdata(&universal4415, wm1811);

	universal4415.dev = &pdev->dev;

	ret = snd_soc_register_card(&universal4415);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		goto err_register_card;
	}
	pr_info("wm1811: %s: register card\n", __func__);

	sound_pdata = exynos_sound_get_platform_data();
	if (!sound_pdata) {
		pr_info("%s: don't use sound pdata\n", __func__);
		goto err_register_card;
	}

	if (sound_pdata->set_ext_sub_mic)
		wm1811->set_sub_mic_f = sound_pdata->set_ext_sub_mic;

	if (sound_pdata->get_ground_det_value)
		wm1811->get_g_det_value_f = sound_pdata->get_ground_det_value;

	return 0;

err_register_card:
	clk_put(wm1811->clk);
err_clk_get:
	kfree(wm1811);
err_kzalloc:
	return ret;
}

static int __devexit snd_universal4415_remove(struct platform_device *pdev)
{
	struct wm1811_machine_priv *wm1811 =
				snd_soc_card_get_drvdata(&universal4415);

	snd_soc_unregister_card(&universal4415);
	clk_disable(wm1811->clk);
	clk_put(wm1811->clk);
	kfree(wm1811);

	return 0;
}

static struct platform_driver snd_universal4415_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "universal4415-i2s",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_universal4415_probe,
	.remove = __devexit_p(snd_universal4415_remove),
};
module_platform_driver(snd_universal4415_driver);

MODULE_AUTHOR("JS. Park <aitdark.park@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC universal4415 WM1811");
MODULE_LICENSE("GPL");

