// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI I2S Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt6989-afe-common.h"
#include "mt6989-interconnection.h"

enum AUD_TX_LCH_RPT {
	AUD_TX_LCH_RPT_NO_REPEAT = 0,
	AUD_TX_LCH_RPT_REPEAT = 1
};

enum AUD_VBT_16K_MODE {
	AUD_VBT_16K_MODE_DISABLE = 0,
	AUD_VBT_16K_MODE_ENABLE = 1
};

enum AUD_EXT_MODEM {
	AUD_EXT_MODEM_SELECT_INTERNAL = 0,
	AUD_EXT_MODEM_SELECT_EXTERNAL = 1
};

enum AUD_PCM_SYNC_TYPE {
	/* bck sync length = 1 */
	AUD_PCM_ONE_BCK_CYCLE_SYNC = 0,
	/* bck sync length = PCM_INTF_CON1[9:13] */
	AUD_PCM_EXTENDED_BCK_CYCLE_SYNC = 1
};

enum AUD_BT_MODE {
	AUD_BT_MODE_DUAL_MIC_ON_TX = 0,
	AUD_BT_MODE_SINGLE_MIC_ON_TX = 1
};

enum AUD_PCM_AFIFO_SRC {
	/* slave mode & external modem uses different crystal */
	AUD_PCM_AFIFO_ASRC = 0,
	/* slave mode & external modem uses the same crystal */
	AUD_PCM_AFIFO_AFIFO = 1
};

enum AUD_PCM_CLOCK_SOURCE {
	AUD_PCM_CLOCK_MASTER_MODE = 0,
	AUD_PCM_CLOCK_SLAVE_MODE = 1
};

enum AUD_PCM_WLEN {
	AUD_PCM_WLEN_PCM_32_BCK_CYCLES = 0,
	AUD_PCM_WLEN_PCM_64_BCK_CYCLES = 1
};

enum AUD_PCM_MODE {
	AUD_PCM_MODE_PCM_MODE_8K = 0,
	AUD_PCM_MODE_PCM_MODE_16K = 1,
	AUD_PCM_MODE_PCM_MODE_32K = 2,
	AUD_PCM_MODE_PCM_MODE_48K = 3,
};

enum AUD_PCM_FMT {
	AUD_PCM_FMT_I2S = 0,
	AUD_PCM_FMT_EIAJ = 1,
	AUD_PCM_FMT_PCM_MODE_A = 2,
	AUD_PCM_FMT_PCM_MODE_B = 3
};

enum AUD_BCLK_OUT_INV {
	AUD_BCLK_OUT_INV_NO_INVERSE = 0,
	AUD_BCLK_OUT_INV_INVERSE = 1
};

enum AUD_PCM_EN {
	AUD_PCM_EN_DISABLE = 0,
	AUD_PCM_EN_ENABLE = 1
};

enum AUD_PCM1_1x_EN_Domain {
	HOPPING_26M = 0,
	APLL = 1,
	SLAVE = 6,
};

enum AUD_PCM1_1x_EN_SLAVE_MODE {
	PCM0_SLAVE_1x_EN = 1,
	PCM1_SLAVE_1x_EN = 2,
};

enum {
	PCM_8K = 0,
	PCM_16K = 4,
	PCM_32K = 8,
	PCM_48K = 10
};

static unsigned int pcm_1x_rate_transform(struct device *dev,
				       unsigned int rate)
{
	switch (rate) {
	case 8000:
		return PCM_8K;
	case 16000:
		return PCM_16K;
	case 32000:
		return PCM_32K;
	case 48000:
		return PCM_48K;
	default:
		dev_info(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, PCM_32K);
		return PCM_32K;
	}
}

/* dai component */
static const struct snd_kcontrol_new mtk_pcm_0_playback_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN096_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN096_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN096_1,
				    I_DL_24CH_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_0_playback_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN097_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN097_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN097_1,
				    I_DL_24CH_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_0_playback_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN099_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN099_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN099_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN099_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN099_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN099_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN099_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN099_1,
				    I_DL_24CH_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_1_playback_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN102_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN102_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN102_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN102_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN102_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN102_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN102_1,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN102_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN102_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN102_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN102_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH1", AFE_CONN102_5,
				    I_I2SIN6_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_1_playback_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN103_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN103_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN103_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN103_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN103_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN103_0,
				    I_ADDA_UL_CH6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN103_1,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN103_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN103_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN103_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN103_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN6_CH2", AFE_CONN103_5,
				    I_I2SIN6_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_1_playback_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN104_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN104_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN104_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH4", AFE_CONN104_0,
				    I_ADDA_UL_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH5", AFE_CONN104_0,
				    I_ADDA_UL_CH5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH6", AFE_CONN104_0,
				    I_ADDA_UL_CH6, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_1_playback_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH1", AFE_CONN105_4,
				    I_I2SIN1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN105_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN105_1,
				    I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH1", AFE_CONN105_4,
				    I_I2SIN0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN105_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH1", AFE_CONN105_4,
				    I_I2SIN4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN105_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN105_1,
				    I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN105_2,
				    I_DL24_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN105_6,
				    I_SRC_2_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN105_6,
				    I_SRC_1_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_pcm_1_playback_ch5_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN1_CH2", AFE_CONN106_4,
				    I_I2SIN1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN106_1,
				    I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN0_CH2", AFE_CONN106_4,
				    I_I2SIN0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2SIN4_CH2", AFE_CONN106_4,
				    I_I2SIN4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN106_1,
				    I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN106_2,
				    I_DL24_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN106_6,
				    I_SRC_2_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN106_6,
				    I_SRC_1_OUT_CH2, 1, 0),
};

static int mtk_pcm_en_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);
	return 0;
}

static const struct snd_soc_dapm_widget mtk_dai_pcm_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("PCM_0_PB_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_0_playback_ch1_mix,
			   ARRAY_SIZE(mtk_pcm_0_playback_ch1_mix)),
	SND_SOC_DAPM_MIXER("PCM_0_PB_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_0_playback_ch2_mix,
			   ARRAY_SIZE(mtk_pcm_0_playback_ch2_mix)),
	SND_SOC_DAPM_MIXER("PCM_0_PB_CH4", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_0_playback_ch4_mix,
			   ARRAY_SIZE(mtk_pcm_0_playback_ch4_mix)),
	SND_SOC_DAPM_MIXER("PCM_1_PB_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_1_playback_ch1_mix,
			   ARRAY_SIZE(mtk_pcm_1_playback_ch1_mix)),
	SND_SOC_DAPM_MIXER("PCM_1_PB_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_1_playback_ch2_mix,
			   ARRAY_SIZE(mtk_pcm_1_playback_ch2_mix)),
	SND_SOC_DAPM_MIXER("PCM_1_PB_CH3", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_1_playback_ch3_mix,
			   ARRAY_SIZE(mtk_pcm_1_playback_ch3_mix)),
	SND_SOC_DAPM_MIXER("PCM_1_PB_CH4", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_1_playback_ch4_mix,
			   ARRAY_SIZE(mtk_pcm_1_playback_ch4_mix)),
	SND_SOC_DAPM_MIXER("PCM_1_PB_CH5", SND_SOC_NOPM, 0, 0,
			   mtk_pcm_1_playback_ch5_mix,
			   ARRAY_SIZE(mtk_pcm_1_playback_ch5_mix)),

	SND_SOC_DAPM_SUPPLY("PCM_0_EN",
			    AFE_PCM0_INTF_CON0, PCM0_EN_SFT, 0,
			    mtk_pcm_en_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("PCM_1_EN",
			    AFE_PCM1_INTF_CON0, PCM1_EN_SFT, 0,
			    mtk_pcm_en_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("MD1_TO_AFE"),
	SND_SOC_DAPM_INPUT("MD2_TO_AFE"),
	SND_SOC_DAPM_OUTPUT("AFE_TO_MD1"),
	SND_SOC_DAPM_OUTPUT("AFE_TO_MD2"),
};

static const struct snd_soc_dapm_route mtk_dai_pcm_routes[] = {
	{"PCM 0 Playback", NULL, "PCM_0_PB_CH1"},
	{"PCM 0 Playback", NULL, "PCM_0_PB_CH2"},
	{"PCM 0 Playback", NULL, "PCM_0_PB_CH4"},
	{"PCM 1 Playback", NULL, "PCM_1_PB_CH1"},
	{"PCM 1 Playback", NULL, "PCM_1_PB_CH2"},
	{"PCM 1 Playback", NULL, "PCM_1_PB_CH3"},
	{"PCM 1 Playback", NULL, "PCM_1_PB_CH4"},
	{"PCM 1 Playback", NULL, "PCM_1_PB_CH5"},

	{"PCM 0 Playback", NULL, "PCM_0_EN"},
	{"PCM 1 Playback", NULL, "PCM_1_EN"},
	{"PCM 0 Capture", NULL, "PCM_0_EN"},
	{"PCM 1 Capture", NULL, "PCM_1_EN"},

	{"AFE_TO_MD1", NULL, "PCM 1 Playback"},
	{"AFE_TO_MD2", NULL, "PCM 0 Playback"},
	{"PCM 1 Capture", NULL, "MD1_TO_AFE"},
	{"PCM 0 Capture", NULL, "MD2_TO_AFE"},

	{"PCM_0_PB_CH1", "DL2_CH1", "DL2"},
	{"PCM_0_PB_CH2", "DL2_CH2", "DL2"},
	{"PCM_0_PB_CH4", "DL0_CH1", "DL0"},
	{"PCM_1_PB_CH1", "DL2_CH1", "DL2"},
	{"PCM_1_PB_CH2", "DL2_CH2", "DL2"},
	{"PCM_1_PB_CH1", "HW_SRC_0_OUT_CH1", "HW_SRC_0_Out"},
	{"PCM_1_PB_CH2", "HW_SRC_0_OUT_CH2", "HW_SRC_0_Out"},
	{"PCM_1_PB_CH4", "HW_SRC_1_OUT_CH1", "HW_SRC_1_Out"},
	{"PCM_1_PB_CH5", "HW_SRC_1_OUT_CH2", "HW_SRC_1_Out"},
	{"PCM_1_PB_CH4", "DL0_CH1", "DL0"},
	{"PCM_1_PB_CH4", "DL24_CH1", "DL24"},

	{"PCM_0_PB_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"PCM_0_PB_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"PCM_0_PB_CH4", "DL_24CH_CH1", "DL_24CH"},
	{"PCM_1_PB_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"PCM_1_PB_CH2", "DL_24CH_CH2", "DL_24CH"},
	{"PCM_1_PB_CH4", "DL_24CH_CH1", "DL_24CH"},

	{"PCM_0_PB_CH4", "I2SIN0_CH1", "I2SIN0"},
	{"PCM_0_PB_CH4", "I2SIN4_CH1", "I2SIN4"},
	{"PCM_1_PB_CH4", "I2SIN1_CH1", "I2SIN1"},
	{"PCM_1_PB_CH4", "I2SIN4_CH1", "I2SIN4"},
	{"PCM_1_PB_CH4", "HW_SRC_2_OUT_CH1", "HW_SRC_2_Out"},
	{"PCM_1_PB_CH5", "DL0_CH2", "DL0"},
	{"PCM_1_PB_CH5", "DL24_CH2", "DL24"},
	{"PCM_1_PB_CH5", "DL_24CH_CH2", "DL_24CH"},
	{"PCM_1_PB_CH5", "I2SIN0_CH2", "I2SIN0"},
	{"PCM_1_PB_CH5", "I2SIN1_CH2", "I2SIN1"},
	{"PCM_1_PB_CH5", "I2SIN4_CH2", "I2SIN4"},
	{"PCM_1_PB_CH1", "I2SIN0_CH1", "I2SIN0"},
	{"PCM_1_PB_CH2", "I2SIN0_CH2", "I2SIN0"},
	{"PCM_1_PB_CH1", "I2SIN4_CH1", "I2SIN4"},
	{"PCM_1_PB_CH2", "I2SIN4_CH2", "I2SIN4"},
	{"PCM_1_PB_CH1", "I2SIN6_CH1", "I2SIN6"},
	{"PCM_1_PB_CH2", "I2SIN6_CH2", "I2SIN6"},
	{"PCM_1_PB_CH5", "HW_SRC_2_OUT_CH2", "HW_SRC_2_Out"},
};

/* dai ops */
static int mtk_dai_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int rate = params_rate(params);
	unsigned int rate_reg = mt6989_rate_transform(afe->dev, rate, dai->id);
	unsigned int x_rate_reg = pcm_1x_rate_transform(afe->dev, rate);
	unsigned int pcm_con0 = 0;
	unsigned int pcm_con1 = 0;

	dev_info(afe->dev, "%s(), id %d, stream %d, rate %d, rate_reg %d, widget active p %d, c %d\n",
		 __func__,
		 dai->id,
		 substream->stream,
		 rate,
		 rate_reg,
		 dai->playback_widget->active,
		 dai->capture_widget->active);

	if (dai->playback_widget->active || dai->capture_widget->active)
		return 0;

	switch (dai->id) {
	case MT6989_DAI_PCM_0:
		pcm_con0 |= AUD_BCLK_OUT_INV_NO_INVERSE << PCM0_BCLK_OUT_INV_SFT;
		pcm_con0 |= AUD_TX_LCH_RPT_NO_REPEAT << PCM0_TX_LCH_RPT_SFT;
		pcm_con0 |= AUD_VBT_16K_MODE_DISABLE << PCM0_VBT_16K_MODE_SFT;
		pcm_con1 |= AUD_EXT_MODEM_SELECT_INTERNAL << PCM0_EXT_MODEM_SFT;
		pcm_con0 |= 0 << PCM0_SYNC_LENGTH_SFT;
		pcm_con0 |= AUD_PCM_ONE_BCK_CYCLE_SYNC << PCM0_SYNC_TYPE_SFT;
		pcm_con1 |= AUD_BT_MODE_DUAL_MIC_ON_TX << PCM0_BT_MODE_SFT;
		pcm_con0 |= AUD_PCM_AFIFO_AFIFO << PCM0_BYP_ASRC_SFT;
		/* no pcm0 on IPM2.0 only for lpbk debug */
		pcm_con0 |= AUD_PCM_CLOCK_MASTER_MODE << PCM0_SLAVE_SFT;
		pcm_con0 |= rate_reg << PCM0_MODE_SFT;
		pcm_con0 |= AUD_PCM_FMT_PCM_MODE_B << PCM0_FMT_SFT;

		regmap_update_bits(afe->regmap, AFE_PCM0_INTF_CON0,
				   0xfffffffe, pcm_con0);
		regmap_update_bits(afe->regmap, AFE_PCM0_INTF_CON1,
				   0xffffffff, pcm_con1);
		break;
	case MT6989_DAI_PCM_1:
		pcm_con0 |= AUD_TX_LCH_RPT_NO_REPEAT << PCM1_TX_LCH_RPT_SFT;
		pcm_con0 |= AUD_VBT_16K_MODE_DISABLE << PCM1_VBT_16K_MODE_SFT;
		pcm_con0 |= AUD_BT_MODE_DUAL_MIC_ON_TX << PCM1_BT_MODE_SFT;
		pcm_con0 |= AUD_PCM_AFIFO_AFIFO << PCM1_AFIFO_SFT;
		pcm_con0 |= AUD_PCM_WLEN_PCM_32_BCK_CYCLES << PCM1_WLEN_SFT;
		pcm_con0 |= rate_reg << PCM1_MODE_SFT;
		pcm_con0 |= AUD_PCM_FMT_PCM_MODE_B << PCM1_FMT_SFT;

		regmap_update_bits(afe->regmap, AFE_PCM1_INTF_CON0,
				   0xfffffffe, pcm_con0);

		pcm_con1 |= HOPPING_26M << PCM1_1X_EN_DOMAIN_SFT;
		pcm_con1 |= x_rate_reg << PCM1_1X_EN_MODE_SFT;

		regmap_update_bits(afe->regmap, AFE_PCM1_INTF_CON1,
				   0x3fc0000, pcm_con1);


		break;
	default:
		dev_info(afe->dev, "%s(), id %d not support\n",
			 __func__, dai->id);
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_pcm_ops = {
	.hw_params = mtk_dai_pcm_hw_params,
};

/* dai driver */
#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000 |\
		       SNDRV_PCM_RATE_16000 |\
		       SNDRV_PCM_RATE_32000 |\
		       SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_pcm_driver[] = {
	{
		.name = "PCM 0",
		.id = MT6989_DAI_PCM_0,
		.playback = {
			.stream_name = "PCM 0 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "PCM 0 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_dai_pcm_ops,
		.symmetric_rate = 1,
		.symmetric_sample_bits = 1,
	},
	{
		.name = "PCM 1",
		.id = MT6989_DAI_PCM_1,
		.playback = {
			.stream_name = "PCM 1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.capture = {
			.stream_name = "PCM 1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mtk_dai_pcm_ops,
		.symmetric_rate = 1,
		.symmetric_sample_bits = 1,
	},
};

int mt6989_dai_pcm_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_pcm_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_pcm_driver);

	dai->dapm_widgets = mtk_dai_pcm_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_pcm_widgets);
	dai->dapm_routes = mtk_dai_pcm_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_pcm_routes);
	return 0;
}

