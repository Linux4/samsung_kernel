/*
 * sound/soc/sprd/dai/vbc/r1p0/vbc-codec.c
 *
 * SPRD SoC VBC Codec -- SpreadTrum SOC VBC Codec function.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "sprd-asoc-common.h"
#include "vbc-codec.h"

#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define SOC_REG(r) ((unsigned short)(r))

struct vbc_fw_header {
	char magic[VBC_EQ_FIRMWARE_MAGIC_LEN];
	u32 profile_version;
	u32 num_profile;
};

struct vbc_eq_profile {
	char magic[VBC_EQ_FIRMWARE_MAGIC_LEN];
	char name[VBC_EQ_PROFILE_NAME_MAX];
	/* TODO */
	u32 effect_paras[VBC_EFFECT_PARAS_LEN];
};

static const u32 vbc_eq_profile_default[VBC_EFFECT_PARAS_LEN] = {
/* TODO the default register value */
	0x00000000,		/*  DAPATCHCTL      */
	0x00001818,		/*  DADGCTL         */
	0x0000007F,		/*  DAHPCTL         */
	0x00000000,		/*  DAALCCTL0       */
	0x00000000,		/*  DAALCCTL1       */
	0x00000000,		/*  DAALCCTL2       */
	0x00000000,		/*  DAALCCTL3       */
	0x00000000,		/*  DAALCCTL4       */
	0x00000000,		/*  DAALCCTL5       */
	0x00000000,		/*  DAALCCTL6       */
	0x00000000,		/*  DAALCCTL7       */
	0x00000000,		/*  DAALCCTL8       */
	0x00000000,		/*  DAALCCTL9       */
	0x00000000,		/*  DAALCCTL10      */
	0x00000183,		/*  STCTL0          */
	0x00000183,		/*  STCTL1          */
	0x00000000,		/*  ADPATCHCTL      */
	0x00001818,		/*  ADDGCTL         */
	0x00000000,		/*  HPCOEF0         */
};

struct vbc_equ {
	struct device *dev;
	struct snd_soc_codec *codec;
	int is_active;
	int is_loading;
	int valid;
	int now_profile;
	struct vbc_fw_header hdr;
	struct vbc_eq_profile *data;
	void (*vbc_eq_apply) (struct snd_soc_codec * codec, void *data,
			      int vbc_idx);
};

static void vbc_eq_try_apply(struct snd_soc_codec *codec, int vbc_idx);

#define VBC_DG_VAL_MAX (0x7F)

typedef int (*vbc_dg_set) (int enable, int dg);
struct vbc_dg {
	int dg_switch[2];
	int dg_val[2];
	vbc_dg_set dg_set[2];
};

struct st_hpf_dg {
	int hpf_switch[2];
	int dg_val[2];
	int hpf_val[2];
};

/*vbc mux setting*/
enum {
	SPRD_VBC_MUX_START = 0,
	SPRD_VBC_AD0_INMUX,
	SPRD_VBC_AD1_INMUX,
	SPRD_VBC_AD_IISMUX,
	SPRD_VBC_MUX_MAX
};

#define IS_SPRD_VBC_MUX_RANG(reg) ((reg) >= SPRD_VBC_MUX_START && (reg) < (SPRD_VBC_MUX_MAX))
#define SPRD_VBC_MUX_IDX(reg) (reg - SPRD_VBC_MUX_START)

static const char *vbc_mux_debug_str[SPRD_VBC_MUX_MAX] = {
	"ad0 inmux",
	"ad1 inmux",
	"ad iis mux",
};

typedef int (*sprd_vbc_mux_set) (int sel);
struct sprd_vbc_mux_op {
	int val;
	sprd_vbc_mux_set set;
};

enum {
	VBC_LOOP_SWITCH_START = SPRD_VBC_MUX_MAX,
	VBC_AD_LOOP_SWITCH = VBC_LOOP_SWITCH_START,
	VBC_LOOP_SWITCH_MAX
};

#define IS_SPRD_VBC_SWITCH_RANG(reg) ((reg) >= VBC_LOOP_SWITCH_START && (reg) < (VBC_LOOP_SWITCH_MAX))
#define SPRD_VBC_SWITCH_IDX(reg) (reg - VBC_LOOP_SWITCH_START)

struct vbc_codec_priv {
	struct snd_soc_codec *codec;
	struct vbc_equ vbc_eq_setting;
	struct mutex load_mutex;
	int vbc_control;
	int vbc_loop_switch[SPRD_VBC_SWITCH_IDX(VBC_LOOP_SWITCH_MAX)];
	struct vbc_dg dg[VBC_IDX_MAX];
	int vbc_da_iis_port;
	int adc_dgmux_val[ADC_DGMUX_MAX];
	struct st_hpf_dg st_dg;
	struct sprd_vbc_mux_op sprd_vbc_mux[SPRD_VBC_MUX_MAX];
};

static int vbc_ad0_inmux_set(int val)
{
	vbc_reg_update(ADPATCHCTL, val << VBADPATH_AD0_INMUX_SHIFT,
		       VBADPATH_AD0_INMUX_MASK);
	return 0;
}

static int vbc_ad1_inmux_set(int val)
{
	vbc_reg_update(ADPATCHCTL, val << VBADPATH_AD1_INMUX_SHIFT,
		       VBADPATH_AD1_INMUX_MASK);
	return 0;
}

static int vbc_ad0_dgmux_set(int val)
{
	vbc_reg_update(ADPATCHCTL, val << VBADPATH_AD0_DGMUX_SHIFT,
		       VBADPATH_AD0_DGMUX_MASK);
	return 0;
}

static int vbc_ad1_dgmux_set(int val)
{
	vbc_reg_update(ADPATCHCTL, val << VBADPATH_AD1_DGMUX_SHIFT,
		       VBADPATH_AD1_DGMUX_MASK);
	return 0;
}

static int vbc_ad_iismux_set(int port)
{
	vbc_reg_update(VBIISSEL, port << VBIISSEL_AD_PORT_SHIFT,
		       VBIISSEL_AD_PORT_MASK);
	return 0;
}

static int vbc_da_iismux_set(int port)
{
	vbc_reg_update(VBIISSEL, port << VBIISSEL_DA_PORT_SHIFT,
		       VBIISSEL_DA_PORT_MASK);
	return 0;
}

static int vbc_try_ad_iismux_set(int port);

static sprd_vbc_mux_set vbc_mux_cfg[SPRD_VBC_MUX_MAX] = {
	vbc_ad0_inmux_set,
	vbc_ad1_inmux_set,
	vbc_try_ad_iismux_set,
};

static inline int vbc_da0_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_update(DADGCTL, 0x80 | (0xFF & dg), 0xFF);
	} else {
		vbc_reg_update(DADGCTL, 0, 0x80);
	}
	return 0;
}

static inline int vbc_da1_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_update(DADGCTL, (0x80 | (0xFF & dg)) << 8, 0xFF00);
	} else {
		vbc_reg_update(DADGCTL, 0, 0x8000);
	}
	return 0;
}

static inline int vbc_ad0_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_update(ADDGCTL, 0x80 | (0xFF & dg), 0xFF);
	} else {
		vbc_reg_update(ADDGCTL, 0, 0x80);
	}
	return 0;
}

static inline int vbc_ad1_dg_set(int enable, int dg)
{
	if (enable) {
		vbc_reg_update(ADDGCTL, (0x80 | (0xFF & dg)) << 8, 0xFF00);
	} else {
		vbc_reg_update(ADDGCTL, 0, 0x8000);
	}
	return 0;
}

static inline int vbc_st0_dg_set(int dg)
{
	vbc_reg_update(STCTL0, (0x7F & dg) << 4, 0x7F0);
	return 0;
}

static inline int vbc_st1_dg_set(int dg)
{
	vbc_reg_update(STCTL1, (0x7F & dg) << 4, 0x7F0);
	return 0;
}

static inline int vbc_st0_hpf_set(int enable, int hpf_val)
{
	if (enable) {
		vbc_reg_update(STCTL0, BIT(VBST_HPF_0), BIT(VBST_HPF_0));
		vbc_reg_update(STCTL0, 0xF & hpf_val, 0xF);
	} else {
		vbc_reg_update(STCTL0, 0, BIT(VBST_HPF_0));
		vbc_reg_update(STCTL0, 3, 0xF);
	}
	return 0;
}

static inline int vbc_st1_hpf_set(int enable, int hpf_val)
{
	if (enable) {
		vbc_reg_update(STCTL1, BIT(VBST_HPF_1), BIT(VBST_HPF_1));
		vbc_reg_update(STCTL1, 0xF & hpf_val, 0xF);
	} else {
		vbc_reg_update(STCTL1, 0, BIT(VBST_HPF_1));
		vbc_reg_update(STCTL1, 3, 0xF);
	}
	return 0;
}

static int vbc_try_dg_set(struct vbc_codec_priv *vbc_codec, int vbc_idx, int id)
{
	struct vbc_dg *p_dg = &vbc_codec->dg[vbc_idx];
	int dg = p_dg->dg_val[id];
	if (p_dg->dg_switch[id]) {
		p_dg->dg_set[id] (1, dg);
	} else {
		p_dg->dg_set[id] (0, dg);
	}
	return 0;
}

static int vbc_try_st_dg_set(struct vbc_codec_priv *vbc_codec, int id)
{
	struct st_hpf_dg *p_st_dg = &vbc_codec->st_dg;
	if (id == VBC_LEFT) {
		vbc_st0_dg_set(p_st_dg->dg_val[id]);
	} else {
		vbc_st1_dg_set(p_st_dg->dg_val[id]);
	}
	return 0;
}

static int vbc_try_st_hpf_set(struct vbc_codec_priv *vbc_codec, int id)
{
	struct st_hpf_dg *p_st_dg = &vbc_codec->st_dg;
	if (id == VBC_LEFT) {
		vbc_st0_hpf_set(p_st_dg->hpf_switch[id], p_st_dg->hpf_val[id]);
	} else {
		vbc_st1_hpf_set(p_st_dg->hpf_switch[id], p_st_dg->hpf_val[id]);
	}
	return 0;
}

static int vbc_try_da_iismux_set(int port)
{
	return vbc_da_iismux_set(port ? (port + 1) : 0);
}

static int vbc_try_ad_iismux_set(int port)
{
	return vbc_ad_iismux_set(port);
}

static int vbc_try_ad_dgmux_set(struct vbc_codec_priv *vbc_codec, int id)
{
	switch (id) {
	case ADC0_DGMUX:
		vbc_ad0_dgmux_set(vbc_codec->adc_dgmux_val[ADC0_DGMUX]);
		break;
	case ADC1_DGMUX:
		vbc_ad1_dgmux_set(vbc_codec->adc_dgmux_val[ADC1_DGMUX]);
		break;
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_SND_SOC_SPRD_AUDIO_DEBUG
static const char *get_event_name(int event)
{
	const char *ev_name;
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ev_name = "PRE_PMU";
		break;
	case SND_SOC_DAPM_POST_PMU:
		ev_name = "POST_PMU";
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ev_name = "PRE_PMD";
		break;
	case SND_SOC_DAPM_POST_PMD:
		ev_name = "POST_PMD";
		break;
	default:
		BUG();
		return 0;
	}
	return ev_name;
}
#endif

static const char *ad0_inmux_txt[] = {
	"IIS0AD0", "IIS1AD0", "NOINPUT",
};

static const char *ad1_inmux_txt[] = {
	"IIS1AD1", "IIS0AD1", "NOINPUT",
};

static const char *ad_iis_txt[] = {
	"AUDIIS0", "DIGFM",
};

#define SPRD_VBC_ENUM(xreg, xmax, xtexts)\
		  SOC_ENUM_SINGLE(FUN_REG(xreg), 0, xmax, xtexts)

static const struct soc_enum vbc_mux_sel_enum[SPRD_VBC_MUX_MAX] = {
	/*AD INMUX */
	SPRD_VBC_ENUM(SPRD_VBC_AD0_INMUX, 4, ad0_inmux_txt),
	SPRD_VBC_ENUM(SPRD_VBC_AD1_INMUX, 4, ad1_inmux_txt),
	/*IIS INMUX */
	SPRD_VBC_ENUM(SPRD_VBC_AD_IISMUX, 4, ad_iis_txt),
};

static int vbc_chan_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	int vbc_idx = FUN_REG(w->reg);
	int chan = w->shift;
	int ret = 0;

	sp_asoc_pr_dbg("%s(%s%d) Event is %s\n", __func__,
		       vbc_get_name(vbc_idx), chan, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		vbc_chan_enable(1, vbc_idx, chan);
		break;
	case SND_SOC_DAPM_POST_PMD:
		vbc_chan_enable(0, vbc_idx, chan);
		break;
	default:
		BUG();
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int vbc_power_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		vbc_power(1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		vbc_power(0);
		break;
	default:
		BUG();
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int dfm_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n", __func__, get_event_name(event));
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		fm_set_vbc_buffer_size();	/*No use in FM function, just for debug VBC */
		vbc_try_st_dg_set(vbc_codec, VBC_LEFT);
		vbc_try_st_dg_set(vbc_codec, VBC_RIGHT);
		if (!p_eq_setting->codec)
			p_eq_setting->codec = codec;
		/*eq setting */
		if (p_eq_setting->is_active && p_eq_setting->data)
			vbc_eq_try_apply(codec, VBC_PLAYBACK);
		vbc_enable(1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		vbc_enable(0);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int aud_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int vbc_idx = FUN_REG(w->reg);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	int ret = 0;

	sp_asoc_pr_dbg("%s Event is %s\n Chan is %s", __func__,
		       get_event_name(event), vbc_get_name(vbc_idx));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/*eq setting */
		if (!p_eq_setting->codec)
			p_eq_setting->codec = codec;
		if (p_eq_setting->is_active && p_eq_setting->data)
			vbc_eq_try_apply(codec, vbc_idx);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int mux_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int id = SPRD_VBC_MUX_IDX(FUN_REG(w->reg));
	struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[id]);
	int ret = 0;

	sp_asoc_pr_dbg("%s Set %s(%d) Event is %s\n", __func__,
		       vbc_mux_debug_str[id], mux->val, get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mux->set = vbc_mux_cfg[id];
		ret = mux->set(mux->val);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		mux->set = 0;
		ret = vbc_mux_cfg[id] (0);
		break;
	default:
		BUG();
		ret = -EINVAL;
	}

	return ret;
}

static int sprd_vbc_mux_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = SPRD_VBC_MUX_IDX(FUN_REG(e->reg));
	struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[reg]);

	ucontrol->value.enumerated.item[0] = mux->val;

	return 0;
}

static int sprd_vbc_mux_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = SPRD_VBC_MUX_IDX(FUN_REG(e->reg));
	unsigned int max = e->max;
	unsigned int mask = (1 << fls(max)) - 1;
	struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[reg]);
	int ret = 0;

	if (mux->val == ucontrol->value.enumerated.item[0])
		return 0;

	sp_asoc_pr_info("Set MUX[%s] to %d\n", vbc_mux_debug_str[reg],
			ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.enumerated.item[0] > e->max - 1)
		return -EINVAL;

	/*notice the sequence */
	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

	/*update reg: must be set after snd_soc_dapm_put_enum_double->change = snd_soc_test_bits(widget->codec, e->reg, mask, val); */
	mux->val = (ucontrol->value.enumerated.item[0] & mask);
	if (mux->set) {
		ret = mux->set(mux->val);
	}

	return ret;
}

#define SPRD_VBC_MUX(xname, xenum) \
	 SOC_DAPM_ENUM_EXT(xname, xenum, sprd_vbc_mux_get, sprd_vbc_mux_put)

static const struct snd_kcontrol_new vbc_mux[SPRD_VBC_MUX_MAX] = {
	SPRD_VBC_MUX("AD0 INMUX", vbc_mux_sel_enum[SPRD_VBC_AD0_INMUX]),
	SPRD_VBC_MUX("AD1 INMUX", vbc_mux_sel_enum[SPRD_VBC_AD1_INMUX]),
	SPRD_VBC_MUX("AD IISMUX", vbc_mux_sel_enum[SPRD_VBC_AD_IISMUX]),
};

#define VBC_DAPM_MUX_E(wname, wreg) \
	SND_SOC_DAPM_MUX_E(wname, FUN_REG(wreg), 0, 0, &vbc_mux[wreg], mux_event, \
							SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD)

static int vbc_loop_switch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = SPRD_VBC_SWITCH_IDX(FUN_REG(mc->reg));
	ucontrol->value.integer.value[0] = vbc_codec->vbc_loop_switch[id];
	return 0;
}

static int vbc_loop_switch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = SPRD_VBC_SWITCH_IDX(FUN_REG(mc->reg));

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->vbc_loop_switch[id]) {
		return ret;
	}

	sp_asoc_pr_info("VBC AD LOOP Switch Set %x\n",
			(int)ucontrol->value.integer.value[0]);

	snd_soc_dapm_put_volsw(kcontrol, ucontrol);

	vbc_codec->vbc_loop_switch[id] = ret;

	return ret;
}

static const struct snd_kcontrol_new vbc_loop_control[] = {
	SOC_SINGLE_EXT("Switch",
		       FUN_REG(VBC_AD_LOOP_SWITCH),
		       0, 1, 0,
		       vbc_loop_switch_get,
		       vbc_loop_switch_put),
};

static const struct snd_soc_dapm_widget vbc_codec_dapm_widgets[] = {
	/*power */
	SND_SOC_DAPM_SUPPLY("VBC Power", SND_SOC_NOPM, 0, 0,
			    vbc_power_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*ADC inmux */
	VBC_DAPM_MUX_E("AD0 INMUX", SPRD_VBC_AD0_INMUX),
	VBC_DAPM_MUX_E("AD1 INMUX", SPRD_VBC_AD1_INMUX),

	/*IIS MUX */
	VBC_DAPM_MUX_E("AD IISMUX", SPRD_VBC_AD_IISMUX),

	/*ST Switch */
	SND_SOC_DAPM_PGA_S("ST0 Switch", 4, SOC_REG(STCTL0), VBST_EN_0, 0, NULL,
			   0),
	SND_SOC_DAPM_PGA_S("ST1 Switch", 4, SOC_REG(STCTL1), VBST_EN_1, 0, NULL,
			   0),

	/*FM Mixer */
	SND_SOC_DAPM_PGA_S("DA0 FM Mixer", 4, SOC_REG(DAPATCHCTL),
			   VBDAPATH_DA0_ADDFM_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("DA1 FM Mixer", 4, SOC_REG(DAPATCHCTL),
			   VBDAPATH_DA1_ADDFM_SHIFT, 0, NULL, 0),

	SND_SOC_DAPM_LINE("DFM", dfm_event),

	/*VBC Chan Switch */
	SND_SOC_DAPM_PGA_S("DA0 Switch", 5, FUN_REG(VBC_PLAYBACK), VBC_LEFT, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("DA1 Switch", 5, FUN_REG(VBC_PLAYBACK), VBC_RIGHT, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("AD0 Switch", 5, FUN_REG(VBC_CAPTRUE), VBC_LEFT, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("AD1 Switch", 5, FUN_REG(VBC_CAPTRUE), VBC_RIGHT, 0,
			   vbc_chan_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*AUD loop var vbc switch */
	SND_SOC_DAPM_PGA_S("Aud input", 4, FUN_REG(VBC_CAPTRUE), 0, 0,
			   aud_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SWITCH("Aud Loop in VBC", SND_SOC_NOPM, 0, 0,
			    &vbc_loop_control[SPRD_VBC_SWITCH_IDX
					      (VBC_AD_LOOP_SWITCH)]),
};

/* sprd_vbc supported interconnection*/
static const struct snd_soc_dapm_route vbc_codec_intercon[] = {
	/************************power********************************/
	/* digital fm playback need to open DA Clk and DA power */
	{"DFM", NULL, "VBC Power"},

	/********************** capture in  path in vbc ********************/
	/* AD input route */
	{"Aud Loop in VBC", "Switch", "Aud input"},

	/* AD */
	{"AD IISMUX", "DIGFM", "Dig FM Jack"},
	{"AD IISMUX", "AUDIIS0", "Aud Loop in VBC"},

	{"AD0 INMUX", "IIS0AD0", "AD IISMUX"},
	{"AD0 INMUX", "IIS1AD0", "AD IISMUX"},
	{"AD1 INMUX", "IIS1AD1", "AD IISMUX"},
	{"AD1 INMUX", "IIS0AD1", "AD IISMUX"},

	{"AD0 Switch", NULL, "AD0 INMUX"},
	{"AD1 Switch", NULL, "AD1 INMUX"},

	/********************** fm playback route ********************/
	/*ST route */
	{"ST0 INMUX", NULL, "AD0 Switch"},
	{"ST1 INMUX", NULL, "AD1 Switch"},

	{"ST0 Switch", NULL, "ST0 INMUX"},
	{"ST1 Switch", NULL, "ST1 INMUX"},
	{"DA0 Switch", NULL, "ST0 Switch"},
	{"DA1 Switch", NULL, "ST1 Switch"},
	{"DA0 FM Mixer", NULL, "DA0 Switch"},
	{"DA1 FM Mixer", NULL, "DA1 Switch"},
	{"DFM", NULL, "DA0 FM Mixer"},
	{"DFM", NULL, "DA1 FM Mixer"},
};

int vbc_codec_startup(int vbc_idx, struct snd_soc_dai *dai)
{
	struct vbc_codec_priv *vbc_codec = dev_get_drvdata(dai->dev);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	sp_asoc_pr_dbg("%s %s\n", __func__, vbc_get_name(vbc_idx));

	switch (vbc_idx) {
	case VBC_PLAYBACK:
		vbc_try_da_iismux_set(vbc_codec->vbc_da_iis_port);
		break;
	case VBC_CAPTRUE:
		vbc_try_ad_iismux_set(vbc_codec->sprd_vbc_mux
				      [SPRD_VBC_AD_IISMUX].val);
		vbc_try_ad_dgmux_set(vbc_codec, 0);
		vbc_try_ad_dgmux_set(vbc_codec, 1);
		break;
	}

	if (p_eq_setting->is_active && p_eq_setting->data)
		vbc_eq_try_apply(vbc_codec->codec, vbc_idx);

	vbc_try_dg_set(vbc_codec, vbc_idx, VBC_LEFT);
	vbc_try_dg_set(vbc_codec, vbc_idx, VBC_RIGHT);

	return 0;
}

/*
 * proc interface
 */

#ifdef CONFIG_PROC_FS
static void vbc_proc_read(struct snd_info_entry *entry,
			  struct snd_info_buffer *buffer)
{
	int reg;

	snd_iprintf(buffer, "vbc register dump\n");
	for (reg = ARM_VB_BASE + 0x10; reg < ARM_VB_END; reg += 0x10) {
		snd_iprintf(buffer, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			    (reg - ARM_VB_BASE)
			    , vbc_reg_read(reg + 0x00)
			    , vbc_reg_read(reg + 0x04)
			    , vbc_reg_read(reg + 0x08)
			    , vbc_reg_read(reg + 0x0C)
		    );
	}
}

static void vbc_proc_init(struct snd_soc_codec *codec)
{
	struct snd_info_entry *entry;

	if (!snd_card_proc_new(codec->card->snd_card, "vbc", &entry))
		snd_info_set_text_ops(entry, NULL, vbc_proc_read);
}
#else /* !CONFIG_PROC_FS */
static inline void vbc_proc_init(struct snd_soc_codec *codec)
{
}
#endif

static int vbc_eq_reg_offset(u32 reg)
{
	int i = 0;
	if ((reg >= DAPATCHCTL) && (reg <= ADDGCTL)) {
		i = (reg - DAPATCHCTL) >> 2;
	} else if ((reg >= HPCOEF0) && (reg <= HPCOEF42)) {
		i = ((reg - HPCOEF0) >> 2) + ((ADDGCTL - DAPATCHCTL) >> 2) + 1;
	}
	BUG_ON(i >= VBC_EFFECT_PARAS_LEN);
	return i;
}

static inline void vbc_eq_reg_set(u32 reg, void *data)
{
	u32 *effect_paras = (u32 *) data;
	vbc_reg_write(reg, effect_paras[vbc_eq_reg_offset(reg)]);
}

static inline void vbc_eq_reg_set_range(u32 reg_start, u32 reg_end, void *data)
{
	u32 reg_addr;
	for (reg_addr = reg_start; reg_addr <= reg_end; reg_addr += 4) {
		vbc_eq_reg_set(reg_addr, data);
	}
}

static void vbc_eq_reg_apply(struct snd_soc_codec *codec, void *data,
			     int vbc_idx)
{
	if (vbc_idx == VBC_PLAYBACK) {
		vbc_eq_reg_set_range(DAALCCTL0, DAALCCTL10, data);
		vbc_eq_reg_set_range(HPCOEF0, HPCOEF42, data);

		vbc_eq_reg_set(DAHPCTL, data);
		/*FM function maybe use this regs,and EQ setting don't use them */
		/*vbc_da_eq_reg_set(DAPATCHCTL, data); */

		/*vbc_da_eq_reg_set(STCTL0, data); */
		/*vbc_da_eq_reg_set(STCTL1, data); */

	} else {
		vbc_eq_reg_set(ADPATCHCTL, data);
	}
}

static void vbc_eq_profile_apply(struct snd_soc_codec *codec, void *data,
				 int vbc_idx)
{
	if (codec) {
		vbc_eq_reg_apply(codec, data, vbc_idx);
	}
}

static void vbc_eq_profile_close(struct snd_soc_codec *codec, int vbc_idx)
{
	vbc_eq_profile_apply(codec, &vbc_eq_profile_default, vbc_idx);
}

static void vbc_eq_try_apply(struct snd_soc_codec *codec, int vbc_idx)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	u32 *data;

	sp_asoc_pr_dbg("%s 0x%x\n", __func__, (int)p_eq_setting->vbc_eq_apply);
	if (p_eq_setting->vbc_eq_apply) {
		struct vbc_eq_profile *now;
		mutex_lock(&vbc_codec->load_mutex);
		now = &p_eq_setting->data[p_eq_setting->now_profile];
		data = now->effect_paras;
		sp_asoc_pr_info("VBC %s EQ Apply '%s'\n",
				vbc_get_name(vbc_idx), now->name);
		p_eq_setting->vbc_eq_apply(codec, data, vbc_idx);
		mutex_unlock(&vbc_codec->load_mutex);
	}
}

static int vbc_eq_profile_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	ucontrol->value.integer.value[0] = p_eq_setting->now_profile;
	return 0;
}

static int vbc_eq_profile_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int vbc_idx = FUN_REG(mc->reg);
	int profile_max;

	profile_max = p_eq_setting->hdr.num_profile;

	ret = ucontrol->value.integer.value[0];
	if (ret == p_eq_setting->now_profile) {
		return ret;
	}

	sp_asoc_pr_info("VBC %s EQ Select %ld max %d\n",
			vbc_get_name(vbc_idx),
			ucontrol->value.integer.value[0], profile_max);

	if (ret < profile_max) {
		p_eq_setting->now_profile = ret;
	}

	if (p_eq_setting->is_active && p_eq_setting->data)
		vbc_eq_try_apply(codec, vbc_idx);

	return ret;
}

static int vbc_eq_loading(struct snd_soc_codec *codec)
{
	int ret;
	const u8 *fw_data;
	const struct firmware *fw;
	int i;
	int offset = 0;
	int len = 0;
	int old_num_profile;
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;

	sp_asoc_pr_dbg("%s\n", __func__);
	mutex_lock(&vbc_codec->load_mutex);
	p_eq_setting->is_loading = 1;

	/* request firmware for VBC EQ */
	ret = request_firmware(&fw, "vbc_eq", p_eq_setting->dev);
	if (ret != 0) {
		pr_err("ERR:Failed to load firmware: %d\n", ret);
		goto req_fw_err;
	}
	fw_data = fw->data;
	old_num_profile = p_eq_setting->hdr.num_profile;

	memcpy(&p_eq_setting->hdr, fw_data, sizeof(p_eq_setting->hdr));

	if (strncmp
	    (p_eq_setting->hdr.magic, VBC_EQ_FIRMWARE_MAGIC_ID,
	     VBC_EQ_FIRMWARE_MAGIC_LEN)) {
		pr_err("ERR:Firmware magic error!\n");
		ret = -EINVAL;
		goto eq_out;
	}

	if (p_eq_setting->hdr.profile_version != VBC_EQ_PROFILE_VERSION) {
		pr_err("ERR:Firmware support version is 0x%x!\n",
		       VBC_EQ_PROFILE_VERSION);
		ret = -EINVAL;
		goto eq_out;
	}

	if (p_eq_setting->hdr.num_profile > VBC_EQ_PROFILE_CNT_MAX) {
		pr_err
		    ("ERR:Firmware profile to large at %d, max count is %d!\n",
		     p_eq_setting->hdr.num_profile, VBC_EQ_PROFILE_CNT_MAX);
		ret = -EINVAL;
		goto eq_out;
	}

	if (old_num_profile != p_eq_setting->hdr.num_profile) {
		vbc_safe_kfree(&p_eq_setting->data);
	}

	len = p_eq_setting->hdr.num_profile * sizeof(struct vbc_eq_profile);
	if (p_eq_setting->data == NULL) {
		p_eq_setting->data = kzalloc(len, GFP_KERNEL);
		if (p_eq_setting->data == NULL) {
			ret = -ENOMEM;
			goto eq_out;
		}
	}
	offset = sizeof(struct vbc_fw_header);
	memcpy(p_eq_setting->data, fw_data + offset, len);

	for (i = 0; i < p_eq_setting->hdr.num_profile; i++) {
		const char *magic = (char *)p_eq_setting->data[i].magic;
		if (strncmp
		    (magic, VBC_EQ_FIRMWARE_MAGIC_ID,
		     VBC_EQ_FIRMWARE_MAGIC_LEN)) {
			pr_err
			    ("ERR:Firmware profile[%d] magic error!magic: %s \n",
			     i, magic);
			ret = -EINVAL;
			goto eq_out;
		}
	}

	ret = 0;
	goto eq_out;

eq_out:
	release_firmware(fw);
req_fw_err:
	p_eq_setting->is_loading = 0;
	mutex_unlock(&vbc_codec->load_mutex);
	if (ret >= 0) {
		if (p_eq_setting->is_active && p_eq_setting->data)
			vbc_eq_try_apply(codec, 0);
	}
	sp_asoc_pr_dbg("return %i\n", ret);
	return ret;
}

static int vbc_switch_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	ret = arch_audio_vbc_switch(AUDIO_NO_CHANGE);
	if (ret >= 0)
		ucontrol->value.integer.value[0] =
		    (ret == AUDIO_TO_DSP_CTRL ? 0 : 1);
	return ret;
}

static int vbc_switch_reg_val[];
static int vbc_switch_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	sp_asoc_pr_info("VBC Switch to %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);

	ret = ucontrol->value.integer.value[0];
	ret = arch_audio_vbc_switch(ret == 0 ?
				    AUDIO_TO_DSP_CTRL : AUDIO_TO_ARM_CTRL);

	return ret;
}

static int vbc_eq_switch_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	ucontrol->value.integer.value[0] = p_eq_setting->is_active;
	return 0;
}

static int vbc_eq_switch_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int ret;

	ret = ucontrol->value.integer.value[0];
	if (ret == p_eq_setting->is_active) {
		return ret;
	}

	sp_asoc_pr_info("VBC EQ Switch %s\n",
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	if ((ret == 0) || (ret == 1)) {
		p_eq_setting->is_active = ret;
		if (p_eq_setting->is_active && p_eq_setting->data) {
			p_eq_setting->vbc_eq_apply = vbc_eq_profile_apply;
			vbc_eq_try_apply(codec, id);
		} else {
			p_eq_setting->vbc_eq_apply = 0;
			vbc_eq_profile_close(codec, id);
		}
	}

	return ret;
}

static int vbc_eq_load_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	ucontrol->value.integer.value[0] = p_eq_setting->is_loading;
	return 0;
}

static int vbc_eq_load_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int ret;
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;

	sp_asoc_pr_info("VBC EQ %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);

	ret = ucontrol->value.integer.value[0];
	if (ret == 1) {
		ret = vbc_eq_loading(codec);
	}

	return ret;
}

static int vbc_dg_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->dg[vbc_idx].dg_val[id];
	return 0;
}

static int vbc_dg_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = mc->shift;

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->dg[vbc_idx].dg_val[id]) {
		return ret;
	}

	sp_asoc_pr_info("VBC %s%d DG set 0x%02x\n",
			vbc_get_name(vbc_idx), id,
			(int)ucontrol->value.integer.value[0]);

	if (ret <= VBC_DG_VAL_MAX) {
		vbc_codec->dg[vbc_idx].dg_val[id] = ret;
	}

	vbc_try_dg_set(vbc_codec, vbc_idx, id);

	return ret;
}

static int vbc_st_dg_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->st_dg.dg_val[id];
	return 0;
}

static int vbc_st_dg_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->st_dg.dg_val[id]) {
		return ret;
	}

	sp_asoc_pr_info("VBC ST%d DG set 0x%02x\n", id,
			(int)ucontrol->value.integer.value[0]);

	if (ret <= VBC_DG_VAL_MAX) {
		vbc_codec->st_dg.dg_val[id] = ret;
	}

	vbc_try_st_dg_set(vbc_codec, id);

	return ret;
}

static int vbc_dg_switch_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = mc->shift;

	ucontrol->value.integer.value[0] = vbc_codec->dg[vbc_idx].dg_switch[id];
	return 0;
}

static int vbc_dg_switch_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);
	int vbc_idx = mc->shift;

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->dg[vbc_idx].dg_switch[id]) {
		return ret;
	}

	sp_asoc_pr_info("VBC %s%d DG Switch %s\n",
			vbc_get_name(vbc_idx), id,
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	vbc_codec->dg[vbc_idx].dg_switch[id] = ret;

	vbc_try_dg_set(vbc_codec, vbc_idx, id);

	return ret;
}

static int vbc_st_hpf_switch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->st_dg.hpf_switch[id];
	return 0;
}

static int vbc_st_hpf_switch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->st_dg.hpf_switch[id]) {
		return ret;
	}

	sp_asoc_pr_info("VBC ST%d HPF Switch %s\n", id,
			STR_ON_OFF(ucontrol->value.integer.value[0]));

	vbc_codec->st_dg.hpf_switch[id] = ret;

	vbc_try_st_hpf_set(vbc_codec, id);

	return ret;
}

static int vbc_st_hpf_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->st_dg.hpf_val[id];
	return 0;
}

static int vbc_st_hpf_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->st_dg.hpf_val[id]) {
		return ret;
	}

	sp_asoc_pr_info("VBC ST%d HPF set 0x%02x\n", id,
			(int)ucontrol->value.integer.value[0]);

	if (ret <= VBC_DG_VAL_MAX) {
		vbc_codec->st_dg.hpf_val[id] = ret;
	}

	vbc_try_st_hpf_set(vbc_codec, id);

	return ret;
}

static int adc_dgmux_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ucontrol->value.integer.value[0] = vbc_codec->adc_dgmux_val[id];
	return 0;
}

static int adc_dgmux_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	ret = ucontrol->value.integer.value[0];
	if (ret == vbc_codec->adc_dgmux_val[id]) {
		return ret;
	}

	sp_asoc_pr_info("VBC AD%d DG mux : %ld\n", id,
			ucontrol->value.integer.value[0]);

	vbc_codec->adc_dgmux_val[id] = ret;
	vbc_try_ad_dgmux_set(vbc_codec, id);

	return ret;
}

static int dac_iismux_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.integer.value[0] = vbc_codec->vbc_da_iis_port;
	return 0;
}

static int dac_iismux_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *texts = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);

	if (vbc_codec->vbc_da_iis_port == ucontrol->value.integer.value[0])
		return 0;

	sp_asoc_pr_info("VBC DA output to %s\n",
			texts->texts[ucontrol->value.integer.value[0]]);

	vbc_codec->vbc_da_iis_port = ucontrol->value.integer.value[0];

	vbc_try_da_iismux_set(vbc_codec->vbc_da_iis_port);

	return 1;
}

static const char *switch_function[] = { "dsp", "arm" };
static const char *eq_load_function[] = { "idle", "loading" };
static const char *da_iis_mux_function[] =
    { "sprd-codec", "ext-codec-4", "ext-codec-6" };
static const char *fm_sample_rate_function[] = { "32000", "48000" };

static const struct soc_enum vbc_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, switch_function),
	SOC_ENUM_SINGLE_EXT(2, eq_load_function),
	SOC_ENUM_SINGLE_EXT(3, da_iis_mux_function),
	SOC_ENUM_SINGLE_EXT(2, fm_sample_rate_function),
};

static const struct snd_kcontrol_new vbc_codec_snd_controls[] = {
	SOC_ENUM_EXT("VBC Switch", vbc_enum[0], vbc_switch_get,
		     vbc_switch_put),
	SOC_SINGLE_EXT("VBC EQ Switch", FUN_REG(VBC_PLAYBACK), 0, 1, 0,
		       vbc_eq_switch_get,
		       vbc_eq_switch_put),
	SOC_ENUM_EXT("VBC EQ Update", vbc_enum[1], vbc_eq_load_get,
		     vbc_eq_load_put),

	SOC_SINGLE_EXT("VBC DACL DG Set", FUN_REG(VBC_LEFT),
		       VBC_PLAYBACK, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC DACR DG Set", FUN_REG(VBC_RIGHT),
		       VBC_PLAYBACK, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADCL DG Set", FUN_REG(VBC_LEFT),
		       VBC_CAPTRUE, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC ADCR DG Set", FUN_REG(VBC_RIGHT),
		       VBC_CAPTRUE, VBC_DG_VAL_MAX, 0, vbc_dg_get,
		       vbc_dg_put),
	SOC_SINGLE_EXT("VBC STL DG Set", FUN_REG(VBC_LEFT),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_dg_get, vbc_st_dg_put),
	SOC_SINGLE_EXT("VBC STR DG Set", FUN_REG(VBC_RIGHT),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_dg_get, vbc_st_dg_put),

	SOC_SINGLE_EXT("VBC DACL DG Switch", FUN_REG(VBC_LEFT),
		       VBC_PLAYBACK, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC DACR DG Switch", FUN_REG(VBC_RIGHT),
		       VBC_PLAYBACK, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADCL DG Switch", FUN_REG(VBC_LEFT),
		       VBC_CAPTRUE, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC ADCR DG Switch", FUN_REG(VBC_RIGHT),
		       VBC_CAPTRUE, 1, 0, vbc_dg_switch_get,
		       vbc_dg_switch_put),
	SOC_SINGLE_EXT("VBC STL HPF Switch", FUN_REG(VBC_LEFT),
		       0, 1, 0, vbc_st_hpf_switch_get, vbc_st_hpf_switch_put),
	SOC_SINGLE_EXT("VBC STR HPF Switch", FUN_REG(VBC_RIGHT),
		       0, 1, 0, vbc_st_hpf_switch_get, vbc_st_hpf_switch_put),
	SOC_SINGLE_EXT("VBC STL HPF Set", FUN_REG(VBC_LEFT),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_hpf_get, vbc_st_hpf_put),
	SOC_SINGLE_EXT("VBC STR HPF Set", FUN_REG(VBC_RIGHT),
		       0, VBC_DG_VAL_MAX, 0, vbc_st_hpf_get, vbc_st_hpf_put),

	SOC_SINGLE_EXT("VBC AD0 DG Mux", FUN_REG(ADC0_DGMUX), 0, 1, 0,
		       adc_dgmux_get, adc_dgmux_put),
	SOC_SINGLE_EXT("VBC AD1 DG Mux", FUN_REG(ADC1_DGMUX), 0, 1, 0,
		       adc_dgmux_get, adc_dgmux_put),
	SOC_ENUM_EXT("VBC DA IIS Mux", vbc_enum[2], dac_iismux_get,
		     dac_iismux_put),
	SOC_SINGLE_EXT("VBC DA EQ Profile Select", FUN_REG(VBC_PLAYBACK), 0,
		       VBC_EQ_PROFILE_CNT_MAX, 0,
		       vbc_eq_profile_get, vbc_eq_profile_put),

};

static unsigned int vbc_codec_read(struct snd_soc_codec *codec,
				   unsigned int reg)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	/* Because snd_soc_update_bits reg is 16 bits short type,
	   so muse do following convert
	 */
	if (IS_SPRD_VBC_RANG(reg | SPRD_VBC_BASE_HI)) {
		reg |= SPRD_VBC_BASE_HI;
		return vbc_reg_read(reg);
	} else if (IS_SPRD_VBC_MUX_RANG(FUN_REG(reg))) {
		int id = SPRD_VBC_MUX_IDX(FUN_REG(reg));
		struct sprd_vbc_mux_op *mux = &(vbc_codec->sprd_vbc_mux[id]);
		return mux->val;
	} else if (IS_SPRD_VBC_SWITCH_RANG(FUN_REG(reg))) {
		int id = SPRD_VBC_SWITCH_IDX(FUN_REG(reg));
		return vbc_codec->vbc_loop_switch[id];
	}

	sp_asoc_pr_dbg("The Register is NOT VBC Codec's reg = 0x%x\n", reg);
	return 0;
}

static int vbc_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			   unsigned int val)
{
	if (IS_SPRD_VBC_RANG(reg | SPRD_VBC_BASE_HI)) {
		reg |= SPRD_VBC_BASE_HI;
		return vbc_reg_write(reg, val);
	}
	sp_asoc_pr_dbg("The Register is NOT VBC Codec's reg = 0x%x\n", reg);
	return 0;
}

static int vbc_codec_soc_probe(struct snd_soc_codec *codec)
{
	struct vbc_codec_priv *vbc_codec = snd_soc_codec_get_drvdata(codec);
	struct vbc_equ *p_eq_setting = &vbc_codec->vbc_eq_setting;
	int ret = 0;

	sp_asoc_pr_dbg("%s\n", __func__);

	codec->dapm.idle_bias_off = 1;

	vbc_codec->codec = codec;
	p_eq_setting->codec = codec;

	vbc_proc_init(codec);

	return ret;
}

/* power down chip */
static int vbc_codec_soc_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_vbc_codec = {
	.probe = vbc_codec_soc_probe,
	.remove = vbc_codec_soc_remove,
	.read = vbc_codec_read,
	.write = vbc_codec_write,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
	.dapm_widgets = vbc_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(vbc_codec_dapm_widgets),
	.dapm_routes = vbc_codec_intercon,
	.num_dapm_routes = ARRAY_SIZE(vbc_codec_intercon),
	.controls = vbc_codec_snd_controls,
	.num_controls = ARRAY_SIZE(vbc_codec_snd_controls),
};

static int sprd_vbc_codec_probe(struct platform_device *pdev)
{
	struct vbc_codec_priv *vbc_codec;
	int ret;

	sp_asoc_pr_dbg("%s\n", __func__);

	vbc_codec = devm_kzalloc(&pdev->dev, sizeof(struct vbc_codec_priv),
				 GFP_KERNEL);
	if (vbc_codec == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, vbc_codec);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_vbc_codec, NULL, 0);
	if (ret != 0) {
		pr_err("ERR:Failed to register VBC-CODEC: %d\n", ret);
	}

	/* AP refer to array vbc_switch_reg_val */
	vbc_codec->vbc_control = 2;
	vbc_codec->vbc_eq_setting.dev = &pdev->dev;
	vbc_codec->st_dg.dg_val[0] = 0x18;
	vbc_codec->st_dg.dg_val[1] = 0x18;
	vbc_codec->st_dg.hpf_val[0] = 0x3;
	vbc_codec->st_dg.hpf_val[1] = 0x3;
	vbc_codec->dg[0].dg_val[0] = 0x18;
	vbc_codec->dg[0].dg_val[1] = 0x18;
	vbc_codec->dg[1].dg_val[0] = 0x18;
	vbc_codec->dg[1].dg_val[1] = 0x18;
	vbc_codec->dg[0].dg_set[0] = vbc_da0_dg_set;
	vbc_codec->dg[0].dg_set[1] = vbc_da1_dg_set;
	vbc_codec->dg[1].dg_set[0] = vbc_ad0_dg_set;
	vbc_codec->dg[1].dg_set[1] = vbc_ad1_dg_set;
	mutex_init(&vbc_codec->load_mutex);

	return ret;
}

static int sprd_vbc_codec_remove(struct platform_device *pdev)
{
	struct vbc_codec_priv *vbc_codec = platform_get_drvdata(pdev);
	vbc_codec->vbc_eq_setting.dev = 0;

	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

MODULE_DESCRIPTION("SPRD ASoC VBC Codec Driver");
MODULE_AUTHOR("Zhenfang Wang <zhenfang.wang@spreadtrum.com>");
MODULE_AUTHOR("Ken.Kuang <ken.kuang@spreadtrum.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("codec:VBC Codec");
