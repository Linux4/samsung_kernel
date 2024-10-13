/*
 * linux/sound/soc/codecs/tlv320aic32x4.c
 *
 * Copyright 2011 Vista Silicon S.L.
 *
 * Author: Javier Martin <javier.martin@vista-silicon.com>
 *
 * Based on sound/soc/codecs/wm8974 and TI driver for kernel 2.6.27.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#define NINGLEI_LOOP_TEST 1
#define NINGLEI_CODEC_REG_TEST 1

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <sound/tlv320aic32x4.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tlv320aic32x4.h"

#include <linux/sysfs.h>



#define N_PRINT pr_debug
//#define N_PRINT(fmt,...) {};
//#define N_PRINT pr_debug
#define N_PRINT_CODEC_REG_TEST pr_err

#define TI3254_GAIN(size, m, db) (1 == m ? ((1 << (size)) - (db)) : (db) )
#define TI3254_HP_GAIN(m, db) (TI3254_GAIN(6, m, db))

#define TI3254_HPL_GAIN (TI3254_HP_GAIN(0, 0))
#define TI3254_HPR_GAIN (TI3254_HPL_GAIN)

#define TI3254_MCLK (24000)
#define TI3254_PLL_P (2)
#define TI3254_PLL_R (1)
#define TI3254_PLL_J (7)
#define TI3254_PLL_D (1680)
//#define BCLK_FROM_DAC_MOD_CLK
#define TI3254_BCLK_N (14)

#define TI3254_NDAC (2)
#define TI3254_MDAC (7)
#define TI3254_DOSR (128)


struct aic32x4_rate_divs {
	u32 mclk;
	u32 rate;
	u8 p_val;
	u8 pll_j;
	u16 pll_d;
	u16 dosr;
	u8 ndac;
	u8 mdac;
	u8 aosr;
	u8 nadc;
	u8 madc;
	u8 blck_N;
};

struct aic32x4_priv {
	u32 sysclk;
	u8 page_no;
	void *control_data;
	u32 power_cfg;
	u32 micpga_routing;
	bool swapdacs;
	int rstn_gpio;
	struct snd_soc_codec *codec;
};

/* 0dB min, 1dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_1, 0, 100, 0);
/* 0dB min, 0.5dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_0_5, 0, 50, 0);

static const struct snd_kcontrol_new aic32x4_snd_controls[] = {
	SOC_DOUBLE_R_TLV("PCM Playback Volume", AIC32X4_LDACVOL,
			AIC32X4_RDACVOL, 0, 0x30, 0, tlv_step_0_5),
	SOC_DOUBLE_R_TLV("HP Driver Gain Volume", AIC32X4_HPLGAIN,
			AIC32X4_HPRGAIN, 0, 0x1D, 0, tlv_step_1),
	SOC_DOUBLE_R_TLV("LO Driver Gain Volume", AIC32X4_LOLGAIN,
			AIC32X4_LORGAIN, 0, 0x1D, 0, tlv_step_1),
	SOC_DOUBLE_R("HP DAC Playback Switch", AIC32X4_HPLGAIN,
			AIC32X4_HPRGAIN, 6, 0x01, 1),
	SOC_DOUBLE_R("LO DAC Playback Switch", AIC32X4_LOLGAIN,
			AIC32X4_LORGAIN, 6, 0x01, 1),
	SOC_DOUBLE_R("Mic PGA Switch", AIC32X4_LMICPGAVOL,
			AIC32X4_RMICPGAVOL, 7, 0x01, 1),

	SOC_SINGLE("ADCFGA Left Mute Switch", AIC32X4_ADCFGA, 7, 1, 1),
	SOC_SINGLE("ADCFGA Right Mute Switch", AIC32X4_ADCFGA, 3, 1, 1),

	SOC_DOUBLE_R_TLV("ADC Level Volume", AIC32X4_LADCVOL,
			AIC32X4_RADCVOL, 0, 0x28, 0, tlv_step_0_5),
	SOC_DOUBLE_R_TLV("PGA Level Volume", AIC32X4_LMICPGAVOL,
			AIC32X4_RMICPGAVOL, 0, 0x5f, 0, tlv_step_0_5),

	SOC_SINGLE("Auto-mute Switch", AIC32X4_DACMUTE, 4, 7, 0),

	SOC_SINGLE("AGC Left Switch", AIC32X4_LAGC1, 7, 1, 0),
	SOC_SINGLE("AGC Right Switch", AIC32X4_RAGC1, 7, 1, 0),
	SOC_DOUBLE_R("AGC Target Level", AIC32X4_LAGC1, AIC32X4_RAGC1,
			4, 0x07, 0),
	SOC_DOUBLE_R("AGC Gain Hysteresis", AIC32X4_LAGC1, AIC32X4_RAGC1,
			0, 0x03, 0),
	SOC_DOUBLE_R("AGC Hysteresis", AIC32X4_LAGC2, AIC32X4_RAGC2,
			6, 0x03, 0),
	SOC_DOUBLE_R("AGC Noise Threshold", AIC32X4_LAGC2, AIC32X4_RAGC2,
			1, 0x1F, 0),
	SOC_DOUBLE_R("AGC Max PGA", AIC32X4_LAGC3, AIC32X4_RAGC3,
			0, 0x7F, 0),
	SOC_DOUBLE_R("AGC Attack Time", AIC32X4_LAGC4, AIC32X4_RAGC4,
			3, 0x1F, 0),
	SOC_DOUBLE_R("AGC Decay Time", AIC32X4_LAGC5, AIC32X4_RAGC5,
			3, 0x1F, 0),
	SOC_DOUBLE_R("AGC Noise Debounce", AIC32X4_LAGC6, AIC32X4_RAGC6,
			0, 0x1F, 0),
	SOC_DOUBLE_R("AGC Signal Debounce", AIC32X4_LAGC7, AIC32X4_RAGC7,
			0, 0x0F, 0),
};

static const struct aic32x4_rate_divs aic32x4_divs[] = {
	/* 8k rate */
	{AIC32X4_FREQ_12000000, 8000, 1, 7, 6800, 768, 5, 3, 128, 5, 18, 24},
	{AIC32X4_FREQ_24000000, 8000, 2, 7, 6800, 768, 15, 1, 64, 45, 4, 24},
	{AIC32X4_FREQ_25000000, 8000, 2, 7, 3728, 768, 15, 1, 64, 45, 4, 24},
	/* 11.025k rate */
	{AIC32X4_FREQ_12000000, 11025, 1, 7, 5264, 512, 8, 2, 128, 8, 8, 16},
	{AIC32X4_FREQ_24000000, 11025, 2, 7, 5264, 512, 16, 1, 64, 32, 4, 16},
	/* 16k rate */
	{AIC32X4_FREQ_12000000, 16000, 1, 7, 6800, 384, 5, 3, 128, 5, 9, 12},
	{AIC32X4_FREQ_24000000, 16000, 2, 7, 6800, 384, 15, 1, 64, 18, 5, 12},
	{AIC32X4_FREQ_25000000, 16000, 2, 7, 3728, 384, 15, 1, 64, 18, 5, 12},
	/* 22.05k rate */
	{AIC32X4_FREQ_12000000, 22050, 1, 7, 5264, 256, 4, 4, 128, 4, 8, 8},
	{AIC32X4_FREQ_24000000, 22050, 2, 7, 5264, 256, 16, 1, 64, 16, 4, 8},
	{AIC32X4_FREQ_25000000, 22050, 2, 7, 2253, 256, 16, 1, 64, 16, 4, 8},
	/* 32k rate */
	{AIC32X4_FREQ_12000000, 32000, 1, 7, 1680, 192, 2, 7, 64, 2, 21, 6},
	{AIC32X4_FREQ_24000000, 32000, 2, 7, 1680, 192, 7, 2, 64, 7, 6, 6},
	/* 44.1k rate */
	{AIC32X4_FREQ_12000000, 44100, 1, 7, 5264, 128, 2, 8, 128, 2, 8, 4},
	{AIC32X4_FREQ_24000000, 44100, 2, 7, 5264, 128, 8, 2, 64, 8, 4, 4},
	{AIC32X4_FREQ_25000000, 44100, 2, 7, 2253, 128, 8, 2, 64, 8, 4, 4},
	/* 48k rate */
	{AIC32X4_FREQ_12000000, 48000, 1, 8, 1920, 128, 2, 8, 128, 2, 8, 4},
	{AIC32X4_FREQ_24000000, 48000, 2, 8, 1920, 128, 8, 2, 64, 8, 4, 4},
	{AIC32X4_FREQ_25000000, 48000, 2, 7, 8643, 128, 8, 2, 64, 8, 4, 4},

	/*48K rate 26M*/
	{AIC32X4_FREQ_26000000, 48000, TI3254_PLL_P, TI3254_PLL_J, TI3254_PLL_D, TI3254_DOSR, TI3254_NDAC, TI3254_MDAC, 0x80, 1, 1, TI3254_BCLK_N},
};

static const struct snd_kcontrol_new hpl_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("L_DAC Switch", AIC32X4_HPLROUTE, 3, 1, 0),
	SOC_DAPM_SINGLE("IN1_L Switch", AIC32X4_HPLROUTE, 2, 1, 0),

	SOC_DAPM_SINGLE("MA_L Switch", AIC32X4_HPLROUTE, 1, 1, 0),
};

static const struct snd_kcontrol_new hpr_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("R_DAC Switch", AIC32X4_HPRROUTE, 3, 1, 0),
	SOC_DAPM_SINGLE("IN1_R Switch", AIC32X4_HPRROUTE, 2, 1, 0),

	SOC_DAPM_SINGLE("MA_R Switch", AIC32X4_HPRROUTE, 1, 1, 0),
};

static const struct snd_kcontrol_new lol_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("L_DAC Switch", AIC32X4_LOLROUTE, 3, 1, 0),
};

static const struct snd_kcontrol_new lor_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("R_DAC Switch", AIC32X4_LORROUTE, 3, 1, 0),
};

static const struct snd_kcontrol_new left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1_L P Switch", AIC32X4_LMICPGAPIN, 7, 1, 0),// bit 7
	SOC_DAPM_SINGLE("IN2_L P Switch", AIC32X4_LMICPGAPIN, 4, 1, 0),
	SOC_DAPM_SINGLE("IN3_L P Switch", AIC32X4_LMICPGAPIN, 2, 1, 0),
};

static const struct snd_kcontrol_new right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1_R P Switch", AIC32X4_RMICPGAPIN, 7, 1, 0), //bit 7
	SOC_DAPM_SINGLE("IN2_R P Switch", AIC32X4_RMICPGAPIN, 4, 1, 0),
	SOC_DAPM_SINGLE("IN3_R P Switch", AIC32X4_RMICPGAPIN, 2, 1, 0),
};

static const struct snd_soc_dapm_widget aic32x4_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", AIC32X4_DACSETUP, 7, 0),
	SND_SOC_DAPM_MIXER("HPL Output Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_output_mixer_controls[0],
			   ARRAY_SIZE(hpl_output_mixer_controls)),
	SND_SOC_DAPM_PGA("HPL Power", AIC32X4_OUTPWRCTL, 5, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("LOL Output Mixer", SND_SOC_NOPM, 0, 0,
			   &lol_output_mixer_controls[0],
			   ARRAY_SIZE(lol_output_mixer_controls)),
	SND_SOC_DAPM_PGA("LOL Power", AIC32X4_OUTPWRCTL, 3, 0, NULL, 0),

	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", AIC32X4_DACSETUP, 6, 0),
	SND_SOC_DAPM_MIXER("HPR Output Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_output_mixer_controls[0],
			   ARRAY_SIZE(hpr_output_mixer_controls)),
	SND_SOC_DAPM_PGA("HPR Power", AIC32X4_OUTPWRCTL, 4, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("LOR Output Mixer", SND_SOC_NOPM, 0, 0,
			   &lor_output_mixer_controls[0],
			   ARRAY_SIZE(lor_output_mixer_controls)),
	SND_SOC_DAPM_PGA("LOR Power", AIC32X4_OUTPWRCTL, 2, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Left Input Mixer", SND_SOC_NOPM, 0, 0,
			   &left_input_mixer_controls[0],
			   ARRAY_SIZE(left_input_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Input Mixer", SND_SOC_NOPM, 0, 0,
			   &right_input_mixer_controls[0],
			   ARRAY_SIZE(right_input_mixer_controls)),
			   
	SND_SOC_DAPM_PGA("MAL Power", AIC32X4_OUTPWRCTL, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MAR Power", AIC32X4_OUTPWRCTL, 0, 0, NULL, 0),
	
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", AIC32X4_ADCSETUP, 7, 0),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", AIC32X4_ADCSETUP, 6, 0),
	SND_SOC_DAPM_MICBIAS("Mic Bias", AIC32X4_MICBIAS, 6, 0),

	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("LOL"),
	SND_SOC_DAPM_OUTPUT("LOR"),
	SND_SOC_DAPM_INPUT("IN1_L"),
	SND_SOC_DAPM_INPUT("IN1_R"),
	SND_SOC_DAPM_INPUT("IN2_L"),
	SND_SOC_DAPM_INPUT("IN2_R"),
	SND_SOC_DAPM_INPUT("IN3_L"),
	SND_SOC_DAPM_INPUT("IN3_R"),
};

static const struct snd_soc_dapm_route aic32x4_dapm_routes[] = {
	/* Left Output */
	{"HPL Output Mixer", "L_DAC Switch", "Left DAC"},
	{"HPL Output Mixer", "IN1_L Switch", "IN1_L"},
	{"HPL Output Mixer", "MA_L Switch", "MAL Power"},
	
	{"HPL Power", NULL, "HPL Output Mixer"},
	{"HPL", NULL, "HPL Power"},

	{"LOL Output Mixer", "L_DAC Switch", "Left DAC"},

	{"LOL Power", NULL, "LOL Output Mixer"},
	{"LOL", NULL, "LOL Power"},

	/* Right Output */
	{"HPR Output Mixer", "R_DAC Switch", "Right DAC"},
	{"HPR Output Mixer", "IN1_R Switch", "IN1_R"},
	{"HPR Output Mixer", "MA_R Switch", "MAR Power"},

	{"HPR Power", NULL, "HPR Output Mixer"},
	{"HPR", NULL, "HPR Power"},

	{"LOR Output Mixer", "R_DAC Switch", "Right DAC"},

	{"LOR Power", NULL, "LOR Output Mixer"},
	{"LOR", NULL, "LOR Power"},

	/* Left input */
	{"Left Input Mixer", "IN1_L P Switch", "Mic Bias"},
	{"Mic Bias", NULL, "IN1_L"},
	{"Left Input Mixer", "IN2_L P Switch", "IN2_L"},
	{"Left Input Mixer", "IN3_L P Switch", "IN3_L"},
	
	{"Left ADC", NULL, "Left Input Mixer"},
	{"MAL Power", NULL, "Left Input Mixer"},

	/* Right Input */
	{"Right Input Mixer", "IN1_R P Switch", "Mic Bias"},
	{"Mic Bias", NULL, "IN1_R"},
	{"Right Input Mixer", "IN2_R P Switch", "IN2_R"},
	{"Right Input Mixer", "IN3_R P Switch", "IN3_R"},

	{"Right ADC", NULL, "Right Input Mixer"},
	{"MAR Power", NULL, "Right Input Mixer"},
};

static inline int aic32x4_change_page(struct snd_soc_codec *codec,
					unsigned int new_page)
{
	struct aic32x4_priv *aic32x4 = snd_soc_codec_get_drvdata(codec);
	u8 data[2];
	int ret;

	data[0] = 0x00;
	data[1] = new_page & 0xff;

	ret = codec->hw_write(codec->control_data, data, 2);
	if (ret == 2) {
		aic32x4->page_no = new_page;
		return 0;
	} else {
		return ret;
	}
}

static int aic32x4_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int val)
{
	struct aic32x4_priv *aic32x4 = snd_soc_codec_get_drvdata(codec);
	unsigned int page = reg / 128;
	unsigned int fixed_reg = reg % 128;
	u8 data[2];
	int ret;
	N_PRINT("ninglei aic32x4_write page = %u, fixed_reg = %u_0x%x, val = %u_0x%x\n", page, fixed_reg, fixed_reg, val, val);
	/* A write to AIC32X4_PSEL is really a non-explicit page change */
	if (reg == AIC32X4_PSEL)
		return aic32x4_change_page(codec, val);

	if (aic32x4->page_no != page) {
		ret = aic32x4_change_page(codec, page);
		if (ret != 0)
			return ret;
	}

	data[0] = fixed_reg & 0xff;
	data[1] = val & 0xff;

	if (codec->hw_write(codec->control_data, data, 2) == 2)
		return 0;
	else
		return -EIO;
}

static unsigned int aic32x4_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct aic32x4_priv *aic32x4 = snd_soc_codec_get_drvdata(codec);
	unsigned int page = reg / 128;
	unsigned int fixed_reg = reg % 128;
	int ret;
	s32 retval = 0; 

	if (aic32x4->page_no != page) {
		ret = aic32x4_change_page(codec, page);
		if (ret != 0)
			return ret;
	}
	retval = i2c_smbus_read_byte_data(codec->control_data, fixed_reg & 0xff);
	N_PRINT("ninglei aic32x4_read page = %u, fixed_reg = %u_0x%x, val = %u_0x%x\n", page, fixed_reg, fixed_reg, retval, retval);
	return retval;
}

static inline int aic32x4_get_divs(int mclk, int rate)
{
	int i;
	N_PRINT("ninglei aic32x4_get_divs mclk=%d, rate=%d", mclk, rate);
	for (i = 0; i < ARRAY_SIZE(aic32x4_divs); i++) {
		if ((aic32x4_divs[i].rate == rate)
		    && (aic32x4_divs[i].mclk == mclk)) {
			return i;
		}
	}
	printk(KERN_ERR "aic32x4: master clock and sample rate is not supported\n");
	return -EINVAL;
}

static int aic32x4_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(&codec->dapm, aic32x4_dapm_widgets,
				  ARRAY_SIZE(aic32x4_dapm_widgets));

	snd_soc_dapm_add_routes(&codec->dapm, aic32x4_dapm_routes,
				ARRAY_SIZE(aic32x4_dapm_routes));

	snd_soc_dapm_new_widgets(&codec->dapm);
	return 0;
}

static int aic32x4_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic32x4_priv *aic32x4 = snd_soc_codec_get_drvdata(codec);
	N_PRINT("ninglei enter aic32x4_set_dai_sysclk");
	switch (freq) {
	case AIC32X4_FREQ_12000000:
	case AIC32X4_FREQ_24000000:
	case AIC32X4_FREQ_25000000:
		aic32x4->sysclk = freq;
		return 0;
	}
	printk(KERN_ERR "aic32x4: invalid frequency to set DAI system clock\n");
	return -EINVAL;
}

static int aic32x4_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 iface_reg_1;
	u8 iface_reg_2;
	u8 iface_reg_3;
	N_PRINT("ninglei enter aic32x4_set_dai_fmt\n");
	iface_reg_1 = snd_soc_read(codec, AIC32X4_IFACE1);
	iface_reg_1 = iface_reg_1 & ~(3 << 6 | 3 << 2);
	iface_reg_2 = snd_soc_read(codec, AIC32X4_IFACE2);
	iface_reg_2 = 0;
	iface_reg_3 = snd_soc_read(codec, AIC32X4_IFACE3);
	iface_reg_3 = iface_reg_3 & ~(1 << 3);
	
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface_reg_1 |= AIC32X4_BCLKMASTER | AIC32X4_WCLKMASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		printk(KERN_ERR "aic32x4: invalid DAI master/slave interface\n");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_reg_1 |= (AIC32X4_DSP_MODE << AIC32X4_PLLJ_SHIFT);
		iface_reg_3 |= (1 << 3); /* invert bit clock */
		iface_reg_2 = 0x01; /* add offset 1 */
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface_reg_1 |= (AIC32X4_DSP_MODE << AIC32X4_PLLJ_SHIFT);
		iface_reg_3 |= (1 << 3); /* invert bit clock */
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg_1 |=
			(AIC32X4_RIGHT_JUSTIFIED_MODE << AIC32X4_PLLJ_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg_1 |=
			(AIC32X4_LEFT_JUSTIFIED_MODE << AIC32X4_PLLJ_SHIFT);
		break;
	default:
		printk(KERN_ERR "aic32x4: invalid DAI interface format\n");
		return -EINVAL;
	}

	snd_soc_write(codec, AIC32X4_IFACE1, iface_reg_1);
	snd_soc_write(codec, AIC32X4_IFACE2, iface_reg_2);
	snd_soc_write(codec, AIC32X4_IFACE3, iface_reg_3);
	return 0;
}

static int aic32x4_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aic32x4_priv *aic32x4 = snd_soc_codec_get_drvdata(codec);
	u8 data;
	int i;
	N_PRINT("ninglei enter aic32x4_hw_params\n");
	i = aic32x4_get_divs(aic32x4->sysclk, 48000);
	if (i < 0) {
		printk(KERN_ERR "aic32x4: sampling rate not supported\n");
		return i;
	}

	/* Use PLL as CODEC_CLKIN and DAC_MOD_CLK as BDIV_CLKIN */
	snd_soc_write(codec, AIC32X4_CLKMUX, AIC32X4_PLLCLKIN);
	#ifdef BCLK_FROM_DAC_MOD_CLK
		snd_soc_write(codec, AIC32X4_IFACE3, AIC32X4_DACMOD2BCLK);
	#endif


	/* We will fix R value to 1 and will make P & J=K.D as varialble */
	data = snd_soc_read(codec, AIC32X4_PLLPR);
	data &= ~(7 << 4);
	snd_soc_write(codec, AIC32X4_PLLPR,
		      (data | BIT(7) | (aic32x4_divs[i].p_val << 4) | 0x01));

	snd_soc_write(codec, AIC32X4_PLLJ, aic32x4_divs[i].pll_j);

	snd_soc_write(codec, AIC32X4_PLLDMSB, (aic32x4_divs[i].pll_d >> 8));
	snd_soc_write(codec, AIC32X4_PLLDLSB,
		      (aic32x4_divs[i].pll_d & 0xff));
	/* NDAC divider value */
	data = snd_soc_read(codec, AIC32X4_NDAC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC32X4_NDAC, data | BIT(7) | aic32x4_divs[i].ndac);

	/* MDAC divider value */
	data = snd_soc_read(codec, AIC32X4_MDAC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC32X4_MDAC, data | BIT(7) | aic32x4_divs[i].mdac);

	/* DOSR MSB & LSB values */
	snd_soc_write(codec, AIC32X4_DOSRMSB, aic32x4_divs[i].dosr >> 8);
	snd_soc_write(codec, AIC32X4_DOSRLSB,
		      (aic32x4_divs[i].dosr & 0xff));

	/* NADC divider value */
	data = snd_soc_read(codec, AIC32X4_NADC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC32X4_NADC, data | aic32x4_divs[i].nadc);

	/* MADC divider value */
	data = snd_soc_read(codec, AIC32X4_MADC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC32X4_MADC, data | aic32x4_divs[i].madc);

	/* AOSR value */
	snd_soc_write(codec, AIC32X4_AOSR, aic32x4_divs[i].aosr);

	/* BCLK N divider */
	data = snd_soc_read(codec, AIC32X4_BCLKN);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC32X4_BCLKN, data | BIT(7) | aic32x4_divs[i].blck_N);

	data = snd_soc_read(codec, AIC32X4_IFACE1);
	data = data & ~(3 << 4);
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (AIC32X4_WORD_LEN_20BITS << AIC32X4_DOSRMSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (AIC32X4_WORD_LEN_24BITS << AIC32X4_DOSRMSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (AIC32X4_WORD_LEN_32BITS << AIC32X4_DOSRMSB_SHIFT);
		break;
	}
	data = data | (3<<6)|(1<<3)|(1<<2);// ljf,bclk out,wclk out
	snd_soc_write(codec, AIC32X4_IFACE1, data);
	snd_soc_write(codec, AIC32X4_IFACE2,0x1); //0.0x1c
	return 0;
}

static int aic32x4_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 dac_reg;
	N_PRINT("ninglei enter aic32x4_mute mute = %d\n", mute);
	dac_reg = snd_soc_read(codec, AIC32X4_DACMUTE) & ~AIC32X4_MUTEON;
	if (mute)
		snd_soc_write(codec, AIC32X4_DACMUTE, dac_reg | AIC32X4_MUTEON);
	else
		snd_soc_write(codec, AIC32X4_DACMUTE, dac_reg);
	return 0;
}

static int aic32x4_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	N_PRINT("ninglei enter aic32x4_set_bias_level=%d\n", (unsigned int)level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		/* Switch on PLL */
		snd_soc_update_bits(codec, AIC32X4_PLLPR,
				    AIC32X4_PLLEN, AIC32X4_PLLEN);

		/* Switch on NDAC Divider */
		snd_soc_update_bits(codec, AIC32X4_NDAC,
				    AIC32X4_NDACEN, AIC32X4_NDACEN);

		/* Switch on MDAC Divider */
		snd_soc_update_bits(codec, AIC32X4_MDAC,
				    AIC32X4_MDACEN, AIC32X4_MDACEN);

		/* Switch on NADC Divider */
		snd_soc_update_bits(codec, AIC32X4_NADC,
				    AIC32X4_NADCEN, AIC32X4_NADCEN);

		/* Switch on MADC Divider */
		snd_soc_update_bits(codec, AIC32X4_MADC,
				    AIC32X4_MADCEN, AIC32X4_MADCEN);

		/* Switch on BCLK_N Divider */
		snd_soc_update_bits(codec, AIC32X4_BCLKN,
				    AIC32X4_BCLKEN, AIC32X4_BCLKEN);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		/* Switch off PLL */
		snd_soc_update_bits(codec, AIC32X4_PLLPR,
				    AIC32X4_PLLEN, 0);

		/* Switch off NDAC Divider */
		snd_soc_update_bits(codec, AIC32X4_NDAC,
				    AIC32X4_NDACEN, 0);

		/* Switch off MDAC Divider */
		snd_soc_update_bits(codec, AIC32X4_MDAC,
				    AIC32X4_MDACEN, 0);

		/* Switch off NADC Divider */
		snd_soc_update_bits(codec, AIC32X4_NADC,
				    AIC32X4_NADCEN, 0);

		/* Switch off MADC Divider */
		snd_soc_update_bits(codec, AIC32X4_MADC,
				    AIC32X4_MADCEN, 0);

		/* Switch off BCLK_N Divider */
		snd_soc_update_bits(codec, AIC32X4_BCLKN,
				    AIC32X4_BCLKEN, 0);
		break;
	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

#define AIC32X4_RATES	SNDRV_PCM_RATE_8000_48000
#define AIC32X4_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops aic32x4_ops = {
	.hw_params = aic32x4_hw_params,
	.digital_mute = aic32x4_mute,
	.set_fmt = aic32x4_set_dai_fmt,
	.set_sysclk = aic32x4_set_dai_sysclk,
};

static struct snd_soc_dai_driver aic32x4_dai = {
	.name = "tlv320aic32x4-hifi",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = AIC32X4_RATES,
		     .formats = AIC32X4_FORMATS,},
	.capture = {
		    .stream_name = "Capture",
		    .channels_min = 1,
		    .channels_max = 2,
		    .rates = AIC32X4_RATES,
		    .formats = AIC32X4_FORMATS,},
	.ops = &aic32x4_ops,
	.symmetric_rates = 1,
};

static int aic32x4_suspend(struct snd_soc_codec *codec)
{
	N_PRINT("ninglei enter aic32x4_suspend");
	aic32x4_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aic32x4_resume(struct snd_soc_codec *codec)
{
	N_PRINT("ninglei enter aic32x4_resume");
	aic32x4_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

#define I2C_USE_COUNT 2



#define CONFIG_PROC_FS

#ifdef CONFIG_PROC_FS

#define WHILE_COUNT 200

static void ti_codec_proc_write_only_one_reg(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	volatile u32 count = 0;
	struct aic32x4_priv *ti_codec = entry->private_data;
	struct snd_soc_codec *codec = ti_codec->codec;
	pr_err("%s i2c test enter while write count =%d page=%d reg = %d,please watch i2c signal in oscilloscope\n", codec->name, WHILE_COUNT, AIC32X4_RESET/128, AIC32X4_RESET%128);
	while( WHILE_COUNT > count){
		snd_soc_write(codec, AIC32X4_RESET, 0x01);
		count++;
	}
}


#if (NINGLEI_LOOP_TEST)
static int ti3254_check(struct snd_soc_codec *codec)
{
	u32 fail = 1;
	u32 temp_val = 0;
	if(NULL == codec){
		return -EINVAL;
	}
	while(fail){
		temp_val = snd_soc_read(codec, AIC32X4_IFACE2);
		snd_soc_write(codec, AIC32X4_IFACE2, 0xA5);
		if(0xA5 != snd_soc_read(codec, AIC32X4_IFACE2)){
			pr_err("codec test fial!\r\n");
			snd_soc_write(codec, AIC32X4_IFACE2, 0xA5);
		}else{
			snd_soc_write(codec, AIC32X4_IFACE2, temp_val);
			fail = 0;
		}
	}
	return 0;
}

static u32 ti3254_clock_init(struct snd_soc_codec *codec)
{
	u32 temp_val = 0;
	u32 mask = 0;
	u32 val = 0;
	#ifdef TI3254_PLL_P
	snd_soc_write(codec, AIC32X4_CLKMUX, BIT(0)|BIT(1));
	snd_soc_write(codec, AIC32X4_PLLJ, TI3254_PLL_J);
	snd_soc_write(codec, AIC32X4_PLLDMSB, (TI3254_PLL_D >> 8) & 0x3F);
	snd_soc_write(codec, AIC32X4_PLLDLSB, (TI3254_PLL_D >> 0) & 0xFF);
	snd_soc_write(codec, AIC32X4_PLLPR, (BIT(7) | (TI3254_PLL_P << 4) | (TI3254_PLL_R << 0)));
	#endif
	// BCLK = DAC_CLK / TI3254_BCLK_N
	#ifdef BCLK_FROM_DAC_MOD_CLK
	//dac_mod_clk
	mask = BIT(0)|BIT(1);
	val = BIT(0);
	temp_val = snd_soc_read(codec, 0x1D);
	temp_val &= ~mask;
	temp_val |= (val & mask);
	snd_soc_write(codec,0 + 0x1D,temp_val);
	#endif
	snd_soc_write(codec, AIC32X4_BCLKN, BIT(7) | (TI3254_BCLK_N<<0));
	return 0;
}

static u32 ti3254_DAC_FS(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AIC32X4_NDAC, (0x1 << 7)|(TI3254_NDAC&0x7F)); // 0.0x0b
	snd_soc_write(codec, AIC32X4_MDAC,(0x1 << 7)|(TI3254_MDAC & 0x7F)); // 0.0xc
	snd_soc_write(codec, AIC32X4_DOSRMSB,(TI3254_DOSR >> 8)&0x03); // 0.0x0d
	snd_soc_write(codec, AIC32X4_DOSRLSB,(TI3254_DOSR >> 0)&0xFF); // 0.0x0e
	return 0;
}

static u32 ti3254_i2s_init(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AIC32X4_IFACE1,(3<<6)|(2<<4)|(1<<3)|(1<<2)); //0.0x1b
	snd_soc_write(codec, AIC32X4_IFACE2,0x1); //0.0x1c	
	return 0;
}

static u32 ti3254_power_up(struct snd_soc_codec *codec)
{
	//software reset
	snd_soc_write(codec, AIC32X4_RESET, 0x01 << 0);
	udelay(1000);

	// 26MHz Mclk, PLL setting, CODEC_CLKIN = 92.160MHz
	ti3254_clock_init(codec);
	ti3254_DAC_FS(codec);
	ti3254_i2s_init(codec);
	snd_soc_write(codec, AIC32X4_CMMODE, (0x1 << 1)|(0x1 << 0));// 1.0x0a
	snd_soc_write(codec, AIC32X4_PWRCFG, (0x1 << 3)); // 1.0x01
	snd_soc_write(codec, AIC32X4_LDOCTL, (0x2 << 6)|(0x2 << 4) | (0x1 << 0));// 1. 0x02
	snd_soc_write(codec, AIC3254_REFPWRUP, 0x01); // 1.0x7b
	return 0;
}

static u32 ti3254_ADC_power_router(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AIC32X4_ADCSETUP, 0xC0); //0.0x51 
	snd_soc_write(codec, AIC32X4_ADCFGA, 0x00); //0.0x52
	return 0;
}

static u32 ti3254_in1_2ma_router(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AIC32X4_LMICPGAPIN, 0x80); // 1.0x34
	snd_soc_write(codec, AIC32X4_LMICPGANIN, 0x80); // 1.0x36
	snd_soc_write(codec, AIC32X4_RMICPGAPIN, 0x80); // 1.0x37
	snd_soc_write(codec, AIC32X4_RMICPGANIN, 0x80); // 1.0x39
	snd_soc_write(codec, AIC32X4_LMICPGAVOL, 0x0c); // 1.0x3b
	snd_soc_write(codec, AIC32X4_RMICPGAVOL, 0x0c); // 1.0x3c
	return 0;
}

static u32 ti3254_DAC_power_router(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AIC32X4_DACSETUP, 0xD6); // 0.0x3F
	snd_soc_write(codec, AIC32X4_DACMUTE, 0x00); // 0.0x40
	return 0;
}

static u32 ti3254_hp_power_up(struct snd_soc_codec *codec)
{
	volatile u32 temp = 0;
	snd_soc_write(codec, AIC32X4_HPLGAIN, (0 << 6)|(TI3254_HPL_GAIN << 0)); // 1.0x10 unmute
	snd_soc_write(codec, AIC32X4_HPRGAIN, (0 << 6)|(TI3254_HPR_GAIN << 0)); // 1.0x11 unmute
	
	temp = snd_soc_read(codec, AIC32X4_OUTPWRCTL);
	temp = ((0x1 << 5)|(0x1 << 4) | temp);
	snd_soc_write(codec, AIC32X4_OUTPWRCTL, temp); // 1.0x09
	return 0;
}

static u32 ti3254_ma2hp_router(struct snd_soc_codec *codec)
{
	volatile u32 temp = 0;
	snd_soc_write(codec, AIC32X4_HPLROUTE, (0x1 << 1)); // 1.0x0c
	snd_soc_write(codec, AIC32X4_HPRROUTE, (0x1 << 1)); // 1.0x0d
	temp = snd_soc_read(codec, AIC32X4_OUTPWRCTL);
	temp = temp | (0x1 << 0)|(0x1 << 1);
	snd_soc_write(codec, AIC32X4_OUTPWRCTL, temp); // 1.0x09	
	return 0;
}

static u32 ti3254_set_micbias(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AIC32X4_MICBIAS,0x68); // 1.0x33
	return 0;
}

static void ti_damp_care_regs(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct aic32x4_priv *ti_codec = entry->private_data;
	struct snd_soc_codec *codec = ti_codec->codec;
//	snd_soc_read(codec,AIC32X4_ADCSETUP);
//	snd_soc_read(codec,AIC32X4_ADCFGA);
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_LMICPGAPIN/128,AIC32X4_LMICPGAPIN%128,AIC32X4_LMICPGAPIN%128, snd_soc_read(codec,AIC32X4_LMICPGAPIN));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_RMICPGAPIN/128,AIC32X4_RMICPGAPIN%128,AIC32X4_RMICPGAPIN%128, snd_soc_read(codec,AIC32X4_RMICPGAPIN));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_LMICPGAVOL/128,AIC32X4_LMICPGAVOL%128,AIC32X4_LMICPGAVOL%128, snd_soc_read(codec,AIC32X4_LMICPGAVOL));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_RMICPGAVOL/128,AIC32X4_RMICPGAVOL%128,AIC32X4_RMICPGAVOL%128, snd_soc_read(codec,AIC32X4_RMICPGAVOL));	
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_HPLGAIN/128,AIC32X4_HPLGAIN%128,AIC32X4_HPLGAIN%128, snd_soc_read(codec,AIC32X4_HPLGAIN));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_HPRGAIN/128,AIC32X4_HPRGAIN%128,AIC32X4_HPRGAIN%128, snd_soc_read(codec,AIC32X4_HPRGAIN));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_OUTPWRCTL/128,AIC32X4_OUTPWRCTL%128,AIC32X4_OUTPWRCTL%128, snd_soc_read(codec,AIC32X4_OUTPWRCTL));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_HPLROUTE/128,AIC32X4_HPLROUTE%128,AIC32X4_HPLROUTE%128, snd_soc_read(codec,AIC32X4_HPLROUTE));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_HPRROUTE/128,AIC32X4_HPRROUTE%128,AIC32X4_HPRROUTE%128, snd_soc_read(codec,AIC32X4_HPRROUTE));
	snd_iprintf(buffer, "page=%d, fixreg=%d_0x%02x,val=0x%02x\n", AIC32X4_MICBIAS/128,AIC32X4_MICBIAS%128,AIC32X4_MICBIAS%128, snd_soc_read(codec,AIC32X4_MICBIAS));
//	snd_soc_read(codec,AIC32X4_DACSETUP);
//	snd_soc_read(codec,AIC32X4_DACMUTE);
}



static void ti_codec_proc_loop(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	struct aic32x4_priv *ti_codec = entry->private_data;
	struct snd_soc_codec *codec = ti_codec->codec;
	ti3254_check(codec);
	ti3254_power_up(codec);
	//ti3254_ADC_power_router(codec);
	ti3254_in1_2ma_router(codec);
	//ti3254_DAC_power_router(codec);
	ti3254_hp_power_up(codec);
	ti3254_ma2hp_router(codec);
	ti3254_set_micbias(codec);
}

#endif

static uint8_t g_page_ti3254 =0;
static uint8_t g_fixed_reg_ti3254 = 0;
static uint8_t g_get_val_ti3254 = 0;
static char	   g_rw_ti3254 = 0;

static void ti3254_i2c_read(struct snd_info_entry *entry,
				 struct snd_info_buffer *buffer)
{
	volatile u32 count = 0;
	struct aic32x4_priv *ti_codec = entry->private_data;
	struct snd_soc_codec *codec = ti_codec->codec;
	// todo ;only print help
	if(0 == g_rw_ti3254){
		snd_iprintf(buffer, "usage:echo r/w page fixed_reg [val] > %s\n", entry->name);
	}
	if('r' == g_rw_ti3254){
		snd_iprintf(buffer, "page =%d reg =%d_0x%x val=%d_0x%x\n", g_page_ti3254, g_fixed_reg_ti3254, g_fixed_reg_ti3254, g_get_val_ti3254, g_get_val_ti3254);
		g_rw_ti3254 = 0;
	}
	
}

static void ti3254_i2c_write(struct snd_info_entry *entry,
                                      struct snd_info_buffer *buffer)
{
	char line[64];
	int ret = 0;
    uint8_t page = 0;
	uint8_t fixed_reg = 0;
	uint32_t page_M128_reg = 0;
	uint8_t set_val = 0;
	uint8_t get_val = 0;
	char rw = 'w';
	N_PRINT_CODEC_REG_TEST("enter ti3254_i2c_write\n");
	struct aic32x4_priv *ti_codec = entry->private_data;
	struct snd_soc_codec *codec = ti_codec->codec;
	while (!snd_info_get_line(buffer, line, sizeof(line))) {
		if (sscanf(line, "%c", &rw) != 1)
			continue;
		if('r' != rw && 'w' != rw){
			pr_err("usage:r/w page fixed_reg [val]\n");
			g_rw_ti3254 = 0;
			return 0;
		}
		if('r' == rw){
			sscanf(line, "%c %d 0x%x", &rw, &page, &fixed_reg);
			N_PRINT_CODEC_REG_TEST("sscanf:rw = %c, page = %d, fixed_reg = 0x%x\n", rw, page, fixed_reg);
			page_M128_reg = page * 128 + fixed_reg;
			get_val = snd_soc_read(codec, page_M128_reg);
			N_PRINT_CODEC_REG_TEST("r page =%d reg =%d_0x%x val=%d_0x%x\n", page, fixed_reg,fixed_reg,get_val,get_val);
			g_rw_ti3254 = rw;
			g_page_ti3254 = page;
			g_fixed_reg_ti3254 = fixed_reg;
			g_get_val_ti3254 = get_val;
		}else if('w' == rw){
			sscanf(line, "%c %d 0x%x 0x%x", &rw, &page, &fixed_reg, &set_val);
			g_rw_ti3254 = rw;
			N_PRINT_CODEC_REG_TEST("sscanf:rw= %c,page = %d, fixed_reg= 0x%x, set_val = 0x%x\n",rw, page, fixed_reg, set_val);
			page_M128_reg = page * 128 + fixed_reg;
			snd_soc_write(codec, page_M128_reg, set_val);		
			N_PRINT_CODEC_REG_TEST("w page =%d reg =%d_0x%x val=%d_0x%x\n", page, fixed_reg, fixed_reg, set_val, set_val);
		}else{
			g_rw_ti3254 = 0;
			pr_err("%s unknow command\n", __func__);
		} 
	}
}


static void ti3254_codec_proc_init_ti(	struct aic32x4_priv *ti_codec)
{
	struct snd_info_entry *entry;
	struct snd_soc_codec *codec = ti_codec->codec;

	if (!snd_card_proc_new(codec->card->snd_card, "test_i2c_write_only_one_reg", &entry)){
		snd_info_set_text_ops(entry, ti_codec, ti_codec_proc_write_only_one_reg);
	}
	
	if (!snd_card_proc_new(codec->card->snd_card, "ti3254_i2c_test", &entry)){
		snd_info_set_text_ops(entry, ti_codec, ti3254_i2c_read);
		entry->c.text.write = ti3254_i2c_write;
		entry->mode |= S_IWUSR;
	}

	#if	(NINGLEI_LOOP_TEST)
	if (!snd_card_proc_new(codec->card->snd_card, "loop_test", &entry)){
		snd_info_set_text_ops(entry, ti_codec, ti_codec_proc_loop);
	}
	if (!snd_card_proc_new(codec->card->snd_card, "damp_care_regs", &entry)){
		snd_info_set_text_ops(entry, ti_codec, ti_damp_care_regs);
	}
	#endif	
}

#else /* !CONFIG_PROC_FS */
static inline void ti3254_codec_proc_init_ti(struct aic32x4_priv *ti_codec)
{
}
#endif

#if (NINGLEI_CODEC_REG_TEST)
static struct kobject kobj_codec;
typedef struct codec_reg_test_attribute
{
    struct attribute attr;
	struct snd_soc_codec *codec;
    ssize_t (*show)(char *buf);
    ssize_t (*store)(const char *buf, size_t count, struct snd_soc_codec *codec);
}codec_reg_test_attribute_t;

static uint8_t g_page =0;
static uint8_t g_fixed_reg = 0;
static uint8_t g_set_val = 0;
static uint8_t g_get_val = 0;
static char	   g_rw = 'r';

static ssize_t codec_reg_test_show(char *buf)
{
	sprintf(buf, "page =%d reg =%d val=%d\n", g_page, g_fixed_reg, g_get_val);
    return strlen(buf);
}
static ssize_t codec_reg_test_store(const char *buf, size_t count, struct snd_soc_codec *codec)
{
	int ret = 0;
    uint8_t page = 0;
	uint8_t fixed_reg = 0;
	uint32_t c_reg = 0;
	uint8_t set_val = 0;
	uint8_t get_val = 0;
	char rw = 'r';
	N_PRINT_CODEC_REG_TEST("enter codec_reg_test_store\n");
	sscanf(buf, "%c", &rw);
	if('r' != rw && 'w' != rw){
		pr_err("usage:r/w page fixed_reg [val]\n");
		return 0;
	}
	g_rw = rw;
	if('r' == rw){
		sscanf(buf, "%c %d 0x%x", &rw, &page, &fixed_reg);
		N_PRINT_CODEC_REG_TEST("sscanf:rw = %c, page = %d, fixed_reg = 0x%x\n", rw, page, fixed_reg);
		g_page = page;
		g_fixed_reg = fixed_reg;
		c_reg = page * 128 + fixed_reg;
		get_val = snd_soc_read(codec, c_reg);
		g_get_val = get_val;
		N_PRINT_CODEC_REG_TEST("r page =%d reg =%d_0x%x val=%d_0x%x\n", page, fixed_reg,fixed_reg,get_val,get_val);
	}else if('w' == rw){
		sscanf(buf, "%c %d 0x%x 0x%x", &rw, &page, &fixed_reg, &set_val);
		N_PRINT_CODEC_REG_TEST("sscanf:rw= %c,page = %d, fixed_reg= 0x%x, set_val = 0x%x\n",rw, page, fixed_reg, set_val);
		g_page = page;
		g_fixed_reg = fixed_reg;
		c_reg = page * 128 + fixed_reg;
		g_set_val = set_val;
		snd_soc_write(codec, c_reg, set_val);		
		N_PRINT_CODEC_REG_TEST("w page =%d reg =%d_0x%x val=%d_0x%x\n", page, fixed_reg, fixed_reg, set_val, set_val);
	}else{
		pr_err("%s unknow command\n", __func__);
	} 
    return strlen(buf);
}


static codec_reg_test_attribute_t codec_attr_test = {
	.attr = {
		.name = "codec_reg_test",
		.mode = S_IRUGO |S_IWUGO,
	},
	.show = codec_reg_test_show,
	.store = codec_reg_test_store,
};

static struct attribute *codec_def_attrs[] = {
        &codec_attr_test.attr,
        NULL,
};
static ssize_t kobj_codec_test_show(struct kobject *kobject, struct attribute *attr,char *buf)
{
	ssize_t len = 0;
	codec_reg_test_attribute_t *a  = container_of(attr, codec_reg_test_attribute_t, attr);

	if (a->show)
	{
		len = a->show(buf);
	}

	return len;

}
static ssize_t kobj_codec_test_store(struct kobject *kobject,struct attribute *attr,const char *buf, size_t count)
{
	ssize_t len = 0;
	codec_reg_test_attribute_t *a  = container_of(attr, codec_reg_test_attribute_t, attr);

	if (a->store)
	{
		len = a->store(buf, count, a->codec);
	}

	return len;
}
struct sysfs_ops obj_codec_test_sysops =
{
        .show = kobj_codec_test_show,
        .store = kobj_codec_test_store,
};
static void obj_test_release(struct kobject *kobject)
{
        printk("vbc_test: release .\n");
}

static struct kobj_type ktype_codec =
{
        .release = obj_test_release,
        .sysfs_ops=&obj_codec_test_sysops,
        .default_attrs=codec_def_attrs,
};
#endif

static int aic32x4_probe(struct snd_soc_codec *codec)
{
	struct aic32x4_priv *aic32x4 = snd_soc_codec_get_drvdata(codec);
	u32 tmp_reg;
	int ret;
	
	codec->hw_write = (hw_write_t) i2c_master_send;
	codec->control_data = aic32x4->control_data;

	if (aic32x4->rstn_gpio >= 0) {
		pr_debug("ninglei aic32x4->rstn_gpio 0x%x", aic32x4->rstn_gpio);
		ret = devm_gpio_request_one(codec->dev, aic32x4->rstn_gpio,
				GPIOF_OUT_INIT_LOW, "tlv320aic32x4 rstn");
		if (ret != 0)
			return ret;
		ndelay(10);
		gpio_set_value(aic32x4->rstn_gpio, 1);
	}
	
	aic32x4->codec = codec;
	ti3254_codec_proc_init_ti(aic32x4);
	#if (NINGLEI_CODEC_REG_TEST)
	codec_attr_test.codec = codec;
	kobject_init_and_add(&kobj_codec,&ktype_codec,NULL,"codec_reg_test");
	
	#endif
	ti3254_check(codec);
	snd_soc_write(codec, AIC32X4_RESET, 0x01);
	/*clock init*/
//	ti3254_clock_init(codec);
	/* Power platform configuration */
	pr_debug("ninglei aic32x4_probe aic32x4->power_cfg=0x%x", aic32x4->power_cfg);
	if (aic32x4->power_cfg & AIC32X4_PWR_MICBIAS_2075_LDOIN) {
		snd_soc_write(codec, AIC32X4_MICBIAS, AIC32X4_MICBIAS_LDOIN |
						      AIC32X4_MICBIAS_2075V);
	}
	if (aic32x4->power_cfg & AIC32X4_PWR_AVDD_DVDD_WEAK_DISABLE) {
		//(0x1 << 3)); // 1.0x01
		snd_soc_write(codec, AIC32X4_PWRCFG, AIC32X4_AVDDWEAKDISABLE);
	}

	tmp_reg = (aic32x4->power_cfg & AIC32X4_PWR_AIC32X4_LDO_ENABLE) ?
			AIC32X4_LDOCTLEN : 0;
	//(0x2 << 6)|(0x2 << 4) | (0x1 << 0));// 1. 0x02
	tmp_reg |= (0x2 << 6)|(0x2 << 4);
	snd_soc_write(codec, AIC32X4_LDOCTL, tmp_reg);

	tmp_reg = snd_soc_read(codec, AIC32X4_CMMODE);
	if (aic32x4->power_cfg & AIC32X4_PWR_CMMODE_LDOIN_RANGE_18_36) {
		tmp_reg |= AIC32X4_LDOIN_18_36;
	}
	if (aic32x4->power_cfg & AIC32X4_PWR_CMMODE_HP_LDOIN_POWERED) {
		tmp_reg |= AIC32X4_LDOIN2HP;
	}
	//(0x1 << 1)|(0x1 << 0))
	snd_soc_write(codec, AIC32X4_CMMODE, tmp_reg);
	
	snd_soc_write(codec, AIC3254_REFPWRUP, 0x01); // 1.0x7b

	/* Do DACs need to be swapped? */
	
//	01: Right DAC data Right Channel Audio Interface Data
//	01: Left DAC data Left Channel Audio Interface Data
	if (aic32x4->swapdacs) {// 0
		snd_soc_write(codec, AIC32X4_DACSETUP, AIC32X4_LDAC2RCHN | AIC32X4_RDAC2LCHN);
	} else {
		snd_soc_write(codec, AIC32X4_DACSETUP, AIC32X4_LDAC2LCHN | AIC32X4_RDAC2RCHN);
	}

	/* Mic PGA routing */
	if (aic32x4->micpga_routing & AIC32X4_MICPGA_ROUTE_LMIC_IN2R_10K) {
		snd_soc_write(codec, AIC32X4_LMICPGANIN, AIC32X4_LMICPGANIN_IN2R_10K);
	}
	if (aic32x4->micpga_routing & AIC32X4_MICPGA_ROUTE_RMIC_IN1L_10K) {
		snd_soc_write(codec, AIC32X4_RMICPGANIN, AIC32X4_RMICPGANIN_IN1L_10K);
	}
	//0x80
	if (aic32x4->micpga_routing & AIC32X4_MICPGA_ROUTE_LMIC_CM_20K) {
		snd_soc_write(codec, AIC32X4_LMICPGANIN, AIC32X4_LMICPGANIN_CM_20K);
	}
	// 0x80
	if (aic32x4->micpga_routing & AIC32X4_MICPGA_ROUTE_RMIC_CM_20K) {
		snd_soc_write(codec, AIC32X4_RMICPGANIN, AIC32X4_RMICPGANIN_CM_20K);
	}

	snd_soc_add_codec_controls(codec, aic32x4_snd_controls,
			     ARRAY_SIZE(aic32x4_snd_controls));
	aic32x4_add_widgets(codec);

	/*
	 * Workaround: for an unknown reason, the ADC needs to be powered up
	 * and down for the first capture to work properly. It seems related to
	 * a HW BUG or some kind of behavior not documented in the datasheet.
	 */
	tmp_reg = snd_soc_read(codec, AIC32X4_ADCSETUP);
	snd_soc_write(codec, AIC32X4_ADCSETUP, tmp_reg |
				AIC32X4_LADC_EN | AIC32X4_RADC_EN);
	snd_soc_write(codec, AIC32X4_ADCSETUP, tmp_reg);
	return 0;
}

static int aic32x4_remove(struct snd_soc_codec *codec)
{
	N_PRINT("ninglei enter aic32x4_remove\n");
	aic32x4_set_bias_level(codec, SND_SOC_BIAS_OFF);
	#if (NINGLEI_CODEC_REG_TEST)
	kobject_del(&kobj_codec);
	#endif
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_aic32x4 = {
	.read = aic32x4_read,
	.write = aic32x4_write,
	.probe = aic32x4_probe,
	.remove = aic32x4_remove,
	.suspend = aic32x4_suspend,
	.resume = aic32x4_resume,
	.set_bias_level = aic32x4_set_bias_level,
};
/*
* return value : 0 if sucess
*/
static int ti3254_parse_dt(struct aic32x4_priv *info, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 val_u32_arr[1] = {0};
	if (!of_property_read_u32_array(np, "power_cfg", &val_u32_arr[0], 1)) {
		info->power_cfg= val_u32_arr[0];
	
	} else {
		pr_err("%s, ERR:Must give me the power_cfg!\n", __func__);
		return -EINVAL;
	}
	
	memset(val_u32_arr, 0x0, sizeof(val_u32_arr));
	if (!of_property_read_u32_array(np, "swapdacs", &val_u32_arr[0], 1)) {
		info->swapdacs= val_u32_arr[0];
	
	} else {
		pr_err("%s, ERR:Must give me the swapdacs !\n", __func__);
		return -EINVAL;
	}
	
	memset(val_u32_arr, 0x0, sizeof(val_u32_arr));
	if (!of_property_read_u32_array(np, "micpga_routing", &val_u32_arr[0], 1)) {
		info->micpga_routing= val_u32_arr[0];
	} else {
		pr_err("%s, ERR:Must give me the micpga_routing !\n", __func__);
		return -EINVAL;
	}
	
	memset(val_u32_arr, 0x0, sizeof(val_u32_arr));
	if (!of_property_read_u32_array(np, "rstn_gpio", &val_u32_arr[0], 1)) {
		if(0xffffffff == val_u32_arr[0]){
			info->rstn_gpio= -1;
		}else{
			info->rstn_gpio= (int)val_u32_arr[0];
		}
	} else {
		pr_err("%s, ERR:Must give me the rstn_gpio !\n", __func__);
		return -EINVAL;
	}

	memset(val_u32_arr, 0x0, sizeof(val_u32_arr));
	if (!of_property_read_u32_array(np, "sysclk", &val_u32_arr[0], 1)) {
			info->sysclk= val_u32_arr[0];
	} else {
		pr_err("%s, ERR:Must give me the sysclk !\n", __func__);
		return -EINVAL;
	}
	return 0;
}


static int aic32x4_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct aic32x4_priv *aic32x4;
	int ret;
	N_PRINT("ninglei enter aic32x4_i2c_probe\n");
	aic32x4 = devm_kzalloc(&i2c->dev, sizeof(struct aic32x4_priv),
			       GFP_KERNEL);
	if (aic32x4 == NULL)
		return -ENOMEM;

	aic32x4->control_data = i2c;
	i2c_set_clientdata(i2c, aic32x4);

	ret = ti3254_parse_dt(aic32x4, &i2c->dev);
	
	if (0 == ret) {
		pr_err("parse aic32x4 from dts\n");
		pr_err("sysclk=%u power_cfg=0x%x swapdacs=0x%x micpga_routing=0x%x rstn_gpio=%d\n", aic32x4->sysclk, aic32x4->power_cfg, aic32x4->swapdacs,
			aic32x4->micpga_routing, aic32x4->rstn_gpio);
	} else {
		dev_dbg(&i2c->dev, "pdata is null");
		aic32x4->power_cfg = 0;
		aic32x4->swapdacs = false;
		aic32x4->micpga_routing = 0;
		aic32x4->rstn_gpio = -1;
	}

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_aic32x4, &aic32x4_dai, 1);
	return ret;
}

static int aic32x4_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id aic32x4_i2c_id[] = {
	{ "tlv320aic32x4_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aic32x4_i2c_id);

static const struct of_device_id aic32x4_of_id[] = {
	{ .compatible = "ti,tlv320aic32x4_i2c", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, aic32x4_of_id);

static struct i2c_driver aic32x4_i2c_driver = {
	.driver = {
		.name = "tlv320aic32x4",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aic32x4_of_id),
	},
	.probe =    aic32x4_i2c_probe,
	.remove =   aic32x4_i2c_remove,
	.id_table = aic32x4_i2c_id,
};
module_i2c_driver(aic32x4_i2c_driver);

MODULE_DESCRIPTION("ASoC tlv320aic32x4 codec driver");
MODULE_AUTHOR("Javier Martin <javier.martin@vista-silicon.com>");
MODULE_LICENSE("GPL");
