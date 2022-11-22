/* sound/soc/codecs/rt_hubs.c
 * RT55xx common code
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/mfd/rt5511/registers.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "rt5511.h"
#include "rt_hubs.h"

#include <linux/version.h>
#include <linux/mfd/rt5511/core.h>
#include <linux/mfd/rt5511/registers.h>
#include <linux/mfd/rt5511/pdata.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
/* > 3.4.0 ==> use codec add controls instead of the original one */

static inline int snd_soc_add_controls(struct snd_soc_codec *codec,
	const struct snd_kcontrol_new *controls, int num_controls)
{
	return snd_soc_add_codec_controls(codec, controls, num_controls);
}
#endif

static const DECLARE_TLV_DB_SCALE(mic_pga_tlv, -1650, 150, 0);
static const DECLARE_TLV_DB_SCALE(mic_lrpga_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(aux_pga_tlv, -500, 100, 0);
static const DECLARE_TLV_DB_SCALE(aux_lrpga_tlv, -300, 300, 0);
static const DECLARE_TLV_DB_SCALE(lrpga2lrspk_tlv, -600, 300, 0);
static const DECLARE_TLV_DB_SCALE(lrpga2lrhp_tlv, -600, 300, 0);
static const DECLARE_TLV_DB_SCALE(lrspkpga_tlv, -4300, 100, 0);
static const DECLARE_TLV_DB_SCALE(lrhppga_tlv, -3100, 100, 0);
static const DECLARE_TLV_DB_SCALE(recvboost_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(spkboost_tlv, 600, 300, 0);
static const DECLARE_TLV_DB_SCALE(hpboost_tlv, -1800, 150, 0);

static const char * const hpf_freqcut_text[] = {
	"4Hz", "379Hz", "770Hz", "1594Hz"
};

static const struct soc_enum hpf_freqcut_enum[] = {
	SOC_ENUM_SINGLE(RT5511_ADC_HPF_MODE, 0, 4, hpf_freqcut_text),
	SOC_ENUM_SINGLE(RT5511_ADC_HPF_MODE, 2, 4, hpf_freqcut_text),
	SOC_ENUM_SINGLE(RT5511_ADC_HPF_MODE, 4, 4, hpf_freqcut_text),
};

static int rt5511_set_hp_boost_volume(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int volume_reg;
	unsigned int en_mask, volume_mask;
	unsigned int volume_new, volume_old, hp_en;

	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	volume_reg = mc->reg;
	volume_mask = (1 << fls(mc->max)) - 1;
	volume_new = (ucontrol->value.integer.value[0] & volume_mask);

	if (volume_reg == RT5511_LHP_DRV_VOL)
		en_mask = RT5511_D_LHP_EN;
	else
		en_mask = RT5511_D_RHP_EN;

	mutex_lock(&codec->mutex);
	volume_old = snd_soc_read(codec, volume_reg);
	volume_old &= volume_mask;

	if (volume_old == volume_new) {
		mutex_unlock(&codec->mutex);
		return 0;
	}

	hp_en = snd_soc_read(codec, RT5511_SPK_HP_DRIVER) & en_mask;

	if (hp_en)
		snd_soc_update_bits(codec, RT5511_SPK_HP_DRIVER, hp_en, 0);

	snd_soc_write(codec, volume_reg, volume_new);

	if (hp_en)
		snd_soc_update_bits(codec, RT5511_SPK_HP_DRIVER, hp_en, hp_en);

	mutex_unlock(&codec->mutex);
	return true;
}

static const struct snd_kcontrol_new analogue_snd_controls[] = {

/* Analogue Volume */
	SOC_SINGLE_TLV("Aux PGA Volume",
		RT5511_AUX_PGA_VOL, 0, 31, 0, aux_pga_tlv),
	SOC_SINGLE_TLV("Mic1 PGA Volume",
		RT5511_MIC1_CTRL, 0, 31, 0, mic_pga_tlv),
	SOC_SINGLE_TLV("Mic2 PGA Volume",
		RT5511_MIC2_CTRL, 0, 31, 0, mic_pga_tlv),
	SOC_SINGLE_TLV("Mic3 PGA Volume",
		RT5511_MIC3_CTRL, 0, 31, 0, mic_pga_tlv),

	SOC_SINGLE_TLV("AuxL LPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXL2LPGA_VOL,
		6, 3, 0, aux_lrpga_tlv),
	SOC_SINGLE_TLV("AuxR RPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXR2LPGA_VOL,
		6, 3, 0, aux_lrpga_tlv),
	SOC_SINGLE_TLV("Mic1 LPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXL2LPGA_VOL,
		4, 3, 0, mic_lrpga_tlv),
	SOC_SINGLE_TLV("Mic1 RPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXR2LPGA_VOL,
		4, 3, 0, mic_lrpga_tlv),
	SOC_SINGLE_TLV("Mic2 LPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXL2LPGA_VOL,
		2, 3, 0, mic_lrpga_tlv),
	SOC_SINGLE_TLV("Mic2 RPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXR2LPGA_VOL,
		2, 3, 0, mic_lrpga_tlv),
	SOC_SINGLE_TLV("Mic3 LPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXL2LPGA_VOL,
		0, 3, 0, mic_lrpga_tlv),
	SOC_SINGLE_TLV("Mic3 RPGA Volume",
		RT5511_ANA_PATH_MIXER_VOL_AUXR2LPGA_VOL,
		0, 3, 0, mic_lrpga_tlv),

	SOC_SINGLE_TLV("LPGA2LSPK PGA Volume",
		RT5511_PGA2PGA_MIXER_VOL, 6, 3, 0, lrpga2lrspk_tlv),
	SOC_SINGLE_TLV("RPGA2LSPK PGA Volume",
		RT5511_PGA2PGA_MIXER_VOL, 4, 3, 0, lrpga2lrspk_tlv),
	SOC_SINGLE_TLV("LPGA2RSPK PGA Volume",
		RT5511_PGA2PGA_MIXER_VOL, 2, 3, 0, lrpga2lrspk_tlv),
	SOC_SINGLE_TLV("RPGA2RSPK PGA Volume",
		RT5511_PGA2PGA_MIXER_VOL, 0, 3, 0, lrpga2lrspk_tlv),

	SOC_SINGLE_TLV("LPGA2LHP PGA Volume",
		RT5511_HP_PGA_VOL, 6, 3, 0, lrpga2lrhp_tlv),
	SOC_SINGLE_TLV("RPGA2LHP PGA Volume",
		RT5511_HP_PGA_VOL, 4, 3, 0, lrpga2lrhp_tlv),
	SOC_SINGLE_TLV("LPGA2RHP PGA Volume",
		RT5511_HP_PGA_VOL, 2, 3, 0, lrpga2lrhp_tlv),
	SOC_SINGLE_TLV("RPGA2RHP PGA Volume",
		RT5511_HP_PGA_VOL, 0, 3, 0, lrpga2lrhp_tlv),

	SOC_SINGLE_TLV("LSPK PGA Volume",
		RT5511_LSPKPGA_VOL, 0, 46, 0, lrspkpga_tlv),
	SOC_SINGLE_TLV("RSPK PGA Volume",
		RT5511_RSPKPGA_VOL, 0, 46, 0, lrspkpga_tlv),
	SOC_SINGLE_TLV("LHP PGA Volume",
		RT5511_LHP_MIX_VOL, 0, 31, 0, lrhppga_tlv),
	SOC_SINGLE_TLV("RHP PGA Volume",
		RT5511_RHP_MIX_VOL, 0, 31, 0, lrhppga_tlv),

	SOC_SINGLE_TLV("RECV Boost Volume",
		RT5511_RCV_CTRL, 3, 1, 0, recvboost_tlv),
	SOC_SINGLE_TLV("LSPK Boost Volume",
		RT5511_SPK_BYPASS_BOOST, 4, 3, 0, spkboost_tlv),
	SOC_SINGLE_TLV("RSPK Boost Volume",
		RT5511_SPK_BYPASS_BOOST, 0, 3, 0, spkboost_tlv),
	SOC_SINGLE_EXT_TLV("LHP Boost Volume", RT5511_LHP_DRV_VOL, 0, 15, 0,
		snd_soc_get_volsw, rt5511_set_hp_boost_volume, hpboost_tlv),
	SOC_SINGLE_EXT_TLV("RHP Boost Volume", RT5511_RHP_DRV_VOL, 0, 15, 0,
		snd_soc_get_volsw, rt5511_set_hp_boost_volume, hpboost_tlv),

/* ZC Control */
	SOC_SINGLE("MIC PGA VOL ZC ramp enable",
		RT5511_ZC_RAMP_TIME, 6, 1, 0),
	SOC_SINGLE("SPK PGA VOL ZC ramp enable",
		RT5511_ZC_RAMP_TIME, 5, 1, 0),
	SOC_SINGLE("HP PGA VOL ZC ramp enable",
		RT5511_ZC_RAMP_TIME, 4, 1, 0),

	SOC_SINGLE("MIC ZC ramp time duration",
		RT5511_ZC_RAMP_TIME, 0, 3, 0),
	SOC_SINGLE("SPK ZC ramp time duration",
		RT5511_ZC_RAMP_TIME_EN, 6, 3, 0),
	SOC_SINGLE("HP ZC ramp time duration",
		RT5511_ZC_RAMP_TIME_EN, 4, 3, 0),

	SOC_SINGLE("D_HP_ZC_EN", RT5511_ZC_RAMP_TIME_EN, 1, 1, 0),
	SOC_SINGLE("D_PGA_ZC_EN", RT5511_ZC_RAMP_TIME_EN, 0, 1, 0),

	SOC_SINGLE("MIC Volume ZC enable",
		RT5511_ZC_TIME_OUT_AUX_MIC, 3, 1, 0),
	SOC_SINGLE("MIC ZC time out select",
		RT5511_ZC_TIME_OUT_AUX_MIC, 0, 7, 0),
	SOC_SINGLE("SPK Volume ZC enable",
		RT5511_ZC_TIME_OUT_SPK_HP, 7, 1, 0),
	SOC_SINGLE("SPK ZC time out select",
		RT5511_ZC_TIME_OUT_SPK_HP, 4, 7, 0),
	SOC_SINGLE("HP Volume ZC enable",
		RT5511_ZC_TIME_OUT_SPK_HP, 3, 1, 0),
	SOC_SINGLE("HP ZC time out select",
		RT5511_ZC_TIME_OUT_SPK_HP, 0, 7, 0),

/* Others */
	SOC_SINGLE("HP offset compensation",
		RT5511_D_HP_IBSEL, 2, 3, 0),

/* Cut Off frequency */
	SOC_ENUM("ADC HPF1 FC", hpf_freqcut_enum[0]),
	SOC_ENUM("ADC HPF2 FC", hpf_freqcut_enum[1]),
	SOC_ENUM("ADC HPF3 FC", hpf_freqcut_enum[2]),
};

/*
 * [BLOCK] RT5511 Analog KControl
 */

static const char * const analog_bypass_sel[] = {
	"Aux", "Mic1", "Mic2", "Mic3"
};

static const struct soc_enum analog_bypass_enum =
	SOC_ENUM_SINGLE(RT5511_BYPASS_SEL, 0, 4, analog_bypass_sel);

const struct snd_kcontrol_new analog_bypass_controls =
	SOC_DAPM_ENUM("Analog Bypass", analog_bypass_enum);

static const struct snd_kcontrol_new ladc_mixer_controls[] = {
	SOC_DAPM_SINGLE("AuxL Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 7, 1, 0),
	SOC_DAPM_SINGLE("Mic1 Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 6, 1, 0),
	SOC_DAPM_SINGLE("Mic2 Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 5, 1, 0),
	SOC_DAPM_SINGLE("Mic3 Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 4, 1, 0),
};

static const struct snd_kcontrol_new radc_mixer_controls[] = {
	SOC_DAPM_SINGLE("AuxR Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 3, 1, 0),
	SOC_DAPM_SINGLE("Mic1 Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 2, 1, 0),
	SOC_DAPM_SINGLE("Mic2 Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 1, 1, 0),
	SOC_DAPM_SINGLE("Mic3 Switch",
		RT5511_ANA_PATH_MIXER_CTRL_TO_PGA, 0, 1, 0),
};

static const struct snd_kcontrol_new left_hp_mix[] = {
	SOC_DAPM_SINGLE("LPGA Switch", RT5511_MIX_PGA, 7, 1, 0),
	SOC_DAPM_SINGLE("RPGA Switch", RT5511_MIX_PGA, 6, 1, 0),
};

static const struct snd_kcontrol_new right_hp_mix[] = {
	SOC_DAPM_SINGLE("LPGA Switch", RT5511_MIX_PGA, 5, 1, 0),
	SOC_DAPM_SINGLE("RPGA Switch", RT5511_MIX_PGA, 4, 1, 0),
};

static const struct snd_kcontrol_new left_spk_mix[] = {
	SOC_DAPM_SINGLE("LPGA Switch", RT5511_MIXER_ENABLE_CTRL, 7, 1, 0),
	SOC_DAPM_SINGLE("RPGA Switch", RT5511_MIXER_ENABLE_CTRL, 6, 1, 0),
	SOC_DAPM_SINGLE("LSPKDAC Switch", RT5511_RCV_CTRL, 1, 1, 0),
};

static const struct snd_kcontrol_new right_spk_mix[] = {
	SOC_DAPM_SINGLE("LPGA Switch", RT5511_MIXER_ENABLE_CTRL, 5, 1, 0),
	SOC_DAPM_SINGLE("RPGA Switch", RT5511_MIXER_ENABLE_CTRL, 4, 1, 0),
	SOC_DAPM_SINGLE("RSPKDAC Switch", RT5511_RCV_CTRL, 0, 1, 0),
};

static const char * const adc_mux_onoff[] = {
	"Off",
	"On",
};

static const struct soc_enum adc_mux_enum =
	SOC_ENUM_SINGLE(0, 0, 2, adc_mux_onoff);

static const struct snd_kcontrol_new lhp_to_left_hp_boost =
	SOC_DAPM_ENUM_VIRT("LHP Mixer Virt Mux", adc_mux_enum);

static const struct snd_kcontrol_new rhp_to_right_hp_boost =
	SOC_DAPM_ENUM_VIRT("RHP Mixer Virt Mux", adc_mux_enum);

static const struct snd_kcontrol_new lhpdac_to_left_hp_boost =
	SOC_DAPM_ENUM_VIRT("LHPDAC Virt Mux", adc_mux_enum);

static const struct snd_kcontrol_new rhpdac_to_right_hp_boost =
	SOC_DAPM_ENUM_VIRT("RHPDAC Virt Mux", adc_mux_enum);

static const struct snd_kcontrol_new lhpdac_to_left_rec =
	SOC_DAPM_ENUM_VIRT("LREC Virt Mux", adc_mux_enum);

static const struct snd_kcontrol_new rhpdac_to_right_rec =
	SOC_DAPM_ENUM_VIRT("RREC Virt Mux", adc_mux_enum);

static const struct snd_kcontrol_new left_hp_boost[] = {
	SOC_DAPM_SINGLE("Direct Voice Switch", RT5511_MIX_PGA, 1, 1, 0),
};

static const struct snd_kcontrol_new right_hp_boost[] = {
	SOC_DAPM_SINGLE("Direct Voice Switch", RT5511_MIX_PGA, 0, 1, 0),
};

static const struct snd_kcontrol_new rcv_boost[] = {
	SOC_DAPM_SINGLE("Direct Voice Switch", RT5511_RCV_CTRL, 4, 1, 0),
	SOC_DAPM_SINGLE("LSPK Switch", RT5511_RCV_CTRL, 6, 1, 0),
	SOC_DAPM_SINGLE("RSPK Switch", RT5511_RCV_CTRL, 5, 1, 0),
};

static const struct snd_kcontrol_new left_spk_boost[] = {
	SOC_DAPM_SINGLE("Direct Voice Switch",
		RT5511_SPK_BYPASS_BOOST, 7, 1, 0),
	SOC_DAPM_SINGLE("LSPK Switch",
		RT5511_MIXER_ENABLE_CTRL, 3, 1, 0),
	SOC_DAPM_SINGLE("RSPK Switch",
		RT5511_MIXER_ENABLE_CTRL, 2, 1, 0),
};

static const struct snd_kcontrol_new right_spk_boost[] = {
	SOC_DAPM_SINGLE("Direct Voice Switch",
		RT5511_SPK_BYPASS_BOOST, 3, 1, 0),
	SOC_DAPM_SINGLE("LSPK Switch",
		RT5511_MIXER_ENABLE_CTRL, 1, 1, 0),
	SOC_DAPM_SINGLE("RSPK Switch",
		RT5511_MIXER_ENABLE_CTRL, 0, 1, 0),
};

static inline int rt_select_spk_dab(struct snd_soc_codec *codec, int val)
{
	u32 regval;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	if (rt5511->revision >= RT5511_VID_5511A) {
		/*
		 * If SPK is still enabled,
		 * we can't change to AB type (avoid pop noise)
		 */
		if (val == RT5511_D_SPK_DAB_CLASS_AB) {
			regval = snd_soc_read(codec, RT5511_SPK_HP_DRIVER);
			if (regval & RT5511_D_SPK_EN_MASK)
				return 0;
		}

		snd_soc_update_bits(codec,
			RT5511_SPK_HP_DRIVER, RT5511_D_SPK_DAB_SEL_MASK, val);
	}

	return 0;
}

static int rt_spkdrv_pevent(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	struct rt_hubs_data *hubs = snd_soc_codec_get_drvdata(w->codec);

	RT_DBG("widget_event=> %s : %d\n", w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		hubs->eq_enable(w->codec, true);
		hubs->drc_dac_enable(w->codec, true);
		rt_select_spk_dab(w->codec, RT5511_D_SPK_DAB_CLASS_D);
		usleep_range(1000, 2000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		hubs->eq_enable(w->codec, false);
		hubs->drc_dac_enable(w->codec, false);
		rt_select_spk_dab(w->codec, RT5511_D_SPK_DAB_CLASS_AB);
		usleep_range(1000, 2000);
		break;
	}
	return 0;
}

static inline int rt_set_ampdet_src(struct snd_soc_codec *codec, int src)
{
	snd_soc_update_bits(codec,
		RT5511_D_AMPDET_FREQ, RT5511_AMPDET_SRC_SEL_MASK,
		src << RT5511_AMPDET_SRC_SEL_SHIFT);
	return 0;
}

static inline int rt_set_ampdet_vth(struct snd_soc_codec *codec, int val)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	if (rt5511->revision >= RT5511_VID_5511A) {
		snd_soc_update_bits(codec,
			RT5511_D_AMPDET_VTH, RT5511_D_AMPDET_VTH_MASK,
			val << RT5511_D_AMPDET_VTH_SHIFT);
	}
	return 0;
}

static int rt_enable_charge_pump(struct snd_soc_codec *codec, bool bOn)
{
	bool bChargePump;
	unsigned int retval;

	retval = snd_soc_read(codec, RT5511_SPK_HP_DRIVER);
	bChargePump = retval & RT5511_D_CP_EN;

	if (bOn == bChargePump) /* Skip delay twice */
		return 0;

	if (bOn) {
		snd_soc_update_bits(codec,
			RT5511_SPK_HP_DRIVER, RT5511_D_CP_EN, RT5511_D_CP_EN);
		usleep_range(2000, 3000);
	} else {
		/* Disable charge pump only in HP & RCV path both close */
		if (retval & (RT5511_D_LHP_EN|RT5511_D_RHP_EN))
			return 0;

		retval = snd_soc_read(codec, RT5511_RCV_CTRL);
		if (retval & RT5511_D_RCV_EN)
			return 0;

		usleep_range(1000, 2000);
		snd_soc_update_bits(codec,
			RT5511_SPK_HP_DRIVER, RT5511_D_CP_EN, 0x0);
		usleep_range(1000, 2000);
	}

	return 0;
}

static int rt_hpdrv_pevent(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	RT_DBG("widget_event=> %s : %d\n", w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rt_set_ampdet_src(w->codec, RT5511_AMPDET_SRC_SEL_HP);
		rt_set_ampdet_vth(w->codec, RT5511_D_AMPDET_VTH_250MV);
		rt_enable_charge_pump(w->codec, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rt_enable_charge_pump(w->codec, false);
		break;
	}
	return 0;
}

static int rt_recvdrv_pevent(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	RT_DBG("widget_event=> %s : %d\n", w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rt_select_spk_dab(w->codec, RT5511_D_SPK_DAB_CLASS_AB);
		rt_set_ampdet_src(w->codec, RT5511_AMPDET_SRC_SEL_RCV);
		rt_set_ampdet_vth(w->codec, RT5511_D_AMPDET_VTH_350MV);
		rt_enable_charge_pump(w->codec, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rt_select_spk_dab(w->codec, RT5511_D_SPK_DAB_CLASS_D);
		rt_enable_charge_pump(w->codec, false);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget analogue_dapm_widgets[] = {

	SND_SOC_DAPM_INPUT("MIC1P"),
	SND_SOC_DAPM_INPUT("MIC1N"),
	SND_SOC_DAPM_INPUT("MIC3P"),
	SND_SOC_DAPM_INPUT("MIC3N"),

	SND_SOC_DAPM_MICBIAS("MICBIAS1",
		RT5511_BIASE_MIC_CTRL, 5, 0),
	SND_SOC_DAPM_MICBIAS("MICBIAS2",
		RT5511_BIASE_MIC_CTRL, 4, 0),

/* Input PGA */
	SND_SOC_DAPM_PGA("Aux PGA",  RT5511_ADC_MIC, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC1 PGA", RT5511_ADC_MIC, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC2 PGA", RT5511_ADC_MIC, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC3 PGA", RT5511_ADC_MIC, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("Analog Bypass Mux",
		SND_SOC_NOPM, 0, 0, &analog_bypass_controls),
	SND_SOC_DAPM_PGA("Direct Voice", SND_SOC_NOPM, 0, 0, NULL, 0),

/* Input Mixer */
	SND_SOC_DAPM_MIXER("LADC Mixer", RT5511_ADC_MIC, 5, 0,
		ladc_mixer_controls, ARRAY_SIZE(ladc_mixer_controls)),
	SND_SOC_DAPM_MIXER("RADC Mixer", RT5511_ADC_MIC, 4, 0,
		radc_mixer_controls, ARRAY_SIZE(radc_mixer_controls)),

/* Output Mixer */
	SND_SOC_DAPM_MIXER("LHP Mixer", RT5511_MIX_EN_DAC_EN,
		7, 0, left_hp_mix, ARRAY_SIZE(left_hp_mix)),

	SND_SOC_DAPM_MIXER("RHP Mixer", RT5511_MIX_EN_DAC_EN, 6, 0,
		right_hp_mix, ARRAY_SIZE(right_hp_mix)),

	SND_SOC_DAPM_MIXER("LSPK Mixer", RT5511_MIX_EN_DAC_EN, 5, 0,
		left_spk_mix, ARRAY_SIZE(left_spk_mix)),

	SND_SOC_DAPM_MIXER("RSPK Mixer", RT5511_MIX_EN_DAC_EN, 4, 0,
		right_spk_mix, ARRAY_SIZE(right_spk_mix)),

	SND_SOC_DAPM_VIRT_MUX("LHP Mixer Virt Mux", SND_SOC_NOPM, 0, 0,
		&lhp_to_left_hp_boost),
	SND_SOC_DAPM_VIRT_MUX("RHP Mixer Virt Mux", SND_SOC_NOPM, 0, 0,
		&rhp_to_right_hp_boost),
	SND_SOC_DAPM_VIRT_MUX("LHPDAC Virt Mux", SND_SOC_NOPM, 0, 0,
		&lhpdac_to_left_hp_boost),
	SND_SOC_DAPM_VIRT_MUX("RHPDAC Virt Mux", SND_SOC_NOPM, 0, 0,
		&rhpdac_to_right_hp_boost),
	SND_SOC_DAPM_VIRT_MUX("LREC Virt Mux", SND_SOC_NOPM, 0, 0,
		&lhpdac_to_left_rec),
	SND_SOC_DAPM_VIRT_MUX("RREC Virt Mux", SND_SOC_NOPM, 0, 0,
		&rhpdac_to_right_rec),

/* Output Boost Driver */
	SND_SOC_DAPM_MIXER("HPL Boost", SND_SOC_NOPM, 0, 0,
		left_hp_boost, ARRAY_SIZE(left_hp_boost)),

	SND_SOC_DAPM_OUT_DRV_E("HPL Driver", RT5511_SPK_HP_DRIVER,
		1, 0, NULL, 0, rt_hpdrv_pevent,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("HPR Boost", SND_SOC_NOPM, 0, 0,
		right_hp_boost, ARRAY_SIZE(right_hp_boost)),

	SND_SOC_DAPM_OUT_DRV_E("HPR Driver", RT5511_SPK_HP_DRIVER,
		0, 0, NULL, 0, rt_hpdrv_pevent,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("RECV Boost", SND_SOC_NOPM, 0, 0,
		rcv_boost, ARRAY_SIZE(rcv_boost)),

	SND_SOC_DAPM_OUT_DRV_E("RECV Driver", RT5511_RCV_CTRL,
		7, 0, NULL, 0, rt_recvdrv_pevent,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("SPKL Boost", SND_SOC_NOPM, 0, 0,
		left_spk_boost, ARRAY_SIZE(left_spk_boost)),

	SND_SOC_DAPM_MIXER("SPKR Boost", SND_SOC_NOPM, 0, 0,
		right_spk_boost, ARRAY_SIZE(right_spk_boost)),

	SND_SOC_DAPM_OUT_DRV_E("SPKL Driver", RT5511_SPK_HP_DRIVER,
		3, 0, NULL, 0, rt_spkdrv_pevent,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUT_DRV_E("SPKR Driver", RT5511_SPK_HP_DRIVER,
		2, 0, NULL, 0, rt_spkdrv_pevent,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

/* Output Pin */
	SND_SOC_DAPM_OUTPUT("SPKOUTLP"),
	SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
	SND_SOC_DAPM_OUTPUT("SPKOUTRP"),
	SND_SOC_DAPM_OUTPUT("SPKOUTRN"),
	SND_SOC_DAPM_OUTPUT("HPOUT1L"),
	SND_SOC_DAPM_OUTPUT("HPOUT1R"),
	SND_SOC_DAPM_OUTPUT("HPOUT2P"),	/* Recv Output */
	SND_SOC_DAPM_OUTPUT("HPOUT2N"),	/* Recv Output */
	SND_SOC_DAPM_OUTPUT("RECOUTL"),	/* Virt Output */
	SND_SOC_DAPM_OUTPUT("RECOUTR"),	/* Virt Output */
};

static const struct snd_soc_dapm_route analogue_routes[] = {

	{ "Analog Bypass Mux", "Mic1", "MIC1P" },
	{ "Analog Bypass Mux", "Mic1", "MIC1N" },	/* As MIC2P */
	{ "Analog Bypass Mux", "Mic3", "MIC3P" },
	{ "Analog Bypass Mux", "Mic3", "MIC3N" },

	{ "Direct Voice", NULL, "Analog Bypass Mux" },

	{ "LADC Mixer", "AuxL Switch", "Aux PGA" },
	{ "LADC Mixer", "Mic1 Switch", "MIC1 PGA" },
	{ "LADC Mixer", "Mic2 Switch", "MIC2 PGA" },
	{ "LADC Mixer", "Mic3 Switch", "MIC3 PGA" },

	{ "RADC Mixer", "AuxR Switch", "Aux PGA" },
	{ "RADC Mixer", "Mic1 Switch", "MIC1 PGA" },
	{ "RADC Mixer", "Mic2 Switch", "MIC2 PGA" },
	{ "RADC Mixer", "Mic3 Switch", "MIC3 PGA" },

	{ "LADC", NULL, "LADC Mixer" },
	{ "RADC", NULL, "RADC Mixer" },

	{ "LHP Mixer", "LPGA Switch", "LADC Mixer" },
	{ "LHP Mixer", "RPGA Switch", "RADC Mixer" },

	{ "RHP Mixer", "LPGA Switch", "LADC Mixer" },
	{ "RHP Mixer", "RPGA Switch", "RADC Mixer" },

	{ "LSPK Mixer", "LPGA Switch", "LADC Mixer" },
	{ "LSPK Mixer", "RPGA Switch", "RADC Mixer" },

	{ "RSPK Mixer", "LPGA Switch", "LADC Mixer" },
	{ "RSPK Mixer", "RPGA Switch", "RADC Mixer" },

	{ "SPKL Boost", "LSPK Switch", "LSPK Mixer" },
	{ "SPKL Boost", "RSPK Switch", "RSPK Mixer" },
	{ "SPKL Boost", "Direct Voice Switch", "Direct Voice" },

	{ "SPKR Boost", "LSPK Switch", "LSPK Mixer" },
	{ "SPKR Boost", "RSPK Switch", "RSPK Mixer" },
	{ "SPKR Boost", "Direct Voice Switch", "Direct Voice" },

	{ "SPKL Driver", NULL, "SPKL Boost" },
	{ "SPKR Driver", NULL, "SPKR Boost" },

	{ "SPKOUTLP", NULL, "SPKL Driver" },
	{ "SPKOUTLN", NULL, "SPKL Driver" },
	{ "SPKOUTRP", NULL, "SPKR Driver" },
	{ "SPKOUTRN", NULL, "SPKR Driver" },

	{ "LHP Mixer Virt Mux", "On", "LHP Mixer" },
	{ "RHP Mixer Virt Mux", "On", "RHP Mixer" },

	{ "HPL Boost", NULL, "LHPDAC Virt Mux" },
	{ "HPL Boost", NULL, "LHP Mixer Virt Mux" },
	{ "HPL Boost", "Direct Voice Switch", "Direct Voice" },

	{ "HPR Boost", NULL, "RHPDAC Virt Mux" },
	{ "HPR Boost", NULL, "RHP Mixer Virt Mux" },
	{ "HPR Boost", "Direct Voice Switch", "Direct Voice" },

	{ "HPL Driver", NULL, "HPL Boost" },
	{ "HPR Driver", NULL, "HPR Boost" },

	{ "HPOUT1L", NULL, "HPL Driver" },
	{ "HPOUT1R", NULL, "HPR Driver" },

	{ "RECV Boost", "Direct Voice Switch", "Direct Voice" },
	{ "RECV Boost", "LSPK Switch", "LSPK Mixer" },
	{ "RECV Boost", "RSPK Switch", "RSPK Mixer" },

	{ "RECV Driver", NULL, "RECV Boost" },

	{ "HPOUT2P", NULL, "RECV Driver" },
	{ "HPOUT2N", NULL, "RECV Driver" },

	{ "RECOUTL", NULL, "LREC Virt Mux" },
	{ "RECOUTR", NULL, "RREC Virt Mux" },
};

static const struct snd_soc_dapm_route mic1_diff_routes[] = {
	{ "MIC1 PGA", "", "MIC1P" },
	{ "MIC2 PGA", "", "MIC1N" },

	{ "MIC1 PGA", "", "MIC1N" },
};

static const struct snd_soc_dapm_route mic1_se_routes[] = {
	{ "MIC1 PGA", "", "MIC1P" },
	{ "MIC2 PGA", "", "MIC1N" },
};

static const struct snd_soc_dapm_route mic3_diff_routes[] = {
	{ "MIC3 PGA", "", "MIC3P" },
	{ "MIC3 PGA", "", "MIC3N" },
};

static const struct snd_soc_dapm_route mic3_se_routes[] = {
	{ "MIC3 PGA", "", "MIC3P" },
};

int rt_hubs_add_analogue_controls(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* TBD: default volume & ZC on */

	snd_soc_add_controls(codec, analogue_snd_controls,
			     ARRAY_SIZE(analogue_snd_controls));

	snd_soc_dapm_new_controls(dapm, analogue_dapm_widgets,
				  ARRAY_SIZE(analogue_dapm_widgets));

	return 0;
}
EXPORT_SYMBOL_GPL(rt_hubs_add_analogue_controls);

int rt_hubs_add_analogue_routes(struct snd_soc_codec *codec)
{
	struct rt_hubs_data *hubs = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_add_routes(dapm, analogue_routes,
				ARRAY_SIZE(analogue_routes));

	if (hubs->mic_diff[0])
		snd_soc_dapm_add_routes(dapm, mic1_diff_routes,
					ARRAY_SIZE(mic1_diff_routes));
	else
		snd_soc_dapm_add_routes(dapm, mic1_se_routes,
					ARRAY_SIZE(mic1_se_routes));

	if (hubs->mic_diff[2])
		snd_soc_dapm_add_routes(dapm, mic3_diff_routes,
					ARRAY_SIZE(mic3_diff_routes));
	else
		snd_soc_dapm_add_routes(dapm, mic3_se_routes,
					ARRAY_SIZE(mic3_se_routes));

	return 0;
}
EXPORT_SYMBOL_GPL(rt_hubs_add_analogue_routes);

int rt_hubs_handle_analogue_pdata(struct snd_soc_codec *codec,
	int mic1_diff, int mic2_diff, int mic3_diff,
	int micbias1_lvl, int micbias2_lvl)
{
	int i;
	int regval;
	struct rt_hubs_data *hubs = snd_soc_codec_get_drvdata(codec);

	hubs->mic_diff[0] = mic1_diff;
	hubs->mic_diff[1] = mic2_diff;
	hubs->mic_diff[2] = mic3_diff;

	for (i = 0; i < 3; i++) {
		snd_soc_update_bits(codec,
			RT5511_MIC1_CTRL + i, RT5511_MIC_DIFF_EN,
			hubs->mic_diff[i] ? RT5511_MIC_DIFF_EN : 0);
	};

	regval = (micbias1_lvl << RT5511_D_MICBIAS1_LVL_SHIFT);
	regval |= micbias2_lvl;

	snd_soc_update_bits(codec, RT5511_MICBIAS_CTRL,
		RT5511_D_MICBIAS1_LVL_MASK | RT5511_D_MICBIAS2_LVL_MASK,
		regval);

	return 0;
}
EXPORT_SYMBOL_GPL(rt_hubs_handle_analogue_pdata);

void rt_hubs_vmid_ena(struct snd_soc_codec *codec)
{
	/* TBD */
}
EXPORT_SYMBOL_GPL(rt_hubs_vmid_ena);

void rt_hubs_set_bias_level(struct snd_soc_codec *codec,
			    enum snd_soc_bias_level level)
{
	/* TBD */
}
EXPORT_SYMBOL_GPL(rt_hubs_set_bias_level);

MODULE_DESCRIPTION("Shared support for Richtek hubs products");
MODULE_AUTHOR("Tsunghan Tsai <tsunghan_tsai@richtek.com>");
MODULE_LICENSE("GPL");
