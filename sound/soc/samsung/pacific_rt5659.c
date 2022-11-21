/*
 *  pacific_arizona.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/tlv.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>

#include <linux/mfd/arizona/registers.h>
#include <linux/mfd/arizona/core.h>

#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include <mach/exynos-audio.h>

#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/rt5659.h"

#define WAKE_LOCK_TIME		(HZ * 5)	/* 5 sec */
/* PACIFIC use CLKOUT from AP */
#define PACIFIC_MCLK_FREQ	24000000

#ifdef CONFIG_SND_CODEC_CONTROL_MCLK
extern void es325_audio_set_mclk(bool enable, bool forced);
#endif

int rt5659_jack_status_check(void);

struct rt5659_machine_priv {
	struct clk *mclk;
	struct snd_soc_codec *codec;
	struct snd_soc_jack rt5659_hp_jack;
	struct snd_soc_jack_gpio rt5659_hp_jack_gpio;
	int sub_mic_bias_gpio;
	bool jd_status;
	struct timer_list jd_check_timer;
	struct work_struct jd_check_work;
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL_RT5659
	bool earmic_enabled;
#endif
	struct mutex mutex;
	struct wake_lock jack_wake_lock;
	int det_delay_time;
};

static struct rt5659_machine_priv pacific_rt5659_priv = {
#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
	.rt5659_hp_jack_gpio = {
		.name = "headphone detect",
		.invert = 1,
		.report = SND_JACK_HEADSET | SND_JACK_BTN_0 | SND_JACK_BTN_1
			| SND_JACK_BTN_2 | SND_JACK_BTN_3,
		.debounce_time = 0,
		.wake = true,
		.jack_status_check = rt5659_jack_status_check,
	},
#endif
};

static const struct snd_soc_component_driver pacific_cmpnt = {
	.name	= "pacific-audio",
};

static const struct snd_kcontrol_new pacific_codec_controls[] = {
};
#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
static struct switch_dev rt5659_headset_switch = {
	.name = "h2w",
};
/*
static int rt5659_jack_notifier(struct notifier_block *self,
			      unsigned long action, void *dev)
{
	struct snd_soc_jack *jack = dev;
	struct snd_soc_codec *codec = jack->codec;
	struct snd_soc_card *card = codec->card;
	struct rt5659_machine_priv *priv = card->drvdata;

	if (jack == &priv->rt5659_hp_jack)
		switch_set_state(&rt5659_headset_switch,
			rt5659_get_jack_type(codec, action));

	return NOTIFY_OK;
}

static struct notifier_block rt5659_jack_detect_nb = {
	.notifier_call = rt5659_jack_notifier,
};
*/
#endif

#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
int rt5659_jack_status_check(void)
{
	struct snd_soc_codec *codec = pacific_rt5659_priv.codec;
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL_RT5659
	struct rt5659_priv *rt5659 = snd_soc_codec_get_drvdata(codec);
#endif
	int report = 0, ret;

	/* prevent suspend to allow user space to respond to switch */
	if (codec->card->instantiated)
		wake_lock_timeout(&pacific_rt5659_priv.jack_wake_lock, WAKE_LOCK_TIME);

	msleep(pacific_rt5659_priv.det_delay_time);
	pr_info("%s start\n", __func__);

	mutex_lock(&pacific_rt5659_priv.mutex);

	if (rt5659_check_jd_status(codec) && !pacific_rt5659_priv.jd_status) {
		pacific_rt5659_priv.jd_status = true;
		ret = rt5659_get_jack_type(codec, 1);
		switch_set_state(&rt5659_headset_switch, ret);
		if (ret == 1) {
			report |= SND_JACK_HEADSET;
			pacific_rt5659_priv.det_delay_time = 50;
		} else {
			report |= SND_JACK_HEADPHONE;
		}
		printk("%s: Jack inserted - %d\n", __func__, ret);

	} else if (rt5659_check_jd_status(codec) && pacific_rt5659_priv.jd_status) {
		pr_info("%s esle if\n", __func__);
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL_RT5659
		if (!pacific_rt5659_priv.earmic_enabled)
			rt5659_dynamic_control_micbias(MIC_BIAS_V2P70V);
#endif
		switch (rt5659_button_detect(codec)) {
		case 0x8000:
		case 0x4000:
		case 0x2000:
			report |= SND_JACK_BTN_0;
			break;
		case 0x1000:
		case 0x0800:
		case 0x0400:
			report |= SND_JACK_BTN_1;
			break;
		case 0x0200:
		case 0x0100:
		case 0x0080:
			report |= SND_JACK_BTN_2;
			break;
		case 0x0040:
		case 0x0020:
		case 0x0010:
			report |= SND_JACK_BTN_3;
			break;
		default:
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL_RT5659
			if (!pacific_rt5659_priv.earmic_enabled)
				rt5659_dynamic_control_micbias(rt5659->pdata.dynamic_micb_ctrl_voltage);
#endif
			break;
		}
		if (report)
			printk("%s: Jack key pressed - %d\n", __func__, report);
		else
			printk("%s: Jack key realsed\n", __func__);

		report |= SND_JACK_HEADSET;

		if (report & (SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2 | SND_JACK_BTN_3))
			mod_timer(&pacific_rt5659_priv.jd_check_timer, jiffies);
		else
			del_timer(&pacific_rt5659_priv.jd_check_timer);
	} else {
		if (pacific_rt5659_priv.jd_status)
			printk("%s: Jack removed\n", __func__);

		pacific_rt5659_priv.jd_status = false;
		rt5659_get_jack_type(codec, 0);
		switch_set_state(&rt5659_headset_switch, 0);
		pacific_rt5659_priv.det_delay_time = 200;
	}

	mutex_unlock(&pacific_rt5659_priv.mutex);

	return report;
}
#endif

/*
static int rt5659_ext_submicbias(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,  int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct rt5659_machine_priv *priv = card->drvdata;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		gpio_set_value(priv->sub_mic_bias_gpio,  1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		gpio_set_value(priv->sub_mic_bias_gpio,  0);
		break;
	}

	dev_dbg(w->dapm->dev, "Sub Mic BIAS: %d mic_bias_gpio %d %d\n",
		event, priv->sub_mic_bias_gpio, gpio_get_value(priv->sub_mic_bias_gpio));

	return 0;
}
*/
static const struct snd_kcontrol_new pacific_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget rt5659_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
};

const struct snd_soc_dapm_route pacific_rt5659_dapm_routes[] = {
	{ "HP", NULL, "HPOL" },
	{ "HP", NULL, "HPOR" },
	{ "RCV", NULL, "MONOOUT" },
	{ "SPK", NULL, "SPOL" },
	{ "SPK", NULL, "SPOR" },
	{ "IN1P", NULL, "Headset Mic" },
	{ "IN1N", NULL, "Headset Mic" },
	{ "IN3P", NULL, "MICBIAS3" },
	{ "IN3P", NULL, "Sub Mic" },
	{ "IN3N", NULL, "Sub Mic" },
	{ "IN4P", NULL, "MICBIAS2" },
	{ "IN4P", NULL, "Main Mic" },
	{ "IN4N", NULL, "Main Mic" },
};

static int pacific_aif_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	dev_info(card->dev, "%s-%d startup ++, playback=%d, capture=%d\n",
			rtd->dai_link->name, substream->stream,
			codec_dai->playback_active, codec_dai->capture_active);
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL_RT5659
	if (strcmp(rtd->dai_link->name, "NXP")!=0 && substream->stream == SNDRV_PCM_STREAM_CAPTURE && pacific_rt5659_priv.jd_status) {
		pacific_rt5659_priv.earmic_enabled = true;
		rt5659_dynamic_control_micbias(MIC_BIAS_V2P70V);
	}
#endif
	return 0;
}

static void pacific_aif_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	struct snd_soc_dai *codec_dai = rtd->codec_dai;
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL_RT5659
	struct rt5659_priv *rt5659 = snd_soc_codec_get_drvdata(pacific_rt5659_priv.codec);
#endif

	dev_info(card->dev, "%s-%d shutdown++ codec_dai[%s]:playback=%d\n",
			rtd->dai_link->name, substream->stream,
			codec_dai->name, codec_dai->playback_active);
#ifdef CONFIG_DYNAMIC_MICBIAS_CONTROL_RT5659
	if (strcmp(rtd->dai_link->name, "NXP")!=0 && substream->stream == SNDRV_PCM_STREAM_CAPTURE && pacific_rt5659_priv.jd_status) {
		pacific_rt5659_priv.earmic_enabled = false;
		rt5659_dynamic_control_micbias(rt5659->pdata.dynamic_micb_ctrl_voltage);
	}
#endif
	return;
}

static int pacific_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	dev_info(card->dev, "%s-%d %dch, %dHz, %dbytes\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params),
			params_buffer_bytes(params));

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 codec fmt: %d\n", ret);
		return ret;
	}

	/* Set Codec DAI TDM configuration */
	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0x3, 0x3, 2, 32);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 codec tdm fmt: %d\n", ret);
		return ret;
	}

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 cpu fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_CDCL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_OPCL: %d\n", ret);
		return ret;
	}

	if (params_rate(params) == 192000) {
		ret = snd_soc_dai_set_pll(codec_dai, 0, RT5659_PLL1_S_MCLK,
				PACIFIC_MCLK_FREQ, 49152000);
		if (ret < 0) {
			dev_err(card->dev, "codec_dai pll not set\n");
			return ret;
		}

		ret = snd_soc_dai_set_sysclk(codec_dai, RT5659_SCLK_S_PLL1,
				49152000, SND_SOC_CLOCK_IN);

		if (ret < 0) {
			dev_err(card->dev, "codec_dai clock not set\n");
			return ret;
		}
	} else {
		ret = snd_soc_dai_set_pll(codec_dai, 0, RT5659_PLL1_S_MCLK,
				PACIFIC_MCLK_FREQ, 24576000);
		if (ret < 0) {
			dev_err(card->dev, "codec_dai pll not set\n");
			return ret;
		}

		ret = snd_soc_dai_set_sysclk(codec_dai, RT5659_SCLK_S_PLL1,
				24576000, SND_SOC_CLOCK_IN);

		if (ret < 0) {
			dev_err(card->dev, "codec_dai clock not set\n");
			return ret;
		}
	}
	return ret;
}

static int pacific_aif1_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d\n hw_free",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static int pacific_aif1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d prepare\n",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static int pacific_aif1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d trigger\n",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static struct snd_soc_ops pacific_aif1_ops = {
	.startup = pacific_aif_startup,
	.shutdown = pacific_aif_shutdown,
	.hw_params = pacific_aif1_hw_params,
	.hw_free = pacific_aif1_hw_free,
	.prepare = pacific_aif1_prepare,
	.trigger = pacific_aif1_trigger,
};

static int pacific_aif2_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int prate, bclk, ret;

	dev_info(card->dev, "%s-%d %dch, %dHz\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params));

	prate = params_rate(params);
	switch (prate) {
	case 16000:
		bclk = 512000;
		break;
	default:
		dev_warn(card->dev,
			"Unsupported LRCLK %d, falling back to 16000Hz\n",
			(int)params_rate(params));
		bclk = 512000;
	}

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 codec fmt: %d\n", ret);
		return ret;
	}
	
	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5659_PLL1_S_MCLK,
			PACIFIC_MCLK_FREQ, 24576000);
	if (ret < 0) {
		dev_err(card->dev, "codec_dai pll not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5659_SCLK_S_PLL1,
			24576000, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return ret;
	}

	return 0;
}

static struct snd_soc_ops pacific_aif2_ops = {
	.startup = pacific_aif_startup,
	.shutdown = pacific_aif_shutdown,
	.hw_params = pacific_aif2_hw_params,
};

static int pacific_aif3_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	dev_info(card->dev, "%s-%d %dch, %dHz\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params));

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set BT mode: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5659_PLL1_S_MCLK,
			PACIFIC_MCLK_FREQ, 24576000);
	if (ret < 0) {
		dev_err(card->dev, "codec_dai pll not set\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5659_SCLK_S_PLL1,
			24576000, SND_SOC_CLOCK_IN);

	if (ret < 0) {
		dev_err(card->dev, "codec_dai clock not set\n");
		return ret;
	}

	return 0;
}

static struct snd_soc_ops pacific_aif3_ops = {
	.startup = pacific_aif_startup,
	.shutdown = pacific_aif_shutdown,
	.hw_params = pacific_aif3_hw_params,
};


static struct snd_soc_dai_link pacific_rt5659_dai[] = {
	{ /* playback & recording */
		.name = "playback-pri",
		.stream_name = "playback-pri",
		.codec_dai_name = "rt5659-aif1",
		.ops = &pacific_aif1_ops,
	},
	{ /* voice call */
		.name = "baseband",
		.stream_name = "baseband",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "rt5659-aif2",
		.ops = &pacific_aif2_ops,
		.ignore_suspend = 1,
	},
	{ /* bluetooth sco */
		.name = "bluetooth sco",
		.stream_name = "bluetooth sco",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "rt5659-aif3",
		.ops = &pacific_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* deep buffer playback */
		.name = "playback-sec",
		.stream_name = "playback-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "rt5659-aif1",
		.ops = &pacific_aif1_ops,
	},
	{ /* eax0 playback */
		.name = "playback-eax0",
		.stream_name = "playback-eax0",
		.cpu_dai_name = "samsung-eax.0",
		.platform_name = "samsung-eax.0",
		.codec_dai_name = "rt5659-aif1",
		.ops = &pacific_aif1_ops,
	},
	{ /* NXP */
		.name = "NXP",
		.stream_name = "NXP SPEAK",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "rt5659-aif3",
		.ops = &pacific_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* eax1 playback */
		.name = "playback-eax1",
		.stream_name = "playback-eax1",
		.cpu_dai_name = "samsung-eax.1",
		.platform_name = "samsung-eax.1",
		.codec_dai_name = "rt5659-aif1",
		.ops = &pacific_aif1_ops,
	},
	{ /* eax2 playback */
		.name = "playback-eax2",
		.stream_name = "playback-eax2",
		.cpu_dai_name = "samsung-eax.2",
		.platform_name = "samsung-eax.2",
		.codec_dai_name = "rt5659-aif1",
		.ops = &pacific_aif1_ops,
	},
	{ /* eax3playback */
		.name = "playback-eax3",
		.stream_name = "playback-eax3",
		.cpu_dai_name = "samsung-eax.3",
		.platform_name = "samsung-eax.3",
		.codec_dai_name = "rt5659-aif1",
		.ops = &pacific_aif1_ops,
	},
};

static int pacific_of_get_pdata(struct snd_soc_card *card)
{
	struct device_node *pdata_np;
	struct rt5659_machine_priv *priv = card->drvdata;
	int ret;

	pdata_np = of_find_node_by_path("/audio_pdata");
	if (!pdata_np) {
		dev_err(card->dev,
			"Property 'samsung,audio-pdata' missing or invalid\n");
		return -EINVAL;
	}

	priv->rt5659_hp_jack_gpio.gpio = of_get_named_gpio(pdata_np, "hp_det_gpio", 0);

	priv->sub_mic_bias_gpio = of_get_named_gpio(pdata_np, "sub_mic_bias_gpio", 0);
	if (priv->sub_mic_bias_gpio >= 0) {
		ret = gpio_request(priv->sub_mic_bias_gpio, "MICBIAS_EN_AP");
		if (ret)
			dev_err(card->dev, "Failed to request gpio: %d\n", ret);

		gpio_direction_output(priv->sub_mic_bias_gpio, 0);
	}

	return 0;
}
#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
static void jd_check_handler(struct work_struct *work)
{
	struct snd_soc_codec *codec = pacific_rt5659_priv.codec;

	if (!rt5659_check_jd_status(codec)) {
		pacific_rt5659_priv.jd_status = false;
		rt5659_get_jack_type(codec, 0);
		switch_set_state(&rt5659_headset_switch, 0);
		snd_soc_jack_report(&pacific_rt5659_priv.rt5659_hp_jack, 0,
			pacific_rt5659_priv.rt5659_hp_jack_gpio.report);
		del_timer(&pacific_rt5659_priv.jd_check_timer);
	}
}

static void jd_check_callback(unsigned long data)
{
	schedule_work(&pacific_rt5659_priv.jd_check_work);

	if (mod_timer(&pacific_rt5659_priv.jd_check_timer,
		jiffies + msecs_to_jiffies(500)))
		printk(KERN_INFO "Error in mod_timer\n");
}
#endif
static int pacific_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct rt5659_machine_priv *priv = card->drvdata;
#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
	struct snd_soc_jack *jack = &priv->rt5659_hp_jack;
#endif
	int ret;

	priv->codec = codec;
	priv->jd_status = false;

	pacific_of_get_pdata(card);

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	/* close codec device immediately when pcm is closed */
	codec->ignore_pmdown_time = true;

	ret = snd_soc_add_codec_controls(codec, pacific_codec_controls,
					ARRAY_SIZE(pacific_codec_controls));
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

	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");
	snd_soc_dapm_sync(&codec->dapm);
#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
	setup_timer(&priv->jd_check_timer, jd_check_callback, 0);
	INIT_WORK(&priv->jd_check_work, jd_check_handler);
    mutex_init(&priv->mutex);

	if (gpio_is_valid(priv->rt5659_hp_jack_gpio.gpio)) {
		snd_soc_jack_new(codec, "Headphone Jack", SND_JACK_HEADPHONE,
				jack);
#ifdef CONFIG_SWITCH
/*
		snd_soc_jack_notifier_register(jack, &rt5659_jack_detect_nb);
*/
#endif
		snd_soc_jack_add_gpios(jack, 1, &priv->rt5659_hp_jack_gpio);

		snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_MEDIA);
		snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
		snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
		snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);
	}

	priv->det_delay_time = 200;
	wake_lock_init(&priv->jack_wake_lock, WAKE_LOCK_SUSPEND, "jack_det");
#endif
	return 0;
}

static int pacific_start_sysclk(struct snd_soc_card *card)
{
	struct rt5659_machine_priv *priv = card->drvdata;

	if (priv->mclk) {
		clk_enable(priv->mclk);
		dev_info(card->dev, "mclk enabled\n");
	} else{
#ifdef CONFIG_SND_CODEC_CONTROL_MCLK
		es325_audio_set_mclk(true, 0);
#else
		exynos5_audio_set_mclk(true, 0);
#endif
	}
	return 0;
}

static int pacific_stop_sysclk(struct snd_soc_card *card)
{
	struct rt5659_machine_priv *priv = card->drvdata;

	if (priv->mclk) {
		clk_disable(priv->mclk);
		dev_info(card->dev, "mclk disbled\n");
	} else{
#ifdef CONFIG_SND_CODEC_CONTROL_MCLK
		es325_audio_set_mclk(false, 0);
#else
		exynos5_audio_set_mclk(false, 0);
#endif
	}
	return 0;
}

static int pacific_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct rt5659_machine_priv *priv = card->drvdata;

	if (!priv->codec || dapm != &priv->codec->dapm)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (card->dapm.bias_level == SND_SOC_BIAS_PREPARE)
			pacific_stop_sysclk(card);
		break;
	case SND_SOC_BIAS_PREPARE:
		if (card->dapm.bias_level == SND_SOC_BIAS_STANDBY)
			pacific_start_sysclk(card);
		break;
	default:
	break;
	}

	card->dapm.bias_level = level;
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int pacific_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int pacific_suspend_post(struct snd_soc_card *card)
{
	dev_info(card->dev, "%s: \n", __func__);
	return 0;
}

static int pacific_resume_pre(struct snd_soc_card *card)
{
	dev_info(card->dev, "%s: \n", __func__);
	return 0;
}

static struct snd_soc_card pacific_cards = {
	.name = "Pacific ALC5659 Sound",
	.owner = THIS_MODULE,

	.dai_link = pacific_rt5659_dai,
	.num_links = ARRAY_SIZE(pacific_rt5659_dai),

	.controls = pacific_controls,
	.num_controls = ARRAY_SIZE(pacific_controls),
	.dapm_widgets = rt5659_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5659_dapm_widgets),
	.dapm_routes = pacific_rt5659_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(pacific_rt5659_dapm_routes),

	.late_probe = pacific_late_probe,

	.suspend_post = pacific_suspend_post,
	.resume_pre = pacific_resume_pre,

	.set_bias_level = pacific_set_bias_level,
	.set_bias_level_post = pacific_set_bias_level_post,

	.drvdata = &pacific_rt5659_priv,
};

static int pacific_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np, *cpu_np;
	struct snd_soc_card *card;
	struct snd_soc_dai_link *dai_link;
	struct rt5659_machine_priv *priv;

	card = &pacific_cards;
	dai_link = card->dai_link;

	card->dev = &pdev->dev;
	priv = card->drvdata;

	priv->mclk = devm_clk_get(card->dev, "mclk");
	if (IS_ERR(priv->mclk)) {
		dev_dbg(card->dev, "%s :Device tree node not found for mclk", __func__);
		priv->mclk = NULL;
	} else
		clk_prepare(priv->mclk);

	for (n = 0; n < card->num_links; n++) {
		if (!dai_link[n].cpu_name && !dai_link[n].cpu_dai_name) {
			cpu_np = of_parse_phandle(np, "samsung,audio-cpu", n);
			if (cpu_np) {
				dai_link[n].cpu_of_node = cpu_np;
				dai_link[n].platform_of_node = cpu_np;
			} else
				dev_err(&pdev->dev, "Property 'samsung,audio-cpu'"
						": dai_link[%d] missing or invalid\n",n);
		}
		if (!dai_link[n].codec_name) {
			codec_np = of_parse_phandle(np, "samsung,audio-codec", n);
			if (codec_np)
				dai_link[n].codec_of_node = codec_np;
			else
				dev_err(&pdev->dev, "Property 'samsung,audio-codec'"
						": dai_link[%d] missing or invalid\n", n);
		}
	}

#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
	/* Addd h2w swith class support */
	ret = switch_dev_register(&rt5659_headset_switch);
	if (ret < 0) {
		dev_err(&pdev->dev, "switch_dev_register() failed: %d\n",
			ret);
	}
#endif

	ret = snd_soc_register_card(card);

	if (ret)
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
	else
		dev_err(&pdev->dev, "Success register card:%d\n", ret);

	return ret;

}

static int pacific_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct rt5659_machine_priv *priv = card->drvdata;

	snd_soc_unregister_component(card->dev);
	snd_soc_unregister_card(card);

	if (priv->mclk) {
		clk_unprepare(priv->mclk);
		devm_clk_put(card->dev, priv->mclk);
	}

#if !defined(CONFIG_SEC_DEV_JACK) &&!defined(CONFIG_SEC_DEV_JACK_ANT)
	switch_dev_unregister(&rt5659_headset_switch);
#endif
	snd_soc_jack_free_gpios(&priv->rt5659_hp_jack, 1,
				&priv->rt5659_hp_jack_gpio);

	wake_lock_destroy(&priv->jack_wake_lock);

	return 0;
}

static const struct of_device_id pacific_arizona_of_match[] = {
	{ .compatible = "samsung,pacific-rt5659", },
	{ },
};
MODULE_DEVICE_TABLE(of, pacific_arizona_of_match);

static struct platform_driver pacific_audio_driver = {
	.driver	= {
		.name	= "pacific-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(pacific_arizona_of_match),
#ifdef CONFIG_MULTITHREAD_PROBE
		.multithread_probe = 1,
#endif
	},
	.probe	= pacific_audio_probe,
	.remove	= pacific_audio_remove,
};

module_platform_driver(pacific_audio_driver);

MODULE_DESCRIPTION("ALSA SoC PACIFIC RT5659");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pacific-audio");

