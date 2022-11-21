/*
 * linux/sound/soc/pxa/mmp3asoc-zsp.c
 *
 * Copyright (C) 2009 Marvell International Ltd.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/i2c.h>

#include <asm/mach-types.h>
#include <linux/io.h>
#include <linux/switch.h>

#include <linux/uaccess.h>
#include <plat/ssp.h>
#include <mach/addr-map.h>
#include <mach/regs-sspa.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-apmu.h>

#include "../codecs/88pm805-codec.h"
#include "mmp2-squ.h"
#include "mmp2-sspa.h"
#include <linux/delay.h>
#include <mach/mmp-zsp.h>
#include "mmp3-sspa-zsp.h"

#define MMP3ASOC_SAMPLE_RATES SNDRV_PCM_RATE_44100

#define MMP3ASOC_HEADPHONE_FUNC		0
#define MMP3ASOC_HS_MIC_FUNC		1
#define MMP3ASOC_SPK_FUNC			2
#define MMP3ASOC_MAIN_MIC_FUNC		3

#define MMP3ASOC_CTRL_ON	0
#define MMP3ASOC_CTRL_OFF	1

static int mmp3asoc_headphone_func;
static int mmp3asoc_hs_mic_func;
static int mmp3asoc_spk_func;
static int mmp3asoc_main_mic_func;

static void mmp3asoc_ext_control(struct snd_soc_dapm_context *dapm, int func)
{
	switch (func) {
	case MMP3ASOC_HEADPHONE_FUNC:
		if (mmp3asoc_headphone_func == MMP3ASOC_CTRL_ON)
			snd_soc_dapm_enable_pin(dapm, "Headset Stereophone");
		else
			snd_soc_dapm_disable_pin(dapm, "Headset Stereophone");
		break;
	case MMP3ASOC_HS_MIC_FUNC:
		if (mmp3asoc_hs_mic_func == MMP3ASOC_CTRL_ON)
			snd_soc_dapm_enable_pin(dapm, "Headset Mic 2");
		else
			snd_soc_dapm_disable_pin(dapm, "Headset Mic 2");
		break;
	case MMP3ASOC_SPK_FUNC:
		if (mmp3asoc_spk_func == MMP3ASOC_CTRL_ON) {
			snd_soc_dapm_enable_pin(dapm, "Ext Spk");
		} else {
			snd_soc_dapm_disable_pin(dapm, "Ext Spk");
		}
		break;
	case MMP3ASOC_MAIN_MIC_FUNC:
		if (mmp3asoc_main_mic_func == MMP3ASOC_CTRL_ON) {
			snd_soc_dapm_enable_pin(dapm, "Ext Mic 1");
			snd_soc_dapm_enable_pin(dapm, "Ext Mic 3");
		} else {
			snd_soc_dapm_disable_pin(dapm, "Ext Mic 1");
			snd_soc_dapm_disable_pin(dapm, "Ext Mic 3");
		}
		break;
	default:
		pr_err("wrong func type\n");
		return;
	}

	snd_soc_dapm_sync(dapm);
	return;
}

static int mmp3asoc_get_headphone(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mmp3asoc_headphone_func;
	return 0;
}

static int mmp3asoc_set_headphone(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	if (mmp3asoc_headphone_func == ucontrol->value.integer.value[0])
		return 0;

	mutex_lock(&codec->mutex);
	mmp3asoc_headphone_func = ucontrol->value.integer.value[0];
	mmp3asoc_ext_control(dapm, MMP3ASOC_HEADPHONE_FUNC);
	mutex_unlock(&codec->mutex);
	return 1;
}

static int mmp3asoc_get_hs_mic(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mmp3asoc_hs_mic_func;
	return 0;
}

static int mmp3asoc_set_hs_mic(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	if (mmp3asoc_hs_mic_func == ucontrol->value.integer.value[0])
		return 0;

	mutex_lock(&codec->mutex);
	mmp3asoc_hs_mic_func = ucontrol->value.integer.value[0];
	mmp3asoc_ext_control(dapm, MMP3ASOC_HS_MIC_FUNC);
	mutex_unlock(&codec->mutex);
	return 1;
}

static int mmp3asoc_get_spk(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mmp3asoc_spk_func;
	return 0;
}

static int mmp3asoc_set_spk(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	if (mmp3asoc_spk_func == ucontrol->value.integer.value[0])
		return 0;

	mutex_lock(&codec->mutex);
	mmp3asoc_spk_func = ucontrol->value.integer.value[0];
	mmp3asoc_ext_control(dapm, MMP3ASOC_SPK_FUNC);
	mutex_unlock(&codec->mutex);
	return 1;
}

static int mmp3asoc_get_main_mic(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mmp3asoc_main_mic_func;
	return 0;
}

static int mmp3asoc_set_main_mic(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	if (mmp3asoc_main_mic_func == ucontrol->value.integer.value[0])
		return 0;

	mutex_lock(&codec->mutex);
	mmp3asoc_main_mic_func = ucontrol->value.integer.value[0];
	mmp3asoc_ext_control(dapm, MMP3ASOC_MAIN_MIC_FUNC);
	mutex_unlock(&codec->mutex);
	return 1;
}

static const struct snd_soc_dapm_widget mmp3asoc_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Stereophone", NULL),
	SND_SOC_DAPM_LINE("Lineout Out 1", NULL),
	SND_SOC_DAPM_LINE("Lineout Out 2", NULL),
	SND_SOC_DAPM_SPK("Ext Speaker", NULL),
	SND_SOC_DAPM_MIC("Ext Mic 1", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Ext Mic 3", NULL),
};

static const struct snd_soc_dapm_route mmp3asoc_dapm_routes[] = {
	{"Headset Stereophone", NULL, "HS1"},
	{"Headset Stereophone", NULL, "HS2"},

	{"Ext Speaker", NULL, "LSP"},
	{"Ext Speaker", NULL, "LSN"},

	{"Lineout Out 1", NULL, "LINEOUT1"},
	{"Lineout Out 2", NULL, "LINEOUT2"},

	{"MIC1P", NULL, "Mic1 Bias"},
	{"MIC1N", NULL, "Mic1 Bias"},
	{"Mic1 Bias", NULL, "Ext Mic 1"},

	{"MIC2P", NULL, "Mic1 Bias"},
	{"MIC2N", NULL, "Mic1 Bias"},
	{"Mic1 Bias", NULL, "Headset Mic 2"},

	{"MIC3P", NULL, "Mic3 Bias"},
	{"MIC3N", NULL, "Mic3 Bias"},
	{"Mic3 Bias", NULL, "Ext Mic 3"},
};

static const char *const jack_function[] = {
	"Headphone", "Mic", "Headset", "Off" };

static const char *headphone_function[] = {"On", "Off"};
static const char *hs_mic_function[] = {"On", "Off"};
static const char *spk_function[] = {"On", "Off"};
static const char *main_mic_function[] = {"On", "Off"};

static const struct soc_enum mmp3asoc_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, headphone_function),
	SOC_ENUM_SINGLE_EXT(2, hs_mic_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, main_mic_function),
};

static const struct snd_kcontrol_new mmp3asoc_elba_controls[] = {
	SOC_ENUM_EXT("Headphone Function", mmp3asoc_enum[0],
		     mmp3asoc_get_headphone, mmp3asoc_set_headphone),
	SOC_ENUM_EXT("Headset Mic Function", mmp3asoc_enum[1],
		     mmp3asoc_get_hs_mic, mmp3asoc_set_hs_mic),
	SOC_ENUM_EXT("Speaker Function", mmp3asoc_enum[2],
		     mmp3asoc_get_spk, mmp3asoc_set_spk),
	SOC_ENUM_EXT("Main Mic Function", mmp3asoc_enum[3],
		     mmp3asoc_get_main_mic, mmp3asoc_set_main_mic),
};

static int codec_hdmi_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct platform_device *mmp3asoc_snd_device[2];

static int codec_elba_init(struct snd_soc_pcm_runtime *rtd)
{
#if 0
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int err;


	/* Add mmp3asoc specific controls */
	err = snd_soc_add_controls(codec, mmp3asoc_elba_controls,
				   ARRAY_SIZE(mmp3asoc_elba_controls));
	if (err < 0)
		return err;

	/* add mmp3asoc specific widgets */
	snd_soc_dapm_new_controls(dapm, mmp3asoc_dapm_widgets,
				  ARRAY_SIZE(mmp3asoc_dapm_widgets));

	/* set up mmp3asoc specific audio routes */
	snd_soc_dapm_add_routes(dapm, mmp3asoc_dapm_routes,
				ARRAY_SIZE(mmp3asoc_dapm_routes));

	snd_soc_dapm_enable_pin(dapm, "Ext Speaker");
	snd_soc_dapm_enable_pin(dapm, "Ext Mic 1");
	snd_soc_dapm_enable_pin(dapm, "Ext Mic 3");
	snd_soc_dapm_disable_pin(dapm, "Headset Mic 2");
	snd_soc_dapm_disable_pin(dapm, "Headset Stereophone");

	/* set endpoints to not connected */
	snd_soc_dapm_nc_pin(dapm, "AUX1");
	snd_soc_dapm_nc_pin(dapm, "AUX2");
	snd_soc_dapm_nc_pin(dapm, "MIC1P");
	snd_soc_dapm_nc_pin(dapm, "MIC1N");
	snd_soc_dapm_nc_pin(dapm, "MIC2P");
	snd_soc_dapm_nc_pin(dapm, "MIC2N");
	snd_soc_dapm_nc_pin(dapm, "MIC3P");
	snd_soc_dapm_nc_pin(dapm, "MIC3N");

	/* output widget */
	snd_soc_dapm_nc_pin(dapm, "HS1");
	snd_soc_dapm_nc_pin(dapm, "HS2");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2");
	snd_soc_dapm_nc_pin(dapm, "EARP");
	snd_soc_dapm_nc_pin(dapm, "EARN");
	snd_soc_dapm_nc_pin(dapm, "LSP");
	snd_soc_dapm_nc_pin(dapm, "LSN");

	mutex_lock(&codec->mutex);
	mmp3asoc_headphone_func = MMP3ASOC_CTRL_OFF;
	mmp3asoc_hs_mic_func = MMP3ASOC_CTRL_OFF;
	mmp3asoc_spk_func = MMP3ASOC_CTRL_OFF;
	mmp3asoc_main_mic_func = MMP3ASOC_CTRL_OFF;
	mmp3asoc_ext_control(dapm, MMP3ASOC_HEADPHONE_FUNC);
	mmp3asoc_ext_control(dapm, MMP3ASOC_HS_MIC_FUNC);
	mmp3asoc_ext_control(dapm, MMP3ASOC_SPK_FUNC);
	mmp3asoc_ext_control(dapm, MMP3ASOC_MAIN_MIC_FUNC);
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&codec->mutex);
#endif
	return 0;
}

static int mmp3asoc_probe(struct snd_soc_card *card)
{
	return 0;
}

static int mmp3asoc_hdmi_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	cpu_dai->driver->playback.channels_min = 2;
	cpu_dai->driver->playback.channels_max = 2;

	cpu_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->playback.rates = MMP3ASOC_SAMPLE_RATES;

	return 0;
}

static int mmp3asoc_hdmi_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
	int retclk;
	struct mmp_zsp_clkcfg zspclkcfg;

	pr_debug("%s: enter, rate %d\n", __func__, params_rate(params));

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = params_rate(params) * 256;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = params_rate(params) * 512;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);

#ifdef CONFIG_SND_ELBA_MASTER_MODE
	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
#else
	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
#endif
	/* SSPA clock ctrl register changes, and can't use previous API */
	/* set the sspa pll */
	snd_soc_dai_set_sysclk(cpu_dai, 0, freq_out, 0);
	/* set the mclk */
	snd_soc_dai_set_pll(cpu_dai, MMP_SYSCLK, SSPA_AUDIO_PLL,
			freq_out, sysclk);

	/* set elba sysclk */
	snd_soc_dai_set_sysclk(codec_dai, 0, 0, PM805_CODEC_CLK_DIR_OUT);

	if ((params_rate(params) % 8000) == 0)
		zspclkcfg.asclk = MMP_ZSP_ASCLK_24576000;
	else
		zspclkcfg.asclk = MMP_ZSP_ASCLK_22579200;

	/* get current clock settings */
	retclk = zsp_set_clock_preference(MMP_ZSP_ASCLK_FLAGS,
			&zspclkcfg);
	if (retclk != 0) {
		/* failed, cannot change now */
		return -EINVAL;
	}


	return 0;
}
static int mmp3asoc_elba_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	cpu_dai->driver->playback.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->capture.formats = SNDRV_PCM_FMTBIT_S16_LE;
	cpu_dai->driver->playback.rates = MMP3ASOC_SAMPLE_RATES;
	cpu_dai->driver->capture.rates = MMP3ASOC_SAMPLE_RATES;

	return 0;
}

static int mmp3asoc_elba_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int freq_in, freq_out, sspa_mclk, sysclk, sspa_div;
	int retclk;
	struct mmp_zsp_clkcfg zspclkcfg;

	pr_debug("%s: enter, rate %d\n", __func__, params_rate(params));

	freq_in = 26000000;
	if (params_rate(params) > 11025) {
		freq_out = params_rate(params) * 512;
		sysclk = params_rate(params) * 256;
		sspa_mclk = params_rate(params) * 64;
	} else {
		freq_out = params_rate(params) * 1024;
		sysclk = params_rate(params) * 512;
		sspa_mclk = params_rate(params) * 64;
	}
	sspa_div = freq_out;
	do_div(sspa_div, sspa_mclk);
	if (machine_is_yellowstone() || machine_is_thunderstonem()) {
		if (cpu_dai->id == 0) {
			snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
			snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
		} else {
			snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
			snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
		}
	} else {
		snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
		snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			    SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	}
	/* SSPA clock ctrl register changes, and can't use previous API */
	/* set the sspa pll */
	snd_soc_dai_set_sysclk(cpu_dai, 0, freq_out, 0);
	/* set the mclk */
	snd_soc_dai_set_pll(cpu_dai, MMP_SYSCLK, SSPA_AUDIO_PLL,
			freq_out, sysclk);

	/* set elba sysclk */
	snd_soc_dai_set_sysclk(codec_dai, 0, 0, PM805_CODEC_CLK_DIR_OUT);

	return 0;
}

#ifdef CONFIG_PM
static int mmp3asoc_suspend_post(struct snd_soc_card *card)
{

	return 0;
}

static int mmp3asoc_resume_pre(struct snd_soc_card *card)
{

	return 0;
}

#endif

/* machine stream operations */
static struct snd_soc_ops mmp3asoc_machine_ops[] = {
	{
	 .startup = mmp3asoc_hdmi_startup,
	 .hw_params = mmp3asoc_hdmi_hw_params,
	 },
	{
	 .startup = mmp3asoc_elba_startup,
	 .hw_params = mmp3asoc_elba_hw_params,
	 },
};

/* digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link mmp3_asoc_elba_dai[] = {
	{
	 .name = "ELBA I2S",
	 .stream_name = "I2S Audio 1",
	 .codec_name = "88pm80x-codec",
	 .platform_name = "mmp3-pcm-audio",
	 .cpu_dai_name = "mmp3-sspa-dai.0",
	 .codec_dai_name = "88pm805-i2s",
	 .ops = &mmp3asoc_machine_ops[1],
	 .init = codec_elba_init,
	 },
	{
	 .name = "ELBA PCM",
	 .stream_name = "I2S Audio 2",
	 .codec_name = "88pm80x-codec",
	 .platform_name = "mmp3-pcm-audio",
	 .cpu_dai_name = "mmp3-sspa-dai.1",
	 .codec_dai_name = "88pm805-i2s",
	 .ops = &mmp3asoc_machine_ops[1],
	 .init = codec_elba_init,
	 },

};

static struct snd_soc_dai_link mmp3_asoc_hdmi_dai[] = {
	{
	 .name = "HDMI",
	 .stream_name = "hdmi Audio",
	 .codec_name = "dummy-codec",
	 .platform_name = "mmp3-pcm-audio",
	 .cpu_dai_name = "mmp3-sspa-dai.0",
	 .codec_dai_name = "dummy-dai",
	 .ops = &mmp3asoc_machine_ops[0],
	 .init = codec_hdmi_init,
	 },
};

/* audio machine driver */
static struct snd_soc_card snd_soc_mmp3asoc[] = {
	{
	 .name = "mmp3 asoc",
	 .dai_link = &mmp3_asoc_elba_dai[0],
	 .num_links = 2,
	 .probe = mmp3asoc_probe,
#ifdef CONFIG_PM
	 .suspend_post = mmp3asoc_suspend_post,
	 .resume_pre = mmp3asoc_resume_pre,
#endif
	 },
	{
	 .name = "mmp3 hdmi",
	 .dai_link = &mmp3_asoc_hdmi_dai[0],
	 .num_links = 1,
	 .probe = mmp3asoc_probe,
	 },
};

static int __init mmp3asoc_init(void)
{
	int i, ret[2];

	if (!machine_is_abilene() && !machine_is_yellowstone()
		&& !machine_is_orchid() && !machine_is_thunderstonem()) {
		pr_err("%s: Unkonwn machine not supported!\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < 2; i++) {
		mmp3asoc_snd_device[i] = platform_device_alloc("soc-audio", i);
		if (!mmp3asoc_snd_device[i])
			return -ENOMEM;
		platform_set_drvdata(mmp3asoc_snd_device[i],
				&snd_soc_mmp3asoc[i]);
		ret[i] = platform_device_add(mmp3asoc_snd_device[i]);

		if (ret[i])
			platform_device_put(mmp3asoc_snd_device[i]);
	}

	return ret[1];
}

static void __exit mmp3asoc_exit(void)
{
	int i;
	for (i = 0; i < 2; i++)
		platform_device_unregister(mmp3asoc_snd_device[i]);
}

module_init(mmp3asoc_init);
module_exit(mmp3asoc_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC MMP3 ZSP");
MODULE_LICENSE("GPL");
