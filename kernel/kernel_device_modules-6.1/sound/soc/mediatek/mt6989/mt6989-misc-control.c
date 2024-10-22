// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio Misc Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <mtk_peak_power_budget.h>
#include <linux/regulator/consumer.h>

#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-afe-platform-driver.h"


#include "mt6989-afe-common.h"
#include "mt6989-afe-gpio.h"

#define SGEN_TIE_CH1_KCONTROL_NAME "Audio_SineGen_Tie_Ch1"
#define SGEN_TIE_CH2_KCONTROL_NAME "Audio_SineGen_Tie_Ch2"

static const char *const mt6989_sgen_mode_str[] = {
	"I0I1",   "I2",     "I3I4",   "I5I6",
	"I7I8",   "I9",     "I10I11", "I12I13",
	"I14",    "I15I16", "I17I18", "I19I20",
	"I21I22", "I23I24", "I25I26", "I27I28",
	"I29I30", "I31I32", "I33", "I34I35",
	"I36I37", "I38I39", "I40I41", "I42I43",
	"I44I45", "I46I47", "I48I49", "I50I51",
	"I52I53", "I54I55", "I56I57", "I58I59",
	"I60I61", "I62I63", "I78I79",
	"O0O1",   "O2",     "O3O4",   "O5O6",
	"O7O8",   "O9O10",  "O11",    "O12",
	"O13O14", "O15O16", "O17O18", "O19O20",
	"O21O22", "O23O24", "O25",    "O28O29",
	"O34",    "O32O33", "O35", "O36O37",
	"O38O39", "O30O31", "O40O41", "O42O43",
	"O44O45", "O46O47", "O48O49", "O50O51",
	"O54O55", "O56O57", "O58O59", "O60O61",
	"O62O63", "O64O65", "O66O67", "O68O69",
	"O70O71", "O72O73", "O74O75", "O76O77",
	"O78O79", "O80O81", "O82O83",
	"OFF", "O3", "O4",
};

static const int mt6989_sgen_mode_idx[] = {
	0, 1, 2, 3,
	4, 5, 6, 7,
	8, 9, 10, 11,
	12, 13, 14, 15,
	16, 17, 18, 19,
	20, 21,	22, 23,
	24, 25,	26, 27,
	28, 29, 30, 31,
	32, 33, 36,
	64, 65, 66, 67,
	68, 69, 70, 71,
	72, 73, 74, 75,
	76, 77, 78, 79,
	80, 81, 82, 83,
	84, 85, 86, 87,
	88, 89, 90, 91,
	92, 93, 94, 95,
	96, 97, 98, 99,
	100, 101, 102, 103,
	104, 105, 106, 107,
	-1, -1,	-1,
};
static const char *const mt6989_sgen_sel_str[] = {
	"In", "Out"
};

static const char *const mt6989_sgen_mode_in_str[] = {
	"I0I1",     "I6I7",     "I8I9",     "I10I11",
	"I12I13",   "I14",      "I16I17",   "I18I19",
	"I20I21",   "I24I25",   "I28I29",   "I30I31",
	"I32I33",   "I34I35",   "I36I37",   "I38I39",
	"I40I41",   "I42I43",   "I44I45",   "I46I47",
	"I48I49",   "I54I55",   "I56I57",   "I58I59",
	"I60I61",   "I78I79",   "I80I81",   "I82I83",
	"I130I131", "I132I133", "I134I135", "I136I137",
	"I138I139", "I142I143", "I144I145", "I146I147",
	"I148I149", "I166I167", "I198I199", "I200I201",
	"I202I203", "I204I205",
	"OFF",
};
static const int mt6989_sgen_mode_in_idx[] = {
	0, 3, 4, 5,
	6, 7, 8, 9,
	10, 12, 14, 15,
	16, 17, 18, 19,
	20, 21,	22, 23,
	24, 27, 28, 29,
	30, 39, 40, 41,
	65, 66, 67, 68,
	69, 71, 72, 73,
	74, 83, 99, 100,
	101, 102,
	-1,
};


static const char *const mt6989_sgen_mode_out_str[] = {
	"O4O5",     "O6O7",     "O8O9",     "O10O11",
	"O12",      "O14O15",   "O16O17",   "O18O19",
	"O20O21",   "O22O23",   "O24O25",   "O26O27",
	"O28O29",   "O30O31",   "O32O33",   "O34O35",
	"O36O37",   "O38O39",   "O40O41",   "O42O43",
	"O44O45",   "O46O47",   "O48O49",   "O50O51",
	"O52O53",   "O54O55",   "O56O57",   "O58O59",
	"O60O61",   "O62O63",   "O66O67",   "O68O69",
	"O96O97",   "O98O99",   "O100O101", "O102O103",
	"O104O105", "O106",     "O108O109", "O110O111",
	"O112O113", "O116O117", "O118O119", "O120O121",
	"O122O123", "O148O149", "O180O181", "O182O183",
	"O184O185", "O186O187",
	"OFF",
};


static const int mt6989_sgen_mode_out_idx[] = {
	2, 3, 4, 5,
	6, 7, 8, 9,
	10, 11, 12, 13,
	14, 15, 16, 17,
	18, 19, 20, 21,
	22, 23, 24, 25,
	26, 27, 28, 29,
	30, 31, 33, 34,
	48, 49, 50, 51,
	52, 53, 54, 55,
	56, 58, 59, 60,
	61, 74, 90, 91,
	92, 93,
	-1,
};

static const char *const mt6989_sgen_rate_str[] = {
	"8K", "11K", "12K", "16K",
	"22K", "24K", "32K", "44K",
	"48K", "88k", "96k", "176k",
	"192k", "352k", "384k"
};

static const int mt6989_sgen_rate_idx[] = {
	0, 1, 2, 4,
	5, 6, 8, 9,
	10, 13, 14, 17,
	18, 21, 22
};

/* this order must match reg bit amp_div_ch1/2 */
static const char *const mt6989_sgen_amp_str[] = {
	"1/128", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1"
};

static const char *const mt6989_sgen_tie_str[] = {
	"Off", "On"
};

static int mt6989_sgen_sel_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_sel;
	return 0;
}

static int mt6989_sgen_sel_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int sel;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	sel = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), sel %d\n", __func__, sel);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACKI_SEL_MASK_SFT,
			   sel << INNER_LOOP_BACKI_SEL_SFT);

	afe_priv->sgen_sel = sel;

	return 0;
}

static int mt6989_sgen_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_mode;
	return 0;
}

static int mt6989_sgen_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mode;
	int mode_sel;
	int mode_idx;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mode = ucontrol->value.integer.value[0];
	mode_sel = afe_priv->sgen_sel;
	if (!mode_sel)
		mode_idx = mt6989_sgen_mode_in_idx[mode];
	else
		mode_idx = mt6989_sgen_mode_out_idx[mode];

	dev_info(afe->dev, "%s(), mode %d, mode_sel %d, mode_idx %d\n",
		 __func__, mode, mode_sel, mode_idx);


	if (mode_idx > 0) {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   mode_idx << INNER_LOOP_BACK_MODE_SFT);
		regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x04ac2ac1);
	} else {
		/* disable sgen */
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   DAC_EN_MASK_SFT,
				   0x0);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   0xff << INNER_LOOP_BACK_MODE_SFT);
	}

	afe_priv->sgen_mode = mode;
	return 0;
}

static int mt6989_sgen_tmp_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_mode;
	return 0;
}

static int mt6989_sgen_tmp_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mode;
	int mode_idx;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mode = ucontrol->value.integer.value[0];
	mode_idx = mt6989_sgen_mode_idx[mode];

	dev_info(afe->dev, "%s(), mode %d, mode_idx %d\n",
		 __func__, mode, mode_idx);


	if (mode_idx == 66) {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   INNER_LOOP_BACKI_SEL_MASK_SFT,
			   0x1 << INNER_LOOP_BACKI_SEL_SFT);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   0x7 << INNER_LOOP_BACK_MODE_SFT);
		regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x04702702);
	} else {
		/* disable sgen */
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   DAC_EN_MASK_SFT,
				   0x0);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   INNER_LOOP_BACK_MODE_MASK_SFT,
				   0xff << INNER_LOOP_BACK_MODE_SFT);
	}

	afe_priv->sgen_mode = mode;
	return 0;
}


static int mt6989_sgen_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_rate;
	return 0;
}

static int mt6989_sgen_rate_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int rate;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	rate = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), rate %d\n", __func__, rate);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   SINE_MODE_MASK_SFT,
			   mt6989_sgen_rate_idx[rate] << SINE_MODE_SFT);

	afe_priv->sgen_rate = rate;
	return 0;
}

static int mt6989_sgen_amplitude_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->sgen_amplitude;
	return 0;
}

static int mt6989_sgen_amplitude_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int amplitude;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	amplitude = ucontrol->value.integer.value[0];
	if (amplitude > AMP_DIV_CH1_MASK) {
		dev_info(afe->dev, "%s(), amplitude %d invalid\n",
			 __func__, amplitude);
		return -EINVAL;
	}

	dev_info(afe->dev, "%s(), amplitude %d\n", __func__, amplitude);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AMP_DIV_CH1_MASK_SFT,
			   amplitude << AMP_DIV_CH1_SFT);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AMP_DIV_CH2_MASK_SFT,
			   amplitude << AMP_DIV_CH2_SFT);

	afe_priv->sgen_amplitude = amplitude;

	return 0;
}

static int mt6989_sgen_tie_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int tie = 0;

	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &tie);

	if (strcmp(kcontrol->id.name, SGEN_TIE_CH1_KCONTROL_NAME) == 0)
		return (tie >> TIE_SW_CH1_SFT) & TIE_SW_CH1_MASK;
	else
		return (tie >> TIE_SW_CH2_SFT) & TIE_SW_CH2_MASK;
}

static int mt6989_sgen_tie_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int tie;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	tie = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, tie %d\n",
		 __func__, kcontrol->id.name, tie);

	if (strcmp(kcontrol->id.name, SGEN_TIE_CH1_KCONTROL_NAME) == 0) {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   TIE_SW_CH1_MASK_SFT,
				   tie << TIE_SW_CH1_SFT);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON2,
				   TIE_CH1_CONSTENT_MASK_SFT,
				   0x0 << TIE_CH1_CONSTENT_SFT);
	} else {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   TIE_SW_CH2_MASK_SFT,
				   tie << TIE_SW_CH2_SFT);
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON3,
				   TIE_CH2_CONSTENT_MASK_SFT,
				   0x0 << TIE_CH2_CONSTENT_SFT);
	}

	return 0;
}

static const struct soc_enum mt6989_afe_sgen_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_sgen_sel_str),
			    mt6989_sgen_sel_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_sgen_mode_in_str),
			    mt6989_sgen_mode_in_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_sgen_mode_out_str),
			    mt6989_sgen_mode_out_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_sgen_rate_str),
			    mt6989_sgen_rate_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_sgen_amp_str),
			    mt6989_sgen_amp_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_sgen_tie_str),
			    mt6989_sgen_tie_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6989_sgen_mode_str),
			    mt6989_sgen_mode_str),
};

static const struct snd_kcontrol_new mt6989_afe_sgen_controls[] = {
	SOC_ENUM_EXT("Audio_SineGen_Sel", mt6989_afe_sgen_enum[0],
		     mt6989_sgen_sel_get, mt6989_sgen_sel_set),
	SOC_ENUM_EXT("Audio_SineGen_Switch_In", mt6989_afe_sgen_enum[1],
		     mt6989_sgen_get, mt6989_sgen_set),
	SOC_ENUM_EXT("Audio_SineGen_Switch_Out", mt6989_afe_sgen_enum[2],
		     mt6989_sgen_get, mt6989_sgen_set),
	SOC_ENUM_EXT("Audio_SineGen_Switch", mt6989_afe_sgen_enum[6],
		     mt6989_sgen_tmp_get, mt6989_sgen_tmp_set),
	SOC_ENUM_EXT("Audio_SineGen_SampleRate", mt6989_afe_sgen_enum[3],
		     mt6989_sgen_rate_get, mt6989_sgen_rate_set),
	SOC_ENUM_EXT("Audio_SineGen_Amplitude", mt6989_afe_sgen_enum[4],
		     mt6989_sgen_amplitude_get, mt6989_sgen_amplitude_set),
	SOC_ENUM_EXT(SGEN_TIE_CH1_KCONTROL_NAME, mt6989_afe_sgen_enum[5],
		     mt6989_sgen_tie_get, mt6989_sgen_tie_set),
	SOC_ENUM_EXT(SGEN_TIE_CH2_KCONTROL_NAME, mt6989_afe_sgen_enum[5],
		     mt6989_sgen_tie_get, mt6989_sgen_tie_set),
	SOC_SINGLE("Audio_SineGen_Freq_Div_Ch1", AFE_SINEGEN_CON0,
		   FREQ_DIV_CH1_SFT, FREQ_DIV_CH1_MASK, 0),
	SOC_SINGLE("Audio_SineGen_Freq_Div_Ch2", AFE_SINEGEN_CON0,
		   FREQ_DIV_CH2_SFT, FREQ_DIV_CH2_MASK, 0),
};

/* usb call control */
static int mt6989_usb_echo_ref_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->usb_call_echo_ref_size;
	return 0;
}

static int mt6989_usb_echo_ref_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int dl_id = MT6989_MEMIF_DL0;
	int ul_id = MT6989_MEMIF_VUL8;
	struct mtk_base_afe_memif *dl_memif = &afe->memif[dl_id];
	struct mtk_base_afe_memif *ul_memif = &afe->memif[ul_id];
	int enable;
	int size;

	size = ucontrol->value.integer.value[0];

	if (size > 0)
		enable = true;
	else
		enable = false;

	if (!dl_memif->substream) {
		dev_info(afe->dev, "%s(), dl_memif->substream == NULL\n",
			 __func__);
		return -EINVAL;
	}

	if (!ul_memif->substream) {
		dev_info(afe->dev, "%s(), ul_memif->substream == NULL\n",
			 __func__);
		return -EINVAL;
	}

	if (enable) {
		dev_info(afe->dev, "%s(), prev enable %d, user size %d, default dma_addr %pad, bytes %zu, reallocate %d\n",
			 __func__,
			 afe_priv->usb_call_echo_ref_enable,
			 size,
			 &dl_memif->dma_addr, dl_memif->dma_bytes,
			 afe_priv->usb_call_echo_ref_reallocate);

		if (afe_priv->usb_call_echo_ref_enable) {
			mtk_memif_set_disable(afe, dl_id);
			mtk_memif_set_disable(afe, ul_id);
		}

		/* reallocate if needed */
		if (size != dl_memif->dma_bytes) {
			unsigned char *dma_area;

			if (afe_priv->usb_call_echo_ref_reallocate) {
				/* free previous allocate */
				dma_free_coherent(afe->dev,
						  dl_memif->dma_bytes,
						  dl_memif->dma_area,
						  dl_memif->dma_addr);
			}

			dl_memif->dma_bytes = size;
			dma_area = dma_alloc_coherent(afe->dev,
						      dl_memif->dma_bytes,
						      &dl_memif->dma_addr,
						      GFP_KERNEL | GFP_DMA);
			if (!dma_area) {
				dev_info(afe->dev, "%s(), dma_alloc_coherent fail\n",
					__func__);
				return -ENOMEM;
			}
			dl_memif->dma_area = dma_area;

			mtk_memif_set_addr(afe, dl_id,
					   dl_memif->dma_area,
					   dl_memif->dma_addr,
					   dl_memif->dma_bytes);

			afe_priv->usb_call_echo_ref_reallocate = true;
		}

		/* just to double confirm the buffer size is align */
		if (dl_memif->dma_bytes !=
		    word_size_align(dl_memif->dma_bytes)) {
			dev_info(afe->dev, "%s(), buffer size not align\n",
				__func__);
		}

		/* let ul use the same memory as dl */
		mtk_memif_set_addr(afe, ul_id,
				   dl_memif->dma_area,
				   dl_memif->dma_addr,
				   dl_memif->dma_bytes);

		/* clean buffer */
		memset_io(dl_memif->dma_area, 0, dl_memif->dma_bytes);

		mtk_memif_set_pbuf_size(afe, dl_id,
					MT6989_MEMIF_PBUF_SIZE_32_BYTES);

		/* enable memif with a bit delay */
		/* note: dl memif have prefetch buffer, */
		/* it will have a leap at the beginning */
		mtk_memif_set_enable(afe, dl_id);
		udelay(30);
		mtk_memif_set_enable(afe, ul_id);

		dev_info(afe->dev, "%s(), memif_lpbk path hw enabled\n",
			 __func__);
	} else {
		dev_info(afe->dev, "%s(), disable\n", __func__);

		mtk_memif_set_disable(afe, dl_id);
		mtk_memif_set_disable(afe, ul_id);

		if (afe_priv->usb_call_echo_ref_reallocate) {
			/* free previous allocate */
			dma_free_coherent(afe->dev,
					  dl_memif->dma_bytes,
					  dl_memif->dma_area,
					  dl_memif->dma_addr);
		}

		afe_priv->usb_call_echo_ref_reallocate = false;
	}

	afe_priv->usb_call_echo_ref_enable = enable;
	afe_priv->usb_call_echo_ref_size = size;

	return 0;
}

static const struct snd_kcontrol_new mt6989_afe_usb_controls[] = {
	SOC_SINGLE_EXT("usb_call_echo_ref", SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       mt6989_usb_echo_ref_get, mt6989_usb_echo_ref_set),
};

/* speech mixctrl instead property usage */
static void *get_sph_property_by_name(struct mt6989_afe_private *afe_priv,
				      const char *name)
{
	if (strcmp(name, "Speech_A2M_Msg_ID") == 0)
		return &afe_priv->speech_a2m_msg_id;
	else if (strcmp(name, "Speech_MD_Status") == 0)
		return &afe_priv->speech_md_status;
	else if (strcmp(name, "Speech_SCP_CALL_STATE") == 0)
		return &afe_priv->speech_adsp_status;
	else if (strcmp(name, "Speech_Mic_Mute") == 0)
		return &afe_priv->speech_mic_mute;
	else if (strcmp(name, "Speech_DL_Mute") == 0)
		return &afe_priv->speech_dl_mute;
	else if (strcmp(name, "Speech_UL_Mute") == 0)
		return &afe_priv->speech_ul_mute;
	else if (strcmp(name, "Speech_Phone1_MD_Idx") == 0)
		return &afe_priv->speech_phone1_md_idx;
	else if (strcmp(name, "Speech_Phone2_MD_Idx") == 0)
		return &afe_priv->speech_phone2_md_idx;
	else if (strcmp(name, "Speech_Phone_ID") == 0)
		return &afe_priv->speech_phone_id;
	else if (strcmp(name, "Speech_MD_EPOF") == 0)
		return &afe_priv->speech_md_epof;
	else if (strcmp(name, "Speech_BT_SCO_WB") == 0)
		return &afe_priv->speech_bt_sco_wb;
	else if (strcmp(name, "Speech_SHM_Init") == 0)
		return &afe_priv->speech_shm_init;
	else if (strcmp(name, "Speech_SHM_USIP") == 0)
		return &afe_priv->speech_shm_usip;
	else if (strcmp(name, "Speech_SHM_Widx") == 0)
		return &afe_priv->speech_shm_widx;
	else if (strcmp(name, "Speech_MD_HeadVersion") == 0)
		return &afe_priv->speech_md_headversion;
	else if (strcmp(name, "Speech_MD_Version") == 0)
		return &afe_priv->speech_md_version;
	else if (strcmp(name, "Speech_Cust_Param_Init") == 0)
		return &afe_priv->speech_cust_param_init;
	else
		return NULL;
}

static int speech_property_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(afe_priv,
			kcontrol->id.name);
	if (!sph_property) {
		dev_info(afe->dev, "%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = *sph_property;

	return 0;
}

static int speech_property_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(afe_priv,
			kcontrol->id.name);
	if (!sph_property) {
		dev_info(afe->dev, "%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	*sph_property = ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new mt6989_afe_speech_controls[] = {
	SOC_SINGLE_EXT("Speech_A2M_Msg_ID",
		       SND_SOC_NOPM, 0, 0xFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Status",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SCP_CALL_STATE",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Mic_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_DL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_UL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone1_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone2_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone_ID",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_EPOF",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_BT_SCO_WB",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SHM_Init",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SHM_USIP",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_SHM_Widx",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_HeadVersion",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Version",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Cust_Param_Init",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
};

/* VOW barge in control */
static int mt6989_afe_vow_barge_in_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int id;

	id = MT6989_BARGE_IN_MEMIF;
	ucontrol->value.integer.value[0] = afe->memif[id].vow_barge_in_enable;

	return 0;
}

static int mt6989_afe_vow_barge_in_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int id;
	int val;

	id = MT6989_BARGE_IN_MEMIF;
	val = ucontrol->value.integer.value[0];
	dev_info(afe->dev, "%s(), %d\n", __func__, val);

	afe->memif[id].vow_barge_in_enable = (val > 0) ? true : false;

	return 0;
}
static int mt6989_dev_power_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int power = 0;

	ucontrol->value.integer.value[0] = power;

	return 0;
}

static int mt6989_dev_power_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int power = 0;

	power = ucontrol->value.integer.value[0];
	kicker_ppb_request_power(KR_AUDIO, power);

	return 0;
}


static const struct snd_kcontrol_new mt6989_afe_barge_in_controls[] = {
	SOC_SINGLE_EXT("Vow_bargein_echo_ref", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_afe_vow_barge_in_get,
		       mt6989_afe_vow_barge_in_set),
};


static const struct snd_kcontrol_new mt6989_afe_power_controls[] = {
	SOC_SINGLE_EXT("Dev_Power", SND_SOC_NOPM, 0, 0xffff, 0,
		       mt6989_dev_power_get,
		       mt6989_dev_power_set),
};

/* VOW scp dmic control */
static int mt6989_afe_vow_scp_dmic_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt6989_afe_vow_scp_dmic_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6989_afe_private *afe_priv = afe->platform_priv;
	int val = ucontrol->value.integer.value[0];
	int status = 0;

	dev_info(afe->dev, "%s(), %d\n", __func__, val);

	if (val == true) {
		if (!IS_ERR(afe_priv->reg_vib18)) {
			status = regulator_enable(afe_priv->reg_vib18);
			if (status)
				dev_info(afe->dev, "%s() failed to enable vib18(%d)\n",
					__func__, status);
		}
		mt6989_afe_gpio_request(afe, true, MT6989_DAI_VOW_SCP_DMIC, 0);
	} else if (val == false) {
		mt6989_afe_gpio_request(afe, false, MT6989_DAI_VOW_SCP_DMIC, 0);
		if (!IS_ERR(afe_priv->reg_vib18)) {
			status = regulator_disable(afe_priv->reg_vib18);
			if (status)
				dev_info(afe->dev, "%s() failed to disable vib18(%d)\n",
					__func__, status);
		}
	}
	return 0;
}

static const struct snd_kcontrol_new mt6989_afe_scp_dmic_controls[] = {
	SOC_SINGLE_EXT("Vow_scp_dmic_gpio", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6989_afe_vow_scp_dmic_get,
		       mt6989_afe_vow_scp_dmic_set),
};

int mt6989_add_misc_control(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	snd_soc_add_component_controls(component,
				       mt6989_afe_sgen_controls,
				       ARRAY_SIZE(mt6989_afe_sgen_controls));

	snd_soc_add_component_controls(component,
				       mt6989_afe_usb_controls,
				       ARRAY_SIZE(mt6989_afe_usb_controls));

	snd_soc_add_component_controls(component,
				       mt6989_afe_speech_controls,
				       ARRAY_SIZE(mt6989_afe_speech_controls));

	snd_soc_add_component_controls(component,
				       mt6989_afe_barge_in_controls,
				       ARRAY_SIZE(mt6989_afe_barge_in_controls));

	snd_soc_add_component_controls(component,
				       mt6989_afe_power_controls,
				       ARRAY_SIZE(mt6989_afe_power_controls));

	snd_soc_add_component_controls(component,
				       mt6989_afe_scp_dmic_controls,
				       ARRAY_SIZE(mt6989_afe_scp_dmic_controls));

	return 0;
}

#define USECS_TO_CYCLES(time_usecs)			\
	xloops_to_cycles((time_usecs) * 0x10C7UL)

static inline unsigned long xloops_to_cycles(unsigned long xloops)
{
	return (xloops * loops_per_jiffy * HZ) >> 32;
}

void mt6989_aud_delay(unsigned long usecs)
{
	unsigned long cycles = 0;
	cycles_t start = get_cycles();

	cycles = USECS_TO_CYCLES(usecs);

	while ((get_cycles() - start) < cycles)
		cpu_relax();
}

