/* sound/soc/codecs/rt5511-dsp2.c
 * RT5511 DSP functions
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
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <trace/events/asoc.h>

#include <linux/version.h>
#include <linux/mfd/rt5511/core.h>
#include <linux/mfd/rt5511/registers.h>
#include <linux/mfd/rt5511/pdata.h>

#include "rt5511.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
/* > 3.4.0 ==> use codec add controls instead of the original one */

static inline int snd_soc_add_controls(struct snd_soc_codec *codec,
	const struct snd_kcontrol_new *controls, int num_controls)
{
	return snd_soc_add_codec_controls(codec, controls, num_controls);
}
#endif

static void rt5511_dsp_apply(struct snd_soc_codec *codec, int path, int start)
{
	/* do nothing now */
}

int rt5511_aif_ev(struct snd_soc_dapm_widget *w,
		  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int i;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_PRE_PMU:
		for (i = 0; i < 3; i++)
			rt5511_dsp_apply(codec, i, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		for (i = 0; i < 3; i++)
			rt5511_dsp_apply(codec, i, 0);
		break;
	}

	return 0;
}

/* Check if DSP2 is in use on another AIF */

static int rt5511_mbc_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int rt5511_mbc_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int mbc = kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5511->mbc_ena[mbc];

	return 0;
}

static int rt5511_mbc_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

#define RT5511_MBC_SWITCH(xname, xval) {\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,\
	.info = rt5511_mbc_info, \
	.get = rt5511_mbc_get, .put = rt5511_mbc_put, \
	.private_value = xval }

int rt5511_put_vss_enum(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	rt5511->vss_cfg = value;

	return 0;
}

int rt5511_get_vss_enum(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = rt5511->vss_cfg;

	return 0;
}

static int rt5511_vss_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int rt5511_vss_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int vss = kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5511->vss_ena[vss];

	return 0;
}

static int rt5511_vss_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int vss = kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	if (rt5511->vss_ena[vss] == ucontrol->value.integer.value[0])
		return 0;


	return 0;
}


#define RT5511_VSS_SWITCH(xname, xval) {\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,\
	.info = rt5511_vss_info, \
	.get = rt5511_vss_get, .put = rt5511_vss_put, \
	.private_value = xval }

static int rt5511_hpf_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int rt5511_hpf_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int rt5511_hpf_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

#define RT5511_HPF_SWITCH(xname, xval) {\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,\
	.info = rt5511_hpf_info, \
	.get = rt5511_hpf_get, .put = rt5511_hpf_put, \
	.private_value = xval }

int rt5511_put_enh_eq_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];
	rt5511->enh_eq_cfg = value;
	return 0;
}

int rt5511_get_enh_eq_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = rt5511->enh_eq_cfg;

	return 0;
}

static int rt5511_enh_eq_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int rt5511_enh_eq_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int eq = kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5511->enh_eq_ena[eq];

	return 0;
}

static int rt5511_enh_eq_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

#define RT5511_ENH_EQ_SWITCH(xname, xval) {\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,\
	.info = rt5511_enh_eq_info, \
	.get = rt5511_enh_eq_get, .put = rt5511_enh_eq_put, \
	.private_value = xval }

static const struct snd_kcontrol_new rt5511_mbc_snd_controls[] = {
RT5511_MBC_SWITCH("AIF1DAC1 MBC Switch", 0),
RT5511_MBC_SWITCH("AIF1DAC2 MBC Switch", 1),
RT5511_MBC_SWITCH("AIF2DAC MBC Switch", 2),
};

static const struct snd_kcontrol_new rt5511_vss_snd_controls[] = {
RT5511_VSS_SWITCH("AIF1DAC1 VSS Switch", 0),
RT5511_VSS_SWITCH("AIF1DAC2 VSS Switch", 1),
RT5511_VSS_SWITCH("AIF2DAC VSS Switch", 2),
RT5511_HPF_SWITCH("AIF1DAC1 HPF1 Switch", 0),
RT5511_HPF_SWITCH("AIF1DAC2 HPF1 Switch", 1),
RT5511_HPF_SWITCH("AIF2DAC HPF1 Switch", 2),
RT5511_HPF_SWITCH("AIF1DAC1 HPF2 Switch", 3),
RT5511_HPF_SWITCH("AIF1DAC2 HPF2 Switch", 4),
RT5511_HPF_SWITCH("AIF2DAC HPF2 Switch", 5),
};

static const struct snd_kcontrol_new rt5511_enh_eq_snd_controls[] = {
RT5511_ENH_EQ_SWITCH("AIF1DAC1 Enhanced EQ Switch", 0),
RT5511_ENH_EQ_SWITCH("AIF1DAC2 Enhanced EQ Switch", 1),
RT5511_ENH_EQ_SWITCH("AIF2DAC Enhanced EQ Switch", 2),
};


void rt5511_dsp2_init(struct snd_soc_codec *codec)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	struct rt5511_pdata *pdata = rt5511->pdata;

	rt5511->dsp_active = -1;

	snd_soc_add_controls(codec, rt5511_mbc_snd_controls,
			     ARRAY_SIZE(rt5511_mbc_snd_controls));
	snd_soc_add_controls(codec, rt5511_vss_snd_controls,
			     ARRAY_SIZE(rt5511_vss_snd_controls));
	snd_soc_add_controls(codec, rt5511_enh_eq_snd_controls,
			     ARRAY_SIZE(rt5511_enh_eq_snd_controls));

	if (!pdata)
		return;
    /* We don't provide DSP function now */
    /* DRC / EQ / HPF are built-in by HW */
}
