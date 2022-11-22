/*
 *  universal222ap_wm1811.c
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

#include <plat/adc.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "s3c-i2s-v2.h"
#include "../codecs/wm8994.h"
#include "../../../arch/arm/mach-exynos/board-universal222ap.h"


#define UNIVERSAL222AP_DEFAULT_MCLK1	24000000
#define UNIVERSAL222AP_DEFAULT_MCLK2	32768
#define UNIVERSAL222AP_DEFAULT_SYNC_CLK	12288000

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

#define JACK_ADC_CH		0
#define JACK_SAMPLE_SIZE	5

#define MAX_ZONE_LIMIT		10
/* keep this value if you support double-pressed concept */
#define WAKE_LOCK_TIME		(HZ * 5)	/* 5 sec */
#define EAR_CHECK_LOOP_CNT	10

static struct wm8958_micd_rate universal222ap_det_rates[] = {
	{ UNIVERSAL222AP_DEFAULT_MCLK2,     true,  0,  0 },
	{ UNIVERSAL222AP_DEFAULT_MCLK2,    false,  0,  0 },
	{ UNIVERSAL222AP_DEFAULT_SYNC_CLK,  true,  7,  7 },
	{ UNIVERSAL222AP_DEFAULT_SYNC_CLK, false,  7,  7 },
};

static struct wm8958_micd_rate universal222ap_jackdet_rates[] = {
	{ UNIVERSAL222AP_DEFAULT_MCLK2,     true,  0,  0 },
	{ UNIVERSAL222AP_DEFAULT_MCLK2,    false,  0,  0 },
	{ UNIVERSAL222AP_DEFAULT_SYNC_CLK,  true, 12, 12 },
	{ UNIVERSAL222AP_DEFAULT_SYNC_CLK, false,  7,  8 },
};

struct wm1811_machine_priv {
	unsigned int pll_out;
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct delayed_work mic_work;
	struct delayed_work headset_stmr_work;
	struct wake_lock jackdet_wake_lock;
	void (*set_mclk) (bool enable, bool forced);
	void (*lineout_switch_f) (int on);
	void (*set_main_mic_f) (int on);
	void (*set_sub_mic_f) (int on);
	int (*get_g_det_value_f) (void);
	int (*get_g_det_irq_num_f) (void);
	struct s3c_adc_client *padc;
	struct jack_zone *zones;
	int	num_zones;
	int	use_jackdet_type;
};

enum {
	SEC_JACK_NO_DEVICE		= 0x0,
	SEC_HEADSET_4POLE		= 0x01 << 0,
	SEC_HEADSET_3POLE		= 0x01 << 1,
	SEC_TTY_DEVICE			= 0x01 << 2,
	SEC_FM_HEADSET			= 0x01 << 3,
	SEC_FM_SPEAKER			= 0x01 << 4,
	SEC_TVOUT_DEVICE		= 0x01 << 5,
	SEC_EXTRA_DOCK_SPEAKER		= 0x01 << 6,
	SEC_EXTRA_CAR_DOCK_SPEAKER	= 0x01 << 7,
	SEC_UNKNOWN_DEVICE		= 0x01 << 8,
};

static bool recheck_jack;
static int jack_get_adc_data(struct s3c_adc_client *padc);
static void jack_set_type(struct wm1811_machine_priv *wm1811, int jack_type);

static int aif2_mode;
const char *aif2_mode_text[] = {
	"Slave", "Master"
};

const char *switch_mode_text[] = {
	"Off", "On"
};

const char *mic_bias_mode_text[] = {
	"Disable", "Force Disable", "Enable", "Force Enable"
};

static int input_clamp;
static int aif2_digital_mute;
static int sub_mic_bias_mode;
static int main_mic_bias_mode;
static int headset_stmr_mode;
static int lineout_mode;

#ifndef CONFIG_SEC_DEV_JACK
/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;
#endif

static const struct soc_enum switch_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(switch_mode_text), switch_mode_text),
};

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

static int get_aif2_mute_status(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aif2_digital_mute;
	return 0;
}

static int set_aif2_mute_status(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int reg;

	aif2_digital_mute = ucontrol->value.integer.value[0];

	if (snd_soc_read(codec, WM8994_POWER_MANAGEMENT_6)
		& WM8994_AIF2_DACDAT_SRC)
		aif2_digital_mute = 0;

	if (aif2_digital_mute)
		reg = WM8994_AIF1DAC1_MUTE;
	else
		reg = 0;

	snd_soc_update_bits(codec, WM8994_AIF2_DAC_FILTERS_1,
				WM8994_AIF1DAC1_MUTE, reg);

	pr_info("set aif2_digital_mute : %s\n",
			switch_mode_text[aif2_digital_mute]);

	if (aif2_digital_mute)
		msleep(50);

	return 0;
}

static int get_input_clamp(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = input_clamp;
	return 0;
}

static int set_input_clamp(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	input_clamp = ucontrol->value.integer.value[0];

	if (input_clamp) {
		snd_soc_update_bits(codec, WM8994_INPUT_MIXER_1,
				WM8994_INPUTS_CLAMP, WM8994_INPUTS_CLAMP);
		msleep(50);
	} else {
		snd_soc_update_bits(codec, WM8994_INPUT_MIXER_1,
				WM8994_INPUTS_CLAMP, 0);
	}
	pr_info("set input_clamp : %s\n", switch_mode_text[input_clamp]);

	return 0;
}

static void universal222ap_micd_set_rate(struct snd_soc_codec *codec)
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
		rates = universal222ap_jackdet_rates;
		num_rates = ARRAY_SIZE(universal222ap_jackdet_rates);
		wm8994->pdata->micd_rates = universal222ap_jackdet_rates;
		wm8994->pdata->num_micd_rates = num_rates;
	} else {
		rates = universal222ap_det_rates;
		num_rates = ARRAY_SIZE(universal222ap_det_rates);
		wm8994->pdata->micd_rates = universal222ap_det_rates;
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

static int jack_get_adc_data(struct s3c_adc_client *padc)
{
	int adc_data;
	int adc_max = 0;
	int adc_min = 0xFFFF;
	int adc_total = 0;
	int adc_retry_cnt = 0;
	int i;

	for (i = 0; i < JACK_SAMPLE_SIZE; i++) {

		adc_data = s3c_adc_read(padc, JACK_ADC_CH);

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

	return (adc_total - adc_max - adc_min) / (JACK_SAMPLE_SIZE - 2);
}

static void determine_jack_type(struct wm1811_machine_priv *wm1811)
{
	struct jack_zone *zones = wm1811->zones;
	struct snd_soc_codec *codec = wm1811->codec;
	int size = wm1811->num_zones;
	int count[MAX_ZONE_LIMIT] = {0};
	int adc;
	int i;

	/* set mic bias to enable adc */
	while (snd_soc_read(codec, WM1811_JACKDET_CTRL) & WM1811_JACKDET_LVL) {
		adc = jack_get_adc_data(wm1811->padc);

		pr_info("%s: adc = %d\n", __func__, adc);

		if (adc < 0)
			break;

		/* determine the type of headset based on the
		 * adc value.  An adc value can fall in various
		 * ranges or zones.  Within some ranges, the type
		 * can be returned immediately.  Within others, the
		 * value is considered unstable and we need to sample
		 * a few more types (up to the limit determined by
		 * the range) before we return the type for that range.
		 */
		for (i = 0; i < size; i++) {
			if (adc <= zones[i].adc_high) {
				if (++count[i] > zones[i].check_count) {
					if (recheck_jack == true && i == 4) {
						pr_info("%s : something wrong connection!\n",
								__func__);

						recheck_jack = false;
						return;
					}
					jack_set_type(wm1811,
						zones[i].jack_type);
					return;
				}
				msleep(zones[i].delay_ms);
				break;
			}
		}
	}

	recheck_jack = false;
	/* jack removed before detection complete */
	pr_debug("%s : jack removed before detection complete\n", __func__);
}

static void jack_set_type(struct wm1811_machine_priv *wm1811, int jack_type)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(wm1811->codec);

	if (jack_type == SEC_HEADSET_4POLE) {
		dev_info(wm1811->codec->dev, "Detected microphone\n");

		wm8994->mic_detecting = false;
		wm8994->jack_mic = true;

		universal222ap_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADSET,
				    SND_JACK_HEADSET);

		snd_soc_update_bits(wm1811->codec, WM8958_MIC_DETECT_1,
					    WM8958_MICD_ENA, 1);
	} else {
		dev_info(wm1811->codec->dev, "Detected headphone\n");
		wm8994->mic_detecting = false;

		universal222ap_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADPHONE,
				    SND_JACK_HEADSET);

		/* If we have jackdet that will detect removal */
		if (wm8994->jackdet) {
			snd_soc_update_bits(wm1811->codec, WM8958_MIC_DETECT_1,
					    WM8958_MICD_ENA, 0);

			if (wm8994->active_refcount) {
				snd_soc_update_bits(wm1811->codec,
					WM8994_ANTIPOP_2,
					WM1811_JACKDET_MODE_MASK,
					WM1811_JACKDET_MODE_AUDIO);
			}

			if (wm8994->pdata->jd_ext_cap) {
				mutex_lock(&wm1811->codec->mutex);
				snd_soc_dapm_disable_pin(&wm1811->codec->dapm,
							 "MICBIAS2");
				snd_soc_dapm_sync(&wm1811->codec->dapm);
				mutex_unlock(&wm1811->codec->mutex);
			}
		}
	}
}

static void universal222ap_micdet(void *data)
{
	struct wm1811_machine_priv *wm1811 = data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(wm1811->codec);

	struct snd_soc_codec *codec = wm1811->codec;

	pr_info("%s: detected jack\n", __func__);
	wm8994->mic_detecting = true;

	wake_lock_timeout(&wm1811->jackdet_wake_lock, 5 * HZ);

	snd_soc_update_bits(codec, WM8958_MICBIAS2,
				WM8958_MICB2_MODE, 0);

	/* Apply delay time(150ms) to remove pop noise
	 * during to enable micbias */
	msleep(150);

	determine_jack_type(wm1811);
}

static void universal222ap_mic_id(void *data, u16 status)
{
	struct wm1811_machine_priv *wm1811 = data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(wm1811->codec);

	wake_lock_timeout(&wm1811->jackdet_wake_lock, 5 * HZ);

	/* Either nothing present or just starting detection */
	if (!(status & WM8958_MICD_STS)) {
		if (!wm8994->jackdet) {
			/* If nothing present then clear our statuses */
			dev_dbg(wm1811->codec->dev, "Detected open circuit\n");
			wm8994->jack_mic = false;
			wm8994->mic_detecting = true;

			universal222ap_micd_set_rate(wm1811->codec);

			snd_soc_jack_report(wm8994->micdet[0].jack, 0,
					    wm8994->btn_mask |
					     SND_JACK_HEADSET);
		}
	}

	/* If the measurement is showing a high impedence we've got a
	 * microphone.
	 */
	if (wm8994->mic_detecting && (status & 0x400)) {
		dev_info(wm1811->codec->dev, "Detected microphone\n");

		wm8994->mic_detecting = false;
		wm8994->jack_mic = true;

		universal222ap_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADSET,
				    SND_JACK_HEADSET);
	}

	if (wm8994->mic_detecting && status & 0x4) {
		dev_info(wm1811->codec->dev, "Detected headphone\n");
		wm8994->mic_detecting = false;

		universal222ap_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADPHONE,
				    SND_JACK_HEADSET);

		/* If we have jackdet that will detect removal */
		if (wm8994->jackdet) {
			mutex_lock(&wm8994->accdet_lock);

			snd_soc_update_bits(wm1811->codec, WM8958_MIC_DETECT_1,
					    WM8958_MICD_ENA, 0);

			if (wm8994->active_refcount) {
				snd_soc_update_bits(wm1811->codec,
					WM8994_ANTIPOP_2,
					WM1811_JACKDET_MODE_MASK,
					WM1811_JACKDET_MODE_AUDIO);
			}

			mutex_unlock(&wm8994->accdet_lock);

			if (wm8994->pdata->jd_ext_cap) {
				mutex_lock(&wm1811->codec->mutex);
				snd_soc_dapm_disable_pin(&wm1811->codec->dapm,
							 "MICBIAS2");
				snd_soc_dapm_sync(&wm1811->codec->dapm);
				mutex_unlock(&wm1811->codec->mutex);
			}
		}
	}
}

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
static int universal222ap_wm1811_aif1_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct wm1811_machine_priv *wm1811
			= snd_soc_card_get_drvdata(codec->card);
	unsigned int pll_out;
	int bfs, rfs, ret;

	bfs = (params_format(params) == SNDRV_PCM_FORMAT_S24_LE) ? 48 : 32;

	if (params_rate(params) == 8000 || params_rate(params) == 11025)
		rfs = 512;
	else
		rfs = 256;

	pll_out = params_rate(params) * rfs;

	if (clk_get_rate(wm1811->pll) != (pll_out * 4))
		clk_set_rate(wm1811->pll, pll_out * 4);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_NB_NF
			| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
			pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to switch to FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1,
			WM8994_FLL_SRC_BCLK,
			params_rate(params) * bfs,
			params_rate(params) * rfs);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Unable to start FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
			0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
			0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
			rfs, SND_SOC_CLOCK_OUT);

	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}
#else

static int universal222ap_wm1811_aif1_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret;

	dev_info(codec_dai->dev, "%s ++\n", __func__);
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
				  UNIVERSAL222AP_DEFAULT_MCLK1,
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

	dev_info(codec_dai->dev, "%s --\n", __func__);

	return 0;
}
#endif

static int universal222ap_wm1811_aif2_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;
	int prate;
	int bclk;
	unsigned int read_val;

	dev_info(codec_dai->dev, "aif2: %dch, %dHz, %dbytes\n",
				params_channels(params),
				params_rate(params),
				params_buffer_bytes(params));

	prate = params_rate(params);
	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 48000:
	       break;
	default:
		dev_warn(codec_dai->dev, "Unsupported LRCLK %d, falling back to 8000Hz\n",
				(int)params_rate(params));
		prate = 8000;
	}

	/* Set the codec DAI configuration */
	if (aif2_mode == 0)
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A
				| SND_SOC_DAIFMT_IB_NF
				| SND_SOC_DAIFMT_CBS_CFS);
	else {
		if (prate == 48000)
			ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
		else
			ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A
					| SND_SOC_DAIFMT_IB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	}


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
					  UNIVERSAL222AP_DEFAULT_MCLK1,
					  prate * 256);
	}

	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to configure FLL2: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2,
				     prate * 256, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to switch to FLL2: %d\n", ret);

	read_val = snd_soc_read(codec, WM8994_INTERRUPT_RAW_STATUS_2);
	dev_info(codec_dai->dev, "interrupt raw status val:0x%x\n", read_val);

	return 0;
}

static int universal222ap_wm1811_aif3_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int prate;

	dev_info(codec_dai->dev, "%s ++\n", __func__);

	prate = params_rate(params);
	switch (params_rate(params)) {
	case 8000:
	case 16000:
	       break;
	default:
		prate = 8000;
	}

	if (aif2_mode != 0) {
		pr_info("%s: prate is %d\n", __func__, prate);

		snd_soc_update_bits(codec_dai->codec, WM8994_AIF2_BCLK,
				WM8994_AIF1_BCLK_DIV_MASK,
				((prate == 8000) ? 0x00 : 0x02)
				<< WM8994_AIF1_BCLK_DIV_SHIFT);
	}
	dev_info(codec_dai->dev, "%s --\n", __func__);
	return 0;
}

static int get_main_mic_bias_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = main_mic_bias_mode;
	return 0;
}

static int set_main_mic_bias_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);
	int status = 0;

	status = ucontrol->value.integer.value[0];

	switch (status) {
	case MIC_FORCE_ENABLE:
		main_mic_bias_mode = status;
		if (wm1811->set_main_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, WM8994_MICB1_ENA);
		else
			wm1811->set_main_mic_f(1);
		break;
	case MIC_ENABLE:
		if (main_mic_bias_mode != MIC_FORCE_ENABLE)
			main_mic_bias_mode = status;
		if (wm1811->set_main_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, WM8994_MICB1_ENA);
		else
			wm1811->set_main_mic_f(1);
		if (main_mic_bias_mode != MIC_FORCE_ENABLE)
			msleep(100);
		break;
	case MIC_FORCE_DISABLE:
		main_mic_bias_mode = status;
		if (wm1811->set_main_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, 0);
		else
			wm1811->set_main_mic_f(0);
		break;
	case MIC_DISABLE:
		if (main_mic_bias_mode != MIC_FORCE_ENABLE) {
		if (wm1811->set_main_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, 0);
		else
				wm1811->set_main_mic_f(0);
		} else
			dev_info(codec->dev,
				"SKIP mainmic disable=%d\n", status);
		break;
	default:
		break;
	}

	dev_info(codec->dev, "main_mic_bias_mod=%d: status=%d\n",
				main_mic_bias_mode, status);

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
		if (wm1811->set_sub_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, WM8994_MICB1_ENA);
		else
			wm1811->set_sub_mic_f(1);
		break;
	case MIC_ENABLE:
		if (sub_mic_bias_mode != MIC_FORCE_ENABLE)
			sub_mic_bias_mode = status;
		if (wm1811->set_sub_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, WM8994_MICB1_ENA);
		else
			wm1811->set_sub_mic_f(1);
		if (sub_mic_bias_mode != MIC_FORCE_ENABLE)
			msleep(30);
		break;
	case MIC_FORCE_DISABLE:
		sub_mic_bias_mode = status;
		if (wm1811->set_sub_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, 0);
		else
			wm1811->set_sub_mic_f(0);
		break;
	case MIC_DISABLE:
		if (sub_mic_bias_mode != MIC_FORCE_ENABLE) {
		if (wm1811->set_sub_mic_f == NULL)
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
							WM8994_MICB1_ENA, 0);
		else
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

static int get_headset_stmr_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = headset_stmr_mode;
	return 0;
}

static int set_headset_stmr_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);
	int status = ucontrol->value.integer.value[0];

	if (status) {
		/* 1000ms delay to remove STMR Pop noise
		 * After all paths are set, after enough time is past,
		 * we should set DAC1L/R Left Sidetone switch*/
		schedule_delayed_work(&wm1811->headset_stmr_work,
				      msecs_to_jiffies(1000));
	} else {
		cancel_delayed_work_sync(&wm1811->headset_stmr_work);
		snd_soc_update_bits(codec, WM8994_DAC1_LEFT_MIXER_ROUTING,
						WM8994_ADCL_TO_DAC1L, 0);
		snd_soc_update_bits(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING,
						WM8994_ADCL_TO_DAC1R, 0);
	}

	return 0;
}

static int get_lineout_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = lineout_mode;
	return 0;
}

static int set_lineout_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);
	lineout_mode = ucontrol->value.integer.value[0];

	if (lineout_mode) {
		wm8994_vmid_mode(codec, WM8994_VMID_FORCE);
		if (wm1811->lineout_switch_f)
			wm1811->lineout_switch_f(1);
	} else {
		if (wm1811->lineout_switch_f)
			wm1811->lineout_switch_f(0);
		wm8994_vmid_mode(codec, WM8994_VMID_NORMAL);
	}

	dev_info(codec->dev, "set lineout mode : %s\n",
			switch_mode_text[lineout_mode]);

	return 0;
}

/*
 * universal222ap WM1811 DAI operations.
 */
static struct snd_soc_ops universal222ap_wm1811_aif1_ops = {
	.hw_params = universal222ap_wm1811_aif1_hw_params,
};

static struct snd_soc_ops universal222ap_wm1811_aif2_ops = {
	.hw_params = universal222ap_wm1811_aif2_hw_params,
};

static struct snd_soc_ops universal222ap_wm1811_aif3_ops = {
	.hw_params = universal222ap_wm1811_aif3_hw_params,
};

static const struct snd_kcontrol_new universal222ap_codec_controls[] = {
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),

	SOC_ENUM_EXT("SubMicBias Mode", mic_bias_mode_enum[0],
		get_sub_mic_bias_mode, set_sub_mic_bias_mode),

	SOC_ENUM_EXT("MainMicBias Mode", mic_bias_mode_enum[0],
		get_main_mic_bias_mode, set_main_mic_bias_mode),

	SOC_ENUM_EXT("AIF2 digital mute", switch_mode_enum[0],
		get_aif2_mute_status, set_aif2_mute_status),

	SOC_ENUM_EXT("Input Clamp", switch_mode_enum[0],
		get_input_clamp, set_input_clamp),

	SOC_ENUM_EXT("Headset STMR Mode", switch_mode_enum[0],
		get_headset_stmr_mode, set_headset_stmr_mode),

	SOC_ENUM_EXT("LineoutSwitch Mode", switch_mode_enum[0],
		get_lineout_mode, set_lineout_mode),
};

static const struct snd_kcontrol_new universal222ap_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("FM In"),
	SOC_DAPM_PIN_SWITCH("LINE"),
	SOC_DAPM_PIN_SWITCH("HDMI"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget universal222ap_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("LINE", NULL),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_LINE("FM In", NULL),
};

const struct snd_soc_dapm_route universal222ap_dapm_routes[] = {
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

	{ "IN1RP", NULL, "MICBIAS1" },
	{ "MICBIAS1", NULL, "Sub Mic" },

	{ "IN1LP", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },

	{ "IN2RN", NULL, "FM In" },
	{ "IN2RP:VXRP", NULL, "FM In" },
};

static struct snd_soc_dai_driver universal222ap_ext_dai[] = {
	{
		.name = "universal222ap.cp",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000
					| SNDRV_PCM_RATE_16000
					| SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000
					| SNDRV_PCM_RATE_16000
					| SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "universal222ap.bt",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000
					| SNDRV_PCM_RATE_16000
					| SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = SNDRV_PCM_RATE_8000
					| SNDRV_PCM_RATE_16000
					| SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link universal222ap_dai[] = {
	{ /* Primary DAI i/f */
		.name = "WM8994 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.ops = &universal222ap_wm1811_aif1_ops,
	},
	{
		.name = "WM8994 Voice",
		.stream_name = "Voice Tx/Rx",
		.cpu_dai_name = "universal222ap.cp",
		.codec_dai_name = "wm8994-aif2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm8994-codec",
		.ops = &universal222ap_wm1811_aif2_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "WM8994 BT",
		.stream_name = "BT Tx/Rx",
		.cpu_dai_name = "universal222ap.bt",
		.codec_dai_name = "wm8994-aif3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm8994-codec",
		.ops = &universal222ap_wm1811_aif3_ops,
		.ignore_suspend = 1,
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
		.ops = &universal222ap_wm1811_aif1_ops,
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

	return sprintf(buf, "%d\n", report);
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

	return sprintf(buf, "%d\n", report);
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

	universal222ap_micd_set_rate(codec);

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

static void universal222ap_headset_stmr(struct work_struct *work)
{
	struct wm1811_machine_priv *wm1811 = container_of(work,
						struct wm1811_machine_priv,
						headset_stmr_work.work);
	struct snd_soc_codec *codec = wm1811->codec;

	snd_soc_update_bits(codec, WM8994_DAC1_LEFT_MIXER_ROUTING,
					WM8994_ADCL_TO_DAC1L, 0x10);
	snd_soc_update_bits(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING,
					WM8994_ADCL_TO_DAC1R, 0x10);
}

static int universal222ap_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct wm1811_machine_priv *wm1811 =
				snd_soc_card_get_drvdata(codec->card);
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	const struct exynos_sound_platform_data *sound_pdata;
	int ret;

	codec->ignore_pmdown_time = true;

	sound_pdata = exynos_sound_get_platform_data();
	if (IS_ERR_OR_NULL(sound_pdata)) {
		pr_info("%s: doesn't use the sound_pdata\n", __func__);
		wm1811->use_jackdet_type = 0;

	} else {
		wm8994->hubs.dcs_codes_l = sound_pdata->dcs_offset_l;
		wm8994->hubs.dcs_codes_r = sound_pdata->dcs_offset_r;

		if (sound_pdata->zones) {
			wm1811->zones = (struct jack_zone *)sound_pdata->zones;
			wm1811->num_zones = sound_pdata->num_zones;
		}
		pr_info("%s:use_jackdet_type = %d\n", __func__,
				sound_pdata->use_jackdet_type);
		wm1811->use_jackdet_type = sound_pdata->use_jackdet_type;
	}

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK1,
				     UNIVERSAL222AP_DEFAULT_MCLK1,
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

	ret = snd_soc_add_codec_controls(codec, universal222ap_codec_controls,
					ARRAY_SIZE(universal222ap_codec_controls));
	if (ret < 0) {
		dev_err(codec->dev,
				"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	universal222ap_micd_set_rate(codec);

#ifdef CONFIG_SEC_DEV_JACK
	/* By default use idle_bias_off, will override for WM8994 */
	codec->dapm.idle_bias_off = 0;
#else /* CONFIG_SEC_DEV_JACK */
	wm1811->jack.status = 0;

	ret = snd_soc_jack_new(codec, "universal222ap Jack",
				SND_JACK_HEADSET | SND_JACK_BTN_0 |
				SND_JACK_BTN_1 | SND_JACK_BTN_2,
				&wm1811->jack);

	if (ret < 0)
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_MEDIA: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_1,
							KEY_VOLUMEUP);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEUP: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_2,
							KEY_VOLUMEDOWN);

	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEDOWN: %d\n", ret);

	if (wm8994->revision > 1) {
		dev_info(codec->dev, "wm1811: Rev %c support mic detection\n",
			'A' + wm8994->revision);
		if (wm1811->use_jackdet_type) {
			ret = wm8958_mic_detect(codec, &wm1811->jack,
					universal222ap_micdet,
					wm1811, NULL, NULL);
			if (ret < 0)
				dev_err(codec->dev, "Failed start detection: %d\n",
					ret);
		} else {
			ret = wm8958_mic_detect(codec, &wm1811->jack, NULL,
					NULL, universal222ap_mic_id, wm1811);
			if (ret < 0)
				dev_err(codec->dev, "Failed start detection: %d\n",
					ret);
		}

	} else {
		dev_info(codec->dev, "wm1811: Rev %c doesn't support mic detection\n",
			'A' + wm8994->revision);
		codec->dapm.idle_bias_off = 0;
	}
	/* To wakeup for earjack event in suspend mode */
	enable_irq_wake(wm8994->wm8994->irq);

	/* Init work to enable STMR */
	INIT_DELAYED_WORK(&wm1811->headset_stmr_work,
			  universal222ap_headset_stmr);

	wake_lock_init(&wm1811->jackdet_wake_lock,
					WAKE_LOCK_SUSPEND,
					"universal222ap_jackdet");

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

	return snd_soc_dapm_sync(&codec->dapm);
}

static int unviversal222ap_card_suspend_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);

	if (wm1811->lineout_switch_f) {
		if (lineout_mode == 1 &&
				wm8994->vmid_mode == WM8994_VMID_FORCE) {
			wm1811->lineout_switch_f(0);
			wm8994_vmid_mode(codec, WM8994_VMID_NORMAL);
		}
	}

	return 0;
}

static int universal222ap_card_suspend_post(struct snd_soc_card *card)
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
					     UNIVERSAL222AP_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);

		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2: %d\n",
				ret);

		ret = snd_soc_dai_set_pll(aif2_dai, WM8994_FLL2, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL2\n");

		ret = snd_soc_dai_set_sysclk(aif1_dai,
					     WM8994_SYSCLK_MCLK2,
					     UNIVERSAL222AP_DEFAULT_MCLK2,
					     SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev, "Unable to switch to MCLK2\n");

		ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1, 0, 0, 0);

		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL1\n");

		machine->set_mclk(false, true);
	}

	snd_soc_dai_set_tristate(aif1_dai, 1);

	return 0;
}

static int universal222ap_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct wm1811_machine_priv *machine =
				snd_soc_card_get_drvdata(codec->card);
	int ret;

	machine->set_mclk(true, false);
	snd_soc_dai_set_tristate(aif1_dai, 0);

	/* Switch the FLL */
	ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1,
				  WM8994_FLL_SRC_MCLK1,
				  UNIVERSAL222AP_DEFAULT_MCLK1,
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

static int universal222ap_card_resume_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);

	if (wm1811->lineout_switch_f) {
		if (lineout_mode == 1 &&
				wm8994->vmid_mode == WM8994_VMID_NORMAL) {
			wm8994_vmid_mode(codec, WM8994_VMID_FORCE);
			wm1811->lineout_switch_f(1);
		}
	}
	return 0;
}

static struct snd_soc_card universal222ap = {
	.name = "Universal222ap WM1811",
	.owner = THIS_MODULE,
	.dai_link = universal222ap_dai,

	/* If you want to use sec_fifo device,
	 * changes the num_link = 2 or ARRAY_SIZE(universal222ap_dai). */
	.num_links = ARRAY_SIZE(universal222ap_dai),

	.controls = universal222ap_controls,
	.num_controls = ARRAY_SIZE(universal222ap_controls),
	.dapm_widgets = universal222ap_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(universal222ap_dapm_widgets),
	.dapm_routes = universal222ap_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(universal222ap_dapm_routes),

	.late_probe = universal222ap_late_probe,

	.suspend_pre = unviversal222ap_card_suspend_pre,
	.suspend_post = universal222ap_card_suspend_post,
	.resume_pre = universal222ap_card_resume_pre,
	.resume_post = universal222ap_card_resume_post,
};

static int __devinit snd_universal222ap_probe(struct platform_device *pdev)
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

	sound_pdata = exynos_sound_get_platform_data();
	if (!sound_pdata) {
		pr_err("Failed to get sound pdata\n");
		goto err_get_platform_data;
	}

	/* Start the reference clock for the codec's FLL */
	if (sound_pdata->set_mclk) {
		wm1811->set_mclk = sound_pdata->set_mclk;
		wm1811->set_mclk(true, false);
	} else {
		pr_err("Failed to set mclk\n");
		goto err_set_mclk;
	}

	wm1811->pll_out = UNIVERSAL222AP_DEFAULT_SYNC_CLK;

	ret = snd_soc_register_dais(&pdev->dev, universal222ap_ext_dai,
					ARRAY_SIZE(universal222ap_ext_dai));
	if (ret != 0)
		pr_err("Failed to register external DAIs: %d\n", ret);

	snd_soc_card_set_drvdata(&universal222ap, wm1811);

	universal222ap.dev = &pdev->dev;

	ret = snd_soc_register_card(&universal222ap);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed %d\n", ret);
		goto err_register_card;
	}

	if (sound_pdata->set_ext_main_mic)
		wm1811->set_main_mic_f = sound_pdata->set_ext_main_mic;

	if (sound_pdata->set_ext_sub_mic)
		wm1811->set_sub_mic_f = sound_pdata->set_ext_sub_mic;

	if (sound_pdata->set_lineout_switch)
		wm1811->lineout_switch_f = sound_pdata->set_lineout_switch;

	if (sound_pdata->get_ground_det_value)
		wm1811->get_g_det_value_f = sound_pdata->get_ground_det_value;

	if (sound_pdata->use_jackdet_type)
		wm1811->padc = s3c_adc_register(pdev, NULL, NULL, 0);

	return 0;

err_register_card:
err_set_mclk:
err_get_platform_data:
	kfree(wm1811);
err_kzalloc:
	return ret;
}

static int __devexit snd_universal222ap_remove(struct platform_device *pdev)
{
	struct wm1811_machine_priv *wm1811 =
				snd_soc_card_get_drvdata(&universal222ap);

	if (wm1811->use_jackdet_type)
		s3c_adc_release(wm1811->padc);

	snd_soc_unregister_card(&universal222ap);
	kfree(wm1811);

	return 0;
}

static struct platform_driver snd_universal222ap_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "universal222ap-i2s",
		.pm = &snd_soc_pm_ops,
	},
	.probe = snd_universal222ap_probe,
	.remove = __devexit_p(snd_universal222ap_remove),
};
module_platform_driver(snd_universal222ap_driver);

MODULE_AUTHOR("JS. Park <aitdark.park@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC universal222ap WM1811");
MODULE_LICENSE("GPL");

