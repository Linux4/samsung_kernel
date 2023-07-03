/*
 * Universal7580-COD3022X Audio Machine driver.
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Sayanta Pattanayak <sayanta.p@samsung.com>
 *			<sayantapattanayak@gmail.com>
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
#include <linux/pm_runtime.h>

#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/s2801x.h>
#include <sound/cod3022x.h>

#include <mach/regs-pmu.h>
#ifdef CONFIG_SAMSUNG_JACK
#include <linux/iio/consumer.h>
#include <linux/sec_jack.h>
#endif

#ifdef CONFIG_PM_DEVFREQ
#define CPU0_MIN_FREQ 600000
#endif

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/cod3022x.h"

#define COD3022X_BFS_48KHZ		32
#define COD3022X_RFS_48KHZ		512
#define COD3022X_SAMPLE_RATE_48KHZ	48000

#define COD3022X_BFS_192KHZ		64
#define COD3022X_RFS_192KHZ		128
#define COD3022X_SAMPLE_RATE_192KHZ	192000

#define JD_SELECT_3022_INTERNEL		0
#define JD_SELECT_EXTERNEL		1

#define EXYNOS7580_BT1_MASK		0x2

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

static struct snd_soc_card universal7580_cod3022x_card;

enum {
	MB_NONE,
	MB_INT_BIAS1,
	MB_INT_BIAS2,
	MB_EXT_GPIO,
};

struct universal7580_mic_bias {
	int mode[3];
	int gpio[3];
	atomic_t use_count[3];
};

enum {
	EX_SPK_BUCK,
	EX_SPK_AMP_EN,
	EX_SPK_CONTROL_CNT,
};

struct universal7580_ext_spk_bias {
	int gpio[EX_SPK_CONTROL_CNT];
};

struct cod3022x_machine_priv {
	struct snd_soc_codec *codec;
	struct device *i2s_dev;
	struct snd_soc_dai *cpu_dai;
	int aifrate;
	int gpio_jd_ic_select; /* jack detection ic selection gpio */
	struct clk *bclk_codec;
	bool use_external_jd;
	struct universal7580_mic_bias mic_bias;
	bool use_bt1_for_fm;
	bool use_i2scdclk_for_codec_bclk;
	bool use_ext_spk;
	struct universal7580_ext_spk_bias ext_spk_bias;
#ifdef CONFIG_PM_DEVFREQ
	struct pm_qos_request aud_cpu0_qos;
#endif
};

static const struct snd_soc_component_driver universal7580_cmpnt = {
	.name = "universal7580-audio",
};

/**
 * In some variants of the boards, the MCLK for the codec chip is connected to
 * I2SCDCLK line. This clock needs to configured inside I2S block for the codec
 * to work properly. If the AP playback has not happened before the CP or FM
 * interface usage, this clock remains unconfigured and thus results in
 * mal-fucntion of codec chip. This function is made generic so that it can be
 * called from AP/CP/FM interfaces to enable MCLK.
 */
static int universal7580_configure_cpu_dai(struct snd_soc_card *card,
					struct snd_soc_dai *cpu_dai,
					int rfs, int bfs, int interface)
{
	int ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif%d: Failed to set CPU DAIFMT\n",
							interface);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
				rfs, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(card->dev, "aif%d: Failed to set SAMSUNG_I2S_CDCLK\n",
						interface);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
						0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "aif%d: Failed to set SAMSUNG_I2S_OPCLK\n",
								interface);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0) {
		dev_err(card->dev,
				"aif%d: Failed to set SAMSUNG_I2S_RCLKSRC_1\n",
							interface);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0) {
		dev_err(card->dev, "aif%d: Failed to set BFS\n", interface);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_RCLK, rfs);
	if (ret < 0) {
		dev_err(card->dev, "aif%d: Failed to set RFS\n", interface);
		return ret;
	}

	if (interface != 1) {
		/* Provide sample-rate to I2S block to configure PSR value.
		 * It also updates the BFS/RFS values in I2S h/w.
		 */
		ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_UPDATE,
				COD3022X_SAMPLE_RATE_48KHZ);
		if (ret < 0) {
			dev_err(card->dev, "aif%d: Failed to update dividers\n",
					interface);
			return ret;
		}
	}

	return 0;
}

static int universal7580_aif1_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;
	int rfs, bfs;

	dev_info(card->dev, "aif1: %dch, %dHz, %dbytes\n",
		 params_channels(params), params_rate(params),
		 params_buffer_bytes(params));

	priv->aifrate = params_rate(params);
	if (priv->aifrate == COD3022X_SAMPLE_RATE_192KHZ) {
		rfs = COD3022X_RFS_192KHZ;
		bfs = COD3022X_BFS_192KHZ;
	} else {
		rfs = COD3022X_RFS_48KHZ;
		bfs = COD3022X_BFS_48KHZ;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to set Codec DAIFMT\n");
		return ret;
	}

	ret = universal7580_configure_cpu_dai(card, cpu_dai, rfs, bfs, 1);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to configure cpu dai\n");
		return ret;
	}

	ret = s2801x_hw_params(substream, params, bfs, 1);
	if (ret < 0) {
		dev_err(card->dev, "aif1: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7580_aif2_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = priv->cpu_dai;
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

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
						| SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(card->dev, "aif2: Failed to set Codec DAIFMT\n");
		return ret;
	}

	if (priv->use_i2scdclk_for_codec_bclk) {
		ret = universal7580_configure_cpu_dai(card, cpu_dai,
			COD3022X_RFS_48KHZ, COD3022X_BFS_48KHZ, 2);
		if (ret < 0) {
			dev_err(card->dev, "aif2: Failed to configure cpu dai\n");
			return ret;
		}
	}

	ret = s2801x_hw_params(substream, params, bfs, 2);
	if (ret < 0) {
		dev_err(card->dev, "aif2: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7580_aif3_hw_params(struct snd_pcm_substream *substream,
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

	ret = s2801x_hw_params(substream, params, bfs, 3);
	if (ret < 0) {
		dev_err(card->dev, "aif3: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7580_aif4_hw_params(struct snd_pcm_substream *substream,
						 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = priv->cpu_dai;
	int bfs, ret;

	dev_info(card->dev, "aif4: %dch, %dHz, %dbytes\n",
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
		dev_err(card->dev, "aif4: Unsupported PCM_FORMAT\n");
		return -EINVAL;
	}

	if (priv->use_i2scdclk_for_codec_bclk) {
		/* I2s registers need to be configured , to provide the clk to codec*/
		ret = universal7580_configure_cpu_dai(card, cpu_dai,
				COD3022X_RFS_48KHZ, COD3022X_BFS_48KHZ, 4);
		if (ret < 0) {
			dev_err(card->dev, "aif4: Failed to configure cpu dai\n");
			return ret;
		}
	}

	/*Mixer interfcaes should be set as 3 - since FM uses BT interface */
	ret = s2801x_hw_params(substream, params, bfs, 3);
	if (ret < 0) {
		dev_err(card->dev, "aif4: Failed to configure mixer\n");
		return ret;
	}

	return 0;
}

static int universal7580_aif1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	dev_dbg(card->dev, "aif1: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2801x_startup(S2801X_IF_AP);

#ifdef CONFIG_PM_DEVFREQ
	if (substream->stream)
		pm_qos_update_request(&priv->aud_cpu0_qos, CPU0_MIN_FREQ);
#endif

	return 0;
}

static int universal7580_aif2_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	dev_dbg(card->dev, "aif2: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	if (priv->use_i2scdclk_for_codec_bclk)
		pm_runtime_get_sync(priv->i2s_dev);

	s2801x_startup(S2801X_IF_CP);

	return 0;
}

static int universal7580_aif3_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif3: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2801x_startup(S2801X_IF_BT);

	return 0;
}

static int universal7580_aif4_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	unsigned int val;

	dev_dbg(card->dev, "aif4: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	if (priv->use_i2scdclk_for_codec_bclk)
		pm_runtime_get_sync(priv->i2s_dev);

	s2801x_startup(S2801X_IF_BT);

	if (priv->use_bt1_for_fm) {
		val = readl(EXYNOS_PMU_AUD_PATH_CFG);
		val |= EXYNOS7580_BT1_MASK;
		writel(val, EXYNOS_PMU_AUD_PATH_CFG);
	}
	return 0;
}

void universal7580_aif1_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	dev_dbg(card->dev, "aif1: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2801x_shutdown(S2801X_IF_AP);

#ifdef CONFIG_PM_DEVFREQ
	if (substream->stream)
		pm_qos_update_request(&priv->aud_cpu0_qos, 0);
#endif
}

void universal7580_aif2_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	dev_dbg(card->dev, "aif2: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2801x_shutdown(S2801X_IF_CP);

	if (priv->use_i2scdclk_for_codec_bclk)
		pm_runtime_put_sync(priv->i2s_dev);
}

void universal7580_aif3_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "aif3: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2801x_shutdown(S2801X_IF_BT);
}

void universal7580_aif4_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	unsigned int val;

	dev_dbg(card->dev, "aif4: (%s) %s called\n",
			substream->stream ? "C" : "P", __func__);

	s2801x_shutdown(S2801X_IF_BT);

	if (priv->use_bt1_for_fm) {
		val = readl(EXYNOS_PMU_AUD_PATH_CFG);
		val &= ~EXYNOS7580_BT1_MASK;
		writel(val, EXYNOS_PMU_AUD_PATH_CFG);
	}

	if (priv->use_i2scdclk_for_codec_bclk)
		pm_runtime_put_sync(priv->i2s_dev);
}

static int universal7580_set_bias_level(struct snd_soc_card *card,
				 struct snd_soc_dapm_context *dapm,
				 enum snd_soc_bias_level level)
{

	return 0;
}

static int universal7580_ext_gpio_spk_ev(struct snd_soc_card *card,
				int event)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int i;
	dev_dbg(card->dev, "%s Called: %d\n", __func__,	event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		for (i=0; i < EX_SPK_CONTROL_CNT; i++)
			if (gpio_is_valid(priv->ext_spk_bias.gpio[i]))
				gpio_set_value(priv->ext_spk_bias.gpio[i], 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		for (i=0; i < EX_SPK_CONTROL_CNT; i++)
			if (gpio_is_valid(priv->ext_spk_bias.gpio[i]))
				gpio_set_value(priv->ext_spk_bias.gpio[i], 0);
		break;
	}
	return 0;
}

static void universal7580_ext_gpio_bias_ev(struct snd_soc_card *card,
				int event, int gpio)
{
	dev_dbg(card->dev, "%s Called: %d, ext mic bias gpio %s\n", __func__,
			event, gpio_is_valid(gpio) ?
			"valid" : "invalid");

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (gpio_is_valid(gpio))
			gpio_set_value(gpio, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (gpio_is_valid(gpio))
			gpio_set_value(gpio, 0);
		break;
	}
}

static int universal7580_int_bias1_ev(struct snd_soc_card *card,
				int event)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	dev_dbg(card->dev, "%s called\n", __func__);
	return cod3022x_mic_bias_ev(priv->codec, COD3022X_MIC1, event);
}

static int universal7580_int_bias2_ev(struct snd_soc_card *card,
				int event)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	dev_dbg(card->dev, "%s called\n", __func__);
	return cod3022x_mic_bias_ev(priv->codec, COD3022X_MIC2, event);
}

static int universal7580_configure_mic_bias(struct snd_soc_card *card,
		int index, int event)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int process_event = 0;
	int mode_index = priv->mic_bias.mode[index];

	/* validate the bias mode */
	if ((mode_index < MB_INT_BIAS1) || (mode_index > MB_EXT_GPIO))
		return 0;

	/* decrement the bias mode index to match use count buffer
	 * because use count buffer size is 0-2, and the mode is 1-3
	 * so decrement, and this veriable should be used only for indexing
	 * use_count.
	 */
	mode_index--;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		atomic_inc(&priv->mic_bias.use_count[mode_index]);
		if (atomic_read(&priv->mic_bias.use_count[mode_index]) == 1)
			process_event = 1;
		break;

	case SND_SOC_DAPM_POST_PMD:
		atomic_dec(&priv->mic_bias.use_count[mode_index]);
		if (atomic_read(&priv->mic_bias.use_count[mode_index]) == 0)
			process_event = 1;
		break;

	default:
		break;
	}

	if (!process_event)
		return 0;

	switch(priv->mic_bias.mode[index]) {
	case MB_INT_BIAS1:
		universal7580_int_bias1_ev(card, event);
		break;
	case MB_INT_BIAS2:
		universal7580_int_bias2_ev(card, event);
		break;
	case MB_EXT_GPIO:
		universal7580_ext_gpio_bias_ev(card, event,
				priv->mic_bias.gpio[index]);
		break;
	default:
		break;
	};

	return 0;
}

static void universal7580_ex_spk_parse_dt(struct platform_device *pdev, struct
						cod3022x_machine_priv *priv)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret;
	int i;
	int gpio, gpio_cnt;
	char gpio_name[32];

	for (i = 0; i < EX_SPK_CONTROL_CNT; i++) {
		priv->ext_spk_bias.gpio[i] = -EINVAL;
	}

	for (i = 0, gpio_cnt = 0; i < EX_SPK_CONTROL_CNT; i++) {
		gpio = of_get_named_gpio(np, "ext-spk-gpios",
				gpio_cnt++);
		if (gpio_is_valid(gpio)) {
			priv->ext_spk_bias.gpio[i] = gpio;

			sprintf(gpio_name, "ext_spk_bias-%d", i);

			ret = devm_gpio_request_one(dev, gpio,
					GPIOF_OUT_INIT_LOW, gpio_name);

			if (ret < 0) {
				dev_err(dev, "ext-spk bias GPIO request failed\n");
				continue;
			}

			gpio_direction_output(gpio, 0);
		} else {
			dev_err(&pdev->dev, "Invalid spk-bias gpio[%d]\n", i);
		}
	}
}

static int universal7580_ext_spk_bias(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct cod3022x_machine_priv *priv = card->drvdata;

	if (priv->use_ext_spk)
		return universal7580_ext_gpio_spk_ev(w->dapm->card, event);

	return 0;
}

static int universal7580_mic1_bias(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	return universal7580_configure_mic_bias(w->dapm->card, 0, event);
}

static int universal7580_mic2_bias(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	return universal7580_configure_mic_bias(w->dapm->card, 1, event);
}

static int universal7580_linein_bias(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	return universal7580_configure_mic_bias(w->dapm->card, 2, event);
}

static void universal7580_parse_dt(struct snd_soc_card *card)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	struct device *dev = card->dev;

	if (of_find_property(dev->of_node, "samsung,fm-uses-bt1", NULL))
		priv->use_bt1_for_fm = true;
	else
		priv->use_bt1_for_fm = false;

	if (of_find_property(dev->of_node, "samsung,codec-uses-i2scdclk", NULL))
		priv->use_i2scdclk_for_codec_bclk = true;
	else
		priv->use_i2scdclk_for_codec_bclk = false;

	if (of_find_property(dev->of_node, "samsung,uses-ext-spk", NULL))
		priv->use_ext_spk = true;
	else
		priv->use_ext_spk = false;
}

static int universal7580_request_ext_mic_bias_en_gpio(struct snd_soc_card *card)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	struct device *dev = card->dev;
	int ret;
	int gpio;
	int i;
	char gpio_name[32];

	dev_dbg(dev, "%s called\n", __func__);

	for (i = 0; i < 3; i++) {
		gpio = priv->mic_bias.gpio[i];

		/* This is optional GPIO, don't report if not present */
		if (!gpio_is_valid(gpio))
			continue;

		sprintf(gpio_name, "ext_mic_bias-%d", i);

		ret = devm_gpio_request_one(dev, gpio,
				GPIOF_OUT_INIT_LOW, gpio_name);

		if (ret < 0) {
			dev_err(dev, "Ext-MIC bias GPIO request failed\n");
			continue;
		}

		gpio_direction_output(gpio, 0);
	}

	return 0;
}

static int universal7580_jack_detect_dev_select(struct snd_soc_card *card)
{
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	struct device *dev = card->dev;
	int ret;

	if (of_find_property(dev->of_node, "samsung,use-externel-jd",
					NULL) != NULL) {
		priv->use_external_jd = true;
		cod3022x_set_externel_jd(priv->codec);
#ifdef CONFIG_SAMSUNG_JACK
		sec_jack_register_button_notify_cb(&cod3022x_process_button_ev);
#endif
	} else {
		priv->use_external_jd = false;
	}

	if (of_get_property(dev->of_node, "gpios", NULL) != NULL) {
		priv->gpio_jd_ic_select = of_get_gpio(dev->of_node, 0);
		if (priv->gpio_jd_ic_select < 0) {
			dev_err(dev, "failed to get JD gpio info\n");
			return -ENODEV;
		}

		ret = gpio_request_one(priv->gpio_jd_ic_select,
				GPIOF_OUT_INIT_LOW, "jd_select");
		if (ret < 0) {
			dev_err(dev, "JD select gpio request failed\n");
			return -EINVAL;
		}

		if (priv->use_external_jd)
			gpio_set_value(priv->gpio_jd_ic_select, 1);
		else
			gpio_set_value(priv->gpio_jd_ic_select, 0);
	} else {
		dev_err(dev, "Property 'gpios' not found\n");
			return -EINVAL;
	}

	return 0;
}

static int universal7580_enable_codec_bclk(struct snd_soc_card *card)
{
	struct device *dev = card->dev;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	int ret;

	priv->bclk_codec = clk_get(dev, "codec_bclk");
	if (IS_ERR(priv->bclk_codec)) {
		dev_err(dev, "codec_bclk not found\n");
		return PTR_ERR(priv->bclk_codec);
	}

	ret = clk_prepare_enable(priv->bclk_codec);
	if (ret < 0) {
		dev_err(dev, "clk enable failed for codec bclk\n");
		clk_put(priv->bclk_codec);
		return ret;
	}

	pm_runtime_get_sync(cpu_dai->dev);
	/* setting as interface 2, to ensure PSR is set */
	ret = universal7580_configure_cpu_dai(card, cpu_dai,
				COD3022X_RFS_48KHZ, COD3022X_BFS_48KHZ, 2);
	pm_runtime_put_sync(cpu_dai->dev);

	if (ret) {
		dev_err(dev, "Failed to configure cpu dai for codec clk\n");
		return ret;
	}

	return 0;
}

static int universal7580_init_soundcard(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;

	priv->codec = codec;
	priv->i2s_dev = cpu_dai->dev;
	priv->cpu_dai = cpu_dai;

	/* Parse DT properties */
	universal7580_parse_dt(card);

	/**
	 * On Universal board, Codec COD3022X requires the I2SCDCLK as
	 * bit-clock. This clock is default enabled, but it requires the I2S
	 * block to be active. Hence we need to keep this clock enabled for CP
	 * call mode.
	 */
	ret = universal7580_enable_codec_bclk(card);
	if (ret) {
		dev_err(codec->dev, "Failed to enable bclk for codec\n");
		return ret;
	}

	/**
	 * If jack detector selection fails, default internel jack detector will
	 * be enabled in codec.
	 */
	ret = universal7580_jack_detect_dev_select(card);
	if (ret)
		dev_warn(codec->dev, "Failed to get jd gpios :%d\n", ret);

	ret = universal7580_request_ext_mic_bias_en_gpio(card);
	if (ret)
		dev_warn(codec->dev, "Failed to get ext mic bias gpios :%d\n",
								ret);

	if (!priv->use_external_jd) {
		ret = cod3022x_jack_mic_register(codec);
		if (ret < 0) {
			dev_err(card->dev,
			"Jack mic registration failed with error %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int universal7580_late_probe(struct snd_soc_card *card)
{

	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK Bias");
	return 0;
}

static int s2801x_init(struct snd_soc_dapm_context *dapm)
{
	dev_dbg(dapm->dev, "%s called\n", __func__);

	return 0;
}

static const struct snd_kcontrol_new universal7580_controls[] = {
};

/* DAPM widgets and routes for controlling board-specific microphone bias*/
const struct snd_soc_dapm_widget universal7580_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("MIC1 Bias", universal7580_mic1_bias),
	SND_SOC_DAPM_MIC("MIC2 Bias", universal7580_mic2_bias),
	SND_SOC_DAPM_MIC("LINEIN Bias", universal7580_linein_bias),
	SND_SOC_DAPM_SPK("SPK Bias", universal7580_ext_spk_bias),
};

const struct snd_soc_dapm_route universal7580_dapm_routes[] = {
	{"MIC1_PGA", NULL, "MIC1 Bias"},
	{"MIC1 Bias", NULL, "IN1L"},

	{"MIC2_PGA", NULL, "MIC2 Bias"},
	{"MIC2 Bias", NULL, "IN2L"},

	{"LINEIN_PGA", NULL, "LINEIN Bias"},
	{"LINEIN Bias", NULL, "IN3L" },

	{"SPK Bias", NULL, "SPKOUTLN" },
};

static struct snd_soc_ops universal7580_aif1_ops = {
	.hw_params = universal7580_aif1_hw_params,
	.startup = universal7580_aif1_startup,
	.shutdown = universal7580_aif1_shutdown,
};

static struct snd_soc_ops universal7580_aif2_ops = {
	.hw_params = universal7580_aif2_hw_params,
	.startup = universal7580_aif2_startup,
	.shutdown = universal7580_aif2_shutdown,
};

static struct snd_soc_ops universal7580_aif3_ops = {
	.hw_params = universal7580_aif3_hw_params,
	.startup = universal7580_aif3_startup,
	.shutdown = universal7580_aif3_shutdown,
};

static struct snd_soc_ops universal7580_aif4_ops = {
	.hw_params = universal7580_aif4_hw_params,
	.startup = universal7580_aif4_startup,
	.shutdown = universal7580_aif4_shutdown,
};

static struct snd_soc_dai_driver universal7580_ext_dai[] = {
	{
		.name = "universal7580 voice call",
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
		.name = "universal7580 BT",
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
		.name = "universal7580 FM",
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

static struct snd_soc_dai_link universal7580_cod3022x_dai[] = {
	/* Playback and Recording */
	{
		.name = "universal7580-cod3022x",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "cod3022x-aif",
		.ops = &universal7580_aif1_ops,
	},
	/* Deep buffer playback */
	{
		.name = "universal7580-cod3022x-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.stream_name = "i2s0-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "cod3022x-aif",
		.ops = &universal7580_aif1_ops,
	},
	/* Voice Call */
	{
		.name = "cp",
		.stream_name = "voice call",
		.cpu_dai_name = "universal7580 voice call",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "cod3022x-aif2",
		.ops = &universal7580_aif2_ops,
		.ignore_suspend = 1,
	},
	/* BT */
	{
		.name = "bt",
		.stream_name = "bluetooth audio",
		.cpu_dai_name = "universal7580 BT",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "dummy-aif2",
		.ops = &universal7580_aif3_ops,
		.ignore_suspend = 1,
	},
	/* FM Playback */
	{
		.name = "fm",
		.stream_name = "FM audio",
		.cpu_dai_name = "universal7580 FM",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "cod3022x-aif",
		.ops = &universal7580_aif4_ops,
		.ignore_suspend = 1,
	},
	/* EAX0 Playback */
	{
		.name = "playback-eax0",
		.stream_name = "eax0",
		.cpu_dai_name = "samsung-eax.0",
		.platform_name = "samsung-eax.0",
		.codec_dai_name = "cod3022x-aif",
		.ops = &universal7580_aif1_ops,
		.ignore_suspend = 1,
	},
	/* EAX1 Playback */
	{
		.name = "playback-eax1",
		.stream_name = "eax1",
		.cpu_dai_name = "samsung-eax.1",
		.platform_name = "samsung-eax.1",
		.codec_dai_name = "cod3022x-aif",
		.ops = &universal7580_aif1_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_aux_dev s2801x_aux_dev[] = {
	{
		.init = s2801x_init,
	},
};

static struct snd_soc_codec_conf s2801x_codec_conf[] = {
	{
		.name_prefix = "S2801",
	},
};

static struct snd_soc_card universal7580_cod3022x_card = {
	.name = "Universal7580-I2S",
	.owner = THIS_MODULE,

	.dai_link = universal7580_cod3022x_dai,
	.num_links = ARRAY_SIZE(universal7580_cod3022x_dai),

	.controls = universal7580_controls,
	.num_controls = ARRAY_SIZE(universal7580_controls),
	.dapm_widgets = universal7580_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal7580_dapm_widgets),
	.dapm_routes = universal7580_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal7580_dapm_routes),

	.late_probe = universal7580_late_probe,
	.set_bias_level = universal7580_set_bias_level,
	.aux_dev = s2801x_aux_dev,
	.num_aux_devs = ARRAY_SIZE(s2801x_aux_dev),
	.codec_conf = s2801x_codec_conf,
	.num_configs = ARRAY_SIZE(s2801x_codec_conf),
};

static void universal7580_mic_bias_parse_dt(struct platform_device *pdev, struct
						cod3022x_machine_priv *priv)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;
	int i;
	int gpio, gpio_cnt;

	for (i = 0; i < 3; i++) {
		priv->mic_bias.mode[i] = MB_NONE;
		priv->mic_bias.gpio[i] = -EINVAL;
		atomic_set(&priv->mic_bias.use_count[i], 0);
	}

	ret = of_property_read_u32_array(np, "mic-bias-mode",
			priv->mic_bias.mode, 3);
	if (ret) {
		dev_err(&pdev->dev, "Could not read `mic-bias-mode`\n");
		return;
	}

	for (i = 0, gpio_cnt = 0; i < 3; i++) {
		if (priv->mic_bias.mode[i] == MB_EXT_GPIO) {
			gpio = of_get_named_gpio(np, "mic-bias-gpios",
					gpio_cnt++);
			if (gpio_is_valid(gpio))
				priv->mic_bias.gpio[i] = gpio;
			else
				dev_err(&pdev->dev, "Invalid mic-bias gpio\n");
		}
	}

	dev_dbg(&pdev->dev, "BIAS: Main-Mic(%d), Sub-Mic(%d), Ear-Mic(%d)\n",
			priv->mic_bias.mode[0], priv->mic_bias.mode[1],
			priv->mic_bias.mode[2]);
}

static int universal7580_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpu_np, *codec_np, *auxdev_np;
	struct snd_soc_card *card = &universal7580_cod3022x_card;
	struct cod3022x_machine_priv *priv;

	if (!np) {
		dev_err(&pdev->dev, "Failed to get device node\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	card->dev = &pdev->dev;
	card->num_links = 0;

	ret = snd_soc_register_component(card->dev, &universal7580_cmpnt,
					universal7580_ext_dai,
					ARRAY_SIZE(universal7580_ext_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(universal7580_cod3022x_dai); n++) {
		/* Skip parsing DT for fully formed dai links */
		if (universal7580_cod3022x_dai[n].platform_name &&
				universal7580_cod3022x_dai[n].codec_name) {
			dev_dbg(card->dev,
			"Skipping dt for populated dai link %s\n",
			universal7580_cod3022x_dai[n].name);
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

		universal7580_cod3022x_dai[n].codec_of_node = codec_np;
		if (!universal7580_cod3022x_dai[n].cpu_dai_name)
			universal7580_cod3022x_dai[n].cpu_of_node = cpu_np;

		if (!universal7580_cod3022x_dai[n].platform_name)
			universal7580_cod3022x_dai[n].platform_of_node = cpu_np;

		card->num_links++;
	}

	for (n = 0; n < ARRAY_SIZE(s2801x_aux_dev); n++) {
		auxdev_np = of_parse_phandle(np, "samsung,auxdev", n);
		if (!auxdev_np) {
			dev_err(&pdev->dev,
				"Property 'samsung,auxdev' missing\n");
			return -EINVAL;
		}
		s2801x_aux_dev[n].of_node = auxdev_np;
		s2801x_codec_conf[n].of_node = auxdev_np;
	}

	snd_soc_card_set_drvdata(card, priv);

	universal7580_mic_bias_parse_dt(pdev, priv);

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);


	universal7580_init_soundcard(card);

	if (priv->use_ext_spk)
		universal7580_ex_spk_parse_dt(pdev, priv);
#ifdef CONFIG_SAMSUNG_JACK
	jack_adc = iio_channel_get_all(&pdev->dev);
	jack_controls.get_adc = get_adc_value;
	jack_controls.snd_card_registered = 1;
#endif
#ifdef CONFIG_PM_DEVFREQ
	pm_qos_add_request(&priv->aud_cpu0_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
#endif
	return ret;
}

static int universal7580_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct cod3022x_machine_priv *priv = snd_soc_card_get_drvdata(card);

	if (!IS_ERR(priv->bclk_codec)) {
		clk_disable_unprepare(priv->bclk_codec);
		clk_put(priv->bclk_codec);
	}

#ifdef CONFIG_SAMSUNG_JACK
	iio_channel_release(jack_adc);
#endif
	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id universal7580_cod3022x_of_match[] = {
	{.compatible = "samsung,universal7580-cod3022x",},
	{},
};
MODULE_DEVICE_TABLE(of, universal7580_cod3022x_of_match);

static struct platform_driver universal7580_audio_driver = {
	.driver = {
		.name = "Universal7580-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(universal7580_cod3022x_of_match),
	},
	.probe = universal7580_audio_probe,
	.remove = universal7580_audio_remove,
};

module_platform_driver(universal7580_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Universal7580 COD3022X");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:universal7580-audio");
