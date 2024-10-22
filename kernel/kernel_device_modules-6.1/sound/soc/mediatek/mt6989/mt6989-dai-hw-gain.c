// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI HW Gain Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/regmap.h>
#include "mt6989-afe-common.h"
#include "mt6989-interconnection.h"

#define HW_GAIN_0_EN_W_NAME "HW GAIN 0 Enable"
#define HW_GAIN_1_EN_W_NAME "HW GAIN 1 Enable"
#define HW_GAIN_2_EN_W_NAME "HW GAIN 2 Enable"
#define HW_GAIN_3_EN_W_NAME "HW GAIN 3 Enable"


/* dai component */
static const struct snd_kcontrol_new mtk_hw_gain0_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN004_0,
				    I_CONNSYS_I2S_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain0_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN005_0,
				    I_CONNSYS_I2S_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain1_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN006_0,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain1_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN007_0,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain2_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN008_0,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain2_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN009_0,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain3_in_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN010_0,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_hw_gain3_in_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN011_0,
				    I_ADDA_UL_CH2, 1, 0),
};

static int mtk_hw_gain_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int gain_cur_l = 0;
	unsigned int gain_cur_r = 0;
	unsigned int gain_con1_l = 0;
	unsigned int gain_con1_r = 0;

	dev_info(cmpnt->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (strcmp(w->name, HW_GAIN_0_EN_W_NAME) == 0) {
			gain_cur_l = AFE_GAIN0_CUR_L;
			gain_cur_r = AFE_GAIN0_CUR_R;
			gain_con1_l = AFE_GAIN0_CON1_L;
			gain_con1_r = AFE_GAIN0_CON1_R;
		} else if (strcmp(w->name, HW_GAIN_1_EN_W_NAME) == 0) {
			gain_cur_l = AFE_GAIN1_CUR_L;
			gain_cur_r = AFE_GAIN1_CUR_R;
			gain_con1_l = AFE_GAIN1_CON1_L;
			gain_con1_r = AFE_GAIN1_CON1_R;
		} else if (strcmp(w->name, HW_GAIN_2_EN_W_NAME) == 0) {
			gain_cur_l = AFE_GAIN2_CUR_L;
			gain_cur_r = AFE_GAIN2_CUR_R;
			gain_con1_l = AFE_GAIN2_CON1_L;
			gain_con1_r = AFE_GAIN2_CON1_R;
		} else if (strcmp(w->name, HW_GAIN_3_EN_W_NAME) == 0) {
			gain_cur_l = AFE_GAIN3_CUR_L;
			gain_cur_r = AFE_GAIN3_CUR_R;
			gain_con1_l = AFE_GAIN3_CON1_L;
			gain_con1_r = AFE_GAIN3_CON1_R;
		}

		/* let hw gain ramp up, set cur gain to 0 */
		regmap_update_bits(afe->regmap,
				   gain_cur_l,
				   AFE_GAIN_CUR_L_MASK_SFT,
				   0);
		regmap_update_bits(afe->regmap,
				   gain_cur_r,
				   AFE_GAIN_CUR_R_MASK_SFT,
				   0);

		/* set target gain to 0 */
		regmap_update_bits(afe->regmap,
				   gain_con1_l,
				   GAIN_TARGET_L_MASK_SFT,
				   0x0);
		regmap_update_bits(afe->regmap,
				   gain_con1_r,
				   GAIN_TARGET_R_MASK_SFT,
				   0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt6989_gain0_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int value = 0x0;

	regmap_read(afe->regmap, AFE_GAIN0_CUR_L, &value);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mt6989_gain0_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	unsigned int hw_gain_target = 0x0;
	int i = 0, index = 0;

	hw_gain_target = ucontrol->value.integer.value[0];
	dev_info(afe->dev, "%s(), hw_gain_target %d, value = %ld\n",
		 __func__, hw_gain_target, ucontrol->value.integer.value[0]);

	for (i = 0; i < ARRAY_SIZE(kHWGainMap); i++) {
		if (hw_gain_target == kHWGainMap[i]) {
			index = i;
			break;
		}
	}
	regmap_update_bits(afe->regmap,
			   AFE_GAIN0_CON1_L,
			   GAIN_TARGET_L_MASK_SFT,
			   kHWGainMap_IPM2P[index] << GAIN_TARGET_L_SFT);
	regmap_update_bits(afe->regmap,
			   AFE_GAIN0_CON1_R,
			   GAIN_TARGET_R_MASK_SFT,
			   kHWGainMap_IPM2P[index] << GAIN_TARGET_R_SFT);

	return 0;
}

static int mt6989_gain1_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	unsigned int value = 0x0;

	regmap_read(afe->regmap, AFE_GAIN0_CUR_L, &value);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mt6989_gain1_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	unsigned int hw_gain_target = 0x0;
	int i = 0, index = 0;

	hw_gain_target = ucontrol->value.integer.value[0];
	for (i = 0; i < ARRAY_SIZE(kHWGainMap); i++) {
		if (hw_gain_target == kHWGainMap[i]) {
			index = i;
			break;
		}
	}

	regmap_update_bits(afe->regmap,
			   AFE_GAIN1_CON1_L,
			   GAIN_TARGET_L_MASK_SFT,
			   kHWGainMap_IPM2P[index] << GAIN_TARGET_L_SFT);
	regmap_update_bits(afe->regmap,
			   AFE_GAIN1_CON1_R,
			   GAIN_TARGET_R_MASK_SFT,
			   kHWGainMap_IPM2P[index] << GAIN_TARGET_R_SFT);

	return 0;
}
static const struct snd_soc_dapm_widget mtk_dai_hw_gain_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("HW_GAIN0_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain0_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_gain0_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN0_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain0_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_gain0_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN1_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain1_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_gain1_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN1_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain1_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_gain1_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN2_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain2_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_gain2_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN2_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain2_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_gain2_in_ch2_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN3_IN_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain3_in_ch1_mix,
			   ARRAY_SIZE(mtk_hw_gain3_in_ch1_mix)),
	SND_SOC_DAPM_MIXER("HW_GAIN3_IN_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_hw_gain3_in_ch2_mix,
			   ARRAY_SIZE(mtk_hw_gain3_in_ch2_mix)),

	SND_SOC_DAPM_SUPPLY(HW_GAIN_0_EN_W_NAME,
			    AFE_GAIN0_CON0, GAIN_ON_SFT, 0,
			    mtk_hw_gain_event,
			    SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY(HW_GAIN_1_EN_W_NAME,
			    AFE_GAIN1_CON0, GAIN_ON_SFT, 0,
			    mtk_hw_gain_event,
			    SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY(HW_GAIN_2_EN_W_NAME,
			    AFE_GAIN2_CON0, GAIN_ON_SFT, 0,
			    mtk_hw_gain_event,
			    SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY(HW_GAIN_3_EN_W_NAME,
			    AFE_GAIN3_CON0, GAIN_ON_SFT, 0,
			    mtk_hw_gain_event,
			    SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_INPUT("HW Gain 0 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW Gain 1 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW Gain 2 Out Endpoint"),
	SND_SOC_DAPM_INPUT("HW Gain 3 Out Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW Gain 0 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW Gain 1 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW Gain 2 In Endpoint"),
	SND_SOC_DAPM_OUTPUT("HW Gain 3 In Endpoint"),
};

static const struct snd_soc_dapm_route mtk_dai_hw_gain_routes[] = {
	{"HW Gain 0 In", NULL, "HW_GAIN0_IN_CH1"},
	{"HW Gain 0 In", NULL, "HW_GAIN0_IN_CH2"},
	{"HW Gain 1 In", NULL, "HW_GAIN1_IN_CH1"},
	{"HW Gain 1 In", NULL, "HW_GAIN1_IN_CH2"},
	{"HW Gain 2 In", NULL, "HW_GAIN2_IN_CH1"},
	{"HW Gain 2 In", NULL, "HW_GAIN2_IN_CH2"},
	{"HW Gain 3 In", NULL, "HW_GAIN3_IN_CH1"},
	{"HW Gain 3 In", NULL, "HW_GAIN3_IN_CH2"},

	{"HW Gain 0 In", NULL, HW_GAIN_0_EN_W_NAME},
	{"HW Gain 0 Out", NULL, HW_GAIN_0_EN_W_NAME},
	{"HW Gain 1 In", NULL, HW_GAIN_1_EN_W_NAME},
	{"HW Gain 1 Out", NULL, HW_GAIN_1_EN_W_NAME},
	{"HW Gain 2 In", NULL, HW_GAIN_2_EN_W_NAME},
	{"HW Gain 2 Out", NULL, HW_GAIN_2_EN_W_NAME},
	{"HW Gain 3 In", NULL, HW_GAIN_3_EN_W_NAME},
	{"HW Gain 3 Out", NULL, HW_GAIN_3_EN_W_NAME},

	{"HW Gain 0 In Endpoint", NULL, "HW Gain 0 In"},
	{"HW Gain 1 In Endpoint", NULL, "HW Gain 1 In"},
	{"HW Gain 2 In Endpoint", NULL, "HW Gain 2 In"},
	{"HW Gain 3 In Endpoint", NULL, "HW Gain 3 In"},
	{"HW Gain 0 Out", NULL, "HW Gain 0 Out Endpoint"},
	{"HW Gain 1 Out", NULL, "HW Gain 1 Out Endpoint"},
	{"HW Gain 2 Out", NULL, "HW Gain 2 Out Endpoint"},
	{"HW Gain 3 Out", NULL, "HW Gain 3 Out Endpoint"},
};

static const struct snd_kcontrol_new mtk_hw_gain_controls[] = {
	SOC_SINGLE_EXT("HW Gain 1", SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6989_gain0_get, mt6989_gain0_set),
	SOC_SINGLE_EXT("HW Gain 2", SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6989_gain1_get, mt6989_gain1_set),
	SOC_SINGLE("HW Gain 0 L", AFE_GAIN0_CON1_L,
		   GAIN_TARGET_L_SFT, GAIN_TARGET_L_MASK, 0),
	SOC_SINGLE("HW Gain 0 R", AFE_GAIN0_CON1_R,
		   GAIN_TARGET_R_SFT, GAIN_TARGET_R_MASK, 0),
	SOC_SINGLE("HW Gain 1 L", AFE_GAIN1_CON1_L,
		   GAIN_TARGET_L_SFT, GAIN_TARGET_L_MASK, 0),
	SOC_SINGLE("HW Gain 1 R", AFE_GAIN1_CON1_R,
		   GAIN_TARGET_R_SFT, GAIN_TARGET_R_MASK, 0),
	SOC_SINGLE("HW Gain 2 L", AFE_GAIN2_CON1_L,
		   GAIN_TARGET_L_SFT, GAIN_TARGET_L_MASK, 0),
	SOC_SINGLE("HW Gain 2 R", AFE_GAIN2_CON1_R,
		   GAIN_TARGET_R_SFT, GAIN_TARGET_R_MASK, 0),
	SOC_SINGLE("HW Gain 3 L", AFE_GAIN3_CON1_L,
		   GAIN_TARGET_L_SFT, GAIN_TARGET_L_MASK, 0),
	SOC_SINGLE("HW Gain 3 R", AFE_GAIN3_CON1_R,
		   GAIN_TARGET_R_SFT, GAIN_TARGET_R_MASK, 0),
};

/* dai ops */
static int mtk_dai_gain_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	unsigned int rate = params_rate(params);
	unsigned int rate_reg = mt6989_rate_transform(afe->dev, rate, dai->id);

	dev_info(afe->dev, "%s(), id %d, stream %d, rate %d\n",
		 __func__,
		 dai->id,
		 substream->stream,
		 rate);

	switch (dai->id) {
	case MT6989_DAI_HW_GAIN_0:
		/* rate */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN0_CON0,
				   GAIN_SEL_FS_MASK_SFT,
				   rate_reg << GAIN_SEL_FS_SFT);

		/* sample per step */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN0_CON0,
				   GAIN_SAMPLE_PER_STEP_MASK_SFT,
				   0x40 << GAIN_SAMPLE_PER_STEP_SFT);
		break;
	case MT6989_DAI_HW_GAIN_1:
		/* rate */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN1_CON0,
				   GAIN_SEL_FS_MASK_SFT,
				   rate_reg << GAIN_SEL_FS_SFT);

		/* sample per step */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN1_CON0,
				   GAIN_SAMPLE_PER_STEP_MASK_SFT,
				   0x0 << GAIN_SAMPLE_PER_STEP_SFT);
		break;
	case MT6989_DAI_HW_GAIN_2:
		/* rate */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN2_CON0,
				   GAIN_SEL_FS_MASK_SFT,
				   rate_reg << GAIN_SEL_FS_SFT);

		/* sample per step */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN2_CON0,
				   GAIN_SAMPLE_PER_STEP_MASK_SFT,
				   0x40 << GAIN_SAMPLE_PER_STEP_SFT);
		break;
	case MT6989_DAI_HW_GAIN_3:
		/* rate */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN3_CON0,
				   GAIN_SEL_FS_MASK_SFT,
				   rate_reg << GAIN_SEL_FS_SFT);

		/* sample per step */
		regmap_update_bits(afe->regmap,
				   AFE_GAIN3_CON0,
				   GAIN_SAMPLE_PER_STEP_MASK_SFT,
				   0x0 << GAIN_SAMPLE_PER_STEP_SFT);
		break;
	default:
		break;
	}


	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_gain_ops = {
	.hw_params = mtk_dai_gain_hw_params,
};

/* dai driver */
#define MTK_HW_GAIN_RATES (SNDRV_PCM_RATE_8000_48000 |\
			   SNDRV_PCM_RATE_88200 |\
			   SNDRV_PCM_RATE_96000 |\
			   SNDRV_PCM_RATE_176400 |\
			   SNDRV_PCM_RATE_192000)

#define MTK_HW_GAIN_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			     SNDRV_PCM_FMTBIT_S24_LE |\
			     SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_gain_driver[] = {
	{
		.name = "HW Gain 0",
		.id = MT6989_DAI_HW_GAIN_0,
		.playback = {
			.stream_name = "HW Gain 0 In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.capture = {
			.stream_name = "HW Gain 0 Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.ops = &mtk_dai_gain_ops,
		.symmetric_rate = 1,
		.symmetric_channels = 1,
		.symmetric_sample_bits = 1,
	},
	{
		.name = "HW Gain 1",
		.id = MT6989_DAI_HW_GAIN_1,
		.playback = {
			.stream_name = "HW Gain 1 In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.capture = {
			.stream_name = "HW Gain 1 Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.ops = &mtk_dai_gain_ops,
		.symmetric_rate = 1,
		.symmetric_channels = 1,
		.symmetric_sample_bits = 1,
	},
	{
		.name = "HW Gain 2",
		.id = MT6989_DAI_HW_GAIN_2,
		.playback = {
			.stream_name = "HW Gain 2 In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.capture = {
			.stream_name = "HW Gain 2 Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.ops = &mtk_dai_gain_ops,
		.symmetric_rate = 1,
		.symmetric_channels = 1,
		.symmetric_sample_bits = 1,
	},
	{
		.name = "HW Gain 3",
		.id = MT6989_DAI_HW_GAIN_3,
		.playback = {
			.stream_name = "HW Gain 3 In",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.capture = {
			.stream_name = "HW Gain 3 Out",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HW_GAIN_RATES,
			.formats = MTK_HW_GAIN_FORMATS,
		},
		.ops = &mtk_dai_gain_ops,
		.symmetric_rate = 1,
		.symmetric_channels = 1,
		.symmetric_sample_bits = 1,
	},
};

int mt6989_dai_hw_gain_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_gain_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_gain_driver);

	dai->controls = mtk_hw_gain_controls;
	dai->num_controls = ARRAY_SIZE(mtk_hw_gain_controls);
	dai->dapm_widgets = mtk_dai_hw_gain_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_hw_gain_widgets);
	dai->dapm_routes = mtk_dai_hw_gain_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_hw_gain_routes);
	return 0;
}

