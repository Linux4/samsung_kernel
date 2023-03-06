/* sound/soc/codecs/rt5511.c
 * RT5511 ALSA SoC Audio driver
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
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <trace/events/asoc.h>
#include <linux/version.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif /* #ifdef CONFIG_DEBUG_FS */

#include <linux/mfd/rt5511/core.h>
#include <linux/mfd/rt5511/registers.h>
#include <linux/mfd/rt5511/pdata.h>

#define OPTIMIZE_HP_POP_NOISE		1

/* For BT (supported A2DP) is connected  */
#define SKIP_AIF2_MUTE_WHEN_AIF3_USED	0

#include "rt5511.h"
#include "rt_hubs.h"

#ifdef CONFIG_DEBUG_FS
struct rt_debug_st {
	void *info;
	int id;
};

struct rt_debug_info {
	struct snd_soc_codec *codec;
	unsigned char reg_addr;
	unsigned int  reg_data;
	unsigned char part_id;
	unsigned int  dbg_length;
	unsigned char dbg_data[256];
};

enum {
	RT5511_DBG_REG = 0,
	RT5511_DBG_DATA,
	RT5511_DBG_REGS,
	RT5511_DBG_ADBREG,
	RT5511_DBG_ADBDATA,
	RT5511_DBG_MAX
};

static struct dentry *debugfs_rt_dent;
static struct dentry *debugfs_file[RT5511_DBG_MAX];
static struct rt_debug_st rtdbg_data[RT5511_DBG_MAX];
#endif /* #ifdef CONFIG_DEBUG_FS */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
/* > 3.4.0 ==> use codec add controls instead of the original one */

static inline int snd_soc_add_controls(struct snd_soc_codec *codec,
	const struct snd_kcontrol_new *controls, int num_controls)
{
	return snd_soc_add_codec_controls(codec, controls, num_controls);
}
#endif

#if 1
#define RT5511_CODEC_RW_DBG(format, args...) \
	printk(KERN_DEBUG "[%s]" format, __func__, ##args)
#else
#define RT5511_CODEC_RW_DBG(format, args...)
#endif  /* #if 1 */

static int rt5511_readable(struct snd_soc_codec *codec, u32 reg)
{
	return (rt5511_access_masks[reg].readable != 0) ? 1 : 0;
}

static int rt5511_volatile(struct snd_soc_codec *codec, u32 reg)
{
	return rt5511_access_masks[reg].is_volatile;
}

static inline bool check_regsize_equal_1_4(int index)
{
	if (rt5511_reg_size[index] == 1)
		return true;
	if (rt5511_reg_size[index] == 4)
		return true;
	return false;
}

/*
 * [BLOCK] Read/Write HW Registers
 */

static unsigned int virt_ctrl_regdat;	/* default = 0 */

static inline unsigned int rt5511_hw_read(
	struct snd_soc_codec *codec, u32 reg, int size)
{
	u32 val;
	int ret = rt5511_reg_read(codec->control_data, reg, &val, size);
	if (ret < 0) {
		dev_err(codec->dev, "Read from %x failed: %d\n",
			reg, ret);
	}
	return val;
}

static inline int rt5511_hw_write(
	struct snd_soc_codec *codec, u32 reg, u32 val, int size)
{
	int ret = rt5511_reg_write(codec->control_data, reg, val, size);

	if (ret < 0) {
		dev_warn(codec->dev, "Write to %x : 0x%x\n",
			 reg, ret);
	}
	return ret;
}

/*
 * ret value :
 * < 0 : error happen
 * = 0 : doesn't be changed
 * > 0 : change success
*/

static int rt5511_enable_power_down(
	struct snd_soc_codec *codec, bool bOn)
{
	int ret;
	u32 val;
	const u32 reg = RT5511_SWRESET_POWERDOWN;
	const u32 mask = RT5511_PDWN;

	val = rt5511_hw_read(codec, reg, 1);

	if (bOn)
		val = (val | mask);
	else
		val = (val & ~mask);

	ret = rt5511_hw_write(codec, reg, val, 1);

	if (ret == 0)
		return 1;
	else
		return ret;
}

static int rt5511_write(struct snd_soc_codec *codec, u32 reg, u32 value)
{
	int ret, ret1 = 0;
	int size = rt5511_reg_size[reg]; /* Get size by looking up table */
	struct rt5511 *control = codec->control_data;	/* get from mfd */

	RT5511_CODEC_RW_DBG("Reg[0x%x] <= 0x%x (size=%d)\n",
			    reg, value, size);

	BUG_ON(size != 4 && size != 1);

	if (reg == RT5511_VIRT_CTRL) {
		virt_ctrl_regdat = value;
		return 0;
	}

	if (virt_ctrl_regdat & RT5511_VIRT_BYPASS_CODEC_W)
		return 0;

	if (!rt5511_volatile(codec, reg))
		rt5511_reg_cache[reg] = value;

	if (control->suspended) {
		ret1 = rt5511_enable_power_down(codec, false);
		if (ret1 < 0)
			return ret1;
	}

	ret = rt5511_hw_write(codec, reg, value, size);

	if (ret1 > 0) {
		ret1 = rt5511_enable_power_down(codec, true);
		if (ret1 < 0)
			return ret1;
	}

	return ret;
}

static unsigned int rt5511_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	int ret;
	int size = rt5511_reg_size[reg];
	bool bBypassCache;

	if (reg == RT5511_VIRT_CTRL)
		return virt_ctrl_regdat;

	if (size != 1 && size != 4) {
		ret = rt5511_reg_cache[reg];
		dev_info(codec->dev, "Cache read from %x : 0x%x\n",
			 reg, ret);
		return ret;
	}

	bBypassCache = virt_ctrl_regdat & RT5511_VIRT_BYPASS_CACHE_R;
	if (!rt5511_volatile(codec, reg) && rt5511_readable(codec, reg) &&
	    (reg < RT5511_CACHE_SIZE) && (!bBypassCache)) {
		return rt5511_reg_cache[reg];
	}

	return rt5511_hw_read(codec, reg, size);
}

/*
 * [BLOCK] Sync reference soruce
 */

static inline int rt5511_get_i2scfg_reg(
	struct snd_soc_codec *codec, int id);

static inline int rt5511_is_enable_aif_mute(
	struct snd_soc_codec *codec, int id);

static inline int rt5511_enable_aif_mute(
	struct snd_soc_codec *codec, int id, int mute);

static inline void rt5511_notify_asrc_change(
	struct snd_soc_codec *codec, int asrc_id)
{
	int reg_addr;
	unsigned int regval;

	if (asrc_id <= 0) {
		regval = snd_soc_read(codec, RT5511_I2S_CK_SEL);
		asrc_id = ((regval&RT5511_SYNC_REF_MASK) == 0) ? 2 : 1;
	}

	reg_addr = rt5511_get_i2scfg_reg(codec, asrc_id);
	regval = snd_soc_read(codec, reg_addr);

	snd_soc_update_bits(codec, reg_addr,
		RT5511_I2S_SR_MODE_MASK, RT5511_I2S_SR_MODE_MASK);
	snd_soc_update_bits(codec, reg_addr,
		RT5511_I2S_SR_MODE_MASK, regval);
}

static inline int rt5511_change_sync_reference(
	struct snd_soc_codec *codec, int sync_ref_sel, int ck_sel)
{
	int ret = 0;
	int asrc_id = 1;	/* 1:AIF1, 2: AIF2 */
	int asrc_mute = 1;
	int regval, regval_old;

	regval = regval_old = snd_soc_read(codec, RT5511_I2S_CK_SEL);

	if (sync_ref_sel >= 0) {
		regval &= ~RT5511_SYNC_REF_MASK;
		regval |= sync_ref_sel << RT5511_SYNC_REF_SHIFT;
		asrc_id = (sync_ref_sel == 0) ? 2 : 1;
	}

	if (ck_sel >= 0) {
		regval &= ~RT5511_CK_SEL_MASK;
		regval |= ck_sel & RT5511_CK_SEL_MASK;
	}

	if (regval == regval_old) /* Don't need change any thing */
		return 0;

	asrc_mute = rt5511_is_enable_aif_mute(codec, asrc_id);
	if (asrc_mute == 0) {
		rt5511_enable_aif_mute(codec, asrc_id, 1);
		msleep(50); /* Waiting for ramp */
	}

	ret = snd_soc_write(codec, RT5511_I2S_CK_SEL, regval);

	rt5511_notify_asrc_change(codec, asrc_id);
	if (asrc_mute == 0)
		rt5511_enable_aif_mute(codec, asrc_id, 0);

	if (ret < 0)
		return 0;
	else
		return 1;
}

/*
 * [BLOCK] Clock/PLL Setting
 */

enum {
	RT5511_CKMODE_24_576000 = 0,	/* 48K Based */
	RT5511_CKMODE_22_579200 = 1,	/* 44.1K Based */
};

static const unsigned int c_ckmode_freq[] = {
	24576000, 22579200
};

static int rt5511_calc_ckmode(unsigned int freq)
{
	int mode;
	unsigned int freq_44_1_base;

	freq_44_1_base = (freq / 44100) * 44100;

	if (freq_44_1_base == freq)
		mode = RT5511_CKMODE_22_579200;
	else
		mode = RT5511_CKMODE_24_576000;

	RT_DBG("freq%d, ckmode=%d\n", freq, mode);
	return mode;
}

static int rt5511_set_ssrc_ckmode(struct snd_soc_codec *codec,
				   unsigned int freq_out)
{
	int ssrc_mode, asrc_mode;
	int regval, regmask, mode;

	mode = rt5511_calc_ckmode(freq_out);
	RT_DBG("mode=%d, sysclk=%d\n", mode, c_ckmode_freq[mode]);

	regval = snd_soc_read(codec, RT5511_ASRC_CTRL1);

    asrc_mode = (regval & RT5511_SSRC_CKMODE_MASK) ?
        RT5511_CKMODE_BASE_44_1K : RT5511_CKMODE_BASE_48_0K;

	if (mode == RT5511_CKMODE_22_579200)
		ssrc_mode = RT5511_CKMODE_BASE_44_1K;
	else if (mode == RT5511_CKMODE_24_576000)
		ssrc_mode = RT5511_CKMODE_BASE_48_0K;

	asrc_mode <<= RT5511_ASRC_CKMODE_SHIFT;
	ssrc_mode <<= RT5511_SSRC_CKMODE_SHIFT;

	regval = ssrc_mode|asrc_mode;
	regmask = RT5511_SSRC_CKMODE_MASK|RT5511_ASRC_CKMODE_MASK;

	return snd_soc_update_bits(codec, RT5511_ASRC_CTRL1, regmask, regval);
}

static int rt5511_set_pll_freerun(struct snd_soc_codec *codec, bool bOn)
{
	unsigned int value;

	if (bOn)
		value = RT5511_PLL_FREERUN;
	else
		value = 0;

	return snd_soc_update_bits(
		codec, RT5511_PLL_BWCON, RT5511_PLL_FREERUN, value);
}

static int configure_clock(struct snd_soc_codec *codec)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int change0, change1 = 0, change2 = 0;
	int regval, regval2 = -1;
	const int regmask = RT5511_PLAYMODE_N | RT5511_CK_SEL_PLAY;

	RT_DBG("sysclk:%d\n", rt5511->sysclk);

	switch (rt5511->sysclk) {
	case RT5511_SYSCLK_PLL:
		regval = RT5511_PLAYMODE_N | RT5511_CK_SEL_PLAY;

		if (rt5511->pll.src < RT5511_PLL_SRC_MCLK2)
			regval2 = RT5511_SYNC_REF_I2S1;
		else
			regval2 = RT5511_SYNC_REF_I2S2;

		if (rt5511->sysclk_rate != rt5511->pll.out) {
			RT_DBG("sysclk (%d) != pll_out (%d)\n",
				rt5511->sysclk_rate, rt5511->pll.out);
		}
		break;

	case RT5511_SYSCLK_PLL_DIV2:
		regval = 0;

		if (rt5511->pll.src < RT5511_PLL_SRC_MCLK2)
			regval2 = RT5511_SYNC_REF_I2S1;
		else
			regval2 = RT5511_SYNC_REF_I2S2;
		break;

	case RT5511_SYSCLK_MCLK1:
		regval2 = RT5511_SYNC_REF_I2S1;
		regval = RT5511_CK_SEL_PLAY;
		break;

	case RT5511_SYSCLK_MCLK2:
		regval2 = RT5511_SYNC_REF_I2S2;
		regval = RT5511_CK_SEL_PLAY;
		break;

	default:
		return -EINVAL;
	}

	rt5511_set_pll_freerun(codec, true);
	change0 = rt5511_set_ssrc_ckmode(codec, rt5511->sysclk_rate);

	if (regval2 >= 0)
		change1 = rt5511_change_sync_reference(codec, regval2, -1);

	/* Sequence for change RT5511 SYSCLK Source */
	if (snd_soc_read(codec, RT5511_CLOCK_GATING_CTRL)&RT5511_PLAYMODE_N) {
		change2 = snd_soc_update_bits(
			codec, RT5511_CLOCK_GATING_CTRL, regmask, regval);
	} else {
		snd_soc_update_bits(codec, RT5511_CLOCK_GATING_CTRL,
				     RT5511_PLAYMODE_N, regval);
		change2 = snd_soc_update_bits(codec, RT5511_CLOCK_GATING_CTRL,
					       RT5511_CK_SEL_PLAY, regval);
	}

	rt5511_set_pll_freerun(codec, false);

	if (change0 | change1 | change2)
		snd_soc_dapm_sync(&codec->dapm);
	return 0;
}

/*
 * [BLOCK] EQ & DRC Enable State
 */

enum eRT5511_EQDRC_En {
	eEQDRC_Disable = 0,
	eEQDRC_AutoEnable,
	eEQDRC_ForceEnable,
};

static const char * const eqdrc_en_state_text[] = {
	"Disable", "Auto-enable", "Force-enable"
};

static const struct soc_enum eqdrc_en_state_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		ARRAY_SIZE(eqdrc_en_state_text), eqdrc_en_state_text),
};

static bool rt5511_get_eqdrc_enable(int en_state, bool auto_en)
{
	switch (en_state) {
	case eEQDRC_AutoEnable:
		return auto_en;
	case eEQDRC_ForceEnable:
		return true;
	default:
		return false;
	};
}

static int rt5511_write_eqdrc_en_state(
	struct snd_soc_codec *codec, u32 reg, u32 shift, int en_state)
{
	u32 regval;
	switch (en_state) {
	case eEQDRC_AutoEnable:	/* Depend on driver behaviour to on/off */
		return 0;
	case eEQDRC_ForceEnable:
		regval = shift;
		break;
	case eEQDRC_Disable:
		regval = 0;
	default:
		return -EINVAL;
	};

	return snd_soc_update_bits(codec, reg, shift, regval);
}

/*
 * [BLOCK] EQ Control
 */

static const u8 RT5511_EQEnBit[RT5511_BQ_PATH_NUM] = {
	RT5511_EQ_1_EN, RT5511_EQ_2_EN
};

static void rt5511_set_eq(struct snd_soc_codec *codec, int set, int eq)
{
	int addr = RT5511_BQ1 + eq;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	struct rt5511_pdata *pdata = rt5511->pdata;
	struct rt5511_eq_cfg *eq_cfg = &pdata->eq_cfgs[set];

	if (!eq_cfg->en[eq])
		return;

	rt5511_bulk_write(codec->control_data, addr,
			  RT5511_BQ_REG_SIZE, eq_cfg->coeff[eq]);
}

static void rt5511_set_eqset(struct snd_soc_codec *codec, int set)
{
	int i;
	snd_soc_update_bits(codec,
		RT5511_EQ_BAND_SEL, RT5511_EQ_BAND_SEL_MASK, set);

	for (i = 0; i < RT5511_BQ_REG_NUM; i++)
		rt5511_set_eq(codec, set, i);
}

static void rt5511_eq_auto_enable(struct snd_soc_codec *codec, bool auto_en)
{
	int eq;
	u8 regval = 0;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	for (eq = 0; eq < RT5511_BQ_PATH_NUM; eq++) {
		if (rt5511_get_eqdrc_enable(rt5511->eq_en[eq], auto_en))
			regval |= RT5511_EQEnBit[eq];
	}

	snd_soc_update_bits(codec,
		RT5511_DAC_DRC_EQ_EN, RT5511_EQ_EN_MASK, regval);
}

static void rt5511_eq_init(struct snd_soc_codec *codec)
{
	int eq;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	struct rt5511_pdata *pdata = rt5511->pdata;

	for (eq = 0; eq < RT5511_BQ_PATH_NUM; eq++) {
		if (pdata->eq_en[eq]) {
			dev_info(
				codec->dev, "%s : init eq%d\n", __func__, eq);
			rt5511_set_eqset(codec, eq*RT5511_BQ_PATH_NUM);
			rt5511_set_eqset(codec, eq*RT5511_BQ_PATH_NUM + 1);
		}
	}
}

/*
 * [BLOCK] EQ Enable State KControl
 */

#define RT5511_NAME_EQ1_ENABLE		"EQ1 Enable"
#define RT5511_NAME_EQ2_ENABLE		"EQ2 Enable"

static const char * const eq_enable_text[] = {
	RT5511_NAME_EQ1_ENABLE, RT5511_NAME_EQ2_ENABLE
};

static int rt5511_get_eq_index(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(eq_enable_text); i++) {
		if (strcmp(name, eq_enable_text[i]) == 0)
			return i;
	}

	return -EINVAL;
}

static int rt5511_get_eq_en_state(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	int eq = rt5511_get_eq_index(kcontrol->id.name);
	if (eq < 0)
		return eq;

	ucontrol->value.enumerated.item[0] = rt5511->eq_en[eq];
	return 0;
}

static int rt5511_set_eq_en_state(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	u32 reg, shift;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int eq = rt5511_get_eq_index(kcontrol->id.name);
	int value = ucontrol->value.integer.value[0];

	if (eq < 0)
		return eq;

	reg = RT5511_DAC_DRC_EQ_EN;
	shift = RT5511_EQEnBit[eq];

	ret = rt5511_write_eqdrc_en_state(codec, reg, shift, value);
	if (ret >= 0)
		rt5511->eq_en[eq] = value;

	return ret;
}

/*
 * [BLOCK] DRC Control
 */

static const u8 RT5511_DRCEnBit[RT5511_DRC_REG_NUM] = {
	RT5511_DAC_DRC1_EN, RT5511_DAC_DRC2_EN, RT5511_DAC_DRC3_EN,
	RT5511_DAC_DRC4_EN, RT5511_ADC_DRC1_EN, RT5511_ADC_DRC2_EN
};

static void rt5511_set_drc(struct snd_soc_codec *codec, int drc)
{
	int addr1 = RT5511_DAC_DRC1_COEFF_CTRL + drc;
	int addr2 = RT5511_DAC_DRC1_ALPHA + drc;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	struct rt5511_pdata *pdata = rt5511->pdata;
	struct rt5511_drc_cfg *drc_cfg = &pdata->drc_cfgs[drc];

	rt5511_bulk_write(codec->control_data,
			  addr1, RT5511_DRC_REG1_SIZE, drc_cfg->coeff1);

	rt5511_bulk_write(codec->control_data,
			  addr2, RT5511_DRC_REG2_SIZE, drc_cfg->coeff2);
}

static void rt5511_drc_dac_auto_enable(
	struct snd_soc_codec *codec, bool auto_en)
{
	int i;
	int regval = 0;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	for (i = 0; i < RT5511_DRC_DAC_REG_NUM; i++) {
		if (rt5511_get_eqdrc_enable(rt5511->drc_en[i], auto_en))
			regval |= RT5511_DRCEnBit[i];
	}

	snd_soc_update_bits(codec, RT5511_DAC_DRC_EQ_EN,
			    RT5511_DAC_DRC_EN_MASK, regval);
}

static void rt5511_drc_adc_auto_enable(
	struct snd_soc_codec *codec, bool auto_en)
{
	int i, j;
	int regval = 0;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	for (j = 0; j < RT5511_DRC_ADC_REG_NUM; j++) {
		i = RT5511_DRC_DAC_REG_NUM + j;
		if (rt5511_get_eqdrc_enable(rt5511->drc_en[i], auto_en))
			regval |= RT5511_DRCEnBit[i];
	}

	snd_soc_update_bits(codec, RT5511_ADC_CTRL,
			    RT5511_ADC_DRC_EN_MASK, regval);
}

static void rt5511_drc_ng_enable(struct snd_soc_codec *codec, bool en)
{
	int regval = 0;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	struct rt5511_pdata *pdata = rt5511->pdata;

	if (en)
		regval = pdata->drc_ng_en;

	snd_soc_update_bits(codec, RT5511_DRC_ADC_SEL,
			RT5511_DRC_NOISE_EN_MASK, regval);
}

static void rt5511_drc_init(struct snd_soc_codec *codec)
{
	int drc;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	struct rt5511_pdata *pdata = rt5511->pdata;

	if (pdata->drc_ng_en) {
		rt5511_bulk_write(codec->control_data, RT5511_DRC_NG_CTRL,
				RT5511_DRC_NG_REG_SIZE, pdata->drc_ng_cfg);
	}

	for (drc = 0; drc < RT5511_DRC_REG_NUM; drc++) {
		if (pdata->drc_en[drc]) {
			dev_info(codec->dev,
				"%s : init drc%d\n", __func__, drc);
			rt5511_set_drc(codec, drc);
		}
	}
}

/*
 * [BLOCK] DRC Enable State KControl
 */

#define RT5511_NAME_DACDRC1_EN		"DACDRC1 Enable"
#define RT5511_NAME_DACDRC2_EN		"DACDRC2 Enable"
#define RT5511_NAME_DACDRC3_EN		"DACDRC3 Enable"
#define RT5511_NAME_DACDRC4_EN		"DACDRC4 Enable"
#define RT5511_NAME_ADCDRC1_EN		"ADCDRC1 Enable"
#define RT5511_NAME_ADCDRC2_EN		"ADCDRC2 Enable"

static const char * const drc_enable_text[] = {
	RT5511_NAME_DACDRC1_EN, RT5511_NAME_DACDRC2_EN, RT5511_NAME_DACDRC3_EN,
	RT5511_NAME_DACDRC4_EN, RT5511_NAME_ADCDRC1_EN, RT5511_NAME_ADCDRC2_EN
};

static int rt5511_get_drc_index(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(drc_enable_text); i++) {
		if (strcmp(name, drc_enable_text[i]) == 0)
			return i;
	}

	return -EINVAL;
}

static int rt5511_get_drc_en_state(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int drc = rt5511_get_drc_index(kcontrol->id.name);

	ucontrol->value.enumerated.item[0] = rt5511->drc_en[drc];
	return 0;
}

static int rt5511_set_drc_en_state(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	u32 reg, shift;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int drc = rt5511_get_drc_index(kcontrol->id.name);
	int value = ucontrol->value.integer.value[0];

	if (drc < 0)
		return drc;

	if (drc < RT5511_DRC_DAC_REG_NUM)
		reg = RT5511_DAC_DRC_EQ_EN;
	else
		reg = RT5511_ADC_CTRL;

	shift = RT5511_DRCEnBit[drc];

	ret = rt5511_write_eqdrc_en_state(codec, reg, shift, value);
	if (ret >= 0)
		rt5511->drc_en[drc] = value;

	return ret;
}

/*
 * [BLOCK] DRC_NG Enable State KControl
 */

static const char * const drc_ng_enable_text[] = {
	"Disable", "Enable"
};

static const struct soc_enum drc_ng_enable_enum[] = {
	SOC_ENUM_SINGLE(RT5511_DRC_ADC_SEL, 1, 2, drc_ng_enable_text),
	SOC_ENUM_SINGLE(RT5511_DRC_ADC_SEL, 0, 2, drc_ng_enable_text),
};

/*
 * [BLOCK] DRE Control
 */

#define DRE_EN_BYTE		(0x01)
#define DRE_EN_BITS		(0x02)

static int rt5511_get_dre_en_state(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	/*
		Only for driver behavior.
		If you enable/disable DRE by debug tool,
		then this state maybe will become unsynchronized.
	*/

	ucontrol->value.enumerated.item[0] =
		(rt5511->dre_cfg[DRE_EN_BYTE] & DRE_EN_BITS) ? 1 : 0;
	return 0;
}

static int rt5511_set_dre_en_state(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	int value = ucontrol->value.integer.value[0];

	rt5511->dre_cfg[DRE_EN_BYTE] &= ~(DRE_EN_BITS);

	if (value)
		rt5511->dre_cfg[DRE_EN_BYTE] |= DRE_EN_BITS;

	return rt5511_bulk_write(codec->control_data,
		RT5511A_DRE_CTRL, RT5511A_DRE_REG_SIZE, rt5511->dre_cfg);
}

static const char * const dre_en_state_text[] = {
	"Disable", "Enable"
};

static const struct soc_enum dre_en_state_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		ARRAY_SIZE(dre_en_state_text), dre_en_state_text),
};

static const struct snd_kcontrol_new rt5511A_dre_en_state_controls[] = {
	SOC_ENUM_EXT("DRE Enable", dre_en_state_enum[0],
		rt5511_get_dre_en_state, rt5511_set_dre_en_state)
};

static void rt5511_dre_init(struct snd_soc_codec *codec)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	if (rt5511->revision < RT5511_VID_5511A)
		return;

	rt5511_bulk_write(codec->control_data,
		RT5511A_DRE_CTRL, RT5511A_DRE_REG_SIZE, rt5511->dre_cfg);
}

/*
 * [BLOCK] RT5511 Digital Sound KControl
 */

static const char * const dac_mix_level_text[] = {
	"x0", "x1/4", "x1/2", "x1"
};

static const struct soc_enum dac_mix_level_enum[] = {
	SOC_ENUM_SINGLE(RT5511_DAC_L_MIX, 6, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_DAC_L_MIX, 4, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_DAC_L_MIX, 2, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_DAC_L_MIX, 0, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_DAC_R_MIX, 6, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_DAC_R_MIX, 4, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_DAC_R_MIX, 2, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_DAC_R_MIX, 0, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_L_MIX, 6, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_L_MIX, 4, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_L_MIX, 2, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_L_MIX, 0, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_R_MIX, 6, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_R_MIX, 4, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_R_MIX, 2, 4, dac_mix_level_text),
	SOC_ENUM_SINGLE(RT5511_AUD_TX2_R_MIX, 0, 4, dac_mix_level_text),
};

static const char * const aif_left_source_text[] = {
	"Left", "Right",
};

static const char * const aif_right_source_text[] = {
	"Right", "Left",
};

static const struct soc_enum aif_source_enum[] = {
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 0, 2, aif_left_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 1, 2, aif_right_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 2, 2, aif_left_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 3, 2, aif_right_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 8, 2, aif_left_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 9, 2, aif_right_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 10, 2, aif_left_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 11, 2, aif_right_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 16, 2, aif_left_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 17, 2, aif_right_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 18, 2, aif_left_source_text),
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 19, 2, aif_right_source_text),
};

/* Discard the LSB to match driver style */
static const DECLARE_TLV_DB_SCALE(digital1_tlv, -10375, 25, 0);
static const DECLARE_TLV_DB_SCALE(digital2_tlv, -12750, 50, 1);

static const char * const onoff_mode_text[] = {
	"Off", "On"
};

static const struct soc_enum virt_ctrl_state_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(onoff_mode_text), onoff_mode_text),
};

#define RT_SOC_VIRT_CTRL(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_ext, \
	.get = get_virt_ctrl_state, \
	.put = set_virt_ctrl_state, \
	.private_value = (unsigned long)&xenum }

static unsigned int rt5511_get_virt_ctrl_mask(const char *name)
{
	unsigned int mask = 0;
	if (strcmp(name, "BypassCmd") == 0)
		mask = RT5511_VIRT_BYPASS_CODEC_W;
	else if (strcmp(name, "BypassCache") == 0)
		mask = RT5511_VIRT_BYPASS_CACHE_R;
	else if (strcmp(name, "AIF1 Auto TriState") == 0)
		mask = RT5511_VIRT_AIF1_AUTO_TRISTATE;
	else if (strcmp(name, "AIF2 Auto TriState") == 0)
		mask = RT5511_VIRT_AIF2_AUTO_TRISTATE;
	else if (strcmp(name, "AIF3 Auto TriState") == 0)
		mask = RT5511_VIRT_AIF3_AUTO_TRISTATE;
	else if (strcmp(name, "AIF1 Auto SkipMute") == 0)
		mask = RT5511_VIRT_AIF1_AUTO_SKIPMUTE;
	else if (strcmp(name, "AIF2 Auto SkipMute") == 0)
		mask = RT5511_VIRT_AIF2_AUTO_SKIPMUTE;

	return mask;
}

static int get_virt_ctrl_state(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	unsigned int mask = rt5511_get_virt_ctrl_mask(kcontrol->id.name);

	ucontrol->value.integer.value[0] = (virt_ctrl_regdat&mask) ? 1 : 0;
	return 0;
}

static int set_virt_ctrl_state(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	unsigned int mask = rt5511_get_virt_ctrl_mask(kcontrol->id.name);

	if (ucontrol->value.integer.value[0])
		virt_ctrl_regdat |= mask;
	else
		virt_ctrl_regdat &= ~(mask);

	RT_DBG("VirtCtrl[0x%08x] : %s\n", mask,
		onoff_mode_text[ucontrol->value.integer.value[0]]);

	return 0;
}

static const struct snd_kcontrol_new rt5511_snd_controls[] = {

/* dac mixer input level */
	SOC_ENUM("DACL Mixer AIF2 Input", dac_mix_level_enum[0]),
	SOC_ENUM("DACL Mixer AIF1 Input", dac_mix_level_enum[1]),
	SOC_ENUM("DACL Mixer ADCL Input", dac_mix_level_enum[2]),
	SOC_ENUM("DACL Mixer ADCR Input", dac_mix_level_enum[3]),

	SOC_ENUM("DACR Mixer AIF2 Input", dac_mix_level_enum[4]),
	SOC_ENUM("DACR Mixer AIF1 Input", dac_mix_level_enum[5]),
	SOC_ENUM("DACR Mixer ADCL Input", dac_mix_level_enum[6]),
	SOC_ENUM("DACR Mixer ADCR Input", dac_mix_level_enum[7]),

	SOC_ENUM("TX2L Mixer AIF2 Input", dac_mix_level_enum[8]),
	SOC_ENUM("TX2L Mixer AIF1 Input", dac_mix_level_enum[9]),
	SOC_ENUM("TX2L Mixer ADCL Input", dac_mix_level_enum[10]),
	SOC_ENUM("TX2L Mixer ADCR Input", dac_mix_level_enum[11]),

	SOC_ENUM("TX2R Mixer AIF2 Input", dac_mix_level_enum[12]),
	SOC_ENUM("TX2R Mixer AIF1 Input", dac_mix_level_enum[13]),
	SOC_ENUM("TX2R Mixer ADCL Input", dac_mix_level_enum[14]),
	SOC_ENUM("TX2R Mixer ADCR Input", dac_mix_level_enum[15]),

/* aif source select */
	SOC_ENUM("AIF1ADCL Source", aif_source_enum[0]),
	SOC_ENUM("AIF1ADCR Source", aif_source_enum[1]),
	SOC_ENUM("AIF1DACL Source", aif_source_enum[2]),
	SOC_ENUM("AIF1DACR Source", aif_source_enum[3]),

	SOC_ENUM("AIF2ADCL Source", aif_source_enum[4]),
	SOC_ENUM("AIF2ADCR Source", aif_source_enum[5]),
	SOC_ENUM("AIF2DACL Source", aif_source_enum[6]),
	SOC_ENUM("AIF2DACR Source", aif_source_enum[7]),

	SOC_ENUM("AIF3ADCL Source", aif_source_enum[8]),
	SOC_ENUM("AIF3ADCR Source", aif_source_enum[9]),
	SOC_ENUM("AIF3DACL Source", aif_source_enum[10]),
	SOC_ENUM("AIF3DACR Source", aif_source_enum[11]),

/* Digital Volume */
	SOC_DOUBLE_TLV("DAC PGA Volume", RT5511_DAC_VOL,
	17, 1, 511, 1, digital1_tlv),
	SOC_DOUBLE_TLV("TX2 PGA Volume", RT5511_TX2_VOL,
	17, 1, 511, 1, digital1_tlv),
	SOC_DOUBLE_TLV("AIF2DAC PGA Volume", RT5511_AUD_DAC1_VOL,
	17, 1, 511, 1, digital1_tlv),
	SOC_DOUBLE_TLV("AIF1DAC PGA Volume", RT5511_AUD_DAC2_VOL,
	17, 1, 511, 1, digital1_tlv),
	SOC_DOUBLE_TLV("AIF2ADC PGA Volume", RT5511_AUD_ADC1_VOL,
	17, 1, 511, 1, digital1_tlv),
	SOC_DOUBLE_TLV("AIF1ADC PGA Volume", RT5511_AUD_ADC2_VOL,
	17, 1, 511, 1, digital1_tlv),
	SOC_DOUBLE_TLV("HPDAC PGA Volume", RT5511_SDM_VOL,
	24, 16, 255, 1, digital2_tlv),
	SOC_DOUBLE_TLV("SPKDAC PGA Volume", RT5511_SDM_VOL,
	8, 0, 255, 1, digital2_tlv),

	SOC_SINGLE_TLV("ADC Fix PGA1 Volume",
		RT5511_ADC_FG_VOL, 17, 511, 1, digital1_tlv),

	SOC_SINGLE_TLV("ADC Fix PGA2 Volume",
		RT5511_ADC_FG_VOL, 1, 511, 1, digital1_tlv),

/* EQ/DRC enable state */
	SOC_ENUM_EXT(RT5511_NAME_EQ1_ENABLE, eqdrc_en_state_enum[0],
		rt5511_get_eq_en_state, rt5511_set_eq_en_state),

	SOC_ENUM_EXT(RT5511_NAME_EQ2_ENABLE, eqdrc_en_state_enum[0],
		rt5511_get_eq_en_state, rt5511_set_eq_en_state),

	SOC_ENUM_EXT(RT5511_NAME_DACDRC1_EN, eqdrc_en_state_enum[0],
		rt5511_get_drc_en_state, rt5511_set_drc_en_state),

	SOC_ENUM_EXT(RT5511_NAME_DACDRC2_EN, eqdrc_en_state_enum[0],
		rt5511_get_drc_en_state, rt5511_set_drc_en_state),

	SOC_ENUM_EXT(RT5511_NAME_DACDRC3_EN, eqdrc_en_state_enum[0],
		rt5511_get_drc_en_state, rt5511_set_drc_en_state),

	SOC_ENUM_EXT(RT5511_NAME_DACDRC4_EN, eqdrc_en_state_enum[0],
		rt5511_get_drc_en_state, rt5511_set_drc_en_state),

	SOC_ENUM_EXT(RT5511_NAME_ADCDRC1_EN, eqdrc_en_state_enum[0],
		rt5511_get_drc_en_state, rt5511_set_drc_en_state),

	SOC_ENUM_EXT(RT5511_NAME_ADCDRC2_EN, eqdrc_en_state_enum[0],
		rt5511_get_drc_en_state, rt5511_set_drc_en_state),

	SOC_ENUM("DAC Noise Gate", drc_ng_enable_enum[0]),
	SOC_ENUM("ADC Noise Gate", drc_ng_enable_enum[1]),

/* Special Control */
	RT_SOC_VIRT_CTRL("BypassCmd", virt_ctrl_state_enum[0]),
	RT_SOC_VIRT_CTRL("BypassCache", virt_ctrl_state_enum[0]),
	RT_SOC_VIRT_CTRL("AIF1 Auto TriState", virt_ctrl_state_enum[0]),
	RT_SOC_VIRT_CTRL("AIF2 Auto TriState", virt_ctrl_state_enum[0]),
	RT_SOC_VIRT_CTRL("AIF3 Auto TriState", virt_ctrl_state_enum[0]),
	RT_SOC_VIRT_CTRL("AIF1 Auto SkipMute", virt_ctrl_state_enum[0]),
	RT_SOC_VIRT_CTRL("AIF2 Auto SkipMute", virt_ctrl_state_enum[0]),
};

/* AIF Tri-state */
static const char * const aif_tri_state_text[] = {
	"Off", "On",
};

static const struct soc_enum aif_tri_state_enum[] = {
	SOC_ENUM_SINGLE(RT5511A_AIF_TRI_STATE, 5, 2, aif_tri_state_text),
	SOC_ENUM_SINGLE(RT5511A_AIF_TRI_STATE, 6, 2, aif_tri_state_text),
	SOC_ENUM_SINGLE(RT5511A_AIF_TRI_STATE, 7, 2, aif_tri_state_text),
};

/* AIF Gating */
static const char * const aif_gating_text[] = {
	"Off", "On",
};

static const struct soc_enum aif_gating_enum[] = {
	SOC_ENUM_SINGLE(RT5511A_AIF_TRI_STATE, 0, 2, aif_tri_state_text),
	SOC_ENUM_SINGLE(RT5511A_AIF_TRI_STATE, 1, 2, aif_tri_state_text),
	SOC_ENUM_SINGLE(RT5511A_AIF_TRI_STATE, 2, 2, aif_tri_state_text),
};

/* AIF3 Ref */

static const char * const aif3_ref_text[] = {
	"AIF1", "AIF2",
};

static const struct soc_enum aif3_ref_enum[] = {
	SOC_ENUM_SINGLE(RT5511_AUD_FMT, 4, 2, aif3_ref_text),
};

static const struct snd_kcontrol_new rt5511A_snd_controls[] = {
	SOC_ENUM("AIF1 Tri-state", aif_tri_state_enum[0]),
	SOC_ENUM("AIF2 Tri-state", aif_tri_state_enum[1]),
	SOC_ENUM("AIF3 Tri-state", aif_tri_state_enum[2]),
	SOC_ENUM("AIF1 Gating", aif_gating_enum[0]),
	SOC_ENUM("AIF2 Gating", aif_gating_enum[1]),
	SOC_ENUM("AIF3 Gating", aif_gating_enum[2]),

	SOC_ENUM("AIF3 Ref", aif3_ref_enum[0]),
};

static const struct snd_soc_dapm_widget rt5511_lateclk_widgets[] = {

};

/*
 * [BLOCK] RT5511 Digital Routing Control
 */

static int rt5511_hpdac_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
#if OPTIMIZE_HP_POP_NOISE
	int index;
	const unsigned int hpdac_reg[2] = {
		RT5511_LHP_DRV_VOL, RT5511_RHP_DRV_VOL };
	static unsigned int hpdac_save[2] = {0};

	if (strcmp(w->name, "LHPDAC") == 0)
		index = 0;
	else
		index = 1;

	RT_DBG("event (%d)\n", event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		hpdac_save[index] = snd_soc_read(w->codec, hpdac_reg[index]);
		snd_soc_write(w->codec, hpdac_reg[index], 0x0f);
		break;

	case SND_SOC_DAPM_POST_PMU:
		snd_soc_write(w->codec, hpdac_reg[index], hpdac_save[index]);
		break;
	}
#endif

	return 0;
}

static const struct snd_soc_dapm_widget rt5511_dac_widgets[] = {
	SND_SOC_DAPM_DAC_E("LHPDAC", NULL, RT5511_MIX_EN_DAC_EN, 3, 0,
		rt5511_hpdac_event, SND_SOC_DAPM_PRE_PMU|SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_DAC_E("RHPDAC", NULL, RT5511_MIX_EN_DAC_EN, 2, 0,
		rt5511_hpdac_event, SND_SOC_DAPM_PRE_PMU|SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_DAC("LSPKDAC", NULL, RT5511_MIX_EN_DAC_EN, 1, 0),
	SND_SOC_DAPM_DAC("RSPKDAC", NULL, RT5511_MIX_EN_DAC_EN, 0, 0),
};

static const struct snd_soc_dapm_widget rt5511_adc_widgets[] = {
	SND_SOC_DAPM_ADC("LADC", NULL, RT5511_ADC_MIC, 7, 0),
	SND_SOC_DAPM_ADC("RADC", NULL, RT5511_ADC_MIC, 6, 0),
};

static const char * const aif1_si_mux_text[] = {
	"AIF1", "AIF3",
};

static const char * const aif2_si_mux_text[] = {
	"AIF2", "AIF3",
};

static const struct soc_enum aif1_si_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL1, 0, 2, aif1_si_mux_text);

static const struct soc_enum aif2_si_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL1, 1, 2, aif2_si_mux_text);

static const struct snd_kcontrol_new aif1_si_mux =
	SOC_DAPM_ENUM("AIF1 SI Mux", aif1_si_mux_enum);

static const struct snd_kcontrol_new aif2_si_mux =
	SOC_DAPM_ENUM("AIF2 SI Mux", aif2_si_mux_enum);


static const char * const aif_dac1_mux_text[] = {
	"PATH1", "AIF3", "Mix"
};

static const char * const aif_dac2_mux_text[] = {
	"PATH2", "AIF3", "Mix"
};

static const struct soc_enum aif_dac1_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 24, 3, aif_dac1_mux_text);

static const struct soc_enum aif_dac2_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 26, 3, aif_dac2_mux_text);

static const struct snd_kcontrol_new aif_dac1_mux =
	SOC_DAPM_ENUM("AIF DAC1 Mux", aif_dac1_mux_enum);

static const struct snd_kcontrol_new aif_dac2_mux =
	SOC_DAPM_ENUM("AIF DAC2 Mux", aif_dac2_mux_enum);


static const char * const aif1_2_adc_mux_text[] = {
	"Internal", "AIF3/SLIMBus"
};

static const char * const aif3_adc_mux_text[] = {
	"PATH2", "PATH1"
};

static const struct soc_enum aif1_adc_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 6, 2, aif1_2_adc_mux_text);

static const struct soc_enum aif2_adc_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 14, 2, aif1_2_adc_mux_text);

static const struct soc_enum aif3_adc_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL2, 22, 2, aif3_adc_mux_text);

static const struct snd_kcontrol_new aif1_adc_mux =
	SOC_DAPM_ENUM("AIF1 ADC Mux", aif1_adc_mux_enum);

static const struct snd_kcontrol_new aif2_adc_mux =
	SOC_DAPM_ENUM("AIF2 ADC Mux", aif2_adc_mux_enum);

static const struct snd_kcontrol_new aif3_adc_mux =
	SOC_DAPM_ENUM("AIF3 ADC Mux", aif3_adc_mux_enum);

/*
 * [BLOCK] AIF1 ADC output select (fake range)
 */

enum rt_aif1_adc_out_real_sel {
	RT_AIF1_ADC_OUT_REAL_SEL_ADC = 0,
	RT_AIF1_ADC_OUT_REAL_SEL_DAC = 1,
	RT_AIF1_ADC_OUT_REAL_SEL_MIX = 2,
	RT_AIF1_ADC_OUT_REAL_SEL_REC = 3,
	RT_AIF1_ADC_OUT_REAL_SEL_MASK = 0x03,
};

enum rt_aif1_adc_out_sel {
	RT_AIF1_ADC_OUT_SEL_ADC = 0,
	RT_AIF1_ADC_OUT_SEL_DAC = 1,
	RT_AIF1_ADC_OUT_SEL_MIX = 2,
	RT_AIF1_ADC_OUT_SEL_ADC_PLUS = 3,
	RT_AIF1_ADC_OUT_SEL_DAC_PLUS = 4,
	RT_AIF1_ADC_OUT_SEL_REC = 5,
	RT_AIF1_ADC_OUT_SEL_MAX = 7,
};

static const char * const aif1_adc_out_mux_text[] = {
	"ADC", "DAC", "Mix", "ADC+", "DAC+", "REC", "RSVD1", "RSVD2",
};

/* Real maximum value is 4, not 8 */
static const struct soc_enum aif1_adcl_out_mux_enum =
	SOC_ENUM_SINGLE(RT5511_BASS_3D_EN, 6, 8, aif1_adc_out_mux_text);

static const struct soc_enum aif1_adcr_out_mux_enum =
	SOC_ENUM_SINGLE(RT5511_BASS_3D_EN, 4, 8, aif1_adc_out_mux_text);

#define RT_AIF1_ADC_OUT_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = aif1_adc_out_get_enum_double, \
	.put = aif1_adc_out_put_enum_double, \
	.private_value = (unsigned long)&xenum }

static int aif1_adc_out_get_enum_double(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	ret = snd_soc_dapm_get_enum_double(kcontrol, ucontrol);
	ucontrol->value.enumerated.item[0] &= RT_AIF1_ADC_OUT_REAL_SEL_MASK;
	if (ucontrol->value.enumerated.item[0] == RT_AIF1_ADC_OUT_REAL_SEL_REC)
		ucontrol->value.enumerated.item[0] = RT_AIF1_ADC_OUT_SEL_REC;
	return ret;
}

static int aif1_adc_out_put_enum_double(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int mask, bitmask;
	unsigned int val, extraval;
	int retval;

	for (bitmask = 1; bitmask < e->max; bitmask <<= 1)
		;
	mask = (bitmask - 1) << e->shift_l;

	val = ucontrol->value.enumerated.item[0];

	retval = snd_soc_read(codec, RT5511_BASS_3D_EN);
	retval = (retval & mask) >> e->shift_l;

	extraval = retval & (~RT_AIF1_ADC_OUT_REAL_SEL_MASK);
	retval = retval & RT_AIF1_ADC_OUT_REAL_SEL_MASK;

	switch (val) {
	case RT_AIF1_ADC_OUT_SEL_ADC:
	case RT_AIF1_ADC_OUT_SEL_DAC:
	case RT_AIF1_ADC_OUT_SEL_MIX:
		retval = val;
		break;
	case RT_AIF1_ADC_OUT_SEL_ADC_PLUS:
		if (retval == RT_AIF1_ADC_OUT_REAL_SEL_DAC)
			retval = RT_AIF1_ADC_OUT_REAL_SEL_MIX;
		else if (retval == RT_AIF1_ADC_OUT_REAL_SEL_REC)
			retval = RT_AIF1_ADC_OUT_REAL_SEL_ADC;
		break;
	case RT_AIF1_ADC_OUT_SEL_DAC_PLUS:
		if (retval == RT_AIF1_ADC_OUT_REAL_SEL_ADC)
			retval = RT_AIF1_ADC_OUT_REAL_SEL_MIX;
		else if (retval == RT_AIF1_ADC_OUT_REAL_SEL_REC)
			retval = RT_AIF1_ADC_OUT_REAL_SEL_DAC;
		break;
	case RT_AIF1_ADC_OUT_SEL_REC:
		retval = RT_AIF1_ADC_OUT_REAL_SEL_REC;
		break;
	default:	/* reserved */
		break;
	}

	ucontrol->value.enumerated.item[0] = extraval + retval;
	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static const struct snd_kcontrol_new aif1_adcl_out_mux =
	RT_AIF1_ADC_OUT_ENUM("AIF1 ADCL Out Mux", aif1_adcl_out_mux_enum);

static const struct snd_kcontrol_new aif1_adcr_out_mux =
	RT_AIF1_ADC_OUT_ENUM("AIF1 ADCR Out Mux", aif1_adcr_out_mux_enum);

static const char * const aif1_so_mux_text[] = {
	"AIF1", "AIF3",
};

static const char * const aif2_so_mux_text[] = {
	"AIF2", "AIF3",
};

static const char * const aif3_so_mux_text[] = {
	"AIF3 Out", "AIF1 In", "AIF1 Out", "AIF2 In", "AIF2 Out",
};

static const struct soc_enum aif1_so_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL1, 2, 2, aif1_so_mux_text);

static const struct soc_enum aif2_so_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL1, 3, 2, aif2_so_mux_text);

static const struct soc_enum aif3_so_mux_enum =
	SOC_ENUM_SINGLE(RT5511_AIF_CTRL1, 4, 5, aif3_so_mux_text);


static const struct snd_kcontrol_new aif1_so_mux =
	SOC_DAPM_ENUM("AIF1 SO Mux", aif1_so_mux_enum);

static const struct snd_kcontrol_new aif2_so_mux =
	SOC_DAPM_ENUM("AIF2 SO Mux", aif2_so_mux_enum);

static const struct snd_kcontrol_new aif3_so_mux =
	SOC_DAPM_ENUM("AIF3 SO Mux", aif3_so_mux_enum);


static inline int rt5511_enable_tristate(
	struct snd_soc_codec *codec, int id, int tristate);

static inline bool rt5511_is_auto_tristate(
	struct snd_soc_codec *codec, int id)
{
	int index;
	unsigned int mask;

	index = id-1;
	mask = 1<<(index+RT5511_VIRT_AIF_AUTO_TRISTATE_SHIFT);

	return virt_ctrl_regdat & mask;
}

static int rt5511_aifclk_ev(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol, int event)
{
	int index, id;
	bool auto_tristate;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(w->codec);

	if (strcmp(w->name, "AIF1CLK") == 0)
		index = 0;
	else if (strcmp(w->name, "AIF2CLK") == 0)
		index = 1;
	else
		index = 2;

	id = index+1;
	auto_tristate = rt5511_is_auto_tristate(w->codec, id);

	/* RT_DBG("event (%d), AIF%d:%d\n",
		event, index+1, auto_tristate ? 1 : 0); */

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rt5511->aifclk_en[index] = 1;
		if (auto_tristate)
			rt5511_enable_tristate(w->codec, id, 0);
		break;

	case SND_SOC_DAPM_POST_PMD:
		rt5511->aifclk_en[index] = 0;
		if (auto_tristate)
			rt5511_enable_tristate(w->codec, id, 1);
		break;
	}

	return 0;
}


static int rt5511_pre_ev(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	RT_DBG("event (%d)\n", event);
	return 0;
}

static int rt5511_post_ev(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	RT_DBG("event (%d)\n", event);
	return 0;
}

#define BT_BYPASS_MODE	1

static const struct snd_soc_dapm_widget rt5511_dapm_widgets[] = {

/* DAC Output Mixer */
	SND_SOC_DAPM_MIXER("DACL Mixer", RT5511_MUTE_CTRL, 7, 1, NULL, 0),
	SND_SOC_DAPM_MIXER("DACR Mixer", RT5511_MUTE_CTRL, 6, 1, NULL, 0),
	SND_SOC_DAPM_MIXER("TX2L Mixer", RT5511_MUTE_CTRL, 5, 1, NULL, 0),
	SND_SOC_DAPM_MIXER("TX2R Mixer", RT5511_MUTE_CTRL, 4, 1, NULL, 0),

/* AIF In */
#if BT_BYPASS_MODE
	SND_SOC_DAPM_MUX("AIF1 SI Mux", SND_SOC_NOPM, 0, 0, &aif1_si_mux),
	SND_SOC_DAPM_MUX("AIF2 SI Mux", SND_SOC_NOPM, 0, 0, &aif2_si_mux),
#endif

	SND_SOC_DAPM_MUX("AIF DAC1 Mux", SND_SOC_NOPM, 0, 0, &aif_dac1_mux),
	SND_SOC_DAPM_MUX("AIF DAC2 Mux", SND_SOC_NOPM, 0, 0, &aif_dac2_mux),

	SND_SOC_DAPM_MIXER("AIF DAC1 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("AIF DAC2 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_AIF_IN("AIF1DAC", NULL, 0, RT5511_SOURCE_ENABLE, 2, 0),
	SND_SOC_DAPM_AIF_IN("AIF2DAC", NULL, 0, RT5511_SOURCE_ENABLE, 3, 0),

#if BT_BYPASS_MODE
	SND_SOC_DAPM_AIF_IN(
		"AIF1DACDAT", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN(
		"AIF2DACDAT", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
#else
	SND_SOC_DAPM_AIF_IN(
		"AIF1DACDAT", "AIF1 Playback", 0, RT5511_AIF_CTRL1, 0, 1),
	SND_SOC_DAPM_AIF_IN(
		"AIF2DACDAT", "AIF2 Playback", 0, RT5511_AIF_CTRL1, 1, 1),
#endif
	SND_SOC_DAPM_AIF_IN(
		"AIF3DACDAT", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),

/* AIF Out */

#if BT_BYPASS_MODE
	SND_SOC_DAPM_MUX("AIF1 SO Mux", SND_SOC_NOPM, 0, 0, &aif1_so_mux),
	SND_SOC_DAPM_MUX("AIF2 SO Mux", SND_SOC_NOPM, 0, 0, &aif2_so_mux),
	SND_SOC_DAPM_MUX("AIF3 SO Mux", SND_SOC_NOPM, 0, 0, &aif3_so_mux),
#endif

	SND_SOC_DAPM_MUX("AIF1 ADC Mux", SND_SOC_NOPM, 0, 0, &aif1_adc_mux),
	SND_SOC_DAPM_MUX("AIF2 ADC Mux", SND_SOC_NOPM, 0, 0, &aif2_adc_mux),
	SND_SOC_DAPM_MUX("AIF3 ADC Mux", SND_SOC_NOPM, 0, 0, &aif3_adc_mux),

	SND_SOC_DAPM_MUX(
		"AIF1 ADCL Out Mux", SND_SOC_NOPM, 0, 0, &aif1_adcl_out_mux),
	SND_SOC_DAPM_MUX(
		"AIF1 ADCR Out Mux", SND_SOC_NOPM, 0, 0, &aif1_adcr_out_mux),

	SND_SOC_DAPM_AIF_OUT("AIF1ADC", NULL, 0, RT5511_SOURCE_ENABLE, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2ADC", NULL, 0, RT5511_SOURCE_ENABLE, 1, 0),

#if BT_BYPASS_MODE
	SND_SOC_DAPM_AIF_OUT(
		"AIF1ADCDAT", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT(
		"AIF2ADCDAT", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
#else
	SND_SOC_DAPM_AIF_OUT(
		"AIF1ADCDAT", "AIF1 Capture", 0, RT5511_AIF_CTRL1, 2, 1),
	SND_SOC_DAPM_AIF_OUT(
		"AIF2ADCDAT", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
#endif
	SND_SOC_DAPM_AIF_OUT(
		"AIF3ADCDAT", "AIF3 Capture", 0, SND_SOC_NOPM, 0, 0),

/* CLK Source */
	SND_SOC_DAPM_SUPPLY("AIF1CLK", SND_SOC_NOPM, 0, 0, rt5511_aifclk_ev,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("AIF2CLK", SND_SOC_NOPM, 0, 0, rt5511_aifclk_ev,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("AIF3CLK", SND_SOC_NOPM, 0, 0, rt5511_aifclk_ev,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PRE("Pre Power", rt5511_pre_ev),
	SND_SOC_DAPM_POST("Post Power", rt5511_post_ev),
};

static const struct snd_soc_dapm_route intercon[] = {

/* AIF1/DAC2 input */
#if BT_BYPASS_MODE
	{ "AIF1 SI Mux", "AIF1", "AIF1DACDAT" },
	{ "AIF1 SI Mux", "AIF3", "AIF3DACDAT" },

	{ "AIF DAC2 Mixer", NULL, "AIF1 SI Mux" },
	{ "AIF DAC2 Mixer", NULL, "AIF3DACDAT" },

	{ "AIF DAC2 Mux", "PATH2", "AIF1 SI Mux" },
#else
	{ "AIF DAC2 Mixer", NULL, "AIF1DACDAT" },
	{ "AIF DAC2 Mixer", NULL, "AIF3DACDAT" },

	{ "AIF DAC2 Mux", "PATH2", "AIF1DACDAT" },
#endif
	{ "AIF DAC2 Mux", "AIF3", "AIF3DACDAT" },
	{ "AIF DAC2 Mux", "Mix", "AIF DAC2 Mixer" },

	{ "AIF1DAC", NULL, "AIF DAC2 Mux" },

/* AIF2/DAC1 input */
#if BT_BYPASS_MODE
	{ "AIF2 SI Mux", "AIF2", "AIF2DACDAT" },
	{ "AIF2 SI Mux", "AIF3", "AIF3DACDAT" },

	{ "AIF DAC1 Mixer", NULL, "AIF2 SI Mux" },
	{ "AIF DAC1 Mixer", NULL, "AIF3DACDAT" },

	{ "AIF DAC1 Mux", "PATH1", "AIF2 SI Mux" },
#else
	{ "AIF DAC1 Mixer", NULL, "AIF2DACDAT" },
	{ "AIF DAC1 Mixer", NULL, "AIF3DACDAT" },

	{ "AIF DAC1 Mux", "PATH1", "AIF2DACDAT" },
#endif
	{ "AIF DAC1 Mux", "AIF3", "AIF3DACDAT" },
	{ "AIF DAC1 Mux", "Mix", "AIF DAC1 Mixer" },

	{ "AIF2DAC", NULL, "AIF DAC1 Mux" },

/* AIF1/DAC2 Output */
	{ "AIF1 ADCL Out Mux", "ADC", "LADC" },
	{ "AIF1 ADCL Out Mux", "DAC", "AIF2DAC" },
	{ "AIF1 ADCL Out Mux", "Mix", "LADC" },
	{ "AIF1 ADCL Out Mux", "Mix", "AIF2DAC" },
	{ "AIF1 ADCL Out Mux", NULL, "LREC Virt Mux" },

	{ "AIF1 ADCR Out Mux", "ADC", "RADC" },
	{ "AIF1 ADCR Out Mux", "DAC", "AIF2DAC" },
	{ "AIF1 ADCR Out Mux", "Mix", "RADC" },
	{ "AIF1 ADCR Out Mux", "Mix", "AIF2DAC" },
	{ "AIF1 ADCR Out Mux", NULL, "RREC Virt Mux" },

	{ "AIF1ADC", NULL, "AIF1 ADCL Out Mux" },
	{ "AIF1ADC", NULL, "AIF1 ADCR Out Mux" },

	{ "AIF1 ADC Mux", "Internal", "AIF1ADC" },
	{ "AIF1 ADC Mux", "AIF3/SLIMBus", "AIF3DACDAT" },

#if BT_BYPASS_MODE
	{ "AIF1 SO Mux", "AIF1", "AIF1 ADC Mux" },
	{ "AIF1 SO Mux", "AIF3", "AIF3DACDAT" },

	{ "AIF1ADCDAT", NULL, "AIF1 SO Mux" },
#else
	{ "AIF1ADCDAT", NULL, "AIF1 ADC Mux" },
#endif

/* AIF2/DAC1 Output */
	{ "TX2L Mixer", NULL, "AIF1DAC" },
	{ "TX2L Mixer", NULL, "AIF2DAC" },
	{ "TX2L Mixer", NULL, "LADC" },
	{ "TX2L Mixer", NULL, "RADC" },

	{ "TX2R Mixer", NULL, "AIF1DAC" },
	{ "TX2R Mixer", NULL, "AIF2DAC" },
	{ "TX2R Mixer", NULL, "LADC" },
	{ "TX2R Mixer", NULL, "RADC" },

	{ "AIF2ADC", NULL, "TX2L Mixer" },
	{ "AIF2ADC", NULL, "TX2R Mixer" },

	{ "AIF2 ADC Mux", "Internal", "AIF2ADC" },
	{ "AIF2 ADC Mux", "AIF3/SLIMBus", "AIF3DACDAT" },

#if BT_BYPASS_MODE
	{ "AIF2 SO Mux", "AIF2", "AIF2 ADC Mux" },
	{ "AIF2 SO Mux", "AIF3", "AIF3DACDAT" },

	{ "AIF2ADCDAT", NULL, "AIF2 SO Mux" },
#else
	{ "AIF2ADCDAT", NULL, "AIF2 ADC Mux" },
#endif

/* AIF3 output */
	{ "AIF3 ADC Mux", "PATH2", "AIF1ADC" },
	{ "AIF3 ADC Mux", "PATH1", "AIF2ADC" },

#if BT_BYPASS_MODE
	{ "AIF3 SO Mux", "AIF3 Out", "AIF3 ADC Mux" },
	{ "AIF3 SO Mux", "AIF1 Out", "AIF1 ADC Mux" },
	{ "AIF3 SO Mux", "AIF1 In", "AIF1DACDAT" },
	{ "AIF3 SO Mux", "AIF2 Out", "AIF2 ADC Mux" },
	{ "AIF3 SO Mux", "AIF2 In", "AIF2DACDAT" },

	{ "AIF3ADCDAT", NULL, "AIF3 SO Mux" },
#else
	{ "AIF3ADCDAT", NULL, "AIF3 ADC Mux" },
#endif
/* DAC inputs */
	{ "DACL Mixer", NULL, "AIF1DAC" },
	{ "DACL Mixer", NULL, "AIF2DAC" },
	{ "DACL Mixer", NULL, "LADC" },
	{ "DACL Mixer", NULL, "RADC" },

	{ "DACR Mixer", NULL, "AIF1DAC" },
	{ "DACR Mixer", NULL, "AIF2DAC" },
	{ "DACR Mixer", NULL, "LADC" },
	{ "DACR Mixer", NULL, "RADC" },

/* Output stages */
	{ "LREC Virt Mux", "On", "LHPDAC" },
	{ "RREC Virt Mux", "On", "RHPDAC" },

	{ "LHPDAC Virt Mux", "On", "LHPDAC" },
	{ "RHPDAC Virt Mux", "On", "RHPDAC" },

	{ "LSPK Mixer", "LSPKDAC Switch", "LSPKDAC" },
	{ "RSPK Mixer", "RSPKDAC Switch", "RSPKDAC" },

/* CLK Source */
	{ "AIF1DACDAT", NULL, "AIF1CLK" },
	{ "AIF1ADCDAT", NULL, "AIF1CLK" },
	{ "AIF2DACDAT", NULL, "AIF2CLK" },
	{ "AIF2ADCDAT", NULL, "AIF2CLK" },
	{ "AIF3DACDAT", NULL, "AIF3CLK" },
	{ "AIF3ADCDAT", NULL, "AIF3CLK" },
};

static const struct snd_soc_dapm_route rt5511_lateclk_intercon[] = {
	{ "LSPKDAC", NULL, "DACL Mixer"},
	{ "RSPKDAC", NULL, "DACR Mixer"},
	{ "LHPDAC", NULL, "DACL Mixer"},
	{ "RHPDAC", NULL, "DACR Mixer"},
};

static const struct snd_soc_dapm_route rt5511_intercon[] = {

};

/*
 * [BLOCK] PLL & SYSCLK
 */

static int rt5511_set_clkdiv(
	struct snd_soc_dai *codec_dai, int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	RT_DBG("div_id(%d), div(%d)\n", div_id, div);

	if (div_id != RT5511_CLK_DIV_PLL) {
		dev_err(codec->dev, "Invalid clock divider");
		return -EINVAL;
	};

	div = (div << RT5511_PLL_N_I_SHIFT) & RT5511_PLL_N_MASK;
	snd_soc_write(codec, RT5511_PLL_N, div);
	return 0;
}

static int rt5511_set_pll_divisor(struct snd_soc_codec *codec,
	unsigned int freq_in, unsigned int freq_out)
{
	unsigned int r, r1, r2, r3, r4, divI, divF, div, remain;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	RT_DBG("(In=%d, Out=%d)\n", freq_in, freq_out);
	freq_out = c_ckmode_freq[rt5511_calc_ckmode(freq_out)];
	rt5511->real_pll = freq_out;
	divI = freq_out / freq_in;
	BUG_ON(divI > 0xffff);
	remain = freq_out - freq_in * divI;

	r1 = (remain << 4) / freq_in;
	remain = (remain << 4) - freq_in * r1;
	r2 = (remain << 4) / freq_in;
	remain = (remain << 4) - freq_in * r2;
	r3 = (remain << 4) / freq_in;
	remain = (remain << 4) - freq_in * r3;
	r4 = (remain << 4) / freq_in;
	remain = (remain << 4) - freq_in * r4;
	r = (r1 << 12) | (r2 << 8) | (r3 << 4) | (r4);
	BUG_ON(r > 0xffff);
	divI = (divI << RT5511_PLL_N_I_SHIFT) & RT5511_PLL_N_I_MASK;
	divF = (r) & RT5511_PLL_N_F_MASK;
	div = divI | divF;

	RT_DBG("update value: (0x%08x)\n", div);
	snd_soc_update_bits(codec, RT5511_PLL_N, RT5511_PLL_N_MASK, div);
	return 0;
}

static int rt5511_set_pll_source(struct snd_soc_codec *codec, int src)
{
	int ckSel;
	int syncRefSel;

	RT_DBG("Src=%d\n", src);
	if (src >= RT5511_PLL_SRC_MCLK2) {
		syncRefSel = RT5511_SYNC_REF_I2S2;
		src -= 4;
	} else {
		syncRefSel = RT5511_SYNC_REF_I2S1;
	}

	switch (src) {
	case RT5511_PLL_SRC_MCLK1:
	case RT5511_PLL_SRC_BCLK1:
	case RT5511_PLL_SRC_LRCK1:
		ckSel = src - 1;
		break;
	default:
		return -EINVAL;
	}

	rt5511_change_sync_reference(codec, syncRefSel, ckSel);
	return 0;
}

static int __rt5511_set_pll(struct snd_soc_codec *codec, int id, int src,
			    unsigned int freq_in, unsigned int freq_out)
{
	int ret;
	static bool was_enabled; /* false */
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	RT_DBG("id(%d), src(%d), in(%d), out(%d)\n)",
		id, src, freq_in, freq_out);

	switch (src) {
	case RT5511_PLL_SRC_NULL:
		/* Allow no source specification when stopping */
		if (freq_out)
			return -EINVAL;
		src = rt5511->pll.src;
		break;
	case RT5511_PLL_SRC_MCLK1:
	case RT5511_PLL_SRC_BCLK1:
	case RT5511_PLL_SRC_LRCK1:
	case RT5511_PLL_SRC_MCLK2:
	case RT5511_PLL_SRC_BCLK2:
	case RT5511_PLL_SRC_LRCK2:
		break;
	default:
		return -EINVAL;
	}

	/* Are we changing anything? */
	if (rt5511->pll.src == src &&
	    rt5511->pll.in == freq_in && rt5511->pll.out == freq_out)
		return 0;

	/* TBD: Make sure that we're not providing SYSCLK right now */

	/* We always need to disable the PLL while reconfiguring */
	rt5511_set_pll_freerun(codec, true);

	/* If we're stopping the PLL redo the old config - no
	 * registers will actually be written but we avoid GCC flow
	 * analysis bugs spewing warnings.
	 */

	if (freq_out)
		ret = rt5511_set_pll_divisor(
			codec, freq_in, freq_out);
	else
		ret = rt5511_set_pll_divisor(
			codec, rt5511->pll.in, rt5511->pll.out);

	if (ret < 0)
		return ret;

	rt5511_set_pll_source(codec, src);

	/* Enable (with fractional mode if required) */
	if (freq_out) {
		/* Enable VMID if we need it */
		if (!was_enabled) {
			/*
			 * TBD , implement it later
			 * active_reference(codec);
			 * vmid_reference(codec);
			 */
		}

		was_enabled = true;
		usleep_range(5*1000, 10*1000);
	} else if (was_enabled) {
		/* vmid_dereference(codec); */
		/* active_dereference(codec); */
	}

	rt5511->pll.in = freq_in;
	rt5511->pll.out = freq_out;
	rt5511->pll.src = src;
	if (freq_out)
		configure_clock(codec);
	return 0;
}

static int rt5511_set_pll(struct snd_soc_dai *dai, int id, int src,
			  unsigned int freq_in, unsigned int freq_out)
{
	return __rt5511_set_pll(dai->codec, id, src, freq_in, freq_out);
}

static int rt5511_set_dai_sysclk(struct snd_soc_dai *dai,
				 int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	RT_DBG("CLK (%d), Freq(%d), Dir(%d)\n", clk_id, freq, dir);

	rt5511->sysclk_rate = freq;
	switch (clk_id) {
	case RT5511_SYSCLK_PLL:
		rt5511->sysclk = clk_id;
		dev_dbg(dai->dev, "AIF%d using PLL\n", dai->id);
		break;

	case RT5511_SYSCLK_PLL_DIV2:
		rt5511->sysclk = clk_id;
		dev_dbg(dai->dev, "AIF%d using PLL/2\n", dai->id);
		break;

	case RT5511_SYSCLK_MCLK1:
		rt5511->sysclk = clk_id;
		rt5511->mclk[0] = freq;
		dev_dbg(dai->dev, "AIF%d using MCLK1 at %uHz\n",
			dai->id, freq);
		break;

	case RT5511_SYSCLK_MCLK2:
		rt5511->sysclk = clk_id;
		rt5511->mclk[1] = freq;
		dev_dbg(dai->dev, "AIF%d using MCLK2 at %uHz\n",
			dai->id, freq);
		break;

	default:
		return -EINVAL;
	}

	configure_clock(codec);
	return 0;
}

/*
 * [BLOCK] BIAS & VMID
 */

static int rt5511_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	const unsigned int dac_mask =
		RT5511_PATH1_DAC_EN | RT5511_PATH2_DAC_EN;

	const unsigned int bias_mask =
		RT5511_D_FAST_CHARGE | RT5511_D_VMID_EN | RT5511_D_BIAS_EN;

	rt_hubs_set_bias_level(codec, level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		if (codec->dapm.bias_level == SND_SOC_BIAS_STANDBY) {
			RT_DBG("BIAS: STANDBY -> PREPARE\n");

#if	OPTIMIZE_HP_POP_NOISE
			snd_soc_update_bits(codec, RT5511_SOURCE_ENABLE,
				dac_mask, dac_mask);
			msleep(20);
			snd_soc_update_bits(codec, RT5511_SOURCE_ENABLE,
				dac_mask, 0);
#endif

			snd_soc_update_bits(codec, RT5511_BIASE_MIC_CTRL,
					    bias_mask, bias_mask);
			msleep(20);
			snd_soc_update_bits(codec, RT5511_D_VREF_LDO_EN,
				RT5511_D_VCORE_LDO_EN, RT5511_D_VCORE_LDO_EN);
			snd_soc_update_bits(codec, RT5511_BIASE_MIC_CTRL,
				RT5511_D_FAST_CHARGE, 0);
			usleep_range(1000, 2000);

			rt5511_notify_asrc_change(codec, 0);
			/* active_reference(codec); */
		}
		break;

	case SND_SOC_BIAS_STANDBY:
#if 0
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
			pm_runtime_get_sync(codec->dev);
#endif

		if (codec->dapm.bias_level == SND_SOC_BIAS_PREPARE) {
			RT_DBG("BIAS: PREPARE -> STANDBY\n");
			snd_soc_update_bits(codec, RT5511_D_VREF_LDO_EN,
					    RT5511_D_VCORE_LDO_EN, 0);
			snd_soc_update_bits(codec, RT5511_BIASE_MIC_CTRL,
					    bias_mask, RT5511_D_FAST_CHARGE);
			/* active_dereference(codec); */

		}
		break;

	case SND_SOC_BIAS_OFF:
		if (codec->dapm.bias_level == SND_SOC_BIAS_STANDBY) {
			rt5511->cur_fw = NULL;
			/* pm_runtime_put(codec->dev); */
		}
		break;
	}

	codec->dapm.bias_level = level;
	return 0;
}

int rt5511_vmid_mode(struct snd_soc_codec *codec, enum rt5511_vmid_mode mode)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	switch (mode) {
	case RT5511_VMID_NORMAL:
		/* Do the sync with the old mode to allow it to clean up */
		snd_soc_dapm_sync(&codec->dapm);
		rt5511->vmid_mode = mode;
		break;

	case RT5511_VMID_FORCE:
		rt5511->vmid_mode = mode;
		snd_soc_dapm_sync(&codec->dapm);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * [BLOCK] Param & Format
 */

static inline int rt5511_get_i2scfg_reg(
	struct snd_soc_codec *codec, int id)
{
	int i2scfg_reg;
	switch (id) {
	case 1: /* AIF1 */
		i2scfg_reg = RT5511_CLK1_MODE;
		break;
	case 2: /* AIF2 */
		i2scfg_reg = RT5511_CLK2_MODE;
		break;
	default:
		dev_err(codec->dev, "this codec dai id is not supported\n");
		return -EINVAL;
	}

	return i2scfg_reg;
}

static inline int rt5511_get_aif3_refer(struct snd_soc_codec *codec)
{
	int retval;
	int aif3_ref;

	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	if (rt5511->revision >= RT5511_VID_5511A) {
		retval = snd_soc_read(codec, RT5511_AUD_FMT);
		aif3_ref = (retval & RT5511_AIF3_I2S_SEL) != 0;
	} else {
		retval = snd_soc_read(codec, RT5511_I2S_CK_SEL);
		aif3_ref = (retval & RT5511_SYNC_REF_MASK) != 0;
	}

	aif3_ref += 1;
	return aif3_ref;
}

static inline int rt5511_get_srate_regval(
	struct snd_soc_codec *codec, struct snd_pcm_hw_params *params)
{
	int srate_regval;
	switch (params_rate(params)) {
	case 8000:
		srate_regval = RT5511_I2S_SR_8000;
		break;
	case 11025:
	case 12000:
		srate_regval = RT5511_I2S_SR_12000_11025;
		break;
	case 16000:
		srate_regval = RT5511_I2S_SR_16000;
		break;
	case 22050:
	case 24000:
		srate_regval = RT5511_I2S_SR_24000_22050;
		break;
	case 32000:
		srate_regval = RT5511_I2S_SR_32000;
		break;
	case 44100:
	case 48000:
		srate_regval = RT5511_I2S_SR_48000_44100;
		break;
	case 88200:
	case 96000:
		srate_regval = RT5511_I2S_SR_96000_88200;
		break;
	default:
		dev_err(codec->dev, "not supported sampling rate\n");
		return -EINVAL;
	}

	return srate_regval;
}

static inline int rt5511_get_bckmode_regval(
	struct snd_soc_codec *codec, struct snd_pcm_hw_params *params)
{
	int bckmode_regval;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bckmode_regval = RT5511_I2S_BCK_32FS;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		bckmode_regval = RT5511_I2S_BCK_48FS;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bckmode_regval = RT5511_I2S_BCK_48FS;
		break;
	default:
		dev_err(codec->dev, "not supported data bit format\n");
		return -EINVAL;
	}

	if (rt5511->revision >= RT5511_VID_5511E)
		bckmode_regval = RT5511_I2S_BCK_64FS;

	bckmode_regval <<= RT5511_I2S_BCK_MODE_SHIFT;
	return bckmode_regval;
}

static inline int rt5511_get_audfmt_reg(
	struct snd_soc_codec *codec, int id)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	if (rt5511->revision >= RT5511_VID_5511A) {
		switch (id) {
		case 1:
			return RT5511A_AUD1_FMT;
		case 2:
			return RT5511A_AUD2_FMT;
		case 3:
			return RT5511A_AUD3_FMT_DSP_MODE;
		default:
			dev_err(codec->dev, "this codec dai id is not supported\n");
			return -EINVAL;
		}
	} else {
		return RT5511_AUD_FMT;
	}
}

static inline int rt5511_get_audbit_regval(
	struct snd_soc_codec *codec, struct snd_pcm_hw_params *params)
{
	int audbits_regval;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		audbits_regval = RT5511_AUD_16BIT;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		audbits_regval = RT5511_AUD_20BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		audbits_regval = RT5511_AUD_24BIT;
		break;
	default:
		dev_err(codec->dev, "not supported data bit format\n");
		return -EINVAL;
	}

	return audbits_regval;
}

static inline int rt5511_get_auddsp_regmask(
	struct snd_soc_codec *codec, int id)
{
	switch (id) {
	case 1:
		return RT5511_AIF1_DSP_MODE;
	case 2:
		return RT5511_AIF2_DSP_MODE;
	case 3:
		return RT5511_AIF3_DSP_MODE;
	default:
		dev_err(codec->dev, "this codec dai id is not supported\n");
		return -EINVAL;
	}
}

static int rt5511_set_master_fmt(
	struct snd_soc_codec *codec, int id, int master_fmt)
{
	int reg_mask = 0;
	int reg_value = 0;

	switch (id) {
	case 1:
		reg_mask = RT5511_AIF1_MS_EN;
		break;
	case 2:
		reg_mask = RT5511_AIF2_MS_EN;
		break;
	default:
		return -EINVAL;
	}

	switch (master_fmt) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		reg_value |= reg_mask;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, RT5511_AUD_FMT, reg_mask, reg_value);
	return 0;
}

static int rt5511_set_aud_fmt(
	struct snd_soc_codec *codec, int id, int format_fmt)
{
	int audcfg_reg;
	int audfmt_regval;
	int auddspmode_regval;
	int auddspmode_regmask;

	bool bDSPMode = false;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	audcfg_reg = rt5511_get_audfmt_reg(codec, id);

	switch (format_fmt) {
	case SND_SOC_DAIFMT_I2S:
		audfmt_regval = RT5511_AUD_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		audfmt_regval = RT5511_AUD_FMT_RJ;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		audfmt_regval = RT5511_AUD_FMT_LJ;
		break;

	case SND_SOC_DAIFMT_DSP_B:
	case SND_SOC_DAIFMT_DSP_A:
		if (rt5511->revision >= RT5511_VID_5511A) {
			bDSPMode = true;
			audfmt_regval = RT5511_AUD_FMT_DSP;

			auddspmode_regmask =
				rt5511_get_auddsp_regmask(codec, id);

			if (format_fmt == SND_SOC_DAIFMT_DSP_B)
				auddspmode_regval = auddspmode_regmask;
			else
				auddspmode_regval = 0;
			break;
		} else
			return -EINVAL;
	default:
		return -EINVAL;
	}
	audfmt_regval <<= RT5511_AUD_FMT_SHIFT;

	snd_soc_update_bits(codec, audcfg_reg,
			RT5511_AUD_FMT_MASK, audfmt_regval);

	if (bDSPMode) {
		snd_soc_update_bits(codec, RT5511A_AUD3_FMT_DSP_MODE,
				auddspmode_regmask, auddspmode_regval);
	}

	return 0;
}

static int rt5511_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	int ret;
	int master_fmt;
	int format_fmt;
	int aif3_ref;

	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	RT_DBG("id:%d, value: 0x%08x\n", dai->id, fmt);

	master_fmt = fmt & SND_SOC_DAIFMT_MASTER_MASK;
	format_fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	ret = rt5511_set_master_fmt(codec, dai->id, master_fmt);
	if (ret != 0)
		return ret;

	ret = rt5511_set_aud_fmt(codec, dai->id, format_fmt);
	if (ret != 0)
		return ret;

	aif3_ref = rt5511_get_aif3_refer(codec);

	if (rt5511->revision >= RT5511_VID_5511A) {
		if (dai->id == aif3_ref) {
			ret = rt5511_set_aud_fmt(codec, 3, format_fmt);
			if (ret != 0)
				return ret;
		}
	}

	return 0;
}

static int rt5511_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	int i2scfg_reg;
	int audcfg_reg;
	int srate_regval;
	int audbits_regval;
	int bckmode_regval;

	RT_DBG("codec hw_param dai id %d\n", dai->id);
	RT_DBG("codec hw_param rate%d\n", params_rate(params));
	RT_DBG("codec hw_param format%d\n", params_format(params));
	RT_DBG("codec hw_param stream%d\n", substream->stream);

	i2scfg_reg = rt5511_get_i2scfg_reg(codec, dai->id);
	srate_regval = rt5511_get_srate_regval(codec, params);
	bckmode_regval = rt5511_get_bckmode_regval(codec, params);

	audcfg_reg = rt5511_get_audfmt_reg(codec, dai->id);
	audbits_regval = rt5511_get_audbit_regval(codec, params);

	snd_soc_update_bits(codec, i2scfg_reg,
			    RT5511_I2S_SR_MODE_MASK | RT5511_I2S_BCK_MODE_MASK,
			    srate_regval | bckmode_regval);

	snd_soc_update_bits(codec, audcfg_reg,
			RT5511_AUD_BITS_MASK, audbits_regval);

	return 0;
}

static int rt5511_aif3_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
#ifdef	RT5511_AIF3_AUTO_APPLY_SRATE
	int i2scfg_reg;
	int srate_regval;
#endif

	int aif3_ref;
	int audcfg_reg;
	int audbits_regval;

	struct snd_soc_codec *codec = dai->codec;

	aif3_ref = rt5511_get_aif3_refer(codec);

	RT_DBG("codec hw_param dai id %d\n", dai->id);
	RT_DBG("codec hw_param rate%d\n", params_rate(params));
	RT_DBG("codec hw_param format%d\n", params_format(params));
	RT_DBG("codec hw_param stream%d\n", substream->stream);
	RT_DBG("Reference: I2S%d\n", aif3_ref);

#ifdef	RT5511_AIF3_AUTO_APPLY_SRATE
	i2scfg_reg = rt5511_get_i2scfg_reg(codec, aif3_ref);
	srate_regval = rt5511_get_srate_regval(codec, params);

	snd_soc_update_bits(codec, i2scfg_reg,
			    RT5511_I2S_SR_MODE_MASK, srate_regval);
#endif

	audcfg_reg = rt5511_get_audfmt_reg(codec, dai->id);
	audbits_regval = rt5511_get_audbit_regval(codec, params);

	snd_soc_update_bits(codec, audcfg_reg,
			    RT5511_AUD_BITS_MASK, audbits_regval);
	return 0;
}

/*
 * [BLOCK] Mute
 */

static inline unsigned int rt5511_get_mute_mask(
	struct snd_soc_codec *codec, int id)
{
	unsigned int mute_mask;
	switch (id) {
	case 1: /* AIF1 */
		mute_mask = RT5511_DAC_L2_MUTE | RT5511_DAC_R2_MUTE;
		break;
	case 2: /* AIF2 */
		mute_mask = RT5511_DAC_L1_MUTE | RT5511_DAC_R1_MUTE;
		break;
	default:
		mute_mask = 0;
	}

	return mute_mask;
}

static inline int rt5511_is_enable_aif_mute(
	struct snd_soc_codec *codec, int id)
{
	int regval;
	unsigned int mute_mask = rt5511_get_mute_mask(codec, id);

	regval = snd_soc_read(codec, RT5511_MUTE_CTRL) & mute_mask;

	return (regval != 0);
}

static inline int rt5511_enable_aif_mute(
	struct snd_soc_codec *codec, int id, int mute)
{
	unsigned int mute_mask = rt5511_get_mute_mask(codec, id);

	return snd_soc_update_bits(codec, RT5511_MUTE_CTRL,
			mute_mask, mute ? mute_mask : 0);
}

static int rt5511_aif_mute(struct snd_soc_dai *codec_dai, int mute)
{
#if SKIP_AIF2_MUTE_WHEN_AIF3_USED
	int retval;
#endif

	int ret;
	int index;
	unsigned int mask;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	RT_DBG("id(%d), mute(%d)\n", codec_dai->id, mute);

	if (codec_dai->id == 3)
		return -EINVAL;

#if SKIP_AIF2_MUTE_WHEN_AIF3_USED
	if (codec_dai->id == 2) {
		retval = snd_soc_read(codec, RT5511_AIF_CTRL2);
		if (retval & RT5511_DAC1_SEL_MASK)
			mute = 0;
		retval = snd_soc_read(codec, RT5511_AIF_CTRL1);
		if (retval & RT5511_AIF2_SO_SEL)
			mute = 0;
	}
#endif

	index = codec_dai->id-1;
	mask = 1<<(index+RT5511_VIRT_AIF_AUTO_SKIPMUTE_SHIFT);

	if (virt_ctrl_regdat & mask) {
		if (rt5511->aifclk_en[2])
			mute = 0;
	}

	ret = rt5511_enable_aif_mute(codec, codec_dai->id, mute);

	if (ret > 0) {
		if (!mute)	/* Waiting for ramp */
			msleep(50);
		return 0;
	}

	return ret;
}

/*
 * [BLOCK] tristate
 */

static inline int rt5511_get_tristate_mask(
	struct snd_soc_codec *codec, int id)
{
	unsigned int mask;
	switch (id) {
	case 1: /* AIF1 */
		mask = RT5511_AIF1_TRI;
		break;
	case 2: /* AIF2 */
		mask = RT5511_AIF2_TRI;
		break;
	case 3: /* AIF3 */
		mask = RT5511_AIF3_TRI;
		break;
	default:
		mask = 0;
	}

	return mask;
}

static inline int rt5511_enable_tristate(
	struct snd_soc_codec *codec, int id, int tristate)
{
	unsigned int mask = rt5511_get_tristate_mask(codec, id);

	return snd_soc_update_bits(codec, RT5511A_AIF_TRI_STATE,
			mask, tristate ? mask : 0);
}

static int rt5511_set_tristate(struct snd_soc_dai *codec_dai, int tristate)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	if (rt5511->revision < RT5511_VID_5511A) {
		RT_DBG("Not Implement");
		return 0;
	}

	/* If user decide to auto control it by codec driver,
		then don't directly turn-off it, reload final statues */
	if (rt5511_is_auto_tristate(codec, codec_dai->id)) {
		if ((tristate == 0) && (!rt5511->aifclk_en[codec_dai->id-1]))
			tristate = 1;
	}

	return rt5511_enable_tristate(codec, codec_dai->id, tristate);
}

static int rt5511_aif2_probe(struct snd_soc_dai *dai)
{
	/* Disable the pulls on the AIF if we're using it to save power. */
	return 0;
}

#define RT5511_RATES SNDRV_PCM_RATE_8000_96000

#define RT5511_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops rt5511_aif1_dai_ops = {
	.set_sysclk	= rt5511_set_dai_sysclk,
	.set_fmt	= rt5511_set_dai_fmt,
	.hw_params	= rt5511_hw_params,
	.digital_mute	= rt5511_aif_mute,
	.set_pll	= rt5511_set_pll,
	.set_tristate	= rt5511_set_tristate,
	.set_clkdiv	= rt5511_set_clkdiv,
};

static struct snd_soc_dai_ops rt5511_aif2_dai_ops = {
	.set_sysclk	= rt5511_set_dai_sysclk,
	.set_fmt	= rt5511_set_dai_fmt,
	.hw_params	= rt5511_hw_params,
	.digital_mute   = rt5511_aif_mute,
	.set_pll	= rt5511_set_pll,
	.set_tristate	= rt5511_set_tristate,
};

static struct snd_soc_dai_ops rt5511_aif3_dai_ops = {
	.hw_params	= rt5511_aif3_hw_params,
	.set_tristate	= rt5511_set_tristate,
};

static struct snd_soc_dai_driver rt5511_dai[] = {
	{
		.name = RT5511_CODEC_DAI1_NAME,
		.id = 1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5511_RATES,
			.formats = RT5511_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5511_RATES,
			.formats = RT5511_FORMATS,
		},
		.ops = &rt5511_aif1_dai_ops,
	},
	{
		.name = RT5511_CODEC_DAI2_NAME,
		.id = 2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5511_RATES,
			.formats = RT5511_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5511_RATES,
			.formats = RT5511_FORMATS,
		},
		.probe = rt5511_aif2_probe,
		.ops = &rt5511_aif2_dai_ops,
	},
	{
		.name = RT5511_CODEC_DAI3_NAME,
		.id = 3,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5511_RATES,
			.formats = RT5511_FORMATS,
		},
		.capture = {
			.stream_name = "AIF3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5511_RATES,
			.formats = RT5511_FORMATS,
		},
		.ops = &rt5511_aif3_dai_ops,
	}
};

#ifdef CONFIG_PM

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
static int rt5511_codec_suspend(struct snd_soc_codec *codec)
#else
static int rt5511_codec_suspend(struct snd_soc_codec *codec, pm_message_t state)
#endif
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = rt5511_mfd_do_suspend(codec->control_data);
	if (ret < 0)
		dev_warn(codec->dev,
			"Failed to call mfd_suspend (%d)\n", ret);

	RT_DBG("suspend\n");

	memcpy(&rt5511->pll_suspend, &rt5511->pll,
	       sizeof(struct rt5511_pll_config));

	ret = __rt5511_set_pll(codec, RT5511_PLL, 0, 0, 0);
	if (ret < 0)
		dev_warn(codec->dev, "Failed to stop PLL (%d)\n", ret);

	rt5511_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int rt5511_codec_resume(struct snd_soc_codec *codec)
{
	/* Must check, Added by Patrick */
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);
	int ret;

	RT_DBG("resume\n");

	rt5511_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	if (rt5511->pll_suspend.out) {
		ret = __rt5511_set_pll(codec, RT5511_PLL,
				       rt5511->pll_suspend.src,
				       rt5511->pll_suspend.in,
				       rt5511->pll_suspend.out);
		if (ret < 0)
			dev_warn(codec->dev,
				"Failed to restore PLL: %d\n", ret);
	}

	ret = rt5511_mfd_do_resume(codec->control_data);
	if (ret < 0)
		dev_warn(codec->dev, "Failed to call mfd_resume (%d)\n", ret);

	return 0;
}
#else
#define rt5511_codec_suspend NULL
#define rt5511_codec_resume NULL
#endif

static void rt5511_handle_pdata(struct rt5511_priv *rt5511)
{
	int i;
	struct snd_soc_codec *codec = rt5511->codec;
	struct rt5511_pdata *pdata = rt5511->pdata;

	if (!pdata)
		return;

	rt_hubs_handle_analogue_pdata(codec,
				      pdata->mic1_differential,
				      pdata->mic2_differential,
				      pdata->mic3_differential,
				      pdata->micbias[0],
				      pdata->micbias[1]);

	/* Copy platform setting to current state */
	for (i = 0; i < RT5511_BQ_PATH_NUM; i++)
		rt5511->eq_en[i] = pdata->eq_en[i];

	for (i = 0; i < RT5511_DRC_REG_NUM; i++)
		rt5511->drc_en[i] = pdata->drc_en[i];

	memcpy(rt5511->dre_cfg, pdata->dre_cfg, RT5511A_DRE_REG_SIZE);

	rt5511_eq_init(codec);
	rt5511_drc_init(codec);
	rt5511_dre_init(codec);

	/* Noise gate only work in drc enable case */
	rt5511_drc_ng_enable(codec, true);

	/* Alwats enable drc_adc if user enable it */
	rt5511_drc_adc_auto_enable(codec, true);
}

/* restroe registers' setting */
void rt5511_codec_poweron_cb(void *data)
{
	struct snd_soc_codec *codec = data;
	int ret;
	u32 nBypass;

	RT_DBG("poweron_cb\n");

	/* Init EQ & DRC & DRE Coeff */
	rt5511_eq_init(codec);
	rt5511_drc_init(codec);
	rt5511_dre_init(codec);

	/* Restore the registers */
	nBypass = virt_ctrl_regdat & RT5511_VIRT_BYPASS_CODEC_W;
	virt_ctrl_regdat &= (~RT5511_VIRT_BYPASS_CODEC_W);
	ret = rt5511_reg_cache_sync(codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s : error->cannot sync cache(%d)\n",
			__func__, ret);
	}
	virt_ctrl_regdat |= nBypass;
}

#ifdef CONFIG_DEBUG_FS
static int reg_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (strict_strtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static int get_datas(char *buf,  unsigned char *data_buffer,
		      unsigned char data_length)
{
	int  length;
	int  i, ptr;
	long int value;
	char token[5];

	token[0] = '0';
	token[1] = 'x';
	token[4] = 0;
	if (buf[0] != '0' || buf[1] != 'x')
		return -EINVAL;
	length = strlen(buf);

	ptr = 2;
	for (i = 0; (i < data_length) && (ptr + 2 <= length); i++) {
		token[2] = buf[ptr++];
		token[3] = buf[ptr++];
		if (strict_strtoul(token, 16, &value) != 0)
			return -EINVAL;
		data_buffer[i] = value;
	}
	return 0;
}

static ssize_t reg_debug_read(struct file *filp, char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	struct rt_debug_st *st = filp->private_data;
	struct rt_debug_info *di = st->info;
	char lbuf[1000];
	int i = 0, j = 0;

	lbuf[0] = '\0';
	switch (st->id) {
	case RT5511_DBG_REG:
		snprintf(lbuf, sizeof(lbuf), "0x%02x\n", di->reg_addr);
		break;
	case RT5511_DBG_DATA:
		di->reg_data = rt5511_read(di->codec, di->reg_addr);
		snprintf(lbuf, sizeof(lbuf), "0x%08x\n", di->reg_data);
		break;
	case RT5511_DBG_REGS:
		switch (di->part_id) {
		case 0:
			for (i = 0; i < 85 && j < 985; i++) {
				if (check_regsize_equal_1_4(i))
					j += sprintf(lbuf + j,
						"reg_%02x:%08x\n", i,
						rt5511_read(di->codec, i));
				else
					continue;
			}
			break;
		case 1:
			for (i = 85; i < 170 && j < 985; i++) {
				if (check_regsize_equal_1_4(i))
					j += sprintf(lbuf + j,
						"reg_%02x:%08x\n", i,
						rt5511_read(di->codec, i));
				else
					continue;
			}
			break;
		case 2:
			for (i = 170; i < RT5511_CACHE_SIZE && j < 985; i++) {
				if (check_regsize_equal_1_4(i))
					j += sprintf(lbuf + j,
						"reg_%02x:%08x\n", i,
						rt5511_read(di->codec, i));
				else
					continue;
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	case RT5511_DBG_ADBREG:
		snprintf(lbuf, sizeof(lbuf),
			"0x%02x %3d\n", di->reg_addr, di->dbg_length);
		break;
	case RT5511_DBG_ADBDATA:
		if (di->reg_addr == RT5511_VIRT_CTRL) {
			di->dbg_data[0] = virt_ctrl_regdat;
		} else {
			j = rt5511_bulk_read(di->codec->control_data,
				di->reg_addr, di->dbg_length, di->dbg_data);
			if (j < 0)
				dev_err(di->codec->dev, "Read from %x failed: %d\n",
					di->reg_addr, j);
		}
		j = sprintf(lbuf, "0x");
		for (i = 0; i < di->dbg_length; i++)
			j += sprintf(lbuf + j, "%02x", di->dbg_data[i]);
		j += sprintf(lbuf + j, "\n");
		break;
	default:
		return -EINVAL;
	}
	return simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
}

static ssize_t reg_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct rt_debug_st *st = filp->private_data;
	struct rt_debug_info *di = st->info;
	char lbuf[46];
	int rc;
	long int param[5];
	u32 value;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	switch (st->id) {
	case RT5511_DBG_REG:
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] < RT5511_CACHE_SIZE) && (rc == 0)) {
			if (rt5511_reg_size[param[0]] == 1 ||
			    rt5511_reg_size[param[0]] == 4)
				di->reg_addr = (unsigned char)param[0];
			else
				rc = -EINVAL;
		} else
			rc = -EINVAL;
		break;
	case RT5511_DBG_DATA:
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xffffffff) && (rc == 0))
			rt5511_write(di->codec, di->reg_addr, (u32)param[0]);
		else
			rc = -EINVAL;
		break;
	case RT5511_DBG_REGS:
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] < 3) && (rc == 0))
			di->part_id = (unsigned char)param[0];
		else
			rc = -EINVAL;
		break;
	case RT5511_DBG_ADBREG:
		rc = get_parameters(lbuf, param, 2);
		di->reg_addr   = (param[0] < RT5511_CACHE_SIZE) ? param[0] :
				 (RT5511_CACHE_SIZE - 1);
		di->dbg_length = (param[1] > 256) ? 256 : param[1];
		break;
	case RT5511_DBG_ADBDATA:
		rc = get_datas(lbuf, di->dbg_data, di->dbg_length);
		if (rc < 0)
			return -EINVAL;

		if ((rt5511_reg_size[di->reg_addr] == 1
			&& di->dbg_length == 1) ||
		    (rt5511_reg_size[di->reg_addr] == 4
			&& di->dbg_length == 4)) {
			if (di->dbg_length == 1)
				value = (u32)di->dbg_data[0];
			else {
				value = *(u32 *)&di->dbg_data[0];
				value = be32_to_cpu(value);
			}
			rc = rt5511_write(di->codec, di->reg_addr, value);
		} else {
			rc = rt5511_bulk_write(di->codec->control_data,
				di->reg_addr, di->dbg_length, di->dbg_data);
		}
		break;
	default:
		return -EINVAL;
	}
	if (rc == 0)
		rc = cnt;
	return rc;
}

static const struct file_operations reg_debug_ops = {
	.open = reg_debug_open,
	.write = reg_debug_write,
	.read = reg_debug_read
};
#endif /* #ifdef CONFIG_DEBUG_FS */

static int rt5511_codec_probe(struct snd_soc_codec *codec)
{
	struct rt5511 *control;		/* get from mfd */
	struct rt5511_priv *rt5511;	/* private data of codec device */
	struct snd_soc_dapm_context *dapm = &codec->dapm;
#ifdef CONFIG_DEBUG_FS
	struct rt_debug_info *di;
#endif /* #ifdef CONFIG_DEBUG_FS */
	int i;

	RT_DBG("codec probe, version: %s\n", RT5511_DRV_VER);
	rt5511_init_cache();
	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;

	rt5511 = kzalloc(sizeof(struct rt5511_priv), GFP_KERNEL);
	if (rt5511 == NULL)
		return -ENOMEM;
	snd_soc_codec_set_drvdata(codec, rt5511);

	rt5511->pdata = dev_get_platdata(codec->dev->parent);
	rt5511->codec = codec;

	mutex_init(&rt5511->accdet_lock);

	for (i = 0; i < ARRAY_SIZE(rt5511->fll_locked); i++)
		init_completion(&rt5511->fll_locked[i]);

#if 0 /* To do */
	pm_runtime_enable(codec->dev);
	pm_runtime_resume(codec->dev);
#endif

	/* By default use idle_bias_off, will override for RT5511 */
	codec->dapm.idle_bias_off = 1;

	/* Set revision-specific configuration */
	rt5511->revision = snd_soc_read(codec, RT5511_VID);

	/*rt5511_set_bias_level(codec, SND_SOC_BIAS_STANDBY);*/

	/* Latch volume updates (right only; we always do left then right). */

	/* TBD: Put MICBIAS into bypass mode by default on newer devices */

	/* Enable AIF1, AIF2, AIF3 */
	snd_soc_update_bits(codec, RT5511_SOURCE_ENABLE, 0x70, 0x70);

	/* Select AIF3 pin (original: GPIO pin) */
	snd_soc_write(codec, RT5511_J_DET_SEL, 0);
	snd_soc_update_bits(codec, RT5511_BIASE_MIC_CTRL, RT5511_D_JD_EN, 0);

	/* Reduce High Freq Noise Issue */
	snd_soc_update_bits(codec, RT5511_DAC_DRC_EQ_EN,
			RT5511_DAC_CK_INV, RT5511_DAC_CK_INV);

	/* Avoid PLL lost off clock */
	snd_soc_update_bits(codec, RT5511_PLL_RSEL,
			RT5511_PLL_CKSEL_MASK, 2 << RT5511_PLL_CKSEL_SHIFT);

	/* AIF3 refer to AIF2 */
	snd_soc_update_bits(codec, RT5511_AUD_FMT,
			RT5511_AIF3_I2S_SEL, RT5511_AIF3_I2S_SEL);

	/* Select AIF1/AIF2 clock refer to PLL */
	snd_soc_update_bits(codec, RT5511_AIF_CTRL2,
			AIF1_MS_SEL_MASK | AIF2_MS_SEL_MASK,
			(AIF_MS_REF_PLL << AIF1_MS_SEL_SHIFT) |
			(AIF_MS_REF_PLL << AIF2_MS_SEL_SHIFT));

	/* Analogy Input Initial Setting */
	snd_soc_write(codec, RT5511_D_INT1_IBSEL, 0x29);

	/* Headphone offset compensation for POP-noise */
	snd_soc_write(codec, RT5511_D_HP_IBSEL, 0x60);

	/* Disable Low Power Mode for MIC Bias */
	snd_soc_update_bits(codec, RT5511_MIC_LPM_CTRL,
		RT5511_MIC_LPM_EN, 0);

	if (rt5511->revision >= RT5511_VID_5511A) {
		/* NEW Clk rate Control */
		snd_soc_write(codec, RT5511A_NEW_CLK_RATE_CTRL, 0x08);
	}

	if (rt5511->revision >= RT5511_VID_5511E) {
		/* Disable TDM debug mode */
		snd_soc_write(codec, RT5511E_TDM_DEBUG, 0x03);
	}

	rt5511->hubs.eq_enable = rt5511_eq_auto_enable;
	rt5511->hubs.drc_dac_enable = rt5511_drc_dac_auto_enable;

	rt5511_handle_pdata(rt5511);

	rt_hubs_add_analogue_controls(codec);
	snd_soc_add_controls(codec, rt5511_snd_controls,
			     ARRAY_SIZE(rt5511_snd_controls));

	if (rt5511->revision >= RT5511_VID_5511A) {
		snd_soc_add_controls(codec, rt5511A_snd_controls,
				ARRAY_SIZE(rt5511A_snd_controls));

		snd_soc_add_controls(codec, rt5511A_dre_en_state_controls,
				ARRAY_SIZE(rt5511A_dre_en_state_controls));
	}

	snd_soc_dapm_new_controls(dapm, rt5511_dapm_widgets,
				  ARRAY_SIZE(rt5511_dapm_widgets));
	snd_soc_dapm_new_controls(dapm, rt5511_lateclk_widgets,
				  ARRAY_SIZE(rt5511_lateclk_widgets));
	snd_soc_dapm_new_controls(dapm, rt5511_adc_widgets,
				  ARRAY_SIZE(rt5511_adc_widgets));
	snd_soc_dapm_new_controls(dapm, rt5511_dac_widgets,
				  ARRAY_SIZE(rt5511_dac_widgets));

	rt_hubs_add_analogue_routes(codec);
	snd_soc_dapm_add_routes(dapm, intercon, ARRAY_SIZE(intercon));

	switch (control->type) {
	case RT5511:
	default:
		snd_soc_dapm_add_routes(dapm, rt5511_lateclk_intercon,
					ARRAY_SIZE(rt5511_lateclk_intercon));
		snd_soc_dapm_add_routes(dapm, rt5511_intercon,
					ARRAY_SIZE(rt5511_intercon));
		break;
	}

	rt5511_set_private_power_on_event(control,
					  rt5511_codec_poweron_cb, codec);

#ifdef CONFIG_DEBUG_FS
	di = devm_kzalloc(codec->dev, sizeof(*di), GFP_KERNEL);
	di->codec = codec;
	debugfs_rt_dent = debugfs_create_dir("rt5511_dbg", 0);
	if (!IS_ERR(debugfs_rt_dent)) {
		rtdbg_data[0].info = di;
		rtdbg_data[0].id = RT5511_DBG_REG;
		debugfs_file[0] = debugfs_create_file("reg",
			S_IFREG | S_IRUGO, debugfs_rt_dent,
			(void *) &rtdbg_data[0], &reg_debug_ops);

		rtdbg_data[1].info = di;
		rtdbg_data[1].id = RT5511_DBG_DATA;
		debugfs_file[1] = debugfs_create_file("data",
			S_IFREG | S_IRUGO, debugfs_rt_dent,
			(void *) &rtdbg_data[1], &reg_debug_ops);

		rtdbg_data[2].info = di;
		rtdbg_data[2].id = RT5511_DBG_REGS;
		debugfs_file[2] = debugfs_create_file("regs",
			S_IFREG | S_IRUGO, debugfs_rt_dent,
			(void *) &rtdbg_data[2], &reg_debug_ops);

		rtdbg_data[3].info = di;
		rtdbg_data[3].id = RT5511_DBG_ADBREG;
		debugfs_file[3] = debugfs_create_file("adbreg",
			S_IFREG | S_IRUGO, debugfs_rt_dent,
			(void *) &rtdbg_data[3], &reg_debug_ops);

		rtdbg_data[4].info = di;
		rtdbg_data[4].id = RT5511_DBG_ADBDATA;
		debugfs_file[4] = debugfs_create_file("adbdata",
			S_IFREG | S_IRUGO, debugfs_rt_dent,
			(void *) &rtdbg_data[4], &reg_debug_ops);
	}
#endif /* #ifdef CONFIG_DEBUG_FS */
	RT_DBG("codec probe successfully\n");
	return 0;
}

static int  rt5511_codec_remove(struct snd_soc_codec *codec)
{
	struct rt5511_priv *rt5511 = snd_soc_codec_get_drvdata(codec);

	rt5511_set_bias_level(codec, SND_SOC_BIAS_OFF);

#if 0 /* To do */
	pm_runtime_disable(codec->dev);
#endif

	kfree(rt5511->eq_texts);
	kfree(rt5511->drc_texts);
	kfree(rt5511);
#ifdef CONFIG_DEBUG_FS
	if (!IS_ERR(debugfs_rt_dent))
		debugfs_remove_recursive(debugfs_rt_dent);
#endif /* #ifdef CONFIG_DEBUG_FS */

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_rt5511 = {
	.probe =	rt5511_codec_probe,
	.remove =	rt5511_codec_remove,
	.suspend =	rt5511_codec_suspend,
	.resume =	rt5511_codec_resume,
	.read =		rt5511_read,
	.write =	rt5511_write,

	.set_bias_level = rt5511_set_bias_level,

	.readable_register = rt5511_readable,
	.volatile_register = rt5511_volatile,

	/* .max_register = RT5511_MAX_REGISTER, */
	.reg_cache_size = 0,
	.reg_cache_default = rt5511_reg_defaults,
	.reg_word_size = 1,
	.compress_type = SND_SOC_FLAT_COMPRESSION,
};

static int rt5511_probe(struct platform_device *pdev)
{
	int ret;
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_rt5511,
				     rt5511_dai, ARRAY_SIZE(rt5511_dai));
	/*RT_DBG("ret = %d, use_dt=%zu\n", ret, pdev->dev.of_node);*/
	return ret;
}

static int rt5511_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rt5511_suspend(struct device *dev)
{
	return 0;
}

static int rt5511_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops rt5511_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rt5511_suspend, rt5511_resume)
};

#ifdef CONFIG_OF
static const struct of_device_id rt5511_codec_of_match[] = {
	{ .compatible = "richtek,rt5511-codec",},
	{},
};
#else
#define rt5511_codec_of_match	NULL
#endif

static struct platform_driver rt5511_codec_driver = {
	.driver = {
		.name = "rt5511-codec",
		.owner = THIS_MODULE,
		.pm = &rt5511_pm_ops,
		.of_match_table = rt5511_codec_of_match,
	},
	.probe = rt5511_probe,
	.remove = rt5511_remove,
};

static __init int rt5511_init(void)
{
	return platform_driver_register(&rt5511_codec_driver);
}
module_init(rt5511_init);

static __exit void rt5511_exit(void)
{
	platform_driver_unregister(&rt5511_codec_driver);
}
module_exit(rt5511_exit);


MODULE_DESCRIPTION("ASoC RT5511 driver");
MODULE_AUTHOR("Tsunghan Tsai <tsunghan_tsai@richtek.com>");
MODULE_VERSION(RT5511_DRV_VER);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rt5511-codec");
